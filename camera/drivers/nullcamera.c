/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2015 - Michael Lelli
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

#include "../camera_driver.h"

static void *nullcamera_init(const char *device, uint64_t caps,
      unsigned width, unsigned height)
{
   (void)device;
   return (void*)-1;
}

static void nullcamera_free(void *data)
{
   (void)data;
}

static bool nullcamera_start(void *data)
{
   (void)data;
   return true;
}

static void nullcamera_stop(void *data)
{
   (void)data;
}

static bool nullcamera_poll(void *data,
      retro_camera_frame_raw_framebuffer_t frame_raw_cb,
      retro_camera_frame_opengl_texture_t frame_gl_cb)
{
   (void)data;
   (void)frame_raw_cb;
   (void)frame_gl_cb;

   return true;
}

camera_driver_t camera_null = {
   nullcamera_init,
   nullcamera_free,
   nullcamera_start,
   nullcamera_stop,
   nullcamera_poll,
   "null",
};
