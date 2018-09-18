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
#include <string.h>

#include "../input_driver.h"

#include "../../tasks/tasks_internal.h"

#include "../../configuration.h"

#include "../../defines/ps2_defines.h"

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#define PS2_MAX_PADS 1

static uint64_t pad_state[PS2_MAX_PADS];
static int16_t analog_state[PS2_MAX_PADS][2][2];

extern uint64_t lifecycle_state;

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
      if (!input_autoconfigure_connect(
               ps2_joypad_name(i),
               NULL,
               ps2_joypad.ident,
               i,
               0,
               0
               ))
         input_config_set_device_name(i, ps2_joypad_name(i));
   }

   return true;
}

static bool ps2_joypad_button(unsigned port_num, uint16_t key)
{
   if (port_num >= PS2_MAX_PADS)
      return false;

   return (pad_state[port_num] & (UINT64_C(1) << key));
}

static void ps2_joypad_get_buttons(unsigned port_num, input_bits_t *state)
{
	if (port_num < PS2_MAX_PADS)
   {
		BITS_COPY16_PTR( state, pad_state[port_num] );
	}
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t ps2_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   int    val  = 0;
   int    axis = -1;
   bool is_neg = false;
   bool is_pos = false;

   if (joyaxis == AXIS_NONE || port_num >= PS2_MAX_PADS)
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
   unsigned players_count = PS2_MAX_PADS;

   BIT64_CLEAR(lifecycle_state, RARCH_MENU_TOGGLE);

   for (player = 0; player < players_count; player++)
   {
      unsigned j, k;
      unsigned i  = player;
      unsigned p  = player;
      int32_t state_tmp = padGetState(player, player);
      

      pad_state[i] = 0;
      analog_state[i][0][0] = analog_state[i][0][1] =
         analog_state[i][1][0] = analog_state[i][1][1] = 0;

#ifdef HAVE_KERNEL_PRX
      state_tmp.Buttons = (state_tmp.Buttons & 0x0000FFFF)
         | (read_system_buttons() & 0xFFFF0000);
#endif

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

#ifdef HAVE_KERNEL_PRX
      if (state_tmp & PS2_CTRL_NOTE)
         BIT64_SET(lifecycle_state, RARCH_MENU_TOGGLE);
#endif

      for (j = 0; j < 2; j++)
         for (k = 0; k < 2; k++)
            if (analog_state[i][j][k] == -0x8000)
               analog_state[i][j][k] = -0x7fff;
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
