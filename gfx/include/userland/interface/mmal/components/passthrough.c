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

#define PASSTHROUGH_PORTS_NUM 1

/*****************************************************************************/
typedef struct MMAL_COMPONENT_MODULE_T
{
   MMAL_BOOL_T error;      /**< Error state */

} MMAL_COMPONENT_MODULE_T;

typedef struct MMAL_PORT_MODULE_T
{
   MMAL_QUEUE_T *queue; /**< queue for the buffers sent to the ports */

} MMAL_PORT_MODULE_T;

/*****************************************************************************/

/** Destroy a previously created component */
static MMAL_STATUS_T passthrough_component_destroy(MMAL_COMPONENT_T *component)
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
static MMAL_STATUS_T passthrough_port_enable(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
   MMAL_PARAM_UNUSED(port);
   MMAL_PARAM_UNUSED(cb);
   return MMAL_SUCCESS;
}

/** Flush a port */
static MMAL_STATUS_T passthrough_port_flush(MMAL_PORT_T *port)
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
static MMAL_STATUS_T passthrough_port_disable(MMAL_PORT_T *port)
{
   /* We just need to flush our internal queue */
   return passthrough_port_flush(port);
}

/** Send a buffer header to a port */
static MMAL_STATUS_T passthrough_port_send(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_PORT_T **other_port, *in_port, *out_port;
   MMAL_BUFFER_HEADER_T **other_buffer, *in = 0, *out = 0;
   MMAL_STATUS_T status;

   if (module->error)
   {
      mmal_queue_put(port->priv->module->queue, buffer);
      return MMAL_SUCCESS; /* Just do nothing */
   }

   in_port = port->component->input[port->index];
   out_port = port->component->output[port->index];

   if (port->type == MMAL_PORT_TYPE_INPUT)
   {
      other_port = &out_port;
      other_buffer = &out;
      in = buffer;
   }
   else
   {
      other_port = &in_port;
      other_buffer = &in;
      out = buffer;
   }

   /* Get a buffer header from the matching port */
   *other_buffer = mmal_queue_get((*other_port)->priv->module->queue);
   if (!*other_buffer)
   {
      /* None available. Just queue the buffer header for now. */
      mmal_queue_put(port->priv->module->queue, buffer);
      return MMAL_SUCCESS;
   }

   /* Copy our input buffer header */
   status = mmal_buffer_header_replicate(out, in);
   if (status != MMAL_SUCCESS)
      goto error;

   /* Consume the input buffer */
   in->length = 0;

   /* Send buffers back */
   mmal_port_buffer_header_callback(in_port, in);
   mmal_port_buffer_header_callback(out_port, out);

   return MMAL_SUCCESS;

 error:
   mmal_queue_put(in_port->priv->module->queue, in);
   mmal_queue_put(out_port->priv->module->queue, out);
   status = mmal_event_error_send(port->component, status);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("unable to send an error event buffer (%i)", (int)status);
      return MMAL_SUCCESS;
   }
   module->error = 1;
   return MMAL_SUCCESS;
}

/** Set format on a port */
static MMAL_STATUS_T passthrough_port_format_commit(MMAL_PORT_T *port)
{
   /* Sanity check */
   if (port->type == MMAL_PORT_TYPE_OUTPUT)
   {
      LOG_ERROR("output port is read-only");
      return MMAL_EINVAL;
   }

   return mmal_format_full_copy(port->component->output[port->index]->format, port->format);
}

static MMAL_STATUS_T passthrough_port_parameter_set(MMAL_PORT_T *port, const MMAL_PARAMETER_HEADER_T *param)
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
static MMAL_STATUS_T mmal_component_create_passthrough(const char *name, MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module;
   MMAL_STATUS_T status = MMAL_ENOMEM;
   unsigned int i;
   MMAL_PARAM_UNUSED(name);

   /* Allocate the context for our module */
   component->priv->module = module = vcos_malloc(sizeof(*module), "mmal module");
   if (!module)
      return MMAL_ENOMEM;
   memset(module, 0, sizeof(*module));

   component->priv->pf_destroy = passthrough_component_destroy;

   /* Allocate and initialise all the ports for this component */
   component->input = mmal_ports_alloc(component, PASSTHROUGH_PORTS_NUM,
                                       MMAL_PORT_TYPE_INPUT, sizeof(MMAL_PORT_MODULE_T));
   if(!component->input)
      goto error;
   component->input_num = PASSTHROUGH_PORTS_NUM;
   for(i = 0; i < component->input_num; i++)
   {
      component->input[i]->priv->pf_enable = passthrough_port_enable;
      component->input[i]->priv->pf_disable = passthrough_port_disable;
      component->input[i]->priv->pf_flush = passthrough_port_flush;
      component->input[i]->priv->pf_send = passthrough_port_send;
      component->input[i]->priv->pf_set_format = passthrough_port_format_commit;
      component->input[i]->priv->pf_parameter_set = passthrough_port_parameter_set;
      component->input[i]->buffer_num_min = 1;
      component->input[i]->buffer_num_recommended = 0;
      component->input[i]->priv->module->queue = mmal_queue_create();
      if(!component->input[i]->priv->module->queue)
         goto error;
   }

   component->output = mmal_ports_alloc(component, PASSTHROUGH_PORTS_NUM,
                                        MMAL_PORT_TYPE_OUTPUT, sizeof(MMAL_PORT_MODULE_T));
   if(!component->output)
      goto error;
   component->output_num = PASSTHROUGH_PORTS_NUM;
   for(i = 0; i < component->output_num; i++)
   {
      component->output[i]->priv->pf_enable = passthrough_port_enable;
      component->output[i]->priv->pf_disable = passthrough_port_disable;
      component->output[i]->priv->pf_flush = passthrough_port_flush;
      component->output[i]->priv->pf_send = passthrough_port_send;
      component->output[i]->priv->pf_set_format = passthrough_port_format_commit;
      component->output[i]->priv->pf_parameter_set = passthrough_port_parameter_set;
      component->output[i]->buffer_num_min = 1;
      component->output[i]->buffer_num_recommended = 0;
      component->output[i]->capabilities = MMAL_PORT_CAPABILITY_PASSTHROUGH;
      component->output[i]->priv->module->queue = mmal_queue_create();
      if(!component->output[i]->priv->module->queue)
         goto error;
   }

   return MMAL_SUCCESS;

 error:
   passthrough_component_destroy(component);
   return status;
}

MMAL_CONSTRUCTOR(mmal_register_component_passthrough);
void mmal_register_component_passthrough(void)
{
   mmal_component_supplier_register("passthrough", mmal_component_create_passthrough);
}
