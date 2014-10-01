/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - Michael Lelli
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

#include "boolean.h"
#include "libretro.h"
#include "retro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "general.h"
#include "performance.h"
#include "input/keyboard_line.h"

static void video_frame(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   const char *msg = NULL;

   if (!driver.video_active)
      return;

   g_extern.frame_cache.data   = data;
   g_extern.frame_cache.width  = width;
   g_extern.frame_cache.height = height;
   g_extern.frame_cache.pitch  = pitch;

   if (g_extern.system.pix_fmt == RETRO_PIXEL_FORMAT_0RGB1555 &&
         data && data != RETRO_HW_FRAME_BUFFER_VALID)
   {
      RARCH_PERFORMANCE_INIT(video_frame_conv);
      RARCH_PERFORMANCE_START(video_frame_conv);
      driver.scaler.in_width = width;
      driver.scaler.in_height = height;
      driver.scaler.out_width = width;
      driver.scaler.out_height = height;
      driver.scaler.in_stride = pitch;
      driver.scaler.out_stride = width * sizeof(uint16_t);

      scaler_ctx_scale(&driver.scaler, driver.scaler_out, data);
      data = driver.scaler_out;
      pitch = driver.scaler.out_stride;
      RARCH_PERFORMANCE_STOP(video_frame_conv);
   }

   /* Slightly messy code,
    * but we really need to do processing before blocking on VSync
    * for best possible scheduling.
    */
   if (driver.recording_data && (!g_extern.filter.filter
            || !g_settings.video.post_filter_record || !data
            || g_extern.record_gpu_buffer)
      )
      rarch_recording_dump_frame(data, width, height, pitch);

   msg = msg_queue_pull(g_extern.msg_queue);
   driver.current_msg = msg;

   if (g_extern.filter.filter && data)
   {
      unsigned owidth  = 0;
      unsigned oheight = 0;
      unsigned opitch  = 0;

      rarch_softfilter_get_output_size(g_extern.filter.filter,
            &owidth, &oheight, width, height);

      opitch = owidth * g_extern.filter.out_bpp;

      RARCH_PERFORMANCE_INIT(softfilter_process);
      RARCH_PERFORMANCE_START(softfilter_process);
      rarch_softfilter_process(g_extern.filter.filter,
            g_extern.filter.buffer, opitch,
            data, width, height, pitch);
      RARCH_PERFORMANCE_STOP(softfilter_process);

      if (driver.recording_data && g_settings.video.post_filter_record)
         rarch_recording_dump_frame(g_extern.filter.buffer,
               owidth, oheight, opitch);

      data = g_extern.filter.buffer;
      width = owidth;
      height = oheight;
      pitch = opitch;
   }

   if (!driver.video->frame(driver.video_data, data, width, height, pitch, msg))
      driver.video_active = false;
}

static void audio_sample(int16_t left, int16_t right)
{
   g_extern.audio_data.conv_outsamples[g_extern.audio_data.data_ptr++] = left;
   g_extern.audio_data.conv_outsamples[g_extern.audio_data.data_ptr++] = right;

   if (g_extern.audio_data.data_ptr < g_extern.audio_data.chunk_size)
      return;

   driver.audio_active = rarch_audio_flush(g_extern.audio_data.conv_outsamples,
         g_extern.audio_data.data_ptr) && driver.audio_active;

   g_extern.audio_data.data_ptr = 0;
}

static size_t audio_sample_batch(const int16_t *data, size_t frames)
{
   if (frames > (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1))
      frames = AUDIO_CHUNK_SIZE_NONBLOCKING >> 1;

   driver.audio_active = rarch_audio_flush(data, frames << 1)
      && driver.audio_active;

   return frames;
}

/* Turbo scheme: If turbo button is held, all buttons pressed except
 * for D-pad will go into a turbo mode. Until the button is
 * released again, the input state will be modulated by a periodic pulse
 * defined by the configured duty cycle. */

static bool input_apply_turbo(unsigned port, unsigned id, bool res)
{
   if (res && g_extern.turbo_frame_enable[port])
      g_extern.turbo_enable[port] |= (1 << id);
   else if (!res)
      g_extern.turbo_enable[port] &= ~(1 << id);

   if (g_extern.turbo_enable[port] & (1 << id))
      return res && ((g_extern.turbo_count % g_settings.input.turbo_period)
            < g_settings.input.turbo_duty_cycle);
   return res;
}

static int16_t input_state(unsigned port, unsigned device,
      unsigned index, unsigned id)
{
   int16_t res = 0;

   device &= RETRO_DEVICE_MASK;

   if (g_extern.bsv.movie && g_extern.bsv.movie_playback)
   {
      int16_t ret;
      if (bsv_movie_get_input(g_extern.bsv.movie, &ret))
         return ret;

      g_extern.bsv.movie_end = true;
   }

   static const struct retro_keybind *binds[MAX_PLAYERS] = {
      g_settings.input.binds[0],
      g_settings.input.binds[1],
      g_settings.input.binds[2],
      g_settings.input.binds[3],
      g_settings.input.binds[4],
      g_settings.input.binds[5],
      g_settings.input.binds[6],
      g_settings.input.binds[7],
      g_settings.input.binds[8],
      g_settings.input.binds[9],
      g_settings.input.binds[10],
      g_settings.input.binds[11],
      g_settings.input.binds[12],
      g_settings.input.binds[13],
      g_settings.input.binds[14],
      g_settings.input.binds[15],
   };

   if (!driver.block_libretro_input &&
         ((id < RARCH_FIRST_META_KEY ||
            device == RETRO_DEVICE_KEYBOARD))
         )
      res = driver.input->input_state(driver.input_data, binds, port,
            device, index, id);

#ifdef HAVE_OVERLAY
   if (device == RETRO_DEVICE_JOYPAD && port == 0)
      res |= driver.overlay_state.buttons & (UINT64_C(1) << id) ? 1 : 0;
   else if (device == RETRO_DEVICE_KEYBOARD && port == 0 && id < RETROK_LAST)
      res |= OVERLAY_GET_KEY(&driver.overlay_state, id) ? 1 : 0;
   else if (device == RETRO_DEVICE_ANALOG && port == 0)
   {
      unsigned base = (index == RETRO_DEVICE_INDEX_ANALOG_RIGHT) ? 2 : 0;
      base += (id == RETRO_DEVICE_ID_ANALOG_Y) ? 1 : 0;
      if (driver.overlay_state.analog[base])
         res = driver.overlay_state.analog[base];
   }
#endif

   /* flushing_input will be cleared in rarch_main_iterate. */
   if (driver.flushing_input)
      res = 0;

   /* Don't allow turbo for D-pad. */
   if (device == RETRO_DEVICE_JOYPAD && (id < RETRO_DEVICE_ID_JOYPAD_UP ||
            id > RETRO_DEVICE_ID_JOYPAD_RIGHT))
      res = input_apply_turbo(port, id, res);

   if (g_extern.bsv.movie && !g_extern.bsv.movie_playback)
      bsv_movie_set_input(g_extern.bsv.movie, res);

   return res;
}

static void audio_sample_rewind(int16_t left, int16_t right)
{
   g_extern.audio_data.rewind_buf[--g_extern.audio_data.rewind_ptr] = right;
   g_extern.audio_data.rewind_buf[--g_extern.audio_data.rewind_ptr] = left;
}

static size_t audio_sample_batch_rewind(const int16_t *data, size_t frames)
{
   size_t i;
   size_t samples = frames << 1;

   for (i = 0; i < samples; i++)
      g_extern.audio_data.rewind_buf[--g_extern.audio_data.rewind_ptr] = data[i];

   return frames;
}

#ifdef HAVE_OVERLAY
static inline void input_poll_overlay(void)
{
   input_overlay_state_t old_key_state;
   unsigned i, j, device;
   uint16_t key_mod = 0;
   bool polled = false;

   memcpy(old_key_state.keys, driver.overlay_state.keys,
         sizeof(driver.overlay_state.keys));
   memset(&driver.overlay_state, 0, sizeof(driver.overlay_state));

   device = input_overlay_full_screen(driver.overlay) ?
      RARCH_DEVICE_POINTER_SCREEN : RETRO_DEVICE_POINTER;

   for (i = 0;
         driver.input->input_state(driver.input_data, NULL, 0, device, i,
            RETRO_DEVICE_ID_POINTER_PRESSED);
         i++)
   {
      int16_t x = driver.input->input_state(driver.input_data, NULL, 0,
            device, i, RETRO_DEVICE_ID_POINTER_X);
      int16_t y = driver.input->input_state(driver.input_data, NULL, 0,
            device, i, RETRO_DEVICE_ID_POINTER_Y);

      input_overlay_state_t polled_data;
      input_overlay_poll(driver.overlay, &polled_data, x, y);

      driver.overlay_state.buttons |= polled_data.buttons;

      for (j = 0; j < ARRAY_SIZE(driver.overlay_state.keys); j++)
         driver.overlay_state.keys[j] |= polled_data.keys[j];

      /* Fingers pressed later take prio and matched up
       * with overlay poll priorities. */
      for (j = 0; j < 4; j++)
         if (polled_data.analog[j])
            driver.overlay_state.analog[j] = polled_data.analog[j];

      polled = true;
   }

   key_mod |= (OVERLAY_GET_KEY(&driver.overlay_state, RETROK_LSHIFT) ||
         OVERLAY_GET_KEY(&driver.overlay_state, RETROK_RSHIFT)) ?
      RETROKMOD_SHIFT : 0;
   key_mod |= (OVERLAY_GET_KEY(&driver.overlay_state, RETROK_LCTRL) ||
         OVERLAY_GET_KEY(&driver.overlay_state, RETROK_RCTRL)) ?
      RETROKMOD_CTRL : 0;
   key_mod |= (OVERLAY_GET_KEY(&driver.overlay_state, RETROK_LALT) ||
         OVERLAY_GET_KEY(&driver.overlay_state, RETROK_RALT)) ?
      RETROKMOD_ALT : 0;
   key_mod |= (OVERLAY_GET_KEY(&driver.overlay_state, RETROK_LMETA) ||
         OVERLAY_GET_KEY(&driver.overlay_state, RETROK_RMETA)) ?
      RETROKMOD_META : 0;

   /* CAPSLOCK SCROLLOCK NUMLOCK */
   for (i = 0; i < ARRAY_SIZE(driver.overlay_state.keys); i++)
   {
      if (driver.overlay_state.keys[i] != old_key_state.keys[i])
      {
         uint32_t orig_bits = old_key_state.keys[i];
         uint32_t new_bits  = driver.overlay_state.keys[i];

         for (j = 0; j < 32; j++)
            if ((orig_bits & (1 << j)) != (new_bits & (1 << j)))
               input_keyboard_event(new_bits & (1 << j), i * 32 + j, 0, key_mod);
      }
   }

   /* Map "analog" buttons to analog axes like regular input drivers do. */
   for (j = 0; j < 4; j++)
   {
      if (!driver.overlay_state.analog[j])
      {
         unsigned bind_plus  = RARCH_ANALOG_LEFT_X_PLUS + 2 * j;
         unsigned bind_minus = bind_plus + 1;
         driver.overlay_state.analog[j] += (driver.overlay_state.buttons &
               (1ULL << bind_plus)) ? 0x7fff : 0;
         driver.overlay_state.analog[j] -= (driver.overlay_state.buttons &
               (1ULL << bind_minus)) ? 0x7fff : 0;
      }
   }

   /* Check for analog_dpad_mode.
    * Map analogs to d-pad buttons when configured. */
   switch (g_settings.input.analog_dpad_mode[0])
   {
      case ANALOG_DPAD_LSTICK:
      case ANALOG_DPAD_RSTICK:
      {
         unsigned analog_base = g_settings.input.analog_dpad_mode[0] == 
            ANALOG_DPAD_LSTICK ? 0 : 2;
         float analog_x = (float)driver.overlay_state.analog[analog_base + 0] / 0x7fff;
         float analog_y = (float)driver.overlay_state.analog[analog_base + 1] / 0x7fff;
         driver.overlay_state.buttons |=
            (analog_x <= -g_settings.input.axis_threshold) ?
            (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) : 0;
         driver.overlay_state.buttons |=
            (analog_x >=  g_settings.input.axis_threshold) ?
            (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT) : 0;
         driver.overlay_state.buttons |=
            (analog_y <= -g_settings.input.axis_threshold) ?
            (1ULL << RETRO_DEVICE_ID_JOYPAD_UP) : 0;
         driver.overlay_state.buttons |=
            (analog_y >=  g_settings.input.axis_threshold) ?
            (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN) : 0;
         break;
      }

      default:
         break;
   }

   if (polled)
      input_overlay_post_poll(driver.overlay);
   else
      input_overlay_poll_clear(driver.overlay);
}
#endif

static void input_poll(void)
{
   driver.input->poll(driver.input_data);

#ifdef HAVE_OVERLAY
   if (driver.overlay)
      input_poll_overlay();
#endif

#ifdef HAVE_COMMAND
   if (driver.command)
      rarch_cmd_poll(driver.command);
#endif
}

void retro_set_default_callbacks(void *data)
{
   struct retro_callbacks *cbs = (struct retro_callbacks*)data;

   if (!cbs)
      return;

   cbs->frame_cb        = video_frame;
   cbs->sample_cb       = audio_sample;
   cbs->sample_batch_cb = audio_sample_batch;
   cbs->state_cb        = input_state;
   cbs->poll_cb         = input_poll;
}

void retro_init_libretro_cbs(void *data)
{
   struct retro_callbacks *cbs = (struct retro_callbacks*)data;

   if (!cbs)
      return;

   pretro_set_video_refresh(video_frame);
   pretro_set_audio_sample(audio_sample);
   pretro_set_audio_sample_batch(audio_sample_batch);
   pretro_set_input_state(input_state);
   pretro_set_input_poll(input_poll);

   retro_set_default_callbacks(cbs);

#ifdef HAVE_NETPLAY
   if (!driver.netplay_data)
      return;

   if (g_extern.netplay_is_spectate)
   {
      pretro_set_input_state(
            (g_extern.netplay_is_client ?
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

void retro_set_rewind_callbacks(void)
{
   if (g_extern.frame_is_reverse)
   {
      pretro_set_audio_sample(audio_sample_rewind);
      pretro_set_audio_sample_batch(audio_sample_batch_rewind);
   }
   else
   {
      pretro_set_audio_sample(audio_sample);
      pretro_set_audio_sample_batch(audio_sample_batch);
   }
}
