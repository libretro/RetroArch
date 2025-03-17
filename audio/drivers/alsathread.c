/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2012-2015 - Michael Lelli
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

#include <stdlib.h>

#include <alsa/asoundlib.h>

#include <rthreads/rthreads.h>
#include <queues/fifo_queue.h>
#include <string/stdstring.h>
#include <asm-generic/errno.h>

#include "../audio_driver.h"
#include "../common/alsa.h" /* For some common functions/types */
#include "../common/alsathread.h"
#include "../../verbosity.h"

typedef struct alsa_thread
{
   alsa_thread_info_t info;
   bool nonblock;
   bool is_paused;
} alsa_thread_t;

static void alsa_worker_thread(void *data)
{
   alsa_thread_t *alsa = (alsa_thread_t*)data;
   uint8_t        *buf = (uint8_t *)calloc(1, alsa->info.stream_info.period_size);
   uintptr_t thread_id = sthread_get_current_thread_id();

   if (!buf)
   {
      RARCH_ERR("[ALSA] [playback thread %u]: Failed to allocate audio buffer\n", thread_id);
      goto end;
   }

   RARCH_DBG("[ALSA] [playback thread %p]: Beginning playback worker thread\n", thread_id);
   while (!alsa->info.thread_dead)
   {
      size_t avail;
      size_t fifo_size;
      snd_pcm_sframes_t frames;
      slock_lock(alsa->info.fifo_lock);
      avail     = FIFO_READ_AVAIL(alsa->info.buffer);
      fifo_size = MIN(alsa->info.stream_info.period_size, avail);
      fifo_read(alsa->info.buffer, buf, fifo_size);
      scond_signal(alsa->info.cond);
      slock_unlock(alsa->info.fifo_lock);

      /* If underrun, fill rest with silence. */
      memset(buf + fifo_size, 0, alsa->info.stream_info.period_size - fifo_size);

      frames = snd_pcm_writei(alsa->info.pcm, buf, alsa->info.stream_info.period_frames);

      if (frames == -EPIPE || frames == -EINTR ||
            frames == -ESTRPIPE)
      {
         if (snd_pcm_recover(alsa->info.pcm, frames, false) < 0)
         {
            RARCH_ERR("[ALSA] [playback thread %u]: Failed to recover from error: %s\n",
               thread_id,
               snd_strerror(frames));
            break;
         }

         continue;
      }
      else if (frames < 0)
      {
         RARCH_ERR("[ALSA] [playback thread %u]: Error writing audio to device: %s\n",
            thread_id,
            snd_strerror(frames));
         break;
      }
   }

end:
   slock_lock(alsa->info.cond_lock);
   alsa->info.thread_dead = true;
   scond_signal(alsa->info.cond);
   slock_unlock(alsa->info.cond_lock);
   free(buf);
   RARCH_DBG("[ALSA] [playback thread %p]: Ending playback worker thread\n", thread_id);
}

static bool alsa_thread_use_float(void *data)
{
   alsa_thread_t *alsa = (alsa_thread_t*)data;
   return alsa->info.stream_info.has_float;
}

static void alsa_thread_free(void *data)
{
   alsa_thread_t *alsa = (alsa_thread_t*)data;

   if (alsa)
   {
      alsa_thread_free_info_members(&alsa->info);
      free(alsa);
   }
}

static void *alsa_thread_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   alsa_thread_t *alsa = (alsa_thread_t*)calloc(1, sizeof(alsa_thread_t));

   if (!alsa)
   {
      RARCH_ERR("[ALSA] Failed to allocate driver context\n");
      return NULL;
   }

   RARCH_LOG("[ALSA] Using ALSA version %s\n", snd_asoundlib_version());

   if (alsa_init_pcm(&alsa->info.pcm, device, SND_PCM_STREAM_PLAYBACK, rate, latency, 2, &alsa->info.stream_info, new_rate, 0) < 0)
   {
      goto error;
   }

   alsa->info.fifo_lock = slock_new();
   alsa->info.cond_lock = slock_new();
   alsa->info.cond = scond_new();
   alsa->info.buffer = fifo_new(alsa->info.stream_info.buffer_size);
   if (!alsa->info.fifo_lock || !alsa->info.cond_lock || !alsa->info.cond || !alsa->info.buffer)
      goto error;

   alsa->info.worker_thread = sthread_create(alsa_worker_thread, alsa);
   if (!alsa->info.worker_thread)
   {
      RARCH_ERR("[ALSA]: Failed to initialize worker thread\n");
      goto error;
   }

   return alsa;

error:
   RARCH_ERR("[ALSA]: Failed to initialize...\n");

   alsa_thread_free(alsa);

   return NULL;
}

static ssize_t alsa_thread_write(void *data, const void *s, size_t len)
{
   ssize_t written = 0;
   alsa_thread_t *alsa = (alsa_thread_t*)data;

   if (alsa->info.thread_dead)
      return -1;

   if (alsa->nonblock)
   {
      size_t avail;

      slock_lock(alsa->info.fifo_lock);
      avail           = FIFO_WRITE_AVAIL(alsa->info.buffer);
      written         = MIN(avail, len);

      fifo_write(alsa->info.buffer, s, written);
      slock_unlock(alsa->info.fifo_lock);
   }
   else
   {
      while (written < (ssize_t)len && !alsa->info.thread_dead)
      {
         size_t avail;
         slock_lock(alsa->info.fifo_lock);
         avail = FIFO_WRITE_AVAIL(alsa->info.buffer);

         if (avail == 0)
         {
            slock_unlock(alsa->info.fifo_lock);
            slock_lock(alsa->info.cond_lock);
            if (!alsa->info.thread_dead)
               scond_wait(alsa->info.cond, alsa->info.cond_lock);
            slock_unlock(alsa->info.cond_lock);
         }
         else
         {
            size_t write_amt = MIN(len - written, avail);
            fifo_write(alsa->info.buffer,
                  (const char*)s + written, write_amt);
            slock_unlock(alsa->info.fifo_lock);
            written += write_amt;
         }
      }
   }
   return written;
}

static bool alsa_thread_alive(void *data)
{
   alsa_thread_t *alsa = (alsa_thread_t*)data;
   if (!alsa)
      return false;
   return !alsa->is_paused;
}

static bool alsa_thread_stop(void *data)
{
   alsa_thread_t *alsa = (alsa_thread_t*)data;

   if (alsa)
      alsa->is_paused = true;
   return true;
}

static void alsa_thread_set_nonblock_state(void *data, bool state)
{
   alsa_thread_t *alsa = (alsa_thread_t*)data;
   alsa->nonblock = state;
}

static bool alsa_thread_start(void *data, bool is_shutdown)
{
   alsa_thread_t *alsa = (alsa_thread_t*)data;

   if (alsa)
      alsa->is_paused = false;
   return true;
}

static size_t alsa_thread_write_avail(void *data)
{
   alsa_thread_t *alsa = (alsa_thread_t*)data;
   size_t val;

   if (alsa->info.thread_dead)
      return 0;
   slock_lock(alsa->info.fifo_lock);
   val = FIFO_WRITE_AVAIL(alsa->info.buffer);
   slock_unlock(alsa->info.fifo_lock);
   return val;
}

static size_t alsa_thread_buffer_size(void *data)
{
   alsa_thread_t *alsa = (alsa_thread_t*)data;
   return alsa->info.stream_info.buffer_size;
}

audio_driver_t audio_alsathread = {
   alsa_thread_init,
   alsa_thread_write,
   alsa_thread_stop,
   alsa_thread_start,
   alsa_thread_alive,
   alsa_thread_set_nonblock_state,
   alsa_thread_free,
   alsa_thread_use_float,
   "alsathread",
   alsa_device_list_new, /* Reusing these functions from alsa.c */
   alsa_device_list_free, /* because they don't use the driver context */
   alsa_thread_write_avail,
   alsa_thread_buffer_size
};
