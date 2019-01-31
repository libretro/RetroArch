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
#include "util/mmal_connection.h"
#include "util/mmal_default_components.h"
#include "util/mmal_util_params.h"
#include "interface/vcos/vcos.h"
#include <stdio.h>

#define CHECK_STATUS(status, msg) if (status != MMAL_SUCCESS) { fprintf(stderr, msg"\n"); goto error; }

/** Context for our application */
static struct CONTEXT_T {
   VCOS_SEMAPHORE_T semaphore;
   MMAL_STATUS_T status;
   MMAL_BOOL_T eos;
} context;

/** Callback from a control port. Error and EOS events stop playback. */
static void control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   struct CONTEXT_T *ctx = (struct CONTEXT_T *)port->userdata;

   if (buffer->cmd == MMAL_EVENT_ERROR)
      ctx->status = *(MMAL_STATUS_T *)buffer->data;
   else if (buffer->cmd == MMAL_EVENT_EOS)
      ctx->eos = MMAL_TRUE;

   mmal_buffer_header_release(buffer);

   /* The processing is done in our main thread */
   vcos_semaphore_post(&ctx->semaphore);
}

/** Callback from the connection. Buffer is available. */
static void connection_callback(MMAL_CONNECTION_T *connection)
{
   struct CONTEXT_T *ctx = (struct CONTEXT_T *)connection->user_data;

   /* The processing is done in our main thread */
   vcos_semaphore_post(&ctx->semaphore);
}

int main(int argc, char **argv)
{
   MMAL_STATUS_T status;
   MMAL_COMPONENT_T *reader = 0, *decoder = 0, *renderer = 0;
   MMAL_CONNECTION_T *connection[2] = {0};
   unsigned int i, count, connection_num = vcos_countof(connection);

   if (argc < 2)
   {
      fprintf(stderr, "invalid arguments\n");
      return -1;
   }

   bcm_host_init();

   vcos_semaphore_create(&context.semaphore, "example", 1);

   /* Create the components */
   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CONTAINER_READER, &reader);
   CHECK_STATUS(status, "failed to create reader");
   reader->control->userdata = (void *)&context;
   status = mmal_port_enable(reader->control, control_callback);
   CHECK_STATUS(status, "failed to enable control port");
   status = mmal_component_enable(reader);
   CHECK_STATUS(status, "failed to enable component");

   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_DECODER, &decoder);
   CHECK_STATUS(status, "failed to create decoder");
   decoder->control->userdata = (void *)&context;
   status = mmal_port_enable(decoder->control, control_callback);
   CHECK_STATUS(status, "failed to enable control port");
   status = mmal_component_enable(decoder);
   CHECK_STATUS(status, "failed to enable component");

   status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &renderer);
   CHECK_STATUS(status, "failed to create renderer");
   renderer->control->userdata = (void *)&context;
   status = mmal_port_enable(renderer->control, control_callback);
   CHECK_STATUS(status, "failed to enable control port");
   status = mmal_component_enable(renderer);
   CHECK_STATUS(status, "failed to enable component");

   /* Configure the reader using the given URI */
   status = mmal_util_port_set_uri(reader->control, argv[1]);
   CHECK_STATUS(status, "failed to set uri");

   /* Create the connections between the components */
   status = mmal_connection_create(&connection[0], reader->output[0], decoder->input[0], 0);
   CHECK_STATUS(status, "failed to create connection between reader / decoder");
   connection[0]->user_data = &context;
   connection[0]->callback = connection_callback;
   status = mmal_connection_create(&connection[1], decoder->output[0], renderer->input[0], 0);
   CHECK_STATUS(status, "failed to create connection between decoder / renderer");
   connection[1]->user_data = &context;
   connection[1]->callback = connection_callback;

   /* Enable all our connections */
   for (i = connection_num; i; i--)
   {
      status = mmal_connection_enable(connection[i-1]);
      CHECK_STATUS(status, "failed to enable connection");
   }

   /* Start playback */
   fprintf(stderr, "start playback\n");

   /* This is the main processing loop */
   for (count = 0; count < 500; count++)
   {
      MMAL_BUFFER_HEADER_T *buffer;
      vcos_semaphore_wait(&context.semaphore);

      /* Check for errors */
      status = context.status;
      CHECK_STATUS(status, "error during playback");

      /* Check for end of stream */
      if (context.eos)
         break;

      /* Handle buffers for all our connections */
      for (i = 0; i < connection_num; i++)
      {
         if (connection[i]->flags & MMAL_CONNECTION_FLAG_TUNNELLING)
            continue; /* Nothing else to do in tunnelling mode */

         /* Send empty buffers to the output port of the connection */
         while ((buffer = mmal_queue_get(connection[i]->pool->queue)) != NULL)
         {
            status = mmal_port_send_buffer(connection[i]->out, buffer);
            CHECK_STATUS(status, "failed to send buffer");
         }

         /* Send any queued buffer to the next component */
         while ((buffer = mmal_queue_get(connection[i]->queue)) != NULL)
         {
            status = mmal_port_send_buffer(connection[i]->in, buffer);
            CHECK_STATUS(status, "failed to send buffer");
         }
      }
   }

   /* Stop everything */
   fprintf(stderr, "stop playback\n");
   for (i = 0; i < connection_num; i++)
   {
      mmal_connection_disable(connection[i]);
   }

 error:
   /* Cleanup everything */
   for (i = 0; i < connection_num; i++)
   {
      if (connection[i])
         mmal_connection_destroy(connection[i]);
   }
   if (reader)
      mmal_component_destroy(reader);
   if (decoder)
      mmal_component_destroy(decoder);
   if (renderer)
      mmal_component_destroy(renderer);

   vcos_semaphore_delete(&context.semaphore);
   return status == MMAL_SUCCESS ? 0 : -1;
}
