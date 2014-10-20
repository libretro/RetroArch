/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include <sdk_version.h>
#include "../boolean.h"

#include "../ps3/sdk_defines.h"

#include "../driver.h"
#include "../libretro.h"
#include "../general.h"

#ifdef HAVE_MOUSE
#ifndef __PSL1GHT__
#define MAX_MICE 7
#endif
#endif

#ifndef __PSL1GHT__
#define MAX_PADS 7
#endif

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
#endif
   const rarch_joypad_driver_t *joypad;
} ps3_input_t;

static void ps3_input_poll(void *data)
{
   ps3_input_t *ps3 = (ps3_input_t*)data;

   if (ps3 && ps3->joypad)
      ps3->joypad->poll();

#ifdef HAVE_MOUSE
   CellMouseInfo mouse_info;
   cellMouseGetInfo(&mouse_info);
   ps3->mice_connected = mouse_info.now_connect;
#endif
}

#ifdef HAVE_MOUSE
static int16_t ps3_mouse_device_state(ps3_input_t *ps3,
      unsigned player, unsigned id)
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

static int16_t ps3_input_state(void *data,
      const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   ps3_input_t *ps3 = (ps3_input_t*)data;

   if (!ps3)
      return 0;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         return input_joypad_pressed(ps3->joypad, port, binds[port], id);
      case RETRO_DEVICE_ANALOG:
         return input_joypad_analog(ps3->joypad, port, idx, id, binds[port]);
#if 0
      case RETRO_DEVICE_SENSOR_ACCELEROMETER:
         switch (id)
         {
            // fixed range of 0x000 - 0x3ff
            case RETRO_DEVICE_ID_SENSOR_ACCELEROMETER_X:
               retval = ps3->accelerometer_state[port].x;
               break;
            case RETRO_DEVICE_ID_SENSOR_ACCELEROMETER_Y:
               retval = ps3->accelerometer_state[port].y;
               break;
            case RETRO_DEVICE_ID_SENSOR_ACCELEROMETER_Z:
               retval = ps3->accelerometer_state[port].z;
               break;
            default:
               retval = 0;
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

   if (ps3->joypad)
      ps3->joypad->destroy();

#ifdef HAVE_MOUSE
   cellMouseEnd();
#endif
   free(data);
}


static void* ps3_input_init(void)
{
   ps3_input_t *ps3 = (ps3_input_t*)calloc(1, sizeof(*ps3));
   if (!ps3)
      return NULL;

   ps3->joypad = input_joypad_init_driver(g_settings.input.joypad_driver);

   if (ps3->joypad)
      ps3->joypad->init();

#ifdef HAVE_MOUSE
   cellMouseInit(MAX_MICE);
#endif
   return ps3;
}

static bool ps3_input_key_pressed(void *data, int key)
{
   ps3_input_t *ps3 = (ps3_input_t*)data;
   if (ps3)
      return (g_extern.lifecycle_state & (1ULL << key)) || 
         input_joypad_pressed(ps3->joypad, 0, g_settings.input.binds[0], key);
   return false;
}

static uint64_t ps3_input_get_capabilities(void *data)
{
   (void)data;
   return
#ifdef HAVE_MOUSE
      (1 << RETRO_DEVICE_MOUSE)  |
#endif
      (1 << RETRO_DEVICE_JOYPAD) |
      (1 << RETRO_DEVICE_ANALOG);
}

static bool ps3_input_set_sensor_state(void *data, unsigned port,
      enum retro_sensor_action action, unsigned event_rate)
{
   CellPadInfo2 pad_info;
   (void)event_rate;

   switch (action)
   {
      case RETRO_SENSOR_ACCELEROMETER_ENABLE:
         cellPadGetInfo2(&pad_info);
         if ((pad_info.device_capability[port] 
                  & CELL_PAD_CAPABILITY_SENSOR_MODE) 
               != CELL_PAD_CAPABILITY_SENSOR_MODE)
            return false;

         cellPadSetPortSetting(port, CELL_PAD_SETTING_SENSOR_ON);
         return true;
      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         cellPadSetPortSetting(port, 0);
         return true;

      default:
         return false;
   }
}

static bool ps3_input_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   ps3_input_t *ps3 = (ps3_input_t*)data;

   if (ps3 && ps3->joypad)
      return input_joypad_set_rumble(ps3->joypad,
            port, effect, strength);
   return false;
}

static const rarch_joypad_driver_t *ps3_input_get_joypad_driver(void *data)
{
   ps3_input_t *ps3 = (ps3_input_t*)data;
   if (ps3)
      return ps3->joypad;
   return NULL;
}

input_driver_t input_ps3 = {
   ps3_input_init,
   ps3_input_poll,
   ps3_input_state,
   ps3_input_key_pressed,
   ps3_input_free_input,
   ps3_input_set_sensor_state,
   NULL,
   ps3_input_get_capabilities,
   "ps3",

   NULL,
   ps3_input_set_rumble,
   ps3_input_get_joypad_driver,
};
