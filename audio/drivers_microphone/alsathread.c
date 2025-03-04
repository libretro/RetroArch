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

#include <alsa/asoundlib.h>
#include <alsa/pcm.h>
#include <asm-generic/errno.h>

#include "audio/common/alsa.h"
#include "audio/common/alsathread.h"
#include "audio/microphone_driver.h"
#include "verbosity.h"
#include "retro_assert.h"


typedef struct alsa_thread_microphone_handle
{
   alsa_thread_info_t info;
} alsa_thread_microphone_handle_t;

typedef struct alsa_thread
{
   bool nonblock;
} alsa_thread_microphone_t;

static void *alsa_thread_microphone_init(void)
{
   alsa_thread_microphone_t *alsa = (alsa_thread_microphone_t*)calloc(1, sizeof(alsa_thread_microphone_t));

   if (!alsa)
   {
      RARCH_ERR("[ALSA] Failed to allocate driver context\n");
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
      RARCH_ERR("[ALSA] [capture thread %p]: Failed to allocate audio buffer\n", thread_id);
      goto end;
   }

   RARCH_DBG("[ALSA] [capture thread %p]: Beginning microphone worker thread\n", thread_id);
   RARCH_DBG("[ALSA] [capture thread %p]: Microphone \"%s\" is in state %s\n",
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
         RARCH_DBG("[ALSA] [capture thread %p]: Timeout after 33ms waiting for input\n", thread_id);
         continue;
      }
      else if (errnum == -EPIPE || errnum == -ESTRPIPE || errnum == -EINTR)
      {
         RARCH_WARN("[ALSA] [capture thread %p]: Wait error: %s\n",
                    thread_id,
                    snd_strerror(errnum));

         if ((errnum = snd_pcm_recover(mic->info.pcm, errnum, false)) < 0)
         {
            RARCH_ERR("[ALSA] [capture thread %p]: Failed to recover from prior wait error: %s\n",
                      thread_id,
                      snd_strerror(errnum));

            break;
         }

         continue;
      }

      frames = snd_pcm_readi(mic->info.pcm, buf, mic->info.stream_info.period_frames);

      if (frames == -EPIPE || frames == -EINTR || frames == -ESTRPIPE)
      {
         RARCH_WARN("[ALSA] [capture thread %p]: Read error: %s\n",
                    thread_id,
                    snd_strerror(frames));

         if ((errnum = snd_pcm_recover(mic->info.pcm, frames, false)) < 0)
         {
            RARCH_ERR("[ALSA] [capture thread %p]: Failed to recover from prior read error: %s\n",
                      thread_id,
                      snd_strerror(errnum));
            break;
         }

         continue;
      }
      else if (frames < 0)
      {
         RARCH_ERR("[ALSA] [capture thread %p]: Read error: %s.\n",
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
   RARCH_DBG("[ALSA] [capture thread %p]: Ending microphone worker thread\n", thread_id);
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
      RARCH_WARN("[ALSA]: Expected microphone \"%s\" to be in state RUNNING, was in state %s\n",
                 snd_pcm_name(mic->info.pcm), snd_pcm_state_name(state));

      errnum = snd_pcm_start(mic->info.pcm);
      if (errnum < 0)
      {
         RARCH_ERR("[ALSA]: Failed to start microphone \"%s\": %s\n",
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
      RARCH_ERR("[ALSA] Failed to allocate microphone context\n");
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
      RARCH_ERR("[ALSA]: Failed to initialize microphone worker thread\n");
      goto error;
   }
   RARCH_DBG("[ALSA]: Initialized microphone worker thread\n");

   return mic;

error:
   RARCH_ERR("[ALSA]: Failed to initialize microphone...\n");

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
