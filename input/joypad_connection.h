/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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

#ifndef _JOYPAD_CONNECTION_H
#define _JOYPAD_CONNECTION_H

#include <stdint.h>
#include <stddef.h>

typedef struct pad_connection_interface
{
   void*    (*connect)(void *data, uint32_t slot);
   void     (*disconnect)(void* device);
   void     (*packet_handler)(void* device, uint8_t *packet, uint16_t size);
   void     (*set_rumble)(void* device, enum retro_rumble_effect effect,
         uint16_t strength);
   uint32_t (*get_buttons)(void *data);
   int16_t  (*get_axis)(void *data, unsigned axis);
} pad_connection_interface_t;

extern pad_connection_interface_t apple_pad_wii;
extern pad_connection_interface_t apple_pad_ps3;

int32_t pad_connection_connect(const char* name, void *data);

int32_t apple_joypad_connect_gcapi(void);

void pad_connection_disconnect(uint32_t slot);

void pad_connection_packet(uint32_t slot, uint8_t* data, uint32_t length);

uint32_t pad_connection_get_buttons(void *data);

int16_t pad_connection_get_axis(void *data, unsigned i);

/* Determine if connected joypad is a hidpad backed device.
 * If false, pad_connection_packet cannot be used */

bool pad_connection_has_interface(uint32_t slot);

#endif
