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

#ifndef __D3D12_UPSCALER_H
#define __D3D12_UPSCALER_H

/* (the C interfaces, before any Direct3D header: in a unity build
 * everything after shares the first inclusion) */
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <d3d12.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <libretro.h>

#include "../video_defines.h"
#include "../video_upscaler.h"

RETRO_BEGIN_DECLS

/* The temporal upscaler on a Direct3D 12 device, the Vulkan one's twin
 * (vulkan_upscaler.h): the backend of the kind set (DLSS: d3d12_ngx.h;
 * FSR: d3d12_ffx.h; XeSS: d3d12_xess.h) behind one face. Retired
 * features and textures go after the frames in flight; nothing waits. */
typedef struct d3d12_upscaler d3d12_upscaler_t;

/* A frame's texture: the core's as set_texture handed it over, in
 * @state; the output in D3D12_RESOURCE_STATE_COPY_SOURCE, the state the
 * driver takes the core's own in. */
typedef struct d3d12_upscaler_image
{
   ID3D12Resource *texture;
   DXGI_FORMAT format;
   D3D12_RESOURCE_STATES state;
   unsigned dims;             /* VIDEO_SCALE_PACK */
} d3d12_upscaler_image_t;

d3d12_upscaler_t *d3d12_upscaler_init(enum video_temporal_upscaler kind,
      ID3D12Device *device, const char *dir);

/* @mode is video_upscaler_mode()'s word */
bool d3d12_upscaler_optimal(d3d12_upscaler_t *up, unsigned mode,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims);

/* Records into @cmd the upscale of @color with @inputs to @out_dims and
 * names the output in *@out; false, nothing recorded, when the frame
 * cannot be taken. Once per frame, before anything that reads *@out. */
bool d3d12_upscaler_upscale(d3d12_upscaler_t *up, ID3D12GraphicsCommandList *cmd,
      const d3d12_upscaler_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned mode, unsigned out_dims, d3d12_upscaler_image_t *out);

void d3d12_upscaler_frame_done(d3d12_upscaler_t *up);

/* The queue is drained (teardown): everything released. */
void d3d12_upscaler_free(d3d12_upscaler_t *up);

/* What the backends share: a UAV texture of their own, the transitions
 * around a dispatch, and the ring that retires a texture and feature
 * after the frames that may still read them. */

typedef struct d3d12_upscaler_output
{
   ID3D12Resource *texture;
   DXGI_FORMAT format;
   unsigned dims;             /* VIDEO_SCALE_PACK */
} d3d12_upscaler_output_t;

/* A committed UAV texture of @format and @dims, in UNORDERED_ACCESS,
 * into *@out (zeroed on failure) */
bool d3d12_upscaler_output_create(ID3D12Device *device, DXGI_FORMAT format,
      unsigned dims, d3d12_upscaler_output_t *out);
void d3d12_upscaler_output_destroy(d3d12_upscaler_output_t *out);

void d3d12_upscaler_transition(ID3D12GraphicsCommandList *cmd,
      ID3D12Resource *resource, D3D12_RESOURCE_STATES before,
      D3D12_RESOURCE_STATES after);

#define D3D12_UPSCALER_RES(img) ((ID3D12Resource*)(uintptr_t)(img)->image)

#define D3D12_UPSCALER_RETIRE_FRAMES 20
#define D3D12_UPSCALER_RETIRED_MAX   8

typedef void (*d3d12_upscaler_release_t)(void *data, void *feature);

typedef struct d3d12_upscaler_retired
{
   void *feature;
   d3d12_upscaler_output_t output;
   unsigned frames;
} d3d12_upscaler_retired_t;

typedef struct d3d12_upscaler_ring
{
   d3d12_upscaler_retired_t slot[D3D12_UPSCALER_RETIRED_MAX];
   d3d12_upscaler_release_t release;  /* frees a feature; NULL for none */
   void *release_data;
} d3d12_upscaler_ring_t;

void d3d12_upscaler_ring_retire(d3d12_upscaler_ring_t *ring,
      void *feature, d3d12_upscaler_output_t *output);
void d3d12_upscaler_ring_frame_done(d3d12_upscaler_ring_t *ring);
void d3d12_upscaler_ring_flush(d3d12_upscaler_ring_t *ring);

RETRO_END_DECLS

#endif
