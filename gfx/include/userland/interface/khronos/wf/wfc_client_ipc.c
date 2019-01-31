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

#include "interface/khronos/wf/wfc_client_ipc.h"
#include "interface/khronos/wf/wfc_ipc.h"
#include "interface/vcos/vcos.h"
#include "interface/vchiq_arm/vchiq.h"

#define VCOS_LOG_CATEGORY (&wfc_client_ipc_log_category)

//#define WFC_FULL_LOGGING
#ifdef WFC_FULL_LOGGING
#define WFC_CLIENT_IPC_LOGLEVEL VCOS_LOG_TRACE
#else
#define WFC_CLIENT_IPC_LOGLEVEL VCOS_LOG_WARN
#endif

#define WFC_CLIENT_IPC_MAX_WAITERS 16
static VCOS_ONCE_T wfc_client_ipc_once = VCOS_ONCE_INIT;
static VCHIQ_INSTANCE_T wfc_client_ipc_vchiq_instance;
static VCOS_LOG_CAT_T wfc_client_ipc_log_category;

/** Client threads use one of these to wait for
 * a reply from VideoCore.
 */
typedef struct WFC_WAITER_T
{
   VCOS_SEMAPHORE_T sem;
   unsigned inuse;
   void *dest;                   /**< Where to write reply */
   size_t destlen;               /**< Max length for reply */
} WFC_WAITER_T;

/** We have an array of waiters and allocate them to waiting
  * threads. They can be released back to the pool in any order.
  * If there are none free, the calling thread will block until
  * one becomes available.
  */
typedef struct
{
   WFC_WAITER_T waiters[WFC_CLIENT_IPC_MAX_WAITERS];
   VCOS_SEMAPHORE_T sem;
} WFC_WAITPOOL_T;

struct WFC_CLIENT_IPC_T
{
   int refcount;
   int keep_alive_count;
   VCOS_MUTEX_T lock;
   VCHIQ_SERVICE_HANDLE_T service;
   WFC_WAITPOOL_T waitpool;
};
typedef struct WFC_CLIENT_IPC_T WFC_CLIENT_IPC_T;

/* One client per process/VC connection. Multiple threads may
 * be using a single client.
 */
static WFC_CLIENT_IPC_T wfc_client_ipc;

static void init_once(void)
{
   vcos_mutex_create(&wfc_client_ipc.lock, VCOS_FUNCTION);
}

/** Create a pool of wait-structures.
  */
static VCOS_STATUS_T wfc_client_ipc_create_waitpool(WFC_WAITPOOL_T *waitpool)
{
   VCOS_STATUS_T status;
   int i;

   status = vcos_semaphore_create(&waitpool->sem, VCOS_FUNCTION,
                                  WFC_CLIENT_IPC_MAX_WAITERS);
   if (status != VCOS_SUCCESS)
      return status;

   for (i = 0; i < WFC_CLIENT_IPC_MAX_WAITERS; i++)
   {
      waitpool->waiters[i].inuse = 0;
      status = vcos_semaphore_create(&waitpool->waiters[i].sem,
                                     "wfc ipc waiter", 0);
      if (status != VCOS_SUCCESS)
         break;
   }

   if (status != VCOS_SUCCESS)
   {
      /* clean up */
      i--;
      while (i >= 0)
      {
         vcos_semaphore_delete(&waitpool->waiters[i].sem);
         i--;
      }
      vcos_semaphore_delete(&waitpool->sem);
   }
   return status;
}

static void wfc_client_ipc_destroy_waitpool(WFC_WAITPOOL_T *waitpool)
{
   int i;

   for (i = 0; i < WFC_CLIENT_IPC_MAX_WAITERS; i++)
      vcos_semaphore_delete(&waitpool->waiters[i].sem);

   vcos_semaphore_delete(&waitpool->sem);
}

/** Grab a waiter from the pool. Return immediately if one already
  * available, or wait for one to become available.
  */
static WFC_WAITER_T *wfc_client_ipc_get_waiter(WFC_CLIENT_IPC_T *client)
{
   int i;
   WFC_WAITER_T *waiter = NULL;

   vcos_semaphore_wait(&client->waitpool.sem);
   vcos_mutex_lock(&client->lock);

   for (i = 0; i < WFC_CLIENT_IPC_MAX_WAITERS; i++)
   {
      if (client->waitpool.waiters[i].inuse == 0)
         break;
   }

   /* If this fails, the semaphore isn't working */
   if (vcos_verify(i != WFC_CLIENT_IPC_MAX_WAITERS))
   {
      waiter = client->waitpool.waiters + i;
      waiter->inuse = 1;
   }
   vcos_mutex_unlock(&client->lock);

   return waiter;
}

/** Return a waiter to the pool.
  */
static void wfc_client_ipc_release_waiter(WFC_CLIENT_IPC_T *client, WFC_WAITER_T *waiter)
{
   vcos_log_trace("%s: at %p", VCOS_FUNCTION, waiter);

   vcos_assert(waiter);
   vcos_assert(waiter->inuse);

   waiter->inuse = 0;
   vcos_semaphore_post(&client->waitpool.sem);
}

/** Callback invoked by VCHIQ
  */
static VCHIQ_STATUS_T wfc_client_ipc_vchiq_callback(VCHIQ_REASON_T reason,
                                                VCHIQ_HEADER_T *vchiq_header,
                                                VCHIQ_SERVICE_HANDLE_T service,
                                                void *context)
{
   vcos_log_trace("%s: reason %d", VCOS_FUNCTION, reason);

   switch (reason)
   {
   case VCHIQ_MESSAGE_AVAILABLE:
      {
         WFC_IPC_MSG_HEADER_T *response = (WFC_IPC_MSG_HEADER_T *)vchiq_header->data;

         vcos_assert(vchiq_header->size >= sizeof(*response));
         vcos_assert(response->magic == WFC_IPC_MSG_MAGIC);

         if (response->type == WFC_IPC_MSG_CALLBACK)
         {
            WFC_IPC_MSG_CALLBACK_T *callback_msg = (WFC_IPC_MSG_CALLBACK_T *)response;
            WFC_CALLBACK_T cb_func = callback_msg->callback_fn.ptr;

            vcos_assert(vchiq_header->size == sizeof(*callback_msg));
            if (vcos_verify(cb_func != NULL))
            {
               /* Call the client function */
               (*cb_func)(callback_msg->callback_data.ptr);
            }
            vchiq_release_message(service, vchiq_header);
         }
         else
         {
            WFC_WAITER_T *waiter = response->waiter.ptr;
            int len;

            vcos_assert(waiter != NULL);

            vcos_log_trace("%s: waking up waiter at %p", VCOS_FUNCTION, waiter);
            vcos_assert(waiter->inuse);

            /* Limit response data length */
            len = vcos_min(waiter->destlen, vchiq_header->size - sizeof(*response));
            waiter->destlen = len;

            vcos_log_trace("%s: copying %d bytes from %p to %p first word 0x%x",
                  VCOS_FUNCTION, len, response + 1, waiter->dest, *(uint32_t *)(response + 1));
            memcpy(waiter->dest, response + 1, len);

            vchiq_release_message(service, vchiq_header);
            vcos_semaphore_post(&waiter->sem);
         }
      }
      break;
   case VCHIQ_BULK_TRANSMIT_DONE:
   case VCHIQ_BULK_RECEIVE_DONE:
   case VCHIQ_BULK_RECEIVE_ABORTED:
   case VCHIQ_BULK_TRANSMIT_ABORTED:
      {
         vcos_assert_msg(0, "bulk messages not used");
         vchiq_release_message(service, vchiq_header);
      }
      break;
   case VCHIQ_SERVICE_OPENED:
      vcos_log_trace("%s: service %x opened", VCOS_FUNCTION, service);
      break;
   case VCHIQ_SERVICE_CLOSED:
      vcos_log_trace("%s: service %x closed", VCOS_FUNCTION, service);
      break;
   default:
      vcos_assert_msg(0, "unexpected message reason");
      break;
   }
   return VCHIQ_SUCCESS;
}

static VCOS_STATUS_T wfc_client_ipc_send_client_pid(void)
{
   WFC_IPC_MSG_SET_CLIENT_PID_T msg;
   uint64_t pid = vcos_process_id_current();
   uint32_t pid_lo = (uint32_t) pid;
   uint32_t pid_hi = (uint32_t) (pid >> 32);

   msg.header.type = WFC_IPC_MSG_SET_CLIENT_PID;
   msg.pid_lo = pid_lo;
   msg.pid_hi = pid_hi;

   vcos_log_trace("%s: setting client pid to 0x%x%08x", VCOS_FUNCTION, pid_hi, pid_lo);

   return wfc_client_ipc_send(&msg.header, sizeof(msg));
}

VCOS_STATUS_T wfc_client_ipc_sendwait(WFC_IPC_MSG_HEADER_T *msg,
                                       size_t size,
                                       void *dest,
                                       size_t *destlen)
{
   VCOS_STATUS_T ret = VCOS_SUCCESS;
   WFC_WAITER_T *waiter;
   VCHIQ_STATUS_T vst;
   VCHIQ_ELEMENT_T elems[] = {{msg, size}};

   vcos_assert(size >= sizeof(*msg));
   vcos_assert(dest);

   if (!vcos_verify(wfc_client_ipc.refcount))
   {
      VCOS_ALERT("%s: client uninitialised", VCOS_FUNCTION);
      /* Client has not been initialised */
      return VCOS_EINVAL;
   }

   msg->magic = WFC_IPC_MSG_MAGIC;

   waiter = wfc_client_ipc_get_waiter(&wfc_client_ipc);
   waiter->dest = dest;
   waiter->destlen = *destlen;
   msg->waiter.ptr = waiter;

   wfc_client_ipc_use_keep_alive();

   vcos_log_trace("%s: wait %p, reply to %p", VCOS_FUNCTION, waiter, dest);
   vst = vchiq_queue_message(wfc_client_ipc.service, elems, 1);

   if (vst != VCHIQ_SUCCESS)
   {
      vcos_log_error("%s: failed to queue, 0x%x", VCOS_FUNCTION, vst);
      ret = VCOS_ENXIO;
      goto completed;
   }

   /* now wait for the reply...
    *
    * FIXME: we could do with a timeout here. Need to be careful to cancel
    * the semaphore on a timeout.
    */
   vcos_semaphore_wait(&waiter->sem);
   vcos_log_trace("%s: got reply (len %i/%i)", VCOS_FUNCTION, (int)*destlen, (int)waiter->destlen);
   *destlen = waiter->destlen;

   /* Drop through completion code */

completed:
   wfc_client_ipc_release_waiter(&wfc_client_ipc, waiter);
   wfc_client_ipc_release_keep_alive();

   return ret;
}

VCOS_STATUS_T wfc_client_ipc_send(WFC_IPC_MSG_HEADER_T *msg,
                                    size_t size)
{
   VCHIQ_STATUS_T vst;
   VCHIQ_ELEMENT_T elems[] = {{msg, size}};

   vcos_log_trace("%s: type %d, len %d", VCOS_FUNCTION, msg->type, size);

   vcos_assert(size >= sizeof(*msg));

   if (!vcos_verify(wfc_client_ipc.refcount))
   {
      VCOS_ALERT("%s: client uninitialised", VCOS_FUNCTION);
      /* Client has not been initialised */
      return VCOS_EINVAL;
   }

   msg->magic  = WFC_IPC_MSG_MAGIC;
   msg->waiter.ptr = NULL;

   wfc_client_ipc_use_keep_alive();

   vst = vchiq_queue_message(wfc_client_ipc.service, elems, 1);

   wfc_client_ipc_release_keep_alive();

   if (vst != VCHIQ_SUCCESS)
   {
      vcos_log_error("%s: failed to queue, 0x%x", VCOS_FUNCTION, vst);
      return VCOS_ENXIO;
   }

   return VCOS_SUCCESS;
}

VCOS_STATUS_T wfc_client_ipc_init(void)
{
   VCHIQ_SERVICE_PARAMS_T vchiq_params;
   bool vchiq_initialised = false, waitpool_initialised = false;
   bool service_initialised = false;
   VCOS_STATUS_T status = VCOS_ENXIO;
   VCHIQ_STATUS_T vchiq_status;

   vcos_once(&wfc_client_ipc_once, init_once);

   vcos_mutex_lock(&wfc_client_ipc.lock);

   if (wfc_client_ipc.refcount++ > 0)
   {
      /* Already initialised so nothing to do */
      vcos_mutex_unlock(&wfc_client_ipc.lock);
      return VCOS_SUCCESS;
   }

   vcos_log_set_level(VCOS_LOG_CATEGORY, WFC_CLIENT_IPC_LOGLEVEL);
   vcos_log_register("wfcipc", VCOS_LOG_CATEGORY);

   vcos_log_trace("%s: starting initialisation", VCOS_FUNCTION);

   /* Initialise a VCHIQ instance */
   vchiq_status = vchiq_initialise(&wfc_client_ipc_vchiq_instance);
   if (vchiq_status != VCHIQ_SUCCESS)
   {
      vcos_log_error("%s: failed to initialise vchiq: %d", VCOS_FUNCTION, vchiq_status);
      goto error;
   }
   vchiq_initialised = true;

   vchiq_status = vchiq_connect(wfc_client_ipc_vchiq_instance);
   if (vchiq_status != VCHIQ_SUCCESS)
   {
      vcos_log_error("%s: failed to connect to vchiq: %d", VCOS_FUNCTION, vchiq_status);
      goto error;
   }

   memset(&vchiq_params, 0, sizeof(vchiq_params));
   vchiq_params.fourcc = WFC_IPC_CONTROL_FOURCC();
   vchiq_params.callback = wfc_client_ipc_vchiq_callback;
   vchiq_params.userdata = &wfc_client_ipc;
   vchiq_params.version = WFC_IPC_VER_CURRENT;
   vchiq_params.version_min = WFC_IPC_VER_MINIMUM;

   vchiq_status = vchiq_open_service(wfc_client_ipc_vchiq_instance, &vchiq_params, &wfc_client_ipc.service);
   if (vchiq_status != VCHIQ_SUCCESS)
   {
      vcos_log_error("%s: could not open vchiq service: %d", VCOS_FUNCTION, vchiq_status);
      goto error;
   }
   service_initialised = true;

   status = wfc_client_ipc_create_waitpool(&wfc_client_ipc.waitpool);
   if (status != VCOS_SUCCESS)
   {
      vcos_log_error("%s: could not create wait pool: %d", VCOS_FUNCTION, status);
      goto error;
   }
   waitpool_initialised = true;

   /* Allow videocore to suspend, drops count to zero. */
   vchiq_release_service(wfc_client_ipc.service);

   vcos_mutex_unlock(&wfc_client_ipc.lock);

   status = wfc_client_ipc_send_client_pid();
   if (status != VCOS_SUCCESS)
   {
      vcos_log_error("%s: could not send client pid: %d", VCOS_FUNCTION, status);
      vcos_mutex_lock(&wfc_client_ipc.lock);
      goto error;
   }

   return VCOS_SUCCESS;

error:
   if (waitpool_initialised)
      wfc_client_ipc_destroy_waitpool(&wfc_client_ipc.waitpool);
   if (service_initialised)
      vchiq_remove_service(wfc_client_ipc.service);
   if (vchiq_initialised)
      vchiq_shutdown(wfc_client_ipc_vchiq_instance);
   vcos_log_unregister(VCOS_LOG_CATEGORY);
   wfc_client_ipc.refcount--;

   vcos_mutex_unlock(&wfc_client_ipc.lock);
   return status;
}

bool wfc_client_ipc_deinit(void)
{
   bool service_destroyed = false;

   vcos_once(&wfc_client_ipc_once, init_once);

   vcos_mutex_lock(&wfc_client_ipc.lock);

   if (!wfc_client_ipc.refcount)
   {
      /* Never initialised */
      goto completed;
   }

   if (--wfc_client_ipc.refcount != 0)
   {
      /* Still in use so don't do anything */
      goto completed;
   }

   vcos_log_trace("%s: starting deinitialisation", VCOS_FUNCTION);

   /* Last reference dropped, tear down service */
   wfc_client_ipc_destroy_waitpool(&wfc_client_ipc.waitpool);
   vchiq_remove_service(wfc_client_ipc.service);
   vchiq_shutdown(wfc_client_ipc_vchiq_instance);
   vcos_log_unregister(VCOS_LOG_CATEGORY);

   wfc_client_ipc.service = 0;

   service_destroyed = true;

completed:
   vcos_mutex_unlock(&wfc_client_ipc.lock);

   return service_destroyed;
}

/* ------------------------------------------------------------------------- */

void wfc_client_ipc_use_keep_alive(void)
{
   vcos_mutex_lock(&wfc_client_ipc.lock);

   if (!wfc_client_ipc.keep_alive_count++)
      vchiq_use_service(wfc_client_ipc.service);

   vcos_mutex_unlock(&wfc_client_ipc.lock);
}

/* ------------------------------------------------------------------------- */

void wfc_client_ipc_release_keep_alive(void)
{
   vcos_mutex_lock(&wfc_client_ipc.lock);

   if (vcos_verify(wfc_client_ipc.keep_alive_count > 0))
   {
      if (!--wfc_client_ipc.keep_alive_count)
         vchiq_release_service(wfc_client_ipc.service);
   }

   vcos_mutex_unlock(&wfc_client_ipc.lock);
}

/* ------------------------------------------------------------------------- */

WFC_CLIENT_IPC_T *wfc_client_ipc_get_client(void)
{
   return &wfc_client_ipc;
}
