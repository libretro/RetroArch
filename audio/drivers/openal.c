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
#include <retro_atomic.h>
#include <string.h>
#include <time.h>

#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include <retro_miscellaneous.h>
#include <retro_timers.h>
#include <lists/string_list.h>
#include <string/stdstring.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "../audio_driver.h"
#include "../../verbosity.h"

/* AL_SOFT_events, declared here rather than through alext.h, which
 * Apple's framework does not ship. Resolved through alGetProcAddress()
 * at init, so a library without it (that framework, older builds)
 * takes the sleep-poll path below and nothing here refers to a symbol
 * it lacks. The mixer thread raises BUFFER_COMPLETED the moment a
 * queued buffer has played, which is the wake this driver otherwise
 * has to poll for. */
#define AL_EVENT_TYPE_BUFFER_COMPLETED_SOFT 0x19A4
#define AL_EVENT_TYPE_DISCONNECTED_SOFT     0x19A6
typedef void (AL_APIENTRY *al_event_proc_t)(ALenum event_type, ALuint object,
      ALuint param, ALsizei length, const ALchar *message, void *user);
typedef void (AL_APIENTRY *al_event_control_t)(ALsizei count,
      const ALenum *types, ALboolean enable);
typedef void (AL_APIENTRY *al_event_callback_t)(al_event_proc_t callback,
      void *user);

#define OPENAL_BUFSIZE 1024

typedef struct al
{
   ALuint source;
   ALuint *buffers;
   ALuint *res_buf;
   ALCdevice *handle;
   ALCcontext *ctx;
#ifdef HAVE_THREADS
   /* Signalled from the mixer thread's event callback; waited on in
    * al_get_buffer() and al_wait_writable() when events are present. */
   slock_t *lock;
   scond_t *cond;
   /* Bumped by the callback under lock; the waiter sleeps only while
    * it is unchanged, and never holds the lock across an AL call. */
   unsigned completed;
#endif
   al_event_control_t  event_control;
   al_event_callback_t event_callback;
   size_t res_ptr;
   ALsizei num_buffers;
   int rate;
   ALenum format;
   bool nonblock;
   bool is_paused;
   bool events;
   /* Raised by the DISCONNECTED event: the device is not coming back,
    * so no wait for it has anything to wait for. */
   bool disconnected;
   /* Frames the source has finished playing, for the sink rate
    * estimate; see al_frames_consumed(). */
   retro_atomic_size_t consumed;
} al_t;

static void al_free(void *data)
{
   al_t *al = (al_t*)data;

   if (!al)
      return;

   /* Before the source and context go: the mixer thread may call back
    * until the callback is cleared. */
   if (al->events && al->event_callback)
      al->event_callback(NULL, NULL);

   alSourceStop(al->source);
   alDeleteSources(1, &al->source);

   if (al->buffers)
      alDeleteBuffers(al->num_buffers, al->buffers);

   free(al->buffers);
   free(al->res_buf);
   alcMakeContextCurrent(NULL);

   if (al->ctx)
      alcDestroyContext(al->ctx);
   if (al->handle)
      alcCloseDevice(al->handle);
#ifdef HAVE_THREADS
   if (al->lock)
      slock_free(al->lock);
   if (al->cond)
      scond_free(al->cond);
#endif
   free(al);
}

#ifdef HAVE_THREADS
/* Runs on the mixer thread. Takes no AL call - the extension forbids
 * it here - and only wakes whoever is waiting for a buffer. */
static void AL_APIENTRY al_event_cb(ALenum event_type, ALuint object,
      ALuint param, ALsizei length, const ALchar *message, void *user)
{
   al_t *al = (al_t*)user;
   (void)object; (void)param; (void)length; (void)message;

   if (     event_type != AL_EVENT_TYPE_BUFFER_COMPLETED_SOFT
         && event_type != AL_EVENT_TYPE_DISCONNECTED_SOFT)
      return;

   slock_lock(al->lock);
   if (event_type == AL_EVENT_TYPE_DISCONNECTED_SOFT)
      al->disconnected = true;
   al->completed++;
   scond_signal(al->cond);
   slock_unlock(al->lock);
}

/* Resolves and arms AL_SOFT_events on the current context. Leaves
 * al->events false, and the sleep-poll in use, on any library that
 * lacks it or refuses it. */
static void al_init_events(al_t *al)
{
   ALenum types[2];
   union { void *p; al_event_control_t  f; } ctl;
   union { void *p; al_event_callback_t f; } cb;

   if (!alIsExtensionPresent("AL_SOFT_events"))
      return;
   ctl.p = alGetProcAddress("alEventControlSOFT");
   cb.p  = alGetProcAddress("alEventCallbackSOFT");
   if (!ctl.p || !cb.p)
      return;
   if (!(al->lock = slock_new()))
      return;
   if (!(al->cond = scond_new()))
      return;

   al->event_control  = ctl.f;
   al->event_callback = cb.f;
   types[0] = AL_EVENT_TYPE_BUFFER_COMPLETED_SOFT;
   types[1] = AL_EVENT_TYPE_DISCONNECTED_SOFT;
   alGetError();
   al->event_control(2, types, AL_TRUE);
   al->event_callback(al_event_cb, al);
   if (alGetError() != AL_NO_ERROR)
   {
      al->event_control  = NULL;
      al->event_callback = NULL;
      return;
   }
   al->events = true;
   RARCH_LOG("[OpenAL] Buffer completion events available; waiting on them rather than polling.\n");
}
#endif

static void *al_list_new(void *u)
{
   union string_list_elem_attr attr;
   const char *audio_out_device_list;
   struct string_list *sl = string_list_new();

   if (!sl)
      return NULL;

   attr.i = 0;

   if (alcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT"))
      audio_out_device_list = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
   else
      audio_out_device_list = alcGetString(NULL, ALC_DEVICE_SPECIFIER);

   if (audio_out_device_list)
   {
      while (*audio_out_device_list)
      {
         string_list_append(sl, audio_out_device_list, attr);
         audio_out_device_list += strlen(audio_out_device_list) + 1;
      }
   }

   return sl;
}


static void *al_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   size_t _latency;
   char *dev_id = NULL;
   al_t *al     = (al_t*)calloc(1, sizeof(al_t));
   if (!al)
      return NULL;

   retro_atomic_size_init(&al->consumed, 0);

   if (device)
   {
      struct string_list *list = (struct string_list*)al_list_new(NULL);

       /* Search for device name first */
      if (list && list->elems)
      {
         int32_t idx_found = -1;
         if (list->elems)
         {
            size_t i;
            for (i = 0; i < list->size; i++)
            {
               if (string_is_equal(device, list->elems[i].data))
               {
                  RARCH_DBG("[OpenAL] Found device #%d: \"%s\".\n", i, list->elems[i].data);
                  idx_found = (int32_t)i;
                  dev_id    = strdup(list->elems[i].data);
                  break;
               }
            }
            /* Index was not found yet based on name string,
             * just assume id is a one-character number index. */

            if (idx_found == -1 && isdigit(device[0]))
            {
               idx_found = (int32_t)strtoul(device, NULL, 0);
               RARCH_LOG("[OpenAL] Fallback, device index is a single number index instead: %d.\n", idx_found);

               if (idx_found != -1)
               {
                  if (idx_found < (int32_t)list->size)
                  {
                     RARCH_LOG("[OpenAL] Corresponding name: %s.\n", list->elems[idx_found].data);
                     dev_id    = strdup(list->elems[idx_found].data);
                  }
               }
            }
         }
      }

      string_list_free(list);
   }

   al->handle = alcOpenDevice(dev_id);
   if (dev_id)
      free(dev_id);
   dev_id = NULL;
   if (!al->handle)
      goto error;

   al->ctx = alcCreateContext(al->handle, NULL);
   if (!al->ctx)
      goto error;

   alcMakeContextCurrent(al->ctx);
#ifdef HAVE_THREADS
   al_init_events(al);
#endif

   al->rate  = rate;
   *new_rate = rate;

   if (alIsExtensionPresent("AL_EXT_FLOAT32"))
   {
      al->format      = alGetEnumValue("AL_FORMAT_STEREO_FLOAT32");
      _latency        = latency * rate * 2 * sizeof(float);
      RARCH_LOG("[OpenAL] Device supports float sample format\n");
   }
   else
   {
      al->format      = AL_FORMAT_STEREO16;
      _latency        = latency * rate * 2 * sizeof(int16_t);
   }

   al->num_buffers = (ALsizei)(_latency / (1000 * OPENAL_BUFSIZE));
   if (al->num_buffers < 2)
      al->num_buffers = 2;

   RARCH_LOG("[OpenAL] Using %u buffers of %u bytes (%s format).\n", (unsigned)al->num_buffers, OPENAL_BUFSIZE, (al->format == AL_FORMAT_STEREO16) ? "integer" : "float");

   al->buffers = (ALuint*)calloc(al->num_buffers, sizeof(ALuint));
   al->res_buf = (ALuint*)calloc(al->num_buffers, sizeof(ALuint));
   if (!al->buffers || !al->res_buf)
      goto error;

   alGenSources(1, &al->source);
   alSourcei(al->source, AL_LOOPING, AL_FALSE);
   alGenBuffers(al->num_buffers, al->buffers);

   memcpy(al->res_buf, al->buffers, al->num_buffers * sizeof(ALuint));

   al->res_ptr = al->num_buffers;

   return al;

error:
   al_free(al);
   return NULL;
}

/* The device may have gone (a disconnected default device, a lost
 * context), in which case AL_BUFFERS_PROCESSED is never delivered.
 * Bound the sleep-poll in al_get_buffer() so a write against such a
 * device returns short rather than never. */
#define OPENAL_GET_BUFFER_WAIT_MS 200
/* Granularity of the event wait: an event ends it early, so this is
 * only how often a device that sends none is re-checked. */
#define OPENAL_WAIT_STEP_MS 50

static bool al_unqueue_buffers(al_t *al)
{
   ALint val = 0;
   ALsizei room;

   alGetError();
   alGetSourcei(al->source, AL_BUFFERS_PROCESSED, &val);

   /* On a failed query val is whatever it was, so it starts at zero
    * and the error is checked; and the count is capped to the slots
    * left in res_buf, which the processed count can never exceed on a
    * healthy source but which no query result is allowed to overrun. */
   if (alGetError() != AL_NO_ERROR || val <= 0)
      return false;
   room = al->num_buffers - (ALsizei)al->res_ptr;
   if (val > room)
      val = room;
   if (val <= 0)
      return false;

   alSourceUnqueueBuffers(al->source, val, &al->res_buf[al->res_ptr]);
   if (alGetError() != AL_NO_ERROR)
      return false;
   al->res_ptr += val;
   /* Buffers the source has finished with are frames the device has
    * played. Counted here, where they are collected, rather than from
    * a callback OpenAL does not offer. */
   retro_atomic_fetch_add_size(&al->consumed,
         (size_t)val * (OPENAL_BUFSIZE
            / (al->format == AL_FORMAT_STEREO16
               ? 2 * sizeof(int16_t) : 2 * sizeof(float))));
   return true;
}

/* Sleep until at least want buffers are free in res_buf, or until the
 * bound. With events the mixer thread wakes this the moment a buffer
 * completes; without them there is nothing to wake on, so it sleeps a
 * millisecond and asks again. Returns false on the bound, on a
 * disconnected device, or at once in non-blocking mode. */
static bool al_wait_free(al_t *al, size_t want)
{
   int waited_ms = 0;

   if (al->res_ptr >= want)
      return true;
   al_unqueue_buffers(al);
   if (al->res_ptr >= want)
      return true;
   if (al->nonblock)
      return false;

#ifdef HAVE_THREADS
   if (al->events)
   {
      for (;;)
      {
         unsigned gen;
         bool gone;

         /* Read the generation, then ask the device with the lock
          * released: no AL call is ever made under it. An event that
          * lands after the read and before the wait changes the
          * generation, so the wait below is skipped rather than
          * missed. */
         slock_lock(al->lock);
         gen  = al->completed;
         gone = al->disconnected;
         slock_unlock(al->lock);

         al_unqueue_buffers(al);
         if (al->res_ptr >= want)
            return true;
         if (gone || waited_ms >= OPENAL_GET_BUFFER_WAIT_MS)
            return false;

         slock_lock(al->lock);
         if (al->completed == gen && !al->disconnected)
         {
            if (!scond_wait_timeout(al->cond, al->lock,
                     (int64_t)OPENAL_WAIT_STEP_MS * 1000))
               waited_ms += OPENAL_WAIT_STEP_MS;
         }
         slock_unlock(al->lock);
      }
   }
#endif

   /* No events: sleep-poll. A device that processes nothing within
    * the bound has stopped, and the caller gets what it managed. */
   while (al->res_ptr < want)
   {
      if (waited_ms >= OPENAL_GET_BUFFER_WAIT_MS)
         return false;
      retro_sleep(1);
      waited_ms++;
      al_unqueue_buffers(al);
   }
   return true;
}

static bool al_get_buffer(al_t *al, ALuint *buffer)
{
   if (!al_wait_free(al, 1))
      return false;
   *buffer = al->res_buf[--al->res_ptr];
   return true;
}

static ssize_t al_write(void *data, const void *s, size_t len)
{
   al_t           *al = (al_t*)data;
   const uint8_t *buf = (const uint8_t*)s;
   size_t        _len = 0;

   while (len)
   {
      ALuint buffer;
      size_t rc    = MIN(OPENAL_BUFSIZE, len);

      if (!al_get_buffer(al, &buffer))
         break;

      alBufferData(buffer, al->format, buf, (ALsizei)rc, al->rate);
      alSourceQueueBuffers(al->source, 1, &buffer);

      _len           += rc;
      buf            += rc;
      len            -= rc;
   }

   /* Kick the source once, after everything is queued, rather than
    * once per OPENAL_BUFSIZE chunk.  Queueing onto a stopped source
    * does not start it, so the restart still happens before this call
    * returns; it just happens with the whole write already queued
    * instead of partway through it.  Skipped entirely when nothing was
    * queued, which is what the per-chunk form did too. */
   if (_len)
   {
      ALint val;
      alGetSourcei(al->source, AL_SOURCE_STATE, &val);
      if (val != AL_PLAYING)
         alSourcePlay(al->source);
   }

   return _len;
}

static bool al_stop(void *data)
{
   al_t *al = (al_t*)data;
   if (al)
      al->is_paused = true;
   return true;
}

static bool al_alive(void *data)
{
   al_t *al = (al_t*)data;
   return al && !al->is_paused;
}

static void al_set_nonblock_state(void *data, bool state)
{
   al_t *al = (al_t*)data;
   if (al)
      al->nonblock = state;
}

static bool al_start(void *data, bool is_shutdown)
{
   al_t *al = (al_t*)data;
   if (al)
      al->is_paused = false;
   return true;
}

static size_t al_write_avail(void *data)
{
   al_t *al = (al_t*)data;
   al_unqueue_buffers(al);
   return al->res_ptr * OPENAL_BUFSIZE;
}

static size_t al_buffer_size(void *data)
{
   al_t *al = (al_t*)data;
   return (al->num_buffers) * OPENAL_BUFSIZE;
}

/* Sleep until at least len bytes fit, capped at half the buffers so
 * the wait always has an end within a healthy device's reach. Returns
 * the free space then, or 0 when none came within the bound - a pass
 * to skip, per the wait_writable() contract - so the threaded
 * pipeline can pace on the device instead of the inline path. */
static size_t al_wait_writable(void *data, size_t len)
{
   al_t *al    = (al_t*)data;
   size_t want = (len + OPENAL_BUFSIZE - 1) / OPENAL_BUFSIZE;
   size_t half = (size_t)al->num_buffers / 2;

   if (want < 1)
      want = 1;
   if (want > half)
      want = half;
   if (!al_wait_free(al, want))
      return 0;
   return al->res_ptr * OPENAL_BUFSIZE;
}

/* Frames the device has played since the source started.
 *
 * OpenAL has no callback per period, so this counts buffers as they
 * come back processed - the same quantity OpenSL gets from its
 * callback, collected on the frontend's thread instead. A buffer is
 * counted once, when it is first seen as processed, so the running
 * total stays right even though the moment of observation is the
 * frontend's rather than the device's: a late look moves when a frame
 * is counted, not how many there are.
 *
 * AL_SAMPLE_OFFSET would give the position inside the current buffer
 * too, but it is per-buffer rather than cumulative and would have to
 * be stitched to this count to be useful. The buffer quantum here is
 * 256 frames of int16 stereo, finer than the 480-frame periods the
 * estimator is already specified to tolerate, so the extra resolution
 * would buy nothing.
 */
static size_t al_frames_consumed(void *data)
{
   al_t *al = (al_t*)data;
   if (!al)
      return 0;
   return retro_atomic_load_acquire_size(&al->consumed);
}

static bool al_use_float(void *data)
{
   al_t *al = (al_t*)data;
   if (al->format == AL_FORMAT_STEREO16)
      return false;
   return true;
}

static void al_device_list_free(void *u, void *slp)
{
   struct string_list *sl = (struct string_list*)slp;

   if (sl)
      string_list_free(sl);
}

audio_driver_t audio_openal = {
   al_init,
   al_write,
   al_stop,
   al_start,
   al_alive,
   al_set_nonblock_state,
   al_free,
   al_use_float,
   "openal",
   al_list_new,
   al_device_list_free,
   al_write_avail,
   al_buffer_size,
   NULL, /* write_raw */
   al_wait_writable,
   al_frames_consumed
};
