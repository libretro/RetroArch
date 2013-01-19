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
#include "gfx/scaler/scaler.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_ZLIB_DEFLATE
#include "gfx/rpng/rpng.h"
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

   dump_lines_file(file, lines, line_size, height);

end:
   for (int i = 0; i < height; i++)
      free(lines[i]);
   free(lines);
}
#endif

void screenshot_generate_filename(char *filename, size_t size)
{
   time_t cur_time;
   time(&cur_time);

#ifdef HAVE_ZLIB_DEFLATE
#define IMG_EXT "png"
#else
#define IMG_EXT "bmp"
#endif

   strftime(filename, size, "RetroArch-%m%d-%H%M%S." IMG_EXT, localtime(&cur_time));
}

// Take frame bottom-up.
bool screenshot_dump(const char *folder, const void *frame,
      unsigned width, unsigned height, int pitch, bool bgr24)
{
   char filename[PATH_MAX];
   char shotname[PATH_MAX];

   screenshot_generate_filename(shotname, sizeof(shotname));
   fill_pathname_join(filename, folder, shotname, sizeof(filename));

#ifdef HAVE_ZLIB_DEFLATE
   uint8_t *out_buffer = (uint8_t*)malloc(width * height * 3);
   if (!out_buffer)
      return false;

   struct scaler_ctx scaler = {0};
   scaler.in_width   = width;
   scaler.in_height  = height;
   scaler.out_width  = width;
   scaler.out_height = height;
   scaler.in_stride  = -pitch;
   scaler.out_stride = width * 3;
   scaler.out_fmt = SCALER_FMT_BGR24;
   scaler.scaler_type = SCALER_TYPE_POINT;

   if (bgr24)
      scaler.in_fmt = SCALER_FMT_BGR24;
   else if (g_extern.system.pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888)
      scaler.in_fmt = SCALER_FMT_ARGB8888;
   else
      scaler.in_fmt = SCALER_FMT_RGB565;

   scaler_ctx_gen_filter(&scaler);
   scaler_ctx_scale(&scaler, out_buffer, (const uint8_t*)frame + ((int)height - 1) * pitch);
   scaler_ctx_gen_reset(&scaler);

   RARCH_LOG("Using RPNG for PNG screenshots.\n");
   bool ret = rpng_save_image_bgr24(filename, out_buffer, width, height, width * 3);
   if (!ret)
      RARCH_ERR("Failed to take screenshot.\n");
   free(out_buffer);
   return ret;
#else
   FILE *file = fopen(filename, "wb");
   if (!file)
   {
      RARCH_ERR("Failed to open file \"%s\" for screenshot.\n", filename);
      return false;
   }

   bool ret = write_header_bmp(file, width, height);

   if (ret)
      dump_content(file, frame, width, height, pitch, bgr24);
   else
      RARCH_ERR("Failed to write image header.\n");

   fclose(file);
   return ret;
#endif
}

