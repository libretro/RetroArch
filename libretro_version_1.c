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

static void video_frame(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   const char *msg = NULL;

   if (!g_extern.video_active)
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
   if (g_extern.rec && (!g_extern.filter.filter
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

      if (g_extern.rec && g_settings.video.post_filter_record)
         rarch_recording_dump_frame(g_extern.filter.buffer,
               owidth, oheight, opitch);

      data = g_extern.filter.buffer;
      width = owidth;
      height = oheight;
      pitch = opitch;
   }

   if (!driver.video->frame(driver.video_data, data, width, height, pitch, msg))
      g_extern.video_active = false;
}

static void audio_sample(int16_t left, int16_t right)
{
   g_extern.audio_data.conv_outsamples[g_extern.audio_data.data_ptr++] = left;
   g_extern.audio_data.conv_outsamples[g_extern.audio_data.data_ptr++] = right;

   if (g_extern.audio_data.data_ptr < g_extern.audio_data.chunk_size)
      return;

   g_extern.audio_active = rarch_audio_flush(g_extern.audio_data.conv_outsamples,
         g_extern.audio_data.data_ptr) && g_extern.audio_active;

   g_extern.audio_data.data_ptr = 0;
}

static size_t audio_sample_batch(const int16_t *data, size_t frames)
{
   if (frames > (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1))
      frames = AUDIO_CHUNK_SIZE_NONBLOCKING >> 1;

   g_extern.audio_active = rarch_audio_flush(data, frames << 1)
      && g_extern.audio_active;

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

   if (!driver.block_libretro_input && (id < RARCH_FIRST_META_KEY ||
            device == RETRO_DEVICE_KEYBOARD))
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

void retro_set_default_callbacks(void *data)
{
   struct retro_callbacks *cbs = (struct retro_callbacks*)data;

   if (!cbs)
      return;

   cbs->frame_cb        = video_frame;
   cbs->sample_cb       = audio_sample;
   cbs->sample_batch_cb = audio_sample_batch;
   cbs->state_cb        = input_state;
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
   pretro_set_input_poll(rarch_input_poll);

   retro_set_default_callbacks(cbs);

#ifdef HAVE_NETPLAY
   if (!g_extern.netplay)
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
