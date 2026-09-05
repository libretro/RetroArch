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

#ifndef _LINUX_COMMON_H
#define _LINUX_COMMON_H

#include <boolean.h>

void linux_terminal_restore_input(void);

void linux_terminal_claim_stdin(void);

bool linux_terminal_grab_stdin(void *data);

bool linux_terminal_disable_input(void);

/**
 * Corresponds to the illuminance sensor exposed via the IIO interface.
 * @see https://github.com/torvalds/linux/blob/master/Documentation/ABI/testing/sysfs-bus-iio
 */
typedef struct linux_illuminance_sensor linux_illuminance_sensor_t;

/**
 * Iterates through /sys/bus/iio/devices and returns the first illuminance sensor found,
 * or NULL if none was found.
 *
 * @param rate The rate at which to poll the sensor, in Hz.
 */
linux_illuminance_sensor_t *linux_open_illuminance_sensor(unsigned rate);

void linux_close_illuminance_sensor(linux_illuminance_sensor_t *sensor);

/** Returns the light sensor's most recent reading in lux, or a negative number on error. */
float linux_get_illuminance_reading(const linux_illuminance_sensor_t *sensor);

void linux_set_illuminance_sensor_rate(linux_illuminance_sensor_t *sensor, unsigned rate);

#endif
