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

#include "screenshot.h"
#include "compat/strl.h"
#include <stdio.h>
#include <time.h>
#include "boolean.h"
#include <stdint.h>
#include <string.h>
#include "general.h"
#include "file.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_LIBPNG
#include <png.h>
#endif

#ifdef HAVE_LIBPNG
static png_structp png_ptr;
static png_infop png_info_ptr;

static void destroy_png(void)
{
   if (png_ptr)
      png_destroy_write_struct(&png_ptr, &png_info_ptr);
}

static bool write_header_png(FILE *file, unsigned width, unsigned height)
{
   png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
   if (!png_ptr)
      return false;

   if (setjmp(png_jmpbuf(png_ptr)))
      goto error;

   png_info_ptr = png_create_info_struct(png_ptr);
   if (!png_info_ptr)
      goto error;

   png_init_io(png_ptr, file);

   png_set_IHDR(png_ptr, png_info_ptr, width, height, 8,
         PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

   png_write_info(png_ptr, png_info_ptr);
   png_set_compression_level(png_ptr, 2);

   return true;

error:
   destroy_png();
   return false;
}

static void dump_lines_png(uint8_t **lines, int height)
{
   if (setjmp(png_jmpbuf(png_ptr)))
   {
      RARCH_ERR("PNG: dump_lines_png() failed!\n");
      goto end;
   }

   // PNG is top-down, BMP is bottom-up.
   for (int i = 0, j = height - 1; i < j; i++, j--)
   {
      uint8_t *tmp = lines[i];
      lines[i] = lines[j];
      lines[j] = tmp;
   }

   png_set_rows(png_ptr, png_info_ptr, lines);
   png_write_png(png_ptr, png_info_ptr, PNG_TRANSFORM_BGR, NULL);
   png_write_end(png_ptr, NULL);

end:
   destroy_png();
}

#else

static bool write_header_bmp(FILE *file, unsigned width, unsigned height)
{
   unsigned line_size = (width * 3 + 3) & ~3;
   unsigned size = line_size * height + 54;
   unsigned size_array = line_size * height;

   // Generic BMP stuff.
   const uint8_t header[] = {
      'B', 'M',
      (uint8_t)(size >> 0), (uint8_t)(size >> 8), (uint8_t)(size >> 16), (uint8_t)(size >> 24),
      0, 0, 0, 0,
      54, 0, 0, 0,
      40, 0, 0, 0,
      (uint8_t)(width >> 0), (uint8_t)(width >> 8), (uint8_t)(width >> 16), (uint8_t)(width >> 24),
      (uint8_t)(height >> 0), (uint8_t)(height >> 8), (uint8_t)(height >> 16), (uint8_t)(height >> 24),
      1, 0,
      24, 0,
      0, 0, 0, 0,
      (uint8_t)(size_array >> 0), (uint8_t)(size_array >> 8), (uint8_t)(size_array >> 16), (uint8_t)(size_array >> 24),
      19, 11, 0, 0,
      19, 11, 0, 0,
      0, 0, 0, 0,
      0, 0, 0, 0
   };

   return fwrite(header, 1, sizeof(header), file) == sizeof(header);
}

static void dump_lines_file(FILE *file, uint8_t **lines, size_t line_size, unsigned height)
{
   for (unsigned i = 0; i < height; i++)
      fwrite(lines[i], 1, line_size, file);
}

#endif

static void dump_line_bgr(uint8_t *line, const uint8_t *src, unsigned width)
{
   memcpy(line, src, width * 3);
}

static void dump_line_16(uint8_t *line, const uint16_t *src, unsigned width)
{
   for (unsigned i = 0; i < width; i++)
   {
      uint16_t pixel = *src++;
      uint8_t b = (pixel >>  0) & 0x1f;
      uint8_t g = (pixel >>  5) & 0x3f;
      uint8_t r = (pixel >> 11) & 0x1f;
      *line++   = (b << 3) | (b >> 2);
      *line++   = (g << 2) | (g >> 4);
      *line++   = (r << 3) | (r >> 2);
   }
}

static void dump_line_32(uint8_t *line, const uint32_t *src, unsigned width)
{
   for (unsigned i = 0; i < width; i++)
   {
      uint32_t pixel = *src++;
      *line++ = (pixel >>  0) & 0xff;
      *line++ = (pixel >>  8) & 0xff;
      *line++ = (pixel >> 16) & 0xff;
   }
}

static void dump_content(FILE *file, const void *frame,
      int width, int height, int pitch, bool bgr24)
{
   union
   {
      const uint8_t *u8;
      const uint16_t *u16;
      const uint32_t *u32;
   } u;
   u.u8 = (const uint8_t*)frame;

   uint8_t **lines = (uint8_t**)calloc(height, sizeof(uint8_t*));
   if (!lines)
      return;

   size_t line_size = (width * 3 + 3) & ~3;

   for (int i = 0; i < height; i++)
   {
      lines[i] = (uint8_t*)calloc(1, line_size);
      if (!lines[i])
         goto end;
   }

   if (bgr24) // BGR24 byte order. Can directly copy.
   {
      for (int j = 0; j < height; j++, u.u8 += pitch)
         dump_line_bgr(lines[j], u.u8, width);
   }
   else if (g_extern.system.pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888)
   {
      for (int j = 0; j < height; j++, u.u8 += pitch)
         dump_line_32(lines[j], u.u32, width);
   }
   else // RGB565
   {
      for (int j = 0; j < height; j++, u.u8 += pitch)
         dump_line_16(lines[j], u.u16, width);
   }

#ifdef HAVE_LIBPNG
   dump_lines_png(lines, height);
#else
   dump_lines_file(file, lines, line_size, height);
#endif

end:
   for (int i = 0; i < height; i++)
      free(lines[i]);
   free(lines);
}

void screenshot_generate_filename(char *filename, size_t size)
{
   time_t cur_time;
   time(&cur_time);

#ifdef HAVE_LIBPNG
#define IMG_EXT "png"
#else
#define IMG_EXT "bmp"
#endif

   strftime(filename, size, "RetroArch-%m%d-%H%M%S." IMG_EXT, localtime(&cur_time));
}

bool screenshot_dump(const char *folder, const void *frame,
      unsigned width, unsigned height, int pitch, bool bgr24)
{
   char filename[PATH_MAX];
   char shotname[PATH_MAX];

   screenshot_generate_filename(shotname, sizeof(shotname));
   fill_pathname_join(filename, folder, shotname, sizeof(filename));

   FILE *file = fopen(filename, "wb");
   if (!file)
   {
      RARCH_ERR("Failed to open file \"%s\" for screenshot.\n", filename);
      return false;
   }

#ifdef HAVE_LIBPNG
   bool ret = write_header_png(file, width, height);
#else
   bool ret = write_header_bmp(file, width, height);
#endif

   if (ret)
      dump_content(file, frame, width, height, pitch, bgr24);
   else
      RARCH_ERR("Failed to write image header.\n");

   fclose(file);
   return ret;
}

