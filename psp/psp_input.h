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

#ifdef SN_TARGET_PSP2
#define STATE_BUTTON(state) ((state).buttons)
#define STATE_ANALOGLX(state) ((state).lx)
#define STATE_ANALOGLY(state) ((state).ly)
#define STATE_ANALOGRX(state) ((state).rx)
#define STATE_ANALOGRY(state) ((state).ry)
#define DEFAULT_SAMPLING_MODE (SCE_CTRL_MODE_DIGITALANALOG)

#define PSP_CTRL_LEFT SCE_CTRL_LEFT
#define PSP_CTRL_DOWN SCE_CTRL_DOWN
#define PSP_CTRL_RIGHT SCE_CTRL_RIGHT
#define PSP_CTRL_UP SCE_CTRL_UP
#define PSP_CTRL_START SCE_CTRL_START
#define PSP_CTRL_SELECT SCE_CTRL_SELECT
#define PSP_CTRL_TRIANGLE SCE_CTRL_TRIANGLE
#define PSP_CTRL_SQUARE SCE_CTRL_SQUARE
#define PSP_CTRL_CROSS SCE_CTRL_CROSS
#define PSP_CTRL_CIRCLE SCE_CTRL_CIRCLE
#define PSP_CTRL_L SCE_CTRL_L
#define PSP_CTRL_R SCE_CTRL_R

#define sceCtrlReadBufferPositive(port, pad_data, bufs) sceCtrlReadBufferPositive(port, pad_data, bufs)
#else
#define STATE_BUTTON(state) ((state).Buttons)
#define STATE_ANALOGLX(state) ((state).Lx)
#define STATE_ANALOGLX(state) ((state).Ly)
#define STATE_ANALOGRX(state) ((state).Rx)
#define STATE_ANALOGRY(state) ((state).Ry)

#define DEFAULT_SAMPLING_MODE (PSP_CTRL_MODE_ANALOG)

#define sceCtrlReadBufferPositive(port, pad_data, bufs) sceCtrlReadBufferPositive(pad_data, bufs)
#endif

#endif
