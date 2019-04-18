//
//  RendererCommon.m
//  MetalRenderer
//
//  Created by Stuart Carnie on 6/3/18.
//  Copyright © 2018 Stuart Carnie. All rights reserved.
//

#import "RendererCommon.h"
#import <Metal/Metal.h>

NSUInteger RPixelFormatToBPP(RPixelFormat format)
{
   switch (format)
   {
      case RPixelFormatBGRA8Unorm:
      case RPixelFormatBGRX8Unorm:
         return 4;

      case RPixelFormatB5G6R5Unorm:
      case RPixelFormatBGRA4Unorm:
         return 2;

      default:
         RARCH_ERR("[Metal]: unknown RPixel format: %d\n", format);
         return 4;
   }
}

static NSString *RPixelStrings[RPixelFormatCount];

NSString *NSStringFromRPixelFormat(RPixelFormat format)
{
   static dispatch_once_t onceToken;
   dispatch_once(&onceToken, ^{

#define STRING(literal) RPixelStrings[literal] = @#literal
      STRING(RPixelFormatInvalid);
      STRING(RPixelFormatB5G6R5Unorm);
      STRING(RPixelFormatBGRA4Unorm);
      STRING(RPixelFormatBGRA8Unorm);
      STRING(RPixelFormatBGRX8Unorm);
#undef STRING

   });

   if (format >= RPixelFormatCount)
   {
      format = RPixelFormatInvalid;
   }

   return RPixelStrings[format];
}

matrix_float4x4 make_matrix_float4x4(const float *v)
{
   simd_float4 P = simd_make_float4(v[0], v[1], v[2], v[3]);
   v += 4;
   simd_float4 Q = simd_make_float4(v[0], v[1], v[2], v[3]);
   v += 4;
   simd_float4 R = simd_make_float4(v[0], v[1], v[2], v[3]);
   v += 4;
   simd_float4 S = simd_make_float4(v[0], v[1], v[2], v[3]);

   matrix_float4x4 mat = {P, Q, R, S};
   return mat;
}

matrix_float4x4 matrix_proj_ortho(float left, float right, float top, float bottom)
{
   float near = 0;
   float far = 1;

   float sx = 2 / (right - left);
   float sy = 2 / (top - bottom);
   float sz = 1 / (far - near);
   float tx = (right + left) / (left - right);
   float ty = (top + bottom) / (bottom - top);
   float tz = near / (far - near);

   simd_float4 P = simd_make_float4(sx, 0, 0, 0);
   simd_float4 Q = simd_make_float4(0, sy, 0, 0);
   simd_float4 R = simd_make_float4(0, 0, sz, 0);
   simd_float4 S = simd_make_float4(tx, ty, tz, 1);

   matrix_float4x4 mat = {P, Q, R, S};
   return mat;
}

matrix_float4x4 matrix_rotate_z(float rot)
{
   float cz, sz;
   __sincosf(rot, &sz, &cz);

   simd_float4 P = simd_make_float4(cz, -sz, 0, 0);
   simd_float4 Q = simd_make_float4(sz,  cz, 0, 0);
   simd_float4 R = simd_make_float4( 0,   0, 1, 0);
   simd_float4 S = simd_make_float4( 0,   0, 0, 1);

   matrix_float4x4 mat = {P, Q, R, S};
   return mat;
}
