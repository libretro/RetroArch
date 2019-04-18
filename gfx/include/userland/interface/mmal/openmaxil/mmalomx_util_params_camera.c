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

static const MMALOMX_PARAM_ENUM_TRANSLATE_T mmalomx_param_enum_awb_mode[] = {
   {MMAL_PARAM_AWBMODE_OFF,         OMX_WhiteBalControlOff},
   {MMAL_PARAM_AWBMODE_AUTO,        OMX_WhiteBalControlAuto},
   {MMAL_PARAM_AWBMODE_SUNLIGHT,    OMX_WhiteBalControlSunLight},
   {MMAL_PARAM_AWBMODE_CLOUDY,      OMX_WhiteBalControlCloudy},
   {MMAL_PARAM_AWBMODE_SHADE,       OMX_WhiteBalControlShade},
   {MMAL_PARAM_AWBMODE_TUNGSTEN,    OMX_WhiteBalControlTungsten},
   {MMAL_PARAM_AWBMODE_FLUORESCENT, OMX_WhiteBalControlFluorescent},
   {MMAL_PARAM_AWBMODE_INCANDESCENT,OMX_WhiteBalControlIncandescent},
   {MMAL_PARAM_AWBMODE_FLASH,       OMX_WhiteBalControlFlash},
   {MMAL_PARAM_AWBMODE_HORIZON,     OMX_WhiteBalControlHorizon},
};

static const MMALOMX_PARAM_ENUM_TRANSLATE_T mmalomx_param_enum_image_effect[] = {
   {MMAL_PARAM_IMAGEFX_NONE,        OMX_ImageFilterNone},
   {MMAL_PARAM_IMAGEFX_NEGATIVE,    OMX_ImageFilterNegative},
   {MMAL_PARAM_IMAGEFX_SOLARIZE,    OMX_ImageFilterSolarize},
   {MMAL_PARAM_IMAGEFX_SKETCH,      OMX_ImageFilterSketch},
   {MMAL_PARAM_IMAGEFX_DENOISE,     OMX_ImageFilterNoise},
   {MMAL_PARAM_IMAGEFX_EMBOSS,      OMX_ImageFilterEmboss},
   {MMAL_PARAM_IMAGEFX_OILPAINT,    OMX_ImageFilterOilPaint},
   {MMAL_PARAM_IMAGEFX_HATCH,       OMX_ImageFilterHatch},
   {MMAL_PARAM_IMAGEFX_GPEN,        OMX_ImageFilterGpen},
   {MMAL_PARAM_IMAGEFX_PASTEL,      OMX_ImageFilterPastel},
   {MMAL_PARAM_IMAGEFX_WATERCOLOUR, OMX_ImageFilterWatercolor},
   {MMAL_PARAM_IMAGEFX_FILM,        OMX_ImageFilterFilm},
   {MMAL_PARAM_IMAGEFX_BLUR,        OMX_ImageFilterBlur},
   {MMAL_PARAM_IMAGEFX_SATURATION,  OMX_ImageFilterSaturation},
   {MMAL_PARAM_IMAGEFX_COLOURSWAP,  OMX_ImageFilterColourSwap},
   {MMAL_PARAM_IMAGEFX_WASHEDOUT,   OMX_ImageFilterWashedOut},
   {MMAL_PARAM_IMAGEFX_POSTERISE,   OMX_ImageFilterPosterise},
   {MMAL_PARAM_IMAGEFX_COLOURPOINT, OMX_ImageFilterColourPoint},
   {MMAL_PARAM_IMAGEFX_COLOURBALANCE, OMX_ImageFilterColourBalance},
   {MMAL_PARAM_IMAGEFX_CARTOON,     OMX_ImageFilterCartoon},
};

static MMAL_STATUS_T mmalomx_param_mapping_colour_effect(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param)
{
   OMX_CONFIG_COLORENHANCEMENTTYPE *omx = (OMX_CONFIG_COLORENHANCEMENTTYPE *)omx_param;
   MMAL_PARAMETER_COLOURFX_T *mmal = (MMAL_PARAMETER_COLOURFX_T *)mmal_param;

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
   {
      mmal->enable = omx->bColorEnhancement;
      mmal->u = omx->nCustomizedU;
      mmal->v = omx->nCustomizedV;
   }
   else
   {
      omx->bColorEnhancement = mmal->enable;
      omx->nCustomizedU = mmal->u;
      omx->nCustomizedV = mmal->v;
   }

   return MMAL_SUCCESS;
}

static const MMALOMX_PARAM_ENUM_TRANSLATE_T mmalomx_param_enum_flicker_avoid[] = {
   {MMAL_PARAM_FLICKERAVOID_OFF,    OMX_COMMONFLICKERCANCEL_OFF},
   {MMAL_PARAM_FLICKERAVOID_AUTO,   OMX_COMMONFLICKERCANCEL_AUTO},
   {MMAL_PARAM_FLICKERAVOID_50HZ,   OMX_COMMONFLICKERCANCEL_50},
   {MMAL_PARAM_FLICKERAVOID_60HZ,   OMX_COMMONFLICKERCANCEL_60},
};

static const MMALOMX_PARAM_ENUM_TRANSLATE_T mmalomx_param_enum_flash[] = {
   {MMAL_PARAM_FLASH_OFF,     OMX_IMAGE_FlashControlOff},
   {MMAL_PARAM_FLASH_AUTO,    OMX_IMAGE_FlashControlAuto},
   {MMAL_PARAM_FLASH_ON,      OMX_IMAGE_FlashControlOn},
   {MMAL_PARAM_FLASH_REDEYE,  OMX_IMAGE_FlashControlRedEyeReduction},
   {MMAL_PARAM_FLASH_FILLIN,  OMX_IMAGE_FlashControlFillin},
   {MMAL_PARAM_FLASH_TORCH,   OMX_IMAGE_FlashControlTorch},
};

static const MMALOMX_PARAM_ENUM_TRANSLATE_T mmalomx_param_enum_redeye[] = {
   {MMAL_PARAM_REDEYE_OFF,    OMX_RedEyeRemovalNone},
   {MMAL_PARAM_REDEYE_ON,     OMX_RedEyeRemovalOn},
   {MMAL_PARAM_REDEYE_ON,     OMX_RedEyeRemovalAuto},
   {MMAL_PARAM_REDEYE_SIMPLE, OMX_RedEyeRemovalSimple}
};

static MMAL_STATUS_T mmalomx_param_mapping_focus(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param)
{
   static const struct MMALOMX_PARAM_ENUM_TRANSLATE_T mmalomx_param_enum_focus[] = {
      {MMAL_PARAM_FOCUS_AUTO,             OMX_IMAGE_FocusControlAutoLock},
      {MMAL_PARAM_FOCUS_CAF,              OMX_IMAGE_FocusControlAuto},
      {MMAL_PARAM_FOCUS_FIXED_INFINITY,   OMX_IMAGE_FocusControlInfinityFixed},
      {MMAL_PARAM_FOCUS_FIXED_HYPERFOCAL, OMX_IMAGE_FocusControlHyperfocal},
      {MMAL_PARAM_FOCUS_FIXED_NEAR,       OMX_IMAGE_FocusControlNearFixed},
      {MMAL_PARAM_FOCUS_FIXED_MACRO,      OMX_IMAGE_FocusControlMacroFixed},
      {MMAL_PARAM_FOCUS_AUTO_MACRO,       OMX_IMAGE_FocusControlAutoLockMacro},
      {MMAL_PARAM_FOCUS_AUTO_NEAR,        OMX_IMAGE_FocusControlAutoLock},
      {MMAL_PARAM_FOCUS_CAF_NEAR,         OMX_IMAGE_FocusControlAutoNear},
      {MMAL_PARAM_FOCUS_CAF_MACRO,        OMX_IMAGE_FocusControlAutoMacro},
      {MMAL_PARAM_FOCUS_CAF_FAST,         OMX_IMAGE_FocusControlAutoFast},
      {MMAL_PARAM_FOCUS_CAF_MACRO_FAST,   OMX_IMAGE_FocusControlAutoMacroFast},
      {MMAL_PARAM_FOCUS_CAF_NEAR_FAST,    OMX_IMAGE_FocusControlAutoNearFast},
      /* {MMAL_PARAM_FOCUS_EDOF, ???}, */
   };
   OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE *omx = (OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE *)omx_param;
   MMAL_PARAMETER_FOCUS_T *mmal = (MMAL_PARAMETER_FOCUS_T *)mmal_param;
   MMALOMX_PARAM_ENUM_FIND(struct MMALOMX_PARAM_ENUM_TRANSLATE_T, xlat_enum, mmalomx_param_enum_focus,
      dir, mmal->value, omx->eFocusControl);

   if (!xlat_enum)
      return MMAL_EINVAL;

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
   {
      mmal->value = xlat_enum->mmal;
   }
   else
   {
      omx->eFocusControl = xlat_enum->omx;
      omx->nFocusStepIndex = -1;
   }

   return MMAL_SUCCESS;
}

static const MMALOMX_PARAM_ENUM_TRANSLATE_T mmalomx_param_enum_mirror[] = {
   {MMAL_PARAM_MIRROR_NONE,         OMX_MirrorNone},
   {MMAL_PARAM_MIRROR_VERTICAL,     OMX_MirrorVertical},
   {MMAL_PARAM_MIRROR_HORIZONTAL,   OMX_MirrorHorizontal},
   {MMAL_PARAM_MIRROR_BOTH,         OMX_MirrorBoth}
};

static const MMALOMX_PARAM_ENUM_TRANSLATE_T mmalomx_param_enum_exposure_mode[] = {
   {MMAL_PARAM_EXPOSUREMODE_OFF,           OMX_ExposureControlOff},
   {MMAL_PARAM_EXPOSUREMODE_AUTO,          OMX_ExposureControlAuto},
   {MMAL_PARAM_EXPOSUREMODE_NIGHT,         OMX_ExposureControlNight},
   {MMAL_PARAM_EXPOSUREMODE_NIGHTPREVIEW,  OMX_ExposureControlNightWithPreview},
   {MMAL_PARAM_EXPOSUREMODE_BACKLIGHT,     OMX_ExposureControlBackLight},
   {MMAL_PARAM_EXPOSUREMODE_SPOTLIGHT,     OMX_ExposureControlSpotLight},
   {MMAL_PARAM_EXPOSUREMODE_SPORTS,        OMX_ExposureControlSports},
   {MMAL_PARAM_EXPOSUREMODE_SNOW,          OMX_ExposureControlSnow},
   {MMAL_PARAM_EXPOSUREMODE_BEACH,         OMX_ExposureControlBeach},
   {MMAL_PARAM_EXPOSUREMODE_VERYLONG,      OMX_ExposureControlVeryLong},
   {MMAL_PARAM_EXPOSUREMODE_FIXEDFPS,      OMX_ExposureControlFixedFps},
   {MMAL_PARAM_EXPOSUREMODE_ANTISHAKE,     OMX_ExposureControlAntishake},
   {MMAL_PARAM_EXPOSUREMODE_FIREWORKS,     OMX_ExposureControlFireworks},
};

static const MMALOMX_PARAM_ENUM_TRANSLATE_T mmalomx_param_enum_capture_status[] = {
   {MMAL_PARAM_CAPTURE_STATUS_NOT_CAPTURING,   OMX_NotCapturing},
   {MMAL_PARAM_CAPTURE_STATUS_CAPTURE_STARTED, OMX_CaptureStarted},
   {MMAL_PARAM_CAPTURE_STATUS_CAPTURE_ENDED,   OMX_CaptureComplete},
};

static MMAL_STATUS_T mmalomx_param_mapping_face_track(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param)
{
   static const MMALOMX_PARAM_ENUM_TRANSLATE_T mmalomx_param_enum_face_track[] = {
      {MMAL_PARAM_FACE_DETECT_NONE,      OMX_FaceDetectionControlNone},
      {MMAL_PARAM_FACE_DETECT_ON,        OMX_FaceDetectionControlOn},
   };
   OMX_CONFIG_FACEDETECTIONCONTROLTYPE *omx = (OMX_CONFIG_FACEDETECTIONCONTROLTYPE *)omx_param;
   MMAL_PARAMETER_FACE_TRACK_T *mmal = (MMAL_PARAMETER_FACE_TRACK_T *)mmal_param;
   MMALOMX_PARAM_ENUM_FIND(MMALOMX_PARAM_ENUM_TRANSLATE_T, xenum, mmalomx_param_enum_face_track,
      dir, mmal->mode, omx->eMode);

   if (!xenum)
      return MMAL_EINVAL;

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
   {
      mmal->mode = xenum->mmal;
      mmal->maxRegions = omx->nMaxRegions;
      mmal->frames = omx->nFrames;
      mmal->quality = omx->nQuality;
   }
   else
   {
      omx->eMode = xenum->omx;
      omx->nMaxRegions = mmal->maxRegions;
      omx->nFrames = mmal->frames;
      omx->nQuality = mmal->quality;
   }

   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmalomx_param_mapping_thumb_cfg(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param)
{
   OMX_PARAM_BRCMTHUMBNAILTYPE *omx = (OMX_PARAM_BRCMTHUMBNAILTYPE *)omx_param;
   MMAL_PARAMETER_THUMBNAIL_CONFIG_T *mmal = (MMAL_PARAMETER_THUMBNAIL_CONFIG_T *)mmal_param;

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
   {
      mmal->enable = !!omx->bEnable;
      mmal->width = omx->nWidth;
      mmal->height = omx->nHeight;
      mmal->quality = 0;
   }
   else
   {
      omx->bEnable = mmal->enable ? OMX_TRUE : OMX_FALSE;
      omx->bUsePreview = OMX_FALSE;
      omx->nWidth = mmal->width;
      omx->nHeight = mmal->height;
      /* We don't have an API for setting the thumbnail quality */
   }

   return MMAL_SUCCESS;
}

static const MMALOMX_PARAM_ENUM_TRANSLATE_T mmalomx_param_enum_stc[] = {
   {MMAL_PARAM_STC_MODE_OFF,        OMX_TimestampModeZero},
   {MMAL_PARAM_STC_MODE_RAW,        OMX_TimestampModeRawStc},
   {MMAL_PARAM_STC_MODE_COOKED,     OMX_TimestampModeResetStc},
};

static const MMALOMX_PARAM_ENUM_TRANSLATE_T mmalomx_param_enum_capture_mode[] = {
   {MMAL_PARAM_CAPTUREMODE_WAIT_FOR_END,          OMX_CameraCaptureModeWaitForCaptureEnd},
   {MMAL_PARAM_CAPTUREMODE_RESUME_VF_IMMEDIATELY, OMX_CameraCaptureModeResumeViewfinderImmediately},
   /*{MMAL_PARAM_CAPTUREMODE_WAIT_FOR_END_AND_HOLD, OMX_CameraCaptureModeWaitForCaptureEndAndUsePreviousInputImage}, Don't enable for now as not working */
};

static MMAL_STATUS_T mmalomx_param_mapping_sensor_info(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param)
{
   OMX_CONFIG_CAMERAINFOTYPE *omx = (OMX_CONFIG_CAMERAINFOTYPE *)omx_param;
   MMAL_PARAMETER_SENSOR_INFORMATION_T *mmal = (MMAL_PARAMETER_SENSOR_INFORMATION_T *)mmal_param;

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
   {
      mmal->f_number = mmal_rational_from_fixed_16_16(omx->xFNumber);
      mmal->focal_length = mmal_rational_from_fixed_16_16(omx->xFocalLength);
      mmal->model_id = omx->nModelId;
      mmal->manufacturer_id = omx->nManufacturerId;
      mmal->revision = omx->nRevNum;
   }
   else
   {
      omx->xFNumber = mmal_rational_to_fixed_16_16(mmal->f_number);
      omx->xFocalLength = mmal_rational_to_fixed_16_16(mmal->focal_length);
      omx->nModelId = mmal->model_id;
      omx->nManufacturerId = mmal->manufacturer_id;
      omx->nRevNum = mmal->revision;
   }

   return MMAL_SUCCESS;
}

static const MMALOMX_PARAM_ENUM_TRANSLATE_T mmalomx_param_enum_flash_select[] = {
   {MMAL_PARAMETER_CAMERA_INFO_FLASH_TYPE_XENON,   OMX_CameraFlashXenon},
   {MMAL_PARAMETER_CAMERA_INFO_FLASH_TYPE_LED,     OMX_CameraFlashLED},
   {MMAL_PARAMETER_CAMERA_INFO_FLASH_TYPE_OTHER,   OMX_CameraFlashNone},
};

static MMAL_STATUS_T mmalomx_param_mapping_fov(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param)
{
   OMX_CONFIG_BRCMFOVTYPE *omx = (OMX_CONFIG_BRCMFOVTYPE *)omx_param;
   MMAL_PARAMETER_FIELD_OF_VIEW_T *mmal = (MMAL_PARAMETER_FIELD_OF_VIEW_T *)mmal_param;

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
   {
      mmal->fov_h = mmal_rational_from_fixed_16_16(omx->xFieldOfViewHorizontal);
      mmal->fov_v = mmal_rational_from_fixed_16_16(omx->xFieldOfViewVertical);
   }
   else
   {
      omx->xFieldOfViewHorizontal = mmal_rational_to_fixed_16_16(mmal->fov_h);
      omx->xFieldOfViewVertical = mmal_rational_to_fixed_16_16(mmal->fov_v);
   }

   return MMAL_SUCCESS;
}

static const MMALOMX_PARAM_ENUM_TRANSLATE_T mmalomx_param_enum_drc[] = {
   {MMAL_PARAMETER_DRC_STRENGTH_OFF,      OMX_DynRangeExpOff},
   {MMAL_PARAMETER_DRC_STRENGTH_LOW,      OMX_DynRangeExpLow},
   {MMAL_PARAMETER_DRC_STRENGTH_MEDIUM,   OMX_DynRangeExpMedium},
   {MMAL_PARAMETER_DRC_STRENGTH_HIGH,     OMX_DynRangeExpHigh},
};

static MMAL_STATUS_T mmalomx_param_mapping_algo_ctrl(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param)
{
   static const MMALOMX_PARAM_ENUM_TRANSLATE_T mmalomx_param_enum_algo_ctrl[] = {
      { MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_FACETRACKING,             OMX_CameraDisableAlgorithmFacetracking},
      { MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_REDEYE_REDUCTION,         OMX_CameraDisableAlgorithmRedEyeReduction},
      { MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_VIDEO_STABILISATION,      OMX_CameraDisableAlgorithmVideoStabilisation},
      { MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_WRITE_RAW,                OMX_CameraDisableAlgorithmWriteRaw},
      { MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_VIDEO_DENOISE,            OMX_CameraDisableAlgorithmVideoDenoise},
      { MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_STILLS_DENOISE,           OMX_CameraDisableAlgorithmStillsDenoise},
      { MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_TEMPORAL_DENOISE,         OMX_CameraDisableAlgorithmMax},
      { MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_ANTISHAKE,                OMX_CameraDisableAlgorithmAntiShake},
      { MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_IMAGE_EFFECTS,            OMX_CameraDisableAlgorithmImageEffects},
      { MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_DYNAMIC_RANGE_COMPRESSION,OMX_CameraDisableAlgorithmDynamicRangeExpansion},
      { MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_FACE_RECOGNITION,         OMX_CameraDisableAlgorithmFaceRecognition},
      { MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_FACE_BEAUTIFICATION,      OMX_CameraDisableAlgorithmFaceBeautification},
      { MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_SCENE_DETECTION,          OMX_CameraDisableAlgorithmSceneDetection},
      { MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_HIGH_DYNAMIC_RANGE,       OMX_CameraDisableAlgorithmHighDynamicRange},
   };
   OMX_PARAM_CAMERADISABLEALGORITHMTYPE *omx = (OMX_PARAM_CAMERADISABLEALGORITHMTYPE *)omx_param;
   MMAL_PARAMETER_ALGORITHM_CONTROL_T *mmal = (MMAL_PARAMETER_ALGORITHM_CONTROL_T *)mmal_param;
   MMALOMX_PARAM_ENUM_FIND(MMALOMX_PARAM_ENUM_TRANSLATE_T, xenum, mmalomx_param_enum_algo_ctrl,
      dir, mmal->algorithm, omx->eAlgorithm);

   if (!xenum)
      return MMAL_EINVAL;

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
   {
      mmal->algorithm = xenum->mmal;
      mmal->enabled = !omx->bDisabled;
   }
   else
   {
      omx->eAlgorithm = xenum->omx;
      omx->bDisabled = !mmal->enabled;
   }

   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmalomx_param_mapping_image_effect_params(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param)
{
   OMX_CONFIG_IMAGEFILTERPARAMSTYPE *omx = (OMX_CONFIG_IMAGEFILTERPARAMSTYPE *)omx_param;
   MMAL_PARAMETER_IMAGEFX_PARAMETERS_T *mmal = (MMAL_PARAMETER_IMAGEFX_PARAMETERS_T *)mmal_param;
   MMALOMX_PARAM_ENUM_FIND(MMALOMX_PARAM_ENUM_TRANSLATE_T, xenum, mmalomx_param_enum_image_effect,
      dir, mmal->effect, omx->eImageFilter);

   if (!xenum)
      return MMAL_EINVAL;

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
   {
      if (omx->nNumParams > MMAL_COUNTOF(mmal->effect_parameter))
         return MMAL_EINVAL;
      mmal->effect = xenum->mmal;
      mmal->num_effect_params = omx->nNumParams;
      memcpy(mmal->effect_parameter, omx->nParams, sizeof(uint32_t) * omx->nNumParams);
   }
   else
   {
      if (mmal->num_effect_params > MMAL_COUNTOF(omx->nParams))
         return MMAL_EINVAL;
      omx->eImageFilter = xenum->omx;
      omx->nNumParams = mmal->num_effect_params;
      memcpy(omx->nParams, mmal->effect_parameter, sizeof(uint32_t) * omx->nNumParams);
   }

   return MMAL_SUCCESS;
}

static const MMALOMX_PARAM_ENUM_TRANSLATE_T mmalomx_param_enum_use_case[] = {
   {MMAL_PARAM_CAMERA_USE_CASE_UNKNOWN,         OMX_CameraUseCaseAuto},
   {MMAL_PARAM_CAMERA_USE_CASE_STILLS_CAPTURE,  OMX_CameraUseCaseStills},
   {MMAL_PARAM_CAMERA_USE_CASE_VIDEO_CAPTURE,   OMX_CameraUseCaseVideo},
};

static MMAL_STATUS_T mmalomx_param_mapping_fps_range(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param)
{
   OMX_PARAM_BRCMFRAMERATERANGETYPE *omx = (OMX_PARAM_BRCMFRAMERATERANGETYPE *)omx_param;
   MMAL_PARAMETER_FPS_RANGE_T *mmal = (MMAL_PARAMETER_FPS_RANGE_T *)mmal_param;

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
   {
      mmal->fps_low = mmal_rational_from_fixed_16_16(omx->xFramerateLow);
      mmal->fps_high = mmal_rational_from_fixed_16_16(omx->xFramerateHigh);
   }
   else
   {
      omx->xFramerateLow = mmal_rational_to_fixed_16_16(mmal->fps_low);
      omx->xFramerateHigh = mmal_rational_to_fixed_16_16(mmal->fps_high);
   }

   return MMAL_SUCCESS;
}

static MMAL_STATUS_T mmalomx_param_mapping_ev_comp(MMALOMX_PARAM_MAPPING_DIRECTION dir,
   MMAL_PARAMETER_HEADER_T *mmal_param, OMX_PTR omx_param)
{
   OMX_PARAM_S32TYPE *omx = (OMX_PARAM_S32TYPE *)omx_param;
   MMAL_PARAMETER_INT32_T *mmal = (MMAL_PARAMETER_INT32_T *)mmal_param;

   if (dir == MMALOMX_PARAM_MAPPING_TO_MMAL)
      mmal->value = (omx->nS32 * 6) >> 16;
   else
      omx->nS32 = (mmal->value << 16) / 6;

   return MMAL_SUCCESS;
}

const MMALOMX_PARAM_TRANSLATION_T mmalomx_param_xlator_camera[] = {
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_ROTATION, MMAL_PARAMETER_INT32_T,
      OMX_IndexConfigCommonRotate, OMX_CONFIG_ROTATIONTYPE),
   MMALOMX_PARAM_ENUM(MMAL_PARAMETER_AWB_MODE, MMAL_PARAM_AWBMODE_T,
      OMX_IndexConfigCommonWhiteBalance, OMX_CONFIG_WHITEBALCONTROLTYPE, mmalomx_param_enum_awb_mode),
   MMALOMX_PARAM_ENUM(MMAL_PARAMETER_IMAGE_EFFECT, MMAL_PARAMETER_IMAGEFX_T,
      OMX_IndexConfigCommonImageFilter, OMX_CONFIG_IMAGEFILTERTYPE, mmalomx_param_enum_image_effect),
   MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_COLOUR_EFFECT, MMAL_PARAMETER_COLOURFX_T,
      OMX_IndexConfigCommonColorEnhancement, OMX_CONFIG_COLORENHANCEMENTTYPE, mmalomx_param_mapping_colour_effect),
   MMALOMX_PARAM_ENUM(MMAL_PARAMETER_FLICKER_AVOID, MMAL_PARAMETER_FLICKERAVOID_T,
      OMX_IndexConfigCommonFlickerCancellation, OMX_CONFIG_FLICKERCANCELTYPE, mmalomx_param_enum_flicker_avoid),
   MMALOMX_PARAM_ENUM(MMAL_PARAMETER_FLASH, MMAL_PARAMETER_FLASH_T,
      OMX_IndexParamFlashControl, OMX_IMAGE_PARAM_FLASHCONTROLTYPE, mmalomx_param_enum_flash),
   MMALOMX_PARAM_ENUM(MMAL_PARAMETER_REDEYE, MMAL_PARAMETER_REDEYE_T,
      OMX_IndexConfigCommonRedEyeRemoval, OMX_CONFIG_REDEYEREMOVALTYPE, mmalomx_param_enum_redeye),
   MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_FOCUS, MMAL_PARAMETER_FOCUS_T,
      OMX_IndexConfigFocusControl, OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE, mmalomx_param_mapping_focus),
   MMALOMX_PARAM_ENUM(MMAL_PARAMETER_REDEYE, MMAL_PARAMETER_REDEYE_T,
      OMX_IndexConfigCommonRedEyeRemoval, OMX_CONFIG_REDEYEREMOVALTYPE, mmalomx_param_enum_flash),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_ZOOM, MMAL_PARAMETER_SCALEFACTOR_T,
      OMX_IndexConfigCommonDigitalZoom, OMX_CONFIG_SCALEFACTORTYPE),
   MMALOMX_PARAM_ENUM(MMAL_PARAMETER_MIRROR, MMAL_PARAMETER_MIRROR_T,
      OMX_IndexConfigCommonMirror, OMX_CONFIG_MIRRORTYPE, mmalomx_param_enum_mirror),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_CAMERA_NUM, MMAL_PARAMETER_UINT32_T,
      OMX_IndexParamCameraDeviceNumber, OMX_PARAM_U32TYPE),
   MMALOMX_PARAM_BOOLEAN(MMAL_PARAMETER_CAPTURE,
      OMX_IndexConfigPortCapturing),
   MMALOMX_PARAM_ENUM(MMAL_PARAMETER_EXPOSURE_MODE, MMAL_PARAMETER_EXPOSUREMODE_T,
      OMX_IndexConfigCommonExposure, OMX_CONFIG_EXPOSURECONTROLTYPE, mmalomx_param_enum_exposure_mode),
   MMALOMX_PARAM_ENUM_PORTLESS(MMAL_PARAMETER_CAPTURE_STATUS, MMAL_PARAMETER_CAPTURE_STATUS_T,
      OMX_IndexParamCaptureStatus, OMX_PARAM_CAPTURESTATETYPE, mmalomx_param_enum_capture_status),
   MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_FACE_TRACK, MMAL_PARAMETER_FACE_TRACK_T,
      OMX_IndexConfigCommonFaceDetectionControl, OMX_CONFIG_FACEDETECTIONCONTROLTYPE, mmalomx_param_mapping_face_track),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_DRAW_BOX_FACES_AND_FOCUS,
      OMX_IndexConfigDrawBoxAroundFaces),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_JPEG_Q_FACTOR, MMAL_PARAMETER_UINT32_T,
      OMX_IndexParamQFactor, OMX_IMAGE_PARAM_QFACTORTYPE),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_EXIF_DISABLE,
      OMX_IndexParamBrcmDisableEXIF),
   MMALOMX_PARAM_STRAIGHT_MAPPING_PORTLESS(MMAL_PARAMETER_THUMBNAIL_CONFIGURATION, MMAL_PARAMETER_THUMBNAIL_CONFIG_T,
      OMX_IndexParamBrcmThumbnail, OMX_PARAM_BRCMTHUMBNAILTYPE, mmalomx_param_mapping_thumb_cfg),
   MMALOMX_PARAM_ENUM(MMAL_PARAMETER_USE_STC, MMAL_PARAMETER_CAMERA_STC_MODE_T,
      OMX_IndexParamCommonUseStcTimestamps, OMX_PARAM_TIMESTAMPMODETYPE, mmalomx_param_enum_stc),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_VIDEO_STABILISATION, MMAL_PARAMETER_BOOLEAN_T,
      OMX_IndexConfigCommonFrameStabilisation, OMX_CONFIG_FRAMESTABTYPE),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_ENABLE_DPF_FILE,
      OMX_IndexParamUseDynamicParameterFile),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_DPF_FAIL_IS_FATAL,
      OMX_IndexParamDynamicParameterFileFailFatal),
   MMALOMX_PARAM_ENUM(MMAL_PARAMETER_CAPTURE_MODE, MMAL_PARAMETER_CAPTUREMODE_T,
      OMX_IndexParamCameraCaptureMode, OMX_PARAM_CAMERACAPTUREMODETYPE, mmalomx_param_enum_capture_mode),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_INPUT_CROP, MMAL_PARAMETER_INPUT_CROP_T,
      OMX_IndexConfigInputCropPercentages, OMX_CONFIG_INPUTCROPTYPE),
   MMALOMX_PARAM_STRAIGHT_MAPPING_PORTLESS(MMAL_PARAMETER_SENSOR_INFORMATION, MMAL_PARAMETER_SENSOR_INFORMATION_T,
      OMX_IndexConfigCameraInfo, OMX_CONFIG_CAMERAINFOTYPE, mmalomx_param_mapping_sensor_info),
   MMALOMX_PARAM_ENUM(MMAL_PARAMETER_FLASH_SELECT, MMAL_PARAMETER_FLASH_SELECT_T,
      OMX_IndexParamCameraFlashType, OMX_PARAM_CAMERAFLASHTYPE, mmalomx_param_enum_flash_select),
   MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_FIELD_OF_VIEW, MMAL_PARAMETER_FIELD_OF_VIEW_T,
      OMX_IndexConfigFieldOfView, OMX_CONFIG_BRCMFOVTYPE, mmalomx_param_mapping_fov),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_HIGH_DYNAMIC_RANGE,
      OMX_IndexConfigBrcmHighDynamicRange),
   MMALOMX_PARAM_ENUM_PORTLESS(MMAL_PARAMETER_DYNAMIC_RANGE_COMPRESSION, MMAL_PARAMETER_DRC_T,
      OMX_IndexConfigDynamicRangeExpansion, OMX_CONFIG_DYNAMICRANGEEXPANSIONTYPE, mmalomx_param_enum_drc),
   MMALOMX_PARAM_STRAIGHT_MAPPING_PORTLESS(MMAL_PARAMETER_ALGORITHM_CONTROL, MMAL_PARAMETER_ALGORITHM_CONTROL_T,
      OMX_IndexParamCameraDisableAlgorithm, OMX_PARAM_CAMERADISABLEALGORITHMTYPE, mmalomx_param_mapping_algo_ctrl),
   MMALOMX_PARAM_RATIONAL(MMAL_PARAMETER_SHARPNESS, MMAL_PARAMETER_RATIONAL_T,
      OMX_IndexConfigCommonSharpness, OMX_CONFIG_SHARPNESSTYPE, 100),
   MMALOMX_PARAM_RATIONAL(MMAL_PARAMETER_CONTRAST, MMAL_PARAMETER_RATIONAL_T,
      OMX_IndexConfigCommonContrast, OMX_CONFIG_CONTRASTTYPE, 100),
   MMALOMX_PARAM_RATIONAL(MMAL_PARAMETER_BRIGHTNESS, MMAL_PARAMETER_RATIONAL_T,
      OMX_IndexConfigCommonContrast, OMX_CONFIG_CONTRASTTYPE, 100),
   MMALOMX_PARAM_RATIONAL(MMAL_PARAMETER_SATURATION, MMAL_PARAMETER_RATIONAL_T,
      OMX_IndexConfigCommonSaturation, OMX_CONFIG_SATURATIONTYPE, 100),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_ANTISHAKE,
      OMX_IndexConfigStillsAntiShakeEnable),
   MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_IMAGE_EFFECT_PARAMETERS, MMAL_PARAMETER_IMAGEFX_PARAMETERS_T,
      OMX_IndexConfigCommonImageFilterParameters, OMX_CONFIG_IMAGEFILTERPARAMSTYPE, mmalomx_param_mapping_image_effect_params),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_CAMERA_BURST_CAPTURE,
      OMX_IndexConfigBurstCapture),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_CAMERA_MIN_ISO, MMAL_PARAMETER_UINT32_T,
      OMX_IndexConfigCameraIsoReferenceValue, OMX_PARAM_U32TYPE),
   MMALOMX_PARAM_ENUM_PORTLESS(MMAL_PARAMETER_CAMERA_USE_CASE, MMAL_PARAMETER_CAMERA_USE_CASE_T,
      OMX_IndexConfigCameraUseCase, OMX_CONFIG_CAMERAUSECASETYPE, mmalomx_param_enum_use_case),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_CAPTURE_STATS_PASS,
      OMX_IndexConfigCameraEnableStatsPass),
   MMALOMX_PARAM_PASSTHROUGH(MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, MMAL_PARAMETER_UINT32_T,
      OMX_IndexParamCameraCustomSensorConfig, OMX_PARAM_U32TYPE),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_ENABLE_REGISTER_FILE,
      OMX_IndexConfigBrcmUseRegisterFile),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_REGISTER_FAIL_IS_FATAL,
      OMX_IndexConfigBrcmRegisterFileFailFatal),
   MMALOMX_PARAM_PASSTHROUGH_PORTLESS(MMAL_PARAMETER_CONFIGFILE_REGISTERS, MMAL_PARAMETER_CONFIGFILE_T,
      OMX_IndexParamBrcmConfigFileRegisters, OMX_PARAM_BRCMCONFIGFILETYPE),
   MMALOMX_PARAM_PASSTHROUGH_PORTLESS(MMAL_PARAMETER_CONFIGFILE_CHUNK_REGISTERS, MMAL_PARAMETER_CONFIGFILE_CHUNK_T,
      OMX_IndexParamBrcmConfigFileChunkRegisters, OMX_PARAM_BRCMCONFIGFILECHUNKTYPE),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_JPEG_ATTACH_LOG,
      OMX_IndexParamBrcmAttachLog),
   MMALOMX_PARAM_PASSTHROUGH_PORTLESS(MMAL_PARAMETER_ZERO_SHUTTER_LAG, MMAL_PARAMETER_ZEROSHUTTERLAG_T,
      OMX_IndexParamCameraZeroShutterLag, OMX_CONFIG_ZEROSHUTTERLAGTYPE),
   MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_FPS_RANGE, MMAL_PARAMETER_FPS_RANGE_T,
      OMX_IndexParamBrcmFpsRange, OMX_PARAM_BRCMFRAMERATERANGETYPE, mmalomx_param_mapping_fps_range),
   MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_CAPTURE_EXPOSURE_COMP, MMAL_PARAMETER_INT32_T,
      OMX_IndexParamCaptureExposureCompensation, OMX_PARAM_S32TYPE, mmalomx_param_mapping_ev_comp),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_SW_SHARPEN_DISABLE,
      OMX_IndexParamSWSharpenDisable),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_FLASH_REQUIRED,
      OMX_IndexConfigBrcmFlashRequired),
   MMALOMX_PARAM_BOOLEAN_PORTLESS(MMAL_PARAMETER_SW_SATURATION_DISABLE,
      OMX_IndexParamSWSaturationDisable),
   MMALOMX_PARAM_TERMINATE()
};

#if 0 /* Conversions which are still left to implement */
MMALOMX_PARAM_CUSTOM(MMAL_PARAMETER_CAMERA_CONFIG, MMAL_PARAMETER_CAMERA_CONFIG_T,
    0, 0, mmal_ril_param_set_cam_config),
MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_EXPOSURE_COMP, MMAL_PARAMETER_INT32_T,
   OMX_IndexConfigCommonExposureValue, OMX_CONFIG_EXPOSUREVALUETYPE, 0),
MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_EXP_METERING_MODE, MMAL_PARAMETER_EXPOSUREMETERINGMODE_T,
   OMX_IndexConfigCommonExposureValue, OMX_CONFIG_EXPOSUREVALUETYPE, 0),
MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_ISO, MMAL_PARAMETER_UINT32_T,
   OMX_IndexConfigCommonExposureValue, OMX_CONFIG_EXPOSUREVALUETYPE, 0),
MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_FOCUS_STATUS, MMAL_PARAMETER_FOCUS_STATUS_T,
   OMX_IndexConfigCommonFocusStatus, OMX_PARAM_FOCUSSTATUSTYPE, mmalomx_param_mapping_focus_status),
MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_EXIF, MMAL_PARAMETER_EXIF_T,
   OMX_IndexConfigMetadataItem, OMX_CONFIG_METADATAITEMTYPE, 0),
MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_FACE_TRACK_RESULTS, MMAL_PARAMETER_FACE_TRACK_RESULTS_T,
   OMX_IndexConfigCommonFaceDetectionRegion, OMX_CONFIG_FACEDETECTIONREGIONTYPE, 0),
MMALOMX_PARAM_STRAIGHT_MAPPING(MMAL_PARAMETER_ENABLE_RAW_CAPTURE, MMAL_PARAMETER_BOOLEAN_T,
   OMX_IndexConfigCaptureRawImageURI, OMX_PARAM_CONTENTURITYPE, 0),
MMALOMX_PARAM_PASSTHROUGH_PORTLESS(MMAL_PARAMETER_DPF_FILE, MMAL_PARAMETER_URI_T,
   OMX_IndexParamDynamicParameterFile, OMX_PARAM_CONTENTURITYPE),
MMALOMX_PARAM_PASSTHROUGH_PORTLESS(MMAL_PARAMETER_FOCUS_REGIONS, ,
   OMX_IndexConfigCommonFocusRegionXY, ),
#endif
