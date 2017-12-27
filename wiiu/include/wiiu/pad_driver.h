/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
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

#ifndef __PAD_DRIVER__H
#define __PAD_DRIVER__H

#ifdef HAVE_CONFIG_H
#include  "../../config.h"
#endif // HAVE_CONFIG_H

#include <wiiu/vpad.h>
#include <wiiu/kpad.h>
#include <string.h>

#include "../../input/input_driver.h"
#include "../../input/connect/joypad_connection.h"
#include "../../tasks/tasks_internal.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../command.h"
#include "../../gfx/video_driver.h"

/**
 * Magic button sequence that triggers an exit. Useful for if the visuals are
 * corrupted, but won't work in the case of a hard lock.
 */
#define PANIC_BUTTON_MASK (VPAD_BUTTON_R | VPAD_BUTTON_L | VPAD_BUTTON_STICK_R | VPAD_BUTTON_STICK_L)

/**
 * Applies a standard transform to the Wii U gamepad's analog stick.
 * No  idea where 0x7ff0 comes from.
 */

#define WIIU_ANALOG_FACTOR 0x7ff0
#define WIIU_READ_STICK(stick) ((stick) * WIIU_ANALOG_FACTOR)

/**
 * the wiimote driver uses these to delimit which pads correspond to the
 * wiimotes.
 */
#define PAD_GAMEPAD 0
#define WIIU_WIIMOTE_CHANNELS 4

/**
 * These are used by the wiimote driver to identify the wiimote configuration
 * attached to the channel.
 */
// wiimote with Wii U Pro controller
#define WIIMOTE_TYPE_PRO     0x1f
// wiimote with Classic Controller
#define WIIMOTE_TYPE_CLASSIC 0x02
// wiimote with nunchuk
#define WIIMOTE_TYPE_NUNCHUK 0x01
// wiimote plus (no accessory attached)
#define WIIMOTE_TYPE_WIIPLUS 0x00
// wiimote not attached on this channel
#define WIIMOTE_TYPE_NONE    0xFD

/**
 * The Wii U gamepad and wiimotes have 3 sets of x/y axes. The third
 * is used by the gamepad for the touchpad driver; the wiimotes is
 * currently unimplemented, but could be used for future IR pointer
 * support.
 */
#define WIIU_DEVICE_INDEX_TOUCHPAD 2

typedef struct _axis_data axis_data;

struct _axis_data {
  int32_t axis;
  bool is_negative;
};

typedef struct _wiiu_pad_functions wiiu_pad_functions_t;

struct _wiiu_pad_functions {
  int16_t (*get_axis_value)(int32_t axis, int16_t state[3][2], bool is_negative);
  void (*set_axis_value)(int16_t state[3][2], int16_t left_x, int16_t left_y,
    int16_t right_x, int16_t right_y, int16_t touch_x, int16_t touch_y);
  void (*read_axis_data)(uint32_t axis, axis_data *data);
  void (*connect)(unsigned pad, input_device_driver_t *driver);
};

extern wiiu_pad_functions_t pad_functions;
extern input_device_driver_t wpad_driver;
extern input_device_driver_t kpad_driver;
extern input_device_driver_t hidpad_driver;

#endif // __PAD_DRIVER__H
