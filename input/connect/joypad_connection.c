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

#include <stdlib.h>
#include <string.h>

#include <string/stdstring.h>
#include "../tasks/tasks_internal.h"

#include "../input_driver.h"
#include "../../verbosity.h"

#include "joypad_connection.h"
#include "../common/input_ignore_devices.h"

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#define CONNECTION_LIVE 1
#define CONNECTION_USER 2

bool pad_connection_acquire(joypad_connection_t *joyconn)
{
   int state = retro_atomic_fetch_add_int(&joyconn->gate, CONNECTION_USER);
   if (state & CONNECTION_LIVE)
      return true;
   (void)retro_atomic_fetch_sub_int(&joyconn->gate, CONNECTION_USER);
   return false;
}

void pad_connection_release(joypad_connection_t *joyconn)
{
   (void)retro_atomic_fetch_sub_int(&joyconn->gate, CONNECTION_USER);
}

/* Preserve rejected readers still leaving a closed slot. */
static void slot_publish(joypad_connection_t *joyconn)
{
   (void)retro_atomic_fetch_or_int(&joyconn->gate, CONNECTION_LIVE);
}

/* No new reader goes in; returns once none is inside. */
static void slot_retire(joypad_connection_t *joyconn)
{
   (void)retro_atomic_fetch_and_int(&joyconn->gate, ~CONNECTION_LIVE);
   while (retro_atomic_load_acquire_int(&joyconn->gate))
   {
#ifdef HAVE_THREADS
      sthread_yield();
#endif
   }
}

/* Clears a retired slot for reuse, keeping its counters. */
static void slot_clear(joypad_connection_t *joyconn)
{
   joyconn->iface        = NULL;
   joyconn->input_driver = NULL;
   joyconn->data         = NULL;
   joyconn->connection   = NULL;
   joyconn->connected    = false;
   pad_connection_release_slot(joyconn);
}

static joypad_connection_entry_t pad_map[] = {
   { "Nintendo RVL-CNT-01",
      VID_NINTENDO,
      PID_NINTENDO_PRO,
      &pad_connection_wii
   },
   { "Nintendo RVL-CNT-01-UC",
      VID_NINTENDO,
      PID_NINTENDO_PRO,
      &pad_connection_wiiupro
   },
   /* the Wii Remote Plus, and the clones that give its name */
   { "Nintendo RVL-CNT-01-TR",
      VID_NINTENDO,
      PID_NINTENDO_PRO,
      &pad_connection_wii
   },
   { "Wireless Controller",
      VID_SONY,
      PID_SONY_DS4,
      &pad_connection_ps4
   },
   { "Wireless Controller",
      VID_SONY,
      PID_SONY_DS4_R2,
      &pad_connection_ps4
   },
   { "DualSense Wireless Controller",
      VID_SONY,
      PID_SONY_DS5,
      &pad_connection_ps5
   },
   { "DualSense Edge Wireless Controller",
      VID_SONY,
      PID_SONY_DS5_EDGE,
      &pad_connection_ps5_edge
   },
   /* Sony's grip add-on, and its USB wireless adaptor */
   { "STRIKEPAD PS4 Grip Add-on",
      VID_SONY,
      SWAP_IF_BIG(0x05c5),
      &pad_connection_ps4
   },
   { "DUALSHOCK 4 USB Wireless Adaptor",
      VID_SONY,
      SWAP_IF_BIG(0x0ba0),
      &pad_connection_ps4
   },
   /* pads that pass for a DualShock 4, as SDL lists them */
   { "Venom Arcade Stick",
      SWAP_IF_BIG(0x0079),
      SWAP_IF_BIG(0x181b),
      &pad_connection_ps4_third_party
   },
   { "Thrustmaster eSwap Pro",
      SWAP_IF_BIG(0x044f),
      SWAP_IF_BIG(0xd00e),
      &pad_connection_ps4_third_party
   },
   { "Mad Catz FightPad Pro PS4",
      SWAP_IF_BIG(0x0738),
      SWAP_IF_BIG(0x8250),
      &pad_connection_ps4_third_party
   },
   { "Mad Catz FightStick TE S+ PS4",
      SWAP_IF_BIG(0x0738),
      SWAP_IF_BIG(0x8384),
      &pad_connection_ps4_third_party
   },
   { "Mad Catz FightStick TE 2 PS4",
      SWAP_IF_BIG(0x0738),
      SWAP_IF_BIG(0x8480),
      &pad_connection_ps4_third_party
   },
   { "Mad Catz FightStick TE 2+ PS4",
      SWAP_IF_BIG(0x0738),
      SWAP_IF_BIG(0x8481),
      &pad_connection_ps4_third_party
   },
   { "Armor 3 Pad PS4",
      SWAP_IF_BIG(0x0c12),
      SWAP_IF_BIG(0x0e10),
      &pad_connection_ps4_third_party
   },
   { "ZEROPLUS P4 Wired Gamepad",
      SWAP_IF_BIG(0x0c12),
      SWAP_IF_BIG(0x0e13),
      &pad_connection_ps4_third_party
   },
   { "Game:Pad 4",
      SWAP_IF_BIG(0x0c12),
      SWAP_IF_BIG(0x0e15),
      &pad_connection_ps4_third_party
   },
   { "Brook Mars Controller",
      SWAP_IF_BIG(0x0c12),
      SWAP_IF_BIG(0x0e20),
      &pad_connection_ps4_third_party
   },
   { "Hitbox Arcade Stick",
      SWAP_IF_BIG(0x0c12),
      SWAP_IF_BIG(0x0ef6),
      &pad_connection_ps4_third_party
   },
   { "EMIO PS4 Elite Controller",
      SWAP_IF_BIG(0x0c12),
      SWAP_IF_BIG(0x1cf6),
      &pad_connection_ps4_third_party
   },
   { "P4 Wired Gamepad",
      SWAP_IF_BIG(0x0c12),
      SWAP_IF_BIG(0x1e10),
      &pad_connection_ps4_third_party
   },
   { "ZEROPLUS P4 Wired Gamepad",
      SWAP_IF_BIG(0x0c12),
      SWAP_IF_BIG(0x2e18),
      &pad_connection_ps4_third_party
   },
   { "Victrix Pro FS",
      SWAP_IF_BIG(0x0e6f),
      SWAP_IF_BIG(0x0203),
      &pad_connection_ps4_no_output
   },
   { "Victrix Pro FS V2",
      SWAP_IF_BIG(0x0e6f),
      SWAP_IF_BIG(0x0207),
      &pad_connection_ps4_no_output
   },
   { "Victrix Pro FS PS4/PS5",
      SWAP_IF_BIG(0x0e6f),
      SWAP_IF_BIG(0x020a),
      &pad_connection_ps4_third_party
   },
   { "HORIPAD 4 FPS",
      SWAP_IF_BIG(0x0f0d),
      SWAP_IF_BIG(0x0055),
      &pad_connection_ps4_third_party
   },
   { "HORI Fighting Commander 4",
      SWAP_IF_BIG(0x0f0d),
      SWAP_IF_BIG(0x005e),
      &pad_connection_ps4_third_party
   },
   { "HORIPAD 4 FPS Plus",
      SWAP_IF_BIG(0x0f0d),
      SWAP_IF_BIG(0x0066),
      &pad_connection_ps4_third_party
   },
   { "HORI Fighting Commander",
      SWAP_IF_BIG(0x0f0d),
      SWAP_IF_BIG(0x0084),
      &pad_connection_ps4_third_party
   },
   { "HORI Fighting Stick mini 4",
      SWAP_IF_BIG(0x0f0d),
      SWAP_IF_BIG(0x0087),
      &pad_connection_ps4_third_party
   },
   { "HORI Real Arcade Pro 4",
      SWAP_IF_BIG(0x0f0d),
      SWAP_IF_BIG(0x008a),
      &pad_connection_ps4_third_party
   },
   { "HORI TAC PRO",
      SWAP_IF_BIG(0x0f0d),
      SWAP_IF_BIG(0x009c),
      &pad_connection_ps4_third_party
   },
   { "HORI TAC4",
      SWAP_IF_BIG(0x0f0d),
      SWAP_IF_BIG(0x00a0),
      &pad_connection_ps4_third_party
   },
   { "HORI Fighting Stick alpha",
      SWAP_IF_BIG(0x0f0d),
      SWAP_IF_BIG(0x011c),
      &pad_connection_ps4_third_party
   },
   { "HORI Wireless Controller Light",
      SWAP_IF_BIG(0x0f0d),
      SWAP_IF_BIG(0x0123),
      &pad_connection_ps4_third_party
   },
   { "HORI Fighting Commander OCTA",
      SWAP_IF_BIG(0x0f0d),
      SWAP_IF_BIG(0x0162),
      &pad_connection_ps4_third_party
   },
   { "PS4 Fun Controller",
      SWAP_IF_BIG(0x11c0),
      SWAP_IF_BIG(0x4001),
      &pad_connection_ps4_third_party
   },
   { "Nacon Revolution Pro Controller",
      SWAP_IF_BIG(0x146b),
      SWAP_IF_BIG(0x0d01),
      &pad_connection_ps4_third_party
   },
   { "Nacon Revolution Pro Controller 2",
      SWAP_IF_BIG(0x146b),
      SWAP_IF_BIG(0x0d02),
      &pad_connection_ps4_third_party
   },
   { "Nacon Asymmetric Wireless Dongle",
      SWAP_IF_BIG(0x146b),
      SWAP_IF_BIG(0x0d06),
      &pad_connection_ps4_third_party
   },
   { "Nacon Revolution Unlimited Dongle",
      SWAP_IF_BIG(0x146b),
      SWAP_IF_BIG(0x0d08),
      &pad_connection_ps4_third_party
   },
   { "Nacon Daija Fight Stick",
      SWAP_IF_BIG(0x146b),
      SWAP_IF_BIG(0x0d09),
      &pad_connection_ps4_third_party
   },
   { "Nacon Revolution Infinite",
      SWAP_IF_BIG(0x146b),
      SWAP_IF_BIG(0x0d10),
      &pad_connection_ps4_third_party
   },
   { "Nacon Revolution Pro Controller 3",
      SWAP_IF_BIG(0x146b),
      SWAP_IF_BIG(0x0d13),
      &pad_connection_ps4_third_party
   },
   { "Nacon Asymmetric Controller",
      SWAP_IF_BIG(0x146b),
      SWAP_IF_BIG(0x1103),
      &pad_connection_ps4_third_party
   },
   { "Razer Panthera",
      SWAP_IF_BIG(0x1532),
      SWAP_IF_BIG(0x0401),
      &pad_connection_ps4_razer
   },
   { "Razer Raiju",
      SWAP_IF_BIG(0x1532),
      SWAP_IF_BIG(0x1000),
      &pad_connection_ps4_razer
   },
   { "Razer Raiju 2 Ultimate",
      SWAP_IF_BIG(0x1532),
      SWAP_IF_BIG(0x1004),
      &pad_connection_ps4_razer
   },
   { "Razer Raiju 2 Tournament Edition",
      SWAP_IF_BIG(0x1532),
      SWAP_IF_BIG(0x1007),
      &pad_connection_ps4_razer
   },
   { "Razer Panthera Evo",
      SWAP_IF_BIG(0x1532),
      SWAP_IF_BIG(0x1008),
      &pad_connection_ps4_razer
   },
   { "Razer Raiju 2 Ultimate",
      SWAP_IF_BIG(0x1532),
      SWAP_IF_BIG(0x1009),
      &pad_connection_ps4_razer
   },
   { "Razer Raiju 2 Tournament Edition",
      SWAP_IF_BIG(0x1532),
      SWAP_IF_BIG(0x100a),
      &pad_connection_ps4_razer
   },
   { "Razer Raion",
      SWAP_IF_BIG(0x1532),
      SWAP_IF_BIG(0x1100),
      &pad_connection_ps4_razer
   },
   { "PowerA Fusion Fight Pad",
      SWAP_IF_BIG(0x20d6),
      SWAP_IF_BIG(0x792a),
      &pad_connection_ps4_third_party
   },
   { "Qanba Drone",
      SWAP_IF_BIG(0x2c22),
      SWAP_IF_BIG(0x2000),
      &pad_connection_ps4_third_party
   },
   { "Qanba Obsidian",
      SWAP_IF_BIG(0x2c22),
      SWAP_IF_BIG(0x2300),
      &pad_connection_ps4_third_party
   },
   { "Qanba Dragon",
      SWAP_IF_BIG(0x2c22),
      SWAP_IF_BIG(0x2500),
      &pad_connection_ps4_third_party
   },
   { "Nacon Revolution 5 Pro",
      SWAP_IF_BIG(0x3285),
      SWAP_IF_BIG(0x0d16),
      &pad_connection_ps4_third_party
   },
   { "Nacon Revolution 5 Pro",
      SWAP_IF_BIG(0x3285),
      SWAP_IF_BIG(0x0d17),
      &pad_connection_ps4_third_party
   },
   { "Armor 3",
      SWAP_IF_BIG(0x7545),
      SWAP_IF_BIG(0x0104),
      &pad_connection_ps4_third_party
   },
   { "Astro C40",
      SWAP_IF_BIG(0x9886),
      SWAP_IF_BIG(0x0025),
      &pad_connection_ps4_third_party
   },
   /* pads that pass for a DualSense, as SDL lists them */
   { "Victrix Pro FS",
      SWAP_IF_BIG(0x0e6f),
      SWAP_IF_BIG(0x0209),
      &pad_connection_ps5_third_party
   },
   { "HORI Fighting Commander OCTA",
      SWAP_IF_BIG(0x0f0d),
      SWAP_IF_BIG(0x0163),
      &pad_connection_ps5_third_party
   },
   { "HORI Fighting Stick alpha",
      SWAP_IF_BIG(0x0f0d),
      SWAP_IF_BIG(0x0184),
      &pad_connection_ps5_third_party
   },
   { "Razer Wolverine V2 Pro",
      SWAP_IF_BIG(0x1532),
      SWAP_IF_BIG(0x100b),
      &pad_connection_ps5_third_party
   },
   { "Razer Wolverine V2 Pro",
      SWAP_IF_BIG(0x1532),
      SWAP_IF_BIG(0x100c),
      &pad_connection_ps5_third_party
   },
   { "Razer Kitsune",
      SWAP_IF_BIG(0x1532),
      SWAP_IF_BIG(0x1012),
      &pad_connection_ps5_third_party
   },
   { "Razer Raiju V3 Pro",
      SWAP_IF_BIG(0x1532),
      SWAP_IF_BIG(0x1024),
      &pad_connection_ps5_third_party
   },
   { "Razer Raiju V3 Pro",
      SWAP_IF_BIG(0x1532),
      SWAP_IF_BIG(0x1026),
      &pad_connection_ps5_third_party
   },
   { "NACON Revolution 5 Pro",
      SWAP_IF_BIG(0x3285),
      SWAP_IF_BIG(0x0d18),
      &pad_connection_ps5_third_party
   },
   { "NACON Revolution 5 Pro",
      SWAP_IF_BIG(0x3285),
      SWAP_IF_BIG(0x0d19),
      &pad_connection_ps5_third_party
   },
   { "Backbone One PlayStation Edition",
      SWAP_IF_BIG(0x358a),
      SWAP_IF_BIG(0x0104),
      &pad_connection_ps5_third_party
   },
   { "Backbone One PlayStation Edition",
      SWAP_IF_BIG(0x358a),
      SWAP_IF_BIG(0x0304),
      &pad_connection_ps5_third_party
   },
   { "Pro Controller",
      VID_NINTENDO,
      PID_NINTENDO_SWITCH_PRO,
      &pad_connection_switch_pro
   },
   /* the Switch Online pads, which speak as the Pro Controller does and
    * are read by its handler, as SDL reads them; on the right Joy-Con's
    * id, only the NES and Famicom pads, by their names */
   { "SNES Controller",
      VID_NINTENDO,
      SWAP_IF_BIG(0x2017),
      &pad_connection_switch_pro
   },
   { "N64 Controller",
      VID_NINTENDO,
      SWAP_IF_BIG(0x2019),
      &pad_connection_switch_pro_n64
   },
   { "MD/Gen Control Pad",
      VID_NINTENDO,
      SWAP_IF_BIG(0x201e),
      &pad_connection_switch_pro
   },
   { "NES Controller",
      VID_NINTENDO,
      SWAP_IF_BIG(0x2007),
      &pad_connection_switch_pro
   },
   { "HVC Controller",
      VID_NINTENDO,
      SWAP_IF_BIG(0x2007),
      &pad_connection_switch_pro
   },
   /* pads that pass for a Pro Controller on their own ids */
   { "Pro Controller",
      SWAP_IF_BIG(0x0e6f),
      SWAP_IF_BIG(0x0186),
      &pad_connection_switch_pro
   },
   { "Pro Controller",
      SWAP_IF_BIG(0x0e6f),
      SWAP_IF_BIG(0x018c),
      &pad_connection_switch_pro
   },
   { "Pro Controller",
      SWAP_IF_BIG(0x0f0d),
      SWAP_IF_BIG(0x00f6),
      &pad_connection_switch_pro
   },
   { "Nintendo Switch 2 Pro Controller",
      VID_NINTENDO,
      PID_NINTENDO_SWITCH2_PRO,
      &pad_connection_switch2_pro
   },
   /* a ShanWan pad that gives Sony's id, known by its name */
   { "ShanWan",
      VID_SONY,
      PID_SONY_DS3,
      &pad_connection_ps3_shanwan
   },
   { "PLAYSTATION(R)3 Controller",
      VID_SONY,
      PID_SONY_DS3,
      &pad_connection_ps3
   },
   { "PLAYSTATION(R)3 Controller",
      VID_SHANWAN,
      PID_SHANWAN_DS3,
      &pad_connection_ps3_shanwan
   },
   { "PLAYSTATION(R)3 Controller",
      VID_PS3_CLONE,
      PID_DS3_CLONE,
      &pad_connection_ps3
   },
   { "Generic SNES USB Controller",
      VID_SNES_CLONE,
      PID_SNES_CLONE,
      &pad_connection_snesusb
   },
   { "USB Gamepad",
      VID_MICRONTEK,
      PID_MICRONTEK_NES,
      &pad_connection_nesusb
   },
   { "Wii U GC Controller Adapter",
      VID_NINTENDO,
      PID_NINTENDO_GCA,
      &pad_connection_wiiugca
   },
   { "PS2/PSX Controller Adapter",
      VID_PCS,
      PID_PCS_PS2PSX,
      &pad_connection_ps2adapter
   },
   { "PSX to PS3 Controller Adapter",
      VID_PCS,
      PID_PCS_PSX2PS3,
      &pad_connection_psxadapter
   },
   { "Mayflash DolphinBar",
      1406,
      774,
      &pad_connection_wii
   },
   { "Retrode",
      VID_RETRODE,
      PID_RETRODE,
      &pad_connection_retrode
   },
   { "HORI mini wired PS4",
      VID_HORI_1,
      PID_HORI_MINI_WIRED_PS4,
      &pad_connection_ps4_hori_mini
   },
   { "Kade Kick Ass Dynamic Encoder",
      VID_KADE,
      PID_KADE,
      &pad_connection_kade
   },
   /* GameCube adapters in their PC mode, as SDL lists them */
   { "GameCube Controller Adapter",
      VID_DRAGONRISE,
      SWAP_IF_BIG(0x1843),
      &pad_connection_gamecube_pc
   },
   { "GameCube Controller Adapter",
      VID_DRAGONRISE,
      SWAP_IF_BIG(0x1844),
      &pad_connection_gamecube_pc
   },
   { "GameCube Controller Adapter",
      VID_DRAGONRISE,
      SWAP_IF_BIG(0x1846),
      &pad_connection_gamecube_pc
   },
   { "DragonRise ZeroDelay Encoder",
      VID_DRAGONRISE,
      PID_DRAGONRISE,
      &pad_connection_dragonrise
   },
#ifndef __APPLE__
   /* macOS reads it from its report descriptor: its hat is the
    * descriptor's there, which a handler's would not be */
   { "8Bitdo SN30 Pro",
      VID_8BITDO,
      PID_8BITDO_SN30_PRO,
      &pad_connection_8bitdo_sn30_pro
   },
   { "8Bitdo SN30 Pro",
      VID_8BITDO,
      PID_8BITDO_SN30_PRO_BT,
      &pad_connection_8bitdo_sn30_pro
   },
   { "8Bitdo SF30 Pro",
      VID_8BITDO,
      PID_8BITDO_SF30_PRO,
      &pad_connection_8bitdo_sf30_pro
   },
   { "8Bitdo SF30 Pro",
      VID_8BITDO,
      PID_8BITDO_SF30_PRO_BT,
      &pad_connection_8bitdo_sf30_pro
   },
   { "8BitDo Pro 2",
      VID_8BITDO,
      PID_8BITDO_PRO_2,
      &pad_connection_8bitdo_pro_2
   },
   { "8BitDo Pro 2",
      VID_8BITDO,
      PID_8BITDO_PRO_2_BT,
      &pad_connection_8bitdo_pro_2
   },
   { "8BitDo Pro 3",
      VID_8BITDO,
      PID_8BITDO_PRO_3,
      &pad_connection_8bitdo_pro_3
   },
   { "8BitDo Ultimate 2 Wireless",
      VID_8BITDO,
      PID_8BITDO_ULTIMATE_2,
      &pad_connection_8bitdo_ultimate_2
   },
   { "8BitDo Ultimate 2C Wireless",
      VID_8BITDO,
      PID_8BITDO_ULTIMATE_2C,
      &pad_connection_8bitdo_ultimate_2c
   },
   { "8BitDo Ultimate 3",
      VID_8BITDO,
      PID_8BITDO_ULTIMATE_3,
      &pad_connection_8bitdo_ultimate_3
   },
#endif
   { 0, 0}
};

static bool joypad_is_end_of_list(joypad_connection_t *pad)
{
   return pad
      && !pad->connected
      && !pad->iface
      &&  (pad->data == (void *)0xdeadbeef);
}

/* claimed: free, taken, or the list's end marker */
#define SLOT_FREE  0
#define SLOT_TAKEN 1
#define SLOT_END   2

static bool slot_claim(joypad_connection_t *joyconn)
{
#ifdef RETRO_ATOMIC_HAS_CAS
   return retro_atomic_cas_int(&joyconn->claimed, SLOT_FREE, SLOT_TAKEN) != 0;
#else
   if (retro_atomic_load_acquire_int(&joyconn->claimed) != SLOT_FREE)
      return false;
   retro_atomic_store_release_int(&joyconn->claimed, SLOT_TAKEN);
   return true;
#endif
}

void pad_connection_mark_end(joypad_connection_t *joyconn)
{
   joyconn->connected = false;
   joyconn->iface     = NULL;
   joyconn->data      = (void *)0xdeadbeef;
   retro_atomic_int_init(&joyconn->gate, 0);
   retro_atomic_int_init(&joyconn->claimed, SLOT_END);
}

void pad_connection_release_slot(joypad_connection_t *joyconn)
{
   if (joyconn)
      retro_atomic_store_release_int(&joyconn->claimed, SLOT_FREE);
}

/* Reads nothing but the claims: the fields of a slot belong to the
 * thread that holds it. */
int pad_connection_find_vacant_pad(joypad_connection_t *joyconn)
{
   if (joyconn)
   {
      int i;
      for (i = 0; ; i++)
      {
         int state = retro_atomic_load_acquire_int(&joyconn[i].claimed);
         if (state == SLOT_END)
            break;
         if (state == SLOT_FREE && slot_claim(&joyconn[i]))
            return i;
      }
   }
   return -1;
}

/**
 * Since the pad_connection_destroy() call needs to iterate through this
 * list, we allocate pads+1 entries and use the extra spot to store a
 * marker.
 */
joypad_connection_t *pad_connection_init(unsigned pads)
{
   int i;
   joypad_connection_t *joyconn;

   if (pads > MAX_USERS)
   {
      RARCH_WARN("[Joypad] Invalid number of pads requested (%d), using default (%d).\n",
            pads, MAX_USERS);
      pads = MAX_USERS;
   }

   if (!(joyconn = (joypad_connection_t*)calloc(pads+1, sizeof(joypad_connection_t))))
      return NULL;

   for (i = 0; i < (int)pads; i++)
   {
      joypad_connection_t *conn  = (joypad_connection_t*)&joyconn[i];
      retro_atomic_int_init(&conn->gate, 0);
      retro_atomic_int_init(&conn->claimed, 0);
      retro_atomic_int_init(&conn->player, 0);

      conn->connected            = false;
      conn->iface                = NULL;
      conn->data                 = NULL;
   }

   pad_connection_mark_end(&joyconn[pads]);

   return joyconn;
}
/* "USB Gamepad", as every pad on Microntek's chip calls itself; Linux
 * puts the maker's name in front */
static bool joypad_connection_is_nes_clone_name(const char *name)
{
   return strstr(name, "USB Gamepad") != NULL;
}

/* Whether some handler is for @vid and @pid, whatever its name. */
bool pad_connection_has_ids(uint16_t vid, uint16_t pid)
{
   unsigned i;
   for (i = 0; pad_map[i].name != NULL; i++)
      if (pad_map[i].vid == vid && pad_map[i].pid == pid)
         return true;
   return false;
}

joypad_connection_entry_t *find_connection_entry(uint16_t vid, uint16_t pid, const char *name)
{
   unsigned i;
   const bool has_name = name && *name;
   size_t name_len     = strlen(name);

   for (i = 0; pad_map[i].name != NULL; i++)
   {
      char *name_match = NULL;
      /* The Wii Pro Controller and WiiU Pro controller have
       * the same VID/PID, so we have to use the
       * descriptor string to differentiate them. */
      if (     pad_map[i].vid == VID_NINTENDO
            && pad_map[i].pid == PID_NINTENDO_PRO
            && pad_map[i].vid == vid
            && pad_map[i].pid == pid)
      {
         name_match = has_name
            ? (char*)strstr(pad_map[i].name, name)
            : NULL;
         if (has_name && name_len < 19)
         {
            /* Wii U: Argument 'name' may be truncated. This is not enough for a reliable name match! */
            RARCH_ERR("[Joypad] find_connection_entry(0x%04x,0x%04x): device name '%s' too short: assuming controller '%s'.\n",
                  SWAP_IF_BIG(vid), SWAP_IF_BIG(pid), name, pad_map[i].name);
         }
         else if (!string_is_equal(pad_map[i].name, name))
            continue;
      }

      /* Microntek's chip gives one id, and one name, to pads of every
       * make - NES and SNES pads, Saturn pads - and reports them all
       * the same way: four face buttons, four shoulder buttons, Select
       * and Start. Its handler takes a pad that names itself as the
       * chip does; any other on the id is read from its report
       * descriptor. With no name to go by, it is taken as before. */
      if (     pad_map[i].iface == &pad_connection_nesusb
            && pad_map[i].vid == vid && pad_map[i].pid == pid
            && has_name
            && !string_is_equal(name, pad_map[i].name)
            && !joypad_connection_is_nes_clone_name(name))
         continue;

      /* the right Joy-Con's id: a Switch Online pad by its name */
      if (     pad_map[i].vid == VID_NINTENDO
            && pad_map[i].pid == SWAP_IF_BIG(0x2007)
            && (!has_name || !string_starts_with(name, pad_map[i].name)))
         continue;

      /* on Sony's id, ShanWan's own handler only for a pad that says
       * it is ShanWan's, as SDL tells them */
      if (     pad_map[i].iface == &pad_connection_ps3_shanwan
            && pad_map[i].vid == VID_SONY
            && (!has_name || !string_starts_with_case_insensitive(name,
                  "ShanWan")))
         continue;

      if (name_match || (pad_map[i].vid == vid && pad_map[i].pid == pid))
         return &pad_map[i];
   }

   return NULL;
}

static int joypad_to_slot(joypad_connection_t *haystack,
      joypad_connection_t *needle)
{
   int i;

   if (!needle)
      return -1;

   for (i = 0; !joypad_is_end_of_list(&haystack[i]); i++)
   {
      if (&haystack[i] == needle)
         return i;
   }
   return -1;
}

void legacy_pad_connection_pad_deregister(joypad_connection_t *pad_list, pad_connection_interface_t *iface, void *pad_data)
{
   int i;
   for (i = 0; !joypad_is_end_of_list(&pad_list[i]); i++)
   {
      if (pad_list[i].connection == pad_data)
      {
         slot_retire(&pad_list[i]);
         input_autoconfigure_disconnect(i, iface ? iface->get_name(pad_data) : NULL);
         slot_clear(&pad_list[i]);
         return;
      }
   }
}

void pad_connection_pad_deregister(joypad_connection_t *joyconn,
      pad_connection_interface_t *iface, void *pad_data)
{
   int i;

   if (!iface || !iface->multi_pad)
   {
      legacy_pad_connection_pad_deregister(joyconn, iface, pad_data);
      return;
   }

   for (i = 0; i < iface->max_pad; i++)
   {
      int slot = joypad_to_slot(joyconn, iface->joypad(pad_data, i));
      if (slot >= 0)
      {
         slot_retire(&joyconn[slot]);
         input_autoconfigure_disconnect(slot, iface->get_name(joyconn[slot].connection));
         iface->pad_deinit(joyconn[slot].connection);
         slot_clear(&joyconn[slot]);
      }
   }
}



void pad_connection_pad_refresh(joypad_connection_t *joyconn,
      pad_connection_interface_t *iface,
      void *device_data, void *handle,
      input_device_driver_t *input_driver)
{
   int i, slot;
   int8_t state;
   joypad_connection_t *joypad;

   if (!iface->multi_pad || iface->max_pad < 1)
      return;

   for (i = 0; i < iface->max_pad; i++)
   {
      state = iface->status(device_data, i);
      switch(state)
      {
         /* The pad slot is bound to a joypad
            that's no longer connected */
         case PAD_CONNECT_BOUND:
            joypad = iface->joypad(device_data, i);
            slot   = joypad_to_slot(joyconn, joypad);
            slot_retire(joypad);
            input_autoconfigure_disconnect(slot,
                  iface->get_name(joypad->connection));

            iface->pad_deinit(joypad->connection);
            slot_clear(joypad);
            break;
            /* The joypad is connected but has not been bound */
         case PAD_CONNECT_READY:
            if ((slot = pad_connection_find_vacant_pad(joyconn)) >= 0)
            {
               joypad               = &joyconn[slot];
               joypad->connection   = iface->pad_init(device_data,
                                      i, joypad);
               joypad->data         = handle;
               joypad->iface        = iface;
               joypad->input_driver = input_driver;
               joypad->connected    = true;
               slot_publish(joypad);
               input_pad_connect(slot, input_driver);
            }
            break;
         default:
#ifndef NDEBUG
            if (state > 0x03)
               RARCH_LOG("[Joypad] Unrecognized state: 0x%02x.", state);
#endif
            break;
      }
   }
}

void pad_connection_pad_register(joypad_connection_t *joyconn,
      pad_connection_interface_t *iface,
      void *device_data, void *handle,
      input_device_driver_t *input_driver, int slot)
{
   int i;
   int max_pad = 1;

   if (iface->multi_pad)
   {
      if (iface->max_pad <= 1 || !iface->status || !iface->pad_init)
      {
         RARCH_ERR("[Joypad] pad_connection_pad_register: multi-pad driver has incomplete implementation.\n");
         return;
      }
      max_pad = iface->max_pad;
   }

   for (i = 0; i < max_pad; i++)
   {
      int status = iface->multi_pad
         ? iface->status(device_data, i)
         : PAD_CONNECT_READY;
      if (status == PAD_CONNECT_READY)
      {
         void *connection = NULL;
         int found_slot   = (slot == SLOT_AUTO)
            ? pad_connection_find_vacant_pad(joyconn)
            : slot;
         if (found_slot < 0)
            continue;
         if (iface->multi_pad)
            connection = iface->pad_init(device_data, i,
                  &joyconn[found_slot]);
         else
            connection = device_data;

         joyconn[found_slot].iface        = iface;
         joyconn[found_slot].data         = handle;
         joyconn[found_slot].connection   = connection;
         joyconn[found_slot].input_driver = input_driver;
         joyconn[found_slot].connected    = true;
         slot_publish(&joyconn[found_slot]);

         RARCH_LOG("[Joypad] Connecting pad to slot %d.\n", found_slot);
         input_pad_connect(found_slot, input_driver);
      }
   }
}

int32_t pad_connection_pad_init_entry(joypad_connection_t *joyconn,
      joypad_connection_entry_t *entry,
      void *data, hid_driver_t *driver)
{
   joypad_connection_t *conn = NULL;
   int pad  = pad_connection_find_vacant_pad(joyconn);
   int told = -1; /* the player its pad was given; -1 for none to give */

   if (pad < 0)
      return -1;
   /* Another program reads it and stands in for it (Steam Input) */
   if (entry && input_device_ignored(entry->vid, entry->pid))
      return -1;

   if (!(conn = &joyconn[pad]))
      return -1;

   if (entry)
   {
      conn->iface      = entry->iface;
      conn->connection = conn->iface->init(data, pad, driver);
      if (conn->connection && conn->iface->set_player)
         told = retro_atomic_load_acquire_int(&conn->player);
      if (told > 0)
         conn->iface->set_player(conn->connection, (unsigned)(told - 1));
   }
   else
   {
      /* We failed to find a matching pad.
       * Set up one without an interface */
      RARCH_DBG("[Joypad] Pad was not matched. Setting up without an interface.\n");
      conn->iface      = NULL;
   }
   conn->data          = data;
   conn->connected     = true;
   slot_publish(conn);
   /* told while it started, before the frontend could reach it */
   if (     told >= 0
         && retro_atomic_load_acquire_int(&conn->player) != told)
   {
      told = retro_atomic_load_acquire_int(&conn->player);
      if (told)
         conn->iface->set_player(conn->connection, (unsigned)(told - 1));
   }

   return pad;
}

int32_t pad_connection_pad_init(joypad_connection_t *joyconn,
   const char *name, uint16_t vid, uint16_t pid,
   void *data, hid_driver_t *driver)
{
   joypad_connection_entry_t *entry = find_connection_entry(vid, pid, name);
   if (input_device_ignored(vid, pid))
      return -1;
   return pad_connection_pad_init_entry(joyconn, entry, data, driver);
}

void pad_connection_pad_deinit(joypad_connection_t *joyconn,
      uint32_t pad)
{
   if (!joyconn || !joyconn->connected)
       return;

   slot_retire(joyconn);

   if (joyconn->iface)
   {
      joyconn->iface->set_rumble(joyconn->connection,
            RETRO_RUMBLE_STRONG, 0);
      joyconn->iface->set_rumble(joyconn->connection,
            RETRO_RUMBLE_WEAK, 0);

      if (joyconn->iface->deinit)
         joyconn->iface->deinit(joyconn->connection);
   }

   joyconn->iface      = NULL;
   joyconn->connected  = false;
   joyconn->connection = NULL;
   pad_connection_release_slot(joyconn);
}

void pad_connection_packet(joypad_connection_t *joyconn, uint32_t pad,
      uint8_t* data, uint32_t length)
{
   if (!joyconn || !pad_connection_acquire(joyconn))
      return;
   if (     joyconn->connection
         && joyconn->iface
         && joyconn->iface->packet_handler)
      joyconn->iface->packet_handler(joyconn->connection, data, length);
   pad_connection_release(joyconn);
}

void pad_connection_idle(joypad_connection_t *joyconn)
{
   /* most handlers have none: no gate taken for them. The interface
    * is static and checked again under the gate. */
   const pad_connection_interface_t *iface;
   if (     !joyconn
         || !(iface = joyconn->iface)
         || !iface->idle
         || !pad_connection_acquire(joyconn))
      return;
   if (     joyconn->connection
         && joyconn->iface
         && joyconn->iface->idle)
      joyconn->iface->idle(joyconn->connection);
   pad_connection_release(joyconn);
}

bool pad_connection_set_sensor(joypad_connection_t *joyconn,
      enum retro_sensor_action action)
{
   bool ret = false;
   if (!joyconn || !pad_connection_acquire(joyconn))
      return false;
   if (     joyconn->connection
         && joyconn->iface
         && joyconn->iface->set_sensor)
      ret = joyconn->iface->set_sensor(joyconn->connection, action);
   pad_connection_release(joyconn);
   return ret;
}

bool pad_connection_sensor(joypad_connection_t *joyconn, unsigned id,
      float *value)
{
   bool ret = false;
   if (!joyconn || !pad_connection_acquire(joyconn))
      return false;
   if (     joyconn->connection
         && joyconn->iface
         && joyconn->iface->sensor)
      ret = joyconn->iface->sensor(joyconn->connection, id, value);
   pad_connection_release(joyconn);
   return ret;
}

void pad_connection_get_buttons(joypad_connection_t *joyconn,
      unsigned pad, input_bits_t *state)
{
   if (joyconn && pad_connection_acquire(joyconn))
   {
      if (joyconn->iface)
         joyconn->iface->get_buttons(joyconn->connection, state);
      else
         BIT256_CLEAR_ALL_PTR( state );
      pad_connection_release(joyconn);
   }
   else
      BIT256_CLEAR_ALL_PTR( state );
}

int16_t pad_connection_get_axis(joypad_connection_t *joyconn,
   unsigned idx, unsigned i)
{
   int16_t val = 0;
   if (joyconn && pad_connection_acquire(joyconn))
   {
      if (joyconn->iface)
         val = joyconn->iface->get_axis(joyconn->connection, i);
      pad_connection_release(joyconn);
   }
   return val;
}

bool pad_connection_has_interface(joypad_connection_t *joyconn,
      unsigned pad)
{
   bool ret = false;
   if (joyconn && pad < MAX_USERS && pad_connection_acquire(&joyconn[pad]))
   {
      ret = joyconn[pad].iface != NULL;
      pad_connection_release(&joyconn[pad]);
   }
   return ret;
}

void pad_connection_destroy(joypad_connection_t *joyconn)
{
   int i;

   for (i = 0; !joypad_is_end_of_list(&joyconn[i]); i ++)
     pad_connection_pad_deinit(&joyconn[i], i);

   free(joyconn);
}

bool pad_connection_rumble(joypad_connection_t *joyconn,
   unsigned pad, enum retro_rumble_effect effect, uint16_t strength)
{
   bool ret = false;
   if (!pad_connection_acquire(joyconn))
      return false;
   if (joyconn->iface && joyconn->iface->set_rumble)
   {
      joyconn->iface->set_rumble(joyconn->connection, effect, strength);
      ret = true;
   }
   pad_connection_release(joyconn);
   return ret;
}

/* Kept for the slot, and told to its pad when it changes. */
void pad_connection_set_player(joypad_connection_t *joyconn,
      unsigned player)
{
   int told = (int)player + 1;
   if (     !joyconn
         || retro_atomic_load_acquire_int(&joyconn->player) == told)
      return;
   retro_atomic_store_release_int(&joyconn->player, told);
   if (!pad_connection_acquire(joyconn))
      return;
   if (joyconn->iface && joyconn->iface->set_player && joyconn->connection)
      joyconn->iface->set_player(joyconn->connection, player);
   pad_connection_release(joyconn);
}

const input_pad_view_t *pad_connection_view(joypad_connection_t *joyconn)
{
   const input_pad_view_t *view = NULL;
   if (joyconn && pad_connection_acquire(joyconn))
   {
      if (     joyconn->iface
            && joyconn->iface->view
            && joyconn->connection)
         view = joyconn->iface->view(joyconn->connection);
      pad_connection_release(joyconn);
   }
   return view;
}

const char* pad_connection_get_name(joypad_connection_t *joyconn,
      unsigned pad)
{
   const char *name = NULL;
   if (joyconn && pad_connection_acquire(joyconn))
   {
      if (joyconn->iface && joyconn->iface->get_name)
         name = joyconn->iface->get_name(joyconn->connection);
      pad_connection_release(joyconn);
   }
   return name;
}
