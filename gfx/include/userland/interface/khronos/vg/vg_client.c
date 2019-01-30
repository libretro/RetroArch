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
Copyright (c) 2008 Broadcom Europe Limited.
All rights reserved.

Project  :  khronos
Module   :  VG client

FILE DESCRIPTION
VG client-side function definitions. Dispatches VG calls via RPC or direct
call. Some functions require support for control messages just over 1kB in
length, 2kB should be fine for all functions.
=============================================================================*/

/*
   Potential spec bugs:

   vgImageSubData, vgGetImageSubData, vgWritePixels, and vgReadPixels require
      the data pointer to be aligned, but do not require the stride to be
      aligned. This seems a little useless, so we require that both the pointer
      and stride are aligned (unless height is 1, in which case we just require
      that the pointer is aligned)
*/

#define VG_VGEXT_PROTOTYPES /* we want the prototypes so the compiler will check that the signatures match */

#include "interface/khronos/common/khrn_client_mangle.h"
#include "interface/khronos/common/khrn_int_common.h"

#include "interface/khronos/common/khrn_client_platform.h"
#include "interface/khronos/common/khrn_client_rpc.h"
#include "interface/khronos/vg/vg_client.h"

#include "interface/khronos/egl/egl_client_config.h"

#include "interface/khronos/common/khrn_int_color.h"
#include "interface/khronos/common/khrn_int_math.h"
#include "interface/khronos/common/khrn_int_util.h"

#include "interface/khronos/include/VG/openvg.h"
#include "interface/khronos/include/VG/vgext.h"
#include "interface/khronos/include/VG/vgu.h"

#include "interface/khronos/vg/vg_int_config.h"
#include "interface/khronos/vg/vg_int_mat3x3.h"
#ifdef RPC_DIRECT
   #include "interface/khronos/vg/vg_int_impl.h" /* for _impl function calls */
#endif
#if defined(WIN32) || defined(__mips__)
#include "interface/khronos/common/khrn_int_misc_impl.h"
#endif

#include "interface/khronos/vg/vg_int_util.h"

#include <stdlib.h>
#include <string.h>

/******************************************************************************
shared state
******************************************************************************/

VG_CLIENT_SHARED_STATE_T *vg_client_shared_state_alloc(void)
{
   VG_CLIENT_SHARED_STATE_T *shared_state;
   KHR_STATUS_T status;

   shared_state = (VG_CLIENT_SHARED_STATE_T *)khrn_platform_malloc(sizeof(VG_CLIENT_SHARED_STATE_T), "VG_CLIENT_SHARED_STATE_T");
   if (!shared_state) {
      return NULL;
   }

   status = platform_mutex_create(&shared_state->mutex);
   if (status != KHR_SUCCESS) {
      khrn_platform_free(shared_state);
      return NULL;
   }

   if (!khrn_pointer_map_init(&shared_state->objects, 128)) {
      platform_mutex_destroy(&shared_state->mutex);
      khrn_platform_free(shared_state);
      return NULL;
   }

   shared_state->ref_count = 1;
   shared_state->stems_count = 0;

   return shared_state;
}

static void object_free(void *object);

static void object_free_callback(KHRN_POINTER_MAP_T *pointer_map, uint32_t key, void *object, void *data)
{
   UNUSED(pointer_map);
   UNUSED(key);
   UNUSED(data);

   object_free(object);
}

void vg_client_shared_state_free(VG_CLIENT_SHARED_STATE_T *shared_state)
{
   vcos_assert(shared_state->ref_count == 0);
   khrn_pointer_map_iterate(&shared_state->objects, object_free_callback, NULL);
   khrn_pointer_map_term(&shared_state->objects);
   platform_mutex_destroy(&shared_state->mutex);
   khrn_platform_free(shared_state);
}

/******************************************************************************
state
******************************************************************************/

VG_CLIENT_STATE_T *vg_client_state_alloc(VG_CLIENT_SHARED_STATE_T *shared_state)
{
   VGuint i;

   VG_CLIENT_STATE_T *state = (VG_CLIENT_STATE_T *)khrn_platform_malloc(sizeof(VG_CLIENT_STATE_T), "VG_CLIENT_STATE_T");
   if (!state) {
      return NULL;
   }

   vg_client_shared_state_acquire(shared_state);
   state->shared_state = shared_state;

   state->render_callback = NULL;
   state->flush_callback = NULL;

   state->matrix_mode = VG_MATRIX_PATH_USER_TO_SURFACE;
   for (i = 0; i != ARR_COUNT(state->matrices); ++i) {
      vg_mat3x3_set_identity(&state->matrices[i].client);
      vg_mat3x3_set_identity(&state->matrices[i].server);
   }

   state->fill_rule = VG_EVEN_ODD;
   state->stroke_line_width = 1.0f;
   state->stroke_cap_style = VG_CAP_BUTT;
   state->stroke_join_style = VG_JOIN_MITER;
   state->stroke_miter_limit = 4.0f;
   state->stroke_dash_pattern_count = 0;
   state->stroke_dash_phase = 0.0f;
   state->stroke_dash_phase_reset = false;
   state->image_quality = VG_IMAGE_QUALITY_FASTER;
   state->image_mode = VG_DRAW_IMAGE_NORMAL;

   state->scissoring = false;
   state->scissor_rects_count = 0;

   state->rendering_quality = VG_RENDERING_QUALITY_BETTER;

   state->fill_paint = VG_INVALID_HANDLE;
   state->stroke_paint = VG_INVALID_HANDLE;
   state->tile_fill_color[0] = 0.0f;
   state->tile_fill_color[1] = 0.0f;
   state->tile_fill_color[2] = 0.0f;
   state->tile_fill_color[3] = 0.0f;
   state->clear_color[0] = 0.0f;
   state->clear_color[1] = 0.0f;
   state->clear_color[2] = 0.0f;
   state->clear_color[3] = 0.0f;

   state->color_transform = false;
   state->color_transform_values[0] = 1.0f;
   state->color_transform_values[1] = 1.0f;
   state->color_transform_values[2] = 1.0f;
   state->color_transform_values[3] = 1.0f;
   state->color_transform_values[4] = 0.0f;
   state->color_transform_values[5] = 0.0f;
   state->color_transform_values[6] = 0.0f;
   state->color_transform_values[7] = 0.0f;

   state->blend_mode = VG_BLEND_SRC_OVER;
   state->masking = false;

   state->filter_format_linear = false;
   state->filter_format_pre = false;
   state->filter_channel_mask = VG_RED | VG_GREEN | VG_BLUE | VG_ALPHA;

   state->pixel_layout = VG_PIXEL_LAYOUT_UNKNOWN;

   return state;
}

void vg_client_state_free(VG_CLIENT_STATE_T *state)
{
   vg_client_shared_state_release(state->shared_state);
   khrn_platform_free(state);
}

/******************************************************************************
helpers
******************************************************************************/

static void clear_error(void)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   RPC_CALL0(vgClearError_impl,
             thread,
             VGCLEARERROR_ID);
}

static void set_error(VGErrorCode error)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   RPC_CALL1(vgSetError_impl,
             thread,
             VGSETERROR_ID,
             RPC_ENUM(error));
}

static VGErrorCode get_error(void)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   return (VGErrorCode)RPC_ENUM_RES(RPC_CALL0_RES(vgGetError_impl,
                                                  thread,
                                                  VGGETERROR_ID));
}

static INLINE bool is_aligned(const void *p, VGuint alignment)
{
   vcos_assert(is_power_of_2(alignment));
   return ((uintptr_t)p & (alignment - 1)) == 0;
}

static INLINE bool is_aligned_float(const VGfloat *p)
{
   return is_aligned(p, alignof(VGfloat));
}

static INLINE bool is_aligned_short(const VGshort *p)
{
   return is_aligned(p, alignof(VGshort));
}

static INLINE bool is_aligned_int(const VGint *p)
{
   return is_aligned(p, alignof(VGint));
}

static INLINE bool is_aligned_uint(const VGuint *p)
{
   return is_aligned(p, alignof(VGuint));
}

static bool is_aligned_path_datatype(const void *p, VGPathDatatype path_datatype)
{
   switch (path_datatype) {
   case VG_PATH_DATATYPE_S_8:  return true;
   case VG_PATH_DATATYPE_S_16: return is_aligned_short((const VGshort *)p);
   case VG_PATH_DATATYPE_S_32: return is_aligned_int((const VGint *)p);
   case VG_PATH_DATATYPE_F:    return is_aligned_float((const VGfloat *)p);
   default:                    UNREACHABLE(); return false;
   }
}

static bool is_aligned_image_format(const void *p, VGImageFormat image_format)
{
   switch (image_format) {
   case VG_sRGBX_8888:
   case VG_sRGBA_8888:
   case VG_sRGBA_8888_PRE:
   case VG_lRGBX_8888:
   case VG_lRGBA_8888:
   case VG_lRGBA_8888_PRE:
   case VG_sXRGB_8888:
   case VG_sARGB_8888:
   case VG_sARGB_8888_PRE:
   case VG_lXRGB_8888:
   case VG_lARGB_8888:
   case VG_lARGB_8888_PRE:
   case VG_sBGRX_8888:
   case VG_sBGRA_8888:
   case VG_sBGRA_8888_PRE:
   case VG_lBGRX_8888:
   case VG_lBGRA_8888:
   case VG_lBGRA_8888_PRE:
   case VG_sXBGR_8888:
   case VG_sABGR_8888:
   case VG_sABGR_8888_PRE:
   case VG_lXBGR_8888:
   case VG_lABGR_8888:
   case VG_lABGR_8888_PRE:
      return is_aligned_uint((const VGuint *)p);
   case VG_sRGB_565:
   case VG_sRGBA_5551:
   case VG_sRGBA_4444:
   case VG_sARGB_1555:
   case VG_sARGB_4444:
   case VG_sBGR_565:
   case VG_sBGRA_5551:
   case VG_sBGRA_4444:
   case VG_sABGR_1555:
   case VG_sABGR_4444:
      return is_aligned_short((const VGshort *)p);
   case VG_sL_8:
   case VG_lL_8:
   case VG_A_8:
   case VG_BW_1:
   case VG_A_1:
   case VG_A_4:
      return true;
   default:
      UNREACHABLE();
      return false;
   }
}

static bool contains_illegal_segment(const VGubyte *segments, VGint segments_count)
{
   for (; segments_count != 0; ++segments, --segments_count) {
      switch (*segments & ~VG_RELATIVE) {
      case VG_CLOSE_PATH: break;
      case VG_MOVE_TO:    break;
      case VG_LINE_TO:    break;
      case VG_HLINE_TO:   break;
      case VG_VLINE_TO:   break;
      case VG_QUAD_TO:    break;
      case VG_CUBIC_TO:   break;
      case VG_SQUAD_TO:   break;
      case VG_SCUBIC_TO:  break;
      case VG_SCCWARC_TO: break;
      case VG_SCWARC_TO:  break;
      case VG_LCCWARC_TO: break;
      case VG_LCWARC_TO:  break;
      default:            return true;
      }
   }
   return false;
}

static VGuint get_coords_count(const VGubyte *segments, VGint segments_count)
{
   VGuint coords_count = 0;
   for (; segments_count != 0; ++segments, --segments_count) {
      coords_count += get_segment_coords_count(*segments & ~VG_RELATIVE);
   }
   return coords_count;
}

static VGuint normalise_segment(VGuint segment)
{
   switch (segment) {
   case VG_CLOSE_PATH: return VG_CLOSE_PATH;
   case VG_MOVE_TO:    return VG_MOVE_TO;
   case VG_LINE_TO:    return VG_LINE_TO;
   case VG_HLINE_TO:   return VG_LINE_TO;
   case VG_VLINE_TO:   return VG_LINE_TO;
   case VG_QUAD_TO:    return VG_CUBIC_TO;
   case VG_CUBIC_TO:   return VG_CUBIC_TO;
   case VG_SQUAD_TO:   return VG_CUBIC_TO;
   case VG_SCUBIC_TO:  return VG_CUBIC_TO;

   /*
      on the client-side, we don't care about the different arc types, just
      normalise them all to the same type
   */

   case VG_SCCWARC_TO: return VG_SCCWARC_TO;
   case VG_SCWARC_TO:  return VG_SCCWARC_TO;
   case VG_LCCWARC_TO: return VG_SCCWARC_TO;
   case VG_LCWARC_TO:  return VG_SCCWARC_TO;
   default:            UNREACHABLE(); return 0;
   }
}

static bool interpolate_segments_compatible(const VGubyte *begin_segments, const VGubyte *end_segments, VGuint segments_count)
{
   for (; segments_count != 0; ++begin_segments, ++end_segments, --segments_count) {
      if (normalise_segment(*begin_segments & ~VG_RELATIVE) != normalise_segment(*end_segments & ~VG_RELATIVE)) {
         return false;
      }
   }
   return true;
}

#ifndef RPC_DIRECT

static VGuint get_log2_image_format_bpp(VGImageFormat image_format)
{
   switch (image_format) {
   case VG_sRGBX_8888:
   case VG_sRGBA_8888:
   case VG_sRGBA_8888_PRE:
   case VG_lRGBX_8888:
   case VG_lRGBA_8888:
   case VG_lRGBA_8888_PRE:
   case VG_sXRGB_8888:
   case VG_sARGB_8888:
   case VG_sARGB_8888_PRE:
   case VG_lXRGB_8888:
   case VG_lARGB_8888:
   case VG_lARGB_8888_PRE:
   case VG_sBGRX_8888:
   case VG_sBGRA_8888:
   case VG_sBGRA_8888_PRE:
   case VG_lBGRX_8888:
   case VG_lBGRA_8888:
   case VG_lBGRA_8888_PRE:
   case VG_sXBGR_8888:
   case VG_sABGR_8888:
   case VG_sABGR_8888_PRE:
   case VG_lXBGR_8888:
   case VG_lABGR_8888:
   case VG_lABGR_8888_PRE:
      return 5;
   case VG_sRGB_565:
   case VG_sRGBA_5551:
   case VG_sRGBA_4444:
   case VG_sARGB_1555:
   case VG_sARGB_4444:
   case VG_sBGR_565:
   case VG_sBGRA_5551:
   case VG_sBGRA_4444:
   case VG_sABGR_1555:
   case VG_sABGR_4444:
      return 4;
   case VG_sL_8:
   case VG_lL_8:
   case VG_A_8:
      return 3;
   case VG_A_4:
      return 2;
   case VG_BW_1:
   case VG_A_1:
      return 0;
   default:
      UNREACHABLE();
      return 0;
   }
}

#endif

static VGint param_to_int(
   bool are_floats,
   const void *values,
   VGuint i)
{
   vcos_assert(values);
   return are_floats ?
      float_to_int_floor(clean_float(((const VGfloat *)values)[i])) :
      ((const VGint *)values)[i];
}

static bool params_to_ints(
   VGint *ints,
   bool are_floats,
   const void *values,
   VGuint count)
{
   bool changed = false;
   VGuint i;
   for (i = 0; i != count; ++i) {
      VGint x = param_to_int(are_floats, values, i);
      if (ints[i] != x) {
         changed = true;
         ints[i] = x;
      }
   }
   return changed;
}

static VGfloat param_to_float(
   bool are_floats,
   const void *values,
   VGuint i)
{
   vcos_assert(values);
   return are_floats ?
      ((const VGfloat *)values)[i] :
      (VGfloat)((const VGint *)values)[i];
}

static bool params_to_floats(
   VGfloat *floats,
   bool are_floats,
   const void *values,
   VGuint count)
{
   bool changed = false;
   VGuint i;
   for (i = 0; i != count; ++i) {
      VGfloat x = param_to_float(are_floats, values, i);
      if (!floats_identical(floats[i], x)) {
         changed = true;
         floats[i] = x;
      }
   }
   return changed;
}

static void int_to_param(
   bool are_floats,
   void *values,
   VGuint i,
   VGint value)
{
   if (are_floats) {
      ((VGfloat *)values)[i] = (VGfloat)value;
   } else {
      ((VGint *)values)[i] = value;
   }
}

static void ints_to_params(
   bool are_floats,
   void *values,
   VGuint count,
   const VGint *ints)
{
   VGuint i;
   for (i = 0; i != count; ++i) {
      int_to_param(are_floats, values, i, ints[i]);
   }
}

static void float_to_param(
   bool are_floats,
   void *values,
   VGuint i,
   VGfloat value)
{
   if (are_floats) {
      ((VGfloat *)values)[i] = value;
   } else {
      ((VGint *)values)[i] = float_to_int_floor(clean_float(value));
   }
}

static void floats_to_params(
   bool are_floats,
   void *values,
   VGuint count,
   const VGfloat *floats)
{
   VGuint i;
   for (i = 0; i != count; ++i) {
      float_to_param(are_floats, values, i, floats[i]);
   }
}

static VGHandle get_stem(VG_CLIENT_STATE_T *state)
{
   VGHandle vg_handle;

   platform_mutex_acquire(&state->shared_state->mutex);

   if (state->shared_state->stems_count == 0) {
      CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
      RPC_HIGH_PRIORITY_BEGIN(thread);
      state->shared_state->stems_count = RPC_UINT_RES(RPC_CALL2_OUT_CTRL_RES(vgCreateStems_impl,
                                                                             thread,
                                                                             VGCREATESTEMS_ID,
                                                                             VG_CLIENT_STEMS_COUNT_MAX,
                                                                             state->shared_state->stems));
      RPC_HIGH_PRIORITY_END(thread);
   }
   vg_handle = (state->shared_state->stems_count == 0) ? VG_INVALID_HANDLE : state->shared_state->stems[--state->shared_state->stems_count];

   platform_mutex_release(&state->shared_state->mutex);

   return vg_handle;
}

static void destroy_stem(VGHandle vg_handle)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   RPC_CALL1(vgDestroyStem_impl,
             thread,
             VGDESTROYSTEM_ID,
             RPC_HANDLE(vg_handle));
}

static VG_CLIENT_FONT_T *font_alloc(void)
{
   VG_CLIENT_FONT_T *font = (VG_CLIENT_FONT_T *)khrn_platform_malloc(sizeof(VG_CLIENT_FONT_T), "VG_CLIENT_FONT_T");
   if (!font) {
      return NULL;
   }

   font->object_type = VG_CLIENT_OBJECT_TYPE_FONT;
#if EGL_BRCM_global_image && EGL_KHR_image
   if (!khrn_global_image_map_init(&font->glyph_global_images, 8)) {
      khrn_platform_free(font);
      return NULL;
   }
#endif

   return font;
}

static void font_free(VG_CLIENT_FONT_T *font)
{
#if EGL_BRCM_global_image && EGL_KHR_image
   khrn_global_image_map_term(&font->glyph_global_images);
#endif
   khrn_platform_free(font);
}

static VG_CLIENT_IMAGE_T *image_alloc(VGImageFormat format, VGint width, VGint height
#if EGL_BRCM_global_image && EGL_KHR_image
   , VGuint global_image_id_0, VGuint global_image_id_1
#endif
   )
{
   VG_CLIENT_IMAGE_T *image = (VG_CLIENT_IMAGE_T *)khrn_platform_malloc(sizeof(VG_CLIENT_IMAGE_T), "VG_CLIENT_IMAGE_T");
   if (!image) {
      return NULL;
   }

   image->object_type = VG_CLIENT_OBJECT_TYPE_IMAGE;
   image->format = format;
   image->width = width;
   image->height = height;
#if EGL_BRCM_global_image && EGL_KHR_image
   if (global_image_id_0 || global_image_id_1) {
      platform_acquire_global_image(global_image_id_0, global_image_id_1);
   }
   image->global_image_id[0] = global_image_id_0;
   image->global_image_id[1] = global_image_id_1;
#endif

   return image;
}

static void image_free(VG_CLIENT_IMAGE_T *image)
{
#if EGL_BRCM_global_image && EGL_KHR_image
   if (image->global_image_id[0] || image->global_image_id[1]) {
      platform_release_global_image(image->global_image_id[0], image->global_image_id[1]);
   }
#endif
   khrn_platform_free(image);
}

static VG_CLIENT_MASK_LAYER_T *mask_layer_alloc(VGint width, VGint height)
{
   VG_CLIENT_MASK_LAYER_T *mask_layer = (VG_CLIENT_MASK_LAYER_T *)khrn_platform_malloc(sizeof(VG_CLIENT_MASK_LAYER_T), "VG_CLIENT_MASK_LAYER_T");
   if (!mask_layer) {
      return NULL;
   }

   mask_layer->object_type = VG_CLIENT_OBJECT_TYPE_MASK_LAYER;
   mask_layer->width = width;
   mask_layer->height = height;

   return mask_layer;
}

static void mask_layer_free(VG_CLIENT_MASK_LAYER_T *mask_layer)
{
   khrn_platform_free(mask_layer);
}

static bool need_paint_gradient(VGint param_type)
{
   return (param_type == VG_PAINT_COLOR_RAMP_SPREAD_MODE) ||
          (param_type == VG_PAINT_COLOR_RAMP_PREMULTIPLIED) ||
          (param_type == VG_PAINT_COLOR_RAMP_STOPS) ||
          (param_type == VG_PAINT_LINEAR_GRADIENT) ||
          (param_type == VG_PAINT_RADIAL_GRADIENT);
}

static VG_CLIENT_PAINT_T *paint_alloc(void)
{
   VG_CLIENT_PAINT_T *paint = (VG_CLIENT_PAINT_T *)khrn_platform_malloc(sizeof(VG_CLIENT_PAINT_T), "VG_CLIENT_PAINT_T");
   if (!paint) {
      return NULL;
   }

   paint->object_type = VG_CLIENT_OBJECT_TYPE_PAINT;
   paint->type = VG_PAINT_TYPE_COLOR;
   paint->color[0] = 0.0f;
   paint->color[1] = 0.0f;
   paint->color[2] = 0.0f;
   paint->color[3] = 1.0f;
   paint->gradient = NULL; /* will be allocated when required */
   paint->pattern_tiling_mode = VG_TILE_FILL;
   paint->pattern = VG_INVALID_HANDLE;
#if EGL_BRCM_global_image && EGL_KHR_image
   paint->pattern_global_image_id[0] = 0;
   paint->pattern_global_image_id[1] = 0;
#endif

   return paint;
}

static bool paint_alloc_gradient(VG_CLIENT_PAINT_T *paint)
{
   VG_CLIENT_PAINT_GRADIENT_T *gradient;

   vcos_assert(!paint->gradient);

   gradient = (VG_CLIENT_PAINT_GRADIENT_T *)khrn_platform_malloc(sizeof(VG_CLIENT_PAINT_GRADIENT_T), "VG_CLIENT_PAINT_GRADIENT_T");
   if (!gradient) {
      return false;
   }

   gradient->linear[0] = 0.0f;
   gradient->linear[1] = 0.0f;
   gradient->linear[2] = 1.0f;
   gradient->linear[3] = 0.0f;
   gradient->radial[0] = 0.0f;
   gradient->radial[1] = 0.0f;
   gradient->radial[2] = 0.0f;
   gradient->radial[3] = 0.0f;
   gradient->radial[4] = 1.0f;
   gradient->ramp_spread_mode = VG_COLOR_RAMP_SPREAD_PAD;
   gradient->ramp_pre = true;
   gradient->ramp_stops = NULL;
   gradient->ramp_stops_count = 0;

   paint->gradient = gradient;

   return true;
}

static void paint_free(VG_CLIENT_PAINT_T *paint)
{
#if EGL_BRCM_global_image && EGL_KHR_image
   if (paint->pattern_global_image_id[0] || paint->pattern_global_image_id[1]) {
      platform_release_global_image(paint->pattern_global_image_id[0], paint->pattern_global_image_id[1]);
   }
#endif
   if (paint->gradient) {
      if (paint->gradient->ramp_stops) {
         khrn_platform_free(paint->gradient->ramp_stops);
      }
      khrn_platform_free(paint->gradient);
   }
   khrn_platform_free(paint);
}

static bool need_path_segments(VGbitfield caps)
{
   /*
      need segments for vgModifyPath and interpolating from, but also need for
      appending from incase the path being appended to needs segments (we don't
      want to have to go to the server to get them)
   */

   return !!(caps & (VG_PATH_CAPABILITY_APPEND_FROM |
                     VG_PATH_CAPABILITY_MODIFY |
                     VG_PATH_CAPABILITY_TRANSFORM_FROM |
                     VG_PATH_CAPABILITY_INTERPOLATE_FROM));
}

static VG_CLIENT_PATH_T *path_alloc(VGint format, VGPathDatatype datatype, VGfloat scale, VGfloat bias, VGbitfield caps, VGint segments_capacity)
{
   VG_CLIENT_PATH_T *path = (VG_CLIENT_PATH_T *)khrn_platform_malloc(sizeof(VG_CLIENT_PATH_T), "VG_CLIENT_PATH_T");
   if (!path) {
      return NULL;
   }

   path->object_type = VG_CLIENT_OBJECT_TYPE_PATH;
   path->format = format;
   path->datatype = datatype;
   path->scale = scale;
   path->bias = bias;
   path->caps = caps;
   if (need_path_segments(caps)) {
      khrn_vector_init(&path->segments, clampi(segments_capacity, 0, 1024));
   }

   return path;
}

static void path_update_caps(VG_CLIENT_PATH_T *path, VGbitfield caps)
{
   if (!need_path_segments(path->caps) && need_path_segments(caps)) {
      khrn_vector_init(&path->segments, 0);
   }
   if (need_path_segments(path->caps) && !need_path_segments(caps)) {
      khrn_vector_term(&path->segments);
   }
   path->caps = caps;
}

static void path_free(VG_CLIENT_PATH_T *path)
{
   if (need_path_segments(path->caps)) {
      khrn_vector_term(&path->segments);
   }
   khrn_platform_free(path);
}

static void object_free(void *object)
{
   switch (*(VG_CLIENT_OBJECT_TYPE_T *)object) {
   case VG_CLIENT_OBJECT_TYPE_FONT:       font_free((VG_CLIENT_FONT_T *)object); break;
   case VG_CLIENT_OBJECT_TYPE_IMAGE:      image_free((VG_CLIENT_IMAGE_T *)object); break;
   case VG_CLIENT_OBJECT_TYPE_MASK_LAYER: mask_layer_free((VG_CLIENT_MASK_LAYER_T *)object); break;
   case VG_CLIENT_OBJECT_TYPE_PAINT:      paint_free((VG_CLIENT_PAINT_T *)object); break;
   case VG_CLIENT_OBJECT_TYPE_PATH:       path_free((VG_CLIENT_PATH_T *)object); break;
   default:                               UNREACHABLE();
   }
}

/*
   see nice_handle in middleware/khronos/vg/vg_set.c
*/

static INLINE uint32_t nice_handle(VGHandle vg_handle)
{
   return _ror((uint32_t)vg_handle, 31);
}

static bool insert_object(VG_CLIENT_STATE_T *state, VGHandle vg_handle, void *object)
{
   void *prev_object = khrn_pointer_map_lookup(&state->shared_state->objects, nice_handle(vg_handle));
   if (prev_object) {
      object_free(prev_object);
   }
   return khrn_pointer_map_insert(&state->shared_state->objects, nice_handle(vg_handle), object);
}

static void delete_object(VG_CLIENT_STATE_T *state, VGHandle vg_handle, VG_CLIENT_OBJECT_TYPE_T type)
{
   void *object = khrn_pointer_map_lookup(&state->shared_state->objects, nice_handle(vg_handle));
   if (object && (*(VG_CLIENT_OBJECT_TYPE_T *)object == type)) {
      object_free(object);
      khrn_pointer_map_delete(&state->shared_state->objects, nice_handle(vg_handle));
   }
}

static INLINE void *lookup_object(VG_CLIENT_STATE_T *state, VGHandle vg_handle, VG_CLIENT_OBJECT_TYPE_T type)
{
   void *object = khrn_pointer_map_lookup(&state->shared_state->objects, nice_handle(vg_handle));
   return (object && (*(VG_CLIENT_OBJECT_TYPE_T *)object == type)) ? object : NULL;
}

static INLINE VG_MAT3X3_SYNC_T *get_matrix_sync(VG_CLIENT_STATE_T *state, VGMatrixMode matrix_mode)
{
   vcos_assert(
      (matrix_mode >= VG_MATRIX_PATH_USER_TO_SURFACE) &&
      (matrix_mode < (VG_MATRIX_PATH_USER_TO_SURFACE + ARR_COUNT(state->matrices))));
   return state->matrices + (matrix_mode - VG_MATRIX_PATH_USER_TO_SURFACE);
}

static INLINE bool is_matrix_affine(VGMatrixMode matrix_mode)
{
   return matrix_mode != VG_MATRIX_IMAGE_USER_TO_SURFACE;
}

static void sync_matrix(VG_CLIENT_STATE_T *state, VGMatrixMode matrix_mode)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_MAT3X3_SYNC_T *matrix_sync = get_matrix_sync(state, matrix_mode);
   if (!vg_mat3x3_identical(&matrix_sync->server, &matrix_sync->client)) {
      RPC_CALL2_IN_CTRL(vgLoadMatrix_impl,
                        thread,
                        VGLOADMATRIX_ID,
                        RPC_ENUM(matrix_mode),
                        &matrix_sync->client,
                        sizeof(VG_MAT3X3_T));
      matrix_sync->server = matrix_sync->client;
   }
}

/******************************************************************************
api misc
******************************************************************************/

VG_API_CALL VGErrorCode VG_API_ENTRY vgGetError(void) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   if (!VG_GET_CLIENT_STATE(thread)) {
      return VG_NO_CONTEXT_ERROR;
   }

   return get_error();
}

VG_API_CALL void VG_API_ENTRY vgFlush(void) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   RPC_CALL0(vgFlush_impl,
             thread,
             VGFLUSH_ID);
   RPC_FLUSH(thread);

#ifdef KHRN_COMMAND_MODE_DISPLAY
   {
      //Check for single buffered windows surface in which case call surface updater
      EGL_SURFACE_T *surface = CLIENT_GET_THREAD_STATE()->openvg.draw;
      if (surface->type == WINDOW && surface->buffers==1) {
         platform_surface_update(surface->internal_handle);
      }
   }
#endif

   if (state->flush_callback)
      state->flush_callback(false);
}

VG_API_CALL void VG_API_ENTRY vgFinish(void) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   (void)RPC_UINT_RES(RPC_CALL0_RES(vgFinish_impl,
                                    thread,
                                    VGFINISH_ID)); /* return value ignored -- read performed to ensure blocking */

#ifdef KHRN_COMMAND_MODE_DISPLAY
   {
   //Check for single buffered windows surface in which case call surface updater
      EGL_SURFACE_T *surface = CLIENT_GET_THREAD_STATE()->openvg.draw;
      if (surface->type == WINDOW && surface->buffers==1) {
         platform_surface_update(surface->internal_handle);
      }
   }
#endif

   if (state->flush_callback)
      state->flush_callback(true);
}

/******************************************************************************
api get/set
******************************************************************************/

static void set_iv_server(
   VGParamType param_type,
   VGint count,
   const VGint *values)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   RPC_CALL3_IN_CTRL(vgSetiv_impl,
                     thread,
                     VGSETIV_ID,
                     RPC_ENUM(param_type),
                     RPC_INT(count),
                     values,
                     count * sizeof(VGint));
}

static void set_fv_server(
   VGParamType param_type,
   VGint count,
   const VGfloat *values)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   RPC_CALL3_IN_CTRL(vgSetfv_impl,
                     thread,
                     VGSETFV_ID,
                     RPC_ENUM(param_type),
                     RPC_INT(count),
                     values,
                     count * sizeof(VGfloat));
}

static void get_fv_server(
   VGParamType param_type,
   VGint count,
   VGfloat *values)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   RPC_CALL3_OUT_CTRL(vgGetfv_impl,
                      thread,
                      VGGETFV_ID,
                      RPC_ENUM(param_type),
                      RPC_INT(count),
                      values);
}

static void set_parameter_iv_server(
   VGHandle vg_handle,
   VG_CLIENT_OBJECT_TYPE_T object_type,
   VGint param_type,
   VGint count,
   const VGint *values)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   RPC_CALL5_IN_CTRL(vgSetParameteriv_impl,
                     thread,
                     VGSETPARAMETERIV_ID,
                     RPC_HANDLE(vg_handle),
                     RPC_ENUM(object_type),
                     RPC_INT(param_type),
                     RPC_INT(count),
                     values,
                     count * sizeof(VGint));
}

static void set_parameter_fv_server(
   VGHandle vg_handle,
   VG_CLIENT_OBJECT_TYPE_T object_type,
   VGint param_type,
   VGint count,
   const VGfloat *values)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   RPC_CALL5_IN_CTRL(vgSetParameterfv_impl,
                     thread,
                     VGSETPARAMETERFV_ID,
                     RPC_HANDLE(vg_handle),
                     RPC_ENUM(object_type),
                     RPC_INT(param_type),
                     RPC_INT(count),
                     values,
                     count * sizeof(VGfloat));
}

static bool get_parameter_iv_server(
   VGHandle vg_handle,
   VG_CLIENT_OBJECT_TYPE_T object_type,
   VGint param_type,
   VGint count,
   VGint *values)
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   return RPC_BOOLEAN_RES(RPC_CALL5_OUT_CTRL_RES(vgGetParameteriv_impl,
                                                 thread,
                                                 VGGETPARAMETERIV_ID,
                                                 RPC_HANDLE(vg_handle),
                                                 RPC_ENUM(object_type),
                                                 RPC_INT(param_type),
                                                 RPC_INT(count),
                                                 values));
}

static void set_ifv(
   CLIENT_THREAD_STATE_T *thread,
   VG_CLIENT_STATE_T *state,
   VGParamType param_type,
   VGint count,
   bool are_floats,
   const void *values)
{
   /*
    * If 'count' is greater than zero, then our caller must have verified
    * that 'values' is non-NULL.
    * If 'count' is zero, then 'values' *must not* be dereferenced.
    * Note that params_to_floats() and params_to_ints() handle a zero 'count'
    * argument correctly and do not dereference 'values' in this case.
    * Other functions, e.g. param_to_int() and param_to_float(), require a
    * check before calling.
    */
   VGint value_i = (count == 1) ? param_to_int(are_floats, values, 0) : 0;
   VGfloat value_f = (count == 1) ? param_to_float(are_floats, values, 0) : 0.0f;

   switch (param_type) {
   /*
      settable scalar param types
   */

   #define CASE(PARAM_TYPE, OK, DO) \
      case PARAM_TYPE: \
      { \
         if ((count != 1) || !(OK)) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; } \
         DO; \
         break; \
      }
   #define CASE_I_SERVER(PARAM_TYPE, OK, VALUE, FN) CASE(PARAM_TYPE, OK, if (VALUE != FN(value_i)) { VALUE = FN(value_i); set_iv_server(PARAM_TYPE, 1, &value_i); })
   #define CASE_F_SERVER(PARAM_TYPE, OK, VALUE) CASE(PARAM_TYPE, OK, if (!floats_identical(VALUE, value_f)) { VALUE = value_f; set_fv_server(PARAM_TYPE, 1, &value_f); })
   CASE(VG_MATRIX_MODE, is_matrix_mode((VGMatrixMode)value_i), state->matrix_mode = (VGMatrixMode)value_i)
   CASE_I_SERVER(VG_FILL_RULE, is_fill_rule((VGFillRule)value_i), state->fill_rule, (VGFillRule))
   CASE_I_SERVER(VG_IMAGE_QUALITY, is_image_quality((VGImageQuality)value_i), state->image_quality, (VGImageQuality))
   CASE_I_SERVER(VG_RENDERING_QUALITY, is_rendering_quality((VGRenderingQuality)value_i), state->rendering_quality, (VGRenderingQuality))
   CASE_I_SERVER(VG_BLEND_MODE, is_blend_mode((VGBlendMode)value_i), state->blend_mode, (VGBlendMode))
   CASE_I_SERVER(VG_IMAGE_MODE, is_image_mode((VGImageMode)value_i), state->image_mode, (VGImageMode))
   CASE_I_SERVER(VG_COLOR_TRANSFORM, true, state->color_transform, clean_boolean)
   CASE_F_SERVER(VG_STROKE_LINE_WIDTH, true, state->stroke_line_width)
   CASE_I_SERVER(VG_STROKE_CAP_STYLE, is_cap_style((VGCapStyle)value_i), state->stroke_cap_style, (VGCapStyle))
   CASE_I_SERVER(VG_STROKE_JOIN_STYLE, is_join_style((VGJoinStyle)value_i), state->stroke_join_style, (VGJoinStyle))
   CASE_F_SERVER(VG_STROKE_MITER_LIMIT, true, state->stroke_miter_limit)
   CASE_F_SERVER(VG_STROKE_DASH_PHASE, true, state->stroke_dash_phase)
   CASE_I_SERVER(VG_STROKE_DASH_PHASE_RESET, true, state->stroke_dash_phase_reset, clean_boolean)
   CASE_I_SERVER(VG_MASKING, true, state->masking, clean_boolean)
   CASE_I_SERVER(VG_SCISSORING, true, state->scissoring, clean_boolean)
   CASE(VG_PIXEL_LAYOUT, is_pixel_layout((VGPixelLayout)value_i), state->pixel_layout = (VGPixelLayout)value_i)
   CASE_I_SERVER(VG_FILTER_FORMAT_LINEAR, true, state->filter_format_linear, clean_boolean)
   CASE_I_SERVER(VG_FILTER_FORMAT_PREMULTIPLIED, true, state->filter_format_pre, clean_boolean)
   CASE_I_SERVER(VG_FILTER_CHANNEL_MASK, true, state->filter_channel_mask, )
   #undef CASE_F_SERVER
   #undef CASE_I_SERVER
   #undef CASE

   /*
      read-only scalar param types
   */

   case VG_SCREEN_LAYOUT:
   case VG_MAX_SCISSOR_RECTS:
   case VG_MAX_DASH_COUNT:
   case VG_MAX_KERNEL_SIZE:
   case VG_MAX_SEPARABLE_KERNEL_SIZE:
   case VG_MAX_COLOR_RAMP_STOPS:
   case VG_MAX_IMAGE_WIDTH:
   case VG_MAX_IMAGE_HEIGHT:
   case VG_MAX_IMAGE_PIXELS:
   case VG_MAX_IMAGE_BYTES:
   case VG_MAX_FLOAT:
   case VG_MAX_GAUSSIAN_STD_DEVIATION:
   {
      if (count != 1) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); }
      break;
   }

   /*
      vector param types
   */

   case VG_SCISSOR_RECTS:
   {
      if (count & 0x3) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; }
      count = _min(count, VG_CONFIG_MAX_SCISSOR_RECTS * 4);
      if (params_to_ints(
         state->scissor_rects,
         are_floats, values, count) ||
         (state->scissor_rects_count != count)) {
         state->scissor_rects_count = count;
         set_iv_server(VG_SCISSOR_RECTS, count, state->scissor_rects);
      }
      break;
   }
   case VG_COLOR_TRANSFORM_VALUES:
   {
      if (count != 8) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; }
      if (params_to_floats(
         state->color_transform_values,
         are_floats, values, count)) {
         set_fv_server(VG_COLOR_TRANSFORM_VALUES, 8, state->color_transform_values);
      }
      break;
   }
   case VG_STROKE_DASH_PATTERN:
   {
      count = _min(count, VG_CONFIG_MAX_DASH_COUNT);
      if (params_to_floats(
         state->stroke_dash_pattern,
         are_floats, values, count) ||
         (state->stroke_dash_pattern_count != count)) {
         state->stroke_dash_pattern_count = count;
         set_fv_server(VG_STROKE_DASH_PATTERN, count, state->stroke_dash_pattern);
      }
      break;
   }
   case VG_TILE_FILL_COLOR:
   case VG_CLEAR_COLOR:
   {
      VGfloat *color;
      if (count != 4) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; }
      color = (param_type == VG_TILE_FILL_COLOR) ? state->tile_fill_color : state->clear_color;
      if (params_to_floats(
         color,
         are_floats, values, count)) {
         VGint rgba = color_floats_to_rgba_clean(color);
         set_iv_server(param_type, 1, &rgba);
      }
      break;
   }
   case VG_GLYPH_ORIGIN:
   {
      /*
         don't store on client as it can change
      */

      VGfloat glyph_origin[2];
      if (count != 2) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; }
      (void)params_to_floats(
         glyph_origin,
         are_floats, values, count); /* ignore return -- it doesn't mean anything in this case */
      set_fv_server(VG_GLYPH_ORIGIN, 2, glyph_origin);
      break;
   }

   /*
      invalid param type
   */

   default:
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
   }
}

static void get_ifv(
   CLIENT_THREAD_STATE_T *thread,
   VG_CLIENT_STATE_T *state,
   VGParamType param_type,
   VGint count,
   bool are_floats,
   void *values)
{
   switch (param_type) {
   /*
      scalar param types
   */

   #define CASE(PARAM_TYPE, FN, VALUE) \
      case PARAM_TYPE: \
      { \
         if (count > 1) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; } \
         FN(are_floats, values, 0, VALUE); \
         break; \
      }
   #define CASE_I(PARAM_TYPE, VALUE) CASE(PARAM_TYPE, int_to_param, VALUE)
   #define CASE_F(PARAM_TYPE, VALUE) CASE(PARAM_TYPE, float_to_param, VALUE)
   CASE_I(VG_MATRIX_MODE, state->matrix_mode)
   CASE_I(VG_FILL_RULE, state->fill_rule)
   CASE_I(VG_IMAGE_QUALITY, state->image_quality)
   CASE_I(VG_RENDERING_QUALITY, state->rendering_quality)
   CASE_I(VG_BLEND_MODE, state->blend_mode)
   CASE_I(VG_IMAGE_MODE, state->image_mode)
   CASE_I(VG_COLOR_TRANSFORM, state->color_transform)
   CASE_F(VG_STROKE_LINE_WIDTH, state->stroke_line_width)
   CASE_I(VG_STROKE_CAP_STYLE, state->stroke_cap_style)
   CASE_I(VG_STROKE_JOIN_STYLE, state->stroke_join_style)
   CASE_F(VG_STROKE_MITER_LIMIT, state->stroke_miter_limit)
   CASE_F(VG_STROKE_DASH_PHASE, state->stroke_dash_phase)
   CASE_I(VG_STROKE_DASH_PHASE_RESET, state->stroke_dash_phase_reset)
   CASE_I(VG_MASKING, state->masking)
   CASE_I(VG_SCISSORING, state->scissoring)
   CASE_I(VG_PIXEL_LAYOUT, state->pixel_layout)
   CASE_I(VG_SCREEN_LAYOUT, VG_CONFIG_SCREEN_LAYOUT)
   CASE_I(VG_FILTER_FORMAT_LINEAR, state->filter_format_linear)
   CASE_I(VG_FILTER_FORMAT_PREMULTIPLIED, state->filter_format_pre)
   CASE_I(VG_FILTER_CHANNEL_MASK, state->filter_channel_mask)
   CASE_I(VG_MAX_SCISSOR_RECTS, VG_CONFIG_MAX_SCISSOR_RECTS)
   CASE_I(VG_MAX_DASH_COUNT, VG_CONFIG_MAX_DASH_COUNT)
   CASE_I(VG_MAX_KERNEL_SIZE, VG_CONFIG_MAX_KERNEL_SIZE)
   CASE_I(VG_MAX_SEPARABLE_KERNEL_SIZE, VG_CONFIG_MAX_SEPARABLE_KERNEL_SIZE)
   CASE_I(VG_MAX_COLOR_RAMP_STOPS, VG_CONFIG_MAX_COLOR_RAMP_STOPS)
   CASE_I(VG_MAX_IMAGE_WIDTH, VG_CONFIG_MAX_IMAGE_WIDTH)
   CASE_I(VG_MAX_IMAGE_HEIGHT, VG_CONFIG_MAX_IMAGE_HEIGHT)
   CASE_I(VG_MAX_IMAGE_PIXELS, VG_CONFIG_MAX_IMAGE_PIXELS)
   CASE_I(VG_MAX_IMAGE_BYTES, VG_CONFIG_MAX_IMAGE_BYTES)
   CASE_F(VG_MAX_FLOAT, VG_CONFIG_MAX_FLOAT)
   CASE_F(VG_MAX_GAUSSIAN_STD_DEVIATION, VG_CONFIG_MAX_GAUSSIAN_STD_DEVIATION)
   #undef CASE_F
   #undef CASE_I
   #undef CASE

   /*
      vector param types
   */

   case VG_SCISSOR_RECTS:
   {
      if ((VGuint)count > state->scissor_rects_count) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; }
      ints_to_params(
         are_floats, values, count,
         state->scissor_rects);
      break;
   }
   case VG_COLOR_TRANSFORM_VALUES:
   {
      if (count > 8) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; }
      floats_to_params(
         are_floats, values, count,
         state->color_transform_values);
      break;
   }
   case VG_STROKE_DASH_PATTERN:
   {
      if ((VGuint)count > state->stroke_dash_pattern_count) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; }
      floats_to_params(
         are_floats, values, count,
         state->stroke_dash_pattern);
      break;
   }
   case VG_TILE_FILL_COLOR:
   case VG_CLEAR_COLOR:
   {
      if (count > 4) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; }
      floats_to_params(
         are_floats, values, count,
         (param_type == VG_TILE_FILL_COLOR) ? state->tile_fill_color : state->clear_color);
      break;
   }
   case VG_GLYPH_ORIGIN:
   {
      VGfloat glyph_origin[2];
      if (count > 2) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; }
      get_fv_server(VG_GLYPH_ORIGIN, 2, glyph_origin);
      floats_to_params(
         are_floats, values, count,
         glyph_origin);
      break;
   }

   /*
      invalid param type
   */

   default:
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
   }
}

static void set_parameter_ifv(
   CLIENT_THREAD_STATE_T *thread,
   VG_CLIENT_STATE_T *state,
   VGHandle vg_handle,
   VGint param_type,
   VGint count,
   bool are_floats,
   const void *values)
{
   /*
    * If 'count' is greater than zero, then our caller must have verified
    * that 'values' is non-NULL.
    * If 'count' is zero, then 'values' *must not* be dereferenced.
    * Note that params_to_floats() and params_to_ints() handle a zero 'count'
    * argument correctly and do not dereference 'values' in this case.
    * Other functions, e.g. param_to_int() and param_to_float(), require a
    * check before calling.
    */
   void *object;

   platform_mutex_acquire(&state->shared_state->mutex);

   object = khrn_pointer_map_lookup(&state->shared_state->objects, nice_handle(vg_handle));
   if (!object) {
      set_error(VG_BAD_HANDLE_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      return;
   }

   switch (*(VG_CLIENT_OBJECT_TYPE_T *)object) {
   case VG_CLIENT_OBJECT_TYPE_PATH:
   {
      switch (param_type) {
      /*
         read-only scalar param types
      */

      case VG_PATH_FORMAT:
      case VG_PATH_DATATYPE:
      case VG_PATH_SCALE:
      case VG_PATH_BIAS:
      case VG_PATH_NUM_SEGMENTS:
      case VG_PATH_NUM_COORDS:
      {
         if (count != 1) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); }
         break;
      }

      /*
         invalid param type
      */

      default:
         set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      }

      break;
   }
   case VG_CLIENT_OBJECT_TYPE_PAINT:
   {
      VG_CLIENT_PAINT_T *paint = (VG_CLIENT_PAINT_T *)object;
      VGint value_i;

      if (!paint->gradient &&
         need_paint_gradient(param_type) &&
         !paint_alloc_gradient(paint)) {
         set_error(VG_OUT_OF_MEMORY_ERROR);
         break;
      }

      value_i = (count == 1) ? param_to_int(are_floats, values, 0) : 0;

      switch (param_type) {
      /*
         settable scalar param types
      */

      #define CASE_I_SERVER(PARAM_TYPE, OK, VALUE, FN) \
         case PARAM_TYPE: \
         { \
            if ((count != 1) || !(OK)) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; } \
            if (VALUE != FN(value_i)) { \
               VALUE = FN(value_i); \
               set_parameter_iv_server(vg_handle, VG_CLIENT_OBJECT_TYPE_PAINT, PARAM_TYPE, 1, &value_i); \
            } \
            break; \
         }
      CASE_I_SERVER(VG_PAINT_TYPE, is_paint_type((VGPaintType)value_i), paint->type, (VGPaintType))
      CASE_I_SERVER(VG_PAINT_COLOR_RAMP_SPREAD_MODE, is_color_ramp_spread_mode((VGColorRampSpreadMode)value_i), paint->gradient->ramp_spread_mode, (VGColorRampSpreadMode))
      CASE_I_SERVER(VG_PAINT_COLOR_RAMP_PREMULTIPLIED, true, paint->gradient->ramp_pre, clean_boolean)
      CASE_I_SERVER(VG_PAINT_PATTERN_TILING_MODE, is_tiling_mode((VGTilingMode)value_i), paint->pattern_tiling_mode, (VGTilingMode))
      #undef CASE_I_SERVER

      /*
         vector param types
      */

      case VG_PAINT_COLOR:
      {
         if (count != 4) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; }
         if (params_to_floats(
            paint->color,
            are_floats, values, count)) {
            VGint rgba = color_floats_to_rgba_clean(paint->color);
            set_parameter_iv_server(vg_handle, VG_CLIENT_OBJECT_TYPE_PAINT, VG_PAINT_COLOR, 1, &rgba);
         }
         break;
      }
      case VG_PAINT_COLOR_RAMP_STOPS:
      {
         if ((count % 5) != 0) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; }
         count = _min(count, VG_CONFIG_MAX_COLOR_RAMP_STOPS * 5);
         if (paint->gradient->ramp_stops_count != count) {
            VGfloat *ramp_stops = NULL;
            if (count != 0) {
               ramp_stops = (VGfloat *)khrn_platform_malloc(count * sizeof(VGfloat), "VG_CLIENT_PAINT_GRADIENT_T.ramp_stops");
               if (!ramp_stops) {
                  set_error(VG_OUT_OF_MEMORY_ERROR);
                  break;
               }
            }
            if (paint->gradient->ramp_stops) {
               khrn_platform_free(paint->gradient->ramp_stops);
            }
            paint->gradient->ramp_stops = ramp_stops;
         }
         if (params_to_floats(
            paint->gradient->ramp_stops,
            are_floats, values, count) ||
            (paint->gradient->ramp_stops_count != count)) {
            paint->gradient->ramp_stops_count = count;
            set_parameter_fv_server(vg_handle, VG_CLIENT_OBJECT_TYPE_PAINT, VG_PAINT_COLOR_RAMP_STOPS, count, paint->gradient->ramp_stops);
         }
         break;
      }
      case VG_PAINT_LINEAR_GRADIENT:
      {
         if (count != 4) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; }
         if (params_to_floats(
            paint->gradient->linear,
            are_floats, values, count)) {
            set_parameter_fv_server(vg_handle, VG_CLIENT_OBJECT_TYPE_PAINT, VG_PAINT_LINEAR_GRADIENT, 4, paint->gradient->linear);
         }
         break;
      }
      case VG_PAINT_RADIAL_GRADIENT:
      {
         if (count != 5) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; }
         if (params_to_floats(
            paint->gradient->radial,
            are_floats, values, count)) {
            set_parameter_fv_server(vg_handle, VG_CLIENT_OBJECT_TYPE_PAINT, VG_PAINT_RADIAL_GRADIENT, 5, paint->gradient->radial);
         }
         break;
      }

      /*
         invalid param type
      */

      default:
         set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      }

      break;
   }
   case VG_CLIENT_OBJECT_TYPE_IMAGE:
   {
      switch (param_type) {
      /*
         read-only scalar param types
      */

      case VG_IMAGE_FORMAT:
      case VG_IMAGE_WIDTH:
      case VG_IMAGE_HEIGHT:
      {
         if (count != 1) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); }
         break;
      }

      /*
         invalid param type
      */

      default:
         set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      }

      break;
   }
   case VG_CLIENT_OBJECT_TYPE_MASK_LAYER:
   {
      switch (param_type) {
      /*
         read-only scalar param types
      */

      case VG_IMAGE_WIDTH:
      case VG_IMAGE_HEIGHT:
      {
         if (count != 1) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); }
         break;
      }

      /*
         invalid param type
      */

      default:
         set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      }

      break;
   }
   case VG_CLIENT_OBJECT_TYPE_FONT:
   {
      switch (param_type) {
      /*
         read-only scalar param types
      */

      case VG_FONT_NUM_GLYPHS:
      {
         if (count != 1) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); }
         break;
      }

      /*
         invalid param type
      */

      default:
         set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      }

      break;
   }
   default:
   {
      UNREACHABLE();
   }
   }

   platform_mutex_release(&state->shared_state->mutex);
}

static void get_parameter_ifv(
   CLIENT_THREAD_STATE_T *thread,
   VG_CLIENT_STATE_T *state,
   VGHandle vg_handle,
   VGint param_type,
   VGint count,
   bool are_floats,
   void *values)
{
   void *object;

   platform_mutex_acquire(&state->shared_state->mutex);

   object = khrn_pointer_map_lookup(&state->shared_state->objects, nice_handle(vg_handle));
   if (!object) {
      set_error(VG_BAD_HANDLE_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      return;
   }

   #define CASE(PARAM_TYPE, FN, VALUE) \
      case PARAM_TYPE: \
      { \
         if (count > 1) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; } \
         FN(are_floats, values, 0, VALUE); \
         break; \
      }
   #define CASE_I(PARAM_TYPE, VALUE) CASE(PARAM_TYPE, int_to_param, VALUE)
   #define CASE_F(PARAM_TYPE, VALUE) CASE(PARAM_TYPE, float_to_param, VALUE)

   switch (*(VG_CLIENT_OBJECT_TYPE_T *)object) {
   case VG_CLIENT_OBJECT_TYPE_PATH:
   {
      VG_CLIENT_PATH_T *path = (VG_CLIENT_PATH_T *)object;

      switch (param_type) {
      /*
         scalar param types
      */

      CASE_I(VG_PATH_FORMAT, path->format)
      CASE_I(VG_PATH_DATATYPE, path->datatype)
      CASE_F(VG_PATH_SCALE, path->scale)
      CASE_F(VG_PATH_BIAS, path->bias)
      case VG_PATH_NUM_SEGMENTS:
      case VG_PATH_NUM_COORDS:
      {
         if (count > 1) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; }
         if (need_path_segments(path->caps)) {
            int_to_param(are_floats, values, 0, (param_type == VG_PATH_NUM_SEGMENTS) ?
               path->segments.size :
               get_coords_count((const VGubyte *)path->segments.data, path->segments.size));
         } else {
            VGint value;
            if (get_parameter_iv_server(vg_handle, VG_CLIENT_OBJECT_TYPE_PATH, param_type, 1, &value)) {
               int_to_param(are_floats, values, 0, value);
            }
         }
         break;
      }

      /*
         invalid param type
      */

      default:
         set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      }

      break;
   }
   case VG_CLIENT_OBJECT_TYPE_PAINT:
   {
      VG_CLIENT_PAINT_T *paint = (VG_CLIENT_PAINT_T *)object;

      if (!paint->gradient &&
         need_paint_gradient(param_type) &&
         !paint_alloc_gradient(paint)) {
         set_error(VG_OUT_OF_MEMORY_ERROR);
         break;
      }

      switch (param_type) {
      /*
         scalar param types
      */

      CASE_I(VG_PAINT_TYPE, paint->type)
      CASE_I(VG_PAINT_COLOR_RAMP_SPREAD_MODE, paint->gradient->ramp_spread_mode)
      CASE_I(VG_PAINT_COLOR_RAMP_PREMULTIPLIED, paint->gradient->ramp_pre)
      CASE_I(VG_PAINT_PATTERN_TILING_MODE, paint->pattern_tiling_mode)

      /*
         vector param types
      */

      case VG_PAINT_COLOR:
      {
         if (count > 4) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; }
         floats_to_params(
            are_floats, values, count,
            paint->color);
         break;
      }
      case VG_PAINT_COLOR_RAMP_STOPS:
      {
         if ((VGuint)count > paint->gradient->ramp_stops_count) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; }
         floats_to_params(
            are_floats, values, count,
            paint->gradient->ramp_stops);
         break;
      }
      case VG_PAINT_LINEAR_GRADIENT:
      {
         if (count > 4) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; }
         floats_to_params(
            are_floats, values, count,
            paint->gradient->linear);
         break;
      }
      case VG_PAINT_RADIAL_GRADIENT:
      {
         if (count > 5) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; }
         floats_to_params(
            are_floats, values, count,
            paint->gradient->radial);
         break;
      }

      /*
         invalid param type
      */

      default:
         set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      }

      break;
   }
   case VG_CLIENT_OBJECT_TYPE_IMAGE:
   {
      VG_CLIENT_IMAGE_T *image = (VG_CLIENT_IMAGE_T *)object;

      switch (param_type) {
      /*
         scalar param types
      */

      CASE_I(VG_IMAGE_FORMAT, image->format)
      CASE_I(VG_IMAGE_WIDTH, image->width)
      CASE_I(VG_IMAGE_HEIGHT, image->height)

      /*
         invalid param type
      */

      default:
         set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      }

      break;
   }
   case VG_CLIENT_OBJECT_TYPE_MASK_LAYER:
   {
      VG_CLIENT_MASK_LAYER_T *mask_layer = (VG_CLIENT_MASK_LAYER_T *)object;

      switch (param_type) {
      /*
         scalar param types
      */

      CASE_I(VG_IMAGE_WIDTH, mask_layer->width)
      CASE_I(VG_IMAGE_HEIGHT, mask_layer->height)

      /*
         invalid param type
      */

      default:
         set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      }

      break;
   }
   case VG_CLIENT_OBJECT_TYPE_FONT:
   {
      switch (param_type) {
      /*
         scalar param types
      */

      case VG_FONT_NUM_GLYPHS:
      {
         VGint glyphs_count;
         if (count > 1) { set_error(VG_ILLEGAL_ARGUMENT_ERROR); break; }
         if (get_parameter_iv_server(vg_handle, VG_CLIENT_OBJECT_TYPE_FONT, VG_FONT_NUM_GLYPHS, 1, &glyphs_count)) {
            int_to_param(are_floats, values, 0, glyphs_count);
         }
         break;
      }

      /*
         invalid param type
      */

      default:
         set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      }

      break;
   }
   default:
   {
      UNREACHABLE();
   }
   }

   #undef CASE_F
   #undef CASE_I
   #undef CASE

   platform_mutex_release(&state->shared_state->mutex);
}

VG_API_CALL void VG_API_ENTRY vgSetf(
   VGParamType param_type,
   VGfloat value) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if (is_vector_param_type(param_type)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   set_ifv(thread, state, param_type, 1, true, &value);
}

VG_API_CALL void VG_API_ENTRY vgSeti(
   VGParamType param_type,
   VGint value) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if (is_vector_param_type(param_type)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   set_ifv(thread, state, param_type, 1, false, &value);
}

VG_API_CALL void VG_API_ENTRY vgSetfv(
   VGParamType param_type,
   VGint count,
   const VGfloat *values) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   /*
    * Verify the arguments, and report an error if any of the following holds:
    *   - 'count' is negative
    *   - 'count' is greater than zero and 'values' is NULL
    *   - 'values' is non-NULL but not aligned on an float boundary
    *
    * If 'count' is zero, then 'values' will not (and must not) be dereferenced
    * in this function or any of it's descendents.  The OpenVG Specification
    * Version 1.1, section 5.2 Setting and Querying Context Parameter Values
    * states:
    *
    *   "If the count parameter is 0, the pointer argument is not dereferenced.
    *    For example, the call vgSet(VG_STROKE_DASH_PATTERN, 0, (void *) 0)
    *    sets the dash pattern to a zero-length array (which has the effect of
    *    disabling dashing) without dereferencing the third parameter."
    *
    * Automatic checking tools such as Coverity may flag this case ('values'
    * not verified non-NULL when 'count' is zero) as a potential NULL pointer
    * dereference. In this case it is not.
    */
   if ((count < 0) || ((count > 0) && !values) || (values && !is_aligned_float(values))) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   set_ifv(thread, state, param_type, count, true, values);
}

VG_API_CALL void VG_API_ENTRY vgSetiv(
   VGParamType param_type,
   VGint count,
   const VGint *values) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   /*
    * Verify the arguments, and report an error if any of the following holds:
    *   - 'count' is negative
    *   - 'count' is greater than zero and 'values' is NULL
    *   - 'values' is non-NULL but not aligned on an integer boundary
    *
    * If 'count' is zero, then 'values' will not (and must not) be dereferenced
    * in this function or any of it's descendents.  See the comment in vgSetfv()
    * above for more detail.
    *
    * Automatic checking tools such as Coverity may flag this case ('values'
    * not verified non-NULL when 'count' is zero) as a potential NULL pointer
    * dereference. In this case it is not.
    */
   if ((count < 0) || ((count > 0) && !values) || (values && !is_aligned_int(values))) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   set_ifv(thread, state, param_type, count, false, values);
}

VG_API_CALL VGfloat VG_API_ENTRY vgGetf(
   VGParamType param_type) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VGfloat value = 0.0f;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return 0.0f;
   }

   if (is_vector_param_type(param_type)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return 0.0f;
   }

   get_ifv(thread, state, param_type, 1, true, &value);
   return value;
}

VG_API_CALL VGint VG_API_ENTRY vgGeti(
   VGParamType param_type) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VGint value = 0;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return 0;
   }

   if (is_vector_param_type(param_type)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return 0;
   }

   get_ifv(thread, state, param_type, 1, false, &value);
   return value;
}

VG_API_CALL VGint VG_API_ENTRY vgGetVectorSize(
   VGParamType param_type) VG_API_EXIT
{

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(CLIENT_GET_THREAD_STATE());
   if (!state) {
      return 0;
   }

   switch (param_type) {
   /*
      scalar param types
   */

   case VG_MATRIX_MODE:
   case VG_FILL_RULE:
   case VG_IMAGE_QUALITY:
   case VG_RENDERING_QUALITY:
   case VG_BLEND_MODE:
   case VG_IMAGE_MODE:
   case VG_COLOR_TRANSFORM:
   case VG_STROKE_LINE_WIDTH:
   case VG_STROKE_CAP_STYLE:
   case VG_STROKE_JOIN_STYLE:
   case VG_STROKE_MITER_LIMIT:
   case VG_STROKE_DASH_PHASE:
   case VG_STROKE_DASH_PHASE_RESET:
   case VG_MASKING:
   case VG_SCISSORING:
   case VG_PIXEL_LAYOUT:
   case VG_SCREEN_LAYOUT:
   case VG_FILTER_FORMAT_LINEAR:
   case VG_FILTER_FORMAT_PREMULTIPLIED:
   case VG_FILTER_CHANNEL_MASK:
   case VG_MAX_SCISSOR_RECTS:
   case VG_MAX_DASH_COUNT:
   case VG_MAX_KERNEL_SIZE:
   case VG_MAX_SEPARABLE_KERNEL_SIZE:
   case VG_MAX_COLOR_RAMP_STOPS:
   case VG_MAX_IMAGE_WIDTH:
   case VG_MAX_IMAGE_HEIGHT:
   case VG_MAX_IMAGE_PIXELS:
   case VG_MAX_IMAGE_BYTES:
   case VG_MAX_FLOAT:
   case VG_MAX_GAUSSIAN_STD_DEVIATION: return 1;

   /*
      vector param types
   */

   case VG_SCISSOR_RECTS:          return state->scissor_rects_count;
   case VG_COLOR_TRANSFORM_VALUES: return 8;
   case VG_STROKE_DASH_PATTERN:    return state->stroke_dash_pattern_count;
   case VG_TILE_FILL_COLOR:
   case VG_CLEAR_COLOR:            return 4;
   case VG_GLYPH_ORIGIN:           return 2;

   /*
      invalid param type
   */

   default: set_error(VG_ILLEGAL_ARGUMENT_ERROR); return 0;
   }
}

VG_API_CALL void VG_API_ENTRY vgGetfv(
   VGParamType param_type,
   VGint count,
   VGfloat *values) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if ((count <= 0) || !values || !is_aligned_float(values)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   get_ifv(thread, state, param_type, count, true, values);
}

VG_API_CALL void VG_API_ENTRY vgGetiv(
   VGParamType param_type,
   VGint count,
   VGint *values) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if ((count <= 0) || !values || !is_aligned_int(values)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   get_ifv(thread, state, param_type, count, false, values);
}

VG_API_CALL void VG_API_ENTRY vgSetParameterf(
   VGHandle vg_handle,
   VGint param_type,
   VGfloat value) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if (is_vector_object_param_type(param_type)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   set_parameter_ifv(thread, state, vg_handle, param_type, 1, true, &value);
}

VG_API_CALL void VG_API_ENTRY vgSetParameteri(
   VGHandle vg_handle,
   VGint param_type,
   VGint value) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if (is_vector_object_param_type(param_type)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   set_parameter_ifv(thread, state, vg_handle, param_type, 1, false, &value);
}

VG_API_CALL void VG_API_ENTRY vgSetParameterfv(
   VGHandle vg_handle,
   VGint param_type,
   VGint count,
   const VGfloat *values) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   /*
    * Verify the arguments, and report an error if any of the following holds:
    *   - 'count' is negative
    *   - 'count' is greater than zero and 'values' is NULL
    *   - 'values' is non-NULL but not aligned on an float boundary
    *
    * If 'count' is zero, then 'values' will not (and must not) be dereferenced
    * in this function or any of it's descendents.  See the comment in vgSetfv()
    * above for more detail.
    *
    * Automatic checking tools such as Coverity may flag this case ('values'
    * not verified non-NULL when 'count' is zero) as a potential NULL pointer
    * dereference. In this case it is not.
    */
   if ((count < 0) || ((count > 0) && !values) || (values && !is_aligned_float(values))) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   set_parameter_ifv(thread, state, vg_handle, param_type, count, true, values);
}

VG_API_CALL void VG_API_ENTRY vgSetParameteriv(
   VGHandle vg_handle,
   VGint param_type,
   VGint count,
   const VGint *values) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   /*
    * Verify the arguments, and report an error if any of the following holds:
    *   - 'count' is negative
    *   - 'count' is greater than zero and 'values' is NULL
    *   - 'values' is non-NULL but not aligned on an integer boundary
    *
    * If 'count' is zero, then 'values' will not (and must not) be dereferenced
    * in this function or any of it's descendents.  See the comment in vgSetfv()
    * above for more detail.
    *
    * Automatic checking tools such as Coverity may flag this case ('values'
    * not verified non-NULL when 'count' is zero) as a potential NULL pointer
    * dereference. In this case it is not.
    */
   if ((count < 0) || ((count > 0) && !values) || (values && !is_aligned_int(values))) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   set_parameter_ifv(thread, state, vg_handle, param_type, count, false, values);
}

VG_API_CALL VGfloat VG_API_ENTRY vgGetParameterf(
   VGHandle vg_handle,
   VGint param_type) VG_API_EXIT
{
   VGfloat value = 0.0f;

   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return 0.0f;
   }

   if (is_vector_object_param_type(param_type)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return 0.0f;
   }

   get_parameter_ifv(thread, state, vg_handle, param_type, 1, true, &value);
   return value;
}

VG_API_CALL VGint VG_API_ENTRY vgGetParameteri(
   VGHandle vg_handle,
   VGint param_type) VG_API_EXIT
{
   VGint value = 0;

   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return 0;
   }

   if (is_vector_object_param_type(param_type)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return 0;
   }

   get_parameter_ifv(thread, state, vg_handle, param_type, 1, false, &value);
   return value;
}

VG_API_CALL VGint VG_API_ENTRY vgGetParameterVectorSize(
   VGHandle vg_handle,
   VGint param_type) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   void *object;
   VGint count = 0;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return 0;
   }

   platform_mutex_acquire(&state->shared_state->mutex);

   object = khrn_pointer_map_lookup(&state->shared_state->objects, nice_handle(vg_handle));
   if (!object) {
      set_error(VG_BAD_HANDLE_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      return 0;
   }

   switch (*(VG_CLIENT_OBJECT_TYPE_T *)object) {
   case VG_CLIENT_OBJECT_TYPE_PATH:
   {
      switch (param_type) {
      /*
         scalar param types
      */

      case VG_PATH_FORMAT:
      case VG_PATH_DATATYPE:
      case VG_PATH_SCALE:
      case VG_PATH_BIAS:
      case VG_PATH_NUM_SEGMENTS:
      case VG_PATH_NUM_COORDS:
      {
         count = 1;
         break;
      }

      /*
         invalid param type
      */

      default:
         set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      }

      break;
   }
   case VG_CLIENT_OBJECT_TYPE_PAINT:
   {
      switch (param_type) {
      /*
         scalar param types
      */

      case VG_PAINT_TYPE:
      case VG_PAINT_COLOR_RAMP_SPREAD_MODE:
      case VG_PAINT_COLOR_RAMP_PREMULTIPLIED:
      case VG_PAINT_PATTERN_TILING_MODE:
      {
         count = 1;
         break;
      }

      /*
         vector param types
      */

      case VG_PAINT_COLOR:
      {
         count = 4;
         break;
      }
      case VG_PAINT_COLOR_RAMP_STOPS:
      {
         VG_CLIENT_PAINT_T *paint = (VG_CLIENT_PAINT_T *)object;
         count = paint->gradient ? paint->gradient->ramp_stops_count : 0;
         break;
      }
      case VG_PAINT_LINEAR_GRADIENT:
      {
         count = 4;
         break;
      }
      case VG_PAINT_RADIAL_GRADIENT:
      {
         count = 5;
         break;
      }

      /*
         invalid param type
      */

      default:
         set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      }

      break;
   }
   case VG_CLIENT_OBJECT_TYPE_IMAGE:
   {
      switch (param_type) {
      /*
         scalar param types
      */

      case VG_IMAGE_FORMAT:
      case VG_IMAGE_WIDTH:
      case VG_IMAGE_HEIGHT:
      {
         count = 1;
         break;
      }

      /*
         invalid param type
      */

      default:
         set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      }

      break;
   }
   case VG_CLIENT_OBJECT_TYPE_MASK_LAYER:
   {
      switch (param_type) {
      /*
         scalar param types
      */

      case VG_IMAGE_WIDTH:
      case VG_IMAGE_HEIGHT:
      {
         count = 1;
         break;
      }

      /*
         invalid param type
      */

      default:
         set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      }

      break;
   }
   case VG_CLIENT_OBJECT_TYPE_FONT:
   {
      switch (param_type) {
      /*
         scalar param types
      */

      case VG_FONT_NUM_GLYPHS:
      {
         count = 1;
         break;
      }

      /*
         invalid param type
      */

      default:
         set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      }

      break;
   }
   default:
   {
      UNREACHABLE();
   }
   }

   platform_mutex_release(&state->shared_state->mutex);
   return count;
}

VG_API_CALL void VG_API_ENTRY vgGetParameterfv(
   VGHandle vg_handle,
   VGint param_type,
   VGint count,
   VGfloat *values) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if ((count <= 0) || !values || !is_aligned_float(values)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   get_parameter_ifv(thread, state, vg_handle, param_type, count, true, values);
}

VG_API_CALL void VG_API_ENTRY vgGetParameteriv(
   VGHandle vg_handle,
   VGint param_type,
   VGint count,
   VGint *values) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if ((count <= 0) || !values || !is_aligned_int(values)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   get_parameter_ifv(thread, state, vg_handle, param_type, count, false, values);
}

/******************************************************************************
api matrices
******************************************************************************/

VG_API_CALL void VG_API_ENTRY vgLoadIdentity(void) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_MAT3X3_SYNC_T *matrix_sync;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   matrix_sync = get_matrix_sync(state, state->matrix_mode);
   vg_mat3x3_set_identity(&matrix_sync->client);
}

VG_API_CALL void VG_API_ENTRY vgLoadMatrix(const VGfloat *matrix) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_MAT3X3_SYNC_T *matrix_sync;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if (!matrix || !is_aligned_float(matrix)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   matrix_sync = get_matrix_sync(state, state->matrix_mode);
   vg_mat3x3_set_clean(&matrix_sync->client, matrix, is_matrix_affine(state->matrix_mode));
}

VG_API_CALL void VG_API_ENTRY vgGetMatrix(VGfloat *matrix) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if (!matrix || !is_aligned_float(matrix)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   vg_mat3x3_get(&get_matrix_sync(state, state->matrix_mode)->client, matrix);
}

VG_API_CALL void VG_API_ENTRY vgMultMatrix(const VGfloat *matrix) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_MAT3X3_T a;
   VG_MAT3X3_SYNC_T *matrix_sync;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if (!matrix || !is_aligned_float(matrix)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   vg_mat3x3_set_clean(&a, matrix, is_matrix_affine(state->matrix_mode));

   matrix_sync = get_matrix_sync(state, state->matrix_mode);
   vg_mat3x3_postmul(&matrix_sync->client, &a);
}

VG_API_CALL void VG_API_ENTRY vgTranslate(VGfloat x, VGfloat y) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state;
   VG_MAT3X3_SYNC_T *matrix_sync;

   x = clean_float(x);
   y = clean_float(y);

   state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   matrix_sync = get_matrix_sync(state, state->matrix_mode);
   vg_mat3x3_postmul_translate(&matrix_sync->client, x, y);
}

VG_API_CALL void VG_API_ENTRY vgScale(VGfloat x, VGfloat y) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state;
   VG_MAT3X3_SYNC_T *matrix_sync;

   x = clean_float(x);
   y = clean_float(y);

   state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   matrix_sync = get_matrix_sync(state, state->matrix_mode);
   vg_mat3x3_postmul_scale(&matrix_sync->client, x, y);
}

VG_API_CALL void VG_API_ENTRY vgShear(VGfloat x, VGfloat y) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state;
   VG_MAT3X3_SYNC_T *matrix_sync;

   x = clean_float(x);
   y = clean_float(y);

   state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   matrix_sync = get_matrix_sync(state, state->matrix_mode);
   vg_mat3x3_postmul_shear(&matrix_sync->client, x, y);
}

VG_API_CALL void VG_API_ENTRY vgRotate(VGfloat angle) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state;
   VG_MAT3X3_SYNC_T *matrix_sync;

   angle = clean_float(angle);

   state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   matrix_sync = get_matrix_sync(state, state->matrix_mode);
   vg_mat3x3_postmul_rotate(&matrix_sync->client, angle * (PI / 180.0f));
}

/******************************************************************************
api mask/clear
******************************************************************************/

VG_API_CALL void VG_API_ENTRY vgMask(
   VGHandle vg_handle, /* theoretically image under vg 1.0 */
   VGMaskOperation operation,
   VGint dst_x, VGint dst_y,
   VGint width, VGint height) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   if (!VG_GET_CLIENT_STATE(thread)) {
      return;
   }

   RPC_CALL6(vgMask_impl,
             thread,
             VGMASK_ID,
             RPC_HANDLE(vg_handle),
             RPC_ENUM(operation),
             RPC_INT(dst_x),
             RPC_INT(dst_y),
             RPC_INT(width),
             RPC_INT(height));
}

VG_API_CALL void VG_API_ENTRY vgRenderToMask(
   VGPath vg_handle,
   VGbitfield paint_modes,
   VGMaskOperation operation) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state;

#ifdef VG_NO_STROKING
   paint_modes &= ~VG_STROKE_PATH;
   if (!paint_modes) { return; }
#endif

   state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   sync_matrix(state, VG_MATRIX_PATH_USER_TO_SURFACE);

   RPC_CALL3(vgRenderToMask_impl,
             thread,
             VGRENDERTOMASK_ID,
             RPC_HANDLE(vg_handle),
             RPC_BITFIELD(paint_modes),
             RPC_ENUM(operation));
}

VG_API_CALL VGMaskLayer VG_API_ENTRY vgCreateMaskLayer(
   VGint width, VGint height) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VGHandle vg_handle;
   VG_CLIENT_MASK_LAYER_T *mask_layer;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return VG_INVALID_HANDLE;
   }

   if ((width <= 0) || (height <= 0) ||
      (width > VG_CONFIG_MAX_IMAGE_WIDTH) || (height > VG_CONFIG_MAX_IMAGE_HEIGHT) ||
      ((width * height) > VG_CONFIG_MAX_IMAGE_PIXELS)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return VG_INVALID_HANDLE;
   }

   if (egl_config_get_mask_format(egl_config_to_id(
      CLIENT_GET_THREAD_STATE()->openvg.draw->config)) == IMAGE_FORMAT_INVALID) {
      return VG_INVALID_HANDLE;
   }

   vg_handle = get_stem(state);
   if (vg_handle == VG_INVALID_HANDLE) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      return VG_INVALID_HANDLE;
   }

   mask_layer = mask_layer_alloc(width, height);
   if (!mask_layer) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      destroy_stem(vg_handle);
      return VG_INVALID_HANDLE;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   if (!insert_object(state, vg_handle, mask_layer)) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      mask_layer_free(mask_layer);
      destroy_stem(vg_handle);
      return VG_INVALID_HANDLE;
   }
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL3(vgCreateMaskLayer_impl,
             thread,
             VGCREATEMASKLAYER_ID,
             RPC_HANDLE(vg_handle),
             RPC_INT(width),
             RPC_INT(height));

   return vg_handle;
}

VG_API_CALL void VG_API_ENTRY vgDestroyMaskLayer(
   VGMaskLayer vg_handle) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   delete_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_MASK_LAYER);
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL1(vgDestroyMaskLayer_impl,
             thread,
             VGDESTROYMASKLAYER_ID,
             RPC_HANDLE(vg_handle));
}

VG_API_CALL void VG_API_ENTRY vgFillMaskLayer(
   VGMaskLayer vg_handle,
   VGint x, VGint y,
   VGint width, VGint height,
   VGfloat value) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   value = clean_float(value);

   if (!VG_GET_CLIENT_STATE(thread)) {
      return;
   }

   RPC_CALL6(vgFillMaskLayer_impl,
             thread,
             VGFILLMASKLAYER_ID,
             RPC_HANDLE(vg_handle),
             RPC_INT(x), RPC_INT(y),
             RPC_INT(width), RPC_INT(height),
             RPC_FLOAT(value));
}

VG_API_CALL void VG_API_ENTRY vgCopyMask(
   VGMaskLayer dst_vg_handle,
   VGint dst_x, VGint dst_y,
   VGint src_x, VGint src_y,
   VGint width, VGint height) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   if (!VG_GET_CLIENT_STATE(thread)) {
      return;
   }

   RPC_CALL7(vgCopyMask_impl,
             thread,
             VGCOPYMASK_ID,
             RPC_HANDLE(dst_vg_handle),
             RPC_INT(dst_x), RPC_INT(dst_y),
             RPC_INT(src_x), RPC_INT(src_y),
             RPC_INT(width), RPC_INT(height));
}

VG_API_CALL void VG_API_ENTRY vgClear(
   VGint x, VGint y,
   VGint width, VGint height) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   //TODO: pixmap behaviour can be better optimized to handle clears
   if (state->render_callback)
      state->render_callback();

   RPC_CALL4(vgClear_impl,
             thread,
             VGCLEAR_ID,
             RPC_INT(x),
             RPC_INT(y),
             RPC_INT(width),
             RPC_INT(height));
}

/******************************************************************************
api path
******************************************************************************/

VG_API_CALL VGPath VG_API_ENTRY vgCreatePath(
   VGint format,
   VGPathDatatype datatype,
   VGfloat scale, VGfloat bias,
   VGint segments_capacity,
   VGint coords_capacity,
   VGbitfield caps) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state;
   VGHandle vg_handle;
   VG_CLIENT_PATH_T *path;

   scale = clean_float(scale);
   bias = clean_float(bias);
   caps &= VG_PATH_CAPABILITY_ALL;

   state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return VG_INVALID_HANDLE;
   }

   if (!is_path_format(format)) {
      set_error(VG_UNSUPPORTED_PATH_FORMAT_ERROR);
      return VG_INVALID_HANDLE;
   }
   if (!is_path_datatype(datatype) || is_zero(scale)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return VG_INVALID_HANDLE;
   }

   vg_handle = get_stem(state);
   if (vg_handle == VG_INVALID_HANDLE) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      return VG_INVALID_HANDLE;
   }

   path = path_alloc(format, datatype, scale, bias, caps, segments_capacity);
   if (!path) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      destroy_stem(vg_handle);
      return VG_INVALID_HANDLE;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   if (!insert_object(state, vg_handle, path)) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      path_free(path);
      destroy_stem(vg_handle);
      return VG_INVALID_HANDLE;
   }
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL8(vgCreatePath_impl,
             thread,
             VGCREATEPATH_ID,
             RPC_HANDLE(vg_handle),
             RPC_INT(format),
             RPC_ENUM(datatype),
             RPC_FLOAT(scale), RPC_FLOAT(bias),
             RPC_INT(segments_capacity),
             RPC_INT(coords_capacity),
             RPC_BITFIELD(caps));

   return vg_handle;
}

VG_API_CALL void VG_API_ENTRY vgClearPath(
   VGPath vg_handle,
   VGbitfield caps) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state;
   VG_CLIENT_PATH_T *path;

   caps &= VG_PATH_CAPABILITY_ALL;

   state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   path = (VG_CLIENT_PATH_T *)lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_PATH);
   if (path) {
      if (need_path_segments(path->caps) && need_path_segments(caps)) {
         khrn_vector_clear(&path->segments);
      }
      path_update_caps(path, caps);
   }
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL2(vgClearPath_impl,
             thread,
             VGCLEARPATH_ID,
             RPC_HANDLE(vg_handle),
             RPC_BITFIELD(caps));
}

VG_API_CALL void VG_API_ENTRY vgDestroyPath(
   VGPath vg_handle) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   delete_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_PATH);
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL1(vgDestroyPath_impl,
             thread,
             VGDESTROYPATH_ID,
             RPC_HANDLE(vg_handle));
}

VG_API_CALL void VG_API_ENTRY vgRemovePathCapabilities(
   VGPath vg_handle,
   VGbitfield caps) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_PATH_T *path;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   path = (VG_CLIENT_PATH_T *)lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_PATH);
   if (path) {
      path_update_caps(path, path->caps & ~caps);
   }
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL2(vgRemovePathCapabilities_impl,
             thread,
             VGREMOVEPATHCAPABILITIES_ID,
             RPC_HANDLE(vg_handle),
             RPC_BITFIELD(caps));
}

VG_API_CALL VGbitfield VG_API_ENTRY vgGetPathCapabilities(
   VGPath vg_handle) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VGbitfield caps = 0;
   VG_CLIENT_PATH_T *path;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return 0;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   path = (VG_CLIENT_PATH_T *)lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_PATH);
   if (path) {
      caps = path->caps;
   } else {
      set_error(VG_BAD_HANDLE_ERROR);
   }
   platform_mutex_release(&state->shared_state->mutex);

   return caps;
}

VG_API_CALL void VG_API_ENTRY vgAppendPath(
   VGPath dst_vg_handle,
   VGPath src_vg_handle) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_PATH_T *dst;
   VG_CLIENT_PATH_T *src;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   dst = (VG_CLIENT_PATH_T *)lookup_object(state, dst_vg_handle, VG_CLIENT_OBJECT_TYPE_PATH);
   src = (VG_CLIENT_PATH_T *)lookup_object(state, src_vg_handle, VG_CLIENT_OBJECT_TYPE_PATH);
   if (dst && src &&
      (dst->caps & VG_PATH_CAPABILITY_APPEND_TO) && (src->caps & VG_PATH_CAPABILITY_APPEND_FROM) &&
      need_path_segments(dst->caps)) {
      VGuint segments_count = src->segments.size;
      if (!khrn_vector_extend(&dst->segments, segments_count)) {
         set_error(VG_OUT_OF_MEMORY_ERROR);
         platform_mutex_release(&state->shared_state->mutex);
         return;
      }
      memcpy((VGubyte *)dst->segments.data + (dst->segments.size - segments_count), src->segments.data, segments_count);
   }
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL2(vgAppendPath_impl,
             thread,
             VGAPPENDPATH_ID,
             RPC_HANDLE(dst_vg_handle),
             RPC_HANDLE(src_vg_handle));
}

VG_API_CALL void VG_API_ENTRY vgAppendPathData(
   VGPath vg_handle,
   VGint segments_count, const VGubyte *segments,
   const void *coords) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_PATH_T *path;
   VGPathDatatype datatype;
   VGuint datatype_size;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   platform_mutex_acquire(&state->shared_state->mutex);

   path = (VG_CLIENT_PATH_T *)lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_PATH);
   if (!path) {
      set_error(VG_BAD_HANDLE_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      return;
   }

   datatype = path->datatype;

   if ((segments_count <= 0) || !segments ||
      contains_illegal_segment(segments, segments_count) ||
      !coords || !is_aligned_path_datatype(coords, datatype)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      return;
   }

   if (!(path->caps & VG_PATH_CAPABILITY_APPEND_TO)) {
      set_error(VG_PATH_CAPABILITY_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      return;
   }

   if (need_path_segments(path->caps)) {
      if (!khrn_vector_extend(&path->segments, segments_count)) {
         set_error(VG_OUT_OF_MEMORY_ERROR);
         platform_mutex_release(&state->shared_state->mutex);
         return;
      }
      memcpy((VGubyte *)path->segments.data + (path->segments.size - segments_count), segments, segments_count);
   }

   platform_mutex_release(&state->shared_state->mutex);

   datatype_size = get_path_datatype_size(datatype);

   #ifdef RPC_DIRECT
      {
         VGuint coords_count = 0;
         VGuint i;
         for (i = 0; i != segments_count; ++i) {
            coords_count += get_segment_coords_count(segments[i] & ~VG_RELATIVE);
         }

         RPC_CALL6(vgAppendPathData_impl, thread, no_id, 
            vg_handle,
            datatype,
            segments_count, segments,
            coords_count * datatype_size, coords);
      }
   #else
      /*
         split into multiple calls if necessary to avoid overflowing control buffer
      */

      while (segments_count != 0) {
         #define MESSAGE_SIZE 20

         VGuint size_max = rpc_send_ctrl_longest(thread, MESSAGE_SIZE + rpc_pad_ctrl(1) + rpc_pad_ctrl(6 * datatype_size)) - MESSAGE_SIZE; /* fit at least one segment */
         VGint chunk_segments_count = 0;
         VGuint chunk_coords_size = 0;
         for (; chunk_segments_count != segments_count; ++chunk_segments_count) {
            VGuint segment_coords_size = get_segment_coords_count(segments[chunk_segments_count] & ~VG_RELATIVE) * datatype_size;
            if ((rpc_pad_ctrl(chunk_segments_count + 1) +
               rpc_pad_ctrl(chunk_coords_size + segment_coords_size)) > size_max) {
               /*
                  can't fit this segment in
               */

               break;
            }
            chunk_coords_size += segment_coords_size;
         }

         {
            uint32_t message[] = {
               VGAPPENDPATHDATA_ID,
               RPC_HANDLE(vg_handle),
               RPC_ENUM(datatype),
               RPC_INT(chunk_segments_count),
               RPC_UINT(chunk_coords_size) };
            vcos_static_assert(sizeof(message) == MESSAGE_SIZE);

            #undef MESSAGE_SIZE
            CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();

            rpc_send_ctrl_begin(thread, sizeof(message) + rpc_pad_ctrl(chunk_segments_count) + rpc_pad_ctrl(chunk_coords_size));
            rpc_send_ctrl_write(thread, message, sizeof(message));
            rpc_send_ctrl_write(thread, (uint32_t *)segments, chunk_segments_count);
            rpc_send_ctrl_write(thread, (uint32_t *)coords, chunk_coords_size);
            rpc_send_ctrl_end(thread);
         }

         segments_count -= chunk_segments_count;
         segments += chunk_segments_count;
         coords = (const uint8_t *)coords + chunk_coords_size;
      }
   #endif
}

VG_API_CALL void VG_API_ENTRY vgModifyPathCoords(
   VGPath vg_handle,
   VGint segments_i, VGint segments_count,
   const void *coords) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_PATH_T *path;
   VGPathDatatype datatype;
   VGuint datatype_size;
   VGuint coords_offset;
   VGuint coords_size;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   platform_mutex_acquire(&state->shared_state->mutex);

   path = (VG_CLIENT_PATH_T *)lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_PATH);
   if (!path) {
      set_error(VG_BAD_HANDLE_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      return;
   }

   if (!(path->caps & VG_PATH_CAPABILITY_MODIFY)) {
      set_error(VG_PATH_CAPABILITY_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      return;
   }

   datatype = path->datatype;

   if ((segments_i < 0) || (segments_count <= 0) ||
      /* will fit in VGuint (may not fit in VGint) */
      (((VGuint)(segments_i + segments_count)) > path->segments.size) ||
      !coords || !is_aligned_path_datatype(coords, datatype)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      return;
   }

   datatype_size = get_path_datatype_size(path->datatype);
   coords_offset = get_coords_count((const VGubyte *)path->segments.data, segments_i) * datatype_size;
   coords_size = get_coords_count((const VGubyte *)path->segments.data + segments_i, segments_count) * datatype_size;

   platform_mutex_release(&state->shared_state->mutex);

   #ifdef RPC_DIRECT
      RPC_CALL5(vgModifyPathCoords_impl, thread, no_id, 
         vg_handle,
         datatype,
         coords_offset, coords_size, coords);
   #else
      /*
         split into multiple calls if necessary to avoid overflowing control buffer
      */

      while (coords_size != 0) {
         #define MESSAGE_SIZE 20

         VGuint chunk_coords_size = _min(coords_size, rpc_send_ctrl_longest(thread, MESSAGE_SIZE + rpc_pad_ctrl(4)) - MESSAGE_SIZE); /* fit at least one coordinate */

         uint32_t message[] = {
            VGMODIFYPATHCOORDS_ID,
            RPC_HANDLE(vg_handle),
            RPC_ENUM(datatype),
            RPC_UINT(coords_offset),
            RPC_UINT(chunk_coords_size) };
         vcos_static_assert(sizeof(message) == MESSAGE_SIZE);

         #undef MESSAGE_SIZE

         rpc_send_ctrl_begin(thread, sizeof(message) + rpc_pad_ctrl(chunk_coords_size));
         rpc_send_ctrl_write(thread, message, sizeof(message));
         rpc_send_ctrl_write(thread, (uint32_t *)coords, chunk_coords_size);
         rpc_send_ctrl_end(thread);

         coords_size -= chunk_coords_size;
         coords_offset += chunk_coords_size;
         coords = (const uint8_t *)coords + chunk_coords_size;
      }
   #endif
}

VG_API_CALL void VG_API_ENTRY vgTransformPath(
   VGPath dst_vg_handle,
   VGPath src_vg_handle) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_PATH_T *dst;
   VG_CLIENT_PATH_T *src;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   dst = (VG_CLIENT_PATH_T *)lookup_object(state, dst_vg_handle, VG_CLIENT_OBJECT_TYPE_PATH);
   src = (VG_CLIENT_PATH_T *)lookup_object(state, src_vg_handle, VG_CLIENT_OBJECT_TYPE_PATH);
   if (dst && src &&
      (dst->caps & VG_PATH_CAPABILITY_TRANSFORM_TO) && (src->caps & VG_PATH_CAPABILITY_TRANSFORM_FROM) &&
      need_path_segments(dst->caps)) {
      VGuint segments_count = src->segments.size;
      if (!khrn_vector_extend(&dst->segments, segments_count)) {
         set_error(VG_OUT_OF_MEMORY_ERROR);
         platform_mutex_release(&state->shared_state->mutex);
         return;
      }
      {
         VGubyte *dst_segments = (VGubyte *)dst->segments.data + (dst->segments.size - segments_count);
         const VGubyte *src_segments = (const VGubyte *)src->segments.data;
         for (; segments_count != 0; ++dst_segments, ++src_segments, --segments_count) {
            VGuint segment = *src_segments;
            if (((segment & ~VG_RELATIVE) == VG_HLINE_TO) || ((segment & ~VG_RELATIVE) == VG_VLINE_TO)) {
               segment = VG_LINE_TO | (segment & VG_RELATIVE);
            }

            /*
               on the server, we also flip arc types if the determinant of the path
               user to surface matrix is negative, but we don't actually care about
               the particular arc type on the client
            */

            *dst_segments = (VGubyte)segment;
         }
      }
   }
   platform_mutex_release(&state->shared_state->mutex);

   sync_matrix(state, VG_MATRIX_PATH_USER_TO_SURFACE);

   RPC_CALL2(vgTransformPath_impl,
             thread,
             VGTRANSFORMPATH_ID,
             RPC_HANDLE(dst_vg_handle),
             RPC_HANDLE(src_vg_handle));
}

VG_API_CALL VGboolean VG_API_ENTRY vgInterpolatePath(
   VGPath dst_vg_handle,
   VGPath begin_vg_handle,
   VGPath end_vg_handle,
   VGfloat t) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state;
   VG_CLIENT_PATH_T *dst;
   VG_CLIENT_PATH_T *begin;
   VG_CLIENT_PATH_T *end;

   t = clean_float(t);

   state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return VG_FALSE;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   dst = (VG_CLIENT_PATH_T *)lookup_object(state, dst_vg_handle, VG_CLIENT_OBJECT_TYPE_PATH);
   begin = (VG_CLIENT_PATH_T *)lookup_object(state, begin_vg_handle, VG_CLIENT_OBJECT_TYPE_PATH);
   end = (VG_CLIENT_PATH_T *)lookup_object(state, end_vg_handle, VG_CLIENT_OBJECT_TYPE_PATH);
   if (dst && begin && end &&
      (dst->caps & VG_PATH_CAPABILITY_INTERPOLATE_TO) && (begin->caps & VG_PATH_CAPABILITY_INTERPOLATE_FROM) && (end->caps & VG_PATH_CAPABILITY_INTERPOLATE_FROM)) {
      if ((begin->segments.size != end->segments.size) ||
         !interpolate_segments_compatible((const VGubyte *)begin->segments.data, (const VGubyte *)end->segments.data, begin->segments.size)) {
         /*
            incompatible paths. no error -- just return false
         */

         platform_mutex_release(&state->shared_state->mutex);
         return VG_FALSE;
      }
      if (need_path_segments(dst->caps)) {
         VGuint segments_count = begin->segments.size;
         if (!khrn_vector_extend(&dst->segments, segments_count)) {
            set_error(VG_OUT_OF_MEMORY_ERROR);
            platform_mutex_release(&state->shared_state->mutex);
            return VG_FALSE;
         }
         {
            VGubyte *dst_segments = (VGubyte *)dst->segments.data + (dst->segments.size - segments_count);
            const VGubyte *begin_segments = (const VGubyte *)begin->segments.data;
            for (; segments_count != 0; ++dst_segments, ++begin_segments, --segments_count) {
               *dst_segments = (VGubyte)normalise_segment(*begin_segments & ~VG_RELATIVE);
            }
         }
      }
   }
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL4(vgInterpolatePath_impl,
             thread,
             VGINTERPOLATEPATH_ID,
             RPC_HANDLE(dst_vg_handle),
             RPC_HANDLE(begin_vg_handle),
             RPC_HANDLE(end_vg_handle),
             RPC_FLOAT(t));

   return VG_TRUE;
}

VG_API_CALL VGfloat VG_API_ENTRY vgPathLength(
   VGPath vg_handle,
   VGint segments_i, VGint segments_count) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   if (!VG_GET_CLIENT_STATE(thread)) {
      return 0.0f;
   }

   return RPC_FLOAT_RES(RPC_CALL3_RES(vgPathLength_impl,
                                      thread,
                                      VGPATHLENGTH_ID,
                                      RPC_HANDLE(vg_handle),
                                      RPC_INT(segments_i), RPC_INT(segments_count)));
}

VG_API_CALL void VG_API_ENTRY vgPointAlongPath(
   VGPath vg_handle,
   VGint segments_i, VGint segments_count,
   VGfloat distance,
   VGfloat *x, VGfloat *y,
   VGfloat *tangent_x, VGfloat *tangent_y) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VGfloat values[4];

   distance = clean_float(distance);

   if (!VG_GET_CLIENT_STATE(thread)) {
      return;
   }

   if ((x && !is_aligned_float(x)) || (y && !is_aligned_float(y)) ||
      (tangent_x && !is_aligned_float(tangent_x)) || (tangent_y && !is_aligned_float(tangent_y))) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   if (RPC_BOOLEAN_RES(RPC_CALL6_OUT_CTRL_RES(vgPointAlongPath_impl,
                                              thread,
                                              VGPOINTALONGPATH_ID,
                                              RPC_HANDLE(vg_handle),
                                              RPC_INT(segments_i), RPC_INT(segments_count),
                                              RPC_FLOAT(distance),
                                              RPC_BITFIELD(((x && y) << 0) | ((tangent_x && tangent_y) << 1)),
                                              values))) {
      /*
         no error occurred on the server side
      */

      if (x && y) {
         *x = values[0];
         *y = values[1];
      }
      if (tangent_x && tangent_y) {
         *tangent_x = values[2];
         *tangent_y = values[3];
      }
   }
}

VG_API_CALL void VG_API_ENTRY vgPathBounds(
   VGPath vg_handle,
   VGfloat *min_x, VGfloat *min_y,
   VGfloat *width, VGfloat *height) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VGfloat values[4];

   if (!VG_GET_CLIENT_STATE(thread)) {
      return;
   }

   if (!min_x || !is_aligned_float(min_x) || !min_y || !is_aligned_float(min_y) ||
      !width || !is_aligned_float(width) || !height || !is_aligned_float(height)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   if (RPC_BOOLEAN_RES(RPC_CALL2_OUT_CTRL_RES(vgPathBounds_impl,
                                              thread,
                                              VGPATHBOUNDS_ID,
                                              RPC_HANDLE(vg_handle),
                                              values))) {
      /*
         no error occurred on the server side
      */

      *min_x = values[0];
      *min_y = values[1];
      *width = values[2];
      *height = values[3];
   }
}

VG_API_CALL void VG_API_ENTRY vgPathTransformedBounds(
   VGPath vg_handle,
   VGfloat *min_x, VGfloat *min_y,
   VGfloat *width, VGfloat *height) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VGfloat values[4];

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if (!min_x || !is_aligned_float(min_x) || !min_y || !is_aligned_float(min_y) ||
      !width || !is_aligned_float(width) || !height || !is_aligned_float(height)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   sync_matrix(state, VG_MATRIX_PATH_USER_TO_SURFACE);

   if (RPC_BOOLEAN_RES(RPC_CALL2_OUT_CTRL_RES(vgPathTransformedBounds_impl,
                                              thread,
                                              VGPATHTRANSFORMEDBOUNDS_ID,
                                              RPC_HANDLE(vg_handle),
                                              values))) {
      /*
         no error occurred on the server side
      */

      *min_x = values[0];
      *min_y = values[1];
      *width = values[2];
      *height = values[3];
   }
}

VG_API_CALL void VG_API_ENTRY vgDrawPath(
   VGPath vg_handle,
   VGbitfield paint_modes) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state;

#ifdef VG_NO_STROKING
   paint_modes &= ~VG_STROKE_PATH;
   if (!paint_modes) { return; }
#endif

   state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   sync_matrix(state, VG_MATRIX_PATH_USER_TO_SURFACE);
   if (paint_modes & VG_FILL_PATH) {
      sync_matrix(state, VG_MATRIX_FILL_PAINT_TO_USER);
   }
   if (paint_modes & VG_STROKE_PATH) {
      sync_matrix(state, VG_MATRIX_STROKE_PAINT_TO_USER);
   }

   if (state->render_callback)
      state->render_callback();

   RPC_CALL2(vgDrawPath_impl,
             thread,
             VGDRAWPATH_ID,
             RPC_HANDLE(vg_handle),
             RPC_BITFIELD(paint_modes));
}

/******************************************************************************
api paint
******************************************************************************/

VG_API_CALL VGPaint VG_API_ENTRY vgCreatePaint(void) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VGHandle vg_handle;
   VG_CLIENT_PAINT_T *paint;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return VG_INVALID_HANDLE;
   }

   vg_handle = get_stem(state);
   if (vg_handle == VG_INVALID_HANDLE) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      return VG_INVALID_HANDLE;
   }

   paint = paint_alloc();
   if (!paint) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      destroy_stem(vg_handle);
      return VG_INVALID_HANDLE;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   if (!insert_object(state, vg_handle, paint)) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      paint_free(paint);
      destroy_stem(vg_handle);
      return VG_INVALID_HANDLE;
   }
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL1(vgCreatePaint_impl,
             thread,
             VGCREATEPAINT_ID,
             RPC_HANDLE(vg_handle));

   return vg_handle;
}

VG_API_CALL void VG_API_ENTRY vgDestroyPaint(
   VGPaint vg_handle) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   delete_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_PAINT);
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL1(vgDestroyPaint_impl,
             thread,
             VGDESTROYPAINT_ID,
             RPC_HANDLE(vg_handle));
}

VG_API_CALL void VG_API_ENTRY vgSetPaint(
   VGPaint vg_handle,
   VGbitfield paint_modes) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if (!is_paint_modes(paint_modes)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   if ((vg_handle != VG_INVALID_HANDLE) &&
      !lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_PAINT)) {
      set_error(VG_BAD_HANDLE_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      return;
   }
   platform_mutex_release(&state->shared_state->mutex);

   if (((paint_modes & VG_FILL_PATH) && (state->fill_paint != vg_handle)) ||
      ((paint_modes & VG_STROKE_PATH) && (state->stroke_paint != vg_handle))) {
      if (paint_modes & VG_FILL_PATH) {
         state->fill_paint = vg_handle;
      }
      if (paint_modes & VG_STROKE_PATH) {
         state->stroke_paint = vg_handle;
      }
      RPC_CALL2(vgSetPaint_impl,
                thread,
                VGSETPAINT_ID,
                RPC_HANDLE(vg_handle),
                RPC_BITFIELD(paint_modes));
   }
}

VG_API_CALL VGPaint VG_API_ENTRY vgGetPaint(
   VGPaintMode paint_mode) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return VG_INVALID_HANDLE;
   }

   if (!is_paint_mode(paint_mode)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return VG_INVALID_HANDLE;
   }

   return (paint_mode == VG_FILL_PATH) ? state->fill_paint : state->stroke_paint;
}

VG_API_CALL void VG_API_ENTRY vgSetColor(
   VGPaint vg_handle,
   VGuint rgba) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state;
   VG_CLIENT_PAINT_T *paint;

   rgba = khrn_color_rgba_flip(rgba);

   state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   paint = (VG_CLIENT_PAINT_T *)lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_PAINT);
   if (!paint) {
      set_error(VG_BAD_HANDLE_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      return;
   }
   if (color_floats_to_rgba_clean(paint->color) != rgba) {
      set_parameter_iv_server(vg_handle, VG_CLIENT_OBJECT_TYPE_PAINT, VG_PAINT_COLOR, 1, (const VGint *)&rgba);
   }
   khrn_color_rgba_to_floats(paint->color, rgba);
   platform_mutex_release(&state->shared_state->mutex);
}

VG_API_CALL VGuint VG_API_ENTRY vgGetColor(
   VGPaint vg_handle) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_PAINT_T *paint;
   VGuint rgba;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return 0;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   paint = (VG_CLIENT_PAINT_T *)lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_PAINT);
   if (!paint) {
      set_error(VG_BAD_HANDLE_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      return 0;
   }
   rgba = color_floats_to_rgba_clean(paint->color);
   platform_mutex_release(&state->shared_state->mutex);
   return khrn_color_rgba_flip(rgba);
}

VG_API_CALL void VG_API_ENTRY vgPaintPattern(
   VGPaint vg_handle,
   VGImage pattern_vg_handle) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_PAINT_T *paint;
   VG_CLIENT_IMAGE_T *pattern;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   paint = (VG_CLIENT_PAINT_T *)lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_PAINT);
   pattern = (pattern_vg_handle == VG_INVALID_HANDLE) ? NULL : (VG_CLIENT_IMAGE_T *)lookup_object(state, pattern_vg_handle, VG_CLIENT_OBJECT_TYPE_IMAGE);
   if (!paint || ((pattern_vg_handle != VG_INVALID_HANDLE) && !pattern)) {
      set_error(VG_BAD_HANDLE_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      return;
   }
   if (paint->pattern != pattern_vg_handle) {
      paint->pattern = pattern_vg_handle;
#if EGL_BRCM_global_image && EGL_KHR_image
      if (pattern && (pattern->global_image_id[0] || pattern->global_image_id[1])) {
         platform_acquire_global_image(pattern->global_image_id[0], pattern->global_image_id[1]);
      }
      if (paint->pattern_global_image_id[0] || paint->pattern_global_image_id[1]) {
         platform_release_global_image(paint->pattern_global_image_id[0], paint->pattern_global_image_id[1]);
      }
      paint->pattern_global_image_id[0] = pattern ? pattern->global_image_id[0] : 0;
      paint->pattern_global_image_id[1] = pattern ? pattern->global_image_id[1] : 0;
#endif
      RPC_CALL2(vgPaintPattern_impl,
                thread,
                VGPAINTPATTERN_ID,
                RPC_HANDLE(vg_handle),
                RPC_HANDLE(pattern_vg_handle));
   }
   platform_mutex_release(&state->shared_state->mutex);
}

/******************************************************************************
api image
******************************************************************************/

VG_API_CALL VGImage VG_API_ENTRY vgCreateImage(
   VGImageFormat format,
   VGint width, VGint height,
   VGbitfield allowed_quality) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VGHandle vg_handle;
   VG_CLIENT_IMAGE_T *image;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return VG_INVALID_HANDLE;
   }

   if (!is_allowed_quality(allowed_quality) ||
      (width <= 0) || (height <= 0) ||
      (width > VG_CONFIG_MAX_IMAGE_WIDTH) || (height > VG_CONFIG_MAX_IMAGE_HEIGHT) ||
      ((width * height) > VG_CONFIG_MAX_IMAGE_PIXELS)) { /* todo: VG_CONFIG_MAX_IMAGE_BYTES */
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return VG_INVALID_HANDLE;
   }
   if (!is_image_format(format)) {
      set_error(VG_UNSUPPORTED_IMAGE_FORMAT_ERROR);
      return VG_INVALID_HANDLE;
   }

   vg_handle = get_stem(state);
   if (vg_handle == VG_INVALID_HANDLE) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      return VG_INVALID_HANDLE;
   }

   image = image_alloc(format, width, height
#if EGL_BRCM_global_image && EGL_KHR_image
      , 0, 0
#endif
      );
   if (!image) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      destroy_stem(vg_handle);
      return VG_INVALID_HANDLE;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   if (!insert_object(state, vg_handle, image)) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      image_free(image);
      destroy_stem(vg_handle);
      return VG_INVALID_HANDLE;
   }
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL5(vgCreateImage_impl,
             thread,
             VGCREATEIMAGE_ID,
             RPC_HANDLE(vg_handle),
             RPC_ENUM(format),
             RPC_INT(width), RPC_INT(height),
             RPC_BITFIELD(allowed_quality));

   return vg_handle;
}

VG_API_CALL void VG_API_ENTRY vgDestroyImage(
   VGImage vg_handle) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   delete_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_IMAGE);
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL1(vgDestroyImage_impl,
             thread,
             VGDESTROYIMAGE_ID,
             RPC_HANDLE(vg_handle));
}

VG_API_CALL void VG_API_ENTRY vgClearImage(
   VGImage vg_handle,
   VGint x, VGint y,
   VGint width, VGint height) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   if (!VG_GET_CLIENT_STATE(thread)) {
      return;
   }

   RPC_CALL5(vgClearImage_impl,
             thread,
             VGCLEARIMAGE_ID,
             RPC_HANDLE(vg_handle),
             RPC_INT(x), RPC_INT(y),
             RPC_INT(width), RPC_INT(height));
}

VG_API_CALL void VG_API_ENTRY vgImageSubData(
   VGImage vg_handle,
   const void *data, VGint data_stride,
   VGImageFormat data_format,
   VGint dst_x, VGint dst_y,
   VGint width, VGint height) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_IMAGE_T *dst;
   VGint dst_width;
   VGint dst_height;
   VGint src_x = 0, src_y = 0;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if (!is_image_format(data_format)) {
      set_error(VG_UNSUPPORTED_IMAGE_FORMAT_ERROR);
      return;
   }

   if (!data || !is_aligned_image_format(data, data_format) ||
      ((height != 1) && !is_aligned_image_format((void *)(uintptr_t)data_stride, data_format)) ||
      (width <= 0) || (height <= 0)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   platform_mutex_acquire(&state->shared_state->mutex);

   dst = (VG_CLIENT_IMAGE_T *)lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_IMAGE);
   if (!dst) {
      set_error(VG_BAD_HANDLE_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      return;
   }

   dst_width = dst->width;
   dst_height = dst->height;

   platform_mutex_release(&state->shared_state->mutex);

   khrn_clip_rect2(
      &dst_x, &dst_y, &src_x, &src_y, &width, &height,
      0, 0, dst_width, dst_height,
      0, 0, width, height);

   if (width <= 0 || height <= 0) {
      return;
   }
   
   data = (const VGubyte *)data + (src_y * data_stride);

   #ifdef RPC_DIRECT
      RPC_CALL11(vgImageSubData_impl, thread, no_id, 
         vg_handle,
         dst_width, dst_height,
         data, data_stride,
         data_format,
         src_x,
         dst_x, dst_y,
         width, height);
   #else
      {
         VGuint log2_bpp = get_log2_image_format_bpp(data_format);
         VGuint line_size;
         VGuint chunk_height_max;

         data = (const VGubyte *)data + ((src_x << log2_bpp) >> 3);
         src_x = ((src_x << log2_bpp) & 0x7) >> log2_bpp;

         line_size = (((src_x + width) << log2_bpp) + 0x7) >> 3;

         /*
            split into KHDISPATCH_WORKSPACE_SIZE chunks (or thereabouts)
         */

         chunk_height_max = (line_size == 0) ? height : (KHDISPATCH_WORKSPACE_SIZE / line_size);
         vcos_assert((height == 0) || (chunk_height_max != 0));

         while (height != 0) {
            VGint chunk_height = _min(height, chunk_height_max);

            uint32_t message[] = {
               VGIMAGESUBDATA_ID,
               RPC_HANDLE(vg_handle),
               RPC_INT(dst_width), RPC_INT(dst_height),
               RPC_UINT(line_size),
               RPC_ENUM(data_format),
               RPC_INT(src_x),
               RPC_INT(dst_x), RPC_INT(dst_y),
               RPC_INT(width), RPC_INT(chunk_height) };

            rpc_begin(thread);
            rpc_send_ctrl_begin(thread, sizeof(message));
            rpc_send_ctrl_write(thread, message, sizeof(message));
            rpc_send_ctrl_end(thread);
            rpc_send_bulk_gather(thread, data, line_size, data_stride, chunk_height);
            data = (const VGubyte *)data + (chunk_height * data_stride);
            rpc_end(thread);

            height -= chunk_height;
            dst_y += chunk_height;
         }
      }
   #endif
}

VG_API_CALL void VG_API_ENTRY vgGetImageSubData(
   VGImage vg_handle,
   void *data, VGint data_stride,
   VGImageFormat data_format,
   VGint src_x, VGint src_y,
   VGint width, VGint height) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_IMAGE_T *src;
   VGint src_width;
   VGint src_height;
   VGint dst_x = 0, dst_y = 0;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if (!is_image_format(data_format)) {
      set_error(VG_UNSUPPORTED_IMAGE_FORMAT_ERROR);
      return;
   }

   if (!data || !is_aligned_image_format(data, data_format) ||
      ((height != 1) && !is_aligned_image_format((void *)(uintptr_t)data_stride, data_format)) ||
      (width <= 0) || (height <= 0)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   platform_mutex_acquire(&state->shared_state->mutex);

   src = (VG_CLIENT_IMAGE_T *)lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_IMAGE);
   if (!src) {
      set_error(VG_BAD_HANDLE_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      return;
   }

   src_width = src->width;
   src_height = src->height;

   platform_mutex_release(&state->shared_state->mutex);

   khrn_clip_rect2(
      &dst_x, &dst_y, &src_x, &src_y, &width, &height,
      0, 0, width, height,
      0, 0, src_width, src_height);

   if (width <= 0 || height <= 0) {
      return;
   }
   
   data = (VGubyte *)data + (dst_y * data_stride);

   #ifdef RPC_DIRECT
      RPC_CALL11(vgGetImageSubData_impl, thread, no_id, 
         vg_handle,
         src_width, src_height,
         data, data_stride,
         data_format,
         dst_x,
         src_x, src_y,
         width, height);
   #else
      {
         VGuint log2_bpp = get_log2_image_format_bpp(data_format);
         VGuint line_size;
         VGuint first_mask;
         VGuint last_mask;
         VGuint chunk_height_max;

         data = (VGubyte *)data + ((dst_x << log2_bpp) >> 3);
         dst_x = ((dst_x << log2_bpp) & 0x7) >> log2_bpp;

         line_size = (((dst_x + width) << log2_bpp) + 0x7) >> 3;
         first_mask = (1 << (dst_x << log2_bpp)) - 1;
         last_mask = ~((2 << ((((dst_x + width) << log2_bpp) - 1) & 0x7)) - 1) & 0xff;

         /*
            split into KHDISPATCH_WORKSPACE_SIZE chunks (or thereabouts)
         */

         chunk_height_max = (line_size == 0) ? height : (KHDISPATCH_WORKSPACE_SIZE / line_size);
         vcos_assert((height == 0) || (chunk_height_max != 0));

         while (height != 0) {
            VGint chunk_height = _min(height, chunk_height_max);

            uint32_t message[] = {
               VGGETIMAGESUBDATA_ID,
               RPC_HANDLE(vg_handle),
               RPC_INT(src_width), RPC_INT(src_height),
               RPC_UINT(line_size),
               RPC_ENUM(data_format),
               RPC_INT(dst_x),
               RPC_INT(src_x), RPC_INT(src_y),
               RPC_INT(width), RPC_INT(chunk_height) };

            rpc_begin(thread);
            rpc_send_ctrl_begin(thread, sizeof(message));
            rpc_send_ctrl_write(thread, message, sizeof(message));
            rpc_send_ctrl_end(thread);
            {
               uint32_t len[] = { 0, data_stride, chunk_height, first_mask, last_mask };
               rpc_recv(thread, data, len, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_BULK_SCATTER | RPC_RECV_FLAG_LEN));
               data = (VGubyte *)data + (chunk_height * data_stride);
               if (len[0] == 0) {  //The command has failed so no data sent back
                  rpc_end(thread);
                  break;
               }
               vcos_assert(len[0] == line_size);
            }
            rpc_end(thread);
            height -= chunk_height;
            src_y += chunk_height;
         }
      }
   #endif
}

VG_API_CALL VGImage VG_API_ENTRY vgChildImage(
   VGImage parent_vg_handle,
   VGint x, VGint y,
   VGint width, VGint height) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_IMAGE_T *parent;
   VGImageFormat format;
   VGint parent_width;
   VGint parent_height;
#if EGL_BRCM_global_image && EGL_KHR_image
   VGuint parent_global_image_id_0, parent_global_image_id_1;
#endif
   VGHandle vg_handle;
   VG_CLIENT_IMAGE_T *image;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return VG_INVALID_HANDLE;
   }

   platform_mutex_acquire(&state->shared_state->mutex);

   parent = (VG_CLIENT_IMAGE_T *)lookup_object(state, parent_vg_handle, VG_CLIENT_OBJECT_TYPE_IMAGE);
   if (!parent) {
      set_error(VG_BAD_HANDLE_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      return VG_INVALID_HANDLE;
   }

   format = parent->format;
   parent_width = parent->width;
   parent_height = parent->height;
#if EGL_BRCM_global_image && EGL_KHR_image
   parent_global_image_id_0 = parent->global_image_id[0];
   parent_global_image_id_1 = parent->global_image_id[1];
#endif

   platform_mutex_release(&state->shared_state->mutex); /* mustn't hold this mutex when calling get_stem, so release now and acquire again later */

   if ((x < 0) || (y < 0) || (width <= 0) || (height <= 0) ||
      /* x + width / y + height will fit in VGuint (may not fit in VGint) */
      (((VGuint)(x + width)) > (VGuint)parent_width) ||
      (((VGuint)(y + height)) > (VGuint)parent_height)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return VG_INVALID_HANDLE;
   }

   vg_handle = get_stem(state);
   if (vg_handle == VG_INVALID_HANDLE) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      return VG_INVALID_HANDLE;
   }

   image = image_alloc(format, width, height
#if EGL_BRCM_global_image && EGL_KHR_image
      , parent_global_image_id_0, parent_global_image_id_1
#endif
      );
   if (!image) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      destroy_stem(vg_handle);
      return VG_INVALID_HANDLE;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   if (!insert_object(state, vg_handle, image)) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      image_free(image);
      destroy_stem(vg_handle);
      return VG_INVALID_HANDLE;
   }
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL8(vgChildImage_impl,
             thread,
             VGCHILDIMAGE_ID,
             RPC_HANDLE(vg_handle),
             RPC_HANDLE(parent_vg_handle),
             RPC_INT(parent_width), RPC_INT(parent_height),
             RPC_INT(x), RPC_INT(y),
             RPC_INT(width), RPC_INT(height));

   return vg_handle;
}

VG_API_CALL VGImage VG_API_ENTRY vgGetParent(
   VGImage vg_handle) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   if (!VG_GET_CLIENT_STATE(thread)) {
      return VG_INVALID_HANDLE;
   }

   return RPC_HANDLE_RES(RPC_CALL1_RES(vgGetParent_impl,
                                       thread,
                                       VGGETPARENT_ID,
                                       RPC_HANDLE(vg_handle)));
}

VG_API_CALL void VG_API_ENTRY vgCopyImage(
   VGImage dst_vg_handle, VGint dst_x, VGint dst_y,
   VGImage src_vg_handle, VGint src_x, VGint src_y,
   VGint width, VGint height,
   VGboolean dither) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   if (!VG_GET_CLIENT_STATE(thread)) {
      return;
   }

   RPC_CALL9(vgCopyImage_impl,
             thread,
             VGCOPYIMAGE_ID,
             RPC_HANDLE(dst_vg_handle), RPC_INT(dst_x), RPC_INT(dst_y),
             RPC_HANDLE(src_vg_handle), RPC_INT(src_x), RPC_INT(src_y),
             RPC_INT(width), RPC_INT(height),
             RPC_BOOLEAN(clean_boolean(dither)));
}

VG_API_CALL void VG_API_ENTRY vgDrawImage(
   VGImage vg_handle) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   sync_matrix(state, VG_MATRIX_IMAGE_USER_TO_SURFACE);
   sync_matrix(state, VG_MATRIX_FILL_PAINT_TO_USER);

   if (state->render_callback)
      state->render_callback();

   RPC_CALL1(vgDrawImage_impl,
             thread,
             VGDRAWIMAGE_ID,
             RPC_HANDLE(vg_handle));
}

/******************************************************************************
api framebuffer
******************************************************************************/

VG_API_CALL void VG_API_ENTRY vgSetPixels(
   VGint dst_x, VGint dst_y,
   VGImage src_vg_handle, VGint src_x, VGint src_y,
   VGint width, VGint height) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if (state->render_callback)
      state->render_callback();

   RPC_CALL7(vgSetPixels_impl,
             thread,
             VGSETPIXELS_ID,
             RPC_INT(dst_x), RPC_INT(dst_y),
             RPC_HANDLE(src_vg_handle), RPC_INT(src_x), RPC_INT(src_y),
             RPC_INT(width), RPC_INT(height));
}

VG_API_CALL void VG_API_ENTRY vgWritePixels(
   const void *data, VGint data_stride,
   VGImageFormat data_format,
   VGint dst_x, VGint dst_y,
   VGint width, VGint height) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   EGL_SURFACE_T *frame;
   VGint src_x = 0, src_y = 0;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if (!is_image_format(data_format)) {
      set_error(VG_UNSUPPORTED_IMAGE_FORMAT_ERROR);
      return;
   }

   if (!data || !is_aligned_image_format(data, data_format) ||
      ((height != 1) && !is_aligned_image_format((void *)(uintptr_t)data_stride, data_format)) ||
      (width <= 0) || (height <= 0)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   frame = CLIENT_GET_THREAD_STATE()->openvg.draw;

   khrn_clip_rect2(
      &dst_x, &dst_y, &src_x, &src_y, &width, &height,
      0, 0, frame->width, frame->height,
      0, 0, width, height);

   if (width <= 0 || height <= 0) {
      return;
   }
   
   data = (const VGubyte *)data + (src_y * data_stride);

   if (state->render_callback)
      state->render_callback();

   #ifdef RPC_DIRECT
      RPC_CALL8(vgWritePixels_impl, thread, no_id, 
         data, data_stride,
         data_format,
         src_x,
         dst_x, dst_y,
         width, height);
   #else
      {
         VGuint log2_bpp = get_log2_image_format_bpp(data_format);
         VGuint line_size;
         VGuint chunk_height_max;

         data = (const VGubyte *)data + ((src_x << log2_bpp) >> 3);
         src_x = ((src_x << log2_bpp) & 0x7) >> log2_bpp;

         line_size = (((src_x + width) << log2_bpp) + 0x7) >> 3;

         /*
            split into KHDISPATCH_WORKSPACE_SIZE chunks (or thereabouts)
         */

         chunk_height_max = (line_size == 0) ? height : (KHDISPATCH_WORKSPACE_SIZE / line_size);
         vcos_assert((height == 0) || (chunk_height_max != 0));

         while (height != 0) {
            VGint chunk_height = _min(height, chunk_height_max);

            uint32_t message[] = {
               VGWRITEPIXELS_ID,
               RPC_UINT(line_size),
               RPC_ENUM(data_format),
               RPC_INT(src_x),
               RPC_INT(dst_x), RPC_INT(dst_y),
               RPC_INT(width), RPC_INT(chunk_height) };

            rpc_begin(thread);
            rpc_send_ctrl_begin(thread, sizeof(message));
            rpc_send_ctrl_write(thread, message, sizeof(message));
            rpc_send_ctrl_end(thread);
            rpc_send_bulk_gather(thread, data, line_size, data_stride, chunk_height);
            data = (const VGubyte *)data + (chunk_height * data_stride);
            rpc_end(thread);

            height -= chunk_height;
            dst_y += chunk_height;
         }
      }
   #endif
}

VG_API_CALL void VG_API_ENTRY vgGetPixels(
   VGImage dst_vg_handle, VGint dst_x, VGint dst_y,
   VGint src_x, VGint src_y,
   VGint width, VGint height) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   if (!VG_GET_CLIENT_STATE(thread)) {
      return;
   }

   RPC_CALL7(vgGetPixels_impl,
             thread,
             VGGETPIXELS_ID,
             RPC_HANDLE(dst_vg_handle), RPC_INT(dst_x), RPC_INT(dst_y),
             RPC_INT(src_x), RPC_INT(src_y),
             RPC_INT(width), RPC_INT(height));
}

VG_API_CALL void VG_API_ENTRY vgReadPixels(
   void *data, VGint data_stride,
   VGImageFormat data_format,
   VGint src_x, VGint src_y,
   VGint width, VGint height) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   EGL_SURFACE_T *frame;
   VGint dst_x = 0, dst_y = 0;

   if (!VG_GET_CLIENT_STATE(thread)) {
      return;
   }

   if (!is_image_format(data_format)) {
      set_error(VG_UNSUPPORTED_IMAGE_FORMAT_ERROR);
      return;
   }

   if (!data || !is_aligned_image_format(data, data_format) ||
      ((height != 1) && !is_aligned_image_format((void *)(uintptr_t)data_stride, data_format)) ||
      (width <= 0) || (height <= 0)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   frame = CLIENT_GET_THREAD_STATE()->openvg.draw;

   khrn_clip_rect2(
      &dst_x, &dst_y, &src_x, &src_y, &width, &height,
      0, 0, width, height,
      0, 0, frame->width, frame->height);

   if (width <= 0 || height <= 0) {
      return;
   }
   
   data = (VGubyte *)data + (dst_y * data_stride);

   #ifdef RPC_DIRECT
      RPC_CALL8(vgReadPixels_impl, thread, no_id, 
         data, data_stride,
         data_format,
         dst_x,
         src_x, src_y,
         width, height);
   #else
      {
         VGuint log2_bpp = get_log2_image_format_bpp(data_format);
         VGuint line_size;
         VGuint first_mask;
         VGuint last_mask;
         VGuint chunk_height_max;

         data = (VGubyte *)data + ((dst_x << log2_bpp) >> 3);
         dst_x = ((dst_x << log2_bpp) & 0x7) >> log2_bpp;

         line_size = (((dst_x + width) << log2_bpp) + 0x7) >> 3;
         first_mask = (1 << (dst_x << log2_bpp)) - 1;
         last_mask = ~((2 << ((((dst_x + width) << log2_bpp) - 1) & 0x7)) - 1) & 0xff;

         /*
            split into KHDISPATCH_WORKSPACE_SIZE chunks (or thereabouts)
         */

         chunk_height_max = (line_size == 0) ? height : (KHDISPATCH_WORKSPACE_SIZE / line_size);
         vcos_assert((height == 0) || (chunk_height_max != 0));

         while (height != 0) {
            VGint chunk_height = _min(height, chunk_height_max);

            uint32_t message[] = {
               VGREADPIXELS_ID,
               RPC_UINT(line_size),
               RPC_ENUM(data_format),
               RPC_INT(dst_x),
               RPC_INT(src_x), RPC_INT(src_y),
               RPC_INT(width), RPC_INT(chunk_height) };

#ifndef __SYMBIAN32__
            rpc_begin(thread);
#endif
            rpc_send_ctrl_begin(thread, sizeof(message));
            rpc_send_ctrl_write(thread, message, sizeof(message));
            rpc_send_ctrl_end(thread);
            {
               uint32_t len[] = { line_size, data_stride, chunk_height, first_mask, last_mask };
               rpc_recv(thread, data, len, (RPC_RECV_FLAG_T)(RPC_RECV_FLAG_BULK | RPC_RECV_FLAG_BULK_SCATTER));
               data = (VGubyte *)data + (chunk_height * data_stride);
            }
#ifndef __SYMBIAN32__
            rpc_end(thread);
#endif
            height -= chunk_height;
            src_y += chunk_height;
         }
      }
   #endif
}

VG_API_CALL void VG_API_ENTRY vgCopyPixels(
   VGint dst_x, VGint dst_y,
   VGint src_x, VGint src_y,
   VGint width, VGint height) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if (state->render_callback)
      state->render_callback();

   RPC_CALL6(vgCopyPixels_impl,
             thread,
             VGCOPYPIXELS_ID,
             RPC_INT(dst_x), RPC_INT(dst_y),
             RPC_INT(src_x), RPC_INT(src_y),
             RPC_INT(width), RPC_INT(height));
}

/******************************************************************************
api fonts
******************************************************************************/

VG_API_CALL VGFont VG_API_ENTRY vgCreateFont(
   VGint glyphs_capacity) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VGHandle vg_handle;
   VG_CLIENT_FONT_T *font;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return VG_INVALID_HANDLE;
   }

   if (glyphs_capacity < 0) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return VG_INVALID_HANDLE;
   }

   vg_handle = get_stem(state);
   if (vg_handle == VG_INVALID_HANDLE) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      return VG_INVALID_HANDLE;
   }

   font = font_alloc();
   if (!font) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      destroy_stem(vg_handle);
      return VG_INVALID_HANDLE;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   if (!insert_object(state, vg_handle, font)) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      font_free(font);
      destroy_stem(vg_handle);
      return VG_INVALID_HANDLE;
   }
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL2(vgCreateFont_impl,
             thread,
             VGCREATEFONT_ID,
             RPC_HANDLE(vg_handle),
             RPC_INT(glyphs_capacity));

   return vg_handle;
}

VG_API_CALL void VG_API_ENTRY vgDestroyFont(
   VGFont vg_handle) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   delete_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_FONT);
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL1(vgDestroyFont_impl,
             thread,
             VGDESTROYFONT_ID,
             RPC_HANDLE(vg_handle));
}

VG_API_CALL void VG_API_ENTRY vgSetGlyphToPath(
   VGFont vg_handle,
   VGuint glyph_id,
   VGPath path_vg_handle,
   VGboolean is_hinted,
   VGfloat glyph_origin[2],
   VGfloat escapement[2]) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
#if EGL_BRCM_global_image && EGL_KHR_image
   VG_CLIENT_FONT_T *font;
#endif

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if (!glyph_origin || !is_aligned_float(glyph_origin) ||
      !escapement || !is_aligned_float(escapement)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

#if EGL_BRCM_global_image && EGL_KHR_image
   platform_mutex_acquire(&state->shared_state->mutex);
   font = (VG_CLIENT_FONT_T *)lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_FONT);
   if (font && ((path_vg_handle == VG_INVALID_HANDLE) || lookup_object(state, path_vg_handle, VG_CLIENT_OBJECT_TYPE_PATH))) {
      khrn_global_image_map_delete(&font->glyph_global_images, glyph_id);
   }
   platform_mutex_release(&state->shared_state->mutex);
#endif

   RPC_CALL8(vgSetGlyphToPath_impl,
             thread,
             VGSETGLYPHTOPATH_ID,
             RPC_HANDLE(vg_handle),
             RPC_UINT(glyph_id),
             RPC_HANDLE(path_vg_handle),
             RPC_BOOLEAN(clean_boolean(is_hinted)),
             RPC_FLOAT(clean_float(glyph_origin[0])), RPC_FLOAT(clean_float(glyph_origin[1])),
             RPC_FLOAT(clean_float(escapement[0])), RPC_FLOAT(clean_float(escapement[1])));
}

VG_API_CALL void VG_API_ENTRY vgSetGlyphToImage(
   VGFont vg_handle,
   VGuint glyph_id,
   VGImage image_vg_handle,
   VGfloat glyph_origin[2],
   VGfloat escapement[2]) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
#if EGL_BRCM_global_image && EGL_KHR_image
   VG_CLIENT_FONT_T *font;
   VG_CLIENT_IMAGE_T *image;
#endif

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if (!glyph_origin || !is_aligned_float(glyph_origin) ||
      !escapement || !is_aligned_float(escapement)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

#if EGL_BRCM_global_image && EGL_KHR_image
   platform_mutex_acquire(&state->shared_state->mutex);
   font = (VG_CLIENT_FONT_T *)lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_FONT);
   image = (image_vg_handle == VG_INVALID_HANDLE) ? NULL : (VG_CLIENT_IMAGE_T *)lookup_object(state, image_vg_handle, VG_CLIENT_OBJECT_TYPE_IMAGE);
   if (font && ((image_vg_handle == VG_INVALID_HANDLE) || image)) {
      if (image && (image->global_image_id[0] || image->global_image_id[1])) {
         if (!khrn_global_image_map_insert(&font->glyph_global_images, glyph_id,
            image->global_image_id[0] | ((uint64_t)image->global_image_id[1] << 32))) {
            set_error(VG_OUT_OF_MEMORY_ERROR);
            platform_mutex_release(&state->shared_state->mutex);
            return;
         }
      } else {
         khrn_global_image_map_delete(&font->glyph_global_images, glyph_id);
      }
   }
   platform_mutex_release(&state->shared_state->mutex);
#endif

   RPC_CALL7(vgSetGlyphToImage_impl,
             thread,
             VGSETGLYPHTOIMAGE_ID,
             RPC_HANDLE(vg_handle),
             RPC_UINT(glyph_id),
             RPC_HANDLE(image_vg_handle),
             RPC_FLOAT(clean_float(glyph_origin[0])), RPC_FLOAT(clean_float(glyph_origin[1])),
             RPC_FLOAT(clean_float(escapement[0])), RPC_FLOAT(clean_float(escapement[1])));
}

VG_API_CALL void VG_API_ENTRY vgClearGlyph(
   VGFont vg_handle,
   VGuint glyph_id) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
#if EGL_BRCM_global_image && EGL_KHR_image
   VG_CLIENT_FONT_T *font;
#endif

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

#if EGL_BRCM_global_image && EGL_KHR_image
   platform_mutex_acquire(&state->shared_state->mutex);
   font = (VG_CLIENT_FONT_T *)lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_FONT);
   if (font) {
      khrn_global_image_map_delete(&font->glyph_global_images, glyph_id);
   }
   platform_mutex_release(&state->shared_state->mutex);
#endif

   RPC_CALL2(vgClearGlyph_impl,
             thread,
             VGCLEARGLYPH_ID,
             RPC_HANDLE(vg_handle),
             RPC_UINT(glyph_id));
}

VG_API_CALL void VG_API_ENTRY vgDrawGlyph(
   VGFont vg_handle,
   VGuint glyph_id,
   VGbitfield paint_modes,
   VGboolean allow_autohinting) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state;

#ifdef VG_NO_STROKING
   paint_modes &= ~VG_STROKE_PATH;
   if (!paint_modes) { return; }
#endif

   state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   sync_matrix(state, VG_MATRIX_GLYPH_USER_TO_SURFACE);
   sync_matrix(state, VG_MATRIX_FILL_PAINT_TO_USER); /* image glyphs will use, so have to send even if paint_modes doesn't have VG_FILL_PATH */
   if (paint_modes & VG_STROKE_PATH) {
      sync_matrix(state, VG_MATRIX_STROKE_PAINT_TO_USER);
   }

   if (state->render_callback)
      state->render_callback();

   RPC_CALL4(vgDrawGlyph_impl,
             thread,
             VGDRAWGLYPH_ID,
             RPC_HANDLE(vg_handle),
             RPC_UINT(glyph_id),
             RPC_BITFIELD(paint_modes),
             RPC_BOOLEAN(clean_boolean(allow_autohinting)));
}

VG_API_CALL void VG_API_ENTRY vgDrawGlyphs(
   VGFont vg_handle,
   VGint glyphs_count,
   const VGuint *glyph_ids,
   const VGfloat *adjustments_x,
   const VGfloat *adjustments_y,
   VGbitfield paint_modes,
   VGboolean allow_autohinting) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state;

#ifdef VG_NO_STROKING
   paint_modes &= ~VG_STROKE_PATH;
   if (!paint_modes) { return; }
#endif

   state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return;
   }

   if ((glyphs_count <= 0) || !glyph_ids || !is_aligned_uint(glyph_ids) ||
      (adjustments_x && !is_aligned_float(adjustments_x)) ||
      (adjustments_y && !is_aligned_float(adjustments_y))) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   sync_matrix(state, VG_MATRIX_GLYPH_USER_TO_SURFACE);
   sync_matrix(state, VG_MATRIX_FILL_PAINT_TO_USER); /* image glyphs will use, so have to send even if paint_modes doesn't have VG_FILL_PATH */
   if (paint_modes & VG_STROKE_PATH) {
      sync_matrix(state, VG_MATRIX_STROKE_PAINT_TO_USER);
   }

   if (state->render_callback)
      state->render_callback();

   #ifdef RPC_DIRECT
      RPC_CALL7(vgDrawGlyphs_impl, thread, no_id, 
         vg_handle,
         glyphs_count, glyph_ids,
         adjustments_x, adjustments_y,
         paint_modes,
         clean_boolean(allow_autohinting));
   #else
      {
         /*
            split into multiple calls if necessary to avoid overflowing control buffer

            todo: if there's an invalid glyph, we're not supposed to draw anything
            at all. currently, this works on a per-chunk level (ie we will try to
            draw the glyphs in a chunk iff there are no invalid glyphs in the chunk)
         */

         VGuint glyph_size = sizeof(VGuint) +
            (adjustments_x ? sizeof(VGfloat) : 0) +
            (adjustments_y ? sizeof(VGfloat) : 0);

         while (glyphs_count != 0) {
            #define MESSAGE_SIZE 24

            /* at least 8 glyphs. this is a hack for the cts -- it checks that we don't draw anything if there's an invalid glyph (see above) */
            VGint chunk_glyphs_count = _min(glyphs_count, (rpc_send_ctrl_longest(thread, MESSAGE_SIZE + (8 * glyph_size)) - MESSAGE_SIZE) / glyph_size);

            uint32_t message[] = {
               VGDRAWGLYPHS_ID,
               RPC_HANDLE(vg_handle),
               RPC_INT(chunk_glyphs_count),
               RPC_BITFIELD((!!adjustments_x << 0) | (!!adjustments_y << 1)),
               RPC_BITFIELD(paint_modes),
               RPC_BOOLEAN(clean_boolean(allow_autohinting)) };
            vcos_static_assert(sizeof(message) == MESSAGE_SIZE);

            #undef MESSAGE_SIZE

            rpc_send_ctrl_begin(thread, sizeof(message) + (chunk_glyphs_count * glyph_size));
            rpc_send_ctrl_write(thread, message, sizeof(message));
            rpc_send_ctrl_write(thread, (uint32_t *)glyph_ids, chunk_glyphs_count * sizeof(VGuint));
            if (adjustments_x) { rpc_send_ctrl_write(thread, (uint32_t *)adjustments_x, chunk_glyphs_count * sizeof(VGfloat)); }
            if (adjustments_y) { rpc_send_ctrl_write(thread, (uint32_t *)adjustments_y, chunk_glyphs_count * sizeof(VGfloat)); }
            rpc_send_ctrl_end(thread);

            glyphs_count -= chunk_glyphs_count;
            glyph_ids += chunk_glyphs_count;
            if (adjustments_x) { adjustments_x += chunk_glyphs_count; }
            if (adjustments_y) { adjustments_y += chunk_glyphs_count; }
         }
      }
   #endif
}

/******************************************************************************
api filters
******************************************************************************/

VG_API_CALL void VG_API_ENTRY vgColorMatrix(
   VGImage dst_vg_handle, VGImage src_vg_handle,
   const VGfloat *matrix) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   if (!VG_GET_CLIENT_STATE(thread)) {
      return;
   }

   if (!matrix || !is_aligned_float(matrix)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   RPC_CALL3_IN_CTRL(vgColorMatrix_impl,
                     thread,
                     VGCOLORMATRIX_ID,
                     RPC_HANDLE(dst_vg_handle),
                     RPC_HANDLE(src_vg_handle),
                     matrix,
                     20 * sizeof(VGfloat));
}

VG_API_CALL void VG_API_ENTRY vgConvolve(
   VGImage dst_vg_handle, VGImage src_vg_handle,
   VGint kernel_width, VGint kernel_height,
   VGint shift_x, VGint shift_y,
   const VGshort *kernel,
   VGfloat scale, VGfloat bias,
   VGTilingMode tiling_mode) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   scale = clean_float(scale);
   bias = clean_float(bias);

   if (!VG_GET_CLIENT_STATE(thread)) {
      return;
   }

   if ((kernel_width <= 0) || (kernel_height <= 0) ||
      (kernel_width > VG_CONFIG_MAX_KERNEL_SIZE) || (kernel_height > VG_CONFIG_MAX_KERNEL_SIZE) ||
      !kernel || !is_aligned_short(kernel)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   RPC_CALL10_IN_CTRL(vgConvolve_impl,
                      thread,
                      VGCONVOLVE_ID,
                      RPC_HANDLE(dst_vg_handle), RPC_HANDLE(src_vg_handle),
                      RPC_INT(kernel_width), RPC_INT(kernel_height),
                      RPC_INT(shift_x), RPC_INT(shift_y),
                      RPC_FLOAT(scale), RPC_FLOAT(bias),
                      RPC_ENUM(tiling_mode),
                      kernel,
                      kernel_width * kernel_height * sizeof(VGshort));
}

VG_API_CALL void VG_API_ENTRY vgSeparableConvolve(
   VGImage dst_vg_handle, VGImage src_vg_handle,
   VGint kernel_width, VGint kernel_height,
   VGint shift_x, VGint shift_y,
   const VGshort *kernel_x,
   const VGshort *kernel_y,
   VGfloat scale, VGfloat bias,
   VGTilingMode tiling_mode) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   scale = clean_float(scale);
   bias = clean_float(bias);

   if (!VG_GET_CLIENT_STATE(thread)) {
      return;
   }

   if ((kernel_width <= 0) || (kernel_height <= 0) ||
      (kernel_width > VG_CONFIG_MAX_SEPARABLE_KERNEL_SIZE) || (kernel_height > VG_CONFIG_MAX_SEPARABLE_KERNEL_SIZE) ||
      !kernel_x || !is_aligned_short(kernel_x) || !kernel_y || !is_aligned_short(kernel_y)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   #ifdef RPC_DIRECT
      RPC_CALL11(vgSeparableConvolve_impl, thread, no_id, 
         dst_vg_handle, src_vg_handle,
         kernel_width, kernel_height,
         shift_x, shift_y,
         kernel_x, kernel_y,
         scale, bias,
         tiling_mode);
   #else
      {
         uint32_t message[] = {
            VGSEPARABLECONVOLVE_ID,
            RPC_HANDLE(dst_vg_handle), RPC_HANDLE(src_vg_handle),
            RPC_INT(kernel_width), RPC_INT(kernel_height),
            RPC_INT(shift_x), RPC_INT(shift_y),
            RPC_FLOAT(scale), RPC_FLOAT(bias),
            RPC_ENUM(tiling_mode) };

         VGuint kernel_x_size = kernel_width * sizeof(VGshort);
         VGuint kernel_y_size = kernel_height * sizeof(VGshort);

         rpc_send_ctrl_begin(thread, sizeof(message) + rpc_pad_ctrl(kernel_x_size) + rpc_pad_ctrl(kernel_y_size));
         rpc_send_ctrl_write(thread, message, sizeof(message));
         rpc_send_ctrl_write(thread, (uint32_t *)kernel_x, kernel_x_size);
         rpc_send_ctrl_write(thread, (uint32_t *)kernel_y, kernel_y_size);
         rpc_send_ctrl_end(thread);
      }
   #endif
}

VG_API_CALL void VG_API_ENTRY vgGaussianBlur(
   VGImage dst_vg_handle, VGImage src_vg_handle,
   VGfloat std_dev_x, VGfloat std_dev_y,
   VGTilingMode tiling_mode) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   std_dev_x = clean_float(std_dev_x);
   std_dev_y = clean_float(std_dev_y);

   if (!VG_GET_CLIENT_STATE(thread)) {
      return;
   }

   RPC_CALL5(vgGaussianBlur_impl,
             thread,
             VGGAUSSIANBLUR_ID,
             RPC_HANDLE(dst_vg_handle), RPC_HANDLE(src_vg_handle),
             RPC_FLOAT(std_dev_x), RPC_FLOAT(std_dev_y),
             RPC_ENUM(tiling_mode));
}

VG_API_CALL void VG_API_ENTRY vgLookup(
   VGImage dst_vg_handle, VGImage src_vg_handle,
   const VGubyte *red_lut,
   const VGubyte *green_lut,
   const VGubyte *blue_lut,
   const VGubyte *alpha_lut,
   VGboolean output_linear,
   VGboolean output_pre) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   if (!VG_GET_CLIENT_STATE(thread)) {
      return;
   }

   if (!red_lut || !green_lut || !blue_lut || !alpha_lut) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   #ifdef RPC_DIRECT
      RPC_CALL8(vgLookup_impl, thread, no_id, 
         dst_vg_handle, src_vg_handle,
         red_lut, green_lut, blue_lut, alpha_lut,
         clean_boolean(output_linear),
         clean_boolean(output_pre));
   #else
      {
         uint32_t message[] = {
            VGLOOKUP_ID,
            RPC_HANDLE(dst_vg_handle), RPC_HANDLE(src_vg_handle),
            RPC_BOOLEAN(clean_boolean(output_linear)),
            RPC_BOOLEAN(clean_boolean(output_pre)) };

         rpc_send_ctrl_begin(thread, sizeof(message) + 1024);
         rpc_send_ctrl_write(thread, message, sizeof(message));
         rpc_send_ctrl_write(thread, (uint32_t *)red_lut, 256);
         rpc_send_ctrl_write(thread, (uint32_t *)green_lut, 256);
         rpc_send_ctrl_write(thread, (uint32_t *)blue_lut, 256);
         rpc_send_ctrl_write(thread, (uint32_t *)alpha_lut, 256);
         rpc_send_ctrl_end(thread);
      }
   #endif
}

VG_API_CALL void VG_API_ENTRY vgLookupSingle(
   VGImage dst_vg_handle, VGImage src_vg_handle,
   const VGuint *lut,
   VGImageChannel source_channel,
   VGboolean output_linear,
   VGboolean output_pre) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   if (!VG_GET_CLIENT_STATE(thread)) {
      return;
   }

   if (!lut || !is_aligned_uint(lut)) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return;
   }

   RPC_CALL6_IN_CTRL(vgLookupSingle_impl,
                     thread,
                     VGLOOKUPSINGLE_ID,
                     RPC_HANDLE(dst_vg_handle), RPC_HANDLE(src_vg_handle),
                     RPC_ENUM(source_channel),
                     RPC_BOOLEAN(clean_boolean(output_linear)),
                     RPC_BOOLEAN(clean_boolean(output_pre)),
                     lut,
                     256 * sizeof(VGuint));
}

/******************************************************************************
api client-side stuff
******************************************************************************/

VG_API_CALL VGHardwareQueryResult VG_API_ENTRY vgHardwareQuery(
   VGHardwareQueryType hardware_query_type,
   VGint setting) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   if (!VG_GET_CLIENT_STATE(thread)) {
      return (VGHardwareQueryResult)0;
   }

   if (!is_hardware_query_type(hardware_query_type) ||
      ((hardware_query_type == VG_IMAGE_FORMAT_QUERY) && !is_image_format((VGImageFormat)setting)) ||
      ((hardware_query_type == VG_PATH_DATATYPE_QUERY) && !is_path_datatype((VGPathDatatype)setting))) {
      set_error(VG_ILLEGAL_ARGUMENT_ERROR);
      return (VGHardwareQueryResult)0;
   }

   return VG_HARDWARE_ACCELERATED;
}

VG_API_CALL const VGubyte * VG_API_ENTRY vgGetString(VGStringID name) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   if (!VG_GET_CLIENT_STATE(thread)) {
      return NULL;
   }

   switch (name) {
   case VG_VENDOR:     return (const VGubyte *)VG_CONFIG_VENDOR;
   case VG_RENDERER:   return (const VGubyte *)VG_CONFIG_RENDERER;
   case VG_VERSION:    return (const VGubyte *)"1.1";
   case VG_EXTENSIONS: return (const VGubyte *)VG_CONFIG_EXTENSIONS;
   default:            return NULL;
   }
}

/******************************************************************************
api vgu
******************************************************************************/

static VGUErrorCode get_vgu_error(void)
{
   switch (get_error()) {
   case VG_NO_ERROR:               return VGU_NO_ERROR;
   case VG_BAD_HANDLE_ERROR:       return VGU_BAD_HANDLE_ERROR;
   case VG_ILLEGAL_ARGUMENT_ERROR: return VGU_ILLEGAL_ARGUMENT_ERROR;
   case VG_OUT_OF_MEMORY_ERROR:    return VGU_OUT_OF_MEMORY_ERROR;
   case VG_PATH_CAPABILITY_ERROR:  return VGU_PATH_CAPABILITY_ERROR;
   default:                        UNREACHABLE(); return (VGUErrorCode)0;
   }
}

VGU_API_CALL VGUErrorCode VGU_API_ENTRY vguLine(
   VGPath vg_handle,
   VGfloat p0_x, VGfloat p0_y,
   VGfloat p1_x, VGfloat p1_y) VGU_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state;
   VG_CLIENT_PATH_T *path;

   p0_x = clean_float(p0_x);
   p0_y = clean_float(p0_y);
   p1_x = clean_float(p1_x);
   p1_y = clean_float(p1_y);

   state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return VGU_NO_ERROR;
   }

   clear_error();

   platform_mutex_acquire(&state->shared_state->mutex);
   path = (VG_CLIENT_PATH_T *)lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_PATH);
   if (path &&
      (path->caps & VG_PATH_CAPABILITY_APPEND_TO) &&
      need_path_segments(path->caps)) {
      VGubyte *segments;
      if (!khrn_vector_extend(&path->segments, 2)) {
         platform_mutex_release(&state->shared_state->mutex);
         return VGU_OUT_OF_MEMORY_ERROR;
      }
      segments = (VGubyte *)path->segments.data + (path->segments.size - 2);
      segments[0] = VG_MOVE_TO_ABS;
      segments[1] = VG_LINE_TO_ABS;
   }
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL5(vguLine_impl,
             thread,
             VGULINE_ID,
             RPC_HANDLE(vg_handle),
             RPC_FLOAT(p0_x), RPC_FLOAT(p0_y),
             RPC_FLOAT(p1_x), RPC_FLOAT(p1_y));

   return get_vgu_error();
}

VGU_API_CALL VGUErrorCode VGU_API_ENTRY vguPolygon(
   VGPath vg_handle,
   const VGfloat *ps, VGint ps_count,
   VGboolean close) VGU_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_PATH_T *path;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return VGU_NO_ERROR;
   }

   clear_error();

   if ((ps_count <= 0) || !ps || !is_aligned_float(ps)) {
      return VGU_ILLEGAL_ARGUMENT_ERROR;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   path = (VG_CLIENT_PATH_T *)lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_PATH);
   if (path &&
      (path->caps & VG_PATH_CAPABILITY_APPEND_TO) &&
      need_path_segments(path->caps)) {
      VGubyte *segments;
      VGuint segments_count = ps_count + (close ? 1 : 0);
      if (!khrn_vector_extend(&path->segments, segments_count)) {
         platform_mutex_release(&state->shared_state->mutex);
         return VGU_OUT_OF_MEMORY_ERROR;
      }
      segments = (VGubyte *)path->segments.data + (path->segments.size - segments_count);
      memset(segments, VG_LINE_TO_ABS, segments_count);
      segments[0] = VG_MOVE_TO_ABS;
      if (close) { segments[segments_count - 1] = VG_CLOSE_PATH; }
   }
   platform_mutex_release(&state->shared_state->mutex);

   #ifdef RPC_DIRECT
      RPC_CALL5(vguPolygon_impl, thread, no_id, 
         vg_handle,
         ps, ps_count,
         true, clean_boolean(close));
   #else
      {
         /*
            split into multiple calls if necessary to avoid overflowing control buffer
         */

         bool first = true;
         while (ps_count != 0) {
            #define MESSAGE_SIZE 20

            VGint chunk_ps_count = _min(ps_count, (rpc_send_ctrl_longest(thread, MESSAGE_SIZE + (2 * sizeof(VGfloat))) - MESSAGE_SIZE) / (2 * sizeof(VGfloat))); /* at least one p */
            bool last = chunk_ps_count == ps_count;

            uint32_t message[] = {
               VGUPOLYGON_ID,
               RPC_HANDLE(vg_handle),
               RPC_INT(chunk_ps_count),
               RPC_BOOLEAN(first), RPC_BOOLEAN(last && close) };
            vcos_static_assert(sizeof(message) == MESSAGE_SIZE);

            #undef MESSAGE_SIZE

            rpc_send_ctrl_begin(thread, 
               sizeof(message) +
               (chunk_ps_count * 2 * sizeof(VGfloat)));
            rpc_send_ctrl_write(thread, message, sizeof(message));
            rpc_send_ctrl_write(thread, (uint32_t *)ps, chunk_ps_count * 2 * sizeof(VGfloat));
            rpc_send_ctrl_end(thread);

            ps_count -= chunk_ps_count;
            ps += 2 * chunk_ps_count;
            first = false;
         }
      }
   #endif

   return get_vgu_error();
}

VGU_API_CALL VGUErrorCode VGU_API_ENTRY vguRect(
   VGPath vg_handle,
   VGfloat x, VGfloat y,
   VGfloat width, VGfloat height) VGU_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state;
   VG_CLIENT_PATH_T *path;

   x = clean_float(x);
   y = clean_float(y);
   width = clean_float(width);
   height = clean_float(height);

   state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return VGU_NO_ERROR;
   }

   clear_error();

   if (is_le_zero(width) || is_le_zero(height)) {
      return VGU_ILLEGAL_ARGUMENT_ERROR;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   path = (VG_CLIENT_PATH_T *)lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_PATH);
   if (path &&
      (path->caps & VG_PATH_CAPABILITY_APPEND_TO) &&
      need_path_segments(path->caps)) {
      VGubyte *segments;
      if (!khrn_vector_extend(&path->segments, 5)) {
         platform_mutex_release(&state->shared_state->mutex);
         return VGU_OUT_OF_MEMORY_ERROR;
      }
      segments = (VGubyte *)path->segments.data + (path->segments.size - 5);
      segments[0] = VG_MOVE_TO_ABS;
      segments[1] = VG_HLINE_TO_REL;
      segments[2] = VG_VLINE_TO_REL;
      segments[3] = VG_HLINE_TO_REL;
      segments[4] = VG_CLOSE_PATH;
   }
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL5(vguRect_impl,
             thread,
             VGURECT_ID,
             RPC_HANDLE(vg_handle),
             RPC_FLOAT(x), RPC_FLOAT(y),
             RPC_FLOAT(width), RPC_FLOAT(height));

   return get_vgu_error();
}

VGU_API_CALL VGUErrorCode VGU_API_ENTRY vguRoundRect(
   VGPath vg_handle,
   VGfloat x, VGfloat y,
   VGfloat width, VGfloat height,
   VGfloat arc_width, VGfloat arc_height) VGU_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state;
   VG_CLIENT_PATH_T *path;

   x = clean_float(x);
   y = clean_float(y);
   width = clean_float(width);
   height = clean_float(height);
   arc_width = clean_float(arc_width);
   arc_height = clean_float(arc_height);

   state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return VGU_NO_ERROR;
   }

   clear_error();

   if (is_le_zero(width) || is_le_zero(height)) {
      return VGU_ILLEGAL_ARGUMENT_ERROR;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   path = (VG_CLIENT_PATH_T *)lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_PATH);
   if (path &&
      (path->caps & VG_PATH_CAPABILITY_APPEND_TO) &&
      need_path_segments(path->caps)) {
      VGubyte *segments;
      if (!khrn_vector_extend(&path->segments, 10)) {
         platform_mutex_release(&state->shared_state->mutex);
         return VGU_OUT_OF_MEMORY_ERROR;
      }
      segments = (VGubyte *)path->segments.data + (path->segments.size - 10);
      segments[0] = VG_MOVE_TO_ABS;
      segments[1] = VG_HLINE_TO_REL;
      segments[2] = VG_SCCWARC_TO_REL;
      segments[3] = VG_VLINE_TO_REL;
      segments[4] = VG_SCCWARC_TO_REL;
      segments[5] = VG_HLINE_TO_REL;
      segments[6] = VG_SCCWARC_TO_REL;
      segments[7] = VG_VLINE_TO_REL;
      segments[8] = VG_SCCWARC_TO_REL;
      segments[9] = VG_CLOSE_PATH;
   }
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL7(vguRoundRect_impl,
             thread,
             VGUROUNDRECT_ID,
             RPC_HANDLE(vg_handle),
             RPC_FLOAT(x), RPC_FLOAT(y),
             RPC_FLOAT(width), RPC_FLOAT(height),
             RPC_FLOAT(arc_width), RPC_FLOAT(arc_height));

   return get_vgu_error();
}

VGU_API_CALL VGUErrorCode VGU_API_ENTRY vguEllipse(
   VGPath vg_handle,
   VGfloat x, VGfloat y,
   VGfloat width, VGfloat height) VGU_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state;
   VG_CLIENT_PATH_T *path;

   x = clean_float(x);
   y = clean_float(y);
   width = clean_float(width);
   height = clean_float(height);

   state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return VGU_NO_ERROR;
   }

   clear_error();

   if (is_le_zero(width) || is_le_zero(height)) {
      return VGU_ILLEGAL_ARGUMENT_ERROR;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   path = (VG_CLIENT_PATH_T *)lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_PATH);
   if (path &&
      (path->caps & VG_PATH_CAPABILITY_APPEND_TO) &&
      need_path_segments(path->caps)) {
      VGubyte *segments;
      if (!khrn_vector_extend(&path->segments, 4)) {
         platform_mutex_release(&state->shared_state->mutex);
         return VGU_OUT_OF_MEMORY_ERROR;
      }
      segments = (VGubyte *)path->segments.data + (path->segments.size - 4);
      segments[0] = VG_MOVE_TO_ABS;
      segments[1] = VG_SCCWARC_TO_REL;
      segments[2] = VG_SCCWARC_TO_REL;
      segments[3] = VG_CLOSE_PATH;
   }
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL5(vguEllipse_impl,
             thread,
             VGUELLIPSE_ID,
             RPC_HANDLE(vg_handle),
             RPC_FLOAT(x), RPC_FLOAT(y),
             RPC_FLOAT(width), RPC_FLOAT(height));

   return get_vgu_error();
}

VGU_API_CALL VGUErrorCode VGU_API_ENTRY vguArc(
   VGPath vg_handle,
   VGfloat x, VGfloat y,
   VGfloat width, VGfloat height,
   VGfloat start_angle, VGfloat angle_extent,
   VGUArcType arc_type) VGU_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_CLIENT_STATE_T *state;
   VGuint angle_o180;
   VG_CLIENT_PATH_T *path;

   x = clean_float(x);
   y = clean_float(y);
   width = clean_float(width);
   height = clean_float(height);
   start_angle = clean_float(start_angle);
   angle_extent = clean_float(angle_extent);

   state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return VGU_NO_ERROR;
   }

   clear_error();

   if (is_le_zero(width) || is_le_zero(height) || !is_arc_type(arc_type)) {
      return VGU_ILLEGAL_ARGUMENT_ERROR;
   }

   angle_o180 = float_to_int_zero(absf_(angle_extent) * (1.0f / 180.0f));

   platform_mutex_acquire(&state->shared_state->mutex);
   path = (VG_CLIENT_PATH_T *)lookup_object(state, vg_handle, VG_CLIENT_OBJECT_TYPE_PATH);
   if (path &&
      (path->caps & VG_PATH_CAPABILITY_APPEND_TO) &&
      need_path_segments(path->caps)) {
      VGubyte *segments;
      VGuint segments_count = 2 + angle_o180;
      switch (arc_type) {
      case VGU_ARC_OPEN:  break;
      case VGU_ARC_CHORD: segments_count += 1; break;
      case VGU_ARC_PIE:   segments_count += 2; break;
      default:            UNREACHABLE();
      }
      if (!khrn_vector_extend(&path->segments, segments_count)) {
         platform_mutex_release(&state->shared_state->mutex);
         return VGU_OUT_OF_MEMORY_ERROR;
      }
      segments = (VGubyte *)path->segments.data + (path->segments.size - segments_count);
      segments[0] = VG_MOVE_TO_ABS;
      memset(segments + 1, VG_SCCWARC_TO_ABS, segments_count - 1); /* don't care about actual arc type on client */
      if (arc_type == VGU_ARC_PIE) { segments[segments_count - 2] = VG_LINE_TO_ABS; }
      if ((arc_type == VGU_ARC_CHORD) || (arc_type == VGU_ARC_PIE)) { segments[segments_count - 1] = VG_CLOSE_PATH; }
   }
   platform_mutex_release(&state->shared_state->mutex);

   RPC_CALL9(vguArc_impl,
             thread,
             VGUARC_ID,
             RPC_HANDLE(vg_handle),
             RPC_FLOAT(x), RPC_FLOAT(y),
             RPC_FLOAT(width), RPC_FLOAT(height),
             RPC_FLOAT(start_angle), RPC_FLOAT(angle_extent), RPC_UINT(angle_o180),
             RPC_ENUM(arc_type));

   return get_vgu_error();
}

/*
   based on "Fundamentals of Texture Mapping and Image Warping" by Paul S Heckbert
*/

static bool warp_square_to_quad(VG_MAT3X3_T *a,
   VGfloat p0_x, VGfloat p0_y,
   VGfloat p1_x, VGfloat p1_y,
   VGfloat p2_x, VGfloat p2_y,
   VGfloat p3_x, VGfloat p3_y)
{
   VGfloat d1_x = p1_x - p3_x;
   VGfloat d1_y = p1_y - p3_y;
   VGfloat d2_x = p2_x - p3_x;
   VGfloat d2_y = p2_y - p3_y;

   VGfloat d = (d1_x * d2_y) - (d1_y * d2_x);

   VGfloat oo_d;
   VGfloat sum_x;
   VGfloat sum_y;
   VGfloat g;
   VGfloat h;

   if (absf_(d) < EPS) {
      return false;
   }
   oo_d = recip_(d);

   sum_x = (p0_x + p3_x) - (p1_x + p2_x);
   sum_y = (p0_y + p3_y) - (p1_y + p2_y);

   g = ((sum_x * d2_y) - (sum_y * d2_x)) * oo_d;
   h = ((d1_x * sum_y) - (d1_y * sum_x)) * oo_d;

   a->m[0][0] = (p1_x - p0_x) + (g * p1_x);
   a->m[0][1] = (p2_x - p0_x) + (h * p2_x);
   a->m[0][2] = p0_x;

   a->m[1][0] = (p1_y - p0_y) + (g * p1_y);
   a->m[1][1] = (p2_y - p0_y) + (h * p2_y);
   a->m[1][2] = p0_y;

   a->m[2][0] = g;
   a->m[2][1] = h;
   a->m[2][2] = 1.0f;

   return true;
}

static bool warp_quad_to_square(VG_MAT3X3_T *a,
   VGfloat p0_x, VGfloat p0_y,
   VGfloat p1_x, VGfloat p1_y,
   VGfloat p2_x, VGfloat p2_y,
   VGfloat p3_x, VGfloat p3_y)
{
   if (!warp_square_to_quad(a, p0_x, p0_y, p1_x, p1_y, p2_x, p2_y, p3_x, p3_y) ||
      !vg_mat3x3_is_invertible(a)) {
      return false;
   }
   vg_mat3x3_invert(a);
   return true;
}

VGU_API_CALL VGUErrorCode VGU_API_ENTRY vguComputeWarpQuadToSquare(
   VGfloat p0_x, VGfloat p0_y,
   VGfloat p1_x, VGfloat p1_y,
   VGfloat p2_x, VGfloat p2_y,
   VGfloat p3_x, VGfloat p3_y,
   VGfloat *matrix) VGU_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_MAT3X3_T a;

   p0_x = clean_float(p0_x);
   p0_y = clean_float(p0_y);
   p1_x = clean_float(p1_x);
   p1_y = clean_float(p1_y);
   p2_x = clean_float(p2_x);
   p2_y = clean_float(p2_y);
   p3_x = clean_float(p3_x);
   p3_y = clean_float(p3_y);

   if (!VG_GET_CLIENT_STATE(thread)) {
      return VGU_NO_ERROR;
   }

   if (!matrix || !is_aligned_float(matrix)) {
      return VGU_ILLEGAL_ARGUMENT_ERROR;
   }

   if (!warp_quad_to_square(&a, p0_x, p0_y, p1_x, p1_y, p2_x, p2_y, p3_x, p3_y)) {
      return VGU_BAD_WARP_ERROR;
   }
   vg_mat3x3_get(&a, matrix);
   return VGU_NO_ERROR;
}

VGU_API_CALL VGUErrorCode VGU_API_ENTRY vguComputeWarpSquareToQuad(
   VGfloat p0_x, VGfloat p0_y,
   VGfloat p1_x, VGfloat p1_y,
   VGfloat p2_x, VGfloat p2_y,
   VGfloat p3_x, VGfloat p3_y,
   VGfloat *matrix) VGU_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_MAT3X3_T a;

   p0_x = clean_float(p0_x);
   p0_y = clean_float(p0_y);
   p1_x = clean_float(p1_x);
   p1_y = clean_float(p1_y);
   p2_x = clean_float(p2_x);
   p2_y = clean_float(p2_y);
   p3_x = clean_float(p3_x);
   p3_y = clean_float(p3_y);

   if (!VG_GET_CLIENT_STATE(thread)) {
      return VGU_NO_ERROR;
   }

   if (!matrix || !is_aligned_float(matrix)) {
      return VGU_ILLEGAL_ARGUMENT_ERROR;
   }

   if (!warp_square_to_quad(&a, p0_x, p0_y, p1_x, p1_y, p2_x, p2_y, p3_x, p3_y)) {
      return VGU_BAD_WARP_ERROR;
   }
   vg_mat3x3_get(&a, matrix);
   return VGU_NO_ERROR;
}

VGU_API_CALL VGUErrorCode VGU_API_ENTRY vguComputeWarpQuadToQuad(
   VGfloat p0_x, VGfloat p0_y,
   VGfloat p1_x, VGfloat p1_y,
   VGfloat p2_x, VGfloat p2_y,
   VGfloat p3_x, VGfloat p3_y,
   VGfloat q0_x, VGfloat q0_y,
   VGfloat q1_x, VGfloat q1_y,
   VGfloat q2_x, VGfloat q2_y,
   VGfloat q3_x, VGfloat q3_y,
   VGfloat *matrix) VGU_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
   VG_MAT3X3_T a, b;

   p0_x = clean_float(p0_x);
   p0_y = clean_float(p0_y);
   p1_x = clean_float(p1_x);
   p1_y = clean_float(p1_y);
   p2_x = clean_float(p2_x);
   p2_y = clean_float(p2_y);
   p3_x = clean_float(p3_x);
   p3_y = clean_float(p3_y);
   q0_x = clean_float(q0_x);
   q0_y = clean_float(q0_y);
   q1_x = clean_float(q1_x);
   q1_y = clean_float(q1_y);
   q2_x = clean_float(q2_x);
   q2_y = clean_float(q2_y);
   q3_x = clean_float(q3_x);
   q3_y = clean_float(q3_y);

   if (!VG_GET_CLIENT_STATE(thread)) {
      return VGU_NO_ERROR;
   }

   if (!matrix || !is_aligned_float(matrix)) {
      return VGU_ILLEGAL_ARGUMENT_ERROR;
   }

   if (!warp_square_to_quad(&a, p0_x, p0_y, p1_x, p1_y, p2_x, p2_y, p3_x, p3_y) ||
      !warp_quad_to_square(&b, q0_x, q0_y, q1_x, q1_y, q2_x, q2_y, q3_x, q3_y)) {
      return VGU_BAD_WARP_ERROR;
   }
   vg_mat3x3_postmul(&a, &b);
   vg_mat3x3_get(&a, matrix);
   return VGU_NO_ERROR;
}

/******************************************************************************
VG_KHR_EGL_image
******************************************************************************/

#if VG_KHR_EGL_image

VG_API_CALL VGImage VG_API_ENTRY vgCreateEGLImageTargetKHR(
   VGeglImageKHR src_egl_handle) VG_API_EXIT
{
   CLIENT_THREAD_STATE_T *thread = CLIENT_GET_THREAD_STATE();
#if EGL_BRCM_global_image
   VGuint global_image_id[2];
#endif
   VGuint format_width_height[3];
   VGImage vg_handle;
   VG_CLIENT_IMAGE_T *image;

   VG_CLIENT_STATE_T *state = VG_GET_CLIENT_STATE(thread);
   if (!state) {
      return VG_INVALID_HANDLE;
   }

#if EGL_BRCM_global_image
   if ((uintptr_t)src_egl_handle & (1 << 31)) {
      CLIENT_PROCESS_STATE_T *process = CLIENT_GET_PROCESS_STATE();
      uint64_t id;

      CLIENT_LOCK();
      id = process->inited ? khrn_global_image_map_lookup(&process->global_image_egl_images, (uint32_t)(uintptr_t)src_egl_handle) : 0;
      CLIENT_UNLOCK();
      if (!id) {
         set_error(VG_ILLEGAL_ARGUMENT_ERROR);
         return VG_INVALID_HANDLE;
      }
      global_image_id[0] = (VGuint)id;
      global_image_id[1] = (VGuint)(id >> 32);

      platform_get_global_image_info(global_image_id[0], global_image_id[1],
         format_width_height + 0, format_width_height + 1, format_width_height + 2);

      if (!(format_width_height[0] & EGL_PIXEL_FORMAT_VG_IMAGE_BRCM) ||
         (format_width_height[1] == 0) || (format_width_height[2] == 0) ||
         (format_width_height[1] > VG_CONFIG_MAX_IMAGE_WIDTH) || (format_width_height[2] > VG_CONFIG_MAX_IMAGE_HEIGHT)) {
         set_error(VG_UNSUPPORTED_IMAGE_FORMAT_ERROR);
         return VG_INVALID_HANDLE;
      }

      switch (format_width_height[0] & ~EGL_PIXEL_FORMAT_USAGE_MASK_BRCM) {
      case EGL_PIXEL_FORMAT_ARGB_8888_PRE_BRCM: format_width_height[0] = VG_sARGB_8888_PRE; break;
      case EGL_PIXEL_FORMAT_ARGB_8888_BRCM:     format_width_height[0] = VG_sARGB_8888; break;
      case EGL_PIXEL_FORMAT_XRGB_8888_BRCM:     format_width_height[0] = VG_sXRGB_8888; break;
      case EGL_PIXEL_FORMAT_RGB_565_BRCM:       format_width_height[0] = VG_sRGB_565; break;
      case EGL_PIXEL_FORMAT_A_8_BRCM:           format_width_height[0] = VG_A_8; break;
      default:                                  UNREACHABLE();
      }

      vg_handle = get_stem(state);
      if (vg_handle == VG_INVALID_HANDLE) {
         set_error(VG_OUT_OF_MEMORY_ERROR);
         return VG_INVALID_HANDLE;
      }

      RPC_CALL3(vgCreateImageFromGlobalImage_impl,
                thread,
                VGCREATEIMAGEFROMGLOBALIMAGE_ID,
                RPC_HANDLE(vg_handle),
                RPC_UINT(global_image_id[0]),
                RPC_UINT(global_image_id[1]));
   } else {
      global_image_id[0] = 0;
      global_image_id[1] = 0;
#endif
      vg_handle = RPC_HANDLE_RES(RPC_CALL2_OUT_CTRL_RES(vgCreateEGLImageTargetKHR_impl,
                                                        thread,
                                                        VGCREATEEGLIMAGETARGETKHR_ID,
                                                        RPC_EGLID(src_egl_handle),
                                                        format_width_height));
      if (vg_handle == VG_INVALID_HANDLE) {
         return VG_INVALID_HANDLE;
      }
#if EGL_BRCM_global_image
   }
#endif

   image = image_alloc((VGImageFormat)format_width_height[0], format_width_height[1], format_width_height[2]
#if EGL_BRCM_global_image
      , global_image_id[0], global_image_id[1]
#endif
      );
   if (!image) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      RPC_CALL1(vgDestroyImage_impl,
                thread,
                VGDESTROYIMAGE_ID,
                RPC_HANDLE(vg_handle));
      return VG_INVALID_HANDLE;
   }

   platform_mutex_acquire(&state->shared_state->mutex);
   if (!insert_object(state, vg_handle, image)) {
      set_error(VG_OUT_OF_MEMORY_ERROR);
      platform_mutex_release(&state->shared_state->mutex);
      image_free(image);
      RPC_CALL1(vgDestroyImage_impl,
                thread,
                VGDESTROYIMAGE_ID,
                RPC_HANDLE(vg_handle));
      return VG_INVALID_HANDLE;
   }
   platform_mutex_release(&state->shared_state->mutex);

   return vg_handle;
}

#endif
