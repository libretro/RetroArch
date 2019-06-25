/* Copyright  (C) 2010-2019 The RetroArch team
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
#include <retro_math.h>
#include <streams/file_stream.h>
#include <retro_endianness.h>
#include <retro_miscellaneous.h>

#include <math.h>
#include <unistd.h>

#ifdef __linux__
#include <stropts.h>
#include <scsi/sg.h>
#endif

#define CDROM_CUE_TRACK_BYTES 86

typedef enum
{
   DIRECTION_NONE,
   DIRECTION_IN,
   DIRECTION_OUT
} CDROM_CMD_Direction;

void lba_to_msf(unsigned lba, unsigned char *min, unsigned char *sec, unsigned char *frame)
{
   if (!min || !sec || !frame)
      return;

   *frame = lba % 75;
   lba /= 75;
   *sec = lba % 60;
   lba /= 60;
   *min = lba;
}

unsigned msf_to_lba(unsigned char min, unsigned char sec, unsigned char frame)
{
   return (min * 60 + sec) * 75 + frame;
}

void increment_msf(unsigned char *min, unsigned char *sec, unsigned char *frame)
{
   if (!min || !sec || !frame)
      return;

   *min = (*frame == 74) ? (*sec < 59 ? *min : *min + 1) : *min;
   *sec = (*frame == 74) ? (*sec < 59 ? (*sec + 1) : 0) : *sec;
   *frame = (*frame < 74) ? (*frame + 1) : 0;
}

static int cdrom_send_command(int fd, CDROM_CMD_Direction dir, void *buf, size_t len, unsigned char *cmd, size_t cmd_len, size_t skip)
{
#ifdef __linux__
   sg_io_hdr_t sgio = {0};
   unsigned char sense[SG_MAX_SENSE] = {0};
   unsigned char *xfer_buf;
   int rv;
   unsigned char retries_left = 10;

   if (!cmd || cmd_len == 0)
      return 1;

   xfer_buf = (unsigned char*)calloc(1, len + skip);

   if (!xfer_buf)
      return 1;

   sgio.interface_id = 'S';

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

   sgio.cmd_len = cmd_len;
   sgio.cmdp = cmd;

   if (xfer_buf)
      sgio.dxferp = xfer_buf;

   if (len)
      sgio.dxfer_len = len + skip;

   sgio.sbp = sense;
   sgio.mx_sb_len = sizeof(sense);
   sgio.timeout = 30000;
retry:
#ifdef CDROM_DEBUG
   {
      unsigned i;

      printf("CDROM Send Command: ");

      for (i = 0; i < cmd_len / sizeof(*cmd); i++)
      {
         printf("%02X ", cmd[i]);
      }

      printf("\n");
   }
#endif
   rv = ioctl(fd, SG_IO, &sgio);

   if (sgio.info & SG_INFO_CHECK)
   {
      unsigned i;
      const char *sense_key_text = NULL;

      switch (sense[2] & 0xF)
      {
         case 0:
         case 2:
         case 3:
         case 4:
         case 6:
            if (retries_left)
            {
#ifdef CDROM_DEBUG
               printf("CDROM Read Retry...\n");
#endif
               retries_left--;
               usleep(1000 * 1000);
               goto retry;
            }
            else
            {
#ifdef CDROM_DEBUG
               printf("CDROM Read Retries failed, giving up.\n");
#endif
            }

            break;
         default:
            break;
      }

#ifdef CDROM_DEBUG
      printf("CHECK CONDITION\n");

      for (i = 0; i < SG_MAX_SENSE; i++)
      {
         printf("%02X ", sense[i]);
      }

      printf("\n");

      if (sense[0] == 0x70)
         printf("CURRENT ERROR:\n");
      if (sense[0] == 0x71)
         printf("DEFERRED ERROR:\n");

      switch (sense[2] & 0xF)
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

      printf("Sense Key: %02X (%s)\n", sense[2] & 0xF, sense_key_text);
      printf("ASC: %02X\n", sense[12]);
      printf("ASCQ: %02X\n", sense[13]);
#endif
   }

   if (rv == 0 && buf)
   {
      memcpy(buf, xfer_buf + skip, len);
   }

   if (xfer_buf)
      free(xfer_buf);

   if (rv == -1)
      return 1;

   return 0;
#else
   (void)fd;
   (void)dir;
   (void)buf;
   (void)len;
   (void)cmd;
   (void)cmd_len;

   return 1;
#endif
}

int cdrom_read_subq(int fd, unsigned char *buf, size_t len)
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

   if (!buf)
      return 1;

   rv = cdrom_send_command(fd, DIRECTION_IN, buf, len, cdb, sizeof(cdb), 0);

   if (rv)
     return 1;

#ifdef CDROM_DEBUG
   data_len = buf[0] << 8 | buf[1];
   first_session = buf[2];
   last_session = buf[3];

   printf("Data Length: %d\n", data_len);
   printf("First Session: %d\n", first_session);
   printf("Last Session: %d\n", last_session);

   for (i = 0; i < (data_len - 2) / 11; i++)
   {
      unsigned char session_num = buf[4 + (i * 11) + 0];
      unsigned char adr = (buf[4 + (i * 11) + 1] >> 4) & 0xF;
      /*unsigned char control = buf[4 + (i * 11) + 1] & 0xF;*/
      unsigned char tno = buf[4 + (i * 11) + 2];
      unsigned char point = buf[4 + (i * 11) + 3];
      unsigned char pmin = buf[4 + (i * 11) + 8];
      unsigned char psec = buf[4 + (i * 11) + 9];
      unsigned char pframe = buf[4 + (i * 11) + 10];

      /*printf("i %d control %d adr %d tno %d point %d: ", i, control, adr, tno, point);*/
      /* why is control always 0? */

      if (/*(control == 4 || control == 6) && */adr == 1 && tno == 0 && point >= 1 && point <= 99)
      {
         printf("- Session#: %d TNO %d POINT %d ", session_num, tno, point);
         printf("Track start time: (MSF %02u:%02u:%02u) ", pmin, psec, pframe);
      }
      else if (/*(control == 4 || control == 6) && */adr == 1 && tno == 0 && point == 0xA0)
      {
         printf("- Session#: %d TNO %d POINT %d ", session_num, tno, point);
         printf("First Track Number: %d ", pmin);
         printf("Disc Type: %d ", psec);
      }
      else if (/*(control == 4 || control == 6) && */adr == 1 && tno == 0 && point == 0xA1)
      {
         printf("- Session#: %d TNO %d POINT %d ", session_num, tno, point);
         printf("Last Track Number: %d ", pmin);
      }
      else if (/*(control == 4 || control == 6) && */adr == 1 && tno == 0 && point == 0xA2)
      {
         printf("- Session#: %d TNO %d POINT %d ", session_num, tno, point);
         printf("Lead-out runtime: (MSF %02u:%02u:%02u) ", pmin, psec, pframe);
      }

      printf("\n");
   }
#endif
   return 0;
}

static int cdrom_read_track_info(int fd, unsigned char track, cdrom_toc_t *toc)
{
   /* MMC Command: READ TRACK INFORMATION */
   unsigned char cdb[] = {0x52, 0x1, 0, 0, 0, track, 0, 0x1, 0x80, 0};
   unsigned char buf[384] = {0};
   unsigned lba = 0;
   unsigned track_size = 0;
   int rv = cdrom_send_command(fd, DIRECTION_IN, buf, sizeof(buf), cdb, sizeof(cdb), 0);

   if (rv)
     return 1;

   memcpy(&lba, buf + 8, 4);
   memcpy(&track_size, buf + 24, 4);

   lba = swap_if_little32(lba);
   track_size = swap_if_little32(track_size);

   /* lba_start may be earlier than the MSF start times seen in read_subq */
   toc->track[track - 1].lba_start = lba;
   toc->track[track - 1].track_size = track_size;

#ifdef CDROM_DEBUG
   printf("Track %d Info: ", track);
   printf("Copy: %d ", (buf[5] & 0x10) > 0);
   printf("Data Mode: %d ", buf[6] & 0xF);
   printf("LBA Start: %d ", lba);
   printf("Track Size: %d\n", track_size);
#endif

   return 0;
}

int cdrom_write_cue(int fd, char **out_buf, size_t *out_len, char cdrom_drive, unsigned char *num_tracks, cdrom_toc_t *toc)
{
   unsigned char buf[2352] = {0};
   unsigned short data_len = 0;
   size_t len = 0;
   size_t pos = 0;
   int rv = 0;
   int i;

   if (!out_buf || !out_len || !num_tracks || !toc)
   {
#ifdef CDROM_DEBUG
      printf("Invalid buffer/length pointer for CDROM cue sheet\n");
#endif
      return 1;
   }

   rv = cdrom_read_subq(fd, buf, sizeof(buf));

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
         printf("Number of CDROM tracks: %d\n", *num_tracks);
#endif
         break;
      }
   }

   if (!*num_tracks || *num_tracks > 99)
   {
#ifdef CDROM_DEBUG
      printf("Invalid number of CDROM tracks: %d\n", *num_tracks);
#endif
      return 1;
   }

   len = CDROM_CUE_TRACK_BYTES * (*num_tracks);
   toc->num_tracks = *num_tracks;
   *out_buf = (char*)calloc(1, len);
   *out_len = len;

   for (i = 0; i < (data_len - 2) / 11; i++)
   {
      /*unsigned char session_num = buf[4 + (i * 11) + 0];*/
      unsigned char adr = (buf[4 + (i * 11) + 1] >> 4) & 0xF;
      unsigned char control = buf[4 + (i * 11) + 1] & 0xF;
      unsigned char tno = buf[4 + (i * 11) + 2];
      unsigned char point = buf[4 + (i * 11) + 3];
      unsigned char pmin = buf[4 + (i * 11) + 8];
      unsigned char psec = buf[4 + (i * 11) + 9];
      unsigned char pframe = buf[4 + (i * 11) + 10];

      /*printf("i %d control %d adr %d tno %d point %d: ", i, control, adr, tno, point);*/
      /* why is control always 0? */

      if (/*(control == 4 || control == 6) && */adr == 1 && tno == 0 && point >= 1 && point <= 99)
      {
         unsigned char next_pmin = (pframe == 74) ? (psec < 59 ? pmin : pmin + 1) : pmin;
         unsigned char next_psec = (pframe == 74) ? (psec < 59 ? (psec + 1) : 0) : psec;
         unsigned char next_pframe = (pframe < 74) ? (pframe + 1) : 0;
         unsigned char mode = 1;
         bool audio = false;
         const char *track_type = "MODE1/2352";

         mode = adr;
         audio = (!(control & 0x4) && !(control & 0x5));

#ifdef CDROM_DEBUG
         printf("Track %02d CONTROL %01X ADR %01X MODE %d AUDIO? %d\n", point, control, adr, mode, audio);
#endif

         toc->track[point - 1].track_num = point;
         toc->track[point - 1].min = pmin;
         toc->track[point - 1].sec = psec;
         toc->track[point - 1].frame = pframe;
         toc->track[point - 1].mode = mode;
         toc->track[point - 1].audio = audio;

         if (audio)
            track_type = "AUDIO";
         else if (mode == 1)
            track_type = "MODE1/2352";
         else if (mode == 2)
            track_type = "MODE2/2352";

         cdrom_read_track_info(fd, point, toc);

         pos += snprintf(*out_buf + pos, len - pos, "FILE \"cdrom://drive%c-track%02d.bin\" BINARY\n", cdrom_drive, point);
         pos += snprintf(*out_buf + pos, len - pos, "  TRACK %02d %s\n", point, track_type);
         pos += snprintf(*out_buf + pos, len - pos, "    INDEX 01 %02d:%02d:%02d\n", pmin, psec, pframe);
      }
   }

   return 0;
}

/* needs 32 bytes for full vendor, product and version */
int cdrom_get_inquiry(int fd, char *model, int len)
{
   /* MMC Command: INQUIRY */
   unsigned char cdb[] = {0x12, 0, 0, 0, 0xff, 0};
   unsigned char buf[256] = {0};
   int rv = cdrom_send_command(fd, DIRECTION_IN, buf, sizeof(buf), cdb, sizeof(cdb), 0);

   if (rv)
      return 1;

   if (model && len >= 32)
   {
      memset(model, 0, len);

      /* vendor */
      memcpy(model, buf + 8, 8);

      model[8] = ' ';

      /* product */
      memcpy(model + 9, buf + 16, 16);

      model[25] = ' ';

      /* version */
      memcpy(model + 26, buf + 32, 4);
   }

   return 0;
}

int cdrom_read(int fd, unsigned char min, unsigned char sec, unsigned char frame, void *s, size_t len, size_t skip)
{
   /* MMC Command: READ CD MSF */
   unsigned char cdb[] = {0xB9, 0, 0, min, sec, frame, 0, 0, 0, 0xF8, 0, 0};
   int rv;

   if (len + skip <= 2352)
   {
      unsigned char next_min = (frame == 74) ? (sec < 59 ? min : min + 1) : min;
      unsigned char next_sec = (frame == 74) ? (sec < 59 ? (sec + 1) : 0) : sec;
      unsigned char next_frame = (frame < 74) ? (frame + 1) : 0;

      cdb[6] = next_min;
      cdb[7] = next_sec;
      cdb[8] = next_frame;

#ifdef CDROM_DEBUG
      printf("single-frame read: from %d %d %d to %d %d %d skip %ld\n", cdb[3], cdb[4], cdb[5], cdb[6], cdb[7], cdb[8], skip);
#endif
   }
   else
   {
      unsigned frames = msf_to_lba(min, sec, frame) + round((len + skip) / 2352.0);

      lba_to_msf(frames, &cdb[6], &cdb[7], &cdb[8]);

#ifdef CDROM_DEBUG
      printf("multi-frame read: from %d %d %d to %d %d %d skip %ld\n", cdb[3], cdb[4], cdb[5], cdb[6], cdb[7], cdb[8], skip);
#endif
   }

   rv = cdrom_send_command(fd, DIRECTION_IN, s, len, cdb, sizeof(cdb), skip);

#ifdef CDROM_DEBUG
   printf("read status code %d\n", rv);
#endif

   if (rv)
      return 1;

   return 0;
}

