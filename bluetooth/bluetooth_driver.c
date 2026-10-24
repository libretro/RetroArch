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
#include <stdlib.h>

#include <retro_atomic.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../driver.h"
#include "../list_special.h"
#include "../retroarch.h"
#include "../runloop.h"
#include "../verbosity.h"

#include "bluetooth_driver.h"

static bluetooth_driver_t bluetooth_null = {
   NULL, /* init */
   NULL, /* free */
   NULL, /* scan_begin */
   NULL, /* scan_end */
   NULL, /* get_devices */
   NULL, /* device_is_connected */
   NULL, /* device_get_sublabel */
   NULL, /* connect_device */
   NULL, /* remove_device */
   "null",
};

const bluetooth_driver_t *bluetooth_drivers[] = {
#ifdef HAVE_BLUETOOTH
#ifdef _WIN32
   &bluetooth_windows,
#else
   &bluetooth_bluetoothctl,
#endif
#ifdef HAVE_DBUS
   &bluetooth_bluez,
#endif
#endif
   &bluetooth_null,
   NULL,
};

static bluetooth_driver_state_t bluetooth_driver_st = {0};

/* The frontend's hold, and one for each scan running: whichever lets
 * go last frees the driver. */
struct bluetooth_hold
{
   const bluetooth_driver_t *drv;
   void *data;
   retro_atomic_int_t refs;
};

bluetooth_driver_state_t *bluetooth_state_get_ptr(void)
{
   return &bluetooth_driver_st;
}

/**
 * config_get_bluetooth_driver_options:
 *
 * Get an enumerated list of all bluetooth driver names,
 * separated by '|'.
 *
 * Returns: string listing of all bluetooth driver names,
 * separated by '|'.
 **/
const char* config_get_bluetooth_driver_options(void)
{
   return char_list_new_special(STRING_LIST_BLUETOOTH_DRIVERS, NULL);
}

struct bluetooth_hold *driver_bluetooth_scan_hold(void)
{
   bluetooth_driver_state_t *bt_st = &bluetooth_driver_st;
   if (!bt_st->active || !bt_st->hold)
      return NULL;
   retro_atomic_inc_int(&bt_st->hold->refs);
   return bt_st->hold;
}

void driver_bluetooth_scan_release(struct bluetooth_hold *hold)
{
   if (!hold || retro_atomic_fetch_sub_int(&hold->refs, 1) != 1)
      return;
   if (hold->drv->free)
      hold->drv->free(hold->data);
   free(hold);
}

void driver_bluetooth_scan_begin(struct bluetooth_hold *hold)
{
   if (hold && hold->drv->scan_begin)
      hold->drv->scan_begin(hold->data);
}

void driver_bluetooth_scan_end(struct bluetooth_hold *hold)
{
   if (hold && hold->drv->scan_end)
      hold->drv->scan_end(hold->data);
}

bool driver_bluetooth_scan_done(struct bluetooth_hold *hold)
{
   return hold && hold->drv->scan_done && hold->drv->scan_done(hold->data);
}

bool driver_bluetooth_adapter_generic(void)
{
   bluetooth_driver_state_t *bt_st = &bluetooth_driver_st;
   return      bt_st->active
           &&  bt_st->drv->adapter_generic
           &&  bt_st->drv->adapter_generic(bt_st->data);
}

void driver_bluetooth_get_devices(struct string_list* devices)
{
   bluetooth_driver_state_t *bt_st = &bluetooth_driver_st;
   if (     bt_st
        &&  bt_st->active
        &&  bt_st->drv->get_devices )
      bt_st->drv->get_devices(bt_st->data, devices);
}

bool driver_bluetooth_device_is_connected(unsigned i)
{
   bluetooth_driver_state_t *bt_st = &bluetooth_driver_st;
   if (    bt_st
        && bt_st->active
        && bt_st->drv->device_is_connected )
      return bt_st->drv->device_is_connected(bt_st->data, i);
   return false;
}

void driver_bluetooth_device_get_sublabel(char *s, unsigned i, size_t len)
{
   bluetooth_driver_state_t *bt_st = &bluetooth_driver_st;
   if (     bt_st
        &&  bt_st->active
        &&  bt_st->drv->device_get_sublabel )
      bt_st->drv->device_get_sublabel(bt_st->data, s, i, len);
}

bool driver_bluetooth_connect_device(unsigned i)
{
   bluetooth_driver_state_t *bt_st = &bluetooth_driver_st;
   if (bt_st->active)
      return bt_st->drv->connect_device(bt_st->data, i);
   return false;
}

bool driver_bluetooth_remove_device(unsigned i)
{
   bluetooth_driver_state_t *bt_st = &bluetooth_driver_st;
   if (bt_st->active)
      return bt_st->drv->remove_device(bt_st->data, i);
   return false;
}

bool bluetooth_driver_ctl(enum rarch_bluetooth_ctl_state state, void *data)
{
   bluetooth_driver_state_t *bt_st  = &bluetooth_driver_st;
   settings_t             *settings = config_get_ptr();

   switch (state)
   {
      case RARCH_BLUETOOTH_CTL_DESTROY:
         bt_st->drv              = NULL;
         bt_st->data             = NULL;
         bt_st->active           = false;
         break;
      case RARCH_BLUETOOTH_CTL_FIND_DRIVER:
         {
            const char *prefix   = "bluetooth driver";
            int i                = (int)driver_find_index(
                  "bluetooth_driver",
                  settings->arrays.bluetooth_driver);

            if (i >= 0)
               bt_st->drv        = (const bluetooth_driver_t*)bluetooth_drivers[i];
            else
            {
               if (verbosity_is_enabled())
               {
                  unsigned d;
                  RARCH_ERR("Couldn't find any %s named \"%s\"\n", prefix,
                        settings->arrays.bluetooth_driver);
                  RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
                  for (d = 0; bluetooth_drivers[d]; d++)
                     RARCH_LOG_OUTPUT("\t%s\n", bluetooth_drivers[d]->ident);

                  RARCH_WARN("Going to default to first %s...\n", prefix);
               }

               bt_st->drv = (const bluetooth_driver_t*)bluetooth_drivers[0];

               if (!bt_st->drv)
                  retroarch_fail(1, "find_bluetooth_driver()");
            }
         }
         break;
      case RARCH_BLUETOOTH_CTL_DEINIT:
        if (bt_st->hold)
           driver_bluetooth_scan_release(bt_st->hold);
        else if (bt_st->data && bt_st->drv && bt_st->drv->free)
           bt_st->drv->free(bt_st->data);

        bt_st->hold   = NULL;
        bt_st->data   = NULL;
        bt_st->active = false;
        break;
      case RARCH_BLUETOOTH_CTL_INIT:
        /* Resource leaks will follow if bluetooth is initialized twice. */
        if (bt_st->data)
           return false;

        bluetooth_driver_ctl(RARCH_BLUETOOTH_CTL_FIND_DRIVER, NULL);

        if (bt_st->drv && bt_st->drv->init)
        {
           bt_st->active = true;
           bt_st->data   = bt_st->drv->init();

           if (bt_st->data && !(bt_st->hold = (struct bluetooth_hold*)
                     calloc(1, sizeof(*bt_st->hold))))
           {
              if (bt_st->drv->free)
                 bt_st->drv->free(bt_st->data);
              bt_st->data = NULL;
           }
           if (!bt_st->data)
           {
              RARCH_ERR("Failed to initialize bluetooth driver. Will continue without bluetooth.\n");
              bt_st->active = false;
           }
           else
           {
              bt_st->hold->drv  = bt_st->drv;
              bt_st->hold->data = bt_st->data;
              retro_atomic_int_init(&bt_st->hold->refs, 1);
           }
        }
        else
           bt_st->active = false;
        break;
      default:
         break;
   }

   return false;
}
