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

#include "mmal.h"
#include "core/mmal_component_private.h"
#include "core/mmal_port_private.h"
#include "util/mmal_graph.h"
#include "mmal_logging.h"

#define AGGREGATOR_PREFIX "aggregator"
#define AGGREGATOR_PIPELINE_PREFIX "pipeline"

typedef struct MMAL_GRAPH_USERDATA_T {
    int dummy;
} MMAL_GRAPH_USERDATA_T;

static MMAL_STATUS_T aggregator_parameter_set(MMAL_GRAPH_T *graph,
   MMAL_PORT_T *port, const MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_PARAM_UNUSED(graph);
   MMAL_PARAM_UNUSED(port);
   MMAL_PARAM_UNUSED(param);
   LOG_TRACE("graph %p, port %p, param %p", graph, port, param);
   return MMAL_ENOSYS;
}

static MMAL_STATUS_T aggregator_parameter_get(MMAL_GRAPH_T *graph,
      MMAL_PORT_T *port, MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_PARAM_UNUSED(graph);
   MMAL_PARAM_UNUSED(port);
   MMAL_PARAM_UNUSED(param);
   LOG_TRACE("graph %p, port %p, param %p", graph, port, param);
   return MMAL_ENOSYS;
}

/** Create an instance of a component  */
static MMAL_STATUS_T mmal_component_create_aggregator_pipeline(const char *full_name,
   const char *component_names, MMAL_COMPONENT_T *component)
{
   MMAL_GRAPH_T *graph = 0;
   MMAL_COMPONENT_T *subcomponent[2] = {0};
   MMAL_STATUS_T status = MMAL_ENOMEM;
   unsigned int length;
   char *orig, *names;

   length = strlen(component_names);
   names = orig = vcos_calloc(1, length + 1, "mmal aggregator");
   if (!names)
      goto error;
   memcpy(names, component_names, length);

   /* We'll build the aggregator using a graph */
   status = mmal_graph_create(&graph, sizeof(MMAL_GRAPH_USERDATA_T));
   if (status != MMAL_SUCCESS)
      goto error;
   graph->pf_parameter_get = aggregator_parameter_get;
   graph->pf_parameter_set = aggregator_parameter_set;

   /* Iterate through all the specified components */
   while (*names)
   {
      MMAL_CONNECTION_T *connection;
      const char *name;

      /* Switch to a new connection */
      if (subcomponent[0])
         mmal_component_destroy(subcomponent[0]);
      subcomponent[0] = subcomponent[1];
      subcomponent[1] = 0;

      /* Extract the name of the next component */
      for (name = names; *names && *names != ':'; names++);

      /* Replace the separator */
      if (*names)
         *(names++) = 0;

      /* Skip empty strings */
      if (!*name)
         continue;

      status = mmal_component_create(name, &subcomponent[1]);
      if (status != MMAL_SUCCESS)
         goto error;

      status = mmal_graph_add_component(graph, subcomponent[1]);
      if (status != MMAL_SUCCESS)
         goto error;

      /* Special case for dealing with the first component in the chain */
      if (!subcomponent[0])
      {
         /* Add first input port if any */
         if (subcomponent[1]->input_num)
         {
            status = mmal_graph_add_port(graph, subcomponent[1]->input[0]);
            if (status != MMAL_SUCCESS)
               goto error;
         }
         continue;
      }

      /* Create connection between the 2 ports */
      if (subcomponent[0]->output_num < 1 || subcomponent[1]->input_num < 1)
         goto error;
      status = mmal_connection_create(&connection, subcomponent[0]->output[0],
         subcomponent[1]->input[0], 0);
      if (status != MMAL_SUCCESS)
         goto error;

      status = mmal_graph_add_connection(graph, connection);
      /* Now the connection is added to the graph we don't care about it anymore */
      mmal_connection_destroy(connection);
      if (status != MMAL_SUCCESS)
         goto error;
   }

   /* Add last output port if any */
   if (subcomponent[1] && subcomponent[1]->output_num && subcomponent[1]->output[0])
   {
      status = mmal_graph_add_port(graph, subcomponent[1]->output[0]);
      if (status != MMAL_SUCCESS)
         goto error;
   }

   /* Build the graph */
   component->priv->module = (struct MMAL_COMPONENT_MODULE_T *)graph;
   status = mmal_graph_component_constructor(full_name, component);
   if (status != MMAL_SUCCESS)
      goto error;

 end:
   if (subcomponent[0])
      mmal_component_destroy(subcomponent[0]);
   if (subcomponent[1])
      mmal_component_destroy(subcomponent[1]);
   vcos_free(orig);
   return status;

 error:
   if (graph)
      mmal_graph_destroy(graph);
   goto end;
}

/** Create an instance of a component  */
static MMAL_STATUS_T mmal_component_create_aggregator(const char *name, MMAL_COMPONENT_T *component)
{
   const char *stripped = name + sizeof(AGGREGATOR_PREFIX);

   /* Select the requested aggregator */
   if (!strncmp(stripped, AGGREGATOR_PIPELINE_PREFIX ":", sizeof(AGGREGATOR_PIPELINE_PREFIX)))
      return mmal_component_create_aggregator_pipeline(name,
         stripped + sizeof(AGGREGATOR_PIPELINE_PREFIX), component);

   return MMAL_ENOSYS;
}

MMAL_CONSTRUCTOR(mmal_register_component_aggregator);
void mmal_register_component_aggregator(void)
{
   mmal_component_supplier_register(AGGREGATOR_PREFIX, mmal_component_create_aggregator);
}
