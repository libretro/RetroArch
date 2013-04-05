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

#ifndef __IOS_RARCH_INPUT_H__
#define __IOS_RARCH_INPUT_H__

// Input responder
#define MAX_TOUCHES 16
#define MAX_KEYS 256

typedef struct
{
   int16_t screen_x, screen_y;
   int16_t fixed_x, fixed_y;
   int16_t full_x, full_y;
} ios_touch_data_t;

typedef struct
{
   ios_touch_data_t touches[MAX_TOUCHES];
   uint32_t touch_count;

   uint32_t keys[MAX_KEYS];

   uint32_t pad_buttons;
   int16_t pad_axis[4];
} ios_input_data_t;

extern ios_input_data_t g_ios_input_data;

// Defined in main.m, must be called on the emu thread in a dispatch_sync block
void ios_copy_input(ios_input_data_t* data);

// Called from main.m, defined in ios_input.c
void ios_add_key_event(bool down, unsigned keycode, uint32_t character, uint16_t keyModifiers);

#endif
