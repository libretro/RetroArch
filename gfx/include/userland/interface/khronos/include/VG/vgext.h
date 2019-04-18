/* $Revision: 6810 $ on $Date:: 2008-10-29 14:31:37 +0000 #$ */

/*------------------------------------------------------------------------
 *
 * VG extensions Reference Implementation
 * -------------------------------------
 *
 * Copyright (c) 2008 The Khronos Group Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and /or associated documentation files
 * (the "Materials "), to deal in the Materials without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Materials,
 * and to permit persons to whom the Materials are furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Materials.
 *
 * THE MATERIALS ARE PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE MATERIALS OR
 * THE USE OR OTHER DEALINGS IN THE MATERIALS.
 *
 *//**
 * \file
 * \brief	VG extensions
 *//*-------------------------------------------------------------------*/

#ifndef _VGEXT_H
#define _VGEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "openvg.h"
#include "vgu.h"

#ifndef VG_API_ENTRYP
#   define VG_API_ENTRYP VG_API_ENTRY*
#endif

#ifndef VGU_API_ENTRYP
#   define VGU_API_ENTRYP VGU_API_ENTRY*
#endif

/*-------------------------------------------------------------------------------
 * KHR extensions
 *------------------------------------------------------------------------------*/

typedef enum  {

#ifndef VG_KHR_iterative_average_blur
  VG_MAX_AVERAGE_BLUR_DIMENSION_KHR        = 0x116B,
  VG_AVERAGE_BLUR_DIMENSION_RESOLUTION_KHR = 0x116C,
  VG_MAX_AVERAGE_BLUR_ITERATIONS_KHR       = 0x116D,
#endif

  VG_PARAM_TYPE_KHR_FORCE_SIZE             = VG_MAX_ENUM
} VGParamTypeKHR;

#ifndef VG_KHR_EGL_image
#define VG_KHR_EGL_image 1
/* VGEGLImageKHR is an opaque handle to an EGLImage */
typedef void* VGeglImageKHR;

#ifdef VG_VGEXT_PROTOTYPES
VG_API_CALL VGImage VG_API_ENTRY vgCreateEGLImageTargetKHR(VGeglImageKHR image);
#endif
typedef VGImage (VG_API_ENTRYP PFNVGCREATEEGLIMAGETARGETKHRPROC) (VGeglImageKHR image);

#endif

#ifndef VG_KHR_iterative_average_blur
#define VG_KHR_iterative_average_blur 1

#ifdef VG_VGEXT_PROTOTYPES
VG_API_CALL void vgIterativeAverageBlurKHR(VGImage dst,VGImage src,VGfloat dimX,VGfloat dimY,VGuint iterative,VGTilingMode tilingMode);
#endif
typedef void (VG_API_ENTRYP PFNVGITERATIVEAVERAGEBLURKHRPROC) (VGImage dst,VGImage src,VGfloat dimX,VGfloat dimY,VGuint iterative,VGTilingMode tilingMode);

#endif

#ifndef VG_KHR_advanced_blending
#define VG_KHR_advanced_blending 1

typedef enum {
  VG_BLEND_OVERLAY_KHR        = 0x2010,
  VG_BLEND_HARDLIGHT_KHR      = 0x2011,
  VG_BLEND_SOFTLIGHT_SVG_KHR  = 0x2012,
  VG_BLEND_SOFTLIGHT_KHR      = 0x2013,
  VG_BLEND_COLORDODGE_KHR     = 0x2014,
  VG_BLEND_COLORBURN_KHR      = 0x2015,
  VG_BLEND_DIFFERENCE_KHR     = 0x2016,
  VG_BLEND_SUBTRACT_KHR       = 0x2017,
  VG_BLEND_INVERT_KHR         = 0x2018,
  VG_BLEND_EXCLUSION_KHR      = 0x2019,
  VG_BLEND_LINEARDODGE_KHR    = 0x201a,
  VG_BLEND_LINEARBURN_KHR     = 0x201b,
  VG_BLEND_VIVIDLIGHT_KHR     = 0x201c,
  VG_BLEND_LINEARLIGHT_KHR    = 0x201d,
  VG_BLEND_PINLIGHT_KHR       = 0x201e,
  VG_BLEND_HARDMIX_KHR        = 0x201f,
  VG_BLEND_CLEAR_KHR          = 0x2020,
  VG_BLEND_DST_KHR            = 0x2021,
  VG_BLEND_SRC_OUT_KHR        = 0x2022,
  VG_BLEND_DST_OUT_KHR        = 0x2023,
  VG_BLEND_SRC_ATOP_KHR       = 0x2024,
  VG_BLEND_DST_ATOP_KHR       = 0x2025,
  VG_BLEND_XOR_KHR            = 0x2026,

  VG_BLEND_MODE_KHR_FORCE_SIZE= VG_MAX_ENUM
} VGBlendModeKHR;
#endif

#ifndef VG_KHR_parametric_filter
#define VG_KHR_parametric_filter 1

typedef enum {
  VG_PF_OBJECT_VISIBLE_FLAG_KHR = (1 << 0),
  VG_PF_KNOCKOUT_FLAG_KHR       = (1 << 1),
  VG_PF_OUTER_FLAG_KHR          = (1 << 2),
  VG_PF_INNER_FLAG_KHR          = (1 << 3),

  VG_PF_TYPE_KHR_FORCE_SIZE     = VG_MAX_ENUM
} VGPfTypeKHR;

typedef enum {
  VGU_IMAGE_IN_USE_ERROR           = 0xF010,

  VGU_ERROR_CODE_KHR_FORCE_SIZE    = VG_MAX_ENUM
} VGUErrorCodeKHR;

#ifdef VG_VGEXT_PROTOTYPES
VG_API_CALL void VG_API_ENTRY vgParametricFilterKHR(VGImage dst,VGImage src,VGImage blur,VGfloat strength,VGfloat offsetX,VGfloat offsetY,VGbitfield filterFlags,VGPaint highlightPaint,VGPaint shadowPaint);
VGU_API_CALL VGUErrorCode VGU_API_ENTRY vguDropShadowKHR(VGImage dst,VGImage src,VGfloat dimX,VGfloat dimY,VGuint iterative,VGfloat strength,VGfloat distance,VGfloat angle,VGbitfield filterFlags,VGbitfield allowedQuality,VGuint shadowColorRGBA);
VGU_API_CALL VGUErrorCode VGU_API_ENTRY vguGlowKHR(VGImage dst,VGImage src,VGfloat dimX,VGfloat dimY,VGuint iterative,VGfloat strength,VGbitfield filterFlags,VGbitfield allowedQuality,VGuint glowColorRGBA) ;
VGU_API_CALL VGUErrorCode VGU_API_ENTRY vguBevelKHR(VGImage dst,VGImage src,VGfloat dimX,VGfloat dimY,VGuint iterative,VGfloat strength,VGfloat distance,VGfloat angle,VGbitfield filterFlags,VGbitfield allowedQuality,VGuint highlightColorRGBA,VGuint shadowColorRGBA);
VGU_API_CALL VGUErrorCode VGU_API_ENTRY vguGradientGlowKHR(VGImage dst,VGImage src,VGfloat dimX,VGfloat dimY,VGuint iterative,VGfloat strength,VGfloat distance,VGfloat angle,VGbitfield filterFlags,VGbitfield allowedQuality,VGuint stopsCount,const VGfloat* glowColorRampStops);
VGU_API_CALL VGUErrorCode VGU_API_ENTRY vguGradientBevelKHR(VGImage dst,VGImage src,VGfloat dimX,VGfloat dimY,VGuint iterative,VGfloat strength,VGfloat distance,VGfloat angle,VGbitfield filterFlags,VGbitfield allowedQuality,VGuint stopsCount,const VGfloat* bevelColorRampStops);
#endif
typedef void (VG_API_ENTRYP PFNVGPARAMETRICFILTERKHRPROC) (VGImage dst,VGImage src,VGImage blur,VGfloat strength,VGfloat offsetX,VGfloat offsetY,VGbitfield filterFlags,VGPaint highlightPaint,VGPaint shadowPaint);
typedef VGUErrorCode (VGU_API_ENTRYP PFNVGUDROPSHADOWKHRPROC) (VGImage dst,VGImage src,VGfloat dimX,VGfloat dimY,VGuint iterative,VGfloat strength,VGfloat distance,VGfloat angle,VGbitfield filterFlags,VGbitfield allowedQuality,VGuint shadowColorRGBA);
typedef VGUErrorCode (VGU_API_ENTRYP PFNVGUGLOWKHRPROC) (VGImage dst,VGImage src,VGfloat dimX,VGfloat dimY,VGuint iterative,VGfloat strength,VGbitfield filterFlags,VGbitfield allowedQuality,VGuint glowColorRGBA);
typedef VGUErrorCode (VGU_API_ENTRYP PFNVGUBEVELKHRPROC) (VGImage dst,VGImage src,VGfloat dimX,VGfloat dimY,VGuint iterative,VGfloat strength,VGfloat distance,VGfloat angle,VGbitfield filterFlags,VGbitfield allowedQuality,VGuint highlightColorRGBA,VGuint shadowColorRGBA);
typedef VGUErrorCode (VGU_API_ENTRYP PFNVGUGRADIENTGLOWKHRPROC) (VGImage dst,VGImage src,VGfloat dimX,VGfloat dimY,VGuint iterative,VGfloat strength,VGfloat distance,VGfloat angle,VGbitfield filterFlags,VGbitfield allowedQuality,VGuint stopsCount,const VGfloat* glowColorRampStops);
typedef VGUErrorCode (VGU_API_ENTRYP PFNVGUGRADIENTBEVELKHRPROC) (VGImage dst,VGImage src,VGfloat dimX,VGfloat dimY,VGuint iterative,VGfloat strength,VGfloat distance,VGfloat angle,VGbitfield filterFlags,VGbitfield allowedQuality,VGuint stopsCount,const VGfloat* bevelColorRampStops);

#endif

/*-------------------------------------------------------------------------------
 * NDS extensions
 *------------------------------------------------------------------------------*/

#ifndef VG_NDS_paint_generation
#define VG_NDS_paint_generation 1

typedef enum {
  VG_PAINT_COLOR_RAMP_LINEAR_NDS            = 0x1A10,
  VG_COLOR_MATRIX_NDS                       = 0x1A11,
  VG_PAINT_COLOR_TRANSFORM_LINEAR_NDS       = 0x1A12,

  VG_PAINT_PARAM_TYPE_NDS_FORCE_SIZE        = VG_MAX_ENUM
} VGPaintParamTypeNds;

typedef enum {
  VG_DRAW_IMAGE_COLOR_MATRIX_NDS            = 0x1F10,

  VG_IMAGE_MODE_NDS_FORCE_SIZE              = VG_MAX_ENUM
} VGImageModeNds;
#endif

#ifndef VG_NDS_projective_geometry
#define VG_NDS_projective_geometry 1

typedef enum {
  VG_CLIP_MODE_NDS                          = 0x1180,
  VG_CLIP_LINES_NDS                         = 0x1181,
  VG_MAX_CLIP_LINES_NDS                     = 0x1182,

  VG_PARAM_TYPE_NDS_FORCE_SIZE        = VG_MAX_ENUM
} VGParamTypeNds;

typedef enum {
  VG_CLIPMODE_NONE_NDS                      = 0x3000,
  VG_CLIPMODE_CLIP_CLOSED_NDS               = 0x3001,
  VG_CLIPMODE_CLIP_OPEN_NDS                 = 0x3002,
  VG_CLIPMODE_CULL_NDS                      = 0x3003,

  VG_CLIPMODE_NDS_FORCE_SIZE = VG_MAX_ENUM
} VGClipModeNds;

typedef enum {
  VG_RQUAD_TO_NDS              = ( 13 << 1 ),
  VG_RCUBIC_TO_NDS             = ( 14 << 1 ),

  VG_PATH_SEGMENT_NDS_FORCE_SIZE = VG_MAX_ENUM
} VGPathSegmentNds;

typedef enum {
  VG_RQUAD_TO_ABS_NDS            = (VG_RQUAD_TO_NDS  | VG_ABSOLUTE),
  VG_RQUAD_TO_REL_NDS            = (VG_RQUAD_TO_NDS  | VG_RELATIVE),
  VG_RCUBIC_TO_ABS_NDS           = (VG_RCUBIC_TO_NDS | VG_ABSOLUTE),
  VG_RCUBIC_TO_REL_NDS           = (VG_RCUBIC_TO_NDS | VG_RELATIVE),

  VG_PATH_COMMAND_NDS_FORCE_SIZE = VG_MAX_ENUM
} VGPathCommandNds;

#ifdef VG_VGEXT_PROTOTYPES
VG_API_CALL void VG_API_ENTRY vgProjectiveMatrixNDS(VGboolean enable) ;
VGU_API_CALL VGUErrorCode VGU_API_ENTRY vguTransformClipLineNDS(const VGfloat Ain,const VGfloat Bin,const VGfloat Cin,const VGfloat* matrix,const VGboolean inverse,VGfloat* Aout,VGfloat* Bout,VGfloat* Cout);
#endif
typedef void (VG_API_ENTRYP PFNVGPROJECTIVEMATRIXNDSPROC) (VGboolean enable) ;
typedef VGUErrorCode (VGU_API_ENTRYP PFNVGUTRANSFORMCLIPLINENDSPROC) (const VGfloat Ain,const VGfloat Bin,const VGfloat Cin,const VGfloat* matrix,const VGboolean inverse,VGfloat* Aout,VGfloat* Bout,VGfloat* Cout);

#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _VGEXT_H */
