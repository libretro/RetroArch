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

#if defined(KHRN_IMPL_STRUCT)
#define FN(type, name, args) type (*name) args;
#elif defined(KHRN_IMPL_STRUCT_INIT)
#define FN(type, name, args) name,
#else
#define FN(type, name, args) extern type name args;
#endif

FN(void, vgClearError_impl, (void))
FN(void, vgSetError_impl, (VGErrorCode error))
FN(VGErrorCode, vgGetError_impl, (void))
FN(void, vgFlush_impl, (void))
FN(VGuint, vgFinish_impl, (void))
FN(VGuint, vgCreateStems_impl, (VGuint count, VGHandle *vg_handles))
FN(void, vgDestroyStem_impl, (VGHandle vg_handle))
FN(void, vgSetiv_impl, (VGParamType param_type, VGint count, const VGint *values))
FN(void, vgSetfv_impl, (VGParamType param_type, VGint count, const VGfloat *values))
FN(void, vgGetfv_impl, (VGParamType param_type, VGint count, VGfloat *values))
FN(void, vgSetParameteriv_impl, (VGHandle vg_handle, VG_CLIENT_OBJECT_TYPE_T client_object_type, VGint param_type, VGint count, const VGint *values))
FN(void, vgSetParameterfv_impl, (VGHandle vg_handle, VG_CLIENT_OBJECT_TYPE_T client_object_type, VGint param_type, VGint count, const VGfloat *values))
FN(bool, vgGetParameteriv_impl, (VGHandle vg_handle, VG_CLIENT_OBJECT_TYPE_T client_object_type, VGint param_type, VGint count, VGint *values))
FN(void, vgLoadMatrix_impl, (VGMatrixMode matrix_mode, const VG_MAT3X3_T *matrix))
FN(void, vgMask_impl, (VGImage vg_handle, VGMaskOperation operation, VGint dst_x, VGint dst_y, VGint width, VGint height))
FN(void, vgRenderToMask_impl, (VGPath vg_handle, VGbitfield paint_modes, VGMaskOperation operation))
FN(void, vgCreateMaskLayer_impl, (VGHandle vg_handle, VGint width, VGint height))
FN(void, vgDestroyMaskLayer_impl, (VGMaskLayer vg_handle))
FN(void, vgFillMaskLayer_impl, (VGMaskLayer vg_handle, VGint x, VGint y, VGint width, VGint height, VGfloat value))
FN(void, vgCopyMask_impl, (VGMaskLayer dst_vg_handle, VGint dst_x, VGint dst_y, VGint src_x, VGint src_y, VGint width, VGint height))
FN(void, vgClear_impl, (VGint x, VGint y, VGint width, VGint height))
FN(void, vgCreatePath_impl, (VGHandle vg_handle, VGint format, VGPathDatatype datatype, VGfloat scale, VGfloat bias, VGint segments_capacity, VGint coords_capacity, VGbitfield caps))
FN(void, vgClearPath_impl, (VGPath vg_handle, VGbitfield caps))
FN(void, vgDestroyPath_impl, (VGPath vg_handle))
FN(void, vgRemovePathCapabilities_impl, (VGPath vg_handle, VGbitfield caps))
FN(void, vgAppendPath_impl, (VGPath dst_vg_handle, VGPath src_vg_handle))
FN(void, vgAppendPathData_impl, (VGPath vg_handle, VGPathDatatype datatype, VGint segments_count, const VGubyte *segments, VGuint coords_size, const void *coords))
FN(void, vgModifyPathCoords_impl, (VGPath vg_handle, VGPathDatatype datatype, VGuint coords_offset, VGuint coords_size, const void *coords))
FN(void, vgTransformPath_impl, (VGPath dst_vg_handle, VGPath src_vg_handle))
FN(void, vgInterpolatePath_impl, (VGPath dst_vg_handle, VGPath begin_vg_handle, VGPath end_vg_handle, VGfloat t))
FN(VGfloat, vgPathLength_impl, (VGPath vg_handle, VGint segments_i, VGint segments_count))
FN(bool, vgPointAlongPath_impl, (VGPath vg_handle, VGint segments_i, VGint segments_count, VGfloat distance, VGbitfield mask, VGfloat *values))
FN(bool, vgPathBounds_impl, (VGPath vg_handle, VGfloat *values))
FN(bool, vgPathTransformedBounds_impl, (VGPath vg_handle, VGfloat *values))
FN(void, vgDrawPath_impl, (VGPath vg_handle, VGbitfield paint_modes))
FN(void, vgCreatePaint_impl, (VGHandle vg_handle))
FN(void, vgDestroyPaint_impl, (VGPaint vg_handle))
FN(void, vgSetPaint_impl, (VGPaint vg_handle, VGbitfield paint_modes))
FN(void, vgPaintPattern_impl, (VGPaint vg_handle, VGImage pattern_vg_handle))
FN(void, vgCreateImage_impl, (VGHandle vg_handle, VGImageFormat format, VGint width, VGint height, VGbitfield allowed_quality))
FN(void, vgDestroyImage_impl, (VGImage vg_handle))
FN(void, vgClearImage_impl, (VGImage vg_handle, VGint x, VGint y, VGint width, VGint height))
FN(void, vgImageSubData_impl, (VGImage vg_handle, VGint dst_width, VGint dst_height, const void *data, VGint data_stride, VGImageFormat data_format, VGint src_x, VGint dst_x, VGint dst_y, VGint width, VGint height))
FN(bool, vgGetImageSubData_impl, (VGImage vg_handle, VGint src_width, VGint src_height, void *data, VGint data_stride, VGImageFormat data_format, VGint dst_x, VGint src_x, VGint src_y, VGint width, VGint height))
FN(void, vgChildImage_impl, (VGHandle vg_handle, VGImage parent_vg_handle, VGint parent_width, VGint parent_height, VGint x, VGint y, VGint width, VGint height))
FN(VGImage, vgGetParent_impl, (VGImage vg_handle))
FN(void, vgCopyImage_impl, (VGImage dst_vg_handle, VGint dst_x, VGint dst_y, VGImage src_vg_handle, VGint src_x, VGint src_y, VGint width, VGint height, bool dither))
FN(void, vgDrawImage_impl, (VGImage vg_handle))
FN(void, vgSetPixels_impl, (VGint dst_x, VGint dst_y, VGImage src_vg_handle, VGint src_x, VGint src_y, VGint width, VGint height))
FN(void, vgWritePixels_impl, (const void *data, VGint data_stride, VGImageFormat data_format, VGint src_x, VGint dst_x, VGint dst_y, VGint width, VGint height))
FN(void, vgGetPixels_impl, (VGImage dst_vg_handle, VGint dst_x, VGint dst_y, VGint src_x, VGint src_y, VGint width, VGint height))
FN(void, vgReadPixels_impl, (void *data, VGint data_stride, VGImageFormat data_format, VGint dst_x, VGint src_x, VGint src_y, VGint width, VGint height))
FN(void, vgCopyPixels_impl, (VGint dst_x, VGint dst_y, VGint src_x, VGint src_y, VGint width, VGint height))
FN(void, vgCreateFont_impl, (VGHandle vg_handle, VGint glyphs_capacity))
FN(void, vgDestroyFont_impl, (VGFont vg_handle))
FN(void, vgSetGlyphToPath_impl, (VGFont vg_handle, VGuint glyph_id, VGPath path_vg_handle, bool is_hinted, VGfloat glyph_origin_x, VGfloat glyph_origin_y, VGfloat escapement_x, VGfloat escapement_y))
FN(void, vgSetGlyphToImage_impl, (VGFont vg_handle, VGuint glyph_id, VGImage image_vg_handle, VGfloat glyph_origin_x, VGfloat glyph_origin_y, VGfloat escapement_x, VGfloat escapement_y))
FN(void, vgClearGlyph_impl, (VGFont vg_handle, VGuint glyph_id))
FN(void, vgDrawGlyph_impl, (VGFont vg_handle, VGuint glyph_id, VGbitfield paint_modes, bool allow_autohinting))
FN(void, vgDrawGlyphs_impl, (VGFont vg_handle, VGint glyphs_count, const VGuint *glyph_ids, const VGfloat *adjustments_x, const VGfloat *adjustments_y, VGbitfield paint_modes, bool allow_autohinting))
FN(void, vgColorMatrix_impl, (VGImage dst_vg_handle, VGImage src_vg_handle, const VGfloat *matrix))
FN(void, vgConvolve_impl, (VGImage dst_vg_handle, VGImage src_vg_handle, VGint kernel_width, VGint kernel_height, VGint shift_x, VGint shift_y, VGfloat scale, VGfloat bias, VGTilingMode tiling_mode, const VGshort *kernel))
FN(void, vgSeparableConvolve_impl, (VGImage dst_vg_handle, VGImage src_vg_handle, VGint kernel_width, VGint kernel_height, VGint shift_x, VGint shift_y, const VGshort *kernel_x, const VGshort *kernel_y, VGfloat scale, VGfloat bias, VGTilingMode tiling_mode))
FN(void, vgGaussianBlur_impl, (VGImage dst_vg_handle, VGImage src_vg_handle, VGfloat std_dev_x, VGfloat std_dev_y, VGTilingMode tiling_mode))
FN(void, vgLookup_impl, (VGImage dst_vg_handle, VGImage src_vg_handle, const VGubyte *red_lut, const VGubyte *green_lut, const VGubyte *blue_lut, const VGubyte *alpha_lut, bool output_linear, bool output_pre))
FN(void, vgLookupSingle_impl, (VGImage dst_vg_handle, VGImage src_vg_handle, VGImageChannel source_channel, bool output_linear, bool output_pre, const VGuint *lut))
FN(void, vguLine_impl, (VGPath vg_handle, VGfloat p0_x, VGfloat p0_y, VGfloat p1_x, VGfloat p1_y))
FN(void, vguPolygon_impl, (VGPath vg_handle, const VGfloat *ps, VGint ps_count, bool first, bool close))
FN(void, vguRect_impl, (VGPath vg_handle, VGfloat x, VGfloat y, VGfloat width, VGfloat height))
FN(void, vguRoundRect_impl, (VGPath vg_handle, VGfloat x, VGfloat y, VGfloat width, VGfloat height, VGfloat arc_width, VGfloat arc_height))
FN(void, vguEllipse_impl, (VGPath vg_handle, VGfloat x, VGfloat y, VGfloat width, VGfloat height))
FN(void, vguArc_impl, (VGPath vg_handle, VGfloat x, VGfloat y, VGfloat width, VGfloat height, VGfloat start_angle, VGfloat angle_extent, VGuint angle_o180, VGUArcType arc_type))
#if VG_KHR_EGL_image
FN(VGImage, vgCreateEGLImageTargetKHR_impl, (VGeglImageKHR src_egl_handle, VGuint *format_width_height))
#if EGL_BRCM_global_image
FN(void, vgCreateImageFromGlobalImage_impl, (VGHandle vg_handle, VGuint id_0, VGuint id_1))
#endif
#endif

#undef FN
