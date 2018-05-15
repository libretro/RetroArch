/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2018 - Daniel De Matteis
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

#ifndef __D3D9_RENDERCHAIN_H__
#define __D3D9_RENDERCHAIN_H__

#include <stdint.h>

#include <retro_common_api.h>
#include <retro_inline.h>
#include <boolean.h>

#include <d3d9.h>
#include "../common/d3d9_common.h"

RETRO_BEGIN_DECLS

struct lut_info
{
   LPDIRECT3DTEXTURE9 tex;
   char id[64];
   bool smooth;
};

#define D3D_PI 3.14159265358979323846264338327

#define VECTOR_LIST_TYPE unsigned
#define VECTOR_LIST_NAME unsigned
#include "../../libretro-common/lists/vector_list.c"
#undef VECTOR_LIST_TYPE
#undef VECTOR_LIST_NAME

#define VECTOR_LIST_TYPE struct lut_info
#define VECTOR_LIST_NAME lut_info
#include "../../libretro-common/lists/vector_list.c"
#undef VECTOR_LIST_TYPE
#undef VECTOR_LIST_NAME

static INLINE void d3d9_renderchain_blit_to_texture(
      LPDIRECT3DTEXTURE9 tex,
      const void *frame,
      unsigned tex_width,  unsigned tex_height,
      unsigned width,      unsigned height,
      unsigned last_width, unsigned last_height,
      unsigned pitch, unsigned pixel_size)
{
   D3DLOCKED_RECT d3dlr    = {0, NULL};

   if (
         (last_width != width || last_height != height)
      )
   {
      d3d9_lock_rectangle(tex, 0, &d3dlr,
            NULL, tex_height, D3DLOCK_NOSYSLOCK);
      d3d9_lock_rectangle_clear(tex, 0, &d3dlr,
            NULL, tex_height, D3DLOCK_NOSYSLOCK);
   }

   if (d3d9_lock_rectangle(tex, 0, &d3dlr, NULL, 0, 0))
   {
      d3d9_texture_blit(pixel_size, tex,
            &d3dlr, frame, width, height, pitch);
      d3d9_unlock_rectangle(tex);
   }
}

RETRO_END_DECLS

#endif
