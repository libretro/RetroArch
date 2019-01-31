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
#include "util/mmal_connection.h"
#include "mmal_logging.h"
#include <stdio.h>

#define CONNECTION_NAME_FORMAT "%s:%.2222s:%i/%s:%.2222s:%i"

typedef struct
{
   MMAL_CONNECTION_T connection; /**< Must be the first member! */
   MMAL_PORT_T *pool_port;       /**< Port used to create the pool */

   /** Reference counting */
   int refcount;

} MMAL_CONNECTION_PRIVATE_T;

/** Callback from a clock port. Buffer is immediately sent to next component. */
static void mmal_connection_bh_clock_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_STATUS_T status = MMAL_SUCCESS;
   MMAL_CONNECTION_T *connection = (MMAL_CONNECTION_T *)port->userdata;
   MMAL_PORT_T *other_port = (port == connection->in) ? connection->out : connection->in;

   LOG_TRACE("(%s)%p,%p,%p,%i", port->name, port, buffer, buffer->data, (int)buffer->length);

   if (other_port->is_enabled)
   {
      status = mmal_port_send_buffer(other_port, buffer);
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("error sending buffer to clock port (%i)", status);
         mmal_buffer_header_release(buffer);
      }
   }
   else
   {
      mmal_buffer_header_release(buffer);
   }
}

/** Callback from an input port. Buffer is released. */
static void mmal_connection_bh_in_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   LOG_TRACE("(%s)%p,%p,%p,%i", port->name, port, buffer, buffer->data, (int)buffer->length);

   /* We're done with the buffer, just recycle it */
   mmal_buffer_header_release(buffer);
}

/** Callback from an output port. Buffer is queued for the next component. */
static void mmal_connection_bh_out_cb(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_CONNECTION_T *connection = (MMAL_CONNECTION_T *)port->userdata;

   LOG_TRACE("(%s)%p,%p,%p,%i", port->name, port, buffer, buffer->data, (int)buffer->length);

   /* Queue the buffer produced by the output port */
   mmal_queue_put(connection->queue, buffer);

   if (connection->callback)
      connection->callback(connection);
}

/** Callback from the pool. Buffer is available. */
static MMAL_BOOL_T mmal_connection_bh_release_cb(MMAL_POOL_T *pool, MMAL_BUFFER_HEADER_T *buffer,
   void *userdata)
{
   MMAL_CONNECTION_T *connection = (MMAL_CONNECTION_T *)userdata;
   MMAL_PARAM_UNUSED(pool);

   /* Queue the buffer produced by the output port */
   mmal_queue_put(pool->queue, buffer);

   if (connection->callback)
      connection->callback(connection);

   return 0;
}

/*****************************************************************************/
static MMAL_STATUS_T mmal_connection_destroy_internal(MMAL_CONNECTION_T *connection)
{
   MMAL_STATUS_T status;

   if (connection->is_enabled)
   {
      status = mmal_connection_disable(connection);
      if (status != MMAL_SUCCESS)
         return status;
   }

   /* Special case for tunnelling */
   if (connection->flags & MMAL_CONNECTION_FLAG_TUNNELLING)
   {
      status = mmal_port_disconnect(connection->out);
      if (status != MMAL_SUCCESS)
         LOG_ERROR("connection %s could not be cleared", connection->name);
   }

   /* Cleanup resources */
   if (connection->pool)
      mmal_pool_destroy(connection->pool);
   if (connection->queue)
      mmal_queue_destroy(connection->queue);

   vcos_free(connection);
   return MMAL_SUCCESS;
}

/*****************************************************************************/
MMAL_STATUS_T mmal_connection_destroy(MMAL_CONNECTION_T *connection)
{
   MMAL_CONNECTION_PRIVATE_T *private = (MMAL_CONNECTION_PRIVATE_T *)connection;

   LOG_TRACE("%p, %s", connection, connection->name);

   if (--private->refcount)
   {
      LOG_DEBUG("delaying destruction of %s (refount %i)", connection->name,
                private->refcount);
      return MMAL_SUCCESS;
   }

   return mmal_connection_destroy_internal(connection);
}

/*****************************************************************************/
MMAL_STATUS_T mmal_connection_create(MMAL_CONNECTION_T **cx,
   MMAL_PORT_T *out, MMAL_PORT_T *in, uint32_t flags)
{
   MMAL_STATUS_T status = MMAL_ENOMEM;
   unsigned int name_size = strlen(out->component->name) + strlen(in->component->name) + sizeof(CONNECTION_NAME_FORMAT);
   unsigned int size = sizeof(MMAL_CONNECTION_PRIVATE_T) + name_size;
   MMAL_CONNECTION_PRIVATE_T *private;
   MMAL_CONNECTION_T *connection;
   char *name;

   /* Sanity checking */
   if (!cx)
      return MMAL_EINVAL;

   private = vcos_malloc(size, "mmal connection");
   if (!private)
      return MMAL_ENOMEM;
   memset(private, 0, size);
   connection = &private->connection;
   private->refcount = 1;
   name = (char *)&private[1];

   vcos_snprintf(name, name_size - 1, CONNECTION_NAME_FORMAT,
            out->component->name,
            mmal_port_type_to_string(out->type), (int)out->index,
            in->component->name,
            mmal_port_type_to_string(in->type), (int)in->index);

   LOG_TRACE("out %p, in %p, flags %x, %s", out, in, flags, name);

   connection->out = out;
   connection->in = in;
   connection->flags = flags;
   connection->name = name;

   connection->time_setup = vcos_getmicrosecs();

   if (!(connection->flags & MMAL_CONNECTION_FLAG_KEEP_PORT_FORMATS))
   {
      /* Set the format of the input port to match the output one */
      status = mmal_format_full_copy(in->format, out->format);
      if (status == MMAL_SUCCESS)
         status = mmal_port_format_commit(in);
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("format not set on input port");
         goto error;
      }
   }

   /* In pass-through mode we need to propagate the buffer requirements of the
    * connected input port */
   if (out->capabilities & MMAL_PORT_CAPABILITY_PASSTHROUGH)
   {
      MMAL_PARAMETER_BUFFER_REQUIREMENTS_T param =
         {{MMAL_PARAMETER_BUFFER_REQUIREMENTS, sizeof(MMAL_PARAMETER_BUFFER_REQUIREMENTS_T)},
           in->buffer_num_min, in->buffer_size_min, in->buffer_alignment_min,
           in->buffer_num_recommended, in->buffer_size_recommended};
      status = mmal_port_parameter_set(out, &param.hdr);
      if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
      {
         LOG_ERROR("failed to propagate buffer requirements");
         goto error;
      }
      status = MMAL_SUCCESS;
   }

   /* Special case for tunnelling */
   if (connection->flags & MMAL_CONNECTION_FLAG_TUNNELLING)
   {
      status = mmal_port_connect(out, in);
      if (status != MMAL_SUCCESS)
         LOG_ERROR("connection could not be made");
      goto done;
   }

   /* Create empty pool of buffer headers for now (will be resized later on) */
   private->pool_port = (in->capabilities & MMAL_PORT_CAPABILITY_ALLOCATION) ? in : out;
   if (flags & MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT)
      private->pool_port = in;
   if (flags & MMAL_CONNECTION_FLAG_ALLOCATION_ON_OUTPUT)
      private->pool_port = out;
   connection->pool = mmal_port_pool_create(private->pool_port, 0, 0);
   if (!connection->pool)
      goto error;
   mmal_pool_callback_set(connection->pool, mmal_connection_bh_release_cb, (void *)connection);

   /* Create a queue to store the buffers from the output port */
   connection->queue = mmal_queue_create();
   if (!connection->queue)
      goto error;

 done:
   out->userdata = (void *)connection;
   in->userdata = (void *)connection;
   connection->time_setup = vcos_getmicrosecs() - connection->time_setup;
   *cx = connection;
   return status;

 error:
   /* coverity[var_deref_model] mmal_connection_destroy_internal will check connection->pool correctly */
   mmal_connection_destroy_internal(connection);
   return status == MMAL_SUCCESS ? MMAL_ENOMEM : status;
}

/*****************************************************************************/
void mmal_connection_acquire(MMAL_CONNECTION_T *connection)
{
   MMAL_CONNECTION_PRIVATE_T *private = (MMAL_CONNECTION_PRIVATE_T *)connection;
   LOG_TRACE("connection %s(%p), refcount %i", connection->name, connection,
             private->refcount);
   private->refcount++;
}

/*****************************************************************************/
MMAL_STATUS_T mmal_connection_release(MMAL_CONNECTION_T *connection)
{
   MMAL_CONNECTION_PRIVATE_T *private = (MMAL_CONNECTION_PRIVATE_T *)connection;
   LOG_TRACE("connection %s(%p), refcount %i", connection->name, connection,
             private->refcount);

   if (--private->refcount)
      return MMAL_SUCCESS;

   LOG_TRACE("destroying connection %s(%p)", connection->name, connection);
   return mmal_connection_destroy_internal(connection);
}

/*****************************************************************************/
MMAL_STATUS_T mmal_connection_enable(MMAL_CONNECTION_T *connection)
{
   MMAL_PORT_T *in = connection->in, *out = connection->out;
   uint32_t buffer_num, buffer_size;
   MMAL_STATUS_T status;

   LOG_TRACE("%p, %s", connection, connection->name);

   if (connection->is_enabled)
      return MMAL_SUCCESS;

   connection->time_enable = vcos_getmicrosecs();

   /* Override the buffer values with the recommended ones (the port probably knows best) */
   if (!(connection->flags & MMAL_CONNECTION_FLAG_KEEP_BUFFER_REQUIREMENTS))
   {
      if (out->buffer_num_recommended)
         out->buffer_num = out->buffer_num_recommended;
      if (out->buffer_size_recommended)
         out->buffer_size = out->buffer_size_recommended;
      if (in->buffer_num_recommended)
         in->buffer_num = in->buffer_num_recommended;
      if (in->buffer_size_recommended)
         in->buffer_size = in->buffer_size_recommended;
   }

   /* Special case for tunnelling */
   if (connection->flags & MMAL_CONNECTION_FLAG_TUNNELLING)
   {
      /* Enable port. No callback because the port is connected. Other end of the connection
       * will be enabled automatically. */
      status = mmal_port_enable(out, NULL);
      if (status)
         LOG_ERROR("output port couldn't be enabled");
      goto done;
   }

   /* Set the buffering properties on both ports */
   buffer_num = MMAL_MAX(out->buffer_num, in->buffer_num);
   buffer_size = MMAL_MAX(out->buffer_size, in->buffer_size);
   out->buffer_num = in->buffer_num = buffer_num;
   out->buffer_size = in->buffer_size = buffer_size;

   /* In pass-through mode there isn't any need to allocate memory */
   if (out->capabilities & MMAL_PORT_CAPABILITY_PASSTHROUGH)
      buffer_size = 0;

   /* Resize the output pool */
   status = mmal_pool_resize(connection->pool, buffer_num, buffer_size);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("couldn't resize pool");
      goto done;
   }

   /* Enable output port. The callback specified here is the function which
    * will be called when an empty buffer header comes back to the port. */
   status = mmal_port_enable(out, (out->type == MMAL_PORT_TYPE_CLOCK) ?
                             mmal_connection_bh_clock_cb : mmal_connection_bh_out_cb);
   if(status)
   {
      LOG_ERROR("output port couldn't be enabled");
      goto done;
   }

   /* Enable input port. The callback specified here is the function which
    * will be called when an empty buffer header comes back to the port. */
   status = mmal_port_enable(in, (in->type == MMAL_PORT_TYPE_CLOCK) ?
                             mmal_connection_bh_clock_cb : mmal_connection_bh_in_cb);
   if(status)
   {
      LOG_ERROR("input port couldn't be enabled");
      mmal_port_disable(out);
      goto done;
   }

   /* Clock ports need buffers to send clock updates, so
    * populate both connected clock ports */
   if ((out->type == MMAL_PORT_TYPE_CLOCK) && (in->type == MMAL_PORT_TYPE_CLOCK))
   {
      MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(connection->pool->queue);
      while (buffer)
      {
         mmal_port_send_buffer(out, buffer);
         buffer = mmal_queue_get(connection->pool->queue);
         if (buffer)
         {
            mmal_port_send_buffer(in, buffer);
            buffer = mmal_queue_get(connection->pool->queue);
         }
      }
   }

 done:
   connection->time_enable = vcos_getmicrosecs() - connection->time_enable;
   connection->is_enabled = status == MMAL_SUCCESS;
   return status;
}

/*****************************************************************************/
MMAL_STATUS_T mmal_connection_disable(MMAL_CONNECTION_T *connection)
{
   MMAL_STATUS_T status;
   MMAL_BUFFER_HEADER_T *buffer;

   LOG_TRACE("%p, %s", connection, connection->name);

   if (!connection->is_enabled)
      return MMAL_SUCCESS;

   connection->time_disable = vcos_getmicrosecs();

   /* Special case for tunnelling */
   if (connection->flags & MMAL_CONNECTION_FLAG_TUNNELLING)
   {
      /* Disable port. Other end of the connection will be disabled automatically. */
      status = mmal_port_disable(connection->out);
      if (status)
         LOG_ERROR("output port couldn't be disabled");
      goto done;
   }

   /* Disable input port. */
   status = mmal_port_disable(connection->in);
   if(status)
   {
      LOG_ERROR("input port couldn't be disabled");
      goto done;
   }

   /* Disable output port */
   status = mmal_port_disable(connection->out);
   if(status)
   {
      LOG_ERROR("output port couldn't be disabled");
      goto done;
   }

   /* Flush the queue */
   buffer = mmal_queue_get(connection->queue);
   while (buffer)
   {
      mmal_buffer_header_release(buffer);
      buffer = mmal_queue_get(connection->queue);
   }
   vcos_assert(mmal_queue_length(connection->pool->queue) == connection->pool->headers_num);

 done:
   connection->time_disable = vcos_getmicrosecs() - connection->time_disable;
   connection->is_enabled = !(status == MMAL_SUCCESS);
   return status;
}

/*****************************************************************************/
static MMAL_STATUS_T mmal_connection_reconfigure(MMAL_CONNECTION_T *connection, MMAL_ES_FORMAT_T *format)
{
   MMAL_STATUS_T status;
   LOG_TRACE("%p, %s", connection, connection->name);

   status = mmal_connection_disable(connection);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("connection couldn't be disabled");
      return status;
   }

   /* Set the new format for the output port */
   status = mmal_format_full_copy(connection->out->format, format);
   if (status == MMAL_SUCCESS)
      status = mmal_port_format_commit(connection->out);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("commit failed on port %s(%p) (%i)",
                connection->out->name, connection->out, status);
      return status;
   }

   /* Set the new format for the input port */
   status = mmal_format_full_copy(connection->in->format, connection->out->format);
   if (status == MMAL_SUCCESS)
      status = mmal_port_format_commit(connection->in);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("commit failed on port %s(%p) (%i)",
                connection->in->name, connection->in, status);
      return status;
   }

   /* Enable ports */
   status = mmal_connection_enable(connection);
   if (status)
   {
      LOG_ERROR("connection couldn't be enabled");
      return status;
   }

   return MMAL_SUCCESS;
}

/*****************************************************************************/
MMAL_STATUS_T mmal_connection_event_format_changed(MMAL_CONNECTION_T *connection,
   MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_EVENT_FORMAT_CHANGED_T *event;
   MMAL_STATUS_T status;

   LOG_TRACE("%p, %s", connection, connection->name);

   if (buffer->cmd != MMAL_EVENT_FORMAT_CHANGED)
      return MMAL_EINVAL;

   event = mmal_event_format_changed_get(buffer);
   if (!event)
      return MMAL_EINVAL;

   /* If we don't need to recreate our buffers then we can just forward the event
    * to the next component (so it gets configured properly) */
   if ((connection->in->capabilities & MMAL_PORT_CAPABILITY_SUPPORTS_EVENT_FORMAT_CHANGE) &&
       event->buffer_size_min <= connection->out->buffer_size &&
       event->buffer_num_min <= connection->out->buffer_num)
   {
      status = mmal_format_full_copy(connection->out->format, event->format);
      if (status == MMAL_SUCCESS)
         status = mmal_port_format_commit(connection->out);
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("format commit failed on port %s(%p) (%i)",
                   connection->out->name, connection->out, status);
         return status;
      }

      mmal_buffer_header_acquire(buffer);
      status = mmal_port_send_buffer(connection->in, buffer);
      if (status != MMAL_SUCCESS)
      {
         LOG_ERROR("buffer send failed on port %s(%p) (%i)",
                   connection->in->name, connection->in, status);
         mmal_buffer_header_release(buffer);
         return status;
      }

      return MMAL_SUCCESS;
   }

   /* Otherwise we have to reconfigure our pipeline */
   return mmal_connection_reconfigure(connection, event->format);
}
