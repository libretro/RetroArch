/* Copyright (C) 2010-2021 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (psp_defines.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _PSP_DEFINES_H
#define _PSP_DEFINES_H

/*============================================================
	ERROR PROTOTYPES
============================================================ */

#ifndef SCE_OK
#define SCE_OK 0
#endif

/*============================================================
	DISPLAY PROTOTYPES
============================================================ */

#if defined(SN_TARGET_PSP2) || defined(VITA)

#ifdef VITA
int sceClibPrintf ( const char * format, ... );
#define printf sceClibPrintf
#define PSP_DISPLAY_PIXEL_FORMAT_8888 (SCE_DISPLAY_PIXELFORMAT_A8B8G8R8)
#else
#define PSP_DISPLAY_PIXEL_FORMAT_8888 (SCE_DISPLAY_PIXELFORMAT_A8B8G8R8)
#endif

#define DisplaySetFrameBuf(topaddr, bufferwidth, pixelformat, sync) sceDisplaySetFrameBuf(topaddr, sync)

#define PSP_FB_WIDTH        960
#define PSP_FB_HEIGHT       544
#define PSP_PITCH_PIXELS 1024

/* Memory left to the system for threads and other internal stuffs */
#ifdef SCE_LIBC_SIZE
#define RAM_THRESHOLD 0x2000000 + SCE_LIBC_SIZE
#else
#define RAM_THRESHOLD 0x2000000
#endif

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

#if defined(SN_TARGET_PSP2) || defined(VITA)

#define STATE_BUTTON(state) ((state).buttons)
#define STATE_ANALOGLX(state) ((state).lx)
#define STATE_ANALOGLY(state) ((state).ly)
#define STATE_ANALOGRX(state) ((state).rx)
#define STATE_ANALOGRY(state) ((state).ry)

#if defined(VITA)
#define DEFAULT_SAMPLING_MODE (SCE_CTRL_MODE_ANALOG)

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
#define PSP_CTRL_L SCE_CTRL_L1
#define PSP_CTRL_R SCE_CTRL_R1
#define PSP_CTRL_L2 SCE_CTRL_LTRIGGER
#define PSP_CTRL_R2 SCE_CTRL_RTRIGGER
#define PSP_CTRL_L3 SCE_CTRL_L3
#define PSP_CTRL_R3 SCE_CTRL_R3
#else
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
#endif

#if defined(VITA)
#define CtrlSetSamplingMode(mode) sceCtrlSetSamplingModeExt(mode)
#define CtrlPeekBufferPositive(port, pad_data, bufs) sceCtrlPeekBufferPositiveExt2(port, pad_data, bufs)
#else
#define CtrlSetSamplingMode(mode) sceCtrlSetSamplingMode(mode)
#define CtrlPeekBufferPositive(port, pad_data, bufs) sceCtrlPeekBufferPositive(port, pad_data, bufs)
#endif

#elif defined(PSP)

#define PSP_CTRL_L PSP_CTRL_LTRIGGER
#define PSP_CTRL_R PSP_CTRL_RTRIGGER

#define STATE_BUTTON(state) ((state).Buttons)
#define STATE_ANALOGLX(state) ((state).Lx)
#define STATE_ANALOGLY(state) ((state).Ly)
#define STATE_ANALOGRX(state) ((state).Rx)
#define STATE_ANALOGRY(state) ((state).Ry)

#define DEFAULT_SAMPLING_MODE (PSP_CTRL_MODE_ANALOG)

#define CtrlSetSamplingMode(mode) sceCtrlSetSamplingMode(mode)
#define CtrlPeekBufferPositive(port, pad_data, bufs) sceCtrlPeekBufferPositive(pad_data, bufs)
#endif

#endif
