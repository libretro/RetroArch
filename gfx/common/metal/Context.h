//
//  Context.h
//  MetalRenderer
//
//  Created by Stuart Carnie on 6/9/18.
//  Copyright © 2018 Stuart Carnie. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import "RendererCommon.h"

@interface Texture : NSObject
@property (nonatomic, readonly) id<MTLTexture> texture;
@property (nonatomic, readonly) id<MTLSamplerState> sampler;
@end

typedef struct
{
   void *data;
   NSUInteger offset;
   __unsafe_unretained id<MTLBuffer> buffer;
} BufferRange;

typedef NS_ENUM(NSUInteger, ViewportResetMode) {
   kFullscreenViewport,
   kVideoViewport
};

/*! @brief Context contains the render state used by various components */
@interface Context : NSObject

@property (nonatomic, readonly) id<MTLDevice> device;
@property (nonatomic, readonly) id<MTLLibrary> library;
@property (nonatomic, readwrite) MTLClearColor clearColor;
@property (nonatomic, readwrite) video_viewport_t *viewport;
@property (nonatomic, readonly) Uniforms *uniforms;

/*! @brief Specifies whether rendering is synchronized with the display */
@property (nonatomic, readwrite) bool displaySyncEnabled;

/*! @brief captureEnabled allows previous frames to be read */
@property (nonatomic, readwrite) bool captureEnabled;

/*! @brief Returns the command buffer used for pre-render work,
 * such as mip maps and shader effects
 * */
@property (nonatomic, readonly) id<MTLCommandBuffer> blitCommandBuffer;

/*! @brief Returns the command buffer for the current frame */
@property (nonatomic, readonly) id<MTLCommandBuffer> commandBuffer;
@property (nonatomic, readonly) id<CAMetalDrawable> nextDrawable;

/*! @brief Main render encoder to back buffer */
@property (nonatomic, readonly) id<MTLRenderCommandEncoder> rce;

- (instancetype)initWithDevice:(id<MTLDevice>)d
                         layer:(CAMetalLayer *)layer
                       library:(id<MTLLibrary>)l;

- (Texture *)newTexture:(struct texture_image)image filter:(enum texture_filter_type)filter;
- (id<MTLTexture>)newTexture:(struct texture_image)image mipmapped:(bool)mipmapped;
- (void)convertFormat:(RPixelFormat)fmt from:(id<MTLTexture>)src to:(id<MTLTexture>)dst;
- (id<MTLRenderPipelineState>)getStockShader:(int)index blend:(bool)blend;

/*! @brief resets the viewport for the main render encoder to \a mode */
- (void)resetRenderViewport:(ViewportResetMode)mode;

/*! @brief resets the scissor rect for the main render encoder to the drawable size */
- (void)resetScissorRect;

/*! @brief draws a quad at the specified position (normalized coordinates) using the main render encoder */
- (void)drawQuadX:(float)x y:(float)y w:(float)w h:(float)h
                r:(float)r g:(float)g b:(float)b a:(float)a;

- (bool)allocRange:(BufferRange *)range length:(NSUInteger)length;

/*! @brief begin marks the beginning of a frame */
- (void)begin;

/*! @brief end commits the command buffer */
- (void)end;

- (void)setRotation:(unsigned)rotation;
- (bool)readBackBuffer:(uint8_t *)buffer;

@end
