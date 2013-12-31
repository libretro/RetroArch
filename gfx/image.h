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

#ifndef __RARCH_IMAGE_H
#define __RARCH_IMAGE_H

#include <stdint.h>
#include "../boolean.h"

#ifdef _XBOX1
#include <xtl.h>
#include "../xdk/xdk_defines.h"
#endif

struct texture_image
{
   unsigned width;
   unsigned height;
#ifdef _XBOX1
   unsigned x;
   unsigned y;
   LPDIRECT3DTEXTURE pixels;
   LPDIRECT3DVERTEXBUFFER vertex_buf;
#else
   uint32_t *pixels;
#endif
};

bool texture_image_load(const char *path, struct texture_image* img);
void texture_image_free(struct texture_image *img);

#endif

