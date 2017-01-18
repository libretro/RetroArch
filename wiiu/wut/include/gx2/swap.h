#pragma once
#include <wut.h>
#include "enum.h"

/**
 * \defgroup gx2_swap Swap
 * \ingroup gx2
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GX2ColorBuffer GX2ColorBuffer;
typedef struct GX2Texture GX2Texture;

void
GX2CopyColorBufferToScanBuffer(GX2ColorBuffer *buffer,
                               GX2ScanTarget scanTarget);

void
GX2SwapScanBuffers();

BOOL
GX2GetLastFrame(GX2ScanTarget scanTarget,
                GX2Texture *texture);

BOOL
GX2GetLastFrameGamma(GX2ScanTarget scanTarget,
                     float *gammaOut);

uint32_t
GX2GetSwapInterval();

void
GX2SetSwapInterval(uint32_t interval);

#ifdef __cplusplus
}
#endif

/** @} */
