/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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
#include "input_common.h"

static long oldKbmd = 0xffff;
static struct termios oldTerm, newTerm;

typedef struct linuxraw_input
{
   const rarch_joypad_driver_t *joypad;
   bool state[0x80];
} linuxraw_input_t;


struct key_bind
{
   uint8_t x;
   enum retro_key sk;
};

static unsigned keysym_lut[RETROK_LAST];
static const struct key_bind lut_binds[] = {
   { KEY_ESC, RETROK_ESCAPE },
   { KEY_1, RETROK_1 },
   { KEY_2, RETROK_2 },
   { KEY_3, RETROK_3},
   { KEY_4, RETROK_4 },
   { KEY_5, RETROK_5 },
   { KEY_6, RETROK_6 },
   { KEY_7, RETROK_7 },
   { KEY_8, RETROK_8 },
   { KEY_9, RETROK_9 },
   { KEY_0, RETROK_0 },
   { KEY_MINUS, RETROK_MINUS },
   { KEY_EQUAL, RETROK_EQUALS },
   { KEY_BACKSPACE, RETROK_BACKSPACE },
   { KEY_TAB, RETROK_TAB },
   { KEY_Q, RETROK_q },
   { KEY_W, RETROK_w },
   { KEY_E, RETROK_e },
   { KEY_R, RETROK_r },
   { KEY_T, RETROK_t },
   { KEY_Y, RETROK_y },
   { KEY_U, RETROK_u },
   { KEY_I, RETROK_i },
   { KEY_O, RETROK_o },
   { KEY_P, RETROK_p },
   { KEY_LEFTBRACE, RETROK_LEFTBRACKET },
   { KEY_RIGHTBRACE, RETROK_RIGHTBRACKET },
   { KEY_ENTER, RETROK_RETURN },
   { KEY_LEFTCTRL, RETROK_LCTRL },
   { KEY_A, RETROK_a },
   { KEY_S, RETROK_s },
   { KEY_D, RETROK_d },
   { KEY_F, RETROK_f },
   { KEY_G, RETROK_g },
   { KEY_H, RETROK_h },
   { KEY_J, RETROK_j },
   { KEY_K, RETROK_k },
   { KEY_L, RETROK_l },
   { KEY_SEMICOLON, RETROK_SEMICOLON },
   { KEY_APOSTROPHE, RETROK_QUOTE },
   { KEY_GRAVE, RETROK_BACKQUOTE },
   { KEY_LEFTSHIFT, RETROK_LSHIFT },
   { KEY_BACKSLASH, RETROK_BACKSLASH },
   { KEY_Z, RETROK_z },
   { KEY_X, RETROK_x },
   { KEY_C, RETROK_c },
   { KEY_V, RETROK_v },
   { KEY_B, RETROK_b },
   { KEY_N, RETROK_n },
   { KEY_M, RETROK_m },
   { KEY_COMMA, RETROK_COMMA },
   { KEY_DOT, RETROK_PERIOD },
   { KEY_SLASH, RETROK_SLASH },
   { KEY_RIGHTSHIFT, RETROK_RSHIFT },
   { KEY_KPASTERISK, RETROK_KP_MULTIPLY },
   { KEY_LEFTALT, RETROK_LALT },
   { KEY_SPACE, RETROK_SPACE },
   { KEY_CAPSLOCK, RETROK_CAPSLOCK },
   { KEY_F1, RETROK_F1 },
   { KEY_F2, RETROK_F2 },
   { KEY_F3, RETROK_F3 },
   { KEY_F4, RETROK_F4 },
   { KEY_F5, RETROK_F5 },
   { KEY_F6, RETROK_F6 },
   { KEY_F7, RETROK_F7 },
   { KEY_F8, RETROK_F8 },
   { KEY_F9, RETROK_F9 },
   { KEY_F10, RETROK_F10 },
   { KEY_NUMLOCK, RETROK_NUMLOCK },
   { KEY_SCROLLLOCK, RETROK_SCROLLOCK },
   { KEY_KP7, RETROK_KP7 },
   { KEY_KP8, RETROK_KP8 },
   { KEY_KP9, RETROK_KP9 },
   { KEY_KPMINUS, RETROK_KP_MINUS },
   { KEY_KP4, RETROK_KP4 },
   { KEY_KP5, RETROK_KP5 },
   { KEY_KP6, RETROK_KP6 },
   { KEY_KPPLUS, RETROK_KP_PLUS },
   { KEY_KP1, RETROK_KP1 },
   { KEY_KP2, RETROK_KP2 },
   { KEY_KP3, RETROK_KP3 },
   { KEY_KP0, RETROK_KP0 },
   { KEY_KPDOT, RETROK_KP_PERIOD },

   { KEY_F11, RETROK_F11 },
   { KEY_F12, RETROK_F12 },

   { KEY_KPENTER, RETROK_KP_ENTER },
   { KEY_RIGHTCTRL, RETROK_RCTRL },
   { KEY_KPSLASH, RETROK_KP_DIVIDE },
   { KEY_SYSRQ, RETROK_PRINT },
   { KEY_RIGHTALT, RETROK_RALT },

   { KEY_HOME, RETROK_HOME },
   { KEY_UP, RETROK_UP },
   { KEY_PAGEUP, RETROK_PAGEUP },
   { KEY_LEFT, RETROK_LEFT },
   { KEY_RIGHT, RETROK_RIGHT },
   { KEY_END, RETROK_END },
   { KEY_DOWN, RETROK_DOWN },
   { KEY_PAGEDOWN, RETROK_PAGEDOWN },
   { KEY_INSERT, RETROK_INSERT },
   { KEY_DELETE, RETROK_DELETE },

   { KEY_PAUSE, RETROK_PAUSE },
};

static void init_lut(void)
{
   unsigned i;
   memset(keysym_lut, 0, sizeof(keysym_lut));
   for (i = 0; i < ARRAY_SIZE(lut_binds); i++)
      keysym_lut[lut_binds[i].sk] = lut_binds[i].x;
}

static void linuxraw_resetKbmd(void)
{
   if (oldKbmd != 0xffff)
   {
      ioctl(0, KDSKBMODE, oldKbmd);
      tcsetattr(0, TCSAFLUSH, &oldTerm);
      oldKbmd = 0xffff;
   }

   driver.stdin_claimed = false;
}

static void linuxraw_exitGracefully(int sig)
{
   linuxraw_resetKbmd();
   kill(getpid(), sig);
}

static void *linuxraw_input_init(void)
{
   // only work on terminals
   if (!isatty(0))
      return NULL;

   if (driver.stdin_claimed)
   {
      RARCH_WARN("stdin is already used for ROM loading. Cannot use stdin for input.\n");
      return NULL;
   }

   linuxraw_input_t *linuxraw = (linuxraw_input_t*)calloc(1, sizeof(*linuxraw));
   if (!linuxraw)
      return NULL;

   if (oldKbmd == 0xffff)
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

   struct sigaction sa;
   sa.sa_handler = linuxraw_exitGracefully;
   sa.sa_flags = SA_RESTART | SA_RESETHAND;
   sigemptyset(&sa.sa_mask);
   // trap some standard termination codes so we can restore the keyboard before we lose control
   sigaction(SIGABRT, &sa, NULL);
   sigaction(SIGBUS, &sa, NULL);
   sigaction(SIGFPE, &sa, NULL);
   sigaction(SIGILL, &sa, NULL);
   sigaction(SIGQUIT, &sa, NULL);
   sigaction(SIGSEGV, &sa, NULL);

   atexit(linuxraw_resetKbmd);

   linuxraw->joypad = input_joypad_init_driver(g_settings.input.joypad_driver);
   init_lut();

   driver.stdin_claimed = true; // We need to disable use of stdin command interface if stdin is supposed to be used for input.
   return linuxraw;
}

static bool linuxraw_key_pressed(linuxraw_input_t *linuxraw, int key)
{
   return linuxraw->state[keysym_lut[key]];
}

static bool linuxraw_is_pressed(linuxraw_input_t *linuxraw, const struct retro_keybind *binds, unsigned id)
{
   if (id < RARCH_BIND_LIST_END)
   {
      const struct retro_keybind *bind = &binds[id];
      return bind->valid && linuxraw_key_pressed(linuxraw, binds[id].key);
   }
   else
      return false;
}

static bool linuxraw_bind_button_pressed(void *data, int key)
{
   linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;
   return linuxraw_is_pressed(linuxraw, g_settings.input.binds[0], key) ||
      input_joypad_pressed(linuxraw->joypad, 0, g_settings.input.binds[0], key);
}

static int16_t linuxraw_input_state(void *data, const struct retro_keybind **binds, unsigned port, unsigned device, unsigned index, unsigned id)
{
   linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return linuxraw_is_pressed(linuxraw, binds[port], id) ||
            input_joypad_pressed(linuxraw->joypad, port, binds[port], id);

      case RETRO_DEVICE_ANALOG:
         return input_joypad_analog(linuxraw->joypad, port, index, id, binds[port]);

      default:
         return 0;
   }
}

static void linuxraw_input_free(void *data)
{
   linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;

   if (linuxraw->joypad)
      linuxraw->joypad->destroy();

   linuxraw_resetKbmd();
   free(data);
}

static bool linuxraw_set_rumble(void *data, unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
   linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;
   return input_joypad_set_rumble(linuxraw->joypad, port, effect, strength);
}

static const rarch_joypad_driver_t *linuxraw_get_joypad_driver(void *data)
{
   linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;
   return linuxraw->joypad;
}

static void linuxraw_input_poll(void *data)
{
   linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;
   uint8_t c;
   uint16_t t;

   while (read(STDIN_FILENO, &c, 1) > 0)
   {
      if (c == KEY_C && (linuxraw->state[KEY_LEFTCTRL] || linuxraw->state[KEY_RIGHTCTRL]))
         kill(getpid(), SIGINT);

      bool pressed = !(c & 0x80);
      c &= ~0x80;

      // ignore extended scancodes
      if (!c)
         read(STDIN_FILENO, &t, 2);
      else
         linuxraw->state[c] = pressed;
   }

   input_joypad_poll(linuxraw->joypad);
}

const input_driver_t input_linuxraw = {
   linuxraw_input_init,
   linuxraw_input_poll,
   linuxraw_input_state,
   linuxraw_bind_button_pressed,
   linuxraw_input_free,
   NULL,
   "linuxraw",
   NULL,
   linuxraw_set_rumble,
   linuxraw_get_joypad_driver,
};
