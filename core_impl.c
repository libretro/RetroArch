/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2016-2017 - Brad Parker
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

#include <boolean.h>
#include <lists/string_list.h>
#include <string/stdstring.h>
#include <libretro.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_NETWORKING
#include "network/netplay/netplay.h"
#endif

#include "core.h"
#include "content.h"
#include "dynamic.h"
#include "msg_hash.h"
#include "managers/state_manager.h"
#include "verbosity.h"
#include "gfx/video_driver.h"
#include "audio/audio_driver.h"

struct                     retro_callbacks retro_ctx;
struct                     retro_core_t current_core;

static void retro_run_null(void)
{
}

static void retro_frame_null(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
}

static void retro_input_poll_null(void)
{
}

static void core_input_state_poll_maybe(void)
{
   if (current_core.poll_type == POLL_TYPE_NORMAL)
      input_poll();
}

static int16_t core_input_state_poll(unsigned port,
      unsigned device, unsigned idx, unsigned id)
{
   if (current_core.poll_type == POLL_TYPE_LATE)
   {
      if (!current_core.input_polled)
         input_poll();

      current_core.input_polled = true;
   }
   return input_state(port, device, idx, id);
}

void core_set_input_state(retro_ctx_input_state_info_t *info)
{
   current_core.retro_set_input_state(info->cb);
}

/**
 * core_init_libretro_cbs:
 * @data           : pointer to retro_callbacks object
 *
 * Initializes libretro callbacks, and binds the libretro callbacks
 * to default callback functions.
 **/
static bool core_init_libretro_cbs(struct retro_callbacks *cbs)
{
   current_core.retro_set_video_refresh(video_driver_frame);
   current_core.retro_set_audio_sample(audio_driver_sample);
   current_core.retro_set_audio_sample_batch(audio_driver_sample_batch);
   current_core.retro_set_input_state(core_input_state_poll);
   current_core.retro_set_input_poll(core_input_state_poll_maybe);

   core_set_default_callbacks(cbs);

#ifdef HAVE_NETWORKING
   if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
      return true;

   core_set_netplay_callbacks();
#endif

   return true;
}

/**
 * core_set_default_callbacks:
 * @data           : pointer to retro_callbacks object
 *
 * Binds the libretro callbacks to default callback functions.
 **/
bool core_set_default_callbacks(struct retro_callbacks *cbs)
{
   cbs->frame_cb        = video_driver_frame;
   cbs->sample_cb       = audio_driver_sample;
   cbs->sample_batch_cb = audio_driver_sample_batch;
   cbs->state_cb        = core_input_state_poll;
   cbs->poll_cb         = input_poll;

   return true;
}


bool core_deinit(void *data)
{
   struct retro_callbacks *cbs = (struct retro_callbacks*)data;

   if (!cbs)
      return false;

   cbs->frame_cb        = retro_frame_null;
   cbs->sample_cb       = NULL;
   cbs->sample_batch_cb = NULL;
   cbs->state_cb        = NULL;
   cbs->poll_cb         = retro_input_poll_null;

   current_core.inited  = false;

   return true;
}

bool core_uninit_libretro_callbacks(void)
{
   return core_deinit(&retro_ctx);
}

/**
 * core_set_rewind_callbacks:
 *
 * Sets the audio sampling callbacks based on whether or not
 * rewinding is currently activated.
 **/
bool core_set_rewind_callbacks(void)
{
   if (state_manager_frame_is_reversed())
   {
      current_core.retro_set_audio_sample(audio_driver_sample_rewind);
      current_core.retro_set_audio_sample_batch(audio_driver_sample_batch_rewind);
   }
   else
   {
      current_core.retro_set_audio_sample(audio_driver_sample);
      current_core.retro_set_audio_sample_batch(audio_driver_sample_batch);
   }
   return true;
}

#ifdef HAVE_NETWORKING
/**
 * core_set_netplay_callbacks:
 *
 * Set the I/O callbacks to use netplay's interceding callback system. Should
 * only be called while initializing netplay.
 **/
bool core_set_netplay_callbacks(void)
{
   /* Force normal poll type for netplay. */
   current_core.poll_type = POLL_TYPE_NORMAL;

   /* And use netplay's interceding callbacks */
   current_core.retro_set_video_refresh(video_frame_net);
   current_core.retro_set_audio_sample(audio_sample_net);
   current_core.retro_set_audio_sample_batch(audio_sample_batch_net);
   current_core.retro_set_input_state(input_state_net);

   return true;
}

/**
 * core_unset_netplay_callbacks
 *
 * Unset the I/O callbacks from having used netplay's interceding callback
 * system. Should only be called while uninitializing netplay.
 */
bool core_unset_netplay_callbacks(void)
{
   struct retro_callbacks cbs;
   if (!core_set_default_callbacks(&cbs))
      return false;

   current_core.retro_set_video_refresh(cbs.frame_cb);
   current_core.retro_set_audio_sample(cbs.sample_cb);
   current_core.retro_set_audio_sample_batch(cbs.sample_batch_cb);
   current_core.retro_set_input_state(cbs.state_cb);

   return true;
}
#endif

bool core_set_cheat(retro_ctx_cheat_info_t *info)
{
   current_core.retro_cheat_set(info->index, info->enabled, info->code);
   return true;
}

bool core_reset_cheat(void)
{
   current_core.retro_cheat_reset();
   return true;
}

bool core_api_version(retro_ctx_api_info_t *api)
{
   if (!api)
      return false;
   api->version = current_core.retro_api_version();
   return true;
}

bool core_set_poll_type(unsigned *type)
{
   current_core.poll_type = *type;
   return true;
}

void core_uninit_symbols(void)
{
   uninit_libretro_sym(&current_core);
   current_core.symbols_inited = false;
}

bool core_init_symbols(enum rarch_core_type *type)
{
   if (!type || !init_libretro_sym(*type, &current_core))
      return false;

   if (!current_core.retro_run)
      current_core.retro_run = retro_run_null;
   current_core.symbols_inited = true;
   return true;
}

bool core_set_controller_port_device(retro_ctx_controller_info_t *pad)
{
   if (!pad)
      return false;
   current_core.retro_set_controller_port_device(pad->port, pad->device);
   return true;
}

bool core_get_memory(retro_ctx_memory_info_t *info)
{
   if (!info)
      return false;
   info->size  = current_core.retro_get_memory_size(info->id);
   info->data  = current_core.retro_get_memory_data(info->id);
   return true;
}

bool core_load_game(retro_ctx_load_content_info_t *load_info)
{
   bool contentless = false;
   bool is_inited   = false;

   content_get_status(&contentless, &is_inited);

   if (load_info && load_info->special)
      current_core.game_loaded = current_core.retro_load_game_special(
            load_info->special->id, load_info->info, load_info->content->size);
   else if (load_info && !string_is_empty(load_info->content->elems[0].data))
      current_core.game_loaded = current_core.retro_load_game(load_info->info);
   else if (contentless)
      current_core.game_loaded = current_core.retro_load_game(NULL);
   else
      current_core.game_loaded = false;

   return current_core.game_loaded;
}

bool core_get_system_info(struct retro_system_info *system)
{
   if (!system)
      return false;
   current_core.retro_get_system_info(system);
   return true;
}

bool core_unserialize(retro_ctx_serialize_info_t *info)
{
   if (!info || !current_core.retro_unserialize(info->data_const, info->size))
      return false;

#if HAVE_NETWORKING
   netplay_driver_ctl(RARCH_NETPLAY_CTL_LOAD_SAVESTATE, info);
#endif

   return true;
}

bool core_serialize(retro_ctx_serialize_info_t *info)
{
   if (!info || !current_core.retro_serialize(info->data, info->size))
      return false;
   return true;
}

bool core_serialize_size(retro_ctx_size_info_t *info)
{
   if (!info)
      return false;
   info->size = current_core.retro_serialize_size();
   return true;
}

uint64_t core_serialization_quirks(void)
{
   return current_core.serialization_quirks_v;
}

void core_set_serialization_quirks(uint64_t quirks)
{
   current_core.serialization_quirks_v = quirks;
}

bool core_set_environment(retro_ctx_environ_info_t *info)
{
   if (!info)
      return false;
   current_core.retro_set_environment(info->env);
   return true;
}

bool core_get_system_av_info(struct retro_system_av_info *av_info)
{
   if (!av_info)
      return false;
   current_core.retro_get_system_av_info(av_info);
   return true;
}

bool core_reset(void)
{
   current_core.retro_reset();
   return true;
}

bool core_init(void)
{
   current_core.retro_init();
   current_core.inited          = true;
   return true;
}

bool core_unload(void)
{
   current_core.retro_deinit();
   return true;
}


bool core_unload_game(void)
{
   video_driver_free_hw_context();
   audio_driver_stop();

   current_core.retro_unload_game();

   current_core.game_loaded = false;
   return true;
}

bool core_run(void)
{
#ifdef HAVE_NETWORKING
   if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_PRE_FRAME, NULL))
   {
      /* Paused due to netplay. We must poll and display something so that a
       * netplay peer pausing doesn't just hang. */
      input_poll();
      video_driver_cached_frame();
      return true;
   }
#endif

   switch (current_core.poll_type)
   {
      case POLL_TYPE_EARLY:
         input_poll();
         break;
      case POLL_TYPE_LATE:
         current_core.input_polled = false;
         break;
      default:
         break;
   }

   current_core.retro_run();

   if (current_core.poll_type == POLL_TYPE_LATE && !current_core.input_polled)
      input_poll();

#ifdef HAVE_NETWORKING
   netplay_driver_ctl(RARCH_NETPLAY_CTL_POST_FRAME, NULL);
#endif

   return true;
}

bool core_load(unsigned poll_type_behavior)
{
   current_core.poll_type = poll_type_behavior;

   if (!core_verify_api_version())
      return false;
   if (!core_init_libretro_cbs(&retro_ctx))
      return false;

   core_get_system_av_info(video_viewport_get_system_av_info());

   return true;
}

bool core_verify_api_version(void)
{
   unsigned api_version = current_core.retro_api_version();
   RARCH_LOG("%s: %u\n",
         msg_hash_to_str(MSG_VERSION_OF_LIBRETRO_API),
         api_version);
   RARCH_LOG("%s: %u\n",
         msg_hash_to_str(MSG_COMPILED_AGAINST_API),
         RETRO_API_VERSION);

   if (api_version != RETRO_API_VERSION)
   {
      RARCH_WARN("%s\n", msg_hash_to_str(MSG_LIBRETRO_ABI_BREAK));
      return false;
   }
   return true;
}

bool core_get_region(retro_ctx_region_info_t *info)
{
  if (!info)
    return false;
  info->region = current_core.retro_get_region();
  return true;
}

bool core_has_set_input_descriptor(void)
{
   return current_core.has_set_input_descriptors;
}

void core_set_input_descriptors(void)
{
   current_core.has_set_input_descriptors = true;
}

void core_unset_input_descriptors(void)
{
   current_core.has_set_input_descriptors = false;
}

bool core_is_inited(void)
{
  return current_core.inited;
}

bool core_is_symbols_inited(void)
{
  return current_core.symbols_inited;
}

bool core_is_game_loaded(void)
{
  return current_core.game_loaded;
}
