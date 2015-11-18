/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "input_sensor.h"

#include "../driver.h"

/**
 * input_sensor_set_state:
 * @port               : User number.
 * @effect             : Sensor action.
 * @rate               : Sensor rate update.
 *
 * Sets the sensor state.
 * Used by RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE.
 **/
bool input_sensor_set_state(unsigned port,
      enum retro_sensor_action action, unsigned rate)
{
   driver_t *driver = driver_get_ptr();

   if (driver->input && driver->input_data &&
         driver->input->set_sensor_state)
      return driver->input->set_sensor_state(driver->input_data,
            port, action, rate);
   return false;
}

float input_sensor_get_input(unsigned port, unsigned id)
{
   driver_t *driver = driver_get_ptr();

   if (driver->input && driver->input_data &&
         driver->input->get_sensor_input)
      return driver->input->get_sensor_input(driver->input_data,
            port, id);
   return 0.0f;
}
