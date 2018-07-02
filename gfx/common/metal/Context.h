//
//  Context.h
//  MetalRenderer
//
//  Created by Stuart Carnie on 6/9/18.
//  Copyright © 2018 Stuart Carnie. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

NS_ASSUME_NONNULL_BEGIN

@interface Context : NSObject

@property (readonly) id<MTLDevice>        device;
@property (readonly) id<MTLLibrary>       library;
@property (readonly) id<MTLCommandQueue>  commandQueue;
/*! @brief Returns the command buffer for the current frame */
@property (readonly) id<MTLCommandBuffer> commandBuffer;
@property (readonly) id<CAMetalDrawable>  nextDrawable;
@property (readonly) id<MTLTexture>       renderTexture;

+ (instancetype)newContextWithDevice:(id<MTLDevice>)d
                               layer:(CAMetalLayer *)layer
                             library:(id<MTLLibrary>)l
                        commandQueue:(id<MTLCommandQueue>)q;

/*! @brief begin marks the beginning of a frame */
- (void)begin;

/*! @brief end commits the command buffer */
- (void)end;

@end

NS_ASSUME_NONNULL_END
