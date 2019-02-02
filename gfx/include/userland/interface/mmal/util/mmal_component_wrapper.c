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
#include "util/mmal_component_wrapper.h"
#include "mmal_logging.h"
#include <stdio.h>

typedef struct
{
   MMAL_WRAPPER_T wrapper; /**< Must be the first member! */

   VCOS_SEMAPHORE_T sema;

} MMAL_WRAPPER_PRIVATE_T;

/** Callback from a control port. Error events will be received there. */
static void mmal_wrapper_control_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_WRAPPER_PRIVATE_T *private = (MMAL_WRAPPER_PRIVATE_T *)port->userdata;
   LOG_TRACE("%s(%p),%p,%4.4s", port->name, port, buffer, (char *)&buffer->cmd);

   if (buffer->cmd == MMAL_EVENT_ERROR)
   {
      private->wrapper.status = *(MMAL_STATUS_T *)buffer->data;
      mmal_buffer_header_release(buffer);

      vcos_semaphore_post(&private->sema);

      if (private->wrapper.callback)
         private->wrapper.callback(&private->wrapper);
      return;
   }

   mmal_buffer_header_release(buffer);
}

/** Callback from an input port. Buffer is released. */
static void mmal_wrapper_bh_in_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_PARAM_UNUSED(port);
   LOG_TRACE("(%s)%p,%p,%p,%i", port->name, port, buffer, buffer->data, (int)buffer->length);

   /* We're done with the buffer, just recycle it */
   mmal_buffer_header_release(buffer);
}

/** Callback from an output port. Buffer is queued for the next component. */
static void mmal_wrapper_bh_out_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_WRAPPER_PRIVATE_T *private = (MMAL_WRAPPER_PRIVATE_T *)port->userdata;
   LOG_TRACE("(%s)%p,%p,%p,%i", port->name, port, buffer, buffer->data, (int)buffer->length);

   /* Queue the buffer produced by the output port */
   mmal_queue_put(private->wrapper.output_queue[port->index], buffer);
   vcos_semaphore_post(&private->sema);

   if (private->wrapper.callback)
      private->wrapper.callback(&private->wrapper);
}

/** Callback from the pool. Buffer is available. */
static MMAL_BOOL_T mmal_wrapper_bh_release_cb(MMAL_POOL_T *pool, MMAL_BUFFER_HEADER_T *buffer,
   void *userdata)
{
   MMAL_WRAPPER_PRIVATE_T *private = (MMAL_WRAPPER_PRIVATE_T *)userdata;

   mmal_queue_put(pool->queue, buffer);
   vcos_semaphore_post(&private->sema);

   if (private->wrapper.callback)
      private->wrapper.callback(&private->wrapper);

   return 0;
}

/*****************************************************************************/
MMAL_STATUS_T mmal_wrapper_destroy(MMAL_WRAPPER_T *wrapper)
{
   MMAL_WRAPPER_PRIVATE_T *private = (MMAL_WRAPPER_PRIVATE_T *)wrapper;
   unsigned int i;

   LOG_TRACE("%p, %s", wrapper, wrapper->component->name);

   /* Cleanup resources */
   mmal_component_destroy(wrapper->component);

   for (i = 0; i < wrapper->input_num; i++)
   {
      if (wrapper->input_pool[i])
         mmal_pool_destroy(wrapper->input_pool[i]);
   }

   for (i = 0; i < wrapper->output_num; i++)
   {
      if (wrapper->output_pool[i])
         mmal_pool_destroy(wrapper->output_pool[i]);
      if (wrapper->output_queue[i])
         mmal_queue_destroy(wrapper->output_queue[i]);
   }

   vcos_semaphore_delete(&private->sema);
   vcos_free(private);
   return MMAL_SUCCESS;
}

/*****************************************************************************/
MMAL_STATUS_T mmal_wrapper_create(MMAL_WRAPPER_T **ctx, const char *name)
{
   MMAL_STATUS_T status;
   MMAL_COMPONENT_T *component;
   MMAL_WRAPPER_PRIVATE_T *private;
   MMAL_WRAPPER_T *wrapper;
   int64_t start_time;
   unsigned int i, extra_size;

   LOG_TRACE("wrapper %p, name %s", ctx, name);

   /* Sanity checking */
   if (!ctx || !name)
      return MMAL_EINVAL;

   start_time = vcos_getmicrosecs();

   status = mmal_component_create(name, &component);
   if (status != MMAL_SUCCESS)
      return status;

   extra_size = (component->input_num * sizeof(MMAL_POOL_T*)) + (component->output_num * (sizeof(MMAL_POOL_T*) + sizeof(MMAL_QUEUE_T*)));
   private = vcos_calloc(1, sizeof(*private) + extra_size, "mmal wrapper");
   if (!private)
   {
      mmal_component_destroy(component);
      return MMAL_ENOMEM;
   }

   if (vcos_semaphore_create(&private->sema, "mmal wrapper", 0) != VCOS_SUCCESS)
   {
      mmal_component_destroy(component);
      vcos_free(private);
      return MMAL_ENOMEM;
   }

   wrapper = &private->wrapper;
   wrapper->component = component;
   wrapper->control = component->control;
   wrapper->input_num = component->input_num;
   wrapper->input = component->input;
   wrapper->output_num = component->output_num;
   wrapper->output = component->output;
   wrapper->input_pool = (MMAL_POOL_T **)&private[1];
   wrapper->output_pool = (MMAL_POOL_T **)&wrapper->input_pool[component->input_num];
   wrapper->output_queue = (MMAL_QUEUE_T **)&wrapper->output_pool[component->output_num];

   /* Create our pools and queues */
   for (i = 0; i < wrapper->input_num; i++)
   {
      wrapper->input_pool[i] = mmal_port_pool_create(wrapper->input[i], 0, 0);
      if (!wrapper->input_pool[i])
         goto error;
      mmal_pool_callback_set(wrapper->input_pool[i], mmal_wrapper_bh_release_cb, (void *)wrapper);

      wrapper->input[i]->userdata = (void *)wrapper;
   }
   for (i = 0; i < wrapper->output_num; i++)
   {
      wrapper->output_pool[i] = mmal_port_pool_create(wrapper->output[i], 0, 0);
      wrapper->output_queue[i] = mmal_queue_create();
      if (!wrapper->output_pool[i] || !wrapper->output_queue[i])
         goto error;
      mmal_pool_callback_set(wrapper->output_pool[i], mmal_wrapper_bh_release_cb, (void *)wrapper);

      wrapper->output[i]->userdata = (void *)wrapper;
   }

   /* Setup control port */
   wrapper->control->userdata = (void *)wrapper;
   status = mmal_port_enable(wrapper->control, mmal_wrapper_control_cb);
   if (status != MMAL_SUCCESS)
      goto error;

   wrapper->time_setup = vcos_getmicrosecs() - start_time;
   *ctx = wrapper;
   return MMAL_SUCCESS;

 error:
   mmal_wrapper_destroy(wrapper);
   return status == MMAL_SUCCESS ? MMAL_ENOMEM : status;
}

/*****************************************************************************/
MMAL_STATUS_T mmal_wrapper_port_enable(MMAL_PORT_T *port, uint32_t flags)
{
   MMAL_WRAPPER_PRIVATE_T *private = (MMAL_WRAPPER_PRIVATE_T *)port->userdata;
   MMAL_WRAPPER_T *wrapper = &private->wrapper;
   int64_t start_time = vcos_getmicrosecs();
   uint32_t buffer_size;
   MMAL_STATUS_T status;
   MMAL_POOL_T *pool;

   LOG_TRACE("%p, %s", wrapper, port->name);

   if (port->type != MMAL_PORT_TYPE_INPUT && port->type != MMAL_PORT_TYPE_OUTPUT)
      return MMAL_EINVAL;

   if (port->is_enabled)
      return MMAL_SUCCESS;

   pool = port->type == MMAL_PORT_TYPE_INPUT ?
      wrapper->input_pool[port->index] : wrapper->output_pool[port->index];
   buffer_size = (flags & MMAL_WRAPPER_FLAG_PAYLOAD_ALLOCATE) ? port->buffer_size : 0;

   /* FIXME: we don't support switching between shared and non-shared memory.
    * We would need to save the flag and force a pool resize when switching. */
   if (flags & MMAL_WRAPPER_FLAG_PAYLOAD_USE_SHARED_MEMORY)
   {
      MMAL_PARAMETER_BOOLEAN_T param_zc =
         {{MMAL_PARAMETER_ZERO_COPY, sizeof(MMAL_PARAMETER_BOOLEAN_T)}, 1};
      status = mmal_port_parameter_set(port, &param_zc.hdr);
      if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
      {
         LOG_ERROR("failed to set zero copy on %s", port->name);
         return status;
      }
   }

   /* Resize the pool */
   status = mmal_pool_resize(pool, port->buffer_num, buffer_size);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("could not resize pool (%i/%i)", (int)port->buffer_num, (int)buffer_size);
      return status;
   }

   /* Enable port. The callback specified here is the function which
    * will be called when a buffer header comes back to the port. */
   status = mmal_port_enable(port, port->type == MMAL_PORT_TYPE_INPUT ?
                             mmal_wrapper_bh_in_cb : mmal_wrapper_bh_out_cb);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("could not enable port");
      return status;
   }

   wrapper->time_enable += vcos_getmicrosecs() - start_time;
   return MMAL_SUCCESS;
}

/*****************************************************************************/
MMAL_STATUS_T mmal_wrapper_port_disable(MMAL_PORT_T *port)
{
   MMAL_WRAPPER_PRIVATE_T *private = (MMAL_WRAPPER_PRIVATE_T *)port->userdata;
   MMAL_WRAPPER_T *wrapper = &private->wrapper;
   int64_t start_time = vcos_getmicrosecs();
   MMAL_STATUS_T status;

   LOG_TRACE("%p, %s", wrapper, port->name);

   if (port->type != MMAL_PORT_TYPE_INPUT && port->type != MMAL_PORT_TYPE_OUTPUT)
      return MMAL_EINVAL;

   if (!port->is_enabled)
      return MMAL_SUCCESS;

   /* Disable port */
   status = mmal_port_disable(port);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("could not disable port");
      return status;
   }

   /* Flush the queue */
   if (port->type == MMAL_PORT_TYPE_OUTPUT)
   {
      MMAL_POOL_T *pool = wrapper->output_pool[port->index];
      MMAL_QUEUE_T *queue = wrapper->output_queue[port->index];
      MMAL_BUFFER_HEADER_T *buffer;

      while ((buffer = mmal_queue_get(queue)) != NULL)
         mmal_buffer_header_release(buffer);

      if ( !vcos_verify(mmal_queue_length(pool->queue) == pool->headers_num) )
      {
         LOG_ERROR("coul dnot release all buffers");
      }
   }

   wrapper->time_disable = vcos_getmicrosecs() - start_time;
   return status;
}

/** Wait for an empty buffer to be available on a port */
MMAL_STATUS_T mmal_wrapper_buffer_get_empty(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T **buffer,
   uint32_t flags)
{
   MMAL_WRAPPER_PRIVATE_T *private = (MMAL_WRAPPER_PRIVATE_T *)port->userdata;
   MMAL_WRAPPER_T *wrapper = &private->wrapper;
   MMAL_POOL_T *pool;

   LOG_TRACE("%p, %s", wrapper, port->name);

   if (!buffer || (port->type != MMAL_PORT_TYPE_INPUT && port->type != MMAL_PORT_TYPE_OUTPUT))
      return MMAL_EINVAL;

   pool = port->type == MMAL_PORT_TYPE_INPUT ?
      wrapper->input_pool[port->index] : wrapper->output_pool[port->index];

   while (wrapper->status == MMAL_SUCCESS &&
          (*buffer = mmal_queue_get(pool->queue)) == NULL)
   {
      if (!(flags & MMAL_WRAPPER_FLAG_WAIT))
         break;
      vcos_semaphore_wait(&private->sema);
   }

   return wrapper->status == MMAL_SUCCESS && !*buffer ? MMAL_EAGAIN : wrapper->status;
}

/** Wait for a full buffer to be available on a port */
MMAL_STATUS_T mmal_wrapper_buffer_get_full(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T **buffer,
   uint32_t flags)
{
   MMAL_WRAPPER_PRIVATE_T *private = (MMAL_WRAPPER_PRIVATE_T *)port->userdata;
   MMAL_WRAPPER_T *wrapper = &private->wrapper;
   MMAL_QUEUE_T *queue;

   LOG_TRACE("%p, %s", wrapper, port->name);

   if (!buffer || port->type != MMAL_PORT_TYPE_OUTPUT)
      return MMAL_EINVAL;
   queue = wrapper->output_queue[port->index];

   while (wrapper->status == MMAL_SUCCESS &&
          (*buffer = mmal_queue_get(queue)) == NULL)
   {
      if (!(flags & MMAL_WRAPPER_FLAG_WAIT))
         break;
      vcos_semaphore_wait(&private->sema);
   }

   return wrapper->status == MMAL_SUCCESS && !*buffer ? MMAL_EAGAIN : wrapper->status;
}
