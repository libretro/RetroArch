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

#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal.h"
#include "mmal_vc_api.h"
#include "mmal_vc_msgs.h"
#include "mmal_vc_client_priv.h"
#include "mmal_vc_opaque_alloc.h"
#include "mmal_vc_shm.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/core/mmal_component_private.h"
#include "interface/mmal/core/mmal_port_private.h"
#include "interface/mmal/core/mmal_buffer_private.h"
#include "interface/vcos/vcos.h"

/** Private information for MMAL VC components
 */

typedef enum MMAL_ZEROLEN_CHECK_T
{
   ZEROLEN_NOT_INITIALIZED,
   ZEROLEN_COMPATIBLE,
   ZEROLEN_INCOMPATIBLE
} MMAL_ZEROLEN_CHECK_T;

typedef enum MMAL_PORT_FLUSH_CHECK_T
{
   PORT_FLUSH_NOT_INITIALIZED,
   PORT_FLUSH_COMPATIBLE,
   PORT_FLUSH_INCOMPATIBLE
} MMAL_PORT_FLUSH_CHECK_T;

typedef struct MMAL_PORT_MODULE_T
{
   uint32_t magic;
   uint32_t component_handle;
   MMAL_PORT_T *port;
   uint32_t port_handle;

   MMAL_BOOL_T has_pool;
   VCOS_BLOCKPOOL_T pool;

   MMAL_BOOL_T is_zero_copy;
   MMAL_BOOL_T zero_copy_workaround;
   uint32_t opaque_allocs;

   MMAL_BOOL_T sent_data_on_port;

   MMAL_PORT_T *connected;           /**< Connected port if any */
} MMAL_PORT_MODULE_T;

typedef struct MMAL_COMPONENT_MODULE_T
{
   uint32_t component_handle;

   MMAL_PORT_MODULE_T **ports;
   uint32_t ports_num;

   MMAL_QUEUE_T *callback_queue;   /**< Used to queue the callbacks we need to make to the client */

   MMAL_BOOL_T event_ctx_initialised;
   MMAL_VC_CLIENT_BUFFER_CONTEXT_T event_ctx; /**< Used as the ctx for event buffers */
} MMAL_COMPONENT_MODULE_T;

/*****************************************************************************
 * Local function prototypes
 *****************************************************************************/
static void mmal_vc_do_callback(MMAL_COMPONENT_T *component);
static MMAL_STATUS_T mmal_vc_port_info_get(MMAL_PORT_T *port);

/*****************************************************************************/
MMAL_STATUS_T mmal_vc_get_version(uint32_t *major, uint32_t *minor, uint32_t *minimum)
{
   mmal_worker_version msg;
   size_t len = sizeof(msg);
   MMAL_STATUS_T status;

   status = mmal_vc_sendwait_message(mmal_vc_get_client(), &msg.header, sizeof(msg),
                                     MMAL_WORKER_GET_VERSION, &msg, &len, MMAL_FALSE);

   if (status != MMAL_SUCCESS)
      return status;

   if (!vcos_verify(len == sizeof(msg)))
      return MMAL_EINVAL;

   *major = msg.major;
   *minor = msg.minor;
   *minimum = msg.minimum;
   return MMAL_SUCCESS;
}

/*****************************************************************************/
MMAL_STATUS_T mmal_vc_get_stats(MMAL_VC_STATS_T *stats, int reset)
{
   mmal_worker_stats msg;
   size_t len = sizeof(msg);
   msg.reset = reset;

   MMAL_STATUS_T status = mmal_vc_sendwait_message(mmal_vc_get_client(),
                                                   &msg.header, sizeof(msg),
                                                   MMAL_WORKER_GET_STATS,
                                                   &msg, &len, MMAL_FALSE);

   if (status == MMAL_SUCCESS)
   {
      vcos_assert(len == sizeof(msg));
      *stats = msg.stats;
   }
   return status;
}

/** Set port buffer requirements. */
static MMAL_STATUS_T mmal_vc_port_requirements_set(MMAL_PORT_T *port)
{
   MMAL_PORT_MODULE_T *module = port->priv->module;
   MMAL_STATUS_T status;
   mmal_worker_reply reply;
   mmal_worker_port_action msg;
   size_t replylen = sizeof(reply);

   msg.component_handle = module->component_handle;
   msg.action = MMAL_WORKER_PORT_ACTION_SET_REQUIREMENTS;
   msg.port_handle = module->port_handle;
   msg.param.enable.port = *port;

   status = mmal_vc_sendwait_message(mmal_vc_get_client(), &msg.header, sizeof(msg),
                                     MMAL_WORKER_PORT_ACTION, &reply, &replylen, MMAL_FALSE);
   if (status == MMAL_SUCCESS)
   {
      vcos_assert(replylen == sizeof(reply));
      status = reply.status;
   }
   if (status != MMAL_SUCCESS)
      LOG_ERROR("failed to set port requirements (%i/%i,%i/%i)",
                port->buffer_num, port->buffer_num_min,
                port->buffer_size, port->buffer_size_min);

   return status;
}

/** Get port buffer requirements. */
static MMAL_STATUS_T mmal_vc_port_requirements_get(MMAL_PORT_T *port)
{
   MMAL_PORT_MODULE_T *module = port->priv->module;
   mmal_worker_port_info_get msg;
   mmal_worker_port_info reply;
   size_t replylen = sizeof(reply);
   MMAL_STATUS_T status;

   msg.component_handle = module->component_handle;
   msg.port_type = port->type;
   msg.index = port->index;

   LOG_TRACE("get port requirements (%i:%i)", port->type, port->index);

   status = mmal_vc_sendwait_message(mmal_vc_get_client(), &msg.header, sizeof(msg),
                                     MMAL_WORKER_PORT_INFO_GET, &reply, &replylen, MMAL_FALSE);
   if (status == MMAL_SUCCESS)
   {
      vcos_assert(replylen == sizeof(reply));
      status = reply.status;
   }
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("failed to get port requirements (%i:%i)", port->type, port->index);
      return status;
   }

   port->buffer_num_min = reply.port.buffer_num_min;
   port->buffer_num_recommended = reply.port.buffer_num_recommended;
   port->buffer_size_min = reply.port.buffer_size_min;
   port->buffer_size_recommended = reply.port.buffer_size_recommended;
   port->buffer_alignment_min = reply.port.buffer_alignment_min;

   return MMAL_SUCCESS;
}

/** Enable processing on a port */
static MMAL_STATUS_T mmal_vc_port_enable(MMAL_PORT_T *port, MMAL_PORT_BH_CB_T cb)
{
   MMAL_PORT_MODULE_T *module = port->priv->module;
   MMAL_STATUS_T status;
   mmal_worker_reply reply;
   mmal_worker_port_action msg;
   size_t replylen = sizeof(reply);
   MMAL_PARAM_UNUSED(cb);

   if (!port->component->priv->module->event_ctx_initialised)
   {
      MMAL_POOL_T *pool = port->component->priv->event_pool;
      MMAL_DRIVER_BUFFER_T *drv;
      unsigned int i;

      /* We need to associate our vc client context to all our event buffers.
       * This only needs to be done when the first port is enabled because no event
       * can be received on disabled ports. */
      for (i = 0; i < pool->headers_num; i++)
      {
         drv = mmal_buffer_header_driver_data(pool->header[i]);
         drv->client_context = &port->component->priv->module->event_ctx;
         drv->magic = MMAL_MAGIC;
      }

      port->component->priv->module->event_ctx_initialised = MMAL_TRUE;
   }

   if (!module->connected)
   {
      if (vcos_blockpool_create_on_heap(&module->pool, port->buffer_num,
             sizeof(MMAL_VC_CLIENT_BUFFER_CONTEXT_T),
             VCOS_BLOCKPOOL_ALIGN_DEFAULT, VCOS_BLOCKPOOL_FLAG_NONE, "mmal vc port pool") != VCOS_SUCCESS)
      {
         LOG_ERROR("failed to create port pool");
         return MMAL_ENOMEM;
      }
      module->has_pool = 1;
   }

   if (module->connected)
   {
      /* The connected port won't be enabled explicitly so make sure we apply
       * the buffer requirements now. */
      status = mmal_vc_port_requirements_set(module->connected);
      if (status != MMAL_SUCCESS)
         goto error;
   }

   msg.component_handle = module->component_handle;
   msg.action = MMAL_WORKER_PORT_ACTION_ENABLE;
   msg.port_handle = module->port_handle;
   msg.param.enable.port = *port;

   status = mmal_vc_sendwait_message(mmal_vc_get_client(), &msg.header, sizeof(msg),
                                     MMAL_WORKER_PORT_ACTION, &reply, &replylen, MMAL_FALSE);
   if (status == MMAL_SUCCESS)
   {
      vcos_assert(replylen == sizeof(reply));
      status = reply.status;
   }
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("failed to enable port %s: %s",
               port->name, mmal_status_to_string(status));
      goto error;
   }

   if (module->connected)
      mmal_vc_port_info_get(module->connected);

   return MMAL_SUCCESS;

 error:
   if (module->has_pool)
      vcos_blockpool_delete(&module->pool);
   return status;
}

/** Disable processing on a port */
static MMAL_STATUS_T mmal_vc_port_disable(MMAL_PORT_T *port)
{
   MMAL_PORT_MODULE_T *module = port->priv->module;
   MMAL_STATUS_T status;
   mmal_worker_reply reply;
   mmal_worker_port_action msg;
   size_t replylen = sizeof(reply);

   msg.component_handle = module->component_handle;
   msg.action = MMAL_WORKER_PORT_ACTION_DISABLE;
   msg.port_handle = module->port_handle;

   status = mmal_vc_sendwait_message(mmal_vc_get_client(), &msg.header, sizeof(msg),
                                     MMAL_WORKER_PORT_ACTION, &reply, &replylen, MMAL_FALSE);
   if (status == MMAL_SUCCESS)
   {
      vcos_assert(replylen == sizeof(reply));
      status = reply.status;
   }
   if (status != MMAL_SUCCESS)
      LOG_ERROR("failed to disable port - reason %d", status);

   if (module->has_pool)
   {
      /* MMAL server should make sure that all buffers are sent back before it
       * disables the port. */
      vcos_assert(vcos_blockpool_available_count(&module->pool) == port->buffer_num);
      vcos_blockpool_delete(&module->pool);
      module->has_pool = 0;
   }

   /* We need to make sure all the queued callbacks have been done */
   while (mmal_queue_length(port->component->priv->module->callback_queue))
      mmal_vc_do_callback(port->component);

   if (module->connected)
      mmal_vc_port_info_get(module->connected);

   return status;
}

/** Flush a port using MMAL_WORKER_PORT_ACTION - when the port is zero-copy or no data has been sent */
static MMAL_STATUS_T mmal_vc_port_flush_normal(MMAL_PORT_T *port)
{
   MMAL_PORT_MODULE_T *module = port->priv->module;
   MMAL_STATUS_T status;
   mmal_worker_reply reply;
   mmal_worker_port_action msg;
   size_t replylen = sizeof(reply);

   msg.component_handle = module->component_handle;
   msg.action = MMAL_WORKER_PORT_ACTION_FLUSH;
   msg.port_handle = module->port_handle;

   status = mmal_vc_sendwait_message(mmal_vc_get_client(), &msg.header, sizeof(msg),
                                     MMAL_WORKER_PORT_ACTION, &reply, &replylen, MMAL_FALSE);
   if (status == MMAL_SUCCESS)
   {
      vcos_assert(replylen == sizeof(reply));
      status = reply.status;
   }
   if (status != MMAL_SUCCESS)
      LOG_ERROR("failed to disable port - reason %d", status);

   return status;
}

/** Flush a port using PORT_FLUSH - generates a dummy bulk transfer to keep it in sync
  * with buffers being passed using bulk transfer */
static MMAL_STATUS_T mmal_vc_port_flush_sync(MMAL_PORT_T *port)
{
   MMAL_PORT_MODULE_T *module = port->priv->module;
   MMAL_STATUS_T status;
   mmal_worker_reply reply;
   MMAL_VC_CLIENT_BUFFER_CONTEXT_T client_context;
   mmal_worker_buffer_from_host *msg;

   size_t replylen = sizeof(reply);

   msg = &client_context.msg;

   client_context.magic = MMAL_MAGIC;
   client_context.port = port;

   msg->drvbuf.client_context = &client_context;
   msg->drvbuf.component_handle = module->component_handle;
   msg->drvbuf.port_handle = module->port_handle;
   msg->drvbuf.magic = MMAL_MAGIC;

   status = mmal_vc_sendwait_message(mmal_vc_get_client(), &msg->header, sizeof(*msg),
                                     MMAL_WORKER_PORT_FLUSH, &reply, &replylen, MMAL_TRUE);
   if (status == MMAL_SUCCESS)
   {
      vcos_assert(replylen == sizeof(reply));
      status = reply.status;
   }
   if (status != MMAL_SUCCESS)
      LOG_ERROR("failed to disable port - reason %d", status);

   return status;
}

/** Flush a port */
static MMAL_STATUS_T mmal_vc_port_flush(MMAL_PORT_T *port)
{
   static MMAL_PORT_FLUSH_CHECK_T is_port_flush_compatible = PORT_FLUSH_NOT_INITIALIZED;
   uint32_t major = 0, minor = 0, minimum = 0;
   MMAL_STATUS_T status;
   /* Buffers sent to videocore, if not zero-copy, use vchiq bulk transfers to copy the data.
      A flush could be sent while one of these buffers is being copied. If the normal flushing method
      is used, the flush can arrive before the buffer, which causes confusion when a pre-flush buffer
      arrives after the flush. So use a special flush mode that uses a dummy vchiq transfer to synchronise
      things.
      If data has never been sent on the port, then we don't need to worry about a flush overtaking data.
      In that case, the port may not actually be set up on the other end to receive bulk transfers, so use
      the normal flushing mechanism in that case.
    */

   if (port->priv->module->is_zero_copy || !port->priv->module->sent_data_on_port)
      return mmal_vc_port_flush_normal(port);

   if (is_port_flush_compatible == PORT_FLUSH_NOT_INITIALIZED)
   {
      status = mmal_vc_get_version(&major, &minor, &minimum);
      if (major >= 15)
      {
         is_port_flush_compatible = PORT_FLUSH_COMPATIBLE;
      }
      else
      {
         LOG_ERROR("Version number of MMAL Server incompatible. Required Major:14 Minor: 2 \
          or Greater. Current Major %d , Minor %d",major,minor);
         is_port_flush_compatible = PORT_FLUSH_INCOMPATIBLE;
      }
   }

   if (is_port_flush_compatible == PORT_FLUSH_COMPATIBLE)
      return mmal_vc_port_flush_sync(port);
   else
      return mmal_vc_port_flush_normal(port);
}

/** Connect 2 ports together */
static MMAL_STATUS_T mmal_vc_port_connect(MMAL_PORT_T *port, MMAL_PORT_T *other_port)
{
   MMAL_PORT_MODULE_T *module = port->priv->module;
   MMAL_STATUS_T status;
   mmal_worker_reply reply;
   mmal_worker_port_action msg;
   size_t replylen = sizeof(reply);

   /* We only support connecting vc components together */
   if (other_port && port->priv->pf_enable != other_port->priv->pf_enable)
      return MMAL_ENOSYS;

   /* Send the request to the video side */
   msg.component_handle = module->component_handle;
   msg.action = other_port ? MMAL_WORKER_PORT_ACTION_CONNECT : MMAL_WORKER_PORT_ACTION_DISCONNECT;
   msg.port_handle = module->port_handle;
   if (other_port)
   {
      msg.param.connect.component_handle = other_port->priv->module->component_handle;
      msg.param.connect.port_handle = other_port->priv->module->port_handle;
   }

   status = mmal_vc_sendwait_message(mmal_vc_get_client(), &msg.header, sizeof(msg),
                                     MMAL_WORKER_PORT_ACTION, &reply, &replylen, MMAL_FALSE);
   if (status == MMAL_SUCCESS)
   {
      vcos_assert(replylen == sizeof(reply));
      status = reply.status;
   }

   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("failed to connect ports: %s", mmal_status_to_string(status));
      return status;
   }

   if (other_port)
   {
      /* Connection */
      module->connected = other_port;
      other_port->priv->module->connected = port;
   }
   else
   {
      /* Disconnection */
      if (module->connected)
         module->connected->priv->module->connected = NULL;
      module->connected = NULL;
   }

   return MMAL_SUCCESS;
}

/*****************************************************************************/
static void mmal_vc_do_callback(MMAL_COMPONENT_T *component)
{
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_BUFFER_HEADER_T *buffer;
   MMAL_PORT_T *port;

   /* Get a buffer from this port */
   buffer = mmal_queue_get(module->callback_queue);
   if (!buffer)
      return; /* Will happen when a port gets disabled */

   port = (MMAL_PORT_T *)buffer->priv->component_data;

   /* Catch and report any transmission error */
   if (buffer->flags & MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED)
       mmal_event_error_send(port->component, MMAL_EIO);

   /* Events generated by this component are handled differently */
   if (mmal_buffer_header_driver_data(buffer)->client_context ==
       &component->priv->module->event_ctx)
   {
      mmal_port_event_send(port, buffer);
      return;
   }

   buffer->data = mmal_vc_shm_lock(buffer->data, port->priv->module->zero_copy_workaround);
   mmal_port_buffer_header_callback(port, buffer);
}

static void mmal_vc_do_callback_loop(MMAL_COMPONENT_T *component)
{
   while (mmal_queue_length(component->priv->module->callback_queue))
      mmal_vc_do_callback(component);
}

/** Called back from VCHI(Q) event handler when buffers come back from the copro.
 *
 * The message points to the message sent by videocore, and which should have
 * a pointer back to our original client side context.
 *
 */
static void mmal_vc_port_send_callback(mmal_worker_buffer_from_host *msg)
{
   MMAL_BUFFER_HEADER_T *buffer;
   MMAL_PORT_T *port;
   MMAL_VC_CLIENT_BUFFER_CONTEXT_T *client_context = msg->drvbuf.client_context;

   vcos_assert(client_context);
   vcos_assert(client_context->magic == MMAL_MAGIC);

   buffer = client_context->buffer;
   port = client_context->port;
   vcos_blockpool_free(msg->drvbuf.client_context);

   vcos_assert(port->priv->module->magic == MMAL_MAGIC);
   mmal_vc_msg_to_buffer_header(buffer, msg);

   /* Queue the callback so it is delivered by the action thread */
   buffer->priv->component_data = (void *)port;
   mmal_queue_put(port->component->priv->module->callback_queue, buffer);
   mmal_component_action_trigger(port->component);
}

static void mmal_vc_port_send_event_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   /* Queue the event to be delivered by the action thread */
   buffer->priv->component_data = (void *)port;
   mmal_queue_put(port->component->priv->module->callback_queue, buffer);
   mmal_component_action_trigger(port->component);
}

/** Called from the client to send a buffer (empty or full) to
  * the copro.
  */
static MMAL_STATUS_T mmal_vc_port_send(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   MMAL_PORT_MODULE_T *module = port->priv->module;
   MMAL_STATUS_T status;
   MMAL_VC_CLIENT_BUFFER_CONTEXT_T *client_context;
   mmal_worker_buffer_from_host *msg;
   uint32_t length;
   uint32_t msgid = MMAL_WORKER_BUFFER_FROM_HOST;
   uint32_t major = 0, minor = 0, minimum = 0;
   static MMAL_ZEROLEN_CHECK_T is_vc_zerolength_compatible = ZEROLEN_NOT_INITIALIZED;

   vcos_assert(port);
   vcos_assert(module);
   vcos_assert(module->magic == MMAL_MAGIC);

   /* Handle event buffers */
   if (buffer->cmd)
   {
      MMAL_EVENT_FORMAT_CHANGED_T *event = mmal_event_format_changed_get(buffer);
      if (event)
      {
         mmal_format_copy(port->format, event->format);
         status = port->priv->pf_set_format(port);
         if(status != MMAL_SUCCESS)
            LOG_ERROR("format not set on port %p", port);
      }
      else
      {
         LOG_ERROR("discarding event %i on port %p", (int)buffer->cmd, port);
      }

      buffer->length = 0;
      mmal_port_buffer_header_callback(port, buffer);
      return MMAL_SUCCESS;
   }

   /* We can only send buffers if we have a pool */
   if (!module->has_pool)
   {
      LOG_ERROR("no pool on port %p", port);
      return MMAL_EINVAL;
   }

   client_context = vcos_blockpool_alloc(&module->pool);
   if(!client_context)
   {
      LOG_INFO("couldn't allocate client buffer context from pool");
      return MMAL_ENOMEM;
   }
   msg = &client_context->msg;

   client_context->magic = MMAL_MAGIC;
   client_context->buffer = buffer;
   client_context->callback = mmal_vc_port_send_callback;
   client_context->callback_event = NULL;
   client_context->port = port;

   msg->drvbuf.client_context = client_context;
   msg->drvbuf.component_handle = module->component_handle;
   msg->drvbuf.port_handle = module->port_handle;
   msg->drvbuf.magic = MMAL_MAGIC;

   length = buffer->length;

   if (length <= MMAL_VC_SHORT_DATA && !port->priv->module->is_zero_copy &&
       (port->format->encoding == MMAL_ENCODING_OPAQUE ||
        port->type == MMAL_PORT_TYPE_CLOCK))
   {
      memcpy(msg->short_data, buffer->data + buffer->offset, buffer->length);
      msg->payload_in_message = length;
      length = 0;
   }
   else
   {
      msg->payload_in_message = 0;
   }

   buffer->data =
      mmal_vc_shm_unlock(buffer->data, &length, port->priv->module->zero_copy_workaround);
   mmal_vc_buffer_header_to_msg(msg, buffer);

   if (!VCOS_BLOCKPOOL_IS_VALID_HANDLE_FORMAT(msg->drvbuf.component_handle, 256))
   {
      LOG_ERROR("bad component handle 0x%x", msg->drvbuf.component_handle);
      return MMAL_EINVAL;
   }

   if (msg->drvbuf.port_handle > 255)
   {
      LOG_ERROR("bad port handle 0x%x", msg->drvbuf.port_handle);
      return MMAL_EINVAL;
   }

   if (module->is_zero_copy)
      length = 0;

   if (is_vc_zerolength_compatible == ZEROLEN_NOT_INITIALIZED)
   {
      status = mmal_vc_get_version(&major, &minor, &minimum);
      if ((major > 12 ) || ((major == 12) && (minor >= 2)))
      {
         is_vc_zerolength_compatible = ZEROLEN_COMPATIBLE;
      }
      else
      {
         LOG_ERROR("Version number of MMAL Server incompatible. Required Major:12 Minor: 2 \
          or Greater. Current Major %d , Minor %d",major,minor);
         is_vc_zerolength_compatible = ZEROLEN_INCOMPATIBLE;
      }
   }

   if ((is_vc_zerolength_compatible == ZEROLEN_COMPATIBLE) && !(module->is_zero_copy) && !length
       && (msg->buffer_header.flags & MMAL_BUFFER_HEADER_FLAG_EOS))
   {
      length = 8;
      msgid = MMAL_WORKER_BUFFER_FROM_HOST_ZEROLEN;
   }

   if (length)
   {
      // We're doing a bulk transfer. Note this so that flushes know
      // they need to use the more cumbersome fake-bulk-transfer mechanism
      // to guarantee correct ordering.
      port->priv->module->sent_data_on_port = MMAL_TRUE;

      // Data will be received at the start of the destination buffer, so fixup
      // the offset in the destination buffer header.
      msg->buffer_header.offset = 0;
   }

   status = mmal_vc_send_message(mmal_vc_get_client(), &msg->header, sizeof(*msg),
                                 buffer->data + buffer->offset, length,
                                 msgid);
   if (status != MMAL_SUCCESS)
   {
      LOG_INFO("failed %d", status);
      vcos_blockpool_free(msg->drvbuf.client_context);
      buffer->data = mmal_vc_shm_lock(buffer->data, port->priv->module->zero_copy_workaround);
   }

   return status;
}

static MMAL_STATUS_T mmal_vc_component_disable(MMAL_COMPONENT_T *component)
{
   MMAL_STATUS_T status;
   mmal_worker_reply reply;
   mmal_worker_component_disable msg;
   size_t replylen = sizeof(reply);

   vcos_assert(component && component->priv && component->priv->module);

   msg.component_handle = component->priv->module->component_handle;

   status = mmal_vc_sendwait_message(mmal_vc_get_client(), &msg.header, sizeof(msg),
         MMAL_WORKER_COMPONENT_DISABLE,
         &reply, &replylen, MMAL_FALSE);

   if (status == MMAL_SUCCESS)
   {
      vcos_assert(replylen == sizeof(reply));
      status = reply.status;
   }

   if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
   {
      LOG_ERROR("failed to disable component - reason %d", status);
      goto fail;
   }

   return status;
fail:
   return status;
}

static MMAL_STATUS_T mmal_vc_component_enable(MMAL_COMPONENT_T *component)
{
   MMAL_STATUS_T status;
   mmal_worker_reply reply;
   mmal_worker_component_enable msg;
   size_t replylen = sizeof(reply);

   vcos_assert(component && component->priv && component->priv->module);

   msg.component_handle = component->priv->module->component_handle;

   status = mmal_vc_sendwait_message(mmal_vc_get_client(), &msg.header, sizeof(msg),
                                     MMAL_WORKER_COMPONENT_ENABLE, &reply, &replylen, MMAL_FALSE);

   if (status == MMAL_SUCCESS)
   {
      vcos_assert(replylen == sizeof(reply));
      status = reply.status;
   }

   if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
   {
      LOG_ERROR("failed to enable component: %s", mmal_status_to_string(status));
      return status;
   }

   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmal_vc_component_destroy(MMAL_COMPONENT_T *component)
{
   MMAL_STATUS_T status;
   mmal_worker_component_destroy msg;
   mmal_worker_reply reply;
   size_t replylen = sizeof(reply);

   vcos_assert(component && component->priv && component->priv->module);

   msg.component_handle = component->priv->module->component_handle;

   status = mmal_vc_sendwait_message(mmal_vc_get_client(), &msg.header, sizeof(msg),
         MMAL_WORKER_COMPONENT_DESTROY,
         &reply, &replylen, MMAL_FALSE);

   if (status == MMAL_SUCCESS)
   {
      vcos_assert(replylen == sizeof(reply));
      status = reply.status;
   }
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("failed to destroy component - reason %d", status );
      goto fail;
   }

   if(component->input_num)
      mmal_ports_free(component->input, component->input_num);
   if(component->output_num)
      mmal_ports_free(component->output, component->output_num);
   if(component->clock_num)
      mmal_ports_free(component->clock, component->clock_num);

   mmal_queue_destroy(component->priv->module->callback_queue);

   vcos_free(component->priv->module);
   component->priv->module = NULL;

fail:
   // no longer require videocore
   mmal_vc_release();
   mmal_vc_deinit();
   return status;
}

MMAL_STATUS_T mmal_vc_consume_mem(size_t size, uint32_t *handle)
{
   MMAL_STATUS_T status;
   mmal_worker_consume_mem req;
   mmal_worker_consume_mem reply;
   size_t len = sizeof(reply);

   req.size = (uint32_t) size;

   status = mmal_vc_sendwait_message(mmal_vc_get_client(),
                                     &req.header, sizeof(req),
                                     MMAL_WORKER_CONSUME_MEM,
                                     &reply, &len, MMAL_FALSE);
   if (status == MMAL_SUCCESS)
   {
      vcos_assert(len == sizeof(reply));
      status = reply.status;
      *handle = reply.handle;
   }
   return status;
}

MMAL_STATUS_T mmal_vc_compact(MMAL_VC_COMPACT_MODE_T mode, uint32_t *duration)
{
   MMAL_STATUS_T status;
   mmal_worker_compact req;
   mmal_worker_compact reply;
   size_t len = sizeof(reply);

   req.mode = (uint32_t)mode;
   status = mmal_vc_sendwait_message(mmal_vc_get_client(),
                                     &req.header, sizeof(req),
                                     MMAL_WORKER_COMPACT,
                                     &reply, &len, MMAL_FALSE);
   if (status == MMAL_SUCCESS)
   {
      vcos_assert(len == sizeof(reply));
      status = reply.status;
      *duration = reply.duration;
   }
   return status;
}

MMAL_STATUS_T mmal_vc_lmk(uint32_t alloc_size)
{
   MMAL_STATUS_T status;
   mmal_worker_lmk req;
   mmal_worker_lmk reply;
   size_t len = sizeof(reply);

   req.alloc_size = alloc_size;

   status = mmal_vc_sendwait_message(mmal_vc_get_client(),
                                     &req.header, sizeof(req),
                                     MMAL_WORKER_LMK,
                                     &reply, &len, MMAL_FALSE);
   return status;
}

MMAL_STATUS_T mmal_vc_host_log(const char *msg)
{
   MMAL_STATUS_T status = MMAL_EINVAL;
   if (msg)
   {
      mmal_worker_host_log req;
      mmal_worker_reply reply;
      size_t replylen = sizeof(reply);
      size_t msg_len = vcos_safe_strcpy(req.msg, msg, sizeof(req.msg), 0);

      /* Reduce the length if it is shorter than the max message length */
      status = mmal_vc_sendwait_message(mmal_vc_get_client(), &req.header,
            sizeof(req) - sizeof(req.msg) + vcos_min(sizeof(req.msg), msg_len + 1),
            MMAL_WORKER_HOST_LOG,
            &reply, &replylen, MMAL_FALSE);

      if (status == MMAL_SUCCESS)
      {
         vcos_assert(replylen == sizeof(reply));
         status = reply.status;
      }
   }
   return status;
}

MMAL_STATUS_T mmal_vc_get_core_stats(MMAL_CORE_STATISTICS_T *stats,
                                     MMAL_STATS_RESULT_T *result,
                                     char *name,
                                     size_t namelen,
                                     MMAL_PORT_TYPE_T type,
                                     unsigned component_index,
                                     unsigned port_index,
                                     MMAL_CORE_STATS_DIR dir,
                                     MMAL_BOOL_T reset)
{
   mmal_worker_get_core_stats_for_port req;
   mmal_worker_get_core_stats_for_port_reply reply;
   MMAL_STATUS_T status;
   size_t len = sizeof(reply);

   req.component_index = component_index;
   req.port_index = port_index;
   req.type = type;
   req.reset = reset;
   req.dir = dir;

   status = mmal_vc_sendwait_message(mmal_vc_get_client(),
                                     &req.header, sizeof(req),
                                     MMAL_WORKER_GET_CORE_STATS_FOR_PORT,
                                     &reply, &len, MMAL_FALSE);

   if (status == MMAL_SUCCESS)
   {
      vcos_assert(len == sizeof(reply));
      *stats = reply.stats;
      *result = reply.result;
      strncpy(name, reply.component_name, namelen);
      name[namelen-1] = '\0';
   }
   return status;
}

/** Get port context data. */
static MMAL_STATUS_T mmal_vc_port_info_get(MMAL_PORT_T *port)
{
   MMAL_PORT_MODULE_T *module = port->priv->module;
   mmal_worker_port_info_get msg;
   mmal_worker_port_info reply;
   size_t replylen = sizeof(reply);
   MMAL_STATUS_T status;

   msg.component_handle = module->component_handle;
   msg.port_type = port->type;
   msg.index = port->index;

   LOG_TRACE("get port info (%i:%i)", port->type, port->index);

   status = mmal_vc_sendwait_message(mmal_vc_get_client(), &msg.header, sizeof(msg),
                                     MMAL_WORKER_PORT_INFO_GET, &reply, &replylen, MMAL_FALSE);
   if (status == MMAL_SUCCESS)
   {
      vcos_assert(replylen == sizeof(reply));
      status = reply.status;
   }

   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("failed to get port info (%i:%i): %s", port->type, port->index,
                mmal_status_to_string(status));
      return status;
   }

   module->port_handle = reply.port_handle;
   port->buffer_num_min = reply.port.buffer_num_min;
   port->buffer_num_recommended = reply.port.buffer_num_recommended;
   port->buffer_num = reply.port.buffer_num;
   port->buffer_size_min = reply.port.buffer_size_min;
   port->buffer_size_recommended = reply.port.buffer_size_recommended;
   port->buffer_size = reply.port.buffer_size;
   port->buffer_alignment_min = reply.port.buffer_alignment_min;
   port->is_enabled = reply.port.is_enabled;
   port->capabilities = reply.port.capabilities;
   reply.format.extradata = port->format->extradata;
   reply.format.es = port->format->es;
   *port->format = reply.format;
   *port->format->es = reply.es;
   if(port->format->extradata_size)
   {
      status = mmal_format_extradata_alloc(port->format, port->format->extradata_size);
      if(status != MMAL_SUCCESS)
      {
         vcos_assert(0);
         port->format->extradata_size = 0;
         LOG_ERROR("couldn't allocate extradata %i", port->format->extradata_size);
         return MMAL_ENOMEM;
      }
      memcpy(port->format->extradata, reply.extradata, port->format->extradata_size);
   }

   return MMAL_SUCCESS;
}

/** Set port context data. */
static MMAL_STATUS_T mmal_vc_port_info_set(MMAL_PORT_T *port)
{
   MMAL_PORT_MODULE_T *module = port->priv->module;
   mmal_worker_port_info_set msg;
   mmal_worker_port_info reply;
   size_t replylen = sizeof(reply);
   MMAL_STATUS_T status;

   msg.component_handle = module->component_handle;
   msg.port_type = port->type;
   msg.index = port->index;
   msg.port = *port;
   msg.format = *port->format;
   msg.es = *port->format->es;
   if(msg.format.extradata_size > MMAL_FORMAT_EXTRADATA_MAX_SIZE)
   {
      vcos_assert(0);
      msg.format.extradata_size = MMAL_FORMAT_EXTRADATA_MAX_SIZE;
   }
   memcpy(msg.extradata, msg.format.extradata, msg.format.extradata_size);

   LOG_TRACE("set port info (%i:%i)", port->type, port->index);

   status = mmal_vc_sendwait_message(mmal_vc_get_client(), &msg.header, sizeof(msg),
                                     MMAL_WORKER_PORT_INFO_SET, &reply, &replylen, MMAL_FALSE);
   if (status == MMAL_SUCCESS)
   {
      vcos_assert(replylen == sizeof(reply));
      status = reply.status;
   }

   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("failed to set port info (%i:%i): %s", port->type, port->index,
                mmal_status_to_string(status));
      return status;
   }

   port->buffer_num_min = reply.port.buffer_num_min;
   port->buffer_num_recommended = reply.port.buffer_num_recommended;
   port->buffer_num = reply.port.buffer_num;
   port->buffer_size_min = reply.port.buffer_size_min;
   port->buffer_size_recommended = reply.port.buffer_size_recommended;
   port->buffer_size = reply.port.buffer_size;
   port->buffer_alignment_min = reply.port.buffer_alignment_min;
   port->is_enabled = reply.port.is_enabled;
   port->capabilities = reply.port.capabilities;
   reply.format.extradata = port->format->extradata;
   reply.format.es = port->format->es;
   *port->format = reply.format;
   *port->format->es = reply.es;
   if(port->format->extradata_size)
   {
      status = mmal_format_extradata_alloc(port->format, port->format->extradata_size);
      if(status != MMAL_SUCCESS)
      {
         vcos_assert(0);
         port->format->extradata_size = 0;
         LOG_ERROR("couldn't allocate extradata %i", port->format->extradata_size);
         return MMAL_ENOMEM;
      }
      memcpy(port->format->extradata, reply.extradata, port->format->extradata_size);
   }

   return MMAL_SUCCESS;
}

/** Set format on a port */
static MMAL_STATUS_T mmal_vc_port_set_format(MMAL_PORT_T *port)
{
   MMAL_COMPONENT_T *component = port->component;
   MMAL_COMPONENT_MODULE_T *module = component->priv->module;
   MMAL_STATUS_T status;
   unsigned int i;

   status = mmal_vc_port_info_set(port);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("mmal_vc_port_info_set failed %p (%s)", port,
                mmal_status_to_string(status));
      return status;
   }

   /* Get the setting back for this port */
   status = mmal_vc_port_info_get(port);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("mmal_vc_port_info_get failed %p (%s)", port,
                mmal_status_to_string(status));
      return status;
   }

   /* Get the settings for the output ports in case they have changed */
   if (port->type == MMAL_PORT_TYPE_INPUT)
   {
      for (i = 0; i < module->ports_num; i++)
      {
         if (module->ports[i]->port->type != MMAL_PORT_TYPE_OUTPUT)
            continue;

         status = mmal_vc_port_info_get(module->ports[i]->port);
         if (status != MMAL_SUCCESS)
         {
            LOG_ERROR("mmal_vc_port_info_get failed %p (%i)",
                      module->ports[i]->port, status);
            return status;
         }
      }
   }

   return MMAL_SUCCESS;
}

/** Set parameter on a port */
static MMAL_STATUS_T mmal_vc_port_parameter_set(MMAL_PORT_T *port, const MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_PORT_MODULE_T *module = port->priv->module;
   MMAL_STATUS_T status;
   mmal_worker_port_param_set msg;
   size_t msglen = MMAL_OFFSET(mmal_worker_port_param_set, param) + param->size;
   mmal_worker_reply reply;
   size_t replylen = sizeof(reply);

   if(param->size > MMAL_WORKER_PORT_PARAMETER_SET_MAX)
   {
      LOG_ERROR("parameter too large (%u > %u)", param->size, MMAL_WORKER_PORT_PARAMETER_SET_MAX);
      return MMAL_ENOSPC;
   }

   /* Intercept the zero copy parameter */
   if (param->id == MMAL_PARAMETER_ZERO_COPY &&
       param->size >= sizeof(MMAL_PARAMETER_BOOLEAN_T) )
   {
      module->is_zero_copy = !!((MMAL_PARAMETER_BOOLEAN_T *)param)->enable;
      module->zero_copy_workaround = ((MMAL_PARAMETER_BOOLEAN_T *)param)->enable == 0xBEEF;
      LOG_DEBUG("%s zero copy on port %p", module->is_zero_copy ? "enable" : "disable", port);
   }

   msg.component_handle = module->component_handle;
   msg.port_handle = module->port_handle;
   /* coverity[overrun-buffer-arg] */
   memcpy(&msg.param, param, param->size);

   status = mmal_vc_sendwait_message(mmal_vc_get_client(), &msg.header, msglen,
                                     MMAL_WORKER_PORT_PARAMETER_SET, &reply, &replylen, MMAL_FALSE);

   if (status == MMAL_SUCCESS)
   {
      vcos_assert(replylen == sizeof(reply));
      status = reply.status;
   }
   if (status != MMAL_SUCCESS)
   {
      LOG_WARN("failed to set port parameter %u:%u %u:%u %s", msg.component_handle, msg.port_handle,
            param->id, param->size, mmal_status_to_string(status));
      return status;
   }

   if (param->id == MMAL_PARAMETER_BUFFER_REQUIREMENTS)
   {
      /* This might have changed the buffer requirements of other ports so fetch them all */
      MMAL_COMPONENT_T *component = port->component;
      unsigned int i;
      for (i = 0; status == MMAL_SUCCESS && i < component->input_num; i++)
         status = mmal_vc_port_requirements_get(component->input[i]);
      for (i = 0; status == MMAL_SUCCESS && i < component->output_num; i++)
         status = mmal_vc_port_requirements_get(component->output[i]);
   }

   return status;
}

/** Get parameter on a port */
static MMAL_STATUS_T mmal_vc_port_parameter_get(MMAL_PORT_T *port, MMAL_PARAMETER_HEADER_T *param)
{
   MMAL_PORT_MODULE_T *module = port->priv->module;
   MMAL_STATUS_T status;
   mmal_worker_port_param_get msg;
   size_t msglen = MMAL_OFFSET(mmal_worker_port_param_get, param) + param->size;
   mmal_worker_port_param_get_reply reply;
   size_t replylen = MMAL_OFFSET(mmal_worker_port_param_get_reply, param) + param->size;

   if(param->size > MMAL_WORKER_PORT_PARAMETER_GET_MAX)
   {
      LOG_ERROR("parameter too large (%u > %u) id %u", param->size,
            MMAL_WORKER_PORT_PARAMETER_GET_MAX, param->id);
      return MMAL_ENOMEM;
   }

   msg.component_handle = module->component_handle;
   msg.port_handle = module->port_handle;
   memcpy(&msg.param, param, param->size);

   status = mmal_vc_sendwait_message(mmal_vc_get_client(), &msg.header, msglen,
                                     MMAL_WORKER_PORT_PARAMETER_GET, &reply, &replylen, MMAL_FALSE);
   if (status == MMAL_SUCCESS)
   {
      status = reply.status;
      /* Reply must include the parameter header */
      vcos_assert(replylen >= MMAL_OFFSET(mmal_worker_port_param_get_reply, space));

      /* If the call fails with MMAL_ENOSPC then reply.param.size is set to the size required for
       * the call to succeed, and that may be bigger than the buffers, so only check these asserts
       * if the call succeeded.
       */
      if ( status == MMAL_SUCCESS )
      {
         /* Reply mustn't be bigger than the parameter given */
         vcos_assert(replylen <= (MMAL_OFFSET(mmal_worker_port_param_get_reply, param) + param->size));
         /* Reply must be consistent with the parameter size embedded in it */
         vcos_assert(replylen == (MMAL_OFFSET(mmal_worker_port_param_get_reply, param) + reply.param.size));
      }
   }

   if (status != MMAL_SUCCESS && status != MMAL_ENOSPC)
   {
      LOG_WARN("failed to get port parameter %u:%u %u:%u %s", msg.component_handle, msg.port_handle,
            param->id, param->size, mmal_status_to_string(status));
      return status;
   }

   if (status == MMAL_ENOSPC)
   {
      /* Copy only as much as we have space for but report true size of parameter */
      /* coverity[overrun-buffer-arg] */
      memcpy(param, &reply.param, param->size);
      param->size = reply.param.size;
   }
   else
   {
      memcpy(param, &reply.param, reply.param.size);
   }

   return status;
}

static uint8_t *mmal_vc_port_payload_alloc(MMAL_PORT_T *port, uint32_t payload_size)
{
   MMAL_PORT_MODULE_T *module = port->priv->module;
   MMAL_BOOL_T can_deref = MMAL_TRUE;
   char buf[5];
   void *ret;
   (void)buf;

   LOG_TRACE("%s: allocating %d bytes, format %s, is_zero_copy %d",
             port->name,
             payload_size,
             mmal_4cc_to_string(buf, sizeof(buf), port->format->encoding),
             module->is_zero_copy);

   if (port->format->encoding == MMAL_ENCODING_OPAQUE &&
       module->is_zero_copy)
   {
      MMAL_OPAQUE_IMAGE_HANDLE_T h = mmal_vc_opaque_alloc_desc(port->name);
      can_deref = MMAL_FALSE;
      ret = (void*)h;
      if (!ret)
      {
         LOG_ERROR("%s: failed to allocate %d bytes opaque memory",
                   port->name, payload_size);
         return NULL;
      }
      module->opaque_allocs++;
   }

   else if (module->is_zero_copy)
   {
      ret = mmal_vc_shm_alloc(payload_size);
      if (!ret)
      {
         LOG_ERROR("%s: failed to allocate %d bytes of shared memory",
                   port->name, payload_size);
         return NULL;
      }
   }

   else
   {
      /* Allocate conventional memory */
      ret = vcos_calloc(1, payload_size, "mmal_vc_port payload");
      if (!ret)
      {
         LOG_ERROR("could not allocate %i bytes", (int)payload_size);
         return NULL;
      }
   }

   /* Ensure that newly minted opaque buffers are always in a sensible
    * state, and don't have random garbage in them.
    */
   if (can_deref && port->format->encoding == MMAL_ENCODING_OPAQUE)
      memset(ret, 0, payload_size);

   LOG_DEBUG("%s: allocated at %p", port->name, ret);
   return ret;
}

static void mmal_vc_port_payload_free(MMAL_PORT_T *port, uint8_t *payload)
{
   MMAL_PORT_MODULE_T *module = port->priv->module;

   if (module->opaque_allocs)
   {
      module->opaque_allocs--;
      mmal_vc_opaque_release((MMAL_OPAQUE_IMAGE_HANDLE_T)payload);
      return;
   }

   else if (mmal_vc_shm_free(payload) == MMAL_SUCCESS)
      return;

   /* We're dealing with conventional memory */
   vcos_free(payload);
}

/** Create a component given its name. */
static MMAL_STATUS_T mmal_vc_component_create(const char *name, MMAL_COMPONENT_T *component)
{
   MMAL_STATUS_T status;
   const char *basename;
   mmal_worker_component_create msg;
   mmal_worker_component_create_reply reply;
   size_t replylen = sizeof(reply);
   MMAL_COMPONENT_MODULE_T *module = NULL;
   unsigned int ports_num, i;

   LOG_TRACE("%s", name);

   if (strstr(name, VIDEOCORE_PREFIX ".") != name)
      return MMAL_ENOSYS;

   basename = name + sizeof(VIDEOCORE_PREFIX ".") - 1;
   if (strlen(basename) >= sizeof(msg.name)-1)
   {
      vcos_assert(0);
      return MMAL_EINVAL;
   }

   msg.client_component = component;
   /* coverity[secure_coding] Length tested above */
   strcpy(msg.name, basename);
#ifdef __linux__
   msg.pid = getpid();
#endif

   status = mmal_vc_init();
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("failed to initialise mmal ipc for '%s' (%i:%s)",
                name, status, mmal_status_to_string(status));
      return status;
   }
   // claim VC for entire duration of component.
   status = mmal_vc_use();

   status = mmal_vc_sendwait_message(mmal_vc_get_client(), &msg.header, sizeof(msg),
                                     MMAL_WORKER_COMPONENT_CREATE, &reply, &replylen, MMAL_FALSE);

   vcos_log_info("%s: %s: handle 0x%x status %d reply status %d",
                 __FUNCTION__, name, reply.component_handle, status, reply.status);

   if (status == MMAL_SUCCESS)
   {
      vcos_assert(replylen == sizeof(reply));
      status = reply.status;
   }

   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("failed to create component '%s' (%i:%s)", name, status,
                mmal_status_to_string(status));
      mmal_vc_release();
      mmal_vc_deinit();
      return status;
   }

   /* Component has been created, allocate our context. */
   status = MMAL_ENOMEM;
   ports_num = 1 + reply.input_num + reply.output_num + reply.clock_num;
   module = vcos_calloc(1, sizeof(*module) + ports_num * sizeof(*module->ports), "mmal_vc_module");
   if (!module)
   {
      mmal_worker_component_destroy msg;
      mmal_worker_reply reply;
      size_t replylen = sizeof(reply);
      MMAL_STATUS_T destroy_status;

      destroy_status = mmal_vc_sendwait_message(mmal_vc_get_client(), &msg.header, sizeof(msg),
                               MMAL_WORKER_COMPONENT_DESTROY, &reply, &replylen, MMAL_FALSE);
      vcos_assert(destroy_status == MMAL_SUCCESS);
      mmal_vc_release();
      mmal_vc_deinit();
      return status;
   }
   module->ports = (MMAL_PORT_MODULE_T **)&module[1];
   module->component_handle = reply.component_handle;
   component->priv->module = module;

   /* Allocate our local ports. Control port reallocated to set module size. */
   mmal_port_free(component->control);
   component->control = mmal_port_alloc(component, MMAL_PORT_TYPE_CONTROL,
                                        sizeof(MMAL_PORT_MODULE_T));
   if (!component->control)
      goto fail;

   if (reply.input_num)
   {
      component->input = mmal_ports_alloc(component, reply.input_num, MMAL_PORT_TYPE_INPUT,
                                          sizeof(MMAL_PORT_MODULE_T));
      if (!component->input)
         goto fail;
   }
   component->input_num = reply.input_num;

   if (reply.output_num)
   {
      component->output = mmal_ports_alloc(component, reply.output_num, MMAL_PORT_TYPE_OUTPUT,
                                           sizeof(MMAL_PORT_MODULE_T));
      if (!component->output)
         goto fail;
   }
   component->output_num = reply.output_num;

   if (reply.clock_num)
   {
      component->clock = mmal_ports_alloc(component, reply.clock_num, MMAL_PORT_TYPE_CLOCK,
                                           sizeof(MMAL_PORT_MODULE_T));
      if (!component->clock)
         goto fail;
   }
   component->clock_num = reply.clock_num;

   /* We want to do the buffer callbacks to the client into a separate thread.
    * We'll need to queue these callbacks and have an action which does the actual callback. */
   module->callback_queue = mmal_queue_create();
   if (!module->callback_queue)
      goto fail;
   status = mmal_component_action_register(component, mmal_vc_do_callback_loop);
   if (status != MMAL_SUCCESS)
      goto fail;

   LOG_TRACE(" handle %i", reply.component_handle);

   module->ports[module->ports_num] = component->control->priv->module;
   module->ports[module->ports_num]->port = component->control;
   module->ports[module->ports_num]->component_handle = module->component_handle;
   module->ports_num++;

   for (i = 0; i < component->input_num; i++, module->ports_num++)
   {
      module->ports[module->ports_num] = component->input[i]->priv->module;
      module->ports[module->ports_num]->port = component->input[i];
      module->ports[module->ports_num]->component_handle = module->component_handle;
   }

   for (i = 0; i < component->output_num; i++, module->ports_num++)
   {
      module->ports[module->ports_num] = component->output[i]->priv->module;
      module->ports[module->ports_num]->port = component->output[i];
      module->ports[module->ports_num]->component_handle = module->component_handle;
   }

   for (i = 0; i < component->clock_num; i++, module->ports_num++)
   {
      module->ports[module->ports_num] = component->clock[i]->priv->module;
      module->ports[module->ports_num]->port = component->clock[i];
      module->ports[module->ports_num]->component_handle = module->component_handle;
   }

   /* Get the ports info */
   for (i = 0; i < module->ports_num; i++)
   {
      MMAL_PORT_T *port = module->ports[i]->port;
      port->priv->pf_set_format = mmal_vc_port_set_format;
      port->priv->pf_enable = mmal_vc_port_enable;
      port->priv->pf_disable = mmal_vc_port_disable;
      port->priv->pf_send = mmal_vc_port_send;
      port->priv->pf_flush = mmal_vc_port_flush;
      port->priv->pf_connect = mmal_vc_port_connect;
      port->priv->pf_parameter_set = mmal_vc_port_parameter_set;
      port->priv->pf_parameter_get = mmal_vc_port_parameter_get;
      port->priv->pf_payload_alloc = mmal_vc_port_payload_alloc;
      port->priv->pf_payload_free = mmal_vc_port_payload_free;
      port->priv->module->component_handle = module->component_handle;
      port->priv->module->magic = MMAL_MAGIC;

      status = mmal_vc_port_info_get(port);
      if (status != MMAL_SUCCESS)
         goto fail;
   }

   /* Initialise the vc client context which will be used for our event buffers */
   module->event_ctx_initialised = MMAL_FALSE;
   module->event_ctx.magic = MMAL_MAGIC;
   module->event_ctx.callback_event = mmal_vc_port_send_event_callback;

   /* populate component structure */
   component->priv->pf_enable = mmal_vc_component_enable;
   component->priv->pf_disable = mmal_vc_component_disable;
   component->priv->pf_destroy = mmal_vc_component_destroy;
   return MMAL_SUCCESS;

fail:
   mmal_vc_component_destroy(component);
   return status;
}

MMAL_CONSTRUCTOR(mmal_register_component_videocore);
void mmal_register_component_videocore(void)
{
   mmal_vc_shm_init();
   mmal_component_supplier_register(VIDEOCORE_PREFIX, mmal_vc_component_create);
}
