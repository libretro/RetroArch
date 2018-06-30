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

@interface Texture()
@property (readwrite) id<MTLTexture> texture;
@property (readwrite) id<MTLSamplerState> sampler;
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
      _library = l;
      _commandQueue = [_device newCommandQueue];
      
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
      
      if (![self _initConversionFilters])
         return nil;
      
      if (![self _initMainState])
         return nil;
   }
   return self;
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
   
   // TODO(sgc): mipmapping is not working
   BOOL mipmapped = filter == TEXTURE_FILTER_MIPMAP_LINEAR || filter == TEXTURE_FILTER_MIPMAP_NEAREST;
   
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
   
   Texture *tex = [Texture new];
   tex.texture = t;
   tex.sampler = _samplers[filter];
   
   return tex;
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
      rpd.colorAttachments[0].texture = self.nextDrawable.texture;
      _rce = [_commandBuffer renderCommandEncoderWithDescriptor:rpd];
   }
   return _rce;
}

- (void)end
{
   assert(self->_commandBuffer != nil);
   
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
}

@end

@implementation Texture
@end
