/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "input_common.h"
#include <string.h>
#include <stdlib.h>

#include "../general.h"
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_DINPUT
#undef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#endif

#ifdef HAVE_SDL
#include "SDL.h"
#endif

#ifdef HAVE_X11
#include <X11/keysym.h>
#endif

static const rarch_joypad_driver_t *joypad_drivers[] = {
#ifdef HAVE_DINPUT
   &dinput_joypad,
#endif
#if defined(__linux) && !defined(ANDROID)
   &linuxraw_joypad,
#endif
#ifdef HAVE_SDL
   &sdl_joypad,
#endif
};

const rarch_joypad_driver_t *input_joypad_find_driver(const char *ident)
{
   for (unsigned i = 0; i < ARRAY_SIZE(joypad_drivers); i++)
   {
      if (strcmp(ident, joypad_drivers[i]->ident) == 0)
      {
         RARCH_LOG("Found joypad driver: \"%s\".\n", joypad_drivers[i]->ident);
         return joypad_drivers[i];
      }
   }

   return NULL;
}

const rarch_joypad_driver_t *input_joypad_init_first(void)
{
   for (unsigned i = 0; i < ARRAY_SIZE(joypad_drivers); i++)
   {
      if (joypad_drivers[i]->init())
      {
         RARCH_LOG("Found joypad driver: \"%s\".\n", joypad_drivers[i]->ident);
         return joypad_drivers[i];
      }
   }

   return NULL;
}

void input_joypad_poll(const rarch_joypad_driver_t *driver)
{
   if (driver)
      driver->poll();
}

bool input_joypad_pressed(const rarch_joypad_driver_t *driver,
      unsigned port, const struct retro_keybind *key)
{
   if (!driver)
      return false;

   int joy_index = g_settings.input.joypad_map[port];
   if (joy_index < 0 || joy_index >= MAX_PLAYERS)
      return false;

   if (!key->valid)
      return false;

   if (driver->button(joy_index, (uint16_t)key->joykey))
      return true;

   int16_t axis = driver->axis(joy_index, key->joyaxis);
   float scaled_axis = (float)abs(axis) / 0x8000;
   return scaled_axis > g_settings.input.axis_threshold;
}

int16_t input_joypad_analog(const rarch_joypad_driver_t *driver,
      unsigned port, unsigned index, unsigned id, const struct retro_keybind *binds)
{
   if (!driver)
      return 0;

   int joy_index = g_settings.input.joypad_map[port];
   if (joy_index < 0)
      return 0;

   unsigned id_minus = 0;
   unsigned id_plus  = 0;
   input_conv_analog_id_to_bind_id(index, id, &id_minus, &id_plus);

   const struct retro_keybind *bind_minus = &binds[id_minus];
   const struct retro_keybind *bind_plus  = &binds[id_plus];
   if (!bind_minus->valid || !bind_plus->valid)
      return 0;

   int16_t pressed_minus = abs(driver->axis(joy_index, bind_minus->joyaxis));
   int16_t pressed_plus  = abs(driver->axis(joy_index, bind_plus->joyaxis));

   int16_t res = pressed_plus - pressed_minus;

   if (res != 0)
      return res;

   int16_t digital_left  = driver->button(joy_index, (uint16_t)bind_minus->joykey) ? -0x7fff : 0;
   int16_t digital_right = driver->button(joy_index, (uint16_t)bind_plus->joykey)  ?  0x7fff : 0;
   return digital_right + digital_left;
}

int16_t input_joypad_axis_raw(const rarch_joypad_driver_t *driver,
      unsigned joypad, unsigned axis)
{
   if (!driver)
      return 0;

   return driver->axis(joypad, AXIS_POS(axis)) + driver->axis(joypad, AXIS_NEG(axis));
}

bool input_joypad_button_raw(const rarch_joypad_driver_t *driver,
      unsigned joypad, unsigned button)
{
   if (!driver)
      return false;

   return driver->button(joypad, button);
}

bool input_joypad_hat_raw(const rarch_joypad_driver_t *driver,
      unsigned joypad, unsigned hat_dir, unsigned hat)
{
   if (!driver)
      return false;

   return driver->button(joypad, HAT_MAP(hat, hat_dir));
}

bool input_translate_coord_viewport(int mouse_x, int mouse_y,
      int16_t *res_x, int16_t *res_y, int16_t *res_screen_x, int16_t *res_screen_y)
{
   struct rarch_viewport vp = {0};
   if (driver.video->viewport_info)
      video_viewport_info_func(&vp);
   else
      return false;

   int scaled_screen_x = (2 * mouse_x * 0x7fff) / (int)vp.full_width - 0x7fff;
   int scaled_screen_y = (2 * mouse_y * 0x7fff) / (int)vp.full_height - 0x7fff;
   if (scaled_screen_x < -0x7fff || scaled_screen_x > 0x7fff)
      scaled_screen_x = -0x8000; // OOB
   if (scaled_screen_y < -0x7fff || scaled_screen_y > 0x7fff)
      scaled_screen_y = -0x8000; // OOB

   mouse_x -= vp.x;
   mouse_y -= vp.y;

   int scaled_x = (2 * mouse_x * 0x7fff) / (int)vp.width - 0x7fff;
   int scaled_y = (2 * mouse_y * 0x7fff) / (int)vp.height - 0x7fff;
   if (scaled_x < -0x7fff || scaled_x > 0x7fff)
      scaled_x = -0x8000; // OOB
   if (scaled_y < -0x7fff || scaled_y > 0x7fff)
      scaled_y = -0x8000; // OOB

   *res_x = scaled_x;
   *res_y = scaled_y;
   *res_screen_x = scaled_screen_x;
   *res_screen_y = scaled_screen_y;
   return true;
}

#ifdef HAVE_X11
const struct rarch_key_map rarch_key_map_x11[] = {
   { XK_Left, RETROK_LEFT },
   { XK_Right, RETROK_RIGHT },
   { XK_Up, RETROK_UP },
   { XK_Down, RETROK_DOWN },
   { XK_Return, RETROK_RETURN },
   { XK_Tab, RETROK_TAB },
   { XK_Insert, RETROK_INSERT },
   { XK_Home, RETROK_HOME },
   { XK_End, RETROK_END },
   { XK_Page_Up, RETROK_PAGEUP },
   { XK_Page_Down, RETROK_PAGEDOWN },
   { XK_Delete, RETROK_DELETE },
   { XK_Shift_R, RETROK_RSHIFT },
   { XK_Shift_L, RETROK_LSHIFT },
   { XK_Control_L, RETROK_LCTRL },
   { XK_Alt_L, RETROK_LALT },
   { XK_space, RETROK_SPACE },
   { XK_Escape, RETROK_ESCAPE },
   { XK_BackSpace, RETROK_BACKSPACE },
   { XK_KP_Enter, RETROK_KP_ENTER },
   { XK_KP_Add, RETROK_KP_PLUS },
   { XK_KP_Subtract, RETROK_KP_MINUS },
   { XK_KP_Multiply, RETROK_KP_MULTIPLY },
   { XK_KP_Divide, RETROK_KP_DIVIDE },
   { XK_grave, RETROK_BACKQUOTE },
   { XK_Pause, RETROK_PAUSE },
   { XK_KP_0, RETROK_KP0 },
   { XK_KP_1, RETROK_KP1 },
   { XK_KP_2, RETROK_KP2 },
   { XK_KP_3, RETROK_KP3 },
   { XK_KP_4, RETROK_KP4 },
   { XK_KP_5, RETROK_KP5 },
   { XK_KP_6, RETROK_KP6 },
   { XK_KP_7, RETROK_KP7 },
   { XK_KP_8, RETROK_KP8 },
   { XK_KP_9, RETROK_KP9 },
   { XK_0, RETROK_0 },
   { XK_1, RETROK_1 },
   { XK_2, RETROK_2 },
   { XK_3, RETROK_3 },
   { XK_4, RETROK_4 },
   { XK_5, RETROK_5 },
   { XK_6, RETROK_6 },
   { XK_7, RETROK_7 },
   { XK_8, RETROK_8 },
   { XK_9, RETROK_9 },
   { XK_F1, RETROK_F1 },
   { XK_F2, RETROK_F2 },
   { XK_F3, RETROK_F3 },
   { XK_F4, RETROK_F4 },
   { XK_F5, RETROK_F5 },
   { XK_F6, RETROK_F6 },
   { XK_F7, RETROK_F7 },
   { XK_F8, RETROK_F8 },
   { XK_F9, RETROK_F9 },
   { XK_F10, RETROK_F10 },
   { XK_F11, RETROK_F11 },
   { XK_F12, RETROK_F12 },
   { XK_a, RETROK_a },
   { XK_b, RETROK_b },
   { XK_c, RETROK_c },
   { XK_d, RETROK_d },
   { XK_e, RETROK_e },
   { XK_f, RETROK_f },
   { XK_g, RETROK_g },
   { XK_h, RETROK_h },
   { XK_i, RETROK_i },
   { XK_j, RETROK_j },
   { XK_k, RETROK_k },
   { XK_l, RETROK_l },
   { XK_m, RETROK_m },
   { XK_n, RETROK_n },
   { XK_o, RETROK_o },
   { XK_p, RETROK_p },
   { XK_q, RETROK_q },
   { XK_r, RETROK_r },
   { XK_s, RETROK_s },
   { XK_t, RETROK_t },
   { XK_u, RETROK_u },
   { XK_v, RETROK_v },
   { XK_w, RETROK_w },
   { XK_x, RETROK_x },
   { XK_y, RETROK_y },
   { XK_z, RETROK_z },
   { 0, RETROK_UNKNOWN },
};
#endif

#ifdef HAVE_SDL
const struct rarch_key_map rarch_key_map_sdl[] = {
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
   { 0, RETROK_UNKNOWN },
};
#endif

#ifdef HAVE_DINPUT
const struct rarch_key_map rarch_key_map_dinput[] = {
   { DIK_LEFT, RETROK_LEFT },
   { DIK_RIGHT, RETROK_RIGHT },
   { DIK_UP, RETROK_UP },
   { DIK_DOWN, RETROK_DOWN },
   { DIK_RETURN, RETROK_RETURN },
   { DIK_TAB, RETROK_TAB },
   { DIK_INSERT, RETROK_INSERT },
   { DIK_DELETE, RETROK_DELETE },
   { DIK_RSHIFT, RETROK_RSHIFT },
   { DIK_LSHIFT, RETROK_LSHIFT },
   { DIK_LCONTROL, RETROK_LCTRL },
   { DIK_END, RETROK_END },
   { DIK_HOME, RETROK_HOME },
   { DIK_NEXT, RETROK_PAGEDOWN },
   { DIK_PRIOR, RETROK_PAGEUP },
   { DIK_LALT, RETROK_LALT },
   { DIK_SPACE, RETROK_SPACE },
   { DIK_ESCAPE, RETROK_ESCAPE },
   { DIK_BACKSPACE, RETROK_BACKSPACE },
   { DIK_NUMPADENTER, RETROK_KP_ENTER },
   { DIK_NUMPADPLUS, RETROK_KP_PLUS },
   { DIK_NUMPADMINUS, RETROK_KP_MINUS },
   { DIK_NUMPADSTAR, RETROK_KP_MULTIPLY },
   { DIK_NUMPADSLASH, RETROK_KP_DIVIDE },
   { DIK_GRAVE, RETROK_BACKQUOTE },
   { DIK_PAUSE, RETROK_PAUSE },
   { DIK_NUMPAD0, RETROK_KP0 },
   { DIK_NUMPAD1, RETROK_KP1 },
   { DIK_NUMPAD2, RETROK_KP2 },
   { DIK_NUMPAD3, RETROK_KP3 },
   { DIK_NUMPAD4, RETROK_KP4 },
   { DIK_NUMPAD5, RETROK_KP5 },
   { DIK_NUMPAD6, RETROK_KP6 },
   { DIK_NUMPAD7, RETROK_KP7 },
   { DIK_NUMPAD8, RETROK_KP8 },
   { DIK_NUMPAD9, RETROK_KP9 },
   { DIK_0, RETROK_0 },
   { DIK_1, RETROK_1 },
   { DIK_2, RETROK_2 },
   { DIK_3, RETROK_3 },
   { DIK_4, RETROK_4 },
   { DIK_5, RETROK_5 },
   { DIK_6, RETROK_6 },
   { DIK_7, RETROK_7 },
   { DIK_8, RETROK_8 },
   { DIK_9, RETROK_9 },
   { DIK_F1, RETROK_F1 },
   { DIK_F2, RETROK_F2 },
   { DIK_F3, RETROK_F3 },
   { DIK_F4, RETROK_F4 },
   { DIK_F5, RETROK_F5 },
   { DIK_F6, RETROK_F6 },
   { DIK_F7, RETROK_F7 },
   { DIK_F8, RETROK_F8 },
   { DIK_F9, RETROK_F9 },
   { DIK_F10, RETROK_F10 },
   { DIK_F11, RETROK_F11 },
   { DIK_F12, RETROK_F12 },
   { DIK_A, RETROK_a },
   { DIK_B, RETROK_b },
   { DIK_C, RETROK_c },
   { DIK_D, RETROK_d },
   { DIK_E, RETROK_e },
   { DIK_F, RETROK_f },
   { DIK_G, RETROK_g },
   { DIK_H, RETROK_h },
   { DIK_I, RETROK_i },
   { DIK_J, RETROK_j },
   { DIK_K, RETROK_k },
   { DIK_L, RETROK_l },
   { DIK_M, RETROK_m },
   { DIK_N, RETROK_n },
   { DIK_O, RETROK_o },
   { DIK_P, RETROK_p },
   { DIK_Q, RETROK_q },
   { DIK_R, RETROK_r },
   { DIK_S, RETROK_s },
   { DIK_T, RETROK_t },
   { DIK_U, RETROK_u },
   { DIK_V, RETROK_v },
   { DIK_W, RETROK_w },
   { DIK_X, RETROK_x },
   { DIK_Y, RETROK_y },
   { DIK_Z, RETROK_z },
   { 0, RETROK_UNKNOWN },
};
#endif

static enum retro_key rarch_keysym_lut[RETROK_LAST];

void input_init_keyboard_lut(const struct rarch_key_map *map)
{
   memset(rarch_keysym_lut, 0, sizeof(rarch_keysym_lut));
   for (; map->rk != RETROK_UNKNOWN; map++)
      rarch_keysym_lut[map->rk] = (enum retro_key)map->sym;
}

enum retro_key input_translate_keysym_to_rk(unsigned sym)
{
   for (unsigned i = 0; i < ARRAY_SIZE(rarch_keysym_lut); i++)
   {
      if (rarch_keysym_lut[i] == sym)
         return (enum retro_key)i;
   }

   return RETROK_UNKNOWN;
}

unsigned input_translate_rk_to_keysym(enum retro_key key)
{
   return rarch_keysym_lut[key];
}
