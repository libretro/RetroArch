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
#include <unistd.h>
#include <boolean.h>
#include <retro_timers.h>

#include "../audio_driver.h"
#include "../../verbosity.h"
#include "../../frontend/drivers/platform_emscripten.h"

#define RWEBAUDIO_BUFFER_SIZE_MS 10

/* forward declarations */
unsigned RWebAudioSampleRate(void);
void *RWebAudioInit(unsigned latency);
ssize_t RWebAudioQueueBuffer(size_t num_frames, float *left, float *right);
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
   float *tmpbuf_left;
   float *tmpbuf_right;
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
   if (rwebaudio->tmpbuf_left)
      free(rwebaudio->tmpbuf_left);
   if (rwebaudio->tmpbuf_right)
      free(rwebaudio->tmpbuf_right);
   free(rwebaudio);
   rwebaudio_static_data = NULL;
}

static void *rwebaudio_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   rwebaudio_data_t *rwebaudio;
   if (rwebaudio_static_data)
   {
      RARCH_ERR("[RWebAudio] Tried to start already running driver.\n");
      return NULL;
   }

   rwebaudio = (rwebaudio_data_t*)calloc(1, sizeof(rwebaudio_data_t));
   if (!rwebaudio)
      return NULL;
   if (!RWebAudioInit(latency))
   {
      RARCH_ERR("[RWebAudio] Failed to initialize driver.\n");
      return NULL;
   }
   rwebaudio_static_data = rwebaudio;
   *new_rate = RWebAudioSampleRate();
   rwebaudio->tmpbuf_frames = RWEBAUDIO_BUFFER_SIZE_MS * *new_rate / 1000;
   rwebaudio->tmpbuf_left   = memalign(sizeof(float), rwebaudio->tmpbuf_frames * sizeof(float));
   rwebaudio->tmpbuf_right  = memalign(sizeof(float), rwebaudio->tmpbuf_frames * sizeof(float));
   RARCH_LOG("[RWebAudio] Device rate: %d Hz.\n", *new_rate);
   RARCH_LOG("[RWebAudio] Buffer size: %lu bytes.\n", RWebAudioBufferSizeFrames() * 2 * sizeof(float));
   return rwebaudio;
}

static ssize_t rwebaudio_write(void *data, const void *s, size_t len)
{
   rwebaudio_data_t *rwebaudio = (rwebaudio_data_t*)data;
   const float *samples = (const float*)s;
   size_t num_frames = len / 2 / sizeof(float);
   size_t written = 0;
   if (!rwebaudio)
      return -1;

   while (num_frames)
   {
      rwebaudio->tmpbuf_left[rwebaudio->tmpbuf_offset]  = *(samples++);
      rwebaudio->tmpbuf_right[rwebaudio->tmpbuf_offset] = *(samples++);
      num_frames--;
      if (++rwebaudio->tmpbuf_offset == rwebaudio->tmpbuf_frames)
      {
         size_t queued = RWebAudioQueueBuffer(rwebaudio->tmpbuf_frames, rwebaudio->tmpbuf_left, rwebaudio->tmpbuf_right);
         rwebaudio->tmpbuf_offset = 0;
         /* fast-forward or context is suspended */
         if (queued < rwebaudio->tmpbuf_frames)
            break;
         written += queued;
      }
   }

   if (rwebaudio->nonblock)
      return written;

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
   while (RWebAudioWriteAvailFrames() == 0)
   {
#ifdef EMSCRIPTEN_FULL_ASYNCIFY
      retro_sleep(1);
#endif
      RWebAudioResumeCtx();
   }
#endif

   return written;
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
   while (!rwebaudio->nonblock && RWebAudioWriteAvailFrames() == 0)
   {
      RWebAudioResumeCtx();
#ifdef EMSCRIPTEN_AUDIO_ASYNC_BLOCK
      retro_sleep(1);
#else
      return true;
#endif
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
   size_t avail_frames;
   if (!rwebaudio)
      return 0;

   avail_frames = RWebAudioWriteAvailFrames();
   if (avail_frames > rwebaudio->tmpbuf_offset)
      return (avail_frames - rwebaudio->tmpbuf_offset) * 2 * sizeof(float);
   return 0;
}

static size_t rwebaudio_buffer_size(void *data)
{
   return RWebAudioBufferSizeFrames() * 2 * sizeof(float);
}

static bool rwebaudio_use_float(void *data) { return true; }

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
};
