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

#include "libretro.h"
#include "libretro_version_1.h"
#include "general.h"
#include "rewind.h"
#include "gfx/video_driver.h"
#include "audio/audio_driver.h"

#ifdef HAVE_NETPLAY
#include "netplay/netplay.h"
#endif

struct retro_callbacks retro_ctx;

static bool input_polled;

static int16_t input_state_poll(unsigned port,
      unsigned device, unsigned idx, unsigned id)
{
   if (core.poll_type == POLL_TYPE_LATE)
   {
      if (!input_polled)
         input_poll();

      input_polled = true;
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

static bool retro_uninit_libretro_cbs(void)
{
   struct retro_callbacks *cbs = (struct retro_callbacks*)&retro_ctx;

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
   if (core.poll_type == POLL_TYPE_NORMAL)
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
   core.poll_type = POLL_TYPE_NORMAL;

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
   switch (state)
   {
      case CORE_CTL_RETRO_RUN:
         switch (core.poll_type)
         {
            case POLL_TYPE_EARLY:
               input_poll();
               break;
            case POLL_TYPE_LATE:
               input_polled = false;
               break;
         }
         core.retro_run();
         break;
      case CORE_CTL_SET_CBS:
         return retro_set_default_callbacks(data);
      case CORE_CTL_SET_CBS_REWIND:
         retro_set_rewind_callbacks();
         break;
      case CORE_CTL_INIT:
         return retro_init_libretro_cbs(data);
      case CORE_CTL_DEINIT:
         return retro_uninit_libretro_cbs();
      case CORE_CTL_NONE:
      default:
         break;
   }

   return true;
}
