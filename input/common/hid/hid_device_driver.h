/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef HID_DEVICE_DRIVER__H
#define HID_DEVICE_DRIVER__H

#include "../../input_driver.h"
#include "../../connect/joypad_connection.h"
#include "../../include/hid_driver.h"
#include "../../include/gamepad.h"
#include "../../../verbosity.h"
#include "../../../tasks/tasks_internal.h"

typedef struct hid_device {
  void *(*init)(void *handle);
  void (*free)(void *data);
  void (*handle_packet)(void *data, uint8_t *buffer, size_t size);
  bool (*detect)(uint16_t vid, uint16_t pid);
  const char *name;
} hid_device_t;

extern hid_device_t wiiu_gca_hid_device;
extern hid_device_t ds3_hid_device;
extern hid_device_t ds4_hid_device;
extern hid_driver_instance_t hid_instance;

hid_device_t *hid_device_driver_lookup(uint16_t vendor_id, uint16_t product_id);
joypad_connection_t *hid_pad_register(void *pad_handle, pad_connection_interface_t *iface);
void hid_pad_deregister(joypad_connection_t *pad);
bool hid_init(hid_driver_instance_t *instance, hid_driver_t *hid_driver, input_device_driver_t *pad_driver, unsigned slots);
void hid_deinit(hid_driver_instance_t *instance);

#endif /* HID_DEVICE_DRIVER__H */
