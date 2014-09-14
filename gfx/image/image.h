/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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
#include "../../boolean.h"

#ifdef _WIN32
#include "../context/win32_common.h"
#ifdef _XBOX1
#include "../d3d/d3d_defines.h"
#endif
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

bool texture_image_load(struct texture_image *img, const char *path);
void texture_image_free(struct texture_image *img);

#endif
