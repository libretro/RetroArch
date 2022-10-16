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

#ifndef __PSL1GHT__
#include <sdk_version.h>
#endif

#include <boolean.h>
#include <libretro.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <defines/ps3_defines.h>

#include "../input_driver.h"

#ifdef HAVE_MOUSE
#ifndef MAX_MICE
#define MAX_MICE 7
#endif
#endif

/* TODO/FIXME -
 * fix game focus toggle */

typedef struct
{
   float x;
   float y;
   float z;
} sensor_t;

typedef struct ps3_input
{
#ifdef HAVE_MOUSE
   unsigned mice_connected;
#else
   void *empty;
#endif
} ps3_input_t;

#ifdef HAVE_MOUSE
static void ps3_input_poll(void *data)
{
   CellMouseInfo mouse_info;
   ps3_input_t *ps3 = (ps3_input_t*)data;

   cellMouseGetInfo(&mouse_info);
   ps3->mice_connected = mouse_info.now_connect;
}

static int16_t ps3_mouse_device_state(ps3_input_t *ps3,
      unsigned user, unsigned id)
{
   CellMouseData mouse_state;
   cellMouseGetData(id, &mouse_state);

   if (!ps3->mice_connected)
      return 0;

   switch (id)
   {
      /* TODO: mouse wheel up/down */
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return (mouse_state.buttons & CELL_MOUSE_BUTTON_1);
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return (mouse_state.buttons & CELL_MOUSE_BUTTON_2);
      case RETRO_DEVICE_ID_MOUSE_X:
         return (mouse_state.x_axis);
      case RETRO_DEVICE_ID_MOUSE_Y:
         return (mouse_state.y_axis);
   }

   return 0;
}
#endif

static int16_t ps3_input_state(
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
   ps3_input_t *ps3           = (ps3_input_t*)data;

   if (!ps3)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
      case RETRO_DEVICE_ANALOG:
         break;
#if 0
      case RETRO_DEVICE_SENSOR_ACCELEROMETER:
         switch (id)
         {
            /* Fixed range of 0x000 - 0x3ff */
            case RETRO_DEVICE_ID_SENSOR_ACCELEROMETER_X:
               return ps3->accelerometer_state[port].x;
            case RETRO_DEVICE_ID_SENSOR_ACCELEROMETER_Y:
               return ps3->accelerometer_state[port].y;
            case RETRO_DEVICE_ID_SENSOR_ACCELEROMETER_Z:
               return ps3->accelerometer_state[port].z;
            default:
               break;
         }
         break;
#endif
#ifdef HAVE_MOUSE
      case RETRO_DEVICE_MOUSE:
         return ps3_mouse_device_state(data, port, id);
#endif
   }

   return 0;
}

static void ps3_input_free_input(void *data)
{
   ps3_input_t *ps3 = (ps3_input_t*)data;

   if (!ps3)
      return;

#ifdef HAVE_MOUSE
   cellMouseEnd();
#endif
   free(data);
}

static void* ps3_input_init(const char *joypad_driver)
{
   ps3_input_t *ps3 = (ps3_input_t*)calloc(1, sizeof(*ps3));
   if (!ps3)
      return NULL;

#ifdef HAVE_MOUSE
   cellMouseInit(MAX_MICE);
#endif

   return ps3;
}

static uint64_t ps3_input_get_capabilities(void *data)
{
   return
#ifdef HAVE_MOUSE
        (1 << RETRO_DEVICE_MOUSE)  |
#endif
        (1 << RETRO_DEVICE_JOYPAD)
      | (1 << RETRO_DEVICE_ANALOG);
}

static bool ps3_input_set_sensor_state(void *data, unsigned port,
      enum retro_sensor_action action, unsigned event_rate)
{
   CellPadInfo2 pad_info;

   switch (action)
   {
      case RETRO_SENSOR_ACCELEROMETER_ENABLE:
         cellPadGetInfo2(&pad_info);
         if ((pad_info.device_capability[port]
                  & CELL_PAD_CAPABILITY_SENSOR_MODE)
               == CELL_PAD_CAPABILITY_SENSOR_MODE)
         {
            cellPadSetPortSetting(port, CELL_PAD_SETTING_SENSOR_ON);
            return true;
         }
         break;
      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         cellPadSetPortSetting(port, 0);
         return true;
      default:
         break;
   }

   return false;
}

input_driver_t input_ps3 = {
   ps3_input_init,
#ifdef HAVE_MOUSE
   ps3_input_poll,
#else
   NULL,                         /* poll */
#endif
   ps3_input_state,
   ps3_input_free_input,
   ps3_input_set_sensor_state,
   NULL,
   ps3_input_get_capabilities,
   "ps3",

   NULL,                         /* grab_mouse */
   NULL,
   NULL
};
