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

#ifndef INPUT_MAPPER_H__
#define INPUT_MAPPER_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

typedef struct input_mapper input_mapper_t;

input_mapper_t *input_mapper_new(uint16_t port);

void input_mapper_free(input_mapper_t *handle);

void input_mapper_poll(input_mapper_t *handle);

bool input_mapper_key_pressed(int key);

void input_mapper_state(
      int16_t *ret,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id);

RETRO_END_DECLS

#endif
