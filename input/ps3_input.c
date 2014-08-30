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
   uint64_t pad_state[MAX_PADS];
   int16_t analog_state[MAX_PADS][2][2];
   unsigned pads_connected;
#ifdef HAVE_MOUSE
   unsigned mice_connected;
#endif
   sensor_t accelerometer_state[MAX_PADS];
   const rarch_joypad_driver_t *joypad;
} ps3_input_t;

static inline int16_t convert_u8_to_s16(uint8_t val)
{
   if (val == 0)
      return -0x7fff;
   else
      return val * 0x0101 - 0x8000;
}

static void ps3_input_poll(void *data)
{
   CellPadInfo2 pad_info;
   ps3_input_t *ps3 = (ps3_input_t*)data;
   uint64_t *lifecycle_state = (uint64_t*)&g_extern.lifecycle_state;

   if (!ps3)
      return;

   for (unsigned port = 0; port < MAX_PADS; port++)
   {
      static CellPadData state_tmp;
      cellPadGetData(port, &state_tmp);

      if (state_tmp.len != 0)
      {
         uint64_t *state_cur = &ps3->pad_state[port];
         *state_cur = 0;
#ifdef __PSL1GHT__
         *state_cur |= (state_tmp.BTN_LEFT)     ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
         *state_cur |= (state_tmp.BTN_DOWN)     ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
         *state_cur |= (state_tmp.BTN_RIGHT)    ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
         *state_cur |= (state_tmp.BTN_UP)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
         *state_cur |= (state_tmp.BTN_START)    ? (1ULL << RETRO_DEVICE_ID_JOYPAD_START) : 0;
         *state_cur |= (state_tmp.BTN_R3)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R3) : 0;
         *state_cur |= (state_tmp.BTN_L3)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L3) : 0;
         *state_cur |= (state_tmp.BTN_SELECT)   ? (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
         *state_cur |= (state_tmp.BTN_TRIANGLE) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_X) : 0;
         *state_cur |= (state_tmp.BTN_SQUARE)   ? (1ULL << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
         *state_cur |= (state_tmp.BTN_CROSS)    ? (1ULL << RETRO_DEVICE_ID_JOYPAD_B) : 0;
         *state_cur |= (state_tmp.BTN_CIRCLE)   ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A) : 0;
         *state_cur |= (state_tmp.BTN_R1)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R) : 0;
         *state_cur |= (state_tmp.BTN_L1)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L) : 0;
         *state_cur |= (state_tmp.BTN_R2)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
         *state_cur |= (state_tmp.BTN_L2)       ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
#else
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_LEFT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_DOWN) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_RIGHT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_UP) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_START) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_START) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_R3) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R3) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_L3) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L3) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_SELECT) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_TRIANGLE) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_X) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_SQUARE) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_Y) : 0;

         if (*lifecycle_state & (1ULL << MODE_MENU))
         {
            int value = 0;
            if (cellSysutilGetSystemParamInt(CELL_SYSUTIL_SYSTEMPARAM_ID_ENTER_BUTTON_ASSIGN, &value) == 0)
            {
               if (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CROSS)
                  *state_cur |=  (value == CELL_SYSUTIL_ENTER_BUTTON_ASSIGN_CROSS) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A) : (1ULL << RETRO_DEVICE_ID_JOYPAD_B);
               if (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CIRCLE)
                  *state_cur |=  (value == CELL_SYSUTIL_ENTER_BUTTON_ASSIGN_CIRCLE) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A) : (1ULL << RETRO_DEVICE_ID_JOYPAD_B);
            }
         }
         else
         {
            *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CROSS) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_B) : 0;
            *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CIRCLE) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_A) : 0;
         }
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R1) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L1) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R2) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L2) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L2) ? (1ULL << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
         //RARCH_LOG("lsx : %d (%hd) lsy : %d (%hd) rsx : %d (%hd) rsy : %d (%hd)\n", lsx, ls_x, lsy, ls_y, rsx, rs_x, rsy, rs_y);
         uint8_t lsx = (uint8_t)(state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X]);
         uint8_t lsy = (uint8_t)(state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y]);
         uint8_t rsx = (uint8_t)(state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X]);
         uint8_t rsy = (uint8_t)(state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y]);
         ps3->analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT ][RETRO_DEVICE_ID_ANALOG_X] = convert_u8_to_s16(lsx);
         ps3->analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT ][RETRO_DEVICE_ID_ANALOG_Y] = convert_u8_to_s16(lsy);
         ps3->analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = convert_u8_to_s16(rsx);
         ps3->analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = convert_u8_to_s16(rsy);

         ps3->accelerometer_state[port].x = state_tmp.button[CELL_PAD_BTN_OFFSET_SENSOR_X];
         ps3->accelerometer_state[port].y = state_tmp.button[CELL_PAD_BTN_OFFSET_SENSOR_Y];
         ps3->accelerometer_state[port].z = state_tmp.button[CELL_PAD_BTN_OFFSET_SENSOR_Z];
#endif
      }

      for (int i = 0; i < 2; i++)
         for (int j = 0; j < 2; j++)
            if (ps3->analog_state[port][i][j] == -0x8000)
               ps3->analog_state[port][i][j] = -0x7fff;
   }

   uint64_t *state_p1 = &ps3->pad_state[0];

   *lifecycle_state &= ~((1ULL << RARCH_MENU_TOGGLE));

   if ((*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_L3)) && (*state_p1 & (1ULL << RETRO_DEVICE_ID_JOYPAD_R3)))
      *lifecycle_state |= (1ULL << RARCH_MENU_TOGGLE);

   cellPadGetInfo2(&pad_info);
   ps3->pads_connected = pad_info.now_connect; 

#ifdef HAVE_MOUSE
   CellMouseInfo mouse_info;
   cellMouseGetInfo(&mouse_info);
   ps3->mice_connected = mouse_info.now_connect;
#endif
}

#ifdef HAVE_MOUSE

static int16_t ps3_mouse_device_state(void *data, unsigned player, unsigned id)
{
   CellMouseData mouse_state;
   ps3_input_t *ps3 = (ps3_input_t*)data;
   cellMouseGetData(id, &mouse_state);

   if (!ps3)
      return 0;

   switch (id)
   {
      /* TODO: mouse wheel up/down */
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return (!ps3->mice_connected ? 0 : mouse_state.buttons & CELL_MOUSE_BUTTON_1);
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return (!ps3->mice_connected ? 0 : mouse_state.buttons & CELL_MOUSE_BUTTON_2);
      case RETRO_DEVICE_ID_MOUSE_X:
         return (!ps3->mice_connected ? 0 : mouse_state.x_axis);
      case RETRO_DEVICE_ID_MOUSE_Y:
         return (!ps3->mice_connected ? 0 : mouse_state.y_axis);
      default:
         return 0;
   }
}

#endif

static int16_t ps3_input_state(void *data, const struct retro_keybind **binds,
      unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   ps3_input_t *ps3 = (ps3_input_t*)data;
   int16_t retval = 0;

   if (!ps3)
      return 0;

   if (port < ps3->pads_connected)
   {
      switch (device)
      {
         case RETRO_DEVICE_JOYPAD:
            return input_joypad_pressed(ps3->joypad, port, binds[port], id);
         case RETRO_DEVICE_ANALOG:
            return input_joypad_analog(ps3->joypad, port, index, id, binds[port]);
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
            retval = ps3_mouse_device_state(data, port, id);
            break;
#endif
         default:
            return 0;
      }
   }

   return retval;
}

static void ps3_input_free_input(void *data)
{
   ps3_input_t *ps3 = (ps3_input_t*)data;
   if (!ps3)
      return;

   if (ps3->joypad)
      ps3->joypad->destroy();

   cellPadEnd();
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

   cellPadInit(MAX_PADS);
#ifdef HAVE_MOUSE
   cellMouseInit(MAX_MICE);
#endif

   ps3->joypad = input_joypad_init_driver(g_settings.input.joypad_driver);
   return ps3;
}

static bool ps3_input_key_pressed(void *data, int key)
{
   ps3_input_t *ps3 = (ps3_input_t*)data;
   return (g_extern.lifecycle_state & (1ULL << key)) || input_joypad_pressed(ps3->joypad, 0, g_settings.input.binds[0], key);
}

static uint64_t ps3_input_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);
#ifdef HAVE_MOUSE
   caps |= (1 << RETRO_DEVICE_MOUSE);
#endif
   caps |= (1 << RETRO_DEVICE_ANALOG);

   return caps;
}

static bool ps3_input_set_sensor_state(void *data, unsigned port, enum retro_sensor_action action, unsigned event_rate)
{
   CellPadInfo2 pad_info;
   (void)event_rate;

   switch (action)
   {
      case RETRO_SENSOR_ACCELEROMETER_ENABLE:
         cellPadGetInfo2(&pad_info);
         if ((pad_info.device_capability[port] & CELL_PAD_CAPABILITY_SENSOR_MODE) != CELL_PAD_CAPABILITY_SENSOR_MODE)
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

static bool ps3_input_set_rumble(void *data, unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
   CellPadActParam params;

   switch (effect)
   {
      case RETRO_RUMBLE_WEAK:
         if (strength > 1)
            strength = 1;
         params.motor[0] = strength;
         break;
      case RETRO_RUMBLE_STRONG:
         if (strength > 255)
            strength = 255;
         params.motor[1] = strength;
         break;
   }

   cellPadSetActDirect(port, &params);

   return true;
}

static const rarch_joypad_driver_t *ps3_input_get_joypad_driver(void *data)
{
   ps3_input_t *ps3 = (ps3_input_t*)data;
   return ps3->joypad;
}

const input_driver_t input_ps3 = {
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
