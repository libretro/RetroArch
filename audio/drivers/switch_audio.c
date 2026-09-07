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

#include "switch_audio_compat.h"
#include "../audio_driver.h"
#include "../../verbosity.h"

#ifdef HAVE_LIBNX
#define BUFFER_COUNT 5
#else
#define BUFFER_COUNT 3
#endif

#define SAMPLE_RATE 48000
#define NUM_CHANNELS 2

#define SAMPLE_BUFFER_SIZE (((SAMPLE_RATE * NUM_CHANNELS * sizeof(uint16_t)) + 0xfff) & ~0xfff)

typedef struct
{
   bool blocking;
   bool is_paused;
   uint64_t last_append;
   unsigned latency;
   compat_audio_out_buffer buffers[BUFFER_COUNT];
   compat_audio_out_buffer *current_buffer;

#ifndef HAVE_LIBNX
   audio_output_t output;
   handle_t event;
#endif
} switch_audio_t;

#ifdef HAVE_LIBNX
static uint32_t switch_audio_data_size(void)
{
   static const int framerate    = 1000 / 30;
   static const int sample_count = (SAMPLE_RATE / framerate);
   return (sample_count * NUM_CHANNELS * sizeof(uint16_t));
}
#else
static uint32_t switch_audio_data_size(void)
{
   return SAMPLE_BUFFER_SIZE;
}
#endif

static size_t switch_audio_buffer_size(void *data)
{
#ifdef HAVE_LIBNX
   return (switch_audio_data_size() + 0xfff) & ~0xfff;
#else
   return SAMPLE_BUFFER_SIZE;
#endif
}

/* Makes swa->current_buffer the next released buffer, waiting for one
 * to be played out when block is set. Returns 1 with a buffer in hand,
 * 0 when none was available and block was not set, -1 on error. The
 * buffer stays current across calls until a write fills and appends
 * it, so a wait that acquires one hands it to the write that follows. */
/* Bound on a wait for the service to release a buffer: one wait, in
 * nanoseconds, and how many before the write is skipped. */
#define SWITCH_AUDIO_WAIT_NS   100000000ULL
#define SWITCH_AUDIO_WAIT_LAPS 8

static int switch_audio_acquire_buffer(switch_audio_t *swa, bool block)
{
   uint32_t num;

   if (swa->current_buffer)
      return 1;

   if (switch_audio_ipc_output_get_released_buffer(swa, num) != 0)
   {
      RARCH_ERR("[Switch audio] Failed to get released buffer.\n");
      return -1;
   }

   if (num < 1)
      swa->current_buffer = NULL;

   if (!swa->current_buffer)
   {
      /* The service releases a buffer as it finishes playing one; a
       * service that has stopped - the device change, the console on
       * its way to sleep - releases none, and this waited for it with
       * no end. Each wait is bounded and the loop is capped; with no
       * buffer in hand after that, the write is skipped this call. */
      int laps = SWITCH_AUDIO_WAIT_LAPS;

      if (!block)
         return 0;

      while (!swa->current_buffer)
      {
#ifndef HAVE_LIBNX
         uint32_t handle_idx = 0;
#endif
         num                 = 0;

#ifdef HAVE_LIBNX
         if (audoutWaitPlayFinish(&swa->current_buffer, &num,
                  SWITCH_AUDIO_WAIT_NS) != 0)
            swa->current_buffer = NULL;
#else
         svcWaitSynchronization(&handle_idx, &swa->event, 1, SWITCH_AUDIO_WAIT_NS);
         svcResetSignal(swa->event);

         if (switch_audio_ipc_output_get_released_buffer(swa, num) != 0)
            return -1;
#endif
         if (!swa->current_buffer && --laps < 0)
            return 0;
      }
   }

   swa->current_buffer->data_size = 0;
   return 1;
}

/* Waits until a buffer is in hand and returns the room left in it; a
 * buffer's whole capacity is at least a frame of audio, so this always
 * makes progress whatever len is. Returns 0 on error. */
static size_t switch_audio_wait_writable(void *data, size_t len)
{
   switch_audio_t *swa = (switch_audio_t*)data;

   (void)len;
   if (!swa)
      return 0;
   if (switch_audio_acquire_buffer(swa, true) <= 0)
      return 0;
   return switch_audio_buffer_size(NULL) - swa->current_buffer->data_size;
}

static ssize_t switch_audio_write(void *data, const void *s, size_t len)
{
   size_t _len = len;
	switch_audio_t *swa = (switch_audio_t*)data;

   if (!swa)
      return -1;

   {
      int r = switch_audio_acquire_buffer(swa, swa->blocking);
      if (r < 0)
         return -1;
      if (r == 0)
         return 0;
   }

	if (_len > switch_audio_buffer_size(NULL) - swa->current_buffer->data_size)
		_len = switch_audio_buffer_size(NULL) - swa->current_buffer->data_size;

#ifndef HAVE_LIBNX
   memcpy(((uint8_t*) swa->current_buffer->sample_data) + swa->current_buffer->data_size, s, _len);
#else
   memcpy(((uint8_t*) swa->current_buffer->buffer) + swa->current_buffer->data_size, s, _len);
#endif
	swa->current_buffer->data_size   += _len;
	swa->current_buffer->buffer_size  = switch_audio_buffer_size(NULL);

	if (swa->current_buffer->data_size > (48000 * swa->latency) / 1000)
   {
         if (switch_audio_ipc_output_append_buffer(swa, swa->current_buffer) != 0)
            return -1;
		swa->current_buffer = NULL;
	}

	swa->last_append = svcGetSystemTick();

	return _len;
}

static bool switch_audio_stop(void *data)
{
   switch_audio_t *swa = (switch_audio_t*) data;
   if (!swa)
      return false;

   /* TODO/FIXME - fix libnx codepath */
#ifndef HAVE_LIBNX
   if (!swa->is_paused)
	   if (switch_audio_ipc_output_stop(swa) != 0)
		   return false;

   swa->is_paused = true;
#endif
   return true;
}

static bool switch_audio_start(void *data, bool is_shutdown)
{
   switch_audio_t *swa = (switch_audio_t*) data;
   if (!swa)
      return false;

   /* TODO/FIXME - fix libnx codepath */
#ifndef HAVE_LIBNX
   if (swa->is_paused)
	   if (switch_audio_ipc_output_start(swa) != 0)
		   return false;

   swa->is_paused = false;
#endif
   return true;
}

static bool switch_audio_alive(void *data)
{
   switch_audio_t *swa = (switch_audio_t*) data;
   return (swa && !swa->is_paused);
}

static void switch_audio_free(void *data)
{
#ifdef HAVE_LIBNX
   int i;
#endif
   switch_audio_t *swa = (switch_audio_t*) data;

   if (!swa)
      return;

#ifdef HAVE_LIBNX
   if (!swa->is_paused)
      audoutStopAudioOut();

   audoutExit();
   for (i = 0; i < BUFFER_COUNT; i++)
      free(swa->buffers[i].buffer);
#else
   audio_ipc_output_close(&swa->output);
   audio_ipc_finalize();
#endif
   free(swa);
}

/* audout is PCM_INT16 only; init checks for exactly that format and
 * refuses anything else. Float is not something this service offers. */
static bool switch_audio_use_float(void *data) { return false; }

static size_t switch_audio_write_avail(void *data)
{
   switch_audio_t *swa = (switch_audio_t*) data;

   if (!swa || !swa->current_buffer)
      return 0;

   return swa->current_buffer->buffer_size;
}

static void switch_audio_set_nonblock_state(void *data, bool state)
{
   switch_audio_t *swa = (switch_audio_t*) data;

   if (swa)
      swa->blocking = !state;
}

static void *switch_audio_init(const char *device,
      unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   unsigned i;
#ifndef HAVE_LIBNX
   char names[8][0x20];
   uint32_t num_names  = 0;
#endif
   switch_audio_t *swa = (switch_audio_t*) calloc(1, sizeof(*swa));

   if (!swa)
      return NULL;

   if (switch_audio_ipc_init() != 0)
      goto fail;

#ifdef HAVE_LIBNX
   if (audoutStartAudioOut() != 0)
      goto fail;
#else
   if (audio_ipc_list_outputs(&names[0], 8, &num_names) != 0)
      goto fail_audio_ipc;

   if (num_names != 1)
   {
      RARCH_ERR("[Switch audio] Got back more than one AudioOut.\n");
      goto fail_audio_ipc;
   }

   if (audio_ipc_open_output(names[0], &swa->output) != 0)
      goto fail_audio_ipc;

   if (swa->output.sample_rate != SAMPLE_RATE)
   {
      RARCH_ERR("[Switch audio] Expected sample rate of %d, got sample rate of %d.\n",
            SAMPLE_RATE, swa->output.sample_rate);
      goto fail_audio_output;
   }

   if (swa->output.num_channels != NUM_CHANNELS)
   {
      RARCH_ERR("[Switch audio] Expected %d channels, got %d.\n", NUM_CHANNELS,
            swa->output.num_channels);
      goto fail_audio_output;
   }

   if (swa->output.sample_format != PCM_INT16)
   {
      RARCH_ERR("[Switch audio] Expected PCM_INT16, got %d.\n", swa->output.sample_format);
      goto fail_audio_output;
   }

   if (audio_ipc_output_register_buffer_event(&swa->output, &swa->event) != 0)
      goto fail_audio_output;
#endif

   for (i = 0; i < BUFFER_COUNT; i++)
   {
      swa->buffers[i].buffer_size = switch_audio_buffer_size(NULL);
      swa->buffers[i].data_size   = switch_audio_data_size();

#ifdef HAVE_LIBNX
      swa->buffers[i].next        = NULL; /* Unused */
      swa->buffers[i].data_offset = 0;
      swa->buffers[i].buffer      = memalign(0x1000, switch_audio_buffer_size(NULL));

      if (!swa->buffers[i].buffer)
         goto fail_audio_output;

      memset(swa->buffers[i].buffer, 0, switch_audio_buffer_size(NULL));
#else
      swa->buffers[i].ptr         = &swa->buffers[i].sample_data;
      swa->buffers[i].unknown     = 0;
      swa->buffers[i].sample_data = alloc_pages(SAMPLE_BUFFER_SIZE,
            switch_audio_buffer_size(NULL), NULL);

      if (!swa->buffers[i].sample_data)
	      goto fail_audio_output;
#endif

      if (switch_audio_ipc_output_append_buffer(swa, &swa->buffers[i]) != 0)
         goto fail_audio_output;
   }

#ifdef HAVE_LIBNX
   *new_rate           = audoutGetSampleRate();
#else
   *new_rate           = swa->output.sample_rate;
#endif

   swa->current_buffer = NULL;
   swa->latency        = latency;
   swa->last_append    = svcGetSystemTick();

   swa->blocking       = block_frames;
   swa->is_paused      = true;

   RARCH_LOG("[Switch audio] Audio initialized.\n");

   return swa;

fail_audio_output:
/* TODO/FIXME - fix libnx codepath */
#ifndef HAVE_LIBNX
   audio_ipc_output_close(&swa->output);
fail_audio_ipc:
#endif

/* TODO/FIXME - fix libnx codepath */
#ifndef HAVE_LIBNX
   audio_ipc_finalize();
#endif
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
