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

enum input_game_focus_cmd_type
{
   GAME_FOCUS_CMD_OFF = 0,
   GAME_FOCUS_CMD_ON,
   GAME_FOCUS_CMD_TOGGLE,
   GAME_FOCUS_CMD_REAPPLY
};

/* Input config. */
struct input_bind_map
{
   const char *base;
   enum msg_hash_enums desc;
   /* Meta binds get input as prefix, not input_playerN".
    * 0 = libretro related.
    * 1 = Common hotkey.
    * 2 = Uncommon/obscure hotkey.
    */
   uint8_t meta;
   uint8_t retro_key;
   bool valid;
};


/* Turbo support. */
struct turbo_buttons
{
   int32_t turbo_pressed[MAX_USERS];
   unsigned count;
   uint16_t enable[MAX_USERS];
   bool frame_enable[MAX_USERS];
   bool mode1_enable[MAX_USERS];
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

typedef struct retro_keybind retro_keybind_set[RARCH_BIND_LIST_END];

typedef struct
{
   uint32_t data[8];
   uint16_t analogs[8];
   uint16_t analog_buttons[16];
} input_bits_t;

typedef struct input_mapper
{
   /* Left X, Left Y, Right X, Right Y */
   int16_t analog_value[MAX_USERS][8];
   /* The whole keyboard state */
   uint32_t keys[RETROK_LAST / 32 + 1];
   /* RetroPad button state of remapped keyboard keys */
   unsigned key_button[RETROK_LAST];
   /* This is a bitmask of (1 << key_bind_id). */
   input_bits_t buttons[MAX_USERS];
} input_mapper_t;

typedef struct input_game_focus_state
{
   bool enabled;
   bool core_requested;
} input_game_focus_state_t;

#define INPUT_CONFIG_BIND_MAP_GET(i) ((const struct input_bind_map*)&input_config_bind_map[(i)])

extern const struct input_bind_map input_config_bind_map[RARCH_BIND_LIST_END_NULL];

typedef struct rarch_joypad_driver input_device_driver_t;
typedef struct input_keyboard_line input_keyboard_line_t;
typedef struct rarch_joypad_info rarch_joypad_info_t;
typedef struct input_driver input_driver_t;
typedef struct input_keyboard_ctx_wait input_keyboard_ctx_wait_t;
typedef struct turbo_buttons turbo_buttons_t;
typedef struct joypad_connection joypad_connection_t;
#endif /* __INPUT_TYPES__H */
