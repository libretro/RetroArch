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
#define HID_REPORT_INPUT   1
#define HID_REPORT_OUTPUT  2
#define HID_REPORT_FEATURE 3
#define HID_REPORT_COUNT   4
/* are there more? */

/*
 * This is the interface for the HID subsystem.
 *
 * The handle parameter is the pointer returned by init() and stores the implementation
 * state data for the HID driver.
 */

struct input_pad_view;

struct hid_driver
{
   void *(*init)(void);
   bool (*query_pad)(void *handle, unsigned pad);
   void (*free)(const void *handle);
   int16_t (*button)(void *handle, unsigned pad, uint16_t button);
   int16_t (*state)(void *data, rarch_joypad_info_t *joypad_info,
         const uint16_t *joykeys, const uint32_t *joyaxes, unsigned port);
   void (*get_buttons)(void *handle, unsigned pad, input_bits_t *state);
   int16_t (*axis)(void *handle, unsigned pad, uint32_t axis);
   void (*poll)(void *handle);
   bool (*set_rumble)(void *handle, unsigned pad, enum retro_rumble_effect effect, uint16_t);
   const char *(*name)(void *handle, unsigned pad);
   const char *ident;
   void (*send_control)(void *handle, uint8_t *s, size_t len);
   int32_t (*set_report)(void *handle, uint8_t report_type, uint8_t report_id, uint8_t *data, size_t len);
   int32_t (*get_report)(void *handle, uint8_t report_type, uint8_t report_id, uint8_t *data, size_t len);
   int32_t (*set_idle)(void *handle, uint8_t amount);
   int32_t (*set_protocol)(void *handle, uint8_t protocol);
   int32_t (*read)(void *handle, void *s, size_t len);
   /* as input_device_driver_t's */
   bool (*get_bt_host)(void *handle, unsigned pad, uint8_t *addr);
   bool (*set_bt_host)(void *handle, unsigned pad, const uint8_t *addr);
   /* Whether the device - send_control's handle - is on Bluetooth, for
    * a handler whose reports differ by the way the pad came and which
    * cannot tell it by their length: Windows gives every report at the
    * longest one's length. NULL where the lengths tell. */
   bool (*is_bluetooth)(void *handle);
   /* as input_device_driver_t's: the player @pad's lights are to show */
   void (*set_player)(void *handle, unsigned pad, unsigned player);
   /* as input_device_driver_t's: pad @pad's state, unchanged until the
    * next poll; NULL where it cannot be lent */
   const struct input_pad_view *(*view)(void *handle, unsigned pad);
   /* as input_device_driver_t's: pad @pad's motion sensors; NULL where
    * the driver reads none */
   bool (*set_sensor_state)(void *handle, unsigned pad,
         enum retro_sensor_action action, unsigned rate);
   bool (*get_sensor_input)(void *handle, unsigned pad, unsigned id,
         float *value);
};

#endif /* HID_DRIVER_H__ */
