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

#ifndef __IOS_RARCH_BTPAD_H__
#define __IOS_RARCH_BTPAD_H__

uint32_t btpad_get_buttons();
void btpad_set_pad_type(bool wiimote);

// Private interface
struct btpad_interface
{
   void* (*connect)();
   void (*disconnect)(void* device);
   void (*set_leds)(void* device, unsigned leds);

   uint32_t (*get_buttons)(void* device);

   void (*packet_handler)(void* device, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
};

extern struct btpad_interface btpad_ps3;
extern struct btpad_interface btpad_wii;

#endif
