#pragma once
#include <wut.h>
#include "enum.h"

/**
 * \defgroup gx2_surface Surface
 * \ingroup gx2
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GX2Surface GX2Surface;
typedef struct GX2DepthBuffer GX2DepthBuffer;
typedef struct GX2ColorBuffer GX2ColorBuffer;

struct GX2Surface
{
   GX2SurfaceDim dim;
   uint32_t width;
   uint32_t height;
   uint32_t depth;
   uint32_t mipLevels;
   GX2SurfaceFormat format;
   GX2AAMode aa;
   GX2SurfaceUse use;
   uint32_t imageSize;
   void *image;
   uint32_t mipmapSize;
   void *mipmaps;
   GX2TileMode tileMode;
   uint32_t swizzle;
   uint32_t alignment;
   uint32_t pitch;
   uint32_t mipLevelOffset[13];
};
CHECK_OFFSET(GX2Surface, 0x0, dim);
CHECK_OFFSET(GX2Surface, 0x4, width);
CHECK_OFFSET(GX2Surface, 0x8, height);
CHECK_OFFSET(GX2Surface, 0xc, depth);
CHECK_OFFSET(GX2Surface, 0x10, mipLevels);
CHECK_OFFSET(GX2Surface, 0x14, format);
CHECK_OFFSET(GX2Surface, 0x18, aa);
CHECK_OFFSET(GX2Surface, 0x1c, use);
CHECK_OFFSET(GX2Surface, 0x20, imageSize);
CHECK_OFFSET(GX2Surface, 0x24, image);
CHECK_OFFSET(GX2Surface, 0x28, mipmapSize);
CHECK_OFFSET(GX2Surface, 0x2c, mipmaps);
CHECK_OFFSET(GX2Surface, 0x30, tileMode);
CHECK_OFFSET(GX2Surface, 0x34, swizzle);
CHECK_OFFSET(GX2Surface, 0x38, alignment);
CHECK_OFFSET(GX2Surface, 0x3C, pitch);
CHECK_OFFSET(GX2Surface, 0x40, mipLevelOffset);
CHECK_SIZE(GX2Surface, 0x74);

struct GX2DepthBuffer
{
   GX2Surface surface;

   uint32_t viewMip;
   uint32_t viewFirstSlice;
   uint32_t viewNumSlices;
   void *hiZPtr;
   uint32_t hiZSize;
   float depthClear;
   uint32_t stencilClear;

   uint32_t regs[7];
};
CHECK_OFFSET(GX2DepthBuffer, 0x74, viewMip);
CHECK_OFFSET(GX2DepthBuffer, 0x78, viewFirstSlice);
CHECK_OFFSET(GX2DepthBuffer, 0x7C, viewNumSlices);
CHECK_OFFSET(GX2DepthBuffer, 0x80, hiZPtr);
CHECK_OFFSET(GX2DepthBuffer, 0x84, hiZSize);
CHECK_OFFSET(GX2DepthBuffer, 0x88, depthClear);
CHECK_OFFSET(GX2DepthBuffer, 0x8C, stencilClear);
CHECK_OFFSET(GX2DepthBuffer, 0x90, regs);
CHECK_SIZE(GX2DepthBuffer, 0xAC);

struct GX2ColorBuffer
{
   GX2Surface surface;

   uint32_t viewMip;
   uint32_t viewFirstSlice;
   uint32_t viewNumSlices;
   void *aaBuffer;
   uint32_t aaSize;

   uint32_t regs[5];
};
CHECK_OFFSET(GX2ColorBuffer, 0x74, viewMip);
CHECK_OFFSET(GX2ColorBuffer, 0x78, viewFirstSlice);
CHECK_OFFSET(GX2ColorBuffer, 0x7C, viewNumSlices);
CHECK_OFFSET(GX2ColorBuffer, 0x80, aaBuffer);
CHECK_OFFSET(GX2ColorBuffer, 0x84, aaSize);
CHECK_OFFSET(GX2ColorBuffer, 0x88, regs);
CHECK_SIZE(GX2ColorBuffer, 0x9C);

void
GX2CalcSurfaceSizeAndAlignment(GX2Surface *surface);

void
GX2CalcDepthBufferHiZInfo(GX2DepthBuffer *depthBuffer,
                          uint32_t *outSize,
                          uint32_t *outAlignment);

void
GX2CalcColorBufferAuxInfo(GX2ColorBuffer *surface,
                          uint32_t *outSize,
                          uint32_t *outAlignment);

void
GX2SetColorBuffer(GX2ColorBuffer *colorBuffer,
                  GX2RenderTarget target);

void
GX2SetDepthBuffer(GX2DepthBuffer *depthBuffer);

void
GX2InitColorBufferRegs(GX2ColorBuffer *colorBuffer);

void
GX2InitDepthBufferRegs(GX2DepthBuffer *depthBuffer);

void
GX2InitDepthBufferHiZEnable(GX2DepthBuffer *depthBuffer,
                            BOOL enable);

uint32_t
GX2GetSurfaceSwizzle(GX2Surface *surface);

void
GX2SetSurfaceSwizzle(GX2Surface *surface,
                     uint32_t swizzle);

void
GX2CopySurface(GX2Surface *src,
               uint32_t srcLevel,
               uint32_t srcDepth,
               GX2Surface *dst,
               uint32_t dstLevel,
               uint32_t dstDepth);

#ifdef __cplusplus
}
#endif

/** @} */
