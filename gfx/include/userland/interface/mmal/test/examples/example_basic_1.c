/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "bcm_host.h"
#include "mmal.h"
#include "util/mmal_default_components.h"
#include "interface/vcos/vcos.h"
#include <stdio.h>

#define CHECK_STATUS(status, msg) if (status != MMAL_SUCCESS) { fprintf(stderr, msg"\n"); goto error; }

static uint8_t codec_header_bytes[512];
static unsigned int codec_header_bytes_size = sizeof(codec_header_bytes);

static FILE *source_file;

/* Macros abstracting the I/O, just to make the example code clearer */
#define SOURCE_OPEN(uri) \
   source_file = fopen(uri, "rb"); if (!source_file) goto error;
#define SOURCE_READ_CODEC_CONFIG_DATA(bytes, size) \
   size = fread(bytes, 1, size, source_file); rewind(source_file)
#define SOURCE_READ_DATA_INTO_BUFFER(a) \
   a->length = fread(a->data, 1, a->alloc_size - 128, source_file); \
   a->offset = 0; a->pts = a->dts = MMAL_TIME_UNKNOWN
#define SOURCE_CLOSE() \
   if (source_file) fclose(source_file)

/** Context for our application */
static struct CONTEXT_T {
   VCOS_SEMAPHORE_T semaphore;
   MMAL_QUEUE_T *queue;
} context;

/** Callback from the input port.
 * Buffer has been consumed and is available to be used again. */
static void input_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   struct CONTEXT_T *ctx = (struct CONTEXT_T *)port->userdata;

   /* The decoder is done with the data, just recycle the buffer header into its pool */
   mmal_buffer_header_release(buffer);

   /* Kick the processing thread */
   vcos_semaphore_post(&ctx->semaphore);
}

/** Callback from the output port.
 * Buffer has been produced by the port and is available for processing. */
static void output_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   struct CONTEXT_T *ctx = (struct CONTEXT_T *)port->userdata;

   /* Queue the decoded video frame */
   mmal_queue_put(ctx->queue, buffer);

   /* Kick the processing thread */
   vcos_semaphore_post(&ctx->semaphore);
}

int main(int argc, char **argv)
{
   MMAL_STATUS_T status = MMAL_EINVAL;
   MMAL_COMPONENT_T *decoder = 0;
   MMAL_POOL_T *pool_in = 0, *pool_out = 0;
   unsigned int count;

   if (argc < 2)
   {
      fprintf(stderr, "invalid arguments\n");
      return -1;
   }

   bcm_host_init();

   vcos_semaphore_create(&context.semaphore, "example", 1);

   SOURCE_OPEN(argv[1]);

   /* Create the decoder component.
    * This specific component exposes 2 ports (1 input and 1 output). Like most components
    * its expects the format of its input port to be set by the client in order for it to
    * know what kind of data it will be fed. */
   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_DECODER, &decoder);
   CHECK_STATUS(status, "failed to create decoder");

   /* Set format of video decoder input port */
   MMAL_ES_FORMAT_T *format_in = decoder->input[0]->format;
   format_in->type = MMAL_ES_TYPE_VIDEO;
   format_in->encoding = MMAL_ENCODING_H264;
   format_in->es->video.width = 1280;
   format_in->es->video.height = 720;
   format_in->es->video.frame_rate.num = 30;
   format_in->es->video.frame_rate.den = 1;
   format_in->es->video.par.num = 1;
   format_in->es->video.par.den = 1;
   /* If the data is known to be framed then the following flag should be set:
    * format_in->flags |= MMAL_ES_FORMAT_FLAG_FRAMED; */

   SOURCE_READ_CODEC_CONFIG_DATA(codec_header_bytes, codec_header_bytes_size);
   status = mmal_format_extradata_alloc(format_in, codec_header_bytes_size);
   CHECK_STATUS(status, "failed to allocate extradata");
   format_in->extradata_size = codec_header_bytes_size;
   if (format_in->extradata_size)
      memcpy(format_in->extradata, codec_header_bytes, format_in->extradata_size);

   status = mmal_port_format_commit(decoder->input[0]);
   CHECK_STATUS(status, "failed to commit format");

   /* Display the output port format */
   MMAL_ES_FORMAT_T *format_out = decoder->output[0]->format;
   fprintf(stderr, "%s\n", decoder->output[0]->name);
   fprintf(stderr, " type: %i, fourcc: %4.4s\n", format_out->type, (char *)&format_out->encoding);
   fprintf(stderr, " bitrate: %i, framed: %i\n", format_out->bitrate,
           !!(format_out->flags & MMAL_ES_FORMAT_FLAG_FRAMED));
   fprintf(stderr, " extra data: %i, %p\n", format_out->extradata_size, format_out->extradata);
   fprintf(stderr, " width: %i, height: %i, (%i,%i,%i,%i)\n",
           format_out->es->video.width, format_out->es->video.height,
           format_out->es->video.crop.x, format_out->es->video.crop.y,
           format_out->es->video.crop.width, format_out->es->video.crop.height);

   /* The format of both ports is now set so we can get their buffer requirements and create
    * our buffer headers. We use the buffer pool API to create these. */
   decoder->input[0]->buffer_num = decoder->input[0]->buffer_num_min;
   decoder->input[0]->buffer_size = decoder->input[0]->buffer_size_min;
   decoder->output[0]->buffer_num = decoder->output[0]->buffer_num_min;
   decoder->output[0]->buffer_size = decoder->output[0]->buffer_size_min;
   pool_in = mmal_pool_create(decoder->input[0]->buffer_num,
                              decoder->input[0]->buffer_size);
   pool_out = mmal_pool_create(decoder->output[0]->buffer_num,
                               decoder->output[0]->buffer_size);

   /* Create a queue to store our decoded video frames. The callback we will get when
    * a frame has been decoded will put the frame into this queue. */
   context.queue = mmal_queue_create();

   /* Store a reference to our context in each port (will be used during callbacks) */
   decoder->input[0]->userdata = (void *)&context;
   decoder->output[0]->userdata = (void *)&context;

   /* Enable all the input port and the output port.
    * The callback specified here is the function which will be called when the buffer header
    * we sent to the component has been processed. */
   status = mmal_port_enable(decoder->input[0], input_callback);
   CHECK_STATUS(status, "failed to enable input port");
   status = mmal_port_enable(decoder->output[0], output_callback);
   CHECK_STATUS(status, "failed to enable output port");

   /* Component won't start processing data until it is enabled. */
   status = mmal_component_enable(decoder);
   CHECK_STATUS(status, "failed to enable component");

   /* Start decoding */
   fprintf(stderr, "start decoding\n");

   /* This is the main processing loop */
   for (count = 0; count < 500; count++)
   {
      MMAL_BUFFER_HEADER_T *buffer;

      /* Wait for buffer headers to be available on either of the decoder ports */
      vcos_semaphore_wait(&context.semaphore);

      /* Send data to decode to the input port of the video decoder */
      if ((buffer = mmal_queue_get(pool_in->queue)) != NULL)
      {
         SOURCE_READ_DATA_INTO_BUFFER(buffer);
         if (!buffer->length)
            break;

         fprintf(stderr, "sending %i bytes\n", (int)buffer->length);
         status = mmal_port_send_buffer(decoder->input[0], buffer);
         CHECK_STATUS(status, "failed to send buffer");
      }

      /* Get our decoded frames */
      while ((buffer = mmal_queue_get(context.queue)) != NULL)
      {
         /* We have a frame, do something with it (why not display it for instance?).
          * Once we're done with it, we release it. It will automatically go back
          * to its original pool so it can be reused for a new video frame.
          */
         fprintf(stderr, "decoded frame\n");
         mmal_buffer_header_release(buffer);
      }

      /* Send empty buffers to the output port of the decoder */
      while ((buffer = mmal_queue_get(pool_out->queue)) != NULL)
      {
         status = mmal_port_send_buffer(decoder->output[0], buffer);
         CHECK_STATUS(status, "failed to send buffer");
      }
   }

   /* Stop decoding */
   fprintf(stderr, "stop decoding\n");

   /* Stop everything. Not strictly necessary since mmal_component_destroy()
    * will do that anyway */
   mmal_port_disable(decoder->input[0]);
   mmal_port_disable(decoder->output[0]);
   mmal_component_disable(decoder);

 error:
   /* Cleanup everything */
   if (decoder)
      mmal_component_destroy(decoder);
   if (pool_in)
      mmal_pool_destroy(pool_in);
   if (pool_out)
      mmal_pool_destroy(pool_out);
   if (context.queue)
      mmal_queue_destroy(context.queue);

   SOURCE_CLOSE();
   vcos_semaphore_delete(&context.semaphore);
   return status == MMAL_SUCCESS ? 0 : -1;
}
