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

#include "../../config.def.h"

#include "../../tasks/tasks_internal.h"
#include "../../configuration.h"
#include "../include/input_pad_view.h"

static const char *qnx_joypad_name(unsigned pad)
{
   return input_config_get_device_name(pad);
}

/* Gamepads are announced by the input driver as libscreen finds them. */
static void *qnx_joypad_init(void *data) { return (void*)-1; }

/* The view of the controller in @port, as the input driver's last
 * event for it left it; NULL for none */
static const input_pad_view_t *qnx_joypad_view(unsigned port)
{
   qnx_input_t *qnx = (qnx_input_t*)input_driver_current_data();
   if (!qnx || port >= DEFAULT_MAX_PADS)
      return NULL;
   return &qnx->devices[port].view;
}

static int32_t qnx_joypad_button(unsigned port, uint16_t joykey)
{
   const input_pad_view_t *v = qnx_joypad_view(port);
   return v ? input_pad_view_button(v, joykey) : 0;
}

static void qnx_joypad_get_buttons(unsigned port, input_bits_t *state)
{
   const input_pad_view_t *v = qnx_joypad_view(port);
   if (v)
      *state = v->buttons;
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t qnx_joypad_axis(unsigned port, uint32_t joyaxis)
{
   const input_pad_view_t *v = qnx_joypad_view(port);
   return v ? input_pad_view_axis(v, joyaxis) : 0;
}

static int16_t qnx_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const uint16_t *joykeys,
      const uint32_t *joyaxes,
      unsigned port)
{
   const input_pad_view_t *v = qnx_joypad_view(joypad_info->joy_idx);
   return v ? input_pad_view_state(v, joypad_info, joykeys, joyaxes) : 0;
}

static void qnx_joypad_poll(void) { }

static bool qnx_joypad_query_pad(unsigned pad)
{
   qnx_input_t *qnx = (qnx_input_t*)input_driver_current_data();
   return qnx && pad < DEFAULT_MAX_PADS
      && qnx->devices[pad].type == SCREEN_EVENT_GAMEPAD;
}

static void qnx_joypad_destroy(void) { }

input_device_driver_t qnx_joypad = {
   qnx_joypad_init,
   qnx_joypad_query_pad,
   qnx_joypad_destroy,
   qnx_joypad_button,
   qnx_joypad_state,
   qnx_joypad_get_buttons,
   qnx_joypad_axis,
   qnx_joypad_poll,
   NULL, /* set_rumble */
   NULL, /* set_rumble_gain */
   NULL, /* set_sensor_state */
   NULL, /* get_sensor_input */
   qnx_joypad_name,
   "qnx",
   NULL, /* get_bt_host */
   NULL, /* set_bt_host */
   NULL, /* survives_video */
   NULL, /* set_player */
   qnx_joypad_view
};
