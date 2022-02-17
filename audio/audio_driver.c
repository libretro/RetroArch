/**
 *  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation, either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with RetroArch. If not, see <http://www.gnu.org/licenses/>.
 **/

#include <math.h>

#include "audio_driver.h"

#include <string/stdstring.h>
#include <encodings/utf.h>
#include <clamping.h>
#include <retro_assert.h>
#include <memalign.h>
#include <audio/conversion/float_to_s16.h>
#include <audio/conversion/s16_to_float.h>
#ifdef HAVE_AUDIOMIXER
#include <audio/audio_mixer.h>
#include "../tasks/task_audio_mixer.h"
#endif
#ifdef HAVE_DSP_FILTER
#include <audio/dsp_filter.h>
#endif
#include <lists/dir_list.h>

#ifdef HAVE_THREADS
#include "audio_thread_wrapper.h"
#endif

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#endif

#ifdef HAVE_NETWORKING
#include "../network/netplay/netplay.h"
#endif

#include "../configuration.h"
#include "../driver.h"
#include "../frontend/frontend_driver.h"
#include "../retroarch.h"
#include "../list_special.h"
#include "../file_path_special.h"
#include "../record/record_driver.h"
#include "../tasks/task_content.h"
#include "../verbosity.h"

#define MENU_SOUND_FORMATS "ogg|mod|xm|s3m|mp3|flac|wav"

 /* Converts decibels to voltage gain. returns voltage gain value. */
#define DB_TO_GAIN(db) (powf(10.0f, (db) / 20.0f))

audio_driver_t audio_null = {
   NULL, /* init */
   NULL, /* write */
   NULL, /* stop */
   NULL, /* start */
   NULL, /* alive */
   NULL, /* set_nonblock_state */
   NULL, /* free */
   NULL, /* use_float */
   "null",
   NULL,
   NULL,
   NULL, /* write_avail */
   NULL
};

audio_driver_t *audio_drivers[] = {
#ifdef HAVE_ALSA
   &audio_alsa,
#if !defined(__QNX__) && defined(HAVE_THREADS)
   &audio_alsathread,
#endif
#endif
#ifdef HAVE_TINYALSA
   &audio_tinyalsa,
#endif
#if defined(HAVE_AUDIOIO)
   &audio_audioio,
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
#ifdef HAVE_COREAUDIO3
   &audio_coreaudio3,
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
#ifdef HAVE_WASAPI
   &audio_wasapi,
#endif
#ifdef HAVE_PULSE
   &audio_pulse,
#endif
#if defined(__PSL1GHT__) || defined(__PS3__)
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
#if defined(PSP) || defined(VITA) || defined(ORBIS)
  &audio_psp,
#endif
#if defined(PS2)
  &audio_ps2,
#endif
#ifdef _3DS
   &audio_ctr_csnd,
   &audio_ctr_dsp,
#ifdef HAVE_THREADS
   &audio_ctr_dsp_thread,
#endif
#endif
#ifdef SWITCH
   &audio_switch,
   &audio_switch_thread,
#ifdef HAVE_LIBNX
   &audio_switch_libnx_audren,
   &audio_switch_libnx_audren_thread,
#endif
#endif
   &audio_null,
   NULL,
};

static audio_driver_state_t audio_driver_st = {0}; /* double alignment */

/**************************************/

audio_driver_state_t *audio_state_get_ptr(void)
{
   return &audio_driver_st;
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

#ifdef HAVE_TRANSLATE
/* TODO/FIXME - Doesn't currently work.  Fix this. */
bool audio_driver_is_ai_service_speech_running(void)
{
#ifdef HAVE_AUDIOMIXER
   enum audio_mixer_state res = audio_driver_mixer_get_stream_state(10);
   bool ret = (res == AUDIO_STREAM_STATE_NONE) || (res == AUDIO_STREAM_STATE_STOPPED);
   if (!ret)
      return true;
#endif
   return false;
}
#endif

static enum resampler_quality audio_driver_get_resampler_quality(
      settings_t *settings)
{
   if (settings)
      return (enum resampler_quality)settings->uints.audio_resampler_quality;
   return RESAMPLER_QUALITY_DONTCARE;
}

static bool audio_driver_free_devices_list(void)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   if (
            !audio_st->current_audio 
         || !audio_st->current_audio->device_list_free
         || !audio_st->context_audio_data)
      return false;
   audio_st->current_audio->device_list_free(
         audio_st->context_audio_data,
         audio_st->devices_list);
   audio_st->devices_list = NULL;
   return true;
}

#ifdef DEBUG
static void report_audio_buffer_statistics(void)
{
   audio_statistics_t audio_stats;
   audio_stats.samples                   = 0;
   audio_stats.average_buffer_saturation = 0.0f;
   audio_stats.std_deviation_percentage  = 0.0f;
   audio_stats.close_to_underrun         = 0.0f;
   audio_stats.close_to_blocking         = 0.0f;

   if (!audio_compute_buffer_statistics(&audio_stats))
      return;

   RARCH_LOG("[Audio]: Average audio buffer saturation: %.2f %%,"
         " standard deviation (percentage points): %.2f %%.\n"
         "[Audio]: Amount of time spent close to underrun: %.2f %%."
         " Close to blocking: %.2f %%.\n",
         audio_stats.average_buffer_saturation,
         audio_stats.std_deviation_percentage,
         audio_stats.close_to_underrun,
         audio_stats.close_to_blocking);
}
#endif

static void audio_driver_deinit_resampler(void)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   if (audio_st->resampler && audio_st->resampler_data)
      audio_st->resampler->free(audio_st->resampler_data);
   audio_st->resampler          = NULL;
   audio_st->resampler_data     = NULL;
   audio_st->resampler_ident[0] = '\0';
   audio_st->resampler_quality  = RESAMPLER_QUALITY_DONTCARE;
}


static bool audio_driver_deinit_internal(bool audio_enable)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   if (     audio_st->current_audio 
         && audio_st->current_audio->free)
   {
      if (audio_st->context_audio_data)
         audio_st->current_audio->free(audio_st->context_audio_data);
      audio_st->context_audio_data = NULL;
   }

   if (audio_st->output_samples_conv_buf)
      memalign_free(audio_st->output_samples_conv_buf);
   audio_st->output_samples_conv_buf     = NULL;

   if (audio_st->input_data)
      memalign_free(audio_st->input_data);

   audio_st->input_data = NULL;
   audio_st->data_ptr   = 0;

#ifdef HAVE_REWIND
   if (audio_st->rewind_buf)
      memalign_free(audio_st->rewind_buf);
   audio_st->rewind_buf  = NULL;
   audio_st->rewind_size = 0;
#endif

   if (!audio_enable)
   {
      audio_st->active   = false;
      return false;
   }

   audio_driver_deinit_resampler();

   if (audio_st->output_samples_buf)
      memalign_free(audio_st->output_samples_buf);
   audio_st->output_samples_buf = NULL;

#ifdef HAVE_DSP_FILTER
   audio_driver_dsp_filter_free();
#endif
#ifdef DEBUG
   report_audio_buffer_statistics();
#endif

   return true;
}

#ifdef HAVE_AUDIOMIXER
static void audio_driver_mixer_deinit(void)
{
   unsigned i;

   audio_driver_st.mixer_active = false;

   for (i = 0; i < AUDIO_MIXER_MAX_SYSTEM_STREAMS; i++)
   {
      audio_driver_mixer_stop_stream(i);
      audio_driver_mixer_remove_stream(i);
   }

   audio_mixer_done();
}
#endif

bool audio_driver_deinit(void)
{
   settings_t *settings = config_get_ptr();
#ifdef HAVE_AUDIOMIXER
   audio_driver_mixer_deinit();
#endif
   audio_driver_free_devices_list();

   return audio_driver_deinit_internal(
         settings->bools.audio_enable);
}

bool audio_driver_find_driver(
      void *settings_data,
      const char *prefix,
      bool verbosity_enabled)
{
   settings_t *settings    = (settings_t*)settings_data;
   int i                   = (int)driver_find_index(
         "audio_driver",
         settings->arrays.audio_driver);

   if (i >= 0)
      audio_driver_st.current_audio = (const audio_driver_t*)
         audio_drivers[i];
   else
   {
      const audio_driver_t *tmp = NULL;
      if (verbosity_enabled)
      {
         unsigned d;
         RARCH_ERR("Couldn't find any %s named \"%s\"\n", prefix,
               settings->arrays.audio_driver);
         RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
         for (d = 0; audio_drivers[d]; d++)
         {
            if (audio_drivers[d])
               RARCH_LOG_OUTPUT("\t%s\n", audio_drivers[d]->ident);
         }
         RARCH_WARN("Going to default to first %s...\n", prefix);
      }

      tmp = (const audio_driver_t*)audio_drivers[0];

      if (!tmp)
         return false;
      audio_driver_st.current_audio = tmp;
   }

   return true;
}

/**
 * audio_driver_flush:
 * @data                 : pointer to audio buffer.
 * @right                : amount of samples to write.
 *
 * Writes audio samples to audio driver. Will first
 * perform DSP processing (if enabled) and resampling.
 **/
static void audio_driver_flush(
      audio_driver_state_t *audio_st,
      float slowmotion_ratio,
      bool audio_fastforward_mute,
      const int16_t *data, size_t samples,
      bool is_slowmotion, bool is_fastmotion)
{
   struct resampler_data src_data;
   float audio_volume_gain           = (audio_st->mute_enable ||
         (audio_fastforward_mute && is_fastmotion))
               ? 0.0f 
               : audio_st->volume_gain;

   src_data.data_out                 = NULL;
   src_data.output_frames            = 0;

   convert_s16_to_float(audio_st->input_data, data, samples,
         audio_volume_gain);

   src_data.data_in                  = audio_st->input_data;
   src_data.input_frames             = samples >> 1;

#ifdef HAVE_DSP_FILTER
   if (audio_st->dsp)
   {
      struct retro_dsp_data dsp_data;

      dsp_data.input                 = NULL;
      dsp_data.input_frames          = 0;
      dsp_data.output                = NULL;
      dsp_data.output_frames         = 0;

      dsp_data.input                 = audio_st->input_data;
      dsp_data.input_frames          = (unsigned)(samples >> 1);

      retro_dsp_filter_process(audio_st->dsp, &dsp_data);

      if (dsp_data.output)
      {
         src_data.data_in            = dsp_data.output;
         src_data.input_frames       = dsp_data.output_frames;
      }
   }
#endif

   src_data.data_out                 = audio_st->output_samples_buf;

   if (audio_st->control)
   {
      /* Readjust the audio input rate. */
      int      half_size             = (int)(audio_st->buffer_size / 2);
         
      int      avail               =
          (int)audio_st->current_audio->write_avail(
               audio_st->context_audio_data);
      int      delta_mid           = avail - half_size;
      double   direction           = (double)delta_mid / half_size;
      double   adjust              = 1.0 +
         audio_st->rate_control_delta * direction;
      unsigned write_idx           =
         audio_st->free_samples_count++ &
         (AUDIO_BUFFER_FREE_SAMPLES_COUNT - 1);

      audio_st->free_samples_buf[write_idx]  = avail;
      audio_st->source_ratio_current         =
         audio_st->source_ratio_original * adjust;

#if 0
      if (verbosity_is_enabled())
      {
         RARCH_LOG_OUTPUT("[Audio]: Audio buffer is %u%% full\n",
               (unsigned)(100 - (avail * 100) /
                  audio_st->buffer_size));
         RARCH_LOG_OUTPUT("[Audio]: New rate: %lf, Orig rate: %lf\n",
               audio_st->source_ratio_current,
               audio_st->source_ratio_original);
      }
#endif
   }

   src_data.ratio           = audio_st->source_ratio_current;

   if (is_slowmotion)
      src_data.ratio       *= slowmotion_ratio;

   /* Note: Ideally we would divide by the user-configured
    * 'fastforward_ratio' when fast forward is enabled,
    * but in practice this doesn't work:
    * - 'fastforward_ratio' is only a limit. If the host
    *   cannot push frames fast enough, the actual ratio
    *   will be lower - and crackling will ensue
    * - Most of the time 'fastforward_ratio' will be
    *   zero (unlimited)
    * So what we would need to do is measure the time since
    * the last audio flush operation, and calculate a 'real'
    * fast-forward ratio - but this doesn't work either.
    * The measurement is inaccurate and the frame-by-frame
    * fluctuations are too large, so crackling is unavoidable.
    * Since it's going to crackle anyway, there's no point
    * trying to do anything. Just leave the ratio as-is,
    * and hope for the best... */

   audio_st->resampler->process(
         audio_st->resampler_data, &src_data);

#ifdef HAVE_AUDIOMIXER
   if (audio_st->mixer_active)
   {
      bool override                       = true;
      float mixer_gain                    = 0.0f;
      bool audio_driver_mixer_mute_enable = audio_st->mixer_mute_enable;

      if (!audio_driver_mixer_mute_enable)
      {
         if (audio_st->mixer_volume_gain == 1.0f)
            override                      = false;
         mixer_gain                       = audio_st->mixer_volume_gain;
            
      }
      audio_mixer_mix(audio_st->output_samples_buf,
            src_data.output_frames, mixer_gain, override);
   }
#endif

   {
      const void *output_data = audio_st->output_samples_buf;
      unsigned output_frames  = (unsigned)src_data.output_frames;

      if (audio_st->use_float)
         output_frames       *= sizeof(float);
      else
      {
         convert_float_to_s16(audio_st->output_samples_conv_buf,
               (const float*)output_data, output_frames * 2);

         output_data          = audio_st->output_samples_conv_buf;
         output_frames       *= sizeof(int16_t);
      }

      audio_st->current_audio->write(audio_st->context_audio_data,
            output_data, output_frames * 2);
   }
}

#ifdef HAVE_AUDIOMIXER
audio_mixer_stream_t *audio_driver_mixer_get_stream(unsigned i)
{
   if (i > (AUDIO_MIXER_MAX_SYSTEM_STREAMS-1))
      return NULL;
   return &audio_driver_st.mixer_streams[i];
}

const char *audio_driver_mixer_get_stream_name(unsigned i)
{
   if (i > (AUDIO_MIXER_MAX_SYSTEM_STREAMS-1))
      return msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE);
   if (!string_is_empty(audio_driver_st.mixer_streams[i].name))
      return audio_driver_st.mixer_streams[i].name;
   return msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE);
}

#endif

bool audio_driver_init_internal(
      void *settings_data,
      bool audio_cb_inited)
{
   unsigned new_rate       = 0;
   float  *samples_buf     = NULL;
   settings_t *settings    = (settings_t*)settings_data;
   size_t max_bufsamples   = AUDIO_CHUNK_SIZE_NONBLOCKING * 2;
   bool audio_enable       = settings->bools.audio_enable;
   bool audio_sync         = settings->bools.audio_sync;
   bool audio_rate_control = settings->bools.audio_rate_control;
   float slowmotion_ratio  = settings->floats.slowmotion_ratio;
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   unsigned audio_latency  = (runloop_st->audio_latency > settings->uints.audio_latency) ?
         runloop_st->audio_latency : settings->uints.audio_latency;
#ifdef HAVE_REWIND
   int16_t *rewind_buf     = NULL;
#endif
   /* Accomodate rewind since at some point we might have two full buffers. */
   size_t outsamples_max   = AUDIO_CHUNK_SIZE_NONBLOCKING * 2 * AUDIO_MAX_RATIO * slowmotion_ratio;
   int16_t *conv_buf       = (int16_t*)memalign_alloc(64, outsamples_max * sizeof(int16_t));
   float *audio_buf        = (float*)memalign_alloc(64, AUDIO_CHUNK_SIZE_NONBLOCKING * 2 * sizeof(float));
   bool verbosity_enabled  = verbosity_is_enabled();

   convert_s16_to_float_init_simd();
   convert_float_to_s16_init_simd();

   /* Used for recording even if audio isn't enabled. */
   retro_assert(conv_buf != NULL);
   retro_assert(audio_buf != NULL);

   if (!conv_buf || !audio_buf)
      goto error;

   memset(audio_buf, 0, AUDIO_CHUNK_SIZE_NONBLOCKING * 2 * sizeof(float));

   audio_driver_st.input_data              = audio_buf;
   audio_driver_st.output_samples_conv_buf = conv_buf;
   audio_driver_st.chunk_block_size        = AUDIO_CHUNK_SIZE_BLOCKING;
   audio_driver_st.chunk_nonblock_size     = AUDIO_CHUNK_SIZE_NONBLOCKING;
   audio_driver_st.chunk_size              = audio_driver_st.chunk_block_size;

#ifdef HAVE_REWIND
   /* Needs to be able to hold full content of a full max_bufsamples
    * in addition to its own. */
   rewind_buf = (int16_t*)memalign_alloc(64, max_bufsamples * sizeof(int16_t));
   retro_assert(rewind_buf != NULL);

   if (!rewind_buf)
      goto error;

   audio_driver_st.rewind_buf              = rewind_buf;
   audio_driver_st.rewind_size             = max_bufsamples;
#endif

   if (!audio_enable)
   {
      audio_driver_st.active = false;
      return false;
   }

   if (!(audio_driver_find_driver(settings,
         "audio driver", verbosity_enabled)))
   {
      RARCH_ERR("Failed to initialize audio driver.\n");
      return false;
   }

   if (!audio_driver_st.current_audio || !audio_driver_st.current_audio->init)
   {
      RARCH_ERR("Failed to initialize audio driver. Will continue without audio.\n");
      audio_driver_st.active = false;
      return false;
   }

#ifdef HAVE_THREADS
   if (audio_cb_inited)
   {
      RARCH_LOG("[Audio]: Starting threaded audio driver ...\n");
      if (!audio_init_thread(
               &audio_driver_st.current_audio,
               &audio_driver_st.context_audio_data,
               *settings->arrays.audio_device
               ? settings->arrays.audio_device : NULL,
               settings->uints.audio_output_sample_rate, &new_rate,
               audio_latency,
               settings->uints.audio_block_frames,
               audio_driver_st.current_audio))
      {
         RARCH_ERR("Cannot open threaded audio driver ... Exiting ...\n");
	 return false;
      }
   }
   else
#endif
   {
      audio_driver_st.context_audio_data =
         audio_driver_st.current_audio->init(*settings->arrays.audio_device ?
               settings->arrays.audio_device : NULL,
               settings->uints.audio_output_sample_rate,
               audio_latency,
               settings->uints.audio_block_frames,
               &new_rate);
   }

   if (new_rate != 0)
      configuration_set_int(settings, settings->uints.audio_output_sample_rate, new_rate);

   if (!audio_driver_st.context_audio_data)
   {
      RARCH_ERR("Failed to initialize audio driver. Will continue without audio.\n");
      audio_driver_st.active    = false;
   }

   audio_driver_st.use_float    = false;
   if (     audio_driver_st.active
         && audio_driver_st.current_audio->use_float(
            audio_driver_st.context_audio_data))
      audio_driver_st.use_float = true;

   if (!audio_sync && audio_driver_st.active)
   {
      if (audio_driver_st.active &&
            audio_driver_st.context_audio_data)
         audio_driver_st.current_audio->set_nonblock_state(
               audio_driver_st.context_audio_data, true);

      audio_driver_st.chunk_size =
         audio_driver_st.chunk_nonblock_size;
   }

   if (audio_driver_st.input <= 0.0f)
   {
      /* Should never happen. */
      RARCH_WARN("[Audio]: Input rate is invalid (%.3f Hz)."
            " Using output rate (%u Hz).\n",
            audio_driver_st.input, settings->uints.audio_output_sample_rate);

      audio_driver_st.input = settings->uints.audio_output_sample_rate;
   }

   audio_driver_st.source_ratio_original   =
      audio_driver_st.source_ratio_current =
      (double)settings->uints.audio_output_sample_rate / audio_driver_st.input;

   if (!string_is_empty(settings->arrays.audio_resampler))
      strlcpy(audio_driver_st.resampler_ident,
            settings->arrays.audio_resampler,
            sizeof(audio_driver_st.resampler_ident));
   else
      audio_driver_st.resampler_ident[0] = '\0';

   audio_driver_st.resampler_quality =
         audio_driver_get_resampler_quality(settings);

   if (!retro_resampler_realloc(
            &audio_driver_st.resampler_data,
            &audio_driver_st.resampler,
            audio_driver_st.resampler_ident,
            audio_driver_st.resampler_quality,
            audio_driver_st.source_ratio_original))
   {
      RARCH_ERR("Failed to initialize resampler \"%s\".\n",
            audio_driver_st.resampler_ident);
      audio_driver_st.active = false;
   }

   audio_driver_st.data_ptr   = 0;

   retro_assert(settings->uints.audio_output_sample_rate <
         audio_driver_st.input * AUDIO_MAX_RATIO);

   samples_buf = (float*)memalign_alloc(64, outsamples_max * sizeof(float));

   retro_assert(samples_buf != NULL);

   if (!samples_buf)
      goto error;

   audio_driver_st.output_samples_buf = (float*)samples_buf;
   audio_driver_st.control            = false;

   if (
         !audio_cb_inited
         && audio_driver_st.active
         && audio_rate_control
         )
   {
      /* Audio rate control requires write_avail
       * and buffer_size to be implemented. */
      if (audio_driver_st.current_audio->buffer_size)
      {
         audio_driver_st.buffer_size =
            audio_driver_st.current_audio->buffer_size(
                  audio_driver_st.context_audio_data);
         audio_driver_st.control     = true;
      }
      else
         RARCH_WARN("[Audio]: Rate control was desired, but driver does not support needed features.\n");
   }

   command_event(CMD_EVENT_DSP_FILTER_INIT, NULL);

   audio_driver_st.free_samples_count = 0;

#ifdef HAVE_AUDIOMIXER
   audio_mixer_init(settings->uints.audio_output_sample_rate);
#endif

   /* Threaded driver is initially stopped. */
   if (
         audio_driver_st.active
         && audio_cb_inited
         )
      audio_driver_start(false);

   return true;

error:
   return audio_driver_deinit();
}

void audio_driver_sample(int16_t left, int16_t right)
{
   audio_driver_state_t *audio_st  = &audio_driver_st;
   recording_state_t *recording_st = NULL;
   runloop_state_t *runloop_st     = NULL;
   if (audio_st->suspended)
      return;
   audio_st->output_samples_conv_buf[audio_st->data_ptr++] = left;
   audio_st->output_samples_conv_buf[audio_st->data_ptr++] = right;

   if (audio_st->data_ptr < audio_st->chunk_size)
      return;

   runloop_st                      = runloop_state_get_ptr();
   recording_st                    = recording_state_get_ptr();

   if (  recording_st->data     &&
         recording_st->driver   &&
         recording_st->driver->push_audio)
   {
      struct record_audio_data ffemu_data;

      ffemu_data.data                    = audio_st->output_samples_conv_buf;
      ffemu_data.frames                  = audio_st->data_ptr / 2;

      recording_st->driver->push_audio(recording_st->data, &ffemu_data);
   }

   if (!(    runloop_st->paused
		   || !audio_st->active
		   || !audio_st->output_samples_buf))
      audio_driver_flush(audio_st,
            config_get_ptr()->floats.slowmotion_ratio,
            config_get_ptr()->bools.audio_fastforward_mute,
            audio_st->output_samples_conv_buf,
            audio_st->data_ptr,
            runloop_st->slowmotion,
            runloop_st->fastmotion);

   audio_st->data_ptr = 0;
}

size_t audio_driver_sample_batch(const int16_t *data, size_t frames)
{
   size_t frames_remaining        = frames;
   recording_state_t *record_st   = recording_state_get_ptr();
   runloop_state_t *runloop_st    = runloop_state_get_ptr();
   audio_driver_state_t *audio_st = &audio_driver_st;

   if (audio_st->suspended || (frames < 1))
      return frames;

   /* We want to run this loop at least once, so use a
    * do...while (do...while has only a single conditional
    * jump, as opposed to for and while which have a
    * conditional jump and an unconditional jump). Note,
    * however, that this is only relevant for compilers
    * that are poor at optimisation... */
   do
   {
      size_t frames_to_write =
            (frames_remaining > (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1)) ?
                  (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1) : frames_remaining;

      if (    record_st->data
           && record_st->driver
           && record_st->driver->push_audio)
      {
         struct record_audio_data ffemu_data;

         ffemu_data.data   = data;
         ffemu_data.frames = frames_to_write;

         record_st->driver->push_audio(record_st->data, &ffemu_data);
      }

      if (!(    runloop_st->paused
            || !audio_st->active
            || !audio_st->output_samples_buf))
         audio_driver_flush(audio_st,
               config_get_ptr()->floats.slowmotion_ratio,
               config_get_ptr()->bools.audio_fastforward_mute,
               data,
               frames_to_write << 1,
               runloop_st->slowmotion,
               runloop_st->fastmotion);

      frames_remaining -= frames_to_write;
      data             += frames_to_write << 1;
   }
   while (frames_remaining > 0);

   return frames;
}

#ifdef HAVE_REWIND
void audio_driver_sample_rewind(int16_t left, int16_t right)
{
   audio_driver_state_t *audio_st  = &audio_driver_st;
   if (audio_st->rewind_ptr == 0)
      return;

   audio_st->rewind_buf[--audio_st->rewind_ptr] = right;
   audio_st->rewind_buf[--audio_st->rewind_ptr] = left;
}

size_t audio_driver_sample_batch_rewind(
      const int16_t *data, size_t frames)
{
   size_t i;
   audio_driver_state_t *audio_st  = &audio_driver_st;
   size_t              samples     = frames << 1;

   for (i = 0; i < samples; i++)
   {
      if (audio_st->rewind_ptr < 1)
         break;

      audio_st->rewind_buf[--audio_st->rewind_ptr] = data[i];
   }

   return frames;
}
#endif

#ifdef HAVE_DSP_FILTER
void audio_driver_dsp_filter_free(void)
{
   audio_driver_state_t *audio_st  = &audio_driver_st;
   if (audio_st->dsp)
      retro_dsp_filter_free(audio_st->dsp);
   audio_st->dsp = NULL;
}

bool audio_driver_dsp_filter_init(const char *device)
{
   retro_dsp_filter_t *audio_driver_dsp = NULL;
   struct string_list *plugs            = NULL;
#if defined(HAVE_DYLIB) && !defined(HAVE_FILTERS_BUILTIN)
   char basedir[PATH_MAX_LENGTH];
   char ext_name[PATH_MAX_LENGTH];

   basedir[0] = ext_name[0]             = '\0';

   fill_pathname_basedir(basedir, device, sizeof(basedir));

   if (!frontend_driver_get_core_extension(ext_name, sizeof(ext_name)))
      return false;

   plugs = dir_list_new(basedir, ext_name, false, true, false, false);
   if (!plugs)
      return false;
#endif
   audio_driver_dsp = retro_dsp_filter_new(
         device, plugs, audio_driver_st.input);
   if (!audio_driver_dsp)
      return false;

   audio_driver_st.dsp = audio_driver_dsp;

   return true;
}
#endif

void audio_driver_set_buffer_size(size_t bufsize)
{
   audio_driver_st.buffer_size = bufsize;
}

float audio_driver_monitor_adjust_system_rates(
      double input_sample_rate,
      double input_fps,
      float video_refresh_rate,
      unsigned video_swap_interval,
      float audio_max_timing_skew)
{
   float inp_sample_rate                  = input_sample_rate;
   const float target_video_sync_rate     = video_refresh_rate
   / video_swap_interval;
   float timing_skew                      =
      fabs(1.0f - input_fps / target_video_sync_rate);
   if (timing_skew <= audio_max_timing_skew)
      return (inp_sample_rate * target_video_sync_rate / input_fps);
   return inp_sample_rate;
}

#ifdef HAVE_REWIND
void audio_driver_setup_rewind(void)
{
   unsigned i;
   audio_driver_state_t *audio_st  = &audio_driver_st;

   /* Push audio ready to be played. */
   audio_st->rewind_ptr = audio_st->rewind_size;

   for (i = 0; i < audio_st->data_ptr; i += 2)
   {
      if (audio_st->rewind_ptr > 0)
         audio_st->rewind_buf[--audio_st->rewind_ptr] =
            audio_st->output_samples_conv_buf[i + 1];

      if (audio_st->rewind_ptr > 0)
         audio_st->rewind_buf[--audio_st->rewind_ptr] =
            audio_st->output_samples_conv_buf[i + 0];
   }

   audio_st->data_ptr = 0;
}
#endif

bool audio_driver_get_devices_list(void **data)
{
   struct string_list**ptr     = (struct string_list**)data;
   if (!ptr)
      return false;
   *ptr = audio_driver_st.devices_list;
   return true;
}

#ifdef HAVE_AUDIOMIXER
bool audio_driver_mixer_extension_supported(const char *ext)
{
   unsigned i;
   struct string_list str_list;
   union string_list_elem_attr attr;
   bool ret                      = false;

   attr.i = 0;
   if (!string_list_initialize(&str_list))
      return false;

#ifdef HAVE_STB_VORBIS
   string_list_append(&str_list, "ogg", attr);
#endif
#ifdef HAVE_IBXM
   string_list_append(&str_list, "mod", attr);
   string_list_append(&str_list, "s3m", attr);
   string_list_append(&str_list, "xm", attr);
#endif
#ifdef HAVE_DR_FLAC
   string_list_append(&str_list, "flac", attr);
#endif
#ifdef HAVE_DR_MP3
   string_list_append(&str_list, "mp3", attr);
#endif
   string_list_append(&str_list, "wav", attr);

   for (i = 0; i < str_list.size; i++)
   {
      const char *str_ext = str_list.elems[i].data;
      if (string_is_equal_noncase(str_ext, ext))
      {
         ret = true;
         break;
      }
   }

   string_list_deinitialize(&str_list);

   return ret;
}

static int audio_mixer_find_index(
      audio_mixer_sound_t *sound)
{
   unsigned i;

   for (i = 0; i < AUDIO_MIXER_MAX_SYSTEM_STREAMS; i++)
   {
      audio_mixer_sound_t *handle = audio_driver_st.mixer_streams[i].handle;
      if (handle == sound)
         return i;
   }
   return -1;
}

static void audio_mixer_play_stop_cb(
      audio_mixer_sound_t *sound, unsigned reason)
{
   int                     idx = audio_mixer_find_index(sound);

   switch (reason)
   {
      case AUDIO_MIXER_SOUND_FINISHED:
         audio_mixer_destroy(sound);

         if (idx >= 0)
         {
            unsigned i = (unsigned)idx;

            if (!string_is_empty(audio_driver_st.mixer_streams[i].name))
               free(audio_driver_st.mixer_streams[i].name);

            audio_driver_st.mixer_streams[i].name    = NULL;
            audio_driver_st.mixer_streams[i].state   = AUDIO_STREAM_STATE_NONE;
            audio_driver_st.mixer_streams[i].volume  = 0.0f;
            audio_driver_st.mixer_streams[i].buf     = NULL;
            audio_driver_st.mixer_streams[i].stop_cb = NULL;
            audio_driver_st.mixer_streams[i].handle  = NULL;
            audio_driver_st.mixer_streams[i].voice   = NULL;
         }
         break;
      case AUDIO_MIXER_SOUND_STOPPED:
         break;
      case AUDIO_MIXER_SOUND_REPEATED:
         break;
   }
}

static void audio_mixer_menu_stop_cb(
      audio_mixer_sound_t *sound, unsigned reason)
{
   int                     idx = audio_mixer_find_index(sound);

   switch (reason)
   {
      case AUDIO_MIXER_SOUND_FINISHED:
         if (idx >= 0)
         {
            unsigned i                              = (unsigned)idx;
            audio_driver_st.mixer_streams[i].state   = AUDIO_STREAM_STATE_STOPPED;
            audio_driver_st.mixer_streams[i].volume  = 0.0f;
         }
         break;
      case AUDIO_MIXER_SOUND_STOPPED:
         break;
      case AUDIO_MIXER_SOUND_REPEATED:
         break;
   }
}

static void audio_mixer_play_stop_sequential_cb(
      audio_mixer_sound_t *sound, unsigned reason)
{
   int                     idx = audio_mixer_find_index(sound);

   switch (reason)
   {
      case AUDIO_MIXER_SOUND_FINISHED:
         audio_mixer_destroy(sound);

         if (idx >= 0)
         {
            unsigned i = (unsigned)idx;

            if (!string_is_empty(audio_driver_st.mixer_streams[i].name))
               free(audio_driver_st.mixer_streams[i].name);

            if (i < AUDIO_MIXER_MAX_STREAMS)
               audio_driver_st.mixer_streams[i].stream_type = AUDIO_STREAM_TYPE_USER;
            else
               audio_driver_st.mixer_streams[i].stream_type = AUDIO_STREAM_TYPE_SYSTEM;

            audio_driver_st.mixer_streams[i].name           = NULL;
            audio_driver_st.mixer_streams[i].state          = AUDIO_STREAM_STATE_NONE;
            audio_driver_st.mixer_streams[i].volume         = 0.0f;
            audio_driver_st.mixer_streams[i].buf            = NULL;
            audio_driver_st.mixer_streams[i].stop_cb        = NULL;
            audio_driver_st.mixer_streams[i].handle         = NULL;
            audio_driver_st.mixer_streams[i].voice          = NULL;

            i++;

            for (; i < AUDIO_MIXER_MAX_SYSTEM_STREAMS; i++)
            {
               if (audio_driver_st.mixer_streams[i].state
                     == AUDIO_STREAM_STATE_STOPPED)
               {
                  audio_driver_mixer_play_stream_sequential(i);
                  break;
               }
            }
         }
         break;
      case AUDIO_MIXER_SOUND_STOPPED:
         break;
      case AUDIO_MIXER_SOUND_REPEATED:
         break;
   }
}

static bool audio_driver_mixer_get_free_stream_slot(
      unsigned *id, enum audio_mixer_stream_type type)
{
   unsigned                  i = AUDIO_MIXER_MAX_STREAMS;
   unsigned              count = AUDIO_MIXER_MAX_SYSTEM_STREAMS;

   if (type == AUDIO_STREAM_TYPE_USER)
   {
      i     = 0;
      count = AUDIO_MIXER_MAX_STREAMS;
   }

   for (; i < count; i++)
   {
      if (audio_driver_st.mixer_streams[i].state == AUDIO_STREAM_STATE_NONE)
      {
         *id = i;
         return true;
      }
   }

   return false;
}

bool audio_driver_mixer_add_stream(audio_mixer_stream_params_t *params)
{
   unsigned free_slot            = 0;
   audio_mixer_voice_t *voice    = NULL;
   audio_mixer_sound_t *handle   = NULL;
   audio_mixer_stop_cb_t stop_cb = audio_mixer_play_stop_cb;
   bool looped                   = false;
   void *buf                     = NULL;

   if (params->stream_type == AUDIO_STREAM_TYPE_NONE)
      return false;

   switch (params->slot_selection_type)
   {
      case AUDIO_MIXER_SLOT_SELECTION_MANUAL:
         free_slot = params->slot_selection_idx;

         /* If we are using a manually specified
          * slot, must free any existing stream
          * before assigning the new one */
         audio_driver_mixer_stop_stream(free_slot);
         audio_driver_mixer_remove_stream(free_slot);

         break;
      case AUDIO_MIXER_SLOT_SELECTION_AUTOMATIC:
      default:
         if (!audio_driver_mixer_get_free_stream_slot(
                  &free_slot, params->stream_type))
            return false;
         break;
   }

   if (params->state == AUDIO_STREAM_STATE_NONE)
      return false;

   buf = malloc(params->bufsize);

   if (!buf)
      return false;

   memcpy(buf, params->buf, params->bufsize);

   switch (params->type)
   {
      case AUDIO_MIXER_TYPE_WAV:
         handle = audio_mixer_load_wav(buf, (int32_t)params->bufsize,
               audio_driver_st.resampler_ident,
               audio_driver_st.resampler_quality);
         /* WAV is a special case - input buffer is not
          * free()'d when sound playback is complete (it is
          * converted to a PCM buffer, which is free()'d instead),
          * so have to do it here */
         free(buf);
         buf = NULL;
         break; 
      case AUDIO_MIXER_TYPE_OGG:
         handle = audio_mixer_load_ogg(buf, (int32_t)params->bufsize);
         break;
      case AUDIO_MIXER_TYPE_MOD:
         handle = audio_mixer_load_mod(buf, (int32_t)params->bufsize);
         break;
      case AUDIO_MIXER_TYPE_FLAC:
#ifdef HAVE_DR_FLAC
         handle = audio_mixer_load_flac(buf, (int32_t)params->bufsize);
#endif
         break;
      case AUDIO_MIXER_TYPE_MP3:
#ifdef HAVE_DR_MP3
         handle = audio_mixer_load_mp3(buf, (int32_t)params->bufsize);
#endif
         break;
      case AUDIO_MIXER_TYPE_NONE:
         break;
   }

   if (!handle)
   {
      free(buf);
      return false;
   }

   switch (params->state)
   {
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
         looped = true;
         voice = audio_mixer_play(handle, looped, params->volume,
               audio_driver_st.resampler_ident,
               audio_driver_st.resampler_quality, stop_cb);
         break;
      case AUDIO_STREAM_STATE_PLAYING:
         voice = audio_mixer_play(handle, looped, params->volume,
               audio_driver_st.resampler_ident,
               audio_driver_st.resampler_quality, stop_cb);
         break;
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
         stop_cb = audio_mixer_play_stop_sequential_cb;
         voice = audio_mixer_play(handle, looped, params->volume,
               audio_driver_st.resampler_ident,
               audio_driver_st.resampler_quality, stop_cb);
         break;
      default:
         break;
   }

   audio_driver_st.mixer_active                         = true;

   audio_driver_st.mixer_streams[free_slot].name        =
      !string_is_empty(params->basename) ? strdup(params->basename) : NULL;
   audio_driver_st.mixer_streams[free_slot].buf         = buf;
   audio_driver_st.mixer_streams[free_slot].handle      = handle;
   audio_driver_st.mixer_streams[free_slot].voice       = voice;
   audio_driver_st.mixer_streams[free_slot].stream_type = params->stream_type;
   audio_driver_st.mixer_streams[free_slot].type        = params->type;
   audio_driver_st.mixer_streams[free_slot].state       = params->state;
   audio_driver_st.mixer_streams[free_slot].volume      = params->volume;
   audio_driver_st.mixer_streams[free_slot].stop_cb     = stop_cb;

   return true;
}

enum audio_mixer_state audio_driver_mixer_get_stream_state(unsigned i)
{
   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return AUDIO_STREAM_STATE_NONE;

   return audio_driver_st.mixer_streams[i].state;
}

static void audio_driver_mixer_play_stream_internal(
      unsigned i, unsigned type)
{
   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;

   switch (audio_driver_st.mixer_streams[i].state)
   {
      case AUDIO_STREAM_STATE_STOPPED:
         audio_driver_st.mixer_streams[i].voice =
            audio_mixer_play(audio_driver_st.mixer_streams[i].handle,
               (type == AUDIO_STREAM_STATE_PLAYING_LOOPED) ? true : false,
               1.0f, audio_driver_st.resampler_ident,
               audio_driver_st.resampler_quality,
               audio_driver_st.mixer_streams[i].stop_cb);
         audio_driver_st.mixer_streams[i].state = (enum audio_mixer_state)type;
         break;
      case AUDIO_STREAM_STATE_PLAYING:
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
      case AUDIO_STREAM_STATE_NONE:
         break;
   }
}

static void audio_driver_load_menu_bgm_callback(retro_task_t *task,
      void *task_data, void *user_data, const char *error)
{
   bool contentless = false;
   bool is_inited   = false;

   content_get_status(&contentless, &is_inited);

   if (!is_inited)
      audio_driver_mixer_play_menu_sound_looped(AUDIO_MIXER_SYSTEM_SLOT_BGM);
}

void audio_driver_load_system_sounds(void)
{
   char sounds_path[PATH_MAX_LENGTH];
   char sounds_fallback_path[PATH_MAX_LENGTH];
   char basename_noext[PATH_MAX_LENGTH];
   settings_t *settings                  = config_get_ptr();
   const char *dir_assets                = settings->paths.directory_assets;
   const bool audio_enable_menu          = settings->bools.audio_enable_menu;
   const bool audio_enable_menu_ok       = audio_enable_menu && settings->bools.audio_enable_menu_ok;
   const bool audio_enable_menu_cancel   = audio_enable_menu && settings->bools.audio_enable_menu_cancel;
   const bool audio_enable_menu_notice   = audio_enable_menu && settings->bools.audio_enable_menu_notice;
   const bool audio_enable_menu_bgm      = audio_enable_menu && settings->bools.audio_enable_menu_bgm;
   const bool audio_enable_cheevo_unlock = settings->bools.cheevos_unlock_sound_enable;
   const char *path_ok                   = NULL;
   const char *path_cancel               = NULL;
   const char *path_notice               = NULL;
   const char *path_bgm                  = NULL;
   const char *path_cheevo_unlock        = NULL;
   struct string_list *list              = NULL;
   struct string_list *list_fallback     = NULL;
   unsigned i                            = 0;

   if (!audio_enable_menu && !audio_enable_cheevo_unlock)
      goto end;

   sounds_path[0] = sounds_fallback_path[0] =
                          basename_noext[0] ='\0';

   fill_pathname_join(
         sounds_fallback_path,
         dir_assets,
         "sounds",
         sizeof(sounds_fallback_path));

   fill_pathname_application_special(
         sounds_path,
         sizeof(sounds_path),
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_SOUNDS);

   list          = dir_list_new(sounds_path, MENU_SOUND_FORMATS, false, false, false, false);
   list_fallback = dir_list_new(sounds_fallback_path, MENU_SOUND_FORMATS, false, false, false, false);

   if (!list)
   {
      list          = list_fallback;
      list_fallback = NULL;
   }

   if (!list || list->size == 0)
      goto end;

   if (list_fallback && list_fallback->size > 0)
   {
      for (i = 0; i < list_fallback->size; i++)
      {
         if (list->size == 0 || !string_list_find_elem(list, list_fallback->elems[i].data))
         {
            union string_list_elem_attr attr = {0};
            string_list_append(list, list_fallback->elems[i].data, attr);
         }
      }
   }

   for (i = 0; i < list->size; i++)
   {
      const char *path = list->elems[i].data;
      const char *ext  = path_get_extension(path);

      if (audio_driver_mixer_extension_supported(ext))
      {
         basename_noext[0] = '\0';
         fill_pathname_base_noext(basename_noext, path, sizeof(basename_noext));

         if (string_is_equal_noncase(basename_noext, "ok"))
            path_ok = path;
         else if (string_is_equal_noncase(basename_noext, "cancel"))
            path_cancel = path;
         else if (string_is_equal_noncase(basename_noext, "notice"))
            path_notice = path;
         else if (string_is_equal_noncase(basename_noext, "bgm"))
            path_bgm = path;
         else if (string_is_equal_noncase(basename_noext, "unlock"))
            path_cheevo_unlock = path;
      }
   }

   if (path_ok && audio_enable_menu_ok)
      task_push_audio_mixer_load(path_ok, NULL, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_OK);
   if (path_cancel && audio_enable_menu_cancel)
      task_push_audio_mixer_load(path_cancel, NULL, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_CANCEL);
   if (path_notice && audio_enable_menu_notice)
      task_push_audio_mixer_load(path_notice, NULL, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_NOTICE);
   if (path_bgm && audio_enable_menu_bgm)
      task_push_audio_mixer_load(path_bgm, audio_driver_load_menu_bgm_callback, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_BGM);
   if (path_cheevo_unlock && audio_enable_cheevo_unlock)
      task_push_audio_mixer_load(path_cheevo_unlock, NULL, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_ACHIEVEMENT_UNLOCK);

end:
   if (list)
      string_list_free(list);
   if (list_fallback)
      string_list_free(list_fallback);
}

void audio_driver_mixer_play_stream(unsigned i)
{
   audio_driver_st.mixer_streams[i].stop_cb = audio_mixer_play_stop_cb;
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING);
}

void audio_driver_mixer_play_menu_sound_looped(unsigned i)
{
   audio_driver_st.mixer_streams[i].stop_cb = audio_mixer_menu_stop_cb;
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING_LOOPED);
}

void audio_driver_mixer_play_menu_sound(unsigned i)
{
   audio_driver_st.mixer_streams[i].stop_cb = audio_mixer_menu_stop_cb;
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING);
}

void audio_driver_mixer_play_stream_looped(unsigned i)
{
   audio_driver_st.mixer_streams[i].stop_cb = audio_mixer_play_stop_cb;
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING_LOOPED);
}

void audio_driver_mixer_play_stream_sequential(unsigned i)
{
   audio_driver_st.mixer_streams[i].stop_cb = audio_mixer_play_stop_sequential_cb;
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL);
}

float audio_driver_mixer_get_stream_volume(unsigned i)
{
   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return 0.0f;

   return audio_driver_st.mixer_streams[i].volume;
}

void audio_driver_mixer_set_stream_volume(unsigned i, float vol)
{
   audio_mixer_voice_t *voice             = NULL;

   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;

   audio_driver_st.mixer_streams[i].volume = vol;

   voice                                  =
      audio_driver_st.mixer_streams[i].voice;

   if (voice)
      audio_mixer_voice_set_volume(voice, DB_TO_GAIN(vol));
}

void audio_driver_mixer_stop_stream(unsigned i)
{
   bool set_state                         = false;

   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;

   switch (audio_driver_st.mixer_streams[i].state)
   {
      case AUDIO_STREAM_STATE_PLAYING:
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
         set_state = true;
         break;
      case AUDIO_STREAM_STATE_STOPPED:
      case AUDIO_STREAM_STATE_NONE:
         break;
   }

   if (set_state)
   {
      audio_mixer_voice_t *voice     = audio_driver_st.mixer_streams[i].voice;

      if (voice)
         audio_mixer_stop(voice);
      audio_driver_st.mixer_streams[i].state   = AUDIO_STREAM_STATE_STOPPED;
      audio_driver_st.mixer_streams[i].volume  = 1.0f;
   }
}

void audio_driver_mixer_remove_stream(unsigned i)
{
   bool destroy                = false;

   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;

   switch (audio_driver_st.mixer_streams[i].state)
   {
      case AUDIO_STREAM_STATE_PLAYING:
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
         audio_driver_mixer_stop_stream(i);
         destroy = true;
         break;
      case AUDIO_STREAM_STATE_STOPPED:
         destroy = true;
         break;
      case AUDIO_STREAM_STATE_NONE:
         break;
   }

   if (destroy)
   {
      audio_mixer_sound_t *handle = audio_driver_st.mixer_streams[i].handle;
      if (handle)
         audio_mixer_destroy(handle);

      if (!string_is_empty(audio_driver_st.mixer_streams[i].name))
         free(audio_driver_st.mixer_streams[i].name);

      audio_driver_st.mixer_streams[i].state   = AUDIO_STREAM_STATE_NONE;
      audio_driver_st.mixer_streams[i].stop_cb = NULL;
      audio_driver_st.mixer_streams[i].volume  = 0.0f;
      audio_driver_st.mixer_streams[i].handle  = NULL;
      audio_driver_st.mixer_streams[i].voice   = NULL;
      audio_driver_st.mixer_streams[i].name    = NULL;
   }
}

bool audio_driver_mixer_toggle_mute(void)
{
   audio_driver_st.mixer_mute_enable  =
      !audio_driver_st.mixer_mute_enable;
   return true;
}
#endif

bool audio_driver_enable_callback(void)
{
   if (!audio_driver_st.callback.callback)
      return false;
   if (audio_driver_st.callback.set_state)
      audio_driver_st.callback.set_state(true);
   return true;
}

bool audio_driver_disable_callback(void)
{
   if (!audio_driver_st.callback.callback)
      return false;

   if (audio_driver_st.callback.set_state)
      audio_driver_st.callback.set_state(false);
   return true;
}

bool audio_driver_callback(void)
{
   settings_t *settings        = config_get_ptr();
   bool runloop_paused         = runloop_state_get_ptr()->paused;
#ifdef HAVE_MENU
#ifdef HAVE_NETWORKING
   bool core_paused            = runloop_paused ||
      (settings->bools.menu_pause_libretro && menu_state_get_ptr()->alive &&
         netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL));
#else
   bool core_paused            = runloop_paused ||
     (settings->bools.menu_pause_libretro && menu_state_get_ptr()->alive);
#endif
#else
   bool core_paused            = runloop_paused;
#endif

   if (!audio_driver_st.callback.callback)
      return false;

   if (!core_paused && audio_driver_st.callback.callback)
      audio_driver_st.callback.callback();

   return true;
}

bool audio_driver_has_callback(void)
{
   return audio_driver_st.callback.callback != NULL;
}

static INLINE bool audio_driver_alive(void)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   if (     audio_st->current_audio
         && audio_st->current_audio->alive
         && audio_st->context_audio_data)
      return audio_st->current_audio->alive(audio_st->context_audio_data);
   return false;
}

bool audio_driver_start(bool is_shutdown)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   if (
            !audio_st->current_audio 
         || !audio_st->current_audio->start
         || !audio_st->context_audio_data)
      goto error;
   if (!audio_st->current_audio->start(
            audio_st->context_audio_data, is_shutdown))
      goto error;

   return true;

error:
   RARCH_ERR("%s\n",
         msg_hash_to_str(MSG_FAILED_TO_START_AUDIO_DRIVER));
   audio_driver_st.active = false;
   return false;
}

bool audio_driver_stop(void)
{
   if (     !audio_driver_st.current_audio
         || !audio_driver_st.current_audio->stop
         || !audio_driver_st.context_audio_data
         || !audio_driver_alive()
      )
      return false;
   return audio_driver_st.current_audio->stop(
         audio_driver_st.context_audio_data);
}

#ifdef HAVE_REWIND
void audio_driver_frame_is_reverse(void)
{
   audio_driver_state_t *audio_st  = &audio_driver_st;
   recording_state_t *recording_st = recording_state_get_ptr();
   runloop_state_t *runloop_st     = runloop_state_get_ptr();

   /* We just rewound. Flush rewind audio buffer. */
   if (  recording_st->data   &&
         recording_st->driver &&
         recording_st->driver->push_audio)
   {
      struct record_audio_data ffemu_data;

      ffemu_data.data              = audio_st->rewind_buf +
         audio_st->rewind_ptr;
      ffemu_data.frames            = (audio_st->rewind_size -
            audio_st->rewind_ptr) / 2;

      recording_st->driver->push_audio(
            recording_st->data,
            &ffemu_data);
   }

   if (!(
             runloop_st->paused
         || !audio_st->active
         || !audio_st->output_samples_buf))
      if (!audio_st->suspended)
      {
         settings_t *settings = config_get_ptr();
         audio_driver_flush(audio_st,
               settings->floats.slowmotion_ratio,
               settings->bools.audio_fastforward_mute,
               audio_st->rewind_buf  +
               audio_st->rewind_ptr,
               audio_st->rewind_size -
               audio_st->rewind_ptr,
               runloop_st->slowmotion,
               runloop_st->fastmotion);
      }
}
#endif

void audio_set_float(enum audio_action action, float val)
{
   switch (action)
   {
      case AUDIO_ACTION_VOLUME_GAIN:
         audio_driver_st.volume_gain        = DB_TO_GAIN(val);
         break;
      case AUDIO_ACTION_MIXER_VOLUME_GAIN:
#ifdef HAVE_AUDIOMIXER
         audio_driver_st.mixer_volume_gain  = DB_TO_GAIN(val);
#endif
         break;
      case AUDIO_ACTION_RATE_CONTROL_DELTA:
         audio_driver_st.rate_control_delta = val;
         break;
      case AUDIO_ACTION_NONE:
      default:
         break;
   }
}

float *audio_get_float_ptr(enum audio_action action)
{
   switch (action)
   {
      case AUDIO_ACTION_RATE_CONTROL_DELTA:
         return &audio_driver_st.rate_control_delta;
      case AUDIO_ACTION_NONE:
      default:
         break;
   }

   return NULL;
}

bool *audio_get_bool_ptr(enum audio_action action)
{
   switch (action)
   {
      case AUDIO_ACTION_MIXER_MUTE_ENABLE:
#ifdef HAVE_AUDIOMIXER
         return &audio_driver_st.mixer_mute_enable;
#else
         break;
#endif
      case AUDIO_ACTION_MUTE_ENABLE:
         return &audio_driver_st.mute_enable;
      case AUDIO_ACTION_NONE:
      default:
         break;
   }

   return NULL;
}

bool audio_compute_buffer_statistics(audio_statistics_t *stats)
{
   unsigned i, low_water_size, high_water_size, avg, stddev;
   uint64_t accum                 = 0;
   uint64_t accum_var             = 0;
   unsigned low_water_count       = 0;
   unsigned high_water_count      = 0;
   audio_driver_state_t *audio_st = &audio_driver_st;
   unsigned samples               = MIN(
         (unsigned)audio_st->free_samples_count,
         AUDIO_BUFFER_FREE_SAMPLES_COUNT);

   if (samples < 3)
      return false;

   stats->samples                = (unsigned)
      audio_st->free_samples_count;

#ifdef WARPUP
   /* uint64 to double not implemented, fair chance
    * signed int64 to double doesn't exist either */
   /* https://forums.libretro.com/t/unsupported-platform-help/13903/ */
   (void)stddev;
#elif defined(_MSC_VER) && _MSC_VER <= 1200
   /* FIXME: error C2520: conversion from unsigned __int64
    * to double not implemented, use signed __int64 */
   (void)stddev;
#else
   for (i = 1; i < samples; i++)
      accum += audio_st->free_samples_buf[i];

   avg = (unsigned)accum / (samples - 1);

   for (i = 1; i < samples; i++)
   {
      int diff     = avg - audio_st->free_samples_buf[i];
      accum_var   += diff * diff;
   }

   stddev                                = (unsigned)
      sqrt((double)accum_var / (samples - 2));

   stats->average_buffer_saturation      = (1.0f - (float)avg
         / audio_st->buffer_size) * 100.0;
   stats->std_deviation_percentage       = ((float)stddev
         / audio_st->buffer_size)  * 100.0;
#endif

   low_water_size  = (unsigned)(audio_st->buffer_size * 3 / 4);
   high_water_size = (unsigned)(audio_st->buffer_size     / 4);

   for (i = 1; i < samples; i++)
   {
      if (audio_st->free_samples_buf[i] >= low_water_size)
         low_water_count++;
      else if (audio_st->free_samples_buf[i] <= high_water_size)
         high_water_count++;
   }

   stats->close_to_underrun      = (100.0f * low_water_count)  / (samples - 1);
   stats->close_to_blocking      = (100.0f * high_water_count) / (samples - 1);

   return true;
}

#ifdef HAVE_MENU
void audio_driver_menu_sample(void)
{
   static int16_t samples_buf[1024]       = {0};
   settings_t *settings                   = config_get_ptr();
   video_driver_state_t *video_st         = video_state_get_ptr();
   runloop_state_t *runloop_st            = runloop_state_get_ptr();
   recording_state_t *recording_st        = recording_state_get_ptr();
   struct retro_system_av_info *av_info   = &video_st->av_info;
   const struct retro_system_timing *info =
      (const struct retro_system_timing*)&av_info->timing;
   unsigned sample_count                  = (info->sample_rate / info->fps) * 2;
   audio_driver_state_t *audio_st         = &audio_driver_st;
   bool check_flush                       = !(
             runloop_st->paused              
         || !audio_st->active     
         || !audio_st->output_samples_buf);
   if (audio_st->suspended)
      check_flush                         = false;

   while (sample_count > 1024)
   {
      if (  recording_st->data   &&
            recording_st->driver &&
            recording_st->driver->push_audio)
      {
         struct record_audio_data ffemu_data;

         ffemu_data.data                    = samples_buf;
         ffemu_data.frames                  = 1024 / 2;

         recording_st->driver->push_audio(
               recording_st->data, &ffemu_data);
      }
      if (check_flush)
         audio_driver_flush(audio_st,
               settings->floats.slowmotion_ratio,
               settings->bools.audio_fastforward_mute,
               samples_buf,
               1024,
               runloop_st->slowmotion,
               runloop_st->fastmotion);
      sample_count -= 1024;
   }
   if (  recording_st->data   &&
         recording_st->driver &&
         recording_st->driver->push_audio)
   {
      struct record_audio_data ffemu_data;

      ffemu_data.data                    = samples_buf;
      ffemu_data.frames                  = sample_count / 2;

      recording_st->driver->push_audio(
            recording_st->data, &ffemu_data);
   }
   if (check_flush)
      audio_driver_flush(audio_st,
            settings->floats.slowmotion_ratio,
            settings->bools.audio_fastforward_mute,
            samples_buf,
            sample_count,
            runloop_st->slowmotion,
            runloop_st->fastmotion);
}
#endif
