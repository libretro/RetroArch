/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Andrés Suárez
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

#define MAPPER_GET_KEY(state, key) (((state)->keys[(key) / 32] >> ((key) % 32)) & 1)
#define MAPPER_SET_KEY(state, key) (state)->keys[(key) / 32] |= 1 << ((key) % 32)

RETRO_BEGIN_DECLS

typedef struct input_mapper
{
   /* Left X, Left Y, Right X, Right Y */
   int16_t analog_value[MAX_USERS][8];
   /* the whole keyboard state */
   uint32_t keys[RETROK_LAST / 32 + 1];
   /* This is a bitmask of (1 << key_bind_id). */
   input_bits_t buttons[MAX_USERS];
} input_mapper_t;

void input_mapper_poll(input_mapper_t *handle,
      void *overlay_pointer,
      void *settings_data,
      void *data,
      unsigned max_users,
      bool poll_overlay);

RETRO_END_DECLS

#endif
