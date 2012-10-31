/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#ifndef _ANDROID_INPUT_H_
#define _ANDROID_INPUT_H_

#define AKEY_EVENT_NO_ACTION 255
#define MAX_PADS 8

enum {
   ANDROID_GAMEPAD_CROSS =		      1 << 0,
   ANDROID_GAMEPAD_SQUARE =	      1 << 1,
   ANDROID_GAMEPAD_SELECT =		   1 << 2,
   ANDROID_GAMEPAD_START =	       	1 << 3,
   ANDROID_GAMEPAD_DPAD_UP =		   1 << 4,
   ANDROID_GAMEPAD_DPAD_DOWN =		1 << 5,
   ANDROID_GAMEPAD_DPAD_LEFT =		1 << 6,
   ANDROID_GAMEPAD_DPAD_RIGHT =		1 << 7,
   ANDROID_GAMEPAD_CIRCLE =		   1 << 8,
   ANDROID_GAMEPAD_TRIANGLE =		   1 << 9,
   ANDROID_GAMEPAD_L1 =			      1 << 10,
   ANDROID_GAMEPAD_R1 =			      1 << 11,
   ANDROID_GAMEPAD_L2 =			      1 << 12,
   ANDROID_GAMEPAD_R2 =			      1 << 13,
   ANDROID_GAMEPAD_L3 =			      1 << 14,
   ANDROID_GAMEPAD_R3 =			      1 << 15,
   ANDROID_GAMEPAD_LSTICK_LEFT_MASK =	1 << 16,
   ANDROID_GAMEPAD_LSTICK_RIGHT_MASK =	1 << 17,
   ANDROID_GAMEPAD_LSTICK_UP_MASK =	1 << 18,
   ANDROID_GAMEPAD_LSTICK_DOWN_MASK =	1 << 19,
   ANDROID_GAMEPAD_RSTICK_LEFT_MASK =	1 << 20,
   ANDROID_GAMEPAD_RSTICK_RIGHT_MASK =	1 << 21,
   ANDROID_GAMEPAD_RSTICK_UP_MASK =	1 << 22,
   ANDROID_GAMEPAD_RSTICK_DOWN_MASK =	1 << 23,
};

typedef struct
{
   int32_t  id;
   uint64_t state;
} android_input_state_t;

#endif
