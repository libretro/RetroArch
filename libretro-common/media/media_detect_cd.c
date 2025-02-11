/* Copyright  (C) 2010-2020 The RetroArch team
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

/*#define MEDIA_CUE_PARSE_DEBUG*/

static void media_zero_trailing_spaces(char *s, size_t len)
{
   int i;
   for (i = len - 1; i >= 0; i--)
   {
      if (s[i] == ' ')
         s[i] = '\0';
      else if (s[i] != '\0')
         break;
   }
}

static bool media_skip_spaces(const char **s, size_t len)
{
   if (s && *s && **s)
   {
      size_t i;
      for (i = 0; i < len; i++)
      {
         if ((*s)[i] == ' ' || (*s)[i] == '\t')
            continue;
         *s += i;
         return true;
      }
   }
   return false;
}

/* Fill in "info" with detected CD info. Use this when you have a cue file and want it parsed to find the first data track and any pregap info. */
bool media_detect_cd_info_cue(const char *path, media_detect_cd_info_t *info)
{
   RFILE *file                          = NULL;
   char *line                           = NULL;
   char track_path[PATH_MAX_LENGTH]     = {0};
   char track_abs_path[PATH_MAX_LENGTH] = {0};
   char track_mode[11]                  = {0};
   bool found_file                      = false;
   bool found_track                     = false;
   unsigned first_data_track            = 0;
   uint64_t data_track_pregap_bytes     = 0;

   if (string_is_empty(path) || !info)
      return false;

   if (!(file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, 0)))
   {
#ifdef MEDIA_CUE_PARSE_DEBUG
      printf("[MEDIA] Could not open cue path for reading: %s\n", path);
      fflush(stdout);
#endif
      return false;
   }

   while (!filestream_eof(file) && (line = filestream_getline(file)))
   {
      size_t _len = 0;
      const char *command = NULL;

      if (string_is_empty(line))
      {
         free(line);
         continue;
      }

      _len    = strlen(line);
      command = line;

      media_skip_spaces(&command, _len);

      if (!found_file && !strncasecmp(command, "FILE", 4))
      {
         const char *file = command + 4;
         media_skip_spaces(&file, _len - 4);

         if (!string_is_empty(file))
         {
            const char *file_end = NULL;
            size_t file_len      = 0;
            bool quoted          = false;

            if (file[0] == '"')
            {
               quoted = true;
               file++;
            }

            if (quoted)
               file_end = strchr(file, '\"');
            else
               file_end = strchr(file, ' ');

            if (file_end)
            {
               file_len = file_end - file;
               memcpy(track_path, file, file_len);
               found_file = true;
#ifdef MEDIA_CUE_PARSE_DEBUG
               printf("Found file: %s\n", track_path);
               fflush(stdout);
#endif
            }
         }
      }
      else if (found_file && !found_track && !strncasecmp(command, "TRACK", 5))
      {
         const char *track = command + 5;
         media_skip_spaces(&track, _len - 5);

         if (!string_is_empty(track))
         {
            char *ptr             = NULL;
            unsigned track_number = (unsigned)strtol(track, &ptr, 10);
#ifdef MEDIA_CUE_PARSE_DEBUG
            printf("Found track: %d\n", track_number);
            fflush(stdout);
#endif
            track++;

            if (track[0] && track[0] != ' ' && track[0] != '\t')
               track++;

            if (!string_is_empty(track))
            {
               media_skip_spaces(&track, strlen(track));
#ifdef MEDIA_CUE_PARSE_DEBUG
               printf("Found track type: %s\n", track);
               fflush(stdout);
#endif
               if (!strncasecmp(track, "MODE", 4))
               {
                  first_data_track = track_number;
                  found_track = true;
                  strlcpy(track_mode, track, sizeof(track_mode));
               }
               else
                  found_file = false;
            }
         }
      }
      else if (found_file && found_track && first_data_track && !strncasecmp(command, "INDEX", 5))
      {
         const char *index = command + 5;
         media_skip_spaces(&index, _len - 5);

         if (!string_is_empty(index))
         {
            char *ptr             = NULL;
            unsigned index_number = (unsigned)strtol(index, &ptr, 10);

            if (index_number == 1)
            {
               const char *pregap = index + 1;

               if (pregap[0] && pregap[0] != ' ' && pregap[0] != '\t')
                  pregap++;

               if (!string_is_empty(pregap))
               {
                  media_skip_spaces(&pregap, strlen(pregap));
                  found_file  = false;
                  found_track = false;

                  if (first_data_track && !string_is_empty(track_mode))
                  {
                     unsigned track_sector_size = 0;
                     unsigned track_mode_number = 0;

                     if (strlen(track_mode) == 10)
                     {
                        sscanf(track_mode, "MODE%d/%d", (int*)&track_mode_number, (int*)&track_sector_size);
#ifdef MEDIA_CUE_PARSE_DEBUG
                        printf("Found track mode %d with sector size %d\n", track_mode_number, track_sector_size);
                        fflush(stdout);
#endif
                        if ((track_mode_number == 1 || track_mode_number == 2) && track_sector_size)
                        {
                           unsigned min = 0;
                           unsigned sec = 0;
                           unsigned frame = 0;
                           sscanf(pregap, "%02d:%02d:%02d", (int*)&min, (int*)&sec, (int*)&frame);

                           if (min || sec || frame || strstr(pregap, "00:00:00"))
                           {
                              data_track_pregap_bytes = ((min * 60 + sec) * 75 + frame) * track_sector_size;
#ifdef MEDIA_CUE_PARSE_DEBUG
                              printf("Found pregap of %02d:%02d:%02d (bytes: %" PRIu64 ")\n", min, sec, frame, data_track_pregap_bytes);
                              fflush(stdout);
#endif
                              break;
                           }
                        }
                     }
                  }
               }
            }
         }
      }

      free(line);
   }

   filestream_close(file);

   if (!string_is_empty(track_path))
   {
      size_t _len;
      if (strstr(track_path, "/") || strstr(track_path, "\\"))
      {
#ifdef MEDIA_CUE_PARSE_DEBUG
         printf("using path %s\n", track_path);
         fflush(stdout);
#endif
         return media_detect_cd_info(track_path, data_track_pregap_bytes, info);
      }

      _len = fill_pathname_basedir(track_abs_path, path, sizeof(track_abs_path));
      strlcpy(track_abs_path + _len, track_path, sizeof(track_abs_path) - _len);
#ifdef MEDIA_CUE_PARSE_DEBUG
      printf("using abs path %s\n", track_abs_path);
      fflush(stdout);
#endif
      return media_detect_cd_info(track_abs_path, data_track_pregap_bytes, info);
   }

   return true;
}

/* Fill in "info" with detected CD info. Use this when you want to open a specific track file directly, and the pregap is known. */
bool media_detect_cd_info(const char *path, uint64_t pregap_bytes, media_detect_cd_info_t *info)
{
   RFILE *file;

   if (string_is_empty(path) || !info)
      return false;

   if (!(file = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ, 0)))
   {
#ifdef MEDIA_CUE_PARSE_DEBUG
      printf("[MEDIA] Could not open path for reading: %s\n", path);
      fflush(stdout);
#endif
      return false;
   }

   {
      unsigned offset      = 0;
      unsigned sector_size = 0;
      unsigned buf_size    = 17 * 2352;
      char *buf            = (char*)calloc(1, buf_size);
      int64_t read_bytes   = 0;

      if (!buf)
         return false;

      if (pregap_bytes)
         filestream_seek(file, pregap_bytes, RETRO_VFS_SEEK_POSITION_START);

      read_bytes = filestream_read(file, buf, buf_size);

      if (read_bytes != buf_size)
      {
#ifdef MEDIA_CUE_PARSE_DEBUG
         printf("[MEDIA] Could not read from media: got %" PRId64 " bytes instead of %d.\n", read_bytes, buf_size);
         fflush(stdout);
#endif
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

         /* Assume Mode 2 formed (formless is rarely used) */
         if (buf[15] == 2)
            offset   = 24;
         else /* Assume Mode 1 */
            offset   = 16;
      }
      else /* Assume sectors only contain user data instead. */
         sector_size = 2048;

      if (!memcmp(buf + offset, "SEGADISCSYSTEM",
               STRLEN_CONST("SEGADISCSYSTEM")))
      {
         const char *title_pos  = NULL;
         const char *serial_pos = NULL;

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
         {
            info->serial[0] = 'N';
            info->serial[1] = '/';
            info->serial[2] = 'A';
            info->serial[3] = '\0';
         }
      }
      else if (!memcmp(buf + offset, "SEGA SEGASATURN",
               STRLEN_CONST("SEGA SEGASATURN")))
      {
         const char *title_pos        = NULL;
         const char *serial_pos       = NULL;
         const char *version_pos      = NULL;
         const char *release_date_pos = NULL;

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
         {
            info->serial[0] = 'N';
            info->serial[1] = '/';
            info->serial[2] = 'A';
            info->serial[3] = '\0';
         }

         version_pos = buf + offset + 0x2a;

         if (media_skip_spaces(&version_pos, 6))
         {
            memcpy(info->version, version_pos, 6 - (version_pos - (buf + offset + 0x2a)));
            media_zero_trailing_spaces(info->version, sizeof(info->version));
         }
         else
         {
            info->version[0] = 'N';
            info->version[1] = '/';
            info->version[2] = 'A';
            info->version[3] = '\0';
         }

         release_date_pos = buf + offset + 0x30;

         if (media_skip_spaces(&release_date_pos, 8))
         {
            memcpy(info->release_date, release_date_pos, 8 - (release_date_pos - (buf + offset + 0x30)));
            media_zero_trailing_spaces(info->release_date, sizeof(info->release_date));
         }
         else
         {
            info->release_date[0] = 'N';
            info->release_date[1] = '/';
            info->release_date[2] = 'A';
            info->release_date[3] = '\0';
         }
      }
      else if (!memcmp(buf + offset, "SEGA SEGAKATANA", STRLEN_CONST("SEGA SEGAKATANA")))
      {
         const char *title_pos        = NULL;
         const char *serial_pos       = NULL;
         const char *version_pos      = NULL;
         const char *release_date_pos = NULL;

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
         {
            info->serial      [0] = 'N';
            info->serial      [1] = '/';
            info->serial      [2] = 'A';
            info->serial      [3] = '\0';
         }

         version_pos = buf + offset + 0x4a;

         if (media_skip_spaces(&version_pos, 6))
         {
            memcpy(info->version, version_pos, 6 - (version_pos - (buf + offset + 0x4a)));
            media_zero_trailing_spaces(info->version, sizeof(info->version));
         }
         else
         {
            info->version     [0] = 'N';
            info->version     [1] = '/';
            info->version     [2] = 'A';
            info->version     [3] = '\0';
         }

         release_date_pos = buf + offset + 0x50;

         if (media_skip_spaces(&release_date_pos, 8))
         {
            memcpy(info->release_date, release_date_pos, 8 - (release_date_pos - (buf + offset + 0x50)));
            media_zero_trailing_spaces(info->release_date, sizeof(info->release_date));
         }
         else
         {
            info->release_date[0] = 'N';
            info->release_date[1] = '/';
            info->release_date[2] = 'A';
            info->release_date[3] = '\0';
         }
      }
      /* Primary Volume Descriptor fields of ISO9660 */
      else if (!memcmp(buf + offset + (16 * sector_size), "\1CD001\1\0PLAYSTATION", 19))
      {
         const char *title_pos = NULL;

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
