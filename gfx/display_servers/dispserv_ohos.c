/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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
#include "../video_display_server.h"
#include "../../frontend/drivers/platform_unix.h"

/* FORWARD DECLARATIONS */
int system_property_get(const char *cmd, const char *args, char *value);

static void* ohos_display_server_init(void) { return NULL; }
static void ohos_display_server_destroy(void *data) { }
static bool ohos_display_server_set_window_opacity(void *data, unsigned opacity) { return true; }
static bool ohos_display_server_set_window_progress(void *data, int progress, bool finished) { return true; }
static uint32_t ohos_display_server_get_flags(void *data) { return 0; }

static void ohos_display_server_set_screen_orientation(void *data,
      enum rotation rotation)
{
   if (!g_ohos)
      return;
    ohos_send_native_event(EVENT_NATIVE_SET_SCREEN_ORIENTATION, rotation);
}

bool ohos_display_get_metrics(void *data,
	enum display_metric_types type, float *value)
{
   static int dpi = -1;

   switch (type)
   {
      case DISPLAY_METRIC_MM_WIDTH:
      case DISPLAY_METRIC_MM_HEIGHT:
         return false;
      case DISPLAY_METRIC_DPI:
         if (dpi == -1)
         {
            if (g_ohos->startParams->DPI == 0)
               goto dpi_fallback;
            dpi = g_ohos->startParams->DPI;
         }
         *value = (float)dpi;
         break;
      case DISPLAY_METRIC_NONE:
      default:
         *value = 0;
         return false;
   }

   return true;

dpi_fallback:
   /* add a fallback in case the device doesn't report DPI.
    * Hopefully fixes issues with the moto G2. */
   dpi    = 90;
   *value = (float)dpi;
   return true;
}

bool ohos_display_has_focus(void *data)
{
   bool                    focused = false;
   struct ohos_app *ohos_app = (struct ohos_app*)g_ohos;
   if (!ohos_app)
      return true;

   slock_lock(ohos_app->mutex);
   focused = !ohos_app->unfocused;
   slock_unlock(ohos_app->mutex);

   return focused;
}

const video_display_server_t dispserv_ohos = {
   ohos_display_server_init,
   ohos_display_server_destroy,
   ohos_display_server_set_window_opacity,
   ohos_display_server_set_window_progress,
   NULL, /* set_window_decorations */
   NULL, /* set_resolution */
   NULL, /* get_resolution_list */
   NULL, /* get_output_options */
   ohos_display_server_set_screen_orientation,
   NULL, /* get_screen_orientation */
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   ohos_display_get_metrics,
   ohos_display_server_get_flags,
   "ohos"
};
