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

static const char *qnx_joypad_name(unsigned pad)
{
   return input_config_get_device_name(pad);
}

static bool qnx_joypad_init(void *data)
{
   unsigned autoconf_pad;

   (void)data;

   for (autoconf_pad = 0; autoconf_pad < MAX_USERS; autoconf_pad++)
      input_autoconfigure_connect(
            qnx_joypad_name(autoconf_pad),
            NULL,
            qnx_joypad.ident,
            autoconf_pad,
            0,
            0
            );

   return true;
}

static bool qnx_joypad_button(unsigned port_num, uint16_t joykey)
{
   qnx_input_device_t* controller = NULL;
   qnx_input_t *qnx              = (qnx_input_t*)input_driver_get_data();

   if (!qnx || port_num >= DEFAULT_MAX_PADS)
      return 0;

   controller = (qnx_input_device_t*)&qnx->devices[port_num];

   if(port_num < MAX_USERS && joykey <= 19)
      return (controller->buttons & (1 << joykey)) != 0;

   return false;
}

static int16_t qnx_joypad_axis(unsigned port_num, uint32_t joyaxis)
{
   int val             = 0;
   int axis            = -1;
   bool is_neg         = false;
   bool is_pos         = false;
   qnx_input_t *qnx    = (qnx_input_t*)input_driver_get_data();

   if (!qnx || joyaxis == AXIS_NONE || port_num >= DEFAULT_MAX_PADS)
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

   qnx_input_device_t* controller = NULL;
   controller = (qnx_input_device_t*)&qnx->devices[port_num];

   switch (axis)
   {
      case 0:
         val = controller->analog0[0];
         break;
      case 1:
          val = controller->analog0[1];
         break;
      case 2:
          val = controller->analog1[0];
         break;
      case 3:
          val = controller->analog1[1];
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
   return (pad < MAX_USERS);
}

static void qnx_joypad_destroy(void)
{
}

input_device_driver_t qnx_joypad = {
   qnx_joypad_init,
   qnx_joypad_query_pad,
   qnx_joypad_destroy,
   qnx_joypad_button,
   NULL,
   qnx_joypad_axis,
   qnx_joypad_poll,
   NULL,
   qnx_joypad_name,
   "qnx",
};
