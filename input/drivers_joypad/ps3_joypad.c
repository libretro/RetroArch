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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../config.def.h"

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#include "../../tasks/tasks_internal.h"
#include "../include/input_pad_view.h"

/* TODO/FIXME - static globals */
/* each pad as its poll left it, lent to the readers (input_pad_view.h):
 * the buttons as RetroPad ids, axes 0-3 the sticks' x and y */
static input_pad_view_t ps3_pads[DEFAULT_MAX_PADS];
static uint64_t pads_connected[DEFAULT_MAX_PADS];
static uint8_t  rumble_motor[DEFAULT_MAX_PADS][2];
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
         NULL, NULL,
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
   return input_pad_view_button(&ps3_pads[port], joykey);
}

static void ps3_joypad_get_buttons(unsigned port_num, input_bits_t *state)
{
   if (port_num < DEFAULT_MAX_PADS)
      *state = ps3_pads[port_num].buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t ps3_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (port >= DEFAULT_MAX_PADS)
      return 0;
   return input_pad_view_axis(&ps3_pads[port], joyaxis);
}

static int16_t ps3_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const uint16_t *joykeys,
      const uint32_t *joyaxes,
      unsigned port)
{
   uint16_t port_idx = joypad_info->joy_idx;
   if (port_idx >= DEFAULT_MAX_PADS)
      return 0;
   return input_pad_view_state(&ps3_pads[port_idx], joypad_info,
         joykeys, joyaxes);
}

/* Read in the poll alone: the same until the next */
static const input_pad_view_t *ps3_joypad_view(unsigned port)
{
   return (port < DEFAULT_MAX_PADS) ? &ps3_pads[port] : NULL;
}

static void ps3_joypad_poll(void)
{
   unsigned port;
   padInfo2 pad_info;

   ioPadGetInfo2(&pad_info);

   for (port = 0; port < DEFAULT_MAX_PADS; port++)
   {
      int i;
      padData state_tmp;
      input_pad_view_t *v = &ps3_pads[port];

      if (pad_info.port_status[port] & CELL_PAD_STATUS_ASSIGN_CHANGES)
      {
         if ( (pad_info.port_status[port] & CELL_PAD_STATUS_CONNECTED) == 0 )
         {
            input_autoconfigure_disconnect(port, ps3_joypad.ident);
            pads_connected[port] = 0;
            rumble_motor[port][0] = 0;
            rumble_motor[port][1] = 0;
         }
         else if ((pad_info.port_status[port] & CELL_PAD_STATUS_CONNECTED) > 0 )
         {
            pads_connected[port] = 1;
            ps3_joypad_autodetect_add(port);
         }
      }

      if (pads_connected[port] == 0)
      {
         /* Nothing held and sticks centred until the pad returns. */
         INPUT_PAD_VIEW_SET_BITS64(v, 0);
         for (i = 0; i < 4; i++)
            v->axes[i] = 0;
         continue;
      }

      ioPadGetData(port, &state_tmp);

      if (state_tmp.len != 0)
      {
         uint64_t bits       = 0;
         uint64_t *state_cur = &bits;
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

#ifdef HAVE_MENU
         if (menu_driver_alive())
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
#endif
         {
            *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CROSS) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_B) : 0;
            *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_CIRCLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_A) : 0;
         }
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R1) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L1) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_R2) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
         *state_cur |= (state_tmp.button[CELL_PAD_BTN_OFFSET_DIGITAL2] & CELL_PAD_CTRL_L2) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
         INPUT_PAD_VIEW_SET_BITS64(v, bits);
         v->axes[0] = convert_u8_to_s16((uint8_t)state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X]);
         v->axes[1] = convert_u8_to_s16((uint8_t)state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y]);
         v->axes[2] = convert_u8_to_s16((uint8_t)state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X]);
         v->axes[3] = convert_u8_to_s16((uint8_t)state_tmp.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y]);
      }

      for (i = 0; i < 4; i++)
         if (v->axes[i] == -0x8000)
            v->axes[i] = -0x7fff;
   }
}

static bool ps3_joypad_query_pad(unsigned pad)
{
   return pad < DEFAULT_MAX_PADS && pads_connected[pad];
}

static bool ps3_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   int i;
   CellPadActParam params;

   if (pad >= DEFAULT_MAX_PADS)
      return false;

   switch (effect)
   {
      case RETRO_RUMBLE_WEAK:
         /* The small motor is on or off. */
         rumble_motor[pad][0] = strength ? 1 : 0;
         break;
      case RETRO_RUMBLE_STRONG:
         rumble_motor[pad][1] = (uint8_t)(strength >> 8);
         break;
      default:
         return false;
   }

   /* Both motors are set at once: the other keeps its last value. */
   params.motor[0] = rumble_motor[pad][0];
   params.motor[1] = rumble_motor[pad][1];
   for (i = 0; i < (int)sizeof(params.reserved); i++)
      params.reserved[i] = 0;

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
   NULL, /* set_rumble_gain */
   NULL, /* set_sensor_state */
   NULL, /* get_sensor_input */
   ps3_joypad_name,
   "ps3",
   NULL, /* get_bt_host */
   NULL, /* set_bt_host */
   NULL, /* survives_video */
   NULL, /* set_player */
   ps3_joypad_view
};
