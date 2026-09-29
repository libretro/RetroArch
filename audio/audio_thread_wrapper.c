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
#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>
#include <rthreads/rthreads.h>
#include <features/features_cpu.h>

#include "audio_thread_wrapper.h"
#include "audio_driver.h"
#include "../verbosity.h"
#include "../frontend/thread_elevation.h"

/* How long a handshake between the main thread and the audio thread
 * may run before it is reported. Both are sub-millisecond on a device
 * that is answering, so anything near this is a device that has
 * stopped returning from a call; the wait continues either way. */
#define AUDIO_THREAD_HANDSHAKE_WARN_US (2 * 1000 * 1000)
/* Long past any device that is merely slow to open: a driver's init()
 * that has not returned by now is not going to, and the frontend goes
 * on without audio rather than never returning from drivers_init(). */
#define AUDIO_THREAD_HANDSHAKE_GIVEUP_US (30 * 1000 * 1000)

/* How long the loop parks when audio_driver_callback() had nothing to
 * render or consume - a core paused behind the menu, or a callback
 * that pushed no samples. There is no device write to pace on in that
 * case, so this is the poll interval for the core coming back, and
 * the bound on how late the first samples after it come out. A stop
 * request signals the condition and cuts the wait short. */
#define AUDIO_THREAD_IDLE_WAIT_US 1000

enum audio_thread_init_state
{
   AUDIO_THREAD_INIT_PENDING = 0,
   AUDIO_THREAD_INIT_OK,
   AUDIO_THREAD_INIT_FAILED,
   AUDIO_THREAD_INIT_ABANDONED
};

typedef struct audio_thread
{
   const audio_driver_t *driver;
   void *driver_data;

   sthread_t *thread;
   const char *device;
   unsigned *new_rate;

   /* Both threads park here: the audio thread between passes it has
    * nothing to do on and while stopped, the main thread for the
    * stop acknowledgement and for init() to return. Every change to
    * the words below is written first and notified after, so a waiter's
    * re-check between prepare and commit sees it. */
   retro_eventcount_t ec;
   /* The thread's two loop conditions, tested on every pass without
    * any lock. alive only ever goes false. */
   retro_atomic_int_t alive;
   retro_atomic_int_t stopped;
   /* The thread is parked in a stopped loop and the driver is stopped;
    * block() waits for it. */
   retro_atomic_int_t stopped_ack;
   /* How init() ended, and who owns this struct: the thread moves it
    * from PENDING to OK or FAILED, the frontend from PENDING to
    * ABANDONED when it gives up waiting. One compare-and-swap each, so
    * exactly one side wins - the other reads what happened and acts on
    * that. An abandoned thread frees the struct itself when init()
    * finally returns. */
   retro_atomic_int_t init_state;

   /* Initialization options. */
   unsigned out_rate;
   unsigned latency;

   bool is_paused;
   bool is_shutdown;
   bool use_float;
   /* The layout the inner driver opened with, read as use_float is:
    * on the thread, once, right after init. */
   uint32_t layout;
   /* Ask the OS for a higher scheduling class from inside the thread. */
   bool raise_priority;
   bool prefer_fast_cores;
} audio_thread_t;

/* Audio thread: parks while stopped, acknowledging the stop so block()
 * can return. block() clears the ack before it raises the flag, so an
 * ack found clear inside the wait window means a start and a second
 * stop landed since this pass acknowledged, and that stop is answered
 * before parking. The ack is stored and notified before the window,
 * never inside it: a notify from inside would cut the thread's own
 * wait short. */
static void audio_thread_park_stopped(audio_thread_t *thr)
{
   for (;;)
   {
      int key;
      if (!retro_atomic_load_acquire_int(&thr->stopped))
         return;
      retro_atomic_store_release_int(&thr->stopped_ack, 1);
      retro_eventcount_notify(&thr->ec);

      key = retro_eventcount_prepare_wait(&thr->ec);
      if (!retro_atomic_load_acquire_int(&thr->stopped))
      {
         retro_eventcount_cancel_wait(&thr->ec);
         return;
      }
      if (!retro_atomic_load_acquire_int(&thr->stopped_ack))
      {
         retro_eventcount_cancel_wait(&thr->ec);
         continue;
      }
      retro_eventcount_commit_wait(&thr->ec, key);
   }
}

/**
 * The thread that manages the life of the audio driver.
 * The wrapped audio driver lives and dies with this function.
 */
static void audio_thread_loop(void *data)
{
   bool is_shutdown;
   audio_thread_t *thr = (audio_thread_t*)data;

   if (!thr)
      return;

   sthread_setname("ra-audio");

   /* Best effort and never fatal: a refusal leaves the default. */
   if (thr->raise_priority)
   {
      const char *via   = NULL;
      const char *added = NULL;
      switch (thread_elevation_raise_current(&via, &added))
      {
         case THREAD_ELEVATION_GRANTED:
            RARCH_LOG("[Audio] Audio thread priority raised.\n");
            break;
         case THREAD_ELEVATION_PENDING:
            /* Finishing on a thread of its own; this one does not wait. */
            RARCH_LOG("[Audio] Audio thread priority not raised directly; asking %s.\n", via);
            break;
         default:
            RARCH_LOG("[Audio] Audio thread priority not raised; the system refused or has no such class.\n");
            break;
      }
      if (added)
         RARCH_LOG("[Audio] Audio thread runs on %s.\n", added);
   }

   if (thr->prefer_fast_cores)
   {
      if (sthread_prefer_fast_cores())
         RARCH_LOG("[Audio] Audio thread placed on the performance cores.\n");
   }

   thr->driver_data   = thr->driver->init(
         thr->device, thr->out_rate, thr->latency,
         thr->new_rate);
   /* Read by the frontend once it sees the outcome below */
   thr->use_float     = false;
   if (thr->driver_data && thr->driver->use_float)
      thr->use_float  = thr->driver->use_float(thr->driver_data);
   thr->layout        = AUDIO_LAYOUT_STEREO;
   if (thr->driver_data && thr->driver->layout)
      thr->layout     = thr->driver->layout(thr->driver_data);

   /* Publish the outcome, unless the frontend gave up waiting first:
    * then nobody is waiting for this any more and nobody else will
    * free it. */
   if (!retro_atomic_cas_int(&thr->init_state, AUDIO_THREAD_INIT_PENDING,
         thr->driver_data ? AUDIO_THREAD_INIT_OK : AUDIO_THREAD_INIT_FAILED))
   {
      if (thr->driver_data && thr->driver->free)
         thr->driver->free(thr->driver_data);
      retro_eventcount_free(&thr->ec);
      free(thr);
      return;
   }
   retro_eventcount_notify(&thr->ec);

   if (!thr->driver_data)
      return;

   /* Wait until we start to avoid calling
    * stop immediately after initialization. A stop can land here as
    * well: start() clears the flag and signals, and a stop that sets
    * it again before this thread has re-checked leaves it parked in
    * this loop rather than the one below. block() waits for the
    * acknowledgement whichever loop the thread is in, so this one
    * gives it too. */
   audio_thread_park_stopped(thr);
   is_shutdown = thr->is_shutdown;

   /* The loop below only calls the driver's start() when it comes out
    * of a stop; the initial start has to be made here, on this thread,
    * for drivers whose init() leaves the device paused (SDL). */
   thr->driver->start(thr->driver_data, is_shutdown);

   for (;;)
   {
      /* Tested without the lock: a pass that is neither leaving nor
       * parking has nothing to say to the main thread, and that is
       * every pass while audio is playing. A request that lands just
       * after either test is taken on the next pass, which is where it
       * was taken before. */
      if (!retro_atomic_load_acquire_int(&thr->alive))
      {
         retro_atomic_store_release_int(&thr->stopped_ack, 1);
         retro_eventcount_notify(&thr->ec);
         break;
      }

      if (retro_atomic_load_acquire_int(&thr->stopped))
      {
         thr->driver->stop(thr->driver_data);
         audio_thread_park_stopped(thr);
         thr->driver->start(thr->driver_data, thr->is_shutdown);
      }

      if (!audio_driver_callback())
      {
         /* The re-check between prepare and commit is what keeps a
          * request that lands here from going unseen until the
          * timeout */
         int key = retro_eventcount_prepare_wait(&thr->ec);
         if (     retro_atomic_load_acquire_int(&thr->alive)
               && !retro_atomic_load_acquire_int(&thr->stopped))
            retro_eventcount_commit_wait_timeout(&thr->ec, key,
                  AUDIO_THREAD_IDLE_WAIT_US);
         else
            retro_eventcount_cancel_wait(&thr->ec);
      }
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

   if (retro_atomic_load_acquire_int(&thr->stopped))
      return;

   /* alive goes false when the thread is told to leave its loop, or
    * leaves it on its own after a failed write. Either way it will not
    * acknowledge anything again, and the wait below would never end.
    * There is nothing running to park, so there is nothing to wait
    * for. */
   if (!retro_atomic_load_acquire_int(&thr->alive))
      return;
   retro_atomic_store_release_int(&thr->stopped_ack, 0);
   retro_atomic_store_release_int(&thr->stopped, 1);
   retro_eventcount_notify(&thr->ec);
   /* The thread may be asleep in the pipeline waiting for data; wake it
    * so it comes back to the loop and acknowledges now rather than
    * after its timeout. */
   audio_driver_pipeline_wake();

   /* Wait until audio driver actually goes to sleep. This wait is not
    * abandoned: callers past it act as though the thread is parked,
    * and free() joins the thread either way, so a device that never
    * returns from a write still stops here. It is reported, so the log
    * names what the frontend is waiting on. A thread that leaves its
    * loop meanwhile acks on the way out. */
   {
      bool warned = false;
      for (;;)
      {
         int key;
         if (retro_atomic_load_acquire_int(&thr->stopped_ack))
            break;
         key = retro_eventcount_prepare_wait(&thr->ec);
         if (retro_atomic_load_acquire_int(&thr->stopped_ack))
         {
            retro_eventcount_cancel_wait(&thr->ec);
            break;
         }
         if (retro_eventcount_commit_wait_timeout(&thr->ec, key,
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
}

/**
 * Resumes the audio thread.
 * This function is called from the main thread.
 */
static void audio_thread_unblock(audio_thread_t *thr)
{
   if (!thr)
      return;

   retro_atomic_store_release_int(&thr->stopped, 0);
   retro_eventcount_notify(&thr->ec);
}

static void audio_thread_free(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;

   if (!thr)
      return;

   if (thr->thread)
   {
      /* Let the thread out of whichever wait it is in; it leaves its
       * loop on seeing alive false. */
      retro_atomic_store_release_int(&thr->stopped, 0);
      retro_atomic_store_release_int(&thr->alive,    0);
      retro_eventcount_notify(&thr->ec);

      /* It may be asleep in the pipeline waiting for data; wake it so
       * it sees alive == false now rather than after its timeout. */
      audio_driver_pipeline_wake();
      sthread_join(thr->thread);
      /* Wait for the audio thread to exit, ensure that it's really dead.
       * (It will call the wrapped driver's free() function.) */
   }

   retro_eventcount_free(&thr->ec);
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
   if (!retro_atomic_load_acquire_int(&thr->alive))
      return false;

   audio_thread_block(thr);
   alive = !thr->is_paused;
   audio_thread_unblock(thr);

   return alive;
}

void audio_thread_apply_control(void *data,
      void (*control)(void *userdata), void *userdata)
{
   audio_thread_t *thr = (audio_thread_t*)data;
   bool running;
   if (!thr || !control)
      return;
   running = !retro_atomic_load_acquire_int(&thr->stopped);
   audio_thread_block(thr);
   control(userdata);
   if (running)
      audio_thread_unblock(thr);
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

   /* Written before the release store that clears stopped, which the
    * thread acquire-loads before it reads is_shutdown */
   thr->is_paused   = false;
   thr->is_shutdown = is_shutdown;
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
/* The wrapper is the driver the frontend sees, so a hook it does not
 * forward is a hook the frontend never calls. This one it did not,
 * and a 5.1 device under the threaded driver got the stereo mix as
 * 8-byte frames into 24-byte ones. */
static uint32_t audio_thread_layout(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;
   if (!thr)
      return AUDIO_LAYOUT_STEREO;
   return thr->layout;
}

static size_t audio_thread_underruns(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;
   if (!thr || !thr->driver->underruns || !thr->driver_data)
      return 0;
   return thr->driver->underruns(thr->driver_data);
}

static size_t audio_thread_frames_consumed(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;
   if (!thr || !thr->driver->frames_consumed || !thr->driver_data)
      return 0;
   return thr->driver->frames_consumed(thr->driver_data);
}

/* The driver's own count of frames the device took, where it keeps one
 * beside the device clock. Not forwarded before, so the sink-rate
 * comparison this exists for went missing on exactly the configuration
 * it is most wanted on - the threaded one. */
static bool audio_thread_device_clock_ppm(void *data, double *ppm)
{
   audio_thread_t *thr = (audio_thread_t*)data;
   if (!thr || !thr->driver->device_clock_ppm || !thr->driver_data)
      return false;
   return thr->driver->device_clock_ppm(thr->driver_data, ppm);
}

static size_t audio_thread_frames_consumed_fallback(void *data)
{
   audio_thread_t *thr = (audio_thread_t*)data;
   if (!thr || !thr->driver->frames_consumed_fallback || !thr->driver_data)
      return 0;
   return thr->driver->frames_consumed_fallback(thr->driver_data);
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
      retro_atomic_store_release_int(&thr->alive, 0);
      retro_eventcount_notify(&thr->ec);
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
   audio_thread_frames_consumed,
   audio_thread_underruns,
   audio_thread_layout,
   audio_thread_frames_consumed_fallback,
   audio_thread_device_clock_ppm
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
      bool raise_priority,
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

   if (!retro_eventcount_init(&thr->ec))
      goto error;

   retro_atomic_int_init(&thr->alive,       1);
   retro_atomic_int_init(&thr->stopped,     1);
   retro_atomic_int_init(&thr->stopped_ack, 0);
   retro_atomic_int_init(&thr->init_state,  AUDIO_THREAD_INIT_PENDING);

   if (!(thr->thread   = sthread_create(audio_thread_loop, thr)))
      goto error;

   /* Wait until thread has initialized (or failed) the driver, but not
    * for ever: a driver whose init() never returns, or a thread that
    * died inside it, would otherwise leave the frontend waiting here
    * with no way out. Past the deadline the thread is told it owns its
    * own state and this returns without it - freeing it here would pull
    * it out from under a thread still using it. */
   {
      bool warned         = false;
      retro_time_t giveup = cpu_features_get_time_usec()
         + AUDIO_THREAD_HANDSHAKE_GIVEUP_US;
      for (;;)
      {
         int key;
         retro_time_t now;
         if (retro_atomic_load_acquire_int(&thr->init_state)
               != AUDIO_THREAD_INIT_PENDING)
            break;
         now = cpu_features_get_time_usec();
         if (now >= giveup)
         {
            /* The thread may publish in this same instant: the
             * exchange decides, and a lost one means init() has just
             * returned and the state is this thread's after all. */
            if (retro_atomic_cas_int(&thr->init_state,
                  AUDIO_THREAD_INIT_PENDING, AUDIO_THREAD_INIT_ABANDONED))
            {
               RARCH_ERR("[Audio] Driver \"%s\" did not return from init after %d seconds; going on without audio.\n",
                     thr->driver->ident ? thr->driver->ident : "?",
                     (int)(AUDIO_THREAD_HANDSHAKE_GIVEUP_US / 1000000));
               sthread_detach(thr->thread);
               *out_driver = NULL;
               *out_data   = NULL;
               return false;
            }
            continue;
         }
         key = retro_eventcount_prepare_wait(&thr->ec);
         if (retro_atomic_load_acquire_int(&thr->init_state)
               != AUDIO_THREAD_INIT_PENDING)
         {
            retro_eventcount_cancel_wait(&thr->ec);
            break;
         }
         if (retro_eventcount_commit_wait_timeout(&thr->ec, key,
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

   if (retro_atomic_load_acquire_int(&thr->init_state)
         != AUDIO_THREAD_INIT_OK) /* Thread failed. */
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
