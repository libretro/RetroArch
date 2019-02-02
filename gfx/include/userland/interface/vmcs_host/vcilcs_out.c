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
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>

#include "interface/vchi/vchi.h"
#include "interface/vcos/vcos_dlfcn.h"
#include "interface/vmcs_host/khronos/IL/OMX_Component.h"
#include "interface/vmcs_host/khronos/IL/OMX_ILCS.h"
#include "interface/vmcs_host/vc_ilcs_defs.h"
#include "interface/vmcs_host/vcilcs.h"
#include "interface/vmcs_host/vcilcs_common.h"
#include "interface/vcos/vcos_dlfcn.h"

static VC_PRIVATE_PORT_T *find_port(VC_PRIVATE_COMPONENT_T *comp, OMX_U32 nPortIndex)
{
   OMX_U32 i=0;
   while (i<comp->numPorts && comp->port[i].port != nPortIndex)
      i++;

   if (i < comp->numPorts)
      return &comp->port[i];

   return NULL;
}

#ifndef NDEBUG
static int is_valid_hostside_buffer(OMX_BUFFERHEADERTYPE *pBuf)
{
   if (!pBuf)
      return 0;
   if (!pBuf->pBuffer)
      return 0;
   if ((unsigned long)pBuf->pBuffer < 0x100)
      return 0; // not believable
   return 1;
}
#endif

static OMX_ERRORTYPE vcil_out_ComponentDeInit(OMX_IN OMX_HANDLETYPE hComponent)
{
   OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *) hComponent;
   VC_PRIVATE_COMPONENT_T *comp;
   IL_EXECUTE_HEADER_T exe;
   IL_RESPONSE_HEADER_T resp;
   ILCS_COMMON_T *st;
   int rlen = sizeof(resp);

   if(!pComp)
      return OMX_ErrorBadParameter;

   st = pComp->pApplicationPrivate;
   comp = (VC_PRIVATE_COMPONENT_T *) pComp->pComponentPrivate;

   exe.reference = comp->reference;

   if(ilcs_execute_function(st->ilcs, IL_COMPONENT_DEINIT, &exe, sizeof(exe), &resp, &rlen) < 0 || rlen != sizeof(resp) ||
      resp.err == OMX_ErrorNone)
   {
      // remove from list, assuming that we successfully managed to deinit
      // this component, or that ilcs has returned an error.  The assumption
      // here is that if the component has managed to correctly signal an
      // error, it still exists, but if the transport has failed then we might
      // as well try and cleanup
      VC_PRIVATE_COMPONENT_T *list, *prev;

      vcos_semaphore_wait(&st->component_lock);

      list = st->component_list;
      prev = NULL;

      while (list != NULL && list != comp)
      {
         prev = list;
         list = list->next;
      }

      // failing to find this component is not a good sign.
      if(vcos_verify(list))
      {
         if (prev == NULL)
            st->component_list = list->next;
         else
            prev->next = list->next;
      }

      vcos_semaphore_post(&st->component_lock);
      vcos_free(comp);
   }

   return resp.err;
}

static OMX_ERRORTYPE vcil_out_GetComponentVersion(OMX_IN  OMX_HANDLETYPE hComponent,
      OMX_OUT OMX_STRING pComponentName,
      OMX_OUT OMX_VERSIONTYPE* pComponentVersion,
      OMX_OUT OMX_VERSIONTYPE* pSpecVersion,
      OMX_OUT OMX_UUIDTYPE* pComponentUUID)
{
   OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *) hComponent;
   VC_PRIVATE_COMPONENT_T *comp;
   IL_EXECUTE_HEADER_T exe;
   IL_GET_VERSION_RESPONSE_T resp;
   ILCS_COMMON_T *st;
   int rlen = sizeof(resp);

   if (!(pComp && pComponentName && pComponentVersion && pSpecVersion && pComponentUUID))
      return OMX_ErrorBadParameter;

   st = pComp->pApplicationPrivate;
   comp = (VC_PRIVATE_COMPONENT_T *) pComp->pComponentPrivate;

   exe.reference = comp->reference;

   if(ilcs_execute_function(st->ilcs, IL_GET_COMPONENT_VERSION, &exe, sizeof(exe), &resp, &rlen) < 0 || rlen != sizeof(resp))
      return OMX_ErrorHardware;

   strncpy(pComponentName, resp.name, 128);
   pComponentName[127] = 0;
   *pComponentVersion = resp.component_version;
   *pSpecVersion = resp.spec_version;
   memcpy(pComponentUUID, resp.uuid, sizeof(OMX_UUIDTYPE));

   return resp.err;
}

static OMX_ERRORTYPE vcil_out_SetCallbacks(OMX_IN  OMX_HANDLETYPE hComponent,
      OMX_IN  OMX_CALLBACKTYPE* pCallbacks,
      OMX_IN  OMX_PTR pAppData)
{
   OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *) hComponent;
   VC_PRIVATE_COMPONENT_T *comp;
   IL_SET_CALLBACKS_EXECUTE_T exe;
   IL_RESPONSE_HEADER_T resp;
   ILCS_COMMON_T *st;
   int rlen = sizeof(resp);

   if(!(pComp && pCallbacks))
      return OMX_ErrorBadParameter;

   st = pComp->pApplicationPrivate;
   comp = (VC_PRIVATE_COMPONENT_T *) pComp->pComponentPrivate;

   comp->callbacks = *pCallbacks;
   comp->callback_state = pAppData;

   exe.reference = comp->reference;
   exe.pAppData = pComp;

   if(ilcs_execute_function(st->ilcs, IL_SET_CALLBACKS, &exe, sizeof(exe), &resp, &rlen) < 0 || rlen != sizeof(resp))
      return OMX_ErrorHardware;

   return resp.err;
}

static OMX_ERRORTYPE vcil_out_GetState(OMX_IN  OMX_HANDLETYPE hComponent,
                                       OMX_OUT OMX_STATETYPE* pState)
{
   OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *) hComponent;
   VC_PRIVATE_COMPONENT_T *comp;
   IL_EXECUTE_HEADER_T exe;
   IL_GET_STATE_RESPONSE_T resp;
   ILCS_COMMON_T *st;
   int rlen = sizeof(resp);

   if (!(pComp && pState))
      return OMX_ErrorBadParameter;

   st = pComp->pApplicationPrivate;
   comp = (VC_PRIVATE_COMPONENT_T *) pComp->pComponentPrivate;

   exe.reference = comp->reference;

   if(ilcs_execute_function(st->ilcs, IL_GET_STATE, &exe, sizeof(exe), &resp, &rlen) < 0 || rlen != sizeof(resp))
      return OMX_ErrorHardware;

   *pState = resp.state;

   return resp.err;
}

static OMX_ERRORTYPE vcil_out_get(OMX_IN  OMX_HANDLETYPE hComponent,
                                  OMX_IN  OMX_INDEXTYPE nParamIndex,
                                  OMX_INOUT OMX_PTR pComponentParameterStructure,
                                  IL_FUNCTION_T func)
{
   OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *) hComponent;
   VC_PRIVATE_COMPONENT_T *comp;
   IL_GET_EXECUTE_T exe;
   IL_GET_RESPONSE_T resp;
   OMX_U32 size;
   ILCS_COMMON_T *st;
   int rlen = sizeof(resp);

   if (!(pComp && pComponentParameterStructure))
      return OMX_ErrorBadParameter;

   st = pComp->pApplicationPrivate;
   comp = (VC_PRIVATE_COMPONENT_T *) pComp->pComponentPrivate;

   exe.reference = comp->reference;
   exe.index = nParamIndex;

   size = *((OMX_U32 *) pComponentParameterStructure);

   if(size > VC_ILCS_MAX_PARAM_SIZE)
      return OMX_ErrorHardware;

   memcpy(exe.param, pComponentParameterStructure, size);

   if(ilcs_execute_function(st->ilcs, func, &exe, size + IL_GET_EXECUTE_HEADER_SIZE, &resp, &rlen) < 0 || rlen > sizeof(resp))
      return OMX_ErrorHardware;

   memcpy(pComponentParameterStructure, resp.param, size);

   return resp.err;
}

static OMX_ERRORTYPE vcil_out_set(OMX_IN  OMX_HANDLETYPE hComponent,
                                  OMX_IN  OMX_INDEXTYPE nParamIndex,
                                  OMX_IN OMX_PTR pComponentParameterStructure,
                                  IL_FUNCTION_T func)
{
   OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *) hComponent;
   VC_PRIVATE_COMPONENT_T *comp;
   IL_SET_EXECUTE_T exe;
   IL_RESPONSE_HEADER_T resp;
   OMX_U32 size;
   ILCS_COMMON_T *st;
   int rlen = sizeof(resp);

   if (!(pComp && pComponentParameterStructure))
      return OMX_ErrorBadParameter;

   st = pComp->pApplicationPrivate;
   comp = (VC_PRIVATE_COMPONENT_T *) pComp->pComponentPrivate;

   exe.reference = comp->reference;
   exe.index = nParamIndex;

   size = *((OMX_U32 *) pComponentParameterStructure);

   if(size > VC_ILCS_MAX_PARAM_SIZE)
      return OMX_ErrorHardware;

   memcpy(exe.param, pComponentParameterStructure, size);

   if(ilcs_execute_function(st->ilcs, func, &exe, size + IL_SET_EXECUTE_HEADER_SIZE, &resp, &rlen) < 0 || rlen != sizeof(resp))
      return OMX_ErrorHardware;

   return resp.err;
}

static OMX_ERRORTYPE vcil_out_GetParameter(OMX_IN  OMX_HANDLETYPE hComponent,
      OMX_IN  OMX_INDEXTYPE nParamIndex,
      OMX_INOUT OMX_PTR pComponentParameterStructure)
{
   return vcil_out_get(hComponent, nParamIndex, pComponentParameterStructure, IL_GET_PARAMETER);
}

static OMX_ERRORTYPE vcil_out_SetParameter(OMX_IN  OMX_HANDLETYPE hComponent,
      OMX_IN  OMX_INDEXTYPE nParamIndex,
      OMX_IN OMX_PTR pComponentParameterStructure)
{
   return vcil_out_set(hComponent, nParamIndex, pComponentParameterStructure, IL_SET_PARAMETER);
}

static OMX_ERRORTYPE vcil_out_GetConfig(OMX_IN  OMX_HANDLETYPE hComponent,
                                        OMX_IN  OMX_INDEXTYPE nParamIndex,
                                        OMX_INOUT OMX_PTR pComponentParameterStructure)
{
   return vcil_out_get(hComponent, nParamIndex, pComponentParameterStructure, IL_GET_CONFIG);
}

static OMX_ERRORTYPE vcil_out_SetConfig(OMX_IN  OMX_HANDLETYPE hComponent,
                                        OMX_IN  OMX_INDEXTYPE nParamIndex,
                                        OMX_IN OMX_PTR pComponentParameterStructure)
{
   return vcil_out_set(hComponent, nParamIndex, pComponentParameterStructure, IL_SET_CONFIG);
}

static OMX_ERRORTYPE vcil_out_SendCommand(OMX_IN  OMX_HANDLETYPE hComponent,
      OMX_IN  OMX_COMMANDTYPE Cmd,
      OMX_IN  OMX_U32 nParam1,
      OMX_IN  OMX_PTR pCmdData)
{
   OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *) hComponent;
   VC_PRIVATE_COMPONENT_T *comp;
   IL_SEND_COMMAND_EXECUTE_T exe;
   IL_RESPONSE_HEADER_T resp;
   ILCS_COMMON_T *st;
   int rlen = sizeof(resp);

   if (!pComp)
      return OMX_ErrorBadParameter;

   st = pComp->pApplicationPrivate;
   comp = (VC_PRIVATE_COMPONENT_T *) pComp->pComponentPrivate;

   exe.reference = comp->reference;
   exe.cmd = Cmd;
   exe.param = nParam1;

   if (Cmd == OMX_CommandMarkBuffer)
   {
      exe.mark = *((OMX_MARKTYPE *) pCmdData);
   }
   else
   {
      exe.mark.hMarkTargetComponent = 0;
      exe.mark.pMarkData = 0;
   }

   if(ilcs_execute_function(st->ilcs, IL_SEND_COMMAND, &exe, sizeof(exe), &resp, &rlen) < 0 || rlen != sizeof(resp))
      return OMX_ErrorHardware;

   return resp.err;
}

// Called to pass a buffer from the host-side across the interface to videcore.

static OMX_ERRORTYPE vcil_out_addBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                        OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
                                        OMX_IN OMX_U32 nPortIndex,
                                        OMX_IN OMX_PTR pAppPrivate,
                                        OMX_IN OMX_U32 nSizeBytes,
                                        OMX_IN OMX_U8* pBuffer,
                                        OMX_IN void *eglImage,
                                        IL_FUNCTION_T func)
{
   OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *) hComponent;
   VC_PRIVATE_COMPONENT_T *comp;
   IL_ADD_BUFFER_EXECUTE_T exe;
   IL_ADD_BUFFER_RESPONSE_T resp;
   OMX_BUFFERHEADERTYPE *pHeader;
   VC_PRIVATE_PORT_T *port;
   ILCS_COMMON_T *st;
   int rlen = sizeof(resp);

   if (!(pComp && ppBufferHdr))
      return OMX_ErrorBadParameter;

   st = pComp->pApplicationPrivate;
   comp = (VC_PRIVATE_COMPONENT_T *) pComp->pComponentPrivate;

   port = find_port(comp, nPortIndex);
   if (!port) // bad port index
      return OMX_ErrorBadPortIndex;

   if (port->numBuffers > 0 && port->func != func)
   {
      // inconsistent use of usebuffer/allocatebuffer/eglimage
      // all ports must receive all buffers by exactly one of these methods
      vc_assert(port->func != func);
      return OMX_ErrorInsufficientResources;
   }
   port->func = func;

   if (!VCHI_BULK_ALIGNED(pBuffer))
   {
      // cannot transfer this buffer across the host interface
      return OMX_ErrorBadParameter;
   }

   pHeader = vcos_malloc(sizeof(*pHeader), "vcout buffer header");

   if (!pHeader)
      return OMX_ErrorInsufficientResources;

   if (func == IL_ALLOCATE_BUFFER)
   {
      pBuffer = vcos_malloc_aligned(nSizeBytes, ILCS_ALIGN, "vcout mapping buffer");
      if (!pBuffer)
      {
         vcos_free(pHeader);
         return OMX_ErrorInsufficientResources;
      }
   }

   exe.reference = comp->reference;
   exe.bufferReference = pHeader;
   exe.port = nPortIndex;
   exe.size = nSizeBytes;
   exe.eglImage = eglImage;

   if(ilcs_execute_function(st->ilcs, func, &exe, sizeof(exe), &resp, &rlen) < 0 || rlen != sizeof(resp))
      resp.err = OMX_ErrorHardware;

   if (resp.err == OMX_ErrorNone)
   {
      memcpy(pHeader, &resp.bufferHeader, sizeof(OMX_BUFFERHEADERTYPE));
      if (port->dir == OMX_DirOutput)
         pHeader->pOutputPortPrivate = resp.reference;
      else
         pHeader->pInputPortPrivate = resp.reference;

      if (func == IL_USE_EGL_IMAGE)
      {
         pHeader->pBuffer = (OMX_U8*)eglImage;
         port->bEGL = OMX_TRUE;
      }         
      else
      {
         pHeader->pBuffer = pBuffer;
         port->bEGL = OMX_FALSE;
      }

      pHeader->pAppPrivate = pAppPrivate;
      *ppBufferHdr = pHeader;
      port->numBuffers++;
   }
   else
   {
      if (func == IL_ALLOCATE_BUFFER)
         vcos_free(pBuffer);
      vcos_free(pHeader);
   }

   return resp.err;
}

static VCOS_ONCE_T loaded_eglIntOpenMAXILDoneMarker = VCOS_ONCE_INIT;
static int (*local_eglIntOpenMAXILDoneMarker) (void* component_handle, void *egl_image) = NULL;

static void load_eglIntOpenMAXILDoneMarker(void)
{
   void *handle;

   /* First try to load from the current process, this will succeed
    * if something that is linked to libEGL is already loaded or
    * something explicitly loaded libEGL with RTLD_GLOBAL
    */
   handle = vcos_dlopen(NULL, VCOS_DL_GLOBAL);
   local_eglIntOpenMAXILDoneMarker = (void * )vcos_dlsym(handle, "eglIntOpenMAXILDoneMarker");
   if (local_eglIntOpenMAXILDoneMarker == NULL)
   {
      vcos_dlclose(handle);
      /* If that failed try to load libEGL.so explicitely */
      handle = vcos_dlopen("libEGL.so", VCOS_DL_LAZY | VCOS_DL_LOCAL);
      vc_assert(handle != NULL);
      local_eglIntOpenMAXILDoneMarker = (void * )vcos_dlsym(handle, "eglIntOpenMAXILDoneMarker");
      vc_assert(local_eglIntOpenMAXILDoneMarker != NULL);
   }
}

static OMX_ERRORTYPE vcil_out_UseEGLImage(OMX_IN OMX_HANDLETYPE hComponent,
      OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
      OMX_IN OMX_U32 nPortIndex,
      OMX_IN OMX_PTR pAppPrivate,
      OMX_IN void* eglImage)
{
   /* Load eglIntOpenMAXILDoneMarker() and libEGL here, it will be needed later */
   vcos_once(&loaded_eglIntOpenMAXILDoneMarker, load_eglIntOpenMAXILDoneMarker);

   return vcil_out_addBuffer(hComponent, ppBufferHdr, nPortIndex, pAppPrivate, 0, NULL, eglImage, IL_USE_EGL_IMAGE);
}

static OMX_ERRORTYPE vcil_out_UseBuffer(OMX_IN OMX_HANDLETYPE hComponent,
                                        OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
                                        OMX_IN OMX_U32 nPortIndex,
                                        OMX_IN OMX_PTR pAppPrivate,
                                        OMX_IN OMX_U32 nSizeBytes,
                                        OMX_IN OMX_U8* pBuffer)
{
   return vcil_out_addBuffer(hComponent, ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes, pBuffer, NULL, IL_USE_BUFFER);
}

static OMX_ERRORTYPE vcil_out_AllocateBuffer(OMX_IN OMX_HANDLETYPE hComponent,
      OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
      OMX_IN OMX_U32 nPortIndex,
      OMX_IN OMX_PTR pAppPrivate,
      OMX_IN OMX_U32 nSizeBytes)
{
   return vcil_out_addBuffer(hComponent, ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes, NULL, NULL, IL_ALLOCATE_BUFFER);
}

static OMX_ERRORTYPE vcil_out_FreeBuffer(OMX_IN  OMX_HANDLETYPE hComponent,
      OMX_IN  OMX_U32 nPortIndex,
      OMX_IN  OMX_BUFFERHEADERTYPE* pBufferHdr)
{
   OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *) hComponent;
   VC_PRIVATE_COMPONENT_T *comp;
   IL_FREE_BUFFER_EXECUTE_T exe;
   IL_RESPONSE_HEADER_T resp;
   VC_PRIVATE_PORT_T *port;
   ILCS_COMMON_T *st;
   int rlen = sizeof(resp);

   if (!(pComp && pBufferHdr))
      return OMX_ErrorBadParameter;

   st = pComp->pApplicationPrivate;
   comp = (VC_PRIVATE_COMPONENT_T *) pComp->pComponentPrivate;

   port = find_port(comp, nPortIndex);
   if (!port)
      return OMX_ErrorBadPortIndex;

   if (port->numBuffers == 0)
      return OMX_ErrorIncorrectStateTransition;

   exe.reference = comp->reference;
   exe.port = nPortIndex;
   if (port->dir == OMX_DirOutput)
      exe.bufferReference = pBufferHdr->pOutputPortPrivate;
   else
      exe.bufferReference = pBufferHdr->pInputPortPrivate;
   exe.func = port->func;
   exe.inputPrivate = NULL;
   exe.outputPrivate = NULL;

   if(ilcs_execute_function(st->ilcs, IL_FREE_BUFFER, &exe, sizeof(exe), &resp, &rlen) < 0 || rlen != sizeof(resp))
      return OMX_ErrorHardware;

   if (resp.err == OMX_ErrorNone)
   {
      if (port->func == IL_ALLOCATE_BUFFER)
         vcos_free(pBufferHdr->pBuffer);
      vcos_free(pBufferHdr);
      port->numBuffers--;
   }

   return resp.err;
}

// Called on host-side to pass a buffer to VideoCore to be emptied
static OMX_ERRORTYPE vcil_out_EmptyThisBuffer(OMX_IN  OMX_HANDLETYPE hComponent,
      OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
   OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *) hComponent;
   VC_PRIVATE_COMPONENT_T *comp;
   ILCS_COMMON_T *st;
 
   if (!(pComp && pBuffer))
      return (OMX_ErrorBadParameter);

   st = pComp->pApplicationPrivate;
   comp = (VC_PRIVATE_COMPONENT_T *) pComp->pComponentPrivate;
   
   return ilcs_pass_buffer(st->ilcs, IL_EMPTY_THIS_BUFFER, comp->reference, pBuffer);
}

// Called from ril_top as OMX_FillThisBuffer().
// ->pBuffer field is expected to be a memory handle.

static OMX_ERRORTYPE vcil_out_FillThisBuffer(OMX_IN  OMX_HANDLETYPE hComponent,
      OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
   OMX_ERRORTYPE err;
   OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *) hComponent;
   VC_PRIVATE_COMPONENT_T *comp;
   VC_PRIVATE_PORT_T *port;
   ILCS_COMMON_T *st;

   if (!(pComp && pBuffer))
      return (OMX_ErrorBadParameter);

   st = pComp->pApplicationPrivate;
   comp = (VC_PRIVATE_COMPONENT_T *) pComp->pComponentPrivate;

   port = find_port(comp, pBuffer->nOutputPortIndex);
   if(!port)
      return OMX_ErrorBadPortIndex;

   if(pBuffer->pBuffer == 0)
      return OMX_ErrorIncorrectStateOperation;

   vcos_assert(pComp != NULL && comp != NULL && port != NULL && st != NULL);
   
   // The lower layers will attempt to transfer the bytes specified if we don't
   // clear these - callers should ideally do this themselves, but it is not
   // mandated in the specification.
   pBuffer->nFilledLen = 0;
   pBuffer->nFlags = 0;

   vc_assert(port->bEGL == OMX_TRUE || is_valid_hostside_buffer(pBuffer));  

   err = ilcs_pass_buffer(st->ilcs, IL_FILL_THIS_BUFFER, comp->reference, pBuffer);
   
   if (err == OMX_ErrorNone && port->bEGL == OMX_TRUE)
   {
      // If an output port is marked as an EGL port, we request EGL to notify the IL component 
      // when it's allowed to render into the buffer/EGLImage.
      vc_assert(local_eglIntOpenMAXILDoneMarker != NULL);
      local_eglIntOpenMAXILDoneMarker(comp->reference, pBuffer->pBuffer);
   }      

   return err;
}

static OMX_ERRORTYPE vcil_out_ComponentTunnelRequest(OMX_IN  OMX_HANDLETYPE hComponent,
      OMX_IN  OMX_U32 nPort,
      OMX_IN  OMX_HANDLETYPE hTunneledComp,
      OMX_IN  OMX_U32 nTunneledPort,
      OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
   OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *) hComponent;
   VC_PRIVATE_COMPONENT_T *comp;
   IL_TUNNEL_REQUEST_EXECUTE_T exe;
   IL_TUNNEL_REQUEST_RESPONSE_T resp;
   VC_PRIVATE_COMPONENT_T *list;
   ILCS_COMMON_T *st;
   int rlen = sizeof(resp);

   if(!pComp)
      return OMX_ErrorBadParameter;

   st = pComp->pApplicationPrivate;
   comp = (VC_PRIVATE_COMPONENT_T *) pComp->pComponentPrivate;

   exe.reference = comp->reference;
   exe.port = nPort;
   exe.tunnel_port = nTunneledPort;
   if (pTunnelSetup)
      exe.setup = *pTunnelSetup;

   // the other component may be on the host or on VC.  Look through our list
   // so we can tell, and tell ILCS on VC the details.
   vcos_semaphore_wait(&st->component_lock);

   list = st->component_list;
   while (list != NULL && list->comp != (void *) hTunneledComp)
      list = list->next;

   vcos_semaphore_post(&st->component_lock);

   if (list == NULL)
   {
      exe.tunnel_ref = hTunneledComp;
      exe.tunnel_host = OMX_TRUE;
   }
   else
   {
      exe.tunnel_ref = list->reference;
      exe.tunnel_host = OMX_FALSE;
   }

   if(ilcs_execute_function(st->ilcs, IL_COMPONENT_TUNNEL_REQUEST, &exe, sizeof(exe), &resp, &rlen) < 0 || rlen != sizeof(resp))
      return OMX_ErrorHardware;

   if (pTunnelSetup)
      *pTunnelSetup = resp.setup;
   return resp.err;
}

static OMX_ERRORTYPE vcil_out_GetExtensionIndex(OMX_IN  OMX_HANDLETYPE hComponent,
      OMX_IN  OMX_STRING cParameterName,
      OMX_OUT OMX_INDEXTYPE* pIndexType)
{
   OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *) hComponent;
   VC_PRIVATE_COMPONENT_T *comp;
   IL_GET_EXTENSION_EXECUTE_T exe;
   IL_GET_EXTENSION_RESPONSE_T resp;
   ILCS_COMMON_T *st;
   int rlen = sizeof(resp);

   if (!(pComp && cParameterName && pIndexType))
      return OMX_ErrorBadParameter;

   st = pComp->pApplicationPrivate;
   comp = (VC_PRIVATE_COMPONENT_T *) pComp->pComponentPrivate;

   exe.reference = comp->reference;
   strncpy(exe.name, cParameterName, 128);
   exe.name[127] = 0;

   if(ilcs_execute_function(st->ilcs, IL_GET_EXTENSION_INDEX, &exe, sizeof(exe), &resp, &rlen) < 0 || rlen != sizeof(resp))
      return OMX_ErrorHardware;

   *pIndexType = resp.index;
   return resp.err;
}

static OMX_ERRORTYPE vcil_out_ComponentRoleEnum(OMX_IN OMX_HANDLETYPE hComponent,
      OMX_OUT OMX_U8 *cRole,
      OMX_IN OMX_U32 nIndex)
{
   OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *) hComponent;
   VC_PRIVATE_COMPONENT_T *comp = (VC_PRIVATE_COMPONENT_T *) pComp->pComponentPrivate;
   IL_COMPONENT_ROLE_ENUM_EXECUTE_T exe;
   IL_COMPONENT_ROLE_ENUM_RESPONSE_T resp;
   ILCS_COMMON_T *st = pComp->pApplicationPrivate;
   int rlen = sizeof(resp);

   exe.reference = comp->reference;
   exe.index = nIndex;

   if(ilcs_execute_function(st->ilcs, IL_COMPONENT_ROLE_ENUM, &exe, sizeof(exe), &resp, &rlen) < 0 || rlen != sizeof(resp))
      return OMX_ErrorHardware;

   strncpy((char *) cRole, (char *) resp.role, 128);
   cRole[127] = 0;
   return resp.err;
}

OMX_ERRORTYPE vcil_out_component_name_enum(ILCS_COMMON_T *st, OMX_STRING cComponentName, OMX_U32 nNameLength, OMX_U32 nIndex)
{
   IL_COMPONENT_NAME_ENUM_EXECUTE_T exe;
   IL_COMPONENT_NAME_ENUM_RESPONSE_T resp;
   int rlen = sizeof(resp);

   exe.index = nIndex;

   if(ilcs_execute_function(st->ilcs, IL_COMPONENT_NAME_ENUM, &exe, sizeof(exe), &resp, &rlen) < 0 || rlen != sizeof(resp))
      return OMX_ErrorHardware;

   if (sizeof(resp.name) < nNameLength)
      nNameLength = sizeof(resp.name);

   strncpy((char *)cComponentName, (char *) resp.name, nNameLength);
   cComponentName[127] = 0;
   return resp.err;
}

OMX_ERRORTYPE vcil_out_get_debug_information(ILCS_COMMON_T *st, OMX_STRING debugInfo, OMX_S32 *pLen)
{
   IL_GET_DEBUG_INFORMATION_EXECUTE_T exe;

   exe.len = *pLen;

   if(ilcs_execute_function(st->ilcs, IL_GET_DEBUG_INFORMATION, &exe, sizeof(exe), debugInfo, (int *) pLen) < 0)
      return OMX_ErrorHardware;

   return OMX_ErrorNone;
}

// Called on the host side to create an OMX component.
OMX_ERRORTYPE vcil_out_create_component(ILCS_COMMON_T *st, OMX_HANDLETYPE hComponent, OMX_STRING component_name)
{
   OMX_COMPONENTTYPE *pComp = (OMX_COMPONENTTYPE *) hComponent;
   IL_CREATE_COMPONENT_EXECUTE_T exe;
   IL_CREATE_COMPONENT_RESPONSE_T resp;
   VC_PRIVATE_COMPONENT_T *comp;
   OMX_U32 i;
   int rlen = sizeof(resp);

   if (strlen(component_name) >= sizeof(exe.name))
      return OMX_ErrorInvalidComponent;

   strcpy(exe.name, component_name);
   exe.mark = pComp;

   if(ilcs_execute_function(st->ilcs, IL_CREATE_COMPONENT, &exe, sizeof(exe), &resp, &rlen) < 0 || rlen != sizeof(resp))
      return OMX_ErrorHardware;

   if (resp.err != OMX_ErrorNone)
      return resp.err;

   comp = vcos_malloc(sizeof(VC_PRIVATE_COMPONENT_T) + (sizeof(VC_PRIVATE_PORT_T) * resp.numPorts), "ILCS Host Comp");
   if (!comp)
   {
      IL_EXECUTE_HEADER_T dexe;
      IL_RESPONSE_HEADER_T dresp;
      int dlen = sizeof(dresp);

      dexe.reference = resp.reference;

      ilcs_execute_function(st->ilcs, IL_COMPONENT_DEINIT, &dexe, sizeof(dexe), &dresp, &dlen);
      return OMX_ErrorInsufficientResources;
   }

   memset(comp, 0, sizeof(VC_PRIVATE_COMPONENT_T) + (sizeof(VC_PRIVATE_PORT_T) * resp.numPorts));

   comp->reference = resp.reference;
   comp->comp = pComp;
   comp->numPorts = resp.numPorts;
   comp->port = (VC_PRIVATE_PORT_T *) ((unsigned char *) comp + sizeof(VC_PRIVATE_COMPONENT_T));

   for (i=0; i<comp->numPorts; i++)
   {
      if (i && !(i&0x1f))
      {
         IL_GET_EXECUTE_T gexe;
         IL_GET_RESPONSE_T gresp;
         OMX_PARAM_PORTSUMMARYTYPE *summary;
         int glen = sizeof(gresp);

         gexe.reference = comp->reference;
         gexe.index = OMX_IndexParamPortSummary;

         summary = (OMX_PARAM_PORTSUMMARYTYPE *) &gexe.param;
         summary->nSize = sizeof(OMX_PARAM_PORTSUMMARYTYPE);
         summary->nVersion.nVersion = OMX_VERSION;
         summary->reqSet = i>>5;

         ilcs_execute_function(st->ilcs, IL_GET_PARAMETER, &gexe,
                               sizeof(OMX_PARAM_PORTSUMMARYTYPE)+IL_GET_EXECUTE_HEADER_SIZE,
                               &gresp, &glen);

         summary = (OMX_PARAM_PORTSUMMARYTYPE *) &gresp.param;
         resp.portDir = summary->portDir;
         memcpy(resp.portIndex, summary->portIndex, sizeof(OMX_U32) * 32);
      }

      comp->port[i].port = resp.portIndex[i&0x1f];
      comp->port[i].dir = ((resp.portDir >> (i&0x1f)) & 1) ? OMX_DirOutput : OMX_DirInput;
   }

   vcos_semaphore_wait(&st->component_lock);
   // insert into head of list
   comp->next = st->component_list;
   st->component_list = comp;
   vcos_semaphore_post(&st->component_lock);

   pComp->pComponentPrivate = comp;
   pComp->pApplicationPrivate = st;

   pComp->GetComponentVersion = vcil_out_GetComponentVersion;
   pComp->ComponentDeInit = vcil_out_ComponentDeInit;
   pComp->SetCallbacks = vcil_out_SetCallbacks;
   pComp->GetState = vcil_out_GetState;
   pComp->GetParameter = vcil_out_GetParameter;
   pComp->SetParameter = vcil_out_SetParameter;
   pComp->GetConfig = vcil_out_GetConfig;
   pComp->SetConfig = vcil_out_SetConfig;
   pComp->SendCommand = vcil_out_SendCommand;
   pComp->UseBuffer = vcil_out_UseBuffer;
   pComp->AllocateBuffer = vcil_out_AllocateBuffer;
   pComp->FreeBuffer = vcil_out_FreeBuffer;
   pComp->EmptyThisBuffer = vcil_out_EmptyThisBuffer;
   pComp->FillThisBuffer = vcil_out_FillThisBuffer;
   pComp->ComponentTunnelRequest = vcil_out_ComponentTunnelRequest;
   pComp->GetExtensionIndex = vcil_out_GetExtensionIndex;
   pComp->UseEGLImage = vcil_out_UseEGLImage;
   pComp->ComponentRoleEnum = vcil_out_ComponentRoleEnum;

   return resp.err;
}

/* callbacks */

void vcil_out_event_handler(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen)
{
   IL_EVENT_HANDLER_EXECUTE_T *exe = call;
   OMX_COMPONENTTYPE *pComp = exe->reference;
   VC_PRIVATE_COMPONENT_T *comp = (VC_PRIVATE_COMPONENT_T *) pComp->pComponentPrivate;

   *rlen = 0;

   vcos_assert(comp->callbacks.EventHandler);
   comp->callbacks.EventHandler(pComp, comp->callback_state, exe->event, exe->data1, exe->data2, exe->eventdata);
}

// Called on host side via RPC in response to empty buffer completing
void vcil_out_empty_buffer_done(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen)
{
   IL_PASS_BUFFER_EXECUTE_T *exe = call;
   OMX_COMPONENTTYPE *pComp = exe->reference;
   VC_PRIVATE_COMPONENT_T *comp = (VC_PRIVATE_COMPONENT_T *) pComp->pComponentPrivate;
   OMX_BUFFERHEADERTYPE *pHeader = exe->bufferHeader.pOutputPortPrivate;
   OMX_U8 *pBuffer = pHeader->pBuffer;
   OMX_PTR *pAppPrivate = pHeader->pAppPrivate;
   OMX_PTR *pPlatformPrivate = pHeader->pPlatformPrivate;
   OMX_PTR *pInputPortPrivate = pHeader->pInputPortPrivate;
   OMX_PTR *pOutputPortPrivate = pHeader->pOutputPortPrivate;

   memcpy(pHeader, &exe->bufferHeader, sizeof(OMX_BUFFERHEADERTYPE));

   pHeader->pBuffer = pBuffer;
   pHeader->pAppPrivate = pAppPrivate;
   pHeader->pPlatformPrivate = pPlatformPrivate;
   pHeader->pInputPortPrivate = pInputPortPrivate;
   pHeader->pOutputPortPrivate = pOutputPortPrivate;

   *rlen = 0;

   vcos_assert(comp->callbacks.EmptyBufferDone);
   comp->callbacks.EmptyBufferDone(pComp, comp->callback_state, pHeader);
}

// Called on host side via RPC in response to a fill-buffer completing
// on the VideoCore side. ->pBuffer is a real pointer.
void vcil_out_fill_buffer_done(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen)
{
   OMX_COMPONENTTYPE *pComp;
   VC_PRIVATE_COMPONENT_T *comp;
   OMX_BUFFERHEADERTYPE *pHeader;

   pHeader = ilcs_receive_buffer(st->ilcs, call, clen, &pComp);
   *rlen = 0;

   if(pHeader)
   {
      comp = (VC_PRIVATE_COMPONENT_T *) pComp->pComponentPrivate;

      vc_assert(comp->callbacks.FillBufferDone);
      comp->callbacks.FillBufferDone(pComp, comp->callback_state, pHeader);
   }
}
