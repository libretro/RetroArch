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

#include "interface/vmcs_host/khronos/IL/OMX_Component.h"

#include "interface/vmcs_host/vc_ilcs_defs.h"
#include "interface/vmcs_host/vcilcs.h"
#include "interface/vmcs_host/vcilcs_common.h"

#ifndef NDEBUG
static int is_valid_hostside_buffer(OMX_BUFFERHEADERTYPE *pBuf)
{
   if (!pBuf->pBuffer)
      return 0;
   if ((unsigned long)pBuf->pBuffer < 0x100)
      return 0; // not believable
   return 1;
}
#endif

void vcil_in_get_state(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen)
{
   IL_EXECUTE_HEADER_T *exe = call;
   IL_GET_STATE_RESPONSE_T *ret = resp;
   OMX_COMPONENTTYPE *pComp = exe->reference;

   *rlen = sizeof(IL_GET_STATE_RESPONSE_T);
   ret->func = IL_GET_STATE;
   ret->err = pComp->GetState(pComp, &ret->state);
}

void vcil_in_get_parameter(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen)
{
   IL_GET_EXECUTE_T *exe = call;
   IL_GET_RESPONSE_T *ret = resp;
   OMX_COMPONENTTYPE *pComp  = exe->reference;
   OMX_U32 size = *((OMX_U32 *) (&exe->param));

   ret->func = IL_GET_PARAMETER;

   if(size > VC_ILCS_MAX_PARAM_SIZE)
   {
      *rlen = IL_GET_RESPONSE_HEADER_SIZE;
      ret->err = OMX_ErrorHardware;
   }
   else
   {
      *rlen = size + IL_GET_RESPONSE_HEADER_SIZE;
      ret->err = pComp->GetParameter(pComp, exe->index, exe->param);
      memcpy(ret->param, exe->param, size);
   }
}

void vcil_in_set_parameter(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen)
{
   IL_SET_EXECUTE_T *exe = call;
   IL_RESPONSE_HEADER_T *ret = resp;
   OMX_COMPONENTTYPE *pComp  = exe->reference;

   *rlen = sizeof(IL_RESPONSE_HEADER_T);
   ret->func = IL_SET_PARAMETER;
   ret->err = pComp->SetParameter(pComp, exe->index, exe->param);
}

void vcil_in_get_config(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen)
{
   IL_GET_EXECUTE_T *exe = call;
   IL_GET_RESPONSE_T *ret = resp;
   OMX_COMPONENTTYPE *pComp  = exe->reference;
   OMX_U32 size = *((OMX_U32 *) (&exe->param));

   ret->func = IL_GET_CONFIG;

   if(size > VC_ILCS_MAX_PARAM_SIZE)
   {
      *rlen = IL_GET_RESPONSE_HEADER_SIZE;
      ret->err = OMX_ErrorHardware;
   }
   else
   {
      *rlen = size + IL_GET_RESPONSE_HEADER_SIZE;
      ret->func = IL_GET_CONFIG;
      ret->err = pComp->GetConfig(pComp, exe->index, exe->param);
      memcpy(ret->param, exe->param, size);
   }
}

void vcil_in_set_config(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen)
{
   IL_SET_EXECUTE_T *exe = call;
   IL_RESPONSE_HEADER_T *ret = resp;
   OMX_COMPONENTTYPE *pComp  = exe->reference;

   *rlen = sizeof(IL_RESPONSE_HEADER_T);
   ret->func = IL_SET_CONFIG;
   ret->err = pComp->SetConfig(pComp, exe->index, exe->param);
}

void vcil_in_use_buffer(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen)
{
   IL_ADD_BUFFER_EXECUTE_T *exe = call;
   IL_ADD_BUFFER_RESPONSE_T *ret = resp;
   OMX_COMPONENTTYPE *pComp = exe->reference;
   OMX_U8 *buffer;
   OMX_BUFFERHEADERTYPE *bufferHeader;

   *rlen = sizeof(IL_ADD_BUFFER_RESPONSE_T);

   buffer = vcos_malloc_aligned(exe->size, 32, "vcin mapping buffer"); // 32-byte aligned
   if (!buffer)
   {
      ret->err = OMX_ErrorInsufficientResources;
      return;
   }

   //OMX_OSAL_Trace(OMX_OSAL_TRACE_COMPONENT, "hostcomp: use buffer(%p)\n", buffer);
   ret->func = IL_USE_BUFFER;
   ret->err = pComp->UseBuffer(pComp, &bufferHeader, exe->port, exe->bufferReference, exe->size, buffer);

   if (ret->err == OMX_ErrorNone)
   {
      // we're going to pass this buffer to VC
      // initialise our private field in their copy with the host buffer reference
      OMX_PARAM_PORTDEFINITIONTYPE def;
      OMX_ERRORTYPE error;
      def.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
      def.nVersion.nVersion = OMX_VERSION;
      def.nPortIndex = exe->port;
      error = pComp->GetParameter(pComp, OMX_IndexParamPortDefinition, &def);
      vc_assert(error == OMX_ErrorNone);

      ret->reference = bufferHeader;
      memcpy(&ret->bufferHeader, bufferHeader, sizeof(OMX_BUFFERHEADERTYPE));

      if (def.eDir == OMX_DirInput)
         ret->bufferHeader.pInputPortPrivate = bufferHeader;
      else
         ret->bufferHeader.pOutputPortPrivate = bufferHeader;
   }
   else
      vcos_free(buffer);
}

void vcil_in_free_buffer(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen)
{
   IL_FREE_BUFFER_EXECUTE_T *exe = call;
   IL_RESPONSE_HEADER_T *ret = resp;
   OMX_COMPONENTTYPE *pComp = exe->reference;
   OMX_BUFFERHEADERTYPE *pHeader;
   OMX_U8 *buffer;
   OMX_PARAM_PORTDEFINITIONTYPE def;
   OMX_ERRORTYPE error;

   def.nSize = sizeof(OMX_PARAM_PORTDEFINITIONTYPE);
   def.nVersion.nVersion = OMX_VERSION;
   def.nPortIndex = exe->port;
   error = pComp->GetParameter(pComp, OMX_IndexParamPortDefinition, &def);
   vc_assert(error == OMX_ErrorNone);
   if (def.eDir == OMX_DirInput)
      pHeader = exe->inputPrivate;
   else
      pHeader = exe->outputPrivate;

   buffer = pHeader->pBuffer;

   *rlen = sizeof(IL_RESPONSE_HEADER_T);
   ret->func = IL_FREE_BUFFER;
   ret->err = pComp->FreeBuffer(pComp, exe->port, pHeader);
   if (ret->err == OMX_ErrorNone)
      vcos_free(buffer);
}

void vcil_in_empty_this_buffer(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen)
{
   IL_RESPONSE_HEADER_T *ret = resp;
   OMX_COMPONENTTYPE *pComp;
   OMX_BUFFERHEADERTYPE *pHeader;

   pHeader = ilcs_receive_buffer(st->ilcs, call, clen, &pComp);

   *rlen = sizeof(IL_RESPONSE_HEADER_T);
   ret->func = IL_EMPTY_THIS_BUFFER;

   if(pHeader)
   {
      vc_assert(is_valid_hostside_buffer(pHeader));
      ret->err = pComp->EmptyThisBuffer(pComp, pHeader);
   }
   else
      ret->err = OMX_ErrorHardware;
}

void vcil_in_fill_this_buffer(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen)
{
   IL_PASS_BUFFER_EXECUTE_T *exe = call;
   IL_RESPONSE_HEADER_T *ret = resp;
   OMX_COMPONENTTYPE *pComp = exe->reference;
   OMX_BUFFERHEADERTYPE *pHeader = exe->bufferHeader.pOutputPortPrivate;
   OMX_U8 *pBuffer = pHeader->pBuffer;
   OMX_PTR *pAppPrivate = pHeader->pAppPrivate;
   OMX_PTR *pPlatformPrivate = pHeader->pPlatformPrivate;
   OMX_PTR *pInputPortPrivate = pHeader->pInputPortPrivate;
   OMX_PTR *pOutputPortPrivate = pHeader->pOutputPortPrivate;

   vc_assert(pHeader);
   memcpy(pHeader, &exe->bufferHeader, sizeof(OMX_BUFFERHEADERTYPE));

   pHeader->pBuffer = pBuffer;
   pHeader->pAppPrivate = pAppPrivate;
   pHeader->pPlatformPrivate = pPlatformPrivate;
   pHeader->pInputPortPrivate = pInputPortPrivate;
   pHeader->pOutputPortPrivate = pOutputPortPrivate;

   vc_assert(is_valid_hostside_buffer(pHeader));

   *rlen = sizeof(IL_RESPONSE_HEADER_T);
   ret->func = IL_FILL_THIS_BUFFER;
   ret->err = pComp->FillThisBuffer(pComp, pHeader);
}

void vcil_in_get_component_version(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen)
{
   IL_EXECUTE_HEADER_T *exe = call;
   IL_GET_VERSION_RESPONSE_T *ret = resp;
   OMX_COMPONENTTYPE *pComp = exe->reference;

   *rlen = sizeof(IL_GET_VERSION_RESPONSE_T);
   ret->func = IL_GET_COMPONENT_VERSION;
   ret->err = pComp->GetComponentVersion(pComp, ret->name, &ret->component_version, &ret->spec_version, &ret->uuid);
}

void vcil_in_get_extension_index(ILCS_COMMON_T *st, void *call, int clen, void *resp, int *rlen)
{
   IL_GET_EXTENSION_EXECUTE_T *exe = call;
   IL_GET_EXTENSION_RESPONSE_T *ret = resp;
   OMX_COMPONENTTYPE *pComp = exe->reference;

   *rlen = sizeof(IL_GET_EXTENSION_RESPONSE_T);
   ret->func = IL_GET_EXTENSION_INDEX;
   ret->err = pComp->GetExtensionIndex(pComp, exe->name, &ret->index);
}
