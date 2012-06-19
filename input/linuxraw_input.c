/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2012 - Michael Lelli
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

#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include "../general.h"
#include "linuxraw_input.h"
#include "rarch_sdl_input.h"

static long oldKbmd = 0xFFFF;
static struct termios oldTerm, newTerm;

struct key_bind
{
   uint8_t x;
   enum rarch_key sk;
};

static unsigned keysym_lut[SK_LAST];
static const struct key_bind lut_binds[] = {
   { KEY_ESC, SK_ESCAPE },
   { KEY_1, SK_1 },
   { KEY_2, SK_2 },
   { KEY_3, SK_3},
   { KEY_4, SK_4 },
   { KEY_5, SK_5 },
   { KEY_6, SK_6 },
   { KEY_7, SK_7 },
   { KEY_8, SK_8 },
   { KEY_9, SK_9 },
   { KEY_0, SK_0 },
   { KEY_MINUS, SK_MINUS },
   { KEY_EQUAL, SK_EQUALS },
   { KEY_BACKSPACE, SK_BACKSPACE },
   { KEY_TAB, SK_TAB },
   { KEY_Q, SK_q },
   { KEY_W, SK_w },
   { KEY_E, SK_e },
   { KEY_R, SK_r },
   { KEY_T, SK_t },
   { KEY_Y, SK_y },
   { KEY_U, SK_u },
   { KEY_I, SK_i },
   { KEY_O, SK_o },
   { KEY_P, SK_p },
   { KEY_LEFTBRACE, SK_LEFTBRACKET },
   { KEY_RIGHTBRACE, SK_RIGHTBRACKET },
   { KEY_ENTER, SK_RETURN },
   { KEY_LEFTCTRL, SK_LCTRL },
   { KEY_A, SK_a },
   { KEY_S, SK_s },
   { KEY_D, SK_d },
   { KEY_F, SK_f },
   { KEY_G, SK_g },
   { KEY_H, SK_h },
   { KEY_J, SK_j },
   { KEY_K, SK_k },
   { KEY_L, SK_l },
   { KEY_SEMICOLON, SK_SEMICOLON },
   { KEY_APOSTROPHE, SK_QUOTE },
   { KEY_GRAVE, SK_BACKQUOTE },
   { KEY_LEFTSHIFT, SK_LSHIFT },
   { KEY_BACKSLASH, SK_BACKSLASH },
   { KEY_Z, SK_z },
   { KEY_X, SK_x },
   { KEY_C, SK_c },
   { KEY_V, SK_v },
   { KEY_B, SK_b },
   { KEY_N, SK_n },
   { KEY_M, SK_m },
   { KEY_COMMA, SK_COMMA },
   { KEY_DOT, SK_PERIOD },
   { KEY_SLASH, SK_SLASH },
   { KEY_RIGHTSHIFT, SK_RSHIFT },
   { KEY_KPASTERISK, SK_KP_MULTIPLY },
   { KEY_LEFTALT, SK_LALT },
   { KEY_SPACE, SK_SPACE },
   { KEY_CAPSLOCK, SK_CAPSLOCK },
   { KEY_F1, SK_F1 },
   { KEY_F2, SK_F2 },
   { KEY_F3, SK_F3 },
   { KEY_F4, SK_F4 },
   { KEY_F5, SK_F5 },
   { KEY_F6, SK_F6 },
   { KEY_F7, SK_F7 },
   { KEY_F8, SK_F8 },
   { KEY_F9, SK_F9 },
   { KEY_F10, SK_F10 },
   { KEY_NUMLOCK, SK_NUMLOCK },
   { KEY_SCROLLLOCK, SK_SCROLLOCK },
   { KEY_KP7, SK_KP7 },
   { KEY_KP8, SK_KP8 },
   { KEY_KP9, SK_KP9 },
   { KEY_KPMINUS, SK_KP_MINUS },
   { KEY_KP4, SK_KP4 },
   { KEY_KP5, SK_KP5 },
   { KEY_KP6, SK_KP6 },
   { KEY_KPPLUS, SK_KP_PLUS },
   { KEY_KP1, SK_KP1 },
   { KEY_KP2, SK_KP2 },
   { KEY_KP3, SK_KP3 },
   { KEY_KP0, SK_KP0 },
   { KEY_KPDOT, SK_KP_PERIOD },

   { KEY_F11, SK_F11 },
   { KEY_F12, SK_F12 },

   { KEY_KPENTER, SK_KP_ENTER },
   { KEY_RIGHTCTRL, SK_RCTRL },
   { KEY_KPSLASH, SK_KP_DIVIDE },
   { KEY_SYSRQ, SK_PRINT },
   { KEY_RIGHTALT, SK_RALT },

   { KEY_HOME, SK_HOME },
   { KEY_UP, SK_UP },
   { KEY_PAGEUP, SK_PAGEUP },
   { KEY_LEFT, SK_LEFT },
   { KEY_RIGHT, SK_RIGHT },
   { KEY_END, SK_END },
   { KEY_DOWN, SK_DOWN },
   { KEY_PAGEDOWN, SK_PAGEDOWN },
   { KEY_INSERT, SK_INSERT },
   { KEY_DELETE, SK_DELETE },

   { KEY_PAUSE, SK_PAUSE },
};

static void init_lut(void)
{
   memset(keysym_lut, 0, sizeof(keysym_lut));
   for (unsigned i = 0; i < sizeof(lut_binds) / sizeof(lut_binds[0]); i++)
      keysym_lut[lut_binds[i].sk] = lut_binds[i].x;
}

static void linuxraw_resetKbmd()
{
   if (oldKbmd != 0xFFFF)
   {
      ioctl(0, KDSKBMODE, oldKbmd);
      tcsetattr(0, TCSAFLUSH, &oldTerm);
      oldKbmd = 0xFFFF;
   }
}

static void linuxraw_exitGracefully(int sig)
{
   linuxraw_resetKbmd();
   signal(sig, SIG_DFL);
   kill(getpid(), sig);
}

static void *linuxraw_input_init(void)
{
   // only work on terminals
   if (!isatty(0))
      return NULL;

   linuxraw_input_t *linuxraw = (linuxraw_input_t*)calloc(1, sizeof(*linuxraw));
   if (!linuxraw)
      return NULL;

   if (oldKbmd == 0xFFFF)
   {
      tcgetattr(0, &oldTerm);
      newTerm = oldTerm;
      newTerm.c_lflag &= ~(ECHO | ICANON | ISIG);
      newTerm.c_iflag &= ~(ISTRIP | IGNCR | ICRNL | INLCR | IXOFF | IXON);
      newTerm.c_cc[VMIN] = 0;
      newTerm.c_cc[VTIME] = 0;

      if (ioctl(0, KDGKBMODE, &oldKbmd) != 0)
         return NULL;
   }

   tcsetattr(0, TCSAFLUSH, &newTerm);

   if (ioctl(0, KDSKBMODE, K_MEDIUMRAW) != 0)
   {
      linuxraw_resetKbmd();
      return NULL;
   }

   // trap some standard termination codes so we can restore the keyboard before we lose control
   signal(SIGABRT, linuxraw_exitGracefully);
   signal(SIGBUS, linuxraw_exitGracefully);
   signal(SIGFPE, linuxraw_exitGracefully);
   signal(SIGILL, linuxraw_exitGracefully);
   signal(SIGQUIT, linuxraw_exitGracefully);
   signal(SIGSEGV, linuxraw_exitGracefully);

   atexit(linuxraw_resetKbmd);

   linuxraw->sdl = (sdl_input_t*)input_sdl.init();
   if (!linuxraw->sdl)
   {
      linuxraw_resetKbmd();
      free(linuxraw);
      return NULL;
   }

   init_lut();

   linuxraw->sdl->use_keyboard = false;
   return linuxraw;
}

static bool linuxraw_key_pressed(linuxraw_input_t *linuxraw, int key)
{
   return linuxraw->state[keysym_lut[key]];
}

static bool linuxraw_is_pressed(linuxraw_input_t *linuxraw, const struct snes_keybind *binds, unsigned id)
{
   if (id < RARCH_BIND_LIST_END)
   {
      const struct snes_keybind *bind = &binds[id];
      return bind->valid && linuxraw_key_pressed(linuxraw, binds[id].key);
   }
   else
      return false;
}

static bool linuxraw_bind_button_pressed(void *data, int key)
{
   linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;
   return linuxraw_is_pressed(linuxraw, g_settings.input.binds[0], key) ||
      input_sdl.key_pressed(linuxraw->sdl, key);
}

static int16_t linuxraw_input_state(void *data, const struct snes_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return linuxraw_is_pressed(linuxraw, binds[port], id) ||
            input_sdl.input_state(linuxraw->sdl, binds, port, device, index, id);

      default:
         return 0;
   }
}

static void linuxraw_input_free(void *data)
{
   linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;
   input_sdl.free(linuxraw->sdl);
   linuxraw_resetKbmd();
   free(data);
}

static void linuxraw_input_poll(void *data)
{
   linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;
   uint8_t c;
   uint16_t t;

   while (read(0, &c, 1))
   {
      bool pressed = !(c & 0x80);
      c &= ~0x80;

      // ignore extended scancodes
      if (!c)
         read(0, &t, 2);

      linuxraw->state[c] = pressed;
   }

   input_sdl.poll(linuxraw->sdl);
}

const input_driver_t input_linuxraw = {
   linuxraw_input_init,
   linuxraw_input_poll,
   linuxraw_input_state,
   linuxraw_bind_button_pressed,
   linuxraw_input_free,
   "linuxraw"
};
