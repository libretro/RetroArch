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
#include <gfx/intel/xess_loader.h>

#include "../../verbosity.h"
#include "dxgi_common.h"
#include "xess_common.h"
#include "d3d12_xess.h"

struct d3d12_xess
{
   const xess_lib_t *lib;
   xess_context_t context;
   ID3D12Device *device;
   d3d12_upscaler_ring_t ring;
   d3d12_upscaler_output_t out;
   xess_common_state_t state;
};

/* A UAV cannot be sRGB: the output takes the UNORM twin, the encoding
 * the same */
static DXGI_FORMAT d3d12_xess_output_format(DXGI_FORMAT format)
{
   switch (format)
   {
      case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM;
      case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return DXGI_FORMAT_B8G8R8A8_UNORM;
      default:                              break;
   }
   return format;
}

d3d12_xess_t *d3d12_xess_init(ID3D12Device *device, const char *dir)
{
   d3d12_xess_t *xess;
   const xess_lib_t *lib = xess_lib_get(dir ? dir : "");
   if (!lib || !lib->d3d12.create_context)
   {
      RARCH_LOG("[D3D12] Temporal Upscaler: %s, upscaler off.\n", xess_lib_status());
      return NULL;
   }
   if (!(xess = (d3d12_xess_t*)calloc(1, sizeof(*xess))))
      return NULL;
   xess->lib    = lib;
   xess->device = device;
   /* (a context of its own answers the size queries before a frame
    * brings the first) */
   if (XESS_FAILED(lib->d3d12.create_context((void*)device, &xess->state.query)))
   {
      RARCH_LOG("[D3D12] Temporal Upscaler: XeSS does not run on this device, upscaler off.\n");
      free(xess);
      return NULL;
   }
   RARCH_LOG("[D3D12] Temporal Upscaler: %s; XeSS ready.\n", xess_lib_status());
   return xess;
}

bool d3d12_xess_optimal(d3d12_xess_t *xess, unsigned mode,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims)
{
   if (!xess)
      return false;
   return xess_common_optimal(xess->lib, &xess->state, mode, out_dims,
         render_dims, min_dims, max_dims);
}

static void d3d12_xess_release(void *data, void *feature)
{
   d3d12_xess_t *xess = (d3d12_xess_t*)data;
   xess->lib->common.destroy_context((xess_context_t)feature);
}

static void d3d12_xess_retire(d3d12_xess_t *xess)
{
   xess->ring.release      = d3d12_xess_release;
   xess->ring.release_data = xess;
   d3d12_upscaler_ring_retire(&xess->ring, xess->context, &xess->out);
   xess->context = NULL;
}

void d3d12_xess_frame_done(d3d12_xess_t *xess)
{
   if (!xess)
      return;
   d3d12_upscaler_ring_frame_done(&xess->ring);
}

static bool d3d12_xess_create(d3d12_xess_t *xess, DXGI_FORMAT format,
      unsigned mode, unsigned flags, unsigned out_dims)
{
   xess_result_t res;
   d3d12_xess_retire(xess);
   if (!d3d12_upscaler_output_create(xess->device,
            d3d12_xess_output_format(format), out_dims, &xess->out))
   {
      RARCH_ERR("[D3D12] Temporal Upscaler: no output texture of %ux%u.\n",
            VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims));
      return false;
   }
   res = xess->lib->d3d12.create_context((void*)xess->device, &xess->context);
   if (XESS_FAILED(res))
   {
      RARCH_ERR("[D3D12] Temporal Upscaler: XeSS context failed (%d).\n", (int)res);
      xess->context = NULL;
      d3d12_xess_retire(xess);
      xess_common_failed(&xess->state, out_dims, mode, flags);
      return false;
   }
   xess_common_created(xess->lib, &xess->state, xess->context, mode, flags, out_dims);
   res = xess->lib->d3d12.build_pipelines(xess->context, NULL,
         xess->lib->common.get_pipeline_build_status ? false : true, flags);
   if (XESS_FAILED(res))
   {
      RARCH_ERR("[D3D12] Temporal Upscaler: XeSS pipelines failed (%d).\n", (int)res);
      d3d12_xess_retire(xess);
      xess_common_failed(&xess->state, out_dims, mode, flags);
      return false;
   }
   return true;
}

static bool d3d12_xess_ready(d3d12_xess_t *xess, unsigned out_dims)
{
   xess_d3d12_init_params_t init;
   xess_result_t res;
   if (xess->state.inited)
      return true;
   if (xess->lib->common.get_pipeline_build_status)
   {
      res = xess->lib->common.get_pipeline_build_status(xess->context);
      if (res == XESS_RESULT_ERROR_OPERATION_IN_PROGRESS)
         return false;
      if (XESS_FAILED(res))
      {
         RARCH_ERR("[D3D12] Temporal Upscaler: XeSS pipeline build failed (%d).\n", (int)res);
         xess_common_failed(&xess->state, out_dims, xess->state.mode, xess->state.flags);
         d3d12_xess_retire(xess);
         return false;
      }
   }
   memset(&init, 0, sizeof(init));
   init.output_resolution.x = VIDEO_SCALE_W(out_dims);
   init.output_resolution.y = VIDEO_SCALE_H(out_dims);
   init.quality             = (int)xess->state.mode;
   init.init_flags          = xess->state.flags;
   init.creation_node_mask  = 1;
   init.visible_node_mask   = 1;
   res = xess->lib->d3d12.init(xess->context, &init);
   if (XESS_FAILED(res))
   {
      RARCH_ERR("[D3D12] Temporal Upscaler: XeSS init %ux%u failed (%d).\n",
            VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims), (int)res);
      xess_common_failed(&xess->state, out_dims, xess->state.mode, xess->state.flags);
      d3d12_xess_retire(xess);
      return false;
   }
   xess_common_inited(xess->lib, &xess->state, xess->context);
   RARCH_LOG("[D3D12] Temporal Upscaler: XeSS -> %ux%u (%s), flags 0x%x.\n",
         VIDEO_SCALE_W(out_dims), VIDEO_SCALE_H(out_dims),
         xess_common_mode_name(xess->state.mode), xess->state.flags);
   return true;
}

bool d3d12_xess_upscale(d3d12_xess_t *xess, ID3D12GraphicsCommandList *cmd,
      const d3d12_upscaler_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned mode, unsigned out_dims, d3d12_upscaler_image_t *out)
{
   xess_d3d12_execute_params_t e;
   unsigned render_dims, min_dims, max_dims;
   unsigned flags;
   xess_result_t res;
   const D3D12_RESOURCE_STATES read = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

   if (!xess || !inputs || !color->texture || !inputs->motion_vectors.image
         || !inputs->depth.image
         || VIDEO_SCALE_W(out_dims) < 32 || VIDEO_SCALE_H(out_dims) < 32)
      return false;
   flags = xess_common_init_flags(inputs);
   if (xess_common_has_failed(&xess->state, out_dims, mode, flags))
      return false;
   if (!d3d12_xess_optimal(xess, mode, out_dims, &render_dims, &min_dims,
            &max_dims))
      return false;
   if (     VIDEO_SCALE_W(color->dims) < VIDEO_SCALE_W(min_dims)
         || VIDEO_SCALE_W(color->dims) > VIDEO_SCALE_W(max_dims)
         || VIDEO_SCALE_H(color->dims) < VIDEO_SCALE_H(min_dims)
         || VIDEO_SCALE_H(color->dims) > VIDEO_SCALE_H(max_dims))
      return false;

   if (   !xess->context
       || xess->state.out_dims != out_dims
       || xess->state.mode != mode
       || xess->state.flags != flags
       || xess->out.format != d3d12_xess_output_format(color->format))
   {
      if (!d3d12_xess_create(xess, color->format, mode, flags, out_dims))
         return false;
   }
   if (!d3d12_xess_ready(xess, out_dims))
      return false;
   xess_common_scales(xess->lib, &xess->state, xess->context, inputs);

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
   if (!xess->state.reset_pending)
      d3d12_upscaler_transition(cmd, xess->out.texture,
            D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

   memset(&e, 0, sizeof(e));
   e.color                 = (void*)color->texture;
   e.velocity              = (void*)D3D12_UPSCALER_RES(&inputs->motion_vectors);
   e.depth                 = (void*)D3D12_UPSCALER_RES(&inputs->depth);
   e.exposure_scale        = inputs->exposure.image
      ? (void*)D3D12_UPSCALER_RES(&inputs->exposure) : NULL;
   e.responsive_pixel_mask = inputs->transparency_mask.image
      ? (void*)D3D12_UPSCALER_RES(&inputs->transparency_mask) : NULL;
   e.output                = (void*)xess->out.texture;
   e.jitter_offset_x       = inputs->jitter_x;
   e.jitter_offset_y       = inputs->jitter_y;
   e.exposure_scale_value  = inputs->exposure_scale == 0.0f ? 1.0f : inputs->exposure_scale;
   e.reset_history         = (xess->state.reset_pending || (inputs->flags & RETRO_HW_UPSCALER_RESET)) ? 1 : 0;
   e.input_width           = VIDEO_SCALE_W(color->dims);
   e.input_height          = VIDEO_SCALE_H(color->dims);
   res = xess->lib->d3d12.execute(xess->context, (void*)cmd, &e);
   xess->state.reset_pending = false;

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
   d3d12_upscaler_transition(cmd, xess->out.texture,
         D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
         D3D12_RESOURCE_STATE_COPY_SOURCE);
   if (XESS_FAILED(res))
   {
      RARCH_ERR("[D3D12] Temporal Upscaler: XeSS execute failed (%d).\n", (int)res);
      xess_common_failed(&xess->state, out_dims, mode, flags);
      return false;
   }
   out->texture = xess->out.texture;
   out->format  = xess->out.format;
   out->state   = D3D12_RESOURCE_STATE_COPY_SOURCE;
   out->dims    = out_dims;
   return true;
}

void d3d12_xess_free(d3d12_xess_t *xess)
{
   if (!xess)
      return;
   d3d12_xess_retire(xess);
   d3d12_upscaler_ring_flush(&xess->ring);
   if (xess->state.query)
      xess->lib->common.destroy_context(xess->state.query);
   RARCH_LOG("[D3D12] Temporal Upscaler: XeSS context released.\n");
   free(xess);
}
