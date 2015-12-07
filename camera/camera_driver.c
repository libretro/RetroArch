/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "camera_driver.h"
#include "../general.h"
#include "../string_list_special.h"
#include "../system.h"
#include "../verbosity.h"

static const camera_driver_t *camera_drivers[] = {
#ifdef HAVE_V4L2
   &camera_v4l2,
#endif
#ifdef EMSCRIPTEN
   &camera_rwebcam,
#endif
#ifdef ANDROID
   &camera_android,
#endif
#if defined(HAVE_AVFOUNDATION)
#if defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH)
    &camera_avfoundation,
#endif
#endif
   &camera_null,
   NULL,
};

static const camera_driver_t *camera_driver;
static void *camera_data;

/**
 * camera_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to camera driver at index. Can be NULL
 * if nothing found.
 **/
const void *camera_driver_find_handle(int idx)
{
   const void *drv = camera_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * camera_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of camera driver at index. Can be NULL
 * if nothing found.
 **/
const char *camera_driver_find_ident(int idx)
{
   const camera_driver_t *drv = camera_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
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
const char* config_get_camera_driver_options(void)
{
   return char_list_new_special(STRING_LIST_CAMERA_DRIVERS, NULL);
}

void find_camera_driver(void)
{
   settings_t *settings = config_get_ptr();
   int i = find_driver_index("camera_driver", settings->camera.driver);

   if (i >= 0)
      camera_driver = (const camera_driver_t*)camera_driver_find_handle(i);
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any camera driver named \"%s\"\n",
            settings->camera.driver);
      RARCH_LOG_OUTPUT("Available camera drivers are:\n");
      for (d = 0; camera_driver_find_handle(d); d++)
         RARCH_LOG_OUTPUT("\t%s\n", camera_driver_find_ident(d));
       
      RARCH_WARN("Going to default to first camera driver...\n");
       
      camera_driver = (const camera_driver_t*)camera_driver_find_handle(0);
       
      if (!camera_driver)
         retro_fail(1, "find_camera_driver()");
   }
}

/**
 * driver_camera_start:
 *
 * Starts camera driver interface.
 * Used by RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool driver_camera_start(void)
{
   settings_t *settings = config_get_ptr();

   if (camera_driver && camera_data && camera_driver->start)
   {
      if (settings->camera.allow)
         return camera_driver->start(camera_data);

      rarch_main_msg_queue_push(
            "Camera is explicitly disabled.\n", 1, 180, false);
   }
   return false;
}

/**
 * driver_camera_stop:
 *
 * Stops camera driver.
 * Used by RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
void driver_camera_stop(void)
{
   if (camera_driver && camera_driver->stop && camera_data)
      camera_driver->stop(camera_data);
}

/**
 * driver_camera_poll:
 *
 * Call camera driver's poll function.
 * Used by RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
void driver_camera_poll(void)
{
   rarch_system_info_t *system = rarch_system_info_get_ptr();

   if (camera_driver && camera_driver->poll && camera_data)
      camera_driver->poll(camera_data,
            system->camera_callback.frame_raw_framebuffer,
            system->camera_callback.frame_opengl_texture);
}

void init_camera(void)
{
   settings_t        *settings = config_get_ptr();
   rarch_system_info_t *system = rarch_system_info_get_ptr();

   /* Resource leaks will follow if camera is initialized twice. */
   if (camera_data)
      return;

   find_camera_driver();

   camera_data = camera_driver->init(
         *settings->camera.device ? settings->camera.device : NULL,
         system->camera_callback.caps,
         settings->camera.width ?
         settings->camera.width : system->camera_callback.width,
         settings->camera.height ?
         settings->camera.height : system->camera_callback.height);

   if (!camera_data)
   {
      RARCH_ERR("Failed to initialize camera driver. Will continue without camera.\n");
      camera_driver_ctl(RARCH_CAMERA_CTL_UNSET_ACTIVE, NULL);
   }

   if (system->camera_callback.initialized)
      system->camera_callback.initialized();
}

static void uninit_camera(void)
{
   rarch_system_info_t *system = rarch_system_info_get_ptr();

   if (camera_data && camera_driver)
   {
      if (system->camera_callback.deinitialized)
         system->camera_callback.deinitialized();

      if (camera_driver->free)
         camera_driver->free(camera_data);
   }

   camera_data = NULL;
}

bool camera_driver_ctl(enum rarch_camera_ctl_state state, void *data)
{
   static bool camera_driver_active              = false;
   static bool camera_driver_data_own            = false;

   switch (state)
   {
      case RARCH_CAMERA_CTL_DESTROY:
         camera_driver_active   = false;
         camera_driver_data_own = false;
         camera_driver          = NULL;
         break;
      case RARCH_CAMERA_CTL_SET_OWN_DRIVER:
         camera_driver_data_own = true;
         break;
      case RARCH_CAMERA_CTL_UNSET_OWN_DRIVER:
         camera_driver_data_own = false;
         break;
      case RARCH_CAMERA_CTL_OWNS_DRIVER:
         return camera_driver_data_own;
      case RARCH_CAMERA_CTL_SET_ACTIVE:
         camera_driver_active = true; 
         break;
      case RARCH_CAMERA_CTL_UNSET_ACTIVE:
         camera_driver_active = false; 
         break;
      case RARCH_CAMERA_CTL_IS_ACTIVE:
        return camera_driver_active; 
      case RARCH_CAMERA_CTL_DEINIT:
        uninit_camera();
        break;
      default:
         break;
   }
   
   return false;
}
