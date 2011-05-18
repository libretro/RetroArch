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


#include "image.h"
#include <Imlib2.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

// Imlib2 <3<3<3<3
bool texture_image_load(const char *path, struct texture_image *out_img)
{
   Imlib_Image img;
   img = imlib_load_image(path);
   if (!img)
      return false;

   imlib_context_set_image(img);

   out_img->width = imlib_image_get_width();
   out_img->height = imlib_image_get_height();

   size_t size = out_img->width * out_img->height * sizeof(uint32_t);
   out_img->pixels = malloc(size);
   if (!out_img->pixels)
   {
      imlib_free_image();
      return false;
   }

   const uint32_t *read = imlib_image_get_data_for_reading_only();
   // Convert ARGB -> RGBA.
   for (unsigned i = 0; i < size / sizeof(uint32_t); i++)
      out_img->pixels[i] = (read[i] >> 24) | (read[i] << 8);

   imlib_free_image();
   return true;
}
