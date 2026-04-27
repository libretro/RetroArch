/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018-2019 - Stuart Carnie
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef METAL_COMMON_H__
#define METAL_COMMON_H__

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include "metal/metal_shader_types.h"
#include "../gfx_display.h"

#include <retro_common_api.h>
#include "../drivers_shader/slang_process.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_COCOATOUCH
#define PLATFORM_METAL_RESOURCE_STORAGE_MODE MTLResourceStorageModeShared
#else
#define PLATFORM_METAL_RESOURCE_STORAGE_MODE MTLResourceStorageModeManaged
#endif

/*! @brief maximum inflight frames for triple buffering */
#define MAX_INFLIGHT 3
#define CHAIN_LENGTH 3

/* macOS requires constants in a buffer to have a 256 byte alignment. */
#ifdef TARGET_OS_MAC
#define kMetalBufferAlignment 256
#else
#define kMetalBufferAlignment 4
#endif

#define MTL_ALIGN_BUFFER(size) ((size + kMetalBufferAlignment - 1) & (~(kMetalBufferAlignment - 1)))

/* HDR output modes — shared with metal.m and metal settings.
 * These are duplicated in metal.m to keep that file self-contained for
 * the bulk of its HDR logic, but exposed here so overlay/menu code can
 * query state if needed.
 * Kept out of the shader struct because the shader-side enum includes an
 * extra mode (3 = PQ->scRGB for shader-emitted HDR) that is an internal
 * detail of the composite fragment. */
#define METAL_HDR_OUTPUT_OFF    0u
#define METAL_HDR_OUTPUT_HDR10  1u
#define METAL_HDR_OUTPUT_SCRGB  2u

RETRO_BEGIN_DECLS

extern MTLPixelFormat glslang_format_to_metal(glslang_format fmt);
extern MTLPixelFormat SelectOptimalPixelFormat(MTLPixelFormat fmt);

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

/*! @brief swapBuffers acquires the next drawable, blocking if needed for vsync.
 *  This should be called after end to match Vulkan's swap_buffers timing. */
- (void)swapBuffers;

- (void)setRotation:(unsigned)rotation;
- (bool)readBackBuffer:(uint8_t *)buffer;

/* HDR.
 *
 * Enabling HDR switches the swapchain drawable to RGB10A2Unorm (HDR10) or
 * RGBA16Float (scRGB), sets up an sRGB offscreen buffer that shader passes
 * render into instead of the drawable, and inserts a composite pass between
 * them.  The composite pass does SDR->HDR10 inverse-tonemap / scRGB scale,
 * or passes through shader-emitted HDR content.
 *
 * hdrOutputMode is one of METAL_HDR_OUTPUT_{OFF,HDR10,SCRGB} — the public
 * settings value.  The composite shader itself also recognises mode 3
 * (PQ->scRGB) which is selected internally when the shader chain emits PQ
 * but the swapchain is scRGB.
 *
 * Viewport size is used to size the HDR offscreen + readback buffers. */
@property (nonatomic, readonly) bool hdrEnabled;
@property (nonatomic, readonly) unsigned hdrOutputMode;
@property (nonatomic, readonly) MTLPixelFormat hdrOffscreenFormat;
/* Current CAMetalLayer pixel format — the format downstream pipelines
 * must compile their colour attachment against to match the drawable.
 * Matches MTLPixelFormatBGRA8Unorm in SDR mode and an HDR format
 * (RGB10A2Unorm / RGBA16Float) in HDR mode. */
@property (nonatomic, readonly) MTLPixelFormat drawableFormat;
- (void)setHDROutputMode:(unsigned)mode
             viewportWidth:(unsigned)w
            viewportHeight:(unsigned)h;

/* Composite the source texture into the current drawable via the HDR encode
 * pipeline (hdr_composite_fragment).  Must be called while a frame is in
 * flight (commandBuffer is live).  The source texture is sampled across
 * the video-viewport rect of the drawable with its full 0..1 UV range;
 * the area outside the viewport is left as the clear colour
 * (letterbox / pillarbox).
 *
 * After this returns, the main rce points at the drawable with load=Load,
 * so follow-up draws (menu / overlay / OSD) compose directly on the HDR
 * backbuffer.
 *
 * The uniforms parameter is the already-populated HDRUniforms describing
 * the current frame's mode / paper-white / expand-gamut state.  The
 * caller supplies the source explicitly: the shader-chain's last-pass RT
 * if a preset is active, or the raw frame texture for the no-shader path. */
- (void)hdrComposite:(const HDRUniforms *)uniforms
          fromSource:(id<MTLTexture>)source;

/* HDR-specific setters exposed for the poke interface. */
- (void)setHDRPaperWhiteNits:(float)nits;
- (void)setHDRMenuNits:(float)nits;
- (void)setHDRExpandGamut:(unsigned)expandGamut;
- (void)setHDRScanlines:(bool)scanlines;
- (void)setHDRSubpixelLayout:(unsigned)layout;

/* (Re)allocate HDR-mode offscreen textures (readback landing pad and
 * SDR UI overlay) to match a new drawable size.  Called from
 * setViewportWidth:height: on window resize; cheap no-op when the
 * current allocations already match. */
- (void)resizeHDRResourcesForWidth:(NSUInteger)w height:(NSUInteger)h;

/* Shader-emitted HDR path: set by FrameView after parsing a shader preset,
 * tells the composite fragment to pass the final pass through without
 * inverse-tonemap / PQ encode. */
- (void)setHDRShaderEmitsHDR10:(bool)emitsHDR10
                    emitsHDR16:(bool)emitsHDR16;

/* Current HDRUniforms for composite pass — updated as settings change. */
- (const HDRUniforms *)currentHDRUniforms;

@end

@protocol FilterDelegate
- (void)configure:(id<MTLCommandEncoder>)encoder;
@end

@interface Filter : NSObject

@property (nonatomic, readwrite, assign) id<FilterDelegate> delegate;
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

#pragma mark - Driver Classes

@interface MetalView : MTKView
@end

@interface FrameView : NSObject

@property(nonatomic, readonly) RPixelFormat format;
@property(nonatomic, readonly) RTextureFilter filter;
@property(nonatomic, readwrite) BOOL visible;
@property(nonatomic, readwrite) CGRect frame;
@property(nonatomic, readwrite) CGSize size;
@property(nonatomic, readonly) ViewDrawState drawState;
@property(nonatomic, readonly) struct video_shader *shader;
@property(nonatomic, readwrite) uint64_t frameCount;

/* Final pass of the shader chain normally renders into the backbuffer
 * drawable.  When HDR is on, it must render into the HDR offscreen
 * instead so the composite pass can encode PQ/scRGB. */
@property(nonatomic, readwrite) BOOL hdrEnabled;

/* Whether the currently loaded shader preset's last pass emits native
 * HDR10 PQ or FP16 output — set by setShaderFromPath based on the
 * final pass's slang output format, read by the driver when composing. */
@property(nonatomic, readonly) BOOL shaderEmitsHDR10;
@property(nonatomic, readonly) BOOL shaderEmitsHDR16;

/* Current raw frame texture (SDR-linear sRGB content from the core).
 * Valid after the first updateFrame:pitch: call.  Only needed by the
 * HDR no-shader path which feeds this texture directly into
 * Context hdrComposite:fromSource: instead of going through
 * drawWithEncoder:. */
@property(nonatomic, readonly) id<MTLTexture> frameTexture;

/* Last shader pass's render target texture.  Nil when no shader preset
 * is active.  Used by the HDR composite path: when a preset is active,
 * the last pass writes here (at video-viewport size) and the composite
 * samples this texture into the video viewport of the drawable. */
@property(nonatomic, readonly) id<MTLTexture> shaderOutputTexture;

- (void)setFilteringIndex:(int)index smooth:(bool)smooth;
- (BOOL)setShaderFromPath:(NSString *)path;
- (void)clearShader;
- (void)updateFrame:(void const *)src pitch:(NSUInteger)pitch;
- (bool)readViewport:(uint8_t *)buffer isIdle:(bool)isIdle;

@end

@interface MetalMenu : NSObject

@property(nonatomic, readonly) bool hasFrame;
@property(nonatomic, readwrite) bool enabled;
@property(nonatomic, readwrite) float alpha;

- (void)updateFrame:(void const *)source;

- (void)updateWidth:(int)width
						 height:(int)height
						 format:(RPixelFormat)format
						 filter:(RTextureFilter)filter;
@end

@interface Overlay : NSObject
@property(nonatomic, readwrite) bool enabled;
@property(nonatomic, readwrite) bool fullscreen;

- (bool)loadImages:(const struct texture_image *)images count:(NSUInteger)count;
- (void)updateVertexX:(float)x y:(float)y w:(float)w h:(float)h index:(NSUInteger)index;
- (void)updateTextureCoordsX:(float)x y:(float)y w:(float)w h:(float)h index:(NSUInteger)index;
- (void)updateAlpha:(float)alpha index:(NSUInteger)index;
@end

@interface MetalDriver : NSObject<MTKViewDelegate>

@property(nonatomic, readonly) video_viewport_t *viewport;
@property(nonatomic, readwrite) bool keepAspect;
@property(nonatomic, readonly) MetalMenu *menu;
@property(nonatomic, readonly) FrameView *frameView;
@property(nonatomic, readonly) MenuDisplay *display;
@property(nonatomic, readonly) Overlay *overlay;
@property(nonatomic, readonly) Context *context;
@property(nonatomic, readonly) Uniforms *viewportMVP;

- (instancetype)initWithVideo:(const video_info_t *)video
												input:(input_driver_t **)input
										inputData:(void **)inputData;

- (void)setVideo:(const video_info_t *)video;
- (bool)renderFrame:(const void *)frame
                     data:(void*)data
							width:(unsigned)width
						 height:(unsigned)height
				 frameCount:(uint64_t)frameCount
							pitch:(unsigned)pitch
								msg:(const char *)msg
							 info:(video_frame_info_t *)video_info;

/*! @brief setNeedsResize triggers a display resize */
- (void)setNeedsResize;
- (void)setViewportWidth:(unsigned)width height:(unsigned)height forceFull:(BOOL)forceFull allowRotate:(BOOL)allowRotate;
- (void)setRotation:(unsigned)rotation;

@end

RETRO_END_DECLS

#endif
