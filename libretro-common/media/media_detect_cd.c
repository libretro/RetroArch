/* Copyright  (C) 2010-2019 The RetroArch team
*
* ---------------------------------------------------------------------------------------
* The following license statement only applies to this file (media_detect_cd.c).
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

#include <media/media_detect_cd.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>

static void media_zero_trailing_spaces(char *buf, size_t len)
{
   int i;

   for (i = len - 1; i >= 0; i--)
   {
      if (buf[i] == ' ')
         buf[i] = '\0';
      else if (buf[i] != '\0')
         break;
   }
}

static bool media_skip_spaces(const char **buf, size_t len)
{
   bool found = false;
   unsigned i;

   if (!buf || !*buf || !**buf)
      return false;

   for (i = 0; i < len; i++)
   {
      if ((*buf)[i] == ' ' || (*buf)[i] == '\t')
         continue;

      *buf += i;
      found = true;
      break;
   }

   if (found)
      return true;

   return false;
}

/* Fill in "info" with detected CD info. Use this when you want to open a specific track file directly, and the pregap is known. */
bool media_detect_cd_info(const char *path, uint64_t pregap_bytes, media_detect_cd_info_t *info)
{
   RFILE *file;

   if (string_is_empty(path) || !info)
      return false;

   file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, 0);

   if (!file)
   {
      printf("[MEDIA] Could not open path for reading: %s\n", path);
      fflush(stdout);
      return false;
   }

   {
      unsigned offset = 0;
      unsigned sector_size = 0;
      unsigned buf_size = 17 * 2352;
      char *buf = (char*)calloc(1, buf_size);
      int64_t read_bytes = 0;

      if (!buf)
         return false;

      if (pregap_bytes)
         filestream_seek(file, pregap_bytes, RETRO_VFS_SEEK_POSITION_START);

      read_bytes = filestream_read(file, buf, buf_size);

      if (read_bytes != buf_size)
      {
         printf("[MEDIA] Could not read from media: got %" PRId64 " bytes instead of %d.\n", read_bytes, buf_size);
         fflush(stdout);
         filestream_close(file);
         free(buf);
         return false;
      }

      /* 12-byte sync field at the start of every sector, common to both mode1 and mode2 data tracks
       * (when at least sync data is requested). This is a CD-ROM standard feature and not specific to any game devices,
       * and as such should not be part of any system-specific detection or "magic" bytes.
       * Depending on what parts of a sector were requested from the disc, the user data might start at
       * byte offset 0, 4, 8, 12, 16 or 24. Cue sheets only specify the total number of bytes requested from the sectors
       * of a track (like 2048 or 2352) and it is then assumed based on the size/mode as to what fields are present. */
      if (!memcmp(buf, "\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\x00", 12))
      {
         /* Assume track data contains all fields. */
         sector_size = 2352;

         if (buf[15] == 2)
         {
            /* assume Mode 2 formed (formless is rarely used) */
            offset = 24;
         }
         else
         {
            /* assume Mode 1 */
            offset = 16;
         }
      }
      else
      {
         /* Assume sectors only contain user data instead. */
         offset = 0;
         sector_size = 2048;
      }

      if (!memcmp(buf + offset, "SEGADISCSYSTEM", strlen("SEGADISCSYSTEM")))
      {
         const char *title_pos;
         const char *serial_pos;
         bool title_found = false;

         /* All discs currently in Redump for MCD start with SEGADISCSYSTEM. There are other strings mentioned elsewhere online,
          * but I have not seen any real examples of them. */
         info->system_id = MEDIA_CD_SYSTEM_MEGA_CD;

         strlcpy(info->system, "Sega CD / Mega CD", sizeof(info->system));

         title_pos = buf + offset + 0x150;

         if (media_skip_spaces(&title_pos, 48))
         {
            memcpy(info->title, title_pos, 48 - (title_pos - (buf + offset + 0x150)));
            media_zero_trailing_spaces(info->title, sizeof(info->title));
         }
         else
            strlcpy(info->title, "N/A", sizeof(info->title));

         serial_pos = buf + offset + 0x183;

         if (media_skip_spaces(&serial_pos, 8))
         {
            memcpy(info->serial, serial_pos, 8 - (serial_pos - (buf + offset + 0x183)));
            media_zero_trailing_spaces(info->serial, sizeof(info->serial));
         }
         else
            strlcpy(info->serial, "N/A", sizeof(info->serial));
      }
      else if (!memcmp(buf + offset, "SEGA SEGASATURN", strlen("SEGA SEGASATURN")))
      {
         const char *title_pos;
         const char *serial_pos;
         const char *version_pos;
         const char *release_date_pos;
         bool title_found = false;

         info->system_id = MEDIA_CD_SYSTEM_SATURN;

         strlcpy(info->system, "Sega Saturn", sizeof(info->system));

         title_pos = buf + offset + 0x60;

         if (media_skip_spaces(&title_pos, 112))
         {
            memcpy(info->title, title_pos, 112 - (title_pos - (buf + offset + 0x60)));
            media_zero_trailing_spaces(info->title, sizeof(info->title));
         }
         else
            strlcpy(info->title, "N/A", sizeof(info->title));

         serial_pos = buf + offset + 0x20;

         if (media_skip_spaces(&serial_pos, 10))
         {
            memcpy(info->serial, serial_pos, 10 - (serial_pos - (buf + offset + 0x20)));
            media_zero_trailing_spaces(info->serial, sizeof(info->serial));
         }
         else
            strlcpy(info->serial, "N/A", sizeof(info->serial));

         version_pos = buf + offset + 0x2a;

         if (media_skip_spaces(&version_pos, 6))
         {
            memcpy(info->version, version_pos, 6 - (version_pos - (buf + offset + 0x2a)));
            media_zero_trailing_spaces(info->version, sizeof(info->version));
         }
         else
            strlcpy(info->version, "N/A", sizeof(info->version));

         release_date_pos = buf + offset + 0x30;

         if (media_skip_spaces(&release_date_pos, 8))
         {
            memcpy(info->release_date, release_date_pos, 8 - (release_date_pos - (buf + offset + 0x30)));
            media_zero_trailing_spaces(info->release_date, sizeof(info->release_date));
         }
         else
            strlcpy(info->release_date, "N/A", sizeof(info->release_date));
      }
      else if (!memcmp(buf + offset, "SEGA SEGAKATANA", strlen("SEGA SEGAKATANA")))
      {
         const char *title_pos;
         const char *serial_pos;
         const char *version_pos;
         const char *release_date_pos;
         bool title_found = false;

         info->system_id = MEDIA_CD_SYSTEM_DREAMCAST;

         strlcpy(info->system, "Sega Dreamcast", sizeof(info->system));

         title_pos = buf + offset + 0x80;

         if (media_skip_spaces(&title_pos, 96))
         {
            memcpy(info->title, title_pos, 96 - (title_pos - (buf + offset + 0x80)));
            media_zero_trailing_spaces(info->title, sizeof(info->title));
         }
         else
            strlcpy(info->title, "N/A", sizeof(info->title));

         serial_pos = buf + offset + 0x40;

         if (media_skip_spaces(&serial_pos, 10))
         {
            memcpy(info->serial, serial_pos, 10 - (serial_pos - (buf + offset + 0x40)));
            media_zero_trailing_spaces(info->serial, sizeof(info->serial));
         }
         else
            strlcpy(info->serial, "N/A", sizeof(info->serial));

         version_pos = buf + offset + 0x4a;

         if (media_skip_spaces(&version_pos, 6))
         {
            memcpy(info->version, version_pos, 6 - (version_pos - (buf + offset + 0x4a)));
            media_zero_trailing_spaces(info->version, sizeof(info->version));
         }
         else
            strlcpy(info->version, "N/A", sizeof(info->version));

         release_date_pos = buf + offset + 0x50;

         if (media_skip_spaces(&release_date_pos, 8))
         {
            memcpy(info->release_date, release_date_pos, 8 - (release_date_pos - (buf + offset + 0x50)));
            media_zero_trailing_spaces(info->release_date, sizeof(info->release_date));
         }
         else
            strlcpy(info->release_date, "N/A", sizeof(info->release_date));
      }
      /* Primary Volume Descriptor fields of ISO9660 */
      else if (!memcmp(buf + offset + (16 * sector_size), "\1CD001\1\0PLAYSTATION", 19))
      {
         const char *title_pos = NULL;
         bool title_found      = false;

         info->system_id = MEDIA_CD_SYSTEM_PSX;

         strlcpy(info->system, "Sony PlayStation", sizeof(info->system));

         title_pos = buf + offset + (16 * sector_size) + 40;

         if (media_skip_spaces(&title_pos, 32))
         {
            memcpy(info->title, title_pos, 32 - (title_pos - (buf + offset + (16 * sector_size) + 40)));
            media_zero_trailing_spaces(info->title, sizeof(info->title));
         }
         else
            strlcpy(info->title, "N/A", sizeof(info->title));
      }
      else if (!memcmp(buf + offset, "\x01\x5a\x5a\x5a\x5a\x5a\x01\x00\x00\x00\x00\x00", 12))
      {
         info->system_id = MEDIA_CD_SYSTEM_3DO;

         strlcpy(info->system, "3DO", sizeof(info->system));
      }
      else if (!memcmp(buf + offset + 0x950, "PC Engine CD-ROM SYSTEM", 23))
      {
         info->system_id = MEDIA_CD_SYSTEM_PC_ENGINE_CD;

         strlcpy(info->system, "TurboGrafx-CD / PC-Engine CD", sizeof(info->system));
      }

      free(buf);
   }

   filestream_close(file);

   return true;
}
