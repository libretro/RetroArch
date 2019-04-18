/*
 * Copyright (c) 2008 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/** @file OMX_Index.h - OpenMax IL version 1.1.2
 *  The OMX_Index header file contains the definitions for both applications
 *  and components .
 */

#ifndef OMX_Index_h
#define OMX_Index_h

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Each OMX header must include all required header files to allow the
 *  header to compile without errors.  The includes below are required
 *  for this header file to compile successfully
 */
#include "OMX_Types.h"

/** The OMX_INDEXTYPE enumeration is used to select a structure when either
 *  getting or setting parameters and/or configuration data.  Each entry in
 *  this enumeration maps to an OMX specified structure.  When the
 *  OMX_GetParameter, OMX_SetParameter, OMX_GetConfig or OMX_SetConfig methods
 *  are used, the second parameter will always be an entry from this enumeration
 *  and the third entry will be the structure shown in the comments for the entry.
 *  For example, if the application is initializing a cropping function, the
 *  OMX_SetConfig command would have OMX_IndexConfigCommonInputCrop as the second parameter
 *  and would send a pointer to an initialized OMX_RECTTYPE structure as the
 *  third parameter.
 *
 *  The enumeration entries named with the OMX_Config prefix are sent using
 *  the OMX_SetConfig command and the enumeration entries named with the
 *  OMX_PARAM_ prefix are sent using the OMX_SetParameter command.
 */
typedef enum OMX_INDEXTYPE {

    OMX_IndexComponentStartUnused = 0x01000000,
    OMX_IndexParamPriorityMgmt,             /**< reference: OMX_PRIORITYMGMTTYPE */
    OMX_IndexParamAudioInit,                /**< reference: OMX_PORT_PARAM_TYPE */
    OMX_IndexParamImageInit,                /**< reference: OMX_PORT_PARAM_TYPE */
    OMX_IndexParamVideoInit,                /**< reference: OMX_PORT_PARAM_TYPE */
    OMX_IndexParamOtherInit,                /**< reference: OMX_PORT_PARAM_TYPE */
    OMX_IndexParamNumAvailableStreams,      /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamActiveStream,             /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamSuspensionPolicy,         /**< reference: OMX_PARAM_SUSPENSIONPOLICYTYPE */
    OMX_IndexParamComponentSuspended,       /**< reference: OMX_PARAM_SUSPENSIONTYPE */
    OMX_IndexConfigCapturing,               /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigCaptureMode,             /**< reference: OMX_CONFIG_CAPTUREMODETYPE */
    OMX_IndexAutoPauseAfterCapture,         /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamContentURI,               /**< reference: OMX_PARAM_CONTENTURITYPE */
    OMX_IndexParamCustomContentPipe,        /**< reference: OMX_PARAM_CONTENTPIPETYPE */
    OMX_IndexParamDisableResourceConcealment, /**< reference: OMX_RESOURCECONCEALMENTTYPE */
    OMX_IndexConfigMetadataItemCount,       /**< reference: OMX_CONFIG_METADATAITEMCOUNTTYPE */
    OMX_IndexConfigContainerNodeCount,      /**< reference: OMX_CONFIG_CONTAINERNODECOUNTTYPE */
    OMX_IndexConfigMetadataItem,            /**< reference: OMX_CONFIG_METADATAITEMTYPE */
    OMX_IndexConfigCounterNodeID,           /**< reference: OMX_CONFIG_CONTAINERNODEIDTYPE */
    OMX_IndexParamMetadataFilterType,       /**< reference: OMX_PARAM_METADATAFILTERTYPE */
    OMX_IndexParamMetadataKeyFilter,        /**< reference: OMX_PARAM_METADATAFILTERTYPE */
    OMX_IndexConfigPriorityMgmt,            /**< reference: OMX_PRIORITYMGMTTYPE */
    OMX_IndexParamStandardComponentRole,    /**< reference: OMX_PARAM_COMPONENTROLETYPE */

    OMX_IndexPortStartUnused = 0x02000000,
    OMX_IndexParamPortDefinition,           /**< reference: OMX_PARAM_PORTDEFINITIONTYPE */
    OMX_IndexParamCompBufferSupplier,       /**< reference: OMX_PARAM_BUFFERSUPPLIERTYPE */
    OMX_IndexReservedStartUnused = 0x03000000,

    /* Audio parameters and configurations */
    OMX_IndexAudioStartUnused = 0x04000000,
    OMX_IndexParamAudioPortFormat,          /**< reference: OMX_AUDIO_PARAM_PORTFORMATTYPE */
    OMX_IndexParamAudioPcm,                 /**< reference: OMX_AUDIO_PARAM_PCMMODETYPE */
    OMX_IndexParamAudioAac,                 /**< reference: OMX_AUDIO_PARAM_AACPROFILETYPE */
    OMX_IndexParamAudioRa,                  /**< reference: OMX_AUDIO_PARAM_RATYPE */
    OMX_IndexParamAudioMp3,                 /**< reference: OMX_AUDIO_PARAM_MP3TYPE */
    OMX_IndexParamAudioAdpcm,               /**< reference: OMX_AUDIO_PARAM_ADPCMTYPE */
    OMX_IndexParamAudioG723,                /**< reference: OMX_AUDIO_PARAM_G723TYPE */
    OMX_IndexParamAudioG729,                /**< reference: OMX_AUDIO_PARAM_G729TYPE */
    OMX_IndexParamAudioAmr,                 /**< reference: OMX_AUDIO_PARAM_AMRTYPE */
    OMX_IndexParamAudioWma,                 /**< reference: OMX_AUDIO_PARAM_WMATYPE */
    OMX_IndexParamAudioSbc,                 /**< reference: OMX_AUDIO_PARAM_SBCTYPE */
    OMX_IndexParamAudioMidi,                /**< reference: OMX_AUDIO_PARAM_MIDITYPE */
    OMX_IndexParamAudioGsm_FR,              /**< reference: OMX_AUDIO_PARAM_GSMFRTYPE */
    OMX_IndexParamAudioMidiLoadUserSound,   /**< reference: OMX_AUDIO_PARAM_MIDILOADUSERSOUNDTYPE */
    OMX_IndexParamAudioG726,                /**< reference: OMX_AUDIO_PARAM_G726TYPE */
    OMX_IndexParamAudioGsm_EFR,             /**< reference: OMX_AUDIO_PARAM_GSMEFRTYPE */
    OMX_IndexParamAudioGsm_HR,              /**< reference: OMX_AUDIO_PARAM_GSMHRTYPE */
    OMX_IndexParamAudioPdc_FR,              /**< reference: OMX_AUDIO_PARAM_PDCFRTYPE */
    OMX_IndexParamAudioPdc_EFR,             /**< reference: OMX_AUDIO_PARAM_PDCEFRTYPE */
    OMX_IndexParamAudioPdc_HR,              /**< reference: OMX_AUDIO_PARAM_PDCHRTYPE */
    OMX_IndexParamAudioTdma_FR,             /**< reference: OMX_AUDIO_PARAM_TDMAFRTYPE */
    OMX_IndexParamAudioTdma_EFR,            /**< reference: OMX_AUDIO_PARAM_TDMAEFRTYPE */
    OMX_IndexParamAudioQcelp8,              /**< reference: OMX_AUDIO_PARAM_QCELP8TYPE */
    OMX_IndexParamAudioQcelp13,             /**< reference: OMX_AUDIO_PARAM_QCELP13TYPE */
    OMX_IndexParamAudioEvrc,                /**< reference: OMX_AUDIO_PARAM_EVRCTYPE */
    OMX_IndexParamAudioSmv,                 /**< reference: OMX_AUDIO_PARAM_SMVTYPE */
    OMX_IndexParamAudioVorbis,              /**< reference: OMX_AUDIO_PARAM_VORBISTYPE */

    OMX_IndexConfigAudioMidiImmediateEvent, /**< reference: OMX_AUDIO_CONFIG_MIDIIMMEDIATEEVENTTYPE */
    OMX_IndexConfigAudioMidiControl,        /**< reference: OMX_AUDIO_CONFIG_MIDICONTROLTYPE */
    OMX_IndexConfigAudioMidiSoundBankProgram, /**< reference: OMX_AUDIO_CONFIG_MIDISOUNDBANKPROGRAMTYPE */
    OMX_IndexConfigAudioMidiStatus,         /**< reference: OMX_AUDIO_CONFIG_MIDISTATUSTYPE */
    OMX_IndexConfigAudioMidiMetaEvent,      /**< reference: OMX_AUDIO_CONFIG_MIDIMETAEVENTTYPE */
    OMX_IndexConfigAudioMidiMetaEventData,  /**< reference: OMX_AUDIO_CONFIG_MIDIMETAEVENTDATATYPE */
    OMX_IndexConfigAudioVolume,             /**< reference: OMX_AUDIO_CONFIG_VOLUMETYPE */
    OMX_IndexConfigAudioBalance,            /**< reference: OMX_AUDIO_CONFIG_BALANCETYPE */
    OMX_IndexConfigAudioChannelMute,        /**< reference: OMX_AUDIO_CONFIG_CHANNELMUTETYPE */
    OMX_IndexConfigAudioMute,               /**< reference: OMX_AUDIO_CONFIG_MUTETYPE */
    OMX_IndexConfigAudioLoudness,           /**< reference: OMX_AUDIO_CONFIG_LOUDNESSTYPE */
    OMX_IndexConfigAudioEchoCancelation,    /**< reference: OMX_AUDIO_CONFIG_ECHOCANCELATIONTYPE */
    OMX_IndexConfigAudioNoiseReduction,     /**< reference: OMX_AUDIO_CONFIG_NOISEREDUCTIONTYPE */
    OMX_IndexConfigAudioBass,               /**< reference: OMX_AUDIO_CONFIG_BASSTYPE */
    OMX_IndexConfigAudioTreble,             /**< reference: OMX_AUDIO_CONFIG_TREBLETYPE */
    OMX_IndexConfigAudioStereoWidening,     /**< reference: OMX_AUDIO_CONFIG_STEREOWIDENINGTYPE */
    OMX_IndexConfigAudioChorus,             /**< reference: OMX_AUDIO_CONFIG_CHORUSTYPE */
    OMX_IndexConfigAudioEqualizer,          /**< reference: OMX_AUDIO_CONFIG_EQUALIZERTYPE */
    OMX_IndexConfigAudioReverberation,      /**< reference: OMX_AUDIO_CONFIG_REVERBERATIONTYPE */
    OMX_IndexConfigAudioChannelVolume,      /**< reference: OMX_AUDIO_CONFIG_CHANNELVOLUMETYPE */

    /* Image specific parameters and configurations */
    OMX_IndexImageStartUnused = 0x05000000,
    OMX_IndexParamImagePortFormat,          /**< reference: OMX_IMAGE_PARAM_PORTFORMATTYPE */
    OMX_IndexParamFlashControl,             /**< reference: OMX_IMAGE_PARAM_FLASHCONTROLTYPE */
    OMX_IndexConfigFocusControl,            /**< reference: OMX_IMAGE_CONFIG_FOCUSCONTROLTYPE */
    OMX_IndexParamQFactor,                  /**< reference: OMX_IMAGE_PARAM_QFACTORTYPE */
    OMX_IndexParamQuantizationTable,        /**< reference: OMX_IMAGE_PARAM_QUANTIZATIONTABLETYPE */
    OMX_IndexParamHuffmanTable,             /**< reference: OMX_IMAGE_PARAM_HUFFMANTTABLETYPE */
    OMX_IndexConfigFlashControl,            /**< reference: OMX_IMAGE_PARAM_FLASHCONTROLTYPE */

    /* Video specific parameters and configurations */
    OMX_IndexVideoStartUnused = 0x06000000,
    OMX_IndexParamVideoPortFormat,          /**< reference: OMX_VIDEO_PARAM_PORTFORMATTYPE */
    OMX_IndexParamVideoQuantization,        /**< reference: OMX_VIDEO_PARAM_QUANTIZATIONTYPE */
    OMX_IndexParamVideoFastUpdate,          /**< reference: OMX_VIDEO_PARAM_VIDEOFASTUPDATETYPE */
    OMX_IndexParamVideoBitrate,             /**< reference: OMX_VIDEO_PARAM_BITRATETYPE */
    OMX_IndexParamVideoMotionVector,        /**< reference: OMX_VIDEO_PARAM_MOTIONVECTORTYPE */
    OMX_IndexParamVideoIntraRefresh,        /**< reference: OMX_VIDEO_PARAM_INTRAREFRESHTYPE */
    OMX_IndexParamVideoErrorCorrection,     /**< reference: OMX_VIDEO_PARAM_ERRORCORRECTIONTYPE */
    OMX_IndexParamVideoVBSMC,               /**< reference: OMX_VIDEO_PARAM_VBSMCTYPE */
    OMX_IndexParamVideoMpeg2,               /**< reference: OMX_VIDEO_PARAM_MPEG2TYPE */
    OMX_IndexParamVideoMpeg4,               /**< reference: OMX_VIDEO_PARAM_MPEG4TYPE */
    OMX_IndexParamVideoWmv,                 /**< reference: OMX_VIDEO_PARAM_WMVTYPE */
    OMX_IndexParamVideoRv,                  /**< reference: OMX_VIDEO_PARAM_RVTYPE */
    OMX_IndexParamVideoAvc,                 /**< reference: OMX_VIDEO_PARAM_AVCTYPE */
    OMX_IndexParamVideoH263,                /**< reference: OMX_VIDEO_PARAM_H263TYPE */
    OMX_IndexParamVideoProfileLevelQuerySupported, /**< reference: OMX_VIDEO_PARAM_PROFILELEVELTYPE */
    OMX_IndexParamVideoProfileLevelCurrent, /**< reference: OMX_VIDEO_PARAM_PROFILELEVELTYPE */
    OMX_IndexConfigVideoBitrate,            /**< reference: OMX_VIDEO_CONFIG_BITRATETYPE */
    OMX_IndexConfigVideoFramerate,          /**< reference: OMX_CONFIG_FRAMERATETYPE */
    OMX_IndexConfigVideoIntraVOPRefresh,    /**< reference: OMX_CONFIG_INTRAREFRESHVOPTYPE */
    OMX_IndexConfigVideoIntraMBRefresh,     /**< reference: OMX_CONFIG_MACROBLOCKERRORMAPTYPE */
    OMX_IndexConfigVideoMBErrorReporting,   /**< reference: OMX_CONFIG_MBERRORREPORTINGTYPE */
    OMX_IndexParamVideoMacroblocksPerFrame, /**< reference: OMX_PARAM_MACROBLOCKSTYPE */
    OMX_IndexConfigVideoMacroBlockErrorMap, /**< reference: OMX_CONFIG_MACROBLOCKERRORMAPTYPE */
    OMX_IndexParamVideoSliceFMO,            /**< reference: OMX_VIDEO_PARAM_AVCSLICEFMO */
    OMX_IndexConfigVideoAVCIntraPeriod,     /**< reference: OMX_VIDEO_CONFIG_AVCINTRAPERIOD */
    OMX_IndexConfigVideoNalSize,            /**< reference: OMX_VIDEO_CONFIG_NALSIZE */

    /* Image & Video common Configurations */
    OMX_IndexCommonStartUnused = 0x07000000,
    OMX_IndexParamCommonDeblocking,         /**< reference: OMX_PARAM_DEBLOCKINGTYPE */
    OMX_IndexParamCommonSensorMode,         /**< reference: OMX_PARAM_SENSORMODETYPE */
    OMX_IndexParamCommonInterleave,         /**< reference: OMX_PARAM_INTERLEAVETYPE */
    OMX_IndexConfigCommonColorFormatConversion, /**< reference: OMX_CONFIG_COLORCONVERSIONTYPE */
    OMX_IndexConfigCommonScale,             /**< reference: OMX_CONFIG_SCALEFACTORTYPE */
    OMX_IndexConfigCommonImageFilter,       /**< reference: OMX_CONFIG_IMAGEFILTERTYPE */
    OMX_IndexConfigCommonColorEnhancement,  /**< reference: OMX_CONFIG_COLORENHANCEMENTTYPE */
    OMX_IndexConfigCommonColorKey,          /**< reference: OMX_CONFIG_COLORKEYTYPE */
    OMX_IndexConfigCommonColorBlend,        /**< reference: OMX_CONFIG_COLORBLENDTYPE */
    OMX_IndexConfigCommonFrameStabilisation,/**< reference: OMX_CONFIG_FRAMESTABTYPE */
    OMX_IndexConfigCommonRotate,            /**< reference: OMX_CONFIG_ROTATIONTYPE */
    OMX_IndexConfigCommonMirror,            /**< reference: OMX_CONFIG_MIRRORTYPE */
    OMX_IndexConfigCommonOutputPosition,    /**< reference: OMX_CONFIG_POINTTYPE */
    OMX_IndexConfigCommonInputCrop,         /**< reference: OMX_CONFIG_RECTTYPE */
    OMX_IndexConfigCommonOutputCrop,        /**< reference: OMX_CONFIG_RECTTYPE */
    OMX_IndexConfigCommonDigitalZoom,       /**< reference: OMX_CONFIG_SCALEFACTORTYPE */
    OMX_IndexConfigCommonOpticalZoom,       /**< reference: OMX_CONFIG_SCALEFACTORTYPE*/
    OMX_IndexConfigCommonWhiteBalance,      /**< reference: OMX_CONFIG_WHITEBALCONTROLTYPE */
    OMX_IndexConfigCommonExposure,          /**< reference: OMX_CONFIG_EXPOSURECONTROLTYPE */
    OMX_IndexConfigCommonContrast,          /**< reference: OMX_CONFIG_CONTRASTTYPE */
    OMX_IndexConfigCommonBrightness,        /**< reference: OMX_CONFIG_BRIGHTNESSTYPE */
    OMX_IndexConfigCommonBacklight,         /**< reference: OMX_CONFIG_BACKLIGHTTYPE */
    OMX_IndexConfigCommonGamma,             /**< reference: OMX_CONFIG_GAMMATYPE */
    OMX_IndexConfigCommonSaturation,        /**< reference: OMX_CONFIG_SATURATIONTYPE */
    OMX_IndexConfigCommonLightness,         /**< reference: OMX_CONFIG_LIGHTNESSTYPE */
    OMX_IndexConfigCommonExclusionRect,     /**< reference: OMX_CONFIG_RECTTYPE */
    OMX_IndexConfigCommonDithering,         /**< reference: OMX_CONFIG_DITHERTYPE */
    OMX_IndexConfigCommonPlaneBlend,        /**< reference: OMX_CONFIG_PLANEBLENDTYPE */
    OMX_IndexConfigCommonExposureValue,     /**< reference: OMX_CONFIG_EXPOSUREVALUETYPE */
    OMX_IndexConfigCommonOutputSize,        /**< reference: OMX_FRAMESIZETYPE */
    OMX_IndexParamCommonExtraQuantData,     /**< reference: OMX_OTHER_EXTRADATATYPE */
    OMX_IndexConfigCommonFocusRegion,       /**< reference: OMX_CONFIG_FOCUSREGIONTYPE */
    OMX_IndexConfigCommonFocusStatus,       /**< reference: OMX_PARAM_FOCUSSTATUSTYPE */
    OMX_IndexConfigCommonTransitionEffect,  /**< reference: OMX_CONFIG_TRANSITIONEFFECTTYPE */

    /* Reserved Configuration range */
    OMX_IndexOtherStartUnused = 0x08000000,
    OMX_IndexParamOtherPortFormat,          /**< reference: OMX_OTHER_PARAM_PORTFORMATTYPE */
    OMX_IndexConfigOtherPower,              /**< reference: OMX_OTHER_CONFIG_POWERTYPE */
    OMX_IndexConfigOtherStats,              /**< reference: OMX_OTHER_CONFIG_STATSTYPE */

    /* Reserved Time range */
    OMX_IndexTimeStartUnused = 0x09000000,
    OMX_IndexConfigTimeScale,               /**< reference: OMX_TIME_CONFIG_SCALETYPE */
    OMX_IndexConfigTimeClockState,          /**< reference: OMX_TIME_CONFIG_CLOCKSTATETYPE */
    OMX_IndexConfigTimeActiveRefClock,      /**< reference: OMX_TIME_CONFIG_ACTIVEREFCLOCKTYPE */
    OMX_IndexConfigTimeCurrentMediaTime,    /**< reference: OMX_TIME_CONFIG_TIMESTAMPTYPE (read only) */
    OMX_IndexConfigTimeCurrentWallTime,     /**< reference: OMX_TIME_CONFIG_TIMESTAMPTYPE (read only) */
    OMX_IndexConfigTimeCurrentAudioReference, /**< reference: OMX_TIME_CONFIG_TIMESTAMPTYPE (write only) */
    OMX_IndexConfigTimeCurrentVideoReference, /**< reference: OMX_TIME_CONFIG_TIMESTAMPTYPE (write only) */
    OMX_IndexConfigTimeMediaTimeRequest,    /**< reference: OMX_TIME_CONFIG_MEDIATIMEREQUESTTYPE (write only) */
    OMX_IndexConfigTimeClientStartTime,     /**<reference:  OMX_TIME_CONFIG_TIMESTAMPTYPE (write only) */
    OMX_IndexConfigTimePosition,            /**< reference: OMX_TIME_CONFIG_TIMESTAMPTYPE */
    OMX_IndexConfigTimeSeekMode,            /**< reference: OMX_TIME_CONFIG_SEEKMODETYPE */

    OMX_IndexKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    /* Vendor specific area */
    OMX_IndexVendorStartUnused = 0x7F000000,
    /* Vendor specific structures should be in the range of 0x7F000000
       to 0x7FFFFFFE.  This range is not broken out by vendor, so
       private indexes are not guaranteed unique and therefore should
       only be sent to the appropriate component. */

    /* used for ilcs-top communication */
    OMX_IndexParamMarkComparison,           /**< reference: OMX_PARAM_MARKCOMPARISONTYPE */
    OMX_IndexParamPortSummary,              /**< reference: OMX_PARAM_PORTSUMMARYTYPE */
    OMX_IndexParamTunnelStatus,             /**< reference : OMX_PARAM_TUNNELSTATUSTYPE */
    OMX_IndexParamBrcmRecursionUnsafe,      /**< reference: OMX_PARAM_BRCMRECURSIONUNSAFETYPE */

    /* used for top-ril communication */
    OMX_IndexParamBufferAddress,            /**< reference : OMX_PARAM_BUFFERADDRESSTYPE */
    OMX_IndexParamTunnelSetup,              /**< reference : OMX_PARAM_TUNNELSETUPTYPE */
    OMX_IndexParamBrcmPortEGL,              /**< reference : OMX_PARAM_BRCMPORTEGLTYPE */
    OMX_IndexParamIdleResourceCount,        /**< reference : OMX_PARAM_U32TYPE */

    /* used for ril-ril communication */
    OMX_IndexParamImagePoolDisplayFunction, /**<reference : OMX_PARAM_IMAGEDISPLAYFUNCTIONTYPE */
    OMX_IndexParamBrcmDataUnit,             /**<reference: OMX_PARAM_DATAUNITTYPE */
    OMX_IndexParamCodecConfig,              /**<reference: OMX_PARAM_CODECCONFIGTYPE */
    OMX_IndexParamCameraPoolToEncoderFunction, /**<reference : OMX_PARAM_CAMERAPOOLTOENCODERFUNCTIONTYPE */
    OMX_IndexParamCameraStripeFunction,     /**<reference : OMX_PARAM_CAMERASTRIPEFUNCTIONTYPE */
    OMX_IndexParamCameraCaptureEventFunction, /**<reference : OMX_PARAM_CAMERACAPTUREEVENTFUNCTIONTYPE */

    /* used for client-ril communication */
    OMX_IndexParamTestInterface,            /**< reference : OMX_PARAM_TESTINTERFACETYPE */

    // 0x7f000010
    OMX_IndexConfigDisplayRegion,           /**< reference : OMX_CONFIG_DISPLAYREGIONTYPE */
    OMX_IndexParamSource,                   /**< reference : OMX_PARAM_SOURCETYPE */
    OMX_IndexParamSourceSeed,               /**< reference : OMX_PARAM_SOURCESEEDTYPE */
    OMX_IndexParamResize,                   /**< reference : OMX_PARAM_RESIZETYPE */
    OMX_IndexConfigVisualisation,           /**< reference : OMX_CONFIG_VISUALISATIONTYPE */
    OMX_IndexConfigSingleStep,              /**<reference : OMX_PARAM_U32TYPE */
    OMX_IndexConfigPlayMode,                /**<reference: OMX_CONFIG_PLAYMODETYPE */
    OMX_IndexParamCameraCamplusId,          /**<reference : OMX_PARAM_U32TYPE */
    OMX_IndexConfigCommonImageFilterParameters,  /**<reference : OMX_CONFIG_IMAGEFILTERPARAMSTYPE */
    OMX_IndexConfigTransitionControl,       /**<reference : OMX_CONFIG_TRANSITIONCONTROLTYPE */
    OMX_IndexConfigPresentationOffset,      /**<reference: OMX_TIME_CONFIG_TIMESTAMPTYPE */
    OMX_IndexParamSourceFunctions,          /**<reference: OMX_PARAM_STILLSFUNCTIONTYPE */
    OMX_IndexConfigAudioMonoTrackControl,   /**<reference : OMX_CONFIG_AUDIOMONOTRACKCONTROLTYPE */
    OMX_IndexParamCameraImagePool,          /**<reference : OMX_PARAM_CAMERAIMAGEPOOLTYPE */
    OMX_IndexConfigCameraISPOutputPoolHeight,/**<reference : OMX_PARAM_U32TYPE */
    OMX_IndexParamImagePoolSize,            /**<reference: OMX_PARAM_IMAGEPOOLSIZETYPE */

    // 0x7f000020
    OMX_IndexParamImagePoolExternal,        /**<reference: OMX_PARAM_IMAGEPOOLEXTERNALTYPE */
    OMX_IndexParamRUTILFifoInfo,            /**<reference: OMX_PARAM_RUTILFIFOINFOTYPE*/
    OMX_IndexParamILFifoConfig,             /**<reference: OMX_PARAM_ILFIFOCONFIG */
    OMX_IndexConfigCameraSensorModes,       /**<reference : OMX_CONFIG_CAMERASENSORMODETYPE */
    OMX_IndexConfigBrcmPortStats,           /**<reference : OMX_CONFIG_BRCMPORTSTATSTYPE */
    OMX_IndexConfigBrcmPortBufferStats,     /**<reference : OMX_CONFIG_BRCMPORTBUFFERSTATSTYPE */
    OMX_IndexConfigBrcmCameraStats,         /**<reference : OMX_CONFIG_BRCMCAMERASTATSTYPE */
    OMX_IndexConfigBrcmIOPerfStats,         /**<reference : OMX_CONFIG_BRCMIOPERFSTATSTYPE */
    OMX_IndexConfigCommonSharpness,         /**<reference : OMX_CONFIG_SHARPNESSTYPE */
    OMX_IndexConfigCommonFlickerCancellation,   /**reference : OMX_CONFIG_FLICKERCANCELTYPE */
    OMX_IndexParamCameraSwapImagePools,     /**<reference : OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamCameraSingleBufferCaptureInput,  /**<reference : OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigCommonRedEyeRemoval,   /**<reference : OMX_CONFIG_REDEYEREMOVALTYPE  */
    OMX_IndexConfigCommonFaceDetectionControl,  /**<reference : OMX_CONFIG_FACEDETECTIONCONTROLTYPE */
    OMX_IndexConfigCommonFaceDetectionRegion,   /**<reference : OMX_CONFIG_FACEDETECTIONREGIONTYPE */
    OMX_IndexConfigCommonInterlace,         /**<reference: OMX_CONFIG_INTERLACETYPE */

    // 0x7f000030
    OMX_IndexParamISPTunerName,             /**<reference: OMX_PARAM_CAMERAISPTUNERTYPE */
    OMX_IndexParamCameraDeviceNumber,       /**<reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamCameraDevicesPresent,     /**<reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigCameraInputFrame,        /**<reference: OMX_CONFIG_IMAGEPTRTYPE */
    OMX_IndexConfigStillColourDenoiseEnable,    /**<reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigVideoColourDenoiseEnable,    /**<reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigAFAssistLight,           /**<reference: OMX_CONFIG_AFASSISTTYPE */
    OMX_IndexConfigSmartShakeReductionEnable, /**<reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigInputCropPercentages,    /**<reference: OMX_CONFIG_INPUTCROPTYPE */
    OMX_IndexConfigStillsAntiShakeEnable,   /**<reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigWaitForFocusBeforeCapture,/**<reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigAudioRenderingLatency,   /**<reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigDrawBoxAroundFaces,      /**<reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamCodecRequirements,        /**<reference: OMX_PARAM_CODECREQUIREMENTSTYPE */
    OMX_IndexConfigBrcmEGLImageMemHandle,   /**<reference: OMX_CONFIG_BRCMEGLIMAGEMEMHANDLETYPE */
    OMX_IndexConfigPrivacyIndicator,        /**<reference: OMX_CONFIG_PRIVACYINDICATORTYPE */

    // 0x7f000040
    OMX_IndexParamCameraFlashType,          /**<reference: OMX_PARAM_CAMERAFLASHTYPE */
    OMX_IndexConfigCameraEnableStatsPass,   /**<reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigCameraFlashConfig,       /**<reference: OMX_CONFIG_CAMERAFLASHCONFIGTYPE */
    OMX_IndexConfigCaptureRawImageURI,      /**<reference: OMX_PARAM_CONTENTURITYPE */
    OMX_IndexConfigCameraStripeFuncMinLines, /**<reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigCameraAlgorithmVersionDeprecated,   /**<reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigCameraIsoReferenceValue,  /**<reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigCameraCaptureAbortsAutoFocus, /**<reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigBrcmClockMissCount,      /**<reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigFlashChargeLevel,         /**<reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigBrcmVideoEncodedSliceSize, /**<reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigBrcmAudioTrackGaplessPlayback,  /**< reference: OMX_CONFIG_BRCMAUDIOTRACKGAPLESSPLAYBACKTYPE */
    OMX_IndexConfigBrcmAudioTrackChangeControl,    /**< reference: OMX_CONFIG_BRCMAUDIOTRACKCHANGECONTROLTYPE */
    OMX_IndexParamBrcmPixelAspectRatio,     /**< reference: OMX_CONFIG_POINTTYPE */
    OMX_IndexParamBrcmPixelValueRange,      /**< reference: OMX_PARAM_BRCMPIXELVALUERANGETYPE */
    OMX_IndexParamCameraDisableAlgorithm,   /**< reference: OMX_PARAM_CAMERADISABLEALGORITHMTYPE */

    // 0x7f000050
    OMX_IndexConfigBrcmVideoIntraPeriodTime, /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigBrcmVideoIntraPeriod,     /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigBrcmAudioEffectControl, /**< reference: OMX_CONFIG_BRCMAUDIOEFFECTCONTROLTYPE */
    OMX_IndexConfigBrcmMinimumProcessingLatency, /**< reference: OMX_CONFIG_BRCMMINIMUMPROCESSINGLATENCY */
    OMX_IndexParamBrcmVideoAVCSEIEnable,    /**< reference: OMX_PARAM_BRCMVIDEOAVCSEIENABLETYPE */
    OMX_IndexParamBrcmAllowMemChange,   /**< reference: OMX_PARAM_BRCMALLOWMEMCHANGETYPE */
    OMX_IndexConfigBrcmVideoEncoderMBRowsPerSlice, /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamCameraAFAssistDeviceNumber_Deprecated,   /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamCameraPrivacyIndicatorDeviceNumber_Deprecated,   /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigCameraUseCase,               /**< reference: OMX_CONFIG_CAMERAUSECASETYPE */
    OMX_IndexParamBrcmDisableProprietaryTunnels,   /**< reference: OMX_PARAM_BRCMDISABLEPROPRIETARYTUNNELSTYPE */
    OMX_IndexParamBrcmOutputBufferSize,         /**<  reference: OMX_PARAM_BRCMOUTPUTBUFFERSIZETYPE */
    OMX_IndexParamBrcmRetainMemory,             /**< reference: OMX_PARAM_BRCMRETAINMEMORYTYPE */
    OMX_IndexConfigCanFocus_Deprecated,                    /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamBrcmImmutableInput,           /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamDynamicParameterFile,        /**< reference: OMX_PARAM_CONTENTURITYPE */

    // 0x7f000060
    OMX_IndexParamUseDynamicParameterFile,     /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigCameraInfo,                 /**< reference: OMX_CONFIG_CAMERAINFOTYPE */
    OMX_IndexConfigCameraFeatures,             /**< reference: OMX_CONFIG_CAMERAFEATURESTYPE */
    OMX_IndexConfigRequestCallback,            /**< reference: OMX_CONFIG_REQUESTCALLBACKTYPE */ //Should be added to the spec as part of IL416c
    OMX_IndexConfigBrcmOutputBufferFullCount,  /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigCommonFocusRegionXY,        /**< reference: OMX_CONFIG_FOCUSREGIONXYTYPE */
    OMX_IndexParamBrcmDisableEXIF,             /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigUserSettingsId,             /**< reference: OMX_CONFIG_U8TYPE */
    OMX_IndexConfigCameraSettings,             /**< reference: OMX_CONFIG_CAMERASETTINGSTYPE */
    OMX_IndexConfigDrawBoxLineParams,          /**< reference: OMX_CONFIG_DRAWBOXLINEPARAMS */
    OMX_IndexParamCameraRmiControl_Deprecated,            /**< reference: OMX_PARAM_CAMERARMITYPE */
    OMX_IndexConfigBurstCapture,               /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamBrcmEnableIJGTableScaling,   /**< reference: OMX_PARAM_IJGSCALINGTYPE */
    OMX_IndexConfigPowerDown,                  /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigBrcmSyncOutput,             /**< reference: OMX_CONFIG_BRCMSYNCOUTPUTTYPE */
    OMX_IndexParamBrcmFlushCallback,           /**< reference: OMX_PARAM_BRCMFLUSHCALLBACK */

    // 0x7f000070
    OMX_IndexConfigBrcmVideoRequestIFrame,     /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamBrcmNALSSeparate,            /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigConfirmView,                /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigDrmView,                    /**< reference: OMX_CONFIG_DRMVIEWTYPE */
    OMX_IndexConfigBrcmVideoIntraRefresh,      /**< reference: OMX_VIDEO_PARAM_INTRAREFRESHTYPE */
    OMX_IndexParamBrcmMaxFileSize,             /**< reference: OMX_PARAM_BRCMU64TYPE */
    OMX_IndexParamBrcmCRCEnable,               /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamBrcmCRC,                     /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigCameraRmiInUse_Deprecated,             /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigBrcmAudioSource,            /**<reference: OMX_CONFIG_BRCMAUDIOSOURCETYPE */
    OMX_IndexConfigBrcmAudioDestination,       /**< reference: OMX_CONFIG_BRCMAUDIODESTINATIONTYPE */
    OMX_IndexParamAudioDdp,                    /**< reference: OMX_AUDIO_PARAM_DDPTYPE */
    OMX_IndexParamBrcmThumbnail,               /**< reference: OMX_PARAM_BRCMTHUMBNAILTYPE */
    OMX_IndexParamBrcmDisableLegacyBlocks_Deprecated,     /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamBrcmCameraInputAspectRatio,  /**< reference: OMX_PARAM_BRCMASPECTRATIOTYPE */
    OMX_IndexParamDynamicParameterFileFailFatal,/**< reference: OMX_CONFIG_BOOLEANTYPE */

    // 0x7f000080
    OMX_IndexParamBrcmVideoDecodeErrorConcealment, /**< reference: OMX_PARAM_BRCMVIDEODECODEERRORCONCEALMENTTYPE */
    OMX_IndexParamBrcmInterpolateMissingTimestamps, /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamBrcmSetCodecPerformanceMonitoring, /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigFlashInfo,                  /**< reference: OMX_CONFIG_FLASHINFOTYPE */
    OMX_IndexParamBrcmMaxFrameSkips,           /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigDynamicRangeExpansion,      /**< reference: OMX_CONFIG_DYNAMICRANGEEXPANSIONTYPE */
    OMX_IndexParamBrcmFlushCallbackId,         /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamBrcmTransposeBufferCount,    /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigFaceRecognitionControl,     /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigFaceRecognitionSaveFace,    /**< reference: OMX_PARAM_BRCMU64TYPE */
    OMX_IndexConfigFaceRecognitionDatabaseUri, /**< reference: OMX_PARAM_CONTENTURITYPE */
    OMX_IndexConfigClockAdjustment,            /**< reference: OMX_TIME_CONFIG_TIMESTAMPTYPE */
    OMX_IndexParamBrcmThreadAffinity,          /**< reference: OMX_PARAM_BRCMTHREADAFFINITYTYPE */
    OMX_IndexParamAsynchronousOutput,          /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigAsynchronousFailureURI,     /**< reference: OMX_PARAM_CONTENTURITYPE */
    OMX_IndexConfigCommonFaceBeautification,   /**< reference: OMX_CONFIG_BOOLEANTYPE */

    // 0x7f000090
    OMX_IndexConfigCommonSceneDetectionControl,/**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigCommonSceneDetected,        /**< reference: OMX_CONFIG_SCENEDETECTTYPE */
    OMX_IndexParamDisableVllPool,              /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamVideoMvc,                    /**< reference: OMX_VIDEO_PARAM_MVCTYPE */
    OMX_IndexConfigBrcmDrawStaticBox,          /**< reference: OMX_CONFIG_STATICBOXTYPE */
    OMX_IndexConfigBrcmClockReferenceSource,   /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamPassBufferMarks,             /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigPortCapturing,              /**< reference: OMX_CONFIG_PORTBOOLEANTYPE */
    OMX_IndexConfigBrcmDecoderPassThrough,     /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamBrcmDecoderPassThrough=OMX_IndexConfigBrcmDecoderPassThrough,  /* deprecated */
    OMX_IndexParamBrcmMaxCorruptMBs,           /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigBrcmGlobalAudioMute,        /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamCameraCaptureMode,           /**< reference: OMX_PARAM_CAMERACAPTUREMODETYPE */
    OMX_IndexParamBrcmDrmEncryption,           /**< reference: OMX_PARAM_BRCMDRMENCRYPTIONTYPE */
    OMX_IndexConfigBrcmCameraRnDPreprocess,    /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigBrcmCameraRnDPostprocess,   /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigBrcmAudioTrackChangeCount,  /**< reference: OMX_PARAM_U32TYPE */

    // 0x7f0000a0
    OMX_IndexParamCommonUseStcTimestamps,      /**< reference: OMX_PARAM_TIMESTAMPMODETYPE */
    OMX_IndexConfigBufferStall,                /**< reference: OMX_CONFIG_BUFFERSTALLTYPE */
    OMX_IndexConfigRefreshCodec,               /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamCaptureStatus,               /**< reference: OMX_PARAM_CAPTURESTATETYPE */
    OMX_IndexConfigTimeInvalidStartTime,       /**< reference: OMX_TIME_CONFIG_TIMESTAMPTYPE */
    OMX_IndexConfigLatencyTarget,              /**< reference: OMX_CONFIG_LATENCYTARGETTYPE */
    OMX_IndexConfigMinimiseFragmentation,      /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigBrcmUseProprietaryCallback, /**< reference: OMX_CONFIG_BRCMUSEPROPRIETARYTUNNELTYPE */
    OMX_IndexParamPortMaxFrameSize,            /**< reference: OMX_FRAMESIZETYPE */
    OMX_IndexParamComponentName,               /**< reference: OMX_PARAM_COMPONENTROLETYPE */
    OMX_IndexConfigEncLevelExtension,          /**< reference: OMX_VIDEO_CONFIG_LEVEL_EXTEND */
    OMX_IndexConfigTemporalDenoiseEnable,      /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamBrcmLazyImagePoolDestroy,    /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamBrcmEEDEEnable,              /**< reference: OMX_VIDEO_EEDE_ENABLE */
    OMX_IndexParamBrcmEEDELossRate,            /**< reference: OMX_VIDEO_EEDE_LOSSRATE */
    OMX_IndexParamAudioDts,                    /**< reference: OMX_AUDIO_PARAM_DTSTYPE */

    // 0x7f0000b0
    OMX_IndexParamNumOutputChannels,           /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigBrcmHighDynamicRange,       /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigBrcmPoolMemAllocSize,       /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigBrcmBufferFlagFilter,       /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamBrcmVideoEncodeMinQuant,     /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamBrcmVideoEncodeMaxQuant,     /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamRateControlModel,            /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamBrcmExtraBuffers,            /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigFieldOfView,                /**< reference: OMX_CONFIG_BRCMFOVTYPE */
    OMX_IndexParamBrcmAlignHoriz,              /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamBrcmAlignVert,               /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamColorSpace,                  /**< reference: OMX_PARAM_COLORSPACETYPE */
    OMX_IndexParamBrcmDroppablePFrames,        /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamBrcmVideoInitialQuant,       /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamBrcmVideoEncodeQpP,          /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamBrcmVideoRCSliceDQuant,      /**< reference: OMX_PARAM_U32TYPE */

    // 0x7f0000c0
    OMX_IndexParamBrcmVideoFrameLimitBits,     /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamBrcmVideoPeakRate,           /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigBrcmVideoH264DisableCABAC,  /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigBrcmVideoH264LowLatency,    /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigBrcmVideoH264AUDelimiters,  /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigBrcmVideoH264DeblockIDC,    /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigBrcmVideoH264IntraMBMode,   /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexConfigContrastEnhance,            /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamCameraCustomSensorConfig,    /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamBrcmHeaderOnOpen,            /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigBrcmUseRegisterFile,        /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigBrcmRegisterFileFailFatal,  /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamBrcmConfigFileRegisters,     /**< reference: OMX_PARAM_BRCMCONFIGFILETYPE */
    OMX_IndexParamBrcmConfigFileChunkRegisters,/**< reference: OMX_PARAM_BRCMCONFIGFILECHUNKTYPE */
    OMX_IndexParamBrcmAttachLog,               /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamCameraZeroShutterLag,        /**< reference: OMX_CONFIG_ZEROSHUTTERLAGTYPE */

    // 0x7f0000d0
    OMX_IndexParamBrcmFpsRange,                /**< reference: OMX_PARAM_BRCMFRAMERATERANGETYPE */
    OMX_IndexParamCaptureExposureCompensation, /**< reference: OMX_PARAM_S32TYPE */
    OMX_IndexParamBrcmVideoPrecodeForQP,       /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamBrcmVideoTimestampFifo,      /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamSWSharpenDisable,            /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexConfigBrcmFlashRequired,          /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamBrcmVideoDrmProtectBuffer,   /**< reference: OMX_PARAM_BRCMVIDEODRMPROTECTBUFFERTYPE */
    OMX_IndexParamSWSaturationDisable,         /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamBrcmVideoDecodeConfigVD3,    /**< reference: OMX_PARAM_BRCMVIDEODECODECONFIGVD3TYPE */
    OMX_IndexConfigBrcmPowerMonitor,           /**< reference: OMX_CONFIG_BOOLEANTYPE */
    OMX_IndexParamBrcmZeroCopy,                /**< reference: OMX_CONFIG_PORTBOOLEANTYPE */
    OMX_IndexParamBrcmVideoEGLRenderDiscardMode,   /**< reference: OMX_CONFIG_PORTBOOLEANTYPE */
    OMX_IndexParamBrcmVideoAVC_VCLHRDEnable,    /**< reference: OMX_CONFIG_PORTBOOLEANTYPE*/
    OMX_IndexParamBrcmVideoAVC_LowDelayHRDEnable, /**< reference: OMX_CONFIG_PORTBOOLEANTYPE*/
    OMX_IndexParamBrcmVideoCroppingDisable,    /**< reference: OMX_CONFIG_PORTBOOLEANTYPE*/
    OMX_IndexParamBrcmVideoAVCInlineHeaderEnable, /**< reference: OMX_CONFIG_PORTBOOLEANTYPE*/

    // 0x7f0000f0
    OMX_IndexConfigBrcmAudioDownmixCoefficients = 0x7f0000f0, /**< reference: OMX_CONFIG_BRCMAUDIODOWNMIXCOEFFICIENTS */
    OMX_IndexConfigBrcmAudioDownmixCoefficients8x8,           /**< reference: OMX_CONFIG_BRCMAUDIODOWNMIXCOEFFICIENTS8x8 */
    OMX_IndexConfigBrcmAudioMaxSample,                        /**< reference: OMX_CONFIG_BRCMAUDIOMAXSAMPLE */
    OMX_IndexConfigCustomAwbGains,                            /**< reference: OMX_CONFIG_CUSTOMAWBGAINSTYPE */
    OMX_IndexParamRemoveImagePadding,                         /**< reference: OMX_CONFIG_PORTBOOLEANTYPE*/
    OMX_IndexParamBrcmVideoAVCInlineVectorsEnable,            /**< reference: OMX_CONFIG_PORTBOOLEANTYPE */
    OMX_IndexConfigBrcmRenderStats,                           /**< reference: OMX_CONFIG_BRCMRENDERSTATSTYPE */
    OMX_IndexConfigBrcmCameraAnnotate,                        /**< reference: OMX_CONFIG_BRCMANNOTATETYPE */
    OMX_IndexParamBrcmStereoscopicMode,                       /**< reference :OMX_CONFIG_BRCMSTEREOSCOPICMODETYPE */
    OMX_IndexParamBrcmLockStepEnable,                         /**< reference: OMX_CONFIG_PORTBOOLEANTYPE */
    OMX_IndexParamBrcmTimeScale,                              /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamCameraInterface,                            /**< reference: OMX_PARAM_CAMERAINTERFACETYPE */
    OMX_IndexParamCameraClockingMode,                         /**< reference: OMX_PARAM_CAMERACLOCKINGMODETYPE */
    OMX_IndexParamCameraRxConfig,                             /**< reference: OMX_PARAM_CAMERARXCONFIG_TYPE */
    OMX_IndexParamCameraRxTiming,                             /**< reference: OMX_PARAM_CAMERARXTIMING_TYPE */
    OMX_IndexParamDynamicParameterConfig,                     /**< reference: OMX_PARAM_U32TYPE */

    // 0x7f000100
    OMX_IndexParamBrcmVideoAVCSPSTimingEnable,                /** reference: OMX_CONFIG_PORTBOOLEANTYPE */
    OMX_IndexParamBrcmBayerOrder,                             /** reference: OMX_PARAM_BAYERORDERTYPE */
    OMX_IndexParamBrcmMaxNumCallbacks,                        /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamBrcmJpegRestartInterval,                    /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamBrcmSupportsSlices,                         /**< reference: OMX_CONFIG_PORTBOOLEANTYPE */
    OMX_IndexParamBrcmIspBlockOverride,                       /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamBrcmSupportsUnalignedSliceheight,           /**< reference: OMX_CONFIG_PORTBOOLEANTYPE */
    OMX_IndexParamBrcmLensShadingOverride,                    /**< reference: OMX_PARAM_LENSSHADINGOVERRIDETYPE */
    OMX_IndexParamBrcmBlackLevel,                             /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamOutputShift,                                /**< reference: OMX_PARAM_S32TYPE */
    OMX_IndexParamCcmShift,                                   /**< reference: OMX_PARAM_S32TYPE */
    OMX_IndexParamCustomCcm,                                  /**< reference: OMX_PARAM_CUSTOMCCMTYPE */
    OMX_IndexConfigCameraAnalogGain,                          /**< reference: OMX_CONFIG_CAMERAGAINTYPE */
    OMX_IndexConfigCameraDigitalGain,                         /**< reference: OMX_CONFIG_CAMERAGAINTYPE */
    OMX_IndexConfigBrcmDroppableRunLength,                    /**< reference: OMX_PARAM_U32TYPE */
    OMX_IndexParamMinimumAlignment,                           /**< reference: OMX_PARAM_MINALIGNTYPE */
    OMX_IndexMax = 0x7FFFFFFF
} OMX_INDEXTYPE;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
/* File EOF */
