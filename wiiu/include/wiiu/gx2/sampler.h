#pragma once
#include <wiiu/types.h>
#include "enum.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GX2Sampler
{
   uint32_t regs[3];
} GX2Sampler;

void GX2InitSampler(GX2Sampler *sampler, GX2TexClampMode clampMode,
                    GX2TexXYFilterMode minMagFilterMode);
void GX2InitSamplerBorderType(GX2Sampler *sampler, GX2TexBorderType borderType);
void GX2InitSamplerClamping(GX2Sampler *sampler, GX2TexClampMode clampX, GX2TexClampMode clampY,
                            GX2TexClampMode clampZ);
void GX2InitSamplerDepthCompare(GX2Sampler *sampler, GX2CompareFunction depthCompare);
void GX2InitSamplerFilterAdjust(GX2Sampler *sampler, BOOL highPrecision, GX2TexMipPerfMode perfMip,
                                GX2TexZPerfMode perfZ);
void GX2InitSamplerLOD(GX2Sampler *sampler, float lodMin, float lodMax, float lodBias);
void GX2InitSamplerLODAdjust(GX2Sampler *sampler, float unk1, BOOL unk2);
void GX2InitSamplerRoundingMode(GX2Sampler *sampler, GX2RoundingMode roundingMode);
void GX2InitSamplerXYFilter(GX2Sampler *sampler, GX2TexXYFilterMode filterMag,
                            GX2TexXYFilterMode filterMin, GX2TexAnisoRatio maxAniso);
void GX2InitSamplerZMFilter(GX2Sampler *sampler, GX2TexZFilterMode filterZ,
                            GX2TexMipFilterMode filterMip);

#ifdef __cplusplus
}
#endif

/** @} */
