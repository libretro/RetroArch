/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
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

#include "wiiu/input.h"

enum wiiu_pad_axes {
  WIIU_AXIS_LEFT_ANALOG_X,
  WIIU_AXIS_LEFT_ANALOG_Y,
  WIIU_AXIS_RIGHT_ANALOG_X,
  WIIU_AXIS_RIGHT_ANALOG_Y,
  WIIU_AXIS_TOUCH_X,
  WIIU_AXIS_TOUCH_Y,
  WIIU_AXIS_INVALID
};

static int16_t clamp_axis(int16_t value, bool is_negative)
{
   if(is_negative && value > 0)
      return 0;
   if(!is_negative && value < 0)
      return 0;

   return value;
}

static int16_t wiiu_pad_get_axis_value(int32_t axis,
      int16_t state[3][2], bool is_negative)
{
   int16_t value = 0;

   switch(axis)
   {
      case WIIU_AXIS_LEFT_ANALOG_X:
         value = state[RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_X];
         break;
      case WIIU_AXIS_LEFT_ANALOG_Y:
         value = state[RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_Y];
         break;
      case WIIU_AXIS_RIGHT_ANALOG_X:
         value = state[RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X];
         break;
      case WIIU_AXIS_RIGHT_ANALOG_Y:
         value = state[RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y];
         break;
      case WIIU_AXIS_TOUCH_X:
         return state[WIIU_DEVICE_INDEX_TOUCHPAD][RETRO_DEVICE_ID_ANALOG_X];
      case WIIU_AXIS_TOUCH_Y:
         return state[WIIU_DEVICE_INDEX_TOUCHPAD][RETRO_DEVICE_ID_ANALOG_Y];
   }

   return clamp_axis(value, is_negative);
}

void wiiu_pad_set_axis_value(
      int16_t state[3][2],
      int16_t left_x,  int16_t left_y,
      int16_t right_x, int16_t right_y,
      int16_t touch_x, int16_t touch_y)
{
  state[RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_X] = left_x;
  state[RETRO_DEVICE_INDEX_ANALOG_LEFT][RETRO_DEVICE_ID_ANALOG_Y] = left_y;
  state[RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_X] = right_x;
  state[RETRO_DEVICE_INDEX_ANALOG_RIGHT][RETRO_DEVICE_ID_ANALOG_Y] = right_y;
  state[WIIU_DEVICE_INDEX_TOUCHPAD][RETRO_DEVICE_ID_ANALOG_X] = touch_x;
  state[WIIU_DEVICE_INDEX_TOUCHPAD][RETRO_DEVICE_ID_ANALOG_Y] = touch_y;
}

wiiu_pad_functions_t pad_functions = {
  wiiu_pad_get_axis_value,
  wiiu_pad_set_axis_value,
  gamepad_read_axis_data,
};
