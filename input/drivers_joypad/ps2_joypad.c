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

#include "libpad.h"

#define PS2_PAD_SLOT 0 /* Always zero if not using multitap */
#define PS2_ANALOG_STICKS 2
#define PS2_ANALOG_AXIS 2

static unsigned char padBuf[2][256] ALIGNED(64);

static uint64_t pad_state[DEFAULT_MAX_PADS];
static int16_t analog_state[DEFAULT_MAX_PADS][PS2_ANALOG_STICKS][PS2_ANALOG_AXIS];

static INLINE int16_t convert_u8_to_s16(uint8_t val)
{
   if (val == 0)
      return -0x7fff;
   return val * 0x0101 - 0x8000;
}

static bool is_analog_enabled(struct padButtonStatus buttons)
{
   bool enabled = false;

   if (buttons.ljoy_h || buttons.ljoy_v || buttons.rjoy_h || buttons.rjoy_v)
      enabled = true;

   return enabled;
}

static const char *ps2_joypad_name(unsigned pad)
{
   return "PS2 Controller";
}

static bool ps2_joypad_init(void *data)
{
   unsigned ret  = 0;
   unsigned port = 0;
   bool init     = true;

   printf("PortMax: %d\n", padGetPortMax());
   printf("SlotMax: %d\n", padGetSlotMax(port));

   for (port = 0; port < DEFAULT_MAX_PADS; port++)
   {
      input_autoconfigure_connect( ps2_joypad_name(port),
            NULL,
            ps2_joypad.ident,
            port,
            0,
            0);

      /* Port 0 -> Connector 1, Port 1 -> Connector 2 */
      if((ret = padPortOpen(port, PS2_PAD_SLOT, padBuf[port])) == 0)
      {
         printf("padOpenPort failed: %d\n", ret);
         init = false;
         break;
      }
   }

   return init;
}

static bool ps2_joypad_button(unsigned port_num, uint16_t joykey)
{
   if (port_num >= DEFAULT_MAX_PADS)
      return false;

   return (pad_state[port_num] & (UINT64_C(1) << joykey));
}

static void ps2_joypad_get_buttons(unsigned port_num, input_bits_t *state)
{
	BIT256_CLEAR_ALL_PTR(state);
}

static int16_t ps2_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   int val     = 0;
   int axis    = -1;
   bool is_neg = false;
   bool is_pos = false;

   if (joyaxis == AXIS_NONE || port_num >= DEFAULT_MAX_PADS)
      return 0;

   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      axis = AXIS_NEG_GET(joyaxis);
      is_neg = true;
   }
   else if (AXIS_POS_GET(joyaxis) < 4)
   {
      axis = AXIS_POS_GET(joyaxis);
      is_pos = true;
   }

   switch (axis)
   {
      case 0:
         val = analog_state[port_num][0][0];
         break;
      case 1:
         val = analog_state[port_num][0][1];
         break;
      case 2:
         val = analog_state[port_num][1][0];
         break;
      case 3:
         val = analog_state[port_num][1][1];
         break;
   }

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
}

static void ps2_joypad_poll(void)
{
   unsigned player;
   struct padButtonStatus buttons;

   for (player = 0; player < DEFAULT_MAX_PADS; player++)
   {
      int state = padGetState(player, PS2_PAD_SLOT);
      if (state == PAD_STATE_STABLE)
      {
         int ret = padRead(player, PS2_PAD_SLOT, &buttons); /* port, slot, buttons */
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
            if (is_analog_enabled(buttons))
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
   for (port = 0; port < DEFAULT_MAX_PADS; port++)
      padPortClose(port, PS2_PAD_SLOT);
}

input_device_driver_t ps2_joypad = {
   ps2_joypad_init,
   ps2_joypad_query_pad,
   ps2_joypad_destroy,
   ps2_joypad_button,
   ps2_joypad_get_buttons,
   ps2_joypad_axis,
   ps2_joypad_poll,
   ps2_joypad_rumble,
   ps2_joypad_name,
   "ps2",
};
