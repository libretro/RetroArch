/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "screenshot.h"
#include "strl.h"
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
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
      (size >> 0) & 0xff, (size >> 8) & 0xff, (size >> 16) & 0xff, (size >> 24) & 0xff,
      0, 0, 0, 0,
      54, 0, 0, 0,
      40, 0, 0, 0,
      (width >> 0) & 0xff, (width >> 8) & 0xff, (width >> 16) & 0xff, (width >> 24) & 0xff,
      (height >> 0) & 0xff, (height >> 8) & 0xff, (height >> 16) & 0xff, (height >> 24) & 0xff,
      1, 0,
      24, 0,
      0, 0, 0, 0,
      (size_array >> 0) & 0xff, (size_array >> 8) & 0xff, (size_array >> 16) & 0xff, (size_array >> 24) & 0xff,
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
   uint8_t line[line_size];
   memset(line, 0, sizeof(line));

   // BMP likes reverse ordering for some reason :v
   for (int j = height - 1; j >= 0; j--)
   {
      uint8_t * restrict dst = line;
      const uint16_t * restrict src = frame + j * pitch;
      for (unsigned i = 0; i < width; i++)
      {
         uint16_t pixel = *src++;
         uint8_t b = (pixel & 0x1f) << 3;
         uint8_t g = (pixel & 0x03e0) >> 2;
         uint8_t r = (pixel & 0x7c00) >> 7;
         *dst++ = b;
         *dst++ = g;
         *dst++ = r;
      }

      fwrite(line, 1, sizeof(line), file);
   }
}

bool screenshot_dump(const char *folder, const uint16_t *frame,
      unsigned width, unsigned height, unsigned pitch)
{
   time_t cur_time;
   time(&cur_time);

   char timefmt[64];
   strftime(timefmt, sizeof(timefmt), "SSNES-%m%d-%H%M%S.bmp", localtime(&cur_time));

   char filename[256];
   strlcpy(filename, folder, sizeof(filename));
   strlcat(filename, "/", sizeof(filename));
   strlcat(filename, timefmt, sizeof(filename));

   FILE *file = fopen(filename, "wb");
   if (!file)
   {
      SSNES_ERR("Failed to open file \"%s\" for screenshot.\n", filename);
      return false;
   }

   write_header(file, width, height);
   dump_content(file, frame, width, height, pitch);

   fclose(file);

   return true;
}
