/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "../input_autodetect.h"

static const char *qnx_joypad_name(unsigned pad)
{
   settings_t *settings = config_get_ptr();
   return settings ? settings->input.device_names[pad] : NULL;
}

static bool qnx_joypad_init(void *data)
{
   unsigned autoconf_pad;
   settings_t *settings = config_get_ptr();

   (void)data;

   for (autoconf_pad = 0; autoconf_pad < MAX_USERS; autoconf_pad++)
   {
      autoconfig_params_t params = {{0}};

      strlcpy(settings->input.device_names[autoconf_pad], "None",
            sizeof(settings->input.device_names[autoconf_pad]));

      /* TODO - implement VID/PID? */
      params.idx = autoconf_pad;
      strlcpy(params.name, qnx_joypad_name(autoconf_pad), sizeof(params.name));
      strlcpy(params.driver, qnx_joypad.ident, sizeof(params.driver));
      input_config_autoconfigure_joypad(&params);
   }

   return true;
}

static bool qnx_joypad_button(unsigned port_num, uint16_t joykey)
{
   void **input_data   = input_driver_get_data_ptr();
   qnx_input_t *qnx    = (qnx_input_t*)&input_data;

   if (!qnx || port_num >= MAX_PADS)
      return false;

   return qnx->pad_state[port_num] & (UINT64_C(1) << joykey);
}

static uint64_t qnx_joypad_get_buttons(unsigned port_num)
{
   void **input_data   = input_driver_get_data_ptr();
   qnx_input_t *qnx    = (qnx_input_t*)&input_data;

   if (!qnx || port_num >= MAX_PADS)
      return 0;
   return qnx->pad_state[port_num];
}

static int16_t qnx_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   int val             = 0;
   int axis            = -1;
   bool is_neg         = false;
   bool is_pos         = false;
   void **input_data   = input_driver_get_data_ptr();
   qnx_input_t *qnx    = (qnx_input_t*)&input_data;

   if (!qnx || joyaxis == AXIS_NONE || port_num >= MAX_PADS)
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
         val = qnx->analog_state[port_num][0][0];
         break;
      case 1:
         val = qnx->analog_state[port_num][0][1];
         break;
      case 2:
         val = qnx->analog_state[port_num][1][0];
         break;
      case 3:
         val = qnx->analog_state[port_num][1][1];
         break;
   }

   if (is_neg && val > 0)
      val = 0;
   else if (is_pos && val < 0)
      val = 0;

   return val;
}

static void qnx_joypad_poll(void)
{
}

static bool qnx_joypad_query_pad(unsigned pad)
{
   void **input_data   = input_driver_get_data_ptr();
   qnx_input_t *qnx    = (qnx_input_t*)&input_data;
   return (qnx && pad < MAX_USERS && qnx->pad_state[pad]);
}


static void qnx_joypad_destroy(void)
{
}

input_device_driver_t qnx_joypad = {
   qnx_joypad_init,
   qnx_joypad_query_pad,
   qnx_joypad_destroy,
   qnx_joypad_button,
   qnx_joypad_get_buttons,
   qnx_joypad_axis,
   qnx_joypad_poll,
   NULL,
   qnx_joypad_name,
   "qnx",
};
