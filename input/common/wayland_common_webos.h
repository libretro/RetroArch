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

#ifndef _WAYLAND_COMMON_WEBOS_H
#define _WAYLAND_COMMON_WEBOS_H

#include <stdint.h>

#ifndef SYS_memfd_create
#define SYS_memfd_create 279
#endif

#define KEY_WAYLAND_WEBOS_BACK 412
#define KEY_WAYLAND_WEBOS_RED 398
#define KEY_WAYLAND_WEBOS_GREEN 399
#define KEY_WAYLAND_WEBOS_YELLOW 400
#define KEY_WAYLAND_WEBOS_BLUE 401

enum webos_wl_special_keymap
{
   webos_wl_key_back,
   webos_wl_key_size,
};

extern uint8_t webos_wl_special_keymap[webos_wl_key_size];
extern void shutdown_webos_contexts();

#endif
