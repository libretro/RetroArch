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
#include "microphone_driver.h"
#include "configuration.h"
#include "driver.h"
#include "verbosity.h"
#include "runloop.h"
#include "memalign.h"
#include "audio/conversion/s16_to_float.h"
#include "audio/conversion/float_to_s16.h"

static microphone_driver_state_t mic_driver_st = {0}; /* double alignment */

microphone_driver_t microphone_null = {
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      "null",
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
      &microphone_null,
      NULL,
};

microphone_driver_state_t *microphone_state_get_ptr(void)
{
   return &mic_driver_st;
}

unsigned mic_driver_get_sample_size(void)
{
   microphone_driver_state_t *mic_st = &mic_driver_st;
   return (mic_st->flags & MICROPHONE_FLAG_USE_FLOAT) ? sizeof(float) : sizeof(int16_t);
}

bool microphone_driver_find_driver(
      void *settings_data,
      const char *prefix,
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
         RARCH_ERR("Couldn't find any %s named \"%s\"\n", prefix,
                   settings->arrays.microphone_driver);

         RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
         for (d = 0; microphone_drivers[d]; d++)
         {
            if (microphone_drivers[d])
               RARCH_LOG_OUTPUT("\t%s\n", microphone_drivers[d]->ident);
         }
         RARCH_WARN("Going to default to first %s...\n", prefix);
      }

      tmp = (const microphone_driver_t *)microphone_drivers[0];

      if (!tmp)
         return false;
      mic_driver_st.driver = tmp;
   }

   return true;
}

static void mic_driver_microphone_handle_init(retro_microphone_t *microphone)
{
   if (microphone)
   {
      microphone->microphone_context      = NULL;
      microphone->pending_enabled         = false;
      microphone->active                  = true;
      microphone->sample_buffer           = NULL;
      microphone->sample_buffer_length    = 0;
      microphone->most_recent_copy_length = 0;
      microphone->error                   = false;
   }
}

static void mic_driver_microphone_handle_free(retro_microphone_t *microphone)
{
   microphone_driver_state_t *mic_st     = &mic_driver_st;
   const microphone_driver_t *mic_driver = mic_st->driver;
   void *driver_context                  = mic_st->driver_context;

   if (!microphone)
      return;

   if (!driver_context)
      RARCH_WARN("[Microphone]: Attempted to free a microphone without an active driver context\n");

   if (microphone->microphone_context)
   {
      mic_driver->close_mic(driver_context, microphone->microphone_context);
      microphone->microphone_context = NULL;
   }

   if (microphone->sample_buffer)
   {
      memalign_free(microphone->sample_buffer);
      microphone->sample_buffer = NULL;
   }

   memset(microphone, 0, sizeof(*microphone));
   /* Do NOT free the microphone handle itself! It's allocated statically! */
}

static void mic_driver_open_mic_internal(retro_microphone_t* microphone);

bool microphone_driver_init_internal(void *settings_data)
{
   float  *in_samples_buf         = NULL;
   settings_t *settings           = (settings_t*)settings_data;
   size_t max_bufsamples          = AUDIO_CHUNK_SIZE_NONBLOCKING * 2;
   bool audio_enable_microphone   = settings->bools.audio_enable_input;
   bool audio_sync                = settings->bools.audio_sync;
   bool audio_rate_control        = settings->bools.audio_rate_control;
   float slowmotion_ratio         = settings->floats.slowmotion_ratio;
   size_t insamples_max           = AUDIO_CHUNK_SIZE_NONBLOCKING * 1 * AUDIO_MAX_RATIO * slowmotion_ratio;
   int16_t *in_conv_buf           = (int16_t*)memalign_alloc(64, insamples_max * sizeof(int16_t));
   size_t audio_buf_length        = AUDIO_CHUNK_SIZE_NONBLOCKING * 2 * sizeof(float);
   bool verbosity_enabled         = verbosity_is_enabled();

   convert_s16_to_float_init_simd();
   convert_float_to_s16_init_simd();

   if (!in_conv_buf)
      goto error;

   mic_driver_st.input_samples_conv_buf         = in_conv_buf;
   mic_driver_st.input_samples_conv_buf_length  = insamples_max;

   if (!audio_enable_microphone)
   {
      mic_driver_st.flags &= ~MICROPHONE_DRIVER_FLAG_ACTIVE;
      return false;
   }

   if (!(microphone_driver_find_driver(settings,
                                  "microphone driver", verbosity_enabled)))
   {
      RARCH_ERR("[Microphone]: Failed to initialize microphone driver. Will continue without mic input.\n");
      goto error;
   }

   if (!mic_driver_st.driver || !mic_driver_st.driver->init)
   {
      goto error;
   }


   mic_driver_st.driver_context = mic_driver_st.driver->init();


   if (!mic_driver_st.driver_context || !in_samples_buf)
   {
      goto error;
   }

   RARCH_LOG("[Microphone]: Started synchronous microphone driver\n");

   mic_driver_st.input_samples_buf         = (float*)in_samples_buf;
   mic_driver_st.input_samples_buf_length  = insamples_max * sizeof(float);
   mic_driver_st.flags                    &= ~MICROPHONE_DRIVER_FLAG_CONTROL;

   if (  mic_driver_st.microphone.active &&
         !mic_driver_st.microphone.microphone_context)
   { /* If the core requested a microphone before the driver was able to provide one...*/
      /* Now that the driver and driver context are ready, let's initialize the mid */
      mic_driver_open_mic_internal(&mic_driver_st.microphone);

      if (mic_driver_st.microphone.microphone_context)
         RARCH_DBG("[Microphone]: Initialized a previously-pending microphone\n");
      else
      {
         RARCH_ERR("[Microphone]: Failed to initialize a previously pending microphone; microphone will not be used\n");

         microphone_driver_close_mic(&mic_driver_st.microphone);
      }

   }

   return true;

error:
   RARCH_ERR("[Microphone]: Failed to initialize microphone driver. Will continue without audio input.\n");
   mic_driver_st.flags &= ~MICROPHONE_DRIVER_FLAG_ACTIVE;
   return microphone_driver_deinit();
}

/**
 *
 * @param microphone Handle to the microphone to init with a context
 * Check the value of microphone->microphone_context to see if this function succeeded
 */
static void mic_driver_open_mic_internal(retro_microphone_t* microphone)
{
   microphone_driver_state_t *mic_st     = &mic_driver_st;
   settings_t *settings                  = config_get_ptr();
   const microphone_driver_t *mic_driver = mic_st->driver;
   void *driver_context                  = mic_st->driver_context;
   unsigned runloop_audio_latency        = runloop_state_get_ptr()->audio_latency;
   unsigned setting_audio_latency        = settings->uints.audio_input_latency;
   unsigned actual_sample_rate           = 0;
   unsigned audio_latency                = (runloop_audio_latency > setting_audio_latency) ?
                                           runloop_audio_latency : setting_audio_latency;
   float slowmotion_ratio                = settings->floats.slowmotion_ratio;
   size_t insamples_max                  = AUDIO_CHUNK_SIZE_NONBLOCKING * 1 * AUDIO_MAX_RATIO * slowmotion_ratio;

   if (!microphone || !mic_driver)
      return;

   microphone->sample_buffer_length = insamples_max * sizeof(int16_t);
   microphone->error                = false;
   microphone->sample_buffer        =
         (int16_t*)memalign_alloc(64, microphone->sample_buffer_length);

   if (!microphone->sample_buffer)
      goto error;

   microphone->microphone_context = mic_driver->open_mic(driver_context,
      *settings->arrays.audio_input_device ? settings->arrays.audio_input_device : NULL,
      settings->uints.audio_input_sample_rate,
      audio_latency,
      settings->uints.audio_input_block_frames,
      &actual_sample_rate);

   if (!microphone->microphone_context)
      goto error;

   microphone_driver_set_mic_state(microphone, microphone->pending_enabled);

   if (actual_sample_rate != 0)
   {
      RARCH_LOG("[Audio]: Requested microphone sample rate of %uHz, got %uHz. Updating settings with this value.\n",
                settings->uints.audio_input_sample_rate,
                actual_sample_rate
      );
      configuration_set_uint(settings, settings->uints.audio_input_sample_rate, actual_sample_rate);
   }

   RARCH_LOG("[Audio]: Initialized microphone\n", actual_sample_rate);
   return;
error:
   mic_driver_microphone_handle_free(microphone);
   RARCH_ERR("[Audio]: Driver attempted to initialize the microphone but failed\n");
}

void microphone_driver_close_mic(retro_microphone_t *microphone)
{
   microphone_driver_state_t *mic_st     = &mic_driver_st;
   const microphone_driver_t *mic_driver = mic_st->driver;
   void *driver_context                  = mic_st->driver_context;

   if (  microphone &&
         driver_context &&
         mic_driver &&
         mic_driver->close_mic)
   {
      mic_driver_microphone_handle_free(microphone);
   }
}

bool microphone_driver_set_mic_state(retro_microphone_t *microphone, bool state)
{
   microphone_driver_state_t *mic_st     = &mic_driver_st;
   const microphone_driver_t *mic_driver = mic_st->driver;
   void *driver_context                  = mic_st->driver_context;

   if (!microphone || !microphone->active || !mic_driver || !mic_driver->set_mic_active)
      return false;

   if (driver_context && microphone->microphone_context)
   { /* If the driver is initialized... */
      bool success = mic_driver->set_mic_active(driver_context, microphone->microphone_context, state);

      if (success)
         RARCH_DBG("[Microphone]: Set initialized mic state to %s\n",
                   state ? "enabled" : "disabled");
      else
         RARCH_ERR("[Microphone]: Failed to set initialized mic state to %s\n",
                   state ? "enabled" : "disabled");

      return success;
   }
   else
   { /* The driver's not ready yet, so we'll make a note
      * of what the mic's state should be */
      microphone->pending_enabled = state;
      RARCH_DBG("[Microphone]: Set pending mic state to %s\n",
                state ? "enabled" : "disabled");
      return true;
      /* This isn't an error */
   }
}

bool microphone_driver_get_mic_state(const retro_microphone_t *microphone)
{
   microphone_driver_state_t *mic_st     = &mic_driver_st;
   const microphone_driver_t *mic_driver = mic_st->driver;
   void *driver_context                  = mic_st->driver_context;

   if (!microphone || !microphone->active || !mic_driver || !mic_driver->get_mic_active)
      return false;

   if (driver_context && microphone->microphone_context)
   { /* If the driver is initialized... */
      return mic_driver->get_mic_active(driver_context, microphone->microphone_context);
   }
   else
   { /* The driver's not ready yet,
      * so we'll use that note we made of the mic's active state */
      return microphone->pending_enabled;
   }
}

/**
 * Pull queued microphone samples from the driver
 * and copy them to the provided buffer(s).
 *
 * Only one mic is supported right now,
 * but the API is designed to accommodate multiple.
 * If multi-mic support is implemented,
 * you'll want to update this function.
 *
 * Note that microphone samples are provided in mono,
 * so a "sample" and a "frame" are equivalent here.
 *
 * @param mic_st The overall state of the audio driver.
 * @param slowmotion_ratio TODO
 * @param audio_fastforward_mute True if no audio should be input while the game is in fast-forward.
 * @param[out] frames The buffer in which the core will receive microphone samples.
 * @param num_frames The size of \c frames, in samples.
 * @param is_slowmotion True if the player is running the core in slow-motion.
 * @param is_fastmotion True if the player is running the core in fast-forward.
 *
 * @see audio_driver_flush()
 */
static void audio_driver_flush_microphone_input(
      microphone_driver_state_t *mic_st,
      retro_microphone_t *microphone,
      float slowmotion_ratio,
      bool audio_fastforward_mute,
      int16_t *frames, size_t num_frames,
      bool is_slowmotion, bool is_fastmotion)
{
   if (mic_st &&                                   /* If the driver state is valid... */
         (mic_st->flags & MICROPHONE_DRIVER_FLAG_ACTIVE) && /* ...and mic support is on... */
         mic_st->driver &&
         mic_st->driver_context &&                   /* ...and the mic driver is initialized... */
         mic_st->input_samples_buf &&                /* ...with scratch space... */
         microphone &&
         microphone->active &&                       /* ...and the mic itself is initialized... */
         microphone->microphone_context &&           /* ...and ready... */
         microphone->sample_buffer &&
         microphone->sample_buffer_length &&         /* ...with a non-empty sample buffer... */

         mic_st->driver->read &&                     /* ...and valid function pointers... */
         mic_st->driver->get_mic_active &&
         mic_st->driver->get_mic_active(             /* ...and it's enabled... */
               mic_st->driver_context,
               microphone->microphone_context))
   {
      void *buffer_source  = NULL;
      unsigned sample_size = mic_driver_get_sample_size();
      size_t bytes_to_read = MIN(mic_st->input_samples_buf_length, num_frames * sample_size);
      ssize_t bytes_read   = mic_st->driver->read(
            mic_st->driver_context,
            microphone->microphone_context,
            mic_st->input_samples_buf,
            bytes_to_read);
      /* First, get the most recent mic data */

      if (bytes_read < 0)
      { /* If there was an error... */
         microphone->error = true;
         return;
      }
      else if (bytes_read == 0)
      {
         microphone->error = false;
         return;
      }

      if (mic_st->flags & MICROPHONE_FLAG_USE_FLOAT)
      {
         convert_float_to_s16(mic_st->input_samples_conv_buf, mic_st->input_samples_buf, bytes_read / sample_size);
         buffer_source = mic_st->input_samples_conv_buf;
      }
      else
      {
         buffer_source = mic_st->input_samples_buf;
      }

      memcpy(frames, buffer_source, num_frames * sizeof(int16_t));
   }
}


int microphone_driver_read(retro_microphone_t *microphone, int16_t* frames, size_t num_frames)
{
   uint32_t runloop_flags;
   size_t frames_remaining           = num_frames;
   microphone_driver_state_t *mic_st = &mic_driver_st;

   if (mic_st->flags & MICROPHONE_DRIVER_FLAG_SUSPENDED
       || num_frames == 0
       || !frames
       || !microphone
       || !microphone->active
       || !microphone_driver_get_mic_state(microphone)
         )
   { /* If the driver is suspended, or the core didn't actually ask for frames,
      * or the microphone is disabled... */
      return -1;
   }

   if (!(mic_st->driver_context && microphone->microphone_context))
   { /* If the microphone isn't ready... */
      return 0; /* Not an error */
   }

   runloop_flags                   = runloop_get_flags();

   do
   {
      size_t frames_to_read =
            (frames_remaining > (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1)) ?
            (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1) : frames_remaining;

      if (!(    (runloop_flags & RUNLOOP_FLAG_PAUSED)
                || !(mic_st->flags & MICROPHONE_DRIVER_FLAG_ACTIVE)
                || !(mic_st->input_samples_buf)))
         /* If the game is running, the audio driver and mic are running,
          * and the input sample buffer is valid... */
         audio_driver_flush_microphone_input(mic_st,
                                             microphone,
                                             config_get_ptr()->floats.slowmotion_ratio,
                                             config_get_ptr()->bools.audio_fastforward_mute,
                                             frames,
                                             frames_to_read,
                                             runloop_flags & RUNLOOP_FLAG_SLOWMOTION,
                                             runloop_flags & RUNLOOP_FLAG_FASTMOTION);
      frames_remaining -= frames_to_read;
      frames           += frames_to_read << 1;
   }
   while (frames_remaining > 0);

   return num_frames;
}

/* NOTE: The core may request a microphone before the driver is ready.
 * A pending handle will be provided in that case, and the frontend will
 * initialize the microphone when the time is right;
 * do not call this function twice on the same mic. */
retro_microphone_t *microphone_driver_open_mic(void)
{
   microphone_driver_state_t *mic_st     = &mic_driver_st;
   const settings_t *settings            = config_get_ptr();
   const microphone_driver_t *mic_driver = mic_st->driver;
   void *driver_context                  = mic_st->driver_context;

   if (!settings)
   {
      RARCH_ERR("[Microphone]: Failed to open microphone due to uninitialized config\n");
      return NULL;
   }

   if (!mic_driver)
   {
      RARCH_ERR("[Microphone]: Failed to initialize microphone due to uninitialized driver\n");
      return NULL;
   }

   if (!settings->bools.audio_enable_input)
   { /* Not checking mic_st->flags because they might not be set yet;
      * don't forget, the core can ask for a mic
      * before the audio driver is ready to create one. */
      RARCH_WARN("[Microphone]: Refused to initialize microphone because it's disabled in the settings\n");
      return NULL;
   }

   if (mic_st->microphone.active)
   { /* If the core has requested a second microphone... */
      RARCH_ERR("[Microphone]: Failed to initialize a second microphone, frontend only supports one at a time right now\n");
      if (mic_st->microphone.microphone_context)
         /* If that mic is initialized... */
         RARCH_ERR("[Microphone]: An initialized microphone exists\n");
      else
         /* That mic is pending */
         RARCH_ERR("[Microphone]: A microphone is pending initialization\n");

      return NULL;
   }

   /* Cores might ask for a microphone before the audio driver is ready to provide them;
    * if that happens, we have to initialize the microphones later.
    * But the user still wants a handle, so we'll give them one.
    */
   mic_driver_microphone_handle_init(&mic_st->microphone);
   /* If context is NULL, the handle won't have a valid microphone context (but we'll create one later) */

   if (driver_context)
   { /* If the microphone driver is ready to open a microphone... */
      mic_driver_open_mic_internal(&mic_st->microphone);

      if (mic_st->microphone.microphone_context) /* If the microphone was successfully initialized... */
         RARCH_LOG("[Audio]: Initialized the requested microphone successfully\n");
      else
         goto error;
   }
   else
   { /* If the audio driver isn't ready to create a microphone... */
      RARCH_LOG("[Audio]: Microphone requested before audio context was ready; deferring initialization\n");
   }

   return &mic_st->microphone;
error:
   mic_driver_microphone_handle_free(&mic_st->microphone);

   return NULL;
}

/* TODO: When adding support for multiple microphones,
 * make sure you clean them all up in here. */
static bool microphone_driver_close_microphones(void)
{
   microphone_driver_state_t *mic_st     = &mic_driver_st;
   const microphone_driver_t *mic_driver = mic_st->driver;

   if (  !mic_driver ||
         !mic_driver->close_mic ||
         !mic_st->driver_context ||
         !mic_st->microphone.active)
      return false;

   microphone_driver_close_mic(&mic_st->microphone);

   return true;
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

static bool microphone_driver_deinit_internal(bool microphone_enable)
{
   microphone_driver_state_t *mic_st = &mic_driver_st;
   if (mic_st->driver
       && mic_st->driver->free)
   {
      if (mic_st->driver_context)
         mic_st->driver->free(mic_st->driver_context);
      mic_st->driver_context = NULL;
   }

   if (mic_st->input_samples_conv_buf)
      memalign_free(mic_st->input_samples_conv_buf);
   mic_st->input_samples_conv_buf      = NULL;

   if (!microphone_enable)
   {
      mic_st->flags &= ~MICROPHONE_DRIVER_FLAG_ACTIVE;
      return false;
   }

   if (mic_st->input_samples_buf)
      memalign_free(mic_st->input_samples_buf);
   mic_st->input_samples_buf = NULL;

   return true;
}

bool microphone_driver_deinit(void)
{
   settings_t *settings = config_get_ptr();
   microphone_driver_free_devices_list();
   microphone_driver_close_microphones();
   return microphone_driver_deinit_internal(
         settings->bools.audio_enable_input);
}