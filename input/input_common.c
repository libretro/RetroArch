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
#include "../driver.h"
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

#include "../file.h"

static const rarch_joypad_driver_t *joypad_drivers[] = {
#ifndef IS_RETROLAUNCH
#ifdef HAVE_WINXINPUT
   &winxinput_joypad,
#endif
#ifdef HAVE_DINPUT
   &dinput_joypad,
#endif
#if defined(__linux) && !defined(ANDROID)
   &linuxraw_joypad,
#endif
#ifdef HAVE_SDL
   &sdl_joypad,
#endif
#endif
   NULL,
};

const rarch_joypad_driver_t *input_joypad_init_driver(const char *ident)
{
   if (!ident || !*ident)
      return input_joypad_init_first();

   for (unsigned i = 0; joypad_drivers[i]; i++)
   {
      if (strcmp(ident, joypad_drivers[i]->ident) == 0 && joypad_drivers[i]->init())
      {
         RARCH_LOG("Found joypad driver: \"%s\".\n", joypad_drivers[i]->ident);
         return joypad_drivers[i];
      }
   }

   return NULL;
}

const rarch_joypad_driver_t *input_joypad_init_first(void)
{
   for (unsigned i = 0; joypad_drivers[i]; i++)
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

const char *input_joypad_name(const rarch_joypad_driver_t *driver, unsigned joypad)
{
   if (!driver)
      return NULL;

   return driver->name(joypad);
}

bool input_joypad_pressed(const rarch_joypad_driver_t *driver,
      unsigned port, const struct retro_keybind *binds, unsigned key)
{
   if (!driver)
      return false;

   int joy_index = g_settings.input.joypad_map[port];
   if (joy_index < 0 || joy_index >= MAX_PLAYERS)
      return false;

   // Auto-binds are per joypad, not per player.
   const struct retro_keybind *auto_binds = g_settings.input.autoconf_binds[joy_index];

   if (!binds[key].valid)
      return false;

   uint64_t joykey = binds[key].joykey;
   if (joykey == NO_BTN)
      joykey = auto_binds[key].joykey;

   if (driver->button(joy_index, (uint16_t)joykey))
      return true;

   uint32_t joyaxis = binds[key].joyaxis;
   if (joyaxis == AXIS_NONE)
      joyaxis = auto_binds[key].joyaxis;

   int16_t axis = driver->axis(joy_index, joyaxis);
   float scaled_axis = (float)abs(axis) / 0x8000;
   return scaled_axis > g_settings.input.axis_threshold;
}

int16_t input_joypad_analog(const rarch_joypad_driver_t *driver,
      unsigned port, unsigned index, unsigned id, const struct retro_keybind *binds)
{
   if (!driver)
      return 0;

   int joy_index = g_settings.input.joypad_map[port];
   if (joy_index < 0 || joy_index >= MAX_PLAYERS)
      return 0;

   // Auto-binds are per joypad, not per player.
   const struct retro_keybind *auto_binds = g_settings.input.autoconf_binds[joy_index];

   unsigned id_minus = 0;
   unsigned id_plus  = 0;
   input_conv_analog_id_to_bind_id(index, id, &id_minus, &id_plus);

   const struct retro_keybind *bind_minus = &binds[id_minus];
   const struct retro_keybind *bind_plus  = &binds[id_plus];
   if (!bind_minus->valid || !bind_plus->valid)
      return 0;

   uint32_t axis_minus = bind_minus->joyaxis;
   uint32_t axis_plus  = bind_plus->joyaxis;
   if (axis_minus == AXIS_NONE)
      axis_minus = auto_binds[id_minus].joyaxis;
   if (axis_plus == AXIS_NONE)
      axis_plus = auto_binds[id_plus].joyaxis;

   int16_t pressed_minus = abs(driver->axis(joy_index, axis_minus));
   int16_t pressed_plus  = abs(driver->axis(joy_index, axis_plus));

   int16_t res = pressed_plus - pressed_minus;

   if (res != 0)
      return res;

   uint64_t key_minus = bind_minus->joykey;
   uint64_t key_plus  = bind_plus->joykey;
   if (key_minus == NO_BTN)
      key_minus = auto_binds[id_minus].joykey;
   if (key_plus == NO_BTN)
      key_plus = auto_binds[id_plus].joykey;

   int16_t digital_left  = driver->button(joy_index, (uint16_t)key_minus) ? -0x7fff : 0;
   int16_t digital_right = driver->button(joy_index, (uint16_t)key_plus)  ?  0x7fff : 0;
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

#ifndef IS_RETROLAUNCH
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
#endif

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

static const char *bind_player_prefix[MAX_PLAYERS] = {
   "input_player1",
   "input_player2",
   "input_player3",
   "input_player4",
   "input_player5",
   "input_player6",
   "input_player7",
   "input_player8",
};

#define DECLARE_BIND(x, bind, desc) { true, 0, #x, desc, bind }
#define DECLARE_META_BIND(level, x, bind, desc) { true, level, #x, desc, bind }

const struct input_bind_map input_config_bind_map[RARCH_BIND_LIST_END_NULL] = {
      DECLARE_BIND(b,         RETRO_DEVICE_ID_JOYPAD_B, "B button (down)"),
      DECLARE_BIND(y,         RETRO_DEVICE_ID_JOYPAD_Y, "Y button (left)"),
      DECLARE_BIND(select,    RETRO_DEVICE_ID_JOYPAD_SELECT, "Select button"),
      DECLARE_BIND(start,     RETRO_DEVICE_ID_JOYPAD_START, "Start button"),
      DECLARE_BIND(up,        RETRO_DEVICE_ID_JOYPAD_UP, "Up D-pad"),
      DECLARE_BIND(down,      RETRO_DEVICE_ID_JOYPAD_DOWN, "Down D-pad"),
      DECLARE_BIND(left,      RETRO_DEVICE_ID_JOYPAD_LEFT, "Left D-pad"),
      DECLARE_BIND(right,     RETRO_DEVICE_ID_JOYPAD_RIGHT, "Right D-pad"),
      DECLARE_BIND(a,         RETRO_DEVICE_ID_JOYPAD_A, "A button (right)"),
      DECLARE_BIND(x,         RETRO_DEVICE_ID_JOYPAD_X, "X button (top)"),
      DECLARE_BIND(l,         RETRO_DEVICE_ID_JOYPAD_L, "L button (left shoulder)"),
      DECLARE_BIND(r,         RETRO_DEVICE_ID_JOYPAD_R, "R button (right shoulder)"),
      DECLARE_BIND(l2,        RETRO_DEVICE_ID_JOYPAD_L2, "L2 button (left shoulder #2)"),
      DECLARE_BIND(r2,        RETRO_DEVICE_ID_JOYPAD_R2, "R2 button (right shoulder #2)"),
      DECLARE_BIND(l3,        RETRO_DEVICE_ID_JOYPAD_L3, "L3 button (left analog button)"),
      DECLARE_BIND(r3,        RETRO_DEVICE_ID_JOYPAD_R3, "R3 button (right analog button)"),
      DECLARE_BIND(turbo,     RARCH_TURBO_ENABLE, "Turbo enable"),
      DECLARE_BIND(l_x_plus,  RARCH_ANALOG_LEFT_X_PLUS, "Left analog X+ (right)"),
      DECLARE_BIND(l_x_minus, RARCH_ANALOG_LEFT_X_MINUS, "Left analog X- (left)"),
      DECLARE_BIND(l_y_plus,  RARCH_ANALOG_LEFT_Y_PLUS, "Left analog Y+ (down)"),
      DECLARE_BIND(l_y_minus, RARCH_ANALOG_LEFT_Y_MINUS, "Left analog Y- (up)"),
      DECLARE_BIND(r_x_plus,  RARCH_ANALOG_RIGHT_X_PLUS, "Right analog X+ (right)"),
      DECLARE_BIND(r_x_minus, RARCH_ANALOG_RIGHT_X_MINUS, "Right analog X- (left)"),
      DECLARE_BIND(r_y_plus,  RARCH_ANALOG_RIGHT_Y_PLUS, "Right analog Y+ (down)"),
      DECLARE_BIND(r_y_minus, RARCH_ANALOG_RIGHT_Y_MINUS, "Right analog Y- (up)"),

      DECLARE_META_BIND(1, toggle_fast_forward,   RARCH_FAST_FORWARD_KEY, "Fast forward toggle"),
      DECLARE_META_BIND(2, hold_fast_forward,     RARCH_FAST_FORWARD_HOLD_KEY, "Fast forward hold"),
      DECLARE_META_BIND(1, load_state,            RARCH_LOAD_STATE_KEY, "Load state"),
      DECLARE_META_BIND(1, save_state,            RARCH_SAVE_STATE_KEY, "Save state"),
      DECLARE_META_BIND(2, toggle_fullscreen,     RARCH_FULLSCREEN_TOGGLE_KEY, "Fullscreen toggle"),
      DECLARE_META_BIND(2, exit_emulator,         RARCH_QUIT_KEY, "Quit RetroArch"),
      DECLARE_META_BIND(2, state_slot_increase,   RARCH_STATE_SLOT_PLUS, "Savestate slot +"),
      DECLARE_META_BIND(2, state_slot_decrease,   RARCH_STATE_SLOT_MINUS, "Savestate slot -"),
      DECLARE_META_BIND(1, rewind,                RARCH_REWIND, "Rewind"),
      DECLARE_META_BIND(2, movie_record_toggle,   RARCH_MOVIE_RECORD_TOGGLE, "Movie record toggle"),
      DECLARE_META_BIND(2, pause_toggle,          RARCH_PAUSE_TOGGLE, "Pause toggle"),
      DECLARE_META_BIND(2, frame_advance,         RARCH_FRAMEADVANCE, "Frameadvance"),
      DECLARE_META_BIND(2, reset,                 RARCH_RESET, "Reset game"),
      DECLARE_META_BIND(2, shader_next,           RARCH_SHADER_NEXT, "Next shader"),
      DECLARE_META_BIND(2, shader_prev,           RARCH_SHADER_PREV, "Previous shader"),
      DECLARE_META_BIND(2, cheat_index_plus,      RARCH_CHEAT_INDEX_PLUS, "Cheat index +"),
      DECLARE_META_BIND(2, cheat_index_minus,     RARCH_CHEAT_INDEX_MINUS, "Cheat index -"),
      DECLARE_META_BIND(2, cheat_toggle,          RARCH_CHEAT_TOGGLE, "Cheat toggle"),
      DECLARE_META_BIND(2, screenshot,            RARCH_SCREENSHOT, "Take screenshot"),
      DECLARE_META_BIND(2, dsp_config,            RARCH_DSP_CONFIG, "DSP config"),
      DECLARE_META_BIND(2, audio_mute,            RARCH_MUTE, "Audio mute toggle"),
      DECLARE_META_BIND(2, netplay_flip_players,  RARCH_NETPLAY_FLIP, "Netplay flip players"),
      DECLARE_META_BIND(2, slowmotion,            RARCH_SLOWMOTION, "Slow motion"),
      DECLARE_META_BIND(2, enable_hotkey,         RARCH_ENABLE_HOTKEY, "Enable hotkeys"),
      DECLARE_META_BIND(2, volume_up,             RARCH_VOLUME_UP, "Volume +"),
      DECLARE_META_BIND(2, volume_down,           RARCH_VOLUME_DOWN, "Volume -"),
      DECLARE_META_BIND(2, overlay_next,          RARCH_OVERLAY_NEXT, "Overlay next"),
      DECLARE_META_BIND(2, disk_eject_toggle,     RARCH_DISK_EJECT_TOGGLE, "Disk eject toggle"),
      DECLARE_META_BIND(2, disk_next,             RARCH_DISK_NEXT, "Disk next"),
      DECLARE_META_BIND(2, grab_mouse_toggle,     RARCH_GRAB_MOUSE_TOGGLE, "Grab mouse toggle"),
#ifdef HAVE_RGUI
      DECLARE_META_BIND(1, menu_toggle,           RARCH_MENU_TOGGLE, "RGUI menu toggle"),
#endif
};

const struct input_key_map input_config_key_map[] = {
   { "left", RETROK_LEFT },
   { "right", RETROK_RIGHT },
   { "up", RETROK_UP },
   { "down", RETROK_DOWN },
   { "enter", RETROK_RETURN },
   { "kp_enter", RETROK_KP_ENTER },
   { "tab", RETROK_TAB },
   { "insert", RETROK_INSERT },
   { "del", RETROK_DELETE },
   { "end", RETROK_END },
   { "home", RETROK_HOME },
   { "rshift", RETROK_RSHIFT },
   { "shift", RETROK_LSHIFT },
   { "ctrl", RETROK_LCTRL },
   { "alt", RETROK_LALT },
   { "space", RETROK_SPACE },
   { "escape", RETROK_ESCAPE },
   { "add", RETROK_KP_PLUS },
   { "subtract", RETROK_KP_MINUS },
   { "kp_plus", RETROK_KP_PLUS },
   { "kp_minus", RETROK_KP_MINUS },
   { "f1", RETROK_F1 },
   { "f2", RETROK_F2 },
   { "f3", RETROK_F3 },
   { "f4", RETROK_F4 },
   { "f5", RETROK_F5 },
   { "f6", RETROK_F6 },
   { "f7", RETROK_F7 },
   { "f8", RETROK_F8 },
   { "f9", RETROK_F9 },
   { "f10", RETROK_F10 },
   { "f11", RETROK_F11 },
   { "f12", RETROK_F12 },
   { "num0", RETROK_0 },
   { "num1", RETROK_1 },
   { "num2", RETROK_2 },
   { "num3", RETROK_3 },
   { "num4", RETROK_4 },
   { "num5", RETROK_5 },
   { "num6", RETROK_6 },
   { "num7", RETROK_7 },
   { "num8", RETROK_8 },
   { "num9", RETROK_9 },
   { "pageup", RETROK_PAGEUP },
   { "pagedown", RETROK_PAGEDOWN },
   { "keypad0", RETROK_KP0 },
   { "keypad1", RETROK_KP1 },
   { "keypad2", RETROK_KP2 },
   { "keypad3", RETROK_KP3 },
   { "keypad4", RETROK_KP4 },
   { "keypad5", RETROK_KP5 },
   { "keypad6", RETROK_KP6 },
   { "keypad7", RETROK_KP7 },
   { "keypad8", RETROK_KP8 },
   { "keypad9", RETROK_KP9 },
   { "period", RETROK_PERIOD },
   { "capslock", RETROK_CAPSLOCK },
   { "numlock", RETROK_NUMLOCK },
   { "backspace", RETROK_BACKSPACE },
   { "multiply", RETROK_KP_MULTIPLY },
   { "divide", RETROK_KP_DIVIDE },
   { "print_screen", RETROK_PRINT },
   { "scroll_lock", RETROK_SCROLLOCK },
   { "tilde", RETROK_BACKQUOTE },
   { "backquote", RETROK_BACKQUOTE },
   { "pause", RETROK_PAUSE },
   { "nul", RETROK_UNKNOWN },
   { NULL, RETROK_UNKNOWN },
};

static enum retro_key find_sk_bind(const char *str)
{
   for (size_t i = 0; input_config_key_map[i].str; i++)
   {
      if (strcasecmp(input_config_key_map[i].str, str) == 0)
         return input_config_key_map[i].key;
   }

   return RETROK_UNKNOWN;
}

static enum retro_key find_sk_key(const char *str)
{
   if (strlen(str) == 1 && isalpha(*str))
      return (enum retro_key)(RETROK_a + (tolower(*str) - (int)'a'));
   else
      return find_sk_bind(str);
}

void input_config_parse_key(config_file_t *conf, const char *prefix, const char *btn,
      struct retro_keybind *bind)
{
   char tmp[64];
   char key[64];
   snprintf(key, sizeof(key), "%s_%s", prefix, btn);

   if (config_get_array(conf, key, tmp, sizeof(tmp)))
      bind->key = find_sk_key(tmp);
}

const char *input_config_get_prefix(unsigned player, bool meta)
{
   if (player == 0)
      return meta ? "input" : bind_player_prefix[player];
   else if (player != 0 && !meta)
      return bind_player_prefix[player];
   else
      return NULL; // Don't bother with meta bind for anyone else than first player.
}

static void parse_hat(struct retro_keybind *bind, const char *str)
{
   if (!isdigit(*str))
      return;

   char *dir = NULL;
   uint16_t hat = strtoul(str, &dir, 0);
   uint16_t hat_dir = 0;

   if (!dir)
   {
      RARCH_WARN("Found invalid hat in config!\n");
      return;
   }

   if (strcasecmp(dir, "up") == 0)
      hat_dir = HAT_UP_MASK;
   else if (strcasecmp(dir, "down") == 0)
      hat_dir = HAT_DOWN_MASK;
   else if (strcasecmp(dir, "left") == 0)
      hat_dir = HAT_LEFT_MASK;
   else if (strcasecmp(dir, "right") == 0)
      hat_dir = HAT_RIGHT_MASK;

   if (hat_dir)
      bind->joykey = HAT_MAP(hat, hat_dir);
}

void input_config_parse_joy_button(config_file_t *conf, const char *prefix,
      const char *btn, struct retro_keybind *bind)
{
   char tmp[64];
   char key[64];
   snprintf(key, sizeof(key), "%s_%s_btn", prefix, btn);

   if (config_get_array(conf, key, tmp, sizeof(tmp)))
   {
      const char *btn = tmp;
      if (strcmp(btn, "nul") == 0)
         bind->joykey = NO_BTN;
      else
      {
         if (*btn == 'h')
            parse_hat(bind, btn + 1);
         else
            bind->joykey = strtoull(tmp, NULL, 0);
      }
   }
}

void input_config_parse_joy_axis(config_file_t *conf, const char *prefix,
      const char *axis, struct retro_keybind *bind)
{
   char tmp[64];
   char key[64];
   snprintf(key, sizeof(key), "%s_%s_axis", prefix, axis);

   if (config_get_array(conf, key, tmp, sizeof(tmp)))
   {
      if (strcmp(tmp, "nul") == 0)
         bind->joyaxis = AXIS_NONE;
      else if (strlen(tmp) >= 2 && (*tmp == '+' || *tmp == '-'))
      {
         int axis = strtol(tmp + 1, NULL, 0);
         if (*tmp == '+')
            bind->joyaxis = AXIS_POS(axis);
         else
            bind->joyaxis = AXIS_NEG(axis);

      }
   }
}

#if !defined(IS_JOYCONFIG) && !defined(IS_RETROLAUNCH)
static void input_autoconfigure_joypad_conf(config_file_t *conf, struct retro_keybind *binds)
{
   for (unsigned i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      input_config_parse_joy_button(conf, "input", input_config_bind_map[i].base, &binds[i]);
      input_config_parse_joy_axis(conf, "input", input_config_bind_map[i].base, &binds[i]);
   }
}

static bool input_try_autoconfigure_joypad_from_conf(config_file_t *conf, unsigned index, const char *name, const char *driver, bool block_osd_spam)
{
   if (!conf)
      return false;
         
   char ident[1024];
   char input_driver[1024];
   
   *ident = *input_driver = '\0';
   
   config_get_array(conf, "input_device", ident, sizeof(ident));
   config_get_array(conf, "input_driver", input_driver, sizeof(input_driver));

   if (!strcmp(ident, name) && !strcmp(driver, input_driver))
   {
      g_settings.input.autoconfigured[index] = true;
      input_autoconfigure_joypad_conf(conf, g_settings.input.autoconf_binds[index]);

      char msg[512];
      snprintf(msg, sizeof(msg), "Joypad port #%u (%s) configured.",
            index, name);

      if (!block_osd_spam)
         msg_queue_push(g_extern.msg_queue, msg, 0, 60);
      RARCH_LOG("%s\n", msg);

      return true;
   }

   return false;
}

void input_config_autoconfigure_joypad(unsigned index, const char *name, const char *driver)
{
   if (!g_settings.input.autodetect_enable)
      return;

   // This will be the case if input driver is reinit. No reason to spam autoconfigure messages
   // every time (fine in log).
   bool block_osd_spam = g_settings.input.autoconfigured[index] && name;

   for (unsigned i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      g_settings.input.autoconf_binds[index][i].joykey = NO_BTN;
      g_settings.input.autoconf_binds[index][i].joyaxis = AXIS_NONE;
   }
   g_settings.input.autoconfigured[index] = false;

   if (!name)
      return;

   // false = load from both cfg files and internal
   bool internal_only = (!*g_settings.input.autoconfig_dir);

   // First internal
   for (size_t i = 0; input_builtin_autoconfs[i] /* array is NULL terminated */; i++)
   {
      config_file_t *conf = config_file_new_from_string(input_builtin_autoconfs[i]);
      bool success = input_try_autoconfigure_joypad_from_conf(conf, index, name, driver, block_osd_spam);
      config_file_free(conf);
      if (success)
         break;
   }
   
   // Now try files
   if (!internal_only)
   {
      struct string_list *list = dir_list_new(g_settings.input.autoconfig_dir, "cfg", false);
      if (!list)
         return;
   
      for (size_t i = 0; i < list->size; i++)
      {
         config_file_t *conf = config_file_new(list->elems[i].data);
         if (!conf)
            continue;
         bool success = input_try_autoconfigure_joypad_from_conf(conf, index, name, driver, block_osd_spam);
         config_file_free(conf);
         if (success)
            break;
      }
      
      string_list_free(list);
   }
}
#else
void input_config_autoconfigure_joypad(unsigned index, const char *name, const char *driver)
{
   (void)index;
   (void)name;
   (void)driver;
}
#endif

