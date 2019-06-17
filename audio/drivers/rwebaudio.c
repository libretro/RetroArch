/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2015 - Michael Lelli
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
#include <unistd.h>
#include <boolean.h>

#include "../../retroarch.h"

/* forward declarations */
unsigned RWebAudioSampleRate(void);
void *RWebAudioInit(unsigned latency);
ssize_t RWebAudioWrite(const void *buf, size_t size);
bool RWebAudioStop(void);
bool RWebAudioStart(void);
void RWebAudioSetNonblockState(bool state);
void RWebAudioFree(void);
size_t RWebAudioWriteAvail(void);
size_t RWebAudioBufferSize(void);
static bool rwebaudio_is_paused;

static void rwebaudio_free(void *data)
{
   RWebAudioFree();
}

static void *rwebaudio_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   void *data           = RWebAudioInit(latency);

   (void)device;
   (void)rate;

   if (data)
      *new_rate         = RWebAudioSampleRate();
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
   rwebaudio_is_paused = true;
   return RWebAudioStop();
}

static void rwebaudio_set_nonblock_state(void *data, bool state)
{
   (void)data;
   RWebAudioSetNonblockState(state);
}

static bool rwebaudio_alive(void *data)
{
   (void)data;
   return !rwebaudio_is_paused;
}

static bool rwebaudio_start(void *data, bool is_shutdown)
{
   (void)data;
   rwebaudio_is_paused = false;
   return RWebAudioStart();
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

   static bool rwebaudio_use_float(void *data)
{
   (void)data;
   return true;
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
};
