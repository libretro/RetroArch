/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef RARCH_COMMAND_H__
#define RARCH_COMMAND_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include "boolean.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rarch_cmd rarch_cmd_t;

rarch_cmd_t *rarch_cmd_new(bool stdin_enable, bool network_enable, uint16_t port);
void rarch_cmd_free(rarch_cmd_t *handle);

void rarch_cmd_pre_frame(rarch_cmd_t *handle);
void rarch_cmd_set(rarch_cmd_t *handle, unsigned id);
bool rarch_cmd_get(rarch_cmd_t *handle, unsigned id);

#ifdef HAVE_NETWORK_CMD
bool network_cmd_send(const char *cmd);
#endif

#ifdef __cplusplus
}
#endif

#endif

