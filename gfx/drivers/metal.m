/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018      - Stuart Carnie
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

#include <Foundation/Foundation.h>
#include "../../apple_runtime.h"
#include <Metal/Metal.h>
#include <MetalKit/MetalKit.h>
#include <CoreVideo/CoreVideo.h>
#include <QuartzCore/QuartzCore.h>

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <memory.h>
#include <string.h>

#include <simd/simd.h>

#include <encodings/utf.h>
#include <compat/strl.h>
#include <lists/string_list.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include <formats/image.h>
#include <formats/image_yuv_blit.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <retro_math.h>
#include <retro_assert.h>
#include <retro_atomic.h>
#include <features/features_cpu.h>
#include <libretro.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include "../../record/drivers/record_metal.h"
#include "../font_driver.h"
#include "../video_driver.h"
#include "../gfx_instrument.h"
#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif

#include "metal.h"
#include "../common/rgba16_pack.h"
#include "../gfx_display.h"
#include "../drivers_shader/slang_process.h"

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

/* HDR output modes — internal to the Metal driver.
 * Kept distinct from the shader struct enum because the shader-side enum
 * includes an extra mode (3 = PQ->scRGB for shader-emitted HDR) that is
 * an internal detail of the composite fragment. */
#define METAL_HDR_OUTPUT_OFF    0u
#define METAL_HDR_OUTPUT_HDR10  1u
#define METAL_HDR_OUTPUT_SCRGB  2u

/* Forward declarations for C functions defined later in this file but
 * called from earlier @implementations. */
static MTLPixelFormat glslang_format_to_metal(glslang_format fmt);
static MTLPixelFormat SelectOptimalPixelFormat(MTLPixelFormat fmt);

/* When a drawable reached the display (macOS 10.15.4, iOS/tvOS 10.3).
 * Declared here rather than taken from the SDK, so the driver builds
 * against any SDK and decides at runtime whether to ask. */
@protocol MetalPresentedDrawable
- (void)addPresentedHandler:(void (^)(id<MTLDrawable> drawable))block;
- (CFTimeInterval)presentedTime;
@end

#ifdef RETRO_ATOMIC_HAS_64
/* When the last presented drawable reached the display, on the
 * cpu_features_get_time_usec() clock, 0 for not known. Written by the
 * presented handler on whatever thread Metal runs it, read on the video
 * thread. File scope rather than on the Context, so the handler holds
 * no reference to it: a Context torn down with a present in flight is
 * still released on the thread that owns it. One layer presents at a
 * time; a late report from a previous one is still a vblank of the same
 * display. */
static retro_atomic_64_t metal_presented_at;
#endif

#pragma mark - Pixel Formats

typedef NS_ENUM(NSUInteger, RPixelFormat)
{

   RPixelFormatInvalid,

   /* 16-bit formats */
   RPixelFormatBGRA4Unorm,
   RPixelFormatB5G6R5Unorm,

   RPixelFormatBGRA8Unorm,
   RPixelFormatBGRX8Unorm, /* RetroArch XRGB */

   /* RetroArch XRGB2101010 (10-bit per channel). BGR10A2 matches the ABI's
    * packed layout (R in bits [29:20], G [19:10], B [9:0]) with no swizzle,
    * and is directly sampleable, so it uses the same direct-encoder draw
    * path as BGRA8/BGRX8 rather than a conversion filter. */
   RPixelFormatBGR10A2Unorm,

   RPixelFormatCount
};

typedef NS_ENUM(NSUInteger, RTextureFilter)
{
   RTextureFilterNearest,
   RTextureFilterLinear,
   RTextureFilterCount
};

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
/* HDR availability gate.
 *
 * Compile-time: the SDK must expose CAMetalLayer's
 * wantsExtendedDynamicRangeContent property and the PQ colour space name.
 * The PQ colour space name (kCGColorSpaceITUR_2100_PQ) is the binding
 * constraint on macOS — introduced in 10.15.4 but gated to macOS 11.0 in
 * the public headers.  On iOS the EDR surface area on CAMetalLayer was
 * only exposed in the 16.x SDKs.  tvOS never got a public EDR path:
 * wantsExtendedDynamicRangeContent / edrMetadata are not part of the
 * public tvOS interface regardless of SDK version, so HDR is unsupported
 * there and the driver stays in SDR.
 *
 * We key the compile gate off the Availability.h __MAC_... / __IPHONE_...
 * version tokens rather than AvailabilityMacros.h MAC_OS_X_VERSION_*
 * constants: the former are defined consistently across all recent SDKs,
 * while the latter were phased out for newer point releases and checking
 * them fails silently even when the APIs are in fact present.
 *
 * Runtime: the HDR paths are still guarded with cached runtime version
 * checks (apple_runtime_available) because RetroArch's Apple deployment
 * targets (macOS 10.13, iOS 11) are lower than the first HDR-capable OS
 * release on each platform.  When the runtime gate is false the driver
 * stays in SDR mode.
 *
 * METAL_HDR_AVAILABLE guards compile-time only. Whenever we touch an
 * HDR-specific API inside those blocks, an apple_runtime_available
 * check guards runtime dispatch. */
#include <Availability.h>
#include <TargetConditionals.h>
#if defined(TARGET_OS_TV) && TARGET_OS_TV
#  define METAL_HDR_AVAILABLE 0
#elif TARGET_OS_OSX && defined(__MAC_11_0)
#  define METAL_HDR_AVAILABLE 1
#elif defined(HAVE_COCOATOUCH) && defined(__IPHONE_16_0)
#  define METAL_HDR_AVAILABLE 1
#else
#  define METAL_HDR_AVAILABLE 0
#endif

@interface Context : NSObject

@property (nonatomic, readonly) id<MTLDevice> device;
@property (nonatomic, readonly) id<MTLLibrary> library;
@property (nonatomic, readonly) void *frameSDRPipeline;
@property(nonatomic, readonly) BOOL movieHDRSupported;
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
/* An RGBA8 texture the planar conversion writes, at @image's size; the
 * frame is the caller's to put in (metal_planar_update). nil when the
 * conversion cannot be had. */
- (Texture *)newImportedTexture:(MTLPixelFormat)format width:(unsigned)width
      height:(unsigned)height filter:(enum texture_filter_type)filter;
- (Texture *)newPlanarTexture:(const struct texture_image *)image filter:(enum texture_filter_type)filter;
/* The planar conversion, compiled the first time it is asked for; nil
 * from then on when it did not compile */
- (id<MTLComputePipelineState>)planarKernel;
/* False once the planar conversion has failed to compile: a flag, read
 * from any thread */
- (bool)planarAvailable;
#if TARGET_OS_OSX
- (Texture *)newTextureCompressed:(const struct texture_compressed *)tc filter:(enum texture_filter_type)filter;
#endif
- (id<MTLTexture>)newTexture:(struct texture_image)image mipmapped:(bool)mipmapped;
- (void)convertFormat:(RPixelFormat)fmt from:(id<MTLTexture>)src to:(id<MTLTexture>)dst;
- (id<MTLRenderPipelineState>)getStockShader:(int)index blend:(bool)blend;
/* The overlay pipeline for an RGBA16Float (linear scRGB) texture while
 * HDR is on; nil otherwise. Takes the menu nits and the composite's
 * ExpandGamut as a float4 at fragment buffer 0. */
- (id<MTLRenderPipelineState>)linearShader;
/* What the menu composite pass shows the UI at: the menu nits and the
 * gamut expansion, for stock_fragment_linear's inverse */
- (float)hdrMenuNits;
- (unsigned)hdrExpandGamut;
/* The pipeline format of whatever UI draws now target */
- (id<MTLRenderPipelineState>)twinF16:(MTLRenderPipelineDescriptor *)psd;
/* @psd's pipeline for BGRA8, whose blending keeps coverage in alpha */
- (id<MTLRenderPipelineState>)sdrState:(MTLRenderPipelineDescriptor *)psd
                                 error:(NSError **)err;
/* What draws through rce lands in @tex, cleared, until endLayer */
- (void)beginLayer:(id<MTLTexture>)tex;
- (void)endLayer;

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
- (void)endWithRecording:(video_frame_info_t *)video_info;

/*! @brief swapBuffers acquires the next drawable, blocking if needed for vsync.
 *  This should be called after end to match Vulkan's swap_buffers timing. */
- (void)swapBuffers;

/*! @brief Lets go of the drawable just presented without taking the next
 *  one: the next frame acquires its own when it first draws. */
- (void)releaseDrawable;

/*! @brief When the last presented drawable reached the display, on the
 *  cpu_features_get_time_usec() clock; 0 when the OS cannot say. */
- (retro_time_t)lastPresentTime;

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
 * hdrOutputMode is the negotiated output encoding, or OFF when colorspace
 * setup failed. The composite shader itself also recognises mode 3
 * (PQ->scRGB) which is selected internally when the shader chain emits PQ
 * but the swapchain is scRGB.
 *
 * Viewport size is used to size the HDR offscreen + readback buffers. */
@property (nonatomic, readonly) bool hdrEnabled;
@property (nonatomic, readonly) unsigned hdrOutputMode;
#if METAL_HDR_AVAILABLE
- (void)setHDROutputMode:(unsigned)mode
             viewportWidth:(unsigned)w
            viewportHeight:(unsigned)h;
#endif

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
#if METAL_HDR_AVAILABLE
- (void)hdrComposite:(const HDRUniforms *)uniforms
          fromSource:(id<MTLTexture>)source
            rotation:(unsigned)rotation movieLinear:(BOOL)movieLinear;
#endif

/* HDR-specific setters exposed for the poke interface. */
- (void)setHDRPaperWhiteNits:(float)nits;
- (void)setHDRMenuNits:(float)nits;
- (void)setHDRExpandGamut:(unsigned)expandGamut;
- (void)setHDRScanlines:(bool)scanlines;
- (void)setHDRSubpixelLayout:(unsigned)layout;

/* (Re)allocate HDR-mode offscreen textures (readback landing pad and
 * SDR UI overlay) to match a new drawable size.  Called from
 * setViewportDims: on window resize; cheap no-op when the
 * current allocations already match. */
#if METAL_HDR_AVAILABLE
- (void)resizeHDRResourcesForWidth:(NSUInteger)w height:(NSUInteger)h;
#endif

/* Shader-emitted HDR path: set by FrameView after parsing a shader preset,
 * tells the composite fragment to pass the final pass through without
 * inverse-tonemap / PQ encode. */
- (void)setHDRShaderEmitsHDR10:(bool)emitsHDR10
                    emitsHDR16:(bool)emitsHDR16;

/* Native (no tone-map) HDR read-back for HDR screenshots: reads the raw
 * RGB10A2 / RGBA16F drawable rows and converts to three uint16_t per
 * pixel (PQ-coded, bottom-up), with the same viewport clamping as
 * readViewport:.  Reports the measured peak / average light levels and
 * which encoding the swapchain used.  Returns NO when HDR is off or
 * capture is unavailable; the caller then falls back to the SDR path. */
#if METAL_HDR_AVAILABLE
- (bool)readViewportHDR:(uint16_t *)buffer
                 maxCLL:(float *)outMaxCLL
                maxFALL:(float *)outMaxFALL
                isSCRGB:(bool *)outIsSCRGB;
#endif

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
- (BOOL)drawMesh:(const gfx_display_mesh_t *)mesh mvp:(const float *)mvp
      texture:(uintptr_t)texture tint:(const float *)tint dims:(unsigned)video_dims;
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

/* A view's copy of the preset's chain (makeViewChain) */
typedef struct metal_view_chain metal_view_chain_t;

@interface FrameView : NSObject

@property(nonatomic, readonly) RPixelFormat format;
@property(nonatomic, readonly) BOOL movieFrame;
@property(nonatomic, readonly) BOOL movieHDR;
- (enum video_texture_update)importMovieFrame:(const video_frame_import_t *)frame;
@property(nonatomic, readonly) RTextureFilter filter;
@property(nonatomic, readwrite) BOOL visible;
@property(nonatomic, readwrite) CGRect frame;
@property(nonatomic, readwrite) CGSize size;
@property(nonatomic, readonly) ViewDrawState drawState;
@property(nonatomic, readonly) struct video_shader *shader;
@property(nonatomic, readwrite) uint64_t frameCount;
/* SwapCount, TotalSubFrames and CurrentSubFrame for the shader chain,
 * taken from the frame info each render: presents the display has
 * seen before this one (advanced per shader sub-frame), the sub-frame
 * count and the 1-based index of the one being drawn. */
@property(nonatomic, readwrite) uint64_t swapCount;
@property(nonatomic, readwrite) uint32_t totalSubframes;
@property(nonatomic, readwrite) uint32_t currentSubframe;
@property(nonatomic, readwrite) float hdrPaperWhiteNits;
@property(nonatomic, readwrite) float hdrPeakNits;
@property(nonatomic, readwrite) unsigned hdrExpandGamut;

/* Final pass of the shader chain normally renders into the backbuffer
 * drawable.  When HDR is on, it must render into the HDR offscreen
 * instead so the composite pass can encode PQ/scRGB. */
@property(nonatomic, readwrite) BOOL hdrEnabled;

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
/* A frame the preset may draw as views, put off until they are known.
 * commitFrame writes it as updateFrame:pitch: would; the views' chains
 * instead write each view straight in (renderViewChain:), and dropFrame
 * lets it go. */
- (bool)deferFrame:(void const *)src pitch:(NSUInteger)pitch;
- (void)commitFrame;
- (void)dropFrame;
/* The frames from now on are in @format (a software filter changed
 * it): the 16-bit staging texture is made again or dropped */
- (void)setSourceFormat:(RPixelFormat)format;
/* GET_CURRENT_SOFTWARE_FRAMEBUFFER: lend the core a buffer the GPU
 * reads directly. See lendFramebuffer: in the implementation. */
- (bool)lendFramebuffer:(struct retro_framebuffer *)fb;
/* The threaded wrapper's frame buffers (frame_mem_alloc): shared
 * storage a frame is blitted out of where it lies */
- (void *)frameMemAlloc:(size_t)size id:(uintptr_t *)ident;
- (bool)frameMemReady:(uintptr_t)ident;
- (void)frameMemFree:(uintptr_t)ident;
- (bool)readViewport:(uint8_t *)buffer isIdle:(bool)isIdle;
- (bool)readViewportHDR:(uint16_t *)buffer
                 isIdle:(bool)isIdle
                   meta:(struct rpng_hdr_metadata *)meta;
/* Where the frame is drawn, from the viewport */
- (void)syncViewport;
/* The frame made ready to be drawn from, without the preset */
- (void)prepareFrame;
- (metal_view_chain_t *)makeViewChain;
- (void)freeViewChain:(metal_view_chain_t *)c;
- (void)renderViewChain:(metal_view_chain_t *)c
                   view:(const struct retro_video_view *)v
               viewport:(unsigned)vp_dims fresh:(bool)fresh;
/* The last pass's render target, or nil when the last pass draws into
 * each placement, bound by bindViewChainLast */
- (id<MTLTexture>)viewChainFinal:(metal_view_chain_t *)c;
- (BOOL)bindViewChainLast:(metal_view_chain_t *)c
                  encoder:(id<MTLRenderCommandEncoder>)rce;

@end

@interface MetalMenu : NSObject

@property(nonatomic, readonly) bool hasFrame;
@property(nonatomic, readwrite) bool enabled;
@property(nonatomic, readwrite) float alpha;

- (void)updateFrame:(void const *)source;

- (void)updateDims:(unsigned)dims
                 format:(RPixelFormat)format
                 filter:(RTextureFilter)filter;
@end

@interface Overlay : NSObject
@property(nonatomic, readwrite) bool enabled;
@property(nonatomic, readwrite) bool fullscreen;

- (bool)loadImages:(const struct texture_image *)images count:(NSUInteger)count;
/* A page of the overlay pack's textures (metal_load_texture handles):
 * the array retains what it shows and nothing is uploaded. */
- (bool)loadTextures:(const uintptr_t *)textures count:(NSUInteger)count;
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

/* What the last frame said the menu filter should be:
 * set_texture_frame() is applied by the video thread from
 * thread_update_driver_state(), and reading the setting there races
 * the menu writing it. Both users are MetalDriver - this was
 * declared on Context, where nothing referred to it. */
@property(nonatomic, readwrite) bool frameMenuLinearFilter;

- (instancetype)initWithVideo:(const video_info_t *)video;

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
- (void)setViewportDims:(unsigned)dims forceFull:(BOOL)forceFull allowRotate:(BOOL)allowRotate;
- (void)setRotation:(unsigned)rotation;
/*! @brief applyVideoMode sizes the window, or takes it full screen, on
 * the main thread; a zero axis means the view's current size.  Returns
 * the size applied. */
- (unsigned)applyVideoMode:(unsigned)dims fullscreen:(bool)fullscreen;

@end

#include "../../driver.h"
#include "../../configuration.h"

#include "../../retroarch.h"
#ifdef HAVE_REWIND
#include "../../state_manager.h"
#endif
#include "../../verbosity.h"

#include "../../ui/drivers/cocoa/apple_platform.h"
#include "../../ui/drivers/cocoa/cocoa_common.h"
#include <defines/cocoa_defines.h>

/* Reference ownership in this file goes through the RARCH_* ARC/MRR
 * layer in <defines/cocoa_defines.h> (RARCH_RETAIN,
 * RARCH_AUTORELEASE_R, RARCH_RELEASE_NIL, RARCH_ASSIGN, the bridge
 * transfer forms, RARCH_STRUCT_ASSIGN for the unretained engine and
 * buffer-chain slots, RARCH_WEAK, RARCH_SUPER_DEALLOC,
 * RARCH_RETURN_INIT_FAILURE); see that header for the semantics of
 * each form and for why the file must compile under both modes. */

/* METAL_FRAME_LEASE_BEGIN */
#include "../common/metal_frame.h"
#include <limits.h>

/* Local protocols keep older SDK builds independent of shared-event types. */
@protocol RArchMetalSharedEvent <NSObject>
- (id<MTLDevice>)device;
- (uint64_t)signaledValue;
- (void)setSignaledValue:(uint64_t)value;
- (void)notifyListener:(id)listener atValue:(uint64_t)value
      block:(void (^)(id, uint64_t))block;
@end

@protocol RArchMetalEventCommandBuffer <MTLCommandBuffer>
- (void)encodeWaitForEvent:(id)event value:(uint64_t)value;
- (void)encodeSignalEvent:(id)event value:(uint64_t)value;
@end

@interface RArchMetalFrameLease : NSObject
{
@public
   id<MTLDevice> device;
   id<MTLTexture> planes[2];
   id<RArchMetalSharedEvent> ready;
   id<RArchMetalSharedEvent> done;
   id listener;
   CFTypeRef backing[3];
   uint64_t ready_value, done_value;
   metal_frame_view_t view;
}
@end

@implementation RArchMetalFrameLease
- (void)dealloc
{
   unsigned i;
   for (i = 0; i < 3; i++)
      if (backing[i])
         CFRelease(backing[i]);
#if !__has_feature(objc_arc)
   [device release];
   [planes[0] release];
   [planes[1] release];
   [ready release];
   [done release];
   [listener release];
#endif
   RARCH_SUPER_DEALLOC();
}
@end

static void metal_frame_abandon(void *owner, uint64_t unused_done,
      uint64_t unused_value)
{
   RArchMetalFrameLease *lease = (__bridge RArchMetalFrameLease *)owner;
   (void)unused_done;
   (void)unused_value;
   [lease->ready notifyListener:lease->listener atValue:lease->ready_value
      block:^(id event, uint64_t value) {
         @autoreleasepool
         {
            RArchMetalFrameLease *finished =
               RARCH_BRIDGE_TRANSFER(RArchMetalFrameLease *, owner);
            (void)event;
            (void)value;
            [finished->done setSignaledValue:finished->done_value];
         }
      }];
}

static bool metal_frame_event_valid(id event, id device)
{
   return event
      && [event respondsToSelector:@selector(device)]
      && [event respondsToSelector:@selector(signaledValue)]
      && [event respondsToSelector:@selector(setSignaledValue:)]
      && [event respondsToSelector:@selector(notifyListener:atValue:block:)]
      /* Shared events can span devices and report a nil device. */
      && (![(id<RArchMetalSharedEvent>)event device]
            || [(id<RArchMetalSharedEvent>)event device] == device);
}

static bool metal_frame_texture_valid(id<MTLTexture> texture, id device,
      MTLPixelFormat format)
{
   return texture && texture.device == device
      && texture.textureType == MTLTextureType2D
      && texture.sampleCount == 1 && texture.arrayLength == 1
      && texture.depth == 1 && texture.pixelFormat == format
      && texture.width <= UINT_MAX && texture.height <= UINT_MAX;
}

bool metal_frame_create(const metal_frame_source_t *source,
      video_hw_frame_t *output)
{
   id<MTLDevice> device;
   id<MTLTexture> y, uv;
   id<RArchMetalSharedEvent> ready, done;
   MTLPixelFormat format, uv_format = MTLPixelFormatInvalid;
   Class listener_class = NSClassFromString(@"MTLSharedEventListener");
   bool planar;
   unsigned i;
   if (!source || !output || !source->device || !source->planes[0]
         || !source->done_value || !listener_class
         || ![(__bridge id)source->listener isKindOfClass:listener_class])
      return false;
   device = (__bridge id<MTLDevice>)source->device;
   y      = (__bridge id<MTLTexture>)source->planes[0];
   uv     = (__bridge id<MTLTexture>)source->planes[1];
   ready  = (__bridge id<RArchMetalSharedEvent>)source->ready;
   done   = (__bridge id<RArchMetalSharedEvent>)source->done;
   planar = source->pixfmt == IMAGE_PIXFMT_NV12
      || source->pixfmt == IMAGE_PIXFMT_P010;
   if (source->chroma_position > 2 || (!planar && source->chroma_position))
      return false;
   switch (source->pixfmt)
   {
      case IMAGE_PIXFMT_8888:
         format = y.pixelFormat;
         if (format != MTLPixelFormatBGRA8Unorm
               && format != MTLPixelFormatRGBA8Unorm)
            return false;
         break;
      case IMAGE_PIXFMT_FP16:
         format = MTLPixelFormatRGBA16Float;
         break;
      case IMAGE_PIXFMT_NV12:
         format = MTLPixelFormatR8Unorm;
         uv_format = MTLPixelFormatRG8Unorm;
         break;
      case IMAGE_PIXFMT_P010:
         format = MTLPixelFormatR16Unorm;
         uv_format = MTLPixelFormatRG16Unorm;
         break;
      default:
         return false;
   }
   if (!metal_frame_texture_valid(y, device, format)
         || (planar ? !metal_frame_texture_valid(uv, device, uv_format) : !!uv)
         || !metal_frame_geometry_valid(source->pixfmt,
            source->width, source->height, source->crop_x, source->crop_y,
            (unsigned)y.width, (unsigned)y.height,
            planar ? (unsigned)uv.width : 0,
            planar ? (unsigned)uv.height : 0)
         || !metal_frame_event_valid(ready, device)
         || !metal_frame_event_valid(done, device)
         || done.signaledValue >= source->done_value
         || (ready == done && source->done_value <= source->ready_value))
      return false;
   @autoreleasepool
   {
      RArchMetalFrameLease *lease = RARCH_AUTORELEASE_R([RArchMetalFrameLease new]);
      video_hw_frame_t frame;
      if (!lease)
         return false;
      lease->device = RARCH_RETAIN(device);
      lease->planes[0] = RARCH_RETAIN(y);
      lease->planes[1] = RARCH_RETAIN(uv);
      lease->ready = RARCH_RETAIN(ready);
      lease->done = RARCH_RETAIN(done);
      lease->listener = RARCH_RETAIN((__bridge id)source->listener);
      for (i = 0; i < 3; i++)
         if (source->backing[i])
            lease->backing[i] = CFRetain(source->backing[i]);
      lease->ready_value = source->ready_value;
      lease->done_value = source->done_value;
      lease->view.planes[0] = source->planes[0];
      lease->view.planes[1] = source->planes[1];
      lease->view.color = source->color;
      lease->view.pixfmt = source->pixfmt;
      lease->view.width = source->width;
      lease->view.height = source->height;
      lease->view.crop_x = source->crop_x;
      lease->view.crop_y = source->crop_y;
      lease->view.yuv = source->yuv;
      lease->view.chroma_position = source->chroma_position;
      memset(&frame, 0, sizeof(frame));
      frame.owner = RARCH_BRIDGE_RETAINED(lease);
      frame.image = (uint64_t)(uintptr_t)frame.owner;
      frame.wait = (uint64_t)(uintptr_t)source->ready;
      frame.done = (uint64_t)(uintptr_t)source->done;
      frame.wait_value = source->ready_value;
      frame.done_value = source->done_value;
      frame.abandon = metal_frame_abandon;
      frame.width = source->width;
      frame.height = source->height;
      frame.crop_x = source->crop_x;
      frame.crop_y = source->crop_y;
      frame.pixfmt = source->pixfmt;
      frame.yuv = source->yuv;
      frame.color = source->color;
      frame.api = VIDEO_HW_API_METAL;
      *output = frame;
   }
   return true;
}

static RArchMetalFrameLease *metal_frame_validate(
      const struct video_frame_import *frame, void *device, void *command_buffer)
{
   RArchMetalFrameLease *lease;
   id<MTLCommandBuffer> buffer = (__bridge id<MTLCommandBuffer>)command_buffer;
   if (!frame || frame->abandon != metal_frame_abandon || !frame->owner
         || frame->image != (uint64_t)(uintptr_t)frame->owner
         || !buffer)
      return nil;
   lease = (__bridge RArchMetalFrameLease *)frame->owner;
   if ((__bridge void *)lease->device != device || buffer.device != lease->device
         || buffer.status > MTLCommandBufferStatusEnqueued
         || ![buffer respondsToSelector:@selector(encodeWaitForEvent:value:)]
         || ![buffer respondsToSelector:@selector(encodeSignalEvent:value:)]
         || frame->wait != (uint64_t)(uintptr_t)(__bridge void *)lease->ready
         || frame->done != (uint64_t)(uintptr_t)(__bridge void *)lease->done
         || frame->wait_value != lease->ready_value
         || frame->done_value != lease->done_value
         || frame->pixfmt != lease->view.pixfmt
         || frame->dims != VIDEO_SCALE_PACK(lease->view.width, lease->view.height)
         || frame->crop_x != lease->view.crop_x || frame->crop_y != lease->view.crop_y
         || frame->layout || frame->array_slice || frame->plane_views[0] || frame->plane_views[1])
      return nil;
   return lease;
}

bool metal_frame_encode(const struct video_frame_import *frame,
      void *device, void *command_buffer,
      bool (*encode)(void *, const metal_frame_view_t *, void *), void *user)
{
   RArchMetalFrameLease *lease;
   id<MTLCommandBuffer> buffer = (__bridge id<MTLCommandBuffer>)command_buffer;
   void *owner;
   if (!encode || !(lease = metal_frame_validate(frame, device, command_buffer)))
      return false;
   [(id<RArchMetalEventCommandBuffer>)buffer encodeWaitForEvent:(id)lease->ready value:lease->ready_value];
   if (!encode(user, &lease->view, command_buffer))
      return false;
   owner = frame->owner;
   [buffer addCompletedHandler:^(id<MTLCommandBuffer> completed) {
      @autoreleasepool
      {
         if (completed.status == MTLCommandBufferStatusCompleted)
         {
            RArchMetalFrameLease *finished =
               RARCH_BRIDGE_TRANSFER(RArchMetalFrameLease *, owner);
            (void)finished;
         }
         else
            metal_frame_abandon(owner, 0, 0);
      }
   }];
   [(id<RArchMetalEventCommandBuffer>)buffer encodeSignalEvent:(id)lease->done value:lease->done_value];
   return true;
}

static bool metal_frame_copy_encode(void *user, const metal_frame_view_t *view,
      void *commands)
{
   id<MTLCommandBuffer> buffer = (__bridge id<MTLCommandBuffer>)commands;
   id<MTLBlitCommandEncoder> encoder = [buffer blitCommandEncoder];
   if (!encoder)
      return false;
   [encoder copyFromTexture:(__bridge id<MTLTexture>)view->planes[0]
      sourceSlice:0 sourceLevel:0
      sourceOrigin:MTLOriginMake(view->crop_x, view->crop_y, 0)
      sourceSize:MTLSizeMake(view->width, view->height, 1)
      toTexture:(__bridge id<MTLTexture>)user destinationSlice:0
      destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
   [encoder endEncoding];
   return true;
}

bool metal_frame_copy(const struct video_frame_import *frame,
      void *device, void *command_buffer, void *destination)
{
   RArchMetalFrameLease *lease = metal_frame_validate(frame, device, command_buffer);
   id<MTLTexture> texture = (__bridge id<MTLTexture>)destination;
   id<MTLCommandBuffer> buffer = (__bridge id<MTLCommandBuffer>)command_buffer;
   unsigned output;
   if (!lease || !texture || !buffer.retainedReferences)
      return false;
   output = frame->output_pixfmt ? frame->output_pixfmt : IMAGE_PIXFMT_8888;
   if ((lease->view.pixfmt != IMAGE_PIXFMT_8888 && lease->view.pixfmt != IMAGE_PIXFMT_FP16)
         || output != lease->view.pixfmt || texture == lease->planes[0]
         || !metal_frame_texture_valid(texture, lease->device, lease->planes[0].pixelFormat)
         || texture.width != lease->view.width || texture.height != lease->view.height
         || texture.mipmapLevelCount != 1)
      return false;
   return metal_frame_encode(frame, device, command_buffer,
         metal_frame_copy_encode, destination);
}
/* METAL_FRAME_LEASE_END */

/* METAL_FRAME_SDR_PIPELINE_BEGIN */
void *metal_frame_sdr_pipeline_create(void *device)
{
   static NSString *source = @
      "#include <metal_stdlib>\n"
      "using namespace metal;\n"
      "struct FrameSDRParams { uint4 dims; float4 levels, k; uint4 mode; };\n"
      "kernel void frame_sdr(texture2d<float, access::read> y [[texture(0)]],\n"
      " texture2d<float, access::read> uv [[texture(1)]],\n"
      " texture2d<float, access::write> out [[texture(2)]],\n"
      " constant FrameSDRParams &p [[buffer(0)]], uint2 id [[thread_position_in_grid]])\n"
      "{\n"
      " if (any(id >= p.dims.xy)) return;\n"
      " uint2 q = id + p.dims.zw;\n"
      " float luma = y.read(q).r * p.levels.x + p.levels.y;\n"
      " float2 chroma;\n"
      " if (p.mode.y != 0u) {\n"
      "    float2 at = float2(q) * 0.5f;\n"
      "    if (p.mode.y == 1u) at.y -= 0.25f;\n"
      "    int2 lo = int2(floor(at)); float2 f = fract(at);\n"
      "    int2 hi = min(int2(uv.get_width(), uv.get_height()) - 1,\n"
      "       int2((p.dims.xy + p.dims.zw - uint2(1)) >> 1));\n"
      "    float2 a = uv.read(uint2(clamp(lo, int2(0), hi))).rg;\n"
      "    float2 b = uv.read(uint2(clamp(lo + int2(1,0), int2(0), hi))).rg;\n"
      "    float2 c = uv.read(uint2(clamp(lo + int2(0,1), int2(0), hi))).rg;\n"
      "    float2 d = uv.read(uint2(clamp(lo + int2(1,1), int2(0), hi))).rg;\n"
      "    chroma = mix(mix(a,b,f.x), mix(c,d,f.x), f.y);\n"
      " } else chroma = uv.read(q >> 1).rg;\n"
      " float2 c = chroma * p.levels.z + p.levels.w;\n"
      " float3 rgb = saturate(float3(luma) + float3(p.k.x * c.y,\n"
      "    p.k.y * c.x + p.k.z * c.y, p.k.w * c.x));\n"
      " if (p.mode.x == 16u || p.mode.x == 18u) {\n"
      "    float3 l;\n"
      "    if (p.mode.x == 16u) {\n"
      "       float3 v = pow(rgb, float3(1.0f / 78.84375f));\n"
      "       l = 125.0f * pow(max(v - 0.8359375f, float3(0.0f)) /\n"
      "          (18.8515625f - 18.6875f * v), float3(1.0f / 0.1593017578125f));\n"
      "    } else {\n"
      "       float3 scene = select((exp((rgb - 0.55991073f) / 0.17883277f) +\n"
      "          0.28466892f) / 12.0f, rgb * rgb / 3.0f, rgb <= float3(0.5f));\n"
      "       float luminance = dot(scene, float3(0.2627f, 0.6780f, 0.0593f));\n"
      "       l = scene * (12.5f * pow(luminance, 0.2f));\n"
      "    }\n"
      "    rgb = float3(dot(l, float3(1.660491f, -0.587641f, -0.072850f)),\n"
      "       dot(l, float3(-0.124550f, 1.132900f, -0.008349f)),\n"
      "       dot(l, float3(-0.018151f, -0.100579f, 1.118730f)));\n"
      " } else if (p.mode.x == 1u) {\n"
      "    float3 l = select(pow((rgb + 0.099f) / 1.099f, float3(1.0f / 0.45f)),\n"
      "       rgb / 4.5f, rgb < float3(0.081f));\n"
      "    rgb = select(1.055f * pow(l, float3(1.0f / 2.4f)) - 0.055f,\n"
      "       12.92f * l, l <= float3(0.0031308f));\n"
      " }\n"
      " out.write(float4(rgb, 1.0f), id);\n"
      "}\n";
   id<MTLDevice> dev = (__bridge id<MTLDevice>)device;
   if (!dev)
      return NULL;
   @autoreleasepool
   {
      id<MTLLibrary> library = RARCH_AUTORELEASE_R([dev
         newLibraryWithSource:source options:nil error:NULL]);
      id<MTLFunction> function = RARCH_AUTORELEASE_R([library newFunctionWithName:@"frame_sdr"]);
      id<MTLComputePipelineState> pipeline = function ? RARCH_AUTORELEASE_R([dev
         newComputePipelineStateWithFunction:function error:NULL]) : nil;
      if (!pipeline || pipeline.maxTotalThreadsPerThreadgroup < 64)
         return NULL;
      return (void *)RARCH_BRIDGE_RETAINED(pipeline);
   }
}
/* METAL_FRAME_SDR_PIPELINE_END */

/* METAL_FRAME_SDR_CONVERT_BEGIN */
#include "../common/metal_frame_convert.h"

typedef struct metal_frame_sdr_copy
{
   void *pipeline, *destination;
   metal_frame_sdr_params_t params;
} metal_frame_sdr_copy_t;

static bool metal_frame_sdr_encode(void *user, const metal_frame_view_t *view,
      void *commands)
{
   metal_frame_sdr_copy_t *copy = (metal_frame_sdr_copy_t *)user;
   id<MTLCommandBuffer> buffer = (__bridge id<MTLCommandBuffer>)commands;
   id<MTLComputeCommandEncoder> encoder = [buffer computeCommandEncoder];
   if (!encoder)
      return false;
   [encoder setComputePipelineState:(__bridge id<MTLComputePipelineState>)copy->pipeline];
   [encoder setTexture:(__bridge id<MTLTexture>)view->planes[0] atIndex:0];
   [encoder setTexture:(__bridge id<MTLTexture>)view->planes[1] atIndex:1];
   [encoder setTexture:(__bridge id<MTLTexture>)copy->destination atIndex:2];
   [encoder setBytes:&copy->params length:sizeof(copy->params) atIndex:0];
   [encoder dispatchThreadgroups:MTLSizeMake((view->width + 7u) / 8u,
      (view->height + 7u) / 8u, 1) threadsPerThreadgroup:MTLSizeMake(8, 8, 1)];
   [encoder endEncoding];
   return true;
}

static bool metal_frame_convert_planar(const struct video_frame_import *frame,
      void *device, void *command_buffer, void *pipeline, void *destination, bool hdr)
{
   RArchMetalFrameLease *lease = metal_frame_validate(frame, device, command_buffer);
   id<MTLCommandBuffer> buffer = (__bridge id<MTLCommandBuffer>)command_buffer;
   id<MTLTexture> texture = (__bridge id<MTLTexture>)destination;
   id<MTLComputePipelineState> kernel = (__bridge id<MTLComputePipelineState>)pipeline;
   metal_frame_sdr_copy_t copy;
   if (!lease || !texture || !kernel || !buffer.retainedReferences
         || kernel.device != lease->device || kernel.maxTotalThreadsPerThreadgroup < 64
         || (frame->output_pixfmt && frame->output_pixfmt != (hdr ? IMAGE_PIXFMT_FP16 : IMAGE_PIXFMT_8888))
         || texture == lease->planes[0] || texture == lease->planes[1]
         || !metal_frame_texture_valid(texture, lease->device, hdr ? MTLPixelFormatRGBA16Float : MTLPixelFormatRGBA8Unorm)
         || texture.width != lease->view.width || texture.height != lease->view.height
         || texture.mipmapLevelCount != 1 || !(texture.usage & MTLTextureUsageShaderWrite)
         || !(hdr ? metal_frame_hdr_params(&lease->view, &copy.params)
               : metal_frame_sdr_params(&lease->view, &copy.params)))
      return false;
   copy.pipeline = pipeline;
   copy.destination = destination;
   return metal_frame_encode(frame, device, command_buffer, metal_frame_sdr_encode, &copy);
}
bool metal_frame_convert_sdr(const struct video_frame_import *frame,
      void *device, void *command_buffer, void *pipeline, void *destination)
{
   return metal_frame_convert_planar(frame, device, command_buffer, pipeline, destination, false);
}
bool metal_frame_convert_hdr(const struct video_frame_import *frame,
      void *device, void *command_buffer, void *pipeline, void *destination)
{
   return metal_frame_convert_planar(frame, device, command_buffer, pipeline, destination, true);
}
/* METAL_FRAME_SDR_CONVERT_END */

/* METAL_FRAME_CV_BEGIN */
static bool metal_frame_cv_color(CVPixelBufferRef pixels,
      video_color_metadata_t *color)
{
   struct
   {
      CFStringRef key, values[4];
      unsigned codes[4];
      unsigned *field;
   } tags[] = {
      { kCVImageBufferColorPrimariesKey,
         { kCVImageBufferColorPrimaries_ITU_R_709_2,
           kCVImageBufferColorPrimaries_ITU_R_2020, NULL, NULL },
         { 1, 9, 0, 0 }, &color->primaries },
      { kCVImageBufferTransferFunctionKey,
         { kCVImageBufferTransferFunction_ITU_R_709_2,
           kCVImageBufferTransferFunction_sRGB,
           kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ,
           kCVImageBufferTransferFunction_ITU_R_2100_HLG },
         { 1, 13, 16, 18 }, &color->transfer },
      { kCVImageBufferYCbCrMatrixKey,
         { kCVImageBufferYCbCrMatrix_ITU_R_709_2,
           kCVImageBufferYCbCrMatrix_ITU_R_2020, NULL, NULL },
         { 1, 9, 0, 0 }, &color->matrix }
   };
   unsigned i, j;
   for (i = 0; i < (sizeof(tags) / sizeof(tags[0])); i++)
   {
      /* Borrowed reads preserve the pre-macOS 12 deployment floor. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
      CFTypeRef value = CVBufferGetAttachment(pixels, tags[i].key, NULL);
#pragma clang diagnostic pop
      if (!value)
         continue;
      for (j = 0; j < (sizeof(tags[i].values) / sizeof(tags[i].values[0])); j++)
         if (tags[i].values[j] && CFEqual(value, tags[i].values[j]))
            break;
      if (j == (sizeof(tags[i].values) / sizeof(tags[i].values[0]))
            || (*tags[i].field && *tags[i].field != 2
               && *tags[i].field != tags[i].codes[j]))
         return false;
      *tags[i].field = tags[i].codes[j];
   }
   return true;
}

bool metal_frame_create_cv(const metal_frame_source_t *source,
      void *pixel_buffer, void *texture_cache, video_hw_frame_t *output)
{
   CVPixelBufferRef pixels = (CVPixelBufferRef)pixel_buffer;
   CVMetalTextureCacheRef cache = (CVMetalTextureCacheRef)texture_cache;
   CVMetalTextureRef views[2] = { NULL, NULL };
   metal_frame_source_t local;
   MTLPixelFormat formats[2];
   unsigned range, pixfmt, i;
   bool result = false;
   if (!source || !pixels || !cache || !output || source->planes[0]
         || source->planes[1] || source->backing[0] || source->backing[1]
         || source->backing[2] || !CVPixelBufferIsPlanar(pixels)
         || CVPixelBufferGetPlaneCount(pixels) != 2)
      return false;
   switch (CVPixelBufferGetPixelFormatType(pixels))
   {
      case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
      case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
         pixfmt = IMAGE_PIXFMT_NV12;
         range = CVPixelBufferGetPixelFormatType(pixels)
            == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange ? 2 : 1;
         formats[0] = MTLPixelFormatR8Unorm;
         formats[1] = MTLPixelFormatRG8Unorm;
         break;
      case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
      case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
         pixfmt = IMAGE_PIXFMT_P010;
         range = CVPixelBufferGetPixelFormatType(pixels)
            == kCVPixelFormatType_420YpCbCr10BiPlanarFullRange ? 2 : 1;
         formats[0] = MTLPixelFormatR16Unorm;
         formats[1] = MTLPixelFormatRG16Unorm;
         break;
      default:
         return false;
   }
   if (source->pixfmt != pixfmt
         || (source->color.range && source->color.range != range))
      return false;
   local = *source;
   local.color.range = range;
   if (!metal_frame_cv_color(pixels, &local.color))
      return false;
   local.yuv = (local.yuv & ~IMAGE_YUV_FLAG_FULL_RANGE)
      | (range == 2 ? IMAGE_YUV_FLAG_FULL_RANGE : 0);
   local.backing[0] = pixels;
   for (i = 0; i < 2; i++)
   {
      size_t width = CVPixelBufferGetWidthOfPlane(pixels, i);
      size_t height = CVPixelBufferGetHeightOfPlane(pixels, i);
      /* CoreVideo calls upper-left texture coordinates flipped. */
      if (!width || !height || width > UINT_MAX || height > UINT_MAX
            || CVMetalTextureCacheCreateTextureFromImage(NULL, cache, pixels,
               NULL, formats[i], width, height, i, &views[i]) != kCVReturnSuccess
            || !views[i] || !CVMetalTextureIsFlipped(views[i]))
         goto done;
      local.planes[i] = (__bridge void *)CVMetalTextureGetTexture(views[i]);
      local.backing[i + 1] = views[i];
   }
   result = metal_frame_create(&local, output);
done:
   for (i = 0; i < 2; i++)
      if (views[i])
         CFRelease(views[i]);
   return result;
}
/* METAL_FRAME_CV_END */

/* METAL_FRAME_CV_POOL_BEGIN */
struct metal_frame_cv_pool
{
   const void *device, *cache, *ready, *listener, *done[32];
   uint64_t value[32];
   unsigned count, next;
};

void metal_frame_cv_pool_free(metal_frame_cv_pool_t *pool)
{
   unsigned i;
   if (!pool) return;
   for (i = 0; i < pool->count; i++)
      if (pool->done[i]) CFRelease(pool->done[i]);
   if (pool->listener) CFRelease(pool->listener);
   if (pool->ready) CFRelease(pool->ready);
   if (pool->cache) CFRelease(pool->cache);
   if (pool->device) CFRelease(pool->device);
   free(pool);
}

metal_frame_cv_pool_t *metal_frame_cv_pool_new(void *device, void *cache,
      void *ready, void *listener, void *const *done, unsigned count)
{
   metal_frame_cv_pool_t *pool;
   unsigned i, j;
   Class listener_class = NSClassFromString(@"MTLSharedEventListener");
   if (!device || !cache || !listener_class || !listener || !done
         || !count || count > 32
         || ![(__bridge id)listener isKindOfClass:listener_class]
         || !metal_frame_event_valid((__bridge id)ready, (__bridge id)device)
         || [(id<RArchMetalSharedEvent>)(__bridge id)ready signaledValue] < 1)
      return NULL;
   for (i = 0; i < count; i++)
   {
      if (done[i] == ready
            || !metal_frame_event_valid((__bridge id)done[i], (__bridge id)device)
            || [(id<RArchMetalSharedEvent>)(__bridge id)done[i] signaledValue])
         return NULL;
      for (j = 0; j < i; j++) if (done[i] == done[j]) return NULL;
   }
   pool = (metal_frame_cv_pool_t*)calloc(1, sizeof(*pool));
   if (!pool) return NULL;
   pool->count = count;
   pool->device = CFRetain(device); pool->cache = CFRetain(cache);
   pool->ready = CFRetain(ready); pool->listener = CFRetain(listener);
   for (i = 0; i < count; i++) pool->done[i] = CFRetain(done[i]);
   return pool;
}

int metal_frame_cv_pool_export(metal_frame_cv_pool_t *pool,
      const metal_frame_view_t *metadata, void *image, video_hw_frame_t *out)
{
   unsigned n;
   if (!pool || !metadata || !image || !out
         || metadata->planes[0] || metadata->planes[1]) return -1;
   for (n = 0; n < pool->count; n++)
   {
      unsigned i = (pool->next + n) % pool->count;
      id<RArchMetalSharedEvent> done = (__bridge id<RArchMetalSharedEvent>)pool->done[i];
      metal_frame_source_t source;
      if (pool->value[i] == ~(uint64_t)0 || done.signaledValue < pool->value[i])
         continue;
      memset(&source, 0, sizeof(source));
      source.device = (void*)pool->device; source.ready = (void*)pool->ready;
      source.done = (void*)pool->done[i]; source.listener = (void*)pool->listener;
      source.ready_value = 1; source.done_value = pool->value[i] + 1;
      source.pixfmt = metadata->pixfmt; source.width = metadata->width;
      source.height = metadata->height; source.crop_x = metadata->crop_x;
      source.crop_y = metadata->crop_y; source.yuv = metadata->yuv;
      source.chroma_position = metadata->chroma_position;
      source.color = metadata->color;
      if (!metal_frame_create_cv(&source, image, (void*)pool->cache, out)) return -1;
      pool->value[i] = source.done_value; pool->next = (i + 1) % pool->count;
      return 0;
   }
   return 1;
}
/* METAL_FRAME_CV_POOL_END */

/* METAL_FRAME_CV_POOL_SETUP_BEGIN */
#include <dispatch/dispatch.h>
@protocol RArchMetalPoolDevice <MTLDevice>
- (id)newSharedEvent;
@end
@protocol RArchMetalPoolListener <NSObject>
- (id)initWithDispatchQueue:(dispatch_queue_t)queue;
@end

metal_frame_cv_pool_t *metal_frame_cv_pool_create(void *opaque, unsigned count)
{
   @autoreleasepool
   {
      id<RArchMetalPoolDevice> device = (__bridge id<RArchMetalPoolDevice>)opaque;
      Class listener_class = NSClassFromString(@"MTLSharedEventListener");
      id<RArchMetalSharedEvent> ready = nil;
      id events[32] = { nil }, listener = nil;
      void *done[32];
      CVMetalTextureCacheRef cache = NULL;
      metal_frame_cv_pool_t *pool = NULL;
      unsigned i;
      if (!device || !count || count > 32 || !listener_class
            || ![device respondsToSelector:@selector(newSharedEvent)]) return NULL;
      ready = [device newSharedEvent];
      listener = [(id<RArchMetalPoolListener>)[listener_class alloc]
         initWithDispatchQueue:dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0)];
      if (!ready || !listener
            || CVMetalTextureCacheCreate(NULL, NULL, device, NULL, &cache) != kCVReturnSuccess
            || !cache) goto done;
      [ready setSignaledValue:1];
      for (i = 0; i < count; i++)
      {
         events[i] = [device newSharedEvent];
         if (!events[i]) goto done;
         done[i] = (__bridge void*)events[i];
      }
      pool = metal_frame_cv_pool_new(opaque, cache, (__bridge void*)ready,
            (__bridge void*)listener, done, count);
done:
      if (cache) CFRelease(cache);
      for (i = 0; i < count; i++) RARCH_RELEASE(events[i]);
      RARCH_RELEASE(ready); RARCH_RELEASE(listener);
      return pool;
   }
}
/* METAL_FRAME_CV_POOL_SETUP_END */

#if (defined(HAVE_RMP4) || defined(HAVE_RWEBM)) && defined(HAVE_THREADS) \
      && ((defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101300) \
         || (defined(__IPHONE_OS_VERSION_MAX_ALLOWED) && __IPHONE_OS_VERSION_MAX_ALLOWED >= 110000) \
         || (defined(__TV_OS_VERSION_MAX_ALLOWED) && __TV_OS_VERSION_MAX_ALLOWED >= 110000))
#include "../common/metal_hevc_driver.h"
#ifdef METAL_HEVC_DRIVER
static bool metal_hevc_decode_enabled(void *unused)
{
   const settings_t *settings = config_get_ptr();
   (void)unused;
   return !settings || settings->uints.video_hw_decode != VIDEO_HW_DECODE_OFF;
}
#endif
#endif

/* video_hdr_mode values — must match the rest of RetroArch. */
#define METAL_HDR_MODE_OFF    0u
#define METAL_HDR_MODE_HDR10  1u
#define METAL_HDR_MODE_SCRGB  2u

#if METAL_HDR_AVAILABLE
/* Configure the CAMetalLayer's drawable pixel format, colour space, and
 * wantsExtendedDynamicRangeContent flag for the requested HDR mode.
 *
 * Call this BEFORE any render pipelines are compiled against the layer —
 * every MTLRenderPipelineState baked against _layer.pixelFormat needs to
 * know the final format up front.  That means pre-_initMetal for the
 * driver's own pipelines, pre-Context-construction for the menu/font
 * pipelines compiled inside _initMenuStates, and pre-setShaderFromPath
 * for slang shader pipelines.
 *
 * At RetroArch's current deploy targets (macOS 10.13 / iOS 11 / tvOS 12.1)
 * the EDR APIs aren't available without a runtime check, so this function
 * no-ops on older OSes and leaves the layer as BGRA8.  The caller can
 * inspect the returned format to see what actually got applied. */
/* Tell the display what luminances the frame carries, as the D3D and
 * Vulkan drivers do with their own metadata: the peak is the display's
 * where Use Display Peak supplies one, else the user's Peak Brightness.
 * opticalOutputScale is nits per 1.0 of the layer's values - 10,000 for
 * PQ, 80 for extended-linear scRGB, the SDR reference white the
 * extended sRGB space is defined against. CAEDRMetadata is macOS 10.15
 * and iOS 16, at or below this file's EDR floor, so the compile-time
 * gate covers it; a display that ignores the metadata behaves as
 * before. */
/* The EDR calls on the layer, sent by selector: the deployment floor
 * is below the OS that introduced them, and every caller sits behind
 * the apple_runtime_available() check above. */
/* kCGColorSpaceITUR_2100_PQ is in CoreGraphics from 10.15.4 but declared
 * for macOS 11, so it is looked up by name. NULL where it is not
 * exported: the callers then make no colour space, which they handle. */
static CFStringRef metal_colorspace_name_pq(void)
{
   static CFStringRef name;
   if (!name)
   {
      void **p = apple_rt_constant_addr("kCGColorSpaceITUR_2100_PQ");
      if (p)
         name = (CFStringRef)*p;
   }
   return name;
}

static void metal_layer_set_wants_edr(CAMetalLayer *layer, BOOL v)
{
   apple_rt_send_bool(layer, sel_registerName(
         "setWantsExtendedDynamicRangeContent:"), v);
}

static void metal_layer_set_edr_metadata(CAMetalLayer *layer, id metadata)
{
   apple_rt_send_id(layer, sel_registerName("setEDRMetadata:"), metadata);
}

/* +[CAEDRMetadata HDR10MetadataWithMinLuminance:maxLuminance:opticalOutputScale:] */
static id metal_hdr10_metadata(float min_nits, float max_nits, float scale)
{
   id cls = apple_rt_class("CAEDRMetadata");
   if (!cls)
      return nil;
   return ((id (*)(id, SEL, float, float, float))objc_msgSend)(cls,
         sel_registerName(
            "HDR10MetadataWithMinLuminance:maxLuminance:opticalOutputScale:"),
         min_nits, max_nits, scale);
}

static void metal_apply_hdr_metadata(CAMetalLayer *layer, unsigned hdr_mode)
{
   float peak;

   if (!layer || hdr_mode == METAL_HDR_MODE_OFF)
      return;
   if (!apple_runtime_available(APPLE_RUNTIME_VER(10, 15, 0),
            APPLE_RUNTIME_VER(16, 0, 0), APPLE_RUNTIME_VER(16, 0, 0)))
      return;
   if ((peak = video_driver_get_hdr_max_nits()) <= 0.0f)
      return;

   metal_layer_set_edr_metadata(layer, metal_hdr10_metadata(0.005f, peak,
         (hdr_mode == METAL_HDR_MODE_SCRGB) ? 80.0f : 10000.0f));
}

static MTLPixelFormat metal_apply_hdr_layer_config(CAMetalLayer *layer,
      unsigned hdr_mode)
{
   if (!layer)
      return MTLPixelFormatBGRA8Unorm;

   if (hdr_mode == METAL_HDR_MODE_OFF)
   {
      layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
      if (apple_runtime_available(APPLE_RUNTIME_VER(11, 0, 0), APPLE_RUNTIME_VER(16, 0, 0), APPLE_RUNTIME_VER(16, 0, 0)))
      {
         CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
         if (cs)
         {
            layer.colorspace = cs;
            CGColorSpaceRelease(cs);
         }
         metal_layer_set_wants_edr(layer, NO);
         /* Claim no luminance range in SDR */
         metal_layer_set_edr_metadata(layer, nil);
      }
      return MTLPixelFormatBGRA8Unorm;
   }

   if (apple_runtime_available(APPLE_RUNTIME_VER(11, 0, 0), APPLE_RUNTIME_VER(16, 0, 0), APPLE_RUNTIME_VER(16, 0, 0)))
   {
      MTLPixelFormat fmt = (hdr_mode == METAL_HDR_MODE_SCRGB)
         ? MTLPixelFormatRGBA16Float
         : MTLPixelFormatRGB10A2Unorm;
      CFStringRef csName = (hdr_mode == METAL_HDR_MODE_SCRGB)
         ? kCGColorSpaceExtendedLinearSRGB
         : metal_colorspace_name_pq();

      layer.pixelFormat = fmt;
      CGColorSpaceRef cs = csName ? CGColorSpaceCreateWithName(csName) : NULL;
      if (cs)
      {
         layer.colorspace = cs;
         CGColorSpaceRelease(cs);
      }
      metal_layer_set_wants_edr(layer, YES);
      metal_apply_hdr_metadata(layer, hdr_mode);
      return fmt;
   }

   /* HDR requested but OS too old: quietly fall back to SDR. */
   RARCH_WARN("[Metal] HDR requested but OS is below the minimum for EDR APIs; falling back to SDR.\n");
   layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
   return MTLPixelFormatBGRA8Unorm;
}

/* Query whether any connected display currently has EDR headroom above 1.0,
 * i.e. is HDR-capable in its current state.  This is best-effort: on
 * multi-display setups we check the main screen, mirroring Vulkan's
 * single-display assumption.  Returns false when the API isn't available,
 * so HDR support flags won't be announced on older OSes. */
static bool metal_display_supports_edr(void)
{
#if TARGET_OS_OSX
   if (apple_runtime_available(APPLE_RUNTIME_VER(10, 15, 0), 0, 0))
   {
      NSScreen *screen = [NSScreen mainScreen];
      /* Potential, not current: the value reflects what the display *could*
       * produce if EDR were enabled, not what's being used right now.
       * That's the right signal for "is HDR an available mode".  SDR-only
       * displays return exactly 1.0. */
      /* CGFloat, a double: Metal builds are 64-bit only */
      if (screen)
         return apple_rt_get_double(screen, sel_registerName(
                  "maximumPotentialExtendedDynamicRangeColorComponentValue")) > 1.0;
   }
#elif defined(HAVE_COCOATOUCH)
   /* TARGET_OS_TV / TARGET_OS_IOS are always defined to 0 or 1, not
    * just present — have to test the value, not defined-ness. */
#if TARGET_OS_TV
   /* tvOS: no NSScreen/UIScreen EDR API parallel to macOS.  The device
    * itself (Apple TV 4K) advertises HDR capability via AVDisplayCriteria
    * but that's AVFoundation, not a layer we can plumb into here without
    * a deeper refactor.  Assume HDR is available when the tvOS gate
    * passes — the user has to opt in to enable it anyway, and tvOS 16
    * is only shipping on HDR-capable hardware (Apple TV 4K). */
   if (apple_runtime_available(0, 0, APPLE_RUNTIME_VER(16, 0, 0)))
      return true;
#else
   if (apple_runtime_available(0, APPLE_RUNTIME_VER(16, 0, 0), 0))
   {
      UIScreen *screen = [UIScreen mainScreen];
      if (screen)
         return apple_rt_get_double(screen,
               sel_registerName("potentialEDRHeadroom")) > 1.0;
   }
#endif
#endif
   return false;
}
#endif /* METAL_HDR_AVAILABLE */

/*
 * COMMON
 */

static NSUInteger RPixelFormatToBPP(RPixelFormat format)
{
   if (   format == RPixelFormatB5G6R5Unorm
       || format == RPixelFormatBGRA4Unorm      )
      return 2;
   return 4;
}

/* For -debugDescription, i.e. for a human with a debugger.
 *
 * This filled a file-scope array on first call, under dispatch_once
 * with a block. There was nothing to initialise: every value is a
 * compile-time constant, and an NSString literal is a constant object
 * the compiler puts in __DATA - it is not allocated, not refcounted,
 * and needs no first call to bring it into being. So the once was
 * guarding the assignment of constants into a mutable global that only
 * existed to hold them.
 *
 * Nor was it about speed. dispatch_once's fast path is an acquire load
 * and a compare, which is what a plain flag costs too, and the only
 * caller is a debug description - a human in a debugger, or a log line
 * - never a frame. A switch is free of all of it: no global, no token,
 * no block, no bounds check separate from the default, and thread-safe
 * by construction rather than by a barrier. */
static NSString *NSStringFromRPixelFormat(RPixelFormat format)
{
   switch (format)
   {
      case RPixelFormatBGRA4Unorm:
         return @"RPixelFormatBGRA4Unorm";
      case RPixelFormatB5G6R5Unorm:
         return @"RPixelFormatB5G6R5Unorm";
      case RPixelFormatBGRA8Unorm:
         return @"RPixelFormatBGRA8Unorm";
      case RPixelFormatBGRX8Unorm:
         return @"RPixelFormatBGRX8Unorm";
      case RPixelFormatBGR10A2Unorm:
         return @"RPixelFormatBGR10A2Unorm";
      case RPixelFormatInvalid:
      case RPixelFormatCount:
      default:
         break;
   }
   return @"RPixelFormatInvalid";
}

static matrix_float4x4 make_matrix_float4x4(const float *v)
{
   simd_float4 P       = simd_make_float4(v[0], v[1], v[2], v[3]);
   v += 4;
   simd_float4 Q       = simd_make_float4(v[0], v[1], v[2], v[3]);
   v += 4;
   simd_float4 R       = simd_make_float4(v[0], v[1], v[2], v[3]);
   v += 4;
   simd_float4 S       = simd_make_float4(v[0], v[1], v[2], v[3]);
   matrix_float4x4 mat = {P, Q, R, S};
   return mat;
}

static matrix_float4x4 matrix_rotate_z(float rot)
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

static matrix_float4x4 matrix_proj_ortho(float left, float right, float top, float bottom)
{
   float sx            = 2 / (right - left);
   float sy            = 2 / (top   - bottom);
   float tx            = (right + left)   / (left   - right);
   float ty            = (top   + bottom) / (bottom - top);
   simd_float4 P       = simd_make_float4(sx,  0, 0, 0);
   simd_float4 Q       = simd_make_float4(0,  sy, 0, 0);
   simd_float4 R       = simd_make_float4(0,   0, 1, 0);
   simd_float4 S       = simd_make_float4(tx, ty, 0, 1);
   matrix_float4x4 mat = {P, Q, R, S};
   return mat;
}

/*
 * CONTEXT
 */

/* Per-frame transient-buffer suballocator.
 *
 * Plain C89 aggregates rather than Objective-C classes: allocRange runs
 * once per draw batch and the node walks run per frame, making this the
 * one piece of driver-internal state whose accessor dispatch multiplies
 * with scene complexity.  As C structs with static functions the
 * allocator inlines into its callers, drops the per-node isa/refcount
 * words, and (measured on arm64 -O2 against the class version) halves
 * the instruction count and cuts total memory operations by more than a
 * third -- objc_msgSend boundaries otherwise force the compiler to
 * reload every field after each opaque call.  The MTLBuffer / MTLDevice
 * references are owned through RARCH_STRUCT_ASSIGN, exactly like the engine's
 * unretained slots, so the ownership discipline is identical in ARC and
 * MRC builds. */
typedef struct buffer_node
{
   __unsafe_unretained id<MTLBuffer> src; /* owned: one ref per node */
   NSUInteger allocated;
   struct buffer_node *next;
} buffer_node_t;

typedef struct buffer_chain
{
   __unsafe_unretained id<MTLDevice> device; /* owned */
   NSUInteger block_len;
   buffer_node_t *head;
   NSUInteger offset;                        /* offset into *cur */
   buffer_node_t *cur;                       /* borrow into head list */
   NSUInteger length;
   NSUInteger allocated;
} buffer_chain_t;

static void buffer_chain_init(buffer_chain_t *chain,
      id<MTLDevice> device, NSUInteger block_len);
static void buffer_chain_destroy(buffer_chain_t *chain);
static bool buffer_chain_alloc_range(buffer_chain_t *chain,
      BufferRange *range, NSUInteger length);
static void buffer_chain_commit_ranges(buffer_chain_t *chain);
static void buffer_chain_discard(buffer_chain_t *chain);

/* Staging slots per streaming texture: one per frame the GPU can
 * have in flight, so a texture updated every frame finds a free slot
 * rather than dropping every frame whose predecessor's blit has not
 * finished yet. */
#define METAL_STAGING_SLOTS MAX_INFLIGHT

@interface Texture()
{
@public
   /* Slot i of the staging buffer is busy from the commit of a blit
    * that reads it until that command buffer completes. Hazard
    * tracking orders one GPU access against another; it says nothing
    * about the CPU writing a slot the GPU still reads, so a frame
    * whose next slot is busy is dropped. Set by the updating thread
    * only when clear and cleared by the completed handler only when
    * set, each through __atomic stores: the handler runs on whatever
    * thread Metal finishes on. */
   int      _stagingBusy[METAL_STAGING_SLOTS];
   unsigned _stagingNext;
   /* Slots lent to a producer (metal_texture_lend), which writes them
    * itself: an update from other memory never uses them. Touched
    * only on the thread that updates. */
   unsigned _stagingLent;
   /* Made by newPlanarTexture: its updates are planar frames */
   bool     _planar;
}
@property (nonatomic, readwrite, strong) id<MTLTexture> texture;
@property (nonatomic, readwrite, strong) id<MTLSamplerState> sampler;
/* Staging for a streaming update: the pixels go here and the GPU
 * copies them into the texture, so the copy is ordered against the
 * draws that sample it (see metal_update_texture). One buffer of
 * METAL_STAGING_SLOTS frame-sized slots, made on the first update and
 * kept, since a streaming texture updates every frame. */
@property (nonatomic, readwrite, strong) id<MTLBuffer> staging;
@end

@interface Context()
- (bool)_initConversionFilters;
#if METAL_HDR_AVAILABLE
- (bool)_initHDRPipelines;
- (void)_runHDRTonemapForReadbackInto:(id<MTLTexture>)dst;
#endif
@end

@implementation Context
{
   void *_frameSDRPipeline;
   id<MTLCommandQueue> _commandQueue;
   CAMetalLayer *_layer;
   id<CAMetalDrawable> _drawable;
   video_viewport_t _viewport;
   id<MTLSamplerState> _samplers[TEXTURE_FILTER_MIPMAP_NEAREST + 1];
   Filter *_filters[RPixelFormatCount]; /* convert to BGRA8888 */

   /* Main render pass state */
   id<MTLRenderCommandEncoder> _rce;
   /* The drawable has been drawn into this frame: a pass on it again
    * loads it */
   bool _drawableBegun;

   id<MTLCommandBuffer> _blitCommandBuffer;

   /* The planar conversion (planarKernel) */
   id<MTLComputePipelineState> _planarKernel;
   volatile bool _planarFailed;

   NSUInteger _currentChain;
   buffer_chain_t _chain[CHAIN_LENGTH];
   MTLClearColor _clearColor;

   id<MTLRenderPipelineState> _states[GFX_MAX_SHADERS][2];
   id<MTLRenderPipelineState> _clearState;
   /* The same for the HDR UI overlay, which is RGBA16Float: every UI
    * draw goes there while HDR is on (see rce), and a pipeline's colour
    * format must be its target's. getStockShader picks by hdrEnabled. */
   id<MTLRenderPipelineState> _statesF16[GFX_MAX_SHADERS][2];
   id<MTLRenderPipelineState> _clearStateF16;
   /* An RGBA16Float texture, linear scRGB, into the overlay */
   id<MTLRenderPipelineState> _linearStateF16;

   bool _captureEnabled;
   id<MTLTexture> _backBuffer;

   unsigned _rotation;
   matrix_float4x4 _mvp_no_rot;
   matrix_float4x4 _mvp;

   Uniforms _uniforms;
   Uniforms _uniformsNoRotate;

   /* HDR state.
    * Compile-time gated because wantsExtendedDynamicRangeContent /
    * kCGColorSpaceITUR_2100_PQ aren't in older SDKs, but most of the plumbing
    * below (ivars, pipelines) is independent of the SDK and could live
    * unconditionally. The METAL_HDR_AVAILABLE gate keeps the whole block
    * together — simpler to reason about. */
   bool _hdrEnabled;
   unsigned _hdrOutputMode;            /* METAL_HDR_OUTPUT_* */
   unsigned _hdrActiveOutputMode;
   MTLPixelFormat _hdrOffscreenFormat; /* RGBA16Float / RGB10A2Unorm etc. */
   MTLPixelFormat _hdrDrawableFormat;  /* swapchain format when HDR on */
   id<MTLTexture> _hdrReadbackTex;     /* BGRA8 landing pad for screenshots */
   NSUInteger _hdrReadbackW, _hdrReadbackH;

   /* SDR overlay offscreen.  In HDR mode, all menu / overlay / OSD / font
    * draws render into this BGRA8 texture via the stock SDR pipelines (all
    * compiled against BGRA8).  The HDR composite pass at end-of-frame
    * samples both the core video source and this overlay, encodes the core
    * into the HDR drawable's colour space, then alpha-blends the SDR
    * overlay on top (after applying paper-white scaling so UI brightness
    * matches the user's configured menu_nits). */
   id<MTLTexture> _sdrOverlayTex;
   NSUInteger _sdrOverlayW, _sdrOverlayH;
   /* True when the SDR overlay encoder has been opened at least once
    * this frame (and therefore the overlay has been cleared).  If no
    * UI drew anything, the overlay stays untouched from the previous
    * frame — we'd either blend in stale or uninitialised content.
    * Pass 2 of hdrComposite skips when this is false. */
   bool _sdrOverlayDirty;

   /* Composite pipelines.  Forward-HDR-encode the core source into the
    * drawable (blending off; drawable just got cleared), and then encode
    * the BGRA8 SDR overlay on top with Metal alpha blending to composite
    * the UI layer over the core.  Two variants per HDR output mode. */
   id<MTLRenderPipelineState> _hdrCompositeStateHDR10;
   id<MTLRenderPipelineState> _hdrCompositeStateSCRGB;
   id<MTLRenderPipelineState> _hdrMovieCompositeStateHDR10, _hdrMovieCompositeStateSCRGB;
   id<MTLRenderPipelineState> _hdrMenuCompositeStateHDR10;
   id<MTLRenderPipelineState> _hdrMenuCompositeStateSCRGB;
   /* Tonemap ("run HDR offscreen back down to BGRA8 for readback"). */
   id<MTLRenderPipelineState> _hdrTonemapState;
   /* Sampler used for both composite and tonemap source reads. */
   id<MTLSamplerState> _hdrSamplerLinear;

   HDRUniforms _hdrUniforms;
   /* Per-mode override flags that track the Vulkan
    * set_hdr_inverse_tonemap()/set_hdr10() calls. */
   bool _hdrShaderEmitsHDR10;
   bool _hdrShaderEmitsHDR16;
}

- (instancetype)initWithDevice:(id<MTLDevice>)d
                         layer:(CAMetalLayer *)layer
                       library:(id<MTLLibrary>)l
{
   if (self = [super init])
   {
      int i;

      _device                    = RARCH_RETAIN(d);
      if (apple_runtime_available(APPLE_RUNTIME_VER(10, 14, 0), APPLE_RUNTIME_VER(12, 0, 0), APPLE_RUNTIME_VER(12, 0, 0)))
         _frameSDRPipeline = metal_frame_sdr_pipeline_create((__bridge void *)d);
      _layer                     = RARCH_RETAIN(layer);
#ifdef RETRO_ATOMIC_HAS_64
      retro_atomic_store_release_64(&metal_presented_at, 0);
#endif
#if TARGET_OS_OSX
      _layer.framebufferOnly     = NO;
      _layer.displaySyncEnabled  = YES;
#endif
      /* Configure drawable pool for triple-buffering */
      if (apple_runtime_available(APPLE_RUNTIME_VER(10, 15, 4), APPLE_RUNTIME_VER(13, 0, 0), APPLE_RUNTIME_VER(13, 0, 0)))
         apple_rt_send_long(_layer,
               sel_registerName("setMaximumDrawableCount:"), MAX_INFLIGHT);
      _library                   = RARCH_RETAIN(l);
      _commandQueue              = [_device newCommandQueue];
      _clearColor                = MTLClearColorMake(0, 0, 0, 1);
      _uniforms.projectionMatrix = matrix_proj_ortho(0, 1, 0, 1);

      _rotation                  = 0;
      [self setRotation:0];
      _mvp_no_rot                = matrix_proj_ortho(0, 1, 0, 1);
      _mvp                       = matrix_proj_ortho(0, 1, 0, 1);

      {
         MTLSamplerDescriptor *sd = RARCH_AUTORELEASE_R([MTLSamplerDescriptor new]);

         sd.label = @"NEAREST";
         _samplers[TEXTURE_FILTER_NEAREST] = [d newSamplerStateWithDescriptor:sd];

         sd.mipFilter = MTLSamplerMipFilterNearest;
         sd.label = @"MIPMAP_NEAREST";
         _samplers[TEXTURE_FILTER_MIPMAP_NEAREST] = [d newSamplerStateWithDescriptor:sd];

         sd.mipFilter = MTLSamplerMipFilterNotMipmapped;
         sd.minFilter = MTLSamplerMinMagFilterLinear;
         sd.magFilter = MTLSamplerMinMagFilterLinear;
         sd.label = @"LINEAR";
         _samplers[TEXTURE_FILTER_LINEAR] = [d newSamplerStateWithDescriptor:sd];

         sd.mipFilter = MTLSamplerMipFilterLinear;
         sd.label = @"MIPMAP_LINEAR";
         _samplers[TEXTURE_FILTER_MIPMAP_LINEAR] = [d newSamplerStateWithDescriptor:sd];
      }

      if (![self _initConversionFilters])
         RARCH_RETURN_INIT_FAILURE();

      if (![self _initClearState])
         RARCH_RETURN_INIT_FAILURE();

      if (![self _initMenuStates])
         RARCH_RETURN_INIT_FAILURE();

      for (i = 0; i < CHAIN_LENGTH; i++)
         buffer_chain_init(&_chain[i], _device, 65536);

#if METAL_HDR_AVAILABLE
      /* HDR composite & tonemap pipelines are compiled lazily the first
       * time HDR is enabled; compiling them here would waste memory when
       * HDR is never turned on.  Just set defaults. */
      _hdrEnabled         = false;
      _hdrOutputMode      = METAL_HDR_OUTPUT_OFF;
      _hdrActiveOutputMode = METAL_HDR_OUTPUT_OFF;
      _hdrOffscreenFormat = MTLPixelFormatInvalid;
      _hdrDrawableFormat  = _layer.pixelFormat;
      _hdrUniforms.mvp             = _mvp_no_rot;
      _hdrUniforms.BrightnessNits  = 200.0f;
      _hdrUniforms.SubpixelLayout  = 0u;
      _hdrUniforms.Scanlines       = 0.0f;
      _hdrUniforms.ExpandGamut     = 0u;
      _hdrUniforms.InverseTonemap  = 1.0f;
      _hdrUniforms.HDR10           = 1.0f;
      _hdrUniforms.HDRMode         = 0u;
      _hdrUniforms.PaperWhiteNits  = 200.0f;
      _hdrUniforms.Rotation        = 0u;
      _hdrShaderEmitsHDR10 = false;
      _hdrShaderEmitsHDR16 = false;
#endif
   }
   return self;
}

- (void *)frameSDRPipeline { return _frameSDRPipeline; }
- (BOOL)movieHDRSupported
{
#if METAL_HDR_AVAILABLE
   return _hdrEnabled && _hdrMovieCompositeStateHDR10 && _hdrMovieCompositeStateSCRGB;
#else
   return NO;
#endif
}

- (void)dealloc
{
   int i;
   if (_frameSDRPipeline)
      CFRelease(_frameSDRPipeline);
   /* The buffer chains are C aggregates whose MTLBuffer references are
    * owned through RARCH_STRUCT_ASSIGN; drop them explicitly in both modes. */
   for (i = 0; i < CHAIN_LENGTH; i++)
      buffer_chain_destroy(&_chain[i]);
#if !__has_feature(objc_arc)
   {
   int j;
   for (i = 0; i < (int)(TEXTURE_FILTER_MIPMAP_NEAREST + 1); i++)
      [(id)_samplers[i] release];
   [(id)_planarKernel release];
   for (i = 0; i < (int)RPixelFormatCount; i++)
      [_filters[i] release];
   for (i = 0; i < GFX_MAX_SHADERS; i++)
   {
      for (j = 0; j < 2; j++)
      {
         [(id)_states[i][j] release];
         [(id)_statesF16[i][j] release];
      }
   }
   [(id)_clearState release];
   [(id)_clearStateF16 release];
   [(id)_linearStateF16 release];
   [(id)_commandQueue release];
   [(id)_layer release];
   [(id)_device release];
   [(id)_library release];
   [(id)_drawable release];
   [(id)_rce release];
   [(id)_blitCommandBuffer release];
   [(id)_commandBuffer release];
   [(id)_backBuffer release];
   [(id)_hdrReadbackTex release];
   [(id)_sdrOverlayTex release];
   [(id)_hdrCompositeStateHDR10 release];
   [(id)_hdrCompositeStateSCRGB release];
   [(id)_hdrMovieCompositeStateHDR10 release];
   [(id)_hdrMovieCompositeStateSCRGB release];
   [(id)_hdrMenuCompositeStateHDR10 release];
   [(id)_hdrMenuCompositeStateSCRGB release];
   [(id)_hdrTonemapState release];
   [(id)_hdrSamplerLinear release];
   }
   RARCH_SUPER_DEALLOC();
#endif
}

- (video_viewport_t *)viewport
{
   return &_viewport;
}

- (void)setViewport:(video_viewport_t *)viewport
{
   _viewport            = *viewport;
   _uniforms.outputSize = simd_make_float2(VIDEO_SCALE_W(_viewport.full_dims), VIDEO_SCALE_H(_viewport.full_dims));
}

- (Uniforms *)uniforms
{
   return &_uniforms;
}

- (Uniforms *)uniformsNoRotate
{
   return &_uniformsNoRotate;
}

#pragma mark - HDR

- (bool)hdrEnabled
{
#if METAL_HDR_AVAILABLE
   return _hdrEnabled;
#else
   return false;
#endif
}

- (unsigned)hdrOutputMode
{
#if METAL_HDR_AVAILABLE
   return _hdrActiveOutputMode;
#else
   return METAL_HDR_OUTPUT_OFF;
#endif
}

- (const HDRUniforms *)currentHDRUniforms
{
#if METAL_HDR_AVAILABLE
   return &_hdrUniforms;
#else
   return NULL;
#endif
}

- (void)setHDRPaperWhiteNits:(float)nits
{
#if METAL_HDR_AVAILABLE
   /* Clamp to avoid division-by-zero / negative paper-white in the shader.
    * The Vulkan driver does not clamp, but settings UI lets values go to 0
    * and our Tonemap function would collapse. */
   if (nits < 1.0f)
      nits = 1.0f;
   _hdrUniforms.BrightnessNits = nits;
#else
   (void)nits;
#endif
}

- (void)setHDRMenuNits:(float)nits
{
#if METAL_HDR_AVAILABLE
   /* Drives the SDR-overlay blend paper-white independently of the core
    * video paper-white (_hdrUniforms.BrightnessNits).  The composite
    * fragment scales the decoded sRGB overlay by PaperWhiteNits / 80 in
    * scRGB mode, and inverse-tonemaps the overlay to PaperWhiteNits
    * before PQ encoding in HDR10 mode.  Matches Vulkan's hdr.ubo_menu
    * uniform having its own brightness value. */
   _hdrUniforms.PaperWhiteNits = nits;
#else
   (void)nits;
#endif
}

- (void)setHDRExpandGamut:(unsigned)expandGamut
{
#if METAL_HDR_AVAILABLE
   _hdrUniforms.ExpandGamut = expandGamut;
#else
   (void)expandGamut;
#endif
}

- (void)setHDRScanlines:(bool)scanlines
{
#if METAL_HDR_AVAILABLE
   _hdrUniforms.Scanlines = scanlines ? 1.0f : 0.0f;
#else
   (void)scanlines;
#endif
}

- (void)setHDRSubpixelLayout:(unsigned)layout
{
#if METAL_HDR_AVAILABLE
   _hdrUniforms.SubpixelLayout = layout;
#else
   (void)layout;
#endif
}

- (void)setHDRShaderEmitsHDR10:(bool)emitsHDR10 emitsHDR16:(bool)emitsHDR16
{
#if METAL_HDR_AVAILABLE
   _hdrShaderEmitsHDR10 = emitsHDR10;
   _hdrShaderEmitsHDR16 = emitsHDR16;

   /* Mirror the Vulkan vulkan_set_hdr_inverse_tonemap / set_hdr10 logic:
    * when the chain already outputs in the target HDR space, the
    * composite pass bypasses inverse-tonemap + PQ encode and either
    * passes through (mode match) or converts (PQ -> scRGB, mode 3).  */
   if (_hdrOutputMode == METAL_HDR_OUTPUT_SCRGB)
   {
      if (emitsHDR16)
      {
         /* Shader emits FP16 scRGB-compatible content — passthrough. */
         _hdrUniforms.HDRMode        = METAL_HDR_OUTPUT_SCRGB;
         _hdrUniforms.InverseTonemap = 0.0f;
         _hdrUniforms.HDR10          = 0.0f;
      }
      else if (emitsHDR10)
      {
         /* Shader emits PQ HDR10 but swapchain is scRGB — convert. */
         _hdrUniforms.HDRMode        = 3u;
         _hdrUniforms.InverseTonemap = 0.0f;
         _hdrUniforms.HDR10          = 0.0f;
      }
      else
      {
         /* SDR shader output -> scRGB via inverse-tonemap. */
         _hdrUniforms.HDRMode        = METAL_HDR_OUTPUT_SCRGB;
         _hdrUniforms.InverseTonemap = 1.0f;
         _hdrUniforms.HDR10          = 0.0f;
      }
   }
   else if (_hdrOutputMode == METAL_HDR_OUTPUT_HDR10)
   {
      if (emitsHDR10 || emitsHDR16)
      {
         /* Shader emits native HDR that the swapchain can accept — passthrough. */
         _hdrUniforms.HDRMode        = METAL_HDR_OUTPUT_HDR10;
         _hdrUniforms.InverseTonemap = 0.0f;
         _hdrUniforms.HDR10          = 0.0f;
      }
      else
      {
         /* SDR shader output -> HDR10 via inverse-tonemap + PQ encode. */
         _hdrUniforms.HDRMode        = METAL_HDR_OUTPUT_HDR10;
         _hdrUniforms.InverseTonemap = 1.0f;
         _hdrUniforms.HDR10          = 1.0f;
      }
   }
   else
   {
      _hdrUniforms.HDRMode        = 0u;
      _hdrUniforms.InverseTonemap = 0.0f;
      _hdrUniforms.HDR10          = 0.0f;
   }
#else
   (void)emitsHDR10;
   (void)emitsHDR16;
#endif
}

- (void)setRotation:(unsigned)rotation
{
   matrix_float4x4 rot;
   _rotation                          = 270 * rotation;
   /* Calculate projection. */
   _mvp_no_rot                        = matrix_proj_ortho(0, 1, 0, 1);
   rot                                = matrix_rotate_z((float)(M_PI * _rotation / 180.0f));
   _mvp                               = simd_mul(rot, _mvp_no_rot);
   _uniforms.projectionMatrix         = _mvp;
   _uniformsNoRotate.projectionMatrix = _mvp_no_rot;
}

- (void)setDisplaySyncEnabled:(bool)displaySyncEnabled
{
#if TARGET_OS_OSX
   _layer.displaySyncEnabled = displaySyncEnabled;
#endif
}

- (bool)displaySyncEnabled
{
#if TARGET_OS_OSX
   return _layer.displaySyncEnabled;
#else
   return NO;
#endif
}

#pragma mark - shaders

- (id<MTLRenderPipelineState>)getStockShader:(int)index blend:(bool)blend
{
   assert(index > 0 && index < GFX_MAX_SHADERS);

   switch (index)
   {
      case VIDEO_SHADER_STOCK_BLEND:
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         break;
      default:
         index = VIDEO_SHADER_STOCK_BLEND;
         break;
   }

   return _hdrEnabled && _statesF16[index][blend ? 1 : 0]
      ? _statesF16[index][blend ? 1 : 0]
      : _states[index][blend ? 1 : 0];
}

- (id<MTLRenderPipelineState>)linearShader
{
   return _hdrEnabled ? _linearStateF16 : nil;
}

- (float)hdrMenuNits
{
   return _hdrUniforms.PaperWhiteNits;
}

- (unsigned)hdrExpandGamut
{
   return (unsigned)_hdrUniforms.ExpandGamut;
}

/* Alpha keeps coverage, which the views UI layer is composited by; the
 * drawable's alpha is not shown. @psd is left as it was. */
- (id<MTLRenderPipelineState>)sdrState:(MTLRenderPipelineDescriptor *)psd
                                 error:(NSError **)err
{
   id<MTLRenderPipelineState> st;
   MTLRenderPipelineColorAttachmentDescriptor *ca = psd.colorAttachments[0];
   MTLBlendFactor was = ca.sourceAlphaBlendFactor;
   if (ca.blendingEnabled)
      ca.sourceAlphaBlendFactor = MTLBlendFactorOne;
   st = [_device newRenderPipelineStateWithDescriptor:psd error:err];
   ca.sourceAlphaBlendFactor = was;
   return st;
}

/* @psd as made for BGRA8, made again for the RGBA16Float HDR overlay;
 * @psd is left as it was. */
- (id<MTLRenderPipelineState>)twinF16:(MTLRenderPipelineDescriptor *)psd
{
   NSError *err = nil;
   id<MTLRenderPipelineState> st;
   MTLPixelFormat was = psd.colorAttachments[0].pixelFormat;
   psd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA16Float;
   st = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   psd.colorAttachments[0].pixelFormat = was;
   return st;
}

- (MTLVertexDescriptor *)_spriteVertexDescriptor
{
   MTLVertexDescriptor *vd = RARCH_AUTORELEASE_R([MTLVertexDescriptor new]);
   vd.attributes[0].offset = 0;
   vd.attributes[0].format = MTLVertexFormatFloat2;
   vd.attributes[1].offset = offsetof(SpriteVertex, texCoord);
   vd.attributes[1].format = MTLVertexFormatFloat2;
   vd.attributes[2].offset = offsetof(SpriteVertex, color);
   vd.attributes[2].format = MTLVertexFormatUShort4Normalized;
   vd.layouts[0].stride    = sizeof(SpriteVertex);
   return vd;
}

- (bool)_initClearState
{
   NSError *err;
   MTLVertexDescriptor          *vd = [self _spriteVertexDescriptor];
   MTLRenderPipelineDescriptor *psd = RARCH_AUTORELEASE_R([MTLRenderPipelineDescriptor new]);
   psd.label                        = @"clear_state";

   MTLRenderPipelineColorAttachmentDescriptor *ca = psd.colorAttachments[0];
   /* Always BGRA8Unorm — see comment on _t_pipelineState init.  Clear is
    * invoked on the SDR overlay offscreen when HDR is on. */
   ca.pixelFormat       = MTLPixelFormatBGRA8Unorm;

   psd.vertexDescriptor = vd;
   psd.vertexFunction   = RARCH_AUTORELEASE_R([_library newFunctionWithName:@"stock_vertex"]);
   psd.fragmentFunction = RARCH_AUTORELEASE_R([_library newFunctionWithName:@"stock_fragment_color"]);

   if (!psd.vertexFunction || !psd.fragmentFunction)
   {
      RARCH_ERR("[Metal] Failed to load clear state shader functions.\n");
      return NO;
   }

   _clearState = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   _clearStateF16 = [self twinF16:psd];
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating clear pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   return YES;
}

- (bool)_initMenuStates
{
   NSError *err;
   MTLVertexDescriptor          *vd = [self _spriteVertexDescriptor];
   MTLRenderPipelineDescriptor *psd = RARCH_AUTORELEASE_R([MTLRenderPipelineDescriptor new]);
   psd.label                      = @"stock";

   MTLRenderPipelineColorAttachmentDescriptor *ca = psd.colorAttachments[0];
   /* Always BGRA8Unorm — see _t_pipelineState init.  Menu / snow / ribbon /
    * widget pipelines all render into the SDR overlay offscreen in HDR
    * mode; in SDR mode the drawable is BGRA8Unorm anyway. */
   ca.pixelFormat                 = MTLPixelFormatBGRA8Unorm;
   ca.blendingEnabled             = NO;
   ca.sourceRGBBlendFactor        = MTLBlendFactorSourceAlpha;
   ca.destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;
   ca.sourceAlphaBlendFactor      = MTLBlendFactorSourceAlpha;
   ca.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

   psd.sampleCount      = 1;
   psd.vertexDescriptor = vd;
   psd.vertexFunction   = RARCH_AUTORELEASE_R([_library newFunctionWithName:@"stock_vertex"]);
   psd.fragmentFunction = RARCH_AUTORELEASE_R([_library newFunctionWithName:@"stock_fragment"]);

   if (!psd.vertexFunction || !psd.fragmentFunction)
   {
      RARCH_ERR("[Metal] Failed to load stock shader functions.\n");
      return NO;
   }

   _states[VIDEO_SHADER_STOCK_BLEND][0] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   _statesF16[VIDEO_SHADER_STOCK_BLEND][0] = [self twinF16:psd];
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label                            = @"stock_blend";
   ca.blendingEnabled                   = YES;
   _states[VIDEO_SHADER_STOCK_BLEND][1] = [self sdrState:psd error:&err];
   _statesF16[VIDEO_SHADER_STOCK_BLEND][1] = [self twinF16:psd];
   {
      /* The blended stock pipeline with stock_fragment_linear, for the
       * HDR overlay only */
      id<MTLFunction> stockFrag = psd.fragmentFunction;
      psd.fragmentFunction = RARCH_AUTORELEASE_R([_library
            newFunctionWithName:@"stock_fragment_linear"]);
      if (psd.fragmentFunction)
         _linearStateF16 = [self twinF16:psd];
      psd.fragmentFunction = stockFrag;
   }
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   MTLFunctionConstantValues *vals;

   psd.label          = @"snow_simple";
   ca.blendingEnabled = YES;
   {
      vals            = RARCH_AUTORELEASE_R([MTLFunctionConstantValues new]);
      float values[3] = {
         1.25f,   /* baseScale */
         0.50f,   /* density   */
         0.15f,   /* speed     */
      };
      [vals setConstantValue:&values[0] type:MTLDataTypeFloat withName:@"snowBaseScale"];
      [vals setConstantValue:&values[1] type:MTLDataTypeFloat withName:@"snowDensity"];
      [vals setConstantValue:&values[2] type:MTLDataTypeFloat withName:@"snowSpeed"];
   }
   psd.fragmentFunction = RARCH_AUTORELEASE_R([_library newFunctionWithName:@"snow_fragment" constantValues:vals error:&err]);
   _states[VIDEO_SHADER_MENU_3][1] = [self sdrState:psd error:&err];
   _statesF16[VIDEO_SHADER_MENU_3][1] = [self twinF16:psd];
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label          = @"snow";
   ca.blendingEnabled = YES;
   {
      vals            = RARCH_AUTORELEASE_R([MTLFunctionConstantValues new]);
      float values[3] = {
         3.50f,   /* baseScale */
         0.70f,   /* density   */
         0.25f,   /* speed     */
      };
      [vals setConstantValue:&values[0] type:MTLDataTypeFloat withName:@"snowBaseScale"];
      [vals setConstantValue:&values[1] type:MTLDataTypeFloat withName:@"snowDensity"];
      [vals setConstantValue:&values[2] type:MTLDataTypeFloat withName:@"snowSpeed"];
   }
   psd.fragmentFunction = RARCH_AUTORELEASE_R([_library newFunctionWithName:@"snow_fragment" constantValues:vals error:&err]);
   _states[VIDEO_SHADER_MENU_4][1] = [self sdrState:psd error:&err];
   _statesF16[VIDEO_SHADER_MENU_4][1] = [self twinF16:psd];
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label                       = @"bokeh";
   ca.blendingEnabled              = YES;
   psd.fragmentFunction            = RARCH_AUTORELEASE_R([_library newFunctionWithName:@"bokeh_fragment"]);
   _states[VIDEO_SHADER_MENU_5][1] = [self sdrState:psd error:&err];
   _statesF16[VIDEO_SHADER_MENU_5][1] = [self twinF16:psd];
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label                       = @"snowflake";
   ca.blendingEnabled              = YES;
   psd.fragmentFunction            = RARCH_AUTORELEASE_R([_library newFunctionWithName:@"snowflake_fragment"]);
   _states[VIDEO_SHADER_MENU_6][1] = [self sdrState:psd error:&err];
   _statesF16[VIDEO_SHADER_MENU_6][1] = [self twinF16:psd];
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label                       = @"ribbon";
   ca.blendingEnabled              = NO;
   psd.vertexFunction              = RARCH_AUTORELEASE_R([_library newFunctionWithName:@"ribbon_vertex"]);
   psd.fragmentFunction            = RARCH_AUTORELEASE_R([_library newFunctionWithName:@"ribbon_fragment"]);
   _states[VIDEO_SHADER_MENU][0]   = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   _statesF16[VIDEO_SHADER_MENU][0] = [self twinF16:psd];
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label                       = @"ribbon_blend";
   ca.blendingEnabled              = YES;
   ca.sourceRGBBlendFactor         = MTLBlendFactorOne;
   ca.destinationRGBBlendFactor    = MTLBlendFactorOne;
   _states[VIDEO_SHADER_MENU][1]   = [self sdrState:psd error:&err];
   _statesF16[VIDEO_SHADER_MENU][1] = [self twinF16:psd];
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label                       = @"ribbon_simple";
   ca.blendingEnabled              = NO;
   psd.vertexFunction              = RARCH_AUTORELEASE_R([_library newFunctionWithName:@"ribbon_simple_vertex"]);
   psd.fragmentFunction            = RARCH_AUTORELEASE_R([_library newFunctionWithName:@"ribbon_simple_fragment"]);
   _states[VIDEO_SHADER_MENU_2][0] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   _statesF16[VIDEO_SHADER_MENU_2][0] = [self twinF16:psd];
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label                       = @"ribbon_simple_blend";
   ca.blendingEnabled              = YES;
   ca.sourceRGBBlendFactor         = MTLBlendFactorOne;
   ca.destinationRGBBlendFactor    = MTLBlendFactorOne;
   _states[VIDEO_SHADER_MENU_2][1] = [self sdrState:psd error:&err];
   _statesF16[VIDEO_SHADER_MENU_2][1] = [self twinF16:psd];
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   return YES;
}

- (bool)_initConversionFilters
{
   NSError *err = nil;
   _filters[RPixelFormatBGRA4Unorm] = [Filter newFilterWithFunctionName:@"convert_bgra4444_to_bgra8888"
      device:_device
      library:_library
      error:&err];
   if (err)
   {
      RARCH_LOG("[Metal] Unable to create \"convert_bgra4444_to_bgra8888\" conversion filter: %s.\n",
                err.localizedDescription.UTF8String);
      return NO;
   }

   _filters[RPixelFormatB5G6R5Unorm] = [Filter newFilterWithFunctionName:@"convert_rgb565_to_bgra8888"
      device:_device
      library:_library
      error:&err];
   if (err)
   {
      RARCH_LOG("[Metal] Unable to create \"convert_rgb565_to_bgra8888\" conversion filter: %s.\n",
                err.localizedDescription.UTF8String);
      return NO;
   }

   return YES;
}

#if METAL_HDR_AVAILABLE

/* Build the HDR composite + tonemap pipelines.
 *
 * Three render pipeline states are created up front:
 *   - composite(HDR10)  : offscreen (RGBA16Float) -> drawable (RGB10A2Unorm)
 *   - composite(scRGB)  : offscreen (RGBA16Float) -> drawable (RGBA16Float)
 *   - tonemap           : offscreen/drawable HDR -> BGRA8Unorm readback
 *
 * We need separate composite pipelines because the color attachment's pixel
 * format is baked into a MTLRenderPipelineState.  The fragment shader itself
 * is the same in both cases — the HDRMode uniform selects the math.
 *
 * _hdrDrawableFormat must be populated before calling this; setHDROutputMode
 * takes care of that when it transitions from OFF -> on. */
- (bool)_initHDRPipelines
{
   if (   _hdrCompositeStateHDR10    && _hdrCompositeStateSCRGB
       && _hdrMenuCompositeStateHDR10 && _hdrMenuCompositeStateSCRGB
       && _hdrTonemapState)
      return YES;

   NSError *err = nil;
   id<MTLFunction> vs = RARCH_AUTORELEASE_R([_library newFunctionWithName:@"hdr_composite_vertex"]);
   id<MTLFunction> fsComp = RARCH_AUTORELEASE_R([_library newFunctionWithName:@"hdr_composite_fragment"]);
   id<MTLFunction> fsTone = RARCH_AUTORELEASE_R([_library newFunctionWithName:@"hdr_tonemap_fragment"]);
   id<MTLFunction> fsMovie = RARCH_AUTORELEASE_R([_library newFunctionWithName:@"hdr_movie_fragment"]);
   if (!vs || !fsComp || !fsTone)
   {
      RARCH_ERR("[Metal] HDR pipelines: missing shader functions.\n");
      return NO;
   }

   /* Helper block: create one composite pipeline variant with the given
    * pixel format, label, and blend-enable flag.  Blend mode when
    * enabled matches Vulkan's HDR pipeline (SRC_ALPHA / ONE_MINUS_SRC_ALPHA
    * for both colour and alpha channels). */
   id<MTLRenderPipelineState> (^makeComposite)(MTLPixelFormat, NSString *, BOOL, id<MTLFunction>) =
   ^id<MTLRenderPipelineState>(MTLPixelFormat fmt, NSString *label, BOOL blend, id<MTLFunction> fragment)
   {
      NSError *e = nil;
      MTLRenderPipelineDescriptor *psd = RARCH_AUTORELEASE_R([MTLRenderPipelineDescriptor new]);
      psd.label            = label;
      psd.vertexFunction   = vs;
      psd.fragmentFunction = fragment;
      MTLRenderPipelineColorAttachmentDescriptor *ca = psd.colorAttachments[0];
      ca.pixelFormat = fmt;
      if (blend)
      {
         ca.blendingEnabled             = YES;
         ca.sourceRGBBlendFactor        = MTLBlendFactorSourceAlpha;
         ca.destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;
         ca.sourceAlphaBlendFactor      = MTLBlendFactorSourceAlpha;
         ca.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
      }
      id<MTLRenderPipelineState> s = [self->_device newRenderPipelineStateWithDescriptor:psd error:&e];
      if (e)
      {
         RARCH_ERR("[Metal] HDR %s pipeline: %s.\n",
                   label.UTF8String, e.localizedDescription.UTF8String);
      }
      return s;
   };

   _hdrCompositeStateHDR10     = makeComposite(MTLPixelFormatRGB10A2Unorm,
                                               @"HDR composite core (HDR10)",
                                               NO, fsComp);
   if (!_hdrCompositeStateHDR10) return NO;

   _hdrCompositeStateSCRGB     = makeComposite(MTLPixelFormatRGBA16Float,
                                               @"HDR composite core (scRGB)",
                                               NO, fsComp);
   if (!_hdrCompositeStateSCRGB) return NO;

   _hdrMenuCompositeStateHDR10 = makeComposite(MTLPixelFormatRGB10A2Unorm,
                                               @"HDR composite menu (HDR10)",
                                               YES, fsComp);
   if (!_hdrMenuCompositeStateHDR10) return NO;

   _hdrMenuCompositeStateSCRGB = makeComposite(MTLPixelFormatRGBA16Float,
                                               @"HDR composite menu (scRGB)",
                                               YES, fsComp);
   if (!_hdrMenuCompositeStateSCRGB) return NO;

   /* Optional movie pipelines must not change the core HDR startup gate. */
   if (fsMovie)
   {
      _hdrMovieCompositeStateHDR10 = makeComposite(MTLPixelFormatRGB10A2Unorm,
            @"HDR movie (linear scRGB to PQ)", NO, fsMovie);
      _hdrMovieCompositeStateSCRGB = makeComposite(MTLPixelFormatRGBA16Float,
            @"HDR movie (linear scRGB)", NO, fsMovie);
      if (!_hdrMovieCompositeStateHDR10 || !_hdrMovieCompositeStateSCRGB)
      {
         RARCH_RELEASE_NIL(_hdrMovieCompositeStateHDR10);
         RARCH_RELEASE_NIL(_hdrMovieCompositeStateSCRGB);
      }
   }

   /* Tonemap (for screenshot/readback path). */
   {
      MTLRenderPipelineDescriptor *psd = RARCH_AUTORELEASE_R([MTLRenderPipelineDescriptor new]);
      psd.label                        = @"HDR tonemap (readback)";
      psd.vertexFunction               = vs;
      psd.fragmentFunction             = fsTone;
      psd.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
      _hdrTonemapState                 = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
      if (err)
      {
         RARCH_ERR("[Metal] HDR tonemap pipeline: %s.\n",
                   err.localizedDescription.UTF8String);
         return NO;
      }
   }

   /* Dedicated linear sampler for composite/tonemap source reads.  We could
    * reuse _samplers[TEXTURE_FILTER_LINEAR] but tying ownership to the HDR
    * block keeps the resources a single cohesive group. */
   {
      MTLSamplerDescriptor *sd = RARCH_AUTORELEASE_R([MTLSamplerDescriptor new]);
      sd.label                 = @"HDR composite/tonemap";
      sd.minFilter             = MTLSamplerMinMagFilterLinear;
      sd.magFilter             = MTLSamplerMinMagFilterLinear;
      sd.sAddressMode          = MTLSamplerAddressModeClampToEdge;
      sd.tAddressMode          = MTLSamplerAddressModeClampToEdge;
      _hdrSamplerLinear        = [_device newSamplerStateWithDescriptor:sd];
   }

   return YES;
}

/* (Re)allocate the HDR readback landing-pad + SDR overlay textures to
 * match the drawable.  Called from setHDROutputMode on enable and from
 * readBackBuffer on size changes.  The composite path samples directly
 * from the shader chain's last-pass RT (or the raw frame texture) as
 * the core source, and from the SDR overlay here as the UI source. */
- (void)resizeHDRResourcesForWidth:(NSUInteger)w height:(NSUInteger)h
{
   if (!_hdrEnabled || w == 0 || h == 0)
      return;
   if (   _hdrReadbackTex && _hdrReadbackW == w && _hdrReadbackH == h
       && _sdrOverlayTex  && _sdrOverlayW  == w && _sdrOverlayH  == h)
      return;

   _hdrReadbackW = w;
   _hdrReadbackH = h;

   /* Readback landing pad — BGRA8 so the existing row-copy path in
    * readBackBuffer works unchanged.
    * We use Managed on macOS because we'll read straight from it; on iOS
    * MTLStorageModeShared is the equivalent CPU-visible mode. */
   {
      MTLTextureDescriptor *td = [MTLTextureDescriptor
                                   texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                width:w
                                                               height:h
                                                            mipmapped:NO];
#if TARGET_OS_OSX
      td.storageMode = MTLStorageModeManaged;
#else
      td.storageMode = MTLStorageModeShared;
#endif
      td.usage       = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
      id<MTLTexture> rb = RARCH_AUTORELEASE_R([_device newTextureWithDescriptor:td]);
      rb.label          = @"HDR readback";
      /* Plain strong ivar: RARCH_ASSIGN, not RARCH_STRUCT_ASSIGN -- the
       * bridge dance is for unretained struct slots and would pin an
       * extra reference on a __strong ivar under ARC. */
      RARCH_ASSIGN(_hdrReadbackTex, rb);
   }

   /* SDR overlay offscreen — BGRA8Unorm for direct compatibility with the
    * stock/menu/font/clear pipelines (all compiled against BGRA8).
    * Private storage: we only sample it from the composite fragment, never
    * CPU-read it. */
   _sdrOverlayW = w;
   _sdrOverlayH = h;
   {
      MTLTextureDescriptor *td = [MTLTextureDescriptor
                                   texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA16Float
                                                                width:w
                                                               height:h
                                                            mipmapped:NO];
      /* RGBA16Float: the UI's alpha keeps every step, and a linear
       * texture's values above menu white reach the composite. The UI
       * pipelines draw here through their F16 twins (getStockShader). */
      td.storageMode = MTLStorageModePrivate;
      td.usage       = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
      id<MTLTexture> sdr = RARCH_AUTORELEASE_R([_device newTextureWithDescriptor:td]);
      sdr.label          = @"SDR overlay (HDR)";
      RARCH_ASSIGN(_sdrOverlayTex, sdr);
   }

   _hdrUniforms.SourceSize = simd_make_float4((float)w, (float)h,
                                              1.0f / (float)w, 1.0f / (float)h);
   _hdrUniforms.OutputSize = simd_make_float4((float)w, (float)h,
                                              1.0f / (float)w, 1.0f / (float)h);
}

/* Transition between HDR-off and HDR-on, or between HDR10 and scRGB.
 * Reconfigures the CAMetalLayer pixel format + colourspace + EDR flag,
 * compiles pipelines on demand, and sizes the offscreen buffers. */
- (void)setHDROutputMode:(unsigned)mode
             viewportWidth:(unsigned)w
            viewportHeight:(unsigned)h
{
   if (mode == _hdrOutputMode && _hdrEnabled == (mode != METAL_HDR_OUTPUT_OFF))
   {
      /* Same mode, just resize. */
      [self resizeHDRResourcesForWidth:w height:h];
      return;
   }

   bool wantEnable = (mode != METAL_HDR_OUTPUT_OFF);
   unsigned output_mode;

   /* Pick drawable format + colourspace for the layer. */
   MTLPixelFormat newFmt    = _layer.pixelFormat;
   CGColorSpaceRef newCS    = NULL;
   BOOL wantEDR             = NO;

   if (wantEnable)
   {
      if (apple_runtime_available(APPLE_RUNTIME_VER(11, 0, 0), APPLE_RUNTIME_VER(16, 0, 0), APPLE_RUNTIME_VER(16, 0, 0)))
      {
         if (mode == METAL_HDR_OUTPUT_HDR10)
         {
            CFStringRef pq = metal_colorspace_name_pq();
            newFmt   = MTLPixelFormatRGB10A2Unorm;
            newCS    = pq ? CGColorSpaceCreateWithName(pq) : NULL;
            wantEDR  = YES;
         }
         else /* scRGB */
         {
            newFmt   = MTLPixelFormatRGBA16Float;
            newCS    = CGColorSpaceCreateWithName(kCGColorSpaceExtendedLinearSRGB);
            wantEDR  = YES;
         }

         if (   !_hdrCompositeStateHDR10     || !_hdrCompositeStateSCRGB
             || !_hdrMenuCompositeStateHDR10 || !_hdrMenuCompositeStateSCRGB
             || !_hdrTonemapState)
         {
            if (![self _initHDRPipelines])
            {
               if (newCS) CGColorSpaceRelease(newCS);
               RARCH_ERR("[Metal] HDR enable failed: pipelines did not compile.\n");
               return;
            }
         }
      }
      else
      {
         /* HDR requested but OS too old.  Refuse quietly. */
         RARCH_WARN("[Metal] HDR requested but the OS does not expose the required APIs.\n");
         return;
      }
   }
   else
   {
      /* Back to SDR. Reset to BGRA8Unorm — the layer's original default. */
      newFmt   = MTLPixelFormatBGRA8Unorm;
      newCS    = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
      wantEDR  = NO;
   }

   /* Apply to layer.  On HDR enable we flip wantsEDR _after_ setting format
    * + colourspace to avoid a momentary 1-frame state where the compositor
    * treats sRGB 8-bit content as extended range. */
   _layer.pixelFormat = newFmt;
#if METAL_HDR_AVAILABLE
   if (apple_runtime_available(APPLE_RUNTIME_VER(11, 0, 0), APPLE_RUNTIME_VER(16, 0, 0), APPLE_RUNTIME_VER(16, 0, 0)))
   {
      if (newCS)
         _layer.colorspace = newCS;
      metal_layer_set_wants_edr(_layer, wantEDR);
      if (wantEDR)
         metal_apply_hdr_metadata(_layer, mode);
      else
         metal_layer_set_edr_metadata(_layer, nil);
   }
#endif
   output_mode = (!wantEnable || newCS) ? mode : 0;
   _hdrActiveOutputMode = output_mode <= METAL_HDR_OUTPUT_SCRGB
      ? output_mode : METAL_HDR_OUTPUT_OFF;
   if (newCS)
      CGColorSpaceRelease(newCS);

   _hdrDrawableFormat = newFmt;
   _hdrOutputMode     = mode;
   _hdrEnabled        = wantEnable;
   video_driver_set_hdr_output_mode(output_mode);

   if (wantEnable)
   {
      _hdrOffscreenFormat = MTLPixelFormatRGBA16Float;
      [self resizeHDRResourcesForWidth:w height:h];
      /* Apply the current HDRMode + tonemap flags with the stored
       * emits-hdr state.  Bounces through the existing setter to keep
       * the logic in one place. */
      [self setHDRShaderEmitsHDR10:_hdrShaderEmitsHDR10
                       emitsHDR16:_hdrShaderEmitsHDR16];
   }
   else
   {
      _hdrOffscreenFormat = MTLPixelFormatInvalid;
      RARCH_ASSIGN(_hdrReadbackTex,  nil);
      RARCH_ASSIGN(_sdrOverlayTex,   nil);
      _hdrReadbackW = _hdrReadbackH = 0;
      _sdrOverlayW  = _sdrOverlayH  = 0;
      _hdrUniforms.HDRMode        = 0u;
      _hdrUniforms.InverseTonemap = 0.0f;
      _hdrUniforms.HDR10          = 0.0f;
   }
}

/* Composite the core video and SDR UI overlay into the drawable.
 *
 * Runs two sequential passes:
 *
 *   1. Core pass:  pipeline with blending OFF.  Source texture is the
 *                  core video (shader chain output or raw frame
 *                  texture).  The composite fragment HDR-encodes
 *                  pixels inside CoreViewport and emits transparent
 *                  black outside.  Drawable is cleared at pass start.
 *
 *   2. Menu pass:  pipeline with SRC_ALPHA / ONE_MINUS_SRC_ALPHA
 *                  blending.  Source texture is _sdrOverlayTex (the
 *                  BGRA8 offscreen that menu/overlay/OSD rendered
 *                  into).  The composite fragment HDR-encodes the
 *                  SDR pixels with the overlay's own alpha, Metal's
 *                  blender then alpha-composites over the core.
 *                  The menu pass passes PaperWhiteNits in place of
 *                  BrightnessNits so UI brightness is driven by
 *                  video_hdr_menu_nits independent of core paper-white.
 *                  This second pass is skipped (optimisation) when
 *                  _sdrOverlayTex is nil.
 *
 * Called at end-of-frame.  Ends any in-flight _rce (the SDR overlay
 * encoder), opens the drawable, runs both passes, leaves _rce = nil. */
- (void)hdrComposite:(const HDRUniforms *)uniforms
          fromSource:(id<MTLTexture>)source
            rotation:(unsigned)rotation movieLinear:(BOOL)movieLinear
{
   if (!_hdrEnabled || !uniforms)
      return;
   if (_commandBuffer == nil)
      return;

   /* Close the SDR overlay encoder so Metal flushes menu/OSD draws
    * before the menu-composite pass samples _sdrOverlayTex. */
   if (_rce)
   {
      [_rce endEncoding];
      RARCH_RELEASE_NIL(_rce);
   }

   id<CAMetalDrawable> drawable = self.nextDrawable;
   if (!drawable || !drawable.texture)
   {
      RARCH_WARN("[Metal] HDR composite: no drawable.\n");
      return;
   }
   if (_captureEnabled)
      RARCH_ASSIGN(_backBuffer, drawable.texture);

   const BOOL scRGB = (_hdrOutputMode == METAL_HDR_OUTPUT_SCRGB);

   /* --- Pass 1: core composite (blending off, clear).
    * Runs when a core source is available.  When source is nil (no core
    * has rendered yet — e.g. fresh driver init / post-reinit first
    * frame), we still need to clear the drawable to a known state
    * before pass 2 loads it, otherwise the drawable is presented with
    * uninitialised memory (visible as full green / blue on HDR). */
   if (source)
   {
      MTLRenderPassDescriptor *rpd        = RARCH_AUTORELEASE_R([MTLRenderPassDescriptor new]);
      rpd.colorAttachments[0].texture     = drawable.texture;
      rpd.colorAttachments[0].loadAction  = MTLLoadActionClear;
      rpd.colorAttachments[0].clearColor  = _clearColor;
      rpd.colorAttachments[0].storeAction = MTLStoreActionStore;

      id<MTLRenderPipelineState> state = scRGB
         ? (movieLinear ? _hdrMovieCompositeStateSCRGB : _hdrCompositeStateSCRGB)
         : (movieLinear ? _hdrMovieCompositeStateHDR10 : _hdrCompositeStateHDR10);

      /* Patch CoreViewport into a local uniforms copy; caller's buffer
       * stays untouched. */
      HDRUniforms local  = *uniforms;
      local.CoreViewport = simd_make_float4((float)VIDEO_POS_X(_viewport.pos),
                                            (float)VIDEO_POS_Y(_viewport.pos),
                                            (float)VIDEO_SCALE_W(_viewport.dims),
                                            (float)VIDEO_SCALE_H(_viewport.dims));
      /* Core content rotation.  Both sources arrive unrotated. */
      local.Rotation     = rotation & 3u;
      if (movieLinear) local.HDRMode = scRGB ? 2u : 1u;

      id<MTLRenderCommandEncoder> cre = [_commandBuffer renderCommandEncoderWithDescriptor:rpd];
      cre.label = @"HDR composite (core)";
      [cre setRenderPipelineState:state];
      [cre setFragmentBytes:&local length:sizeof(local) atIndex:0];
      [cre setFragmentTexture:source atIndex:0];
      [cre setFragmentSamplerState:_hdrSamplerLinear atIndex:0];
      [cre drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
      [cre endEncoding];
   }
   else
   {
      /* Clear-only pass.  MTLLoadActionClear with no draws gives us a
       * drawable filled with _clearColor, which pass 2's load=Load can
       * then alpha-blend the menu over. */
      MTLRenderPassDescriptor *rpd        = RARCH_AUTORELEASE_R([MTLRenderPassDescriptor new]);
      rpd.colorAttachments[0].texture     = drawable.texture;
      rpd.colorAttachments[0].loadAction  = MTLLoadActionClear;
      rpd.colorAttachments[0].clearColor  = _clearColor;
      rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
      id<MTLRenderCommandEncoder> cre = [_commandBuffer renderCommandEncoderWithDescriptor:rpd];
      cre.label = @"HDR composite (clear-only)";
      [cre endEncoding];
   }

   /* --- Pass 2: menu composite (alpha blending, load) --- */
   if (_sdrOverlayTex && _sdrOverlayDirty)
   {
      MTLRenderPassDescriptor *rpd        = RARCH_AUTORELEASE_R([MTLRenderPassDescriptor new]);
      rpd.colorAttachments[0].texture     = drawable.texture;
      rpd.colorAttachments[0].loadAction  = MTLLoadActionLoad;
      rpd.colorAttachments[0].storeAction = MTLStoreActionStore;

      id<MTLRenderPipelineState> state = scRGB
         ? _hdrMenuCompositeStateSCRGB
         : _hdrMenuCompositeStateHDR10;

      /* Menu uniforms: CoreViewport covers the FULL drawable so the
       * overlay is sampled everywhere without letterboxing.  Use the
       * SDR-shader-path uniforms (InverseTonemap+HDR10 on for HDR10
       * mode, off for scRGB) regardless of what the caller's uniforms
       * said about shader-emitted HDR — the overlay is always SDR.
       * BrightnessNits is replaced with PaperWhiteNits so UI brightness
       * is independent of core paper-white.  Mirrors Vulkan's
       * vk->hdr.ubo_menu / is_menu_composite path. */
      HDRUniforms menuUni = *uniforms;
      menuUni.CoreViewport = simd_make_float4(0.0f, 0.0f,
                                              (float)drawable.texture.width,
                                              (float)drawable.texture.height);
      menuUni.BrightnessNits = uniforms->PaperWhiteNits;
      /* The menu / OSD overlay is never rotated. */
      menuUni.Rotation       = 0u;
      if (scRGB)
      {
         /* scRGB menu pass.  Force InverseTonemap=1 to bypass the
          * shader's shader-emitted-HDR16 passthrough shortcut — the
          * overlay is always SDR-encoded BGRA8 and must run through
          * the sRGB-decode + gamut + BrightnessNits/80 scale path.
          * HDR10 stays 0 (that flag only matters in HDR10 mode). */
         menuUni.InverseTonemap = 1.0f;
         menuUni.HDR10          = 0.0f;
         menuUni.HDRMode        = METAL_HDR_OUTPUT_SCRGB;
      }
      else
      {
         /* HDR10: legacy inverse tonemap + PQ encoding. */
         menuUni.InverseTonemap = 1.0f;
         menuUni.HDR10          = 1.0f;
         menuUni.HDRMode        = METAL_HDR_OUTPUT_HDR10;
      }
      /* Ignore any shader-emitted-HDR semantics for the menu overlay —
       * it's always SDR, so suppress scanline-mode too. */
      menuUni.Scanlines      = 0.0f;

      id<MTLRenderCommandEncoder> cre = [_commandBuffer renderCommandEncoderWithDescriptor:rpd];
      cre.label = @"HDR composite (menu)";
      [cre setRenderPipelineState:state];
      [cre setFragmentBytes:&menuUni length:sizeof(menuUni) atIndex:0];
      [cre setFragmentTexture:_sdrOverlayTex atIndex:0];
      [cre setFragmentSamplerState:_hdrSamplerLinear atIndex:0];
      [cre drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
      [cre endEncoding];
   }
   /* Reset the overlay-dirty flag so next frame starts fresh.  If the
    * next frame has no UI draws, pass 2 will skip and core-only content
    * appears. */
   _sdrOverlayDirty = false;
   /* Leaves _rce = nil.  Presentation happens in MetalDriver's
    * end-of-frame via Context::end. */
}

/* Run the tonemap pipeline over whatever is currently in _backBuffer,
 * writing BGRA8 sRGB-encoded pixels into _hdrReadbackTex for the existing
 * row-by-row copy path in readBackBuffer.  Uses its own short-lived command
 * buffer so it can run synchronously inside readBackBuffer. */
- (void)_runHDRTonemapForReadbackInto:(id<MTLTexture>)dst
{
   if (!_hdrEnabled || !_hdrTonemapState || !_backBuffer || !dst)
      return;

   HDRUniforms u = _hdrUniforms;
   /* Signal the tonemap shader which backbuffer format it's reading.
    * We reuse HDRMode == 1/2 semantics so the tonemap fragment stays
    * parallel to the Vulkan hdr_tonemap.frag.  HDRMode-in-composite and
    * HDRMode-in-tonemap happen to use the same numbers. */
   u.HDRMode = (_hdrOutputMode == METAL_HDR_OUTPUT_SCRGB)
      ? METAL_HDR_OUTPUT_SCRGB : METAL_HDR_OUTPUT_HDR10;

   id<MTLCommandBuffer> cb = [_commandQueue commandBuffer];
   cb.label = @"HDR tonemap (readback)";

   MTLRenderPassDescriptor *rpd        = RARCH_AUTORELEASE_R([MTLRenderPassDescriptor new]);
   rpd.colorAttachments[0].texture     = dst;
   rpd.colorAttachments[0].loadAction  = MTLLoadActionDontCare;
   rpd.colorAttachments[0].storeAction = MTLStoreActionStore;

   id<MTLRenderCommandEncoder> cre = [cb renderCommandEncoderWithDescriptor:rpd];
   cre.label = @"HDR tonemap";
   [cre setRenderPipelineState:_hdrTonemapState];
   MTLViewport vp = { 0, 0, (double)dst.width, (double)dst.height, 0.0, 1.0 };
   [cre setViewport:vp];
   [cre setFragmentBytes:&u length:sizeof(u) atIndex:0];
   [cre setFragmentTexture:_backBuffer atIndex:0];
   [cre setFragmentSamplerState:_hdrSamplerLinear atIndex:0];
   [cre drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
   [cre endEncoding];

#if TARGET_OS_OSX
   /* Force a CPU-visible copy for Managed storage so getBytes works. */
   id<MTLBlitCommandEncoder> bce = [cb blitCommandEncoder];
   [bce synchronizeResource:dst];
   [bce endEncoding];
#endif

   [cb commit];
   [cb waitUntilCompleted];
}

#endif /* METAL_HDR_AVAILABLE */

- (Texture *)newTexture:(struct texture_image)image filter:(enum texture_filter_type)filter
{
   assert(filter >= TEXTURE_FILTER_LINEAR && filter <= TEXTURE_FILTER_MIPMAP_NEAREST);

   if (!image.pixels || !image.width || !image.height)
   {
      /* Create a dummy texture instead. */
#define T0 0xff000000u
#define T1 0xffffffffu
      static const uint32_t checkerboard[] = {
         T0, T1, T0, T1, T0, T1, T0, T1,
         T1, T0, T1, T0, T1, T0, T1, T0,
         T0, T1, T0, T1, T0, T1, T0, T1,
         T1, T0, T1, T0, T1, T0, T1, T0,
         T0, T1, T0, T1, T0, T1, T0, T1,
         T1, T0, T1, T0, T1, T0, T1, T0,
         T0, T1, T0, T1, T0, T1, T0, T1,
         T1, T0, T1, T0, T1, T0, T1, T0,
      };
#undef T0
#undef T1

      image.pixels = (uint32_t *)checkerboard;
      image.width = 8;
      image.height = 8;
      filter = TEXTURE_FILTER_MIPMAP_NEAREST;
   }

   BOOL mipmapped = filter == TEXTURE_FILTER_MIPMAP_LINEAR || filter == TEXTURE_FILTER_MIPMAP_NEAREST;
   Texture *tex   = [Texture new];
   tex.texture    = RARCH_AUTORELEASE_R([self newTexture:image mipmapped:mipmapped]);
   tex.sampler    = _samplers[filter];
   return tex;
}

- (bool)planarAvailable
{
   return !_planarFailed;
}

- (id<MTLComputePipelineState>)planarKernel
{
   NSError *err = nil;
   id<MTLLibrary> lib;
   id<MTLFunction> fn;
   id<MTLComputePipelineState> k;
   /* 8-bit 4:2:0 from a staging slot, where the update wrote it, into
    * RGBA: each pixel takes the chroma its 2x2 block shares */
   static NSString *src = @
      "#include <metal_stdlib>\n"
      "using namespace metal;\n"
      "struct PlanarParams { uint w, h, y_off, y_stride, cb_off, cr_off,"
      " c_stride, c_step; float k[8]; };\n"
      "kernel void planar_yuv420(device const uchar *src [[buffer(0)]],"
      " constant PlanarParams &p [[buffer(1)]],"
      " texture2d<float, access::write> out [[texture(0)]],"
      " uint2 id [[thread_position_in_grid]])\n"
      "{\n"
      "   if (id.x >= p.w || id.y >= p.h) return;\n"
      "   uint  co = (id.y >> 1) * p.c_stride + (id.x >> 1) * p.c_step;\n"
      "   float y  = float(src[p.y_off + id.y * p.y_stride + id.x]) / 255.0;\n"
      "   float cb = float(src[p.cb_off + co]) / 255.0 - 128.0 / 255.0;\n"
      "   float cr = float(src[p.cr_off + co]) / 255.0 - 128.0 / 255.0;\n"
      "   float3 rgb = float3(p.k[0] * y + p.k[1]) + float3(p.k[2] * cr,"
      " p.k[3] * cb + p.k[4] * cr, p.k[5] * cb);\n"
      "   out.write(float4(saturate(rgb), 1.0), id);\n"
      "}\n";
   if (_planarKernel || _planarFailed)
      return _planarKernel;
   _planarFailed = true;
   if (     !(lib = RARCH_AUTORELEASE_R([_device newLibraryWithSource:src
                  options:nil error:&err]))
         || !(fn  = RARCH_AUTORELEASE_R([lib newFunctionWithName:
                  @"planar_yuv420"]))
         || !(k   = RARCH_AUTORELEASE_R([_device
                  newComputePipelineStateWithFunction:fn error:&err])))
   {
      RARCH_WARN("[Metal] Planar conversion unavailable, frames are"
            " converted on the CPU: %s.\n",
            err ? err.localizedDescription.UTF8String : "");
      return nil;
   }
   RARCH_ASSIGN(_planarKernel, k);
   _planarFailed = false;
   return _planarKernel;
}

- (Texture *)newPlanarTexture:(const struct texture_image *)image filter:(enum texture_filter_type)filter
{
   MTLTextureDescriptor *td;
   id<MTLTexture>        t;
   Texture              *tex;
   if (!image->width || !image->height || ![self planarKernel])
      return nil;
   td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
         MTLPixelFormatRGBA8Unorm width:image->width height:image->height
         mipmapped:NO];
   td.usage       = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
   td.storageMode = MTLStorageModePrivate;
   if (!(t = [_device newTextureWithDescriptor:td]))
      return nil;
   tex          = [Texture new];
   tex.texture  = RARCH_AUTORELEASE_R(t);
   tex.sampler  = _samplers[(filter == TEXTURE_FILTER_NEAREST
         || filter == TEXTURE_FILTER_MIPMAP_NEAREST)
      ? TEXTURE_FILTER_NEAREST : TEXTURE_FILTER_LINEAR];
   tex->_planar = true;
   return tex;
}

- (Texture *)newImportedTexture:(MTLPixelFormat)format width:(unsigned)width
      height:(unsigned)height filter:(enum texture_filter_type)filter
{
   MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
      width:width height:height mipmapped:NO];
   desc.storageMode = MTLStorageModePrivate;
   desc.usage = MTLTextureUsageShaderRead;
   if (format == MTLPixelFormatRGBA8Unorm)
      desc.usage |= MTLTextureUsageShaderWrite;
   id<MTLTexture> image = RARCH_AUTORELEASE_R([_device newTextureWithDescriptor:desc]);
   if (!image)
      return nil;
   Texture *texture = [Texture new];
   texture.texture = image;
   texture.sampler = _samplers[filter];
   return texture;
}

- (id<MTLTexture>)newTexture:(struct texture_image)image mipmapped:(bool)mipmapped
{
   /* A 10-bit (XRGB2101010) source uses BGR10A2Unorm, whose packed layout
    * (R in the high 10 bits, B in the low) matches the ABI with no swizzle,
    * exactly as in the Metal source-frame path; otherwise BGRA8. Both are 4
    * bytes/pixel so the row stride is unchanged. */
   /* RGBA half floats (fp16) are RGBA16Float, 8 bytes a pixel in the
    * same R,G,B,A order. */
   MTLPixelFormat        pf = image.fp16
         ? MTLPixelFormatRGBA16Float
         : image.pix10
         ? MTLPixelFormatBGR10A2Unorm
         : MTLPixelFormatBGRA8Unorm;
   MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pf
         width:image.width
         height:image.height
         mipmapped:mipmapped];

   id<MTLTexture> t = [_device newTextureWithDescriptor:td];
   [t replaceRegion:MTLRegionMake2D(0, 0, image.width, image.height)
        mipmapLevel:0
        withBytes:image.pixels
        bytesPerRow:(image.fp16 ? 8 : 4) * image.width];

   if (mipmapped)
   {
      id<MTLCommandBuffer> cb = self.blitCommandBuffer;
      id<MTLBlitCommandEncoder> bce = [cb blitCommandEncoder];
      [bce generateMipmapsForTexture:t];
      [bce endEncoding];
   }

   return t;
}

#if TARGET_OS_OSX
- (Texture *)newTextureCompressed:(const struct texture_compressed *)tc
      filter:(enum texture_filter_type)filter
{
   MTLPixelFormat        pf;
   MTLTextureDescriptor *td;
   id<MTLTexture>        t;
   Texture              *tex;
   unsigned              i;
   unsigned              block_bytes = 16;

   switch (tc->format)
   {
      case TEXTURE_GPU_FORMAT_BC1: pf = MTLPixelFormatBC1_RGBA;    block_bytes = 8; break;
      case TEXTURE_GPU_FORMAT_BC2: pf = MTLPixelFormatBC2_RGBA;                     break;
      case TEXTURE_GPU_FORMAT_BC3: pf = MTLPixelFormatBC3_RGBA;                     break;
      case TEXTURE_GPU_FORMAT_BC7: pf = MTLPixelFormatBC7_RGBAUnorm;                break;
      default:                     return nil;
   }

   td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pf
         width:tc->mips[0].width
         height:tc->mips[0].height
         mipmapped:NO];
   td.mipmapLevelCount = tc->num_mips;

   t = [_device newTextureWithDescriptor:td];
   for (i = 0; i < tc->num_mips; i++)
   {
      NSUInteger blocks_w = (tc->mips[i].width + 3u) >> 2;
      [t replaceRegion:MTLRegionMake2D(0, 0, tc->mips[i].width, tc->mips[i].height)
           mipmapLevel:i
             withBytes:tc->mips[i].data
           bytesPerRow:blocks_w * block_bytes];
   }

   tex         = [Texture new];
   tex.texture = RARCH_AUTORELEASE_R(t);
   tex.sampler = _samplers[filter];
   return tex;
}
#endif

- (id<CAMetalDrawable>)nextDrawable
{
   if (_drawable == nil)
      RARCH_ASSIGN(_drawable, _layer.nextDrawable);
   return _drawable;
}

- (void)convertFormat:(RPixelFormat)fmt from:(id<MTLTexture>)src to:(id<MTLTexture>)dst
{
   if (src.width != dst.width || src.height != dst.height)
   {
      RARCH_ERR("[Metal] convertFormat: texture dimensions mismatch\n");
      return;
   }
   if (fmt < 0 || fmt >= RPixelFormatCount)
   {
      RARCH_ERR("[Metal] convertFormat: invalid pixel format %u\n", (unsigned)fmt);
      return;
   }
   Filter *conv = _filters[fmt];
   if (!conv)
   {
      RARCH_ERR("[Metal] convertFormat: no filter for format %u\n", (unsigned)fmt);
      return;
   }
   [conv apply:self.blitCommandBuffer in:src out:dst];
}

- (id<MTLCommandBuffer>)blitCommandBuffer
{
   if (!_blitCommandBuffer)
   {
      RARCH_ASSIGN(_blitCommandBuffer, [_commandQueue commandBuffer]);
      _blitCommandBuffer.label = @"Blit command buffer";
   }
   return _blitCommandBuffer;
}

- (void)_nextChain
{
   _currentChain = (_currentChain + 1) % CHAIN_LENGTH;
   buffer_chain_discard(&_chain[_currentChain]);
}

- (void)setCaptureEnabled:(bool)captureEnabled
{
   if (_captureEnabled == captureEnabled)
      return;

   _captureEnabled        = captureEnabled;
#if 0
   _layer.framebufferOnly = !captureEnabled;
#endif
}

- (bool)captureEnabled
{
   return _captureEnabled;
}

- (bool)readBackBuffer:(uint8_t *)buffer
{
   /* Read back the viewport region BGRA -> BGR and flip vertically.
    *
    * Rows are streamed from Metal in bounded chunks: each getBytes
    * fetches as many whole rows as fit in the scratch buffer (up to
    * METAL_READBACK_CHUNK bytes), and the BGRA->BGR conversion walks
    * the chunk.  The previous per-row streaming issued one Metal API
    * call per scanline -- 1000+ calls per frame at 1080p, and this
    * path runs every frame while recording through read_viewport.
    * Chunking cuts that to a handful of calls while keeping the
    * transient allocation bounded (the original full-frame copy this
    * replaced was ~32 MiB for a 4K capture; the chunk is at most
    * 1 MiB, and small captures stay on the stack). */
#define METAL_READBACK_CHUNK (1024 * 1024)
   size_t y;
   NSUInteger rowBytes, dstStride, chunkRows, chunkBytes;
   uint8_t *dst;
   uint8_t  stackRow[16 * 1024];
   uint8_t *row        = stackRow;
   uint8_t *heapRow    = NULL;
   /* Texture we actually pull rows from.  In SDR mode this is _backBuffer
    * directly (BGRA8Unorm drawable).  In HDR mode it's _hdrReadbackTex,
    * populated by the tonemap pipeline below — the drawable itself is
    * RGB10A2Unorm or RGBA16Float and can't be read as BGRA8.
    * This is the key read_viewport fix for the HDR off / HDR10 / scRGB
    * tri-state: all three modes converge on the same BGRA8 path here. */
   id<MTLTexture> srcTex = _backBuffer;

   if (!_captureEnabled || _backBuffer == nil)
      return NO;

#if METAL_HDR_AVAILABLE
   if (_hdrEnabled)
   {
      /* Size the readback texture to match the drawable (which is what
       * _backBuffer points at during an active frame).  viewport.width/x
       * etc. index into it after the copy. */
      [self resizeHDRResourcesForWidth:_backBuffer.width height:_backBuffer.height];
      if (!_hdrReadbackTex)
      {
         RARCH_WARN("[Metal] HDR readback: no landing pad texture.\n");
         return NO;
      }
      [self _runHDRTonemapForReadbackInto:_hdrReadbackTex];
      srcTex = _hdrReadbackTex;
   }
#endif

   if (srcTex.pixelFormat != MTLPixelFormatBGRA8Unorm)
   {
      RARCH_WARN("[Metal] Unexpected pixel format %d for readback.\n",
                 (int)srcTex.pixelFormat);
      return NO;
   }

   rowBytes  = srcTex.width * 4;

   /* Chunk geometry: whole rows only, at least one row, at most
    * METAL_READBACK_CHUNK bytes.  Small captures fit the stack
    * buffer outright; anything larger takes a single bounded heap
    * allocation for the whole readback. */
   chunkRows = 1;
   if (rowBytes < METAL_READBACK_CHUNK)
      chunkRows = METAL_READBACK_CHUNK / rowBytes;
   chunkBytes = chunkRows * rowBytes;
   if (chunkBytes > sizeof(stackRow))
   {
      heapRow = (uint8_t *)malloc(chunkBytes);
      if (heapRow)
         row = heapRow;
      else
      {
         /* Degrade to whatever number of whole rows the stack
          * buffer holds; fail only if not even one row fits. */
         chunkRows = sizeof(stackRow) / rowBytes;
         if (!chunkRows)
            return NO;
      }
   }

   dstStride = VIDEO_SCALE_W(_viewport.dims) * 3;
   dst       = buffer + (VIDEO_SCALE_H(_viewport.dims) - 1) * dstStride;

   /* With "Smart"/"Overscale" integer scaling the viewport deliberately
    * overscans the drawable: _viewport.x / _viewport.y may be negative and
    * _viewport.width / _viewport.height may exceed the source texture.
    * Indexing srcTex at the raw viewport offsets then walks off the texture
    * (Metal asserts "Region height OOB" on AGX and segfaults inside the AMD
    * driver, see issue #19038), and the in-row copy reads past the scratch
    * row.  Clamp every access to the texture bounds and leave the off-screen
    * remainder black; the screenshot task crops the saved image back to the
    * on-screen size afterwards. */
   {
      int texW      = (int)srcTex.width;
      int texH      = (int)srcTex.height;
      int chunkBase = 0;  /* first source row currently in the chunk */
      int chunkEnd  = 0;  /* one past the last source row in the chunk */
      /* Output-column span [colStart, colEnd) whose source column
       * (_viewport.x + x) lands inside [0, texW). */
      int colStart = -VIDEO_POS_X(_viewport.pos);
      int colEnd   = texW - VIDEO_POS_X(_viewport.pos);
      if (colStart < 0)
         colStart = 0;
      if (colEnd > (int)VIDEO_SCALE_W(_viewport.dims))
         colEnd   = (int)VIDEO_SCALE_W(_viewport.dims);

      for (y = 0; y < VIDEO_SCALE_H(_viewport.dims); y++, dst -= dstStride)
      {
         size_t x;
         const uint8_t *src_row;
         int    srcRow = VIDEO_POS_Y(_viewport.pos) + (int)y;

         /* Row entirely outside the texture (top/bottom overscan) or no
          * horizontally-visible columns: emit a black scanline. */
         if (srcRow < 0 || srcRow >= texH || colEnd <= colStart)
         {
            memset(dst, 0, dstStride);
            continue;
         }

         /* Refill the chunk when the needed row is outside it.  The
          * visible source rows advance monotonically with y, so each
          * row is fetched exactly once. */
         if (srcRow < chunkBase || srcRow >= chunkEnd)
         {
            chunkBase = srcRow;
            chunkEnd  = srcRow + (int)chunkRows;
            if (chunkEnd > texH)
               chunkEnd = texH;
            [srcTex getBytes:row
                 bytesPerRow:rowBytes
                  fromRegion:MTLRegionMake2D(0, (NSUInteger)chunkBase,
                                             (NSUInteger)texW,
                                             (NSUInteger)(chunkEnd - chunkBase))
                 mipmapLevel:0];
         }
         src_row = row + (size_t)(srcRow - chunkBase) * rowBytes;

         /* Black out left/right overscan before copying the visible span. */
         if (colStart > 0 || colEnd < (int)VIDEO_SCALE_W(_viewport.dims))
            memset(dst, 0, dstStride);

         for (x = (size_t)colStart; x < (size_t)colEnd; x++)
         {
            int srcCol     = VIDEO_POS_X(_viewport.pos) + (int)x;
            dst[3 * x + 0] = src_row[4 * srcCol + 0];
            dst[3 * x + 1] = src_row[4 * srcCol + 1];
            dst[3 * x + 2] = src_row[4 * srcCol + 2];
         }
      }
   }

   free(heapRow);

   return YES;
#undef METAL_READBACK_CHUNK
}

#if METAL_HDR_AVAILABLE
/* CPU-side HDR pixel helpers for the native HDR read-back.  Verbatim
 * ports of the vulkan driver's (unit-tested) helpers -- see
 * vulkan_read_viewport_hdr -- so every driver produces equivalent HDR
 * PNGs from identical backbuffers. */
static float metal_hdr_half_to_float(uint16_t h)
{
   uint32_t sign = (uint32_t)(h & 0x8000) << 16;
   uint32_t exp  = (h >> 10) & 0x1F;
   uint32_t mant = h & 0x3FF;
   uint32_t f;
   float    out;
   if (exp == 0)
   {
      if (mant == 0)
         f = sign;
      else
      {
         exp = 127 - 15 + 1;
         while (!(mant & 0x400)) { mant <<= 1; exp--; }
         mant &= 0x3FF;
         f = sign | (exp << 23) | (mant << 13);
      }
   }
   else if (exp == 0x1F)
      f = sign | 0x7F800000 | (mant << 13);
   else
      f = sign | ((exp - 15 + 127) << 23) | (mant << 13);
   memcpy(&out, &f, sizeof(out));
   return out;
}

/* ST.2084 (PQ) inverse-EOTF: normalised linear [0,1] -> PQ code [0,1]. */
static float metal_hdr_pq_encode(float v)
{
   const float m1 = 0.1593017578125f, m2 = 78.84375f;
   const float c1 = 0.8359375f, c2 = 18.8515625f, c3 = 18.6875f;
   float yp;
   if (v < 0.0f) v = 0.0f;
   else if (v > 1.0f) v = 1.0f;
   yp = powf(v, m1);
   return powf((c1 + c2 * yp) / (1.0f + c3 * yp), m2);
}

/* One scRGB (Rec.709 linear, 1.0 = 80 nits) channel -> 16-bit PQ code. */
static uint16_t metal_hdr_scrgb_to_pq16(float scrgb)
{
   float nits = scrgb * 80.0f;
   float pq;
   if (nits < 0.0f) nits = 0.0f;
   else if (nits > 10000.0f) nits = 10000.0f;
   pq = metal_hdr_pq_encode(nits / 10000.0f);
   if (pq < 0.0f) pq = 0.0f;
   else if (pq > 1.0f) pq = 1.0f;
   return (uint16_t)(pq * 65535.0f + 0.5f);
}

/* ST.2084 EOTF: PQ code [0,1] -> nits, for MaxCLL / MaxFALL. */
static float metal_hdr_pq_to_nits(float pq)
{
   const float m1 = 0.1593017578125f, m2 = 78.84375f;
   const float c1 = 0.8359375f, c2 = 18.8515625f, c3 = 18.6875f;
   float np, num, den;
   if (pq <= 0.0f)
      return 0.0f;
   np  = powf(pq, 1.0f / m2);
   num = np - c1;
   den = c2 - c3 * np;
   if (num < 0.0f) num = 0.0f;
   return powf(num / den, 1.0f / m1) * 10000.0f;
}

- (bool)readViewportHDR:(uint16_t *)buffer
                 maxCLL:(float *)outMaxCLL
                maxFALL:(float *)outMaxFALL
                isSCRGB:(bool *)outIsSCRGB
{
#define METAL_READBACK_CHUNK (1024 * 1024)
   size_t y;
   NSUInteger rowBytes, chunkRows, chunkBytes;
   uint16_t *dst;
   size_t dstStride;
   uint8_t  stackRow[32 * 1024];
   uint8_t *row     = stackRow;
   uint8_t *heapRow = NULL;
   bool     isSCRGB;
   float    maxCLL  = 0.0f;
   double   sumFALL = 0.0;
   id<MTLTexture> srcTex = _backBuffer;

   if (!_hdrEnabled || !_captureEnabled || srcTex == nil)
      return NO;

   if (srcTex.pixelFormat == MTLPixelFormatRGBA16Float)
      isSCRGB = true;
   else if (srcTex.pixelFormat == MTLPixelFormatRGB10A2Unorm)
      isSCRGB = false;
   else
      return NO;

   /* Native rows: 8 bytes/px for RGBA16F, 4 for RGB10A2.  Fetched in
    * bounded multi-row chunks like readBackBuffer: -- one Metal call
    * per chunk instead of one per scanline. */
   rowBytes  = srcTex.width * (isSCRGB ? 8 : 4);
   chunkRows = 1;
   if (rowBytes < METAL_READBACK_CHUNK)
      chunkRows = METAL_READBACK_CHUNK / rowBytes;
   chunkBytes = chunkRows * rowBytes;
   if (chunkBytes > sizeof(stackRow))
   {
      heapRow = (uint8_t *)malloc(chunkBytes);
      if (heapRow)
         row = heapRow;
      else
      {
         chunkRows = sizeof(stackRow) / rowBytes;
         if (!chunkRows)
            return NO;
      }
   }

   dstStride = (size_t)VIDEO_SCALE_W(_viewport.dims) * 3;
   dst       = buffer + (size_t)(VIDEO_SCALE_H(_viewport.dims) - 1) * dstStride;

   /* Same overscan clamping as readViewport: (issue #19038): off-texture
    * rows / columns are written as black (PQ code 0). */
   {
      int texW      = (int)srcTex.width;
      int texH      = (int)srcTex.height;
      int chunkBase = 0;
      int chunkEnd  = 0;
      int colStart  = -VIDEO_POS_X(_viewport.pos);
      int colEnd    = texW - VIDEO_POS_X(_viewport.pos);
      if (colStart < 0)
         colStart = 0;
      if (colEnd > (int)VIDEO_SCALE_W(_viewport.dims))
         colEnd   = (int)VIDEO_SCALE_W(_viewport.dims);

      for (y = 0; y < VIDEO_SCALE_H(_viewport.dims); y++, dst -= dstStride)
      {
         size_t x;
         const uint8_t *src_row;
         int    srcRow = VIDEO_POS_Y(_viewport.pos) + (int)y;

         if (srcRow < 0 || srcRow >= texH || colEnd <= colStart)
         {
            memset(dst, 0, dstStride * sizeof(uint16_t));
            continue;
         }

         if (srcRow < chunkBase || srcRow >= chunkEnd)
         {
            chunkBase = srcRow;
            chunkEnd  = srcRow + (int)chunkRows;
            if (chunkEnd > texH)
               chunkEnd = texH;
            [srcTex getBytes:row
                 bytesPerRow:rowBytes
                  fromRegion:MTLRegionMake2D(0, (NSUInteger)chunkBase,
                                             (NSUInteger)texW,
                                             (NSUInteger)(chunkEnd - chunkBase))
                 mipmapLevel:0];
         }
         src_row = row + (size_t)(srcRow - chunkBase) * rowBytes;

         if (colStart > 0 || colEnd < (int)VIDEO_SCALE_W(_viewport.dims))
            memset(dst, 0, dstStride * sizeof(uint16_t));

         if (isSCRGB)
         {
            const uint16_t *srcPx = (const uint16_t *)src_row;
            for (x = (size_t)colStart; x < (size_t)colEnd; x++)
            {
               int   srcCol = VIDEO_POS_X(_viewport.pos) + (int)x;
               float r      = metal_hdr_half_to_float(srcPx[4 * srcCol + 0]);
               float g      = metal_hdr_half_to_float(srcPx[4 * srcCol + 1]);
               float b      = metal_hdr_half_to_float(srcPx[4 * srcCol + 2]);
               float lvl;
               dst[3 * x + 0] = metal_hdr_scrgb_to_pq16(r);
               dst[3 * x + 1] = metal_hdr_scrgb_to_pq16(g);
               dst[3 * x + 2] = metal_hdr_scrgb_to_pq16(b);
               lvl = r; if (g > lvl) lvl = g; if (b > lvl) lvl = b;
               lvl *= 80.0f;
               if (lvl < 0.0f) lvl = 0.0f;
               else if (lvl > 10000.0f) lvl = 10000.0f;
               if (lvl > maxCLL) maxCLL = lvl;
               sumFALL += lvl;
            }
         }
         else
         {
            const uint32_t *srcPx = (const uint32_t *)src_row;
            for (x = (size_t)colStart; x < (size_t)colEnd; x++)
            {
               /* MTLPixelFormatRGB10A2Unorm: R[9:0] G[19:10] B[29:20]
                * A[31:30] -- same placement as DXGI R10G10B10A2 and
                * Vulkan A2B10G10R10. */
               uint32_t w = srcPx[VIDEO_POS_X(_viewport.pos) + (int)x];
               uint32_t r = (w      ) & 0x3FF;
               uint32_t g = (w >> 10) & 0x3FF;
               uint32_t b = (w >> 20) & 0x3FF;
               uint32_t mx;
               float    lvl;
               dst[3 * x + 0] = (uint16_t)((r << 6) | (r >> 4));
               dst[3 * x + 1] = (uint16_t)((g << 6) | (g >> 4));
               dst[3 * x + 2] = (uint16_t)((b << 6) | (b >> 4));
               mx  = r; if (g > mx) mx = g; if (b > mx) mx = b;
               lvl = metal_hdr_pq_to_nits((float)mx * (1.0f / 1023.0f));
               if (lvl > maxCLL) maxCLL = lvl;
               sumFALL += lvl;
            }
         }
      }
   }

   free(heapRow);
#undef METAL_READBACK_CHUNK

   if (outMaxCLL)
      *outMaxCLL  = maxCLL;
   if (outMaxFALL)
      *outMaxFALL = (VIDEO_SCALE_W(_viewport.dims) && VIDEO_SCALE_H(_viewport.dims))
            ? (float)(sumFALL / ((double)VIDEO_SCALE_W(_viewport.dims)
                               * (double)VIDEO_SCALE_H(_viewport.dims)))
            : 0.0f;
   if (outIsSCRGB)
      *outIsSCRGB = isSCRGB;
   return YES;
}
#endif /* METAL_HDR_AVAILABLE */

- (void)begin
{
   if (_commandBuffer != nil)
   {
      RARCH_WARN("[Metal] begin called with active command buffer - resetting\n");
      RARCH_RELEASE_NIL(_commandBuffer);
   }

   /* Don't use semaphore for frame pacing - let nextDrawable handle it.
    * CAMetalLayer.nextDrawable will block if no drawable is available,
    * which naturally paces us to the display refresh rate.
    * Using a semaphore on top of this causes timing mismatches because
    * the semaphore signals on presentation but the drawable isn't
    * released until the NEXT vsync. */

   RARCH_ASSIGN(_commandBuffer, [_commandQueue commandBuffer]);
   _commandBuffer.label = @"Frame command buffer";
   RARCH_RELEASE_NIL(_backBuffer);
   _drawableBegun = false;
}

- (id<MTLRenderCommandEncoder>)rce
{
   if (_commandBuffer == nil)
   {
      RARCH_ERR("[Metal] rce called without active command buffer\n");
      return nil;
   }
   if (_rce == nil)
   {
#if METAL_HDR_AVAILABLE
      /* HDR mode: the frame-encoder targets a BGRA8 SDR overlay offscreen.
       * Menu / overlay / OSD / widget draws land here through the stock
       * SDR pipelines (all compiled against BGRA8).  At end of frame the
       * HDR composite pass samples this texture and alpha-blends it over
       * the encoded core video into the HDR drawable.  This keeps the UI
       * rendering in native SDR colour space regardless of whether the
       * swapchain is PQ or scRGB encoded. */
      if (_hdrEnabled)
      {
         if (!_sdrOverlayTex)
         {
            RARCH_ERR("[Metal] HDR: SDR overlay offscreen missing.\n");
            return nil;
         }
         MTLRenderPassDescriptor *rpd = RARCH_AUTORELEASE_R([MTLRenderPassDescriptor new]);
         /* Clear to fully-transparent black so the core video shows
          * through where no UI is drawn. */
         rpd.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);
         rpd.colorAttachments[0].loadAction = MTLLoadActionClear;
         rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
         rpd.colorAttachments[0].texture    = _sdrOverlayTex;
         RARCH_ASSIGN(_rce, [_commandBuffer renderCommandEncoderWithDescriptor:rpd]);
         _rce.label = @"SDR overlay encoder (HDR frame)";
         _sdrOverlayDirty = true;
         return _rce;
      }
#endif

      id<CAMetalDrawable> drawable = self.nextDrawable;
      if (!drawable || !drawable.texture)
      {
         RARCH_WARN("[Metal] Failed to acquire drawable - frame dropped\n");
         return nil;
      }

      MTLRenderPassDescriptor *rpd = RARCH_AUTORELEASE_R([MTLRenderPassDescriptor new]);
      rpd.colorAttachments[0].clearColor = _clearColor;
      rpd.colorAttachments[0].loadAction = _drawableBegun
         ? MTLLoadActionLoad : MTLLoadActionClear;
      rpd.colorAttachments[0].texture = drawable.texture;
      _drawableBegun = true;
      if (_captureEnabled)
         RARCH_ASSIGN(_backBuffer, drawable.texture);
      RARCH_ASSIGN(_rce, [_commandBuffer renderCommandEncoderWithDescriptor:rpd]);
      _rce.label = @"Frame command encoder";
   }
   return _rce;
}

- (void)beginLayer:(id<MTLTexture>)tex
{
   MTLRenderPassDescriptor *rpd;
   if (_commandBuffer == nil)
      return;
   if (_rce)
   {
      [_rce endEncoding];
      RARCH_RELEASE_NIL(_rce);
   }
   rpd = RARCH_AUTORELEASE_R([MTLRenderPassDescriptor new]);
   rpd.colorAttachments[0].clearColor  = MTLClearColorMake(0, 0, 0, 0);
   rpd.colorAttachments[0].loadAction  = MTLLoadActionClear;
   rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
   rpd.colorAttachments[0].texture     = tex;
   RARCH_ASSIGN(_rce, [_commandBuffer renderCommandEncoderWithDescriptor:rpd]);
   _rce.label = @"Views UI layer";
}

- (void)endLayer
{
   if (_rce)
   {
      [_rce endEncoding];
      RARCH_RELEASE_NIL(_rce);
   }
}

- (void)resetRenderViewport:(ViewportResetMode)mode
{
   bool fullscreen = mode == kFullscreenViewport;
   MTLViewport vp  = {
      .originX = fullscreen ? 0 : VIDEO_POS_X(_viewport.pos),
      .originY = fullscreen ? 0 : VIDEO_POS_Y(_viewport.pos),
      .width   = fullscreen ? VIDEO_SCALE_W(_viewport.full_dims) : VIDEO_SCALE_W(_viewport.dims),
      .height  = fullscreen ? VIDEO_SCALE_H(_viewport.full_dims) : VIDEO_SCALE_H(_viewport.dims),
      .znear   = 0,
      .zfar    = 1,
   };
   [self.rce setViewport:vp];
}

- (void)resetScissorRect
{
   MTLScissorRect sr = {
      .x       = 0,
      .y       = 0,
      .width   = VIDEO_SCALE_W(_viewport.full_dims),
      .height  = VIDEO_SCALE_H(_viewport.full_dims),
   };
   [self.rce setScissorRect:sr];
}

- (void)drawQuadX:(float)x y:(float)y w:(float)w h:(float)h
                r:(float)r g:(float)g b:(float)b a:(float)a
{
   SpriteVertex v[4];
   float    rgba[4];
   uint64_t color;
   v[0].position = simd_make_float2(x, y);
   v[1].position = simd_make_float2(x + w, y);
   v[2].position = simd_make_float2(x, y + h);
   v[3].position = simd_make_float2(x + w, y + h);

   rgba[0]    = r;
   rgba[1]    = g;
   rgba[2]    = b;
   rgba[3]    = a;
   color      = rgba16_pack(rgba);
   v[0].color = color;
   v[1].color = color;
   v[2].color = color;
   v[3].color = color;

   id<MTLRenderCommandEncoder> rce = self.rce;
   [rce setRenderPipelineState:(_hdrEnabled && _clearStateF16)
      ? _clearStateF16 : _clearState];
   [rce setVertexBytes:&v length:sizeof(v) atIndex:BufferIndexPositions];
   [rce setVertexBytes:&_uniforms length:sizeof(_uniforms) atIndex:BufferIndexUniforms];
   [rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
}

- (void)end { [self endWithRecording:NULL]; }

- (void)endWithRecording:(video_frame_info_t *)video_info
{
   (void)video_info;
   if (_commandBuffer == nil)
   {
      RARCH_WARN("[Metal] end called without active command buffer\n");
      return;
   }

   buffer_chain_commit_ranges(&_chain[_currentChain]);

   if (_blitCommandBuffer)
   {
#if TARGET_OS_OSX
      if (_captureEnabled)
      {
         id<MTLBlitCommandEncoder> bce = [_blitCommandBuffer blitCommandEncoder];
         [bce synchronizeResource:_backBuffer];
         [bce endEncoding];
      }
#endif
      /* Pending blits for mipmaps or render passes for slang shaders.
       * Metal command queues guarantee commit-order execution, so we don't
       * need to block the CPU waiting for completion. */
      [_blitCommandBuffer commit];
      RARCH_RELEASE_NIL(_blitCommandBuffer);
   }

   if (_rce)
   {
      [_rce endEncoding];
      RARCH_RELEASE_NIL(_rce);
   }

   id<CAMetalDrawable> drawable = self.nextDrawable;
#ifdef HAVE_RECORD_METAL
   /* Final render encoder is closed. Capture the negotiated drawable into
    * the fixed encoder pool on this same retained-reference command buffer;
    * normal presentation commits it. Native capture never enables readback. */
   if (drawable && video_info && video_info->recording_gpu_capture)
      record_metal_gpu_capture(video_info->recording_gpu,(__bridge void*)_commandBuffer,
            (__bridge void*)drawable.texture,_viewport.pos,_viewport.dims,
            video_info->recording_gpu_timestamp,self.hdrOutputMode);
#endif

   if (drawable)
   {
#ifdef RETRO_ATOMIC_HAS_64
      /* When it reached the display, for the threaded presenter to lay
       * its vblank grid on: presentedTime is on the host clock, bridged
       * to the frontend's by one paired read, since CLOCK_MONOTONIC
       * counts sleep and the host clock does not. 0 means the drawable
       * was never shown. */
      if (apple_runtime_available(APPLE_RUNTIME_VER(10, 15, 4),
               APPLE_RUNTIME_VER(10, 3, 0), APPLE_RUNTIME_VER(10, 3, 0)))
         [(id<MetalPresentedDrawable>)drawable addPresentedHandler:
            ^(id<MTLDrawable> _Nonnull shown) {
               CFTimeInterval at = [(id<MetalPresentedDrawable>)shown presentedTime];
               if (at > 0.0)
               {
                  retro_time_t   now = cpu_features_get_time_usec();
                  CFTimeInterval age = CACurrentMediaTime() - at;
                  if (age >= 0.0)
                     retro_atomic_store_release_64(&metal_presented_at,
                           (int64_t)(now - (retro_time_t)(age * 1000000.0)));
               }
            }];
#endif
      /* Use addScheduledHandler to present, following Apple's recommendation.
       * According to Apple (and used by MoltenVK), it is more performant to call
       * [drawable present] from within a scheduled-handler than to use
       * [commandBuffer presentDrawable:]. This provides better frame pacing
       * because presentation is queued when the command buffer is scheduled
       * (added to GPU queue), not when it completes. */
      [_commandBuffer addScheduledHandler:^(id<MTLCommandBuffer> _Nonnull buffer) {
         [drawable present];
      }];
   }

   [_commandBuffer commit];

   RARCH_RELEASE_NIL(_commandBuffer);
   [self _nextChain];
}

- (void)swapBuffers
{
   /* Acquire the next drawable after presentation, matching Vulkan's
    * swap_buffers timing where acquisition happens AFTER presenting.
    *
    * We explicitly clear _drawable first to force a fresh acquisition.
    * nextDrawable will block if no drawable is available (all 3 are
    * in-flight), which naturally paces us to the display refresh rate.
    * This blocking behavior is intentional for proper frame pacing. */
   RARCH_RELEASE_NIL(_drawable);
   RARCH_ASSIGN(_drawable, _layer.nextDrawable);
}

- (void)releaseDrawable
{
   RARCH_RELEASE_NIL(_drawable);
}

- (retro_time_t)lastPresentTime
{
#ifdef RETRO_ATOMIC_HAS_64
   retro_time_t at = (retro_time_t)retro_atomic_load_acquire_64(&metal_presented_at);
   if (at > 0)
      return at;
#endif
#if TARGET_OS_OSX
   /* No drawable has said when it was shown: the display's last vblank,
    * from the view's display link, where there is one. */
   {
      retro_time_t cocoa_last_vblank_time(void);
      return cocoa_last_vblank_time();
   }
#else
   return 0;
#endif
}

- (bool)allocRange:(BufferRange *)range length:(NSUInteger)length
{
   return buffer_chain_alloc_range(&_chain[_currentChain], range, length);
}

@end

@implementation Texture

#if !__has_feature(objc_arc)
- (void)dealloc
{
   [(id)_texture release];
   [(id)_sampler release];
   [(id)_staging release];
   [super dealloc];
}
#endif

@end

static void buffer_chain_init(buffer_chain_t *chain,
      id<MTLDevice> device, NSUInteger block_len)
{
   memset(chain, 0, sizeof(*chain));
   RARCH_STRUCT_ASSIGN(chain->device, device);
   chain->block_len = block_len;
}

static void buffer_chain_destroy(buffer_chain_t *chain)
{
   buffer_node_t *n = chain->head;
   while (n)
   {
      buffer_node_t *next = n->next;
      RARCH_STRUCT_ASSIGN(n->src, nil);
      free(n);
      n = next;
   }
   chain->head = NULL;
   chain->cur  = NULL;
   RARCH_STRUCT_ASSIGN(chain->device, nil);
}

static buffer_node_t *buffer_node_new(id<MTLBuffer> src)
{
   buffer_node_t *n = (buffer_node_t *)calloc(1, sizeof(*n));
   if (!n || !src)
   {
      free(n);
      return NULL;
   }
   RARCH_STRUCT_ASSIGN(n->src, src);
   return n;
}

static void buffer_chain_commit_ranges(buffer_chain_t *chain)
{
#if TARGET_OS_OSX
   buffer_node_t *n;
   for (n = chain->head; n; n = n->next)
   {
      if (n->allocated > 0)
         [n->src didModifyRange:NSMakeRange(0, n->allocated)];
   }
#endif
}

static void buffer_chain_discard(buffer_chain_t *chain)
{
   buffer_node_t *n;

   /* Trim the tail: any node that wasn't touched during this
    * chain's previous use (allocated == 0) is dropped. Nodes are
    * appended in alloc order, so once we see the first trailing
    * unused node the whole tail is unused. We only trim when the
    * chain was actually used (allocated > 0) so that a single
    * quiescent frame doesn't drop the entire chain and force
    * reallocation on the next use.
    *
    * This bounds retained memory to the recent high-water mark
    * rather than the all-time high-water mark, which previously
    * grew monotonically: any one-off large allocation (e.g. a
    * heavy shader pass or a brief geometry spike) kept its
    * oversized backing node alive for the lifetime of the driver,
    * across all CHAIN_LENGTH chains. */
   if (chain->head && chain->allocated > 0)
   {
      buffer_node_t *keep = chain->head;
      for (n = chain->head; n; n = n->next)
      {
         if (n->allocated > 0)
            keep = n;
      }
      if (keep->next)
      {
         NSUInteger dropped = 0;
         buffer_node_t *t   = keep->next;
         for (n = t; n; n = n->next)
            dropped += n->src.length;
         chain->length -= dropped;
         keep->next     = NULL;
         while (t)
         {
            n = t->next;
            RARCH_STRUCT_ASSIGN(t->src, nil);
            free(t);
            t = n;
         }
      }
   }

   /* Reset per-node allocated so commit_ranges on the next use of
    * this chain does not didModifyRange: a stale range from this
    * cycle into a node that gets partially refilled. */
   for (n = chain->head; n; n = n->next)
      n->allocated = 0;

   chain->cur       = chain->head;
   chain->offset    = 0;
   chain->allocated = 0;
}

static BOOL buffer_chain_sub_alloc(buffer_chain_t *chain,
      BufferRange *range, NSUInteger length)
{
   NSUInteger next_offset = chain->offset + length;
   id<MTLBuffer> src      = chain->cur->src;
   if (next_offset <= src.length)
   {
      chain->cur->allocated = next_offset;
      chain->allocated     += length;
      range->data           = (uint8_t *)src.contents + chain->offset;
      range->buffer         = src;
      range->offset         = chain->offset;
      chain->offset         = MTL_ALIGN_BUFFER(next_offset);
      return YES;
   }
   return NO;
}

static bool buffer_chain_alloc_range(buffer_chain_t *chain,
      BufferRange *range, NSUInteger length)
{
   MTLResourceOptions opts = PLATFORM_METAL_RESOURCE_STORAGE_MODE;
   memset(range, 0, sizeof(*range));

   if (!chain->head)
   {
      chain->head = buffer_node_new(RARCH_AUTORELEASE_R(
            [chain->device newBufferWithLength:chain->block_len options:opts]));
      if (!chain->head)
         return false;
      chain->length += chain->block_len;
      chain->cur     = chain->head;
      chain->offset  = 0;
   }

   if (buffer_chain_sub_alloc(chain, range, length))
      return true;

   while (chain->cur->next)
   {
      chain->cur    = chain->cur->next;
      chain->offset = 0;
      if (buffer_chain_sub_alloc(chain, range, length))
         return true;
   }

   {
      NSUInteger block_len = chain->block_len;
      if (length > block_len)
         block_len = length;

      chain->cur->next = buffer_node_new(RARCH_AUTORELEASE_R(
            [chain->device newBufferWithLength:block_len options:opts]));
      if (!chain->cur->next)
         return false;

      chain->length += block_len;
   }

   chain->cur    = chain->cur->next;
   chain->offset = 0;
   retro_assert(buffer_chain_sub_alloc(chain, range, length));
   return true;
}

/*
 * FILTER
 */

@interface Filter()
- (instancetype)initWithKernel:(id<MTLComputePipelineState>)kernel sampler:(id<MTLSamplerState>)sampler;
@end

@implementation Filter
{
   id<MTLComputePipelineState> _kernel;
}

+ (instancetype)newFilterWithFunctionName:(NSString *)name device:(id<MTLDevice>)device library:(id<MTLLibrary>)library error:(NSError **)error
{
   id<MTLFunction> function = RARCH_AUTORELEASE_R([library newFunctionWithName:name]);
   id<MTLComputePipelineState> kernel;
   if (!function)
   {
      /* newComputePipelineStateWithFunction asserts (rather than errors)
       * on a nil function, so guard it: return nil and let the caller
       * degrade gracefully instead of aborting the process. */
      RARCH_ERR("[Metal] Compute function \"%s\" not found (library %s).\n",
            name.UTF8String, library ? "loaded" : "is NIL");
      return nil;
   }
   kernel = RARCH_AUTORELEASE_R([device newComputePipelineStateWithFunction:function error:error]);
   if (*error != nil)
      return nil;

   MTLSamplerDescriptor *sd    = RARCH_AUTORELEASE_R([MTLSamplerDescriptor new]);
   sd.minFilter                = MTLSamplerMinMagFilterNearest;
   sd.magFilter                = MTLSamplerMinMagFilterNearest;
   sd.sAddressMode             = MTLSamplerAddressModeClampToEdge;
   sd.tAddressMode             = MTLSamplerAddressModeClampToEdge;
   sd.mipFilter                = MTLSamplerMipFilterNotMipmapped;
   id<MTLSamplerState> sampler = RARCH_AUTORELEASE_R([device newSamplerStateWithDescriptor:sd]);

   return [[Filter alloc] initWithKernel:kernel sampler:sampler];
}

- (instancetype)initWithKernel:(id<MTLComputePipelineState>)kernel sampler:(id<MTLSamplerState>)sampler
{
   if (self = [super init])
   {
      _kernel  = RARCH_RETAIN(kernel);
      _sampler = RARCH_RETAIN(sampler);
   }
   return self;
}

#if !__has_feature(objc_arc)
- (void)dealloc
{
   [(id)_kernel release];
   [(id)_sampler release];
   [super dealloc];
}
#endif

- (void)apply:(id<MTLCommandBuffer>)cb in:(id<MTLTexture>)tin out:(id<MTLTexture>)tout
{
   id<MTLComputeCommandEncoder> ce = [cb computeCommandEncoder];
   ce.label = @"filter kernel";

   [ce setComputePipelineState:_kernel];

   [ce setTexture:tin atIndex:0];
   [ce setTexture:tout atIndex:1];

   [self.delegate configure:ce];

   MTLSize size  = MTLSizeMake(16, 16, 1);
   MTLSize count = MTLSizeMake((tin.width + size.width + 1) / size.width, (tin.height + size.height + 1) / size.height,
                               1);

   [ce dispatchThreadgroups:count threadsPerThreadgroup:size];
   [ce endEncoding];
}

- (void)apply:(id<MTLCommandBuffer>)cb inBuf:(id<MTLBuffer>)tin outTex:(id<MTLTexture>)tout
{
   id<MTLComputeCommandEncoder> ce = [cb computeCommandEncoder];
   ce.label = @"filter kernel";

   [ce setComputePipelineState:_kernel];

   [ce setBuffer:tin offset:0 atIndex:0];
   [ce setTexture:tout atIndex:0];

   [self.delegate configure:ce];

   MTLSize size  = MTLSizeMake(32, 1, 1);
   MTLSize count = MTLSizeMake((tin.length + 31) / 32, 1, 1);

   [ce dispatchThreadgroups:count threadsPerThreadgroup:size];
   [ce endEncoding];
}

@end

#ifdef HAVE_MENU
@implementation MenuDisplay
{
   Context *_context;
   MTLClearColor _clearColor;
   MTLScissorRect _scissorRect;
   BOOL _useScissorRect;
   Uniforms _uniforms;
   bool _clearNextRender;
   /* gfx_display meshes: the pipeline, and each mesh's buffers by id */
   id<MTLRenderPipelineState> _meshState;
   id<MTLRenderPipelineState> _meshStateF16; /* the HDR UI overlay's */
   BOOL _meshStateFailed;
   id<MTLBuffer> _meshVbo[4];
   id<MTLBuffer> _meshIbo[4];
   uint32_t _meshId[4];
   uint64_t _meshLast[4];
   uint64_t _meshDraws;
   /* Sampled by a quad drawn with no texture: solid colour */
   Texture *_white;
}

- (instancetype)initWithContext:(Context *)context
{
   if (self = [super init])
   {
      _context                   = RARCH_RETAIN(context);
      _clearColor                = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
      _uniforms.projectionMatrix = matrix_proj_ortho(0, 1, 0, 1);
      _useScissorRect            = NO;
   }
   return self;
}

#if !__has_feature(objc_arc)
- (void)dealloc
{
   unsigned i;
   for (i = 0; i < 4; i++)
   {
      [_meshVbo[i] release];
      [_meshIbo[i] release];
   }
   [_meshState release];
   [_meshStateF16 release];
   [_white release];
   [_context release];
   [super dealloc];
}
#endif

- (Texture *)_whiteTexture
{
   if (!_white)
   {
      static const uint32_t white = 0xffffffffu;
      struct texture_image image;
      memset(&image, 0, sizeof(image));
      image.pixels = (uint32_t *)&white;
      image.width  = 1;
      image.height = 1;
      _white = [_context newTexture:image filter:TEXTURE_FILTER_NEAREST];
   }
   return _white;
}

+ (const float *)defaultVertices
{
   static float dummy[8] = {
      0.0f, 0.0f,
      1.0f, 0.0f,
      0.0f, 1.0f,
      1.0f, 1.0f,
   };
   return &dummy[0];
}

+ (const float *)defaultTexCoords
{
   static float dummy[8] = {
      0.0f, 1.0f,
      1.0f, 1.0f,
      0.0f, 0.0f,
      1.0f, 0.0f,
   };
   return &dummy[0];
}

+ (const float *)defaultColor
{
   static float dummy[16] = {
      1.0f, 0.0f, 1.0f, 1.0f,
      1.0f, 0.0f, 1.0f, 1.0f,
      1.0f, 0.0f, 1.0f, 1.0f,
      1.0f, 0.0f, 1.0f, 1.0f,
   };
   return &dummy[0];
}

- (void)setClearColor:(MTLClearColor)clearColor
{
   _clearColor      = clearColor;
   _clearNextRender = YES;
}

- (MTLClearColor)clearColor
{
   return _clearColor;
}

- (void)setScissorRect:(MTLScissorRect)rect
{
   _scissorRect = rect;
   _useScissorRect = YES;
}

- (void)clearScissorRect
{
   _useScissorRect = NO;
   [_context resetScissorRect];
}

- (void)drawPipeline:(gfx_display_ctx_draw_t *)draw
{
   static struct video_coords blank_coords;
   draw->pos               = VIDEO_POS_PACK(0, 0);
   draw->matrix_data       = NULL;
   _uniforms.outputSize    = simd_make_float2(VIDEO_SCALE_W(_context.viewport->full_dims), VIDEO_SCALE_H(_context.viewport->full_dims));
   draw->backend_data      = &_uniforms;
   draw->backend_data_size = sizeof(_uniforms);

   switch (draw->pipeline_id)
   {
      /* ribbon */
      default:
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      {
         if (!(draw->coords = gfx_display_effect_coords(disp_get_ptr())))
            return;
         break;
      }

      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
      {
         draw->coords          = &blank_coords;
         blank_coords.vertices = 4;
         break;
      }
   }

   /* The effects' clock, one step ahead, as this driver has always
    * drawn them */
   _uniforms.time = disp_get_ptr()->effect_time + 0.01f;
   if (_uniforms.time > 65536.0f)
      _uniforms.time -= 65536.0f;
}

/* gfx_display meshes. The shader is compiled here from its source
 * rather than taken from the library the others come from, so a device
 * or OS that cannot compile it draws its meshes streamed instead. */
- (BOOL)_initMeshState
{
   NSError *err                     = nil;
   id<MTLDevice> device             = _context.device;
   id<MTLLibrary> lib;
   MTLVertexDescriptor *vd;
   MTLRenderPipelineDescriptor *psd;
   MTLRenderPipelineColorAttachmentDescriptor *ca;
   NSString *src = [NSString stringWithFormat:@
      "#include <metal_stdlib>\n"
      "using namespace metal;\n"
      "struct MeshIn  { float3 position [[attribute(0)]];"
      " float2 texCoord [[attribute(1)]]; float4 color [[attribute(2)]]; };\n"
      "struct MeshUniforms { float4x4 mvp; float4 tint; };\n"
      "struct MeshOut { float4 position [[position]]; float2 texCoord; float4 color; };\n"
      "vertex MeshOut mesh_vertex(MeshIn in [[stage_in]],"
      " constant MeshUniforms &u [[buffer(%d)]])\n"
      "{ MeshOut out; out.position = u.mvp * float4(in.position, 1.0);"
      " out.texCoord = in.texCoord; out.color = in.color * u.tint; return out; }\n"
      "fragment float4 mesh_fragment(MeshOut in [[stage_in]],"
      " texture2d<float> tex [[texture(%d)]], sampler samp [[sampler(%d)]])\n"
      "{ return in.color * tex.sample(samp, in.texCoord); }\n",
      (int)BufferIndexUniforms, (int)TextureIndexColor, (int)SamplerIndexDraw];

   _meshStateFailed = YES;
   if (!device)
      return NO;
   lib = RARCH_AUTORELEASE_R([device newLibraryWithSource:src options:nil error:&err]);
   if (!lib)
   {
      RARCH_WARN("[Metal] Mesh shader unavailable, meshes are streamed: %s.\n",
            err ? err.localizedDescription.UTF8String : "");
      return NO;
   }

   /* Read as stored: three floats, then two 16-bit and four 8-bit
    * normalised integers */
   vd                                      = RARCH_AUTORELEASE_R([MTLVertexDescriptor new]);
   vd.attributes[0].offset                 = 0;
   vd.attributes[0].format                 = MTLVertexFormatFloat3;
   vd.attributes[0].bufferIndex            = BufferIndexPositions;
   vd.attributes[1].offset                 = 12;
   vd.attributes[1].format                 = MTLVertexFormatUShort2Normalized;
   vd.attributes[1].bufferIndex            = BufferIndexPositions;
   vd.attributes[2].offset                 = 16;
   vd.attributes[2].format                 = MTLVertexFormatUChar4Normalized;
   vd.attributes[2].bufferIndex            = BufferIndexPositions;
   vd.layouts[BufferIndexPositions].stride = sizeof(gfx_display_mesh_vertex_t);

   psd                            = RARCH_AUTORELEASE_R([MTLRenderPipelineDescriptor new]);
   psd.label                      = @"mesh";
   ca                             = psd.colorAttachments[0];
   ca.pixelFormat                 = MTLPixelFormatBGRA8Unorm;
   ca.blendingEnabled             = YES;
   ca.sourceRGBBlendFactor        = MTLBlendFactorSourceAlpha;
   ca.destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;
   ca.sourceAlphaBlendFactor      = MTLBlendFactorSourceAlpha;
   ca.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
   psd.sampleCount                = 1;
   psd.vertexDescriptor           = vd;
   psd.vertexFunction             = RARCH_AUTORELEASE_R([lib newFunctionWithName:@"mesh_vertex"]);
   psd.fragmentFunction           = RARCH_AUTORELEASE_R([lib newFunctionWithName:@"mesh_fragment"]);
   if (!psd.vertexFunction || !psd.fragmentFunction)
      return NO;
   _meshState = [_context sdrState:psd error:&err];
   _meshStateF16 = [_context twinF16:psd];
   if (!_meshState)
   {
      RARCH_WARN("[Metal] Mesh pipeline unavailable, meshes are streamed: %s.\n",
            err ? err.localizedDescription.UTF8String : "");
      return NO;
   }
   _meshStateFailed = NO;
   return YES;
}

/* The slot whose buffers hold @mesh, made the first time it is drawn;
 * the one drawn longest ago gives way when all are taken. A command
 * buffer keeps what it draws from alive, so letting go of a buffer here
 * is safe while a frame still reads it. -1 when none can be had. */
- (int)_meshSlot:(const gfx_display_mesh_t *)mesh
{
   id<MTLDevice> device = _context.device;
   uint64_t oldest      = (uint64_t)-1;
   int slot             = -1;
   unsigned i;

   for (i = 0; i < 4; i++)
   {
      if (_meshVbo[i] && _meshId[i] == mesh->id)
         return (int)i;
      if (!_meshVbo[i])
      {
         if (slot < 0 || _meshVbo[slot])
            slot = (int)i;
      }
      else if ((slot < 0 || _meshVbo[slot]) && _meshLast[i] < oldest)
      {
         oldest = _meshLast[i];
         slot   = (int)i;
      }
   }
   if (slot < 0 || !device)
      return -1;
   RARCH_RELEASE(_meshVbo[slot]);
   _meshVbo[slot] = nil;
   RARCH_RELEASE(_meshIbo[slot]);
   _meshIbo[slot] = nil;
   _meshId[slot]  = 0;

   _meshVbo[slot] = [device newBufferWithBytes:mesh->vertices
      length:mesh->vertex_count * sizeof(gfx_display_mesh_vertex_t)
      options:MTLResourceStorageModeShared];
   if (!_meshVbo[slot])
      return -1;
   if (mesh->indices)
   {
      _meshIbo[slot] = [device newBufferWithBytes:mesh->indices
         length:mesh->index_count * sizeof(uint16_t)
         options:MTLResourceStorageModeShared];
      if (!_meshIbo[slot])
      {
         RARCH_RELEASE(_meshVbo[slot]);
         _meshVbo[slot] = nil;
         return -1;
      }
   }
   _meshId[slot] = mesh->id;
   return slot;
}

- (BOOL)drawMesh:(const gfx_display_mesh_t *)mesh mvp:(const float *)mvp
      texture:(uintptr_t)texture tint:(const float *)tint dims:(unsigned)video_dims
{
   /* The vertex stage's constants: a float4x4, then a float4 */
   struct
   {
      matrix_float4x4 mvp;
      vector_float4   tint;
   } u;
   float flipped[16];
   unsigned c;
   int slot;
   MTLPrimitiveType prim;
   Texture *tex                    = texture
      ? (__bridge Texture *)(void *)texture : [self _whiteTexture];
   id<MTLRenderCommandEncoder> rce = _context.rce;

   if (!mesh || tex == nil || !rce)
      return NO;
   if (!_meshState && !_meshStateFailed)
      [self _initMeshState];
   if (!_meshState)
      return NO;
   if ((slot = [self _meshSlot:mesh]) < 0)
      return NO;
   _meshLast[slot] = ++_meshDraws;

   /* The display's 0..1 space is bottom-up, as draw: bakes it: y
    * becomes w - y, the homogeneous 1 - y, before the projection */
   memcpy(flipped, mvp, sizeof(flipped));
   for (c = 0; c < 4; c++)
      flipped[c * 4 + 1] = mvp[c * 4 + 3] - mvp[c * 4 + 1];
   u.mvp  = simd_mul(_uniforms.projectionMatrix, make_matrix_float4x4(flipped));
   u.tint = simd_make_float4(tint[0], tint[1], tint[2], tint[3]);

   if (_clearNextRender)
   {
      [_context resetRenderViewport:kFullscreenViewport];
      [_context drawQuadX:0
                        y:0
                        w:1
                        h:1
                        r:(float)_clearColor.red
                        g:(float)_clearColor.green
                        b:(float)_clearColor.blue
                        a:(float)_clearColor.alpha
      ];
      _clearNextRender = NO;
   }
   {
      /* The whole display, as draw: sets it for a strip at the origin */
      MTLViewport vp = {
         .originX = 0,
         .originY = VIDEO_SCALE_H(_context.viewport->full_dims)
                  - VIDEO_SCALE_H(video_dims),
         .width   = VIDEO_SCALE_W(video_dims),
         .height  = VIDEO_SCALE_H(video_dims),
         .znear   = 0,
         .zfar    = 1,
      };
      [rce setViewport:vp];
   }
   if (_useScissorRect)
      [rce setScissorRect:_scissorRect];

   prim = (mesh->topology == GFX_MESH_TRIANGLE_STRIP)
      ? MTLPrimitiveTypeTriangleStrip : MTLPrimitiveTypeTriangle;
   [rce setRenderPipelineState:(_context.hdrEnabled && _meshStateF16)
      ? _meshStateF16 : _meshState];
   [rce setVertexBytes:&u length:sizeof(u) atIndex:BufferIndexUniforms];
   [rce setVertexBuffer:_meshVbo[slot] offset:0 atIndex:BufferIndexPositions];
   [rce setFragmentTexture:tex.texture atIndex:TextureIndexColor];
   [rce setFragmentSamplerState:tex.sampler atIndex:SamplerIndexDraw];
   if (mesh->indices)
      [rce drawIndexedPrimitives:prim indexCount:mesh->index_count
         indexType:MTLIndexTypeUInt16 indexBuffer:_meshIbo[slot]
         indexBufferOffset:0];
   else
      [rce drawPrimitives:prim vertexStart:0 vertexCount:mesh->vertex_count];
   return YES;
}

- (void)draw:(gfx_display_ctx_draw_t *)draw
{
   unsigned i;
   BufferRange range;
   NSUInteger vertex_count;
   SpriteVertex *pv;
   const float *vertex    = draw->coords->vertex ?: MenuDisplay.defaultVertices;
   const float *tex_coord = draw->coords->tex_coord ?: MenuDisplay.defaultTexCoords;
   const float *color     = draw->coords->color ?: MenuDisplay.defaultColor;
   NSUInteger needed      = draw->coords->vertices * sizeof(SpriteVertex);
   if (![_context allocRange:&range length:needed])
      return;

   vertex_count           = draw->coords->vertices;
   pv                     = (SpriteVertex *)range.data;

   for (i = 0; i < draw->coords->vertices; i++, pv++)
   {
      pv->position = simd_make_float2(vertex[0], 1.0f - vertex[1]);
      vertex += 2;

      pv->texCoord = simd_make_float2(tex_coord[0], tex_coord[1]);
      tex_coord += 2;

      pv->color = rgba16_pack(color);
      color += 4;
   }

   id<MTLRenderCommandEncoder> rce = _context.rce;
   if (_clearNextRender)
   {
      [_context resetRenderViewport:kFullscreenViewport];
      [_context drawQuadX:0
                        y:0
                        w:1
                        h:1
                        r:(float)_clearColor.red
                        g:(float)_clearColor.green
                        b:(float)_clearColor.blue
                        a:(float)_clearColor.alpha
      ];
      _clearNextRender = NO;
   }

   MTLViewport vp = {
      .originX = VIDEO_POS_X(draw->pos),
      .originY = VIDEO_POS_FLIP_Y(VIDEO_SCALE_H(_context.viewport->full_dims),
            draw->pos, draw->dims),
      .width   = VIDEO_SCALE_W(draw->dims),
      .height  = VIDEO_SCALE_H(draw->dims),
      .znear   = 0,
      .zfar    = 1,
   };
   [rce setViewport:vp];

   if (_useScissorRect)
      [rce setScissorRect:_scissorRect];

   switch (draw->pipeline_id)
   {
#if HAVE_SHADERPIPELINE
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         [rce setRenderPipelineState:[_context getStockShader:draw->pipeline_id blend:_blend]];
         [rce setVertexBytes:draw->backend_data length:draw->backend_data_size atIndex:BufferIndexUniforms];
         [rce setVertexBuffer:range.buffer offset:range.offset atIndex:BufferIndexPositions];
         [rce setFragmentBytes:draw->backend_data length:draw->backend_data_size atIndex:BufferIndexUniforms];
         /* Menu draws use a triangle-strip layout. */
         [rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:vertex_count];
         return;
#endif
      default:
         break;
   }

   Texture *tex = draw->texture
      ? (__bridge Texture *)(void *)draw->texture : [self _whiteTexture];
   if (tex == nil)
      return;

   /* A half-float texture is linear scRGB: into the HDR overlay through
    * the inverse of the menu composite's encode */
   if (     tex.texture.pixelFormat == MTLPixelFormatRGBA16Float
         && [_context linearShader] != nil)
   {
      simd_float4 linParams = simd_make_float4(
            [_context hdrMenuNits], (float)[_context hdrExpandGamut],
            0.0f, 0.0f);
      [rce setRenderPipelineState:[_context linearShader]];
      [rce setFragmentBytes:&linParams length:sizeof(linParams) atIndex:0];
   }
   else
      [rce setRenderPipelineState:[_context getStockShader:VIDEO_SHADER_STOCK_BLEND blend:_blend]];

   Uniforms uniforms = {
      .projectionMatrix = draw->matrix_data ? make_matrix_float4x4((const float *)draw->matrix_data)
                                            : _uniforms.projectionMatrix
   };
   [rce setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:BufferIndexUniforms];
   [rce setVertexBuffer:range.buffer offset:range.offset atIndex:BufferIndexPositions];
   [rce setFragmentTexture:tex.texture atIndex:TextureIndexColor];
   [rce setFragmentSamplerState:tex.sampler atIndex:SamplerIndexDraw];
   [rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:vertex_count];
}
@end
#endif

@implementation ViewDescriptor

- (instancetype)init
{
   self = [super init];
   if (self)
      _format = RPixelFormatBGRA8Unorm;
   return self;
}

- (NSString *)debugDescription
{
#if defined(HAVE_COCOATOUCH)
   NSString *sizeDesc = [NSString stringWithFormat:@"width: %f, height: %f",_size.width,_size.height];
#else
   NSString *sizeDesc = NSStringFromSize(_size);
#endif
   return [NSString stringWithFormat:@"( format = %@, frame = %@ )",
           NSStringFromRPixelFormat(_format), sizeDesc];
}

@end

@implementation TexturedView
{
   Context *_context;
   id<MTLTexture> _texture; /* Optimal render texture */
   Vertex _v[4];
   CGSize _size;            /* Size of view in pixels */
   CGRect _frame;
   NSUInteger _bpp;
   id<MTLTexture> _src;     /* Source texture */
   bool _srcDirty;
}

- (instancetype)initWithDescriptor:(ViewDescriptor *)d context:(Context *)c
{
   self = [super init];
   if (self)
   {
      _format       = d.format;
      _bpp          = RPixelFormatToBPP(_format);
      _filter       = d.filter;
      _context      = RARCH_RETAIN(c);
      _visible      = YES;
      if (   _format == RPixelFormatBGRA8Unorm
          || _format == RPixelFormatBGRX8Unorm
          || _format == RPixelFormatBGR10A2Unorm)
         _drawState = ViewDrawStateEncoder;
      else
         _drawState = ViewDrawStateAll;
      self.size     = d.size;
      self.frame    = CGRectMake(0, 0, 1, 1);
   }
   return self;
}

#if !__has_feature(objc_arc)
- (void)dealloc
{
   [_context release];
   [(id)_texture release];
   [(id)_src release];
   [super dealloc];
}
#endif

- (void)setSize:(CGSize)size
{
   if (CGSizeEqualToSize(_size, size))
      return;

   _size = size;

   {
      /* The sampled texture matches the source for the direct-encoder
       * formats (BGRA8/BGRX8/BGR10A2); the filtered formats convert into a
       * BGRA8 texture via a compute pass. */
      MTLPixelFormat texFmt =
            (_format == RPixelFormatBGR10A2Unorm)
            ? MTLPixelFormatBGR10A2Unorm
            : MTLPixelFormatBGRA8Unorm;
      MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:texFmt
            width: (NSUInteger)size.width
            height:(NSUInteger)size.height
            mipmapped:NO];
      td.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
      RARCH_ASSIGN(_texture, RARCH_AUTORELEASE_R([_context.device newTextureWithDescriptor:td]));
   }

   if (   _format != RPixelFormatBGRA8Unorm
       && _format != RPixelFormatBGRX8Unorm
       && _format != RPixelFormatBGR10A2Unorm)
   {
      MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR16Uint
               width:(NSUInteger)size.width
               height:(NSUInteger)size.height
               mipmapped:NO];
      RARCH_ASSIGN(_src, RARCH_AUTORELEASE_R([_context.device newTextureWithDescriptor:td]));
   }
}

- (CGSize)size
{
   return _size;
}

- (void)setFrame:(CGRect)frame
{
   if (CGRectEqualToRect(_frame, frame))
      return;

   _frame      = frame;

   float l     = (float)CGRectGetMinX(frame);
   float t     = (float)CGRectGetMinY(frame);
   float r     = (float)CGRectGetMaxX(frame);
   float b     = (float)CGRectGetMaxY(frame);

   Vertex v[4] = {
      {simd_make_float3(l, b, 0), simd_make_float2(0, 1)},
      {simd_make_float3(r, b, 0), simd_make_float2(1, 1)},
      {simd_make_float3(l, t, 0), simd_make_float2(0, 0)},
      {simd_make_float3(r, t, 0), simd_make_float2(1, 0)},
   };
   memcpy(_v, v, sizeof(_v));
}

- (CGRect)frame
{
   return _frame;
}

- (void)drawWithContext:(Context *)ctx
{
   if (   _format == RPixelFormatBGRA8Unorm
       || _format == RPixelFormatBGRX8Unorm
       || _format == RPixelFormatBGR10A2Unorm)
      return;

   if (!_srcDirty)
      return;

   [_context convertFormat:_format from:_src to:_texture];
   _srcDirty = NO;
}

- (void)drawWithEncoder:(id<MTLRenderCommandEncoder>)rce
{
   [rce setVertexBytes:&_v length:sizeof(_v) atIndex:BufferIndexPositions];
   [rce setFragmentTexture:_texture atIndex:TextureIndexColor];
   [rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
}

- (void)updateFrame:(void const *)src pitch:(NSUInteger)pitch
{
   /* pitch is the source row stride in bytes (libretro convention,
    * matched by the MetalMenu caller which passes BPP * width).
    * Pass it straight through to Metal: multiplying by 4 here told
    * the driver to walk 4x the source memory between rows, reading
    * past the source allocation on every row after the first. The
    * else-branch already passes pitch straight through. */
   if (   _format == RPixelFormatBGRA8Unorm
       || _format == RPixelFormatBGRX8Unorm
       || _format == RPixelFormatBGR10A2Unorm)
   {
      [_texture replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)_size.width, (NSUInteger)_size.height)
                  mipmapLevel:0 withBytes:src
                  bytesPerRow:pitch];
   }
   else
   {
      [_src replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)_size.width, (NSUInteger)_size.height)
              mipmapLevel:0 withBytes:src
              bytesPerRow:pitch];
      _srcDirty = YES;
   }
}

@end

/*
 * DISPLAY DRIVER
 */

static const float *gfx_display_metal_get_default_vertices(void)
{
   return [MenuDisplay defaultVertices];
}

static const float *gfx_display_metal_get_default_tex_coords(void)
{
   return [MenuDisplay defaultTexCoords];
}

static void *gfx_display_metal_get_default_mvp(void *data)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return NULL;

   return (void *)&md.viewportMVP->projectionMatrix;
}

static void gfx_display_metal_blend_begin(void *data)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
      md.display.blend = YES;
}

static void gfx_display_metal_blend_end(void *data)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
      md.display.blend = NO;
}

static void gfx_display_metal_draw(gfx_display_ctx_draw_t *draw,
      void *data,
      unsigned video_dims)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md && draw)
      [md.display draw:draw];
}

static bool gfx_display_metal_mesh_draw(void *data, unsigned video_dims,
      const gfx_display_mesh_t *mesh, const float *mvp,
      uintptr_t texture, const float *tint)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md || !md.display)
      return false;
   return [md.display drawMesh:mesh mvp:mvp texture:texture tint:tint
      dims:video_dims] ? true : false;
}

static void gfx_display_metal_draw_pipeline(
      gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp,
      void *data,
      unsigned video_dims)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md && draw)
      [md.display drawPipeline:draw];
}

static void gfx_display_metal_scissor_begin(void *data, unsigned video_dims,
      int x, int y, unsigned dims)
{
   unsigned width        = VIDEO_SCALE_W(dims);
   unsigned height       = VIDEO_SCALE_H(dims);
   MTLScissorRect r;
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return;

   r.x      = (NSUInteger)x;
   r.y      = (NSUInteger)y;
   r.width  = width;
   r.height = height;
   [md.display setScissorRect:r];
}

static void gfx_display_metal_scissor_end(void *data, unsigned video_dims)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
      [md.display clearScissorRect];
}

/*
 * FONT DRIVER
 */

@interface MetalRaster : NSObject
{
   RARCH_WEAK MetalDriver *_driver;
   const font_renderer_driver_t *_font_driver;
   void *_font_data;
   struct font_atlas *_atlas;

   NSUInteger _stride;
   /* Bytes per atlas coverage sample (1 for A8, 2 for A16) */
   size_t _esz;
   id<MTLBuffer> _buffer;
   id<MTLTexture> _texture;

   id<MTLRenderPipelineState> _state;
   id<MTLRenderPipelineState> _stateF16; /* the HDR UI overlay's */
   id<MTLSamplerState> _sampler;

   Context *_context;

   Uniforms _uniforms;
   BufferRange _range;
   unsigned _vertices;
}

@property (readonly) struct font_atlas *atlas;

- (void)deinit;
- (instancetype)initWithDriver:(MetalDriver *)driver fontPath:(const char *)font_path fontSize:(unsigned)font_size;

- (int)getWidthForMessage:(const char *)msg length:(NSUInteger)length scale:(float)scale;
- (const struct font_glyph *)getGlyph:(uint32_t)code;
- (bool)getLineMetrics:(struct font_line_metrics **)metrics;
@end

@implementation MetalRaster

- (void)deinit
{
   if (_font_driver && _font_data)
      _font_driver->free(_font_data);
   _font_data   = NULL;
   _font_driver = NULL;
}

- (void)dealloc
{
   /* Idempotent: metal_raster_font_free already ran deinit on the
    * normal path; this covers the init-failure path where ARC (or the
    * MRC release below) tears down a half-constructed instance. */
   [self deinit];
#if !__has_feature(objc_arc)
   [_context release];
   [(id)_buffer release];
   [(id)_texture release];
   [(id)_state release];
   [(id)_stateF16 release];
   [(id)_sampler release];
   RARCH_SUPER_DEALLOC();
#endif
}

- (instancetype)initWithDriver:(MetalDriver *)driver fontPath:(const char *)font_path fontSize:(unsigned)font_size
{
   if (self = [super init])
   {
      if (driver == nil)
         RARCH_RETURN_INIT_FAILURE();

      _driver  = driver;
      _context = RARCH_RETAIN(driver.context);
      {
         /* When outputting HDR (scRGB or HDR10), ask the font
          * renderer for a higher-precision coverage atlas; same
          * policy as the d3d12 and vulkan drivers. */
         if (!font_renderer_create_default(
                  &_font_driver,
                  &_font_data, font_path, font_size,
                  _context.hdrEnabled
                  ? FONT_ATLAS_FORMAT_A16 : FONT_ATLAS_FORMAT_A8))
            RARCH_RETURN_INIT_FAILURE();
      }

      _uniforms.projectionMatrix = matrix_proj_ortho(0, 1, 0, 1);
      _atlas  = _font_driver->get_atlas(_font_data);
      /* The atlas may grow, up to a texture every Metal GPU makes; the
       * buffer and texture follow it (_followAtlas) */
      _atlas->max_dims = VIDEO_SCALE_PACK(8192, 8192);
      [self _makeAtlasTexture];

      if (![self _initializeState])
         RARCH_RETURN_INIT_FAILURE();
   }
   return self;
}

/* The managed buffer the atlas is copied into, and the texture made
 * on it, at the atlas's size: at init, and when the atlas has grown. A
 * command buffer still drawing from the old pair keeps it alive. */
- (void)_makeAtlasTexture
{
#if !__has_feature(objc_arc)
   [(id)_texture release];
   [(id)_buffer release];
#endif
   _texture = nil;
   _buffer  = nil;
   _esz    = (_atlas->format == FONT_ATLAS_FORMAT_A16)
         ? sizeof(uint16_t) : sizeof(uint8_t);
   _stride = MTL_ALIGN_BUFFER(_atlas->width * _esz);

   /* Allocate an uninitialized managed buffer and fill it through
    * .contents. This collapses two previous branches (fast path
    * via newBufferWithBytes:, slow path via row memcpy loop) into
    * one: row memcpy handles both the aligned and padded cases
    * and avoids the newBufferWithBytes: workaround (which had to
    * manually didModifyRange: the whole buffer anyway because
    * the initial copy was not correctly invalidated on macOS). */
   _buffer = [_context.device newBufferWithLength:(NSUInteger)(_stride * _atlas->height)
                                          options:PLATFORM_METAL_RESOURCE_STORAGE_MODE];
   {
      size_t i;
      size_t row_bytes   = (size_t)_atlas->width * _esz;
      uint8_t       *dst = (uint8_t *)_buffer.contents;
      const uint8_t *src = (const uint8_t *)_atlas->buffer;
      if (_stride == row_bytes)
      {
         memcpy(dst, src, (size_t)_stride * _atlas->height);
      }
      else
      {
         for (i = 0; i < _atlas->height; i++)
         {
            memcpy(dst, src, row_bytes);
            dst += _stride;
            src += row_bytes;
         }
      }
   }
#if !defined(HAVE_COCOATOUCH)
   [_buffer didModifyRange:NSMakeRange(0, _buffer.length)];
#endif

   MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
                                     (_atlas->format == FONT_ATLAS_FORMAT_A16)
                                           ? MTLPixelFormatR16Unorm
                                           : MTLPixelFormatR8Unorm
                                                                                 width:_atlas->width
                                                                                height:_atlas->height
                                                                             mipmapped:NO];

   _texture  = [_buffer newTextureWithDescriptor:td offset:0 bytesPerRow:_stride];
}

/* Asked for before a message is laid out: when the atlas has grown,
 * the texture is made again at its size before any texture coordinate
 * is taken from it */
- (void)_followAtlas
{
   _atlas = _font_driver->get_atlas(_font_data);
   if (     _atlas->width  != _texture.width
         || _atlas->height != _texture.height)
   {
      [self _makeAtlasTexture];
      _atlas->dirty = false;
   }
}

- (bool)_initializeState
{
   {
      NSError *err;
      MTLVertexDescriptor *vd    = RARCH_AUTORELEASE_R([MTLVertexDescriptor new]);

      vd.attributes[0].offset    = 0;
      vd.attributes[0].format    = MTLVertexFormatFloat2;
      vd.attributes[1].offset    = offsetof(SpriteVertex, texCoord);
      vd.attributes[1].format    = MTLVertexFormatFloat2;
      vd.attributes[2].offset    = offsetof(SpriteVertex, color);
      vd.attributes[2].format    = MTLVertexFormatUShort4Normalized;
      vd.layouts[0].stride       = sizeof(SpriteVertex);
      vd.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

      MTLRenderPipelineDescriptor *psd = RARCH_AUTORELEASE_R([MTLRenderPipelineDescriptor new]);
      psd.label = @"font pipeline";

      MTLRenderPipelineColorAttachmentDescriptor *ca = psd.colorAttachments[0];
      /* Always BGRA8Unorm.  Font glyphs render into the drawable in SDR
       * mode (drawable is BGRA8) or into the SDR overlay offscreen in HDR
       * mode (also BGRA8).  Keeping this a compile-time constant avoids
       * the mode-dependent validation failures we'd get if we tried to
       * match the HDR drawable format, and keeps font rendering visually
       * consistent with the rest of the SDR UI pipeline. */
      ca.pixelFormat                 = MTLPixelFormatBGRA8Unorm;
      ca.blendingEnabled             = YES;
      ca.sourceAlphaBlendFactor      = MTLBlendFactorSourceAlpha;
      ca.sourceRGBBlendFactor        = MTLBlendFactorSourceAlpha;
      ca.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
      ca.destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;

      psd.sampleCount                = 1;
      psd.vertexDescriptor           = vd;
      psd.vertexFunction             = RARCH_AUTORELEASE_R([_context.library newFunctionWithName:@"sprite_vertex"]);
      psd.fragmentFunction           = RARCH_AUTORELEASE_R([_context.library newFunctionWithName:
            (_atlas->format == FONT_ATLAS_FORMAT_A16)
                  ? @"sprite_fragment_a16"
                  : @"sprite_fragment_a8"]);

      if (!psd.vertexFunction || !psd.fragmentFunction)
         return NO;

      _state                         = [_context sdrState:psd error:&err];
      _stateF16                      = [_context twinF16:psd];
      if (err != nil)
         return NO;
   }

   {
      MTLSamplerDescriptor *sd = RARCH_AUTORELEASE_R([MTLSamplerDescriptor new]);
      sd.minFilter             = MTLSamplerMinMagFilterLinear;
      sd.magFilter             = MTLSamplerMinMagFilterLinear;
      _sampler                 = [_context.device newSamplerStateWithDescriptor:sd];
   }
   return YES;
}

/* Copies the atlas's dirty region into the buffer the texture is made
 * on. Whole cells go across, so no texel of a cell's earlier glyph is
 * left beside a smaller one for filtering to pick up. */
- (void)updateDirty
{
   if (_atlas->dirty)
   {
      unsigned row;
      unsigned x0 = VIDEO_SCALE_W(_atlas->dirty_xy0);
      unsigned y0 = VIDEO_SCALE_H(_atlas->dirty_xy0);
      unsigned x1 = VIDEO_SCALE_W(_atlas->dirty_xy1);
      unsigned y1 = VIDEO_SCALE_H(_atlas->dirty_xy1);
      _atlas->dirty = false;
      if (     x1 <= x0 || y1 <= y0
            || x1 > _atlas->width || y1 > _atlas->height)
         return;
      for (row = y0; row < y1; row++)
         memcpy((uint8_t *)_buffer.contents
                  + (size_t)row * _stride + (size_t)x0 * _esz,
               _atlas->buffer
                  + ((size_t)row * _atlas->width + x0) * _esz,
               (size_t)(x1 - x0) * _esz);

#if !defined(HAVE_COCOATOUCH)
      /* didModifyRange takes a byte range */
      [_buffer didModifyRange:NSMakeRange((NSUInteger)y0 * _stride,
            (NSUInteger)(y1 - y0) * _stride)];
#endif
   }
}

- (int)getWidthForMessage:(const char *)msg length:(NSUInteger)length scale:(float)scale
{
   const struct font_glyph* (*get_glyph)(void*, uint32_t);
   const struct font_glyph* glyph_q;
   void *font_data;
   size_t msg_len = length;
   int width      = 0;

   /* Validate font data before use - can become invalid during
    * video context reset or if font was freed while in use */
   if (!_font_driver || !_font_data)
      return 0;

   get_glyph = _font_driver->get_glyph;
   font_data = _font_data;
   glyph_q   = get_glyph(font_data, '?');
   /* Every lookup, the fallback glyph's included (it can itself have
    * just been rasterized after eviction), is paired with an update of
    * its cell, so no cell is stranded when an unrelated glyph clears
    * the dirty flag. */
   if (glyph_q)
      [self updateDirty];
#define FONT_MEASURE_DIRTY(glyph) \
   do { (void)(glyph); [self updateDirty]; } while (0)
#define FONT_MEASURE_SUM width
#include "../font_measure.h"
   return (int)(width * scale);
}

- (const struct font_glyph *)getGlyph:(uint32_t)code
{
   const struct font_glyph *glyph;
   if (!_font_driver || !_font_data)
      return NULL;
   glyph = _font_driver->get_glyph(_font_data, code);
   if (glyph)
      [self updateDirty];
   return glyph;
}

- (bool)getLineMetrics:(struct font_line_metrics **)metrics
{
   if (_font_driver && _font_data)
   {
      _font_driver->get_line_metrics(_font_data, metrics);
      return true;
   }
   return false;
}

static INLINE void write_quad6(SpriteVertex *pv,
      float x, float y, float width, float height,
      float tex_x, float tex_y, float tex_width, float tex_height,
      uint64_t color)
{
   int i;
   static const float strip[2 * 6] = {
      0.0f, 0.0f,
      0.0f, 1.0f,
      1.0f, 0.0f,
      1.0f, 1.0f,
      1.0f, 0.0f,
      0.0f, 1.0f,
   };

   for (i = 0; i < 6; i++)
   {
      pv[i].position = simd_make_float2(
            x + strip[2 * i + 0] * width,
            y + strip[2 * i + 1] * height);
      pv[i].texCoord = simd_make_float2(
            tex_x + strip[2 * i + 0] * tex_width,
            tex_y + strip[2 * i + 1] * tex_height);
      pv[i].color    = color;
   }
}

- (void)_flush
{
   if (_vertices == 0)
      return;

   id<MTLRenderCommandEncoder> rce = _context.rce;
   [rce pushDebugGroup:@"render fonts"];

   [_context resetRenderViewport:kFullscreenViewport];
   [rce setRenderPipelineState:(_context.hdrEnabled && _stateF16)
      ? _stateF16 : _state];
   [rce setVertexBytes:&_uniforms length:sizeof(Uniforms) atIndex:BufferIndexUniforms];
   [rce setVertexBuffer:_range.buffer offset:_range.offset atIndex:BufferIndexPositions];
   [rce setFragmentTexture:_texture atIndex:TextureIndexColor];
   [rce setFragmentSamplerState:_sampler atIndex:SamplerIndexDraw];
   [rce drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:_vertices];
   [rce popDebugGroup];

   _vertices = 0;
}

/* Lays a message out through gfx/font_layout.h, appending its glyph
 * quads to the vertex range; every glyph lookup, the '?' stand-in's
 * included, is paired with an update of that glyph's atlas cell, so a
 * cell is never stranded when a later lookup clears the dirty flag. */
- (void)renderMessage:(const char *)msg
               length:(NSUInteger)msg_len
               height:(unsigned)height
                scale:(float)scale
                color:(vector_float4)color
                 posX:(float)posX
                 posY:(float)posY
              aligned:(unsigned)aligned
{
   float line_height;
   float inv_tex_size_x;
   float inv_tex_size_y;
   float inv_win_width;
   float inv_win_height;
   float rgba[4];
   uint64_t packed;
   unsigned full_w;
   unsigned full_h;
   SpriteVertex *v;
   struct font_line_metrics *line_metrics = NULL;
   const struct font_glyph *glyph_q       = NULL;
   const struct font_glyph* (*get_glyph)(void*, uint32_t);
   void *font_data;
   int x                                  = 0;
   int y                                  = 0;

   /* Validate font data before use - can become invalid during
    * video context reset or if font was freed while in use */
   if (!_font_driver || !_font_data)
      return;

   [self _followAtlas];

   get_glyph        = _font_driver->get_glyph;
   font_data        = _font_data;
   _font_driver->get_line_metrics(_font_data, &line_metrics);
   line_height      = line_metrics->height * scale / height;

   rgba[0]          = color.x;
   rgba[1]          = color.y;
   rgba[2]          = color.z;
   rgba[3]          = color.w;
   packed           = rgba16_pack(rgba);
   full_w           = VIDEO_SCALE_W(_driver.viewport->full_dims);
   full_h           = VIDEO_SCALE_H(_driver.viewport->full_dims);
   inv_tex_size_x   = 1.0f / _texture.width;
   inv_tex_size_y   = 1.0f / _texture.height;
   inv_win_width    = 1.0f / full_w;
   inv_win_height   = 1.0f / full_h;

   glyph_q          = get_glyph(font_data, '?');
   if (glyph_q)
      [self updateDirty];

   v                = (SpriteVertex *)_range.data + _vertices;

#define FONT_LAYOUT_ALIGNED (aligned == TEXT_ALIGN_RIGHT \
      || aligned == TEXT_ALIGN_CENTER)
#define FONT_LAYOUT_DIRTY(glyph) \
   do { (void)(glyph); [self updateDirty]; } while (0)
#define FONT_LAYOUT_LINE(line, line_width, count, bytes) \
   do \
   { \
      (void)(count); \
      x = (int)roundf(posX * full_w); \
      y = (int)roundf((1.0f - (posY - (float)(line) * line_height)) \
            * full_h); \
      if (aligned == TEXT_ALIGN_RIGHT) \
         x -= (int)((line_width) * scale); \
      else if (aligned == TEXT_ALIGN_CENTER) \
         x -= (int)((line_width) * scale) / 2; \
   } while (0)
#define FONT_LAYOUT_GLYPH(glyph, pen_x, pen_y) \
   do \
   { \
      int off_x  = (glyph)->draw_offset_x; \
      int off_y  = (glyph)->draw_offset_y; \
      int tex_x  = VIDEO_SCALE_W((glyph)->atlas_pos); \
      int tex_y  = VIDEO_SCALE_H((glyph)->atlas_pos); \
      int g_w    = VIDEO_SCALE_W((glyph)->dims); \
      int g_h    = VIDEO_SCALE_H((glyph)->dims); \
      write_quad6(v, \
            (x + (off_x + (pen_x)) * scale) * inv_win_width, \
            (y + (off_y + (pen_y)) * scale) * inv_win_height, \
            g_w * scale * inv_win_width, \
            g_h * scale * inv_win_height, \
            tex_x * inv_tex_size_x, \
            tex_y * inv_tex_size_y, \
            g_w * inv_tex_size_x, \
            g_h * inv_tex_size_y, \
            packed); \
      _vertices += 6; \
      v         += 6; \
   } while (0)
#include "../font_layout.h"
}

- (void)renderMessage:(const char *)msg
               length:(NSUInteger)msg_len
                width:(unsigned)width
               height:(unsigned)height
               params:(const struct font_params *)params
{
   font_params_resolved_t rp;
   float x, y, scale, drop_mod, drop_alpha;
   int drop_x, drop_y;
   enum text_alignment text_align;
   vector_float4 color;

   if (!msg || !*msg)
      return;

   font_driver_resolve_params(params, &rp);
   x           = rp.x;
   y           = rp.y;
   scale       = rp.scale;
   text_align  = rp.text_align;
   drop_x      = rp.drop_x;
   drop_y      = rp.drop_y;
   drop_mod    = rp.drop_mod;
   drop_alpha  = rp.drop_alpha;
   {
      const float *c = rp.color_hp ? rp.color_hp : rp.color;
      color          = simd_make_float4(c[0], c[1], c[2], c[3]);
   }


   @autoreleasepool
   {
      size_t max_glyphs = strlen(msg);
      if (drop_x || drop_y)
         max_glyphs *= 2;

      NSUInteger needed = max_glyphs * 6 * sizeof(SpriteVertex);
      if (![_context allocRange:&_range length:needed])
         return;

      _vertices = 0;

      if (drop_x || drop_y)
      {
         vector_float4 color_dark;
         color_dark.x = color.x * drop_mod;
         color_dark.y = color.y * drop_mod;
         color_dark.z = color.z * drop_mod;
         color_dark.w = color.w * drop_alpha;

         [self renderMessage:msg
                      length:msg_len
                      height:height
                       scale:scale
                       color:color_dark
                        posX:x + scale * drop_x / width
                        posY:y + scale * drop_y / height
                     aligned:text_align];
      }

      [self renderMessage:msg
                   length:msg_len
                   height:height
                    scale:scale
                    color:color
                     posX:x
                     posY:y
                  aligned:text_align];

      [self _flush];
   }
}

@end

static void *metal_raster_font_init(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   @autoreleasepool
   {
      MetalRaster *r = [[MetalRaster alloc] initWithDriver:(__bridge MetalDriver *)data fontPath:font_path fontSize:(unsigned)font_size];

      if (!r)
         return NULL;

      return RARCH_BRIDGE_RETAINED(RARCH_AUTORELEASE_R(r));
   }
}

static void metal_raster_font_free(void *data, bool is_threaded)
{
   @autoreleasepool
   {
      MetalRaster *r = RARCH_BRIDGE_TRANSFER(MetalRaster *, data);

      [r deinit];
      r = nil;
   }
}

static int metal_raster_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{
   MetalRaster *r = (__bridge MetalRaster *)data;
   return [r getWidthForMessage:msg length:(unsigned)msg_len scale:scale];
}

static void metal_raster_font_render_msg(
      void *userdata,
      void *data, const char *msg, size_t msg_len,
      const struct font_params *params)
{
   MetalRaster *r       = (__bridge MetalRaster *)data;
   MetalDriver *d       = (__bridge MetalDriver *)userdata;
   video_viewport_t *vp = [d viewport];
   unsigned width       = VIDEO_SCALE_W(vp->full_dims);
   unsigned height      = VIDEO_SCALE_H(vp->full_dims);
   [r renderMessage:msg length:msg_len width:width height:height
             params:params];
}

static const struct font_glyph *metal_raster_font_get_glyph(
   void *data, uint32_t code)
{
   MetalRaster *r = (__bridge MetalRaster *)data;
   return [r getGlyph:code];
}

static bool metal_get_line_metrics(void *data,
      struct font_line_metrics **metrics)
{
   MetalRaster *r = (__bridge MetalRaster *)data;
   if (r)
      return [r getLineMetrics:metrics];
   return false;
}

/*
 * VIDEO DRIVER
 */

@implementation MetalView

#if !defined(HAVE_COCOATOUCH)
- (void)keyDown:(NSEvent*)theEvent { }
#endif

/* Stop the annoying sound when pressing a key. */
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)isFlipped { return YES; }
@end

#pragma mark - private categories

@interface FrameView()

@property (nonatomic, readwrite) video_viewport_t *viewport;

- (instancetype)initWithDescriptor:(ViewDescriptor *)td context:(Context *)context;
- (void)drawWithContext:(Context *)ctx;
- (void)drawWithEncoder:(id<MTLRenderCommandEncoder>)rce;

@end

@interface MetalMenu()
@property (nonatomic, readonly) TexturedView *view;
- (instancetype)initWithContext:(Context *)context;
@end

@interface Overlay()
- (instancetype)initWithContext:(Context *)context;
- (void)drawWithEncoder:(id<MTLRenderCommandEncoder>)rce;
@end

/* Context for the cached_frame_read callback that pulls the last
 * cached core frame into the FrameView after a driver
 * (re-)init.  The callback runs inside the cached_frame_read
 * lifetime envelope, so the pixel pointer is safe for the
 * duration of the [view updateFrame:pitch:] call -- no need for
 * the conditional NULL-checks the previous direct-field-read
 * version had to do, and no UAF window if a concurrent core
 * close races the render thread. */
struct metal_pull_cached_ctx
{
   FrameView *view;
   bool      *uploaded_flag;
};

static void metal_pull_cached_frame_cb(void *userdata,
      const void *data,
      unsigned dims, size_t pitch)
{
   struct metal_pull_cached_ctx *ctx
      = (struct metal_pull_cached_ctx*)userdata;
   if (     !ctx || !data || !VIDEO_SCALE_W(dims)
         || !VIDEO_SCALE_H(dims) || !pitch)
      return;
   ctx->view.size = CGSizeMake(VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));
   [ctx->view updateFrame:data pitch:pitch];
   *ctx->uploaded_flag = true;
}

/* A texture the views draw into, owned through RARCH_STRUCT_ASSIGN */
typedef struct metal_views_texture
{
   __unsafe_unretained id<MTLTexture> tex;
} metal_views_texture_t;

/* Views, drawn straight from the frame as the stock pipeline draws it,
 * or each through its copy of the preset's chain. A view's stock chain
 * is the views state itself. The canvas holds both eyes for anaglyph
 * and interlaced, the UI layer the UI drawn once for both eyes. The
 * objects the driver owns are referenced, the pipelines and textures
 * owned through RARCH_STRUCT_ASSIGN. */
typedef struct metal_views
{
   video_views_chains_t  chains;
   metal_views_texture_t canvas;
   metal_views_texture_t ui;
   video_views_target_t  canvas_target;
   video_views_target_t  ui_target;
   video_views_ui_dims_t saved;
   __unsafe_unretained id<MTLDevice>               device;
   __unsafe_unretained Context                    *context;
   __unsafe_unretained FrameView                  *frameView;
   __unsafe_unretained id<MTLSamplerState>         nearest;
   __unsafe_unretained id<MTLSamplerState>         linear;
   __unsafe_unretained id<MTLRenderPipelineState>  rect;
   __unsafe_unretained id<MTLRenderPipelineState>  layer;
   __unsafe_unretained id<MTLRenderPipelineState>  anaglyph;
   __unsafe_unretained id<MTLRenderPipelineState>  interlaced;
   /* For get_flags on the main thread: whether views may be drawn,
    * and whether the last frame with a map was drawn whole. */
   retro_atomic_int_t    allowed;
   retro_atomic_int_t    fallback;
   bool                  tried;
   bool                  failed;
   bool                  active;
   bool                  direct_seen;
} metal_views_t;

/* The views shaders' constants */
typedef struct
{
   matrix_float4x4 mvp;
   simd_float4     rect;
   simd_float4     clamp;
} metal_views_ubo_t;

/* What a frame's placement draws read: the frame, the encoder the
 * placements go into and the sampler they read with */
struct metal_views_draw
{
   const video_frame_info_t *video_info;
   __unsafe_unretained id<MTLRenderCommandEncoder> rce;
   __unsafe_unretained id<MTLSamplerState>         sampler;
   unsigned src_dims;
};

static void metal_views_draw_direct(metal_views_t *vs,
      const struct metal_views_draw *d, unsigned view,
      const video_views_rect_t *dst, const video_views_rect_t *cut);
static void metal_views_draw_pass(metal_views_t *vs,
      const struct metal_views_draw *d, metal_view_chain_t *c,
      const video_views_rect_t *dst, const video_views_rect_t *cut);
static bool metal_views_make(metal_views_t *vs, metal_views_texture_t *t,
      unsigned dims);
static bool metal_views_retire(metal_views_t *vs, metal_views_texture_t *t);

/* The views compositor, with these calls bound in. The stock chain's
 * views are drawn straight from the frame, so their chain is the views
 * state, never drawn; a preset's are copies of the main chain's, which
 * read its parameters. */
#define VV_FN(name)                     metal_vv_##name
#define VV_DRV                          metal_views_t
#define VV_CHAIN                        metal_view_chain_t
#define VV_CREATE_STOCK(drv)            ((void*)(drv))
#define VV_SYNC_PARAMS(drv, c)          ((void)0)
#define VV_DRAW                         struct metal_views_draw
#define VV_DIRECT(p, d, v, r, c)        metal_views_draw_direct(p, d, v, r, c)
#define VV_PASS(p, d, f, r, c, a)       metal_views_draw_pass(p, d, f, r, c)
#define VV_TARGET                       metal_views_texture_t
#define VV_TARGET_MAKE(drv, t, dims)    metal_views_make(drv, t, dims)
#define VV_TARGET_RETIRE(drv, t)        metal_views_retire(drv, t)
#include "../video_views_draw.h"

/* Views: not with HDR output, or without their shaders */
static bool metal_views_allowed(const metal_views_t *vs)
{
   return !vs->failed && !vs->context.hdrEnabled;
}

static void metal_views_release(metal_views_t *vs)
{
   RARCH_STRUCT_ASSIGN(vs->rect,       nil);
   RARCH_STRUCT_ASSIGN(vs->layer,      nil);
   RARCH_STRUCT_ASSIGN(vs->anaglyph,   nil);
   RARCH_STRUCT_ASSIGN(vs->interlaced, nil);
   RARCH_STRUCT_ASSIGN(vs->canvas.tex, nil);
   RARCH_STRUCT_ASSIGN(vs->ui.tex,     nil);
}

/* @fn's pipeline into BGRA8, with blending premultiplied */
static id<MTLRenderPipelineState> metal_views_pipeline(id<MTLDevice> device,
      id<MTLLibrary> lib, NSString *fn, bool blend)
{
   NSError *err = nil;
   MTLRenderPipelineDescriptor *psd = RARCH_AUTORELEASE_R(
         [MTLRenderPipelineDescriptor new]);
   MTLRenderPipelineColorAttachmentDescriptor *ca = psd.colorAttachments[0];
   psd.label                      = fn;
   psd.vertexFunction             = RARCH_AUTORELEASE_R([lib
         newFunctionWithName:@"views_vertex"]);
   psd.fragmentFunction           = RARCH_AUTORELEASE_R([lib
         newFunctionWithName:fn]);
   ca.pixelFormat                 = MTLPixelFormatBGRA8Unorm;
   ca.blendingEnabled             = blend ? YES : NO;
   ca.sourceRGBBlendFactor        = MTLBlendFactorOne;
   ca.destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;
   ca.sourceAlphaBlendFactor      = MTLBlendFactorOne;
   ca.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
   if (!psd.vertexFunction || !psd.fragmentFunction)
      return nil;
   return RARCH_AUTORELEASE_R([device
         newRenderPipelineStateWithDescriptor:psd error:&err]);
}

/* The pipelines views draw with, made on first use: most sessions never
 * draw views. */
static void metal_views_init(metal_views_t *vs)
{
   /* Rect: the part of the texture drawn, top-left first. Clamp: its
    * first and last texel centres, so filtering stops at its edge as at
    * a texture's. The canvas holds the left eye on its left half, the
    * right eye on its right; anaglyph is Dubois' least-squares red/cyan,
    * interlaced takes even rows from the left half, odd from the right.
    * The UI layer is premultiplied. */
   static NSString *src = @
      "#include <metal_stdlib>\n"
      "using namespace metal;\n"
      "struct ViewsUBO { float4x4 mvp; float4 rect; float4 clamp; };\n"
      "struct ViewsOut { float4 position [[position]]; float2 texcoord; };\n"
      "vertex ViewsOut views_vertex(uint vid [[vertex_id]],"
      " constant ViewsUBO &u [[buffer(0)]])\n"
      "{\n"
      "   ViewsOut out;\n"
      "   float2 p     = float2(float(vid & 1u), float(vid >> 1));\n"
      "   out.position = u.mvp * float4(p, 0.0, 1.0);\n"
      "   out.texcoord = mix(u.rect.xy, u.rect.zw, p);\n"
      "   return out;\n"
      "}\n"
      "fragment float4 views_rect(ViewsOut in [[stage_in]],"
      " constant ViewsUBO &u [[buffer(0)]],"
      " texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]])\n"
      "{\n"
      "   return float4(t.sample(s,"
      " clamp(in.texcoord, u.clamp.xy, u.clamp.zw)).rgb, 1.0);\n"
      "}\n"
      "fragment float4 views_layer(ViewsOut in [[stage_in]],"
      " texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]])\n"
      "{\n"
      "   return t.sample(s, in.texcoord);\n"
      "}\n"
      "fragment float4 views_anaglyph(ViewsOut in [[stage_in]],"
      " texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]])\n"
      "{\n"
      "   float2 uv = float2(in.texcoord.x * 0.5, in.texcoord.y);\n"
      "   float3 l  = t.sample(s, uv).rgb;\n"
      "   float3 r  = t.sample(s, uv + float2(0.5, 0.0)).rgb;\n"
      "   float3 c  = float3(\n"
      "         dot(l, float3( 0.456100,  0.500484,  0.176381))\n"
      "       + dot(r, float3(-0.0434706, -0.0879388, -0.00155529)),\n"
      "         dot(l, float3(-0.0400822, -0.0378246, -0.0157589))\n"
      "       + dot(r, float3( 0.378476,   0.73364,   -0.0184503)),\n"
      "         dot(l, float3(-0.0152161, -0.0205971, -0.00546856))\n"
      "       + dot(r, float3(-0.0721527, -0.112961,   1.2264)));\n"
      "   return float4(saturate(c), 1.0);\n"
      "}\n"
      "fragment float4 views_interlaced(ViewsOut in [[stage_in]],"
      " texture2d<float> t [[texture(0)]], sampler s [[sampler(0)]])\n"
      "{\n"
      "   float row = floor(in.texcoord.y * float(t.get_height()));\n"
      "   return float4(t.sample(s, float2(in.texcoord.x * 0.5"
      " + 0.5 * fmod(row, 2.0), in.texcoord.y)).rgb, 1.0);\n"
      "}\n";
   NSError *err      = nil;
   id<MTLLibrary> lib;

   vs->tried = true;
   lib       = RARCH_AUTORELEASE_R([vs->device newLibraryWithSource:src
            options:nil error:&err]);
   if (lib)
   {
      RARCH_STRUCT_ASSIGN(vs->rect, metal_views_pipeline(vs->device, lib,
               @"views_rect", false));
      RARCH_STRUCT_ASSIGN(vs->anaglyph, metal_views_pipeline(vs->device, lib,
               @"views_anaglyph", false));
      RARCH_STRUCT_ASSIGN(vs->interlaced, metal_views_pipeline(vs->device,
               lib, @"views_interlaced", false));
      RARCH_STRUCT_ASSIGN(vs->layer, metal_views_pipeline(vs->device, lib,
               @"views_layer", true));
   }
   if (vs->rect && vs->anaglyph && vs->interlaced && vs->layer)
      return;
   RARCH_WARN("[Metal] Views can't be drawn: their shaders failed: %s.\n",
         err ? err.localizedDescription.UTF8String : "");
   metal_views_release(vs);
   vs->failed = true;
}

static bool metal_views_retire(metal_views_t *vs, metal_views_texture_t *t)
{
   (void)vs;
   RARCH_STRUCT_ASSIGN(t->tex, nil);
   return true;
}

/* A render target of BGRA8 sized @dims, which the preset's last pass
 * can draw into */
static bool metal_views_make(metal_views_t *vs, metal_views_texture_t *t,
      unsigned dims)
{
   MTLTextureDescriptor *td = [MTLTextureDescriptor
      texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
      width:VIDEO_SCALE_W(dims) height:VIDEO_SCALE_H(dims) mipmapped:NO];
   td.usage       = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
   td.storageMode = MTLStorageModePrivate;
   RARCH_STRUCT_ASSIGN(t->tex, RARCH_AUTORELEASE_R([vs->device
            newTextureWithDescriptor:td]));
   if (t->tex)
      return true;
   RARCH_WARN("[Metal] Views can't draw into a %ux%u image. "
         "Drawing without it.\n", VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));
   return false;
}

/* A top-left rectangle as a viewport, and as a scissor rectangle */
static MTLViewport metal_views_vp(const video_views_rect_t *r)
{
   MTLViewport vp;
   vp.originX = (double)VIDEO_POS_X(r->pos);
   vp.originY = (double)VIDEO_POS_Y(r->pos);
   vp.width   = (double)VIDEO_SCALE_W(r->dims);
   vp.height  = (double)VIDEO_SCALE_H(r->dims);
   vp.znear   = 0.0;
   vp.zfar    = 1.0;
   return vp;
}

static MTLScissorRect metal_views_sc(const video_views_rect_t *r)
{
   MTLScissorRect sc;
   sc.x      = (NSUInteger)VIDEO_POS_X(r->pos);
   sc.y      = (NSUInteger)VIDEO_POS_Y(r->pos);
   sc.width  = VIDEO_SCALE_W(r->dims);
   sc.height = VIDEO_SCALE_H(r->dims);
   return sc;
}

/* The constants: @mvp, the part of the texture drawn and where its
 * filtering stops */
static void metal_views_ubo(id<MTLRenderCommandEncoder> rce,
      const matrix_float4x4 *mvp, simd_float4 rect, simd_float4 clamp)
{
   metal_views_ubo_t u;
   u.mvp   = *mvp;
   u.rect  = rect;
   u.clamp = clamp;
   [rce setVertexBytes:&u length:sizeof(u) atIndex:0];
   [rce setFragmentBytes:&u length:sizeof(u) atIndex:0];
}

/* View @view of the frame over @dst, cut to @cut, as the stock pipeline
 * draws the frame: the same texels, the same filter, stopping at the
 * view's edge as at a texture's. */
static void metal_views_draw_direct(metal_views_t *vs,
      const struct metal_views_draw *d, unsigned view,
      const video_views_rect_t *dst, const video_views_rect_t *cut)
{
   simd_float4 rect, clamp;
   const struct retro_video_view *v = &d->video_info->views.views[view];
   float tw = (float)VIDEO_SCALE_W(d->src_dims);
   float th = (float)VIDEO_SCALE_H(d->src_dims);

   rect  = simd_make_float4((float)v->x / tw, (float)v->y / th,
         (float)(v->x + v->width) / tw, (float)(v->y + v->height) / th);
   clamp = simd_make_float4(rect.x + 0.5f / tw, rect.y + 0.5f / th,
         rect.z - 0.5f / tw, rect.w - 0.5f / th);
   metal_views_ubo(d->rce, &vs->context.uniforms->projectionMatrix,
         rect, clamp);
   [d->rce setViewport:metal_views_vp(dst)];
   [d->rce setScissorRect:metal_views_sc(cut)];
   [d->rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0
              vertexCount:4];
}

/* A placement through its view's chain: its last pass drawn into the
 * placement, or the render target it drew into placed there */
static void metal_views_draw_pass(metal_views_t *vs,
      const struct metal_views_draw *d, metal_view_chain_t *c,
      const video_views_rect_t *dst, const video_views_rect_t *cut)
{
   id<MTLTexture> final = [vs->frameView viewChainFinal:c];

   if (final)
   {
      float tw = (float)final.width;
      float th = (float)final.height;
      [d->rce setRenderPipelineState:vs->rect];
      [d->rce setFragmentTexture:final atIndex:0];
      [d->rce setFragmentSamplerState:d->sampler atIndex:0];
      metal_views_ubo(d->rce, &vs->context.uniforms->projectionMatrix,
            simd_make_float4(0.0f, 0.0f, 1.0f, 1.0f),
            simd_make_float4(0.5f / tw, 0.5f / th,
               1.0f - 0.5f / tw, 1.0f - 0.5f / th));
   }
   else if (![vs->frameView bindViewChainLast:c encoder:d->rce])
      return;
   [d->rce setViewport:metal_views_vp(dst)];
   [d->rce setScissorRect:metal_views_sc(cut)];
   [d->rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0
              vertexCount:4];
}

/* @set's preset chains from index @first on freed */
static void metal_views_free_chains(metal_views_t *vs, unsigned first)
{
   unsigned i;
   for (i = first; i < RETRO_VIDEO_VIEWS_MAX; i++)
   {
      if (vs->chains.preset[i])
         [vs->frameView freeViewChain:
            (metal_view_chain_t *)vs->chains.preset[i]];
      vs->chains.preset[i] = NULL;
   }
}

/* A preset chain for each of views [0, count) that has none, copied
 * from the main chain: nothing is compiled or read again. */
static void metal_views_build_chains(metal_views_t *vs)
{
   unsigned i;
   for (i = 0; i < vs->chains.count; i++)
      if (     !vs->chains.preset[i]
            && !(vs->chains.preset[i] = [vs->frameView makeViewChain]))
         break;
}

static void metal_views_set_count(metal_views_t *vs, unsigned count)
{
   unsigned i;
   vs->chains.count = MIN(count, RETRO_VIDEO_VIEWS_MAX);
   for (i = vs->chains.count; i < RETRO_VIDEO_VIEWS_MAX; i++)
      vs->chains.stock[i] = NULL;
   metal_views_free_chains(vs, vs->chains.count);
}

/* Whether this frame draws its views, with each drawn view's chain
 * picked and the rectangle its first placement takes; in *blend whether
 * they draw into the canvas, in *direct whether straight from the frame
 * (the stock pipeline). @frame_dims is the frame's size. */
static bool metal_views_setup(metal_views_t *vs,
      const video_frame_info_t *video_info, const void *frame,
      unsigned frame_dims, bool *blend, bool *direct,
      metal_view_chain_t **chains, video_views_rect_t *rects)
{
   bool views;
   const video_views_layout_t *layout = &video_info->views_layout;
   struct video_shader *shader        = vs->frameView.shader;
   bool offscreen = video_info->views.num_views && layout->offscreen;
   bool stock     = !video_info->shader_active || !shader || !shader->passes;

   if (video_info->views.num_views && !vs->tried)
      metal_views_init(vs);
   /* Anaglyph and interlaced draw both eyes into the canvas, or without
    * one only the left eye's area into the window. */
   *blend = offscreen && !vs->failed
      && metal_vv_target_ensure(vs, &vs->canvas_target, &vs->canvas,
            layout->canvas_dims);
   /* A real frame without the blend modes frees the canvas. */
   if (frame && !offscreen)
      metal_vv_target_drop(vs, &vs->canvas_target, &vs->canvas);
   views = video_info->views.num_views
      && metal_views_allowed(vs)
      && video_views_fit_frame(&video_info->views, frame_dims);
   if (views && !stock)
      metal_views_build_chains(vs);
   views = views
      && metal_vv_pick_chains(vs, &vs->chains, &video_info->views, layout,
            stock, *blend || !layout->offscreen,
            *blend ? layout->canvas_dims : video_info->dims, chains, rects);
   *blend  = views && *blend;
   *direct = views && stock;
   if (*direct && !vs->direct_seen)
   {
      vs->direct_seen = true;
      RARCH_LOG("[Metal] Views drawn straight from the frame.\n");
   }
   return views;
}

/* Draws the views of the frame into the drawable, or with @blend into
 * the canvas, whose halves one pass then blends into the drawable: with
 * @direct straight from the frame, else each through its chain, whose
 * passes are drawn first, for a @fresh frame from a new copy of its
 * view. The canvas and the chains' passes go on the blit command buffer,
 * which runs before the frame's. */
static void metal_views_render(metal_views_t *vs,
      const video_frame_info_t *video_info, metal_view_chain_t **chains,
      const video_views_rect_t *rects, bool blend, bool direct, bool fresh,
      bool smooth)
{
   unsigned i;
   unsigned dims;
   struct metal_views_draw d;
   id<MTLRenderCommandEncoder> rce;
   const video_views_layout_t *layout = &video_info->views_layout;
   Context *ctx                       = vs->context;
   id<MTLTexture> src                 = vs->frameView.frameTexture;
   CGSize size                        = vs->frameView.size;

   if (!src)
      return;
   for (i = 0; !direct && i < video_info->views.num_views; i++)
      if (chains[i])
         [vs->frameView renderViewChain:chains[i]
                                   view:&video_info->views.views[i]
                               viewport:rects[i].dims fresh:fresh];

   if (blend)
   {
      MTLRenderPassDescriptor *rpd = RARCH_AUTORELEASE_R(
            [MTLRenderPassDescriptor new]);
      rpd.colorAttachments[0].clearColor  = MTLClearColorMake(0, 0, 0, 0);
      rpd.colorAttachments[0].loadAction  = MTLLoadActionClear;
      rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
      rpd.colorAttachments[0].texture     = vs->canvas.tex;
      rce = [ctx.blitCommandBuffer renderCommandEncoderWithDescriptor:rpd];
      rce.label = @"Views canvas";
      dims      = layout->canvas_dims;
   }
   else
   {
      /* The drawable's own size: a threaded frame can carry a layout
       * for a larger window, and a scissor rect must lie inside it. */
      id<MTLTexture> t = ctx.nextDrawable.texture;
      rce  = ctx.rce;
      dims = t ? VIDEO_SCALE_PACK((unsigned)t.width, (unsigned)t.height)
         : video_info->dims;
   }
   if (!rce)
      return;

   d.video_info = video_info;
   d.rce        = rce;
   d.sampler    = smooth ? vs->linear : vs->nearest;
   d.src_dims   = VIDEO_SCALE_PACK((unsigned)size.width,
         (unsigned)size.height);
   [rce setRenderPipelineState:vs->rect];
   [rce setFragmentTexture:src atIndex:0];
   [rce setFragmentSamplerState:d.sampler atIndex:0];
   metal_vv_draw_placements(vs, &d, layout, chains,
         blend || !layout->offscreen, dims, direct);

   if (blend)
   {
      MTLViewport vp;
      [rce endEncoding];
      if (!(rce = ctx.rce))
         return;
      vp.originX = 0.0;
      vp.originY = 0.0;
      vp.width   = (double)VIDEO_SCALE_W(video_info->dims);
      vp.height  = (double)VIDEO_SCALE_H(video_info->dims);
      vp.znear   = 0.0;
      vp.zfar    = 1.0;
      [rce setRenderPipelineState:
         (layout->stereo_mode == VIDEO_STEREO_MODE_INTERLACED)
         ? vs->interlaced : vs->anaglyph];
      [rce setFragmentTexture:vs->canvas.tex atIndex:0];
      [rce setFragmentSamplerState:vs->nearest atIndex:0];
      metal_views_ubo(rce, &ctx.uniformsNoRotate->projectionMatrix,
            simd_make_float4(0.0f, 0.0f, 1.0f, 1.0f),
            simd_make_float4(0.0f, 0.0f, 1.0f, 1.0f));
      [rce setViewport:vp];
      [rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0
                vertexCount:4];
   }
   else
      [ctx resetScissorRect];
}

@interface MetalDriver()
- (metal_views_t *)views;
@end

@implementation MetalDriver
{
   FrameView *_frameView;
   MetalMenu *_menu;
   Overlay *_overlay;
   metal_views_t _views;

   video_info_t _video;

   id<MTLDevice> _device;
   id<MTLLibrary> _library;
   Context *_context;

   CAMetalLayer *_layer;

   /* Render target layer state */
   id<MTLRenderPipelineState> _t_pipelineState;
   id<MTLRenderPipelineState> _t_pipelineStateNoAlpha;
   /* The HDR UI overlay's */
   id<MTLRenderPipelineState> _t_pipelineStateF16;
   id<MTLRenderPipelineState> _t_pipelineStateNoAlphaF16;

   id<MTLSamplerState> _samplerStateLinear;
   id<MTLSamplerState> _samplerStateNearest;

   /* other state */
   Uniforms _viewportMVP;

   /* HDR output mode locked in at initWithVideo time.  Runtime toggling
    * would require rebuilding every pipeline whose colour attachment was
    * baked against _layer.pixelFormat (stock blit, menu, font, slang
    * shader passes) — out of scope for this implementation.  Matching
    * Vulkan's behaviour: HDR is effectively an init-time choice that
    * takes effect on the next driver reinit. */
   unsigned _initial_hdr_mode;

   /* Tracks whether _frameView has received any real frame since this
    * driver instance came up.  When the HDR mode is toggled mid-session
    * the frontend fires CMD_EVENT_REINIT, destroying and recreating the
    * driver.  If the user was in the menu with a core paused in the
    * background, the new driver's frame texture is empty and subsequent
    * menu-loop frames arrive with frame==NULL, leaving hdrComposite
    * nothing to sample — the paused core background disappears until
    * the user F1-cycles.  On the first frame==NULL after reinit we fall
    * back to the frontend's frame_cache_data to repopulate the texture
    * ourselves. */
   bool _frameEverUploaded;

   /* List of selectable Metal devices reported to the frontend so the
    * "GPU Index" menu entry can enumerate them.  Owned by this driver
    * instance: created in init, published via
    * video_driver_set_gpu_api_devices(), freed in dealloc (where we
    * also clear the frontend pointer).  On iOS/tvOS this is always a
    * 1-element list containing the system default device, since
    * MTLCopyAllDevices() is macOS-only. */
   struct string_list *_gpu_list;
}

- (instancetype)initWithVideo:(const video_info_t *)video
{
   if (self = [super init])
   {
      /* Build the list of selectable Metal devices and pick one per the
       * user-configured metal_gpu_index.
       *
       * iOS/tvOS: MTLCopyAllDevices() is macOS-only (explicitly marked
       * API_UNAVAILABLE(ios) in <Metal/MTLDevice.h>), and these platforms
       * only ever expose a single integrated GPU.  We publish a
       * one-element list containing the system default device so the
       * frontend's GPU-index menu entry shows something sensible, and
       * the setting is effectively a no-op.
       *
       * macOS: enumerate every adapter via MTLCopyAllDevices(), honour
       * settings->ints.metal_gpu_index if it's in range, and fall back
       * to MTLCreateSystemDefaultDevice() otherwise (matches the
       * behaviour of every other RetroArch driver that exposes this
       * setting — see d3d10.c:2725 for the reference pattern).
       *
       * The list is stored as a plain C struct string_list, so it's
       * safe to cross the ARC/MRC TU boundary; ownership is tracked
       * explicitly and cleared in dealloc. */
      {
         union string_list_elem_attr attr = {0};
         _gpu_list                        = string_list_new();

#if defined(HAVE_COCOATOUCH)
         _device                          = MTLCreateSystemDefaultDevice();

         if (_gpu_list && _device)
         {
            const char *name              = [[_device name] UTF8String];
            string_list_append(_gpu_list, name ? name : "Default", attr);
         }
#else
         {
            settings_t               *settings_ptr = config_get_ptr();
            int                       gpu_index    = settings_ptr
               ? settings_ptr->ints.metal_gpu_index : 0;
            NSArray<id<MTLDevice>> *devices        = RARCH_AUTORELEASE_R(MTLCopyAllDevices());
            NSUInteger                count        = devices ? [devices count] : 0;
            NSUInteger                i;

            if (_gpu_list)
            {
               for (i = 0; i < count; i++)
               {
                  id<MTLDevice> d            = [devices objectAtIndex:i];
                  const char   *name         = [[d name] UTF8String];
                  RARCH_LOG("[Metal] Found GPU #%u: \"%s\".\n",
                        (unsigned)i, name ? name : "Unknown");
                  string_list_append(_gpu_list,
                        name ? name : "Unknown", attr);
               }
            }

            /* The device the index was chosen as, wherever the list
             * now puts it */
            gpu_index = video_driver_gpu_index_resolve(GFX_CTX_METAL_API,
                  gpu_index, _gpu_list);

            if (count > 0 && gpu_index >= 0 && gpu_index < (int)count)
            {
               const char *picked_name;
               _device     = RARCH_RETAIN([devices objectAtIndex:gpu_index]);
               picked_name = [[_device name] UTF8String];
               RARCH_LOG("[Metal] Using GPU #%d: \"%s\".\n",
                     gpu_index, picked_name ? picked_name : "Unknown");
            }
            else
            {
               if (gpu_index != 0)
                  RARCH_WARN("[Metal] metal_gpu_index %d out of range (%u device(s) found), "
                        "falling back to system default device.\n",
                        gpu_index, (unsigned)count);
               _device = MTLCreateSystemDefaultDevice();
            }
         }
#endif

         video_driver_set_gpu_api_devices(GFX_CTX_METAL_API, _gpu_list);
      }

      MetalView *view               = (MetalView *)apple_platform.renderView;
      view.device                   = _device;
      _layer                        = RARCH_RETAIN((CAMetalLayer *)view.layer);

      /* Configure the layer for HDR (or SDR) BEFORE building any pipelines.
       * Pipelines compiled inside _initMetal / Context initWithDevice: bake
       * the layer's pixelFormat into their state, so flipping the format
       * after the fact would leave them invalid.  All downstream pipeline
       * creation sees whatever metal_apply_hdr_layer_config picked.
       *
       * We read video_hdr_mode from settings at init time and treat it as
       * fixed for the lifetime of this driver instance — see comment on
       * metal_apply_hdr_layer_config. */
#if METAL_HDR_AVAILABLE
      {
         settings_t *settings        = config_get_ptr();
         unsigned    initial_hdr_mode = settings
            ? settings->uints.video_hdr_mode
            : METAL_HDR_MODE_OFF;
         /* Clamp the request to the display before it configures the
          * layer.  The capability announce further down correctly
          * advertises no HDR on SDR displays, but two things trust the
          * *setting* rather than the flags: SET_PIXEL_FORMAT's
          * HDR10_2101010 gate (documented there: it runs too early in
          * core init to test the flags, so "non-zero means requested
          * and possible") and this very block, which would otherwise
          * configure an EDR layer the display clamps.  The D3D and
          * Vulkan paths already force the setting to 0 on unsupported
          * displays; Metal was the one HDR driver that did not, so a
          * stale mode on an SDR Mac accepted PQ frames from a core.
          * Mirrors dxgi_check_display_hdr_support. */
         if (     initial_hdr_mode != METAL_HDR_MODE_OFF
               && !metal_display_supports_edr())
         {
            RARCH_WARN("[Metal] HDR requested but the display reports no EDR headroom; forcing HDR off.\n");
            initial_hdr_mode = METAL_HDR_MODE_OFF;
            if (settings)
            {
               settings->flags               |= SETTINGS_FLG_MODIFIED;
               settings->uints.video_hdr_mode = METAL_HDR_MODE_OFF;
            }
         }
         metal_apply_hdr_layer_config(_layer, initial_hdr_mode);
         _initial_hdr_mode           = initial_hdr_mode;
      }
#else
      _initial_hdr_mode              = METAL_HDR_MODE_OFF;
#endif

      if (![self _initMetal])
         RARCH_RETURN_INIT_FAILURE();

      _video                        = *video;
      _viewport                     = (video_viewport_t *)calloc(1, sizeof(video_viewport_t));
      /* NULL-check: Context's setViewport: (see line ~516)
       * unconditionally dereferences the pointer via
       * _viewport = *viewport;  A NULL here would crash on
       * the first _context.viewport = _viewport assignment
       * downstream in setVideoMode / show_mouse / frame.
       * Return nil to fail the init - matches the pattern
       * used by the _initMetal bailout just above.  Returning through
       * RARCH_RETURN_INIT_FAILURE tears down the partial instance
       * (Metal library/queue ref-counts, string_list _gpu_list) via
       * dealloc in both modes. */
      if (!_viewport)
         RARCH_RETURN_INIT_FAILURE();
      _viewportMVP.projectionMatrix = matrix_proj_ortho(0, 1, 0, 1);

      _keepAspect                   = _video.force_aspect;
      /* Resize callbacks require the context and viewport to be ready. */
      view.delegate                 = self;

#if METAL_HDR_AVAILABLE
      unsigned mode_dims =
#endif
      [self applyVideoMode:_video.dims
                fullscreen:_video.fullscreen];

      /* graphics display driver */
      _display       = [[MenuDisplay alloc] initWithContext:_context];
      /* menu view */
      _menu          = [[MetalMenu alloc] initWithContext:_context];

      /* Framebuffer view */
      {
         ViewDescriptor *vd  = RARCH_AUTORELEASE_R([ViewDescriptor new]);
         vd.format           = _video.source_10bit
               ? RPixelFormatBGR10A2Unorm
               : (_video.rgb32 ? RPixelFormatBGRX8Unorm : RPixelFormatB5G6R5Unorm);
         vd.size             = CGSizeMake(VIDEO_SCALE_W(video->dims), VIDEO_SCALE_H(video->dims));
         vd.filter           = _video.smooth ? RTextureFilterLinear : RTextureFilterNearest;
         _frameView          = [[FrameView alloc] initWithDescriptor:vd context:_context];
         _frameView.viewport = _viewport;
         [_frameView setFilteringIndex:0 smooth:video->smooth];
         _frameView.hdrEnabled = (_initial_hdr_mode != METAL_HDR_MODE_OFF);
      }

      /* Overlay view */
      _overlay = [[Overlay alloc] initWithContext:_context];

      _views.device    = _device;
      _views.context   = _context;
      _views.frameView = _frameView;
      _views.nearest   = _samplerStateNearest;
      _views.linear    = _samplerStateLinear;


      /* Tell Context to allocate HDR offscreen + readback textures and
       * compile its composite/tonemap pipelines.  By this point the CAMetalLayer's
       * pixelFormat has already been set (above), _initMetal's downstream
       * pipelines are compiled against that format, and the framebuffer
       * view / overlays are live.  Deferring until after font_driver init
       * keeps all HDR-specific resource creation in one easy-to-find block. */
#if METAL_HDR_AVAILABLE
      /* Announce HDR capability to the wider driver layer.  RetroArch
       * uses these flags to gate the HDR menu options and to clamp the
       * user's selected mode to what the driver can actually do. */
      {
         uint32_t hdr_flags  = 0;
         bool edr_supported  = metal_display_supports_edr();
         if (edr_supported)
         {
            /* Both modes map to the same Metal surface path (swap the
             * CAMetalLayer pixel format + colour space), so whenever
             * EDR is available we advertise both. */
            hdr_flags |= VIDEO_FLAG_HDR10_SUPPORT | VIDEO_FLAG_SCRGB_SUPPORT;
            hdr_flags |= VIDEO_FLAG_HDR_SUPPORT;
         }
         video_driver_modify_disp_flags(hdr_flags,
                 VIDEO_FLAG_HDR_SUPPORT
               | VIDEO_FLAG_HDR10_SUPPORT
               | VIDEO_FLAG_SCRGB_SUPPORT);
         RARCH_LOG("[Metal] HDR capability: display EDR %s, advertising HDR support %s.\n",
               edr_supported ? "detected" : "not detected",
               edr_supported ? "YES (HDR10 + scRGB)" : "NO");
      }
#else
      RARCH_LOG("[Metal] HDR support: compiled out (SDK too old for EDR APIs).\n");
#endif

#if METAL_HDR_AVAILABLE

      if (_initial_hdr_mode != METAL_HDR_MODE_OFF)
      {
         /* Viewport isn't finalised yet (no frame has run).  Start with
          * drawable size as a sensible initial value; the first renderFrame
          * will re-size if the viewport changes. */
         CGSize size = view.drawableSize;
         if (size.width == 0 || size.height == 0)
            size = CGSizeMake(VIDEO_SCALE_W(mode_dims),
                  VIDEO_SCALE_H(mode_dims));
         [_context setHDROutputMode:_initial_hdr_mode
                    viewportWidth:(unsigned)size.width
                   viewportHeight:(unsigned)size.height];
         /* Push initial paper-white, expand-gamut, scanlines, subpixel
          * layout from settings so the first composite frame uses the
          * user-configured values instead of the Context defaults. */
         settings_t *settings = config_get_ptr();
         if (settings)
         {
            [_context setHDRPaperWhiteNits:settings->floats.video_hdr_paper_white_nits];
            [_context setHDRMenuNits:settings->floats.video_hdr_menu_nits];
            [_context setHDRExpandGamut:settings->uints.video_hdr_expand_gamut];
            [_context setHDRScanlines:settings->bools.video_hdr_scanlines];
            [_context setHDRSubpixelLayout:settings->uints.video_hdr_subpixel_layout];
         }
      }
#endif
      /* After HDR is set up: views are not drawn with it. */
      retro_atomic_store_release_int(&_views.allowed,
            metal_views_allowed(&_views) ? 1 : 0);
   }
   return self;
}

- (void)dealloc
{
   metal_views_free_chains(&_views, 0);
   metal_views_release(&_views);

   if (_viewport)
   {
      free(_viewport);
      _viewport = nil;
   }

   /* Tear down the GPU list we published to the frontend.  We clear
    * the slot first so any later code paths (e.g. the menu cbs) that
    * look up the list for GFX_CTX_METAL_API see NULL instead of a
    * pointer to freed memory before the next driver instance comes
    * up and repopulates it. */
   if (_gpu_list)
   {
      video_driver_set_gpu_api_devices(GFX_CTX_METAL_API, NULL);
      string_list_free(_gpu_list);
      _gpu_list = NULL;
   }

#if METAL_HDR_AVAILABLE
   /* Withdraw the HDR-capability flags so a subsequent driver init (e.g.
    * after the user switches to Vulkan and back) doesn't see stale bits. */
   video_driver_modify_disp_flags(0,
           VIDEO_FLAG_HDR_SUPPORT
         | VIDEO_FLAG_HDR10_SUPPORT
         | VIDEO_FLAG_SCRGB_SUPPORT);
#endif

#if !__has_feature(objc_arc)
   [_frameView release];
   [_menu release];
   [_overlay release];
   [_display release];
   [_context release];
   [(id)_t_pipelineState release];
   [(id)_t_pipelineStateNoAlpha release];
   [(id)_t_pipelineStateF16 release];
   [(id)_t_pipelineStateNoAlphaF16 release];
   [(id)_samplerStateLinear release];
   [(id)_samplerStateNearest release];
   [(id)_layer release];
   [(id)_library release];
   [(id)_device release];
   RARCH_SUPER_DEALLOC();
#endif
}

- (bool)_initMetal
{
   _library = [_device newDefaultLibrary];
   if (!_library)
   {
      /* newDefaultLibrary only resolves the metallib from inside a .app
       * bundle's resources.  For a bare executable (CLI builds, non-.app
       * installs, a dev build run in place) it returns nil, so fall back
       * to loading default.metallib explicitly from the executable's own
       * directory (where `make install` places it). */
      NSString *exe = [[NSBundle mainBundle] executablePath];
      if (exe)
      {
         NSString *path = [[exe stringByDeletingLastPathComponent]
               stringByAppendingPathComponent:@"default.metallib"];
         NSError  *lerr = nil;
         _library       = [_device newLibraryWithURL:[NSURL fileURLWithPath:path]
                                               error:&lerr];
         if (_library)
            RARCH_LOG("[Metal] Loaded shader library from \"%s\".\n",
                  path.UTF8String);
         else
            RARCH_ERR("[Metal] Could not load \"%s\": %s.\n", path.UTF8String,
                  lerr ? lerr.localizedDescription.UTF8String : "unknown error");
      }
   }
   _context = [[Context alloc] initWithDevice:_device
                                        layer:_layer
                                      library:_library];

   {
      NSError *err;
      MTLRenderPipelineDescriptor *psd;
      MTLRenderPipelineColorAttachmentDescriptor *ca;
      MTLVertexDescriptor *vd        = RARCH_AUTORELEASE_R([MTLVertexDescriptor new]);
      vd.attributes[0].offset        = 0;
      vd.attributes[0].format        = MTLVertexFormatFloat3;
      vd.attributes[1].offset        = offsetof(Vertex, texCoord);
      vd.attributes[1].format        = MTLVertexFormatFloat2;
      vd.layouts[0].stride           = sizeof(Vertex);

      psd                            = RARCH_AUTORELEASE_R([MTLRenderPipelineDescriptor new]);
      psd.label                      = @"Pipeline+Alpha";

      ca                             = psd.colorAttachments[0];
      /* Always compile against BGRA8Unorm.  In SDR mode that's the drawable
       * format too; in HDR mode these pipelines render into a BGRA8 SDR
       * overlay offscreen that the HDR composite pass alpha-blends on top
       * of the encoded core video.  This way the menu / overlay / OSD
       * render in native SDR colour space regardless of output mode,
       * which avoids the "menu looks wrong on HDR" class of bugs. */
      ca.pixelFormat                 = MTLPixelFormatBGRA8Unorm;
      ca.blendingEnabled             = YES;
      ca.sourceAlphaBlendFactor      = MTLBlendFactorSourceAlpha;
      ca.sourceRGBBlendFactor        = MTLBlendFactorSourceAlpha;
      ca.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
      ca.destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;

      psd.sampleCount                = 1;
      psd.vertexDescriptor           = vd;
      psd.vertexFunction             = RARCH_AUTORELEASE_R([_library newFunctionWithName:@"basic_vertex_proj_tex"]);
      psd.fragmentFunction           = RARCH_AUTORELEASE_R([_library newFunctionWithName:@"basic_fragment_proj_tex"]);

      if (!psd.vertexFunction || !psd.fragmentFunction)
      {
         RARCH_ERR("[Metal] Failed to load main pipeline shader functions.\n");
         return NO;
      }

      _t_pipelineState = [_context sdrState:psd error:&err];
      _t_pipelineStateF16 = [_context twinF16:psd];
      if (err != nil)
      {
         RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
         return NO;
      }

      psd.label               = @"Pipeline+No Alpha";
      ca.blendingEnabled      = NO;
      _t_pipelineStateNoAlpha = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
      _t_pipelineStateNoAlphaF16 = [_context twinF16:psd];
      if (err != nil)
      {
         RARCH_ERR("[Metal] Error creating pipeline state (no alpha) %s.\n", err.localizedDescription.UTF8String);
         return NO;
      }
   }

   {
      MTLSamplerDescriptor *sd = RARCH_AUTORELEASE_R([MTLSamplerDescriptor new]);
      _samplerStateNearest     = [_device newSamplerStateWithDescriptor:sd];

      sd.minFilter             = MTLSamplerMinMagFilterLinear;
      sd.magFilter             = MTLSamplerMinMagFilterLinear;
      _samplerStateLinear      = [_device newSamplerStateWithDescriptor:sd];
   }

   return YES;
}

- (void)setViewportDims:(unsigned)dims forceFull:(BOOL)forceFull allowRotate:(BOOL)allowRotate
{
   _viewport->full_dims    = dims;
   video_driver_set_output_dims(_viewport->full_dims);
   _layer.drawableSize     = CGSizeMake(VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));
   video_driver_update_viewport(_viewport, forceFull, _keepAspect, YES);
   _context.viewport       = _viewport; /* Update matrix */
   _viewportMVP.outputSize = simd_make_float2(VIDEO_SCALE_W(_viewport->full_dims), VIDEO_SCALE_H(_viewport->full_dims));

#if METAL_HDR_AVAILABLE
   /* Keep the HDR offscreens matched to the drawable size on resize.
    * Cheap no-op when already the right size. */
   if (_context.hdrEnabled)
      [_context resizeHDRResourcesForWidth:VIDEO_SCALE_W(dims)
                                     height:VIDEO_SCALE_H(dims)];
#endif
}

#pragma mark - video

- (void)setVideo:(const video_info_t *)video { }

- (metal_views_t *)views { return &_views; }

/* Moves UI drawing into the UI layer, laid out at one eye's size */
- (void)_viewsUIBegin:(video_frame_info_t *)video_info
{
   id<MTLRenderCommandEncoder> rce;
   [_context beginLayer:_views.ui.tex];
   /* Menus and widgets size from video_info->dims, fonts and the
    * display from the viewport's full_dims. */
   video_views_ui_enter(&_views.saved, &video_info->dims,
         &_viewport->full_dims, video_info->views_layout.ui_dims);
   _context.viewport = _viewport;
   if ((rce = _context.rce))
      [rce setVertexBytes:_context.uniforms length:sizeof(*_context.uniforms)
                  atIndex:BufferIndexUniforms];
}

/* Back onto the drawable, and the UI layer into each eye's area. It was
 * drawn over transparent black, so its colour is premultiplied. */
- (void)_viewsUIEnd:(video_frame_info_t *)video_info
{
   unsigned a;
   id<MTLRenderCommandEncoder> rce;
   const video_views_layout_t *layout = &video_info->views_layout;

   [_context endLayer];
   video_views_ui_leave(&_views.saved, &video_info->dims,
         &_viewport->full_dims);
   _context.viewport = _viewport;
   if (!(rce = _context.rce))
      return;
   [rce setRenderPipelineState:_views.layer];
   [rce setFragmentTexture:_views.ui.tex atIndex:0];
   /* Only full side by side places it unscaled. */
   [rce setFragmentSamplerState:
      (layout->stereo_mode == VIDEO_STEREO_MODE_SBS_FULL)
      ? _samplerStateNearest : _samplerStateLinear atIndex:0];
   metal_views_ubo(rce, &_context.uniformsNoRotate->projectionMatrix,
         simd_make_float4(0.0f, 0.0f, 1.0f, 1.0f),
         simd_make_float4(0.0f, 0.0f, 1.0f, 1.0f));
   for (a = 0; a < layout->num_areas; a++)
   {
      [rce setViewport:metal_views_vp(&layout->areas[a])];
      [rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0
                vertexCount:4];
   }
}

- (bool)renderFrame:(const void *)frame
               data:(void*)data
              width:(unsigned)width
             height:(unsigned)height
         frameCount:(uint64_t)frameCount
              pitch:(unsigned)pitch
                msg:(const char *)msg
               info:(video_frame_info_t *)video_info
{
   /* Travels with the frame, for set_texture_frame() to read rather
    * than the setting the menu writes */
   self.frameMenuLinearFilter = video_info->menu_linear_filter;

   @autoreleasepool
   {
      bool statistics_show = video_info->statistics_show;
      bool menu_is_alive   = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
      id<MTLRenderCommandEncoder> rce;
      bool hdrOn           = _context.hdrEnabled;
      metal_view_chain_t *view_chains[RETRO_VIDEO_VIEWS_MAX];
      video_views_rect_t view_rects[RETRO_VIDEO_VIEWS_MAX];
      bool views;
      bool views_blend     = false;
      bool views_direct    = false;
      bool views_per_eye;
      bool views_ui;
      bool statistics;

      /* Begin frame: re-evaluate viewport, push to context if it changed,
       * then begin the command buffer / render pass. */
      {
         video_viewport_t vp = *_viewport;
         video_driver_update_viewport(_viewport, _views.active,
               _keepAspect, YES);
         if (memcmp(&vp, _viewport, sizeof(vp)) != 0)
            _context.viewport = _viewport;
         [_context begin];
      }

      _frameView.frameCount      = frameCount;
      /* The sub-frame index is 0 on the core frame and j on the j-th
       * re-render below; the shader sees it 1-based, and each
       * sub-frame is one more present on the display */
      _frameView.swapCount       = video_info->swap_count
                                 + video_info->current_subframe;
      _frameView.totalSubframes  = video_info->shader_subframes > 1
                                 ? video_info->shader_subframes : 1;
      _frameView.currentSubframe = video_info->current_subframe + 1;
      _frameView.hdrPaperWhiteNits = video_info->hdr_paper_white_nits;
      _frameView.hdrPeakNits = video_info->hdr_max_nits;
      _frameView.hdrExpandGamut = video_info->hdr_expand_gamut;
      if (frame && frame != RETRO_HW_FRAME_BUFFER_VALID && width && height)
      {
         struct video_shader *shader = _frameView.shader;
         _frameView.size      = CGSizeMake(width, height);
         /* Views through the preset have each view written straight
          * in: the frame waits until they are known */
         if (!(   video_info->views.num_views && video_info->shader_active
               && shader && shader->passes
               && [_frameView deferFrame:frame pitch:pitch]))
            [_frameView updateFrame:frame pitch:pitch];
         _frameEverUploaded   = true;
      }
      else if (_frameView.movieFrame)
         _frameEverUploaded = true;
      else if (!_frameEverUploaded)
      {
         /* Driver just came up (init or post-reinit).  The frontend
          * hasn't delivered a real frame yet and — on CMD_EVENT_REINIT
          * from inside the menu — likely won't until the user leaves
          * the menu.  Pull the cached frame so the core image reappears
          * under the menu immediately instead of waiting for an F1
          * cycle.  Safe to do every frame while the flag is clear; we
          * latch it after the first successful upload.
          *
          * Routes through video_driver_cached_frame_read so the read
          * is lifetime-safe: the callback runs inside the cached
          * frame's lock, so a concurrent core-close / driver-reinit
          * can't free the source buffer while we're uploading.  HW-
          * render frames are skipped automatically -- the API hands
          * the callback a NULL data pointer for those, which the
          * callback's guard short-circuits. */
         struct metal_pull_cached_ctx ctx;
         ctx.view          = _frameView;
         ctx.uploaded_flag = &_frameEverUploaded;
         video_driver_cached_frame_read(&ctx, metal_pull_cached_frame_cb);
      }

      {
         CGSize size = _frameView.size;
         views = metal_views_setup(&_views, video_info, frame,
               VIDEO_SCALE_PACK((unsigned)size.width, (unsigned)size.height),
               &views_blend, &views_direct, view_chains, view_rects);
      }
      /* Drawn any other way, the frame is written now */
      if (!views || views_direct)
         [_frameView commitFrame];
      /* The viewport is the whole window while views draw. */
      if (views != _views.active)
      {
         _views.active = views;
         video_driver_update_viewport(_viewport, views, _keepAspect, YES);
         _context.viewport = _viewport;
         [_frameView syncViewport];
      }
      retro_atomic_store_release_int(&_views.fallback,
            (video_info->views.num_views && !views) ? 1 : 0);
      retro_atomic_store_release_int(&_views.allowed,
            metal_views_allowed(&_views) ? 1 : 0);

      /* Acquire the frame encoder.  In SDR mode this lazily opens a pass
       * on the drawable (BGRA8).  In HDR mode it lazily opens a pass on
       * the BGRA8 SDR overlay offscreen — menu / overlay / OSD / widgets
       * all render there, and the HDR composite at end-of-frame reads the
       * overlay texture, alpha-blends it over the encoded core, and
       * writes the result into the HDR drawable. */
      rce = _context.rce;

      /* Draw core: back buffer + optional encoder pass. */
      if (views)
      {
         [_frameView prepareFrame];
         metal_views_render(&_views, video_info, view_chains, view_rects,
               views_blend, views_direct, frame && width && height,
               _frameView.filter != RTextureFilterNearest);
         [_frameView dropFrame];
      }
      else
         [_frameView drawWithContext:_context];

      /* Bind uniforms once for all subsequent encoder work on the main
       * command encoder. FrameView's shader passes can rebind vertex
       * slots at shader-reflection-driven indices on the back-buffer
       * pass, so bind AFTER drawWithContext: returns, not before.
       * All subsequent blocks (core encoder pass, menu, overlay) reuse
       * this binding instead of each rebinding the same uniforms. */
      if (rce)
         [rce setVertexBytes:_context.uniforms length:sizeof(*_context.uniforms) atIndex:BufferIndexUniforms];

      /* SDR no-shader blit into drawable.  In HDR mode the composite
       * pass at end-of-frame reads the core frame texture directly, so
       * we skip the blit here; the core never writes into the SDR
       * overlay. */
      if (!hdrOn && !views && (_frameView.drawState & ViewDrawStateEncoder) != 0)
      {
         [rce setRenderPipelineState:(_context.hdrEnabled
               && _t_pipelineStateNoAlphaF16)
            ? _t_pipelineStateNoAlphaF16 : _t_pipelineStateNoAlpha];
         if (_frameView.filter == RTextureFilterNearest)
            [rce setFragmentSamplerState:_samplerStateNearest atIndex:SamplerIndexDraw];
         else
            [rce setFragmentSamplerState:_samplerStateLinear atIndex:SamplerIndexDraw];
         [_frameView drawWithEncoder:rce];
      }

      statistics = statistics_show && frame && width && height
         && !menu_is_alive;

      /* As the main thread laid the UI out, also in a frame drawn whole:
       * it lays out later frames whole once it reads the fallback. */
      views_per_eye = video_views_ui_per_eye(&video_info->views,
            &video_info->views_layout, metal_views_allowed(&_views));
      /* A real frame without it frees the UI layer. */
      if (frame && !views_per_eye)
         metal_vv_target_drop(&_views, &_views.ui_target, &_views.ui);
      /* Drawn and composited only when there is UI; overlays are hidden. */
      views_ui = VIDEO_VIEWS_UI_WANTED(views_per_eye, _menu.enabled, msg,
            statistics,
            video_info->widgets_visible)
         && metal_vv_target_ensure(&_views, &_views.ui_target, &_views.ui,
               video_info->views_layout.ui_dims);
      if (views_ui)
      {
         [self _viewsUIBegin:video_info];
         rce = _context.rce;
      }

      /* Draw menu: textured menu frame if present, otherwise delegate to
       * the menu driver for widget-based menus. */
      if (_menu.enabled)
      {
         if (_menu.hasFrame)
         {
            [_menu.view drawWithContext:_context];
            [rce setRenderPipelineState:(_context.hdrEnabled
                  && _t_pipelineStateF16)
               ? _t_pipelineStateF16 : _t_pipelineState];
            if (_menu.view.filter == RTextureFilterNearest)
               [rce setFragmentSamplerState:_samplerStateNearest atIndex:SamplerIndexDraw];
            else
               [rce setFragmentSamplerState:_samplerStateLinear atIndex:SamplerIndexDraw];
            if (_views.active || views_ui)
            {
               /* The viewport is the whole window while views draw, so
                * fit the menu to this pass's target here. */
               video_viewport_t vp;
               MTLViewport      menu_vp;
               vp.full_dims     = _viewport->full_dims;
               video_driver_update_viewport(&vp, false, _keepAspect, false);
               menu_vp.originX  = (double)VIDEO_POS_X(vp.pos);
               menu_vp.originY  = (double)VIDEO_POS_Y(vp.pos);
               menu_vp.width    = (double)VIDEO_SCALE_W(vp.dims);
               menu_vp.height   = (double)VIDEO_SCALE_H(vp.dims);
               menu_vp.znear    = 0.0;
               menu_vp.zfar     = 1.0;
               [rce setViewport:menu_vp];
            }
            [_menu.view drawWithEncoder:rce];
         }
#if defined(HAVE_MENU)
         else
         {
            [_context resetRenderViewport:kFullscreenViewport];
            menu_driver_frame(menu_is_alive, video_info);
         }
#endif
      }

#ifdef HAVE_OVERLAY
      /* Touch overlays would straddle both eyes. */
      if (_overlay.enabled && !views_per_eye)
      {
         [_context resetRenderViewport:_overlay.fullscreen ? kFullscreenViewport : kVideoViewport];
         [rce setRenderPipelineState:[_context getStockShader:VIDEO_SHADER_STOCK_BLEND blend:YES]];
         [rce setFragmentSamplerState:_samplerStateLinear atIndex:SamplerIndexDraw];
         [_overlay drawWithEncoder:rce];
      }
#endif

      /* Only show statistics when menu is not visible and content is running */
      if (statistics)
      {
         struct font_params *osd_params = (struct font_params *)&video_info->osd_stat_params;
         if (osd_params)
            font_driver_render_msg(data, video_info->stat_text, video_info->stat_text_len, osd_params, NULL);
      }

#ifdef HAVE_GFX_WIDGETS
      if (video_info->widgets_active)
         gfx_widgets_frame(video_info);
#endif

      /* Render on-screen message */
      if (msg && *msg)
         font_driver_render_msg(data, msg, strlen(msg), NULL, NULL);

      if (views_ui)
         [self _viewsUIEnd:video_info];

      /* End-of-frame HDR composite.  Menu / overlay / OSD / widgets have
       * rendered into the BGRA8 SDR overlay offscreen (_sdrOverlayTex);
       * the core source is either shaderOutputTexture (shader chain
       * active) or frameTexture (no-shader path), or nil (no core frame
       * yet).  Composite touches the drawable unconditionally to avoid
       * presenting uninitialised swapchain memory — when src is nil, a
       * clear-only pass runs in place of the core encode. */
#if METAL_HDR_AVAILABLE
      if (hdrOn)
      {
         const HDRUniforms *u  = _context.currentHDRUniforms;
         /* Both sources hold unrotated content, so the composite rotates
          * the sampling.  Under HDR the last slang pass always owns an RT
          * (it is the composite's input), and a pass that owns an RT
          * renders with the unrotated mvp_last_pass. */
         unsigned          rot = retroarch_get_rotation() & 3;
         id<MTLTexture>    src = _frameView.shaderOutputTexture;
         if (!src)
            src = _frameView.frameTexture;
         [_context hdrComposite:u fromSource:src rotation:rot
               movieLinear:(_frameView.movieHDR && !_frameView.shaderOutputTexture)];
      }
#endif

      [self _endFrame:video_info];
   }

   return YES;
}

- (void)_endFrame:(video_frame_info_t *)video_info { [_context endWithRecording:video_info]; }
/* TODO/FIXME (sgc): resize*/
- (void)setNeedsResize { }
- (void)setRotation:(unsigned)rotation { [_context setRotation:rotation]; }
- (Uniforms *)viewportMVP { return &_viewportMVP; }

#pragma mark - MTKViewDelegate

- (unsigned)applyVideoMode:(unsigned)dims fullscreen:(bool)fullscreen
{
   gfx_ctx_mode_t mode;
   MetalView *view = (MetalView *)apple_platform.renderView;

   /* A zero axis asks for full screen, which the view already covers */
   if (!VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims))
   {
      CGSize size  = view.frame.size;
      dims         = VIDEO_SCALE_PACK(size.width, size.height);
   }

   mode.dims       = dims;
   mode.fullscreen = fullscreen;
   [apple_platform setVideoMode:mode];

#ifdef HAVE_COCOATOUCH
   [self mtkView:view drawableSizeWillChange:CGSizeMake(
         VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims))];
#else
   /* Restored windows need not resize when the video mode is applied. */
   [self mtkView:view drawableSizeWillChange:view.drawableSize];
#endif

   return dims;
}

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size
{
#ifdef HAVE_COCOATOUCH
    CGFloat scale = [[UIScreen mainScreen] scale];
    [self setViewportDims:VIDEO_SCALE_PACK(
          (unsigned)((unsigned int)view.bounds.size.width  * scale),
          (unsigned)((unsigned int)view.bounds.size.height * scale))
                forceFull:NO allowRotate:YES];
#else
   [self setViewportDims:VIDEO_SCALE_PACK((unsigned int)size.width,
         (unsigned int)size.height) forceFull:NO allowRotate:YES];
#endif
}

- (void)drawInMTKView:(MTKView *)view { }
@end

@implementation MetalMenu
{
   Context *_context;
   TexturedView *_view;
   bool _enabled;
}

- (instancetype)initWithContext:(Context *)context
{
   if (self = [super init])
      _context = RARCH_RETAIN(context);
   return self;
}

#if !__has_feature(objc_arc)
- (void)dealloc
{
   [_context release];
   [_view release];
   [super dealloc];
}
#endif

- (bool)hasFrame { return _view != nil; }

- (void)setEnabled:(bool)enabled
{
   if (_enabled == enabled)
      return;
   _enabled      = enabled;
   _view.visible = enabled;
}

- (bool)enabled { return _enabled; }

- (void)updateDims:(unsigned)dims
            format:(RPixelFormat)format
            filter:(RTextureFilter)filter
{
   CGSize size = CGSizeMake(VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));

   if (_view)
   {
      if (!(CGSizeEqualToSize(_view.size, size) &&
            _view.format == format &&
            _view.filter == filter))
         RARCH_RELEASE_NIL(_view);
   }

   if (!_view)
   {
      ViewDescriptor *vd = RARCH_AUTORELEASE_R([ViewDescriptor new]);
      vd.format          = format;
      vd.filter          = filter;
      vd.size            = size;
      _view              = [[TexturedView alloc] initWithDescriptor:vd context:_context];
      _view.visible      = _enabled;
   }
}

- (void)updateFrame:(void const *)source
{
   [_view updateFrame:source pitch:RPixelFormatToBPP(_view.format) * (NSUInteger)_view.size.width];
}

@end

#pragma mark - FrameView

#define MTLALIGN(x) __attribute__((aligned(x)))

typedef struct
{
   float x;
   float y;
   float z;
   float w;
} float4_t;

typedef struct texture
{
   __unsafe_unretained id<MTLTexture> view;
   float4_t size_data;
} texture_t;

typedef struct MTLALIGN(16)
{
   matrix_float4x4 mvp;
   matrix_float4x4 mvp_last_pass;

   struct
   {
      texture_t texture[GFX_MAX_FRAME_HISTORY + 1];
      MTLViewport viewport;
      float4_t output_size;
      bool valid;
   } frame;

   struct
   {
      __unsafe_unretained id<MTLBuffer> buffers[SLANG_CBUFFER_MAX];
      texture_t rt;
      texture_t feedback;
      uint32_t frame_count;
      int32_t frame_direction;
      int32_t frame_time_delta;
      float original_fps;
      uint32_t rotation;
      float_t core_aspect;
      float_t core_aspect_rot;
      uint32_t total_subframes;
      uint32_t current_subframe;
      uint32_t swap_count;
      uint32_t hdr_mode;
      uint32_t passthrough;
      float inverse_tonemap;
      float hdr10;
      float scanlines;
      unsigned subpixel_layout;
      float paper_white_nits;
      float peak_nits;
      float enable_hdr;
      unsigned expand_gamut;
      pass_semantics_t semantics;
      MTLViewport viewport;
      __unsafe_unretained id<MTLRenderPipelineState> _state;
   } pass[GFX_MAX_SHADERS];

   texture_t luts[GFX_MAX_TEXTURES];

} engine_t;

/* Capture constants before CPU staging is reused by another draw. */
static bool metal_bind_slang_constants(
      id<MTLRenderCommandEncoder> rce, id<MTLDevice> device,
      const cbuffer_sem_t *buffer_sem, id<MTLBuffer> buffer)
{
   const void *data;
   id<MTLBuffer> snapshot;
   NSUInteger size = buffer_sem->size;

   if (!buffer_sem->stage_mask || !size)
      return true;
   if (!rce || !buffer || buffer.length < size || !(data = buffer.contents))
      return false;
   if (size < 4096)
   {
      if (buffer_sem->stage_mask & SLANG_STAGE_VERTEX_MASK)
         [rce setVertexBytes:data length:size atIndex:buffer_sem->binding];
      if (buffer_sem->stage_mask & SLANG_STAGE_FRAGMENT_MASK)
         [rce setFragmentBytes:data length:size atIndex:buffer_sem->binding];
      return true;
   }

   snapshot = RARCH_AUTORELEASE_R([device newBufferWithBytes:data
         length:size options:PLATFORM_METAL_RESOURCE_STORAGE_MODE]);
   if (!snapshot)
      return false;
#if !defined(HAVE_COCOATOUCH)
   [snapshot didModifyRange:NSMakeRange(0, size)];
#endif
   if (buffer_sem->stage_mask & SLANG_STAGE_VERTEX_MASK)
      [rce setVertexBuffer:snapshot offset:0 atIndex:buffer_sem->binding];
   if (buffer_sem->stage_mask & SLANG_STAGE_FRAGMENT_MASK)
      [rce setFragmentBuffer:snapshot offset:0 atIndex:buffer_sem->binding];
   return true;
}

struct metal_view_chain
{
   engine_t engine;
   /* A 16-bit view as written, converted from into history[0] */
   texture_t src16;
   unsigned rt_frame_dims;
   unsigned rt_vp_dims;
   bool history_made;
};

@implementation FrameView
{
   Context *_context;
   id<MTLTexture> _texture; /* final render texture */
   Vertex _v[4];
   VertexSlang _vertex[4];
   CGSize _size; /* size of view in pixels */
   CGRect _frame;
   NSUInteger _bpp;

   id<MTLTexture> _src; /* source texture */
   bool _srcDirty;
   /* A frame put off by deferFrame:pitch:, and where it lies in frame
    * memory (_pendingMem, -1 for none, at _pendingOff) */
   void const *_pendingSrc;
   NSUInteger  _pendingPitch;
   int         _pendingMem;
   NSUInteger  _pendingOff;
   bool        _writtenSeen;

   /* Frame memory: shared-storage buffers a frame pushed from is
    * blitted out of, with no host copy - the threaded wrapper's frame
    * buffers (frame_mem_alloc) and the framebuffers lent to the core.
    * _memReader is the command buffer of the last blit out of each: the
    * buffer is written again once it has completed, never waited for. */
#define METAL_FRAME_MEM_MAX 12
   id<MTLBuffer>        _mem[METAL_FRAME_MEM_MAX];
   id<MTLCommandBuffer> _memReader[METAL_FRAME_MEM_MAX];
   /* The framebuffers lent to the core (GET_CURRENT_SOFTWARE_FRAMEBUFFER):
    * three of the frame memory's, by index plus one, at the frame's size
    * and tight pitch. _loanLast is the index the last frame was pushed
    * from, which the cached frame names; -1 for none. */
#define METAL_LOAN_SLOTS 3
   int                  _loanMem[METAL_LOAN_SLOTS];
   int                  _loanLast;
   unsigned             _loanNext;
   NSUInteger           _loanStride;
   NSUInteger           _loanWidth;
   NSUInteger           _loanHeight;

   id<MTLSamplerState> _samplers[RARCH_FILTER_MAX][RARCH_WRAP_MAX][2];
   struct video_shader *_shader;

   engine_t _engine;
   texture_t _moviePrevious[GFX_MAX_FRAME_HISTORY + 1];

   bool resize_render_targets;
   bool init_history;
   BOOL _movieFrame;
   BOOL _movieHDR;
   video_viewport_t *_viewport;

   /* HDR state.
    *
    * _hdrEnabled is the driver-level HDR on/off.  When true, the last
    * shader pass's RT is allocated as an HDR-capable texture (RGBA16F or
    * RGB10A2 depending on shader-emitted format).  The driver samples
    * that RT through shaderOutputTexture and hands it to
    * Context hdrComposite:fromSource: which encodes PQ / scales scRGB
    * into the drawable.
    *
    * _shaderEmitsHDR{10,16} are set by setShaderFromPath after slang
    * parsing determines the last pass's output format.  They feed back
    * into Context to suppress the composite's inverse-tonemap / PQ encode
    * when the shader preset has already emitted HDR content. */
   BOOL _hdrEnabled;
   BOOL _shaderEmitsHDR10;
   BOOL _shaderEmitsHDR16;
}

- (BOOL)hdrEnabled { return _hdrEnabled; }
- (BOOL)movieFrame { return _movieFrame; }
- (BOOL)movieHDR { return _movieFrame && _movieHDR; }
- (id<MTLTexture>)frameTexture { return _engine.frame.texture[0].view; }

- (id<MTLTexture>)shaderOutputTexture
{
   if (!_shader || _shader->passes == 0)
      return nil;
   return _engine.pass[_shader->passes - 1].rt.view;
}

- (void)setHdrEnabled:(BOOL)enabled
{
   if (_hdrEnabled == enabled)
      return;
   _hdrEnabled            = enabled;
   /* Pipelines for the final shader pass are compiled against a specific
    * color-attachment pixel format.  A flip between HDR and SDR changes
    * that format (BGRA8 vs RGBA16Float) so the pipelines need rebuilding.
    * Forcing resize_render_targets only re-allocates RTs; we also need
    * a full shader re-process.  Simplest correct thing: signal that we
    * need RT rebuild and let the driver re-run setShaderFromPath on the
    * next frame if necessary.  A shader re-process on toggle is a known
    * cost — Vulkan rebuilds its swapchain too. */
   resize_render_targets  = YES;
}

- (instancetype)initWithDescriptor:(ViewDescriptor *)d context:(Context *)c
{
   self = [super init];
   if (self)
   {
      _context              = RARCH_RETAIN(c);
      _format               = d.format;
      _bpp                  = RPixelFormatToBPP(_format);
      _filter               = d.filter;
      if (   _format == RPixelFormatBGRA8Unorm
          || _format == RPixelFormatBGRX8Unorm
          || _format == RPixelFormatBGR10A2Unorm)
         _drawState         = ViewDrawStateEncoder;
      else
         _drawState         = ViewDrawStateAll;
      _visible              = YES;
      _engine.mvp           = matrix_proj_ortho(0, 1, 0, 1);
      [self _initSamplers];

      self.size             = d.size;
      self.frame            = CGRectMake(0, 0, 1, 1);
      resize_render_targets = YES;
      _loanLast             = -1;

      /* Initialize slang vertex buffer */
      VertexSlang v[4]      = {
         {simd_make_float4(0, 1, 0, 1), simd_make_float2(0, 1)},
         {simd_make_float4(1, 1, 0, 1), simd_make_float2(1, 1)},
         {simd_make_float4(0, 0, 0, 1), simd_make_float2(0, 0)},
         {simd_make_float4(1, 0, 0, 1), simd_make_float2(1, 0)},
      };
      memcpy(_vertex, v, sizeof(_vertex));
   }
   return self;
}

- (void)dealloc
{
   int i;

   /* The cached frame may point into frame memory, which goes with
    * this view: retire it before the buffers do. (The GPU's own reads
    * hold a buffer through their command buffer.) */
   video_driver_cached_frame_retire();

   /* The engine's unretained slots each own one reference placed there
    * by RARCH_STRUCT_ASSIGN; they must be dropped explicitly in both modes.
    * _freeVideoShader clears the per-pass slots and the LUTs and frees
    * the C shader struct; the frame-history textures live outside the
    * shader and are cleared here. */
   [self _freeVideoShader:_shader];
   _shader = NULL;
   for (i = 0; i < GFX_MAX_FRAME_HISTORY + 1; i++)
   {
      RARCH_STRUCT_ASSIGN(_engine.frame.texture[i].view, nil);
      RARCH_STRUCT_ASSIGN(_moviePrevious[i].view, nil);
   }

#if !__has_feature(objc_arc)
   {
      int w, m;
      for (w = 0; w < RARCH_WRAP_MAX; w++)
      {
         for (m = 0; m < 2; m++)
         {
            /* The RARCH_FILTER_UNSPEC row aliases one of the two rows
             * below (see setFilteringIndex:smooth:) and owns nothing. */
            [(id)_samplers[RARCH_FILTER_LINEAR][w][m] release];
            [(id)_samplers[RARCH_FILTER_NEAREST][w][m] release];
         }
      }
   }
   [_context release];
   [(id)_texture release];
   [(id)_src release];
   for (i = 0; i < METAL_FRAME_MEM_MAX; i++)
   {
      [(id)_mem[i] release];
      [(id)_memReader[i] release];
   }
   RARCH_SUPER_DEALLOC();
#endif
}

- (void)_initSamplers
{
   int i;
   MTLSamplerDescriptor *sd = RARCH_AUTORELEASE_R([MTLSamplerDescriptor new]);

   /* Initialize samplers */
   for (i = 0; i < RARCH_WRAP_MAX; i++)
   {
      switch (i)
      {
         case RARCH_WRAP_BORDER:
#if defined(HAVE_COCOATOUCH)
            sd.sAddressMode = MTLSamplerAddressModeClampToZero;
#else
            sd.sAddressMode = MTLSamplerAddressModeClampToBorderColor;
#endif
            break;

         case RARCH_WRAP_EDGE:
            sd.sAddressMode = MTLSamplerAddressModeClampToEdge;
            break;

         case RARCH_WRAP_REPEAT:
            sd.sAddressMode = MTLSamplerAddressModeRepeat;
            break;

         case RARCH_WRAP_MIRRORED_REPEAT:
            sd.sAddressMode = MTLSamplerAddressModeMirrorRepeat;
            break;

         default:
            continue;
      }
      sd.tAddressMode        = sd.sAddressMode;
      sd.rAddressMode        = sd.sAddressMode;
      sd.minFilter           = MTLSamplerMinMagFilterLinear;
      sd.magFilter           = MTLSamplerMinMagFilterLinear;
      sd.mipFilter           = MTLSamplerMipFilterNotMipmapped;

      id<MTLSamplerState> ss = [_context.device newSamplerStateWithDescriptor:sd];
      _samplers[RARCH_FILTER_LINEAR][i][0] = ss;
      
      sd.mipFilter           = MTLSamplerMipFilterLinear;
      ss = [_context.device newSamplerStateWithDescriptor:sd];
      _samplers[RARCH_FILTER_LINEAR][i][1] = ss;

      sd.minFilter           = MTLSamplerMinMagFilterNearest;
      sd.magFilter           = MTLSamplerMinMagFilterNearest;
      sd.mipFilter           = MTLSamplerMipFilterNotMipmapped;

      ss                     = [_context.device newSamplerStateWithDescriptor:sd];
      _samplers[RARCH_FILTER_NEAREST][i][0] = ss;
      
      sd.mipFilter           = MTLSamplerMipFilterNearest;
      ss = [_context.device newSamplerStateWithDescriptor:sd];
      _samplers[RARCH_FILTER_NEAREST][i][1] = ss;
   }
}

- (void)setFilteringIndex:(int)index smooth:(bool)smooth
{
   int i;
   for (i = 0; i < RARCH_WRAP_MAX; i++)
   {
      if (smooth)
      {
         _samplers[RARCH_FILTER_UNSPEC][i][0] = _samplers[RARCH_FILTER_LINEAR][i][0];
         _samplers[RARCH_FILTER_UNSPEC][i][1] = _samplers[RARCH_FILTER_LINEAR][i][1];
      }
      else
      {
         _samplers[RARCH_FILTER_UNSPEC][i][0] = _samplers[RARCH_FILTER_NEAREST][i][0];
         _samplers[RARCH_FILTER_UNSPEC][i][1] = _samplers[RARCH_FILTER_NEAREST][i][1];
      }
   }
}

- (void)setSize:(CGSize)size
{
   if (CGSizeEqualToSize(_size, size))
      return;

   _size                 = size;
   resize_render_targets = YES;

   if (   _format != RPixelFormatBGRA8Unorm
       && _format != RPixelFormatBGRX8Unorm
       && _format != RPixelFormatBGR10A2Unorm)
   {
      MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR16Uint
                                 width:(NSUInteger)size.width
                                 height:(NSUInteger)size.height
                                 mipmapped:NO];
      RARCH_ASSIGN(_src, RARCH_AUTORELEASE_R([_context.device newTextureWithDescriptor:td]));
   }
}

- (CGSize)size { return _size; }

- (void)setSourceFormat:(RPixelFormat)format
{
   CGSize size = _size;
   if (format == _format)
      return;
   _format   = format;
   _bpp      = RPixelFormatToBPP(_format);
   _srcDirty = NO;
   RARCH_RELEASE_NIL(_src);
   /* setSize: makes the staging texture a 16-bit format needs; a
    * view that has had no frame yet gets it with its first */
   _size     = CGSizeMake(0, 0);
   [self setSize:size];
}

- (void)setFrame:(CGRect)frame
{
   if (CGRectEqualToRect(_frame, frame))
      return;

   /* update vertices */
   CGPoint o   = frame.origin;
   CGSize  s   = frame.size;

   CGFloat l   = o.x;
   CGFloat t   = o.y;
   CGFloat r   = o.x + s.width;
   CGFloat b   = o.y + s.height;

   Vertex v[4] = {
      {simd_make_float3(l, b, 0), simd_make_float2(0, 1)},
      {simd_make_float3(r, b, 0), simd_make_float2(1, 1)},
      {simd_make_float3(l, t, 0), simd_make_float2(0, 0)},
      {simd_make_float3(r, t, 0), simd_make_float2(1, 0)},
   };

   _frame      = frame;
   memcpy(_v, v, sizeof(_v));
}

- (CGRect)frame { return _frame; }

/* METAL_FRAME_HISTORY_BEGIN */
- (void)_updateHistory
{
   if (_shader)
   {
      if (_shader->history_size)
      {
         if (init_history)
         {
            int i;
            MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
                     _movieFrame ? (_movieHDR ? MTLPixelFormatRGBA16Float : MTLPixelFormatRGBA8Unorm) : MTLPixelFormatBGRA8Unorm
                     width:(NSUInteger)_size.width
                     height:(NSUInteger)_size.height
                     mipmapped:false];
            td.usage = MTLTextureUsageShaderRead
                     | MTLTextureUsageShaderWrite
                     | MTLTextureUsageRenderTarget;

            for (i = 0; i < _shader->history_size + 1; i++)
               [self _initTexture:&_engine.frame.texture[i] withDescriptor:td];
            init_history = NO;
         }
         else
         {
            int k;
            /* TODO/FIXME: what about frame-duping ?
             * maybe clone d3d10_texture_t with AddRef */
            texture_t tmp = _engine.frame.texture[_shader->history_size];
            for (k = _shader->history_size; k > 0; k--)
               _engine.frame.texture[k] = _engine.frame.texture[k - 1];
            _engine.frame.texture[0] = tmp;
         }
      }
   }

   [self _ensureFront];
}
/* METAL_FRAME_HISTORY_END */

/* The front slot at the frame's size: no history, or one of another
 * size moved there */
/* METAL_FRAME_FRONT_BEGIN */
- (void)_ensureFront
{
   MTLPixelFormat wanted = _movieFrame ? (_movieHDR ? MTLPixelFormatRGBA16Float : MTLPixelFormatRGBA8Unorm)
      : _format == RPixelFormatBGR10A2Unorm ? MTLPixelFormatBGR10A2Unorm : MTLPixelFormatBGRA8Unorm;
   if (   _engine.frame.texture[0].size_data.x != _size.width
       || _engine.frame.texture[0].size_data.y != _size.height
       || _engine.frame.texture[0].view.pixelFormat != wanted)
   {
      /* The front slot receives the uploaded source frame directly, so for a
       * native 10-bit source it must be BGR10A2 (which matches the ABI's
       * XRGB2101010 packing with no swizzle); all other formats upload into
       * or convert to BGRA8. */
      MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:wanted
               width:(NSUInteger)_size.width
               height:(NSUInteger)_size.height
               mipmapped:false];
      td.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
      [self _initTexture:&_engine.frame.texture[0] withDescriptor:td];
   }
}
/* METAL_FRAME_FRONT_END */
/* METAL_MOVIE_CLEAR_BEGIN */
- (void)_clearMovieFrame
{
   if (_movieHDR) init_history = YES;
   _movieFrame = NO;
   _movieHDR = NO;
}
/* METAL_MOVIE_CLEAR_END */


- (bool)readViewport:(uint8_t *)buffer isIdle:(bool)isIdle
{
   bool res;
   bool enabled = _context.captureEnabled;
   if (!enabled)
      _context.captureEnabled = YES;

   video_driver_cached_frame();

   res = [_context readBackBuffer:buffer];

   if (!enabled)
      _context.captureEnabled = NO;

   return res;
}

- (bool)readViewportHDR:(uint16_t *)buffer
                 isIdle:(bool)isIdle
                   meta:(struct rpng_hdr_metadata *)meta
{
#if METAL_HDR_AVAILABLE
   bool res      = NO;
   bool isSCRGB  = false;
   float maxCLL  = 0.0f;
   float maxFALL = 0.0f;
   bool enabled  = _context.captureEnabled;
   if (!enabled)
      _context.captureEnabled = YES;

   if (!isIdle)
      video_driver_cached_frame();

   res = [_context readViewportHDR:buffer
                            maxCLL:&maxCLL
                           maxFALL:&maxFALL
                           isSCRGB:&isSCRGB];

   if (!enabled)
      _context.captureEnabled = NO;

   if (res && meta)
   {
      /* Same tagging as the vulkan / dxgi implementations: PQ transfer,
       * BT.2100 primaries for HDR10 or BT.709 for scRGB, D65 white,
       * cLLI from the measured levels, mDCV from the configured range. */
      memset(meta, 0, sizeof(*meta));
      meta->colour_primaries      = isSCRGB ? 1 : 9;
      meta->transfer_function     = 16; /* SMPTE ST 2084 (PQ) */
      meta->matrix_coefficients   = 0;  /* RGB (must be 0 for PNG) */
      meta->video_full_range_flag = 1;
      meta->max_cll               = maxCLL;
      meta->max_fall              = maxFALL;
      meta->write_mdcv            = 1;
      if (isSCRGB)
      {
         meta->primary_chromaticity[0][0] = 0.640f; meta->primary_chromaticity[0][1] = 0.330f;
         meta->primary_chromaticity[1][0] = 0.300f; meta->primary_chromaticity[1][1] = 0.600f;
         meta->primary_chromaticity[2][0] = 0.150f; meta->primary_chromaticity[2][1] = 0.060f;
      }
      else
      {
         meta->primary_chromaticity[0][0] = 0.708f; meta->primary_chromaticity[0][1] = 0.292f;
         meta->primary_chromaticity[1][0] = 0.170f; meta->primary_chromaticity[1][1] = 0.797f;
         meta->primary_chromaticity[2][0] = 0.131f; meta->primary_chromaticity[2][1] = 0.046f;
      }
      meta->white_point[0] = 0.3127f; meta->white_point[1] = 0.3290f; /* D65 */
      /* Same mastering-display defaults the vulkan and d3d12 drivers
       * use; RetroArch has no per-display luminance query on Metal. */
      meta->max_luminance  = 1000.0f;
      meta->min_luminance  = 0.001f;
   }
   return res;
#else
   return NO;
#endif
}

- (void)syncViewport
{
   if (_shader && (_engine.frame.output_size.x != VIDEO_SCALE_W(_viewport->dims)
               ||  _engine.frame.output_size.y != VIDEO_SCALE_H(_viewport->dims)))
      resize_render_targets       = YES;

   _engine.frame.viewport.originX = VIDEO_POS_X(_viewport->pos);
   _engine.frame.viewport.originY = VIDEO_POS_Y(_viewport->pos);
   _engine.frame.viewport.width   = VIDEO_SCALE_W(_viewport->dims);
   _engine.frame.viewport.height  = VIDEO_SCALE_H(_viewport->dims);
   _engine.frame.viewport.znear   = 0.0f;
   _engine.frame.viewport.zfar    = 1.0f;
   _engine.frame.output_size.x    = VIDEO_SCALE_W(_viewport->dims);
   _engine.frame.output_size.y    = VIDEO_SCALE_H(_viewport->dims);
   _engine.frame.output_size.z    = 1.0f / VIDEO_SCALE_W(_viewport->dims);
   _engine.frame.output_size.w    = 1.0f / VIDEO_SCALE_H(_viewport->dims);

   if (resize_render_targets)
      [self _updateRenderTargets];
}

- (bool)deferFrame:(void const *)src pitch:(NSUInteger)pitch
{
   [self _clearMovieFrame];
   [self syncViewport];
   [self _ensureFront];
   _pendingSrc   = src;
   _pendingPitch = pitch;
   _pendingMem   = [self _frameMemFind:src pitch:pitch offset:&_pendingOff];
   _loanLast     = _pendingMem;
   return true;
}

- (void)commitFrame
{
   void const *src = _pendingSrc;
   if (!src)
      return;
   _pendingSrc = NULL;
   [self updateFrame:src pitch:_pendingPitch];
}

- (void)dropFrame
{
   _pendingSrc = NULL;
}

- (void)updateFrame:(void const *)src pitch:(NSUInteger)pitch
{
   [self _clearMovieFrame];
   [self syncViewport];

   [self _updateHistory];

   {
      NSUInteger off;
      int        mem    = [self _frameMemFind:src pitch:pitch offset:&off];
      bool       direct = (   _format == RPixelFormatBGRA8Unorm
                           || _format == RPixelFormatBGRX8Unorm
                           || _format == RPixelFormatBGR10A2Unorm);
      _loanLast         = mem;
      /* In frame memory - a loan, a window into one, or a frame the
       * threaded wrapper put there: already where the GPU reads it.
       * Blit it into the texture the frame is sampled from (or
       * converted from), on the blit command buffer the format
       * conversion also uses, which the context commits ahead of the
       * frame's own. */
      if (mem >= 0)
      {
         id<MTLTexture> dst = direct ? _engine.frame.texture[0].view : _src;
         id<MTLCommandBuffer> cb = _context.blitCommandBuffer;
         id<MTLBlitCommandEncoder> bce = [cb blitCommandEncoder];
         GFX_INSTR_INC(GFX_INSTR_FRAME_MEM);
         [bce copyFromBuffer:_mem[mem]
                sourceOffset:off
           sourceBytesPerRow:pitch
         sourceBytesPerImage:0
                  sourceSize:MTLSizeMake((NSUInteger)_size.width, (NSUInteger)_size.height, 1)
                   toTexture:dst
            destinationSlice:0
            destinationLevel:0
           destinationOrigin:MTLOriginMake(0, 0, 0)];
         [bce endEncoding];
         RARCH_ASSIGN(_memReader[mem], cb);
         if (!direct)
            _srcDirty = YES;
      }
      else if (direct)
      {
         id<MTLTexture> tex = _engine.frame.texture[0].view;
         GFX_INSTR_INC(GFX_INSTR_FRAME_COPY_HOST);
         [tex replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)_size.width, (NSUInteger)_size.height)
                mipmapLevel:0 withBytes:src
                bytesPerRow:pitch];
      }
      else
      {
         GFX_INSTR_INC(GFX_INSTR_FRAME_COPY_HOST);
         [_src replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)_size.width, (NSUInteger)_size.height)
                 mipmapLevel:0 withBytes:src
                 bytesPerRow:(NSUInteger)(pitch)];
         _srcDirty = YES;
      }
   }
}

/* METAL_MOVIE_FRAME_BEGIN */
- (enum video_texture_update)importMovieFrame:(const video_frame_import_t *)frame
{
   @autoreleasepool
   {
      metal_frame_sdr_params_t params;
      RArchMetalFrameLease *lease;
      id<MTLCommandBuffer> buffer;
      BOOL previous_movie = _movieFrame, previous_hdr = _movieHDR;
      bool hdr, previous_init = init_history, previous_resize = resize_render_targets;
      id<MTLTexture> previous_source;
      CGSize previous_size = _size;
      unsigned history_count, i;
      if (!frame)
      {
         [self _clearMovieFrame];
         return VIDEO_TEXTURE_UPDATE_DONE;
      }
      buffer = _context.blitCommandBuffer;
      lease = metal_frame_validate(frame, (__bridge void*)_context.device, (__bridge void*)buffer);
      if (!lease || !_context.frameSDRPipeline)
         return VIDEO_TEXTURE_UPDATE_REFUSED;
      hdr = metal_frame_hdr_params(&lease->view, &params);
      if ((hdr ? !_context.movieHDRSupported : !metal_frame_sdr_params(&lease->view, &params))
            || (frame->output_pixfmt && frame->output_pixfmt != (hdr ? IMAGE_PIXFMT_FP16 : IMAGE_PIXFMT_8888)))
         return VIDEO_TEXTURE_UPDATE_REFUSED;
      history_count = _shader && _shader->history_size > 0 ? (unsigned)_shader->history_size + 1u : 1u;
      if (history_count > sizeof(_moviePrevious) / sizeof(_moviePrevious[0]))
         return VIDEO_TEXTURE_UPDATE_REFUSED;
      previous_source = RARCH_RETAIN(_src);
      /* Persistent scratch avoids a history-sized frame allocation. Retain the
       * old slots until all fallible destination/encoder setup has succeeded. */
      for (i = 0; i < history_count; i++)
      {
         _moviePrevious[i].size_data = _engine.frame.texture[i].size_data;
         RARCH_STRUCT_ASSIGN(_moviePrevious[i].view, _engine.frame.texture[i].view);
      }
      if (!_movieFrame || _movieHDR != hdr) init_history = YES;
      _movieFrame = YES; _movieHDR = hdr;
      self.size = CGSizeMake(lease->view.width, lease->view.height);
      [self syncViewport];
      [self _updateHistory];
      if (!(hdr ? metal_frame_convert_hdr(frame, (__bridge void*)_context.device,
               (__bridge void*)buffer, _context.frameSDRPipeline,
               (__bridge void*)_engine.frame.texture[0].view)
            : metal_frame_convert_sdr(frame, (__bridge void*)_context.device,
               (__bridge void*)buffer, _context.frameSDRPipeline,
               (__bridge void*)_engine.frame.texture[0].view)))
      {
         _movieFrame = previous_movie; _movieHDR = previous_hdr;
         self.size = previous_size; init_history = previous_init;
         resize_render_targets = previous_resize;
         RARCH_ASSIGN(_src, previous_source);
         RARCH_RELEASE(previous_source);
         for (i = 0; i < history_count; i++)
         {
            RARCH_STRUCT_ASSIGN(_engine.frame.texture[i].view, _moviePrevious[i].view);
            _engine.frame.texture[i].size_data = _moviePrevious[i].size_data;
            RARCH_STRUCT_ASSIGN(_moviePrevious[i].view, nil);
         }
         [self syncViewport];
         return VIDEO_TEXTURE_UPDATE_REFUSED;
      }
      RARCH_RELEASE(previous_source);
      for (i = 0; i < history_count; i++)
         RARCH_STRUCT_ASSIGN(_moviePrevious[i].view, nil);
      _pendingSrc = NULL;
      _srcDirty = NO;
      _loanLast = -1;
      return VIDEO_TEXTURE_UPDATE_DONE;
   }
}
/* METAL_MOVIE_FRAME_END */

/* Where a pushed frame lies in frame memory: the index of the buffer,
 * with the frame's byte offset in it, when @src at @pitch with the
 * view's size is inside one, on a texel and whole - the whole loan, a
 * window into it at its pitch (an overscan crop by offset; beetle-psx
 * does this), or a frame the threaded wrapper put there. -1 otherwise. */
- (int)_frameMemFind:(const void *)src pitch:(NSUInteger)pitch
              offset:(NSUInteger *)offset
{
   int        i;
   uintptr_t  p   = (uintptr_t)src;
   NSUInteger bpp = RPixelFormatToBPP(_format);
   NSUInteger w   = (NSUInteger)_size.width;
   NSUInteger h   = (NSUInteger)_size.height;

   if (!p || !bpp || !w || !h || (pitch % bpp) || pitch < w * bpp)
      return -1;
   for (i = 0; i < METAL_FRAME_MEM_MAX; i++)
   {
      uintptr_t  base;
      NSUInteger len, off;
      if (!_mem[i])
         continue;
      base = (uintptr_t)_mem[i].contents;
      len  = _mem[i].length;
      if (p < base || p - base >= len)
         continue;
      off  = (NSUInteger)(p - base);
      if (     (off % bpp)
            || len - off < w * bpp
            || (len - off - w * bpp) / pitch < h - 1)
         return -1;
      *offset = off;
      return i;
   }
   return -1;
}

- (void *)frameMemAlloc:(size_t)size id:(uintptr_t *)ident
{
   int i;
   if (!size)
      return NULL;
   for (i = 0; i < METAL_FRAME_MEM_MAX; i++)
      if (!_mem[i])
         break;
   if (i == METAL_FRAME_MEM_MAX)
      return NULL;
   RARCH_ASSIGN(_mem[i], RARCH_AUTORELEASE_R([_context.device
         newBufferWithLength:size
                     options:MTLResourceStorageModeShared]));
   if (!_mem[i])
      return NULL;
   RARCH_RELEASE_NIL(_memReader[i]);
   *ident = (uintptr_t)i + 1;
   return _mem[i].contents;
}

/* Never waits: the last blit out of the buffer has completed */
- (bool)frameMemReady:(uintptr_t)ident
{
   NSUInteger i;
   if (!ident || ident > METAL_FRAME_MEM_MAX)
      return true;
   i = (NSUInteger)ident - 1;
   if (!_memReader[i])
      return true;
   if (_memReader[i].status < MTLCommandBufferStatusCompleted)
      return false;
   RARCH_RELEASE_NIL(_memReader[i]);
   return true;
}

/* A blit still queued holds the buffer through its command buffer */
- (void)frameMemFree:(uintptr_t)ident
{
   NSUInteger i;
   if (!ident || ident > METAL_FRAME_MEM_MAX)
      return;
   i = (NSUInteger)ident - 1;
   RARCH_RELEASE_NIL(_memReader[i]);
   RARCH_RELEASE_NIL(_mem[i]);
   if (_loanLast == (int)i)
      _loanLast = -1;
}

/* Lend the core a framebuffer of the size it asks for, in the frame's
 * pixel format, at the tight pitch: three buffers of frame memory,
 * shared storage, so on Apple silicon the core writes the memory the
 * GPU reads, and a frame pushed from one needs one blit and no host
 * copy (updateFrame:pitch:). Never the buffer the last frame was pushed
 * from, which the cached frame names, nor one a blit still reads; none
 * is waited for, and the core keeps its own buffer meanwhile. Declined
 * for a 10-bit source. A resize retires the cached frame first, as the
 * pointer it holds is into a buffer about to go. */
- (bool)lendFramebuffer:(struct retro_framebuffer *)fb
{
   NSUInteger bpp, stride;
   unsigned   k;

   if (!fb->width || !fb->height)
      return false;
   switch (_format)
   {
      case RPixelFormatBGRA8Unorm:
      case RPixelFormatBGRX8Unorm:
      case RPixelFormatB5G6R5Unorm:
         break;
      default:
         return false;
   }
   bpp    = RPixelFormatToBPP(_format);
   stride = (NSUInteger)fb->width * bpp;

   if (     !_loanMem[0]
         || _loanWidth  != fb->width
         || _loanHeight != fb->height
         || _loanStride != stride)
   {
      uintptr_t ident;
      /* The cached frame may point into a buffer about to go */
      video_driver_cached_frame_retire();
      for (k = 0; k < METAL_LOAN_SLOTS; k++)
      {
         [self frameMemFree:(uintptr_t)_loanMem[k]];
         _loanMem[k] = 0;
      }
      _loanWidth = _loanHeight = _loanStride = 0;
      for (k = 0; k < METAL_LOAN_SLOTS; k++)
      {
         ident = 0;
         if (![self frameMemAlloc:stride * fb->height id:&ident])
         {
            unsigned j;
            for (j = 0; j < k; j++)
            {
               [self frameMemFree:(uintptr_t)_loanMem[j]];
               _loanMem[j] = 0;
            }
            return false;
         }
         _loanMem[k] = (int)ident;
      }
      GFX_INSTR_INC(GFX_INSTR_FRAME_TEX_CREATE);
      _loanWidth  = fb->width;
      _loanHeight = fb->height;
      _loanStride = stride;
   }

   for (k = 0; k < METAL_LOAN_SLOTS; k++)
   {
      unsigned s = (_loanNext + k) % METAL_LOAN_SLOTS;
      int      i = _loanMem[s] - 1;
      if (     i != _loanLast
            && [self frameMemReady:(uintptr_t)_loanMem[s]])
      {
         _loanNext        = (s + 1) % METAL_LOAN_SLOTS;
         fb->data         = _mem[i].contents;
         fb->pitch        = stride;
         fb->format       = (_format == RPixelFormatB5G6R5Unorm)
            ? RETRO_PIXEL_FORMAT_RGB565 : RETRO_PIXEL_FORMAT_XRGB8888;
         /* Shared storage is host-cached memory: the core may read it
          * back. */
         fb->memory_flags = RETRO_MEMORY_TYPE_CACHED;
         return true;
      }
   }
   return false;
}

/* METAL_FRAME_TEXTURE_BEGIN */
- (void)_initTexture:(texture_t *)t withDescriptor:(MTLTextureDescriptor *)td
{
   RARCH_STRUCT_ASSIGN(t->view, RARCH_AUTORELEASE_R([_context.device newTextureWithDescriptor:td]));
   t->size_data.x = td.width;
   t->size_data.y = td.height;
   t->size_data.z = 1.0f / td.width;
   t->size_data.w = 1.0f / td.height;
}
/* METAL_FRAME_TEXTURE_END */

- (void)drawWithEncoder:(id<MTLRenderCommandEncoder>)rce
{
   if (_texture)
   {
      [rce setViewport:_engine.frame.viewport];
      [rce setVertexBytes:&_v length:sizeof(_v) atIndex:BufferIndexPositions];
      [rce setFragmentTexture:_texture atIndex:TextureIndexColor];
      [rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
   }
}

- (void)prepareFrame
{
   RARCH_ASSIGN(_texture, _engine.frame.texture[0].view);

   if (     (_format != RPixelFormatBGRA8Unorm)
         && (_format != RPixelFormatBGRX8Unorm)
         && (_format != RPixelFormatBGR10A2Unorm)
         && _srcDirty)
   {
      [_context convertFormat:_format from:_src to:_texture];
      _srcDirty = NO;
   }
}

- (void)drawWithContext:(Context *)ctx
{
   [self prepareFrame];

   if (!_shader || _shader->passes == 0)
      return;

   [self _chainFeedback:&_engine];
   RARCH_ASSIGN(_texture, [self _chainRender:&_engine context:ctx
         backBuffer:YES]);

   if (_texture)
      _drawState = ViewDrawStateAll;
   else
      _drawState = ViewDrawStateContext;
}

- (void)_chainFeedback:(engine_t *)e
{
   size_t i;
   for (i = 0; i < _shader->passes; i++)
   {
      if (_shader->pass[i].feedback)
      {
         texture_t tmp      = e->pass[i].feedback;
         e->pass[i].feedback = e->pass[i].rt;
         e->pass[i].rt       = tmp;
      }
   }
}

/* Pass @i of @e on @rce: its pipeline, constants, textures and quad */
- (BOOL)_chainBind:(engine_t *)e pass:(size_t)i
           encoder:(id<MTLRenderCommandEncoder>)rce
{
   int j;
   const HDRUniforms *hdr = [_context currentHDRUniforms];
   __unsafe_unretained id<MTLTexture> textures[SLANG_NUM_BINDINGS] = {NULL};
   id<MTLSamplerState> samplers[SLANG_NUM_BINDINGS] = {NULL};

   if (!rce)
      return NO;
   [rce setRenderPipelineState:e->pass[i]._state];

   NSURL *shaderPath = [NSURL fileURLWithPath:e->pass[i]._state.label];
   rce.label = shaderPath.lastPathComponent.stringByDeletingPathExtension;

   e->pass[i].frame_count = (uint32_t)_frameCount;
   if (_shader->pass[i].frame_count_mod)
      e->pass[i].frame_count %= _shader->pass[i].frame_count_mod;
   /* Not modulo'd: SwapCount counts what the display was shown */
   e->pass[i].swap_count       = (uint32_t)_swapCount;
   e->pass[i].total_subframes  = _totalSubframes;
   e->pass[i].current_subframe = _currentSubframe;
   e->pass[i].hdr_mode = _context.hdrOutputMode;
   e->pass[i].passthrough = e->pass[i].hdr_mode
      && (_shaderEmitsHDR10 || _shaderEmitsHDR16) ? 1 : 0;
   e->pass[i].enable_hdr = e->pass[i].hdr_mode ? 1.0f : 0.0f;
   e->pass[i].paper_white_nits = e->pass[i].hdr_mode
      ? _hdrPaperWhiteNits : 0.0f;
   e->pass[i].peak_nits = e->pass[i].hdr_mode
      ? _hdrPeakNits : 0.0f;
   e->pass[i].inverse_tonemap = e->pass[i].hdr_mode && hdr
      ? hdr->InverseTonemap : 0.0f;
   e->pass[i].hdr10 = e->pass[i].hdr_mode && hdr
      ? hdr->HDR10 : 0.0f;
   e->pass[i].scanlines = e->pass[i].hdr_mode && hdr
      ? hdr->Scanlines : 0.0f;
   e->pass[i].subpixel_layout = e->pass[i].hdr_mode && hdr
      ? hdr->SubpixelLayout : 0;
   e->pass[i].expand_gamut = e->pass[i].hdr_mode
      ? _hdrExpandGamut : 0;

#ifdef HAVE_REWIND
   if (state_manager_frame_is_reversed())
      e->pass[i].frame_direction = -1;
   else
#else
   e->pass[i].frame_direction    = 1;
#endif
   e->pass[i].frame_time_delta   = (uint32_t)video_driver_get_frame_time_delta_usec();
   e->pass[i].original_fps       = video_driver_get_original_fps();
   e->pass[i].rotation           = retroarch_get_rotation();
   e->pass[i].core_aspect        = video_driver_get_core_aspect();

   /* OriginalAspectRotated: return 1 / aspect for 90 and 270 rotated content */
   int rot                       = retroarch_get_rotation();
   float core_aspect_rot         = video_driver_get_core_aspect();
   if (rot == 1 || rot == 3)
      core_aspect_rot            = 1 / core_aspect_rot;
   e->pass[i].core_aspect_rot    = core_aspect_rot;

   for (j = 0; j < SLANG_CBUFFER_MAX; j++)
   {
      id<MTLBuffer> buffer      = e->pass[i].buffers[j];
      cbuffer_sem_t *buffer_sem = &e->pass[i].semantics.cbuffers[j];

      if (buffer_sem->size && buffer_sem->stage_mask && buffer_sem->uniforms)
      {
         void             *data = buffer.contents;
         uniform_sem_t *uniform = buffer_sem->uniforms;

         if (!data || buffer.length < buffer_sem->size)
            return NO;
         while (uniform->size)
         {
            if (uniform->param)
            {
               float v = video_shader_param_get(uniform->param);
               memcpy((uint8_t *)data + uniform->offset, &v, sizeof(v));
            }
            else if (uniform->data)
               memcpy((uint8_t *)data + uniform->offset, uniform->data, uniform->size);
            uniform++;
         }

         if (!metal_bind_slang_constants(rce, _context.device,
                  buffer_sem, buffer))
            return NO;
      }
   }

   texture_sem_t *texture_sem = e->pass[i].semantics.textures;
   while (texture_sem->stage_mask)
   {
      int binding        = texture_sem->binding;
      id<MTLTexture> tex = (__bridge id<MTLTexture>)*(void **)texture_sem->texture_data;
      textures[binding]  = tex;

      samplers[binding]  = _samplers[texture_sem->filter][texture_sem->wrap][tex.mipmapLevelCount > 1 ? 1 : 0];
      texture_sem++;
   }

   [rce setFragmentTextures:textures withRange:NSMakeRange(0, SLANG_NUM_BINDINGS)];
   [rce setFragmentSamplerStates:samplers withRange:NSMakeRange(0, SLANG_NUM_BINDINGS)];
   [rce setVertexBytes:_vertex length:sizeof(_vertex) atIndex:4];
   return YES;
}

/* @e's passes drawn, each into its render target; a last pass that has
 * none into the frame's encoder with @toBackBuffer, else left to the
 * caller. Returns the last pass's render target, nil for none. */
- (id<MTLTexture>)_chainRender:(engine_t *)e context:(Context *)ctx
                    backBuffer:(BOOL)toBackBuffer
{
   size_t i;
   id<MTLCommandBuffer> cb = ctx.blitCommandBuffer;

   e->frame.valid = false;
   MTLRenderPassDescriptor *rpd        = RARCH_AUTORELEASE_R([MTLRenderPassDescriptor new]);
   rpd.colorAttachments[0].loadAction  = MTLLoadActionDontCare;
   rpd.colorAttachments[0].storeAction = MTLStoreActionStore;

   for (i = 0; i < _shader->passes; i++)
   {
      id<MTLRenderCommandEncoder> rce = nil;
      BOOL backBuffer                 = (e->pass[i].rt.view == nil);

      if (backBuffer)
      {
         if (!toBackBuffer)
            break;
         rce = _context.rce;
      }
      else
      {
         rpd.colorAttachments[0].texture = e->pass[i].rt.view;
         rce = [cb renderCommandEncoderWithDescriptor:rpd];
      }

      if (![self _chainBind:e pass:i encoder:rce])
      {
         if (!backBuffer)
            [rce endEncoding];
         return nil;
      }

      if (backBuffer)
         [rce setViewport:e->frame.viewport];
      else
         [rce setViewport:e->pass[i].viewport];

      [rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];

      if (!backBuffer)
      {
         [rce endEncoding];

         if (e->pass[i].rt.view.mipmapLevelCount > 1)
         {
            id<MTLBlitCommandEncoder> bce = [cb blitCommandEncoder];
            [bce generateMipmapsForTexture:(e->pass[i].rt.view)];
            [bce endEncoding];
            bce = nil;
         }
      }
   }

   e->frame.valid = true;
   return e->pass[_shader->passes - 1].rt.view;
}

- (void)_updateRenderTargets
{
   if (!_shader || !resize_render_targets)
      return;

   [self _chainTargets:&_engine width:(NSUInteger)_size.width
                height:(NSUInteger)_size.height viewport:_viewport->dims
                   hdr:_hdrEnabled];

   resize_render_targets = NO;
}

/* @e's render targets for a frame of @width x @height drawn at
 * @vp_dims, the last pass's for HDR output with @hdr */
- (void)_chainTargets:(engine_t *)e width:(NSUInteger)width
               height:(NSUInteger)height viewport:(unsigned)vp_dims
                  hdr:(BOOL)hdr
{
   size_t i;
   NSUInteger src_w = width;
   NSUInteger src_h = height;

   /* Release existing targets */
   for (i = 0; i < _shader->passes; i++)
   {
      RARCH_STRUCT_ASSIGN(e->pass[i].rt.view, nil);
      RARCH_STRUCT_ASSIGN(e->pass[i].feedback.view, nil);
      memset(&e->pass[i].rt, 0, sizeof(e->pass[i].rt));
      memset(&e->pass[i].feedback, 0, sizeof(e->pass[i].feedback));
   }

   e->mvp_last_pass = _context.uniformsNoRotate->projectionMatrix;
   int rot = retroarch_get_rotation();

   width  = src_w;
   height = src_h;

   for (i = 0; i < _shader->passes; i++)
   {
      struct video_shader_pass *shader_pass = &_shader->pass[i];

      if (shader_pass->fbo.flags & FBO_SCALE_FLAG_VALID)
      {
         switch (shader_pass->fbo.type_x)
         {
            case RARCH_SCALE_INPUT:
               width *= shader_pass->fbo.scale_x;
               break;

            case RARCH_SCALE_VIEWPORT:
               width = (NSUInteger)((rot % 2 ? VIDEO_SCALE_H(vp_dims) : VIDEO_SCALE_W(vp_dims)) * shader_pass->fbo.scale_x);
               break;

            case RARCH_SCALE_ABSOLUTE:
               width = shader_pass->fbo.abs_x;
               break;

            default:
               break;
         }

         if (!width)
            width = VIDEO_SCALE_W(vp_dims);

         switch (shader_pass->fbo.type_y)
         {
            case RARCH_SCALE_INPUT:
               height *= shader_pass->fbo.scale_y;
               break;

            case RARCH_SCALE_VIEWPORT:
               height = (NSUInteger)((rot % 2 ? VIDEO_SCALE_W(vp_dims) : VIDEO_SCALE_H(vp_dims)) * shader_pass->fbo.scale_y);
               break;

            case RARCH_SCALE_ABSOLUTE:
               height = shader_pass->fbo.abs_y;
               break;

            default:
               break;
         }

         if (!height)
            height = VIDEO_SCALE_H(vp_dims);
      }
      else if (i == (_shader->passes - 1))
      {
         width  = rot % 2 ? VIDEO_SCALE_H(vp_dims) : VIDEO_SCALE_W(vp_dims);
         height = rot % 2 ? VIDEO_SCALE_W(vp_dims) : VIDEO_SCALE_H(vp_dims);
      }

      /* Updating framebuffer size */

      MTLPixelFormat fmt = SelectOptimalPixelFormat(glslang_format_to_metal(e->pass[i].semantics.format));
      /* When HDR is on, the last pass needs an explicit RT (the HDR
       * composite pass will later sample from it).  Force allocation
       * regardless of dimension/format match to the viewport.
       * We still honour the shader's semantics.format for LAST pass if
       * it's an explicit HDR format (RGBA16F or RGB10A2 == HDR10 PQ),
       * otherwise RGBA16F gives comfortable precision for the composite. */
      BOOL lastPass = (i == (_shader->passes - 1));
      BOOL forceAllocForHDR = NO;
      if (lastPass && hdr)
      {
         forceAllocForHDR = YES;
         /* Must match the attachment format _initPipelines picks, which
          * only honours an HDR format the shader declared itself.  A
          * format derived from preset FBO flags is not an HDR encode, so
          * the composite input stays RGBA16F for it. */
         if (!(   e->pass[i].semantics.explicit_format
               && (   fmt == MTLPixelFormatRGBA16Float
                   || fmt == MTLPixelFormatRGB10A2Unorm)))
            fmt = MTLPixelFormatRGBA16Float;
      }

      if (   (!lastPass)
          || forceAllocForHDR
          || shader_pass->feedback
          || (width  != VIDEO_SCALE_W(vp_dims))
          || (height != VIDEO_SCALE_H(vp_dims))
          || fmt != MTLPixelFormatBGRA8Unorm)
      {
         e->pass[i].viewport.width  = width;
         e->pass[i].viewport.height = height;
         e->pass[i].viewport.znear  = 0.0;
         e->pass[i].viewport.zfar   = 1.0;

         bool useMipMap = false;
         if (!lastPass)
         {
           /* mipmap refers to the input of the pass */
            struct video_shader_pass *nextPass = &_shader->pass[i + 1];
            if (nextPass && nextPass->mipmap)
               useMipMap = true;
         }

         MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:fmt width:width height:height mipmapped:(useMipMap ? YES : NO)];
        // td.storageMode = MTLStorageModePrivate;
         td.usage       = MTLTextureUsageShaderRead
                        | MTLTextureUsageRenderTarget;

         if (useMipMap)
            td.mipmapLevelCount = [self getMipLevels:(unsigned)width height:(unsigned)height];

         [self _initTexture:&e->pass[i].rt withDescriptor:td];

         if (shader_pass->feedback)
            [self _initTexture:&e->pass[i].feedback withDescriptor:td];
      }
      else
      {
         if (rot % 2)
         {
             NSUInteger tmp = width;
             width = height;
             height = tmp;
         }

         e->mvp_last_pass = _context.uniforms->projectionMatrix;
         e->pass[i].rt.size_data.x = width;
         e->pass[i].rt.size_data.y = height;
         e->pass[i].rt.size_data.z = 1.0f / width;
         e->pass[i].rt.size_data.w = 1.0f / height;
      }
   }
}

/* A view's chain: the main chain's pipelines and lookup textures,
 * referenced, with semantics that read this chain's own values and
 * textures - where the main chain's point into it, these point at the
 * same place in the copy - and constant buffers, render targets and
 * history of its own. The preset is the main chain's, so its parameters
 * are the ones edited. NULL when it can't be made. */
- (metal_view_chain_t *)makeViewChain
{
   size_t i;
   int j;
   const char *lo = (const char *)&_engine;
   const char *hi = lo + sizeof(_engine);
   metal_view_chain_t *c;
   if (!_shader || !_shader->passes)
      return NULL;
   if (!(c = (metal_view_chain_t *)calloc(1, sizeof(*c))))
      return NULL;
#define METAL_REBASE(ptr) (((const char *)(ptr) >= lo && (const char *)(ptr) < hi) \
      ? (void *)((char *)&c->engine + ((const char *)(ptr) - lo)) : (void *)(ptr))

   c->engine.mvp = _engine.mvp;
   for (i = 0; i < _shader->luts; i++)
   {
      RARCH_STRUCT_ASSIGN(c->engine.luts[i].view, _engine.luts[i].view);
      c->engine.luts[i].size_data = _engine.luts[i].size_data;
   }
   for (i = 0; i < _shader->passes; i++)
   {
      const pass_semantics_t *sp = &_engine.pass[i].semantics;
      pass_semantics_t       *dp = &c->engine.pass[i].semantics;
      RARCH_STRUCT_ASSIGN(c->engine.pass[i]._state, _engine.pass[i]._state);
      *dp          = *sp;
      dp->textures = NULL;
      for (j = 0; j < SLANG_CBUFFER_MAX; j++)
         dp->cbuffers[j].uniforms = NULL;
      if (sp->textures)
      {
         int k;
         int n = sp->texture_count + 1;
         if (!(dp->textures = (texture_sem_t *)malloc(n * sizeof(*dp->textures))))
            goto error;
         memcpy(dp->textures, sp->textures, n * sizeof(*dp->textures));
         for (k = 0; k < n; k++)
            dp->textures[k].texture_data = METAL_REBASE(dp->textures[k].texture_data);
      }
      for (j = 0; j < SLANG_CBUFFER_MAX; j++)
      {
         const cbuffer_sem_t *sb = &sp->cbuffers[j];
         id<MTLBuffer> src       = _engine.pass[i].buffers[j];
         if (sb->uniforms)
         {
            int k;
            int n            = sb->uniform_count + 1;
            uniform_sem_t *u = (uniform_sem_t *)malloc(n * sizeof(*u));
            if (!u)
               goto error;
            memcpy(u, sb->uniforms, n * sizeof(*u));
            for (k = 0; k < n; k++)
               u[k].data = METAL_REBASE(u[k].data);
            dp->cbuffers[j].uniforms = u;
         }
         if (src)
         {
            id<MTLBuffer> buf = RARCH_AUTORELEASE_R([_context.device
                  newBufferWithLength:src.length
                  options:PLATFORM_METAL_RESOURCE_STORAGE_MODE]);
            if (!buf)
               goto error;
            RARCH_STRUCT_ASSIGN(c->engine.pass[i].buffers[j], buf);
         }
      }
   }
#undef METAL_REBASE
   return c;

error:
   [self freeViewChain:c];
   return NULL;
}

- (void)freeViewChain:(metal_view_chain_t *)c
{
   int i, j;
   if (!c)
      return;
   for (i = 0; i < GFX_MAX_SHADERS; i++)
   {
      RARCH_STRUCT_ASSIGN(c->engine.pass[i].rt.view, nil);
      RARCH_STRUCT_ASSIGN(c->engine.pass[i].feedback.view, nil);
      RARCH_STRUCT_ASSIGN(c->engine.pass[i]._state, nil);
      for (j = 0; j < SLANG_CBUFFER_MAX; j++)
      {
         RARCH_STRUCT_ASSIGN(c->engine.pass[i].buffers[j], nil);
         free(c->engine.pass[i].semantics.cbuffers[j].uniforms);
      }
      free(c->engine.pass[i].semantics.textures);
   }
   for (i = 0; i < GFX_MAX_TEXTURES; i++)
      RARCH_STRUCT_ASSIGN(c->engine.luts[i].view, nil);
   for (i = 0; i < GFX_MAX_FRAME_HISTORY + 1; i++)
      RARCH_STRUCT_ASSIGN(c->engine.frame.texture[i].view, nil);
   RARCH_STRUCT_ASSIGN(c->src16.view, nil);
   free(c);
}

/* View @v's chain made ready and its passes drawn into their render
 * targets, but for a last pass that draws into each placement: the view
 * put into the chain's history, behind the history the preset keeps for
 * a @fresh frame - written straight out of a frame deferFrame:pitch: put
 * off, else copied out of the frame. @vp_dims is the size its first
 * placement is drawn at. */
- (void)renderViewChain:(metal_view_chain_t *)c
                   view:(const struct retro_video_view *)v
               viewport:(unsigned)vp_dims fresh:(bool)fresh
{
   unsigned dims      = VIDEO_SCALE_PACK(v->width, v->height);
   id<MTLTexture> src = _engine.frame.texture[0].view;

   c->engine.frame.valid = false;
   if (src && (fresh || !c->history_made))
   {
      unsigned k;
      unsigned n                     = _shader->history_size + 1;
      id<MTLTexture> front           = c->engine.frame.texture[0].view;
      id<MTLBlitCommandEncoder> bce;
      if (     !c->history_made
            || !front
            || front.width       != v->width
            || front.height      != v->height
            || front.pixelFormat != src.pixelFormat)
      {
         MTLTextureDescriptor *td = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:src.pixelFormat
            width:v->width height:v->height mipmapped:NO];
         /* Written by a 16-bit format's conversion too */
         td.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
         for (k = 0; k < n; k++)
            [self _initTexture:&c->engine.frame.texture[k] withDescriptor:td];
         c->history_made = true;
      }
      else if (fresh && n > 1)
      {
         texture_t tmp = c->engine.frame.texture[n - 1];
         for (k = n - 1; k > 0; k--)
            c->engine.frame.texture[k] = c->engine.frame.texture[k - 1];
         c->engine.frame.texture[0] = tmp;
      }
      if (c->engine.frame.texture[0].view && fresh && _pendingSrc)
      {
         id<MTLTexture> dst = c->engine.frame.texture[0].view;
         NSUInteger at      = (NSUInteger)v->y * _pendingPitch
            + (NSUInteger)v->x * _bpp;
         bool convert       = (   _format != RPixelFormatBGRA8Unorm
                               && _format != RPixelFormatBGRX8Unorm
                               && _format != RPixelFormatBGR10A2Unorm);
         /* A 16-bit view goes in as it is, then is converted as the
          * frame would have been */
         if (convert)
         {
            if (     !c->src16.view
                  || c->src16.view.width  != v->width
                  || c->src16.view.height != v->height)
            {
               MTLTextureDescriptor *td = [MTLTextureDescriptor
                  texture2DDescriptorWithPixelFormat:MTLPixelFormatR16Uint
                  width:v->width height:v->height mipmapped:NO];
               [self _initTexture:&c->src16 withDescriptor:td];
            }
            dst = c->src16.view;
         }
         if (_pendingMem >= 0)
         {
            /* Already where the GPU reads it: blitted out of there */
            id<MTLCommandBuffer> cb = _context.blitCommandBuffer;
            bce = [cb blitCommandEncoder];
            [bce copyFromBuffer:_mem[_pendingMem]
                   sourceOffset:_pendingOff + at
              sourceBytesPerRow:_pendingPitch
            sourceBytesPerImage:0
                     sourceSize:MTLSizeMake(v->width, v->height, 1)
                      toTexture:dst
               destinationSlice:0
               destinationLevel:0
              destinationOrigin:MTLOriginMake(0, 0, 0)];
            [bce endEncoding];
            RARCH_ASSIGN(_memReader[_pendingMem], cb);
         }
         else
            [dst replaceRegion:MTLRegionMake2D(0, 0, v->width, v->height)
                   mipmapLevel:0
                     withBytes:(const uint8_t*)_pendingSrc + at
                   bytesPerRow:_pendingPitch];
         if (convert)
            [_context convertFormat:_format from:dst
                                 to:c->engine.frame.texture[0].view];
         if (!_writtenSeen)
         {
            _writtenSeen = true;
            RARCH_LOG("[Metal] Views' presets have each view of a software "
                  "frame written straight in.\n");
         }
      }
      else if (c->engine.frame.texture[0].view)
      {
         bce = [_context.blitCommandBuffer blitCommandEncoder];
         [bce copyFromTexture:src
                  sourceSlice:0
                  sourceLevel:0
                 sourceOrigin:MTLOriginMake(v->x, v->y, 0)
                   sourceSize:MTLSizeMake(v->width, v->height, 1)
                    toTexture:c->engine.frame.texture[0].view
             destinationSlice:0
             destinationLevel:0
            destinationOrigin:MTLOriginMake(0, 0, 0)];
         [bce endEncoding];
      }
   }

   if (c->rt_frame_dims != dims || c->rt_vp_dims != vp_dims)
   {
      [self _chainTargets:&c->engine width:v->width height:v->height
                 viewport:vp_dims hdr:NO];
      c->rt_frame_dims = dims;
      c->rt_vp_dims    = vp_dims;
   }
   c->engine.frame.output_size.x = VIDEO_SCALE_W(vp_dims);
   c->engine.frame.output_size.y = VIDEO_SCALE_H(vp_dims);
   c->engine.frame.output_size.z = 1.0f / VIDEO_SCALE_W(vp_dims);
   c->engine.frame.output_size.w = 1.0f / VIDEO_SCALE_H(vp_dims);

   if (!c->engine.frame.texture[0].view)
      return;
   [self _chainFeedback:&c->engine];
   [self _chainRender:&c->engine context:_context backBuffer:NO];
}

- (id<MTLTexture>)viewChainFinal:(metal_view_chain_t *)c
{
   if (!c->engine.frame.valid)
      return nil;
   return c->engine.pass[_shader->passes - 1].rt.view;
}

- (BOOL)bindViewChainLast:(metal_view_chain_t *)c
                  encoder:(id<MTLRenderCommandEncoder>)rce
{
   return c->engine.frame.valid
      && [self _chainBind:&c->engine pass:_shader->passes - 1 encoder:rce];
}

- (void)_freeVideoShader:(struct video_shader *)shader
{
   int i;
   if (!shader)
      return;

   for (i = 0; i < GFX_MAX_SHADERS; i++)
   {
      int j;
      RARCH_STRUCT_ASSIGN(_engine.pass[i].rt.view, nil);
      RARCH_STRUCT_ASSIGN(_engine.pass[i].feedback.view, nil);
      memset(&_engine.pass[i].rt, 0, sizeof(_engine.pass[i].rt));
      memset(&_engine.pass[i].feedback, 0, sizeof(_engine.pass[i].feedback));

      RARCH_STRUCT_ASSIGN(_engine.pass[i]._state, nil);

      for (j = 0; j < SLANG_CBUFFER_MAX; j++)
      {
         RARCH_STRUCT_ASSIGN(_engine.pass[i].buffers[j], nil);
      }
   }

   for (i = 0; i < GFX_MAX_TEXTURES; i++)
   {
      RARCH_STRUCT_ASSIGN(_engine.luts[i].view, nil);
   }

   free(shader);
}

- (unsigned)getMipLevels:(unsigned)_width height:(unsigned)_height
{
   unsigned levels = 0;
   unsigned size = _width > _height ? _width : _height;
   if (!size)
      size = 1;
   
   while(size)
   {
      size >>= 1;
      levels++;
   }
   return levels;
}

- (BOOL)setShaderFromPath:(NSString *)path
{
   [self _freeVideoShader:_shader];
   _shader                      = nil;

   /* Reset shader-emits-HDR flags.  These propagate to Context after the
    * pass loop so the composite pipeline knows whether the last pass is
    * already emitting HDR content. */
   _shaderEmitsHDR10            = NO;
   _shaderEmitsHDR16            = NO;

   struct video_shader *shader  = (struct video_shader *)calloc(1, sizeof(*shader));
   /* NULL-check: video_shader_load_preset_into_shader below
    * writes shader->path / shader->passes etc. via field access
    * with no NULL guard on its second parameter, so a NULL
    * shader would crash inside the parser.  Return NO here -
    * caller treats this as a shader-load failure and falls
    * back to the stock (non-slang) render path.  The @finally
    * block below is a no-op for NULL shader via the guard in
    * _freeVideoShader. */
   if (!shader)
      return NO;
   settings_t        *settings  = config_get_ptr();
   const char *dir_video_shader = settings->paths.directory_video_shader;
   NSString *shadersPath = [NSString stringWithFormat:@"%s/", dir_video_shader];

   @try
   {
      unsigned i;
      uint8_t lut_read[GFX_MAX_TEXTURES];
      texture_t *source = NULL;
      if (!video_shader_load_preset_into_shader(path.UTF8String, shader)
            || !video_shader_check_hdr_output(shader, _context.hdrOutputMode))
         return NO;

      source = &_engine.frame.texture[0];

      for (i = 0; i < shader->passes; source = &_engine.pass[i++].rt)
      {
         matrix_float4x4 *mvp = (i == shader->passes-1)
            ? &_engine.mvp_last_pass
            : &_engine.mvp;

         /* clang-format off */
         semantics_map_t semantics_map = {
            {
               /* Original */
               {&_engine.frame.texture[0].view, 0,
                &_engine.frame.texture[0].size_data, 0},

               /* Source */
               {&source->view, 0, &source->size_data, 0},

               /* OriginalHistory */
               {&_engine.frame.texture[0].view,      sizeof(*_engine.frame.texture),
                &_engine.frame.texture[0].size_data, sizeof(*_engine.frame.texture)},

               /* PassOutput */
               {&_engine.pass[0].rt.view,      sizeof(*_engine.pass),
                &_engine.pass[0].rt.size_data, sizeof(*_engine.pass)},

               /* PassFeedback */
               {&_engine.pass[0].feedback.view,      sizeof(*_engine.pass),
                &_engine.pass[0].feedback.size_data, sizeof(*_engine.pass)},

               /* User */
               {&_engine.luts[0].view,      sizeof(*_engine.luts),
                &_engine.luts[0].size_data, sizeof(*_engine.luts)},
            },
            {
               mvp,                              /* MVP */
               &_engine.pass[i].rt.size_data,    /* OutputSize */
               &_engine.frame.output_size,       /* FinalViewportSize */
               &_engine.pass[i].frame_count,     /* FrameCount */
               &_engine.pass[i].frame_direction, /* FrameDirection */
               &_engine.pass[i].frame_time_delta,/* FrameTimeDelta */
               &_engine.pass[i].original_fps,        /* OriginalFPS */
               &_engine.pass[i].rotation,        /* Rotation */
               &_engine.pass[i].core_aspect,     /* OriginalAspect */
               &_engine.pass[i].core_aspect_rot, /* OriginalAspectRotated */
               &_engine.pass[i].total_subframes, /* TotalSubFrames */
               &_engine.pass[i].current_subframe,/* CurrentSubFrame */
               &_engine.pass[i].hdr_mode,         /* HDRMode */
               &_engine.pass[i].paper_white_nits, /* BrightnessNits */
               &_engine.pass[i].scanlines,        /* Scanlines */
               &_engine.pass[i].subpixel_layout,  /* SubpixelLayout */
               &_engine.pass[i].expand_gamut,     /* ExpandGamut */
               &_engine.pass[i].inverse_tonemap, /* InverseTonemap */
               &_engine.pass[i].hdr10,            /* HDR10 */
               NULL,                             /* Gyroscope */
               NULL,                             /* Accelerometer */
               NULL,                             /* AccelerometerRest */
               &_engine.pass[i].swap_count,      /* SwapCount */
            }
         };
         /* clang-format on */

         semantics_map.uniforms[SLANG_SEMANTIC_PAPER_WHITE_ALIAS] =
               semantics_map.uniforms[SLANG_SEMANTIC_PAPER_WHITE_NITS];
         semantics_map.uniforms[SLANG_SEMANTIC_PEAK_NITS] =
               &_engine.pass[i].peak_nits;
         semantics_map.uniforms[SLANG_SEMANTIC_MAX_NITS_ALIAS] =
               semantics_map.uniforms[SLANG_SEMANTIC_PEAK_NITS];
         semantics_map.uniforms[SLANG_SEMANTIC_PASSTHROUGH] =
               &_engine.pass[i].passthrough;
         semantics_map.uniforms[SLANG_SEMANTIC_ENABLE_HDR] =
               &_engine.pass[i].enable_hdr;

         if (!slang_process(shader, i, RARCH_SHADER_METAL, 20000, &semantics_map, &_engine.pass[i].semantics))
            return NO;

#ifdef DEBUG
         bool save_msl    = true;
#else
         bool save_msl    = false;
#endif
         NSString *vs_src = [NSString stringWithUTF8String:shader->pass[i].source.string.vertex];
         NSString *fs_src = [NSString stringWithUTF8String:shader->pass[i].source.string.fragment];

         /* Vertex descriptor */
         @try
         {
            NSError *err;
            MTLVertexDescriptor *vd      = RARCH_AUTORELEASE_R([MTLVertexDescriptor new]);
            vd.attributes[0].offset      = offsetof(VertexSlang, position);
            vd.attributes[0].format      = MTLVertexFormatFloat4;
            vd.attributes[0].bufferIndex = 4;
            vd.attributes[1].offset      = offsetof(VertexSlang, texCoord);
            vd.attributes[1].format      = MTLVertexFormatFloat2;
            vd.attributes[1].bufferIndex = 4;
            vd.layouts[4].stride         = sizeof(VertexSlang);
            vd.layouts[4].stepFunction   = MTLVertexStepFunctionPerVertex;

            MTLRenderPipelineDescriptor *psd = RARCH_AUTORELEASE_R([MTLRenderPipelineDescriptor new]);

            psd.label = [[NSString stringWithUTF8String:shader->pass[i].source.path]
                          stringByReplacingOccurrencesOfString:shadersPath withString:@""];

            MTLRenderPipelineColorAttachmentDescriptor *ca = psd.colorAttachments[0];

            /* Pipeline colour-attachment format must match the render target
             * the pass will bind.  For intermediate passes this is whatever
             * the shader asked for, optimised; for the last pass it's the
             * drawable format in SDR mode, or the HDR offscreen format in
             * HDR mode.
             *
             * Shader-emitted HDR: if the shader preset explicitly requested
             * A2B10G10R10 (HDR10 PQ) or R16G16B16A16_SFLOAT (FP16 HDR16) as
             * its last-pass format, honour that even when HDR is on — the
             * composite fragment switches paths based on that via
             * Context.setHDRShaderEmitsHDR10/emitsHDR16. */
            BOOL lastPass = (i == shader->passes - 1);
            MTLPixelFormat fmt = SelectOptimalPixelFormat(glslang_format_to_metal(_engine.pass[i].semantics.format));
            BOOL passEmitsHDR10 = NO;
            BOOL passEmitsHDR16 = NO;
            if (lastPass)
            {
               /* Only a format the shader declared itself (#pragma format)
                * means the shader performed its own HDR encode.  A format
                * derived from preset FBO flags (float_framebuffer /
                * rgb10_framebuffer) carries no such intent, so it must not
                * put the composite pass into passthrough. */
               if (_engine.pass[i].semantics.explicit_format)
               {
                  if (   _engine.pass[i].semantics.format == SLANG_FORMAT_A2B10G10R10_UNORM_PACK32
                      || _engine.pass[i].semantics.format == SLANG_FORMAT_A2B10G10R10_UINT_PACK32)
                     passEmitsHDR10 = YES;
                  else if (_engine.pass[i].semantics.format == SLANG_FORMAT_R16G16B16A16_SFLOAT)
                     passEmitsHDR16 = YES;
               }

               if (video_shader_get_hdr_mode(shader) >= 0)
               {
                  passEmitsHDR10 = video_shader_get_hdr_mode(shader) == 1;
                  passEmitsHDR16 = video_shader_get_hdr_mode(shader) == 2;
               }

               /* Method-scope flags — will be pushed to Context after the loop. */
               _shaderEmitsHDR10 = passEmitsHDR10;
               _shaderEmitsHDR16 = passEmitsHDR16;

               if (_hdrEnabled)
               {
                  /* Match RT format picked in _updateRenderTargets. */
                  if (passEmitsHDR10)
                     fmt = MTLPixelFormatRGB10A2Unorm;
                  else
                     fmt = MTLPixelFormatRGBA16Float;
               }
            }
            ca.pixelFormat = fmt;

            /* TODO/FIXME (sgc): confirm we never need blending for render passes */
            ca.blendingEnabled             = NO;
            ca.sourceAlphaBlendFactor      = MTLBlendFactorSourceAlpha;
            ca.sourceRGBBlendFactor        = MTLBlendFactorSourceAlpha;
            ca.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            ca.destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;

            psd.sampleCount                = 1;
            psd.vertexDescriptor           = vd;

            id<MTLLibrary>             lib = RARCH_AUTORELEASE_R([_context.device newLibraryWithSource:vs_src options:nil error:&err]);
            if (err != nil)
            {
               if (lib == nil)
               {
                  save_msl = true;
                  RARCH_ERR("[Metal] Unable to compile vertex shader: %s.\n", err.localizedDescription.UTF8String);
                  return NO;
               }
#if DEBUG
               RARCH_WARN("[Metal] Warnings compiling vertex shader: %s.\n", err.localizedDescription.UTF8String);
#endif
            }

            psd.vertexFunction = RARCH_AUTORELEASE_R([lib newFunctionWithName:@"main0"]);

            lib = RARCH_AUTORELEASE_R([_context.device newLibraryWithSource:fs_src options:nil error:&err]);
            if (err != nil)
            {
               if (lib == nil)
               {
                  save_msl = true;
                  RARCH_ERR("[Metal] Unable to compile fragment shader: %s.\n", err.localizedDescription.UTF8String);
                  return NO;
               }
#if DEBUG
               RARCH_WARN("[Metal] Warnings compiling fragment shader: %s.\n", err.localizedDescription.UTF8String);
#endif
            }
            psd.fragmentFunction = RARCH_AUTORELEASE_R([lib newFunctionWithName:@"main0"]);

            RARCH_STRUCT_ASSIGN(_engine.pass[i]._state,
                          RARCH_AUTORELEASE_R([_context.device newRenderPipelineStateWithDescriptor:psd error:&err]));
            if (err != nil)
            {
               save_msl = true;
               RARCH_ERR("[Metal] Error creating pipeline state for pass %d: %s.\n", i,
                         err.localizedDescription.UTF8String);
               return NO;
            }

            for (unsigned j = 0; j < SLANG_CBUFFER_MAX; j++)
            {
               unsigned int size = _engine.pass[i].semantics.cbuffers[j].size;
               if (size == 0)
                  continue;

                id<MTLBuffer> buf = RARCH_AUTORELEASE_R([_context.device newBufferWithLength:size options:PLATFORM_METAL_RESOURCE_STORAGE_MODE]);
               RARCH_STRUCT_ASSIGN(_engine.pass[i].buffers[j], buf);
            }
         } @finally
         {
            if (save_msl)
            {
               NSError *err = nil;
               NSString *basePath = [[NSString stringWithUTF8String:shader->pass[i].source.path] stringByDeletingPathExtension];

               /* Saving Metal shader files... */

               [vs_src writeToFile:[basePath stringByAppendingPathExtension:@"vs.metal"]
                        atomically:NO
                          encoding:NSStringEncodingConversionAllowLossy
                             error:&err];
               if (err != nil)
               {
                  RARCH_ERR("[Metal] Unable to save vertex shader source: %s.\n", err.localizedDescription.UTF8String);
               }

               err = nil;
               [fs_src writeToFile:[basePath stringByAppendingPathExtension:@"fs.metal"]
                        atomically:NO
                          encoding:NSStringEncodingConversionAllowLossy
                             error:&err];
               if (err != nil)
               {
                  RARCH_ERR("[Metal] Unable to save fragment shader source: %s.\n",
                            err.localizedDescription.UTF8String);
               }
            }

            free(shader->pass[i].source.string.vertex);
            free(shader->pass[i].source.string.fragment);

            shader->pass[i].source.string.vertex   = NULL;
            shader->pass[i].source.string.fragment = NULL;
         }
      }
      
      id<MTLBlitCommandEncoder> bce = [_context.blitCommandBuffer blitCommandEncoder];

      /* The LUTs some pass samples or takes the size of: one no pass
       * reads is never decoded */
      memset(lut_read, 0, sizeof(lut_read));
      for (i = 0; i < shader->passes; i++)
         slang_semantics_luts_read(&_engine.pass[i].semantics,
               &_engine.luts[0].view, &_engine.luts[0].size_data,
               sizeof(*_engine.luts), shader->luts, lut_read);

      for (i = 0; i < shader->luts; i++)
      {
         struct texture_image image;
         if (!lut_read[i])
            continue;
         memset(&image, 0, sizeof(image));
         image.supports_rgba        = true;

         if (!image_texture_load(&image, shader->lut[i].path))
            return NO;
         
         bool mipmapped = shader->lut[i].mipmap;

         MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
             width:image.width height:image.height mipmapped:(mipmapped ? YES : NO)];
         
         td.usage                 = MTLTextureUsageShaderRead;
         
         if (mipmapped)
            td.mipmapLevelCount = [self getMipLevels:image.width height:image.height];
         
         [self _initTexture:&_engine.luts[i] withDescriptor:td];

         [_engine.luts[i].view replaceRegion:MTLRegionMake2D(0, 0, image.width, image.height)
                                 mipmapLevel:0 withBytes:image.pixels
                                 bytesPerRow:4 * image.width];

         if (mipmapped)
            [bce generateMipmapsForTexture:_engine.luts[i].view];
         
         image_texture_free(&image);
      }
      [bce endEncoding];
      [_context.commandBuffer commit];
      [_context.commandBuffer waitUntilCompleted];
      
      _shader = shader;
      shader = nil;
   }
   @finally
   {
      if (shader)
         [self _freeVideoShader:shader];
   }

   /* Tell Context what the shader chain's final output looks like.
    * This must happen regardless of _hdrEnabled because Context's
    * composite uniform setup branches on both the driver mode and the
    * shader-emit state.  When HDR is off, this is a no-op. */
   [_context setHDRShaderEmitsHDR10:_shaderEmitsHDR10
                        emitsHDR16:_shaderEmitsHDR16];

   resize_render_targets = YES;
   init_history          = YES;

   return YES;
}

- (void)clearShader
{
   [self _freeVideoShader:_shader];
   _shader = nil;
   _shaderEmitsHDR10 = NO;
   _shaderEmitsHDR16 = NO;
   [_context setHDRShaderEmitsHDR10:NO emitsHDR16:NO];
   RARCH_LOG("[Metal] Shader cleared, using stock.\n");
}

@end

@implementation Overlay
{
   Context *_context;
   NSMutableArray<id<MTLTexture>> *_images;
   id<MTLBuffer> _vert;
   bool _vertDirty;
}

- (instancetype)initWithContext:(Context *)context
{
   if (self = [super init])
      _context = RARCH_RETAIN(context);
   return self;
}

#if !__has_feature(objc_arc)
- (void)dealloc
{
   [_context release];
   [_images release];
   [(id)_vert release];
   [super dealloc];
}
#endif

- (bool)loadImages:(const struct texture_image *)images count:(NSUInteger)count
{
   size_t i;
   [self _freeImages];

   /* Factory result is autoreleased -- must be retained into the ivar
    * or an MRC build reads a dead array on the next frame. */
   RARCH_ASSIGN(_images, [NSMutableArray arrayWithCapacity:count]);

   NSUInteger needed = sizeof(SpriteVertex) * count * 4;
   if (!_vert || _vert.length < needed)
      RARCH_ASSIGN(_vert, RARCH_AUTORELEASE_R([_context.device newBufferWithLength:needed options:PLATFORM_METAL_RESOURCE_STORAGE_MODE]));

   for (i = 0; i < count; i++)
   {
      /* nil into an NSMutableArray is an exception, not an empty
       * slot: a texture that could not be made ends the page here,
       * with the images before it and nothing after. */
      id<MTLTexture> tex = RARCH_AUTORELEASE_R([_context newTexture:images[i] mipmapped:NO]);
      if (!tex)
         return NO;
      _images[i] = tex;
      [self updateVertexX:0 y:0 w:1 h:1 index:i];
      [self updateTextureCoordsX:0 y:0 w:1 h:1 index:i];
      [self _updateColor:RGBA16_WHITE index:i];
   }

   _vertDirty = YES;

   return YES;
}

- (bool)loadTextures:(const uintptr_t *)textures count:(NSUInteger)count
{
   size_t i;
   [self _freeImages];

   RARCH_ASSIGN(_images, [NSMutableArray arrayWithCapacity:count]);

   NSUInteger needed = sizeof(SpriteVertex) * count * 4;
   if (!_vert || _vert.length < needed)
      RARCH_ASSIGN(_vert, RARCH_AUTORELEASE_R([_context.device newBufferWithLength:needed options:PLATFORM_METAL_RESOURCE_STORAGE_MODE]));

   for (i = 0; i < count; i++)
   {
      Texture *t = (__bridge Texture *)(void *)textures[i];
      if (!t.texture)
         return NO;
      _images[i] = t.texture;
      [self updateVertexX:0 y:0 w:1 h:1 index:i];
      [self updateTextureCoordsX:0 y:0 w:1 h:1 index:i];
      [self _updateColor:RGBA16_WHITE index:i];
   }

   _vertDirty = YES;

   return YES;
}

- (void)drawWithEncoder:(id<MTLRenderCommandEncoder>)rce
{
   size_t i;
   NSUInteger count;
#if !defined(HAVE_COCOATOUCH)
   if (_vertDirty)
   {
      [_vert didModifyRange:NSMakeRange(0, _vert.length)];
      _vertDirty = NO;
   }
#endif

   count = _images.count;
   for (i = 0; i < count; ++i)
   {
      NSUInteger offset = sizeof(SpriteVertex) * 4 * i;
      [rce setVertexBuffer:_vert offset:offset atIndex:BufferIndexPositions];
      [rce setFragmentTexture:_images[i] atIndex:TextureIndexColor];
      [rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
   }
}

/* The four vertices of sprite @index, or NULL when the buffer has no
 * such sprite: no page loaded, a buffer that could not be made, an
 * index off the end. The setters are called whenever the frontend
 * likes, not only after a load that worked. */
- (SpriteVertex *)_getForIndex:(NSUInteger)index
{
   SpriteVertex *pv = (SpriteVertex *)_vert.contents;
   if (!pv || (index + 1) * 4 * sizeof(SpriteVertex) > _vert.length)
      return NULL;
   return &pv[index * 4];
}

- (void)_updateColor:(uint64_t)color index:(NSUInteger)index
{
   SpriteVertex *pv  = [self _getForIndex:index];
   if (!pv)
      return;
   pv[0].color       = color;
   pv[1].color       = color;
   pv[2].color       = color;
   pv[3].color       = color;
   _vertDirty        = YES;
}

- (void)updateAlpha:(float)alpha index:(NSUInteger)index
{
   [self _updateColor:rgba16_white(alpha) index:index];
}

- (void)updateVertexX:(float)x y:(float)y w:(float)w h:(float)h index:(NSUInteger)index
{
   SpriteVertex *pv = [self _getForIndex:index];
   if (!pv)
      return;
   pv[0].position   = simd_make_float2(x, y);
   pv[1].position   = simd_make_float2(x + w, y);
   pv[2].position   = simd_make_float2(x, y + h);
   pv[3].position   = simd_make_float2(x + w, y + h);
   _vertDirty       = YES;
}

- (void)updateTextureCoordsX:(float)x y:(float)y w:(float)w h:(float)h index:(NSUInteger)index
{
   SpriteVertex *pv = [self _getForIndex:index];
   if (!pv)
      return;
   pv[0].texCoord   = simd_make_float2(x, y);
   pv[1].texCoord   = simd_make_float2(x + w, y);
   pv[2].texCoord   = simd_make_float2(x, y + h);
   pv[3].texCoord   = simd_make_float2(x + w, y + h);
   _vertDirty       = YES;
}

- (void)_freeImages { RARCH_RELEASE_NIL(_images); }

@end

static MTLPixelFormat glslang_format_to_metal(glslang_format fmt)
{
#undef FMT2
#define FMT2(x, y) case SLANG_FORMAT_##x: return MTLPixelFormat##y

   switch (fmt)
   {
      FMT2(R8_UNORM, R8Unorm);
      FMT2(R8_SINT, R8Sint);
      FMT2(R8_UINT, R8Uint);
      FMT2(R8G8_UNORM, RG8Unorm);
      FMT2(R8G8_SINT, RG8Sint);
      FMT2(R8G8_UINT, RG8Uint);
      FMT2(R8G8B8A8_UNORM, RGBA8Unorm);
      FMT2(R8G8B8A8_SINT, RGBA8Sint);
      FMT2(R8G8B8A8_UINT, RGBA8Uint);
      FMT2(R8G8B8A8_SRGB, RGBA8Unorm_sRGB);

      FMT2(A2B10G10R10_UNORM_PACK32, RGB10A2Unorm);
      FMT2(A2B10G10R10_UINT_PACK32, RGB10A2Uint);

      FMT2(R16_UINT, R16Uint);
      FMT2(R16_SINT, R16Sint);
      FMT2(R16_SFLOAT, R16Float);
      FMT2(R16G16_UINT, RG16Uint);
      FMT2(R16G16_SINT, RG16Sint);
      FMT2(R16G16_SFLOAT, RG16Float);
      FMT2(R16G16B16A16_UINT, RGBA16Uint);
      FMT2(R16G16B16A16_SINT, RGBA16Sint);
      FMT2(R16G16B16A16_SFLOAT, RGBA16Float);

      FMT2(R32_UINT, R32Uint);
      FMT2(R32_SINT, R32Sint);
      FMT2(R32_SFLOAT, R32Float);
      FMT2(R32G32_UINT, RG32Uint);
      FMT2(R32G32_SINT, RG32Sint);
      FMT2(R32G32_SFLOAT, RG32Float);
      FMT2(R32G32B32A32_UINT, RGBA32Uint);
      FMT2(R32G32B32A32_SINT, RGBA32Sint);
      FMT2(R32G32B32A32_SFLOAT, RGBA32Float);

      case SLANG_FORMAT_UNKNOWN:
      default:
         break;
   }
#undef FMT2
   return MTLPixelFormatInvalid;
}

static MTLPixelFormat SelectOptimalPixelFormat(MTLPixelFormat fmt)
{
   switch (fmt)
   {
      case MTLPixelFormatRGBA8Unorm:
         return MTLPixelFormatBGRA8Unorm;

      case MTLPixelFormatRGBA8Unorm_sRGB:
         return MTLPixelFormatBGRA8Unorm_sRGB;

      default:
         break;
   }

   return fmt;
}

static uint32_t metal_get_flags(void *data);

#pragma mark Graphics Context for Metal

/* Metal context data for swap_buffers */
static void *metal_ctx_data = NULL;

static void metal_ctx_swap_buffers(void *data)
{
   @autoreleasepool
   {
      MetalDriver *md = (__bridge MetalDriver *)metal_ctx_data;
      if (md)
         [md.context swapBuffers];
   }
}

static bool metal_set_shader(void *data,
      enum rarch_shader_type type, const char *path);

typedef struct
{
   const video_info_t *video;
   void *result;
} metal_init_args_t;

/* Defined in ui/drivers/cocoa/cocoa_common.m.  Declared locally (same
 * pattern as the cocoa ctx drivers) to avoid touching the
 * CRLF-formatted cocoa_common.h. */
void cocoa_main_thread_sync(void (*func)(void *userdata), void *userdata);

/* The whole init is main-thread work: setViewType replaces the
 * window's contentView (with threaded video, calling it from the
 * worker raises NSInternalInconsistencyException inside
 * -[NSWindow setContentView:] and aborts), and MetalDriver's
 * -initWithVideo: attaches to the render view (view.device /
 * view.delegate / view.frame) and drives [apple_platform
 * setVideoMode:].  metal_frame needs no such treatment: it presents
 * via CAMetalLayer.nextDrawable, which is the sanctioned off-main
 * path. */
static void metal_init_mainthread(void *userdata)
{
   metal_init_args_t *args = (metal_init_args_t*)userdata;
   MetalDriver *md         = nil;

   [apple_platform setViewType:APPLE_VIEW_TYPE_METAL];

   md = [[MetalDriver alloc] initWithVideo:args->video];
   if (md == nil)
   {
      args->result = NULL;
      return;
   }

   /* Store reference for context swap_buffers calls */
   metal_ctx_data = (__bridge void *)md;
#ifdef METAL_HEVC_DRIVER
   metal_hevc_start_supported((__bridge void*)md, (__bridge void*)md.context.device,
         md.context.frameSDRPipeline, md.context.movieHDRSupported);
#endif

   args->result   = RARCH_BRIDGE_RETAINED(RARCH_AUTORELEASE_R(md));
}

static void *metal_init(
      const video_info_t *video)
{
   metal_init_args_t args;

   args.video      = video;
   args.result     = NULL;

   cocoa_main_thread_sync(metal_init_mainthread, &args);

   return args.result;
}

/* Flag to prevent recursive shader_subframes calls */
static bool metal_subframe_lock = false;

/* Flag and value for swap_interval emulation (like Vulkan) */
static bool metal_swap_interval_lock = false;
static unsigned metal_swap_interval = 1;

static bool metal_frame(void *data, const void *frame,
      unsigned dims,
      uint64_t frame_count,
      unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   unsigned frame_width = VIDEO_SCALE_W(dims);
   unsigned frame_height = VIDEO_SCALE_H(dims);
   int j;
   MetalDriver *md = (__bridge MetalDriver *)data;

   if (![md renderFrame:frame
                   data:data
                  width:frame_width
                 height:frame_height
             frameCount:frame_count
                  pitch:pitch
                    msg:msg
                   info:video_info])
      return false;

   /* Call swap_buffers to acquire next drawable. This moves the blocking
    * acquisition to AFTER presenting (like Vulkan), instead of BEFORE
    * rendering. This is critical for proper 120Hz on ProMotion displays.
    * Under the threaded display pacer the push is already timed to the
    * display, so the next drawable is taken when the next frame draws:
    * held from here it would sit idle for a period, leave one drawable
    * on screen and one queued, and block this thread until the next
    * vblank on every frame - which the pacer reads as a frame queued
    * behind another. */
   if (video_info->threaded_display_pacing)
   {
      @autoreleasepool
      {
         [md.context releaseDrawable];
      }
   }
   else
      metal_ctx_swap_buffers(NULL);

   /* Frame duping for shader_subframes - present multiple times per core frame
    * to match high refresh rate displays (e.g., 60fps core on 120Hz display).
    * This follows the same pattern as the Vulkan driver. */
   if (      (video_info->shader_subframes > 1)
         &&  !metal_subframe_lock
         &&  !video_info->input_driver_nonblock_state
         &&  !video_info->runloop_is_slowmotion
         &&  !video_info->runloop_is_paused
         &&  !(video_info->menu_st_flags & MENU_ST_FLAG_ALIVE))
   {
      metal_subframe_lock = true;
      for (j = 1; j < (int)video_info->shader_subframes; j++)
      {
         /* Re-render and present with NULL frame data (reuse previous
          * frame); the index tells the shader which sub-frame this is */
         video_info->current_subframe = (unsigned)j;
         if (!metal_frame(data, NULL, 0, frame_count, 0, msg, video_info))
         {
            video_info->current_subframe = 0;
            metal_subframe_lock = false;
            return false;
         }
      }
      video_info->current_subframe = 0;
      metal_subframe_lock = false;
   }

   /* Metal doesn't directly support swap_interval > 1, so we fake it by
    * duplicating frames. When display runs at 120Hz but core runs at 60fps,
    * swap_interval = 2, meaning we present each frame twice to fill the gap.
    * This matches Vulkan's behavior in vulkan.c:vulkan_frame(). */
   if (      (metal_swap_interval > 1)
         &&  !(video_info->shader_subframes > 1)
         &&  !video_info->black_frame_insertion
         &&  !metal_swap_interval_lock
         &&  !video_info->input_driver_nonblock_state
         &&  !video_info->runloop_is_slowmotion
         &&  !video_info->runloop_is_paused
         &&  !(video_info->menu_st_flags & MENU_ST_FLAG_ALIVE))
   {
      metal_swap_interval_lock = true;
      for (j = 1; j < (int)metal_swap_interval; j++)
      {
         if (!metal_frame(data, NULL, 0, frame_count, 0, msg, video_info))
         {
            metal_swap_interval_lock = false;
            return false;
         }
      }
      metal_swap_interval_lock = false;
   }

   return true;
}

static void metal_set_nonblock_state(void *data, bool non_block,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   md.context.displaySyncEnabled = !non_block;
   /* Presenting a frame again only holds it for another display frame
    * while presents wait on the display; without that it is a second,
    * unpaced present. */
   metal_swap_interval = non_block ? 1 : swap_interval;
}

static bool metal_alive(void *data) { return true; }
static bool metal_has_windowed(void *data) { return true; }
/* apple_platform.hasFocus is [NSApp isActive] (AppKit, main-thread-
 * only); with threaded video this is queried from the worker every
 * frame.  cocoa_has_focus() reads the value the main thread last
 * published, lock-free. */
static bool metal_focus(void *data) { return cocoa_has_focus(NULL); }

static bool metal_suppress_screensaver(void *data, bool disable)
{
   return [apple_platform setDisableDisplaySleep:disable];
}

static bool metal_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
#if defined(HAVE_SLANG) && (defined(HAVE_SPIRV_CROSS) || defined(HAVE_RSLC))
   @autoreleasepool
   {
      MetalDriver *md = (__bridge MetalDriver *)data;
      if (md)
      {
         if (type != RARCH_SHADER_SLANG)
         {
            if (path && *path && type != RARCH_SHADER_SLANG)
               RARCH_WARN("[Metal] Only Slang shaders are supported. Falling back to stock.\n");
            path = NULL;
         }

         /* The views' chains read the preset */
         metal_views_free_chains(md.views, 0);

         if (!path || !*path)
         {
            [md.frameView clearShader];
            return true;
         }

         if ([md.frameView setShaderFromPath:[NSString stringWithUTF8String:path]])
            return true;
      }
   }
#endif
   return false;
}

static void metal_free(void *data)
{
   @autoreleasepool
   {
      __attribute__((unused)) MetalDriver *md = RARCH_BRIDGE_TRANSFER(MetalDriver *, data);
#ifdef METAL_HEVC_DRIVER
      metal_hevc_stop((__bridge void*)md);
#endif
      metal_ctx_data = NULL;
      md = nil;
   }
}

static void metal_set_viewport(void *data, unsigned dims,
      bool force_full, bool allow_rotate)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
      [md setViewportDims:dims forceFull:force_full allowRotate:allow_rotate];
}

static void metal_set_rotation(void *data, unsigned rotation)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
      [md setRotation:rotation];
}

static void metal_viewport_info(void *data, struct video_viewport *vp)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   *vp = *md.viewport;
}

static bool metal_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   @autoreleasepool
   {
      MetalDriver *md = (__bridge MetalDriver *)data;
      return [md.frameView readViewport:buffer isIdle:is_idle];
   }
}

static bool metal_read_viewport_hdr(void *data, uint16_t *buffer,
      bool is_idle, struct rpng_hdr_metadata *out_meta)
{
   @autoreleasepool
   {
      MetalDriver *md = (__bridge MetalDriver *)data;
      return [md.frameView readViewportHDR:buffer isIdle:is_idle meta:out_meta];
   }
}

#ifdef HAVE_THREADS
typedef struct
{
   void                         *video_data;  /* unretained MetalDriver * */
   struct texture_image         *image;
   enum texture_filter_type      filter_type;
   uintptr_t                     handle;
} metal_texture_cmd_t;
#endif

/* Inner load function -- performs the actual texture creation.
 * Must run on the same thread that owns the Metal context's
 * blit command buffer (the video thread when threaded video is
 * active, otherwise the main thread).
 *
 * Metal's blit command buffer is lazily created on first use
 * and committed during frame end.  It is stored on the shared
 * Context instance, not thread-local.  Mipmapped texture loads
 * encode mipmap-generation commands into this buffer via a
 * BlitCommandEncoder -- and MTLCommandBuffer / MTLCommandEncoder
 * are explicitly documented as single-thread-only.  Concurrent
 * access from the main thread (mid-load) and the video thread
 * (mid-frame-end commit) is undefined behaviour. */
static enum video_texture_update metal_planar_update(MetalDriver *md,
      Texture *t, const struct texture_image *ti);

static uintptr_t metal_load_texture_internal(void *video_data, void *data,
      enum texture_filter_type filter_type)
{
   MetalDriver           *md  = (__bridge MetalDriver *)video_data;
   struct texture_image *img  = (struct texture_image *)data;
   if (!img)
      return 0;

   @autoreleasepool
   {
      struct texture_image image = *img;
      Texture *t;
      if (img->planar)
      {
         if (!(t = RARCH_AUTORELEASE_R([md.context newPlanarTexture:img
                     filter:filter_type]))
               || metal_planar_update(md, t, img) != VIDEO_TEXTURE_UPDATE_DONE)
            return 0;
         return (uintptr_t)RARCH_BRIDGE_RETAINED(t);
      }
      t = [md.context newTexture:image filter:filter_type];
      return (uintptr_t)RARCH_BRIDGE_RETAINED(RARCH_AUTORELEASE_R(t));
   }
}

#ifdef HAVE_THREADS
/* Wrap function invoked on the video thread via
 * CMD_CUSTOM_COMMAND.  Writes the handle to cmd->handle rather
 * than the int return channel to avoid truncation on 64-bit
 * Apple platforms where uintptr_t is 64 bits. */
static uintptr_t metal_texture_load_wrap(void *data)
{
   metal_texture_cmd_t *cmd = (metal_texture_cmd_t*)data;
   cmd->handle = metal_load_texture_internal(
         cmd->video_data, cmd->image, cmd->filter_type);
   return 0;
}
#endif

static uintptr_t metal_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
#ifdef HAVE_THREADS
   /* When threaded video is active, mipmapped loads must run on the
    * video thread: they encode mipmap generation into the shared
    * Context.blitCommandBuffer, and MTLCommandBuffer / encoders are
    * single-thread-only, so the encode has to be serialised with the
    * video thread's frame-end commit.
    *
    * Non-mipmapped loads never touch the shared command buffer --
    * they only call MTLDevice newTextureWithDescriptor: (MTLDevice is
    * documented thread-safe), replaceRegion: on the freshly created
    * texture, and read the immutable sampler table -- so they run
    * inline.  Menu icon and thumbnail loads are all non-mipmapped;
    * routing each of them through a video-thread round-trip was a
    * per-icon latency tax on menu population. */
   /* A planar frame is converted on the blit command buffer: there too */
   if (      threaded
         && (   filter_type == TEXTURE_FILTER_MIPMAP_LINEAR
             || filter_type == TEXTURE_FILTER_MIPMAP_NEAREST
             || (data && ((struct texture_image *)data)->planar)))
   {
      metal_texture_cmd_t cmd;
      cmd.video_data  = video_data;
      cmd.image       = (struct texture_image *)data;
      cmd.filter_type = filter_type;
      cmd.handle      = 0;
      video_thread_texture_handle(&cmd, metal_texture_load_wrap);
      return cmd.handle;
   }
#endif

   return metal_load_texture_internal(video_data, data, filter_type);
}

static void metal_unload_texture(void *data,
      bool threaded, uintptr_t handle)
{
   if (!handle)
      return;
   /* Metal command buffers retain their referenced resources,
    * so releasing the handle here does not free the
    * underlying MTLTexture if the video thread's command
    * buffer is still using it -- the Metal runtime refcounts
    * resources across CPU and GPU.  No cross-thread
    * serialisation needed for unload. */
   @autoreleasepool
   {
      __attribute__((unused)) Texture *t = RARCH_BRIDGE_TRANSFER(Texture *, handle);
      t = nil;
   }
}

/* Same-size contents into a texture metal_load_texture made, the way
 * the menu frame and an animated preview stream into the one texture
 * every frame.
 *
 * The pixels go through a staging buffer and a GPU blit rather than
 * replaceRegion:. replaceRegion: writes from the CPU the moment it is
 * called, while the command buffers already committed may still be
 * sampling that texture - a command buffer retains what it samples,
 * which keeps the object alive but says nothing about its contents.
 * The other backends each refuse that: Vulkan checks its fences,
 * D3D11 maps with DO_NOT_WAIT, D3D12 compares a fence tag, and each
 * drops the frame. A blit on the shared blit command buffer needs no
 * such rule - it executes in commit order with the draws around it,
 * and Metal's hazard tracking orders the write against the reads - so
 * the frame is neither raced nor dropped.
 *
 * A mipmapped texture would need its levels regenerated after the
 * copy: refused, so the caller loads a replacement.
 *
 * Must run on the thread that owns Context.blitCommandBuffer (see
 * metal_load_texture_internal): metal_update_texture routes it to
 * the video thread when threaded video is up. */
/* The texture's staging buffer, METAL_STAGING_SLOTS slots of @len
 * bytes, made on first use and kept: a texture streams at one size,
 * and lent slots point into it. */
static bool metal_texture_staging(Texture *t, NSUInteger len)
{
   id<MTLBuffer> buf;
   if (t.staging.length >= len * METAL_STAGING_SLOTS)
      return true;
   /* Grown under a lent slot would leave the producer the old one */
   if (t->_stagingLent)
      return false;
   if (!(buf = [t.texture.device newBufferWithLength:len * METAL_STAGING_SLOTS
               options:PLATFORM_METAL_RESOURCE_STORAGE_MODE]))
      return false;
   t.staging = RARCH_AUTORELEASE_R(buf);
   return true;
}

/* A planar frame into planar texture @t: its planes copied once into a
 * staging slot no conversion is still reading, then converted there by
 * the planar kernel straight into the texture, on the blit command
 * buffer, in order with the draws around it. A slot still the GPU's
 * drops the frame, as an RGBA update does. Must run on the thread that
 * owns Context.blitCommandBuffer. */
static enum video_texture_update metal_planar_update(MetalDriver *md,
      Texture *t, const struct texture_image *ti)
{
   const struct texture_planar *tp = ti->planar;
   id<MTLTexture> tex = t.texture;
   id<MTLComputePipelineState> kernel;
   id<MTLCommandBuffer> cb;
   id<MTLComputeCommandEncoder> ce;
   struct
   {
      uint32_t w, h, y_off, y_stride, cb_off, cr_off, c_stride, c_step;
      float k[8];
   } params;
   NSUInteger w  = ti->width;
   NSUInteger h  = ti->height;
   NSUInteger cw = (w + 1) / 2;
   NSUInteger ch = (h + 1) / 2;
   NSUInteger len, off, y, x, tw;
   unsigned slot, k;
   uint8_t *dst;

   if (     !tp || !t->_planar || !tex
         || !tp->planes[0] || !tp->planes[1] || !tp->planes[2]
         || (tp->chroma_step != 1 && tp->chroma_step != 2)
         || tex.width != w || tex.height != h
         || !(kernel = [md.context planarKernel]))
      return VIDEO_TEXTURE_UPDATE_REFUSED;
   /* Luma, then Cb and Cr planes, tightly packed; kept a multiple of
    * four so each slot starts aligned */
   len = (w * h + 2 * cw * ch + 3) & ~(NSUInteger)3;
   if (!metal_texture_staging(t, len))
      return VIDEO_TEXTURE_UPDATE_REFUSED;
   for (k = 0; k < METAL_STAGING_SLOTS; k++)
   {
      slot = (t->_stagingNext + k) % METAL_STAGING_SLOTS;
      if (!__atomic_load_n(&t->_stagingBusy[slot], __ATOMIC_ACQUIRE))
         break;
   }
   if (k == METAL_STAGING_SLOTS)
      return VIDEO_TEXTURE_UPDATE_DROPPED;
   if (!(cb = md.context.blitCommandBuffer))
      return VIDEO_TEXTURE_UPDATE_REFUSED;
   t->_stagingNext = (slot + 1) % METAL_STAGING_SLOTS;
   off = (NSUInteger)slot * len;
   dst = (uint8_t *)t.staging.contents + off;
   for (y = 0; y < h; y++)
      memcpy(dst + y * w, tp->planes[0] + y * tp->strides[0], w);
   for (y = 0; y < ch; y++)
   {
      const uint8_t *cbp = tp->planes[1] + y * tp->strides[1];
      const uint8_t *crp = tp->planes[2] + y * tp->strides[2];
      uint8_t *ob        = dst + w * h + y * cw;
      uint8_t *oc        = dst + w * h + cw * ch + y * cw;
      if (tp->chroma_step == 1)
      {
         memcpy(ob, cbp, cw);
         memcpy(oc, crp, cw);
      }
      else
         for (x = 0; x < cw; x++)
         {
            ob[x] = cbp[x * 2];
            oc[x] = crp[x * 2];
         }
   }
#if TARGET_OS_OSX
   if (t.staging.storageMode == MTLStorageModeManaged)
      [t.staging didModifyRange:NSMakeRange(off, len)];
#endif
   params.w        = (uint32_t)w;
   params.h        = (uint32_t)h;
   params.y_off    = 0;
   params.y_stride = (uint32_t)w;
   params.cb_off   = (uint32_t)(w * h);
   params.cr_off   = (uint32_t)(w * h + cw * ch);
   params.c_stride = (uint32_t)cw;
   params.c_step   = 1;
   image_yuv_coefficients(tp->yuv, params.k);
   params.k[6]     = 0.0f;
   params.k[7]     = 0.0f;

   ce = [cb computeCommandEncoder];
   ce.label = @"planar conversion";
   [ce setComputePipelineState:kernel];
   [ce setBuffer:t.staging offset:off atIndex:0];
   [ce setBytes:&params length:sizeof(params) atIndex:1];
   [ce setTexture:tex atIndex:0];
   tw = kernel.threadExecutionWidth;
   if (!tw)
      tw = 8;
   {
      NSUInteger th = kernel.maxTotalThreadsPerThreadgroup / tw;
      MTLSize size, count;
      if (th > 8)
         th = 8;
      if (!th)
         th = 1;
      size  = MTLSizeMake(tw, th, 1);
      count = MTLSizeMake((w + tw - 1) / tw, (h + th - 1) / th, 1);
      [ce dispatchThreadgroups:count threadsPerThreadgroup:size];
   }
   [ce endEncoding];
   __atomic_store_n(&t->_stagingBusy[slot], 1, __ATOMIC_RELEASE);
   {
      Texture *held = t;
      [cb addCompletedHandler:^(id<MTLCommandBuffer> done) {
         (void)done;
         __atomic_store_n(&held->_stagingBusy[slot], 0, __ATOMIC_RELEASE);
      }];
   }
   return VIDEO_TEXTURE_UPDATE_DONE;
}

static enum video_texture_update metal_update_texture_internal(
      void *video_data, uintptr_t handle, const struct texture_image *ti)
{
   MetalDriver *md = (__bridge MetalDriver *)video_data;
   if (md && handle && ti && ti->planar)
   {
      @autoreleasepool
      {
         return metal_planar_update(md,
               (__bridge Texture *)(void *)handle, ti);
      }
   }
   if (!md || !handle || !ti || !ti->pixels)
      return VIDEO_TEXTURE_UPDATE_REFUSED;

   @autoreleasepool
   {
      Texture *t          = (__bridge Texture *)(void *)handle;
      id<MTLTexture> tex  = t.texture;
      if (     !tex
            || tex.mipmapLevelCount > 1
            || tex.width  != ti->width
            || tex.height != ti->height)
         return VIDEO_TEXTURE_UPDATE_REFUSED;
      {
         /* The texture's own texel size: RGBA16Float half floats are 8 */
         NSUInteger bpp = (tex.pixelFormat == MTLPixelFormatRGBA16Float)
            ? 8 : 4;
         NSUInteger len = (NSUInteger)ti->width * ti->height * bpp;
         unsigned slot  = METAL_STAGING_SLOTS;
         unsigned k;
         NSUInteger off;
         id<MTLCommandBuffer> cb;
         id<MTLBlitCommandEncoder> bce;

         if (!metal_texture_staging(t, len))
            return VIDEO_TEXTURE_UPDATE_REFUSED;
         /* A lent slot holding the frame: the producer wrote it where
          * the blit reads from, once the slot's last blit had run */
         for (k = 0; k < METAL_STAGING_SLOTS; k++)
            if (     (t->_stagingLent & (1u << k))
                  && ti->pixels == (const void*)
                     ((uint8_t *)t.staging.contents + (NSUInteger)k * len))
               slot = k;
         if (slot < METAL_STAGING_SLOTS)
         {
            if (__atomic_load_n(&t->_stagingBusy[slot], __ATOMIC_ACQUIRE))
               /* written early: keep the last frame */
               return VIDEO_TEXTURE_UPDATE_DROPPED;
         }
         else
         {
            /* Otherwise the next slot nobody writes but this path */
            for (k = 0; k < METAL_STAGING_SLOTS; k++)
            {
               slot = (t->_stagingNext + k) % METAL_STAGING_SLOTS;
               if (!(t->_stagingLent & (1u << slot)))
                  break;
            }
            if (k == METAL_STAGING_SLOTS)
               return VIDEO_TEXTURE_UPDATE_DROPPED;
            /* The copy out of this slot from METAL_STAGING_SLOTS
             * frames ago has not finished: the frame is dropped rather
             * than written over the GPU's shoulder, which is the
             * policy the other backends keep. */
            if (__atomic_load_n(&t->_stagingBusy[slot], __ATOMIC_ACQUIRE))
               return VIDEO_TEXTURE_UPDATE_DROPPED;
            t->_stagingNext = (slot + 1) % METAL_STAGING_SLOTS;
         }
         off = (NSUInteger)slot * len;
         if (!(cb = md.context.blitCommandBuffer))
            return VIDEO_TEXTURE_UPDATE_REFUSED;
         if (!(t->_stagingLent & (1u << slot)))
            memcpy((uint8_t *)t.staging.contents + off, ti->pixels, len);
#if TARGET_OS_OSX
         if (t.staging.storageMode == MTLStorageModeManaged)
            [t.staging didModifyRange:NSMakeRange(off, len)];
#endif
         bce = [cb blitCommandEncoder];
         [bce copyFromBuffer:t.staging
                sourceOffset:off
           sourceBytesPerRow:bpp * ti->width
         sourceBytesPerImage:len
                  sourceSize:MTLSizeMake(ti->width, ti->height, 1)
                   toTexture:tex
            destinationSlice:0
            destinationLevel:0
           destinationOrigin:MTLOriginMake(0, 0, 0)];
         [bce endEncoding];
         __atomic_store_n(&t->_stagingBusy[slot], 1, __ATOMIC_RELEASE);
         {
            /* The slot is free again when this command buffer is done
             * with it. The handler holds the Texture so the flag it
             * clears is still there to clear. */
            Texture *held = t;
            [cb addCompletedHandler:^(id<MTLCommandBuffer> done) {
               (void)done;
               __atomic_store_n(&held->_stagingBusy[slot], 0,
                     __ATOMIC_RELEASE);
            }];
         }
      }
   }
   return VIDEO_TEXTURE_UPDATE_DONE;
}

#ifdef HAVE_THREADS
/* Runs on the video thread via CMD_CUSTOM_COMMAND; the outcome comes
 * back as the command's return value. */
static uintptr_t metal_texture_update_wrap(void *data)
{
   metal_texture_cmd_t *cmd = (metal_texture_cmd_t*)data;
   return (uintptr_t)metal_update_texture_internal(cmd->video_data,
         cmd->handle, cmd->image);
}
#endif

static enum video_texture_update metal_update_texture(void *video_data,
      uintptr_t handle, const struct texture_image *ti, bool threaded)
{
   if (!handle || !ti)
      return VIDEO_TEXTURE_UPDATE_REFUSED;

#ifdef HAVE_THREADS
   /* The update encodes a blit into Context.blitCommandBuffer, which
    * the video thread commits at frame end: the same single-thread
    * rule metal_load_texture follows for mipmapped loads, so under
    * threaded video it runs there too. */
   if (threaded)
   {
      metal_texture_cmd_t cmd;
      cmd.video_data  = video_data;
      cmd.image       = (struct texture_image *)ti;
      cmd.filter_type = TEXTURE_FILTER_LINEAR;
      cmd.handle      = handle;
      return (enum video_texture_update)video_thread_texture_handle(
            &cmd, metal_texture_update_wrap);
   }
#endif

   return metal_update_texture_internal(video_data, handle, ti);
}

/* On the driver's own thread (see texture_lend in video_driver.h):
 * the slot's staging memory, where the update's blit reads from */
static void *metal_texture_lend(void *video_data, uintptr_t handle,
      unsigned slot, size_t pitch, size_t *out_pitch)
{
   void *mem = NULL;
   if (!video_data || !handle || slot >= 2)
      return NULL;
   @autoreleasepool
   {
      Texture *t         = (__bridge Texture *)(void *)handle;
      id<MTLTexture> tex = t.texture;
      NSUInteger bpp, len;
      if (!tex || tex.mipmapLevelCount > 1)
         return NULL;
      bpp = (tex.pixelFormat == MTLPixelFormatRGBA16Float) ? 8 : 4;
      len = (NSUInteger)tex.width * tex.height * bpp;
      if (     (NSUInteger)pitch != (NSUInteger)tex.width * bpp
            || !metal_texture_staging(t, len))
         return NULL;
      t->_stagingLent |= 1u << slot;
      mem = (uint8_t *)t.staging.contents + (NSUInteger)slot * len;
   }
   if (mem && out_pitch)
      *out_pitch = pitch;
   return mem;
}

static bool metal_texture_lend_ready(void *video_data, uintptr_t handle,
      unsigned slot)
{
   Texture *t;
   if (!video_data || !handle || slot >= METAL_STAGING_SLOTS)
      return true;
   t = (__bridge Texture *)(void *)handle;
   if (!(t->_stagingLent & (1u << slot)))
      return true;
   return !__atomic_load_n(&t->_stagingBusy[slot], __ATOMIC_ACQUIRE);
}

typedef struct
{
   void    *data;
   unsigned dims;
   bool     fullscreen;
} metal_set_video_mode_args_t;

static void metal_set_video_mode_mainthread(void *userdata)
{
   metal_set_video_mode_args_t *args = (metal_set_video_mode_args_t*)userdata;
   MetalDriver *md                   = (__bridge MetalDriver *)args->data;

   [md applyVideoMode:args->dims fullscreen:args->fullscreen];
   [apple_platform setCursorVisible:!args->fullscreen];
}

static void metal_set_video_mode(void *data, unsigned dims,
      bool fullscreen)
{
   metal_set_video_mode_args_t args;

   if (!data)
      return;

   args.data       = data;
   args.dims       = dims;
   args.fullscreen = fullscreen;

   /* Window and full-screen surgery is AppKit/UIKit; with threaded
    * video this is reached from the worker thread. */
   cocoa_main_thread_sync(metal_set_video_mode_mainthread, &args);
}

static float metal_get_refresh_rate(void *data)
{
   /* Body consolidated into cocoa_common.m.  Kept as a named
    * poke entry because video_driver_get_refresh_rate falls back
    * to poke->get_refresh_rate when dispserv returns 0. */
   return cocoa_get_refresh_rate();
}

static void metal_set_filtering(void *data, unsigned index, bool smooth, bool ctx_scaling)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   [md.frameView setFilteringIndex:index smooth:smooth];
}

static void metal_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   MetalDriver *md = (__bridge MetalDriver *)data;

   md.keepAspect = YES;
   [md setNeedsResize];
}

static void metal_apply_state_changes(void *data)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   [md setNeedsResize];
}

static void metal_set_texture_frame(void *data, const void *frame,
      bool rgb32, unsigned dims,
      float alpha)
{
   MetalDriver *md         = (__bridge MetalDriver *)data;
   bool menu_linear_filter;
   if (!md)
      return;
   /* What the last frame carried, not what the setting says now: the
    * video thread applies this from thread_update_driver_state(). */
   menu_linear_filter      = md.frameMenuLinearFilter;

   [md.menu updateDims:dims
                format:rgb32 ? RPixelFormatBGRA8Unorm : RPixelFormatBGRA4Unorm
                filter:menu_linear_filter ? RTextureFilterLinear : RTextureFilterNearest];
   [md.menu updateFrame:frame];
   md.menu.alpha = alpha;
}

static void metal_set_texture_enable(void *data, bool state, bool full_screen)
{
   MetalDriver *md    = (__bridge MetalDriver *)data;
   if (!md)
      return;

   md.menu.enabled    = state;
#if 0
   md.menu.fullScreen = full_screen;
#endif
}

static void metal_show_mouse_mainthread(void *userdata)
{
   [apple_platform setCursorVisible:(userdata != NULL)];
}

static void metal_show_mouse(void *data, bool state)
{
   /* setCursorVisible is NSCursor (AppKit); with threaded video this
    * can be reached from the worker thread. */
   cocoa_main_thread_sync(metal_show_mouse_mainthread,
         state ? (void*)1 : NULL);
}

static bool metal_get_current_sw_framebuffer(void *data,
      struct retro_framebuffer *framebuffer)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return false;
   return [md.frameView lendFramebuffer:framebuffer];
}

static struct video_shader *metal_get_current_shader(void *data)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return NULL;

   return md.frameView.shader;
}

static uint32_t metal_get_flags(void *data)
{
   uint32_t flags  = 0;
   MetalDriver *md = (__bridge MetalDriver *)data;

   BIT32_SET(flags, GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);
   BIT32_SET(flags, GFX_CTX_FLAGS_SCREEN_10BPC_SOURCE);

#if defined(HAVE_SLANG) && (defined(HAVE_SPIRV_CROSS) || defined(HAVE_RSLC))
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif
   if (md && retro_atomic_load_acquire_int(&md.views->allowed))
      BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS);
   if (md && retro_atomic_load_acquire_int(&md.views->fallback))
      BIT32_SET(flags, GFX_CTX_FLAGS_VIDEO_VIEWS_FALLBACK);

   return flags;
}

static void metal_set_view_count(void *data, unsigned count)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
      metal_views_set_count(md.views, count);
}

static void metal_get_video_output_size(void *data,
      unsigned *dims, char *desc, size_t desc_len)
{
   /* Body consolidated into cocoa_common.m.  Kept as a named
    * poke entry because video_thread_wrapper.c's
    * thread_get_video_output_size calls poke->get_video_output_size
    * directly, bypassing dispserv_apple. */
   cocoa_get_video_output_size(dims, desc, desc_len);
}

/* HDR poke interface setters.
 *
 * These thin wrappers forward to Context.  When METAL_HDR_AVAILABLE is 0
 * the Context methods are no-ops, so the wrappers stay valid on older OSes
 * too (the poke interface entries just don't do anything observable).
 *
 * Paper-white and expand-gamut affect composite output per frame via the
 * shared HDRUniforms buffer.  Menu-nits is currently stored but unused;
 * it's reserved for a future separate-menu-compositor path that matches
 * Vulkan's vk->hdr.menu_nits.  Scanlines and subpixel layout drive the
 * CRT-mask branch in hdr_composite_fragment when the output resolution
 * is tall enough. */
static void metal_set_hdr_menu_nits(void *data, float menu_nits)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
      [md.context setHDRMenuNits:menu_nits];
}

static void metal_set_hdr_paper_white_nits(void *data, float paper_white_nits)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
      [md.context setHDRPaperWhiteNits:paper_white_nits];
}

static void metal_set_hdr_expand_gamut(void *data, unsigned expand_gamut)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
      [md.context setHDRExpandGamut:expand_gamut];
}

static void metal_set_hdr_scanlines(void *data, bool scanlines)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
      [md.context setHDRScanlines:scanlines];
}

static void metal_set_hdr_subpixel_layout(void *data, unsigned subpixel_layout)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
      [md.context setHDRSubpixelLayout:subpixel_layout];
}

static retro_time_t metal_get_last_present_time(void *data)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   return md ? [md.context lastPresentTime] : 0;
}

static bool metal_supports_texture_format(void *video_data,
      enum texture_gpu_format fmt)
{
   /* A pix10 image loads as BGR10A2Unorm and updates as raw 32-bit
    * words into it, on every Apple target. */
   if (fmt == TEXTURE_GPU_FORMAT_RGB10A2)
      return video_data != NULL;
   /* RGBA16Float from half floats loads and updates on every Apple
    * target */
   if (fmt == TEXTURE_GPU_FORMAT_RGBA16F)
      return video_data != NULL;
   /* Converted by the planar kernel, which every Metal device runs */
   if (fmt == TEXTURE_GPU_FORMAT_YUV420)
   {
      MetalDriver *md = (__bridge MetalDriver *)video_data;
      return md && [md.context planarAvailable];
   }
   /* stock_fragment_linear shows such a texture as linear scRGB in the
    * RGBA16Float HDR overlay; in SDR there is no linear light */
   if (fmt == TEXTURE_GPU_FORMAT_SCRGB)
   {
      MetalDriver *mdl = (__bridge MetalDriver *)video_data;
      return mdl && mdl.context.hdrEnabled
         && [mdl.context linearShader] != nil;
   }
#if TARGET_OS_OSX
   MetalDriver  *md = (__bridge MetalDriver *)video_data;
   id<MTLDevice> dev;
   if (!md)
      return false;
   switch (fmt)
   {
      case TEXTURE_GPU_FORMAT_BC1:
      case TEXTURE_GPU_FORMAT_BC2:
      case TEXTURE_GPU_FORMAT_BC3:
      case TEXTURE_GPU_FORMAT_BC7:
         break;
      default:
         return false;
   }
   dev = md.context.device;
   if (apple_runtime_available(APPLE_RUNTIME_VER(11, 0, 0), 0, 0))
      return apple_rt_get_bool(dev,
            sel_registerName("supportsBCTextureCompression")) ? true : false;
   return true; /* BC always available on pre-11 (Intel) Macs */
#else
   (void)video_data;
   (void)fmt;
   return false;
#endif
}

static uintptr_t metal_load_texture_compressed(void *video_data,
      const struct texture_compressed *tc, bool threaded,
      enum texture_filter_type filter_type)
{
#if TARGET_OS_OSX
   @autoreleasepool
   {
      MetalDriver *md = (__bridge MetalDriver *)video_data;
      Texture     *t;
      (void)threaded;
      if (!md || !tc || tc->num_mips == 0)
         return 0;
      t = [md.context newTextureCompressed:tc filter:filter_type];
      if (!t)
         return 0;
      return (uintptr_t)RARCH_BRIDGE_RETAINED(RARCH_AUTORELEASE_R(t));
   }
#else
   (void)video_data;
   (void)tc;
   (void)threaded;
   (void)filter_type;
   return 0;
#endif
}

/* The frame texture is 32-bit whatever the source; a 16-bit source is
 * converted into it from a staging texture. A new format changes only
 * that, and the frames' size is taken per frame already. */
/* The threaded wrapper's frame buffers, on the driver's own thread */
static void *metal_frame_mem_alloc(void *data, size_t size, uintptr_t *ident,
      bool *cached, unsigned *pitch_align)
{
   void *p = NULL;
   @autoreleasepool
   {
      MetalDriver *md = (__bridge MetalDriver *)data;
      if (md)
         p = [md.frameView frameMemAlloc:size id:ident];
   }
   /* Shared storage is host-cached, and a blit reads rows at any pitch
    * on a texel */
   *cached      = true;
   *pitch_align = 1;
   return p;
}

static bool metal_frame_mem_ready(void *data, uintptr_t ident)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   return !md || [md.frameView frameMemReady:ident];
}

static void metal_frame_mem_free(void *data, uintptr_t ident)
{
   @autoreleasepool
   {
      MetalDriver *md = (__bridge MetalDriver *)data;
      if (md)
         [md.frameView frameMemFree:ident];
   }
}

static bool metal_set_frame_format(void *data, bool rgb32,
      unsigned input_scale)
{
   @autoreleasepool
   {
      MetalDriver *md = (__bridge MetalDriver *)data;
      (void)input_scale;
      if (!md || md.frameView.format == RPixelFormatBGR10A2Unorm)
         return false;
      [md.frameView setSourceFormat:rgb32
            ? RPixelFormatBGRX8Unorm : RPixelFormatB5G6R5Unorm];
      return true;
   }
}

/* METAL_FRAME_IMPORT_BEGIN */
static enum video_texture_update metal_import_texture(void *data, uintptr_t *handle,
      const video_frame_import_t *frame, enum texture_filter_type filter)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md || !handle || !frame || (filter != TEXTURE_FILTER_LINEAR
            && filter != TEXTURE_FILTER_NEAREST))
      return VIDEO_TEXTURE_UPDATE_REFUSED;
   @autoreleasepool
   {
      id<MTLDevice> device = md.context.device;
      id<MTLCommandBuffer> buffer = md.context.blitCommandBuffer;
      RArchMetalFrameLease *lease = metal_frame_validate(frame,
            (__bridge void *)device, (__bridge void *)buffer);
      Texture *texture = (__bridge Texture *)(void *)*handle;
      Texture *target = texture;
      bool planar = lease && (lease->view.pixfmt == IMAGE_PIXFMT_NV12
            || lease->view.pixfmt == IMAGE_PIXFMT_P010);
      MTLPixelFormat format;
      bool hdr = false;
      metal_frame_sdr_params_t params;
      if (!lease || (texture && (texture->_planar || texture->_stagingLent)))
         return VIDEO_TEXTURE_UPDATE_REFUSED;
      if (planar)
      {
         hdr = metal_frame_hdr_params(&lease->view, &params);
         if (!md.context.frameSDRPipeline
               || (hdr ? (!md.context.hdrEnabled || ![md.context linearShader])
                  : !metal_frame_sdr_params(&lease->view, &params))
               || (frame->output_pixfmt && frame->output_pixfmt != (hdr ? IMAGE_PIXFMT_FP16 : IMAGE_PIXFMT_8888)))
            return VIDEO_TEXTURE_UPDATE_REFUSED;
         format = hdr ? MTLPixelFormatRGBA16Float : MTLPixelFormatRGBA8Unorm;
      }
      else
      {
         if ((lease->view.pixfmt != IMAGE_PIXFMT_8888
                  && lease->view.pixfmt != IMAGE_PIXFMT_FP16)
               || (frame->output_pixfmt ? frame->output_pixfmt : IMAGE_PIXFMT_8888) != lease->view.pixfmt
               || lease->view.color.primaries != 1
               || lease->view.color.transfer != (lease->view.pixfmt == IMAGE_PIXFMT_FP16 ? 8u : 13u)
               || lease->view.color.matrix || lease->view.color.range != 2
               || (lease->view.pixfmt == IMAGE_PIXFMT_FP16
                  && (!md.context.hdrEnabled || ![md.context linearShader])))
            return VIDEO_TEXTURE_UPDATE_REFUSED;
         format = lease->planes[0].pixelFormat;
      }
      if (!target || target.texture.width != lease->view.width
            || target.texture.height != lease->view.height
            || target.texture.pixelFormat != format)
         target = RARCH_AUTORELEASE_R([md.context newImportedTexture:format
            width:lease->view.width height:lease->view.height filter:filter]);
      if (!target)
         return VIDEO_TEXTURE_UPDATE_REFUSED;
      if (planar ? !(hdr ? metal_frame_convert_hdr(frame, (__bridge void *)device,
               (__bridge void *)buffer, md.context.frameSDRPipeline,
               (__bridge void *)target.texture) : metal_frame_convert_sdr(frame, (__bridge void *)device,
               (__bridge void *)buffer, md.context.frameSDRPipeline,
               (__bridge void *)target.texture))
            : !metal_frame_copy(frame, (__bridge void *)device,
               (__bridge void *)buffer, (__bridge void *)target.texture))
         return VIDEO_TEXTURE_UPDATE_REFUSED;
      if (!texture)
         *handle = (uintptr_t)RARCH_BRIDGE_RETAINED(target);
      else if (target != texture)
      {
         /* Encoded readers retain the old image; keep the public handle. */
         texture.texture = target.texture;
         texture.sampler = target.sampler;
      }
      return VIDEO_TEXTURE_UPDATE_DONE;
   }
}
/* METAL_FRAME_IMPORT_END */

static enum video_texture_update metal_import_movie_frame(void *data,
      const video_frame_import_t *frame)
{
   MetalDriver *md = (__bridge MetalDriver*)data;
   if (!md || !md.frameView) return VIDEO_TEXTURE_UPDATE_REFUSED;
   return [md.frameView importMovieFrame:frame];
}

static bool metal_get_device(void *data, video_device_info_t *info)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   id<MTLDevice> device = md.context.device;
   if (!device || !info)
      return false;
   info->api                    = VIDEO_DEVICE_METAL;
   info->instance               = NULL;
   info->physical_device        = NULL;
   info->device                 = (__bridge void *)device;
   info->queue                  = NULL;
   info->get_instance_proc_addr  = NULL;
   info->queue_family           = 0;
   info->import_formats         = 0;
#ifdef METAL_HEVC_DRIVER
   if (metal_hevc_owns((__bridge void*)md))
      info->import_formats = IMAGE_PIXFMT_NV12 | IMAGE_PIXFMT_P010;
#endif
   return true;
}

static const video_poke_interface_t metal_poke_interface = {
   metal_get_flags,
   metal_load_texture,
   metal_unload_texture,
   metal_set_video_mode,
   metal_get_refresh_rate,
   metal_set_filtering,
   metal_get_video_output_size,
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   metal_set_aspect_ratio,
   metal_apply_state_changes,
   metal_set_texture_frame,
   metal_set_texture_enable,
   font_driver_render_msg,
   metal_show_mouse,
   NULL, /* grab_mouse_toggle */
   metal_get_current_shader,
   metal_get_current_sw_framebuffer,
   NULL, /* get_hw_render_interface */
   metal_set_hdr_menu_nits,
   metal_set_hdr_paper_white_nits,
   metal_set_hdr_expand_gamut,
   metal_set_hdr_scanlines,
   metal_set_hdr_subpixel_layout,
   metal_supports_texture_format,
   metal_load_texture_compressed,
   NULL, /* present_last */
   metal_get_last_present_time,
   NULL, /* hw_ring_install */
   NULL, /* hw_ring_fence_new */
   NULL, /* hw_ring_fence_free */
   NULL, /* hw_ring_fence_signal */
   NULL, /* hw_ring_fence_wait */
   NULL, /* hw_ring_capture */
   NULL, /* hw_ring_present_slot */
   NULL, /* hw_ring_context_new */
   NULL, /* hw_ring_context_free */
   NULL, /* hw_ring_framebuffer */
   metal_update_texture,
   NULL, /* get_swap_interval_cap */
   metal_texture_lend,
   metal_texture_lend_ready,
   NULL, /* get_last_present_wait */
   metal_set_view_count,
   NULL, /* hw_context_destroying */
   NULL, /* get_headset_refresh */
   metal_set_frame_format,
   metal_frame_mem_alloc,
   metal_frame_mem_ready,
   metal_frame_mem_free,
   metal_get_device,
   metal_import_texture,
   NULL, /* get_last_present_sync */
   NULL, /* record_read */
   metal_import_movie_frame
};

static void metal_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   *iface = &metal_poke_interface;
}

#ifdef HAVE_OVERLAY
static void metal_overlay_enable(void *data, bool state)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
      md.overlay.enabled = state;
}

static bool metal_overlay_load(void *data,
      const void *images, unsigned num_images)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return NO;

   return [md.overlay loadImages:(const struct texture_image *)images count:num_images];
}

static bool metal_overlay_load_textures(void *data,
      const uintptr_t *textures, unsigned num_textures)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return NO;

   return [md.overlay loadTextures:textures count:num_textures];
}

static void metal_overlay_tex_geom(void *data, unsigned index,
      float x, float y, float w, float h)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
      [md.overlay updateTextureCoordsX:x y:y w:w h:h index:index];
}

static void metal_overlay_vertex_geom(void *data, unsigned index,
      float x, float y, float w, float h)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
      [md.overlay updateVertexX:x y:y w:w h:h index:index];
}

static void metal_overlay_full_screen(void *data, bool enable)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
      md.overlay.fullscreen = enable;
}

static void metal_overlay_set_alpha(void *data, unsigned index, float mod)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
      [md.overlay updateAlpha:mod index:index];
}

static const video_overlay_interface_t metal_overlay_interface = {
   metal_overlay_enable,
   metal_overlay_load,
   metal_overlay_load_textures,
   metal_overlay_tex_geom,
   metal_overlay_vertex_geom,
   metal_overlay_full_screen,
   metal_overlay_set_alpha,
};

static void metal_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   *iface = &metal_overlay_interface;
}
#endif

#ifdef HAVE_GFX_WIDGETS
static bool metal_widgets_enabled(void *data) { return true; }
#endif

static font_renderer_t metal_raster_font = {
   metal_raster_font_init,
   metal_raster_font_free,
   metal_raster_font_render_msg,
   "metal",
   metal_raster_font_get_glyph,
   NULL, /* bind_block  */
   NULL, /* flush_block */
   metal_raster_font_get_message_width,
   metal_get_line_metrics
};

video_driver_t video_metal = {
   metal_init,
   metal_frame,
   metal_set_nonblock_state,
   metal_alive,
   metal_focus,
   metal_suppress_screensaver,
   metal_has_windowed,
   metal_set_shader,
   metal_free,
   "metal",
   metal_set_viewport,
   metal_set_rotation,
   metal_viewport_info,
   metal_read_viewport,
#ifdef HAVE_OVERLAY
   metal_get_overlay_interface,
#endif
   metal_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   metal_widgets_enabled,
#endif
   NULL, /* invalidate_hw_render_cache */
   metal_read_viewport_hdr,
   &metal_raster_font
};

gfx_display_ctx_driver_t gfx_display_ctx_metal = {
   gfx_display_metal_draw,
   gfx_display_metal_draw_pipeline,
   gfx_display_metal_blend_begin,
   gfx_display_metal_blend_end,
   gfx_display_metal_get_default_mvp,
   gfx_display_metal_get_default_vertices,
   gfx_display_metal_get_default_tex_coords,
   &metal_raster_font,
   GFX_VIDEO_DRIVER_METAL,
   "metal",
   false,
   /* handles_vertex_strip: this driver's draw walks
    * draw->coords->vertices rather than reading a fixed four, so a
    * strip may be handed to it whole.
    *
    * It was missing entirely, which is what the pointer-to-bool
    * warning was reporting: scissor_begin was landing in this bool,
    * scissor_end in scissor_begin, and scissor_end was left null. The
    * compiler had been saying so for a while. */
   true,
   gfx_display_metal_scissor_begin,
   gfx_display_metal_scissor_end,
   gfx_display_metal_mesh_draw
};
