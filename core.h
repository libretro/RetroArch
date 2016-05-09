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

#ifdef __cplusplus
extern "C" {
#endif

#include <boolean.h>

#include "core_type.h"
#include "libretro.h"

enum
{
   /* Polling is performed before 
    * call to retro_run. */
   POLL_TYPE_EARLY = 0,

   /* Polling is performed when requested. */
   POLL_TYPE_NORMAL,

   /* Polling is performed on first call to 
    * retro_input_state per frame. */
   POLL_TYPE_LATE
};

typedef struct retro_ctx_input_state_info
{
   retro_input_state_t cb;
} retro_ctx_input_state_info_t;

typedef struct retro_ctx_cheat_info
{
   unsigned index;
   bool enabled;
   const char *code;
} retro_ctx_cheat_info_t;

typedef struct retro_ctx_api_info
{
   unsigned version;
} retro_ctx_api_info_t;

typedef struct retro_ctx_controller_info
{
   unsigned port;
   unsigned device;
} retro_ctx_controller_info_t;

typedef struct retro_ctx_memory_info
{
   void *data;
   size_t size;
   unsigned id;
} retro_ctx_memory_info_t;

typedef struct retro_ctx_load_content_info
{
   struct retro_game_info *info;
   const struct string_list *content;
   const struct retro_subsystem_info *special;
} retro_ctx_load_content_info_t;

typedef struct retro_ctx_serialize_info
{
   const void *data_const;
   void *data;
   size_t size;
} retro_ctx_serialize_info_t;

typedef struct retro_ctx_size_info
{
   size_t size;
} retro_ctx_size_info_t;

typedef struct retro_ctx_environ_info
{
   retro_environment_t env;
} retro_ctx_environ_info_t;

typedef struct retro_ctx_frame_info
{
   const void *data;
   unsigned width;
   unsigned height;
   size_t pitch;
} retro_ctx_frame_info_t;

typedef struct retro_callbacks
{
   retro_video_refresh_t frame_cb;
   retro_audio_sample_t sample_cb;
   retro_audio_sample_batch_t sample_batch_cb;
   retro_input_state_t state_cb;
   retro_input_poll_t poll_cb;
} retro_callbacks_t;

bool core_load(void);

bool core_unload(void);

bool core_set_default_callbacks(void *data);

bool core_set_rewind_callbacks(void);

bool core_set_poll_type(unsigned *type);

/* Runs the core for one frame. */
bool core_run(void);

bool core_init(void);

bool core_deinit(void *data);

bool core_unload_game(void);

bool core_reset(void);

bool core_frame(retro_ctx_frame_info_t *info);

bool core_poll(void);

bool core_set_environment(retro_ctx_environ_info_t *info);

bool core_serialize_size(retro_ctx_size_info_t *info);

bool core_serialize(retro_ctx_serialize_info_t *info);

bool core_unserialize(retro_ctx_serialize_info_t *info);

bool core_init_symbols(enum rarch_core_type *type);

bool core_set_cheat(retro_ctx_cheat_info_t *info);

bool core_reset_cheat(void);

bool core_api_version(retro_ctx_api_info_t *api);

/* Compare libretro core API version against API version
 * used by RetroArch.
 *
 * TODO - when libretro v2 gets added, allow for switching
 * between libretro version backend dynamically.
 */
bool core_verify_api_version(void);

bool core_get_memory(retro_ctx_memory_info_t *info);

/* Initialize system A/V information. */
bool core_get_system_av_info(struct retro_system_av_info *av_info);

/* Get system A/V information. */
bool core_get_system_info(struct retro_system_info *system);

bool core_load_game(retro_ctx_load_content_info_t *load_info);

bool core_set_controller_port_device(retro_ctx_controller_info_t *pad);

bool core_has_set_input_descriptor(void);

void core_set_input_descriptors(void);

void core_unset_input_descriptors(void);

bool core_uninit_libretro_callbacks(void);

void core_set_input_state(retro_ctx_input_state_info_t *info);

#ifdef __cplusplus
}
#endif

#endif
