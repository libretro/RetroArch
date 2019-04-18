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
#include "mmal_logging.h"

/*****************************************************************************/
typedef struct MMAL_COMPONENT_MODULE_T
{
   MMAL_STATUS_T status; /**< current status of the component */

} MMAL_COMPONENT_MODULE_T;

typedef struct MMAL_PORT_MODULE_T
{
   MMAL_QUEUE_T *queue; /**< queue for the buffers sent to the ports */
   MMAL_BOOL_T needs_configuring; /**< port is waiting for a format commit */

} MMAL_PORT_MODULE_T;

/*****************************************************************************/

/** Actual processing function */
static MMAL_BOOL_T copy_do_processing(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_PORT_T *port_in = component->input[0];
   MMAL_PORT_T *port_out = component->output[0];
   MMAL_BUFFER_HEADER_T *in, *out;

   if (port_out->priv->module->needs_configuring)
      return 0;

   in = mmal_queue_get(port_in->priv->module->queue);
   if (!in)
      return 0;

   /* Handle event buffers */
   if (in->cmd)
   {
      MMAL_EVENT_FORMAT_CHANGED_T *event = mmal_event_format_changed_get(in);
      if (event)
      {
         module->status = mmal_format_full_copy(port_in->format, event->format);
         if (module->status == MMAL_SUCCESS)
            module->status = port_in->priv->pf_set_format(port_in);
         if (module->status != MMAL_SUCCESS)
         {
            LOG_ERROR("format not set on port %s %p (%i)", port_in->name, port_in, module->status);
            if (mmal_event_error_send(component, module->status) != MMAL_SUCCESS)
               LOG_ERROR("unable to send an error event buffer");
         }
      }
      else
      {
         LOG_ERROR("discarding event %i on port %s %p", (int)in->cmd, port_in->name, port_in);
      }

      in->length = 0;
      mmal_port_buffer_header_callback(port_in, in);
      return 1;
   }

   /* Don't do anything if we've already seen an error */
   if (module->status != MMAL_SUCCESS)
   {
      mmal_queue_put_back(port_in->priv->module->queue, in);
      return 0;
   }

   out = mmal_queue_get(port_out->priv->module->queue);
   if (!out)
   {
      mmal_queue_put_back(port_in->priv->module->queue, in);
      return 0;
   }

   /* Sanity check the output buffer is big enough */
   if (out->alloc_size < in->length)
   {
      module->status = MMAL_EINVAL;
      if (mmal_event_error_send(component, module->status) != MMAL_SUCCESS)
         LOG_ERROR("unable to send an error event buffer");
      return 0;
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

   /* Send buffers back */
   in->length = 0;
   mmal_port_buffer_header_callback(port_in, in);
   mmal_port_buffer_header_callback(port_out, out);
   return 1;
}

/*****************************************************************************/
static void copy_do_processing_loop(MMAL_COMPONENT_T *component)
{
   while (copy_do_processing(component));
}

/** Destroy a previously created component */
static MMAL_STATUS_T copy_component_destroy(MMAL_COMPONENT_T *component)
{
   unsigned int i;

   for(i = 0; i < component->input_num; i++)
      if(component->input[i]->priv->module->queue)
         mmal_queue_destroy(component->input[i]->priv->module->queue);
   if(component->input_num)
      mmal_ports_free(component->input, component->input_num);

   for(i = 0; i < component->output_num; i++)
      if(component->output[i]->priv->module->queue)
         mmal_queue_destroy(component->output[i]->priv->module->queue);
   if(component->output_num)
      mmal_ports_free(component->output, component->output_num);

   vcos_free(component->priv->module);
   return MMAL_SUCCESS;
}

/** Enable processing on a port */
static MMAL_STATUS_T copy_port_enable(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
   MMAL_PARAM_UNUSED(cb);

   /* We need to propagate the buffer requirements when the input port is
    * enabled */
   if (port->type == MMAL_PORT_TYPE_INPUT)
      return port->priv->pf_set_format(port);

   return MMAL_SUCCESS;
}

/** Flush a port */
static MMAL_STATUS_T copy_port_flush(MMAL_PORT_T *port)
{
   MMAL_PORT_MODULE_T *port_module = port->priv->module;
   MMAL_BUFFER_HEADER_T *buffer;

   /* Flush buffers that our component is holding on to */
   buffer = mmal_queue_get(port_module->queue);
   while(buffer)
   {
      mmal_port_buffer_header_callback(port, buffer);
      buffer = mmal_queue_get(port_module->queue);
   }

   return MMAL_SUCCESS;
}

/** Disable processing on a port */
static MMAL_STATUS_T copy_port_disable(MMAL_PORT_T *port)
{
   /* We just need to flush our internal queue */
   return copy_port_flush(port);
}

/** Send a buffer header to a port */
static MMAL_STATUS_T copy_port_send(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   mmal_queue_put(port->priv->module->queue, buffer);
   mmal_component_action_trigger(port->component);
   return MMAL_SUCCESS;
}

/** Set format on input port */
static MMAL_STATUS_T copy_input_port_format_commit(MMAL_PORT_T *in)
{
   MMAL_COMPONENT_T *component = in->component;
   MMAL_PORT_T *out = component->output[0];
   MMAL_EVENT_FORMAT_CHANGED_T *event;
   MMAL_BUFFER_HEADER_T *buffer;
   MMAL_STATUS_T status;

   /* Check if there's anything to propagate to the output port */
   /* The format of the output port needs to match the input port */
   if (!mmal_format_compare(in->format, out->format) &&
       out->buffer_size_min == out->buffer_size_recommended &&
       out->buffer_size_min == MMAL_MAX(in->buffer_size_min, in->buffer_size))
      return MMAL_SUCCESS;

   /* If the output port is not enabled we just need to update its format.
    * Otherwise we'll have to trigger a format changed event for it. */
   if (!out->is_enabled)
   {
      out->buffer_size_min = out->buffer_size_recommended =
         MMAL_MAX(in->buffer_size, in->buffer_size_min);
      return mmal_format_full_copy(out->format, in->format);
   }

   /* Send an event on the output port */
   status = mmal_port_event_get(out, &buffer, MMAL_EVENT_FORMAT_CHANGED);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("unable to get an event buffer");
      return status;
   }

   event = mmal_event_format_changed_get(buffer);
   mmal_format_copy(event->format, in->format); /* FIXME: can full copy be done ? */

   /* Pass on the buffer requirements */
   event->buffer_num_min = out->buffer_num_min;
   event->buffer_num_recommended = out->buffer_num_recommended;
   event->buffer_size_min = event->buffer_size_recommended =
      MMAL_MAX(in->buffer_size_min, in->buffer_size);

   out->priv->module->needs_configuring = 1;
   mmal_port_event_send(out, buffer);
   return status;
}

/** Set format on output port */
static MMAL_STATUS_T copy_output_port_format_commit(MMAL_PORT_T *out)
{
   MMAL_COMPONENT_T *component = out->component;
   MMAL_PORT_T *in = component->input[0];

   /* The format of the output port needs to match the input port */
   if (mmal_format_compare(out->format, in->format))
      return MMAL_EINVAL;

   out->priv->module->needs_configuring = 0;
   mmal_component_action_trigger(out->component);
   return MMAL_SUCCESS;
}

/** Create an instance of a component  */
static MMAL_STATUS_T mmal_component_create_copy(const char *name, MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module;
   MMAL_STATUS_T status = MMAL_ENOMEM;
   MMAL_PARAM_UNUSED(name);

   /* Allocate the context for our module */
   component->priv->module = module = vcos_malloc(sizeof(*module), "mmal module");
   if (!module)
      return MMAL_ENOMEM;
   memset(module, 0, sizeof(*module));

   component->priv->pf_destroy = copy_component_destroy;

   /* Allocate and initialise all the ports for this component */
   component->input = mmal_ports_alloc(component, 1, MMAL_PORT_TYPE_INPUT, sizeof(MMAL_PORT_MODULE_T));
   if(!component->input)
      goto error;
   component->input_num = 1;
   component->input[0]->priv->pf_enable = copy_port_enable;
   component->input[0]->priv->pf_disable = copy_port_disable;
   component->input[0]->priv->pf_flush = copy_port_flush;
   component->input[0]->priv->pf_send = copy_port_send;
   component->input[0]->priv->pf_set_format = copy_input_port_format_commit;
   component->input[0]->buffer_num_min = 1;
   component->input[0]->buffer_num_recommended = 0;
   component->input[0]->priv->module->queue = mmal_queue_create();
   if(!component->input[0]->priv->module->queue)
      goto error;

   component->output = mmal_ports_alloc(component, 1, MMAL_PORT_TYPE_OUTPUT, sizeof(MMAL_PORT_MODULE_T));
   if(!component->output)
      goto error;
   component->output_num = 1;
   component->output[0]->priv->pf_enable = copy_port_enable;
   component->output[0]->priv->pf_disable = copy_port_disable;
   component->output[0]->priv->pf_flush = copy_port_flush;
   component->output[0]->priv->pf_send = copy_port_send;
   component->output[0]->priv->pf_set_format = copy_output_port_format_commit;
   component->output[0]->buffer_num_min = 1;
   component->output[0]->buffer_num_recommended = 0;
   component->output[0]->priv->module->queue = mmal_queue_create();
   if(!component->output[0]->priv->module->queue)
      goto error;

   status = mmal_component_action_register(component, copy_do_processing_loop);
   if (status != MMAL_SUCCESS)
      goto error;

   return MMAL_SUCCESS;

 error:
   copy_component_destroy(component);
   return status;
}

MMAL_CONSTRUCTOR(mmal_register_component_copy);
void mmal_register_component_copy(void)
{
   mmal_component_supplier_register("copy", mmal_component_create_copy);
}
