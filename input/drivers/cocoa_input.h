/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2013-2014 - Jason Fetters
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

#ifndef __COCOA_INPUT_H__
#define __COCOA_INPUT_H__

#include <stdint.h>
#include <boolean.h>

#ifdef HAVE_CONFIG_H
#include "../../config.def.h"
#endif

#include "../input_driver.h"

/* Input responder */
#define MAX_TOUCHES  16

/* A position in the window - the pointer's, a touch's - or a motion, x
 * and y packed in one word: written in one store, so the two always go
 * together. The packing is the one the rest of the frontend uses. */
#define COCOA_POS_PACK(x, y) VIDEO_POS_PACK(x, y)
#define COCOA_POS_X(p)       VIDEO_POS_X(p)
#define COCOA_POS_Y(p)       VIDEO_POS_Y(p)

typedef struct
{
   uint32_t screen_pos;      /* COCOA_POS_PACK(x, y) */
   uint32_t fixed_pos;       /* in the viewport, -0x8000 outside */
   uint32_t full_pos;        /* on the whole screen */
   uint32_t confined_pos;    /* in the viewport, held to its edges */
} cocoa_touch_data_t;

typedef struct
{
   uint32_t touch_count;

   uint32_t mouse_buttons;
   cocoa_touch_data_t touches[MAX_TOUCHES];
   uint32_t mouse_last;      /* the pointer at the last poll, packed */
   uint32_t window_pos;      /* COCOA_POS_PACK(x, y) */
   uint32_t mouse_rel;       /* the mouse's motion, packed */
   int16_t mouse_wu;
   int16_t mouse_wd;
   int16_t mouse_wl;
   int16_t mouse_wr;
   bool mouse_grabbed;
} cocoa_input_data_t;

/* What the Apple UI hands this driver: its mouse, pointer and touch
 * events. The UI calls these and does not write the driver's data
 * itself. On the main thread; nothing happens while the Cocoa driver
 * is not the one in use. */
void cocoa_input_mouse_moved(int16_t dx, int16_t dy, int16_t x, int16_t y);
void cocoa_input_mouse_moved_by(int16_t dx, int16_t dy);
void cocoa_input_mouse_button(unsigned number, bool down, bool as_touch);
void cocoa_input_pointer_at(int16_t x, int16_t y);
void cocoa_input_touches_begin(void);
bool cocoa_input_touch_add(int16_t x, int16_t y);
void cocoa_input_touches_reset(void);
bool cocoa_input_mouse_grabbed(void);

#endif
