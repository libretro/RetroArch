/*  RetroArch - A frontend for libretro.
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

#include <lists/string_list.h>
#include <queues/fifo_queue.h>
#include <string/stdstring.h>

#include "../common/mmdevice_common.h"
#include "../common/mmdevice_common_inline.h"
#include "../common/wasapi.h"

#include "../audio_driver.h"
#include "../../verbosity.h"
#include "../../configuration.h"

typedef struct
{
   HANDLE write_event;
   IMMDevice          *device;
   IAudioClient       *client;
   IAudioRenderClient *renderer;
   fifo_buffer_t      *buffer;
   size_t engine_buffer_size;
   unsigned char frame_size;          /* 4 or 8 only */
   bool exclusive;
   bool nonblock;
   bool running;
} wasapi_t;

static void *wasapi_init(const char *dev_id, unsigned rate, unsigned latency,
      unsigned u1, unsigned *new_rate)
{
   HRESULT hr;
   UINT32 frame_count        = 0;
   REFERENCE_TIME dev_period = 0;
   BYTE *dest                = NULL;
   settings_t *settings      = config_get_ptr();
   bool float_format         = settings->bools.audio_wasapi_float_format;
   bool exclusive_mode       = settings->bools.audio_wasapi_exclusive_mode;
   unsigned sh_buffer_length = settings->uints.audio_wasapi_sh_buffer_length;
   wasapi_t *w               = (wasapi_t*)calloc(1, sizeof(wasapi_t));

   if (!w)
      return NULL;

   w->exclusive              = exclusive_mode;
   w->device                 = wasapi_init_device(dev_id, eRender);
   if (!w->device && dev_id)
      w->device = wasapi_init_device(NULL, eRender);
   if (!w->device)
      goto error;

   w->client = wasapi_init_client(w->device,
         &w->exclusive, &float_format, &rate, latency, 2);
   if (!w->client)
      goto error;

   hr = _IAudioClient_GetBufferSize(w->client, &frame_count);
   if (FAILED(hr))
      goto error;

   w->frame_size             = float_format ? 8 : 4;
   w->engine_buffer_size     = frame_count * w->frame_size;

   if (w->exclusive)
   {
      w->buffer = fifo_new(w->engine_buffer_size);
      if (!w->buffer)
         goto error;
   }
   else
   {
      switch (sh_buffer_length)
      {
         case WASAPI_SH_BUFFER_AUDIO_LATENCY:
         case WASAPI_SH_BUFFER_CLIENT_BUFFER:
            sh_buffer_length = frame_count;
            break;
         case WASAPI_SH_BUFFER_DEVICE_PERIOD:
            hr = _IAudioClient_GetDevicePeriod(w->client, &dev_period, NULL);

            if (FAILED(hr))
               goto error;

            sh_buffer_length = dev_period * rate / 10000000;
            break;
         default:
            break;
      }

      w->buffer = fifo_new(sh_buffer_length * w->frame_size);
      if (!w->buffer)
         goto error;
   }

   w->write_event = CreateEventA(NULL, FALSE, FALSE, NULL);
   if (!w->write_event)
      goto error;

   hr = _IAudioClient_SetEventHandle(w->client, w->write_event);
   if (FAILED(hr))
      goto error;

   hr = _IAudioClient_GetService(w->client,
         IID_IAudioRenderClient, (void**)&w->renderer);
   if (FAILED(hr))
      goto error;

   hr = _IAudioRenderClient_GetBuffer(w->renderer, frame_count, &dest);
   if (FAILED(hr))
      goto error;

   hr = _IAudioRenderClient_ReleaseBuffer(
         w->renderer, frame_count,
         AUDCLNT_BUFFERFLAGS_SILENT);
   if (FAILED(hr))
      goto error;

   hr = _IAudioClient_Start(w->client);
   if (FAILED(hr))
      goto error;

   w->running  = true;
   w->nonblock = !settings->bools.audio_sync;

   if (new_rate)
      *new_rate = rate;

   return w;

error:
   IFACE_RELEASE(w->renderer);
   IFACE_RELEASE(w->client);
   IFACE_RELEASE(w->device);
   if (w->write_event)
      CloseHandle(w->write_event);
   if (w->buffer)
      fifo_free(w->buffer);
   free(w);

   return NULL;
}

static bool wasapi_flush(wasapi_t *w, const void *data, size_t size)
{
   BYTE *dest         = NULL;
   UINT32 frame_count = size / w->frame_size;

   if (FAILED(_IAudioRenderClient_GetBuffer(
         w->renderer, frame_count, &dest)))
      return false;

   memcpy(dest, data, size);

   if (FAILED(_IAudioRenderClient_ReleaseBuffer(
         w->renderer, frame_count, 0)))
      return false;

   return true;
}

static bool wasapi_flush_buffer(wasapi_t *w, size_t size)
{
   BYTE *dest         = NULL;
   UINT32 frame_count = size / w->frame_size;

   if (FAILED(_IAudioRenderClient_GetBuffer(
         w->renderer, frame_count, &dest)))
      return false;

   fifo_read(w->buffer, dest, size);

   if (FAILED(_IAudioRenderClient_ReleaseBuffer(
         w->renderer, frame_count, 0)))
      return false;

   return true;
}

static ssize_t wasapi_write_sh_buffer(wasapi_t *w, const void *data, size_t size)
{
   ssize_t written    = -1;
   size_t write_avail = FIFO_WRITE_AVAIL(w->buffer);
   UINT32 padding     = 0;

   if (!write_avail)
   {
      size_t read_avail = 0;
      if (!(WaitForSingleObject(w->write_event, INFINITE) == WAIT_OBJECT_0))
         return -1;

      if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
         return -1;

      read_avail  = FIFO_READ_AVAIL(w->buffer);
      write_avail = w->engine_buffer_size - padding * w->frame_size;
      written     = read_avail < write_avail ? read_avail : write_avail;
      if (written)
         if (!wasapi_flush_buffer(w, written))
            return -1;
   }

   write_avail = FIFO_WRITE_AVAIL(w->buffer);
   written     = size < write_avail ? size : write_avail;
   if (written)
      fifo_write(w->buffer, data, written);

   return written;
}

static ssize_t wasapi_write_sh(wasapi_t *w, const void *data, size_t size)
{
   ssize_t written    = -1;
   size_t write_avail = 0;
   UINT32 padding     = 0;

   if (!(WaitForSingleObject(w->write_event, INFINITE) == WAIT_OBJECT_0))
      return -1;

   if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
      return -1;

   write_avail = w->engine_buffer_size - padding * w->frame_size;
   if (!write_avail)
      return 0;

   written = size < write_avail ? size : write_avail;
   if (written)
      if (!wasapi_flush(w, data, written))
         return -1;

   return written;
}

static ssize_t wasapi_write_sh_nonblock(wasapi_t *w, const void *data, size_t size)
{
   ssize_t written    = -1;
   size_t write_avail = 0;
   UINT32 padding     = 0;

   if (w->buffer)
   {
      write_avail = FIFO_WRITE_AVAIL(w->buffer);
      if (!write_avail)
      {
         size_t read_avail = 0;
         if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
            return -1;

         read_avail  = FIFO_READ_AVAIL(w->buffer);
         write_avail = w->engine_buffer_size - padding * w->frame_size;
         written     = read_avail < write_avail ? read_avail : write_avail;
         if (written)
            if (!wasapi_flush_buffer(w, written))
               return -1;
      }

      write_avail = FIFO_WRITE_AVAIL(w->buffer);
      written     = size < write_avail ? size : write_avail;
      if (written)
         fifo_write(w->buffer, data, written);
   }
   else
   {
      if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
         return -1;

      if (!(write_avail = w->engine_buffer_size - padding * w->frame_size))
         return 0;

      written = size < write_avail ? size : write_avail;
      if (written)
         if (!wasapi_flush(w, data, written))
            return -1;
   }

   return written;
}

static ssize_t wasapi_write_ex(wasapi_t *w, const void *data, size_t size, DWORD ms)
{
   ssize_t written    = 0;
   size_t write_avail = FIFO_WRITE_AVAIL(w->buffer);

   if (!write_avail)
   {
      if (WaitForSingleObject(w->write_event, ms) != WAIT_OBJECT_0)
         return 0;

      if (!wasapi_flush_buffer(w, w->engine_buffer_size))
         return -1;

      write_avail = w->engine_buffer_size;
   }

   written = size < write_avail ? size : write_avail;
   fifo_write(w->buffer, data, written);

   return written;
}

static ssize_t wasapi_write(void *wh, const void *data, size_t size)
{
   size_t written = 0;
   ssize_t ir     = 0;
   wasapi_t *w    = (wasapi_t*)wh;

   if (w->nonblock)
   {
      if (w->exclusive)
         return wasapi_write_ex(w, data, size, 0);
      return wasapi_write_sh_nonblock(w, data, size);
   }

   if (w->exclusive)
   {
      for (ir = -1; written < size; written += ir)
      {
         ir = wasapi_write_ex(w, (char*)data + written, size - written, INFINITE);
         if (ir == -1)
            return -1;
      }
   }
   else
   {
      if (w->buffer)
      {
         for (ir = -1; written < size; written += ir)
         {
            ir = wasapi_write_sh_buffer(w, (char*)data + written, size - written);
            if (ir == -1)
               return -1;
         }
      }
      else
      {
         for (ir = -1; written < size; written += ir)
         {
            ir = wasapi_write_sh(w, (char*)data + written, size - written);
            if (ir == -1)
               return -1;
         }
      }
   }

   return written;
}

static bool wasapi_stop(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;

   if (FAILED(_IAudioClient_Stop(w->client)))
      return !w->running;

   w->running = false;

   return true;
}

static bool wasapi_start(void *wh, bool u)
{
   wasapi_t *w = (wasapi_t*)wh;
   HRESULT  hr = _IAudioClient_Start(w->client);

   if (hr == AUDCLNT_E_NOT_STOPPED)
      return true;

   if (FAILED(hr))
      return w->running;

   w->running = true;

   return true;
}

static bool wasapi_alive(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;

   return w->running;
}

static void wasapi_set_nonblock_state(void *wh, bool nonblock)
{
   wasapi_t *w = (wasapi_t*)wh;

   w->nonblock = nonblock;
}

static void wasapi_free(void *wh)
{
   DWORD ir;
   wasapi_t *w        = (wasapi_t*)wh;
   HANDLE write_event = w->write_event;

   IFACE_RELEASE(w->renderer);
   if (w->client)
      _IAudioClient_Stop(w->client);
   IFACE_RELEASE(w->client);
   IFACE_RELEASE(w->device);
   if (w->buffer)
      fifo_free(w->buffer);
   free(w);

   ir = WaitForSingleObject(write_event, 20);
   if (ir == WAIT_FAILED)
      RARCH_ERR("[WASAPI]: WaitForSingleObject failed with error %d.\n", GetLastError());

   if (!(ir == WAIT_OBJECT_0))
      return;

   CloseHandle(write_event);
}

static bool wasapi_use_float(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;

   return w->frame_size == 8;
}

static void wasapi_device_list_free(void *u, void *slp)
{
   struct string_list *sl = (struct string_list*)slp;

   if (sl)
      string_list_free(sl);
}

static size_t wasapi_write_avail(void *wh)
{
   wasapi_t *w    = (wasapi_t*)wh;
   UINT32 padding = 0;

   if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
      return 0;

   if (w->buffer)
      /* Exaggarate available size for best results.. */
      return FIFO_WRITE_AVAIL(w->buffer) + padding * 2;

   return w->engine_buffer_size - padding * w->frame_size;
}

static size_t wasapi_buffer_size(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;

   if (w->buffer)
      return w->buffer->size;

   return w->engine_buffer_size;
}

static void *wasapi_device_list_new(void *u)
{
   return mmdevice_list_new(u, eRender);
}

audio_driver_t audio_wasapi = {
   wasapi_init,
   wasapi_write,
   wasapi_stop,
   wasapi_start,
   wasapi_alive,
   wasapi_set_nonblock_state,
   wasapi_free,
   wasapi_use_float,
   "wasapi",
   wasapi_device_list_new,
   wasapi_device_list_free,
   wasapi_write_avail,
   wasapi_buffer_size
};
