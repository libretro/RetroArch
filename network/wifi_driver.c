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

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../driver.h"
#include "../list_special.h"
#include "../retroarch.h"
#include "../runloop.h"
#include "../verbosity.h"

#include "wifi_driver.h"

wifi_driver_t wifi_null = {
   NULL, /* init */
   NULL, /* free */
   NULL, /* start */
   NULL, /* stop */
   NULL, /* enable */
   NULL, /* connection_info */
   NULL, /* scan */
   NULL, /* get_ssids */
   NULL, /* ssid_is_online */
   NULL, /* connect_ssid */
   NULL, /* disconnect_ssid */
   NULL, /* tether_start_stop */
   "null",
};

const wifi_driver_t *wifi_drivers[] = {
#ifdef HAVE_LAKKA
   &wifi_connmanctl,
#endif
#ifdef HAVE_WIFI
   &wifi_nmcli,
#endif
   &wifi_null,
   NULL,
};

static wifi_driver_state_t wifi_driver_st = {0}; /* double alignment */

wifi_driver_state_t *wifi_state_get_ptr(void)
{
   return &wifi_driver_st;
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

void driver_wifi_scan(void)
{
   wifi_driver_state_t *wifi_st = &wifi_driver_st;
   if (wifi_st && wifi_st->drv)
      wifi_st->drv->scan(wifi_st->data);
}

bool driver_wifi_enable(bool enabled)
{
   wifi_driver_state_t *wifi_st = &wifi_driver_st;
   if (wifi_st && wifi_st->drv)
      return wifi_st->drv->enable(wifi_st->data, enabled);
   return false;
}

bool driver_wifi_connection_info(wifi_network_info_t *netinfo)
{
   wifi_driver_state_t *wifi_st = &wifi_driver_st;
   if (wifi_st && wifi_st->drv)
      return wifi_st->drv->connection_info(wifi_st->data, netinfo);
   return false;
}

wifi_network_scan_t* driver_wifi_get_ssids(void)
{
   wifi_driver_state_t *wifi_st = &wifi_driver_st;
   if (wifi_st && wifi_st->drv)
      return wifi_st->drv->get_ssids(wifi_st->data);
   return NULL;
}

bool driver_wifi_ssid_is_online(unsigned i)
{
   wifi_driver_state_t *wifi_st = &wifi_driver_st;
   if (wifi_st && wifi_st->drv)
      return wifi_st->drv->ssid_is_online(wifi_st->data, i);
   return false;
}

bool driver_wifi_connect_ssid(const wifi_network_info_t* net)
{
   wifi_driver_state_t *wifi_st = &wifi_driver_st;
   if (wifi_st && wifi_st->drv)
      return wifi_st->drv->connect_ssid(wifi_st->data, net);
   return false;
}

bool driver_wifi_disconnect_ssid(const wifi_network_info_t* net)
{
   wifi_driver_state_t *wifi_st = &wifi_driver_st;
   if (wifi_st && wifi_st->drv)
      return wifi_st->drv->disconnect_ssid(wifi_st->data, net);
   return false;
}

void driver_wifi_tether_start_stop(bool start, char* configfile)
{
   wifi_driver_state_t *wifi_st = &wifi_driver_st;
   if (wifi_st && wifi_st->drv)
      wifi_st->drv->tether_start_stop(wifi_st->data, start, configfile);
}

bool wifi_driver_ctl(enum rarch_wifi_ctl_state state, void *data)
{
   wifi_driver_state_t     *wifi_st = &wifi_driver_st;
   settings_t             *settings = config_get_ptr();

   switch (state)
   {
      case RARCH_WIFI_CTL_DESTROY:
         wifi_st->active          = false;
         wifi_st->drv             = NULL;
         wifi_st->data            = NULL;
         break;
      case RARCH_WIFI_CTL_SET_ACTIVE:
         wifi_st->active          = true;
         break;
      case RARCH_WIFI_CTL_FIND_DRIVER:
         {
            const char *prefix    = "wifi driver";
            int i                 = (int)driver_find_index(
                  "wifi_driver",
                  settings->arrays.wifi_driver);

            if (i >= 0)
               wifi_st->drv = (const wifi_driver_t*)wifi_drivers[i];
            else
            {
               if (verbosity_is_enabled())
               {
                  unsigned d;
                  RARCH_ERR("Couldn't find any %s named \"%s\"\n", prefix,
                        settings->arrays.wifi_driver);
                  RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
                  for (d = 0; wifi_drivers[d]; d++)
                     RARCH_LOG_OUTPUT("\t%s\n", wifi_drivers[d]->ident);

                  RARCH_WARN("Going to default to first %s...\n", prefix);
               }

               wifi_st->drv = (const wifi_driver_t*)wifi_drivers[0];

               if (!wifi_st->drv)
                  retroarch_fail(1, "find_wifi_driver()");
            }
         }
         break;
      case RARCH_WIFI_CTL_UNSET_ACTIVE:
         wifi_st->active = false;
         break;
      case RARCH_WIFI_CTL_IS_ACTIVE:
        return wifi_st->active;
      case RARCH_WIFI_CTL_DEINIT:
        if (wifi_st->data && wifi_st->drv)
        {
           if (wifi_st->drv->free)
              wifi_st->drv->free(wifi_st->data);
        }

        wifi_st->data = NULL;
        break;
      case RARCH_WIFI_CTL_STOP:
        if (     wifi_st->drv
              && wifi_st->drv->stop
              && wifi_st->data)
           wifi_st->drv->stop(wifi_st->data);
        break;
      case RARCH_WIFI_CTL_START:
        if (     wifi_st->drv
              && wifi_st->data
              && wifi_st->drv->start)
        {
           bool wifi_allow      = settings->bools.wifi_allow;
           if (wifi_allow)
              return wifi_st->drv->start(wifi_st->data);
        }
        return false;
      case RARCH_WIFI_CTL_INIT:
        /* Resource leaks will follow if wifi is initialized twice. */
        if (wifi_st->data)
           return false;

        wifi_driver_ctl(RARCH_WIFI_CTL_FIND_DRIVER, NULL);

        if (wifi_st->drv && wifi_st->drv->init)
        {
           wifi_st->data = wifi_st->drv->init();

           if (wifi_st->data)
           {
              wifi_st->drv->enable(wifi_st->data,
                 settings->bools.wifi_enabled);
           }
           else
           {
              RARCH_ERR("Failed to initialize wifi driver. Will continue without wifi.\n");
              wifi_driver_ctl(RARCH_WIFI_CTL_UNSET_ACTIVE, NULL);
           }
        }

        break;
      default:
         break;
   }

   return false;
}
