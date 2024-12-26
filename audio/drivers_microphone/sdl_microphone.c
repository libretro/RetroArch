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
#include "audio/microphone_driver.h"
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
   SDL_AudioDeviceID device_id;
   SDL_AudioSpec device_spec;
} sdl_microphone_handle_t;

typedef struct sdl_microphone
{
   bool nonblock;
} sdl_microphone_t;

static INLINE int sdl_microphone_find_num_frames(int rate, int latency)
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

   if (!(sdl = (sdl_microphone_t*)calloc(1, sizeof(*sdl))))
      return NULL;

   return sdl;
}

static void sdl_microphone_close_mic(void *driver_context, void *microphone_context)
{
   sdl_microphone_handle_t *microphone = (sdl_microphone_handle_t *)microphone_context;

   if (microphone)
   {
      /* If the microphone was originally initialized successfully... */
      if (microphone->device_id > 0)
         SDL_CloseAudioDevice(microphone->device_id);

      fifo_free(microphone->sample_buffer);

#ifdef HAVE_THREADS
      slock_free(microphone->lock);
      scond_free(microphone->cond);
#endif

      RARCH_LOG("[SDL audio]: Freed microphone with former device ID %u\n", microphone->device_id);
      free(microphone);
   }
}


static void sdl_microphone_free(void *data)
{
   sdl_microphone_t *sdl = (sdl_microphone_t*)data;

   if (sdl)
      SDL_QuitSubSystem(SDL_INIT_AUDIO);
   free(sdl);
   /* NOTE: The microphone frontend should've closed the mics by now */
}

static void sdl_audio_record_cb(void *data, Uint8 *stream, int len)
{
   sdl_microphone_handle_t *microphone = (sdl_microphone_handle_t*)data;
   size_t      avail                   = FIFO_WRITE_AVAIL(microphone->sample_buffer);
   size_t read_size                    = MIN(len, (int)avail);
   /* If the sample buffer is almost full, just write as much as we can into it*/

   fifo_write(microphone->sample_buffer, stream, read_size);
#ifdef HAVE_THREADS
   scond_signal(microphone->cond);
#endif
}

static void *sdl_microphone_open_mic(void *driver_context,
      const char *device,
      unsigned rate,
      unsigned latency,
      unsigned *new_rate)
{
   int frames;
   size_t bufsize;
   sdl_microphone_handle_t *microphone = NULL;
   SDL_AudioSpec desired_spec          = {0};
   void *tmp                           = NULL;

   /* If the audio driver wasn't initialized yet... */
   if (!SDL_WasInit(SDL_INIT_AUDIO))
   {
      RARCH_ERR("[SDL mic]: Attempted to initialize input device before initializing the audio subsystem\n");
      return NULL;
   }

   if (!(microphone = (sdl_microphone_handle_t *)
            calloc(1, sizeof(sdl_microphone_handle_t))))
      return NULL;

   /* Only print SDL audio devices if verbose logging is enabled */
   if (verbosity_is_enabled())
   { 
      int i;
      int num_available_microphones = SDL_GetNumAudioDevices(true);
      RARCH_DBG("[SDL mic]: %d audio capture devices found:\n", num_available_microphones);
      for (i = 0; i < num_available_microphones; ++i)
         RARCH_DBG("[SDL mic]:    - %s\n", SDL_GetAudioDeviceName(i, true));
   }

   /* We have to buffer up some data ourselves, so we let SDL
    * carry approximately half of the latency.
    *
    * SDL double buffers audio and we do as well. */
   frames                = sdl_microphone_find_num_frames(rate, latency / 4);

   desired_spec.freq     = rate;
   desired_spec.format   = AUDIO_F32SYS;
   desired_spec.channels = 1; /* Microphones only usually provide input in mono */
   desired_spec.samples  = frames;
   desired_spec.userdata = microphone;
   desired_spec.callback = sdl_audio_record_cb;

   microphone->device_id = SDL_OpenAudioDevice(
         NULL,
         true,
         &desired_spec,
         &microphone->device_spec,
         SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_FORMAT_CHANGE);

   if (microphone->device_id == 0)
   {
      RARCH_ERR("[SDL mic]: Failed to open SDL audio input device: %s\n", SDL_GetError());
      goto error;
   }
   RARCH_DBG("[SDL mic]: Opened SDL audio input device with ID %u\n",
             microphone->device_id);
   RARCH_DBG("[SDL mic]: Requested a microphone frequency of %u Hz, got %u Hz\n",
             desired_spec.freq, microphone->device_spec.freq);
   RARCH_DBG("[SDL mic]: Requested %u channels for microphone, got %u\n",
             desired_spec.channels, microphone->device_spec.channels);
   RARCH_DBG("[SDL mic]: Requested a %u-sample microphone buffer, got %u samples (%u bytes)\n",
             frames, microphone->device_spec.samples, microphone->device_spec.size);
   RARCH_DBG("[SDL mic]: Got a microphone silence value of %u\n", microphone->device_spec.silence);
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
      *new_rate = microphone->device_spec.freq;

#ifdef HAVE_THREADS
   microphone->lock = slock_new();
   microphone->cond = scond_new();
#endif

   RARCH_LOG("[SDL audio]: Requested %u ms latency for input device, got %d ms\n",
             latency, (int)(microphone->device_spec.samples * 4 * 1000 / microphone->device_spec.freq));

   /* Create a buffer twice as big as needed and prefill the buffer. */
   bufsize                   = microphone->device_spec.samples * 2 * (SDL_AUDIO_BITSIZE(microphone->device_spec.format) / 8);
   tmp                       = calloc(1, bufsize);
   microphone->sample_buffer = fifo_new(bufsize);

   RARCH_DBG("[SDL audio]: Initialized microphone sample queue with %u bytes\n", bufsize);

   if (tmp)
   {
      fifo_write(microphone->sample_buffer, tmp, bufsize);
      free(tmp);
   }

   RARCH_LOG("[SDL audio]: Initialized microphone with device ID %u\n", microphone->device_id);
   return microphone;

error:
   free(microphone);
   return NULL;
}

static bool sdl_microphone_mic_alive(const void *data, const void *microphone_context)
{
   const sdl_microphone_handle_t *microphone = (const sdl_microphone_handle_t*)microphone_context;
   if (!microphone)
      return false;
   /* Both params must be non-null */
   return SDL_GetAudioDeviceStatus(microphone->device_id) == SDL_AUDIO_PLAYING;
}

static bool sdl_microphone_start_mic(void *driver_context, void *microphone_context)
{
   sdl_microphone_handle_t *microphone = (sdl_microphone_handle_t*)microphone_context;

   if (!microphone)
      return false;

   SDL_PauseAudioDevice(microphone->device_id, false);

   if (SDL_GetAudioDeviceStatus(microphone->device_id) != SDL_AUDIO_PLAYING)
   {
      RARCH_ERR("[SDL mic]: Failed to start microphone %u: %s\n", microphone->device_id, SDL_GetError());
      return false;
   }

   RARCH_DBG("[SDL mic]: Started microphone %u\n", microphone->device_id);
   return true;
}

static bool sdl_microphone_stop_mic(void *driver_context, void *microphone_context)
{
   sdl_microphone_t *sdl               = (sdl_microphone_t*)driver_context;
   sdl_microphone_handle_t *microphone = (sdl_microphone_handle_t*)microphone_context;

   if (!sdl || !microphone)
      return false;

   SDL_PauseAudioDevice(microphone->device_id, true);

   switch (SDL_GetAudioDeviceStatus(microphone->device_id))
   {
      case SDL_AUDIO_PLAYING:
         RARCH_ERR("[SDL mic]: Microphone %u failed to pause\n", microphone->device_id);
         return false;
      case SDL_AUDIO_STOPPED:
         RARCH_WARN("[SDL mic]: Microphone %u is in state STOPPED; it may not start again\n", microphone->device_id);
         /* fall-through */
      case SDL_AUDIO_PAUSED:
         break;
      default:
         RARCH_ERR("[SDL mic]: Microphone %u is in unknown state\n", microphone->device_id);
         return false;
   }

   return true;
}

static void sdl_microphone_set_nonblock_state(void *driver_context, bool state)
{
   sdl_microphone_t *sdl = (sdl_microphone_t*)driver_context;
   if (sdl)
      sdl->nonblock = state;
}

static int sdl_microphone_read(void *driver_context, void *microphone_context, void *buf, size_t size)
{
   int ret                             = 0;
   sdl_microphone_t *sdl               = (sdl_microphone_t*)driver_context;
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
      ret = (int)read_amt;
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
      ret = (int)read;
   }

   return ret;
}

static bool sdl_microphone_mic_use_float(const void *driver_context, const void *microphone_context)
{
   sdl_microphone_handle_t *microphone = (sdl_microphone_handle_t*)microphone_context;
   return SDL_AUDIO_ISFLOAT(microphone->device_spec.format);
}

microphone_driver_t microphone_sdl = {
      sdl_microphone_init,
      sdl_microphone_free,
      sdl_microphone_read,
      sdl_microphone_set_nonblock_state,
      "sdl2",
      NULL,
      NULL,
      sdl_microphone_open_mic,
      sdl_microphone_close_mic,
      sdl_microphone_mic_alive,
      sdl_microphone_start_mic,
      sdl_microphone_stop_mic,
      sdl_microphone_mic_use_float,
};
