/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#if defined(_MSC_VER) && !defined(_XBOX)
#pragma comment(lib, "dsound")
#pragma comment(lib, "dxguid")
#endif

#ifdef _XBOX
#define DSERR_BUFFERLOST                MAKE_DSHRESULT(150)
#define DSERR_INVALIDPARAM              E_INVALIDARG
#define DSERR_PRIOLEVELNEEDED           MAKE_DSHRESULT(70)

// Send the audio signal (stereo, without attenuation) to all existing speakers
static DSMIXBINVOLUMEPAIR dsmbvp[8] = {
   { DSMIXBIN_FRONT_LEFT,    DSBVOLUME_MAX },
   { DSMIXBIN_FRONT_RIGHT,   DSBVOLUME_MAX },
   { DSMIXBIN_FRONT_CENTER,  DSBVOLUME_MAX },
   { DSMIXBIN_FRONT_CENTER,  DSBVOLUME_MAX },
   { DSMIXBIN_BACK_LEFT,     DSBVOLUME_MAX },
   { DSMIXBIN_BACK_RIGHT,    DSBVOLUME_MAX },
   { DSMIXBIN_LOW_FREQUENCY, DSBVOLUME_MAX },
   { DSMIXBIN_LOW_FREQUENCY, DSBVOLUME_MAX },
};
   
static DSMIXBINS dsmb;
#endif

#include "../driver.h"
#include <stdlib.h>
#include "../boolean.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <dsound.h>
#include "../fifo_buffer.h"
#include "../general.h"

typedef struct dsound
{
   LPDIRECTSOUND ds;
   LPDIRECTSOUNDBUFFER dsb;
   HANDLE event;
   bool nonblock;

   fifo_buffer_t *buffer;
   CRITICAL_SECTION crit;

   volatile bool thread_alive;
   HANDLE thread;
   unsigned buffer_size;
} dsound_t;

static inline unsigned write_avail(unsigned read_ptr, unsigned write_ptr, unsigned buffer_size)
{
   return (read_ptr + buffer_size - write_ptr) % buffer_size;
}

static inline void get_positions(dsound_t *ds, DWORD *read_ptr, DWORD *write_ptr)
{
   IDirectSoundBuffer_GetCurrentPosition(ds->dsb, read_ptr, write_ptr);
}

#define CHUNK_SIZE 256

struct audio_lock
{
   void *chunk1;
   DWORD size1;
   void *chunk2;
   DWORD size2;
};

static inline bool grab_region(dsound_t *ds, DWORD write_ptr, struct audio_lock *region)
{
   HRESULT res = IDirectSoundBuffer_Lock(ds->dsb, write_ptr, CHUNK_SIZE, &region->chunk1, &region->size1, &region->chunk2, &region->size2, 0);
   if (res == DSERR_BUFFERLOST)
   {
      res = IDirectSoundBuffer_Restore(ds->dsb);
      if (res != DS_OK)
         return false;

      res = IDirectSoundBuffer_Lock(ds->dsb, write_ptr, CHUNK_SIZE, &region->chunk1, &region->size1, &region->chunk2, &region->size2, 0);
      if (res != DS_OK)
         return false;
   }

   const char *err;
   switch (res)
   {
      case DSERR_BUFFERLOST:
         err = "DSERR_BUFFERLOST";
         break;
      case DSERR_INVALIDCALL:
         err = "DSERR_INVALIDCALL";
         break;
      case DSERR_INVALIDPARAM:
         err = "DSERR_INVALIDPARAM";
         break;
      case DSERR_PRIOLEVELNEEDED:
         err = "DSERR_PRIOLEVELNEEDED";
         break;

      default:
         err = NULL;
   }

   if (err)
   {
      RARCH_WARN("[DirectSound error]: %s\n", err);
      return false;
   }

   return true;
}

static inline void release_region(dsound_t *ds, const struct audio_lock *region)
{
   IDirectSoundBuffer_Unlock(ds->dsb, region->chunk1, region->size1, region->chunk2, region->size2);
}

static DWORD CALLBACK dsound_thread(PVOID data)
{
   dsound_t *ds = (dsound_t*)data;
   SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

   DWORD write_ptr;
   get_positions(ds, NULL, &write_ptr);
   write_ptr = (write_ptr + ds->buffer_size / 2) % ds->buffer_size;

   while (ds->thread_alive)
   {
      DWORD read_ptr;
      get_positions(ds, &read_ptr, NULL);
      
      DWORD avail = write_avail(read_ptr, write_ptr, ds->buffer_size);

      EnterCriticalSection(&ds->crit);
      DWORD fifo_avail = fifo_read_avail(ds->buffer);
      LeaveCriticalSection(&ds->crit);

      // No space to write, or we don't have data in our fifo, but we can wait some time before it underruns ...
      if (avail < CHUNK_SIZE || ((fifo_avail < CHUNK_SIZE) && (avail < ds->buffer_size / 2)))
      {
         Sleep(1);
         // We could opt for using the notification interface,
         // but it is not guaranteed to work, so use high priority sleeping patterns. :(
      }
      else if (fifo_avail < CHUNK_SIZE) // Got space to write, but nothing in FIFO (underrun), fill block with silence.
      {
         struct audio_lock region;
         if (!grab_region(ds, write_ptr, &region))
         {
            ds->thread_alive = false;
            SetEvent(ds->event);
            break;
         }

         memset(region.chunk1, 0, region.size1);
         memset(region.chunk2, 0, region.size2);

         release_region(ds, &region);
         write_ptr = (write_ptr + region.size1 + region.size2) % ds->buffer_size;
      }
      else // All is good. Pull from it and notify FIFO :D
      {
         struct audio_lock region;
         if (!grab_region(ds, write_ptr, &region))
         {
            ds->thread_alive = false;
            SetEvent(ds->event);
            break;
         }

         EnterCriticalSection(&ds->crit);
         if (region.chunk1)
            fifo_read(ds->buffer, region.chunk1, region.size1);
         if (region.chunk2)
            fifo_read(ds->buffer, region.chunk2, region.size2);
         LeaveCriticalSection(&ds->crit);

         release_region(ds, &region);
         write_ptr = (write_ptr + region.size1 + region.size2) % ds->buffer_size;

         SetEvent(ds->event);
      }
   }

   ExitThread(0);
}

static void dsound_stop_thread(dsound_t *ds)
{
   if (ds->thread)
   {
      ds->thread_alive = false;
      WaitForSingleObject(ds->thread, INFINITE);
      CloseHandle(ds->thread);
      ds->thread = NULL;
   }
}

static bool dsound_start_thread(dsound_t *ds)
{
   if (!ds->thread)
   {
      ds->thread_alive = true;
      ds->thread = CreateThread(NULL, 0, dsound_thread, ds, 0, NULL);
      if (ds->thread == NULL)
         return false;
   }

   return true;
}

static void dsound_clear_buffer(dsound_t *ds)
{
   IDirectSoundBuffer_SetCurrentPosition(ds->dsb, 0);
   void *ptr;
   DWORD size;

   if (IDirectSoundBuffer_Lock(ds->dsb, 0, 0, &ptr, &size, NULL, NULL, DSBLOCK_ENTIREBUFFER) == DS_OK)
   {
      memset(ptr, 0, size);
      IDirectSoundBuffer_Unlock(ds->dsb, ptr, size, NULL, 0);
   }
}

static void dsound_free(void *data)
{
   dsound_t *ds = (dsound_t*)data;
   if (ds)
   {
      if (ds->thread)
      {
         ds->thread_alive = false;
         WaitForSingleObject(ds->thread, INFINITE);
         CloseHandle(ds->thread);
      }

      DeleteCriticalSection(&ds->crit);

      if (ds->dsb)
      {
         IDirectSoundBuffer_Stop(ds->dsb);
         IDirectSoundBuffer_Release(ds->dsb);
      }

      if (ds->ds)
         IDirectSound_Release(ds->ds);

      if (ds->event)
         CloseHandle(ds->event);

      if (ds->buffer)
         fifo_free(ds->buffer);

      free(ds);
   }
}

struct dsound_dev
{
   unsigned device;
   unsigned total_count;
   LPGUID guid;
};

static BOOL CALLBACK enumerate_cb(LPGUID guid, LPCSTR desc, LPCSTR module, LPVOID context)
{
   struct dsound_dev *dev = (struct dsound_dev*)context;
   RARCH_LOG("\t%u: %s\n", dev->total_count, desc);
   if (dev->device == dev->total_count)
      dev->guid = guid;
   dev->total_count++;
   return TRUE;
}

static void *dsound_init(const char *device, unsigned rate, unsigned latency)
{
   WAVEFORMATEX wfx = {0};
   DSBUFFERDESC bufdesc = {0};
   struct dsound_dev dev = {0};

   dsound_t *ds = (dsound_t*)calloc(1, sizeof(*ds));
   if (!ds)
      goto error;

   InitializeCriticalSection(&ds->crit);

   if (device)
      dev.device = strtoul(device, NULL, 0);

   RARCH_LOG("DirectSound devices:\n");
#ifndef _XBOX
   DirectSoundEnumerate(enumerate_cb, &dev);
#endif

   if (DirectSoundCreate(dev.guid, &ds->ds, NULL) != DS_OK)
      goto error;

#ifndef _XBOX
   if (IDirectSound_SetCooperativeLevel(ds->ds, GetDesktopWindow(), DSSCL_PRIORITY) != DS_OK)
      goto error;
#endif

   wfx.wFormatTag = WAVE_FORMAT_PCM;
   wfx.nChannels = 2;
   wfx.nSamplesPerSec = rate;
   wfx.wBitsPerSample = 16;
   wfx.nBlockAlign = 2 * sizeof(int16_t);
   wfx.nAvgBytesPerSec = rate * 2 * sizeof(int16_t);

   ds->buffer_size = (latency * wfx.nAvgBytesPerSec) / 1000;
   ds->buffer_size /= CHUNK_SIZE;
   ds->buffer_size *= CHUNK_SIZE;
   if (ds->buffer_size < 4 * CHUNK_SIZE)
      ds->buffer_size = 4 * CHUNK_SIZE;

   RARCH_LOG("[DirectSound]: Setting buffer size of %u bytes\n", ds->buffer_size);
   RARCH_LOG("[DirectSound]: Latency = %u ms\n", (unsigned)((1000 * ds->buffer_size) / wfx.nAvgBytesPerSec));

   bufdesc.dwSize = sizeof(DSBUFFERDESC);
#ifdef _XBOX
   bufdesc.dwFlags = 0;
#else
   bufdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
#endif
   bufdesc.dwBufferBytes = ds->buffer_size;
   bufdesc.lpwfxFormat = &wfx;

   ds->event = CreateEvent(NULL, false, false, NULL);
   if (!ds->event)
      goto error;

   ds->buffer = fifo_new(4 * 1024);
   if (!ds->buffer)
      goto error;

   if (IDirectSound_CreateSoundBuffer(ds->ds, &bufdesc, &ds->dsb, 0) != DS_OK)
      goto error;

   IDirectSoundBuffer_SetVolume(ds->dsb, DSBVOLUME_MAX);

#ifdef _XBOX
   if(g_extern.console.sound.volume_level == 1)
   {
      dsmb.dwMixBinCount = 8;
      dsmb.lpMixBinVolumePairs = dsmbvp;
      
      IDirectSoundBuffer_SetHeadroom(ds->dsb, DSBHEADROOM_MIN);
      IDirectSoundBuffer_SetMixBins(ds->dsb, &dsmb);
   }
#endif

   IDirectSoundBuffer_SetCurrentPosition(ds->dsb, 0);

   dsound_clear_buffer(ds);

   if (IDirectSoundBuffer_Play(ds->dsb, 0, 0, DSBPLAY_LOOPING) != DS_OK)
      goto error;

   if (!dsound_start_thread(ds))
      goto error;

   return ds;

error:
   RARCH_ERR("[DirectSound] Error occured in init.\n");
   dsound_free(ds);
   return NULL;
}

static bool dsound_stop(void *data)
{
   dsound_t *ds = (dsound_t*)data;
   dsound_stop_thread(ds);
   return IDirectSoundBuffer_Stop(ds->dsb) == DS_OK;
}

static bool dsound_start(void *data)
{
   dsound_t *ds = (dsound_t*)data;
   dsound_clear_buffer(ds);

   if (!dsound_start_thread(ds))
      return false;

   return IDirectSoundBuffer_Play(ds->dsb, 0, 0, DSBPLAY_LOOPING) == DS_OK;
}

static void dsound_set_nonblock_state(void *data, bool state)
{
   dsound_t *ds = (dsound_t*)data;
   ds->nonblock = state;
}

static ssize_t dsound_write(void *data, const void *buf_, size_t size)
{
   dsound_t *ds = (dsound_t*)data;
   const uint8_t *buf = (const uint8_t*)buf_;

   if (!ds->thread_alive)
      return -1;

   size_t written = 0;
   while (size > 0)
   {
      EnterCriticalSection(&ds->crit);
      size_t avail = fifo_write_avail(ds->buffer);
      if (avail > size)
         avail = size;

      fifo_write(ds->buffer, buf, avail);
      LeaveCriticalSection(&ds->crit);

      buf += avail;
      size -= avail;
      written += avail;

      if (ds->nonblock || !ds->thread_alive)
         break;

      if (avail == 0)
         WaitForSingleObject(ds->event, INFINITE);
   }

   return written;
}

static size_t dsound_write_avail(void *data)
{
   dsound_t *ds = (dsound_t*)data;
   EnterCriticalSection(&ds->crit);
   size_t avail = fifo_write_avail(ds->buffer);
   LeaveCriticalSection(&ds->crit);
   return avail;
}

static size_t dsound_buffer_size(void *data)
{
   return 4 * 1024;
}

const audio_driver_t audio_dsound = {
   dsound_init,
   dsound_write,
   dsound_stop,
   dsound_start,
   dsound_set_nonblock_state,
   dsound_free,
   NULL,
   "dsound",
   dsound_write_avail,
   dsound_buffer_size,
};

