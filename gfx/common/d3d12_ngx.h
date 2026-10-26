/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - Libretro team
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

#ifndef _D3D12_NGX_H
#define _D3D12_NGX_H

/* (the C interfaces, before the header is first seen: in a unity build
 * everything after shares this inclusion of it) */
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <d3d12.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <libretro.h>

#include "../video_defines.h"

RETRO_BEGIN_DECLS

/* DLSS Super Resolution on a Direct3D 12 device through NGX, the
 * Vulkan session's twin (vulkan_ngx.h): one session per device, a
 * feature per output size and mode, an output texture the driver reads
 * in the core's place. Retired features and textures go after the
 * frames in flight; nothing waits. */
typedef struct d3d12_ngx d3d12_ngx_t;

/* A frame's texture: the core's as set_texture handed it over, in
 * @state; the output in D3D12_RESOURCE_STATE_COPY_SOURCE, the state the
 * driver takes the core's own in. */
typedef struct d3d12_ngx_image
{
   ID3D12Resource *texture;
   DXGI_FORMAT format;
   D3D12_RESOURCE_STATES state;
   unsigned dims;             /* VIDEO_SCALE_PACK */
} d3d12_ngx_image_t;

d3d12_ngx_t *d3d12_ngx_init(ID3D12Device *device, const char *dlss_dir);

bool d3d12_ngx_optimal(d3d12_ngx_t *ngx, unsigned quality,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims);

/* Records into @cmd the upscale of @color with @inputs to @out_dims,
 * then DLSS Neural Rendering over it where @nr (video_upscaler_nr_mode)
 * turns it on and it runs, and names the output in *@out; false,
 * nothing recorded, when the frame cannot be taken. Once per frame,
 * before anything that reads *@out. */
bool d3d12_ngx_upscale(d3d12_ngx_t *ngx, ID3D12GraphicsCommandList *cmd,
      const d3d12_ngx_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned quality, unsigned nr, unsigned out_dims,
      d3d12_ngx_image_t *out);

void d3d12_ngx_frame_done(d3d12_ngx_t *ngx);

/* The queue is drained (teardown): everything released. */
void d3d12_ngx_free(d3d12_ngx_t *ngx);

RETRO_END_DECLS

#endif
