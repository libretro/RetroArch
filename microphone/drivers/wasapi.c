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
   HANDLE              read_event;
   IMMDevice           *device;
   char                *device_name;
   IAudioClient        *client;
   IAudioCaptureClient *capture;

   /**
    * The buffer in which samples from the microphone will be read and stored
    * until the core fetches them.
    * Will be \c NULL if the mic is opened in shared mode
    * and a shared-buffer length is not given.
    * If \c NULL, then samples are copied directly from the
    * platform's underlying buffer to the driver.
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

/**
 * Flushes microphone's most recent input to the provided buffer.
 * @param microphone Pointer to the microphone context.
 * @param buffer The buffer in which incoming samples will be stored.
 * @param buffer_size The length of \c buffer, in bytes.
 * @return True if the read was successful
 */
static bool wasapi_microphone_fetch_buffer(
      wasapi_microphone_handle_t *microphone,
      void *buffer,
      size_t buffer_size)
{
   BYTE *mic_input = NULL;
   UINT32 frame_count = 0;
   UINT32 byte_count = 0;
   DWORD buffer_status_flags = 0;
   HRESULT hr;

   hr = _IAudioCaptureClient_GetBuffer(microphone->capture, &mic_input, &frame_count, &buffer_status_flags, NULL, NULL);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: Failed to get capture device \"%s\"'s buffer (%s)\n",
         microphone->device_name,
         hresult_name(hr));
      return false;
   }
   byte_count = frame_count * microphone->frame_size;

   memcpy(buffer, mic_input, MIN(buffer_size, frame_count));
   hr = _IAudioCaptureClient_ReleaseBuffer(microphone->capture, byte_count);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: Failed to release capture device \"%s\"'s buffer (%s)\n",
         microphone->device_name,
         hresult_name(hr));
      return false;
   }

   return true;
}

/**
 * Flushes microphone's most recent input to the provided context's FIFO queue.
 * WASAPI requires that fetched input be consumed in its entirety,
 * so the returned value may be less than the queue's size
 * if the next packet won't fit in it.
 * @param microphone Pointer to the microphone context.
 * @return The number of bytes in the queue, or -1 if there was an error.
 */
static ssize_t wasapi_microphone_fetch_fifo(wasapi_microphone_handle_t *microphone)
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
      BYTE *mic_input = NULL;
      UINT32 frames_read = 0;
      UINT32 bytes_read = 0;
      DWORD buffer_status_flags = 0;
      HRESULT hr;

      hr = _IAudioCaptureClient_GetBuffer(microphone->capture, &mic_input, &frames_read, &buffer_status_flags, NULL, NULL);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI]: Failed to get capture device \"%s\"'s buffer: %s\n",
            microphone->device_name,
            hresult_name(hr));
         return -1;
      }
      bytes_read = frames_read * microphone->frame_size;

      if (FIFO_WRITE_AVAIL(microphone->buffer) >= bytes_read && bytes_read > 0)
      { /* If the queue has room for the packets we just got... */
         fifo_write(microphone->buffer, mic_input, bytes_read);
         /* ...then enqueue the bytes directly from the mic's buffer */
      }
      else
      { /* Not enough space for new frames, so we can't consume this packet right now */
         frames_read = 0;
      }
      /* If there's insufficient room in the queue, then we can't read the packet.
       * In that case, we leave the packet for next time. */

      hr = _IAudioCaptureClient_ReleaseBuffer(microphone->capture, frames_read);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI]: Failed to release capture device \"%s\"'s buffer after consuming %u frames: %s\n",
            microphone->device_name,
            frames_read,
            hresult_name(hr));
         return -1;
      }

      if (!microphone->exclusive && frames_read > 0)
      { /* If this is a shared-mode stream and we didn't run out of room in the sample queue... */
         hr = _IAudioCaptureClient_GetNextPacketSize(microphone->capture, &next_packet_size);
         if (FAILED(hr))
         { /* Get the number of frames that the mic has for us. */
            RARCH_ERR("[WASAPI]: Failed to get capture device \"%s\"'s next packet size: %s\n",
                      microphone->device_name, hresult_name(hr));
            return -1;
         }
      }
      else
      { /* Exclusive-mode streams only deliver one packet at a time, though it's bigger. */
         next_packet_size = 0;
      }
   }
   while (next_packet_size != 0);

   return FIFO_READ_AVAIL(microphone->buffer);
}

/**
 * Blocks until the provided microphone's capture event is signalled.
 *
 * @param microphone The microphone to wait on.
 * @param timeout The amount of time to wait, in milliseconds.
 * @return true if the event was signalled,
 * false if it timed out or there was an error.
 */
static bool wasapi_microphone_wait_for_capture_event(wasapi_microphone_handle_t *microphone, DWORD timeout)
{
   switch (WaitForSingleObject(microphone->read_event, timeout))
   { /*...then let's wait for the mic to tell us that samples are ready. */
      case WAIT_OBJECT_0:
         /* Okay, there's data available. */
         return true;
      case WAIT_TIMEOUT:
         /* Time out; there's nothing here for us. */
         RARCH_ERR("[WASAPI]: Failed to wait for capture device \"%s\" event: Timeout after %ums\n", microphone->device_name, timeout);
         return false;
      default:
         RARCH_ERR("[WASAPI]: Failed to wait for capture device \"%s\" event: %s\n", microphone->device_name, wasapi_error(GetLastError()));
         return false;
   }
}

/**
 * Reads incoming samples from a shared-mode microphone,
 * without buffering any.
 * @param microphone
 * @param buffer
 * @param buffer_size
 * @return
 */
static ssize_t wasapi_microphone_read_shared_unbuffered(
   wasapi_microphone_handle_t *microphone,
   void *buffer,
   size_t buffer_size)
{
   size_t read_avail       = 0;
   ssize_t bytes_to_read   = -1;
   UINT32 available_frames = 0;
   HRESULT hr;

   if (!wasapi_microphone_wait_for_capture_event(microphone, INFINITE))
      return -1;

   hr = _IAudioClient_GetCurrentPadding(microphone->client, &available_frames);
   if (FAILED(hr))
   { /* Get the number of frames that the mic has for us. */
      RARCH_ERR("[WASAPI]: Failed to get capture device \"%s\"'s available frames: %s\n",
         microphone->device_name, hresult_name(hr));
      return -1;
   }

   read_avail = microphone->engine_buffer_size - available_frames * microphone->frame_size;
   if (!read_avail)
      return 0;

   bytes_to_read = MIN(buffer_size, read_avail);
   if (bytes_to_read)
      if (!wasapi_microphone_fetch_buffer(microphone, buffer, bytes_to_read))
         return -1;

   return bytes_to_read;
}

/**
 * Reads from a shared-mode microphone into the provided buffer
 * Handles both buffered and unbuffered microphone input,
 * depending on the presence of microphone::buffer.
 * @param microphone
 * @param buffer
 * @param buffer_size
 * @return The number of bytes read,
 * or -1 if there was an error.
 */
static ssize_t wasapi_microphone_read_shared_nonblock(
      wasapi_microphone_handle_t *microphone,
      void *buffer,
      size_t buffer_size)
{
   size_t read_avail       = 0;
   ssize_t bytes_to_read   = -1;
   UINT32 available_frames = 0;

   if (microphone->buffer)
   { /* If we're buffering mic input... */
      read_avail = FIFO_READ_AVAIL(microphone->buffer);
      if (!read_avail)
      { /* If the incoming sample queue is empty... */
         size_t write_avail  = 0;
         HRESULT hr = _IAudioClient_GetCurrentPadding(microphone->client, &available_frames);
         if (FAILED(hr))
         { /* Get the number of frames that the mic has for us. */
            char error[256];
            wasapi_log_hr(hr, error, sizeof(error));
            RARCH_ERR("[WASAPI]: Failed to get capture device \"%s\"'s available frames: %s\n", microphone->device_name, error);
            return -1;
         }

         write_avail  = FIFO_WRITE_AVAIL(microphone->buffer);
         read_avail = microphone->engine_buffer_size - available_frames * microphone->frame_size;
         bytes_to_read     = MIN(write_avail, read_avail);
         if (bytes_to_read)
            if (wasapi_microphone_fetch_fifo(microphone) < 0)
               return -1;
      }

      read_avail = FIFO_READ_AVAIL(microphone->buffer);
      bytes_to_read     = MIN(buffer_size, read_avail);
      if (bytes_to_read)
         fifo_write(microphone->buffer, buffer, bytes_to_read);
   }
   else
   {
      HRESULT hr = _IAudioClient_GetCurrentPadding(microphone->client, &available_frames);
      if (FAILED(hr))
      { /* Get the number of frames that the mic has for us. */
         char error[256];
         wasapi_log_hr(hr, error, sizeof(error));
         RARCH_ERR("[WASAPI]: Failed to get capture device \"%s\"'s available frames: %s\n", microphone->device_name, error);
         return -1;
      }

      read_avail = microphone->engine_buffer_size - available_frames * microphone->frame_size;
      if (!read_avail)
         return 0;

      bytes_to_read = MIN(buffer_size, read_avail);
      if (bytes_to_read)
         if (!wasapi_microphone_fetch_buffer(microphone, buffer, bytes_to_read))
            return -1;
   }

   return bytes_to_read;
}

/**
 * Reads samples from an exclusive-mode microphone,
 * fetching more from it if necessary.
 *
 * @param microphone Pointer to the context of the microphone
 * from which samples will be read.
 * @param buffer The buffer in which the fetched samples will be stored.
 * @param buffer_size The size of buffer, in bytes.
 * @param timeout Timeout for new samples, in milliseconds.
 * 0 means that this function won't wait for new samples,
 * \c INFINITE means that this function will wait indefinitely.
 * @return
 */
static ssize_t wasapi_microphone_read_buffered(
   wasapi_microphone_handle_t *microphone,
   void * buffer,
   size_t buffer_size,
   DWORD timeout)
{
   ssize_t bytes_read      = 0; /* Number of bytes sent to the core */
   ssize_t bytes_available = FIFO_READ_AVAIL(microphone->buffer);

   if (!bytes_available)
   { /* If we don't have any queued samples to give to the core... */
      if (!wasapi_microphone_wait_for_capture_event(microphone, timeout))
      { /* If we couldn't wait for the microphone to signal a capture event... */
         return -1;
      }

      bytes_available = wasapi_microphone_fetch_fifo(microphone);
      if (bytes_available < 0)
      { /* If we couldn't fetch samples from the microphone... */
         return -1;
      }
   }

   /* Now that we have samples available, let's give them to the core */

   bytes_read = MIN(buffer_size, bytes_available);
   fifo_read(microphone->buffer, buffer, bytes_read);
   /* Read data from the sample queue and store it in the provided buffer */

   return bytes_read;
}

static ssize_t wasapi_microphone_read(void *driver_context, void *mic_context, void *buffer, size_t buffer_size)
{
   size_t bytes_read                      = 0;
   wasapi_microphone_t *wasapi            = (wasapi_microphone_t *)driver_context;
   wasapi_microphone_handle_t *microphone = (wasapi_microphone_handle_t*)mic_context;

   if (!wasapi || !microphone || !buffer)
      return -1;

   if (wasapi->nonblock)
   { /* If microphones shouldn't block... */
      if (microphone->exclusive)
         return wasapi_microphone_read_buffered(microphone, buffer, buffer_size, 0);
      return wasapi_microphone_read_shared_nonblock(microphone, buffer, buffer_size);
   }

   if (microphone->exclusive)
   {
      ssize_t read;
      for (read = -1; bytes_read < buffer_size; bytes_read += read)
      {
         read = wasapi_microphone_read_buffered(microphone, (char *) buffer + bytes_read, buffer_size - bytes_read,
                                                INFINITE);
         if (read == -1)
            return -1;
      }
   }
   else
   {
      ssize_t read;
      if (microphone->buffer)
      {
         for (read = -1; bytes_read < buffer_size; bytes_read += read)
         {
            read = wasapi_microphone_read_buffered(microphone, (char *) buffer + bytes_read, buffer_size - bytes_read,
                                                   INFINITE);
            if (read == -1)
               return -1;
         }
      }
      else
      {
         for (read = -1; bytes_read < buffer_size; bytes_read += read)
         {
            read = wasapi_microphone_read_shared_unbuffered(microphone, (char *) buffer + bytes_read, buffer_size - bytes_read);
            if (read == -1)
               return -1;
         }
      }
   }

   return bytes_read;
}

static bool wasapi_microphone_mic_alive(const void *driver_context, const void *mic_context);
static bool wasapi_microphone_start_mic(void *driver_context, void *microphone_context);
static bool wasapi_microphone_start(void *driver_context, bool is_shutdown)
{
   wasapi_microphone_t *wasapi = (wasapi_microphone_t*)driver_context;

   if (!wasapi)
      return false;

   if (wasapi->microphone && wasapi_microphone_mic_alive(wasapi, wasapi->microphone))
   { /* If we have a microphone that was active at the time the driver stopped... */
      bool result = wasapi_microphone_start_mic(wasapi, wasapi->microphone);
   }

   wasapi->running = true;

   return true;
}

static bool wasapi_microphone_stop_mic(void *driver_context, void *microphone_context);
static bool wasapi_microphone_stop(void *driver_context)
{
   wasapi_microphone_t *wasapi = (wasapi_microphone_t*)driver_context;

   if (!wasapi)
      return false;

   if (wasapi->microphone && wasapi_microphone_mic_alive(wasapi, wasapi->microphone))
   { /* If we have a microphone that we need to pause... */
      bool result = wasapi_microphone_stop_mic(wasapi, wasapi->microphone);
   }

   wasapi->running = false;

   return true;
}

static bool wasapi_microphone_alive(void *driver_context)
{
   wasapi_microphone_t *wasapi = (wasapi_microphone_t*)driver_context;

   return wasapi->running;
}

static void wasapi_microphone_set_nonblock_state(void *driver_context, bool nonblock)
{
   wasapi_microphone_t *wasapi = (wasapi_microphone_t*)driver_context;

   RARCH_LOG("[WASAPI mic]: Sync %s.\n", nonblock ? "off" : "on");

   wasapi->nonblock = nonblock;
}

static void *wasapi_microphone_open_mic(void *driver_context, const char *device, unsigned rate,
                                        unsigned latency, unsigned block_frames, unsigned *new_rate)
{
   settings_t *settings          = config_get_ptr();
   HRESULT hr;
   DWORD flags                   = 0;
   UINT32 frame_count            = 0;
   REFERENCE_TIME dev_period     = 0;
   BYTE *dest                    = NULL;
   bool float_format             = settings->bools.microphone_wasapi_float_format;
   bool exclusive_mode           = settings->bools.microphone_wasapi_exclusive_mode;
   int sh_buffer_length          = settings->ints.microphone_wasapi_sh_buffer_length;
   wasapi_microphone_t *wasapi   = (wasapi_microphone_t*)driver_context;
   wasapi_microphone_handle_t *microphone = calloc(1, sizeof(wasapi_microphone_handle_t));

   if (!microphone)
      return NULL;

   microphone->exclusive              = exclusive_mode;
   microphone->device                 = wasapi_init_device(device, eCapture);
   if (device && !microphone->device)
   { /* If we requested a particular capture device, but couldn't open it... */
      RARCH_WARN("[WASAPI]: Failed to open requested capture device \"%s\", attempting to open default device\n", device);
      microphone->device = wasapi_init_device(NULL, eCapture);
   }

   if (!microphone->device)
   {
      RARCH_ERR("[WASAPI]: Failed to open capture device\n");
      goto error;
   }

   microphone->device_name = mmdevice_name(microphone->device);
   if (!microphone->device_name)
   {
      RARCH_ERR("[WASAPI]: Failed to get friendly name of capture device\n");
      goto error;
   }

   microphone->client = wasapi_init_client(microphone->device,
      &microphone->exclusive, &float_format, &rate, latency, 1);
   if (!microphone->client)
   {
      RARCH_ERR("[WASAPI]: Failed to open client for capture device \"%s\"\n", microphone->device_name);
      goto error;
   }

   hr = _IAudioClient_GetBufferSize(microphone->client, &frame_count);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: Failed to get buffer size of IAudioClient for capture device \"%s\": %s\n",
          microphone->device_name, hresult_name(hr));
      goto error;
   }

   microphone->frame_size         = float_format ? sizeof(float) : sizeof(int16_t);
   microphone->engine_buffer_size = frame_count * microphone->frame_size;

   if (microphone->exclusive)
   { /* If this mic should be used *exclusively* by RetroArch... */
      microphone->buffer = fifo_new(microphone->engine_buffer_size);
      if (!microphone->buffer)
      {
         RARCH_ERR("[WASAPI]: Failed to initialize FIFO queue for capture device.\n");
         goto error;
      }

      RARCH_LOG("[WASAPI]: Intermediate exclusive-mode capture buffer length is %u frames (%.1fms, %u bytes).\n",
                frame_count, (double)frame_count * 1000.0 / rate, microphone->engine_buffer_size);
   }
   else if (sh_buffer_length)
   {
      if (sh_buffer_length < 0)
      { /* If the user selected the "default" shared buffer length... */
         hr = _IAudioClient_GetDevicePeriod(microphone->client, &dev_period, NULL);
         if (FAILED(hr))
            goto error;

         sh_buffer_length = (dev_period * rate / 10000000) * 2;
         /* Default buffer seems to be too small, resulting in slowdown.
          * Doubling it seems to work okay. Dunno why. */
      }

      microphone->buffer = fifo_new(sh_buffer_length * microphone->frame_size);
      if (!microphone->buffer)
         goto error;

      RARCH_LOG("[WASAPI]: Intermediate shared-mode capture buffer length is %u frames (%.1fms, %u bytes).\n",
                sh_buffer_length, (double)sh_buffer_length * 1000.0 / rate, sh_buffer_length * microphone->frame_size);
   }
   else
   {
      RARCH_LOG("[WASAPI]: Intermediate capture buffer is off.\n");
   }

   microphone->read_event = CreateEventA(NULL, FALSE, FALSE, NULL);
   if (!microphone->read_event)
   {
      RARCH_ERR("[WASAPI]: Failed to allocate capture device's event handle\n");
      goto error;
   }

   hr = _IAudioClient_SetEventHandle(microphone->client, microphone->read_event);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: Failed to set capture device's event handle: %s\n", hresult_name(hr));
      goto error;
   }

   hr = _IAudioClient_GetService(microphone->client,
                                 IID_IAudioCaptureClient, (void**)&microphone->capture);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: Failed to get capture device's IAudioCaptureClient service: %s\n", hresult_name(hr));
      goto error;
   }

   /* Get and release the buffer, just to ensure that we can. */
   hr = _IAudioCaptureClient_GetBuffer(microphone->capture, &dest, &frame_count, &flags, NULL, NULL);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: Failed to get capture client buffer: %s\n", hresult_name(hr));
      goto error;
   }

   hr = _IAudioCaptureClient_ReleaseBuffer(microphone->capture, 0);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI]: Failed to release capture client buffer: %s\n", hresult_name(hr));
      goto error;
   }

   wasapi->microphone = microphone;
   if (new_rate)
   { /* The rate was (possibly) modified when we initialized the client */
      *new_rate = rate;
   }
   return microphone;

error:
   IFACE_RELEASE(microphone->capture);
   IFACE_RELEASE(microphone->client);
   IFACE_RELEASE(microphone->device);
   if (microphone->read_event)
      CloseHandle(microphone->read_event);
   if (microphone->buffer)
      fifo_free(microphone->buffer);
   if (microphone->device_name)
      free(microphone->device_name);
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

   write_event = microphone->read_event;

   IFACE_RELEASE(microphone->capture);
   if (microphone->client)
      _IAudioClient_Stop(microphone->client);
   IFACE_RELEASE(microphone->client);
   IFACE_RELEASE(microphone->device);
   if (microphone->buffer)
      fifo_free(microphone->buffer);
   if (microphone->device_name)
      free(microphone->device_name);
   free(microphone);

   wasapi->microphone = NULL;

   ir = WaitForSingleObject(write_event, 20);
   if (ir == WAIT_FAILED)
   {
      char error[256];
      wasapi_log_hr(HRESULT_FROM_WIN32(GetLastError()), error, sizeof(error));
      RARCH_ERR("[WASAPI mic]: WaitForSingleObject failed with error %d: %s\n", GetLastError(), error);
   }

   /* If event isn't signaled log and leak */
   if (ir != WAIT_OBJECT_0)
      return;

   CloseHandle(write_event);
}

static bool wasapi_microphone_start_mic(void *driver_context, void *microphone_context)
{
   wasapi_microphone_t *wasapi = (wasapi_microphone_t*)driver_context;
   wasapi_microphone_handle_t *microphone = (wasapi_microphone_handle_t*)microphone_context;

   if (!wasapi || !microphone)
      return false;

   if (wasapi_microphone_alive(wasapi))
   { /* If the microphone should be active... */
      HRESULT hr = _IAudioClient_Start(microphone->client);

      if (SUCCEEDED(hr) || hr == AUDCLNT_E_NOT_STOPPED)
      { /* Starting an active microphone is not an error */

         microphone->running = true;
      }
      else
      {
         RARCH_ERR("[WASAPI mic]: Failed to start capture device \"%s\"'s IAudioClient: %s\n",
            microphone->device_name, hresult_name(hr));
         microphone->running = false;
      }
   }
   else
   {
      microphone->running = true;
      /* The microphone will resume next time the driver itself is resumed */
   }


   return microphone->running;
}

static bool wasapi_microphone_stop_mic(void *driver_context, void *microphone_context)
{
   wasapi_microphone_t *w = (wasapi_microphone_t*)driver_context;
   wasapi_microphone_handle_t *microphone = (wasapi_microphone_handle_t*)microphone_context;
   HRESULT hr;

   if (!w || !microphone)
      return false;

   hr = _IAudioClient_Stop(microphone->client);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI mic]: Failed to stop capture device \"%s\"'s IAudioClient: %s\n",
         microphone->device_name, hresult_name(hr));
      return false;
   }

   RARCH_LOG("[WASAPI mic]: Stopped capture device \"%s\"\n", microphone->device_name);

   microphone->running = false;

   return true;
}

static bool wasapi_microphone_mic_alive(const void *driver_context, const void *mic_context)
{
   wasapi_microphone_handle_t *microphone = (wasapi_microphone_handle_t *)mic_context;
   (void)driver_context;

   return microphone && microphone->running;
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

static bool wasapi_microphone_use_float(const void *driver_context, const void *microphone_context)
{
   wasapi_microphone_handle_t *microphone = (wasapi_microphone_handle_t *)microphone_context;
   (void)driver_context;

   if (!driver_context || !microphone)
      return false;

   return microphone->frame_size == sizeof(float);
}

microphone_driver_t microphone_wasapi = {
      wasapi_microphone_init,
      wasapi_microphone_free,
      wasapi_microphone_read,
      wasapi_microphone_start,
      wasapi_microphone_stop,
      wasapi_microphone_alive,
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