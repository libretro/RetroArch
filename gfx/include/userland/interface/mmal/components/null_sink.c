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

#define NULLSINK_PORTS_NUM 1

/** Destroy a previously created component */
static MMAL_STATUS_T null_sink_component_destroy(MMAL_COMPONENT_T *component)
{
   if(component->input_num)
      mmal_ports_free(component->input, component->input_num);
   return MMAL_SUCCESS;
}

/** Enable processing on a port */
static MMAL_STATUS_T null_sink_port_enable(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
   MMAL_PARAM_UNUSED(port);
   MMAL_PARAM_UNUSED(cb);
   return MMAL_SUCCESS;
}

/** Flush a port */
static MMAL_STATUS_T null_sink_port_flush(MMAL_PORT_T *port)
{
   MMAL_PARAM_UNUSED(port);
   return MMAL_SUCCESS;
}

/** Disable processing on a port */
static MMAL_STATUS_T null_sink_port_disable(MMAL_PORT_T *port)
{
   MMAL_PARAM_UNUSED(port);
   return MMAL_SUCCESS;
}

/** Send a buffer header to a port */
static MMAL_STATUS_T null_sink_port_send(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_BOOL_T eos = buffer->flags & MMAL_BUFFER_HEADER_FLAG_EOS;

   /* Send buffer back */
   buffer->length = 0;
   mmal_port_buffer_header_callback(port, buffer);

   /* Generate EOS events */
   if(eos)
      return mmal_event_eos_send(port);

   return MMAL_SUCCESS;
}

/** Set format on a port */
static MMAL_STATUS_T null_sink_port_format_commit(MMAL_PORT_T *port)
{
   MMAL_PARAM_UNUSED(port);
   return MMAL_SUCCESS;
}

/** Create an instance of a component  */
static MMAL_STATUS_T mmal_component_create_null_sink(const char *name, MMAL_COMPONENT_T *component)
{
   MMAL_STATUS_T status = MMAL_ENOMEM;
   unsigned int i;
   MMAL_PARAM_UNUSED(name);

   component->priv->pf_destroy = null_sink_component_destroy;

   /* Allocate all the ports for this component */
   component->input = mmal_ports_alloc(component, NULLSINK_PORTS_NUM, MMAL_PORT_TYPE_INPUT, 0);
   if(!component->input)
      goto error;
   component->input_num = NULLSINK_PORTS_NUM;

   for(i = 0; i < component->input_num; i++)
   {
      component->input[i]->priv->pf_enable = null_sink_port_enable;
      component->input[i]->priv->pf_disable = null_sink_port_disable;
      component->input[i]->priv->pf_flush = null_sink_port_flush;
      component->input[i]->priv->pf_send = null_sink_port_send;
      component->input[i]->priv->pf_set_format = null_sink_port_format_commit;
      component->input[i]->buffer_num_min = 1;
      component->input[i]->buffer_num_recommended = 1;
   }

   return MMAL_SUCCESS;

 error:
   null_sink_component_destroy(component);
   return status;
}

MMAL_CONSTRUCTOR(mmal_register_component_null_sink);
void mmal_register_component_null_sink(void)
{
   mmal_component_supplier_register("null_sink", mmal_component_create_null_sink);
}
