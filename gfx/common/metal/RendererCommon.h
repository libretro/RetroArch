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

// TODO(sgc): implement triple buffering
/*! @brief maximum inflight frames */
#define MAX_INFLIGHT 1

#pragma mark - Pixel Formats

typedef NS_ENUM(NSUInteger, RPixelFormat) {
   
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

typedef NS_ENUM(NSUInteger, RTextureFilter) {
   RTextureFilterNearest,
   RTextureFilterLinear,
   
   RTextureFilterCount,
};

#endif /* RendererCommon_h */
