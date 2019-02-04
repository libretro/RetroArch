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

#ifndef MMAL_PARAMETERS_VIDEO_H
#define MMAL_PARAMETERS_VIDEO_H

#include "mmal_parameters_common.h"

/*************************************************
 * ALWAYS ADD NEW ENUMS AT THE END OF THIS LIST! *
 ************************************************/

/** Video-specific MMAL parameter IDs.
 * @ingroup MMAL_PARAMETER_IDS
 */
enum {
   MMAL_PARAMETER_DISPLAYREGION           /**< Takes a @ref MMAL_DISPLAYREGION_T */
         = MMAL_PARAMETER_GROUP_VIDEO,
   MMAL_PARAMETER_SUPPORTED_PROFILES,     /**< Takes a @ref MMAL_PARAMETER_VIDEO_PROFILE_T */
   MMAL_PARAMETER_PROFILE,                /**< Takes a @ref MMAL_PARAMETER_VIDEO_PROFILE_T */
   MMAL_PARAMETER_INTRAPERIOD,            /**< Takes a @ref MMAL_PARAMETER_UINT32_T */
   MMAL_PARAMETER_RATECONTROL,            /**< Takes a @ref MMAL_PARAMETER_VIDEO_RATECONTROL_T */
   MMAL_PARAMETER_NALUNITFORMAT,          /**< Takes a @ref MMAL_PARAMETER_VIDEO_NALUNITFORMAT_T */
   MMAL_PARAMETER_MINIMISE_FRAGMENTATION, /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_MB_ROWS_PER_SLICE,      /**< Takes a @ref MMAL_PARAMETER_UINT32_T.
                                           * Setting the value to zero resets to the default (one slice per frame). */
   MMAL_PARAMETER_VIDEO_LEVEL_EXTENSION,  /**< Takes a @ref MMAL_PARAMETER_VIDEO_LEVEL_EXTENSION_T */
   MMAL_PARAMETER_VIDEO_EEDE_ENABLE,      /**< Takes a @ref MMAL_PARAMETER_VIDEO_EEDE_ENABLE_T */
   MMAL_PARAMETER_VIDEO_EEDE_LOSSRATE,    /**< Takes a @ref MMAL_PARAMETER_VIDEO_EEDE_LOSSRATE_T */
   MMAL_PARAMETER_VIDEO_REQUEST_I_FRAME,  /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T.
                                           * Request an I-frame. */
   MMAL_PARAMETER_VIDEO_INTRA_REFRESH,    /**< Takes a @ref MMAL_PARAMETER_VIDEO_INTRA_REFRESH_T */
   MMAL_PARAMETER_VIDEO_IMMUTABLE_INPUT,  /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T. */
   MMAL_PARAMETER_VIDEO_BIT_RATE,         /**< Takes a @ref MMAL_PARAMETER_UINT32_T.
                                           * Run-time bit rate control */
   MMAL_PARAMETER_VIDEO_FRAME_RATE,       /**< Takes a @ref MMAL_PARAMETER_FRAME_RATE_T */
   MMAL_PARAMETER_VIDEO_ENCODE_MIN_QUANT, /**< Takes a @ref MMAL_PARAMETER_UINT32_T. */
   MMAL_PARAMETER_VIDEO_ENCODE_MAX_QUANT, /**< Takes a @ref MMAL_PARAMETER_UINT32_T. */
   MMAL_PARAMETER_VIDEO_ENCODE_RC_MODEL,  /**< Takes a @ref MMAL_PARAMETER_VIDEO_ENCODE_RC_MODEL_T. */
   MMAL_PARAMETER_EXTRA_BUFFERS,          /**< Takes a @ref MMAL_PARAMETER_UINT32_T. */
   MMAL_PARAMETER_VIDEO_ALIGN_HORIZ,      /**< Takes a @ref MMAL_PARAMETER_UINT32_T.
                                               Changing this paramater from the default can reduce frame rate
                                               because image buffers need to be re-pitched.*/
   MMAL_PARAMETER_VIDEO_ALIGN_VERT,        /**< Takes a @ref MMAL_PARAMETER_UINT32_T.
                                               Changing this paramater from the default can reduce frame rate
                                               because image buffers need to be re-pitched.*/
   MMAL_PARAMETER_VIDEO_DROPPABLE_PFRAMES,      /**< Take a @ref MMAL_PARAMETER_BOOLEAN_T. */
   MMAL_PARAMETER_VIDEO_ENCODE_INITIAL_QUANT,   /**< Takes a @ref MMAL_PARAMETER_UINT32_T. */
   MMAL_PARAMETER_VIDEO_ENCODE_QP_P,            /**< Takes a @ref MMAL_PARAMETER_UINT32_T. */
   MMAL_PARAMETER_VIDEO_ENCODE_RC_SLICE_DQUANT, /**< Takes a @ref MMAL_PARAMETER_UINT32_T. */
   MMAL_PARAMETER_VIDEO_ENCODE_FRAME_LIMIT_BITS,    /**< Takes a @ref MMAL_PARAMETER_UINT32_T */
   MMAL_PARAMETER_VIDEO_ENCODE_PEAK_RATE,       /**< Takes a @ref MMAL_PARAMETER_UINT32_T. */

   /*H264 specific parameters*/
   MMAL_PARAMETER_VIDEO_ENCODE_H264_DISABLE_CABAC,      /**< Take a @ref MMAL_PARAMETER_BOOLEAN_T. */
   MMAL_PARAMETER_VIDEO_ENCODE_H264_LOW_LATENCY,        /**< Take a @ref MMAL_PARAMETER_BOOLEAN_T. */
   MMAL_PARAMETER_VIDEO_ENCODE_H264_AU_DELIMITERS,      /**< Take a @ref MMAL_PARAMETER_BOOLEAN_T. */
   MMAL_PARAMETER_VIDEO_ENCODE_H264_DEBLOCK_IDC,        /**< Takes a @ref MMAL_PARAMETER_UINT32_T. */
   MMAL_PARAMETER_VIDEO_ENCODE_H264_MB_INTRA_MODE,      /**< Takes a @ref MMAL_PARAMETER_VIDEO_ENCODER_H264_MB_INTRA_MODES_T. */

   MMAL_PARAMETER_VIDEO_ENCODE_HEADER_ON_OPEN,  /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_VIDEO_ENCODE_PRECODE_FOR_QP,  /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */

   MMAL_PARAMETER_VIDEO_DRM_INIT_INFO,          /**< Takes a @ref MMAL_PARAMETER_VIDEO_DRM_INIT_INFO_T. */
   MMAL_PARAMETER_VIDEO_TIMESTAMP_FIFO,         /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_VIDEO_DECODE_ERROR_CONCEALMENT,        /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_VIDEO_DRM_PROTECT_BUFFER,              /**< Takes a @ref MMAL_PARAMETER_VIDEO_DRM_PROTECT_BUFFER_T. */

   MMAL_PARAMETER_VIDEO_DECODE_CONFIG_VD3,       /**< Takes a @ref MMAL_PARAMETER_BYTES_T */
   MMAL_PARAMETER_VIDEO_ENCODE_H264_VCL_HRD_PARAMETERS, /**< Take a @ref MMAL_PARAMETER_BOOLEAN_T. */
   MMAL_PARAMETER_VIDEO_ENCODE_H264_LOW_DELAY_HRD_FLAG, /**< Take a @ref MMAL_PARAMETER_BOOLEAN_T. */
   MMAL_PARAMETER_VIDEO_ENCODE_INLINE_HEADER,            /**< Take a @ref MMAL_PARAMETER_BOOLEAN_T. */
   MMAL_PARAMETER_VIDEO_ENCODE_SEI_ENABLE,               /**< Take a @ref MMAL_PARAMETER_BOOLEAN_T. */
   MMAL_PARAMETER_VIDEO_ENCODE_INLINE_VECTORS,           /**< Take a @ref MMAL_PARAMETER_BOOLEAN_T. */
   MMAL_PARAMETER_VIDEO_RENDER_STATS,           /**< Take a @ref MMAL_PARAMETER_VIDEO_RENDER_STATS_T. */
   MMAL_PARAMETER_VIDEO_INTERLACE_TYPE,           /**< Take a @ref MMAL_PARAMETER_VIDEO_INTERLACE_TYPE_T. */
   MMAL_PARAMETER_VIDEO_INTERPOLATE_TIMESTAMPS,         /**< Takes a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_VIDEO_ENCODE_SPS_TIMING,         /**< Take a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_VIDEO_MAX_NUM_CALLBACKS,         /**< Take a @ref MMAL_PARAMETER_UINT32_T */
   MMAL_PARAMETER_VIDEO_SOURCE_PATTERN,         /**< Take a @ref MMAL_PARAMETER_SOURCE_PATTERN_T */
   MMAL_PARAMETER_VIDEO_ENCODE_SEPARATE_NAL_BUFS,  /**< Take a @ref MMAL_PARAMETER_BOOLEAN_T */
   MMAL_PARAMETER_VIDEO_DROPPABLE_PFRAME_LENGTH,   /**< Take a @ref MMAL_PARAMETER_UINT32_T */
};

/** Display transformations.
 * Although an enumeration, the values correspond to combinations of:
 * \li 1 Reflect in a vertical axis
 * \li 2 180 degree rotation
 * \li 4 Reflect in the leading diagonal
 */
typedef enum MMAL_DISPLAYTRANSFORM_T {
   MMAL_DISPLAY_ROT0 = 0,
   MMAL_DISPLAY_MIRROR_ROT0 = 1,
   MMAL_DISPLAY_MIRROR_ROT180 = 2,
   MMAL_DISPLAY_ROT180 = 3,
   MMAL_DISPLAY_MIRROR_ROT90 = 4,
   MMAL_DISPLAY_ROT270 = 5,
   MMAL_DISPLAY_ROT90 = 6,
   MMAL_DISPLAY_MIRROR_ROT270 = 7,
   MMAL_DISPLAY_DUMMY = 0x7FFFFFFF
} MMAL_DISPLAYTRANSFORM_T;

/** Display modes. */
typedef enum MMAL_DISPLAYMODE_T {
   MMAL_DISPLAY_MODE_FILL = 0,
   MMAL_DISPLAY_MODE_LETTERBOX = 1,
   // these allow a left eye source->dest to be specified and the right eye mapping will be inferred by symmetry
   MMAL_DISPLAY_MODE_STEREO_LEFT_TO_LEFT = 2,
   MMAL_DISPLAY_MODE_STEREO_TOP_TO_TOP = 3,
   MMAL_DISPLAY_MODE_STEREO_LEFT_TO_TOP = 4,
   MMAL_DISPLAY_MODE_STEREO_TOP_TO_LEFT = 5,
   MMAL_DISPLAY_MODE_DUMMY = 0x7FFFFFFF
} MMAL_DISPLAYMODE_T;

/** Values used to indicate which fields are used when setting the
 * display configuration */
typedef enum MMAL_DISPLAYSET_T {
   MMAL_DISPLAY_SET_NONE = 0,
   MMAL_DISPLAY_SET_NUM = 1,
   MMAL_DISPLAY_SET_FULLSCREEN = 2,
   MMAL_DISPLAY_SET_TRANSFORM = 4,
   MMAL_DISPLAY_SET_DEST_RECT = 8,
   MMAL_DISPLAY_SET_SRC_RECT = 0x10,
   MMAL_DISPLAY_SET_MODE = 0x20,
   MMAL_DISPLAY_SET_PIXEL = 0x40,
   MMAL_DISPLAY_SET_NOASPECT = 0x80,
   MMAL_DISPLAY_SET_LAYER = 0x100,
   MMAL_DISPLAY_SET_COPYPROTECT = 0x200,
   MMAL_DISPLAY_SET_ALPHA = 0x400,
   MMAL_DISPLAY_SET_DUMMY = 0x7FFFFFFF
} MMAL_DISPLAYSET_T;

typedef enum MMAL_DISPLAYALPHAFLAGS_T {
  MMAL_DISPLAY_ALPHA_FLAGS_NONE = 0,
  /**< Discard all lower layers as if this layer were fullscreen and completely
   * opaque. This flag removes the lower layers from the display list, therefore
   * avoiding using resources in wasted effort.
   */
  MMAL_DISPLAY_ALPHA_FLAGS_DISCARD_LOWER_LAYERS = 1<<29,
  /**< Alpha values are already premultiplied */
  MMAL_DISPLAY_ALPHA_FLAGS_PREMULT = 1<<30,
  /**< Mix the per pixel alpha (if present) and the per plane alpha. */
  MMAL_DISPLAY_ALPHA_FLAGS_MIX = 1<<31,
} MMAL_DISPLAYALPHAFLAGS_T;

/**
This config sets the output display device, as well as the region used
on the output display, any display transformation, and some flags to
indicate how to scale the image.
*/

typedef struct MMAL_DISPLAYREGION_T {
   MMAL_PARAMETER_HEADER_T hdr;
   /** Bitfield that indicates which fields are set and should be used. All
    * other fields will maintain their current value.
    * \ref MMAL_DISPLAYSET_T defines the bits that can be combined.
    */
   uint32_t set;
   /** Describes the display output device, with 0 typically being a directly
    * connected LCD display.  The actual values will depend on the hardware.
    * Code using hard-wired numbers (e.g. 2) is certain to fail.
    */
   uint32_t display_num;
   /** Indicates that we are using the full device screen area, rather than
    * a window of the display.  If zero, then dest_rect is used to specify a
    * region of the display to use.
    */
   MMAL_BOOL_T fullscreen;
   /** Indicates any rotation or flipping used to map frames onto the natural
    * display orientation.
    */
   MMAL_DISPLAYTRANSFORM_T transform;
   /** Where to display the frame within the screen, if fullscreen is zero.
    */
   MMAL_RECT_T dest_rect;
   /** Indicates which area of the frame to display. If all values are zero,
    * the whole frame will be used.
    */
   MMAL_RECT_T src_rect;
   /** If set to non-zero, indicates that any display scaling should disregard
    * the aspect ratio of the frame region being displayed.
    */
   MMAL_BOOL_T noaspect;
   /** Indicates how the image should be scaled to fit the display. \code
    * MMAL_DISPLAY_MODE_FILL \endcode indicates that the image should fill the
    * screen by potentially cropping the frames.  Setting \code mode \endcode
    * to \code MMAL_DISPLAY_MODE_LETTERBOX \endcode indicates that all the source
    * region should be displayed and black bars added if necessary.
    */
   MMAL_DISPLAYMODE_T mode;
   /** If non-zero, defines the width of a source pixel relative to \code pixel_y
    * \endcode.  If zero, then pixels default to being square.
    */
   uint32_t pixel_x;
   /** If non-zero, defines the height of a source pixel relative to \code pixel_x
    * \endcode.  If zero, then pixels default to being square.
    */
   uint32_t pixel_y;
   /** Sets the relative depth of the images, with greater values being in front
    * of smaller values.
    */
   int32_t layer;
   /** Set to non-zero to ensure copy protection is used on output.
    */
   MMAL_BOOL_T copyprotect_required;
   /** Bits 7-0: Level of opacity of the layer, where zero is fully transparent and
    * 255 is fully opaque.
    * Bits 31-8: Flags from \code MMAL_DISPLAYALPHAFLAGS_T for alpha mode selection.
    */
   uint32_t alpha;
} MMAL_DISPLAYREGION_T;

/** Video profiles.
 * Only certain combinations of profile and level will be valid.
 * @ref MMAL_VIDEO_LEVEL_T
 */
typedef enum MMAL_VIDEO_PROFILE_T {
    MMAL_VIDEO_PROFILE_H263_BASELINE,
    MMAL_VIDEO_PROFILE_H263_H320CODING,
    MMAL_VIDEO_PROFILE_H263_BACKWARDCOMPATIBLE,
    MMAL_VIDEO_PROFILE_H263_ISWV2,
    MMAL_VIDEO_PROFILE_H263_ISWV3,
    MMAL_VIDEO_PROFILE_H263_HIGHCOMPRESSION,
    MMAL_VIDEO_PROFILE_H263_INTERNET,
    MMAL_VIDEO_PROFILE_H263_INTERLACE,
    MMAL_VIDEO_PROFILE_H263_HIGHLATENCY,
    MMAL_VIDEO_PROFILE_MP4V_SIMPLE,
    MMAL_VIDEO_PROFILE_MP4V_SIMPLESCALABLE,
    MMAL_VIDEO_PROFILE_MP4V_CORE,
    MMAL_VIDEO_PROFILE_MP4V_MAIN,
    MMAL_VIDEO_PROFILE_MP4V_NBIT,
    MMAL_VIDEO_PROFILE_MP4V_SCALABLETEXTURE,
    MMAL_VIDEO_PROFILE_MP4V_SIMPLEFACE,
    MMAL_VIDEO_PROFILE_MP4V_SIMPLEFBA,
    MMAL_VIDEO_PROFILE_MP4V_BASICANIMATED,
    MMAL_VIDEO_PROFILE_MP4V_HYBRID,
    MMAL_VIDEO_PROFILE_MP4V_ADVANCEDREALTIME,
    MMAL_VIDEO_PROFILE_MP4V_CORESCALABLE,
    MMAL_VIDEO_PROFILE_MP4V_ADVANCEDCODING,
    MMAL_VIDEO_PROFILE_MP4V_ADVANCEDCORE,
    MMAL_VIDEO_PROFILE_MP4V_ADVANCEDSCALABLE,
    MMAL_VIDEO_PROFILE_MP4V_ADVANCEDSIMPLE,
    MMAL_VIDEO_PROFILE_H264_BASELINE,
    MMAL_VIDEO_PROFILE_H264_MAIN,
    MMAL_VIDEO_PROFILE_H264_EXTENDED,
    MMAL_VIDEO_PROFILE_H264_HIGH,
    MMAL_VIDEO_PROFILE_H264_HIGH10,
    MMAL_VIDEO_PROFILE_H264_HIGH422,
    MMAL_VIDEO_PROFILE_H264_HIGH444,
    MMAL_VIDEO_PROFILE_H264_CONSTRAINED_BASELINE,
    MMAL_VIDEO_PROFILE_DUMMY = 0x7FFFFFFF
} MMAL_VIDEO_PROFILE_T;

/** Video levels.
 * Only certain combinations of profile and level will be valid.
 * @ref MMAL_VIDEO_PROFILE_T
 */
typedef enum MMAL_VIDEO_LEVEL_T {
    MMAL_VIDEO_LEVEL_H263_10,
    MMAL_VIDEO_LEVEL_H263_20,
    MMAL_VIDEO_LEVEL_H263_30,
    MMAL_VIDEO_LEVEL_H263_40,
    MMAL_VIDEO_LEVEL_H263_45,
    MMAL_VIDEO_LEVEL_H263_50,
    MMAL_VIDEO_LEVEL_H263_60,
    MMAL_VIDEO_LEVEL_H263_70,
    MMAL_VIDEO_LEVEL_MP4V_0,
    MMAL_VIDEO_LEVEL_MP4V_0b,
    MMAL_VIDEO_LEVEL_MP4V_1,
    MMAL_VIDEO_LEVEL_MP4V_2,
    MMAL_VIDEO_LEVEL_MP4V_3,
    MMAL_VIDEO_LEVEL_MP4V_4,
    MMAL_VIDEO_LEVEL_MP4V_4a,
    MMAL_VIDEO_LEVEL_MP4V_5,
    MMAL_VIDEO_LEVEL_MP4V_6,
    MMAL_VIDEO_LEVEL_H264_1,
    MMAL_VIDEO_LEVEL_H264_1b,
    MMAL_VIDEO_LEVEL_H264_11,
    MMAL_VIDEO_LEVEL_H264_12,
    MMAL_VIDEO_LEVEL_H264_13,
    MMAL_VIDEO_LEVEL_H264_2,
    MMAL_VIDEO_LEVEL_H264_21,
    MMAL_VIDEO_LEVEL_H264_22,
    MMAL_VIDEO_LEVEL_H264_3,
    MMAL_VIDEO_LEVEL_H264_31,
    MMAL_VIDEO_LEVEL_H264_32,
    MMAL_VIDEO_LEVEL_H264_4,
    MMAL_VIDEO_LEVEL_H264_41,
    MMAL_VIDEO_LEVEL_H264_42,
    MMAL_VIDEO_LEVEL_H264_5,
    MMAL_VIDEO_LEVEL_H264_51,
    MMAL_VIDEO_LEVEL_DUMMY = 0x7FFFFFFF
} MMAL_VIDEO_LEVEL_T;

/** Video profile and level setting.
 * This is a variable length structure when querying the supported profiles and
 * levels. To get more than one, pass a structure with more profile/level pairs.
 */
typedef struct MMAL_PARAMETER_VIDEO_PROFILE_T
{
   MMAL_PARAMETER_HEADER_T hdr;

   struct
   {
      MMAL_VIDEO_PROFILE_T profile;
      MMAL_VIDEO_LEVEL_T level;
   } profile[1];
} MMAL_PARAMETER_VIDEO_PROFILE_T;

/** Manner of video rate control */
typedef enum MMAL_VIDEO_RATECONTROL_T {
    MMAL_VIDEO_RATECONTROL_DEFAULT,
    MMAL_VIDEO_RATECONTROL_VARIABLE,
    MMAL_VIDEO_RATECONTROL_CONSTANT,
    MMAL_VIDEO_RATECONTROL_VARIABLE_SKIP_FRAMES,
    MMAL_VIDEO_RATECONTROL_CONSTANT_SKIP_FRAMES,
    MMAL_VIDEO_RATECONTROL_DUMMY = 0x7fffffff
} MMAL_VIDEO_RATECONTROL_T;

/** Intra refresh modes */
typedef enum MMAL_VIDEO_INTRA_REFRESH_T {
    MMAL_VIDEO_INTRA_REFRESH_CYCLIC,
    MMAL_VIDEO_INTRA_REFRESH_ADAPTIVE,
    MMAL_VIDEO_INTRA_REFRESH_BOTH,
    MMAL_VIDEO_INTRA_REFRESH_KHRONOSEXTENSIONS = 0x6F000000,
    MMAL_VIDEO_INTRA_REFRESH_VENDORSTARTUNUSED = 0x7F000000,
    MMAL_VIDEO_INTRA_REFRESH_CYCLIC_MROWS,
    MMAL_VIDEO_INTRA_REFRESH_PSEUDO_RAND,
    MMAL_VIDEO_INTRA_REFRESH_MAX,
    MMAL_VIDEO_INTRA_REFRESH_DUMMY         = 0x7FFFFFFF
} MMAL_VIDEO_INTRA_REFRESH_T;

/*Encode RC Models Supported*/
typedef enum MMAL_VIDEO_ENCODE_RC_MODEL_T {
    MMAL_VIDEO_ENCODER_RC_MODEL_DEFAULT    = 0,
    MMAL_VIDEO_ENCODER_RC_MODEL_JVT = MMAL_VIDEO_ENCODER_RC_MODEL_DEFAULT,
    MMAL_VIDEO_ENCODER_RC_MODEL_VOWIFI,
    MMAL_VIDEO_ENCODER_RC_MODEL_CBR,
    MMAL_VIDEO_ENCODER_RC_MODEL_LAST,
    MMAL_VIDEO_ENCODER_RC_MODEL_DUMMY      = 0x7FFFFFFF
} MMAL_VIDEO_ENCODE_RC_MODEL_T;

typedef struct MMAL_PARAMETER_VIDEO_ENCODE_RC_MODEL_T {
    MMAL_PARAMETER_HEADER_T hdr;
    MMAL_VIDEO_ENCODE_RC_MODEL_T rc_model;
}MMAL_PARAMETER_VIDEO_ENCODE_RC_MODEL_T;

/** Video rate control setting */
typedef struct MMAL_PARAMETER_VIDEO_RATECONTROL_T {
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_VIDEO_RATECONTROL_T control;
} MMAL_PARAMETER_VIDEO_RATECONTROL_T;

/*H264 INTRA MB MODES*/
typedef enum MMAL_VIDEO_ENCODE_H264_MB_INTRA_MODES_T {
    MMAL_VIDEO_ENCODER_H264_MB_4x4_INTRA = 1,
    MMAL_VIDEO_ENCODER_H264_MB_8x8_INTRA = 2,
    MMAL_VIDEO_ENCODER_H264_MB_16x16_INTRA = 4,
    MMAL_VIDEO_ENCODER_H264_MB_INTRA_DUMMY = 0x7fffffff
} MMAL_VIDEO_ENCODE_H264_MB_INTRA_MODES_T;

typedef struct MMAL_PARAMETER_VIDEO_ENCODER_H264_MB_INTRA_MODES_T {
    MMAL_PARAMETER_HEADER_T hdr;
    MMAL_VIDEO_ENCODE_H264_MB_INTRA_MODES_T mb_mode;
}MMAL_PARAMETER_VIDEO_ENCODER_H264_MB_INTRA_MODES_T;

/** NAL unit formats */
typedef enum MMAL_VIDEO_NALUNITFORMAT_T {
    MMAL_VIDEO_NALUNITFORMAT_STARTCODES = 1,
    MMAL_VIDEO_NALUNITFORMAT_NALUNITPERBUFFER = 2,
    MMAL_VIDEO_NALUNITFORMAT_ONEBYTEINTERLEAVELENGTH = 4,
    MMAL_VIDEO_NALUNITFORMAT_TWOBYTEINTERLEAVELENGTH = 8,
    MMAL_VIDEO_NALUNITFORMAT_FOURBYTEINTERLEAVELENGTH = 16,
    MMAL_VIDEO_NALUNITFORMAT_DUMMY = 0x7fffffff
} MMAL_VIDEO_NALUNITFORMAT_T;

/** NAL unit format setting */
typedef struct MMAL_PARAMETER_VIDEO_NALUNITFORMAT_T {
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_VIDEO_NALUNITFORMAT_T format;
} MMAL_PARAMETER_VIDEO_NALUNITFORMAT_T;

/** H264 Only: Overrides for max macro-blocks per second, max framesize,
 * and max bitrates. This overrides the default maximums for the configured level.
 */
typedef struct MMAL_PARAMETER_VIDEO_LEVEL_EXTENSION_T {
   MMAL_PARAMETER_HEADER_T hdr;

   uint32_t custom_max_mbps;
   uint32_t custom_max_fs;
   uint32_t custom_max_br_and_cpb;
} MMAL_PARAMETER_VIDEO_LEVEL_EXTENSION_T;

/** H264 Only: Overrides for max macro-blocks per second, max framesize,
 * and max bitrates. This overrides the default maximums for the configured level.
 */
typedef struct MMAL_PARAMETER_VIDEO_INTRA_REFRESH_T {
   MMAL_PARAMETER_HEADER_T hdr;

    MMAL_VIDEO_INTRA_REFRESH_T refresh_mode;
    uint32_t air_mbs;
    uint32_t air_ref;
    uint32_t cir_mbs;
    uint32_t pir_mbs;
} MMAL_PARAMETER_VIDEO_INTRA_REFRESH_T;

/** Structure for enabling EEDE, we keep it like this for now, there could be extra fields in the future */
typedef struct MMAL_PARAMETER_VIDEO_EEDE_ENABLE_T {
   MMAL_PARAMETER_HEADER_T hdr;

   uint32_t enable;
} MMAL_PARAMETER_VIDEO_EEDE_ENABLE_T;

/** Structure for setting lossrate for EEDE, we keep it like this for now, there could be extra fields in the future */
typedef struct MMAL_PARAMETER_VIDEO_EEDE_LOSSRATE_T {
   MMAL_PARAMETER_HEADER_T hdr;

   uint32_t loss_rate;
} MMAL_PARAMETER_VIDEO_EEDE_LOSSRATE_T;

/** Structure for setting initial DRM parameters */
typedef struct MMAL_PARAMETER_VIDEO_DRM_INIT_INFO_T {
   MMAL_PARAMETER_HEADER_T hdr;

   uint32_t current_time;
   uint32_t ticks_per_sec;
   uint8_t  lhs[32];
} MMAL_PARAMETER_VIDEO_DRM_INIT_INFO_T;

/** Structure for requesting a hardware-protected memory buffer **/
typedef struct MMAL_PARAMETER_VIDEO_DRM_PROTECT_BUFFER_T {
   MMAL_PARAMETER_HEADER_T hdr;

   uint32_t size_wanted;     /**< Input. Zero size means internal video decoder buffer,
                                 mem_handle and phys_addr not returned in this case */
   uint32_t protect;         /**< Input. 1 = protect, 0 = unprotect */

   uint32_t mem_handle;      /**< Output. Handle for protected buffer */
   void *   phys_addr;       /**< Output. Physical memory address of protected buffer */

} MMAL_PARAMETER_VIDEO_DRM_PROTECT_BUFFER_T;

typedef struct MMAL_PARAMETER_VIDEO_RENDER_STATS_T {
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_BOOL_T valid;
   uint32_t match;
   uint32_t period;
   uint32_t phase;
   uint32_t pixel_clock_nominal;
   uint32_t pixel_clock;
   uint32_t hvs_status;
   uint32_t dummy[2];
} MMAL_PARAMETER_VIDEO_RENDER_STATS_T;

typedef enum MMAL_INTERLACETYPE_T {
   MMAL_InterlaceProgressive,                    /**< The data is not interlaced, it is progressive scan */
   MMAL_InterlaceFieldSingleUpperFirst,          /**< The data is interlaced, fields sent
                                                     separately in temporal order, with upper field first */
   MMAL_InterlaceFieldSingleLowerFirst,          /**< The data is interlaced, fields sent
                                                     separately in temporal order, with lower field first */
   MMAL_InterlaceFieldsInterleavedUpperFirst,    /**< The data is interlaced, two fields sent together line
                                                     interleaved, with the upper field temporally earlier */
   MMAL_InterlaceFieldsInterleavedLowerFirst,    /**< The data is interlaced, two fields sent together line
                                                     interleaved, with the lower field temporally earlier */
   MMAL_InterlaceMixed,                          /**< The stream may contain a mixture of progressive
                                                     and interlaced frames */
   MMAL_InterlaceKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
   MMAL_InterlaceVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
   MMAL_InterlaceMax = 0x7FFFFFFF
} MMAL_INTERLACETYPE_T;

typedef struct MMAL_PARAMETER_VIDEO_INTERLACE_TYPE_T {
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_INTERLACETYPE_T eMode;       /**< The interlace type of the content */
   MMAL_BOOL_T bRepeatFirstField;    /**< Whether to repeat the first field */
} MMAL_PARAMETER_VIDEO_INTERLACE_TYPE_T;

typedef enum MMAL_SOURCE_PATTERN_T {
   MMAL_VIDEO_SOURCE_PATTERN_WHITE,
   MMAL_VIDEO_SOURCE_PATTERN_BLACK,
   MMAL_VIDEO_SOURCE_PATTERN_DIAGONAL,
   MMAL_VIDEO_SOURCE_PATTERN_NOISE,
   MMAL_VIDEO_SOURCE_PATTERN_RANDOM,
   MMAL_VIDEO_SOURCE_PATTERN_COLOUR,
   MMAL_VIDEO_SOURCE_PATTERN_BLOCKS,
   MMAL_VIDEO_SOURCE_PATTERN_SWIRLY,
   MMAL_VIDEO_SOURCE_PATTERN_DUMMY = 0x7fffffff
} MMAL_SOURCE_PATTERN_T;

typedef struct MMAL_PARAMETER_VIDEO_SOURCE_PATTERN_T {
   MMAL_PARAMETER_HEADER_T hdr;

   MMAL_SOURCE_PATTERN_T pattern;
   uint32_t param;                              /**< Colour for PATTERN_COLOUR mode */
   uint32_t framecount;                         /**< Number of frames to produce. 0 for continuous. */
   MMAL_RATIONAL_T framerate;                   /**< Framerate used when determining buffer timestamps */
} MMAL_PARAMETER_VIDEO_SOURCE_PATTERN_T;

#endif
