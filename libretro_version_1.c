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
#include "runloop.h"
#include "runloop_data.h"
#include "retroarch.h"
#include "rewind.h"
#include "performance.h"
#include "input/input_remapping.h"
#include "record/record_driver.h"
#include "verbosity.h"

#ifdef HAVE_NETPLAY
#include "netplay.h"
#endif

/**
 * video_frame:
 * @data                 : pointer to data of the video frame.
 * @width                : width of the video frame.
 * @height               : height of the video frame.
 * @pitch                : pitch of the video frame.
 *
 * Video frame render callback function.
 **/
static void video_frame(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   uint64_t *frame_count  = NULL;
   unsigned output_width  = 0;
   unsigned output_height = 0;
   unsigned  output_pitch = 0;
   const char *msg        = NULL;
   driver_t  *driver      = driver_get_ptr();
   global_t  *global      = global_get_ptr();
   settings_t *settings   = config_get_ptr();
   const video_driver_t *video = 
      driver ? (const video_driver_t*)driver->video : NULL;

   if (!driver->video_active)
      return;

   if (video_pixel_frame_scale(data, width, height, pitch))
   {
      video_pixel_scaler_t *scaler = scaler_get_ptr();

      data                        = scaler->scaler_out;
      pitch                       = scaler->scaler->out_stride;
   }

   video_driver_cached_frame_set(data, width, height, pitch);

   /* Slightly messy code,
    * but we really need to do processing before blocking on VSync
    * for best possible scheduling.
    */
   if ((!video_driver_ctl(RARCH_DISPLAY_CTL_FRAME_FILTER_ALIVE, NULL)
            || !settings->video.post_filter_record || !data
            || global->record.gpu_buffer)
      )
      recording_dump_frame(data, width, height, pitch);

   msg                = rarch_main_msg_queue_pull();

   *driver->current_msg = 0;

   if (msg)
      strlcpy(driver->current_msg, msg, sizeof(driver->current_msg));

   if (video_driver_frame_filter(data, width, height, pitch,
            &output_width, &output_height, &output_pitch))
   {
      data   = video_driver_frame_filter_get_buf_ptr();
      width  = output_width;
      height = output_height;
      pitch  = output_pitch;
   }

   video_driver_ctl(RARCH_DISPLAY_CTL_GET_FRAME_COUNT, &frame_count);

   if (!video->frame(driver->video_data, data, width, height, *frame_count,
            pitch, driver->current_msg))
      driver->video_active = false;

   *frame_count = *frame_count + 1;
}

/**
 * input_state:
 * @port                 : user number.
 * @device               : device identifier of user.
 * @idx                  : index value of user.
 * @id                   : identifier of key pressed by user.
 *
 * Input state callback function.
 *
 * Returns: Non-zero if the given key (identified by @id) was pressed by the user
 * (assigned to @port).
 **/
static int16_t input_state(unsigned port, unsigned device,
      unsigned idx, unsigned id)
{
   size_t i;
   const struct retro_keybind *libretro_input_binds[MAX_USERS];
   int16_t res                     = 0;
   settings_t *settings            = config_get_ptr();
   driver_t *driver                = driver_get_ptr();
   global_t *global                = global_get_ptr();
   const input_driver_t *input     = driver ? 
      (const input_driver_t*)driver->input : NULL;
   
   for (i = 0; i < MAX_USERS; i++)
      libretro_input_binds[i] = settings->input.binds[i];

   device &= RETRO_DEVICE_MASK;

   if (global->bsv.movie && global->bsv.movie_playback)
   {
      int16_t ret;
      if (bsv_movie_get_input(global->bsv.movie, &ret))
         return ret;

      global->bsv.movie_end = true;
   }

   if (settings->input.remap_binds_enable)
      input_remapping_state(port, &device, &idx, &id);

   if (!driver->flushing_input && !driver->block_libretro_input)
   {
      if (((id < RARCH_FIRST_META_KEY) || (device == RETRO_DEVICE_KEYBOARD)))
         res = input->input_state(driver->input_data, libretro_input_binds, port, device, idx, id);

#ifdef HAVE_OVERLAY
      input_state_overlay(&res, port, device, idx, id);
#endif
   }

   /* Don't allow turbo for D-pad. */
   if (device == RETRO_DEVICE_JOYPAD && (id < RETRO_DEVICE_ID_JOYPAD_UP ||
            id > RETRO_DEVICE_ID_JOYPAD_RIGHT))
   {
      /*
       * Apply turbo button if activated.
       *
       * If turbo button is held, all buttons pressed except
       * for D-pad will go into a turbo mode. Until the button is
       * released again, the input state will be modulated by a 
       * periodic pulse defined by the configured duty cycle. 
       */
      if (res && global->turbo.frame_enable[port])
         global->turbo.enable[port] |= (1 << id);
      else if (!res)
         global->turbo.enable[port] &= ~(1 << id);

      if (global->turbo.enable[port] & (1 << id))
      {
         /* if turbo button is enabled for this key ID */
         res = res && ((global->turbo.count % settings->input.turbo_period)
               < settings->input.turbo_duty_cycle);
      }
   }

   if (global->bsv.movie && !global->bsv.movie_playback)
      bsv_movie_set_input(global->bsv.movie, res);

   return res;
}

/**
 * input_poll:
 *
 * Input polling callback function.
 **/
static void input_poll(void)
{
   driver_t *driver               = driver_get_ptr();
   settings_t *settings           = config_get_ptr();
   const input_driver_t *input     = driver ? 
      (const input_driver_t*)driver->input : NULL;

   (void)settings;

   input->poll(driver->input_data);

#ifdef HAVE_OVERLAY
   input_poll_overlay(settings->input.overlay_opacity);
#endif

#ifdef HAVE_COMMAND
   if (driver->command)
      rarch_cmd_poll(driver->command);
#endif
}

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

   cbs->frame_cb        = video_frame;
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
   driver_t            *driver = driver_get_ptr();
   global_t            *global = global_get_ptr();

   if (!cbs)
      return;

   (void)driver;
   (void)global;

   core.retro_set_video_refresh(video_frame);
   core.retro_set_audio_sample(audio_driver_sample);
   core.retro_set_audio_sample_batch(audio_driver_sample_batch);
   core.retro_set_input_state(input_state);
   core.retro_set_input_poll(input_poll);

   retro_set_default_callbacks(cbs);

#ifdef HAVE_NETPLAY
   if (!driver->netplay_data)
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
