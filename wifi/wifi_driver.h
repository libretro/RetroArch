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

#ifndef __WIFI_DRIVER__H
#define __WIFI_DRIVER__H

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <lists/string_list.h>

RETRO_BEGIN_DECLS

enum rarch_wifi_ctl_state
{
   RARCH_WIFI_CTL_NONE = 0,
   RARCH_WIFI_CTL_DESTROY,
   RARCH_WIFI_CTL_DEINIT,
   RARCH_WIFI_CTL_SET_ACTIVE,
   RARCH_WIFI_CTL_UNSET_ACTIVE,
   RARCH_WIFI_CTL_IS_ACTIVE,
   RARCH_WIFI_CTL_FIND_DRIVER,
   RARCH_WIFI_CTL_SET_CB,
   RARCH_WIFI_CTL_STOP,
   RARCH_WIFI_CTL_START,
   RARCH_WIFI_CTL_INIT
};

typedef struct wifi_driver
{
   void *(*init)(void);

   void (*free)(void *data);

   bool (*start)(void *data);
   void (*stop)(void *data);

   void (*scan)(void);
   void (*get_ssids)(struct string_list *list);
   bool (*ssid_is_online)(unsigned i);
   bool (*connect_ssid)(unsigned i, const char* passphrase);
   void (*tether_start_stop)(bool start, char* configfile);

   const char *ident;
} wifi_driver_t;

extern wifi_driver_t wifi_connmanctl;

/**
 * config_get_wifi_driver_options:
 *
 * Get an enumerated list of all wifi driver names,
 * separated by '|'.
 *
 * Returns: string listing of all wifi driver names,
 * separated by '|'.
 **/
const char* config_get_wifi_driver_options(void);

void driver_wifi_stop(void);

bool driver_wifi_start(void);

void driver_wifi_scan(void);

void driver_wifi_get_ssids(struct string_list *list);

bool driver_wifi_ssid_is_online(unsigned i);

bool driver_wifi_connect_ssid(unsigned i, const char* passphrase);

void driver_wifi_tether_start_stop(bool start, char* configfile);

bool wifi_driver_ctl(enum rarch_wifi_ctl_state state, void *data);

RETRO_END_DECLS

#endif
