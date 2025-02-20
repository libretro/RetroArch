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

/* Max time to wait before continuing */
#define WASAPI_TIMEOUT 256

enum wasapi_flags
{
   WASAPI_FLG_EXCLUSIVE = (1 << 0),
   WASAPI_FLG_NONBLOCK  = (1 << 1),
   WASAPI_FLG_RUNNING   = (1 << 2)
};

typedef struct
{
   HANDLE write_event;
   IMMDevice          *device;
   IAudioClient       *client;
   IAudioRenderClient *renderer;
   fifo_buffer_t      *buffer;
   size_t engine_buffer_size;
   unsigned char frame_size;          /* 4 or 8 only */
   uint8_t flags;
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
   bool audio_sync           = settings->bools.audio_sync;
   unsigned sh_buffer_length = settings->uints.audio_wasapi_sh_buffer_length;
   wasapi_t *w               = (wasapi_t*)calloc(1, sizeof(wasapi_t));

   if (!w)
      return NULL;

   w->device                 = wasapi_init_device(dev_id, eRender);
   if (!w->device && dev_id)
      w->device = wasapi_init_device(NULL, eRender);
   if (!w->device)
      goto error;

   if (!(w->client = wasapi_init_client(w->device,
         &exclusive_mode, &float_format, &rate, latency, 2)))
      goto error;
   if (exclusive_mode)
      w->flags              |= WASAPI_FLG_EXCLUSIVE;

   hr = _IAudioClient_GetBufferSize(w->client, &frame_count);
   if (FAILED(hr))
      goto error;

   w->frame_size             = float_format ? 8 : 4;
   w->engine_buffer_size     = frame_count * w->frame_size;

   if ((w->flags & WASAPI_FLG_EXCLUSIVE) > 0)
   {
      if (!(w->buffer = fifo_new(w->engine_buffer_size)))
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

      if (!(w->buffer = fifo_new(sh_buffer_length * w->frame_size)))
         goto error;
   }

   if (!(w->write_event = CreateEventA(NULL, FALSE, FALSE, NULL)))
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

   w->flags    |=   WASAPI_FLG_RUNNING;
   if (audio_sync)
      w->flags &= ~(WASAPI_FLG_NONBLOCK);
   else
      w->flags |=  (WASAPI_FLG_NONBLOCK);

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

static ssize_t wasapi_write(void *wh, const void *data, size_t len)
{
   size_t written = 0;
   wasapi_t *w    = (wasapi_t*)wh;
   uint8_t flg    = w->flags;

   if (!((flg & WASAPI_FLG_RUNNING) > 0))
      return -1;

   if ((flg & WASAPI_FLG_EXCLUSIVE) > 0)
   {
      if ((flg & WASAPI_FLG_NONBLOCK) > 0)
      {
         size_t write_avail = FIFO_WRITE_AVAIL(w->buffer);
         if (!write_avail)
         {
            UINT32 frame_count;
            BYTE *dest         = NULL;
            if (WaitForSingleObject(w->write_event, 0) != WAIT_OBJECT_0)
               return 0;
            frame_count        = w->engine_buffer_size / w->frame_size;
            if (FAILED(_IAudioRenderClient_GetBuffer(
                        w->renderer, frame_count, &dest)))
               return -1;
            fifo_read(w->buffer, dest, w->engine_buffer_size);
            if (FAILED(_IAudioRenderClient_ReleaseBuffer(
                        w->renderer, frame_count, 0)))
               return -1;
            write_avail = w->engine_buffer_size;
         }
         written = (len < write_avail) ? len : write_avail;
         fifo_write(w->buffer, data, written);
      }
      else
      {
         ssize_t ir;
         for (ir = -1; written < len; written += ir)
         {
            const void *_data  = (char*)data + written;
            size_t __len       = len - written;
            size_t write_avail = FIFO_WRITE_AVAIL(w->buffer);
            if (!write_avail)
            {
               BYTE *dest         = NULL;
               if (WaitForSingleObject(w->write_event, WASAPI_TIMEOUT) != WAIT_OBJECT_0)
                  ir = 1;
               else
               {
                  UINT32 frame_count = w->engine_buffer_size / w->frame_size;
                  if (FAILED(_IAudioRenderClient_GetBuffer(
                              w->renderer, frame_count, &dest)))
                     return -1;
                  fifo_read(w->buffer, dest, w->engine_buffer_size);
                  if (FAILED(_IAudioRenderClient_ReleaseBuffer(
                              w->renderer, frame_count, 0)))
                     return -1;
                  write_avail = w->engine_buffer_size;
               }
            }
            ir = (__len < write_avail) ? __len : write_avail;
            fifo_write(w->buffer, _data, ir);
         }
      }
   }
   else
   {
      if ((flg & WASAPI_FLG_NONBLOCK) > 0)
      {
         size_t write_avail = 0;
         UINT32 padding     = 0;
         if (w->buffer)
         {
            if (!(write_avail = FIFO_WRITE_AVAIL(w->buffer)))
            {
               size_t read_avail = 0;
               if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
                  return -1;
               read_avail  = FIFO_READ_AVAIL(w->buffer);
               write_avail = w->engine_buffer_size - padding * w->frame_size;
               written     = read_avail < write_avail ? read_avail : write_avail;
               if (written)
               {
                  BYTE *dest         = NULL;
                  UINT32 frame_count = written / w->frame_size;
                  if (FAILED(_IAudioRenderClient_GetBuffer(
                              w->renderer, frame_count, &dest)))
                     return -1;
                  fifo_read(w->buffer, dest, written);
                  if (FAILED(_IAudioRenderClient_ReleaseBuffer(
                              w->renderer, frame_count, 0)))
                     return -1;
               }
            }
            write_avail = FIFO_WRITE_AVAIL(w->buffer);
            written     = len < write_avail ? len : write_avail;
            if (written)
               fifo_write(w->buffer, data, written);
         }
         else
         {
            if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
               return -1;
            if (!(write_avail = w->engine_buffer_size - padding * w->frame_size))
               return 0;
            written = (len < write_avail) ? len : write_avail;
            if (written)
            {
               BYTE *dest         = NULL;
               UINT32 frame_count = written / w->frame_size;
               if (FAILED(_IAudioRenderClient_GetBuffer(
                           w->renderer, frame_count, &dest)))
                  return -1;
               memcpy(dest, data, written);
               if (FAILED(_IAudioRenderClient_ReleaseBuffer(
                           w->renderer, frame_count, 0)))
                  return -1;
            }
         }
      }
      else if (w->buffer)
      {
         ssize_t ir;
         for (ir = -1; written < len; written += ir)
         {
            const void *_data  = (char*)data + written;
            size_t _len        = len - written;
            size_t write_avail = FIFO_WRITE_AVAIL(w->buffer);
            UINT32 padding     = 0;
            if (!write_avail)
            {
               size_t read_avail = 0;
               if (!(WaitForSingleObject(w->write_event, WASAPI_TIMEOUT) == WAIT_OBJECT_0))
                  return -1;
               if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
                  return -1;
               read_avail  = FIFO_READ_AVAIL(w->buffer);
               write_avail = w->engine_buffer_size - padding * w->frame_size;
               ir          = read_avail < write_avail ? read_avail : write_avail;
               if (ir)
               {
                  BYTE *dest         = NULL;
                  UINT32 frame_count = ir / w->frame_size;
                  if (FAILED(_IAudioRenderClient_GetBuffer(
                              w->renderer, frame_count, &dest)))
                     return -1;
                  fifo_read(w->buffer, dest, ir);
                  if (FAILED(_IAudioRenderClient_ReleaseBuffer(
                              w->renderer, frame_count, 0)))
                     return -1;
               }
            }
            write_avail = FIFO_WRITE_AVAIL(w->buffer);
            ir          = (_len < write_avail) ? _len : write_avail;
            if (ir)
               fifo_write(w->buffer, _data, ir);
         }
      }
      else
      {
         ssize_t ir;
         for (ir = -1; written < len; written += ir)
         {
            const void *_data  = (char*)data + written;
            size_t _len        = len - written;
            size_t write_avail = 0;
            UINT32 padding     = 0;
            if (!(WaitForSingleObject(w->write_event, WASAPI_TIMEOUT) == WAIT_OBJECT_0))
               return -1;
            if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
               return -1;
            if (!(write_avail = w->engine_buffer_size - padding * w->frame_size))
               ir = 0;
            else
            {
               ir = (_len < write_avail) ? _len : write_avail;
               if (ir)
               {
                  BYTE *dest         = NULL;
                  UINT32 frame_count = ir / w->frame_size;
                  if (FAILED(_IAudioRenderClient_GetBuffer(
                              w->renderer, frame_count, &dest)))
                     return -1;
                  memcpy(dest, _data, ir);
                  if (FAILED(_IAudioRenderClient_ReleaseBuffer(
                              w->renderer, frame_count, 0)))
                     return -1;
               }
            }
         }
      }
   }

   return written;
}

static bool wasapi_stop(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;

   if (FAILED(_IAudioClient_Stop(w->client)))
      return (!(w->flags & WASAPI_FLG_RUNNING));

   w->flags  &= ~(WASAPI_FLG_RUNNING);

   return true;
}

static bool wasapi_start(void *wh, bool u)
{
   wasapi_t *w = (wasapi_t*)wh;
   HRESULT  hr = _IAudioClient_Start(w->client);
   if (hr != AUDCLNT_E_NOT_STOPPED)
   {
      if (FAILED(hr))
         return ((w->flags & WASAPI_FLG_RUNNING) > 0);
      w->flags  |= (WASAPI_FLG_RUNNING);
   }
   return true;
}

static bool wasapi_alive(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;
   return ((w->flags & WASAPI_FLG_RUNNING) > 0);
}

static void wasapi_set_nonblock_state(void *wh, bool nonblock)
{
   wasapi_t *w = (wasapi_t*)wh;

   if (nonblock)
      w->flags |=  WASAPI_FLG_NONBLOCK;
   else
      w->flags &= ~WASAPI_FLG_NONBLOCK;
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
   return (w->frame_size == 8);
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
   if (w->buffer) /* Exaggerate available size for best results.. */
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
