/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2015 - Michael Lelli
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2025      - OlyB
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
#include <malloc.h>   /* memalign(), as in audioworklet.c */
#include <unistd.h>
#include <boolean.h>
#include <retro_timers.h>

#include "../audio_driver.h"
#include "../../verbosity.h"
#include "../../frontend/drivers/platform_emscripten.h"

#define RWEBAUDIO_BUFFER_SIZE_MS 10

/* forward declarations */
unsigned RWebAudioSampleRate(void);
unsigned RWebAudioInit(unsigned latency, unsigned channels);
ssize_t RWebAudioQueueBuffer(size_t num_frames, float *planes, size_t plane_frames);
bool RWebAudioStop(void);
bool RWebAudioStart(void);
void RWebAudioSetNonblockState(bool state);
void RWebAudioFree(void);
size_t RWebAudioWriteAvailFrames(void);
size_t RWebAudioBufferSizeFrames(void);
void RWebAudioRecalibrateTime(void);
bool RWebAudioResumeCtx(void);

typedef struct rwebaudio_data
{
   size_t tmpbuf_frames;
   size_t tmpbuf_offset;
   /* one plane of tmpbuf_frames per channel */
   float *tmpbuf;
   uint32_t layout;
   unsigned channels;
   bool nonblock;
   bool running;
#ifdef EMSCRIPTEN_AUDIO_FAKE_BLOCK
   bool block_requested;
#endif
} rwebaudio_data_t;

static rwebaudio_data_t *rwebaudio_static_data = NULL;

static void rwebaudio_free(void *data)
{
   rwebaudio_data_t *rwebaudio = (rwebaudio_data_t*)data;
   if (!rwebaudio)
      return;

   RWebAudioFree();
   if (rwebaudio->tmpbuf)
      free(rwebaudio->tmpbuf);
   free(rwebaudio);
   rwebaudio_static_data = NULL;
}

static void *rwebaudio_init(const char *device, unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   rwebaudio_data_t *rwebaudio;
   uint32_t want = audio_driver_requested_layout();
   unsigned channels;
   if (rwebaudio_static_data)
   {
      RARCH_ERR("[RWebAudio] Tried to start already running driver.\n");
      return NULL;
   }

   rwebaudio = (rwebaudio_data_t*)calloc(1, sizeof(rwebaudio_data_t));
   if (!rwebaudio)
      return NULL;
   /* The layout asked for, as wide as the page's output goes: 8, 6
    * or 4 channels in the WAVE order, the frontend's own. */
   channels = audio_layout_supported(want) ? audio_layout_channels(want) : 2;
   if (!(channels = RWebAudioInit(latency, channels)))
   {
      RARCH_ERR("[RWebAudio] Failed to initialize driver.\n");
      free(rwebaudio);
      return NULL;
   }
   rwebaudio->channels      = channels;
   rwebaudio->layout        = (channels == 8) ? AUDIO_LAYOUT_7POINT1
                            : (channels == 6) ? AUDIO_LAYOUT_5POINT1
                            : (channels == 4) ? AUDIO_LAYOUT_QUAD
                            : AUDIO_LAYOUT_STEREO;
   rwebaudio_static_data    = rwebaudio;
   *new_rate                = RWebAudioSampleRate();
   rwebaudio->tmpbuf_frames = RWEBAUDIO_BUFFER_SIZE_MS * *new_rate / 1000;
   rwebaudio->tmpbuf        = memalign(16, rwebaudio->tmpbuf_frames
         * channels * sizeof(float));
   /* rwebaudio_write() deinterleaves into these on every call with no
    * test of its own, so a refused allocation has to stop the driver
    * here rather than hand back a context that faults on first use. */
   if (!rwebaudio->tmpbuf)
   {
      RARCH_ERR("[RWebAudio] Failed to allocate the mixing buffers.\n");
      rwebaudio_free(rwebaudio);
      return NULL;
   }
   RARCH_LOG("[RWebAudio] Device rate: %d Hz.\n", *new_rate);
   RARCH_LOG("[RWebAudio] Buffer size: %lu bytes, layout 0x%03x.\n",
         RWebAudioBufferSizeFrames() * channels * sizeof(float),
         (unsigned)rwebaudio->layout);
   return rwebaudio;
}

/* How many times a blocking write asks the context for room before
 * dropping the rest. With ASYNCIFY each lap sleeps a millisecond, so
 * this is about two seconds; without it the laps are resume attempts
 * back to back on the page's thread, so it is a count that ends the
 * spin within a few milliseconds. */
#if defined(EMSCRIPTEN_FULL_ASYNCIFY) || defined(EMSCRIPTEN_AUDIO_ASYNC_BLOCK)
#define RWEBAUDIO_WAIT_LAPS 2000
#else
#define RWEBAUDIO_WAIT_LAPS 100000
#endif

static ssize_t rwebaudio_write(void *data, const void *s, size_t len)
{
   rwebaudio_data_t *rwebaudio = (rwebaudio_data_t*)data;
   const float *samples = (const float*)s;
   size_t num_frames, total_frames;
   unsigned c;
   if (!rwebaudio)
      return -1;
   num_frames   = len / rwebaudio->channels / sizeof(float);
   total_frames = num_frames;

   while (num_frames)
   {
      for (c = 0; c < rwebaudio->channels; c++)
         rwebaudio->tmpbuf[c * rwebaudio->tmpbuf_frames
               + rwebaudio->tmpbuf_offset] = *(samples++);
      num_frames--;
      if (++rwebaudio->tmpbuf_offset == rwebaudio->tmpbuf_frames)
      {
         size_t queued            = RWebAudioQueueBuffer(
               rwebaudio->tmpbuf_frames,
               rwebaudio->tmpbuf,
               rwebaudio->tmpbuf_frames);
         rwebaudio->tmpbuf_offset = 0;
         /* fast-forward or context is suspended */
         if (queued < rwebaudio->tmpbuf_frames)
            break;
      }
   }

   if (!rwebaudio->nonblock)
   {
#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_WRITE_BLOCK
#ifdef EMSCRIPTEN_AUDIO_FAKE_BLOCK
      if (RWebAudioWriteAvailFrames() == 0)
      {
         rwebaudio->block_requested = true;
         platform_emscripten_enter_fake_block(1);
      }
#endif
      /* async external block doesn't need to do anything else */
#else
      {
         /* Bounded: a context that will not run - suspended by the
          * autoplay policy until the page is clicked, or by the browser
          * for a background tab - frees nothing, and without ASYNCIFY
          * this loop is a spin on the page's own thread. It ends, and
          * the audio is dropped, rather than the tab freezing. */
         unsigned laps = RWEBAUDIO_WAIT_LAPS;
         while (RWebAudioWriteAvailFrames() == 0 && laps--)
         {
#ifdef EMSCRIPTEN_FULL_ASYNCIFY
            retro_sleep(1);
#endif
            RWebAudioResumeCtx();
         }
      }
#endif
   }

   /* Return bytes consumed from the input, like every other audio
    * driver (the previous code returned queued *frames*, and zero for
    * anything parked in a partially filled tmpbuf).  Frames awaiting a
    * full block are consumed - a later call queues them.  Frames the
    * context dropped on a short queue (fast-forward / suspended) were
    * still drained from the caller's buffer, so they count too; the
    * remaining num_frames were never touched and do not. */
   return (ssize_t)((total_frames - num_frames) * rwebaudio->channels * sizeof(float));
}

#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_BLOCK
/* returns true if fake block should continue */
bool rwebaudio_external_block(void)
{
   rwebaudio_data_t *rwebaudio = rwebaudio_static_data;

   if (!rwebaudio)
      return false;

#ifdef EMSCRIPTEN_AUDIO_FAKE_BLOCK
   if (!rwebaudio->block_requested)
      return false;
#endif

#ifdef EMSCRIPTEN_AUDIO_EXTERNAL_WRITE_BLOCK
   {
      unsigned laps = RWEBAUDIO_WAIT_LAPS;
      while (!rwebaudio->nonblock && RWebAudioWriteAvailFrames() == 0)
      {
         RWebAudioResumeCtx();
#ifdef EMSCRIPTEN_AUDIO_ASYNC_BLOCK
         retro_sleep(1);
         /* Bounded, as the write's own wait is. */
         if (!laps--)
            break;
#else
         (void)laps;
         return true;
#endif
      }
   }
#endif

#ifdef EMSCRIPTEN_AUDIO_FAKE_BLOCK
   rwebaudio->block_requested = false;
   platform_emscripten_exit_fake_block();
   return true; /* return to RAF if needed */
#endif
   return false;
}
#endif

void rwebaudio_recalibrate_time(void)
{
   if (rwebaudio_static_data)
      RWebAudioRecalibrateTime();
}

static bool rwebaudio_stop(void *data)
{
   rwebaudio_data_t *rwebaudio = (rwebaudio_data_t*)data;
   if (!rwebaudio)
      return false;
   rwebaudio->running = false;
   return RWebAudioStop();
}

static bool rwebaudio_start(void *data, bool is_shutdown)
{
   rwebaudio_data_t *rwebaudio = (rwebaudio_data_t*)data;
   if (!rwebaudio)
      return false;
   rwebaudio->running = true;
   return RWebAudioStart();
}

static bool rwebaudio_alive(void *data)
{
   rwebaudio_data_t *rwebaudio = (rwebaudio_data_t*)data;
   if (!rwebaudio)
      return false;
   return rwebaudio->running;
}

static void rwebaudio_set_nonblock_state(void *data, bool state)
{
   rwebaudio_data_t *rwebaudio = (rwebaudio_data_t*)data;
   if (!rwebaudio)
      return;
   rwebaudio->nonblock = state;
   RWebAudioSetNonblockState(state);
}

static size_t rwebaudio_write_avail(void *data)
{
   rwebaudio_data_t *rwebaudio = (rwebaudio_data_t*)data;
   if (rwebaudio)
   {
      size_t avail_frames = RWebAudioWriteAvailFrames();
      if (avail_frames > rwebaudio->tmpbuf_offset)
         return (avail_frames - rwebaudio->tmpbuf_offset)
               * rwebaudio->channels * sizeof(float);
   }
   return 0;
}

static size_t rwebaudio_buffer_size(void *data)
{
   rwebaudio_data_t *rwebaudio = (rwebaudio_data_t*)data;
   return RWebAudioBufferSizeFrames()
         * (rwebaudio ? rwebaudio->channels : 2) * sizeof(float);
}

static bool rwebaudio_use_float(void *data) { return true; }

static uint32_t rwebaudio_layout(void *data)
{
   rwebaudio_data_t *rwebaudio = (rwebaudio_data_t*)data;
   return rwebaudio ? rwebaudio->layout : AUDIO_LAYOUT_STEREO;
}

audio_driver_t audio_rwebaudio = {
   rwebaudio_init,
   rwebaudio_write,
   rwebaudio_stop,
   rwebaudio_start,
   rwebaudio_alive,
   rwebaudio_set_nonblock_state,
   rwebaudio_free,
   rwebaudio_use_float,
   "rwebaudio",
   NULL,
   NULL,
   rwebaudio_write_avail,
   rwebaudio_buffer_size,
   NULL, /* write_raw */
   NULL, /* wait_writable */
   NULL, /* frames_consumed */
   NULL, /* underruns */
   rwebaudio_layout
};
