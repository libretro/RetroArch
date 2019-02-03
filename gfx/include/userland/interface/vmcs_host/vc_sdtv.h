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
 * SDTV common host header for TV service
 */

#ifndef _VC_SDTV_H_
#define _VC_SDTV_H_

/** Different SDTV modes */
/** colour */
typedef enum SDTV_COLOUR_
{
   SDTV_COLOUR_UNKNOWN = 0x0,
   SDTV_COLOUR_RGB     = 0x4,
   SDTV_COLOUR_YPRPB   = 0x8,
   SDTV_COLOUR_FORCE_32BIT    = 0x80000000
} SDTV_COLOUR_T;
/** operation mode */
typedef enum SDTV_MODE_T_
{
   SDTV_MODE_NTSC       = 0, /**<Normal NTSC */
   SDTV_MODE_NTSC_J     = 1, /**<Japanese version of NTSC - no pedestal.*/
   SDTV_MODE_PAL        = 2, /**<Normal PAL */
   SDTV_MODE_PAL_M      = 3, /**<Brazilian version of PAL - 525/60 rather than 625/50, different subcarrier */
   SDTV_MODE_FORMAT_MASK = 0x3,

   SDTV_MODE_OUTPUT_MASK = 0xc,

   SDTV_MODE_YPRPB_480i = (SDTV_MODE_NTSC | SDTV_COLOUR_YPRPB),
   SDTV_MODE_RGB_480i   = (SDTV_MODE_NTSC | SDTV_COLOUR_RGB),
   SDTV_MODE_YPRPB_576i = (SDTV_MODE_PAL  | SDTV_COLOUR_YPRPB),
   SDTV_MODE_RGB_576i   = (SDTV_MODE_PAL  | SDTV_COLOUR_RGB),

   SDTV_MODE_PROGRESSIVE = 0x10, /**<240p progressive output*/

   SDTV_MODE_OFF        = 0x80,
   SDTV_MODE_FORCE_32BIT = 0x80000000
} SDTV_MODE_T;

/** Different aspect ratios */
typedef enum SDTV_ASPECT_T_
{
   // TODO: extend this to allow picture placement/size to be communicated.
   SDTV_ASPECT_UNKNOWN  = 0, /**<Unknown */
   SDTV_ASPECT_4_3      = 1, /**<4:3 */
   SDTV_ASPECT_14_9     = 2, /**<14:9 */
   SDTV_ASPECT_16_9     = 3, /**<16:9 */
   SDTV_ASPECTFORCE_32BIT = 0x80000000
} SDTV_ASPECT_T;

/** SDTV power on option */
typedef struct SDTV_OPTIONS_T_
{
   SDTV_ASPECT_T   aspect;
} SDTV_OPTIONS_T;

/**
 * Different copy protection modes
 * At the moment we have only implemented Macrovision
 */
typedef enum
{
   SDTV_CP_NONE              = 0, /**<No copy protection */
   SDTV_CP_MACROVISION_TYPE1 = 1, /**<Macrovision Type 1 */
   SDTV_CP_MACROVISION_TYPE2 = 2, /**<Macrovision Type 2 */
   SDTV_CP_MACROVISION_TYPE3 = 3, /**<Macrovision Type 3 */
   SDTV_CP_MACROVISION_TEST1 = 4, /**<Macrovision Test 1 */
   SDTV_CP_MACROVISION_TEST2 = 5, /**<Macrovision Test 2 */
   SDTV_CP_CGMS_COPYFREE     = 6, /**<CGMS copy freely */
   SDTV_CP_CGMS_COPYNOMORE   = 7, /**<CGMS copy no more */
   SDTV_CP_CGMS_COPYONCE     = 8, /**<CGMS copy once */
   SDTV_CP_CGMS_COPYNEVER    = 9, /**<CGMS copy never */
   SDTV_CP_WSS_COPYFREE      = 10, /**<WSS no restriction */
   SDTV_CP_WSS_COPYRIGHT_COPYFREE = 11, /**<WSS copyright asserted */
   SDTV_CP_WSS_NOCOPY        = 12, /**<WSS copying restricted */
   SDTV_CP_WSS_COPYRIGHT_NOCOPY = 13, /**<WSS copying restriced, copyright asserted */
   SDTV_CP_FORCE_32BIT = 0x80000000
} SDTV_CP_MODE_T;

/**
 * SDTV internal state
 */
typedef struct {
   uint32_t state;
   uint32_t width;
   uint32_t height;
   uint16_t frame_rate;
   uint16_t scan_mode;
   SDTV_MODE_T mode;
   SDTV_OPTIONS_T display_options;
   SDTV_COLOUR_T colour;
   SDTV_CP_MODE_T cp_mode;
} SDTV_DISPLAY_STATE_T;

/**
 * SDTV notifications
 */
typedef enum
{
   VC_SDTV_UNPLUGGED          = 1 << 16, /**<SDTV cable unplugged, subject to platform support */
   VC_SDTV_ATTACHED           = 1 << 17, /**<SDTV cable is plugged in */
   VC_SDTV_NTSC               = 1 << 18, /**<SDTV is in NTSC mode */
   VC_SDTV_PAL                = 1 << 19, /**<SDTV is in PAL mode */
   VC_SDTV_CP_INACTIVE        = 1 << 20, /**<Copy protection disabled */
   VC_SDTV_CP_ACTIVE          = 1 << 21  /**<Copy protection enabled */
} VC_SDTV_NOTIFY_T;
#define VC_SDTV_STANDBY (VC_SDTV_ATTACHED) /* For backward code compatibility, to be consistent with HDMI */

/**
 * Callback reason and arguments from vec middleware
 * Each callback comes with two optional uint32_t parameters.
 * Reason                     param1       param2      remark
 * VC_SDTV_UNPLUGGED            -            -         cable is unplugged
 * VC_SDTV_STANDBY              -            -         cable is plugged in
 * VC_SDTV_NTSC              SDTV_MODE_T SDTV_ASPECT_T NTSC mode active with said aspect ratio
 * VC_SDTV_PAL               SDTV_MODE_T SDTV_ASPECT_T PAL  mode active with said aspect ratio
 * VC_SDTV_CP_INACTIVE          -            -         copy protection is inactive
 * VC_SDTV_CP_ACTIVE         SDTV_CP_MODE_T  -         copy protection is active
 */

#endif
