//
//  PixelConverter.h
//  MetalRenderer
//
//  Created by Stuart Carnie on 6/9/18.
//  Copyright © 2018 Stuart Carnie. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#import "RendererCommon.h"

NS_ASSUME_NONNULL_BEGIN

@interface PixelConverter : NSObject
- (void)convertFormat:(RPixelFormat)fmt from:(id<MTLBuffer>)src to:(id<MTLTexture>)dst;
@end

NS_ASSUME_NONNULL_END
