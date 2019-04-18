/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Andrés Suárez
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

#ifndef HID_DRIVER_H__
#define HID_DRIVER_H__

#include "../connect/joypad_connection.h"
#include "../input_driver.h"

/* what is 1? */
#define HID_REPORT_OUTPUT 2
#define HID_REPORT_FEATURE 3
/* are there more? */

/*
 * This is the interface for the HID subsystem.
 *
 * The handle parameter is the pointer returned by init() and stores the implementation
 * state data for the HID driver.
 */

struct hid_driver
{
   void *(*init)(void);
   bool (*query_pad)(void *handle, unsigned pad);
   void (*free)(const void *handle);
   bool (*button)(void *handle, unsigned pad, uint16_t button);
   void (*get_buttons)(void *handle, unsigned pad, input_bits_t *state);
   int16_t (*axis)(void *handle, unsigned pad, uint32_t axis);
   void (*poll)(void *handle);
   bool (*set_rumble)(void *handle, unsigned pad, enum retro_rumble_effect effect, uint16_t);
   const char *(*name)(void *handle, unsigned pad);
   const char *ident;
   void (*send_control)(void *handle, uint8_t *buf, size_t size);
   int32_t (*set_report)(void *handle, uint8_t, uint8_t, void *data, uint32_t size);
   int32_t (*set_idle)(void *handle, uint8_t amount);
   int32_t (*set_protocol)(void *handle, uint8_t protocol);
   int32_t (*read)(void *handle, void *buf, size_t size);
};

#define HID_GET_BUTTONS(pad, state) hid_instance.os_driver->get_buttons( \
   hid_instance.os_driver_data, pad, state)
#define HID_BUTTON(pad, key) hid_instance.os_driver->button( \
   hid_instance.os_driver_data, pad, key)
#define HID_AXIS(pad, axis) hid_instance.os_driver->axis( \
   hid_instance.os_driver_data, pad, axis)
#define HID_PAD_NAME(pad) \
   hid_instance.os_driver->name(hid_instance.os_driver_data, pad)
#define HID_SET_PROTOCOL(pad, protocol) \
   hid_instance.os_driver->set_protocol(pad, protocol)
#define HID_SET_REPORT(pad, rpttype, rptid, data, len) \
   hid_instance.os_driver->set_report(pad, rpttype, rptid, data, len)
#define HID_SEND_CONTROL(pad, data, len) \
   hid_instance.os_driver->send_control(pad, data, len)
#define HID_POLL() hid_instance.os_driver->poll( \
   hid_instance.os_driver_data)
#define HID_MAX_SLOT() hid_instance.max_slot
#define HID_PAD_CONNECTION_PTR(slot) &(hid_instance.pad_list[(slot)])

struct hid_driver_instance {
   hid_driver_t *os_driver;
   void *os_driver_data;
   input_device_driver_t *pad_driver;
   joypad_connection_t *pad_list;
   unsigned max_slot;
};

#endif /* HID_DRIVER_H__ */
