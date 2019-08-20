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

#include <stdlib.h>
#include <string.h>

#include <queues/fifo_queue.h>
#include <rthreads/rthreads.h>

#include "audio_thread_wrapper.h"
#include "../verbosity.h"

typedef struct audio_thread
{
   const audio_driver_t *driver;
   void *driver_data;

   sthread_t *thread;
   slock_t *lock;
   scond_t *cond;
   bool alive;
   bool stopped;
   bool stopped_ack;
   bool is_paused;
   bool is_shutdown;
   bool use_float;

   int inited;

   /* Initialization options. */
   const char *device;
   unsigned *new_rate;
   unsigned out_rate;
   unsigned latency;
   unsigned block_frames;
} audio_thread_t;

static void audio_thread_loop(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;

   if (!thr)
      return;

   thr->driver_data   = thr->driver->init(
         thr->device, thr->out_rate, thr->latency,
         thr->block_frames, thr->new_rate);
   slock_lock(thr->lock);
   thr->inited        = thr->driver_data ? 1 : -1;
   if (thr->inited > 0 && thr->driver->use_float)
      thr->use_float  = thr->driver->use_float(thr->driver_data);
   scond_signal(thr->cond);
   slock_unlock(thr->lock);

   if (thr->inited < 0)
      return;

   /* Wait until we start to avoid calling
    * stop immediately after initialization. */
   slock_lock(thr->lock);
   while (thr->stopped)
      scond_wait(thr->cond, thr->lock);
   slock_unlock(thr->lock);

   for (;;)
   {
      slock_lock(thr->lock);

      if (!thr->alive)
      {
         scond_signal(thr->cond);
         thr->stopped_ack = true;
         slock_unlock(thr->lock);
         break;
      }

      if (thr->stopped)
      {
         thr->driver->stop(thr->driver_data);
         while (thr->stopped)
         {
            /* If we stop right after start, 
             * we might not be able to properly ack.
             * Signal in the loop instead. */
            thr->stopped_ack = true;
            scond_signal(thr->cond);

            scond_wait(thr->cond, thr->lock);
         }
         thr->driver->start(thr->driver_data, thr->is_shutdown);
      }

      slock_unlock(thr->lock);
      audio_driver_callback();
   }

   thr->driver->free(thr->driver_data);
}

static void audio_thread_block(audio_thread_t *thr)
{
   if (!thr)
      return;

   if (thr->stopped)
      return;

   slock_lock(thr->lock);
   thr->stopped_ack = false;
   thr->stopped = true;
   scond_signal(thr->cond);

   /* Wait until audio driver actually goes to sleep. */
   while (!thr->stopped_ack)
      scond_wait(thr->cond, thr->lock);

   slock_unlock(thr->lock);
}

static void audio_thread_unblock(audio_thread_t *thr)
{
   if (!thr)
      return;

   slock_lock(thr->lock);
   thr->stopped = false;
   scond_signal(thr->cond);
   slock_unlock(thr->lock);
}

static void audio_thread_free(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;

   if (!thr)
      return;

   if (thr->thread)
   {
      slock_lock(thr->lock);
      thr->stopped = false;
      thr->alive   = false;
      scond_signal(thr->cond);
      slock_unlock(thr->lock);

      sthread_join(thr->thread);
   }

   if (thr->lock)
      slock_free(thr->lock);
   if (thr->cond)
      scond_free(thr->cond);
   free(thr);
}

static bool audio_thread_alive(void *data)
{
   bool alive          = false;
   audio_thread_t *thr = (audio_thread_t*)data;

   if (!thr)
      return false;

   audio_thread_block(thr);
   alive = !thr->is_paused;
   audio_thread_unblock(thr);

   return alive;
}

static bool audio_thread_stop(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;

   if (!thr)
      return false;

   audio_thread_block(thr);
   thr->is_paused = true;

   audio_driver_disable_callback();

   return true;
}

static bool audio_thread_start(void *data, bool is_shutdown)
{
   audio_thread_t *thr = (audio_thread_t*)data;

   if (!thr)
      return false;

   audio_driver_enable_callback();

   thr->is_paused   = false;
   thr->is_shutdown = is_shutdown;
   audio_thread_unblock(thr);

   return true;
}

static void audio_thread_set_nonblock_state(void *data, bool state)
{
   (void)data;
   (void)state;
}

static bool audio_thread_use_float(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;
   if (!thr)
      return false;
   return thr->use_float;
}

static ssize_t audio_thread_write(void *data, const void *buf, size_t size)
{
   ssize_t ret;
   audio_thread_t *thr = (audio_thread_t*)data;

   if (!thr)
      return 0;

   ret = thr->driver->write(thr->driver_data, buf, size);

   if (ret < 0)
   {
      slock_lock(thr->lock);
      thr->alive = false;
      scond_signal(thr->cond);
      slock_unlock(thr->lock);
   }

   return ret;
}

static const audio_driver_t audio_thread = {
   NULL,
   audio_thread_write,
   audio_thread_stop,
   audio_thread_start,
   audio_thread_alive,
   audio_thread_set_nonblock_state,
   audio_thread_free,
   audio_thread_use_float,
   "audio-thread",
   NULL, /* No point in using rate control with threaded audio. */
   NULL,
};

/**
 * audio_init_thread:
 * @out_driver                : output driver
 * @out_data                  : output audio data
 * @device                    : audio device (optional)
 * @out_rate                  : output audio rate
 * @latency                   : audio latency
 * @driver                    : audio driver
 *
 * Starts a audio driver in a new thread.
 * Access to audio driver will be mediated through this driver.
 * This driver interfaces with audio callback and is
 * only used in that case.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool audio_init_thread(const audio_driver_t **out_driver,
      void **out_data, const char *device, unsigned audio_out_rate,
      unsigned *new_rate, unsigned latency,
      unsigned block_frames, const audio_driver_t *drv)
{
   audio_thread_t *thr = (audio_thread_t*)calloc(1, sizeof(*thr));
   if (!thr)
      return false;

   thr->driver         = (const audio_driver_t*)drv;
   thr->device         = device;
   thr->out_rate       = audio_out_rate;
   thr->new_rate       = new_rate;
   thr->latency        = latency;
   thr->block_frames   = block_frames;

   if (!(thr->cond     = scond_new()))
      goto error;
   if (!(thr->lock     = slock_new()))
      goto error;

   thr->alive          = true;
   thr->stopped        = true;

   if (!(thr->thread   = sthread_create(audio_thread_loop, thr)))
      goto error;

   /* Wait until thread has initialized (or failed) the driver. */
   slock_lock(thr->lock);
   while (!thr->inited)
      scond_wait(thr->cond, thr->lock);
   slock_unlock(thr->lock);

   if (thr->inited < 0) /* Thread failed. */
      goto error;

   *out_driver         = &audio_thread;
   *out_data           = thr;
   return true;

error:
   *out_driver         = NULL;
   *out_data           = NULL;
   audio_thread_free(thr);
   return false;
}
