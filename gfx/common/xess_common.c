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
#include "xess_common.h"

unsigned xess_common_init_flags(const struct retro_hw_upscaler_inputs *inputs)
{
   unsigned flags = 0;
   if (inputs->flags & RETRO_HW_UPSCALER_MV_OUTPUT_RES)
      flags |= XESS_INIT_FLAG_HIGH_RES_MV;
   if (inputs->flags & RETRO_HW_UPSCALER_MV_JITTERED)
      flags |= XESS_INIT_FLAG_JITTERED_MV;
   if (inputs->flags & RETRO_HW_UPSCALER_DEPTH_INVERTED)
      flags |= XESS_INIT_FLAG_INVERTED_DEPTH;
   if (!(inputs->flags & RETRO_HW_UPSCALER_HDR))
      flags |= XESS_INIT_FLAG_LDR_INPUT_COLOR;
   if (inputs->exposure.image)
      flags |= XESS_INIT_FLAG_EXPOSURE_SCALE_TEXTURE;
   else
      flags |= XESS_INIT_FLAG_ENABLE_AUTOEXPOSURE;
   if (inputs->transparency_mask.image)
      flags |= XESS_INIT_FLAG_RESPONSIVE_PIXEL_MASK;
   return flags;
}

bool xess_common_optimal(const xess_lib_t *lib, xess_common_state_t *st,
      unsigned mode, unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims)
{
   /* XESS_QUALITY_ULTRA_PERFORMANCE .. AA */
   static const float ratios[7] = { 3.0f, 2.3f, 2.0f, 1.7f, 1.5f, 1.3f, 1.0f };
   xess_2d_t out, optimal, min, max;
   unsigned ow = VIDEO_SCALE_W(out_dims), oh = VIDEO_SCALE_H(out_dims);
   if (!ow || !oh || mode < XESS_QUALITY_ULTRA_PERFORMANCE || mode > XESS_QUALITY_AA)
      return false;
   out.x = ow;
   out.y = oh;
   if (   !st->query
       || XESS_FAILED(lib->common.get_optimal_input_resolution(st->query, &out,
             (int)mode, &optimal, &min, &max))
       || !optimal.x || !optimal.y)
   {
      optimal.x = (uint32_t)((float)ow / ratios[mode - XESS_QUALITY_ULTRA_PERFORMANCE]);
      optimal.y = (uint32_t)((float)oh / ratios[mode - XESS_QUALITY_ULTRA_PERFORMANCE]);
      min.x     = ow / 3;
      min.y     = oh / 3;
      max       = out;
   }
   if (optimal.x > ow)
      optimal.x = ow;
   if (optimal.y > oh)
      optimal.y = oh;
   if (max.x > ow)
      max.x = ow;
   if (max.y > oh)
      max.y = oh;
   if (min.x < 1)
      min.x = 1;
   if (min.y < 1)
      min.y = 1;
   if (optimal.x < min.x)
      optimal.x = min.x;
   if (optimal.y < min.y)
      optimal.y = min.y;
   *render_dims = VIDEO_SCALE_PACK(optimal.x, optimal.y);
   *min_dims    = VIDEO_SCALE_PACK(min.x, min.y);
   *max_dims    = VIDEO_SCALE_PACK(max.x, max.y);
   return true;
}

void xess_common_created(const xess_lib_t *lib, xess_common_state_t *st,
      xess_context_t ctx, unsigned mode, unsigned flags, unsigned out_dims)
{
   st->out_dims      = out_dims;
   st->mode          = mode;
   st->flags         = flags;
   st->inited        = false;
   st->reset_pending = false;
   st->velocity_x    = 0.0f;
   st->velocity_y    = 0.0f;
   lib->common.set_logging_callback(ctx, XESS_LOGGING_LEVEL_WARNING, xess_common_log);
}

void xess_common_inited(const xess_lib_t *lib, xess_common_state_t *st,
      xess_context_t ctx)
{
   st->inited        = true;
   st->reset_pending = true;
   lib->common.set_jitter_scale(ctx, 1.0f, 1.0f);
}

void xess_common_scales(const xess_lib_t *lib, xess_common_state_t *st,
      xess_context_t ctx, const struct retro_hw_upscaler_inputs *inputs)
{
   float x = inputs->mv_scale_x == 0.0f ? 1.0f : inputs->mv_scale_x;
   float y = inputs->mv_scale_y == 0.0f ? 1.0f : inputs->mv_scale_y;
   if (x == st->velocity_x && y == st->velocity_y)
      return;
   lib->common.set_velocity_scale(ctx, x, y);
   st->velocity_x = x;
   st->velocity_y = y;
}

void xess_common_failed(xess_common_state_t *st, unsigned out_dims,
      unsigned mode, unsigned flags)
{
   st->failed_out_dims = out_dims;
   st->failed_mode     = mode;
   st->failed_flags    = flags;
   st->failed_valid    = true;
}

bool xess_common_has_failed(const xess_common_state_t *st, unsigned out_dims,
      unsigned mode, unsigned flags)
{
   return st->failed_valid && st->failed_out_dims == out_dims
      && st->failed_mode == mode && st->failed_flags == flags;
}

void xess_common_log(const char *message, int level)
{
   if (level >= XESS_LOGGING_LEVEL_ERROR)
      RARCH_ERR("[XeSS] %s\n", message);
   else
      RARCH_WARN("[XeSS] %s\n", message);
}

const char *xess_common_mode_name(unsigned mode)
{
   switch (mode)
   {
      case XESS_QUALITY_ULTRA_PERFORMANCE:  return "Ultra Performance";
      case XESS_QUALITY_PERFORMANCE:        return "Performance";
      case XESS_QUALITY_BALANCED:           return "Balanced";
      case XESS_QUALITY_QUALITY:            return "Quality";
      case XESS_QUALITY_ULTRA_QUALITY:      return "Ultra Quality";
      case XESS_QUALITY_ULTRA_QUALITY_PLUS: return "Ultra Quality Plus";
      case XESS_QUALITY_AA:                 return "Native AA";
      default:                              break;
   }
   return "?";
}
