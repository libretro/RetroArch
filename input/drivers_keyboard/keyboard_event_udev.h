/*  RetroArch - A frontend for libretro.
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

#ifndef _KEYBOARD_DRIVER_UDEV_H
#define _KEYBOARD_DRIVER_UDEV_H

#include <linux/input.h>
#include <libudev.h>

#include <boolean.h>

#include "../input_driver.h"

typedef struct udev_input_device udev_input_device_t;

void udev_handle_keyboard(void *data,
      const struct input_event *event, udev_input_device_t *dev);

bool udev_input_is_pressed(const struct retro_keybind *binds, unsigned id);

bool udev_input_state_kb(void *data, const struct retro_keybind **binds,
      unsigned port, unsigned device, unsigned idx, unsigned id);

void udev_input_kb_free(void);

#endif
