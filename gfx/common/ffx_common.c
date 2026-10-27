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

#include <string.h>

#include "../../verbosity.h"
#include "../video_upscaler.h"
#include "ffx_common.h"

unsigned ffx_common_create_flags(const struct retro_hw_upscaler_inputs *inputs)
{
   unsigned flags = FFX_UPSCALE_ENABLE_DYNAMIC_RESOLUTION;
   if (inputs->flags & RETRO_HW_UPSCALER_MV_OUTPUT_RES)
      flags |= FFX_UPSCALE_ENABLE_DISPLAY_RESOLUTION_MVS;
   if (inputs->flags & RETRO_HW_UPSCALER_MV_JITTERED)
      flags |= FFX_UPSCALE_ENABLE_MV_JITTER_CANCELLATION;
   if (inputs->flags & RETRO_HW_UPSCALER_DEPTH_INVERTED)
      flags |= FFX_UPSCALE_ENABLE_DEPTH_INVERTED;
   if (inputs->flags & RETRO_HW_UPSCALER_HDR)
      flags |= FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE;
   else
      flags |= FFX_UPSCALE_ENABLE_NON_LINEAR_COLORSPACE;
   if (!inputs->exposure.image)
      flags |= FFX_UPSCALE_ENABLE_AUTO_EXPOSURE;
   return flags;
}

uint64_t ffx_common_version(const ffx_lib_t *lib, void *device,
      const char **name)
{
   uint64_t ids[8];
   const char *names[8];
   unsigned n = ffx_lib_versions(lib, device, ids, names, 8);
   *name = "";
   if (!n)
      return 0;
   *name = names[0] ? names[0] : "";
   return ids[0];
}

bool ffx_common_optimal(const ffx_lib_t *lib, unsigned mode,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims)
{
   /* FFX_UPSCALE_MODE_NATIVEAA .. ULTRA_PERFORMANCE */
   static const float ratios[5] = { 1.0f, 1.5f, 1.7f, 2.0f, 3.0f };
   ffx_query_render_of_mode_t q;
   uint32_t rw = 0, rh = 0;
   unsigned ow = VIDEO_SCALE_W(out_dims), oh = VIDEO_SCALE_H(out_dims);
   mode = FFX_MODE_QUALITY(mode);
   if (!ow || !oh || mode > FFX_UPSCALE_MODE_ULTRA_PERFORMANCE)
      return false;
   memset(&q, 0, sizeof(q));
   q.header.type       = FFX_TYPE_UPSCALE_RENDER_OF_MODE;
   q.display_width     = ow;
   q.display_height    = oh;
   q.quality_mode      = mode;
   q.out_render_width  = &rw;
   q.out_render_height = &rh;
   if (lib->query(NULL, &q.header) != FFX_RETURN_OK || !rw || !rh)
   {
      rw = (uint32_t)((float)ow / ratios[mode]);
      rh = (uint32_t)((float)oh / ratios[mode]);
   }
   if (rw > ow)
      rw = ow;
   if (rh > oh)
      rh = oh;
   if (rw < 1)
      rw = 1;
   if (rh < 1)
      rh = 1;
   *render_dims = VIDEO_SCALE_PACK(rw, rh);
   *min_dims    = VIDEO_SCALE_PACK(ow / 3 ? ow / 3 : 1, oh / 3 ? oh / 3 : 1);
   *max_dims    = out_dims;
   return true;
}

void ffx_common_dispatch(ffx_upscale_dispatch_t *d,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned render_dims, unsigned out_dims, bool reset, unsigned sharpness)
{
   if (sharpness > 100)
      sharpness = 100;
   d->header.type           = FFX_TYPE_UPSCALE_DISPATCH;
   d->jitter_offset.x       = inputs->jitter_x;
   d->jitter_offset.y       = inputs->jitter_y;
   d->motion_vector_scale.x = inputs->mv_scale_x == 0.0f ? 1.0f : inputs->mv_scale_x;
   d->motion_vector_scale.y = inputs->mv_scale_y == 0.0f ? 1.0f : inputs->mv_scale_y;
   d->render_size.width     = VIDEO_SCALE_W(render_dims);
   d->render_size.height    = VIDEO_SCALE_H(render_dims);
   d->upscale_size.width    = VIDEO_SCALE_W(out_dims);
   d->upscale_size.height   = VIDEO_SCALE_H(out_dims);
   d->enable_sharpening     = sharpness ? 1 : 0;
   d->sharpness             = (float)sharpness / 100.0f;
   d->frame_time_delta      = video_upscaler_frame_time_ms(inputs);
   d->pre_exposure          = inputs->pre_exposure == 0.0f ? 1.0f : inputs->pre_exposure;
   d->reset                 = (reset || (inputs->flags & RETRO_HW_UPSCALER_RESET)) ? 1 : 0;
   /* The camera a core does not describe: a near plane and a far one
    * well past any scene, a vertical field of view of 60 degrees */
   d->camera_near           = 0.1f;
   d->camera_far            = 100000.0f;
   d->camera_fov_angle_vertical = 1.0471976f;
   d->view_space_to_meters_factor = 1.0f;
   d->flags                 = (inputs->flags & RETRO_HW_UPSCALER_HDR)
      ? 0 : FFX_UPSCALE_FLAG_NON_LINEAR_COLOR_SRGB;
}

void ffx_common_message(uint32_t type, const wchar_t *message)
{
   char s[256];
   unsigned i;
   for (i = 0; i + 1 < sizeof(s) && message && message[i]; i++)
      s[i] = (message[i] < 0x80) ? (char)message[i] : '?';
   s[i] = '\0';
   if (type == 0)
      RARCH_ERR("[FSR] %s\n", s);
   else
      RARCH_WARN("[FSR] %s\n", s);
}

const char *ffx_common_mode_name(unsigned mode)
{
   switch (mode)
   {
      case FFX_UPSCALE_MODE_NATIVEAA:          return "Native AA";
      case FFX_UPSCALE_MODE_QUALITY:           return "Quality";
      case FFX_UPSCALE_MODE_BALANCED:          return "Balanced";
      case FFX_UPSCALE_MODE_PERFORMANCE:       return "Performance";
      case FFX_UPSCALE_MODE_ULTRA_PERFORMANCE: return "Ultra Performance";
      default:                                 break;
   }
   return "?";
}
