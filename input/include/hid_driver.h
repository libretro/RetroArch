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

struct hid_driver
{
   void *(*init)(void);
   bool (*query_pad)(void *handle, unsigned pad);
   void (*free)(const void *handle);
   int16_t (*button)(void *handle, unsigned pad, uint16_t button);
   int16_t (*state)(void *data, rarch_joypad_info_t *joypad_info,
         const void *binds_data, unsigned port);
   void (*get_buttons)(void *handle, unsigned pad, input_bits_t *state);
   int16_t (*axis)(void *handle, unsigned pad, uint32_t axis);
   void (*poll)(void *handle);
   bool (*set_rumble)(void *handle, unsigned pad, enum retro_rumble_effect effect, uint16_t);
   const char *(*name)(void *handle, unsigned pad);
   const char *ident;
   void (*send_control)(void *handle, uint8_t *buf, size_t size);
   int32_t (*set_report)(void *handle, uint8_t report_type, uint8_t report_id, uint8_t *data, size_t length);
   int32_t (*get_report)(void *handle, uint8_t report_type, uint8_t report_id, uint8_t *data, size_t length);
   int32_t (*set_idle)(void *handle, uint8_t amount);
   int32_t (*set_protocol)(void *handle, uint8_t protocol);
   int32_t (*read)(void *handle, void *buf, size_t size);
};

#endif /* HID_DRIVER_H__ */
