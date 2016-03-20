/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2014-2016 - Jean-André Santoni
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <ctype.h>

#include <compat/strcasestr.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_endianness.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "tasks_internal.h"

#ifdef HAVE_LIBRETRODB
#include "../database_info.h"
#endif

#include "../dir_list_special.h"
#include "../msg_hash.h"
#include "../general.h"
#include "../verbosity.h"

#define MAGIC_LEN       16
#define MAX_TOKEN_LEN   255

#ifdef MSB_FIRST
#define MODETEST_VAL    0x00ffffff
#else
#define MODETEST_VAL    0xffffff00
#endif

struct magic_entry
{
    const char *system_name;
    const char *magic;
};

static struct magic_entry MAGIC_NUMBERS[] = {
    {"ps1", "\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x02\x00\x02\x00"},
    {"pcecd", "\x82\xb1\x82\xcc\x83\x76\x83\x8d\x83\x4f\x83\x89\x83\x80\x82\xcc\x92"},
    {"scd", "\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x02\x00\x01\x53"},
    {NULL, NULL}
};

static ssize_t get_token(RFILE *fd, char *token, size_t max_len)
{
   char *c       = token;
   ssize_t len   = 0;
   int in_string = 0;

   while (1)
   {
      int rv = retro_fread(fd, c, 1);
      if (rv == 0)
         return 0;

      if (rv < 1)
      {
         switch (errno)
         {
            case EINTR:
            case EAGAIN:
               continue;
            default:
               return -errno;
         }
      }

      switch (*c)
      {
         case ' ':
         case '\t':
         case '\r':
         case '\n':
            if (c == token)
               continue;

            if (!in_string)
            {
               *c = '\0';
               return len;
            }
            break;
         case '\"':
            if (c == token)
            {
               in_string = 1;
               continue;
            }

            *c = '\0';
            return len;
      }

      len++;
      c++;
      if (len == (ssize_t)max_len)
      {
         *c = '\0';
         return len;
      }
   }
}

static int find_token(RFILE *fd, const char *token)
{
   int     tmp_len = strlen(token);
   char *tmp_token = (char*)calloc(tmp_len+1, 1);

   if (!tmp_token)
      return -1;

   while (strncmp(tmp_token, token, tmp_len) != 0)
   {
      if (get_token(fd, tmp_token, tmp_len) <= 0)
         return -1;
   }

   free(tmp_token);

   return 0;
}


static int detect_ps1_game_sub(const char *track_path,
      char *game_id, int sub_channel_mixed)
{
   uint8_t* tmp;
   uint8_t buffer[2048 * 2];
   int skip, frame_size, is_mode1, cd_sector;
   RFILE *fp = retro_fopen(track_path, RFILE_MODE_READ, -1);
   if (!fp)
      return 0;

   is_mode1 = 0;
   retro_fseek(fp, 0, SEEK_END);

   if (!sub_channel_mixed)
   {
      if (!(retro_ftell(fp) & 0x7FF))
      {
         unsigned int mode_test = 0;

         retro_fseek(fp, 0, SEEK_SET);
         retro_fread(fp, &mode_test, 4);
         if (mode_test != MODETEST_VAL)
            is_mode1 = 1;
      }
   }

   skip       = is_mode1? 0: 24;
   frame_size = sub_channel_mixed? 2448: is_mode1? 2048: 2352;

   retro_fseek(fp, 156 + skip + 16 * frame_size, SEEK_SET);
   retro_fread(fp, buffer, 6);

   cd_sector = buffer[2] | (buffer[3] << 8) | (buffer[4] << 16);
   retro_fseek(fp, skip + cd_sector * frame_size, SEEK_SET);
   retro_fread(fp, buffer, 2048 * 2);

   tmp = buffer;
   while (tmp < (buffer + 2048 * 2))
   {
      if (!*tmp)
         return 0;

      if (!strncasecmp((const char*)(tmp + 33), "SYSTEM.CNF;1", 12))
         break;

      tmp += *tmp;
   }
   if(tmp >= (buffer + 2048 * 2))
      return 0;

   cd_sector = tmp[2] | (tmp[3] << 8) | (tmp[4] << 16);
   retro_fseek(fp, 13 + skip + cd_sector * frame_size, SEEK_SET);
   retro_fread(fp, buffer, 256);

   tmp = (uint8_t*)strrchr((const char*)buffer, '\\');
   if(!tmp)
      tmp = buffer;
   else
      tmp++;

   *game_id++ = toupper(*tmp++);
   *game_id++ = toupper(*tmp++);
   *game_id++ = toupper(*tmp++);
   *game_id++ = toupper(*tmp++);
   *game_id++ = '-';
   tmp++;
   *game_id++ = *tmp++;
   *game_id++ = *tmp++;
   *game_id++ = *tmp++;
   tmp++;
   *game_id++ = *tmp++;
   *game_id++ = *tmp++;
   *game_id = 0;

   retro_fclose(fp);
   return 1;
}

int detect_ps1_game(const char *track_path, char *game_id)
{
   if (detect_ps1_game_sub(track_path, game_id, 0))
      return 1;

   return detect_ps1_game_sub(track_path, game_id, 1);
}

int detect_psp_game(const char *track_path, char *game_id)
{
   bool rv = false;
   unsigned pos;
   RFILE *fd = retro_fopen(track_path, RFILE_MODE_READ, -1);

   if (!fd)
   {
      RARCH_LOG("Could not open data track: %s\n", strerror(errno));
      return -errno;
   }

   for (pos = 0; pos < 100000; pos++)
   {
      retro_fseek(fd, pos, SEEK_SET);

      if (retro_fread(fd, game_id, 5) > 0)
      {
         game_id[5] = '\0';
         if (string_is_equal(game_id, "ULES-")
          || string_is_equal(game_id, "ULUS-")
          || string_is_equal(game_id, "ULJS-")

          || string_is_equal(game_id, "ULEM-")
          || string_is_equal(game_id, "ULUM-")
          || string_is_equal(game_id, "ULJM-")

          || string_is_equal(game_id, "UCES-")
          || string_is_equal(game_id, "UCUS-")
          || string_is_equal(game_id, "UCJS-")
          || string_is_equal(game_id, "UCAS-")

          || string_is_equal(game_id, "NPEH-")
          || string_is_equal(game_id, "NPUH-")
          || string_is_equal(game_id, "NPJH-")

          || string_is_equal(game_id, "NPEG-")
          || string_is_equal(game_id, "NPUG-")
          || string_is_equal(game_id, "NPJG-")
          || string_is_equal(game_id, "NPHG-")

          || string_is_equal(game_id, "NPEZ-")
          || string_is_equal(game_id, "NPUZ-")
          || string_is_equal(game_id, "NPJZ-")
         )
         {
            retro_fseek(fd, pos, SEEK_SET);
            if (retro_fread(fd, game_id, 10) > 0)
            {
#if 0
               game_id[4] = '-';
               game_id[8] = game_id[9];
               game_id[9] = game_id[10];
#endif
               game_id[10] = '\0';
               rv = true;
            }
            break;
         }
      }
      else
         break;
   }

   retro_fclose(fd);
   return rv;
}

int detect_system(const char *track_path, int32_t offset,
        const char **system_name)
{
   int rv;
   char magic[MAGIC_LEN];
   int i;
   RFILE *fd = retro_fopen(track_path, RFILE_MODE_READ, -1);
   
   if (!fd)
   {
      RARCH_LOG("Could not open data track of file '%s': %s\n",
            track_path, strerror(errno));
      rv = -errno;
      goto clean;
   }

   retro_fseek(fd, offset, SEEK_SET);
   if (retro_fread(fd, magic, MAGIC_LEN) < MAGIC_LEN)
   {
      RARCH_LOG("Could not read data from file '%s' at offset %d: %s\n",
            track_path, offset, strerror(errno));
      rv = -errno;
      goto clean;
   }

   RARCH_LOG("Comparing with known magic numbers...\n");
   for (i = 0; MAGIC_NUMBERS[i].system_name != NULL; i++)
   {
      if (memcmp(MAGIC_NUMBERS[i].magic, magic, MAGIC_LEN) == 0)
      {
         *system_name = MAGIC_NUMBERS[i].system_name;
         rv = 0;
         goto clean;
      }
   }

   retro_fseek(fd, 0x8008, SEEK_SET);
   if (retro_fread(fd, magic, 8) > 0)
   {
      magic[8] = '\0';
      if (string_is_equal(magic, "PSP GAME"))
      {
         *system_name = "psp\0";
         rv = 0;
         goto clean;
      }
   }

   RARCH_LOG("Could not find compatible system\n");
   rv = -EINVAL;
clean:
   retro_fclose(fd);
   return rv;
}

int find_first_data_track(const char *cue_path,
      int32_t *offset, char *track_path, size_t max_len)
{
   int rv, m, s, f;
   char tmp_token[MAX_TOKEN_LEN];
   char cue_dir[PATH_MAX_LENGTH];
   RFILE *fd;

   strlcpy(cue_dir, cue_path, sizeof(cue_dir));
   path_basedir(cue_dir);

   fd = retro_fopen(cue_path, RFILE_MODE_READ, -1);
   if (!fd)
   {
      RARCH_LOG("Could not open CUE file '%s': %s\n", cue_path,
            strerror(errno));
      return -errno;
   }

   RARCH_LOG("Parsing CUE file '%s'...\n", cue_path);

   while (get_token(fd, tmp_token, MAX_TOKEN_LEN) > 0)
   {
      if (string_is_equal(tmp_token, "FILE"))
      {
         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         fill_pathname_join(track_path, cue_dir, tmp_token, max_len);

      }
      else if (string_is_equal_noncase(tmp_token, "TRACK"))
      {
         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         if (string_is_equal_noncase(tmp_token, "AUDIO"))
            continue;

         find_token(fd, "INDEX");
         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         get_token(fd, tmp_token, MAX_TOKEN_LEN);

         if (sscanf(tmp_token, "%02d:%02d:%02d", &m, &s, &f) < 3)
         {
            RARCH_LOG("Error parsing time stamp '%s'\n", tmp_token);
            return -errno;
         }

         *offset = ((m * 60) * (s * 75) * f) * 25;

         RARCH_LOG("Found 1st data track on file '%s+%d'\n",
               track_path, *offset);

         rv = 0;
         goto clean;
      }
   }

   rv = -EINVAL;

clean:
   retro_fclose(fd);
   return rv;
}
