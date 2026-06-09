/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - RetroArch
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

#include <stddef.h>
#include <boolean.h>
#include "../video_display_server.h"
#include "../../uwp/uwp_func.h"

static void *uwp_display_server_init(void) { return NULL; }
static void  uwp_display_server_destroy(void *data) { }

static float uwp_display_server_get_refresh_rate(void *data)
{
   return uwp_get_refresh_rate();
}

static void uwp_display_server_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *s, size_t len)
{
   if (width)
      *width  = uwp_get_width();
   if (height)
      *height = uwp_get_height();
}

static bool uwp_display_server_get_metrics(void *data,
      enum display_metric_types type, float *value)
{
   float dpi;

   if (!value)
      return false;

   switch (type)
   {
      case DISPLAY_METRIC_PIXEL_WIDTH:
         *value = (float)uwp_get_width();
         return true;
      case DISPLAY_METRIC_PIXEL_HEIGHT:
         *value = (float)uwp_get_height();
         return true;
      case DISPLAY_METRIC_MM_WIDTH:
         {
            int pixels_x   = uwp_get_width();
            dpi             = uwp_get_dpi();
            if (dpi > 0.0f)
               *value = (float)(254 * pixels_x) / (dpi * 10.0f);
            else
               *value = 0.0f;
         }
         return true;
      case DISPLAY_METRIC_MM_HEIGHT:
         {
            int pixels_y   = uwp_get_height();
            dpi             = uwp_get_dpi();
            if (dpi > 0.0f)
               *value = (float)(254 * pixels_y) / (dpi * 10.0f);
            else
               *value = 0.0f;
         }
         return true;
      case DISPLAY_METRIC_DPI:
         *value = uwp_get_dpi();
         return (*value > 0.0f);
      case DISPLAY_METRIC_NONE:
      default:
         *value = 0.0f;
         break;
   }
   return false;
}

const video_display_server_t dispserv_uwp = {
   uwp_display_server_init,
   uwp_display_server_destroy,
   NULL, /* set_window_opacity */
   NULL, /* set_window_progress */
   NULL, /* set_window_decorations */
   NULL, /* set_resolution */
   NULL, /* get_resolution_list */
   NULL, /* get_output_options */
   NULL, /* set_screen_orientation */
   NULL, /* get_screen_orientation */
   uwp_display_server_get_refresh_rate,
   uwp_display_server_get_video_output_size,
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   uwp_display_server_get_metrics,
   NULL, /* get_flags */
   "uwp"
};
