/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2017 - Brad Parker
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
#include <string.h>

#include <retro_miscellaneous.h>
#include <compat/strcasestr.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <retro_endianness.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../database_info.h"

#include "tasks_internal.h"

#include "../driver.h"
#include "../list_special.h"
#include "../msg_hash.h"
#include "../verbosity.h"

#define MAGIC_LEN       17
#define MAX_TOKEN_LEN   255

#ifdef MSB_FIRST
#define MODETEST_VAL    0x00ffffff
#else
#define MODETEST_VAL    0xffffff00
#endif

struct magic_entry
{
   int32_t offset;
   const char *system_name;
   const char *magic;
};

static struct magic_entry MAGIC_NUMBERS[] = {
   { 0,        "ps1",    "\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x02\x00\x02\x00"},
   { 0x838840, "pcecd",  "\x82\xb1\x82\xcc\x83\x76\x83\x8d\x83\x4f\x83\x89\x83\x80\x82\xcc\x92"},
   { 0,        "scd",    "\x00\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\x00\x00\x02\x00\x01\x53"},
   { 0,        NULL,     NULL}
};

static ssize_t get_token(RFILE *fd, char *token, size_t max_len)
{
   char *c       = token;
   ssize_t len   = 0;
   int in_string = 0;

   while (1)
   {
      int rv = (int)filestream_read(fd, c, 1);
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
   int     tmp_len = (int)strlen(token);
   char *tmp_token = (char*)calloc(tmp_len+1, 1);

   if (!tmp_token)
      return -1;

   while (strncmp(tmp_token, token, tmp_len) != 0)
   {
      if (get_token(fd, tmp_token, tmp_len) <= 0)
      {
         free(tmp_token);
         return -1;
      }
   }

   free(tmp_token);

   return 0;
}


static int detect_ps1_game_sub(const char *track_path,
      char *game_id, int sub_channel_mixed)
{
   uint8_t* tmp;
   uint8_t* boot_file;
   int skip, frame_size, is_mode1, cd_sector;
   uint8_t buffer[2048 * 2];
   RFILE                *fp =
      filestream_open(track_path, RFILE_MODE_READ, -1);
   if (!fp)
      return 0;

   buffer[0] = '\0';
   is_mode1  = 0;
   filestream_seek(fp, 0, SEEK_END);

   if (!sub_channel_mixed)
   {
      if (!(filestream_tell(fp) & 0x7FF))
      {
         unsigned int mode_test = 0;

         filestream_seek(fp, 0, SEEK_SET);
         filestream_read(fp, &mode_test, 4);
         if (mode_test != MODETEST_VAL)
            is_mode1 = 1;
      }
   }

   skip       = is_mode1? 0: 24;
   frame_size = sub_channel_mixed? 2448: is_mode1? 2048: 2352;

   filestream_seek(fp, 156 + skip + 16 * frame_size, SEEK_SET);
   filestream_read(fp, buffer, 6);

   cd_sector = buffer[2] | (buffer[3] << 8) | (buffer[4] << 16);
   filestream_seek(fp, skip + cd_sector * frame_size, SEEK_SET);
   filestream_read(fp, buffer, 2048 * 2);

   tmp = buffer;
   while (tmp < (buffer + 2048 * 2))
   {
      if (!*tmp)
         goto error;

      if (!strncasecmp((const char*)(tmp + 33), "SYSTEM.CNF;1", 12))
         break;

      tmp += *tmp;
   }

   if(tmp >= (buffer + 2048 * 2))
      goto error;

   cd_sector = tmp[2] | (tmp[3] << 8) | (tmp[4] << 16);
   filestream_seek(fp, skip + cd_sector * frame_size, SEEK_SET);
   filestream_read(fp, buffer, 256);
   buffer[256] = '\0';

   tmp = buffer;
   while(*tmp && strncasecmp((const char*)tmp, "boot", 4))
      tmp++;

   if(!*tmp)
      goto error;

   boot_file = tmp;
   while(*tmp && *tmp != '\n')
   {
      if((*tmp == '\\') || (*tmp == ':'))
         boot_file = tmp + 1;

      tmp++;
   }

   tmp = boot_file;
   *game_id++ = toupper(*tmp++);
   *game_id++ = toupper(*tmp++);
   *game_id++ = toupper(*tmp++);
   *game_id++ = toupper(*tmp++);
   *game_id++ = '-';

   if(!isalnum(*tmp))
      tmp++;

   while(isalnum(*tmp))
   {
      *game_id++ = *tmp++;
      if(*tmp == '.')
         tmp++;
   }

   *game_id = 0;

   filestream_close(fp);
   return 1;

error:
   filestream_close(fp);
   return 0;
}

int detect_ps1_game(const char *track_path, char *game_id)
{
   if (detect_ps1_game_sub(track_path, game_id, 0))
      return 1;

   return detect_ps1_game_sub(track_path, game_id, 1);
}

int detect_psp_game(const char *track_path, char *game_id)
{
   unsigned pos;
   bool rv   = false;
   RFILE *fd = filestream_open(track_path, RFILE_MODE_READ, -1);

   if (!fd)
   {
      RARCH_LOG("%s: %s\n",
            msg_hash_to_str(MSG_COULD_NOT_OPEN_DATA_TRACK),
            strerror(errno));
      return -errno;
   }

   for (pos = 0; pos < 100000; pos++)
   {
      filestream_seek(fd, pos, SEEK_SET);

      if (filestream_read(fd, game_id, 5) > 0)
      {
         game_id[5] = '\0';

         if (
               (string_is_equal(game_id, "ULES-"))
               || (string_is_equal(game_id, "ULUS-"))
               || (string_is_equal(game_id, "ULJS-"))

               || (string_is_equal(game_id, "ULEM-"))
               || (string_is_equal(game_id, "ULUM-"))
               || (string_is_equal(game_id, "ULJM-"))

               || (string_is_equal(game_id, "UCES-"))
               || (string_is_equal(game_id, "UCUS-"))
               || (string_is_equal(game_id, "UCJS-"))
               || (string_is_equal(game_id, "UCAS-"))

               || (string_is_equal(game_id, "NPEH-"))
               || (string_is_equal(game_id, "NPUH-"))
               || (string_is_equal(game_id, "NPJH-"))

               || (string_is_equal(game_id, "NPEG-"))
               || (string_is_equal(game_id, "NPUG-"))
               || (string_is_equal(game_id, "NPJG-"))
               || (string_is_equal(game_id, "NPHG-"))

               || (string_is_equal(game_id, "NPEZ-"))
               || (string_is_equal(game_id, "NPUZ-"))
               || (string_is_equal(game_id, "NPJZ-"))
               )
               {
                  filestream_seek(fd, pos, SEEK_SET);
                  if (filestream_read(fd, game_id, 10) > 0)
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

   filestream_close(fd);
   return rv;
}

/**
 * Check for an ASCII serial in the first few bits of the ISO (Wii).
 */
int detect_serial_ascii_game(const char *track_path, char *game_id)
{
   unsigned pos;
   int numberOfAscii = 0;
   bool rv   = false;
   RFILE *fd = filestream_open(track_path, RFILE_MODE_READ, -1);

   /* Attempt to load the file. */
   if (!fd)
   {
      RARCH_LOG("%s: %s\n",
            msg_hash_to_str(MSG_COULD_NOT_OPEN_DATA_TRACK),
            strerror(errno));
      return -errno;
   }

   for (pos = 0; pos < 10000; pos++)
   {
      filestream_seek(fd, pos, SEEK_SET);
      /* Current logic only requires 15 characters (max of 4096 per sizeof game_id). */
      if (filestream_read(fd, game_id, 15) > 0)
      {
         unsigned i;
         game_id[15] = '\0';
         numberOfAscii = 0;

         /* Loop through until we run out of ASCII characters. */
         for (i = 0; i < 15; i++)
         {
            /* Is the given character ASCII? A-Z, 0-9, - */
            if (game_id[i] == 45 || (game_id[i] >= 48 && game_id[i] <= 57) || (game_id[i] >= 65 && game_id[i] <= 90))
               numberOfAscii++;
            else
               break;
         }

         /* If the length of the text is between 3 and 9 characters, it could be a serial. */
         if (numberOfAscii > 3 && numberOfAscii < 9)
         {
            /* Cut the string off, and return it as a valid serial. */
            game_id[numberOfAscii] = '\0';
            rv = true;
            break;
         }
      }
   }

   filestream_close(fd);
   return rv;
}

int detect_system(const char *track_path, const char **system_name)
{
   int rv;
   char magic[MAGIC_LEN];
   int i;
   RFILE *fd = filestream_open(track_path, RFILE_MODE_READ, -1);

   if (!fd)
   {
      RARCH_LOG("Could not open data track of file '%s': %s\n",
            track_path, strerror(errno));
      rv = -errno;
      goto clean;
   }

   RARCH_LOG("%s\n", msg_hash_to_str(MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS));
   for (i = 0; MAGIC_NUMBERS[i].system_name != NULL; i++)
   {
      filestream_seek(fd, MAGIC_NUMBERS[i].offset, SEEK_SET);

      if (filestream_read(fd, magic, MAGIC_LEN) < MAGIC_LEN)
      {
         RARCH_LOG("Could not read data from file '%s' at offset %d: %s\n",
               track_path, MAGIC_NUMBERS[i].offset, strerror(errno));
         rv = -errno;
         goto clean;
      }

      if (string_is_equal_fast(MAGIC_NUMBERS[i].magic, magic, MAGIC_LEN))
      {
         *system_name = MAGIC_NUMBERS[i].system_name;
         rv = 0;
         goto clean;
      }
   }

   filestream_seek(fd, 0x8008, SEEK_SET);
   if (filestream_read(fd, magic, 8) > 0)
   {
      magic[8] = '\0';
      if (!string_is_empty(magic) &&
            string_is_equal(magic, "PSP GAME"))
      {
         *system_name = "psp\0";
         rv = 0;
         goto clean;
      }
   }

   RARCH_LOG("%s\n", msg_hash_to_str(MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM));
   rv = -EINVAL;

clean:
   filestream_close(fd);
   return rv;
}

int find_first_data_track(const char *cue_path,
      int32_t *offset, char *track_path, size_t max_len)
{
   int rv;
   char tmp_token[MAX_TOKEN_LEN];
   RFILE *fd                     =
      filestream_open(cue_path, RFILE_MODE_READ, -1);

   if (!fd)
   {
      RARCH_LOG("Could not open CUE file '%s': %s\n", cue_path,
            strerror(errno));
      return -errno;
   }

   RARCH_LOG("Parsing CUE file '%s'...\n", cue_path);

   tmp_token[0] = '\0';

   while (get_token(fd, tmp_token, MAX_TOKEN_LEN) > 0)
   {
      if (string_is_equal(tmp_token, "FILE"))
      {
         char cue_dir[PATH_MAX_LENGTH];

         cue_dir[0] = '\0';

         fill_pathname_basedir(cue_dir, cue_path, sizeof(cue_dir));

         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         fill_pathname_join(track_path, cue_dir, tmp_token, max_len);

      }
      else if (string_is_equal(tmp_token, "TRACK"))
      {
         int m, s, f;
         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         get_token(fd, tmp_token, MAX_TOKEN_LEN);

         if (string_is_equal(tmp_token, "AUDIO"))
            continue;

         find_token(fd, "INDEX");
         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         get_token(fd, tmp_token, MAX_TOKEN_LEN);

         if (sscanf(tmp_token, "%02d:%02d:%02d", &m, &s, &f) < 3)
         {
            RARCH_LOG("Error parsing time stamp '%s'\n", tmp_token);
            filestream_close(fd);
            return -errno;
         }

         *offset = ((m * 60) * (s * 75) * f) * 25;

         RARCH_LOG("%s '%s+%d'\n",
               msg_hash_to_str(MSG_FOUND_FIRST_DATA_TRACK_ON_FILE),
               track_path, *offset);

         rv = 0;
         goto clean;
      }
   }

   rv = -EINVAL;

clean:
   filestream_close(fd);
   return rv;
}
