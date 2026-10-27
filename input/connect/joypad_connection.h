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
#include <string.h>

#include <libretro.h>
#include <retro_miscellaneous.h>
#include <retro_endianness.h>
#include <retro_atomic.h>
#include "../input_driver.h"
#include "../input_defines.h"

/* Gekko (NGC/Wii) has PID/VID already swapped by USB_GetDescriptors from libogc, so skip bigendian byteswap */
#if defined(MSB_FIRST) && !defined(GEKKO)
#define SWAP_IF_BIG(val) ((((val) & 0x00ff) << 8) | (((val) & 0xff00) >> 8))
#else
#define SWAP_IF_BIG(val) (val)
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
#define VID_KADE          SWAP_IF_BIG(0x10c4)
#define VID_DRAGONRISE    SWAP_IF_BIG(0x0079)
#define VID_8BITDO        SWAP_IF_BIG(0x2dc8)
#define VID_SHANWAN       SWAP_IF_BIG(0x2563)

#define PID_NONE          0x0000
#define PID_NINTENDO_PRO  SWAP_IF_BIG(0x0330)
#define PID_SONY_DS3      SWAP_IF_BIG(0x0268)
#define PID_SONY_DS4      SWAP_IF_BIG(0x05c4)
#define PID_SONY_DS4_R2   SWAP_IF_BIG(0x09cc)
#define PID_SONY_DS5      SWAP_IF_BIG(0x0ce6)
#define PID_SONY_DS5_EDGE SWAP_IF_BIG(0x0df2)
#define PID_NINTENDO_SWITCH_PRO SWAP_IF_BIG(0x2009)
#define PID_NINTENDO_SWITCH2_PRO SWAP_IF_BIG(0x2069)
#define PID_DS3_CLONE     SWAP_IF_BIG(0x20d6)
#define PID_SNES_CLONE    SWAP_IF_BIG(0xe401)
#define PID_MICRONTEK_NES SWAP_IF_BIG(0x0011)
#define PID_NINTENDO_GCA  SWAP_IF_BIG(0x0337)
#define PID_PCS_PS2PSX    SWAP_IF_BIG(0x0001)
#define PID_PCS_PSX2PS3   SWAP_IF_BIG(0x0003)
#define PID_RETRODE       SWAP_IF_BIG(0x97c1)
#define PID_HORI_MINI_WIRED_PS4 SWAP_IF_BIG(0x00ee)
#define PID_KADE          SWAP_IF_BIG(0x82c0)
#define PID_DRAGONRISE    SWAP_IF_BIG(0x0006)
#define PID_SHANWAN_DS3   SWAP_IF_BIG(0x0523)
#define PID_8BITDO_SF30_PRO    SWAP_IF_BIG(0x6000)
#define PID_8BITDO_SN30_PRO    SWAP_IF_BIG(0x6001)
#define PID_8BITDO_SF30_PRO_BT SWAP_IF_BIG(0x6100)
#define PID_8BITDO_SN30_PRO_BT SWAP_IF_BIG(0x6101)
#define PID_8BITDO_PRO_2       SWAP_IF_BIG(0x6003)
#define PID_8BITDO_PRO_2_BT    SWAP_IF_BIG(0x6006)
#define PID_8BITDO_PRO_3       SWAP_IF_BIG(0x6009)
#define PID_8BITDO_ULTIMATE_2  SWAP_IF_BIG(0x6012)
#define PID_8BITDO_ULTIMATE_2C SWAP_IF_BIG(0x301b)
#define PID_8BITDO_ULTIMATE_3  SWAP_IF_BIG(0x202f)

/* The HID owner publishes a slot and retires it before freeing the
 * pad. One word holds admission (bit 0) and readers (increments of 2)
 * so a reader's claim and the owner's close have one atomic order.
 * The slot storage must outlive all attempts to acquire it. */
struct joypad_connection
{
    struct pad_connection_interface *iface;
    input_device_driver_t *input_driver;
    void* data;
    void* connection;
    retro_atomic_int_t gate;
    /* Taken by pad_connection_find_vacant_pad() for whoever found the
     * slot, so two threads connecting pads at once - a HID driver's
     * and the input poll - never get the same one; given back by
     * pad_connection_release_slot() when the pad is gone. */
    retro_atomic_int_t claimed;
    /* The port the frontend reads this slot's pad on, for its lights:
     * 0 not yet said, else the player plus one - player 0 none. Kept
     * across pads, so one plugged in is lit at once. */
    retro_atomic_int_t player;
    bool connected;
};

#define PAD_CONNECT_OFFLINE     0x00 /* the pad is offline and cannot be used */
#define PAD_CONNECT_READY       0x01 /* the pad is ready but is not bound to a RA slot */
#define PAD_CONNECT_BOUND       0x02 /* the pad is offline and is bound to a RA slot */
#define PAD_CONNECT_IN_USE      0x03 /* the pad is ready and is bound to a RA slot */

#define SLOT_AUTO -1

struct input_pad_view;

/* Whether @driver sends a handler's report as given, its ID first.
 * btstack writes a Bluetooth HIDP header byte (0xA2, 0x52) before the
 * report onto its channel, and iohidmanager takes the first byte off,
 * so handlers written for them put that byte first; the rest want the
 * report alone. */
#define PAD_CONNECTION_IDS_FIRST(driver) ((driver) && (driver)->ident \
      && (  !strcmp((driver)->ident, "hidraw") \
         || !strcmp((driver)->ident, "winusb") \
         || !strcmp((driver)->ident, "libusb") \
         || !strcmp((driver)->ident, "wiiu")))

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
   /* The player the pad's lights are to show, from 1; 0 for none - no
    * port reads it. Told when it changes, and once just after init; a
    * pad never told shows its slot, as before. From the frontend's
    * thread: what is sent goes from the packet handler. May be NULL. */
   void        (*set_player)(void *device, unsigned player);
   /* The pad's state as its packet handler keeps it, which its getters
    * answer from. NULL where the handler keeps it otherwise. */
   const struct input_pad_view *(*view)(void *device);
   /* A poll that brought the pad no report, for a handler with steps
    * that go on by time, not by its reports. May be NULL. */
   void        (*idle)(void *device);
   /* Motion: a core asks for a sensor, or is done with it - true where
    * the pad has it - and one value, in g or radians a second, false
    * where there is none yet. Both from the poll's thread. May be NULL. */
   bool        (*set_sensor)(void *device, enum retro_sensor_action action);
   bool        (*sensor)(void *device, unsigned id, float *value);
} pad_connection_interface_t;

/* A handler's buttons into its view: the 32 it decodes, and the menu
 * toggle where its home button is down. */
#define pad_view_set_buttons(view, bits, menu) do { \
   BIT256_CLEAR_ALL_PTR(&(view)->buttons); \
   BITS_COPY32_PTR(&(view)->buttons, (bits)); \
   if (menu) \
      BIT256_SET_PTR(&(view)->buttons, RARCH_MENU_TOGGLE); \
} while (0)

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
extern pad_connection_interface_t pad_connection_ps5;
extern pad_connection_interface_t pad_connection_ps5_edge;
extern pad_connection_interface_t pad_connection_switch_pro;
extern pad_connection_interface_t pad_connection_switch2_pro;
extern pad_connection_interface_t pad_connection_snesusb;
extern pad_connection_interface_t pad_connection_nesusb;
extern pad_connection_interface_t pad_connection_wiiugca;
/* an original Xbox pad: by its USB interface class, not by its ids */
extern pad_connection_interface_t pad_connection_xid;
extern pad_connection_interface_t pad_connection_ps2adapter;
extern pad_connection_interface_t pad_connection_psxadapter;
extern pad_connection_interface_t pad_connection_retrode;
extern pad_connection_interface_t pad_connection_ps4_hori_mini;
extern pad_connection_interface_t pad_connection_kade;
extern pad_connection_interface_t pad_connection_ps3_shanwan;
extern pad_connection_interface_t pad_connection_ps5_third_party;
extern pad_connection_interface_t pad_connection_ps4_third_party;
extern pad_connection_interface_t pad_connection_ps4_razer;
extern pad_connection_interface_t pad_connection_ps4_no_output;
extern pad_connection_interface_t pad_connection_switch_pro_n64;
extern pad_connection_interface_t pad_connection_8bitdo_sn30_pro;
extern pad_connection_interface_t pad_connection_8bitdo_sf30_pro;
extern pad_connection_interface_t pad_connection_8bitdo_pro_2;
extern pad_connection_interface_t pad_connection_8bitdo_pro_3;
extern pad_connection_interface_t pad_connection_8bitdo_ultimate_2;
extern pad_connection_interface_t pad_connection_8bitdo_ultimate_2c;
extern pad_connection_interface_t pad_connection_8bitdo_ultimate_3;
extern pad_connection_interface_t pad_connection_dragonrise;
extern pad_connection_interface_t pad_connection_gamecube_pc;

int32_t pad_connection_pad_init(joypad_connection_t *joyconn,
   const char* name, uint16_t vid, uint16_t pid,
   void *data, hid_driver_t *driver);

joypad_connection_t *pad_connection_init(unsigned pads);

void pad_connection_destroy(joypad_connection_t *joyconn);

void pad_connection_pad_deinit(joypad_connection_t *joyconn,
   uint32_t idx);

void pad_connection_packet(joypad_connection_t *joyconn,
   uint32_t idx, uint8_t* data, uint32_t length);

/* For a driver that hands its packets over in its own poll: the slot's
 * pad brought none this poll. */
void pad_connection_idle(joypad_connection_t *joyconn);

/* The slot's pad's motion sensors: as the interface's set_sensor and
 * sensor; false where it has none. */
bool pad_connection_set_sensor(joypad_connection_t *joyconn,
      enum retro_sensor_action action);
bool pad_connection_sensor(joypad_connection_t *joyconn, unsigned id,
      float *value);

void pad_connection_get_buttons(joypad_connection_t *joyconn,
   unsigned idx, input_bits_t* state);

int16_t pad_connection_get_axis(joypad_connection_t *joyconn,
   unsigned idx, unsigned i);

/* Determine if connected joypad is a hidpad backed device.
 * If false, pad_connection_packet cannot be used */

bool pad_connection_has_interface(joypad_connection_t *joyconn,
   unsigned idx);

/* A free slot, claimed for the caller until it is released. */
int pad_connection_find_vacant_pad(joypad_connection_t *joyconn);
void pad_connection_release_slot(joypad_connection_t *joyconn);
/* Makes the entry after the last slot the list's end. */
void pad_connection_mark_end(joypad_connection_t *joyconn);

/* The slot, held for calls into its pad until pad_connection_release();
 * false, holding nothing, when it is not live. For a driver that calls
 * the pad's interface itself rather than through the calls here. */
bool pad_connection_acquire(joypad_connection_t *joyconn);
void pad_connection_release(joypad_connection_t *joyconn);

bool pad_connection_rumble(joypad_connection_t *s,
   unsigned pad, enum retro_rumble_effect effect, uint16_t strength);

void pad_connection_set_player(joypad_connection_t *joyconn,
      unsigned player);

/* The slot's pad's state, where its handler keeps one; NULL otherwise.
 * Unchanged until the next packet, so for a driver that hands its
 * packets over in its own poll: it can lend it until then. */
const struct input_pad_view *pad_connection_view(
      joypad_connection_t *joyconn);

const char* pad_connection_get_name(joypad_connection_t *joyconn,
   unsigned idx);

joypad_connection_entry_t *find_connection_entry(uint16_t vid, uint16_t pid, const char *name);
bool pad_connection_has_ids(uint16_t vid, uint16_t pid);
int32_t pad_connection_pad_init_entry(joypad_connection_t *joyconn, joypad_connection_entry_t *entry, void *data, hid_driver_t *driver);
void pad_connection_pad_register(joypad_connection_t *joyconn, pad_connection_interface_t *iface, void *pad_data, void *handle, input_device_driver_t *input_driver, int slot);
void pad_connection_pad_deregister(joypad_connection_t *joyconn, pad_connection_interface_t *iface, void *pad_data);
void pad_connection_pad_refresh(joypad_connection_t *joyconn, pad_connection_interface_t *iface, void *device_data, void *handle, input_device_driver_t *input_driver);
#endif
