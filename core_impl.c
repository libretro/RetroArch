/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2016 - Brad Parker
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
#include "dynamic.h"
#include "msg_hash.h"
#include "managers/state_manager.h"
#include "verbosity.h"
#include "gfx/video_driver.h"
#include "audio/audio_driver.h"

static unsigned            core_poll_type                 = POLL_TYPE_EARLY;
static bool                core_inited                    = false;
static bool                core_symbols_inited            = false;
static bool                core_game_loaded               = false;
static bool                core_input_polled              = false;
static bool                core_has_set_input_descriptors = false;
static uint64_t            core_serialization_quirks_v    = 0;

static struct              retro_callbacks retro_ctx;
static struct              retro_core_t core;

static void core_input_state_poll_maybe(void)
{
   if (core_poll_type == POLL_TYPE_NORMAL)
      input_poll();
}

static int16_t core_input_state_poll(unsigned port,
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

void core_set_input_state(retro_ctx_input_state_info_t *info)
{
   core.retro_set_input_state(info->cb);
}

/**
 * core_init_libretro_cbs:
 * @data           : pointer to retro_callbacks object
 *
 * Initializes libretro callbacks, and binds the libretro callbacks
 * to default callback functions.
 **/
static bool core_init_libretro_cbs(void *data)
{
   struct retro_callbacks *cbs = (struct retro_callbacks*)data;

   if (!cbs)
      return false;

   core.retro_set_video_refresh(video_driver_frame);
   core.retro_set_audio_sample(audio_driver_sample);
   core.retro_set_audio_sample_batch(audio_driver_sample_batch);
   core.retro_set_input_state(core_input_state_poll);
   core.retro_set_input_poll(core_input_state_poll_maybe);

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
bool core_set_default_callbacks(void *data)
{
   struct retro_callbacks *cbs = (struct retro_callbacks*)data;

   if (!cbs)
      return false;

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

   cbs->frame_cb        = NULL;
   cbs->sample_cb       = NULL;
   cbs->sample_batch_cb = NULL;
   cbs->state_cb        = NULL;
   cbs->poll_cb         = NULL;

   core_inited          = false;

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
      core.retro_set_audio_sample(audio_driver_sample_rewind);
      core.retro_set_audio_sample_batch(audio_driver_sample_batch_rewind);
   }
   else
   {
      core.retro_set_audio_sample(audio_driver_sample);
      core.retro_set_audio_sample_batch(audio_driver_sample_batch);
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
   core_poll_type = POLL_TYPE_NORMAL;

   /* And use netplay's interceding callbacks */
   core.retro_set_video_refresh(video_frame_net);
   core.retro_set_audio_sample(audio_sample_net);
   core.retro_set_audio_sample_batch(audio_sample_batch_net);
   core.retro_set_input_state(input_state_net);

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

   core.retro_set_video_refresh(cbs.frame_cb);
   core.retro_set_audio_sample(cbs.sample_cb);
   core.retro_set_audio_sample_batch(cbs.sample_batch_cb);
   core.retro_set_input_state(cbs.state_cb);

   return true;
}
#endif

bool core_set_cheat(retro_ctx_cheat_info_t *info)
{
   core.retro_cheat_set(info->index, info->enabled, info->code);
   return true;
}

bool core_reset_cheat(void)
{
   core.retro_cheat_reset();
   return true;
}

bool core_api_version(retro_ctx_api_info_t *api)
{
   if (!api)
      return false;
   api->version = core.retro_api_version();
   return true;
}

bool core_set_poll_type(unsigned *type)
{
   core_poll_type = *type;
   return true;
}

void core_uninit_symbols(void)
{
   uninit_libretro_sym(&core);
   core_symbols_inited = false;
}

bool core_init_symbols(enum rarch_core_type *type)
{
   if (!type)
      return false;
   if (!init_libretro_sym(*type, &core))
      return false;
   core_symbols_inited = true;
   return true;
}

bool core_set_controller_port_device(retro_ctx_controller_info_t *pad)
{
   if (!pad)
      return false;
   core.retro_set_controller_port_device(pad->port, pad->device);
   return true;
}

bool core_get_memory(retro_ctx_memory_info_t *info)
{
   if (!info)
      return false;
   info->size  = core.retro_get_memory_size(info->id);
   info->data  = core.retro_get_memory_data(info->id);
   return true;
}

bool core_load_game(retro_ctx_load_content_info_t *load_info)
{
   if (load_info && load_info->special)
      core_game_loaded = core.retro_load_game_special(
            load_info->special->id, load_info->info, load_info->content->size);
   else if (load_info && !string_is_empty(load_info->content->elems[0].data))
      core_game_loaded = core.retro_load_game(load_info->info);
   else
      core_game_loaded = core.retro_load_game(NULL);

   return core_game_loaded;
}

bool core_get_system_info(struct retro_system_info *system)
{
   if (!system)
      return false;
   core.retro_get_system_info(system);
   return true;
}

bool core_unserialize(retro_ctx_serialize_info_t *info)
{
   if (!info)
      return false;
   if (!core.retro_unserialize(info->data_const, info->size))
      return false;

#if HAVE_NETWORKING
   netplay_driver_ctl(RARCH_NETPLAY_CTL_LOAD_SAVESTATE, info);
#endif

   return true;
}

bool core_serialize(retro_ctx_serialize_info_t *info)
{
   if (!info)
      return false;
   if (!core.retro_serialize(info->data, info->size))
      return false;
   return true;
}

bool core_serialize_size(retro_ctx_size_info_t *info)
{
   if (!info)
      return false;
   info->size = core.retro_serialize_size();
   return true;
}

uint64_t core_serialization_quirks(void)
{
   return core_serialization_quirks_v;
}

void core_set_serialization_quirks(uint64_t quirks)
{
   core_serialization_quirks_v = quirks;
}

void core_frame(retro_ctx_frame_info_t *info)
{
   if (retro_ctx.frame_cb)
      retro_ctx.frame_cb(
            info->data, info->width, info->height, info->pitch);
}

bool core_poll(void)
{
   if (!retro_ctx.poll_cb)
      return false;
   retro_ctx.poll_cb();
   return true;
}

bool core_set_environment(retro_ctx_environ_info_t *info)
{
   if (!info)
      return false;
   core.retro_set_environment(info->env);
   return true;
}

bool core_get_system_av_info(struct retro_system_av_info *av_info)
{
   if (!av_info)
      return false;
   core.retro_get_system_av_info(av_info);
   return true;
}

bool core_reset(void)
{
   core.retro_reset();
   return true;
}

bool core_init(void)
{
   core.retro_init();
   core_inited          = true;
   return true;
}

bool core_unload(void)
{
   core.retro_deinit();
   return true;
}


bool core_unload_game(void)
{
   video_driver_deinit_hw_context();
   audio_driver_stop();
   core.retro_unload_game();
   core_game_loaded = false;
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

   switch (core_poll_type)
   {
      case POLL_TYPE_EARLY:
         input_poll();
         break;
      case POLL_TYPE_LATE:
         core_input_polled = false;
         break;
      default:
         break;
   }

   if (core.retro_run)
      core.retro_run();
   if (core_poll_type == POLL_TYPE_LATE && !core_input_polled)
      input_poll();

#ifdef HAVE_NETWORKING
   netplay_driver_ctl(RARCH_NETPLAY_CTL_POST_FRAME, NULL);
#endif

   return true;
}

bool core_load(unsigned poll_type_behavior)
{
   core_poll_type = poll_type_behavior;

   if (!core_verify_api_version())
      return false;
   if (!core_init_libretro_cbs(&retro_ctx))
      return false;

   core_get_system_av_info(video_viewport_get_system_av_info());

   return true;
}

bool core_verify_api_version(void)
{
   unsigned api_version = core.retro_api_version();
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
  info->region = core.retro_get_region();
  return true;
}

bool core_has_set_input_descriptor(void)
{
   return core_has_set_input_descriptors;
}

void core_set_input_descriptors(void)
{
   core_has_set_input_descriptors = true;
}

void core_unset_input_descriptors(void)
{
   core_has_set_input_descriptors = false;
}

bool core_is_inited(void)
{
  return core_inited;
}

bool core_is_symbols_inited(void)
{
  return core_symbols_inited;
}

bool core_is_game_loaded(void)
{
  return core_game_loaded;
}
