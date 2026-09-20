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

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>
#include <retro_spsc.h>
#include <retro_inline.h>
#include <retro_math.h>

#include "SDL.h"

/* Upper bound on how long a blocking write will wait for the SDL
 * callback before giving up and reporting a short count.  Never
 * reached in normal operation - the callback notifies every period -
 * so the exact value only decides how long a stalled device takes to
 * be noticed. */
#define SDL_AUDIO_STALL_TIMEOUT_US 256000
#include "SDL_audio.h"

#include "../audio_driver.h"
#include "../../verbosity.h"

/* Audio format bit macros, introduced in SDL 2. */
#define SDL_AUDIO_MASK_BITSIZE       (0xFF)
#define SDL_AUDIO_MASK_DATATYPE      (1<<8)
#define SDL_AUDIO_MASK_ENDIAN        (1<<12)
#define SDL_AUDIO_MASK_SIGNED        (1<<15)
#define SDL_AUDIO_BITSIZE(x)         (x & SDL_AUDIO_MASK_BITSIZE)
#define SDL_AUDIO_ISFLOAT(x)         (x & SDL_AUDIO_MASK_DATATYPE)
#define SDL_AUDIO_ISBIGENDIAN(x)     (x & SDL_AUDIO_MASK_ENDIAN)
#define SDL_AUDIO_ISSIGNED(x)        (x & SDL_AUDIO_MASK_SIGNED)

/* Frames per device buffer for the given slice of the latency setting:
 * a power of two, as SDL prefers, rounded down so the device stage
 * never exceeds its share, with a floor a device will accept. Rounding
 * up put a 16 ms share at 21.3 ms on a 48 kHz device. */
static INLINE int sdl1_audio_find_num_frames(int rate, int latency)
{
   int frames = (rate * latency) / 1000;
   int pow2   = (int)prev_pow2((uint32_t)frames);
   if (pow2 < 64)
      pow2    = 64;
   return pow2;
}

/* Room in a ring against its logical size: the physical room less the
 * capacity that lies beyond the size asked for (retro_spsc rounds up
 * to a power of two). */
static size_t sdl1_ring_room(const retro_spsc_t *ring, size_t ring_size)
{
   size_t room   = retro_spsc_write_avail(ring);
   size_t excess = ring->capacity - ring_size;
   return room > excess ? room - excess : 0;
}

typedef struct sdl1_audio
{
#ifdef HAVE_THREADS
   /* Only the bounded waits a short or full ring puts the other side
    * into; the ring itself is a retro_spsc and carries the data on its
    * own, so there is nothing here for a lock to guard.
    *
    * An eventcount rather than a lock and a condition variable because
    * SDL's callback runs on the device's own thread and announced its
    * work without taking the lock - which left a signal raised between
    * the waiter's avail test and its wait with no waiter to reach it.
    * prepare_wait registers before the re-check, so a notify from that
    * point on either shows up in the re-check or ends the park at once,
    * and the window cannot be missed. */
   retro_eventcount_t park;
#endif
   /* Outgoing samples.  Single producer (the core thread in
    * sdl1_audio_write), single consumer (SDL's playback thread in
    * sdl1_audio_playback_cb): a lock-free retro_spsc ring, so the
    * writer no longer has to SDL_LockAudio() - i.e. stall the playback
    * callback - to push samples.  speaker_ring_size is the size asked
    * for; see sdl1_ring_room. */
   retro_spsc_t  speaker_ring;
   size_t        speaker_ring_size;
   /* Bytes the callback has handed the device, and the periods it had
    * to zero-fill for want of audio. Both are bumped on the device's
    * own thread and read by the frontend. */
   retro_atomic_size_t consumed_bytes;
   retro_atomic_size_t underruns;
   SDL_AudioSpec device_spec;
   uint32_t      layout;   /* the layout asked for, reported when the count matched */
   bool          speaker_ring_init;
   bool          speaker_open;
   bool          nonblock;
   bool          is_paused;
} sdl1_audio_t;

static void sdl1_audio_playback_cb(void *data, Uint8 *stream, int len)
{
   sdl1_audio_t *sdl = (sdl1_audio_t*)data;
   size_t       _len = retro_spsc_read(&sdl->speaker_ring, stream, (size_t)len);
#ifdef HAVE_THREADS
   retro_eventcount_notify(&sdl->park);
#endif
   /* Device time, silence included: the period elapsed either way. */
   retro_atomic_fetch_add_size(&sdl->consumed_bytes, (size_t)len);
   /* If underrun, fill rest with silence. */
   if (_len < (size_t)len)
   {
      retro_atomic_fetch_add_size(&sdl->underruns, 1);
      memset(stream + _len, 0, (size_t)len - _len);
   }
}

static void sdl1_audio_free(void *data);

static void *sdl1_audio_init(const char *device,
      unsigned rate, unsigned latency,
       unsigned *new_rate)
{
   int frames;
   size_t bufsize;
   SDL_AudioSpec spec           = {0};
   void *tmp                    = NULL;
   sdl1_audio_t *sdl            = NULL;
   uint32_t sdl_subsystem_flags = SDL_WasInit(0);

   /* Initialise audio subsystem, if required */
   if (sdl_subsystem_flags == 0)
   {
      if (SDL_Init(SDL_INIT_AUDIO) < 0)
         return NULL;
   }
   else if ((sdl_subsystem_flags & SDL_INIT_AUDIO) == 0)
   {
      if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
         return NULL;
   }

   sdl = (sdl1_audio_t*)calloc(1, sizeof(*sdl));
   if (!sdl)
      return NULL;

   /* Before the device can call back. */
   retro_atomic_size_init(&sdl->consumed_bytes, 0);
   retro_atomic_size_init(&sdl->underruns, 0);

   /* We have to buffer up some data ourselves, so we let SDL
    * carry approximately half of the latency.
    *
    * SDL double buffers audio and we do as well. */
   frames        = sdl1_audio_find_num_frames(rate, latency / 4);

   /* First, let's initialize the output device. */
   spec.freq     = rate;
   /* SDL 1.2 output is int16 only. */
   spec.format   = AUDIO_S16SYS;
   /* The channels of the layout the frontend wants; SDL converts if
    * the device lacks them, so what it grants is what the frontend
    * gets, in SDL's fixed interleaved order for the count - which is
    * the layout's ascending-bit order. SDL does not say whether a
    * six-channel device's rear pair is at the back or the sides; it
    * takes the frames in that order and maps them itself, so the
    * layout reported is the one asked for. */
   sdl->layout   = audio_driver_requested_layout();
   spec.channels = (Uint8)audio_layout_channels(sdl->layout);
   spec.samples  = frames; /* This is in audio frames, not samples ... :( */
   spec.callback = sdl1_audio_playback_cb;
   spec.userdata = sdl;

   if (SDL_OpenAudio(&spec, &sdl->device_spec) < 0)
   {
      RARCH_ERR("[SDL audio] Failed to open SDL audio output device: %s.\n", SDL_GetError());
      free(sdl);
      return NULL;
   }
   sdl->speaker_open        = true;

   *new_rate                = sdl->device_spec.freq;
   RARCH_DBG("[SDL audio] Requested a speaker frequency of %u Hz, received %u Hz.\n",
             spec.freq, sdl->device_spec.freq);
   RARCH_DBG("[SDL audio] Requested %u channels for speaker, received %u.\n",
             spec.channels, sdl->device_spec.channels);
   RARCH_DBG("[SDL audio] Requested a %u-frame speaker buffer, received %u frames (%u bytes).\n",
             frames, sdl->device_spec.samples, sdl->device_spec.size);
   RARCH_DBG("[SDL audio] Got a speaker silence value of %u.\n", sdl->device_spec.silence);
   RARCH_DBG("[SDL audio] Requested speaker audio format: %u-bit %s %s %s endian.\n",
             SDL_AUDIO_BITSIZE(spec.format),
             SDL_AUDIO_ISSIGNED(spec.format) ? "signed" : "unsigned",
             SDL_AUDIO_ISFLOAT(spec.format) ? "floating-point" : "integer",
             SDL_AUDIO_ISBIGENDIAN(spec.format) ? "big" : "little");
   RARCH_DBG("[SDL audio] Received speaker audio format: %u-bit %s %s %s endian.\n",
             SDL_AUDIO_BITSIZE(sdl->device_spec.format),
             SDL_AUDIO_ISSIGNED(sdl->device_spec.format) ? "signed" : "unsigned",
             SDL_AUDIO_ISFLOAT(sdl->device_spec.format) ? "floating-point" : "integer",
             SDL_AUDIO_ISBIGENDIAN(sdl->device_spec.format) ? "big" : "little");

#ifdef HAVE_THREADS
   /* A failed init is not a wait that degrades, it is a NULL condition
    * variable handed to scond_wait. */
   if (!retro_eventcount_init(&sdl->park))
   {
      RARCH_ERR("[SDL audio] Could not create the speaker's park.\n");
      sdl1_audio_free(sdl);
      return NULL;
   }
#endif

   /* The fifo in front of the device holds the latency setting, as the
    * other drivers' buffers do; the device buffer behind it, a quarter
    * of the setting rounded down to a power of two, is SDL's and adds
    * on top, unmeasured. The fifo is never smaller than two device
    * buffers, so the callback always has a buffer's worth in hand. */
   {
      size_t frame_bytes = (size_t)sdl->device_spec.channels
            * (SDL_AUDIO_BITSIZE(sdl->device_spec.format) / 8);
      size_t fifo_frames = ((size_t)(*new_rate) * latency) / 1000;
      if (fifo_frames < (size_t)sdl->device_spec.samples * 2)
         fifo_frames     = (size_t)sdl->device_spec.samples * 2;
      bufsize            = fifo_frames * frame_bytes;
   }

   RARCH_LOG("[SDL audio] Requested %u ms latency: %u ms fifo, plus a %u-frame (%u ms) device buffer.\n",
         latency,
         (unsigned)(bufsize / (2 * (SDL_AUDIO_BITSIZE(sdl->device_spec.format) / 8)) * 1000 / (*new_rate)),
         (unsigned)sdl->device_spec.samples,
         (unsigned)(sdl->device_spec.samples * 1000 / (*new_rate)));

   tmp                    = calloc(1, bufsize);
   sdl->speaker_ring_size = bufsize;
   sdl->speaker_ring_init = retro_spsc_init(&sdl->speaker_ring, bufsize);

   /* Bail on OOM. */
   if (!sdl->speaker_ring_init)
   {
      free(tmp);
      sdl1_audio_free(sdl);
      return NULL;
   }

   if (tmp)
   {
      retro_spsc_write(&sdl->speaker_ring, tmp, bufsize);
      free(tmp);
   }

   RARCH_DBG("[SDL audio] Initialized speaker sample queue with %u bytes.\n", bufsize);

   /* SDL 1.2 opens the device with its thread already running and
    * unpauses with a plain flag write, so the ring and the park set up
    * above reach that thread with nothing ordering them. Its own lock,
    * which the thread takes around every period, is the edge: a period
    * that sees the device unpaused ran after this unlock. SDL2 does
    * this itself; 1.2 does not, and the handhelds this driver serves
    * are not x86. */
   SDL_LockAudio();
   SDL_PauseAudio(false);
   SDL_UnlockAudio();

   return sdl;
}

static ssize_t sdl1_audio_write(void *data, const void *s, size_t len)
{
   size_t _len       = 0;
   sdl1_audio_t *sdl = (sdl1_audio_t*)data;

   /* If we shouldn't wait for space in a full outgoing sample queue... */
   if (sdl->nonblock)
   {
      size_t avail     = sdl1_ring_room(&sdl->speaker_ring, sdl->speaker_ring_size);
      size_t write_amt = (avail > len) ? len : avail; /* Enqueue as much data as we can */
      retro_spsc_write(&sdl->speaker_ring, s, write_amt);
      _len             = write_amt; /* If the queue was full...well, too bad. */
   }
   else
   {
      /* Until we've written all the sample data we have available... */
      while (_len < len)
      {
         size_t avail;
#ifdef HAVE_THREADS
         bool signalled;
#endif

         avail = sdl1_ring_room(&sdl->speaker_ring, sdl->speaker_ring_size);

         /* If the outgoing sample queue is full... */
         if (avail == 0)
         {
            /* Wait for the SDL speaker thread to play the enqueued samples,
             * which will free up space for us to write new ones. */
#ifdef HAVE_THREADS
            /* Registered before the room is read again: a callback
             * that pulls between the avail test above and this park
             * either shows up in the re-check or ends the park at once.
             * Still bounded, because a device that has stopped calling
             * back has no next callback and an unbounded park here held
             * the core's thread for good. */
            int key   = retro_eventcount_prepare_wait(&sdl->park);
            if (sdl1_ring_room(&sdl->speaker_ring, sdl->speaker_ring_size))
            {
               retro_eventcount_cancel_wait(&sdl->park);
               signalled = true;
            }
            else
               signalled = retro_eventcount_commit_wait_timeout(&sdl->park,
                     key, SDL_AUDIO_STALL_TIMEOUT_US);
            /* Now let this thread use the outgoing sample queue (which we'll do next iteration) */
            if (!signalled)
               break;   /* Report what we managed to enqueue */
#else
            /* Without threads there is nothing to wait on, and spinning
             * here never made progress either. */
            break;
#endif
         }
         else
         {
            size_t write_amt = len - _len > avail ? avail : len - _len;
            retro_spsc_write(&sdl->speaker_ring, (const char*)s + _len, write_amt);
            /* Enqueue as many samples as we have available without overflowing the queue */
            _len += write_amt;
         }
      }
   }

   return _len;
}

static bool sdl1_audio_stop(void *data)
{
   sdl1_audio_t *sdl = (sdl1_audio_t*)data;
   sdl->is_paused    = true;
   SDL_PauseAudio(true);
   return true;
}

static bool sdl1_audio_alive(void *data)
{
   sdl1_audio_t *sdl = (sdl1_audio_t*)data;
   if (!sdl)
      return false;
   return !sdl->is_paused;
}

static bool sdl1_audio_start(void *data, bool is_shutdown)
{
   sdl1_audio_t *sdl = (sdl1_audio_t*)data;
   sdl->is_paused    = false;
   SDL_PauseAudio(false);
   return true;
}

static void sdl1_audio_set_nonblock_state(void *data, bool state)
{
   sdl1_audio_t *sdl = (sdl1_audio_t*)data;
   if (sdl)
      sdl->nonblock = state;
}

static void sdl1_audio_free(void *data)
{
   sdl1_audio_t *sdl = (sdl1_audio_t*)data;

   if (sdl)
   {
      if (sdl->speaker_open)
         SDL_CloseAudio();

      if (sdl->speaker_ring_init)
         retro_spsc_free(&sdl->speaker_ring);

#ifdef HAVE_THREADS
      retro_eventcount_free(&sdl->park);
#endif

      SDL_QuitSubSystem(SDL_INIT_AUDIO);
   }
   free(sdl);
}

static uint32_t sdl1_audio_layout(void *data)
{
   sdl1_audio_t *sdl = (sdl1_audio_t*)data;
   if (!sdl || sdl->device_spec.channels != audio_layout_channels(sdl->layout))
      return AUDIO_LAYOUT_STEREO;
   return sdl->layout;
}

static bool sdl1_audio_use_float(void *data)
{
   sdl1_audio_t *sdl = (sdl1_audio_t*)data;
   return SDL_AUDIO_ISFLOAT(sdl->device_spec.format) ? true : false;
}

static size_t sdl1_audio_write_avail(void *data)
{
   sdl1_audio_t *sdl = (sdl1_audio_t*)data;
   return sdl1_ring_room(&sdl->speaker_ring, sdl->speaker_ring_size);
}

static size_t sdl1_audio_buffer_size(void *data)
{
   sdl1_audio_t *sdl = (sdl1_audio_t*)data;
   /* The size asked for, which is what write_avail() can reach; the
    * ring's physical capacity may be larger (power of two). */
   return sdl->speaker_ring_size;
}

/* Park on the eventcount the speaker callback notifies after every
 * pull, until at least len bytes fit in the outgoing queue, capped at
 * half of it so the wait always ends. Returns the free space then, or 0
 * when the thread has gone quiet for the stall timeout (device lost)
 * or there is no thread to wait on. */
static size_t sdl1_audio_wait_writable(void *data, size_t len)
{
   size_t avail;
   sdl1_audio_t *sdl = (sdl1_audio_t*)data;
   /* Each wait ends on a timeout; this ends the loop when the device
    * keeps calling back but never frees enough. */
   int laps = 8;

   if (len > sdl->speaker_ring_size / 2)
      len = sdl->speaker_ring_size / 2;

   for (;;)
   {
#ifdef HAVE_THREADS
      bool signalled;
#endif
      if (laps-- < 0)
         break;
      avail = sdl1_ring_room(&sdl->speaker_ring, sdl->speaker_ring_size);
      if (avail >= len)
         return avail;
#ifdef HAVE_THREADS
      {
         int key = retro_eventcount_prepare_wait(&sdl->park);
         if (sdl1_ring_room(&sdl->speaker_ring, sdl->speaker_ring_size) >= len)
         {
            retro_eventcount_cancel_wait(&sdl->park);
            signalled = true;
         }
         else
            signalled = retro_eventcount_commit_wait_timeout(&sdl->park,
                  key, SDL_AUDIO_STALL_TIMEOUT_US);
      }
      if (!signalled)
         break;
#else
      break;
#endif
   }
   return 0;
}

/* Frames the device has taken since the device was opened. The
 * callback is the device asking for exactly one period, so what it
 * asks for is device time - the same way coreaudio and the WASAPI
 * pump count it. Bytes are counted on that path and turned into
 * frames here, off it. */
static size_t sdl1_audio_frames_consumed(void *data)
{
   sdl1_audio_t *sdl = (sdl1_audio_t*)data;
   size_t frame_bytes;

   if (!sdl)
      return 0;

   frame_bytes = (size_t)sdl->device_spec.channels
         * (SDL_AUDIO_BITSIZE(sdl->device_spec.format) / 8);
   if (!frame_bytes)
      return 0;

   return retro_atomic_load_acquire_size(&sdl->consumed_bytes) / frame_bytes;
}

static size_t sdl1_audio_underruns(void *data)
{
   sdl1_audio_t *sdl = (sdl1_audio_t*)data;
   return sdl ? retro_atomic_load_acquire_size(&sdl->underruns) : 0;
}

audio_driver_t audio_sdl1 = {
   sdl1_audio_init,
   sdl1_audio_write,
   sdl1_audio_stop,
   sdl1_audio_start,
   sdl1_audio_alive,
   sdl1_audio_set_nonblock_state,
   sdl1_audio_free,
   sdl1_audio_use_float,
   "sdl",
   NULL, /* device_list_new - SDL 1.2 has no device enumeration */
   NULL, /* device_list_free */
   sdl1_audio_write_avail,
   sdl1_audio_buffer_size,
   NULL, /* write_raw */
   sdl1_audio_wait_writable,
   sdl1_audio_frames_consumed,
   sdl1_audio_underruns,
   sdl1_audio_layout
};
