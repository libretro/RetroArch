#pragma once
#include <wiiu/types.h>
#include <gx2/surface.h>
#include "resource.h"

#ifdef __cplusplus
extern "C" {
#endif

bool GX2RCreateSurface(GX2Surface *surface, GX2RResourceFlags flags);
bool GX2RCreateSurfaceUserMemory(GX2Surface *surface, uint8_t *image, uint8_t *mipmap, GX2RResourceFlags flags);
void GX2RDestroySurfaceEx(GX2Surface *surface, GX2RResourceFlags flags);
void GX2RInvalidateSurface(GX2Surface *surface, int32_t level, GX2RResourceFlags flags);
void *GX2RLockSurfaceEx(GX2Surface *surface, int32_t level, GX2RResourceFlags flags);
void GX2RUnlockSurfaceEx(GX2Surface *surface, int32_t level, GX2RResourceFlags flags);

#ifdef __cplusplus
}
#endif
