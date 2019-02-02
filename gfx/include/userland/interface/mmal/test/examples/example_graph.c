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
#include "util/mmal_graph.h"
#include "util/mmal_default_components.h"
#include "util/mmal_util_params.h"
#include <stdio.h>

#define CHECK_STATUS(status, msg) if (status != MMAL_SUCCESS) { fprintf(stderr, msg"\n"); goto error; }

int main(int argc, char **argv)
{
   MMAL_STATUS_T status;
   MMAL_GRAPH_T *graph = 0;
   MMAL_COMPONENT_T *reader = 0, *decoder = 0, *renderer = 0;

   if (argc < 2)
   {
      fprintf(stderr, "invalid arguments\n");
      return -1;
   }

   bcm_host_init();

   /* Create the graph */
   status = mmal_graph_create(&graph, 0);
   CHECK_STATUS(status, "failed to create graph");

   /* Add the components */
   status = mmal_graph_new_component(graph, MMAL_COMPONENT_DEFAULT_CONTAINER_READER, &reader);
   CHECK_STATUS(status, "failed to create reader");

   status = mmal_graph_new_component(graph, MMAL_COMPONENT_DEFAULT_VIDEO_DECODER, &decoder);
   CHECK_STATUS(status, "failed to create decoder");

   status = mmal_graph_new_component(graph, MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &renderer);
   CHECK_STATUS(status, "failed to create renderer");

   /* Configure the reader using the given URI */
   status = mmal_util_port_set_uri(reader->control, argv[1]);
   CHECK_STATUS(status, "failed to set uri");

   /* connect them up - this propagates port settings from outputs to inputs */
   status = mmal_graph_new_connection(graph, reader->output[0], decoder->input[0], 0, NULL);
   CHECK_STATUS(status, "failed to connect reader to decoder");
   status = mmal_graph_new_connection(graph, decoder->output[0], renderer->input[0], 0, NULL);
   CHECK_STATUS(status, "failed to connect decoder to renderer");

   /* Start playback */
   fprintf(stderr, "start playback\n");
   status = mmal_graph_enable(graph, NULL, NULL);
   CHECK_STATUS(status, "failed to enable graph");

   sleep(5);

   /* Stop everything */
   fprintf(stderr, "stop playback\n");
   mmal_graph_disable(graph);

 error:
   /* Cleanup everything */
   if (reader)
      mmal_component_release(reader);
   if (decoder)
      mmal_component_release(decoder);
   if (renderer)
      mmal_component_release(renderer);
   if (graph)
      mmal_graph_destroy(graph);

   return status == MMAL_SUCCESS ? 0 : -1;
}
