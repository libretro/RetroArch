//
//  Context.h
//  MetalRenderer
//
//  Created by Stuart Carnie on 6/9/18.
//  Copyright © 2018 Stuart Carnie. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import "RendererCommon.h"

@interface Texture : NSObject
@property (readonly) id<MTLTexture> texture;
@property (readonly) id<MTLSamplerState> sampler;
@end

/*! @brief Context contains the render state used by various components */
@interface Context : NSObject

@property (readonly) id<MTLDevice>        device;
@property (readonly) id<MTLLibrary>       library;
@property (readonly) id<MTLCommandQueue>  commandQueue;
/*! @brief Returns the command buffer for the current frame */
@property (readonly) id<MTLCommandBuffer> commandBuffer;
@property (readonly) id<CAMetalDrawable>  nextDrawable;
@property (readonly) id<MTLTexture>       renderTexture;

- (instancetype)initWithDevice:(id<MTLDevice>)d
                         layer:(CAMetalLayer *)layer
                       library:(id<MTLLibrary>)l;

- (Texture *)newTexture:(struct texture_image)image filter:(enum texture_filter_type)filter;
- (void)convertFormat:(RPixelFormat)fmt from:(id<MTLBuffer>)src to:(id<MTLTexture>)dst;

/*! @brief begin marks the beginning of a frame */
- (void)begin;

/*! @brief end commits the command buffer */
- (void)end;

@end
