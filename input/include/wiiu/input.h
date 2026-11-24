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
#include "../input_pad_view.h"

#include <wiiu/os.h>
#include <wiiu/syshid.h>
#include <wiiu/vpad.h>
#include <wiiu/kpad.h>
#include <wiiu/pad_strings.h>

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

#define WIIU_GAMEPAD_CHANNELS 2
#define WIIU_WIIMOTE_CHANNELS 4

#define WIIU_ANALOG_FACTOR 0x7ff0
#define WIIU_READ_STICK(stick) ((stick) * WIIU_ANALOG_FACTOR)

/* A GamePad's or a Wiimote's view (input_pad_view.h), as their polls
 * fill it and lend it: axes 0-3 the sticks' x and y, 4 and 5 where the
 * GamePad's screen is touched */
#define WIIU_AXIS_TOUCH_X 4
#define WIIU_AXIS_TOUCH_Y 5

typedef struct _wiimote_state {
   input_pad_view_t view;
   uint8_t  type;
} wiimote_state;

typedef struct _drc_state
{
   input_pad_view_t view;
} drc_state;

typedef struct wiiu_kpad {
   bool ready;
   int channel_slot_map[WIIU_WIIMOTE_CHANNELS];
   int poll_failures[WIIU_WIIMOTE_CHANNELS];
   wiimote_state wiimotes[WIIU_WIIMOTE_CHANNELS];
} wiiu_kpad_t;

typedef struct wiiu_wpad {
   drc_state pads[WIIU_GAMEPAD_CHANNELS];
   int channel_slot_map[WIIU_GAMEPAD_CHANNELS];
   bool ready;
} wiiu_wpad_t;

typedef struct wiiu_hidpad {
   bool ready;
} wiiu_hidpad_t;

typedef struct wiiu_joypad {
   joypad_connection_t pads[MAX_USERS+1];
   int max_slot;

   wiiu_kpad_t kpad;
   wiiu_wpad_t wpad;
   wiiu_hidpad_t hid;
} wiiu_joypad_t;

extern wiiu_joypad_t joypad_state;
extern input_device_driver_t wiiu_joypad;
extern input_device_driver_t wpad_driver;
extern input_device_driver_t kpad_driver;
extern input_device_driver_t hidpad_driver;
extern hid_driver_t wiiu_hid;

#endif /* __WIIU_INPUT__H */
