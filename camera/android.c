/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 *  Copyright (C) 2012-2013 - Michael Lelli
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

#include "../driver.h"
#include "../android/native/jni/jni_macros.h"

static void *android_init(const char *device, uint64_t caps, unsigned width, unsigned height)
{
   (void)device;
   (void)caps;
   (void)width;
   (void)height;
   return (void*)0;
}

static void android_free(void *data)
{
   struct android_app *android_app = (struct android_app*)g_android;
   (void)android_app;
   (void)data;
}

static bool android_start(void *data)
{
   struct android_app *android_app = (struct android_app*)g_android;
   (void)android_app;
   (void)data;
   return true;
}

static void android_stop(void *data)
{
   struct android_app *android_app = (struct android_app*)g_android;
   (void)android_app;
   (void)data;
}

static bool android_poll(void *data, retro_camera_frame_raw_framebuffer_t frame_raw_cb,
      retro_camera_frame_opengl_texture_t frame_gl_cb)
{
   struct android_app *android_app = (struct android_app*)g_android;
   (void)android_app;
   (void)data;
   return true;
}

const camera_driver_t camera_android = {
   android_init,
   android_free,
   android_start,
   android_stop,
   android_poll,
   "android",
};
