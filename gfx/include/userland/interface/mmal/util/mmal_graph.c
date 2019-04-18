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
#include "util/mmal_util.h"
#include "util/mmal_graph.h"
#include "core/mmal_component_private.h"
#include "core/mmal_port_private.h"
#include "mmal_logging.h"

#define GRAPH_CONNECTIONS_MAX 16
#define PROCESSING_TIME_MAX 20000

/*****************************************************************************/

/** Private context for our graph.
 * This also acts as a MMAL_COMPONENT_MODULE_T for when components are instantiated from graphs */
typedef struct MMAL_COMPONENT_MODULE_T
{
   MMAL_GRAPH_T graph; /**< Must be the first member! */

   MMAL_COMPONENT_T *component[GRAPH_CONNECTIONS_MAX];
   MMAL_GRAPH_TOPOLOGY_T topology[GRAPH_CONNECTIONS_MAX];
   unsigned int component_num;

   MMAL_CONNECTION_T *connection[GRAPH_CONNECTIONS_MAX];
   unsigned int connection_num;
   unsigned int connection_current;

   MMAL_PORT_T *input[GRAPH_CONNECTIONS_MAX];
   unsigned int input_num;
   MMAL_PORT_T *output[GRAPH_CONNECTIONS_MAX];
   unsigned int output_num;
   MMAL_PORT_T *clock[GRAPH_CONNECTIONS_MAX];
   unsigned int clock_num;

   MMAL_COMPONENT_T *graph_component;

   MMAL_BOOL_T stop_thread;      /**< informs the worker thread to exit */
   VCOS_THREAD_T thread;         /**< worker thread which processes all internal connections */
   VCOS_SEMAPHORE_T sema;        /**< informs the worker thread that buffers are available */

   MMAL_GRAPH_EVENT_CB event_cb; /**< callback for sending control port events to the client */
   void *event_cb_data;          /**< callback data supplied by the client */

} MMAL_GRAPH_PRIVATE_T;

typedef MMAL_GRAPH_PRIVATE_T MMAL_COMPONENT_MODULE_T;

/*****************************************************************************/
static MMAL_STATUS_T mmal_component_create_from_graph(const char *name, MMAL_COMPONENT_T *component);
static MMAL_BOOL_T graph_do_processing(MMAL_GRAPH_PRIVATE_T *graph);
static void graph_process_buffer(MMAL_GRAPH_PRIVATE_T *graph_private,
   MMAL_CONNECTION_T *connection, MMAL_BUFFER_HEADER_T *buffer);

/*****************************************************************************/
static void graph_control_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_GRAPH_PRIVATE_T *graph = (MMAL_GRAPH_PRIVATE_T *)port->userdata;

   LOG_TRACE("port: %s(%p), buffer: %p, event: %4.4s", port->name, port,
             buffer, (char *)&buffer->cmd);

   if (graph->event_cb)
   {
      graph->event_cb((MMAL_GRAPH_T *)graph, port, buffer, graph->event_cb_data);
   }
   else
   {
      LOG_ERROR("event lost on port %i,%i (event callback not defined)",
                (int)port->type, (int)port->index);
      mmal_buffer_header_release(buffer);
   }
}

/*****************************************************************************/
static void graph_connection_cb(MMAL_CONNECTION_T *connection)
{
   MMAL_GRAPH_PRIVATE_T *graph = (MMAL_GRAPH_PRIVATE_T *)connection->user_data;
   MMAL_BUFFER_HEADER_T *buffer;

   if (connection->flags == MMAL_CONNECTION_FLAG_DIRECT &&
       (buffer = mmal_queue_get(connection->queue)) != NULL)
   {
      graph_process_buffer(graph, connection, buffer);
      return;
   }

   vcos_semaphore_post(&graph->sema);
}

/*****************************************************************************/
static void* graph_worker_thread(void* ctx)
{
   MMAL_GRAPH_PRIVATE_T *graph = (MMAL_GRAPH_PRIVATE_T *)ctx;

   while (1)
   {
      vcos_semaphore_wait(&graph->sema);
      if (graph->stop_thread)
         break;
      while(graph_do_processing(graph));
   }

   LOG_TRACE("worker thread exit %p", graph);

   return 0;
}

/*****************************************************************************/
static void graph_stop_worker_thread(MMAL_GRAPH_PRIVATE_T *graph)
{
   graph->stop_thread = MMAL_TRUE;
   vcos_semaphore_post(&graph->sema);
   vcos_thread_join(&graph->thread, NULL);
}

/*****************************************************************************/
MMAL_STATUS_T mmal_graph_create(MMAL_GRAPH_T **graph, unsigned int userdata_size)
{
   MMAL_GRAPH_PRIVATE_T *private;

   LOG_TRACE("graph %p, userdata_size %u", graph, userdata_size);

   /* Sanity checking */
   if (!graph)
      return MMAL_EINVAL;

   private = vcos_calloc(1, sizeof(MMAL_GRAPH_PRIVATE_T) + userdata_size, "mmal connection graph");
   if (!private)
      return MMAL_ENOMEM;
   *graph = &private->graph;
   if (userdata_size)
      (*graph)->userdata = (struct MMAL_GRAPH_USERDATA_T *)&private[1];

   if (vcos_semaphore_create(&private->sema, "mmal graph sema", 0) != VCOS_SUCCESS)
   {
      LOG_ERROR("failed to create semaphore %p", graph);
      vcos_free(private);
      return MMAL_ENOSPC;
   }

   return MMAL_SUCCESS;
}

/*****************************************************************************/
MMAL_STATUS_T mmal_graph_destroy(MMAL_GRAPH_T *graph)
{
   unsigned i;
   MMAL_GRAPH_PRIVATE_T *private = (MMAL_GRAPH_PRIVATE_T *)graph;

   if (!graph)
      return MMAL_EINVAL;

   LOG_TRACE("%p", graph);

   /* Notify client of destruction */
   if (graph->pf_destroy)
      graph->pf_destroy(graph);

   for (i = 0; i < private->connection_num; i++)
      mmal_connection_release(private->connection[i]);

   for (i = 0; i < private->component_num; i++)
      mmal_component_release(private->component[i]);

   vcos_semaphore_delete(&private->sema);

   vcos_free(graph);
   return MMAL_SUCCESS;
}

/*****************************************************************************/
MMAL_STATUS_T mmal_graph_add_component(MMAL_GRAPH_T *graph, MMAL_COMPONENT_T *component)
{
   MMAL_GRAPH_PRIVATE_T *private = (MMAL_GRAPH_PRIVATE_T *)graph;

   LOG_TRACE("graph: %p, component: %s(%p)", graph, component ? component->name: 0, component);

   if (!component)
      return MMAL_EINVAL;

   if (private->component_num >= GRAPH_CONNECTIONS_MAX)
   {
      LOG_ERROR("no space for component %s", component->name);
      return MMAL_ENOSPC;
   }

   mmal_component_acquire(component);
   private->component[private->component_num++] = component;

   return MMAL_SUCCESS;
}

/*****************************************************************************/
MMAL_STATUS_T mmal_graph_component_topology(MMAL_GRAPH_T *graph, MMAL_COMPONENT_T *component,
    MMAL_GRAPH_TOPOLOGY_T topology, int8_t *input, unsigned int input_num,
    int8_t *output, unsigned int output_num)
{
   MMAL_GRAPH_PRIVATE_T *private = (MMAL_GRAPH_PRIVATE_T *)graph;
   MMAL_PARAM_UNUSED(input); MMAL_PARAM_UNUSED(input_num);
   MMAL_PARAM_UNUSED(output); MMAL_PARAM_UNUSED(output_num);
   unsigned int i;

   LOG_TRACE("graph: %p, component: %s(%p)", graph, component ? component->name: 0, component);

   if (!component)
      return MMAL_EINVAL;

   for (i = 0; i < private->component_num; i++)
      if (component == private->component[i])
         break;

   if (i == private->component_num)
      return MMAL_EINVAL; /* Component not found */

   if (topology > MMAL_GRAPH_TOPOLOGY_STRAIGHT)
      return MMAL_ENOSYS; /* Currently not supported */

   private->topology[i] = topology;

   return MMAL_SUCCESS;
}

/*****************************************************************************/
MMAL_STATUS_T mmal_graph_add_connection(MMAL_GRAPH_T *graph, MMAL_CONNECTION_T *cx)
{
   MMAL_GRAPH_PRIVATE_T *private = (MMAL_GRAPH_PRIVATE_T *)graph;

   LOG_TRACE("graph: %p, connection: %s(%p)", graph, cx ? cx->name: 0, cx);

   if (!cx)
      return MMAL_EINVAL;

   if (private->connection_num >= GRAPH_CONNECTIONS_MAX)
   {
      LOG_ERROR("no space for connection %s", cx->name);
      return MMAL_ENOSPC;
   }

   mmal_connection_acquire(cx);
   private->connection[private->connection_num++] = cx;
   return MMAL_SUCCESS;
}

/*****************************************************************************/
MMAL_STATUS_T mmal_graph_add_port(MMAL_GRAPH_T *graph, MMAL_PORT_T *port)
{
   MMAL_GRAPH_PRIVATE_T *private = (MMAL_GRAPH_PRIVATE_T *)graph;
   MMAL_PORT_T **list;
   unsigned int *list_num;

   LOG_TRACE("graph: %p, port: %s(%p)", graph, port ? port->name: 0, port);

   if (!port)
      return MMAL_EINVAL;

   switch (port->type)
   {
   case MMAL_PORT_TYPE_INPUT:
      list = private->input;
      list_num = &private->input_num;
      break;
   case MMAL_PORT_TYPE_OUTPUT:
      list = private->output;
      list_num = &private->output_num;
      break;
   case MMAL_PORT_TYPE_CLOCK:
      list = private->clock;
      list_num = &private->clock_num;
      break;
   default:
      return MMAL_EINVAL;
   }

   if (*list_num >= GRAPH_CONNECTIONS_MAX)
   {
      LOG_ERROR("no space for port %s", port->name);
      return MMAL_ENOSPC;
   }

   list[(*list_num)++] = port;
   return MMAL_SUCCESS;
}

/*****************************************************************************/
MMAL_STATUS_T mmal_graph_new_component(MMAL_GRAPH_T *graph, const char *name,
   MMAL_COMPONENT_T **component)
{
   MMAL_GRAPH_PRIVATE_T *private = (MMAL_GRAPH_PRIVATE_T *)graph;
   MMAL_COMPONENT_T *comp;
   MMAL_STATUS_T status;

   LOG_TRACE("graph: %p, name: %s, component: %p", graph, name, component);

   if (private->component_num >= GRAPH_CONNECTIONS_MAX)
   {
      LOG_ERROR("no space for component %s", name);
      return MMAL_ENOSPC;
   }

   status = mmal_component_create(name, &comp);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("could not create component %s (%i)", name, status);
      return status;
   }

   private->component[private->component_num++] = comp;
   if (component)
   {
      mmal_component_acquire(comp);
      *component = comp;
   }

   return MMAL_SUCCESS;
}

/*****************************************************************************/
MMAL_STATUS_T mmal_graph_new_connection(MMAL_GRAPH_T *graph, MMAL_PORT_T *out, MMAL_PORT_T *in,
   uint32_t flags, MMAL_CONNECTION_T **connection)
{
   MMAL_GRAPH_PRIVATE_T *private = (MMAL_GRAPH_PRIVATE_T *)graph;
   MMAL_CONNECTION_T *cx;
   MMAL_STATUS_T status;

   if (!out || !in)
      return MMAL_EINVAL;
   if (out->type == MMAL_PORT_TYPE_CLOCK && in->type != MMAL_PORT_TYPE_CLOCK)
      return MMAL_EINVAL;
   if (out->type != MMAL_PORT_TYPE_CLOCK &&
       (out->type != MMAL_PORT_TYPE_OUTPUT || in->type != MMAL_PORT_TYPE_INPUT))
      return MMAL_EINVAL;

   LOG_TRACE("graph: %p, out: %s(%p), in: %s(%p), flags %x, connection: %p",
             graph, out->name, out, in->name, in, (int)flags, connection);

   if (private->connection_num >= GRAPH_CONNECTIONS_MAX)
   {
      LOG_ERROR("no space for connection %s/%s", out->name, in->name);
      return MMAL_ENOSPC;
   }

   status = mmal_connection_create(&cx, out, in, flags);
   if (status != MMAL_SUCCESS)
      return status;

   private->connection[private->connection_num++] = cx;
   if (connection)
   {
      mmal_connection_acquire(cx);
      *connection = cx;
   }

   return MMAL_SUCCESS;
}

/*****************************************************************************/
MMAL_STATUS_T mmal_graph_enable(MMAL_GRAPH_T *graph, MMAL_GRAPH_EVENT_CB cb, void *cb_data)
{
   MMAL_GRAPH_PRIVATE_T *private = (MMAL_GRAPH_PRIVATE_T *)graph;
   MMAL_STATUS_T status = MMAL_SUCCESS;
   unsigned int i;

   LOG_TRACE("graph: %p", graph);

   if (vcos_thread_create(&private->thread, "mmal graph thread", NULL,
                          graph_worker_thread, private) != VCOS_SUCCESS)
   {
      LOG_ERROR("failed to create worker thread %p", graph);
      return MMAL_ENOSPC;
   }

   private->event_cb = cb;
   private->event_cb_data = cb_data;

   /* Enable all control ports */
   for (i = 0; i < private->component_num; i++)
   {
      private->component[i]->control->userdata = (void *)private;
      status = mmal_port_enable(private->component[i]->control, graph_control_cb);
      if (status != MMAL_SUCCESS)
         LOG_ERROR("could not enable port %s", private->component[i]->control->name);
   }

   /* Enable all our connections */
   for (i = 0; i < private->connection_num; i++)
   {
      MMAL_CONNECTION_T *cx = private->connection[i];

      cx->callback = graph_connection_cb;
      cx->user_data = private;

      status = mmal_connection_enable(cx);
      if (status != MMAL_SUCCESS)
         goto error;
   }

   /* Trigger the worker thread to populate the output ports with empty buffers */
   vcos_semaphore_post(&private->sema);
   return status;

 error:
   graph_stop_worker_thread(private);
   return status;
}

/*****************************************************************************/
MMAL_STATUS_T mmal_graph_disable(MMAL_GRAPH_T *graph)
{
   MMAL_GRAPH_PRIVATE_T *private = (MMAL_GRAPH_PRIVATE_T *)graph;
   MMAL_STATUS_T status = MMAL_SUCCESS;
   unsigned int i;

   LOG_TRACE("graph: %p", graph);

   graph_stop_worker_thread(private);

   /* Disable all our connections */
   for (i = 0; i < private->connection_num; i++)
   {
      status = mmal_connection_disable(private->connection[i]);
      if (status != MMAL_SUCCESS)
         break;
   }

   return status;
}

/*****************************************************************************/
MMAL_STATUS_T mmal_graph_build(MMAL_GRAPH_T *graph,
   const char *name, MMAL_COMPONENT_T **component)
{
   LOG_TRACE("graph: %p, name: %s, component: %p", graph, name, component);
   return mmal_component_create_with_constructor(name, mmal_component_create_from_graph,
      (MMAL_GRAPH_PRIVATE_T *)graph, component);
}

/*****************************************************************************/
MMAL_STATUS_T mmal_graph_component_constructor(const char *name,
   MMAL_COMPONENT_T *component)
{
   LOG_TRACE("name: %s, component: %p", name, component);
   return mmal_component_create_from_graph(name, component);
}

/*****************************************************************************/
static void graph_component_control_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_COMPONENT_T *graph_component = (MMAL_COMPONENT_T *)port->userdata;
   MMAL_GRAPH_PRIVATE_T *graph_private = graph_component->priv->module;
   MMAL_STATUS_T status;

   LOG_TRACE("%s(%p),%p,%4.4s", port->name, port, buffer, (char *)&buffer->cmd);

   /* Call user defined function first */
   if (graph_private->graph.pf_control_callback)
   {
      status = graph_private->graph.pf_control_callback(&graph_private->graph,
         port, buffer);
      if (status != MMAL_ENOSYS)
         return;
   }

   /* Forward the event on the graph control port */
   mmal_port_event_send(graph_component->control, buffer);
}

/*****************************************************************************/
static void graph_component_connection_cb(MMAL_CONNECTION_T *connection)
{
   MMAL_COMPONENT_T *component = (MMAL_COMPONENT_T *)connection->user_data;
   MMAL_BUFFER_HEADER_T *buffer;

   if (connection->flags == MMAL_CONNECTION_FLAG_DIRECT &&
       (buffer = mmal_queue_get(connection->queue)) != NULL)
   {
      graph_process_buffer((MMAL_GRAPH_PRIVATE_T *)component->priv->module,
         connection, buffer);
      return;
   }

   mmal_component_action_trigger(component);
}

/*****************************************************************************/
static void graph_port_event_handler(MMAL_CONNECTION_T *connection,
   MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_STATUS_T status;

   LOG_TRACE("port: %s(%p), buffer: %p, event: %4.4s", port->name, port,
             buffer, (char *)&buffer->cmd);

   if (buffer->cmd == MMAL_EVENT_FORMAT_CHANGED && port->type == MMAL_PORT_TYPE_OUTPUT)
   {
      MMAL_EVENT_FORMAT_CHANGED_T *event = mmal_event_format_changed_get(buffer);
      if (event)
      {
         LOG_DEBUG("----------Port format changed----------");
         mmal_log_dump_port(port);
         LOG_DEBUG("-----------------to---------------------");
         mmal_log_dump_format(event->format);
         LOG_DEBUG(" buffers num (opt %i, min %i), size (opt %i, min: %i)",
                   event->buffer_num_recommended, event->buffer_num_min,
                   event->buffer_size_recommended, event->buffer_size_min);
         LOG_DEBUG("----------------------------------------");
      }

      status = mmal_connection_event_format_changed(connection, buffer);
   }

   else
      status = MMAL_SUCCESS; /* FIXME: ignore any other event for now */

   mmal_buffer_header_release(buffer);

   if (status != MMAL_SUCCESS)
      mmal_event_error_send(port->component, status);
}

/*****************************************************************************/
static void graph_process_buffer(MMAL_GRAPH_PRIVATE_T *graph_private,
   MMAL_CONNECTION_T *connection, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_STATUS_T status;

   /* Call user defined function first */
   if (graph_private->graph.pf_connection_buffer)
   {
      status = graph_private->graph.pf_connection_buffer(&graph_private->graph, connection, buffer);
      if (status != MMAL_ENOSYS)
         return;
   }

   if (buffer->cmd)
   {
      graph_port_event_handler(connection, connection->out, buffer);
      return;
   }

   status = mmal_port_send_buffer(connection->in, buffer);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("%s(%p) could not send buffer to %s(%p) (%s)",
                connection->out->name, connection->out,
                connection->in->name, connection->in,
                mmal_status_to_string(status));
      mmal_buffer_header_release(buffer);
      mmal_event_error_send(connection->out->component, status);
   }
}

/*****************************************************************************/
static MMAL_BOOL_T graph_do_processing(MMAL_GRAPH_PRIVATE_T *graph_private)
{
   MMAL_BUFFER_HEADER_T *buffer;
   MMAL_BOOL_T run_again = 0;
   MMAL_STATUS_T status;
   unsigned int i, j;

   /* Process all the empty buffers first */
   for (i = 0, j = graph_private->connection_current;
        i < graph_private->connection_num; i++, j++)
   {
      MMAL_CONNECTION_T *connection =
         graph_private->connection[j%graph_private->connection_num];

      if ((connection->flags & MMAL_CONNECTION_FLAG_TUNNELLING) ||
          !connection->pool)
         continue; /* Nothing else to do in tunnelling mode */

      /* Send empty buffers to the output port of the connection */
      while ((buffer = mmal_queue_get(connection->pool->queue)) != NULL)
      {
         run_again = 1;

         status = mmal_port_send_buffer(connection->out, buffer);
         if (status != MMAL_SUCCESS)
         {
            if (connection->out->is_enabled)
               LOG_ERROR("mmal_port_send_buffer failed (%i)", status);
            mmal_queue_put_back(connection->pool->queue, buffer);
            run_again = 0;
            break;
         }
      }
   }

   /* Loop through all the connections */
   for (i = 0, j = graph_private->connection_current++;
        i < graph_private->connection_num; i++, j++)
   {
      MMAL_CONNECTION_T *connection =
         graph_private->connection[j%graph_private->connection_num];
      int64_t duration = vcos_getmicrosecs64();

      if (connection->flags & MMAL_CONNECTION_FLAG_TUNNELLING)
         continue; /* Nothing else to do in tunnelling mode */
      if (connection->flags & MMAL_CONNECTION_FLAG_DIRECT)
         continue; /* Nothing else to do in direct mode */

      /* Send any queued buffer to the next component.
       * We also make sure no connection can starve the others by
       * having a timeout. */
      while (vcos_getmicrosecs64() - duration < PROCESSING_TIME_MAX &&
             (buffer = mmal_queue_get(connection->queue)) != NULL)
      {
         run_again = 1;

         graph_process_buffer(graph_private, connection, buffer);
      }
   }

   return run_again;
}

/*****************************************************************************/
static void graph_do_processing_loop(MMAL_COMPONENT_T *component)
{
   while (graph_do_processing((MMAL_GRAPH_PRIVATE_T *)component->priv->module));
}

/*****************************************************************************/
static MMAL_PORT_T *find_port_from_graph(MMAL_GRAPH_PRIVATE_T *graph, MMAL_PORT_T *port)
{
   MMAL_PORT_T **list;
   unsigned int *list_num;

   switch (port->type)
   {
   case MMAL_PORT_TYPE_INPUT:
      list = graph->input;
      list_num = &graph->input_num;
      break;
   case MMAL_PORT_TYPE_OUTPUT:
      list = graph->output;
      list_num = &graph->output_num;
      break;
   case MMAL_PORT_TYPE_CLOCK:
      list = graph->clock;
      list_num = &graph->clock_num;
      break;
   default:
      return 0;
   }

   if (port->index > *list_num)
      return 0;

   return list[port->index];
}

static MMAL_PORT_T *find_port_to_graph(MMAL_GRAPH_PRIVATE_T *graph, MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = graph->graph_component;
   MMAL_PORT_T **list, **component_list;
   unsigned int i, *list_num;

   switch (port->type)
   {
   case MMAL_PORT_TYPE_INPUT:
      list = graph->input;
      list_num = &graph->input_num;
      component_list = component->input;
      break;
   case MMAL_PORT_TYPE_OUTPUT:
      list = graph->output;
      list_num = &graph->output_num;
      component_list = component->output;
      break;
   case MMAL_PORT_TYPE_CLOCK:
      list = graph->clock;
      list_num = &graph->clock_num;
      component_list = component->clock;
      break;
   default:
      return 0;
   }

   for (i = 0; i < *list_num; i++)
      if (list[i] == port)
         break;

   if (i == *list_num)
      return 0;
   return component_list[i];
}

static MMAL_STATUS_T graph_port_update(MMAL_GRAPH_PRIVATE_T *graph,
   MMAL_PORT_T *graph_port, MMAL_BOOL_T init)
{
   MMAL_STATUS_T status;
   MMAL_PORT_T *port;

   port = find_port_from_graph(graph, graph_port);
   if (!port)
   {
      LOG_ERROR("could not find matching port for %p", graph_port);
      return MMAL_EINVAL;
   }

   status = mmal_format_full_copy(graph_port->format, port->format);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("format copy failed on port %s", port->name);
      return status;
   }

   graph_port->buffer_num_min = port->buffer_num_min;
   graph_port->buffer_num_recommended = port->buffer_num_recommended;
   graph_port->buffer_size_min = port->buffer_size_min;
   graph_port->buffer_size_recommended = port->buffer_size_recommended;
   graph_port->buffer_alignment_min = port->buffer_alignment_min;
   graph_port->capabilities = port->capabilities;
   if (init)
   {
      graph_port->buffer_num = port->buffer_num;
      graph_port->buffer_size = port->buffer_size;
   }
   return MMAL_SUCCESS;
}

static MMAL_STATUS_T graph_port_update_requirements(MMAL_GRAPH_PRIVATE_T *graph,
   MMAL_PORT_T *graph_port)
{
   MMAL_PORT_T *port;

   port = find_port_from_graph(graph, graph_port);
   if (!port)
   {
      LOG_ERROR("could not find matching port for %p", graph_port);
      return MMAL_EINVAL;
   }

   graph_port->buffer_num_min = port->buffer_num_min;
   graph_port->buffer_num_recommended = port->buffer_num_recommended;
   graph_port->buffer_size_min = port->buffer_size_min;
   graph_port->buffer_size_recommended = port->buffer_size_recommended;
   graph_port->buffer_alignment_min = port->buffer_alignment_min;
   return MMAL_SUCCESS;
}

/** Destroy a previously created component */
static MMAL_STATUS_T graph_component_destroy(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *graph = component->priv->module;

   /* Notify client of destruction */
   if (graph->graph.pf_destroy)
      graph->graph.pf_destroy(&graph->graph);
   graph->graph.pf_destroy = NULL;

   if (component->input_num)
      mmal_ports_free(component->input, component->input_num);

   if (component->output_num)
      mmal_ports_free(component->output, component->output_num);

   if (component->clock_num)
      mmal_ports_clock_free(component->clock, component->clock_num);

   /* coverity[address_free] Freeing the first item in the structure is safe */
   mmal_graph_destroy(&graph->graph);
   return MMAL_SUCCESS;
}

/** Enable processing on a component */
static MMAL_STATUS_T graph_component_enable(MMAL_COMPONENT_T *component)
{
   MMAL_GRAPH_PRIVATE_T *graph_private = component->priv->module;
   MMAL_STATUS_T status = MMAL_ENOSYS;

   /* Call user defined function first */
   if (graph_private->graph.pf_graph_enable)
      status = graph_private->graph.pf_graph_enable(&graph_private->graph, MMAL_TRUE);

   return status;
}

/** Disable processing on a component */
static MMAL_STATUS_T graph_component_disable(MMAL_COMPONENT_T *component)
{
   MMAL_GRAPH_PRIVATE_T *graph_private = component->priv->module;
   MMAL_STATUS_T status = MMAL_ENOSYS;

   /* Call user defined function first */
   if (graph_private->graph.pf_graph_enable)
      status = graph_private->graph.pf_graph_enable(&graph_private->graph, MMAL_FALSE);

   return status;
}

/** Callback given to mmal_port_enable() */
static void graph_port_enable_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_GRAPH_PRIVATE_T *graph_private = (MMAL_GRAPH_PRIVATE_T *)port->userdata;
   MMAL_PORT_T *graph_port;
   MMAL_STATUS_T status;

   graph_port = find_port_to_graph(graph_private, port);
   if (!graph_port)
   {
      vcos_assert(0);
      mmal_buffer_header_release(buffer);
      return;
   }

   /* Call user defined function first */
   if (graph_private->graph.pf_return_buffer)
   {
      status = graph_private->graph.pf_return_buffer(&graph_private->graph, graph_port, buffer);
      if (status != MMAL_ENOSYS)
         return;
   }

   /* Forward the callback */
   if (buffer->cmd)
      mmal_port_event_send(graph_port, buffer);
   else
      mmal_port_buffer_header_callback(graph_port, buffer);
}

/** Check whether 2 ports of a component are linked */
static MMAL_BOOL_T graph_component_topology_ports_linked(MMAL_GRAPH_PRIVATE_T *graph,
   MMAL_PORT_T *port1, MMAL_PORT_T *port2)
{
   MMAL_COMPONENT_T *component = port1->component;
   unsigned int i;

   for (i = 0; i < graph->component_num; i++)
      if (component == graph->component[i])
         break;

   if (i == graph->component_num)
      return MMAL_FALSE; /* Component not found */

   if (graph->topology[i] == MMAL_GRAPH_TOPOLOGY_STRAIGHT)
      return port1->index == port2->index;

   return MMAL_TRUE;
}

/** Propagate a port enable */
static MMAL_STATUS_T graph_port_state_propagate(MMAL_GRAPH_PRIVATE_T *graph,
   MMAL_PORT_T *port, MMAL_BOOL_T enable)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_STATUS_T status = MMAL_SUCCESS;
   MMAL_PORT_TYPE_T type = port->type;
   unsigned int i, j;

   LOG_TRACE("graph: %p, port %s(%p)", graph, port->name, port);

   if (port->type == MMAL_PORT_TYPE_OUTPUT)
      type = MMAL_PORT_TYPE_INPUT;
   if (port->type == MMAL_PORT_TYPE_INPUT)
      type = MMAL_PORT_TYPE_OUTPUT;

   /* Loop through all the output ports of the component and if they are not enabled and
    * match one of the connections we maintain, then we need to propagate the port enable. */
   for (i = 0; i < component->port_num; i++)
   {
      if (component->port[i]->type != type)
         continue;

      if ((component->port[i]->is_enabled && enable) ||
          (!component->port[i]->is_enabled && !enable))
         continue;

      /* Find the matching connection */
      for (j = 0; j < graph->connection_num; j++)
         if (graph->connection[j]->out == component->port[i] ||
             graph->connection[j]->in == component->port[i])
            break;

      if (j == graph->connection_num)
         continue; /* No match */

      if (!graph_component_topology_ports_linked(graph, port, component->port[i]))
            continue; /* Ports are independent */

      if (enable)
      {
         status = mmal_connection_enable(graph->connection[j]);
         if (status != MMAL_SUCCESS)
            break;

         mmal_log_dump_port(graph->connection[j]->out);
         mmal_log_dump_port(graph->connection[j]->in);
      }

      status = graph_port_state_propagate(graph, graph->connection[j]->in == component->port[i] ?
         graph->connection[j]->out : graph->connection[j]->in, enable);
      if (status != MMAL_SUCCESS)
         break;

      if (!enable)
      {
         status = mmal_connection_disable(graph->connection[j]);
         if (status != MMAL_SUCCESS)
            break;
      }
   }

   return status;
}

/** Enable processing on a port */
static MMAL_STATUS_T graph_port_enable(MMAL_PORT_T *graph_port, MMAL_PORT_BH_CB_T cb)
{
   MMAL_GRAPH_PRIVATE_T *graph_private = graph_port->component->priv->module;
   MMAL_PORT_T *port;
   MMAL_STATUS_T status;
   MMAL_PARAM_UNUSED(cb);

   port = find_port_from_graph(graph_private, graph_port);
   if (!port)
      return MMAL_EINVAL;

   /* Update the buffer requirements */
   port->buffer_num = graph_port->buffer_num;
   port->buffer_size = graph_port->buffer_size;

   /* Call user defined function first */
   if (graph_private->graph.pf_enable)
   {
      status = graph_private->graph.pf_enable(&graph_private->graph, graph_port);
      if (status != MMAL_ENOSYS)
         return status;
   }

   /* We'll intercept the callback */
   port->userdata = (void *)graph_private;
   status = mmal_port_enable(port, graph_port_enable_cb);
   if (status != MMAL_SUCCESS)
      return status;

   /* We need to enable all the connected connections */
   status = graph_port_state_propagate(graph_private, port, 1);

   mmal_component_action_trigger(graph_port->component);
   return status;
}

/** Disable processing on a port */
static MMAL_STATUS_T graph_port_disable(MMAL_PORT_T *graph_port)
{
   MMAL_GRAPH_PRIVATE_T *graph_private = graph_port->component->priv->module;
   MMAL_STATUS_T status;
   MMAL_PORT_T *port;

   port = find_port_from_graph(graph_port->component->priv->module, graph_port);
   if (!port)
      return MMAL_EINVAL;

   /* Call user defined function first */
   if (graph_private->graph.pf_disable)
   {
      status = graph_private->graph.pf_disable(&graph_private->graph, graph_port);
      if (status != MMAL_ENOSYS)
         return status;
   }

   /* We need to disable all the connected connections.
    * Since disable does an implicit flush, we only want to do that if
    * we're acting on an input port or we risk discarding buffers along
    * the way. */
   if (!graph_private->input_num || port->type == MMAL_PORT_TYPE_INPUT)
   {
      MMAL_STATUS_T status = graph_port_state_propagate(graph_private, port, 0);
      if (status != MMAL_SUCCESS)
         return status;
   }

   /* Forward the call */
   return mmal_port_disable(port);
}

/** Propagate a port flush */
static MMAL_STATUS_T graph_port_flush_propagate(MMAL_GRAPH_PRIVATE_T *graph,
   MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_STATUS_T status;
   unsigned int i, j;

   LOG_TRACE("graph: %p, port %s(%p)", graph, port->name, port);

   status = mmal_port_flush(port);
   if (status != MMAL_SUCCESS)
      return status;

   if (port->type == MMAL_PORT_TYPE_OUTPUT)
      return MMAL_SUCCESS;

   /* Loop through all the output ports of the component and if they match one
    * of the connections we maintain, then we need to propagate the flush. */
   for (i = 0; i < component->port_num; i++)
   {
      if (component->port[i]->type != MMAL_PORT_TYPE_OUTPUT)
         continue;
      if (!component->port[i]->is_enabled)
         continue;

      /* Find the matching connection */
      for (j = 0; j < graph->connection_num; j++)
         if (graph->connection[j]->out == component->port[i])
            break;

      if (j == graph->connection_num)
         continue; /* No match */

      if (!graph_component_topology_ports_linked(graph, port, component->port[i]))
         continue; /* Ports are independent */

      /* Flush any buffer waiting in the connection queue */
      if (graph->connection[j]->queue)
      {
         MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(graph->connection[j]->queue);
         while(buffer)
         {
            mmal_buffer_header_release(buffer);
            buffer = mmal_queue_get(graph->connection[j]->queue);
         }
      }

      status = graph_port_flush_propagate(graph, graph->connection[j]->in);
      if (status != MMAL_SUCCESS)
         break;
   }

   return status;
}

/** Flush a port */
static MMAL_STATUS_T graph_port_flush(MMAL_PORT_T *graph_port)
{
   MMAL_GRAPH_PRIVATE_T *graph_private = graph_port->component->priv->module;
   MMAL_STATUS_T status;
   MMAL_PORT_T *port;

   port = find_port_from_graph(graph_private, graph_port);
   if (!port)
      return MMAL_EINVAL;

   /* Call user defined function first */
   if (graph_private->graph.pf_flush)
   {
      status = graph_private->graph.pf_flush(&graph_private->graph, graph_port);
      if (status != MMAL_ENOSYS)
         return status;
   }

   /* Forward the call */
   return graph_port_flush_propagate(graph_private, port);
}

/** Send a buffer header to a port */
static MMAL_STATUS_T graph_port_send(MMAL_PORT_T *graph_port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_GRAPH_PRIVATE_T *graph_private = graph_port->component->priv->module;
   MMAL_STATUS_T status;
   MMAL_PORT_T *port;

   port = find_port_from_graph(graph_port->component->priv->module, graph_port);
   if (!port)
      return MMAL_EINVAL;

   /* Call user defined function first */
   if (graph_private->graph.pf_send_buffer)
   {
      status = graph_private->graph.pf_send_buffer(&graph_private->graph, graph_port, buffer);
      if (status != MMAL_ENOSYS)
         return status;
   }

   /* Forward the call */
   return mmal_port_send_buffer(port, buffer);
}

/** Propagate a format change */
static MMAL_STATUS_T graph_port_format_commit_propagate(MMAL_GRAPH_PRIVATE_T *graph,
   MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_STATUS_T status = MMAL_SUCCESS;
   unsigned int i, j;

   LOG_TRACE("graph: %p, port %s(%p)", graph, port->name, port);

   if (port->type == MMAL_PORT_TYPE_OUTPUT || port->type == MMAL_PORT_TYPE_CLOCK)
      return MMAL_SUCCESS; /* Nothing to do */

   /* Loop through all the output ports of the component and if they are not enabled and
    * match one of the connections we maintain, then we need to propagate the format change. */
   for (i = 0; i < component->output_num; i++)
   {
      MMAL_PORT_T *in, *out;

      if (component->output[i]->is_enabled)
         continue;

      /* Find the matching connection */
      for (j = 0; j < graph->connection_num; j++)
         if (graph->connection[j]->out == component->output[i])
            break;

      if (j == graph->connection_num)
         continue; /* No match */

      if (!graph_component_topology_ports_linked(graph, port, component->output[i]))
         continue; /* Ports are independent */

      in = graph->connection[j]->in;
      out = graph->connection[j]->out;

      /* Apply the format to the input port */
      status = mmal_format_full_copy(in->format, out->format);
      if (status != MMAL_SUCCESS)
         break;
      status = mmal_port_format_commit(in);
      if (status != MMAL_SUCCESS)
         break;

      mmal_log_dump_port(out);
      mmal_log_dump_port(in);

      status = graph_port_format_commit_propagate(graph, in);
      if (status != MMAL_SUCCESS)
         break;
   }

   return status;
}

/** Set format on a port */
static MMAL_STATUS_T graph_port_format_commit(MMAL_PORT_T *graph_port)
{
   MMAL_GRAPH_PRIVATE_T *graph_private = graph_port->component->priv->module;
   MMAL_STATUS_T status;
   MMAL_PORT_T *port;
   unsigned int i;

   /* Call user defined function first */
   if (graph_private->graph.pf_format_commit)
   {
      status = graph_private->graph.pf_format_commit(&graph_private->graph, graph_port);
      if (status == MMAL_SUCCESS)
         goto end;
      if (status != MMAL_ENOSYS)
         return status;
   }

   port = find_port_from_graph(graph_private, graph_port);
   if (!port)
      return MMAL_EINVAL;

   /* Update actual port */
   status = mmal_format_full_copy(port->format, graph_port->format);
   if (status != MMAL_SUCCESS)
      return status;
   port->buffer_num = graph_port->buffer_num;
   port->buffer_size = graph_port->buffer_size;

   /* Forward the call */
   status = mmal_port_format_commit(port);
   if (status != MMAL_SUCCESS)
      return status;

   /* Propagate format changes to the connections */
   status = graph_port_format_commit_propagate(graph_private, port);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("couldn't propagate format commit of port %s(%p)", port->name, port);
      return status;
   }

 end:
   /* Read the values back */
   status = graph_port_update(graph_private, graph_port, MMAL_FALSE);
   if (status != MMAL_SUCCESS)
      return status;

   /* Get the settings for the output ports in case they have changed */
   if (graph_port->type == MMAL_PORT_TYPE_INPUT)
   {
      for (i = 0; i < graph_private->output_num; i++)
      {
         status = graph_port_update(graph_private, graph_port->component->output[i], MMAL_FALSE);
         if (status != MMAL_SUCCESS)
            return status;
      }
   }

   return MMAL_SUCCESS;
}

static MMAL_STATUS_T graph_port_control_parameter_get(MMAL_PORT_T *graph_port,
   MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_GRAPH_PRIVATE_T *graph_private = graph_port->component->priv->module;
   MMAL_STATUS_T status = MMAL_ENOSYS;
   unsigned int i;

   /* Call user defined function first */
   if (graph_private->graph.pf_parameter_get)
   {
      status = graph_private->graph.pf_parameter_get(&graph_private->graph, graph_port, param);
      if (status != MMAL_ENOSYS)
         return status;
   }

   /* By default we do a get parameter on each component until one succeeds */
   for (i = 0; i < graph_private->component_num && status != MMAL_SUCCESS; i++)
      status = mmal_port_parameter_get(graph_private->component[i]->control, param);

   return status;
}

static MMAL_STATUS_T graph_port_parameter_get(MMAL_PORT_T *graph_port,
   MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_GRAPH_PRIVATE_T *graph_private = graph_port->component->priv->module;
   MMAL_STATUS_T status;
   MMAL_PORT_T *port;

   /* Call user defined function first */
   if (graph_private->graph.pf_parameter_get)
   {
      status = graph_private->graph.pf_parameter_get(&graph_private->graph, graph_port, param);
      if (status != MMAL_ENOSYS)
         return status;
   }

   port = find_port_from_graph(graph_private, graph_port);
   if (!port)
      return MMAL_EINVAL;

   /* Forward the call */
   return mmal_port_parameter_get(port, param);
}

static MMAL_STATUS_T graph_port_control_parameter_set(MMAL_PORT_T *graph_port,
   const MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_GRAPH_PRIVATE_T *graph_private = graph_port->component->priv->module;
   MMAL_STATUS_T status = MMAL_ENOSYS;
   unsigned int i;

   /* Call user defined function first */
   if (graph_private->graph.pf_parameter_set)
   {
      status = graph_private->graph.pf_parameter_set(&graph_private->graph, graph_port, param);
      if (status != MMAL_ENOSYS)
         return status;
   }

   /* By default we do a set parameter on each component until one succeeds */
   for (i = 0; i < graph_private->component_num && status != MMAL_SUCCESS; i++)
      status = mmal_port_parameter_set(graph_private->component[i]->control, param);

   return status;
}

static MMAL_STATUS_T graph_port_parameter_set(MMAL_PORT_T *graph_port,
   const MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_GRAPH_PRIVATE_T *graph_private = graph_port->component->priv->module;
   MMAL_STATUS_T status;
   MMAL_PORT_T *port;

   /* Call user defined function first */
   if (graph_private->graph.pf_parameter_set)
   {
      status = graph_private->graph.pf_parameter_set(&graph_private->graph, graph_port, param);
      if (status != MMAL_ENOSYS)
         return status;
   }

   port = find_port_from_graph(graph_private, graph_port);
   if (!port)
      return MMAL_EINVAL;

   /* Forward the call */
   status = mmal_port_parameter_set(port, param);
   if (status != MMAL_SUCCESS)
      goto end;

   if (param->id == MMAL_PARAMETER_BUFFER_REQUIREMENTS)
   {
      /* This might have changed the buffer requirements of other ports so fetch them all */
      MMAL_COMPONENT_T *component = graph_port->component;
      unsigned int i;
      for (i = 0; status == MMAL_SUCCESS && i < component->input_num; i++)
         status = graph_port_update_requirements(graph_private, component->input[i]);
      for (i = 0; status == MMAL_SUCCESS && i < component->output_num; i++)
         status = graph_port_update_requirements(graph_private, component->output[i]);
   }

 end:
   return status;
}

static MMAL_STATUS_T graph_port_connect(MMAL_PORT_T *graph_port, MMAL_PORT_T *other_port)
{
   MMAL_PORT_T *port;

   LOG_TRACE("%s(%p) %s(%p)", graph_port->name, graph_port, other_port->name, other_port);

   port = find_port_from_graph(graph_port->component->priv->module, graph_port);
   if (!port)
      return MMAL_EINVAL;

   /* Forward the call */
   return other_port ? mmal_port_connect(port, other_port) : mmal_port_disconnect(port);
}

static uint8_t *graph_port_payload_alloc(MMAL_PORT_T *graph_port, uint32_t payload_size)
{
   MMAL_GRAPH_PRIVATE_T *graph_private = graph_port->component->priv->module;
   MMAL_STATUS_T status;
   MMAL_PORT_T *port;
   uint8_t *payload;

   port = find_port_from_graph(graph_port->component->priv->module, graph_port);
   if (!port)
      return 0;

   /* Call user defined function first */
   if (graph_private->graph.pf_payload_alloc)
   {
      status = graph_private->graph.pf_payload_alloc(&graph_private->graph, graph_port,
         payload_size, &payload);
      if (status != MMAL_ENOSYS)
         return status == MMAL_SUCCESS ? payload : NULL;
   }

   /* Forward the call */
   return mmal_port_payload_alloc(port, payload_size);
}

static void graph_port_payload_free(MMAL_PORT_T *graph_port, uint8_t *payload)
{
   MMAL_GRAPH_PRIVATE_T *graph_private = graph_port->component->priv->module;
   MMAL_STATUS_T status;
   MMAL_PORT_T *port;

   port = find_port_from_graph(graph_port->component->priv->module, graph_port);
   if (!port)
      return;

   /* Call user defined function first */
   if (graph_private->graph.pf_payload_free)
   {
      status = graph_private->graph.pf_payload_free(&graph_private->graph, graph_port, payload);
      if (status == MMAL_SUCCESS)
         return;
   }

   /* Forward the call */
   mmal_port_payload_free(port, payload);
}

/** Create an instance of a component  */
static MMAL_STATUS_T mmal_component_create_from_graph(const char *name, MMAL_COMPONENT_T *component)
{
   MMAL_STATUS_T status = MMAL_ENOMEM;
   /* Our context is already allocated and available */
   MMAL_GRAPH_PRIVATE_T *graph = component->priv->module;
   unsigned int i;
   MMAL_PARAM_UNUSED(name);

   component->control->priv->pf_parameter_get = graph_port_control_parameter_get;
   component->control->priv->pf_parameter_set = graph_port_control_parameter_set;

   /* Allocate the ports for this component */
   if(graph->input_num)
   {
      component->input = mmal_ports_alloc(component, graph->input_num, MMAL_PORT_TYPE_INPUT, 0);
      if(!component->input)
         goto error;
   }
   component->input_num = graph->input_num;
   for(i = 0; i < component->input_num; i++)
   {
      component->input[i]->priv->pf_enable = graph_port_enable;
      component->input[i]->priv->pf_disable = graph_port_disable;
      component->input[i]->priv->pf_flush = graph_port_flush;
      component->input[i]->priv->pf_send = graph_port_send;
      component->input[i]->priv->pf_set_format = graph_port_format_commit;
      component->input[i]->priv->pf_parameter_get = graph_port_parameter_get;
      component->input[i]->priv->pf_parameter_set = graph_port_parameter_set;
      if (graph->input[i]->priv->pf_connect && 0 /* FIXME: disabled for now */)
         component->input[i]->priv->pf_connect = graph_port_connect;
      component->input[i]->priv->pf_payload_alloc = graph_port_payload_alloc;
      component->input[i]->priv->pf_payload_free = graph_port_payload_free;

      /* Mirror the port values */
      status = graph_port_update(graph, component->input[i], MMAL_TRUE);
      if (status != MMAL_SUCCESS)
         goto error;
   }
   if(graph->output_num)
   {
      component->output = mmal_ports_alloc(component, graph->output_num, MMAL_PORT_TYPE_OUTPUT, 0);
      if(!component->output)
         goto error;
   }
   component->output_num = graph->output_num;
   for(i = 0; i < component->output_num; i++)
   {
      component->output[i]->priv->pf_enable = graph_port_enable;
      component->output[i]->priv->pf_disable = graph_port_disable;
      component->output[i]->priv->pf_flush = graph_port_flush;
      component->output[i]->priv->pf_send = graph_port_send;
      component->output[i]->priv->pf_set_format = graph_port_format_commit;
      component->output[i]->priv->pf_parameter_get = graph_port_parameter_get;
      component->output[i]->priv->pf_parameter_set = graph_port_parameter_set;
      if (graph->output[i]->priv->pf_connect && 0 /* FIXME: disabled for now */)
         component->output[i]->priv->pf_connect = graph_port_connect;
      component->output[i]->priv->pf_payload_alloc = graph_port_payload_alloc;
      component->output[i]->priv->pf_payload_free = graph_port_payload_free;

      /* Mirror the port values */
      status = graph_port_update(graph, component->output[i], MMAL_TRUE);
      if (status != MMAL_SUCCESS)
         goto error;
   }
   if(graph->clock_num)
   {
      component->clock = mmal_ports_clock_alloc(component, graph->clock_num, 0, NULL);
      if(!component->clock)
      {
         status = MMAL_ENOMEM;
         goto error;
      }
   }
   component->clock_num = graph->clock_num;
   for(i = 0; i < component->clock_num; i++)
   {
      component->clock[i]->priv->pf_enable = graph_port_enable;
      component->clock[i]->priv->pf_disable = graph_port_disable;
      component->clock[i]->priv->pf_flush = graph_port_flush;
      component->clock[i]->priv->pf_send = graph_port_send;
      component->clock[i]->priv->pf_set_format = graph_port_format_commit;
      component->clock[i]->priv->pf_parameter_get = graph_port_parameter_get;
      component->clock[i]->priv->pf_parameter_set = graph_port_parameter_set;
      component->clock[i]->priv->pf_connect = NULL; /* FIXME: disabled for now */
      component->clock[i]->priv->pf_payload_alloc = graph_port_payload_alloc;
      component->clock[i]->priv->pf_payload_free = graph_port_payload_free;

      /* Mirror the port values */
      status = graph_port_update(graph, component->clock[i], MMAL_TRUE);
      if (status != MMAL_SUCCESS)
         goto error;
   }

   status = mmal_component_action_register(component, graph_do_processing_loop);
   if (status != MMAL_SUCCESS)
      goto error;

#if 1 // FIXME
   /* Set our connection callback */
   for (i = 0; i < graph->connection_num; i++)
   {
      graph->connection[i]->callback = graph_component_connection_cb;
      graph->connection[i]->user_data = (void *)component;
   }
#endif

   component->priv->pf_destroy = graph_component_destroy;
   component->priv->pf_enable = graph_component_enable;
   component->priv->pf_disable = graph_component_disable;
   graph->graph_component = component;

   /* Enable all the control ports */
   for (i = 0; i < graph->component_num; i++)
   {
      graph->component[i]->control->userdata = (void *)component;
      status = mmal_port_enable(graph->component[i]->control, graph_component_control_cb);
      if (status != MMAL_SUCCESS)
         LOG_ERROR("could not enable port %s", component->control->name);
   }

   return MMAL_SUCCESS;

 error:
   graph_component_destroy(component);
   return status;
}

MMAL_PORT_T *mmal_graph_find_port(MMAL_GRAPH_T *graph,
                                  const char *name,
                                  MMAL_PORT_TYPE_T type,
                                  unsigned index)
{
   unsigned i;
   MMAL_GRAPH_PRIVATE_T *private = (MMAL_GRAPH_PRIVATE_T *)graph;
   for (i=0; i<private->component_num; i++)
   {
      MMAL_COMPONENT_T *comp = private->component[i];
      if (vcos_strcasecmp(name, comp->name) == 0)
      {
         unsigned num;
         MMAL_PORT_T **ports;
         if (type == MMAL_PORT_TYPE_INPUT) {
            num = comp->input_num;
            ports = comp->input;
         }
         else if (type == MMAL_PORT_TYPE_OUTPUT) {
            num = comp->output_num;
            ports = comp->output;
         }
         else if (type == MMAL_PORT_TYPE_CLOCK) {
            num = comp->clock_num;
            ports = comp->clock;
         }
         else if (type == MMAL_PORT_TYPE_CONTROL) {
            num = 1;
            ports = &comp->control;
         }
         else {
            vcos_assert(0);
            return NULL;
         }
         if (index < num)
         {
            /* coverity[ptr_arith] num is 1 at this point */
            return ports[index];
         }
      }
   }
   LOG_INFO("port %s:%d not found", name, index);
   return NULL;
}
