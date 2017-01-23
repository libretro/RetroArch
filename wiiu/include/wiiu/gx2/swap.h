#pragma once
#include <wiiu/types.h>
#include "enum.h"
#include "surface.h"
#include "texture.h"

#ifdef __cplusplus
extern "C" {
#endif

void GX2CopyColorBufferToScanBuffer(GX2ColorBuffer *buffer, GX2ScanTarget scanTarget);
void GX2SwapScanBuffers();
BOOL GX2GetLastFrame(GX2ScanTarget scanTarget, GX2Texture *texture);
BOOL GX2GetLastFrameGamma(GX2ScanTarget scanTarget, float *gammaOut);
uint32_t GX2GetSwapInterval();
void GX2SetSwapInterval(uint32_t interval);

#ifdef __cplusplus
}
#endif
