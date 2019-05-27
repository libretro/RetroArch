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

#include "../include/gamepad.h"

enum gamepad_pad_axes
{
   AXIS_LEFT_ANALOG_X,
   AXIS_LEFT_ANALOG_Y,
   AXIS_RIGHT_ANALOG_X,
   AXIS_RIGHT_ANALOG_Y,
   AXIS_INVALID
};

static int16_t gamepad_clamp_axis(int16_t value, bool is_negative)
{
   if (is_negative && value > 0)
      return 0;
   if (!is_negative && value < 0)
      return 0;

   return value;
}

void gamepad_read_axis_data(uint32_t axis, axis_data *data)
{
   if(!data)
      return;

   data->axis = AXIS_POS_GET(axis);
   data->is_negative = false;

   if(data->axis >= AXIS_INVALID)
   {
      data->axis = AXIS_NEG_GET(axis);
      data->is_negative = true;
   }
}

int16_t gamepad_get_axis_value(int16_t state[3][2], axis_data *data)
{
   int16_t value = 0;

   if(!data)
      return 0;

   switch(data->axis)
   {
      case AXIS_LEFT_ANALOG_X:
         value = state[RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_X];
         break;
      case AXIS_LEFT_ANALOG_Y:
         value = state[RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_Y];
         break;
      case AXIS_RIGHT_ANALOG_X:
         value = state[RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X];
         break;
      case AXIS_RIGHT_ANALOG_Y:
         value = state[RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y];
         break;
   }

   return gamepad_clamp_axis(value, data->is_negative);
}
