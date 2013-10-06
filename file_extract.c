/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "file_extract.h"
#include "file.h"
#include "compat/strl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WANT_MINIZ
#include "deps/miniz/zlib.h"
#else
#include <zlib.h>
#endif

#include "hash.h"

// Modified from nall::unzip (higan).

#undef GOTO_END_ERROR
#define GOTO_END_ERROR() do { \
   RARCH_ERR("ZIP extraction failed at line: %d.\n", __LINE__); \
   ret = false; \
   goto end; \
} while(0)

static uint32_t read_le(const uint8_t *data, unsigned size)
{
   uint32_t val = 0;
   size *= 8;
   for (unsigned i = 0; i < size; i += 8)
      val |= *data++ << i;

   return val;
}

static bool inflate_data_to_file(const char *path, const uint8_t *cdata,
      uint32_t csize, uint32_t size, uint32_t crc32)
{
   bool ret = true;
   uint8_t *out_data = (uint8_t*)malloc(size);
   if (!out_data)
      return false;

   uint32_t real_crc32 = 0;
   z_stream stream = {0};

   if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
      GOTO_END_ERROR();

   stream.next_in = (uint8_t*)cdata;
   stream.avail_in = csize;
   stream.next_out = out_data;
   stream.avail_out = size;

   if (inflate(&stream, Z_FINISH) != Z_STREAM_END)
   {
      inflateEnd(&stream);
      GOTO_END_ERROR();
   }
   inflateEnd(&stream);

   real_crc32 = crc32_calculate(out_data, size);
   if (real_crc32 != crc32)
      RARCH_WARN("File CRC differs from ZIP CRC. File: 0x%x, ZIP: 0x%x.\n",
            (unsigned)real_crc32, (unsigned)crc32);

   if (!write_file(path, out_data, size))
      GOTO_END_ERROR();

end:
   free(out_data);
   return ret;
}

bool zlib_parse_file(const char *file, zlib_file_cb file_cb, void *userdata)
{
   const uint8_t *footer = NULL;
   const uint8_t *directory = NULL;

   bool ret = true;
   uint8_t *data = NULL;
   ssize_t zip_size = read_file(file, (void**)&data);
   if (zip_size < 22)
      GOTO_END_ERROR();

   footer = data + zip_size - 22;
   for (;; footer--)
   {
      if (footer <= data + 22)
         GOTO_END_ERROR();
      if (read_le(footer, 4) == 0x06054b50)
      {
         unsigned comment_len = read_le(footer + 20, 2);
         if (footer + 22 + comment_len == data + zip_size)
            break;
      }
   }

   directory = data + read_le(footer + 16, 4);

   for (;;)
   {
      uint32_t signature = read_le(directory + 0, 4);
      if (signature != 0x02014b50)
         break;

      unsigned cmode = read_le(directory + 10, 2);
      uint32_t crc32 = read_le(directory + 16, 4);
      uint32_t csize = read_le(directory + 20, 4);
      uint32_t size  = read_le(directory + 24, 4);

      unsigned namelength    = read_le(directory + 28, 2);
      unsigned extralength   = read_le(directory + 30, 2);
      unsigned commentlength = read_le(directory + 32, 2);

      char filename[PATH_MAX] = {0};
      if (namelength >= PATH_MAX)
         GOTO_END_ERROR();

      memcpy(filename, directory + 46, namelength);

      uint32_t offset   = read_le(directory + 42, 4);
      unsigned offsetNL = read_le(data + offset + 26, 2);
      unsigned offsetEL = read_le(data + offset + 28, 2);

      uint8_t *cdata = data + offset + 30 + offsetNL + offsetEL;

      RARCH_LOG("OFFSET: %u, CSIZE: %u, SIZE: %u.\n", offset + 30 + offsetNL + offsetEL, csize, size);

      if (!file_cb(filename, cdata, cmode, csize, size, crc32, userdata))
         break;

      directory += 46 + namelength + extralength + commentlength;
   }

end:
   free(data);
   return ret;
}

struct zip_extract_userdata
{
   char *zip_path;
   size_t zip_path_size;
   struct string_list *ext;
   bool found_rom;
};

static bool zip_extract_cb(const char *name, const uint8_t *cdata, unsigned cmode, uint32_t csize, uint32_t size,
      uint32_t crc32, void *userdata)
{
   struct zip_extract_userdata *data = (struct zip_extract_userdata*)userdata;

   // Extract first ROM that matches our list.
   const char *ext = path_get_extension(name);
   if (ext && string_list_find_elem(data->ext, ext))
   {
      char new_path[PATH_MAX];
      fill_pathname_resolve_relative(new_path, data->zip_path,
            path_basename(name), sizeof(new_path));

      switch (cmode)
      {
         case 0: // Uncompressed
            data->found_rom = write_file(new_path, cdata, size);
            return false;

         case 8: // Deflate
            if (inflate_data_to_file(new_path, cdata, csize, size, crc32))
            {
               strlcpy(data->zip_path, new_path, data->zip_path_size);
               data->found_rom = true;
               return false;
            }
            else
               return false;

         default:
            return false;
      }
   }

   return true;
}

bool zlib_extract_first_rom(char *zip_path, size_t zip_path_size, const char *valid_exts)
{
   bool ret;
   struct zip_extract_userdata userdata = {0};
   struct string_list *list;

   if (!valid_exts)
   {
      RARCH_ERR("Libretro implementation does not have any valid extensions. Cannot unzip without knowing this.\n");
      return false;
   }

   ret = true;
   list = string_split(valid_exts, "|");
   if (!list)
      GOTO_END_ERROR();

   userdata.zip_path = zip_path;
   userdata.zip_path_size = zip_path_size;
   userdata.ext = list;

   if (!zlib_parse_file(zip_path, zip_extract_cb, &userdata))
   {
      RARCH_ERR("Parsing ZIP failed.\n");
      GOTO_END_ERROR();
   }

   if (!userdata.found_rom)
   {
      RARCH_ERR("Didn't find any ROMS that matched valid extensions for libretro implementation.\n");
      GOTO_END_ERROR();
   }

end:
   if (list)
      string_list_free(list);
   return ret;
}

static bool zlib_get_file_list_cb(const char *path, const uint8_t *cdata, unsigned cmode,
      uint32_t csize, uint32_t size,
      uint32_t crc32, void *userdata)
{
   (void)cdata;
   (void)cmode;
   (void)csize;
   (void)size;
   (void)crc32;
   struct string_list *list = (struct string_list*)userdata;
   union string_list_elem_attr attr;
   memset(&attr, 0, sizeof(attr));
   return string_list_append(list, path, attr);
}

struct string_list *zlib_get_file_list(const char *path)
{
   struct string_list *list = string_list_new();
   if (!list)
      return NULL;

   if (!zlib_parse_file(path, zlib_get_file_list_cb, list))
   {
      RARCH_ERR("Parsing ZIP failed.\n");
      string_list_free(list);
      return NULL;
   }
   else
      return list;
}

