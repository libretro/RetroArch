/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef __INPUT_TYPES__H
#define __INPUT_TYPES__H

#include "../msg_hash.h"

enum input_auto_game_focus_type
{
   AUTO_GAME_FOCUS_OFF = 0,
   AUTO_GAME_FOCUS_ON,
   AUTO_GAME_FOCUS_DETECT,
   AUTO_GAME_FOCUS_LAST
};

struct retro_keybind
{
   /* Human-readable label for the control. */
   char     *joykey_label;
   /* Human-readable label for an analog axis. */
   char     *joyaxis_label;
   /*
    * Joypad axis. Negative and positive axes are both 
    * represented by this variable.
    */
   uint32_t joyaxis;
   /* Default joy axis binding value for resetting bind to default. */
   uint32_t def_joyaxis;
   /* Used by input_{push,pop}_analog_dpad(). */
   uint32_t orig_joyaxis;

   enum msg_hash_enums enum_idx;

   enum retro_key key;

   uint16_t id;
   /* What mouse button ID has been mapped to this control. */
   uint16_t mbutton;
   /* Joypad key. Joypad POV (hats) are embedded into this key as well. */
   uint16_t joykey;
   /* Default key binding value (for resetting bind). */
   uint16_t def_joykey;
   /* Determines whether or not the binding is valid. */
   bool valid;
};

typedef struct
{
   uint32_t data[8];
   uint16_t analogs[8];
   uint16_t analog_buttons[16];
} input_bits_t;

typedef struct rarch_joypad_driver input_device_driver_t;
typedef struct input_keyboard_line input_keyboard_line_t;
typedef struct rarch_joypad_info rarch_joypad_info_t;
typedef struct input_driver input_driver_t;
typedef struct input_keyboard_ctx_wait input_keyboard_ctx_wait_t;

typedef struct joypad_connection joypad_connection_t;
typedef struct pad_connection_listener_interface pad_connection_listener_t;

#endif /* __INPUT_TYPES__H */
