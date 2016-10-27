/****************************************************************************
 * Copyright (C) 2015
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#ifndef __GX2_FUNCTIONS_H_
#define __GX2_FUNCTIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "gx2_types.h"

void InitGX2FunctionPointers(void);

extern void (* GX2Init)(u32 * init_attribs);
extern void (* GX2Shutdown)(void);
extern void (* GX2Flush)(void);
extern s32 (* GX2GetMainCoreId)(void) ;
extern s32 (* GX2DrawDone)(void);
extern void (* GX2ClearColor)(GX2ColorBuffer *colorBuffer, f32 r, f32 g, f32 b, f32 a);
extern void (* GX2SetViewport)(f32 x, f32 y, f32 w, f32 h, f32 nearZ, f32 farZ);
extern void (* GX2SetScissor)(u32 x_orig, u32 y_orig, u32 wd, u32 ht);
extern void (* GX2SetContextState)(const GX2ContextState* state);
extern void (* GX2DrawEx)(s32 primitive_type, u32 count, u32 first_vertex, u32 instances_count);
extern void (* GX2DrawIndexedEx)(s32 primitive_type, u32 count, s32 index_format, const void* idx, u32 first_vertex, u32 instances_count);
extern void (* GX2ClearDepthStencilEx)(GX2DepthBuffer *depthBuffer, f32 depth_value, u8 stencil_value, s32 clear_mode);
extern void (* GX2CopyColorBufferToScanBuffer)(const GX2ColorBuffer *colorBuffer, s32 scan_target);
extern void (* GX2SwapScanBuffers)(void);
extern void (* GX2SetTVEnable)(s32 enable);
extern void (* GX2SetSwapInterval)(u32 swap_interval);
extern u32 (* GX2GetSwapInterval)(void);
extern void (* GX2WaitForVsync)(void);
extern void (* GX2CalcTVSize)(s32 tv_render_mode, s32 format, s32 buffering_mode, u32 * size, s32 * scale_needed);
extern void (* GX2Invalidate)(s32 invalidate_type, void * ptr, u32 buffer_size);
extern void (* GX2SetTVBuffer)(void *buffer, u32 buffer_size, s32 tv_render_mode, s32 format, s32 buffering_mode);
extern void (* GX2CalcSurfaceSizeAndAlignment)(GX2Surface *surface);
extern void (* GX2InitDepthBufferRegs)(GX2DepthBuffer *depthBuffer);
extern void (* GX2InitColorBufferRegs)(GX2ColorBuffer *colorBuffer);
extern void (* GX2CalcColorBufferAuxInfo)(GX2ColorBuffer *colorBuffer, u32 *size, u32 *align);
extern void (* GX2CalcDepthBufferHiZInfo)(GX2DepthBuffer *depthBuffer, u32 *size, u32 *align);
extern void (* GX2InitDepthBufferHiZEnable)(GX2DepthBuffer *depthBuffer, s32 hiZ_enable);
extern void (* GX2SetupContextStateEx)(GX2ContextState* state, s32 enable_profiling);
extern void (* GX2SetColorBuffer)(const GX2ColorBuffer *colorBuffer, s32 target);
extern void (* GX2SetDepthBuffer)(const GX2DepthBuffer *depthBuffer);
extern void (* GX2SetAttribBuffer)(u32 attr_index, u32 attr_size, u32 stride, const void* attr);
extern void (* GX2InitTextureRegs)(GX2Texture *texture);
extern void (* GX2InitSampler)(GX2Sampler *sampler, s32 tex_clamp, s32 min_mag_filter);
extern u32 (* GX2CalcFetchShaderSizeEx)(u32 num_attrib, s32 fetch_shader_type, s32 tessellation_mode);
extern void (* GX2InitFetchShaderEx)(GX2FetchShader* fs, void* fs_buffer, u32 count, const GX2AttribStream* attribs, s32 fetch_shader_type, s32 tessellation_mode);
extern void (* GX2SetFetchShader)(const GX2FetchShader* fs);
extern void (* GX2SetVertexUniformReg)(u32 offset, u32 count, const void *values);
extern void (* GX2SetPixelUniformReg)(u32 offset, u32 count, const void *values);
extern void (* GX2SetPixelTexture)(const GX2Texture *texture, u32 texture_hw_location);
extern void (* GX2SetVertexTexture)(const GX2Texture *texture, u32 texture_hw_location);
extern void (* GX2SetPixelSampler)(const GX2Sampler *sampler, u32 sampler_hw_location);
extern void (* GX2SetVertexSampler)(const GX2Sampler *sampler, u32 sampler_hw_location);
extern void (* GX2SetPixelShader)(const GX2PixelShader* pixelShader);
extern void (* GX2SetVertexShader)(const GX2VertexShader* vertexShader);
extern void (* GX2InitSamplerZMFilter)(GX2Sampler *sampler, s32 z_filter, s32 mip_filter);
extern void (* GX2SetColorControl)(s32 lop, u8 blend_enable_mask, s32 enable_multi_write, s32 enable_color_buffer);
extern void (* GX2SetDepthOnlyControl)(s32 enable_depth, s32 enable_depth_write, s32 depth_comp_function);
extern void (* GX2SetBlendControl)(s32 target, s32 color_src_blend, s32 color_dst_blend, s32 color_combine, s32 separate_alpha_blend, s32 alpha_src_blend, s32 alpha_dst_blend, s32 alpha_combine);
extern void (* GX2CalcDRCSize)(s32 drc_mode, s32 format, s32 buffering_mode, u32 *size, s32 *scale_needed);
extern void (* GX2SetDRCBuffer)(void *buffer, u32 buffer_size, s32 drc_mode, s32 surface_format, s32 buffering_mode);
extern void (* GX2SetDRCScale)(u32 width, u32 height);
extern void (* GX2SetDRCEnable)(s32 enable);
extern void (* GX2SetPolygonControl)(s32 front_face_mode, s32 cull_front, s32 cull_back, s32 enable_mode, s32 mode_font, s32 mode_back, s32 poly_offset_front, s32 poly_offset_back, s32 point_line_offset);
extern void (* GX2SetCullOnlyControl)(s32 front_face_mode, s32 cull_front, s32 cull_back);
extern void (* GX2SetDepthStencilControl)(s32 enable_depth_test, s32 enable_depth_write, s32 depth_comp_function,  s32 stencil_test_enable, s32 back_stencil_enable,
                                          s32 font_stencil_func, s32 front_stencil_z_pass, s32 front_stencil_z_fail, s32 front_stencil_fail,
                                          s32 back_stencil_func, s32 back_stencil_z_pass, s32 back_stencil_z_fail, s32 back_stencil_fail);
extern void (* GX2SetStencilMask)(u8 mask_front, u8 write_mask_front, u8 ref_front, u8 mask_back, u8 write_mask_back, u8 ref_back);
extern void (* GX2SetLineWidth)(f32 width);
extern void (* GX2SetTVGamma)(f32 val);
extern void (* GX2SetDRCGamma)(f32 val);
extern s32 (* GX2GetSystemTVScanMode)(void);
extern s32 (* GX2GetSystemDRCScanMode)(void);
extern void (* GX2RSetAllocator)(void * (*allocFunc)(u32, u32, u32), void (*freeFunc)(u32, void*));

static inline void GX2InitDepthBuffer(GX2DepthBuffer *depthBuffer, s32 dimension, u32 width, u32 height, u32 depth, s32 format, s32 aa)
{
    depthBuffer->surface.dimension = dimension;
    depthBuffer->surface.width = width;
    depthBuffer->surface.height = height;
    depthBuffer->surface.depth = depth;
    depthBuffer->surface.num_mips = 1;
    depthBuffer->surface.format = format;
    depthBuffer->surface.aa = aa;
    depthBuffer->surface.use = ((format==GX2_SURFACE_FORMAT_D_D24_S8_UNORM) || (format==GX2_SURFACE_FORMAT_D_D24_S8_FLOAT)) ? GX2_SURFACE_USE_DEPTH_BUFFER : GX2_SURFACE_USE_DEPTH_BUFFER_TEXTURE;
    depthBuffer->surface.tile = GX2_TILE_MODE_DEFAULT;
    depthBuffer->surface.swizzle  = 0;
    depthBuffer->view_mip = 0;
    depthBuffer->view_first_slice = 0;
    depthBuffer->view_slices_count = depth;
    depthBuffer->clear_depth = 1.0f;
    depthBuffer->clear_stencil = 0;
    depthBuffer->hiZ_data = NULL;
    depthBuffer->hiZ_size = 0;
    GX2CalcSurfaceSizeAndAlignment(&depthBuffer->surface);
    GX2InitDepthBufferRegs(depthBuffer);
}

static inline void GX2InitColorBuffer(GX2ColorBuffer *colorBuffer, s32 dimension, u32 width, u32 height, u32 depth, s32 format, s32 aa)
{
    colorBuffer->surface.dimension = dimension;
    colorBuffer->surface.width = width;
    colorBuffer->surface.height = height;
    colorBuffer->surface.depth = depth;
    colorBuffer->surface.num_mips = 1;
    colorBuffer->surface.format = format;
    colorBuffer->surface.aa = aa;
    colorBuffer->surface.use = GX2_SURFACE_USE_COLOR_BUFFER_TEXTURE_FTV;
    colorBuffer->surface.image_size = 0;
    colorBuffer->surface.image_data = NULL;
    colorBuffer->surface.mip_size = 0;
    colorBuffer->surface.mip_data = NULL;
    colorBuffer->surface.tile = GX2_TILE_MODE_DEFAULT;
    colorBuffer->surface.swizzle = 0;
    colorBuffer->surface.align = 0;
    colorBuffer->surface.pitch = 0;
    u32 i;
    for(i = 0; i < 13; i++)
        colorBuffer->surface.mip_offset[i] = 0;
    colorBuffer->view_mip = 0;
    colorBuffer->view_first_slice = 0;
    colorBuffer->view_slices_count = depth;
    colorBuffer->aux_data = NULL;
    colorBuffer->aux_size = 0;
    for(i = 0; i < 5; i++)
        colorBuffer->regs[i] = 0;

    GX2CalcSurfaceSizeAndAlignment(&colorBuffer->surface);
    GX2InitColorBufferRegs(colorBuffer);
}

static inline void GX2InitAttribStream(GX2AttribStream* attr, u32 location, u32 buffer, u32 offset, s32 format)
{
    attr->location = location;
    attr->buffer = buffer;
    attr->offset = offset;
    attr->format = format;
    attr->index_type = 0;
    attr->divisor = 0;
    attr->destination_selector = attribute_dest_comp_selector[format & 0xff];
    attr->endian_swap  = GX2_ENDIANSWAP_DEFAULT;
}

static inline void GX2InitTexture(GX2Texture *tex, u32 width, u32 height, u32 depth, u32 num_mips, s32 format, s32 dimension, s32 tile)
{
    tex->surface.dimension = dimension;
    tex->surface.width = width;
    tex->surface.height = height;
    tex->surface.depth = depth;
    tex->surface.num_mips = num_mips;
    tex->surface.format = format;
    tex->surface.aa = GX2_AA_MODE_1X;
    tex->surface.use = GX2_SURFACE_USE_TEXTURE;
    tex->surface.image_size = 0;
    tex->surface.image_data = NULL;
    tex->surface.mip_size = 0;
    tex->surface.mip_data = NULL;
    tex->surface.tile = tile;
    tex->surface.swizzle = 0;
    tex->surface.align = 0;
    tex->surface.pitch = 0;
    u32 i;
    for(i = 0; i < 13; i++)
        tex->surface.mip_offset[i] = 0;
    tex->view_first_mip = 0;
    tex->view_mips_count = num_mips;
    tex->view_first_slice = 0;
    tex->view_slices_count = depth;
    tex->component_selector = texture_comp_selector[format & 0x3f];
    for(i = 0; i < 5; i++)
        tex->regs[i] = 0;

    GX2CalcSurfaceSizeAndAlignment(&tex->surface);
    GX2InitTextureRegs(tex);
}

#ifdef __cplusplus
}
#endif

#endif // __GX2_FUNCTIONS_H_
