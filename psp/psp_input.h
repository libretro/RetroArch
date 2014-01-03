/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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
   PSP_GAMEPAD_CROSS =			      1ULL << 0,
   PSP_GAMEPAD_SQUARE =			      1ULL << 1,
   PSP_GAMEPAD_SELECT =			      1ULL << 2,
   PSP_GAMEPAD_START =			      1ULL << 3,
   PSP_GAMEPAD_DPAD_UP =		      1ULL << 4,
   PSP_GAMEPAD_DPAD_DOWN =		      1ULL << 5,
   PSP_GAMEPAD_DPAD_LEFT =		      1ULL << 6,
   PSP_GAMEPAD_DPAD_RIGHT =		   1ULL << 7,
   PSP_GAMEPAD_CIRCLE =			      1ULL << 8,
   PSP_GAMEPAD_TRIANGLE =		      1ULL << 9,
   PSP_GAMEPAD_L =			         1ULL << 10,
   PSP_GAMEPAD_R =			         1ULL << 11,
   PSP_GAMEPAD_LSTICK_LEFT_MASK =	1ULL << 16,
   PSP_GAMEPAD_LSTICK_RIGHT_MASK =	1ULL << 17,
   PSP_GAMEPAD_LSTICK_UP_MASK	 =	   1ULL << 18,
   PSP_GAMEPAD_LSTICK_DOWN_MASK =	1ULL << 19,
#ifdef SN_TARGET_PSP2
   PSP_GAMEPAD_RSTICK_LEFT_MASK =	1ULL << 20,
   PSP_GAMEPAD_RSTICK_RIGHT_MASK =	1ULL << 21,
   PSP_GAMEPAD_RSTICK_UP_MASK 	=	1ULL << 22,
   PSP_GAMEPAD_RSTICK_DOWN_MASK =	1ULL << 23,
#endif
};

#endif
