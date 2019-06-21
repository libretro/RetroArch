/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Andrés Suárez
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <compat/strl.h>
#include <compat/posix_string.h>
#include <retro_miscellaneous.h>
#include <libretro.h>

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#endif

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "input_mapper.h"

#ifdef HAVE_OVERLAY
#include "input_overlay.h"
#endif

#include "../configuration.h"
#include "../msg_hash.h"
#include "../verbosity.h"

#define MAPPER_GET_KEY(state, key) (((state)->keys[(key) / 32] >> ((key) % 32)) & 1)
#define MAPPER_SET_KEY(state, key) (state)->keys[(key) / 32] |= 1 << ((key) % 32)

struct input_mapper
{
   /* Left X, Left Y, Right X, Right Y */
   int16_t analog_value[MAX_USERS][8];
   /* the whole keyboard state */
   uint32_t keys[RETROK_LAST / 32 + 1];
   /* This is a bitmask of (1 << key_bind_id). */
   input_bits_t buttons[MAX_USERS];
};

static bool input_mapper_button_pressed(input_mapper_t *handle, unsigned port, unsigned id)
{
   return BIT256_GET(handle->buttons[port], id);
}

input_mapper_t *input_mapper_new(void)
{
   input_mapper_t* handle = (input_mapper_t*)
      calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   return handle;
}

void input_mapper_free(input_mapper_t *handle)
{
   if (!handle)
      return;
   free (handle);
}

void input_mapper_poll(input_mapper_t *handle)
{
   unsigned i, j;
   input_bits_t current_input;
   settings_t *settings                       = config_get_ptr();
   unsigned max_users                         =
      *(input_driver_get_uint(INPUT_ACTION_MAX_USERS));
   bool key_event[RARCH_CUSTOM_BIND_LIST_END] = { false };
#ifdef HAVE_OVERLAY
   bool poll_overlay = input_overlay_is_alive(overlay_ptr) ? true : false;
#endif

#ifdef HAVE_MENU
   if (menu_driver_is_alive())
      return;
#endif

   memset(handle->keys, 0, sizeof(handle->keys));

   for (i = 0; i < max_users; i++)
   {
      unsigned device  = settings->uints.input_libretro_device[i];
      device          &= RETRO_DEVICE_MASK;

      switch (device)
      {
            /* keyboard to gamepad remapping */
         case RETRO_DEVICE_KEYBOARD:
            BIT256_CLEAR_ALL_PTR(&current_input);
            input_get_state_for_port(settings, i, &current_input);
            for (j = 0; j < RARCH_CUSTOM_BIND_LIST_END; j++)
            {
               unsigned remap_button         =
                  settings->uints.input_keymapper_ids[i][j];
               bool remap_valid              = remap_button != RETROK_UNKNOWN;

               if (remap_valid)
               {
                  unsigned current_button_value = BIT256_GET(current_input, j);
#ifdef HAVE_OVERLAY
               if (poll_overlay && i == 0)
                  current_button_value |= input_overlay_key_pressed(overlay_ptr, j);
#endif
                  if ((current_button_value == 1) && (j != remap_button))
                  {
                     MAPPER_SET_KEY (handle,
                           remap_button);
                     input_keyboard_event(true,
                           remap_button,
                           0, 0, RETRO_DEVICE_KEYBOARD);
                     key_event[j] = true;
                  }
                  /* key_event tracks if a key is pressed for ANY PLAYER, so we must check
                     if the key was used by any player before releasing */
                  else if (!key_event[j])
                  {
                     input_keyboard_event(false,
                           remap_button,
                           0, 0, RETRO_DEVICE_KEYBOARD);
                  }
               }
            }
            break;

            /* gamepad remapping */
         case RETRO_DEVICE_JOYPAD:
         case RETRO_DEVICE_ANALOG:
            /* this loop iterates on all users and all buttons,
             * and checks if a pressed button is assigned to any
             * other button than the default one, then it sets
             * the bit on the mapper input bitmap, later on the
             * original input is cleared in input_state */
            BIT256_CLEAR_ALL(handle->buttons[i]);
            BIT256_CLEAR_ALL_PTR(&current_input);

            for (j = 0; j < 8; j++)
               handle->analog_value[i][j] = 0;

            input_get_state_for_port(settings, i, &current_input);

            for (j = 0; j < RARCH_FIRST_CUSTOM_BIND; j++)
            {
               bool remap_valid;
               unsigned remap_button;
               unsigned current_button_value = BIT256_GET(current_input, j);
#ifdef HAVE_OVERLAY
               if (poll_overlay && i == 0)
                  current_button_value |= input_overlay_key_pressed(overlay_ptr, j);
#endif

               remap_button                  =
                  settings->uints.input_remap_ids[i][j];
               remap_valid                   = (current_button_value == 1) &&
                  (j != remap_button) && (remap_button != RARCH_UNMAPPED);

               if (remap_valid)
               {
                  if (remap_button < RARCH_FIRST_CUSTOM_BIND)
                  {
                     BIT256_SET(handle->buttons[i], remap_button);
                  }
                  else if (remap_button >= RARCH_FIRST_CUSTOM_BIND)
                  {
                     int invert = 1;

                     if (remap_button % 2 != 0)
                        invert = -1;

                     handle->analog_value[i][
                        remap_button - RARCH_FIRST_CUSTOM_BIND] =
                           (current_input.analog_buttons[j] ? current_input.analog_buttons[j] : 32767) * invert;
                  }
               }
            }

            for (j = 0; j < 8; j++)
            {
               unsigned k                 = j + RARCH_FIRST_CUSTOM_BIND;
               int16_t current_axis_value = current_input.analogs[j];
               unsigned remap_axis        =
                  settings->uints.input_remap_ids[i][k];

               if (
                     (abs(current_axis_value) > 0 &&
                     (k != remap_axis)            &&
                     (remap_axis != RARCH_UNMAPPED)
                  ))
               {
                  if (remap_axis < RARCH_FIRST_CUSTOM_BIND &&
                     abs(current_axis_value) > *input_driver_get_float(INPUT_ACTION_AXIS_THRESHOLD) * 32767)
                  {
                     BIT256_SET(handle->buttons[i], remap_axis);
                  }
                  else
                  {
                     int invert = 1;
                     unsigned remap_axis_bind = remap_axis - RARCH_FIRST_CUSTOM_BIND;

                     if (  (k % 2 == 0 && remap_axis % 2 != 0) ||
                           (k % 2 != 0 && remap_axis % 2 == 0)
                        )
                        invert = -1;

                     if (remap_axis_bind < sizeof(handle->analog_value[i]))
                     {
                        handle->analog_value[i][
                           remap_axis_bind] =
                              current_axis_value * invert;
                     }
#if 0
                     RARCH_LOG("axis %d(%d) remapped to axis %d val %d\n",
                           j, k,
                           remap_axis - RARCH_FIRST_CUSTOM_BIND,
                           current_axis_value);
#endif
                  }
               }

            }
            break;
         default:
            break;
      }
   }
}

void input_mapper_state(
      input_mapper_t *handle,
      int16_t *ret,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   if (!handle)
      return;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (input_mapper_button_pressed(handle, port, id))
            *ret            = 1;
         break;
      case RETRO_DEVICE_ANALOG:
         if (idx < 2 && id < 2)
         {
            int         val = 0;
            unsigned offset = 0 + (idx * 4) + (id * 2);
            int        val1 = handle->analog_value[port][offset];
            int        val2 = handle->analog_value[port][offset+1];

            if (val1)
               val          = val1;
            else if (val2)
               val          = val2;

            if (val1 || val2)
               *ret        |= val;
         }
         break;
      case RETRO_DEVICE_KEYBOARD:
         if (id < RETROK_LAST)
            if (MAPPER_GET_KEY(handle, id))
               *ret |= 1;
         break;
      default:
         break;
   }
   return;
}
