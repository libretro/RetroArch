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

   x11->sdl->use_keyboard = false;
   return x11;
}

static const int sdl2xlut[] = {
   0, 0, 0, 0, 0, 0, 0, 0, // 0
   XK_BackSpace, XK_Tab, 0, 0, 0, XK_Return, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, // 16
   0, 0, 0, XK_Escape, 0, 0, 0, 0,
   XK_space, 0, 0, 0, 0, 0, 0, 0, // 32
   0, 0, 0, 0, 0, 0, 0, 0,
   XK_0, XK_1, XK_2, XK_3, XK_4, XK_5, XK_6, XK_7, // 48
   XK_8, XK_9, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, // 64
   0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, // 80
   0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, // 96
};

static int sdl_to_xkeysym(int key)
{
   if (key < sizeof(sdl2xlut) / sizeof(int))
      return sdl2xlut[key];
   else
      return 0;
}

static bool x_key_pressed(x11_input_t *x11, int key)
{
   key = sdl_to_xkeysym(key);

   int keycode = XKeysymToKeycode(x11->display, key);
   fprintf(stderr, "key: %d -> keycode: %d\n", key, keycode);
   return x11->state[keycode >> 3] & (1 << (keycode & 7));
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
   const struct snes_keybind *binds = g_settings.input.binds[0];
   for (int i = 0; binds[i].id != -1; i++)
   {
      if (binds[i].id == key)
      {
         bool pressed = x_key_pressed(x11, binds[i].key);
         if (!pressed)
            return input_sdl.key_pressed(x11->sdl, key);
      }
   }

   return false;
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
   //for (int i = 0; i < 32; i++)
   //{
   //   fprintf(stderr, "State %d: 0x%x\n", i, (unsigned)x11->state[i]);
   //}
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

