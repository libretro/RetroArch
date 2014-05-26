/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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
#ifdef IOS
   &camera_ios,
#endif
   NULL,
};

static int find_camera_driver_index(const char *driver)
{
   unsigned i;
   for (i = 0; camera_drivers[i]; i++)
      if (strcasecmp(driver, camera_drivers[i]->ident) == 0)
         return i;
   return -1;
}

static void find_camera_driver(void)
{
   int i = find_camera_driver_index(g_settings.camera.driver);
   if (i >= 0)
      driver.camera = camera_drivers[i];
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any camera driver named \"%s\"\n", g_settings.camera.driver);
      RARCH_LOG_OUTPUT("Available camera drivers are:\n");
      for (d = 0; camera_drivers[d]; d++)
         RARCH_LOG_OUTPUT("\t%s\n", camera_drivers[d]->ident);

      rarch_fail(1, "find_camera_driver()");
   }
}

void find_prev_camera_driver(void)
{
   int i = find_camera_driver_index(g_settings.camera.driver);
   if (i > 0)
      strlcpy(g_settings.camera.driver, camera_drivers[i - 1]->ident, sizeof(g_settings.camera.driver));
   else
      RARCH_WARN("Couldn't find any previous camera driver (current one: \"%s\").\n", g_settings.camera.driver);
}

void find_next_camera_driver(void)
{
   int i = find_camera_driver_index(g_settings.camera.driver);
   if (i >= 0 && camera_drivers[i + 1])
      strlcpy(g_settings.camera.driver, camera_drivers[i + 1]->ident, sizeof(g_settings.camera.driver));
   else
      RARCH_WARN("Couldn't find any next camera driver (current one: \"%s\").\n", g_settings.camera.driver);
}

bool driver_camera_start(void)
{
   if (driver.camera && driver.camera_data && driver.camera->start)
   {
      if (g_settings.camera.allow)
         return driver.camera->start(driver.camera_data);
      else
         msg_queue_push(g_extern.msg_queue, "Camera is explicitly disabled.\n", 1, 180);
      return false;
   }
   else
      return false;
}

void driver_camera_stop(void)
{
   if (driver.camera && driver.camera_data)
      driver.camera->stop(driver.camera_data);
}

void driver_camera_poll(void)
{
   if (driver.camera && driver.camera_data)
   {
      driver.camera->poll(driver.camera_data,
            g_extern.system.camera_callback.frame_raw_framebuffer,
            g_extern.system.camera_callback.frame_opengl_texture);
   }
}

void init_camera(void)
{
   // Resource leaks will follow if camera is initialized twice.
   if (driver.camera_data)
      return;

   find_camera_driver();

   driver.camera_data = camera_init_func(
         *g_settings.camera.device ? g_settings.camera.device : NULL,
         g_extern.system.camera_callback.caps,
         g_settings.camera.width ? g_settings.camera.width : g_extern.system.camera_callback.width,
         g_settings.camera.height ? g_settings.camera.height : g_extern.system.camera_callback.height);

   if (!driver.camera_data)
   {
      RARCH_ERR("Failed to initialize camera driver. Will continue without camera.\n");
      g_extern.camera_active = false;
   }

   if (g_extern.system.camera_callback.initialized)
      g_extern.system.camera_callback.initialized();
}

void uninit_camera(void)
{
   if (driver.camera_data && driver.camera)
   {
      if (g_extern.system.camera_callback.deinitialized)
         g_extern.system.camera_callback.deinitialized();
      driver.camera->free(driver.camera_data);
   }
}
