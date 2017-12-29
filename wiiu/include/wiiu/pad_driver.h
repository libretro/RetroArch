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

#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <wiiu/os.h>
#include <wiiu/syshid.h>
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
 * These are used to map pad names to controller mappings. You can
 * change these relatively free-form.
 */

#define PAD_NAME_WIIU_GAMEPAD "WiiU Gamepad"
#define PAD_NAME_WIIU_PRO "WiiU Pro Controller"
#define PAD_NAME_WIIMOTE "Wiimote Controller"
#define PAD_NAME_NUNCHUK "Wiimote+Nunchuk Controller"
#define PAD_NAME_CLASSIC "Classic Controller"
#define PAD_NAME_HID "HID Controller"

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

/**
 * HID driver data structures
 */

typedef struct wiiu_hid {
  // used to register for HID notifications
  HIDClient *client;
  // list of HID pads
  joypad_connection_t *connections;
  // size of connections list
  unsigned connections_size;
  // thread state data for HID polling thread
  OSThread *polling_thread;
  // stack space for polling thread
  void *polling_thread_stack;
  // watch variable to tell the polling thread to terminate
  volatile bool polling_thread_quit;
} wiiu_hid_t;

typedef struct wiiu_adapter wiiu_adapter_t;

struct wiiu_adapter {
  wiiu_adapter_t *next;
  wiiu_hid_t *hid;
  uint8_t state;
  uint8_t *rx_buffer;
  int32_t rx_size;
  int32_t slot;
  uint32_t handle;
  uint8_t interface_index;
};

typedef struct wiiu_attach wiiu_attach_event;

struct wiiu_attach {
  wiiu_attach_event *next;
  uint32_t type;
  uint32_t handle;
  uint16_t vendor_id;
  uint16_t product_id;
  uint8_t interface_index;
  uint8_t is_keyboard;
  uint8_t is_mouse;
  uint16_t max_packet_size_rx;
  uint16_t max_packet_size_tx;
};

typedef struct _wiiu_event_list wiiu_event_list;
typedef struct _wiiu_adapter_list wiiu_adapter_list;

struct _wiiu_event_list {
  OSFastMutex lock;
  wiiu_attach_event *list;
};

struct _wiiu_adapter_list {
  OSFastMutex lock;
  wiiu_adapter_t *list;
};

extern wiiu_pad_functions_t pad_functions;
extern input_device_driver_t wiiu_joypad;
extern input_device_driver_t wpad_driver;
extern input_device_driver_t kpad_driver;
extern input_device_driver_t hidpad_driver;
extern hid_driver_t wiiu_hid;

#endif // __PAD_DRIVER__H
