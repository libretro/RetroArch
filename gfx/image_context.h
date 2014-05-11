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

typedef struct image_ctx_driver
{
   bool (*load)(void*, const char*, void *);
   void (*free)(void *, void *);
   // Human readable string.
   const char *ident;
} image_ctx_driver_t;

extern const image_ctx_driver_t image_ctx_xdk1;
extern const image_ctx_driver_t image_ctx_ps3;
extern const image_ctx_driver_t image_ctx_sdl;
extern const image_ctx_driver_t image_ctx_rpng;

void find_prev_image_driver(void);
void find_next_image_driver(void);
void find_image_driver(void);

#endif
