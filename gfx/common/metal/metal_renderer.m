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

#include <retro_assert.h>

#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>

#import "metal_common.h"
#import "metal_shader_types.h"

#ifdef HAVE_MENU
#include "../../../menu/menu_driver.h"
#endif

#import "../metal_common.h"
#include "../../verbosity.h"

/*
 * COMMON
 */

static NSString *RPixelStrings[RPixelFormatCount];

NSUInteger RPixelFormatToBPP(RPixelFormat format)
{
   switch (format)
   {
      case RPixelFormatBGRA8Unorm:
      case RPixelFormatBGRX8Unorm:
         return 4;

      case RPixelFormatB5G6R5Unorm:
      case RPixelFormatBGRA4Unorm:
         return 2;

      default:
         RARCH_ERR("[Metal]: unknown RPixel format: %d\n", format);
         return 4;
   }
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
   {
      format = RPixelFormatInvalid;
   }

   return RPixelStrings[format];
}

matrix_float4x4 make_matrix_float4x4(const float *v)
{
   simd_float4 P = simd_make_float4(v[0], v[1], v[2], v[3]);
   v += 4;
   simd_float4 Q = simd_make_float4(v[0], v[1], v[2], v[3]);
   v += 4;
   simd_float4 R = simd_make_float4(v[0], v[1], v[2], v[3]);
   v += 4;
   simd_float4 S = simd_make_float4(v[0], v[1], v[2], v[3]);

   matrix_float4x4 mat = {P, Q, R, S};
   return mat;
}

matrix_float4x4 matrix_proj_ortho(float left, float right, float top, float bottom)
{
   float near = 0;
   float far = 1;

   float sx = 2 / (right - left);
   float sy = 2 / (top - bottom);
   float sz = 1 / (far - near);
   float tx = (right + left) / (left - right);
   float ty = (top + bottom) / (bottom - top);
   float tz = near / (far - near);

   simd_float4 P = simd_make_float4(sx, 0, 0, 0);
   simd_float4 Q = simd_make_float4(0, sy, 0, 0);
   simd_float4 R = simd_make_float4(0, 0, sz, 0);
   simd_float4 S = simd_make_float4(tx, ty, tz, 1);

   matrix_float4x4 mat = {P, Q, R, S};
   return mat;
}

matrix_float4x4 matrix_rotate_z(float rot)
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
@end

@implementation Context
{
   dispatch_semaphore_t _inflightSemaphore;
   id<MTLCommandQueue> _commandQueue;
   CAMetalLayer *_layer;
   id<CAMetalDrawable> _drawable;
   video_viewport_t _viewport;
   id<MTLSamplerState> _samplers[TEXTURE_FILTER_MIPMAP_NEAREST + 1];
   Filter *_filters[RPixelFormatCount]; // convert to bgra8888

   // main render pass state
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
}

- (instancetype)initWithDevice:(id<MTLDevice>)d
                         layer:(CAMetalLayer *)layer
                       library:(id<MTLLibrary>)l
{
   if (self = [super init])
   {
      _inflightSemaphore         = dispatch_semaphore_create(MAX_INFLIGHT);
      _device                    = d;
      _layer                     = layer;
#ifdef OSX
      _layer.framebufferOnly     = NO;
      _layer.displaySyncEnabled  = YES;
#endif
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

      for (int i = 0; i < CHAIN_LENGTH; i++)
      {
         _chain[i] = [[BufferChain alloc] initWithDevice:_device blockLen:65536];
      }
   }
   return self;
}

- (video_viewport_t *)viewport
{
   return &_viewport;
}

- (void)setViewport:(video_viewport_t *)viewport
{
   _viewport = *viewport;
   _uniforms.outputSize = simd_make_float2(_viewport.full_width, _viewport.full_height);
}

- (Uniforms *)uniforms
{
   return &_uniforms;
}

- (void)setRotation:(unsigned)rotation
{
   _rotation = 270 * rotation;

   /* Calculate projection. */
   _mvp_no_rot = matrix_proj_ortho(0, 1, 0, 1);

   bool allow_rotate = true;
   if (!allow_rotate)
   {
      _mvp = _mvp_no_rot;
      return;
   }

   matrix_float4x4 rot = matrix_rotate_z((float)(M_PI * _rotation / 180.0f));
   _mvp = simd_mul(rot, _mvp_no_rot);

   _uniforms.projectionMatrix = _mvp;
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
   vd.layouts[0].stride = sizeof(SpriteVertex);
   return vd;
}

- (bool)_initClearState
{
   MTLVertexDescriptor *vd = [self _spriteVertexDescriptor];
   MTLRenderPipelineDescriptor *psd = [MTLRenderPipelineDescriptor new];
   psd.label = @"clear_state";

   MTLRenderPipelineColorAttachmentDescriptor *ca = psd.colorAttachments[0];
   ca.pixelFormat = _layer.pixelFormat;

   psd.vertexDescriptor = vd;
   psd.vertexFunction = [_library newFunctionWithName:@"stock_vertex"];
   psd.fragmentFunction = [_library newFunctionWithName:@"stock_fragment_color"];

   NSError *err;
   _clearState = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal]: error creating clear pipeline state %s\n", err.localizedDescription.UTF8String);
      return NO;
   }

   return YES;
}

- (bool)_initMenuStates
{
   MTLVertexDescriptor *vd = [self _spriteVertexDescriptor];
   MTLRenderPipelineDescriptor *psd = [MTLRenderPipelineDescriptor new];
   psd.label = @"stock";

   MTLRenderPipelineColorAttachmentDescriptor *ca = psd.colorAttachments[0];
   ca.pixelFormat = _layer.pixelFormat;
   ca.blendingEnabled = NO;
   ca.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
   ca.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
   ca.sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
   ca.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

   psd.sampleCount = 1;
   psd.vertexDescriptor = vd;
   psd.vertexFunction = [_library newFunctionWithName:@"stock_vertex"];
   psd.fragmentFunction = [_library newFunctionWithName:@"stock_fragment"];

   NSError *err;
   _states[VIDEO_SHADER_STOCK_BLEND][0] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal]: error creating pipeline state %s\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label = @"stock_blend";
   ca.blendingEnabled = YES;
   _states[VIDEO_SHADER_STOCK_BLEND][1] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal]: error creating pipeline state %s\n", err.localizedDescription.UTF8String);
      return NO;
   }

   MTLFunctionConstantValues *vals;

   psd.label = @"snow_simple";
   ca.blendingEnabled = YES;
   {
      vals = [MTLFunctionConstantValues new];
      float values[3] = {
         1.25f,   // baseScale
         0.50f,   // density
         0.15f,   // speed
      };
      [vals setConstantValue:&values[0] type:MTLDataTypeFloat withName:@"snowBaseScale"];
      [vals setConstantValue:&values[1] type:MTLDataTypeFloat withName:@"snowDensity"];
      [vals setConstantValue:&values[2] type:MTLDataTypeFloat withName:@"snowSpeed"];
   }
   psd.fragmentFunction = [_library newFunctionWithName:@"snow_fragment" constantValues:vals error:&err];
   _states[VIDEO_SHADER_MENU_3][1] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal]: error creating pipeline state %s\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label = @"snow";
   ca.blendingEnabled = YES;
   {
      vals = [MTLFunctionConstantValues new];
      float values[3] = {
         3.50f,   // baseScale
         0.70f,   // density
         0.25f,   // speed
      };
      [vals setConstantValue:&values[0] type:MTLDataTypeFloat withName:@"snowBaseScale"];
      [vals setConstantValue:&values[1] type:MTLDataTypeFloat withName:@"snowDensity"];
      [vals setConstantValue:&values[2] type:MTLDataTypeFloat withName:@"snowSpeed"];
   }
   psd.fragmentFunction = [_library newFunctionWithName:@"snow_fragment" constantValues:vals error:&err];
   _states[VIDEO_SHADER_MENU_4][1] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal]: error creating pipeline state %s\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label = @"bokeh";
   ca.blendingEnabled = YES;
   psd.fragmentFunction = [_library newFunctionWithName:@"bokeh_fragment"];
   _states[VIDEO_SHADER_MENU_5][1] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal]: error creating pipeline state %s\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label = @"snowflake";
   ca.blendingEnabled = YES;
   psd.fragmentFunction = [_library newFunctionWithName:@"snowflake_fragment"];
   _states[VIDEO_SHADER_MENU_6][1] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal]: error creating pipeline state %s\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label = @"ribbon";
   ca.blendingEnabled = NO;
   psd.vertexFunction = [_library newFunctionWithName:@"ribbon_vertex"];
   psd.fragmentFunction = [_library newFunctionWithName:@"ribbon_fragment"];
   _states[VIDEO_SHADER_MENU][0] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal]: error creating pipeline state %s\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label = @"ribbon_blend";
   ca.blendingEnabled = YES;
   ca.sourceRGBBlendFactor = MTLBlendFactorOne;
   ca.destinationRGBBlendFactor = MTLBlendFactorOne;
   _states[VIDEO_SHADER_MENU][1] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal]: error creating pipeline state %s\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label = @"ribbon_simple";
   ca.blendingEnabled = NO;
   psd.vertexFunction = [_library newFunctionWithName:@"ribbon_simple_vertex"];
   psd.fragmentFunction = [_library newFunctionWithName:@"ribbon_simple_fragment"];
   _states[VIDEO_SHADER_MENU_2][0] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal]: error creating pipeline state %s\n", err.localizedDescription.UTF8String);
      return NO;
   }

   psd.label = @"ribbon_simple_blend";
   ca.blendingEnabled = YES;
   ca.sourceRGBBlendFactor = MTLBlendFactorOne;
   ca.destinationRGBBlendFactor = MTLBlendFactorOne;
   _states[VIDEO_SHADER_MENU_2][1] = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
   if (err != nil)
   {
      RARCH_ERR("[Metal]: error creating pipeline state %s\n", err.localizedDescription.UTF8String);
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
      RARCH_LOG("[Metal]: unable to create 'convert_bgra4444_to_bgra8888' conversion filter: %s\n",
                err.localizedDescription.UTF8String);
      return NO;
   }

   _filters[RPixelFormatB5G6R5Unorm] = [Filter newFilterWithFunctionName:@"convert_rgb565_to_bgra8888"
                                                                  device:_device
                                                                 library:_library
                                                                   error:&err];
   if (err)
   {
      RARCH_LOG("[Metal]: unable to create 'convert_rgb565_to_bgra8888' conversion filter: %s\n",
                err.localizedDescription.UTF8String);
      return NO;
   }

   return YES;
}

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

   Texture *tex = [Texture new];
   tex.texture = [self newTexture:image mipmapped:mipmapped];
   tex.sampler = _samplers[filter];

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
   {
      _drawable = _layer.nextDrawable;
   }
   return _drawable;
}

- (void)convertFormat:(RPixelFormat)fmt from:(id<MTLTexture>)src to:(id<MTLTexture>)dst
{
   assert(src.width == dst.width && src.height == dst.height);
   assert(fmt >= 0 && fmt < RPixelFormatCount);
   Filter *conv = _filters[fmt];
   assert(conv != nil);
   [conv apply:self.blitCommandBuffer in:src out:dst];
}

- (id<MTLCommandBuffer>)blitCommandBuffer
{
   if (!_blitCommandBuffer) {
      _blitCommandBuffer = [_commandQueue commandBuffer];
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

   _captureEnabled = captureEnabled;
   //_layer.framebufferOnly = !captureEnabled;
}

- (bool)captureEnabled
{
   return _captureEnabled;
}

- (bool)readBackBuffer:(uint8_t *)buffer
{
   if (!_captureEnabled || _backBuffer == nil)
      return NO;

   if (_backBuffer.pixelFormat != MTLPixelFormatBGRA8Unorm)
   {
      RARCH_WARN("[Metal]: unexpected pixel format %d\n", _backBuffer.pixelFormat);
      return NO;
   }

   uint8_t *tmp = malloc(_backBuffer.width * _backBuffer.height * 4);

   [_backBuffer getBytes:tmp
             bytesPerRow:4 * _backBuffer.width
              fromRegion:MTLRegionMake2D(0, 0, _backBuffer.width, _backBuffer.height)
             mipmapLevel:0];

   NSUInteger srcStride = _backBuffer.width * 4;
   uint8_t const *src = tmp + (_viewport.y * srcStride);

   NSUInteger dstStride = _viewport.width * 3;
   uint8_t *dst = buffer + (_viewport.height - 1) * dstStride;

   for (int y = 0; y < _viewport.height; y++, src += srcStride, dst -= dstStride)
   {
      for (int x = 0; x < _viewport.width; x++)
      {
         dst[3 * x + 0] = src[4 * (_viewport.x + x) + 0];
         dst[3 * x + 1] = src[4 * (_viewport.x + x) + 1];
         dst[3 * x + 2] = src[4 * (_viewport.x + x) + 2];
      }
   }

   free(tmp);

   return YES;
}

- (void)begin
{
   assert(_commandBuffer == nil);
   dispatch_semaphore_wait(_inflightSemaphore, DISPATCH_TIME_FOREVER);
   _commandBuffer = [_commandQueue commandBuffer];
   _commandBuffer.label = @"Frame command buffer";
   _backBuffer = nil;
}

- (id<MTLRenderCommandEncoder>)rce
{
   assert(_commandBuffer != nil);
   if (_rce == nil)
   {
      MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor new];
      rpd.colorAttachments[0].clearColor = _clearColor;
      rpd.colorAttachments[0].loadAction = MTLLoadActionClear;
      rpd.colorAttachments[0].texture = self.nextDrawable.texture;
      if (_captureEnabled)
      {
         _backBuffer = self.nextDrawable.texture;
      }
      _rce = [_commandBuffer renderCommandEncoderWithDescriptor:rpd];
      _rce.label = @"Frame command encoder";
   }
   return _rce;
}

- (void)resetRenderViewport:(ViewportResetMode)mode
{
   bool fullscreen = mode == kFullscreenViewport;
   MTLViewport vp = {
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
   assert(_commandBuffer != nil);

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
      // pending blits for mipmaps or render passes for slang shaders
      [_blitCommandBuffer commit];
      [_blitCommandBuffer waitUntilCompleted];
      _blitCommandBuffer = nil;
   }

   if (_rce)
   {
      [_rce endEncoding];
      _rce = nil;
   }

   __block dispatch_semaphore_t inflight = _inflightSemaphore;
   [_commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> _) {
      dispatch_semaphore_signal(inflight);
   }];

   if (self.nextDrawable)
   {
      [_commandBuffer presentDrawable:self.nextDrawable];
   }

   [_commandBuffer commit];

   _commandBuffer = nil;
   _drawable = nil;
   [self _nextChain];
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
   {
      _src = src;
   }
   return self;
}

@end

@implementation BufferChain
{
   id<MTLDevice> _device;
   NSUInteger _blockLen;
   BufferNode *_head;
   NSUInteger _offset; // offset into _current
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
      _device = device;
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
   _current   = _head;
   _offset    = 0;
   _allocated = 0;
}

- (bool)allocRange:(BufferRange *)range length:(NSUInteger)length
{
   MTLResourceOptions opts;
   opts = PLATFORM_METAL_RESOURCE_STORAGE_MODE;
   memset(range, 0, sizeof(*range));

   if (!_head)
   {
      _head = [[BufferNode alloc] initWithBuffer:[_device newBufferWithLength:_blockLen options:opts]];
      _length += _blockLen;
      _current = _head;
      _offset = 0;
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
   _offset = 0;
}

- (BOOL)_subAllocRange:(BufferRange *)range length:(NSUInteger)length
{
   NSUInteger nextOffset = _offset + length;
   if (nextOffset <= _current.src.length)
   {
      _current.allocated = nextOffset;
      _allocated += length;
      range->data = _current.src.contents + _offset;
      range->buffer = _current.src;
      range->offset = _offset;
      _offset = MTL_ALIGN_BUFFER(nextOffset);
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
   {
      return nil;
   }

   MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
   sd.minFilter = MTLSamplerMinMagFilterNearest;
   sd.magFilter = MTLSamplerMinMagFilterNearest;
   sd.sAddressMode = MTLSamplerAddressModeClampToEdge;
   sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
   sd.mipFilter = MTLSamplerMipFilterNotMipmapped;
   id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:sd];

   return [[Filter alloc] initWithKernel:kernel sampler:sampler];
}

- (instancetype)initWithKernel:(id<MTLComputePipelineState>)kernel sampler:(id<MTLSamplerState>)sampler
{
   if (self = [super init])
   {
      _kernel = kernel;
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

   MTLSize size = MTLSizeMake(16, 16, 1);
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

   MTLSize size = MTLSizeMake(32, 1, 1);
   MTLSize count = MTLSizeMake((tin.length + 00) / 32, 1, 1);

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

- (MTLPrimitiveType)_toPrimitiveType:(enum gfx_display_prim_type)prim
{
   switch (prim)
   {
      case GFX_DISPLAY_PRIM_TRIANGLESTRIP:
         return MTLPrimitiveTypeTriangleStrip;
      case GFX_DISPLAY_PRIM_TRIANGLES:
      default:
         /* Unexpected primitive type, defaulting to triangle */
         break;
   }

   return MTLPrimitiveTypeTriangle;
}

- (void)drawPipeline:(gfx_display_ctx_draw_t *)draw
{
   static struct video_coords blank_coords;

   draw->x = 0;
   draw->y = 0;
   draw->matrix_data = NULL;

   _uniforms.outputSize = simd_make_float2(_context.viewport->full_width, _context.viewport->full_height);

   draw->backend_data = &_uniforms;
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
         draw->prim_type       = GFX_DISPLAY_PRIM_TRIANGLESTRIP;
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
         [rce drawPrimitives:[self _toPrimitiveType:draw->prim_type] vertexStart:0 vertexCount:vertex_count];
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
   {
      _format = RPixelFormatBGRA8Unorm;
   }
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
                                     NSStringFromRPixelFormat(_format),
                                     sizeDesc];
}

@end

@implementation TexturedView
{
   Context *_context;
   id<MTLTexture> _texture; // optimal render texture
   Vertex _v[4];
   CGSize _size; // size of view in pixels
   CGRect _frame;
   NSUInteger _bpp;

   id<MTLTexture> _src;    // source texture
   bool _srcDirty;
}

- (instancetype)initWithDescriptor:(ViewDescriptor *)d context:(Context *)c
{
   self = [super init];
   if (self)
   {
      _format = d.format;
      _bpp = RPixelFormatToBPP(_format);
      _filter = d.filter;
      _context = c;
      _visible = YES;
      if (_format == RPixelFormatBGRA8Unorm || _format == RPixelFormatBGRX8Unorm)
      {
         _drawState = ViewDrawStateEncoder;
      }
      else
      {
         _drawState = ViewDrawStateAll;
      }
      self.size = d.size;
      self.frame = CGRectMake(0, 0, 1, 1);
   }
   return self;
}

- (void)setSize:(CGSize)size
{
   if (CGSizeEqualToSize(_size, size))
   {
      return;
   }

   _size = size;

   {
      MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                    width:(NSUInteger)size.width
                                                                                   height:(NSUInteger)size.height
                                                                                mipmapped:NO];
      td.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
      _texture = [_context.device newTextureWithDescriptor:td];
   }

   if (_format != RPixelFormatBGRA8Unorm && _format != RPixelFormatBGRX8Unorm)
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
   {
      return;
   }

   _frame = frame;

   float l = (float)CGRectGetMinX(frame);
   float t = (float)CGRectGetMinY(frame);
   float r = (float)CGRectGetMaxX(frame);
   float b = (float)CGRectGetMaxY(frame);

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

- (void)_convertFormat
{
   if (_format == RPixelFormatBGRA8Unorm || _format == RPixelFormatBGRX8Unorm)
      return;

   if (!_srcDirty)
      return;

   [_context convertFormat:_format from:_src to:_texture];
   _srcDirty = NO;
}

- (void)drawWithContext:(Context *)ctx
{
   [self _convertFormat];
}

- (void)drawWithEncoder:(id<MTLRenderCommandEncoder>)rce
{
   [rce setVertexBytes:&_v length:sizeof(_v) atIndex:BufferIndexPositions];
   [rce setFragmentTexture:_texture atIndex:TextureIndexColor];
   [rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
}

- (void)updateFrame:(void const *)src pitch:(NSUInteger)pitch
{
   if (_format == RPixelFormatBGRA8Unorm || _format == RPixelFormatBGRX8Unorm)
   {
      [_texture replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)_size.width, (NSUInteger)_size.height)
                  mipmapLevel:0 withBytes:src
                  bytesPerRow:(NSUInteger)(4 * pitch)];
   }
   else
   {
      [_src replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)_size.width, (NSUInteger)_size.height)
              mipmapLevel:0 withBytes:src
              bytesPerRow:(NSUInteger)(pitch)];
      _srcDirty = YES;
   }
}

@end
