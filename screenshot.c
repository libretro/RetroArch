/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

// Simple 24bpp .BMP writer.

static void write_header(FILE *file, unsigned width, unsigned height)
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

   fwrite(header, 1, sizeof(header), file);
}

static void dump_content(FILE *file, const uint16_t *frame, unsigned width, unsigned height, unsigned pitch)
{
   pitch >>= 1;
   unsigned line_size = (width * 3 + 3) & ~3;
   uint8_t *line = (uint8_t*)calloc(1, line_size);
   if (!line)
      return;

   // BMP likes reverse ordering for some reason :v
   for (int j = height - 1; j >= 0; j--)
   {
      uint8_t *dst = line;
      const uint16_t *src = frame + j * pitch;
      for (unsigned i = 0; i < width; i++)
      {
         uint16_t pixel = *src++;
         uint8_t b = (pixel >>  0) & 0x1f;
         uint8_t g = (pixel >>  5) & 0x1f;
         uint8_t r = (pixel >> 10) & 0x1f;
         *dst++ = (b << 3) | (b >> 2);
         *dst++ = (g << 3) | (g >> 2);
         *dst++ = (r << 3) | (r >> 2);
      }

      fwrite(line, 1, line_size, file);
   }

   free(line);
}

bool screenshot_dump(const char *folder, const uint16_t *frame,
      unsigned width, unsigned height, unsigned pitch)
{
   time_t cur_time;
   time(&cur_time);

   char timefmt[128];
   strftime(timefmt, sizeof(timefmt), "SSNES-%m%d-%H%M%S.bmp", localtime(&cur_time));

   char filename[256];
   strlcpy(filename, folder, sizeof(filename));
   strlcat(filename, "/", sizeof(filename));
   strlcat(filename, timefmt, sizeof(filename));

   FILE *file = fopen(filename, "wb");
   if (!file)
   {
      RARCH_ERR("Failed to open file \"%s\" for screenshot.\n", filename);
      return false;
   }

   write_header(file, width, height);
   dump_content(file, frame, width, height, pitch);

   fclose(file);

   return true;
}
