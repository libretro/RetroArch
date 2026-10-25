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

#include "../configuration.h"
#include "video_upscaler.h"

size_t video_upscaler_dlss_dir(char *s, size_t len)
{
   settings_t *settings = config_get_ptr();
   if (!string_is_empty(settings->paths.directory_dlss))
      return strlcpy(s, settings->paths.directory_dlss, len);
   /* the application directory's: beside shaders and system */
   return fill_pathname_expand_special(s, ":/upscalers/dlss", len);
}

unsigned video_upscaler_dlss_probe(void)
{
   char dir[DIR_MAX_LENGTH];
   unsigned found = 0;
   if (ngx_lib_get())
      found |= VIDEO_UPSCALER_DLSS_NGX;
   video_upscaler_dlss_dir(dir, sizeof(dir));
   if (ngx_dlss_feature_in(dir))
      found |= VIDEO_UPSCALER_DLSS_FEATURE;
   return found;
}

size_t video_upscaler_status_string(char *s, size_t len)
{
   static const char *modes[VIDEO_TEMPORAL_UPSCALER_LAST] = { "Off", "DLSS" };
   unsigned want  = config_get_ptr()->uints.video_temporal_upscaler;
   unsigned found = video_upscaler_dlss_probe();
   size_t _len;
   if (want >= VIDEO_TEMPORAL_UPSCALER_LAST)
      want = VIDEO_TEMPORAL_UPSCALER_OFF;
   _len  = strlcpy(s, modes[want], len);
   _len += strlcpy(s + _len, (found & VIDEO_UPSCALER_DLSS_NGX)
         ? " (NGX: found" : " (NGX: not found", len - _len);
   _len += strlcpy(s + _len, (found & VIDEO_UPSCALER_DLSS_FEATURE)
         ? ", DLSS runtime: found)" : ", DLSS runtime: not in DLSS Directory)",
         len - _len);
   return _len;
}

/* The core's side: the request and the frame's inputs. HW cores run on
 * the thread that presents, so a frame's inputs are set and taken in
 * one video_refresh. */
static const video_upscaler_backend_t *upscaler_backend;
static const struct retro_hw_upscaler_inputs *upscaler_inputs;
static unsigned upscaler_out_dims;

unsigned video_upscaler_mode(unsigned out_dims)
{
   unsigned q = config_get_ptr()->uints.video_temporal_upscaler_quality;
   switch (q)
   {
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_NATIVE:
         return NGX_PERF_QUALITY_DLAA;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_QUALITY:
         return NGX_PERF_QUALITY_MAX_QUALITY;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_BALANCED:
         return NGX_PERF_QUALITY_BALANCED;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_PERFORMANCE:
         return NGX_PERF_QUALITY_MAX_PERF;
      case VIDEO_TEMPORAL_UPSCALER_QUALITY_ULTRA_PERFORMANCE:
         return NGX_PERF_QUALITY_ULTRA_PERFORMANCE;
      default:
         break;
   }
   /* Auto: the sharper mode up to 1440 lines, Balanced from there */
   return VIDEO_SCALE_H(out_dims) > 1440
      ? NGX_PERF_QUALITY_BALANCED : NGX_PERF_QUALITY_MAX_QUALITY;
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
