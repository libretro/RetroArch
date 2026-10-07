/*  RetroArch - A frontend for libretro.
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

/* Audio through the wave output functions of the Windows multimedia
 * library (waveOutOpen and the rest, winmm.dll): what every Windows
 * has had since 3.1, and the only audio a build for Windows 95 or
 * NT 4.0 with no DirectX can count on.
 *
 * The device is given a ring of blocks. A block is filled by write()
 * and handed over when it is full; the device plays them in order and
 * hands each back marked done, setting an event as it does. Nothing
 * here runs on the device's thread - no callback function, so nothing
 * to lock: a block is the driver's or the device's, and the done mark
 * is the one word that crosses, read where it is needed. */

#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <mmsystem.h>

#include <boolean.h>
#include <retro_miscellaneous.h>

#include "../audio_driver.h"
#include "../../verbosity.h"

/* The ring: enough blocks that one more or less with the device is a
 * small step for the rate control, few enough to be cheap to go
 * through. */
#define WAVEOUT_BLOCKS       8
/* A block no shorter than this, in frames: a device is not asked to
 * take a block every millisecond. */
#define WAVEOUT_BLOCK_FRAMES 256
/* 16-bit stereo */
#define WAVEOUT_FRAME_BYTES  4

typedef struct waveout
{
   HWAVEOUT hwo;
   HANDLE   event;        /* set as the device hands a block back */
   WAVEHDR *hdr;          /* the blocks' headers */
   char    *data;         /* ... and their memory, in one piece */
   size_t   block_bytes;
   size_t   fill;         /* bytes in the block being filled */
   size_t   frames_done;  /* frames in the blocks handed back so far */
   unsigned cur;          /* the block being filled */
   unsigned oldest;       /* the block the device hands back next */
   unsigned queued;       /* blocks with the device */
   bool     nonblock;
   bool     paused;
} waveout_t;

/* Take back the blocks the device is done with. They come back in the
 * order they went. */
static void waveout_reclaim(waveout_t *wo)
{
   while (     wo->queued
            && (wo->hdr[wo->oldest].dwFlags & WHDR_DONE))
   {
      wo->frames_done += wo->hdr[wo->oldest].dwBufferLength
         / WAVEOUT_FRAME_BYTES;
      wo->oldest       = (wo->oldest + 1) % WAVEOUT_BLOCKS;
      wo->queued--;
   }
}

static void waveout_free(void *data)
{
   waveout_t *wo = (waveout_t*)data;
   unsigned i;

   if (!wo)
      return;
   if (wo->hwo)
   {
      /* every block comes back, played or not */
      waveOutReset(wo->hwo);
      if (wo->hdr)
         for (i = 0; i < WAVEOUT_BLOCKS; i++)
            if (wo->hdr[i].dwFlags & WHDR_PREPARED)
               waveOutUnprepareHeader(wo->hwo, &wo->hdr[i], sizeof(WAVEHDR));
      waveOutClose(wo->hwo);
   }
   if (wo->event)
      CloseHandle(wo->event);
   free(wo->hdr);
   free(wo->data);
   free(wo);
}

static void *waveout_init(const char *device, unsigned rate,
      unsigned latency, unsigned *new_rate)
{
   WAVEFORMATEX fmt;
   unsigned i;
   UINT dev       = WAVE_MAPPER;
   size_t frames;
   waveout_t *wo  = (waveout_t*)calloc(1, sizeof(*wo));

   if (!wo)
      return NULL;

   /* a device by its number, as waveOutGetDevCaps() counts them */
   if (device && device[0] >= '0' && device[0] <= '9')
      dev = (UINT)strtoul(device, NULL, 10);

   /* the latency asked for, across the ring */
   frames = ((size_t)rate * latency / 1000 + WAVEOUT_BLOCKS - 1)
      / WAVEOUT_BLOCKS;
   if (frames < WAVEOUT_BLOCK_FRAMES)
      frames = WAVEOUT_BLOCK_FRAMES;
   wo->block_bytes = frames * WAVEOUT_FRAME_BYTES;

   wo->hdr   = (WAVEHDR*)calloc(WAVEOUT_BLOCKS, sizeof(WAVEHDR));
   wo->data  = (char*)calloc(WAVEOUT_BLOCKS, wo->block_bytes);
   wo->event = CreateEvent(NULL, FALSE, FALSE, NULL);
   if (!wo->hdr || !wo->data || !wo->event)
      goto error;

   memset(&fmt, 0, sizeof(fmt));
   fmt.wFormatTag      = WAVE_FORMAT_PCM;
   fmt.nChannels       = 2;
   fmt.nSamplesPerSec  = rate;
   fmt.wBitsPerSample  = 16;
   fmt.nBlockAlign     = WAVEOUT_FRAME_BYTES;
   fmt.nAvgBytesPerSec = rate * WAVEOUT_FRAME_BYTES;

   if (waveOutOpen(&wo->hwo, dev, &fmt, (DWORD_PTR)wo->event, 0,
            CALLBACK_EVENT) != MMSYSERR_NOERROR)
   {
      wo->hwo = NULL;
      RARCH_ERR("[waveout] The device does not open at %u Hz, 16-bit stereo.\n", rate);
      goto error;
   }

   for (i = 0; i < WAVEOUT_BLOCKS; i++)
   {
      wo->hdr[i].lpData         = wo->data + i * wo->block_bytes;
      wo->hdr[i].dwBufferLength = (DWORD)wo->block_bytes;
      if (waveOutPrepareHeader(wo->hwo, &wo->hdr[i], sizeof(WAVEHDR))
            != MMSYSERR_NOERROR)
         goto error;
   }

   *new_rate = rate;
   RARCH_LOG("[waveout] %u Hz, %u blocks of %u frames.\n",
         rate, (unsigned)WAVEOUT_BLOCKS, (unsigned)frames);
   return wo;

error:
   waveout_free(wo);
   return NULL;
}

static size_t waveout_write_avail(void *data)
{
   waveout_t *wo = (waveout_t*)data;

   waveout_reclaim(wo);
   if (wo->queued >= WAVEOUT_BLOCKS)
      return 0;
   return (WAVEOUT_BLOCKS - wo->queued) * wo->block_bytes - wo->fill;
}

static size_t waveout_buffer_size(void *data)
{
   waveout_t *wo = (waveout_t*)data;
   return WAVEOUT_BLOCKS * wo->block_bytes;
}

static ssize_t waveout_write(void *data, const void *s, size_t len)
{
   waveout_t *wo     = (waveout_t*)data;
   const char *buf   = (const char*)s;
   size_t written    = 0;
   unsigned waits    = 0;

   while (len)
   {
      size_t n;

      waveout_reclaim(wo);

      if (wo->queued >= WAVEOUT_BLOCKS)
      {
         /* every block is with the device */
         if (wo->nonblock || wo->paused)
            break;
         /* a device that hands nothing back in half a second has
          * stopped: what was taken is what was written */
         if (++waits > 5)
            break;
         WaitForSingleObject(wo->event, 100);
         continue;
      }

      n = wo->block_bytes - wo->fill;
      if (n > len)
         n = len;
      memcpy(wo->hdr[wo->cur].lpData + wo->fill, buf, n);
      wo->fill += n;
      buf      += n;
      len      -= n;
      written  += n;

      if (wo->fill == wo->block_bytes)
      {
         WAVEHDR *hdr        = &wo->hdr[wo->cur];
         hdr->dwBufferLength = (DWORD)wo->block_bytes;
         hdr->dwFlags       &= ~WHDR_DONE;
         if (waveOutWrite(wo->hwo, hdr, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
            return -1;
         wo->cur   = (wo->cur + 1) % WAVEOUT_BLOCKS;
         wo->queued++;
         wo->fill  = 0;
         waits     = 0;
      }
   }

   return (ssize_t)written;
}

/* Until there is room for @len, or a little while: then what there is. */
static size_t waveout_wait_writable(void *data, size_t len)
{
   waveout_t *wo  = (waveout_t*)data;
   size_t most    = WAVEOUT_BLOCKS * wo->block_bytes;
   unsigned waits = 0;
   size_t avail;

   if (len > most)
      len = most;
   for (;;)
   {
      avail = waveout_write_avail(wo);
      if (avail >= len || wo->paused || ++waits > 4)
         break;
      WaitForSingleObject(wo->event, 25);
   }
   return avail;
}

static size_t waveout_frames_consumed(void *data)
{
   waveout_t *wo = (waveout_t*)data;
   waveout_reclaim(wo);
   return wo->frames_done;
}

static bool waveout_stop(void *data)
{
   waveout_t *wo = (waveout_t*)data;
   if (!wo->paused)
   {
      waveOutPause(wo->hwo);
      wo->paused = true;
   }
   return true;
}

static bool waveout_start(void *data, bool is_shutdown)
{
   waveout_t *wo = (waveout_t*)data;
   (void)is_shutdown;
   if (wo->paused)
   {
      waveOutRestart(wo->hwo);
      wo->paused = false;
   }
   return true;
}

static bool waveout_alive(void *data)
{
   waveout_t *wo = (waveout_t*)data;
   return !wo->paused;
}

static void waveout_set_nonblock_state(void *data, bool state)
{
   waveout_t *wo = (waveout_t*)data;
   wo->nonblock  = state;
}

static bool waveout_use_float(void *data)
{
   (void)data;
   return false;
}

audio_driver_t audio_waveout = {
   waveout_init,
   waveout_write,
   waveout_stop,
   waveout_start,
   waveout_alive,
   waveout_set_nonblock_state,
   waveout_free,
   waveout_use_float,
   "waveout",
   NULL, /* device_list_new */
   NULL, /* device_list_free */
   waveout_write_avail,
   waveout_buffer_size,
   NULL, /* write_raw */
   waveout_wait_writable,
   waveout_frames_consumed
};
