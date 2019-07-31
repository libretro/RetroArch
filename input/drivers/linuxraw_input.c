/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2019 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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

#include <stdlib.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <signal.h>

#include <boolean.h>

#include "../../verbosity.h"

#include "../common/linux_common.h"

#include "../input_keymaps.h"
#include "../input_driver.h"

/* TODO/FIXME -
 * fix game focus toggle */

typedef struct linuxraw_input
{
   const input_device_driver_t *joypad;
   bool state[0x80];
} linuxraw_input_t;

static void *linuxraw_input_init(const char *joypad_driver)
{
   linuxraw_input_t *linuxraw  = NULL;

   /* Only work on terminals. */
   if (!isatty(0))
      return NULL;

   if (linux_terminal_grab_stdin(NULL))
   {
      RARCH_WARN("stdin is already used for content loading. Cannot use stdin for input.\n");
      return NULL;
   }

   linuxraw = (linuxraw_input_t*)calloc(1, sizeof(*linuxraw));
   if (!linuxraw)
      return NULL;

   if (!linux_terminal_disable_input())
   {
      linux_terminal_restore_input();
      free(linuxraw);
      return NULL;
   }

   linuxraw->joypad = input_joypad_init_driver(joypad_driver, linuxraw);
   input_keymaps_init_keyboard_lut(rarch_key_map_linux);

   linux_terminal_claim_stdin();

   return linuxraw;
}

static int16_t linuxraw_analog_pressed(linuxraw_input_t *linuxraw,
      const struct retro_keybind *binds, unsigned idx, unsigned id)
{
   const struct retro_keybind *bind_minus, *bind_plus;
   int16_t pressed_minus = 0, pressed_plus = 0;
   unsigned id_minus = 0;
   unsigned id_plus  = 0;

   input_conv_analog_id_to_bind_id(idx, id, id_minus, id_plus);

   bind_minus = &binds[id_minus];
   bind_plus  = &binds[id_plus];

   if (!bind_minus->valid || !bind_plus->valid)
      return 0;

   if (bind_minus->key < RETROK_LAST)
   {
      unsigned sym = rarch_keysym_lut[(enum retro_key)bind_minus->key];
      if (linuxraw->state[sym] & 0x80)
         pressed_minus = -0x7fff;
   }
   if (bind_plus->key  < RETROK_LAST)
   {
      unsigned sym = rarch_keysym_lut[(enum retro_key)bind_minus->key];
      if (linuxraw->state[sym] & 0x80)
         pressed_plus = 0x7fff;
   }

   return pressed_plus + pressed_minus;
}

static int16_t linuxraw_input_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds, unsigned port,
      unsigned device, unsigned idx, unsigned id)
{
   linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;
            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               /* Auto-binds are per joypad, not per user. */
               const uint64_t joykey  = (binds[port][i].joykey != NO_BTN)
                  ? binds[port][i].joykey : joypad_info.auto_binds[i].joykey;
               const uint32_t joyaxis = (binds[port][i].joyaxis != AXIS_NONE)
                  ? binds[port][i].joyaxis : joypad_info.auto_binds[i].joyaxis;

               if ((binds[port]->valid &&
                        linuxraw->state[rarch_keysym_lut[
                        (enum retro_key)binds[port][i].key]]
                   ))
               {
                  ret |= (1 << i);
                  continue;
               }

               if ((uint16_t)joykey != NO_BTN && linuxraw->joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
               {
                  ret |= (1 << i);
                  continue;
               }
               if (((float)abs(linuxraw->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
               {
                  ret |= (1 << i);
                  continue;
               }
            }

            return ret;
         }
         else
         {
            /* Auto-binds are per joypad, not per user. */
            const uint64_t joykey  = (binds[port][id].joykey != NO_BTN)
               ? binds[port][id].joykey : joypad_info.auto_binds[id].joykey;
            const uint32_t joyaxis = (binds[port][id].joyaxis != AXIS_NONE)
               ? binds[port][id].joyaxis : joypad_info.auto_binds[id].joyaxis;
            if (((id < RARCH_BIND_LIST_END) && binds[port]->valid &&
                     linuxraw->state[rarch_keysym_lut[(enum retro_key)binds[port][id].key]]
                ))
               return true;

            if ((uint16_t)joykey != NO_BTN && linuxraw->joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
               return true;
            if (((float)abs(linuxraw->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
               return true;
         }
         break;
      case RETRO_DEVICE_ANALOG:
         {
            int16_t ret = linuxraw_analog_pressed(linuxraw, binds[port], idx, id);
            if (!ret && binds[port])
               ret = input_joypad_analog(linuxraw->joypad,
                     joypad_info, port, idx, id, binds[port]);
            return ret;
         }
   }

   return 0;
}

static void linuxraw_input_free(void *data)
{
   linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;

   if (!linuxraw)
      return;

   if (linuxraw->joypad)
      linuxraw->joypad->destroy();

   linux_terminal_restore_input();
   free(data);
}

static bool linuxraw_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;
   if (!linuxraw)
      return false;
   return input_joypad_set_rumble(linuxraw->joypad, port, effect, strength);
}

static const input_device_driver_t *linuxraw_get_joypad_driver(void *data)
{
   linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;
   if (!linuxraw)
      return NULL;
   return linuxraw->joypad;
}

static void linuxraw_input_poll(void *data)
{
   uint8_t c;
   linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;

   while (read(STDIN_FILENO, &c, 1) > 0)
   {
      bool pressed;
      uint16_t t;

      if (c == KEY_C && (linuxraw->state[KEY_LEFTCTRL] || linuxraw->state[KEY_RIGHTCTRL]))
         kill(getpid(), SIGINT);

      pressed = !(c & 0x80);
      c &= ~0x80;

      /* ignore extended scancodes */
      if (!c)
         read(STDIN_FILENO, &t, 2);
      else
         linuxraw->state[c] = pressed;
   }

   if (linuxraw->joypad)
      linuxraw->joypad->poll();
}

static uint64_t linuxraw_get_capabilities(void *data)
{
   uint64_t caps = 0;

   (void)data;

   caps |= (1 << RETRO_DEVICE_JOYPAD);
   caps |= (1 << RETRO_DEVICE_ANALOG);

   return caps;
}

static void linuxraw_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

input_driver_t input_linuxraw = {
   linuxraw_input_init,
   linuxraw_input_poll,
   linuxraw_input_state,
   linuxraw_input_free,
   NULL,
   NULL,
   linuxraw_get_capabilities,
   "linuxraw",
   linuxraw_grab_mouse,
   linux_terminal_grab_stdin,
   linuxraw_set_rumble,
   linuxraw_get_joypad_driver,
   NULL,
   false
};
