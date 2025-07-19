/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2023-2025 - Jesse Talavera-Greenberg
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
#include <boolean.h>

#include <alsa/asoundlib.h>
#include <alsa/pcm.h>

#include <rthreads/rthreads.h>
#include <queues/fifo_queue.h>
#include <string/stdstring.h>
#include <asm-generic/errno.h>

#include <retro_assert.h>

#include "../audio_driver.h"
#include "../common/alsa.h" /* For some common functions/types */
#include "../../verbosity.h"

typedef struct alsa_thread_info
{
   snd_pcm_t *pcm;
   fifo_buffer_t *buffer;
   sthread_t *worker_thread;
   slock_t *fifo_lock;
   scond_t *cond;
   slock_t *cond_lock;
   alsa_stream_info_t stream_info;
   volatile bool thread_dead;
} alsa_thread_info_t;

static void alsa_thread_free_info_members(alsa_thread_info_t *info)
{
   if (info)
   {
      if (info->worker_thread)
      {
         slock_lock(info->cond_lock);
         info->thread_dead = true;
         slock_unlock(info->cond_lock);
         sthread_join(info->worker_thread);
      }
      if (info->buffer)
         fifo_free(info->buffer);
      if (info->cond)
         scond_free(info->cond);
      if (info->fifo_lock)
         slock_free(info->fifo_lock);
      if (info->cond_lock)
         slock_free(info->cond_lock);
      if (info->pcm)
         alsa_free_pcm(info->pcm);
   }
   /* Do NOT free() info itself; it's embedded within another struct
    * that will be freed. */
}

#ifdef HAVE_MICROPHONE
#include "../microphone_driver.h"

typedef struct alsa_thread_microphone_handle
{
   alsa_thread_info_t info;
} alsa_thread_microphone_handle_t;

typedef struct alsa_thread_microphone
{
   bool nonblock;
} alsa_thread_microphone_t;

static void *alsa_thread_microphone_init(void)
{
   alsa_thread_microphone_t *alsa = (alsa_thread_microphone_t*)calloc(1, sizeof(alsa_thread_microphone_t));

   if (!alsa)
   {
      RARCH_ERR("[ALSA] Failed to allocate driver context.\n");
      return NULL;
   }

   RARCH_LOG("[ALSA] Using ALSA version %s\n", snd_asoundlib_version());

   return alsa;
}

/* Forward declaration */
static void alsa_thread_microphone_close_mic(void *driver_context, void *mic_context);

static void alsa_thread_microphone_free(void *driver_context)
{
   alsa_thread_microphone_t *alsa = (alsa_thread_microphone_t*)driver_context;

   if (alsa)
      free(alsa);
}

/** @see alsa_thread_read_microphone() */
static void alsa_microphone_worker_thread(void *mic_context)
{
   alsa_thread_microphone_handle_t *mic = (alsa_thread_microphone_handle_t*)mic_context;
   uint8_t                         *buf = NULL;
   uintptr_t                  thread_id = sthread_get_current_thread_id();

   retro_assert(mic != NULL);

   if (!(buf = (uint8_t *)calloc(1, mic->info.stream_info.period_size)))
   {
      RARCH_ERR("[ALSA] [capture thread %p] Failed to allocate audio buffer.\n", thread_id);
      goto end;
   }

   RARCH_DBG("[ALSA] [capture thread %p] Beginning microphone worker thread.\n", thread_id);
   RARCH_DBG("[ALSA] [capture thread %p] Microphone \"%s\" is in state %s.\n",
             thread_id,
             snd_pcm_name(mic->info.pcm),
             snd_pcm_state_name(snd_pcm_state(mic->info.pcm)));

   /* Until we're told to stop... */
   while (!mic->info.thread_dead)
   {
      size_t avail;
      size_t fifo_size;
      snd_pcm_sframes_t frames;
      int errnum = 0;

      /* Lock the incoming sample queue (the main thread may block) */
      slock_lock(mic->info.fifo_lock);

      /* Fill the incoming sample queue with whatever we recently read */
      avail     = FIFO_WRITE_AVAIL(mic->info.buffer);
      fifo_size = MIN(mic->info.stream_info.period_size, avail);
      fifo_write(mic->info.buffer, buf, fifo_size);

      /* Tell the main thread that it's okay to query the mic again */
      scond_signal(mic->info.cond);

      /* Unlock the incoming sample queue (the main thread may resume) */
      slock_unlock(mic->info.fifo_lock);

      /* If underrun, fill rest with silence. */
      memset(buf + fifo_size, 0, mic->info.stream_info.period_size - fifo_size);

      errnum = snd_pcm_wait(mic->info.pcm, 33);

      if (errnum == 0)
      {
         RARCH_DBG("[ALSA] [capture thread %p] Timeout after 33ms waiting for input.\n", thread_id);
         continue;
      }
      else if (errnum == -EPIPE || errnum == -ESTRPIPE || errnum == -EINTR)
      {
         RARCH_WARN("[ALSA] [capture thread %p] Wait error: %s.\n",
                    thread_id,
                    snd_strerror(errnum));

         if ((errnum = snd_pcm_recover(mic->info.pcm, errnum, false)) < 0)
         {
            RARCH_ERR("[ALSA] [capture thread %p] Failed to recover from prior wait error: %s.\n",
                      thread_id,
                      snd_strerror(errnum));

            break;
         }

         continue;
      }

      frames = snd_pcm_readi(mic->info.pcm, buf, mic->info.stream_info.period_frames);

      if (frames == -EPIPE || frames == -EINTR || frames == -ESTRPIPE)
      {
         RARCH_WARN("[ALSA] [capture thread %p] Read error: %s.\n",
                    thread_id,
                    snd_strerror(frames));

         if ((errnum = snd_pcm_recover(mic->info.pcm, frames, false)) < 0)
         {
            RARCH_ERR("[ALSA] [capture thread %p] Failed to recover from prior read error: %s.\n",
                      thread_id,
                      snd_strerror(errnum));
            break;
         }

         continue;
      }
      else if (frames < 0)
      {
         RARCH_ERR("[ALSA] [capture thread %p] Read error: %s.\n",
                   thread_id,
                   snd_strerror(frames));
         break;
      }
   }

end:
   slock_lock(mic->info.cond_lock);
   mic->info.thread_dead = true;
   scond_signal(mic->info.cond);
   slock_unlock(mic->info.cond_lock);
   free(buf);
   RARCH_DBG("[ALSA] [capture thread %p] Ending microphone worker thread.\n", thread_id);
}

static int alsa_thread_microphone_read(void *driver_context, void *mic_context, void *s, size_t len)
{
   alsa_thread_microphone_t       *alsa = (alsa_thread_microphone_t*)driver_context;
   alsa_thread_microphone_handle_t *mic = (alsa_thread_microphone_handle_t*)mic_context;
   snd_pcm_state_t state;

   if (!alsa || !mic || !s) /* If any of the parameters were invalid... */
      return -1;

   if (mic->info.thread_dead) /* If the mic thread is shutting down... */
      return -1;

   state = snd_pcm_state(mic->info.pcm);
   if (state != SND_PCM_STATE_RUNNING)
   {
      int errnum;
      RARCH_WARN("[ALSA] Expected microphone \"%s\" to be in state RUNNING, was in state %s.\n",
                 snd_pcm_name(mic->info.pcm), snd_pcm_state_name(state));

      errnum = snd_pcm_start(mic->info.pcm);
      if (errnum < 0)
      {
         RARCH_ERR("[ALSA] Failed to start microphone \"%s\": %s.\n",
                   snd_pcm_name(mic->info.pcm), snd_strerror(errnum));

         return -1;
      }
   }

   /* If driver interactions shouldn't block... */
   if (alsa->nonblock)
   {
      size_t avail;
      size_t write_amt;

      /* "Hey, I'm gonna borrow the queue." */
      slock_lock(mic->info.fifo_lock);

      avail           = FIFO_READ_AVAIL(mic->info.buffer);
      write_amt       = MIN(avail, len);

      /* "It's okay if you don't have any new samples, I'll just check in on you later." */
      fifo_read(mic->info.buffer, s, write_amt);

      /* "Here, take this queue back." */
      slock_unlock(mic->info.fifo_lock);

      return (int)write_amt;
   }
   else
   {
      size_t read = 0;

      /* Until we've read all requested samples (or we're told to stop)... */
      while (read < len && !mic->info.thread_dead)
      {
         size_t avail;

         /* "Hey, I'm gonna borrow the queue." */
         slock_lock(mic->info.fifo_lock);

         avail = FIFO_READ_AVAIL(mic->info.buffer);

         if (avail == 0)
         { /* "Oh, wait, it's empty." */

            /* "Here, take it back..." */
            slock_unlock(mic->info.fifo_lock);

            /* "...I'll just wait right here." */
            slock_lock(mic->info.cond_lock);

            /* "Unless we're closing up shop..." */
            if (!mic->info.thread_dead)
               /* "...let me know when you've produced some samples." */
               scond_wait(mic->info.cond, mic->info.cond_lock);

            /* "Oh, you're ready? Okay, I'm gonna continue." */
            slock_unlock(mic->info.cond_lock);
         }
         else
         {
            size_t read_amt = MIN(len - read, avail);

            /* "I'll just go ahead and consume all these samples..."
             * (As many as will fit in s, or as many as are available.) */
            fifo_read(mic->info.buffer,s + read, read_amt);

            /* "I'm done, you can take the queue back now." */
            slock_unlock(mic->info.fifo_lock);
            read += read_amt;
         }

         /* "I'll be right back..." */
      }
      return (int)read;
   }
}

static bool alsa_thread_microphone_mic_alive(const void *driver_context, const void *mic_context);

static void *alsa_thread_microphone_open_mic(void *driver_context,
   const char *device, unsigned rate, unsigned latency, unsigned *new_rate)
{
   alsa_thread_microphone_t       *alsa = (alsa_thread_microphone_t*)driver_context;
   alsa_thread_microphone_handle_t *mic = NULL;

   if (!alsa) /* If we weren't given a valid ALSA context... */
      return NULL;

   /* If the microphone context couldn't be allocated... */
   if (!(mic = calloc(1, sizeof(alsa_thread_microphone_handle_t))))
   {
      RARCH_ERR("[ALSA] Failed to allocate microphone context.\n");
      return NULL;
   }

   if (alsa_init_pcm(&mic->info.pcm, device, SND_PCM_STREAM_CAPTURE, rate, latency,
            1, &mic->info.stream_info, new_rate, 0) < 0)
      goto error;

   mic->info.fifo_lock = slock_new();
   mic->info.cond_lock = slock_new();
   mic->info.cond      = scond_new();
   mic->info.buffer    = fifo_new(mic->info.stream_info.buffer_size);
   if (!mic->info.fifo_lock || !mic->info.cond_lock || !mic->info.cond || !mic->info.buffer || !mic->info.pcm)
      goto error;

   mic->info.worker_thread = sthread_create(alsa_microphone_worker_thread, mic);
   if (!mic->info.worker_thread)
   {
      RARCH_ERR("[ALSA] Failed to initialize microphone worker thread.\n");
      goto error;
   }
   RARCH_DBG("[ALSA] Initialized microphone worker thread.\n");

   return mic;

error:
   RARCH_ERR("[ALSA] Failed to initialize microphone.\n");

   if (mic)
   {
      if (mic->info.pcm)
         snd_pcm_close(mic->info.pcm);

      alsa_thread_microphone_close_mic(alsa, mic);
   }

   return NULL;
}

static void alsa_thread_microphone_close_mic(void *driver_context, void *mic_context)
{
   alsa_thread_microphone_handle_t *mic  = (alsa_thread_microphone_handle_t*)mic_context;
   if (mic)
   {
      alsa_thread_free_info_members(&mic->info);
      free(mic);
   }
}

static bool alsa_thread_microphone_mic_alive(const void *driver_context, const void *mic_context)
{
   alsa_thread_microphone_handle_t *mic = (alsa_thread_microphone_handle_t *)mic_context;
   if (!mic)
      return false;
   return snd_pcm_state(mic->info.pcm) == SND_PCM_STATE_RUNNING;
}

static void alsa_thread_microphone_set_nonblock_state(void *driver_context, bool state)
{
   alsa_thread_microphone_t *alsa = (alsa_thread_microphone_t*)driver_context;
   alsa->nonblock = state;
}

static struct string_list *alsa_thread_microphone_device_list_new(const void *data)
{
   return alsa_device_list_type_new("Input");
}

static void alsa_thread_microphone_device_list_free(const void *driver_context, struct string_list *devices)
{
   string_list_free(devices);
   /* Does nothing if devices is NULL */
}

static bool alsa_thread_microphone_start_mic(void *driver_context, void *mic_context)
{
   alsa_thread_microphone_handle_t *mic = (alsa_thread_microphone_handle_t*)mic_context;
   if (!mic)
      return false;
   return alsa_start_pcm(mic->info.pcm);
}

static bool alsa_thread_microphone_stop_mic(void *driver_context, void *mic_context)
{
   alsa_thread_microphone_handle_t *mic = (alsa_thread_microphone_handle_t*)mic_context;
   if (!mic)
      return false;
   return alsa_stop_pcm(mic->info.pcm);
}

static bool alsa_thread_microphone_mic_use_float(const void *driver_context, const void *mic_context)
{
   alsa_thread_microphone_handle_t *mic = (alsa_thread_microphone_handle_t*)mic_context;
   return mic->info.stream_info.has_float;
}

microphone_driver_t microphone_alsathread = {
      alsa_thread_microphone_init,
      alsa_thread_microphone_free,
      alsa_thread_microphone_read,
      alsa_thread_microphone_set_nonblock_state,
      "alsathread",
      alsa_thread_microphone_device_list_new,
      alsa_thread_microphone_device_list_free,
      alsa_thread_microphone_open_mic,
      alsa_thread_microphone_close_mic,
      alsa_thread_microphone_mic_alive,
      alsa_thread_microphone_start_mic,
      alsa_thread_microphone_stop_mic,
      alsa_thread_microphone_mic_use_float
};
#endif

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
      RARCH_ERR("[ALSA] [playback thread %u] Failed to allocate audio buffer.\n", thread_id);
      goto end;
   }

   RARCH_DBG("[ALSA] [playback thread %p] Beginning playback worker thread.\n", thread_id);
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
            RARCH_ERR("[ALSA] [playback thread %u] Failed to recover from error: %s.\n",
               thread_id,
               snd_strerror(frames));
            break;
         }

         continue;
      }
      else if (frames < 0)
      {
         RARCH_ERR("[ALSA] [playback thread %u] Error writing audio to device: %s.\n",
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
   RARCH_DBG("[ALSA] [playback thread %p] Ending playback worker thread...\n", thread_id);
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
      RARCH_ERR("[ALSA] Failed to allocate driver context.\n");
      return NULL;
   }

   RARCH_LOG("[ALSA] Using ALSA version %s.\n", snd_asoundlib_version());

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
      RARCH_ERR("[ALSA] Failed to initialize worker thread.\n");
      goto error;
   }

   return alsa;

error:
   RARCH_ERR("[ALSA] Failed to initialize.\n");

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
