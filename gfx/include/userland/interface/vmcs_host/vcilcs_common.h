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

// OpenMAX IL Component Service common - Host side header

typedef struct {
   OMX_U32 port;
   IL_FUNCTION_T func;
   OMX_BOOL bEGL;
   OMX_U32 numBuffers;
   OMX_DIRTYPE dir;
} VC_PRIVATE_PORT_T;

struct _VC_PRIVATE_COMPONENT_T {
   OMX_COMPONENTTYPE *comp;
   void *reference;
   OMX_U32 numPorts;
   OMX_CALLBACKTYPE callbacks;
   OMX_PTR callback_state;
   VC_PRIVATE_PORT_T *port;
   struct _VC_PRIVATE_COMPONENT_T *next;
};
typedef struct _VC_PRIVATE_COMPONENT_T  VC_PRIVATE_COMPONENT_T;

struct ILCS_COMMON_T {
   VCOS_SEMAPHORE_T component_lock;
   VC_PRIVATE_COMPONENT_T *component_list;
   ILCS_SERVICE_T *ilcs;
};

VCHPRE_ void VCHPOST_ vcilcs_config(ILCS_CONFIG_T *config);

// functions that implement incoming functions calls
// from VideoCore components to host based components
VCHPRE_ void VCHPOST_ vcil_in_get_state(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen);
VCHPRE_ void VCHPOST_ vcil_in_get_parameter(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen);
VCHPRE_ void VCHPOST_ vcil_in_set_parameter(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen);
VCHPRE_ void VCHPOST_ vcil_in_get_config(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen);
VCHPRE_ void VCHPOST_ vcil_in_set_config(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen);
VCHPRE_ void VCHPOST_ vcil_in_use_buffer(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen);
VCHPRE_ void VCHPOST_ vcil_in_free_buffer(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen);
VCHPRE_ void VCHPOST_ vcil_in_empty_this_buffer(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen);
VCHPRE_ void VCHPOST_ vcil_in_fill_this_buffer(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen);
VCHPRE_ void VCHPOST_ vcil_in_get_component_version(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen);
VCHPRE_ void VCHPOST_ vcil_in_get_extension_index(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen);
VCHPRE_ void VCHPOST_ vcil_in_component_role_enum(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen);

// functions that implement callbacks from VideoCore
// components to the host core.
// The prefix is vcil_out since they implement part
// of the API that the host uses out to VideoCore
VCHPRE_ void VCHPOST_ vcil_out_event_handler(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen);
VCHPRE_ void VCHPOST_ vcil_out_empty_buffer_done(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen);
VCHPRE_ void VCHPOST_ vcil_out_fill_buffer_done(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen);

// functions used by the host IL core
VCHPRE_ OMX_ERRORTYPE VCHPOST_ vcil_out_get_debug_information(ILCS_COMMON_T *st, OMX_STRING debugInfo, OMX_S32 *pLen);
VCHPRE_ OMX_ERRORTYPE VCHPOST_ vcil_out_create_component(ILCS_COMMON_T *st, OMX_HANDLETYPE hComponent, OMX_STRING component_name);
VCHPRE_ OMX_ERRORTYPE VCHPOST_ vcil_out_component_name_enum(ILCS_COMMON_T *st, OMX_STRING cComponentName, OMX_U32 nNameLength, OMX_U32 nIndex);
