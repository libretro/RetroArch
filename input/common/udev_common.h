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

#ifndef _UDEV_COMMON_H
#define _UDEV_COMMON_H

#include <stdint.h>
#include <libudev.h>

#include <boolean.h>

extern struct udev_monitor *g_udev_mon;
extern struct udev *g_udev;

bool udev_mon_new(void);

void udev_mon_free(bool is_joypad);

bool udev_mon_hotplug_available(void);

#endif
