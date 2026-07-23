/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <lists/string_list.h>

#include <alsa/asoundlib.h>
#include <alsa/pcm.h>
#include <errno.h>

#include "../audio_driver.h"
#include "../common/alsa.h"
#include "../../verbosity.h"

#ifdef HAVE_MICROPHONE
#include "../microphone_driver.h"

#define BYTES_TO_FRAMES(bytes, frame_bits)  ((bytes) * 8 / frame_bits)
#define FRAMES_TO_BYTES(frames, frame_bits) ((frames) * frame_bits / 8)

typedef struct alsa_microphone_handle
{
   snd_pcm_t *pcm;
   alsa_stream_info_t stream_info;
} alsa_microphone_handle_t;

typedef struct alsa_microphone
{
   bool nonblock;
} alsa_microphone_t;

static void *alsa_microphone_init(void)
{
   alsa_microphone_t *alsa = (alsa_microphone_t*)calloc(1, sizeof(alsa_microphone_t));

   if (!alsa)
   {
      RARCH_ERR("[ALSA] Failed to allocate driver context.\n");
      return NULL;
   }

   RARCH_LOG("[ALSA] Using ALSA version %s.\n", snd_asoundlib_version());

   return alsa;
}

static void alsa_microphone_close_mic(void *driver_context, void *mic_context);
static void alsa_microphone_free(void *driver_context)
{
   alsa_microphone_t *alsa = (alsa_microphone_t*)driver_context;
   /* The mic frontend should've closed all mics before calling free(). */

   if (alsa)
   {
      snd_config_update_free_global();
      free(alsa);
   }
}

static bool alsa_microphone_start_mic(void *driver_context, void *mic_context);

static int alsa_microphone_read(void *driver_context, void *mic_context, void *s, size_t len)
{
   size_t frames_size;
   snd_pcm_sframes_t size;
   snd_pcm_state_t state;
   alsa_microphone_t       *alsa = (alsa_microphone_t*)driver_context;
   alsa_microphone_handle_t *mic = (alsa_microphone_handle_t*)mic_context;
   uint8_t *buf                  = (uint8_t*)s;
   snd_pcm_sframes_t read        = 0;
   int errnum                    = 0;

   if (!alsa || !mic || !buf)
      return -1;

   size        = BYTES_TO_FRAMES(len, mic->stream_info.frame_bits);
   frames_size = mic->stream_info.has_float ? sizeof(float) : sizeof(int16_t);

   state = snd_pcm_state(mic->pcm);
   if (state != SND_PCM_STATE_RUNNING)
   {
      RARCH_WARN("[ALSA] Expected microphone \"%s\" to be in state RUNNING, was in state %s.\n",
                 snd_pcm_name(mic->pcm),
                 snd_pcm_state_name(state));

      errnum = snd_pcm_start(mic->pcm);
      if (errnum < 0)
      {
         RARCH_ERR("[ALSA] Failed to start microphone \"%s\": %s.\n",
                   snd_pcm_name(mic->pcm),
                   snd_strerror(errnum));

         return -1;
      }
   }

   if (alsa->nonblock)
   {
      while (size)
      {
         snd_pcm_sframes_t frames = snd_pcm_readi(mic->pcm, buf, size);

         if (frames == -EPIPE || frames == -EINTR || frames == -ESTRPIPE)
         {
            errnum = snd_pcm_recover(mic->pcm, frames, 0);
            if (errnum < 0)
            {
               RARCH_ERR("[ALSA] Failed to read from microphone: %s.\n", snd_strerror(frames));
               RARCH_ERR("[ALSA] Additionally, recovery failed with: %s.\n", snd_strerror(errnum));
               return -1;
            }

            break;
         }
         else if (frames == -EAGAIN)
            break;
         else if (frames < 0)
            return -1;

         read += frames;
         buf  += frames_size;
         size -= frames;
      }
   }
   else
   {
      bool eagain_retry         = true;

      while (size)
      {
         snd_pcm_sframes_t frames;
         int rc = snd_pcm_wait(mic->pcm, -1);

         if (rc == -EPIPE || rc == -ESTRPIPE || rc == -EINTR)
         {
            if (snd_pcm_recover(mic->pcm, rc, 1) < 0)
               return -1;
            continue;
         }

         frames = snd_pcm_readi(mic->pcm, buf, size);

         if (frames == -EPIPE || frames == -EINTR || frames == -ESTRPIPE)
         {
            if (snd_pcm_recover(mic->pcm, frames, 1) < 0)
               return -1;

            break;
         }
         else if (frames == -EAGAIN)
         {
            /* Definitely not supposed to happen. */
            if (eagain_retry)
            {
               eagain_retry = false;
               continue;
            }
            break;
         }
         else if (frames < 0)
            return -1;

         read += frames;
         buf  += frames_size;
         size -= frames;
      }
   }

   return FRAMES_TO_BYTES(read, mic->stream_info.frame_bits);
}

static bool alsa_microphone_mic_alive(const void *driver_context, const void *mic_context)
{
   alsa_microphone_handle_t *mic = (alsa_microphone_handle_t*)mic_context;
   (void)driver_context;

   if (!mic)
      return false;

   return snd_pcm_state(mic->pcm) == SND_PCM_STATE_RUNNING;
}

static void alsa_microphone_set_nonblock_state(void *driver_context, bool nonblock)
{
   alsa_microphone_t *alsa = (alsa_microphone_t*)driver_context;
   alsa->nonblock = nonblock;
}

static struct string_list *alsa_microphone_device_list_new(const void *data)
{
   return alsa_device_list_type_new("Input");
}

static void alsa_microphone_device_list_free(const void *driver_context, struct string_list *devices)
{
   string_list_free(devices);
   /* Does nothing if devices is NULL */
}

static void *alsa_microphone_open_mic(void *driver_context,
   const char *device,
   unsigned rate,
   unsigned latency,
   unsigned *new_rate)
{
   alsa_microphone_t       *alsa = (alsa_microphone_t*)driver_context;
   alsa_microphone_handle_t *mic = NULL;

   if (!alsa) /* If we weren't given a valid ALSA context... */
      return NULL;

   /* If the microphone context couldn't be allocated... */
   if (!(mic = calloc(1, sizeof(alsa_microphone_handle_t))))
      return NULL;

   /* channels hardcoded to 1, because we only support mono mic input */
   if (alsa_init_pcm(&mic->pcm, device, SND_PCM_STREAM_CAPTURE, rate, latency, 1,
            &mic->stream_info, new_rate, SND_PCM_NONBLOCK) < 0)
      goto error;

   return mic;

error:
   RARCH_ERR("[ALSA] Failed to initialize microphone.\n");

   alsa_microphone_close_mic(alsa, mic);

   return NULL;

}
static void alsa_microphone_close_mic(void *driver_context, void *mic_context)
{
   alsa_microphone_handle_t *mic = (alsa_microphone_handle_t*)mic_context;
   (void)driver_context;

   if (mic)
   {
      alsa_free_pcm(mic->pcm);
      free(mic);
   }
}

static bool alsa_microphone_start_mic(void *driver_context, void *mic_context)
{
   alsa_microphone_handle_t *mic = (alsa_microphone_handle_t*)mic_context;
   if (!mic)
      return false;
   return alsa_start_pcm(mic->pcm);
}

static bool alsa_microphone_stop_mic(void *driver_context, void *mic_context)
{
   alsa_microphone_handle_t *mic = (alsa_microphone_handle_t*)mic_context;
   if (!mic)
      return false;
   return alsa_stop_pcm(mic->pcm);
}

static bool alsa_microphone_mic_use_float(const void *driver_context, const void *mic_context)
{
   alsa_microphone_handle_t *mic = (alsa_microphone_handle_t*)mic_context;
   return mic->stream_info.has_float;
}

microphone_driver_t microphone_alsa = {
        alsa_microphone_init,
        alsa_microphone_free,
        alsa_microphone_read,
        alsa_microphone_set_nonblock_state,
        "alsa",
        alsa_microphone_device_list_new,
        alsa_microphone_device_list_free,
        alsa_microphone_open_mic,
        alsa_microphone_close_mic,
        alsa_microphone_mic_alive,
        alsa_microphone_start_mic,
        alsa_microphone_stop_mic,
        alsa_microphone_mic_use_float
};
#endif

typedef struct alsa
{
   snd_pcm_t *pcm;
   alsa_stream_info_t stream_info;
   bool nonblock;
   bool is_paused;
} alsa_t;

static bool alsa_use_float(void *data)
{
   alsa_t *alsa = (alsa_t*)data;
   return alsa->stream_info.has_float;
}

static void alsa_free(void *data);
static void *alsa_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   alsa_t *alsa = (alsa_t*)calloc(1, sizeof(alsa_t));

   if (!alsa)
   {
      RARCH_ERR("[ALSA] Failed to allocate driver context.\n");
      return NULL;
   }

   RARCH_LOG("[ALSA] Using ALSA version %s.\n", snd_asoundlib_version());

   if (alsa_init_pcm(&alsa->pcm, device, SND_PCM_STREAM_PLAYBACK, rate,
            latency, 2, &alsa->stream_info, new_rate, SND_PCM_NONBLOCK) < 0)
      goto error;

   return alsa;

error:
   RARCH_ERR("[ALSA] Failed to initialize.\n");

   alsa_free(alsa);

   return NULL;
}

#define BYTES_TO_FRAMES(bytes, frame_bits)  ((bytes) * 8 / frame_bits)
#define FRAMES_TO_BYTES(frames, frame_bits) ((frames) * frame_bits / 8)

static bool alsa_start(void *data, bool is_shutdown)
{
   alsa_t *alsa = (alsa_t*)data;
   if (!alsa->is_paused)
      return true;

   if (     alsa->stream_info.can_pause
         && alsa->is_paused)
   {
      int ret = snd_pcm_pause(alsa->pcm, 0);

      if (ret < 0)
      {
         RARCH_ERR("[ALSA] Failed to unpause: %s.\n",
               snd_strerror(ret));
         return false;
      }

      alsa->is_paused = false;
   }
   return true;
}

static ssize_t alsa_write(void *data, const void *buf_, size_t len)
{
   ssize_t _len = 0;
   alsa_t *alsa           = (alsa_t*)data;
   const uint8_t *buf     = (const uint8_t*)buf_;
   snd_pcm_sframes_t size = BYTES_TO_FRAMES(len, alsa->stream_info.frame_bits);
   size_t frames_size     = alsa->stream_info.has_float ? sizeof(float) : sizeof(int16_t);

   /* Workaround buggy menu code.
    * If a write happens while we're paused, we might never progress. */
   if (alsa->is_paused && !alsa_start(alsa, false))
      return -1;

   if (alsa->nonblock)
   {
      while (size)
      {
         snd_pcm_sframes_t frames = snd_pcm_writei(alsa->pcm, buf, size);

         if (frames == -EPIPE || frames == -EINTR || frames == -ESTRPIPE)
         {
            if (snd_pcm_recover(alsa->pcm, frames, 1) < 0)
               return -1;

            break;
         }
         else if (frames == -EAGAIN)
            break;
         else if (frames < 0)
            return -1;

         _len  += FRAMES_TO_BYTES(frames, alsa->stream_info.frame_bits);
         buf   += (frames << 1) * frames_size;
         size  -= frames;
      }
   }
   else
   {
      bool eagain_retry         = true;

      while (size)
      {
         snd_pcm_sframes_t frames;
         int rc = snd_pcm_wait(alsa->pcm, -1);

         if (rc == -EPIPE || rc == -ESTRPIPE || rc == -EINTR)
         {
            if (snd_pcm_recover(alsa->pcm, rc, 1) < 0)
               return -1;
            continue;
         }

         frames = snd_pcm_writei(alsa->pcm, buf, size);

         if (frames == -EPIPE || frames == -EINTR || frames == -ESTRPIPE)
         {
            if (snd_pcm_recover(alsa->pcm, frames, 1) < 0)
               return -1;

            break;
         }
         else if (frames == -EAGAIN)
         {
            /* Definitely not supposed to happen. */
            if (eagain_retry)
            {
               eagain_retry = false;
               continue;
            }
            break;
         }
         else if (frames < 0)
            return -1;

         _len += FRAMES_TO_BYTES(frames, alsa->stream_info.frame_bits);
         buf  += (frames << 1) * frames_size;
         size -= frames;
      }
   }

   return _len;
}

static bool alsa_alive(void *data)
{
   alsa_t *alsa = (alsa_t*)data;
   if (!alsa)
      return false;
   return !alsa->is_paused;
}

static bool alsa_stop(void *data)
{
   alsa_t *alsa = (alsa_t*)data;
   if (alsa->is_paused)
	  return true;

   if (alsa->stream_info.can_pause
         && !alsa->is_paused)
   {
      int ret = snd_pcm_pause(alsa->pcm, 1);

      if (ret < 0)
         return false;

      alsa->is_paused = true;
   }

   return true;
}

static void alsa_set_nonblock_state(void *data, bool state)
{
   alsa_t *alsa = (alsa_t*)data;
   alsa->nonblock = state;
}

static void alsa_free(void *data)
{
   alsa_t *alsa = (alsa_t*)data;

   if (alsa)
   {
      alsa_free_pcm(alsa->pcm);

      snd_config_update_free_global();
      free(alsa);
   }
}

static size_t alsa_write_avail(void *data)
{
   alsa_t *alsa            = (alsa_t*)data;
   snd_pcm_sframes_t avail = snd_pcm_avail(alsa->pcm);

   if (avail < 0)
      return alsa->stream_info.buffer_size;

   return FRAMES_TO_BYTES(avail, alsa->stream_info.frame_bits);
}

static size_t alsa_buffer_size(void *data)
{
   alsa_t *alsa = (alsa_t*)data;
   return alsa->stream_info.buffer_size;
}

void *alsa_device_list_new(void *data)
{
   return alsa_device_list_type_new("Output");
}

void alsa_device_list_free(void *data, void *array_list_data)
{
   struct string_list *s = (struct string_list*)array_list_data;

   if (s)
      string_list_free(s);
}

audio_driver_t audio_alsa = {
   alsa_init,
   alsa_write,
   alsa_stop,
   alsa_start,
   alsa_alive,
   alsa_set_nonblock_state,
   alsa_free,
   alsa_use_float,
   "alsa",
   alsa_device_list_new,
   alsa_device_list_free,
   alsa_write_avail,
   alsa_buffer_size,
   NULL /* write_raw */
};

/* ===========================================================================
 * Threaded ALSA driver ("alsathread"): a worker thread owns the blocking
 * snd_pcm_writei while the frontend writes into a fifo.  A separate,
 * independently selectable driver - not a replacement for the synchronous
 * "alsa" driver above, which is always built.  This section is guarded by
 * exactly the expression audio_driver.c uses to register audio_alsathread
 * and microphone_alsathread, so the vtables exist iff they are registered
 * (previously alsathread.c had no internal guard and relied on the build
 * system, which griffin did not enforce).
 * ======================================================================== */
#if !defined(__QNX__) && !defined(MIYOO) && defined(HAVE_THREADS)

#include <boolean.h>
#include <rthreads/rthreads.h>
#include <queues/fifo_queue.h>

typedef struct alsa_thread_info
{
   snd_pcm_t *pcm;
   fifo_buffer_t *buffer;
   /* True only while the playback worker is inside its bounded wait
    * for the producer; lets the producer skip the condition signal
    * entirely in steady state (checked under fifo_lock, so no torn
    * reads). */
   volatile bool worker_waiting;
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

static int alsa_thread_microphone_read(void *driver_context, void *mic_context, void *sv, size_t len)
{
   snd_pcm_state_t state;
   size_t _len = 0;
   uint8_t *s  = (uint8_t*)sv;
   alsa_thread_microphone_t       *alsa = (alsa_thread_microphone_t*)driver_context;
   alsa_thread_microphone_handle_t *mic = (alsa_thread_microphone_handle_t*)mic_context;

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

      /* "Hey, I'm gonna borrow the queue." */
      slock_lock(mic->info.fifo_lock);

      avail           = FIFO_READ_AVAIL(mic->info.buffer);
      _len            = MIN(avail, len);

      /* "It's okay if you don't have any new samples, I'll just check in on you later." */
      fifo_read(mic->info.buffer, s, _len);

      /* "Here, take this queue back." */
      slock_unlock(mic->info.fifo_lock);
   }
   else
   {
      /* Until we've read all requested samples (or we're told to stop)... */
      while (_len < len && !mic->info.thread_dead)
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
            size_t read_amt = MIN(len - _len, avail);

            /* "I'll just go ahead and consume all these samples..."
             * (As many as will fit in s, or as many as are available.) */
            fifo_read(mic->info.buffer,s + _len, read_amt);

            /* "I'm done, you can take the queue back now." */
            slock_unlock(mic->info.fifo_lock);
            _len += read_amt;
         }
         /* "I'll be right back..." */
      }
   }
   return _len;
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
   /* Bounded wait budget for a late producer, expressed in wall time:
    * after a blocking writei returns, the device still holds close to
    * a full buffer of queued audio, so the worker can afford to wait
    * (buffer/period - 1) periods for the fifo to reach a full period
    * before padding with silence.  Waiting there absorbs ordinary
    * producer lateness (scheduler jitter, video-frame beat against
    * the period grid) without injecting an audible mid-stream gap;
    * only a genuinely stalled producer still degrades to silence,
    * and it does so before the device xruns. */
   int64_t period_us = (int64_t)alsa->info.stream_info.period_frames
         * 1000000 / alsa->info.stream_info.rate;
   int64_t budget_us = period_us
         * ((int64_t)(alsa->info.stream_info.buffer_size
               / alsa->info.stream_info.period_size) - 1);

   while (!alsa->info.thread_dead)
   {
      size_t avail;
      size_t fifo_size;
      snd_pcm_sframes_t frames;
      slock_lock(alsa->info.fifo_lock);
      avail     = FIFO_READ_AVAIL(alsa->info.buffer);
      if (avail < alsa->info.stream_info.period_size)
      {
         int64_t waited_us = 0;
         while (   avail < alsa->info.stream_info.period_size
                && waited_us < budget_us
                && !alsa->info.thread_dead)
         {
            alsa->info.worker_waiting = true;
            slock_unlock(alsa->info.fifo_lock);
            slock_lock(alsa->info.cond_lock);
            if (!alsa->info.thread_dead)
               scond_wait_timeout(alsa->info.cond,
                     alsa->info.cond_lock, period_us / 2);
            slock_unlock(alsa->info.cond_lock);
            /* Upper bound on time spent regardless of early wakeups:
             * overestimating only makes the worker give up sooner,
             * never lets the device xrun. */
            waited_us += period_us / 2;
            slock_lock(alsa->info.fifo_lock);
            avail = FIFO_READ_AVAIL(alsa->info.buffer);
         }
         alsa->info.worker_waiting = false;
      }
      fifo_size = MIN(alsa->info.stream_info.period_size, avail);
      fifo_read(alsa->info.buffer, buf, fifo_size);
      scond_signal(alsa->info.cond);
      slock_unlock(alsa->info.fifo_lock);

      /* Genuine underrun (producer stalled past the wait budget):
       * fill the rest with silence. */
      memset(buf + fifo_size, 0, alsa->info.stream_info.period_size - fifo_size);

      frames = snd_pcm_writei(alsa->info.pcm, buf, alsa->info.stream_info.period_frames);

      if (     frames == -EPIPE
            || frames == -EINTR
            || frames == -ESTRPIPE)
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

   if (alsa_init_pcm(&alsa->info.pcm, device, SND_PCM_STREAM_PLAYBACK, rate,
            latency, 2, &alsa->info.stream_info, new_rate, 0) < 0)
      goto error;

   alsa->info.fifo_lock = slock_new();
   alsa->info.cond_lock = slock_new();
   alsa->info.cond      = scond_new();
   alsa->info.buffer    = fifo_new(alsa->info.stream_info.buffer_size);
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
   ssize_t _len = 0;
   alsa_thread_t *alsa = (alsa_thread_t*)data;

   if (alsa->info.thread_dead)
      return -1;

   if (alsa->nonblock)
   {
      size_t avail;

      slock_lock(alsa->info.fifo_lock);
      avail = FIFO_WRITE_AVAIL(alsa->info.buffer);
      _len  = MIN(avail, len);

      fifo_write(alsa->info.buffer, s, _len);
      if (alsa->info.worker_waiting)
         scond_signal(alsa->info.cond);
      slock_unlock(alsa->info.fifo_lock);
   }
   else
   {
      while (_len < (ssize_t)len && !alsa->info.thread_dead)
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
            size_t write_amt = MIN(len - _len, avail);
            fifo_write(alsa->info.buffer,
                  (const char*)s + _len, write_amt);
            if (alsa->info.worker_waiting)
               scond_signal(alsa->info.cond);
            slock_unlock(alsa->info.fifo_lock);
            _len += write_amt;
         }
      }
   }
   return _len;
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
   size_t _len;
   alsa_thread_t *alsa = (alsa_thread_t*)data;
   if (alsa->info.thread_dead)
      return 0;
   slock_lock(alsa->info.fifo_lock);
   _len = FIFO_WRITE_AVAIL(alsa->info.buffer);
   slock_unlock(alsa->info.fifo_lock);
   return _len;
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
   alsa_device_list_new, /* Shared with the sync driver above -
                            * they don't use the driver context. */
   alsa_device_list_free,
   alsa_thread_write_avail,
   alsa_thread_buffer_size,
   NULL /* write_raw */
};

#endif /* !__QNX__ && !MIYOO && HAVE_THREADS */
