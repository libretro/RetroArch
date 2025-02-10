/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2023 Jesse Talavera-Greenberg
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

#include <math.h>
#include <memalign.h>
#include <audio/conversion/s16_to_float.h>
#include <audio/conversion/float_to_s16.h>
#include <retro_assert.h>
#include <string/stdstring.h>
#include <lists/string_list.h>
#include <audio/conversion/dual_mono.h>

#include "microphone_driver.h"
#include "audio_defines.h"

#include "../configuration.h"
#include "../driver.h"
#include "../list_special.h"
#include "../runloop.h"
#include "../verbosity.h"

static microphone_driver_state_t mic_driver_st;

microphone_driver_t microphone_null = {
      NULL,
      NULL,
      NULL,
      NULL,
      "null",
      NULL,
      NULL,
      NULL,
      NULL
};

microphone_driver_t *microphone_drivers[] = {
#ifdef HAVE_ALSA
      &microphone_alsa,
#if !defined(__QNX__) && !defined(MIYOO) && defined(HAVE_THREADS)
   &microphone_alsathread,
#endif
#endif
#ifdef HAVE_WASAPI
      &microphone_wasapi,
#endif
#ifdef HAVE_SDL2
      &microphone_sdl, /* Microphones are not supported in SDL 1 */
#endif
#ifdef HAVE_PIPEWIRE
      &microphone_pipewire,
#endif
      &microphone_null,
      NULL,
};

microphone_driver_state_t *microphone_state_get_ptr(void)
{
   return &mic_driver_st;
}

#define mic_driver_get_sample_size(microphone) \
   (((microphone)->flags & MICROPHONE_FLAG_USE_FLOAT) ? sizeof(float) : sizeof(int16_t))

static bool mic_driver_open_mic_internal(retro_microphone_t* microphone);
bool microphone_driver_start(void)
{
   microphone_driver_state_t *mic_st = &mic_driver_st;
   retro_microphone_t    *microphone = &mic_st->microphone;

   if (microphone->flags & MICROPHONE_FLAG_ACTIVE)
   { /* If there's an opened microphone that the core turned on... */

      if (microphone->flags & MICROPHONE_FLAG_PENDING)
      { /* If this microphone was requested before the driver was ready...*/
         retro_assert(microphone->microphone_context == NULL);
         /* The microphone context shouldn't have been created yet */

         /* Now that the driver and driver context are ready, let's initialize the mic */
         if (mic_driver_open_mic_internal(microphone))
         {
            /* open_mic_internal will start the microphone if it's enabled */
            RARCH_DBG("[Microphone]: Initialized a previously-pending microphone.\n");
         }
         else
         {
            RARCH_ERR("[Microphone]: Failed to initialize a previously pending microphone; microphone will not be used.\n");

            microphone_driver_close_mic(microphone);
            /* Not returning false because a mic failure shouldn't take down the driver;
             * what if the player just unplugged their mic? */
         }
      }
      else
      { /* The mic was already created, so let's just unpause it */
         microphone_driver_set_mic_state(microphone, true);

         RARCH_DBG("[Microphone]: Started a microphone that was enabled when the driver was last stopped.\n");
      }
   }

   return true;
}

bool microphone_driver_stop(void)
{
   microphone_driver_state_t *mic_st = &mic_driver_st;
   retro_microphone_t    *microphone = &mic_st->microphone;
   bool result                       = true;

   if ((microphone->flags & MICROPHONE_FLAG_ACTIVE)
         && (microphone->flags & MICROPHONE_FLAG_ENABLED)
         && !(microphone->flags & MICROPHONE_FLAG_PENDING))
   { /* If there's an opened microphone that the core turned on and received... */

      result = mic_st->driver->stop_mic(mic_st->driver_context, microphone->microphone_context);
   }
   /* If the mic is pending, then we don't need to do anything. */

   return result;
}

/**
 * config_get_microphone_driver_options:
 *
 * Get an enumerated list of all microphone driver names, separated by '|'.
 *
 * Returns: string listing of all microphone driver names, separated by '|'.
 **/
const char *config_get_microphone_driver_options(void)
{
   return char_list_new_special(STRING_LIST_MICROPHONE_DRIVERS, NULL);
}

bool microphone_driver_find_driver(void *settings_data, const char *prefix,
      bool verbosity_enabled)
{
   settings_t *settings = (settings_t*)settings_data;
   int i                 = (int)driver_find_index(
         "microphone_driver",
         settings->arrays.microphone_driver);

   if (i >= 0)
      mic_driver_st.driver = (const microphone_driver_t *)
            microphone_drivers[i];
   else
   {
      const microphone_driver_t *tmp = NULL;
      if (verbosity_enabled)
      {
         unsigned d;
         RARCH_ERR("Couldn't find any %s named \"%s\".\n", prefix,
                   settings->arrays.microphone_driver);

         RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
         for (d = 0; microphone_drivers[d]; d++)
         {
            if (microphone_drivers[d])
               RARCH_LOG_OUTPUT("\t%s\n", microphone_drivers[d]->ident);
         }
         RARCH_WARN("Going to default to first %s..\n", prefix);
      }

      tmp = (const microphone_driver_t *)microphone_drivers[0];

      if (!tmp)
         return false;
      mic_driver_st.driver = tmp;
   }

   return true;
}

static void mic_driver_microphone_handle_init(retro_microphone_t *microphone,
      const retro_microphone_params_t *params)
{
   if (microphone)
   {
      const settings_t *settings        = config_get_ptr();
      unsigned microphone_sample_rate   = settings->uints.microphone_sample_rate;
      microphone->microphone_context    = NULL;
      microphone->flags                 = MICROPHONE_FLAG_ACTIVE;
      microphone->sample_buffer         = NULL;
      microphone->sample_buffer_length  = 0;

      microphone->requested_params.rate = params ? params->rate : microphone_sample_rate;
      microphone->actual_params.rate    = 0;
      /* We don't set the actual parameters until we actually open the mic.
       * (Remember, the core can request one before the driver is ready.) */
      microphone->effective_params.rate = params ? params->rate : microphone_sample_rate;
      /* We set the effective parameters because
       * the frontend has to do what it can
       * to give the core what it asks for. */
   }
}

static void mic_driver_microphone_handle_free(retro_microphone_t *microphone, bool is_reset)
{
   microphone_driver_state_t *mic_st     = &mic_driver_st;
   const microphone_driver_t *mic_driver = mic_st->driver;
   void *driver_context                  = mic_st->driver_context;

   if (!microphone)
      return;

   if (!driver_context)
      RARCH_WARN("[Microphone]: Attempted to free a microphone without an active driver context.\n");

   if (microphone->microphone_context)
   {
      mic_driver->close_mic(driver_context, microphone->microphone_context);
      microphone->microphone_context = NULL;
   }

   if (microphone->sample_buffer)
   {
      memalign_free(microphone->sample_buffer);
      microphone->sample_buffer = NULL;
      microphone->sample_buffer_length = 0;
   }

   if (microphone->outgoing_samples)
   {
      fifo_free(microphone->outgoing_samples);
      microphone->outgoing_samples = NULL;
   }

   if (microphone->resampler && microphone->resampler->free && microphone->resampler_data)
      microphone->resampler->free(microphone->resampler_data);

   microphone->resampler      = NULL;
   microphone->resampler_data = NULL;

   /* If the mic driver is being reset and the microphone was already valid... */
   if ((microphone->flags & MICROPHONE_FLAG_ACTIVE) && is_reset)
      microphone->flags |= MICROPHONE_FLAG_PENDING;
      /* ...then we need to keep the handle itself valid
       * so it can be reinitialized.
       * Otherwise the core will lose mic input. */
   else
      memset(microphone, 0, sizeof(*microphone));
   /* Do NOT free the microphone handle itself! It's allocated statically! */
}

bool microphone_driver_init_internal(void *settings_data)
{
   settings_t *settings   = (settings_t*)settings_data;
   microphone_driver_state_t *mic_st = &mic_driver_st;
   bool verbosity_enabled = verbosity_is_enabled();
   size_t max_frames   = AUDIO_CHUNK_SIZE_NONBLOCKING * AUDIO_MAX_RATIO;

   /* If the user has mic support turned off... */
   if (!settings->bools.microphone_enable)
   {
      mic_st->flags &= ~MICROPHONE_DRIVER_FLAG_ACTIVE;
      return false;
   }

   convert_s16_to_float_init_simd();
   convert_float_to_s16_init_simd();

   if (!(microphone_driver_find_driver(settings,
               "microphone driver", verbosity_enabled)))
   {
      RARCH_ERR("[Microphone]: Failed to initialize microphone driver. Will continue without mic input.\n");
      goto error;
   }

   mic_st->input_frames_length = max_frames * sizeof(float);
   mic_st->input_frames = (float*)memalign_alloc(64, mic_st->input_frames_length);
   if (!mic_st->input_frames)
      goto error;

   mic_st->converted_input_frames_length = max_frames * sizeof(float);
   mic_st->converted_input_frames = (float*)memalign_alloc(64, mic_st->converted_input_frames_length);
   if (!mic_st->converted_input_frames)
      goto error;

   /* Need room for dual-mono frames */
   mic_st->dual_mono_frames_length = max_frames * sizeof(float) * 2;
   mic_st->dual_mono_frames = (float*)memalign_alloc(64, mic_st->dual_mono_frames_length);
   if (!mic_st->dual_mono_frames)
      goto error;

   mic_st->resampled_frames_length = max_frames * sizeof(float) * 2;
   mic_st->resampled_frames = (float*) memalign_alloc(64, mic_st->resampled_frames_length);
   if (!mic_st->resampled_frames)
      goto error;

   mic_st->resampled_mono_frames_length = max_frames * sizeof(float);
   mic_st->resampled_mono_frames = (float*) memalign_alloc(64, mic_st->resampled_mono_frames_length);
   if (!mic_st->resampled_mono_frames)
      goto error;

   mic_st->final_frames_length = max_frames * sizeof(int16_t);
   mic_st->final_frames = (int16_t*) memalign_alloc(64, mic_st->final_frames_length);
   if (!mic_st->final_frames)
      goto error;

   if (!mic_st->driver || !mic_st->driver->init)
      goto error;

   if (!(mic_st->driver_context = mic_st->driver->init()))
      goto error;

   if (!string_is_empty(settings->arrays.microphone_resampler))
      strlcpy(mic_st->resampler_ident,
            settings->arrays.microphone_resampler,
            sizeof(mic_st->resampler_ident));
   else
      mic_st->resampler_ident[0] = '\0';

   mic_st->resampler_quality     = (enum resampler_quality)settings->uints.microphone_resampler_quality;

   RARCH_LOG("[Microphone]: Initialized microphone driver.\n");

   /* The mic driver was initialized, now we're ready to open mics */
   mic_st->flags |= MICROPHONE_DRIVER_FLAG_ACTIVE;

   if (!microphone_driver_start())
      goto error;

   return true;

error:
   RARCH_ERR("[Microphone]: Failed to start microphone driver. Will continue without audio input.\n");
   mic_st->flags &= ~MICROPHONE_DRIVER_FLAG_ACTIVE;
   return microphone_driver_deinit(false);
}

/**
 *
 * @param microphone Handle to the microphone to init with a context
 */
static bool mic_driver_open_mic_internal(retro_microphone_t* microphone)
{
   microphone_driver_state_t *mic_st     = &mic_driver_st;
   settings_t *settings                  = config_get_ptr();
   const microphone_driver_t *mic_driver = mic_st->driver;
   void *driver_context                  = mic_st->driver_context;
   unsigned runloop_audio_latency        = runloop_state_get_ptr()->audio_latency;
   unsigned setting_audio_latency        = settings->uints.microphone_latency;
   unsigned audio_latency                = MAX(runloop_audio_latency, setting_audio_latency);
   size_t max_samples                    = AUDIO_CHUNK_SIZE_NONBLOCKING * 1 * AUDIO_MAX_RATIO;

   if (!microphone || !mic_driver || !(mic_st->flags & MICROPHONE_DRIVER_FLAG_ACTIVE))
      return false;

   microphone->sample_buffer_length = max_samples * sizeof(int16_t);
   microphone->sample_buffer        =
         (int16_t*)memalign_alloc(64, microphone->sample_buffer_length);

   if (!microphone->sample_buffer)
      goto error;

   microphone->outgoing_samples = fifo_new(max_samples * sizeof(int16_t));
   if (!microphone->outgoing_samples)
      goto error;

   microphone->microphone_context = mic_driver->open_mic(driver_context,
      *settings->arrays.microphone_device ? settings->arrays.microphone_device : NULL,
      microphone->requested_params.rate,
      audio_latency,
      &microphone->actual_params.rate);

   if (!microphone->microphone_context)
      goto error;

   microphone_driver_set_mic_state(microphone, microphone->flags & MICROPHONE_FLAG_ENABLED);

   RARCH_LOG("[Microphone]: Requested microphone sample rate of %uHz, got %uHz.\n",
             microphone->requested_params.rate,
             microphone->actual_params.rate
   );

   if (     mic_driver->mic_use_float
         && mic_driver->mic_use_float(mic_st->driver_context, microphone->microphone_context))
      microphone->flags      |= MICROPHONE_FLAG_USE_FLOAT;

   microphone->original_ratio = (double)microphone->effective_params.rate / microphone->actual_params.rate;

   if (!retro_resampler_realloc(
         &microphone->resampler_data,
         &microphone->resampler,
         mic_st->resampler_ident,
         mic_st->resampler_quality,
         microphone->original_ratio))
   {
      RARCH_ERR("[Microphone]: Failed to initialize resampler \"%s\".\n", mic_st->resampler_ident);
      goto error;
   }

   microphone->flags &= ~MICROPHONE_FLAG_PENDING;
   RARCH_LOG("[Microphone]: Initialized microphone.\n");
   return true;
error:
   mic_driver_microphone_handle_free(microphone, false);
   RARCH_ERR("[Microphone]: Driver attempted to initialize the microphone but failed.\n");
   return false;
}

static void microphone_driver_close_mic_internal(retro_microphone_t *microphone, bool is_reset)
{
   microphone_driver_state_t *mic_st     = &mic_driver_st;
   const microphone_driver_t *mic_driver = mic_st->driver;
   void *driver_context                  = mic_st->driver_context;

   if (     microphone
         && driver_context
         && mic_driver
         && mic_driver->close_mic)
      mic_driver_microphone_handle_free(microphone, is_reset);
}

void microphone_driver_close_mic(retro_microphone_t *microphone)
{
   mic_driver_microphone_handle_free(microphone, false);
}

bool microphone_driver_set_mic_state(retro_microphone_t *microphone, bool state)
{
   microphone_driver_state_t *mic_st     = &mic_driver_st;
   const microphone_driver_t *mic_driver = mic_st->driver;
   void *driver_context                  = mic_st->driver_context;

   if (!microphone
         || !(microphone->flags & MICROPHONE_FLAG_ACTIVE)
         || !mic_driver
         || !mic_driver->start_mic
         || !mic_driver->stop_mic)
      return false;
   /* If the provided microphone was null or invalid, or the driver is incomplete, stop. */

   /* If the driver is initialized... */
   if (driver_context && microphone->microphone_context)
   {
      bool success;

      /* If we want to enable this mic... */
      if (state)
      {
         success = mic_driver->start_mic(driver_context, microphone->microphone_context);
         /* Enable the mic. (Enabling an active mic is a successful noop.) */

         if (success)
         {
            microphone->flags |= MICROPHONE_FLAG_ENABLED;
            RARCH_LOG("[Microphone]: Enabled microphone.\n");
         }
         else
         {
            RARCH_ERR("[Microphone]: Failed to enable microphone.\n");
         }
      }
      else
      { /* If we want to pause this mic... */
         success = mic_driver->stop_mic(driver_context, microphone->microphone_context);

         /* Disable the mic. (If the mic is already stopped, disabling it should still be successful.) */
         if (success)
         {
            microphone->flags &= ~MICROPHONE_FLAG_ENABLED;
            RARCH_LOG("[Microphone]: Disabled microphone.\n");
         }
         else
         {
            RARCH_ERR("[Microphone]: Failed to disable microphone.\n");
         }
      }

      return success;
   }
   else
   { /* The driver's not ready yet, so we'll make a note
      * of what the mic's state should be */
      if (state)
         microphone->flags |= MICROPHONE_FLAG_ENABLED;
      else
         microphone->flags &= ~MICROPHONE_FLAG_ENABLED;

      RARCH_DBG("[Microphone]: Set pending state to %s.\n",
                state ? "enabled" : "disabled");
      return true;
      /* This isn't an error */
   }
}

bool microphone_driver_get_mic_state(const retro_microphone_t *microphone)
{
   if (!microphone || !(microphone->flags & MICROPHONE_FLAG_ACTIVE))
      return false;
   return microphone->flags & MICROPHONE_FLAG_ENABLED;
}

/**
 * Pull queued microphone samples from the driver
 * and copy them to the provided buffer(s).
 *
 * Note that microphone samples are provided in mono,
 * so a "sample" and a "frame" are equivalent here.
 *
 * @param mic_st The overall state of the audio driver.
 * @param[out] frames The buffer in which the core will receive microphone samples.
 * @param num_frames The size of \c frames, in samples.
 */
static size_t microphone_driver_flush(
      microphone_driver_state_t *mic_st,
      retro_microphone_t *microphone,
      size_t num_frames)
{
   struct resampler_data resampler_data;
   unsigned sample_size = mic_driver_get_sample_size(microphone);
   size_t bytes_to_read = MIN(mic_st->input_frames_length, num_frames * sample_size);
   size_t frames_to_enqueue;
   int bytes_read       = mic_st->driver->read(
         mic_st->driver_context,
         microphone->microphone_context,
         mic_st->input_frames,
         bytes_to_read);
   /* First, get the most recent mic data */

   if (bytes_read <= 0)
      return 0;

   resampler_data.input_frames = bytes_read / sample_size;
   /* This is in frames, not samples or bytes;
    * we're up-channeling the audio to stereo,
    * so this number still applies. */

   resampler_data.output_frames = 0;
   /* The resampler sets the value of output_frames */

   resampler_data.data_in  = mic_st->dual_mono_frames;
   resampler_data.data_out = mic_st->resampled_frames;
   /* The buffers that will be used for the resampler's input and output */

   resampler_data.ratio    = (double)microphone->effective_params.rate / (double)microphone->actual_params.rate;

   if (fabs(resampler_data.ratio - 1.0f) < 1e-8)
   { /* If the mic's native rate is practically the same as the requested one... */

      /* ...then skip the resampler, since it'll produce (more or less) identical results. */
      frames_to_enqueue = MIN(FIFO_WRITE_AVAIL(microphone->outgoing_samples), resampler_data.input_frames);

      /* If this mic provides floating-point samples... */
      if (microphone->flags & MICROPHONE_FLAG_USE_FLOAT)
      {
         convert_float_to_s16(mic_st->final_frames, (const float*)mic_st->input_frames, resampler_data.input_frames);
         fifo_write(microphone->outgoing_samples, mic_st->final_frames, frames_to_enqueue * sizeof(int16_t));
      }
      else
         fifo_write(microphone->outgoing_samples, mic_st->input_frames, frames_to_enqueue * sizeof(int16_t));

      return resampler_data.input_frames;
   }
   /* Couldn't take the fast path, so let's resample the mic input */

   /* First we need to format the input for the resampler. */
   /* If this mic provides floating-point samples... */
   if (microphone->flags & MICROPHONE_FLAG_USE_FLOAT)
      /* Samples are already in floating-point, so we just need to up-channel them. */
      convert_to_dual_mono_float(mic_st->dual_mono_frames,
            (const float*)mic_st->input_frames, resampler_data.input_frames);
   else
   {
      /* Samples are 16-bit, so we need to convert them first. */
      convert_s16_to_float(mic_st->converted_input_frames, (const int16_t*)mic_st->input_frames, resampler_data.input_frames, 1.0f);
      convert_to_dual_mono_float(mic_st->dual_mono_frames, mic_st->converted_input_frames, resampler_data.input_frames);
   }

   /* Now we resample the mic data. */
   microphone->resampler->process(microphone->resampler_data, &resampler_data);

   /* Next, we convert the resampled data back to mono... */
   convert_to_mono_float_left(mic_st->resampled_mono_frames, mic_st->resampled_frames, resampler_data.output_frames);
   /* Why the left channel? No particular reason.
    * Left and right channels are the same in this case anyway. */

   /* Finally, we convert the audio back to 16-bit ints, as the mic interface requires. */
   convert_float_to_s16(mic_st->final_frames, mic_st->resampled_mono_frames, resampler_data.output_frames);

   frames_to_enqueue = MIN(FIFO_WRITE_AVAIL(microphone->outgoing_samples), resampler_data.output_frames);
   fifo_write(microphone->outgoing_samples, mic_st->final_frames, frames_to_enqueue * sizeof(int16_t));
   return resampler_data.output_frames;
}

int microphone_driver_read(retro_microphone_t *microphone, int16_t* frames, size_t num_frames)
{
   uint32_t runloop_flags            = runloop_get_flags();
   size_t frames_remaining           = num_frames;
   microphone_driver_state_t *mic_st = &mic_driver_st;
   const microphone_driver_t *driver = mic_st->driver;
   bool core_paused                  = (runloop_flags & RUNLOOP_FLAG_PAUSED)           ? true : false;
   bool is_fastforward               = (runloop_flags & RUNLOOP_FLAG_FASTMOTION)       ? true : false;
   bool is_slowmo                    = (runloop_flags & RUNLOOP_FLAG_SLOWMOTION)       ? true : false;
   bool is_rewind                    = state_manager_frame_is_reversed();
   bool driver_active                = (mic_st->flags & MICROPHONE_DRIVER_FLAG_ACTIVE) ? true : false;

   /* If the provided arguments aren't valid... */
   if (!frames || !microphone)
      return -1;

   /* If the microphone or driver aren't active... */
   if (!driver_active || !(microphone->flags & MICROPHONE_FLAG_ACTIVE))
      return -1;

   /* If the driver is invalid or doesn't have the functions it needs... */
   if (!driver || !driver->read || !driver->mic_alive)
      return -1;

   /* If the core didn't actually ask for any frames... */
   if (num_frames == 0)
      return 0;

   if (   (microphone->flags & MICROPHONE_FLAG_PENDING)
      ||  (microphone->flags & MICROPHONE_FLAG_SUSPENDED)
      || !(microphone->flags & MICROPHONE_FLAG_ENABLED)
      || is_fastforward
      || is_slowmo
      || is_rewind
      )
   { /* If the microphone is pending, suspended, or disabled...
        ...or if the core is in fast-forward, slow-mo, or rewind...*/
      memset(frames, 0, num_frames * sizeof(*frames));
      return (int)num_frames;
      /* ...then copy silence to the provided buffer. Not an error if the mic is pending,
       * because the user might have requested a microphone
       * before the driver could provide it. */
   }

   /* Why mute the mic when the core isn't running at standard speed?
    * Because I couldn't think of anything useful for the mic to do.
    * If you can, send a PR! */

   /* If the driver or microphone's state haven't been allocated... */
   if (!mic_st->driver_context || !microphone->microphone_context)
      return -1;

   /* If the mic isn't active like it should be at this point... */
   if (!driver->mic_alive(mic_st->driver_context, microphone->microphone_context))
   {
      RARCH_ERR("[Microphone]: Mic frontend has the mic enabled, but the backend has it disabled.\n");
      return -1;
   }

   /* If the core asked for more frames than we can fit... */
   if (num_frames > microphone->outgoing_samples->size)
      return -1;

   retro_assert(mic_st->input_frames != NULL);

   while (FIFO_READ_AVAIL(microphone->outgoing_samples) < num_frames * sizeof(int16_t))
   { /* Until we can give the core the frames it asked for... */
      size_t frames_to_read = MIN(AUDIO_CHUNK_SIZE_NONBLOCKING, frames_remaining);
      size_t frames_read    = 0;

      /* If the game is running and the mic driver is active... */
      if (!core_paused)
         frames_read = microphone_driver_flush(mic_st, microphone, frames_to_read);

      /* Otherwise, advance the counters. We're not gonna get new data,
       * but we still need to finish this loop */
      frames_remaining -= frames_read;
   } /* If the queue already has enough samples to give, the loop will be skipped */

   fifo_read(microphone->outgoing_samples, frames, num_frames * sizeof(int16_t));
   return (int)num_frames;
}

bool microphone_driver_get_effective_params(const retro_microphone_t *microphone, retro_microphone_params_t *params)
{
   /* If the arguments are null... */
   if (!microphone || !params)
      return false;
   /* If this isn't an opened microphone... */
   if (!(microphone->flags & MICROPHONE_FLAG_ACTIVE))
      return false;
   *params = microphone->effective_params;
   return true;
}

/* NOTE: The core may request a microphone before the driver is ready.
 * A pending handle will be provided in that case, and the frontend will
 * initialize the microphone when the time is right;
 * do not call this function twice on the same mic. */
retro_microphone_t *microphone_driver_open_mic(const retro_microphone_params_t *params)
{
   microphone_driver_state_t *mic_st     = &mic_driver_st;
   const settings_t *settings            = config_get_ptr();
   const microphone_driver_t *mic_driver = mic_st->driver;
   void *driver_context                  = mic_st->driver_context;

   if (!settings)
      return NULL;

   /* Not checking mic_st->flags because they might not be set yet;
    * don't forget, the core can ask for a mic
    * before the audio driver is ready to create one. */
   if (!settings->bools.microphone_enable)
   {
      RARCH_DBG("[Microphone]: Refused to open microphone because it's disabled in the settings.\n");
      return NULL;
   }

   if (mic_driver == &microphone_null)
   {
      RARCH_WARN("[Microphone]: Cannot open microphone, null driver is configured.\n");
      return NULL;
   }

   if (        !mic_driver
            && (string_is_equal(settings->arrays.microphone_driver, "null")
            || string_is_empty(settings->arrays.microphone_driver)))
   { /* If the mic driver hasn't been initialized, but it's not going to be... */
      RARCH_ERR("[Microphone]: Cannot open microphone as the driver won't be initialized.\n");
      return NULL;
   }

   /* If the core has requested a second microphone... */
   if (mic_st->microphone.flags & MICROPHONE_FLAG_ACTIVE)
   {
      RARCH_ERR("[Microphone]: Failed to open a second microphone, frontend only supports one at a time right now.\n");
      if (mic_st->microphone.flags & MICROPHONE_FLAG_PENDING)
         /* If that mic is pending... */
         RARCH_ERR("[Microphone]: A microphone is pending initialization.\n");
      else
         /* That mic is initialized */
         RARCH_ERR("[Microphone]: An initialized microphone exists.\n");

      return NULL;
   }

   /* Cores might ask for a microphone before the audio driver is ready to provide them;
    * if that happens, we have to initialize the microphones later.
    * But the user still wants a handle, so we'll give them one.
    */
   mic_driver_microphone_handle_init(&mic_st->microphone, params);

   /* If driver_context is NULL, the handle won't have
    * a valid microphone context (but we'll create one later) */
   if (driver_context)
   {
      /* If the microphone driver is ready to open a microphone... */
      if (mic_driver_open_mic_internal(&mic_st->microphone)) /* If the microphone was successfully initialized... */
         RARCH_LOG("[Microphone]: Opened the requested microphone successfully.\n");
      else
         goto error;
   }
   else
   { /* If the driver isn't ready to create a microphone... */
      mic_st->microphone.flags |= MICROPHONE_FLAG_PENDING;
      RARCH_LOG("[Microphone]: Microphone requested before driver context was ready; deferring initialization.\n");
   }

   return &mic_st->microphone;
error:
   mic_driver_microphone_handle_free(&mic_st->microphone, false);
   /* This function cleans up any resources and unsets all flags */

   return NULL;
}

static bool microphone_driver_free_devices_list(void)
{
   microphone_driver_state_t *mic_st = &mic_driver_st;
   if (
            !mic_st->driver
         || !mic_st->driver->device_list_free
         || !mic_st->driver_context
         || !mic_st->devices_list)
      return false;

   mic_st->driver->device_list_free(mic_st->driver_context, mic_st->devices_list);
   mic_st->devices_list = NULL;
   return true;
}

bool microphone_driver_deinit(bool is_reset)
{
   microphone_driver_state_t *mic_st = &mic_driver_st;
   const microphone_driver_t *driver = mic_st->driver;

   microphone_driver_free_devices_list();
   microphone_driver_close_mic_internal(&mic_st->microphone, is_reset);

   if (driver && driver->free)
   {
      if (mic_st->driver_context)
         driver->free(mic_st->driver_context);

      mic_st->driver_context = NULL;
   }

   if (mic_st->input_frames)
      memalign_free(mic_st->input_frames);
   mic_st->input_frames        = NULL;
   mic_st->input_frames_length = 0;

   if (mic_st->converted_input_frames)
      memalign_free(mic_st->converted_input_frames);
   mic_st->converted_input_frames        = NULL;
   mic_st->converted_input_frames_length = 0;

   if (mic_st->dual_mono_frames)
      memalign_free(mic_st->dual_mono_frames);
   mic_st->dual_mono_frames        = NULL;
   mic_st->dual_mono_frames_length = 0;

   if (mic_st->resampled_frames)
      memalign_free(mic_st->resampled_frames);
   mic_st->resampled_frames        = NULL;
   mic_st->resampled_frames_length = 0;

   if (mic_st->resampled_mono_frames)
      memalign_free(mic_st->resampled_mono_frames);
   mic_st->resampled_mono_frames        = NULL;
   mic_st->resampled_mono_frames_length = 0;

   if (mic_st->final_frames)
      memalign_free(mic_st->final_frames);
   mic_st->final_frames        = NULL;
   mic_st->final_frames_length = 0;

   mic_st->resampler_quality  = RESAMPLER_QUALITY_DONTCARE;
   mic_st->flags             &= ~MICROPHONE_DRIVER_FLAG_ACTIVE;
   memset(mic_st->resampler_ident, '\0', sizeof(mic_st->resampler_ident));

   return true;
}

bool microphone_driver_get_devices_list(void **data)
{
   struct string_list**ptr     = (struct string_list**)data;
   if (!ptr)
      return false;
   *ptr = mic_driver_st.devices_list;
   return true;
}
