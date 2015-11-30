/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "audio_driver.h"
#include "audio_resampler_driver.h"
#include "audio_utils.h"
#include "audio_thread_wrapper.h"

#include "../general.h"
#include "../verbosity.h"
#include "../string_list_special.h"

#ifndef AUDIO_BUFFER_FREE_SAMPLES_COUNT
#define AUDIO_BUFFER_FREE_SAMPLES_COUNT (8 * 1024)
#endif

typedef struct audio_driver_input_data
{
   float *data;

   size_t data_ptr;
   size_t chunk_size;
   size_t nonblock_chunk_size;
   size_t block_chunk_size;

   double src_ratio;
   float in_rate;

   bool use_float;

   float *outsamples;
   int16_t *conv_outsamples;

   int16_t *rewind_buf;
   size_t rewind_ptr;
   size_t rewind_size;

   rarch_dsp_filter_t *dsp;

   bool rate_control; 
   double orig_src_ratio;
   size_t driver_buffer_size;

   float volume_gain;
   struct retro_audio_callback audio_callback;

   unsigned buffer_free_samples[AUDIO_BUFFER_FREE_SAMPLES_COUNT];
   uint64_t buffer_free_samples_count;
} audio_driver_input_data_t;

static bool audio_active;
static bool audio_data_own;
static const rarch_resampler_t *audio_resampler;
static void *audio_resampler_data;
static const audio_driver_t *current_audio;
static void *context_audio_data;

static audio_driver_input_data_t audio_data;

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
   uint64_t accum = 0, accum_var = 0;
   unsigned low_water_count = 0, high_water_count = 0;
   unsigned samples = min(audio_data.buffer_free_samples_count,
         AUDIO_BUFFER_FREE_SAMPLES_COUNT);

   if (samples < 3)
      return;

   for (i = 1; i < samples; i++)
      accum += audio_data.buffer_free_samples[i];

   avg = accum / (samples - 1);

   for (i = 1; i < samples; i++)
   {
      int diff = avg - audio_data.buffer_free_samples[i];
      accum_var += diff * diff;
   }

   stddev          = (unsigned)sqrt((double)accum_var / (samples - 2));
   avg_filled      = 1.0f - (float)avg / audio_data.driver_buffer_size;
   deviation       = (float)stddev / audio_data.driver_buffer_size;

   low_water_size  = audio_data.driver_buffer_size * 3 / 4;
   high_water_size = audio_data.driver_buffer_size / 4;

   for (i = 1; i < samples; i++)
   {
      if (audio_data.buffer_free_samples[i] >= low_water_size)
         low_water_count++;
      else if (audio_data.buffer_free_samples[i] <= high_water_size)
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

   if (context_audio_data && current_audio)
      current_audio->free(context_audio_data);

   if (audio_data.conv_outsamples)
      free(audio_data.conv_outsamples);
   audio_data.conv_outsamples = NULL;
   audio_data.data_ptr        = 0;

   if (audio_data.rewind_buf)
      free(audio_data.rewind_buf);
   audio_data.rewind_buf = NULL;

   if (!settings->audio.enable)
   {
      audio_active = false;
      return false;
   }

   rarch_resampler_freep(&audio_resampler,
         &audio_resampler_data);

   if (audio_data.audio_callback.callback)
   {
      audio_data.audio_callback.callback  = NULL;
      audio_data.audio_callback.set_state = NULL;
   }

   if (audio_data.data)
      free(audio_data.data);
   audio_data.data = NULL;

   if (audio_data.outsamples)
      free(audio_data.outsamples);
   audio_data.outsamples = NULL;

   event_command(EVENT_CMD_DSP_FILTER_DEINIT);

   compute_audio_buffer_statistics();

   current_audio = NULL;

   if (!audio_data_own)
      context_audio_data = NULL;

   return true;
}

static bool init_audio(void)
{
   size_t outsamples_max, max_bufsamples = AUDIO_CHUNK_SIZE_NONBLOCKING * 2;
   settings_t *settings = config_get_ptr();

   audio_convert_init_simd();

   /* Resource leaks will follow if audio is initialized twice. */
   if (context_audio_data)
      return false;

   /* Accomodate rewind since at some point we might have two full buffers. */
   outsamples_max = max_bufsamples * AUDIO_MAX_RATIO * 
      settings->slowmotion_ratio;

   /* Used for recording even if audio isn't enabled. */
   retro_assert(audio_data.conv_outsamples =
         (int16_t*)malloc(outsamples_max * sizeof(int16_t)));

   if (!audio_data.conv_outsamples)
      goto error;

   audio_data.block_chunk_size    = AUDIO_CHUNK_SIZE_BLOCKING;
   audio_data.nonblock_chunk_size = AUDIO_CHUNK_SIZE_NONBLOCKING;
   audio_data.chunk_size          = audio_data.block_chunk_size;

   /* Needs to be able to hold full content of a full max_bufsamples
    * in addition to its own. */
   retro_assert(audio_data.rewind_buf = (int16_t*)
         malloc(max_bufsamples * sizeof(int16_t)));

   if (!audio_data.rewind_buf)
      goto error;

   audio_data.rewind_size             = max_bufsamples;

   if (!settings->audio.enable)
   {
      audio_active = false;
      return false;
   }

   audio_driver_ctl(RARCH_AUDIO_CTL_FIND_DRIVER, NULL);
#ifdef HAVE_THREADS
   if (audio_data.audio_callback.callback)
   {
      RARCH_LOG("Starting threaded audio driver ...\n");
      if (!rarch_threaded_audio_init(&current_audio, &context_audio_data,
               *settings->audio.device ? settings->audio.device : NULL,
               settings->audio.out_rate, settings->audio.latency,
               current_audio))
      {
         RARCH_ERR("Cannot open threaded audio driver ... Exiting ...\n");
         retro_fail(1, "init_audio()");
      }
   }
   else
#endif
   {
      context_audio_data = current_audio->init(*settings->audio.device ?
            settings->audio.device : NULL,
            settings->audio.out_rate, settings->audio.latency);
   }

   if (!context_audio_data)
   {
      RARCH_ERR("Failed to initialize audio driver. Will continue without audio.\n");
      audio_active = false;
   }

   audio_data.use_float = false;
   if (audio_active && current_audio->use_float(context_audio_data))
      audio_data.use_float = true;

   if (!settings->audio.sync && audio_active)
   {
      event_command(EVENT_CMD_AUDIO_SET_NONBLOCKING_STATE);
      audio_data.chunk_size = audio_data.nonblock_chunk_size;
   }

   if (audio_data.in_rate <= 0.0f)
   {
      /* Should never happen. */
      RARCH_WARN("Input rate is invalid (%.3f Hz). Using output rate (%u Hz).\n",
            audio_data.in_rate, settings->audio.out_rate);
      audio_data.in_rate = settings->audio.out_rate;
   }

   audio_data.orig_src_ratio = audio_data.src_ratio =
      (double)settings->audio.out_rate / audio_data.in_rate;

   if (!rarch_resampler_realloc(&audio_resampler_data,
            &audio_resampler,
         settings->audio.resampler, audio_data.orig_src_ratio))
   {
      RARCH_ERR("Failed to initialize resampler \"%s\".\n",
            settings->audio.resampler);
      audio_active = false;
   }

   retro_assert(audio_data.data = (float*)
         malloc(max_bufsamples * sizeof(float)));

   if (!audio_data.data)
      goto error;

   audio_data.data_ptr = 0;

   retro_assert(settings->audio.out_rate <
         audio_data.in_rate * AUDIO_MAX_RATIO);
   retro_assert(audio_data.outsamples = (float*)
         malloc(outsamples_max * sizeof(float)));

   if (!audio_data.outsamples)
      goto error;

   audio_data.rate_control = false;
   if (!audio_data.audio_callback.callback && audio_active &&
         settings->audio.rate_control)
   {
      /* Audio rate control requires write_avail
       * and buffer_size to be implemented. */
      if (current_audio->buffer_size)
      {
         audio_data.driver_buffer_size = 
            current_audio->buffer_size(context_audio_data);
         audio_data.rate_control = true;
      }
      else
         RARCH_WARN("Audio rate control was desired, but driver does not support needed features.\n");
   }

   event_command(EVENT_CMD_DSP_FILTER_INIT);

   audio_data.buffer_free_samples_count = 0;

   /* Threaded driver is initially stopped. */
   if (audio_active && !settings->audio.mute_enable &&
         audio_data.audio_callback.callback)
      audio_driver_ctl(RARCH_AUDIO_CTL_START, NULL);

   return true;

error:
   return audio_driver_ctl(RARCH_AUDIO_CTL_DEINIT, NULL);
}

/*
 * audio_driver_readjust_input_rate:
 *
 * Readjust the audio input rate.
 */
static void audio_driver_readjust_input_rate(void)
{
   settings_t *settings = config_get_ptr();
   unsigned write_idx   = audio_data.buffer_free_samples_count++ &
      (AUDIO_BUFFER_FREE_SAMPLES_COUNT - 1);
   int      half_size   = audio_data.driver_buffer_size / 2;
   int      avail       = current_audio->write_avail(context_audio_data);
   int      delta_mid   = avail - half_size;
   double   direction   = (double)delta_mid / half_size;
   double   adjust      = 1.0 + settings->audio.rate_control_delta * direction;

#if 0
   RARCH_LOG_OUTPUT("Audio buffer is %u%% full\n",
         (unsigned)(100 - (avail * 100) / audio_data.driver_buffer_size));
#endif

   audio_data.buffer_free_samples[write_idx] = avail;
   audio_data.src_ratio = audio_data.orig_src_ratio * adjust;

#if 0
   RARCH_LOG_OUTPUT("New rate: %lf, Orig rate: %lf\n",
         audio_data.src_ratio, audio_data.orig_src_ratio);
#endif
}

void audio_driver_set_nonblocking_state(bool enable)
{
   settings_t *settings = config_get_ptr();
   if (audio_active && context_audio_data)
      current_audio->set_nonblock_state(context_audio_data,
            settings->audio.sync ? enable : true);

   audio_data.chunk_size = enable ? audio_data.nonblock_chunk_size : 
      audio_data.block_chunk_size;
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
   bool is_slowmotion, is_paused;
   static struct retro_perf_counter audio_convert_s16 = {0};
   static struct retro_perf_counter audio_convert_float = {0};
   static struct retro_perf_counter audio_dsp         = {0};
   static struct retro_perf_counter resampler_proc    = {0};
   struct resampler_data src_data              = {0};
   struct rarch_dsp_data dsp_data              = {0};
   const void *output_data                     = NULL;
   unsigned output_frames                      = 0;
   size_t   output_size                        = sizeof(float);
   settings_t *settings                        = config_get_ptr();

   recording_push_audio(data, samples);

   rarch_main_ctl(RARCH_MAIN_CTL_IS_PAUSED, &is_paused);

   if (is_paused || settings->audio.mute_enable)
      return true;
   if (!audio_active || !audio_data.data)
      return false;

   rarch_perf_init(&audio_convert_s16, "audio_convert_s16");
   retro_perf_start(&audio_convert_s16);
   audio_convert_s16_to_float(audio_data.data, data, samples,
         audio_data.volume_gain);
   retro_perf_stop(&audio_convert_s16);

   src_data.data_in               = audio_data.data;
   src_data.input_frames          = samples >> 1;

   dsp_data.input                 = audio_data.data;
   dsp_data.input_frames          = samples >> 1;

   if (audio_data.dsp)
   {
      rarch_perf_init(&audio_dsp, "audio_dsp");
      retro_perf_start(&audio_dsp);
      rarch_dsp_filter_process(audio_data.dsp, &dsp_data);
      retro_perf_stop(&audio_dsp);

      if (dsp_data.output)
      {
         src_data.data_in      = dsp_data.output;
         src_data.input_frames = dsp_data.output_frames;
      }
   }

   src_data.data_out = audio_data.outsamples;

   if (audio_data.rate_control)
      audio_driver_readjust_input_rate();

   src_data.ratio = audio_data.src_ratio;

   rarch_main_ctl(RARCH_MAIN_CTL_IS_SLOWMOTION, &is_slowmotion);

   if (is_slowmotion)
      src_data.ratio *= settings->slowmotion_ratio;

   rarch_perf_init(&resampler_proc, "resampler_proc");
   retro_perf_start(&resampler_proc);
   rarch_resampler_process(audio_resampler, audio_resampler_data, &src_data);
   retro_perf_stop(&resampler_proc);

   output_data   = audio_data.outsamples;
   output_frames = src_data.output_frames;

   if (!audio_data.use_float)
   {
      rarch_perf_init(&audio_convert_float, "audio_convert_float");
      retro_perf_start(&audio_convert_float);
      audio_convert_float_to_s16(audio_data.conv_outsamples,
            (const float*)output_data, output_frames * 2);
      retro_perf_stop(&audio_convert_float);

      output_data = audio_data.conv_outsamples;
      output_size = sizeof(int16_t);
   }

   if (current_audio->write(context_audio_data, output_data, output_frames * output_size * 2) < 0)
   {
      audio_active = false;
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
   audio_data.conv_outsamples[audio_data.data_ptr++] = left;
   audio_data.conv_outsamples[audio_data.data_ptr++] = right;

   if (audio_data.data_ptr < audio_data.chunk_size)
      return;

   audio_driver_flush(audio_data.conv_outsamples, audio_data.data_ptr);

   audio_data.data_ptr = 0;
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
 * Audio sample render callback function (rewind version). This callback
 * function will be used instead of audio_driver_sample when rewinding is activated.
 **/
void audio_driver_sample_rewind(int16_t left, int16_t right)
{
   audio_data.rewind_buf[--audio_data.rewind_ptr] = right;
   audio_data.rewind_buf[--audio_data.rewind_ptr] = left;
}

/**
 * audio_driver_sample_batch_rewind:
 * @data                 : pointer to audio buffer.
 * @frames               : amount of audio frames to push.
 *
 * Batched audio sample render callback function (rewind version). This callback
 * function will be used instead of audio_driver_sample_batch when rewinding is activated.
 *
 * Returns: amount of frames sampled. Will be equal to @frames
 * unless @frames exceeds (AUDIO_CHUNK_SIZE_NONBLOCKING / 2).
 **/
size_t audio_driver_sample_batch_rewind(const int16_t *data, size_t frames)
{
   size_t i;
   size_t samples   = frames << 1;

   for (i = 0; i < samples; i++)
      audio_data.rewind_buf[--audio_data.rewind_ptr] = data[i];

   return frames;
}

void audio_driver_set_volume_gain(float gain)
{
   audio_data.volume_gain = gain;
}

void audio_driver_dsp_filter_free(void)
{
   if (audio_data.dsp)
      rarch_dsp_filter_free(audio_data.dsp);
   audio_data.dsp = NULL;
}

void audio_driver_dsp_filter_init(const char *device)
{
   audio_data.dsp = rarch_dsp_filter_new(device, audio_data.in_rate);
   if (!audio_data.dsp)
      RARCH_ERR("[DSP]: Failed to initialize DSP filter \"%s\".\n", device);
}

void audio_driver_set_buffer_size(size_t bufsize)
{
   audio_data.driver_buffer_size = bufsize;
}

void audio_driver_set_callback(const void *data)
{
   const struct retro_audio_callback *cb = 
      (const struct retro_audio_callback*)data;

   if (cb)
      audio_data.audio_callback = *cb;
}

void audio_driver_callback_set_state(bool state)
{
   if (!audio_driver_ctl(RARCH_AUDIO_CTL_HAS_CALLBACK, NULL))
      return;

   if (audio_data.audio_callback.set_state)
      audio_data.audio_callback.set_state(state);
}

static void audio_monitor_adjust_system_rates(void)
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
   audio_data.in_rate = info->sample_rate;

   if (timing_skew <= settings->audio.max_timing_skew)
      audio_data.in_rate *= (settings->video.refresh_rate / info->fps);

   RARCH_LOG("Set audio input rate to: %.2f Hz.\n",
         audio_data.in_rate);
}

static void audio_driver_setup_rewind(void)
{
   unsigned i;

   /* Push audio ready to be played. */
   audio_data.rewind_ptr = audio_data.rewind_size;

   for (i = 0; i < audio_data.data_ptr; i += 2)
   {
      audio_data.rewind_buf[--audio_data.rewind_ptr] =
         audio_data.conv_outsamples[i + 1];

      audio_data.rewind_buf[--audio_data.rewind_ptr] =
         audio_data.conv_outsamples[i + 0];
   }

   audio_data.data_ptr = 0;
}

static bool find_audio_driver(void)
{
   settings_t *settings = config_get_ptr();

   int i = find_driver_index("audio_driver", settings->audio.driver);

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
         retro_fail(1, "find_audio_driver()");
   }

   return true;
}

bool audio_driver_ctl(enum rarch_audio_ctl_state state, void *data)
{
   settings_t        *settings = config_get_ptr();

   switch (state)
   {
      case RARCH_AUDIO_CTL_NONE:
         break;
      case RARCH_AUDIO_CTL_INIT:
         return init_audio();
      case RARCH_AUDIO_CTL_DEINIT:
         return uninit_audio();
      case RARCH_AUDIO_CTL_SETUP_REWIND:
         audio_driver_setup_rewind();
         return true;
      case RARCH_AUDIO_CTL_HAS_CALLBACK:
         return audio_data.audio_callback.callback;
      case RARCH_AUDIO_CTL_CALLBACK:
         if (!audio_driver_ctl(RARCH_AUDIO_CTL_HAS_CALLBACK, NULL))
            return false;

         if (audio_data.audio_callback.callback)
            audio_data.audio_callback.callback();
         return true;
      case RARCH_AUDIO_CTL_MONITOR_ADJUST_SYSTEM_RATES:
         audio_monitor_adjust_system_rates();
         return true;
      case RARCH_AUDIO_CTL_MONITOR_SET_REFRESH_RATE:
         {
            double new_src_ratio = (double)settings->audio.out_rate / 
               audio_data.in_rate;

            audio_data.orig_src_ratio = new_src_ratio;
            audio_data.src_ratio      = new_src_ratio;
         }
         return true;
      case RARCH_AUDIO_CTL_MUTE_TOGGLE:
         if (!context_audio_data || !audio_active)
            return false;

         settings->audio.mute_enable = !settings->audio.mute_enable;

         if (settings->audio.mute_enable)
            event_command(EVENT_CMD_AUDIO_STOP);
         else if (!event_command(EVENT_CMD_AUDIO_START))
         {
            audio_active = false;
            return false;
         }
         return true;
      case RARCH_AUDIO_CTL_ALIVE:
         if (!context_audio_data)
            return false;
         return current_audio->alive(context_audio_data);
      case RARCH_AUDIO_CTL_START:
         return current_audio->start(context_audio_data);
      case RARCH_AUDIO_CTL_STOP:
         return current_audio->stop(context_audio_data);
      case RARCH_AUDIO_CTL_FIND_DRIVER:
         return find_audio_driver();
      case RARCH_AUDIO_CTL_SET_OWN_DRIVER:
         audio_data_own = true;
         break;
      case RARCH_AUDIO_CTL_UNSET_OWN_DRIVER:
         audio_data_own = false;
         break;
      case RARCH_AUDIO_CTL_OWNS_DRIVER:
         return audio_data_own;
      case RARCH_AUDIO_CTL_SET_ACTIVE:
         audio_active = true;
         break;
      case RARCH_AUDIO_CTL_UNSET_ACTIVE:
         audio_active = false;
         break;
      case RARCH_AUDIO_CTL_FRAME_IS_REVERSE:
         /* We just rewound. Flush rewind audio buffer. */
         audio_driver_flush(audio_data.rewind_buf + audio_data.rewind_ptr,
               audio_data.rewind_size - audio_data.rewind_ptr);
         return true;
   }

   return false;
}
