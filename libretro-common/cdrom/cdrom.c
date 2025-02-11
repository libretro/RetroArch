/* Copyright  (C) 2010-2020 The RetroArch team
*
* ---------------------------------------------------------------------------------------
* The following license statement only applies to this file (cdrom.c).
* ---------------------------------------------------------------------------------------
*
* Permission is hereby granted, free of charge,
* to any person obtaining a copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
* and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cdrom/cdrom.h>
#include <libretro.h>
#include <stdio.h>
#include <string.h>
#include <compat/strl.h>
#include <compat/strcasestr.h>
#include <retro_math.h>
#include <retro_timers.h>
#include <streams/file_stream.h>
#include <retro_endianness.h>
#include <retro_miscellaneous.h>
#include <vfs/vfs_implementation.h>
#include <lists/string_list.h>
#include <lists/dir_list.h>
#include <string/stdstring.h>
#include <memalign.h>

#include <math.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#if defined(__linux__) && !defined(ANDROID)
#include <sys/ioctl.h>
#include <scsi/sg.h>
#endif

#if defined(_WIN32) && !defined(_XBOX)
#include <windows.h>
#include <winioctl.h>
#include <ntddscsi.h>
#endif

#define CDROM_CUE_TRACK_BYTES 107
#define CDROM_MAX_SENSE_BYTES 16
#define CDROM_MAX_RETRIES 10
#define CDROM_MIN_BUFSIZE 9

typedef enum
{
   DIRECTION_NONE,
   DIRECTION_IN,
   DIRECTION_OUT
} CDROM_CMD_Direction;

void cdrom_lba_to_msf(unsigned lba, unsigned char *min, unsigned char *sec, unsigned char *frame)
{
   if (!min || !sec || !frame)
      return;

   *frame  = lba % 75;
   lba    /= 75;
   *sec    = lba % 60;
   lba    /= 60;
   *min    = lba;
}

unsigned cdrom_msf_to_lba(unsigned char min, unsigned char sec, unsigned char frame)
{
   return (min * 60 + sec) * 75 + frame;
}

void increment_msf(unsigned char *min, unsigned char *sec, unsigned char *frame)
{
   if (!min || !sec || !frame)
      return;

   *min   = (*frame == 74) ? (*sec < 59 ? *min : *min + 1) : *min;
   *sec   = (*frame == 74) ? (*sec < 59 ? (*sec + 1) : 0) : *sec;
   *frame = (*frame  < 74) ? (*frame + 1) : 0;
}

#ifdef CDROM_DEBUG
static void cdrom_print_sense_data(const unsigned char *s, size_t len)
{
   unsigned i;
   const char *sense_key_text = NULL;
   unsigned char key;
   unsigned char asc;
   unsigned char ascq;

   if (len < 16)
   {
      printf("[CDROM] Sense data buffer length too small.\n");
      fflush(stdout);
      return;
   }

   key  = s[2] & 0xF;
   asc  = s[12];
   ascq = s[13];

   printf("[CDROM] Sense Data: ");

   for (i = 0; i < MIN(len, 16); i++)
      printf("%02X ", s[i]);

   printf("\n");

   if (s[0] == 0x70)
      printf("[CDROM] CURRENT ERROR:\n");
   if (s[0] == 0x71)
      printf("[CDROM] DEFERRED ERROR:\n");

   switch (key)
   {
      case 0:
         sense_key_text = "NO SENSE";
         break;
      case 1:
         sense_key_text = "RECOVERED ERROR";
         break;
      case 2:
         sense_key_text = "NOT READY";
         break;
      case 3:
         sense_key_text = "MEDIUM ERROR";
         break;
      case 4:
         sense_key_text = "HARDWARE ERROR";
         break;
      case 5:
         sense_key_text = "ILLEGAL REQUEST";
         break;
      case 6:
         sense_key_text = "UNIT ATTENTION";
         break;
      case 7:
         sense_key_text = "DATA PROTECT";
         break;
      case 8:
         sense_key_text = "BLANK CHECK";
         break;
      case 9:
         sense_key_text = "VENDOR SPECIFIC";
         break;
      case 10:
         sense_key_text = "COPY ABORTED";
         break;
      case 11:
         sense_key_text = "ABORTED COMMAND";
         break;
      case 13:
         sense_key_text = "VOLUME OVERFLOW";
         break;
      case 14:
         sense_key_text = "MISCOMPARE";
         break;
   }

   printf("[CDROM] Sense Key: %02X (%s)\n", key, sense_key_text ? sense_key_text : "null");
   printf("[CDROM] ASC: %02X\n", asc);
   printf("[CDROM] ASCQ: %02X\n", ascq);

   switch (key)
   {
      case 2:
      {
         switch (asc)
         {
            case 4:
            {
               switch (ascq)
               {
                  case 1:
                     printf("[CDROM] Description: LOGICAL UNIT IS IN PROCESS OF BECOMING READY\n");
                     break;
                  default:
                     break;
               }

               break;
            }
            case 0x3a:
            {
               switch (ascq)
               {
                  case 0:
                     printf("[CDROM] Description: MEDIUM NOT PRESENT\n");
                     break;
                  case 3:
                     printf("[CDROM] Description: MEDIUM NOT PRESENT - LOADABLE\n");
                     break;
                  case 1:
                     printf("[CDROM] Description: MEDIUM NOT PRESENT - TRAY CLOSED\n");
                     break;
                  case 2:
                     printf("[CDROM] Description: MEDIUM NOT PRESENT - TRAY OPEN\n");
                     break;
                  default:
                     break;
               }

               break;
            }
            default:
               break;
         }
      }
      case 3:
      {
         if (asc == 0x11 && ascq == 0x5)
            printf("[CDROM] Description: L-EC UNCORRECTABLE ERROR\n");
         break;
      }
      case 5:
      {
         if (asc == 0x20 && ascq == 0)
            printf("[CDROM] Description: INVALID COMMAND OPERATION CODE\n");
         else if (asc == 0x24 && ascq == 0)
            printf("[CDROM] Description: INVALID FIELD IN CDB\n");
         else if (asc == 0x26 && ascq == 0)
            printf("[CDROM] Description: INVALID FIELD IN PARAMETER LIST\n");
         break;
      }
      case 6:
      {
         if (asc == 0x28 && ascq == 0)
            printf("[CDROM] Description: NOT READY TO READY CHANGE, MEDIUM MAY HAVE CHANGED\n");
         break;
      }
      default:
         break;
   }

   fflush(stdout);
}
#endif

#if defined(_WIN32) && !defined(_XBOX)
static int cdrom_send_command_win32(const libretro_vfs_implementation_file *stream, CDROM_CMD_Direction dir, void *buf, size_t len, unsigned char *cmd, size_t cmd_len, unsigned char *sense, size_t sense_len)
{
   DWORD ioctl_bytes;
   BOOL ioctl_rv;
#ifdef CDROM_DEBUG
   clock_t t                       = clock();
   const char *extra               = " ";
   static unsigned char last_min   = 0;
   static unsigned char last_sec   = 0;
   static unsigned char last_frame = 0;

   unsigned lba_cur = cdrom_msf_to_lba(last_min, last_sec, last_frame);
   unsigned lba_req = cdrom_msf_to_lba(cmd[3], cmd[4], cmd[5]);
#endif
   struct sptd_with_sense
   {
     SCSI_PASS_THROUGH_DIRECT s;
     UCHAR sense[128];
   } sptd;

   memset(&sptd, 0, sizeof(sptd));

   sptd.s.Length    = sizeof(sptd.s);
   sptd.s.CdbLength = cmd_len;

   switch (dir)
   {
      case DIRECTION_IN:
         sptd.s.DataIn = SCSI_IOCTL_DATA_IN;
         break;
      case DIRECTION_OUT:
         sptd.s.DataIn = SCSI_IOCTL_DATA_OUT;
         break;
      case DIRECTION_NONE:
      default:
         sptd.s.DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;
         break;
   }

   sptd.s.TimeOutValue       = 5;
   sptd.s.DataBuffer         = buf;
   sptd.s.DataTransferLength = len;
   sptd.s.SenseInfoLength    = sizeof(sptd.sense);
   sptd.s.SenseInfoOffset    = offsetof(struct sptd_with_sense, sense);

   memcpy(sptd.s.Cdb, cmd, cmd_len);

   ioctl_rv = DeviceIoControl(stream->fh, IOCTL_SCSI_PASS_THROUGH_DIRECT, &sptd,
      sizeof(sptd), &sptd, sizeof(sptd), &ioctl_bytes, NULL);

#ifdef CDROM_DEBUG
   if (lba_req < lba_cur)
      extra = " BACKWARDS SECTOR READ";
   else if (lba_req > lba_cur)
      extra = " SKIPPED SECTOR READ";

   if (cmd[0] == 0xB9)
   {
      double time_taken = (double)(((clock() - t) * 1000) / CLOCKS_PER_SEC);
      printf("time taken %f ms for DT received length %ld of %" PRId64 " for %02d:%02d:%02d to %02d:%02d:%02d%s req %d cur %d cur_lba %d\n", time_taken, sptd.s.DataTransferLength, len, cmd[3], cmd[4], cmd[5], cmd[6], cmd[7], cmd[8], extra, lba_req, lba_cur, stream->cdrom.cur_lba);
      fflush(stdout);
   }

   last_min   = cmd[3];
   last_sec   = cmd[4];
   last_frame = cmd[5];
   increment_msf(&last_min, &last_sec, &last_frame);
#endif

   if (!ioctl_rv || sptd.s.ScsiStatus != 0)
      return 1;

   return 0;
}
#endif

#if defined(__linux__) && !defined(ANDROID)
static int cdrom_send_command_linux(const libretro_vfs_implementation_file *stream, CDROM_CMD_Direction dir, void *buf, size_t len, unsigned char *cmd, size_t cmd_len, unsigned char *sense, size_t sense_len)
{
   int rv;
   sg_io_hdr_t sgio = {0};

   switch (dir)
   {
      case DIRECTION_IN:
         sgio.dxfer_direction = SG_DXFER_FROM_DEV;
         break;
      case DIRECTION_OUT:
         sgio.dxfer_direction = SG_DXFER_TO_DEV;
         break;
      case DIRECTION_NONE:
      default:
         sgio.dxfer_direction = SG_DXFER_NONE;
         break;
   }

   sgio.interface_id = 'S';
   sgio.cmd_len      = cmd_len;
   sgio.cmdp         = cmd;
   sgio.dxferp       = buf;
   sgio.dxfer_len    = len;
   sgio.sbp          = sense;
   sgio.mx_sb_len    = sense_len;
   sgio.timeout      = 5000;

   rv = ioctl(fileno(stream->fp), SG_IO, &sgio);

   if (rv == -1 || sgio.info & SG_INFO_CHECK)
      return 1;

   return 0;
}
#endif

static int cdrom_send_command(libretro_vfs_implementation_file *stream, CDROM_CMD_Direction dir,
      void *s, size_t len, unsigned char *cmd, size_t cmd_len, size_t skip)
{
   int i, rv = 0;
   int frames = 1;
   unsigned char *xfer_buf     = NULL;
   unsigned char *xfer_buf_pos = xfer_buf;
   unsigned char sense[CDROM_MAX_SENSE_BYTES] = {0};
   unsigned char retries_left  = CDROM_MAX_RETRIES;
   size_t padded_req_bytes;
   size_t copied_bytes         = 0;
   bool read_cd                = false;

   if (!cmd || cmd_len == 0 || cmd_len < CDROM_MIN_BUFSIZE)
      return 1;

   if (cmd[0] == 0xBE || cmd[0] == 0xB9)
   {
      frames = ceil((len + skip) / 2352.0);
      padded_req_bytes = 2352 * frames;
      read_cd = true;
      /* these will be incremented below */
      cmd[6] = cmd[3];
      cmd[7] = cmd[4];
      cmd[8] = cmd[5];
   }
   else
   {
      padded_req_bytes = len + skip;
   }

   xfer_buf     = (unsigned char*)memalign_alloc(4096, padded_req_bytes);
   xfer_buf_pos = xfer_buf;

   if (!xfer_buf)
      return 1;

   memset(xfer_buf, 0, padded_req_bytes);
#ifdef CDROM_DEBUG
   printf("Number of frames to read: %d\n", frames);
   fflush(stdout);
#endif
   for (i = 0; i < frames; i++)
   {
      size_t request_len = padded_req_bytes;
      size_t copy_len = request_len;
      bool cached_read = false;

      if (read_cd)
      {
         unsigned lba_req = 0;

         request_len = 2352;
         copy_len = request_len;

         increment_msf(&cmd[6], &cmd[7], &cmd[8]);

         if (i > 0)
         {
            skip = 0;
            increment_msf(&cmd[3], &cmd[4], &cmd[5]);
         }
         else
         {
            if (skip)
               copy_len -= skip;
         }

         if (i == frames - 1)
         {
            copy_len = len - copied_bytes;
         }

         lba_req = cdrom_msf_to_lba(cmd[3], cmd[4], cmd[5]);

         if (stream->cdrom.last_frame_valid && lba_req == stream->cdrom.last_frame_lba)
         {
            /* use cached frame */
            cached_read = true;
#ifdef CDROM_DEBUG
            printf("[CDROM] Using cached frame\n");
            fflush(stdout);
#endif
            /* assumes request_len is always equal to the size of last_frame */
            memcpy(xfer_buf_pos, stream->cdrom.last_frame, sizeof(stream->cdrom.last_frame));
         }

      }

#ifdef CDROM_DEBUG
      if (!cached_read)
      {
         unsigned j;

         printf("[CDROM] Send Command: ");

         for (j = 0; j < cmd_len / sizeof(*cmd); j++)
         {
            printf("%02X ", cmd[j]);
         }

         if (len)
            printf("(buffer of size %" PRId64 " with skip bytes %" PRId64 " padded to %" PRId64 "), frame %d\n", len, skip, padded_req_bytes, i);
         else
            printf("\n");

         fflush(stdout);
      }
#endif

retry:
#if defined(__linux__) && !defined(ANDROID)
      if (cached_read || !cdrom_send_command_linux(stream, dir, xfer_buf_pos, request_len, cmd, cmd_len, sense, sizeof(sense)))
#else
#if defined(_WIN32) && !defined(_XBOX)
      if (cached_read || !cdrom_send_command_win32(stream, dir, xfer_buf_pos, request_len, cmd, cmd_len, sense, sizeof(sense)))
#endif
#endif
      {
         rv = 0;

         if (s)
         {
#if 0
            printf("offsetting %" PRId64 " from buf, copying at xfer_buf offset %" PRId64 ", copying %" PRId64 " bytes\n", copied_bytes, (xfer_buf_pos + skip) - xfer_buf, copy_len);
            fflush(stdout);
#endif
            memcpy((char*)s + copied_bytes, xfer_buf_pos + skip, copy_len);
            copied_bytes += copy_len;

            if (read_cd && !cached_read && request_len >= 2352)
            {
               unsigned frame_end = cdrom_msf_to_lba(cmd[6], cmd[7], cmd[8]);

               /* cache the last received frame */
               memcpy(stream->cdrom.last_frame, xfer_buf_pos, sizeof(stream->cdrom.last_frame));
               stream->cdrom.last_frame_valid = true;
               /* the ending frame is never actually read, so what we really just read is the one right before that */
               stream->cdrom.last_frame_lba = frame_end - 1;
            }
            else
               stream->cdrom.last_frame_valid = false;

#if 0
            printf("Frame %d, adding %" PRId64 " to buf_pos, is now %" PRId64 ". skip is %" PRId64 "\n", i, request_len, (xfer_buf_pos + request_len) - xfer_buf, skip);
            fflush(stdout);
#endif
            xfer_buf_pos += request_len;
         }
      }
      else
      {
#ifdef CDROM_DEBUG
         cdrom_print_sense_data(sense, sizeof(sense));
#endif

         /* INQUIRY/TEST/SENSE should never fail, don't retry. */
         /* READ ATIP seems to fail outright on some drives with pressed discs, skip retries. */
         if (cmd[0] != 0x0 && cmd[0] != 0x12 && cmd[0] != 0x5A && !(cmd[0] == 0x43 && cmd[2] == 0x4))
         {
            unsigned char key = sense[2] & 0xF;

            switch (key)
            {
               case 0:
               case 2:
               case 3:
               case 4:
               case 6:
                  if (retries_left)
                  {
   #ifdef CDROM_DEBUG
                     printf("[CDROM] Read Retry...\n");
                     fflush(stdout);
   #endif
                     retries_left--;
                      retro_sleep(1000);
                     goto retry;
                  }
                  else
                  {
                     rv = 1;
   #ifdef CDROM_DEBUG
                     printf("[CDROM] Read retries failed, giving up.\n");
                     fflush(stdout);
   #endif
                  }

                  break;
               default:
                  break;
            }
         }

         rv = 1;
      }
   }

   if (xfer_buf)
      memalign_free(xfer_buf);
   return rv;
}

static const char *cdrom_get_profile(unsigned short profile)
{
   switch (profile)
   {
      case 2:
         return "Removable disk";
      case 8:
         return "CD-ROM";
      case 9:
         return "CD-R";
      case 0xA:
         return "CD-RW";
      case 0x10:
         return "DVD-ROM";
      case 0x11:
         return "DVD-R Sequential Recording";
      case 0x12:
         return "DVD-RAM";
      case 0x13:
         return "DVD-RW Restricted Overwrite";
      case 0x14:
         return "DVD-RW Sequential recording";
      case 0x15:
         return "DVD-R Dual Layer Sequential Recording";
      case 0x16:
         return "DVD-R Dual Layer Jump Recording";
      case 0x17:
         return "DVD-RW Dual Layer";
      case 0x1A:
         return "DVD+RW";
      case 0x1B:
         return "DVD+R";
      case 0x2A:
         return "DVD+RW Dual Layer";
      case 0x2B:
         return "DVD+R Dual Layer";
      case 0x40:
         return "BD-ROM";
      case 0x41:
         return "BD-R SRM";
      case 0x42:
         return "BD-R RRM";
      case 0x43:
         return "BD-RE";
      case 0x50:
         return "HD DVD-ROM";
      case 0x51:
         return "HD DVD-R";
      case 0x52:
         return "HD DVD-RAM";
      case 0x53:
         return "HD DVD-RW";
      case 0x58:
         return "HD DVD-R Dual Layer";
      case 0x5A:
         return "HD DVD-RW Dual Layer";
      default:
         break;
   }

   return "Unknown";
}

/* TODO/FIXME - sense never used here? */
int cdrom_get_sense(libretro_vfs_implementation_file *stream, unsigned char *sense, size_t len)
{
   unsigned char cdb[CDROM_MIN_BUFSIZE] = {0x3, 0, 0, 0, 0xFC, 0};
   unsigned char buf[0xFC] = {0};
   int rv = cdrom_send_command(stream, DIRECTION_IN, buf, sizeof(buf), cdb, sizeof(cdb), 0);

#ifdef CDROM_DEBUG
   printf("[CDROM] get sense data status code %d\n", rv);
   fflush(stdout);
#endif

   if (rv)
      return 1;

#ifdef CDROM_DEBUG
   cdrom_print_sense_data(buf, sizeof(buf));
#endif

   return 0;
}

void cdrom_get_current_config_random_readable(libretro_vfs_implementation_file *stream)
{
   unsigned char cdb[] = {0x46, 0x2, 0, 0x10, 0, 0, 0, 0, 0x14, 0};
   unsigned char buf[0x14] = {0};
   int rv = cdrom_send_command(stream, DIRECTION_IN, buf, sizeof(buf), cdb, sizeof(cdb), 0);
   int i;

   printf("[CDROM] get current config random readable status code %d\n", rv);

   if (rv)
      return;

   printf("[CDROM] Feature Header: ");

   for (i = 0; i < 8; i++)
   {
      printf("%02X ", buf[i]);
   }

   printf("\n");

   printf("[CDROM] Random Readable Feature Descriptor: ");

   for (i = 0; i < 12; i++)
   {
      printf("%02X ", buf[8 + i]);
   }

   printf("\n");

   printf("[CDROM] Supported commands: READ CAPACITY, READ (10)\n");
}

void cdrom_get_current_config_multiread(libretro_vfs_implementation_file *stream)
{
   unsigned char cdb[] = {0x46, 0x2, 0, 0x1D, 0, 0, 0, 0, 0xC, 0};
   unsigned char buf[0xC] = {0};
   int rv = cdrom_send_command(stream, DIRECTION_IN, buf, sizeof(buf), cdb, sizeof(cdb), 0);
   int i;

   printf("[CDROM] get current config multi-read status code %d\n", rv);

   if (rv)
      return;

   printf("[CDROM] Feature Header: ");

   for (i = 0; i < 8; i++)
   {
      printf("%02X ", buf[i]);
   }

   printf("\n");

   printf("[CDROM] Multi-Read Feature Descriptor: ");

   for (i = 0; i < 4; i++)
   {
      printf("%02X ", buf[8 + i]);
   }

   printf("\n");

   printf("[CDROM] Supported commands: READ (10), READ CD, READ DISC INFORMATION, READ TRACK INFORMATION\n");
}

void cdrom_get_current_config_cdread(libretro_vfs_implementation_file *stream)
{
   unsigned char cdb[] = {0x46, 0x2, 0, 0x1E, 0, 0, 0, 0, 0x10, 0};
   unsigned char buf[0x10] = {0};
   int rv = cdrom_send_command(stream, DIRECTION_IN, buf, sizeof(buf), cdb, sizeof(cdb), 0);
   int i;

   printf("[CDROM] get current config cd read status code %d\n", rv);

   if (rv)
      return;

   printf("[CDROM] Feature Header: ");

   for (i = 0; i < 8; i++)
   {
      printf("%02X ", buf[i]);
   }

   printf("\n");

   printf("[CDROM] CD Read Feature Descriptor: ");

   for (i = 0; i < 8; i++)
   {
      printf("%02X ", buf[8 + i]);
   }

   if (buf[8 + 2] & 1)
      printf("(current)\n");

   printf("[CDROM] Supported commands: READ CD, READ CD MSF, READ TOC/PMA/ATIP\n");
}

void cdrom_get_current_config_profiles(libretro_vfs_implementation_file *stream)
{
   unsigned char cdb[] = {0x46, 0x2, 0, 0x0, 0, 0, 0, 0xFF, 0xFA, 0};
   unsigned char buf[0xFFFA] = {0};
   int rv = cdrom_send_command(stream, DIRECTION_IN, buf, sizeof(buf), cdb, sizeof(cdb), 0);
   int i;

   printf("[CDROM] get current config profiles status code %d\n", rv);

   if (rv)
      return;

   printf("[CDROM] Feature Header: ");

   for (i = 0; i < 8; i++)
   {
      printf("%02X ", buf[i]);
   }

   printf("\n");

   printf("[CDROM] Profile List Descriptor: ");

   for (i = 0; i < 4; i++)
   {
      printf("%02X ", buf[8 + i]);
   }

   printf("\n");

   printf("[CDROM] Number of profiles: %u\n", buf[8 + 3] / 4);

   for (i = 0; i < buf[8 + 3] / 4; i++)
   {
      unsigned short profile = (buf[8 + (4 * (i + 1))] << 8) | buf[8 + (4 * (i + 1)) + 1];

      printf("[CDROM] Profile Number: %04X (%s) ", profile, cdrom_get_profile(profile));

      if (buf[8 + (4 * (i + 1)) + 2] & 1)
         printf("(current)\n");
      else
         printf("\n");
   }
}

void cdrom_get_current_config_core(libretro_vfs_implementation_file *stream)
{
   unsigned char cdb[] = {0x46, 0x2, 0, 0x1, 0, 0, 0, 0, 0x14, 0};
   unsigned char buf[20] = {0};
   unsigned intf_std = 0;
   int rv = cdrom_send_command(stream, DIRECTION_IN, buf, sizeof(buf), cdb, sizeof(cdb), 0);
   int i;
   const char *intf_std_name = "Unknown";

   printf("[CDROM] get current config core status code %d\n", rv);

   if (rv)
      return;

   printf("[CDROM] Feature Header: ");

   for (i = 0; i < 8; i++)
   {
      printf("%02X ", buf[i]);
   }

   printf("\n");

   if (buf[6] == 0 && buf[7] == 8)
      printf("[CDROM] Current Profile: CD-ROM\n");
   else
      printf("[CDROM] Current Profile: %02X%02X\n", buf[6], buf[7]);

   printf("[CDROM] Core Feature Descriptor: ");

   for (i = 0; i < 12; i++)
   {
      printf("%02X ", buf[8 + i]);
   }

   printf("\n");

   intf_std = buf[8 + 4] << 24 | buf[8 + 5] << 16 | buf[8 + 6] << 8 | buf[8 + 7];

   switch (intf_std)
   {
      case 0:
         intf_std_name = "Unspecified";
         break;
      case 1:
         intf_std_name = "SCSI Family";
         break;
      case 2:
         intf_std_name = "ATAPI";
         break;
      case 7:
         intf_std_name = "Serial ATAPI";
         break;
      case 8:
         intf_std_name = "USB";
         break;
      default:
         break;
   }

   printf("[CDROM] Physical Interface Standard: %u (%s)\n", intf_std, intf_std_name);
}

int cdrom_read_subq(libretro_vfs_implementation_file *stream, unsigned char *s, size_t len)
{
   /* MMC Command: READ TOC/PMA/ATIP */
   unsigned char cdb[] = {0x43, 0x2, 0x2, 0, 0, 0, 0x1, 0x9, 0x30, 0};
#ifdef CDROM_DEBUG
   unsigned short data_len = 0;
   unsigned char first_session = 0;
   unsigned char last_session = 0;
   int i;
#endif
   int rv;

   if (!s)
      return 1;

   rv = cdrom_send_command(stream, DIRECTION_IN, s, len, cdb, sizeof(cdb), 0);

   if (rv)
     return 1;

#ifdef CDROM_DEBUG
   data_len      = s[0] << 8 | s[1];
   first_session = s[2];
   last_session  = s[3];

   printf("[CDROM] Data Length: %d\n", data_len);
   printf("[CDROM] First Session: %d\n", first_session);
   printf("[CDROM] Last Session: %d\n", last_session);

   for (i = 0; i < (data_len - 2) / 11; i++)
   {
      unsigned char session_num = s[4 + (i * 11) + 0];
      unsigned char adr         = (s[4 + (i * 11) + 1] >> 4) & 0xF;
#if 0
      unsigned char control     = s[4 + (i * 11) + 1] & 0xF;
#endif
      unsigned char tno         = s[4 + (i * 11) + 2];
      unsigned char point       = s[4 + (i * 11) + 3];
      unsigned char pmin        = s[4 + (i * 11) + 8];
      unsigned char psec        = s[4 + (i * 11) + 9];
      unsigned char pframe      = s[4 + (i * 11) + 10];

      /*printf("i %d control %d adr %d tno %d point %d: ", i, control, adr, tno, point);*/
      /* why is control always 0? */

      if (/*(control == 4 || control == 6) && */adr == 1 && tno == 0 && point >= 1 && point <= 99)
      {
         printf("[CDROM] - Session#: %d TNO %d POINT %d ", session_num, tno, point);
         printf("Track start time: (aMSF %02u:%02u:%02u) ", (unsigned)pmin, (unsigned)psec, (unsigned)pframe);
      }
      else if (/*(control == 4 || control == 6) && */adr == 1 && tno == 0 && point == 0xA0)
      {
         printf("[CDROM] - Session#: %d TNO %d POINT %d ", session_num, tno, point);
         printf("First Track Number: %d ", pmin);
         printf("Disc Type: %d ", psec);
      }
      else if (/*(control == 4 || control == 6) && */adr == 1 && tno == 0 && point == 0xA1)
      {
         printf("[CDROM] - Session#: %d TNO %d POINT %d ", session_num, tno, point);
         printf("Last Track Number: %d ", pmin);
      }
      else if (/*(control == 4 || control == 6) && */adr == 1 && tno == 0 && point == 0xA2)
      {
         printf("[CDROM] - Session#: %d TNO %d POINT %d ", session_num, tno, point);
         printf("Lead-out start time: (aMSF %02u:%02u:%02u) ", (unsigned)pmin, (unsigned)psec, (unsigned)pframe);
      }

      printf("\n");
   }

   fflush(stdout);
#endif
   return 0;
}

static int cdrom_read_track_info(libretro_vfs_implementation_file *stream, unsigned char track, cdrom_toc_t *toc)
{
   /* MMC Command: READ TRACK INFORMATION */
   unsigned char cdb[] = {0x52, 0x1, 0, 0, 0, 0, 0, 0x1, 0x80, 0};
   unsigned char buf[384] = {0};
   unsigned lba = 0;
   unsigned track_size = 0;
   int rv;
   ssize_t pregap_lba_len;

   cdb[5] = track;

   if ((rv = cdrom_send_command(stream, DIRECTION_IN, buf,
         sizeof(buf), cdb, sizeof(cdb), 0)))
     return 1;

   memcpy(&lba, buf + 8, 4);
   memcpy(&track_size, buf + 24, 4);

   lba = swap_if_little32(lba);
   track_size = swap_if_little32(track_size);

   /* lba_start may be earlier than the MSF start times seen in read_subq */
   toc->track[track - 1].lba_start = lba;
   toc->track[track - 1].track_size = track_size;

   pregap_lba_len = (toc->track[track - 1].audio ? 0 : (toc->track[track - 1].lba - toc->track[track - 1].lba_start));

   toc->track[track - 1].track_bytes = (track_size - pregap_lba_len) * 2352;
   toc->track[track - 1].mode = buf[6] & 0xF;

#ifdef CDROM_DEBUG
   printf("[CDROM] Track %d Info: ", track);
   printf("Copy: %d ", (buf[5] & 0x10) > 0);
   printf("Data Mode: %d ", toc->track[track - 1].mode);
   printf("LBA Start: %d (%d) ", lba, toc->track[track - 1].lba);
   printf("Track Size: %d\n", track_size);
   fflush(stdout);
#endif

   return 0;
}

int cdrom_set_read_speed(libretro_vfs_implementation_file *stream, unsigned speed)
{
   /* MMC Command: SET CD SPEED */
   unsigned char cmd[] = {0xBB, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

   cmd[2] = (speed >> 24) & 0xFF;
   cmd[3] = (speed >> 16) & 0xFF;
   cmd[4] = (speed >> 8) & 0xFF;
   cmd[5] = speed & 0xFF;

   return cdrom_send_command(stream, DIRECTION_NONE, NULL, 0, cmd, sizeof(cmd), 0);
}

int cdrom_write_cue(libretro_vfs_implementation_file *stream, char **out_buf, size_t *out_len, char cdrom_drive, unsigned char *num_tracks, cdrom_toc_t *toc)
{
   int i;
   unsigned char buf[2352] = {0};
   unsigned short data_len = 0;
   size_t _len = 0, pos = 0;
   int rv = 0;

   if (!out_buf || !out_len || !num_tracks || !toc)
   {
#ifdef CDROM_DEBUG
      printf("[CDROM] Invalid buffer/length pointer for CDROM cue sheet\n");
      fflush(stdout);
#endif
      return 1;
   }

   cdrom_set_read_speed(stream, 0xFFFFFFFF);

   rv = cdrom_read_subq(stream, buf, sizeof(buf));

   if (rv)
      return rv;

   data_len = buf[0] << 8 | buf[1];

   for (i = 0; i < (data_len - 2) / 11; i++)
   {
      unsigned char adr = (buf[4 + (i * 11) + 1] >> 4) & 0xF;
      unsigned char tno = buf[4 + (i * 11) + 2];
      unsigned char point = buf[4 + (i * 11) + 3];
      unsigned char pmin = buf[4 + (i * 11) + 8];

      if (/*(control == 4 || control == 6) && */adr == 1 && tno == 0 && point == 0xA1)
      {
         *num_tracks = pmin;
#ifdef CDROM_DEBUG
         printf("[CDROM] Number of CDROM tracks: %d\n", *num_tracks);
         fflush(stdout);
#endif
         break;
      }
   }

   if (!*num_tracks || *num_tracks > 99)
   {
#ifdef CDROM_DEBUG
      printf("[CDROM] Invalid number of CDROM tracks: %d\n", *num_tracks);
      fflush(stdout);
#endif
      return 1;
   }

   _len            = CDROM_CUE_TRACK_BYTES * (*num_tracks);
   toc->num_tracks = *num_tracks;
   *out_buf        = (char*)calloc(1, _len);
   *out_len        = _len;

   for (i = 0; i < (data_len - 2) / 11; i++)
   {
      /*unsigned char session_num = buf[4 + (i * 11) + 0];*/
      unsigned char adr = (buf[4 + (i * 11) + 1] >> 4) & 0xF;
      unsigned char control = buf[4 + (i * 11) + 1] & 0xF;
      unsigned char tno = buf[4 + (i * 11) + 2];
      unsigned char point = buf[4 + (i * 11) + 3];
      /*unsigned char amin = buf[4 + (i * 11) + 4];
      unsigned char asec = buf[4 + (i * 11) + 5];
      unsigned char aframe = buf[4 + (i * 11) + 6];*/
      unsigned char pmin = buf[4 + (i * 11) + 8];
      unsigned char psec = buf[4 + (i * 11) + 9];
      unsigned char pframe = buf[4 + (i * 11) + 10];
      unsigned lba = cdrom_msf_to_lba(pmin, psec, pframe);

      /*printf("i %d control %d adr %d tno %d point %d: amin %d asec %d aframe %d pmin %d psec %d pframe %d\n", i, control, adr, tno, point, amin, asec, aframe, pmin, psec, pframe);*/
      /* why is control always 0? */

      if (/*(control == 4 || control == 6) && */adr == 1 && tno == 0 && point >= 1 && point <= 99)
      {
         bool audio = false;
         const char *track_type = "MODE1/2352";

         audio = (!(control & 0x4) && !(control & 0x5));

#ifdef CDROM_DEBUG
         printf("[CDROM] Track %02d CONTROL %01X ADR %01X AUDIO? %d\n", point, control, adr, audio);
         fflush(stdout);
#endif

         toc->track[point - 1].track_num = point;
         toc->track[point - 1].min = pmin;
         toc->track[point - 1].sec = psec;
         toc->track[point - 1].frame = pframe;
         toc->track[point - 1].lba = lba;
         toc->track[point - 1].audio = audio;

         cdrom_read_track_info(stream, point, toc);

         if (audio)
            track_type = "AUDIO";
         else if (toc->track[point - 1].mode == 1)
            track_type = "MODE1/2352";
         else if (toc->track[point - 1].mode == 2)
            track_type = "MODE2/2352";

#if defined(_WIN32) && !defined(_XBOX)
         pos += snprintf(*out_buf + pos, _len - pos, "FILE \"cdrom://%c:/drive-track%02d.bin\" BINARY\n", cdrom_drive, point);
#else
         pos += snprintf(*out_buf + pos, _len - pos, "FILE \"cdrom://drive%c-track%02d.bin\" BINARY\n", cdrom_drive, point);
#endif
         pos += snprintf(*out_buf + pos, _len - pos, "  TRACK %02d %s\n", point, track_type);

         {
            unsigned pregap_lba_len = toc->track[point - 1].lba - toc->track[point - 1].lba_start;

            if (toc->track[point - 1].audio && pregap_lba_len > 0)
            {
               unsigned char min = 0;
               unsigned char sec = 0;
               unsigned char frame = 0;

               cdrom_lba_to_msf(pregap_lba_len, &min, &sec, &frame);

               pos += snprintf(*out_buf + pos, _len - pos, "    INDEX 00 00:00:00\n");
               pos += snprintf(*out_buf + pos, _len - pos, "    INDEX 01 %02u:%02u:%02u\n", (unsigned)min, (unsigned)sec, (unsigned)frame);
            }
            else
               pos += snprintf(*out_buf + pos, _len - pos, "    INDEX 01 00:00:00\n");
         }
      }
   }

   return 0;
}

/* needs 32 bytes for full vendor, product and version */
int cdrom_get_inquiry(libretro_vfs_implementation_file *stream, char *s, size_t len, bool *is_cdrom)
{
   /* MMC Command: INQUIRY */
   unsigned char cdb[CDROM_MIN_BUFSIZE] = {0x12, 0, 0, 0, 0xff, 0};
   unsigned char buf[256] = {0};
   int rv = cdrom_send_command(stream, DIRECTION_IN, buf, sizeof(buf), cdb, sizeof(cdb), 0);
   bool cdrom = false;

   if (rv)
      return 1;

   if (s && len >= 32)
   {
      memset(s, 0, len);
      /* vendor */
      memcpy(s, buf + 8, 8);
      s[8] = ' ';
      /* product */
      memcpy(s + 9, buf + 16, 16);
      s[25] = ' ';
      /* version */
      memcpy(s + 26, buf + 32, 4);
   }

   cdrom = (buf[0] == 5);

   if (is_cdrom && cdrom)
      *is_cdrom = true;

#ifdef CDROM_DEBUG
   printf("[CDROM] Device Model: %s (is CD-ROM? %s)\n", s, (cdrom ? "yes" : "no"));
#endif
   return 0;
}

int cdrom_read(libretro_vfs_implementation_file *stream,
      cdrom_group_timeouts_t *timeouts, unsigned char min,
      unsigned char sec, unsigned char frame, void *s,
      size_t len, size_t skip)
{
   /* MMC Command: READ CD MSF */
   unsigned char cdb[] = {0xB9, 0, 0, 0, 0, 0, 0, 0, 0, 0xF8, 0, 0};
   int rv;
   double frames = ceil((len + skip) / 2352.0);
   unsigned frame_end = cdrom_msf_to_lba(min, sec, frame) + frames;

   cdb[3] = min;
   cdb[4] = sec;
   cdb[5] = frame;

   if (frames <= 1)
   {
      cdrom_lba_to_msf(frame_end, &cdb[6], &cdb[7], &cdb[8]);
#ifdef CDROM_DEBUG
      printf("[CDROM] single-frame read: %d %d %d skip %" PRId64 "\n", cdb[3], cdb[4], cdb[5], skip);
      fflush(stdout);
#endif
   }
   else
   {
      cdrom_lba_to_msf(frame_end, &cdb[6], &cdb[7], &cdb[8]);

#ifdef CDROM_DEBUG
      printf("[CDROM] multi-frame read: %d sectors starting from %02d:%02d:%02d skip %" PRId64 "\n", (int)frames, cdb[3], cdb[4], cdb[5], skip);
      fflush(stdout);
#endif
   }

   /* regardless of the length specified here, a new buffer will be allocated and padded to a sector multiple inside cdrom_send_command */
   rv = cdrom_send_command(stream, DIRECTION_IN, s, len, cdb, sizeof(cdb), skip);

#ifdef CDROM_DEBUG
   printf("[CDROM] read msf status code %d\n", rv);
   fflush(stdout);
#endif

   if (rv)
   {
      stream->cdrom.last_frame_valid = false;
      return 1;
   }

   return 0;
}

int cdrom_stop(libretro_vfs_implementation_file *stream)
{
   /* MMC Command: START STOP UNIT */
   unsigned char cdb[CDROM_MIN_BUFSIZE] = {0x1B, 0, 0, 0, 0x0, 0};
   int rv = cdrom_send_command(stream, DIRECTION_NONE, NULL, 0, cdb, sizeof(cdb), 0);

#ifdef CDROM_DEBUG
   printf("[CDROM] stop status code %d\n", rv);
   fflush(stdout);
#endif

   if (rv)
      return 1;

   return 0;
}

int cdrom_unlock(libretro_vfs_implementation_file *stream)
{
   /* MMC Command: PREVENT ALLOW MEDIUM REMOVAL */
   unsigned char cdb[CDROM_MIN_BUFSIZE] = {0x1E, 0, 0, 0, 0x2, 0};
   int rv = cdrom_send_command(stream, DIRECTION_NONE, NULL, 0, cdb, sizeof(cdb), 0);

#ifdef CDROM_DEBUG
   printf("[CDROM] persistent prevent clear status code %d\n", rv);
   fflush(stdout);
#endif

   if (rv)
      return 1;

   cdb[4] = 0x0;

   rv = cdrom_send_command(stream, DIRECTION_NONE, NULL, 0, cdb, sizeof(cdb), 0);

#ifdef CDROM_DEBUG
   printf("[CDROM] prevent clear status code %d\n", rv);
   fflush(stdout);
#endif

   if (rv)
      return 1;

   return 0;
}

int cdrom_open_tray(libretro_vfs_implementation_file *stream)
{
   /* MMC Command: START STOP UNIT */
   unsigned char cdb[CDROM_MIN_BUFSIZE] = {0x1B, 0, 0, 0, 0x2, 0};
   int rv;

   cdrom_unlock(stream);
   cdrom_stop(stream);

   rv = cdrom_send_command(stream, DIRECTION_NONE, NULL, 0, cdb, sizeof(cdb), 0);

#ifdef CDROM_DEBUG
   printf("[CDROM] open tray status code %d\n", rv);
   fflush(stdout);
#endif

   if (rv)
      return 1;

   return 0;
}

int cdrom_close_tray(libretro_vfs_implementation_file *stream)
{
   /* MMC Command: START STOP UNIT */
   unsigned char cdb[CDROM_MIN_BUFSIZE] = {0x1B, 0, 0, 0, 0x3, 0};
   int rv = cdrom_send_command(stream, DIRECTION_NONE, NULL, 0, cdb, sizeof(cdb), 0);

#ifdef CDROM_DEBUG
   printf("[CDROM] close tray status code %d\n", rv);
   fflush(stdout);
#endif

   if (rv)
      return 1;

   return 0;
}

struct string_list* cdrom_get_available_drives(void)
{
   struct string_list *list = string_list_new();
#if defined(__linux__) && !defined(ANDROID)
   struct string_list *dir_list = dir_list_new("/dev", NULL, false, false, false, false);
   int i;
   bool found = false;

   if (!dir_list)
      return list;

   for (i = 0; i < (int)dir_list->size; i++)
   {
      if (string_starts_with_size(dir_list->elems[i].data, "/dev/sg",
               STRLEN_CONST("/dev/sg")))
      {
         char drive_string[33];
         libretro_vfs_implementation_file *stream;
         char drive_model[32]             = {0};
         union string_list_elem_attr attr = {0};
         int dev_index                    = 0;
         RFILE *file                      = filestream_open(
               dir_list->elems[i].data, RETRO_VFS_FILE_ACCESS_READ, 0);
         bool is_cdrom                    = false;

         found = true;

         if (!file)
         {
#ifdef CDROM_DEBUG
            printf("[CDROM] Could not open %s, please check permissions.\n", dir_list->elems[i].data);
            fflush(stdout);
#endif
            continue;
         }

         stream = filestream_get_vfs_handle(file);
         cdrom_get_inquiry(stream, drive_model, sizeof(drive_model), &is_cdrom);
         filestream_close(file);

         if (!is_cdrom)
            continue;

         sscanf(dir_list->elems[i].data + STRLEN_CONST("/dev/sg"),
               "%d", &dev_index);

         dev_index = '0' + dev_index;
         attr.i    = dev_index;

         if (!string_is_empty(drive_model))
            strlcpy(drive_string, drive_model, sizeof(drive_string));
         else
            strlcpy(drive_string, "Unknown Drive", sizeof(drive_string));

         string_list_append(list, drive_string, attr);
      }
   }

   if (!found)
   {
      char *buf   = NULL;
      int64_t len = 0;

      if (filestream_read_file("/proc/modules", (void**)&buf, &len))
      {
#ifdef CDROM_DEBUG
         bool found              = false;
#endif
         struct string_list mods = {0};

         string_list_initialize(&mods);

         if (string_split_noalloc(&mods, buf, "\n"))
         {
            for (i = 0; i < (int)mods.size; i++)
            {
               if (strcasestr(mods.elems[i].data, "sg "))
               {
#ifdef CDROM_DEBUG
                  found = true;
#endif
                  break;
               }
            }
         }
         string_list_deinitialize(&mods);
         free(buf);

#ifdef CDROM_DEBUG
         if (found)
         {
            printf("[CDROM] No sg devices found but kernel module is loaded.\n");
            fflush(stdout);
         }
         else
         {
            printf("[CDROM] No sg devices found and sg kernel module is not loaded.\n");
            fflush(stdout);
         }
#endif
      }
#ifdef CDROM_DEBUG
      else
      {
         printf("[CDROM] No sg devices found, could not check if sg kernel module is loaded.\n");
         fflush(stdout);
      }
#endif
   }

   string_list_free(dir_list);
#endif
#if defined(_WIN32) && !defined(_XBOX)
   DWORD drive_mask = GetLogicalDrives();
   int i;

   for (i = 0; i < (int)(sizeof(DWORD) * 8); i++)
   {
      char path[]       = {"a:\\"};
      char cdrom_path[] = {"cdrom://a:/drive-track01.bin"};

      path[0]          += i;
      cdrom_path[8]    += i;

      /* this drive letter doesn't exist */
      if (!(drive_mask & (1 << i)))
         continue;

      if (GetDriveType(path) != DRIVE_CDROM)
         continue;

      {
         char drive_string[33];
         libretro_vfs_implementation_file *stream;
         bool is_cdrom                    = false;
         char drive_model[32]             = {0};
         union string_list_elem_attr attr = {0};
         RFILE *file = filestream_open(cdrom_path, RETRO_VFS_FILE_ACCESS_READ, 0);
         if (!file)
            continue;

         stream = filestream_get_vfs_handle(file);
         cdrom_get_inquiry(stream, drive_model, sizeof(drive_model), &is_cdrom);
         filestream_close(file);

         if (!is_cdrom)
            continue;

         attr.i = path[0];

         if (!string_is_empty(drive_model))
            strlcpy(drive_string, drive_model, sizeof(drive_string));
         else
            strlcpy(drive_string, "Unknown Drive", sizeof(drive_string));

         string_list_append(list, drive_string, attr);
      }
   }
#endif
   return list;
}

bool cdrom_is_media_inserted(libretro_vfs_implementation_file *stream)
{
   /* MMC Command: TEST UNIT READY */
   unsigned char cdb[CDROM_MIN_BUFSIZE] = {0x00, 0, 0, 0, 0, 0};
   int rv = cdrom_send_command(stream, DIRECTION_NONE, NULL, 0, cdb, sizeof(cdb), 0);

#ifdef CDROM_DEBUG
   printf("[CDROM] media inserted status code %d\n", rv);
   fflush(stdout);
#endif

   /* Will also return false if the drive is simply not ready yet (tray open, disc spinning back up after tray closed etc).
    * Command will not block or wait for media to become ready. */
   if (rv)
      return false;

   return true;
}

bool cdrom_drive_has_media(const char drive)
{
   RFILE *file;
   char cdrom_path_bin[256] = {0};

   cdrom_device_fillpath(cdrom_path_bin, sizeof(cdrom_path_bin), drive, 1, false);

   file = filestream_open(cdrom_path_bin, RETRO_VFS_FILE_ACCESS_READ, 0);

   if (file)
   {
      libretro_vfs_implementation_file *stream = filestream_get_vfs_handle(file);
      bool has_media = cdrom_is_media_inserted(stream);

      filestream_close(file);

      return has_media;
   }

   return false;
}

bool cdrom_set_read_cache(libretro_vfs_implementation_file *stream, bool enabled)
{
   int i;
   /* MMC Command: MODE SENSE (10) and MODE SELECT (10) */
   unsigned char cdb_sense_changeable[] = {0x5A, 0, 0x48, 0, 0, 0, 0, 0, 0x14, 0};
   unsigned char cdb_sense[]            = {0x5A, 0, 0x8, 0, 0, 0, 0, 0, 0x14, 0};
   unsigned char cdb_select[]           = {0x55, 0x10, 0, 0, 0, 0, 0, 0, 0x14, 0};
   unsigned char buf[20]                = {0};
   int rv = cdrom_send_command(stream, DIRECTION_IN, buf, sizeof(buf),
         cdb_sense_changeable, sizeof(cdb_sense_changeable), 0);

#ifdef CDROM_DEBUG
   printf("[CDROM] mode sense changeable status code %d\n", rv);
   fflush(stdout);
#endif

   if (rv)
      return false;

   if (!(buf[10] & 0x1))
   {
      /* RCD (read cache disable) bit is not changeable */
#ifdef CDROM_DEBUG
      printf("[CDROM] RCD (read cache disable) bit is not changeable.\n");
      fflush(stdout);
#endif
      return false;
   }

   memset(buf, 0, sizeof(buf));

   rv = cdrom_send_command(stream, DIRECTION_IN, buf, sizeof(buf), cdb_sense, sizeof(cdb_sense), 0);

#ifdef CDROM_DEBUG
   printf("mode sense status code %d\n", rv);
   fflush(stdout);
#endif

   if (rv)
      return false;

#ifdef CDROM_DEBUG
   printf("Mode sense data for caching mode page: ");

   for (i = 0; i < (int)sizeof(buf); i++)
      printf("%02X ", buf[i]);

   printf("\n");
   fflush(stdout);
#endif

   /* "When transferred during execution of the MODE SELECT (10) command, Mode Data Length is reserved." */
   for (i = 0; i < 8; i++)
      buf[i] = 0;

   if (enabled)
      buf[10] &= ~1;
   else
      buf[10] |=  1;

   rv = cdrom_send_command(stream, DIRECTION_OUT, buf, sizeof(buf), cdb_select, sizeof(cdb_select), 0);

#ifdef CDROM_DEBUG
   printf("mode select status code %d\n", rv);
   fflush(stdout);
#endif

   if (rv)
      return false;

   return true;
}

bool cdrom_get_timeouts(libretro_vfs_implementation_file *stream, cdrom_group_timeouts_t *timeouts)
{
   /* MMC Command: MODE SENSE (10) */
   int rv;
   unsigned char cdb[]   = {0x5A, 0, 0x1D, 0, 0, 0, 0, 0, 0x14, 0};
   unsigned char buf[20] = {0};
   unsigned short g1     = 0;
   unsigned short g2     = 0;
   unsigned short g3     = 0;

   if (!timeouts)
      return false;

   rv = cdrom_send_command(stream, DIRECTION_IN, buf, sizeof(buf), cdb, sizeof(cdb), 0);

#ifdef CDROM_DEBUG
   printf("get timeouts status code %d\n", rv);
   fflush(stdout);
#endif

   if (rv)
      return false;

   g1 = buf[14] << 8 | buf[15];
   g2 = buf[16] << 8 | buf[17];
   g3 = buf[18] << 8 | buf[19];

#ifdef CDROM_DEBUG
   {
      int i;

      printf("Mode sense data for timeout groups: ");

      for (i = 0; i < (int)sizeof(buf); i++)
         printf("%02X ", buf[i]);

      printf("\n");

      printf("Group 1 Timeout: %d\n", g1);
      printf("Group 2 Timeout: %d\n", g2);
      printf("Group 3 Timeout: %d\n", g3);

      fflush(stdout);
   }
#endif

   timeouts->g1_timeout = g1;
   timeouts->g2_timeout = g2;
   timeouts->g3_timeout = g3;

   return true;
}

bool cdrom_has_atip(libretro_vfs_implementation_file *stream)
{
   /* MMC Command: READ TOC/PMA/ATIP */
   unsigned char cdb[]     = {0x43, 0x2, 0x4, 0, 0, 0, 0, 0x9, 0x30, 0};
   unsigned char buf[32]   = {0};
   unsigned short atip_len = 0;
   int rv = cdrom_send_command(stream, DIRECTION_IN, buf, sizeof(buf), cdb, sizeof(cdb), 0);

   if (rv)
     return false;

   atip_len = buf[0] << 8 | buf[1];

#ifdef CDROM_DEBUG
   printf("ATIP Length %d, Disc Type %d, Disc Sub-Type %d\n",
         atip_len,
         (buf[6]  >> 6) & 0x1,
         ((buf[6] >> 5) & 0x1) << 2 | ((buf[6] >> 4) & 0x1) << 1 | ((buf[6] >> 3) & 0x1) << 0);
#endif

   if (atip_len < 5)
      return false;

   return true;
}

size_t cdrom_device_fillpath(char *s, size_t len, char drive, unsigned char track, bool is_cue)
{
   if (s && len > 0)
   {
      if (is_cue)
      {
#ifdef _WIN32
         size_t _len = strlcpy(s, "cdrom://", len);
         if (len > _len)
            s[_len++] = drive;
         _len += strlcpy(s + _len, ":/drive.cue", len - _len);
         return _len;
#else
#ifdef __linux__
         size_t _len = strlcpy(s, "cdrom://drive", len);
         if (len > _len + 1)
         {
            s[_len++] = drive;
            s[_len]   = '\0';
         }
         _len += strlcpy(s + _len, ".cue", len - _len);
         return _len;
#endif
#endif
      }
      else
      {
#ifdef _WIN32
         size_t _len = strlcpy(s, "cdrom://", len);
         if (len > _len + 1)
         {
            s[_len++] = drive;
            s[_len]   = '\0';
         }
         _len += snprintf(s + _len, len - _len, ":/drive-track%02d.bin", track);
         return _len;
#else
#ifdef __linux__
         size_t _len = strlcpy(s, "cdrom://drive", len);
         if (len > _len)
            s[_len++] = drive;
         _len += snprintf(s + _len, len - _len, "-track%02d.bin", track);
         return _len;
#endif
#endif
      }
   }
   return 0;
}
