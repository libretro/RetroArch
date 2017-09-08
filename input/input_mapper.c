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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <compat/strl.h>
#include <compat/posix_string.h>
#include <file/file_path.h>
#include <retro_miscellaneous.h>
#include <libretro.h>
#include <net/net_compat.h>
#include <net/net_socket.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "input_mapper.h"

#include "../configuration.h"
#include "../msg_hash.h"
#include "../verbosity.h"


struct input_mapper
{

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
   int net_fd[MAX_USERS];
#endif

   bool state[RARCH_BIND_LIST_END];
};

typedef struct input_mapper_state
{
   /* This is a bitmask of (1 << key_bind_id). */
   uint64_t buttons;
   /* Left X, Left Y, Right X, Right Y */
   int16_t analog[4]; 
   /* the whole keyboard state */
   uint32_t keys[RETROK_LAST / 32 + 1];
} input_mapper_state_t;

input_mapper_t *input_mapper_new(uint16_t port)
{
   return NULL;
}

void input_mapper_free(input_mapper_t *handle)
{
   return;
}

void input_mapper_poll(input_mapper_t *handle, unsigned max_users)
{
   return;
}

bool input_mapper_key_pressed(int key, unsigned port)
{
   return false;
}

void input_mapper_state(
      int16_t *ret,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   return;
}