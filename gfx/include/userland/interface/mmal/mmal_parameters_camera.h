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

/*=============================================================================
Copyright (c) 2011 Broadcom Europe Limited.
All rights reserved.
=============================================================================*/
/** \file
 * Multi-Media Abstraction Layer - Definition of some standard parameters.
 */

#ifndef MMAL_PARAMETERS_CAMERA_H
#define MMAL_PARAMETERS_CAMERA_H

#include "mmal_parameters_common.h"

/*************************************************
 * ALWAYS ADD NEW ENUMS AT THE END OF THIS LIST! *
 ************************************************/

/** Camera-specific MMAL parameter IDs.
 * @ingroup MMAL_PARAMETER_IDS
 */
enum {
   /* 0 */
   MMAL_PARAMETER_THUMBNAIL_CONFIGURATION    /**< Takes a @ref MMAL_PARAMETER_THUMBNAIL_CONFIG_T */
         = MMAL_PARAMETER_GROUP_CAMERA,
   MMAL_PARAMETER_CAPTURE_QUALITY,           /**< Unused? */
   MMAL_PARAMETER_ROTATION,                  /**< Takes a @ref MMAL_PARAMETER_INT32_T */
   MMAL_PARAMETER_EXIF_DISABLE,              /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_EXIF,                      /**< Takes a @ref MMAL_PARAMETER_EXIF_T */
   MMAL_PARAMETER_AWB_MODE,                  /**< Takes a @ref MMAL_PARAM_AWBMODE_T */
   MMAL_PARAMETER_IMAGE_EFFECT,              /**< Takes a @ref MMAL_PARAMETER_IMAGEFX_T */
   MMAL_PARAMETER_COLOUR_EFFECT,             /**< Takes a @ref MMAL_PARAMETER_COLOURFX_T */
   MMAL_PARAMETER_FLICKER_AVOID,             /**< Takes a @ref MMAL_PARAMETER_FLICKERAVOID_T */
   MMAL_PARAMETER_FLASH,                     /**< Takes a @ref MMAL_PARAMETER_FLASH_T */
   MMAL_PARAMETER_REDEYE,                    /**< Takes a @ref MMAL_PARAMETER_REDEYE_T */
   MMAL_PARAMETER_FOCUS,                     /**< Takes a @ref MMAL_PARAMETER_FOCUS_T */
   MMAL_PARAMETER_FOCAL_LENGTHS,             /**< Unused? */
   MMAL_PARAMETER_EXPOSURE_COMP,             /**< Takes a @ref MMAL_PARAMETER_INT32_T or MMAL_PARAMETER_RATIONAL_T */
   MMAL_PARAMETER_ZOOM,                      /**< Takes a @ref MMAL_PARAMETER_SCALEFACTOR_T */
   MMAL_PARAMETER_MIRROR,                    /**< Takes a @ref MMAL_PARAMETER_MIRROR_T */

   /* 0x10 */
   MMAL_PARAMETER_CAMERA_NUM,                /**< Takes a @ref MMAL_PARAMETER_UINT32_T */
   MMAL_PARAMETER_CAPTURE,                   /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_EXPOSURE_MODE,             /**< Takes a @ref MMAL_PARAMETER_EXPOSUREMODE_T */
   MMAL_PARAMETER_EXP_METERING_MODE,         /**< Takes a @ref MMAL_PARAMETER_EXPOSUREMETERINGMODE_T */
   MMAL_PARAMETER_FOCUS_STATUS,              /**< Takes a @ref MMAL_PARAMETER_FOCUS_STATUS_T */
   MMAL_PARAMETER_CAMERA_CONFIG,             /**< Takes a @ref MMAL_PARAMETER_CAMERA_CONFIG_T */
   MMAL_PARAMETER_CAPTURE_STATUS,            /**< Takes a @ref MMAL_PARAMETER_CAPTURE_STATUS_T */
   MMAL_PARAMETER_FACE_TRACK,                /**< Takes a @ref MMAL_PARAMETER_FACE_TRACK_T */
   MMAL_PARAMETER_DRAW_BOX_FACES_AND_FOCUS,  /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_JPEG_Q_FACTOR,             /**< Takes a @ref MMAL_PARAMETER_UINT32_T */
   MMAL_PARAMETER_FRAME_RATE,                /**< Takes a @ref MMAL_PARAMETER_FRAME_RATE_T */
   MMAL_PARAMETER_USE_STC,                   /**< Takes a @ref MMAL_PARAMETER_CAMERA_STC_MODE_T */
   MMAL_PARAMETER_CAMERA_INFO,               /**< Takes a @ref MMAL_PARAMETER_CAMERA_INFO_T */
   MMAL_PARAMETER_VIDEO_STABILISATION,       /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_FACE_TRACK_RESULTS,        /**< Takes a @ref MMAL_PARAMETER_FACE_TRACK_RESULTS_T */
   MMAL_PARAMETER_ENABLE_RAW_CAPTURE,        /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */

   /* 0x20 */
   MMAL_PARAMETER_DPF_FILE,                  /**< Takes a @ref MMAL_PARAMETER_URI_T */
   MMAL_PARAMETER_ENABLE_DPF_FILE,           /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_DPF_FAIL_IS_FATAL,         /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_CAPTURE_MODE,              /**< Takes a @ref MMAL_PARAMETER_CAPTUREMODE_T */
   MMAL_PARAMETER_FOCUS_REGIONS,             /**< Takes a @ref MMAL_PARAMETER_FOCUS_REGIONS_T */
   MMAL_PARAMETER_INPUT_CROP,                /**< Takes a @ref MMAL_PARAMETER_INPUT_CROP_T */
   MMAL_PARAMETER_SENSOR_INFORMATION,        /**< Takes a @ref MMAL_PARAMETER_SENSOR_INFORMATION_T */
   MMAL_PARAMETER_FLASH_SELECT,              /**< Takes a @ref MMAL_PARAMETER_FLASH_SELECT_T */
   MMAL_PARAMETER_FIELD_OF_VIEW,             /**< Takes a @ref MMAL_PARAMETER_FIELD_OF_VIEW_T */
   MMAL_PARAMETER_HIGH_DYNAMIC_RANGE,        /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_DYNAMIC_RANGE_COMPRESSION, /**< Takes a @ref MMAL_PARAMETER_DRC_T */
   MMAL_PARAMETER_ALGORITHM_CONTROL,         /**< Takes a @ref MMAL_PARAMETER_ALGORITHM_CONTROL_T */
   MMAL_PARAMETER_SHARPNESS,                 /**< Takes a @ref MMAL_PARAMETER_RATIONAL_T */
   MMAL_PARAMETER_CONTRAST,                  /**< Takes a @ref MMAL_PARAMETER_RATIONAL_T */
   MMAL_PARAMETER_BRIGHTNESS,                /**< Takes a @ref MMAL_PARAMETER_RATIONAL_T */
   MMAL_PARAMETER_SATURATION,                /**< Takes a @ref MMAL_PARAMETER_RATIONAL_T */

   /* 0x30 */
   MMAL_PARAMETER_ISO,                       /**< Takes a @ref MMAL_PARAMETER_UINT32_T */
   MMAL_PARAMETER_ANTISHAKE,                 /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_IMAGE_EFFECT_PARAMETERS,   /**< Takes a @ref MMAL_PARAMETER_IMAGEFX_PARAMETERS_T */
   MMAL_PARAMETER_CAMERA_BURST_CAPTURE,      /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_CAMERA_MIN_ISO,            /**< Takes a @ref MMAL_PARAMETER_UINT32_T */
   MMAL_PARAMETER_CAMERA_USE_CASE,           /**< Takes a @ref MMAL_PARAMETER_CAMERA_USE_CASE_T */
   MMAL_PARAMETER_CAPTURE_STATS_PASS,        /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_CAMERA_CUSTOM_SENSOR_CONFIG, /**< Takes a @ref MMAL_PARAMETER_UINT32_T */
   MMAL_PARAMETER_ENABLE_REGISTER_FILE,      /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_REGISTER_FAIL_IS_FATAL,    /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_CONFIGFILE_REGISTERS,      /**< Takes a @ref MMAL_PARAMETER_CONFIGFILE_T */
   MMAL_PARAMETER_CONFIGFILE_CHUNK_REGISTERS,/**< Takes a @ref MMAL_PARAMETER_CONFIGFILE_CHUNK_T */
   MMAL_PARAMETER_JPEG_ATTACH_LOG,           /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_ZERO_SHUTTER_LAG,          /**< Takes a @ref MMAL_PARAMETER_ZEROSHUTTERLAG_T */
   MMAL_PARAMETER_FPS_RANGE,                 /**< Takes a @ref MMAL_PARAMETER_FPS_RANGE_T */
   MMAL_PARAMETER_CAPTURE_EXPOSURE_COMP,     /**< Takes a @ref MMAL_PARAMETER_INT32_T */

   /* 0x40 */
   MMAL_PARAMETER_SW_SHARPEN_DISABLE,        /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_FLASH_REQUIRED,            /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_SW_SATURATION_DISABLE,     /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_SHUTTER_SPEED,             /**< Takes a @ref MMAL_PARAMETER_UINT32_T */
   MMAL_PARAMETER_CUSTOM_AWB_GAINS,          /**< Takes a @ref MMAL_PARAMETER_AWB_GAINS_T */
   MMAL_PARAMETER_CAMERA_SETTINGS,           /**< Takes a @ref MMAL_PARAMETER_CAMERA_SETTINGS_T */
   MMAL_PARAMETER_PRIVACY_INDICATOR,         /**< Takes a @ref MMAL_PARAMETER_PRIVACY_INDICATOR_T */
   MMAL_PARAMETER_VIDEO_DENOISE,             /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_STILLS_DENOISE,            /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_ANNOTATE,                  /**< Takes a @ref MMAL_PARAMETER_CAMERA_ANNOTATE_T */
   MMAL_PARAMETER_STEREOSCOPIC_MODE,         /**< Takes a @ref MMAL_PARAMETER_STEREOSCOPIC_MODE_T */
   MMAL_PARAMETER_CAMERA_INTERFACE,          /**< Takes a @ref MMAL_PARAMETER_CAMERA_INTERFACE_T */
   MMAL_PARAMETER_CAMERA_CLOCKING_MODE,      /**< Takes a @ref MMAL_PARAMETER_CAMERA_CLOCKING_MODE_T */
   MMAL_PARAMETER_CAMERA_RX_CONFIG,          /**< Takes a @ref MMAL_PARAMETER_CAMERA_RX_CONFIG_T */
   MMAL_PARAMETER_CAMERA_RX_TIMING,          /**< Takes a @ref MMAL_PARAMETER_CAMERA_RX_TIMING_T */
   MMAL_PARAMETER_DPF_CONFIG,                /**< Takes a @ref MMAL_PARAMETER_UINT32_T */

   /* 0x50 */
   MMAL_PARAMETER_JPEG_RESTART_INTERVAL,     /**< Takes a @ref MMAL_PARAMETER_UINT32_T */
   MMAL_PARAMETER_CAMERA_ISP_BLOCK_OVERRIDE, /**< Takes a @ref MMAL_PARAMETER_UINT32_T */
   MMAL_PARAMETER_LENS_SHADING_OVERRIDE,     /**< Takes a @ref MMAL_PARAMETER_LENS_SHADING_T */
   MMAL_PARAMETER_BLACK_LEVEL,               /**< Takes a @ref MMAL_PARAMETER_UINT32_T */
   MMAL_PARAMETER_RESIZE_PARAMS,             /**< Takes a @ref MMAL_PARAMETER_RESIZE_T */
   MMAL_PARAMETER_CROP,                      /**< Takes a @ref MMAL_PARAMETER_CROP_T */
   MMAL_PARAMETER_OUTPUT_SHIFT,              /**< Takes a @ref MMAL_PARAMETER_INT32_T */
   MMAL_PARAMETER_CCM_SHIFT,                 /**< Takes a @ref MMAL_PARAMETER_INT32_T */
   MMAL_PARAMETER_CUSTOM_CCM,                /**< Takes a @ref MMAL_PARAMETER_CUSTOM_CCM_T */
   MMAL_PARAMETER_ANALOG_GAIN,               /**< Takes a @ref MMAL_PARAMETER_RATIONAL_T */
   MMAL_PARAMETER_DIGITAL_GAIN,              /**< Takes a @ref MMAL_PARAMETER_RATIONAL_T */
};

/** Thumbnail configuration parameter type */
typedef struct MMAL_PARAMETER_THUMBNAIL_CONFIG_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   uint32_t enable;                  /**< Enable generation of thumbnails during still capture */
   uint32_t width;                   /**< Desired width of the thumbnail */
   uint32_t height;                  /**< Desired height of the thumbnail */
   uint32_t quality;                 /**< Desired compression quality of the thumbnail */
} MMAL_PARAMETER_THUMBNAIL_CONFIG_T;

/** EXIF parameter type. */
typedef struct MMAL_PARAMETER_EXIF_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   uint32_t keylen;                            /**< If 0, assume key is terminated by '=', otherwise length of key and treat data as binary */
   uint32_t value_offset;                      /**< Offset within data buffer of the start of the value. If 0, look for a "key=value" string */
   uint32_t valuelen;                          /**< If 0, assume value is null-terminated, otherwise length of value and treat data as binary */
   uint8_t data[1];                            /**< EXIF key/value string. Variable length */
} MMAL_PARAMETER_EXIF_T;

/** Exposure modes. */
typedef enum
{
   MMAL_PARAM_EXPOSUREMODE_OFF,
   MMAL_PARAM_EXPOSUREMODE_AUTO,
   MMAL_PARAM_EXPOSUREMODE_NIGHT,
   MMAL_PARAM_EXPOSUREMODE_NIGHTPREVIEW,
   MMAL_PARAM_EXPOSUREMODE_BACKLIGHT,
   MMAL_PARAM_EXPOSUREMODE_SPOTLIGHT,
   MMAL_PARAM_EXPOSUREMODE_SPORTS,
   MMAL_PARAM_EXPOSUREMODE_SNOW,
   MMAL_PARAM_EXPOSUREMODE_BEACH,
   MMAL_PARAM_EXPOSUREMODE_VERYLONG,
   MMAL_PARAM_EXPOSUREMODE_FIXEDFPS,
   MMAL_PARAM_EXPOSUREMODE_ANTISHAKE,
   MMAL_PARAM_EXPOSUREMODE_FIREWORKS,
   MMAL_PARAM_EXPOSUREMODE_MAX = 0x7fffffff
} MMAL_PARAM_EXPOSUREMODE_T;

typedef struct MMAL_PARAMETER_EXPOSUREMODE_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_PARAM_EXPOSUREMODE_T value;   /**< exposure mode */
} MMAL_PARAMETER_EXPOSUREMODE_T;

typedef enum
{
   MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE,
   MMAL_PARAM_EXPOSUREMETERINGMODE_SPOT,
   MMAL_PARAM_EXPOSUREMETERINGMODE_BACKLIT,
   MMAL_PARAM_EXPOSUREMETERINGMODE_MATRIX,
   MMAL_PARAM_EXPOSUREMETERINGMODE_MAX = 0x7fffffff
} MMAL_PARAM_EXPOSUREMETERINGMODE_T;

typedef struct MMAL_PARAMETER_EXPOSUREMETERINGMODE_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_PARAM_EXPOSUREMETERINGMODE_T value;   /**< metering mode */
} MMAL_PARAMETER_EXPOSUREMETERINGMODE_T;

/** AWB parameter modes. */
typedef enum MMAL_PARAM_AWBMODE_T
{
   MMAL_PARAM_AWBMODE_OFF,
   MMAL_PARAM_AWBMODE_AUTO,
   MMAL_PARAM_AWBMODE_SUNLIGHT,
   MMAL_PARAM_AWBMODE_CLOUDY,
   MMAL_PARAM_AWBMODE_SHADE,
   MMAL_PARAM_AWBMODE_TUNGSTEN,
   MMAL_PARAM_AWBMODE_FLUORESCENT,
   MMAL_PARAM_AWBMODE_INCANDESCENT,
   MMAL_PARAM_AWBMODE_FLASH,
   MMAL_PARAM_AWBMODE_HORIZON,
   MMAL_PARAM_AWBMODE_MAX = 0x7fffffff
} MMAL_PARAM_AWBMODE_T;

/** AWB parameter type. */
typedef struct MMAL_PARAMETER_AWBMODE_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_PARAM_AWBMODE_T value;   /**< AWB mode */
} MMAL_PARAMETER_AWBMODE_T;

/** Image effect */
typedef enum MMAL_PARAM_IMAGEFX_T
{
   MMAL_PARAM_IMAGEFX_NONE,
   MMAL_PARAM_IMAGEFX_NEGATIVE,
   MMAL_PARAM_IMAGEFX_SOLARIZE,
   MMAL_PARAM_IMAGEFX_POSTERIZE,
   MMAL_PARAM_IMAGEFX_WHITEBOARD,
   MMAL_PARAM_IMAGEFX_BLACKBOARD,
   MMAL_PARAM_IMAGEFX_SKETCH,
   MMAL_PARAM_IMAGEFX_DENOISE,
   MMAL_PARAM_IMAGEFX_EMBOSS,
   MMAL_PARAM_IMAGEFX_OILPAINT,
   MMAL_PARAM_IMAGEFX_HATCH,
   MMAL_PARAM_IMAGEFX_GPEN,
   MMAL_PARAM_IMAGEFX_PASTEL,
   MMAL_PARAM_IMAGEFX_WATERCOLOUR,
   MMAL_PARAM_IMAGEFX_FILM,
   MMAL_PARAM_IMAGEFX_BLUR,
   MMAL_PARAM_IMAGEFX_SATURATION,
   MMAL_PARAM_IMAGEFX_COLOURSWAP,
   MMAL_PARAM_IMAGEFX_WASHEDOUT,
   MMAL_PARAM_IMAGEFX_POSTERISE,
   MMAL_PARAM_IMAGEFX_COLOURPOINT,
   MMAL_PARAM_IMAGEFX_COLOURBALANCE,
   MMAL_PARAM_IMAGEFX_CARTOON,
   MMAL_PARAM_IMAGEFX_DEINTERLACE_DOUBLE,
   MMAL_PARAM_IMAGEFX_DEINTERLACE_ADV,
   MMAL_PARAM_IMAGEFX_DEINTERLACE_FAST,
   MMAL_PARAM_IMAGEFX_MAX = 0x7fffffff
} MMAL_PARAM_IMAGEFX_T;

typedef struct MMAL_PARAMETER_IMAGEFX_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_PARAM_IMAGEFX_T value;   /**< Image effect mode */
} MMAL_PARAMETER_IMAGEFX_T;

#define MMAL_MAX_IMAGEFX_PARAMETERS 6  /* Image effects library currently uses a maximum of 5 parameters per effect */

typedef struct MMAL_PARAMETER_IMAGEFX_PARAMETERS_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_PARAM_IMAGEFX_T effect;   /**< Image effect mode */
   uint32_t num_effect_params;     /**< Number of used elements in */
   uint32_t effect_parameter[MMAL_MAX_IMAGEFX_PARAMETERS]; /**< Array of parameters */
} MMAL_PARAMETER_IMAGEFX_PARAMETERS_T;

/** Colour effect parameter type*/
typedef struct MMAL_PARAMETER_COLOURFX_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   int32_t enable;
   uint32_t u;
   uint32_t v;
} MMAL_PARAMETER_COLOURFX_T;

typedef enum MMAL_CAMERA_STC_MODE_T
{
   MMAL_PARAM_STC_MODE_OFF,         /**< Frames do not have STCs, as needed in OpenMAX/IL */
   MMAL_PARAM_STC_MODE_RAW,         /**< Use raw clock STC, needed for true pause/resume support */
   MMAL_PARAM_STC_MODE_COOKED,      /**< Start the STC from the start of capture, only for quick demo code */
   MMAL_PARAM_STC_MODE_MAX = 0x7fffffff
} MMAL_CAMERA_STC_MODE_T;

typedef struct MMAL_PARAMETER_CAMERA_STC_MODE_T
{
   MMAL_PARAMETER_HEADER_T hdr;
   MMAL_CAMERA_STC_MODE_T value;
} MMAL_PARAMETER_CAMERA_STC_MODE_T;

typedef enum MMAL_PARAM_FLICKERAVOID_T
{
   MMAL_PARAM_FLICKERAVOID_OFF,
   MMAL_PARAM_FLICKERAVOID_AUTO,
   MMAL_PARAM_FLICKERAVOID_50HZ,
   MMAL_PARAM_FLICKERAVOID_60HZ,
   MMAL_PARAM_FLICKERAVOID_MAX = 0x7FFFFFFF
} MMAL_PARAM_FLICKERAVOID_T;

typedef struct MMAL_PARAMETER_FLICKERAVOID_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_PARAM_FLICKERAVOID_T value;   /**< Flicker avoidance mode */
} MMAL_PARAMETER_FLICKERAVOID_T;

typedef enum MMAL_PARAM_FLASH_T
{
   MMAL_PARAM_FLASH_OFF,
   MMAL_PARAM_FLASH_AUTO,
   MMAL_PARAM_FLASH_ON,
   MMAL_PARAM_FLASH_REDEYE,
   MMAL_PARAM_FLASH_FILLIN,
   MMAL_PARAM_FLASH_TORCH,
   MMAL_PARAM_FLASH_MAX = 0x7FFFFFFF
} MMAL_PARAM_FLASH_T;

typedef struct MMAL_PARAMETER_FLASH_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_PARAM_FLASH_T value;   /**< Flash mode */
} MMAL_PARAMETER_FLASH_T;

typedef enum MMAL_PARAM_REDEYE_T
{
   MMAL_PARAM_REDEYE_OFF,
   MMAL_PARAM_REDEYE_ON,
   MMAL_PARAM_REDEYE_SIMPLE,
   MMAL_PARAM_REDEYE_MAX = 0x7FFFFFFF
} MMAL_PARAM_REDEYE_T;

typedef struct MMAL_PARAMETER_REDEYE_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_PARAM_REDEYE_T value;   /**< Red eye reduction mode */
} MMAL_PARAMETER_REDEYE_T;

typedef enum MMAL_PARAM_FOCUS_T
{
   MMAL_PARAM_FOCUS_AUTO,
   MMAL_PARAM_FOCUS_AUTO_NEAR,
   MMAL_PARAM_FOCUS_AUTO_MACRO,
   MMAL_PARAM_FOCUS_CAF,
   MMAL_PARAM_FOCUS_CAF_NEAR,
   MMAL_PARAM_FOCUS_FIXED_INFINITY,
   MMAL_PARAM_FOCUS_FIXED_HYPERFOCAL,
   MMAL_PARAM_FOCUS_FIXED_NEAR,
   MMAL_PARAM_FOCUS_FIXED_MACRO,
   MMAL_PARAM_FOCUS_EDOF,
   MMAL_PARAM_FOCUS_CAF_MACRO,
   MMAL_PARAM_FOCUS_CAF_FAST,
   MMAL_PARAM_FOCUS_CAF_NEAR_FAST,
   MMAL_PARAM_FOCUS_CAF_MACRO_FAST,
   MMAL_PARAM_FOCUS_FIXED_CURRENT,
   MMAL_PARAM_FOCUS_MAX = 0x7FFFFFFF
} MMAL_PARAM_FOCUS_T;

typedef struct MMAL_PARAMETER_FOCUS_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_PARAM_FOCUS_T value;   /**< Focus mode */
} MMAL_PARAMETER_FOCUS_T;

typedef enum MMAL_PARAM_CAPTURE_STATUS_T
{
   MMAL_PARAM_CAPTURE_STATUS_NOT_CAPTURING,
   MMAL_PARAM_CAPTURE_STATUS_CAPTURE_STARTED,
   MMAL_PARAM_CAPTURE_STATUS_CAPTURE_ENDED,

   MMAL_PARAM_CAPTURE_STATUS_MAX = 0x7FFFFFFF
} MMAL_PARAM_CAPTURE_STATUS_T;

typedef struct MMAL_PARAMETER_CAPTURE_STATUS_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_PARAM_CAPTURE_STATUS_T status;   /**< Capture status */
} MMAL_PARAMETER_CAPTURE_STATUS_T;

typedef enum MMAL_PARAM_FOCUS_STATUS_T
{
   MMAL_PARAM_FOCUS_STATUS_OFF,
   MMAL_PARAM_FOCUS_STATUS_REQUEST,
   MMAL_PARAM_FOCUS_STATUS_REACHED,
   MMAL_PARAM_FOCUS_STATUS_UNABLE_TO_REACH,
   MMAL_PARAM_FOCUS_STATUS_LOST,
   MMAL_PARAM_FOCUS_STATUS_CAF_MOVING,
   MMAL_PARAM_FOCUS_STATUS_CAF_SUCCESS,
   MMAL_PARAM_FOCUS_STATUS_CAF_FAILED,
   MMAL_PARAM_FOCUS_STATUS_MANUAL_MOVING,
   MMAL_PARAM_FOCUS_STATUS_MANUAL_REACHED,
   MMAL_PARAM_FOCUS_STATUS_CAF_WATCHING,
   MMAL_PARAM_FOCUS_STATUS_CAF_SCENE_CHANGED,

   MMAL_PARAM_FOCUS_STATUS_MAX = 0x7FFFFFFF
} MMAL_PARAM_FOCUS_STATUS_T;

typedef struct MMAL_PARAMETER_FOCUS_STATUS_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_PARAM_FOCUS_STATUS_T status;   /**< Focus status */
} MMAL_PARAMETER_FOCUS_STATUS_T;

typedef enum MMAL_PARAM_FACE_TRACK_MODE_T
{
   MMAL_PARAM_FACE_DETECT_NONE,                           /**< Disables face detection */
   MMAL_PARAM_FACE_DETECT_ON,                             /**< Enables face detection */
   MMAL_PARAM_FACE_DETECT_MAX = 0x7FFFFFFF
} MMAL_PARAM_FACE_TRACK_MODE_T;

typedef struct MMAL_PARAMETER_FACE_TRACK_T /* face tracking control */
{
   MMAL_PARAMETER_HEADER_T hdr;
   MMAL_PARAM_FACE_TRACK_MODE_T mode;
   uint32_t maxRegions;
   uint32_t frames;
   uint32_t quality;
} MMAL_PARAMETER_FACE_TRACK_T;

typedef struct MMAL_PARAMETER_FACE_TRACK_FACE_T /* face tracking face information */
{
   int32_t     face_id;             /**< Face ID. Should remain the same whilst the face is detected to remain in the scene */
   int32_t     score;               /**< Confidence of the face detection. Range 1-100 (1=unsure, 100=positive). */
   MMAL_RECT_T face_rect;           /**< Rectangle around the whole face */

   MMAL_RECT_T eye_rect[2];         /**< Rectangle around the eyes ([0] = left eye, [1] = right eye) */
   MMAL_RECT_T mouth_rect;          /**< Rectangle around the mouth */
} MMAL_PARAMETER_FACE_TRACK_FACE_T;

typedef struct MMAL_PARAMETER_FACE_TRACK_RESULTS_T /* face tracking results */
{
   MMAL_PARAMETER_HEADER_T hdr;

   uint32_t num_faces;        /**< Number of faces detected */
   uint32_t frame_width;      /**< Width of the frame on which the faces were detected (allows scaling) */
   uint32_t frame_height;     /**< Height of the frame on which the faces were detected (allows scaling) */

   MMAL_PARAMETER_FACE_TRACK_FACE_T faces[1];   /**< Face information (variable length array */
} MMAL_PARAMETER_FACE_TRACK_RESULTS_T;

typedef enum MMAL_PARAMETER_CAMERA_CONFIG_TIMESTAMP_MODE_T
{
   MMAL_PARAM_TIMESTAMP_MODE_ZERO,           /**< Always timestamp frames as 0 */
   MMAL_PARAM_TIMESTAMP_MODE_RAW_STC,        /**< Use the raw STC value for the frame timestamp */
   MMAL_PARAM_TIMESTAMP_MODE_RESET_STC,      /**< Use the STC timestamp but subtract the timestamp
                                              * of the first frame sent to give a zero based timestamp.
                                              */
   MMAL_PARAM_TIMESTAMP_MODE_MAX = 0x7FFFFFFF
} MMAL_PARAMETER_CAMERA_CONFIG_TIMESTAMP_MODE_T;

typedef struct MMAL_PARAMETER_CAMERA_CONFIG_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   /* Parameters for setting up the image pools */
   uint32_t max_stills_w;        /**< Max size of stills capture */
   uint32_t max_stills_h;
   uint32_t stills_yuv422;       /**< Allow YUV422 stills capture */
   uint32_t one_shot_stills;     /**< Continuous or one shot stills captures. */

   uint32_t max_preview_video_w; /**< Max size of the preview or video capture frames */
   uint32_t max_preview_video_h;
   uint32_t num_preview_video_frames;

   uint32_t stills_capture_circular_buffer_height; /**< Sets the height of the circular buffer for stills capture. */

   uint32_t fast_preview_resume;    /**< Allows preview/encode to resume as fast as possible after the stills input frame
                                     * has been received, and then processes the still frame in the background
                                     * whilst preview/encode has resumed.
                                     * Actual mode is controlled by MMAL_PARAMETER_CAPTURE_MODE.
                                     */

   MMAL_PARAMETER_CAMERA_CONFIG_TIMESTAMP_MODE_T use_stc_timestamp;
                                    /**< Selects algorithm for timestamping frames if there is no clock component connected.
                                      */


} MMAL_PARAMETER_CAMERA_CONFIG_T;

#define MMAL_PARAMETER_CAMERA_INFO_MAX_CAMERAS 4
#define MMAL_PARAMETER_CAMERA_INFO_MAX_FLASHES 2
#define MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN 16

typedef struct MMAL_PARAMETER_CAMERA_INFO_CAMERA_T
{
   uint32_t    port_id;
   uint32_t    max_width;
   uint32_t    max_height;
   MMAL_BOOL_T lens_present;
   char        camera_name[MMAL_PARAMETER_CAMERA_INFO_MAX_STR_LEN];
} MMAL_PARAMETER_CAMERA_INFO_CAMERA_T;

typedef enum MMAL_PARAMETER_CAMERA_INFO_FLASH_TYPE_T
{
   MMAL_PARAMETER_CAMERA_INFO_FLASH_TYPE_XENON = 0, /* Make values explicit */
   MMAL_PARAMETER_CAMERA_INFO_FLASH_TYPE_LED   = 1, /* to ensure they match */
   MMAL_PARAMETER_CAMERA_INFO_FLASH_TYPE_OTHER = 2, /* values in config ini */
   MMAL_PARAMETER_CAMERA_INFO_FLASH_TYPE_MAX = 0x7FFFFFFF
} MMAL_PARAMETER_CAMERA_INFO_FLASH_TYPE_T;

typedef struct MMAL_PARAMETER_CAMERA_INFO_FLASH_T
{
   MMAL_PARAMETER_CAMERA_INFO_FLASH_TYPE_T flash_type;
} MMAL_PARAMETER_CAMERA_INFO_FLASH_T;

typedef struct MMAL_PARAMETER_CAMERA_INFO_T
{
   MMAL_PARAMETER_HEADER_T             hdr;
   uint32_t                            num_cameras;
   uint32_t                            num_flashes;
   MMAL_PARAMETER_CAMERA_INFO_CAMERA_T cameras[MMAL_PARAMETER_CAMERA_INFO_MAX_CAMERAS];
   MMAL_PARAMETER_CAMERA_INFO_FLASH_T  flashes[MMAL_PARAMETER_CAMERA_INFO_MAX_FLASHES];
} MMAL_PARAMETER_CAMERA_INFO_T;

typedef enum MMAL_PARAMETER_CAPTUREMODE_MODE_T
{
   MMAL_PARAM_CAPTUREMODE_WAIT_FOR_END,            /**< Resumes preview once capture is completed. */
   MMAL_PARAM_CAPTUREMODE_WAIT_FOR_END_AND_HOLD,   /**< Resumes preview once capture is completed, and hold the image for subsequent reprocessing. */
   MMAL_PARAM_CAPTUREMODE_RESUME_VF_IMMEDIATELY,   /**< Resumes preview as soon as possible once capture frame is received from the sensor.
                                                    *   Requires fast_preview_resume to be set via MMAL_PARAMETER_CAMERA_CONFIG.
                                                    */
} MMAL_PARAMETER_CAPTUREMODE_MODE_T;

/** Stills capture mode control. */
typedef struct MMAL_PARAMETER_CAPTUREMODE_T
{
   MMAL_PARAMETER_HEADER_T hdr;
   MMAL_PARAMETER_CAPTUREMODE_MODE_T mode;
} MMAL_PARAMETER_CAPTUREMODE_T;

typedef enum MMAL_PARAMETER_FOCUS_REGION_TYPE_T
{
   MMAL_PARAMETER_FOCUS_REGION_TYPE_NORMAL,     /**< Region defines a generic region */
   MMAL_PARAMETER_FOCUS_REGION_TYPE_FACE,       /**< Region defines a face */
   MMAL_PARAMETER_FOCUS_REGION_TYPE_MAX
} MMAL_PARAMETER_FOCUS_REGION_TYPE_T;

typedef struct MMAL_PARAMETER_FOCUS_REGION_T
{
   MMAL_RECT_T rect;    /**< Focus rectangle as 0P16 fixed point values. */
   uint32_t weight;     /**< Region weighting. */
   uint32_t mask;       /**< Mask for multi-stage regions */
   MMAL_PARAMETER_FOCUS_REGION_TYPE_T type;  /**< Region type */
} MMAL_PARAMETER_FOCUS_REGION_T;

typedef struct MMAL_PARAMETER_FOCUS_REGIONS_T
{
   MMAL_PARAMETER_HEADER_T          hdr;
   uint32_t                         num_regions;      /**< Number of regions defined */
   MMAL_BOOL_T                      lock_to_faces;    /**< If region is within tolerance of a face, adopt face rect instead of defined region */
   MMAL_PARAMETER_FOCUS_REGION_T    regions[1];       /**< Variable number of regions */
} MMAL_PARAMETER_FOCUS_REGIONS_T;

typedef struct MMAL_PARAMETER_INPUT_CROP_T
{
   MMAL_PARAMETER_HEADER_T hdr;
   MMAL_RECT_T             rect;    /**< Crop rectangle as 16P16 fixed point values */
} MMAL_PARAMETER_INPUT_CROP_T;

typedef struct MMAL_PARAMETER_SENSOR_INFORMATION_T
{
   MMAL_PARAMETER_HEADER_T          hdr;
   MMAL_RATIONAL_T                  f_number;         /**< Lens f-number */
   MMAL_RATIONAL_T                  focal_length;     /**< Lens focal length */
   uint32_t                         model_id;         /**< Sensor reported model id */
   uint32_t                         manufacturer_id;  /**< Sensor reported manufacturer id */
   uint32_t                         revision;         /**< Sensor reported revision */
} MMAL_PARAMETER_SENSOR_INFORMATION_T;

typedef struct MMAL_PARAMETER_FLASH_SELECT_T
{
   MMAL_PARAMETER_HEADER_T          hdr;
   MMAL_PARAMETER_CAMERA_INFO_FLASH_TYPE_T flash_type;   /**< Flash type to use */
} MMAL_PARAMETER_FLASH_SELECT_T;

typedef struct MMAL_PARAMETER_FIELD_OF_VIEW_T
{
   MMAL_PARAMETER_HEADER_T          hdr;
   MMAL_RATIONAL_T                  fov_h;         /**< Horizontal field of view */
   MMAL_RATIONAL_T                  fov_v;         /**< Vertical field of view */
} MMAL_PARAMETER_FIELD_OF_VIEW_T;

typedef enum MMAL_PARAMETER_DRC_STRENGTH_T
{
   MMAL_PARAMETER_DRC_STRENGTH_OFF,
   MMAL_PARAMETER_DRC_STRENGTH_LOW,
   MMAL_PARAMETER_DRC_STRENGTH_MEDIUM,
   MMAL_PARAMETER_DRC_STRENGTH_HIGH,
   MMAL_PARAMETER_DRC_STRENGTH_MAX = 0x7fffffff
} MMAL_PARAMETER_DRC_STRENGTH_T;

typedef struct MMAL_PARAMETER_DRC_T
{
   MMAL_PARAMETER_HEADER_T          hdr;
   MMAL_PARAMETER_DRC_STRENGTH_T    strength;      /**< DRC strength */
} MMAL_PARAMETER_DRC_T;

typedef enum MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_T
{
   MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_FACETRACKING,
   MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_REDEYE_REDUCTION,
   MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_VIDEO_STABILISATION,
   MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_WRITE_RAW,
   MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_VIDEO_DENOISE,
   MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_STILLS_DENOISE,
   MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_TEMPORAL_DENOISE,
   MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_ANTISHAKE,
   MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_IMAGE_EFFECTS,
   MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_DYNAMIC_RANGE_COMPRESSION,
   MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_FACE_RECOGNITION,
   MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_FACE_BEAUTIFICATION,
   MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_SCENE_DETECTION,
   MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_HIGH_DYNAMIC_RANGE,
   MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_MAX = 0x7fffffff
} MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_T;

typedef struct MMAL_PARAMETER_ALGORITHM_CONTROL_T
{
   MMAL_PARAMETER_HEADER_T          hdr;
   MMAL_PARAMETER_ALGORITHM_CONTROL_ALGORITHMS_T algorithm;
   MMAL_BOOL_T                      enabled;
} MMAL_PARAMETER_ALGORITHM_CONTROL_T;


typedef enum MMAL_PARAM_CAMERA_USE_CASE_T
{
   MMAL_PARAM_CAMERA_USE_CASE_UNKNOWN,             /**< Compromise on behaviour as use case totally unknown */
   MMAL_PARAM_CAMERA_USE_CASE_STILLS_CAPTURE,      /**< Stills capture use case */
   MMAL_PARAM_CAMERA_USE_CASE_VIDEO_CAPTURE,       /**< Video encode (camcorder) use case */

   MMAL_PARAM_CAMERA_USE_CASE_MAX = 0x7fffffff
} MMAL_PARAM_CAMERA_USE_CASE_T;

typedef struct MMAL_PARAMETER_CAMERA_USE_CASE_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_PARAM_CAMERA_USE_CASE_T use_case;   /**< Use case */
} MMAL_PARAMETER_CAMERA_USE_CASE_T;

typedef struct MMAL_PARAMETER_FPS_RANGE_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_RATIONAL_T   fps_low;                /**< Low end of the permitted framerate range */
   MMAL_RATIONAL_T   fps_high;               /**< High end of the permitted framerate range */
} MMAL_PARAMETER_FPS_RANGE_T;

typedef struct MMAL_PARAMETER_ZEROSHUTTERLAG_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_BOOL_T zero_shutter_lag_mode;        /**< Select zero shutter lag mode from sensor */
   MMAL_BOOL_T concurrent_capture;           /**< Activate full zero shutter lag mode and
                                              *  use the last preview raw image for the stills capture
                                              */
} MMAL_PARAMETER_ZEROSHUTTERLAG_T;

typedef struct MMAL_PARAMETER_AWB_GAINS_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_RATIONAL_T r_gain;                   /**< Red gain */
   MMAL_RATIONAL_T b_gain;                   /**< Blue gain */
} MMAL_PARAMETER_AWB_GAINS_T;

typedef struct MMAL_PARAMETER_CAMERA_SETTINGS_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   uint32_t exposure;
   MMAL_RATIONAL_T analog_gain;
   MMAL_RATIONAL_T digital_gain;
   MMAL_RATIONAL_T awb_red_gain;
   MMAL_RATIONAL_T awb_blue_gain;
   uint32_t focus_position;
} MMAL_PARAMETER_CAMERA_SETTINGS_T;

typedef enum MMAL_PARAM_PRIVACY_INDICATOR_T
{
   MMAL_PARAMETER_PRIVACY_INDICATOR_OFF,        /**< Indicator will be off. */
   MMAL_PARAMETER_PRIVACY_INDICATOR_ON,         /**< Indicator will come on just after a stills capture and
                                                 *   and remain on for 2seconds, or will be on whilst output[1]
                                                 *   is actively producing images.
                                                 */
   MMAL_PARAMETER_PRIVACY_INDICATOR_FORCE_ON,   /**< Turns indicator of for 2s independent of capture status.
                                                 *   Set this mode repeatedly to keep the indicator on for a
                                                 *   longer period.
                                                 */
   MMAL_PARAMETER_PRIVACY_INDICATOR_MAX = 0x7fffffff
} MMAL_PARAM_PRIVACY_INDICATOR_T;

typedef struct MMAL_PARAMETER_PRIVACY_INDICATOR_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_PARAM_PRIVACY_INDICATOR_T mode;
} MMAL_PARAMETER_PRIVACY_INDICATOR_T;

#define MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN 32
typedef struct MMAL_PARAMETER_CAMERA_ANNOTATE_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_BOOL_T enable;
   char text[MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN];
   MMAL_BOOL_T show_shutter;
   MMAL_BOOL_T show_analog_gain;
   MMAL_BOOL_T show_lens;
   MMAL_BOOL_T show_caf;
   MMAL_BOOL_T show_motion;
} MMAL_PARAMETER_CAMERA_ANNOTATE_T;

#define MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V2 256
typedef struct MMAL_PARAMETER_CAMERA_ANNOTATE_V2_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_BOOL_T enable;
   MMAL_BOOL_T show_shutter;
   MMAL_BOOL_T show_analog_gain;
   MMAL_BOOL_T show_lens;
   MMAL_BOOL_T show_caf;
   MMAL_BOOL_T show_motion;
   MMAL_BOOL_T show_frame_num;
   MMAL_BOOL_T black_text_background;
   char text[MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V2];
} MMAL_PARAMETER_CAMERA_ANNOTATE_V2_T;

#define MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3 256
typedef struct MMAL_PARAMETER_CAMERA_ANNOTATE_V3_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_BOOL_T enable;
   MMAL_BOOL_T show_shutter;
   MMAL_BOOL_T show_analog_gain;
   MMAL_BOOL_T show_lens;
   MMAL_BOOL_T show_caf;
   MMAL_BOOL_T show_motion;
   MMAL_BOOL_T show_frame_num;
   MMAL_BOOL_T enable_text_background;
   MMAL_BOOL_T custom_background_colour;
   uint8_t     custom_background_Y;
   uint8_t     custom_background_U;
   uint8_t     custom_background_V;
   uint8_t     dummy1;
   MMAL_BOOL_T custom_text_colour;
   uint8_t     custom_text_Y;
   uint8_t     custom_text_U;
   uint8_t     custom_text_V;
   uint8_t     text_size;
   char text[MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3];
} MMAL_PARAMETER_CAMERA_ANNOTATE_V3_T;

#define MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V4 256
typedef struct MMAL_PARAMETER_CAMERA_ANNOTATE_V4_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_BOOL_T enable;
   MMAL_BOOL_T show_shutter;
   MMAL_BOOL_T show_analog_gain;
   MMAL_BOOL_T show_lens;
   MMAL_BOOL_T show_caf;
   MMAL_BOOL_T show_motion;
   MMAL_BOOL_T show_frame_num;
   MMAL_BOOL_T enable_text_background;
   MMAL_BOOL_T custom_background_colour;
   uint8_t     custom_background_Y;
   uint8_t     custom_background_U;
   uint8_t     custom_background_V;
   uint8_t     dummy1;
   MMAL_BOOL_T custom_text_colour;
   uint8_t     custom_text_Y;
   uint8_t     custom_text_U;
   uint8_t     custom_text_V;
   uint8_t     text_size;
   char text[MMAL_CAMERA_ANNOTATE_MAX_TEXT_LEN_V3];
   uint32_t    justify; //0=centre, 1=left, 2=right
   uint32_t    x_offset; //Offset from the justification edge
   uint32_t    y_offset;
} MMAL_PARAMETER_CAMERA_ANNOTATE_V4_T;

typedef enum MMAL_STEREOSCOPIC_MODE_T {
   MMAL_STEREOSCOPIC_MODE_NONE = 0,
   MMAL_STEREOSCOPIC_MODE_SIDE_BY_SIDE = 1,
   MMAL_STEREOSCOPIC_MODE_TOP_BOTTOM = 2,
   MMAL_STEREOSCOPIC_MODE_MAX = 0x7FFFFFFF,
} MMAL_STEREOSCOPIC_MODE_T;

typedef struct MMAL_PARAMETER_STEREOSCOPIC_MODE_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_STEREOSCOPIC_MODE_T mode;
   MMAL_BOOL_T decimate;
   MMAL_BOOL_T swap_eyes;
} MMAL_PARAMETER_STEREOSCOPIC_MODE_T;

typedef enum MMAL_CAMERA_INTERFACE_T {
   MMAL_CAMERA_INTERFACE_CSI2 = 0,
   MMAL_CAMERA_INTERFACE_CCP2 = 1,
   MMAL_CAMERA_INTERFACE_CPI = 2,
   MMAL_CAMERA_INTERFACE_MAX = 0x7FFFFFFF,
} MMAL_CAMERA_INTERFACE_T;

typedef struct MMAL_PARAMETER_CAMERA_INTERFACE_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_CAMERA_INTERFACE_T mode;
} MMAL_PARAMETER_CAMERA_INTERFACE_T;

typedef enum MMAL_CAMERA_CLOCKING_MODE_T {
   MMAL_CAMERA_CLOCKING_MODE_STROBE = 0,
   MMAL_CAMERA_CLOCKING_MODE_CLOCK = 1,
   MMAL_CAMERA_CLOCKING_MODE_MAX = 0x7FFFFFFF,
} MMAL_CAMERA_CLOCKING_MODE_T;

typedef struct MMAL_PARAMETER_CAMERA_CLOCKING_MODE_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_CAMERA_CLOCKING_MODE_T mode;
} MMAL_PARAMETER_CAMERA_CLOCKING_MODE_T;

typedef enum MMAL_CAMERA_RX_CONFIG_DECODE {
   MMAL_CAMERA_RX_CONFIG_DECODE_NONE = 0,
   MMAL_CAMERA_RX_CONFIG_DECODE_DPCM8TO10 = 1,
   MMAL_CAMERA_RX_CONFIG_DECODE_DPCM7TO10 = 2,
   MMAL_CAMERA_RX_CONFIG_DECODE_DPCM6TO10 = 3,
   MMAL_CAMERA_RX_CONFIG_DECODE_DPCM8TO12 = 4,
   MMAL_CAMERA_RX_CONFIG_DECODE_DPCM7TO12 = 5,
   MMAL_CAMERA_RX_CONFIG_DECODE_DPCM6TO12 = 6,
   MMAL_CAMERA_RX_CONFIG_DECODE_DPCM10TO14 = 7,
   MMAL_CAMERA_RX_CONFIG_DECODE_DPCM8TO14 = 8,
   MMAL_CAMERA_RX_CONFIG_DECODE_DPCM12TO16 = 9,
   MMAL_CAMERA_RX_CONFIG_DECODE_DPCM10TO16 = 10,
   MMAL_CAMERA_RX_CONFIG_DECODE_DPCM8TO16 = 11,
   MMAL_CAMERA_RX_CONFIG_DECODE_MAX = 0x7FFFFFFF
} MMAL_CAMERA_RX_CONFIG_DECODE;

typedef enum MMAL_CAMERA_RX_CONFIG_ENCODE {
   MMAL_CAMERA_RX_CONFIG_ENCODE_NONE = 0,
   MMAL_CAMERA_RX_CONFIG_ENCODE_DPCM10TO8 = 1,
   MMAL_CAMERA_RX_CONFIG_ENCODE_DPCM12TO8 = 2,
   MMAL_CAMERA_RX_CONFIG_ENCODE_DPCM14TO8 = 3,
   MMAL_CAMERA_RX_CONFIG_ENCODE_MAX = 0x7FFFFFFF
} MMAL_CAMERA_RX_CONFIG_ENCODE;

typedef enum MMAL_CAMERA_RX_CONFIG_UNPACK {
   MMAL_CAMERA_RX_CONFIG_UNPACK_NONE = 0,
   MMAL_CAMERA_RX_CONFIG_UNPACK_6 = 1,
   MMAL_CAMERA_RX_CONFIG_UNPACK_7 = 2,
   MMAL_CAMERA_RX_CONFIG_UNPACK_8 = 3,
   MMAL_CAMERA_RX_CONFIG_UNPACK_10 = 4,
   MMAL_CAMERA_RX_CONFIG_UNPACK_12 = 5,
   MMAL_CAMERA_RX_CONFIG_UNPACK_14 = 6,
   MMAL_CAMERA_RX_CONFIG_UNPACK_16 = 7,
   MMAL_CAMERA_RX_CONFIG_UNPACK_MAX = 0x7FFFFFFF
} MMAL_CAMERA_RX_CONFIG_UNPACK;

typedef enum MMAL_CAMERA_RX_CONFIG_PACK {
   MMAL_CAMERA_RX_CONFIG_PACK_NONE = 0,
   MMAL_CAMERA_RX_CONFIG_PACK_8 = 1,
   MMAL_CAMERA_RX_CONFIG_PACK_10 = 2,
   MMAL_CAMERA_RX_CONFIG_PACK_12 = 3,
   MMAL_CAMERA_RX_CONFIG_PACK_14 = 4,
   MMAL_CAMERA_RX_CONFIG_PACK_16 = 5,
   MMAL_CAMERA_RX_CONFIG_PACK_RAW10 = 6,
   MMAL_CAMERA_RX_CONFIG_PACK_RAW12 = 7,
   MMAL_CAMERA_RX_CONFIG_PACK_MAX = 0x7FFFFFFF
} MMAL_CAMERA_RX_CONFIG_PACK;

typedef struct MMAL_PARAMETER_CAMERA_RX_CONFIG_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_CAMERA_RX_CONFIG_DECODE decode;
   MMAL_CAMERA_RX_CONFIG_ENCODE encode;
   MMAL_CAMERA_RX_CONFIG_UNPACK unpack;
   MMAL_CAMERA_RX_CONFIG_PACK pack;
   uint32_t data_lanes;
   uint32_t encode_block_length;
   uint32_t embedded_data_lines;
   uint32_t image_id;
} MMAL_PARAMETER_CAMERA_RX_CONFIG_T;

typedef struct MMAL_PARAMETER_CAMERA_RX_TIMING_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   uint32_t timing1;
   uint32_t timing2;
   uint32_t timing3;
   uint32_t timing4;
   uint32_t timing5;
   uint32_t term1;
   uint32_t term2;
   uint32_t cpi_timing1;
   uint32_t cpi_timing2;
} MMAL_PARAMETER_CAMERA_RX_TIMING_T;

typedef struct MMAL_PARAMETER_LENS_SHADING_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_BOOL_T enabled;
   uint32_t grid_cell_size;
   uint32_t grid_width;
   uint32_t grid_stride;
   uint32_t grid_height;
   uint32_t mem_handle_table;
   uint32_t ref_transform;
} MMAL_PARAMETER_LENS_SHADING_T;

/*
The mode determines the kind of resize.
MMAL_RESIZE_BOX allow the max_width and max_height to set a bounding box into
which the output must fit.
MMAL_RESIZE_BYTES allows max_bytes to set the maximum number of bytes into which the
full output frame must fit.  Two flags aid the setting of the output
size. preserve_aspect_ratio sets whether the resize should
preserve the aspect ratio of the incoming
image. allow_upscaling sets whether the resize is allowed to
increase the size of the output image compared to the size of the
input image.
*/
typedef enum MMAL_RESIZEMODE_T {
   MMAL_RESIZE_NONE,
   MMAL_RESIZE_CROP,
   MMAL_RESIZE_BOX,
   MMAL_RESIZE_BYTES,
   MMAL_RESIZE_DUMMY = 0x7FFFFFFF
} MMAL_RESIZEMODE_T;

typedef struct MMAL_PARAMETER_RESIZE_T {
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_RESIZEMODE_T mode;
   uint32_t max_width;
   uint32_t max_height;
   uint32_t max_bytes;
   MMAL_BOOL_T preserve_aspect_ratio;
   MMAL_BOOL_T allow_upscaling;
} MMAL_PARAMETER_RESIZE_T;

typedef struct MMAL_PARAMETER_CROP_T {
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_RECT_T rect;
} MMAL_PARAMETER_CROP_T;

typedef struct MMAL_PARAMETER_CCM_T {
   MMAL_RATIONAL_T ccm[3][3];
   int32_t offsets[3];
} MMAL_PARAMETER_CCM_T;

typedef struct MMAL_PARAMETER_CUSTOM_CCM_T {
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_BOOL_T enable;           /**< Enable the custom CCM. */
   MMAL_PARAMETER_CCM_T ccm;     /**< CCM to be used. */
} MMAL_PARAMETER_CUSTOM_CCM_T;

#endif  /* MMAL_PARAMETERS_CAMERA_H */
