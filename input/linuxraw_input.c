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
      {
         free(linuxraw);
         return NULL;
      }
   }

   tcsetattr(0, TCSAFLUSH, &newTerm);

   if (ioctl(0, KDSKBMODE, K_MEDIUMRAW) != 0)
   {
      linuxraw_resetKbmd();
      free(linuxraw);
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
   input_init_keyboard_lut(rarch_key_map_linux);

   driver.stdin_claimed = true; // We need to disable use of stdin command interface if stdin is supposed to be used for input.
   return linuxraw;
}

static bool linuxraw_key_pressed(linuxraw_input_t *linuxraw, int key)
{
   unsigned sym = input_translate_rk_to_keysym((enum retro_key)key);
   return linuxraw->state[sym];
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

static int16_t linuxraw_analog_pressed(linuxraw_input_t *linuxraw,
      const struct retro_keybind *binds, unsigned index, unsigned id)
{
   unsigned id_minus = 0;
   unsigned id_plus  = 0;
   input_conv_analog_id_to_bind_id(index, id, &id_minus, &id_plus);

   int16_t pressed_minus = linuxraw_is_pressed(linuxraw,
         binds, id_minus) ? -0x7fff : 0;
   int16_t pressed_plus = linuxraw_is_pressed(linuxraw,
         binds, id_plus) ? 0x7fff : 0;
   return pressed_plus + pressed_minus;
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
   int16_t ret;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return linuxraw_is_pressed(linuxraw, binds[port], id) ||
            input_joypad_pressed(linuxraw->joypad, port, binds[port], id);

      case RETRO_DEVICE_ANALOG:
         ret = linuxraw_analog_pressed(linuxraw, binds[port], index, id);
         if (!ret)
            ret = input_joypad_analog(linuxraw->joypad, port, index, id, binds[port]);
         return ret;

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

static uint64_t linuxraw_get_capabilities(void *data)
{
   (void)data;
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);
   caps |= (1 << RETRO_DEVICE_ANALOG);

   return caps;
}

const input_driver_t input_linuxraw = {
   linuxraw_input_init,
   linuxraw_input_poll,
   linuxraw_input_state,
   linuxraw_bind_button_pressed,
   linuxraw_input_free,
   NULL,
   NULL,
   linuxraw_get_capabilities,
   "linuxraw",
   NULL,
   linuxraw_set_rumble,
   linuxraw_get_joypad_driver,
};
