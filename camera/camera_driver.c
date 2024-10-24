/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2021 - Daniel De Matteis
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

#include <libretro.h>

#include "../configuration.h"
#include "../driver.h"
#include "../list_special.h"
#include "../runloop.h"
#include "../verbosity.h"

#include "camera_driver.h"

static void *nullcamera_init(const char *device, uint64_t caps,
      unsigned width, unsigned height) { return (void*)-1; }
static void nullcamera_free(void *data) { }
static void nullcamera_stop(void *data) { }
static bool nullcamera_start(void *data) { return true; }
static bool nullcamera_poll(void *a,
      retro_camera_frame_raw_framebuffer_t b,
      retro_camera_frame_opengl_texture_t c) { return true; }

static camera_driver_t camera_null = {
   nullcamera_init,
   nullcamera_free,
   nullcamera_start,
   nullcamera_stop,
   nullcamera_poll,
   "null",
};

const camera_driver_t *camera_drivers[] = {
#ifdef HAVE_V4L2
   &camera_v4l2,
#endif
#ifdef EMSCRIPTEN
   &camera_rwebcam,
#endif
#ifdef ANDROID
   &camera_android,
#endif
   &camera_null,
   NULL,
};

static camera_driver_state_t camera_driver_st     = {0};

camera_driver_state_t *camera_state_get_ptr(void)
{
   return &camera_driver_st;
}

/**
 * config_get_camera_driver_options:
 *
 * Get an enumerated list of all camera driver names,
 * separated by '|'.
 *
 * Returns: string listing of all camera driver names,
 * separated by '|'.
 **/
const char *config_get_camera_driver_options(void)
{
   return char_list_new_special(STRING_LIST_CAMERA_DRIVERS, NULL);
}

bool driver_camera_start(void)
{
   camera_driver_state_t *camera_st = &camera_driver_st;
   if (     camera_st
         && camera_st->data
         && camera_st->driver
         && camera_st->driver->start)
   {
      settings_t *settings = config_get_ptr();
      bool camera_allow    = settings->bools.camera_allow;
      if (camera_allow)
         return camera_st->driver->start(camera_st->data);

      runloop_msg_queue_push(
            "Camera is explicitly disabled.\n", 1, 180, false,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
   return true;
}

void driver_camera_stop(void)
{
   camera_driver_state_t *camera_st = &camera_driver_st;
   if (     camera_st->driver
         && camera_st->driver->stop
         && camera_st->data)
      camera_st->driver->stop(camera_st->data);
}

bool camera_driver_find_driver(const char *prefix,
      bool verbosity_enabled)
{
   settings_t *settings         = config_get_ptr();
   camera_driver_state_t 
      *camera_st                = &camera_driver_st;
   int i                        = (int)driver_find_index(
         "camera_driver",
         settings->arrays.camera_driver);

   if (i >= 0)
      camera_st->driver = (const camera_driver_t*)camera_drivers[i];
   else
   {
      if (verbosity_enabled)
      {
         unsigned d;
         RARCH_ERR("Couldn't find any %s named \"%s\"\n", prefix,
               settings->arrays.camera_driver);
         RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
         for (d = 0; camera_drivers[d]; d++)
         {
            if (camera_drivers[d])
            {
               RARCH_LOG_OUTPUT("\t%s\n", camera_drivers[d]->ident);
            }
         }

         RARCH_WARN("Going to default to first %s...\n", prefix);
      }

      if (!(camera_st->driver = (const camera_driver_t*)camera_drivers[0]))
         return false;
   }
   return true;
}
