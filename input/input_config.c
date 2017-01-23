/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Brad Parker
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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <file/config_file.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "input_config.h"
#include "input_keymaps.h"
#include "input_remapping.h"

#include "../msg_hash.h"
#include "../configuration.h"
#include "../file_path_special.h"
#include "../verbosity.h"

/* Input config. */
struct input_bind_map
{
   bool valid;

   /* Meta binds get input as prefix, not input_playerN".
    * 0 = libretro related.
    * 1 = Common hotkey.
    * 2 = Uncommon/obscure hotkey.
    */
   unsigned meta;

   const char *base;
   enum msg_hash_enums desc;
   unsigned retro_key;
};

static const char *bind_user_prefix[MAX_USERS] = {
   "input_player1",
   "input_player2",
   "input_player3",
   "input_player4",
   "input_player5",
   "input_player6",
   "input_player7",
   "input_player8",
   "input_player9",
   "input_player10",
   "input_player11",
   "input_player12",
   "input_player13",
   "input_player14",
   "input_player15",
   "input_player16",
};

#define DECLARE_BIND(x, bind, desc) { true, 0, #x, desc, bind }
#define DECLARE_META_BIND(level, x, bind, desc) { true, level, #x, desc, bind }

const struct input_bind_map input_config_bind_map[RARCH_BIND_LIST_END_NULL] = {
      DECLARE_BIND(b,         RETRO_DEVICE_ID_JOYPAD_B,      MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B),
      DECLARE_BIND(y,         RETRO_DEVICE_ID_JOYPAD_Y,      MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y),
      DECLARE_BIND(select,    RETRO_DEVICE_ID_JOYPAD_SELECT, MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT),
      DECLARE_BIND(start,     RETRO_DEVICE_ID_JOYPAD_START,  MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START),
      DECLARE_BIND(up,        RETRO_DEVICE_ID_JOYPAD_UP,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP),
      DECLARE_BIND(down,      RETRO_DEVICE_ID_JOYPAD_DOWN,   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN),
      DECLARE_BIND(left,      RETRO_DEVICE_ID_JOYPAD_LEFT,   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT),
      DECLARE_BIND(right,     RETRO_DEVICE_ID_JOYPAD_RIGHT,  MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT),
      DECLARE_BIND(a,         RETRO_DEVICE_ID_JOYPAD_A,      MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A),
      DECLARE_BIND(x,         RETRO_DEVICE_ID_JOYPAD_X,      MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X),
      DECLARE_BIND(l,         RETRO_DEVICE_ID_JOYPAD_L,      MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L),
      DECLARE_BIND(r,         RETRO_DEVICE_ID_JOYPAD_R,      MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R),
      DECLARE_BIND(l2,        RETRO_DEVICE_ID_JOYPAD_L2,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2),
      DECLARE_BIND(r2,        RETRO_DEVICE_ID_JOYPAD_R2,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2),
      DECLARE_BIND(l3,        RETRO_DEVICE_ID_JOYPAD_L3,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3),
      DECLARE_BIND(r3,        RETRO_DEVICE_ID_JOYPAD_R3,     MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3),
      DECLARE_BIND(l_x_plus,  RARCH_ANALOG_LEFT_X_PLUS,      MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS),
      DECLARE_BIND(l_x_minus, RARCH_ANALOG_LEFT_X_MINUS,     MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS),
      DECLARE_BIND(l_y_plus,  RARCH_ANALOG_LEFT_Y_PLUS,      MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS),
      DECLARE_BIND(l_y_minus, RARCH_ANALOG_LEFT_Y_MINUS,     MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS),
      DECLARE_BIND(r_x_plus,  RARCH_ANALOG_RIGHT_X_PLUS,     MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS),
      DECLARE_BIND(r_x_minus, RARCH_ANALOG_RIGHT_X_MINUS,    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS),
      DECLARE_BIND(r_y_plus,  RARCH_ANALOG_RIGHT_Y_PLUS,     MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS),
      DECLARE_BIND(r_y_minus, RARCH_ANALOG_RIGHT_Y_MINUS,    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS),

      DECLARE_BIND(turbo,     RARCH_TURBO_ENABLE,            MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE),

      DECLARE_META_BIND(1, toggle_fast_forward,   RARCH_FAST_FORWARD_KEY,      MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY),
      DECLARE_META_BIND(2, hold_fast_forward,     RARCH_FAST_FORWARD_HOLD_KEY, MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY),
      DECLARE_META_BIND(1, load_state,            RARCH_LOAD_STATE_KEY,        MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY),
      DECLARE_META_BIND(1, save_state,            RARCH_SAVE_STATE_KEY,        MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY),
      DECLARE_META_BIND(2, toggle_fullscreen,     RARCH_FULLSCREEN_TOGGLE_KEY, MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY),
      DECLARE_META_BIND(2, exit_emulator,         RARCH_QUIT_KEY,              MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY),
      DECLARE_META_BIND(2, state_slot_increase,   RARCH_STATE_SLOT_PLUS,       MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS),
      DECLARE_META_BIND(2, state_slot_decrease,   RARCH_STATE_SLOT_MINUS,      MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS),
      DECLARE_META_BIND(1, rewind,                RARCH_REWIND,                MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND),
      DECLARE_META_BIND(2, movie_record_toggle,   RARCH_MOVIE_RECORD_TOGGLE,   MENU_ENUM_LABEL_VALUE_INPUT_META_MOVIE_RECORD_TOGGLE),
      DECLARE_META_BIND(2, pause_toggle,          RARCH_PAUSE_TOGGLE,          MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE),
      DECLARE_META_BIND(2, frame_advance,         RARCH_FRAMEADVANCE,          MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE),
      DECLARE_META_BIND(2, reset,                 RARCH_RESET,                 MENU_ENUM_LABEL_VALUE_INPUT_META_RESET),
      DECLARE_META_BIND(2, shader_next,           RARCH_SHADER_NEXT,           MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT),
      DECLARE_META_BIND(2, shader_prev,           RARCH_SHADER_PREV,           MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV),
      DECLARE_META_BIND(2, cheat_index_plus,      RARCH_CHEAT_INDEX_PLUS,      MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS),
      DECLARE_META_BIND(2, cheat_index_minus,     RARCH_CHEAT_INDEX_MINUS,     MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS),
      DECLARE_META_BIND(2, cheat_toggle,          RARCH_CHEAT_TOGGLE,          MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE),
      DECLARE_META_BIND(2, screenshot,            RARCH_SCREENSHOT,            MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT),
      DECLARE_META_BIND(2, audio_mute,            RARCH_MUTE,                  MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE),
      DECLARE_META_BIND(2, osk_toggle,            RARCH_OSK,                   MENU_ENUM_LABEL_VALUE_INPUT_META_OSK),
      DECLARE_META_BIND(2, netplay_flip_players_1_2, RARCH_NETPLAY_FLIP,       MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_FLIP),
      DECLARE_META_BIND(2, netplay_game_watch,    RARCH_NETPLAY_GAME_WATCH,    MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH),
      DECLARE_META_BIND(2, slowmotion,            RARCH_SLOWMOTION,            MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION),
      DECLARE_META_BIND(2, enable_hotkey,         RARCH_ENABLE_HOTKEY,         MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY),
      DECLARE_META_BIND(2, volume_up,             RARCH_VOLUME_UP,             MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP),
      DECLARE_META_BIND(2, volume_down,           RARCH_VOLUME_DOWN,           MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN),
      DECLARE_META_BIND(2, overlay_next,          RARCH_OVERLAY_NEXT,          MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT),
      DECLARE_META_BIND(2, disk_eject_toggle,     RARCH_DISK_EJECT_TOGGLE,     MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE),
      DECLARE_META_BIND(2, disk_next,             RARCH_DISK_NEXT,             MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT),
      DECLARE_META_BIND(2, disk_prev,             RARCH_DISK_PREV,             MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV),
      DECLARE_META_BIND(2, grab_mouse_toggle,     RARCH_GRAB_MOUSE_TOGGLE,     MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE),
      DECLARE_META_BIND(2, game_focus_toggle,     RARCH_GAME_FOCUS_TOGGLE,     MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE),
#ifdef HAVE_MENU
      DECLARE_META_BIND(1, menu_toggle,           RARCH_MENU_TOGGLE,           MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE),
#endif
};

static const void *input_config_bind_map_get(unsigned i)
{
   return (const struct input_bind_map*)&input_config_bind_map[i];
}

bool input_config_bind_map_get_valid(unsigned i)
{
   const struct input_bind_map *keybind = 
      (const struct input_bind_map*)input_config_bind_map_get(i);
   if (!keybind)
      return false;
   return keybind->valid;
}

unsigned input_config_bind_map_get_meta(unsigned i)
{
   const struct input_bind_map *keybind = 
      (const struct input_bind_map*)input_config_bind_map_get(i);
   if (!keybind)
      return 0;
   return keybind->meta;
}

const char *input_config_bind_map_get_base(unsigned i)
{
   const struct input_bind_map *keybind = 
      (const struct input_bind_map*)input_config_bind_map_get(i);
   if (!keybind)
      return NULL;
   return keybind->base;
}

const char *input_config_bind_map_get_desc(unsigned i)
{
   const struct input_bind_map *keybind = 
      (const struct input_bind_map*)input_config_bind_map_get(i);
   if (!keybind)
      return NULL;
   return msg_hash_to_str(keybind->desc);
}

void input_config_parse_key(void *data,
      const char *prefix, const char *btn,
      struct retro_keybind *bind)
{
   char tmp[64];
   char key[64];
   config_file_t *conf = (config_file_t*)data;

   tmp[0] = key[0] = '\0';

   fill_pathname_join_delim(key, prefix, btn, '_', sizeof(key));

   if (config_get_array(conf, key, tmp, sizeof(tmp)))
      bind->key = input_config_translate_str_to_rk(tmp);
}

const char *input_config_get_prefix(unsigned user, bool meta)
{
   if (user == 0)
      return meta ? "input" : bind_user_prefix[user];

   if (user != 0 && !meta)
      return bind_user_prefix[user];

   /* Don't bother with meta bind for anyone else than first user. */
   return NULL;
}

static enum retro_key find_rk_bind(const char *str)
{
   size_t i;

   for (i = 0; input_config_key_map[i].str; i++)
   {
      if (string_is_equal_noncase(input_config_key_map[i].str, str))
         return input_config_key_map[i].key;
   }

   RARCH_WARN("Key name %s not found.\n", str);
   return RETROK_UNKNOWN;
}

/**
 * input_config_translate_str_to_rk:
 * @str                            : String to translate to key ID.
 *
 * Translates tring representation to key identifier.
 *
 * Returns: key identifier.
 **/
enum retro_key input_config_translate_str_to_rk(const char *str)
{
   if (strlen(str) == 1 && isalpha((int)*str))
      return (enum retro_key)(RETROK_a + (tolower((int)*str) - (int)'a'));
   return find_rk_bind(str);
}

/**
 * input_config_translate_str_to_bind_id:
 * @str                            : String to translate to bind ID.
 *
 * Translate string representation to bind ID.
 *
 * Returns: Bind ID value on success, otherwise 
 * RARCH_BIND_LIST_END on not found.
 **/
unsigned input_config_translate_str_to_bind_id(const char *str)
{
   unsigned i;

   for (i = 0; input_config_bind_map[i].valid; i++)
      if (string_is_equal(str, input_config_bind_map[i].base))
         return i;

   return RARCH_BIND_LIST_END;
}

static void parse_hat(struct retro_keybind *bind, const char *str)
{
   uint16_t     hat;
   uint16_t hat_dir = 0;
   char        *dir = NULL;

   if (!bind || !str)
      return;

   if (!isdigit((int)*str))
      return;

   hat = strtoul(str, &dir, 0);

   if (!dir)
   {
      RARCH_WARN("Found invalid hat in config!\n");
      return;
   }

   if      (string_is_equal_noncase(dir, "up"))
      hat_dir = HAT_UP_MASK;
   else if (string_is_equal_noncase(dir, "down"))
      hat_dir = HAT_DOWN_MASK;
   else if (string_is_equal_noncase(dir, "left"))
      hat_dir = HAT_LEFT_MASK;
   else if (string_is_equal_noncase(dir, "right"))
      hat_dir = HAT_RIGHT_MASK;

   if (hat_dir)
      bind->joykey = HAT_MAP(hat, hat_dir);
}

void input_config_parse_joy_button(void *data, const char *prefix,
      const char *btn, struct retro_keybind *bind)
{
   char str[256];
   char tmp[64];
   char key[64];
   char key_label[64];
   char *tmp_a              = NULL;
   config_file_t *conf      = (config_file_t*)data;

   str[0] = tmp[0] = key[0] = key_label[0] = '\0';

   fill_pathname_join_delim(str, prefix, btn,
         '_', sizeof(str));
   fill_pathname_join_delim(key, str,
         "btn", '_', sizeof(key));
   fill_pathname_join_delim(key_label, str,
         "btn_label", '_', sizeof(key_label));

   if (config_get_array(conf, key, tmp, sizeof(tmp)))
   {
      btn = tmp;
      if (string_is_equal(btn, file_path_str(FILE_PATH_NUL)))
         bind->joykey = NO_BTN;
      else
      {
         if (*btn == 'h')
            parse_hat(bind, btn + 1);
         else
            bind->joykey = strtoull(tmp, NULL, 0);
      }
   }

   if (config_get_string(conf, key_label, &tmp_a))
   {
      strlcpy(bind->joykey_label, tmp_a, sizeof(bind->joykey_label));
      free(tmp_a);
   }
}

void input_config_parse_joy_axis(void *data, const char *prefix,
      const char *axis, struct retro_keybind *bind)
{
   char str[256];
   char       tmp[64];
   char       key[64];
   char key_label[64];
   char        *tmp_a       = NULL;
   config_file_t *conf      = (config_file_t*)data;

   str[0] = tmp[0] = key[0] = key_label[0] = '\0';

   fill_pathname_join_delim(str, prefix, axis,
         '_', sizeof(str));
   fill_pathname_join_delim(key, str,
         "axis", '_', sizeof(key));
   fill_pathname_join_delim(key_label, str,
         "axis_label", '_', sizeof(key_label));

   if (config_get_array(conf, key, tmp, sizeof(tmp)))
   {
      if (string_is_equal(tmp, file_path_str(FILE_PATH_NUL)))
         bind->joyaxis = AXIS_NONE;
      else if (strlen(tmp) >= 2 && (*tmp == '+' || *tmp == '-'))
      {
         int i_axis = strtol(tmp + 1, NULL, 0);
         if (*tmp == '+')
            bind->joyaxis = AXIS_POS(i_axis);
         else
            bind->joyaxis = AXIS_NEG(i_axis);
      }

      /* Ensure that D-pad emulation doesn't screw this over. */
      bind->orig_joyaxis = bind->joyaxis;
   }

   if (config_get_string(conf, key_label, &tmp_a))
   {
      strlcpy(bind->joyaxis_label, tmp_a, sizeof(bind->joyaxis_label));
      free(tmp_a);
   }
}

static void input_config_get_bind_string_joykey(char *buf, const char *prefix,
      const struct retro_keybind *bind, size_t size)
{
   settings_t *settings = config_get_ptr();
   bool label_show      = settings->input.input_descriptor_label_show;

   if (GET_HAT_DIR(bind->joykey))
   {
      const char *dir = "?";

      switch (GET_HAT_DIR(bind->joykey))
      {
         case HAT_UP_MASK:
            dir = "up";
            break;
         case HAT_DOWN_MASK:
            dir = "down";
            break;
         case HAT_LEFT_MASK:
            dir = "left";
            break;
         case HAT_RIGHT_MASK:
            dir = "right";
            break;
         default:
            break;
      }

      if (!string_is_empty(bind->joykey_label) && label_show)
         snprintf(buf, size, "%s %s ", prefix, bind->joykey_label);
      else
         snprintf(buf, size, "%sHat #%u %s (%s)", prefix,
               (unsigned)GET_HAT(bind->joykey), dir,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
   }
   else
   {
      if (!string_is_empty(bind->joykey_label) && label_show)
         snprintf(buf, size, "%s%s (btn) ", prefix, bind->joykey_label);
      else
         snprintf(buf, size, "%s%u (%s) ", prefix, (unsigned)bind->joykey,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
   }
}

static void input_config_get_bind_string_joyaxis(char *buf, const char *prefix,
      const struct retro_keybind *bind, size_t size)
{
   unsigned axis        = 0;
   char dir             = '\0';
   settings_t *settings = config_get_ptr();

   if (AXIS_NEG_GET(bind->joyaxis) != AXIS_DIR_NONE)
   {
      dir = '-';
      axis = AXIS_NEG_GET(bind->joyaxis);
   }
   else if (AXIS_POS_GET(bind->joyaxis) != AXIS_DIR_NONE)
   {
      dir = '+';
      axis = AXIS_POS_GET(bind->joyaxis);
   }
   if (!string_is_empty(bind->joyaxis_label) 
         && settings->input.input_descriptor_label_show)
      snprintf(buf, size, "%s%s (axis) ", prefix, bind->joyaxis_label);
   else
      snprintf(buf, size, "%s%c%u (%s) ", prefix, dir, axis,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
}

void input_config_get_bind_string(char *buf, const struct retro_keybind *bind,
      const struct retro_keybind *auto_bind, size_t size)
{
#ifndef RARCH_CONSOLE
   char key[64];
   char keybuf[64];

   key[0] = keybuf[0] = '\0';
#endif

   *buf = '\0';
   if (bind->joykey != NO_BTN)
      input_config_get_bind_string_joykey(buf, "", bind, size);
   else if (bind->joyaxis != AXIS_NONE)
      input_config_get_bind_string_joyaxis(buf, "", bind, size);
   else if (auto_bind && auto_bind->joykey != NO_BTN)
      input_config_get_bind_string_joykey(buf, "Auto: ", auto_bind, size);
   else if (auto_bind && auto_bind->joyaxis != AXIS_NONE)
      input_config_get_bind_string_joyaxis(buf, "Auto: ", auto_bind, size);

#ifndef RARCH_CONSOLE
   input_keymaps_translate_rk_to_str(bind->key, key, sizeof(key));
   if (string_is_equal(key, file_path_str(FILE_PATH_NUL)))
      *key = '\0';

   snprintf(keybuf, sizeof(keybuf), msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_KEY), key);
   strlcat(buf, keybuf, size);
#endif
}

const char *input_config_get_device_name(unsigned port)
{
   settings_t *settings = config_get_ptr();
   if (string_is_empty(settings->input.device_names[port]))
      return NULL;
   return settings->input.device_names[port];
}

void input_config_set_device_name(unsigned port, const char *name)
{
   if (!string_is_empty(name))
   {
      settings_t *settings = config_get_ptr();
      strlcpy(settings->input.device_names[port],
            name,
            sizeof(settings->input.device_names[port]));
   }
}

void input_config_set_device(unsigned port, unsigned id)
{
   settings_t *settings = config_get_ptr();

   if (settings)
      settings->input.libretro_device[port] = id;
}

bool input_config_get_bind_idx(unsigned port, unsigned *joy_idx_real)
{
   settings_t *settings = config_get_ptr();
   unsigned joy_idx     = settings->input.joypad_map[port];

   if (joy_idx >= MAX_USERS)
      return false;

   *joy_idx_real        = joy_idx;
   return true;
}

const struct retro_keybind *input_config_get_bind_auto(unsigned port, unsigned id)
{
   settings_t *settings = config_get_ptr();
   unsigned joy_idx     = 0;

   if (settings)
      joy_idx = settings->input.joypad_map[port];

   if (joy_idx < MAX_USERS)
      return &settings->input.autoconf_binds[joy_idx][id];
   return NULL;
}

