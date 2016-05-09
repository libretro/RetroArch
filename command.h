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

#ifndef COMMAND_H__
#define COMMAND_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct command command_t;

typedef struct command_handle
{
   command_t *handle;
   unsigned id;
} command_handle_t;

command_t *command_new(bool stdin_enable,
      bool network_enable, uint16_t port);

#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
bool command_send(const char *cmd_);
#endif

bool command_poll(command_t *handle);

bool command_set(command_handle_t *handle);

bool command_get(command_handle_t *handle);

bool command_free(command_t *handle);

#ifdef __cplusplus
}
#endif

#endif

