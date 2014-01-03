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

#ifndef _PSP_SDK_DEFINES_H
#define _PSP_SDK_DEFINES_H

/*============================================================
	ERROR PROTOTYPES
============================================================ */

#if defined(PSP)
#define SCE_OK 0
#endif

/*============================================================
	DISPLAY PROTOTYPES
============================================================ */

#if defined(SN_TARGET_PSP2)
#define PSP_DISPLAY_PIXEL_FORMAT_8888 (SCE_DISPLAY_PIXELFORMAT_A8B8G8R8)

#define DisplaySetFrameBuf(topaddr, bufferwidth, pixelformat, sync) sceDisplaySetFrameBuf(topaddr, sync)

#define PSP_FB_WIDTH        960
#define PSP_FB_HEIGHT       544
#define PSP_PITCH_PIXELS 1024

#elif defined(PSP)
#define DisplaySetFrameBuf(topaddr, bufferwidth, pixelformat, sync) sceDisplaySetFrameBuf(topaddr, bufferwidth, pixelformat, sync)

#define SCE_DISPLAY_UPDATETIMING_NEXTVSYNC 1

#define PSP_FB_WIDTH        512
#define PSP_FB_HEIGHT       512
#define PSP_PITCH_PIXELS 512

#endif

/*============================================================
	INPUT PROTOTYPES
============================================================ */

#if defined(SN_TARGET_PSP2)

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

#define CtrlReadBufferPositive(port, pad_data, bufs) sceCtrlReadBufferPositive(port, pad_data, bufs)

#elif defined(PSP)

#define PSP_CTRL_L PSP_CTRL_LTRIGGER
#define PSP_CTRL_R PSP_CTRL_RTRIGGER

#define STATE_BUTTON(state) ((state).Buttons)
#define STATE_ANALOGLX(state) ((state).Lx)
#define STATE_ANALOGLY(state) ((state).Ly)
#define STATE_ANALOGRX(state) ((state).Rx)
#define STATE_ANALOGRY(state) ((state).Ry)

#define DEFAULT_SAMPLING_MODE (PSP_CTRL_MODE_ANALOG)

#define CtrlReadBufferPositive(port, pad_data, bufs) sceCtrlReadBufferPositive(pad_data, bufs)
#endif

#endif
