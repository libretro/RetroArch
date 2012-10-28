/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#include "../driver.h"

#include "SDL.h"
#include "../gfx/gfx_context.h"
#include "../boolean.h"
#include "../general.h"
#include <stdint.h>
#include <stdlib.h>
#include "../libretro.h"
#include "input_common.h"

typedef struct sdl_input
{
   const rarch_joypad_driver_t *joypad;

   int mouse_x, mouse_y;
   int mouse_abs_x, mouse_abs_y;
   int mouse_l, mouse_r, mouse_m;
} sdl_input_t;

struct key_bind
{
   unsigned sdl;
   enum retro_key sk;
};

static unsigned keysym_lut[RETROK_LAST];
static const struct key_bind lut_binds[] = {
   { SDLK_LEFT, RETROK_LEFT },
   { SDLK_RIGHT, RETROK_RIGHT },
   { SDLK_UP, RETROK_UP },
   { SDLK_DOWN, RETROK_DOWN },
   { SDLK_RETURN, RETROK_RETURN },
   { SDLK_TAB, RETROK_TAB },
   { SDLK_INSERT, RETROK_INSERT },
   { SDLK_DELETE, RETROK_DELETE },
   { SDLK_RSHIFT, RETROK_RSHIFT },
   { SDLK_LSHIFT, RETROK_LSHIFT },
   { SDLK_LCTRL, RETROK_LCTRL },
   { SDLK_END, RETROK_END },
   { SDLK_HOME, RETROK_HOME },
   { SDLK_PAGEDOWN, RETROK_PAGEDOWN },
   { SDLK_PAGEUP, RETROK_PAGEUP },
   { SDLK_LALT, RETROK_LALT },
   { SDLK_SPACE, RETROK_SPACE },
   { SDLK_ESCAPE, RETROK_ESCAPE },
   { SDLK_BACKSPACE, RETROK_BACKSPACE },
   { SDLK_KP_ENTER, RETROK_KP_ENTER },
   { SDLK_KP_PLUS, RETROK_KP_PLUS },
   { SDLK_KP_MINUS, RETROK_KP_MINUS },
   { SDLK_KP_MULTIPLY, RETROK_KP_MULTIPLY },
   { SDLK_KP_DIVIDE, RETROK_KP_DIVIDE },
   { SDLK_BACKQUOTE, RETROK_BACKQUOTE },
   { SDLK_PAUSE, RETROK_PAUSE },
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
};

static void init_lut(void)
{
   memset(keysym_lut, 0, sizeof(keysym_lut));
   for (unsigned i = 0; i < sizeof(lut_binds) / sizeof(lut_binds[0]); i++)
      keysym_lut[lut_binds[i].sk] = lut_binds[i].sdl;
}

static void *sdl_input_init(void)
{
   init_lut();
   sdl_input_t *sdl = (sdl_input_t*)calloc(1, sizeof(*sdl));
   if (!sdl)
      return NULL;

   sdl->joypad = input_joypad_init_first();
   return sdl;
}

static bool sdl_key_pressed(int key)
{
   if (key >= RETROK_LAST)
      return false;

   int num_keys;
   Uint8 *keymap = SDL_GetKeyState(&num_keys);
   if (key >= num_keys)
      return false;

   return keymap[key];
}

static bool sdl_is_pressed(sdl_input_t *sdl, unsigned port_num, const struct retro_keybind *key)
{
   if (sdl_key_pressed(key->key))
      return true;

   return input_joypad_pressed(sdl->joypad, port_num, key);
}

static bool sdl_bind_button_pressed(void *data, int key)
{
   const struct retro_keybind *binds = g_settings.input.binds[0];
   if (key >= 0 && key < RARCH_BIND_LIST_END)
   {
      const struct retro_keybind *bind = &binds[key];
      return sdl_is_pressed((sdl_input_t*)data, 0, bind);
   }
   else
      return false;
}

static int16_t sdl_joypad_device_state(sdl_input_t *sdl, const struct retro_keybind **binds_, 
      unsigned port_num, unsigned id)
{
   const struct retro_keybind *binds = binds_[port_num];
   if (id < RARCH_BIND_LIST_END)
   {
      const struct retro_keybind *bind = &binds[id];
      return bind->valid && sdl_is_pressed(sdl, port_num, bind);
   }
   else
      return 0;
}

static int16_t sdl_analog_device_state(sdl_input_t *sdl, const struct retro_keybind **binds,
      unsigned port_num, unsigned index, unsigned id)
{
   return input_joypad_analog(sdl->joypad, port_num, index, id, binds[port_num]);
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
      case RETRO_DEVICE_ID_MOUSE_X:
         return sdl->mouse_x;
      case RETRO_DEVICE_ID_MOUSE_Y:
         return sdl->mouse_y;
      default:
         return 0;
   }
}

static int16_t sdl_pointer_device_state(sdl_input_t *sdl, unsigned id)
{
   int16_t res_x = 0, res_y = 0;
   bool valid = input_translate_coord_viewport(sdl->mouse_abs_x, sdl->mouse_abs_y, &res_x, &res_y);

   if (!valid)
      return 0;

   bool inside = (res_x >= -0x7fff) && (res_x <= 0x7fff) &&
      (res_y >= -0x7fff) && (res_y <= 0x7fff);

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
      default:
         return 0;
   }
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
      default:
         return 0;
   }
}

static int16_t sdl_input_state(void *data_, const struct retro_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   sdl_input_t *data = (sdl_input_t*)data_;
   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return sdl_joypad_device_state(data, binds, port, id);
      case RETRO_DEVICE_ANALOG:
         return sdl_analog_device_state(data, binds, port, index, id);
      case RETRO_DEVICE_MOUSE:
         return sdl_mouse_device_state(data, id);
      case RETRO_DEVICE_POINTER:
         return sdl_pointer_device_state(data, id);
      case RETRO_DEVICE_KEYBOARD:
         return sdl_keyboard_device_state(data, id);
      case RETRO_DEVICE_LIGHTGUN:
         return sdl_lightgun_device_state(data, id);

      default:
         return 0;
   }
}

static void sdl_input_free(void *data)
{
   if (!data)
      return;

   // Flush out all pending events.
   SDL_Event event;
   while (SDL_PollEvent(&event));

   sdl_input_t *sdl = (sdl_input_t*)data;

   if (sdl->joypad)
      sdl->joypad->destroy();

   free(data);
}

static void sdl_poll_mouse(sdl_input_t *sdl)
{
   Uint8 btn = SDL_GetRelativeMouseState(&sdl->mouse_x, &sdl->mouse_y);
   SDL_GetMouseState(&sdl->mouse_abs_x, &sdl->mouse_abs_y);
   sdl->mouse_l = SDL_BUTTON(SDL_BUTTON_LEFT) & btn ? 1 : 0;
   sdl->mouse_r = SDL_BUTTON(SDL_BUTTON_RIGHT) & btn ? 1 : 0;
   sdl->mouse_m = SDL_BUTTON(SDL_BUTTON_MIDDLE) & btn ? 1 : 0;
}

static void sdl_input_poll(void *data)
{
   SDL_PumpEvents();
   sdl_input_t *sdl = (sdl_input_t*)data;

   input_joypad_poll(sdl->joypad);
   sdl_poll_mouse(sdl);
}

const input_driver_t input_sdl = {
   sdl_input_init,
   sdl_input_poll,
   sdl_input_state,
   sdl_bind_button_pressed,
   sdl_input_free,
   "sdl",
};

