/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef REMOTE_H__
#define REMOTE_H__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <boolean.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rarch_remote rarch_remote_t;

rarch_remote_t *rarch_remote_new(uint16_t port);

void rarch_remote_free(rarch_remote_t *handle);

void rarch_remote_poll(rarch_remote_t *handle);

void rarch_remote_set(rarch_remote_t *handle, unsigned id);

bool rarch_remote_get(rarch_remote_t *handle, unsigned id);

#ifdef __cplusplus
}
#endif

#endif

