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

#include "../../retroarch.h"
#include "../../verbosity.h"

#include "SDL.h"
#include "SDL_audio.h"

typedef struct sdl_audio
{
   bool nonblock;
   bool is_paused;

#ifdef HAVE_THREADS
   slock_t *lock;
   scond_t *cond;
#endif
   fifo_buffer_t *buffer;
} sdl_audio_t;

static void sdl_audio_cb(void *data, Uint8 *stream, int len)
{
   sdl_audio_t  *sdl = (sdl_audio_t*)data;
   size_t      avail = fifo_read_avail(sdl->buffer);
   size_t write_size = len > (int)avail ? avail : len;

   fifo_read(sdl->buffer, stream, write_size);
#ifdef HAVE_THREADS
   scond_signal(sdl->cond);
#endif

   /* If underrun, fill rest with silence. */
   memset(stream + write_size, 0, len - write_size);
}

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
   SDL_AudioSpec spec   = {0};
   void            *tmp = NULL;
   sdl_audio_t *sdl     = NULL;

   (void)device;

   if (SDL_WasInit(0) == 0)
   {
      if (SDL_Init(SDL_INIT_AUDIO) < 0)
         return NULL;
   }
   else if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
      return NULL;

   sdl = (sdl_audio_t*)calloc(1, sizeof(*sdl));
   if (!sdl)
      return NULL;

   /* We have to buffer up some data ourselves, so we let SDL
    * carry approximately half of the latency.
    *
    * SDL double buffers audio and we do as well. */
   frames        = find_num_frames(rate, latency / 4);

   spec.freq     = rate;
   spec.format   = AUDIO_S16SYS;
   spec.channels = 2;
   spec.samples  = frames; /* This is in audio frames, not samples ... :( */
   spec.callback = sdl_audio_cb;
   spec.userdata = sdl;

   if (SDL_OpenAudio(&spec, &out) < 0)
   {
      RARCH_ERR("[SDL audio]: Failed to open SDL audio: %s\n", SDL_GetError());
      goto error;
   }

   *new_rate                = out.freq;

#ifdef HAVE_THREADS
   sdl->lock                = slock_new();
   sdl->cond                = scond_new();
#endif

   RARCH_LOG("[SDL audio]: Requested %u ms latency, got %d ms\n",
         latency, (int)(out.samples * 4 * 1000 / (*new_rate)));

   /* Create a buffer twice as big as needed and prefill the buffer. */
   bufsize     = out.samples * 4 * sizeof(int16_t);
   tmp         = calloc(1, bufsize);
   sdl->buffer = fifo_new(bufsize);

   if (tmp)
   {
      fifo_write(sdl->buffer, tmp, bufsize);
      free(tmp);
   }

   SDL_PauseAudio(0);
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

      SDL_LockAudio();
      avail = fifo_write_avail(sdl->buffer);
      write_amt = avail > size ? size : avail;
      fifo_write(sdl->buffer, buf, write_amt);
      SDL_UnlockAudio();
      ret = write_amt;
   }
   else
   {
      size_t written = 0;

      while (written < size)
      {
         size_t avail;

         SDL_LockAudio();
         avail = fifo_write_avail(sdl->buffer);

         if (avail == 0)
         {
            SDL_UnlockAudio();
#ifdef HAVE_THREADS
            slock_lock(sdl->lock);
            scond_wait(sdl->cond, sdl->lock);
            slock_unlock(sdl->lock);
#endif
         }
         else
         {
            size_t write_amt = size - written > avail ? avail : size - written;
            fifo_write(sdl->buffer, (const char*)buf + written, write_amt);
            SDL_UnlockAudio();
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
   SDL_PauseAudio(1);
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

   SDL_PauseAudio(0);
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

   SDL_CloseAudio();
   SDL_QuitSubSystem(SDL_INIT_AUDIO);

   if (sdl)
   {
      fifo_free(sdl->buffer);
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
