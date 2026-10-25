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

/* (the C interfaces, before any Direct3D header: in a unity build
 * everything after shares the first inclusion) */
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <stdlib.h>
#include <string.h>

#include <retro_miscellaneous.h>
#include <gfx/nvidia/ngx_loader.h>

#include "../../verbosity.h"
#include "dxgi_common.h"
#include "d3d12_ngx.h"

#define D3D12_NGX_PROJECT_ID     "3fe3a1ab-9a3c-4b57-a9d7-26c5a2a3b0d1"
#define D3D12_NGX_ENGINE_VERSION "1.0"
#define D3D12_NGX_RETIRE_FRAMES  20
#define D3D12_NGX_RETIRED_MAX    8

typedef struct d3d12_ngx_retired
{
   ngx_handle_t *feature;
   ID3D12Resource *texture;
   unsigned frames;
} d3d12_ngx_retired_t;

struct d3d12_ngx
{
   const ngx_lib_t *lib;
   ngx_parameter_t *caps;
   ngx_parameter_t *params;
   ngx_handle_t *feature;
   ID3D12Device *device;
   /* What Init was given, alive until after Shutdown: NGX keeps
    * reading it */
   wchar_t *wdir;
   const wchar_t *paths[1];
   ngx_feature_common_info_t info;
   ID3D12Resource *out_texture;
   d3d12_ngx_retired_t retired[D3D12_NGX_RETIRED_MAX];
   DXGI_FORMAT out_format;
   /* What the feature was made for; sizes are VIDEO_SCALE_PACK */
   unsigned feature_out_dims;
   unsigned feature_max_dims;
   unsigned feature_flags;
   unsigned feature_quality;
   /* The last optimal query */
   unsigned opt_quality;
   unsigned opt_out_dims;
   unsigned opt_render_dims;
   unsigned opt_min_dims;
   unsigned opt_max_dims;
   /* What failed last: not tried again until the output or mode changes */
   unsigned failed_out_dims;
   unsigned failed_quality;
   bool opt_valid;
   bool reset_pending;
};

static void d3d12_ngx_transition(ID3D12GraphicsCommandList *cmd,
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

d3d12_ngx_t *d3d12_ngx_init(ID3D12Device *device, const char *dlss_dir)
{
   d3d12_ngx_t *ngx;
   ngx_result_t res;
   int available = 0;
   int in_use = 0;
   const ngx_lib_t *lib;

   if (!ngx_dlss_feature_in(dlss_dir ? dlss_dir : ""))
   {
      RARCH_LOG("[D3D12] Temporal Upscaler: no DLSS runtime in \"%s\", upscaler off.\n",
            dlss_dir ? dlss_dir : "");
      return NULL;
   }
   lib = ngx_lib_get();
   if (!lib || !lib->d3d12.init_project_id)
   {
      RARCH_LOG("[D3D12] Temporal Upscaler: no NGX core library.\n");
      return NULL;
   }
   if (!(ngx = (d3d12_ngx_t*)calloc(1, sizeof(*ngx))))
      return NULL;
   if (!(ngx->wdir = (wchar_t*)calloc(DIR_MAX_LENGTH, sizeof(wchar_t))))
   {
      free(ngx);
      return NULL;
   }
   ngx_utf8_to_wide(ngx->wdir, DIR_MAX_LENGTH, dlss_dir ? dlss_dir : "");
   ngx->paths[0]              = ngx->wdir;
   ngx->info.path_list.path   = ngx->paths;
   ngx->info.path_list.length = 1;
   res = lib->d3d12.init_project_id(D3D12_NGX_PROJECT_ID,
         NGX_ENGINE_TYPE_CUSTOM, D3D12_NGX_ENGINE_VERSION, ngx->wdir,
         (void*)device, NGX_VERSION_API, &ngx->info);
   if (NGX_FAILED(res))
   {
      RARCH_WARN("[D3D12] Temporal Upscaler: NGX init failed (0x%08x).\n",
            (unsigned)res);
      free(ngx->wdir);
      free(ngx);
      return NULL;
   }
   ngx->lib    = lib;
   ngx->device = device;
   if (   NGX_FAILED(lib->d3d12.get_capability_parameters(&ngx->caps))
       || NGX_FAILED(lib->param.get_i(ngx->caps,
             NGX_PARAM_SUPER_SAMPLING_AVAILABLE, &available))
       || !available)
   {
      int needs_driver = 0, major = 0, minor = 0, init_result = 0;
      if (ngx->caps)
      {
         lib->param.get_i(ngx->caps, NGX_PARAM_SUPER_SAMPLING_NEEDS_UPDATED_DRIVER, &needs_driver);
         lib->param.get_i(ngx->caps, NGX_PARAM_SUPER_SAMPLING_MIN_DRIVER_MAJOR, &major);
         lib->param.get_i(ngx->caps, NGX_PARAM_SUPER_SAMPLING_MIN_DRIVER_MINOR, &minor);
         lib->param.get_i(ngx->caps, NGX_PARAM_SUPER_SAMPLING_FEATURE_INIT_RESULT, &init_result);
      }
      if (needs_driver)
         RARCH_WARN("[D3D12] Temporal Upscaler: DLSS needs driver %d.%d or newer.\n",
               major, minor);
      else
         RARCH_WARN("[D3D12] Temporal Upscaler: DLSS Super Resolution not available on this device (0x%08x).\n",
               (unsigned)init_result);
      lib->d3d12.shutdown1((void*)device, &in_use);
      free(ngx->wdir);
      free(ngx);
      return NULL;
   }
   if (NGX_FAILED(lib->d3d12.allocate_parameters(&ngx->params)))
   {
      lib->d3d12.shutdown1((void*)device, &in_use);
      free(ngx->wdir);
      free(ngx);
      return NULL;
   }
   RARCH_LOG("[D3D12] Temporal Upscaler: DLSS Super Resolution ready.\n");
   return ngx;
}

/* The mode's name for the log: the quality, and the preset when one is asked for */
static const char *ngx_quality_name(unsigned mode)
{
   switch (NGX_MODE_QUALITY(mode))
   {
      case NGX_PERF_QUALITY_MAX_PERF:          return "Performance";
      case NGX_PERF_QUALITY_BALANCED:          return "Balanced";
      case NGX_PERF_QUALITY_MAX_QUALITY:       return "Quality";
      case NGX_PERF_QUALITY_ULTRA_PERFORMANCE: return "Ultra Performance";
      case NGX_PERF_QUALITY_ULTRA_QUALITY:     return "Ultra Quality";
      case NGX_PERF_QUALITY_DLAA:              return "DLAA";
      default:                                 break;
   }
   return "?";
}

static const char *ngx_preset_name(unsigned mode)
{
   switch (NGX_MODE_PRESET(mode))
   {
      case NGX_DLSS_PRESET_J: return ", preset J";
      case NGX_DLSS_PRESET_K: return ", preset K";
      default:                break;
   }
   return "";
}

/* The render preset hints, one a quality: the one asked for, or the library's default */
static void ngx_set_preset(const ngx_lib_t *lib, ngx_parameter_t *p, unsigned mode)
{
   const unsigned preset = NGX_MODE_PRESET(mode);
   lib->param.set_ui(p, NGX_PARAM_DLSS_PRESET_DLAA, preset);
   lib->param.set_ui(p, NGX_PARAM_DLSS_PRESET_QUALITY, preset);
   lib->param.set_ui(p, NGX_PARAM_DLSS_PRESET_BALANCED, preset);
   lib->param.set_ui(p, NGX_PARAM_DLSS_PRESET_PERFORMANCE, preset);
   lib->param.set_ui(p, NGX_PARAM_DLSS_PRESET_ULTRA_PERFORMANCE, preset);
   lib->param.set_ui(p, NGX_PARAM_DLSS_PRESET_ULTRA_QUALITY, preset);
}

bool d3d12_ngx_optimal(d3d12_ngx_t *ngx, unsigned quality,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims)
{
   const ngx_lib_t *lib;
   void *callback = NULL;
   ngx_result_t (NGX_CONV *optimal)(ngx_parameter_t *p);
   if (!ngx || !VIDEO_SCALE_W(out_dims) || !VIDEO_SCALE_H(out_dims))
      return false;
   lib = ngx->lib;
   if (!ngx->opt_valid || ngx->opt_quality != NGX_MODE_QUALITY(quality)
         || ngx->opt_out_dims != out_dims)
   {
      ngx_parameter_t *p = ngx->caps;
      unsigned rw = 0, rh = 0, mnw, mnh, mxw, mxh;
      ngx->opt_valid = false;
      lib->param.get_void_pointer(p, NGX_PARAM_DLSS_OPTIMAL_SETTINGS_CALLBACK,
            &callback);
      if (!callback)
         return false;
      memcpy(&optimal, &callback, sizeof(optimal));
      lib->param.set_ui(p, NGX_PARAM_WIDTH, VIDEO_SCALE_W(out_dims));
      lib->param.set_ui(p, NGX_PARAM_HEIGHT, VIDEO_SCALE_H(out_dims));
      lib->param.set_i(p, NGX_PARAM_PERF_QUALITY, (int)NGX_MODE_QUALITY(quality));
      lib->param.set_i(p, NGX_PARAM_RTX_VALUE, 0);
      if (NGX_FAILED(optimal(p)))
         return false;
      lib->param.get_ui(p, NGX_PARAM_OUT_WIDTH, &rw);
      lib->param.get_ui(p, NGX_PARAM_OUT_HEIGHT, &rh);
      if (!rw || !rh)
         return false;
      mxw = mnw = rw;
      mxh = mnh = rh;
      lib->param.get_ui(p, NGX_PARAM_DLSS_DYNAMIC_MAX_RENDER_WIDTH, &mxw);
      lib->param.get_ui(p, NGX_PARAM_DLSS_DYNAMIC_MAX_RENDER_HEIGHT, &mxh);
      lib->param.get_ui(p, NGX_PARAM_DLSS_DYNAMIC_MIN_RENDER_WIDTH, &mnw);
      lib->param.get_ui(p, NGX_PARAM_DLSS_DYNAMIC_MIN_RENDER_HEIGHT, &mnh);
      ngx->opt_render_dims = VIDEO_SCALE_PACK(rw, rh);
      ngx->opt_min_dims    = VIDEO_SCALE_PACK(mnw, mnh);
      ngx->opt_max_dims    = VIDEO_SCALE_PACK(mxw, mxh);
      ngx->opt_quality     = NGX_MODE_QUALITY(quality);
      ngx->opt_out_dims    = out_dims;
      ngx->opt_valid       = true;
   }
   *render_dims = ngx->opt_render_dims;
   *min_dims    = ngx->opt_min_dims;
   *max_dims    = ngx->opt_max_dims;
   return true;
}

static void d3d12_ngx_retire(d3d12_ngx_t *ngx)
{
   unsigned i;
   d3d12_ngx_retired_t *slot = NULL;
   if (!ngx->feature && !ngx->out_texture)
      return;
   for (i = 0; i < D3D12_NGX_RETIRED_MAX; i++)
      if (!ngx->retired[i].frames)
      {
         slot = &ngx->retired[i];
         break;
      }
   if (!slot)
   {
      slot = &ngx->retired[0];
      for (i = 1; i < D3D12_NGX_RETIRED_MAX; i++)
         if (ngx->retired[i].frames < slot->frames)
            slot = &ngx->retired[i];
      if (slot->feature)
         ngx->lib->d3d12.release_feature(slot->feature);
      if (slot->texture)
         slot->texture->lpVtbl->Release(slot->texture);
   }
   slot->feature    = ngx->feature;
   slot->texture    = ngx->out_texture;
   slot->frames     = D3D12_NGX_RETIRE_FRAMES;
   ngx->feature     = NULL;
   ngx->out_texture = NULL;
}

void d3d12_ngx_frame_done(d3d12_ngx_t *ngx)
{
   unsigned i;
   int in_use = 0;
   if (!ngx)
      return;
   for (i = 0; i < D3D12_NGX_RETIRED_MAX; i++)
   {
      d3d12_ngx_retired_t *r = &ngx->retired[i];
      if (!r->frames || --r->frames)
         continue;
      if (r->feature)
         ngx->lib->d3d12.release_feature(r->feature);
      if (r->texture)
         r->texture->lpVtbl->Release(r->texture);
      memset(r, 0, sizeof(*r));
   }
}

static bool d3d12_ngx_create_output(d3d12_ngx_t *ngx, DXGI_FORMAT format,
      unsigned dims)
{
   D3D12_HEAP_PROPERTIES heap;
   D3D12_RESOURCE_DESC desc;
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
   if (FAILED(ngx->device->lpVtbl->CreateCommittedResource(ngx->device,
               &heap, D3D12_HEAP_FLAG_NONE, &desc,
               D3D12_RESOURCE_STATE_UNORDERED_ACCESS, NULL,
               uuidof(ID3D12Resource), (void**)&ngx->out_texture)))
   {
      ngx->out_texture = NULL;
      return false;
   }
   ngx->out_format = format;
   return true;
}

static bool d3d12_ngx_create_feature(d3d12_ngx_t *ngx,
      ID3D12GraphicsCommandList *cmd, DXGI_FORMAT format, unsigned quality,
      unsigned flags, unsigned out_dims, unsigned max_dims)
{
   const ngx_lib_t *lib = ngx->lib;
   ngx_parameter_t *p   = ngx->params;
   ngx_result_t res;

   d3d12_ngx_retire(ngx);
   if (!d3d12_ngx_create_output(ngx, format, out_dims))
   {
      RARCH_ERR("[D3D12] Temporal Upscaler: no output texture of %ux%u.\n",
            VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims));
      return false;
   }
   lib->param.set_ui(p, NGX_PARAM_CREATION_NODE_MASK, 1);
   lib->param.set_ui(p, NGX_PARAM_VISIBILITY_NODE_MASK, 1);
   lib->param.set_ui(p, NGX_PARAM_WIDTH, VIDEO_SCALE_W(max_dims));
   lib->param.set_ui(p, NGX_PARAM_HEIGHT, VIDEO_SCALE_H(max_dims));
   lib->param.set_ui(p, NGX_PARAM_OUT_WIDTH, VIDEO_SCALE_W(out_dims));
   lib->param.set_ui(p, NGX_PARAM_OUT_HEIGHT, VIDEO_SCALE_H(out_dims));
   lib->param.set_i(p, NGX_PARAM_PERF_QUALITY, (int)NGX_MODE_QUALITY(quality));
   ngx_set_preset(lib, p, quality);
   lib->param.set_i(p, NGX_PARAM_DLSS_CREATE_FLAGS, (int)flags);
   lib->param.set_i(p, NGX_PARAM_DLSS_ENABLE_OUTPUT_SUBRECTS, 0);
   res = lib->d3d12.create_feature((void*)cmd, NGX_FEATURE_SUPER_SAMPLING,
         p, &ngx->feature);
   if (NGX_FAILED(res))
   {
      RARCH_ERR("[D3D12] Temporal Upscaler: DLSS feature %ux%u -> %ux%u failed (0x%08x).\n",
            VIDEO_SCALE_W(max_dims), VIDEO_SCALE_H(max_dims),
            VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims), (unsigned)res);
      ngx->feature = NULL;
      d3d12_ngx_retire(ngx);
      ngx->failed_out_dims = out_dims;
      ngx->failed_quality  = quality;
      return false;
   }
   ngx->feature_out_dims = out_dims;
   ngx->feature_max_dims = max_dims;
   ngx->feature_flags    = flags;
   ngx->feature_quality  = quality;
   ngx->reset_pending    = true;
   RARCH_LOG("[D3D12] Temporal Upscaler: DLSS feature up to %ux%u -> %ux%u (%s%s), flags 0x%x.\n",
         VIDEO_SCALE_W(max_dims), VIDEO_SCALE_H(max_dims),
         VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims),
         ngx_quality_name(quality), ngx_preset_name(quality), flags);
   return true;
}

#define D3D12_NGX_RES(img) ((ID3D12Resource*)(uintptr_t)(img)->image)

bool d3d12_ngx_upscale(d3d12_ngx_t *ngx, ID3D12GraphicsCommandList *cmd,
      const d3d12_ngx_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned quality, unsigned out_dims, d3d12_ngx_image_t *out)
{
   const ngx_lib_t *lib;
   ngx_parameter_t *p;
   unsigned render_dims, min_dims, max_dims;
   unsigned flags;
   ngx_result_t res;
   const D3D12_RESOURCE_STATES read = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

   if (!ngx || !inputs || !color->texture || !inputs->motion_vectors.image
         || !inputs->depth.image
         || VIDEO_SCALE_W(out_dims) < 32 || VIDEO_SCALE_H(out_dims) < 32)
      return false;
   if (ngx->failed_out_dims == out_dims && ngx->failed_quality == quality)
      return false;
   lib = ngx->lib;
   p   = ngx->params;
   if (!d3d12_ngx_optimal(ngx, quality, out_dims, &render_dims, &min_dims,
            &max_dims))
      return false;
   if (     VIDEO_SCALE_W(color->dims) < VIDEO_SCALE_W(min_dims)
         || VIDEO_SCALE_W(color->dims) > VIDEO_SCALE_W(max_dims)
         || VIDEO_SCALE_H(color->dims) < VIDEO_SCALE_H(min_dims)
         || VIDEO_SCALE_H(color->dims) > VIDEO_SCALE_H(max_dims))
      return false;

   flags = 0;
   if (!(inputs->flags & RETRO_HW_UPSCALER_MV_OUTPUT_RES))
      flags |= NGX_DLSS_FLAG_MV_LOW_RES;
   if (inputs->flags & RETRO_HW_UPSCALER_MV_JITTERED)
      flags |= NGX_DLSS_FLAG_MV_JITTERED;
   if (inputs->flags & RETRO_HW_UPSCALER_DEPTH_INVERTED)
      flags |= NGX_DLSS_FLAG_DEPTH_INVERTED;
   if (inputs->flags & RETRO_HW_UPSCALER_HDR)
      flags |= NGX_DLSS_FLAG_IS_HDR;
   if (!inputs->exposure.image)
      flags |= NGX_DLSS_FLAG_AUTO_EXPOSURE;

   if (   !ngx->feature
       || ngx->feature_out_dims != out_dims
       || ngx->feature_max_dims != max_dims
       || ngx->feature_flags != flags || ngx->feature_quality != quality
       || ngx->out_format != color->format)
   {
      if (!d3d12_ngx_create_feature(ngx, cmd, color->format, quality,
               flags, out_dims, max_dims))
         return false;
   }

   /* Inputs readable by the compute shader; the output a UAV. The
    * output's last frame left it a copy source. */
   d3d12_ngx_transition(cmd, color->texture, color->state, read);
   d3d12_ngx_transition(cmd, D3D12_NGX_RES(&inputs->motion_vectors),
         (D3D12_RESOURCE_STATES)inputs->motion_vectors.state, read);
   d3d12_ngx_transition(cmd, D3D12_NGX_RES(&inputs->depth),
         (D3D12_RESOURCE_STATES)inputs->depth.state, read);
   if (inputs->exposure.image)
      d3d12_ngx_transition(cmd, D3D12_NGX_RES(&inputs->exposure),
            (D3D12_RESOURCE_STATES)inputs->exposure.state, read);
   if (inputs->transparency_mask.image)
      d3d12_ngx_transition(cmd, D3D12_NGX_RES(&inputs->transparency_mask),
            (D3D12_RESOURCE_STATES)inputs->transparency_mask.state, read);
   if (!ngx->reset_pending)
      d3d12_ngx_transition(cmd, ngx->out_texture,
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

   lib->param.set_d3d12_resource(p, NGX_PARAM_COLOR, color->texture);
   lib->param.set_d3d12_resource(p, NGX_PARAM_OUTPUT, ngx->out_texture);
   lib->param.set_d3d12_resource(p, NGX_PARAM_DEPTH, D3D12_NGX_RES(&inputs->depth));
   lib->param.set_d3d12_resource(p, NGX_PARAM_MOTION_VECTORS,
         D3D12_NGX_RES(&inputs->motion_vectors));
   lib->param.set_d3d12_resource(p, NGX_PARAM_EXPOSURE_TEXTURE,
         inputs->exposure.image ? D3D12_NGX_RES(&inputs->exposure) : NULL);
   lib->param.set_d3d12_resource(p, NGX_PARAM_TRANSPARENCY_MASK,
         inputs->transparency_mask.image ? D3D12_NGX_RES(&inputs->transparency_mask) : NULL);
   lib->param.set_f(p, NGX_PARAM_JITTER_OFFSET_X, inputs->jitter_x);
   lib->param.set_f(p, NGX_PARAM_JITTER_OFFSET_Y, inputs->jitter_y);
   lib->param.set_f(p, NGX_PARAM_SHARPNESS, 0.0f);
   lib->param.set_i(p, NGX_PARAM_RESET,
         (ngx->reset_pending || (inputs->flags & RETRO_HW_UPSCALER_RESET)) ? 1 : 0);
   lib->param.set_f(p, NGX_PARAM_MV_SCALE_X,
         inputs->mv_scale_x == 0.0f ? 1.0f : inputs->mv_scale_x);
   lib->param.set_f(p, NGX_PARAM_MV_SCALE_Y,
         inputs->mv_scale_y == 0.0f ? 1.0f : inputs->mv_scale_y);
   lib->param.set_f(p, NGX_PARAM_DLSS_PRE_EXPOSURE,
         inputs->pre_exposure == 0.0f ? 1.0f : inputs->pre_exposure);
   lib->param.set_f(p, NGX_PARAM_DLSS_EXPOSURE_SCALE,
         inputs->exposure_scale == 0.0f ? 1.0f : inputs->exposure_scale);
   lib->param.set_f(p, NGX_PARAM_FRAME_TIME_DELTA_MS, inputs->frame_time_ms);
   lib->param.set_ui(p, NGX_PARAM_DLSS_RENDER_SUBRECT_WIDTH, VIDEO_SCALE_W(color->dims));
   lib->param.set_ui(p, NGX_PARAM_DLSS_RENDER_SUBRECT_HEIGHT, VIDEO_SCALE_H(color->dims));
   lib->param.set_ui(p, NGX_PARAM_DLSS_INPUT_COLOR_SUBRECT_X, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_INPUT_COLOR_SUBRECT_Y, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_INPUT_DEPTH_SUBRECT_X, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_INPUT_DEPTH_SUBRECT_Y, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_INPUT_MV_SUBRECT_X, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_INPUT_MV_SUBRECT_Y, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_OUTPUT_SUBRECT_X, 0);
   lib->param.set_ui(p, NGX_PARAM_DLSS_OUTPUT_SUBRECT_Y, 0);
   lib->param.set_i(p, NGX_PARAM_DLSS_INDICATOR_INVERT_X, 0);
   lib->param.set_i(p, NGX_PARAM_DLSS_INDICATOR_INVERT_Y, 0);
   res = lib->d3d12.evaluate_feature((void*)cmd, ngx->feature, p, NULL);
   ngx->reset_pending = false;

   /* Everything back as the core left it; the output a copy source */
   d3d12_ngx_transition(cmd, color->texture, read, color->state);
   d3d12_ngx_transition(cmd, D3D12_NGX_RES(&inputs->motion_vectors), read,
         (D3D12_RESOURCE_STATES)inputs->motion_vectors.state);
   d3d12_ngx_transition(cmd, D3D12_NGX_RES(&inputs->depth), read,
         (D3D12_RESOURCE_STATES)inputs->depth.state);
   if (inputs->exposure.image)
      d3d12_ngx_transition(cmd, D3D12_NGX_RES(&inputs->exposure), read,
            (D3D12_RESOURCE_STATES)inputs->exposure.state);
   if (inputs->transparency_mask.image)
      d3d12_ngx_transition(cmd, D3D12_NGX_RES(&inputs->transparency_mask), read,
            (D3D12_RESOURCE_STATES)inputs->transparency_mask.state);
   d3d12_ngx_transition(cmd, ngx->out_texture,
         D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
         D3D12_RESOURCE_STATE_COPY_SOURCE);
   if (NGX_FAILED(res))
   {
      RARCH_ERR("[D3D12] Temporal Upscaler: evaluate failed (0x%08x).\n",
            (unsigned)res);
      ngx->failed_out_dims = out_dims;
      ngx->failed_quality  = quality;
      return false;
   }
   out->texture = ngx->out_texture;
   out->format  = ngx->out_format;
   out->state   = D3D12_RESOURCE_STATE_COPY_SOURCE;
   out->dims    = out_dims;
   return true;
}

void d3d12_ngx_free(d3d12_ngx_t *ngx)
{
   unsigned i;
   int in_use = 0;
   if (!ngx)
      return;
   d3d12_ngx_retire(ngx);
   for (i = 0; i < D3D12_NGX_RETIRED_MAX; i++)
      ngx->retired[i].frames = ngx->retired[i].frames ? 1 : 0;
   d3d12_ngx_frame_done(ngx);
   RARCH_LOG("[D3D12] Temporal Upscaler: DLSS feature released.\n");
   if (ngx->params)
      ngx->lib->d3d12.destroy_parameters(ngx->params);
   if (ngx->caps)
      ngx->lib->d3d12.destroy_parameters(ngx->caps);
   RARCH_LOG("[D3D12] Temporal Upscaler: NGX parameters destroyed.\n");
   ngx->lib->d3d12.shutdown1((void*)ngx->device, &in_use);
   RARCH_LOG("[D3D12] Temporal Upscaler: NGX shut down.\n");
   free(ngx->wdir);
   free(ngx);
}
