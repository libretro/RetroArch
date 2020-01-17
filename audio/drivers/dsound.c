/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#ifndef _XBOX
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#endif

#include <dsound.h>

#include <boolean.h>

#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <retro_timers.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif
#include <lists/string_list.h>
#include <queues/fifo_queue.h>
#include <string/stdstring.h>

#include "../../retroarch.h"
#include "../../verbosity.h"

#ifdef _XBOX
#define DSERR_BUFFERLOST                MAKE_DSHRESULT(150)
#define DSERR_INVALIDPARAM              E_INVALIDARG
#define DSERR_PRIOLEVELNEEDED           MAKE_DSHRESULT(70)
#endif

#if defined(_MSC_VER) && !defined(_XBOX)
#pragma comment(lib, "dsound")
#pragma comment(lib, "dxguid")
#endif

typedef struct dsound
{
   LPDIRECTSOUND ds;
   LPDIRECTSOUNDBUFFER dsb;

   fifo_buffer_t *buffer;
   CRITICAL_SECTION crit;

   HANDLE      event;
#ifdef HAVE_THREADS
   sthread_t *thread;
#else
   HANDLE thread;
#endif

   unsigned buffer_size;

   bool nonblock;
   bool is_paused;
   volatile bool thread_alive;
} dsound_t;

/* Forward declarations */
static void *dsound_list_new(void *u);

static INLINE unsigned write_avail(unsigned read_ptr,
      unsigned write_ptr, unsigned buffer_size)
{
   return (read_ptr + buffer_size - write_ptr) % buffer_size;
}

#define CHUNK_SIZE 256

struct audio_lock
{
   void *chunk1;
   void *chunk2;
   DWORD size1;
   DWORD size2;
};

static bool grab_region(dsound_t *ds, uint32_t write_ptr,
      struct audio_lock *region, HRESULT res)
{
   if (res == DSERR_BUFFERLOST)
   {
#ifdef DEBUG
      RARCH_WARN("[DirectSound error]: %s\n", "DSERR_BUFFERLOST");
#endif
      if ((res = IDirectSoundBuffer_Restore(ds->dsb)) != DS_OK)
         return false;
      if ((res = IDirectSoundBuffer_Lock(ds->dsb, write_ptr, CHUNK_SIZE,
                  &region->chunk1, &region->size1, &region->chunk2, &region->size2, 0)) != DS_OK)
         return false;
      return true;
   }

#ifdef DEBUG
   switch (res)
   {
      case DSERR_INVALIDCALL:
         RARCH_WARN("[DirectSound error]: %s\n", "DSERR_INVALIDCALL");
         break;
      case DSERR_INVALIDPARAM:
         RARCH_WARN("[DirectSound error]: %s\n", "DSERR_INVALIDPARAM");
         break;
      case DSERR_PRIOLEVELNEEDED:
         RARCH_WARN("[DirectSound error]: %s\n", "DSERR_PRIOLEVELNEEDED");
         break;
      default:
         break;
   }
#endif

   return false;
}

#ifdef HAVE_THREADS
static void dsound_thread(void *data)
#else
static DWORD CALLBACK dsound_thread(PVOID data)
#endif
{
   DWORD write_ptr;
   dsound_t *ds = (dsound_t*)data;

   SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

   IDirectSoundBuffer_GetCurrentPosition(ds->dsb, NULL, &write_ptr);
   write_ptr = (write_ptr + ds->buffer_size / 2) % ds->buffer_size;

   while (ds->thread_alive)
   {
      HRESULT res;
      bool is_pull = false;
      struct audio_lock region;
      DWORD read_ptr, avail, fifo_avail;

      IDirectSoundBuffer_GetCurrentPosition(ds->dsb, &read_ptr, NULL);
      avail = write_avail(read_ptr, write_ptr, ds->buffer_size);

      EnterCriticalSection(&ds->crit);
      fifo_avail = fifo_read_avail(ds->buffer);
      LeaveCriticalSection(&ds->crit);

      if (avail < CHUNK_SIZE || ((fifo_avail < CHUNK_SIZE) && (avail < ds->buffer_size / 2)))
      {
         /* No space to write, or we don't have data in our fifo,
          * but we can wait some time before it underruns ... */

         /* We could opt for using the notification interface,
          * but it is not guaranteed to work, so use high
          * priority sleeping patterns.
          */
         retro_sleep(1);
         continue;
      }

      if ((res = IDirectSoundBuffer_Lock(ds->dsb, write_ptr, CHUNK_SIZE,
                  &region.chunk1, &region.size1, &region.chunk2, &region.size2, 0)) != DS_OK)
      {
         if (!grab_region(ds, write_ptr, &region, res))
         {
            ds->thread_alive = false;
            SetEvent(ds->event);
            break;
         }
      }

      if (fifo_avail < CHUNK_SIZE)
      {
         /* Got space to write, but nothing in FIFO (underrun),
          * fill block with silence. */
         memset(region.chunk1, 0, region.size1);
         memset(region.chunk2, 0, region.size2);
      }
      else
      {
         /* All is good. Pull from it and notify FIFO. */

         EnterCriticalSection(&ds->crit);
         if (region.chunk1)
            fifo_read(ds->buffer, region.chunk1, region.size1);
         if (region.chunk2)
            fifo_read(ds->buffer, region.chunk2, region.size2);
         LeaveCriticalSection(&ds->crit);

         is_pull = true;
      }

      IDirectSoundBuffer_Unlock(ds->dsb, region.chunk1,
            region.size1, region.chunk2, region.size2);
      write_ptr = (write_ptr + region.size1 + region.size2) 
         % ds->buffer_size;

      if (is_pull)
         SetEvent(ds->event);
   }

   ExitThread(0);
}

static void dsound_stop_thread(dsound_t *ds)
{
   if (!ds->thread)
      return;

   ds->thread_alive = false;

#ifdef HAVE_THREADS
   sthread_join(ds->thread);
#else
   WaitForSingleObject(ds->thread, INFINITE);
   CloseHandle(ds->thread);
#endif

   ds->thread = NULL;
}

static bool dsound_start_thread(dsound_t *ds)
{
   if (!ds->thread)
   {
      ds->thread_alive = true;

#ifdef HAVE_THREADS
      ds->thread       = sthread_create(dsound_thread, ds);
#else
      ds->thread       = CreateThread(NULL, 0, dsound_thread, ds, 0, NULL);
#endif
      if (!ds->thread)
         return false;
   }

   return true;
}

static void dsound_clear_buffer(dsound_t *ds)
{
   DWORD size;
   void *ptr  = NULL;

   IDirectSoundBuffer_SetCurrentPosition(ds->dsb, 0);

   if (IDirectSoundBuffer_Lock(ds->dsb, 0, 0, &ptr, &size,
            NULL, NULL, DSBLOCK_ENTIREBUFFER) == DS_OK)
   {
      memset(ptr, 0, size);
      IDirectSoundBuffer_Unlock(ds->dsb, ptr, size, NULL, 0);
   }
}

static void dsound_free(void *data)
{
   dsound_t *ds = (dsound_t*)data;

   if (!ds)
      return;

   if (ds->thread)
   {
      ds->thread_alive = false;
#ifdef HAVE_THREADS
      sthread_join(ds->thread);
#else
      WaitForSingleObject(ds->thread, INFINITE);
      CloseHandle(ds->thread);
#endif
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

static BOOL CALLBACK enumerate_cb(LPGUID guid, LPCSTR desc, LPCSTR module, LPVOID context)
{
   union string_list_elem_attr attr;
   struct string_list *list = (struct string_list*)context;

   attr.i = 0;

   string_list_append(list, desc, attr);

   if (guid)
   {
      unsigned i;
      LPGUID guid_copy = (LPGUID)malloc(sizeof(GUID) * 1);
      guid_copy->Data1 = guid->Data1;
      guid_copy->Data2 = guid->Data2;
      guid_copy->Data3 = guid->Data3;
      for (i = 0; i < 8; i++)
         guid_copy->Data4[i] = guid->Data4[i];

      list->elems[list->size-1].userdata = guid_copy;
   }

   return TRUE;
}

static void dsound_set_wavefmt(WAVEFORMATEX *wfx,
      unsigned channels, unsigned samplerate)
{
   wfx->wFormatTag        = WAVE_FORMAT_PCM;
   wfx->nBlockAlign       = channels * sizeof(int16_t);
   wfx->wBitsPerSample    = 16;

   wfx->nChannels         = channels;
   wfx->nSamplesPerSec    = samplerate;
   wfx->nAvgBytesPerSec   = wfx->nSamplesPerSec * wfx->nBlockAlign;
   wfx->cbSize            = 0;
}

static void *dsound_init(const char *dev, unsigned rate, unsigned latency,
      unsigned block_frames,
      unsigned *new_rate)
{
   LPGUID selected_device   = NULL;
   WAVEFORMATEX wfx         = {0};
   DSBUFFERDESC bufdesc     = {0};
   int32_t idx_found        = -1;
   struct string_list *list = (struct string_list*)dsound_list_new(NULL);
   dsound_t          *ds    = (dsound_t*)calloc(1, sizeof(*ds));

   if (!ds)
      goto error;

   InitializeCriticalSection(&ds->crit);

   if (dev)
   {
       /* Search for device name first */
      if (list && list->elems)
      {
         if (list->elems)
         {
            unsigned i;
            for (i = 0; i < list->size; i++)
            {
               if (string_is_equal(dev, list->elems[i].data))
               {
                  idx_found       = i;
                  selected_device = (LPGUID)list->elems[idx_found].userdata;
                  break;
               }
            }
            /* Index was not found yet based on name string,
             * just assume id is a one-character number index. */

            if (idx_found == -1 && isdigit(dev[0]))
            {
               idx_found = strtoul(dev, NULL, 0);
               RARCH_LOG("[DirectSound]: Fallback, device index is a single number index instead: %d.\n", idx_found);

               if (idx_found != -1)
               {
                  if (idx_found < (int32_t)list->size)
                  {
                     RARCH_LOG("[DirectSound]: Corresponding name: %s\n", list->elems[idx_found].data);
                     selected_device = (LPGUID)list->elems[idx_found].userdata;
                  }
               }
            }
         }
      }
   }

   if (DirectSoundCreate(selected_device, &ds->ds, NULL) != DS_OK)
      goto error;

#ifndef _XBOX
   if (IDirectSound_SetCooperativeLevel(ds->ds, GetDesktopWindow(), DSSCL_PRIORITY) != DS_OK)
      goto error;
#endif

   dsound_set_wavefmt(&wfx, 2, rate);

   ds->buffer_size       = (latency * wfx.nAvgBytesPerSec) / 1000;
   ds->buffer_size      /= CHUNK_SIZE;
   ds->buffer_size      *= CHUNK_SIZE;
   if (ds->buffer_size < 4 * CHUNK_SIZE)
      ds->buffer_size    = 4 * CHUNK_SIZE;

   RARCH_LOG("[DirectSound]: Setting buffer size of %u bytes\n", ds->buffer_size);
   RARCH_LOG("[DirectSound]: Latency = %u ms\n", (unsigned)((1000 * ds->buffer_size) / wfx.nAvgBytesPerSec));

   bufdesc.dwSize        = sizeof(DSBUFFERDESC);
   bufdesc.dwFlags       = 0;
#ifndef _XBOX
   bufdesc.dwFlags       = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
#endif
   bufdesc.dwBufferBytes = ds->buffer_size;
   bufdesc.lpwfxFormat   = &wfx;

   ds->event = CreateEvent(NULL, false, false, NULL);
   if (!ds->event)
      goto error;

   ds->buffer = fifo_new(4 * 1024);
   if (!ds->buffer)
      goto error;

   if (IDirectSound_CreateSoundBuffer(ds->ds, &bufdesc, &ds->dsb, 0) != DS_OK)
      goto error;

   IDirectSoundBuffer_SetVolume(ds->dsb, DSBVOLUME_MAX);
   IDirectSoundBuffer_SetCurrentPosition(ds->dsb, 0);

   dsound_clear_buffer(ds);

   if (IDirectSoundBuffer_Play(ds->dsb, 0, 0, DSBPLAY_LOOPING) != DS_OK)
      goto error;

   if (!dsound_start_thread(ds))
      goto error;

   string_list_free(list);
   return ds;

error:
   RARCH_ERR("[DirectSound] Error occurred in init.\n");
   if (list)
      string_list_free(list);
   dsound_free(ds);
   return NULL;
}

static bool dsound_stop(void *data)
{
   dsound_t *ds = (dsound_t*)data;

   dsound_stop_thread(ds);
   ds->is_paused = (IDirectSoundBuffer_Stop(ds->dsb) == DS_OK) ? true : false;

   return (ds->is_paused) ? true : false;
}

static bool dsound_start(void *data, bool is_shutdown)
{
   dsound_t *ds = (dsound_t*)data;

   dsound_clear_buffer(ds);

   if (!dsound_start_thread(ds))
      return false;

   ds->is_paused = (IDirectSoundBuffer_Play(
            ds->dsb, 0, 0, DSBPLAY_LOOPING) == DS_OK) ? false : true;
   return (ds->is_paused) ? false : true;
}

static bool dsound_alive(void *data)
{
   dsound_t *ds = (dsound_t*)data;

   if (!ds)
      return false;
   return !ds->is_paused;
}

static void dsound_set_nonblock_state(void *data, bool state)
{
   dsound_t *ds = (dsound_t*)data;
   if (ds)
      ds->nonblock = state;
}

static ssize_t dsound_write(void *data, const void *buf_, size_t size)
{
   size_t     written = 0;
   dsound_t       *ds = (dsound_t*)data;
   const uint8_t *buf = (const uint8_t*)buf_;

   if (!ds->thread_alive)
      return -1;

   if (ds->nonblock)
   {
      if (size > 0)
      {
         size_t avail;

         EnterCriticalSection(&ds->crit);
         avail = fifo_write_avail(ds->buffer);
         if (avail > size)
            avail = size;

         fifo_write(ds->buffer, buf, avail);
         LeaveCriticalSection(&ds->crit);

         buf     += avail;
         size    -= avail;
         written += avail;
      }
   }
   else
   {
      while (size > 0)
      {
         size_t avail;

         EnterCriticalSection(&ds->crit);
         avail = fifo_write_avail(ds->buffer);
         if (avail > size)
            avail = size;

         fifo_write(ds->buffer, buf, avail);
         LeaveCriticalSection(&ds->crit);

         buf     += avail;
         size    -= avail;
         written += avail;

         if (!ds->thread_alive)
            break;

         if (avail == 0)
            WaitForSingleObject(ds->event, INFINITE);
      }
   }

   return written;
}

static size_t dsound_write_avail(void *data)
{
   size_t avail;
   dsound_t *ds = (dsound_t*)data;

   EnterCriticalSection(&ds->crit);
   avail = fifo_write_avail(ds->buffer);
   LeaveCriticalSection(&ds->crit);
   return avail;
}

static size_t dsound_buffer_size(void *data)
{
   return 4 * 1024;
}

static bool dsound_use_float(void *data)
{
   (void)data;
   return false;
}

static void *dsound_list_new(void *u)
{
   struct string_list *sl          = string_list_new();

   if (!sl)
      return NULL;

#ifndef _XBOX
#ifdef UNICODE
   DirectSoundEnumerate((LPDSENUMCALLBACKW)enumerate_cb, sl);
#else
   DirectSoundEnumerate((LPDSENUMCALLBACKA)enumerate_cb, sl);
#endif
#endif

   return sl;
}

static void dsound_device_list_free(void *u, void *slp)
{
   struct string_list *sl = (struct string_list*)slp;

   if (sl)
      string_list_free(sl);
}

audio_driver_t audio_dsound = {
   dsound_init,
   dsound_write,
   dsound_stop,
   dsound_start,
   dsound_alive,
   dsound_set_nonblock_state,
   dsound_free,
   dsound_use_float,
   "dsound",
   dsound_list_new,
   dsound_device_list_free,
   dsound_write_avail,
   dsound_buffer_size,
};
