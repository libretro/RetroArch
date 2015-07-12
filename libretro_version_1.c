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
#include <retro_log.h>
#include <boolean.h>

#include "libretro_version_1.h"
#include "libretro.h"
#include "dynamic.h"
#include "general.h"
#include "runloop.h"
#include "runloop_data.h"
#include "retroarch.h"
#include "performance.h"
#include "input/keyboard_line.h"
#include "input/input_remapping.h"
#include "audio/audio_driver.h"
#include "audio/audio_utils.h"
#include "record/record_driver.h"
#include "gfx/video_pixel_converter.h"

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
   if ((!video_driver_frame_filter_alive()
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

   if (!video->frame(driver->video_data, data, width, height, pitch, driver->current_msg))
      driver->video_active = false;
}

/**
 * input_apply_turbo:
 * @port                 : user number
 * @id                   : identifier of the key
 * @res                  : boolean return value. FIXME/TODO: to be refactored.
 *
 * Apply turbo button if activated.
 *
 * If turbo button is held, all buttons pressed except
 * for D-pad will go into a turbo mode. Until the button is
 * released again, the input state will be modulated by a 
 * periodic pulse defined by the configured duty cycle. 
 *
 * Returns: 1 (true) if turbo button is enabled for this
 * key ID, otherwise the value of @res will be returned.
 *
 **/
static bool input_apply_turbo(unsigned port, unsigned id, bool res)
{
   settings_t *settings           = config_get_ptr();
   global_t *global               = global_get_ptr();

   if (res && global->turbo_frame_enable[port])
      global->turbo_enable[port] |= (1 << id);
   else if (!res)
      global->turbo_enable[port] &= ~(1 << id);

   if (global->turbo_enable[port] & (1 << id))
      return res && ((global->turbo_count % settings->input.turbo_period)
            < settings->input.turbo_duty_cycle);
   return res;
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

   if (!driver->block_libretro_input)
   {
      if (((id < RARCH_FIRST_META_KEY) || (device == RETRO_DEVICE_KEYBOARD)))
         res = input->input_state(driver->input_data, libretro_input_binds, port, device, idx, id);

#ifdef HAVE_OVERLAY
      input_state_overlay(&res, port, device, idx, id);
#endif
   }

   /* flushing_input will be cleared in rarch_main_iterate. */
   if (driver->flushing_input)
      res = 0;

   /* Don't allow turbo for D-pad. */
   if (device == RETRO_DEVICE_JOYPAD && (id < RETRO_DEVICE_ID_JOYPAD_UP ||
            id > RETRO_DEVICE_ID_JOYPAD_RIGHT))
      res = input_apply_turbo(port, id, res);

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

   input->poll(driver->input_data);

   (void)driver;

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

   pretro_set_video_refresh(video_frame);
   pretro_set_audio_sample(audio_driver_sample);
   pretro_set_audio_sample_batch(audio_driver_sample_batch);
   pretro_set_input_state(input_state);
   pretro_set_input_poll(input_poll);

   retro_set_default_callbacks(cbs);

#ifdef HAVE_NETPLAY
   if (!driver->netplay_data)
      return;

   if (global->netplay_is_spectate)
   {
      pretro_set_input_state(
            (global->netplay_is_client ?
             input_state_spectate_client : input_state_spectate)
            );
   }
   else
   {
      pretro_set_video_refresh(video_frame_net);
      pretro_set_audio_sample(audio_sample_net);
      pretro_set_audio_sample_batch(audio_sample_batch_net);
      pretro_set_input_state(input_state_net);
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
   global_t *global = global_get_ptr();

   if (global->rewind.frame_is_reverse)
   {
      pretro_set_audio_sample(audio_driver_sample_rewind);
      pretro_set_audio_sample_batch(audio_driver_sample_batch_rewind);
   }
   else
   {
      pretro_set_audio_sample(audio_driver_sample);
      pretro_set_audio_sample_batch(audio_driver_sample_batch);
   }
}
