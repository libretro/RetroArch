/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#ifndef _KEYBOARD_EVENT_DOS_H
#define _KEYBOARD_EVENT_DOS_H

#include "../input_driver.h"

/*
 * Key codes.
 */
enum {
   DOSKEY_ESCAPE = 0x1,
   DOSKEY_F1 = 0x3b,
   DOSKEY_F2 = 0x3c,
   DOSKEY_F3 = 0x3d,
   DOSKEY_F4 = 0x3e,
   DOSKEY_F5 = 0x3f,
   DOSKEY_F6 = 0x40,
   DOSKEY_F7 = 0x41,
   DOSKEY_F8 = 0x42,
   DOSKEY_F9 = 0x43,
   DOSKEY_F10 = 0x44,

   DOSKEY_BACKQUOTE = 0x29,
   DOSKEY_1 = 0x2,
   DOSKEY_2 = 0x3,
   DOSKEY_3 = 0x4,
   DOSKEY_4 = 0x5,
   DOSKEY_5 = 0x6,
   DOSKEY_6 = 0x7,
   DOSKEY_7 = 0x8,
   DOSKEY_8 = 0x9,
   DOSKEY_9 = 0xa,
   DOSKEY_0 = 0xb,
   DOSKEY_MINUS = 0xc,
   DOSKEY_EQUAL = 0xd,
   DOSKEY_BACKSPACE = 0xe,

   DOSKEY_TAB = 0xf,
   DOSKEY_q = 0x10,
   DOSKEY_w = 0x11,
   DOSKEY_e = 0x12,
   DOSKEY_r = 0x13,
   DOSKEY_t = 0x14,
   DOSKEY_y = 0x15,
   DOSKEY_u = 0x16,
   DOSKEY_i = 0x17,
   DOSKEY_o = 0x18,
   DOSKEY_p = 0x19,
   DOSKEY_LBRACKET = 0x1a,
   DOSKEY_RBRACKET = 0x1b,
   DOSKEY_BACKSLASH = 0x2b,

   DOSKEY_CAPSLOCK = 0x3a,
   DOSKEY_a = 0x1e,
   DOSKEY_s = 0x1f,
   DOSKEY_d = 0x20,
   DOSKEY_f = 0x21,
   DOSKEY_g = 0x22,
   DOSKEY_h = 0x23,
   DOSKEY_j = 0x24,
   DOSKEY_k = 0x25,
   DOSKEY_l = 0x26,
   DOSKEY_SEMICOLON = 0x27,
   DOSKEY_QUOTE = 0x28,
   DOSKEY_RETURN = 0x1c,

   DOSKEY_LSHIFT = 0x2a,
   DOSKEY_z = 0x2c,
   DOSKEY_x = 0x2d,
   DOSKEY_c = 0x2e,
   DOSKEY_v = 0x2f,
   DOSKEY_b = 0x30,
   DOSKEY_n = 0x31,
   DOSKEY_m = 0x32,
   DOSKEY_COMMA = 0x33,
   DOSKEY_PERIOD = 0x34,
   DOSKEY_SLASH = 0x35,
   DOSKEY_RSHIFT = 0x36,

   DOSKEY_LCTRL = 0x1d,
   DOSKEY_LSUPER = 0x15b,
   DOSKEY_LALT = 0x38,
   DOSKEY_SPACE = 0x39,
   DOSKEY_RALT = 0x138,
   DOSKEY_RSUPER = 0x15c,
   DOSKEY_MENU = 0x15d,
   DOSKEY_RCTRL = 0x11d,

   DOSKEY_UP = 0x148,
   DOSKEY_DOWN = 0x150,
   DOSKEY_LEFT = 0x14b,
   DOSKEY_RIGHT = 0x14d,

   DOSKEY_HOME = 0x147,
   DOSKEY_END = 0x14f,
   DOSKEY_PGUP = 0x149,
   DOSKEY_PGDN = 0x151,
};

#include <stdint.h>

#include <boolean.h>

#include "../../config.def.h"

#define LAST_KEYCODE 0x1ff

uint16_t *dos_keyboard_state_get(unsigned port);

#endif
