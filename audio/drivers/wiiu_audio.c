/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2016 - Ali Bouhlel
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

#include <sndcore2/core.h>
#include <sndcore2/device.h>
#include <sndcore2/drcvs.h>
#include <sndcore2/result.h>
#include <sndcore2/voice.h>
#include <coreinit/time.h>
#include <coreinit/cache.h>
#include <coreinit/thread.h>


#include "wiiu/wiiu_dbg.h"
#include "wiiu/system/memory.h"

#include "audio/audio_driver.h"
#include "configuration.h"
#include "performance_counters.h"
#include "runloop.h"

typedef struct
{
   AXVoice* voice_l;
   AXVoice* voice_r;
   uint16_t* buffer_l;
   uint16_t* buffer_r;
   bool nonblocking;

   uint32_t pos;
} ax_audio_t;

#define AX_AUDIO_COUNT_SHIFT        13u
#define AX_AUDIO_COUNT              (1u << AX_AUDIO_COUNT_SHIFT)
#define AX_AUDIO_COUNT_MASK         (AX_AUDIO_COUNT - 1u)
#define AX_AUDIO_SIZE               (AX_AUDIO_COUNT << 1u)
#define AX_AUDIO_SIZE_MASK          (AX_AUDIO_SIZE - 1u)

//#define AX_AUDIO_FRAME_COUNT        144
#define AX_AUDIO_FRAME_COUNT        160
#define AX_AUDIO_RATE               48000
//#define ax_audio_ticks_to_samples(ticks)     (((ticks) * 64) / 82875)
//#define ax_audio_samples_to_ticks(samples)   (((samples) * 82875) / 64)

static inline int ax_diff(int v1, int v2)
{
   return ((v1 - v2) << (32u - AX_AUDIO_COUNT_SHIFT)) >> (32u - AX_AUDIO_COUNT_SHIFT);
}

AXResult ax_aux_callback(void* data, ax_audio_t* ax)
{
   AXVoiceOffsets offsets;
   AXGetVoiceOffsets(ax->voice_l, &offsets);

   if (ax_diff(offsets.currentOffset, ax->pos) < 0)
   {
      AXSetVoiceState(ax->voice_l, AX_VOICE_STATE_STOPPED);
      AXSetVoiceState(ax->voice_r, AX_VOICE_STATE_STOPPED);
   }

   return AX_RESULT_SUCCESS;
}

static void* ax_audio_init(const char* device, unsigned rate, unsigned latency)
{
   ax_audio_t* ax = (ax_audio_t*)calloc(1, sizeof(ax_audio_t));

   if (!ax)
      return NULL;

   AXInitParams init = {AX_INIT_RENDERER_48KHZ, 0, 0};

   AXInitWithParams(&init);

   ax->voice_l = AXAcquireVoice(10, NULL, ax);
   ax->voice_r = AXAcquireVoice(10, NULL, ax);

   if (!ax->voice_l || !ax->voice_r)
   {
      free(ax);
      return NULL;
   }

   ax->buffer_l = MEM1_alloc(AX_AUDIO_SIZE, 0x100);
   ax->buffer_r = MEM1_alloc(AX_AUDIO_SIZE, 0x100);

   AXVoiceOffsets offsets;

   offsets.data = ax->buffer_l;
   offsets.currentOffset = 0;
   offsets.loopOffset = 0;
   offsets.endOffset = AX_AUDIO_COUNT - 1;
   offsets.loopingEnabled = AX_VOICE_LOOP_ENABLED;
   offsets.dataType = AX_VOICE_FORMAT_LPCM16;
   AXSetVoiceOffsets(ax->voice_l, &offsets);

   offsets.data = ax->buffer_r;
   AXSetVoiceOffsets(ax->voice_r, &offsets);

   AXSetVoiceSrcType(ax->voice_l, AX_VOICE_SRC_TYPE_NONE);
   AXSetVoiceSrcType(ax->voice_r, AX_VOICE_SRC_TYPE_NONE);
   AXSetVoiceSrcRatio(ax->voice_l, 1.0f);
   AXSetVoiceSrcRatio(ax->voice_r, 1.0f);
   AXVoiceVeData ve = {0x8000, 0};
   AXSetVoiceVe(ax->voice_l, &ve);
   AXSetVoiceVe(ax->voice_r, &ve);
   u32 mix[24] = {0};
   mix[0] = 0x80000000;
   AXSetVoiceDeviceMix(ax->voice_l, AX_DEVICE_TYPE_DRC, 0, (AXVoiceDeviceMixData*)mix);
   AXSetVoiceDeviceMix(ax->voice_l, AX_DEVICE_TYPE_TV, 0, (AXVoiceDeviceMixData*)mix);
   mix[0] = 0;
   mix[4] = 0x80000000;
   AXSetVoiceDeviceMix(ax->voice_r, AX_DEVICE_TYPE_DRC, 0, (AXVoiceDeviceMixData*)mix);
   AXSetVoiceDeviceMix(ax->voice_r, AX_DEVICE_TYPE_TV, 0, (AXVoiceDeviceMixData*)mix);

   AXSetVoiceState(ax->voice_l, AX_VOICE_STATE_STOPPED);
   AXSetVoiceState(ax->voice_r, AX_VOICE_STATE_STOPPED);

   ax->pos = 0;

   config_get_ptr()->audio.out_rate = AX_AUDIO_RATE;

   AXRegisterAuxCallback(AX_DEVICE_TYPE_DRC, 0, 0, (AXAuxCallback)ax_aux_callback, ax);

   return ax;
}

static void ax_audio_free(void* data)
{
   ax_audio_t* ax = (ax_audio_t*)data;

   AXRegisterAuxCallback(AX_DEVICE_TYPE_DRC, 0, 0, NULL, NULL);

   MEM1_free(ax->buffer_l);
   MEM1_free(ax->buffer_r);
   free(ax);
   AXQuit();
}

static void ax_audio_buffer_write(ax_audio_t* ax, const uint16_t* src, int count)
{
   uint16_t* dst_l = ax->buffer_l + ax->pos;
   uint16_t* dst_r = ax->buffer_r + ax->pos;
   uint16_t* dst_l_max = ax->buffer_l + AX_AUDIO_COUNT;

   while(count-- && (dst_l < dst_l_max))
   {
      *dst_l++ = *src++;
      *dst_r++ = *src++;
   }
   DCFlushRange(ax->buffer_l + ax->pos, (dst_l - ax->pos - ax->buffer_l) << 1);
   DCFlushRange(ax->buffer_r + ax->pos, (dst_r - ax->pos - ax->buffer_r) << 1);

   if(++count)
   {
      dst_l = ax->buffer_l;
      dst_r = ax->buffer_r;

      while(count-- && (dst_l < dst_l_max))
      {
         *dst_l++ = *src++;
         *dst_r++ = *src++;
      }
      DCFlushRange(ax->buffer_l, (dst_l - ax->buffer_l) << 1);
      DCFlushRange(ax->buffer_r, (dst_r - ax->buffer_r) << 1);
   }
   ax->pos = dst_l - ax->buffer_l;
   ax->pos &= AX_AUDIO_COUNT_MASK;

}

static ssize_t ax_audio_write(void* data, const void* buf, size_t size)
{
   static struct retro_perf_counter ax_audio_write_perf = {0};
   ax_audio_t* ax = (ax_audio_t*)data;
   const uint16_t* src = buf;
   int i;

   performance_counter_init(&ax_audio_write_perf, "ax_audio_write");
   performance_counter_start(&ax_audio_write_perf);

   int count = size >> 2;
   AXVoiceOffsets offsets;
   AXGetVoiceOffsets(ax->voice_l, &offsets);

   if((((offsets.currentOffset  - ax->pos) & AX_AUDIO_COUNT_MASK) < (AX_AUDIO_COUNT >> 2)) ||
      (((ax->pos - offsets.currentOffset ) & AX_AUDIO_COUNT_MASK) < (AX_AUDIO_COUNT >> 4)) ||
      (((offsets.currentOffset  - ax->pos) & AX_AUDIO_COUNT_MASK) < (size >> 2)))
   {
      if (ax->nonblocking)
         ax->pos = (offsets.currentOffset + (AX_AUDIO_COUNT >> 1)) & AX_AUDIO_COUNT_MASK;
      else
      {
         do{
            retro_sleep(1);
            AXGetVoiceOffsets(ax->voice_l, &offsets);
         }while(AXIsVoiceRunning(ax->voice_l) &&
               (((offsets.currentOffset - ax->pos) & AX_AUDIO_COUNT_MASK) < (AX_AUDIO_COUNT >> 1) ||
                (((ax->pos - offsets.currentOffset) & AX_AUDIO_COUNT_MASK) < (AX_AUDIO_COUNT >> 4))));
      }
   }


//   ax_audio_buffer_write(ax, buf, count);

   for (i = 0; i < (size >> 1); i += 2)
   {
      ax->buffer_l[ax->pos] = src[i];
      ax->buffer_r[ax->pos] = src[i + 1];
      ax->pos++;
      ax->pos &= AX_AUDIO_COUNT_MASK;
   }
   DCFlushRange(ax->buffer_l, AX_AUDIO_SIZE);
   DCFlushRange(ax->buffer_r, AX_AUDIO_SIZE);

//   if(!AXIsVoiceRunning(ax->voice_l) && (((ax->pos - offsets.currentOffset) & AX_AUDIO_COUNT_MASK) > AX_AUDIO_FRAME_COUNT))
//   {
      AXSetVoiceState(ax->voice_l, AX_VOICE_STATE_PLAYING);
      AXSetVoiceState(ax->voice_r, AX_VOICE_STATE_PLAYING);
//   }

   performance_counter_stop(&ax_audio_write_perf);

   return size;
}

static bool ax_audio_stop(void* data)
{
   ax_audio_t* ax = (ax_audio_t*)data;

   AXSetVoiceState(ax->voice_l, AX_VOICE_STATE_STOPPED);
   AXSetVoiceState(ax->voice_r, AX_VOICE_STATE_STOPPED);
   return true;
}

static bool ax_audio_alive(void* data)
{
   ax_audio_t* ax = (ax_audio_t*)data;
   return AXIsVoiceRunning(ax->voice_l);
}

static bool ax_audio_start(void* data)
{
   ax_audio_t* ax = (ax_audio_t*)data;

   /* Prevents restarting audio when the menu
    * is toggled off on shutdown */

   if (runloop_ctl(RUNLOOP_CTL_IS_SHUTDOWN, NULL))
      return true;

   AXSetVoiceState(ax->voice_l, AX_VOICE_STATE_PLAYING);
   AXSetVoiceState(ax->voice_r, AX_VOICE_STATE_PLAYING);

   return true;
}

static void ax_audio_set_nonblock_state(void* data, bool state)
{
   ax_audio_t* ax = (ax_audio_t*)data;

   if (ax)
      ax->nonblocking = state;
}

static bool ax_audio_use_float(void* data)
{
   (void)data;
   return false;
}

static size_t ax_audio_write_avail(void* data)
{
   ax_audio_t* ax = (ax_audio_t*)data;

   AXVoiceOffsets offsets;
   AXGetVoiceOffsets(ax->voice_l, &offsets);

   return (offsets.currentOffset - ax->pos) & AX_AUDIO_COUNT_MASK;
}

static size_t ax_audio_buffer_size(void* data)
{
   (void)data;
   return AX_AUDIO_COUNT;
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
   NULL,
   NULL,
//   ax_audio_write_avail,
//   ax_audio_buffer_size
};
