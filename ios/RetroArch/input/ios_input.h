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

typedef struct touch_data
{
   int16_t screen_x, screen_y;
   int16_t fixed_x, fixed_y;
   int16_t full_x, full_y;
} touch_data_t;

// Defined in main.m, lists are filled by the sendEvent selector
extern uint32_t ios_key_list[MAX_KEYS];
extern uint32_t ios_touch_count;
extern touch_data_t ios_touch_list[MAX_TOUCHES];

#endif