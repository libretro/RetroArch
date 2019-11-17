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

#include "core_type.h"
#include "input/input_defines.h"

RETRO_BEGIN_DECLS

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

typedef struct rarch_memory_descriptor
{
   struct retro_memory_descriptor core;
   size_t disconnect_mask;
} rarch_memory_descriptor_t;

typedef struct rarch_memory_map
{
   rarch_memory_descriptor_t *descriptors;
   unsigned num_descriptors;
} rarch_memory_map_t;

typedef struct rarch_system_info
{
   struct retro_system_info info;

   unsigned rotation;
   unsigned performance_level;
   bool load_no_content;

   const char *input_desc_btn[MAX_USERS][RARCH_FIRST_META_KEY];
   char valid_extensions[255];

   bool supports_vfs;

   struct retro_disk_control_callback  disk_control_cb;
   struct retro_location_callback      location_cb;

   struct
   {
      struct retro_subsystem_info *data;
      unsigned size;
   } subsystem;

   struct
   {
      struct retro_controller_info *data;
      unsigned size;
   } ports;

   rarch_memory_map_t mmaps;
} rarch_system_info_t;

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

typedef struct retro_ctx_region_info
{
  unsigned region;
} retro_ctx_region_info_t;

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

typedef struct retro_callbacks
{
   retro_video_refresh_t frame_cb;
   retro_audio_sample_t sample_cb;
   retro_audio_sample_batch_t sample_batch_cb;
   retro_input_state_t state_cb;
   retro_input_poll_t poll_cb;
} retro_callbacks_t;

bool core_set_default_callbacks(struct retro_callbacks *cbs);

bool core_set_rewind_callbacks(void);

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

RETRO_END_DECLS

#endif
