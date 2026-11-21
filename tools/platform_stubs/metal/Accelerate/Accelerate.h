/* Stand-in for the vImage declarations camera/drivers/avfoundation.m
 * uses; shapes follow the SDK, nothing here links. */
#ifndef STUB_ACCELERATE_H
#define STUB_ACCELERATE_H
#include <stddef.h>
#include <stdint.h>
typedef unsigned long vImagePixelCount;
typedef long vImage_Error;
typedef uint32_t vImage_Flags;
typedef uint8_t Pixel_8888[4];
typedef struct vImage_Buffer
{
   void *data;
   vImagePixelCount height, width;
   size_t rowBytes;
} vImage_Buffer;
typedef struct { int32_t Yp_bias, CbCr_bias, YpRangeMax, CbCrRangeMax; }
   vImage_YpCbCrPixelRange;
typedef struct { char opaque[128]; } vImage_YpCbCrToARGB;
typedef struct vImage_YpCbCrToARGBMatrix vImage_YpCbCrToARGBMatrix;
extern const vImage_YpCbCrToARGBMatrix *kvImage_YpCbCrToARGBMatrix_ITU_R_601_4;
typedef enum { kvImage420Yp8_CbCr8 = 2 } vImageYpCbCrType;
typedef enum { kvImageARGB8888 = 0 } vImageARGBType;
enum { kvImageNoError = 0, kvImageNoFlags = 0,
   kvImageHighQualityResampling = 32 };
vImage_Error vImagePermuteChannels_ARGB8888(const vImage_Buffer *src,
      const vImage_Buffer *dst, const uint8_t permuteMap[4], vImage_Flags f);
vImage_Error vImageConvert_YpCbCrToARGB_GenerateConversion(
      const vImage_YpCbCrToARGBMatrix *matrix,
      const vImage_YpCbCrPixelRange *range, vImage_YpCbCrToARGB *out,
      vImageYpCbCrType in_type, vImageARGBType out_type, vImage_Flags f);
vImage_Error vImageConvert_420Yp8_CbCr8ToARGB8888(const vImage_Buffer *y,
      const vImage_Buffer *cbcr, const vImage_Buffer *dst,
      const vImage_YpCbCrToARGB *info, const uint8_t permuteMap[4],
      uint8_t alpha, vImage_Flags f);
vImage_Error vImageRotate90_ARGB8888(const vImage_Buffer *src,
      const vImage_Buffer *dst, uint8_t rotation,
      const Pixel_8888 background, vImage_Flags f);
vImage_Error vImageHorizontalReflect_ARGB8888(const vImage_Buffer *src,
      const vImage_Buffer *dst, vImage_Flags f);
vImage_Error vImageScale_ARGB8888(const vImage_Buffer *src,
      const vImage_Buffer *dst, void *temp, vImage_Flags f);
#endif
