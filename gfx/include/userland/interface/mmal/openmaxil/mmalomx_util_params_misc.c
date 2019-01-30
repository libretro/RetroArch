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

#include "mmalomx.h"
#include "mmalomx_util_params_common.h"
#include "mmalomx_logging.h"

static MMAL_STATUS_T mmalomx_param_mapping_event_request(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param)
{
   OMX_CONFIG_REQUESTCALLBACKTYPE *omx = (OMX_CONFIG_REQUESTCALLBACKTYPE *)omx_param;
   MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T *mmal = (MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T *)mmal_param;
   const MMALOMX_PARAM_TRANSLATION_T *change_xlat;

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
   {
      change_xlat = mmalomx_find_parameter_from_omx_id(omx->nIndex);
      if (!change_xlat)
      {
         VCOS_ALERT("ommalomx_param_mapping_event_request: omx parameter "
                    "0x%08x not recognised", omx->nIndex);
         return MMAL_EINVAL;
      }

      mmal->change_id = change_xlat->mmal_id;
      mmal->enable = omx->bEnable;
   }
   else
   {
      change_xlat = mmalomx_find_parameter_from_mmal_id(mmal->change_id);
      if (!change_xlat)
      {
         VCOS_ALERT("mmalomx_param_mapping_event_request: mmal parameter "
                    "0x%08x not recognised", mmal->change_id);
         return MMAL_EINVAL;
      }

      omx->nIndex = change_xlat->omx_id;
      omx->bEnable = mmal->enable;
   }

   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmalomx_param_mapping_statistics(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param)
{
   OMX_CONFIG_BRCMPORTSTATSTYPE *omx = (OMX_CONFIG_BRCMPORTSTATSTYPE *)omx_param;
   MMAL_PARAMETER_STATISTICS_T *mmal = (MMAL_PARAMETER_STATISTICS_T *)mmal_param;

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
   {
      mmal->buffer_count        = omx->nBufferCount;
      mmal->frame_count         = omx->nImageCount + omx->nFrameCount;
      mmal->frames_skipped      = omx->nFrameSkips;
      mmal->frames_discarded    = omx->nDiscards;
      mmal->eos_seen            = omx->nEOS;
      mmal->maximum_frame_bytes = omx->nMaxFrameSize;
      mmal->total_bytes         = omx_ticks_to_s64(omx->nByteCount);
      mmal->corrupt_macroblocks = omx->nCorruptMBs;
   }
   else
   {
      omx->nBufferCount = mmal->buffer_count;
      omx->nFrameCount = mmal->frame_count;
      omx->nImageCount = 0;
      omx->nFrameSkips = mmal->frames_skipped;
      omx->nDiscards = mmal->frames_discarded;
      omx->nEOS = mmal->eos_seen;
      omx->nMaxFrameSize = mmal->maximum_frame_bytes;
      omx->nByteCount = omx_ticks_from_s64(mmal->total_bytes);
      omx->nCorruptMBs = mmal->corrupt_macroblocks;
   }

   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmalomx_param_mapping_buffer_flags(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param)
{
   OMX_PARAM_U32TYPE *omx = (OMX_PARAM_U32TYPE *)omx_param;
   MMAL_PARAMETER_UINT32_T *mmal = (MMAL_PARAMETER_UINT32_T *)mmal_param;

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
      mmal->value = mmalil_buffer_flags_to_mmal(omx->nU32);
   else
      omx->nU32 = mmalil_buffer_flags_to_omx(mmal->value);

   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmalomx_param_mapping_time(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param)
{
   OMX_TIME_CONFIG_TIMESTAMPTYPE *omx = (OMX_TIME_CONFIG_TIMESTAMPTYPE *)omx_param;
   MMAL_PARAMETER_INT64_T *mmal = (MMAL_PARAMETER_INT64_T *)mmal_param;

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
      mmal->value = omx_ticks_to_s64(omx->nTimestamp);
   else
      omx->nTimestamp = omx_ticks_from_s64(mmal->value);

   return MMAL_SUCCESS;
}

const MMALOMX_PARAM_TRANSLATION_T mmalomx_param_xlator_misc[] = {
   MMALOMX_PARAM_STRAIGHT_MAPPING_DOUBLE_TRANSLATION(MMAL_PARAMETER_CHANGE_EVENT_REQUEST, MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T,
      OMX_IndexConfigRequestCallback, OMX_CONFIG_REQUESTCALLBACKTYPE,
      mmalomx_param_mapping_event_request),
   MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_STATISTICS, MMAL_PARAMETER_STATISTICS_T,
      OMX_IndexConfigBrcmPortStats, OMX_CONFIG_BRCMPORTSTATSTYPE,
      mmalomx_param_mapping_statistics),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_MEM_USAGE, MMAL_PARAMETER_MEM_USAGE_T,
      OMX_IndexConfigBrcmPoolMemAllocSize, OMX_PARAM_U32TYPE),
   MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_BUFFER_FLAG_FILTER, MMAL_PARAMETER_UINT32_T,
      OMX_IndexConfigBrcmBufferFlagFilter, OMX_PARAM_U32TYPE,
      mmalomx_param_mapping_buffer_flags),
   MMALOMX_PARAM_BOOLEAN(MMAL_PARAMETER_ZERO_COPY,
      OMX_IndexParamBrcmZeroCopy),
   MMALOMX_PARAM_BOOLEAN(MMAL_PARAMETER_LOCKSTEP_ENABLE,
      OMX_IndexParamBrcmLockStepEnable),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_POWERMON_ENABLE,
      OMX_IndexConfigBrcmPowerMonitor),
   MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_CLOCK_TIME, MMAL_PARAMETER_INT64_T,
      OMX_IndexConfigTimeCurrentMediaTime, OMX_TIME_CONFIG_TIMESTAMPTYPE,
      mmalomx_param_mapping_time),
   MMALOMX_PARAM_TERMINATE()
};

#if 0
/* Conversions which are not done here. Should part of the core. */
MMAL_PARAMETER_SUPPORTED_ENCODINGS
#endif
