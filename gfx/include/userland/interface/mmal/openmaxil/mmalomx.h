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

/** \file
 * OpenMAX IL adaptation layer for MMAL
 */

#include "interface/vmcs_host/khronos/IL/OMX_Core.h"
#include "interface/vmcs_host/khronos/IL/OMX_Component.h"
#include "interface/vmcs_host/khronos/IL/OMX_Video.h"
#include "interface/vmcs_host/khronos/IL/OMX_Audio.h"
#include <mmal.h>
#include <util/mmal_il.h>

/* Define this to 1 if you want to log all buffer transfers */
#define ENABLE_MMAL_EXTRA_LOGGING 0

#ifndef MMALOMX_EXPORT
#  define MMALOMX_EXPORT(a) a
#endif
#ifndef MMALOMX_IMPORT
#  define MMALOMX_IMPORT(a) a
#endif

#define MAX_MARKS_NUM 2
#define MAX_ENCODINGS_NUM 20

/** Per-port context data */
typedef struct MMALOMX_PORT_T
{
   struct MMALOMX_COMPONENT_T *component;
   MMAL_PORT_T *mmal;
   OMX_DIRTYPE direction;
   unsigned int index;
   unsigned int buffers;
   unsigned int buffers_in_transit;

   MMAL_BOOL_T buffers_allocated:1;
   MMAL_BOOL_T enabled:1;
   MMAL_BOOL_T populated:1;
   MMAL_BOOL_T zero_copy:1;
   MMAL_BOOL_T no_cropping:1;
   MMAL_BOOL_T format_changed:1;
   MMAL_POOL_T *pool;

   uint32_t actions;

   OMX_MARKTYPE marks[MAX_MARKS_NUM];
   unsigned int marks_first:8;
   unsigned int marks_num:8;

   OMX_FORMAT_PARAM_TYPE format_param;

   MMAL_PARAMETER_HEADER_T encodings_header;
   MMAL_FOURCC_T encodings[MAX_ENCODINGS_NUM];
   unsigned int encodings_num;

} MMALOMX_PORT_T;

/** Component context data */
typedef struct MMALOMX_COMPONENT_T {
   OMX_COMPONENTTYPE omx;              /**< OMX component type structure */

   unsigned int registry_id;
   const char *name;
   uint32_t role;
   OMX_CALLBACKTYPE callbacks;
   OMX_PTR callbacks_data;

   struct MMAL_COMPONENT_T *mmal;
   OMX_STATETYPE state;
   unsigned int state_transition;

   MMALOMX_PORT_T *ports;
   unsigned int ports_num;
   unsigned int ports_domain_num[4];

   MMAL_BOOL_T actions_running;

   OMX_U32 group_id;
   OMX_U32 group_priority;

   /* Support for command queues */
   MMAL_POOL_T *cmd_pool;
   MMAL_QUEUE_T *cmd_queue;
   VCOS_THREAD_T cmd_thread;
   MMAL_BOOL_T cmd_thread_used;
   VCOS_SEMAPHORE_T cmd_sema;

   VCOS_MUTEX_T lock; /**< Used to protect component state */
   VCOS_MUTEX_T lock_port; /**< Used to protect port state */

} MMALOMX_COMPONENT_T;

OMX_ERRORTYPE mmalomx_callback_event_handler(
   MMALOMX_COMPONENT_T *component,
   OMX_EVENTTYPE eEvent,
   OMX_U32 nData1,
   OMX_U32 nData2,
   OMX_PTR pEventData);

#define MMALOMX_LOCK(a) vcos_mutex_lock(&a->lock)
#define MMALOMX_UNLOCK(a) vcos_mutex_unlock(&a->lock)
#define MMALOMX_LOCK_PORT(a,b) vcos_mutex_lock(&a->lock_port)
#define MMALOMX_UNLOCK_PORT(a,b) vcos_mutex_unlock(&a->lock_port)

