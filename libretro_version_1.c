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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <retro_inline.h>
#include <boolean.h>
#include <lists/string_list.h>

#include "dynamic.h"
#include "libretro.h"
#include "libretro_version_1.h"
#include "general.h"
#include "msg_hash.h"
#include "rewind.h"
#include "system.h"
#include "gfx/video_driver.h"
#include "audio/audio_driver.h"

#ifdef HAVE_NETPLAY
#include "netplay/netplay.h"
#endif

static struct retro_core_t core;
static unsigned            core_poll_type;
static bool                core_input_polled;

static int16_t input_state_poll(unsigned port,
      unsigned device, unsigned idx, unsigned id)
{
   if (core_poll_type == POLL_TYPE_LATE)
   {
      if (!core_input_polled)
         input_poll();

      core_input_polled = true;
   }
   return input_state(port, device, idx, id);
}

/**
 * retro_set_default_callbacks:
 * @data           : pointer to retro_callbacks object
 *
 * Binds the libretro callbacks to default callback functions.
 **/
static bool retro_set_default_callbacks(void *data)
{
   struct retro_callbacks *cbs = (struct retro_callbacks*)data;

   if (!cbs)
      return false;

   cbs->frame_cb        = video_driver_frame;
   cbs->sample_cb       = audio_driver_sample;
   cbs->sample_batch_cb = audio_driver_sample_batch;
   cbs->state_cb        = input_state_poll;
   cbs->poll_cb         = input_poll;

   return true;
}

static bool retro_uninit_libretro_cbs(void *data)
{
   struct retro_callbacks *cbs = (struct retro_callbacks*)data;

   if (!cbs)
      return false;

   cbs->frame_cb        = NULL;
   cbs->sample_cb       = NULL;
   cbs->sample_batch_cb = NULL;
   cbs->state_cb        = NULL;
   cbs->poll_cb         = NULL;

   return true;
}

static void input_poll_maybe(void)
{
   if (core_poll_type == POLL_TYPE_NORMAL)
      input_poll();
}

/**
 * retro_init_libretro_cbs:
 * @data           : pointer to retro_callbacks object
 *
 * Initializes libretro callbacks, and binds the libretro callbacks 
 * to default callback functions.
 **/
static bool retro_init_libretro_cbs(void *data)
{
   struct retro_callbacks *cbs = (struct retro_callbacks*)data;
#ifdef HAVE_NETPLAY
   global_t            *global = global_get_ptr();
#endif

   if (!cbs)
      return false;

   core.retro_set_video_refresh(video_driver_frame);
   core.retro_set_audio_sample(audio_driver_sample);
   core.retro_set_audio_sample_batch(audio_driver_sample_batch);
   core.retro_set_input_state(input_state_poll);
   core.retro_set_input_poll(input_poll_maybe);

   core_ctl(CORE_CTL_SET_CBS, cbs);

#ifdef HAVE_NETPLAY
   if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
      return true;

   /* Force normal poll type for netplay. */
   core_poll_type = POLL_TYPE_NORMAL;

   if (global->netplay.is_spectate)
   {
      core.retro_set_input_state(
            (global->netplay.is_client ?
             input_state_spectate_client : input_state_spectate)
            );
   }
   else
   {
      core.retro_set_video_refresh(video_frame_net);
      core.retro_set_audio_sample(audio_sample_net);
      core.retro_set_audio_sample_batch(audio_sample_batch_net);
      core.retro_set_input_state(input_state_net);
   }
#endif

   return true;
}

/**
 * retro_set_rewind_callbacks:
 *
 * Sets the audio sampling callbacks based on whether or not
 * rewinding is currently activated.
 **/
static void retro_set_rewind_callbacks(void)
{
   if (state_manager_frame_is_reversed())
   {
      core.retro_set_audio_sample(audio_driver_sample_rewind);
      core.retro_set_audio_sample_batch(audio_driver_sample_batch_rewind);
   }
   else
   {
      core.retro_set_audio_sample(audio_driver_sample);
      core.retro_set_audio_sample_batch(audio_driver_sample_batch);
   }
}

bool core_ctl(enum core_ctl_state state, void *data)
{
   static bool   has_set_input_descriptors = false;
   static struct retro_callbacks retro_ctx;

   switch (state)
   {
      case CORE_CTL_RETRO_CHEAT_SET:
         {
            retro_ctx_cheat_info_t *info = (retro_ctx_cheat_info_t*)data;
            core.retro_cheat_set(info->index, info->enabled, info->code);
         }
         break;
      case CORE_CTL_RETRO_CHEAT_RESET:
         core.retro_cheat_reset();
         break;
      case CORE_CTL_RETRO_API_VERSION:
         {
            retro_ctx_api_info_t *api = (retro_ctx_api_info_t*)data;
            api->version = core.retro_api_version();
         }
         break;
      case CORE_CTL_SET_POLL_TYPE:
         {
            unsigned *poll_type = (unsigned*)data;
            core_poll_type = *poll_type;
         }
         break;
      case CORE_CTL_RETRO_SYMBOLS_INIT:
         {
            enum rarch_core_type *core_type = (enum rarch_core_type*)data;

            if (!core_type)
               return false;
            init_libretro_sym(*core_type, &core);
         }
         break;
      case CORE_CTL_RETRO_SET_CONTROLLER_PORT_DEVICE:
         {
            retro_ctx_controller_info_t *pad = (retro_ctx_controller_info_t*)data;
            if (!pad)
               return false;
            core.retro_set_controller_port_device(pad->port, pad->device);
         }
         break;
      case CORE_CTL_RETRO_GET_MEMORY:
         {
            retro_ctx_memory_info_t *info = (retro_ctx_memory_info_t*)data;
            if (!info)
               return false;
            info->size  = core.retro_get_memory_size(info->id);
            info->data  = core.retro_get_memory_data(info->id);
         }
         break;
      case CORE_CTL_RETRO_LOAD_GAME:
         {
            retro_ctx_load_content_info_t *load_info = 
               (retro_ctx_load_content_info_t*)data;
            if (!load_info)
               return false;

            if (load_info->special)
               return core.retro_load_game_special(load_info->special->id, load_info->info, load_info->content->size);
            return core.retro_load_game(*load_info->content->elems[0].data ? load_info->info : NULL);
         }
      case CORE_CTL_RETRO_GET_SYSTEM_INFO:
         {
            struct retro_system_info *system = (struct retro_system_info*)data;
            if (!system)
               return false;
            core.retro_get_system_info(system);
         }
         break;
      case CORE_CTL_RETRO_UNSERIALIZE:
         {
            retro_ctx_serialize_info_t *info = (retro_ctx_serialize_info_t*)data;
            if (!info)
               return false;
            if (!core.retro_unserialize(info->data_const, info->size))
               return false;
         }
         break;
      case CORE_CTL_RETRO_SERIALIZE:
         {
            retro_ctx_serialize_info_t *info = (retro_ctx_serialize_info_t*)data;
            if (!info)
               return false;
            if (!core.retro_serialize(info->data, info->size))
               return false;
         }
         break;
      case CORE_CTL_RETRO_SERIALIZE_SIZE:
         {
            retro_ctx_size_info_t *info = (retro_ctx_size_info_t *)data;
            if (!info)
               return false;
            info->size = core.retro_serialize_size();
         }
         break;
      case CORE_CTL_RETRO_CTX_FRAME_CB:
         {
            retro_ctx_frame_info_t *info = (retro_ctx_frame_info_t*)data;
            if (!info || !retro_ctx.frame_cb)
               return false;

            retro_ctx.frame_cb(
                  info->data, info->width, info->height, info->pitch);
         }
         break;
      case CORE_CTL_RETRO_CTX_POLL_CB:
         if (!retro_ctx.poll_cb)
            return false;
         retro_ctx.poll_cb();
         break;
      case CORE_CTL_RETRO_SET_ENVIRONMENT:
         {
            retro_ctx_environ_info_t *info = (retro_ctx_environ_info_t*)data;
            if (!info)
               return false;
            core.retro_set_environment(info->env);
         }
         break;
      case CORE_CTL_RETRO_GET_SYSTEM_AV_INFO:
         {
            struct retro_system_av_info *av_info = (struct retro_system_av_info*)data;
            if (!av_info)
               return false;
            core.retro_get_system_av_info(av_info);
         }
         break;
      case CORE_CTL_RETRO_RESET:
         core.retro_reset();
         break;
      case CORE_CTL_RETRO_INIT:
         core.retro_init();
         break;
      case CORE_CTL_RETRO_DEINIT:
         core.retro_deinit();
         uninit_libretro_sym(&core);
         break;
      case CORE_CTL_RETRO_UNLOAD_GAME:
         video_driver_ctl(RARCH_DISPLAY_CTL_DEINIT_HW_CONTEXT, NULL);
         audio_driver_ctl(RARCH_AUDIO_CTL_STOP, NULL);
         core.retro_unload_game();
         break;
      case CORE_CTL_RETRO_RUN:
         switch (core_poll_type)
         {
            case POLL_TYPE_EARLY:
               input_poll();
               break;
            case POLL_TYPE_LATE:
               core_input_polled = false;
               break;
         }
         if (core.retro_run)
            core.retro_run();
         if (core_poll_type == POLL_TYPE_LATE && !core_input_polled)
            input_poll();
         break;
      case CORE_CTL_SET_CBS:
         return retro_set_default_callbacks(data);
      case CORE_CTL_SET_CBS_REWIND:
         retro_set_rewind_callbacks();
         break;
      case CORE_CTL_INIT:
         {
            settings_t *settings = config_get_ptr();
            core_poll_type = settings->input.poll_type_behavior;
            if (!core_ctl(CORE_CTL_VERIFY_API_VERSION, NULL))
               return false;
            if (!retro_init_libretro_cbs(&retro_ctx))
               return false;
            core_ctl(CORE_CTL_RETRO_GET_SYSTEM_AV_INFO,
                  video_viewport_get_system_av_info());
            runloop_ctl(RUNLOOP_CTL_SET_FRAME_LIMIT, NULL);
         }
         break;
      case CORE_CTL_DEINIT:
         return retro_uninit_libretro_cbs(&retro_ctx);
      case CORE_CTL_VERIFY_API_VERSION:
         {
            unsigned api_version = core.retro_api_version();
            RARCH_LOG("Version of libretro API: %u\n", api_version);
            RARCH_LOG("Compiled against API: %u\n",    RETRO_API_VERSION);

            if (api_version != RETRO_API_VERSION)
            {
               RARCH_WARN("%s\n", msg_hash_to_str(MSG_LIBRETRO_ABI_BREAK));
               return false;
            }
         }
         break;
      case CORE_CTL_HAS_SET_INPUT_DESCRIPTORS:
         return has_set_input_descriptors;
      case CORE_CTL_SET_INPUT_DESCRIPTORS:
         has_set_input_descriptors = true;
         break;
      case CORE_CTL_UNSET_INPUT_DESCRIPTORS:
         has_set_input_descriptors = false;
         break;
      case CORE_CTL_NONE:
      default:
         break;
   }

   return true;
}
