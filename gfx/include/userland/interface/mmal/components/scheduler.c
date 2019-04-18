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
#include "mmal_logging.h"
#include "core/mmal_port_private.h"
#include "core/mmal_component_private.h"
#include "core/mmal_clock_private.h"

#define SCHEDULER_CLOCK_PORTS_NUM  1
#define SCHEDULER_INPUT_PORTS_NUM  1
#define SCHEDULER_OUTPUT_PORTS_NUM 1

#define SCHEDULER_REQUEST_SLOTS 16

/*****************************************************************************/
typedef struct MMAL_COMPONENT_MODULE_T
{
   MMAL_STATUS_T status;     /**< current status of the component */
} MMAL_COMPONENT_MODULE_T;

typedef struct MMAL_PORT_MODULE_T
{
   MMAL_QUEUE_T *queue;      /**< queue for the buffers sent to the port */
   int64_t last_ts;          /***< Last timestamp seen on the input port */
} MMAL_PORT_MODULE_T;

/*****************************************************************************/
/** Process an event buffer */
static MMAL_STATUS_T scheduler_event_process(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_STATUS_T status = MMAL_EINVAL;

   if (buffer->cmd == MMAL_EVENT_FORMAT_CHANGED)
   {
      MMAL_EVENT_FORMAT_CHANGED_T *event =
         mmal_event_format_changed_get(buffer);
      if (!event)
         goto end;

      status = mmal_format_full_copy(port->format, event->format);
      if (status == MMAL_SUCCESS)
         status = mmal_port_format_commit(port);
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("format commit failed on port %s (%i)",
                   port->name, status);
         goto end;
      }

      status = MMAL_SUCCESS;
   }
   /* Forward any other event as is to the next component */
   else
   {
      LOG_DEBUG("forwarding unknown event %4.4s", (char *)&buffer->cmd);
      status = mmal_event_forward(buffer, port->component->output[port->index]);
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("unable to forward event %4.4s", (char *)&buffer->cmd);
         goto end;
      }
   }

 end:
   buffer->length = 0;
   mmal_port_buffer_header_callback(port, buffer);
   return status;
}

/** Invoked when a clock request has been serviced */
static void scheduler_component_clock_port_request_cb(MMAL_PORT_T *port, int64_t media_time, void *cb_data)
{
   MMAL_COMPONENT_T *component = port->component;;
   MMAL_PORT_T *port_in = component->input[0];
   MMAL_PORT_T *port_out = component->output[0];
   MMAL_BUFFER_HEADER_T *buffer = (MMAL_BUFFER_HEADER_T*)cb_data;

   LOG_TRACE("media-time %"PRIi64" pts %"PRIi64" delta %"PRIi64,
         media_time, buffer->pts, media_time - buffer->pts);

   if (buffer->cmd)
      scheduler_event_process(port_in, buffer);
   else
      /* Forward the buffer to the next component */
      mmal_port_buffer_header_callback(port_out, buffer);
}

/** Process buffers on the input and output ports */
static MMAL_BOOL_T scheduler_component_process_buffers(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_PORT_T *port_in = component->input[0];
   MMAL_PORT_T *port_out = component->output[0];
   MMAL_QUEUE_T *queue_in = port_in->priv->module->queue;
   MMAL_QUEUE_T *queue_out = port_out->priv->module->queue;
   MMAL_BUFFER_HEADER_T *in, *out;
   MMAL_STATUS_T cb_status = MMAL_EINVAL;

   /* Don't do anything if we've already seen an error */
   if (module->status != MMAL_SUCCESS)
   {
      LOG_ERROR("module failure");
      return MMAL_FALSE;
   }

   in = mmal_queue_get(queue_in);

   /* Special case for dealing with event buffers */
   if (in && in->cmd)
   {
      /* We normally schedule cmds so they come out in the right order,
       * except when we don't know when to schedule them, which will only
       * happen at the start of the stream.
       * The fudge factor added to the last timestamp here is because the
       * cmd really applies to the next buffer so we want to make sure
       * we leave enough time to the next component to process the previous
       * buffer before forwarding the event. */
      in->pts = port_in->priv->module->last_ts + 1000;
      if (in->pts != MMAL_TIME_UNKNOWN)
         cb_status = mmal_port_clock_request_add(component->clock[0],
            in->pts, scheduler_component_clock_port_request_cb, in);
      if (cb_status != MMAL_SUCCESS)
      {
         if (in->pts != MMAL_TIME_UNKNOWN)
            LOG_ERROR("failed to add request for cmd");
         scheduler_event_process(port_in, in);
      }
      return MMAL_TRUE;
   }

   /* Need both an input and output buffer to be able to go any further */
   out = mmal_queue_get(queue_out);
   if (!in || !out)
      goto end;

   if (port_out->capabilities & MMAL_PORT_CAPABILITY_PASSTHROUGH)
   {
      /* Just need to keep a reference to the input buffer */
      module->status = mmal_buffer_header_replicate(out, in);
   }
   else
   {
      /* Make a full copy of the input payload */
      if (out->alloc_size < in->length)
      {
         LOG_ERROR("output buffer too small");

         module->status = MMAL_EINVAL;
         if (mmal_event_error_send(component, module->status) != MMAL_SUCCESS)
            LOG_ERROR("unable to send an error event buffer");
         goto end;
      }
      mmal_buffer_header_mem_lock(out);
      mmal_buffer_header_mem_lock(in);
      memcpy(out->data, in->data + in->offset, in->length);
      mmal_buffer_header_mem_unlock(in);
      mmal_buffer_header_mem_unlock(out);
      out->length     = in->length;
      out->offset     = 0;
      out->flags      = in->flags;
      out->pts        = in->pts;
      out->dts        = in->dts;
      *out->type      = *in->type;
   }

   /* Finished with the input buffer, so return it */
   in->length = 0;
   mmal_port_buffer_header_callback(port_in, in);
   in = 0;

   if (module->status != MMAL_SUCCESS)
   {
      LOG_ERROR("failed to replicate buffer");
      goto end;
   }

   /* Request a clock callback when media-time >= pts */
   LOG_TRACE("requesting callback at %"PRIi64,out->pts);
   port_in->priv->module->last_ts = out->pts;

   cb_status = mmal_port_clock_request_add(component->clock[0], out->pts,
      scheduler_component_clock_port_request_cb, out);
   if (cb_status != MMAL_SUCCESS)
   {
      LOG_ERROR("failed to add request");
      out->length = 0;
      mmal_port_buffer_header_callback(port_out, out);
      if (cb_status != MMAL_ECORRUPT)
         module->status = cb_status;
   }
   out = 0;

 end:
   if (in)
      mmal_queue_put_back(queue_in, in);
   if (out)
      mmal_queue_put_back(queue_out, out);

   return mmal_queue_length(queue_in) && mmal_queue_length(queue_out);
}

/** Main processing action */
static void scheduler_component_action(MMAL_COMPONENT_T *component)
{
   /* Send requests to the clock */
   while (scheduler_component_process_buffers(component));
}

/** Destroy a scheduler component */
static MMAL_STATUS_T scheduler_component_destroy(MMAL_COMPONENT_T *component)
{
   unsigned int i;

   for (i = 0; i < component->input_num; i++)
      if (component->input[i]->priv->module->queue)
         mmal_queue_destroy(component->input[i]->priv->module->queue);
   if (component->input_num)
      mmal_ports_free(component->input, component->input_num);

   for (i = 0; i < component->output_num; i++)
      if (component->output[i]->priv->module->queue)
         mmal_queue_destroy(component->output[i]->priv->module->queue);
   if (component->output_num)
      mmal_ports_free(component->output, component->output_num);

   if (component->clock_num)
      mmal_ports_clock_free(component->clock, component->clock_num);

   vcos_free(component->priv->module);
   return MMAL_SUCCESS;
}

/** Enable processing on a port */
static MMAL_STATUS_T scheduler_port_enable(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
   MMAL_PARAM_UNUSED(port);
   MMAL_PARAM_UNUSED(cb);
   return MMAL_SUCCESS;
}

/** Flush a port */
static MMAL_STATUS_T scheduler_port_flush(MMAL_PORT_T *port)
{
   MMAL_PORT_MODULE_T *port_module = port->priv->module;
   MMAL_BUFFER_HEADER_T *buffer;

   /* Flush buffers associated with pending clock requests */
   mmal_port_clock_request_flush(port->component->clock[0]);

   /* Flush buffers that our component is holding on to */
   buffer = mmal_queue_get(port_module->queue);
   while (buffer)
   {
      mmal_port_buffer_header_callback(port, buffer);
      buffer = mmal_queue_get(port_module->queue);
   }

   port->priv->module->last_ts = MMAL_TIME_UNKNOWN;
   return MMAL_SUCCESS;
}

/** Disable processing on a port */
static MMAL_STATUS_T scheduler_port_disable(MMAL_PORT_T *port)
{
   /* We just need to flush our internal queue */
   return scheduler_port_flush(port);
}

/** Send a buffer header to a port */
static MMAL_STATUS_T scheduler_port_send(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_COMPONENT_T *component = port->component;

   /* notify the clock port */
   if (port->type == MMAL_PORT_TYPE_INPUT && !buffer->cmd)
   {
      MMAL_CLOCK_BUFFER_INFO_T info = { buffer->pts, vcos_getmicrosecs() };
      mmal_port_clock_input_buffer_info(port->component->clock[0], &info);
   }

   mmal_queue_put(port->priv->module->queue, buffer);
   return mmal_component_action_trigger(component);
}

/** Set format on an input port */
static MMAL_STATUS_T scheduler_input_port_format_commit(MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_EVENT_FORMAT_CHANGED_T *event;
   MMAL_PORT_T *output = component->output[0];
   MMAL_BUFFER_HEADER_T *buffer;
   MMAL_STATUS_T status;

   /* If the output port is not enabled we just need to update its format.
    * Otherwise we'll have to trigger a format changed event for it. */
   if (!output->is_enabled)
   {
      status = mmal_format_full_copy(output->format, port->format);
      return status;
   }

   /* Send an event on the output port */
   status = mmal_port_event_get(output, &buffer, MMAL_EVENT_FORMAT_CHANGED);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("unable to get an event buffer");
      return status;
   }
   event = mmal_event_format_changed_get(buffer);
   if (!event)
   {
      mmal_buffer_header_release(buffer);
      LOG_ERROR("failed to set format");
      return MMAL_EINVAL;
   }
   mmal_format_copy(event->format, port->format);

   /* Pass on the buffer requirements */
   event->buffer_num_min = port->buffer_num_min;
   event->buffer_size_min = port->buffer_size_min;
   event->buffer_num_recommended = port->buffer_num_recommended;
   event->buffer_size_recommended = port->buffer_size_recommended;

   mmal_port_event_send(component->output[port->index], buffer);
   return status;
}

/** Set format on an output port */
static MMAL_STATUS_T scheduler_output_port_format_commit(MMAL_PORT_T *port)
{
   /* The format of the output port needs to match the input port */
   if (mmal_format_compare(port->format, port->component->input[port->index]->format))
      LOG_DEBUG("output port format different from input port");

   return MMAL_SUCCESS;
}

static MMAL_STATUS_T scheduler_port_parameter_set(MMAL_PORT_T *port, const MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_PORT_T *in = component->input[port->index], *out = component->input[port->index];

   switch (param->id)
   {
   case MMAL_PARAMETER_BUFFER_REQUIREMENTS:
      {
         /* Propagate the requirements to the matching input and output the ports */
         const MMAL_PARAMETER_BUFFER_REQUIREMENTS_T *req = (const MMAL_PARAMETER_BUFFER_REQUIREMENTS_T *)param;
         uint32_t buffer_num_min = MMAL_MAX(port->buffer_num_min, req->buffer_num_min);
         uint32_t buffer_num_recommended = MMAL_MAX(port->buffer_num_recommended, req->buffer_num_recommended);
         uint32_t buffer_size_min = MMAL_MAX(port->buffer_size_min, req->buffer_size_min);
         uint32_t buffer_size_recommended = MMAL_MAX(port->buffer_size_recommended, req->buffer_size_recommended);

         in->buffer_num_min = buffer_num_min;
         in->buffer_num_recommended = buffer_num_recommended;
         in->buffer_size_min = buffer_size_min;
         in->buffer_size_recommended = buffer_size_recommended;
         out->buffer_num_min = buffer_num_min;
         out->buffer_num_recommended = buffer_num_recommended;
         out->buffer_size_min = buffer_size_min;
         out->buffer_size_recommended = buffer_size_recommended;
      }
      return MMAL_SUCCESS;

   default:
      return MMAL_ENOSYS;
   }
}

/** Create an instance of a component  */
static MMAL_STATUS_T mmal_component_create_scheduler(const char *name, MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module;
   MMAL_STATUS_T status = MMAL_ENOMEM;
   int disable_passthrough = 0;
   unsigned int i;

   /* Allocate the context for our module */
   component->priv->module = module = vcos_calloc(1, sizeof(*module), "mmal module");
   if (!module)
      return MMAL_ENOMEM;

   component->priv->pf_destroy = scheduler_component_destroy;

   /* Allocate and initialise all the ports for this component */
   component->input = mmal_ports_alloc(component, SCHEDULER_INPUT_PORTS_NUM,
                                       MMAL_PORT_TYPE_INPUT, sizeof(MMAL_PORT_MODULE_T));
   if (!component->input)
      goto error;
   component->input_num = SCHEDULER_INPUT_PORTS_NUM;
   for (i = 0; i < component->input_num; i++)
   {
      component->input[i]->priv->pf_enable = scheduler_port_enable;
      component->input[i]->priv->pf_disable = scheduler_port_disable;
      component->input[i]->priv->pf_flush = scheduler_port_flush;
      component->input[i]->priv->pf_send = scheduler_port_send;
      component->input[i]->priv->pf_set_format = scheduler_input_port_format_commit;
      component->input[i]->priv->pf_parameter_set = scheduler_port_parameter_set;
      component->input[i]->buffer_num_min = 1;
      component->input[i]->buffer_num_recommended = 0;
      component->input[i]->capabilities = MMAL_PORT_CAPABILITY_SUPPORTS_EVENT_FORMAT_CHANGE;
      component->input[i]->priv->module->queue = mmal_queue_create();
      if (!component->input[i]->priv->module->queue)
         goto error;
      component->input[i]->priv->module->last_ts = MMAL_TIME_UNKNOWN;
   }

   /* Override passthrough behaviour */
   if (strstr(name, ".copy"))
   {
      LOG_TRACE("disable passthrough on output ports");
      disable_passthrough = 1;
   }

   component->output = mmal_ports_alloc(component, SCHEDULER_OUTPUT_PORTS_NUM,
                                        MMAL_PORT_TYPE_OUTPUT, sizeof(MMAL_PORT_MODULE_T));
   if (!component->output)
      goto error;
   component->output_num = SCHEDULER_OUTPUT_PORTS_NUM;
   for (i = 0; i < component->output_num; i++)
   {
      component->output[i]->priv->pf_enable = scheduler_port_enable;
      component->output[i]->priv->pf_disable = scheduler_port_disable;
      component->output[i]->priv->pf_flush = scheduler_port_flush;
      component->output[i]->priv->pf_send = scheduler_port_send;
      component->output[i]->priv->pf_set_format = scheduler_output_port_format_commit;
      component->output[i]->priv->pf_parameter_set = scheduler_port_parameter_set;
      component->output[i]->buffer_num_min = 1;
      component->output[i]->buffer_num_recommended = 0;
      component->output[i]->capabilities = disable_passthrough ? 0 : MMAL_PORT_CAPABILITY_PASSTHROUGH;
      component->output[i]->priv->module->queue = mmal_queue_create();
      if (!component->output[i]->priv->module->queue)
         goto error;
   }

   /* Create the clock port (clock ports are managed by the framework) */
   component->clock = mmal_ports_clock_alloc(component, SCHEDULER_CLOCK_PORTS_NUM, 0, NULL);
   if (!component->clock)
      goto error;
   component->clock_num = SCHEDULER_CLOCK_PORTS_NUM;

   status = mmal_component_action_register(component, scheduler_component_action);
   if (status != MMAL_SUCCESS)
      goto error;

   return MMAL_SUCCESS;

error:
   scheduler_component_destroy(component);
   return status;
}

MMAL_CONSTRUCTOR(mmal_register_component_scheduler);
void mmal_register_component_scheduler(void)
{
   mmal_component_supplier_register("scheduler", mmal_component_create_scheduler);
}
