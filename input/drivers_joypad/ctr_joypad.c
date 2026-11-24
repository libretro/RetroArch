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
#include "../include/input_pad_view.h"

#include "../../retroarch.h"
#include "../../command.h"
#include "string.h"
#include "3ds.h"

/* TODO/FIXME - static globals */
/* the pad as its poll left it, lent to the readers (input_pad_view.h):
 * the buttons as RetroPad ids, axes 0-3 the circle pad's and the
 * C-stick's x and y */
static input_pad_view_t ctr_pad;

static const char *ctr_joypad_name(unsigned pad)
{
   return "3DS Controller";
}

static void ctr_joypad_autodetect_add(unsigned autoconf_pad)
{
   input_autoconfigure_connect(
         ctr_joypad_name(autoconf_pad),
         NULL, NULL,
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
   return input_pad_view_button(&ctr_pad, joykey);
}

static void ctr_joypad_get_buttons(unsigned port_num, input_bits_t *state)
{
   if (port_num < DEFAULT_MAX_PADS)
      *state = ctr_pad.buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t ctr_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   if (port_num >= DEFAULT_MAX_PADS)
      return 0;
   return input_pad_view_axis(&ctr_pad, joyaxis);
}

static int16_t ctr_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const uint16_t *joykeys,
      const uint32_t *joyaxes,
      unsigned port)
{
   if (joypad_info->joy_idx >= DEFAULT_MAX_PADS)
      return 0;
   return input_pad_view_state(&ctr_pad, joypad_info, joykeys, joyaxes);
}

/* Read in the poll alone: the same until the next */
static const input_pad_view_t *ctr_joypad_view(unsigned port_num)
{
   return (port_num < DEFAULT_MAX_PADS) ? &ctr_pad : NULL;
}

static int16_t ctr_joypad_fix_range(int16_t val)
{
   val = (val > 127) ? 127 : (val < -127) ? -127 : val;
   return val * 256;
}

static void ctr_joypad_poll(void)
{
   uint32_t state_tmp;
   uint32_t bits = 0;
   circlePosition state_tmp_left_analog, state_tmp_right_analog;

   hidScanInput();

   state_tmp = hidKeysHeld();
   hidCircleRead(&state_tmp_left_analog);
   irrstCstickRead(&state_tmp_right_analog);

   bits |= (state_tmp & KEY_DLEFT) ? (1 << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
   bits |= (state_tmp & KEY_DDOWN) ? (1 << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
   bits |= (state_tmp & KEY_DRIGHT) ? (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
   bits |= (state_tmp & KEY_DUP) ? (1 << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
   bits |= (state_tmp & KEY_START) ? (1 << RETRO_DEVICE_ID_JOYPAD_START) : 0;
   bits |= (state_tmp & KEY_SELECT) ? (1 << RETRO_DEVICE_ID_JOYPAD_SELECT) : 0;
   bits |= (state_tmp & KEY_X) ? (1 << RETRO_DEVICE_ID_JOYPAD_X) : 0;
   bits |= (state_tmp & KEY_Y) ? (1 << RETRO_DEVICE_ID_JOYPAD_Y) : 0;
   bits |= (state_tmp & KEY_B) ? (1 << RETRO_DEVICE_ID_JOYPAD_B) : 0;
   bits |= (state_tmp & KEY_A) ? (1 << RETRO_DEVICE_ID_JOYPAD_A) : 0;
   bits |= (state_tmp & KEY_R) ? (1 << RETRO_DEVICE_ID_JOYPAD_R) : 0;
   bits |= (state_tmp & KEY_L) ? (1 << RETRO_DEVICE_ID_JOYPAD_L) : 0;
   bits |= (state_tmp & KEY_ZR) ? (1 << RETRO_DEVICE_ID_JOYPAD_R2) : 0;
   bits |= (state_tmp & KEY_ZL) ? (1 << RETRO_DEVICE_ID_JOYPAD_L2) : 0;

   ctr_pad.buttons.data[0] = bits;
   ctr_pad.axes[0]         =  ctr_joypad_fix_range(state_tmp_left_analog.dx);
   ctr_pad.axes[1]         = -ctr_joypad_fix_range(state_tmp_left_analog.dy);
   ctr_pad.axes[2]         =  ctr_joypad_fix_range(state_tmp_right_analog.dx);
   ctr_pad.axes[3]         = -ctr_joypad_fix_range(state_tmp_right_analog.dy);
}

static bool ctr_joypad_query_pad(unsigned pad) { return pad == 0; }

static void ctr_joypad_destroy(void) { }

input_device_driver_t ctr_joypad = {
   ctr_joypad_init,
   ctr_joypad_query_pad,
   ctr_joypad_destroy,
   ctr_joypad_button,
   ctr_joypad_state,
   ctr_joypad_get_buttons,
   ctr_joypad_axis,
   ctr_joypad_poll,
   NULL, /* set_rumble */
   NULL, /* set_rumble_gain */
   NULL, /* set_sensor_state */
   NULL, /* get_sensor_input */
   ctr_joypad_name,
   "ctr",
   NULL, /* get_bt_host */
   NULL, /* set_bt_host */
   NULL, /* survives_video */
   NULL, /* set_player */
   ctr_joypad_view
};
