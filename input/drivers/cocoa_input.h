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

#include "../input_driver.h"

/* Input responder */
#define MAX_TOUCHES  16

typedef struct
{
   int16_t screen_x, screen_y;
   int16_t fixed_x, fixed_y;
   int16_t full_x, full_y;
} cocoa_touch_data_t;

typedef struct
{
   cocoa_touch_data_t touches[MAX_TOUCHES];
   uint32_t touch_count;

   uint32_t mouse_buttons;
   int16_t mouse_x_last;
   int16_t mouse_y_last;
   int16_t window_pos_x;
   int16_t window_pos_y;
   int16_t mouse_rel_x;
   int16_t mouse_rel_y;
   int16_t mouse_wu;
   int16_t mouse_wd;
   int16_t mouse_wl;
   int16_t mouse_wr;

   const input_device_driver_t *sec_joypad;
   const input_device_driver_t *joypad;
} cocoa_input_data_t;

extern int32_t cocoa_input_find_any_key(void);

extern int32_t cocoa_input_find_any_button(uint32_t port);

extern int32_t cocoa_input_find_any_axis(uint32_t port);

#endif
