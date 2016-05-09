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

#ifndef INPUT_REMOTE_H__
#define INPUT_REMOTE_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct input_remote input_remote_t;

input_remote_t *input_remote_new(uint16_t port);

void input_remote_free(input_remote_t *handle);

void input_remote_poll(input_remote_t *handle);

bool input_remote_key_pressed(int key, unsigned port);

void input_remote_state(
      int16_t *ret,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id);

#ifdef __cplusplus
}
#endif

#endif

