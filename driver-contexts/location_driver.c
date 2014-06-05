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

static const location_driver_t *location_drivers[] = {
#ifdef ANDROID
   &location_android,
#endif
#if defined(IOS) || defined(OSX)
   &location_apple,
#endif
   NULL,
};

static int find_location_driver_index(const char *driver)
{
   unsigned i;
   for (i = 0; location_drivers[i]; i++)
      if (strcasecmp(driver, location_drivers[i]->ident) == 0)
         return i;
   return -1;
}

static void find_location_driver(void)
{
   int i = find_location_driver_index(g_settings.location.driver);
   if (i >= 0)
      driver.location = location_drivers[i];
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any location driver named \"%s\"\n", g_settings.location.driver);
      RARCH_LOG_OUTPUT("Available location drivers are:\n");
      for (d = 0; location_drivers[d]; d++)
         RARCH_LOG_OUTPUT("\t%s\n", location_drivers[d]->ident);

      rarch_fail(1, "find_location_driver()");
   }
}

void find_prev_location_driver(void)
{
   int i = find_location_driver_index(g_settings.location.driver);
   if (i > 0)
      strlcpy(g_settings.location.driver, location_drivers[i - 1]->ident, sizeof(g_settings.location.driver));
   else
      RARCH_WARN("Couldn't find any previous location driver (current one: \"%s\").\n", g_settings.location.driver);
}

void find_next_location_driver(void)
{
   int i = find_location_driver_index(g_settings.location.driver);
   if (i >= 0 && location_drivers[i + 1])
      strlcpy(g_settings.location.driver, location_drivers[i + 1]->ident, sizeof(g_settings.location.driver));
   else
      RARCH_WARN("Couldn't find any next location driver (current one: \"%s\").\n", g_settings.location.driver);
}

bool driver_location_start(void)
{
   if (driver.location && driver.location_data && driver.location->start)
   {
      if (g_settings.location.allow)
         return driver.location->start(driver.location_data);
      else
         msg_queue_push(g_extern.msg_queue, "Location is explicitly disabled.\n", 1, 180);
      return false;
   }
   else
      return false;
}

void driver_location_stop(void)
{
   if (driver.location && driver.location->stop && driver.location_data)
      driver.location->stop(driver.location_data);
}

void driver_location_set_interval(unsigned interval_msecs, unsigned interval_distance)
{
   if (driver.location && driver.location->set_interval && driver.location_data)
      driver.location->set_interval(driver.location_data, interval_msecs, interval_distance);
}

bool driver_location_get_position(double *lat, double *lon, double *horiz_accuracy,
      double *vert_accuracy)
{
   if (driver.location && driver.location->get_position && driver.location_data)
      return driver.location->get_position(driver.location_data, lat, lon, horiz_accuracy, vert_accuracy);

   *lat = 0.0;
   *lon = 0.0;
   *horiz_accuracy = 0.0;
   *vert_accuracy = 0.0;
   return false;
}

void init_location(void)
{
   // Resource leaks will follow if location interface is initialized twice.
   if (driver.location_data)
      return;

   find_location_driver();

   driver.location_data = location_init_func();

   if (!driver.location_data)
   {
      RARCH_ERR("Failed to initialize location driver. Will continue without location.\n");
      g_extern.location_active = false;
   }

   if (g_extern.system.location_callback.initialized)
      g_extern.system.location_callback.initialized();
}

void uninit_location(void)
{
   if (driver.location_data && driver.location)
   {
      if (g_extern.system.location_callback.deinitialized)
         g_extern.system.location_callback.deinitialized();

      if (driver.location->free)
         driver.location->free(driver.location_data);
   }
   driver.location_data = NULL;
}
