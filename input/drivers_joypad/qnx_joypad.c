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

static void *qnx_joypad_init(void *data)
{
   unsigned autoconf_pad;

   for (autoconf_pad = 0; autoconf_pad < MAX_USERS; autoconf_pad++)
      input_autoconfigure_connect(
            qnx_joypad_name(autoconf_pad),
            NULL,
            qnx_joypad.ident,
            autoconf_pad,
            0,
            0
            );

   return (void*)-1;
}

static int32_t qnx_joypad_button(unsigned port, uint16_t joykey)
{
   qnx_input_device_t* controller       = NULL;
   qnx_input_t *qnx                     = (qnx_input_t*)input_driver_get_data();

   if (!qnx || port >= DEFAULT_MAX_PADS)
      return 0;

   controller = (qnx_input_device_t*)&qnx->devices[port];

   if (joykey <= 19)
      return ((controller->buttons & (1 << joykey)) != 0);
   return 0;
}

static int16_t qnx_joypad_axis_state(
      qnx_input_t *qnx,
      qnx_input_device_t *controller,
      unsigned port, uint32_t joyaxis)
{
   int val             = 0;
   int axis            = -1;
   bool is_neg         = false;
   bool is_pos         = false;

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

   switch (axis)
   {
      case 0:
      case 1:
         val = controller->analog0[axis];
         break;
      case 2:
      case 3:
         val = controller->analog1[axis-2];
         break;
   }

   if (is_neg && val > 0)
      return 0;
   else if (is_pos && val < 0)
      return 0;
   return val;
}

static int16_t qnx_joypad_axis(unsigned port, uint32_t joyaxis)
{
   qnx_input_t            *qnx    = (qnx_input_t*)input_driver_get_data();
   qnx_input_device_t* controller = NULL;
   if (!qnx || port >= DEFAULT_MAX_PADS)
      return 0;
   controller                     = (qnx_input_device_t*)&qnx->devices[port];
   return qnx_joypad_axis_state(qnx, controller, port, joyaxis);
}

static int16_t qnx_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t ret                    = 0;
   qnx_input_t            *qnx    = (qnx_input_t*)input_driver_get_data();
   qnx_input_device_t* controller = NULL;
   uint16_t port_idx              = joypad_info->joy_idx;

   if (!qnx || port_idx >= DEFAULT_MAX_PADS)
      return 0;
   controller                     = (qnx_input_device_t*)&qnx->devices[port_idx];

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if (
               (uint16_t)joykey != NO_BTN 
            && (joykey <= 19)
            && ((controller->buttons & (1 << (uint16_t)joykey)) != 0)
         )
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(qnx_joypad_axis_state(qnx, controller, port_idx, joyaxis)) 
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static void qnx_joypad_poll(void) { }

static bool qnx_joypad_query_pad(unsigned pad)
{
   return (pad < MAX_USERS);
}

static void qnx_joypad_destroy(void) { }

input_device_driver_t qnx_joypad = {
   qnx_joypad_init,
   qnx_joypad_query_pad,
   qnx_joypad_destroy,
   qnx_joypad_button,
   qnx_joypad_state,
   NULL,
   qnx_joypad_axis,
   qnx_joypad_poll,
   NULL,
   NULL,
   qnx_joypad_name,
   "qnx",
};
