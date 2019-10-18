//
//  Context.m
//  MetalRenderer
//
//  Created by Stuart Carnie on 6/9/18.
//  Copyright © 2018 Stuart Carnie. All rights reserved.
//

#import "Context.h"
#import "Filter.h"
#import <QuartzCore/QuartzCore.h>

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
      _inflightSemaphore = dispatch_semaphore_create(MAX_INFLIGHT);
      _device = d;
      _layer = layer;
#if TARGET_OS_OSX
      _layer.displaySyncEnabled = YES;
#endif
      _library = l;
      _commandQueue = [_device newCommandQueue];
      _clearColor = MTLClearColorMake(0, 0, 0, 1);
      _uniforms.projectionMatrix = matrix_proj_ortho(0, 1, 0, 1);

      _rotation = 0;
      [self setRotation:0];
      _mvp_no_rot = matrix_proj_ortho(0, 1, 0, 1);
      _mvp = matrix_proj_ortho(0, 1, 0, 1);

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
#if TARGET_OS_OSX
   _layer.displaySyncEnabled = displaySyncEnabled;
#endif
}

- (bool)displaySyncEnabled
{
#if TARGET_OS_OSX
   return _layer.displaySyncEnabled;
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

   for (int y = _viewport.y; y < _viewport.height; y++, src += srcStride, dst -= dstStride)
   {
      for (int x = _viewport.x; x < _viewport.width; x++)
      {
         dst[3 * x + 0] = src[4 * x + 0];
         dst[3 * x + 1] = src[4 * x + 1];
         dst[3 * x + 2] = src[4 * x + 2];
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
#if TARGET_OS_OSX
   for (BufferNode *n = _head; n != nil; n = n.next)
   {
      if (n.allocated > 0)
      {
         [n.src didModifyRange:NSMakeRange(0, n.allocated)];
      }
   }
#endif
}

- (void)discard
{
   _current = _head;
   _offset = 0;
   _allocated = 0;
}

- (bool)allocRange:(BufferRange *)range length:(NSUInteger)length
{
   MTLResourceOptions opts;

   memset(range, 0, sizeof(*range));

#if TARGET_OS_OSX
   opts = MTLResourceStorageModeManaged;
#else
   opts = MTLResourceStorageModeShared;
#endif

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
   {
      blockLen = length;
   }

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
