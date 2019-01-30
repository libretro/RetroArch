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

/*
 * \file
 *
 * \brief Host core implementation.
 */

#include <stdio.h>
#include <stdarg.h>

//includes
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "IL/OMX_Component.h"
#include "interface/vcos/vcos.h"

#include "interface/vmcs_host/vcilcs.h"
#include "interface/vmcs_host/vchost.h"
#include "interface/vmcs_host/vcilcs_common.h"

static int coreInit = 0;
static int nActiveHandles = 0;
static ILCS_SERVICE_T *ilcs_service = NULL;
static VCOS_MUTEX_T lock;
static VCOS_ONCE_T once = VCOS_ONCE_INIT;

/* Atomic creation of lock protecting shared state */
static void initOnce(void)
{
   VCOS_STATUS_T status;
   status = vcos_mutex_create(&lock, VCOS_FUNCTION);
   vcos_demand(status == VCOS_SUCCESS);
}

/* OMX_Init */
OMX_ERRORTYPE OMX_APIENTRY OMX_Init(void)
{
   VCOS_STATUS_T status;
   OMX_ERRORTYPE err = OMX_ErrorNone;

   status = vcos_once(&once, initOnce);
   vcos_demand(status == VCOS_SUCCESS);

   vcos_mutex_lock(&lock);
   
   if(coreInit == 0)
   {
      // we need to connect via an ILCS connection to VideoCore
      VCHI_INSTANCE_T initialise_instance;
      VCHI_CONNECTION_T *connection;
      ILCS_CONFIG_T config;

      vc_host_get_vchi_state(&initialise_instance, &connection);

      vcilcs_config(&config);

      ilcs_service = ilcs_init((VCHIQ_INSTANCE_T) initialise_instance, (void **) &connection, &config, 0);

      if(ilcs_service == NULL)
      {
         err = OMX_ErrorHardware;
         goto end;
      }

      coreInit = 1;
   }
   else
      coreInit++;

end:
   vcos_mutex_unlock(&lock);
   return err;
}

/* OMX_Deinit */
OMX_ERRORTYPE OMX_APIENTRY OMX_Deinit(void)
{
   if(coreInit == 0) // || (coreInit == 1 && nActiveHandles > 0))
      return OMX_ErrorNotReady;

   vcos_mutex_lock(&lock);

   coreInit--;

   if(coreInit == 0)
   {
      // we need to teardown the ILCS connection to VideoCore
      ilcs_deinit(ilcs_service);
      ilcs_service = NULL;
   }

   vcos_mutex_unlock(&lock);
   
   return OMX_ErrorNone;
}


/* OMX_ComponentNameEnum */
OMX_ERRORTYPE OMX_APIENTRY OMX_ComponentNameEnum(
   OMX_OUT OMX_STRING cComponentName,
   OMX_IN  OMX_U32 nNameLength,
   OMX_IN  OMX_U32 nIndex)
{
   if(ilcs_service == NULL)
      return OMX_ErrorBadParameter;

   return vcil_out_component_name_enum(ilcs_get_common(ilcs_service), cComponentName, nNameLength, nIndex);
}


/* OMX_GetHandle */
OMX_ERRORTYPE OMX_APIENTRY OMX_GetHandle(
   OMX_OUT OMX_HANDLETYPE* pHandle,
   OMX_IN  OMX_STRING cComponentName,
   OMX_IN  OMX_PTR pAppData,
   OMX_IN  OMX_CALLBACKTYPE* pCallBacks)
{
   OMX_ERRORTYPE eError;
   OMX_COMPONENTTYPE *pComp;
   OMX_HANDLETYPE hHandle = 0;

   if (pHandle == NULL || cComponentName == NULL || pCallBacks == NULL || ilcs_service == NULL)
   {
      if(pHandle)
         *pHandle = NULL;
      return OMX_ErrorBadParameter;
   }

   {
      pComp = (OMX_COMPONENTTYPE *)malloc(sizeof(OMX_COMPONENTTYPE));
      if (!pComp)
      {
         vcos_assert(0);
         return OMX_ErrorInsufficientResources;
      }
      memset(pComp, 0, sizeof(OMX_COMPONENTTYPE));
      hHandle = (OMX_HANDLETYPE)pComp;
      pComp->nSize = sizeof(OMX_COMPONENTTYPE);
      pComp->nVersion.nVersion = OMX_VERSION;
      eError = vcil_out_create_component(ilcs_get_common(ilcs_service), hHandle, cComponentName);

      if (eError == OMX_ErrorNone) {
         // Check that all function pointers have been filled in.
         // All fields should be non-zero.
         int i;
         uint32_t *p = (uint32_t *) pComp;
         for(i=0; i<sizeof(OMX_COMPONENTTYPE)>>2; i++)
            if(*p++ == 0)
               eError = OMX_ErrorInvalidComponent;

         if(eError != OMX_ErrorNone && pComp->ComponentDeInit)
            pComp->ComponentDeInit(hHandle);
      }      

      if (eError == OMX_ErrorNone) {
         eError = pComp->SetCallbacks(hHandle,pCallBacks,pAppData);
         if (eError != OMX_ErrorNone)
            pComp->ComponentDeInit(hHandle);
      }
      if (eError == OMX_ErrorNone) {
         *pHandle = hHandle;
      }
      else {
         *pHandle = NULL;
         free(pComp);
      }
   } 

   if (eError == OMX_ErrorNone) {
      vcos_mutex_lock(&lock);
      nActiveHandles++;
      vcos_mutex_unlock(&lock);
   }

   return eError;
}

/* OMX_FreeHandle */
OMX_ERRORTYPE OMX_APIENTRY OMX_FreeHandle(
   OMX_IN  OMX_HANDLETYPE hComponent)
{
   OMX_ERRORTYPE eError = OMX_ErrorNone;
   OMX_COMPONENTTYPE *pComp;

   if (hComponent == NULL || ilcs_service == NULL)
      return OMX_ErrorBadParameter;

   pComp = (OMX_COMPONENTTYPE*)hComponent;

   if (ilcs_service == NULL)
      return OMX_ErrorBadParameter;

   eError = (pComp->ComponentDeInit)(hComponent);
   if (eError == OMX_ErrorNone) {
      vcos_mutex_lock(&lock);
      --nActiveHandles;
      vcos_mutex_unlock(&lock);
      free(pComp);
   }

   vcos_assert(nActiveHandles >= 0);

   return eError;
}

/* OMX_SetupTunnel */
OMX_ERRORTYPE OMX_APIENTRY OMX_SetupTunnel(
   OMX_IN  OMX_HANDLETYPE hOutput,
   OMX_IN  OMX_U32 nPortOutput,
   OMX_IN  OMX_HANDLETYPE hInput,
   OMX_IN  OMX_U32 nPortInput)
{
   OMX_ERRORTYPE eError = OMX_ErrorNone;
   OMX_COMPONENTTYPE *pCompIn, *pCompOut;
   OMX_TUNNELSETUPTYPE oTunnelSetup;

   if ((hOutput == NULL && hInput == NULL) || ilcs_service == NULL)
      return OMX_ErrorBadParameter;

   oTunnelSetup.nTunnelFlags = 0;
   oTunnelSetup.eSupplier = OMX_BufferSupplyUnspecified;

   pCompOut = (OMX_COMPONENTTYPE*)hOutput;

   if (hOutput){
      eError = pCompOut->ComponentTunnelRequest(hOutput, nPortOutput, hInput, nPortInput, &oTunnelSetup);
   }

   if (eError == OMX_ErrorNone && hInput) {
      pCompIn = (OMX_COMPONENTTYPE*)hInput;
      eError = pCompIn->ComponentTunnelRequest(hInput, nPortInput, hOutput, nPortOutput, &oTunnelSetup);

      if (eError != OMX_ErrorNone && hOutput) {
         /* cancel tunnel request on output port since input port failed */
         pCompOut->ComponentTunnelRequest(hOutput, nPortOutput, NULL, 0, NULL);
      }
   }
   return eError;
}

/* OMX_GetComponentsOfRole */
OMX_ERRORTYPE OMX_GetComponentsOfRole (
   OMX_IN      OMX_STRING role,
   OMX_INOUT   OMX_U32 *pNumComps,
   OMX_INOUT   OMX_U8  **compNames)
{
   OMX_ERRORTYPE eError = OMX_ErrorNone;

   *pNumComps = 0;
   return eError;
}

/* OMX_GetRolesOfComponent */
OMX_ERRORTYPE OMX_GetRolesOfComponent (
   OMX_IN      OMX_STRING compName,
   OMX_INOUT   OMX_U32 *pNumRoles,
   OMX_OUT     OMX_U8 **roles)
{
   OMX_ERRORTYPE eError = OMX_ErrorNone;

   *pNumRoles = 0;
   return eError;
}

/* OMX_GetDebugInformation */
OMX_ERRORTYPE OMX_GetDebugInformation (
   OMX_OUT    OMX_STRING debugInfo,
   OMX_INOUT  OMX_S32 *pLen)
{
   if(ilcs_service == NULL)
      return OMX_ErrorBadParameter;

   return vcil_out_get_debug_information(ilcs_get_common(ilcs_service), debugInfo, pLen);
}



/* File EOF */

