/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifndef __RARCH_IMAGE_CONTEXT_H
#define __RARCH_IMAGE_CONTEXT_H

#include <stdint.h>

#include <retro_common_api.h>

#include <boolean.h>

RETRO_BEGIN_DECLS

enum image_process_code
{
   IMAGE_PROCESS_ERROR     = -2,
   IMAGE_PROCESS_ERROR_END = -1,
   IMAGE_PROCESS_NEXT      =  0,
   IMAGE_PROCESS_END       =  1
};

struct texture_image
{
   unsigned width;
   unsigned height;
   uint32_t *pixels;
};

bool video_texture_image_set_color_shifts(unsigned *r_shift, unsigned *g_shift,
      unsigned *b_shift, unsigned *a_shift);

bool video_texture_image_color_convert(unsigned r_shift,
      unsigned g_shift, unsigned b_shift, unsigned a_shift,
      struct texture_image *out_img);

bool video_texture_image_load(struct texture_image *img, const char *path);
void video_texture_image_free(struct texture_image *img);

RETRO_END_DECLS

#endif
