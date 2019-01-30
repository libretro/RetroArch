/*
Copyright (c) 2012-2014, Broadcom Europe Ltd
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
#include <stdlib.h>

#include "vchost_platform_config.h"
#include "vchost.h"

#include "interface/vcos/vcos.h"
#include "vc_dispservice_x_defs.h"
#include "vc_dispmanx.h"
#include "interface/vchi/vchi.h"
#include "vcinclude/common.h"
#include "interface/vchi/common/endian.h"
#include "interface/vchi/message_drivers/message.h"
#include "vc_vchi_dispmanx.h"
/******************************************************************************
Local types and defines.
******************************************************************************/
//DispmanX service
typedef struct {
   VCHI_SERVICE_HANDLE_T client_handle[VCHI_MAX_NUM_CONNECTIONS]; //To connect to server on VC
   VCHI_SERVICE_HANDLE_T notify_handle[VCHI_MAX_NUM_CONNECTIONS]; //For incoming notification
   uint32_t              msg_flag[VCHI_MAX_NUM_CONNECTIONS];
   char                  command_buffer[DISPMANX_MSGFIFO_SIZE];
   char                  response_buffer[DISPMANX_MSGFIFO_SIZE];
   uint32_t              response_length;
   uint32_t              notify_buffer[DISPMANX_MSGFIFO_SIZE/sizeof(uint32_t)];
   uint32_t              notify_length;
   uint32_t              num_connections;
   VCOS_MUTEX_T          lock;
   char dispmanx_devices[DISPMANX_MAX_HOST_DEVICES][DISPMANX_MAX_DEVICE_NAME_LEN];
   uint32_t num_devices;
   uint32_t num_modes[DISPMANX_MAX_HOST_DEVICES];

   //Callback for update
   DISPMANX_CALLBACK_FUNC_T update_callback;
   void *update_callback_param;
   DISPMANX_UPDATE_HANDLE_T pending_update_handle;

   //Callback for vsync
   DISPMANX_CALLBACK_FUNC_T vsync_callback;
   void *vsync_callback_param;
   int vsync_enabled;

   int initialised;
} DISPMANX_SERVICE_T;

/******************************************************************************
Static data.
******************************************************************************/
static DISPMANX_SERVICE_T dispmanx_client;
static VCOS_EVENT_T dispmanx_message_available_event;
static VCOS_EVENT_T dispmanx_notify_available_event;
static VCOS_THREAD_T dispmanx_notify_task;

/******************************************************************************
Static functions.
******************************************************************************/
//Lock the host state
static __inline void lock_obtain (void) {
   VCOS_STATUS_T status;
   uint32_t i;
   vcos_assert(dispmanx_client.initialised);
   status = vcos_mutex_lock( &dispmanx_client.lock );
   if(dispmanx_client.initialised)
   {
      for (i=0; i<dispmanx_client.num_connections; i++) {
         vchi_service_use(dispmanx_client.client_handle[i]);
      }
   }
   vcos_assert(status == VCOS_SUCCESS);
}

//Unlock the host state
static __inline void lock_release (void) {
   uint32_t i;
   vcos_assert(dispmanx_client.initialised);
   if(dispmanx_client.initialised)
   {
      for (i=0; i<dispmanx_client.num_connections; i++) {
         vchi_service_release(dispmanx_client.client_handle[i]);
      }
   }
   vcos_mutex_unlock( &dispmanx_client.lock );
}

//Forward declarations
static void dispmanx_client_callback( void *callback_param,
                                      VCHI_CALLBACK_REASON_T reason,
                                      void *msg_handle );

static void dispmanx_notify_callback( void *callback_param,
                                      VCHI_CALLBACK_REASON_T reason,
                                      void *msg_handle );

static int32_t dispmanx_wait_for_reply(void *response, uint32_t max_length);

static int32_t dispmanx_send_command(  uint32_t command, void *buffer, uint32_t length);

static int32_t dispmanx_send_command_reply(  uint32_t command, void *buffer, uint32_t length,
                                        void *response, uint32_t max_length);

static uint32_t dispmanx_get_handle(  uint32_t command, void *buffer, uint32_t length);

static void *dispmanx_notify_func( void *arg );


/******************************************************************************
NAME
   vc_vchi_gencmd_init

SYNOPSIS
   void vc_vchi_gencmd_init(VCHI_INSTANCE_T initialise_instance, VCHI_CONNECTION_T **connections, uint32_t num_connections )

FUNCTION
   Initialise the general command service for use. A negative return value
   indicates failure (which may mean it has not been started on VideoCore).

RETURNS
   int
******************************************************************************/

void vc_vchi_dispmanx_init (VCHI_INSTANCE_T initialise_instance, VCHI_CONNECTION_T **connections, uint32_t num_connections ) {
   VCOS_STATUS_T status;
   int32_t success;
   uint32_t i;

   if (dispmanx_client.initialised)
     return;

   // record the number of connections
   memset( &dispmanx_client, 0, sizeof(DISPMANX_SERVICE_T) );
   dispmanx_client.num_connections = num_connections;

   status = vcos_mutex_create(&dispmanx_client.lock, "HDispmanx");
   vcos_assert(status == VCOS_SUCCESS);

   status = vcos_event_create(&dispmanx_message_available_event, "HDispmanx");
   vcos_assert(status == VCOS_SUCCESS);

   status = vcos_event_create(&dispmanx_notify_available_event, "HDispmanx");
   vcos_assert(status == VCOS_SUCCESS);

   dispmanx_client.initialised = 1;

   for (i=0; i<dispmanx_client.num_connections; i++) {

      VCOS_THREAD_ATTR_T attrs;

      // Create a 'Client' service on the each of the connections
      SERVICE_CREATION_T dispmanx_parameters = { VCHI_VERSION(VC_DISPMANX_VERSION),
                                                 DISPMANX_CLIENT_NAME,     // 4cc service code
                                                 connections[i],           // passed in fn ptrs
                                                 0,                        // tx fifo size (unused)
                                                 0,                        // tx fifo size (unused)
                                                 &dispmanx_client_callback, // service callback
                                                 &dispmanx_message_available_event,  // callback parameter
                                                 VC_FALSE,                  // want_unaligned_bulk_rx
                                                 VC_FALSE,                  // want_unaligned_bulk_tx
                                                 VC_FALSE,                  // want_crc
                                                 };

      SERVICE_CREATION_T dispmanx_parameters2 = { VCHI_VERSION(VC_DISPMANX_VERSION),
                                                  DISPMANX_NOTIFY_NAME,   // 4cc service code
                                                  connections[i],           // passed in fn ptrs
                                                  0,                        // tx fifo size (unused)
                                                  0,                        // tx fifo size (unused)
                                                  &dispmanx_notify_callback, // service callback
                                                  &dispmanx_notify_available_event,  // callback parameter
                                                  VC_FALSE,                  // want_unaligned_bulk_rx
                                                  VC_FALSE,                  // want_unaligned_bulk_tx
                                                  VC_FALSE,                  // want_crc
                                                   };

      success = vchi_service_open( initialise_instance, &dispmanx_parameters, &dispmanx_client.client_handle[i] );
      vcos_assert( success == 0 );

      // Create the async service of dispman to handle update callback

      success = vchi_service_open( initialise_instance, &dispmanx_parameters2, &dispmanx_client.notify_handle[i] );
      vcos_assert( success == 0 );

      //Create the notifier task
      vcos_thread_attr_init(&attrs);
      vcos_thread_attr_setstacksize(&attrs, 2048);
      vcos_thread_attr_settimeslice(&attrs, 1);

      status = vcos_thread_create(&dispmanx_notify_task, "HDispmanx Notify", &attrs, dispmanx_notify_func, NULL);
      vcos_assert(status == VCOS_SUCCESS);

      // release services until they're actually used
      vchi_service_release(dispmanx_client.client_handle[i]);
      vchi_service_release(dispmanx_client.notify_handle[i]);
   }
}

/***********************************************************
 * Name: vc_dispmanx_stop
 *
 * Arguments:
 *       -
 *
 * Description: Stops the Host side part of dispmanx
 *
 * Returns: -
 *
 ***********************************************************/
VCHPRE_ void VCHPOST_ vc_dispmanx_stop( void ) {
   // Wait for the current lock-holder to finish before zapping dispmanx.
   //TODO: kill the notifier task
   void *dummy;
   uint32_t i;

   if (!dispmanx_client.initialised)
      return;

   lock_obtain();
   for (i=0; i<dispmanx_client.num_connections; i++) {
      int32_t result;
      result = vchi_service_close(dispmanx_client.client_handle[i]);
      vcos_assert( result == 0 );
      result = vchi_service_close(dispmanx_client.notify_handle[i]);
      vcos_assert( result == 0 );
   }
   lock_release();
   dispmanx_client.initialised = 0;

   vcos_event_signal(&dispmanx_notify_available_event); 
   vcos_thread_join(&dispmanx_notify_task, &dummy);
   vcos_mutex_delete(&dispmanx_client.lock);
   vcos_event_delete(&dispmanx_message_available_event);
   vcos_event_delete(&dispmanx_notify_available_event);
}

/***********************************************************
 * Name: vc_dispmanx_rect_set
 *
 * Arguments:
 *       VC_RECT_T *rect
 *       uint32_t x_offset
 *       uint32_t y_offset
 *       uint32_t width
 *       uint32_t height
 *
 * Description: Fills in the fields of the supplied VC_RECT_T structure
 *
 * Returns: 0 or failure
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_dispmanx_rect_set( VC_RECT_T *rect, uint32_t x_offset, uint32_t y_offset, uint32_t width, uint32_t height ) {
   rect->x = (int32_t) x_offset;
   rect->y = (int32_t) y_offset;
   rect->width = (int32_t) width;
   rect->height = (int32_t) height;
   return 0;
}

/******************************************************************************
NAME
   vc_dispmanx_query_image_formats

PARAMS
   uint32_t *support_formats - the returned supported image formats

FUNCTION
   Returns the support image formats from the VMCS host

RETURNS
   Success: 0
   Otherwise non-zero
******************************************************************************/
VCHPRE_ int  VCHPOST_ vc_dispmanx_query_image_formats( uint32_t *supported_formats ) {
   *supported_formats = dispmanx_get_handle(EDispmanQueryImageFormats, NULL, 0);
   return (*supported_formats)? 0 : -1;
}

/***********************************************************
 * Name: vc_dispmanx_resource_create
 *
 * Arguments:
 *       VC_IMAGE_TYPE_T type
 *       uint32_t width
 *       uint32_t height
 *
 * Description: Create a new resource (in Videocore memory)
 *
 * Returns: resource handle
 *
 ***********************************************************/
VCHPRE_ DISPMANX_RESOURCE_HANDLE_T VCHPOST_ vc_dispmanx_resource_create( VC_IMAGE_TYPE_T type, uint32_t width, uint32_t height, uint32_t *native_image_handle ) {
   uint32_t resourceCreateParams[] = { (uint32_t)VC_HTOV32(type), VC_HTOV32(width), VC_HTOV32(height) };

   uint32_t resource = 0;

   resource = dispmanx_get_handle(EDispmanResourceCreate, resourceCreateParams, sizeof(resourceCreateParams));

   //We don't get an image handle back, so explicitly set this to zero to let the caller know
   *native_image_handle = 0;
   //The caller should call vc_dispmanx_resource_get_image_handle below to get the VC_IMAGE_T *
   //This will be deprecated soon, however

   return (DISPMANX_RESOURCE_HANDLE_T) resource;
}

/***********************************************************
 * Name: vc_dispmanx_resource_get_image_handle
 *
 * Arguments: resource handle
 *
 * Description: xxx only for hacking purpose, will be obsolete soon
 *
 * Returns: vc_image pointer
 *
 ***********************************************************/
VCHPRE_ uint32_t VCHPOST_ vc_dispmanx_resource_get_image_handle( DISPMANX_RESOURCE_HANDLE_T res) {
   return dispmanx_get_handle(EDispmanResourceGetImage, &res, sizeof(res));
}

/***********************************************************
 * Name: vc_dispmanx_resource_delete
 *
 * Arguments:
 *       DISPMANX_RESOURCE_HANDLE_T res
 *
 * Description:
 *
 * Returns: 0 or failure
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_dispmanx_resource_delete( DISPMANX_RESOURCE_HANDLE_T res ) {
   int status;
   res = VC_HTOV32(res);
   //We block to make sure the memory is freed after the call
   status = (int) dispmanx_send_command(EDispmanResourceDelete, &res, sizeof(res));

   return status;
}

/***********************************************************
 * Name: vc_dispmanx_resource_write_data
 *
 * Arguments:
 *       DISPMANX_RESOURCE_HANDLE_T res
 *       int src_pitch
 *       void * src_address
 *       const VC_RECT_T * rect
 *
 * Description: Copy the bitmap data to VideoCore memory
 *
 * Returns: 0 or failure
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_dispmanx_resource_write_data( DISPMANX_RESOURCE_HANDLE_T handle, VC_IMAGE_TYPE_T src_type /* not used */,
                                                      int src_pitch, void * src_address, const VC_RECT_T * rect ) {
	(void)src_type;

   //Note that x coordinate of the rect is NOT used
   //Address of data in host
   uint8_t *host_start = (uint8_t *)src_address + src_pitch * rect->y;
   int32_t bulk_len = src_pitch * rect->height, success = 0;

   //Now send the bulk transfer across
   //command parameters: resource handle, destination y, bulk length
   uint32_t param[] = {VC_HTOV32(handle), VC_HTOV32(rect->y), VC_HTOV32(bulk_len), VC_HTOV32(src_type) };
   success = dispmanx_send_command(  EDispmanBulkWrite | DISPMANX_NO_REPLY_MASK, param, sizeof(param));
   if(success == 0)
   {
      lock_obtain();
      success = vchi_bulk_queue_transmit( dispmanx_client.client_handle[0],
                                          host_start,
                                          bulk_len,
                                          VCHI_FLAGS_BLOCK_UNTIL_DATA_READ,
                                          NULL );
      lock_release();
   }
   return (int) success;
}
/***********************************************************
 * Name: vc_dispmanx_resource_read_data
 *
 * Arguments:
 *       DISPMANX_RESOURCE_HANDLE_T res
 *       int src_pitch
 *       void * src_address
 *       const VC_RECT_T * rect
 *
 * Description: Copy the bitmap data from VideoCore memory
 *
 * Returns: 0 or failure
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_
vc_dispmanx_resource_read_data(
   DISPMANX_RESOURCE_HANDLE_T handle,
   const VC_RECT_T* p_rect,
   void *   dst_address,
   uint32_t dst_pitch )
{
   uint8_t* host_start;
   int32_t  bulk_len;
   int32_t  success = 0;

   if ( p_rect == 0 || dst_address == 0 || dst_pitch == 0 )
   {
      return -1;
   }

   host_start = (uint8_t *)dst_address + (dst_pitch * p_rect->y);
   bulk_len   = (int32_t)dst_pitch * p_rect->height;

   // Now send the bulk transfer across
   // command parameters: resource handle, destination y, bulk length
   uint32_t param[] = { VC_HTOV32(handle), VC_HTOV32(p_rect->y), VC_HTOV32(bulk_len) };
   success = dispmanx_send_command( EDispmanBulkRead | DISPMANX_NO_REPLY_MASK, param, sizeof(param));
   if (success == 0)
   {
      lock_obtain();
      success = vchi_bulk_queue_receive(  dispmanx_client.client_handle[0],
                                          host_start,
                                          bulk_len,
                                          VCHI_FLAGS_BLOCK_UNTIL_OP_COMPLETE,
                                          0 );
      lock_release();
   }
   return (int) success;
}

/***********************************************************
 * Name: vc_dispmanx_resource_write_data_handle
 *
 * Arguments:
 *       DISPMANX_RESOURCE_HANDLE_T res
 *       int src_pitch
 *       MEM_HANDLE_T handle
 *       uint32_t offset
 *       const VC_RECT_T * rect
 *
 * Description: Copy the bitmap data to VideoCore memory
 *
 * Returns: 0 or failure
 *
 ***********************************************************/
#ifdef SELF_HOSTED
VCHPRE_ int VCHPOST_ vc_dispmanx_resource_write_data_handle( DISPMANX_RESOURCE_HANDLE_T handle, VC_IMAGE_TYPE_T src_type /* not used */,
                                                             int src_pitch, VCHI_MEM_HANDLE_T mem_handle, uint32_t offset,
                                                             const VC_RECT_T * rect ) {
   int32_t bulk_len;
   uint32_t param[3];
   uint32_t success = 0;

   //Note that x coordinate of the rect is NOT used
   //Address of data in host
   offset += src_pitch * rect->y;
   bulk_len = src_pitch * rect->height;

   //Now send the bulk transfer across
   //command parameters: resource handle, destination y, bulk length
   param[0] = VC_HTOV32(handle);
   param[1] = VC_HTOV32(rect->y);
   param[2] = VC_HTOV32(bulk_len);
   success = dispmanx_send_command(  EDispmanBulkWrite | DISPMANX_NO_REPLY_MASK, param, sizeof(param));
   if(success == 0)
   {
      lock_obtain();
      success = vchi_bulk_queue_transmit_reloc( dispmanx_client.client_handle[0],
                                                mem_handle, offset,
                                                bulk_len,
                                                VCHI_FLAGS_BLOCK_UNTIL_DATA_READ,
                                                NULL );
      lock_release();
   }
   return (int) success;
}
#endif

/***********************************************************
 * Name: vc_dispmanx_display_open
 *
 * Arguments:
 *       uint32_t device
 *
 * Description:
 *
 * Returns:
 *
 ***********************************************************/
VCHPRE_ DISPMANX_DISPLAY_HANDLE_T VCHPOST_ vc_dispmanx_display_open( uint32_t device ) {
   uint32_t display_handle;
   char *env = getenv("VC_DISPLAY");

   if (device == 0 && env)
   {
      device = atoi(env);
   }

   device = VC_HTOV32(device);
   display_handle = dispmanx_get_handle(EDispmanDisplayOpen,
                                                 &device, sizeof(device));

   return (DISPMANX_DISPLAY_HANDLE_T) display_handle;
}

/***********************************************************
 * Name: vc_dispmanx_display_open_mode
 *
 * Arguments:
 *       uint32_t device
 *       uint32_t mode
 *
 * Description:
 *
 * Returns:
 *
 ***********************************************************/
VCHPRE_ DISPMANX_DISPLAY_HANDLE_T VCHPOST_ vc_dispmanx_display_open_mode( uint32_t device, uint32_t mode ) {
   uint32_t display_open_param[] = {VC_HTOV32(device), VC_HTOV32(mode)};
   uint32_t display_handle = dispmanx_get_handle(EDispmanDisplayOpenMode,
                                                 &display_open_param, sizeof(display_open_param));

   return (DISPMANX_DISPLAY_HANDLE_T) display_handle;
}

/***********************************************************
 * Name: vc_dispmanx_display_open_offscreen
 *
 * Arguments:
 *       DISPMANX_RESOURCE_HANDLE_T dest
 *       DISPMANX_TRANSFORM_T orientation
 *
 * Description:
 *
 * Returns:
 *
 ***********************************************************/
VCHPRE_ DISPMANX_DISPLAY_HANDLE_T VCHPOST_ vc_dispmanx_display_open_offscreen( DISPMANX_RESOURCE_HANDLE_T dest, DISPMANX_TRANSFORM_T orientation ) {
   uint32_t display_open_param[] = {(uint32_t)VC_HTOV32(dest), (uint32_t)VC_HTOV32(orientation)};
   uint32_t display_handle = dispmanx_get_handle(EDispmanDisplayOpenOffscreen,
                                                 &display_open_param, sizeof(display_open_param));

   return (DISPMANX_DISPLAY_HANDLE_T) display_handle;
}

/***********************************************************
 * Name: vc_dispmanx_display_reconfigure
 *
 * Arguments:
 *       DISPMANX_DISPLAY_HANDLE_T display
 *       uint32_t mode
 *
 * Description:
 *
 * Returns:
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_dispmanx_display_reconfigure( DISPMANX_DISPLAY_HANDLE_T device, uint32_t mode ) {
   uint32_t display_param[] = {(uint32_t)VC_HTOV32(device), VC_HTOV32(mode)};
   int32_t success = dispmanx_send_command(  EDispmanDisplayReconfigure | DISPMANX_NO_REPLY_MASK,
                                             display_param, sizeof(display_param));
   return (int) success;
}

/***********************************************************
 * Name: vc_dispmanx_display_set_destination
 *
 * Arguments:
 *       DISPMANX_DISPLAY_HANDLE_T display
 *       DISPMANX_RESOURCE_HANDLE_T dest
 *
 * Description:
 *
 * Returns:
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_dispmanx_display_set_destination( DISPMANX_DISPLAY_HANDLE_T display, DISPMANX_RESOURCE_HANDLE_T dest ) {
   uint32_t display_param[] = {(uint32_t)VC_HTOV32(display), (uint32_t)VC_HTOV32(dest)};
   int32_t success = dispmanx_send_command(  EDispmanDisplaySetDestination | DISPMANX_NO_REPLY_MASK,
                                             display_param, sizeof(display_param));
   return (int) success;
}

/***********************************************************
 * Name: vc_dispmanx_display_set_background
 *
 * Arguments:
 *       DISPMANX_UPDATE_HANDLE_T update
 *       DISPMANX_DISPLAY_HANDLE_T display
 *       uint8_t red
 *       uint8_t green
 *       uint8_t blue
 *
 * Description:
 *
 * Returns:
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_dispmanx_display_set_background( DISPMANX_UPDATE_HANDLE_T update, DISPMANX_DISPLAY_HANDLE_T display,
                                                                       uint8_t red, uint8_t green, uint8_t blue ) {
   uint32_t display_param[] = {(uint32_t)VC_HTOV32(update), (uint32_t) VC_HTOV32(display), VC_HTOV32(red), VC_HTOV32(green), VC_HTOV32(blue)};
   int success = (int) dispmanx_send_command( EDispmanDisplaySetBackground | DISPMANX_NO_REPLY_MASK,
                                        display_param, sizeof(display_param));
   return success;
}

/***********************************************************
 * Name: vc_dispmanx_display_get_info
 *
 * Arguments:
 *       DISPMANX_DISPLAY_HANDLE_T display
 *       DISPMANX_MODEINFO_T * pinfo
 *
 * Description:
 *
 * Returns: VCHI error
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_
vc_dispmanx_display_get_info (DISPMANX_DISPLAY_HANDLE_T display,
                              DISPMANX_MODEINFO_T *pinfo)
{
   GET_INFO_DATA_T info;
   int32_t success;
   display = VC_HTOV32(display);
   success = dispmanx_send_command_reply (EDispmanDisplayGetInfo,
                                          &display, sizeof(display),
                                          &info, sizeof(info));
   if(success == 0) {
      pinfo->width = VC_VTOH32(info.width);
      pinfo->height = VC_VTOH32(info.height);
      pinfo->transform = (DISPMANX_TRANSFORM_T)VC_VTOH32(info.transform);
      pinfo->input_format = (DISPLAY_INPUT_FORMAT_T)VC_VTOH32(info.input_format);
   }

   return (int) success;
}

/***********************************************************
 * Name: vc_dispmanx_display_close
 *
 * Arguments:
 *       DISPMANX_DISPLAY_HANDLE_T display
 *
 * Description:
 *
 * Returns:
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_dispmanx_display_close( DISPMANX_DISPLAY_HANDLE_T display ) {
   int success;
   display = VC_HTOV32(display);
   success = (int) dispmanx_send_command( EDispmanDisplayClose | DISPMANX_NO_REPLY_MASK,
                                        &display, sizeof(display));
   return success;
}
/***********************************************************
 * Name: vc_dispmanx_update_start
 *
 * Arguments:
 *       int32_t priority
 *
 * Description:
 *
 * Returns:
 *
 ***********************************************************/
VCHPRE_ DISPMANX_UPDATE_HANDLE_T VCHPOST_ vc_dispmanx_update_start( int32_t priority ) {
   uint32_t handle;
   priority = VC_HTOV32(priority);
   handle = dispmanx_get_handle(EDispmanUpdateStart,
                                         &priority, sizeof(priority));

   return (DISPMANX_UPDATE_HANDLE_T) handle;
}


/***********************************************************
 * Name: vc_dispmanx_update_submit
 *
 * Arguments:
 *       DISPMANX_UPDATE_HANDLE_T update
 *       DISPMANX_CALLBACK_FUNC_T cb_func
 *       void *cb_arg
 *
 * Description:
 *
 * Returns:
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_dispmanx_update_submit( DISPMANX_UPDATE_HANDLE_T update, DISPMANX_CALLBACK_FUNC_T cb_func, void *cb_arg ) {
   uint32_t update_param[] = {(uint32_t) VC_HTOV32(update), (uint32_t) ((cb_func)? VC_HTOV32(1) : 0)};
   int success;

   vcos_assert(update); // handles must be non-zero
   if (update)
   {
      //Set the callback
      dispmanx_client.update_callback = cb_func;
      dispmanx_client.update_callback_param = cb_arg;
      dispmanx_client.pending_update_handle = update;
      vchi_service_use(dispmanx_client.notify_handle[0]); // corresponding release is in dispmanx_notify_func
      success = (int) dispmanx_send_command( EDispmanUpdateSubmit | DISPMANX_NO_REPLY_MASK,
                                             update_param, sizeof(update_param));
   }
   else
   {
      success = -1;
   }
   return success;
}

/***********************************************************
 * Name: vc_dispmanx_update_submit_sync
 *
 * Arguments:
 *       DISPMANX_UPDATE_HANDLE_T update
 *
 * Description:
 *
 * Returns: VCHI error code
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_dispmanx_update_submit_sync( DISPMANX_UPDATE_HANDLE_T update ) {
   int success;
   update = VC_HTOV32(update);
   success = (int) dispmanx_send_command( EDispmanUpdateSubmitSync,
                                          &update, sizeof(update));
   return success;
}


/***********************************************************
 * Name: vc_dispmanx_element_add
 *
 * Arguments:
 *       DISPMANX_UPDATE_HANDLE_T update
 *       DISPMANX_DISPLAY_HANDLE_T display
 *       int32_t layer
 *       const VC_RECT_T *dest_rect
 *       DISPMANX_RESOURCE_HANDLE_T src
 *       const VC_RECT_T *src_rect
 *       DISPMANX_FLAGS_T flags
 *       uint8_t opacity
 *       DISPMANX_RESOURCE_HANDLE_T mask
 *       DISPMANX_TRANSFORM_T transform
 *
 * Description:
 *
 * Returns: VCHI error code
 *
 ***********************************************************/
VCHPRE_ DISPMANX_ELEMENT_HANDLE_T VCHPOST_ vc_dispmanx_element_add ( DISPMANX_UPDATE_HANDLE_T update,
                                                                     DISPMANX_DISPLAY_HANDLE_T display,
                                                                     int32_t layer,
                                                                     const VC_RECT_T *dest_rect,
                                                                     DISPMANX_RESOURCE_HANDLE_T src,
                                                                     const VC_RECT_T *src_rect,
                                                                     DISPMANX_PROTECTION_T protection,
                                                                     VC_DISPMANX_ALPHA_T *alpha,
                                                                     DISPMANX_CLAMP_T *clamp,
                                                                     DISPMANX_TRANSFORM_T transform ) {

   int32_t element_param[] = {
      (int32_t) VC_HTOV32(update),
      (int32_t) VC_HTOV32(display),
      (int32_t) VC_HTOV32(layer),
      (int32_t) VC_HTOV32(dest_rect->x),
      (int32_t) VC_HTOV32(dest_rect->y),
      (int32_t) VC_HTOV32(dest_rect->width),
      (int32_t) VC_HTOV32(dest_rect->height),
      (int32_t) VC_HTOV32(src),
      (int32_t) VC_HTOV32(src_rect->x),
      (int32_t) VC_HTOV32(src_rect->y),
      (int32_t) VC_HTOV32(src_rect->width),
      (int32_t) VC_HTOV32(src_rect->height),
      (int32_t) VC_HTOV32(protection),
      alpha ? (int32_t) VC_HTOV32(alpha->flags) : 0,
      alpha ? (int32_t) VC_HTOV32(alpha->opacity) : 0,
      alpha ? (int32_t) VC_HTOV32(alpha->mask) : 0,
      clamp ? (int32_t) VC_HTOV32(clamp->mode) : 0,
      clamp ? (int32_t) VC_HTOV32(clamp->key_mask) : 0,
      clamp ? (int32_t) VC_HTOV32(clamp->key_value.yuv.yy_upper) : 0,
      clamp ? (int32_t) VC_HTOV32(clamp->key_value.yuv.yy_lower) : 0,
      clamp ? (int32_t) VC_HTOV32(clamp->key_value.yuv.cr_upper) : 0,
      clamp ? (int32_t) VC_HTOV32(clamp->key_value.yuv.cr_lower) : 0,
      clamp ? (int32_t) VC_HTOV32(clamp->key_value.yuv.cb_upper) : 0,
      clamp ? (int32_t) VC_HTOV32(clamp->key_value.yuv.cb_lower) : 0,
      clamp ? (int32_t) VC_HTOV32(clamp->replace_value) : 0,
      (int32_t) VC_HTOV32(transform)
   };

   uint32_t handle =  dispmanx_get_handle(EDispmanElementAdd,
                                          element_param, sizeof(element_param));
   return (DISPMANX_ELEMENT_HANDLE_T) handle;
}

/***********************************************************
 * Name: vc_dispmanx_element_change_source
 *
 * Arguments:
 *       DISPMANX_UPDATE_HANDLE_T update
 *       DISPMANX_ELEMENT_HANDLE_T element
 *       DISPMANX_RESOURCE_HANDLE_T src
 *
 * Description:
 *
 * Returns: VCHI error code
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_dispmanx_element_change_source( DISPMANX_UPDATE_HANDLE_T update, DISPMANX_ELEMENT_HANDLE_T element,
                                                        DISPMANX_RESOURCE_HANDLE_T src ) {
   uint32_t element_param[] = { (uint32_t) VC_HTOV32(update),
                                (uint32_t) VC_HTOV32(element),
                                (uint32_t) VC_HTOV32(src) };

   int success = (int) dispmanx_send_command( EDispmanElementChangeSource | DISPMANX_NO_REPLY_MASK,
                                              element_param, sizeof(element_param));
   return success;

}

/***********************************************************
 * Name: vc_dispmanx_element_change_layer
 *
 * Arguments:
 *       DISPMANX_UPDATE_HANDLE_T update
 *       DISPMANX_ELEMENT_HANDLE_T element
 *       int32_t layer
 *
 * Description:
 *
 * Returns: VCHI error code
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_dispmanx_element_change_layer (DISPMANX_UPDATE_HANDLE_T update,
                                                       DISPMANX_ELEMENT_HANDLE_T element,
                                                       int32_t layer)
{
   uint32_t element_param[] = { (uint32_t) VC_HTOV32(update),
                                (uint32_t) VC_HTOV32(element),
                                (uint32_t) VC_HTOV32(layer) };

   int success = (int) dispmanx_send_command( EDispmanElementChangeLayer | DISPMANX_NO_REPLY_MASK,
                                              element_param, sizeof(element_param));
   return success;

}

/***********************************************************
 * Name: vc_dispmanx_element_modified
 *
 * Arguments:
 *       DISPMANX_UPDATE_HANDLE_T update
 *       DISPMANX_ELEMENT_HANDLE_T element
 *       const VC_RECT_T * rect
 *
 * Description:
 *
 * Returns: VCHI error code
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_dispmanx_element_modified( DISPMANX_UPDATE_HANDLE_T update, DISPMANX_ELEMENT_HANDLE_T element, const VC_RECT_T * rect ) {

   uint32_t element_param[6] = { (uint32_t) VC_HTOV32(update),
                                 (uint32_t) VC_HTOV32(element), 0, 0, 0, 0};
   uint32_t param_length = 2*sizeof(uint32_t);
   int success;

   if(rect) {
      element_param[2] = VC_HTOV32(rect->x);
      element_param[3] = VC_HTOV32(rect->y);
      element_param[4] = VC_HTOV32(rect->width);
      element_param[5] = VC_HTOV32(rect->height);
      param_length = 6*sizeof(uint32_t);
   }
   success = (int) dispmanx_send_command( EDispmanElementModified | DISPMANX_NO_REPLY_MASK,
                                            element_param, param_length);
   return success;
}

/***********************************************************
 * Name: vc_dispmanx_element_remove
 *
 * Arguments:
 *       DISPMANX_UPDATE_HANDLE_T update
 *       DISPMANX_ELEMENT_HANDLE_T element
 *
 * Description:
 *
 * Returns:
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_dispmanx_element_remove( DISPMANX_UPDATE_HANDLE_T update, DISPMANX_ELEMENT_HANDLE_T element ) {
   uint32_t element_param[] = {(uint32_t) VC_HTOV32(update), (uint32_t) VC_HTOV32(element)};
   int success = (int) dispmanx_send_command( EDispmanElementRemove | DISPMANX_NO_REPLY_MASK,
                                              element_param, sizeof(element_param));
   return success;
}

/***********************************************************
 * Name: vc_dispmanx_element_change_attributes
 *
 * Arguments:
 *       DISPMANX_UPDATE_HANDLE_T update
 *       DISPMANX_ELEMENT_HANDLE_T element
 *       uint32_t change flags (bit 0 layer, bit 1 opacity, bit 2 dest rect, bit 3 src rect, bit 4 mask, bit 5 transform
 *       uint32_t layer
 *       uint8_t opacity
 *       const VC_RECT_T *dest rect
 *       const VC_RECT_T *src rect
 *       DISPMANX_RESOURCE_HANDLE_T mask
 *       VC_DISPMAN_TRANSFORM_T transform
 *
 * Description:
 *
 * Returns:
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_dispmanx_element_change_attributes( DISPMANX_UPDATE_HANDLE_T update,
                                                            DISPMANX_ELEMENT_HANDLE_T element,
                                                            uint32_t change_flags,
                                                            int32_t layer,
                                                            uint8_t opacity,
                                                            const VC_RECT_T *dest_rect,
                                                            const VC_RECT_T *src_rect,
                                                            DISPMANX_RESOURCE_HANDLE_T mask,
                                                            DISPMANX_TRANSFORM_T transform ) {

   uint32_t element_param[15] = { (uint32_t) VC_HTOV32(update),
                                  (uint32_t) VC_HTOV32(element),
                                  VC_HTOV32(change_flags),
                                  VC_HTOV32(layer),
                                  VC_HTOV32(opacity),
                                  (uint32_t) VC_HTOV32(mask),
                                  (uint32_t) VC_HTOV32(transform), 0, 0, 0, 0, 0, 0, 0, 0};

   uint32_t param_length = 7*sizeof(uint32_t);
   int success;
   if(dest_rect) {
      element_param[7]  = VC_HTOV32(dest_rect->x);
      element_param[8]  = VC_HTOV32(dest_rect->y);
      element_param[9]  = VC_HTOV32(dest_rect->width);
      element_param[10] = VC_HTOV32(dest_rect->height);
      element_param[2] |= ELEMENT_CHANGE_DEST_RECT;
      param_length += 4*sizeof(uint32_t);
   }
   if(src_rect) {
      element_param[11] = VC_HTOV32(src_rect->x);
      element_param[12] = VC_HTOV32(src_rect->y);
      element_param[13] = VC_HTOV32(src_rect->width);
      element_param[14] = VC_HTOV32(src_rect->height);
      element_param[2] |= ELEMENT_CHANGE_SRC_RECT;
      param_length += 4*sizeof(uint32_t);
   }


   success = (int) dispmanx_send_command( EDispmanElementChangeAttributes | DISPMANX_NO_REPLY_MASK,
                                            element_param, param_length);
   return success;
}


/***********************************************************
 * Name: vc_dispmanx_snapshot
 *
 * Arguments:
 *       DISPMANX_DISPLAY_HANDLE_T display
 *       DISPMANX_RESOURCE_HANDLE_T snapshot_resource
 *       DISPMANX_TRANSFORM_T transform
 *
 * Description: Take a snapshot of a display in its current state
 *
 * Returns:
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_dispmanx_snapshot( DISPMANX_DISPLAY_HANDLE_T display,
                                           DISPMANX_RESOURCE_HANDLE_T snapshot_resource,
                                           DISPMANX_TRANSFORM_T transform )
{
   uint32_t display_snapshot_param[] = {
      VC_HTOV32(display),
      VC_HTOV32(snapshot_resource),
      VC_HTOV32(transform)};

   int success = (int) dispmanx_send_command( EDispmanSnapshot,
                                              display_snapshot_param,
                                              sizeof(display_snapshot_param));
   return success;
}

/***********************************************************
 * Name: vc_dispmanx_resource_set_palette
 *
 * Arguments:
 *       DISPMANX_RESOURCE_HANDLE_T res
 *       void * src_address
 *       int offset
 *       int size
 *
 * Description: Set the resource palette (for VC_IMAGE_4BPP and VC_IMAGE_8BPP)
 *              offset should be 0
 *              size is 16*2 for 4BPP and 256*2 for 8BPP
 * Returns: 0 or failure
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_dispmanx_resource_set_palette( DISPMANX_RESOURCE_HANDLE_T handle, 
                                                      void * src_address, int offset, int size) {
   //Note that x coordinate of the rect is NOT used
   //Address of data in host
   uint8_t *host_start = src_address;
   int32_t bulk_len = size, success = 0;

   //Now send the bulk transfer across
   //command parameters: resource size
   uint32_t param[] = {VC_HTOV32(handle), VC_HTOV32(offset), VC_HTOV32(bulk_len) };
   success = dispmanx_send_command(  EDispmanSetPalette | DISPMANX_NO_REPLY_MASK, param, sizeof(param));
   if(success == 0)
   {
      lock_obtain();
      success = vchi_bulk_queue_transmit( dispmanx_client.client_handle[0],
                                          host_start,
                                          bulk_len,
                                          VCHI_FLAGS_BLOCK_UNTIL_DATA_READ,
                                          NULL );
      lock_release();
   }
   return (int) success;
}


/***********************************************************
 * Name: vc_dispmanx_vsync_callback
 *
 * Arguments:
 *       DISPMANX_DISPLAY_HANDLE_T display
 *       DISPMANX_CALLBACK_FUNC_T cb_func
 *       void *cb_arg
 *
 * Description: start sending callbacks on vsync events
 *              Use a NULL cb_func to stop the callbacks
 * Returns: 0 or failure
 *
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_dispmanx_vsync_callback( DISPMANX_DISPLAY_HANDLE_T display, DISPMANX_CALLBACK_FUNC_T cb_func, void *cb_arg )
{
   // Steal the invalid 0 handle to indicate this is a vsync request
   DISPMANX_UPDATE_HANDLE_T update = 0;
   int enable = (cb_func != NULL);
   uint32_t update_param[] = {(uint32_t) VC_HTOV32(display), VC_HTOV32(update), (int32_t)enable};
   int success;

   // Set the callback
   dispmanx_client.vsync_callback = cb_func;
   dispmanx_client.vsync_callback_param = cb_arg;

   if (!dispmanx_client.vsync_enabled && enable) {
      // An extra "use" is required while a vsync callback is registered.
      // The corresponding "release" is below.
      vchi_service_use(dispmanx_client.notify_handle[0]);
   }

   success = (int) dispmanx_send_command( EDispmanVsyncCallback | DISPMANX_NO_REPLY_MASK,
                                          update_param, sizeof(update_param));

   if (dispmanx_client.vsync_enabled && !enable) {
      // The extra "use" added above is no longer required.
      vchi_service_release(dispmanx_client.notify_handle[0]);
   }

   dispmanx_client.vsync_enabled = enable;

   return (int) success;
}


/*********************************************************************************
 *
 *  Static functions definitions
 *
 *********************************************************************************/
//TODO: Might need to handle multiple connections later
/***********************************************************
 * Name: dispmanx_client_callback
 *
 * Arguments: semaphore, callback reason and message handle
 *
 * Description: VCHI callback for the DISP service
 *
 ***********************************************************/
static void dispmanx_client_callback( void *callback_param,
                                      const VCHI_CALLBACK_REASON_T reason,
                                      void *msg_handle ) {

	(void)msg_handle;

   VCOS_EVENT_T *event = (VCOS_EVENT_T *)callback_param;

   if ( reason != VCHI_CALLBACK_MSG_AVAILABLE )
      return;

   if ( event == NULL )
      return;
   
   vcos_event_signal(event);
}

/***********************************************************
 * Name: dispmanx_notify_callback
 *
 * Arguments: semaphore, callback reason and message handle
 *
 * Description: VCHI callback for the update callback
 *
 ***********************************************************/

static void dispmanx_notify_callback( void *callback_param,
                                      const VCHI_CALLBACK_REASON_T reason,
                                      void *msg_handle ) {
   VCOS_EVENT_T *event = (VCOS_EVENT_T *)callback_param;

   (void)msg_handle;

   if ( reason != VCHI_CALLBACK_MSG_AVAILABLE )
      return;

   if ( event == NULL )
      return;

   vcos_event_signal(event);
}

/***********************************************************
 * Name: dispmanx_wait_for_reply
 *
 * Arguments: response buffer, buffer length
 *
 * Description: blocked until something is in the buffer
 *
 * Returns error code of vchi
 *
 ***********************************************************/
static int32_t dispmanx_wait_for_reply(void *response, uint32_t max_length) {
   int32_t success = 0;
   uint32_t length_read = 0;
   do {
      //TODO : we need to deal with messages coming through on more than one connections properly
      //At the moment it will always try to read the first connection if there is something there
      //Check if there is something in the queue, if so return immediately
      //otherwise wait for the semaphore and read again
      success = vchi_msg_dequeue( dispmanx_client.client_handle[0], response, max_length, &length_read, VCHI_FLAGS_NONE );
   } while( length_read == 0 && vcos_event_wait(&dispmanx_message_available_event) == VCOS_SUCCESS );

   return success;

}
/***********************************************************
 * Name: dispmanx_send_command
 *
 * Arguments: command, parameter buffer, parameter legnth
 *
 * Description: send a command and wait for its response (int)
 *
 * Returns: response
 *
 ***********************************************************/

static int32_t dispmanx_send_command(  uint32_t command, void *buffer, uint32_t length) {
   VCHI_MSG_VECTOR_T vector[] = { {&command, sizeof(command)},
                                  {buffer, length} };
   int32_t success = 0, response = -1;
   lock_obtain();
   success = vchi_msg_queuev( dispmanx_client.client_handle[0],
                              vector, sizeof(vector)/sizeof(vector[0]),
                              VCHI_FLAGS_BLOCK_UNTIL_QUEUED, NULL );
   if(success == 0 && !(command & DISPMANX_NO_REPLY_MASK)) {
      //otherwise only wait for a reply if we ask for one
      success = dispmanx_wait_for_reply(&response, sizeof(response));
   } else {
      //Not waiting for a reply, send the success code back instead
      response = success;
   }
   lock_release();
   return VC_VTOH32(response);
}

int32_t vc_dispmanx_send_command (uint32_t command, void *buffer,
                                  uint32_t length)
{
  return dispmanx_send_command (command, buffer, length);
}

/***********************************************************
 * Name: dispmanx_send_command_reply
 *
 * Arguments: command, parameter buffer, parameter legnth, reply buffer, buffer length
 *
 * Description: send a command and wait for its response (in a buffer)
 *
 * Returns: error code
 *
 ***********************************************************/

static int32_t dispmanx_send_command_reply(  uint32_t command, void *buffer, uint32_t length,
                                            void *response, uint32_t max_length) {
   VCHI_MSG_VECTOR_T vector[] = { {&command, sizeof(command)},
                                   {buffer, length} };

   int32_t success = 0;
   lock_obtain();
   success = vchi_msg_queuev( dispmanx_client.client_handle[0],
                               vector, sizeof(vector)/sizeof(vector[0]),
                               VCHI_FLAGS_BLOCK_UNTIL_QUEUED, NULL );
   if(success == 0)
      success = dispmanx_wait_for_reply(response, max_length);

   lock_release();

   return success;
}

int32_t vc_dispmanx_send_command_reply (uint32_t command, void *buffer, uint32_t length,
                                        void *response, uint32_t max_length)
{
  return dispmanx_send_command_reply (command, buffer, length, response, max_length);
}

/***********************************************************
 * Name: dispmanx_get_handle
 *
 * Arguments: command, parameter buffer, parameter legnth
 *
 * Description: same as dispmanx_send_command but returns uint instead
 *
 * Returns: handle
 *
 ***********************************************************/
static uint32_t dispmanx_get_handle(  uint32_t command, void *buffer, uint32_t length) {
   VCHI_MSG_VECTOR_T vector[] = { {&command, sizeof(command)},
                                   {buffer, length} };
   uint32_t success = 0;
   uint32_t response = 0;
   lock_obtain();
   success += vchi_msg_queuev( dispmanx_client.client_handle[0],
                               vector, sizeof(vector)/sizeof(vector[0]),
                               VCHI_FLAGS_BLOCK_UNTIL_QUEUED, NULL );
   if(success == 0)
      success = dispmanx_wait_for_reply(&response, sizeof(response));

   lock_release();
   return VC_VTOH32(response);
}

/***********************************************************
 * Name: dispmanx_notify_handle
 *
 * Arguments: not used
 *
 * Description: this purely notifies the update callback
 *
 * Returns: does not return
 *
 ***********************************************************/
static void *dispmanx_notify_func( void *arg ) {
   int32_t success;
   VCOS_STATUS_T status;

   (void)arg;

   while (1) {
      DISPMANX_UPDATE_HANDLE_T handle;
      status = vcos_event_wait(&dispmanx_notify_available_event);
      if (status != VCOS_SUCCESS || !dispmanx_client.initialised)
         break;

      while (1) {
         success = vchi_msg_dequeue( dispmanx_client.notify_handle[0], dispmanx_client.notify_buffer, sizeof(dispmanx_client.notify_buffer), &dispmanx_client.notify_length, VCHI_FLAGS_NONE );
         if (success != 0)
            break;

         handle = (DISPMANX_UPDATE_HANDLE_T)dispmanx_client.notify_buffer[0];
         if (handle) {
            // This is the response to an update submit
            // Decrement the use count - the corresponding "use" is in vc_dispmanx_update_submit.
            vchi_service_release(dispmanx_client.notify_handle[0]);
            if (dispmanx_client.update_callback ) {
               vcos_assert( dispmanx_client.pending_update_handle == handle);
               dispmanx_client.update_callback(handle, dispmanx_client.update_callback_param);
            }
         } else {
            // This is a vsync notification
            if (dispmanx_client.vsync_callback ) {
               dispmanx_client.vsync_callback(handle, dispmanx_client.vsync_callback_param);
            }
         }
      }
   }
   return 0;
}
