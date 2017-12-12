/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
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

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "wifi_driver.h"

#include "../configuration.h"
#include "../driver.h"
#include "../retroarch.h"
#include "../list_special.h"
#include "../verbosity.h"

static const wifi_driver_t *wifi_driver   = NULL;
static void *wifi_data                      = NULL;
static bool wifi_driver_active              = false;
static bool wifi_driver_data_own            = false;

static const wifi_driver_t *wifi_drivers[] = {
#ifdef HAVE_LAKKA
   &wifi_connmanctl,
#endif
   &wifi_null,
   NULL,
};

/**
 * wifi_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to wifi driver at index. Can be NULL
 * if nothing found.
 **/
const void *wifi_driver_find_handle(int idx)
{
   const void *drv = wifi_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * wifi_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of wifi driver at index. Can be NULL
 * if nothing found.
 **/
const char *wifi_driver_find_ident(int idx)
{
   const wifi_driver_t *drv = wifi_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * config_get_wifi_driver_options:
 *
 * Get an enumerated list of all wifi driver names,
 * separated by '|'.
 *
 * Returns: string listing of all wifi driver names,
 * separated by '|'.
 **/
const char* config_get_wifi_driver_options(void)
{
   return char_list_new_special(STRING_LIST_WIFI_DRIVERS, NULL);
}

void driver_wifi_stop(void)
{
   wifi_driver_ctl(RARCH_WIFI_CTL_START, NULL);
}

bool driver_wifi_start(void)
{
   return wifi_driver_ctl(RARCH_WIFI_CTL_START, NULL);
}

void driver_wifi_scan()
{
   wifi_driver->scan();
}

void driver_wifi_get_ssids(struct string_list* ssids)
{
   wifi_driver->get_ssids(ssids);
}

bool driver_wifi_ssid_is_online(unsigned i)
{
   return wifi_driver->ssid_is_online(i);
}

bool driver_wifi_connect_ssid(unsigned i, const char* passphrase)
{
   return wifi_driver->connect_ssid(i, passphrase);
}

bool wifi_driver_ctl(enum rarch_wifi_ctl_state state, void *data)
{
   settings_t        *settings = config_get_ptr();

   switch (state)
   {
      case RARCH_WIFI_CTL_DESTROY:
         wifi_driver_active   = false;
         wifi_driver_data_own = false;
         wifi_driver          = NULL;
         wifi_data            = NULL;
         break;
      case RARCH_WIFI_CTL_SET_OWN_DRIVER:
         wifi_driver_data_own = true;
         break;
      case RARCH_WIFI_CTL_UNSET_OWN_DRIVER:
         wifi_driver_data_own = false;
         break;
      case RARCH_WIFI_CTL_OWNS_DRIVER:
         return wifi_driver_data_own;
      case RARCH_WIFI_CTL_SET_ACTIVE:
         wifi_driver_active = true;
         break;
      case RARCH_WIFI_CTL_FIND_DRIVER:
         {
            int i;
            driver_ctx_info_t drv;

            drv.label = "wifi_driver";
            drv.s     = settings->arrays.wifi_driver;

            driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

            i = (int)drv.len;

            if (i >= 0)
               wifi_driver = (const wifi_driver_t*)wifi_driver_find_handle(i);
            else
            {
               unsigned d;
               RARCH_ERR("Couldn't find any wifi driver named \"%s\"\n",
                     settings->arrays.wifi_driver);
               RARCH_LOG_OUTPUT("Available wifi drivers are:\n");
               for (d = 0; wifi_driver_find_handle(d); d++)
                  RARCH_LOG_OUTPUT("\t%s\n", wifi_driver_find_ident(d));

               RARCH_WARN("Going to default to first wifi driver...\n");

               wifi_driver = (const wifi_driver_t*)wifi_driver_find_handle(0);

               if (!wifi_driver)
                  retroarch_fail(1, "find_wifi_driver()");
            }
         }
         break;
      case RARCH_WIFI_CTL_UNSET_ACTIVE:
         wifi_driver_active = false;
         break;
      case RARCH_WIFI_CTL_IS_ACTIVE:
        return wifi_driver_active;
      case RARCH_WIFI_CTL_DEINIT:
        if (wifi_data && wifi_driver)
        {
           if (wifi_driver->free)
              wifi_driver->free(wifi_data);
        }

        wifi_data = NULL;
        break;
      case RARCH_WIFI_CTL_STOP:
        if (     wifi_driver
              && wifi_driver->stop
              && wifi_data)
           wifi_driver->stop(wifi_data);
        break;
      case RARCH_WIFI_CTL_START:
        if (wifi_driver && wifi_data && wifi_driver->start)
        {
           if (settings->bools.wifi_allow)
              return wifi_driver->start(wifi_data);
        }
        return false;
      case RARCH_WIFI_CTL_SET_CB:
        {
           /*struct retro_wifi_callback *cb =
              (struct retro_wifi_callback*)data;
           wifi_cb          = *cb;*/
        }
        break;
      case RARCH_WIFI_CTL_INIT:
        /* Resource leaks will follow if wifi is initialized twice. */
        if (wifi_data)
           return false;

        wifi_driver_ctl(RARCH_WIFI_CTL_FIND_DRIVER, NULL);

        wifi_data = wifi_driver->init();

        if (!wifi_data)
        {
           RARCH_ERR("Failed to initialize wifi driver. Will continue without wifi.\n");
           wifi_driver_ctl(RARCH_WIFI_CTL_UNSET_ACTIVE, NULL);
        }

        /*if (wifi_cb.initialized)
           wifi_cb.initialized();*/
        break;
      default:
         break;
   }

   return false;
}
