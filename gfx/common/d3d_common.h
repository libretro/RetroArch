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

#include <boolean.h>
#include <retro_common_api.h>

#include "../video_driver.h"

RETRO_BEGIN_DECLS

typedef struct d3d_texture
{
   void *data;
   INT32 pool;
} d3d_texture_t;

#ifndef BYTE_CLAMP
#define BYTE_CLAMP(i) (int) ((((i) > 255) ? 255 : (((i) < 0) ? 0 : (i))))
#endif

#ifndef D3DCOLOR_ARGB
#define D3DCOLOR_ARGB(_a, _r, _g, _b) ( (DWORD)( ( ( (_a)&0xff)<<24)|( ( (_r)&0xff)<<16)|( ( (_g)&0xff)<<8)|( (_b)&0xff) ) )
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

RETRO_END_DECLS

#endif
