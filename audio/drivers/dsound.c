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
#include <boolean.h>

#ifndef _XBOX
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#endif

#include <dsound.h>

#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <retro_timers.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif
#include <lists/string_list.h>
#include <retro_atomic.h>
#include <retro_spsc.h>
#include <string/stdstring.h>

#if defined(_WIN32_WINNT) && (_WIN32_WINNT >= 0x0600 /*_WIN32_WINNT_VISTA */)
#ifndef HAVE_MMDEVICE
#define HAVE_MMDEVICE
#endif
#endif

#ifdef HAVE_MMDEVICE
#include "../common/mmdevice_common.h"
#include "../common/mmdevice_common_inline.h"
#endif

#include "../audio_driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"

#ifdef _XBOX
#define DSERR_BUFFERLOST                MAKE_DSHRESULT(150)
#define DSERR_INVALIDPARAM              E_INVALIDARG
#define DSERR_PRIOLEVELNEEDED           MAKE_DSHRESULT(70)
#endif

#if defined(_MSC_VER) && !defined(_XBOX)
#pragma comment(lib, "dsound")
#endif

#define CHUNK_SIZE 256
#define DSOUND_TIMEOUT 256

typedef struct dsound
{
   /* Read-mostly after init.  Both threads load ds and dsb on every
    * iteration of their respective loops. */
   LPDIRECTSOUND ds;
   LPDIRECTSOUNDBUFFER dsb;

   /* Lock-free SPSC between dsound_write (producer, emulator thread)
    * and dsound_thread (consumer).  Embedded by value so its lifetime
    * tracks dsound_t; initialised in dsound_init, released in
    * dsound_free after the worker has been joined. */
   retro_spsc_t ring;

   HANDLE      event;
#ifdef HAVE_THREADS
   sthread_t *thread;
#else
   HANDLE thread;
#endif
#ifdef HAVE_MMDEVICE
#ifdef HAVE_THREADS
   sthread_t *imm_thread;
#else
   HANDLE imm_thread;
#endif
#endif
   size_t fifo_bufsize;
   unsigned buffer_size;

   bool nonblock;
   bool is_paused;
   bool use_float;
   /* Written by both the main thread (start/stop) and the mixer thread
    * (which clears it when the buffer can no longer be locked), and
    * read by both. volatile carries no ordering under MSVC
    * /volatile:iso, which is what ARM64 builds get, so state it like
    * the ring buffer beside it already does. */
   retro_atomic_int_t thread_alive;

   /* The play cursor unwrapped, in frames, for the sink rate estimate:
    * the mixer thread reads the cursor every pass, well within a
    * buffer's length, and adds each advance here. Kept in a 32-bit
    * atomic so the reader on another thread sees whole values; at
    * 48 kHz it holds twelve hours, and the estimate treats a wrap as a
    * restart. */
   retro_atomic_int_t frames_played;
   DWORD              last_read_ptr;
} dsound_t;

struct audio_lock
{
   void *chunk1;
   void *chunk2;
   DWORD size1;
   DWORD size2;
};

#ifdef HAVE_MMDEVICE
static void dsound_imm_stop_thread(dsound_t *ds)
{
   if (!ds->imm_thread)
      return;

   PostThreadMessage(IMMNotificationThreadId, WM_QUIT, 0, 0);

#ifdef HAVE_THREADS
   sthread_join(ds->imm_thread);
#else
   WaitForSingleObject(ds->imm_thread, DSOUND_TIMEOUT);
   CloseHandle(ds->imm_thread);
#endif

   IMMNotificationThreadId = 0;
   ds->imm_thread = NULL;
}

static bool dsound_imm_start_thread(dsound_t *ds)
{
   if (!ds->imm_thread)
   {
#ifdef HAVE_THREADS
      ds->imm_thread = sthread_create(mmdevice_thread, ds);
#else
      ds->imm_thread = CreateThread(NULL, 0, mmdevice_thread, ds, 0, NULL);
#endif
      if (!ds->imm_thread)
         return false;
   }
   return true;
}
#endif

static BOOL CALLBACK dsound_enumerate_cb(LPGUID guid,
      LPCSTR desc, LPCSTR module, LPVOID context)
{
   union string_list_elem_attr attr;
   struct string_list *list = (struct string_list*)context;

   attr.i = 0;

   string_list_append(list, desc, attr);

   if (guid)
   {
      LPGUID guid_copy = (LPGUID)malloc(sizeof(GUID) * 1);

      if (guid_copy)
      {
         int i;

         guid_copy->Data1 = guid->Data1;
         guid_copy->Data2 = guid->Data2;
         guid_copy->Data3 = guid->Data3;
         for (i = 0; i < 8; i++)
            guid_copy->Data4[i] = guid->Data4[i];

         list->elems[list->size - 1].userdata = guid_copy;
      }
   }

   return TRUE;
}

static void *dsound_device_list_new(void *u)
{
   struct string_list *sl = string_list_new();

   if (!sl)
      return NULL;

#ifndef _XBOX
#ifdef UNICODE
   DirectSoundEnumerate((LPDSENUMCALLBACKW)dsound_enumerate_cb, sl);
#else
   DirectSoundEnumerate((LPDSENUMCALLBACKA)dsound_enumerate_cb, sl);
#endif
#endif

   return sl;
}

/* Both pointers are already reduced modulo buffer_size, so the sum is
 * strictly below 2 * buffer_size and a conditional subtract is exact.
 * The equal case yields buffer_size, which folds to 0 - same as the
 * modulo it replaces. */
static INLINE unsigned _dsound_write_avail(unsigned read_ptr,
      unsigned write_ptr, unsigned buffer_size)
{
   unsigned avail = read_ptr + buffer_size - write_ptr;
   if (avail >= buffer_size)
      avail -= buffer_size;
   return avail;
}

static bool dsound_grab_region(dsound_t *ds, uint32_t write_ptr,
      struct audio_lock *region, HRESULT res)
{
   if (res == DSERR_BUFFERLOST)
   {
#ifdef DEBUG
      RARCH_WARN("[DirectSound] %s.\n", "DSERR_BUFFERLOST");
#endif
      if ((IDirectSoundBuffer_Restore(ds->dsb)) == DS_OK)
         if ((IDirectSoundBuffer_Lock(ds->dsb, write_ptr, CHUNK_SIZE,
                     &region->chunk1, &region->size1, &region->chunk2, &region->size2, 0)) == DS_OK)
            return true;
   }
#ifdef DEBUG
   else
   {
      switch (res)
      {
         case DSERR_INVALIDCALL:
            RARCH_WARN("[DirectSound] %s.\n", "DSERR_INVALIDCALL");
            break;
         case DSERR_INVALIDPARAM:
            RARCH_WARN("[DirectSound] %s.\n", "DSERR_INVALIDPARAM");
            break;
         case DSERR_PRIOLEVELNEEDED:
            RARCH_WARN("[DirectSound] %s.\n", "DSERR_PRIOLEVELNEEDED");
            break;
         default:
            break;
      }
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
   DWORD write_ptr = 0;
   dsound_t *ds    = (dsound_t*)data;

   SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

   IDirectSoundBuffer_GetCurrentPosition(ds->dsb, &ds->last_read_ptr, &write_ptr);
   write_ptr += ds->buffer_size / 2;
   if (write_ptr >= ds->buffer_size)
      write_ptr -= ds->buffer_size;

   while (retro_atomic_load_acquire_int(&ds->thread_alive))
   {
      HRESULT res;
      bool is_pull = false;
      struct audio_lock region;
      DWORD read_ptr, avail, fifo_avail;

      IDirectSoundBuffer_GetCurrentPosition(ds->dsb, &read_ptr, NULL);
      avail = _dsound_write_avail(read_ptr, write_ptr, ds->buffer_size);

      /* The cursor's advance since the last pass, unwrapped: the
       * device's consumption, silence included. */
      {
         DWORD adv = read_ptr + ds->buffer_size - ds->last_read_ptr;
         if (adv >= ds->buffer_size)
            adv -= ds->buffer_size;
         ds->last_read_ptr = read_ptr;
         retro_atomic_fetch_add_int(&ds->frames_played,
               (int)(adv / (ds->use_float ? 8 : 4)));
      }

      /* Consumer-side query; only this thread advances the read
       * cursor, so the value cannot shrink under us between here and
       * the drain below. */
      fifo_avail = (DWORD)retro_spsc_read_avail(&ds->ring);

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
         if (!dsound_grab_region(ds, write_ptr, &region, res))
         {
            retro_atomic_store_release_int(&ds->thread_alive, 0);
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
         /* All is good. Pull from it and notify the producer. */
         size_t got1 = 0;
         size_t got2 = 0;

         if (region.chunk1)
            got1 = retro_spsc_read(&ds->ring, region.chunk1, region.size1);
         if (region.chunk2)
            got2 = retro_spsc_read(&ds->ring, region.chunk2, region.size2);

         /* fifo_avail >= CHUNK_SIZE == size1 + size2 was established
          * above and only this thread consumes, so a short read is not
          * reachable.  Silence any remainder anyway rather than commit
          * whatever the DirectSound buffer happened to hold - the old
          * fifo_read had no length feedback at all, so this class of
          * bug could not even be detected. */
         if (region.chunk1 && got1 < region.size1)
            memset((uint8_t*)region.chunk1 + got1, 0, region.size1 - got1);
         if (region.chunk2 && got2 < region.size2)
            memset((uint8_t*)region.chunk2 + got2, 0, region.size2 - got2);

         is_pull = true;
      }

      IDirectSoundBuffer_Unlock(ds->dsb, region.chunk1,
            region.size1, region.chunk2, region.size2);
      /* write_ptr < buffer_size and the locked region is CHUNK_SIZE,
       * which buffer_size is both a multiple of and at least four
       * times, so one conditional subtract is enough. */
      write_ptr += region.size1 + region.size2;
      if (write_ptr >= ds->buffer_size)
         write_ptr -= ds->buffer_size;

      if (is_pull)
         SetEvent(ds->event);
   }

   /* Return normally: under HAVE_THREADS this function runs inside the
    * sthread wrapper, which frees its thunk allocation after the
    * function returns - ExitThread here skipped that epilogue and
    * leaked the thunk on every driver teardown. */
#ifndef HAVE_THREADS
   return 0;
#endif
}

static void dsound_stop_thread(dsound_t *ds)
{
   if (!ds->thread)
      return;

   retro_atomic_store_release_int(&ds->thread_alive, 0);

#ifdef HAVE_THREADS
   sthread_join(ds->thread);
#else
   WaitForSingleObject(ds->thread, DSOUND_TIMEOUT);
   CloseHandle(ds->thread);
#endif

   ds->thread = NULL;
}

static bool dsound_start_thread(dsound_t *ds)
{
   if (!ds->thread)
   {
      retro_atomic_store_release_int(&ds->thread_alive, 1);
#ifdef HAVE_THREADS
      ds->thread = sthread_create(dsound_thread, ds);
#else
      ds->thread = CreateThread(NULL, 0, dsound_thread, ds, 0, NULL);
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

#ifdef HAVE_MMDEVICE
   dsound_imm_stop_thread(ds);
#endif
   dsound_stop_thread(ds);

   if (ds->dsb)
   {
      IDirectSoundBuffer_Stop(ds->dsb);
      IDirectSoundBuffer_Release(ds->dsb);
   }

   if (ds->ds)
      IDirectSound_Release(ds->ds);

   if (ds->event)
      CloseHandle(ds->event);

   /* Safe here and only here: dsound_stop_thread has joined the
    * consumer, so the ring has no live reader. */
   retro_spsc_free(&ds->ring);

   free(ds);
}

static void dsound_set_format(WAVEFORMATEX *wf,
      bool float_fmt, unsigned channels, unsigned rate)
{
   WORD wBitsPerSample   = float_fmt ? 32 : 16;
   WORD nBlockAlign      = (channels * wBitsPerSample) / 8;
   DWORD nAvgBytesPerSec = rate * nBlockAlign;

   if (float_fmt)
      wf->wFormatTag     = WAVE_FORMAT_IEEE_FLOAT;
   else
      wf->wFormatTag     = WAVE_FORMAT_PCM;

   wf->nChannels         = channels;
   wf->nSamplesPerSec    = rate;
   wf->nAvgBytesPerSec   = nAvgBytesPerSec;
   wf->nBlockAlign       = nBlockAlign;
   wf->wBitsPerSample    = wBitsPerSample;

   wf->cbSize            = 0;
}

static const char *dsound_wave_format_name(const WAVEFORMATEX *format)
{
   switch (format->wFormatTag)
   {
      case WAVE_FORMAT_PCM:
         return "WAVE_FORMAT_PCM";
      case WAVE_FORMAT_IEEE_FLOAT:
         return "WAVE_FORMAT_IEEE_FLOAT";
      default:
         break;
   }

   return "<unknown>";
}

/* The two stages, from the setting and the format the device took.
 * The staging fifo dsound_write() fills is what rate control measures
 * and holds half full; the DirectSound ring the notify thread keeps
 * full sits behind it, and what the user hears is the ring plus half
 * the fifo. Both used to be sized to the setting, so a 64 ms setting
 * played at about 96. The fifo holds the setting - rounded up to a
 * power of two by retro_spsc, and reported at its real capacity - and
 * the ring takes what is left of the setting after half the fifo, so
 * the two add up to it, floored at 16 ms - or the setting, if lower -
 * to ride out the scheduler between the thread's 1 ms polls, and at
 * four chunks in any case. */
static void dsound_size_stages(dsound_t *ds, unsigned latency,
      const WAVEFORMATEX *wf)
{
   size_t setting_bytes = ((size_t)latency * wf->nAvgBytesPerSec) / 1000;
   size_t fifo_capacity = 4 * 1024;
   size_t floor_bytes   = (16 * (size_t)wf->nAvgBytesPerSec) / 1000;

   /* Never a larger ring than the setting asked for in total: at a
    * setting under 16 ms the ring is the setting, as it always was. */
   if (floor_bytes > setting_bytes)
      floor_bytes       = setting_bytes;
   while (fifo_capacity < setting_bytes)
      fifo_capacity   <<= 1;
   ds->fifo_bufsize     = fifo_capacity;
   ds->buffer_size      = (setting_bytes > fifo_capacity / 2)
         ? (unsigned)(setting_bytes - fifo_capacity / 2) : 0;
   if (ds->buffer_size < floor_bytes)
      ds->buffer_size   = (unsigned)floor_bytes;
   ds->buffer_size     /= CHUNK_SIZE;
   ds->buffer_size     *= CHUNK_SIZE;
   if (ds->buffer_size < 4 * CHUNK_SIZE)
      ds->buffer_size   = 4 * CHUNK_SIZE;
}

static void *dsound_init(const char *dev, unsigned rate, unsigned latency,
      unsigned block_frames, unsigned *new_rate)
{
   LPGUID selected_device = NULL;
   WAVEFORMATEX wf        = {0};
   DSBUFFERDESC bufdesc   = {0};
   bool want_float        = (config_get_ptr()->uints.audio_format_negotiation
         == AUDIO_FORMAT_NEGOTIATION_FLOAT);
   dsound_t *ds           = (dsound_t*)calloc(1, sizeof(*ds));

   if (!ds)
      return NULL;


   if (dev)
   {
      struct string_list *list = (struct string_list*)dsound_device_list_new(NULL);

       /* Search for device name first */
      if (list && list->elems)
      {
         int32_t idx_found = -1;
         if (list->elems)
         {
            size_t i;
            for (i = 0; i < list->size; i++)
            {
               if (string_is_equal(dev, list->elems[i].data))
               {
                  RARCH_DBG("[DirectSound] Found device #%d: \"%s\".\n", i, list->elems[i].data);
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
               RARCH_LOG("[DirectSound] Fallback, device index is a single number index instead: %d.\n", idx_found);

               if (idx_found != -1)
               {
                  if (idx_found < (int32_t)list->size)
                  {
                     RARCH_LOG("[DirectSound] Corresponding name: %s.\n", list->elems[idx_found].data);
                     selected_device = (LPGUID)list->elems[idx_found].userdata;
                  }
               }
            }
         }
      }

      string_list_free(list);
   }

   if (DirectSoundCreate(selected_device, &ds->ds, NULL) != DS_OK)
      goto error;

#ifndef _XBOX
   if (IDirectSound_SetCooperativeLevel(ds->ds, GetDesktopWindow(), DSSCL_PRIORITY) != DS_OK)
      goto error;
#endif

   dsound_set_format(&wf, want_float, 2, rate);
   RARCH_DBG("[DirectSound] Requesting %u-bit %u-channel client with %s samples at %uHz %ums.\n",
         wf.wBitsPerSample,
         wf.nChannels,
         dsound_wave_format_name(&wf),
         wf.nSamplesPerSec,
         latency);

   dsound_size_stages(ds, latency, &wf);

   bufdesc.dwSize        = sizeof(DSBUFFERDESC);
   bufdesc.dwFlags       = 0;
#ifndef _XBOX
   bufdesc.dwFlags       = DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_GLOBALFOCUS;
#endif
   bufdesc.dwBufferBytes = ds->buffer_size;
   bufdesc.lpwfxFormat   = &wf;

   ds->event = CreateEvent(NULL, false, false, NULL);
   if (!ds->event)
      goto error;

   if (IDirectSound_CreateSoundBuffer(ds->ds, &bufdesc, &ds->dsb, 0) != DS_OK)
   {
      /* Only a float request has a lower format to fall back to. An int16
       * request that fails has nowhere lower to go, so it errors out. */
      if (!want_float)
         goto error;

      RARCH_WARN("[DirectSound] Failed to create float buffer, falling back to 16-bit PCM.\n");

      dsound_set_format(&wf, false, 2, rate);
      dsound_size_stages(ds, latency, &wf);

      bufdesc.dwBufferBytes = ds->buffer_size;
      bufdesc.lpwfxFormat   = &wf;

      if (IDirectSound_CreateSoundBuffer(ds->ds, &bufdesc, &ds->dsb, 0) != DS_OK)
         goto error;

      ds->use_float = false;
   }
   else
      ds->use_float = want_float;

   /* Staging fifo between dsound_write and the feeder thread.  This is
    * the only occupancy rate control can observe (write_avail /
    * buffer_size report it), so it must span a useful measurement
    * window: the previous fixed 4 KiB was 10.7 ms of float at 48 kHz -
    * smaller than a single video frame's write, which slammed the
    * controller's occupancy signal rail-to-rail on every batch.
    * Mirror the DirectSound ring (latency-scaled, format-aware via
    * nAvgBytesPerSec, already CHUNK_SIZE-aligned), sized after the
    * float->int16 fallback so both agree on the final format.  Keep
    * the old 4 KiB as the floor for very low latency settings. */
   if (!retro_spsc_init(&ds->ring, ds->fifo_bufsize))
      goto error;
   /* retro_spsc_init rounds capacity up to a power of 2.  Report the
    * true capacity as the driver buffer size, so rate control computes
    * its setpoint against the ring's real bounds rather than the
    * pre-rounding request.  An empty ring's write avail is exactly its
    * capacity. */
   ds->fifo_bufsize = retro_spsc_write_avail(&ds->ring);

   RARCH_LOG("[DirectSound] Initialized %u-bit %s: %u ms setting as a %u-byte fifo (%u ms, rate control holds it about half full) in front of a %u-byte ring (%u ms); about %u ms from write to the device.\n",
         wf.wBitsPerSample,
         dsound_wave_format_name(&wf),
         latency,
         (unsigned)ds->fifo_bufsize,
         (unsigned)((1000 * ds->fifo_bufsize) / wf.nAvgBytesPerSec),
         ds->buffer_size,
         (unsigned)((1000 * ds->buffer_size) / wf.nAvgBytesPerSec),
         (unsigned)((1000 * (ds->fifo_bufsize / 2 + ds->buffer_size)) / wf.nAvgBytesPerSec));

   IDirectSoundBuffer_SetVolume(ds->dsb, DSBVOLUME_MAX);
   IDirectSoundBuffer_SetCurrentPosition(ds->dsb, 0);

   dsound_clear_buffer(ds);

#ifdef HAVE_MMDEVICE
   dsound_imm_start_thread(ds);
#endif

   if (IDirectSoundBuffer_Play(ds->dsb, 0, 0, DSBPLAY_LOOPING) == DS_OK)
      if (dsound_start_thread(ds))
         return ds;

error:
   RARCH_ERR("[DirectSound] Error occurred in init.\n");
   dsound_free(ds);
   return NULL;
}

static bool dsound_stop(void *data)
{
   dsound_t *ds = (dsound_t*)data;
   dsound_stop_thread(ds);
   ds->is_paused = (IDirectSoundBuffer_Stop(ds->dsb) == DS_OK) ? true : false;
   return ds->is_paused;
}

static bool dsound_start(void *data, bool is_shutdown)
{
   dsound_t *ds = (dsound_t*)data;

   dsound_clear_buffer(ds);

   if (!dsound_start_thread(ds))
      return false;

   ds->is_paused = (IDirectSoundBuffer_Play(
            ds->dsb, 0, 0, DSBPLAY_LOOPING) == DS_OK) ? false : true;
   return !ds->is_paused;
}

static bool dsound_alive(void *data)
{
   dsound_t *ds = (dsound_t*)data;
   return ds && !ds->is_paused;
}

static void dsound_set_nonblock_state(void *data, bool state)
{
   dsound_t *ds = (dsound_t*)data;
   if (ds)
      ds->nonblock = state;
}

static ssize_t dsound_write(void *data, const void *buf_, size_t len)
{
   size_t _len = 0;
   dsound_t       *ds = (dsound_t*)data;
   const uint8_t *buf = (const uint8_t*)buf_;

   if (!retro_atomic_load_acquire_int(&ds->thread_alive))
      return -1;

   if (ds->nonblock)
   {
      if (len > 0)
      {
         size_t avail;

         avail = retro_spsc_write_avail(&ds->ring);
         if (avail > len)
            avail = len;

         retro_spsc_write(&ds->ring, buf, avail);

         _len += avail;
      }
   }
   else
   {
      while (len > 0)
      {
         size_t avail;

         avail = retro_spsc_write_avail(&ds->ring);
         if (avail > len)
            avail = len;

         retro_spsc_write(&ds->ring, buf, avail);

         buf  += avail;
         _len += avail;
         len  -= avail;

         if (!retro_atomic_load_acquire_int(&ds->thread_alive))
            break;

         /* The notify thread sets the event after every block it moves
          * and clears thread_alive when the buffer is lost for good, so
          * a period with no event is a play cursor that has stalled,
          * not a device gone: the write returns what went, and a lost
          * buffer still reports through the flag on the next call. */
         if (avail == 0 && !(WaitForSingleObject(ds->event, DSOUND_TIMEOUT) == WAIT_OBJECT_0))
            break;
      }
   }

   return _len;
}

/* Sleep on the event the notify thread sets after every block it moves
 * into the DirectSound buffer until at least len bytes fit in the ring,
 * capped at half of it so the wait always ends. Returns the free space
 * then, or 0 once the thread has died or the event stays silent past
 * the timeout. */
static size_t dsound_wait_writable(void *data, size_t len)
{
   dsound_t *ds = (dsound_t*)data;
   size_t avail;
   int laps     = 8;

   if (len > ds->fifo_bufsize / 2)
      len = ds->fifo_bufsize / 2;

   for (;;)
   {
      if (!retro_atomic_load_acquire_int(&ds->thread_alive))
         return 0;
      avail = retro_spsc_write_avail(&ds->ring);
      if (avail >= len)
         return avail;
      /* Each wait is bounded; this bounds the loop, for a thread that
       * keeps moving blocks but never frees enough. */
      if (--laps < 0)
         return 0;
      if (WaitForSingleObject(ds->event, DSOUND_TIMEOUT) != WAIT_OBJECT_0)
         return 0;
   }
}

static size_t dsound_write_avail(void *data)
{
   size_t avail;
   dsound_t *ds = (dsound_t*)data;

   avail = retro_spsc_write_avail(&ds->ring);
   return avail;
}

static size_t dsound_frames_consumed(void *data)
{
   dsound_t *ds = (dsound_t*)data;
   if (!ds)
      return 0;
   return (size_t)(unsigned)retro_atomic_load_acquire_int(&ds->frames_played);
}

static size_t dsound_buffer_size(void *data)
{
   dsound_t *ds = (dsound_t*)data;
   return ds->fifo_bufsize;
}

static bool dsound_use_float(void *data)
{
   dsound_t *ds = (dsound_t*)data;
   return ds && ds->use_float;
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
   dsound_device_list_new,
   dsound_device_list_free,
   dsound_write_avail,
   dsound_buffer_size,
   NULL, /* write_raw */
   dsound_wait_writable,
   dsound_frames_consumed
};
