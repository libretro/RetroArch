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

#if !defined(__KERNEL__)
#include <string.h>
#endif

#include "interface/vcos/vcos.h"

#include "vchost_platform_config.h"
#include "vchost.h"

#include "interface/vchi/vchi.h"
#include "interface/vchi/common/endian.h"
#include "interface/vchi/message_drivers/message.h"
#include "vc_tvservice.h"

/******************************************************************************
Local types and defines.
******************************************************************************/
#ifndef _min
#define _min(x,y) (((x) <= (y))? (x) : (y))
#endif
#ifndef _max
#define _max(x,y) (((x) >= (y))? (x) : (y))
#endif

typedef struct
{
   TVSERVICE_CALLBACK_T  notify_fn;
   void                 *notify_data;
} TVSERVICE_HOST_CALLBACK_T;

typedef struct {
   uint32_t is_valid;
   uint32_t max_modes; //How big the table have we allocated
   uint32_t num_modes; //How many valid entries are there
   TV_SUPPORTED_MODE_NEW_T *modes;
} TVSERVICE_MODE_CACHE_T;

//TV service host side state (mostly the same as Videocore side - TVSERVICE_STATE_T)
typedef struct {
   //Generic service stuff
   VCHI_SERVICE_HANDLE_T client_handle[VCHI_MAX_NUM_CONNECTIONS]; //To connect to server on VC
   VCHI_SERVICE_HANDLE_T notify_handle[VCHI_MAX_NUM_CONNECTIONS]; //For incoming notification
   uint32_t              msg_flag[VCHI_MAX_NUM_CONNECTIONS];
   char                  command_buffer[TVSERVICE_MSGFIFO_SIZE];
   char                  response_buffer[TVSERVICE_MSGFIFO_SIZE];
   uint32_t              response_length;
   uint32_t              notify_buffer[TVSERVICE_MSGFIFO_SIZE/sizeof(uint32_t)];
   uint32_t              notify_length;
   uint32_t              num_connections;
   VCOS_MUTEX_T          lock;
   TVSERVICE_HOST_CALLBACK_T  callbacks[TVSERVICE_MAX_CALLBACKS];
   int                   initialised;
   int                   to_exit;

   //TV stuff
   uint32_t              copy_protect;

   //HDMI specific stuff
   HDMI_RES_GROUP_T      hdmi_current_group;
   HDMI_MODE_T           hdmi_current_mode;
   HDMI_DISPLAY_OPTIONS_T hdmi_options;

   //If client ever asks for supported modes, we store them for quick return
   HDMI_RES_GROUP_T      hdmi_preferred_group;
   uint32_t              hdmi_preferred_mode;
   TVSERVICE_MODE_CACHE_T dmt_cache;
   TVSERVICE_MODE_CACHE_T cea_cache;

   //SDTV specific stuff
   SDTV_COLOUR_T         sdtv_current_colour;
   SDTV_MODE_T           sdtv_current_mode;
   SDTV_OPTIONS_T        sdtv_options;
   SDTV_CP_MODE_T        sdtv_current_cp_mode;
} TVSERVICE_HOST_STATE_T;

/******************************************************************************
Static data.
******************************************************************************/
static TVSERVICE_HOST_STATE_T tvservice_client;
static VCOS_EVENT_T tvservice_message_available_event;
static VCOS_EVENT_T tvservice_notify_available_event;
static VCOS_THREAD_T tvservice_notify_task;

#define  VCOS_LOG_CATEGORY (&tvservice_log_category)
static VCOS_LOG_CAT_T  tvservice_log_category;

//Command strings - must match what's in vc_tvservice_defs.h
static char* tvservice_command_strings[] = {
   "get_state",
   "hdmi_on_preferred",
   "hdmi_on_best",
   "hdmi_on_explicit",
   "sdtv_on",
   "off",
   "query_supported_modes",
   "query_mode_support",
   "query_audio_support",
   "enable_copy_protect",
   "disable_copy_protect",
   "show_info",
   "get_av_latency",
   "hdcp_set_key",
   "hdcp_set_srm",
   "set_spd",
   "set_display_options",
   "test_mode_start",
   "test_mode_stop",
   "ddc_read",
   "set_attached",
   "set_property",
   "get_property",
   "get_display_state",
   "end_of_list"
};

/******************************************************************************
Static functions.
******************************************************************************/
//Lock the host state
static __inline int tvservice_lock_obtain (void) {
   if(tvservice_client.initialised && vcos_mutex_lock(&tvservice_client.lock) == VCOS_SUCCESS) {
      //Check again in case the service has been stopped
      if (tvservice_client.initialised) {
         vchi_service_use(tvservice_client.client_handle[0]);
         return 0;
      }
      else
         vcos_mutex_unlock(&tvservice_client.lock);
   }

   return -1;
}

//Unlock the host state
static __inline void tvservice_lock_release (void) {
   if (tvservice_client.initialised) {
      vchi_service_release(tvservice_client.client_handle[0]);
   }
   vcos_mutex_unlock(&tvservice_client.lock);
}

//Forward declarations
static void tvservice_client_callback( void *callback_param,
                                      VCHI_CALLBACK_REASON_T reason,
                                      void *msg_handle );

static void tvservice_notify_callback( void *callback_param,
                                      VCHI_CALLBACK_REASON_T reason,
                                      void *msg_handle );

static int32_t tvservice_wait_for_reply(void *response, uint32_t max_length);

static int32_t tvservice_wait_for_bulk_receive(void *buffer, uint32_t max_length);

static int32_t tvservice_send_command( uint32_t command, void *buffer, uint32_t length, uint32_t has_reply);

static int32_t tvservice_send_command_reply( uint32_t command, void *buffer, uint32_t length,
                                             void *response, uint32_t max_length);

static void *tvservice_notify_func(void *arg);


/******************************************************************************
TV service API
******************************************************************************/
/******************************************************************************
NAME
   vc_vchi_tv_init

SYNOPSIS
   int vc_vchi_tv_init(VCHI_INSTANCE_T initialise_instance, VCHI_CONNECTION_T **connections, uint32_t num_connections )

FUNCTION
   Initialise the TV service for use. A negative return value
   indicates failure (which may mean it has not been started on VideoCore).

RETURNS
   int
******************************************************************************/
VCHPRE_ int VCHPOST_ vc_vchi_tv_init(VCHI_INSTANCE_T initialise_instance, VCHI_CONNECTION_T **connections, uint32_t num_connections ) {
   int32_t success;
   uint32_t i;
   VCOS_STATUS_T status;
   VCOS_THREAD_ATTR_T attrs;
   static const HDMI_DISPLAY_OPTIONS_T hdmi_default_display_options =
      {
         HDMI_ASPECT_UNKNOWN,
         VC_FALSE, 0, 0, // No vertical bar information.
         VC_FALSE, 0, 0, // No horizontal bar information.
         0               // No overscan flags.
      };

   if (tvservice_client.initialised)
     return -2;

   vcos_log_set_level(VCOS_LOG_CATEGORY, VCOS_LOG_ERROR);
   vcos_log_register("tvservice-client", VCOS_LOG_CATEGORY);

   vcos_log_trace("[%s]", VCOS_FUNCTION);

   // record the number of connections
   memset( &tvservice_client, 0, sizeof(TVSERVICE_HOST_STATE_T) );
   tvservice_client.num_connections = num_connections;

   status = vcos_mutex_create(&tvservice_client.lock, "HTV");
   vcos_assert(status == VCOS_SUCCESS);
   status = vcos_event_create(&tvservice_message_available_event, "HTV");
   vcos_assert(status == VCOS_SUCCESS);
   status = vcos_event_create(&tvservice_notify_available_event, "HTV");
   vcos_assert(status == VCOS_SUCCESS);

   //Initialise any other non-zero bits of the TV service state here
   tvservice_client.sdtv_current_mode = SDTV_MODE_OFF;
   tvservice_client.sdtv_options.aspect = SDTV_ASPECT_4_3;
   memcpy(&tvservice_client.hdmi_options, &hdmi_default_display_options, sizeof(HDMI_DISPLAY_OPTIONS_T));

   for (i=0; i < tvservice_client.num_connections; i++) {

      // Create a 'Client' service on the each of the connections
      SERVICE_CREATION_T tvservice_parameters = { VCHI_VERSION(VC_TVSERVICE_VER),
                                                  TVSERVICE_CLIENT_NAME,      // 4cc service code
                                                  connections[i],             // passed in fn ptrs
                                                  0,                          // tx fifo size (unused)
                                                  0,                          // tx fifo size (unused)
                                                  &tvservice_client_callback, // service callback
                                                  &tvservice_message_available_event,  // callback parameter
                                                  VC_TRUE,                    // want_unaligned_bulk_rx
                                                  VC_TRUE,                    // want_unaligned_bulk_tx
                                                  VC_FALSE,                   // want_crc
      };

      SERVICE_CREATION_T tvservice_parameters2 = { VCHI_VERSION(VC_TVSERVICE_VER),
                                                   TVSERVICE_NOTIFY_NAME,     // 4cc service code
                                                   connections[i],            // passed in fn ptrs
                                                   0,                         // tx fifo size (unused)
                                                   0,                         // tx fifo size (unused)
                                                   &tvservice_notify_callback,// service callback
                                                   &tvservice_notify_available_event,  // callback parameter
                                                   VC_FALSE,                  // want_unaligned_bulk_rx
                                                   VC_FALSE,                  // want_unaligned_bulk_tx
                                                   VC_FALSE,                  // want_crc
      };

      //Create the client to normal TV service
      success = vchi_service_open( initialise_instance, &tvservice_parameters, &tvservice_client.client_handle[i] );

      //Create the client to the async TV service (any TV related notifications)
      if (success == 0)
      {
         success = vchi_service_open( initialise_instance, &tvservice_parameters2, &tvservice_client.notify_handle[i] );
         if (success == 0)
         {
            vchi_service_release(tvservice_client.client_handle[i]);
            vchi_service_release(tvservice_client.notify_handle[i]);
         }
         else
         {
            vchi_service_close(tvservice_client.client_handle[i]);
            vcos_log_error("Failed to connect to async TV service: %d", success);
         }
      } else {
         vcos_log_error("Failed to connect to TV service: %d", success);
      }

      if (success != 0)
      {
         while (i > 0)
         {
            --i;
            vchi_service_close(tvservice_client.client_handle[i]);
            vchi_service_close(tvservice_client.notify_handle[i]);
         }
         return -1;
      }
   }

   //Create the notifier task
   vcos_thread_attr_init(&attrs);
   vcos_thread_attr_setstacksize(&attrs, 4096);
   vcos_thread_attr_settimeslice(&attrs, 1);

   status = vcos_thread_create(&tvservice_notify_task, "HTV Notify", &attrs, tvservice_notify_func, &tvservice_client);
   vcos_assert(status == VCOS_SUCCESS);

   tvservice_client.initialised = 1;
   vcos_log_trace("TV service initialised");

   return 0;
}

/***********************************************************
 * Name: vc_vchi_tv_stop
 *
 * Arguments:
 *       -
 *
 * Description: Stops the Host side part of TV service
 *
 * Returns: -
 *
 ***********************************************************/
VCHPRE_ void VCHPOST_ vc_vchi_tv_stop( void ) {
   // Wait for the current lock-holder to finish before zapping TV service
   uint32_t i;

   if (!tvservice_client.initialised)
      return;

   vcos_log_trace("[%s]", VCOS_FUNCTION);
   if(tvservice_lock_obtain() == 0)
   {
      void *dummy;
      vchi_service_release(tvservice_client.client_handle[0]); // to match the use in tvservice_lock_obtain()

      for (i=0; i < tvservice_client.num_connections; i++) {
         int32_t result;
         vchi_service_use(tvservice_client.client_handle[i]);
         vchi_service_use(tvservice_client.notify_handle[i]);
         result = vchi_service_close(tvservice_client.client_handle[i]);
         vcos_assert( result == 0 );
         result = vchi_service_close(tvservice_client.notify_handle[i]);
         vcos_assert( result == 0 );
      }

      // Unset the initialise flag so no other threads can obtain the lock
      tvservice_client.initialised = 0;

      tvservice_lock_release();
      tvservice_client.to_exit = 1; //Signal to quit
      vcos_event_signal(&tvservice_notify_available_event);
      vcos_thread_join(&tvservice_notify_task, &dummy);

      if(tvservice_client.cea_cache.modes)
         vcos_free(tvservice_client.cea_cache.modes);

      if(tvservice_client.dmt_cache.modes)
         vcos_free(tvservice_client.dmt_cache.modes);

      vcos_mutex_delete(&tvservice_client.lock);
      vcos_event_delete(&tvservice_message_available_event);
      vcos_event_delete(&tvservice_notify_available_event);
   }
}


/***********************************************************
 * Name: vc_tv_register_callback
 *
 * Arguments:
 *       callback function, context to be passed when function is called
 *
 * Description: Register a callback function for all TV notifications
 *
 * Returns: -
 *
 ***********************************************************/
VCHPRE_ void VCHPOST_ vc_tv_register_callback(TVSERVICE_CALLBACK_T callback, void *callback_data) {

   vcos_assert_msg(callback != NULL, "Use vc_tv_unregister_callback() to remove a callback");

   vcos_log_trace("[%s]", VCOS_FUNCTION);
   if(tvservice_lock_obtain() == 0)
   {
      uint32_t done = 0;
      uint32_t i;
      for(i = 0; (i < TVSERVICE_MAX_CALLBACKS) && !done; i++)
      {
         if(tvservice_client.callbacks[i].notify_fn == NULL)
         {
            tvservice_client.callbacks[i].notify_fn = callback;
            tvservice_client.callbacks[i].notify_data = callback_data;
            done = 1;
         } // if
      } // for
      vcos_assert(done);
      tvservice_lock_release();
   }
}

/***********************************************************
 * Name: vc_tv_unregister_callback
 *
 * Arguments:
 *       callback function
 *
 * Description: Unregister a previously-registered callback function for TV notifications
 *
 * Returns: -
 *
 ***********************************************************/
VCHPRE_ void VCHPOST_ vc_tv_unregister_callback(TVSERVICE_CALLBACK_T callback)
{
   vcos_assert(callback != NULL);

   vcos_log_trace("[%s]", VCOS_FUNCTION);
   if(tvservice_lock_obtain() == 0)
   {
      uint32_t done = 0;
      uint32_t i;
      for(i = 0; (i < TVSERVICE_MAX_CALLBACKS) && !done; i++)
      {
         if(tvservice_client.callbacks[i].notify_fn == callback)
         {
            tvservice_client.callbacks[i].notify_fn = NULL;
            tvservice_client.callbacks[i].notify_data = NULL;
            done = 1;
         } // if
      } // for
      vcos_assert(done);
      tvservice_lock_release();
   }
}

/***********************************************************
 * Name: vc_tv_unregister_callback_full
 *
 * Arguments:
 *       callback function
 *       callback function context
 *
 * Description: Unregister a previously-registered callback function for TV notifications
 *
 * Returns: -
 *
 ***********************************************************/
VCHPRE_ void VCHPOST_ vc_tv_unregister_callback_full(TVSERVICE_CALLBACK_T callback, void *callback_data)
{
   vcos_assert(callback != NULL);

   vcos_log_trace("[%s]", VCOS_FUNCTION);
   if(tvservice_lock_obtain() == 0)
   {
      uint32_t done = 0;
      uint32_t i;
      for(i = 0; (i < TVSERVICE_MAX_CALLBACKS) && !done; i++)
      {
         if(tvservice_client.callbacks[i].notify_fn == callback &&
            tvservice_client.callbacks[i].notify_data == callback_data)
         {
            tvservice_client.callbacks[i].notify_fn = NULL;
            tvservice_client.callbacks[i].notify_data = NULL;
            done = 1;
         } // if
      } // for
      vcos_assert(done);
      tvservice_lock_release();
   }
}

/*********************************************************************************
 *
 *  Static functions definitions
 *
 *********************************************************************************/
//TODO: Might need to handle multiple connections later
/***********************************************************
 * Name: tvservice_client_callback
 *
 * Arguments: semaphore, callback reason and message handle
 *
 * Description: Callback when a message is available for TV service
 *
 ***********************************************************/
static void tvservice_client_callback( void *callback_param,
                                       const VCHI_CALLBACK_REASON_T reason,
                                       void *msg_handle ) {
   VCOS_EVENT_T *event = (VCOS_EVENT_T *)callback_param;

   vcos_log_trace("[%s]", VCOS_FUNCTION);
   (void)msg_handle;

   if ( reason != VCHI_CALLBACK_MSG_AVAILABLE || event == NULL)
      return;

   vcos_event_signal(event);
}

/***********************************************************
 * Name: tvservice_notify_callback
 *
 * Arguments: semaphore, callback reason and message handle
 *
 * Description: Callback when a message is available for TV notify service
 *
 ***********************************************************/
static void tvservice_notify_callback( void *callback_param,
                                       const VCHI_CALLBACK_REASON_T reason,
                                       void *msg_handle ) {
   VCOS_EVENT_T *event = (VCOS_EVENT_T *)callback_param;

   vcos_log_trace("[%s]", VCOS_FUNCTION);
   (void)msg_handle;

   if ( reason != VCHI_CALLBACK_MSG_AVAILABLE || event == NULL)
      return;

   vcos_event_signal(event);
}

/***********************************************************
 * Name: tvservice_wait_for_reply
 *
 * Arguments: response buffer, buffer length
 *
 * Description: blocked until something is in the buffer
 *
 * Returns error code of vchi
 *
 ***********************************************************/
static int32_t tvservice_wait_for_reply(void *response, uint32_t max_length) {
   int32_t success = 0;
   uint32_t length_read = 0;
   vcos_log_trace("[%s]", VCOS_FUNCTION);
   do {
      //TODO : we need to deal with messages coming through on more than one connections properly
      //At the moment it will always try to read the first connection if there is something there
      //Check if there is something in the queue, if so return immediately
      //otherwise wait for the semaphore and read again
      success = vchi_msg_dequeue( tvservice_client.client_handle[0], response, max_length, &length_read, VCHI_FLAGS_NONE );
   } while( length_read == 0 && vcos_event_wait(&tvservice_message_available_event) == VCOS_SUCCESS);
   if(length_read) {
      vcos_log_trace("TV service got reply %d bytes", length_read);
   } else {
      vcos_log_warn("TV service wait for reply failed");
   }
   return success;
}

/***********************************************************
 * Name: tvservice_wait_for_bulk_receive
 *
 * Arguments: response buffer, buffer length
 *
 * Description: blocked until bulk receive
 *
 * Returns error code of vchi
 *
 ***********************************************************/
static int32_t tvservice_wait_for_bulk_receive(void *buffer, uint32_t max_length) {
   /*if(!vcos_verify(((uint32_t) buffer & 0xf) == 0)) //should be 16 byte aligned
      return -1;*/
   vcos_log_trace("[%s]", VCOS_FUNCTION);
   if(!vcos_verify(buffer)) { 
      vcos_log_error("TV service: NULL buffer passed to wait_for_bulk_receive");
      return -1;
   }

   return vchi_bulk_queue_receive( tvservice_client.client_handle[0],
                                   buffer,
                                   max_length,
                                   VCHI_FLAGS_BLOCK_UNTIL_OP_COMPLETE,
                                   NULL );
}

/***********************************************************
 * Name: tvservice_send_command
 *
 * Arguments: command, parameter buffer, parameter length, has reply? (non-zero means yes)
 *
 * Description: send a command and optionally wait for its single value response (TV_GENERAL_RESP_T)
 *
 * Returns: response.ret (currently only 1 int in the wrapped response), endian translated if necessary
 *
 ***********************************************************/

static int32_t tvservice_send_command(  uint32_t command, void *buffer, uint32_t length, uint32_t has_reply) {
   VCHI_MSG_VECTOR_T vector[] = { {&command, sizeof(command)},
                                  {buffer, length} };
   int32_t success = 0;
   TV_GENERAL_RESP_T response;
   response.ret = -1;

   if(vcos_verify(command < VC_TV_END_OF_LIST)) {
      vcos_log_trace("[%s] command:%s param length %d %s", VCOS_FUNCTION,
                     tvservice_command_strings[command], length, 
                     (has_reply)? "has reply" : " no reply");
   } else {
      vcos_log_error("[%s] not sending invalid command %d", VCOS_FUNCTION, command);
      return -1;
   }

   if(tvservice_lock_obtain() == 0)
   {
      success = vchi_msg_queuev( tvservice_client.client_handle[0],
                                 vector, sizeof(vector)/sizeof(vector[0]),
                                 VCHI_FLAGS_BLOCK_UNTIL_QUEUED, NULL );
      if(success == 0 && has_reply) {
         //otherwise only wait for a reply if we ask for one
         success = tvservice_wait_for_reply(&response, sizeof(response));
         response.ret = VC_VTOH32(response.ret);
      } else {
         if(success != 0)
            vcos_log_error("TV service failed to send command %s length %d, error code %d",
                           tvservice_command_strings[command], length, success);
         //No reply expected or failed to send, send the success code back instead
         response.ret = success;
      }
      tvservice_lock_release();
   }
   return response.ret;
}

/***********************************************************
 * Name: tvservice_send_command_reply
 *
 * Arguments: command, parameter buffer, parameter length, reply buffer, buffer length
 *
 * Description: send a command and wait for its non-single value response (in a buffer)
 *
 * Returns: error code, host app is responsible to do endian translation
 *
 ***********************************************************/
static int32_t tvservice_send_command_reply(  uint32_t command, void *buffer, uint32_t length,
                                              void *response, uint32_t max_length) {
   VCHI_MSG_VECTOR_T vector[] = { {&command, sizeof(command)},
                                   {buffer, length} };
   int32_t success = 0;

   vcos_log_trace("[%s] sending command (with reply) %s param length %d", VCOS_FUNCTION,
                  tvservice_command_strings[command], length);

   if(tvservice_lock_obtain() == 0)
   {
      success = vchi_msg_queuev( tvservice_client.client_handle[0],
                                 vector, sizeof(vector)/sizeof(vector[0]),
                                 VCHI_FLAGS_BLOCK_UNTIL_QUEUED, NULL );
      if(success == 0)
         success = tvservice_wait_for_reply(response, max_length);
      else
         vcos_log_error("TV service failed to send command %s param length %d, error code %d",
                        tvservice_command_strings[command], length, success);
      
      tvservice_lock_release();
   }

   return success;
}

/***********************************************************
 * Name: tvservice_notify_func
 *
 * Arguments: TV service state
 *
 * Description: This is the notification task which receives all TV
 *              service notifications
 *
 * Returns: does not return
 *
 ***********************************************************/
static void *tvservice_notify_func(void *arg) {
   int32_t success;
   TVSERVICE_HOST_STATE_T *state = (TVSERVICE_HOST_STATE_T *) arg;
   TV_DISPLAY_STATE_T tvstate;

   vcos_log_trace("TV service async thread started");
   /* Check starting state, and put service in use if necessary */
   success = tvservice_send_command_reply( VC_TV_GET_DISPLAY_STATE, NULL, 0, &tvstate, sizeof(TV_DISPLAY_STATE_T));
   if (success != 0)
      return 0;
   if (tvstate.state & VC_HDMI_ATTACHED)
   {
      /* Connected */
      if (tvstate.state & (VC_HDMI_HDMI | VC_HDMI_DVI))
      {
         /* Mode already selected */
         vchi_service_use(state->notify_handle[0]);
      }
   }
   // the state machine below only wants a single bit to be set
   if (tvstate.state & (VC_HDMI_ATTACHED|VC_HDMI_UNPLUGGED))
      tvstate.state &= ~(VC_HDMI_HDMI | VC_HDMI_DVI);

   while(1) {
      VCOS_STATUS_T status = vcos_event_wait(&tvservice_notify_available_event);
      if(status != VCOS_SUCCESS || !state->initialised || state->to_exit)
         break;

      do {
         uint32_t reason, param1, param2;
         //Get all notifications in the queue
         success = vchi_msg_dequeue( state->notify_handle[0], state->notify_buffer, sizeof(state->notify_buffer), &state->notify_length, VCHI_FLAGS_NONE );
         if(success != 0 || state->notify_length < sizeof(uint32_t)*3 ) {
            vcos_assert(state->notify_length == sizeof(uint32_t)*3);
            break;
         }

         if(tvservice_lock_obtain() != 0)
            break;

         //Check what notification it is and update ourselves accordingly before notifying the host app
         //All notifications are of format: reason, param1, param2 (all 32-bit unsigned int)
         reason = VC_VTOH32(state->notify_buffer[0]), param1 = VC_VTOH32(state->notify_buffer[1]), param2 = VC_VTOH32(state->notify_buffer[2]);
         vcos_log_trace("[%s] %s %d %d", VCOS_FUNCTION, vc_tv_notification_name(reason),
                        param1, param2);
         switch(reason) {
         case VC_HDMI_UNPLUGGED:
            if(tvstate.state & (VC_HDMI_HDMI|VC_HDMI_DVI)) {
               state->copy_protect = 0;
               if((tvstate.state & VC_HDMI_ATTACHED) == 0) {
                  vchi_service_release(state->notify_handle[0]);
               }
            }
            tvstate.state &= ~(VC_HDMI_HDMI|VC_HDMI_DVI|VC_HDMI_ATTACHED|VC_HDMI_HDCP_AUTH);
            tvstate.state |= (VC_HDMI_UNPLUGGED | VC_HDMI_HDCP_UNAUTH);
            vcos_log_trace("[%s] invalidating caches", VCOS_FUNCTION);
            state->cea_cache.is_valid = state->cea_cache.num_modes = 0;
            state->dmt_cache.is_valid = state->dmt_cache.num_modes = 0;
            break;

         case VC_HDMI_ATTACHED:
            if(tvstate.state & (VC_HDMI_HDMI|VC_HDMI_DVI)) {
               state->copy_protect = 0;
               vchi_service_release(state->notify_handle[0]);
            }
            tvstate.state &=  ~(VC_HDMI_HDMI|VC_HDMI_DVI|VC_HDMI_UNPLUGGED|VC_HDMI_HDCP_AUTH);
            tvstate.state |= VC_HDMI_ATTACHED;
            state->hdmi_preferred_group = (HDMI_RES_GROUP_T) param1;
            state->hdmi_preferred_mode = param2;
            break;

         case VC_HDMI_DVI:
            if(tvstate.state & (VC_HDMI_ATTACHED|VC_HDMI_UNPLUGGED)) {
               vchi_service_use(state->notify_handle[0]);
            }
            tvstate.state &= ~(VC_HDMI_HDMI|VC_HDMI_ATTACHED|VC_HDMI_UNPLUGGED);
            tvstate.state |= VC_HDMI_DVI;
            state->hdmi_current_group = (HDMI_RES_GROUP_T) param1;
            state->hdmi_current_mode = param2;
            break;

         case VC_HDMI_HDMI:
            if(tvstate.state & (VC_HDMI_ATTACHED|VC_HDMI_UNPLUGGED)) {
               vchi_service_use(state->notify_handle[0]);
            }
            tvstate.state &= ~(VC_HDMI_DVI|VC_HDMI_ATTACHED|VC_HDMI_UNPLUGGED);
            tvstate.state |= VC_HDMI_HDMI;
            state->hdmi_current_group = (HDMI_RES_GROUP_T) param1;
            state->hdmi_current_mode = param2;
            break;

         case VC_HDMI_HDCP_UNAUTH:
            tvstate.state &= ~VC_HDMI_HDCP_AUTH;
            tvstate.state |= VC_HDMI_HDCP_UNAUTH;
            state->copy_protect = 0;
            //Do we care about the reason for HDCP unauth in param1?
            break;

         case VC_HDMI_HDCP_AUTH:
            tvstate.state &= ~VC_HDMI_HDCP_UNAUTH;
            tvstate.state |= VC_HDMI_HDCP_AUTH;
            state->copy_protect = 1;
            break;

         case VC_HDMI_HDCP_KEY_DOWNLOAD:
         case VC_HDMI_HDCP_SRM_DOWNLOAD:
            //Nothing to do here, just tell the host app whether it is successful or not (in param1)
            break;

         case VC_SDTV_UNPLUGGED: //Currently we don't get this
            if(tvstate.state & (VC_SDTV_PAL | VC_SDTV_NTSC)) {
               state->copy_protect = 0;
            }
            tvstate.state &= ~(VC_SDTV_ATTACHED | VC_SDTV_PAL | VC_SDTV_NTSC);
            tvstate.state |= (VC_SDTV_UNPLUGGED | VC_SDTV_CP_INACTIVE);
            state->sdtv_current_mode = SDTV_MODE_OFF;
            break;

         case VC_SDTV_ATTACHED: //Currently we don't get this either
            tvstate.state &= ~(VC_SDTV_UNPLUGGED | VC_SDTV_PAL | VC_SDTV_NTSC);
            tvstate.state |= VC_SDTV_ATTACHED;
            state->sdtv_current_mode = SDTV_MODE_OFF;
            break;

         case VC_SDTV_NTSC:
            tvstate.state &= ~(VC_SDTV_UNPLUGGED | VC_SDTV_ATTACHED | VC_SDTV_PAL);
            tvstate.state |= VC_SDTV_NTSC;
            state->sdtv_current_mode = (SDTV_MODE_T) param1;
            state->sdtv_options.aspect = (SDTV_ASPECT_T) param2;
            if(param1 & SDTV_COLOUR_RGB) {
               state->sdtv_current_colour = SDTV_COLOUR_RGB;
            } else if(param1 & SDTV_COLOUR_YPRPB) {
               state->sdtv_current_colour = SDTV_COLOUR_YPRPB;
            } else {
               state->sdtv_current_colour = SDTV_COLOUR_UNKNOWN;
            }
            break;

         case VC_SDTV_PAL:
            tvstate.state &= ~(VC_SDTV_UNPLUGGED | VC_SDTV_ATTACHED | VC_SDTV_NTSC);
            tvstate.state |= VC_SDTV_PAL;
            state->sdtv_current_mode = (SDTV_MODE_T) param1;
            state->sdtv_options.aspect = (SDTV_ASPECT_T) param2;
            if(param1 & SDTV_COLOUR_RGB) {
               state->sdtv_current_colour = SDTV_COLOUR_RGB;
            } else if(param1 & SDTV_COLOUR_YPRPB) {
               state->sdtv_current_colour = SDTV_COLOUR_YPRPB;
            } else {
               state->sdtv_current_colour = SDTV_COLOUR_UNKNOWN;
            }
            break;

         case VC_SDTV_CP_INACTIVE:
            tvstate.state &= ~VC_SDTV_CP_ACTIVE;
            tvstate.state |= VC_SDTV_CP_INACTIVE;
            state->copy_protect = 0;
            state->sdtv_current_cp_mode = SDTV_CP_NONE;
            break;

         case VC_SDTV_CP_ACTIVE:
            tvstate.state &= ~VC_SDTV_CP_INACTIVE;
            tvstate.state |= VC_SDTV_CP_ACTIVE;
            state->copy_protect = 1;
            state->sdtv_current_cp_mode = (SDTV_CP_MODE_T) param1;
            break;
         }

         tvservice_lock_release();

         //Now callback the host app(s)
         uint32_t i, called = 0;
         for(i = 0; i < TVSERVICE_MAX_CALLBACKS; i++)
         {
            if(state->callbacks[i].notify_fn != NULL)
            {
               called++;
               state->callbacks[i].notify_fn
                  (state->callbacks[i].notify_data, reason, param1, param2);
            } // if
         } // for
         if(called == 0) {
            vcos_log_info("TV service: No callback handler specified, callback [%s] swallowed",
                          vc_tv_notification_name(reason));
         }
      } while(success == 0 && state->notify_length >= sizeof(uint32_t)*3); //read the next message if any
   } //while (1)
   
   if(state->to_exit)
      vcos_log_trace("TV service async thread exiting");

   return 0;
}

/***********************************************************
 Actual TV service API starts here
***********************************************************/

/***********************************************************
 * Name: vc_tv_get_state
 *
 * Arguments:
 *       Pointer to tvstate structure
 *
 * Description: Get the current TV state
 *
 * Returns: if the command is successful (zero) or not (non-zero)
 *          If the command fails to be sent, passed in state is unchanged
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_tv_get_state(TV_GET_STATE_RESP_T *tvstate) {
   int success = -1;

   vcos_log_trace("[%s]", VCOS_FUNCTION);
   if(vcos_verify(tvstate)) {
      success = tvservice_send_command_reply( VC_TV_GET_STATE, NULL, 0,
                                              tvstate, sizeof(TV_GET_STATE_RESP_T));
      if(success == 0) {
         tvstate->state = VC_VTOH32(tvstate->state);
         tvstate->width = VC_VTOH32(tvstate->width);
         tvstate->height = VC_VTOH32(tvstate->height);
         tvstate->frame_rate = VC_VTOH16(tvstate->frame_rate);
         tvstate->scan_mode = VC_VTOH16(tvstate->scan_mode);
      }
   }
   return success;
}

/***********************************************************
 * Name: vc_tv_get_display_state
 *
 * Arguments:
 *       Pointer to tvstate structure
 *
 * Description: Get the current TV display state.
 *
 * Returns: if the command is successful (zero) or not (non-zero)
 *          If the command fails to be sent, passed in state is unchanged
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_tv_get_display_state(TV_DISPLAY_STATE_T *tvstate) {
   int success = -1;

   vcos_log_trace("[%s]", VCOS_FUNCTION);
   if(vcos_verify(tvstate)) {
      success = tvservice_send_command_reply( VC_TV_GET_DISPLAY_STATE, NULL, 0,
                                              tvstate, sizeof(TV_DISPLAY_STATE_T));
   }
   return success;
}

/***********************************************************
 * Name: vc_tv_hdmi_power_on_preferred/vc_tv_hdmi_power_on_preferred_3d
 *
 * Arguments:
 *       none
 *
 * Description: Power on HDMI at preferred resolution
 *              Enter 3d if the _3d function was called
 *
 *              Analogue TV will be powered down if on (same for the following
 *              two HDMI power on functions below)
 *
 * Returns: single value interpreted as HDMI_RESULT_T (zero means success)
 *          if successful, there will be a callback when the power on is complete
 *
 ***********************************************************/
static int vc_tv_hdmi_power_on_preferred_actual(uint32_t in_3d) {
   TV_HDMI_ON_PREFERRED_PARAM_T param;
   int success;

   vcos_log_trace("[%s]", VCOS_FUNCTION);
   param.in_3d = VC_HTOV32(in_3d);

   success = tvservice_send_command( VC_TV_HDMI_ON_PREFERRED, &param, sizeof(param), 1);
   return success;
}

VCHPRE_ int VCHPOST_ vc_tv_hdmi_power_on_preferred( void ) {
   vcos_log_trace("[%s]", VCOS_FUNCTION);
   return vc_tv_hdmi_power_on_preferred_actual(0);
}

VCHPRE_ int VCHPOST_ vc_tv_hdmi_power_on_preferred_3d( void ) {
   vcos_log_trace("[%s]", VCOS_FUNCTION);
   return vc_tv_hdmi_power_on_preferred_actual(1);
}

/***********************************************************
 * Name: vc_tv_hdmi_power_on_best/vc_tv_hdmi_power_on_best_3d
 *
 * Arguments:
 *       screen width, height, frame rate, scan_mode (HDMI_NONINTERLACED / HDMI_INTERLACED)
 *       match flags
 *
 * Description: Power on HDMI at best matched resolution based on passed in parameters
 *              Enter 3d mode if the _3d function was called
 *
 * Returns: single value interpreted as HDMI_RESULT_T (zero means success)
 *          if successful, there will be a callback when the power on is complete
 *
 ***********************************************************/
static int vc_tv_hdmi_power_on_best_actual(uint32_t width, uint32_t height, uint32_t frame_rate,
                                           HDMI_INTERLACED_T scan_mode, EDID_MODE_MATCH_FLAG_T match_flags,
                                           uint32_t in_3d) {
   TV_HDMI_ON_BEST_PARAM_T param;
   int success;

   vcos_log_trace("[%s]", VCOS_FUNCTION);
   param.width = VC_HTOV32(width);
   param.height = VC_HTOV32(height);
   param.frame_rate = VC_HTOV32(frame_rate);
   param.scan_mode = VC_HTOV32(scan_mode);
   param.match_flags = VC_HTOV32(match_flags);
   param.in_3d = VC_HTOV32(in_3d);

   success = tvservice_send_command( VC_TV_HDMI_ON_BEST, &param, sizeof(TV_HDMI_ON_BEST_PARAM_T), 1);
   return success;
}

VCHPRE_ int VCHPOST_ vc_tv_hdmi_power_on_best(uint32_t width, uint32_t height, uint32_t frame_rate,
                                              HDMI_INTERLACED_T scan_mode, EDID_MODE_MATCH_FLAG_T match_flags) {
   vcos_log_trace("[%s]", VCOS_FUNCTION);
   return vc_tv_hdmi_power_on_best_actual(width, height, frame_rate, scan_mode, match_flags, 0);
}

VCHPRE_ int VCHPOST_ vc_tv_hdmi_power_on_best_3d(uint32_t width, uint32_t height, uint32_t frame_rate,
                                              HDMI_INTERLACED_T scan_mode, EDID_MODE_MATCH_FLAG_T match_flags) {
   vcos_log_trace("[%s]", VCOS_FUNCTION);
   return vc_tv_hdmi_power_on_best_actual(width, height, frame_rate, scan_mode, match_flags, 1);
}

/***********************************************************
 * Name: vc_tv_hdmi_power_on_explicit
 *
 * Arguments:
 *       mode (HDMI_MODE_HDMI/HDMI_MODE_DVI),
 *       group (HDMI_RES_GROUP_CEA/HDMI_RES_GROUP_DMT),
 *       code
 *
 * Description: Power on HDMI at explicit mode
 *              Enter 3d mode if supported by the TV and if mode was set to HDMI_MODE_3D
 *              If Videocore has EDID, this will still be subject to EDID restriction,
 *              otherwise HDMI will be powered on at the said mode
 *
 * Returns: single value interpreted as HDMI_RESULT_T (zero means success)
 *          if successful, there will be a callback when the power on is complete
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_tv_hdmi_power_on_explicit_new(HDMI_MODE_T mode, HDMI_RES_GROUP_T group, uint32_t code) {
   TV_HDMI_ON_EXPLICIT_PARAM_T param;
   int success;

   vcos_log_trace("[%s] mode %d group %d code %d", VCOS_FUNCTION,
         mode, group, code);
   param.hdmi_mode = mode;
   param.group = group;
   param.mode = code;

   success = tvservice_send_command( VC_TV_HDMI_ON_EXPLICIT, &param, sizeof(TV_HDMI_ON_EXPLICIT_PARAM_T), 1);
   return success;
}

/***********************************************************
 * Name: vc_tv_sdtv_power_on
 *
 * Arguments:
 *       SDTV mode, options (currently only aspect ratio)
 *
 * Description: Power on SDTV at required mode and aspect ratio (default 4:3)
 *              HDMI will be powered down if currently on
 *
 * Returns: single value (zero means success)
 *          if successful, there will be a callback when the power on is complete
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_tv_sdtv_power_on(SDTV_MODE_T mode, SDTV_OPTIONS_T *options) {
   TV_SDTV_ON_PARAM_T param;
   int success;

   vcos_log_trace("[%s]", VCOS_FUNCTION);
   param.mode = VC_HTOV32(mode);
   param.aspect = (options)? VC_HTOV32(options->aspect) : VC_HTOV32(SDTV_ASPECT_4_3);

   success = tvservice_send_command( VC_TV_SDTV_ON, &param, sizeof(TV_SDTV_ON_PARAM_T), 1);
   return success;
}


/***********************************************************
 * Name: vc_tv_power_off
 *
 * Arguments:
 *       none
 *
 * Description: Power off whatever is on at the moment, no effect if nothing is on
 *
 * Returns: whether command is successfully sent (and callback for HDMI)
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_tv_power_off( void ) {
   vcos_log_trace("[%s]", VCOS_FUNCTION);
   return tvservice_send_command( VC_TV_OFF, NULL, 0, 0);
}

/***********************************************************
 * Name: vc_tv_hdmi_get_supported_modes
 *
 * Arguments:
 *       group (HDMI_RES_GROUP_CEA/HDMI_RES_GROUP_DMT),
 *       array of TV_SUPPORT_MODE_T structs, length of array, pointer to preferred group,
 *       pointer to prefer mode code (the last two pointers can be NULL, if the caller
 *       is not interested to learn what the preferred mode is)
 *       If passed in a null supported_modes array, or the length of array
 *       is zero, the number of supported modes in that particular group
 *       will be returned instead
 *
 * Description: Get supported modes for a particular group,
 *              the length of array limits no. of modes returned
 *
 * Returns: Returns the number of modes actually written (or the number
 *          of supported modes if passed in a null array or length of array==0).
 *          Returns < 0 for error.
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_tv_hdmi_get_supported_modes_new(HDMI_RES_GROUP_T group,
                                                    TV_SUPPORTED_MODE_NEW_T *supported_modes,
                                                    uint32_t max_supported_modes,
                                                    HDMI_RES_GROUP_T *preferred_group,
                                                    uint32_t *preferred_mode) {
   uint32_t param[2] = {(uint32_t) group, 0};
   TV_QUERY_SUPPORTED_MODES_RESP_T response;
   TVSERVICE_MODE_CACHE_T *cache = NULL;
   int error = -1;
   int modes_copied = 0;

   vcos_log_trace("[%s]", VCOS_FUNCTION);

   switch(group) {
      case HDMI_RES_GROUP_DMT:
         cache = &tvservice_client.dmt_cache;
         break;
      case HDMI_RES_GROUP_CEA:
         cache = &tvservice_client.cea_cache;
         break;
      default:
         vcos_log_error("Invalid group %d in [%s]", group, VCOS_FUNCTION);
         return -1;
   }
   vcos_log_trace("[%s] group %d cache valid %d",
                  VCOS_FUNCTION, group, cache->is_valid);

   memset(&response, 0, sizeof(response));
   if(!cache->is_valid) {
      vchi_service_use(tvservice_client.client_handle[0]);
      if((error = tvservice_send_command_reply(VC_TV_QUERY_SUPPORTED_MODES, &param[0], sizeof(uint32_t),
                                               &response, sizeof(response))) == VC_HDMI_SUCCESS) {
         //First ask how many modes there are, if the current table is big enough to hold
         //all the modes, just copy over, otherwise allocate a new table
         if(response.num_supported_modes) {
            if(cache->max_modes < response.num_supported_modes) {
               cache->max_modes = response.num_supported_modes;
               if(cache->modes) {
                  vcos_free(cache->modes);
                  cache->modes = NULL;
               }
            } else {
               vcos_assert(cache->modes);
               memset(cache->modes, 0, cache->max_modes * sizeof(TV_SUPPORTED_MODE_NEW_T));
            }
            if(cache->modes == NULL) {
               cache->modes = vcos_calloc(cache->max_modes, sizeof(TV_SUPPORTED_MODE_NEW_T), "cached modes");
            }
            if(vcos_verify(cache->modes)) {
               //If we have successfully allocated the table, send
               //another request to actually download the modes from Videocore
               param[1] = response.num_supported_modes;
               if((error = tvservice_send_command_reply(VC_TV_QUERY_SUPPORTED_MODES_ACTUAL, param, sizeof(param),
                                                        &response, sizeof(response))) == VC_HDMI_SUCCESS) {
                  //The response comes back may indicate a different number of modes to param[1].
                  //This happens if a new EDID was read in between the two requests (should be rare),
                  //in which case we just download as many as VC says will be sent
                  cache->num_modes = response.num_supported_modes;
                  vcos_assert(response.num_supported_modes <= param[1]); //VC will not return more than what we have allocated
                  if(cache->num_modes) {
                     error = tvservice_wait_for_bulk_receive(cache->modes, cache->num_modes * sizeof(TV_SUPPORTED_MODE_NEW_T));
                     if(error)
                        vcos_log_error("Failed to download %s cache in [%s]", HDMI_RES_GROUP_NAME(group), VCOS_FUNCTION);
                  } else {
                     vcos_log_error("First query of supported modes indicated there are %d, but now there are none, has the TV been unplugged? [%s]",
                                    param[1], VCOS_FUNCTION);
                  }
               } else {
                  vcos_log_error("Failed to request %s cache in [%s]", HDMI_RES_GROUP_NAME(group), VCOS_FUNCTION);
               }
            } else {
               //If we failed to allocate memory, the request stops here
               vcos_log_error("Failed to allocate memory for %s cache in [%s]", HDMI_RES_GROUP_NAME(group), VCOS_FUNCTION);
            }
         } else {
            //The request also terminates if there are no supported modes reported
            vcos_log_trace("No supported modes returned for group %s in [%s]", HDMI_RES_GROUP_NAME(group), VCOS_FUNCTION);
         }
      } else {
         vcos_log_error("Failed to query supported modes for group %s in [%s]", HDMI_RES_GROUP_NAME(group), VCOS_FUNCTION);
      }
      vchi_service_release(tvservice_client.client_handle[0]);

      if(!error) {
         cache->is_valid = 1;
         vcos_log_trace("[%s] cached %d %s resolutions", VCOS_FUNCTION, response.num_supported_modes, HDMI_RES_GROUP_NAME(group));
         tvservice_client.hdmi_preferred_group = response.preferred_group;
         tvservice_client.hdmi_preferred_mode  = response.preferred_mode;
      }
   }

   if(cache->is_valid) {
      if(supported_modes && max_supported_modes) {
         modes_copied = _min(max_supported_modes, cache->num_modes);
         memcpy(supported_modes, cache->modes, modes_copied*sizeof(TV_SUPPORTED_MODE_NEW_T));
      } else {
         //If we pass in a null pointer, return the size of table instead
         modes_copied = cache->num_modes;
      }
   }

   if(preferred_group && preferred_mode) {
      *preferred_group = tvservice_client.hdmi_preferred_group;
      *preferred_mode = tvservice_client.hdmi_preferred_mode;
   }

   return modes_copied; //If there was an error, this will be zero
}

/***********************************************************
 * Name: vc_tv_hdmi_mode_supported
 *
 * Arguments:
 *       resolution standard (CEA/DMT), mode code
 *
 * Description: Query if a particular mode is supported
 *
 * Returns: single value return > 0 means supported, 0 means unsupported, < 0 means error
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_tv_hdmi_mode_supported(HDMI_RES_GROUP_T group,
                                               uint32_t mode) {
   TV_QUERY_MODE_SUPPORT_PARAM_T param = {group, mode};
   vcos_log_trace("[%s]", VCOS_FUNCTION);

   return tvservice_send_command( VC_TV_QUERY_MODE_SUPPORT, &param, sizeof(TV_QUERY_MODE_SUPPORT_PARAM_T), 1);
}

/***********************************************************
 * Name: vc_tv_hdmi_audio_supported
 *
 * Arguments:
 *       audio format (EDID_AudioFormat + EDID_AudioCodingExtension),
 *       no. of channels (1-8),
 *       sample rate (EDID_AudioSampleRate except "refer to header"),
 *       bit rate (or sample size if pcm)
 *       use EDID_AudioSampleSize as sample size argument
 *
 * Description: Query if a particular audio format is supported
 *
 * Returns: single value return which will be flags in EDID_AUDIO_SUPPORT_FLAG_T
 *          zero means everything is supported, < 0 means error
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_tv_hdmi_audio_supported(uint32_t audio_format, uint32_t num_channels,
                                                EDID_AudioSampleRate fs, uint32_t bitrate) {
   TV_QUERY_AUDIO_SUPPORT_PARAM_T param = { VC_HTOV32(audio_format),
                                            VC_HTOV32(num_channels),
                                            VC_HTOV32(fs),
                                            VC_HTOV32(bitrate) };
   vcos_log_trace("[%s]", VCOS_FUNCTION);
   if(!vcos_verify(num_channels > 0 && num_channels <= 8 && fs != EDID_AudioSampleRate_eReferToHeader))
      return -1;

   return tvservice_send_command( VC_TV_QUERY_AUDIO_SUPPORT, &param, sizeof(TV_QUERY_AUDIO_SUPPORT_PARAM_T), 1);
}

/***********************************************************
 * Name: vc_tv_enable_copyprotect
 *
 * Arguments:
 *       copy protect mode (only used for SDTV), time out in milliseconds
 *
 * Description: Enable copy protection (either HDMI or SDTV must be powered on)
 *
 * Returns: single value return 0 means success, additional result via callback
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_tv_enable_copyprotect(uint32_t cp_mode, uint32_t timeout) {
   TV_ENABLE_COPY_PROTECT_PARAM_T param = {VC_HTOV32(cp_mode), VC_HTOV32(timeout)};
   vcos_log_trace("[%s]", VCOS_FUNCTION);
   return tvservice_send_command( VC_TV_ENABLE_COPY_PROTECT, &param, sizeof(TV_ENABLE_COPY_PROTECT_PARAM_T), 1);
}

/***********************************************************
 * Name: vc_tv_disable_copyprotect
 *
 * Arguments:
 *       none
 *
 * Description: Disable copy protection (either HDMI or SDTV must be powered on)
 *
 * Returns: single value return 0 means success, additional result via callback
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_tv_disable_copyprotect( void ) {
   vcos_log_trace("[%s]", VCOS_FUNCTION);
   return tvservice_send_command( VC_TV_DISABLE_COPY_PROTECT, NULL, 0, 1);
}

/***********************************************************
 * Name: vc_tv_show_info
 *
 * Arguments:
 *       show (1) or hide (0) info screen
 *
 * Description: Show or hide info screen, only works in HDMI at the moment
 *
 * Returns: zero if command is successfully sent
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_tv_show_info(uint32_t show) {
   TV_SHOW_INFO_PARAM_T param = {VC_HTOV32(show)};
   vcos_log_trace("[%s]", VCOS_FUNCTION);
   return tvservice_send_command( VC_TV_SHOW_INFO, &param, sizeof(TV_SHOW_INFO_PARAM_T), 0);
}

/***********************************************************
 * Name: vc_tv_hdmi_get_av_latency
 *
 * Arguments:
 *       none
 *
 * Description: Get the AV latency (in ms) for HDMI (lipsync), only valid if
 *              HDMI is currently powered on, otherwise you get zero
 *
 * Returns: latency (zero if error or latency is not defined), < 0 if failed to send command)
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_tv_hdmi_get_av_latency( void ) {

   vcos_log_trace("[%s]", VCOS_FUNCTION);
   return tvservice_send_command( VC_TV_GET_AV_LATENCY, NULL, 0, 1);
}

/***********************************************************
 * Name: vc_tv_hdmi_set_hdcp_key
 *
 * Arguments:
 *       key block, whether we wait (1) or not (0) for the key to download
 *
 * Description: Download HDCP key
 *
 * Returns: single value return indicating download status
 *          (or queued status if we don't wait)
 *          Callback indicates the validity of key
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_tv_hdmi_set_hdcp_key(const uint8_t *key) {
   TV_HDCP_SET_KEY_PARAM_T param;

   vcos_log_trace("[%s]", VCOS_FUNCTION);
   if(!vcos_verify(key))
      return -1;
   memcpy(param.key, key, HDCP_KEY_BLOCK_SIZE);
   return tvservice_send_command( VC_TV_HDCP_SET_KEY, &param, sizeof(param), 0);
}

/***********************************************************
 * Name: vc_tv_hdmi_set_hdcp_revoked_list
 *
 * Arguments:
 *       list, size of list
 *
 * Description: Download HDCP revoked list
 *
 * Returns: single value return indicating download status
 *          (or queued status if we don't wait)
 *          Callback indicates the number of keys set (zero if failed, unless you are clearing the list)
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_tv_hdmi_set_hdcp_revoked_list(const uint8_t *list, uint32_t num_keys) {
   TV_HDCP_SET_SRM_PARAM_T param = {VC_HTOV32(num_keys)};
   int success = tvservice_send_command( VC_TV_HDCP_SET_SRM, &param, sizeof(TV_HDCP_SET_SRM_PARAM_T), 0);

   vcos_log_trace("[%s]", VCOS_FUNCTION);
   if(success == 0 && num_keys && list) { //Set num_keys to zero if we are clearing the list
      //Sent the command, now download the list
      if(tvservice_lock_obtain() == 0)
      {
         success = vchi_bulk_queue_transmit( tvservice_client.client_handle[0],
                                             list,
                                             num_keys * HDCP_KSV_LENGTH,
                                             VCHI_FLAGS_BLOCK_UNTIL_QUEUED,
                                             0 );
         tvservice_lock_release();
      }
      else
         success = -1;
   }
   return success;
}

/***********************************************************
 * Name: vc_tv_hdmi_set_spd
 *
 * Arguments:
 *       manufacturer, description, product type (HDMI_SPD_TYPE_CODE_T)
 *
 * Description: Set SPD
 *
 * Returns: whether command was sent successfully (zero means success)
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_tv_hdmi_set_spd(const char *manufacturer, const char *description, HDMI_SPD_TYPE_CODE_T type) {
   TV_SET_SPD_PARAM_T param;
   vcos_log_trace("[%s]", VCOS_FUNCTION);

   if(!vcos_verify(manufacturer && description))
      return -1;

   memcpy(param.manufacturer, manufacturer, TV_SPD_NAME_LEN);
   memcpy(param.description, description, TV_SPD_DESC_LEN);
   param.type = VC_HTOV32(type);
   return tvservice_send_command( VC_TV_SET_SPD, &param, sizeof(TV_SET_SPD_PARAM_T), 0);
}

/***********************************************************
 * Name: vc_tv_hdmi_set_display_options
 *
 * Arguments:
 *       aspect ratio (HDMI_ASPECT_T enum), left/right bar width, top/bottom bar height
 *
 * Description: Set active area for HDMI (bar width/height should be set to zero if absent)
 *
 * Returns: whether command was sent successfully (zero means success)
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_tv_hdmi_set_display_options(HDMI_ASPECT_T aspect,
                                                    uint32_t left_bar_width, uint32_t right_bar_width,
                                                    uint32_t top_bar_height, uint32_t bottom_bar_height,
                                                    uint32_t overscan_flags) {
   TV_SET_DISPLAY_OPTIONS_PARAM_T param;
   vcos_log_trace("[%s]", VCOS_FUNCTION);

   param.aspect = VC_HTOV32(aspect);
   param.vertical_bar_present = VC_HTOV32((left_bar_width || right_bar_width)? VC_TRUE : VC_FALSE);
   param.left_bar_width = VC_HTOV32(left_bar_width);
   param.right_bar_width = VC_HTOV32(right_bar_width);
   param.horizontal_bar_present = VC_HTOV32((top_bar_height || bottom_bar_height)? VC_TRUE : VC_FALSE);
   param.top_bar_height = VC_HTOV32(top_bar_height);
   param.bottom_bar_height = VC_HTOV32(bottom_bar_height);
   param.overscan_flags = VC_HTOV32(overscan_flags);
   return tvservice_send_command( VC_TV_SET_DISPLAY_OPTIONS, &param, sizeof(TV_SET_DISPLAY_OPTIONS_PARAM_T), 0);
}

/***********************************************************
 * Name: vc_tv_test_mode_start
 *
 * Arguments:
 *       24-bit colour, test mode (TV_TEST_MODE_T enum)
 *
 * Description: Power on HDMI to test mode, HDMI must be off to start with
 *
 * Returns: whether command was sent successfully (zero means success)
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_tv_test_mode_start(uint32_t colour, TV_TEST_MODE_T test_mode) {
   TV_TEST_MODE_START_PARAM_T param = {VC_HTOV32(colour), VC_HTOV32(test_mode)};

   vcos_log_trace("[%s]", VCOS_FUNCTION);
   return tvservice_send_command( VC_TV_TEST_MODE_START, &param, sizeof(TV_TEST_MODE_START_PARAM_T), 0);
}

/***********************************************************
 * Name: vc_tv_test_mode_stop
 *
 * Arguments:
 *       none
 *
 * Description: Stop test mode and power down HDMI
 *
 * Returns: whether command was sent successfully (zero means success)
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_tv_test_mode_stop( void ) {
   vcos_log_trace("[%s]", VCOS_FUNCTION);
   return tvservice_send_command( VC_TV_TEST_MODE_STOP, NULL, 0, 0);
}

/***********************************************************
 * Name: vc_tv_hdmi_ddc_read
 *
 * Arguments:
 *       offset, length to read, pointer to buffer, must be 16 byte aligned
 *
 * Description: ddc read over i2c (HDMI only at the moment)
 *
 * Returns: length of data read (so zero means error) and the buffer will be filled
 *          only if no error
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_tv_hdmi_ddc_read(uint32_t offset, uint32_t length, uint8_t *buffer) {
   int success;
   TV_DDC_READ_PARAM_T param = {VC_HTOV32(offset), VC_HTOV32(length)};

   vcos_log_trace("[%s]", VCOS_FUNCTION);

   /*if(!vcos_verify(buffer && (((uint32_t) buffer) % 16) == 0))
      return -1;*/

   vchi_service_use(tvservice_client.client_handle[0]);
   success = tvservice_send_command( VC_TV_DDC_READ, &param, sizeof(TV_DDC_READ_PARAM_T), 1);

   if(success == 0) {
      success = tvservice_wait_for_bulk_receive(buffer, length);
   }
   vchi_service_release(tvservice_client.client_handle[0]);
   return (success == 0)? length : 0; //Either return the whole block or nothing
}

/**
 * Sets whether the TV is attached or unplugged.
 * Required when hotplug interrupt is not handled by VideoCore.
 */
VCHPRE_ int VCHPOST_ vc_tv_hdmi_set_attached(uint32_t attached)
{
   vcos_log_trace("[%s] attached %d", VCOS_FUNCTION, attached);
   return tvservice_send_command(VC_TV_SET_ATTACHED, &attached, sizeof(uint32_t), 0);
}

/**
 * Sets a property in HDMI output
 */
VCHPRE_ int VCHPOST_ vc_tv_hdmi_set_property(const HDMI_PROPERTY_PARAM_T *property) {
   HDMI_PROPERTY_PARAM_T _property;
   if(vcos_verify(property)) {
      memcpy(&_property, property, sizeof(_property));
      vcos_log_trace("[%s] property:%d values:%d,%d", VCOS_FUNCTION, property->property, property->param1, property->param2);
      return tvservice_send_command(VC_TV_SET_PROP, &_property, sizeof(_property), 1);
   }
   return -1;
}

/**
 * Gets a property from HDMI
 */
VCHPRE_ int VCHPOST_ vc_tv_hdmi_get_property(HDMI_PROPERTY_PARAM_T *property) {
   int ret = -1;
   if(vcos_verify(property)) {
      TV_GET_PROP_PARAM_T param = {0, {HDMI_PROPERTY_MAX, 0, 0}};
      uint32_t prop = (uint32_t) property->property;
      property->param1 = property->param2 = 0;
      vcos_log_trace("[%s] property:%d", VCOS_FUNCTION, property->property);
      if((ret = tvservice_send_command_reply( VC_TV_GET_PROP, &prop, sizeof(prop),
                                              &param, sizeof(param))) == VC_HDMI_SUCCESS) {
         property->param1 = param.property.param1;
         property->param2 = param.property.param2;
      }
   }
   return ret;
}

/**
 * Converts the notification reason to a string.
 *
 * @param reason is the notification reason
 * @return  The notification reason as a string.
 */
VCHPRE_ const char* vc_tv_notification_name(VC_HDMI_NOTIFY_T reason)
{
   switch (reason)
   {
      case VC_HDMI_UNPLUGGED:
         return "VC_HDMI_UNPLUGGED";
      case VC_HDMI_ATTACHED:
         return "VC_HDMI_ATTACHED";
      case VC_HDMI_DVI:
         return "VC_HDMI_DVI";
      case VC_HDMI_HDMI:
         return "VC_HDMI_HDMI";
      case VC_HDMI_HDCP_UNAUTH:
         return "VC_HDMI_HDCP_UNAUTH";
      case VC_HDMI_HDCP_AUTH:
         return "VC_HDMI_HDCP_AUTH";
      case VC_HDMI_HDCP_KEY_DOWNLOAD:
         return "VC_HDMI_HDCP_KEY_DOWNLOAD";
      case VC_HDMI_HDCP_SRM_DOWNLOAD:
         return "VC_HDMI_HDCP_SRM_DOWNLOAD";
      case VC_HDMI_CHANGING_MODE:
         return "VC_HDMI_CHANGING_MODE";
      default:
         return "VC_HDMI_UNKNOWN";
   }
}

// temporary: maintain backwards compatibility
VCHPRE_ int VCHPOST_ vc_tv_hdmi_get_supported_modes(HDMI_RES_GROUP_T group,
                                                    TV_SUPPORTED_MODE_T *supported_modes_deprecated,
                                                    uint32_t max_supported_modes,
                                                    HDMI_RES_GROUP_T *preferred_group,
                                                    uint32_t *preferred_mode) {
   TV_SUPPORTED_MODE_NEW_T *supported_modes_new = malloc(max_supported_modes * sizeof *supported_modes_new);
   int modes_copied = vc_tv_hdmi_get_supported_modes_new(group==3 ? HDMI_RES_GROUP_CEA:group, supported_modes_new, max_supported_modes, preferred_group, preferred_mode);
   int i, j=0;

   for (i=0; i<modes_copied; i++) {
      TV_SUPPORTED_MODE_T *q = supported_modes_deprecated + j;
      TV_SUPPORTED_MODE_NEW_T *p = supported_modes_new + i;
      if (group != 3 || (p->struct_3d_mask & HDMI_3D_STRUCT_SIDE_BY_SIDE_HALF_HORIZONTAL)) {
         q->scan_mode = p->scan_mode;
         q->native = p->native;
         q->code = p->code;
         q->frame_rate = p->frame_rate;
         q->width = p->width;
         q->height = p->height;
         j++;
      }
   }
   free(supported_modes_new);

   return 0;
}

/**
 * Get the unique device ID from the EDID
 * @param pointer to device ID struct
 * @return zero if successful, non-zero if failed.
 */
VCHPRE_ int VCHPOST_  vc_tv_get_device_id(TV_DEVICE_ID_T *id) {
   int ret = -1;
   TV_DEVICE_ID_T param;
   memset(&param, 0, sizeof(TV_DEVICE_ID_T));
   if(vcos_verify(id)) {
      if((ret = tvservice_send_command_reply( VC_TV_GET_DEVICE_ID, NULL, 0,
                                              &param, sizeof(param))) == VC_HDMI_SUCCESS) {
         memcpy(id, &param, sizeof(TV_DEVICE_ID_T));
      } else {
         id->vendor[0] = '\0';
         id->monitor_name[0] = '\0';
         id->serial_num = 0;
      }
   }
   return ret;
}

// temporary: maintain backwards compatibility
VCHPRE_ int VCHPOST_ vc_tv_hdmi_power_on_explicit(HDMI_MODE_T mode, HDMI_RES_GROUP_T group, uint32_t code) {
   if (group == HDMI_RES_GROUP_CEA_3D) {
      HDMI_PROPERTY_PARAM_T property;
      property.property = HDMI_PROPERTY_3D_STRUCTURE;
      property.param1 = HDMI_RES_GROUP_CEA;
      property.param2 = 0;
      vc_tv_hdmi_set_property(&property);
      group = HDMI_RES_GROUP_CEA;
   }
   return vc_tv_hdmi_power_on_explicit_new(mode, group, code);
}

