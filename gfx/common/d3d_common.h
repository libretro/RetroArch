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

struct d3d_matrix
{
   union {
      struct {
         float        _11, _12, _13, _14;
         float        _21, _22, _23, _24;
         float        _31, _32, _33, _34;
         float        _41, _42, _43, _44;

      };
      float m[4][4];
   };
};

typedef struct d3d_texture
{
   void *data;
   int32_t pool;
} d3d_texture_t;

typedef struct
{
   bool fullscreen;
   bool enabled;
   unsigned tex_w, tex_h;
   float tex_coords[4];
   float vert_coords[4];
   float alpha_mod;
   void *tex;
   void *vert_buf;
} overlay_t;

typedef struct Vertex
{
   float x, y, z;
   uint32_t color;
   float u, v;
} Vertex;

#ifndef BYTE_CLAMP
#define BYTE_CLAMP(i) (int) ((((i) > 255) ? 255 : (((i) < 0) ? 0 : (i))))
#endif

#define D3DTADDRESS_COMM_CLAMP           3
#define D3DTEXF_COMM_LINEAR              2
#define D3DPT_COMM_TRIANGLESTRIP         5

/* Clear target surface */
#define D3D_COMM_CLEAR_TARGET            0x00000001l

void *d3d_matrix_transpose(void *_pout, const void *_pm);

void *d3d_matrix_identity(void *_pout);

void *d3d_matrix_ortho_off_center_lh(void *_pout,
      float l, float r, float b, float t, float zn, float zf);

void *d3d_matrix_multiply(void *_pout,
      const void *_pm1, const void *_pm2);

void *d3d_matrix_rotation_z(void *_pout, float angle);

int32_t d3d_translate_filter(unsigned type);

void d3d_input_driver(const char* input_name,
   const char* joypad_name, input_driver_t** input, void** input_data);

RETRO_END_DECLS

#endif
