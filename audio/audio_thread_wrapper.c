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

#include <lists/string_list.h>
#include <queues/fifo_queue.h>
#include <rthreads/rthreads.h>

#include "audio_thread_wrapper.h"
#include "audio_driver.h"
#include "../verbosity.h"

/* How long a handshake between the main thread and the audio thread
 * may run before it is reported. Both are sub-millisecond on a device
 * that is answering, so anything near this is a device that has
 * stopped returning from a call; the wait continues either way. */
#define AUDIO_THREAD_HANDSHAKE_WARN_US (2 * 1000 * 1000)

typedef struct audio_thread
{
   const audio_driver_t *driver;
   void *driver_data;

   sthread_t *thread;
   slock_t *lock;
   scond_t *cond;
   const char *device;
   unsigned *new_rate;

   int inited;

   /* Initialization options. */
   unsigned out_rate;
   unsigned latency;
   unsigned block_frames;

   bool alive;
   bool stopped;
   bool stopped_ack;
   bool is_paused;
   bool is_shutdown;
   bool use_float;
   /* Ask the OS for a higher scheduling class from inside the thread. */
   bool raise_priority;
   bool prefer_fast_cores;
} audio_thread_t;

/**
 * The thread that manages the life of the audio driver.
 * The wrapped audio driver lives and dies with this function.
 */
static void audio_thread_loop(void *data)
{
   bool is_shutdown;
   audio_thread_t *thr = (audio_thread_t*)data;

   sthread_setname("ra-audio");

   /* Best effort and never fatal: a refusal leaves the default. */
   if (thr->raise_priority)
   {
      if (sthread_raise_current_priority())
         RARCH_LOG("[Audio] Audio thread priority raised.\n");
      else
         RARCH_LOG("[Audio] Audio thread priority not raised; the system refused or has no such class.\n");
   }

   if (thr->prefer_fast_cores)
   {
      if (sthread_prefer_fast_cores())
         RARCH_LOG("[Audio] Audio thread placed on the performance cores.\n");
   }

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
    * stop immediately after initialization. A stop can land here as
    * well: start() clears the flag and signals, and a stop that sets
    * it again before this thread has re-checked leaves it parked in
    * this loop rather than the one below. block() waits for the
    * acknowledgement whichever loop the thread is in, so this one
    * gives it too. */
   slock_lock(thr->lock);
   while (thr->stopped)
   {
      thr->stopped_ack = true;
      scond_signal(thr->cond);
      scond_wait(thr->cond, thr->lock);
   }
   is_shutdown = thr->is_shutdown;
   slock_unlock(thr->lock);

   /* The loop below only calls the driver's start() when it comes out
    * of a stop; the initial start has to be made here, on this thread,
    * for drivers whose init() leaves the device paused (SDL). */
   thr->driver->start(thr->driver_data, is_shutdown);

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

   audio_driver_pipeline_consumer_exit();
   thr->driver->free(thr->driver_data);
}

/**
 * Lets the audio thread finish what it's doing,
 * then stops it from doing further work.
 */
static void audio_thread_block(audio_thread_t *thr)
{
   if (!thr)
      return;

   if (thr->stopped)
      return;

   slock_lock(thr->lock);
   /* alive goes false when the thread is told to leave its loop, or
    * leaves it on its own after a failed write. Either way it will not
    * acknowledge anything again, and the wait below would never end.
    * There is nothing running to park, so there is nothing to wait
    * for. */
   if (!thr->alive)
   {
      slock_unlock(thr->lock);
      return;
   }
   thr->stopped_ack = false;
   thr->stopped = true;
   scond_signal(thr->cond);
   /* The thread may be asleep in the pipeline waiting for data; wake it
    * so it comes back to the loop and acknowledges now rather than
    * after its timeout. */
   audio_driver_pipeline_wake();

   /* Wait until audio driver actually goes to sleep. This wait is not
    * abandoned: callers past it act as though the thread is parked,
    * and free() joins the thread either way, so a device that never
    * returns from a write still stops here. It is reported, so the log
    * names what the frontend is waiting on. */
   {
      bool warned = false;
      while (!thr->stopped_ack)
      {
         if (scond_wait_timeout(thr->cond, thr->lock,
                  AUDIO_THREAD_HANDSHAKE_WARN_US))
            continue;
         if (!warned)
         {
            RARCH_WARN("[Audio] Driver \"%s\" has not acknowledged a stop after %d seconds; it is not returning from a call to the device.\n",
                  thr->driver->ident ? thr->driver->ident : "?",
                  (int)(AUDIO_THREAD_HANDSHAKE_WARN_US / 1000000));
            warned = true;
         }
      }
   }

   slock_unlock(thr->lock);
}

/**
 * Resumes the audio thread.
 * This function is called from the main thread.
 */
static void audio_thread_unblock(audio_thread_t *thr)
{
   if (!thr)
      return;

   slock_lock(thr->lock); /* Prevent the audio thread from touching this flag... */
   thr->stopped = false; /* ...so that the main thread can do it. */
   scond_signal(thr->cond); /* Then let the audio thread know that it's okay to resume. */
   slock_unlock(thr->lock); /* "As you were." */
}

static void audio_thread_free(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;

   if (!thr)
      return;

   if (thr->thread)
   {
      slock_lock(thr->lock); /* Let the audio thread finish what it's doing... */
      thr->stopped = false; /* Then stop it. "You're fired." */
      thr->alive   = false;
      scond_signal(thr->cond); /* Let the thread know it's okay to continue */
      slock_unlock(thr->lock); /* At this point, it will exit its loop. */

      /* It may be asleep in the pipeline waiting for data; wake it so
       * it sees alive == false now rather than after its timeout. */
      audio_driver_pipeline_wake();
      sthread_join(thr->thread);
      /* Wait for the audio thread to exit, ensure that it's really dead.
       * (It will call the wrapped driver's free() function.) */
   }

   if (thr->lock)
      slock_free(thr->lock);
   if (thr->cond)
      scond_free(thr->cond);
   free(thr);
   /* The audio driver is done, clean up the thread itself. */
}

static bool audio_thread_alive(void *data)
{
   bool alive          = false;
   audio_thread_t *thr = (audio_thread_t*)data;

   if (!thr)
      return false;

   /* A thread that has ended after a failed write reports the device
    * as not alive, which is what it is; block() below is a no-op then,
    * and the answer has to come from somewhere. */
   if (!thr->alive)
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

   /* Don't immediately call stop on the driver;
    * let the audio thread finish its current loop iteration.
    * It will call stop then. */
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

   slock_lock(thr->lock);
   thr->is_paused   = false;
   thr->is_shutdown = is_shutdown;
   slock_unlock(thr->lock);
   audio_thread_unblock(thr);

   return true;
}

static void audio_thread_set_nonblock_state(void *data, bool state)
{
   (void)data;
   (void)state;
   /* Ignored, because blocking state is irrelevant
    * when audio is running on a separate thread. */
}

static bool audio_thread_use_float(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;
   if (!thr)
      return false;
   return thr->use_float;
}

/* Rate control runs on this thread when the pipeline is threaded, so
 * the wrapped driver's fill queries are forwarded. They are only ever
 * called from the audio thread, the same thread that writes. Drivers
 * without them return 0 and audio_driver_init_internal() leaves rate
 * control off, as it does without the wrapper. */
static size_t audio_thread_write_avail(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;
   if (!thr || !thr->driver->write_avail || !thr->driver_data)
      return 0;
   return thr->driver->write_avail(thr->driver_data);
}

static size_t audio_thread_buffer_size(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;
   if (!thr || !thr->driver->buffer_size || !thr->driver_data)
      return 0;
   return thr->driver->buffer_size(thr->driver_data);
}

/* Only ever called from the audio thread. Zero from the wrapped
 * driver means no space is coming from this call - the device is
 * stalled, not yet streaming, or gone - and the pipeline skips the
 * pass and asks again on its next wake (see wait_writable() in
 * audio_driver.h). It is not a reason to end this thread: a device
 * that comes back is written to again, and one that never does
 * fails the write that follows, which is where the thread ends. */
static size_t audio_thread_wait_writable(void *data, size_t len)
{
   audio_thread_t *thr = (audio_thread_t*)data;
   if (!thr || !thr->driver->wait_writable || !thr->driver_data)
      return 0;
   return thr->driver->wait_writable(thr->driver_data, len);
}

/* The wrapped driver's count, for the sink rate estimate: without this
 * the frontend saw the wrapper's NULL and never measured under the
 * threaded pipeline - which is where every reporter runs. */
static size_t audio_thread_frames_consumed(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;
   if (!thr || !thr->driver->frames_consumed || !thr->driver_data)
      return 0;
   return thr->driver->frames_consumed(thr->driver_data);
}

static ssize_t audio_thread_write(void *data, const void *s, size_t len)
{
   ssize_t _len;
   audio_thread_t *thr = (audio_thread_t*)data;
   if (!thr)
      return 0;
   _len = thr->driver->write(thr->driver_data, s, len);
   if (_len < 0)
   {
      slock_lock(thr->lock);
      thr->alive = false;
      scond_signal(thr->cond);
      slock_unlock(thr->lock);
   }
   return _len;
}

/* The wrapper stands in for the real driver in audio_driver_st.current_audio,
 * so anything that asks the current driver for something the wrapper does
 * not itself do must be forwarded, or the menu sees a driver called
 * "audio-thread" with no devices and no settings. The device list is
 * enumeration, not streaming, and the underlying drivers already build
 * it from the main thread in the non-threaded case. */
static void *audio_thread_device_list_new(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;
   /* Enumeration does not need the inner driver to be initialised;
    * forward whatever context exists, NULL included. */
   if (thr && thr->driver && thr->driver->device_list_new)
      return thr->driver->device_list_new(thr->driver_data);
   return NULL;
}

static void audio_thread_device_list_free(void *data, void *list)
{
   audio_thread_t *thr = (audio_thread_t*)data;
   if (thr && thr->driver && thr->driver->device_list_free)
      thr->driver->device_list_free(thr->driver_data, list);
   else if (list)
      /* No driver to forward to - the wrapper never started, or the
       * call carries no context: the list is still a string list and
       * is released as one rather than leaked. */
      string_list_free((struct string_list*)list);
}

const audio_driver_t *audio_thread_wrapped_driver(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;
   if (thr)
      return thr->driver;
   return NULL;
}

const char *audio_thread_wrapped_ident(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;
   if (thr && thr->driver)
      return thr->driver->ident;
   return NULL;
}

static const audio_driver_t audio_thread = {
   NULL, /* No need to wrap init, it's called at the start of the thread loop */
   audio_thread_write,
   audio_thread_stop,
   audio_thread_start,
   audio_thread_alive,
   audio_thread_set_nonblock_state,
   audio_thread_free,
   audio_thread_use_float,
   "audio-thread",
   audio_thread_device_list_new,
   audio_thread_device_list_free,
   audio_thread_write_avail,
   audio_thread_buffer_size,
   NULL, /* write_raw */
   audio_thread_wait_writable,
   audio_thread_frames_consumed
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
      unsigned block_frames, bool raise_priority,
      bool prefer_fast_cores,
      const audio_driver_t *drv)
{
   audio_thread_t *thr = (audio_thread_t*)calloc(1, sizeof(*thr));
   if (!thr)
      return false;

   thr->driver         = (const audio_driver_t*)drv;
   thr->raise_priority    = raise_priority;
   thr->prefer_fast_cores = prefer_fast_cores;
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

   /* Wait until thread has initialized (or failed) the driver. Not
    * abandoned either: the thread owns thr until it is joined, so
    * returning early would free it underneath. A driver whose init()
    * does not return is reported instead of stalling silently. */
   slock_lock(thr->lock);
   {
      bool warned = false;
      while (!thr->inited)
      {
         if (scond_wait_timeout(thr->cond, thr->lock,
                  AUDIO_THREAD_HANDSHAKE_WARN_US))
            continue;
         if (!warned)
         {
            RARCH_WARN("[Audio] Driver \"%s\" has not returned from init after %d seconds; the device is not opening.\n",
                  thr->driver->ident ? thr->driver->ident : "?",
                  (int)(AUDIO_THREAD_HANDSHAKE_WARN_US / 1000000));
            warned = true;
         }
      }
   }
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
