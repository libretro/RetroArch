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

#ifndef _KEYBOARD_EVENT_ANDROID_H
#define _KEYBOARD_EVENT_ANDROID_H

#include <stdint.h>

#include <boolean.h>

#ifndef MAX_PADS
#define MAX_PADS 8
#endif

bool android_keyboard_input_pressed(unsigned key);

uint8_t *android_keyboard_state_get(unsigned port);

void android_keyboard_free(void);

#endif
