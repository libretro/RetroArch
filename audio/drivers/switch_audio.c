/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018      - misson20000
 *  Copyright (C) 2018      - m4xw
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

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdint.h>

#include <switch.h>

typedef AudioOutBuffer compat_audio_out_buffer;
#define switch_audio_ipc_init audoutInitialize
#define switch_audio_ipc_finalize audoutExit
/* One released buffer into *out, or none; num says which. */
#define switch_audio_ipc_output_get_released_buffer(a, out, num) audoutGetReleasedAudioOutBuffer(out, &num)
#define switch_audio_ipc_output_append_buffer(a, b) audoutAppendAudioOutBuffer(b)
#define switch_audio_ipc_output_stop(a) audoutStopAudioOut()
#define switch_audio_ipc_output_start(a) audoutStartAudioOut()

#include "../audio_driver.h"
#include "../../verbosity.h"

#define BUFFER_COUNT 5

#define SAMPLE_RATE 48000
#define NUM_CHANNELS 2
#define FRAME_BYTES (NUM_CHANNELS * sizeof(int16_t))

#define SAMPLE_BUFFER_SIZE (((SAMPLE_RATE * NUM_CHANNELS * sizeof(uint16_t)) + 0xfff) & ~0xfff)

typedef struct
{
   compat_audio_out_buffer buffers[BUFFER_COUNT];
   /* Buffers the service has played out and released, taken back
    * without waiting; the next writes fill them. */
   compat_audio_out_buffer *free_buffers[BUFFER_COUNT];
   compat_audio_out_buffer *current_buffer;
   /* Bytes a buffer carries when it is appended: the latency setting
    * spread across the buffers, in whole frames. */
   size_t chunk;
   unsigned free_count;
   bool blocking;
   bool is_paused;
} switch_audio_t;

static uint32_t switch_audio_data_size(void)
{
   static const int framerate    = 1000 / 30;
   static const int sample_count = (SAMPLE_RATE / framerate);
   return (sample_count * NUM_CHANNELS * sizeof(uint16_t));
}

/* The capacity of one service buffer, in bytes. */
static size_t switch_audio_capacity(void)
{
   return (switch_audio_data_size() + 0xfff) & ~0xfff;
}

/* Takes back every buffer the service has released, without waiting.
 * Returns -1 when the service could not be asked. */
static int switch_audio_reap(switch_audio_t *swa)
{
   unsigned n;

   for (n = 0; n < BUFFER_COUNT; n++)
   {
      compat_audio_out_buffer *released = NULL;
      uint32_t num                      = 0;

      if (switch_audio_ipc_output_get_released_buffer(swa, &released, num) != 0)
         return -1;
      if (num < 1 || !released)
         break;
      if (swa->free_count < BUFFER_COUNT)
         swa->free_buffers[swa->free_count++] = released;
   }

   return 0;
}

/* Bytes the driver takes now without waiting: the released buffers,
 * and the rest of the one being filled. */
static size_t switch_audio_room(const switch_audio_t *swa)
{
   size_t room = (size_t)swa->free_count * swa->chunk;
   if (swa->current_buffer && swa->current_buffer->data_size < swa->chunk)
      room += swa->chunk - swa->current_buffer->data_size;
   return room;
}

/* Bound on a wait for the service to release a buffer: one wait, in
 * nanoseconds, and how many before the write is skipped. */
#define SWITCH_AUDIO_WAIT_NS   100000000ULL
#define SWITCH_AUDIO_WAIT_LAPS 8

/* Makes swa->current_buffer a released buffer, waiting for one to be
 * played out when block is set. Returns 1 with a buffer in hand, 0 when
 * none was available and block was not set, -1 on error. The buffer
 * stays current across calls until a write fills and appends it. */
static int switch_audio_acquire_buffer(switch_audio_t *swa, bool block)
{
   /* The service releases a buffer as it finishes playing one; a
    * service that has stopped - the device change, the console on its
    * way to sleep - releases none. Each wait is bounded and the loop is
    * capped; with no buffer in hand after that, the write is skipped
    * this call. */
   int laps = SWITCH_AUDIO_WAIT_LAPS;

   if (swa->current_buffer)
      return 1;

   if (switch_audio_reap(swa) != 0)
   {
      RARCH_ERR("[Switch audio] Failed to get released buffer.\n");
      return -1;
   }

   while (!swa->free_count)
   {
      if (!block || --laps < 0)
         return 0;
      {
         compat_audio_out_buffer *released = NULL;
         uint32_t num                      = 0;
         if (     audoutWaitPlayFinish(&released, &num, SWITCH_AUDIO_WAIT_NS) == 0
               && num >= 1 && released)
            swa->free_buffers[swa->free_count++] = released;
      }
   }

   swa->current_buffer            = swa->free_buffers[--swa->free_count];
   swa->current_buffer->data_size = 0;
   return 1;
}

/* Waits until a buffer is in hand and returns the room the driver has
 * then; a buffer carries at least a frame of audio, so this always
 * makes progress whatever len is. Returns 0 on error. */
static size_t switch_audio_wait_writable(void *data, size_t len)
{
   switch_audio_t *swa = (switch_audio_t*)data;

   (void)len;
   if (!swa)
      return 0;
   if (switch_audio_acquire_buffer(swa, true) <= 0)
      return 0;
   return switch_audio_room(swa);
}

/* Fills buffers a chunk at a time and appends each one full, until the
 * caller's audio is taken or no buffer is free; non-blocking, that is
 * exactly what write_avail() reported. */
static ssize_t switch_audio_write(void *data, const void *s, size_t len)
{
   size_t written      = 0;
   switch_audio_t *swa = (switch_audio_t*)data;

   if (!swa)
      return -1;

   while (written < len)
   {
      size_t _len;
      int r = switch_audio_acquire_buffer(swa, swa->blocking);
      if (r < 0)
         return written ? (ssize_t)written : -1;
      if (r == 0)
         break;

      _len = swa->chunk - swa->current_buffer->data_size;
      if (_len > len - written)
         _len = len - written;

      memcpy(((uint8_t*)swa->current_buffer->buffer)
            + swa->current_buffer->data_size,
            (const uint8_t*)s + written, _len);
      swa->current_buffer->data_size += _len;
      written                        += _len;

      if (swa->current_buffer->data_size >= swa->chunk)
      {
         swa->current_buffer->buffer_size = switch_audio_capacity();
         if (switch_audio_ipc_output_append_buffer(swa, swa->current_buffer) != 0)
            return -1;
         swa->current_buffer = NULL;
      }
   }

   return (ssize_t)written;
}

static bool switch_audio_stop(void *data)
{
   switch_audio_t *swa = (switch_audio_t*) data;
   if (!swa)
      return false;

   /* TODO/FIXME - fix libnx codepath */
   return true;
}

static bool switch_audio_start(void *data, bool is_shutdown)
{
   switch_audio_t *swa = (switch_audio_t*) data;
   if (!swa)
      return false;

   /* TODO/FIXME - fix libnx codepath */
   return true;
}

static bool switch_audio_alive(void *data)
{
   switch_audio_t *swa = (switch_audio_t*) data;
   return (swa && !swa->is_paused);
}

static void switch_audio_free(void *data)
{
   int i;
   switch_audio_t *swa = (switch_audio_t*) data;

   if (!swa)
      return;

   if (!swa->is_paused)
      audoutStopAudioOut();

   audoutExit();
   for (i = 0; i < BUFFER_COUNT; i++)
      free(swa->buffers[i].buffer);
   free(swa);
}

/* audout is PCM_INT16 only; init checks for exactly that format and
 * refuses anything else. Float is not something this service offers. */
static bool switch_audio_use_float(void *data) { return false; }

/* The released buffers, taken back without waiting, and the rest of
 * the one being filled. */
static size_t switch_audio_write_avail(void *data)
{
   switch_audio_t *swa = (switch_audio_t*)data;

   if (!swa)
      return 0;
   switch_audio_reap(swa);
   return switch_audio_room(swa);
}

/* Every buffer carrying its chunk: what the driver holds between
 * write() returning and the service playing it. */
static size_t switch_audio_buffer_size(void *data)
{
   switch_audio_t *swa = (switch_audio_t*)data;
   return swa ? (size_t)BUFFER_COUNT * swa->chunk : 0;
}

static void switch_audio_set_nonblock_state(void *data, bool state)
{
   switch_audio_t *swa = (switch_audio_t*) data;

   if (swa)
      swa->blocking = !state;
}

static void *switch_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   unsigned i;
   switch_audio_t *swa = (switch_audio_t*) calloc(1, sizeof(*swa));

   if (!swa)
      return NULL;

   if (switch_audio_ipc_init() != 0)
      goto fail;

   if (audoutStartAudioOut() != 0)
      goto fail;

   for (i = 0; i < BUFFER_COUNT; i++)
   {
      swa->buffers[i].buffer_size = switch_audio_capacity();
      swa->buffers[i].data_size   = switch_audio_data_size();

      swa->buffers[i].next        = NULL; /* Unused */
      swa->buffers[i].data_offset = 0;
      swa->buffers[i].buffer      = memalign(0x1000, switch_audio_capacity());

      if (!swa->buffers[i].buffer)
         goto fail_audio_output;

      memset(swa->buffers[i].buffer, 0, switch_audio_capacity());

      if (switch_audio_ipc_output_append_buffer(swa, &swa->buffers[i]) != 0)
         goto fail_audio_output;
   }

   *new_rate           = audoutGetSampleRate();

   swa->current_buffer = NULL;
   swa->free_count     = 0;

   {
      /* The latency setting, spread across the buffers, so that all of
       * them queued hold about that much: whole frames, at least a
       * period's worth, never past a buffer's capacity. */
      size_t capacity = switch_audio_capacity() - switch_audio_capacity() % FRAME_BYTES;
      swa->chunk      = (size_t)*new_rate * latency / 1000 * FRAME_BYTES / BUFFER_COUNT;
      swa->chunk     -= swa->chunk % FRAME_BYTES;
      if (swa->chunk < 64 * FRAME_BYTES)
         swa->chunk = 64 * FRAME_BYTES;
      if (swa->chunk > capacity)
         swa->chunk = capacity;
   }

   /* Blocking until the frontend says otherwise. This used to be
    * assigned block_frames - a frame count into a bool, so any block
    * size at all made the driver blocking - and set_nonblock_state
    * overwrote it on the next call regardless. */
   swa->blocking       = true;
   swa->is_paused      = true;

   RARCH_LOG("[Switch audio] Audio initialized.\n");

   return swa;

fail_audio_output:
/* TODO/FIXME - fix libnx codepath */

/* TODO/FIXME - fix libnx codepath */
fail:
   if (swa)
      free(swa);
   return NULL;
}

audio_driver_t audio_switch = {
   switch_audio_init,
   switch_audio_write,
   switch_audio_stop,
   switch_audio_start,
   switch_audio_alive,
   switch_audio_set_nonblock_state,
   switch_audio_free,
   switch_audio_use_float,
   "switch",
   NULL, /* device_list_new */
   NULL, /* device_list_free */
   switch_audio_write_avail,
   switch_audio_buffer_size, /* buffer_size */
   NULL, /* write_raw */
   switch_audio_wait_writable
};
