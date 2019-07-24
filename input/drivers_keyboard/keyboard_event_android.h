/*  RetroArch - A frontend for libretro.
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

#ifndef _KEYBOARD_EVENT_ANDROID_H
#define _KEYBOARD_EVENT_ANDROID_H

// The list of defined Android keycodes is incomplete in SDK version 12 and lower.
// If using an SDK lower than 13 then add missing keycodes here
#if __ANDROID_API__ < 13

/*
 * Key codes.
 */
enum {
    AKEYCODE_ESCAPE          = 111,
    AKEYCODE_FORWARD_DEL     = 112,
    AKEYCODE_CTRL_LEFT       = 113,
    AKEYCODE_CTRL_RIGHT      = 114,
    AKEYCODE_CAPS_LOCK       = 115,
    AKEYCODE_SCROLL_LOCK     = 116,
    AKEYCODE_META_LEFT       = 117,
    AKEYCODE_META_RIGHT      = 118,
    AKEYCODE_FUNCTION        = 119,
    AKEYCODE_SYSRQ           = 120,
    AKEYCODE_BREAK           = 121,
    AKEYCODE_MOVE_HOME       = 122,
    AKEYCODE_MOVE_END        = 123,
    AKEYCODE_INSERT          = 124,
    AKEYCODE_FORWARD         = 125,
    AKEYCODE_MEDIA_PLAY      = 126,
    AKEYCODE_MEDIA_PAUSE     = 127,
    AKEYCODE_MEDIA_CLOSE     = 128,
    AKEYCODE_MEDIA_EJECT     = 129,
    AKEYCODE_MEDIA_RECORD    = 130,
    AKEYCODE_F1              = 131,
    AKEYCODE_F2              = 132,
    AKEYCODE_F3              = 133,
    AKEYCODE_F4              = 134,
    AKEYCODE_F5              = 135,
    AKEYCODE_F6              = 136,
    AKEYCODE_F7              = 137,
    AKEYCODE_F8              = 138,
    AKEYCODE_F9              = 139,
    AKEYCODE_F10             = 140,
    AKEYCODE_F11             = 141,
    AKEYCODE_F12             = 142,
    AKEYCODE_NUM_LOCK        = 143,
    AKEYCODE_NUMPAD_0        = 144,
    AKEYCODE_NUMPAD_1        = 145,
    AKEYCODE_NUMPAD_2        = 146,
    AKEYCODE_NUMPAD_3        = 147,
    AKEYCODE_NUMPAD_4        = 148,
    AKEYCODE_NUMPAD_5        = 149,
    AKEYCODE_NUMPAD_6        = 150,
    AKEYCODE_NUMPAD_7        = 151,
    AKEYCODE_NUMPAD_8        = 152,
    AKEYCODE_NUMPAD_9        = 153,
    AKEYCODE_NUMPAD_DIVIDE   = 154,
    AKEYCODE_NUMPAD_MULTIPLY = 155,
    AKEYCODE_NUMPAD_SUBTRACT = 156,
    AKEYCODE_NUMPAD_ADD      = 157,
    AKEYCODE_NUMPAD_DOT      = 158,
    AKEYCODE_NUMPAD_COMMA    = 159,
    AKEYCODE_NUMPAD_ENTER    = 160,
    AKEYCODE_NUMPAD_EQUALS   = 161,
    AKEYCODE_NUMPAD_LEFT_PAREN = 162,
    AKEYCODE_NUMPAD_RIGHT_PAREN = 163,
    AKEYCODE_VOLUME_MUTE     = 164,
    AKEYCODE_INFO            = 165,
    AKEYCODE_CHANNEL_UP      = 166,
    AKEYCODE_CHANNEL_DOWN    = 167,
    AKEYCODE_ZOOM_IN         = 168,
    AKEYCODE_ZOOM_OUT        = 169,
    AKEYCODE_TV              = 170,
    AKEYCODE_WINDOW          = 171,
    AKEYCODE_GUIDE           = 172,
    AKEYCODE_DVR             = 173,
    AKEYCODE_BOOKMARK        = 174,
    AKEYCODE_CAPTIONS        = 175,
    AKEYCODE_SETTINGS        = 176,
    AKEYCODE_TV_POWER        = 177,
    AKEYCODE_TV_INPUT        = 178,
    AKEYCODE_STB_POWER       = 179,
    AKEYCODE_STB_INPUT       = 180,
    AKEYCODE_AVR_POWER       = 181,
    AKEYCODE_AVR_INPUT       = 182,
    AKEYCODE_PROG_RED        = 183,
    AKEYCODE_PROG_GREEN      = 184,
    AKEYCODE_PROG_YELLOW     = 185,
    AKEYCODE_PROG_BLUE       = 186,
    AKEYCODE_APP_SWITCH      = 187,
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
};

/*
 * Meta key / modifer state.
 */
enum {
    AMETA_CTRL_ON = 0x1000,
};
#endif

#include <stdint.h>

#include <boolean.h>

uint8_t *android_keyboard_state_get(unsigned port);

#endif
