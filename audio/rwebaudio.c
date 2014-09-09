/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Michael Lelli
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

#include "../driver.h"
#include "../general.h"

#include "../emscripten/RWebAudio.h"

static void rwebaudio_free(void *data)
{
   RWebAudioFree();
}

static void *rwebaudio_init(const char *device, unsigned rate, unsigned latency)
{
   (void)device;
   (void)rate;
   void *data = RWebAudioInit(latency);
   if (data)
      g_settings.audio.out_rate = RWebAudioSampleRate();
   return data;
}

static ssize_t rwebaudio_write(void *data, const void *buf, size_t size)
{
   (void)data;
   return RWebAudioWrite(buf, size);
}

static bool rwebaudio_stop(void *data)
{
   (void)data;
   return RWebAudioStop();
}

static void rwebaudio_set_nonblock_state(void *data, bool state)
{
   (void)data;
   RWebAudioSetNonblockState(state);
}

static bool rwebaudio_start(void *data)
{
   (void)data;
   return RWebAudioStart();
}

static bool rwebaudio_use_float(void *data)
{
   (void)data;
   return true;
}

static size_t rwebaudio_write_avail(void *data)
{
   (void)data;
   return RWebAudioWriteAvail();
}

static size_t rwebaudio_buffer_size(void *data)
{
   (void)data;
   return RWebAudioBufferSize();
}

const audio_driver_t audio_rwebaudio = {
   rwebaudio_init,
   rwebaudio_write,
   rwebaudio_stop,
   rwebaudio_start,
   rwebaudio_set_nonblock_state,
   rwebaudio_free,
   rwebaudio_use_float,
   "rwebaudio",
   rwebaudio_write_avail,
   rwebaudio_buffer_size,
};

