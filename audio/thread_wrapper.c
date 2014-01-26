/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "thread_wrapper.h"
#include "../thread.h"
#include "../general.h"
#include "../performance.h"
#include "../fifo_buffer.h"
#include <stdlib.h>
#include <string.h>

typedef struct audio_thread
{
   const audio_driver_t *driver;
   void *driver_data;

   sthread_t *thread;
   slock_t *lock;
   scond_t *cond;
   bool alive;
   bool stopped;
   bool use_float;

   int inited;

   // Init options.
   const char *device;
   unsigned out_rate;
   unsigned latency;
} audio_thread_t;

static void audio_thread_loop(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;

   RARCH_LOG("[Audio Thread]: Initializing audio driver.\n");
   thr->driver_data = thr->driver->init(thr->device, thr->out_rate, thr->latency);
   slock_lock(thr->lock);
   thr->inited = thr->driver_data ? 1 : -1;
   if (thr->inited > 0 && thr->driver->use_float)
      thr->use_float = thr->driver->use_float(thr->driver_data);
   scond_signal(thr->cond);
   slock_unlock(thr->lock);

   if (thr->inited < 0)
      return;

   // Wait until we start to avoid calling stop immediately after init.
   slock_lock(thr->lock);
   while (thr->stopped)
      scond_wait(thr->cond, thr->lock);
   slock_unlock(thr->lock);

   RARCH_LOG("[Audio Thread]: Starting audio.\n");

   for (;;)
   {
      slock_lock(thr->lock);

      if (!thr->alive)
      {
         scond_signal(thr->cond);
         slock_unlock(thr->lock);
         break;
      }

      if (thr->stopped)
      {
         thr->driver->stop(thr->driver_data);
         while (thr->stopped)
            scond_wait(thr->cond, thr->lock);
         thr->driver->start(thr->driver_data);
      }

      slock_unlock(thr->lock);
      g_extern.system.audio_callback.callback();
   }

   RARCH_LOG("[Audio Thread]: Tearing down driver.\n");
   thr->driver->free(thr->driver_data);
}

static void audio_thread_block(audio_thread_t *thr)
{
   slock_lock(thr->lock);
   thr->stopped = true;
   scond_signal(thr->cond);
   slock_unlock(thr->lock);
}

static void audio_thread_unblock(audio_thread_t *thr)
{
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
      thr->alive = false;
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

static bool audio_thread_stop(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;
   audio_thread_block(thr);
   g_extern.system.audio_callback.set_state(false);
   return true;
}

static bool audio_thread_start(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;
   g_extern.system.audio_callback.set_state(true);
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
   return thr->use_float;
}

static ssize_t audio_thread_write(void *data, const void *buf, size_t size)
{
   audio_thread_t *thr = (audio_thread_t*)data;
   ssize_t ret = thr->driver->write(thr->driver_data, buf, size);
   if (ret < 0)
   {
      slock_lock(thr->lock);
      thr->alive = false;
      scond_signal(thr->cond);
      slock_unlock(thr->lock);
      return ret;
   }

   return ret;
}

static const audio_driver_t audio_thread = {
   NULL,
   audio_thread_write,
   audio_thread_stop,
   audio_thread_start,
   audio_thread_set_nonblock_state,
   audio_thread_free,
   audio_thread_use_float,
   "audio-thread",
   NULL, // No point in using rate control with threaded audio.
   NULL,
};

bool rarch_threaded_audio_init(const audio_driver_t **out_driver, void **out_data,
      const char *device, unsigned out_rate, unsigned latency,
      const audio_driver_t *driver)
{
   audio_thread_t *thr = (audio_thread_t*)calloc(1, sizeof(*thr));
   if (!thr)
      return false;

   thr->driver = driver;
   thr->device = device;
   thr->out_rate = out_rate;
   thr->latency = latency;

   if (!(thr->cond = scond_new()))
      goto error;
   if (!(thr->lock = slock_new()))
      goto error;

   thr->alive = true;
   thr->stopped = true;

   if (!(thr->thread = sthread_create(audio_thread_loop, thr)))
      goto error;

   // Wait until thread has initialized (or failed) the driver.
   slock_lock(thr->lock);
   while (!thr->inited)
      scond_wait(thr->cond, thr->lock);
   slock_unlock(thr->lock);

   if (thr->inited < 0) // Thread failed.
      goto error;

   *out_driver = &audio_thread;
   *out_data   = thr;
   return true;

error:
   *out_driver = NULL;
   *out_data = NULL;
   audio_thread_free(thr);
   return false;
}

