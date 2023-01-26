/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2023 Jesse Talavera-Greenberg
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

#include <stdio.h>
#include "audio/common/wasapi.h"
#include "microphone/microphone_driver.h"
#include "queues/fifo_queue.h"
#include "configuration.h"
#include "verbosity.h"
#include "audio/common/mmdevice_common.h"

typedef struct
{
   HANDLE write_event;
   IMMDevice           *device;
   IAudioClient        *client;
   IAudioCaptureClient *capture;
   fifo_buffer_t       *buffer; /* NULL in unbuffered shared mode */
   size_t frame_size;          /* 2 or 4 only */
   size_t engine_buffer_size;
   bool exclusive;
} wasapi_microphone_handle_t;

typedef struct wasapi_microphone
{
   wasapi_microphone_handle_t *microphone;
   bool nonblock;
   bool running;
} wasapi_microphone_t;


static void wasapi_microphone_close_mic(void *driver_context, void *microphone_context);

static void *wasapi_microphone_init(void)
{
   settings_t *settings        = config_get_ptr();
   wasapi_microphone_t *wasapi = (wasapi_microphone_t*)calloc(1, sizeof(wasapi_microphone_t));

   if (!wasapi)
   {
      RARCH_ERR("[WASAPI mic]: Failed to allocate microphone driver context\n");
      return NULL;
   }

   wasapi->running = true;
   wasapi->nonblock = !settings->bools.audio_sync;
   RARCH_DBG("[WASAPI mic]: Initialized microphone driver context\n");

   return wasapi;
}


static void wasapi_microphone_free(void *driver_context)
{
   wasapi_microphone_t *wasapi = (wasapi_microphone_t*)driver_context;

   if (!wasapi)
      return;

   if (wasapi->microphone)
   {
      wasapi_microphone_close_mic(wasapi, wasapi->microphone);
   }

   free(wasapi);
}

static void *wasapi_microphone_open_mic(void *driver_context, const char *dev_id, unsigned rate, unsigned latency,
                                        unsigned u1, unsigned *u2)
{
   char error_message[256]       = {0};
   settings_t *settings          = config_get_ptr();
   HRESULT hr;
   UINT32 frame_count            = 0;
   REFERENCE_TIME dev_period     = 0;
   BYTE *dest                    = NULL;
   bool float_format             = settings->bools.audio_wasapi_float_format;
   bool exclusive_mode           = settings->bools.audio_wasapi_exclusive_mode;
   int sh_buffer_length          = settings->ints.audio_wasapi_sh_buffer_length;
   wasapi_microphone_t *wasapi   = (wasapi_microphone_t*)driver_context;
   wasapi_microphone_handle_t *microphone = calloc(1, sizeof(wasapi_microphone_handle_t));

   if (!microphone)
      return NULL;

   microphone->exclusive              = exclusive_mode;
   microphone->device                 = wasapi_init_device(dev_id, eCapture);
   if (!microphone->device && dev_id)
      microphone->device = wasapi_init_device(NULL, eCapture);
   if (!microphone->device)
      goto error;

   microphone->client = wasapi_init_client(microphone->device,
                                           &microphone->exclusive, &float_format, &rate, latency);
   if (!microphone->client)
      goto error;

   hr = _IAudioClient_GetBufferSize(microphone->client, &frame_count);
   if (FAILED(hr))
      goto error;

   microphone->frame_size         = float_format ? 8 : 4;
   microphone->engine_buffer_size = frame_count * microphone->frame_size;

   if (microphone->exclusive)
   {
      microphone->buffer = fifo_new(microphone->engine_buffer_size);
      if (!microphone->buffer)
         goto error;

      RARCH_LOG("[WASAPI]: Intermediate buffer length is %u frames (%.1fms).\n",
                frame_count, (double)frame_count * 1000.0 / rate);
   }
   else if (sh_buffer_length)
   {
      if (sh_buffer_length < 0)
      {
         hr = _IAudioClient_GetDevicePeriod(microphone->client, &dev_period, NULL);
         if (FAILED(hr))
            goto error;

         sh_buffer_length = dev_period * rate / 10000000;
      }

      microphone->buffer = fifo_new(sh_buffer_length * microphone->frame_size);
      if (!microphone->buffer)
         goto error;

      RARCH_LOG("[WASAPI]: Intermediate buffer length is %u frames (%.1fms).\n",
                sh_buffer_length, (double)sh_buffer_length * 1000.0 / rate);
   }
   else
   {
      RARCH_LOG("[WASAPI]: Intermediate buffer is off. \n");
   }

   microphone->write_event = CreateEventA(NULL, FALSE, FALSE, NULL);
   if (!microphone->write_event)
      goto error;

   hr = _IAudioClient_SetEventHandle(microphone->client, microphone->write_event);
   if (FAILED(hr))
      goto error;

   hr = _IAudioClient_GetService(microphone->client,
                                 IID_IAudioRenderClient, (void**)&microphone->capture);
   if (FAILED(hr))
      goto error;

   hr = _IAudioCaptureClient_GetBuffer(microphone->capture, frame_count, &dest);
   if (FAILED(hr))
      goto error;

   hr = _IAudioCaptureClient_ReleaseBuffer(
         microphone->capture, frame_count,
         AUDCLNT_BUFFERFLAGS_SILENT);
   if (FAILED(hr))
      goto error;

   hr = _IAudioClient_Start(microphone->client);
   if (FAILED(hr))
      goto error;


   return microphone;

error:
   wasapi_log_hr(hr, error_message, sizeof(error_message));
   RARCH_ERR("[WASAPI]: Failed to open microphone: %s\n", error_message);
   IFACE_RELEASE(microphone->capture);
   IFACE_RELEASE(microphone->client);
   IFACE_RELEASE(microphone->device);
   if (microphone->write_event)
      CloseHandle(microphone->write_event);
   if (microphone->buffer)
      fifo_free(microphone->buffer);
   free(microphone);

   return NULL;
}

static void wasapi_microphone_close_mic(void *driver_context, void *microphone_context)
{
   DWORD ir;
   wasapi_microphone_t *wasapi            = (wasapi_microphone_t*)driver_context;
   wasapi_microphone_handle_t *microphone = (wasapi_microphone_handle_t*)microphone_context;
   HANDLE write_event;

   if (!wasapi || !microphone)
      return;

   write_event = microphone->write_event;

   IFACE_RELEASE(microphone->capture);
   if (microphone->client)
      _IAudioClient_Stop(microphone->client);
   IFACE_RELEASE(microphone->client);
   IFACE_RELEASE(microphone->device);
   if (microphone->buffer)
      fifo_free(microphone->buffer);
   free(microphone);

   ir = WaitForSingleObject(write_event, 20);
   if (ir == WAIT_FAILED)
   {
      char error[256];
      wasapi_log_hr(HRESULT_FROM_WIN32(GetLastError()), error, sizeof(error));
      RARCH_ERR("[WASAPI mic]: WaitForSingleObject failed with error %d.\n", GetLastError());
   }

   /* If event isn't signaled log and leak */
   if (!(ir == WAIT_OBJECT_0))
      return;

   CloseHandle(write_event);
}

static struct string_list *wasapi_microphone_device_list_new(const void *driver_context)
{
   return mmdevice_list_new(driver_context, eCapture);
}

static void wasapi_microphone_device_list_free(const void *driver_context, struct string_list *devices)
{
   struct string_list *sl = (struct string_list*)devices;

   if (sl)
      string_list_free(sl);
}

microphone_driver_t microphone_wasapi = {
      wasapi_microphone_init,
      wasapi_microphone_free,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      NULL,
      "wasapi",
      wasapi_microphone_device_list_new,
      wasapi_microphone_device_list_free,
      wasapi_microphone_open_mic,
      wasapi_microphone_close_mic
};