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

#include <retro_miscellaneous.h>
#include <gfx/amd/ffx_loader.h>

#include "../../verbosity.h"
#include "dxgi_common.h"
#include "../video_upscaler.h"
#include "ffx_common.h"
#include "d3d12_ffx.h"

struct d3d12_ffx
{
   const ffx_lib_t *lib;
   ffx_context_t context;
   ID3D12Device *device;
   d3d12_upscaler_ring_t ring;
   d3d12_upscaler_output_t out;
   uint64_t version;
   /* What the context was made for; sizes are VIDEO_SCALE_PACK */
   unsigned context_out_dims;
   unsigned context_flags;
   /* What failed last: not tried again until the output or flags change */
   unsigned failed_out_dims;
   unsigned failed_flags;
   bool reset_pending;
};

/* ffxApiGetSurfaceFormatDX12 of the SDK's ffx_api_dx12.h */
static unsigned d3d12_ffx_format(DXGI_FORMAT format)
{
   switch (format)
   {
      case DXGI_FORMAT_R32G32B32A32_FLOAT:    return FFX_FORMAT_R32G32B32A32_FLOAT;
      case DXGI_FORMAT_R16G16B16A16_FLOAT:    return FFX_FORMAT_R16G16B16A16_FLOAT;
      case DXGI_FORMAT_R32G32_FLOAT:          return FFX_FORMAT_R32G32_FLOAT;
      case DXGI_FORMAT_R32G8X24_TYPELESS:
      case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
      case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
      case DXGI_FORMAT_D32_FLOAT:
      case DXGI_FORMAT_R32_FLOAT:             return FFX_FORMAT_R32_FLOAT;
      case DXGI_FORMAT_R24G8_TYPELESS:
      case DXGI_FORMAT_D24_UNORM_S8_UINT:
      case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
      case DXGI_FORMAT_R32_UINT:              return FFX_FORMAT_R32_UINT;
      case DXGI_FORMAT_R10G10B10A2_UNORM:     return FFX_FORMAT_R10G10B10A2_UNORM;
      case DXGI_FORMAT_R11G11B10_FLOAT:       return FFX_FORMAT_R11G11B10_FLOAT;
      case DXGI_FORMAT_R8G8B8A8_UNORM:        return FFX_FORMAT_R8G8B8A8_UNORM;
      case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:   return FFX_FORMAT_R8G8B8A8_SRGB;
      case DXGI_FORMAT_B8G8R8A8_UNORM:        return FFX_FORMAT_B8G8R8A8_UNORM;
      case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:   return FFX_FORMAT_B8G8R8A8_SRGB;
      case DXGI_FORMAT_R16G16_FLOAT:          return FFX_FORMAT_R16G16_FLOAT;
      case DXGI_FORMAT_R16_FLOAT:             return FFX_FORMAT_R16_FLOAT;
      case DXGI_FORMAT_D16_UNORM:
      case DXGI_FORMAT_R16_UNORM:             return FFX_FORMAT_R16_UNORM;
      case DXGI_FORMAT_R8_UNORM:
      case DXGI_FORMAT_A8_UNORM:              return FFX_FORMAT_R8_UNORM;
      case DXGI_FORMAT_R8_UINT:               return FFX_FORMAT_R8_UINT;
      default:                                break;
   }
   return FFX_FORMAT_UNKNOWN;
}

static bool d3d12_ffx_is_depth(DXGI_FORMAT format)
{
   switch (format)
   {
      case DXGI_FORMAT_D16_UNORM:
      case DXGI_FORMAT_D24_UNORM_S8_UINT:
      case DXGI_FORMAT_D32_FLOAT:
      case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
      case DXGI_FORMAT_R24G8_TYPELESS:
      case DXGI_FORMAT_R32G8X24_TYPELESS:
         return true;
      default:
         break;
   }
   return false;
}

/* A UAV cannot be sRGB: the output takes the UNORM twin, the encoding
 * the same */
static DXGI_FORMAT d3d12_ffx_output_format(DXGI_FORMAT format)
{
   switch (format)
   {
      case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;
      case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM;
      default:                              break;
   }
   return format;
}

d3d12_ffx_t *d3d12_ffx_init(ID3D12Device *device, const char *dir)
{
   d3d12_ffx_t *ffx;
   const ffx_lib_t *lib = ffx_lib_get(FFX_API_D3D12, dir ? dir : "");
   const char *name;
   if (!lib)
   {
      RARCH_LOG("[D3D12] Temporal Upscaler: %s, upscaler off.\n",
            ffx_lib_status(FFX_API_D3D12));
      return NULL;
   }
   if (!(ffx = (d3d12_ffx_t*)calloc(1, sizeof(*ffx))))
      return NULL;
   ffx->lib     = lib;
   ffx->device  = device;
   ffx->version = ffx_common_version(lib, (void*)device, &name);
   RARCH_LOG("[D3D12] Temporal Upscaler: %s; FSR ready (%s).\n",
         ffx_lib_status(FFX_API_D3D12), *name ? name : "the library's own version");
   return ffx;
}

bool d3d12_ffx_optimal(d3d12_ffx_t *ffx, unsigned mode,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims)
{
   if (!ffx)
      return false;
   return ffx_common_optimal(ffx->lib, mode, out_dims, render_dims,
         min_dims, max_dims);
}

static void d3d12_ffx_release(void *data, void *feature)
{
   d3d12_ffx_t *ffx = (d3d12_ffx_t*)data;
   ffx_context_t context = (ffx_context_t)feature;
   ffx->lib->destroy_context(&context, NULL);
}

static void d3d12_ffx_retire(d3d12_ffx_t *ffx)
{
   ffx->ring.release      = d3d12_ffx_release;
   ffx->ring.release_data = ffx;
   d3d12_upscaler_ring_retire(&ffx->ring, ffx->context, &ffx->out);
   ffx->context = NULL;
}

void d3d12_ffx_frame_done(d3d12_ffx_t *ffx)
{
   if (!ffx)
      return;
   d3d12_upscaler_ring_frame_done(&ffx->ring);
}

static bool d3d12_ffx_create_context(d3d12_ffx_t *ffx, DXGI_FORMAT format,
      unsigned flags, unsigned out_dims)
{
   ffx_upscale_create_t create;
   ffx_backend_dx12_t backend;
   ffx_override_version_t version;
   ffx_return_t res;

   d3d12_ffx_retire(ffx);
   if (!d3d12_upscaler_output_create(ffx->device,
            d3d12_ffx_output_format(format), out_dims, &ffx->out))
   {
      RARCH_ERR("[D3D12] Temporal Upscaler: no output texture of %ux%u.\n",
            VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims));
      return false;
   }
   memset(&create, 0, sizeof(create));
   memset(&backend, 0, sizeof(backend));
   memset(&version, 0, sizeof(version));
   create.header.type            = FFX_TYPE_UPSCALE_CREATE;
   create.header.next            = &backend.header;
   create.flags                  = flags;
   create.max_render_size.width  = VIDEO_SCALE_W(out_dims);
   create.max_render_size.height = VIDEO_SCALE_H(out_dims);
   create.max_upscale_size       = create.max_render_size;
   create.message                = ffx_common_message;
   backend.header.type           = FFX_TYPE_BACKEND_DX12;
   backend.device                = (void*)ffx->device;
   if (ffx->version)
   {
      backend.header.next = &version.header;
      version.header.type = FFX_TYPE_OVERRIDE_VERSION;
      version.version_id  = ffx->version;
   }
   res = ffx->lib->create_context(&ffx->context, &create.header, NULL);
   if (res != FFX_RETURN_OK)
   {
      RARCH_ERR("[D3D12] Temporal Upscaler: FSR context -> %ux%u failed (%u).\n",
            VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims), (unsigned)res);
      ffx->context = NULL;
      d3d12_ffx_retire(ffx);
      ffx->failed_out_dims = out_dims;
      ffx->failed_flags    = flags;
      return false;
   }
   ffx->context_out_dims = out_dims;
   ffx->context_flags    = flags;
   ffx->reset_pending    = true;
   RARCH_LOG("[D3D12] Temporal Upscaler: FSR context -> %ux%u, flags 0x%x.\n",
         VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims), flags);
   return true;
}

/* An input as the library takes it, readable by its compute shaders */
static void d3d12_ffx_resource(ffx_resource_t *r, ID3D12Resource *texture,
      DXGI_FORMAT format, unsigned width, unsigned height, unsigned usage,
      unsigned state)
{
   memset(r, 0, sizeof(*r));
   r->resource              = (void*)texture;
   r->description.type      = FFX_RESOURCE_TEXTURE2D;
   r->description.format    = d3d12_ffx_format(format);
   r->description.width     = width;
   r->description.height    = height;
   r->description.depth     = 1;
   r->description.mip_count = 1;
   r->description.usage     = usage;
   if (d3d12_ffx_is_depth(format))
      r->description.usage |= FFX_USAGE_DEPTHTARGET;
   r->state                 = state;
}

static void d3d12_ffx_input(ffx_resource_t *r,
      const struct retro_hw_upscaler_image *img)
{
   d3d12_ffx_resource(r, D3D12_UPSCALER_RES(img), (DXGI_FORMAT)img->format,
         img->width, img->height, FFX_USAGE_READ_ONLY, FFX_STATE_COMPUTE_READ);
}

bool d3d12_ffx_upscale(d3d12_ffx_t *ffx, ID3D12GraphicsCommandList *cmd,
      const d3d12_upscaler_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned mode, unsigned out_dims, d3d12_upscaler_image_t *out)
{
   ffx_upscale_dispatch_t d;
   unsigned render_dims, min_dims, max_dims;
   unsigned flags;
   ffx_return_t res;
   const D3D12_RESOURCE_STATES read = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

   if (!ffx || !inputs || !color->texture || !inputs->motion_vectors.image
         || !inputs->depth.image
         || VIDEO_SCALE_W(out_dims) < 32 || VIDEO_SCALE_H(out_dims) < 32)
      return false;
   flags = ffx_common_create_flags(inputs);
   if (ffx->failed_out_dims == out_dims && ffx->failed_flags == flags)
      return false;
   if (!d3d12_ffx_optimal(ffx, mode, out_dims, &render_dims, &min_dims,
            &max_dims))
      return false;
   if (     VIDEO_SCALE_W(color->dims) < VIDEO_SCALE_W(min_dims)
         || VIDEO_SCALE_W(color->dims) > VIDEO_SCALE_W(max_dims)
         || VIDEO_SCALE_H(color->dims) < VIDEO_SCALE_H(min_dims)
         || VIDEO_SCALE_H(color->dims) > VIDEO_SCALE_H(max_dims))
      return false;

   if (   !ffx->context
       || ffx->context_out_dims != out_dims
       || ffx->context_flags != flags
       || ffx->out.format != d3d12_ffx_output_format(color->format))
   {
      if (!d3d12_ffx_create_context(ffx, color->format, flags, out_dims))
         return false;
   }

   /* Inputs readable by the compute shader; the output a UAV. The
    * output's last frame left it a copy source. */
   d3d12_upscaler_transition(cmd, color->texture, color->state, read);
   d3d12_upscaler_transition(cmd, D3D12_UPSCALER_RES(&inputs->motion_vectors),
         (D3D12_RESOURCE_STATES)inputs->motion_vectors.state, read);
   d3d12_upscaler_transition(cmd, D3D12_UPSCALER_RES(&inputs->depth),
         (D3D12_RESOURCE_STATES)inputs->depth.state, read);
   if (inputs->exposure.image)
      d3d12_upscaler_transition(cmd, D3D12_UPSCALER_RES(&inputs->exposure),
            (D3D12_RESOURCE_STATES)inputs->exposure.state, read);
   if (inputs->transparency_mask.image)
      d3d12_upscaler_transition(cmd, D3D12_UPSCALER_RES(&inputs->transparency_mask),
            (D3D12_RESOURCE_STATES)inputs->transparency_mask.state, read);
   if (!ffx->reset_pending)
      d3d12_upscaler_transition(cmd, ffx->out.texture,
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

   memset(&d, 0, sizeof(d));
   d.command_list = (void*)cmd;
   d3d12_ffx_resource(&d.color, color->texture, color->format,
         VIDEO_SCALE_W(color->dims), VIDEO_SCALE_H(color->dims),
         FFX_USAGE_READ_ONLY, FFX_STATE_COMPUTE_READ);
   d3d12_ffx_input(&d.depth, &inputs->depth);
   d3d12_ffx_input(&d.motion_vectors, &inputs->motion_vectors);
   if (inputs->exposure.image)
      d3d12_ffx_input(&d.exposure, &inputs->exposure);
   if (inputs->transparency_mask.image)
      d3d12_ffx_input(&d.transparency_and_composition, &inputs->transparency_mask);
   d3d12_ffx_resource(&d.output, ffx->out.texture, ffx->out.format,
         VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims),
         FFX_USAGE_UAV, FFX_STATE_UNORDERED_ACCESS);
   ffx_common_dispatch(&d, inputs, color->dims, out_dims, ffx->reset_pending,
         FFX_MODE_SHARPNESS(mode));
   res = ffx->lib->dispatch(&ffx->context, &d.header);
   ffx->reset_pending = false;

   /* Everything back as the core left it; the output a copy source */
   d3d12_upscaler_transition(cmd, color->texture, read, color->state);
   d3d12_upscaler_transition(cmd, D3D12_UPSCALER_RES(&inputs->motion_vectors), read,
         (D3D12_RESOURCE_STATES)inputs->motion_vectors.state);
   d3d12_upscaler_transition(cmd, D3D12_UPSCALER_RES(&inputs->depth), read,
         (D3D12_RESOURCE_STATES)inputs->depth.state);
   if (inputs->exposure.image)
      d3d12_upscaler_transition(cmd, D3D12_UPSCALER_RES(&inputs->exposure), read,
            (D3D12_RESOURCE_STATES)inputs->exposure.state);
   if (inputs->transparency_mask.image)
      d3d12_upscaler_transition(cmd, D3D12_UPSCALER_RES(&inputs->transparency_mask), read,
            (D3D12_RESOURCE_STATES)inputs->transparency_mask.state);
   d3d12_upscaler_transition(cmd, ffx->out.texture,
         D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
         D3D12_RESOURCE_STATE_COPY_SOURCE);
   if (res != FFX_RETURN_OK)
   {
      RARCH_ERR("[D3D12] Temporal Upscaler: FSR dispatch failed (%u).\n",
            (unsigned)res);
      ffx->failed_out_dims = out_dims;
      ffx->failed_flags    = flags;
      return false;
   }
   out->texture = ffx->out.texture;
   out->format  = ffx->out.format;
   out->state   = D3D12_RESOURCE_STATE_COPY_SOURCE;
   out->dims    = out_dims;
   return true;
}

void d3d12_ffx_free(d3d12_ffx_t *ffx)
{
   if (!ffx)
      return;
   d3d12_ffx_retire(ffx);
   d3d12_upscaler_ring_flush(&ffx->ring);
   RARCH_LOG("[D3D12] Temporal Upscaler: FSR context released.\n");
   free(ffx);
}
