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

#ifndef ILCS_H
#define ILCS_H

#include "interface/vmcs_host/khronos/IL/OMX_Component.h"
#include "interface/vmcs_host/vc_ilcs_defs.h"

struct ILCS_SERVICE_T;
typedef struct ILCS_SERVICE_T ILCS_SERVICE_T;

struct ILCS_COMMON_T;
typedef struct ILCS_COMMON_T ILCS_COMMON_T;

typedef void (*IL_FN_T)(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen);

typedef struct {
   IL_FN_T *fns;
   ILCS_COMMON_T *(*ilcs_common_init)(ILCS_SERVICE_T *);
   void (*ilcs_common_deinit)(ILCS_COMMON_T *st);
   void (*ilcs_thread_init)(ILCS_COMMON_T *st);
   unsigned char *(*ilcs_mem_lock)(OMX_BUFFERHEADERTYPE *buffer);
   void (*ilcs_mem_unlock)(OMX_BUFFERHEADERTYPE *buffer);
} ILCS_CONFIG_T;

// initialise the VideoCore IL Component service
// returns pointer to state on success, NULL on failure
#ifdef USE_VCHIQ_ARM
VCHPRE_ ILCS_SERVICE_T VCHPOST_ *ilcs_init(VCHIQ_INSTANCE_T state, void **connection, ILCS_CONFIG_T *config, int use_memmgr);
#else
VCHPRE_ ILCS_SERVICE_T VCHPOST_ *ilcs_init(VCHIQ_STATE_T *state, void **connection, ILCS_CONFIG_T *config, int use_memmgr);
#endif

// deinitialises the IL Component service
VCHPRE_ void VCHPOST_ ilcs_deinit(ILCS_SERVICE_T *ilcs);

// returns 1 if the current thread is the ilcs thread, 0 otherwise
VCHPRE_ int VCHPOST_ ilcs_thread_current(void *param);

// returns pointer to shared state
VCHPRE_ ILCS_COMMON_T *ilcs_get_common(ILCS_SERVICE_T *ilcs);

VCHPRE_ int VCHPOST_ ilcs_execute_function(ILCS_SERVICE_T *ilcs, IL_FUNCTION_T func, void *data, int len, void *resp, int *rlen);
VCHPRE_ OMX_ERRORTYPE VCHPOST_ ilcs_pass_buffer(ILCS_SERVICE_T *ilcs, IL_FUNCTION_T func, void *reference, OMX_BUFFERHEADERTYPE *pBuffer);
VCHPRE_ OMX_BUFFERHEADERTYPE * VCHPOST_ ilcs_receive_buffer(ILCS_SERVICE_T *ilcs, void *call, int clen, OMX_COMPONENTTYPE **pComp);

// bulks are 16 bytes aligned, implicit in use of vchiq
#define ILCS_ALIGN   16

#define ILCS_ROUND_UP(x) ((((unsigned long)(x))+ILCS_ALIGN-1) & ~(ILCS_ALIGN-1))
#define ILCS_ROUND_DOWN(x) (((unsigned long)(x)) & ~(ILCS_ALIGN-1))
#define ILCS_ALIGNED(x) (((unsigned long)(x) & (ILCS_ALIGN-1)) == 0)


#ifdef _VIDEOCORE
#include "vcfw/logging/logging.h"

#ifdef ILCS_LOGGING

#define LOG_MSG ILCS_LOGGING
extern void ilcs_log_event_handler(OMX_HANDLETYPE hComponent, OMX_PTR pAppData, OMX_EVENTTYPE eEvent,
                                   OMX_U32 nData1,OMX_U32 nData2,OMX_PTR pEventData);

#else

#define LOG_MSG LOGGING_GENERAL
#define ilcs_log_event_handler(...)
extern void dummy_logging_message(int level, const char *format, ...);
#undef logging_message
#define logging_message if (1) {} else dummy_logging_message

#endif // ILCS_LOGGING
#endif // _VIDEOCORE

#endif // ILCS_H

