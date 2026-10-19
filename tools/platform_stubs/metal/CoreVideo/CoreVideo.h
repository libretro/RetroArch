/* CoreVideo declarations used by the Metal compile stand-in. */
#ifndef STUB_COREVIDEO_H
#define STUB_COREVIDEO_H
#include <Metal/Metal.h>
typedef const void *CVPixelBufferRef;
typedef const void *CVMetalTextureCacheRef;
typedef const void *CVMetalTextureRef;
enum { kCVReturnSuccess = 0,
   kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange = 1,
   kCVPixelFormatType_420YpCbCr8BiPlanarFullRange,
   kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange,
   kCVPixelFormatType_420YpCbCr10BiPlanarFullRange };
extern int CVPixelBufferIsPlanar(CVPixelBufferRef);
extern size_t CVPixelBufferGetPlaneCount(CVPixelBufferRef);
extern unsigned CVPixelBufferGetPixelFormatType(CVPixelBufferRef);
extern size_t CVPixelBufferGetWidthOfPlane(CVPixelBufferRef, size_t);
extern size_t CVPixelBufferGetHeightOfPlane(CVPixelBufferRef, size_t);
extern int CVMetalTextureCacheCreateTextureFromImage(const void *, CVMetalTextureCacheRef,
      CVPixelBufferRef, const void *, MTLPixelFormat, size_t, size_t, size_t, CVMetalTextureRef *);
extern int CVMetalTextureIsFlipped(CVMetalTextureRef);
extern id<MTLTexture> CVMetalTextureGetTexture(CVMetalTextureRef);
#endif
