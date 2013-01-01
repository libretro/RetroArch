/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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


#include "../driver.h"
#include <stdlib.h>
#include "../boolean.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "SDL.h"
#include "SDL_audio.h"
#include "../thread.h"

#include "../general.h"
#include "../fifo_buffer.h"

typedef struct sdl_audio
{
   bool nonblock;

   slock_t *lock;
   scond_t *cond;
   fifo_buffer_t *buffer;
} sdl_audio_t;

static void sdl_audio_cb(void *data, Uint8 *stream, int len)
{
   sdl_audio_t *sdl = (sdl_audio_t*)data;

   size_t avail = fifo_read_avail(sdl->buffer);
   size_t write_size = len > (int)avail ? avail : len;
   fifo_read(sdl->buffer, stream, write_size);
   scond_signal(sdl->cond);

   // If underrun, fill rest with silence.
   memset(stream + write_size, 0, len - write_size);
}

static inline int find_num_frames(int rate, int latency)
{
   int frames = (rate * latency) / 1000;
   // SDL only likes 2^n sized buffers.
   return next_pow2(frames);
}

static void *sdl_audio_init(const char *device, unsigned rate, unsigned latency)
{
   (void)device;
   if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
      return NULL;

   sdl_audio_t *sdl = (sdl_audio_t*)calloc(1, sizeof(*sdl));
   if (!sdl)
      return NULL;

   // We have to buffer up some data ourselves, so we let SDL carry approx half of the latency. SDL double buffers audio and we do as well.
   int frames = find_num_frames(rate, latency / 4);

   SDL_AudioSpec spec = {0};
   spec.freq = rate;
   spec.format = AUDIO_S16SYS;
   spec.channels = 2;
   spec.samples = frames; // This is in audio frames, not samples ... :(
   spec.callback = sdl_audio_cb;
   spec.userdata = sdl;

   SDL_AudioSpec out;

   if (SDL_OpenAudio(&spec, &out) < 0)
   {
      RARCH_ERR("Failed to open SDL audio: %s\n", SDL_GetError());
      free(sdl);
      return 0;
   }
   g_settings.audio.out_rate = out.freq;

   sdl->lock = slock_new();
   sdl->cond = scond_new();

   RARCH_LOG("SDL audio: Requested %d ms latency, got %d ms\n", latency, (int)(out.samples * 4 * 1000 / g_settings.audio.out_rate));

   // Create a buffer twice as big as needed and prefill the buffer.
   size_t bufsize = out.samples * 4 * sizeof(int16_t);
   void *tmp = calloc(1, bufsize);
   sdl->buffer = fifo_new(bufsize);
   if (tmp)
   {
      fifo_write(sdl->buffer, tmp, bufsize);
      free(tmp);
   }

   SDL_PauseAudio(0);
   return sdl;
}

static ssize_t sdl_audio_write(void *data, const void *buf, size_t size)
{
   sdl_audio_t *sdl = (sdl_audio_t*)data;

   ssize_t ret = 0;
   if (sdl->nonblock)
   {
      SDL_LockAudio();
      size_t avail = fifo_write_avail(sdl->buffer);
      size_t write_amt = avail > size ? size : avail;
      fifo_write(sdl->buffer, buf, write_amt);
      SDL_UnlockAudio();
      ret = write_amt;
   }
   else
   {
      size_t written = 0;
      while (written < size)
      {
         SDL_LockAudio();
         size_t avail = fifo_write_avail(sdl->buffer);

         if (avail == 0)
         {
            SDL_UnlockAudio();
            slock_lock(sdl->lock);
            scond_wait(sdl->cond, sdl->lock);
            slock_unlock(sdl->lock);
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
   (void)data;
   SDL_PauseAudio(1);
   return true;
}

static bool sdl_audio_start(void *data)
{
   (void)data;
   SDL_PauseAudio(0);
   return true;
}

static void sdl_audio_set_nonblock_state(void *data, bool state)
{
   sdl_audio_t *sdl = (sdl_audio_t*)data;
   sdl->nonblock = state;
}

static void sdl_audio_free(void *data)
{
   SDL_CloseAudio();
   SDL_QuitSubSystem(SDL_INIT_AUDIO);

   sdl_audio_t *sdl = (sdl_audio_t*)data;
   if (sdl)
   {
      fifo_free(sdl->buffer);
      slock_free(sdl->lock);
      scond_free(sdl->cond);
   }
   free(sdl);
}

const audio_driver_t audio_sdl = {
   sdl_audio_init,
   sdl_audio_write,
   sdl_audio_stop,
   sdl_audio_start,
   sdl_audio_set_nonblock_state,
   sdl_audio_free,
   NULL,
   "sdl"
};
   
