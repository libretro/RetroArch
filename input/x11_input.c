/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "driver.h"

#include "SDL.h"
#include "../boolean.h"
#include "general.h"
#include <stdint.h>
#include <stdlib.h>
#include "ssnes_sdl_input.h"
#include "keysym.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

typedef struct x11_input
{
   sdl_input_t *sdl;
   Display *display;
   char state[32];
} x11_input_t;

struct key_bind
{
   unsigned x;
   enum ssnes_key sk;
};

static unsigned keysym_lut[SK_LAST];
static const struct key_bind lut_binds[] = {
   { XK_Left, SK_LEFT },
   { XK_Right, SK_RIGHT },
   { XK_Up, SK_UP },
   { XK_Down, SK_DOWN },
   { XK_Return, SK_RETURN },
   { XK_Tab, SK_TAB },
   { XK_Insert, SK_INSERT },
   { XK_Delete, SK_DELETE },
   { XK_Shift_R, SK_RSHIFT },
   { XK_Shift_L, SK_LSHIFT },
   { XK_Control_L, SK_LCTRL },
   { XK_Alt_L, SK_LALT },
   { XK_space, SK_SPACE },
   { XK_Escape, SK_ESCAPE },
   { XK_BackSpace, SK_BACKSPACE },
   { XK_KP_Enter, SK_KP_ENTER },
   { XK_KP_Add, SK_KP_PLUS },
   { XK_KP_Subtract, SK_KP_MINUS },
   { XK_KP_Multiply, SK_KP_MULTIPLY },
   { XK_KP_Divide, SK_KP_DIVIDE },
   { XK_grave, SK_BACKQUOTE },
   { XK_Pause, SK_PAUSE },
   { XK_KP_0, SK_KP0 },
   { XK_KP_1, SK_KP1 },
   { XK_KP_2, SK_KP2 },
   { XK_KP_3, SK_KP3 },
   { XK_KP_4, SK_KP4 },
   { XK_KP_5, SK_KP5 },
   { XK_KP_6, SK_KP6 },
   { XK_KP_7, SK_KP7 },
   { XK_KP_8, SK_KP8 },
   { XK_KP_9, SK_KP9 },
   { XK_0, SK_0 },
   { XK_1, SK_1 },
   { XK_2, SK_2 },
   { XK_3, SK_3 },
   { XK_4, SK_4 },
   { XK_5, SK_5 },
   { XK_6, SK_6 },
   { XK_7, SK_7 },
   { XK_8, SK_8 },
   { XK_9, SK_9 },
   { XK_F1, SK_F1 },
   { XK_F2, SK_F2 },
   { XK_F3, SK_F3 },
   { XK_F4, SK_F4 },
   { XK_F5, SK_F5 },
   { XK_F6, SK_F6 },
   { XK_F7, SK_F7 },
   { XK_F8, SK_F8 },
   { XK_F9, SK_F9 },
   { XK_F10, SK_F10 },
   { XK_F11, SK_F11 },
   { XK_F12, SK_F12 },
   { XK_a, SK_a },
   { XK_b, SK_b },
   { XK_c, SK_c },
   { XK_d, SK_d },
   { XK_e, SK_e },
   { XK_f, SK_f },
   { XK_g, SK_g },
   { XK_h, SK_h },
   { XK_i, SK_i },
   { XK_j, SK_j },
   { XK_k, SK_k },
   { XK_l, SK_l },
   { XK_m, SK_m },
   { XK_n, SK_n },
   { XK_o, SK_o },
   { XK_p, SK_p },
   { XK_q, SK_q },
   { XK_r, SK_r },
   { XK_s, SK_s },
   { XK_t, SK_t },
   { XK_u, SK_u },
   { XK_v, SK_v },
   { XK_w, SK_w },
   { XK_x, SK_x },
   { XK_y, SK_y },
   { XK_z, SK_z },
};

static void init_lut(void)
{
   memset(keysym_lut, 0, sizeof(keysym_lut));
   for (unsigned i = 0; i < sizeof(lut_binds) / sizeof(lut_binds[0]); i++)
      keysym_lut[lut_binds[i].sk] = lut_binds[i].x;
}

static void *x_input_init(void)
{
   x11_input_t *x11 = (x11_input_t*)calloc(1, sizeof(*x11));
   if (!x11)
      return NULL;

   x11->display = XOpenDisplay(NULL);
   if (!x11->display)
   {
      free(x11);
      return NULL;
   }

   x11->sdl = (sdl_input_t*)input_sdl.init();
   if (!x11->sdl)
   {
      free(x11);
      return NULL;
   }

   init_lut();

   x11->sdl->use_keyboard = false;
   return x11;
}

static bool x_key_pressed(x11_input_t *x11, int key)
{
   key = keysym_lut[key];
   int keycode = XKeysymToKeycode(x11->display, key);
   bool ret = x11->state[keycode >> 3] & (1 << (keycode & 7));
   return ret;
}

static bool x_is_pressed(x11_input_t *x11, const struct snes_keybind *binds, unsigned id)
{
   for (int i = 0; binds[i].id != -1; i++)
   {
      if (binds[i].id == (int)id)
         return x_key_pressed(x11, binds[i].key);
   }

   return false;
}

static bool x_bind_button_pressed(void *data, int key)
{
   x11_input_t *x11 = (x11_input_t*)data;
   bool pressed = x_is_pressed(x11, g_settings.input.binds[0], key);
   if (!pressed)
      return input_sdl.key_pressed(x11->sdl, key);
   return pressed;
}

static int16_t x_input_state(void *data, const struct snes_keybind **binds, bool port, unsigned device, unsigned index, unsigned id)
{
   x11_input_t *x11 = (x11_input_t*)data;
   bool pressed = false;

   switch (device)
   {
      case SNES_DEVICE_JOYPAD:
         pressed = x_is_pressed(x11, binds[(port == SNES_PORT_1) ? 0 : 1], id);
         if (!pressed)
            pressed = input_sdl.input_state(x11->sdl, binds, port, device, index, id);
         return pressed;

      case SNES_DEVICE_MULTITAP:
         pressed = x_is_pressed(x11, binds[(port == SNES_PORT_2) ? 1 + index : 0], id);
         if (!pressed)
            pressed = input_sdl.input_state(x11->sdl, binds, port, device, index, id);
         return pressed;

      default:
         return 0;
   }
}

static void x_input_free(void *data)
{
   x11_input_t *x11 = (x11_input_t*)data;
   input_sdl.free(x11->sdl);
   XCloseDisplay(x11->display);
   free(data);
}

static void x_input_poll(void *data)
{
   x11_input_t *x11 = (x11_input_t*)data;
   XQueryKeymap(x11->display, x11->state);
   input_sdl.poll(x11->sdl);
}

const input_driver_t input_x = {
   x_input_init,
   x_input_poll,
   x_input_state,
   x_bind_button_pressed,
   x_input_free,
   "x"
};

