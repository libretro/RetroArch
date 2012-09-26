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

#include "x11_input.h"
#include "input_common.h"

struct key_bind
{
   unsigned x;
   enum retro_key sk;
};

void x_input_set_disp_win(x11_input_t *x11, Display *dpy, Window win)
{
   if (x11->display && !x11->inherit_disp)
   {
      x11->inherit_disp = true;
      XCloseDisplay(x11->display);
      x11->display = dpy;
      x11->win     = win;
   }
}

static unsigned keysym_lut[RETROK_LAST];
static const struct key_bind lut_binds[] = {
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
   if (key >= RETROK_LAST)
      return false;

   key = keysym_lut[key];
   int keycode = XKeysymToKeycode(x11->display, key);
   bool ret = x11->state[keycode >> 3] & (1 << (keycode & 7));
   return ret;
}

static bool x_is_pressed(x11_input_t *x11, const struct retro_keybind *binds, unsigned id)
{
   if (id < RARCH_BIND_LIST_END)
   {
      const struct retro_keybind *bind = &binds[id];
      return bind->valid && x_key_pressed(x11, binds[id].key);
   }
   else
      return false;
}

static bool x_bind_button_pressed(void *data, int key)
{
   x11_input_t *x11 = (x11_input_t*)data;
   return x_is_pressed(x11, g_settings.input.binds[0], key) ||
      input_sdl.key_pressed(x11->sdl, key);
}

static int16_t x_analog_state(x11_input_t *x11, const struct retro_keybind **binds_,
      unsigned port, unsigned index, unsigned id)
{
   const struct retro_keybind *binds = binds_[port];
   if (id >= RARCH_BIND_LIST_END)
      return 0;

   unsigned id_minus = 0;
   unsigned id_plus  = 0;

   input_conv_analog_id_to_bind_id(index, id, &id_minus, &id_plus);

   const struct retro_keybind *bind_minus = &binds[id_minus];
   const struct retro_keybind *bind_plus  = &binds[id_plus];
   if (!bind_minus->valid || !bind_plus->valid)
      return 0;

   int16_t res_minus = x_is_pressed(x11, binds, id_minus) ? -0x7fff : 0;
   int16_t res_plus  = x_is_pressed(x11, binds, id_plus)  ?  0x7fff : 0;

   return res_plus + res_minus;
}

static int16_t x_mouse_state(x11_input_t *x11, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_X:
         return x11->mouse_x - x11->mouse_last_x;
      case RETRO_DEVICE_ID_MOUSE_Y:
         return x11->mouse_y - x11->mouse_last_y;
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return x11->mouse_l;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return x11->mouse_r;
      default:
         return 0;
   }
}

static int16_t x_lightgun_state(x11_input_t *x11, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_LIGHTGUN_X:
         return x11->mouse_x - x11->mouse_last_x;
      case RETRO_DEVICE_ID_LIGHTGUN_Y:
         return x11->mouse_y - x11->mouse_last_y;
      case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
         return x11->mouse_l;
      case RETRO_DEVICE_ID_LIGHTGUN_CURSOR:
         return x11->mouse_m;
      case RETRO_DEVICE_ID_LIGHTGUN_TURBO:
         return x11->mouse_r;
      case RETRO_DEVICE_ID_LIGHTGUN_START:
         return x11->mouse_m && x11->mouse_r; 
      case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
         return x11->mouse_m && x11->mouse_l; 
      default:
         return 0;
   }
}

static int16_t x_input_state(void *data, const struct retro_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   x11_input_t *x11 = (x11_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return x_is_pressed(x11, binds[port], id) ||
            input_sdl.input_state(x11->sdl, binds, port, device, index, id);

      case RETRO_DEVICE_KEYBOARD:
         return x_key_pressed(x11, id);

      case RETRO_DEVICE_ANALOG:
      {
         int16_t ret = input_sdl.input_state(x11->sdl, binds, port, device, index, id);
         if (!ret)
            ret = x_analog_state(x11, binds, port, index, id);
         return ret;
      }

      case RETRO_DEVICE_MOUSE:
         return x_mouse_state(x11, id);

      case RETRO_DEVICE_LIGHTGUN:
         return x_lightgun_state(x11, id);

      default:
         return 0;
   }
}

static void x_input_free(void *data)
{
   x11_input_t *x11 = (x11_input_t*)data;
   input_sdl.free(x11->sdl);

   if (!x11->inherit_disp)
      XCloseDisplay(x11->display);

   free(data);
}

static void x_input_poll_mouse(x11_input_t *x11)
{
   if (!x11->inherit_disp)
      return;

   Window root_win, child_win;
   int root_x, root_y, win_x, win_y;
   unsigned mask;

   x11->mouse_last_x = x11->mouse_x;
   x11->mouse_last_y = x11->mouse_y;

   XQueryPointer(x11->display,
            x11->win,
            &root_win, &child_win,
            &root_x, &root_y,
            &win_x, &win_y,
            &mask);

   x11->mouse_x = root_x;
   x11->mouse_y = root_y;
   x11->mouse_l = mask & Button1Mask; 
   x11->mouse_m = mask & Button2Mask; 
   x11->mouse_r = mask & Button3Mask; 
}

static void x_input_poll(void *data)
{
   x11_input_t *x11 = (x11_input_t*)data;

   Window win;
   int rev;
   XGetInputFocus(x11->display, &win, &rev);

   if (win == x11->win)
      XQueryKeymap(x11->display, x11->state);
   else
      memset(x11->state, 0, sizeof(x11->state));

   x_input_poll_mouse(x11);
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

