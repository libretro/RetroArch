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

#ifndef _PSP_INPUT_H_
#define _PSP_INPUT_H_

enum {
   PSP_GAMEPAD_CROSS =			      1 << 0,
   PSP_GAMEPAD_SQUARE =			      1 << 1,
   PSP_GAMEPAD_SELECT =			      1 << 2,
   PSP_GAMEPAD_START =			      1 << 3,
   PSP_GAMEPAD_DPAD_UP =		      1 << 4,
   PSP_GAMEPAD_DPAD_DOWN =		      1 << 5,
   PSP_GAMEPAD_DPAD_LEFT =		      1 << 6,
   PSP_GAMEPAD_DPAD_RIGHT =		   1 << 7,
   PSP_GAMEPAD_CIRCLE =			      1 << 8,
   PSP_GAMEPAD_TRIANGLE =		      1 << 9,
   PSP_GAMEPAD_L =			         1 << 10,
   PSP_GAMEPAD_R =			         1 << 11,
   PSP_GAMEPAD_LSTICK_LEFT_MASK =	1 << 16,
   PSP_GAMEPAD_LSTICK_RIGHT_MASK =	1 << 17,
   PSP_GAMEPAD_LSTICK_UP_MASK	 =	   1 << 18,
   PSP_GAMEPAD_LSTICK_DOWN_MASK =	1 << 19,
#ifdef SN_TARGET_PSP2
   PSP_GAMEPAD_RSTICK_LEFT_MASK =	1 << 20,
   PSP_GAMEPAD_RSTICK_RIGHT_MASK =	1 << 21,
   PSP_GAMEPAD_RSTICK_UP_MASK 	=	1 << 22,
   PSP_GAMEPAD_RSTICK_DOWN_MASK =	1 << 23,
#endif
};

#endif
