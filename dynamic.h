/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#ifndef __DYNAMIC_H
#define __DYNAMIC_H

#include <boolean.h>
#include <retro_common_api.h>
#include <libretro.h>

#include "retroarch_types.h"

RETRO_BEGIN_DECLS

enum retro_core_flags
{
   RETRO_CORE_FLAG_INITED                    = (1 << 0),
   RETRO_CORE_FLAG_SYMBOLS_INITED            = (1 << 1),
   RETRO_CORE_FLAG_GAME_LOADED               = (1 << 2),
   RETRO_CORE_FLAG_INPUT_POLLED              = (1 << 3),
   RETRO_CORE_FLAG_HAS_SET_SUBSYSTEMS        = (1 << 4),
   RETRO_CORE_FLAG_HAS_SET_INPUT_DESCRIPTORS = (1 << 5)
};

struct retro_core_t
{
   uint64_t serialization_quirks_v;
   void (*retro_init)(void);
   void (*retro_deinit)(void);
   unsigned (*retro_api_version)(void);
   void (*retro_get_system_info)(struct retro_system_info*);
   void (*retro_get_system_av_info)(struct retro_system_av_info*);
   void (*retro_set_environment)(retro_environment_t);
   void (*retro_set_video_refresh)(retro_video_refresh_t);
   void (*retro_set_audio_sample)(retro_audio_sample_t);
   void (*retro_set_audio_sample_batch)(retro_audio_sample_batch_t);
   void (*retro_set_input_poll)(retro_input_poll_t);
   void (*retro_set_input_state)(retro_input_state_t);
   void (*retro_set_controller_port_device)(unsigned, unsigned);
   void (*retro_reset)(void);
   void (*retro_run)(void);
   size_t (*retro_serialize_size)(void);
   bool (*retro_serialize)(void*, size_t);
   bool (*retro_unserialize)(const void*, size_t);
   void (*retro_cheat_reset)(void);
   void (*retro_cheat_set)(unsigned, bool, const char*);
   bool (*retro_load_game)(const struct retro_game_info*);
   bool (*retro_load_game_special)(unsigned,
         const struct retro_game_info*, size_t);
   void (*retro_unload_game)(void);
   unsigned (*retro_get_region)(void);
   void *(*retro_get_memory_data)(unsigned);
   size_t (*retro_get_memory_size)(unsigned);

   enum poll_type poll_type;
   uint8_t flags;
};


/**
 * libretro_free_system_info:
 * @info                         : Pointer to system info information.
 *
 * Frees system information.
 **/
void libretro_free_system_info(struct retro_system_info *info);

const struct retro_subsystem_info *libretro_find_subsystem_info(
      const struct retro_subsystem_info *info,
      unsigned num_info, const char *ident);

/**
 * libretro_find_controller_description:
 * @info                         : Pointer to controller info handle.
 * @id                           : Identifier of controller to search
 *                                 for.
 *
 * Search for a controller of type @id in @info.
 *
 * Leaf function.
 *
 * @return controller description of found controller on success,
 * otherwise NULL.
 **/
const struct retro_controller_description *
   libretro_find_controller_description(
         const struct retro_controller_info *info, unsigned id);

RETRO_END_DECLS

#endif
