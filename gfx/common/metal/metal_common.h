/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018      - Stuart Carnie
 *  copyright (c) 2011-2021 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef METAL_RENDERER_H
#define METAL_RENDERER_H

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "metal_shader_types.h"

#include "../../gfx_display.h"

/* TODO/FIXME: implement triple buffering */
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
   RPixelFormatBGRX8Unorm, /* RetroArch XRGB */

   RPixelFormatCount
};

extern NSUInteger RPixelFormatToBPP(RPixelFormat format);

typedef NS_ENUM(NSUInteger, RTextureFilter)
{
   RTextureFilterNearest,
   RTextureFilterLinear,
   RTextureFilterCount
};

extern matrix_float4x4 matrix_proj_ortho(float left, float right, float top, float bottom);


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

@protocol FilterDelegate
- (void)configure:(id<MTLCommandEncoder>)encoder;
@end

@interface Filter : NSObject

@property (nonatomic, readwrite) id<FilterDelegate> delegate;
@property (nonatomic, readonly) id<MTLSamplerState> sampler;

- (void)apply:(id<MTLCommandBuffer>)cb in:(id<MTLTexture>)tin out:(id<MTLTexture>)tout;
- (void)apply:(id<MTLCommandBuffer>)cb inBuf:(id<MTLBuffer>)tin outTex:(id<MTLTexture>)tout;

+ (instancetype)newFilterWithFunctionName:(NSString *)name device:(id<MTLDevice>)device library:(id<MTLLibrary>)library error:(NSError **)error;

@end

@interface MenuDisplay : NSObject

@property (nonatomic, readwrite) BOOL blend;
@property (nonatomic, readwrite) MTLClearColor clearColor;

- (instancetype)initWithContext:(Context *)context;
- (void)drawPipeline:(gfx_display_ctx_draw_t *)draw;
- (void)draw:(gfx_display_ctx_draw_t *)draw;
- (void)setScissorRect:(MTLScissorRect)rect;
- (void)clearScissorRect;

#pragma mark - static methods

+ (const float *)defaultVertices;
+ (const float *)defaultTexCoords;
+ (const float *)defaultColor;

@end

typedef NS_ENUM(NSInteger, ViewDrawState)
{
   ViewDrawStateNone    = 0x00,
   ViewDrawStateContext = 0x01,
   ViewDrawStateEncoder = 0x02,

   ViewDrawStateAll     = 0x03
};

@interface ViewDescriptor : NSObject
@property (nonatomic, readwrite) RPixelFormat format;
@property (nonatomic, readwrite) RTextureFilter filter;
@property (nonatomic, readwrite) CGSize size;

- (instancetype)init;
@end

@interface TexturedView : NSObject

@property (nonatomic, readonly) RPixelFormat format;
@property (nonatomic, readonly) RTextureFilter filter;
@property (nonatomic, readwrite) BOOL visible;
@property (nonatomic, readwrite) CGRect frame;
@property (nonatomic, readwrite) CGSize size;
@property (nonatomic, readonly) ViewDrawState drawState;

- (instancetype)initWithDescriptor:(ViewDescriptor *)td context:(Context *)c;

- (void)drawWithContext:(Context *)ctx;
- (void)drawWithEncoder:(id<MTLRenderCommandEncoder>)rce;
- (void)updateFrame:(void const *)src pitch:(NSUInteger)pitch;

@end

#endif
