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

#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <stdlib.h>
#include <string.h>

#include "../../verbosity.h"
#include "dxgi_common.h"
#include "d3d12_upscaler.h"
#include "d3d12_ngx.h"
#include "d3d12_ffx.h"
#include "d3d12_xess.h"

struct d3d12_upscaler
{
   enum video_temporal_upscaler kind;
   d3d12_ngx_t *ngx;
   d3d12_ffx_t *ffx;
   d3d12_xess_t *xess;
};

d3d12_upscaler_t *d3d12_upscaler_init(enum video_temporal_upscaler kind,
      ID3D12Device *device, const char *dir)
{
   d3d12_upscaler_t *up = (d3d12_upscaler_t*)calloc(1, sizeof(*up));
   if (!up)
      return NULL;
   up->kind = kind;
   switch (kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
         up->ngx = d3d12_ngx_init(device, dir);
         break;
      case VIDEO_TEMPORAL_UPSCALER_FSR:
         up->ffx = d3d12_ffx_init(device, dir);
         break;
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         up->xess = d3d12_xess_init(device, dir);
         break;
      default:
         break;
   }
   if (!up->ngx && !up->ffx && !up->xess)
   {
      free(up);
      return NULL;
   }
   return up;
}

bool d3d12_upscaler_optimal(d3d12_upscaler_t *up, unsigned mode,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims)
{
   if (!up)
      return false;
   switch (up->kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
         return d3d12_ngx_optimal(up->ngx, video_upscaler_mode_ngx(mode),
               out_dims, render_dims, min_dims, max_dims);
      case VIDEO_TEMPORAL_UPSCALER_FSR:
         return d3d12_ffx_optimal(up->ffx, video_upscaler_mode_ffx(mode),
               out_dims, render_dims, min_dims, max_dims);
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         return d3d12_xess_optimal(up->xess, video_upscaler_mode_xess(mode),
               out_dims, render_dims, min_dims, max_dims);
      default:
         break;
   }
   return false;
}

bool d3d12_upscaler_upscale(d3d12_upscaler_t *up, ID3D12GraphicsCommandList *cmd,
      const d3d12_upscaler_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned mode, unsigned out_dims, d3d12_upscaler_image_t *out)
{
   if (!up)
      return false;
   switch (up->kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
      {
         d3d12_ngx_image_t in, res;
         in.texture = color->texture;
         in.format  = color->format;
         in.state   = color->state;
         in.dims    = color->dims;
         if (!d3d12_ngx_upscale(up->ngx, cmd, &in, inputs,
                  video_upscaler_mode_ngx(mode), video_upscaler_nr_mode(),
                  out_dims, &res))
            return false;
         out->texture = res.texture;
         out->format  = res.format;
         out->state   = res.state;
         out->dims    = res.dims;
         return true;
      }
      case VIDEO_TEMPORAL_UPSCALER_FSR:
         return d3d12_ffx_upscale(up->ffx, cmd, color, inputs,
               video_upscaler_mode_ffx(mode), out_dims, out);
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         return d3d12_xess_upscale(up->xess, cmd, color, inputs,
               video_upscaler_mode_xess(mode), out_dims, out);
      default:
         break;
   }
   return false;
}

void d3d12_upscaler_frame_done(d3d12_upscaler_t *up)
{
   if (!up)
      return;
   d3d12_ngx_frame_done(up->ngx);
   d3d12_ffx_frame_done(up->ffx);
   d3d12_xess_frame_done(up->xess);
}

void d3d12_upscaler_free(d3d12_upscaler_t *up)
{
   if (!up)
      return;
   d3d12_ngx_free(up->ngx);
   d3d12_ffx_free(up->ffx);
   d3d12_xess_free(up->xess);
   free(up);
}

/* ---- What the backends share ---- */

void d3d12_upscaler_transition(ID3D12GraphicsCommandList *cmd,
      ID3D12Resource *resource, D3D12_RESOURCE_STATES before,
      D3D12_RESOURCE_STATES after)
{
   D3D12_RESOURCE_BARRIER barrier;
   if (before == after)
      return;
   barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
   barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
   barrier.Transition.pResource   = resource;
   barrier.Transition.StateBefore = before;
   barrier.Transition.StateAfter  = after;
   barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
   cmd->lpVtbl->ResourceBarrier(cmd, 1, &barrier);
}

bool d3d12_upscaler_output_create(ID3D12Device *device, DXGI_FORMAT format,
      unsigned dims, d3d12_upscaler_output_t *out)
{
   D3D12_HEAP_PROPERTIES heap;
   D3D12_RESOURCE_DESC desc;
   memset(out, 0, sizeof(*out));
   heap.Type                 = D3D12_HEAP_TYPE_DEFAULT;
   heap.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
   heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
   heap.CreationNodeMask     = 1;
   heap.VisibleNodeMask      = 1;
   desc.Dimension            = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   desc.Alignment            = 0;
   desc.Width                = VIDEO_SCALE_W(dims);
   desc.Height               = VIDEO_SCALE_H(dims);
   desc.DepthOrArraySize     = 1;
   desc.MipLevels            = 1;
   desc.Format               = format;
   desc.SampleDesc.Count     = 1;
   desc.SampleDesc.Quality   = 0;
   desc.Layout               = D3D12_TEXTURE_LAYOUT_UNKNOWN;
   desc.Flags                = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
   if (FAILED(device->lpVtbl->CreateCommittedResource(device,
               &heap, D3D12_HEAP_FLAG_NONE, &desc,
               D3D12_RESOURCE_STATE_UNORDERED_ACCESS, NULL,
               uuidof(ID3D12Resource), (void**)&out->texture)))
   {
      out->texture = NULL;
      return false;
   }
   out->format = format;
   out->dims   = dims;
   return true;
}

void d3d12_upscaler_output_destroy(d3d12_upscaler_output_t *out)
{
   if (out->texture)
      out->texture->lpVtbl->Release(out->texture);
   memset(out, 0, sizeof(*out));
}

static void d3d12_upscaler_ring_free_slot(d3d12_upscaler_ring_t *ring,
      d3d12_upscaler_retired_t *slot)
{
   if (slot->feature && ring->release)
      ring->release(ring->release_data, slot->feature);
   d3d12_upscaler_output_destroy(&slot->output);
   memset(slot, 0, sizeof(*slot));
}

void d3d12_upscaler_ring_retire(d3d12_upscaler_ring_t *ring,
      void *feature, d3d12_upscaler_output_t *output)
{
   unsigned i;
   d3d12_upscaler_retired_t *slot = NULL;
   if (!feature && !output->texture)
      return;
   for (i = 0; i < D3D12_UPSCALER_RETIRED_MAX; i++)
      if (!ring->slot[i].frames)
      {
         slot = &ring->slot[i];
         break;
      }
   if (!slot)
   {
      slot = &ring->slot[0];
      for (i = 1; i < D3D12_UPSCALER_RETIRED_MAX; i++)
         if (ring->slot[i].frames < slot->frames)
            slot = &ring->slot[i];
      d3d12_upscaler_ring_free_slot(ring, slot);
   }
   slot->feature = feature;
   slot->output  = *output;
   slot->frames  = D3D12_UPSCALER_RETIRE_FRAMES;
   memset(output, 0, sizeof(*output));
}

void d3d12_upscaler_ring_frame_done(d3d12_upscaler_ring_t *ring)
{
   unsigned i;
   for (i = 0; i < D3D12_UPSCALER_RETIRED_MAX; i++)
   {
      d3d12_upscaler_retired_t *r = &ring->slot[i];
      if (!r->frames || --r->frames)
         continue;
      d3d12_upscaler_ring_free_slot(ring, r);
   }
}

void d3d12_upscaler_ring_flush(d3d12_upscaler_ring_t *ring)
{
   unsigned i;
   for (i = 0; i < D3D12_UPSCALER_RETIRED_MAX; i++)
      if (ring->slot[i].frames)
         d3d12_upscaler_ring_free_slot(ring, &ring->slot[i]);
}
