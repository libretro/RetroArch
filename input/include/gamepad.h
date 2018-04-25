/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Andrés Suárez
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

#ifndef GAMEPAD_H__
#define GAMEPAD_H__

#include "../input_driver.h"

struct pad_connection_listener_interface {
   void (*connected)(unsigned port, input_device_driver_t *driver);
};

typedef struct _axis_data {
   int32_t axis;
   bool is_negative;
} axis_data;

void gamepad_read_axis_data(uint32_t axis, axis_data *data);
int16_t gamepad_get_axis_value(int16_t state[3][2], axis_data *data);

#endif /* GAMEPAD_H__ */
