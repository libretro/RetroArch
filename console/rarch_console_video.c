/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include <stdint.h>

#include "rarch_console_settings.h"
#include "rarch_console_video.h"

#if defined(HAVE_HLSL)
#include "../gfx/shader_hlsl.h"
#elif defined(HAVE_CG) && defined(HAVE_OPENGL)
#include "../gfx/shader_cg.h"
#endif

struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END] = {
   { "1:1",           1.0f },
   { "2:1",           2.0f },
   { "3:2",           1.5f },
   { "3:4",           0.75f },
   { "4:1",           4.0f },
   { "4:3",           1.3333f },
   { "4:4",           1.0f },
   { "5:4",           1.25f },
   { "6:5",           1.2f },
   { "7:9",           0.7777f },
   { "8:3",           2.6666f },
   { "8:7",           1.1428f },
   { "16:9",          1.7778f },
   { "16:10",         1.6f },
   { "16:15",         3.2f },
   { "19:12",         1.5833f },
   { "19:14",         1.3571f },
   { "30:17",         1.7647f },
   { "32:9",          3.5555f },
   { "Auto",          1.0f },
   { "Core Provided", 1.0f },
   { "Custom",        0.0f }
};

char rotation_lut[ASPECT_RATIO_END][32] =
{
   "Normal",
   "Vertical",
   "Flipped",
   "Flipped Rotated"
};

void rarch_set_auto_viewport(unsigned width, unsigned height)
{
   if(width == 0 || height == 0)
      return;

   unsigned aspect_x, aspect_y, len, highest, i;

   len = width < height ? width : height;
   highest = 1;
   for (i = 1; i < len; i++)
   {
      if ((width % i) == 0 && (height % i) == 0)
         highest = i;
   }

   aspect_x = width / highest;
   aspect_y = height / highest;

   snprintf(aspectratio_lut[ASPECT_RATIO_AUTO].name, sizeof(aspectratio_lut[ASPECT_RATIO_AUTO].name), "%d:%d (Auto)", aspect_x, aspect_y);
   aspectratio_lut[ASPECT_RATIO_AUTO].value = (float) aspect_x / aspect_y;
}

void rarch_set_core_viewport(void)
{
   if (!g_extern.console.emulator_initialized)
      return;

   // fallback to 1:1 pixel ratio if none provided
   if (g_extern.system.av_info.geometry.aspect_ratio == 0.0)
      aspectratio_lut[ASPECT_RATIO_CORE].value = (float) g_extern.system.av_info.geometry.base_width / g_extern.system.av_info.geometry.base_height;
   else
      aspectratio_lut[ASPECT_RATIO_CORE].value = g_extern.system.av_info.geometry.aspect_ratio;
}

#if defined(HAVE_HLSL) || defined(HAVE_CG) || defined(HAVE_GLSL)
void rarch_load_shader(unsigned slot, const char *path)
{
#if defined(HAVE_HLSL)
   hlsl_load_shader(slot, path);
#elif defined(HAVE_CG) && defined(HAVE_OPENGL)
   gl_cg_load_shader(slot, path);
#else
RARCH_WARN("Shader support is not implemented for this build.\n");
#endif

   if (g_extern.console.rmenu.state.msg_info.enable)
      rarch_settings_msg(S_MSG_SHADER_LOADING_SUCCEEDED, S_DELAY_180);
}
#endif
