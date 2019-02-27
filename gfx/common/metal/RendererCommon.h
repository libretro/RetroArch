//
//  RendererCommon.h
//  MetalRenderer
//
//  Created by Stuart Carnie on 6/3/18.
//  Copyright © 2018 Stuart Carnie. All rights reserved.
//

#ifndef RendererCommon_h
#define RendererCommon_h

#import <Foundation/Foundation.h>
#import "ShaderTypes.h"

// TODO(sgc): implement triple buffering
/*! @brief maximum inflight frames */
#define MAX_INFLIGHT 1
#define CHAIN_LENGTH 3

/* macOS requires constants in a buffer to have a 256 byte alignment. */
#ifdef TARGET_OS_MAC
#define kMetalBufferAlignment 256
#else
#define kMetalBufferAlignment 4
#endif

#define MTL_ALIGN_BUFFER(size) ((size + kMetalBufferAlignment - 1) & (~(kMetalBufferAlignment - 1)))

#pragma mark - Pixel Formats

typedef NS_ENUM(NSUInteger, RPixelFormat)
{

   RPixelFormatInvalid,

   /* 16-bit formats */
   RPixelFormatBGRA4Unorm,
   RPixelFormatB5G6R5Unorm,

   RPixelFormatBGRA8Unorm,
   RPixelFormatBGRX8Unorm, // RetroArch XRGB

   RPixelFormatCount,
};

extern NSUInteger RPixelFormatToBPP(RPixelFormat format);
extern NSString *NSStringFromRPixelFormat(RPixelFormat format);

typedef NS_ENUM(NSUInteger, RTextureFilter)
{
   RTextureFilterNearest,
   RTextureFilterLinear,

   RTextureFilterCount,
};

extern matrix_float4x4 matrix_proj_ortho(float left, float right, float top, float bottom);
extern matrix_float4x4 matrix_rotate_z(float rot);
extern matrix_float4x4 make_matrix_float4x4(const float *v);

#endif /* RendererCommon_h */
