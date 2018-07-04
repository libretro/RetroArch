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
   id<MTLSamplerState> _samplers[TEXTURE_FILTER_MIPMAP_NEAREST + 1];
   Filter *_filters[RPixelFormatCount]; // convert to bgra8888
   
   // main render pass state
   id<MTLRenderCommandEncoder> _rce;
   
   id<MTLCommandBuffer> _blitCommandBuffer;
   
   NSUInteger _currentChain;
   BufferChain *_chain[CHAIN_LENGTH];
   MTLClearColor _clearColor;
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
      _layer.displaySyncEnabled = YES;
      _library = l;
      _commandQueue = [_device newCommandQueue];
      _clearColor = MTLClearColorMake(0, 0, 0, 1);
      
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
      
      if (![self _initMainState])
         return nil;
      
      for (int i = 0; i < CHAIN_LENGTH; i++)
      {
         _chain[i] = [[BufferChain alloc] initWithDevice:_device blockLen:65536];
      }
   }
   return self;
}

- (void)setDisplaySyncEnabled:(bool)displaySyncEnabled
{
   _layer.displaySyncEnabled = displaySyncEnabled;
}

- (bool)displaySyncEnabled
{
   return _layer.displaySyncEnabled;
}

- (bool)_initMainState
{
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
   
   if (!image.pixels && !image.width && !image.height)
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

- (void)convertFormat:(RPixelFormat)fmt from:(id<MTLBuffer>)src to:(id<MTLTexture>)dst
{
   assert(dst.width * dst.height == src.length / RPixelFormatToBPP(fmt));
   assert(fmt >= 0 && fmt < RPixelFormatCount);
   Filter *conv = _filters[fmt];
   assert(conv != nil);
   [conv apply:self.blitCommandBuffer inBuf:src outTex:dst];
}

- (id<MTLCommandBuffer>)blitCommandBuffer
{
   if (!_blitCommandBuffer)
      _blitCommandBuffer = [_commandQueue commandBuffer];
   return _blitCommandBuffer;
}

- (void)_nextChain
{
   _currentChain = (_currentChain + 1) % CHAIN_LENGTH;
   [_chain[_currentChain] discard];
}

- (void)begin
{
   assert(_commandBuffer == nil);
   dispatch_semaphore_wait(_inflightSemaphore, DISPATCH_TIME_FOREVER);
   _commandBuffer = [_commandQueue commandBuffer];
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
      _rce = [_commandBuffer renderCommandEncoderWithDescriptor:rpd];
   }
   return _rce;
}

- (void)end
{
   assert(_commandBuffer != nil);
   
   [_chain[_currentChain] commitRanges];
   
   if (_blitCommandBuffer)
   {
      // pending blits for mipmaps
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
   
   [_commandBuffer presentDrawable:self.nextDrawable];
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
   for (BufferNode *n = _head; n != nil; n = n.next)
   {
      if (n.allocated > 0)
      {
         [n.src didModifyRange:NSMakeRange(0, n.allocated)];
      }
   }
}

- (void)discard
{
   _current = _head;
   _offset = 0;
   _allocated = 0;
}

- (bool)allocRange:(BufferRange *)range length:(NSUInteger)length
{
   bzero(range, sizeof(*range));
   
   if (!_head)
   {
      _head = [[BufferNode alloc] initWithBuffer:[_device newBufferWithLength:_blockLen options:MTLResourceStorageModeManaged]];
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
   
   _current.next = [[BufferNode alloc] initWithBuffer:[_device newBufferWithLength:blockLen options:MTLResourceStorageModeManaged]];
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
