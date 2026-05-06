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
#include <Metal/Metal.h>
#include <MetalKit/MetalKit.h>
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
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <retro_math.h>
#include <retro_assert.h>
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

#include "../font_driver.h"
#include "../video_driver.h"
#ifdef HAVE_THREADS
#include "../video_thread_wrapper.h"
#endif

#include "../common/metal/metal_shader_types.h"
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

#include "../common/metal_view.h"

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

#include "../../driver.h"
#include "../../configuration.h"

#include "../../retroarch.h"
#ifdef HAVE_REWIND
#include "../../state_manager.h"
#endif
#include "../../verbosity.h"

#include "../../ui/drivers/cocoa/apple_platform.h"
#include "../../ui/drivers/cocoa/cocoa_common.h"

#define STRUCT_ASSIGN(x, y) \
{ \
   NSObject * __y = y; \
   if (x != nil) { \
      __attribute__((unused)) NSObject * __foo = (__bridge_transfer NSObject *)(__bridge void *)(x); \
      __foo = nil; \
      x = (__bridge __typeof__(x))nil; \
   } \
   if (__y != nil) \
      x = (__bridge __typeof__(x))(__bridge_retained void *)((NSObject *)__y); \
   }

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
 * Runtime: the HDR paths are still guarded with @available(...) checks
 * because RetroArch's Apple deployment targets (macOS 10.13, iOS 11) are
 * lower than the first HDR-capable OS release on each platform.  When
 * the runtime gate is false the driver stays in SDR mode.
 *
 * METAL_HDR_AVAILABLE guards compile-time only. Whenever we touch an HDR-specific
 * API inside those blocks, an @available check guards runtime dispatch. */
#include <Availability.h>
#include <TargetConditionals.h>
#if defined(TARGET_OS_TV) && TARGET_OS_TV
#  define METAL_HDR_AVAILABLE 0
#elif defined(OSX) && defined(__MAC_11_0)
#  define METAL_HDR_AVAILABLE 1
#elif defined(HAVE_COCOATOUCH) && defined(__IPHONE_16_0)
#  define METAL_HDR_AVAILABLE 1
#else
#  define METAL_HDR_AVAILABLE 0
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
static MTLPixelFormat metal_apply_hdr_layer_config(CAMetalLayer *layer,
      unsigned hdr_mode)
{
   if (!layer)
      return MTLPixelFormatBGRA8Unorm;

   if (hdr_mode == METAL_HDR_MODE_OFF)
   {
      layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
      if (@available(macOS 11.0, iOS 16.0, tvOS 16.0, *))
      {
         CGColorSpaceRef cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
         if (cs)
         {
            layer.colorspace = cs;
            CGColorSpaceRelease(cs);
         }
         layer.wantsExtendedDynamicRangeContent = NO;
      }
      return MTLPixelFormatBGRA8Unorm;
   }

   if (@available(macOS 11.0, iOS 16.0, tvOS 16.0, *))
   {
      MTLPixelFormat fmt = (hdr_mode == METAL_HDR_MODE_SCRGB)
         ? MTLPixelFormatRGBA16Float
         : MTLPixelFormatRGB10A2Unorm;
      CFStringRef csName = (hdr_mode == METAL_HDR_MODE_SCRGB)
         ? kCGColorSpaceExtendedLinearSRGB
         : kCGColorSpaceITUR_2100_PQ;

      layer.pixelFormat = fmt;
      CGColorSpaceRef cs = CGColorSpaceCreateWithName(csName);
      if (cs)
      {
         layer.colorspace = cs;
         CGColorSpaceRelease(cs);
      }
      layer.wantsExtendedDynamicRangeContent = YES;
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
#ifdef OSX
   if (@available(macOS 10.15, *))
   {
      NSScreen *screen = [NSScreen mainScreen];
      /* Potential, not current: the value reflects what the display *could*
       * produce if EDR were enabled, not what's being used right now.
       * That's the right signal for "is HDR an available mode".  SDR-only
       * displays return exactly 1.0. */
      if (screen)
         return screen.maximumPotentialExtendedDynamicRangeColorComponentValue > 1.0;
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
   if (@available(tvOS 16.0, *))
      return true;
#else
   if (@available(iOS 16.0, *))
   {
      UIScreen *screen = [UIScreen mainScreen];
      if (screen)
         return screen.potentialEDRHeadroom > 1.0;
   }
#endif
#endif
   return false;
}
#endif /* METAL_HDR_AVAILABLE */

/*
 * COMMON
 */

static NSString *RPixelStrings[RPixelFormatCount];

static NSUInteger RPixelFormatToBPP(RPixelFormat format)
{
   if (   format == RPixelFormatB5G6R5Unorm
       || format == RPixelFormatBGRA4Unorm      )
      return 2;
   return 4;
}

static NSString *NSStringFromRPixelFormat(RPixelFormat format)
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
      format = RPixelFormatInvalid;
   return RPixelStrings[format];
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

@interface BufferNode : NSObject
@property (nonatomic, readonly) id<MTLBuffer> src;
@property (nonatomic, readwrite) NSUInteger allocated;
@property (nonatomic, readwrite) BufferNode *next;
@end

@interface BufferChain : NSObject
- (instancetype)initWithDevice:(id<MTLDevice>)device blockLen:(NSUInteger)blockLen;
- (bool)allocRange:(BufferRange *)range length:(NSUInteger)length;
- (void)commitRanges;
- (void)discard;
@end

@interface Texture()
@property (nonatomic, readwrite) id<MTLTexture> texture;
@property (nonatomic, readwrite) id<MTLSamplerState> sampler;
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
   dispatch_semaphore_t _inflightSemaphore;
   id<MTLCommandQueue> _commandQueue;
   CAMetalLayer *_layer;
   id<CAMetalDrawable> _drawable;
   video_viewport_t _viewport;
   id<MTLSamplerState> _samplers[TEXTURE_FILTER_MIPMAP_NEAREST + 1];
   Filter *_filters[RPixelFormatCount]; /* convert to BGRA8888 */

   /* Main render pass state */
   id<MTLRenderCommandEncoder> _rce;

   id<MTLCommandBuffer> _blitCommandBuffer;

   NSUInteger _currentChain;
   BufferChain *_chain[CHAIN_LENGTH];
   MTLClearColor _clearColor;

   id<MTLRenderPipelineState> _states[GFX_MAX_SHADERS][2];
   id<MTLRenderPipelineState> _clearState;

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

      _inflightSemaphore         = dispatch_semaphore_create(MAX_INFLIGHT);
      _device                    = d;
      _layer                     = layer;
#ifdef OSX
      _layer.framebufferOnly     = NO;
      _layer.displaySyncEnabled  = YES;
#endif
      /* Configure drawable pool for triple-buffering */
      if (@available(iOS 13.0, macOS 10.15.4, tvOS 13.0, *))
         _layer.maximumDrawableCount = MAX_INFLIGHT;
      _library                   = l;
      _commandQueue              = [_device newCommandQueue];
      _clearColor                = MTLClearColorMake(0, 0, 0, 1);
      _uniforms.projectionMatrix = matrix_proj_ortho(0, 1, 0, 1);

      _rotation                  = 0;
      [self setRotation:0];
      _mvp_no_rot                = matrix_proj_ortho(0, 1, 0, 1);
      _mvp                       = matrix_proj_ortho(0, 1, 0, 1);

      {
         MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];

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
         return nil;

      if (![self _initClearState])
         return nil;

      if (![self _initMenuStates])
         return nil;

      for (i = 0; i < CHAIN_LENGTH; i++)
         _chain[i] = [[BufferChain alloc] initWithDevice:_device blockLen:65536];

#if METAL_HDR_AVAILABLE
      /* HDR composite & tonemap pipelines are compiled lazily the first
       * time HDR is enabled; compiling them here would waste memory when
       * HDR is never turned on.  Just set defaults. */
      _hdrEnabled         = false;
      _hdrOutputMode      = METAL_HDR_OUTPUT_OFF;
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
      _hdrShaderEmitsHDR10 = false;
      _hdrShaderEmitsHDR16 = false;
#endif
   }
   return self;
}

- (video_viewport_t *)viewport
{
   return &_viewport;
}

- (void)setViewport:(video_viewport_t *)viewport
{
   _viewport            = *viewport;
   _uniforms.outputSize = simd_make_float2(_viewport.full_width, _viewport.full_height);
}

- (Uniforms *)uniforms
{
   return &_uniforms;
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
   return _hdrOutputMode;
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
#ifdef OSX
   _layer.displaySyncEnabled = displaySyncEnabled;
#endif
}

- (bool)displaySyncEnabled
{
#ifdef OSX
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

   return _states[index][blend ? 1 : 0];
}

- (MTLVertexDescriptor *)_spriteVertexDescriptor
{
   MTLVertexDescriptor *vd = [MTLVertexDescriptor new];
   vd.attributes[0].offset = 0;
   vd.attributes[0].format = MTLVertexFormatFloat2;
   vd.attributes[1].offset = offsetof(SpriteVertex, texCoord);
   vd.attributes[1].format = MTLVertexFormatFloat2;
   vd.attributes[2].offset = offsetof(SpriteVertex, color);
   vd.attributes[2].format = MTLVertexFormatFloat4;
   vd.layouts[0].stride    = sizeof(SpriteVertex);
   return vd;
}

- (bool)_initClearState
{
   NSError *err;
   MTLVertexDescriptor          *vd = [self _spriteVertexDescriptor];
   MTLRenderPipelineDescriptor *psd = [MTLRenderPipelineDescriptor new];
   psd.label                        = @"clear_state";

   MTLRenderPipelineColorAttachmentDescriptor *ca = psd.colorAttachments[0];
   /* Always BGRA8Unorm — see comment on _t_pipelineState init.  Clear is
    * invoked on the SDR overlay offscreen when HDR is on. */
   ca.pixelFormat       = MTLPixelFormatBGRA8Unorm;

   psd.vertexDescriptor = vd;
   psd.vertexFunction   = [_library newFunctionWithName:@"stock_vertex"];
   psd.fragmentFunction = [_library newFunctionWithName:@"stock_fragment_color"];

   if (!psd.vertexFunction || !psd.fragmentFunction)
   {
      RARCH_ERR("[Metal] Failed to load clear state shader functions.\n");
      return NO;
   }

   _clearState = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
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
   MTLRenderPipelineDescriptor *psd = [MTLRenderPipelineDescriptor new];
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
   psd.vertexFunction   = [_library newFunctionWithName:@"stock_vertex"];
   psd.fragmentFunction = [_library newFunctionWithName:@"stock_fragment"];

   if (!psd.vertexFunction || !psd.fragmentFunction)
   {
      RARCH_ERR("[Metal] Failed to load stock shader functions.\n");
      return NO;
   }

   _states[VIDEO_SHADER_STOCK_BLEND][0] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label                            = @"stock_blend";
   ca.blendingEnabled                   = YES;
   _states[VIDEO_SHADER_STOCK_BLEND][1] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   MTLFunctionConstantValues *vals;

   psd.label          = @"snow_simple";
   ca.blendingEnabled = YES;
   {
      vals            = [MTLFunctionConstantValues new];
      float values[3] = {
         1.25f,   /* baseScale */
         0.50f,   /* density   */
         0.15f,   /* speed     */
      };
      [vals setConstantValue:&values[0] type:MTLDataTypeFloat withName:@"snowBaseScale"];
      [vals setConstantValue:&values[1] type:MTLDataTypeFloat withName:@"snowDensity"];
      [vals setConstantValue:&values[2] type:MTLDataTypeFloat withName:@"snowSpeed"];
   }
   psd.fragmentFunction = [_library newFunctionWithName:@"snow_fragment" constantValues:vals error:&err];
   _states[VIDEO_SHADER_MENU_3][1] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label          = @"snow";
   ca.blendingEnabled = YES;
   {
      vals            = [MTLFunctionConstantValues new];
      float values[3] = {
         3.50f,   /* baseScale */
         0.70f,   /* density   */
         0.25f,   /* speed     */
      };
      [vals setConstantValue:&values[0] type:MTLDataTypeFloat withName:@"snowBaseScale"];
      [vals setConstantValue:&values[1] type:MTLDataTypeFloat withName:@"snowDensity"];
      [vals setConstantValue:&values[2] type:MTLDataTypeFloat withName:@"snowSpeed"];
   }
   psd.fragmentFunction = [_library newFunctionWithName:@"snow_fragment" constantValues:vals error:&err];
   _states[VIDEO_SHADER_MENU_4][1] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label                       = @"bokeh";
   ca.blendingEnabled              = YES;
   psd.fragmentFunction            = [_library newFunctionWithName:@"bokeh_fragment"];
   _states[VIDEO_SHADER_MENU_5][1] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label                       = @"snowflake";
   ca.blendingEnabled              = YES;
   psd.fragmentFunction            = [_library newFunctionWithName:@"snowflake_fragment"];
   _states[VIDEO_SHADER_MENU_6][1] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label                       = @"ribbon";
   ca.blendingEnabled              = NO;
   psd.vertexFunction              = [_library newFunctionWithName:@"ribbon_vertex"];
   psd.fragmentFunction            = [_library newFunctionWithName:@"ribbon_fragment"];
   _states[VIDEO_SHADER_MENU][0]   = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label                       = @"ribbon_blend";
   ca.blendingEnabled              = YES;
   ca.sourceRGBBlendFactor         = MTLBlendFactorOne;
   ca.destinationRGBBlendFactor    = MTLBlendFactorOne;
   _states[VIDEO_SHADER_MENU][1]   = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label                       = @"ribbon_simple";
   ca.blendingEnabled              = NO;
   psd.vertexFunction              = [_library newFunctionWithName:@"ribbon_simple_vertex"];
   psd.fragmentFunction            = [_library newFunctionWithName:@"ribbon_simple_fragment"];
   _states[VIDEO_SHADER_MENU_2][0] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label                       = @"ribbon_simple_blend";
   ca.blendingEnabled              = YES;
   ca.sourceRGBBlendFactor         = MTLBlendFactorOne;
   ca.destinationRGBBlendFactor    = MTLBlendFactorOne;
   _states[VIDEO_SHADER_MENU_2][1] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
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
   id<MTLFunction> vs = [_library newFunctionWithName:@"hdr_composite_vertex"];
   id<MTLFunction> fsComp = [_library newFunctionWithName:@"hdr_composite_fragment"];
   id<MTLFunction> fsTone = [_library newFunctionWithName:@"hdr_tonemap_fragment"];
   if (!vs || !fsComp || !fsTone)
   {
      RARCH_ERR("[Metal] HDR pipelines: missing shader functions.\n");
      return NO;
   }

   /* Helper block: create one composite pipeline variant with the given
    * pixel format, label, and blend-enable flag.  Blend mode when
    * enabled matches Vulkan's HDR pipeline (SRC_ALPHA / ONE_MINUS_SRC_ALPHA
    * for both colour and alpha channels). */
   id<MTLRenderPipelineState> (^makeComposite)(MTLPixelFormat, NSString *, BOOL) =
   ^id<MTLRenderPipelineState>(MTLPixelFormat fmt, NSString *label, BOOL blend)
   {
      NSError *e = nil;
      MTLRenderPipelineDescriptor *psd = [MTLRenderPipelineDescriptor new];
      psd.label            = label;
      psd.vertexFunction   = vs;
      psd.fragmentFunction = fsComp;
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
                                               NO);
   if (!_hdrCompositeStateHDR10) return NO;

   _hdrCompositeStateSCRGB     = makeComposite(MTLPixelFormatRGBA16Float,
                                               @"HDR composite core (scRGB)",
                                               NO);
   if (!_hdrCompositeStateSCRGB) return NO;

   _hdrMenuCompositeStateHDR10 = makeComposite(MTLPixelFormatRGB10A2Unorm,
                                               @"HDR composite menu (HDR10)",
                                               YES);
   if (!_hdrMenuCompositeStateHDR10) return NO;

   _hdrMenuCompositeStateSCRGB = makeComposite(MTLPixelFormatRGBA16Float,
                                               @"HDR composite menu (scRGB)",
                                               YES);
   if (!_hdrMenuCompositeStateSCRGB) return NO;

   /* Tonemap (for screenshot/readback path). */
   {
      MTLRenderPipelineDescriptor *psd = [MTLRenderPipelineDescriptor new];
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
      MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
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
#ifdef OSX
      td.storageMode = MTLStorageModeManaged;
#else
      td.storageMode = MTLStorageModeShared;
#endif
      td.usage       = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
      id<MTLTexture> rb = [_device newTextureWithDescriptor:td];
      rb.label          = @"HDR readback";
      STRUCT_ASSIGN(_hdrReadbackTex, rb);
   }

   /* SDR overlay offscreen — BGRA8Unorm for direct compatibility with the
    * stock/menu/font/clear pipelines (all compiled against BGRA8).
    * Private storage: we only sample it from the composite fragment, never
    * CPU-read it. */
   _sdrOverlayW = w;
   _sdrOverlayH = h;
   {
      MTLTextureDescriptor *td = [MTLTextureDescriptor
                                   texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                width:w
                                                               height:h
                                                            mipmapped:NO];
      td.storageMode = MTLStorageModePrivate;
      td.usage       = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
      id<MTLTexture> sdr = [_device newTextureWithDescriptor:td];
      sdr.label          = @"SDR overlay (HDR)";
      STRUCT_ASSIGN(_sdrOverlayTex, sdr);
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

   /* Pick drawable format + colourspace for the layer. */
   MTLPixelFormat newFmt    = _layer.pixelFormat;
   CGColorSpaceRef newCS    = NULL;
   BOOL wantEDR             = NO;

   if (wantEnable)
   {
      if (@available(macOS 11.0, iOS 16.0, tvOS 16.0, *))
      {
         if (mode == METAL_HDR_OUTPUT_HDR10)
         {
            newFmt   = MTLPixelFormatRGB10A2Unorm;
            newCS    = CGColorSpaceCreateWithName(kCGColorSpaceITUR_2100_PQ);
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
   if (@available(macOS 11.0, iOS 16.0, tvOS 16.0, *))
   {
      if (newCS)
         _layer.colorspace = newCS;
      _layer.wantsExtendedDynamicRangeContent = wantEDR;
   }
#endif
   if (newCS)
      CGColorSpaceRelease(newCS);

   _hdrDrawableFormat = newFmt;
   _hdrOutputMode     = mode;
   _hdrEnabled        = wantEnable;

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
      STRUCT_ASSIGN(_hdrReadbackTex,  nil);
      STRUCT_ASSIGN(_sdrOverlayTex,   nil);
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
      _rce = nil;
   }

   id<CAMetalDrawable> drawable = self.nextDrawable;
   if (!drawable || !drawable.texture)
   {
      RARCH_WARN("[Metal] HDR composite: no drawable.\n");
      return;
   }
   if (_captureEnabled)
      _backBuffer = drawable.texture;

   const BOOL scRGB = (_hdrOutputMode == METAL_HDR_OUTPUT_SCRGB);

   /* --- Pass 1: core composite (blending off, clear).
    * Runs when a core source is available.  When source is nil (no core
    * has rendered yet — e.g. fresh driver init / post-reinit first
    * frame), we still need to clear the drawable to a known state
    * before pass 2 loads it, otherwise the drawable is presented with
    * uninitialised memory (visible as full green / blue on HDR). */
   if (source)
   {
      MTLRenderPassDescriptor *rpd        = [MTLRenderPassDescriptor new];
      rpd.colorAttachments[0].texture     = drawable.texture;
      rpd.colorAttachments[0].loadAction  = MTLLoadActionClear;
      rpd.colorAttachments[0].clearColor  = _clearColor;
      rpd.colorAttachments[0].storeAction = MTLStoreActionStore;

      id<MTLRenderPipelineState> state = scRGB
         ? _hdrCompositeStateSCRGB
         : _hdrCompositeStateHDR10;

      /* Patch CoreViewport into a local uniforms copy; caller's buffer
       * stays untouched. */
      HDRUniforms local  = *uniforms;
      local.CoreViewport = simd_make_float4((float)_viewport.x,
                                            (float)_viewport.y,
                                            (float)_viewport.width,
                                            (float)_viewport.height);

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
      MTLRenderPassDescriptor *rpd        = [MTLRenderPassDescriptor new];
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
      MTLRenderPassDescriptor *rpd        = [MTLRenderPassDescriptor new];
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

   MTLRenderPassDescriptor *rpd        = [MTLRenderPassDescriptor new];
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

#ifdef OSX
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
   tex.texture    = [self newTexture:image mipmapped:mipmapped];
   tex.sampler    = _samplers[filter];
   return tex;
}

- (id<MTLTexture>)newTexture:(struct texture_image)image mipmapped:(bool)mipmapped
{
   MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
         width:image.width
         height:image.height
         mipmapped:mipmapped];

   id<MTLTexture> t = [_device newTextureWithDescriptor:td];
   [t replaceRegion:MTLRegionMake2D(0, 0, image.width, image.height)
        mipmapLevel:0
        withBytes:image.pixels
        bytesPerRow:4 * image.width];

   if (mipmapped)
   {
      id<MTLCommandBuffer> cb = self.blitCommandBuffer;
      id<MTLBlitCommandEncoder> bce = [cb blitCommandEncoder];
      [bce generateMipmapsForTexture:t];
      [bce endEncoding];
   }

   return t;
}

- (id<CAMetalDrawable>)nextDrawable
{
   if (_drawable == nil)
      _drawable = _layer.nextDrawable;
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
      _blitCommandBuffer       = [_commandQueue commandBuffer];
      _blitCommandBuffer.label = @"Blit command buffer";
   }
   return _blitCommandBuffer;
}

- (void)_nextChain
{
   _currentChain = (_currentChain + 1) % CHAIN_LENGTH;
   [_chain[_currentChain] discard];
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
    * We stream one row at a time from Metal into a small scratch
    * buffer, converting as we go. Previously this allocated a
    * full-frame BGRA copy (width * height * 4 bytes) via malloc(),
    * which for a 4K capture is ~32 MiB of transient allocation
    * pressure per screenshot. One row is typically a few KiB and
    * fits comfortably on the stack (up to 16K width here; beyond
    * that we fall back to heap for safety). */
   size_t y;
   NSUInteger rowBytes, dstStride;
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
   if (rowBytes > sizeof(stackRow))
   {
      heapRow = (uint8_t *)malloc(rowBytes);
      if (!heapRow)
         return NO;
      row     = heapRow;
   }

   dstStride = _viewport.width * 3;
   dst       = buffer + (_viewport.height - 1) * dstStride;

   for (y = 0; y < _viewport.height; y++, dst -= dstStride)
   {
      size_t x;
      [srcTex getBytes:row
           bytesPerRow:rowBytes
            fromRegion:MTLRegionMake2D(0, (NSUInteger)_viewport.y + y,
                                       srcTex.width, 1)
           mipmapLevel:0];

      for (x = 0; x < _viewport.width; x++)
      {
         dst[3 * x + 0] = row[4 * (_viewport.x + x) + 0];
         dst[3 * x + 1] = row[4 * (_viewport.x + x) + 1];
         dst[3 * x + 2] = row[4 * (_viewport.x + x) + 2];
      }
   }

   free(heapRow);

   return YES;
}

- (void)begin
{
   if (_commandBuffer != nil)
   {
      RARCH_WARN("[Metal] begin called with active command buffer - resetting\n");
      _commandBuffer = nil;
   }

   /* Don't use semaphore for frame pacing - let nextDrawable handle it.
    * CAMetalLayer.nextDrawable will block if no drawable is available,
    * which naturally paces us to the display refresh rate.
    * Using a semaphore on top of this causes timing mismatches because
    * the semaphore signals on presentation but the drawable isn't
    * released until the NEXT vsync. */

   _commandBuffer = [_commandQueue commandBuffer];
   _commandBuffer.label = @"Frame command buffer";
   _backBuffer = nil;
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
         MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor new];
         /* Clear to fully-transparent black so the core video shows
          * through where no UI is drawn. */
         rpd.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);
         rpd.colorAttachments[0].loadAction = MTLLoadActionClear;
         rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
         rpd.colorAttachments[0].texture    = _sdrOverlayTex;
         _rce       = [_commandBuffer renderCommandEncoderWithDescriptor:rpd];
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

      MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor new];
      rpd.colorAttachments[0].clearColor = _clearColor;
      rpd.colorAttachments[0].loadAction = MTLLoadActionClear;
      rpd.colorAttachments[0].texture = drawable.texture;
      if (_captureEnabled)
         _backBuffer = drawable.texture;
      _rce       = [_commandBuffer renderCommandEncoderWithDescriptor:rpd];
      _rce.label = @"Frame command encoder";
   }
   return _rce;
}

- (void)resetRenderViewport:(ViewportResetMode)mode
{
   bool fullscreen = mode == kFullscreenViewport;
   MTLViewport vp  = {
      .originX = fullscreen ? 0 : _viewport.x,
      .originY = fullscreen ? 0 : _viewport.y,
      .width   = fullscreen ? _viewport.full_width : _viewport.width,
      .height  = fullscreen ? _viewport.full_height : _viewport.height,
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
      .width   = _viewport.full_width,
      .height  = _viewport.full_height,
   };
   [self.rce setScissorRect:sr];
}

- (void)drawQuadX:(float)x y:(float)y w:(float)w h:(float)h
                r:(float)r g:(float)g b:(float)b a:(float)a
{
   SpriteVertex v[4];
   v[0].position = simd_make_float2(x, y);
   v[1].position = simd_make_float2(x + w, y);
   v[2].position = simd_make_float2(x, y + h);
   v[3].position = simd_make_float2(x + w, y + h);

   simd_float4 color = simd_make_float4(r, g, b, a);
   v[0].color = color;
   v[1].color = color;
   v[2].color = color;
   v[3].color = color;

   id<MTLRenderCommandEncoder> rce = self.rce;
   [rce setRenderPipelineState:_clearState];
   [rce setVertexBytes:&v length:sizeof(v) atIndex:BufferIndexPositions];
   [rce setVertexBytes:&_uniforms length:sizeof(_uniforms) atIndex:BufferIndexUniforms];
   [rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
}

- (void)end
{
   if (_commandBuffer == nil)
   {
      RARCH_WARN("[Metal] end called without active command buffer\n");
      return;
   }

   [_chain[_currentChain] commitRanges];

   if (_blitCommandBuffer)
   {
#ifdef OSX
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
      _blitCommandBuffer = nil;
   }

   if (_rce)
   {
      [_rce endEncoding];
      _rce = nil;
   }

   id<CAMetalDrawable> drawable = self.nextDrawable;

   if (drawable)
   {
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

   _commandBuffer = nil;
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
   _drawable = nil;
   _drawable = _layer.nextDrawable;
}

- (bool)allocRange:(BufferRange *)range length:(NSUInteger)length
{
   return [_chain[_currentChain] allocRange:range length:length];
}

@end

@implementation Texture
@end

@implementation BufferNode

- (instancetype)initWithBuffer:(id<MTLBuffer>)src
{
   if (self = [super init])
      _src = src;
   return self;
}

@end

@implementation BufferChain
{
   id<MTLDevice> _device;
   NSUInteger _blockLen;
   BufferNode *_head;
   NSUInteger _offset; /* offset into _current */
   BufferNode *_current;
   NSUInteger _length;
   NSUInteger _allocated;
}

/* macOS requires constants in a buffer to have a 256 byte alignment. */
#ifdef TARGET_OS_MAC
static const NSUInteger kConstantAlignment = 256;
#else
static const NSUInteger kConstantAlignment = 4;
#endif

- (instancetype)initWithDevice:(id<MTLDevice>)device blockLen:(NSUInteger)blockLen
{
   if (self = [super init])
   {
      _device   = device;
      _blockLen = blockLen;
   }
   return self;
}

- (NSString *)debugDescription
{
   return [NSString stringWithFormat:@"length=%ld, allocated=%ld", _length, _allocated];
}

- (void)commitRanges
{
#ifdef OSX
   BufferNode *n;
   for (n = _head; n != nil; n = n.next)
   {
      if (n.allocated > 0)
         [n.src didModifyRange:NSMakeRange(0, n.allocated)];
   }
#endif
}

- (void)discard
{
   /* Trim the tail: any node that wasn't touched during this
    * chain's previous use (allocated == 0) is dropped. Nodes are
    * appended in alloc order, so once we see the first trailing
    * unused node the whole tail is unused. We only trim when the
    * chain was actually used (_allocated > 0) so that a single
    * quiescent frame doesn't drop the entire chain and force
    * reallocation on the next use.
    *
    * This bounds retained memory to the recent high-water mark
    * rather than the all-time high-water mark, which previously
    * grew monotonically: any one-off large allocation (e.g. a
    * heavy shader pass or a brief geometry spike) kept its
    * oversized backing node alive for the lifetime of the driver,
    * across all CHAIN_LENGTH chains. */
   if (_head && _allocated > 0)
   {
      BufferNode *keep = _head;
      BufferNode *n;
      for (n = _head; n != nil; n = n.next)
      {
         if (n.allocated > 0)
            keep = n;
      }
      if (keep.next)
      {
         NSUInteger dropped = 0;
         for (n = keep.next; n != nil; n = n.next)
            dropped += n.src.length;
         _length -= dropped;
         keep.next = nil;
      }
   }

   /* Reset per-node allocated so commitRanges on the next use of
    * this chain does not didModifyRange: a stale range from this
    * cycle into a node that gets partially refilled. */
   {
      BufferNode *n;
      for (n = _head; n != nil; n = n.next)
         n.allocated = 0;
   }

   _current   = _head;
   _offset    = 0;
   _allocated = 0;
}

- (bool)allocRange:(BufferRange *)range length:(NSUInteger)length
{
   MTLResourceOptions opts = PLATFORM_METAL_RESOURCE_STORAGE_MODE;
   memset(range, 0, sizeof(*range));

   if (!_head)
   {
      _head    = [[BufferNode alloc] initWithBuffer:[_device newBufferWithLength:_blockLen options:opts]];
      _length += _blockLen;
      _current = _head;
      _offset  = 0;
   }

   if ([self _subAllocRange:range length:length])
      return YES;

   while (_current.next)
   {
      [self _nextNode];
      if ([self _subAllocRange:range length:length])
         return YES;
   }

   NSUInteger blockLen = _blockLen;
   if (length > blockLen)
      blockLen = length;

   _current.next = [[BufferNode alloc] initWithBuffer:[_device newBufferWithLength:blockLen options:opts]];
   if (!_current.next)
      return NO;

   _length += blockLen;

   [self _nextNode];
   retro_assert([self _subAllocRange:range length:length]);
   return YES;
}

- (void)_nextNode
{
   _current = _current.next;
   _offset  = 0;
}

- (BOOL)_subAllocRange:(BufferRange *)range length:(NSUInteger)length
{
   NSUInteger nextOffset  = _offset + length;
   if (nextOffset <= _current.src.length)
   {
      _current.allocated  = nextOffset;
      _allocated         += length;
      range->data         = _current.src.contents + _offset;
      range->buffer       = _current.src;
      range->offset       = _offset;
      _offset             = MTL_ALIGN_BUFFER(nextOffset);
      return YES;
   }
   return NO;
}

@end

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
   id<MTLFunction> function = [library newFunctionWithName:name];
   id<MTLComputePipelineState> kernel = [device newComputePipelineStateWithFunction:function error:error];
   if (*error != nil)
      return nil;

   MTLSamplerDescriptor *sd    = [MTLSamplerDescriptor new];
   sd.minFilter                = MTLSamplerMinMagFilterNearest;
   sd.magFilter                = MTLSamplerMinMagFilterNearest;
   sd.sAddressMode             = MTLSamplerAddressModeClampToEdge;
   sd.tAddressMode             = MTLSamplerAddressModeClampToEdge;
   sd.mipFilter                = MTLSamplerMipFilterNotMipmapped;
   id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:sd];

   return [[Filter alloc] initWithKernel:kernel sampler:sampler];
}

- (instancetype)initWithKernel:(id<MTLComputePipelineState>)kernel sampler:(id<MTLSamplerState>)sampler
{
   if (self = [super init])
   {
      _kernel  = kernel;
      _sampler = sampler;
   }
   return self;
}

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
}

- (instancetype)initWithContext:(Context *)context
{
   if (self = [super init])
   {
      _context                   = context;
      _clearColor                = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
      _uniforms.projectionMatrix = matrix_proj_ortho(0, 1, 0, 1);
      _useScissorRect            = NO;
   }
   return self;
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
   draw->x                 = 0;
   draw->y                 = 0;
   draw->matrix_data       = NULL;
   _uniforms.outputSize    = simd_make_float2(_context.viewport->full_width, _context.viewport->full_height);
   draw->backend_data      = &_uniforms;
   draw->backend_data_size = sizeof(_uniforms);

   switch (draw->pipeline_id)
   {
      /* ribbon */
      default:
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      {
         gfx_display_t *p_disp   = disp_get_ptr();
         video_coord_array_t *ca = &p_disp->dispca;
         draw->coords            = (struct video_coords *)&ca->coords;
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

   _uniforms.time += 0.01;
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

      pv->color = simd_make_float4(color[0], color[1], color[2], color[3]);
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
      .originX = draw->x,
      .originY = _context.viewport->full_height - draw->y - draw->height,
      .width   = draw->width,
      .height  = draw->height,
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

   Texture *tex = (__bridge Texture *)(void *)draw->texture;
   if (tex == nil)
      return;

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
      _context      = c;
      _visible      = YES;
      if (_format == RPixelFormatBGRA8Unorm || _format == RPixelFormatBGRX8Unorm)
         _drawState = ViewDrawStateEncoder;
      else
         _drawState = ViewDrawStateAll;
      self.size     = d.size;
      self.frame    = CGRectMake(0, 0, 1, 1);
   }
   return self;
}

- (void)setSize:(CGSize)size
{
   if (CGSizeEqualToSize(_size, size))
      return;

   _size = size;

   {
      MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
            width: (NSUInteger)size.width
            height:(NSUInteger)size.height
            mipmapped:NO];
      td.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
      _texture = [_context.device newTextureWithDescriptor:td];
   }

   if (   _format != RPixelFormatBGRA8Unorm
       && _format != RPixelFormatBGRX8Unorm)
   {
      MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR16Uint
               width:(NSUInteger)size.width
               height:(NSUInteger)size.height
               mipmapped:NO];
      _src = [_context.device newTextureWithDescriptor:td];
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
       || _format == RPixelFormatBGRX8Unorm)
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
   if (_format == RPixelFormatBGRA8Unorm || _format == RPixelFormatBGRX8Unorm)
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
      unsigned video_width,
      unsigned video_height)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md && draw)
      [md.display draw:draw];
}

static void gfx_display_metal_draw_pipeline(
      gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp,
      void *data,
      unsigned video_width,
      unsigned video_height)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md && draw)
      [md.display drawPipeline:draw];
}

static void gfx_display_metal_scissor_begin(
      void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y, unsigned width, unsigned height)
{
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

static void gfx_display_metal_scissor_end(void *data,
      unsigned video_width,
      unsigned video_height)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
      [md.display clearScissorRect];
}

gfx_display_ctx_driver_t gfx_display_ctx_metal = {
   gfx_display_metal_draw,
   gfx_display_metal_draw_pipeline,
   gfx_display_metal_blend_begin,
   gfx_display_metal_blend_end,
   gfx_display_metal_get_default_mvp,
   gfx_display_metal_get_default_vertices,
   gfx_display_metal_get_default_tex_coords,
   FONT_DRIVER_RENDER_METAL_API,
   GFX_VIDEO_DRIVER_METAL,
   "metal",
   false,
   gfx_display_metal_scissor_begin,
   gfx_display_metal_scissor_end
};

/*
 * FONT DRIVER
 */

@interface MetalRaster : NSObject
{
   __weak MetalDriver *_driver;
   const font_renderer_driver_t *_font_driver;
   void *_font_data;
   struct font_atlas *_atlas;

   NSUInteger _stride;
   id<MTLBuffer> _buffer;
   id<MTLTexture> _texture;

   id<MTLRenderPipelineState> _state;
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
}

- (instancetype)initWithDriver:(MetalDriver *)driver fontPath:(const char *)font_path fontSize:(unsigned)font_size
{
   if (self = [super init])
   {
      if (driver == nil)
         return nil;

      _driver  = driver;
      _context = driver.context;
      if (!font_renderer_create_default(
               &_font_driver,
               &_font_data, font_path, font_size))
         return nil;

      _uniforms.projectionMatrix = matrix_proj_ortho(0, 1, 0, 1);
      _atlas  = _font_driver->get_atlas(_font_data);
      _stride = MTL_ALIGN_BUFFER(_atlas->width);

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
         uint8_t       *dst = (uint8_t *)_buffer.contents;
         const uint8_t *src = (const uint8_t *)_atlas->buffer;
         if (_stride == _atlas->width)
         {
            memcpy(dst, src, (size_t)_stride * _atlas->height);
         }
         else
         {
            for (i = 0; i < _atlas->height; i++)
            {
               memcpy(dst, src, _atlas->width);
               dst += _stride;
               src += _atlas->width;
            }
         }
      }
#if !defined(HAVE_COCOATOUCH)
      [_buffer didModifyRange:NSMakeRange(0, _buffer.length)];
#endif

      MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                                                    width:_atlas->width
                                                                                   height:_atlas->height
                                                                                mipmapped:NO];

      _texture  = [_buffer newTextureWithDescriptor:td offset:0 bytesPerRow:_stride];

      if (![self _initializeState])
         return nil;
   }
   return self;
}

- (bool)_initializeState
{
   {
      NSError *err;
      MTLVertexDescriptor *vd    = [MTLVertexDescriptor new];

      vd.attributes[0].offset    = 0;
      vd.attributes[0].format    = MTLVertexFormatFloat2;
      vd.attributes[1].offset    = offsetof(SpriteVertex, texCoord);
      vd.attributes[1].format    = MTLVertexFormatFloat2;
      vd.attributes[2].offset    = offsetof(SpriteVertex, color);
      vd.attributes[2].format    = MTLVertexFormatFloat4;
      vd.layouts[0].stride       = sizeof(SpriteVertex);
      vd.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

      MTLRenderPipelineDescriptor *psd = [MTLRenderPipelineDescriptor new];
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
      psd.vertexFunction             = [_context.library newFunctionWithName:@"sprite_vertex"];
      psd.fragmentFunction           = [_context.library newFunctionWithName:@"sprite_fragment_a8"];

      if (!psd.vertexFunction || !psd.fragmentFunction)
         return NO;

      _state                         = [_context.device newRenderPipelineStateWithDescriptor:psd error:&err];
      if (err != nil)
         return NO;
   }

   {
      MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
      sd.minFilter             = MTLSamplerMinMagFilterLinear;
      sd.magFilter             = MTLSamplerMinMagFilterLinear;
      _sampler                 = [_context.device newSamplerStateWithDescriptor:sd];
   }
   return YES;
}

- (void)updateGlyph:(const struct font_glyph *)glyph
{
   if (_atlas->dirty)
   {
      unsigned row;
      for (row = glyph->atlas_offset_y; row < (glyph->atlas_offset_y + glyph->height); row++)
      {
         uint8_t *src = _atlas->buffer + row * _atlas->width + glyph->atlas_offset_x;
         uint8_t *dst = (uint8_t *)_buffer.contents + row * _stride + glyph->atlas_offset_x;
         memcpy(dst, src, glyph->width);
      }

#if !defined(HAVE_COCOATOUCH)
      /* didModifyRange takes a BYTE range, not a row index.
       * Every other call site in this file (lines 958, 1664, 1681,
       * 3082, 3550) passes bytes. Previously offset was the row
       * index, which meant the invalidated range almost never
       * overlapped the actually-modified rows on managed-storage
       * devices, producing stale/garbled glyphs until the entire
       * atlas was invalidated by some other path. */
      NSUInteger offset = (NSUInteger)glyph->atlas_offset_y * _stride;
      NSUInteger len    = (NSUInteger)glyph->height         * _stride;
      [_buffer didModifyRange:NSMakeRange(offset, len)];
#endif

      _atlas->dirty = false;
   }
}

- (int)getWidthForMessage:(const char *)msg length:(NSUInteger)length scale:(float)scale
{
   NSUInteger i;
   int delta_x = 0;
   const struct font_glyph* glyph_q;

   /* Validate font data before use - can become invalid during
    * video context reset or if font was freed while in use */
   if (!_font_driver || !_font_data)
      return 0;

   glyph_q = _font_driver->get_glyph(_font_data, '?');

   for (i = 0; i < length; i++)
   {
      const struct font_glyph *glyph;
      /* Do something smarter here ... */
      if (!(glyph = _font_driver->get_glyph(_font_data, (uint8_t)msg[i])))
         if (!(glyph = glyph_q))
            continue;

      [self updateGlyph:glyph];
      delta_x += glyph->advance_x;
   }

   return (int)(delta_x * scale);
}

- (const struct font_glyph *)getGlyph:(uint32_t)code
{
   const struct font_glyph *glyph;
   if (!_font_driver || !_font_data)
      return NULL;
   glyph = _font_driver->get_glyph(_font_data, code);
   if (glyph)
      [self updateGlyph:glyph];
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
      const vector_float4 *color)
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
      pv[i].color    = *color;
   }
}

- (void)_renderLine:(const char *)msg
             length:(NSUInteger)length
              scale:(float)scale
              color:(vector_float4)color
               posX:(float)posX
               posY:(float)posY
            aligned:(unsigned)aligned
{
   const struct font_glyph* glyph_q;
   const char  *msg_end;
   int                x;
   int                y;
   int          delta_x;
   int          delta_y;
   float inv_tex_size_x;
   float inv_tex_size_y;
   float inv_win_width;
   float inv_win_height;

   if (!_font_driver || !_font_data)
      return;

   msg_end          = msg + length;
   x                = (int)roundf(posX * _driver.viewport->full_width);
   y                = (int)roundf((1.0f - posY) * _driver.viewport->full_height);
   delta_x          = 0;
   delta_y          = 0;
   inv_tex_size_x   = 1.0f / _texture.width;
   inv_tex_size_y   = 1.0f / _texture.height;
   inv_win_width    = 1.0f / _driver.viewport->full_width;
   inv_win_height   = 1.0f / _driver.viewport->full_height;

   switch (aligned)
   {
      case TEXT_ALIGN_RIGHT:
         x -= [self getWidthForMessage:msg length:length scale:scale];
         break;

      case TEXT_ALIGN_CENTER:
         x -= [self getWidthForMessage:msg length:length scale:scale] / 2;
         break;

      default:
         break;
   }

   SpriteVertex *v = (SpriteVertex *)_range.data;
   v              += _vertices;
   glyph_q         = _font_driver->get_glyph(_font_data, '?');

   while (msg < msg_end)
   {
      int off_x, off_y, tex_x, tex_y, width, height;
      const struct font_glyph *glyph;
      unsigned code = utf8_walk(&msg);

      /* Do something smarter here .. */
      if (!(glyph = _font_driver->get_glyph(_font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      [self updateGlyph:glyph];

      off_x  = glyph->draw_offset_x;
      off_y  = glyph->draw_offset_y;
      tex_x  = glyph->atlas_offset_x;
      tex_y  = glyph->atlas_offset_y;
      width  = glyph->width;
      height = glyph->height;

      write_quad6(v,
            (x + (off_x + delta_x) * scale) * inv_win_width,
            (y + (off_y + delta_y) * scale) * inv_win_height,
            width * scale * inv_win_width,
            height * scale * inv_win_height,
            tex_x * inv_tex_size_x,
            tex_y * inv_tex_size_y,
            width * inv_tex_size_x,
            height * inv_tex_size_y,
            &color);

      _vertices += 6;
      v         += 6;

      delta_x   += glyph->advance_x;
      delta_y   += glyph->advance_y;
   }
}

- (void)_flush
{
   if (_vertices == 0)
      return;

   id<MTLRenderCommandEncoder> rce = _context.rce;
   [rce pushDebugGroup:@"render fonts"];

   [_context resetRenderViewport:kFullscreenViewport];
   [rce setRenderPipelineState:_state];
   [rce setVertexBytes:&_uniforms length:sizeof(Uniforms) atIndex:BufferIndexUniforms];
   [rce setVertexBuffer:_range.buffer offset:_range.offset atIndex:BufferIndexPositions];
   [rce setFragmentTexture:_texture atIndex:TextureIndexColor];
   [rce setFragmentSamplerState:_sampler atIndex:SamplerIndexDraw];
   [rce drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:_vertices];
   [rce popDebugGroup];

   _vertices = 0;
}

- (void)renderMessage:(const char *)msg
               height:(unsigned)height
                scale:(float)scale
                color:(vector_float4)color
                 posX:(float)posX
                 posY:(float)posY
              aligned:(unsigned)aligned
{
   int lines = 0;
   float line_height;
   struct font_line_metrics *line_metrics = NULL;
   if (!_font_driver || !_font_data)
      return;
   _font_driver->get_line_metrics(_font_data, &line_metrics);
   line_height = line_metrics->height * scale / height;
   for (;;)
   {
      const char *delim = msg;
      while (*delim && *delim != '\n')
         delim++;
      size_t msg_len = (size_t)(delim - msg);
      /* Draw the line */
      [self _renderLine:msg
                 length:msg_len
                  scale:scale
                  color:color
                   posX:posX
                   posY:posY - (float)lines * line_height
                aligned:aligned];
      if (!*delim)
         break;
      msg += msg_len + 1;
      lines++;
   }
}

- (void)renderMessage:(const char *)msg
                width:(unsigned)width
               height:(unsigned)height
               params:(const struct font_params *)params
{
   float x, y, scale, drop_mod, drop_alpha;
   int drop_x, drop_y;
   enum text_alignment text_align;
   vector_float4 color;

   if (!msg || !*msg)
      return;

   if (params)
   {
      x          = params->x;
      y          = params->y;
      scale      = params->scale;
      text_align = params->text_align;
      drop_x     = params->drop_x;
      drop_y     = params->drop_y;
      drop_mod   = params->drop_mod;
      drop_alpha = params->drop_alpha;

      color      = simd_make_float4(
            FONT_COLOR_GET_RED(params->color) / 255.0f,
            FONT_COLOR_GET_GREEN(params->color) / 255.0f,
            FONT_COLOR_GET_BLUE(params->color) / 255.0f,
            FONT_COLOR_GET_ALPHA(params->color) / 255.0f);

   }
   else
   {
      settings_t *settings     = config_get_ptr();
      float video_msg_pos_x    = settings->floats.video_msg_pos_x;
      float video_msg_pos_y    = settings->floats.video_msg_pos_y;
      float video_msg_color_r  = settings->floats.video_msg_color_r;
      float video_msg_color_g  = settings->floats.video_msg_color_g;
      float video_msg_color_b  = settings->floats.video_msg_color_b;
      x                        = video_msg_pos_x;
      y                        = video_msg_pos_y;
      scale                    = 1.0f;
      text_align               = TEXT_ALIGN_LEFT;

      color                    = simd_make_float4(
            video_msg_color_r,
            video_msg_color_g,
            video_msg_color_b,
            1.0f);

      drop_x                   = -2;
      drop_y                   = -2;
      drop_mod                 = 0.3f;
      drop_alpha               = 1.0f;
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
                      height:height
                       scale:scale
                       color:color_dark
                        posX:x + scale * drop_x / width
                        posY:y + scale * drop_y / height
                     aligned:text_align];
      }

      [self renderMessage:msg
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
   MetalRaster *r = [[MetalRaster alloc] initWithDriver:(__bridge MetalDriver *)data fontPath:font_path fontSize:(unsigned)font_size];

   if (!r)
      return NULL;

   return (__bridge_retained void *)r;
}

static void metal_raster_font_free(void *data, bool is_threaded)
{
   MetalRaster *r = (__bridge_transfer MetalRaster *)data;

   [r deinit];
   r = nil;
}

static int metal_raster_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{
   MetalRaster *r = (__bridge MetalRaster *)data;
   return [r getWidthForMessage:msg length:(unsigned)msg_len scale:scale];
}

static void metal_raster_font_render_msg(
      void *userdata,
      void *data, const char *msg,
      const struct font_params *params)
{
   MetalRaster *r       = (__bridge MetalRaster *)data;
   MetalDriver *d       = (__bridge MetalDriver *)userdata;
   video_viewport_t *vp = [d viewport];
   unsigned width       = vp->full_width;
   unsigned height      = vp->full_height;
   [r renderMessage:msg width:width height:height params:params];
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

font_renderer_t metal_raster_font = {
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

@implementation MetalDriver
{
   FrameView *_frameView;
   MetalMenu *_menu;
   Overlay *_overlay;

   video_info_t _video;

   id<MTLDevice> _device;
   id<MTLLibrary> _library;
   Context *_context;

   CAMetalLayer *_layer;

   /* Render target layer state */
   id<MTLRenderPipelineState> _t_pipelineState;
   id<MTLRenderPipelineState> _t_pipelineStateNoAlpha;

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
                        input:(input_driver_t **)input
                    inputData:(void **)inputData
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
            NSArray<id<MTLDevice>> *devices        = MTLCopyAllDevices();
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

            if (count > 0 && gpu_index >= 0 && gpu_index < (int)count)
            {
               const char *picked_name;
               _device     = [devices objectAtIndex:gpu_index];
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
      view.delegate                 = self;
      _layer                        = (CAMetalLayer *)view.layer;

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
         metal_apply_hdr_layer_config(_layer, initial_hdr_mode);
         _initial_hdr_mode           = initial_hdr_mode;
      }
#else
      _initial_hdr_mode              = METAL_HDR_MODE_OFF;
#endif

      if (![self _initMetal])
         return nil;

      _video                        = *video;
      _viewport                     = (video_viewport_t *)calloc(1, sizeof(video_viewport_t));
      /* NULL-check: Context's setViewport: (see line ~516)
       * unconditionally dereferences the pointer via
       * _viewport = *viewport;  A NULL here would crash on
       * the first _context.viewport = _viewport assignment
       * downstream in setVideoMode / show_mouse / frame.
       * Return nil to fail the init - matches the pattern
       * used by the _initMetal bailout just above.  ARC will
       * tear down the partial instance (Metal library/queue
       * ref-counts, string_list _gpu_list) via dealloc. */
      if (!_viewport)
         return nil;
      _viewportMVP.projectionMatrix = matrix_proj_ortho(0, 1, 0, 1);

      _keepAspect                   = _video.force_aspect;

      gfx_ctx_mode_t mode = {
         .width = _video.width,
         .height = _video.height,
         .fullscreen = _video.fullscreen,
      };

      if (mode.width == 0 || mode.height == 0)
      {
         /* 0 indicates full screen, so we'll use the view's dimensions,
          * which should already be full screen
          * If this turns out to be the wrong assumption, we can use NSScreen
          * to query the dimensions */
         CGSize size = view.frame.size;
         mode.width  = (unsigned int)size.width;
         mode.height = (unsigned int)size.height;
      }

      [apple_platform setVideoMode:mode];

#ifdef HAVE_COCOATOUCH
      [self mtkView:view drawableSizeWillChange:CGSizeMake(mode.width, mode.height)];
#endif

      *input         = NULL;
      *inputData     = NULL;
      /* graphics display driver */
      _display       = [[MenuDisplay alloc] initWithContext:_context];
      /* menu view */
      _menu          = [[MetalMenu alloc] initWithContext:_context];

      /* Framebuffer view */
      {
         ViewDescriptor *vd  = [ViewDescriptor new];
         vd.format           = _video.rgb32 ? RPixelFormatBGRX8Unorm : RPixelFormatB5G6R5Unorm;
         vd.size             = CGSizeMake(video->width, video->height);
         vd.filter           = _video.smooth ? RTextureFilterLinear : RTextureFilterNearest;
         _frameView          = [[FrameView alloc] initWithDescriptor:vd context:_context];
         _frameView.viewport = _viewport;
         [_frameView setFilteringIndex:0 smooth:video->smooth];
         _frameView.hdrEnabled = (_initial_hdr_mode != METAL_HDR_MODE_OFF);
      }

      /* Overlay view */
      _overlay = [[Overlay alloc] initWithContext:_context];

      font_driver_init_osd((__bridge void *)self,
            video,
            false,
            video->is_threaded,
            FONT_DRIVER_RENDER_METAL_API);

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
         uint32_t disp_flags = video_driver_get_disp_flags();
         bool edr_supported  = metal_display_supports_edr();
         disp_flags &= ~(VIDEO_FLAG_HDR_SUPPORT
                        | VIDEO_FLAG_HDR10_SUPPORT
                        | VIDEO_FLAG_SCRGB_SUPPORT);
         if (edr_supported)
         {
            /* Both modes map to the same Metal surface path (swap the
             * CAMetalLayer pixel format + colour space), so whenever
             * EDR is available we advertise both. */
            disp_flags |= VIDEO_FLAG_HDR10_SUPPORT | VIDEO_FLAG_SCRGB_SUPPORT;
            disp_flags |= VIDEO_FLAG_HDR_SUPPORT;
         }
         video_driver_set_disp_flags(disp_flags);
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
            size = CGSizeMake(mode.width, mode.height);
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
   }
   return self;
}

- (void)dealloc
{
   if (_viewport)
   {
      free(_viewport);
      _viewport = nil;
   }
   font_driver_free_osd();

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
   video_driver_set_disp_flags(video_driver_get_disp_flags()
         & ~(VIDEO_FLAG_HDR_SUPPORT
            | VIDEO_FLAG_HDR10_SUPPORT
            | VIDEO_FLAG_SCRGB_SUPPORT));
#endif
}

- (bool)_initMetal
{
   _library = [_device newDefaultLibrary];
   _context = [[Context alloc] initWithDevice:_device
                                        layer:_layer
                                      library:_library];

   {
      NSError *err;
      MTLRenderPipelineDescriptor *psd;
      MTLRenderPipelineColorAttachmentDescriptor *ca;
      MTLVertexDescriptor *vd        = [MTLVertexDescriptor new];
      vd.attributes[0].offset        = 0;
      vd.attributes[0].format        = MTLVertexFormatFloat3;
      vd.attributes[1].offset        = offsetof(Vertex, texCoord);
      vd.attributes[1].format        = MTLVertexFormatFloat2;
      vd.layouts[0].stride           = sizeof(Vertex);

      psd                            = [MTLRenderPipelineDescriptor new];
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
      psd.vertexFunction             = [_library newFunctionWithName:@"basic_vertex_proj_tex"];
      psd.fragmentFunction           = [_library newFunctionWithName:@"basic_fragment_proj_tex"];

      if (!psd.vertexFunction || !psd.fragmentFunction)
      {
         RARCH_ERR("[Metal] Failed to load main pipeline shader functions.\n");
         return NO;
      }

      _t_pipelineState = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
      if (err != nil)
      {
         RARCH_ERR("[Metal] Error creating pipeline state %s.\n", err.localizedDescription.UTF8String);
         return NO;
      }

      psd.label               = @"Pipeline+No Alpha";
      ca.blendingEnabled      = NO;
      _t_pipelineStateNoAlpha = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
      if (err != nil)
      {
         RARCH_ERR("[Metal] Error creating pipeline state (no alpha) %s.\n", err.localizedDescription.UTF8String);
         return NO;
      }
   }

   {
      MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
      _samplerStateNearest     = [_device newSamplerStateWithDescriptor:sd];

      sd.minFilter             = MTLSamplerMinMagFilterLinear;
      sd.magFilter             = MTLSamplerMinMagFilterLinear;
      _samplerStateLinear      = [_device newSamplerStateWithDescriptor:sd];
   }

   return YES;
}

- (void)setViewportWidth:(unsigned)width height:(unsigned)height forceFull:(BOOL)forceFull allowRotate:(BOOL)allowRotate
{
   _viewport->full_width   = width;
   _viewport->full_height  = height;
   video_driver_set_output_size(_viewport->full_width, _viewport->full_height);
   _layer.drawableSize     = CGSizeMake(width, height);
   video_driver_update_viewport(_viewport, forceFull, _keepAspect, YES);
   _context.viewport       = _viewport; /* Update matrix */
   _viewportMVP.outputSize = simd_make_float2(_viewport->full_width, _viewport->full_height);

#if METAL_HDR_AVAILABLE
   /* Keep the HDR offscreens matched to the drawable size on resize.
    * Cheap no-op when already the right size. */
   if (_context.hdrEnabled)
      [_context resizeHDRResourcesForWidth:width height:height];
#endif
}

#pragma mark - video

- (void)setVideo:(const video_info_t *)video { }

- (bool)renderFrame:(const void *)frame
               data:(void*)data
              width:(unsigned)width
             height:(unsigned)height
         frameCount:(uint64_t)frameCount
              pitch:(unsigned)pitch
                msg:(const char *)msg
               info:(video_frame_info_t *)video_info
{
   @autoreleasepool
   {
      bool statistics_show = video_info->statistics_show;
      bool menu_is_alive   = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
      id<MTLRenderCommandEncoder> rce;
      bool hdrOn           = _context.hdrEnabled;

      /* Begin frame: re-evaluate viewport, push to context if it changed,
       * then begin the command buffer / render pass. */
      {
         video_viewport_t vp = *_viewport;
         video_driver_update_viewport(_viewport, NO, _keepAspect, YES);
         if (memcmp(&vp, _viewport, sizeof(vp)) != 0)
            _context.viewport = _viewport;
         [_context begin];
      }

      _frameView.frameCount = frameCount;
      if (frame && width && height)
      {
         _frameView.size      = CGSizeMake(width, height);
         [_frameView updateFrame:frame pitch:pitch];
         _frameEverUploaded   = true;
      }
      else if (!_frameEverUploaded)
      {
         /* Driver just came up (init or post-reinit).  The frontend
          * hasn't delivered a real frame yet and — on CMD_EVENT_REINIT
          * from inside the menu — likely won't until the user leaves
          * the menu.  Pull the cached frame so the core image reappears
          * under the menu immediately instead of waiting for an F1
          * cycle.  Safe to do every frame while the flag is clear; we
          * latch it after the first successful upload. */
         video_driver_state_t *video_st = video_state_get_ptr();
         if (     video_st
               && video_st->frame_cache_data
               && video_st->frame_cache_data != RETRO_HW_FRAME_BUFFER_VALID
               && video_st->frame_cache_width
               && video_st->frame_cache_height
               && video_st->frame_cache_pitch)
         {
            _frameView.size    = CGSizeMake(video_st->frame_cache_width,
                                            video_st->frame_cache_height);
            [_frameView updateFrame:video_st->frame_cache_data
                              pitch:video_st->frame_cache_pitch];
            _frameEverUploaded = true;
         }
      }

      /* Acquire the frame encoder.  In SDR mode this lazily opens a pass
       * on the drawable (BGRA8).  In HDR mode it lazily opens a pass on
       * the BGRA8 SDR overlay offscreen — menu / overlay / OSD / widgets
       * all render there, and the HDR composite at end-of-frame reads the
       * overlay texture, alpha-blends it over the encoded core, and
       * writes the result into the HDR drawable. */
      rce = _context.rce;

      /* Draw core: back buffer + optional encoder pass. */
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
      if (!hdrOn && (_frameView.drawState & ViewDrawStateEncoder) != 0)
      {
         [rce setRenderPipelineState:_t_pipelineStateNoAlpha];
         if (_frameView.filter == RTextureFilterNearest)
            [rce setFragmentSamplerState:_samplerStateNearest atIndex:SamplerIndexDraw];
         else
            [rce setFragmentSamplerState:_samplerStateLinear atIndex:SamplerIndexDraw];
         [_frameView drawWithEncoder:rce];
      }

      /* Draw menu: textured menu frame if present, otherwise delegate to
       * the menu driver for widget-based menus. */
      if (_menu.enabled)
      {
         if (_menu.hasFrame)
         {
            [_menu.view drawWithContext:_context];
            [rce setRenderPipelineState:_t_pipelineState];
            if (_menu.view.filter == RTextureFilterNearest)
               [rce setFragmentSamplerState:_samplerStateNearest atIndex:SamplerIndexDraw];
            else
               [rce setFragmentSamplerState:_samplerStateLinear atIndex:SamplerIndexDraw];
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
      if (_overlay.enabled)
      {
         [_context resetRenderViewport:_overlay.fullscreen ? kFullscreenViewport : kVideoViewport];
         [rce setRenderPipelineState:[_context getStockShader:VIDEO_SHADER_STOCK_BLEND blend:YES]];
         [rce setFragmentSamplerState:_samplerStateLinear atIndex:SamplerIndexDraw];
         [_overlay drawWithEncoder:rce];
      }
#endif

      /* Only show statistics when menu is not visible and content is running */
      if (statistics_show && frame && width && height && !menu_is_alive)
      {
         struct font_params *osd_params = (struct font_params *)&video_info->osd_stat_params;
         if (osd_params)
            font_driver_render_msg(data, video_info->stat_text, osd_params, NULL);
      }

#ifdef HAVE_GFX_WIDGETS
      if (video_info->widgets_active)
         gfx_widgets_frame(video_info);
#endif

      /* Render on-screen message: optional background quad + text. */
      if (msg && *msg)
      {
         settings_t *settings    = config_get_ptr();
         bool msg_bgcolor_enable = settings->bools.video_msg_bgcolor_enable;

         if (msg_bgcolor_enable)
         {
            int msg_width         = font_driver_get_message_width(NULL,
                  msg, strlen(msg), 1.0f);
            float font_size       = settings->floats.video_font_size;
            unsigned bgcolor_red  = settings->uints.video_msg_bgcolor_red;
            unsigned bgcolor_green= settings->uints.video_msg_bgcolor_green;
            unsigned bgcolor_blue = settings->uints.video_msg_bgcolor_blue;
            float bgcolor_opacity = settings->floats.video_msg_bgcolor_opacity;
            float x               = settings->floats.video_msg_pos_x;
            float y               = 1.0f - settings->floats.video_msg_pos_y;
            float bg_w            = msg_width / (float)_viewport->full_width;
            float bg_h            = font_size / (float)_viewport->full_height;
            float x2              = 0.005f; /* extend background around text */
            float y2              = 0.005f;
            float r               = bgcolor_red   / 255.0f;
            float g               = bgcolor_green / 255.0f;
            float b               = bgcolor_blue  / 255.0f;
            float a               = bgcolor_opacity;

            y                    -= bg_h;
            x                    -= x2;
            y                    -= y2;
            bg_w                 += x2;
            bg_h                 += y2;

            [_context resetRenderViewport:kFullscreenViewport];
            [_context drawQuadX:x y:y w:bg_w h:bg_h r:r g:g b:b a:a];
         }

         font_driver_render_msg(data, msg, NULL, NULL);
      }

      /* End-of-frame HDR composite.  Menu / overlay / OSD / widgets have
       * rendered into the BGRA8 SDR overlay offscreen (_sdrOverlayTex);
       * the core source is either shaderOutputTexture (shader chain
       * active) or frameTexture (no-shader path), or nil (no core frame
       * yet).  Composite touches the drawable unconditionally to avoid
       * presenting uninitialised swapchain memory — when src is nil, a
       * clear-only pass runs in place of the core encode. */
      if (hdrOn)
      {
         const HDRUniforms *u  = _context.currentHDRUniforms;
         id<MTLTexture>    src = _frameView.shaderOutputTexture;
         if (!src)
            src                = _frameView.frameTexture;
         [_context hdrComposite:u fromSource:src];
      }

      [self _endFrame];
   }

   return YES;
}

- (void)_endFrame { [_context end]; }
/* TODO/FIXME (sgc): resize*/
- (void)setNeedsResize { }
- (void)setRotation:(unsigned)rotation { [_context setRotation:rotation]; }
- (Uniforms *)viewportMVP { return &_viewportMVP; }

#pragma mark - MTKViewDelegate

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size
{
#ifdef HAVE_COCOATOUCH
    CGFloat scale = [[UIScreen mainScreen] scale];
    [self setViewportWidth:(unsigned int)view.bounds.size.width*scale height:(unsigned int)view.bounds.size.height*scale forceFull:NO allowRotate:YES];
#else
   [self setViewportWidth:(unsigned int)size.width height:(unsigned int)size.height forceFull:NO allowRotate:YES];
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
      _context = context;
   return self;
}

- (bool)hasFrame { return _view != nil; }

- (void)setEnabled:(bool)enabled
{
   if (_enabled == enabled)
      return;
   _enabled      = enabled;
   _view.visible = enabled;
}

- (bool)enabled { return _enabled; }

- (void)updateWidth:(int)width
             height:(int)height
             format:(RPixelFormat)format
             filter:(RTextureFilter)filter
{
   CGSize size = CGSizeMake(width, height);

   if (_view)
   {
      if (!(CGSizeEqualToSize(_view.size, size) &&
            _view.format == format &&
            _view.filter == filter))
         _view = nil;
   }

   if (!_view)
   {
      ViewDescriptor *vd = [ViewDescriptor new];
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

   struct
   {
      texture_t texture[GFX_MAX_FRAME_HISTORY + 1];
      MTLViewport viewport;
      float4_t output_size;
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
      pass_semantics_t semantics;
      MTLViewport viewport;
      __unsafe_unretained id<MTLRenderPipelineState> _state;
   } pass[GFX_MAX_SHADERS];

   texture_t luts[GFX_MAX_TEXTURES];

} engine_t;

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

   id<MTLSamplerState> _samplers[RARCH_FILTER_MAX][RARCH_WRAP_MAX];
   struct video_shader *_shader;

   engine_t _engine;

   bool resize_render_targets;
   bool init_history;
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
      _context              = c;
      _format               = d.format;
      _bpp                  = RPixelFormatToBPP(_format);
      _filter               = d.filter;
      if (_format == RPixelFormatBGRA8Unorm || _format == RPixelFormatBGRX8Unorm)
         _drawState         = ViewDrawStateEncoder;
      else
         _drawState         = ViewDrawStateAll;
      _visible              = YES;
      _engine.mvp           = matrix_proj_ortho(0, 1, 0, 1);
      [self _initSamplers];

      self.size             = d.size;
      self.frame            = CGRectMake(0, 0, 1, 1);
      resize_render_targets = YES;

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

- (void)_initSamplers
{
   int i;
   MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];

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

      id<MTLSamplerState> ss = [_context.device newSamplerStateWithDescriptor:sd];
      _samplers[RARCH_FILTER_LINEAR][i] = ss;

      sd.minFilter           = MTLSamplerMinMagFilterNearest;
      sd.magFilter           = MTLSamplerMinMagFilterNearest;

      ss                     = [_context.device newSamplerStateWithDescriptor:sd];
      _samplers[RARCH_FILTER_NEAREST][i] = ss;
   }
}

- (void)setFilteringIndex:(int)index smooth:(bool)smooth
{
   int i;
   for (i = 0; i < RARCH_WRAP_MAX; i++)
   {
      if (smooth)
         _samplers[RARCH_FILTER_UNSPEC][i] = _samplers[RARCH_FILTER_LINEAR][i];
      else
         _samplers[RARCH_FILTER_UNSPEC][i] = _samplers[RARCH_FILTER_NEAREST][i];
   }
}

- (void)setSize:(CGSize)size
{
   if (CGSizeEqualToSize(_size, size))
      return;

   _size                 = size;
   resize_render_targets = YES;

   if (   _format != RPixelFormatBGRA8Unorm
       && _format != RPixelFormatBGRX8Unorm)
   {
      MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR16Uint
                                 width:(NSUInteger)size.width
                                 height:(NSUInteger)size.height
                                 mipmapped:NO];
      _src = [_context.device newTextureWithDescriptor:td];
   }
}

- (CGSize)size { return _size; }

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

- (void)_updateHistory
{
   if (_shader)
   {
      if (_shader->history_size)
      {
         if (init_history)
         {
            int i;
            MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
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

   /* Either no history, or we moved a texture of a different size in the front slot */
   if (   _engine.frame.texture[0].size_data.x != _size.width
       || _engine.frame.texture[0].size_data.y != _size.height)
   {
      MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
               width:(NSUInteger)_size.width
               height:(NSUInteger)_size.height
               mipmapped:false];
      td.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
      [self _initTexture:&_engine.frame.texture[0] withDescriptor:td];
   }
}

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

- (void)updateFrame:(void const *)src pitch:(NSUInteger)pitch
{
   if (_shader && (_engine.frame.output_size.x != _viewport->width
               ||  _engine.frame.output_size.y != _viewport->height))
      resize_render_targets       = YES;

   _engine.frame.viewport.originX = _viewport->x;
   _engine.frame.viewport.originY = _viewport->y;
   _engine.frame.viewport.width   = _viewport->width;
   _engine.frame.viewport.height  = _viewport->height;
   _engine.frame.viewport.znear   = 0.0f;
   _engine.frame.viewport.zfar    = 1.0f;
   _engine.frame.output_size.x    = _viewport->width;
   _engine.frame.output_size.y    = _viewport->height;
   _engine.frame.output_size.z    = 1.0f / _viewport->width;
   _engine.frame.output_size.w    = 1.0f / _viewport->height;

   if (resize_render_targets)
      [self _updateRenderTargets];

   [self _updateHistory];

   if (   _format == RPixelFormatBGRA8Unorm
       || _format == RPixelFormatBGRX8Unorm)
   {
      id<MTLTexture> tex = _engine.frame.texture[0].view;
      [tex replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)_size.width, (NSUInteger)_size.height)
             mipmapLevel:0 withBytes:src
             bytesPerRow:pitch];
   }
   else
   {
      [_src replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)_size.width, (NSUInteger)_size.height)
              mipmapLevel:0 withBytes:src
              bytesPerRow:(NSUInteger)(pitch)];
      _srcDirty = YES;
   }
}

- (void)_initTexture:(texture_t *)t withDescriptor:(MTLTextureDescriptor *)td
{
   STRUCT_ASSIGN(t->view, [_context.device newTextureWithDescriptor:td]);
   t->size_data.x = td.width;
   t->size_data.y = td.height;
   t->size_data.z = 1.0f / td.width;
   t->size_data.w = 1.0f / td.height;
}

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

- (void)drawWithContext:(Context *)ctx
{
   size_t i;
   _texture = _engine.frame.texture[0].view;

   if (     (_format != RPixelFormatBGRA8Unorm)
         && (_format != RPixelFormatBGRX8Unorm)
         && _srcDirty)
   {
      [_context convertFormat:_format from:_src to:_texture];
      _srcDirty = NO;
   }

   if (!_shader || _shader->passes == 0)
      return;

   for (i = 0; i < _shader->passes; i++)
   {
      if (_shader->pass[i].feedback)
      {
         texture_t tmp            = _engine.pass[i].feedback;
         _engine.pass[i].feedback = _engine.pass[i].rt;
         _engine.pass[i].rt       = tmp;
      }
   }

   id<MTLCommandBuffer> cb = ctx.blitCommandBuffer;

   MTLRenderPassDescriptor *rpd        = [MTLRenderPassDescriptor new];
   rpd.colorAttachments[0].loadAction  = MTLLoadActionDontCare;
   rpd.colorAttachments[0].storeAction = MTLStoreActionStore;

   for (i = 0; i < _shader->passes; i++)
   {
      int j;
      __unsafe_unretained id<MTLTexture> textures[SLANG_NUM_BINDINGS] = {NULL};
      id<MTLSamplerState> samplers[SLANG_NUM_BINDINGS] = {NULL};
      id<MTLRenderCommandEncoder> rce                  = nil;
      BOOL backBuffer                                  = (_engine.pass[i].rt.view == nil);

      if (backBuffer)
         rce = _context.rce;
      else
      {
         rpd.colorAttachments[0].texture = _engine.pass[i].rt.view;
         rce = [cb renderCommandEncoderWithDescriptor:rpd];
      }

      [rce setRenderPipelineState:_engine.pass[i]._state];

      NSURL *shaderPath = [NSURL fileURLWithPath:_engine.pass[i]._state.label];
      rce.label = shaderPath.lastPathComponent.stringByDeletingPathExtension;

      _engine.pass[i].frame_count = (uint32_t)_frameCount;
      if (_shader->pass[i].frame_count_mod)
         _engine.pass[i].frame_count %= _shader->pass[i].frame_count_mod;

#ifdef HAVE_REWIND
      if (state_manager_frame_is_reversed())
         _engine.pass[i].frame_direction = -1;
      else
#else
      _engine.pass[i].frame_direction    = 1;
#endif
      _engine.pass[i].frame_time_delta   = (uint32_t)video_driver_get_frame_time_delta_usec();
      _engine.pass[i].original_fps       = video_driver_get_original_fps();
      _engine.pass[i].rotation           = retroarch_get_rotation();
      _engine.pass[i].core_aspect        = video_driver_get_core_aspect();

      /* OriginalAspectRotated: return 1 / aspect for 90 and 270 rotated content */
      int rot                            = retroarch_get_rotation();
      float core_aspect_rot              = video_driver_get_core_aspect();
      if (rot == 1 || rot == 3)
         core_aspect_rot                 = 1 / core_aspect_rot;
      _engine.pass[i].core_aspect_rot    = core_aspect_rot;

      for (j = 0; j < SLANG_CBUFFER_MAX; j++)
      {
         id<MTLBuffer> buffer      = _engine.pass[i].buffers[j];
         cbuffer_sem_t *buffer_sem = &_engine.pass[i].semantics.cbuffers[j];

         if (buffer_sem->stage_mask && buffer_sem->uniforms)
         {
            void             *data = buffer.contents;
            uniform_sem_t *uniform = buffer_sem->uniforms;

            while (uniform->size)
            {
               if (uniform->data)
                  memcpy((uint8_t *)data + uniform->offset, uniform->data, uniform->size);
               uniform++;
            }

            if (buffer_sem->stage_mask & SLANG_STAGE_VERTEX_MASK)
               [rce setVertexBuffer:buffer offset:0 atIndex:buffer_sem->binding];

            if (buffer_sem->stage_mask & SLANG_STAGE_FRAGMENT_MASK)
               [rce setFragmentBuffer:buffer offset:0 atIndex:buffer_sem->binding];
#if !defined(HAVE_COCOATOUCH)
            [buffer didModifyRange:NSMakeRange(0, buffer.length)];
#endif
         }
      }

      texture_sem_t *texture_sem = _engine.pass[i].semantics.textures;
      while (texture_sem->stage_mask)
      {
         int binding        = texture_sem->binding;
         id<MTLTexture> tex = (__bridge id<MTLTexture>)*(void **)texture_sem->texture_data;
         textures[binding]  = tex;
         samplers[binding]  = _samplers[texture_sem->filter][texture_sem->wrap];
         texture_sem++;
      }

      if (backBuffer)
         [rce setViewport:_engine.frame.viewport];
      else
         [rce setViewport:_engine.pass[i].viewport];

      [rce setFragmentTextures:textures withRange:NSMakeRange(0, SLANG_NUM_BINDINGS)];
      [rce setFragmentSamplerStates:samplers withRange:NSMakeRange(0, SLANG_NUM_BINDINGS)];
      [rce setVertexBytes:_vertex length:sizeof(_vertex) atIndex:4];
      [rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];

      if (!backBuffer)
         [rce endEncoding];

      _texture = _engine.pass[i].rt.view;
   }

   if (_texture)
      _drawState = ViewDrawStateAll;
   else
      _drawState = ViewDrawStateContext;
}

- (void)_updateRenderTargets
{
   size_t i;
   NSUInteger width, height;
   if (!_shader || !resize_render_targets)
      return;

   /* Release existing targets */
   for (i = 0; i < _shader->passes; i++)
   {
      STRUCT_ASSIGN(_engine.pass[i].rt.view, nil);
      STRUCT_ASSIGN(_engine.pass[i].feedback.view, nil);
      memset(&_engine.pass[i].rt, 0, sizeof(_engine.pass[i].rt));
      memset(&_engine.pass[i].feedback, 0, sizeof(_engine.pass[i].feedback));
   }

   width  = (NSUInteger)_size.width;
   height = (NSUInteger)_size.height;

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
               width = (NSUInteger)(_viewport->width * shader_pass->fbo.scale_x);
               break;

            case RARCH_SCALE_ABSOLUTE:
               width = shader_pass->fbo.abs_x;
               break;

            default:
               break;
         }

         if (!width)
            width = _viewport->width;

         switch (shader_pass->fbo.type_y)
         {
            case RARCH_SCALE_INPUT:
               height *= shader_pass->fbo.scale_y;
               break;

            case RARCH_SCALE_VIEWPORT:
               height = (NSUInteger)(_viewport->height * shader_pass->fbo.scale_y);
               break;

            case RARCH_SCALE_ABSOLUTE:
               height = shader_pass->fbo.abs_y;
               break;

            default:
               break;
         }

         if (!height)
            height = _viewport->height;
      }
      else if (i == (_shader->passes - 1))
      {
         width  = _viewport->width;
         height = _viewport->height;
      }

      /* Updating framebuffer size */

      MTLPixelFormat fmt = SelectOptimalPixelFormat(glslang_format_to_metal(_engine.pass[i].semantics.format));
      /* When HDR is on, the last pass needs an explicit RT (the HDR
       * composite pass will later sample from it).  Force allocation
       * regardless of dimension/format match to the viewport.
       * We still honour the shader's semantics.format for LAST pass if
       * it's an explicit HDR format (RGBA16F or RGB10A2 == HDR10 PQ),
       * otherwise RGBA16F gives comfortable precision for the composite. */
      BOOL lastPass = (i == (_shader->passes - 1));
      BOOL forceAllocForHDR = NO;
      if (lastPass && _hdrEnabled)
      {
         forceAllocForHDR = YES;
         if (fmt != MTLPixelFormatRGBA16Float && fmt != MTLPixelFormatRGB10A2Unorm)
            fmt = MTLPixelFormatRGBA16Float;
      }

      if (   (!lastPass)
          || forceAllocForHDR
          || (width  != _viewport->width)
          || (height != _viewport->height)
          || fmt != MTLPixelFormatBGRA8Unorm)
      {
         _engine.pass[i].viewport.width  = width;
         _engine.pass[i].viewport.height = height;
         _engine.pass[i].viewport.znear  = 0.0;
         _engine.pass[i].viewport.zfar   = 1.0;

         MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:fmt
            width:width height:height mipmapped:false];
         td.storageMode = MTLStorageModePrivate;
         td.usage       = MTLTextureUsageShaderRead
                        | MTLTextureUsageRenderTarget;

         [self _initTexture:&_engine.pass[i].rt withDescriptor:td];

         if (shader_pass->feedback)
            [self _initTexture:&_engine.pass[i].feedback withDescriptor:td];
      }
      else
      {
         _engine.pass[i].rt.size_data.x = width;
         _engine.pass[i].rt.size_data.y = height;
         _engine.pass[i].rt.size_data.z = 1.0f / width;
         _engine.pass[i].rt.size_data.w = 1.0f / height;
      }
   }

   resize_render_targets = NO;
}

- (void)_freeVideoShader:(struct video_shader *)shader
{
   int i;
   if (!shader)
      return;

   for (i = 0; i < GFX_MAX_SHADERS; i++)
   {
      int j;
      STRUCT_ASSIGN(_engine.pass[i].rt.view, nil);
      STRUCT_ASSIGN(_engine.pass[i].feedback.view, nil);
      memset(&_engine.pass[i].rt, 0, sizeof(_engine.pass[i].rt));
      memset(&_engine.pass[i].feedback, 0, sizeof(_engine.pass[i].feedback));

      STRUCT_ASSIGN(_engine.pass[i]._state, nil);

      for (j = 0; j < SLANG_CBUFFER_MAX; j++)
      {
         STRUCT_ASSIGN(_engine.pass[i].buffers[j], nil);
      }
   }

   for (i = 0; i < GFX_MAX_TEXTURES; i++)
   {
      STRUCT_ASSIGN(_engine.luts[i].view, nil);
   }

   free(shader);
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
      texture_t *source = NULL;
      if (!video_shader_load_preset_into_shader(path.UTF8String, shader))
         return NO;

      source = &_engine.frame.texture[0];

      for (i = 0; i < shader->passes; source = &_engine.pass[i++].rt)
      {
         matrix_float4x4 *mvp = (i == shader->passes-1)
            ? &_context.uniforms->projectionMatrix
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
            }
         };
         /* clang-format on */

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
            MTLVertexDescriptor *vd      = [MTLVertexDescriptor new];
            vd.attributes[0].offset      = offsetof(VertexSlang, position);
            vd.attributes[0].format      = MTLVertexFormatFloat4;
            vd.attributes[0].bufferIndex = 4;
            vd.attributes[1].offset      = offsetof(VertexSlang, texCoord);
            vd.attributes[1].format      = MTLVertexFormatFloat2;
            vd.attributes[1].bufferIndex = 4;
            vd.layouts[4].stride         = sizeof(VertexSlang);
            vd.layouts[4].stepFunction   = MTLVertexStepFunctionPerVertex;

            MTLRenderPipelineDescriptor *psd = [MTLRenderPipelineDescriptor new];

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
               if (   _engine.pass[i].semantics.format == SLANG_FORMAT_A2B10G10R10_UNORM_PACK32
                   || _engine.pass[i].semantics.format == SLANG_FORMAT_A2B10G10R10_UINT_PACK32)
                  passEmitsHDR10 = YES;
               else if (_engine.pass[i].semantics.format == SLANG_FORMAT_R16G16B16A16_SFLOAT)
                  passEmitsHDR16 = YES;

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

            id<MTLLibrary>             lib = [_context.device newLibraryWithSource:vs_src options:nil error:&err];
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

            psd.vertexFunction = [lib newFunctionWithName:@"main0"];

            lib = [_context.device newLibraryWithSource:fs_src options:nil error:&err];
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
            psd.fragmentFunction = [lib newFunctionWithName:@"main0"];

            STRUCT_ASSIGN(_engine.pass[i]._state,
                          [_context.device newRenderPipelineStateWithDescriptor:psd error:&err]);
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

                id<MTLBuffer> buf = [_context.device newBufferWithLength:size options:PLATFORM_METAL_RESOURCE_STORAGE_MODE];
               STRUCT_ASSIGN(_engine.pass[i].buffers[j], buf);
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

      for (i = 0; i < shader->luts; i++)
      {
         struct texture_image image;
         image.pixels               = NULL;
         image.width                = 0;
         image.height               = 0;
         image.supports_rgba        = true;

         if (!image_texture_load(&image, shader->lut[i].path))
            return NO;

         MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
             width:image.width height:image.height
             mipmapped:shader->lut[i].mipmap];
         td.usage                 = MTLTextureUsageShaderRead;
         [self _initTexture:&_engine.luts[i] withDescriptor:td];

         [_engine.luts[i].view replaceRegion:MTLRegionMake2D(0, 0, image.width, image.height)
                                 mipmapLevel:0 withBytes:image.pixels
                                 bytesPerRow:4 * image.width];

         /* TODO/FIXME (sgc): generate mip maps */
         image_texture_free(&image);
      }
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
      _context = context;
   return self;
}

- (bool)loadImages:(const struct texture_image *)images count:(NSUInteger)count
{
   size_t i;
   [self _freeImages];

   _images = [NSMutableArray arrayWithCapacity:count];

   NSUInteger needed = sizeof(SpriteVertex) * count * 4;
   if (!_vert || _vert.length < needed)
      _vert = [_context.device newBufferWithLength:needed options:PLATFORM_METAL_RESOURCE_STORAGE_MODE];

   for (i = 0; i < count; i++)
   {
      _images[i] = [_context newTexture:images[i] mipmapped:NO];
      [self updateVertexX:0 y:0 w:1 h:1 index:i];
      [self updateTextureCoordsX:0 y:0 w:1 h:1 index:i];
      [self _updateColorRed:1.0 green:1.0 blue:1.0 alpha:1.0 index:i];
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

- (SpriteVertex *)_getForIndex:(NSUInteger)index
{
   SpriteVertex *pv = (SpriteVertex *)_vert.contents;
   return &pv[index * 4];
}

- (void)_updateColorRed:(float)r green:(float)g blue:(float)b alpha:(float)a index:(NSUInteger)index
{
   simd_float4 color = simd_make_float4(r, g, b, a);
   SpriteVertex *pv  = [self _getForIndex:index];
   pv[0].color       = color;
   pv[1].color       = color;
   pv[2].color       = color;
   pv[3].color       = color;
   _vertDirty        = YES;
}

- (void)updateAlpha:(float)alpha index:(NSUInteger)index
{
   [self _updateColorRed:1.0 green:1.0 blue:1.0 alpha:alpha index:index];
}

- (void)updateVertexX:(float)x y:(float)y w:(float)w h:(float)h index:(NSUInteger)index
{
   SpriteVertex *pv = [self _getForIndex:index];
   pv[0].position   = simd_make_float2(x, y);
   pv[1].position   = simd_make_float2(x + w, y);
   pv[2].position   = simd_make_float2(x, y + h);
   pv[3].position   = simd_make_float2(x + w, y + h);
   _vertDirty       = YES;
}

- (void)updateTextureCoordsX:(float)x y:(float)y w:(float)w h:(float)h index:(NSUInteger)index
{
   SpriteVertex *pv = [self _getForIndex:index];
   pv[0].texCoord   = simd_make_float2(x, y);
   pv[1].texCoord   = simd_make_float2(x + w, y);
   pv[2].texCoord   = simd_make_float2(x, y + h);
   pv[3].texCoord   = simd_make_float2(x + w, y + h);
   _vertDirty       = YES;
}

- (void)_freeImages { _images = nil; }

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
   MetalDriver *md = (__bridge MetalDriver *)metal_ctx_data;
   if (md)
      [md.context swapBuffers];
}

static bool metal_set_shader(void *data,
      enum rarch_shader_type type, const char *path);

static void *metal_init(
      const video_info_t *video,
      input_driver_t **input,
      void **input_data)
{
   MetalDriver *md = nil;

   [apple_platform setViewType:APPLE_VIEW_TYPE_METAL];

   md = [[MetalDriver alloc] initWithVideo:video input:input inputData:input_data];
   if (md == nil)
      return NULL;

   /* Store reference for context swap_buffers calls */
   metal_ctx_data = (__bridge void *)md;

   return (__bridge_retained void *)md;
}

/* Flag to prevent recursive shader_subframes calls */
static bool metal_subframe_lock = false;

/* Flag and value for swap_interval emulation (like Vulkan) */
static bool metal_swap_interval_lock = false;
static unsigned metal_swap_interval = 1;

static bool metal_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height,
      uint64_t frame_count,
      unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
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
    * rendering. This is critical for proper 120Hz on ProMotion displays. */
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
         /* Re-render and present with NULL frame data (reuse previous frame) */
         if (!metal_frame(data, NULL, 0, 0, frame_count, 0, msg, video_info))
         {
            metal_subframe_lock = false;
            return false;
         }
      }
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
         if (!metal_frame(data, NULL, 0, 0, frame_count, 0, msg, video_info))
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
   metal_swap_interval = swap_interval;
}

static bool metal_alive(void *data) { return true; }
static bool metal_has_windowed(void *data) { return true; }
static bool metal_focus(void *data) { return apple_platform.hasFocus; }

static bool metal_suppress_screensaver(void *data, bool disable)
{
   return [apple_platform setDisableDisplaySleep:disable];
}

static bool metal_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
   {
      if (type != RARCH_SHADER_SLANG)
      {
         if (path && *path && type != RARCH_SHADER_SLANG)
            RARCH_WARN("[Metal] Only Slang shaders are supported. Falling back to stock.\n");
         path = NULL;
      }

      if (!path || !*path)
      {
         [md.frameView clearShader];
         return true;
      }

      if ([md.frameView setShaderFromPath:[NSString stringWithUTF8String:path]])
         return true;
   }
#endif
   return false;
}

static void metal_free(void *data)
{
   __attribute__((unused)) MetalDriver *md = (__bridge_transfer MetalDriver *)data;
   metal_ctx_data = NULL;
   md = nil;
}

static void metal_set_viewport(void *data, unsigned vp_width, unsigned vp_height,
      bool force_full, bool allow_rotate)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
      [md setViewportWidth:vp_width height:vp_height forceFull:force_full allowRotate:allow_rotate];
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
   MetalDriver *md = (__bridge MetalDriver *)data;
   return [md.frameView readViewport:buffer isIdle:is_idle];
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
static uintptr_t metal_load_texture_internal(void *video_data, void *data,
      enum texture_filter_type filter_type)
{
   MetalDriver           *md  = (__bridge MetalDriver *)video_data;
   struct texture_image *img  = (struct texture_image *)data;
   if (!img)
      return 0;

   struct texture_image image = *img;
   Texture *t = [md.context newTexture:image filter:filter_type];
   return (uintptr_t)(__bridge_retained void *)(t);
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
   /* When threaded video is active, dispatch to the video
    * thread so Context.blitCommandBuffer access is serialised
    * with the video thread's frame-end commit.  This is only
    * required for mipmapped filters (which encode into the
    * blit buffer), but we dispatch unconditionally for
    * consistency and simplicity. */
   if (threaded)
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
    * so ARC-releasing the handle here does not free the
    * underlying MTLTexture if the video thread's command
    * buffer is still using it -- the Metal runtime refcounts
    * resources across CPU and GPU.  No cross-thread
    * serialisation needed for unload. */
   __attribute__((unused)) Texture *t = (__bridge_transfer Texture *)(void *)handle;
   t = nil;
}

/* TODO/FIXME - implement */
static void metal_set_video_mode(void *data,
                                 unsigned width, unsigned height,
                                 bool fullscreen)
{
   RARCH_DBG("[Metal] set_video_mode res=%dx%d fullscreen=%s\n",
             width, height,
             fullscreen ? "YES" : "NO");
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
      bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   MetalDriver *md         = (__bridge MetalDriver *)data;
   settings_t *settings;
   bool menu_linear_filter;
   if (!md)
      return;
   settings                = config_get_ptr();
   menu_linear_filter      = settings->bools.menu_linear_filter;

   [md.menu updateWidth:width
                 height:height
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

static void metal_show_mouse(void *data, bool state)
{
   [apple_platform setCursorVisible:state];
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
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);

#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif

   return flags;
}

static void metal_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *desc, size_t desc_len)
{
   /* Body consolidated into cocoa_common.m.  Kept as a named
    * poke entry because video_thread_wrapper.c's
    * thread_get_video_output_size calls poke->get_video_output_size
    * directly, bypassing dispserv_apple. */
   cocoa_get_video_output_size(width, height, desc, desc_len);
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
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   metal_set_hdr_menu_nits,
   metal_set_hdr_paper_white_nits,
   metal_set_hdr_expand_gamut,
   metal_set_hdr_scanlines,
   metal_set_hdr_subpixel_layout
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
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   metal_get_overlay_interface,
#endif
   metal_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   metal_widgets_enabled
#endif
};
