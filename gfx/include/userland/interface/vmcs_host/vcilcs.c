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

#include <stdio.h>

/* Project includes */
#include "vcinclude/common.h"

#include "interface/vchiq_arm/vchiq.h"
#include "interface/vmcs_host/khronos/IL/OMX_Component.h"
#include "interface/vmcs_host/vc_ilcs_defs.h"
#include "interface/vmcs_host/vcilcs.h"

#ifdef USE_VCHI
#include "interface/vchi/vchiq_wrapper.h"
#endif

#ifdef _VIDEOCORE
#include "applications/vmcs/ilcs/ilcs_common.h"
#endif

/******************************************************************************
Private types and defines.
******************************************************************************/

// number of threads that can use ilcs
#define ILCS_MAX_WAITING 8

// maximum number of retries to grab a wait slot,
// before the message is discarded and failure returned.
#define ILCS_WAIT_TIMEOUT 250

// maximum number of concurrent function calls that can
// be going at once.  Each function call requires to copy
// the message data so we can dequeue the message from vchi
// before executing the function, otherwise ILCS may cause
// a deadlock.  Must be larger than ILCS_MAX_WAITING
#define ILCS_MAX_NUM_MSGS (ILCS_MAX_WAITING+1)
#define ILCS_MSG_INUSE_MASK ((1<<ILCS_MAX_NUM_MSGS)-1)

typedef struct {
   int xid;
   void *resp;
   int *rlen;
   VCOS_EVENT_T event;
} ILCS_WAIT_T;

typedef enum {
   NORMAL_SERVICE  = 0,  // process all messages
   ABORTED_BULK    = 1,  // reject incoming calls
   CLOSED_CALLBACK = 2,  // quit thread and cleanup, use reaper, VC only
   DEINIT_CALLED   = 3,  // quit thread and cleanup, no reaper
} ILCS_QUIT_T;

struct ILCS_SERVICE_T {
   char name[12];
#ifdef USE_VCHIQ_ARM
   VCHIQ_INSTANCE_T vchiq;
   VCHIQ_SERVICE_HANDLE_T service;
#else
   VCHIQ_STATE_T *vchiq;
#endif
   int fourcc;
   VCOS_TIMER_T timer;
   volatile int timer_expired;
   VCOS_THREAD_T thread;
   int timer_needed;
   ILCS_QUIT_T kill_service;
   int use_memmgr;

   ILCS_COMMON_T *ilcs_common;
   ILCS_CONFIG_T config;

   VCHIU_QUEUE_T queue;
   VCOS_EVENT_T bulk_rx;

   VCOS_SEMAPHORE_T send_sem; // for making control+bulk serialised
   VCOS_MUTEX_T wait_mtx; // for protecting ->wait and ->next_xid
   ILCS_WAIT_T wait[ILCS_MAX_WAITING];
   int next_xid;
   VCOS_EVENT_T wait_event;  // for signalling when a wait becomes free

   // don't need locking around msg_inuse as only touched by
   // the server thread in ilcs_process_message
   unsigned int msg_inuse;
   unsigned char msg[ILCS_MAX_NUM_MSGS][VCHIQ_SLOT_SIZE];
   uint32_t header_array[(sizeof(VCHIQ_HEADER_T)+8)/4];
};

/******************************************************************************
Private functions in this file.
Define as static.
******************************************************************************/
#ifdef USE_VCHIQ_ARM
static VCHIQ_STATUS_T ilcs_callback(VCHIQ_REASON_T reason, VCHIQ_HEADER_T *header, VCHIQ_SERVICE_HANDLE_T service_user, void *bulk_user);
#else
static int ilcs_callback(VCHIQ_REASON_T reason, VCHIQ_HEADER_T *header, void *service_user, void *bulk_user);
#endif
static void *ilcs_task(void *param);
static void ilcs_response(ILCS_SERVICE_T *st, uint32_t xid, const unsigned char *msg, int len );
static void ilcs_transmit(ILCS_SERVICE_T *st, uint32_t cmd, uint32_t xid,
                          const unsigned char *msg, int len,
                          const unsigned char *msg2, int len2);
static void ilcs_command(ILCS_SERVICE_T *st, uint32_t cmd, uint32_t xid, unsigned char *msg, int len);
static void ilcs_timer(void *param);
static int ilcs_process_message(ILCS_SERVICE_T *st, int block);

/* ----------------------------------------------------------------------
 * initialise OpenMAX IL component service
 * -------------------------------------------------------------------- */
#ifdef USE_VCHIQ_ARM
ILCS_SERVICE_T *ilcs_init(VCHIQ_INSTANCE_T state, void **connection, ILCS_CONFIG_T *config, int use_memmgr)
#else
ILCS_SERVICE_T *ilcs_init(VCHIQ_STATE_T *state, void **connection, ILCS_CONFIG_T *config, int use_memmgr)
#endif
{
   int32_t i;
   VCOS_THREAD_ATTR_T thread_attrs;
   ILCS_SERVICE_T *st;
   VCHIQ_SERVICE_PARAMS_T params;

   st = vcos_malloc(sizeof(ILCS_SERVICE_T), "ILCS State");
   if(!st)
      goto fail_alloc;

   memset(st, 0, sizeof(ILCS_SERVICE_T));
   st->vchiq = state;
   st->fourcc = VCHIQ_MAKE_FOURCC('I', 'L', 'C', 'S');
   st->config = *config;

   // setting this to true implies we have relocatable handles as
   // buffer pointers, otherwise we interpret them to be real pointers
   st->use_memmgr = use_memmgr;

   // create semaphore for protecting wait/xid structures
   if(vcos_mutex_create(&st->wait_mtx, "ILCS") != VCOS_SUCCESS)
      goto fail_all;

   // create smaphore for control+bulk protection
   if(vcos_semaphore_create(&st->send_sem, "ILCS", 1) != VCOS_SUCCESS)
      goto fail_send_sem;

   // create event group for signalling when a waiting slot becomes free
   if(vcos_event_create(&st->wait_event, "ILCS") != VCOS_SUCCESS)
      goto fail_wait_event;

   for(i=0; i<ILCS_MAX_WAITING; i++)
      if(vcos_event_create(&st->wait[i].event, "ILCS") != VCOS_SUCCESS)
      {
         while(--i >= 0)
            vcos_event_delete(&st->wait[i].event);
         goto fail_wait_events;
      }

   if(vcos_timer_create(&st->timer, "ILCS", ilcs_timer, st) != VCOS_SUCCESS)
      goto fail_timer;

   // create the queue of incoming messages
   if(!vchiu_queue_init(&st->queue, 256))
      goto fail_queue;

   // create the bulk receive event
   if(vcos_event_create(&st->bulk_rx, "ILCS") != VCOS_SUCCESS)
      goto fail_bulk_event;

   // create an 'ILCS' service
#ifdef USE_VCHIQ_ARM
   /* VCHIQ_ARM distinguishes between servers and clients. Use use_memmgr
      parameter to detect usage by the client.
    */

   memset(&params,0,sizeof(params));
   params.fourcc = st->fourcc;
   params.callback = ilcs_callback;
   params.userdata = st;
   params.version = VC_ILCS_VERSION;
   params.version_min = VC_ILCS_VERSION;

   if (use_memmgr == 0)
   {
      // Host side, which will connect to a listening VideoCore side
      if (vchiq_open_service(st->vchiq, &params, &st->service) != VCHIQ_SUCCESS)
         goto fail_service;
   }
   else
   {
      // VideoCore side, a listening service not connected
      if (vchiq_add_service(st->vchiq, &params, &st->service) != VCHIQ_SUCCESS)
         goto fail_service;

      // On VC shutdown we defer calling vchiq_remove_service until after the callback has
      // returned, so we require not to have the autoclose behaviour
      vchiq_set_service_option(st->service, VCHIQ_SERVICE_OPTION_AUTOCLOSE, 0);
   }
#else
#ifdef USE_VCHI
   if(!vchiq_wrapper_add_service(st->vchiq, connection, st->fourcc, ilcs_callback, st))
      goto fail_service;
#else
   if(!vchiq_add_service(st->vchiq, st->fourcc, ilcs_callback, st))
      goto fail_service;
#endif
#endif

   if((st->ilcs_common = st->config.ilcs_common_init(st)) == NULL)
      goto fail_ilcs_common;

   vcos_thread_attr_init(&thread_attrs);
   vcos_thread_attr_setstacksize(&thread_attrs, 4096);

   snprintf(st->name, sizeof(st->name), "ILCS_%s", use_memmgr ? "VC" : "HOST");

   if(vcos_thread_create(&st->thread, st->name, &thread_attrs, ilcs_task, st) != VCOS_SUCCESS)
      goto fail_thread;

   return st;

 fail_thread:
   st->config.ilcs_common_deinit(st->ilcs_common);
 fail_ilcs_common:
#ifdef USE_VCHIQ_ARM
   vchiq_remove_service(st->service);
#endif
 fail_service:
   vcos_event_delete(&st->bulk_rx);
 fail_bulk_event:
   vchiu_queue_delete(&st->queue);
 fail_queue:
   vcos_timer_delete(&st->timer);
 fail_timer:
   for(i=0; i<ILCS_MAX_WAITING; i++)
      vcos_event_delete(&st->wait[i].event);
 fail_wait_events:
   vcos_event_delete(&st->wait_event);
 fail_wait_event:
   vcos_semaphore_delete(&st->send_sem);
 fail_send_sem:
   vcos_mutex_delete(&st->wait_mtx);
 fail_all:
   vcos_free(st);
 fail_alloc:
   return NULL;
}

/* ----------------------------------------------------------------------
 * sends a message to the thread to quit
 * -------------------------------------------------------------------- */
static void ilcs_send_quit(ILCS_SERVICE_T *st)
{
   // We're closing, so tell the task to cleanup
   VCHIQ_HEADER_T *header = (VCHIQ_HEADER_T *)st->header_array;
   char *msg;
   int i;
   header->size = 8;
   msg = header->data;
   msg[0] = IL_SERVICE_QUIT & 0xff;
   msg[1] = (IL_SERVICE_QUIT >> 8) & 0xff;
   msg[2] = (IL_SERVICE_QUIT >> 16) & 0xff;
   msg[3] = IL_SERVICE_QUIT >> 24;

   vchiu_queue_push(&st->queue, header);

   // force all currently waiting clients to wake up
   for(i=0; i<ILCS_MAX_WAITING; i++)
      if(st->wait[i].resp)
         vcos_event_signal(&st->wait[i].event);

   vcos_event_signal(&st->wait_event);
}

/* ----------------------------------------------------------------------
 * deinitialises the OpenMAX IL Component Service.
 * This is the usual way that the host side service shuts down, called
 * from OMX_Deinit().
 * -------------------------------------------------------------------- */
void ilcs_deinit(ILCS_SERVICE_T *st)
{
   void *data;
   st->kill_service = DEINIT_CALLED;
   ilcs_send_quit(st);
   vcos_thread_join(&st->thread, &data);
   vcos_free(st);
}

/* ----------------------------------------------------------------------
 * sets the wait event, to timeout blocked threads
 * -------------------------------------------------------------------- */
static void ilcs_timer(void *param)
{
   ILCS_SERVICE_T *st = (ILCS_SERVICE_T *) param;

   vcos_assert(st->timer_expired == 0);
   st->timer_expired = 1;
   vcos_event_signal(&st->wait_event);
}

/* ----------------------------------------------------------------------
 * returns pointer to common object
 * -------------------------------------------------------------------- */
ILCS_COMMON_T *ilcs_get_common(ILCS_SERVICE_T *st)
{
   return st->ilcs_common;
}

/* ----------------------------------------------------------------------
 * whether the ilcsg thread is currently running
 * returns 1 if the ilcsg is the current thread, 0 otherwise
 * -------------------------------------------------------------------- */
int ilcs_thread_current(void *param)
{
   ILCS_SERVICE_T *st = (ILCS_SERVICE_T *) param;
   return vcos_thread_current() == &st->thread;
}

/* ----------------------------------------------------------------------
 * called from the vchiq layer whenever an event happens.
 * here, we are only interested in the 'message available' callback
 * -------------------------------------------------------------------- */
#ifdef USE_VCHIQ_ARM
static VCHIQ_STATUS_T ilcs_callback(VCHIQ_REASON_T reason, VCHIQ_HEADER_T *header, VCHIQ_SERVICE_HANDLE_T service_user, void *bulk_user)
{
   ILCS_SERVICE_T *st = (ILCS_SERVICE_T *)VCHIQ_GET_SERVICE_USERDATA(service_user);
#else
static int ilcs_callback(VCHIQ_REASON_T reason, VCHIQ_HEADER_T *header, void *service_user, void *bulk_user)
{
   ILCS_SERVICE_T *st = (ILCS_SERVICE_T *) service_user;
#endif

   switch(reason) {

#ifdef USE_VCHIQ_ARM
   case VCHIQ_SERVICE_OPENED:
      {
#ifdef _VIDEOCORE
         // We're on the VideoCore side and we've been connected to, so we need to spawn another
         // listening service.  Create another ILCS instance.
         ILCS_CONFIG_T config;
         ilcs_config(&config);
         ilcs_init(st->vchiq, NULL, &config, st->use_memmgr);
#else
         vcos_abort();
#endif
      }
      break;

   case VCHIQ_SERVICE_CLOSED:
      if(st && st->kill_service < CLOSED_CALLBACK)
      {
         st->kill_service = CLOSED_CALLBACK;
         ilcs_send_quit(st);
      }
      break;

   case VCHIQ_BULK_RECEIVE_ABORTED:
      // bulk rx only aborted if we're about to close the service,
      // so signal this now so that the person waiting for this
      // bulk rx can return a failure to the user
      st->kill_service = ABORTED_BULK;
      vcos_event_signal(&st->bulk_rx);
      break;
#endif

   case VCHIQ_MESSAGE_AVAILABLE:
#ifndef _VIDEOCORE
      {
	 static int queue_warn = 0;
	 int queue_len = st->queue.write - st->queue.read;
	 if (!queue_warn)
	    queue_warn = getenv("ILCS_WARN") ? (st->queue.size/2) : st->queue.size;
	 if (queue_len >= queue_warn)
	 {
	    if (queue_len == st->queue.size)
	       VCOS_ALERT("ILCS queue full");
	    else
	       VCOS_ALERT("ILCS queue len = %d", queue_len);
	    queue_warn = queue_warn + (st->queue.size - queue_warn)/2;
	 }
      }
#endif
      vchiu_queue_push(&st->queue, header);
      break;

   case VCHIQ_BULK_RECEIVE_DONE:
      vcos_event_signal(&st->bulk_rx);
      break;

   default:
      break;
   }

#ifdef USE_VCHIQ_ARM
   return VCHIQ_SUCCESS;
#else
   return 1;
#endif
}

/* ----------------------------------------------------------------------
 * send a message and wait for reply.
 * repeats continuously, on each connection
 * -------------------------------------------------------------------- */
static void *ilcs_task(void *param)
{
   ILCS_SERVICE_T *st = (ILCS_SERVICE_T *) param;
   int i;

   st->config.ilcs_thread_init(st->ilcs_common);

   while(st->kill_service < CLOSED_CALLBACK)
      ilcs_process_message(st, 1);

   // tidy up after ourselves
   st->config.ilcs_common_deinit(st->ilcs_common);
#ifdef USE_VCHIQ_ARM
   vchiq_remove_service(st->service);
#endif
   vcos_event_delete(&st->bulk_rx);
   vchiu_queue_delete(&st->queue);
   vcos_timer_delete(&st->timer);
   for(i=0; i<ILCS_MAX_WAITING; i++)
      vcos_event_delete(&st->wait[i].event);
   vcos_event_delete(&st->wait_event);
   vcos_semaphore_delete(&st->send_sem);
   vcos_mutex_delete(&st->wait_mtx);

   if(st->kill_service == CLOSED_CALLBACK)
   {
#ifdef _VIDEOCORE
      // need vcos reaper thread to do join/free for us
      vcos_thread_reap(&st->thread, vcos_free, st);
#else
      // we've got a CLOSED callback from vchiq without ilcs_deinit being called.
      // this shouldn't really happen, so we just want to abort at this point.
      vcos_abort();
#endif
   }

   return 0;
}

/* ----------------------------------------------------------------------
 * check to see if there are any pending messages
 *
 * if there are no messages, return 0
 *
 * otherwise, fetch and process the first queued message (which will
 * be either a command or response from host)
 * -------------------------------------------------------------------- */
#define UINT32(p) ((p)[0] | ((p)[1] << 8) | ((p)[2] << 16) | ((p)[3] << 24))

static int ilcs_process_message(ILCS_SERVICE_T *st, int block)
{
   unsigned char *msg;
   VCHIQ_HEADER_T *header;
   uint32_t i, msg_len, cmd, xid;

   if(!block && vchiu_queue_is_empty(&st->queue))
      return 0; // no more messages

   header = vchiu_queue_pop(&st->queue);

   msg = (unsigned char *) header->data;

   cmd = UINT32(msg);
   xid = UINT32(msg+4);

   msg += 8;
   msg_len = header->size - 8;

   if(cmd == IL_RESPONSE)
   {
      ilcs_response(st, xid, msg, msg_len);
#ifdef USE_VCHIQ_ARM
      vchiq_release_message(st->service, header);
#else
      vchiq_release_message(st->vchiq, header);
#endif
   }
   else if(cmd == IL_SERVICE_QUIT)
   {
      return 1;
   }
   else
   {
      // we can only handle commands if we have space to copy the message first
      if(st->msg_inuse == ILCS_MSG_INUSE_MASK)
      {
         // this shouldn't happen, since we have more msg slots than the
         // remote side is allowed concurrent clients.  this is classed
         // as a failure case, so we discard the message knowing that things
         // will surely lock up fairly soon after.
         vcos_assert(0);
         return 1;
      }

      i = 0;
      while(st->msg_inuse & (1<<i))
         i++;

      st->msg_inuse |= (1<<i);

      memcpy(st->msg[i], msg, msg_len);
#ifdef USE_VCHIQ_ARM
      vchiq_release_message(st->service, header);
#else
      vchiq_release_message(st->vchiq, header);
#endif
      ilcs_command(st, cmd, xid, st->msg[i], msg_len);

      // mark the message copy as free
      st->msg_inuse &= ~(1<<i);
   }

   return 1;
}

/* ----------------------------------------------------------------------
 * received response to an ILCS command
 * -------------------------------------------------------------------- */
static void ilcs_response(ILCS_SERVICE_T *st, uint32_t xid, const unsigned char *msg, int len)
{
   ILCS_WAIT_T *wait = NULL;
   int i, copy = len;

   // atomically retrieve given ->wait entry
   vcos_mutex_lock(&st->wait_mtx);
   for (i=0; i<ILCS_MAX_WAITING; i++) {
      wait = &st->wait[i];
      if(wait->resp && wait->xid == xid)
         break;
   }
   vcos_mutex_unlock(&st->wait_mtx);

   if(i == ILCS_MAX_WAITING) {
      // something bad happened, someone has sent a response back
      // when the caller said they weren't expecting a response
      vcos_assert(0);
      return;
   }

   // check that we have enough space to copy into.
   // if we haven't the user can tell by the updated rlen value.
   if(len > *wait->rlen)
      copy = *wait->rlen;

   *wait->rlen = len;

   // extract command from fifo and place in response buffer.
   memcpy(wait->resp, msg, copy);

   vcos_event_signal(&wait->event);
}

/* ----------------------------------------------------------------------
 * helper function to transmit an ilcs command/response + payload
 * -------------------------------------------------------------------- */
static void ilcs_transmit(ILCS_SERVICE_T *st, uint32_t cmd, uint32_t xid,
                          const unsigned char *msg, int len,
                          const unsigned char *msg2, int len2)
{
   VCHIQ_ELEMENT_T vec[4];
   int32_t count = 3;

   vec[0].data = &cmd;
   vec[0].size = sizeof(cmd);
   vec[1].data = &xid;
   vec[1].size = sizeof(xid);
   vec[2].data = msg;
   vec[2].size = len;

   if(msg2)
   {
      vec[3].data = msg2;
      vec[3].size = len2;
      count++;
   }

#ifdef USE_VCHIQ_ARM
   vchiq_queue_message(st->service, vec, count);
#else
   vchiq_queue_message(st->vchiq, st->fourcc, vec, count);
#endif
}

/* ----------------------------------------------------------------------
 * received response to an ILCS command
 * -------------------------------------------------------------------- */
static void
ilcs_command(ILCS_SERVICE_T *st, uint32_t cmd, uint32_t xid, unsigned char *msg, int len)
{
   // execute this function call
   unsigned char resp[VC_ILCS_MAX_CMD_LENGTH];
   unsigned char *rbuf = resp;
   int rlen = -1;
   IL_FN_T fn;

   if(cmd >= IL_FUNCTION_MAX_NUM) {
      vcos_assert(0);
      return;
   }

   fn = st->config.fns[cmd];
   if(!fn) {
      vcos_assert(0);
      return;
   }

   // for one method we allow the response to go in the same slot as the
   // msg, since it wants to return quite a big amount of debug information
   // and we know this is safe.
   if(cmd == IL_GET_DEBUG_INFORMATION)
   {
      int max = VCHIQ_SLOT_SIZE - 8;
      IL_GET_DEBUG_INFORMATION_EXECUTE_T *exe = (IL_GET_DEBUG_INFORMATION_EXECUTE_T *) msg;
      if(exe->len > max)
         exe->len = max;

      rbuf = msg;
   }

   // at this point we are executing in ILCS task context
   // NOTE: this can cause ilcs_execute_function() calls from within guts of openmaxil!
   fn(st->ilcs_common, msg, len, rbuf, &rlen);

   // make sure rlen has been initialised by the function
   vcos_assert(rlen != -1);

   if(rlen > 0)
      ilcs_transmit(st, IL_RESPONSE, xid, rbuf, rlen, NULL, 0);
}

/**
 * send a string to the host side IL component service.  if resp is NULL
 * then there is no response to this call, so we should not wait for one.
 *
 * returns 0 on successful call made, -1 on failure to send call.
 * on success, the response is written to 'resp' pointer
 *
 * @param data            function parameter data
 * @param len             length of function parameter data
 * @param data            optional second function parameter data
 * @param len2            length of second function parameter data
 * @param msg_mem_handle  option mem handle to be sent as part of msg data
 * @param msg_offset      Offset with msg mem handle
 * @param msg_len         Length of msg mem handle
 * @param bulk_mem_handle Mem handle sent using VCHI bulk transfer
 * @param bulk_offset     Offset within memory handle
 * @param bulk_len        Length of bulk transfer
 *
 * -------------------------------------------------------------------- */

static int ilcs_execute_function_ex(ILCS_SERVICE_T *st, IL_FUNCTION_T func,
                                    void *data, int len,
                                    void *data2, int len2,
                                    VCHI_MEM_HANDLE_T bulk_mem_handle, void *bulk_offset, int bulk_len,
                                    void *resp, int *rlen)
{
   ILCS_WAIT_T *wait = NULL;
   int num = 0;
   uint32_t xid;

   if(st->kill_service)
      return -1;

   // need to atomically find free ->wait entry
   vcos_mutex_lock(&st->wait_mtx);

   // if resp is NULL, we do not expect any response
   if(resp == NULL) {
      xid = st->next_xid++;
   }
   else
   {
      int i;

      if(st->timer_needed++ == 0)
      {
         vcos_timer_set(&st->timer, 10);
      }

      // we try a number of times then give up with an error message
      // rather than just deadlocking

      // Note: the real reason for the timeout is nothing to do with hardware
      // errors, but is to ensure that if the ILCS thread is calling this function
      // (because the client makes an OMX call from one of the callbacks) then
      // the queue of messages from VideoCore still gets serviced.

      for (i=0; i<ILCS_WAIT_TIMEOUT; i++) {
         num = 0;

         while(num < ILCS_MAX_WAITING && st->wait[num].resp != NULL)
            num++;

         if(num < ILCS_MAX_WAITING || i == ILCS_WAIT_TIMEOUT-1)
            break;

         // the previous time round this loop, we woke up because the timer
         // expired, so restart it
         if (st->timer_expired)
         {
            st->timer_expired = 0;
            vcos_timer_set(&st->timer, 10);
         }

         // might be a fatal error if another thread is relying
         // on this call completing before it can complete
         // we'll pause until we can carry on and hope that's sufficient.
         vcos_mutex_unlock(&st->wait_mtx);

         // if we're the ilcs thread, then the waiters might need
         // us to handle their response, so try and clear those now
         if(vcos_thread_current() == &st->thread)
         {
            while (vcos_event_try(&st->wait_event) != VCOS_SUCCESS)
            {
               while(ilcs_process_message(st, 0))
                  if(st->kill_service >= CLOSED_CALLBACK)
                     return -1;
               if (vcos_event_try(&st->wait_event) == VCOS_SUCCESS)
                  break;
               vcos_sleep(1);
            }
         }
         else
         {
            vcos_event_wait(&st->wait_event);
         }

         vcos_mutex_lock(&st->wait_mtx);
      }

      if(--st->timer_needed == 0)
      {
         vcos_timer_cancel(&st->timer);
         st->timer_expired = 0;
      }

      if(num == ILCS_MAX_WAITING)
      {
         // failed to send message.
         vcos_mutex_unlock(&st->wait_mtx);
         return -1;
      }

      wait = &st->wait[num];

      wait->resp = resp;
      wait->rlen = rlen;
      xid = wait->xid = st->next_xid++;
   }

   vcos_mutex_unlock(&st->wait_mtx);

   if(bulk_len != 0)
      vcos_semaphore_wait(&st->send_sem);

   ilcs_transmit(st, func, xid, data, len, data2, len2);

   if(bulk_len != 0)
   {
#ifdef USE_VCHIQ_ARM
      vchiq_queue_bulk_transmit_handle(st->service, bulk_mem_handle, bulk_offset, bulk_len, NULL);
#else
      vchiq_queue_bulk_transmit(st->vchiq, st->fourcc, bulk_mem_handle, bulk_offset, bulk_len, NULL);
#endif
      vcos_semaphore_post(&st->send_sem);
   }

   if(!wait)
   {
      // nothing more to do
      return 0;
   }

   if(vcos_thread_current() != &st->thread)
   {
      // block waiting for response
      vcos_event_wait(&wait->event);
   }
   else
   {
      // since we're the server task, to receive our own response code
      // we need to keep reading messages from the other side.  In
      // addition, our function executing on the host may also call
      // functions on VideoCore before replying, so we need to handle
      // all incoming messages until our response arrives.
      for (;;)
      {
         // wait->sem will not be released until we process the response message
         // so handle one incoming message
         ilcs_process_message(st, 1);

         // did the last message release wait->sem ?
         if(st->kill_service >= CLOSED_CALLBACK || vcos_event_try(&wait->event) == VCOS_SUCCESS)
            break;
      }
   }

   // safe to do the following - the assignment of NULL is effectively atomic
   wait->resp = NULL;
   vcos_event_signal(&st->wait_event);

   return st->kill_service >= CLOSED_CALLBACK ? -1 : 0;
}

int ilcs_execute_function(ILCS_SERVICE_T *st, IL_FUNCTION_T func, void *data, int len, void *resp, int *rlen)
{
   return ilcs_execute_function_ex(st, func, data, len, NULL, 0, VCHI_MEM_HANDLE_INVALID, 0, 0, resp, rlen);
}

/* ----------------------------------------------------------------------
 * send a buffer via the IL component service.
 * -------------------------------------------------------------------- */

OMX_ERRORTYPE ilcs_pass_buffer(ILCS_SERVICE_T *st, IL_FUNCTION_T func, void *reference,
                               OMX_BUFFERHEADERTYPE *pBuffer)
{
   IL_PASS_BUFFER_EXECUTE_T exe;
   IL_BUFFER_BULK_T fixup;
   IL_RESPONSE_HEADER_T resp;
   VCHI_MEM_HANDLE_T mem_handle = VCHI_MEM_HANDLE_INVALID;
   void *ret = NULL, *data2 = NULL, *bulk_offset = NULL;
   int len2 = 0, bulk_len = 0;
   OMX_U8 *ptr = NULL;
   int rlen = sizeof(resp);

   if(st->kill_service)
      return OMX_ErrorHardware;

   if((func == IL_EMPTY_THIS_BUFFER && pBuffer->pInputPortPrivate == NULL) ||
      (func == IL_FILL_THIS_BUFFER && pBuffer->pOutputPortPrivate == NULL))
   {
      // return this to pass conformance
      // the actual error is using a buffer that hasn't be registered with usebuffer/allocatebuffer
      return OMX_ErrorIncorrectStateOperation;
   }

   if((pBuffer->nFlags & OMX_BUFFERFLAG_EXTRADATA) || pBuffer->nFilledLen)
      ptr = st->config.ilcs_mem_lock(pBuffer) + pBuffer->nOffset;

   exe.reference = reference;
   memcpy(&exe.bufferHeader, pBuffer, sizeof(OMX_BUFFERHEADERTYPE));

   exe.bufferLen = pBuffer->nFilledLen;
   if(pBuffer->nFlags & OMX_BUFFERFLAG_EXTRADATA)
   {
      // walk down extra-data's appended to the buffer data to work out their length
      OMX_U8 *end = ptr + pBuffer->nAllocLen - pBuffer->nOffset;
      OMX_OTHER_EXTRADATATYPE *extra =
         (OMX_OTHER_EXTRADATATYPE *) (((uint32_t) (ptr + pBuffer->nFilledLen + 3)) & ~3);
      OMX_BOOL b_corrupt = OMX_FALSE;
      OMX_EXTRADATATYPE extra_type;

      do
      {
         // sanity check the extra data before doing anything with it
         if(((uint8_t *)extra) + sizeof(OMX_OTHER_EXTRADATATYPE) > end ||
            ((uint8_t *)extra) + extra->nSize > end ||
            extra->nSize < sizeof(OMX_OTHER_EXTRADATATYPE) ||
            (extra->nSize & 3))
         {
            // shouldn't happen. probably a problem with the component
            b_corrupt = OMX_TRUE;
            break;
         }

         extra_type = extra->eType;
         extra = (OMX_OTHER_EXTRADATATYPE *) (((uint8_t *) extra) + extra->nSize);
      }
      while(extra_type != OMX_ExtraDataNone);

      // if corrupt then drop the extra data since we can't do anything with it
      if(b_corrupt)
         pBuffer->nFlags &= ~OMX_BUFFERFLAG_EXTRADATA;
      else
         exe.bufferLen = ((uint8_t *) extra) - ptr;
   }

   // check that the buffer fits in the allocated region
   if(exe.bufferLen + pBuffer->nOffset > pBuffer->nAllocLen)
   {
      if(ptr != NULL)
         st->config.ilcs_mem_unlock(pBuffer);

      return OMX_ErrorBadParameter;
   }

   if(exe.bufferLen)
   {
      if(exe.bufferLen + sizeof(IL_PASS_BUFFER_EXECUTE_T) <= VC_ILCS_MAX_INLINE)
      {
         // Pass the data in the message itself, and avoid doing a bulk transfer at all...
         exe.method = IL_BUFFER_INLINE;

         data2 = ptr;
         len2 = exe.bufferLen;
      }
      else
      {
         // Pass the misaligned area at the start at end inline within the
         // message, and the bulk of the message using a separate bulk
         // transfer
         const uint8_t *start = ptr;
         const uint8_t *end   = start + exe.bufferLen;
         const uint8_t *round_start = (const OMX_U8*)ILCS_ROUND_UP(start);
         const uint8_t *round_end   = (const OMX_U8*)ILCS_ROUND_DOWN(end);

         exe.method = IL_BUFFER_BULK;

         if(st->use_memmgr)
         {
            bulk_offset = (void *) (round_start-(ptr-pBuffer->nOffset));
            mem_handle = (VCHI_MEM_HANDLE_T) pBuffer->pBuffer;
         }
         else
            bulk_offset = (void *) round_start;

         bulk_len = round_end-round_start;

         if((fixup.headerlen = round_start - start) > 0)
            memcpy(fixup.header, start, fixup.headerlen);

         if((fixup.trailerlen = end - round_end) > 0)
            memcpy(fixup.trailer, round_end, fixup.trailerlen);

         data2 = &fixup;
         len2 = sizeof(fixup);
      }
   }
   else
   {
      exe.method = IL_BUFFER_NONE;
   }

   // when used for callbacks to client, no need for response
   // so only set ret when use component to component
   if(func == IL_EMPTY_THIS_BUFFER || func == IL_FILL_THIS_BUFFER)
      ret = &resp;

   if(ilcs_execute_function_ex(st, func, &exe, sizeof(IL_PASS_BUFFER_EXECUTE_T),
                               data2, len2, mem_handle, bulk_offset, bulk_len, ret, &rlen) < 0 || rlen != sizeof(resp))
   {
      ret = &resp;
      resp.err = OMX_ErrorHardware;
   }

   if(ptr != NULL)
      st->config.ilcs_mem_unlock(pBuffer);

   return ret ? resp.err : OMX_ErrorNone;
}

/* ----------------------------------------------------------------------
 * receive a buffer via the IL component service.
 * -------------------------------------------------------------------- */

OMX_BUFFERHEADERTYPE *ilcs_receive_buffer(ILCS_SERVICE_T *st, void *call, int clen, OMX_COMPONENTTYPE **pComp)
{
   IL_PASS_BUFFER_EXECUTE_T *exe = call;
   OMX_BUFFERHEADERTYPE *pHeader = exe->bufferHeader.pInputPortPrivate;
   OMX_U8 *dest, *pBuffer = pHeader->pBuffer;
   OMX_PTR *pAppPrivate = pHeader->pAppPrivate;
   OMX_PTR *pPlatformPrivate = pHeader->pPlatformPrivate;
   OMX_PTR *pInputPortPrivate = pHeader->pInputPortPrivate;
   OMX_PTR *pOutputPortPrivate = pHeader->pOutputPortPrivate;

   if(st->kill_service)
      return NULL;

   vcos_assert(pHeader);
   memcpy(pHeader, &exe->bufferHeader, sizeof(OMX_BUFFERHEADERTYPE));

   *pComp = exe->reference;

   pHeader->pBuffer = pBuffer;
   pHeader->pAppPrivate = pAppPrivate;
   pHeader->pPlatformPrivate = pPlatformPrivate;
   pHeader->pInputPortPrivate = pInputPortPrivate;
   pHeader->pOutputPortPrivate = pOutputPortPrivate;

   dest = st->config.ilcs_mem_lock(pHeader) + pHeader->nOffset;

   if(exe->method == IL_BUFFER_BULK)
   {
      IL_BUFFER_BULK_T *fixup = (IL_BUFFER_BULK_T *) (exe+1);
      VCHI_MEM_HANDLE_T mem_handle = VCHI_MEM_HANDLE_INVALID;
      void *bulk_offset;
      int32_t bulk_len = exe->bufferLen - fixup->headerlen - fixup->trailerlen;

      vcos_assert(clen == sizeof(IL_PASS_BUFFER_EXECUTE_T) + sizeof(IL_BUFFER_BULK_T));

      if(st->use_memmgr)
      {
         mem_handle = (VCHI_MEM_HANDLE_T) pBuffer;
         bulk_offset = (void*)(pHeader->nOffset + fixup->headerlen);
      }
      else
         bulk_offset = dest + fixup->headerlen;

#ifdef USE_VCHIQ_ARM
      vchiq_queue_bulk_receive_handle(st->service, mem_handle, bulk_offset, bulk_len, NULL);
#else
      vchiq_queue_bulk_receive(st->vchiq, st->fourcc, mem_handle, bulk_offset, bulk_len, NULL);
#endif

      vcos_event_wait(&st->bulk_rx);

      if(st->kill_service)
      {
         // the bulk receive was aborted, and we're about the quit, however this function
         // being called means the buffer header control message made it across, so we
         // need to think that this buffer is on VideoCore.  So pretend this is all okay,
         // but zero the buffer contents so we don't process bad data
         pHeader->nFilledLen = 0;
         pHeader->nFlags = 0;
      }
      else if(fixup->headerlen || fixup->trailerlen)
      {
         uint8_t *end = dest + exe->bufferLen;

         if(fixup->headerlen)
            memcpy(dest, fixup->header, fixup->headerlen);
         if(fixup->trailerlen)
            memcpy(end-fixup->trailerlen, fixup->trailer, fixup->trailerlen);
      }
   }
   else if(exe->method == IL_BUFFER_INLINE)
   {
      IL_BUFFER_INLINE_T *buffer = (IL_BUFFER_INLINE_T *) (exe+1);

      vcos_assert(clen == sizeof(IL_PASS_BUFFER_EXECUTE_T) + exe->bufferLen);
      memcpy(dest, buffer->buffer, exe->bufferLen);
   }
   else if(exe->method == IL_BUFFER_NONE)
   {
      vcos_assert(clen == sizeof(IL_PASS_BUFFER_EXECUTE_T));
   }
   else
   {
      vcos_assert(0);
   }

   st->config.ilcs_mem_unlock(pHeader);
   return pHeader;
}
