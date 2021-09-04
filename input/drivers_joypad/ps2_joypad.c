/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2018 - Francisco Javier Trujillo Mata - fjtrujy
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
#include <stddef.h>
#include <boolean.h>

#include "../../config.def.h"

#include "../input_driver.h"

#include "libmtap.h"
#include "libpad.h"

#define PS2_MAX_PORT 2 /* each ps2 has 2 ports */
#define PS2_MAX_SLOT 4 /* maximum - 4 slots in one multitap */
#define PS2_ANALOG_STICKS 2
#define PS2_ANALOG_AXIS 2

/* TODO/FIXME - static globals */
static unsigned char padBuf[PS2_MAX_PORT][PS2_MAX_SLOT][256] ALIGNED(64);
static uint64_t pad_state[DEFAULT_MAX_PADS];
static int16_t analog_state[DEFAULT_MAX_PADS][PS2_ANALOG_STICKS][PS2_ANALOG_AXIS];

static INLINE int16_t convert_u8_to_s16(uint8_t val)
{
   if (val == 0)
      return -0x7fff;
   return val * 0x0101 - 0x8000;
}

static const char *ps2_joypad_name(unsigned pad)
{
   return "PS2 Controller";
}

static void *ps2_joypad_init(void *data)
{
   unsigned ret  = 0;
   unsigned port = 0;
   unsigned slot = 0;
   unsigned pad  = 0;

   for (port = 0; port < PS2_MAX_PORT; port++)
      mtapPortOpen(port);
   /* it can fail - we dont care, we will check it more strictly when padPortOpen */

   for (slot = 0; slot < PS2_MAX_SLOT; slot++)
   {
      for (port = 0; port < PS2_MAX_PORT; port++)
      {
         input_autoconfigure_connect( ps2_joypad_name(pad), /* name */
            NULL,                                           /* display name */
            ps2_joypad.ident,                               /* driver */
            pad,                                            /* idx */
            0,                                              /* vid */
            0);                                             /* pid */
         pad++;
         /* 2 main controller ports acts the same with and without multitap
            Port 0,0 -> Connector 1 - the same as Port 0
            Port 1,0 -> Connector 2 - the same as Port 1
            Port 0,1 -> Connector 3
            Port 1,1 -> Connector 4
            Port 0,2 -> Connector 5
            Port 1,2 -> Connector 6
            Port 0,3 -> Connector 7
            Port 1,3 -> Connector 8
          */

         if((ret = padPortOpen(port, slot, padBuf[port][slot])) == 0)
            return NULL;
      }
   }
   return (void*)-1;
}

static int32_t ps2_joypad_button(unsigned port, uint16_t joykey)
{
   if (port >= DEFAULT_MAX_PADS)
      return 0;
   return pad_state[port] & (UINT64_C(1) << joykey);
}

static int16_t ps2_joypad_axis_state(unsigned port_num, uint32_t joyaxis)
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
         val = analog_state[port_num][0][axis];
         break;
      case 2:
      case 3:
         val = analog_state[port_num][1][axis-2];
         break;
   }

   if (is_neg && val > 0)
      return 0;
   else if (is_pos && val < 0)
      return 0;
   return val;
}

static int16_t ps2_joypad_state(
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
            && pad_state[port_idx] & (UINT64_C(1) << joykey)
         )
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(ps2_joypad_axis_state(port_idx, joyaxis))
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static int16_t ps2_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   if (port_num >= DEFAULT_MAX_PADS)
      return 0;
   return ps2_joypad_axis_state(port_num, joyaxis);
}

static void ps2_joypad_get_buttons(unsigned port_num, input_bits_t *state)
{
   BIT256_CLEAR_ALL_PTR(state);
}

static void ps2_joypad_poll(void)
{
   unsigned player;
   struct padButtonStatus buttons;

   for (player = 0; player < DEFAULT_MAX_PADS; player++)
   {
      int ps2_slot = player >> 1;
      int ps2_port = player & 0x1;

      int state = padGetState(ps2_port, ps2_slot);
      if (state != PAD_STATE_DISCONN || state != PAD_STATE_EXECCMD || state != PAD_STATE_ERROR)
      {
         int ret = padRead(ps2_port, ps2_slot, &buttons); /* port, slot, buttons */
         if (ret != 0)
         {
            int32_t state_tmp = 0xffff ^ buttons.btns;
            pad_state[player] = 0;

            pad_state[player] |= (state_tmp & PAD_LEFT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
            pad_state[player] |= (state_tmp & PAD_DOWN) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
            pad_state[player] |= (state_tmp & PAD_RIGHT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
            pad_state[player] |= (state_tmp & PAD_UP) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
            pad_state[player] |= (state_tmp & PAD_START) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START) : 0;
            pad_state[player] |= (state_tmp & PAD_SELECT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
            pad_state[player] |= (state_tmp & PAD_TRIANGLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_X) : 0;
            pad_state[player] |= (state_tmp & PAD_SQUARE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
            pad_state[player] |= (state_tmp & PAD_CROSS) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_B) : 0;
            pad_state[player] |= (state_tmp & PAD_CIRCLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_A) : 0;
            pad_state[player] |= (state_tmp & PAD_R1) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R) : 0;
            pad_state[player] |= (state_tmp & PAD_L1) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L) : 0;
            pad_state[player] |= (state_tmp & PAD_R2) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
            pad_state[player] |= (state_tmp & PAD_L2) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
            pad_state[player] |= (state_tmp & PAD_R3) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R3) : 0;
            pad_state[player] |= (state_tmp & PAD_L3) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L3) : 0;

            /* Analog */
            if (buttons.ljoy_h || buttons.ljoy_v || buttons.rjoy_h || buttons.rjoy_v)
            {
               analog_state[player][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_X] = convert_u8_to_s16(buttons.ljoy_h);
               analog_state[player][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_Y] = convert_u8_to_s16(buttons.ljoy_v);;
               analog_state[player][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = convert_u8_to_s16(buttons.rjoy_h);;
               analog_state[player][RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = convert_u8_to_s16(buttons.rjoy_v);;
            }

         }
      }
   }

}

static bool ps2_joypad_query_pad(unsigned pad)
{
   return pad < DEFAULT_MAX_PADS && pad_state[pad];
}

static bool ps2_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   return false;
}

static void ps2_joypad_destroy(void)
{
   unsigned port;
   unsigned slot;
   for (port = 0; port < PS2_MAX_PORT; port++)
   {
      for (slot = 0; slot < PS2_MAX_SLOT; slot++)
         padPortClose(port, slot);
      mtapPortClose(port);
   }
}

input_device_driver_t ps2_joypad = {
   ps2_joypad_init,
   ps2_joypad_query_pad,
   ps2_joypad_destroy,
   ps2_joypad_button,
   ps2_joypad_state,
   ps2_joypad_get_buttons,
   ps2_joypad_axis,
   ps2_joypad_poll,
   ps2_joypad_rumble,
   NULL,
   ps2_joypad_name,
   "ps2",
};
