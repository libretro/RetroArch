/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef _D3D_COMMON_H
#define _D3D_COMMON_H

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#if !defined(__WINRT__) && !defined(_XBOX)

#ifndef HAVE_WINDOW
#define HAVE_WINDOW
#endif

#ifndef HAVE_MONITOR
#define HAVE_MONITOR
#endif

#endif

#include <boolean.h>
#include <retro_common_api.h>

#include "../font_driver.h"
#include "../../retroarch.h"

RETRO_BEGIN_DECLS

typedef struct Vertex
{
   float x, y, z;
   uint32_t color;
   float u, v;
} Vertex;

typedef struct
{
   void *tex;
   void *vert_buf;
   /* The quad last written to vert_buf (valid while vert_sent_ok):
    * the draw locks the buffer only when the quad has changed. */
   Vertex vert_sent[4];
   /* The texture's pixel size, packed. */
   unsigned tex_dims;
   float tex_coords[4];
   float vert_coords[4];
   float alpha_mod;
   bool fullscreen;
   bool enabled;
   bool vert_sent_ok;
} overlay_t;

int32_t d3d_translate_filter(unsigned type);

void d3d_input_driver(const char* input_name,
   const char* joypad_name, input_driver_t** input, void** input_data);

RETRO_END_DECLS

#endif
