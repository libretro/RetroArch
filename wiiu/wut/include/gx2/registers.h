#pragma once
#include <wut.h>
#include "enum.h"
#include "surface.h"

/**
 * \defgroup gx2_registers Registers
 * \ingroup gx2
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GX2AAMaskReg GX2AAMaskReg;
typedef struct GX2AlphaTestReg GX2AlphaTestReg;
typedef struct GX2AlphaToMaskReg GX2AlphaToMaskReg;
typedef struct GX2BlendControlReg GX2BlendControlReg;
typedef struct GX2BlendConstantColorReg GX2BlendConstantColorReg;
typedef struct GX2ColorControlReg GX2ColorControlReg;
typedef struct GX2DepthStencilControlReg GX2DepthStencilControlReg;
typedef struct GX2StencilMaskReg GX2StencilMaskReg;
typedef struct GX2LineWidthReg GX2LineWidthReg;
typedef struct GX2PointSizeReg GX2PointSizeReg;
typedef struct GX2PointLimitsReg GX2PointLimitsReg;
typedef struct GX2PolygonControlReg GX2PolygonControlReg;
typedef struct GX2PolygonOffsetReg GX2PolygonOffsetReg;
typedef struct GX2ScissorReg GX2ScissorReg;
typedef struct GX2TargetChannelMaskReg GX2TargetChannelMaskReg;
typedef struct GX2ViewportReg GX2ViewportReg;

struct GX2AAMaskReg
{
   uint32_t pa_sc_aa_mask;
};
CHECK_OFFSET(GX2AAMaskReg, 0, pa_sc_aa_mask);
CHECK_SIZE(GX2AAMaskReg, 4);

struct GX2AlphaTestReg
{
   uint32_t sx_alpha_test_control;
   uint32_t sx_alpha_ref;
};
CHECK_OFFSET(GX2AlphaTestReg, 0, sx_alpha_test_control);
CHECK_OFFSET(GX2AlphaTestReg, 4, sx_alpha_ref);
CHECK_SIZE(GX2AlphaTestReg, 8);

struct GX2AlphaToMaskReg
{
   uint32_t db_alpha_to_mask;
};
CHECK_OFFSET(GX2AlphaToMaskReg, 0, db_alpha_to_mask);
CHECK_SIZE(GX2AlphaToMaskReg, 4);

struct GX2BlendControlReg
{
   GX2RenderTarget target;
   uint32_t cb_blend_control;
};
CHECK_OFFSET(GX2BlendControlReg, 0, target);
CHECK_OFFSET(GX2BlendControlReg, 4, cb_blend_control);
CHECK_SIZE(GX2BlendControlReg, 8);

struct GX2BlendConstantColorReg
{
   float red;
   float green;
   float blue;
   float alpha;
};
CHECK_OFFSET(GX2BlendConstantColorReg, 0x00, red);
CHECK_OFFSET(GX2BlendConstantColorReg, 0x04, green);
CHECK_OFFSET(GX2BlendConstantColorReg, 0x08, blue);
CHECK_OFFSET(GX2BlendConstantColorReg, 0x0c, alpha);
CHECK_SIZE(GX2BlendConstantColorReg, 0x10);

struct GX2ColorControlReg
{
   uint32_t cb_color_control;
};
CHECK_OFFSET(GX2ColorControlReg, 0x00, cb_color_control);
CHECK_SIZE(GX2ColorControlReg, 4);

struct GX2DepthStencilControlReg
{
   uint32_t db_depth_control;
};
CHECK_OFFSET(GX2DepthStencilControlReg, 0, db_depth_control);
CHECK_SIZE(GX2DepthStencilControlReg, 4);

struct GX2StencilMaskReg
{
   uint32_t db_stencilrefmask;
   uint32_t db_stencilrefmask_bf;
};
CHECK_OFFSET(GX2StencilMaskReg, 0, db_stencilrefmask);
CHECK_OFFSET(GX2StencilMaskReg, 4, db_stencilrefmask_bf);
CHECK_SIZE(GX2StencilMaskReg, 8);

struct GX2LineWidthReg
{
   uint32_t pa_su_line_cntl;
};
CHECK_OFFSET(GX2LineWidthReg, 0, pa_su_line_cntl);
CHECK_SIZE(GX2LineWidthReg, 4);

struct GX2PointSizeReg
{
   uint32_t pa_su_point_size;
};
CHECK_OFFSET(GX2PointSizeReg, 0, pa_su_point_size);
CHECK_SIZE(GX2PointSizeReg, 4);

struct GX2PointLimitsReg
{
   uint32_t pa_su_point_minmax;
};
CHECK_OFFSET(GX2PointLimitsReg, 0, pa_su_point_minmax);
CHECK_SIZE(GX2PointLimitsReg, 4);

struct GX2PolygonControlReg
{
   uint32_t pa_su_sc_mode_cntl;
};
CHECK_OFFSET(GX2PolygonControlReg, 0, pa_su_sc_mode_cntl);
CHECK_SIZE(GX2PolygonControlReg, 4);

struct GX2PolygonOffsetReg
{
   uint32_t pa_su_poly_offset_front_scale;
   uint32_t pa_su_poly_offset_front_offset;
   uint32_t pa_su_poly_offset_back_scale;
   uint32_t pa_su_poly_offset_back_offset;
   uint32_t pa_su_poly_offset_clamp;
};
CHECK_OFFSET(GX2PolygonOffsetReg, 0x00, pa_su_poly_offset_front_scale);
CHECK_OFFSET(GX2PolygonOffsetReg, 0x04, pa_su_poly_offset_front_offset);
CHECK_OFFSET(GX2PolygonOffsetReg, 0x08, pa_su_poly_offset_back_scale);
CHECK_OFFSET(GX2PolygonOffsetReg, 0x0C, pa_su_poly_offset_back_offset);
CHECK_OFFSET(GX2PolygonOffsetReg, 0x10, pa_su_poly_offset_clamp);
CHECK_SIZE(GX2PolygonOffsetReg, 20);

struct GX2ScissorReg
{
   uint32_t pa_sc_generic_scissor_tl;
   uint32_t pa_sc_generic_scissor_br;
};
CHECK_OFFSET(GX2ScissorReg, 0x00, pa_sc_generic_scissor_tl);
CHECK_OFFSET(GX2ScissorReg, 0x04, pa_sc_generic_scissor_br);
CHECK_SIZE(GX2ScissorReg, 8);

struct GX2TargetChannelMaskReg
{
   uint32_t cb_target_mask;
};
CHECK_OFFSET(GX2TargetChannelMaskReg, 0x00, cb_target_mask);
CHECK_SIZE(GX2TargetChannelMaskReg, 4);

struct GX2ViewportReg
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
};
CHECK_OFFSET(GX2ViewportReg, 0x00, pa_cl_vport_xscale);
CHECK_OFFSET(GX2ViewportReg, 0x04, pa_cl_vport_xoffset);
CHECK_OFFSET(GX2ViewportReg, 0x08, pa_cl_vport_yscale);
CHECK_OFFSET(GX2ViewportReg, 0x0C, pa_cl_vport_yoffset);
CHECK_OFFSET(GX2ViewportReg, 0x10, pa_cl_vport_zscale);
CHECK_OFFSET(GX2ViewportReg, 0x14, pa_cl_vport_zoffset);
CHECK_OFFSET(GX2ViewportReg, 0x18, pa_cl_gb_vert_clip_adj);
CHECK_OFFSET(GX2ViewportReg, 0x1C, pa_cl_gb_vert_disc_adj);
CHECK_OFFSET(GX2ViewportReg, 0x20, pa_cl_gb_horz_clip_adj);
CHECK_OFFSET(GX2ViewportReg, 0x24, pa_cl_gb_horz_disc_adj);
CHECK_OFFSET(GX2ViewportReg, 0x28, pa_sc_vport_zmin);
CHECK_OFFSET(GX2ViewportReg, 0x2C, pa_sc_vport_zmax);
CHECK_SIZE(GX2ViewportReg, 48);

void
GX2SetAAMask(uint8_t upperLeft,
             uint8_t upperRight,
             uint8_t lowerLeft,
             uint8_t lowerRight);

void
GX2InitAAMaskReg(GX2AAMaskReg *reg,
                 uint8_t upperLeft,
                 uint8_t upperRight,
                 uint8_t lowerLeft,
                 uint8_t lowerRight);

void
GX2GetAAMaskReg(GX2AAMaskReg *reg,
                uint8_t *upperLeft,
                uint8_t *upperRight,
                uint8_t *lowerLeft,
                uint8_t *lowerRight);

void
GX2SetAAMaskReg(GX2AAMaskReg *reg);

void
GX2SetAlphaTest(BOOL alphaTest,
                GX2CompareFunction func,
                float ref);

void
GX2InitAlphaTestReg(GX2AlphaTestReg *reg,
                    BOOL alphaTest,
                    GX2CompareFunction func,
                    float ref);

void
GX2GetAlphaTestReg(const GX2AlphaTestReg *reg,
                   BOOL *alphaTest,
                   GX2CompareFunction *func,
                   float *ref);

void
GX2SetAlphaTestReg(GX2AlphaTestReg *reg);

void
GX2SetAlphaToMask(BOOL alphaToMask,
                  GX2AlphaToMaskMode mode);

void
GX2InitAlphaToMaskReg(GX2AlphaToMaskReg *reg,
                      BOOL alphaToMask,
                      GX2AlphaToMaskMode mode);

void
GX2GetAlphaToMaskReg(const GX2AlphaToMaskReg *reg,
                     BOOL *alphaToMask,
                     GX2AlphaToMaskMode *mode);

void
GX2SetAlphaToMaskReg(GX2AlphaToMaskReg *reg);

void
GX2SetBlendConstantColor(float red,
                         float green,
                         float blue,
                         float alpha);

void
GX2InitBlendConstantColorReg(GX2BlendConstantColorReg *reg,
                             float red,
                             float green,
                             float blue,
                             float alpha);

void
GX2GetBlendConstantColorReg(GX2BlendConstantColorReg *reg,
                            float *red,
                            float *green,
                            float *blue,
                            float *alpha);

void
GX2SetBlendConstantColorReg(GX2BlendConstantColorReg *reg);

void
GX2SetBlendControl(GX2RenderTarget target,
                   GX2BlendMode colorSrcBlend,
                   GX2BlendMode colorDstBlend,
                   GX2BlendCombineMode colorCombine,
                   BOOL useAlphaBlend,
                   GX2BlendMode alphaSrcBlend,
                   GX2BlendMode alphaDstBlend,
                   GX2BlendCombineMode alphaCombine);

void
GX2InitBlendControlReg(GX2BlendControlReg *reg,
                       GX2RenderTarget target,
                       GX2BlendMode colorSrcBlend,
                       GX2BlendMode colorDstBlend,
                       GX2BlendCombineMode colorCombine,
                       BOOL useAlphaBlend,
                       GX2BlendMode alphaSrcBlend,
                       GX2BlendMode alphaDstBlend,
                       GX2BlendCombineMode alphaCombine);

void
GX2GetBlendControlReg(GX2BlendControlReg *reg,
                      GX2RenderTarget *target,
                      GX2BlendMode *colorSrcBlend,
                      GX2BlendMode *colorDstBlend,
                      GX2BlendCombineMode *colorCombine,
                      BOOL *useAlphaBlend,
                      GX2BlendMode *alphaSrcBlend,
                      GX2BlendMode *alphaDstBlend,
                      GX2BlendCombineMode *alphaCombine);

void
GX2SetBlendControlReg(GX2BlendControlReg *reg);

void
GX2SetColorControl(GX2LogicOp rop3,
                   uint8_t targetBlendEnable,
                   BOOL multiWriteEnable,
                   BOOL colorWriteEnable);

void
GX2InitColorControlReg(GX2ColorControlReg *reg,
                       GX2LogicOp rop3,
                       uint8_t targetBlendEnable,
                       BOOL multiWriteEnable,
                       BOOL colorWriteEnable);

void
GX2GetColorControlReg(GX2ColorControlReg *reg,
                      GX2LogicOp *rop3,
                      uint8_t *targetBlendEnable,
                      BOOL *multiWriteEnable,
                      BOOL *colorWriteEnable);

void
GX2SetColorControlReg(GX2ColorControlReg *reg);

void
GX2SetDepthOnlyControl(BOOL depthTest,
                       BOOL depthWrite,
                       GX2CompareFunction depthCompare);

void
GX2SetDepthStencilControl(BOOL depthTest,
                          BOOL depthWrite,
                          GX2CompareFunction depthCompare,
                          BOOL stencilTest,
                          BOOL backfaceStencil,
                          GX2CompareFunction frontStencilFunc,
                          GX2StencilFunction frontStencilZPass,
                          GX2StencilFunction frontStencilZFail,
                          GX2StencilFunction frontStencilFail,
                          GX2CompareFunction backStencilFunc,
                          GX2StencilFunction backStencilZPass,
                          GX2StencilFunction backStencilZFail,
                          GX2StencilFunction backStencilFail);

void
GX2InitDepthStencilControlReg(GX2DepthStencilControlReg *reg,
                              BOOL depthTest,
                              BOOL depthWrite,
                              GX2CompareFunction depthCompare,
                              BOOL stencilTest,
                              BOOL backfaceStencil,
                              GX2CompareFunction frontStencilFunc,
                              GX2StencilFunction frontStencilZPass,
                              GX2StencilFunction frontStencilZFail,
                              GX2StencilFunction frontStencilFail,
                              GX2CompareFunction backStencilFunc,
                              GX2StencilFunction backStencilZPass,
                              GX2StencilFunction backStencilZFail,
                              GX2StencilFunction backStencilFail);

void
GX2GetDepthStencilControlReg(GX2DepthStencilControlReg *reg,
                             BOOL *depthTest,
                             BOOL *depthWrite,
                             GX2CompareFunction *depthCompare,
                             BOOL *stencilTest,
                             BOOL *backfaceStencil,
                             GX2CompareFunction *frontStencilFunc,
                             GX2StencilFunction *frontStencilZPass,
                             GX2StencilFunction *frontStencilZFail,
                             GX2StencilFunction *frontStencilFail,
                             GX2CompareFunction *backStencilFunc,
                             GX2StencilFunction *backStencilZPass,
                             GX2StencilFunction *backStencilZFail,
                             GX2StencilFunction *backStencilFail);

void
GX2SetDepthStencilControlReg(GX2DepthStencilControlReg *reg);

void
GX2SetStencilMask(uint8_t frontMask,
                  uint8_t frontWriteMask,
                  uint8_t frontRef,
                  uint8_t backMask,
                  uint8_t backWriteMask,
                  uint8_t backRef);

void
GX2InitStencilMaskReg(GX2StencilMaskReg *reg,
                      uint8_t frontMask,
                      uint8_t frontWriteMask,
                      uint8_t frontRef,
                      uint8_t backMask,
                      uint8_t backWriteMask,
                      uint8_t backRef);

void
GX2GetStencilMaskReg(GX2StencilMaskReg *reg,
                     uint8_t *frontMask,
                     uint8_t *frontWriteMask,
                     uint8_t *frontRef,
                     uint8_t *backMask,
                     uint8_t *backWriteMask,
                     uint8_t *backRef);

void
GX2SetStencilMaskReg(GX2StencilMaskReg *reg);

void
GX2SetLineWidth(float width);

void
GX2InitLineWidthReg(GX2LineWidthReg *reg,
                    float width);

void
GX2GetLineWidthReg(GX2LineWidthReg *reg,
                   float *width);

void
GX2SetLineWidthReg(GX2LineWidthReg *reg);

void
GX2SetPointSize(float width,
                float height);

void
GX2InitPointSizeReg(GX2PointSizeReg *reg,
                    float width,
                    float height);

void
GX2GetPointSizeReg(GX2PointSizeReg *reg,
                   float *width,
                   float *height);

void
GX2SetPointSizeReg(GX2PointSizeReg *reg);

void
GX2SetPointLimits(float min,
                  float max);

void
GX2InitPointLimitsReg(GX2PointLimitsReg *reg,
                      float min,
                      float max);

void
GX2GetPointLimitsReg(GX2PointLimitsReg *reg,
                     float *min,
                     float *max);

void
GX2SetPointLimitsReg(GX2PointLimitsReg *reg);

void
GX2SetCullOnlyControl(GX2FrontFace frontFace,
                      BOOL cullFront,
                      BOOL cullBack);

void
GX2SetPolygonControl(GX2FrontFace frontFace,
                     BOOL cullFront,
                     BOOL cullBack,
                     BOOL polyMode,
                     GX2PolygonMode polyModeFront,
                     GX2PolygonMode polyModeBack,
                     BOOL polyOffsetFrontEnable,
                     BOOL polyOffsetBackEnable,
                     BOOL polyOffsetParaEnable);

void
GX2InitPolygonControlReg(GX2PolygonControlReg *reg,
                         GX2FrontFace frontFace,
                         BOOL cullFront,
                         BOOL cullBack,
                         BOOL polyMode,
                         GX2PolygonMode polyModeFront,
                         GX2PolygonMode polyModeBack,
                         BOOL polyOffsetFrontEnable,
                         BOOL polyOffsetBackEnable,
                         BOOL polyOffsetParaEnable);

void
GX2GetPolygonControlReg(GX2PolygonControlReg *reg,
                        GX2FrontFace *frontFace,
                        BOOL *cullFront,
                        BOOL *cullBack,
                        BOOL *polyMode,
                        GX2PolygonMode *polyModeFront,
                        GX2PolygonMode *polyModeBack,
                        BOOL *polyOffsetFrontEnable,
                        BOOL *polyOffsetBackEnable,
                        BOOL *polyOffsetParaEnable);

void
GX2SetPolygonControlReg(GX2PolygonControlReg *reg);

void
GX2SetPolygonOffset(float frontOffset,
                    float frontScale,
                    float backOffset,
                    float backScale,
                    float clamp);

void
GX2InitPolygonOffsetReg(GX2PolygonOffsetReg *reg,
                        float frontOffset,
                        float frontScale,
                        float backOffset,
                        float backScale,
                        float clamp);

void
GX2GetPolygonOffsetReg(GX2PolygonOffsetReg *reg,
                       float *frontOffset,
                       float *frontScale,
                       float *backOffset,
                       float *backScale,
                       float *clamp);

void
GX2SetPolygonOffsetReg(GX2PolygonOffsetReg *reg);

void
GX2SetScissor(uint32_t x,
              uint32_t y,
              uint32_t width,
              uint32_t height);

void
GX2InitScissorReg(GX2ScissorReg *reg,
                  uint32_t x,
                  uint32_t y,
                  uint32_t width,
                  uint32_t height);

void
GX2GetScissorReg(GX2ScissorReg *reg,
                 uint32_t *x,
                 uint32_t *y,
                 uint32_t *width,
                 uint32_t *height);

void
GX2SetScissorReg(GX2ScissorReg *reg);

void
GX2SetTargetChannelMasks(GX2ChannelMask mask0,
                         GX2ChannelMask mask1,
                         GX2ChannelMask mask2,
                         GX2ChannelMask mask3,
                         GX2ChannelMask mask4,
                         GX2ChannelMask mask5,
                         GX2ChannelMask mask6,
                         GX2ChannelMask mask7);

void
GX2InitTargetChannelMasksReg(GX2TargetChannelMaskReg *reg,
                             GX2ChannelMask mask0,
                             GX2ChannelMask mask1,
                             GX2ChannelMask mask2,
                             GX2ChannelMask mask3,
                             GX2ChannelMask mask4,
                             GX2ChannelMask mask5,
                             GX2ChannelMask mask6,
                             GX2ChannelMask mask7);

void
GX2GetTargetChannelMasksReg(GX2TargetChannelMaskReg *reg,
                            GX2ChannelMask *mask0,
                            GX2ChannelMask *mask1,
                            GX2ChannelMask *mask2,
                            GX2ChannelMask *mask3,
                            GX2ChannelMask *mask4,
                            GX2ChannelMask *mask5,
                            GX2ChannelMask *mask6,
                            GX2ChannelMask *mask7);

void
GX2SetTargetChannelMasksReg(GX2TargetChannelMaskReg *reg);

void
GX2SetViewport(float x,
               float y,
               float width,
               float height,
               float nearZ,
               float farZ);

void
GX2InitViewportReg(GX2ViewportReg *reg,
                   float x,
                   float y,
                   float width,
                   float height,
                   float nearZ,
                   float farZ);

void
GX2GetViewportReg(GX2ViewportReg *reg,
                  float *x,
                  float *y,
                  float *width,
                  float *height,
                  float *nearZ,
                  float *farZ);

void
GX2SetViewportReg(GX2ViewportReg *reg);

#ifdef __cplusplus
}
#endif

/** @} */
