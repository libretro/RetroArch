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

static void* android_display_server_init(void)
{
   return NULL;
}

static void android_display_server_destroy(void *data)
{
   (void)data;
}

static bool android_display_server_set_window_opacity(void *data, unsigned opacity)
{
   (void)data;
   (void)opacity;
   return true;
}

static bool android_display_server_set_window_progress(void *data, int progress, bool finished)
{
   (void)data;
   (void)progress;
   (void)finished;
   return true;
}

static void android_display_server_set_screen_orientation(enum rotation rotation)
{
   JNIEnv *env = jni_thread_getenv();

   if (!env || !g_android)
      return;

   if (g_android->setScreenOrientation)
      CALL_VOID_METHOD_PARAM(env, g_android->activity->clazz,
            g_android->setScreenOrientation, rotation);
}

static uint32_t android_display_server_get_flags(void *data)
{
   uint32_t             flags   = 0;

   return flags;
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
