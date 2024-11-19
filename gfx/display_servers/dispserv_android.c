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

static void* android_display_server_init(void) { return NULL; }
static void android_display_server_destroy(void *data) { }
static bool android_display_server_set_window_opacity(void *data, unsigned opacity) { return true; }
static bool android_display_server_set_window_progress(void *data, int progress, bool finished) { return true; }
static uint32_t android_display_server_get_flags(void *data) { return 0; }

static void android_display_server_set_screen_orientation(void *data,
      enum rotation rotation)
{
   JNIEnv *env = jni_thread_getenv();

   if (!env || !g_android)
      return;

   if (g_android->setScreenOrientation)
      CALL_VOID_METHOD_PARAM(env, g_android->activity->clazz,
            g_android->setScreenOrientation, rotation);
}

static void android_display_dpi_get_density(char *s, size_t len)
{
   static bool inited_once             = false;
   static bool inited2_once            = false;
   static char string[PROP_VALUE_MAX]  = {0};
   static char string2[PROP_VALUE_MAX] = {0};
   if (!inited_once)
   {
      system_property_get("getprop", "ro.sf.lcd_density", string);
      inited_once = true;
   }

   if (!string_is_empty(string))
   {
      strlcpy(s, string, len);
      return;
   }

   if (!inited2_once)
   {
      system_property_get("wm", "density", string2);
      inited2_once = true;
   }

   strlcpy(s, string2, len);
}

bool android_display_get_metrics(void *data,
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
            char density[PROP_VALUE_MAX];
            android_display_dpi_get_density(density, sizeof(density));
            if (string_is_empty(density))
               goto dpi_fallback;
            if ((dpi = atoi(density)) <= 0)
               goto dpi_fallback;
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

bool android_display_has_focus(void *data)
{
   bool                    focused = false;
   struct android_app *android_app = (struct android_app*)g_android;
   if (!android_app)
      return true;

   slock_lock(android_app->mutex);
   focused = !android_app->unfocused;
   slock_unlock(android_app->mutex);

   return focused;
}

const video_display_server_t dispserv_android = {
   android_display_server_init,
   android_display_server_destroy,
   android_display_server_set_window_opacity,
   android_display_server_set_window_progress,
   NULL, /* set_window_decorations */
   NULL, /* set_resolution */
   NULL, /* get_resolution_list */
   NULL, /* get_output_options */
   android_display_server_set_screen_orientation,
   NULL, /* get_screen_orientation */
   android_display_server_get_flags,
   "android"
};
