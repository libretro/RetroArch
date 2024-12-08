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

typedef struct linuxraw_input
{
   bool state[0x80];
   linux_illuminance_sensor_t *illuminance_sensor;
} linuxraw_input_t;

static void *linuxraw_input_init(const char *joypad_driver)
{
   linuxraw_input_t *linuxraw  = NULL;

   /* Only work on terminals. */
   if (!isatty(0))
      return NULL;

   if (linux_terminal_grab_stdin(NULL))
   {
      RARCH_DBG("stdin is already used for content loading. Cannot use stdin for input.\n");
      return NULL;
   }

   if (!(linuxraw = (linuxraw_input_t*)calloc(1, sizeof(*linuxraw))))
      return NULL;

   if (!linux_terminal_disable_input())
   {
      linux_terminal_restore_input();
      free(linuxraw);
      return NULL;
   }

   input_keymaps_init_keyboard_lut(rarch_key_map_linux);

   linux_terminal_claim_stdin();

   return linuxraw;
}

static int16_t linuxraw_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;

            if (!keyboard_mapping_blocked)
            {
               for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
               {
                  if (binds[port][i].valid)
                  {
                     if (     (binds[port][i].key && binds[port][i].key < RETROK_LAST)
                           && linuxraw->state[rarch_keysym_lut[(enum retro_key)binds[port][i].key]])
                        ret |= (1 << i);
                  }
               }
            }

            return ret;
         }

         if (id < RARCH_BIND_LIST_END)
         {
            if (binds[port][id].valid)
            {
               if (     (binds[port][id].key && binds[port][id].key < RETROK_LAST)
                     && linuxraw->state[rarch_keysym_lut[(enum retro_key)binds[port][id].key]]
                     && (id == RARCH_GAME_FOCUS_TOGGLE || !keyboard_mapping_blocked)
                  )
                  return 1;
            }
         }
         break;
      case RETRO_DEVICE_ANALOG:
         if (binds[port])
         {
            int id_minus_key      = 0;
            int id_plus_key       = 0;
            unsigned id_minus     = 0;
            unsigned id_plus      = 0;
            int16_t ret           = 0;
            bool id_plus_valid    = false;
            bool id_minus_valid   = false;

            input_conv_analog_id_to_bind_id(idx, id, id_minus, id_plus);

            id_minus_valid        = binds[port][id_minus].valid;
            id_plus_valid         = binds[port][id_plus].valid;
            id_minus_key          = binds[port][id_minus].key;
            id_plus_key           = binds[port][id_plus].key;

            if (id_plus_valid && id_plus_key && id_plus_key < RETROK_LAST)
            {
               unsigned sym = rarch_keysym_lut[(enum retro_key)id_plus_key];
               if (linuxraw->state[sym] & 0x80)
                  ret = 0x7fff;
            }
            if (id_minus_valid && id_minus_key && id_minus_key < RETROK_LAST)
            {
               unsigned sym = rarch_keysym_lut[(enum retro_key)id_minus_key];
               if (linuxraw->state[sym] & 0x80)
                  ret += -0x7fff;
            }

            return ret;
         }
         break;
   }

   return 0;
}

static void linuxraw_input_free(void *data)
{
   linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;

   if (!linuxraw)
      return;

   linux_terminal_restore_input();
   linux_close_illuminance_sensor(linuxraw->illuminance_sensor);
   free(data);
}

static bool linuxraw_input_set_sensor_state(void *data, unsigned port, enum retro_sensor_action action, unsigned rate)
{
   linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;

   if (!linuxraw)
      return false;

   switch (action)
   {
      case RETRO_SENSOR_ILLUMINANCE_DISABLE:
         /* If already disabled, then do nothing */
         linux_close_illuminance_sensor(linuxraw->illuminance_sensor); /* noop if NULL */
         linuxraw->illuminance_sensor = NULL;
      case RETRO_SENSOR_GYROSCOPE_DISABLE:
      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         /** Unimplemented sensor actions that probably shouldn't fail */
         return true;

      case RETRO_SENSOR_ILLUMINANCE_ENABLE:
         if (linuxraw->illuminance_sensor)
            /* If the light sensor is already open, just set the rate */
            linux_set_illuminance_sensor_rate(linuxraw->illuminance_sensor, rate);
         else
            linuxraw->illuminance_sensor = linux_open_illuminance_sensor(rate);

         return linuxraw->illuminance_sensor != NULL;
      default:
         break;
   }

   return false;
}

static float linuxraw_input_get_sensor_input(void *data, unsigned port, unsigned id)
{
   linuxraw_input_t *linuxraw = (linuxraw_input_t*)data;

   if (!linuxraw)
      return 0.0f;

   switch (id)
   {
      case RETRO_SENSOR_ILLUMINANCE:
         if (linuxraw->illuminance_sensor)
            return linux_get_illuminance_reading(linuxraw->illuminance_sensor);
      default:
         break;
   }

   return 0.0f;
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

      pressed  = !(c & 0x80);
      c       &= ~0x80;

      /* ignore extended scancodes */
      if (!c)
         read(STDIN_FILENO, &t, 2);
      else
         linuxraw->state[c] = pressed;
   }
}

static uint64_t linuxraw_get_capabilities(void *data)
{
   return (1 << RETRO_DEVICE_JOYPAD)
        | (1 << RETRO_DEVICE_ANALOG);
}

input_driver_t input_linuxraw = {
   linuxraw_input_init,
   linuxraw_input_poll,
   linuxraw_input_state,
   linuxraw_input_free,
   linuxraw_input_set_sensor_state,
   linuxraw_input_get_sensor_input,
   linuxraw_get_capabilities,
   "linuxraw",
   NULL,                         /* grab_mouse */
   linux_terminal_grab_stdin,
   NULL
};
