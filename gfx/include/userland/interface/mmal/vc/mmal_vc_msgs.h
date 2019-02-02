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

#ifndef MMAL_VC_MSGS_H
#define MMAL_VC_MSGS_H

/** @file mmal_vc_msgs.h
  *
  * Private message definitions, defining the message API between
  * the host and VideoCore.
  */
#include "interface/vcos/vcos.h"
#include "interface/mmal/mmal.h"
#include "mmal_vc_api.h"

#define MMAL_CONTROL_FOURCC() VCHIQ_MAKE_FOURCC('m','m','a','l')

/* Major version indicates binary backwards compatibility */
#define WORKER_VER_MAJOR   16
#define WORKER_VER_MINIMUM 10
/* Minor version is not used normally.
 */
#define WORKER_VER_MINOR   1
#ifndef WORKER_VER_MINIMUM
#endif

#define VIDEOCORE_PREFIX "vc"

#define MMAL_MAX_PORTS     8                 /**< Max ports per component */

#define MMAL_WORKER_MAX_MSG_LEN  512
#define MMAL_VC_CORE_STATS_NAME_MAX      32  /**< Length of the name in the core stats message */

/** A MMAL_CONTROL_SERVICE_T gets space for a single message. This
  * is the space allocated for these messages.
  */
#define MMAL_WORKER_MSG_LEN  28

/** Maximum size of the format extradata.
 * FIXME: should probably be made bigger and maybe be passed separately from the info.
 */
#define MMAL_FORMAT_EXTRADATA_MAX_SIZE 128

/** Size of space reserved in a buffer message for short messages.
 */
#define MMAL_VC_SHORT_DATA 128

/** Message ids sent to worker thread.
  */

/* Please update the array in mmal_vc_msgnames.c if this is updated.
 */
typedef enum {
   MMAL_WORKER_QUIT = 1,
   MMAL_WORKER_SERVICE_CLOSED,
   MMAL_WORKER_GET_VERSION,
   MMAL_WORKER_COMPONENT_CREATE,
   MMAL_WORKER_COMPONENT_DESTROY,
   MMAL_WORKER_COMPONENT_ENABLE,
   MMAL_WORKER_COMPONENT_DISABLE,
   MMAL_WORKER_PORT_INFO_GET,
   MMAL_WORKER_PORT_INFO_SET,
   MMAL_WORKER_PORT_ACTION,
   MMAL_WORKER_BUFFER_FROM_HOST,
   MMAL_WORKER_BUFFER_TO_HOST,
   MMAL_WORKER_GET_STATS,
   MMAL_WORKER_PORT_PARAMETER_SET,
   MMAL_WORKER_PORT_PARAMETER_GET,
   MMAL_WORKER_EVENT_TO_HOST,
   MMAL_WORKER_GET_CORE_STATS_FOR_PORT,
   MMAL_WORKER_OPAQUE_ALLOCATOR,
   /* VC debug mode only - due to security, denial of service implications */
   MMAL_WORKER_CONSUME_MEM,
   MMAL_WORKER_LMK,
   MMAL_WORKER_OPAQUE_ALLOCATOR_DESC,
   MMAL_WORKER_DRM_GET_LHS32,
   MMAL_WORKER_DRM_GET_TIME,
   MMAL_WORKER_BUFFER_FROM_HOST_ZEROLEN,
   MMAL_WORKER_PORT_FLUSH,
   MMAL_WORKER_HOST_LOG,
   MMAL_WORKER_COMPACT,
   MMAL_WORKER_MSG_LAST
} MMAL_WORKER_CMD_T;

/** Every message has one of these at the start.
  */
typedef struct
{
   uint32_t magic;
   uint32_t msgid;
   struct MMAL_CONTROL_SERVICE_T *control_service;       /** Handle to the control service */

   union {
      struct MMAL_WAITER_T *waiter;    /** User-land wait structure, passed back */
   } u;

   MMAL_STATUS_T status;            /** Result code, passed back */
   /* Make sure this structure is 64 bit aligned */
   uint32_t dummy;
} mmal_worker_msg_header;

/* Make sure mmal_worker_msg_header will preserve 64 bits alignment */
vcos_static_assert(!(sizeof(mmal_worker_msg_header) & 0x7));

/* Message structures sent to worker thread.
 */

/** Tell the worker a service has closed. It should start to delete
  * the associated components.
  */
typedef struct
{
   mmal_worker_msg_header header;
} mmal_worker_service_closed;
vcos_static_assert(sizeof(mmal_worker_service_closed) <= MMAL_WORKER_MSG_LEN);

/** Send from VC to host to report our version */
typedef struct
{
   mmal_worker_msg_header header;
   uint32_t flags;
   uint32_t major;
   uint32_t minor;
   uint32_t minimum;
} mmal_worker_version;

/** Request component creation */
typedef struct
{
   mmal_worker_msg_header header;
   void *client_component;             /** Client component */
   char name[128];
   uint32_t pid;                       /**< For debug */
} mmal_worker_component_create;

/** Reply to component-creation message. Reports back
  * the number of ports.
  */
typedef struct
{
   mmal_worker_msg_header header;
   MMAL_STATUS_T status;
   uint32_t component_handle;          /** Handle on VideoCore for component */
   uint32_t input_num;                 /**< Number of input ports */
   uint32_t output_num;                /**< Number of output ports */
   uint32_t clock_num;                 /**< Number of clock ports */
} mmal_worker_component_create_reply;
vcos_static_assert(sizeof(mmal_worker_component_create_reply) <= MMAL_WORKER_MAX_MSG_LEN);

/** Destroys a component
  */
typedef struct
{
   mmal_worker_msg_header header;
   uint32_t component_handle;          /**< which component */
} mmal_worker_component_destroy;

/** Enables a component
  */
typedef struct
{
   mmal_worker_msg_header header;
   uint32_t component_handle;          /**< which component */
} mmal_worker_component_enable;

/** Disable a component
  */
typedef struct
{
   mmal_worker_msg_header header;
   uint32_t component_handle;          /**< Which component */
} mmal_worker_component_disable;

/** Component port info. Used to get port info.
  */
typedef struct
{
   mmal_worker_msg_header header;
   uint32_t component_handle;          /**< Which component */
   MMAL_PORT_TYPE_T port_type;         /**< Type of port */
   uint32_t index;                     /**< Which port of given type to get */
} mmal_worker_port_info_get;
vcos_static_assert(sizeof(mmal_worker_port_info_get) <= MMAL_WORKER_MAX_MSG_LEN);

/** Component port info. Used to set port info.
  */
typedef struct
{
   mmal_worker_msg_header header;
   uint32_t component_handle;          /**< Which component */
   MMAL_PORT_TYPE_T port_type;         /**< Type of port */
   uint32_t index;                     /**< Which port of given type to get */
   MMAL_PORT_T port;
   MMAL_ES_FORMAT_T format;
   MMAL_ES_SPECIFIC_FORMAT_T es;
   uint8_t  extradata[MMAL_FORMAT_EXTRADATA_MAX_SIZE];
} mmal_worker_port_info_set;
vcos_static_assert(sizeof(mmal_worker_port_info_set) <= MMAL_WORKER_MAX_MSG_LEN);

/** Report port info back in response to a get / set. */
typedef struct
{
   mmal_worker_msg_header header;
   MMAL_STATUS_T status;               /**< Result of query */
   uint32_t component_handle;          /**< Which component */
   MMAL_PORT_TYPE_T port_type;         /**< Type of port */
   uint32_t index;                     /**< Which port of given type to get */
   int32_t found;                      /**< Did we find anything? */
   uint32_t port_handle;               /**< Handle to use for this port */
   MMAL_PORT_T port;
   MMAL_ES_FORMAT_T format;
   MMAL_ES_SPECIFIC_FORMAT_T es;
   uint8_t  extradata[MMAL_FORMAT_EXTRADATA_MAX_SIZE];
} mmal_worker_port_info;
vcos_static_assert(sizeof(mmal_worker_port_info) <= MMAL_WORKER_MAX_MSG_LEN);

typedef struct
{
   mmal_worker_msg_header header;
   MMAL_STATUS_T status;
} mmal_worker_reply;

typedef struct
{
   mmal_worker_msg_header header;
   MMAL_STATUS_T status;
   uint8_t secret[32];
} mmal_worker_drm_get_lhs32_reply;
vcos_static_assert(sizeof(mmal_worker_drm_get_lhs32_reply) <= MMAL_WORKER_MAX_MSG_LEN);

typedef struct
{
   mmal_worker_msg_header header;
   MMAL_STATUS_T status;
   uint32_t time;
} mmal_worker_drm_get_time_reply;
vcos_static_assert(sizeof(mmal_worker_drm_get_time_reply) <= MMAL_WORKER_MAX_MSG_LEN);

/** List of actions for a port */
enum MMAL_WORKER_PORT_ACTIONS
{
   MMAL_WORKER_PORT_ACTION_UNKNOWN = 0,        /**< Unknown action */
   MMAL_WORKER_PORT_ACTION_ENABLE,             /**< Enable a port */
   MMAL_WORKER_PORT_ACTION_DISABLE,            /**< Disable a port */
   MMAL_WORKER_PORT_ACTION_FLUSH,              /**< Flush a port */
   MMAL_WORKER_PORT_ACTION_CONNECT,            /**< Connect 2 ports together */
   MMAL_WORKER_PORT_ACTION_DISCONNECT,         /**< Disconnect 2 ports connected together */
   MMAL_WORKER_PORT_ACTION_SET_REQUIREMENTS,   /**< Set buffer requirements  */
   MMAL_WORKER_PORT_ACTION_MAX = 0x7fffffff    /**< Make the enum 32bits */
};

/** Trigger an action on a port.
  */
typedef struct
{
   mmal_worker_msg_header header;
   uint32_t component_handle;
   uint32_t port_handle;
   enum MMAL_WORKER_PORT_ACTIONS action;

   /** Action parameter */
   union {
      struct {
         MMAL_PORT_T port;
      } enable;
      struct {
         uint32_t component_handle;
         uint32_t port_handle;
      } connect;
   } param;

} mmal_worker_port_action;
vcos_static_assert(sizeof(mmal_worker_port_action) <= MMAL_WORKER_MAX_MSG_LEN);

#define MMAL_WORKER_PORT_PARAMETER_SPACE      96

#define MMAL_WORKER_PORT_PARAMETER_SET_MAX \
   (MMAL_WORKER_PORT_PARAMETER_SPACE*sizeof(uint32_t)+sizeof(MMAL_PARAMETER_HEADER_T))

#define MMAL_WORKER_PORT_PARAMETER_GET_MAX   MMAL_WORKER_PORT_PARAMETER_SET_MAX

/** Component port parameter set. Doesn't include space for the parameter data.
  */
typedef struct
{
   mmal_worker_msg_header header;
   uint32_t component_handle;          /**< Which component */
   uint32_t port_handle;               /**< Which port */
   MMAL_PARAMETER_HEADER_T param;      /**< Parameter ID and size */
   uint32_t space[MMAL_WORKER_PORT_PARAMETER_SPACE];
} mmal_worker_port_param_set;
vcos_static_assert(sizeof(mmal_worker_port_param_set) <= MMAL_WORKER_MAX_MSG_LEN);

/** Component port parameter get.
  */
typedef struct
{
   mmal_worker_msg_header header;
   uint32_t component_handle;          /**< Which component */
   uint32_t port_handle;               /**< Which port */
   MMAL_PARAMETER_HEADER_T param;      /**< Parameter ID and size */
   uint32_t space[MMAL_WORKER_PORT_PARAMETER_SPACE];
} mmal_worker_port_param_get;
vcos_static_assert(sizeof(mmal_worker_port_param_get) <= MMAL_WORKER_MAX_MSG_LEN);

typedef struct
{
   mmal_worker_msg_header header;
   uint32_t component_handle;          /**< Which component */
   uint32_t port_handle;               /**< Which port */
   MMAL_PARAMETER_HEADER_T param;      /**< Parameter ID and size */
} mmal_worker_port_param_get_old;

/** Component port parameter get reply. Doesn't include space for the parameter data.
  */
typedef struct
{
   mmal_worker_msg_header header;
   MMAL_STATUS_T status;               /**< Status of mmal_port_parameter_get call */
   MMAL_PARAMETER_HEADER_T param;      /**< Parameter ID and size */
   uint32_t space[MMAL_WORKER_PORT_PARAMETER_SPACE];
} mmal_worker_port_param_get_reply;
vcos_static_assert(sizeof(mmal_worker_port_param_get_reply) <= MMAL_WORKER_MAX_MSG_LEN);

/** Buffer header driver area structure. In the private area
  * of a buffer header there is a driver area where we can
  * put values. This structure defines the layout of that.
  */
struct MMAL_DRIVER_BUFFER_T
{
   uint32_t magic;
   uint32_t component_handle;    /**< The component this buffer is from */
   uint32_t port_handle;         /**< Index into array of ports for this component */

   /** Client side uses this to get back to its context structure. */
   struct MMAL_VC_CLIENT_BUFFER_CONTEXT_T *client_context;
};

/** Receive a buffer from the host.
  *
  * @sa mmal_port_send_buffer()
  */

typedef struct mmal_worker_buffer_from_host
{
   mmal_worker_msg_header header;

   /** Our control data, copied from the buffer header "driver area"
    * @sa mmal_buffer_header_driver_data().
    */
   struct MMAL_DRIVER_BUFFER_T drvbuf;

   /** Referenced buffer control data.
    * This is set if the buffer is referencing another
    * buffer as is the case with passthrough ports where
    * buffers on the output port reference buffers on the
    * input port. */
   struct MMAL_DRIVER_BUFFER_T drvbuf_ref;

   /** the buffer header itself */
   MMAL_BUFFER_HEADER_T buffer_header;
   MMAL_BUFFER_HEADER_TYPE_SPECIFIC_T buffer_header_type_specific;

   MMAL_BOOL_T is_zero_copy;
   MMAL_BOOL_T has_reference;

   /** If the data is short enough, then send it in the control message rather
    * than using a separate VCHIQ bulk transfer.
    */
   uint32_t payload_in_message;
   uint8_t short_data[MMAL_VC_SHORT_DATA];

} mmal_worker_buffer_from_host;
vcos_static_assert(sizeof(mmal_worker_buffer_from_host) <= MMAL_WORKER_MAX_MSG_LEN);

/** Maximum number of event data bytes that can be passed in the message.
 * More than this and the data is passed in a bulk message.
 */
#define MMAL_WORKER_EVENT_SPACE 256

/** Send an event buffer from the host.
  *
  * @sa mmal_port_send_event()
  */

typedef struct mmal_worker_event_to_host
{
   mmal_worker_msg_header header;

   struct MMAL_COMPONENT_T *client_component;
   uint32_t port_type;
   uint32_t port_num;

   uint32_t cmd;
   uint32_t length;
   uint8_t data[MMAL_WORKER_EVENT_SPACE];
   MMAL_BUFFER_HEADER_T *delayed_buffer;  /* Only used to remember buffer for bulk rx */
} mmal_worker_event_to_host;
vcos_static_assert(sizeof(mmal_worker_event_to_host) <= MMAL_WORKER_MAX_MSG_LEN);

typedef struct
{
   mmal_worker_msg_header header;
   MMAL_VC_STATS_T stats;
   uint32_t reset;
} mmal_worker_stats;
vcos_static_assert(sizeof(mmal_worker_stats) <= MMAL_WORKER_MAX_MSG_LEN);

typedef enum {
   MMAL_WORKER_OPAQUE_MEM_ALLOC,
   MMAL_WORKER_OPAQUE_MEM_RELEASE,
   MMAL_WORKER_OPAQUE_MEM_ACQUIRE,
   MMAL_WORKER_OPAQUE_MEM_MAX = 0x7fffffff,
} MMAL_WORKER_OPAQUE_MEM_OP;

typedef struct
{
   mmal_worker_msg_header header;
   MMAL_WORKER_OPAQUE_MEM_OP op;
   uint32_t handle;
   MMAL_STATUS_T status;
   char description[32];
} mmal_worker_opaque_allocator;

/*
 * Per-port core statistics
 */
typedef struct
{
   mmal_worker_msg_header header;
   uint32_t component_index;
   uint32_t port_index;
   MMAL_PORT_TYPE_T type;
   MMAL_CORE_STATS_DIR dir;
   MMAL_BOOL_T reset;
} mmal_worker_get_core_stats_for_port;

typedef struct
{
   mmal_worker_msg_header header;
   MMAL_STATUS_T status;
   MMAL_STATS_RESULT_T result;
   MMAL_CORE_STATISTICS_T stats;
   char component_name[MMAL_VC_CORE_STATS_NAME_MAX];
} mmal_worker_get_core_stats_for_port_reply;

typedef struct
{
   mmal_worker_msg_header header;
   MMAL_STATUS_T status;
   /* The amount of memory to reserve */
   uint32_t size;
   /* Handle to newly allocated memory or MEM_HANDLE_INVALD on failure */
   uint32_t handle;
} mmal_worker_consume_mem;
vcos_static_assert(sizeof(mmal_worker_consume_mem) <= MMAL_WORKER_MAX_MSG_LEN);

typedef struct
{
   mmal_worker_msg_header header;
   MMAL_STATUS_T status;
   uint32_t mode;
   uint32_t duration;
} mmal_worker_compact;
vcos_static_assert(sizeof(mmal_worker_compact) <= MMAL_WORKER_MAX_MSG_LEN);

typedef struct
{
   mmal_worker_msg_header header;
   /* Message text to add to the circular buffer */
   char msg[MMAL_WORKER_MAX_MSG_LEN - sizeof(mmal_worker_msg_header)];
} mmal_worker_host_log;
vcos_static_assert(sizeof(mmal_worker_host_log) <= MMAL_WORKER_MAX_MSG_LEN);

typedef struct
{
   mmal_worker_msg_header header;
   /* The memory allocation size to pass to lmk, as if in a response to an
    * allocation for this amount of memory. */
   uint32_t alloc_size;
} mmal_worker_lmk;
vcos_static_assert(sizeof(mmal_worker_lmk) <= MMAL_WORKER_MAX_MSG_LEN);

static inline void mmal_vc_buffer_header_to_msg(mmal_worker_buffer_from_host *msg,
                                                MMAL_BUFFER_HEADER_T *header)
{
   msg->buffer_header.cmd           = header->cmd;
   msg->buffer_header.offset        = header->offset;
   msg->buffer_header.length        = header->length;
   msg->buffer_header.flags         = header->flags;
   msg->buffer_header.pts           = header->pts;
   msg->buffer_header.dts           = header->dts;
   msg->buffer_header.alloc_size    = header->alloc_size;
   msg->buffer_header.data          = header->data;
   msg->buffer_header_type_specific = *header->type;
}

static inline void mmal_vc_msg_to_buffer_header(MMAL_BUFFER_HEADER_T *header,
                                                mmal_worker_buffer_from_host *msg)
{
   header->cmd    = msg->buffer_header.cmd;
   header->offset = msg->buffer_header.offset;
   header->length = msg->buffer_header.length;
   header->flags  = msg->buffer_header.flags;
   header->pts    = msg->buffer_header.pts;
   header->dts    = msg->buffer_header.dts;
   *header->type  = msg->buffer_header_type_specific;
}

#endif

