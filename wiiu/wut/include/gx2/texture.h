#pragma once
#include <wut.h>
#include "surface.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct GX2Texture GX2Texture;

struct GX2Texture
{
   GX2Surface surface;
   uint32_t viewFirstMip;
   uint32_t viewNumMips;
   uint32_t viewFirstSlice;
   uint32_t viewNumSlices;
   uint32_t compMap;

   uint32_t regs[5];
};

void GX2InitTextureRegs(GX2Texture *texture);
void GX2SetPixelTexture(GX2Texture *texture, uint32_t unit);
void GX2SetVertexTexture(GX2Texture *texture, uint32_t unit);
void GX2SetGeometryTexture(GX2Texture *texture, uint32_t unit);

#ifdef __cplusplus
}
#endif

/** @} */
