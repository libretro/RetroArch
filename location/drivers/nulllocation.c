/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "../location_driver.h"

static void *null_location_init(void)
{
   return NULL;
}

static void null_location_free(void *data)
{
   (void)data;
}

static bool null_location_start(void *data)
{
   (void)data;
   return true;
}

static void null_location_stop(void *data)
{
   (void)data;
}

static bool null_location_get_position(void *data, double *latitude,
      double *longitude, double *horiz_accuracy,
      double *vert_accuracy)
{
   *latitude  = 0.0;
   *longitude = 0.0;
   *horiz_accuracy  = 0.0;
   *vert_accuracy  = 0.0;
   return true;
}

static void null_location_set_interval(void *data,
      unsigned interval_ms, unsigned interval_distance)
{
}

location_driver_t location_null = {
   null_location_init,
   null_location_free,
   null_location_start,
   null_location_stop,
   null_location_get_position,
   null_location_set_interval,
   "null",
};
