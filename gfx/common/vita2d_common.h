/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef VITA2D_COMMON_H__
#define VITA2D_COMMON_H__

#include <vita2d.h>

#include <retro_inline.h>
#include <string/stdstring.h>
#include <gfx/math/matrix_4x4.h>

#include "../../defines/psp_defines.h"
#include "../../driver.h"
#include "../../retroarch.h"
#include "../video_coord_array.h"

typedef struct vita_menu_frame
{
   bool active;
   int width;
   int height;
   vita2d_texture *texture;
} vita_menu_t;

#ifdef HAVE_OVERLAY
struct vita_overlay_data
{
   vita2d_texture *tex;
   float x;
   float y;
   float w;
   float h;
   float tex_x;
   float tex_y;
   float tex_w;
   float tex_h;
   float alpha_mod;
   float width;
   float height;
};
#endif

typedef struct vita_video
{
   vita2d_texture *texture;
   SceGxmTextureFormat format;
   int width;
   int height;
   SceGxmTextureFilter tex_filter;

   bool fullscreen;
   bool vsync;
   bool rgb32;

   video_viewport_t vp;

   unsigned rotation;
   math_matrix_4x4 mvp, mvp_no_rot;

   bool vblank_not_reached;
   bool keep_aspect;
   bool should_resize;

   vita_menu_t menu;

#ifdef HAVE_OVERLAY
   struct vita_overlay_data *overlay;
   unsigned overlays;
   bool overlay_enable;
   bool overlay_full_screen;
#endif

} vita_video_t;

#endif
