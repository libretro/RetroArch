/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2018 - Francisco Javier Trujillo Mata - fjtrujy
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

#include <stdint.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <libpad.h>

#include <boolean.h>
#include <libretro.h>
#include <retro_miscellaneous.h>

#include "../input_driver.h"

typedef struct ps2_input
{
   const input_device_driver_t *joypad;
} ps2_input_t;

static void ps2_input_poll(void *data)
{
   ps2_input_t *ps2 = (ps2_input_t*)data;

   if (ps2 && ps2->joypad)
      ps2->joypad->poll();
}

static int16_t ps2_input_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   ps2_input_t *ps2           = (ps2_input_t*)data;

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

               if ((uint16_t)joykey != NO_BTN && ps2->joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
               {
                  ret |= (1 << i);
                  continue;
               }
               if (((float)abs(ps2->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
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

            if ((uint16_t)joykey != NO_BTN && ps2->joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
               return true;
            if (((float)abs(ps2->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
               return true;
         }
         break;
      case RETRO_DEVICE_ANALOG:
         if (binds[port])
            return input_joypad_analog(ps2->joypad, joypad_info, port, idx, id, binds[port]);
         break;
   }

   return 0;
}

static void ps2_input_free_input(void *data)
{
   ps2_input_t *ps2 = (ps2_input_t*)data;

   if (ps2 && ps2->joypad)
      ps2->joypad->destroy();

   free(data);
}

static void* ps2_input_initialize(const char *joypad_driver)
{
   ps2_input_t *ps2 = (ps2_input_t*)calloc(1, sizeof(*ps2));
   if (!ps2)
      return NULL;

   ps2->joypad = input_joypad_init_driver(joypad_driver, ps2);

   return ps2;
}

static uint64_t ps2_input_get_capabilities(void *data)
{
   (void)data;

   return (1 << RETRO_DEVICE_JOYPAD) |  (1 << RETRO_DEVICE_ANALOG);
}

static const input_device_driver_t *ps2_input_get_joypad_driver(void *data)
{
   ps2_input_t *ps2 = (ps2_input_t*)data;
   if (ps2)
      return ps2->joypad;
   return NULL;
}

static void ps2_input_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool ps2_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   ps2_input_t *ps2 = (ps2_input_t*)data;

   if (ps2 && ps2->joypad)
      return input_joypad_set_rumble(ps2->joypad,
         port, effect, strength);
   return false;
}

input_driver_t input_ps2 = {
   ps2_input_initialize,
   ps2_input_poll,
   ps2_input_state,
   ps2_input_free_input,
   NULL,
   NULL,
   ps2_input_get_capabilities,
   "ps2",
   ps2_input_grab_mouse,
   NULL,
   ps2_input_set_rumble,
   ps2_input_get_joypad_driver,
   NULL,
   false
};
