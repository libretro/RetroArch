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

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <rthreads/rthreads.h>
#include <queues/fifo_queue.h>
#include <retro_inline.h>
#include <retro_math.h>

#include "SDL.h"
#include "SDL_audio.h"

#include "../audio_driver.h"
#include "../../verbosity.h"
#include "retro_assert.h"

/**
 * We need the SDL_{operation}AudioDevice functions for microphone support,
 * but those were introduced in SDL 2.0.0
 * (according to their docs).
 * Some legacy build platforms are stuck on 1.x.x,
 * so we have to accommodate them.
 * That comes in the form of stub implementations of the missing functions
 * that delegate to the non-Device versions.
 *
 * Only three platforms (as of this writing) are stuck on SDL 1.x.x,
 * so it's not a big deal to exclude mic support from them.
 **/
#if HAVE_SDL2
#define SDL_DRIVER_MIC_SUPPORT 1
#define SDL_DRIVER_DEVICE_FUNCTIONS 1
#else
typedef Uint32 SDL_AudioDeviceID;

/** Compatibility stub that defers to SDL_PauseAudio. */
#define SDL_PauseAudioDevice(dev, pause_on) SDL_PauseAudio(pause_on)

/** Compatibility stub that defers to SDL_LockAudio. */
#define SDL_LockAudioDevice(dev) SDL_LockAudio()

/** Compatibility stub that defers to SDL_UnlockAudio. */
#define SDL_UnlockAudioDevice(dev) SDL_UnlockAudio()

/** Compatibility stub that defers to SDL_CloseAudio. */
#define SDL_CloseAudioDevice(dev) SDL_CloseAudio()
#endif

#ifdef SDL_DRIVER_MIC_SUPPORT
typedef struct sdl_audio_microphone
{
#ifdef HAVE_THREADS
   slock_t *lock;
   scond_t *cond;
#endif

   /**
    * The queue used to store incoming samples from the driver.
    * Audio from the microphone is stored here,
    * the first stop before the audio driver processes it
    * and makes it ready for the core.
    */
   fifo_buffer_t *sample_buffer;
   bool is_paused;
   SDL_AudioDeviceID device_id;
} sdl_audio_microphone_t;

static void sdl_audio_free_microphone(void *data, void *microphone_context);
#endif

typedef struct sdl_audio
{
#ifdef HAVE_THREADS
   slock_t *lock;
   scond_t *cond;
#endif
   /**
    * The queue used to store outgoing samples to be played by the driver.
    * Audio from the core ultimately makes its way here,
    * the last stop before the driver plays it.
    */
   fifo_buffer_t *speaker_buffer;
   bool nonblock;
   bool is_paused;
   SDL_AudioDeviceID speaker_device;

#ifdef SDL_DRIVER_MIC_SUPPORT
   /* Only one microphone is supported right now;
    * the driver state should track multiple microphone handles,
    * but the driver *context* should track multiple microphone contexts */
   sdl_audio_microphone_t *microphone;
#endif
} sdl_audio_t;

static void sdl_audio_playback_cb(void *data, Uint8 *stream, int len)
{
   sdl_audio_t  *sdl = (sdl_audio_t*)data;
   size_t      avail = FIFO_READ_AVAIL(sdl->speaker_buffer);
   size_t write_size = len > (int)avail ? avail : len;

   fifo_read(sdl->speaker_buffer, stream, write_size);
#ifdef HAVE_THREADS
   scond_signal(sdl->cond);
#endif

   /* If underrun, fill rest with silence. */
   memset(stream + write_size, 0, len - write_size);
}

#ifdef SDL_DRIVER_MIC_SUPPORT
static void sdl_audio_record_cb(void *data, Uint8 *stream, int len)
{
   sdl_audio_t  *sdl = (sdl_audio_t*)data;
   size_t      avail = FIFO_WRITE_AVAIL(sdl->microphone->sample_buffer);
   size_t read_size  = len > (int)avail ? avail : len;
   /* If the sample buffer is almost full,
    * just write as much as we can into it*/

   fifo_write(sdl->microphone->sample_buffer, stream, read_size);
#ifdef HAVE_THREADS
   scond_signal(sdl->microphone->cond);
#endif
}
#endif

static INLINE int find_num_frames(int rate, int latency)
{
   int frames = (rate * latency) / 1000;

   /* SDL only likes 2^n sized buffers. */

   return next_pow2(frames);
}

static void *sdl_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   int frames;
   size_t bufsize;
   SDL_AudioSpec out;
   SDL_AudioSpec spec           = {0};
   void *tmp                    = NULL;
   sdl_audio_t *sdl             = NULL;
   uint32_t sdl_subsystem_flags = SDL_WasInit(0);

   (void)device;

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

   sdl = (sdl_audio_t*)calloc(1, sizeof(*sdl));
   if (!sdl)
      return NULL;

   /* We have to buffer up some data ourselves, so we let SDL
    * carry approximately half of the latency.
    *
    * SDL double buffers audio and we do as well. */
   frames        = find_num_frames(rate, latency / 4);

   /* First, let's initialize the output device. */
   spec.freq     = rate;
   spec.format   = AUDIO_S16SYS;
   spec.channels = 2;
   spec.samples  = frames; /* This is in audio frames, not samples ... :( */
   spec.callback = sdl_audio_playback_cb;
   spec.userdata = sdl;

   /* No compatibility stub for SDL_OpenAudioDevice because its return value
    * is different from that of SDL_OpenAudio. */
#if SDL_DRIVER_DEVICE_FUNCTIONS
   sdl->speaker_device = SDL_OpenAudioDevice(NULL, false, &spec, &out, 0);

   if (sdl->speaker_device == 0)
#else
   sdl->speaker_device = SDL_OpenAudio(&spec, &out);

   if (sdl->speaker_device < 0)
#endif
   {
      RARCH_ERR("[SDL audio]: Failed to open SDL audio output device: %s\n", SDL_GetError());
      goto error;
   }

   *new_rate                = out.freq;

#ifdef HAVE_THREADS
   sdl->lock                = slock_new();
   sdl->cond                = scond_new();
#endif

   RARCH_LOG("[SDL audio]: Requested %u ms latency for output device, got %d ms\n",
         latency, (int)(out.samples * 4 * 1000 / (*new_rate)));

   /* Create a buffer twice as big as needed and prefill the buffer. */
   bufsize             = out.samples * 4 * sizeof(int16_t);
   tmp                 = calloc(1, bufsize);
   sdl->speaker_buffer = fifo_new(bufsize);

   if (tmp)
   {
      fifo_write(sdl->speaker_buffer, tmp, bufsize);
      free(tmp);
   }

   SDL_PauseAudioDevice(sdl->speaker_device, false);

   return sdl;

error:
   free(sdl);
   return NULL;
}

static ssize_t sdl_audio_write(void *data, const void *buf, size_t size)
{
   ssize_t ret      = 0;
   sdl_audio_t *sdl = (sdl_audio_t*)data;

   if (sdl->nonblock)
   {
      size_t avail, write_amt;

      SDL_LockAudioDevice(sdl->speaker_device);
      avail = FIFO_WRITE_AVAIL(sdl->speaker_buffer);
      write_amt = avail > size ? size : avail;
      fifo_write(sdl->speaker_buffer, buf, write_amt);
      SDL_UnlockAudioDevice(sdl->speaker_device);
      ret = write_amt;
   }
   else
   {
      size_t written = 0;

      while (written < size)
      {
         size_t avail;

         SDL_LockAudioDevice(sdl->speaker_device);
         avail = FIFO_WRITE_AVAIL(sdl->speaker_buffer);

         if (avail == 0)
         {
            SDL_UnlockAudioDevice(sdl->speaker_device);
#ifdef HAVE_THREADS
            slock_lock(sdl->lock);
            scond_wait(sdl->cond, sdl->lock);
            slock_unlock(sdl->lock);
#endif
         }
         else
         {
            size_t write_amt = size - written > avail ? avail : size - written;
            fifo_write(sdl->speaker_buffer, (const char*)buf + written, write_amt);
            SDL_UnlockAudioDevice(sdl->speaker_device);
            written += write_amt;
         }
      }
      ret = written;
   }

   return ret;
}

static bool sdl_audio_stop(void *data)
{
   sdl_audio_t *sdl = (sdl_audio_t*)data;
   sdl->is_paused = true;
   SDL_PauseAudioDevice(sdl->speaker_device, true);

#if SDL_DRIVER_MIC_SUPPORT
   if (sdl->microphone)
   {
      /* Stop the microphone independently of whether it's paused;
       * note that upon sdl_audio_start, the microphone won't resume
       * if it was previously paused */
      SDL_PauseAudioDevice(sdl->microphone->device_id, true);
   }
#endif

   return true;
}

static bool sdl_audio_alive(void *data)
{
   sdl_audio_t *sdl = (sdl_audio_t*)data;
   if (!sdl)
      return false;
   return !sdl->is_paused;
}

static bool sdl_audio_start(void *data, bool is_shutdown)
{
   sdl_audio_t *sdl = (sdl_audio_t*)data;
   sdl->is_paused = false;

   SDL_PauseAudioDevice(sdl->speaker_device, false);

#if SDL_DRIVER_MIC_SUPPORT
   if (sdl->microphone)
   {
      if (!sdl->microphone->is_paused)
         SDL_PauseAudioDevice(sdl->microphone->device_id, false);
      /* If the microphone wasn't paused beforehand... */
   }
#endif

   return true;
}

static void sdl_audio_set_nonblock_state(void *data, bool state)
{
   sdl_audio_t *sdl = (sdl_audio_t*)data;
   if (sdl)
      sdl->nonblock = state;
}

static void sdl_audio_free(void *data)
{
   sdl_audio_t *sdl = (sdl_audio_t*)data;

   if (sdl)
   {
      if (sdl->speaker_device > 0)
      {
         SDL_CloseAudioDevice(sdl->speaker_device);
      }

      if (sdl->speaker_buffer)
      {
         fifo_free(sdl->speaker_buffer);
      }

#if SDL_DRIVER_MIC_SUPPORT
      if (sdl->microphone)
         sdl_audio_free_microphone(sdl, sdl->microphone);
#endif

#ifdef HAVE_THREADS
      slock_free(sdl->lock);
      scond_free(sdl->cond);
#endif
   }
   free(sdl);
}

static bool sdl_audio_use_float(void *data)
{
   (void)data;
   return false;
}

static size_t sdl_audio_write_avail(void *data)
{
   /* stub */
   (void)data;
   return 0;
}

#if SDL_DRIVER_MIC_SUPPORT
static void *sdl_audio_init_microphone(void *data,
   const char *device,
   unsigned rate,
   unsigned latency,
   unsigned block_frames,
   unsigned *new_rate)
{
   int frames;
   size_t bufsize;
   sdl_audio_t *sdl                   = (sdl_audio_t*)data;
   sdl_audio_microphone_t *microphone = NULL;
   SDL_AudioSpec desired_spec         = {0};
   SDL_AudioSpec out;
   void *tmp                          = NULL;

   if (!sdl || !SDL_WasInit(SDL_INIT_AUDIO))
   { /* If the audio driver wasn't initialized yet... */
      RARCH_ERR("[SDL audio]: Attempted to initialize input device before initializing the audio driver\n");
      return NULL;
   }

   if (sdl->microphone)
   {
      RARCH_ERR("[SDL audio]: Attempted to initialize a second microphone, but the driver only supports one right now\n");
      return NULL;
   }

   microphone = (sdl_audio_microphone_t *)calloc(1, sizeof(sdl_audio_microphone_t));
   if (!microphone)
      return NULL;

   if (verbosity_is_enabled())
   { /* Only print SDL audio devices if verbose logging is enabled */
      int i;
      int num_available_microphones = SDL_GetNumAudioDevices(true);
      RARCH_DBG("[SDL audio]: %d audio capture devices found:\n", num_available_microphones);
      for (i = 0; i < num_available_microphones; ++i) {
         RARCH_DBG("[SDL audio]:    - %s\n", SDL_GetAudioDeviceName(i, true));
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
      RARCH_ERR("[SDL audio]: Failed to open SDL audio input device: %s\n", SDL_GetError());
      goto error;
   }
   RARCH_DBG("[SDL audio]: Opened SDL audio input device with ID %u\n",
             microphone->device_id);
   RARCH_DBG("[SDL audio]: Requested a microphone frequency of %u Hz, got %u Hz\n",
             desired_spec.freq, out.freq);
   RARCH_DBG("[SDL audio]: Requested %u channels for microphone, got %u\n",
             desired_spec.channels, out.channels);
   RARCH_DBG("[SDL audio]: Requested a %u-sample microphone buffer, got %u samples (%u bytes)\n",
             frames, out.samples, out.size);
   RARCH_DBG("[SDL audio]: Got a microphone silence value of %u\n", out.silence);
   RARCH_DBG("[SDL audio]: Requested microphone audio format: %u-bit %s %s %s endian\n",
             SDL_AUDIO_BITSIZE(desired_spec.format),
             SDL_AUDIO_ISSIGNED(desired_spec.format) ? "signed" : "unsigned",
             SDL_AUDIO_ISFLOAT(desired_spec.format) ? "floating-point" : "integer",
             SDL_AUDIO_ISBIGENDIAN(desired_spec.format) ? "big" : "little");

   RARCH_DBG("[SDL audio]: Received microphone audio format: %u-bit %s %s %s endian\n",
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

static void sdl_audio_free_microphone(void *data, void *microphone_context)
{
   sdl_audio_t *sdl                   = (sdl_audio_t*)data;
   sdl_audio_microphone_t *microphone = (sdl_audio_microphone_t *)microphone_context;

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

static bool sdl_audio_microphone_get_state(const void *data, const void *microphone_context)
{
   const sdl_audio_t *sdl                   = (const sdl_audio_t*)data;
   const sdl_audio_microphone_t *microphone = (const sdl_audio_microphone_t*)microphone_context;

   if (!sdl || !microphone)
      return false;
   /* Both params must be non-null */

   return !microphone->is_paused;
   /* The mic might be paused due to app requirements,
    * or it might be stopped because the entire audio driver is stopped. */
}

static bool sdl_audio_microphone_set_state(void *data, void *microphone_context, bool enabled)
{
   sdl_audio_t *sdl                   = (sdl_audio_t*)data;
   sdl_audio_microphone_t *microphone = (sdl_audio_microphone_t*)microphone_context;

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

static ssize_t sdl_audio_read_microphone(void *data, void *microphone_context, void *buf, size_t size)
{
   ssize_t ret                        = 0;
   sdl_audio_t *sdl                   = (sdl_audio_t*)data;
   sdl_audio_microphone_t *microphone = (sdl_audio_microphone_t*)microphone_context;

   if (!sdl || !microphone || !buf)
      return -1;

   if (sdl->nonblock)
   {
      size_t avail, read_amt;

      SDL_LockAudioDevice(microphone->device_id);
      avail = FIFO_READ_AVAIL(microphone->sample_buffer);
      read_amt = avail > size ? size : avail;
      fifo_read(microphone->sample_buffer, buf, read_amt);
      SDL_UnlockAudioDevice(microphone->device_id);
      ret = read_amt;
   }
   else
   {
      size_t read = 0;

      while (read < size)
      {
         size_t avail;

         SDL_LockAudioDevice(microphone->device_id);
         avail = FIFO_READ_AVAIL(microphone->sample_buffer);

         if (avail == 0)
         {
            SDL_UnlockAudioDevice(microphone->device_id);
#ifdef HAVE_THREADS
            slock_lock(microphone->lock);
            scond_wait(microphone->cond, microphone->lock);
            slock_unlock(microphone->lock);
#endif
         }
         else
         {
            size_t read_amt = size - read > avail ? avail : size - read;
            fifo_read(microphone->sample_buffer, buf + read, read_amt);
            SDL_UnlockAudioDevice(microphone->device_id);
            read += read_amt;
         }
      }
      ret = read;
   }

   return ret / sizeof(int16_t);
   /* Because the function should return the number of *samples* processed, not bytes
    * (which is what the FIFO queues operate on) */
}

#endif

audio_driver_t audio_sdl = {
   sdl_audio_init,
   sdl_audio_write,
   sdl_audio_stop,
   sdl_audio_start,
   sdl_audio_alive,
   sdl_audio_set_nonblock_state,
   sdl_audio_free,
   sdl_audio_use_float,
#ifdef HAVE_SDL2
   "sdl2",
#else
   "sdl",
#endif
   NULL,
   NULL,
   sdl_audio_write_avail,
   NULL,
#if SDL_DRIVER_MIC_SUPPORT
   sdl_audio_init_microphone,
   sdl_audio_free_microphone,
   sdl_audio_microphone_get_state,
   sdl_audio_microphone_set_state,
   sdl_audio_read_microphone
#else
      NULL, /* Microphone support for this driver requires SDL 2 */
      NULL,
      NULL,
      NULL,
      NULL
#endif
};
