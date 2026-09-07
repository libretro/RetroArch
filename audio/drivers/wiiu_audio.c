/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
 *  Copyright (C) 2016-2017 - FIX94
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

#include <string.h>
#include <malloc.h>
#include <stdint.h>

#include <wiiu/os.h>
#include <wiiu/ax.h>

#include "../../wiiu/wiiu_dbg.h"
#include "../../wiiu/system/memory.h"

#include "../audio_driver.h"

typedef struct
{
   AXMVoice* mvoice;
   uint16_t* buffer_l;
   uint16_t* buffer_r;
   OSSpinLock spinlock;
   /* Signalled by the AX frame callback every 3 ms, so a waiter sleeps
    * a frame at a time instead of yielding in a loop. */
   OSEvent frame_event;
   uint32_t pos;
   uint32_t written;
   /* Frames the voice has played since it started, for the sink rate
    * estimate. Counted in the frame callback beside the decrement of
    * written and under the same spinlock, so it needs no atomics of its
    * own and cannot disagree with what was actually consumed. */
   uint64_t consumed;
   bool nonblock;
} ax_audio_t;

/* 3072 samples main buffer, 64ms total */
#define AX_AUDIO_COUNT              3072
#define AX_AUDIO_SIZE               (AX_AUDIO_COUNT << 1u)

#define AX_AUDIO_SAMPLE_COUNT       144                         /* 3ms */
#define AX_AUDIO_SAMPLE_MIN         (AX_AUDIO_SAMPLE_COUNT * 3) /* 9ms */
#define AX_AUDIO_SAMPLE_LOAD        (AX_AUDIO_SAMPLE_COUNT * 10)/* 30ms */
#define AX_AUDIO_MAX_FREE           (AX_AUDIO_COUNT - (AX_AUDIO_SAMPLE_COUNT * 2))
/* Bound on a wait for the frame callback to free room: one wait, in
 * microseconds, and how many before the caller gets the pass back. */
#define AX_AUDIO_WAIT_US     100000
#define AX_AUDIO_WAIT_LAPS   8
#define AX_AUDIO_RATE               48000

#if 0
#define ax_audio_ticks_to_samples(ticks)     (((ticks) * 64) / 82875)
#define ax_audio_samples_to_ticks(samples)   (((samples) * 82875) / 64)
#endif

static volatile ax_audio_t *wiiu_cb_ax = NULL;
void wiiu_ax_callback(void)
{
   ax_audio_t *ax = NULL;
   /*possibly called before unregister */
   if (!wiiu_cb_ax)
      return;

   ax = (ax_audio_t*)wiiu_cb_ax;

   if (AXIsMultiVoiceRunning(ax->mvoice))
   {
      if (OSUninterruptibleSpinLock_Acquire(&ax->spinlock))
      {
         /* Buffer underrun, stop playback to let it fill up */
         if (ax->written < AX_AUDIO_SAMPLE_MIN)
            AXSetMultiVoiceState(ax->mvoice, AX_VOICE_STATE_STOPPED);
         ax->written  -= AX_AUDIO_SAMPLE_COUNT;
         /* Only here, where the voice is running and a frame of our
          * audio has actually gone: a stopped voice is not taking
          * anything, and a count that ran on regardless would report
          * the device consuming audio it never played. The estimator
          * discards windows that stall, which is the right reading of
          * a voice that stopped. */
         ax->consumed += AX_AUDIO_SAMPLE_COUNT;
         OSUninterruptibleSpinLock_Release(&ax->spinlock);
      }
   }
   OSSignalEvent(&ax->frame_event);
}

/* Frames the voice has played since it started.
 *
 * The AX frame callback fires every 3 ms and takes AX_AUDIO_SAMPLE_COUNT
 * frames when the voice is running, so counting them counts device time
 * - JACK's shape rather than ALSA's, with no queue to subtract. Read
 * under the same spinlock the callback writes it under; a 64-bit read
 * is not atomic on this CPU, and a torn one would look like the device
 * jumping backwards. */
static size_t ax_audio_frames_consumed(void *data)
{
   ax_audio_t *ax  = (ax_audio_t*)data;
   uint64_t    out = 0;
   if (!ax)
      return 0;
   if (OSUninterruptibleSpinLock_Acquire(&ax->spinlock))
   {
      out = ax->consumed;
      OSUninterruptibleSpinLock_Release(&ax->spinlock);
   }
   return (size_t)out;
}

extern void AXRegisterFrameCallback(void *cb);

static void* ax_audio_init(const char* device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   AXVoiceOffsets offsets[2];
   u16 setup_buf[0x30] = {0};
   setup_buf[0x25]     = 2; /* we request 2 channels */
   AXInitParams init   = {AX_INIT_RENDERER_48KHZ, 0, 0};
   AXVoiceVeData ve    = {0x8000, 0};
   ax_audio_t* ax      = (ax_audio_t*)calloc(1, sizeof(ax_audio_t));

   if (!ax)
      return NULL;

   AXInitWithParams(&init);

   AXAcquireMultiVoice(31, NULL, 0, setup_buf, &ax->mvoice);

   if (!ax->mvoice || ax->mvoice->channels != 2)
   {
      free(ax);
      return NULL;
   }

   ax->buffer_l              = MEM1_alloc(AX_AUDIO_SIZE, 0x100);
   ax->buffer_r              = MEM1_alloc(AX_AUDIO_SIZE, 0x100);
   memset(ax->buffer_l,0,AX_AUDIO_SIZE);
   memset(ax->buffer_r,0,AX_AUDIO_SIZE);
   DCFlushRange(ax->buffer_l,AX_AUDIO_SIZE);
   DCFlushRange(ax->buffer_r,AX_AUDIO_SIZE);

   /* shared by both voices */
   offsets[0].currentOffset  = 0;
   offsets[0].loopOffset     = 0;
   offsets[0].endOffset      = AX_AUDIO_COUNT - 1;
   offsets[0].loopingEnabled = AX_VOICE_LOOP_ENABLED;
   offsets[0].dataType       = AX_VOICE_FORMAT_LPCM16;
   memcpy(&offsets[1], &offsets[0], sizeof(AXVoiceOffsets));

   /* different buffers per voice */
   offsets[0].data           = ax->buffer_l;
   offsets[1].data           = ax->buffer_r;
   AXSetMultiVoiceOffsets(ax->mvoice, offsets);

   AXSetMultiVoiceSrcType(ax->mvoice, AX_VOICE_SRC_TYPE_NONE);
   AXSetMultiVoiceSrcRatio(ax->mvoice, 1.0f);

   AXSetMultiVoiceVe(ax->mvoice, &ve);

   AXSetMultiVoiceDeviceMix(ax->mvoice, AX_DEVICE_TYPE_DRC, 0, 0, 0x8000, 0);
   AXSetMultiVoiceDeviceMix(ax->mvoice, AX_DEVICE_TYPE_TV, 0, 0, 0x8000, 0);

   AXSetMultiVoiceState(ax->mvoice, AX_VOICE_STATE_STOPPED);

   ax->pos                   = 0;
   ax->written               = 0;
   *new_rate                 = AX_AUDIO_RATE;

   OSInitSpinLock(&ax->spinlock);
   OSInitEvent(&ax->frame_event, FALSE, OS_EVENT_MODE_AUTO);

   wiiu_cb_ax                = ax;
   AXRegisterFrameCallback(wiiu_ax_callback);

   return ax;
}

static void ax_audio_free(void* data)
{
   ax_audio_t* ax = (ax_audio_t*)data;
   wiiu_cb_ax     = NULL;

   AXRegisterFrameCallback(NULL);
   AXFreeMultiVoice(ax->mvoice);
   AXQuit();

   MEM1_free(ax->buffer_l);
   MEM1_free(ax->buffer_r);
   free(ax);
}

static bool ax_audio_stop(void* data)
{
   ax_audio_t* ax = (ax_audio_t*)data;

   AXSetMultiVoiceState(ax->mvoice, AX_VOICE_STATE_STOPPED);
   return true;
}

static int ax_audio_limit(int in)
{
	if (in < 0)
		in += AX_AUDIO_COUNT;
	else if (in >= AX_AUDIO_COUNT)
		in -= AX_AUDIO_COUNT;
	return in;
}

static bool ax_audio_start(void* data, bool is_shutdown)
{
   /* Prevents restarting audio when the menu
    * is toggled off on shutdown */
   if (!is_shutdown)
   {
      ax_audio_t* ax = (ax_audio_t*)data;
      /* Set back to playing on enough buffered data */
      if (ax->written > AX_AUDIO_SAMPLE_LOAD)
      {
         AXSetMultiVoiceCurrentOffset(ax->mvoice,
               ax_audio_limit(ax->pos - ax->written));
         AXSetMultiVoiceState(ax->mvoice, AX_VOICE_STATE_PLAYING);
      }
   }

   return true;
}

static ssize_t ax_audio_write(void* data, const void* buf, size_t len)
{
   size_t count_avail  = 0;
   ax_audio_t *ax      = (ax_audio_t*)data;
   const uint16_t *src = buf;
   size_t count        = len >> 2;

   if (!len || (len & 0x3))
      return 0;

   if (count > AX_AUDIO_MAX_FREE)
      count            = AX_AUDIO_MAX_FREE;

   count_avail         = (
         (ax->written > AX_AUDIO_MAX_FREE)
         ? 0
         : (AX_AUDIO_MAX_FREE - ax->written));

   if (ax->nonblock)
   {
      /* Not enough available for 3ms of data */
      if (count_avail < AX_AUDIO_SAMPLE_COUNT)
         count = 0;
   }
   else if (count_avail < count)
   {
      /* Sync, wait for free memory. The AX frame callback frees a
       * frame's worth and signals frame_event each frame; wait on that
       * rather than yield-spin, and bound it - a voice that reports
       * running while its callback has stopped freed nothing, and the
       * spin never ended. The write then takes what room there is. */
      int laps = AX_AUDIO_WAIT_LAPS;
      while (AXIsMultiVoiceRunning(ax->mvoice) && (count_avail < count))
      {
         OSWaitEventWithTimeout(&ax->frame_event,
               (OSTime)OSMicroseconds(AX_AUDIO_WAIT_US));
         count_avail = (ax->written > AX_AUDIO_MAX_FREE ? 0 : (AX_AUDIO_MAX_FREE - ax->written));
         if (--laps < 0)
            break;
      }
   }

   /* Over available space, do as much as possible */
   if (count > count_avail)
      count = count_avail;

   /* make sure we have input size */
   if (count > 0)
   {
      size_t i;
      /* write in new data */
      size_t start_pos    = ax->pos;
      int flush_p2_needed = 0;
      int flush_p2        = 0;

      for (i = 0; i < (count << 1); i += 2)
      {
         ax->buffer_l[ax->pos] = src[i];
         ax->buffer_r[ax->pos] = src[i + 1];
         ax->pos               = ax_audio_limit(ax->pos + 1);

         /* wrapped around, make sure to store cache */
         if (ax->pos == 0)
         {
            flush_p2_needed = 1;
            flush_p2        = ((count << 1) - i);
            DCStoreRangeNoSync(ax->buffer_l + start_pos,
                  (AX_AUDIO_COUNT - start_pos) << 1);
            DCStoreRangeNoSync(ax->buffer_r + start_pos, (AX_AUDIO_COUNT - start_pos) << 1);
         }
      }

      /* standard cache store case */
      if (!flush_p2_needed)
      {
         DCStoreRangeNoSync(ax->buffer_l + start_pos, count << 1);
         DCStoreRange(ax->buffer_r + start_pos, count << 1);
      }
      /* store the rest after wrap */
      else if (flush_p2 > 0)
      {
         DCStoreRangeNoSync(ax->buffer_l, flush_p2);
         DCStoreRange(ax->buffer_r, flush_p2);
      }

      /* add in new audio data */
      if (OSUninterruptibleSpinLock_Acquire(&ax->spinlock))
      {
         ax->written += count;
         OSUninterruptibleSpinLock_Release(&ax->spinlock);
      }
   }

   /* Possibly buffer underrun
    *
    * checks if it can be started
    */
   if (!AXIsMultiVoiceRunning(ax->mvoice))
      ax_audio_start(ax, false);

   /* return what was actually copied */
   return (count << 2);
}

static bool ax_audio_alive(void* data)
{
   ax_audio_t* ax = (ax_audio_t*)data;
   return AXIsMultiVoiceRunning(ax->mvoice);
}

static void ax_audio_set_nonblock_state(void* data, bool state)
{
   ax_audio_t* ax = (ax_audio_t*)data;

   if (ax)
      ax->nonblock = state;
}

static bool ax_audio_use_float(void* data) { return false; }

static size_t ax_audio_write_avail(void* data)
{
   ax_audio_t* ax = (ax_audio_t*)data;
   size_t ret = AX_AUDIO_COUNT - ax->written;
   return (ret < AX_AUDIO_SAMPLE_COUNT ? 0 : ret * 2 * sizeof(int16_t));
}

/* Sleep on the frame event until at least len samples are free below
 * the load limit, in the units write_avail() reports, len capped at
 * half the ring so the wait always ends. Returns the free space then,
 * or 0 once the voice is no longer running. */
static size_t ax_audio_wait_writable(void* data, size_t len)
{
   ax_audio_t* ax = (ax_audio_t*)data;
   size_t avail;
   int laps       = AX_AUDIO_WAIT_LAPS;
   /* len arrives in bytes; the count is kept in frames of int16
    * stereo, four bytes each. */
   size_t want    = len / (2 * sizeof(int16_t));

   if (want > AX_AUDIO_MAX_FREE / 2)
      want = AX_AUDIO_MAX_FREE / 2;

   for (;;)
   {
      if (!AXIsMultiVoiceRunning(ax->mvoice))
         return 0;
      avail = (ax->written > AX_AUDIO_MAX_FREE)
            ? 0 : (AX_AUDIO_MAX_FREE - ax->written);
      if (avail >= want)
         return avail * 2 * sizeof(int16_t);
      /* Timed and capped: a callback that has stopped hands the pass
       * back as no space coming from this call. */
      OSWaitEventWithTimeout(&ax->frame_event,
            (OSTime)OSMicroseconds(AX_AUDIO_WAIT_US));
      if (--laps < 0)
         return 0;
   }
}

/* Both in bytes of int16 stereo, as the interface asks: written counts
 * frames, a frame per four bytes. They were reported in frames. */
static size_t ax_audio_buffer_size(void* data)
{
   return AX_AUDIO_COUNT * 2 * sizeof(int16_t);
}

audio_driver_t audio_ax =
{
   ax_audio_init,
   ax_audio_write,
   ax_audio_stop,
   ax_audio_start,
   ax_audio_alive,
   ax_audio_set_nonblock_state,
   ax_audio_free,
   ax_audio_use_float,
   "AX",
   NULL, /* device_list_new */
   NULL, /* device_list_free */
   ax_audio_write_avail,
   ax_audio_buffer_size,
   NULL, /* write_raw */
   ax_audio_wait_writable,
   ax_audio_frames_consumed
};
