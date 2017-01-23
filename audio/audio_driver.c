/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <audio/conversion/float_to_s16.h>
#include <audio/conversion/s16_to_float.h>
#include <audio/audio_resampler.h>
#include <audio/dsp_filter.h>
#include <file/file_path.h>
#include <lists/dir_list.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "audio_driver.h"
#include "audio_thread_wrapper.h"
#include "../record/record_driver.h"
#include "../frontend/frontend_driver.h"

#include "../command.h"
#include "../driver.h"
#include "../configuration.h"
#include "../retroarch.h"
#include "../runloop.h"
#include "../performance_counters.h"
#include "../verbosity.h"
#include "../list_special.h"

#define AUDIO_BUFFER_FREE_SAMPLES_COUNT (8 * 1024)

static const audio_driver_t *audio_drivers[] = {
#ifdef HAVE_ALSA
   &audio_alsa,
#if !defined(__QNX__) && defined(HAVE_THREADS)
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
#ifdef WIIU
   &audio_ax,
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

static size_t audio_driver_chunk_size                    = 0;
static size_t audio_driver_chunk_nonblock_size           = 0;
static size_t audio_driver_chunk_block_size              = 0;

static size_t audio_driver_rewind_ptr                    = 0;
static size_t audio_driver_rewind_size                   = 0;
static int16_t *audio_driver_rewind_buf                  = NULL;

static float *audio_driver_input_data                    = NULL;

static unsigned audio_driver_free_samples_buf[AUDIO_BUFFER_FREE_SAMPLES_COUNT];
static uint64_t audio_driver_free_samples_count          = 0;

static float   *audio_driver_output_samples_buf          = NULL;
static int16_t *audio_driver_output_samples_conv_buf     = NULL;

static float audio_driver_volume_gain                    = 0.0f;

static size_t audio_driver_buffer_size                   = 0;
static size_t audio_driver_data_ptr                      = 0;

static bool  audio_driver_control                        = false; 
static float audio_driver_input                          = 0.0f;
static double audio_source_ratio_original                = 0.0f;
static double audio_source_ratio_current                 = 0.0f;
static struct retro_audio_callback audio_callback        = {0};

static retro_dsp_filter_t *audio_driver_dsp              = NULL;
static struct string_list *audio_driver_devices_list     = NULL;
static const retro_resampler_t *audio_driver_resampler   = NULL;
static void *audio_driver_resampler_data                 = NULL;
static const audio_driver_t *current_audio               = NULL;
static void *audio_driver_context_audio_data             = NULL;

static bool audio_driver_use_float                       = false;
static bool audio_driver_active                          = false;
static bool audio_driver_data_own                        = false;

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
         audio_driver_free_samples_count,
         AUDIO_BUFFER_FREE_SAMPLES_COUNT);

   if (samples < 3)
      return;

   for (i = 1; i < samples; i++)
      accum += audio_driver_free_samples_buf[i];

   avg = accum / (samples - 1);

   for (i = 1; i < samples; i++)
   {
      int diff = avg - audio_driver_free_samples_buf[i];
      accum_var += diff * diff;
   }

   stddev          = (unsigned)sqrt((double)accum_var / (samples - 2));
   avg_filled      = 1.0f - (float)avg / audio_driver_buffer_size;
   deviation       = (float)stddev / audio_driver_buffer_size;

   low_water_size  = audio_driver_buffer_size * 3 / 4;
   high_water_size = audio_driver_buffer_size / 4;

   for (i = 1; i < samples; i++)
   {
      if (audio_driver_free_samples_buf[i] >= low_water_size)
         low_water_count++;
      else if (audio_driver_free_samples_buf[i] <= high_water_size)
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

   if (audio_driver_output_samples_conv_buf)
      free(audio_driver_output_samples_conv_buf);
   audio_driver_output_samples_conv_buf = NULL;

   audio_driver_data_ptr                = 0;

   if (audio_driver_rewind_buf)
      free(audio_driver_rewind_buf);
   audio_driver_rewind_buf   = NULL;

   audio_driver_rewind_size  = 0;

   if (!settings->audio.enable)
   {
      audio_driver_active = false;
      return false;
   }

   audio_driver_deinit_resampler();

   if (audio_driver_input_data)
      free(audio_driver_input_data);
   audio_driver_input_data = NULL;

   if (audio_driver_output_samples_buf)
      free(audio_driver_output_samples_buf);
   audio_driver_output_samples_buf = NULL;

   command_event(CMD_EVENT_DSP_FILTER_DEINIT, NULL);

   compute_audio_buffer_statistics();

   return true;
}

static bool audio_driver_init_resampler(void)
{
   settings_t *settings = config_get_ptr();
   return retro_resampler_realloc(
         &audio_driver_resampler_data,
         &audio_driver_resampler,
         settings->audio.resampler,
         audio_source_ratio_original);
}

static bool audio_driver_init_internal(bool audio_cb_inited)
{
   unsigned new_rate     = 0;
   float   *aud_inp_data = NULL;
   float *samples_buf    = NULL;
   int16_t *conv_buf     = NULL;
   int16_t *rewind_buf   = NULL;
   size_t outsamples_max = AUDIO_CHUNK_SIZE_NONBLOCKING * 2;
   size_t max_bufsamples = AUDIO_CHUNK_SIZE_NONBLOCKING * 2;
   settings_t *settings  = config_get_ptr();

   convert_s16_to_float_init_simd();
   convert_float_to_s16_init_simd();

   /* Accomodate rewind since at some point we might have two full buffers. */
   outsamples_max = max_bufsamples * AUDIO_MAX_RATIO * 
      settings->slowmotion_ratio;

   conv_buf = (int16_t*)malloc(outsamples_max 
         * sizeof(int16_t));
   /* Used for recording even if audio isn't enabled. */
   retro_assert(conv_buf != NULL);

   if (!conv_buf)
      goto error;

   audio_driver_output_samples_conv_buf = conv_buf;
   audio_driver_chunk_block_size        = AUDIO_CHUNK_SIZE_BLOCKING;
   audio_driver_chunk_nonblock_size     = AUDIO_CHUNK_SIZE_NONBLOCKING;
   audio_driver_chunk_size              = audio_driver_chunk_block_size;

   /* Needs to be able to hold full content of a full max_bufsamples
    * in addition to its own. */
   rewind_buf = (int16_t*)malloc(max_bufsamples * sizeof(int16_t));
   retro_assert(rewind_buf != NULL);

   if (!rewind_buf)
      goto error;

   audio_driver_rewind_buf              = rewind_buf;
   audio_driver_rewind_size             = max_bufsamples;

   if (!settings->audio.enable)
   {
      audio_driver_active = false;
      return false;
   }

   audio_driver_find_driver();
#ifdef HAVE_THREADS
   if (audio_cb_inited)
   {
      RARCH_LOG("Starting threaded audio driver ...\n");
      if (!audio_init_thread(
               &current_audio,
               &audio_driver_context_audio_data,
               *settings->audio.device ? settings->audio.device : NULL,
               settings->audio.out_rate, &new_rate, 
               settings->audio.latency,
               settings->audio.block_frames,
               current_audio))
      {
         RARCH_ERR("Cannot open threaded audio driver ... Exiting ...\n");
         retroarch_fail(1, "audio_driver_init_internal()");
      }
   }
   else
#endif
   {
      audio_driver_context_audio_data = 
         current_audio->init(*settings->audio.device ?
               settings->audio.device : NULL,
               settings->audio.out_rate, settings->audio.latency,
               settings->audio.block_frames,
               &new_rate);
   }

   if (new_rate != 0)
      settings->audio.out_rate = new_rate;

   if (!audio_driver_context_audio_data)
   {
      RARCH_ERR("Failed to initialize audio driver. Will continue without audio.\n");
      audio_driver_active = false;
   }

   audio_driver_use_float = false;
   if (     audio_driver_active 
         && current_audio->use_float(audio_driver_context_audio_data))
      audio_driver_use_float = true;

   if (!settings->audio.sync && audio_driver_active)
   {
      command_event(CMD_EVENT_AUDIO_SET_NONBLOCKING_STATE, NULL);
      audio_driver_chunk_size = audio_driver_chunk_nonblock_size;
   }

   if (audio_driver_input <= 0.0f)
   {
      /* Should never happen. */
      RARCH_WARN("Input rate is invalid (%.3f Hz). Using output rate (%u Hz).\n",
            audio_driver_input, settings->audio.out_rate);
      audio_driver_input = settings->audio.out_rate;
   }

   audio_source_ratio_original   = audio_source_ratio_current =
      (double)settings->audio.out_rate / audio_driver_input;

   if (!audio_driver_init_resampler())
   {
      RARCH_ERR("Failed to initialize resampler \"%s\".\n",
            settings->audio.resampler);
      audio_driver_active = false;
   }

   aud_inp_data = (float*)malloc(max_bufsamples * sizeof(float));
   retro_assert(aud_inp_data != NULL);

   if (!aud_inp_data)
      goto error;

   audio_driver_input_data = aud_inp_data;
   audio_driver_data_ptr   = 0;

   retro_assert(settings->audio.out_rate <
         audio_driver_input * AUDIO_MAX_RATIO);

   samples_buf = (float*)malloc(outsamples_max * sizeof(float));
   retro_assert(samples_buf != NULL);

   if (!samples_buf)
      goto error;

   audio_driver_output_samples_buf = samples_buf;
   audio_driver_control            = false;

   if (
         !audio_cb_inited
         && audio_driver_active 
         && settings->audio.rate_control
         )
   {
      /* Audio rate control requires write_avail
       * and buffer_size to be implemented. */
      if (current_audio->buffer_size)
      {
         audio_driver_buffer_size = 
            current_audio->buffer_size(audio_driver_context_audio_data);
         audio_driver_control = true;
      }
      else
         RARCH_WARN("Audio rate control was desired, but driver does not support needed features.\n");
   }

   /* If we start muted, stop the audio driver, so subsequent unmute works. */
   if (!audio_cb_inited && audio_driver_active && settings->audio.mute_enable)
      audio_driver_stop();

   command_event(CMD_EVENT_DSP_FILTER_INIT, NULL);

   audio_driver_free_samples_count = 0;

   /* Threaded driver is initially stopped. */
   if (
         audio_driver_active
         && !settings->audio.mute_enable
         && audio_cb_inited
         )
      audio_driver_start(false);

   return true;

error:
   return audio_driver_deinit();
}

void audio_driver_set_nonblocking_state(bool enable)
{
   settings_t *settings = config_get_ptr();
   if (
         audio_driver_active
         && audio_driver_context_audio_data
      )
      current_audio->set_nonblock_state(audio_driver_context_audio_data,
            settings->audio.sync ? enable : true);

   audio_driver_chunk_size = enable ? 
      audio_driver_chunk_nonblock_size : 
      audio_driver_chunk_block_size;
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
   struct resampler_data src_data;
   bool is_paused                                       = false;
   bool is_idle                                         = false;
   bool is_slowmotion                                   = false;
   static struct retro_perf_counter resampler_proc      = {0};
   static struct retro_perf_counter audio_convert_s16   = {0};
   const void *output_data                              = NULL;
   unsigned output_frames                               = 0;
   size_t   output_size                                 = sizeof(float);
   settings_t *settings                                 = config_get_ptr();

   src_data.data_in                                     = NULL;
   src_data.data_out                                    = NULL;
   src_data.input_frames                                = 0;
   src_data.output_frames                               = 0;
   src_data.ratio                                       = 0.0f;

   if (recording_data)
      recording_push_audio(data, samples);

   runloop_get_status(&is_paused, &is_idle, &is_slowmotion);

   if (is_paused || settings->audio.mute_enable)
      return true;
   if (!audio_driver_active || !audio_driver_input_data)
      return false;

   performance_counter_init(&audio_convert_s16, "audio_convert_s16");
   performance_counter_start(&audio_convert_s16);
   convert_s16_to_float(audio_driver_input_data, data, samples,
         audio_driver_volume_gain);
   performance_counter_stop(&audio_convert_s16);

   src_data.data_in               = audio_driver_input_data;
   src_data.input_frames          = samples >> 1;


   if (audio_driver_dsp)
   {
      static struct retro_perf_counter audio_dsp           = {0};
      struct retro_dsp_data dsp_data;

      dsp_data.input                 = NULL;
      dsp_data.input_frames          = 0;
      dsp_data.output                = NULL;
      dsp_data.output_frames         = 0;

      dsp_data.input                 = audio_driver_input_data;
      dsp_data.input_frames          = samples >> 1;

      performance_counter_init(&audio_dsp, "audio_dsp");
      performance_counter_start(&audio_dsp);
      retro_dsp_filter_process(audio_driver_dsp, &dsp_data);
      performance_counter_stop(&audio_dsp);

      if (dsp_data.output)
      {
         src_data.data_in      = dsp_data.output;
         src_data.input_frames = dsp_data.output_frames;
      }
   }

   src_data.data_out = audio_driver_output_samples_buf;

   if (audio_driver_control)
   {
      /* Readjust the audio input rate. */
      unsigned write_idx   = audio_driver_free_samples_count++ &
         (AUDIO_BUFFER_FREE_SAMPLES_COUNT - 1);
      int      half_size   = audio_driver_buffer_size / 2;
      int      avail       = 
         current_audio->write_avail(audio_driver_context_audio_data);
      int      delta_mid   = avail - half_size;
      double   direction   = (double)delta_mid / half_size;
      double   adjust      = 1.0 + settings->audio.rate_control_delta * direction;

#if 0
      RARCH_LOG_OUTPUT("Audio buffer is %u%% full\n",
            (unsigned)(100 - (avail * 100) / audio_driver_buffer_size));
#endif

      audio_driver_free_samples_buf[write_idx] = avail;
      audio_source_ratio_current   = 
         audio_source_ratio_original * adjust;

#if 0
      RARCH_LOG_OUTPUT("New rate: %lf, Orig rate: %lf\n",
            audio_source_ratio_current,
            audio_source_ratio_original);
#endif
   }

   src_data.ratio = audio_source_ratio_current;

   if (is_slowmotion)
      src_data.ratio *= settings->slowmotion_ratio;

   performance_counter_init(&resampler_proc, "resampler_proc");
   performance_counter_start(&resampler_proc);

   audio_driver_resampler->process(audio_driver_resampler_data, &src_data);
   performance_counter_stop(&resampler_proc);

   output_data   = audio_driver_output_samples_buf;
   output_frames = src_data.output_frames;

   if (!audio_driver_use_float)
   {
      static struct retro_perf_counter audio_convert_float = {0};

      performance_counter_init(&audio_convert_float, "audio_convert_float");
      performance_counter_start(&audio_convert_float);
      convert_float_to_s16(audio_driver_output_samples_conv_buf,
            (const float*)output_data, output_frames * 2);
      performance_counter_stop(&audio_convert_float);

      output_data = audio_driver_output_samples_conv_buf;
      output_size = sizeof(int16_t);
   }

   if (current_audio->write(audio_driver_context_audio_data,
            output_data, output_frames * output_size * 2) < 0)
   {
      audio_driver_active = false;
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
   audio_driver_output_samples_conv_buf[audio_driver_data_ptr++] = left;
   audio_driver_output_samples_conv_buf[audio_driver_data_ptr++] = right;

   if (audio_driver_data_ptr < audio_driver_chunk_size)
      return;

   audio_driver_flush(audio_driver_output_samples_conv_buf, 
         audio_driver_data_ptr);

   audio_driver_data_ptr = 0;
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
   audio_driver_rewind_buf[--audio_driver_rewind_ptr] = right;
   audio_driver_rewind_buf[--audio_driver_rewind_ptr] = left;
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
      audio_driver_rewind_buf[--audio_driver_rewind_ptr] = data[i];

   return frames;
}

void audio_driver_set_volume_gain(float gain)
{
   audio_driver_volume_gain = gain;
}

void audio_driver_dsp_filter_free(void)
{
   if (audio_driver_dsp)
      retro_dsp_filter_free(audio_driver_dsp);
   audio_driver_dsp = NULL;
}

void audio_driver_dsp_filter_init(const char *device)
{
#if defined(HAVE_DYLIB) && !defined(HAVE_FILTERS_BUILTIN)
   char basedir[PATH_MAX_LENGTH];
   char ext_name[PATH_MAX_LENGTH];
#endif
   struct string_list *plugs     = NULL;
#if defined(HAVE_DYLIB) && !defined(HAVE_FILTERS_BUILTIN)
   fill_pathname_basedir(basedir, device, sizeof(basedir));

   if (!frontend_driver_get_core_extension(ext_name, sizeof(ext_name)))
      goto error;

   plugs = dir_list_new(basedir, ext_name, false, true, false, false);
   if (!plugs)
      goto error;
#endif
   audio_driver_dsp = retro_dsp_filter_new(
         device, plugs, audio_driver_input);
   if (!audio_driver_dsp)
      goto error;

   return;

error:
   if (!audio_driver_dsp)
      RARCH_ERR("[DSP]: Failed to initialize DSP filter \"%s\".\n", device);
}

void audio_driver_set_buffer_size(size_t bufsize)
{
   audio_driver_buffer_size = bufsize;
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

   timing_skew             = fabs(1.0f - info->fps / settings->video.refresh_rate);
   audio_driver_input      = info->sample_rate;

   if (timing_skew <= settings->audio.max_timing_skew)
      audio_driver_input *= (settings->video.refresh_rate / info->fps);

   RARCH_LOG("Set audio input rate to: %.2f Hz.\n",
         audio_driver_input);
}

void audio_driver_setup_rewind(void)
{
   unsigned i;

   /* Push audio ready to be played. */
   audio_driver_rewind_ptr = audio_driver_rewind_size;

   for (i = 0; i < audio_driver_data_ptr; i += 2)
   {
      audio_driver_rewind_buf[--audio_driver_rewind_ptr] =
         audio_driver_output_samples_conv_buf[i + 1];

      audio_driver_rewind_buf[--audio_driver_rewind_ptr] =
         audio_driver_output_samples_conv_buf[i + 0];
   }

   audio_driver_data_ptr = 0;
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
         retroarch_fail(1, "audio_driver_find()");
   }

   return true;
}

void audio_driver_deinit_resampler(void)
{
   if (audio_driver_resampler && audio_driver_resampler_data)
      audio_driver_resampler->free(audio_driver_resampler_data);
   audio_driver_resampler      = NULL;
   audio_driver_resampler_data = NULL;
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

   if (cb)
      audio_callback = *cb;

   return true;
}

bool audio_driver_enable_callback(void)
{
   if (!audio_callback.callback)
      return false; 
   if (audio_callback.set_state)
      audio_callback.set_state(true);
   return true;
}

bool audio_driver_disable_callback(void)
{
   if (!audio_callback.callback)
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
      audio_driver_input;

   audio_source_ratio_original = new_src_ratio;
   audio_source_ratio_current  = new_src_ratio;
}

bool audio_driver_callback(void)
{
   if (!audio_callback.callback)
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
   if (!audio_driver_active)
      return false;

   settings->audio.mute_enable = !settings->audio.mute_enable;

   if (settings->audio.mute_enable)
      command_event(CMD_EVENT_AUDIO_STOP, NULL);
   else if (!command_event(CMD_EVENT_AUDIO_START, NULL))
   {
      audio_driver_active = false;
      return false;
   }
   return true;
}

bool audio_driver_start(bool is_shutdown)
{
   if (!current_audio || !current_audio->start 
         || !audio_driver_context_audio_data)
      return false;
   return current_audio->start(audio_driver_context_audio_data, is_shutdown);
}

bool audio_driver_stop(void)
{
   if (!current_audio || !current_audio->stop 
         || !audio_driver_context_audio_data)
      return false;
   return current_audio->stop(audio_driver_context_audio_data);
}

void audio_driver_unset_callback(void)
{
   audio_callback.callback  = NULL;
   audio_callback.set_state = NULL;
}

bool audio_driver_alive(void)
{
   if (     current_audio 
         && current_audio->alive 
         && audio_driver_context_audio_data)
      return current_audio->alive(audio_driver_context_audio_data);
   return false;
}

void audio_driver_frame_is_reverse(void)
{
   /* We just rewound. Flush rewind audio buffer. */
   audio_driver_flush(
         audio_driver_rewind_buf + audio_driver_rewind_ptr,
         audio_driver_rewind_size - audio_driver_rewind_ptr);
}

void audio_driver_destroy_data(void)
{
   audio_driver_context_audio_data = NULL;
}

void audio_driver_set_own_driver(void)
{
   audio_driver_data_own = true;
}

void audio_driver_unset_own_driver(void)
{
   audio_driver_data_own = false;
}

bool audio_driver_owns_driver(void)
{
   return audio_driver_data_own;
}

void audio_driver_set_active(void)
{
   audio_driver_active = true;
}

void audio_driver_unset_active(void)
{
   audio_driver_active = false;
}

void audio_driver_destroy(void)
{
   audio_driver_active   = false;
   audio_driver_data_own = false;
   current_audio         = NULL;
}
