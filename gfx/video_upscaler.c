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

#include <math.h>
#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <gfx/nvidia/ngx_loader.h>
#include <gfx/amd/ffx_loader.h>
#include <gfx/intel/xess_loader.h>

#include "../configuration.h"
#include "../verbosity.h"
#include "video_driver.h"
#include "video_upscaler.h"

const char *video_upscaler_name(enum video_temporal_upscaler kind)
{
   switch (kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS: return "DLSS";
      case VIDEO_TEMPORAL_UPSCALER_FSR:  return "FSR";
      case VIDEO_TEMPORAL_UPSCALER_XESS: return "XeSS";
      default:                           break;
   }
   return "Off";
}

size_t video_upscaler_dir(enum video_temporal_upscaler kind, char *s, size_t len)
{
   settings_t *settings = config_get_ptr();
   const char *set      = NULL;
   const char *fallback = NULL;
   switch (kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
         set      = settings->paths.directory_dlss;
         fallback = ":/upscalers/dlss";
         break;
      case VIDEO_TEMPORAL_UPSCALER_FSR:
         set      = settings->paths.directory_fsr;
         fallback = ":/upscalers/fsr";
         break;
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         set      = settings->paths.directory_xess;
         fallback = ":/upscalers/xess";
         break;
      default:
         *s = '\0';
         return 0;
   }
   if (!string_is_empty(set))
      return strlcpy(s, set, len);
   /* the application directory's: beside shaders and system */
   return fill_pathname_expand_special(s, fallback, len);
}

/* Which API's FSR library the driver up would load: the probe looks
 * for both, and Vulkan's where there is no driver to say */
static enum ffx_api video_upscaler_ffx_api(void)
{
   const char *ident = video_driver_get_ident();
   if (string_is_equal(ident, "d3d12"))
      return FFX_API_D3D12;
   return FFX_API_VULKAN;
}

unsigned video_upscaler_probe(enum video_temporal_upscaler kind)
{
   char dir[DIR_MAX_LENGTH];
   unsigned found = 0;
   video_upscaler_dir(kind, dir, sizeof(dir));
   switch (kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
         if (ngx_lib_get())
            found |= VIDEO_UPSCALER_FOUND_LIBRARY;
         if (ngx_dlss_feature_in(dir))
            found |= VIDEO_UPSCALER_FOUND_RUNTIME;
         break;
      case VIDEO_TEMPORAL_UPSCALER_FSR:
         if (ffx_runtime_in(FFX_API_D3D12, dir) || ffx_runtime_in(FFX_API_VULKAN, dir))
            found |= VIDEO_UPSCALER_FOUND_RUNTIME;
         if (ffx_lib_get(video_upscaler_ffx_api(), dir))
            found |= VIDEO_UPSCALER_FOUND_LIBRARY;
         break;
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         if (xess_runtime_in(dir))
            found |= VIDEO_UPSCALER_FOUND_RUNTIME;
         if (xess_lib_get(dir))
            found |= VIDEO_UPSCALER_FOUND_LIBRARY;
         break;
      default:
         break;
   }
   return found;
}

size_t video_upscaler_status_string(char *s, size_t len)
{
   unsigned want  = config_get_ptr()->uints.video_temporal_upscaler;
   unsigned found;
   size_t _len;
   if (want >= VIDEO_TEMPORAL_UPSCALER_LAST)
      want = VIDEO_TEMPORAL_UPSCALER_OFF;
   _len  = strlcpy(s, video_upscaler_name((enum video_temporal_upscaler)want), len);
   switch (want)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
         found = video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_DLSS);
         _len += strlcpy(s + _len, (found & VIDEO_UPSCALER_FOUND_LIBRARY)
               ? " (NGX: found" : " (NGX: not found", len - _len);
         _len += strlcpy(s + _len, (found & VIDEO_UPSCALER_FOUND_RUNTIME)
               ? ", DLSS runtime: found)" : ", DLSS runtime: not in DLSS Directory)",
               len - _len);
         break;
      case VIDEO_TEMPORAL_UPSCALER_FSR:
         found = video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_FSR);
         _len += strlcpy(s + _len, (found & VIDEO_UPSCALER_FOUND_RUNTIME)
               ? " (FSR library: found)" : " (FSR library: not in FSR Directory)",
               len - _len);
         break;
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         found = video_upscaler_probe(VIDEO_TEMPORAL_UPSCALER_XESS);
         _len += strlcpy(s + _len, (found & VIDEO_UPSCALER_FOUND_RUNTIME)
               ? " (XeSS library: found)" : " (XeSS library: not in XeSS Directory)",
               len - _len);
         break;
      default:
         break;
   }
   return _len;
}

void video_upscaler_log_probe(enum video_temporal_upscaler kind)
{
   char dir[DIR_MAX_LENGTH];
   video_upscaler_dir(kind, dir, sizeof(dir));
   switch (kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
         ngx_lib_get();
         RARCH_LOG("[Video] Temporal Upscaler: %s.\n", ngx_lib_status());
         RARCH_LOG("[Video] Temporal Upscaler: DLSS runtime (nvngx_dlss.dll / libnvidia-ngx-dlss.so) %s in \"%s\".\n",
               ngx_dlss_feature_in(dir) ? "found" : "NOT found", dir);
         break;
      case VIDEO_TEMPORAL_UPSCALER_FSR:
      {
         enum ffx_api api = video_upscaler_ffx_api();
         ffx_lib_get(api, dir);
         RARCH_LOG("[Video] Temporal Upscaler: %s.\n", ffx_lib_status(api));
         break;
      }
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         xess_lib_get(dir);
         RARCH_LOG("[Video] Temporal Upscaler: %s.\n", xess_lib_status());
         break;
      default:
         break;
   }
}

static const video_upscaler_backend_t *upscaler_backend;
static const struct retro_hw_upscaler_inputs *upscaler_inputs;
static unsigned upscaler_out_dims;

static unsigned video_upscaler_quality(unsigned out_dims)
{
   unsigned q = config_get_ptr()->uints.video_temporal_upscaler_quality;
   if (q > VIDEO_TEMPORAL_UPSCALER_QUALITY_AUTO && q < VIDEO_TEMPORAL_UPSCALER_QUALITY_LAST)
      return q;
   /* Auto: the sharper mode up to 1440 lines, Balanced from there */
   return VIDEO_SCALE_H(out_dims) > 1440
      ? VIDEO_TEMPORAL_UPSCALER_QUALITY_BALANCED : VIDEO_TEMPORAL_UPSCALER_QUALITY_QUALITY;
}

unsigned video_upscaler_mode(unsigned out_dims)
{
   unsigned preset = config_get_ptr()->uints.video_temporal_upscaler_preset;
   if (preset >= VIDEO_TEMPORAL_UPSCALER_PRESET_LAST)
      preset = VIDEO_TEMPORAL_UPSCALER_PRESET_DEFAULT;
   return VIDEO_UPSCALER_MODE_PACK(video_upscaler_quality(out_dims), preset);
}

unsigned video_upscaler_mode_ngx(unsigned mode)
{
   unsigned quality = NGX_PERF_QUALITY_MAX_QUALITY;
   unsigned preset  = NGX_DLSS_PRESET_DEFAULT;
   switch (VIDEO_UPSCALER_MODE_QUALITY(mode))
   {
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_NATIVE:
         quality = NGX_PERF_QUALITY_DLAA;
         break;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_BALANCED:
         quality = NGX_PERF_QUALITY_BALANCED;
         break;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_PERFORMANCE:
         quality = NGX_PERF_QUALITY_MAX_PERF;
         break;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_ULTRA_PERFORMANCE:
         quality = NGX_PERF_QUALITY_ULTRA_PERFORMANCE;
         break;
      default:
         break;
   }
   switch (VIDEO_UPSCALER_MODE_PRESET(mode))
   {
      case VIDEO_TEMPORAL_UPSCALER_PRESET_J:
         preset = NGX_DLSS_PRESET_J;
         break;
      case VIDEO_TEMPORAL_UPSCALER_PRESET_K:
         preset = NGX_DLSS_PRESET_K;
         break;
      default:
         break;
   }
   return NGX_MODE_PACK(quality, preset);
}

unsigned video_upscaler_mode_ffx(unsigned mode)
{
   switch (VIDEO_UPSCALER_MODE_QUALITY(mode))
   {
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_NATIVE:
         return FFX_UPSCALE_MODE_NATIVEAA;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_BALANCED:
         return FFX_UPSCALE_MODE_BALANCED;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_PERFORMANCE:
         return FFX_UPSCALE_MODE_PERFORMANCE;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_ULTRA_PERFORMANCE:
         return FFX_UPSCALE_MODE_ULTRA_PERFORMANCE;
      default:
         break;
   }
   return FFX_UPSCALE_MODE_QUALITY;
}

unsigned video_upscaler_mode_xess(unsigned mode)
{
   switch (VIDEO_UPSCALER_MODE_QUALITY(mode))
   {
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_NATIVE:
         return XESS_QUALITY_AA;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_BALANCED:
         return XESS_QUALITY_BALANCED;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_PERFORMANCE:
         return XESS_QUALITY_PERFORMANCE;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_ULTRA_PERFORMANCE:
         return XESS_QUALITY_ULTRA_PERFORMANCE;
      default:
         break;
   }
   return XESS_QUALITY_QUALITY;
}

void video_upscaler_attach(const video_upscaler_backend_t *backend)
{
   upscaler_backend = backend;
}

void video_upscaler_detach(void)
{
   upscaler_backend = NULL;
   upscaler_inputs  = NULL;
}

void video_upscaler_set_output(unsigned dims)
{
   upscaler_out_dims = dims;
}

static bool video_upscaler_get_request(void *handle,
      struct retro_hw_upscaler_request *request)
{
   unsigned mode;
   unsigned render_dims, min_dims, max_dims;
   const video_upscaler_backend_t *backend = upscaler_backend;
   (void)handle;
   if (!backend || !VIDEO_SCALE_W(upscaler_out_dims)
         || !VIDEO_SCALE_H(upscaler_out_dims)
         || !config_get_ptr()->uints.video_temporal_upscaler)
      return false;
   mode = video_upscaler_mode(upscaler_out_dims);
   if (!backend->optimal(backend->data, mode, upscaler_out_dims,
            &render_dims, &min_dims, &max_dims))
      return false;
   request->output_width      = VIDEO_SCALE_W(upscaler_out_dims);
   request->output_height     = VIDEO_SCALE_H(upscaler_out_dims);
   request->render_width      = VIDEO_SCALE_W(render_dims);
   request->render_height     = VIDEO_SCALE_H(render_dims);
   request->min_render_width  = VIDEO_SCALE_W(min_dims);
   request->min_render_height = VIDEO_SCALE_H(min_dims);
   request->max_render_width  = VIDEO_SCALE_W(max_dims);
   request->max_render_height = VIDEO_SCALE_H(max_dims);
   request->flags             = 0;
   request->mip_bias          = (float)(log((double)request->render_width
            / (double)request->output_width) / log(2.0)) - 1.0f;
   return true;
}

static void video_upscaler_set_inputs(void *handle,
      const struct retro_hw_upscaler_inputs *inputs)
{
   (void)handle;
   upscaler_inputs = inputs;
}

const struct retro_hw_upscaler_inputs *video_upscaler_take_inputs(void)
{
   const struct retro_hw_upscaler_inputs *inputs = upscaler_inputs;
   upscaler_inputs = NULL;
   return inputs;
}

static const struct retro_hw_render_upscaler_interface upscaler_iface = {
   RETRO_HW_RENDER_UPSCALER_INTERFACE_VERSION,
   NULL,
   video_upscaler_get_request,
   video_upscaler_set_inputs
};

const struct retro_hw_render_upscaler_interface *video_upscaler_get_interface(void)
{
   if (!upscaler_backend)
      return NULL;
   return &upscaler_iface;
}
