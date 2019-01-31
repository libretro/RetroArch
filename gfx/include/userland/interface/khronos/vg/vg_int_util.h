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

#ifndef VG_INT_UTIL_H
#define VG_INT_UTIL_H

#include "interface/khronos/common/khrn_int_util.h"
#include "interface/khronos/include/VG/openvg.h"
#include "interface/khronos/include/VG/vgu.h"

static INLINE bool is_matrix_mode(VGMatrixMode matrix_mode)
{
   return (matrix_mode >= VG_MATRIX_PATH_USER_TO_SURFACE) &&
          (matrix_mode <= VG_MATRIX_GLYPH_USER_TO_SURFACE);
}

static INLINE bool is_fill_rule(VGFillRule fill_rule)
{
   return (fill_rule == VG_EVEN_ODD) ||
          (fill_rule == VG_NON_ZERO);
}

static INLINE bool is_image_quality(VGImageQuality image_quality)
{
   return (image_quality == VG_IMAGE_QUALITY_NONANTIALIASED) ||
          (image_quality == VG_IMAGE_QUALITY_FASTER) ||
          (image_quality == VG_IMAGE_QUALITY_BETTER);
}

static INLINE bool is_rendering_quality(VGRenderingQuality rendering_quality)
{
   return (rendering_quality >= VG_RENDERING_QUALITY_NONANTIALIASED) &&
          (rendering_quality <= VG_RENDERING_QUALITY_BETTER);
}

static INLINE bool is_blend_mode(VGBlendMode blend_mode)
{
   return (blend_mode >= VG_BLEND_SRC) &&
          (blend_mode <= VG_BLEND_ADDITIVE);
}

static INLINE bool is_image_mode(VGImageMode image_mode)
{
   return (image_mode >= VG_DRAW_IMAGE_NORMAL) &&
          (image_mode <= VG_DRAW_IMAGE_STENCIL);
}

static INLINE bool is_cap_style(VGCapStyle cap_style)
{
   return (cap_style >= VG_CAP_BUTT) &&
          (cap_style <= VG_CAP_SQUARE);
}

static INLINE bool is_join_style(VGJoinStyle join_style)
{
   return (join_style >= VG_JOIN_MITER) &&
          (join_style <= VG_JOIN_BEVEL);
}

static INLINE bool is_pixel_layout(VGPixelLayout pixel_layout)
{
   return (pixel_layout >= VG_PIXEL_LAYOUT_UNKNOWN) &&
          (pixel_layout <= VG_PIXEL_LAYOUT_BGR_HORIZONTAL);
}

static INLINE bool is_paint_type(VGPaintType paint_type)
{
   return (paint_type >= VG_PAINT_TYPE_COLOR) &&
          (paint_type <= VG_PAINT_TYPE_PATTERN);
}

static INLINE bool is_color_ramp_spread_mode(VGColorRampSpreadMode color_ramp_spread_mode)
{
   return (color_ramp_spread_mode >= VG_COLOR_RAMP_SPREAD_PAD) &&
          (color_ramp_spread_mode <= VG_COLOR_RAMP_SPREAD_REFLECT);
}

static INLINE bool is_tiling_mode(VGTilingMode tiling_mode)
{
   return (tiling_mode >= VG_TILE_FILL) &&
          (tiling_mode <= VG_TILE_REFLECT);
}

static INLINE bool is_vector_param_type(VGParamType param_type)
{
   return (param_type == VG_SCISSOR_RECTS) ||
          (param_type == VG_COLOR_TRANSFORM_VALUES) ||
          (param_type == VG_STROKE_DASH_PATTERN) ||
          (param_type == VG_TILE_FILL_COLOR) ||
          (param_type == VG_CLEAR_COLOR) ||
          (param_type == VG_GLYPH_ORIGIN);
}

static INLINE bool is_vector_object_param_type(int32_t param_type)
{
   return (param_type == VG_PAINT_COLOR) ||
          (param_type == VG_PAINT_COLOR_RAMP_STOPS) ||
          (param_type == VG_PAINT_LINEAR_GRADIENT) ||
          (param_type == VG_PAINT_RADIAL_GRADIENT);
}

static INLINE bool is_path_format(int32_t path_format)
{
   return path_format == VG_PATH_FORMAT_STANDARD;
}

static INLINE bool is_path_datatype(VGPathDatatype path_datatype)
{
#ifdef __HIGHC__
   #pragma Offwarn(428) /* unsigned compare with 0 always true */
#endif
   return (path_datatype >= VG_PATH_DATATYPE_S_8) &&
          (path_datatype <= VG_PATH_DATATYPE_F);
#ifdef __HIGHC__
   #pragma Popwarn
#endif
}

static INLINE uint32_t get_path_datatype_size(VGPathDatatype path_datatype)
{
   switch (path_datatype) {
   case VG_PATH_DATATYPE_S_8:  return 1;
   case VG_PATH_DATATYPE_S_16: return sizeof(int16_t);
   case VG_PATH_DATATYPE_S_32: return sizeof(int32_t);
   case VG_PATH_DATATYPE_F:    return sizeof(float);
   default:                    UNREACHABLE(); return 0;
   }
}

static INLINE uint32_t get_segment_coords_count(uint32_t segment)
{
   switch (segment) {
   case VG_CLOSE_PATH: return 0;
   case VG_MOVE_TO:    return 2;
   case VG_LINE_TO:    return 2;
   case VG_HLINE_TO:   return 1;
   case VG_VLINE_TO:   return 1;
   case VG_QUAD_TO:    return 4;
   case VG_CUBIC_TO:   return 6;
   case VG_SQUAD_TO:   return 2;
   case VG_SCUBIC_TO:  return 4;
   case VG_SCCWARC_TO: return 5;
   case VG_SCWARC_TO:  return 5;
   case VG_LCCWARC_TO: return 5;
   case VG_LCWARC_TO:  return 5;
   default:            UNREACHABLE(); return 0;
   }
}

static INLINE float get_coord(
   VGPathDatatype datatype, float scale, float bias,
   const void **coords)
{
   switch (datatype) {
   case VG_PATH_DATATYPE_S_8:  return bias + (scale * *((*(const int8_t **)coords)++));
   case VG_PATH_DATATYPE_S_16: return bias + (scale * *((*(const int16_t **)coords)++));
   case VG_PATH_DATATYPE_S_32: return bias + (scale * *((*(const int32_t **)coords)++));
   case VG_PATH_DATATYPE_F:    return bias + (scale * *((*(const float **)coords)++));
   default:                    UNREACHABLE(); return 0.0f;
   }
}

static INLINE void put_coord(
   VGPathDatatype datatype, float oo_scale, float bias,
   void **coords, float x)
{
   x = oo_scale * (x - bias);
   switch (datatype) {
   case VG_PATH_DATATYPE_S_8:  *((*(int8_t **)coords)++) = (int8_t)clampi(float_to_int(x), -0x80, 0x7f); break;
   case VG_PATH_DATATYPE_S_16: *((*(int16_t **)coords)++) = (int16_t)clampi(float_to_int(x), -0x8000, 0x7fff); break;
   case VG_PATH_DATATYPE_S_32: *((*(int32_t **)coords)++) = float_to_int(x); break;
   case VG_PATH_DATATYPE_F:    *((*(float **)coords)++) = x; break;
   default:                    UNREACHABLE();
   }
}

static INLINE bool is_paint_modes(uint32_t paint_modes)
{
   return paint_modes && !(paint_modes & ~(VG_STROKE_PATH | VG_FILL_PATH));
}

static INLINE bool is_paint_mode(VGPaintMode paint_mode)
{
   return (paint_mode == VG_STROKE_PATH) ||
          (paint_mode == VG_FILL_PATH);
}

static INLINE bool is_allowed_quality(uint32_t allowed_quality)
{
   return allowed_quality && !(allowed_quality & ~(VG_IMAGE_QUALITY_NONANTIALIASED | VG_IMAGE_QUALITY_FASTER | VG_IMAGE_QUALITY_BETTER));
}

static INLINE bool is_hardware_query_type(VGHardwareQueryType hardware_query_type)
{
   return (hardware_query_type == VG_IMAGE_FORMAT_QUERY) ||
          (hardware_query_type == VG_PATH_DATATYPE_QUERY);
}

static INLINE bool is_image_format(VGImageFormat image_format)
{
#ifdef __HIGHC__
   #pragma Offwarn(428) /* unsigned compare with 0 always true */
#endif
   return ((image_format >= VG_sRGBX_8888) &&
          (image_format <= VG_A_4)) ||
          (image_format == VG_sXRGB_8888) ||
          (image_format == VG_sARGB_8888) ||
          (image_format == VG_sARGB_8888_PRE) ||
          (image_format == VG_sARGB_1555) ||
          (image_format == VG_sARGB_4444) ||
          (image_format == VG_lXRGB_8888) ||
          (image_format == VG_lARGB_8888) ||
          (image_format == VG_lARGB_8888_PRE) ||
          (image_format == VG_sBGRX_8888) ||
          (image_format == VG_sBGRA_8888) ||
          (image_format == VG_sBGRA_8888_PRE) ||
          (image_format == VG_sBGR_565) ||
          (image_format == VG_sBGRA_5551) ||
          (image_format == VG_sBGRA_4444) ||
          (image_format == VG_lBGRX_8888) ||
          (image_format == VG_lBGRA_8888) ||
          (image_format == VG_lBGRA_8888_PRE) ||
          (image_format == VG_sXBGR_8888) ||
          (image_format == VG_sABGR_8888) ||
          (image_format == VG_sABGR_8888_PRE) ||
          (image_format == VG_sABGR_1555) ||
          (image_format == VG_sABGR_4444) ||
          (image_format == VG_lXBGR_8888) ||
          (image_format == VG_lABGR_8888) ||
          (image_format == VG_lABGR_8888_PRE);
#ifdef __HIGHC__
   #pragma Popwarn
#endif
}

static INLINE bool is_arc_type(VGUArcType arc_type)
{
   return (arc_type >= VGU_ARC_OPEN) &&
          (arc_type <= VGU_ARC_PIE);
}

static INLINE bool is_mask_operation(VGMaskOperation mask_operation)
{
   return (mask_operation >= VG_CLEAR_MASK) &&
          (mask_operation <= VG_SUBTRACT_MASK);
}

static INLINE bool is_image_channel(VGImageChannel image_channel)
{
   return (image_channel == VG_RED) ||
          (image_channel == VG_GREEN) ||
          (image_channel == VG_BLUE) ||
          (image_channel == VG_ALPHA);
}

#endif
