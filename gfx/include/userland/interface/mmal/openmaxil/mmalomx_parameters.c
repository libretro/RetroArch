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
#include "mmalomx_parameters.h"
#include "mmalomx_util_params.h"
#include "mmalomx_roles.h"
#include "mmalomx_registry.h"
#include "mmalomx_logging.h"
#include <util/mmal_util.h>
#include <util/mmal_util_params.h>
#include <util/mmal_util_rational.h>

#define PARAM_GET_PORT(port, component, index) \
   if (index >= component->ports_num) return OMX_ErrorBadPortIndex; \
   port = &component->ports[index]

#define MMALOMX_PARAM_GENERIC_MAX    256

/** A structure capable of holding any OMX parameter that contains a port */
typedef struct MMALOMX_PARAM_OMX_GENERIC_T
{
   MMALOMX_PARAM_OMX_HEADER_T header;
   uint8_t data[MMALOMX_PARAM_GENERIC_MAX];
} MMALOMX_PARAM_OMX_GENERIC_T;

/** A structure capable of holding any OMX parameter that doesn't contain a port */
typedef struct MMALOMX_PARAM_OMX_GENERIC_PORTLESS_T
{
   MMALOMX_PARAM_OMX_HEADER_PORTLESS_T hdr;
   uint8_t data[MMALOMX_PARAM_GENERIC_MAX];
} MMALOMX_PARAM_OMX_GENERIC_PORTLESS_T;

/** A structure capable of holding any MMAL parameter */
typedef struct MMALOMX_PARAM_MMAL_GENERIC_T
{
   MMAL_PARAMETER_HEADER_T header;
   uint8_t data[MMALOMX_PARAM_GENERIC_MAX];
} MMALOMX_PARAM_MMAL_GENERIC_T;

static OMX_ERRORTYPE mmalomx_parameter_set_xlat(MMALOMX_COMPONENT_T *component,
   OMX_INDEXTYPE nParamIndex, OMX_PTR pParam)
{
   const MMALOMX_PARAM_TRANSLATION_T *xlat = mmalomx_find_parameter_from_omx_id(nParamIndex);
   MMALOMX_PARAM_OMX_HEADER_T *omx_header = (MMALOMX_PARAM_OMX_HEADER_T *)pParam;
   MMALOMX_PARAM_MMAL_GENERIC_T mmal_generic;
   MMAL_PARAMETER_HEADER_T *mmal_header = &mmal_generic.header;
   MMAL_PORT_T *mmal_port = component->mmal->control;
   MMAL_STATUS_T status;

   if (!xlat)
   {
      LOG_DEBUG("no translation for omx id 0x%08x", nParamIndex);
      return OMX_ErrorNotImplemented;
   }

   if (!xlat->portless)
   {
      if (omx_header->nSize < sizeof(*omx_header))
         return OMX_ErrorBadParameter;
      if (omx_header->nPortIndex >= component->ports_num)
         return OMX_ErrorBadPortIndex;
      mmal_port = component->ports[omx_header->nPortIndex].mmal;
   }

   if (omx_header->nSize < xlat->omx_size)
      return OMX_ErrorBadParameter;

   /* Handle the direct case first */
   if (xlat->type == MMALOMX_PARAM_TRANSLATION_TYPE_DIRECT)
   {
      mmal_header = (MMAL_PARAMETER_HEADER_T *)(((uint8_t *)pParam) + (xlat->portless ? 0 : 4));
      mmal_generic.header = *mmal_header;
      mmal_header->size = omx_header->nSize - (xlat->portless ? 0 : 4);
      mmal_header->id = xlat->mmal_id;
      status = mmal_port_parameter_set(mmal_port, mmal_header);
      *mmal_header = mmal_generic.header;
      return mmalil_error_to_omx(status);
   }

   if (!xlat->fn.generic && !xlat->fn.simple)
   {
      // FIXME
      return OMX_ErrorNotImplemented;
   }

   // FIXME: check size of mmal_generic is sufficient
   if (sizeof(mmal_generic) < xlat->mmal_size)
      return OMX_ErrorBadParameter;

   mmal_header->size = xlat->mmal_size;
   mmal_header->id = xlat->mmal_id;
   if (xlat->fn.generic)
      status = xlat->fn.generic(MMALOMX_PARAM_MAPPING_TO_MMAL, xlat, mmal_header, pParam, mmal_port);
   else
      status = xlat->fn.simple(MMALOMX_PARAM_MAPPING_TO_MMAL, mmal_header, pParam);
   if (status != MMAL_SUCCESS)
      goto error;

   status = mmal_port_parameter_set(mmal_port, mmal_header);

 error:
   return mmalil_error_to_omx(status);
}

static OMX_ERRORTYPE mmalomx_parameter_get_xlat(MMALOMX_COMPONENT_T *component,
   OMX_INDEXTYPE nParamIndex, OMX_PTR pParam)
{
   const MMALOMX_PARAM_TRANSLATION_T *xlat = mmalomx_find_parameter_from_omx_id(nParamIndex);
   MMALOMX_PARAM_OMX_HEADER_T *omx_header = (MMALOMX_PARAM_OMX_HEADER_T *)pParam;
   MMALOMX_PARAM_MMAL_GENERIC_T mmal_generic;
   MMAL_PARAMETER_HEADER_T *mmal_header = &mmal_generic.header;
   MMAL_PORT_T *mmal_port = component->mmal->control;
   MMAL_STATUS_T status = MMAL_SUCCESS;

   if (!xlat)
      return OMX_ErrorNotImplemented;

   if (!xlat->portless)
   {
      if (omx_header->nSize < sizeof(*omx_header))
         return OMX_ErrorBadParameter;
      if (omx_header->nPortIndex >= component->ports_num)
         return OMX_ErrorBadPortIndex;
      mmal_port = component->ports[omx_header->nPortIndex].mmal;
   }

   if (omx_header->nSize < xlat->omx_size)
      return OMX_ErrorBadParameter;

   /* Handle the direct case first */
   if (xlat->type == MMALOMX_PARAM_TRANSLATION_TYPE_DIRECT)
   {
      OMX_U32 size;
      mmal_header = (MMAL_PARAMETER_HEADER_T *)(((uint8_t *)pParam) + (xlat->portless ? 0 : 4));
      mmal_generic.header = *mmal_header;
      mmal_header->size = omx_header->nSize - (xlat->portless ? 0 : 4);
      mmal_header->id = xlat->mmal_id;
      status = mmal_port_parameter_get(mmal_port, mmal_header);
      *mmal_header = mmal_generic.header;
      size = mmal_header->size + (xlat->portless ? 0 : 4);
      omx_header->nSize = size;
      return mmalil_error_to_omx(status);
   }

   if (xlat->fn.custom)
   {
      return mmalil_error_to_omx(xlat->fn.custom(MMALOMX_PARAM_MAPPING_TO_OMX, xlat, mmal_header,
         pParam, mmal_port));
   }

   if (xlat->fn.list)
   {
      OMX_U32 index, elements;
      mmal_header = mmal_port_parameter_alloc_get(mmal_port, xlat->mmal_id,
         10*xlat->mmal_size, &status);
      if (!mmal_header)
         return OMX_ErrorInsufficientResources;

      /* Check we're not requesting too much */
      index = *(OMX_U32 *)(((uint8_t *)pParam) + xlat->xlat_enum_num);
      elements = (mmal_header->size - sizeof(MMAL_PARAMETER_HEADER_T)) /
         (xlat->mmal_size - sizeof(MMAL_PARAMETER_HEADER_T));
      if (index >= elements)
      {
         vcos_free(mmal_header);
         return OMX_ErrorNoMore;
      }
      status = xlat->fn.list(MMALOMX_PARAM_MAPPING_TO_OMX, xlat, index, mmal_header, pParam, mmal_port);
      vcos_free(mmal_header);
      return mmalil_error_to_omx(status);
   }

   if (!xlat->fn.generic && !xlat->fn.simple)
   {
      // FIXME
      return OMX_ErrorNotImplemented;
   }

   // FIXME: check size of mmal_generic is sufficient
   if (sizeof(mmal_generic) < xlat->mmal_size)
      return OMX_ErrorBadParameter;

   mmal_header->size = xlat->mmal_size;
   mmal_header->id = xlat->mmal_id;

   if (xlat->double_translation)
   {
      if (xlat->fn.generic)
         status = xlat->fn.generic(MMALOMX_PARAM_MAPPING_TO_MMAL, xlat, mmal_header, pParam, mmal_port);
      else
         status = xlat->fn.simple(MMALOMX_PARAM_MAPPING_TO_MMAL, mmal_header, pParam);
   }
   if (status != MMAL_SUCCESS)
      goto error;

   status = mmal_port_parameter_get(mmal_port, mmal_header);
   if (status != MMAL_SUCCESS)
      goto error;

   if (xlat->fn.generic)
      status = xlat->fn.generic(MMALOMX_PARAM_MAPPING_TO_OMX, xlat, mmal_header, pParam, mmal_port);
   else
      status = xlat->fn.simple(MMALOMX_PARAM_MAPPING_TO_OMX, mmal_header, pParam);

 error:
   return mmalil_error_to_omx(status);
}

OMX_ERRORTYPE mmalomx_parameter_extension_index_get(OMX_STRING cParameterName,
   OMX_INDEXTYPE *pIndex)
{
   const MMALOMX_PARAM_TRANSLATION_T *xlat;
   MMAL_BOOL_T config = MMAL_FALSE;
   unsigned int i = 0;

   /* Check we're dealing with our extensions */
   if (!vcos_strncasecmp(cParameterName, MMALOMX_COMPONENT_PREFIX, sizeof(MMALOMX_COMPONENT_PREFIX)-1))
      return OMX_ErrorNotImplemented;
   cParameterName += sizeof(MMALOMX_COMPONENT_PREFIX)-1;

   /* Check if we're dealing with a config or param */
   if (!vcos_strncasecmp(cParameterName, "index.config.", sizeof("index.config.")-1))
      config = MMAL_TRUE;
   if (!config && vcos_strncasecmp(cParameterName, "index.param.", sizeof("index.param.")-1))
      return OMX_ErrorNotImplemented;
   if (config)
      cParameterName += sizeof("index.config.")-1;
   else
      cParameterName += sizeof("index.param.")-1;

   /* Loop through all the */
   while ((xlat = mmalomx_find_parameter_enum(i++)) != NULL)
   {
      const char *name = xlat->omx_name;

      /* We only report vendor extensions */
      if (xlat->omx_id < OMX_IndexVendorStartUnused)
         continue;

      /* Strip out the standard prefix */
      if (config)
      {
         if (!strncmp(name, "OMX_IndexConfigBrcm", sizeof("OMX_IndexConfigBrcm")-1))
            name += sizeof("OMX_IndexConfigBrcm")-1;
         else if (!strncmp(name, "OMX_IndexConfig", sizeof("OMX_IndexConfig")-1))
            name += sizeof("OMX_IndexConfig")-1;
         else continue;
      }
      else
      {
         if (!strncmp(name, "OMX_IndexParamBrcm", sizeof("OMX_IndexParamBrcm")-1))
            name += sizeof("OMX_IndexParamBrcm")-1;
         else if (!strncmp(name, "OMX_IndexParam", sizeof("OMX_IndexParam")-1))
            name += sizeof("OMX_IndexParam")-1;
         else continue;
      }

      /* Compare the last part of the name */
      if (!vcos_strcasecmp(name, cParameterName))
      {
         *pIndex = xlat->omx_id;
         return OMX_ErrorNone;
      }
   }

   return OMX_ErrorNotImplemented;
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_get_video_param(MMALOMX_PORT_T *port,
   uint32_t *profile, uint32_t *level, uint32_t *intraperiod)
{
   MMAL_PARAMETER_VIDEO_PROFILE_T mmal_param = {{MMAL_PARAMETER_PROFILE, sizeof(mmal_param)},
      {{(MMAL_VIDEO_PROFILE_T)0, (MMAL_VIDEO_LEVEL_T)0}}};

   *profile = *level = *intraperiod = 0;

   mmal_port_parameter_get_uint32(port->mmal, MMAL_PARAMETER_INTRAPERIOD, intraperiod);

   if (mmal_port_parameter_get(port->mmal, &mmal_param.hdr) == MMAL_SUCCESS)
   {
      *profile = mmalil_video_profile_to_omx(mmal_param.profile[0].profile);
      *level = mmalil_video_level_to_omx(mmal_param.profile[0].level);
   }

   return OMX_ErrorNone;
}

/*****************************************************************************/
OMX_ERRORTYPE mmalomx_parameter_get(MMALOMX_COMPONENT_T *component,
   OMX_INDEXTYPE nParamIndex, OMX_PTR pParam)
{
   MMALOMX_PORT_T *port = NULL;

   switch(nParamIndex)
   {
   /* All OMX_IndexParamVideo parameters are only partially implemented
    * and we try and use sensible hard-coded values for the rest. */
   case OMX_IndexParamVideoAvc:
      {
         OMX_VIDEO_PARAM_AVCTYPE *param = (OMX_VIDEO_PARAM_AVCTYPE *)pParam;
         uint32_t profile, level, intraperiod;
         PARAM_GET_PORT(port, component, param->nPortIndex);
         if (param->nSize < sizeof(*param))
            return OMX_ErrorBadParameter;
         memset(&param->nSliceHeaderSpacing, 0,
            param->nSize - offsetof(OMX_VIDEO_PARAM_AVCTYPE, nSliceHeaderSpacing));

         mmalomx_get_video_param(port, &profile, &level, &intraperiod);
         param->eProfile = (OMX_VIDEO_AVCPROFILETYPE)profile;
         param->eLevel = (OMX_VIDEO_AVCLEVELTYPE)level;
         param->nPFrames = intraperiod - 1;
         param->bUseHadamard = OMX_TRUE;
         param->nRefFrames = 1;
         param->nAllowedPictureTypes = OMX_VIDEO_PictureTypeI | OMX_VIDEO_PictureTypeP;
         param->bFrameMBsOnly = OMX_TRUE;
         if (param->eProfile != OMX_VIDEO_AVCProfileBaseline)
            param->bEntropyCodingCABAC = OMX_TRUE;
         param->eLoopFilterMode = OMX_VIDEO_AVCLoopFilterEnable;
      }
      return OMX_ErrorNone;
   case OMX_IndexParamVideoMpeg4:
      {
         OMX_VIDEO_PARAM_MPEG4TYPE *param = (OMX_VIDEO_PARAM_MPEG4TYPE *)pParam;
         uint32_t profile, level, intraperiod;
         PARAM_GET_PORT(port, component, param->nPortIndex);
         if (param->nSize < sizeof(*param))
            return OMX_ErrorBadParameter;
         memset(&param->nSliceHeaderSpacing, 0,
            param->nSize - offsetof(OMX_VIDEO_PARAM_MPEG4TYPE, nSliceHeaderSpacing));

         mmalomx_get_video_param(port, &profile, &level, &intraperiod);
         param->eProfile = (OMX_VIDEO_MPEG4PROFILETYPE)profile;
         param->eLevel = (OMX_VIDEO_MPEG4LEVELTYPE)level;
         param->nPFrames = intraperiod - 1;
         param->bACPred = OMX_TRUE;
         param->nAllowedPictureTypes = OMX_VIDEO_PictureTypeI | OMX_VIDEO_PictureTypeP;
      }
      return OMX_ErrorNone;
   case OMX_IndexParamVideoH263:
      {
         OMX_VIDEO_PARAM_H263TYPE *param = (OMX_VIDEO_PARAM_H263TYPE *)pParam;
         uint32_t profile, level, intraperiod;
         PARAM_GET_PORT(port, component, param->nPortIndex);
         if (param->nSize < sizeof(*param))
            return OMX_ErrorBadParameter;
         memset(&param->nPFrames, 0,
            param->nSize - offsetof(OMX_VIDEO_PARAM_H263TYPE, nPFrames));

         mmalomx_get_video_param(port, &profile, &level, &intraperiod);
         param->eProfile = (OMX_VIDEO_H263PROFILETYPE)profile;
         param->eLevel = (OMX_VIDEO_H263LEVELTYPE)level;
         param->nPFrames = intraperiod - 1;
         param->nAllowedPictureTypes = OMX_VIDEO_PictureTypeI | OMX_VIDEO_PictureTypeP;
      }
      return OMX_ErrorNone;
   case OMX_IndexParamVideoMpeg2:
   case OMX_IndexParamVideoWmv:
   case OMX_IndexParamVideoRv:
      {
         OMX_FORMAT_PARAM_TYPE *param = (OMX_FORMAT_PARAM_TYPE *)pParam;
         PARAM_GET_PORT(port, component, param->common.nPortIndex);
         OMX_U32 offset = offsetof(OMX_PARAM_U32TYPE, nU32);
         if (param->common.nSize > sizeof(port->format_param) ||
             param->common.nSize < offset)
            return OMX_ErrorBadParameter;
         memcpy(&param->common.nU32, &port->format_param.common.nU32,
                param->common.nSize - offset);
         return OMX_ErrorNone;
      }
   case OMX_IndexParamAudioPcm:
   case OMX_IndexParamAudioAac:
   case OMX_IndexParamAudioMp3:
   case OMX_IndexParamAudioDdp:
      {
         OMX_FORMAT_PARAM_TYPE *param = (OMX_FORMAT_PARAM_TYPE *)pParam;
         PARAM_GET_PORT(port, component, param->common.nPortIndex);
         OMX_U32 offset = offsetof(OMX_PARAM_U32TYPE, nU32);
         if (param->common.nSize > sizeof(port->format_param) ||
             param->common.nSize < offset)
            return OMX_ErrorBadParameter;
         memcpy(&param->common.nU32, &port->format_param.common.nU32,
                param->common.nSize - offset);
         mmalil_format_to_omx_audio_param(param, NULL, port->mmal->format);
         return OMX_ErrorNone;
      }
   case OMX_IndexParamBrcmPixelAspectRatio:
      {
         OMX_CONFIG_POINTTYPE *param = (OMX_CONFIG_POINTTYPE *)pParam;
         PARAM_GET_PORT(port, component, param->nPortIndex);
         param->nX = port->mmal->format->es->video.par.num;
         param->nY = port->mmal->format->es->video.par.den;
         return OMX_ErrorNone;
      }
   case OMX_IndexParamColorSpace:
      {
         OMX_PARAM_COLORSPACETYPE *param = (OMX_PARAM_COLORSPACETYPE *)pParam;
         PARAM_GET_PORT(port, component, param->nPortIndex);
         param->eColorSpace = mmalil_color_space_to_omx(port->mmal->format->es->video.color_space);
         return OMX_ErrorNone;
      }
   case OMX_IndexConfigCommonOutputCrop:
      {
         OMX_CONFIG_RECTTYPE *param = (OMX_CONFIG_RECTTYPE *)pParam;
         PARAM_GET_PORT(port, component, param->nPortIndex);
         param->nLeft = port->mmal->format->es->video.crop.x;
         param->nTop = port->mmal->format->es->video.crop.y;
         param->nWidth = port->mmal->format->es->video.width;
         if (port->mmal->format->es->video.crop.width)
            param->nWidth = port->mmal->format->es->video.crop.width;
         param->nHeight = port->mmal->format->es->video.height;
         if (port->mmal->format->es->video.crop.height)
            param->nHeight = port->mmal->format->es->video.crop.height;
         return OMX_ErrorNone;
      }
   case OMX_IndexConfigCommonScale:
      {
         OMX_CONFIG_SCALEFACTORTYPE *param = (OMX_CONFIG_SCALEFACTORTYPE *)pParam;
         PARAM_GET_PORT(port, component, param->nPortIndex);
         param->xWidth = param->xHeight = 1<<16;
         if (port->mmal->format->es->video.par.num &&
             port->mmal->format->es->video.par.den)
            param->xWidth = mmal_rational_to_fixed_16_16(port->mmal->format->es->video.par);
         return OMX_ErrorNone;
      }
   default:
      return mmalomx_parameter_get_xlat(component, nParamIndex, pParam);
   }

   return OMX_ErrorNotImplemented;
}

/*****************************************************************************/
static OMX_ERRORTYPE mmalomx_set_video_param(MMALOMX_PORT_T *port,
   uint32_t profile, uint32_t level, uint32_t intraperiod)
{
   MMAL_PARAMETER_VIDEO_PROFILE_T mmal_param = {{MMAL_PARAMETER_PROFILE, sizeof(mmal_param)},
      {{(MMAL_VIDEO_PROFILE_T)0, (MMAL_VIDEO_LEVEL_T)0}}};
   OMX_VIDEO_CODINGTYPE coding =
      mmalil_encoding_to_omx_video_coding(port->mmal->format->encoding);

   if (mmal_port_parameter_set_uint32(port->mmal, MMAL_PARAMETER_INTRAPERIOD,
          intraperiod) != MMAL_SUCCESS)
      return OMX_ErrorBadParameter;

   mmal_param.profile[0].profile = (MMAL_VIDEO_PROFILE_T)
      mmalil_omx_video_profile_to_mmal(profile, coding);
   mmal_param.profile[0].level = (MMAL_VIDEO_LEVEL_T)
      mmalil_omx_video_level_to_mmal(level, coding);
   if (mmal_port_parameter_set(port->mmal, &mmal_param.hdr) != MMAL_SUCCESS)
      return OMX_ErrorBadParameter;

   return OMX_ErrorNone;
}

/*****************************************************************************/
OMX_ERRORTYPE mmalomx_parameter_set(MMALOMX_COMPONENT_T *component,
   OMX_INDEXTYPE nParamIndex, OMX_PTR pParam)
{
   MMALOMX_PORT_T *port = NULL;

   switch(nParamIndex)
   {
   /* All OMX_IndexParamVideo parameters are only partially implemented */
   case OMX_IndexParamVideoAvc:
      {
         OMX_VIDEO_PARAM_AVCTYPE *param = (OMX_VIDEO_PARAM_AVCTYPE *)pParam;
         PARAM_GET_PORT(port, component, param->nPortIndex);
         if (param->nSize < sizeof(*param))
            return OMX_ErrorBadParameter;
         return mmalomx_set_video_param(port, param->eProfile, param->eLevel,
            param->nPFrames + 1);
      }
   case OMX_IndexParamVideoMpeg4:
      {
         OMX_VIDEO_PARAM_MPEG4TYPE *param = (OMX_VIDEO_PARAM_MPEG4TYPE *)pParam;
         PARAM_GET_PORT(port, component, param->nPortIndex);
         if (param->nSize < sizeof(*param))
            return OMX_ErrorBadParameter;
         return mmalomx_set_video_param(port, param->eProfile, param->eLevel,
            param->nPFrames + 1);
      }
   case OMX_IndexParamVideoH263:
      {
         OMX_VIDEO_PARAM_H263TYPE *param = (OMX_VIDEO_PARAM_H263TYPE *)pParam;
         PARAM_GET_PORT(port, component, param->nPortIndex);
         if (param->nSize < sizeof(*param))
            return OMX_ErrorBadParameter;
         return mmalomx_set_video_param(port, param->eProfile, param->eLevel,
            param->nPFrames + 1);
      }
   case OMX_IndexParamVideoMpeg2:
   case OMX_IndexParamVideoWmv:
   case OMX_IndexParamVideoRv:
      {
         OMX_FORMAT_PARAM_TYPE *param = (OMX_FORMAT_PARAM_TYPE *)pParam;
         OMX_U32 offset = offsetof(OMX_PARAM_U32TYPE, nU32);
         PARAM_GET_PORT(port, component, param->common.nPortIndex);
         if (param->common.nSize > sizeof(port->format_param) ||
             param->common.nSize < offset)
            return OMX_ErrorBadParameter;
         memcpy(&port->format_param.common.nU32, &param->common.nU32,
                param->common.nSize - offset);
         return OMX_ErrorNone;
      }
   case OMX_IndexParamAudioPcm:
   case OMX_IndexParamAudioAac:
   case OMX_IndexParamAudioMp3:
   case OMX_IndexParamAudioDdp:
      {
         OMX_FORMAT_PARAM_TYPE *param = (OMX_FORMAT_PARAM_TYPE *)pParam;
         OMX_U32 offset = offsetof(OMX_PARAM_U32TYPE, nU32);
         PARAM_GET_PORT(port, component, param->common.nPortIndex);
         if (param->common.nSize > sizeof(port->format_param) ||
             param->common.nSize < offset)
            return OMX_ErrorBadParameter;
         memcpy(&port->format_param.common.nU32, &param->common.nU32,
                param->common.nSize - offset);
         mmalil_omx_audio_param_to_format(port->mmal->format,
            mmalil_omx_audio_param_index_to_coding(nParamIndex), param);
         mmal_port_format_commit(port->mmal);
         return OMX_ErrorNone;
      }
   case OMX_IndexParamBrcmPixelAspectRatio:
      {
         OMX_CONFIG_POINTTYPE *param = (OMX_CONFIG_POINTTYPE *)pParam;
         PARAM_GET_PORT(port, component, param->nPortIndex);
         port->mmal->format->es->video.par.num = param->nX;
         port->mmal->format->es->video.par.den = param->nY;
         mmal_rational_simplify(&port->mmal->format->es->video.par);
         return mmalil_error_to_omx(mmal_port_format_commit(port->mmal));
      }
   case OMX_IndexParamColorSpace:
      {
         OMX_PARAM_COLORSPACETYPE *param = (OMX_PARAM_COLORSPACETYPE *)pParam;
         PARAM_GET_PORT(port, component, param->nPortIndex);
         port->mmal->format->es->video.color_space = mmalil_omx_color_space_to_mmal(param->eColorSpace);
         return mmalil_error_to_omx(mmal_port_format_commit(port->mmal));
      }
   case OMX_IndexParamBrcmVideoCroppingDisable:
      {
         OMX_CONFIG_PORTBOOLEANTYPE *param = (OMX_CONFIG_PORTBOOLEANTYPE *)pParam;
         PARAM_GET_PORT(port, component, param->nPortIndex);
         port->no_cropping = param->bEnabled;
         return OMX_ErrorNone;
      }
   default:
      return mmalomx_parameter_set_xlat(component, nParamIndex, pParam);
   }

   return OMX_ErrorNotImplemented;
}
