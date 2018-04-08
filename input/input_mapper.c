/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Andrés Suárez
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
   retro_bits_t buttons[MAX_USERS];
};

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

bool input_mapper_button_pressed(input_mapper_t *handle, unsigned port, unsigned id)
{
   return BIT256_GET(handle->buttons[port], id);
}

void input_mapper_poll(input_mapper_t *handle)
{
   int i, j, k;
   settings_t *settings = config_get_ptr();
   retro_bits_t current_input;
   unsigned max_users   = *(input_driver_get_uint(INPUT_ACTION_MAX_USERS));
   unsigned device      = 0;
   unsigned current_button_value;
   int16_t current_axis_value;
   unsigned remap_button, remap_axis;
   bool key_event[RARCH_CUSTOM_BIND_LIST_END];
#ifdef HAVE_MENU
   bool menu_is_alive   = menu_driver_is_alive();
#endif

#ifdef HAVE_MENU
   if (menu_is_alive)
      return;
#endif

   memset(handle->keys, 0, sizeof(handle->keys));

   for (i = 0; i < max_users; i++)
   {
      device = settings->uints.input_libretro_device[i];
      device &= RETRO_DEVICE_MASK;

      /* keyboard to gamepad remapping */
      if (device == RETRO_DEVICE_KEYBOARD)
      {
         input_get_state_for_port(settings, i, &current_input);
         for (j = 0; j < RARCH_CUSTOM_BIND_LIST_END; j++)
         {
            {
               current_button_value = BIT256_GET(current_input, j);
               remap_button = settings->uints.input_keymapper_ids[i][j];
               if (current_button_value == 1 && j != remap_button &&
                  remap_button != RETROK_UNKNOWN)
               {
                  MAPPER_SET_KEY (handle,
                     remap_button);
                  input_keyboard_event(true,
                        remap_button,
                        0, 0, RETRO_DEVICE_KEYBOARD);
                  key_event[j] = true;
               }
               else
               {
                  if (key_event[j] == false &&
                     remap_button != RETROK_UNKNOWN)
                  {
                     input_keyboard_event(false,
                           remap_button,
                           0, 0, RETRO_DEVICE_KEYBOARD);
                  }
               }
            }
         }
      }

      /* gamepad remapping */
      if (device == RETRO_DEVICE_JOYPAD || device == RETRO_DEVICE_ANALOG)
      {
         /* this loop iterates on all users and all buttons, and checks if a pressed button
            is assigned to any other button than the default one, then it sets the bit on the
            mapper input bitmap, later on the original input is cleared in input_state */
         BIT256_CLEAR_ALL(handle->buttons[i]);
         for (j = 0; j < 8; j++)
            handle->analog_value[i][j] = 0;

         input_get_state_for_port(settings, i, &current_input);

         for (j = 0; j < RARCH_FIRST_CUSTOM_BIND; j++)
         {
            current_button_value = BIT256_GET(current_input, j);
            remap_button = settings->uints.input_remap_ids[i][j];
            if (current_button_value == 1 && j != remap_button &&
               remap_button != RARCH_UNMAPPED)
               BIT256_SET(handle->buttons[i], remap_button);
         }
#if 1
         /* --CURRENTLY NOT IMPLEMENTED--
            this loop should iterate on all users and all analog stick axes and if the axes are
            moved and is assigned to a button it should set the bit on the mapper input bitmap.
            Once implemented we should make sure to clear the original analog
            stick input in input_state in input_driver.c */

         for (j = 0; j < 8; j++)
         {
            k = j + RARCH_FIRST_CUSTOM_BIND;

            current_axis_value = current_input.analogs[j];
            remap_axis = settings->uints.input_remap_ids[i][k];

            if (current_axis_value != 0 && k != remap_axis && remap_axis != RARCH_UNMAPPED)
            {
               if (remap_axis < RARCH_FIRST_CUSTOM_BIND)
               {
                  BIT256_SET(handle->buttons[i], remap_axis);
                  /* RARCH_LOG("axis %d remapped to button %d val %d\n", j, remap_axis, current_axis_value); */
               }
               else
               {
                  int invert = 1;
                  /*if ((k == 16 && remap_axis == 17) || (k == 17 && remap_axis == 16) ||
                      (k == 18 && remap_axis == 19) || (k == 19 && remap_axis == 18) ||
                      (k == 20 && remap_axis == 21) || (k == 21 && remap_axis == 20) ||
                      (k == 22 && remap_axis == 23) || (k == 23 && remap_axis == 22))*/
                  if ((k % 2 == 0 && remap_axis % 2 != 0) || (k % 2 != 0 && remap_axis % 2 == 0))
                     invert = -1;

                  handle->analog_value[i][remap_axis - RARCH_FIRST_CUSTOM_BIND] = current_axis_value * invert;
                  /* RARCH_LOG("axis %d(%d) remapped to axis %d val %d\n", j, k, remap_axis - RARCH_FIRST_CUSTOM_BIND, current_axis_value); */
               }
            }

         }
#endif
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
            *ret = 1;
         break;
      case RETRO_DEVICE_ANALOG:
         {
            int val = 0;
            if (idx == 0)
            {
               if (id == 0)
               {
                  if (handle->analog_value[port][0])
                     val = handle->analog_value[port][0];
                  else if (handle->analog_value[port][1])
                     val = handle->analog_value[port][1];

                  if(handle->analog_value[port][0] || handle->analog_value[port][1])
                  {
                     *ret = val;
                  }
               }
               if (id == 1)
               {
                  if (handle->analog_value[port][2])
                     val = handle->analog_value[port][2];
                  else if (handle->analog_value[port][3])
                     val = handle->analog_value[port][3];

                  if(handle->analog_value[port][2] || handle->analog_value[port][3])
                  {
                     *ret = val;
                  }
               }
               if (idx == 1)
               {
                  if (id == 0)
                  {
                     if (handle->analog_value[port][4])
                        val = handle->analog_value[port][4];
                     else if (handle->analog_value[port][5])
                        val = handle->analog_value[port][5];

                     if(handle->analog_value[port][4] || handle->analog_value[port][5])
                     {
                        *ret = val;
                     }
                  }
                  if (id == 1)
                  {
                     if (handle->analog_value[port][6])
                        val = handle->analog_value[port][6];
                     else if (handle->analog_value[port][7])
                        val = handle->analog_value[port][7];

                     if(handle->analog_value[port][6] || handle->analog_value[port][7])
                     {
                        *ret = val;
                     }
                  }
               }
            }
         }
         break;
      case RETRO_DEVICE_KEYBOARD:
         if (id < RETROK_LAST)
         {
            if (MAPPER_GET_KEY(handle, id))
               *ret |= 1;
         }
         break;
      default:
         break;
   }
   return;
}
