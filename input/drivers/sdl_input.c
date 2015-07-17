/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2014-2015 - Higor Euripedes
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

#include <boolean.h>

#include "../../driver.h"

#include "SDL.h"
#include "../../gfx/video_context_driver.h"
#include "../../general.h"
#include "../../libretro.h"
#include "../input_autodetect.h"
#include "../input_common.h"
#include "../input_joypad.h"
#include "../input_keymaps.h"
#include "../keyboard_line.h"

typedef struct sdl_input
{
   bool blocked;
   const input_device_driver_t *joypad;

   int mouse_x, mouse_y;
   int mouse_abs_x, mouse_abs_y;
   int mouse_l, mouse_r, mouse_m, mouse_wu, mouse_wd, mouse_wl, mouse_wr;
} sdl_input_t;

const struct rarch_key_map rarch_key_map_sdl[] = {
   { SDLK_BACKSPACE, RETROK_BACKSPACE },
   { SDLK_TAB, RETROK_TAB },
   { SDLK_CLEAR, RETROK_CLEAR },
   { SDLK_RETURN, RETROK_RETURN },
   { SDLK_PAUSE, RETROK_PAUSE },
   { SDLK_ESCAPE, RETROK_ESCAPE },
   { SDLK_SPACE, RETROK_SPACE },
   { SDLK_EXCLAIM, RETROK_EXCLAIM },
   { SDLK_QUOTEDBL, RETROK_QUOTEDBL },
   { SDLK_HASH, RETROK_HASH },
   { SDLK_DOLLAR, RETROK_DOLLAR },
   { SDLK_AMPERSAND, RETROK_AMPERSAND },
   { SDLK_QUOTE, RETROK_QUOTE },
   { SDLK_LEFTPAREN, RETROK_LEFTPAREN },
   { SDLK_RIGHTPAREN, RETROK_RIGHTPAREN },
   { SDLK_ASTERISK, RETROK_ASTERISK },
   { SDLK_PLUS, RETROK_PLUS },
   { SDLK_COMMA, RETROK_COMMA },
   { SDLK_MINUS, RETROK_MINUS },
   { SDLK_PERIOD, RETROK_PERIOD },
   { SDLK_SLASH, RETROK_SLASH },
   { SDLK_0, RETROK_0 },
   { SDLK_1, RETROK_1 },
   { SDLK_2, RETROK_2 },
   { SDLK_3, RETROK_3 },
   { SDLK_4, RETROK_4 },
   { SDLK_5, RETROK_5 },
   { SDLK_6, RETROK_6 },
   { SDLK_7, RETROK_7 },
   { SDLK_8, RETROK_8 },
   { SDLK_9, RETROK_9 },
   { SDLK_COLON, RETROK_COLON },
   { SDLK_SEMICOLON, RETROK_SEMICOLON },
   { SDLK_LESS, RETROK_LESS },
   { SDLK_EQUALS, RETROK_EQUALS },
   { SDLK_GREATER, RETROK_GREATER },
   { SDLK_QUESTION, RETROK_QUESTION },
   { SDLK_AT, RETROK_AT },
   { SDLK_LEFTBRACKET, RETROK_LEFTBRACKET },
   { SDLK_BACKSLASH, RETROK_BACKSLASH },
   { SDLK_RIGHTBRACKET, RETROK_RIGHTBRACKET },
   { SDLK_CARET, RETROK_CARET },
   { SDLK_UNDERSCORE, RETROK_UNDERSCORE },
   { SDLK_BACKQUOTE, RETROK_BACKQUOTE },
   { SDLK_a, RETROK_a },
   { SDLK_b, RETROK_b },
   { SDLK_c, RETROK_c },
   { SDLK_d, RETROK_d },
   { SDLK_e, RETROK_e },
   { SDLK_f, RETROK_f },
   { SDLK_g, RETROK_g },
   { SDLK_h, RETROK_h },
   { SDLK_i, RETROK_i },
   { SDLK_j, RETROK_j },
   { SDLK_k, RETROK_k },
   { SDLK_l, RETROK_l },
   { SDLK_m, RETROK_m },
   { SDLK_n, RETROK_n },
   { SDLK_o, RETROK_o },
   { SDLK_p, RETROK_p },
   { SDLK_q, RETROK_q },
   { SDLK_r, RETROK_r },
   { SDLK_s, RETROK_s },
   { SDLK_t, RETROK_t },
   { SDLK_u, RETROK_u },
   { SDLK_v, RETROK_v },
   { SDLK_w, RETROK_w },
   { SDLK_x, RETROK_x },
   { SDLK_y, RETROK_y },
   { SDLK_z, RETROK_z },
   { SDLK_DELETE, RETROK_DELETE },
#ifdef HAVE_SDL2
   { SDLK_KP_0, RETROK_KP0 },
   { SDLK_KP_1, RETROK_KP1 },
   { SDLK_KP_2, RETROK_KP2 },
   { SDLK_KP_3, RETROK_KP3 },
   { SDLK_KP_4, RETROK_KP4 },
   { SDLK_KP_5, RETROK_KP5 },
   { SDLK_KP_6, RETROK_KP6 },
   { SDLK_KP_7, RETROK_KP7 },
   { SDLK_KP_8, RETROK_KP8 },
   { SDLK_KP_9, RETROK_KP9 },
#else
   { SDLK_KP0, RETROK_KP0 },
   { SDLK_KP1, RETROK_KP1 },
   { SDLK_KP2, RETROK_KP2 },
   { SDLK_KP3, RETROK_KP3 },
   { SDLK_KP4, RETROK_KP4 },
   { SDLK_KP5, RETROK_KP5 },
   { SDLK_KP6, RETROK_KP6 },
   { SDLK_KP7, RETROK_KP7 },
   { SDLK_KP8, RETROK_KP8 },
   { SDLK_KP9, RETROK_KP9 },
#endif
   { SDLK_KP_PERIOD, RETROK_KP_PERIOD },
   { SDLK_KP_DIVIDE, RETROK_KP_DIVIDE },
   { SDLK_KP_MULTIPLY, RETROK_KP_MULTIPLY },
   { SDLK_KP_MINUS, RETROK_KP_MINUS },
   { SDLK_KP_PLUS, RETROK_KP_PLUS },
   { SDLK_KP_ENTER, RETROK_KP_ENTER },
   { SDLK_KP_EQUALS, RETROK_KP_EQUALS },
   { SDLK_UP, RETROK_UP },
   { SDLK_DOWN, RETROK_DOWN },
   { SDLK_RIGHT, RETROK_RIGHT },
   { SDLK_LEFT, RETROK_LEFT },
   { SDLK_INSERT, RETROK_INSERT },
   { SDLK_HOME, RETROK_HOME },
   { SDLK_END, RETROK_END },
   { SDLK_PAGEUP, RETROK_PAGEUP },
   { SDLK_PAGEDOWN, RETROK_PAGEDOWN },
   { SDLK_F1, RETROK_F1 },
   { SDLK_F2, RETROK_F2 },
   { SDLK_F3, RETROK_F3 },
   { SDLK_F4, RETROK_F4 },
   { SDLK_F5, RETROK_F5 },
   { SDLK_F6, RETROK_F6 },
   { SDLK_F7, RETROK_F7 },
   { SDLK_F8, RETROK_F8 },
   { SDLK_F9, RETROK_F9 },
   { SDLK_F10, RETROK_F10 },
   { SDLK_F11, RETROK_F11 },
   { SDLK_F12, RETROK_F12 },
   { SDLK_F13, RETROK_F13 },
   { SDLK_F14, RETROK_F14 },
   { SDLK_F15, RETROK_F15 },
#ifdef HAVE_SDL2
   { SDLK_NUMLOCKCLEAR, RETROK_NUMLOCK },
#else
   { SDLK_NUMLOCK, RETROK_NUMLOCK },
#endif
   { SDLK_CAPSLOCK, RETROK_CAPSLOCK },
#ifdef HAVE_SDL2
   { SDLK_SCROLLLOCK, RETROK_SCROLLOCK },
#else
   { SDLK_SCROLLOCK, RETROK_SCROLLOCK },
#endif
   { SDLK_RSHIFT, RETROK_RSHIFT },
   { SDLK_LSHIFT, RETROK_LSHIFT },
   { SDLK_RCTRL, RETROK_RCTRL },
   { SDLK_LCTRL, RETROK_LCTRL },
   { SDLK_RALT, RETROK_RALT },
   { SDLK_LALT, RETROK_LALT },
#ifdef HAVE_SDL2
   /* { ?, RETROK_RMETA }, */
   /* { ?, RETROK_LMETA }, */
   { SDLK_LGUI, RETROK_LSUPER },
   { SDLK_RGUI, RETROK_RSUPER },
#else
   { SDLK_RMETA, RETROK_RMETA },
   { SDLK_LMETA, RETROK_LMETA },
   { SDLK_LSUPER, RETROK_LSUPER },
   { SDLK_RSUPER, RETROK_RSUPER },
#endif
   { SDLK_MODE, RETROK_MODE },
#ifndef HAVE_SDL2
   { SDLK_COMPOSE, RETROK_COMPOSE },
#endif
   { SDLK_HELP, RETROK_HELP },
#ifdef HAVE_SDL2
   { SDLK_PRINTSCREEN, RETROK_PRINT },
#else
   { SDLK_PRINT, RETROK_PRINT },
#endif
   { SDLK_SYSREQ, RETROK_SYSREQ },
   { SDLK_PAUSE, RETROK_BREAK },
   { SDLK_MENU, RETROK_MENU },
   { SDLK_POWER, RETROK_POWER },

#ifndef HAVE_SDL2
   { SDLK_EURO, RETROK_EURO },
#endif
   { SDLK_UNDO, RETROK_UNDO },

   { 0, RETROK_UNKNOWN },
};

static void *sdl_input_init(void)
{
   settings_t *settings;
   sdl_input_t *sdl;
   input_keymaps_init_keyboard_lut(rarch_key_map_sdl);
   settings = config_get_ptr();
   sdl = (sdl_input_t*)calloc(1, sizeof(*sdl));
   if (!sdl)
      return NULL;

   sdl->joypad = input_joypad_init_driver(settings->input.joypad_driver, sdl);

   RARCH_LOG("[SDL]: Input driver initialized.\n");
   return sdl;
}

static bool sdl_key_pressed(int key)
{
   int num_keys;
   const uint8_t *keymap;
   unsigned sym;

   if (key >= RETROK_LAST)
      return false;

   sym = input_keymaps_translate_rk_to_keysym((enum retro_key)key);

#ifdef HAVE_SDL2
   sym = SDL_GetScancodeFromKey(sym);
   keymap = SDL_GetKeyboardState(&num_keys);
#else
   keymap = SDL_GetKeyState(&num_keys);
#endif
   if (sym >= (unsigned)num_keys)
      return false;

   return keymap[sym];
}

static bool sdl_is_pressed(sdl_input_t *sdl, unsigned port_num, const struct retro_keybind *binds, unsigned key)
{
   if (sdl_key_pressed(binds[key].key))
      return true;

   return input_joypad_pressed(sdl->joypad, port_num, binds, key);
}

static int16_t sdl_analog_pressed(sdl_input_t *sdl, const struct retro_keybind *binds,
      unsigned idx, unsigned id)
{
   int16_t pressed_minus = 0, pressed_plus = 0;
   unsigned id_minus = 0;
   unsigned id_plus  = 0;

   input_conv_analog_id_to_bind_id(idx, id, &id_minus, &id_plus);

   if (sdl_key_pressed(binds[id_minus].key))
      pressed_minus = -0x7fff;
   if (sdl_key_pressed(binds[id_plus].key))
      pressed_plus  = 0x7fff;

   return pressed_plus + pressed_minus;
}

static bool sdl_input_key_pressed(void *data, int key)
{
   settings_t *settings = config_get_ptr();
   const struct retro_keybind *binds = settings->input.binds[0];
   if (key >= 0 && key < RARCH_BIND_LIST_END)
      return sdl_is_pressed((sdl_input_t*)data, 0, binds, key);
   return false;
}

static bool sdl_input_meta_key_pressed(void *data, int key)
{
   return false;
}

static int16_t sdl_joypad_device_state(sdl_input_t *sdl, const struct retro_keybind **binds_, 
      unsigned port_num, unsigned id)
{
   const struct retro_keybind *binds = binds_[port_num];
   if (id < RARCH_BIND_LIST_END)
      return binds[id].valid && sdl_is_pressed(sdl, port_num, binds, id);
   return 0;
}

static int16_t sdl_analog_device_state(sdl_input_t *sdl, const struct retro_keybind **binds,
      unsigned port_num, unsigned idx, unsigned id)
{
   int16_t ret = sdl_analog_pressed(sdl, binds[port_num], idx, id);
   if (!ret)
      ret = input_joypad_analog(sdl->joypad, port_num, idx, id, binds[port_num]);
   return ret;
}

static int16_t sdl_keyboard_device_state(sdl_input_t *sdl, unsigned id)
{
   return sdl_key_pressed(id);
}

static int16_t sdl_mouse_device_state(sdl_input_t *sdl, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return sdl->mouse_l;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return sdl->mouse_r;
      case RETRO_DEVICE_ID_MOUSE_WHEELUP:
         return sdl->mouse_wu;
      case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
         return sdl->mouse_wd;
      case RETRO_DEVICE_ID_MOUSE_X:
         return sdl->mouse_x;
      case RETRO_DEVICE_ID_MOUSE_Y:
         return sdl->mouse_y;
      case RETRO_DEVICE_ID_MOUSE_MIDDLE:
         return sdl->mouse_m;
   }

   return 0;
}

static int16_t sdl_pointer_device_state(sdl_input_t *sdl,
      unsigned idx, unsigned id, bool screen)
{
   bool valid, inside;
   int16_t res_x = 0, res_y = 0, res_screen_x = 0, res_screen_y = 0;

   if (idx != 0)
      return 0;

   valid = input_translate_coord_viewport(sdl->mouse_abs_x, sdl->mouse_abs_y,
         &res_x, &res_y, &res_screen_x, &res_screen_y);

   if (!valid)
      return 0;

   if (screen)
   {
      res_x = res_screen_x;
      res_y = res_screen_y;
   }

   inside = (res_x >= -0x7fff) && (res_y >= -0x7fff);

   if (!inside)
      return 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_POINTER_X:
         return res_x;
      case RETRO_DEVICE_ID_POINTER_Y:
         return res_y;
      case RETRO_DEVICE_ID_POINTER_PRESSED:
         return sdl->mouse_l;
   }

   return 0;
}

static int16_t sdl_lightgun_device_state(sdl_input_t *sdl, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_LIGHTGUN_X:
         return sdl->mouse_x;
      case RETRO_DEVICE_ID_LIGHTGUN_Y:
         return sdl->mouse_y;
      case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
         return sdl->mouse_l;
      case RETRO_DEVICE_ID_LIGHTGUN_CURSOR:
         return sdl->mouse_m;
      case RETRO_DEVICE_ID_LIGHTGUN_TURBO:
         return sdl->mouse_r;
      case RETRO_DEVICE_ID_LIGHTGUN_START:
         return sdl->mouse_m && sdl->mouse_r; 
      case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
         return sdl->mouse_m && sdl->mouse_l; 
   }

   return 0;
}

static int16_t sdl_input_state(void *data_, const struct retro_keybind **binds,
      unsigned port, unsigned device, unsigned idx, unsigned id)
{
   sdl_input_t *data = (sdl_input_t*)data_;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return sdl_joypad_device_state(data, binds, port, id);
      case RETRO_DEVICE_ANALOG:
         return sdl_analog_device_state(data, binds, port, idx, id);
      case RETRO_DEVICE_MOUSE:
         return sdl_mouse_device_state(data, id);
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         return sdl_pointer_device_state(data, idx, id, device == RARCH_DEVICE_POINTER_SCREEN);
      case RETRO_DEVICE_KEYBOARD:
         return sdl_keyboard_device_state(data, id);
      case RETRO_DEVICE_LIGHTGUN:
         return sdl_lightgun_device_state(data, id);
   }

   return 0;
}

static void sdl_input_free(void *data)
{
   sdl_input_t *sdl = (sdl_input_t*)data;

   if (!data)
      return;

   /* Flush out all pending events. */
#ifdef HAVE_SDL2
   SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
#else
   SDL_Event event;
   while (SDL_PollEvent(&event));
#endif

   if (sdl->joypad)
      sdl->joypad->destroy();

   free(data);
}

static void sdl_grab_mouse(void *data, bool state)
{
#ifdef HAVE_SDL2
   struct temp{
      SDL_Window *w;
   };
   sdl_input_t *sdl = (sdl_input_t*)data;
   driver_t *driver = driver_get_ptr();

   if (driver->video != &video_sdl2)
      return;

   /* First member of sdl2_video_t is the window */
   SDL_SetWindowGrab(((struct temp*)driver->video_data)->w,
         state ? SDL_TRUE : SDL_FALSE);
#endif
}

static bool sdl_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   sdl_input_t *sdl = (sdl_input_t*)data;
   if (!sdl)
      return false;
   return input_joypad_set_rumble(sdl->joypad, port, effect, strength);
}

static const input_device_driver_t *sdl_get_joypad_driver(void *data)
{
   sdl_input_t *sdl = (sdl_input_t*)data;
   if (!sdl)
      return NULL;
   return sdl->joypad;
}

static void sdl_poll_mouse(sdl_input_t *sdl)
{
   Uint8 btn = SDL_GetRelativeMouseState(&sdl->mouse_x, &sdl->mouse_y);

   SDL_GetMouseState(&sdl->mouse_abs_x, &sdl->mouse_abs_y);

   sdl->mouse_l  = SDL_BUTTON(SDL_BUTTON_LEFT)      & btn ? 1 : 0;
   sdl->mouse_r  = SDL_BUTTON(SDL_BUTTON_RIGHT)     & btn ? 1 : 0;
   sdl->mouse_m  = SDL_BUTTON(SDL_BUTTON_MIDDLE)    & btn ? 1 : 0;
#ifndef HAVE_SDL2
   sdl->mouse_wu = SDL_BUTTON(SDL_BUTTON_WHEELUP)   & btn ? 1 : 0;
   sdl->mouse_wd = SDL_BUTTON(SDL_BUTTON_WHEELDOWN) & btn ? 1 : 0;
#endif
}

static void sdl_input_poll(void *data)
{
   sdl_input_t *sdl = (sdl_input_t*)data;
   SDL_Event event;

   SDL_PumpEvents();

   if (sdl->joypad)
      sdl->joypad->poll();
   sdl_poll_mouse(sdl);

#ifdef HAVE_SDL2
   while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_KEYDOWN, SDL_MOUSEWHEEL) > 0)
#else
   while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_KEYEVENTMASK) > 0)
#endif
   {
      if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
      {
         uint16_t mod = 0;
         unsigned code = input_keymaps_translate_keysym_to_rk(event.key.keysym.sym);

         if (event.key.keysym.mod & KMOD_SHIFT)
            mod |= RETROKMOD_SHIFT;

         if (event.key.keysym.mod & KMOD_CTRL)
            mod |= RETROKMOD_CTRL;

         if (event.key.keysym.mod & KMOD_ALT)
            mod |= RETROKMOD_ALT;

         if (event.key.keysym.mod & KMOD_NUM)
            mod |= RETROKMOD_NUMLOCK;

         if (event.key.keysym.mod & KMOD_CAPS)
            mod |= RETROKMOD_CAPSLOCK;

         input_keyboard_event(event.type == SDL_KEYDOWN, code, code, mod,
               RETRO_DEVICE_KEYBOARD);
      }
#ifdef HAVE_SDL2
      else if (event.type == SDL_MOUSEWHEEL)
      {
         sdl->mouse_wu = event.wheel.y < 0;
         sdl->mouse_wd = event.wheel.y > 0;
         sdl->mouse_wl = event.wheel.x < 0;
         sdl->mouse_wr = event.wheel.x > 0;
         break;
      }
#endif
   }
}

static uint64_t sdl_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);
   caps |= (1 << RETRO_DEVICE_MOUSE);
   caps |= (1 << RETRO_DEVICE_KEYBOARD);
   caps |= (1 << RETRO_DEVICE_LIGHTGUN);
   caps |= (1 << RETRO_DEVICE_POINTER);
   caps |= (1 << RETRO_DEVICE_ANALOG);

   return caps;
}

static bool sdl_keyboard_mapping_is_blocked(void *data)
{
   sdl_input_t *sdl = (sdl_input_t*)data;
   if (!sdl)
      return false;
   return sdl->blocked;
}

static void sdl_keyboard_mapping_set_block(void *data, bool value)
{
   sdl_input_t *sdl = (sdl_input_t*)data;
   if (!sdl)
      return;
   sdl->blocked = value;
}

input_driver_t input_sdl = {
   sdl_input_init,
   sdl_input_poll,
   sdl_input_state,
   sdl_input_key_pressed,
   sdl_input_meta_key_pressed,
   sdl_input_free,
   NULL,
   NULL,
   sdl_get_capabilities,
#ifdef HAVE_SDL2
   "sdl2",
#else
   "sdl",
#endif
   sdl_grab_mouse,
   NULL,
   sdl_set_rumble,
   sdl_get_joypad_driver,
   sdl_keyboard_mapping_is_blocked,
   sdl_keyboard_mapping_set_block,
};
