/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __SSNES_KEYSYM_H
#define __SSNES_KEYSYM_H

// Global keysym table for SSNES.
// As you may have noticed, it's the same as SDL 1.2 since I'm lazy.

enum ssnes_key
{
   SK_UNKNOWN        = 0,
   SK_FIRST          = 0,
   SK_BACKSPACE      = 8,
   SK_TAB            = 9,
   SK_CLEAR          = 12,
   SK_RETURN         = 13,
   SK_PAUSE          = 19,
   SK_ESCAPE         = 27,
   SK_SPACE          = 32,
   SK_EXCLAIM        = 33,
   SK_QUOTEDBL       = 34,
   SK_HASH           = 35,
   SK_DOLLAR         = 36,
   SK_AMPERSAND      = 38,
   SK_QUOTE          = 39,
   SK_LEFTPAREN      = 40,
   SK_RIGHTPAREN     = 41,
   SK_ASTERISK       = 42,
   SK_PLUS           = 43,
   SK_COMMA          = 44,
   SK_MINUS          = 45,
   SK_PERIOD         = 46,
   SK_SLASH          = 47,
   SK_0              = 48,
   SK_1              = 49,
   SK_2              = 50,
   SK_3              = 51,
   SK_4              = 52,
   SK_5              = 53,
   SK_6              = 54,
   SK_7              = 55,
   SK_8              = 56,
   SK_9              = 57,
   SK_COLON          = 58,
   SK_SEMICOLON      = 59,
   SK_LESS           = 60,
   SK_EQUALS         = 61,
   SK_GREATER        = 62,
   SK_QUESTION       = 63,
   SK_AT             = 64,
   SK_LEFTBRACKET    = 91,
   SK_BACKSLASH      = 92,
   SK_RIGHTBRACKET   = 93,
   SK_CARET          = 94,
   SK_UNDERSCORE     = 95,
   SK_BACKQUOTE      = 96,
   SK_a              = 97,
   SK_b              = 98,
   SK_c              = 99,
   SK_d              = 100,
   SK_e              = 101,
   SK_f              = 102,
   SK_g              = 103,
   SK_h              = 104,
   SK_i              = 105,
   SK_j              = 106,
   SK_k              = 107,
   SK_l              = 108,
   SK_m              = 109,
   SK_n              = 110,
   SK_o              = 111,
   SK_p              = 112,
   SK_q              = 113,
   SK_r              = 114,
   SK_s              = 115,
   SK_t              = 116,
   SK_u              = 117,
   SK_v              = 118,
   SK_w              = 119,
   SK_x              = 120,
   SK_y              = 121,
   SK_z              = 122,
   SK_DELETE         = 127,

   SK_KP0            = 256,
   SK_KP1            = 257,
   SK_KP2            = 258,
   SK_KP3            = 259,
   SK_KP4            = 260,
   SK_KP5            = 261,
   SK_KP6            = 262,
   SK_KP7            = 263,
   SK_KP8            = 264,
   SK_KP9            = 265,
   SK_KP_PERIOD      = 266,
   SK_KP_DIVIDE      = 267,
   SK_KP_MULTIPLY    = 268,
   SK_KP_MINUS       = 269,
   SK_KP_PLUS        = 270,
   SK_KP_ENTER       = 271,
   SK_KP_EQUALS      = 272,

   SK_UP             = 273,
   SK_DOWN           = 274,
   SK_RIGHT          = 275,
   SK_LEFT           = 276,
   SK_INSERT         = 277,
   SK_HOME           = 278,
   SK_END            = 279,
   SK_PAGEUP         = 280,
   SK_PAGEDOWN       = 281,

   SK_F1             = 282,
   SK_F2             = 283,
   SK_F3             = 284,
   SK_F4             = 285,
   SK_F5             = 286,
   SK_F6             = 287,
   SK_F7             = 288,
   SK_F8             = 289,
   SK_F9             = 290,
   SK_F10            = 291,
   SK_F11            = 292,
   SK_F12            = 293,
   SK_F13            = 294,
   SK_F14            = 295,
   SK_F15            = 296,

   SK_NUMLOCK        = 300,
   SK_CAPSLOCK       = 301,
   SK_SCROLLOCK      = 302,
   SK_RSHIFT         = 303,
   SK_LSHIFT         = 304,
   SK_RCTRL          = 305,
   SK_LCTRL          = 306,
   SK_RALT           = 307,
   SK_LALT           = 308,
   SK_RMETA          = 309,
   SK_LMETA          = 310,
   SK_LSUPER         = 311,
   SK_RSUPER         = 312,
   SK_MODE           = 313,
   SK_COMPOSE        = 314,

   SK_HELP           = 315,
   SK_PRINT          = 316,
   SK_SYSREQ         = 317,
   SK_BREAK          = 318,
   SK_MENU           = 319,
   SK_POWER          = 320,
   SK_EURO           = 321,
   SK_UNDO           = 322,

   SK_LAST
};

#endif

