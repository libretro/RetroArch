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

#if !defined(WFC_IPC_H)
#define WFC_IPC_H

#include "interface/vcos/vcos.h"
#include "interface/khronos/include/WF/wfc.h"
#include "interface/khronos/wf/wfc_int.h"
#include "interface/khronos/wf/wfc_server_api.h"
#include "interface/khronos/include/EGL/eglext.h"

#define WFC_IPC_CONTROL_FOURCC()   VCHIQ_MAKE_FOURCC('W','F','C','I')

/* Define the current version number of the IPC API that the host library of VC
 * server is built against.
 */
/* The current IPC version number */
#define WFC_IPC_VER_CURRENT     8

/* The minimum version number for backwards compatibility */
#ifndef WFC_IPC_VER_MINIMUM
#define WFC_IPC_VER_MINIMUM     5
#endif

/** Definitions of messages used for implementing the WFC API on the server.
 *
 * These are passed to the server thread via VCHIQ.
 */

typedef enum {
   WFC_IPC_MSG_SET_CLIENT_PID,
   WFC_IPC_MSG_GET_VERSION,            /**< Returns major, minor and minimum values */
   WFC_IPC_MSG_CREATE_CONTEXT,         /**< Returns uint32_t */
   WFC_IPC_MSG_DESTROY_CONTEXT,
   WFC_IPC_MSG_COMMIT_SCENE,
   WFC_IPC_MSG_ACTIVATE,
   WFC_IPC_MSG_DEACTIVATE,
   WFC_IPC_MSG_SET_DEFERRAL_STREAM,
   WFC_IPC_MSG_SS_CREATE,              /**< Returns WFCNativeStreamType */
   WFC_IPC_MSG_SS_DESTROY,
   WFC_IPC_MSG_SS_ON_RECTS_CHANGE,
   WFC_IPC_MSG_SS_GET_RECTS,
   WFC_IPC_MSG_SS_IS_IN_USE,           /**< Returns uint32_t */
   WFC_IPC_MSG_SS_ALLOCATE_IMAGES,     /**< Returns uint32_t */
   WFC_IPC_MSG_SS_SIGNAL_EGLIMAGE_DATA,
   WFC_IPC_MSG_SS_SIGNAL_MM_IMAGE_DATA,
   WFC_IPC_MSG_SS_SIGNAL_RAW_PIXELS,
   WFC_IPC_MSG_SS_REGISTER,
   WFC_IPC_MSG_SS_UNREGISTER,
   WFC_IPC_MSG_SS_ON_IMAGE_AVAILABLE,
   WFC_IPC_MSG_SS_SIGNAL_IMAGE,        /**< Signal to update the front buffer of a generic image stream */
   WFC_IPC_MSG_SS_CREATE_INFO,         /**< Returns WFCNativeStreamType */
   WFC_IPC_MSG_SS_GET_INFO,            /**< Get stream configuration information */

   WFC_IPC_MSG_COUNT,                  /**< Always immediately after last client message type */

   WFC_IPC_MSG_CALLBACK,               /**< Sent from server to complete callback */

   WFC_IPC_MSG_MAX = 0x7FFFFFFF        /**< Force type to be 32-bit */
} WFC_IPC_MSG_TYPE;

/** Padded pointer type, for when client and server have different size pointers.
 * Set the padding field type to be big enough on both to hold a pointer.
 */
#define WFC_IPC_PTR_T(T)  union { uint32_t padding; T ptr; }
typedef WFC_IPC_PTR_T(WFC_CALLBACK_T)  WFC_IPC_CALLBACK_T;
typedef WFC_IPC_PTR_T(void *)          WFC_IPC_VOID_PTR_T;

/** The message header. All messages must start with this structure. */
typedef struct
{
   uint32_t magic;                     /**< Sentinel value to perform simple validation */
   WFC_IPC_MSG_TYPE type;              /**< The type of the message */

   /** Opaque client pointer, passed back in a reply */
   WFC_IPC_PTR_T(struct WFC_WAITER_T *) waiter;
} WFC_IPC_MSG_HEADER_T;

/** General purpose message, for passing just a uint32_t. */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   uint32_t value;
} WFC_IPC_MSG_UINT32_T;

/** General purpose message, for passing just a context. */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   WFCContext context;
} WFC_IPC_MSG_CONTEXT_T;

/** General purpose message, for passing just a stream. */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   WFCNativeStreamType stream;
} WFC_IPC_MSG_STREAM_T;

/** General purpose message, for calling a client provided callback. */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   WFC_IPC_CALLBACK_T callback_fn;     /**< Opaque client function pointer */
   WFC_IPC_VOID_PTR_T callback_data;   /**< Opaque client data */
} WFC_IPC_MSG_CALLBACK_T;

/** Set client process identifier message */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   uint32_t pid_lo;
   uint32_t pid_hi;
} WFC_IPC_MSG_SET_CLIENT_PID_T;

/** Get version reply message */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   uint32_t major;
   uint32_t minor;
   uint32_t minimum;
} WFC_IPC_MSG_GET_VERSION_T;

/** Create context message */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   WFCContext context;
   uint32_t context_type;
   uint32_t screen_or_stream_num;
   uint32_t pid_lo;
   uint32_t pid_hi;
} WFC_IPC_MSG_CREATE_CONTEXT_T;

/** Compose scene message */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   WFC_IPC_CALLBACK_T scene_taken_cb;     /**< Opaque client function pointer */
   WFC_IPC_VOID_PTR_T scene_taken_data;   /**< Opaque client data */
   WFCContext context;
   uint32_t flags;
   WFC_SCENE_T scene;
} WFC_IPC_MSG_COMMIT_SCENE_T;

/** Set deferral stream message */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   WFCContext context;
   WFCNativeStreamType stream;
} WFC_IPC_MSG_SET_DEFERRAL_STREAM_T;

/** Create stream message */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   WFCNativeStreamType stream;
   uint32_t flags;
   uint32_t pid_lo;
   uint32_t pid_hi;
} WFC_IPC_MSG_SS_CREATE_T;

/** Create stream using info block message */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   WFCNativeStreamType stream;
   WFC_STREAM_INFO_T info;
   uint32_t pid_lo;
   uint32_t pid_hi;
} WFC_IPC_MSG_SS_CREATE_INFO_T;

/** Destroy stream message */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   WFCNativeStreamType stream;
   uint32_t pid_lo;
   uint32_t pid_hi;
} WFC_IPC_MSG_SS_DESTROY_T;

/** Set stream rectangle update callback message */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   WFCNativeStreamType stream;
   WFC_IPC_CALLBACK_T rects_change_cb;    /**< Opaque client function pointer */
   WFC_IPC_VOID_PTR_T rects_change_data;  /**< Opaque client data */
} WFC_IPC_MSG_SS_ON_RECTS_CHANGE_T;

/** Get rectangles reply message */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   uint32_t result;
   int32_t rects[WFC_SERVER_STREAM_RECTS_SIZE];
} WFC_IPC_MSG_SS_GET_RECTS_T;

/** Allocate stream target images message */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   WFCNativeStreamType stream;
   uint32_t width;
   uint32_t height;
   uint32_t nbufs;
} WFC_IPC_MSG_SS_ALLOCATE_IMAGES_T;

/** Signal new EGLImage image message */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   WFCNativeStreamType stream;
   uint32_t ustorage;
   uint32_t width;
   uint32_t height;
   uint32_t stride;
   uint32_t offset;
   uint32_t format;
   uint32_t flags;
   bool flip;
} WFC_IPC_MSG_SS_SIGNAL_EGLIMAGE_DATA_T;

/** Signal new multimedia image message */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   WFCNativeStreamType stream;
   uint32_t image_handle;
} WFC_IPC_MSG_SS_SIGNAL_MM_IMAGE_DATA_T;

/** Signal new raw pixel image message */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   WFCNativeStreamType stream;
   uint32_t handle;
   uint32_t format;
   uint32_t width;
   uint32_t height;
   uint32_t pitch;
   uint32_t vpitch;
} WFC_IPC_MSG_SS_SIGNAL_RAW_PIXELS_T;

/** Signals a new image buffer */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */
   WFCNativeStreamType stream;

   /**< Descibes the image buffer.
    * image.length initialised to sizeof(WFC_STREAM_IMAGE_T) */
   WFC_STREAM_IMAGE_T image;

} WFC_IPC_MSG_SS_SIGNAL_IMAGE_T;

/** Register stream as owned by process message */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   WFCNativeStreamType stream;
   uint32_t pid_lo;
   uint32_t pid_hi;
} WFC_IPC_MSG_SS_REGISTER_T;

/** Unregister stream as owned by process message */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   WFCNativeStreamType stream;
   uint32_t pid_lo;
   uint32_t pid_hi;
} WFC_IPC_MSG_SS_UNREGISTER_T;

typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   uint32_t result;
   WFC_STREAM_INFO_T info;
} WFC_IPC_MSG_SS_GET_INFO_T;

/** Set stream image available callback message */
typedef struct {
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   WFCNativeStreamType stream;
   WFC_IPC_CALLBACK_T image_available_cb;    /**< Opaque client function pointer */
   WFC_IPC_VOID_PTR_T image_available_data;  /**< Opaque client data */
} WFC_IPC_MSG_SS_ON_IMAGE_AVAILABLE_T;

/** All messages sent between the client and server must be represented in
 * this union.
 */
typedef union
{
   WFC_IPC_MSG_HEADER_T header;  /**< All messages start with a header */

   WFC_IPC_MSG_UINT32_T u32_msg;
   WFC_IPC_MSG_CONTEXT_T context;
   WFC_IPC_MSG_STREAM_T stream;
   WFC_IPC_MSG_CREATE_CONTEXT_T create_context;
   WFC_IPC_MSG_COMMIT_SCENE_T commit_scene;
   WFC_IPC_MSG_SET_DEFERRAL_STREAM_T set_deferral_stream;
   WFC_IPC_MSG_SS_CREATE_T ss_create;
   WFC_IPC_MSG_SS_CREATE_INFO_T ss_create_info;
   WFC_IPC_MSG_SS_DESTROY_T ss_destroy;
   WFC_IPC_MSG_SS_ON_RECTS_CHANGE_T ss_on_rects_change;
   WFC_IPC_MSG_SS_ALLOCATE_IMAGES_T ss_allocate_images;
   WFC_IPC_MSG_SS_SIGNAL_EGLIMAGE_DATA_T ss_signal_eglimage_data;
   WFC_IPC_MSG_SS_SIGNAL_MM_IMAGE_DATA_T ss_signal_mm_image_data;
   WFC_IPC_MSG_SS_SIGNAL_RAW_PIXELS_T ss_signal_raw_image_data;
   WFC_IPC_MSG_SS_REGISTER_T ss_register;
   WFC_IPC_MSG_SS_UNREGISTER_T ss_unregister;
   WFC_IPC_MSG_SS_ON_IMAGE_AVAILABLE_T ss_on_image_available;
   WFC_IPC_MSG_GET_VERSION_T get_version;
   WFC_IPC_MSG_SS_GET_RECTS_T ss_get_rects;
   WFC_IPC_MSG_SS_SIGNAL_IMAGE_T ss_signal_image;
} WFC_IPC_MSG_T;

#define WFC_IPC_MSG_MAGIC       VCHIQ_MAKE_FOURCC('W', 'F', 'C', 'm')

#endif   /* WFC_IPC_H */
