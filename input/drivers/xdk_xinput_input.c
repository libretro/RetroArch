/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifdef _XBOX
#include <xtl.h>
#endif

#include <boolean.h>
#include <libretro.h>

#include "../../config.def.h"

#include "../input_driver.h"

/* TODO/FIXME -
 * fix game focus toggle */

typedef struct xdk_input
{
   const input_device_driver_t *joypad;
} xdk_input_t;

static void xdk_input_poll(void *data)
{
   xdk_input_t *xdk = (xdk_input_t*)data;

   if (xdk && xdk->joypad)
      xdk->joypad->poll();
}

static int16_t xdk_input_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   xdk_input_t *xdk           = (xdk_input_t*)data;

   if (port >= DEFAULT_MAX_PADS)
      return 0;

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

               if ((uint16_t)joykey != NO_BTN && xdk->joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
               {
                  ret |= (1 << i);
                  continue;
               }
               if (((float)abs(xdk->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
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

            if ((uint16_t)joykey != NO_BTN && xdk->joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
               return true;
            if (((float)abs(xdk->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
               return true;
         }
         break;
      case RETRO_DEVICE_ANALOG:
         if (binds[port])
            return input_joypad_analog(xdk->joypad, joypad_info, port, index, id, binds[port]);
         break;
   }

   return 0;
}

static void xdk_input_free_input(void *data)
{
   xdk_input_t *xdk = (xdk_input_t*)data;

   if (!xdk)
      return;

   if (xdk->joypad)
      xdk->joypad->destroy();

   free(xdk);
}

static void *xdk_input_init(const char *joypad_driver)
{
   xdk_input_t *xdk     = (xdk_input_t*)calloc(1, sizeof(*xdk));
   if (!xdk)
      return NULL;

   xdk->joypad = input_joypad_init_driver(joypad_driver, xdk);

   return xdk;
}

static uint64_t xdk_input_get_capabilities(void *data)
{
   (void)data;

   return (1 << RETRO_DEVICE_JOYPAD) | (1 << RETRO_DEVICE_ANALOG);
}

/* FIXME - are we sure about treating low frequency motor as the
 * "strong" motor? Does it apply for Xbox too? */

static bool xdk_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
#ifdef _XBOX360
#if 0
   XINPUT_VIBRATION rumble_state;
#endif
#endif
   xdk_input_t *xdk = (xdk_input_t*)data;
   bool val         = false;

   (void)xdk;

#if 0
#if defined(_XBOX360)
   if (effect == RETRO_RUMBLE_STRONG)
      rumble_state.wLeftMotorSpeed = strength;
   else if (effect == RETRO_RUMBLE_WEAK)
      rumble_state.wRightMotorSpeed = strength;
   val = XInputSetState(port, &rumble_state) == ERROR_SUCCESS;
#elif defined(_XBOX1)
#if 0
   XINPUT_FEEDBACK rumble_state;

   if (effect == RETRO_RUMBLE_STRONG)
      rumble_state.Rumble.wLeftMotorSpeed = strength;
   else if (effect == RETRO_RUMBLE_WEAK)
      rumble_state.Rumble.wRightMotorSpeed = strength;
   val = XInputSetState(xdk->gamepads[port], &rumble_state) == ERROR_SUCCESS;
#endif
#endif
#endif
   return val;
}

static const input_device_driver_t *xdk_input_get_joypad_driver(void *data)
{
   xdk_input_t *xdk = (xdk_input_t*)data;
   if (!xdk)
      return NULL;
   return xdk->joypad;
}

static void xdk_input_grab_mouse(void *data, bool state)
{
   (void)data;
   (void)state;
}

input_driver_t input_xinput = {
   xdk_input_init,
   xdk_input_poll,
   xdk_input_state,
   xdk_input_free_input,
   NULL,
   NULL,
   xdk_input_get_capabilities,
   "xinput",
   xdk_input_grab_mouse,
   NULL,
   xdk_input_set_rumble,
   xdk_input_get_joypad_driver,
   NULL,
   false
};
