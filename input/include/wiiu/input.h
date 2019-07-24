/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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

#ifndef __WIIU_INPUT__H
#define __WIIU_INPUT__H

#include <string.h>
#include <malloc.h>
#include <unistd.h>

#include "hid_types.h"
#include "../gamepad.h"

#include <wiiu/os.h>
#include <wiiu/syshid.h>
#include <wiiu/vpad.h>
#include <wiiu/kpad.h>
#include <wiiu/pad_strings.h>
#include "hid.h"

#include "../../common/hid/hid_device_driver.h"
#include "../../connect/joypad_connection.h"
#include "../../../retroarch.h"
#include "../../../verbosity.h"
#include "../../../command.h"

#ifdef HAVE_CONFIG_H
#include "../../../config.h"
#endif /* HAVE_CONFIG_H */

#include "../../../tasks/tasks_internal.h"

#define WIIMOTE_TYPE_WIIPLUS 0x00
#define WIIMOTE_TYPE_NUNCHUK 0x01
#define WIIMOTE_TYPE_CLASSIC 0x02
#define WIIMOTE_TYPE_PRO     0x1f
#define WIIMOTE_TYPE_NONE    0xfd

#define WIIU_DEVICE_INDEX_TOUCHPAD 2

#define PAD_GAMEPAD 0
#define WIIU_WIIMOTE_CHANNELS 4

#define WIIU_ANALOG_FACTOR 0x7ff0
#define WIIU_READ_STICK(stick) ((stick) * WIIU_ANALOG_FACTOR)

struct _wiiu_pad_functions {
   int16_t (*get_axis_value)(int32_t axis, int16_t state[3][2], bool is_negative);
   void (*set_axis_value)(int16_t state[3][2], int16_t left_x, int16_t left_y,
      int16_t right_x, int16_t right_y, int16_t touch_x, int16_t touch_y);
   void (*read_axis_data)(uint32_t axis, axis_data *data);
   void (*connect)(unsigned pad, input_device_driver_t *driver);
};

#endif /* __WIIU_INPUT__H */
