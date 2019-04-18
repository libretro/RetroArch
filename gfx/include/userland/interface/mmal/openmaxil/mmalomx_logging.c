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

#include "interface/vmcs_host/khronos/IL/OMX_Core.h"
#include "interface/vmcs_host/khronos/IL/OMX_Component.h"
#include "interface/vmcs_host/khronos/IL/OMX_Video.h"
#include "interface/vmcs_host/khronos/IL/OMX_Audio.h"
#include "interface/vmcs_host/khronos/IL/OMX_Broadcom.h"
#include "mmalomx_logging.h"
#include "mmalomx.h"
#include "mmalomx_util_params.h"
#include "interface/vcos/vcos_types.h"

VCOS_LOG_CAT_T mmalomx_log_category;
static VCOS_LOG_LEVEL_T mmalomx_log_level = VCOS_LOG_ERROR;

#define MMALOMX_SAT(a,b,c) ((a)<(b)?(a):(a)>(c)?(c):(a))

void mmalomx_logging_init(void)
{
   vcos_log_set_level(VCOS_LOG_CATEGORY, mmalomx_log_level);
   vcos_log_register("mmalomx", VCOS_LOG_CATEGORY);
}

void mmalomx_logging_deinit(void)
{
   mmalomx_log_level = mmalomx_log_category.level;
   vcos_log_unregister(VCOS_LOG_CATEGORY);
}

const char *mmalomx_param_to_string(OMX_INDEXTYPE param)
{
  static const struct {
    const char *string;
    const OMX_INDEXTYPE param;
  } param_to_names[] =
  {
    {"OMX_IndexParamPriorityMgmt", OMX_IndexParamPriorityMgmt},
    {"OMX_IndexParamAudioInit", OMX_IndexParamAudioInit},
    {"OMX_IndexParamImageInit", OMX_IndexParamImageInit},
    {"OMX_IndexParamVideoInit", OMX_IndexParamVideoInit},
    {"OMX_IndexParamOtherInit", OMX_IndexParamOtherInit},
    {"OMX_IndexParamPortDefinition", OMX_IndexParamPortDefinition},
    {"OMX_IndexParamCompBufferSupplier", OMX_IndexParamCompBufferSupplier},
    {"OMX_IndexParamAudioPortFormat", OMX_IndexParamAudioPortFormat},
    {"OMX_IndexParamVideoPortFormat", OMX_IndexParamVideoPortFormat},
    {"OMX_IndexParamImagePortFormat", OMX_IndexParamImagePortFormat},
    {"OMX_IndexParamOtherPortFormat", OMX_IndexParamOtherPortFormat},
    {"OMX_IndexParamAudioPcm", OMX_IndexParamAudioPcm},
    {"OMX_IndexParamAudioAac", OMX_IndexParamAudioAac},
    {"OMX_IndexParamAudioMp3", OMX_IndexParamAudioMp3},
    {"OMX_IndexParamVideoMpeg2", OMX_IndexParamVideoMpeg2},
    {"OMX_IndexParamVideoMpeg4", OMX_IndexParamVideoMpeg4},
    {"OMX_IndexParamVideoWmv", OMX_IndexParamVideoWmv},
    {"OMX_IndexParamVideoRv", OMX_IndexParamVideoRv},
    {"OMX_IndexParamVideoAvc", OMX_IndexParamVideoAvc},
    {"OMX_IndexParamVideoH263", OMX_IndexParamVideoH263},
    {"OMX_IndexParamStandardComponentRole", OMX_IndexParamStandardComponentRole},
    {"OMX_IndexParamContentURI", OMX_IndexParamContentURI},
    {"OMX_IndexParamCommonSensorMode", OMX_IndexParamCommonSensorMode},
    {"OMX_IndexConfigCommonWhiteBalance", OMX_IndexConfigCommonWhiteBalance},
    {"OMX_IndexConfigCommonDigitalZoom", OMX_IndexConfigCommonDigitalZoom},
    {"OMX_IndexConfigCommonExposureValue", OMX_IndexConfigCommonExposureValue},
    {"OMX_IndexConfigCapturing", OMX_IndexConfigCapturing},
    {"OMX_IndexAutoPauseAfterCapture", OMX_IndexAutoPauseAfterCapture},
    {"OMX_IndexConfigCommonRotate", OMX_IndexConfigCommonRotate},
    {"OMX_IndexConfigCommonMirror", OMX_IndexConfigCommonMirror},
    {"OMX_IndexConfigCommonScale", OMX_IndexConfigCommonScale},
    {"OMX_IndexConfigCommonInputCrop", OMX_IndexConfigCommonInputCrop},
    {"OMX_IndexConfigCommonOutputCrop", OMX_IndexConfigCommonOutputCrop},
    {"OMX_IndexParamNumAvailableStreams", OMX_IndexParamNumAvailableStreams},
    {"OMX_IndexParamActiveStream", OMX_IndexParamActiveStream},
    {"OMX_IndexParamVideoBitrate", OMX_IndexParamVideoBitrate},
    {"OMX_IndexParamVideoProfileLevelQuerySupported", OMX_IndexParamVideoProfileLevelQuerySupported},

    {"OMX_IndexParam unknown", (OMX_INDEXTYPE)0}
  };
  const char *name = mmalomx_parameter_name_omx((uint32_t)param);
  int i;

  if (name)
     return name;

  for(i = 0; param_to_names[i].param &&
      param_to_names[i].param != param; i++);

  return param_to_names[i].string;
}

const char *mmalomx_cmd_to_string(OMX_COMMANDTYPE cmd)
{
  static const char *names[] = {
    "OMX_CommandStateSet", "OMX_CommandFlush", "OMX_CommandPortDisable",
    "OMX_CommandPortEnable", "OMX_CommandMarkBuffer", "OMX_Command unknown"
  };

  return names[MMALOMX_SAT((int)cmd, 0, (int)vcos_countof(names)-1)];
}

const char *mmalomx_state_to_string(OMX_STATETYPE state)
{
  static const char *names[] = {
    "OMX_StateInvalid", "OMX_StateLoaded", "OMX_StateIdle",
    "OMX_StateExecuting", "OMX_StatePause", "OMX_StateWaitForResources",
    "OMX_State unknown"
  };

  return names[MMALOMX_SAT((int)state, 0, (int)vcos_countof(names)-1)];
}

const char *mmalomx_event_to_string(OMX_EVENTTYPE event)
{
  static const char *names[] = {
    "OMX_EventCmdComplete", "OMX_EventError", "OMX_EventMark",
    "OMX_EventPortSettingsChanged", "OMX_EventBufferFlag",
    "OMX_EventResourcesAcquired", "OMX_EventComponentResumed",
    "OMX_EventDynamicResourcesAvailable", "OMX_EventPortFormatDetected",
    "OMX_Event unknown"
  };

  return names[MMALOMX_SAT((int)event, 0, (int)vcos_countof(names)-1)];
}

const char *mmalomx_error_to_string(OMX_ERRORTYPE error)
{
  static const char *names[] = {
    "OMX_ErrorInsufficientResources", "OMX_ErrorUndefined",
    "OMX_ErrorInvalidComponentName", "OMX_ErrorComponentNotFound",
    "OMX_ErrorInvalidComponent", "OMX_ErrorBadParameter",
    "OMX_ErrorNotImplemented", "OMX_ErrorUnderflow",
    "OMX_ErrorOverflow", "OMX_ErrorHardware", "OMX_ErrorInvalidState",
    "OMX_ErrorStreamCorrupt", "OMX_ErrorPortsNotCompatible",
    "OMX_ErrorResourcesLost", "OMX_ErrorNoMore", "OMX_ErrorVersionMismatch",
    "OMX_ErrorNotReady", "OMX_ErrorTimeout", "OMX_ErrorSameState",
    "OMX_ErrorResourcesPreempted", "OMX_ErrorPortUnresponsiveDuringAllocation",
    "OMX_ErrorPortUnresponsiveDuringDeallocation",
    "OMX_ErrorPortUnresponsiveDuringStop", "OMX_ErrorIncorrectStateTransition",
    "OMX_ErrorIncorrectStateOperation", "OMX_ErrorUnsupportedSetting",
    "OMX_ErrorUnsupportedIndex", "OMX_ErrorBadPortIndex",
    "OMX_ErrorPortUnpopulated", "OMX_ErrorComponentSuspended",
    "OMX_ErrorDynamicResourcesUnavailable", "OMX_ErrorMbErrorsInFrame",
    "OMX_ErrorFormatNotDetected", "OMX_ErrorContentPipeOpenFailed",
    "OMX_ErrorContentPipeCreationFailed", "OMX_ErrorSeperateTablesUsed",
    "OMX_ErrorTunnelingUnsupported",
    "OMX_Error unknown"
  };

  if(error == OMX_ErrorNone) return "OMX_ErrorNone";

  error -= OMX_ErrorInsufficientResources;
  return names[MMALOMX_SAT((int)error, 0, (int)vcos_countof(names)-1)];
}
