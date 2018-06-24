//
//  Renderer.m
//  MetalRenderer
//
//  Created by Stuart Carnie on 5/31/18.
//  Copyright © 2018 Stuart Carnie. All rights reserved.
//

#import <simd/simd.h>

#import "RendererCommon.h"
#import "Renderer.h"
#import "View.h"
#import "PixelConverter+private.h"

// Include header shared between C code here, which executes Metal API commands, and .metal files
#import "ShaderTypes.h"

@implementation Renderer
{
   dispatch_semaphore_t _inflightSemaphore;
   id<MTLDevice> _device;
   id<MTLLibrary> _library;
   id<MTLCommandQueue> _commandQueue;
   Context *_context;

   PixelConverter *_conv;

   CAMetalLayer *_layer;

   // render target layer state
   id<MTLRenderPipelineState> _t_pipelineState;
   id<MTLRenderPipelineState> _t_pipelineStateNoAlpha;
   MTLRenderPassDescriptor *_t_rpd;

   id<MTLSamplerState> _samplerStateLinear;
   id<MTLSamplerState> _samplerStateNearest;

   // views

   NSMutableArray<id<View>> *_views;

   // other state
   Uniforms _uniforms;
   BOOL _begin, _end;
}

- (instancetype)initWithDevice:(id<MTLDevice>)device layer:(CAMetalLayer *)layer
{
   self = [super init];
   if (self) {
      _inflightSemaphore = dispatch_semaphore_create(MAX_INFLIGHT);
      _device = device;
      _layer = layer;
      _views = [NSMutableArray new];
      [self _initMetal];

      _conv = [[PixelConverter alloc] initWithContext:_context];
      _begin = NO;
      _end   = NO;
   }

   return self;
}

- (void)_initMetal
{
   _commandQueue = [_device newCommandQueue];
   _library = [_device newDefaultLibrary];
   _context = [Context newContextWithDevice:_device
                                      layer:_layer
                                    library:_library
                               commandQueue:_commandQueue];

   {
      MTLVertexDescriptor *vd = [MTLVertexDescriptor new];
      vd.attributes[0].offset = 0;
      vd.attributes[0].format = MTLVertexFormatFloat3;
      vd.attributes[1].offset = offsetof(Vertex, texCoord);
      vd.attributes[1].format = MTLVertexFormatFloat2;
      vd.layouts[0].stride = sizeof(Vertex);
      vd.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

      MTLRenderPipelineDescriptor *psd = [MTLRenderPipelineDescriptor new];
      psd.label = @"Pipeline+Alpha";

      MTLRenderPipelineColorAttachmentDescriptor *ca = psd.colorAttachments[0];
      ca.pixelFormat = _layer.pixelFormat;
      ca.blendingEnabled = YES;
      ca.sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
      ca.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
      ca.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
      ca.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

      psd.sampleCount = 1;
      psd.vertexDescriptor = vd;
      psd.vertexFunction = [_library newFunctionWithName:@"basic_vertex_proj_tex"];
      psd.fragmentFunction = [_library newFunctionWithName:@"basic_fragment_proj_tex"];

      NSError *err;
      _t_pipelineState = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
      if (err != nil) {
         NSLog(@"error creating pipeline state: %@", err.localizedDescription);
         abort();
      }

      ca.blendingEnabled = NO;
      _t_pipelineStateNoAlpha = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
      if (err != nil) {
         NSLog(@"error creating pipeline state: %@", err.localizedDescription);
         abort();
      }
   }

   {
      MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor new];
      rpd.colorAttachments[0].loadAction = MTLLoadActionDontCare;
      rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
      _t_rpd = rpd;
   }

   {
      MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
      _samplerStateNearest = [_device newSamplerStateWithDescriptor:sd];

      sd.minFilter = MTLSamplerMinMagFilterLinear;
      sd.magFilter = MTLSamplerMinMagFilterLinear;
      _samplerStateLinear = [_device newSamplerStateWithDescriptor:sd];
   }
}

- (void)_updateUniforms
{
   //CGSize s = _layer.drawableSize;
   //_uniforms.projectionMatrix = matrix_proj_ortho(0, s.width, 0, s.height);
   _uniforms.projectionMatrix = matrix_proj_ortho(0, 1, 0, 1);
}

- (void)beginFrame
{
   assert(!_begin && !_end);
   _begin = YES;
   dispatch_semaphore_wait(_inflightSemaphore, DISPATCH_TIME_FOREVER);
   [_context begin];
   [self _updateUniforms];
}

- (void)endFrame
{
   assert(!_begin && _end);
   _end = NO;
   
   id<MTLCommandBuffer> cb = _context.commandBuffer;
   __block dispatch_semaphore_t inflight = _inflightSemaphore;
   [cb addCompletedHandler:^(id<MTLCommandBuffer> _) {
      dispatch_semaphore_signal(inflight);
   }];

   [cb presentDrawable:_context.nextDrawable];
   [_context end];
}

- (void)drawViews
{
   @autoreleasepool {
      assert(_begin && !_end);
      _begin = NO;
      _end   = YES;
      
      id<MTLCommandBuffer> cb = _context.commandBuffer;
      cb.label = @"renderer cb";
      
      for (id<View> v in _views) {
         if (!v.visible) continue;
         if ([v respondsToSelector:@selector(drawWithContext:)]) {
            [v drawWithContext:_context];
         }
      }
      
      BOOL pendingDraws = NO;
      for (id<View> v in _views) {
         if (v.visible && (v.drawState & ViewDrawStateEncoder) != 0) {
            pendingDraws = YES;
            break;
         }
      }
      
      if (pendingDraws) {
         id<CAMetalDrawable> drawable = _context.nextDrawable;
         _t_rpd.colorAttachments[0].texture = drawable.texture;
         
         id<MTLRenderCommandEncoder> rce = [cb renderCommandEncoderWithDescriptor:_t_rpd];
         [rce setVertexBytes:&_uniforms length:sizeof(_uniforms) atIndex:BufferIndexUniforms];
         
         for (id<View> v in _views) {
            if (!v.visible ||
                ![v respondsToSelector:@selector(drawWithEncoder:)] ||
                (v.drawState & ViewDrawStateEncoder) == 0) {
               continue;
            }
            
            // set view state
            if (v.format == RPixelFormatBGRX8Unorm || v.format == RPixelFormatB5G6R5Unorm) {
               [rce setRenderPipelineState:_t_pipelineStateNoAlpha];
            }
            else {
               [rce setRenderPipelineState:_t_pipelineState];
            }
            
            if (v.filter == RTextureFilterNearest) {
               [rce setFragmentSamplerState:_samplerStateNearest atIndex:SamplerIndexDraw];
            }
            else {
               [rce setFragmentSamplerState:_samplerStateLinear atIndex:SamplerIndexDraw];
            }
            
            [v drawWithEncoder:rce];
         }
         
         [rce endEncoding];
      }
   }
}

#pragma mark - view APIs

- (void)bringViewToFront:(id<View>)view
{
   NSUInteger pos = [_views indexOfObject:view];
   if (pos == NSNotFound || pos == _views.count - 1)
      return;
   [_views removeObjectAtIndex:pos];
   [_views addObject:view];
}

- (void)sendViewToBack:(id<View>)view
{
   NSUInteger pos = [_views indexOfObject:view];
   if (pos == NSNotFound || pos == 0)
      return;
   [_views removeObjectAtIndex:pos];
   [_views insertObject:view atIndex:0];
}

- (void)addView:(id<View>)view
{
   [_views addObject:view];
}

- (void)removeView:(id<View>)view
{
   NSUInteger pos = [_views indexOfObject:view];
   if (pos == NSNotFound)
      return;
   [_views removeObjectAtIndex:pos];
}

- (void)drawableSizeWillChange:(CGSize)size
{
   _layer.drawableSize = size;
}

@end
