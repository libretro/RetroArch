/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <formats/tga.h>
#include <formats/image.h>

bool rtga_image_load_shift(uint8_t *buf,
      void *data,
      unsigned a_shift, unsigned r_shift,
      unsigned g_shift, unsigned b_shift)
{
   unsigned i, bits, size, bits_mul;
   uint8_t               info[6] = {0};
   unsigned                width = 0;
   unsigned               height = 0;
   const uint8_t            *tmp = NULL;
   struct texture_image *out_img = (struct texture_image*)data;

   if (!buf || buf[2] != 2)
   {
      fprintf(stderr, "TGA image is not uncompressed RGB.\n");
      goto error;
   }

   memcpy(info, buf + 12, 6);

   width  = info[0] + ((unsigned)info[1] * 256);
   height = info[2] + ((unsigned)info[3] * 256);
   bits   = info[4];

   fprintf(stderr, "Loaded TGA: (%ux%u @ %u bpp)\n", width, height, bits);

   size            = width * height * sizeof(uint32_t);
   out_img->pixels = (uint32_t*)malloc(size);
   out_img->width  = width;
   out_img->height = height;

   if (!out_img->pixels)
   {
      fprintf(stderr, "Failed to allocate TGA pixels.\n");
      goto error;
   }

   tmp      = buf + 18;
   bits_mul = 3;

   if (bits != 32 && bits != 24)
   {
      fprintf(stderr, "Bit depth of TGA image is wrong. Only 32-bit and 24-bit supported.\n");
      goto error;
   }

   if (bits == 32)
      bits_mul = 4;

   for (i = 0; i < width * height; i++)
   {
      uint32_t b = tmp[i * bits_mul + 0];
      uint32_t g = tmp[i * bits_mul + 1];
      uint32_t r = tmp[i * bits_mul + 2];
      uint32_t a = tmp[i * bits_mul + 3];

      if (bits == 24)
         a = 0xff;

      out_img->pixels[i] = (a << a_shift) |
         (r << r_shift) | (g << g_shift) | (b << b_shift);
   }

   return true;

error:
   if (out_img->pixels)
      free(out_img->pixels);

   out_img->pixels = NULL;
   out_img->width  = out_img->height = 0;
   return false;
}
