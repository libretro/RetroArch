/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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
#include "general.h"
#include "rewind.h"
#include "gfx/video_driver.h"
#include "audio/audio_driver.h"

#ifdef HAVE_NETPLAY
#include "netplay.h"
#endif

/**
 * retro_set_default_callbacks:
 * @data           : pointer to retro_callbacks object
 *
 * Binds the libretro callbacks to default callback functions.
 **/
void retro_set_default_callbacks(void *data)
{
   struct retro_callbacks *cbs = (struct retro_callbacks*)data;

   if (!cbs)
      return;

   cbs->frame_cb        = video_driver_frame;
   cbs->sample_cb       = audio_driver_sample;
   cbs->sample_batch_cb = audio_driver_sample_batch;
   cbs->state_cb        = input_state;
   cbs->poll_cb         = input_poll;
}

/**
 * retro_init_libretro_cbs:
 * @data           : pointer to retro_callbacks object
 *
 * Initializes libretro callbacks, and binds the libretro callbacks 
 * to default callback functions.
 **/
void retro_init_libretro_cbs(void *data)
{
   struct retro_callbacks *cbs = (struct retro_callbacks*)data;
   global_t            *global = global_get_ptr();

   if (!cbs)
      return;

   (void)global;

   core.retro_set_video_refresh(video_driver_frame);
   core.retro_set_audio_sample(audio_driver_sample);
   core.retro_set_audio_sample_batch(audio_driver_sample_batch);
   core.retro_set_input_state(input_state);
   core.retro_set_input_poll(input_poll);

   retro_set_default_callbacks(cbs);

#ifdef HAVE_NETPLAY
   if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
      return;

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
}

/**
 * retro_set_rewind_callbacks:
 *
 * Sets the audio sampling callbacks based on whether or not
 * rewinding is currently activated.
 **/
void retro_set_rewind_callbacks(void)
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
