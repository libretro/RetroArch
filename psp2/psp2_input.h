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

#ifndef _PSP2_INPUT_H_
#define _PSP2_INPUT_H_

enum {
   PSP2_GAMEPAD_CROSS =			      1 << 0,
   PSP2_GAMEPAD_SQUARE =			   1 << 1,
   PSP2_GAMEPAD_SELECT =			   1 << 2,
   PSP2_GAMEPAD_START =			      1 << 3,
   PSP2_GAMEPAD_DPAD_UP =		      1 << 4,
   PSP2_GAMEPAD_DPAD_DOWN =		   1 << 5,
   PSP2_GAMEPAD_DPAD_LEFT =		   1 << 6,
   PSP2_GAMEPAD_DPAD_RIGHT =		   1 << 7,
   PSP2_GAMEPAD_CIRCLE =			   1 << 8,
   PSP2_GAMEPAD_TRIANGLE =		      1 << 9,
   PSP2_GAMEPAD_L =			         1 << 10,
   PSP2_GAMEPAD_R =			         1 << 11,
   PSP2_GAMEPAD_LSTICK_LEFT_MASK =	1 << 16,
   PSP2_GAMEPAD_LSTICK_RIGHT_MASK =	1 << 17,
   PSP2_GAMEPAD_LSTICK_UP_MASK	 =	1 << 18,
   PSP2_GAMEPAD_LSTICK_DOWN_MASK =	1 << 19,
   PSP2_GAMEPAD_RSTICK_LEFT_MASK =	1 << 20,
   PSP2_GAMEPAD_RSTICK_RIGHT_MASK =	1 << 21,
   PSP2_GAMEPAD_RSTICK_UP_MASK 	=	1 << 22,
   PSP2_GAMEPAD_RSTICK_DOWN_MASK =	1 << 23,
};

#endif
