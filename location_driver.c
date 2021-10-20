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

#include "configuration.h"
#include "driver.h"
#include "list_special.h"
#include "location_driver.h"
#include "retroarch.h"
#include "verbosity.h"

static const location_driver_t *rarch_location_driver;
static void *rarch_location_data;

static location_driver_t location_null = {
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   NULL,
   "null",
};

const location_driver_t *location_drivers[] = {
#ifdef ANDROID
   &location_android,
#endif
   &location_null,
   NULL,
};

const char *config_get_location_driver_options(void)
{
   return char_list_new_special(STRING_LIST_LOCATION_DRIVERS, NULL);
}

void location_driver_find_driver(
      settings_t *settings,
      const char *prefix,
      bool verbosity_enabled)
{
   int i                        = (int)driver_find_index(
         "location_driver",
         settings->arrays.location_driver);

   if (i >= 0)
      rarch_location_driver  = (const location_driver_t*)location_drivers[i];
   else
   {
      if (verbosity_enabled)
      {
         unsigned d;
         RARCH_ERR("Couldn't find any %s named \"%s\"\n", prefix,
               settings->arrays.location_driver);
         RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
         for (d = 0; location_drivers[d]; d++)
            RARCH_LOG_OUTPUT("\t%s\n", location_drivers[d]->ident);

         RARCH_WARN("Going to default to first %s...\n", prefix);
      }

      rarch_location_driver = (const location_driver_t*)location_drivers[0];
   }
}

bool driver_location_start(void)
{
   if (     rarch_location_driver
         && rarch_location_data
         && rarch_location_driver->start)
   {
      settings_t *settings = config_get_ptr();
      bool location_allow  = settings->bools.location_allow;
      if (location_allow)
         return rarch_location_driver->start(rarch_location_data);

      runloop_msg_queue_push("Location is explicitly disabled.\n",
            1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT,
            MESSAGE_QUEUE_CATEGORY_INFO);
   }
   return false;
}

void driver_location_stop(void)
{
   if (     rarch_location_driver
         && rarch_location_driver->stop
         && rarch_location_data)
      rarch_location_driver->stop(rarch_location_data);
}

void driver_location_set_interval(unsigned interval_msecs,
      unsigned interval_distance)
{
   if (     rarch_location_driver
         && rarch_location_driver->set_interval
         && rarch_location_data)
      rarch_location_driver->set_interval(rarch_location_data,
            interval_msecs, interval_distance);
}

bool driver_location_get_position(double *lat, double *lon,
      double *horiz_accuracy, double *vert_accuracy)
{
   if (     rarch_location_driver
         && rarch_location_driver->get_position
         && rarch_location_data)
      return rarch_location_driver->get_position(rarch_location_data,
            lat, lon, horiz_accuracy, vert_accuracy);

   *lat            = 0.0;
   *lon            = 0.0;
   *horiz_accuracy = 0.0;
   *vert_accuracy  = 0.0;
   return false;
}

bool init_location(
      void *data,
      settings_t *settings,
      bool verbosity_enabled)
{
   rarch_system_info_t *system = (rarch_system_info_t*)data;
   /* Resource leaks will follow if location 
      interface is initialized twice. */
   if (rarch_location_data)
      return true;

   location_driver_find_driver(settings,
         "location driver", verbosity_enabled);

   rarch_location_data = rarch_location_driver->init();

   if (!rarch_location_data)
   {
      RARCH_ERR("Failed to initialize location driver. Will continue without location.\n");
      return false;
   }

   if (system->location_cb.initialized)
      system->location_cb.initialized();
 
   return true;
}

void uninit_location(void *data)
{
	rarch_system_info_t *system = (rarch_system_info_t*)data;

   if (rarch_location_data && rarch_location_driver)
   {
      if (system->location_cb.deinitialized)
         system->location_cb.deinitialized();

      if (rarch_location_driver->free)
         rarch_location_driver->free(rarch_location_data);
   }

   rarch_location_data = NULL;
}

void destroy_location(void)
{
   rarch_location_driver                         = NULL;
}
