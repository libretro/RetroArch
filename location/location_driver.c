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

#include "../general.h"
#include "../string_list_special.h"

static const location_driver_t *location_drivers[] = {
#ifdef ANDROID
   &location_android,
#endif
#ifdef HAVE_CORELOCATION
#if defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH)
   &location_corelocation,
#endif
#endif
   &location_null,
   NULL,
};

/**
 * location_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to location driver at index. Can be NULL
 * if nothing found.
 **/
const void *location_driver_find_handle(int idx)
{
   const void *drv = location_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * location_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of location driver at index. Can be NULL
 * if nothing found.
 **/
const char *location_driver_find_ident(int idx)
{
   const location_driver_t *drv = location_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * config_get_location_driver_options:
 *
 * Get an enumerated list of all location driver names,
 * separated by '|'.
 *
 * Returns: string listing of all location driver names,
 * separated by '|'.
 **/
const char* config_get_location_driver_options(void)
{
   return char_list_new_special(STRING_LIST_LOCATION_DRIVERS, NULL);
}

void find_location_driver(void)
{
   driver_t *driver     = driver_get_ptr();
   settings_t *settings = config_get_ptr();
   int i                = find_driver_index("location_driver", settings->location.driver);

   if (i >= 0)
      driver->location = (const location_driver_t*)location_driver_find_handle(i);
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any location driver named \"%s\"\n",
            settings->location.driver);
      RARCH_LOG_OUTPUT("Available location drivers are:\n");
      for (d = 0; location_driver_find_handle(d); d++)
         RARCH_LOG_OUTPUT("\t%s\n", location_driver_find_ident(d));
       
      RARCH_WARN("Going to default to first location driver...\n");
       
      driver->location = (const location_driver_t*)location_driver_find_handle(0);

      if (!driver->location)
         retro_fail(1, "find_location_driver()");
   }
}

/**
 * driver_location_start:
 *
 * Starts location driver interface..
 * Used by RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool driver_location_start(void)
{
   driver_t *driver     = driver_get_ptr();
   settings_t *settings = config_get_ptr();

   if (driver->location && driver->location_data && driver->location->start)
   {
      if (settings->location.allow)
         return driver->location->start(driver->location_data);

      rarch_main_msg_queue_push("Location is explicitly disabled.\n", 1, 180, true);
   }
   return false;
}

/**
 * driver_location_stop:
 *
 * Stops location driver interface..
 * Used by RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
void driver_location_stop(void)
{
   driver_t *driver = driver_get_ptr();
   if (driver->location && driver->location->stop && driver->location_data)
      driver->location->stop(driver->location_data);
}

/**
 * driver_location_set_interval:
 * @interval_msecs     : Interval time in milliseconds.
 * @interval_distance  : Distance at which to update.
 *
 * Sets interval update time for location driver interface.
 * Used by RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE.
 **/
void driver_location_set_interval(unsigned interval_msecs,
      unsigned interval_distance)
{
   driver_t *driver = driver_get_ptr();
   if (driver->location && driver->location->set_interval
         && driver->location_data)
      driver->location->set_interval(driver->location_data,
            interval_msecs, interval_distance);
}

/**
 * driver_location_get_position:
 * @lat                : Latitude of current position.
 * @lon                : Longitude of current position.
 * @horiz_accuracy     : Horizontal accuracy.
 * @vert_accuracy      : Vertical accuracy.
 *
 * Gets current positioning information from 
 * location driver interface.
 * Used by RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE.
 *
 * Returns: bool (1) if successful, otherwise false (0).
 **/
bool driver_location_get_position(double *lat, double *lon,
      double *horiz_accuracy, double *vert_accuracy)
{
   driver_t *driver = driver_get_ptr();
   if (driver->location && driver->location->get_position
         && driver->location_data)
      return driver->location->get_position(driver->location_data,
            lat, lon, horiz_accuracy, vert_accuracy);

   *lat = 0.0;
   *lon = 0.0;
   *horiz_accuracy = 0.0;
   *vert_accuracy = 0.0;
   return false;
}

void init_location(void)
{
   driver_t            *driver = driver_get_ptr();
   rarch_system_info_t *system = rarch_system_info_get_ptr();

   /* Resource leaks will follow if location interface is initialized twice. */
   if (driver->location_data)
      return;

   find_location_driver();

   driver->location_data = driver->location->init();

   if (!driver->location_data)
   {
      RARCH_ERR("Failed to initialize location driver. Will continue without location.\n");
      driver->location_active = false;
   }

   if (system->location_callback.initialized)
      system->location_callback.initialized();
}

void uninit_location(void)
{
   driver_t            *driver = driver_get_ptr();
   rarch_system_info_t *system = rarch_system_info_get_ptr();

   if (driver->location_data && driver->location)
   {
      if (system->location_callback.deinitialized)
         system->location_callback.deinitialized();

      if (driver->location->free)
         driver->location->free(driver->location_data);
   }
   driver->location_data = NULL;
}
