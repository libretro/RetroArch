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

enum rarch_cmd_ctl_state
{
   RARCH_CMD_CTL_NONE = 0,
   RARCH_CMD_CTL_GET,
   RARCH_CMD_CTL_SET,
   RARCH_CMD_CTL_POLL,
   RARCH_CMD_CTL_NETWORK_SEND,
   RARCH_CMD_CTL_FREE
};

typedef struct rarch_cmd rarch_cmd_t;

typedef struct rarch_cmd_handle
{
   rarch_cmd_t *handle;
   unsigned id;
} rarch_cmd_handle_t;

rarch_cmd_t *rarch_cmd_new(bool stdin_enable,
      bool network_enable, uint16_t port);

bool rarch_cmd_ctl(enum rarch_cmd_ctl_state state, void *data);

#ifdef __cplusplus
}
#endif

#endif

