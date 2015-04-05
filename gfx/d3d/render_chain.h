/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef __D3D_RENDER_CHAIN_H
#define __D3D_RENDER_CHAIN_H

#include "d3d.h"
#include "../video_state_tracker.h"
#include "../video_shader_parse.h"
#include "../../libretro.h"

struct LinkInfo
{
   unsigned tex_w, tex_h;
   struct video_shader_pass *pass;
};

#define MAX_VARIABLES 64

enum
{
   TEXTURES = 8,
   TEXTURESMASK = TEXTURES - 1
};

void renderchain_free(void *data);

void *renderchain_new(void);

void renderchain_deinit(void *data);

void renderchain_deinit_shader(void);

bool renderchain_init_shader(void *data);

bool renderchain_init(void *data,
      const video_info_t *video_info,
      void *dev_data,
      const void *final_viewport_data,
      const void *info_data,
      unsigned fmt);

void renderchain_set_final_viewport(void *data,
      void *renderchain_data, const void *viewport_data);

bool renderchain_add_pass(void *data, const void *info_data);

bool renderchain_add_lut(void *data,
      const char *id, const char *path,
      bool smooth);

void renderchain_add_state_tracker(void *data, void *tracker_data);

bool renderchain_render(void *chain_data, const void *data,
      unsigned width, unsigned height, unsigned pitch, unsigned rotation);

void renderchain_convert_geometry(void *data, const void *info_data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height,
      D3DVIEWPORT *final_viewport);

void renderchain_render_pass(void *data, void *pass_data, unsigned pass_index);

bool renderchain_init_shader_fvf(void *data, void *pass_data);

#endif

