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

#ifndef _RETRO_IMPLEMENTATION_V1_H
#define _RETRO_IMPLEMENTATION_V1_H

#ifdef __cplusplus
extern "C" {
#endif

#include <boolean.h>

#include "content.h"
#include "libretro.h"

enum
{
   /* Polling is performed before call to retro_run */
   POLL_TYPE_EARLY = 0,
   /* Polling is performed when requested. */
   POLL_TYPE_NORMAL,
   /* Polling is performed on first call to 
    * retro_input_state per frame. */
   POLL_TYPE_LATE
};

enum core_ctl_state
{
   CORE_CTL_NONE = 0,

   CORE_CTL_INIT,

   CORE_CTL_DEINIT,

   CORE_CTL_SET_CBS,

   CORE_CTL_SET_CBS_REWIND,

   /* Runs the core for one frame. */
   CORE_CTL_RETRO_RUN,

   CORE_CTL_RETRO_INIT,

   CORE_CTL_RETRO_DEINIT,

   CORE_CTL_RETRO_UNLOAD_GAME,

   CORE_CTL_RETRO_RESET,

   CORE_CTL_RETRO_GET_SYSTEM_AV_INFO,

   CORE_CTL_RETRO_CTX_FRAME_CB,

   CORE_CTL_RETRO_CTX_POLL_CB,
   
   CORE_CTL_RETRO_SET_ENVIRONMENT,

   CORE_CTL_RETRO_SERIALIZE_SIZE,

   CORE_CTL_RETRO_SERIALIZE,

   CORE_CTL_RETRO_UNSERIALIZE,

   CORE_CTL_RETRO_SYMBOLS_INIT,

   /* Compare libretro core API version against API version
    * used by RetroArch.
    *
    * TODO - when libretro v2 gets added, allow for switching
    * between libretro version backend dynamically.
    */
   CORE_CTL_VERIFY_API_VERSION,

   CORE_CTL_RETRO_GET_MEMORY,

   /**
    * Initialize system A/V information.
    **/
   CORE_CTL_INIT_SYSTEM_AV_INFO,

   CORE_CTL_RETRO_GET_SYSTEM_INFO,

   CORE_CTL_RETRO_LOAD_GAME,

   CORE_CTL_RETRO_SET_CONTROLLER_PORT_DEVICE
};

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

bool core_ctl(enum core_ctl_state state, void *data);

#ifdef __cplusplus
}
#endif

#endif
