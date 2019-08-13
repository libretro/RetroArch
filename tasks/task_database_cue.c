/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
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
#include <streams/interface_stream.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../database_info.h"

#include "tasks_internal.h"

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
   { 0x000400, "gc",     "\x00\x01\xC7\x04\x80\x28\x00\x60\x00\x00\x00\x00\x00\x00\x00\x00\x00"},
   { 0,        NULL,     NULL}
};

static int64_t get_token(intfstream_t *fd, char *token, uint64_t max_len)
{
   char *c       = token;
   int64_t len   = 0;
   int in_string = 0;

   while (1)
   {
      int64_t rv = (int64_t)intfstream_read(fd, c, 1);
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
      if (len == (int64_t)max_len)
      {
         *c = '\0';
         return len;
      }
   }
}

static int detect_ps1_game_sub(intfstream_t *fp,
      char *game_id, int sub_channel_mixed)
{
   uint8_t* tmp;
   uint8_t* boot_file;
   int skip, frame_size, is_mode1, cd_sector;
   uint8_t buffer[2048 * 2];

   buffer[0] = '\0';
   is_mode1  = 0;

   if (intfstream_seek(fp, 0, SEEK_END) == -1)
      goto error;

   if (!sub_channel_mixed)
   {
      if (!(intfstream_tell(fp) & 0x7FF))
      {
         unsigned int mode_test = 0;

         if (intfstream_seek(fp, 0, SEEK_SET) == -1)
            goto error;

         intfstream_read(fp, &mode_test, 4);
         if (mode_test != MODETEST_VAL)
            is_mode1 = 1;
      }
   }

   skip       = is_mode1? 0: 24;
   frame_size = sub_channel_mixed? 2448: is_mode1? 2048: 2352;

   if (intfstream_seek(fp, 156 + skip + 16 * frame_size, SEEK_SET) == -1)
      goto error;

   intfstream_read(fp, buffer, 6);

   cd_sector = buffer[2] | (buffer[3] << 8) | (buffer[4] << 16);

   if (intfstream_seek(fp, skip + cd_sector * frame_size, SEEK_SET) == -1)
      goto error;
   intfstream_read(fp, buffer, 2048 * 2);

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
   if (intfstream_seek(fp, skip + cd_sector * frame_size, SEEK_SET) == -1)
      goto error;

   intfstream_read(fp, buffer, 256);
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

   return 1;

error:
   return 0;
}

int detect_ps1_game(intfstream_t *fd, char *game_id)
{
   if (detect_ps1_game_sub(fd, game_id, 0))
      return 1;

   return detect_ps1_game_sub(fd, game_id, 1);
}

int detect_psp_game(intfstream_t *fd, char *game_id)
{
   unsigned pos;
   bool rv   = false;

   for (pos = 0; pos < 100000; pos++)
   {
      intfstream_seek(fd, pos, SEEK_SET);

      if (intfstream_read(fd, game_id, 5) > 0)
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
                  intfstream_seek(fd, pos, SEEK_SET);
                  if (intfstream_read(fd, game_id, 10) > 0)
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

   return rv;
}

int detect_gc_game(intfstream_t *fd, char *game_id)
{
   bool rv   = false;

   intfstream_seek(fd, 0, SEEK_SET);

   if (intfstream_read(fd, game_id, 6) > 0)
   {
      game_id[6] = '\0';
      rv = true;
   }

   return rv;
}

/**
 * Check for an ASCII serial in the first few bits of the ISO (Wii).
 */
int detect_serial_ascii_game(intfstream_t *fd, char *game_id)
{
   unsigned pos;
   int numberOfAscii = 0;
   bool rv   = false;

   for (pos = 0; pos < 10000; pos++)
   {
      intfstream_seek(fd, pos, SEEK_SET);
      if (intfstream_read(fd, game_id, 15) > 0)
      {
         unsigned i;
         game_id[15] = '\0';
         numberOfAscii = 0;

         /* When scanning WBFS files, "WBFS" is discovered as the first serial. Ignore it. */
         if (string_is_equal(game_id, "WBFS")) {
            continue;
         }

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

   return rv;
}

int detect_system(intfstream_t *fd, const char **system_name)
{
   int rv;
   char magic[MAGIC_LEN];
   int i;
   int64_t read;

   RARCH_LOG("%s\n", msg_hash_to_str(MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS));
   for (i = 0; MAGIC_NUMBERS[i].system_name != NULL; i++)
   {
      intfstream_seek(fd, MAGIC_NUMBERS[i].offset, SEEK_SET);

      read = intfstream_read(fd, magic, MAGIC_LEN);
      if (read < 0)
      {
         RARCH_LOG("Could not read data at offset %d: %s\n",
               MAGIC_NUMBERS[i].offset, strerror(errno));
         rv = -errno;
         goto clean;
      }

      if (read < MAGIC_LEN)
         continue;

      if (memcmp(MAGIC_NUMBERS[i].magic, magic, MAGIC_LEN) == 0)
      {
         *system_name = MAGIC_NUMBERS[i].system_name;
         rv = 0;
         goto clean;
      }
   }

   intfstream_seek(fd, 0x8008, SEEK_SET);
   if (intfstream_read(fd, magic, 8) > 0)
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
   return rv;
}

static int64_t intfstream_get_file_size(const char *path)
{
   int64_t rv;
   intfstream_t *fd = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
   if (!fd)
      return -1;
   rv = intfstream_get_size(fd);
   intfstream_close(fd);
   free(fd);
   return rv;
}

static bool update_cand(int64_t *cand_index, int64_t *last_index,
                        uint64_t *largest, char *last_file, uint64_t *offset,
                        uint64_t *size, char *track_path, uint64_t max_len)
{
   if (*cand_index != -1)
   {
      if ((uint64_t)(*last_index - *cand_index) > *largest)
      {
         *largest    = *last_index - *cand_index;
         strlcpy(track_path, last_file, (size_t)max_len);
         *offset     = *cand_index;
         *size       = *largest;
         *cand_index = -1;
         return true;
      }
      *cand_index = -1;
   }
   return false;
}

int cue_find_track(const char *cue_path, bool first,
      uint64_t *offset, uint64_t *size, char *track_path, uint64_t max_len)
{
   int rv;
   intfstream_info_t info;
   char *tmp_token            = (char*)malloc(MAX_TOKEN_LEN);
   char *last_file            = (char*)malloc(PATH_MAX_LENGTH + 1);
   intfstream_t *fd           = NULL;
   int64_t last_index         = -1;
   int64_t cand_index         = -1;
   int32_t cand_track         = -1;
   int32_t track              = 0;
   uint64_t largest             = 0;
   int64_t volatile file_size = -1;
   bool is_data               = false;
   char *cue_dir              = (char*)malloc(PATH_MAX_LENGTH);
   cue_dir[0]                 = '\0';

   fill_pathname_basedir(cue_dir, cue_path, PATH_MAX_LENGTH);

   info.type        = INTFSTREAM_FILE;
   fd               = (intfstream_t*)intfstream_init(&info);

   if (!fd)
      goto error;

   if (!intfstream_open(fd, cue_path,
            RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE))
   {
      RARCH_LOG("Could not open CUE file '%s': %s\n", cue_path,
            strerror(errno));
      goto error;
   }

   RARCH_LOG("Parsing CUE file '%s'...\n", cue_path);

   tmp_token[0] = '\0';

   rv = -EINVAL;

   while (get_token(fd, tmp_token, MAX_TOKEN_LEN) > 0)
   {
      if (string_is_equal_noncase(tmp_token, "FILE"))
      {
         /* Set last index to last EOF */
         if (file_size != -1)
            last_index = file_size;

         /* We're changing files since the candidate, update it */
         if (update_cand(&cand_index, &last_index, &largest, last_file, offset,
                         size, track_path, max_len))
         {
            rv = 0;
            if (first)
               goto clean;
         }

         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         fill_pathname_join(last_file, cue_dir, tmp_token, PATH_MAX_LENGTH);

         file_size = intfstream_get_file_size(last_file);

         get_token(fd, tmp_token, MAX_TOKEN_LEN);

      }
      else if (string_is_equal_noncase(tmp_token, "TRACK"))
      {
         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         is_data = !string_is_equal_noncase(tmp_token, "AUDIO");
         ++track;
      }
      else if (string_is_equal_noncase(tmp_token, "INDEX"))
      {
         int m, s, f;
         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         get_token(fd, tmp_token, MAX_TOKEN_LEN);

         if (sscanf(tmp_token, "%02d:%02d:%02d", &m, &s, &f) < 3)
         {
            RARCH_LOG("Error parsing time stamp '%s'\n", tmp_token);
            goto error;
         }

         last_index = (size_t) (((m * 60 + s) * 75) + f) * 2352;

         /* If we've changed tracks since the candidate, update it */
         if (cand_track != -1 && track != cand_track &&
             update_cand(&cand_index, &last_index, &largest, last_file, offset,
                         size, track_path, max_len))
         {
            rv = 0;
            if (first)
               goto clean;
         }

         if (!is_data)
            continue;

         if (cand_index == -1)
         {
            cand_index = last_index;
            cand_track = track;
         }
      }
   }

   if (file_size != -1)
      last_index = file_size;

   if (update_cand(&cand_index, &last_index, &largest, last_file, offset,
                   size, track_path, max_len))
      rv = 0;

clean:
   free(cue_dir);
   free(tmp_token);
   free(last_file);
   intfstream_close(fd);
   free(fd);
   return rv;

error:
   free(cue_dir);
   free(tmp_token);
   free(last_file);
   if (fd)
   {
      intfstream_close(fd);
      free(fd);
   }
   return -errno;
}

bool cue_next_file(intfstream_t *fd,
      const char *cue_path, char *path, uint64_t max_len)
{
   bool rv                    = false;
   char *tmp_token            = (char*)malloc(MAX_TOKEN_LEN);
   char *cue_dir              = (char*)malloc(PATH_MAX_LENGTH);
   cue_dir[0]                 = '\0';

   fill_pathname_basedir(cue_dir, cue_path, PATH_MAX_LENGTH);

   tmp_token[0] = '\0';

   while (get_token(fd, tmp_token, MAX_TOKEN_LEN) > 0)
   {
      if (string_is_equal_noncase(tmp_token, "FILE"))
      {
         get_token(fd, tmp_token, MAX_TOKEN_LEN);
         fill_pathname_join(path, cue_dir, tmp_token, (size_t)max_len);
         rv = true;
         break;
      }
   }

   free(cue_dir);
   free(tmp_token);
   return rv;
}

int gdi_find_track(const char *gdi_path, bool first,
      char *track_path, uint64_t max_len)
{
   int rv;
   intfstream_info_t info;
   char *tmp_token   = (char*)malloc(MAX_TOKEN_LEN);
   intfstream_t *fd  = NULL;
   uint64_t largest  = 0;
   int size          = -1;
   int mode          = -1;
   int64_t file_size = -1;

   info.type         = INTFSTREAM_FILE;

   fd                = (intfstream_t*)intfstream_init(&info);

   if (!fd)
      goto error;

   if (!intfstream_open(fd, gdi_path,
            RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE))
   {
      RARCH_LOG("Could not open GDI file '%s': %s\n", gdi_path,
            strerror(errno));
      goto error;
   }

   RARCH_LOG("Parsing GDI file '%s'...\n", gdi_path);

   tmp_token[0] = '\0';

   rv = -EINVAL;

   /* Skip track count */
   get_token(fd, tmp_token, MAX_TOKEN_LEN);

   /* Track number */
   while (get_token(fd, tmp_token, MAX_TOKEN_LEN) > 0)
   {
      /* Offset */
      if (get_token(fd, tmp_token, MAX_TOKEN_LEN) <= 0)
      {
         errno = EINVAL;
         goto error;
      }

      /* Mode */
      if (get_token(fd, tmp_token, MAX_TOKEN_LEN) <= 0)
      {
         errno = EINVAL;
         goto error;
      }
      mode = atoi(tmp_token);

      /* Sector size */
      if (get_token(fd, tmp_token, MAX_TOKEN_LEN) <= 0)
      {
         errno = EINVAL;
         goto error;
      }
      size = atoi(tmp_token);

      /* File name */
      if (get_token(fd, tmp_token, MAX_TOKEN_LEN) <= 0)
      {
         errno = EINVAL;
         goto error;
      }

      /* Check for data track */
      if (!(mode == 0 && size == 2352))
      {
         char *last_file   = (char*)malloc(PATH_MAX_LENGTH + 1);
         char *gdi_dir     = (char*)malloc(PATH_MAX_LENGTH);

         gdi_dir[0]        = '\0';

         fill_pathname_basedir(gdi_dir, gdi_path, PATH_MAX_LENGTH);

         fill_pathname_join(last_file,
               gdi_dir, tmp_token, PATH_MAX_LENGTH);
         file_size = intfstream_get_file_size(last_file);
         if (file_size < 0)
         {
            free(gdi_dir);
            free(last_file);
            goto error;
         }

         if ((uint64_t)file_size > largest)
         {
            strlcpy(track_path, last_file, (size_t)max_len);

            rv      = 0;
            largest = file_size;

            if (first)
            {
               free(gdi_dir);
               free(last_file);
               goto clean;
            }
         }
         free(gdi_dir);
         free(last_file);
      }

      /* Disc offset (not used?) */
      if (get_token(fd, tmp_token, MAX_TOKEN_LEN) <= 0)
      {
         errno = EINVAL;
         goto error;
      }
   }

clean:
   free(tmp_token);
   intfstream_close(fd);
   free(fd);
   return rv;

error:
   free(tmp_token);
   if (fd)
   {
      intfstream_close(fd);
      free(fd);
   }
   return -errno;
}

bool gdi_next_file(intfstream_t *fd, const char *gdi_path,
      char *path, uint64_t max_len)
{
   bool rv         = false;
   char *tmp_token = (char*)malloc(MAX_TOKEN_LEN);
   int64_t offset  = -1;

   tmp_token[0]    = '\0';

   /* Skip initial track count */
   offset = intfstream_tell(fd);
   if (offset == 0)
      get_token(fd, tmp_token, MAX_TOKEN_LEN);

   /* Track number */
   get_token(fd, tmp_token, MAX_TOKEN_LEN);

   /* Offset */
   get_token(fd, tmp_token, MAX_TOKEN_LEN);

   /* Mode */
   get_token(fd, tmp_token, MAX_TOKEN_LEN);

   /* Sector size */
   get_token(fd, tmp_token, MAX_TOKEN_LEN);

   /* File name */
   if (get_token(fd, tmp_token, MAX_TOKEN_LEN) > 0)
   {
      char *gdi_dir   = (char*)malloc(PATH_MAX_LENGTH);

      gdi_dir[0]      = '\0';

      fill_pathname_basedir(gdi_dir, gdi_path, PATH_MAX_LENGTH);

      fill_pathname_join(path, gdi_dir, tmp_token, (size_t)max_len);
      rv = true;

      /* Disc offset */
      get_token(fd, tmp_token, MAX_TOKEN_LEN);

      free(gdi_dir);
   }

   free(tmp_token);
   return rv;
}
