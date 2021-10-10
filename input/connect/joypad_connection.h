/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <libretro.h>
#include <retro_miscellaneous.h>
#include <retro_endianness.h>
#include "../input_driver.h"

/* Wii have PID/VID already swapped by USB_GetDescriptors from libogc */
#ifdef GEKKO
#define SWAP_IF_BIG(val) (val)
#else
#define SWAP_IF_BIG(val) swap_if_big16(val)
#endif

#define VID_NONE          0x0000
#define VID_NINTENDO      SWAP_IF_BIG(0x057e)
#define VID_SONY          SWAP_IF_BIG(0x054c)
#define VID_MICRONTEK     SWAP_IF_BIG(0x0079)
#define VID_PCS           SWAP_IF_BIG(0x0810)
#define VID_PS3_CLONE     SWAP_IF_BIG(0x0313)
#define VID_SNES_CLONE    SWAP_IF_BIG(0x081f)
#define VID_RETRODE       SWAP_IF_BIG(0x0403)
#define VID_HORI_1        SWAP_IF_BIG(0x0f0d)

#define PID_NONE          0x0000
#define PID_NINTENDO_PRO  SWAP_IF_BIG(0x0330)
#define PID_SONY_DS3      SWAP_IF_BIG(0x0268)
#define PID_SONY_DS4      SWAP_IF_BIG(0x05c4)
#define PID_DS3_CLONE     SWAP_IF_BIG(0x20d6)
#define PID_SNES_CLONE    SWAP_IF_BIG(0xe401)
#define PID_MICRONTEK_NES SWAP_IF_BIG(0x0011)
#define PID_NINTENDO_GCA  SWAP_IF_BIG(0x0337)
#define PID_PCS_PS2PSX    SWAP_IF_BIG(0x0001)
#define PID_PCS_PSX2PS3   SWAP_IF_BIG(0x0003)
#define PID_RETRODE       SWAP_IF_BIG(0x97c1)
#define PID_HORI_MINI_WIRED_PS4 SWAP_IF_BIG(0x00ee)

struct joypad_connection
{
    struct pad_connection_interface *iface;
    input_device_driver_t *input_driver;
    void* data;
    void* connection;
    bool connected;
};

#define PAD_CONNECT_OFFLINE     0x00 /* the pad is offline and cannot be used */
#define PAD_CONNECT_READY       0x01 /* the pad is ready but is not bound to a RA slot */
#define PAD_CONNECT_BOUND       0x02 /* the pad is offline and is bound to a RA slot */
#define PAD_CONNECT_IN_USE      0x03 /* the pad is ready and is bound to a RA slot */

#define SLOT_AUTO -1

typedef struct pad_connection_interface
{
   void*    	(*init)(void *data, uint32_t slot, hid_driver_t *driver);
   void     	(*deinit)(void* device);
   void     	(*packet_handler)(void* device, uint8_t *packet, uint16_t size);
   void     	(*set_rumble)(void* device, enum retro_rumble_effect effect,
					uint16_t strength);
   void			(*get_buttons)(void *data, input_bits_t *state);
   int16_t  	(*get_axis)(void *data, unsigned axis);
   const char*	(*get_name)(void *data);
   int32_t     (*button)(void *data, uint16_t joykey);
   /* all fields/methods below this point are only required for multi-pad devices */
   bool        multi_pad;  /* does the device provide multiple pads? */
   int8_t      max_pad;    /* number of pads this device can provide */
   void*       (*pad_init)(void *data, int pad_index, joypad_connection_t *joyconn);
   void        (*pad_deinit)(void *pad_data);
   /* pad_index is a number from 0 to max_pad-1 */
   int8_t      (*status)(void *data, int pad_index); /* returns a PAD_CONNECT_* state */
   joypad_connection_t* (*joypad)(void *device_data, int pad_index);
} pad_connection_interface_t;

typedef struct joypad_connection_entry {
   const char* name;
   uint16_t vid;
   uint16_t pid;
   pad_connection_interface_t *iface;
} joypad_connection_entry_t;

extern pad_connection_interface_t pad_connection_wii;
extern pad_connection_interface_t pad_connection_wiiupro;
extern pad_connection_interface_t pad_connection_ps3;
extern pad_connection_interface_t pad_connection_ps4;
extern pad_connection_interface_t pad_connection_snesusb;
extern pad_connection_interface_t pad_connection_nesusb;
extern pad_connection_interface_t pad_connection_wiiugca;
extern pad_connection_interface_t pad_connection_ps2adapter;
extern pad_connection_interface_t pad_connection_psxadapter;
extern pad_connection_interface_t pad_connection_retrode;
extern pad_connection_interface_t pad_connection_ps4_hori_mini;

int32_t pad_connection_pad_init(joypad_connection_t *joyconn,
   const char* name, uint16_t vid, uint16_t pid,
   void *data, hid_driver_t *driver);

joypad_connection_t *pad_connection_init(unsigned pads);

void pad_connection_destroy(joypad_connection_t *joyconn);

void pad_connection_pad_deinit(joypad_connection_t *joyconn,
   uint32_t idx);

void pad_connection_packet(joypad_connection_t *joyconn,
   uint32_t idx, uint8_t* data, uint32_t length);

void pad_connection_get_buttons(joypad_connection_t *joyconn,
   unsigned idx, input_bits_t* state);

int16_t pad_connection_get_axis(joypad_connection_t *joyconn,
   unsigned idx, unsigned i);

/* Determine if connected joypad is a hidpad backed device.
 * If false, pad_connection_packet cannot be used */

bool pad_connection_has_interface(joypad_connection_t *joyconn,
   unsigned idx);

int pad_connection_find_vacant_pad(joypad_connection_t *joyconn);

bool pad_connection_rumble(joypad_connection_t *s,
   unsigned pad, enum retro_rumble_effect effect, uint16_t strength);

const char* pad_connection_get_name(joypad_connection_t *joyconn,
   unsigned idx);

joypad_connection_entry_t *find_connection_entry(int16_t vid, int16_t pid, const char *name);
int32_t pad_connection_pad_init_entry(joypad_connection_t *joyconn, joypad_connection_entry_t *entry, void *data, hid_driver_t *driver);
void pad_connection_pad_register(joypad_connection_t *joyconn, pad_connection_interface_t *iface, void *pad_data, void *handle, input_device_driver_t *input_driver, int slot);
void pad_connection_pad_deregister(joypad_connection_t *joyconn, pad_connection_interface_t *iface, void *pad_data);
void pad_connection_pad_refresh(joypad_connection_t *joyconn, pad_connection_interface_t *iface, void *device_data, void *handle, input_device_driver_t *input_driver);
#endif
