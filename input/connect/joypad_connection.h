/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "../../libretro.h"

typedef void (*send_control_t)(void *data, uint8_t *buf, size_t size);

struct joypad_connection
{
    bool connected;
    struct pad_connection_interface *iface;
    void* data;
};

typedef struct pad_connection_interface
{
   void*    (*init)(void *data, uint32_t slot, send_control_t ptr);
   void     (*deinit)(void* device);
   void     (*packet_handler)(void* device, uint8_t *packet, uint16_t size);
   void     (*set_rumble)(void* device, enum retro_rumble_effect effect,
         uint16_t strength);
   uint64_t (*get_buttons)(void *data);
   int16_t  (*get_axis)(void *data, unsigned axis);
} pad_connection_interface_t;

typedef struct joypad_connection joypad_connection_t;

extern pad_connection_interface_t pad_connection_wii;
extern pad_connection_interface_t pad_connection_wiiupro;
extern pad_connection_interface_t pad_connection_ps3;
extern pad_connection_interface_t pad_connection_ps4;

int32_t pad_connection_pad_init(joypad_connection_t *joyconn,
   const char* name, uint16_t vid, uint16_t pid,
   void *data, send_control_t ptr);

joypad_connection_t *pad_connection_init(unsigned pads);

void pad_connection_destroy(joypad_connection_t *joyconn);

void pad_connection_pad_deinit(joypad_connection_t *joyconn,
   unsigned idx);

void pad_connection_packet(joypad_connection_t *joyconn,
   unsigned idx, uint8_t* data, uint32_t length);

uint64_t pad_connection_get_buttons(joypad_connection_t *joyconn,
   unsigned idx);

int16_t pad_connection_get_axis(joypad_connection_t *joyconn,
   unsigned idx, unsigned i);

/* Determine if connected joypad is a hidpad backed device.
 * If false, pad_connection_packet cannot be used */

bool pad_connection_has_interface(joypad_connection_t *joyconn,
   unsigned idx);

int pad_connection_find_vacant_pad(joypad_connection_t *joyconn);

bool pad_connection_rumble(joypad_connection_t *s,
   unsigned pad, enum retro_rumble_effect effect, uint16_t strength);

#endif
