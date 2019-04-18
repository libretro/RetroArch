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

#define ARTIFICIAL_CAMERA_PORTS_NUM 3

/* Buffering requirements */
#define OUTPUT_MIN_BUFFER_NUM 1
#define OUTPUT_RECOMMENDED_BUFFER_NUM 4

#define DEFAULT_WIDTH 320
#define DEFAULT_HEIGHT 240

/*****************************************************************************/
typedef struct MMAL_PORT_MODULE_T
{
   MMAL_BUFFER_HEADER_VIDEO_SPECIFIC_T frame;
   unsigned int frame_size;
   int count;

   MMAL_QUEUE_T *queue;

} MMAL_PORT_MODULE_T;

typedef struct MMAL_COMPONENT_MODULE_T
{
   MMAL_STATUS_T status;

} MMAL_COMPONENT_MODULE_T;

/*****************************************************************************/
static void artificial_camera_do_processing(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_BUFFER_HEADER_T *buffer;
   unsigned int i;

   if (module->status != MMAL_SUCCESS)
      return;

   /* Loop through all the ports */
   for (i = 0; i < component->output_num; i++)
   {
      MMAL_PORT_T *port = component->output[i];

      buffer = mmal_queue_get(port->priv->module->queue);
      if (!buffer)
         continue;

      /* Sanity check the buffer size */
      if (buffer->alloc_size < port->priv->module->frame_size)
      {
         LOG_ERROR("buffer too small (%i/%i)",
                   buffer->alloc_size, port->priv->module->frame_size);
         module->status = MMAL_EINVAL;
         mmal_queue_put_back(port->priv->module->queue, buffer);
         mmal_event_error_send(component, module->status);
         return;
      }
      module->status = mmal_buffer_header_mem_lock(buffer);
      if (module->status != MMAL_SUCCESS)
      {
         LOG_ERROR("invalid buffer (%p, %p)", buffer, buffer->data);
         mmal_queue_put_back(port->priv->module->queue, buffer);
         mmal_event_error_send(component, module->status);
         return;
      }

      buffer->offset = 0;
      buffer->length = port->priv->module->frame_size;
      buffer->type->video = port->priv->module->frame;

      memset(buffer->data, 0xff, buffer->length);
      if (buffer->type->video.planes > 1)
         memset(buffer->data + buffer->type->video.offset[1],
                0x7f - port->priv->module->count++,
                buffer->length - buffer->type->video.offset[1]);

      mmal_buffer_header_mem_unlock(buffer);
      mmal_port_buffer_header_callback(port, buffer);
   }

   vcos_sleep(10); /* Make sure we don't peg all the resources */
}

/** Destroy a previously created component */
static MMAL_STATUS_T artificial_camera_component_destroy(MMAL_COMPONENT_T *component)
{
   unsigned int i;

   for (i = 0; i < component->output_num; i++)
      if (component->output[i]->priv->module->queue)
         mmal_queue_destroy(component->output[i]->priv->module->queue);

   if(component->output_num)
      mmal_ports_free(component->output, component->output_num);

   vcos_free(component->priv->module);
   return MMAL_SUCCESS;
}

/** Enable processing on a port */
static MMAL_STATUS_T artificial_camera_port_enable(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
   MMAL_PARAM_UNUSED(port);
   MMAL_PARAM_UNUSED(cb);
   return MMAL_SUCCESS;
}

/** Flush a port */
static MMAL_STATUS_T artificial_camera_port_flush(MMAL_PORT_T *port)
{
   MMAL_PARAM_UNUSED(port);
   return MMAL_SUCCESS;
}

/** Disable processing on a port */
static MMAL_STATUS_T artificial_camera_port_disable(MMAL_PORT_T *port)
{
   MMAL_PARAM_UNUSED(port);
   return MMAL_SUCCESS;
}

/** Send a buffer header to a port */
static MMAL_STATUS_T artificial_camera_port_send(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   /* Just queue the buffer */
   mmal_queue_put(port->priv->module->queue, buffer);
   mmal_component_action_trigger(port->component);
   return MMAL_SUCCESS;
}

/** Set format on a port */
static MMAL_STATUS_T artificial_camera_port_format_commit(MMAL_PORT_T *port)
{
   MMAL_PORT_MODULE_T *port_module = port->priv->module;
   unsigned int width = port->format->es->video.width;
   unsigned int height = port->format->es->video.height;
   width = (width + 31) & ~31;
   height = (height + 15) & ~15;

   /* We only support a few formats */
   switch(port->format->encoding)
   {
   case MMAL_ENCODING_I420:
      port_module->frame_size = width * height * 3 / 2;
      port_module->frame.planes = 3;
      port_module->frame.pitch[0] = width;
      port_module->frame.offset[1] = port_module->frame.pitch[0] * height;
      port_module->frame.pitch[1] = width / 2;
      port_module->frame.offset[2] = port_module->frame.offset[1] + port_module->frame.pitch[1] * height / 2;
      port_module->frame.pitch[2] = width / 2;
      break;
   case MMAL_ENCODING_NV21:
      port_module->frame_size = width * height * 3 / 2;
      port_module->frame.planes = 2;
      port_module->frame.pitch[0] = width;
      port_module->frame.offset[1] = port_module->frame.pitch[0] * height;
      port_module->frame.pitch[1] = width;
      break;
   case MMAL_ENCODING_I422:
      port_module->frame_size = width * height * 2;
      port_module->frame.planes = 3;
      port_module->frame.pitch[0] = width;
      port_module->frame.offset[1] = port_module->frame.pitch[0] * height;
      port_module->frame.pitch[1] = width / 2;
      port_module->frame.offset[2] = port_module->frame.offset[1] + port_module->frame.pitch[1] * height;
      port_module->frame.pitch[2] = width / 2;
      break;
   default:
      return MMAL_ENOSYS;
   }

   port->buffer_size_min = port->buffer_size_recommended = port_module->frame_size;
   return MMAL_SUCCESS;
}

/** Set parameter on a port */
static MMAL_STATUS_T artificial_port_parameter_set(MMAL_PORT_T *port, const MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_PARAM_UNUSED(port);
   switch (param->id)
   {
   default:
      return MMAL_ENOSYS;
   }
}

/** Get parameter on a port */
static MMAL_STATUS_T artificial_port_parameter_get(MMAL_PORT_T *port, MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_PARAM_UNUSED(port);
   switch (param->id)
   {
   default:
      return MMAL_ENOSYS;
   }
}

/** Create an instance of a component  */
static MMAL_STATUS_T mmal_component_create_artificial_camera(const char *name, MMAL_COMPONENT_T *component)
{
   MMAL_STATUS_T status = MMAL_ENOMEM;
   unsigned int i;
   MMAL_PARAM_UNUSED(name);

   /* Allocate our module context */
   component->priv->module = vcos_calloc(1, sizeof(*component->priv->module), "mmal module");
   if (!component->priv->module)
      return MMAL_ENOMEM;

   component->priv->pf_destroy = artificial_camera_component_destroy;

   /* Allocate all the ports for this component */
   component->output = mmal_ports_alloc(component, ARTIFICIAL_CAMERA_PORTS_NUM, MMAL_PORT_TYPE_OUTPUT,
                                        sizeof(MMAL_PORT_MODULE_T));
   if(!component->output)
      goto error;
   component->output_num = ARTIFICIAL_CAMERA_PORTS_NUM;

   for (i = 0; i < component->output_num; i++)
   {
      component->output[i]->priv->pf_enable = artificial_camera_port_enable;
      component->output[i]->priv->pf_disable = artificial_camera_port_disable;
      component->output[i]->priv->pf_flush = artificial_camera_port_flush;
      component->output[i]->priv->pf_send = artificial_camera_port_send;
      component->output[i]->priv->pf_send = artificial_camera_port_send;
      component->output[i]->priv->pf_set_format = artificial_camera_port_format_commit;
      component->output[i]->priv->pf_parameter_set = artificial_port_parameter_set;
      component->output[i]->priv->pf_parameter_get = artificial_port_parameter_get;
      component->output[i]->format->type = MMAL_ES_TYPE_VIDEO;
      component->output[i]->format->encoding = MMAL_ENCODING_I420;
      component->output[i]->format->es->video.width = DEFAULT_WIDTH;
      component->output[i]->format->es->video.height = DEFAULT_HEIGHT;
      component->output[i]->buffer_num_min = OUTPUT_MIN_BUFFER_NUM;
      component->output[i]->buffer_num_recommended = OUTPUT_RECOMMENDED_BUFFER_NUM;
      artificial_camera_port_format_commit(component->output[i]);

      component->output[i]->priv->module->queue = mmal_queue_create();
      if (!component->output[i]->priv->module->queue)
         goto error;
   }

   status = mmal_component_action_register(component, artificial_camera_do_processing);
   if (status != MMAL_SUCCESS)
      goto error;

   return MMAL_SUCCESS;

 error:
   artificial_camera_component_destroy(component);
   return status;
}

MMAL_CONSTRUCTOR(mmal_register_component_artificial_camera);
void mmal_register_component_artificial_camera(void)
{
   mmal_component_supplier_register("artificial_camera", mmal_component_create_artificial_camera);
}
