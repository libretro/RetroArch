/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <string.h>

#include <retro_assert.h>

#include <lists/string_list.h>

#include "audio_driver.h"
#include "audio_resampler_driver.h"
#include "../record/record_driver.h"
#include "audio_utils.h"
#include "audio_thread_wrapper.h"

#include "../command_event.h"
#include "../configuration.h"
#include "../retroarch.h"
#include "../runloop.h"
#include "../performance.h"
#include "../verbosity.h"
#include "../list_special.h"

#ifndef AUDIO_BUFFER_FREE_SAMPLES_COUNT
#define AUDIO_BUFFER_FREE_SAMPLES_COUNT (8 * 1024)
#endif

typedef struct audio_driver_input_data
{
   float *data;

   size_t data_ptr;
   struct
   {
      size_t size;
      size_t nonblock_size;
      size_t block_size;
   } chunk;


   struct
   {
      float input;
      bool  control; 
      struct
      {
         double original;
         double current;
      } source_ratio;
   } audio_rate;

   bool use_float;

   struct
   {
      int16_t *buf;
      size_t ptr;
      size_t size;
   } rewind;

   rarch_dsp_filter_t *dsp;

   size_t driver_buffer_size;

   float volume_gain;

   struct
   {
      float *buf;
      int16_t *conv_buf;
   } output_samples;

   struct
   {
      unsigned buf[AUDIO_BUFFER_FREE_SAMPLES_COUNT];
      uint64_t count;
   } free_samples;
} audio_driver_input_data_t;

static const audio_driver_t *audio_drivers[] = {
#ifdef HAVE_ALSA
   &audio_alsa,
#ifndef __QNX__
   &audio_alsathread,
#endif
#endif
#if defined(HAVE_OSS) || defined(HAVE_OSS_BSD)
   &audio_oss,
#endif
#ifdef HAVE_RSOUND
   &audio_rsound,
#endif
#ifdef HAVE_COREAUDIO
   &audio_coreaudio,
#endif
#ifdef HAVE_AL
   &audio_openal,
#endif
#ifdef HAVE_SL
   &audio_opensl,
#endif
#ifdef HAVE_ROAR
   &audio_roar,
#endif
#ifdef HAVE_JACK
   &audio_jack,
#endif
#if defined(HAVE_SDL) || defined(HAVE_SDL2)
   &audio_sdl,
#endif
#ifdef HAVE_XAUDIO
   &audio_xa,
#endif
#ifdef HAVE_DSOUND
   &audio_dsound,
#endif
#ifdef HAVE_PULSE
   &audio_pulse,
#endif
#ifdef __CELLOS_LV2__
   &audio_ps3,
#endif
#ifdef XENON
   &audio_xenon360,
#endif
#ifdef GEKKO
   &audio_gx,
#endif
#ifdef EMSCRIPTEN
   &audio_rwebaudio,
#endif
#if defined(PSP) || defined(VITA)
   &audio_psp,
#endif   
#ifdef _3DS
   &audio_ctr_csnd,
   &audio_ctr_dsp,
#endif
   &audio_null,
   NULL,
};

static audio_driver_input_data_t audio_driver_data;
static struct retro_audio_callback audio_callback;
static struct string_list *audio_driver_devices_list   = NULL;
static struct retro_perf_counter resampler_proc        = {0};
static const rarch_resampler_t *audio_driver_resampler = NULL;
static void *audio_driver_resampler_data               = NULL;
static bool audio_driver_active                        = false;
static bool audio_driver_data_own                      = false;
static const audio_driver_t *current_audio             = NULL;
static void *audio_driver_context_audio_data           = NULL;

/**
 * compute_audio_buffer_statistics:
 *
 * Computes audio buffer statistics.
 *
 **/
static void compute_audio_buffer_statistics(void)
{
   unsigned i, low_water_size, high_water_size, avg, stddev;
   float avg_filled, deviation;
   uint64_t accum                = 0;
   uint64_t accum_var            = 0;
   unsigned low_water_count      = 0;
   unsigned high_water_count     = 0;
   unsigned samples              = MIN(
         audio_driver_data.free_samples.count,
         AUDIO_BUFFER_FREE_SAMPLES_COUNT);

   if (samples < 3)
      return;

   for (i = 1; i < samples; i++)
      accum += audio_driver_data.free_samples.buf[i];

   avg = accum / (samples - 1);

   for (i = 1; i < samples; i++)
   {
      int diff = avg - audio_driver_data.free_samples.buf[i];
      accum_var += diff * diff;
   }

   stddev          = (unsigned)sqrt((double)accum_var / (samples - 2));
   avg_filled      = 1.0f - (float)avg / audio_driver_data.driver_buffer_size;
   deviation       = (float)stddev / audio_driver_data.driver_buffer_size;

   low_water_size  = audio_driver_data.driver_buffer_size * 3 / 4;
   high_water_size = audio_driver_data.driver_buffer_size / 4;

   for (i = 1; i < samples; i++)
   {
      if (audio_driver_data.free_samples.buf[i] >= low_water_size)
         low_water_count++;
      else if (audio_driver_data.free_samples.buf[i] <= high_water_size)
         high_water_count++;
   }

   RARCH_LOG("Average audio buffer saturation: %.2f %%, standard deviation (percentage points): %.2f %%.\n",
         avg_filled * 100.0, deviation * 100.0);
   RARCH_LOG("Amount of time spent close to underrun: %.2f %%. Close to blocking: %.2f %%.\n",
         (100.0 * low_water_count) / (samples - 1),
         (100.0 * high_water_count) / (samples - 1));
}

/**
 * audio_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to audio driver at index. Can be NULL
 * if nothing found.
 **/
const void *audio_driver_find_handle(int idx)
{
   const void *drv = audio_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * audio_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of audio driver at index. Can be NULL
 * if nothing found.
 **/
const char *audio_driver_find_ident(int idx)
{
   const audio_driver_t *drv = audio_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * config_get_audio_driver_options:
 *
 * Get an enumerated list of all audio driver names, separated by '|'.
 *
 * Returns: string listing of all audio driver names, separated by '|'.
 **/
const char *config_get_audio_driver_options(void)
{
   return char_list_new_special(STRING_LIST_AUDIO_DRIVERS, NULL);
}

static bool uninit_audio(void)
{
   settings_t *settings = config_get_ptr();

   if (current_audio && current_audio->free)
   {
      if (audio_driver_context_audio_data)
         current_audio->free(audio_driver_context_audio_data);
      audio_driver_context_audio_data = NULL;
   }

   if (audio_driver_data.output_samples.conv_buf)
      free(audio_driver_data.output_samples.conv_buf);
   audio_driver_data.output_samples.conv_buf = NULL;
   audio_driver_data.data_ptr                = 0;

   if (audio_driver_data.rewind.buf)
      free(audio_driver_data.rewind.buf);
   audio_driver_data.rewind.buf   = NULL;
   audio_driver_data.rewind.size  = 0;

   if (!settings->audio.enable)
   {
      audio_driver_ctl(RARCH_AUDIO_CTL_UNSET_ACTIVE, NULL);
      return false;
   }

   audio_driver_deinit_resampler();

   if (audio_driver_data.data)
      free(audio_driver_data.data);
   audio_driver_data.data = NULL;

   if (audio_driver_data.output_samples.buf)
      free(audio_driver_data.output_samples.buf);
   audio_driver_data.output_samples.buf = NULL;

   event_cmd_ctl(EVENT_CMD_DSP_FILTER_DEINIT, NULL);

   compute_audio_buffer_statistics();

   return true;
}

static bool audio_driver_init_internal(bool audio_cb_inited)
{
   size_t outsamples_max, max_bufsamples = AUDIO_CHUNK_SIZE_NONBLOCKING * 2;
   settings_t *settings = config_get_ptr();

   audio_convert_init_simd();

   /* Accomodate rewind since at some point we might have two full buffers. */
   outsamples_max = max_bufsamples * AUDIO_MAX_RATIO * 
      settings->slowmotion_ratio;

   /* Used for recording even if audio isn't enabled. */
   retro_assert(audio_driver_data.output_samples.conv_buf =
         (int16_t*)malloc(outsamples_max * sizeof(int16_t)));

   if (!audio_driver_data.output_samples.conv_buf)
      goto error;

   audio_driver_data.chunk.block_size    = AUDIO_CHUNK_SIZE_BLOCKING;
   audio_driver_data.chunk.nonblock_size = AUDIO_CHUNK_SIZE_NONBLOCKING;
   audio_driver_data.chunk.size          = audio_driver_data.chunk.block_size;

   /* Needs to be able to hold full content of a full max_bufsamples
    * in addition to its own. */
   retro_assert(audio_driver_data.rewind.buf = (int16_t*)
         malloc(max_bufsamples * sizeof(int16_t)));

   if (!audio_driver_data.rewind.buf)
      goto error;

   audio_driver_data.rewind.size             = max_bufsamples;

   if (!settings->audio.enable)
   {
      audio_driver_ctl(RARCH_AUDIO_CTL_UNSET_ACTIVE, NULL);
      return false;
   }

   audio_driver_find_driver();
#ifdef HAVE_THREADS
   if (audio_cb_inited)
   {
      RARCH_LOG("Starting threaded audio driver ...\n");
      if (!rarch_threaded_audio_init(
               &current_audio,
               &audio_driver_context_audio_data,
               *settings->audio.device ? settings->audio.device : NULL,
               settings->audio.out_rate, settings->audio.latency,
               current_audio))
      {
         RARCH_ERR("Cannot open threaded audio driver ... Exiting ...\n");
         retro_fail(1, "audio_driver_init_internal()");
      }
   }
   else
#endif
   {
      audio_driver_context_audio_data = 
         current_audio->init(*settings->audio.device ?
               settings->audio.device : NULL,
               settings->audio.out_rate, settings->audio.latency);
   }

   if (!audio_driver_context_audio_data)
   {
      RARCH_ERR("Failed to initialize audio driver. Will continue without audio.\n");
      audio_driver_ctl(RARCH_AUDIO_CTL_UNSET_ACTIVE, NULL);
   }

   audio_driver_data.use_float = false;
   if (     audio_driver_ctl(RARCH_AUDIO_CTL_IS_ACTIVE, NULL) 
         && current_audio->use_float(audio_driver_context_audio_data))
      audio_driver_data.use_float = true;

   if (!settings->audio.sync && audio_driver_ctl(RARCH_AUDIO_CTL_IS_ACTIVE, NULL))
   {
      event_cmd_ctl(EVENT_CMD_AUDIO_SET_NONBLOCKING_STATE, NULL);
      audio_driver_data.chunk.size = audio_driver_data.chunk.nonblock_size;
   }

   if (audio_driver_data.audio_rate.input <= 0.0f)
   {
      /* Should never happen. */
      RARCH_WARN("Input rate is invalid (%.3f Hz). Using output rate (%u Hz).\n",
            audio_driver_data.audio_rate.input, settings->audio.out_rate);
      audio_driver_data.audio_rate.input = settings->audio.out_rate;
   }

   audio_driver_data.audio_rate.source_ratio.original   = 
      audio_driver_data.audio_rate.source_ratio.current =
      (double)settings->audio.out_rate / audio_driver_data.audio_rate.input;

   if (!audio_driver_init_resampler())
   {
      RARCH_ERR("Failed to initialize resampler \"%s\".\n",
            settings->audio.resampler);
      audio_driver_ctl(RARCH_AUDIO_CTL_UNSET_ACTIVE, NULL);
   }

   retro_assert(audio_driver_data.data = (float*)
         malloc(max_bufsamples * sizeof(float)));

   if (!audio_driver_data.data)
      goto error;

   audio_driver_data.data_ptr = 0;

   retro_assert(settings->audio.out_rate <
         audio_driver_data.audio_rate.input * AUDIO_MAX_RATIO);
   retro_assert(audio_driver_data.output_samples.buf = (float*)
         malloc(outsamples_max * sizeof(float)));

   if (!audio_driver_data.output_samples.buf)
      goto error;

   audio_driver_data.audio_rate.control = false;
   if (
         !audio_cb_inited
         && audio_driver_ctl(RARCH_AUDIO_CTL_IS_ACTIVE, NULL) 
         && settings->audio.rate_control
         )
   {
      /* Audio rate control requires write_avail
       * and buffer_size to be implemented. */
      if (current_audio->buffer_size)
      {
         audio_driver_data.driver_buffer_size = 
            current_audio->buffer_size(audio_driver_context_audio_data);
         audio_driver_data.audio_rate.control = true;
      }
      else
         RARCH_WARN("Audio rate control was desired, but driver does not support needed features.\n");
   }

   event_cmd_ctl(EVENT_CMD_DSP_FILTER_INIT, NULL);

   audio_driver_data.free_samples.count = 0;

   /* Threaded driver is initially stopped. */
   if (
         audio_driver_ctl(RARCH_AUDIO_CTL_IS_ACTIVE, NULL)
         && !settings->audio.mute_enable
         && audio_cb_inited
         )
      audio_driver_ctl(RARCH_AUDIO_CTL_START, NULL);

   return true;

error:
   return audio_driver_deinit();
}

/*
 * audio_driver_readjust_input_rate:
 *
 * Readjust the audio input rate.
 */
static void audio_driver_readjust_input_rate(void)
{
   settings_t *settings = config_get_ptr();
   unsigned write_idx   = audio_driver_data.free_samples.count++ &
      (AUDIO_BUFFER_FREE_SAMPLES_COUNT - 1);
   int      half_size   = audio_driver_data.driver_buffer_size / 2;
   int      avail       = 
      current_audio->write_avail(audio_driver_context_audio_data);
   int      delta_mid   = avail - half_size;
   double   direction   = (double)delta_mid / half_size;
   double   adjust      = 1.0 + settings->audio.rate_control_delta * direction;

#if 0
   RARCH_LOG_OUTPUT("Audio buffer is %u%% full\n",
         (unsigned)(100 - (avail * 100) / audio_driver_data.driver_buffer_size));
#endif

   audio_driver_data.free_samples.buf[write_idx] = avail;
   audio_driver_data.audio_rate.source_ratio.current = 
      audio_driver_data.audio_rate.source_ratio.original * adjust;

#if 0
   RARCH_LOG_OUTPUT("New rate: %lf, Orig rate: %lf\n",
         audio_driver_data.audio_rate.source_ratio.current,
         audio_driver_data.audio_rate.source_ratio.original);
#endif
}

void audio_driver_set_nonblocking_state(bool enable)
{
   settings_t *settings = config_get_ptr();
   if (
         audio_driver_ctl(RARCH_AUDIO_CTL_IS_ACTIVE, NULL)
         && audio_driver_context_audio_data
      )
      current_audio->set_nonblock_state(audio_driver_context_audio_data,
            settings->audio.sync ? enable : true);

   audio_driver_data.chunk.size = enable ? 
      audio_driver_data.chunk.nonblock_size : 
      audio_driver_data.chunk.block_size;
}

/**
 * audio_driver_flush:
 * @data                 : pointer to audio buffer.
 * @right                : amount of samples to write.
 *
 * Writes audio samples to audio driver. Will first
 * perform DSP processing (if enabled) and resampling.
 *
 * Returns: true (1) if audio samples were written to the audio
 * driver, false (0) in case of an error.
 **/
static bool audio_driver_flush(const int16_t *data, size_t samples)
{
   static struct retro_perf_counter audio_convert_s16 = {0};
   static struct retro_perf_counter audio_convert_float = {0};
   static struct retro_perf_counter audio_dsp         = {0};
   struct resampler_data src_data              = {0};
   struct rarch_dsp_data dsp_data              = {0};
   const void *output_data                     = NULL;
   unsigned output_frames                      = 0;
   size_t   output_size                        = sizeof(float);
   settings_t *settings                        = config_get_ptr();

   recording_push_audio(data, samples);

   if (runloop_ctl(RUNLOOP_CTL_IS_PAUSED, NULL) || settings->audio.mute_enable)
      return true;
   if (!audio_driver_ctl(RARCH_AUDIO_CTL_IS_ACTIVE, NULL))
      return false;
   if (!audio_driver_data.data)
      return false;

   rarch_perf_init(&audio_convert_s16, "audio_convert_s16");
   retro_perf_start(&audio_convert_s16);
   audio_convert_s16_to_float(audio_driver_data.data, data, samples,
         audio_driver_data.volume_gain);
   retro_perf_stop(&audio_convert_s16);

   src_data.data_in               = audio_driver_data.data;
   src_data.input_frames          = samples >> 1;

   dsp_data.input                 = audio_driver_data.data;
   dsp_data.input_frames          = samples >> 1;

   if (audio_driver_data.dsp)
   {
      rarch_perf_init(&audio_dsp, "audio_dsp");
      retro_perf_start(&audio_dsp);
      rarch_dsp_filter_process(audio_driver_data.dsp, &dsp_data);
      retro_perf_stop(&audio_dsp);

      if (dsp_data.output)
      {
         src_data.data_in      = dsp_data.output;
         src_data.input_frames = dsp_data.output_frames;
      }
   }

   src_data.data_out = audio_driver_data.output_samples.buf;

   if (audio_driver_data.audio_rate.control)
      audio_driver_readjust_input_rate();

   src_data.ratio = audio_driver_data.audio_rate.source_ratio.current;

   if (runloop_ctl(RUNLOOP_CTL_IS_SLOWMOTION, NULL))
      src_data.ratio *= settings->slowmotion_ratio;

   audio_driver_process_resampler(&src_data);

   output_data   = audio_driver_data.output_samples.buf;
   output_frames = src_data.output_frames;

   if (!audio_driver_data.use_float)
   {
      rarch_perf_init(&audio_convert_float, "audio_convert_float");
      retro_perf_start(&audio_convert_float);
      audio_convert_float_to_s16(audio_driver_data.output_samples.conv_buf,
            (const float*)output_data, output_frames * 2);
      retro_perf_stop(&audio_convert_float);

      output_data = audio_driver_data.output_samples.conv_buf;
      output_size = sizeof(int16_t);
   }

   if (current_audio->write(audio_driver_context_audio_data,
            output_data, output_frames * output_size * 2) < 0)
   {
      audio_driver_ctl(RARCH_AUDIO_CTL_UNSET_ACTIVE, NULL);
      return false;
   }

   return true;
}

/**
 * audio_driver_sample:
 * @left                 : value of the left audio channel.
 * @right                : value of the right audio channel.
 *
 * Audio sample render callback function.
 **/
void audio_driver_sample(int16_t left, int16_t right)
{
   audio_driver_data.output_samples.conv_buf[audio_driver_data.data_ptr++] = left;
   audio_driver_data.output_samples.conv_buf[audio_driver_data.data_ptr++] = right;

   if (audio_driver_data.data_ptr < audio_driver_data.chunk.size)
      return;

   audio_driver_flush(audio_driver_data.output_samples.conv_buf, 
         audio_driver_data.data_ptr);

   audio_driver_data.data_ptr = 0;
}

/**
 * audio_driver_sample_batch:
 * @data                 : pointer to audio buffer.
 * @frames               : amount of audio frames to push.
 *
 * Batched audio sample render callback function.
 *
 * Returns: amount of frames sampled. Will be equal to @frames
 * unless @frames exceeds (AUDIO_CHUNK_SIZE_NONBLOCKING / 2).
 **/
size_t audio_driver_sample_batch(const int16_t *data, size_t frames)
{
   if (frames > (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1))
      frames = AUDIO_CHUNK_SIZE_NONBLOCKING >> 1;

   audio_driver_flush(data, frames << 1);

   return frames;
}

/**
 * audio_driver_sample_rewind:
 * @left                 : value of the left audio channel.
 * @right                : value of the right audio channel.
 *
 * Audio sample render callback function (rewind version). 
 * This callback function will be used instead of 
 * audio_driver_sample when rewinding is activated.
 **/
void audio_driver_sample_rewind(int16_t left, int16_t right)
{
   audio_driver_data.rewind.buf[--audio_driver_data.rewind.ptr] = right;
   audio_driver_data.rewind.buf[--audio_driver_data.rewind.ptr] = left;
}

/**
 * audio_driver_sample_batch_rewind:
 * @data                 : pointer to audio buffer.
 * @frames               : amount of audio frames to push.
 *
 * Batched audio sample render callback function (rewind version). 
 *
 * This callback function will be used instead of 
 * audio_driver_sample_batch when rewinding is activated.
 *
 * Returns: amount of frames sampled. Will be equal to @frames
 * unless @frames exceeds (AUDIO_CHUNK_SIZE_NONBLOCKING / 2).
 **/
size_t audio_driver_sample_batch_rewind(const int16_t *data, size_t frames)
{
   size_t i;
   size_t samples   = frames << 1;

   for (i = 0; i < samples; i++)
      audio_driver_data.rewind.buf[--audio_driver_data.rewind.ptr] = data[i];

   return frames;
}

void audio_driver_set_volume_gain(float gain)
{
   audio_driver_data.volume_gain = gain;
}

void audio_driver_dsp_filter_free(void)
{
   if (audio_driver_data.dsp)
      rarch_dsp_filter_free(audio_driver_data.dsp);
   audio_driver_data.dsp = NULL;
}

void audio_driver_dsp_filter_init(const char *device)
{
   audio_driver_data.dsp = rarch_dsp_filter_new(
         device, audio_driver_data.audio_rate.input);

   if (!audio_driver_data.dsp)
      RARCH_ERR("[DSP]: Failed to initialize DSP filter \"%s\".\n", device);
}

void audio_driver_set_buffer_size(size_t bufsize)
{
   audio_driver_data.driver_buffer_size = bufsize;
}

void audio_driver_monitor_adjust_system_rates(void)
{
   float timing_skew;
   settings_t                   *settings = config_get_ptr();
   const struct retro_system_timing *info = NULL;
   struct retro_system_av_info   *av_info = video_viewport_get_system_av_info();
   
   if (av_info)
      info = (const struct retro_system_timing*)&av_info->timing;

   if (!info || info->sample_rate <= 0.0)
      return;

   timing_skew        = fabs(1.0f - info->fps / settings->video.refresh_rate);
   audio_driver_data.audio_rate.input = info->sample_rate;

   if (timing_skew <= settings->audio.max_timing_skew)
      audio_driver_data.audio_rate.input *= (settings->video.refresh_rate / info->fps);

   RARCH_LOG("Set audio input rate to: %.2f Hz.\n",
         audio_driver_data.audio_rate.input);
}

void audio_driver_setup_rewind(void)
{
   unsigned i;

   /* Push audio ready to be played. */
   audio_driver_data.rewind.ptr = audio_driver_data.rewind.size;

   for (i = 0; i < audio_driver_data.data_ptr; i += 2)
   {
      audio_driver_data.rewind.buf[--audio_driver_data.rewind.ptr] =
         audio_driver_data.output_samples.conv_buf[i + 1];

      audio_driver_data.rewind.buf[--audio_driver_data.rewind.ptr] =
         audio_driver_data.output_samples.conv_buf[i + 0];
   }

   audio_driver_data.data_ptr = 0;
}

bool audio_driver_find_driver(void)
{
   int i;
   driver_ctx_info_t drv;
   settings_t *settings = config_get_ptr();

   drv.label = "audio_driver";
   drv.s     = settings->audio.driver;

   driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

   i = drv.len;

   if (i >= 0)
      current_audio = (const audio_driver_t*)audio_driver_find_handle(i);
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any audio driver named \"%s\"\n",
            settings->audio.driver);
      RARCH_LOG_OUTPUT("Available audio drivers are:\n");
      for (d = 0; audio_driver_find_handle(d); d++)
         RARCH_LOG_OUTPUT("\t%s\n", audio_driver_find_ident(d));
      RARCH_WARN("Going to default to first audio driver...\n");

      current_audio = (const audio_driver_t*)audio_driver_find_handle(0);

      if (!current_audio)
         retro_fail(1, "audio_driver_find()");
   }

   return true;
}

void audio_driver_deinit_resampler(void)
{
   rarch_resampler_freep(&audio_driver_resampler,
         &audio_driver_resampler_data);
}

bool audio_driver_free_devices_list(void)
{
   if (!current_audio || !current_audio->device_list_free
         || !audio_driver_context_audio_data)
      return false;
   current_audio->device_list_free(audio_driver_context_audio_data, 
         audio_driver_devices_list);
   audio_driver_devices_list = NULL;
   return true;
}

bool audio_driver_new_devices_list(void)
{
   if (!current_audio || !current_audio->device_list_new
         || !audio_driver_context_audio_data)
      return false;
   audio_driver_devices_list = (struct string_list*)
      current_audio->device_list_new(audio_driver_context_audio_data);
   if (!audio_driver_devices_list)
      return false;
   return true;
}

bool audio_driver_init(void)
{
   return audio_driver_init_internal(audio_callback.callback != NULL);
}

bool audio_driver_get_devices_list(void **data)
{
   struct string_list**ptr = (struct string_list**)data;
   if (!ptr)
      return false;
   *ptr = audio_driver_devices_list;
   return true;
}

bool audio_driver_init_resampler(void)
{
   settings_t *settings = config_get_ptr();
   return rarch_resampler_realloc(
         &audio_driver_resampler_data,
         &audio_driver_resampler,
         settings->audio.resampler,
         audio_driver_data.audio_rate.source_ratio.original);
}

void audio_driver_process_resampler(struct resampler_data *data)
{
   rarch_perf_init(&resampler_proc, "resampler_proc");
   retro_perf_start(&resampler_proc);
   rarch_resampler_process(audio_driver_resampler, 
         audio_driver_resampler_data, data);
   retro_perf_stop(&resampler_proc);
}

bool audio_driver_deinit(void)
{
   audio_driver_free_devices_list();
   if (!uninit_audio())
      return false;
   return true;
}

bool audio_driver_set_callback(const void *data)
{
   const struct retro_audio_callback *cb = (const struct retro_audio_callback*)data;
#ifdef HAVE_NETPLAY
   global_t *global = global_get_ptr();
#endif

   if (recording_driver_get_data_ptr()) /* A/V sync is a must. */
      return false;

#ifdef HAVE_NETPLAY
   if (global->netplay.enable)
      return false;
#endif
   if (cb)
      audio_callback = *cb;

   return true;
}

bool audio_driver_enable_callback(void)
{
   if (!audio_driver_has_callback())
      return false; 
   if (audio_callback.set_state)
      audio_callback.set_state(true);
   return true;
}

bool audio_driver_disable_callback(void)
{
   if (!audio_driver_has_callback())
      return false;

   if (audio_callback.set_state)
      audio_callback.set_state(false);
   return true;
}

/* Sets audio monitor rate to new value. */
void audio_driver_monitor_set_rate(void)
{
   settings_t *settings = config_get_ptr();
   double new_src_ratio = (double)settings->audio.out_rate / 
      audio_driver_data.audio_rate.input;

   audio_driver_data.audio_rate.source_ratio.original = new_src_ratio;
   audio_driver_data.audio_rate.source_ratio.current  = new_src_ratio;
}

bool audio_driver_callback(void)
{
   if (!audio_driver_has_callback())
      return false;

   if (audio_callback.callback)
      audio_callback.callback();

   return true;
}

bool audio_driver_has_callback(void)
{
   return audio_callback.callback;
}

bool audio_driver_toggle_mute(void)
{
   settings_t *settings = config_get_ptr();
   if (!audio_driver_context_audio_data)
      return false;
   if (!audio_driver_ctl(RARCH_AUDIO_CTL_IS_ACTIVE, NULL))
      return false;

   settings->audio.mute_enable = !settings->audio.mute_enable;

   if (settings->audio.mute_enable)
      event_cmd_ctl(EVENT_CMD_AUDIO_STOP, NULL);
   else if (!event_cmd_ctl(EVENT_CMD_AUDIO_START, NULL))
   {
      audio_driver_ctl(RARCH_AUDIO_CTL_UNSET_ACTIVE, NULL);
      return false;
   }
   return true;
}

bool audio_driver_ctl(enum rarch_audio_ctl_state state, void *data)
{
   settings_t        *settings                            = config_get_ptr();

   switch (state)
   {
      case RARCH_AUDIO_CTL_DESTROY:
         audio_driver_active   = false;
         audio_driver_data_own = false;
         current_audio         = NULL;
         break;
      case RARCH_AUDIO_CTL_DESTROY_DATA:
         audio_driver_context_audio_data = NULL;
         break;
      case RARCH_AUDIO_CTL_UNSET_CALLBACK:
         audio_callback.callback  = NULL;
         audio_callback.set_state = NULL;
         break;
      case RARCH_AUDIO_CTL_ALIVE:
         if (!current_audio || !current_audio->alive 
               || !audio_driver_context_audio_data)
            return false;
         return current_audio->alive(audio_driver_context_audio_data);
      case RARCH_AUDIO_CTL_START:
         if (!current_audio || !current_audio->start 
               || !audio_driver_context_audio_data)
            return false;
         return current_audio->start(audio_driver_context_audio_data);
      case RARCH_AUDIO_CTL_STOP:
         if (!current_audio || !current_audio->stop 
               || !audio_driver_context_audio_data)
            return false;
         return current_audio->stop(audio_driver_context_audio_data);
      case RARCH_AUDIO_CTL_SET_OWN_DRIVER:
         audio_driver_data_own = true;
         break;
      case RARCH_AUDIO_CTL_UNSET_OWN_DRIVER:
         audio_driver_data_own = false;
         break;
      case RARCH_AUDIO_CTL_OWNS_DRIVER:
         return audio_driver_data_own;
      case RARCH_AUDIO_CTL_SET_ACTIVE:
         audio_driver_active = true;
         break;
      case RARCH_AUDIO_CTL_UNSET_ACTIVE:
         audio_driver_active = false;
         break;
      case RARCH_AUDIO_CTL_IS_ACTIVE:
         return audio_driver_active;
      case RARCH_AUDIO_CTL_FRAME_IS_REVERSE:
         /* We just rewound. Flush rewind audio buffer. */
         audio_driver_flush(
               audio_driver_data.rewind.buf + audio_driver_data.rewind.ptr,
               audio_driver_data.rewind.size - audio_driver_data.rewind.ptr);
         break;
      case RARCH_AUDIO_CTL_NONE:
      default:
         break;
   }

   return true;
}
