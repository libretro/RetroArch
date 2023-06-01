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

#ifndef _D3D8_DEFINES_H
#define _D3D8_DEFINES_H

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_inline.h>

#include <d3d8.h>

#include "../../retroarch.h"
#include "../../verbosity.h"

RETRO_BEGIN_DECLS

typedef struct d3d8_video
{
   bool keep_aspect;
   bool should_resize;
   bool quitting;
   bool needs_restore;
   bool overlays_enabled;
   /* TODO - refactor this away properly. */
   bool resolution_hd_enable;

   /* Only used for Xbox */
   bool widescreen_mode;

   unsigned cur_mon_id;
   unsigned dev_rotation;

   overlay_t *menu;
   void *renderchain_data;

   math_matrix_4x4 mvp;
   math_matrix_4x4 mvp_rotate;
   math_matrix_4x4 mvp_transposed;

   struct video_viewport vp;
   struct video_shader shader;
   video_info_t video_info;
#ifdef HAVE_WINDOW
   WNDCLASSEX windowClass;
#endif
   LPDIRECT3DDEVICE8 dev;
   D3DVIEWPORT8 final_viewport;

   char *shader_path;

   struct
   {
      int size;
      int offset;
      void *buffer;
      void *decl;
   }menu_display;

   size_t overlays_size;
   overlay_t *overlays;
} d3d8_video_t;

RETRO_END_DECLS

#endif
