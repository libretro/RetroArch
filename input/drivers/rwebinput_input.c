/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2015 - Michael Lelli
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <encodings/crc32.h>
#include <encodings/utf.h>

#include <emscripten/html5.h>

#include "../input_driver.h"
#include "../input_keymaps.h"

#include "../../tasks/tasks_internal.h"
#include "../../configuration.h"
#include "../../verbosity.h"

typedef struct rwebinput_key_to_code_map_entry
{
   const char *key;
   enum retro_key rk;
} rwebinput_key_to_code_map_entry_t;

typedef struct
{
   bool keys[RETROK_LAST];
} rwebinput_key_states_t;

typedef struct rwebinput_input
{
   rwebinput_key_states_t keyboard;
   int mouse_x;
   int mouse_y;
   char mouse_l;
   char mouse_r;
   bool blocked;
} rwebinput_input_t;

/* KeyboardEvent.keyCode has been deprecated for a while and doesn't have
 * separate left/right modifer codes, so we have to map string labels from
 * KeyboardEvent.code to retro keys */
static const rwebinput_key_to_code_map_entry_t rwebinput_key_to_code_map[] =
{
   { "KeyA", RETROK_a },
   { "KeyB", RETROK_b },
   { "KeyC", RETROK_c },
   { "KeyD", RETROK_d },
   { "KeyE", RETROK_e },
   { "KeyF", RETROK_f },
   { "KeyG", RETROK_g },
   { "KeyH", RETROK_h },
   { "KeyI", RETROK_i },
   { "KeyJ", RETROK_j },
   { "KeyK", RETROK_k },
   { "KeyL", RETROK_l },
   { "KeyM", RETROK_m },
   { "KeyN", RETROK_n },
   { "KeyO", RETROK_o },
   { "KeyP", RETROK_p },
   { "KeyQ", RETROK_q },
   { "KeyR", RETROK_r },
   { "KeyS", RETROK_s },
   { "KeyT", RETROK_t },
   { "KeyU", RETROK_u },
   { "KeyV", RETROK_v },
   { "KeyW", RETROK_w },
   { "KeyX", RETROK_x },
   { "KeyY", RETROK_y },
   { "KeyZ", RETROK_z },
   { "ArrowLeft", RETROK_LEFT },
   { "ArrowRight", RETROK_RIGHT },
   { "ArrowUp", RETROK_UP },
   { "ArrowDown", RETROK_DOWN },
   { "Enter", RETROK_RETURN },
   { "NumpadEnter", RETROK_KP_ENTER },
   { "Tab", RETROK_TAB },
   { "Insert", RETROK_INSERT },
   { "Delete", RETROK_DELETE },
   { "End", RETROK_END },
   { "Home", RETROK_HOME },
   { "ShiftRight", RETROK_RSHIFT },
   { "ShiftLeft", RETROK_LSHIFT },
   { "ControlLeft", RETROK_LCTRL },
   { "AltLeft", RETROK_LALT },
   { "Space", RETROK_SPACE },
   { "Escape", RETROK_ESCAPE },
   { "NumpadAdd", RETROK_KP_PLUS },
   { "NumpadSubtract", RETROK_KP_MINUS },
   { "F1", RETROK_F1 },
   { "F2", RETROK_F2 },
   { "F3", RETROK_F3 },
   { "F4", RETROK_F4 },
   { "F5", RETROK_F5 },
   { "F6", RETROK_F6 },
   { "F7", RETROK_F7 },
   { "F8", RETROK_F8 },
   { "F9", RETROK_F9 },
   { "F10", RETROK_F10 },
   { "F11", RETROK_F11 },
   { "F12", RETROK_F12 },
   { "Digit0", RETROK_0 },
   { "Digit1", RETROK_1 },
   { "Digit2", RETROK_2 },
   { "Digit3", RETROK_3 },
   { "Digit4", RETROK_4 },
   { "Digit5", RETROK_5 },
   { "Digit6", RETROK_6 },
   { "Digit7", RETROK_7 },
   { "Digit8", RETROK_8 },
   { "Digit9", RETROK_9 },
   { "PageUp", RETROK_PAGEUP },
   { "PageDown", RETROK_PAGEDOWN },
   { "Numpad0", RETROK_KP0 },
   { "Numpad1", RETROK_KP1 },
   { "Numpad2", RETROK_KP2 },
   { "Numpad3", RETROK_KP3 },
   { "Numpad4", RETROK_KP4 },
   { "Numpad5", RETROK_KP5 },
   { "Numpad6", RETROK_KP6 },
   { "Numpad7", RETROK_KP7 },
   { "Numpad8", RETROK_KP8 },
   { "Numpad9", RETROK_KP9 },
   { "Period", RETROK_PERIOD },
   { "CapsLock", RETROK_CAPSLOCK },
   { "NumLock", RETROK_NUMLOCK },
   { "Backspace", RETROK_BACKSPACE },
   { "NumpadMultiply", RETROK_KP_MULTIPLY },
   { "NumpadDivide", RETROK_KP_DIVIDE },
   { "PrintScreen", RETROK_PRINT },
   { "ScrollLock", RETROK_SCROLLOCK },
   { "Backquote", RETROK_BACKQUOTE },
   { "Pause", RETROK_PAUSE },
   { "Quote", RETROK_QUOTE },
   { "Comma", RETROK_COMMA },
   { "Minus", RETROK_MINUS },
   { "Slash", RETROK_SLASH },
   { "Semicolon", RETROK_SEMICOLON },
   { "Equal", RETROK_EQUALS },
   { "BracketLeft", RETROK_LEFTBRACKET },
   { "Backslash", RETROK_BACKSLASH },
   { "BracketRight", RETROK_RIGHTBRACKET },
   { "NumpadDecimal", RETROK_KP_PERIOD },
   { "NumpadEqual", RETROK_KP_EQUALS },
   { "ControlRight", RETROK_RCTRL },
   { "AltRight", RETROK_RALT },
   { "F13", RETROK_F13 },
   { "F14", RETROK_F14 },
   { "F15", RETROK_F15 },
   { "MetaRight", RETROK_RMETA },
   { "MetaLeft", RETROK_LMETA },
   { "Help", RETROK_HELP },
   { "ContextMenu", RETROK_MENU },
   { "Power", RETROK_POWER },
};

static bool g_initialized;
static rwebinput_key_states_t *g_keyboard;

/* to make the string labels for codes from JavaScript work, we convert them
 * to CRC32 hashes for the LUT */
static void rwebinput_generate_lut(void)
{
   int i;
   struct rarch_key_map *key_map;

   retro_assert(ARRAY_SIZE(rarch_key_map_rwebinput) ==
      ARRAY_SIZE(rwebinput_key_to_code_map) + 1);

   for (i = 0; i < ARRAY_SIZE(rwebinput_key_to_code_map); i++)
   {
      int j;
      uint32_t crc;
      const rwebinput_key_to_code_map_entry_t *key_to_code =
         &rwebinput_key_to_code_map[i];
      key_map = &rarch_key_map_rwebinput[i];
      crc = encoding_crc32(0, (const uint8_t *)key_to_code->key,
         strlen(key_to_code->key));

      /* sanity check: make sure there's no collisions */
      for (j = 0; j < i; j++)
      {
         retro_assert(rarch_key_map_rwebinput[j].sym != crc);
      }

      key_map->rk = key_to_code->rk;
      key_map->sym = crc;
   }

   /* set terminating entry */
   key_map = &rarch_key_map_rwebinput[ARRAY_SIZE(rarch_key_map_rwebinput) - 1];
   key_map->rk = RETROK_UNKNOWN;
   key_map->sym = 0;
}

static EM_BOOL rwebinput_input_cb(int event_type,
   const EmscriptenKeyboardEvent *key_event, void *user_data)
{
   uint32_t crc;
   uint32_t keycode;
   unsigned translated_keycode;
   uint32_t character = 0;
   uint16_t mod = 0;
   bool keydown = event_type == EMSCRIPTEN_EVENT_KEYDOWN;

   (void)user_data;

   /* capture keypress events and silence them */
   if (event_type == EMSCRIPTEN_EVENT_KEYPRESS)
      return EM_TRUE;

   /* a printable key: populate character field */
   if (utf8len(key_event->key) == 1)
   {
      const char *key_ptr = &key_event->key[0];
      character = utf8_walk(&key_ptr);
   }

   if (key_event->ctrlKey)
      mod |= RETROKMOD_CTRL;
   if (key_event->altKey)
      mod |= RETROKMOD_ALT;
   if (key_event->shiftKey)
      mod |= RETROKMOD_SHIFT;
   if (key_event->metaKey)
      mod |= RETROKMOD_META;

   keycode = encoding_crc32(0, (const uint8_t *)key_event->code,
      strnlen(key_event->code, sizeof(key_event->code)));
   translated_keycode = input_keymaps_translate_keysym_to_rk(keycode);

   input_keyboard_event(keydown, translated_keycode, character, mod,
      RETRO_DEVICE_KEYBOARD);

   if (translated_keycode < RETROK_LAST)
   {
      g_keyboard->keys[translated_keycode] = keydown;
   }

   return EM_TRUE;
}

static void *rwebinput_input_init(const char *joypad_driver)
{
   rwebinput_input_t *rwebinput =
      (rwebinput_input_t*)calloc(1, sizeof(*rwebinput));
   g_keyboard =
      (rwebinput_key_states_t*)calloc(1, sizeof(rwebinput_key_states_t));

   if (!rwebinput || !g_keyboard)
      goto error;

   if (!g_initialized)
   {
      EMSCRIPTEN_RESULT r;

      g_initialized = true;
      rwebinput_generate_lut();

      /* emscripten currently doesn't have an API to remove handlers, so make
       * once and reuse it */
      r = emscripten_set_keydown_callback("#document", NULL, false,
         rwebinput_input_cb);
      if (r != EMSCRIPTEN_RESULT_SUCCESS)
      {
         RARCH_ERR(
            "[EMSCRIPTEN/INPUT] failed to create keydown callback: %d\n", r);
      }

      r = emscripten_set_keyup_callback("#document", NULL, false,
         rwebinput_input_cb);
      if (r != EMSCRIPTEN_RESULT_SUCCESS)
      {
         RARCH_ERR(
            "[EMSCRIPTEN/INPUT] failed to create keydown callback: %d\n", r);
      }

      r = emscripten_set_keypress_callback("#document", NULL, false,
         rwebinput_input_cb);
      if (r != EMSCRIPTEN_RESULT_SUCCESS)
      {
         RARCH_ERR(
            "[EMSCRIPTEN/INPUT] failed to create keypress callback: %d\n", r);
      }
   }

   input_keymaps_init_keyboard_lut(rarch_key_map_rwebinput);

   return rwebinput;

error:
   free(g_keyboard);
   free(rwebinput);
   return NULL;
}

static bool rwebinput_key_pressed(void *data, int key)
{
   unsigned sym;
   bool ret;
   rwebinput_input_t *rwebinput = (rwebinput_input_t*)data;

   if (key >= RETROK_LAST)
      return false;

   return rwebinput->keyboard.keys[key];
}

static bool rwebinput_is_pressed(rwebinput_input_t *rwebinput,
      const struct retro_keybind *binds, unsigned id)
{
   if (id < RARCH_BIND_LIST_END)
   {
      const struct retro_keybind *bind = &binds[id];
      int key                          = binds[id].key;
      return bind->valid && (key < RETROK_LAST)
         && rwebinput_key_pressed(rwebinput, key);
   }

   return false;
}

static int16_t rwebinput_mouse_state(rwebinput_input_t *rwebinput, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_X:
         return (int16_t) rwebinput->mouse_x;
      case RETRO_DEVICE_ID_MOUSE_Y:
         return (int16_t) rwebinput->mouse_y;
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return rwebinput->mouse_l;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return rwebinput->mouse_r;
   }

   return 0;
}

static int16_t rwebinput_analog_pressed(rwebinput_input_t *rwebinput,
      const struct retro_keybind *binds, unsigned idx, unsigned id)
{
   int16_t pressed_minus = 0, pressed_plus = 0;
   unsigned id_minus = 0;
   unsigned id_plus  = 0;

   input_conv_analog_id_to_bind_id(idx, id, &id_minus, &id_plus);

   if (rwebinput_is_pressed(rwebinput, binds, id_minus))
      pressed_minus = -0x7fff;
   if (rwebinput_is_pressed(rwebinput, binds, id_plus))
      pressed_plus = 0x7fff;

   return pressed_plus + pressed_minus;
}

static int16_t rwebinput_input_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds,
      unsigned port, unsigned device, unsigned idx, unsigned id)
{
   rwebinput_input_t *rwebinput  = (rwebinput_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return rwebinput_is_pressed(rwebinput, binds[port], id);
      case RETRO_DEVICE_ANALOG:
         return rwebinput_analog_pressed(rwebinput, binds[port], idx, id);
      case RETRO_DEVICE_KEYBOARD:
         return rwebinput_key_pressed(rwebinput, id);
      case RETRO_DEVICE_MOUSE:
         return rwebinput_mouse_state(rwebinput, id);
   }

   return 0;
}

static void rwebinput_input_free(void *data)
{
   rwebinput_input_t *rwebinput = (rwebinput_input_t*)data;

   free(g_keyboard);
   g_keyboard = NULL;

   if (!rwebinput)
      return;

   free(rwebinput);
}

static void rwebinput_input_poll(void *data)
{
   unsigned i;
   rwebinput_input_t *rwebinput = (rwebinput_input_t*)data;

   memcpy(&rwebinput->keyboard, g_keyboard, sizeof(*g_keyboard));
}

static void rwebinput_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool rwebinput_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   (void)data;
   (void)port;
   (void)effect;
   (void)strength;

   return false;
}

static uint64_t rwebinput_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);
   caps |= (1 << RETRO_DEVICE_ANALOG);
   caps |= (1 << RETRO_DEVICE_KEYBOARD);
   caps |= (1 << RETRO_DEVICE_MOUSE);

   return caps;
}

static bool rwebinput_keyboard_mapping_is_blocked(void *data)
{
   rwebinput_input_t *rwebinput = (rwebinput_input_t*)data;
   if (!rwebinput)
      return false;
   return rwebinput->blocked;
}

static void rwebinput_keyboard_mapping_set_block(void *data, bool value)
{
   rwebinput_input_t *rwebinput = (rwebinput_input_t*)data;
   if (!rwebinput)
      return;
   rwebinput->blocked = value;
}

input_driver_t input_rwebinput = {
   rwebinput_input_init,
   rwebinput_input_poll,
   rwebinput_input_state,
   rwebinput_input_free,
   NULL,
   NULL,
   rwebinput_get_capabilities,
   "rwebinput",
   rwebinput_grab_mouse,
   NULL,
   rwebinput_set_rumble,
   NULL,
   NULL,
   rwebinput_keyboard_mapping_is_blocked,
   rwebinput_keyboard_mapping_set_block,
};
