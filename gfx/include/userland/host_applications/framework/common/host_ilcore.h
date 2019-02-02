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

#ifndef HOST_ILCORE_H
#define HOST_ILCORE_H

#ifdef WANT_OMX_NAME_MANGLE
OMX_API OMX_ERRORTYPE OMX_APIENTRY host_OMX_Init(void);
OMX_API OMX_ERRORTYPE OMX_APIENTRY host_OMX_Deinit(void);
OMX_API OMX_ERRORTYPE OMX_APIENTRY host_OMX_ComponentNameEnum(
   OMX_OUT OMX_STRING cComponentName,
   OMX_IN  OMX_U32 nNameLength,
   OMX_IN  OMX_U32 nIndex);
OMX_API OMX_ERRORTYPE OMX_APIENTRY host_OMX_GetHandle(
   OMX_OUT OMX_HANDLETYPE* pHandle,
   OMX_IN  OMX_STRING cComponentName,
   OMX_IN  OMX_PTR pAppData,
   OMX_IN  OMX_CALLBACKTYPE* pCallBacks);
OMX_API OMX_ERRORTYPE OMX_APIENTRY host_OMX_FreeHandle(
   OMX_IN  OMX_HANDLETYPE hComponent);
OMX_API OMX_ERRORTYPE OMX_APIENTRY host_OMX_SetupTunnel(
   OMX_IN  OMX_HANDLETYPE hOutput,
   OMX_IN  OMX_U32 nPortOutput,
   OMX_IN  OMX_HANDLETYPE hInput,
   OMX_IN  OMX_U32 nPortInput);
OMX_API OMX_ERRORTYPE host_OMX_GetComponentsOfRole (
   OMX_IN      OMX_STRING role,
   OMX_INOUT   OMX_U32 *pNumComps,
   OMX_INOUT   OMX_U8  **compNames);
OMX_API OMX_ERRORTYPE host_OMX_GetRolesOfComponent (
   OMX_IN      OMX_STRING compName,
   OMX_INOUT   OMX_U32 *pNumRoles,
   OMX_OUT     OMX_U8 **roles);
OMX_ERRORTYPE host_OMX_GetDebugInformation (
   OMX_OUT    OMX_STRING debugInfo,
   OMX_INOUT  OMX_S32 *pLen);

#define OMX_Init host_OMX_Init
#define OMX_Deinit host_OMX_Deinit
#define OMX_ComponentNameEnum host_OMX_ComponentNameEnum
#define OMX_GetHandle host_OMX_GetHandle
#define OMX_FreeHandle host_OMX_FreeHandle
#define OMX_SetupTunnel host_OMX_SetupTunnel
#define OMX_GetComponentsOfRole host_OMX_GetComponentsOfRole
#define OMX_GetRolesOfComponent host_OMX_GetRolesOfComponent
#define OMX_GetDebugInformation host_OMX_GetDebugInformation
#else
OMX_ERRORTYPE OMX_GetDebugInformation (
   OMX_OUT    OMX_STRING debugInfo,
   OMX_INOUT  OMX_S32 *pLen);
#endif

#endif // HOST_ILCORE_H


