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

#ifndef INPUT_HID_DRIVER_H__
#define INPUT_HID_DRIVER_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <boolean.h>

#include "../libretro.h"

typedef struct hid_driver hid_driver_t;

struct hid_driver
{
   void *(*init)(void);
   bool (*query_pad)(void *, unsigned);
   void (*free)(void *);
   bool (*button)(void *, unsigned, uint16_t);
   uint64_t (*get_buttons)(void *, unsigned);
   int16_t (*axis)(void *, unsigned, uint32_t);
   void (*poll)(void *);
   bool (*set_rumble)(void *, unsigned, enum retro_rumble_effect, uint16_t);
   const char *(*name)(void *, unsigned);

   const char *ident;
};

extern hid_driver_t iohidmanager_hid;
extern hid_driver_t btstack_hid;
extern hid_driver_t libusb_hid;
extern hid_driver_t wiiusb_hid;
extern hid_driver_t null_hid;

/**
 * hid_driver_find_handle:
 * @index              : index of driver to get handle to.
 *
 * Returns: handle to HID driver at index. Can be NULL
 * if nothing found.
 **/
const void *hid_driver_find_handle(int index);

/**
 * hid_driver_find_ident:
 * @index              : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of HID driver at index. Can be NULL
 * if nothing found.
 **/
const char *hid_driver_find_ident(int index);

/**
 * config_get_hid_driver_options:
 *
 * Get an enumerated list of all HID driver names, separated by '|'.
 *
 * Returns: string listing of all HID driver names, separated by '|'.
 **/
const char* config_get_hid_driver_options(void);

/**
 * input_hid_init_first:
 *
 * Finds first suitable HID driver and initializes.
 *
 * Returns: HID driver if found, otherwise NULL.
 **/
const hid_driver_t *input_hid_init_first(void);

#ifdef __cplusplus
}
#endif

#endif
