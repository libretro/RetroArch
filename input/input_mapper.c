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
   /* The controller port that will be polled*/
   uint8_t port;
   /* This is a bitmask of (1 << key_bind_id). */
   uint64_t buttons;
   /* Left X, Left Y, Right X, Right Y */
   int16_t analog[4];
   /* the whole keyboard state */
   uint32_t keys[RETROK_LAST / 32 + 1];
};

static input_mapper_t *mapper_ptr;

input_mapper_t *input_mapper_new(uint16_t port)
{
   settings_t *settings = config_get_ptr();
   input_mapper_t* handle = (input_mapper_t*)
      calloc(1, sizeof(*handle));

   if (!handle)
      return NULL;

   /* testing values for mgs */
   
   settings->uints.input_keymapper_ids[0] = RETROK_n;
   settings->uints.input_keymapper_ids[1] = RETROK_SPACE;
   settings->uints.input_keymapper_ids[2] = RETROK_F1;
   settings->uints.input_keymapper_ids[3] = RETROK_RETURN;
   settings->uints.input_keymapper_ids[4] = RETROK_UP;
   settings->uints.input_keymapper_ids[5] = RETROK_DOWN;
   settings->uints.input_keymapper_ids[6] = RETROK_LEFT;
   settings->uints.input_keymapper_ids[7] = RETROK_RIGHT;
   settings->uints.input_keymapper_ids[8] = RETROK_F1;
   settings->uints.input_keymapper_ids[9] = RETROK_F2;
   settings->uints.input_keymapper_ids[10] = RETROK_F3;
   settings->uints.input_keymapper_ids[11] = RETROK_F4;
   settings->uints.input_keymapper_ids[12] = RETROK_F5;
   settings->uints.input_keymapper_ids[13] = RETROK_F6;
   settings->uints.input_keymapper_ids[14] = RETROK_F7;
   settings->uints.input_keymapper_ids[15] = RETROK_F8;
   
   /* testing values for keen5 */
   /*settings->uints.input_keymapper_ids[0] = RETROK_LCTRL;
   settings->uints.input_keymapper_ids[1] = RETROK_SPACE;
   settings->uints.input_keymapper_ids[2] = RETROK_ESCAPE;
   settings->uints.input_keymapper_ids[3] = RETROK_RETURN;
   settings->uints.input_keymapper_ids[4] = RETROK_UP;
   settings->uints.input_keymapper_ids[5] = RETROK_DOWN;
   settings->uints.input_keymapper_ids[6] = RETROK_LEFT;
   settings->uints.input_keymapper_ids[7] = RETROK_RIGHT;
   settings->uints.input_keymapper_ids[8] = RETROK_F1;
   */
   handle->port = port;
   mapper_ptr = handle;
   return handle;
}

void input_mapper_free(input_mapper_t *handle)
{
   free (handle);
}

void input_mapper_poll(input_mapper_t *handle)
{
   settings_t *settings = config_get_ptr();
   unsigned device = settings->uints.input_libretro_device[handle->port];
   device &= RETRO_DEVICE_MASK;

   /* for now we only handle keyboard inputs */
   if (device == RETRO_DEVICE_KEYBOARD)
   {
      memset(handle->keys, 0, sizeof(handle->keys));

      for (int i = 0; i < RARCH_CUSTOM_BIND_LIST_END; i++)
      {
         if(i < RETROK_LAST)
         {
            if (input_state(0, RETRO_DEVICE_JOYPAD, handle->port, i))
            {
               MAPPER_SET_KEY (handle, settings->uints.input_keymapper_ids[i]);
               input_keyboard_event(true, settings->uints.input_keymapper_ids[i], 0, 0, RETRO_DEVICE_KEYBOARD);
            }
            else
            {
               input_keyboard_event(false, settings->uints.input_keymapper_ids[i], 0, 0, RETRO_DEVICE_KEYBOARD);
            }
               
         }
      }
   }
   return;
}

void input_mapper_state(
      int16_t *ret,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{

   settings_t *settings = config_get_ptr();
   switch (device)
   {
      case RETRO_DEVICE_KEYBOARD:
         if (id < RETROK_LAST)
         {
            /*
            RARCH_LOG("State: UDLR %u %u %u %u\n", 
               MAPPER_GET_KEY(mapper_ptr, RETROK_UP), 
               MAPPER_GET_KEY(mapper_ptr, RETROK_DOWN), 
               MAPPER_GET_KEY(mapper_ptr, RETROK_LEFT),
               MAPPER_GET_KEY(mapper_ptr, RETROK_RIGHT)
            );*/

            if (MAPPER_GET_KEY(mapper_ptr, id))
               *ret |= 1;
         }
         break;
   }
   
   return;
}