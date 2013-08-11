/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
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

#ifndef __APPLE_RARCH_INPUT_H__
#define __APPLE_RARCH_INPUT_H__

// Input responder
#define MAX_TOUCHES 16
#define MAX_KEYS 256
#define MAX_PADS 4

typedef struct
{
   int16_t screen_x, screen_y;
   int16_t fixed_x, fixed_y;
   int16_t full_x, full_y;
} apple_touch_data_t;

typedef struct
{
   apple_touch_data_t touches[MAX_TOUCHES];
   uint32_t touch_count;

   uint32_t mouse_buttons;
   int16_t mouse_delta[2];

   uint32_t keys[MAX_KEYS];

   uint32_t pad_buttons[MAX_PADS];
   int16_t pad_axis[MAX_PADS][4];
} apple_input_data_t;

extern apple_input_data_t g_current_input_data; //< Main thread data
extern apple_input_data_t g_polled_input_data;  //< Game thread data

// Main thread only
void apple_input_enable_icade(bool on);
void apple_input_handle_key_event(unsigned keycode, bool down);

#endif
