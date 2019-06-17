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

#ifndef RARCH_AUDIO_THREAD_H__
#define RARCH_AUDIO_THREAD_H__

#include <boolean.h>

#include "../retroarch.h"

/**
 * audio_init_thread:
 * @out_driver                : output driver
 * @out_data                  : output audio data
 * @device                    : audio device (optional)
 * @out_rate                  : output audio rate
 * @new_rate                  : new output audio rate
 * @latency                   : audio latency
 * @driver                    : audio driver
 *
 * Starts a audio driver in a new thread.
 * Access to audio driver will be mediated through this driver.
 * This driver interfaces with audio callback and is only used in that case.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool audio_init_thread(const audio_driver_t **out_driver, void **out_data,
      const char *device, unsigned out_rate, unsigned *new_rate, unsigned latency,
      unsigned block_frames,
      const audio_driver_t *driver);

#endif
