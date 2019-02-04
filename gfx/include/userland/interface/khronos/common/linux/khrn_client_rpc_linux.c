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
#define VCOS_LOG_CATEGORY (&khrn_client_log)

#include "vchiq.h"
#include "interface/khronos/common/khrn_int_common.h"
#include "interface/khronos/common/khrn_int_ids.h"

#include "interface/khronos/common/khrn_client.h"
#include "interface/khronos/common/khrn_client_rpc.h"


#include <string.h>
#include <stdio.h>

extern VCOS_LOG_CAT_T khrn_client_log;

static void *workspace; /* for scatter/gather bulks */
static PLATFORM_MUTEX_T mutex;

#define FOURCC_KHAN VCHIQ_MAKE_FOURCC('K', 'H', 'A', 'N')
#define FOURCC_KHRN VCHIQ_MAKE_FOURCC('K', 'H', 'R', 'N')
#define FOURCC_KHHN VCHIQ_MAKE_FOURCC('K', 'H', 'H', 'N')

static VCHIQ_INSTANCE_T khrn_vchiq_instance;

static VCHIQ_SERVICE_HANDLE_T vchiq_khan_service;
static VCHIQ_SERVICE_HANDLE_T vchiq_khrn_service;
static VCHIQ_SERVICE_HANDLE_T vchiq_khhn_service;

static VCHIU_QUEUE_T khrn_queue;
static VCHIU_QUEUE_T khhn_queue;

static VCOS_EVENT_T bulk_event;

VCHIQ_STATUS_T khrn_callback(VCHIQ_REASON_T reason, VCHIQ_HEADER_T *header,
                  VCHIQ_SERVICE_HANDLE_T handle, void *bulk_userdata)
{
   switch (reason) {
   case VCHIQ_MESSAGE_AVAILABLE:
      vchiu_queue_push(&khrn_queue, header);
      break;
   case VCHIQ_BULK_TRANSMIT_DONE:
   case VCHIQ_BULK_RECEIVE_DONE:
      vcos_event_signal(&bulk_event);
      break;
   case VCHIQ_SERVICE_OPENED:
   case VCHIQ_SERVICE_CLOSED:
   case VCHIQ_BULK_TRANSMIT_ABORTED:
   case VCHIQ_BULK_RECEIVE_ABORTED:
      UNREACHABLE(); /* not implemented */
   }

   return VCHIQ_SUCCESS;
}

VCHIQ_STATUS_T khhn_callback(VCHIQ_REASON_T reason, VCHIQ_HEADER_T *header,
                  VCHIQ_SERVICE_HANDLE_T handle, void *bulk_userdata)
{
   switch (reason) {
   case VCHIQ_MESSAGE_AVAILABLE:
      vchiu_queue_push(&khhn_queue, header);
      break;
   case VCHIQ_BULK_TRANSMIT_DONE:
   case VCHIQ_BULK_RECEIVE_DONE:
      vcos_event_signal(&bulk_event);
      break;
   case VCHIQ_SERVICE_OPENED:
   case VCHIQ_SERVICE_CLOSED:
   case VCHIQ_BULK_TRANSMIT_ABORTED:
   case VCHIQ_BULK_RECEIVE_ABORTED:
      UNREACHABLE(); /* not implemented */      
   }

   return VCHIQ_SUCCESS;
}

VCHIQ_STATUS_T khan_callback(VCHIQ_REASON_T reason, VCHIQ_HEADER_T *header,
                  VCHIQ_SERVICE_HANDLE_T handle, void *bulk_userdata)
{
   switch (reason) {
   case VCHIQ_MESSAGE_AVAILABLE:
   {
      int32_t *data = (int32_t *) header->data;
      int32_t command = data[0];
      int32_t *msg = &data[1];
      vcos_assert(header->size == 16);

      // TODO should be able to remove this eventually.
      // If incoming message is not addressed to this process, then ignore it.
      // Correct process should then pick it up.
      uint64_t pid = vchiq_get_client_id(handle);
      if((msg[0] != (uint32_t) pid) || (msg[1] != (uint32_t) (pid >> 32)))
      {
         printf("khan_callback: message for wrong process; pid = %X, msg pid = %X\n",
            (uint32_t) pid, msg[0]);
         return VCHIQ_SUCCESS;
      } // if

      if (command == ASYNC_COMMAND_DESTROY)
      {
         /* todo: destroy */
      }
      else
      {
         PLATFORM_SEMAPHORE_T sem;
         if (khronos_platform_semaphore_create(&sem, msg, 1) == KHR_SUCCESS)
         {
            switch (command) {
            case ASYNC_COMMAND_WAIT:
               khronos_platform_semaphore_acquire(&sem);
               break;
            case ASYNC_COMMAND_POST:
               khronos_platform_semaphore_release(&sem);
               break;
            default:
               vcos_assert_msg(0, "khan_callback: unknown message type");
               break;
            }
            khronos_platform_semaphore_destroy(&sem);
         }
      } // else
      vchiq_release_message(handle, header);
      break;
   }
   case VCHIQ_BULK_TRANSMIT_DONE:
   case VCHIQ_BULK_RECEIVE_DONE:
      UNREACHABLE();
      break;
   case VCHIQ_SERVICE_OPENED:
      vcos_assert_msg(0, "khan_callback: VCHIQ_SERVICE_OPENED");
      break;
   case VCHIQ_SERVICE_CLOSED:
      vcos_assert_msg(0, "khan_callback: VCHIQ_SERVICE_CLOSED");
      break;
   case VCHIQ_BULK_TRANSMIT_ABORTED:
   case VCHIQ_BULK_RECEIVE_ABORTED:
   default:
      UNREACHABLE(); /* not implemented */      
   }

   return VCHIQ_SUCCESS;
}

void vc_vchi_khronos_init()
{
   VCOS_STATUS_T status = vcos_event_create(&bulk_event, NULL);
   UNUSED_NDEBUG(status);
   vcos_assert(status == VCOS_SUCCESS);
   
   if (vchiq_initialise(&khrn_vchiq_instance) != VCHIQ_SUCCESS)
   {
      vcos_log_error("* failed to open vchiq device");
      
      exit(1);
   }

   vcos_log_trace("gldemo: connecting");

   if (vchiq_connect(khrn_vchiq_instance) != VCHIQ_SUCCESS)
   {
      vcos_log_error("* failed to connect");
      
      exit(1);
   }

   VCHIQ_STATUS_T khan_return, khrn_return, khhn_return;
   VCHIQ_SERVICE_PARAMS_T params;

   params.userdata = NULL;
   params.version = VC_KHRN_VERSION;
   params.version_min = VC_KHRN_VERSION;

   params.fourcc = FOURCC_KHAN;
   params.callback = khan_callback;
   khan_return = vchiq_open_service(khrn_vchiq_instance, &params, &vchiq_khan_service);

   params.fourcc = FOURCC_KHRN;
   params.callback = khrn_callback;
   khrn_return = vchiq_open_service(khrn_vchiq_instance, &params, &vchiq_khrn_service);

   params.fourcc = FOURCC_KHHN;
   params.callback = khhn_callback;
   khhn_return = vchiq_open_service(khrn_vchiq_instance, &params, &vchiq_khhn_service);

   if (khan_return != VCHIQ_SUCCESS ||
       khrn_return != VCHIQ_SUCCESS ||
       khhn_return != VCHIQ_SUCCESS)
   {
      vcos_log_error("* failed to add service - already in use?");
      
      exit(1);
   }
   vchiu_queue_init(&khrn_queue, 64);
   vchiu_queue_init(&khhn_queue, 64);

   vcos_log_trace("gldemo: connected");

   /*
      attach to process (there's just one)
   */

//   bool success = client_process_attach();
//   vcos_assert(success);
}

bool khclient_rpc_init(void)
{
   workspace = NULL;
   return platform_mutex_create(&mutex) == KHR_SUCCESS;
}

void rpc_term(void)
{
   if (workspace) { khrn_platform_free(workspace); }
   platform_mutex_destroy(&mutex);
}

static VCHIQ_SERVICE_HANDLE_T get_handle(CLIENT_THREAD_STATE_T *thread)
{
   VCHIQ_SERVICE_HANDLE_T result = (thread->high_priority ? vchiq_khhn_service : vchiq_khrn_service);
   	return result;
}

static VCHIU_QUEUE_T *get_queue(CLIENT_THREAD_STATE_T *thread)
{
   return thread->high_priority ? &khhn_queue : &khrn_queue;
}

static void check_workspace(uint32_t size)
{
   /* todo: find a better way to handle scatter/gather bulks */
   vcos_assert(size <= KHDISPATCH_WORKSPACE_SIZE);
   if (!workspace) {
      workspace = khrn_platform_malloc(KHDISPATCH_WORKSPACE_SIZE, "rpc_workspace");
      vcos_assert(workspace);
   }
}

static void merge_flush(CLIENT_THREAD_STATE_T *thread)
{
   vcos_log_trace("merge_flush start");
   
   vcos_assert(thread->merge_pos >= CLIENT_MAKE_CURRENT_SIZE);

   /*
      don't transmit just a make current -- in the case that there is only a
      make current in the merge buffer, we have already sent a control message
      for the rpc call (and with it a make current) and own the big lock
   */

   if (thread->merge_pos > CLIENT_MAKE_CURRENT_SIZE) {
      VCHIQ_ELEMENT_T element;
      
      rpc_begin(thread);

      element.data = thread->merge_buffer;
      element.size = thread->merge_pos;

      VCHIQ_STATUS_T success = vchiq_queue_message(get_handle(thread), &element, 1);
      UNUSED_NDEBUG(success);      
      vcos_assert(success == VCHIQ_SUCCESS);

      thread->merge_pos = 0;

      client_send_make_current(thread);

      vcos_assert(thread->merge_pos == CLIENT_MAKE_CURRENT_SIZE);
      
      rpc_end(thread);
   }
   vcos_log_trace( "merge_flush end");
   
}

void rpc_flush(CLIENT_THREAD_STATE_T *thread)
{
   merge_flush(thread);
}

void rpc_high_priority_begin(CLIENT_THREAD_STATE_T *thread)
{
   vcos_assert(!thread->high_priority);
   merge_flush(thread);
   thread->high_priority = true;
}

void rpc_high_priority_end(CLIENT_THREAD_STATE_T *thread)
{
   vcos_assert(thread->high_priority);
   merge_flush(thread);
   thread->high_priority = false;
}

uint32_t rpc_send_ctrl_longest(CLIENT_THREAD_STATE_T *thread, uint32_t len_min)
{
   uint32_t len = MERGE_BUFFER_SIZE - thread->merge_pos;
   if (len < len_min) {
      len = MERGE_BUFFER_SIZE - CLIENT_MAKE_CURRENT_SIZE;
   }
   return len;
}

void rpc_send_ctrl_begin(CLIENT_THREAD_STATE_T *thread, uint32_t len)
{
   //CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   vcos_assert(len == rpc_pad_ctrl(len));
   if ((thread->merge_pos + len) > MERGE_BUFFER_SIZE) {
      merge_flush(thread);
   }

   thread->merge_end = thread->merge_pos + len;
}

void rpc_send_ctrl_write(CLIENT_THREAD_STATE_T *thread, const uint32_t in[], uint32_t len) /* len bytes read, rpc_pad_ctrl(len) bytes written */
{
   //CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   memcpy(thread->merge_buffer + thread->merge_pos, in, len);
   thread->merge_pos += rpc_pad_ctrl(len);
   vcos_assert(thread->merge_pos <= MERGE_BUFFER_SIZE);
}

void rpc_send_ctrl_end(CLIENT_THREAD_STATE_T *thread)
{
   //CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

   vcos_assert(thread->merge_pos == thread->merge_end);
}

static void send_bulk(CLIENT_THREAD_STATE_T *thread, const void *in, uint32_t len)
{
   if (len <= KHDISPATCH_CTRL_THRESHOLD) {
      VCHIQ_ELEMENT_T element;

      element.data = in;
      element.size = len;

      VCHIQ_STATUS_T vchiq_status = vchiq_queue_message(get_handle(thread), &element, 1);
      UNUSED_NDEBUG(vchiq_status);
      vcos_assert(vchiq_status == VCHIQ_SUCCESS);
   } else {
      VCHIQ_STATUS_T vchiq_status = vchiq_queue_bulk_transmit(get_handle(thread), in, rpc_pad_bulk(len), NULL);
      UNUSED_NDEBUG(vchiq_status);      
      vcos_assert(vchiq_status == VCHIQ_SUCCESS);
      VCOS_STATUS_T vcos_status = vcos_event_wait(&bulk_event);
      UNUSED_NDEBUG(vcos_status);      
      vcos_assert(vcos_status == VCOS_SUCCESS);
   }
}

static void recv_bulk(CLIENT_THREAD_STATE_T *thread, void *out, uint32_t len)
{
   if (len <= KHDISPATCH_CTRL_THRESHOLD) {
      VCHIQ_HEADER_T *header = vchiu_queue_pop(get_queue(thread));
      vcos_assert(header->size == len);
      memcpy(out, header->data, len);
      vchiq_release_message(get_handle(thread), header);
   } else {
      VCHIQ_STATUS_T vchiq_status = vchiq_queue_bulk_receive(get_handle(thread), out, rpc_pad_bulk(len), NULL);
      UNUSED_NDEBUG(vchiq_status);
      vcos_assert(vchiq_status == VCHIQ_SUCCESS);
      VCOS_STATUS_T vcos_status = vcos_event_wait(&bulk_event);
      UNUSED_NDEBUG(vcos_status);      
      vcos_assert(vcos_status == VCOS_SUCCESS);
   }
}

void rpc_send_bulk(CLIENT_THREAD_STATE_T *thread, const void *in, uint32_t len)
{
   if (in && len) {
      //CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

      merge_flush(thread);

      send_bulk(thread, in, len);
   }
}

void rpc_send_bulk_gather(CLIENT_THREAD_STATE_T *thread, const void *in, uint32_t len, int32_t stride, uint32_t n)
{
#if 1
   if (in && len) {
      //CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

      merge_flush(thread);

      if (len == stride) {
         /* hopefully should be the common case */
         send_bulk(thread, in, n * len);
      } else {
         check_workspace(n * len);
         rpc_gather(workspace, in, len, stride, n);
         send_bulk(thread, workspace, n * len);
      }
   }
#else
   UNREACHABLE();
#endif
}

uint32_t rpc_recv(CLIENT_THREAD_STATE_T *thread, void *out, uint32_t *len_io, RPC_RECV_FLAG_T flags)
{
   uint32_t res = 0;
   uint32_t len;
   bool recv_ctrl;

   if (!len_io) { len_io = &len; }

   recv_ctrl = flags & (RPC_RECV_FLAG_RES | RPC_RECV_FLAG_CTRL | RPC_RECV_FLAG_LEN); /* do we want to receive anything in the control channel at all? */
   vcos_assert(recv_ctrl || (flags & RPC_RECV_FLAG_BULK)); /* must receive something... */
   vcos_assert(!(flags & RPC_RECV_FLAG_CTRL) || !(flags & RPC_RECV_FLAG_BULK)); /* can't receive user data over both bulk and control... */

   if (recv_ctrl || len_io[0]) { /* do nothing if we're just receiving bulk of length 0 */
      merge_flush(thread);

      if (recv_ctrl) {
         VCHIQ_HEADER_T *header = vchiu_queue_pop(get_queue(thread));
         uint8_t *ctrl = (uint8_t *)header->data;
         vcos_assert(header->size >= (!!(flags & RPC_RECV_FLAG_LEN)*4 + !!(flags & RPC_RECV_FLAG_RES)*4) );
         if (flags & RPC_RECV_FLAG_LEN) {
            len_io[0] = *((uint32_t *)ctrl);
            ctrl += 4;
         }
         if (flags & RPC_RECV_FLAG_RES) {
            res = *((uint32_t *)ctrl);
            ctrl += 4;
         }
         if (flags & RPC_RECV_FLAG_CTRL) {
            memcpy(out, ctrl, len_io[0]);
            ctrl += len_io[0];
         }
         vcos_assert(ctrl == ((uint8_t *)header->data + header->size));
         vchiq_release_message(get_handle(thread), header);
      }

      if ((flags & RPC_RECV_FLAG_BULK) && len_io[0]) {
         if (flags & RPC_RECV_FLAG_BULK_SCATTER) {
            if ((len_io[0] == len_io[1]) && !len_io[3] && !len_io[4]) {
               /* hopefully should be the common case */
               recv_bulk(thread, out, len_io[2] * len_io[0]);
            } else {
               check_workspace(len_io[2] * len_io[0]);
               recv_bulk(thread, workspace, len_io[2] * len_io[0]);
               rpc_scatter(out, len_io[0], len_io[1], len_io[2], len_io[3], len_io[4], workspace);
            }
         } else {
            recv_bulk(thread, out, len_io[0]);
         }
      }
   }

   return res;
}

void rpc_begin(CLIENT_THREAD_STATE_T *thread)
{
   UNUSED(thread);
   platform_mutex_acquire(&mutex);
}

void rpc_end(CLIENT_THREAD_STATE_T *thread)
{
   UNUSED(thread);
   platform_mutex_release(&mutex);
}

void rpc_use(CLIENT_THREAD_STATE_T *thread)
{ // TODO - implement this for linux
   UNUSED(thread);
}

void rpc_release(CLIENT_THREAD_STATE_T *thread)
{ // TODO - implement this for linux
   UNUSED(thread);
}

void rpc_call8_makecurrent(CLIENT_THREAD_STATE_T *thread, uint32_t id, uint32_t p0,
   uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5, uint32_t p6, uint32_t p7)
{
   uint32_t data;
   if (thread->merge_pos == CLIENT_MAKE_CURRENT_SIZE && (memcpy(&data,thread->merge_buffer,sizeof(data)), data == EGLINTMAKECURRENT_ID))
   {
      rpc_begin(thread);
      vcos_log_trace("rpc_call8_makecurrent collapse onto previous makecurrent");

      thread->merge_pos = 0;
      
      RPC_CALL8(eglIntMakeCurrent_impl, thread, EGLINTMAKECURRENT_ID, p0, p1, p2, p3, p4, p5, p6, p7);
      vcos_assert(thread->merge_pos == CLIENT_MAKE_CURRENT_SIZE);

      rpc_end(thread);
   }
   else
   {
      RPC_CALL8(eglIntMakeCurrent_impl, thread, EGLINTMAKECURRENT_ID, p0, p1, p2, p3, p4, p5, p6, p7);
   }
}

uint64_t rpc_get_client_id(CLIENT_THREAD_STATE_T *thread)
{
   return vchiq_get_client_id(get_handle(thread));
}
