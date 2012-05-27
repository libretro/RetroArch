/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#ifndef NETWORK_CMD_H__
#define NETWORK_CMD_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct network_cmd network_cmd_t;

network_cmd_t *network_cmd_new(uint16_t port);
void network_cmd_free(network_cmd_t *handle);

void network_cmd_pre_frame(network_cmd_t *handle);
void network_cmd_set(network_cmd_t *handle, unsigned id);
bool network_cmd_get(network_cmd_t *handle, unsigned id);

#endif

