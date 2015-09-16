/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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
#include <fcntl.h>

#include <compat/strcasestr.h>
#include <compat/strl.h>
#include <retro_endianness.h>

#include "tasks.h"

#ifdef HAVE_LIBRETRODB
#include "../database_info.h"
#endif

#include "../dir_list_special.h"
#include "../file_ops.h"
#include "../msg_hash.h"
#include "../general.h"

#define MAGIC_LEN 16

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

ssize_t get_token(int fd, char *token, size_t max_len)
{
   int rv;
   char *c       = token;
   ssize_t len   = 0;
   int in_string = 0;

   while (1)
   {
      rv = read(fd, c, 1);
      if (rv == 0)
         return 0;
      else if (rv < 1)
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

int find_token(int fd, const char *token)
{
   int     tmp_len = strlen(token);
   char *tmp_token = (char*)calloc(tmp_len, 1);

   if (!tmp_token)
      return -1;

   while (strncmp(tmp_token, token, tmp_len) != 0)
   {
      if (get_token(fd, tmp_token, tmp_len) <= 0)
         return -1;
   }

   return 0;
}

int detect_ps1_game(const char *track_path, char *game_id)
{
   int i;
   const char *pat_c;
   char *c, *id_start;
   const char *pattern = "cdrom:";
   int              fd = open(track_path, O_RDONLY);

   if (fd < 0)
   {
      RARCH_LOG("Could not open data track: %s\n", strerror(errno));
      return -errno;
   }

   lseek(fd, 0x9340, SEEK_SET);

   if (read(fd, game_id, 10) > 0)
   {
      game_id[10] = '\0';
      game_id[4] = '-';
   }

   close(fd);
   return 1;
}

int detect_system(const char *track_path, int32_t offset,
        const char **system_name)
{
   int rv;
   char magic[MAGIC_LEN];
   int i;
   int fd = open(track_path, O_RDONLY);

   if (fd < 0)
   {
      RARCH_LOG("Could not open data track of file '%s': %s\n",
            track_path, strerror(errno));
      rv = -errno;
      goto clean;
   }

   lseek(fd, offset, SEEK_SET);
   if (read(fd, magic, MAGIC_LEN) < MAGIC_LEN)
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

   RARCH_LOG("Could not find compatible system\n");
   rv = -EINVAL;
clean:
   close(fd);
   return rv;
}

int find_first_data_track(const char *cue_path,
      int32_t *offset, char *track_path, size_t max_len)
{
   int rv;
   char tmp_token[MAX_TOKEN_LEN];
   int m, s, f;
   char cue_dir[PATH_MAX];
   int fd = -1;

   strlcpy(cue_dir, cue_path, PATH_MAX);
   path_basedir(cue_dir);

   fd = open(cue_path, O_RDONLY);
   if (fd < 0)
   {
      RARCH_LOG("Could not open CUE file '%s': %s\n", cue_path,
            strerror(errno));
      return -errno;
   }

   RARCH_LOG("Parsing CUE file '%s'...\n", cue_path);

   while (get_token(fd, tmp_token, MAX_TOKEN_LEN) > 0)
   {
      if (strcmp(tmp_token, "FILE") == 0)
      {
         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         fill_pathname_join(track_path, cue_dir, tmp_token, max_len);

      }
      else if (strcasecmp(tmp_token, "TRACK") == 0)
      {
         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         if (strcasecmp(tmp_token, "AUDIO") == 0)
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
   close(fd);
   return rv;
}
