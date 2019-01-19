/*  RetroArch - A frontend for libretro.
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

#ifndef WAYLAND_COMMON_H__
#define WAYLAND_COMMON_H__

#include <stdint.h>
#include <boolean.h>

#include <linux/input.h>
#include <wayland-client.h>
#include <wayland-cursor.h>

#include "../../input/input_driver.h"

#define UDEV_KEY_MAX			0x2ff
#define UDEV_MAX_KEYS (UDEV_KEY_MAX + 7) / 8

#define MAX_TOUCHES             16

typedef struct
{
   bool active;
   int16_t x;
   int16_t y;
} wayland_touch_data_t;

typedef struct input_ctx_wayland_data
{
   /* Wayland uses Linux keysyms. */
   uint8_t key_state[UDEV_MAX_KEYS];
   bool keyboard_focus;

   struct
   {
      int last_x, last_y;
      int x, y;
      int delta_x, delta_y;
      bool last_valid;
      bool focus;
      bool left, right, middle;
   } mouse;

   struct wl_display *dpy;
   int fd;

   const input_device_driver_t *joypad;
   bool blocked;

   wayland_touch_data_t touches[MAX_TOUCHES];

} input_ctx_wayland_data_t;

#endif
