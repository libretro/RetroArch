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
#include <string.h>
#include <stdio.h>
#include "vchost_platform_config.h"
#include "vchost.h"

#include "interface/vcos/vcos.h"
#include "interface/vchi/vchi.h"
#include "interface/vchi/common/endian.h"
#include "interface/vchi/message_drivers/message.h"
#include "vc_cecservice.h"
#include "vc_service_common.h"

/******************************************************************************
Local types and defines.
******************************************************************************/
#ifndef _min
#define _min(x,y) (((x) <= (y))? (x) : (y))
#endif
#ifndef _max
#define _max(x,y) (((x) >= (y))? (x) : (y))
#endif

//TV service host side state (mostly the same as Videocore side - TVSERVICE_STATE_T)
typedef struct {
   //Generic service stuff
   VCHI_SERVICE_HANDLE_T client_handle[VCHI_MAX_NUM_CONNECTIONS]; //To connect to server on VC
   VCHI_SERVICE_HANDLE_T notify_handle[VCHI_MAX_NUM_CONNECTIONS]; //For incoming notification
   uint32_t              msg_flag[VCHI_MAX_NUM_CONNECTIONS];
   char                  command_buffer[CECSERVICE_MSGFIFO_SIZE];
   char                  response_buffer[CECSERVICE_MSGFIFO_SIZE];
   uint32_t              response_length;
   uint32_t              notify_buffer[CECSERVICE_MSGFIFO_SIZE/sizeof(uint32_t)];
   uint32_t              notify_length;
   uint32_t              num_connections;
   VCOS_MUTEX_T          lock;
   CECSERVICE_CALLBACK_T notify_fn;
   void                 *notify_data;
   int                   initialised;
   int                   to_exit;

   //CEC state, not much here
   //Most things live on Videocore side
   uint16_t               physical_address; //16-bit packed physical address
   CEC_DEVICE_TYPE_T      logical_address;  //logical address
   VC_CEC_TOPOLOGY_T     *topology; //16-byte aligned for the transfer

} CECSERVICE_HOST_STATE_T;

/******************************************************************************
Static data.
******************************************************************************/
static CECSERVICE_HOST_STATE_T cecservice_client;
static VCOS_EVENT_T cecservice_message_available_event;
static VCOS_EVENT_T cecservice_notify_available_event;
static VCOS_THREAD_T cecservice_notify_task;
static uint32_t cecservice_log_initialised = 0;

//Command strings - must match what's in vc_cecservice_defs.h
static char* cecservice_command_strings[] = {
   "register_cmd",
   "register_all",
   "deregister_cmd",
   "deregister_all",
   "send_msg",
   "get_logical_addr",
   "alloc_logical_addr",
   "release_logical_addr",
   "get_topology",
   "set_vendor_id",
   "set_osd_name",
   "get_physical_addr",
   "get_vendor_id",
   "poll_addr",
   "set_logical_addr",
   "add_device",
   "set_passive",
   "end_of_list"
};

static const uint32_t max_command_strings = sizeof(cecservice_command_strings)/sizeof(char *);

//Notification strings - must match vc_cec.h VC_CEC_NOTIFY_T
static char* cecservice_notify_strings[] = {
   "none",
   "TX",
   "RX",
   "User Ctrl Pressed",
   "User Ctrl Released",
   "Vendor Remote Down",
   "Vendor Remote Up",
   "logical address",
   "topology",
   "logical address lost",
   "???"
};

static const uint32_t max_notify_strings = sizeof(cecservice_notify_strings)/sizeof(char *);

//Device type strings
static char* cecservice_devicetype_strings[] = {
   "TV",
   "Rec",
   "Reserved",
   "Tuner",
   "Playback",
   "Audio",
   "Switch",
   "VidProc",
   "8", "9", "10", "11", "12", "13", "14", "invalid"
};

static const uint32_t max_devicetype_strings = sizeof(cecservice_devicetype_strings)/sizeof(char *);

/******************************************************************************
Static functions.
******************************************************************************/
//Lock the host state
static __inline int lock_obtain (void) {
   VCOS_STATUS_T status = VCOS_EAGAIN;
   if(cecservice_client.initialised && (status = vcos_mutex_lock(&cecservice_client.lock)) == VCOS_SUCCESS) {
      if(cecservice_client.initialised) { // check service hasn't been closed while we were waiting for the lock.
         vchi_service_use(cecservice_client.client_handle[0]);
         return status;
      } else {
         vcos_mutex_unlock(&cecservice_client.lock);
         vc_cec_log_error("CEC Service closed while waiting for lock");
         return VCOS_EAGAIN;
      }
   }
   vc_cec_log_error("CEC service failed to obtain lock, initialised:%d, lock status:%d",
                    cecservice_client.initialised, status);
   return status;
}

//Unlock the host state
static __inline void lock_release (void) {
   if(cecservice_client.initialised) {
      vchi_service_release(cecservice_client.client_handle[0]);
   }
   vcos_mutex_unlock(&cecservice_client.lock);
}

//Forward declarations
static void cecservice_client_callback( void *callback_param,
                                       VCHI_CALLBACK_REASON_T reason,
                                       void *msg_handle );

static void cecservice_notify_callback( void *callback_param,
                                      VCHI_CALLBACK_REASON_T reason,
                                      void *msg_handle );

static int32_t cecservice_wait_for_reply(void *response, uint32_t max_length);

static int32_t cecservice_wait_for_bulk_receive(void *buffer, uint32_t max_length);

static int32_t cecservice_send_command( uint32_t command, const void *buffer, uint32_t length, uint32_t has_reply);

static int32_t cecservice_send_command_reply( uint32_t command, void *buffer, uint32_t length,
                                              void *response, uint32_t max_length);

static void *cecservice_notify_func(void *arg);

static void cecservice_logging_init(void);

/******************************************************************************
 Global data
*****************************************************************************/
VCOS_LOG_CAT_T cechost_log_category;

/******************************************************************************
CEC service API
******************************************************************************/
/******************************************************************************
NAME
   vc_vchi_cec_init

SYNOPSIS
  void vc_vchi_cec_init(VCHI_INSTANCE_T initialise_instance, VCHI_CONNECTION_T **connections, uint32_t num_connections )

FUNCTION
  Initialise the CEC service for use.  A negative return value
   indicates failure (which may mean it has not been started on VideoCore).

RETURNS
   int
******************************************************************************/
VCHPRE_ void VCHPOST_ vc_vchi_cec_init(VCHI_INSTANCE_T initialise_instance, VCHI_CONNECTION_T **connections, uint32_t num_connections ) {
   int32_t success = -1;
   VCOS_STATUS_T status;
   VCOS_THREAD_ATTR_T attrs;
   uint32_t i;

   if (cecservice_client.initialised)
     return;

   vc_cec_log_info("Initialising CEC service");
   // record the number of connections
   vcos_memset( &cecservice_client, 0, sizeof(CECSERVICE_HOST_STATE_T) );
   cecservice_client.num_connections = num_connections;
   cecservice_client.physical_address = CEC_CLEAR_ADDR;
   cecservice_client.logical_address = CEC_AllDevices_eUnRegistered;

   status = vcos_mutex_create(&cecservice_client.lock, "HCEC");
   vcos_assert(status == VCOS_SUCCESS);
   status = vcos_event_create(&cecservice_message_available_event, "HCEC");
   vcos_assert(status == VCOS_SUCCESS);
   status = vcos_event_create(&cecservice_notify_available_event, "HCEC");
   vcos_assert(status == VCOS_SUCCESS);

   cecservice_client.topology = vcos_malloc_aligned(sizeof(VC_CEC_TOPOLOGY_T), 16, "HCEC topology");
   vcos_assert(cecservice_client.topology);

   for (i=0; i < cecservice_client.num_connections; i++) {

      // Create a 'Client' service on the each of the connections
      SERVICE_CREATION_T cecservice_parameters = { VCHI_VERSION(VC_CECSERVICE_VER),
                                                   CECSERVICE_CLIENT_NAME,     // 4cc service code
                                                   connections[i],             // passed in fn ptrs
                                                   0,                          // tx fifo size (unused)
                                                   0,                          // tx fifo size (unused)
                                                   &cecservice_client_callback,// service callback
                                                   &cecservice_message_available_event,  // callback parameter
                                                   VC_FALSE,                   // want_unaligned_bulk_rx
                                                   VC_FALSE,                   // want_unaligned_bulk_tx
                                                   VC_FALSE,                   // want_crc
      };

      SERVICE_CREATION_T cecservice_parameters2 = { VCHI_VERSION(VC_CECSERVICE_VER),
                                                    CECSERVICE_NOTIFY_NAME,    // 4cc service code
                                                    connections[i],            // passed in fn ptrs
                                                    0,                         // tx fifo size (unused)
                                                    0,                         // tx fifo size (unused)
                                                    &cecservice_notify_callback,// service callback
                                                    &cecservice_notify_available_event,  // callback parameter
                                                    VC_FALSE,                  // want_unaligned_bulk_rx
                                                    VC_FALSE,                  // want_unaligned_bulk_tx
                                                    VC_FALSE,                  // want_crc
      };

      //Create the client to normal CEC service
      success = vchi_service_open( initialise_instance, &cecservice_parameters, &cecservice_client.client_handle[i] );
      vcos_assert( success == 0 );
      if(success)
         vc_cec_log_error("Failed to connected to CEC service: %d", success);

      //Create the client to the async CEC service (any CEC related notifications)
      success = vchi_service_open( initialise_instance, &cecservice_parameters2, &cecservice_client.notify_handle[i] );
      vcos_assert( success == 0 );

      if(success)
         vc_cec_log_error("Failed to connected to CEC async service: %d", success);

      vchi_service_release(cecservice_client.client_handle[i]);
      vchi_service_release(cecservice_client.notify_handle[i]);
   }

   //Create the notifier task
   vcos_thread_attr_init(&attrs);
   vcos_thread_attr_setstacksize(&attrs, 2048);
   vcos_thread_attr_settimeslice(&attrs, 1);

   //Initialise logging
   cecservice_logging_init();

   status = vcos_thread_create(&cecservice_notify_task, "HCEC Notify", &attrs, cecservice_notify_func, &cecservice_client);
   vcos_assert(status == VCOS_SUCCESS);

   cecservice_client.initialised = 1;
   vc_cec_log_info("CEC service initialised");
}

/***********************************************************
 * Name: vc_vchi_cec_stop
 *
 * Arguments:
 *       -
 *
 * Description: Stops the Host side part of CEC service
 *
 * Returns: -
 *
 ***********************************************************/
VCHPRE_ void VCHPOST_ vc_vchi_cec_stop( void ) {
   // Wait for the current lock-holder to finish before zapping TV service
   uint32_t i;

   if (!cecservice_client.initialised)
      return;

   if(lock_obtain() == 0)
   {
      void *dummy;
      vchi_service_release(cecservice_client.client_handle[0]);
      vc_cec_log_info("Stopping CEC service");
      for (i=0; i < cecservice_client.num_connections; i++) {
         int32_t result;
         vchi_service_use(cecservice_client.client_handle[i]);
         vchi_service_use(cecservice_client.notify_handle[i]);
         result = vchi_service_close(cecservice_client.client_handle[i]);
         vcos_assert( result == 0 );
         result = vchi_service_close(cecservice_client.notify_handle[i]);
         vcos_assert( result == 0 );
      }
      cecservice_client.initialised = 0;

      lock_release();
      cecservice_client.to_exit = 1;
      vcos_event_signal(&cecservice_notify_available_event);
      vcos_thread_join(&cecservice_notify_task, &dummy);
      vcos_mutex_delete(&cecservice_client.lock);
      vcos_event_delete(&cecservice_message_available_event);
      vcos_event_delete(&cecservice_notify_available_event);
      vcos_free(cecservice_client.topology);
      vc_cec_log_info("CEC service stopped");
   }
}

/***********************************************************
 * Name: vc_cec_register_callaback
 *
 * Arguments:
 *       callback function, context to be passed when function is called
 *
 * Description: Register a callback function for all CEC notifications
 *
 * Returns: -
 *
 ***********************************************************/
VCHPRE_ void VCHPOST_ vc_cec_register_callback(CECSERVICE_CALLBACK_T callback, void *callback_data) {
   if(lock_obtain() == 0){
      cecservice_client.notify_fn   = callback;
      cecservice_client.notify_data = callback_data;
      vc_cec_log_info("CEC service registered callback 0x%x", (uint32_t) callback);
      lock_release();
   } else {
      vc_cec_log_error("CEC service registered callback 0x%x failed", (uint32_t) callback);
   }
}

/*********************************************************************************
 *
 *  Static functions definitions
 *
 *********************************************************************************/
//TODO: Might need to handle multiple connections later
/***********************************************************
 * Name: cecservice_client_callback
 *
 * Arguments: semaphore, callback reason and message handle
 *
 * Description: Callback when a message is available for CEC service
 *
 ***********************************************************/
static void cecservice_client_callback( void *callback_param,
                                        const VCHI_CALLBACK_REASON_T reason,
                                        void *msg_handle ) {

   VCOS_EVENT_T *event = (VCOS_EVENT_T *)callback_param;

   if ( reason != VCHI_CALLBACK_MSG_AVAILABLE || event == NULL)
      return;

   vcos_event_signal(event);
}

/***********************************************************
 * Name: cecservice_notify_callback
 *
 * Arguments: semaphore, callback reason and message handle
 *
 * Description: Callback when a message is available for CEC notify service
 *
 ***********************************************************/
static void cecservice_notify_callback( void *callback_param,
                                        const VCHI_CALLBACK_REASON_T reason,
                                        void *msg_handle ) {
   VCOS_EVENT_T *event = (VCOS_EVENT_T *)callback_param;

   if ( reason != VCHI_CALLBACK_MSG_AVAILABLE || event == NULL)
      return;

   vcos_event_signal(event);
}

/***********************************************************
 * Name: cecservice_wait_for_reply
 *
 * Arguments: response buffer, buffer length
 *
 * Description: blocked until something is in the buffer
 *
 * Returns zero if successful or error code of vchi otherwise (see vc_service_common_defs.h)
 *         If success, response is updated
 *
 ***********************************************************/
static int32_t cecservice_wait_for_reply(void *response, uint32_t max_length) {
   int32_t success = 0;
   uint32_t length_read = 0;
   do {
      //TODO : we need to deal with messages coming through on more than one connections properly
      //At the moment it will always try to read the first connection if there is something there
      //Check if there is something in the queue, if so return immediately
      //otherwise wait for the semaphore and read again
      success = (int32_t) vchi2service_status(vchi_msg_dequeue( cecservice_client.client_handle[0], response, max_length, &length_read, VCHI_FLAGS_NONE ));
   } while( length_read == 0 && vcos_event_wait(&cecservice_message_available_event) == VCOS_SUCCESS);
   if(length_read) {
      vc_cec_log_info("CEC service got reply %d bytes", length_read);
   } else {
      vc_cec_log_warn("CEC service wait for reply failed, error: %s",
                      vchi2service_status_string(success));
   }

   return success;
}

/***********************************************************
 * Name: cecservice_wait_for_bulk_receive
 *
 * Arguments: response buffer, buffer length
 *
 * Description: blocked until bulk receive
 *
 * Returns error code of vchi
 *
 ***********************************************************/
static int32_t cecservice_wait_for_bulk_receive(void *buffer, uint32_t max_length) {
   if(!vcos_verify(buffer)) {
      vc_cec_log_error("CEC: NULL buffer passed to wait_for_bulk_receive");
      return -1;
   }
   return (int32_t) vchi2service_status(vchi_bulk_queue_receive( cecservice_client.client_handle[0],
                                                                 buffer,
                                                                 max_length,
                                                                 VCHI_FLAGS_BLOCK_UNTIL_OP_COMPLETE,
                                                                 NULL ));
}

/***********************************************************
 * Name: cecservice_send_command
 *
 * Arguments: command, parameter buffer, parameter legnth, has reply? (non-zero means yes)
 *
 * Description: send a command and optionally wait for its single value response (TV_GENERAL_RESP_T)
 *
 * Returns: < 0 if there is VCHI error, if tranmission is successful, value
 *          returned is the response from CEC server (which will be VC_CEC_ERROR_T (>= 0))
 *
 ***********************************************************/

static int32_t cecservice_send_command(  uint32_t command, const void *buffer, uint32_t length, uint32_t has_reply) {
   VCHI_MSG_VECTOR_T vector[] = { {&command, sizeof(command)},
                                  {buffer, length} };
   int32_t success = 0;
   int32_t response = -1;
   vc_cec_log_info("CEC sending command %s length %d %s",
                   cecservice_command_strings[command], length,
                   (has_reply)? "has reply" : " no reply");
   if(lock_obtain() == 0)
   {
      success =  (int32_t) vchi2service_status(vchi_msg_queuev(cecservice_client.client_handle[0],
                                                               vector, sizeof(vector)/sizeof(vector[0]),
                                                               VCHI_FLAGS_BLOCK_UNTIL_QUEUED, NULL ));
      if(success == VC_SERVICE_VCHI_SUCCESS && has_reply) {
         //otherwise only wait for a reply if we ask for one
         success = cecservice_wait_for_reply(&response, sizeof(response));
         if(success == VC_SERVICE_VCHI_SUCCESS) {
            response = VC_VTOH32(response);
         } else {
            response = success;
         }
      } else {
         if(success != VC_SERVICE_VCHI_SUCCESS)
            vc_cec_log_error("CEC failed to send command %s length %d, error: %s",
                             cecservice_command_strings[command], length,
                             vchi2service_status_string(success));
         //No reply expected or failed to send, send the success code back instead
         response = success;
      }
      lock_release();
   }
   return response;
}

/***********************************************************
 * Name: cecservice_send_command_reply
 *
 * Arguments: command, parameter buffer, parameter legnth, reply buffer, buffer length
 *
 * Description: send a command and wait for its non-single value response (in a buffer)
 *
 * Returns: error code, host app is responsible to do endian translation
 *
 ***********************************************************/
static int32_t cecservice_send_command_reply(  uint32_t command, void *buffer, uint32_t length,
                                               void *response, uint32_t max_length) {
   VCHI_MSG_VECTOR_T vector[] = { {&command, sizeof(command)},
                                  {buffer, length} };
   int32_t success = VC_SERVICE_VCHI_VCHIQ_ERROR, ret = 0;

   vc_cec_log_info("CEC sending command (with reply) %s length %d",
                   cecservice_command_strings[command], length);
   if(lock_obtain() == 0)
   {
      if((ret = vchi_msg_queuev( cecservice_client.client_handle[0],
                                 vector, sizeof(vector)/sizeof(vector[0]),
                                 VCHI_FLAGS_BLOCK_UNTIL_QUEUED, NULL )) ==  VC_SERVICE_VCHI_SUCCESS) {
         success = cecservice_wait_for_reply(response, max_length);
      } else {
         vc_cec_log_error("CEC failed to send command %s length %d, error code %d",
                          cecservice_command_strings[command], length, ret);
      }
      lock_release();
   }
   return success;
}

/***********************************************************
 * Name: cecservice_notify_func
 *
 * Arguments: CEC service state
 *
 * Description: This is the notification task which receives all CEC
 *              service notifications
 *
 * Returns: does not return
 *
 ***********************************************************/
static void *cecservice_notify_func(void *arg) {
   int32_t success;
   CECSERVICE_HOST_STATE_T *state = (CECSERVICE_HOST_STATE_T *) arg;

   vc_cec_log_info("CEC service async thread started");
   while(1) {
      VCOS_STATUS_T status = vcos_event_wait(&cecservice_notify_available_event);
      uint32_t cb_reason_str_idx = max_notify_strings - 1;
      if(status != VCOS_SUCCESS || !state->initialised || state->to_exit)
         break;

      do {
         uint32_t reason, param1, param2, param3, param4;
         //Get all notifications in the queue
         success = vchi_msg_dequeue( state->notify_handle[0], state->notify_buffer, sizeof(state->notify_buffer), &state->notify_length, VCHI_FLAGS_NONE );
         if(success != 0 || state->notify_length < sizeof(uint32_t)*5 ) { //reason + 4x32-bit parameter
            vcos_assert(state->notify_length == sizeof(uint32_t)*5);
            break;
         }

         //if(lock_obtain() != 0)
         //   break;
         //All notifications are of format: reason, param1, param2, param3, param4 (all 32-bit unsigned int)
         reason = VC_VTOH32(state->notify_buffer[0]);
         param1 = VC_VTOH32(state->notify_buffer[1]);
         param2 = VC_VTOH32(state->notify_buffer[2]);
         param3 = VC_VTOH32(state->notify_buffer[3]);
         param4 = VC_VTOH32(state->notify_buffer[4]);
         //lock_release();

         //Store away physical/logical addresses
         if(CEC_CB_REASON(reason) == VC_CEC_LOGICAL_ADDR) {
            state->logical_address = (CEC_DEVICE_TYPE_T) param1;
            state->physical_address = (uint16_t) (param2 & 0xFFFF);
         }

         switch(CEC_CB_REASON(reason)) {
         case VC_CEC_NOTIFY_NONE:
            cb_reason_str_idx = 0; break;
         case VC_CEC_TX:
            cb_reason_str_idx = 1; break;
         case VC_CEC_RX:
            cb_reason_str_idx = 2; break;
         case VC_CEC_BUTTON_PRESSED:
            cb_reason_str_idx = 3; break;
         case VC_CEC_BUTTON_RELEASE:
            cb_reason_str_idx = 4; break;
         case VC_CEC_REMOTE_PRESSED:
            cb_reason_str_idx = 5; break;
         case VC_CEC_REMOTE_RELEASE:
            cb_reason_str_idx = 6; break;
         case VC_CEC_LOGICAL_ADDR:
            cb_reason_str_idx = 7; break;
         case VC_CEC_TOPOLOGY:
            cb_reason_str_idx = 8; break;
         case VC_CEC_LOGICAL_ADDR_LOST:
            cb_reason_str_idx = 9; break;
         }

         vc_cec_log_info("CEC service callback [%s]: 0x%x, 0x%x, 0x%x, 0x%x",
                         cecservice_notify_strings[cb_reason_str_idx], param1, param2, param3, param4);

         if(state->notify_fn) {
            (*state->notify_fn)(state->notify_data, reason, param1, param2, param3, param4);
         } else {
            vc_cec_log_info("CEC service: No callback handler specified, callback [%s] swallowed",
                            cecservice_notify_strings[cb_reason_str_idx]);
         }

      } while(success == 0 && state->notify_length >= sizeof(uint32_t)*5); //read the next message if any
   } //while (1)

   if(state->to_exit)
      vc_cec_log_info("CEC service async thread exiting");

   return 0;
}

/***********************************************************
 * Name: cecservice_logging_init
 *
 * Arguments: None
 *
 * Description: Initialise VCOS logging
 *
 * Returns: -
 *
 ***********************************************************/
static void cecservice_logging_init() {
   if(cecservice_log_initialised == 0) {
      vcos_log_set_level(&cechost_log_category, VCOS_LOG_WARN);
      vcos_log_register("cecservice-client", &cechost_log_category);
      vc_cec_log_info("CEC HOST: log initialised");
      cecservice_log_initialised = 1;
   }
}

/***********************************************************
 Actual CEC service API starts here
***********************************************************/
/***********************************************************
 * Name: vc_cec_register_command (deprecated)
 *
 * Arguments:
 *
 * Description
 *
 * Returns: zero
 ***********************************************************/
VCHPRE_ int VCOS_DEPRECATED("has no effect") VCHPOST_ vc_cec_register_command(CEC_OPCODE_T opcode) {
   return 0;
}

/***********************************************************
 * Name: vc_cec_register_all (deprecated)
 *
 * Arguments:
 *
 * Description
 *
 * Returns: zero
 ***********************************************************/
VCHPRE_ int VCOS_DEPRECATED("has no effect") VCHPOST_ vc_cec_register_all( void ) {
    return 0;
}

/***********************************************************
 * Name: vc_cec_deregister_command (deprecated)
 *
 * Arguments:
 *
 * Description
 *
 * Returns: zero
 ***********************************************************/
VCHPRE_ int VCOS_DEPRECATED("has no effect") VCHPOST_ vc_cec_deregister_command(CEC_OPCODE_T opcode) {
   return 0;
}

/***********************************************************
 * Name: vc_cec_deregister_all (deprecated)
 *
 * Arguments:
 *
 * Description
 *
 * Returns: zero
 ***********************************************************/
VCHPRE_ int VCOS_DEPRECATED("has no effect") VCHPOST_ vc_cec_deregister_all( void ) {
    return 0;
}

/***********************************************************
 * Name: vc_cec_send_message
 *
 * Arguments:
 *       Follower's logical address
 *       Message payload WITHOUT the header byte (can be NULL)
 *       Payload length WITHOUT the header byte (can be zero)
 *       VC_TRUE if the message is a reply to an incoming message
 *       (For poll message set payload to NULL and length to zero)
 *
 * Description
 *       Remove all commands to be forwarded. This does not affect
 *       the button presses which are always forwarded
 *
 * Returns: if the command is successful (zero) or not (non-zero)
 *          If the command is successful, there will be a Tx callback
 *          in due course to indicate whether the message has been
 *          acknowledged by the recipient or not
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_send_message(const uint32_t follower,
                                         const uint8_t *payload,
                                         uint32_t length,
                                         vcos_bool_t is_reply) {
   int success = -1;
   CEC_SEND_MSG_PARAM_T param;
   if(!vcos_verify(length <= CEC_MAX_XMIT_LENGTH))
      return -1;

   param.follower = VC_HTOV32(follower);
   param.length = VC_HTOV32(length);
   param.is_reply = VC_HTOV32(is_reply);
   vcos_memset(param.payload, 0, sizeof(param.payload));
   vc_cec_log_info("CEC service sending CEC message (%d->%d) (0x%02X) length %d%s",
                   cecservice_client.logical_address, follower,
                   (payload)? payload[0] : 0xFF, length, (is_reply)? " as reply" : "");

   if(length > 0 && vcos_verify(payload)) {
      char s[96] = {0}, *p = &s[0];
      int i;
      vcos_memcpy(param.payload, payload, _min(length, CEC_MAX_XMIT_LENGTH));
      p += sprintf(p, "0x%02X",  (cecservice_client.logical_address << 4) | (follower & 0xF));
      for(i = 0; i < _min(length, CEC_MAX_XMIT_LENGTH); i++) {
         p += sprintf(p, " %02X", payload[i]);
      }
      vc_cec_log_info("CEC message: %s", s);
   }

   success = cecservice_send_command( VC_CEC_SEND_MSG, &param, sizeof(param), 1);
   return success;
}

/***********************************************************
 * Name: vc_cec_get_logical_address
 *
 * Arguments:
 *       pointer to logical address
 *
 * Description
 *       Get the logical address, if one is being allocated
 *       0xF (unregistered) will be returned
 *
 * Returns: if the command is successful (zero) or not (non-zero)
 *          logical_address is not modified if command failed
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_get_logical_address(CEC_AllDevices_T *logical_address) {
   uint32_t response;
   int32_t success = cecservice_send_command_reply( VC_CEC_GET_LOGICAL_ADDR, NULL, 0,
                                                    &response, sizeof(response));
   if(success == 0) {
      *logical_address = (CEC_AllDevices_T)(VC_VTOH32(response) & 0xF);
      vc_cec_log_info("CEC got logical address %d", *logical_address);
   }
   return success;
}

/***********************************************************
 * Name: vc_cec_alloc_logical_address
 *
 * Arguments:
 *       None
 *
 * Description
 *       Start the allocation of a logical address. The host only
 *       needs to call this if the initial allocation failed
 *       (logical address being 0xF and physical address is NOT 0xFFFF
 *        from VC_CEC_LOGICAL_ADDR notification), or if the host explicitly
 *       released its logical address.
 *
 * Returns: if the command is successful (zero) or not (non-zero)
 *         If successful, there will be a callback notification
 *         VC_CEC_LOGICAL_ADDR. The host should wait for this before
 *         calling this function again.
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_alloc_logical_address( void ) {
   return cecservice_send_command( VC_CEC_ALLOC_LOGICAL_ADDR, NULL, 0, 0);
}

/***********************************************************
 * Name: vc_cec_release_logical_address
 *
 * Arguments:
 *       None
 *
 * Description
 *       Release our logical address. This effectively disables CEC.
 *       The host will need to allocate a new logical address before
 *       doing any CEC calls (send/receive message, get topology, etc.).
 *
 * Returns: if the command is successful (zero) or not (non-zero)
 *         The host should get a callback VC_CEC_LOGICAL_ADDR with
 *         0xF being the logical address and the current physical address.
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_release_logical_address( void ) {
   return cecservice_send_command( VC_CEC_RELEASE_LOGICAL_ADDR, NULL, 0, 0);
}

/***********************************************************
 * Name: vc_cec_get_topology (deprecated)
 *
 * Arguments:
 *
 * Description
 *
 * Returns: if the command is successful (zero) or not (non-zero)
 *
 ***********************************************************/
VCHPRE_ int VCOS_DEPRECATED("returns invalid result") VCHPOST_ vc_cec_get_topology( VC_CEC_TOPOLOGY_T* topology) {
   int32_t success = -1;
   vchi_service_use(cecservice_client.client_handle[0]);
   success = cecservice_send_command( VC_CEC_GET_TOPOLOGY, NULL, 0, 1);
   if(success == 0) {
      success = cecservice_wait_for_bulk_receive(cecservice_client.topology, sizeof(VC_CEC_TOPOLOGY_T));
   }
   vchi_service_release(cecservice_client.client_handle[0]);
   if(success == 0) {
      int i;
      cecservice_client.topology->active_mask = VC_VTOH16(cecservice_client.topology->active_mask);
      cecservice_client.topology->num_devices = VC_VTOH16(cecservice_client.topology->num_devices);
      vc_cec_log_info("CEC topology: mask=0x%x; #device=%d",
                      cecservice_client.topology->active_mask,
                      cecservice_client.topology->num_devices);
      for(i = 0; i < 15; i++) {
         cecservice_client.topology->device_attr[i] = VC_VTOH32(cecservice_client.topology->device_attr[i]);
      }
      vcos_memcpy(topology, cecservice_client.topology, sizeof(VC_CEC_TOPOLOGY_T));
   }
   return success;
}

/***********************************************************
 * Name: vc_cec_set_vendor_id
 *
 * Arguments:
 *       24-bit IEEE vendor id
 *
 * Description
 *       Set the response to <Give Device Vendor ID>
 *
 * Returns: if the command is successful (zero) or not (non-zero)
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_set_vendor_id( uint32_t id ) {
   uint32_t vendor_id = VC_HTOV32(id);
   vc_cec_log_info("CEC setting vendor id to 0x%x", vendor_id);
   return cecservice_send_command( VC_CEC_SET_VENDOR_ID, &vendor_id, sizeof(vendor_id), 0);
}

/***********************************************************
 * Name: vc_cec_set_osd_name
 *
 * Arguments:
 *       OSD name (14 byte array)
 *
 * Description
 *       Set the response to <Give OSD Name>
 *
 * Returns: if the command is successful (zero) or not (non-zero)
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_set_osd_name( const char* name ) {
   vc_cec_log_info("CEC setting OSD name to %s", name);
   return cecservice_send_command( VC_CEC_SET_OSD_NAME, name, OSD_NAME_LENGTH, 0);
}

/***********************************************************
 * Name: vc_cec_get_physical_address
 *
 * Arguments:
 *       pointer to physical address (returned as 16-bit packed value)
 *
 * Description
 *       Get the physical address
 *
 * Returns: if the command is successful (zero) or not (non-zero)
 *          If failed, physical address argument will not be changed
 *          A physical address of 0xFFFF means CEC is not supported
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_get_physical_address(uint16_t *physical_address) {
   uint32_t response;
   int32_t success = cecservice_send_command_reply( VC_CEC_GET_PHYSICAL_ADDR, NULL, 0,
                                                    &response, sizeof(response));
   if(success == 0) {
      *physical_address = (uint16_t)(VC_VTOH32(response) & 0xFFFF);
      vc_cec_log_info("CEC got physical address: %d.%d.%d.%d",
                      (*physical_address >> 12), (*physical_address >> 8) & 0xF,
                      (*physical_address >> 4) & 0xF, (*physical_address) & 0xF);
   }
   return success;
}

/***********************************************************
 * Name: vc_cec_get_vendor_id
 *
 * Arguments:
 *       logical address [in]
 *       pointer to 24-bit IEEE vendor id [out]
 *
 * Description
 *       Get the vendor ID of the device with the said logical address
 *       Application should send <Give Device Vendor ID> if vendor ID
 *       is not known (and register opcode <Device Vendor ID>)
 *
 * Returns: if the command is successful (zero) or not (non-zero)
 *          vendor ID is set to zero if unknown or 0xFFFFFF if
 *          device does not exist.
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_get_vendor_id( const CEC_AllDevices_T logical_address, uint32_t *vendor_id) {
   uint32_t log_addr = VC_HTOV32(logical_address);
   uint32_t response;
   int32_t success = cecservice_send_command_reply( VC_CEC_GET_VENDOR_ID, &log_addr, sizeof(log_addr),
                                                    &response, sizeof(response));
   if(success == 0) {
      vcos_assert(vendor_id);
      *vendor_id = VC_VTOH32(response);
      vc_cec_log_info("CEC got vendor id 0x%X", *vendor_id);
   }
   return success;
}
/***********************************************************
 * Name: vc_cec_device_type
 *
 * Arguments:
 *       logical address [in]
 *
 * Description
 *       Get the default device type of a logical address
 *       Logical address 12 to 14 cannot be used
 *
 * Returns: For logical addresses 0-11 the default
 *          device type of that address will be returned
 *          logical address 12-14 will return "reserved" type.
 *
 ***********************************************************/
VCHPRE_ CEC_DEVICE_TYPE_T VCHPOST_ vc_cec_device_type(const CEC_AllDevices_T logical_address) {
   CEC_DEVICE_TYPE_T device_type = CEC_DeviceType_Invalid;
   switch(logical_address) {
   case CEC_AllDevices_eSTB1:
   case CEC_AllDevices_eSTB2:
   case CEC_AllDevices_eSTB3:
   case CEC_AllDevices_eSTB4:
      device_type = CEC_DeviceType_Tuner;
      break;
   case CEC_AllDevices_eDVD1:
   case CEC_AllDevices_eDVD2:
   case CEC_AllDevices_eDVD3:
      device_type = CEC_DeviceType_Playback;
      break;
   case CEC_AllDevices_eRec1:
   case CEC_AllDevices_eRec2:
   case CEC_AllDevices_eRec3:
      device_type = CEC_DeviceType_Rec;
      break;
   case CEC_AllDevices_eAudioSystem:
      device_type = CEC_DeviceType_Audio;
      break;
   case CEC_AllDevices_eTV:
      device_type = CEC_DeviceType_TV;
      break;
   case CEC_AllDevices_eRsvd3:
   case CEC_AllDevices_eRsvd4:
   case CEC_AllDevices_eFreeUse:
      device_type = CEC_DeviceType_Reserved; //XXX: Are we allowed to use this?
      break;
   default:
      vcos_assert(0); //Invalid
      break;
   }
   return device_type;
}

/***********************************************************
 * Name: vc_cec_send_message2
 *
 * Arguments:
 *       pointer to encapsulated message
 *
 * Description
 *       Call vc_cec_send_message above
 *       messages are always sent as non-reply
 *
 * Returns: if the command is successful (zero) or not (non-zero)
 *          If the command is successful, there will be a Tx callback
 *          in due course to indicate whether the message has been
 *          acknowledged by the recipient or not
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_send_message2(const VC_CEC_MESSAGE_T *message) {
   if(vcos_verify(message)) {
      return vc_cec_send_message(message->follower,
                                 (message->length)?
                                 message->payload : NULL,
                                 message->length,
                                 VC_FALSE);
   } else {
      return -1;
   }
}

/***********************************************************
 * Name: vc_cec_param2message
 *
 * Arguments:
 *       arguments from CEC callback (reason, param1 to param4)
 *       pointer to VC_CEC_MESSAGE_T
 *
 * Description
 *       Turn the CEC_TX/CEC_RX/BUTTON_PRESS/BUTTON_RELEASE
 *       callback parameters back into an encapsulated form
 *
 * Returns: zero normally
 *          non-zero if something has gone wrong
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_param2message( const uint32_t reason, const uint32_t param1,
                                           const uint32_t param2, const uint32_t param3,
                                           const uint32_t param4, VC_CEC_MESSAGE_T *message) {
   if(vcos_verify(message &&
                  CEC_CB_REASON(reason) != VC_CEC_LOGICAL_ADDR &&
                  CEC_CB_REASON(reason) != VC_CEC_TOPOLOGY)) {
      message->length = CEC_CB_MSG_LENGTH(reason) - 1; //Length is without the header byte
      message->initiator = CEC_CB_INITIATOR(param1);
      message->follower = CEC_CB_FOLLOWER(param1);
      if(message->length) {
         uint32_t tmp = param1 >> 8;
         vcos_memcpy(message->payload, &tmp, sizeof(uint32_t)-1);
         vcos_memcpy(message->payload+sizeof(uint32_t)-1, &param2, sizeof(uint32_t));
         vcos_memcpy(message->payload+sizeof(uint32_t)*2-1, &param3, sizeof(uint32_t));
         vcos_memcpy(message->payload+sizeof(uint32_t)*3-1, &param4, sizeof(uint32_t));
      } else {
         vcos_memset(message->payload, 0, sizeof(message->payload));
      }
      return 0;
   } else {
      return -1;
   }
}

//Extra API if CEC is running in passive mode

/***********************************************************
 * Name: vc_cec_poll_address
 *
 * Arguments:
 *       logical address to try
 *
 * Description
 *       Sets and polls a particular address to find out
 *       its availability in the CEC network. Only available
 *       when CEC is running in passive mode. The host can
 *       only call this function during logical address allocation stage.
 *
 * Returns: 0 if poll is successful (address is occupied)
 *         >0 if poll is unsuccessful (address is free if error code is VC_CEC_ERROR_NO_ACK)
 *         <0 other (VCHI) errors
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_poll_address(const CEC_AllDevices_T logical_address) {
   uint32_t log_addr = VC_HTOV32(logical_address);
   int32_t response =  VC_CEC_ERROR_INVALID_ARGUMENT;
   int32_t success = -1;
   vc_cec_log_info("CEC polling address %d", logical_address);
   success = cecservice_send_command_reply( VC_CEC_POLL_ADDR, &log_addr, sizeof(log_addr),
                                                    &response, sizeof(response));
   return (success == 0)? response : success;
}

/***********************************************************
 * Name: vc_cec_set_logical_address
 *
 * Arguments:
 *       logical address, device type, vendor id
 *
 * Description
 *       sets the logical address, device type and vendor ID to be in use.
 *       Only available when CEC is running in passive mode. It is the
 *       responsibility of the host to make sure the logical address
 *       is actually free (see vc_cec_poll_address). Physical address used
 *       will be what is read from EDID and cannot be set.
 *
 * Returns: 0 if successful, non-zero otherwise
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_set_logical_address(const CEC_AllDevices_T logical_address,
                                                const CEC_DEVICE_TYPE_T device_type,
                                                const uint32_t vendor_id) {
   CEC_SET_LOGICAL_ADDR_PARAM_T param = {VC_HTOV32(logical_address),
                                         VC_HTOV32(device_type),
                                         VC_HTOV32(vendor_id)};
   int32_t response = VC_CEC_ERROR_INVALID_ARGUMENT;
   int32_t success = VC_CEC_ERROR_INVALID_ARGUMENT;
   if(vcos_verify(logical_address <= CEC_AllDevices_eUnRegistered &&
                  (device_type <= CEC_DeviceType_VidProc ||
                   device_type == CEC_DeviceType_Invalid))) {
      vc_cec_log_info("CEC setting logical address to %d; device type %s; vendor 0x%X",
                      logical_address,
                      cecservice_devicetype_strings[device_type], vendor_id );
      success = cecservice_send_command_reply( VC_CEC_SET_LOGICAL_ADDR, &param, sizeof(param),
                                               &response, sizeof(response));
   } else {
      vc_cec_log_error("CEC invalid arguments for set_logical_address");
   }
   return (success == 0)? response : success;
}

/***********************************************************
 * Name: vc_cec_add_device (deprecated)
 *
 * Arguments:
 *
 * Description
 *
 * Returns: 0 if successful, non-zero otherwise
 ***********************************************************/
VCHPRE_ int  VCOS_DEPRECATED("has no effect") VCHPOST_ vc_cec_add_device(const CEC_AllDevices_T logical_address,
                                       const uint16_t physical_address,
                                       const CEC_DEVICE_TYPE_T device_type,
                                       vcos_bool_t last_device) {
   CEC_ADD_DEVICE_PARAM_T param = {VC_HTOV32(logical_address),
                                   VC_HTOV32(physical_address),
                                   VC_HTOV32(device_type),
                                   VC_HTOV32(last_device)};
   int32_t response =  VC_CEC_ERROR_INVALID_ARGUMENT;
   int32_t success = VC_CEC_ERROR_INVALID_ARGUMENT;
   if(vcos_verify(logical_address <= CEC_AllDevices_eUnRegistered &&
                  (device_type <= CEC_DeviceType_VidProc ||
                   device_type == CEC_DeviceType_Invalid))) {
      vc_cec_log_info("CEC adding device %d (0x%X); device type %s",
                      logical_address, physical_address,
                      cecservice_devicetype_strings[device_type]);
      success = cecservice_send_command_reply( VC_CEC_ADD_DEVICE, &param, sizeof(param),
                                               &response, sizeof(response));
   } else {
      vc_cec_log_error("CEC invalid arguments for add_device");
   }
   return (success == 0)? response : success;
}

/***********************************************************
 * Name: vc_cec_set_passive
 *
 * Arguments:
 *       Enable/disable (TRUE to enable/ FALSE to disable)
 *
 * Description
 * Enable / disable CEC passive mode
 *
 * Returns: 0 if successful, non-zero otherwise
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_set_passive(vcos_bool_t enabled) {
   uint32_t param = VC_HTOV32(enabled);
   int32_t response;
   int32_t success = cecservice_send_command_reply( VC_CEC_SET_PASSIVE, &param, sizeof(param),
                                                    &response, sizeof(response));
   return (success == 0)? response : success;
}

/***********************************************************
 API for some common CEC messages, uses the API above to
 actually send the message
***********************************************************/

/***********************************************************
 * Name: vc_cec_send_FeatureAbort
 *
 * Arguments:
 *       follower, rejected opcode, reject reason, reply or not
 *
 * Description
 *       send <Feature Abort> for a received command
 *
 * Returns: if the command is successful (zero) or not (non-zero)
 *          Tx callback if successful
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_send_FeatureAbort(uint32_t follower,
                                              CEC_OPCODE_T opcode,
                                              CEC_ABORT_REASON_T reason) {
   uint8_t tx_buf[3];
   tx_buf[0] = CEC_Opcode_FeatureAbort;    // <Feature Abort>
   tx_buf[1] = opcode;
   tx_buf[2] = reason;
   return vc_cec_send_message(follower,
                              tx_buf,
                              sizeof(tx_buf),
                              VC_TRUE);
}

/***********************************************************
 * Name: vc_cec_send_ActiveSource
 *
 * Arguments:
 *       physical address, reply or not
 *
 * Description
 *       send <Active Source>
 *
 * Returns: if the command is successful (zero) or not (non-zero)
 *          Tx callback if successful
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_send_ActiveSource(uint16_t physical_address,
                                              vcos_bool_t is_reply) {
   uint8_t tx_buf[3];
   tx_buf[0] = CEC_Opcode_ActiveSource;    // <Active Source>
   tx_buf[1] = physical_address >> 8;      // physical address msb
   tx_buf[2] = physical_address & 0x00FF;  // physical address lsb
   return vc_cec_send_message(CEC_BROADCAST_ADDR, // This is a broadcast only message
                              tx_buf,
                              sizeof(tx_buf),
                              is_reply);
}

/***********************************************************
 * Name: vc_cec_send_ImageViewOn
 *
 * Arguments:
 *       follower, reply or not
 * Description
 *       send <Image View On>
 *
 * Returns: if the command is successful (zero) or not (non-zero)
 *          Tx callback if successful
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_send_ImageViewOn(uint32_t follower,
                                             vcos_bool_t is_reply) {
   uint8_t tx_buf[1];
   tx_buf[0] = CEC_Opcode_ImageViewOn;      // <Image View On> no param required
   return vc_cec_send_message(follower,
                              tx_buf,
                              sizeof(tx_buf),
                              is_reply);
}

/***********************************************************
 * Name: vc_cec_send_SetOSDString
 *
 * Arguments:
 *       follower, display control, string (char[13]), reply or not
 *
 * Description
 *       send <Image View On>
 *
 * Returns: if the command is successful (zero) or not (non-zero)
 *          Tx callback if successful
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_send_SetOSDString(uint32_t follower,
                                              CEC_DISPLAY_CONTROL_T disp_ctrl,
                                              const char* string,
                                              vcos_bool_t is_reply) {
   uint8_t tx_buf[CEC_MAX_XMIT_LENGTH];
   tx_buf[0] = CEC_Opcode_SetOSDString;     // <Set OSD String>
   tx_buf[1] = disp_ctrl;
   vcos_memset(&tx_buf[2], 0, sizeof(tx_buf)-2);
   vcos_memcpy(&tx_buf[2], string, _min(strlen(string), CEC_MAX_XMIT_LENGTH-2));
   return vc_cec_send_message(follower,
                              tx_buf,
                              sizeof(tx_buf),
                              is_reply);
}

/***********************************************************
 * Name: vc_cec_send_Standby
 *
 * Arguments:
 *       follower, reply or not
 *
 * Description
 *       send <Standby>. Turn other devices to standby
 *
 * Returns: if the command is successful (zero) or not (non-zero)
 *          Tx callback if successful
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_send_Standby(uint32_t follower, vcos_bool_t is_reply) {
   uint8_t tx_buf[1];
   tx_buf[0] = CEC_Opcode_Standby;           // <Standby>
   return vc_cec_send_message(follower,
                              tx_buf,
                              sizeof(tx_buf),
                              is_reply);
}

/***********************************************************
 * Name: vc_cec_send_MenuStatus
 *
 * Arguments:
 *       follower, menu state, reply or not
 *
 * Description
 *       send <Menu Status> (response to <Menu Request>)
 *       menu state is either CEC_MENU_STATE_ACTIVATED or CEC_MENU_STATE_DEACTIVATED
 *
 * Returns: if the command is successful (zero) or not (non-zero)
 *          Tx callback if successful
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_send_MenuStatus(uint32_t follower,
                                            CEC_MENU_STATE_T menu_state,
                                            vcos_bool_t is_reply) {
   uint8_t tx_buf[2];
   if(!vcos_verify(menu_state < CEC_MENU_STATE_QUERY))
      return -1;

   tx_buf[0] = CEC_Opcode_MenuStatus;        // <Menu Status>
   tx_buf[1] = menu_state;
   return vc_cec_send_message(follower,
                              tx_buf,
                              sizeof(tx_buf),
                              is_reply);
}

/***********************************************************
 * Name: vc_cec_send_ReportPhysicalAddress
 *
 * Arguments:
 *       physical address, device type, reply or not
 *
 * Description
 *       send <Report Physical Address> (first command to be
 *       sent after a successful logical address allocation
 *       device type should be the appropriate one for
 *       the allocated logical address
 *
 * Returns: if the command is successful (zero) or not (non-zero)
 *          Tx callback if successful. We also get a failure
 *          if we do not currently have a valid physical address
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_send_ReportPhysicalAddress(uint16_t physical_address,
                                                       CEC_DEVICE_TYPE_T device_type,
                                                       vcos_bool_t is_reply) {
   uint8_t tx_buf[4];
   if(vcos_verify(physical_address == cecservice_client.physical_address &&
                  cecservice_client.physical_address != CEC_CLEAR_ADDR)) {
      tx_buf[0] = CEC_Opcode_ReportPhysicalAddress;
      tx_buf[1] = physical_address >> 8;       // physical address msb
      tx_buf[2] = physical_address & 0x00FF;   // physical address lsb
      tx_buf[3] = device_type;                 // 'device type'
      return vc_cec_send_message(CEC_BROADCAST_ADDR, // This is a broadcast only message
                                 tx_buf,
                                 sizeof(tx_buf),
                                 is_reply);
   } else {
      //Current we do not allow sending a random physical address
      vc_cec_log_error("CEC cannot send physical address 0x%X, does not match internal 0x%X",
                       physical_address, cecservice_client.physical_address);
      return VC_CEC_ERROR_NO_PA;
   }
}
