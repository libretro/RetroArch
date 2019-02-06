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

// OpenMAX IL Component Service definitions

#ifndef VC_ILCS_DEFS_H
#define VC_ILCS_DEFS_H

#define VC_ILCS_VERSION 1

#ifdef USE_VCHIQ_ARM
#include "interface/vchiq_arm/vchiq.h"
#else
#include "interface/vchiq/vchiq.h"
#endif

typedef enum {
   IL_RESPONSE,
   IL_CREATE_COMPONENT,

   IL_GET_COMPONENT_VERSION,
   IL_SEND_COMMAND,
   IL_GET_PARAMETER,
   IL_SET_PARAMETER,
   IL_GET_CONFIG,
   IL_SET_CONFIG,
   IL_GET_EXTENSION_INDEX,
   IL_GET_STATE,
   IL_COMPONENT_TUNNEL_REQUEST,
   IL_USE_BUFFER,
   IL_USE_EGL_IMAGE,
   IL_ALLOCATE_BUFFER,
   IL_FREE_BUFFER,
   IL_EMPTY_THIS_BUFFER,
   IL_FILL_THIS_BUFFER,
   IL_SET_CALLBACKS,
   IL_COMPONENT_ROLE_ENUM,

   IL_COMPONENT_DEINIT,

   IL_EVENT_HANDLER,
   IL_EMPTY_BUFFER_DONE,
   IL_FILL_BUFFER_DONE,

   IL_COMPONENT_NAME_ENUM,
   IL_GET_DEBUG_INFORMATION,

   IL_SERVICE_QUIT,
   IL_FUNCTION_MAX_NUM,
   IL_FUNCTION_MAX = 0x7fffffff
} IL_FUNCTION_T;

// size of the largest structure passed by get/set
// parameter/config
// this should be calculated at compile time from IL headers
// must be a multiple of VC_INTERFACE_BLOCK_SIZE
#define VC_ILCS_MAX_PARAM_SIZE 288

// size of the largest structure below
#define VC_ILCS_MAX_CMD_LENGTH (sizeof(IL_GET_EXECUTE_T))

#define VC_ILCS_MAX_INLINE (VCHIQ_SLOT_SIZE-8-16)

// all structures should be padded to be multiples of
// VC_INTERFACE_BLOCK_SIZE in length (currently 16)
typedef struct {
   void *reference;
} IL_EXECUTE_HEADER_T;

typedef struct {
   IL_FUNCTION_T func;
   OMX_ERRORTYPE err;
} IL_RESPONSE_HEADER_T;

// create instance
typedef struct {
   OMX_PTR mark;
   char name[256];
} IL_CREATE_COMPONENT_EXECUTE_T;

typedef struct {
   IL_FUNCTION_T func;
   OMX_ERRORTYPE err;
   void *reference;
   OMX_U32 numPorts;
   OMX_U32 portDir;
   OMX_U32 portIndex[32];
} IL_CREATE_COMPONENT_RESPONSE_T;

// set callbacks
typedef struct {
   void *reference;
   void *pAppData;
} IL_SET_CALLBACKS_EXECUTE_T;

// get state
typedef struct {
   IL_FUNCTION_T func;
   OMX_ERRORTYPE err;
   OMX_STATETYPE state;
} IL_GET_STATE_RESPONSE_T;

// get parameter & get config
#define IL_GET_EXECUTE_HEADER_SIZE 8
typedef struct {
   void *reference;
   OMX_INDEXTYPE index;
   unsigned char param[VC_ILCS_MAX_PARAM_SIZE];
} IL_GET_EXECUTE_T;

#define IL_GET_RESPONSE_HEADER_SIZE 8
typedef struct {
   IL_FUNCTION_T func;
   OMX_ERRORTYPE err;
   unsigned char param[VC_ILCS_MAX_PARAM_SIZE];
} IL_GET_RESPONSE_T;

// set parameter & set config
#define IL_SET_EXECUTE_HEADER_SIZE 8
typedef struct {
   void *reference;
   OMX_INDEXTYPE index;
   unsigned char param[VC_ILCS_MAX_PARAM_SIZE];
} IL_SET_EXECUTE_T;

// send command
typedef struct {
   void *reference;
   OMX_COMMANDTYPE cmd;
   OMX_U32 param;
   OMX_MARKTYPE mark;
} IL_SEND_COMMAND_EXECUTE_T;

// event handler callback
typedef struct {
   void *reference;
   OMX_EVENTTYPE event;
   OMX_U32 data1;
   OMX_U32 data2;
   OMX_PTR eventdata;
} IL_EVENT_HANDLER_EXECUTE_T;

// use/allocate buffer
typedef struct {
   void *reference;
   OMX_PTR bufferReference;
   OMX_U32 port;
   OMX_U32 size;
   void *eglImage;
} IL_ADD_BUFFER_EXECUTE_T;

typedef struct {
   IL_FUNCTION_T func;
   OMX_ERRORTYPE err;
   OMX_PTR reference;
   OMX_BUFFERHEADERTYPE bufferHeader;
} IL_ADD_BUFFER_RESPONSE_T;

// free buffer
typedef struct {
   void *reference;
   OMX_U32 port;
   OMX_PTR bufferReference;
   IL_FUNCTION_T func;
   OMX_PTR inputPrivate;
   OMX_PTR outputPrivate;
} IL_FREE_BUFFER_EXECUTE_T;

// empty/fill this buffer
typedef enum {
   IL_BUFFER_NONE,
   IL_BUFFER_BULK,
   IL_BUFFER_INLINE,
   IL_BUFFER_MAX = 0x7fffffff
} IL_BUFFER_METHOD_T;

#define IL_BUFFER_BULK_UNALIGNED_MAX (32) // This value needs to be the same on voth VC and HOST.
                                          // Here, we just manually set it to the max of VCHI_BULK_ALIGN on VC and HOST.
#if ( VCHI_BULK_ALIGN > IL_BUFFER_BULK_UNALIGNED_MAX )
   #error "VCHI_BULK_ALIGN > IL_BUFFER_BULK_UNALIGNED_MAX. Just set max higher on both VC and HOST so there's space to put the unaligned bytes."
#endif
typedef struct {
   OMX_U8 header[IL_BUFFER_BULK_UNALIGNED_MAX-1];
   OMX_U8 headerlen;
   OMX_U8 trailer[IL_BUFFER_BULK_UNALIGNED_MAX-1];
   OMX_U8 trailerlen;
} IL_BUFFER_BULK_T;

typedef struct {
   OMX_U8 buffer[1];
} IL_BUFFER_INLINE_T;

typedef struct {
   void *reference;
   OMX_BUFFERHEADERTYPE bufferHeader;
   IL_BUFFER_METHOD_T method;
   OMX_U32 bufferLen;
} IL_PASS_BUFFER_EXECUTE_T;

// get component version
typedef struct {
   IL_FUNCTION_T func;
   OMX_ERRORTYPE err;
   char name[128];
   OMX_VERSIONTYPE component_version;
   OMX_VERSIONTYPE spec_version;
   OMX_UUIDTYPE uuid;
} IL_GET_VERSION_RESPONSE_T;

// get extension index
typedef struct {
   void *reference;
   char name[128];
} IL_GET_EXTENSION_EXECUTE_T;

typedef struct {
   IL_FUNCTION_T func;
   OMX_ERRORTYPE err;
   OMX_INDEXTYPE index;
} IL_GET_EXTENSION_RESPONSE_T;

// component role enum
typedef struct {
   void *reference;
   OMX_U32 index;
} IL_COMPONENT_ROLE_ENUM_EXECUTE_T;

typedef struct {
   IL_FUNCTION_T func;
   OMX_ERRORTYPE err;
   OMX_U8 role[128];
} IL_COMPONENT_ROLE_ENUM_RESPONSE_T;

typedef struct {
   void *reference;
   OMX_U32 port;
   OMX_PTR tunnel_ref;       // reference to use in requests - address of host/vc component
   OMX_BOOL tunnel_host;     // whether tunnel_ref is a host component
   OMX_U32 tunnel_port;
   OMX_TUNNELSETUPTYPE setup;
} IL_TUNNEL_REQUEST_EXECUTE_T;

typedef struct {
   IL_FUNCTION_T func;
   OMX_ERRORTYPE err;
   OMX_TUNNELSETUPTYPE setup;
} IL_TUNNEL_REQUEST_RESPONSE_T;

typedef struct {
   int index;
} IL_COMPONENT_NAME_ENUM_EXECUTE_T;

typedef struct {
   IL_FUNCTION_T func;
   OMX_ERRORTYPE err;
   OMX_U8 name[128];
} IL_COMPONENT_NAME_ENUM_RESPONSE_T;

typedef struct {
   OMX_S32 len;
} IL_GET_DEBUG_INFORMATION_EXECUTE_T;

#endif // VC_ILCS_DEFS_H
