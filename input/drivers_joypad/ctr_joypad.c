/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Ali Bouhlel
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../config.def.h"


#include "../../tasks/tasks_internal.h"

#include "../../retroarch.h"
#include "../../command.h"
#include "string.h"
#include "3ds.h"

/* TODO/FIXME - static globals */
static uint32_t pad_state;
static int16_t analog_state[DEFAULT_MAX_PADS][2][2];

static const char *ctr_joypad_name(unsigned pad)
{
   return "3DS Controller";
}

static void ctr_joypad_autodetect_add(unsigned autoconf_pad)
{
   input_autoconfigure_connect(
         ctr_joypad_name(autoconf_pad),
         NULL,
         ctr_joypad.ident,
         autoconf_pad,
         0,
         0
         );
}

static void *ctr_joypad_init(void *data)
{
   ctr_joypad_autodetect_add(0);
   return (void*)-1;
}

static int32_t ctr_joypad_button(unsigned port_num, uint16_t joykey)
{
   if (port_num >= DEFAULT_MAX_PADS)
      return 0;
   return (pad_state & (1 << joykey));
}

static void ctr_joypad_get_buttons(unsigned port_num, input_bits_t *state)
{
	if (port_num < DEFAULT_MAX_PADS)
   {
		BITS_COPY16_PTR( state, pad_state );
	}
   else
		BIT256_CLEAR_ALL_PTR(state);
}

static int16_t ctr_joypad_axis_state(unsigned port_num, uint32_t joyaxis)
{
   int    val  = 0;
   int    axis = -1;
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
         val = analog_state[port_num][1][axis - 2];
         break;
   }

   if (is_neg && val > 0)
      return 0;
   else if (is_pos && val < 0)
      return 0;
   return val;
}

static int16_t ctr_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   if (port_num >= DEFAULT_MAX_PADS)
      return 0;
   return ctr_joypad_axis_state(port_num, joyaxis);
}

static int16_t ctr_joypad_state(
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
      
      if ((uint16_t)joykey != NO_BTN && 
            (pad_state & (1 << (uint16_t)joykey)))
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(ctr_joypad_axis_state(port_idx, joyaxis)) 
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static int16_t ctr_joypad_fix_range(int16_t val)
{
   val = (val > 127)? 127: (val < -127)? -127: val;
   return val * 256;
}

static void ctr_joypad_poll(void)
{
   uint32_t state_tmp;
   circlePosition state_tmp_left_analog, state_tmp_right_analog;
   touchPosition state_tmp_touch;

   hidScanInput();

   state_tmp = hidKeysHeld();
   hidCircleRead(&state_tmp_left_analog);
   irrstCstickRead(&state_tmp_right_analog);
   hidTouchRead(&state_tmp_touch);

   pad_state = 0;
   pad_state |= (state_tmp & KEY_DLEFT) ? (1 << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
   pad_state |= (state_tmp & KEY_DDOWN) ? (1 << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
   pad_state |= (state_tmp & KEY_DRIGHT) ? (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
   pad_state |= (state_tmp & KEY_DUP) ? (1 << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
   pad_state |= (state_tmp & KEY_START) ? (1 << RETRO_DEVICE_ID_JOYPAD_START) : 0;
   pad_state |= (state_tmp & KEY_SELECT) ? (1 << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
   pad_state |= (state_tmp & KEY_X) ? (1 << RETRO_DEVICE_ID_JOYPAD_X) : 0;
   pad_state |= (state_tmp & KEY_Y) ? (1 << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
   pad_state |= (state_tmp & KEY_B) ? (1 << RETRO_DEVICE_ID_JOYPAD_B) : 0;
   pad_state |= (state_tmp & KEY_A) ? (1 << RETRO_DEVICE_ID_JOYPAD_A) : 0;
   pad_state |= (state_tmp & KEY_R) ? (1 << RETRO_DEVICE_ID_JOYPAD_R) : 0;
   pad_state |= (state_tmp & KEY_L) ? (1 << RETRO_DEVICE_ID_JOYPAD_L) : 0;
   pad_state |= (state_tmp & KEY_ZR) ? (1 << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
   pad_state |= (state_tmp & KEY_ZL) ? (1 << RETRO_DEVICE_ID_JOYPAD_L2) : 0;

   analog_state[0][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_X]  =  ctr_joypad_fix_range(state_tmp_left_analog.dx);
   analog_state[0][RETRO_DEVICE_INDEX_ANALOG_LEFT] [RETRO_DEVICE_ID_ANALOG_Y]  = -ctr_joypad_fix_range(state_tmp_left_analog.dy);
   analog_state[0][RETRO_DEVICE_INDEX_ANALOG_RIGHT] [RETRO_DEVICE_ID_ANALOG_X] =  ctr_joypad_fix_range(state_tmp_right_analog.dx);
   analog_state[0][RETRO_DEVICE_INDEX_ANALOG_RIGHT] [RETRO_DEVICE_ID_ANALOG_Y] = -ctr_joypad_fix_range(state_tmp_right_analog.dy);
}

static bool ctr_joypad_query_pad(unsigned pad)
{
   /* FIXME */
   return pad < MAX_USERS && pad_state;
}

static void ctr_joypad_destroy(void)
{
}

input_device_driver_t ctr_joypad = {
   ctr_joypad_init,
   ctr_joypad_query_pad,
   ctr_joypad_destroy,
   ctr_joypad_button,
   ctr_joypad_state,
   ctr_joypad_get_buttons,
   ctr_joypad_axis,
   ctr_joypad_poll,
   NULL,
   NULL,
   ctr_joypad_name,
   "ctr",
};
