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

#include "../input_driver.h"

#include "libpad.h"

#define PS2_MAX_PADS 2
#define PS2_PAD_SLOT 0 /* Always zero if not using multitap */

static unsigned char padBuf[2][256] ALIGNED(64);

static uint64_t pad_state[PS2_MAX_PADS];

extern uint64_t lifecycle_state;

static const char *ps2_joypad_name(unsigned pad)
{
   return "PS2 Controller";
}

static bool ps2_joypad_init(void *data)
{
   unsigned ret, port;
   bool init = true;

   printf("PortMax: %d\n", padGetPortMax());
   printf("SlotMax: %d\n", padGetSlotMax(port));

   for (port = 0; port < PS2_MAX_PADS; port++) {
      bool auto_configure = input_autoconfigure_connect( ps2_joypad_name(port),
                                                         NULL,
                                                         ps2_joypad.ident,
                                                         port,
                                                         0,
                                                         0);
      if (!auto_configure) {
         input_config_set_device_name(port, ps2_joypad_name(port));
      }

      /* Port 0 -> Connector 1, Port 1 -> Connector 2 */
      if((ret = padPortOpen(port, PS2_PAD_SLOT, padBuf[port])) == 0) {
         printf("padOpenPort failed: %d\n", ret);
         init = false;
         break;
      }
   }

   return init;
}

static bool ps2_joypad_button(unsigned port_num, uint16_t joykey)
{
   if (port_num >= PS2_MAX_PADS)
      return false;

   return (pad_state[port_num] & (UINT64_C(1) << joykey));
}

static void ps2_joypad_get_buttons(unsigned port_num, input_bits_t *state)
{
	BIT256_CLEAR_ALL_PTR(state);
}

static int16_t ps2_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   return 0;
}

static void ps2_joypad_poll(void)
{
   unsigned player;
   struct padButtonStatus buttons;

   for (player = 0; player < PS2_MAX_PADS; player++) {
      int state = padGetState(player, PS2_PAD_SLOT);
      if (state == PAD_STATE_STABLE) {
         int ret = padRead(player, PS2_PAD_SLOT, &buttons); /* port, slot, buttons */
         if (ret != 0) {
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
            
         }
      }
   }

}

static bool ps2_joypad_query_pad(unsigned pad)
{
   return pad < PS2_MAX_PADS && pad_state[pad];
}

static bool ps2_joypad_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   return false;
}

static void ps2_joypad_destroy(void)
{
   unsigned port;
   for (port = 0; port < PS2_MAX_PADS; port++) {
      padPortClose(port, PS2_PAD_SLOT);
   }
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
