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
#include <stdbool.h>
#include "general.h"
#include <stdint.h>
#include <stdlib.h>
#include "ssnes_sdl_input.h"

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
   int x;
   int sdl;
};

static int keysym_lut[SDLK_LAST];
static const struct key_bind lut_binds[] = {
   { XK_Left, SDLK_LEFT },
   { XK_Right, SDLK_RIGHT },
   { XK_Up, SDLK_UP },
   { XK_Down, SDLK_DOWN },
   { XK_Return, SDLK_RETURN },
   { XK_Tab, SDLK_TAB },
   { XK_Insert, SDLK_INSERT },
   { XK_Delete, SDLK_DELETE },
   { XK_Shift_R, SDLK_RSHIFT },
   { XK_Shift_L, SDLK_LSHIFT },
   { XK_Control_L, SDLK_LCTRL },
   { XK_Alt_L, SDLK_LALT },
   { XK_space, SDLK_SPACE },
   { XK_Escape, SDLK_ESCAPE },
   { XK_KP_Add, SDLK_KP_PLUS },
   { XK_KP_Subtract, SDLK_KP_MINUS },
   { XK_BackSpace, SDLK_BACKSPACE },
   { XK_KP_Enter, SDLK_KP_ENTER },
   { XK_KP_Add, SDLK_KP_PLUS },
   { XK_KP_Subtract, SDLK_KP_MINUS },
   { XK_KP_Multiply, SDLK_KP_MULTIPLY },
   { XK_KP_Divide, SDLK_KP_DIVIDE },
   { XK_grave, SDLK_BACKQUOTE },
   { XK_KP_0, SDLK_KP0 },
   { XK_KP_1, SDLK_KP1 },
   { XK_KP_2, SDLK_KP2 },
   { XK_KP_3, SDLK_KP3 },
   { XK_KP_4, SDLK_KP4 },
   { XK_KP_5, SDLK_KP5 },
   { XK_KP_6, SDLK_KP6 },
   { XK_KP_7, SDLK_KP7 },
   { XK_KP_8, SDLK_KP8 },
   { XK_KP_9, SDLK_KP9 },
   { XK_0, SDLK_0 },
   { XK_1, SDLK_1 },
   { XK_2, SDLK_2 },
   { XK_3, SDLK_3 },
   { XK_4, SDLK_4 },
   { XK_5, SDLK_5 },
   { XK_6, SDLK_6 },
   { XK_7, SDLK_7 },
   { XK_8, SDLK_8 },
   { XK_9, SDLK_9 },
   { XK_F1, SDLK_F1 },
   { XK_F2, SDLK_F2 },
   { XK_F3, SDLK_F3 },
   { XK_F4, SDLK_F4 },
   { XK_F5, SDLK_F5 },
   { XK_F6, SDLK_F6 },
   { XK_F7, SDLK_F7 },
   { XK_F8, SDLK_F8 },
   { XK_F9, SDLK_F9 },
   { XK_F10, SDLK_F10 },
   { XK_F11, SDLK_F11 },
   { XK_F12, SDLK_F12 },
   { XK_a, SDLK_a },
   { XK_b, SDLK_b },
   { XK_c, SDLK_c },
   { XK_d, SDLK_d },
   { XK_e, SDLK_e },
   { XK_f, SDLK_f },
   { XK_g, SDLK_g },
   { XK_h, SDLK_h },
   { XK_i, SDLK_i },
   { XK_j, SDLK_j },
   { XK_k, SDLK_k },
   { XK_l, SDLK_l },
   { XK_m, SDLK_m },
   { XK_n, SDLK_n },
   { XK_o, SDLK_o },
   { XK_p, SDLK_p },
   { XK_q, SDLK_q },
   { XK_r, SDLK_r },
   { XK_s, SDLK_s },
   { XK_t, SDLK_t },
   { XK_u, SDLK_u },
   { XK_v, SDLK_v },
   { XK_w, SDLK_w },
   { XK_x, SDLK_x },
   { XK_y, SDLK_y },
   { XK_z, SDLK_z },
};

static void init_lut(void)
{
   memset(keysym_lut, 0, sizeof(keysym_lut));
   for (unsigned i = 0; i < sizeof(lut_binds) / sizeof(lut_binds[0]); i++)
      keysym_lut[lut_binds[i].sdl] = lut_binds[i].x;
}

static void* x_input_init(void)
{
   x11_input_t *x11 = calloc(1, sizeof(*x11));
   if (!x11)
      return NULL;

   x11->display = XOpenDisplay(NULL);
   if (!x11->display)
   {
      free(x11);
      return NULL;
   }

   x11->sdl = input_sdl.init();
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
      if (binds[i].id == id)
         return x_key_pressed(x11, binds[i].key);
   }

   return false;
}

static bool x_bind_button_pressed(void *data, int key)
{
   x11_input_t *x11 = data;
   bool pressed = x_is_pressed(x11, g_settings.input.binds[0], key);
   if (!pressed)
      return input_sdl.key_pressed(x11->sdl, key);
   return pressed;
}

static int16_t x_input_state(void *data, const struct snes_keybind **binds, bool port, unsigned device, unsigned index, unsigned id)
{
   x11_input_t *x11 = data;
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
   x11_input_t *x11 = data;
   input_sdl.free(x11->sdl);
   XCloseDisplay(x11->display);
   free(data);
}

static void x_input_poll(void *data)
{
   x11_input_t *x11 = data;
   XQueryKeymap(x11->display, x11->state);
   input_sdl.poll(x11->sdl);
}

const input_driver_t input_x = {
   .init = x_input_init,
   .poll = x_input_poll,
   .input_state = x_input_state,
   .key_pressed = x_bind_button_pressed,
   .free = x_input_free,
   .ident = "x"
};

