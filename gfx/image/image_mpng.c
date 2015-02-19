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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "image.h"
#include "../../file_ops.h"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "../../general.h"
#include <formats/mpng.h>
#include <file/nbio.h>

void texture_image_free(struct texture_image *img)
{
   if (!img)
      return;

   free(img->pixels);
   memset(img, 0, sizeof(*img));
}


bool texture_image_load(struct texture_image *out_img, const char *path)
{
   size_t len;
   bool looped = false;

   bool ret         = false;
   bool use_rgba    = driver.gfx_use_rgba;
   unsigned a_shift = 24;
   unsigned r_shift = use_rgba ? 0 : 16;
   unsigned g_shift = 8;
   unsigned b_shift = use_rgba ? 16 : 0;

   void *ptr = NULL;
   struct mpng_image img = {0};
   struct nbio_t* read = nbio_open(path, NBIO_READ);

   if (!read)
      return false;

   ptr = nbio_get_ptr(read, &len);

   if (len != 1024*1024)
      RARCH_ERR("ERROR: wrong size (2).\n");
   if (ptr)
      RARCH_WARN("Read pointer is available before iterating?\n");

   nbio_begin_read(read);

   while (!nbio_iterate(read))
      looped=true;

   if (!looped)
      RARCH_LOG("Read finished immediately?\n");

   ptr = nbio_get_ptr(read, &len);

   if (len != 1024*1024)
      puts("ERROR: wrong size (3)");
   if (*(char*)ptr != 0x42 || memcmp(ptr, (char*)ptr+1, 1024*1024-1))
      puts("ERROR: wrong data");

   if (!png_decode(ptr, len, &img, FMT_ARGB8888))
      goto error;

   out_img->width  = img.width;
   out_img->height = img.height;
   out_img->pixels = img.pixels;

   /* This is quite uncommon. */
   if (a_shift != 24 || r_shift != 16 || g_shift != 8 || b_shift != 0)
   {
      uint32_t i;
      uint32_t num_pixels = out_img->width * out_img->height;
      uint32_t *pixels = (uint32_t*)out_img->pixels;

      for (i = 0; i < num_pixels; i++)
      {
         uint32_t col = pixels[i];
         uint8_t a = (uint8_t)(col >> 24);
         uint8_t r = (uint8_t)(col >> 16);
         uint8_t g = (uint8_t)(col >>  8);
         uint8_t b = (uint8_t)(col >>  0);
         pixels[i] = (a << a_shift) |
            (r << r_shift) | (g << g_shift) | (b << b_shift);
      }
   }

   nbio_free(read);

   return true;

error:
   if (read)
      nbio_free(read);
   return false;
}
