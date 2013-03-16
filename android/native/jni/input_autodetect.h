/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#ifndef _ANDROID_INPUT_AUTODETECT_H
#define _ANDROID_INPUT_AUTODETECT_H

#define AKEY_EVENT_NO_ACTION 255

enum {
   ICADE_PROFILE_RED_SAMURAI = 0,
   ICADE_PROFILE_IPEGA_PG9017,
} icade_profile_enums;

enum {
   AKEYCODE_META_FUNCTION_ON = 8,
   AKEYCODE_ESCAPE          = 111,
   AKEYCODE_FORWARD_DEL     = 112,
   AKEYCODE_CTRL_LEFT       = 113,
   AKEYCODE_CTRL_RIGHT      = 114,
   AKEYCODE_CAPS_LOCK       = 115,
   AKEYCODE_SCROLL_LOCK     = 116,
   AKEYCODE_SYSRQ           = 120, AKEYCODE_BREAK           = 121,
   AKEYCODE_MOVE_HOME       = 122,
   AKEYCODE_MOVE_END        = 123,
   AKEYCODE_INSERT          = 124,
   AKEYCODE_FORWARD         = 125,
   AKEYCODE_MEDIA_PLAY      = 126,
   AKEYCODE_MEDIA_PAUSE     = 127,
   AKEYCODE_F2              = 132,
   AKEYCODE_F3              = 133,
   AKEYCODE_F4              = 134,
   AKEYCODE_F5              = 135,
   AKEYCODE_F6              = 136,
   AKEYCODE_F7              = 137,
   AKEYCODE_F8              = 138,
   AKEYCODE_F9              = 139,
   AKEYCODE_NUMPAD_1        = 145,
   AKEYCODE_NUMPAD_2        = 146,
   AKEYCODE_NUMPAD_3        = 147,
   AKEYCODE_NUMPAD_4        = 148,
   AKEYCODE_NUMPAD_5        = 149,
   AKEYCODE_NUMPAD_6        = 150,
   AKEYCODE_NUMPAD_7        = 151,
   AKEYCODE_NUMPAD_8        = 152,
   AKEYCODE_NUMPAD_9        = 153,
   AKEYCODE_BUTTON_1        = 188,
   AKEYCODE_BUTTON_2        = 189,
   AKEYCODE_BUTTON_3        = 190,
   AKEYCODE_BUTTON_4        = 191,
   AKEYCODE_BUTTON_5        = 192,
   AKEYCODE_BUTTON_6        = 193,
   AKEYCODE_BUTTON_7        = 194,
   AKEYCODE_BUTTON_8        = 195,
   AKEYCODE_BUTTON_9        = 196,
   AKEYCODE_BUTTON_10       = 197,
   AKEYCODE_BUTTON_11       = 198,
   AKEYCODE_BUTTON_12       = 199,
   AKEYCODE_BUTTON_13       = 200,
   AKEYCODE_BUTTON_14       = 201,
   AKEYCODE_BUTTON_15       = 202,
   AKEYCODE_BUTTON_16       = 203,
   AKEYCODE_ASSIST          = 219,
};

#define LAST_KEYCODE AKEYCODE_ASSIST

void input_autodetect_setup (void *data, char *msg, size_t sizeof_msg, unsigned port, unsigned id, int source);

#endif
