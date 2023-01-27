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

#include "SDL.h"
#include "SDL_audio.h"
#include "verbosity.h"
#include "retro_assert.h"
#include "retro_math.h"
#include "microphone/microphone_driver.h"
#include <rthreads/rthreads.h>
#include <queues/fifo_queue.h>

typedef struct sdl_microphone_handle
{
#ifdef HAVE_THREADS
   slock_t *lock;
   scond_t *cond;
#endif

   /**
    * The queue used to store incoming samples from the driver.
    */
   fifo_buffer_t *sample_buffer;
   bool is_paused;
   SDL_AudioDeviceID device_id;
} sdl_microphone_handle_t;

typedef struct sdl_microphone
{
#ifdef HAVE_THREADS
   slock_t *lock;
   scond_t *cond;
#endif

   bool nonblock;
   bool is_paused;

   /* Only one microphone is supported right now;
    * the driver state should track multiple microphone handles,
    * but the driver *context* should track multiple microphone contexts */
   sdl_microphone_handle_t *microphone;
} sdl_microphone_t;

static INLINE int find_num_frames(int rate, int latency)
{
   int frames = (rate * latency) / 1000;

   /* SDL only likes 2^n sized buffers. */

   return next_pow2(frames);
}

static void *sdl_microphone_init(void)
{
   sdl_microphone_t *sdl        = NULL;
   uint32_t sdl_subsystem_flags = SDL_WasInit(0);

   /* Initialise audio subsystem, if required */
   if (sdl_subsystem_flags == 0)
   {
      if (SDL_Init(SDL_INIT_AUDIO) < 0)
         return NULL;
   }
   else if ((sdl_subsystem_flags & SDL_INIT_AUDIO) == 0)
   {
      if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
         return NULL;
   }

   sdl = (sdl_microphone_t*)calloc(1, sizeof(*sdl));
   if (!sdl)
      return NULL;

#ifdef HAVE_THREADS
   sdl->lock = slock_new();
   sdl->cond = scond_new();
   if (!sdl->lock || !sdl->cond)
   {
      goto error;
   }
#endif

   return sdl;

error:
#ifdef HAVE_THREADS
   if (sdl)
   {
      slock_free(sdl->lock);
      scond_free(sdl->cond);
   }
#endif
   free(sdl);
   return NULL;
}

static void sdl_microphone_close_mic(void *data, void *microphone_context);

static void sdl_microphone_free(void *data)
{
   sdl_microphone_t *sdl = (sdl_microphone_t*)data;

   if (sdl)
   {
      if (sdl->microphone)
         sdl_microphone_close_mic(sdl, sdl->microphone);

#ifdef HAVE_THREADS
      slock_free(sdl->lock);
      scond_free(sdl->cond);
#endif

      SDL_QuitSubSystem(SDL_INIT_AUDIO);
   }
   free(sdl);
}

static void sdl_audio_record_cb(void *data, Uint8 *stream, int len)
{
   sdl_microphone_t  *sdl = (sdl_microphone_t*)data;
   size_t      avail = FIFO_WRITE_AVAIL(sdl->microphone->sample_buffer);
   size_t read_size  = len > (int)avail ? avail : len;
   /* If the sample buffer is almost full,
    * just write as much as we can into it*/

   fifo_write(sdl->microphone->sample_buffer, stream, read_size);
#ifdef HAVE_THREADS
   scond_signal(sdl->microphone->cond);
#endif
}

static void *sdl_microphone_open_mic(void *data,
                                     const char *device,
                                     unsigned rate,
                                     unsigned latency,
                                     unsigned block_frames,
                                     unsigned *new_rate)
{
   int frames;
   size_t bufsize;
   sdl_microphone_t *sdl                   = (sdl_microphone_t*)data;
   sdl_microphone_handle_t *microphone = NULL;
   SDL_AudioSpec desired_spec         = {0};
   SDL_AudioSpec out;
   void *tmp                          = NULL;

   if (!sdl || !SDL_WasInit(SDL_INIT_AUDIO))
   { /* If the audio driver wasn't initialized yet... */
      RARCH_ERR("[SDL mic]: Attempted to initialize input device before initializing the audio subsystem\n");
      return NULL;
   }

   if (sdl->microphone)
   {
      RARCH_ERR("[SDL mic]: Attempted to initialize a second microphone, but the driver only supports one right now\n");
      return NULL;
   }

   microphone = (sdl_microphone_handle_t *)calloc(1, sizeof(sdl_microphone_handle_t));
   if (!microphone)
      return NULL;

   if (verbosity_is_enabled())
   { /* Only print SDL audio devices if verbose logging is enabled */
      int i;
      int num_available_microphones = SDL_GetNumAudioDevices(true);
      RARCH_DBG("[SDL mic]: %d audio capture devices found:\n", num_available_microphones);
      for (i = 0; i < num_available_microphones; ++i) {
         RARCH_DBG("[SDL mic]:    - %s\n", SDL_GetAudioDeviceName(i, true));
      }
   }

   /* We have to buffer up some data ourselves, so we let SDL
    * carry approximately half of the latency.
    *
    * SDL double buffers audio and we do as well. */
   frames = find_num_frames(rate, latency / 4);

   desired_spec.freq     = rate;
   desired_spec.format   = AUDIO_S16SYS;
   desired_spec.channels = 1; /* Microphones only usually provide input in mono */
   desired_spec.samples  = frames;
   desired_spec.userdata = sdl;
   desired_spec.callback = sdl_audio_record_cb;

   microphone->device_id = SDL_OpenAudioDevice(NULL, true, &desired_spec, &out, 0);
   if (microphone->device_id == 0)
   {
      RARCH_ERR("[SDL mic]: Failed to open SDL audio input device: %s\n", SDL_GetError());
      goto error;
   }
   RARCH_DBG("[SDL mic]: Opened SDL audio input device with ID %u\n",
             microphone->device_id);
   RARCH_DBG("[SDL mic]: Requested a microphone frequency of %u Hz, got %u Hz\n",
             desired_spec.freq, out.freq);
   RARCH_DBG("[SDL mic]: Requested %u channels for microphone, got %u\n",
             desired_spec.channels, out.channels);
   RARCH_DBG("[SDL mic]: Requested a %u-sample microphone buffer, got %u samples (%u bytes)\n",
             frames, out.samples, out.size);
   RARCH_DBG("[SDL mic]: Got a microphone silence value of %u\n", out.silence);
   RARCH_DBG("[SDL mic]: Requested microphone audio format: %u-bit %s %s %s endian\n",
             SDL_AUDIO_BITSIZE(desired_spec.format),
             SDL_AUDIO_ISSIGNED(desired_spec.format) ? "signed" : "unsigned",
             SDL_AUDIO_ISFLOAT(desired_spec.format) ? "floating-point" : "integer",
             SDL_AUDIO_ISBIGENDIAN(desired_spec.format) ? "big" : "little");

   RARCH_DBG("[SDL mic]: Received microphone audio format: %u-bit %s %s %s endian\n",
             SDL_AUDIO_BITSIZE(desired_spec.format),
             SDL_AUDIO_ISSIGNED(desired_spec.format) ? "signed" : "unsigned",
             SDL_AUDIO_ISFLOAT(desired_spec.format) ? "floating-point" : "integer",
             SDL_AUDIO_ISBIGENDIAN(desired_spec.format) ? "big" : "little");

   if (new_rate)
      *new_rate = out.freq;

#ifdef HAVE_THREADS
   microphone->lock = slock_new();
   microphone->cond = scond_new();
#endif

   RARCH_LOG("[SDL audio]: Requested %u ms latency for input device, got %d ms\n",
             latency, (int)(out.samples * 4 * 1000 / out.freq));

   /* Create a buffer twice as big as needed and prefill the buffer. */
   bufsize                   = out.samples * 2 * sizeof(int16_t);
   tmp                       = calloc(1, bufsize);
   microphone->sample_buffer = fifo_new(bufsize);

   RARCH_DBG("[SDL audio]: Initialized microphone sample queue with %u bytes\n", bufsize);

   if (tmp)
   {
      fifo_write(microphone->sample_buffer, tmp, bufsize);
      free(tmp);
   }

   sdl->microphone = microphone;

   RARCH_LOG("[SDL audio]: Initialized microphone with device ID %u\n", microphone->device_id);
   return microphone;

error:
   free(microphone);
   return NULL;
}

static void sdl_microphone_close_mic(void *data, void *microphone_context)
{
   sdl_microphone_t *sdl                   = (sdl_microphone_t*)data;
   sdl_microphone_handle_t *microphone = (sdl_microphone_handle_t *)microphone_context;

   if (sdl && microphone)
   {
      retro_assert(sdl->microphone == microphone);
      /* Driver only supports one microphone for now; when multi-mic support
       * is added, you may still want to assert that the microphone
       * was indeed created by the driver */

      if (microphone->device_id > 0)
      { /* If the microphone was originally initialized successfully... */
         SDL_CloseAudioDevice(microphone->device_id);
      }

      if (microphone->sample_buffer)
      {
         fifo_free(microphone->sample_buffer);
      }

#ifdef HAVE_THREADS
      slock_free(microphone->lock);
      scond_free(microphone->cond);
#endif

      RARCH_LOG("[SDL audio]: Freed microphone with former device ID %u\n", microphone->device_id);
      sdl->microphone = NULL;
      free(microphone);
   }
}

static bool sdl_microphone_mic_alive(const void *data, const void *microphone_context)
{
   const sdl_microphone_t *sdl                   = (const sdl_microphone_t*)data;
   const sdl_microphone_handle_t *microphone = (const sdl_microphone_handle_t*)microphone_context;

   if (!sdl || !microphone)
      return false;
   /* Both params must be non-null */

   return !microphone->is_paused;
   /* The mic might be paused due to app requirements,
    * or it might be stopped because the entire audio driver is stopped. */
}

static bool sdl_microphone_set_mic_active(void *data, void *microphone_context, bool enabled)
{
   sdl_microphone_t *sdl                   = (sdl_microphone_t*)data;
   sdl_microphone_handle_t *microphone = (sdl_microphone_handle_t*)microphone_context;

   if (!sdl || !microphone)
      return false;

   microphone->is_paused = !enabled;
   if (!sdl->is_paused)
   { /* If the entire audio driver isn't paused... */
      SDL_PauseAudioDevice(microphone->device_id, microphone->is_paused);
   }
   RARCH_DBG("[SDL audio]: Set state of microphone %u to %s\n",
      microphone->device_id, enabled ? "enabled" : "disabled");
   return true;
}

static bool sdl_microphone_start_mic(void *data, void *microphone_context)
{
   return sdl_microphone_set_mic_active(data, microphone_context, true);
}

static bool sdl_microphone_stop_mic(void *data, void *microphone_context)
{
   return sdl_microphone_set_mic_active(data, microphone_context, false);
}

static void sdl_microphone_set_nonblock_state(void *data, bool state)
{
   sdl_microphone_t *sdl = (sdl_microphone_t*)data;
   if (sdl)
      sdl->nonblock = state;
}

static ssize_t sdl_microphone_read(void *data, void *microphone_context, void *buf, size_t size)
{
   ssize_t ret                        = 0;
   sdl_microphone_t *sdl                   = (sdl_microphone_t*)data;
   sdl_microphone_handle_t *microphone = (sdl_microphone_handle_t*)microphone_context;

   if (!sdl || !microphone || !buf)
      return -1;

   if (sdl->nonblock)
   { /* If we shouldn't block on an empty queue... */
      size_t avail, read_amt;

      SDL_LockAudioDevice(microphone->device_id); /* Stop the SDL mic thread */
      avail = FIFO_READ_AVAIL(microphone->sample_buffer);
      read_amt = avail > size ? size : avail;
      if (read_amt > 0)
      {  /* If the incoming queue isn't empty... */
         fifo_read(microphone->sample_buffer, buf, read_amt);
         /* ...then read as much data as will fit in buf */
      }
      SDL_UnlockAudioDevice(microphone->device_id); /* Let the mic thread run again */
      ret = read_amt;
   }
   else
   {
      size_t read = 0;

      while (read < size)
      { /* Until we've given the caller as much data as they've asked for... */
         size_t avail;

         SDL_LockAudioDevice(microphone->device_id);
         /* Stop the SDL microphone thread from running */
         avail = FIFO_READ_AVAIL(microphone->sample_buffer);

         if (avail == 0)
         { /* If the incoming sample queue is empty... */
            SDL_UnlockAudioDevice(microphone->device_id);
            /* Let the SDL microphone thread run so it can push some incoming samples */
#ifdef HAVE_THREADS
            slock_lock(microphone->lock);
            /* Let *only* the SDL microphone thread access the incoming sample queue. */

            scond_wait(microphone->cond, microphone->lock);
            /* Wait until the SDL microphone thread tells us it's added some samples. */

            slock_unlock(microphone->lock);
            /* Allow this thread to access the incoming sample queue, which we'll do next iteration */
#endif
         }
         else
         {
            size_t read_amt = MIN(size - read, avail);
            fifo_read(microphone->sample_buffer, buf + read, read_amt);
            /* Read as many samples as we have available without underflowing the queue */

            SDL_UnlockAudioDevice(microphone->device_id);
            /* Let the SDL microphone thread run again */
            read += read_amt;
         }
      }
      ret = read;
   }

   return ret;
}

static bool sdl_microphone_stop(void *data)
{
   sdl_microphone_t *sdl = (sdl_microphone_t*)data;
   sdl->is_paused = true;

   if (sdl->microphone)
   {
      /* Stop the microphone independently of whether it's paused;
       * note that upon sdl_audio_start, the microphone won't resume
       * if it was previously paused */
      SDL_PauseAudioDevice(sdl->microphone->device_id, true);
   }

   return true;
}

static bool sdl_microphone_alive(void *data)
{
   sdl_microphone_t *sdl = (sdl_microphone_t*)data;
   if (!sdl)
      return false;
   return !sdl->is_paused;
}

static bool sdl_microphone_start(void *data, bool is_shutdown)
{
   sdl_microphone_t *sdl = (sdl_microphone_t*)data;
   sdl->is_paused = false;

   if (sdl->microphone)
   {
      if (!sdl->microphone->is_paused)
         SDL_PauseAudioDevice(sdl->microphone->device_id, false);
      /* If the microphone wasn't paused beforehand... */
   }

   return true;
}

microphone_driver_t microphone_sdl = {
      sdl_microphone_init,
      sdl_microphone_free,
      sdl_microphone_read,
      sdl_microphone_start,
      sdl_microphone_stop,
      sdl_microphone_alive,
      sdl_microphone_set_nonblock_state,
      "sdl2",
      NULL,
      NULL,
      sdl_microphone_open_mic,
      sdl_microphone_close_mic,
      sdl_microphone_mic_alive,
      sdl_microphone_start_mic,
      sdl_microphone_stop_mic
};