/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2012-2013 - Michael Lelli
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
#ifdef PANDORA
#include <alsa/asoundlib.h>
#else
#include <asoundlib.h>
#endif
#include "../general.h"
#include "../thread.h"
#include "../fifo_buffer.h"

#define TRY_ALSA(x) if (x < 0) { \
                  goto error; \
               }

typedef struct alsa_thread
{
   snd_pcm_t *pcm;
   bool nonblock;
   bool has_float;
   volatile bool thread_dead;

   size_t buffer_size;
   size_t period_size;
   snd_pcm_uframes_t period_frames;

   fifo_buffer_t *buffer;
   sthread_t *worker_thread;
   slock_t *fifo_lock;
   scond_t *cond;
   slock_t *cond_lock;
} alsa_thread_t;

static void alsa_worker_thread(void *data)
{
   alsa_thread_t *alsa = (alsa_thread_t*)data;

   uint8_t *buf = (uint8_t *)calloc(1, alsa->period_size);
   if (!buf)
   {
      RARCH_ERR("failed to allocate audio buffer");
      goto end;
   }

   while (!alsa->thread_dead)
   {
      slock_lock(alsa->fifo_lock);
      size_t avail = fifo_read_avail(alsa->buffer);
      size_t fifo_size = min(alsa->period_size, avail);
      fifo_read(alsa->buffer, buf, fifo_size);
      scond_signal(alsa->cond);
      slock_unlock(alsa->fifo_lock);

      // If underrun, fill rest with silence.
      memset(buf + fifo_size, 0, alsa->period_size - fifo_size);

      snd_pcm_sframes_t frames = snd_pcm_writei(alsa->pcm, buf, alsa->period_frames);

      if (frames == -EPIPE || frames == -EINTR || frames == -ESTRPIPE)
      {
         if (snd_pcm_recover(alsa->pcm, frames, 1) < 0)
         {
            RARCH_ERR("[ALSA]: (#2) Failed to recover from error (%s)\n",
                  snd_strerror(frames));
            break;
         }

         continue;
      }
      else if (frames < 0)
      {
         RARCH_ERR("[ALSA]: Unknown error occured (%s).\n", snd_strerror(frames));
         break;
      }
   }

end:
   slock_lock(alsa->cond_lock);
   alsa->thread_dead = true;
   scond_signal(alsa->cond);
   slock_unlock(alsa->cond_lock);
   free(buf);
}

static bool alsa_thread_use_float(void *data)
{
   alsa_thread_t *alsa = (alsa_thread_t*)data;
   return alsa->has_float;
}

static bool alsathread_find_float_format(snd_pcm_t *pcm, snd_pcm_hw_params_t *params)
{
   if (snd_pcm_hw_params_test_format(pcm, params, SND_PCM_FORMAT_FLOAT) == 0)
   {
      RARCH_LOG("ALSA: Using floating point format.\n");
      return true;
   }
   RARCH_LOG("ALSA: Using signed 16-bit format.\n");
   return false;
}

static void alsa_thread_free(void *data)
{
   alsa_thread_t *alsa = (alsa_thread_t*)data;

   if (alsa)
   {
      if (alsa->worker_thread)
      {
         alsa->thread_dead = true;
         sthread_join(alsa->worker_thread);
      }
      if (alsa->buffer)
         fifo_free(alsa->buffer);
      if (alsa->cond)
         scond_free(alsa->cond);
      if (alsa->fifo_lock)
         slock_free(alsa->fifo_lock);
      if (alsa->cond_lock)
         slock_free(alsa->cond_lock);
      if (alsa->pcm)
      {
         snd_pcm_drop(alsa->pcm);
         snd_pcm_close(alsa->pcm);
      }
      free(alsa);
   }
}

static void *alsa_thread_init(const char *device, unsigned rate, unsigned latency)
{
   alsa_thread_t *alsa = (alsa_thread_t*)calloc(1, sizeof(alsa_thread_t));
   if (!alsa)
      return NULL;

   snd_pcm_hw_params_t *params = NULL;
   snd_pcm_sw_params_t *sw_params = NULL;

   const char *alsa_dev = "default";
   if (device)
      alsa_dev = device;

   unsigned latency_usec = latency * 1000 / 2;

   unsigned channels = 2;
   unsigned periods = 4;
   snd_pcm_uframes_t buffer_size;
   snd_pcm_format_t format;

   TRY_ALSA(snd_pcm_open(&alsa->pcm, alsa_dev, SND_PCM_STREAM_PLAYBACK, 0));

   TRY_ALSA(snd_pcm_hw_params_malloc(&params));
   alsa->has_float = alsathread_find_float_format(alsa->pcm, params);
   format = alsa->has_float ? SND_PCM_FORMAT_FLOAT : SND_PCM_FORMAT_S16;

   TRY_ALSA(snd_pcm_hw_params_any(alsa->pcm, params));
   TRY_ALSA(snd_pcm_hw_params_set_access(alsa->pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED));
   TRY_ALSA(snd_pcm_hw_params_set_format(alsa->pcm, params, format));
   TRY_ALSA(snd_pcm_hw_params_set_channels(alsa->pcm, params, channels));
   TRY_ALSA(snd_pcm_hw_params_set_rate(alsa->pcm, params, rate, 0));

   TRY_ALSA(snd_pcm_hw_params_set_buffer_time_near(alsa->pcm, params, &latency_usec, NULL));
   TRY_ALSA(snd_pcm_hw_params_set_periods_near(alsa->pcm, params, &periods, NULL));

   TRY_ALSA(snd_pcm_hw_params(alsa->pcm, params));

   snd_pcm_hw_params_get_period_size(params, &alsa->period_frames, NULL);
   RARCH_LOG("ALSA: Period size: %d frames\n", (int)alsa->period_frames);
   snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
   RARCH_LOG("ALSA: Buffer size: %d frames\n", (int)buffer_size);
   alsa->buffer_size = snd_pcm_frames_to_bytes(alsa->pcm, buffer_size);
   alsa->period_size = snd_pcm_frames_to_bytes(alsa->pcm, alsa->period_frames);

   TRY_ALSA(snd_pcm_sw_params_malloc(&sw_params));
   TRY_ALSA(snd_pcm_sw_params_current(alsa->pcm, sw_params));
   TRY_ALSA(snd_pcm_sw_params_set_start_threshold(alsa->pcm, sw_params, buffer_size / 2));
   TRY_ALSA(snd_pcm_sw_params(alsa->pcm, sw_params));

   snd_pcm_hw_params_free(params);
   snd_pcm_sw_params_free(sw_params);

   alsa->fifo_lock = slock_new();
   alsa->cond_lock = slock_new();
   alsa->cond = scond_new();
   alsa->buffer = fifo_new(alsa->buffer_size);
   if (!alsa->fifo_lock || !alsa->cond_lock || !alsa->cond || !alsa->buffer)
      goto error;

   alsa->worker_thread = sthread_create(alsa_worker_thread, alsa);
   if (!alsa->worker_thread)
   {
      RARCH_ERR("error initializing worker thread");
      goto error;
   }

   return alsa;

error:
   RARCH_ERR("ALSA: Failed to initialize...\n");
   if (params)
      snd_pcm_hw_params_free(params);

   if (sw_params)
      snd_pcm_sw_params_free(sw_params);

   alsa_thread_free(alsa);

   return NULL;
}

static ssize_t alsa_thread_write(void *data, const void *buf, size_t size)
{
   alsa_thread_t *alsa = (alsa_thread_t*)data;

   if (alsa->thread_dead)
      return -1;

   if (alsa->nonblock)
   {
      slock_lock(alsa->fifo_lock);
      size_t avail = fifo_write_avail(alsa->buffer);
      size_t write_amt = min(avail, size);
      fifo_write(alsa->buffer, buf, write_amt);
      slock_unlock(alsa->fifo_lock);
      return write_amt;
   }
   else
   {
      size_t written = 0;
      while (written < size && !alsa->thread_dead)
      {
         slock_lock(alsa->fifo_lock);
         size_t avail = fifo_write_avail(alsa->buffer);

         if (avail == 0)
         {
            slock_unlock(alsa->fifo_lock);
            slock_lock(alsa->cond_lock);
            if (!alsa->thread_dead)
               scond_wait(alsa->cond, alsa->cond_lock);
            slock_unlock(alsa->cond_lock);
         }
         else
         {
            size_t write_amt = min(size - written, avail);
            fifo_write(alsa->buffer, (const char*)buf + written, write_amt);
            slock_unlock(alsa->fifo_lock);
            written += write_amt;
         }
      }
      return written;
   }
}

static bool alsa_thread_stop(void *data)
{
   (void)data;
   return true;
}

static void alsa_thread_set_nonblock_state(void *data, bool state)
{
   alsa_thread_t *alsa = (alsa_thread_t*)data;
   alsa->nonblock = state;
}

static bool alsa_thread_start(void *data)
{
   (void)data;
   return true;
}

static size_t alsa_thread_write_avail(void *data)
{
   alsa_thread_t *alsa = (alsa_thread_t*)data;

   if (alsa->thread_dead)
      return 0;
   slock_lock(alsa->fifo_lock);
   size_t val = fifo_write_avail(alsa->buffer);
   slock_unlock(alsa->fifo_lock);
   return val;
}

static size_t alsa_thread_buffer_size(void *data)
{
   alsa_thread_t *alsa = (alsa_thread_t*)data;
   return alsa->buffer_size;
}

const audio_driver_t audio_alsathread = {
   alsa_thread_init,
   alsa_thread_write,
   alsa_thread_stop,
   alsa_thread_start,
   alsa_thread_set_nonblock_state,
   alsa_thread_free,
   alsa_thread_use_float,
   "alsathread",
   alsa_thread_write_avail,
   alsa_thread_buffer_size,
};
