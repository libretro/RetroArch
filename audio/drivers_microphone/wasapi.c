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
#include "audio/microphone_driver.h"
#include "queues/fifo_queue.h"
#include "configuration.h"
#include "verbosity.h"
#include "audio/common/mmdevice_common.h"

typedef struct
{
   HANDLE              read_event;
   IMMDevice           *device;
   char                *device_name;
   IAudioClient        *client;
   IAudioCaptureClient *capture;

   /**
    * The buffer in which samples from the microphone will be read and stored
    * until the frontend fetches them.
    */
   fifo_buffer_t       *buffer;

   /**
    * The size of an audio frame, in bytes.
    * Mic input is in one channel with either 16-bit ints or 32-bit floats,
    * so this will be 2 or 4.
    */
   size_t frame_size;
   size_t engine_buffer_size;
   bool exclusive;
   bool running;
} wasapi_microphone_handle_t;

typedef struct wasapi_microphone
{
   bool nonblock;
} wasapi_microphone_t;

static void wasapi_microphone_close_mic(void *driver_context, void *mic_context)
{
   DWORD ir;
   HANDLE write_event;
   wasapi_microphone_t     *wasapi = (wasapi_microphone_t*)driver_context;
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t*)mic_context;

   if (!wasapi || !mic)
      return;

   write_event = mic->read_event;

   IFACE_RELEASE(mic->capture);
   if (mic->client)
      _IAudioClient_Stop(mic->client);
   IFACE_RELEASE(mic->client);
   IFACE_RELEASE(mic->device);
   if (mic->buffer)
      fifo_free(mic->buffer);
   if (mic->device_name)
      free(mic->device_name);
   free(mic);

   ir = WaitForSingleObject(write_event, 20);
   if (ir == WAIT_FAILED)
   {
      RARCH_ERR("[WASAPI mic]: WaitForSingleObject failed: %s\n", wasapi_error(GetLastError()));
   }

   /* If event isn't signaled log and leak */
   if (ir != WAIT_OBJECT_0)
      return;

   CloseHandle(write_event);
}


static void *wasapi_microphone_init(void)
{
   wasapi_microphone_t *wasapi = (wasapi_microphone_t*)calloc(1, sizeof(wasapi_microphone_t));
   if (!wasapi)
   {
      RARCH_ERR("[WASAPI mic]: Failed to allocate microphone driver context\n");
      return NULL;
   }
   wasapi->nonblock = !config_get_ptr()->bools.audio_sync;
   RARCH_DBG("[WASAPI mic]: Initialized microphone driver context.\n");
   return wasapi;
}

static void wasapi_microphone_free(void *driver_context)
{
   wasapi_microphone_t *wasapi = (wasapi_microphone_t*)driver_context;
   if (wasapi)
      free(wasapi);
}

/**
 * Flushes microphone's most recent input to the provided context's FIFO queue.
 * WASAPI requires that fetched input be consumed in its entirety,
 * so the returned value may be less than the queue's size
 * if the next packet won't fit in it.
 * @param microphone Pointer to the microphone context.
 * @return The number of bytes in the queue after fetching input,
 * or -1 if there was an error.
 */
static int wasapi_microphone_fetch_fifo(wasapi_microphone_handle_t *mic)
{
   UINT32 next_packet_size = 0;
   /* Shared-mode capture streams split their input buffer into multiple packets,
    * while exclusive-mode capture streams just use the one.
    *
    * The following loop will run at least once;
    * for exclusive-mode streams, that's all that we'll need.
    */

   do
   {
      BYTE *mic_input           = NULL;
      UINT32 frames_read        = 0;
      UINT32 bytes_read         = 0;
      DWORD buffer_status_flags = 0;
      HRESULT hr = _IAudioCaptureClient_GetBuffer(mic->capture,
            &mic_input, &frames_read, &buffer_status_flags, NULL, NULL);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI]: Failed to get capture device \"%s\"'s buffer: %s\n",
            mic->device_name, hresult_name(hr));
         return -1;
      }
      bytes_read = frames_read * mic->frame_size;

      /* If the queue has room for the packets we just got... */
      if (FIFO_WRITE_AVAIL(mic->buffer) >= bytes_read && bytes_read > 0)
      {
         fifo_write(mic->buffer, mic_input, bytes_read);
         /* ...then enqueue the bytes directly from the mic's buffer */
      }
      else /* Not enough space for new frames, so we can't consume this packet right now */
         frames_read = 0;
      /* If there's insufficient room in the queue, then we can't read the packet.
       * In that case, we leave the packet for next time. */

      hr = _IAudioCaptureClient_ReleaseBuffer(mic->capture, frames_read);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI]: Failed to release capture device \"%s\"'s buffer after consuming %u frames: %s\n",
            mic->device_name, frames_read, hresult_name(hr));
         return -1;
      }

      /* If this is a shared-mode stream and we didn't run out of room in the sample queue... */
      if (!mic->exclusive && frames_read > 0)
      {
         hr = _IAudioCaptureClient_GetNextPacketSize(mic->capture, &next_packet_size);
         /* Get the number of frames that the mic has for us. */
         if (FAILED(hr))
         {
            RARCH_ERR("[WASAPI]: Failed to get capture device \"%s\"'s next packet size: %s\n",
                      mic->device_name, hresult_name(hr));
            return -1;
         }
      }
      /* Exclusive-mode streams only deliver one packet at a time, though it's bigger. */
      else
         next_packet_size = 0;
   }
   while (next_packet_size != 0);

   return FIFO_READ_AVAIL(mic->buffer);
}

/**
 * Blocks until the provided microphone's capture event is signalled.
 *
 * @param microphone The microphone to wait on.
 * @param timeout The amount of time to wait, in milliseconds.
 * @return \c true if the event was signalled,
 * \c false if it timed out or there was an error.
 */
static bool wasapi_microphone_wait_for_capture_event(wasapi_microphone_handle_t *mic, DWORD timeout)
{
   /*...then let's wait for the mic to tell us that samples are ready. */
   switch (WaitForSingleObject(mic->read_event, timeout))
   {
      case WAIT_OBJECT_0:
         /* Okay, there's data available. */
         return true;
      case WAIT_TIMEOUT:
         /* Time out; there's nothing here for us. */
         RARCH_ERR("[WASAPI]: Failed to wait for capture device \"%s\" event: Timeout after %ums\n", mic->device_name, timeout);
         break;
      default:
         RARCH_ERR("[WASAPI]: Failed to wait for capture device \"%s\" event: %s\n", mic->device_name, wasapi_error(GetLastError()));
         break;
   }
   return false;
}

/**
 * Reads samples from a microphone,
 * fetching more from it if necessary.
 * Works for exclusive and shared-mode streams.
 *
 * @param microphone Pointer to the context of the microphone
 * from which samples will be read.
 * @param buffer The buffer in which the fetched samples will be stored.
 * @param buffer_size The size of buffer, in bytes.
 * @param timeout Timeout for new samples, in milliseconds.
 * 0 means that this function won't wait for new samples,
 * \c INFINITE means that this function will wait indefinitely.
 * @return The number of samples that were retrieved,
 * or -1 if there was an error (including timeout).
 */
static int wasapi_microphone_read_buffered(
   wasapi_microphone_handle_t *mic, void *s, size_t len,
   DWORD timeout)
{
   int bytes_read      = 0; /* Number of bytes sent to the core */
   int bytes_available = FIFO_READ_AVAIL(mic->buffer);

   /* If we don't have any queued samples to give to the core... */
   if (!bytes_available)
   {
      /* If we couldn't wait for the microphone to signal a capture event... */
      if (!wasapi_microphone_wait_for_capture_event(mic, timeout))
         return -1;

      bytes_available = wasapi_microphone_fetch_fifo(mic);
      /* If we couldn't fetch samples from the microphone... */
      if (bytes_available < 0)
         return -1;
   }

   /* Now that we have samples available, let's give them to the core */

   bytes_read = MIN((int)len, bytes_available);
   fifo_read(mic->buffer, s, bytes_read);
   /* Read data from the sample queue and store it in the provided buffer */
   return bytes_read;
}

static int wasapi_microphone_read(void *driver_context, void *mic_context, void *s, size_t len)
{
   int bytes_read = 0;
   wasapi_microphone_t     *wasapi = (wasapi_microphone_t *)driver_context;
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t*)mic_context;

   if (!wasapi || !mic || !s)
      return -1;

   /* If microphones shouldn't block... */
   if (wasapi->nonblock)
      return wasapi_microphone_read_buffered(mic, s, len, 0);

   if (mic->exclusive)
   {
      int read;
      for (read = -1; (size_t)bytes_read < len; bytes_read += read)
      {
         read = wasapi_microphone_read_buffered(mic,
               (char *)s   + bytes_read,
               len         - bytes_read,
               INFINITE);
         if (read == -1)
            return -1;
      }
   }
   else
   {
      int read;
      for (read = -1; (size_t)bytes_read < len; bytes_read += read)
      {
         read = wasapi_microphone_read_buffered(mic,
               (char *)s   + bytes_read,
               len         - bytes_read,
               INFINITE);
         if (read == -1)
            return -1;
      }
   }

   return bytes_read;
}

static void wasapi_microphone_set_nonblock_state(void *driver_context, bool nonblock)
{
   wasapi_microphone_t *wasapi = (wasapi_microphone_t*)driver_context;
   wasapi->nonblock = nonblock;
}

static void *wasapi_microphone_open_mic(void *driver_context, const char *device, unsigned rate,
      unsigned latency, unsigned *new_rate)
{
   HRESULT hr;
   settings_t *settings            = config_get_ptr();
   DWORD flags                     = 0;
   UINT32 frame_count              = 0;
   REFERENCE_TIME dev_period       = 0;
   BYTE *dest                      = NULL;
   bool float_format               = settings->bools.microphone_wasapi_float_format;
   bool exclusive_mode             = settings->bools.microphone_wasapi_exclusive_mode;
   unsigned sh_buffer_length       = settings->uints.microphone_wasapi_sh_buffer_length;
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t*)calloc(
         1, sizeof(wasapi_microphone_handle_t));

   if (!mic)
      return NULL;

   mic->exclusive         = exclusive_mode;
   mic->device            = wasapi_init_device(device, eCapture);

   /* If we requested a particular capture device, but couldn't open it... */
   if (device && !mic->device)
   {
      RARCH_WARN("[WASAPI]: Failed to open requested capture device \"%s\", attempting to open default device\n", device);
      mic->device = wasapi_init_device(NULL, eCapture);
   }

   if (!mic->device)
   {
      RARCH_ERR("[WASAPI]: Failed to open capture device\n");
      goto error;
   }

   if (!(mic->device_name = mmdevice_name(mic->device)))
   {
      RARCH_ERR("[WASAPI]: Failed to get friendly name of capture device\n");
      goto error;
   }

   mic->client = wasapi_init_client(mic->device,
      &mic->exclusive, &float_format, &rate, latency, 1);
   if (!mic->client)
   {
      RARCH_ERR("[WASAPI]: Failed to open client for capture device \"%s\"\n", mic->device_name);
      goto error;
   }

   hr = _IAudioClient_GetBufferSize(mic->client, &frame_count);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: Failed to get buffer size of IAudioClient for capture device \"%s\": %s\n",
          mic->device_name, hresult_name(hr));
      goto error;
   }

   mic->frame_size         = float_format ? sizeof(float) : sizeof(int16_t);
   mic->engine_buffer_size = frame_count * mic->frame_size;

   /* If this mic should be used *exclusively* by RetroArch... */
   if (mic->exclusive)
   {
      mic->buffer = fifo_new(mic->engine_buffer_size);
      if (!mic->buffer)
      {
         RARCH_ERR("[WASAPI]: Failed to initialize FIFO queue for capture device.\n");
         goto error;
      }

      RARCH_LOG("[WASAPI]: Intermediate exclusive-mode capture buffer length is %u frames (%.1fms, %u bytes).\n",
                frame_count, (double)frame_count * 1000.0 / rate, mic->engine_buffer_size);
   }
   else
   {
      /* If the user selected the "default" shared buffer length... */
      if (sh_buffer_length <= 0)
      {
         hr = _IAudioClient_GetDevicePeriod(mic->client, &dev_period, NULL);
         if (FAILED(hr))
            goto error;

         sh_buffer_length = (dev_period * rate / 10000000) * 2;
         /* Default buffer seems to be too small, resulting in slowdown.
          * Doubling it seems to work okay. Dunno why. */
      }

      mic->buffer = fifo_new(sh_buffer_length * mic->frame_size);
      if (!mic->buffer)
         goto error;

      RARCH_LOG("[WASAPI]: Intermediate shared-mode capture buffer length is %u frames (%.1fms, %u bytes).\n",
                sh_buffer_length, (double)sh_buffer_length * 1000.0 / rate, sh_buffer_length * mic->frame_size);
   }

   if (!(mic->read_event = CreateEventA(NULL, FALSE, FALSE, NULL)))
   {
      RARCH_ERR("[WASAPI]: Failed to allocate capture device's event handle\n");
      goto error;
   }

   hr = _IAudioClient_SetEventHandle(mic->client, mic->read_event);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: Failed to set capture device's event handle: %s\n", hresult_name(hr));
      goto error;
   }

   hr = _IAudioClient_GetService(mic->client,
         IID_IAudioCaptureClient, (void**)&mic->capture);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: Failed to get capture device's IAudioCaptureClient service: %s\n", hresult_name(hr));
      goto error;
   }

   /* Get and release the buffer, just to ensure that we can. */
   hr = _IAudioCaptureClient_GetBuffer(mic->capture, &dest, &frame_count, &flags, NULL, NULL);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: Failed to get capture client buffer: %s\n", hresult_name(hr));
      goto error;
   }

   hr = _IAudioCaptureClient_ReleaseBuffer(mic->capture, 0);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: Failed to release capture client buffer: %s\n", hresult_name(hr));
      goto error;
   }

   /* The rate was (possibly) modified when we initialized the client */
   if (new_rate)
      *new_rate = rate;
   return mic;

error:
   IFACE_RELEASE(mic->capture);
   IFACE_RELEASE(mic->client);
   IFACE_RELEASE(mic->device);
   if (mic->read_event)
      CloseHandle(mic->read_event);
   if (mic->buffer)
      fifo_free(mic->buffer);
   if (mic->device_name)
      free(mic->device_name);
   free(mic);
   return NULL;
}

static bool wasapi_microphone_start_mic(void *driver_context, void *mic_context)
{
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t*)mic_context;
   HRESULT hr;
   if (!mic)
      return false;
   hr = _IAudioClient_Start(mic->client);

   /* Starting an already-active microphone is not an error */
   if (SUCCEEDED(hr) || hr == AUDCLNT_E_NOT_STOPPED)
      mic->running = true;
   else
   {
      RARCH_ERR("[WASAPI mic]: Failed to start capture device \"%s\"'s IAudioClient: %s\n",
         mic->device_name, hresult_name(hr));
      mic->running = false;
   }
   return mic->running;
}

static bool wasapi_microphone_stop_mic(void *driver_context, void *mic_context)
{
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t*)mic_context;
   HRESULT hr;
   if (!mic)
      return false;
   hr = _IAudioClient_Stop(mic->client);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI mic]: Failed to stop capture device \"%s\"'s IAudioClient: %s\n",
         mic->device_name, hresult_name(hr));
      return false;
   }
   RARCH_LOG("[WASAPI mic]: Stopped capture device \"%s\".\n", mic->device_name);
   mic->running = false;
   return true;
}

static bool wasapi_microphone_mic_alive(const void *driver_context, const void *mic_context)
{
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t *)mic_context;
   return mic && mic->running;
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

static bool wasapi_microphone_use_float(const void *driver_context, const void *mic_context)
{
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t *)mic_context;
   return (mic && (mic->frame_size == sizeof(float)));
}

microphone_driver_t microphone_wasapi = {
      wasapi_microphone_init,
      wasapi_microphone_free,
      wasapi_microphone_read,
      wasapi_microphone_set_nonblock_state,
      "wasapi",
      wasapi_microphone_device_list_new,
      wasapi_microphone_device_list_free,
      wasapi_microphone_open_mic,
      wasapi_microphone_close_mic,
      wasapi_microphone_mic_alive,
      wasapi_microphone_start_mic,
      wasapi_microphone_stop_mic,
      wasapi_microphone_use_float
};
