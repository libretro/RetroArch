/*  RetroArch - A frontend for libretro.
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

#ifndef __BLUETOOTH_DRIVER__H
#define __BLUETOOTH_DRIVER__H

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <lists/string_list.h>

RETRO_BEGIN_DECLS

enum rarch_bluetooth_ctl_state
{
   RARCH_BLUETOOTH_CTL_NONE = 0,
   RARCH_BLUETOOTH_CTL_DESTROY,
   RARCH_BLUETOOTH_CTL_DEINIT,
   RARCH_BLUETOOTH_CTL_FIND_DRIVER,
   RARCH_BLUETOOTH_CTL_INIT
};

typedef struct bluetooth_driver
{
   void *(*init)(void);

   void (*free)(void *data);

   void (*scan)(void *data);
   void (*get_devices)(void *data, struct string_list *list);
   bool (*device_is_connected)(void *data, unsigned i);
   void (*device_get_sublabel)(void *data, char *s, unsigned i, size_t len);
   bool (*connect_device)(void *data, unsigned i);
   bool (*remove_device)(void *data, unsigned i);

   const char *ident;
} bluetooth_driver_t;

extern bluetooth_driver_t bluetooth_bluetoothctl;
extern bluetooth_driver_t bluetooth_bluez;

typedef struct
{
   const bluetooth_driver_t *drv;
   void *data;
   bool active;
} bluetooth_driver_state_t;

/**
 * config_get_bluetooth_driver_options:
 *
 * Get an enumerated list of all bluetooth driver names,
 * separated by '|'.
 *
 * Returns: string listing of all bluetooth driver names,
 * separated by '|'.
 **/
const char* config_get_bluetooth_driver_options(void);

void driver_bluetooth_scan(void);

void driver_bluetooth_get_devices(struct string_list *list);

bool driver_bluetooth_device_is_connected(unsigned i);

void driver_bluetooth_device_get_sublabel(char *s, unsigned i, size_t len);

bool driver_bluetooth_connect_device(unsigned i);

bool driver_bluetooth_remove_device(unsigned i);

bool bluetooth_driver_ctl(enum rarch_bluetooth_ctl_state state, void *data);

bluetooth_driver_state_t *bluetooth_state_get_ptr(void);

extern const bluetooth_driver_t *bluetooth_drivers[];

RETRO_END_DECLS

#endif
