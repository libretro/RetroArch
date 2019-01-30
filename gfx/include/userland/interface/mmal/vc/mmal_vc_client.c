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
#include "mmal_vc_msgs.h"
#include "mmal_vc_api.h"
#include "mmal_vc_client_priv.h"
#include "interface/vcos/vcos.h"
#include "vchiq_util.h"
#include "interface/mmal/core/mmal_buffer_private.h"
#include "interface/mmal/core/mmal_component_private.h"
#include "interface/mmal/core/mmal_port_private.h"
#include "interface/mmal/util/mmal_list.h"
#include "interface/mmal/util/mmal_util.h"

#define VCOS_LOG_CATEGORY (&mmal_ipc_log_category)
#include "interface/mmal/mmal_logging.h"

#include <stdio.h>

#define MAX_WAITERS 16
static VCOS_ONCE_T once = VCOS_ONCE_INIT;
static VCHIQ_INSTANCE_T mmal_vchiq_instance;
static VCOS_LOG_CAT_T mmal_ipc_log_category;

/** Client threads use one of these to wait for
 * a reply from VideoCore.
 */
typedef struct MMAL_WAITER_T
{
   VCOS_SEMAPHORE_T sem;
   unsigned inuse;
   void *dest;                   /**< Where to write reply */
   size_t destlen;               /**< Max length for reply */
} MMAL_WAITER_T;

/** We have an array of waiters and allocate them to waiting
  * threads. They can be released back to the pool in any order.
  * If there are none free, the calling thread will block until
  * one becomes available.
  */
typedef struct 
{
   MMAL_WAITER_T waiters[MAX_WAITERS];
   VCOS_SEMAPHORE_T sem;
} MMAL_WAITPOOL_T;

struct MMAL_CLIENT_T
{
   int refcount;
   int usecount;
   VCOS_MUTEX_T lock;
   VCHIQ_SERVICE_HANDLE_T service;
   MMAL_WAITPOOL_T waitpool;
   VCOS_MUTEX_T bulk_lock;

   MMAL_BOOL_T inited;
};

/* One client per process/VC connection. Multiple threads may
 * be using a single client.
 */
static MMAL_CLIENT_T client;

static void init_once(void)
{
   vcos_mutex_create(&client.lock, VCOS_FUNCTION);
}

/** Create a pool of wait-structures.
  */
static MMAL_STATUS_T create_waitpool(MMAL_WAITPOOL_T *waitpool)
{
   VCOS_STATUS_T status;
   int i;

   status = vcos_semaphore_create(&waitpool->sem, VCOS_FUNCTION,
                                  MAX_WAITERS);
   if (status != VCOS_SUCCESS)
      return status==VCOS_SUCCESS ? MMAL_SUCCESS : MMAL_ENOSPC;

   for (i=0; i<MAX_WAITERS; i++)
   {
      waitpool->waiters[i].inuse = 0;
      status = vcos_semaphore_create(&waitpool->waiters[i].sem,
                                     "mmal waiter", 0);
      if (status != VCOS_SUCCESS)
         break;
   }

   if (status != VCOS_SUCCESS)
   {
      /* clean up */
      i--;
      while (i>=0)
      {
         vcos_semaphore_delete(&waitpool->waiters[i].sem);
         i--;
      }
      vcos_semaphore_delete(&waitpool->sem);
   }
   return status==VCOS_SUCCESS ? MMAL_SUCCESS : MMAL_ENOSPC;
}

static void destroy_waitpool(MMAL_WAITPOOL_T *waitpool)
{
   int i;
   for (i=0; i<MAX_WAITERS; i++)
      vcos_semaphore_delete(&waitpool->waiters[i].sem);

   vcos_semaphore_delete(&waitpool->sem);
}

/** Grab a waiter from the pool. Return immediately if one already
  * available, or wait for one to become available.
  */
static MMAL_WAITER_T *get_waiter(MMAL_CLIENT_T *client)
{
   int i;
   MMAL_WAITER_T *waiter = NULL;
   vcos_semaphore_wait(&client->waitpool.sem);
   vcos_mutex_lock(&client->lock);
   for (i=0; i<MAX_WAITERS; i++)
   {
      if (client->waitpool.waiters[i].inuse == 0)
         break;
   }
   /* If this fails, the semaphore is not working */
   if (vcos_verify(i != MAX_WAITERS))
   {
      waiter = client->waitpool.waiters+i;
      waiter->inuse = 1;
   }
   vcos_mutex_unlock(&client->lock);

   return waiter;
}

/** Return a waiter to the pool.
  */
static void release_waiter(MMAL_CLIENT_T *client, MMAL_WAITER_T *waiter)
{
   LOG_TRACE("at %p", waiter);
   vcos_assert(waiter);
   vcos_assert(waiter->inuse);
   waiter->inuse = 0;
   vcos_semaphore_post(&client->waitpool.sem);
}

static MMAL_PORT_T *mmal_vc_port_by_number(MMAL_COMPONENT_T *component, uint32_t type, uint32_t number)
{
   switch (type)
   {
      case MMAL_PORT_TYPE_CONTROL:
         vcos_assert(number == 0);
         return component->control;
      case MMAL_PORT_TYPE_INPUT:
         vcos_assert(number < component->input_num);
         return component->input[number];
      case MMAL_PORT_TYPE_OUTPUT:
         vcos_assert(number < component->output_num);
         return component->output[number];
      case MMAL_PORT_TYPE_CLOCK:
         vcos_assert(number < component->clock_num);
         return component->clock[number];
   }

   return NULL;
}

static void mmal_vc_handle_event_msg(VCHIQ_HEADER_T *vchiq_header,
                                    VCHIQ_SERVICE_HANDLE_T service,
                                    void *context)
{
   mmal_worker_event_to_host *msg = (mmal_worker_event_to_host *)vchiq_header->data;
   MMAL_COMPONENT_T *component = msg->client_component;
   MMAL_BUFFER_HEADER_T *buffer;
   MMAL_STATUS_T status;
   MMAL_PORT_T *port;

   LOG_DEBUG("event to host, cmd 0x%08x len %d to component %p port (%d,%d)",
         msg->cmd, msg->length, msg->client_component, msg->port_type, msg->port_num);
   (void)context;

   port = mmal_vc_port_by_number(component, msg->port_type, msg->port_num);
   if (!vcos_verify(port))
   {
      LOG_ERROR("port (%i,%i) doesn't exist", (int)msg->port_type, (int)msg->port_num);
      goto error;
   }

   status = mmal_port_event_get(port, &buffer, msg->cmd);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("no event buffer available to receive event (%i)", (int)status);
      goto error;
   }

   if (!vcos_verify(msg->length <= buffer->alloc_size))
   {
      LOG_ERROR("event buffer to small to receive event (%i/%i)",
                (int)buffer->alloc_size, (int)msg->length);
      goto error;
   }
   buffer->length = msg->length;

   /* Sanity check that the event buffers have the proper vc client context */
   if (!vcos_verify(mmal_buffer_header_driver_data(buffer)->magic == MMAL_MAGIC &&
          mmal_buffer_header_driver_data(buffer)->client_context &&
          mmal_buffer_header_driver_data(buffer)->client_context->magic == MMAL_MAGIC &&
          mmal_buffer_header_driver_data(buffer)->client_context->callback_event))
   {
      LOG_ERROR("event buffers not configured properly by component");
      goto error;
   }

   if (buffer->length > MMAL_WORKER_EVENT_SPACE)
   {
      /* a buffer full of data for us to process */
      int len = buffer->length;
      len = (len+3) & (~3);
      LOG_DEBUG("queue event bulk rx: %p, %d", buffer->data, buffer->length);
      msg->delayed_buffer = buffer;

      VCHIQ_STATUS_T vst = vchiq_queue_bulk_receive(service, buffer->data, len, vchiq_header);
      if (vst != VCHIQ_SUCCESS)
      {
         LOG_TRACE("queue event bulk rx len %d failed to start", buffer->length);
         mmal_buffer_header_release(buffer);
         goto error;
      }
   }
   else
   {
      if (msg->length)
         memcpy(buffer->data, msg->data, msg->length);

      mmal_buffer_header_driver_data(buffer)->client_context->callback_event(port, buffer);
      LOG_DEBUG("done callback back to client");
      vchiq_release_message(service, vchiq_header);
   }

   return;

error:
   /* FIXME: How to abort bulk receive if necessary? */
   msg->length = 0; /* FIXME: set a buffer flag to signal error */
   vchiq_release_message(service, vchiq_header);
}

static MMAL_STATUS_T mmal_vc_use_internal(MMAL_CLIENT_T *client)
{
   MMAL_STATUS_T status = MMAL_SUCCESS;
   vcos_mutex_lock(&client->lock);
   if(client->usecount++ == 0)
   {
      if(vchiq_use_service(client->service) != VCHIQ_SUCCESS)
      {
         client->usecount--;
         status = MMAL_EIO;
      }
   }
   vcos_mutex_unlock(&client->lock);
   return status;
}

static MMAL_STATUS_T mmal_vc_release_internal(MMAL_CLIENT_T *client)
{
   MMAL_STATUS_T status = MMAL_SUCCESS;
   vcos_mutex_lock(&client->lock);
   if(--client->usecount == 0)
   {
      if(vchiq_release_service(client->service) != VCHIQ_SUCCESS)
      {
         client->usecount++;
         status = MMAL_EIO;
      }
   }
   vcos_mutex_unlock(&client->lock);
   return status;
}


/** Callback invoked by VCHIQ
  */
static VCHIQ_STATUS_T mmal_vc_vchiq_callback(VCHIQ_REASON_T reason,
                                             VCHIQ_HEADER_T *vchiq_header,
                                             VCHIQ_SERVICE_HANDLE_T service,
                                             void *context)
{
   LOG_TRACE("reason %d", reason);

   switch (reason)
   {
   case VCHIQ_MESSAGE_AVAILABLE:
      {
         mmal_worker_msg_header *msg = (mmal_worker_msg_header*)vchiq_header->data;
         vcos_assert(msg->magic == MMAL_MAGIC);

         if (msg->msgid == MMAL_WORKER_BUFFER_TO_HOST)
         {
            LOG_TRACE("buffer to host");
            mmal_worker_buffer_from_host *msg = (mmal_worker_buffer_from_host *)vchiq_header->data;
            LOG_TRACE("len %d context %p", msg->buffer_header.length, msg->drvbuf.client_context);
            vcos_assert(msg->drvbuf.client_context);
            vcos_assert(msg->drvbuf.client_context->magic == MMAL_MAGIC);

            /* If the buffer is referencing another, need to replicate it here
             * in order to use the reference buffer's payload and ensure the
             * reference is not released prematurely */
            if (msg->has_reference)
               mmal_buffer_header_replicate(msg->drvbuf.client_context->buffer,
                                            msg->drvbuf_ref.client_context->buffer);

            /* Sanity check the size of the transfer so we don't overrun our buffer */
            if (!vcos_verify(msg->buffer_header.offset + msg->buffer_header.length <=
                             msg->drvbuf.client_context->buffer->alloc_size))
            {
               LOG_TRACE("buffer too small (%i, %i)",
                         msg->buffer_header.offset + msg->buffer_header.length,
                         msg->drvbuf.client_context->buffer->alloc_size);
               msg->buffer_header.length = 0; /* FIXME: set a buffer flag to signal error */
               msg->drvbuf.client_context->callback(msg);
               vchiq_release_message(service, vchiq_header);
               break;
            }
            /*To handle VC to HOST filled buffer callback of EOS buffer to receive in sync with data buffers*/
            if (!msg->is_zero_copy &&
                  (msg->buffer_header.length != 0 ||
                     (msg->buffer_header.flags & MMAL_BUFFER_HEADER_FLAG_EOS)))
            {
               /* a buffer full of data for us to process */
               VCHIQ_STATUS_T vst = VCHIQ_SUCCESS;
               LOG_TRACE("queue bulk rx: %p, %d", msg->drvbuf.client_context->buffer->data +
                         msg->buffer_header.offset, msg->buffer_header.length);
               int len = msg->buffer_header.length;
               len = (len+3) & (~3);

               if (!len && (msg->buffer_header.flags & MMAL_BUFFER_HEADER_FLAG_EOS))
               {
                  len = 8;
               }
               if (!msg->payload_in_message)
               {
                  /* buffer transferred using vchiq bulk xfer */
                  vst = vchiq_queue_bulk_receive(service,
                     msg->drvbuf.client_context->buffer->data + msg->buffer_header.offset,
                     len, vchiq_header);

                  if (vst != VCHIQ_SUCCESS)
                  {
                     LOG_TRACE("queue bulk rx len %d failed to start", msg->buffer_header.length);
                     msg->buffer_header.length = 0; /* FIXME: set a buffer flag to signal error */
                     msg->drvbuf.client_context->callback(msg);
                     vchiq_release_message(service, vchiq_header);
                  }
               }
               else if (msg->payload_in_message <= MMAL_VC_SHORT_DATA)
               {
                  /* we have already received the buffer data in the message! */
                  MMAL_BUFFER_HEADER_T *dst = msg->drvbuf.client_context->buffer;
                  LOG_TRACE("short data: dst = %p, dst->data = %p, len %d short len %d", dst, dst? dst->data : 0, msg->buffer_header.length, msg->payload_in_message);
                  memcpy(dst->data, msg->short_data, msg->payload_in_message);
                  dst->offset = 0;
                  dst->length = msg->payload_in_message;
                  vchiq_release_message(service, vchiq_header);
                  msg->drvbuf.client_context->callback(msg);
               }
               else
               {
                  /* impossible short data length */
                  LOG_ERROR("Message with invalid short payload length %d",
                            msg->payload_in_message);
                  vcos_assert(0);
               }
            }
            else
            {

               /* Message received from videocore; the client_context should have
                * been passed all the way through by videocore back to us, and will
                * be picked up in the callback to complete the sequence.
                */
               LOG_TRACE("doing cb (%p) context %p",
                         msg->drvbuf.client_context, msg->drvbuf.client_context ?
                         msg->drvbuf.client_context->callback : 0);
               msg->drvbuf.client_context->callback(msg);
               LOG_TRACE("done callback back to client");
               vchiq_release_message(service, vchiq_header);
            }
         }
         else if (msg->msgid == MMAL_WORKER_EVENT_TO_HOST)
         {
            mmal_vc_handle_event_msg(vchiq_header, service, context);
         }
         else
         {
            MMAL_WAITER_T *waiter = msg->u.waiter;
            LOG_TRACE("waking up waiter at %p", waiter);
            vcos_assert(waiter->inuse);
            int len = vcos_min(waiter->destlen, vchiq_header->size);
            waiter->destlen = len;
            LOG_TRACE("copying payload @%p to %p len %d", waiter->dest, msg, len);
            memcpy(waiter->dest, msg, len);
            vchiq_release_message(service, vchiq_header);
            vcos_semaphore_post(&waiter->sem);
         }
      }
      break;
   case VCHIQ_BULK_TRANSMIT_DONE:
      {
         /* nothing to do here, need to wait for the copro to tell us it
          * has emptied the buffer before we can recycle it, otherwise we
          * end up feeding the copro with buffers it cannot handle.
          */
#ifdef VCOS_LOGGING_ENABLED
         mmal_worker_buffer_from_host *msg = (mmal_worker_buffer_from_host *)context;
#endif
         LOG_TRACE("bulk tx done: %p, %d", msg->buffer_header.data, msg->buffer_header.length);
      }
      break;
   case VCHIQ_BULK_RECEIVE_DONE:
      {
         VCHIQ_HEADER_T *header = (VCHIQ_HEADER_T *)context;
         mmal_worker_msg_header *msg_hdr = (mmal_worker_msg_header*)header->data;
         if (msg_hdr->msgid == MMAL_WORKER_BUFFER_TO_HOST)
         {
            mmal_worker_buffer_from_host *msg = (mmal_worker_buffer_from_host *)msg_hdr;
            vcos_assert(msg->drvbuf.client_context->magic == MMAL_MAGIC);
            msg->drvbuf.client_context->callback(msg);
            LOG_TRACE("bulk rx done: %p, %d", msg->buffer_header.data, msg->buffer_header.length);
         }
         else
         {
            mmal_worker_event_to_host *msg = (mmal_worker_event_to_host *)msg_hdr;
            MMAL_PORT_T *port = mmal_vc_port_by_number(msg->client_component, msg->port_type, msg->port_num);

            vcos_assert(port);
            mmal_buffer_header_driver_data(msg->delayed_buffer)->
               client_context->callback_event(port, msg->delayed_buffer);
            LOG_DEBUG("event bulk rx done, length %d", msg->length);
         }
         vchiq_release_message(service, header);
      }
      break;
   case VCHIQ_BULK_RECEIVE_ABORTED:
      {
         VCHIQ_HEADER_T *header = (VCHIQ_HEADER_T *)context;
         mmal_worker_msg_header *msg_hdr = (mmal_worker_msg_header*)header->data;
         if (msg_hdr->msgid == MMAL_WORKER_BUFFER_TO_HOST)
         {
            mmal_worker_buffer_from_host *msg = (mmal_worker_buffer_from_host *)msg_hdr;
            LOG_TRACE("bulk rx aborted: %p, %d", msg->buffer_header.data, msg->buffer_header.length);
            vcos_assert(msg->drvbuf.client_context->magic == MMAL_MAGIC);
            msg->buffer_header.flags |= MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED;
            msg->drvbuf.client_context->callback(msg);
         }
         else
         {
            mmal_worker_event_to_host *msg = (mmal_worker_event_to_host *)msg_hdr;
            MMAL_PORT_T *port = mmal_vc_port_by_number(msg->client_component, msg->port_type, msg->port_num);

            vcos_assert(port);
            LOG_DEBUG("event bulk rx aborted");
            msg->delayed_buffer->flags |= MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED;
            mmal_buffer_header_driver_data(msg->delayed_buffer)->
               client_context->callback_event(port, msg->delayed_buffer);
         }
         vchiq_release_message(service, header);
      }
      break;
   case VCHIQ_BULK_TRANSMIT_ABORTED:
      {
         mmal_worker_buffer_from_host *msg = (mmal_worker_buffer_from_host *)context;
         LOG_INFO("bulk tx aborted: %p, %d", msg->buffer_header.data, msg->buffer_header.length);
         vcos_assert(msg->drvbuf.client_context->magic == MMAL_MAGIC);
         /* Nothing to do as the VC side will release the buffer and notify us of the error */
      }
      break;
   default:
      break;
   }

   return VCHIQ_SUCCESS;
}

/** Send a message and wait for a reply.
  *
  * @param client       client to send message for
  * @param msg_header   message vchiq_header to send
  * @param size         length of message, including header
  * @param msgid        message id
  * @param dest         destination for reply
  * @param destlen      size of destination, updated with actual length
  * @param send_dummy_bulk whether to send a dummy bulk transfer
  */
MMAL_STATUS_T mmal_vc_sendwait_message(struct MMAL_CLIENT_T *client,
                                       mmal_worker_msg_header *msg_header,
                                       size_t size,
                                       uint32_t msgid,
                                       void *dest,
                                       size_t *destlen,
                                       MMAL_BOOL_T send_dummy_bulk)
{
   MMAL_STATUS_T ret;
   MMAL_WAITER_T *waiter;
   VCHIQ_STATUS_T vst;
   VCHIQ_ELEMENT_T elems[] = {{msg_header, size}};

   vcos_assert(size >= sizeof(mmal_worker_msg_header));
   vcos_assert(dest);

   if (!client->inited)
   {
      vcos_assert(0);
      return MMAL_EINVAL;
   }

   if (send_dummy_bulk)
      vcos_mutex_lock(&client->bulk_lock);

   waiter = get_waiter(client);
   msg_header->msgid  = msgid;
   msg_header->u.waiter = waiter;
   msg_header->magic  = MMAL_MAGIC;

   waiter->dest    = dest;
   waiter->destlen = *destlen;
   LOG_TRACE("wait %p, reply to %p", waiter, dest);
   mmal_vc_use_internal(client);

   vst = vchiq_queue_message(client->service, elems, 1);

   if (vst != VCHIQ_SUCCESS)
   {
      ret = MMAL_EIO;
      if (send_dummy_bulk)
        vcos_mutex_unlock(&client->bulk_lock);
      goto fail_msg;
   }

   if (send_dummy_bulk)
   {
      uint32_t data_size = 8;
      /* The data is just some dummy bytes so it's fine for it to be static */
      static uint8_t data[8];
      vst = vchiq_queue_bulk_transmit(client->service, data, data_size, msg_header);

      vcos_mutex_unlock(&client->bulk_lock);

      if (!vcos_verify(vst == VCHIQ_SUCCESS))
      {
         LOG_ERROR("failed bulk transmit");
         /* This really should not happen and if it does, things will go wrong as
          * we've already queued the vchiq message above. */
         vcos_assert(0);
         ret = MMAL_EIO;
         goto fail_msg;
      }
   }

   /* now wait for the reply...
    *
    * FIXME: we could do with a timeout here. Need to be careful to cancel
    * the semaphore on a timeout.
    */
   /* coverity[lock] This semaphore isn't being used as a mutex */
   vcos_semaphore_wait(&waiter->sem);

   mmal_vc_release_internal(client);
   LOG_TRACE("got reply (len %i/%i)", (int)*destlen, (int)waiter->destlen);
   *destlen = waiter->destlen;

   release_waiter(client, waiter);
   return MMAL_SUCCESS;

fail_msg:
   mmal_vc_release_internal(client);

   release_waiter(client, waiter);
   return ret;
}

/** Send a message and do not wait for a reply.
  *
  * @note
  * This function should only be called from within a mmal component, so
  * vchiq_use/release_service calls aren't required (dealt with at higher level).
  *
  * @param client       client to send message for
  * @param msg_header   message header to send
  * @param size         length of message, including header
  * @param msgid        message id
  */
MMAL_STATUS_T mmal_vc_send_message(MMAL_CLIENT_T *client,
                                   mmal_worker_msg_header *msg_header, size_t size,
                                   uint8_t *data, size_t data_size,
                                   uint32_t msgid)
{
   VCHIQ_STATUS_T vst;
   VCHIQ_ELEMENT_T elems[] = {{msg_header, size}};
   MMAL_BOOL_T using_bulk_transfer = (data_size != 0);

   LOG_TRACE("len %d", data_size);
   vcos_assert(size >= sizeof(mmal_worker_msg_header));

   if (!client->inited)
   {
      vcos_assert(0);
      return MMAL_EINVAL;
   }

   if (using_bulk_transfer)
      vcos_mutex_lock(&client->bulk_lock);

   msg_header->msgid  = msgid;
   msg_header->magic  = MMAL_MAGIC;

   vst = vchiq_queue_message(client->service, elems, 1);

   if (vst != VCHIQ_SUCCESS)
   {
      if (using_bulk_transfer)
         vcos_mutex_unlock(&client->bulk_lock);

      LOG_ERROR("failed");
      goto error;
   }

   if (using_bulk_transfer)
   {
      LOG_TRACE("bulk transmit: %p, %i", data, data_size);

      data_size = (data_size + 3) & ~3;
      vst = vchiq_queue_bulk_transmit(client->service, data, data_size, msg_header);

      vcos_mutex_unlock(&client->bulk_lock);

      if (!vcos_verify(vst == VCHIQ_SUCCESS))
      {
         LOG_ERROR("failed bulk transmit");
         /* This really should not happen and if it does, things will go wrong as
          * we've already queued the vchiq message above. */
         vcos_assert(0);
         goto error;
      }
   }

   return MMAL_SUCCESS;

 error:
   return MMAL_EIO;
}

MMAL_STATUS_T mmal_vc_use(void)
{
   MMAL_STATUS_T status = MMAL_ENOTCONN;
   if(client.inited)
      status = mmal_vc_use_internal(&client);
   return status;
}

MMAL_STATUS_T mmal_vc_release(void)
{
   MMAL_STATUS_T status = MMAL_ENOTCONN;
   if(client.inited)
      status = mmal_vc_release_internal(&client);
   return status;
}

MMAL_STATUS_T mmal_vc_init_fd(int dev_vchiq_fd)
{
   VCHIQ_SERVICE_PARAMS_T vchiq_params;
   MMAL_BOOL_T vchiq_initialised = 0, waitpool_initialised = 0;
   MMAL_BOOL_T service_initialised = 0;
   MMAL_STATUS_T status = MMAL_EIO;
   VCHIQ_STATUS_T vchiq_status;
   int count;

   vcos_once(&once, init_once);

   vcos_mutex_lock(&client.lock);

   count = client.refcount++;
   if (count > 0)
   {
      /* Already initialised so nothing to do */
      vcos_mutex_unlock(&client.lock);
      return MMAL_SUCCESS;
   }

   vcos_log_register("mmalipc", VCOS_LOG_CATEGORY);

   /* Initialise a VCHIQ instance */
   vchiq_status = vchiq_initialise_fd(&mmal_vchiq_instance, dev_vchiq_fd);
   if (vchiq_status != VCHIQ_SUCCESS)
   {
      LOG_ERROR("failed to initialise vchiq");
      status = MMAL_EIO;
      goto error;
   }
   vchiq_initialised = 1;

   vchiq_status = vchiq_connect(mmal_vchiq_instance);
   if (vchiq_status != VCHIQ_SUCCESS)
   {
      LOG_ERROR("failed to connect to vchiq");
      status = MMAL_EIO;
      goto error;
   }

   memset(&vchiq_params,0,sizeof(vchiq_params));
   vchiq_params.fourcc = MMAL_CONTROL_FOURCC();
   vchiq_params.callback = mmal_vc_vchiq_callback;
   vchiq_params.userdata = &client;
   vchiq_params.version = WORKER_VER_MAJOR;
   vchiq_params.version_min = WORKER_VER_MINIMUM;

   vchiq_status = vchiq_open_service(mmal_vchiq_instance, &vchiq_params, &client.service);
   if (vchiq_status != VCHIQ_SUCCESS)
   {
      LOG_ERROR("could not open vchiq service");
      status = MMAL_EIO;
      goto error;
   }
   client.usecount = 1; /* usecount set to 1 by the open call. */
   service_initialised = 1;

   status = create_waitpool(&client.waitpool);
   if (status != MMAL_SUCCESS)
   {
      LOG_ERROR("could not create wait pool");
      goto error;
   }
   waitpool_initialised = 1;

   if (vcos_mutex_create(&client.bulk_lock, "mmal client bulk lock") != VCOS_SUCCESS)
   {
      LOG_ERROR("could not create bulk lock");
      status = MMAL_ENOSPC;
      goto error;
   }

   client.inited = 1;

   vcos_mutex_unlock(&client.lock);
   /* assume we're not using VC immediately.  Do this outside the lock */
   mmal_vc_release();


   return MMAL_SUCCESS;

 error:
   if (waitpool_initialised)
      destroy_waitpool(&client.waitpool);
   if (service_initialised)
   {
      client.usecount = 0;
      vchiq_close_service(client.service);
   }
   if (vchiq_initialised)
      vchiq_shutdown(mmal_vchiq_instance);
   vcos_log_unregister(VCOS_LOG_CATEGORY);
   client.refcount--;

   vcos_mutex_unlock(&client.lock);
   return status;
}

MMAL_STATUS_T mmal_vc_init(void)
{
   return mmal_vc_init_fd(-1);
}

void mmal_vc_deinit(void)
{
   int count;

   vcos_mutex_lock(&client.lock);
   count = --client.refcount;
   if (count != 0)
   {
      /* Still in use so don't do anything */
      vcos_mutex_unlock(&client.lock);
      return;
   }

   vcos_mutex_delete(&client.bulk_lock);
   destroy_waitpool(&client.waitpool);
   vchiq_close_service(client.service);
   vchiq_shutdown(mmal_vchiq_instance);
   vcos_log_unregister(VCOS_LOG_CATEGORY);

   client.service = VCHIQ_SERVICE_HANDLE_INVALID;
   client.inited = 0;
   vcos_mutex_unlock(&client.lock);
}

MMAL_CLIENT_T *mmal_vc_get_client(void)
{
   return &client;
}
