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

   if (!video_driver_frame(data, width, height, pitch, driver->current_msg))
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
   input_overlay_state_t *ol_state = input_overlay_get_state_ptr();
   
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
         res = input_driver_state(libretro_input_binds, port, device, idx, id);

#ifdef HAVE_OVERLAY
      if (port == 0)
      {
         switch (device)
         {
            case RETRO_DEVICE_JOYPAD:
               if (ol_state->buttons & (UINT64_C(1) << id))
                  res |= 1;
               break;
            case RETRO_DEVICE_KEYBOARD:
               if (id < RETROK_LAST)
               {
                  if (OVERLAY_GET_KEY(ol_state, id))
                     res |= 1;
               }
               break;
            case RETRO_DEVICE_ANALOG:
               {
                  unsigned base = 0;
                  
                  if (idx == RETRO_DEVICE_INDEX_ANALOG_RIGHT)
                     base = 2;
                  if (id == RETRO_DEVICE_ID_ANALOG_Y)
                     base += 1;
                  if (ol_state->analog[base])
                     res = ol_state->analog[base];
               }
               break;
         }
      }
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


#ifdef HAVE_OVERLAY
/*
 * input_poll_overlay:
 * @overlay_device : pointer to overlay 
 *
 * Poll pressed buttons/keys on currently active overlay.
 **/
static INLINE void input_poll_overlay(
      input_overlay_t *overlay_device, float opacity)
{
   input_overlay_state_t old_key_state;
   unsigned i, j, device;
   uint16_t key_mod                = 0;
   bool polled                     = false;
   driver_t *driver                = driver_get_ptr();
   settings_t *settings            = config_get_ptr();
   input_overlay_state_t *ol_state = input_overlay_get_state_ptr();

   if (overlay_device->state != OVERLAY_STATUS_ALIVE || !ol_state)
      return;

   memcpy(old_key_state.keys, ol_state->keys,
         sizeof(ol_state->keys));
   memset(ol_state, 0, sizeof(*ol_state));

   device = input_overlay_full_screen(overlay_device) ?
      RARCH_DEVICE_POINTER_SCREEN : RETRO_DEVICE_POINTER;

   for (i = 0;
         input_driver_state(NULL, 0, device, i,
            RETRO_DEVICE_ID_POINTER_PRESSED);
         i++)
   {
      input_overlay_state_t polled_data;
      int16_t x = input_driver_state(NULL, 0,
            device, i, RETRO_DEVICE_ID_POINTER_X);
      int16_t y = input_driver_state(NULL, 0,
            device, i, RETRO_DEVICE_ID_POINTER_Y);

      input_overlay_poll(overlay_device, &polled_data, x, y);

      ol_state->buttons |= polled_data.buttons;

      for (j = 0; j < ARRAY_SIZE(ol_state->keys); j++)
         ol_state->keys[j] |= polled_data.keys[j];

      /* Fingers pressed later take prio and matched up
       * with overlay poll priorities. */
      for (j = 0; j < 4; j++)
         if (polled_data.analog[j])
            ol_state->analog[j] = polled_data.analog[j];

      polled = true;
   }

   if (OVERLAY_GET_KEY(ol_state, RETROK_LSHIFT) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RSHIFT))
      key_mod |= RETROKMOD_SHIFT;

   if (OVERLAY_GET_KEY(ol_state, RETROK_LCTRL) ||
    OVERLAY_GET_KEY(ol_state, RETROK_RCTRL))
      key_mod |= RETROKMOD_CTRL;

   if (OVERLAY_GET_KEY(ol_state, RETROK_LALT) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RALT))
      key_mod |= RETROKMOD_ALT;

   if (OVERLAY_GET_KEY(ol_state, RETROK_LMETA) ||
         OVERLAY_GET_KEY(ol_state, RETROK_RMETA))
      key_mod |= RETROKMOD_META;

   /* CAPSLOCK SCROLLOCK NUMLOCK */
   for (i = 0; i < ARRAY_SIZE(ol_state->keys); i++)
   {
      if (ol_state->keys[i] != old_key_state.keys[i])
      {
         uint32_t orig_bits = old_key_state.keys[i];
         uint32_t new_bits  = ol_state->keys[i];

         for (j = 0; j < 32; j++)
            if ((orig_bits & (1 << j)) != (new_bits & (1 << j)))
               input_keyboard_event(new_bits & (1 << j),
                     i * 32 + j, 0, key_mod, RETRO_DEVICE_POINTER);
      }
   }

   /* Map "analog" buttons to analog axes like regular input drivers do. */
   for (j = 0; j < 4; j++)
   {
      unsigned bind_plus  = RARCH_ANALOG_LEFT_X_PLUS + 2 * j;
      unsigned bind_minus = bind_plus + 1;

      if (ol_state->analog[j])
         continue;

      if (ol_state->buttons & (1UL << bind_plus))
         ol_state->analog[j] += 0x7fff;
      if (ol_state->buttons & (1UL << bind_minus))
         ol_state->analog[j] -= 0x7fff;
   }

   /* Check for analog_dpad_mode.
    * Map analogs to d-pad buttons when configured. */
   switch (settings->input.analog_dpad_mode[0])
   {
      case ANALOG_DPAD_LSTICK:
      case ANALOG_DPAD_RSTICK:
      {
         float analog_x, analog_y;
         unsigned analog_base = 2;

         if (settings->input.analog_dpad_mode[0] == ANALOG_DPAD_LSTICK)
            analog_base = 0;

         analog_x = (float)ol_state->analog[analog_base + 0] / 0x7fff;
         analog_y = (float)ol_state->analog[analog_base + 1] / 0x7fff;

         if (analog_x <= -settings->input.axis_threshold)
            ol_state->buttons |= (1UL << RETRO_DEVICE_ID_JOYPAD_LEFT);
         if (analog_x >=  settings->input.axis_threshold)
            ol_state->buttons |= (1UL << RETRO_DEVICE_ID_JOYPAD_RIGHT);
         if (analog_y <= -settings->input.axis_threshold)
            ol_state->buttons |= (1UL << RETRO_DEVICE_ID_JOYPAD_UP);
         if (analog_y >=  settings->input.axis_threshold)
            ol_state->buttons |= (1UL << RETRO_DEVICE_ID_JOYPAD_DOWN);
         break;
      }

      default:
         break;
   }

   if (polled)
      input_overlay_post_poll(overlay_device, opacity);
   else
      input_overlay_poll_clear(overlay_device, opacity);
}
#endif

/**
 * input_poll:
 *
 * Input polling callback function.
 **/
static void input_poll(void)
{
   driver_t *driver               = driver_get_ptr();
   settings_t *settings           = config_get_ptr();
   input_overlay_t *overlay       = input_overlay_get_ptr();

   input_driver_poll();

#ifdef HAVE_OVERLAY
   if (overlay)
      input_poll_overlay(overlay,
            settings->input.overlay_opacity);
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
