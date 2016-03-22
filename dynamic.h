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

#ifndef __DYNAMIC_H
#define __DYNAMIC_H

#include <boolean.h>

#include "core_type.h"
#include "libretro.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * libretro_get_environment_info:
 * @func                         : Function pointer for get_environment_info.
 * @load_no_content              : If true, core should be able to auto-start
 *                                 without any content loaded.
 *
 * Sets environment callback in order to get statically known 
 * information from it.
 *
 * Fetched via environment callbacks instead of
 * retro_get_system_info(), as this info is part of extensions.
 *
 * Should only be called once right after core load to 
 * avoid overwriting the "real" environ callback.
 *
 * For statically linked cores, pass retro_set_environment as argument.
 */
void libretro_get_environment_info(void (*)(retro_environment_t),
      bool *load_no_content);

#ifdef HAVE_DYNAMIC
/**
 * libretro_get_system_info:
 * @path                         : Path to libretro library.
 * @info                         : System info information.
 * @load_no_content              : If true, core should be able to auto-start
 *                                 without any content loaded.
 *
 * Gets system info from an arbitrary lib.
 * The struct returned must be freed as strings are allocated dynamically.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool libretro_get_system_info(const char *path,
      struct retro_system_info *info, bool *load_no_content);
#else
/**
 * libretro_get_system_info_static:
 * @info                         : System info information.
 * @load_no_content              : If true, core should be able to auto-start
 *                                 without any content loaded.
 *
 * Gets system info from the current statically linked libretro library.
 * The struct returned must be freed as strings are allocated dynamically.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool libretro_get_system_info_static(struct retro_system_info *info,
      bool *load_no_content);
#endif

/**
 * libretro_free_system_info:
 * @info                         : Pointer to system info information.
 *
 * Frees system information.
 **/
void libretro_free_system_info(struct retro_system_info *info);

/**
 * libretro_get_current_core_pathname:
 * @name                         : Sanitized name of libretro core.
 * @size                         : Size of @name
 *
 * Transforms a library id to a name suitable as a pathname.
 **/
void libretro_get_current_core_pathname(char *name, size_t size);

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
 * Returns: controller description of found controller on success,
 * otherwise NULL.
 **/
const struct retro_controller_description *
   libretro_find_controller_description(
         const struct retro_controller_info *info, unsigned id);

/**
 * rarch_environment_cb:
 * @cmd                          : Identifier of command.
 * @data                         : Pointer to data.
 *
 * Environment callback function implementation.
 *
 * Returns: true (1) if environment callback command could
 * be performed, otherwise false (0).
 **/
bool rarch_environment_cb(unsigned cmd, void *data);

struct retro_core_t
{
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
};

/**
 * init_libretro_sym:
 * @type                        : Type of core to be loaded.
 *                                If CORE_TYPE_DUMMY, will 
 *                                load dummy symbols.
 *
 * Initializes libretro symbols and
 * setups environment callback functions.
 **/
void init_libretro_sym(enum rarch_core_type type,
      struct retro_core_t *core);

/**
 * uninit_libretro_sym:
 *
 * Frees libretro core.
 *
 * Frees all core options,
 * associated state, and
 * unbind all libretro callback symbols.
 **/
void uninit_libretro_sym(struct retro_core_t *core);

#ifdef __cplusplus
}
#endif

#endif

