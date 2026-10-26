#pragma once
#include <wiiu/types.h>
#include "enum.h"
#include "surface.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
   uint32_t pa_sc_aa_mask;
} GX2AAMaskReg;

typedef struct
{
   uint32_t sx_alpha_test_control;
   uint32_t sx_alpha_ref;
} GX2AlphaTestReg;

typedef struct
{
   uint32_t db_alpha_to_mask;
} GX2AlphaToMaskReg;

typedef struct
{
   GX2RenderTarget target;
   uint32_t cb_blend_control;
} GX2BlendControlReg;

typedef struct
{
   float red;
   float green;
   float blue;
   float alpha;
} GX2BlendConstantColorReg;

typedef struct
{
   uint32_t cb_color_control;
} GX2ColorControlReg;

typedef struct
{
   uint32_t db_depth_control;
} GX2DepthStencilControlReg;

typedef struct
{
   uint32_t db_stencilrefmask;
   uint32_t db_stencilrefmask_bf;
} GX2StencilMaskReg;

typedef struct
{
   uint32_t pa_su_line_cntl;
} GX2LineWidthReg;

typedef struct
{
   uint32_t pa_su_point_size;
} GX2PointSizeReg;

typedef struct
{
   uint32_t pa_su_point_minmax;
} GX2PointLimitsReg;

typedef struct
{
   uint32_t pa_su_sc_mode_cntl;
} GX2PolygonControlReg;

typedef struct
{
   uint32_t pa_su_poly_offset_front_scale;
   uint32_t pa_su_poly_offset_front_offset;
   uint32_t pa_su_poly_offset_back_scale;
   uint32_t pa_su_poly_offset_back_offset;
   uint32_t pa_su_poly_offset_clamp;
} GX2PolygonOffsetReg;

typedef struct
{
   uint32_t pa_sc_generic_scissor_tl;
   uint32_t pa_sc_generic_scissor_br;
} GX2ScissorReg;

typedef struct
{
   uint32_t cb_target_mask;
} GX2TargetChannelMaskReg;

typedef struct
{
   uint32_t pa_cl_vport_xscale;
   uint32_t pa_cl_vport_xoffset;
   uint32_t pa_cl_vport_yscale;
   uint32_t pa_cl_vport_yoffset;
   uint32_t pa_cl_vport_zscale;
   uint32_t pa_cl_vport_zoffset;
   uint32_t pa_cl_gb_vert_clip_adj;
   uint32_t pa_cl_gb_vert_disc_adj;
   uint32_t pa_cl_gb_horz_clip_adj;
   uint32_t pa_cl_gb_horz_disc_adj;
   uint32_t pa_sc_vport_zmin;
   uint32_t pa_sc_vport_zmax;
} GX2ViewportReg;

void GX2SetAAMask(uint8_t upperLeft, uint8_t upperRight, uint8_t lowerLeft, uint8_t lowerRight);
void GX2InitAAMaskReg(GX2AAMaskReg *reg, uint8_t upperLeft, uint8_t upperRight, uint8_t lowerLeft,
                      uint8_t lowerRight);
void GX2GetAAMaskReg(GX2AAMaskReg *reg, uint8_t *upperLeft, uint8_t *upperRight, uint8_t *lowerLeft,
                     uint8_t *lowerRight);
void GX2SetAAMaskReg(GX2AAMaskReg *reg);
void GX2SetAlphaTest(BOOL alphaTest, GX2CompareFunction func, float ref);
void GX2InitAlphaTestReg(GX2AlphaTestReg *reg, BOOL alphaTest, GX2CompareFunction func, float ref);
void GX2GetAlphaTestReg(const GX2AlphaTestReg *reg, BOOL *alphaTest, GX2CompareFunction *func,
                        float *ref);
void GX2SetAlphaTestReg(GX2AlphaTestReg *reg);
void GX2SetAlphaToMask(BOOL alphaToMask, GX2AlphaToMaskMode mode);
void GX2InitAlphaToMaskReg(GX2AlphaToMaskReg *reg, BOOL alphaToMask, GX2AlphaToMaskMode mode);
void GX2GetAlphaToMaskReg(const GX2AlphaToMaskReg *reg, BOOL *alphaToMask,
                          GX2AlphaToMaskMode *mode);
void GX2SetAlphaToMaskReg(GX2AlphaToMaskReg *reg);
void GX2SetBlendConstantColor(float red, float green, float blue, float alpha);
void GX2InitBlendConstantColorReg(GX2BlendConstantColorReg *reg, float red, float green, float blue,
                                  float alpha);
void GX2GetBlendConstantColorReg(GX2BlendConstantColorReg *reg, float *red, float *green,
                                 float *blue, float *alpha);
void GX2SetBlendConstantColorReg(GX2BlendConstantColorReg *reg);
void GX2SetBlendControl(GX2RenderTarget target, GX2BlendMode colorSrcBlend,
                        GX2BlendMode colorDstBlend, GX2BlendCombineMode colorCombine, BOOL useAlphaBlend,
                        GX2BlendMode alphaSrcBlend, GX2BlendMode alphaDstBlend, GX2BlendCombineMode alphaCombine);
void GX2InitBlendControlReg(GX2BlendControlReg *reg, GX2RenderTarget target,
                            GX2BlendMode colorSrcBlend, GX2BlendMode colorDstBlend, GX2BlendCombineMode colorCombine,
                            BOOL useAlphaBlend, GX2BlendMode alphaSrcBlend, GX2BlendMode alphaDstBlend,
                            GX2BlendCombineMode alphaCombine);
void GX2GetBlendControlReg(GX2BlendControlReg *reg, GX2RenderTarget *target,
                           GX2BlendMode *colorSrcBlend, GX2BlendMode *colorDstBlend, GX2BlendCombineMode *colorCombine,
                           BOOL *useAlphaBlend, GX2BlendMode *alphaSrcBlend, GX2BlendMode *alphaDstBlend,
                           GX2BlendCombineMode *alphaCombine);
void GX2SetBlendControlReg(GX2BlendControlReg *reg);
void GX2SetColorControl(GX2LogicOp rop3, uint8_t targetBlendEnable, BOOL multiWriteEnable,
                        BOOL colorWriteEnable);
void GX2InitColorControlReg(GX2ColorControlReg *reg, GX2LogicOp rop3, uint8_t targetBlendEnable,
                            BOOL multiWriteEnable, BOOL colorWriteEnable);
void GX2GetColorControlReg(GX2ColorControlReg *reg, GX2LogicOp *rop3, uint8_t *targetBlendEnable,
                           BOOL *multiWriteEnable, BOOL *colorWriteEnable);
void GX2SetColorControlReg(GX2ColorControlReg *reg);
void GX2SetDepthOnlyControl(BOOL depthTest, BOOL depthWrite, GX2CompareFunction depthCompare);
void GX2SetDepthStencilControl(BOOL depthTest, BOOL depthWrite,  GX2CompareFunction depthCompare,
                               BOOL stencilTest, BOOL backfaceStencil, GX2CompareFunction frontStencilFunc,
                               GX2StencilFunction frontStencilZPass,  GX2StencilFunction frontStencilZFail,
                               GX2StencilFunction frontStencilFail, GX2CompareFunction backStencilFunc,
                               GX2StencilFunction backStencilZPass, GX2StencilFunction backStencilZFail,
                               GX2StencilFunction backStencilFail);
void GX2InitDepthStencilControlReg(GX2DepthStencilControlReg *reg, BOOL depthTest, BOOL depthWrite,
                                   GX2CompareFunction depthCompare, BOOL stencilTest, BOOL backfaceStencil,
                                   GX2CompareFunction frontStencilFunc, GX2StencilFunction frontStencilZPass,
                                   GX2StencilFunction frontStencilZFail, GX2StencilFunction frontStencilFail,
                                   GX2CompareFunction backStencilFunc, GX2StencilFunction backStencilZPass,
                                   GX2StencilFunction backStencilZFail, GX2StencilFunction backStencilFail);
void GX2GetDepthStencilControlReg(GX2DepthStencilControlReg *reg, BOOL *depthTest, BOOL *depthWrite,
                                  GX2CompareFunction *depthCompare, BOOL *stencilTest, BOOL *backfaceStencil,
                                  GX2CompareFunction *frontStencilFunc, GX2StencilFunction *frontStencilZPass,
                                  GX2StencilFunction *frontStencilZFail, GX2StencilFunction *frontStencilFail,
                                  GX2CompareFunction *backStencilFunc, GX2StencilFunction *backStencilZPass,
                                  GX2StencilFunction *backStencilZFail, GX2StencilFunction *backStencilFail);
void GX2SetDepthStencilControlReg(GX2DepthStencilControlReg *reg);
void GX2SetStencilMask(uint8_t frontMask, uint8_t frontWriteMask, uint8_t frontRef,
                       uint8_t backMask, uint8_t backWriteMask, uint8_t backRef);
void GX2InitStencilMaskReg(GX2StencilMaskReg *reg, uint8_t frontMask, uint8_t frontWriteMask,
                           uint8_t frontRef, uint8_t backMask, uint8_t backWriteMask, uint8_t backRef);
void GX2GetStencilMaskReg(GX2StencilMaskReg *reg, uint8_t *frontMask, uint8_t *frontWriteMask,
                          uint8_t *frontRef, uint8_t *backMask, uint8_t *backWriteMask, uint8_t *backRef);
void GX2SetStencilMaskReg(GX2StencilMaskReg *reg);
void GX2SetLineWidth(float width);
void GX2InitLineWidthReg(GX2LineWidthReg *reg, float width);
void GX2GetLineWidthReg(GX2LineWidthReg *reg, float *width);
void GX2SetLineWidthReg(GX2LineWidthReg *reg);
void GX2SetPointSize(float width, float height);
void GX2InitPointSizeReg(GX2PointSizeReg *reg, float width, float height);
void GX2GetPointSizeReg(GX2PointSizeReg *reg, float *width, float *height);
void GX2SetPointSizeReg(GX2PointSizeReg *reg);
void GX2SetPointLimits(float min, float max);
void GX2InitPointLimitsReg(GX2PointLimitsReg *reg, float min, float max);
void GX2GetPointLimitsReg(GX2PointLimitsReg *reg, float *min, float *max);
void GX2SetPointLimitsReg(GX2PointLimitsReg *reg);
void GX2SetCullOnlyControl(GX2FrontFace frontFace, BOOL cullFront, BOOL cullBack);
void GX2SetPolygonControl(GX2FrontFace frontFace, BOOL cullFront, BOOL cullBack, BOOL polyMode,
                          GX2PolygonMode polyModeFront, GX2PolygonMode polyModeBack, BOOL polyOffsetFrontEnable,
                          BOOL polyOffsetBackEnable, BOOL polyOffsetParaEnable);
void GX2InitPolygonControlReg(GX2PolygonControlReg *reg, GX2FrontFace frontFace, BOOL cullFront,
                              BOOL cullBack, BOOL polyMode, GX2PolygonMode polyModeFront, GX2PolygonMode polyModeBack,
                              BOOL polyOffsetFrontEnable, BOOL polyOffsetBackEnable, BOOL polyOffsetParaEnable);
void GX2GetPolygonControlReg(GX2PolygonControlReg *reg, GX2FrontFace *frontFace, BOOL *cullFront,
                             BOOL *cullBack, BOOL *polyMode, GX2PolygonMode *polyModeFront, GX2PolygonMode *polyModeBack,
                             BOOL *polyOffsetFrontEnable, BOOL *polyOffsetBackEnable, BOOL *polyOffsetParaEnable);
void GX2SetPolygonControlReg(GX2PolygonControlReg *reg);
void GX2SetPolygonOffset(float frontOffset, float frontScale, float backOffset, float backScale,
                         float clamp);
void GX2InitPolygonOffsetReg(GX2PolygonOffsetReg *reg, float frontOffset, float frontScale,
                             float backOffset, float backScale, float clamp);
void GX2GetPolygonOffsetReg(GX2PolygonOffsetReg *reg, float *frontOffset, float *frontScale,
                            float *backOffset, float *backScale, float *clamp);
void GX2SetPolygonOffsetReg(GX2PolygonOffsetReg *reg);
void GX2SetScissor(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void GX2InitScissorReg(GX2ScissorReg *reg, uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void GX2GetScissorReg(GX2ScissorReg *reg, uint32_t *x, uint32_t *y, uint32_t *width,
                      uint32_t *height);
void GX2SetScissorReg(GX2ScissorReg *reg);
void GX2SetTargetChannelMasks(GX2ChannelMask mask0, GX2ChannelMask mask1, GX2ChannelMask mask2,
                              GX2ChannelMask mask3, GX2ChannelMask mask4, GX2ChannelMask mask5, GX2ChannelMask mask6,
                              GX2ChannelMask mask7);
void GX2InitTargetChannelMasksReg(GX2TargetChannelMaskReg *reg, GX2ChannelMask mask0,
                                  GX2ChannelMask mask1, GX2ChannelMask mask2, GX2ChannelMask mask3, GX2ChannelMask mask4,
                                  GX2ChannelMask mask5, GX2ChannelMask mask6, GX2ChannelMask mask7);
void GX2GetTargetChannelMasksReg(GX2TargetChannelMaskReg *reg, GX2ChannelMask *mask0,
                                 GX2ChannelMask *mask1, GX2ChannelMask *mask2, GX2ChannelMask *mask3, GX2ChannelMask *mask4,
                                 GX2ChannelMask *mask5, GX2ChannelMask *mask6, GX2ChannelMask *mask7);
void GX2SetTargetChannelMasksReg(GX2TargetChannelMaskReg *reg);
void GX2SetViewport(float x, float y, float width, float height, float nearZ, float farZ);
void GX2InitViewportReg(GX2ViewportReg *reg, float x, float y, float width, float height,
                        float nearZ, float farZ);
void GX2GetViewportReg(GX2ViewportReg *reg, float *x, float *y, float *width, float *height,
                       float *nearZ, float *farZ);
void GX2SetViewportReg(GX2ViewportReg *reg);
#ifdef __cplusplus
}
#endif

/** @} */
