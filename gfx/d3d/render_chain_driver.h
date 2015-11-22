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

#include "../video_driver.h"
#include "../video_shader_parse.h"
#include "../video_state_tracker.h"
#include "../../libretro.h"
#include "../../defines/d3d_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

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

typedef struct renderchain_driver
{
   void (*chain_free)(void *data);
   void *(*chain_new)(void);
   bool (*init_shader)(void *data, void *renderchain_data);
   bool (*init_shader_fvf)(void *data, void *pass_data);
   bool (*reinit)(void *data, const void *info_data);
   bool (*init)(void *data,
         const void *video_info_data,
         void *dev_data,
         const void *final_viewport_data,
         const void *info_data,
         bool rgb32);
   void (*set_final_viewport)(void *data,
         void *renderchain_data, const void *viewport_data);
   bool (*add_pass)(void *data, const void *info_data);
   bool (*add_lut)(void *data,
         const char *id, const char *path,
         bool smooth);
   void (*add_state_tracker)(void *data, void *tracker_data);
   bool (*render)(void *chain_data, const void *data,
         unsigned width, unsigned height, unsigned pitch, unsigned rotation);
   void (*convert_geometry)(void *data, const void *info_data,
         unsigned *out_width, unsigned *out_height,
         unsigned width, unsigned height,
         void *final_viewport);
   void (*set_font_rect)(void *data, const void *param_data);
   bool (*read_viewport)(void *data, uint8_t *buffer);
   void (*viewport_info)(void *data, struct video_viewport *vp);
   const char *ident;
} renderchain_driver_t;

extern renderchain_driver_t cg_d3d9_renderchain;
extern renderchain_driver_t xdk_renderchain;
extern renderchain_driver_t null_renderchain;

bool renderchain_init_first(const renderchain_driver_t **renderchain_driver,
	void **renderchain_handle);

#ifdef __cplusplus
}
#endif

#endif

