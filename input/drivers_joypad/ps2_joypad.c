/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2013-2014 - CatalystG
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

#define PS2_MAX_PADS 1

/*
 * Global var's
 */
// pad_dma_buf is provided by the user, one buf for each pad
// contains the pad's current state
static char padBuf[256] __attribute__((aligned(64)));

static uint64_t pad_state[PS2_MAX_PADS];


static const char *ps2_joypad_name(unsigned pad)
{
   return "PS2 Controller";
}

static bool ps2_joypad_init(void *data)
{
   unsigned i;
   unsigned players_count = PS2_MAX_PADS;

   for (i = 0; i < players_count; i++)
   {
      bool auto_configure = input_autoconfigure_connect( ps2_joypad_name(i),
                                                         NULL,
                                                         ps2_joypad.ident,
                                                         i,
                                                         0,
                                                         0);
      if (!auto_configure) {
         input_config_set_device_name(i, ps2_joypad_name(i));
      }

      padInit(i);

      int ret;
      int port, slot;

      port = 0; // 0 -> Connector 1, 1 -> Connector 2
      slot = 0; // Always zero if not using multitap

      printf("PortMax: %d\n", padGetPortMax());
      printf("SlotMax: %d\n", padGetSlotMax(port));


      if((ret = padPortOpen(port, slot, padBuf)) == 0) {
         printf("padOpenPort failed: %d\n", ret);
      }


   }

   return true;
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
   unsigned players_count = PS2_MAX_PADS;
   struct padButtonStatus buttons;

   for (player = 0; player < players_count; player++)
   {
      unsigned j, k;
      unsigned i  = player;
      unsigned p  = player;
   
      int ret = padRead(player, player, &buttons); // port, slot, buttons
      if (ret != 0)
      {
         int32_t state_tmp = 0xffff ^ buttons.btns;

         pad_state[i] = 0;

         pad_state[i] |= (state_tmp & PAD_LEFT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
         pad_state[i] |= (state_tmp & PAD_DOWN) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
         pad_state[i] |= (state_tmp & PAD_RIGHT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
         pad_state[i] |= (state_tmp & PAD_UP) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
         pad_state[i] |= (state_tmp & PAD_START) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START) : 0;
         pad_state[i] |= (state_tmp & PAD_SELECT) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
         pad_state[i] |= (state_tmp & PAD_TRIANGLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_X) : 0;
         pad_state[i] |= (state_tmp & PAD_SQUARE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
         pad_state[i] |= (state_tmp & PAD_CROSS) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_B) : 0;
         pad_state[i] |= (state_tmp & PAD_CIRCLE) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_A) : 0;
         pad_state[i] |= (state_tmp & PAD_R1) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R) : 0;
         pad_state[i] |= (state_tmp & PAD_L1) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L) : 0;
         pad_state[i] |= (state_tmp & PAD_R2) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
         pad_state[i] |= (state_tmp & PAD_L2) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L2) : 0;
         pad_state[i] |= (state_tmp & PAD_R3) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R3) : 0;
         pad_state[i] |= (state_tmp & PAD_L3) ? (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L3) : 0;
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
