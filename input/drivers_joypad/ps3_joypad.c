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
#include <retro_inline.h>

#include "../../config.def.h"

#include "../../tasks/tasks_internal.h"

/* TODO/FIXME - static globals */
static uint64_t pad_state[DEFAULT_MAX_PADS];
static int16_t analog_state[DEFAULT_MAX_PADS][2][2];
static uint64_t pads_connected[DEFAULT_MAX_PADS];
#if 0
sensor_t accelerometer_state[DEFAULT_MAX_PADS];
#endif

static INLINE int16_t convert_u8_to_s16(uint8_t val)
{
   if (val == 0)
      return -0x7fff;
   return val * 0x0101 - 0x8000;
}

static const char *ps3_joypad_name(unsigned pad)
{
   return "SixAxis Controller";
}

static void ps3_joypad_autodetect_add(unsigned autoconf_pad)
{
   input_autoconfigure_connect(
         ps3_joypad_name(autoconf_pad),
         NULL,
         ps3_joypad.ident,
         autoconf_pad,
         0,
         0
         );
}

static void *ps3_joypad_init(void *data)
{
   ioPadInit(DEFAULT_MAX_PADS);

   return (void*)-1;
}

static int32_t ps3_joypad_button(unsigned port, uint16_t joykey)
{
   if (port >= DEFAULT_MAX_PADS)
      return 0;
   return pad_state[port] & (UINT64_C(1) << joykey);
}

static void ps3_joypad_get_buttons(unsigned port_num, input_bits_t *state)
{
	if (port_num < DEFAULT_MAX_PADS)
   {
		BITS_COPY16_PTR( state, pad_state[port_num] );
	}
   else
		BIT256_CLEAR_ALL_PTR(state);
}

static int16_t ps3_joypad_axis_state(unsigned port, uint32_t joyaxis)
{
   int val     = 0;
   int axis    = -1;
   bool is_neg = false;
   bool is_pos = false;

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      axis   = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) < 4)
   {
      axis   = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }
   else
      return 0;

   switch (axis)
   {
      case 0:
      case 1:
         val = analog_state[port][0][axis];
         break;
      case 2:
      case 3:
         val = analog_state[port][1][axis-2];
         break;
   }

   if (is_neg && val > 0)
      return 0;
   else if (is_pos && val < 0)
      return 0;
   return val;
}

static int16_t ps3_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (port >= DEFAULT_MAX_PADS)
      return 0;
   return ps3_joypad_axis_state(port, joyaxis);
}

static int16_t ps3_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t ret                          = 0;
   uint16_t port_idx                    = joypad_info->joy_idx;

   if (port_idx >= DEFAULT_MAX_PADS)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if (
               (uint16_t)joykey != NO_BTN 
            && pad_state[port_idx] & (UINT64_C(1) << (uint16_t)joykey)
         )
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(ps3_joypad_axis_state(port_idx, joyaxis)) 
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static void ps3_joypad_poll(void)
{
   unsigned port;
   padInfo2 pad_info;

   ioPadGetInfo2(&pad_info);

   for (port = 0; port < DEFAULT_MAX_PADS; port++)
   {
      padData state_tmp;

      if (pad_info.port_status[port] & CELL_PAD_STATUS_ASSIGN_CHANGES)
      {
         if ( (pad_info.port_status[port] & CELL_PAD_STATUS_CONNECTED) == 0 )
         {
            input_autoconfigure_disconnect(port, ps3_joypad.ident);
            pads_connected[port] = 0;
         }
         else if ((pad_info.port_status[port] & CELL_PAD_STATUS_CONNECTED) > 0 )
         {
            pads_connected[port] = 1;
            ps3_joypad_autodetect_add(port);
         }
      }

      if (pads_connected[port] == 0)
         continue;

      ioPadGetData(port, &state_tmp);

      if (state_tmp.len != 0)
      {
         uint64_t *state_cur = &pad_state[port];
         *state_cur = 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_LEFT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_DOWN) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_RIGHT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_UP) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_START) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_R3) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R3) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_L3) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L3) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL1] & CELL_PAD_CTRL_SELECT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_TRIANGLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_X) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_SQUARE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_Y) : 0;

         if (menu_driver_is_alive())
         {
            int value = 0;
            if (cellSysutilGetSystemParamInt(CELL_SYSUTIL_SYSTEMPARAM_ID_ENTER_BUTTON_ASSIGN, &value) == 0)
            {
               if (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CROSS)
                  *state_cur |=  (value == CELL_SYSUTIL_ENTER_BUTTON_ASSIGN_CROSS) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_A) : (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_B);
               if (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CIRCLE)
                  *state_cur |=  (value == CELL_SYSUTIL_ENTER_BUTTON_ASSIGN_CIRCLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_A) : (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_B);
            }
         }
         else
         {
            *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CROSS) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_B) : 0;
            *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CIRCLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_A) : 0;
         }
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R1) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L1) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R2) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L2) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L2) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
         uint8_t lsx = (uint8_t)(state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X]);
         uint8_t lsy = (uint8_t)(state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y]);
         uint8_t rsx = (uint8_t)(state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X]);
         uint8_t rsy = (uint8_t)(state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y]);
         analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT ][RETRO_DEVICE_ID_ANALOG_X] = convert_u8_to_s16(lsx);
         analog_state[port][RETRO_DEVICE_INDEX_ANALOG_LEFT ][RETRO_DEVICE_ID_ANALOG_Y] = convert_u8_to_s16(lsy);
         analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = convert_u8_to_s16(rsx);
         analog_state[port][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = convert_u8_to_s16(rsy);

#if 0
         accelerometer_state[port].x = state_tmp.button[CELL_PAD_BTN_OFFSET_SENSOR_X];
         accelerometer_state[port].y = state_tmp.button[CELL_PAD_BTN_OFFSET_SENSOR_Y];
         accelerometer_state[port].z = state_tmp.button[CELL_PAD_BTN_OFFSET_SENSOR_Z];
#endif
      }

      for (int i = 0; i < 2; i++)
         for (int j = 0; j < 2; j++)
            if (analog_state[port][i][j] == -0x8000)
               analog_state[port][i][j] = -0x7fff;
   }
}

static bool ps3_joypad_query_pad(unsigned pad)
{
   return pad < MAX_USERS && pad_state[pad];
}

static bool ps3_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
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

   cellPadSetActDirect(pad, &params);

   return true;
}

static void ps3_joypad_destroy(void)
{
   ioPadEnd();
}

input_device_driver_t ps3_joypad = {
   ps3_joypad_init,
   ps3_joypad_query_pad,
   ps3_joypad_destroy,
   ps3_joypad_button,
   ps3_joypad_state,
   ps3_joypad_get_buttons,
   ps3_joypad_axis,
   ps3_joypad_poll,
   ps3_joypad_rumble,
   NULL,
   ps3_joypad_name,
   "ps3",
};
