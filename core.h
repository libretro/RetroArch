/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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

#ifndef _LIBRETRO_CORE_IMPL_H
#define _LIBRETRO_CORE_IMPL_H

#include <boolean.h>
#include <libretro.h>

#include <retro_common_api.h>

#include "retroarch_types.h"

RETRO_BEGIN_DECLS

#ifdef HAVE_REWIND
bool core_set_rewind_callbacks(void);
#endif

#ifdef HAVE_NETWORKING
bool core_set_netplay_callbacks(void);

bool core_unset_netplay_callbacks(void);
#endif

bool core_set_poll_type(unsigned type);

/* Runs the core for one frame. */
bool core_run(void);

bool core_reset(void);

bool core_serialize_size(retro_ctx_size_info_t *info);

uint64_t core_serialization_quirks(void);

bool core_serialize(retro_ctx_serialize_info_t *info);

bool core_unserialize(retro_ctx_serialize_info_t *info);

bool core_set_cheat(retro_ctx_cheat_info_t *info);

bool core_reset_cheat(void);

bool core_get_memory(retro_ctx_memory_info_t *info);

/* Get system A/V information. */
bool core_get_system_info(struct retro_system_info *system);

bool core_load_game(retro_ctx_load_content_info_t *load_info);

bool core_set_controller_port_device(retro_ctx_controller_info_t *pad);

bool core_has_set_input_descriptor(void);

/**
 * core_set_default_callbacks:
 * @data           : pointer to retro_callbacks object
 *
 * Binds the libretro callbacks to default callback functions.
 **/
bool core_set_default_callbacks(void *data);

RETRO_END_DECLS

#endif
