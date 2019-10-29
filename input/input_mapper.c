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
#include <libretro.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "input_driver.h"
#include "input_mapper.h"

#ifdef HAVE_OVERLAY
#include "input_overlay.h"
#endif

#include "../configuration.h"

void input_mapper_poll(input_mapper_t *handle,
      void *ol_pointer,
      void *settings_data,
      void *input_data,
      unsigned max_users,
      bool poll_overlay)
{
   unsigned i, j;
#ifdef HAVE_OVERLAY
   input_overlay_t *overlay_pointer           = (input_overlay_t*)ol_pointer;
#endif
   settings_t *settings                       = (settings_t*)settings_data;
   input_bits_t *current_inputs               = (input_bits_t*)input_data;

   memset(handle->keys, 0, sizeof(handle->keys));

   for (i = 0; i < max_users; i++)
   {
      unsigned device  = settings->uints.input_libretro_device[i] 
         & RETRO_DEVICE_MASK;
      input_bits_t current_input = *current_inputs++;

      switch (device)
      {
         /* keyboard to gamepad remapping */
         case RETRO_DEVICE_KEYBOARD:
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
                     current_button_value |= input_overlay_key_pressed(overlay_pointer, j);
#endif
                  if ((current_button_value == 1) && (j != remap_button))
                  {
                     MAPPER_SET_KEY (handle,
                           remap_button);
                     input_keyboard_event(true,
                           remap_button,
                           0, 0, RETRO_DEVICE_KEYBOARD);
                     continue;
                  }

                  /* Release keyboard event*/
                  input_keyboard_event(false,
                        remap_button,
                        0, 0, RETRO_DEVICE_KEYBOARD);
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

            for (j = 0; j < 8; j++)
               handle->analog_value[i][j] = 0;

            for (j = 0; j < RARCH_FIRST_CUSTOM_BIND; j++)
            {
               bool remap_valid;
               unsigned remap_button         =
                  settings->uints.input_remap_ids[i][j];
               unsigned current_button_value = BIT256_GET(current_input, j);
#ifdef HAVE_OVERLAY
               if (poll_overlay && i == 0)
                  current_button_value |= input_overlay_key_pressed(overlay_pointer, j);
#endif
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
                     unsigned remap_axis_bind = remap_axis - RARCH_FIRST_CUSTOM_BIND;

                     if (remap_axis_bind < sizeof(handle->analog_value[i]))
                     {
                        int invert = 1;
                        if (  (k % 2 == 0 && remap_axis % 2 != 0) ||
                              (k % 2 != 0 && remap_axis % 2 == 0)
                           )
                           invert = -1;

                        handle->analog_value[i][
                           remap_axis_bind] =
                              current_axis_value * invert;
                     }
                  }
               }

            }
            break;
         default:
            break;
      }
   }
}
