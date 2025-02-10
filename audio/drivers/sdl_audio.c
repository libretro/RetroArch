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

#ifndef HAVE_SDL2
typedef Uint32 SDL_AudioDeviceID;

/** Compatibility stub that defers to SDL_PauseAudio. */
#define SDL_PauseAudioDevice(dev, pause_on) SDL_PauseAudio(pause_on)

/** Compatibility stub that defers to SDL_LockAudio. */
#define SDL_LockAudioDevice(dev) SDL_LockAudio()

/** Compatibility stub that defers to SDL_UnlockAudio. */
#define SDL_UnlockAudioDevice(dev) SDL_UnlockAudio()

/** Compatibility stub that defers to SDL_CloseAudio. */
#define SDL_CloseAudioDevice(dev) SDL_CloseAudio()

/* Macros for checking audio format bits that were introduced in SDL 2 */
#define SDL_AUDIO_MASK_BITSIZE       (0xFF)
#define SDL_AUDIO_MASK_DATATYPE      (1<<8)
#define SDL_AUDIO_MASK_ENDIAN        (1<<12)
#define SDL_AUDIO_MASK_SIGNED        (1<<15)
#define SDL_AUDIO_BITSIZE(x)         (x & SDL_AUDIO_MASK_BITSIZE)
#define SDL_AUDIO_ISFLOAT(x)         (x & SDL_AUDIO_MASK_DATATYPE)
#define SDL_AUDIO_ISBIGENDIAN(x)     (x & SDL_AUDIO_MASK_ENDIAN)
#define SDL_AUDIO_ISSIGNED(x)        (x & SDL_AUDIO_MASK_SIGNED)
#define SDL_AUDIO_ISINT(x)           (!SDL_AUDIO_ISFLOAT(x))
#define SDL_AUDIO_ISLITTLEENDIAN(x)  (!SDL_AUDIO_ISBIGENDIAN(x))
#define SDL_AUDIO_ISUNSIGNED(x)      (!SDL_AUDIO_ISSIGNED(x))
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
} sdl_audio_t;

static void sdl_audio_playback_cb(void *data, Uint8 *stream, int len)
{
   sdl_audio_t  *sdl = (sdl_audio_t*)data;
   size_t      avail = FIFO_READ_AVAIL(sdl->speaker_buffer);
   size_t       _len = (len > (int)avail) ? avail : (size_t)len;
   fifo_read(sdl->speaker_buffer, stream, _len);
#ifdef HAVE_THREADS
   scond_signal(sdl->cond);
#endif
   /* If underrun, fill rest with silence. */
   memset(stream + _len, 0, len - _len);
}

static INLINE int sdl_audio_find_num_frames(int rate, int latency)
{
   int frames = (rate * latency) / 1000;
   /* SDL only likes 2^n sized buffers. */
   return next_pow2(frames);
}

static void *sdl_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames, unsigned *new_rate)
{
   int frames;
   size_t bufsize;
   SDL_AudioSpec out;
   SDL_AudioSpec spec           = {0};
   void *tmp                    = NULL;
   sdl_audio_t *sdl             = NULL;
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

   sdl = (sdl_audio_t*)calloc(1, sizeof(*sdl));
   if (!sdl)
      return NULL;

   /* We have to buffer up some data ourselves, so we let SDL
    * carry approximately half of the latency.
    *
    * SDL double buffers audio and we do as well. */
   frames        = sdl_audio_find_num_frames(rate, latency / 4);

   /* First, let's initialize the output device. */
   spec.freq     = rate;
   spec.format   = AUDIO_S16SYS;
   spec.channels = 2;
   spec.samples  = frames; /* This is in audio frames, not samples ... :( */
   spec.callback = sdl_audio_playback_cb;
   spec.userdata = sdl;

   /* No compatibility stub for SDL_OpenAudioDevice because its return value
    * is different from that of SDL_OpenAudio. */
#ifdef HAVE_SDL2
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
   RARCH_DBG("[SDL audio]: Opened SDL audio out device with ID %u\n",
             sdl->speaker_device);
   RARCH_DBG("[SDL audio]: Requested a speaker frequency of %u Hz, got %u Hz\n",
             spec.freq, out.freq);
   RARCH_DBG("[SDL audio]: Requested %u channels for speaker, got %u\n",
             spec.channels, out.channels);
   RARCH_DBG("[SDL audio]: Requested a %u-frame speaker buffer, got %u frames (%u bytes)\n",
             frames, out.samples, out.size);
   RARCH_DBG("[SDL audio]: Got a speaker silence value of %u\n", out.silence);
   RARCH_DBG("[SDL audio]: Requested speaker audio format: %u-bit %s %s %s endian\n",
             SDL_AUDIO_BITSIZE(spec.format),
             SDL_AUDIO_ISSIGNED(spec.format) ? "signed" : "unsigned",
             SDL_AUDIO_ISFLOAT(spec.format) ? "floating-point" : "integer",
             SDL_AUDIO_ISBIGENDIAN(spec.format) ? "big" : "little");

   RARCH_DBG("[SDL audio]: Received speaker audio format: %u-bit %s %s %s endian\n",
             SDL_AUDIO_BITSIZE(spec.format),
             SDL_AUDIO_ISSIGNED(spec.format) ? "signed" : "unsigned",
             SDL_AUDIO_ISFLOAT(spec.format) ? "floating-point" : "integer",
             SDL_AUDIO_ISBIGENDIAN(spec.format) ? "big" : "little");

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

   RARCH_DBG("[SDL audio]: Initialized speaker sample queue with %u bytes\n", bufsize);

   SDL_PauseAudioDevice(sdl->speaker_device, false);

   return sdl;

error:
   free(sdl);
   return NULL;
}

static ssize_t sdl_audio_write(void *data, const void *s, size_t len)
{
   ssize_t ret      = 0;
   sdl_audio_t *sdl = (sdl_audio_t*)data;

   /* If we shouldn't wait for space in a full outgoing sample queue... */
   if (sdl->nonblock)
   {
      size_t avail, write_amt;
      SDL_LockAudioDevice(sdl->speaker_device); /* Stop the SDL speaker thread from running */
      avail     = FIFO_WRITE_AVAIL(sdl->speaker_buffer);
      write_amt = (avail > len) ? len : avail; /* Enqueue as much data as we can */
      fifo_write(sdl->speaker_buffer, s, write_amt);
      SDL_UnlockAudioDevice(sdl->speaker_device); /* Let the speaker thread run again */
      ret       = write_amt; /* If the queue was full...well, too bad. */
   }
   else
   {
      size_t written = 0;
      /* Until we've written all the sample data we have available... */
      while (written < len)
      {
         size_t avail;

         /* Stop the SDL speaker thread from running */
         SDL_LockAudioDevice(sdl->speaker_device);
         avail = FIFO_WRITE_AVAIL(sdl->speaker_buffer);

         /* If the outgoing sample queue is full... */
         if (avail == 0)
         {
            SDL_UnlockAudioDevice(sdl->speaker_device);
            /* Let the SDL speaker thread run so it can play the enqueued samples,
             * which will free up space for us to write new ones. */
#ifdef HAVE_THREADS
            slock_lock(sdl->lock);
            /* Let *only* the SDL speaker thread touch the outgoing sample queue */
            scond_wait(sdl->cond, sdl->lock);
            /* Block until SDL tells us that it's made room for new samples */
            slock_unlock(sdl->lock);
            /* Now let this thread use the outgoing sample queue (which we'll do next iteration) */
#endif
         }
         else
         {
            size_t write_amt = len - written > avail ? avail : len - written;
            fifo_write(sdl->speaker_buffer, (const char*)s + written, write_amt);
            /* Enqueue as many samples as we have available without overflowing the queue */
            SDL_UnlockAudioDevice(sdl->speaker_device); /* Let the SDL speaker thread run again */
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
   sdl->is_paused   = true;
   SDL_PauseAudioDevice(sdl->speaker_device, true);
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
   sdl->is_paused   = false;
   SDL_PauseAudioDevice(sdl->speaker_device, false);
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
         fifo_free(sdl->speaker_buffer);

#ifdef HAVE_THREADS
      slock_free(sdl->lock);
      scond_free(sdl->cond);
#endif

      SDL_QuitSubSystem(SDL_INIT_AUDIO);
   }
   free(sdl);
}

/* TODO/FIXME - implement */
static bool sdl_audio_use_float(void *data) { return false; }
static size_t sdl_audio_write_avail(void *data) { return 0; }

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
   NULL
};
