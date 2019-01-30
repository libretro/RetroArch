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

#include "interface/vmcs_host/khronos/IL/OMX_Broadcom.h"
#include "mmalomx.h"
#include "mmalomx_commands.h"
#include "mmalomx_roles.h"
#include "mmalomx_registry.h"
#include "mmalomx_buffer.h"
#include "mmalomx_parameters.h"
#include "mmalomx_logging.h"

#include <util/mmal_util.h>
#include <util/mmal_util_params.h>
#include <string.h>
#include <stdio.h>

#define MAX_CMD_BUFFERS 5

#define PARAM_GET_PORT(port, component, index) \
   if (index >= component->ports_num) return OMX_ErrorBadPortIndex; \
   port = &component->ports[index]

static void *mmalomx_cmd_thread_func(void *arg);
#define MMALOMX_ZERO_COPY_THRESHOLD 256

/*****************************************************************************/
OMX_ERRORTYPE mmalomx_callback_event_handler(
   MMALOMX_COMPONENT_T *component,
   OMX_EVENTTYPE eEvent,
   OMX_U32 nData1,
   OMX_U32 nData2,
   OMX_PTR pEventData)
{
   LOG_DEBUG("component %p, eEvent %i, nData1 %u, nData2 %u, pEventData %p",
             component, (int)eEvent, (unsigned int)nData1, (unsigned int)nData2, pEventData);
   return component->callbacks.EventHandler((OMX_HANDLETYPE)&component->omx,
         component->callbacks_data, eEvent, nData1, nData2, pEventData);
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_ComponentGetComponentVersion(
   OMX_HANDLETYPE hComponent,
   OMX_STRING pComponentName,
   OMX_VERSIONTYPE* pComponentVersion,
   OMX_VERSIONTYPE* pSpecVersion,
   OMX_UUIDTYPE* pComponentUUID)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;
   const char *short_name, *prefix;

   LOG_TRACE("hComponent %p, componentName %p, componentVersion %p, "
             "pSpecVersion %p, componentUUID %p",
             hComponent, pComponentName, pComponentVersion, pSpecVersion,
             pComponentUUID);

   /* Sanity checks */
   if (!hComponent)
      return OMX_ErrorInvalidComponent;
   if (component->state == OMX_StateInvalid)
      return OMX_ErrorInvalidState;
   if (!pComponentName || !pComponentVersion || !pSpecVersion || !pComponentUUID )
      return OMX_ErrorBadParameter;

   short_name = mmalomx_registry_component_name(component->registry_id, &prefix);

   snprintf(pComponentName, OMX_MAX_STRINGNAME_SIZE, "%s%s", short_name, prefix);
   pComponentVersion->nVersion = 0;
   pSpecVersion->nVersion = OMX_VERSION;
   snprintf((char *)(*pComponentUUID), sizeof(OMX_UUIDTYPE), "%s", pComponentName);

   return OMX_ErrorNone;
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_ComponentSendCommand(
   OMX_HANDLETYPE hComponent,
   OMX_COMMANDTYPE Cmd,
   OMX_U32 nParam1,
   OMX_PTR pCmdData)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;
   OMX_ERRORTYPE status = OMX_ErrorNone;

   LOG_TRACE("hComponent %p, Cmd %i (%s), nParam1 %i (%s), pCmdData %p",
             hComponent, Cmd, mmalomx_cmd_to_string(Cmd), (int)nParam1,
             Cmd == OMX_CommandStateSet ? mmalomx_state_to_string((OMX_STATETYPE)nParam1) : "",
             pCmdData);

   /* Sanity checks */
   if (!hComponent)
      return OMX_ErrorInvalidComponent;
   if (component->state == OMX_StateInvalid)
      return OMX_ErrorInvalidState;

   /* Sanity check port index */
   if (Cmd == OMX_CommandFlush || Cmd == OMX_CommandMarkBuffer ||
       Cmd == OMX_CommandPortEnable || Cmd == OMX_CommandPortDisable)
   {
      if (nParam1 != OMX_ALL && nParam1 >= component->ports_num)
         return OMX_ErrorBadPortIndex;
   }

   if (Cmd == OMX_CommandStateSet ||
       Cmd == OMX_CommandFlush ||
       Cmd == OMX_CommandPortEnable ||
       Cmd == OMX_CommandPortDisable)
   {
      status = mmalomx_command_queue(component, Cmd, nParam1);
   }
   else if (Cmd == OMX_CommandMarkBuffer)
   {
      status = mmalomx_command_port_mark(hComponent, nParam1, pCmdData);
   }
   else
   {
      status = OMX_ErrorNotImplemented;
   }

   return status;
}

/*****************************************************************************/
static MMAL_STATUS_T mmalomx_get_port_settings(MMALOMX_PORT_T *port, OMX_PARAM_PORTDEFINITIONTYPE *def)
{
   MMAL_STATUS_T status = MMAL_SUCCESS;
   MMAL_PORT_T *mmal = port->mmal;

   def->eDomain = mmalil_es_type_to_omx_domain(mmal->format->type);
   def->eDir = OMX_DirInput;
   if (mmal->type == MMAL_PORT_TYPE_OUTPUT)
      def->eDir = OMX_DirOutput;

   if (def->eDomain == OMX_PortDomainVideo)
   {
      def->format.video.eColorFormat = OMX_COLOR_FormatUnused;
      def->format.video.eCompressionFormat = mmalil_encoding_to_omx_video_coding(mmal->format->encoding);
      if (def->format.video.eCompressionFormat == OMX_VIDEO_CodingUnused)
         def->format.video.eColorFormat = mmalil_encoding_to_omx_color_format(mmal->format->encoding);

      def->format.video.nBitrate = mmal->format->bitrate;
      def->format.video.nFrameWidth = mmal->format->es->video.width;
      if (mmal->format->es->video.crop.width)
         def->format.video.nFrameWidth = mmal->format->es->video.crop.width;
      def->format.video.nStride = mmal->format->es->video.width;
      if (port->no_cropping)
         def->format.video.nFrameWidth = def->format.video.nStride;
      def->format.video.nStride =
         mmal_encoding_width_to_stride(mmal->format->encoding, def->format.video.nStride);
      def->format.video.nFrameHeight = mmal->format->es->video.height;
      if (mmal->format->es->video.crop.height)
         def->format.video.nFrameHeight = mmal->format->es->video.crop.height;
      def->format.video.nSliceHeight = mmal->format->es->video.height;
      if (port->no_cropping)
         def->format.video.nFrameHeight = def->format.video.nSliceHeight;
      if (mmal->format->es->video.frame_rate.den)
         def->format.video.xFramerate = (((int64_t)mmal->format->es->video.frame_rate.num) << 16) /
            mmal->format->es->video.frame_rate.den;
      else
         def->format.video.xFramerate = 0;
   }
   else if (def->eDomain == OMX_PortDomainImage)
   {
      def->format.image.eColorFormat = OMX_COLOR_FormatUnused;
      def->format.image.eCompressionFormat = mmalil_encoding_to_omx_image_coding(mmal->format->encoding);
      if (def->format.image.eCompressionFormat == OMX_IMAGE_CodingUnused)
         def->format.image.eColorFormat = mmalil_encoding_to_omx_color_format(mmal->format->encoding);
      if (mmal->format->encoding == MMAL_ENCODING_UNKNOWN)
         def->format.image.eCompressionFormat = OMX_IMAGE_CodingAutoDetect;
      def->format.image.nFrameWidth = mmal->format->es->video.width;
      if (mmal->format->es->video.crop.width)
         def->format.image.nFrameWidth = mmal->format->es->video.crop.width;
      def->format.image.nStride = mmal->format->es->video.width;
      if (port->no_cropping)
         def->format.image.nFrameWidth = def->format.image.nStride;
      def->format.image.nStride =
         mmal_encoding_width_to_stride(mmal->format->encoding, def->format.image.nStride);
      def->format.image.nFrameHeight = mmal->format->es->video.height;
      if (mmal->format->es->video.crop.height)
         def->format.image.nFrameHeight = mmal->format->es->video.crop.height;
      def->format.image.nSliceHeight = mmal->format->es->video.height;
      if (port->no_cropping)
         def->format.image.nFrameHeight = def->format.image.nSliceHeight;
   }
   else if(def->eDomain == OMX_PortDomainAudio)
   {
      def->format.audio.eEncoding = mmalil_encoding_to_omx_audio_coding(mmal->format->encoding);
   }
   else
   {
      LOG_ERROR("%s: unsupported domain (%u)", mmal->name, def->eDomain);
      status = MMAL_EINVAL;
      goto finish;
   }

   def->nBufferAlignment = mmal->buffer_alignment_min;
   def->nBufferCountActual = mmal->buffer_num;
   def->nBufferCountMin = mmal->buffer_num_min;
   def->nBufferSize = mmal->buffer_size;
   if (def->nBufferSize < mmal->buffer_size_min)
      def->nBufferSize = mmal->buffer_size_min;
   def->bEnabled = port->enabled;
   def->bPopulated = port->populated;

 finish:
   return status;
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_ComponentGetParameter(
   OMX_HANDLETYPE hComponent,
   OMX_INDEXTYPE nParamIndex,
   OMX_PTR pParam)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;
   MMALOMX_PORT_T *port = NULL;

   LOG_TRACE("hComponent %p, nParamIndex 0x%x (%s), pParam %p",
             hComponent, nParamIndex, mmalomx_param_to_string(nParamIndex), pParam);

   /* Sanity checks */
   if (!hComponent)
      return OMX_ErrorInvalidComponent;
   if (!pParam)
      return OMX_ErrorBadParameter;
   if (*(OMX_U32 *)pParam < sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE))
      return OMX_ErrorBadParameter;
   if (component->state == OMX_StateInvalid)
      return OMX_ErrorInvalidState;

   switch(nParamIndex)
   {
   case OMX_IndexParamAudioInit:
   case OMX_IndexParamVideoInit:
   case OMX_IndexParamImageInit:
   case OMX_IndexParamOtherInit:
      {
         OMX_PORT_PARAM_TYPE *param = (OMX_PORT_PARAM_TYPE *)pParam;
         param->nStartPortNumber = 0;
         param->nPorts = component->ports_domain_num[OMX_PortDomainAudio];
         if (nParamIndex == OMX_IndexParamAudioInit)
            return OMX_ErrorNone;
         param->nStartPortNumber += param->nPorts;
         param->nPorts = component->ports_domain_num[OMX_PortDomainVideo];
         if (nParamIndex == OMX_IndexParamVideoInit)
            return OMX_ErrorNone;
         param->nStartPortNumber += param->nPorts;
         param->nPorts = component->ports_domain_num[OMX_PortDomainImage];
         if (nParamIndex == OMX_IndexParamImageInit)
            return OMX_ErrorNone;
         param->nStartPortNumber += param->nPorts;
         param->nPorts = component->ports_domain_num[OMX_PortDomainOther];
      }
      return OMX_ErrorNone;
      break;
   case OMX_IndexParamPortDefinition:
      {
         OMX_PARAM_PORTDEFINITIONTYPE *param = (OMX_PARAM_PORTDEFINITIONTYPE *)pParam;
         PARAM_GET_PORT(port, component, param->nPortIndex);
         return mmalil_error_to_omx(mmalomx_get_port_settings(port, param));
      }
      return OMX_ErrorNone;
      break;
   case OMX_IndexParamCompBufferSupplier:
      {
         OMX_PARAM_BUFFERSUPPLIERTYPE *param = (OMX_PARAM_BUFFERSUPPLIERTYPE *)pParam;
         PARAM_GET_PORT(port, component, param->nPortIndex);
         param->eBufferSupplier = OMX_BufferSupplyUnspecified;
      }
      return OMX_ErrorNone;
      break;
   case OMX_IndexParamPriorityMgmt:
      {
         OMX_PRIORITYMGMTTYPE *param = (OMX_PRIORITYMGMTTYPE *)pParam;
         param->nGroupPriority = component->group_priority;
         param->nGroupID = component->group_id;
      }
      return OMX_ErrorNone;
      break;
   case OMX_IndexParamVideoPortFormat:
   case OMX_IndexParamAudioPortFormat:
      {
         OMX_VIDEO_PARAM_PORTFORMATTYPE *param = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pParam;
         PARAM_GET_PORT(port, component, param->nPortIndex);

         /* Populate our internal list of encodings the first time around */
         if (!port->encodings_num)
         {
            port->encodings_header.id = MMAL_PARAMETER_SUPPORTED_ENCODINGS;
            port->encodings_header.size = sizeof(port->encodings_header) + sizeof(port->encodings);
            if (mmal_port_parameter_get(port->mmal, &port->encodings_header) == MMAL_SUCCESS)
            {
                port->encodings_num = (port->encodings_header.size - sizeof(port->encodings_header)) /
                   sizeof(port->encodings[0]);
            }
            if (!port->encodings_num)
            {
               port->encodings_num = 1;
               port->encodings[0] = port->mmal->format->encoding;
            }
         }

         if (param->nIndex >= port->encodings_num)
            return OMX_ErrorNoMore;

         if (nParamIndex == OMX_IndexParamVideoPortFormat)
         {
            param->eColorFormat = OMX_COLOR_FormatUnused;
            param->eCompressionFormat =
               mmalil_encoding_to_omx_video_coding(port->encodings[param->nIndex]);
            if (param->eCompressionFormat == OMX_VIDEO_CodingUnused)
               param->eColorFormat =
                  mmalil_encoding_to_omx_color_format(port->encodings[param->nIndex]);
            param->xFramerate = 0;
         }
         else
         {
            OMX_AUDIO_PARAM_PORTFORMATTYPE *aparam =
               (OMX_AUDIO_PARAM_PORTFORMATTYPE *)pParam;
            aparam->eEncoding =
               mmalil_encoding_to_omx_audio_coding(port->encodings[param->nIndex]);
         }
         return OMX_ErrorNone;
      }
      break;
   case OMX_IndexParamImagePortFormat:
   case OMX_IndexParamOtherPortFormat:
      break;
   case OMX_IndexParamStandardComponentRole:
      {
         OMX_PARAM_COMPONENTROLETYPE *param = (OMX_PARAM_COMPONENTROLETYPE *)pParam;
         const char *role = mmalomx_role_to_name(component->role);
         if (!role)
            role = component->name;
         snprintf((char *)param->cRole, sizeof(param->cRole), "%s", role);
      }
      return OMX_ErrorNone;
   default:
      return mmalomx_parameter_get(component, nParamIndex, pParam);
   }

   return OMX_ErrorNotImplemented;
}

/*****************************************************************************/
static MMAL_STATUS_T mmalomx_set_port_settings(MMALOMX_PORT_T *mmalomx_port,
   OMX_PARAM_PORTDEFINITIONTYPE *def)
{
   MMAL_PORT_T *port = mmalomx_port->mmal;
   uint32_t buffer_size_min = port->buffer_size_min;
   MMAL_STATUS_T status;

   port->format->type = mmalil_omx_domain_to_es_type(def->eDomain);
   port->format->encoding_variant = 0;

   if(def->eDomain == OMX_PortDomainVideo)
   {
      if (def->format.video.eCompressionFormat != OMX_VIDEO_CodingUnused)
         port->format->encoding = mmalil_omx_video_coding_to_encoding(def->format.video.eCompressionFormat);
      else
         port->format->encoding = mmalil_omx_color_format_to_encoding(def->format.video.eColorFormat);

      port->format->bitrate = def->format.video.nBitrate;
      port->format->es->video.width = def->format.video.nFrameWidth;
      if (!mmalomx_port->no_cropping)
         port->format->es->video.crop.width = port->format->es->video.width;
      if (mmal_encoding_stride_to_width(port->format->encoding, def->format.video.nStride))
         port->format->es->video.width =
            mmal_encoding_stride_to_width(port->format->encoding, def->format.video.nStride);
      port->format->es->video.height = def->format.video.nFrameHeight;
      if (!mmalomx_port->no_cropping)
         port->format->es->video.crop.height = port->format->es->video.height;
      if (def->format.video.nSliceHeight > def->format.video.nFrameHeight)
         port->format->es->video.height = def->format.video.nSliceHeight;
      port->format->es->video.frame_rate.num = def->format.video.xFramerate;
      port->format->es->video.frame_rate.den = (1<<16);
   }
   else if(def->eDomain == OMX_PortDomainImage)
   {
      if (def->format.image.eCompressionFormat != OMX_IMAGE_CodingUnused)
         port->format->encoding = mmalil_omx_image_coding_to_encoding(def->format.image.eCompressionFormat);
      else
         port->format->encoding = mmalil_omx_color_format_to_encoding(def->format.image.eColorFormat);

      port->format->es->video.width = def->format.image.nFrameWidth;
      if (!mmalomx_port->no_cropping)
         port->format->es->video.crop.width = port->format->es->video.width;
      if (mmal_encoding_stride_to_width(port->format->encoding, def->format.image.nStride))
         port->format->es->video.width =
            mmal_encoding_stride_to_width(port->format->encoding, def->format.image.nStride);
      port->format->es->video.height = def->format.image.nFrameHeight;
      if (!mmalomx_port->no_cropping)
         port->format->es->video.crop.height = port->format->es->video.height;
      if (def->format.image.nSliceHeight > def->format.image.nFrameHeight)
         port->format->es->video.height = def->format.image.nSliceHeight;
   }
   else if(def->eDomain == OMX_PortDomainAudio)
   {
      port->format->encoding = mmalil_omx_audio_coding_to_encoding(def->format.audio.eEncoding);
   }
   else
   {
      port->format->encoding = MMAL_ENCODING_UNKNOWN;
   }

   port->buffer_num = def->nBufferCountActual;
   port->buffer_size = def->nBufferSize;
   if (port->buffer_size < port->buffer_size_min)
      port->buffer_size = port->buffer_size_min;

   status = mmal_port_format_commit(port);
   if (status != MMAL_SUCCESS)
      return status;

   /* Acknowledge any ongoing port format changed event */
   mmalomx_port->format_changed = MMAL_FALSE;

   /* The minimum buffer size only changes when the format significantly changes
    * and in that case we want to advertise the new requirement to the client. */
   if (port->buffer_size_min != buffer_size_min)
      port->buffer_size = port->buffer_size_min;

   return MMAL_SUCCESS;
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_ComponentSetParameter(
   OMX_HANDLETYPE hComponent,
   OMX_INDEXTYPE nParamIndex,
   OMX_PTR pParam)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;
   MMALOMX_PORT_T *port = NULL;

   LOG_TRACE("hComponent %p, nParamIndex 0x%x (%s), pParam %p",
             hComponent, nParamIndex, mmalomx_param_to_string(nParamIndex), pParam);

   /* Sanity checks */
   if (!hComponent)
      return OMX_ErrorInvalidComponent;
   if (!pParam)
      return OMX_ErrorBadParameter;
   if (*(OMX_U32 *)pParam < sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE))
      return OMX_ErrorBadParameter;
   if (component->state == OMX_StateInvalid)
      return OMX_ErrorInvalidState;

   switch(nParamIndex)
   {
   case OMX_IndexParamPortDefinition:
      {
         OMX_PARAM_PORTDEFINITIONTYPE *param = (OMX_PARAM_PORTDEFINITIONTYPE *)pParam;
         PARAM_GET_PORT(port, component, param->nPortIndex);
         return mmalil_error_to_omx(mmalomx_set_port_settings(port, param));
      }
      return OMX_ErrorNone;
      break;
   case OMX_IndexParamCompBufferSupplier:
      {
         OMX_PARAM_BUFFERSUPPLIERTYPE *param = (OMX_PARAM_BUFFERSUPPLIERTYPE *)pParam;
         PARAM_GET_PORT(port, component, param->nPortIndex);
         //param->eBufferSupplier = OMX_BufferSupplyUnspecified;
      }
      return OMX_ErrorNone;
      break;
   case OMX_IndexParamPriorityMgmt:
      {
         OMX_PRIORITYMGMTTYPE *param = (OMX_PRIORITYMGMTTYPE *)pParam;

         if (component->state != OMX_StateLoaded)
         return OMX_ErrorIncorrectStateOperation;

         component->group_priority = param->nGroupPriority;
         component->group_id = param->nGroupID;
      }
      return OMX_ErrorNone;
      break;
   case OMX_IndexParamAudioPortFormat:
      {
         OMX_AUDIO_PARAM_PORTFORMATTYPE *param = (OMX_AUDIO_PARAM_PORTFORMATTYPE *)pParam;
         PARAM_GET_PORT(port, component, param->nPortIndex);
         port->mmal->format->encoding = mmalil_omx_audio_coding_to_encoding(param->eEncoding);
         port->mmal->format->encoding_variant = 0;
         if (mmal_port_format_commit(port->mmal) != MMAL_SUCCESS)
            LOG_ERROR("OMX_IndexParamAudioPortFormat commit failed");
         return OMX_ErrorNone;
      }
      break;
   case OMX_IndexParamVideoPortFormat:
      {
         OMX_VIDEO_PARAM_PORTFORMATTYPE *param = (OMX_VIDEO_PARAM_PORTFORMATTYPE *)pParam;
         PARAM_GET_PORT(port, component, param->nPortIndex);
         if (param->eCompressionFormat != OMX_VIDEO_CodingUnused)
            port->mmal->format->encoding = mmalil_omx_video_coding_to_encoding(param->eCompressionFormat);
         else
            port->mmal->format->encoding = mmalil_omx_color_format_to_encoding(param->eColorFormat);
         port->mmal->format->encoding_variant = 0;

         if (mmal_port_format_commit(port->mmal) != MMAL_SUCCESS)
            LOG_ERROR("OMX_IndexParamAudioPortFormat commit failed");
         return OMX_ErrorNone;
      }
      break;
   case OMX_IndexParamImagePortFormat:
   case OMX_IndexParamOtherPortFormat:
      break;
   case OMX_IndexParamStandardComponentRole:
      {
         OMX_PARAM_COMPONENTROLETYPE *param = (OMX_PARAM_COMPONENTROLETYPE *)pParam;
         return mmalomx_role_set(component, (const char *)param->cRole);
      }
      break;
   default:
      {
         OMX_ERRORTYPE status = mmalomx_parameter_set(component, nParamIndex, pParam);

         /* Keep track of the zero-copy state */
         if (status == OMX_ErrorNone && nParamIndex == OMX_IndexParamBrcmZeroCopy)
         {
            PARAM_GET_PORT(port, component, ((OMX_CONFIG_PORTBOOLEANTYPE *)pParam)->nPortIndex);
            port->zero_copy = ((OMX_CONFIG_PORTBOOLEANTYPE *)pParam)->bEnabled;
         }

         return status;
      }
   }

   return OMX_ErrorNotImplemented;
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_ComponentGetConfig(
   OMX_HANDLETYPE hComponent,
   OMX_INDEXTYPE nParamIndex,
   OMX_PTR pParam)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;

   LOG_TRACE("hComponent %p, nParamIndex 0x%x (%s), pParam %p",
             hComponent, nParamIndex, mmalomx_param_to_string(nParamIndex), pParam);

   /* Sanity checks */
   if (!hComponent)
      return OMX_ErrorInvalidComponent;
   if (!pParam)
      return OMX_ErrorBadParameter;
   if (*(OMX_U32 *)pParam < sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE))
      return OMX_ErrorBadParameter;
   if (component->state == OMX_StateInvalid)
      return OMX_ErrorInvalidState;

   return mmalomx_parameter_get(component, nParamIndex, pParam);
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_ComponentSetConfig(
   OMX_HANDLETYPE hComponent,
   OMX_INDEXTYPE nParamIndex,
   OMX_PTR pParam)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;

   LOG_TRACE("hComponent %p, nParamIndex 0x%x (%s), pParam %p",
             hComponent, nParamIndex, mmalomx_param_to_string(nParamIndex), pParam);

   /* Sanity checks */
   if (!hComponent)
      return OMX_ErrorInvalidComponent;
   if (!pParam)
      return OMX_ErrorBadParameter;
   if (*(OMX_U32 *)pParam < sizeof(OMX_U32) + sizeof(OMX_VERSIONTYPE))
      return OMX_ErrorBadParameter;
   if (component->state == OMX_StateInvalid)
      return OMX_ErrorInvalidState;

   return mmalomx_parameter_set(component, nParamIndex, pParam);
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_ComponentGetExtensionIndex(
   OMX_HANDLETYPE hComponent,
   OMX_STRING cParameterName,
   OMX_INDEXTYPE* pIndexType)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;

   LOG_TRACE("hComponent %p, cParameterName %s, pIndexType %p",
             hComponent, cParameterName, pIndexType);

   /* Sanity checks */
   if (!hComponent)
      return OMX_ErrorInvalidComponent;
   if (component->state == OMX_StateInvalid)
      return OMX_ErrorInvalidState;

   return mmalomx_parameter_extension_index_get(cParameterName, pIndexType);
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_ComponentGetState(
   OMX_HANDLETYPE hComponent,
   OMX_STATETYPE* pState)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;
   MMAL_PARAM_UNUSED(component);

   LOG_TRACE("hComponent %p, pState, %p", hComponent, pState);

   /* Sanity checks */
   if (!hComponent)
      return OMX_ErrorInvalidComponent;
   if (!pState)
      return OMX_ErrorBadParameter;

   *pState = component->state;
   return OMX_ErrorNone;
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_ComponentTunnelRequest(
   OMX_HANDLETYPE hComponent,
   OMX_U32 nPort,
   OMX_HANDLETYPE hTunneledComp,
   OMX_U32 nTunneledPort,
   OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;
   MMAL_PARAM_UNUSED(component);

   LOG_TRACE("hComponent %p, nPort %i, hTunneledComp %p, nTunneledPort %i, "
             "pTunnelSetup %p", hComponent, (int)nPort, hTunneledComp,
             (int)nTunneledPort, pTunnelSetup);

   /* Sanity checks */
   if (!hComponent)
      return OMX_ErrorInvalidComponent;
   if (component->state == OMX_StateInvalid)
      return OMX_ErrorInvalidState;
   if (nPort >= component->ports_num)
      return OMX_ErrorBadPortIndex;
   if (component->state != OMX_StateLoaded && component->ports[nPort].enabled)
      return OMX_ErrorIncorrectStateOperation;
   if (hTunneledComp && !pTunnelSetup)
      return OMX_ErrorBadParameter;

   if (!hTunneledComp)
      return OMX_ErrorNone;
   return OMX_ErrorNotImplemented;
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_ComponentUseBuffer(
   OMX_HANDLETYPE hComponent,
   OMX_BUFFERHEADERTYPE** ppBuffer,
   OMX_U32 nPortIndex,
   OMX_PTR pAppPrivate,
   OMX_U32 nSizeBytes,
   OMX_U8* pBuffer)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;
   OMX_ERRORTYPE status = OMX_ErrorNone;
   MMAL_BOOL_T populated = MMAL_FALSE;
   OMX_BUFFERHEADERTYPE *buffer;
   MMALOMX_PORT_T *port;

   LOG_TRACE("hComponent %p, ppBufferHdr %p, nPortIndex %i, pAppPrivate %p,"
             " nSizeBytes %i, pBuffer %p", hComponent, ppBuffer,
             (int)nPortIndex, pAppPrivate, (int)nSizeBytes, pBuffer);

   /* Sanity checks */
   if (!hComponent)
      return OMX_ErrorInvalidComponent;
   if (!ppBuffer)
      return OMX_ErrorBadParameter;
   if (component->state == OMX_StateInvalid)
      return OMX_ErrorInvalidState;
   if (nPortIndex >= component->ports_num)
      return OMX_ErrorBadPortIndex;

   /* Make sure any previous command has been processed.
   * This is not ideal since done inline but in practice the actual
   * notification to the client will not be done as part of this call. */
   mmalomx_commands_actions_check(component);

   port = &component->ports[nPortIndex];
   MMALOMX_LOCK_PORT(component, port);

   if (!(port->actions & MMALOMX_ACTION_CHECK_ALLOCATED))
      status = OMX_ErrorIncorrectStateOperation;
   if (port->populated)
      status = OMX_ErrorIncorrectStateOperation;
   if (status != OMX_ErrorNone)
      goto error;

   /* Check for mismatched calls to UseBuffer/AllocateBuffer */
   if (port->buffers && port->buffers_allocated)
   {
      status = OMX_ErrorBadParameter;
      goto error;
   }

   /* Sanity check buffer size */
   if (nSizeBytes < port->mmal->buffer_size_min)
   {
      LOG_ERROR("buffer size too small (%i/%i)", (int)nSizeBytes,
                (int)port->mmal->buffer_size_min);
      status = OMX_ErrorBadParameter;
      goto error;
   }
   if (!port->buffers)
      port->mmal->buffer_size = nSizeBytes;
   if (nSizeBytes > port->mmal->buffer_size)
   {
      LOG_ERROR("buffer size too big (%i/%i)", (int)nSizeBytes,
                (int)port->mmal->buffer_size);
      status = OMX_ErrorBadParameter;
      goto error;
   }

   buffer = calloc( 1, sizeof(*buffer) );
   if (!buffer)
   {
      status = OMX_ErrorInsufficientResources;
      goto error;
   }

   buffer->nSize = sizeof(*buffer);
   buffer->nVersion.nVersion = OMX_VERSION;
   buffer->nAllocLen = nSizeBytes;
   buffer->pBuffer = pBuffer;
   buffer->pAppPrivate = pAppPrivate;
   if (port->direction == OMX_DirInput)
   {
      buffer->nInputPortIndex = nPortIndex;
      buffer->pOutputPortPrivate = pAppPrivate;
   }
   else
   {
      buffer->nOutputPortIndex = nPortIndex;
      buffer->pInputPortPrivate = pAppPrivate;
   }

   *ppBuffer = buffer;
   port->buffers++;
   port->buffers_allocated = MMAL_FALSE;
   port->populated = populated = port->buffers == port->mmal->buffer_num;

   MMALOMX_UNLOCK_PORT(component, port);

   LOG_DEBUG("allocated %i/%i buffers", port->buffers, port->mmal->buffer_num);

   if (populated)
      mmalomx_commands_actions_signal(component);

   return OMX_ErrorNone;

error:
   MMALOMX_UNLOCK_PORT(component, port);
   return status;
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_ComponentAllocateBuffer(
   OMX_HANDLETYPE hComponent,
   OMX_BUFFERHEADERTYPE** ppBuffer,
   OMX_U32 nPortIndex,
   OMX_PTR pAppPrivate,
   OMX_U32 nSizeBytes)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;
   OMX_ERRORTYPE status = OMX_ErrorNone;
   MMAL_BOOL_T populated = MMAL_FALSE;
   OMX_BUFFERHEADERTYPE *buffer = 0;
   MMALOMX_PORT_T *port;

   LOG_TRACE("hComponent %p, ppBuffer %p, nPortIndex %i, pAppPrivate %p, "
             "nSizeBytes %i", hComponent, ppBuffer, (int)nPortIndex,
             pAppPrivate, (int)nSizeBytes);

   /* Sanity checks */
   if (!hComponent)
      return OMX_ErrorInvalidComponent;
   if (!ppBuffer)
      return OMX_ErrorBadParameter;
   if (component->state == OMX_StateInvalid)
      return OMX_ErrorInvalidState;
   if (nPortIndex >= component->ports_num)
      return OMX_ErrorBadPortIndex;

   /* Make sure any previous command has been processed.
   * This is not ideal since done inline but in practice the actual
   * notification to the client will not be done as part of this call. */
   mmalomx_commands_actions_check(component);

   port = &component->ports[nPortIndex];
   MMALOMX_LOCK_PORT(component, port);

   if (!(port->actions & MMALOMX_ACTION_CHECK_ALLOCATED))
      status = OMX_ErrorIncorrectStateOperation;
   if (port->populated)
      status = OMX_ErrorIncorrectStateOperation;
   if (status != OMX_ErrorNone)
      goto error;

   /* Check for mismatched calls to UseBuffer/AllocateBuffer */
   if (!status && port->buffers && !port->buffers_allocated)
   {
      status = OMX_ErrorBadParameter;
      goto error;
   }

   /* Sanity check buffer size */
   if (nSizeBytes < port->mmal->buffer_size_min)
   {
      LOG_ERROR("buffer size too small (%i/%i)", (int)nSizeBytes,
                (int)port->mmal->buffer_size_min);
      status = OMX_ErrorBadParameter;
      goto error;
   }
   if (!port->buffers)
      port->mmal->buffer_size = nSizeBytes;
   if (nSizeBytes > port->mmal->buffer_size)
   {
      LOG_ERROR("buffer size too big (%i/%i)", (int)nSizeBytes,
                (int)port->mmal->buffer_size);
      status = OMX_ErrorBadParameter;
      goto error;
   }

   /* Set the zero-copy mode */
   if (!port->buffers_allocated && nSizeBytes > MMALOMX_ZERO_COPY_THRESHOLD &&
       !port->zero_copy)
   {
      MMAL_STATUS_T status = mmal_port_parameter_set_boolean(port->mmal,
         MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
      if (status != MMAL_SUCCESS && status != MMAL_ENOSYS)
         LOG_ERROR("failed to enable zero copy on %s", port->mmal->name);
   }

   buffer = calloc( 1, sizeof(*buffer) );
   if (!buffer)
   {
      status = OMX_ErrorInsufficientResources;
      goto error;
   }

   buffer->pBuffer = mmal_port_payload_alloc(port->mmal, nSizeBytes);
   if (!buffer->pBuffer)
   {
      status = OMX_ErrorInsufficientResources;
      goto error;
   }

   buffer->nSize = sizeof(*buffer);
   buffer->nVersion.nVersion = OMX_VERSION;
   buffer->nAllocLen = nSizeBytes;
   buffer->pAppPrivate = pAppPrivate;
   if (port->direction == OMX_DirInput)
   {
      buffer->nInputPortIndex = nPortIndex;
      buffer->pOutputPortPrivate = pAppPrivate;
   }
   else
   {
      buffer->nOutputPortIndex = nPortIndex;
      buffer->pInputPortPrivate = pAppPrivate;
   }
   /* Keep an unmodified copy of the pointer for when we come to free it */
   buffer->pPlatformPrivate = (OMX_PTR)buffer->pBuffer;

   *ppBuffer = buffer;
   port->buffers++;
   port->buffers_allocated = MMAL_TRUE;
   port->populated = populated = port->buffers == port->mmal->buffer_num;

   MMALOMX_UNLOCK_PORT(component, port);

   LOG_DEBUG("allocated %i/%i buffers", port->buffers, port->mmal->buffer_num);

   if (populated)
      mmalomx_commands_actions_signal(component);

   return OMX_ErrorNone;

error:
   if (!port->buffers_allocated && !port->zero_copy)
      mmal_port_parameter_set_boolean(port->mmal, MMAL_PARAMETER_ZERO_COPY, MMAL_FALSE);

   MMALOMX_UNLOCK_PORT(component, port);
   LOG_ERROR("failed to allocate %i/%i buffers", port->buffers, port->mmal->buffer_num);
   if (buffer)
      free(buffer);
   return status;
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_ComponentFreeBuffer(
   OMX_HANDLETYPE hComponent,
   OMX_U32 nPortIndex,
   OMX_BUFFERHEADERTYPE* pBuffer)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;
   OMX_ERRORTYPE status = OMX_ErrorNone;
   MMAL_BOOL_T unpopulated, allocated;
   MMALOMX_PORT_T *port;
   unsigned int buffers;

   LOG_TRACE("hComponent %p, nPortIndex %i, pBuffer %p",
             hComponent, (int)nPortIndex, pBuffer);

   /* Sanity checks */
   if (!hComponent)
      return OMX_ErrorInvalidComponent;
   if (!pBuffer)
      return OMX_ErrorBadParameter;
   if (nPortIndex >= component->ports_num)
      return OMX_ErrorBadPortIndex;

   /* Make sure any previous command has been processed.
   * This is not ideal since done inline but in practice the actual
   * notification to the client will not be done as part of this call. */
   mmalomx_commands_actions_check(component);

   port = &component->ports[nPortIndex];
   MMALOMX_LOCK_PORT(component, port);

   if (!port->buffers)
   {
      status = OMX_ErrorBadParameter;
      goto error;
   }

   buffers = --port->buffers;
   port->populated = MMAL_FALSE;
   unpopulated = !(port->actions & MMALOMX_ACTION_CHECK_DEALLOCATED);
   allocated = port->buffers_allocated;

   MMALOMX_UNLOCK_PORT(component, port);

   if (allocated) /* Free the unmodified pointer */
      mmal_port_payload_free(port->mmal, pBuffer->pPlatformPrivate);
   free(pBuffer);

   if (allocated && !port->zero_copy) /* Reset the zero-copy status */
      mmal_port_parameter_set_boolean(port->mmal, MMAL_PARAMETER_ZERO_COPY, MMAL_FALSE);

   LOG_DEBUG("freed %i/%i buffers", port->mmal->buffer_num - port->buffers, port->mmal->buffer_num);

   if (unpopulated)
      mmalomx_callback_event_handler(component, OMX_EventError, OMX_ErrorPortUnpopulated, 0, NULL);

   if (!buffers)
      mmalomx_commands_actions_signal(component);

   return OMX_ErrorNone;

error:
   MMALOMX_UNLOCK_PORT(component, port);
   return status;
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_ComponentEmptyThisBuffer(
   OMX_HANDLETYPE hComponent,
   OMX_BUFFERHEADERTYPE* pBuffer)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;

   if (ENABLE_MMAL_EXTRA_LOGGING)
      LOG_TRACE("hComponent %p, port %i, pBuffer %p", hComponent,
                pBuffer ? (int)pBuffer->nInputPortIndex : -1, pBuffer);

   return mmalomx_buffer_send(component, pBuffer, OMX_DirInput);
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_ComponentFillThisBuffer(
   OMX_HANDLETYPE hComponent,
   OMX_BUFFERHEADERTYPE* pBuffer)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;

   if (ENABLE_MMAL_EXTRA_LOGGING)
      LOG_TRACE("hComponent %p, port %i, pBuffer %p", hComponent,
                pBuffer ? (int)pBuffer->nOutputPortIndex : -1, pBuffer);

  return mmalomx_buffer_send(component, pBuffer, OMX_DirOutput);
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_ComponentSetCallbacks(
   OMX_HANDLETYPE hComponent,
   OMX_CALLBACKTYPE* pCallbacks,
   OMX_PTR pAppData)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;
   MMAL_PARAM_UNUSED(component);

   LOG_TRACE("hComponent %p, pCallbacks %p, pAppData %p",
              hComponent, pCallbacks, pAppData);

   /* Sanity checks */
   if (!hComponent)
      return OMX_ErrorInvalidComponent;
   if (!pCallbacks)
      return OMX_ErrorBadParameter;
   if (component->state == OMX_StateInvalid)
      return OMX_ErrorInvalidState;

   if (component->state != OMX_StateLoaded)
      return OMX_ErrorInvalidState;

   component->callbacks = *pCallbacks;
   component->callbacks_data = pAppData;
   return OMX_ErrorNone;
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_ComponentDeInit(
  OMX_HANDLETYPE hComponent)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;
   MMAL_PARAM_UNUSED(component);

   LOG_TRACE("hComponent %p", hComponent);

   /* Sanity checks */
   if (!hComponent)
      return OMX_ErrorInvalidComponent;

   return OMX_ErrorNone;
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_ComponentUseEGLImage(
   OMX_HANDLETYPE hComponent,
   OMX_BUFFERHEADERTYPE** ppBufferHdr,
   OMX_U32 nPortIndex,
   OMX_PTR pAppPrivate,
   void* eglImage)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;
   MMAL_PARAM_UNUSED(component);

   LOG_TRACE("hComponent %p, ppBufferHdr %p, nPortIndex %i, pAppPrivate %p,"
             " eglImage %p", hComponent, ppBufferHdr, (int)nPortIndex,
             pAppPrivate, eglImage);

   /* Sanity checks */
   if (!hComponent)
      return OMX_ErrorInvalidComponent;
   if (component->state == OMX_StateInvalid)
      return OMX_ErrorInvalidState;

   return OMX_ErrorNotImplemented;
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_ComponentRoleEnum(
   OMX_HANDLETYPE hComponent,
   OMX_U8 *cRole,
   OMX_U32 nIndex)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;
   MMALOMX_ROLE_T role;

   LOG_TRACE("hComponent %p, cRole %p, nIndex %i",
             hComponent, cRole, (int)nIndex);

   /* Sanity checks */
   if (!hComponent)
      return OMX_ErrorInvalidComponent;
   if (component->state == OMX_StateInvalid)
      return OMX_ErrorInvalidState;

   role = mmalomx_registry_component_roles(component->registry_id, nIndex);
   if (!role)
      return OMX_ErrorNoMore;
   if (!mmalomx_role_to_name(role))
      return OMX_ErrorNoMore;

   strcpy((char *)cRole, mmalomx_role_to_name(role));
   return OMX_ErrorNone;
}

/*****************************************************************************/
OMX_API OMX_ERRORTYPE OMX_APIENTRY MMALOMX_EXPORT(OMX_Init)(void)
{
   mmalomx_logging_init();
   LOG_TRACE("Init");
   return OMX_ErrorNone;
}

/*****************************************************************************/
OMX_API OMX_ERRORTYPE OMX_APIENTRY MMALOMX_EXPORT(OMX_Deinit)(void)
{
   LOG_TRACE("Deinit");
   mmalomx_logging_deinit();
   return OMX_ErrorNone;
}

/*****************************************************************************/
OMX_API OMX_ERRORTYPE OMX_APIENTRY MMALOMX_EXPORT(OMX_ComponentNameEnum)(
   OMX_STRING cComponentName,
   OMX_U32 nNameLength,
   OMX_U32 nIndex)
{
   const char *prefix, *name;
   name = mmalomx_registry_component_name(nIndex, &prefix);

   LOG_TRACE("cComponentName %p, nNameLength %i, nIndex %i",
             cComponentName, (int)nNameLength, (int)nIndex);

   /* Sanity checking */
   if (!cComponentName)
      return OMX_ErrorBadParameter;
   if (!name)
      return OMX_ErrorNoMore;
   if (nNameLength <= strlen(name) + strlen(prefix))
      return OMX_ErrorBadParameter;

   sprintf(cComponentName, "%s%s", prefix, name);
   LOG_TRACE("cComponentName: %s", cComponentName);
   return OMX_ErrorNone;
}

/*****************************************************************************/
static void mmalomx_buffer_cb_control(
   MMAL_PORT_T *port,
   MMAL_BUFFER_HEADER_T *buffer)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)port->userdata;

   LOG_DEBUG("received event %4.4s on port %s", (char *)&buffer->cmd, port->name);

   if (buffer->cmd == MMAL_EVENT_ERROR)
   {
      mmalomx_callback_event_handler(component, OMX_EventError,
         mmalil_error_to_omx(*(MMAL_STATUS_T *)buffer->data), 0, NULL);
   }
   else if (buffer->cmd == MMAL_EVENT_EOS &&
            buffer->length == sizeof(MMAL_EVENT_END_OF_STREAM_T))
   {
      MMAL_EVENT_END_OF_STREAM_T *eos = (MMAL_EVENT_END_OF_STREAM_T *)buffer->data;
      if (eos->port_index < port->component->input_num)
      {
         MMALOMX_PORT_T *omx_port = (MMALOMX_PORT_T *)
            port->component->input[eos->port_index]->userdata;
         LOG_DEBUG("send EOS on %i", omx_port->index);
         mmalomx_callback_event_handler(component, OMX_EventBufferFlag,
            omx_port->index, OMX_BUFFERFLAG_EOS, NULL);
      }
   }

   mmal_buffer_header_release(buffer);
}

/*****************************************************************************/
OMX_API OMX_ERRORTYPE OMX_APIENTRY MMALOMX_EXPORT(OMX_GetHandle)(
   OMX_HANDLETYPE* pHandle,
   OMX_STRING cComponentName,
   OMX_PTR pAppData,
   OMX_CALLBACKTYPE* pCallBacks)
{
   OMX_ERRORTYPE status = OMX_ErrorInsufficientResources;
   MMALOMX_COMPONENT_T *component = 0;
   MMAL_COMPONENT_T *mmal_component = 0;
   MMAL_STATUS_T mmal_status;
   unsigned int i, ports_num;
   OMX_PORTDOMAINTYPE domain;
   const char *mmal_name;
   int registry_id;

   LOG_TRACE("pHandle %p, cComponentName %s, pAppData %p, pCallBacks %p",
             pHandle, cComponentName, pAppData, pCallBacks);

   /* Sanity check params */
   if (!pHandle || !cComponentName || !pCallBacks)
      return OMX_ErrorBadParameter;

   /* Find component */
   registry_id = mmalomx_registry_find_component(cComponentName);
   if (registry_id < 0)
      return OMX_ErrorComponentNotFound;

   /* create and setup component */
   mmal_name = mmalomx_registry_component_mmal(registry_id);
   mmal_status = mmal_component_create(mmal_name, &mmal_component);
   if (mmal_status != MMAL_SUCCESS)
   {
      LOG_ERROR("could not create mmal component %s", mmal_name);
      return mmalil_error_to_omx(mmal_status);
   }
   mmal_status = mmal_port_enable(mmal_component->control, mmalomx_buffer_cb_control);
   if (mmal_status != MMAL_SUCCESS)
   {
      LOG_ERROR("could not enable %s", mmal_component->control->name);
      mmal_component_destroy(mmal_component);
      return mmalil_error_to_omx(mmal_status);
   }

   ports_num = mmal_component->port_num - 1;

   component = calloc(1, sizeof(*component) + ports_num * sizeof(*component->ports));
   if (!component)
   {
      mmal_component_destroy(mmal_component);
      return OMX_ErrorInsufficientResources;
   }

   if (vcos_mutex_create(&component->lock, "mmalomx lock") != VCOS_SUCCESS)
   {
      mmal_component_destroy(mmal_component);
      free(component);
      return OMX_ErrorInsufficientResources;
   }
   if (vcos_mutex_create(&component->lock_port, "mmalomx port lock") != VCOS_SUCCESS)
   {
      vcos_mutex_delete(&component->lock);
      mmal_component_destroy(mmal_component);
      free(component);
      return OMX_ErrorInsufficientResources;
   }

   component->omx.nSize = sizeof(component->omx);
   component->omx.nVersion.nVersion = OMX_VERSION;
   component->mmal = mmal_component;
   component->state = OMX_StateLoaded;
   component->callbacks = *pCallBacks;
   component->callbacks_data = pAppData;
   component->ports = (MMALOMX_PORT_T *)&component[1];
   component->registry_id = registry_id;
   component->name = mmalomx_registry_component_name(registry_id, 0);
   component->role = mmalomx_registry_component_roles(registry_id, 0);

   // FIXME: make this configurable
   component->cmd_thread_used = MMAL_TRUE;

   /* Sort the ports into separate OMX domains */
   for (domain = OMX_PortDomainAudio; domain < OMX_PortDomainOther; domain++)
   {
      for (i = 1; i < mmal_component->port_num; i++)
      {
         if (domain == mmalil_es_type_to_omx_domain(mmal_component->port[i]->format->type))
         {
            component->ports[component->ports_num].mmal = mmal_component->port[i];
            component->ports_domain_num[domain]++;
            component->ports_num++;
         }
      }
   }
   LOG_DEBUG("ports: %i audio, %i video",
      component->ports_domain_num[OMX_PortDomainAudio],
      component->ports_domain_num[OMX_PortDomainVideo]);

   /* Setup our ports */
   for (i = 0; i < component->ports_num; i++)
   {
      component->ports[i].component = component;
      if (component->ports[i].mmal->type == MMAL_PORT_TYPE_OUTPUT)
         component->ports[i].direction = OMX_DirOutput;
      component->ports[i].index = i;
      component->ports[i].enabled = MMAL_TRUE;
      component->ports[i].pool =
         mmal_port_pool_create(component->ports[i].mmal, 0, 0);
      if (!component->ports[i].pool)
         goto error;
      component->ports[i].mmal->userdata = (struct MMAL_PORT_USERDATA_T *)&component->ports[i];
   }
   mmal_component->control->userdata = (struct MMAL_PORT_USERDATA_T *)component;

   /* Create our OMX commands queue */
   component->cmd_queue = mmal_queue_create();
   if (!component->cmd_queue)
      goto error;
   component->cmd_pool = mmal_pool_create(MAX_CMD_BUFFERS, 0);
   if (!component->cmd_pool)
      goto error;

   if (component->cmd_thread_used &&
       vcos_semaphore_create(&component->cmd_sema,
                             "mmalomx sema", 0) != VCOS_SUCCESS)
   {
      component->cmd_thread_used = MMAL_FALSE;
      goto error;
   }

   if (component->cmd_thread_used &&
       vcos_thread_create(&component->cmd_thread, component->name, NULL,
                          mmalomx_cmd_thread_func, component) != VCOS_SUCCESS)
   {
      vcos_semaphore_delete(&component->cmd_sema);
      component->cmd_thread_used = MMAL_FALSE;
      goto error;
   }

   /* Set the function pointer for the component's interface */
   component->omx.GetComponentVersion = mmalomx_ComponentGetComponentVersion;
   component->omx.SendCommand = mmalomx_ComponentSendCommand;
   component->omx.GetParameter = mmalomx_ComponentGetParameter;
   component->omx.SetParameter = mmalomx_ComponentSetParameter;
   component->omx.GetConfig = mmalomx_ComponentGetConfig;
   component->omx.SetConfig = mmalomx_ComponentSetConfig;
   component->omx.GetExtensionIndex = mmalomx_ComponentGetExtensionIndex;
   component->omx.GetState = mmalomx_ComponentGetState;
   component->omx.ComponentTunnelRequest = mmalomx_ComponentTunnelRequest;
   component->omx.UseBuffer = mmalomx_ComponentUseBuffer;
   component->omx.AllocateBuffer = mmalomx_ComponentAllocateBuffer;
   component->omx.FreeBuffer = mmalomx_ComponentFreeBuffer;
   component->omx.EmptyThisBuffer = mmalomx_ComponentEmptyThisBuffer;
   component->omx.FillThisBuffer = mmalomx_ComponentFillThisBuffer;
   component->omx.SetCallbacks = mmalomx_ComponentSetCallbacks;
   component->omx.ComponentDeInit = mmalomx_ComponentDeInit;
   component->omx.UseEGLImage = mmalomx_ComponentUseEGLImage;
   component->omx.ComponentRoleEnum = mmalomx_ComponentRoleEnum;
   *pHandle = (OMX_HANDLETYPE)&component->omx;

   return OMX_ErrorNone;

 error:
   MMALOMX_IMPORT(OMX_FreeHandle)((OMX_HANDLETYPE)&component->omx);
   return status;
}

/*****************************************************************************/
OMX_API OMX_ERRORTYPE OMX_APIENTRY MMALOMX_EXPORT(OMX_FreeHandle)(
   OMX_HANDLETYPE hComponent)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)hComponent;
   OMX_ERRORTYPE status;
   unsigned int i;

   LOG_TRACE("hComponent %p", hComponent);

   /* Sanity check */
   if (!hComponent)
      return OMX_ErrorInvalidComponent;

   if (component->omx.ComponentDeInit)
   {
      status = component->omx.ComponentDeInit(hComponent);
      if (status != OMX_ErrorNone)
      {
         LOG_ERROR("ComponentDeInit failed");
         return status;
      }
   }

   if (component->cmd_thread_used)
   {
      component->cmd_thread_used = MMAL_FALSE;
      vcos_semaphore_post(&component->cmd_sema);
      vcos_thread_join(&component->cmd_thread, NULL);
   }

   mmal_component_destroy(component->mmal);
   for (i = 0; i < component->ports_num; i++)
      if (component->ports[i].pool)
         mmal_pool_destroy(component->ports[i].pool);

   if (component->cmd_pool)
      mmal_pool_destroy(component->cmd_pool);
   if (component->cmd_queue)
      mmal_queue_destroy(component->cmd_queue);
   if (component->cmd_thread_used)
      vcos_semaphore_delete(&component->cmd_sema);
   vcos_mutex_delete(&component->lock_port);
   vcos_mutex_delete(&component->lock);
   free(component);
   return OMX_ErrorNone;
}

/*****************************************************************************/
OMX_API OMX_ERRORTYPE MMALOMX_EXPORT(OMX_GetRolesOfComponent)(
   OMX_STRING compName,
   OMX_U32 *pNumRoles,
   OMX_U8 **roles)
{
   OMX_U32 i, num_roles;
   MMALOMX_ROLE_T role;
   int registry_id;

   LOG_TRACE("compName %s, pNumRoles %p, roles %p", compName, pNumRoles, roles);

   /* Sanity checks */
   if (!compName || !pNumRoles)
      return OMX_ErrorBadParameter;

   if (!roles || *pNumRoles > MMALOMX_MAX_ROLES)
      num_roles = MMALOMX_MAX_ROLES;
   else
      num_roles = *pNumRoles;
   *pNumRoles = 0;

   /* Find component */
   registry_id = mmalomx_registry_find_component(compName);
   if (registry_id < 0)
      return OMX_ErrorComponentNotFound;

   /* Enumerate Roles */
   for (i = 0; i < num_roles; i++)
   {
      role = mmalomx_registry_component_roles(registry_id, i);
      if (!role || !mmalomx_role_to_name(role))
         break;

      if(roles)
      {
         strncpy((char *)roles[i], mmalomx_role_to_name(role), OMX_MAX_STRINGNAME_SIZE);
         LOG_DEBUG("found role: %s", roles[i]);
      }
   }
   LOG_DEBUG("found %i roles", (int)i);
   *pNumRoles = i;

   return OMX_ErrorNone;
}

/*****************************************************************************/
OMX_API OMX_ERRORTYPE MMALOMX_EXPORT(OMX_GetComponentsOfRole)(
   OMX_STRING role,
   OMX_U32 *pNumComps,
   OMX_U8  **compNames)
{
   OMX_ERRORTYPE status;
   OMX_HANDLETYPE handle;
   OMX_COMPONENTTYPE *comp;
   OMX_U8 name[OMX_MAX_STRINGNAME_SIZE], compRole[OMX_MAX_STRINGNAME_SIZE];
   OMX_U32 nNameLength = OMX_MAX_STRINGNAME_SIZE, nIndex = 0;
   OMX_U32 nRoles, nIndexRoles, nComps = 0;
   OMX_CALLBACKTYPE callbacks = {0,0,0};

   LOG_TRACE("role %s, pNumComps %p, compNames %p", role, pNumComps, compNames);

   /* Sanity checks */
   if (!role || !pNumComps)
      return OMX_ErrorBadParameter;

   /* Enumerates components */
   while ((status = OMX_ComponentNameEnum((OMX_STRING)name, nNameLength,
                                          nIndex++)) == OMX_ErrorNone)
   {
      /* Find component */
      status = MMALOMX_IMPORT(OMX_GetHandle)(&handle, (OMX_STRING)name, 0, &callbacks);
      if(status != OMX_ErrorNone) continue;
      comp = (OMX_COMPONENTTYPE *)handle;

      /* Enumerate Roles */
      status = MMALOMX_IMPORT(OMX_GetRolesOfComponent)((OMX_STRING)name, &nRoles, 0);
      if(status != OMX_ErrorNone) continue;

      for (nIndexRoles = 0; nIndexRoles < nRoles; nIndexRoles++)
      {
         status = comp->ComponentRoleEnum(handle, compRole, nIndexRoles);
         if(status != OMX_ErrorNone) break;

         if(!strncmp((char *)role, (char *)compRole, OMX_MAX_STRINGNAME_SIZE))
         {
            /* Found one */
            nComps++;

            if(!compNames) break;

            /* Check if enough space was provided for all the component names */
            if(nComps > *pNumComps) return OMX_ErrorBadParameter;

            strncpy((char *)compNames[nComps-1], (char *)name, OMX_MAX_STRINGNAME_SIZE);

            LOG_DEBUG("found component: %s", name);
         }
      }

      MMALOMX_IMPORT(OMX_FreeHandle)(handle);
   }
   LOG_DEBUG("found %i components", (int)nComps);
   *pNumComps = nComps;

   return OMX_ErrorNone;
}

/*****************************************************************************/
OMX_API OMX_ERRORTYPE OMX_APIENTRY MMALOMX_EXPORT(OMX_SetupTunnel)(
   OMX_HANDLETYPE hOutput,
   OMX_U32 nPortOutput,
   OMX_HANDLETYPE hInput,
   OMX_U32 nPortInput)
{
   OMX_TUNNELSETUPTYPE tunnel_setup = {0, OMX_BufferSupplyUnspecified};
   OMX_ERRORTYPE status = OMX_ErrorNone;

   LOG_TRACE("hOutput %p, nPortOutput %d, hInput %p, nPortInput %d",
             hOutput, (int)nPortOutput, hInput, (int)nPortInput);

   /* Sanity checks */
   if (!hOutput && !hInput)
      return OMX_ErrorBadParameter;

   if (hOutput)
   {
      status = ((OMX_COMPONENTTYPE *)hOutput)->ComponentTunnelRequest(
         hOutput, nPortOutput, hInput, nPortInput, &tunnel_setup);
      if (status != OMX_ErrorNone)
         LOG_DEBUG("OMX_SetupTunnel failed on output port (%i)", status);
   }

   if (status == OMX_ErrorNone && hInput)
   {
      status = ((OMX_COMPONENTTYPE *)hInput)->ComponentTunnelRequest(
         hInput, nPortInput, hOutput, nPortOutput, &tunnel_setup);
      if (status != OMX_ErrorNone)
      {
         LOG_DEBUG("OMX_SetupTunnel failed on input port (%i)", status);
         /* Cancel request on output port */
         if (hOutput)
            ((OMX_COMPONENTTYPE *)hOutput)->ComponentTunnelRequest(
               hOutput, nPortOutput, NULL, 0, NULL);
      }
   }

   return status;
}

OMX_API OMX_ERRORTYPE MMALOMX_EXPORT(OMX_GetContentPipe)(
   OMX_HANDLETYPE *hPipe,
   OMX_STRING szURI)
{
   MMAL_PARAM_UNUSED(hPipe);
   MMAL_PARAM_UNUSED(szURI);

   LOG_TRACE("hPipe %p, szURI %s", hPipe, szURI);

   return OMX_ErrorNotImplemented;
}

/*****************************************************************************
 * Processing thread
 *****************************************************************************/
static void *mmalomx_cmd_thread_func(void *arg)
{
   MMALOMX_COMPONENT_T *component = (MMALOMX_COMPONENT_T *)arg;
   VCOS_STATUS_T status;

   while (component->cmd_thread_used)
   {
      status = vcos_semaphore_wait(&component->cmd_sema);
      if (status == VCOS_EAGAIN)
         continue;
      mmalomx_commands_actions_check(component);
   }

   return 0;
}

