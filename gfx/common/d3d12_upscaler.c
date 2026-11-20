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
#include <retro_atomic.h>
#include <gfx/nvidia/ngx_loader.h>

#include "../../verbosity.h"
#include "../../configuration.h"
#include "dxgi_common.h"
#include "d3d12_upscaler.h"
#include "d3d12_ngx.h"
#include "d3d12_ffx.h"
#include "d3d12_xess.h"

typedef struct d3d12_upscaler_setup
{
   struct d3d12_upscaler_setup *next;
   ID3D12CommandAllocator *allocator;
   ID3D12GraphicsCommandList *cmd;
   unsigned frames;
} d3d12_upscaler_setup_t;

struct d3d12_upscaler
{
   enum video_temporal_upscaler kind;
   d3d12_ngx_t *ngx;
   d3d12_ffx_t *ffx;
   d3d12_xess_t *xess;
   video_upscaler_async_t *async;
   ID3D12Device *device;
   char *dir;
   char *sr_dir;
   ID3D12Fence *reload_fence;
   HANDLE reload_event;

   d3d12_upscaler_setup_t *setup_ready;
   d3d12_upscaler_setup_t *setup_submit;
   d3d12_upscaler_setup_t *setup_retired;
   unsigned cache_mode, cache_out, cache_render, cache_min, cache_max;
   bool cache_valid;
   retro_atomic_int_t frame_started;
   bool nr_loaded, nr_reload, nr_signalled, nr_queued;
};

static void d3d12_upscaler_setup_free(d3d12_upscaler_t *up, d3d12_upscaler_setup_t *item)
{
   (void)up;
   if (item->cmd)
      item->cmd->lpVtbl->Release(item->cmd);
   if (item->allocator)
      item->allocator->lpVtbl->Release(item->allocator);
   free(item);
}

static void d3d12_upscaler_setup_clear(d3d12_upscaler_t *up)
{
   d3d12_upscaler_setup_t *item;
   if (up->setup_ready)
      d3d12_upscaler_setup_free(up, up->setup_ready);
   if (up->setup_submit)
      d3d12_upscaler_setup_free(up, up->setup_submit);
   up->setup_ready = up->setup_submit = NULL;
   while ((item = up->setup_retired))
   {
      up->setup_retired = item->next;
      d3d12_upscaler_setup_free(up, item);
   }
}

static void d3d12_upscaler_async_prepare(void *data)
{
   d3d12_upscaler_t *up = (d3d12_upscaler_t*)data;
   d3d12_upscaler_setup_t *item = (d3d12_upscaler_setup_t*)calloc(1, sizeof(*item));
   if (!item)
      goto fail;
   if (FAILED(up->device->lpVtbl->CreateCommandAllocator(up->device,
               D3D12_COMMAND_LIST_TYPE_DIRECT, uuidof(ID3D12CommandAllocator),
               (void**)&item->allocator)))
      goto fail;
   if (FAILED(up->device->lpVtbl->CreateCommandList(up->device, 0,
               D3D12_COMMAND_LIST_TYPE_DIRECT, item->allocator, NULL,
               uuidof(ID3D12GraphicsCommandList), (void**)&item->cmd)))
      goto fail;
   d3d12_ngx_prepare(up->ngx, item->cmd);
   if (FAILED(item->cmd->lpVtbl->Close(item->cmd)))
      goto fail;
   up->setup_ready = item;
   return;
fail:
   video_upscaler_async_fail(up->async);
   d3d12_ngx_cancel_prepare(up->ngx);
   if (item)
      d3d12_upscaler_setup_free(up, item);
}

static bool d3d12_upscaler_async_init(void *data)
{
   d3d12_upscaler_t *up = (d3d12_upscaler_t*)data;
   const char *path = up->dir;
   if (!up->reload_fence
         && FAILED(up->device->lpVtbl->CreateFence(up->device, 0, D3D12_FENCE_FLAG_NONE,
               uuidof(ID3D12Fence), (void**)&up->reload_fence)))
      return false;
   if (!up->reload_event)
      up->reload_event = CreateEventW(NULL, FALSE, FALSE, NULL);
   if (!up->reload_event)
      return false;
   if (!up->nr_loaded)
   {
      if (!up->sr_dir)
         up->sr_dir = ngx_lib_sr_runtime(up->dir);
      if (!up->sr_dir)
         return false;
      path = up->sr_dir;
   }
   up->ngx = d3d12_ngx_init(up->device, path);
   d3d12_ngx_defer(up->ngx);
   if (up->ngx && up->nr_loaded)
      ngx_lib_keep_feature(up->dir, NGX_FEATURE_DLSS_NR);
   return up->ngx != NULL;
}

static void d3d12_upscaler_async_reload_nr(void *data)
{
   d3d12_upscaler_t *up = (d3d12_upscaler_t*)data;
   const char *path = up->dir;
   if (FAILED(up->reload_fence->lpVtbl->SetEventOnCompletion(up->reload_fence,
               1, up->reload_event)) || WaitForSingleObject(up->reload_event, INFINITE) != WAIT_OBJECT_0)
   {
      video_upscaler_async_fail(up->async);
      return;
   }
   d3d12_ngx_free(up->ngx);
   d3d12_upscaler_setup_clear(up);
   up->ngx = d3d12_ngx_init(up->device, path);
   d3d12_ngx_defer(up->ngx);
   if (!up->ngx)
      video_upscaler_async_fail(up->async);
   else
      ngx_lib_keep_feature(up->dir, NGX_FEATURE_DLSS_NR);
   up->nr_loaded = true;
}

static void d3d12_upscaler_async_destroy(void *data)
{
   d3d12_upscaler_t *up = (d3d12_upscaler_t*)data;
   d3d12_ngx_free(up->ngx);
   d3d12_upscaler_setup_clear(up);
   if (up->reload_fence)
      up->reload_fence->lpVtbl->Release(up->reload_fence);
   if (up->reload_event)
      CloseHandle(up->reload_event);
   up->device->lpVtbl->Release(up->device);
   free(up->sr_dir);
   free(up->dir);
   free(up);
}

d3d12_upscaler_t *d3d12_upscaler_init_async(enum video_temporal_upscaler kind,
      ID3D12Device *device, const char *dir)
{
#ifdef HAVE_THREADS
   d3d12_upscaler_t *up;
   if (kind != VIDEO_TEMPORAL_UPSCALER_DLSS)
      return d3d12_upscaler_init(kind, device, dir);
   /* Resolve the process-wide dispatch before the worker can use it. */
   if (!ngx_lib_get())
      return NULL;
   up = (d3d12_upscaler_t*)calloc(1, sizeof(*up));
   if (!up)
      return NULL;
   up->kind   = kind;
   up->device = device;
   up->dir    = strdup(dir ? dir : "");
   up->nr_loaded = config_get_ptr()->bools.video_dlss_nr_enable || ngx_lib_is_optiscaler();
   if (up->nr_loaded)
      video_upscaler_retire_cached();
   if (up->dir)
      up->async = video_upscaler_async_new(up, d3d12_upscaler_async_init,
            NULL, d3d12_upscaler_async_destroy);
   if (!up->async)
   {
      free(up->dir);
      free(up);
      return NULL;
   }
   device->lpVtbl->AddRef(device);
   return up;
#else
   return d3d12_upscaler_init(kind, device, dir);
#endif
}

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
         if (video_upscaler_async_busy())
         {
            free(up);
            return NULL;
         }
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

static bool d3d12_upscaler_optimal_impl(d3d12_upscaler_t *up, unsigned mode,
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

bool d3d12_upscaler_optimal(d3d12_upscaler_t *up, unsigned mode,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims)
{
   bool ok;
   if (!up)
      return false;
   if (up->async && !retro_atomic_load_acquire_int(&up->frame_started))
      return false;
   /* This snapshot belongs to the core caller and stays valid while
    * the worker prepares the feature for its requested mode and size. */
   if (up->async && up->cache_valid && up->cache_mode == mode && up->cache_out == out_dims)
   {
      *render_dims = up->cache_render;
      *min_dims    = up->cache_min;
      *max_dims    = up->cache_max;
      return true;
   }
   if (up->async && !video_upscaler_async_enter(up->async))
      return false;

   ok = d3d12_upscaler_optimal_impl(up, mode, out_dims, render_dims, min_dims, max_dims);
   if (up->async)
   {
      if (ok)
      {
         up->cache_mode   = mode;
         up->cache_out    = out_dims;
         up->cache_render = *render_dims;
         up->cache_min    = *min_dims;
         up->cache_max    = *max_dims;
         up->cache_valid  = true;
      }
      video_upscaler_async_leave();
   }
   return ok;
}

static bool d3d12_upscaler_upscale_impl(d3d12_upscaler_t *up, ID3D12GraphicsCommandList *cmd,
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

bool d3d12_upscaler_upscale(d3d12_upscaler_t *up, ID3D12GraphicsCommandList * cmd,
      const d3d12_upscaler_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned mode, unsigned out_dims, d3d12_upscaler_image_t *out)
{
   bool ok;
   if (!up || (up->async && (!retro_atomic_load_acquire_int(&up->frame_started)
               || !video_upscaler_async_enter(up->async))))
      return false;
   if (up->async && (!up->nr_loaded && video_upscaler_nr_mode()))
   {
      if (!up->nr_reload)
         video_upscaler_retire_cached();
      up->nr_reload = true;
   }
   if (up->async && up->nr_reload && !up->nr_loaded)
   {
      video_upscaler_async_leave();
      return false;
   }
   if (up->async && up->setup_ready)
   {
      up->setup_submit = up->setup_ready;
      up->setup_ready  = NULL;
   }
   ok = d3d12_upscaler_upscale_impl(up, cmd, color, inputs, mode, out_dims, out);
   if (up->async)
   {
      if (d3d12_ngx_prepare_pending(up->ngx))
         video_upscaler_async_work(up->async, d3d12_upscaler_async_prepare);
      video_upscaler_async_leave();
   }
   return ok;
}

ID3D12GraphicsCommandList * d3d12_upscaler_take_commands(d3d12_upscaler_t *up)
{
   d3d12_upscaler_setup_t *item;
   if (!up || !(item = up->setup_submit))
      return NULL;
   up->setup_submit  = NULL;
   item->frames      = D3D12_UPSCALER_RETIRE_FRAMES;
   item->next        = up->setup_retired;
   up->setup_retired = item;
   return item->cmd;
}

void d3d12_upscaler_signal_reload(d3d12_upscaler_t *up, ID3D12CommandQueue *queue)
{
   if (!up || !up->nr_reload || up->nr_signalled)
      return;
   if (SUCCEEDED(queue->lpVtbl->Signal(queue, up->reload_fence, 1)))
      up->nr_signalled = true;
   else
      video_upscaler_async_fail(up->async);
}

void d3d12_upscaler_frame_done(d3d12_upscaler_t *up)
{
   if (!up)
      return;
   if (up->async)
   {
      d3d12_upscaler_setup_t **link;
      retro_atomic_store_release_int(&up->frame_started, 1);
      if (up->nr_signalled && !up->nr_queued)
      {
         up->nr_queued = video_upscaler_async_work(up->async, d3d12_upscaler_async_reload_nr);
      }
      video_upscaler_async_start(up->async);
      if (!video_upscaler_async_enter(up->async))
         return;
      for (link = &up->setup_retired; *link;)
      {
         d3d12_upscaler_setup_t *item = *link;
         if (--item->frames)
            link = &item->next;
         else
         {
            *link = item->next;
            d3d12_upscaler_setup_free(up, item);
         }
      }
   }
   d3d12_ngx_frame_done(up->ngx);
   d3d12_ffx_frame_done(up->ffx);
   d3d12_xess_frame_done(up->xess);
   if (up->async)
      video_upscaler_async_leave();
}

void d3d12_upscaler_free(d3d12_upscaler_t *up)
{
   if (!up)
      return;
   if (up->async)
   {
      video_upscaler_async_free(up->async);
      return;
   }
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
