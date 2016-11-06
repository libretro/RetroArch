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
   AXVoiceOffsets offsets_l;
   AXVoiceOffsets offsets_r;
   bool nonblocking;
   bool playing;

   uint32_t pos;
   uint32_t playpos;
   uint32_t cpu_ticks_last;
} ax_audio_t;

#define AX_AUDIO_COUNT       (1u << 14u)
#define AX_AUDIO_COUNT_MASK  (AX_AUDIO_COUNT - 1u)
#define AX_AUDIO_SIZE        (AX_AUDIO_COUNT * sizeof(int16_t))
#define AX_AUDIO_SIZE_MASK   (AX_AUDIO_SIZE  - 1u)


#define AX_AUDIO_RATE                        48000
#define ax_audio_ticks_to_samples(ticks)     (((ticks) * 64) / 82875)
#define ax_audio_samples_to_ticks(samples)   (((samples) * 82875) / 64)

static void ax_voice_callback(ax_audio_t* ax_audio)
{
   DEBUG_LINE();
}

static void ax_audio_update_playpos(ax_audio_t* ax_audio)
{
   uint32_t samples_played = ax_audio_ticks_to_samples(OSGetSystemTick() - ax_audio->cpu_ticks_last);

   ax_audio->playpos   = (ax_audio->playpos + samples_played) & AX_AUDIO_COUNT_MASK;
   ax_audio->cpu_ticks_last += ax_audio_samples_to_ticks(samples_played);
}

void ax_frame_callback(void)
{
   DEBUG_LINE();
}

static void* ax_audio_init(const char* device, unsigned rate, unsigned latency)
{
   ax_audio_t* ax = (ax_audio_t*)calloc(1, sizeof(ax_audio_t));

   if (!ax)
      return NULL;

   AXInitParams init = {AX_INIT_RENDERER_48KHZ, 0, 0};

   AXInitWithParams(&init);

   ax->voice_l = AXAcquireVoice(10, (AXVoiceCallbackFn)ax_voice_callback, ax);
   ax->voice_r = AXAcquireVoice(10, (AXVoiceCallbackFn)ax_voice_callback, ax);
   if(!ax->voice_l || !ax->voice_r)
   {
      free(ax);
      return NULL;
   }
   ax->buffer_l = malloc(AX_AUDIO_SIZE);
   ax->buffer_r = malloc(AX_AUDIO_SIZE);
   ax->offsets_l.data = ax->buffer_l;
   ax->offsets_l.currentOffset = 0;
   ax->offsets_l.loopOffset = 0;
   ax->offsets_l.endOffset = AX_AUDIO_COUNT;
   ax->offsets_l.loopingEnabled = AX_VOICE_LOOP_ENABLED;
   ax->offsets_l.dataType = AX_VOICE_FORMAT_LPCM16;
   ax->offsets_r.data = ax->buffer_r;
   ax->offsets_r.currentOffset = 0;
   ax->offsets_r.loopOffset = 0;
   ax->offsets_r.endOffset = AX_AUDIO_COUNT;
   ax->offsets_r.loopingEnabled = AX_VOICE_LOOP_ENABLED;
   ax->offsets_r.dataType = AX_VOICE_FORMAT_LPCM16;
   AXSetVoiceOffsets(ax->voice_l, &ax->offsets_l);
   AXSetVoiceOffsets(ax->voice_r, &ax->offsets_r);

   AXSetVoiceSrcType(ax->voice_l, AX_VOICE_SRC_TYPE_NONE);
   AXSetVoiceSrcType(ax->voice_r, AX_VOICE_SRC_TYPE_NONE);
   AXSetVoiceSrcRatio(ax->voice_l, 1.0f);
   AXSetVoiceSrcRatio(ax->voice_r, 1.0f);
   AXVoiceVeData ve = {0x4000, 0};
   AXSetVoiceVe(ax->voice_l, &ve);
   AXSetVoiceVe(ax->voice_r, &ve);
   AXVoiceDeviceMixData mix = {0};
   mix.bus[0].volume = 0x4000;
   mix.bus[1].volume = 0x0000;
   AXSetVoiceDeviceMix(ax->voice_l, AX_DEVICE_TYPE_DRC, 0, &mix);
   AXSetVoiceDeviceMix(ax->voice_l, AX_DEVICE_TYPE_TV, 0, &mix);
   mix.bus[0].volume = 0x0000;
   mix.bus[1].volume = 0x4000;
   AXSetVoiceDeviceMix(ax->voice_r, AX_DEVICE_TYPE_DRC, 0, &mix);
   AXSetVoiceDeviceMix(ax->voice_r, AX_DEVICE_TYPE_TV, 0, &mix);
   AXSetVoiceState(ax->voice_l, AX_VOICE_STATE_PLAYING);
   AXSetVoiceState(ax->voice_r, AX_VOICE_STATE_PLAYING);

   ax->pos = 0;
   ax->playpos = 0;
   ax->playing = true;
   ax->cpu_ticks_last = OSGetSystemTick();

   config_get_ptr()->audio.out_rate = AX_AUDIO_RATE;

   return ax;
}

static void ax_audio_free(void* data)
{
   ax_audio_t* ax = (ax_audio_t*)data;

   free(ax->buffer_l);
   free(ax->buffer_r);
   free(ax);
   AXQuit();
}

static ssize_t ax_audio_write(void* data, const void* buf, size_t size)
{
   int i;
   static struct retro_perf_counter ax_audio_write_perf = {0};
   ax_audio_t* ax = (ax_audio_t*)data;
   const uint16_t* src = buf;
   uint32_t samples_played = 0;
   uint64_t current_tick = 0;

   performance_counter_init(&ax_audio_write_perf, "ax_audio_write");
   performance_counter_start(&ax_audio_write_perf);

   ax_audio_update_playpos(ax);

   if ((((ax->playpos  - ax->pos) & AX_AUDIO_COUNT_MASK) < (AX_AUDIO_COUNT >> 2)) ||
         (((ax->pos - ax->playpos) & AX_AUDIO_COUNT_MASK) < (AX_AUDIO_COUNT >> 4)) ||
         (((ax->playpos  - ax->pos) & AX_AUDIO_COUNT_MASK) < (size >> 2)))
   {
      if (ax->nonblocking)
         ax->pos = (ax->playpos + (AX_AUDIO_COUNT >> 1)) & AX_AUDIO_COUNT_MASK;
      else
      {
         do
         {
            /* todo: compute the correct sleep period */
            retro_sleep(1);
            ax_audio_update_playpos(ax);
         }
         while (((ax->playpos - ax->pos) & AX_AUDIO_COUNT_MASK) < (AX_AUDIO_COUNT >> 1)
                || (((ax->pos - ax->playpos) & AX_AUDIO_COUNT_MASK) < (AX_AUDIO_COUNT >> 4)));
      }
   }

   uint16_t* dst_l = (uint16_t*)ax->buffer_l;
   for (i = 0; i < (size >> 1); i += 2)
   {
//      ax->l[ax->pos] = src[i];
//      ax->r[ax->pos] = src[i + 1];
      ax->buffer_l[ax->pos] = (src[i]);
      ax->buffer_r[ax->pos] = (src[i + 1]);
      ax->pos++;
      ax->pos &= AX_AUDIO_COUNT_MASK;
   }
   DCFlushRange(ax->buffer_l, AX_AUDIO_SIZE);
   DCFlushRange(ax->buffer_r, AX_AUDIO_SIZE);


   performance_counter_stop(&ax_audio_write_perf);

   return size;
}

static bool ax_audio_stop(void* data)
{
   ax_audio_t* ax = (ax_audio_t*)data;

   /* TODO */
   if(ax->playing)
   {
//      AXSetVoiceVeDelta(ax->voice, -128);
      AXVoiceVeData ve = {0};
      AXSetVoiceVe(ax->voice_l, &ve);
      AXSetVoiceVe(ax->voice_r, &ve);
      ax->playing = false;
   }

   return true;
}

static bool ax_audio_alive(void* data)
{
   ax_audio_t* ax = (ax_audio_t*)data;
   return ax->playing;
}

static bool ax_audio_start(void* data)
{
   ax_audio_t* ax = (ax_audio_t*)data;

   /* Prevents restarting audio when the menu
    * is toggled off on shutdown */

   if (runloop_ctl(RUNLOOP_CTL_IS_SHUTDOWN, NULL))
      return true;

   /* TODO */

   if(!ax->playing)
   {
      AXVoiceVeData ve = {0x4000, 0};
      AXSetVoiceVe(ax->voice_l, &ve);
      AXSetVoiceVe(ax->voice_r, &ve);

//      AXSetVoiceVeDelta(ax->voice, 128);
      ax->playing = true;
   }

   return true;
}

static void ax_audio_set_nonblock_state(void* data, bool state)
{
   ax_audio_t* ax = (ax_audio_t*)data;

//   if (ax)
//      ax->nonblocking = state;
   ax->nonblocking = true;
}

static bool ax_audio_use_float(void* data)
{
   (void)data;
   return false;
}

static size_t ax_audio_write_avail(void* data)
{
   ax_audio_t* ax = (ax_audio_t*)data;

   ax_audio_update_playpos(ax);
   return (ax->playpos - ax->pos) & AX_AUDIO_COUNT_MASK;
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
   ax_audio_write_avail,
   ax_audio_buffer_size
};
