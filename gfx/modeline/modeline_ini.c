/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2021 - Chris Kennedy, Antonio Giner,
 *                            Alexandre Wodarczyk, Gil Delescluse
 *  Copyright (C) 2026 - The RetroArch team
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>

#include "modeline_ini.h"
#include "modeline_list.h"

#ifndef MODELINE_STANDALONE
#include <streams/file_stream.h>
#include <file/file_path.h>
#include "../../verbosity.h"
#endif

#define MODELINE_INI_KEY_MAX   64
#define MODELINE_INI_VALUE_MAX 256

static const char *modeline_ini_paths[] = {
   "",
#ifdef _WIN32
   ".\\",
   ".\\ini\\",
#else
   "./",
   "./ini/",
   "/etc/",
#endif
   NULL
};

static bool modeline_ini_is_space(char c)
{
   return c == ' ' || c == '\n' || c == '\r' || c == '\t'
      || c == '\f' || c == '\v';
}

void modeline_ini_parse_buffer(video_modeline_gen_t *gen,
      const char *buf, size_t len)
{
   size_t pos = 0;
   char key[MODELINE_INI_KEY_MAX];
   char value[MODELINE_INI_VALUE_MAX];

   while (pos < len)
   {
      size_t start, end, key_end, klen, vlen;

      /* One line */
      start = pos;
      while (pos < len && buf[pos] != '\n')
         pos++;
      end = pos;
      if (pos < len)
         pos++;

      /* Trim both sides */
      while (start < end && modeline_ini_is_space(buf[start]))
         start++;
      while (end > start && modeline_ini_is_space(buf[end - 1]))
         end--;

      if (start == end || buf[start] == '#')
         continue;

      /* Key up to the first blank, value is the rest */
      key_end = start;
      while (key_end < end && !modeline_ini_is_space(buf[key_end]))
         key_end++;
      klen = key_end - start;
      if (klen == 0 || klen >= sizeof(key))
         continue;
      memcpy(key, buf + start, klen);
      key[klen] = '\0';

      while (key_end < end && modeline_ini_is_space(buf[key_end]))
         key_end++;
      vlen = end - key_end;
      if (vlen == 0)
         continue;
      if (vlen >= sizeof(value))
         vlen = sizeof(value) - 1;
      memcpy(value, buf + key_end, vlen);
      value[vlen] = '\0';

      modeline_set_option(gen, key, value);
   }
}

#ifdef MODELINE_STANDALONE
static bool modeline_ini_read(const char *path, char **buf, size_t *len)
{
   long size;
   FILE *f = fopen(path, "rb");
   if (!f)
      return false;
   fseek(f, 0, SEEK_END);
   size = ftell(f);
   fseek(f, 0, SEEK_SET);
   if (size < 0)
   {
      fclose(f);
      return false;
   }
   *buf = (char*)malloc((size_t)size + 1);
   if (!*buf)
   {
      fclose(f);
      return false;
   }
   *len = fread(*buf, 1, (size_t)size, f);
   (*buf)[*len] = '\0';
   fclose(f);
   return true;
}
#else
static bool modeline_ini_read(const char *path, char **buf, size_t *len)
{
   int64_t rlen = 0;
   if (!path_is_valid(path))
      return false;
   if (!filestream_read_file(path, (void**)buf, &rlen))
      return false;
   *len = (size_t)rlen;
   return true;
}
#endif

bool modeline_ini_load(video_modeline_gen_t *gen, const char *file_name)
{
   int i;
   char full_path[512];

   for (i = 0; modeline_ini_paths[i]; i++)
   {
      char *buf  = NULL;
      size_t len = 0;

      snprintf(full_path, sizeof(full_path), "%s%s",
            modeline_ini_paths[i], file_name);

      if (!modeline_ini_read(full_path, &buf, &len))
         continue;

      RARCH_DBG("[Modeline] Parsing %s\n", full_path);
      modeline_ini_parse_buffer(gen, buf, len);
      free(buf);
      return true;
   }
   return false;
}
