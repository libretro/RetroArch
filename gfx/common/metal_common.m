//
//  metal_common.m
//  RetroArch_Metal
//
//  Created by Stuart Carnie on 5/14/18.
//

#import <Foundation/Foundation.h>
#import "metal_common.h"
#import "../../ui/drivers/cocoa/cocoa_common.h"
#import <memory.h>
#import <gfx/video_frame.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import <stddef.h>
#include <simd/simd.h>
#import "Context.h"

#define STRUCT_ASSIGN(x, y) \
{ \
   NSObject * __y = y; \
   if (x != nil) { \
      NSObject * __foo = (__bridge_transfer NSObject *)(__bridge void *)(x); \
      __foo = nil; \
      x = (__bridge __typeof__(x))nil; \
   } \
   if (__y != nil) \
      x = (__bridge __typeof__(x))(__bridge_retained void *)((NSObject *)__y); \
   }

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

@implementation MetalDriver
{
   FrameView *_frameView;
   MetalMenu *_menu;
   
   video_info_t _video;
   
   id<MTLDevice> _device;
   id<MTLLibrary> _library;
   Context *_context;
   
   CAMetalLayer *_layer;
   
   // render target layer state
   id<MTLRenderPipelineState> _t_pipelineState;
   id<MTLRenderPipelineState> _t_pipelineStateNoAlpha;
   
   id<MTLSamplerState> _samplerStateLinear;
   id<MTLSamplerState> _samplerStateNearest;
   
   //
   id<MTLRenderPipelineState> _states[GFX_MAX_SHADERS][2];
   
   // other state
   Uniforms _uniforms;
   Uniforms _viewportMVP;
   Uniforms _viewportMVPNormalized;
}

- (instancetype)initWithVideo:(const video_info_t *)video
                        input:(const input_driver_t **)input
                    inputData:(void **)inputData
{
   if (self = [super init])
   {
      _device = MTLCreateSystemDefaultDevice();
      MetalView *view = (MetalView *)apple_platform.renderView;
      view.device = _device;
      view.delegate = self;
      _layer = (CAMetalLayer *)view.layer;
      
      if (![self _initMetal])
      {
         return nil;
      }
      
      if (![self _initStates])
      {
         return nil;
      }
      
      _video = *video;
      _viewport = (video_viewport_t *)calloc(1, sizeof(video_viewport_t));
      
      _keepAspect = _video.force_aspect;
      
      gfx_ctx_mode_t mode = {
         .width = _video.width,
         .height = _video.height,
         .fullscreen = _video.fullscreen,
      };
      [apple_platform setVideoMode:mode];
      
      *input = NULL;
      *inputData = NULL;
      
      // menu display
      _display = [[MenuDisplay alloc] initWithDriver:self];
      
      // menu view
      _menu = [[MetalMenu alloc] initWithContext:_context];
      
      // frame buffer view
      {
         ViewDescriptor *vd = [ViewDescriptor new];
         vd.format = _video.rgb32 ? RPixelFormatBGRX8Unorm : RPixelFormatB5G6R5Unorm;
         vd.size = CGSizeMake(video->width, video->height);
         vd.filter = _video.smooth ? RTextureFilterLinear : RTextureFilterNearest;
         _frameView = [[FrameView alloc] initWithDescriptor:vd context:_context];
         _frameView.viewport = _viewport;
         [_frameView setFilteringIndex:0 smooth:video->smooth];
      }
      
      font_driver_init_osd((__bridge void *)self, false, video->is_threaded, FONT_DRIVER_RENDER_METAL_API);
   }
   return self;
}

- (void)dealloc
{
   RARCH_LOG("[MetalDriver]: destroyed\n");
   if (_viewport)
   {
      free(_viewport);
      _viewport = nil;
   }
   font_driver_free_osd();
}

- (bool)_initMetal
{
   _library = [_device newDefaultLibrary];
   _context = [[Context alloc] initWithDevice:_device
                                        layer:_layer
                                      library:_library];
   
   {
      MTLVertexDescriptor *vd = [MTLVertexDescriptor new];
      vd.attributes[0].offset = 0;
      vd.attributes[0].format = MTLVertexFormatFloat3;
      vd.attributes[1].offset = offsetof(Vertex, texCoord);
      vd.attributes[1].format = MTLVertexFormatFloat2;
      vd.layouts[0].stride = sizeof(Vertex);
      
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
      if (err != nil)
      {
         RARCH_ERR("[Metal]: error creating pipeline state %s\n", err.localizedDescription.UTF8String);
         return NO;
      }
      
      psd.label = @"Pipeline+No Alpha";
      ca.blendingEnabled = NO;
      _t_pipelineStateNoAlpha = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
      if (err != nil)
      {
         RARCH_ERR("[Metal]: error creating pipeline state (no alpha) %s\n", err.localizedDescription.UTF8String);
         return NO;
      }
   }
   
   {
      MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
      _samplerStateNearest = [_device newSamplerStateWithDescriptor:sd];
      
      sd.minFilter = MTLSamplerMinMagFilterLinear;
      sd.magFilter = MTLSamplerMinMagFilterLinear;
      _samplerStateLinear = [_device newSamplerStateWithDescriptor:sd];
   }
   
   return YES;
}

- (bool)_initStates
{
   MTLVertexDescriptor *vd = [MTLVertexDescriptor new];
   vd.attributes[0].offset = 0;
   vd.attributes[0].format = MTLVertexFormatFloat2;
   vd.attributes[1].offset = offsetof(SpriteVertex, texCoord);
   vd.attributes[1].format = MTLVertexFormatFloat2;
   vd.attributes[2].offset = offsetof(SpriteVertex, color);
   vd.attributes[2].format = MTLVertexFormatFloat4;
   vd.layouts[0].stride = sizeof(SpriteVertex);
   
   {
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
   }
   return YES;
}

- (void)_updateUniforms
{
   _uniforms.projectionMatrix = matrix_proj_ortho(0, 1, 0, 1);
}

- (void)_updateViewport:(CGSize)size
{
   _viewport->full_width = (unsigned int)size.width;
   _viewport->full_height = (unsigned int)size.height;
   video_driver_set_size(&_viewport->full_width, &_viewport->full_height);
   _layer.drawableSize = size;
   video_driver_update_viewport(_viewport, NO, _keepAspect);
   
   _viewportMVP.outputSize = simd_make_float2(_viewport->full_width, _viewport->full_height);
   _viewportMVP.projectionMatrix = matrix_proj_ortho(0, _viewport->full_width, _viewport->full_height, 0);
   _viewportMVP.projectionMatrix = matrix_proj_ortho(0, _viewport->full_width, 0, _viewport->full_height);
   
   _viewportMVPNormalized.outputSize = simd_make_float2(_viewport->full_width, _viewport->full_height);
   _viewportMVPNormalized.projectionMatrix = matrix_proj_ortho(0, 1, 0, 1);
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

#pragma mark - video

- (void)setVideo:(const video_info_t *)video
{
   
}

- (bool)renderFrame:(const void *)data
              width:(unsigned)width
             height:(unsigned)height
         frameCount:(uint64_t)frameCount
              pitch:(unsigned)pitch
                msg:(const char *)msg
               info:(video_frame_info_t *)video_info
{
   @autoreleasepool
   {
      [self _beginFrame];
      
      _frameView.frameCount = frameCount;
      _frameView.size = CGSizeMake(width, height);
      [_frameView updateFrame:data pitch:pitch];
      
      [self _drawViews:video_info];
      
      if (video_info->statistics_show)
      {
         struct font_params *osd_params = (struct font_params *)&video_info->osd_stat_params;
         
         if (osd_params)
         {
            font_driver_render_msg(video_info, NULL, video_info->stat_text, osd_params);
         }
      }
      
      if (msg && *msg)
      {
         font_driver_render_msg(video_info, NULL, msg, NULL);
      }
      
      [self _endFrame];
   }
   
   return YES;
}

- (void)_beginFrame
{
   video_driver_update_viewport(_viewport, NO, _keepAspect);
   [_context begin];
   [self _updateUniforms];
}

- (void)_drawViews:(video_frame_info_t *)video_info
{
   id<MTLRenderCommandEncoder> rce = _context.rce;
   
   // draw back buffer
   [rce pushDebugGroup:@"core frame"];
   [_frameView drawWithContext:_context];
   
   if ((_frameView.drawState & ViewDrawStateEncoder) != 0)
   {
      [rce setVertexBytes:&_uniforms length:sizeof(_uniforms) atIndex:BufferIndexUniforms];
      [rce setRenderPipelineState:_t_pipelineStateNoAlpha];
      if (_frameView.filter == RTextureFilterNearest)
      {
         [rce setFragmentSamplerState:_samplerStateNearest atIndex:SamplerIndexDraw];
      }
      else
      {
         [rce setFragmentSamplerState:_samplerStateLinear atIndex:SamplerIndexDraw];
      }
      [_frameView drawWithEncoder:rce];
   }
   [rce popDebugGroup];
   
   if (_menu.enabled && _menu.hasFrame)
   {
      [_menu.view drawWithContext:_context];
      [rce setVertexBytes:&_uniforms length:sizeof(_uniforms) atIndex:BufferIndexUniforms];
      [rce setRenderPipelineState:_t_pipelineState];
      if (_menu.view.filter == RTextureFilterNearest)
      {
         [rce setFragmentSamplerState:_samplerStateNearest atIndex:SamplerIndexDraw];
      }
      else
      {
         [rce setFragmentSamplerState:_samplerStateLinear atIndex:SamplerIndexDraw];
      }
      [_menu.view drawWithEncoder:rce];
   }

#if defined(HAVE_MENU)
   if (_menu.enabled)
   {
      MTLViewport viewport = {
         .originX = 0.0f,
         .originY = 0.0f,
         .width = _viewport->full_width,
         .height = _viewport->full_height,
         .znear = 0.0f,
         .zfar = 1.0,
      };
      [rce setViewport:viewport];
      [rce pushDebugGroup:@"menu"];
      menu_driver_frame(video_info);
      [rce popDebugGroup];
   }
#endif
}

- (void)_endFrame
{
   [_context end];
}

- (void)setNeedsResize
{
   // TODO(sgc): resize all drawables
}

- (Uniforms *)viewportMVP
{
   return &_viewportMVP;
}

- (Uniforms *)viewportMVPNormalized
{
   return &_viewportMVPNormalized;
}

#pragma mark - MTKViewDelegate

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size
{
   [self _updateViewport:size];
}

- (void)drawInMTKView:(MTKView *)view
{
   
}

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
   {
      _context = context;
   }
   return self;
}

- (bool)hasFrame
{
   return _view != nil;
}

- (void)setEnabled:(bool)enabled
{
   if (_enabled == enabled) return;
   _enabled = enabled;
   _view.visible = enabled;
}

- (bool)enabled
{
   return _enabled;
}

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
      {
         _view = nil;
      }
   }
   
   if (!_view)
   {
      ViewDescriptor *vd = [ViewDescriptor new];
      vd.format = format;
      vd.filter = filter;
      vd.size = size;
      _view = [[TexturedView alloc] initWithDescriptor:vd context:_context];
      _view.visible = _enabled;
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
      pass_semantics_t semantics;
      MTLViewport viewport;
      __unsafe_unretained id<MTLRenderPipelineState> _state;
   } pass[GFX_MAX_SHADERS];
   
   texture_t luts[GFX_MAX_TEXTURES];
   
} engine_t;

@implementation FrameView
{
   Context *_context;
   id<MTLTexture> _texture; // final render texture
   Vertex _v[4];
   CGSize _size; // size of view in pixels
   CGRect _frame;
   NSUInteger _bpp;
   
   id<MTLBuffer> _pixels;   // frame buffer in _srcFmt
   bool _pixelsDirty;
   
   id<MTLSamplerState> _samplers[RARCH_FILTER_MAX][RARCH_WRAP_MAX];
   struct video_shader *_shader;
   
   engine_t _engine;
   
   bool resize_render_targets;
   bool init_history;
   video_viewport_t *_viewport;
}

- (instancetype)initWithDescriptor:(ViewDescriptor *)d context:(Context *)c
{
   self = [super init];
   if (self)
   {
      _context = c;
      _format = d.format;
      _bpp = RPixelFormatToBPP(_format);
      _filter = d.filter;
      if (_format == RPixelFormatBGRA8Unorm || _format == RPixelFormatBGRX8Unorm)
      {
         _drawState = ViewDrawStateEncoder;
      }
      else
      {
         _drawState = ViewDrawStateAll;
      }
      _visible = YES;
      _engine.mvp = matrix_proj_ortho(0, 1, 0, 1);
      [self _initSamplers];
      
      self.size = d.size;
      self.frame = CGRectMake(0, 0, 1, 1);
      resize_render_targets = YES;
   }
   return self;
}

- (void)_initSamplers
{
   MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
   
   /* Initialize samplers */
   for (unsigned i = 0; i < RARCH_WRAP_MAX; i++)
   {
      switch (i)
      {
         case RARCH_WRAP_BORDER:
            sd.sAddressMode = MTLSamplerAddressModeClampToBorderColor;
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
      sd.tAddressMode = sd.sAddressMode;
      sd.rAddressMode = sd.sAddressMode;
      sd.minFilter = MTLSamplerMinMagFilterLinear;
      sd.magFilter = MTLSamplerMinMagFilterLinear;
      
      id<MTLSamplerState> ss = [_context.device newSamplerStateWithDescriptor:sd];
      _samplers[RARCH_FILTER_LINEAR][i] = ss;
      
      sd.minFilter = MTLSamplerMinMagFilterNearest;
      sd.magFilter = MTLSamplerMinMagFilterNearest;
      
      ss = [_context.device newSamplerStateWithDescriptor:sd];
      _samplers[RARCH_FILTER_NEAREST][i] = ss;
   }
}

- (void)setFilteringIndex:(int)index smooth:(bool)smooth
{
   for (int i = 0; i < RARCH_WRAP_MAX; i++)
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
   {
      return;
   }
   
   _size = size;
   
   resize_render_targets = YES;
   
   if (_format != RPixelFormatBGRA8Unorm && _format != RPixelFormatBGRX8Unorm)
   {
      _pixels = [_context.device newBufferWithLength:(NSUInteger)(size.width * size.height * 2)
                                             options:MTLResourceStorageModeManaged];
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
   
   // update vertices
   CGPoint o = frame.origin;
   CGSize s = frame.size;
   
   CGFloat l = o.x;
   CGFloat t = o.y;
   CGFloat r = o.x + s.width;
   CGFloat b = o.y + s.height;
   
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
   
   if (!_pixelsDirty)
      return;
   
   [_context convertFormat:_format from:_pixels to:_texture];
   _pixelsDirty = NO;
}

- (void)_updateHistory
{
   if (_shader)
   {
      if (_shader->history_size)
      {
         if (init_history)
            [self _initHistory];
         else
         {
            int k;
            /* todo: what about frame-duping ?
             * maybe clone d3d10_texture_t with AddRef */
            texture_t tmp = _engine.frame.texture[_shader->history_size];
            for (k = _shader->history_size; k > 0; k--)
               _engine.frame.texture[k] = _engine.frame.texture[k - 1];
            _engine.frame.texture[0] = tmp;
         }
      }
   }
   
   /* either no history, or we moved a texture of a different size in the front slot */
   if (_engine.frame.texture[0].size_data.x != _size.width ||
       _engine.frame.texture[0].size_data.y != _size.height)
   {
      MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                    width:(NSUInteger)_size.width
                                                                                   height:(NSUInteger)_size.height
                                                                                mipmapped:false];
      td.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
      [self _initTexture:&_engine.frame.texture[0] withDescriptor:td];
   }
}

- (void)updateFrame:(void const *)src pitch:(NSUInteger)pitch
{
   if (_shader && (_engine.frame.output_size.x != _viewport->width ||
                   _engine.frame.output_size.y != _viewport->height))
   {
      resize_render_targets = YES;
   }
   
   _engine.frame.viewport.originX = _viewport->x;
   _engine.frame.viewport.originY = _viewport->y;
   _engine.frame.viewport.width = _viewport->width;
   _engine.frame.viewport.height = _viewport->height;
   _engine.frame.viewport.znear = 0.0f;
   _engine.frame.viewport.zfar = 1.0f;
   _engine.frame.output_size.x = _viewport->width;
   _engine.frame.output_size.y = _viewport->height;
   _engine.frame.output_size.z = 1.0f / _viewport->width;
   _engine.frame.output_size.w = 1.0f / _viewport->height;
   
   if (resize_render_targets)
   {
      [self _updateRenderTargets];
   }
   
   [self _updateHistory];
   
   if (_format == RPixelFormatBGRA8Unorm || _format == RPixelFormatBGRX8Unorm)
   {
      id<MTLTexture> tex = _engine.frame.texture[0].view;
      [tex replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)_size.width, (NSUInteger)_size.height)
             mipmapLevel:0 withBytes:src
             bytesPerRow:pitch];
   }
   else
   {
      void *dst = _pixels.contents;
      size_t len = (size_t)(_bpp * _size.width);
      assert(len <= pitch); // the length can't be larger?
      
      if (len < pitch)
      {
         for (int i = 0; i < _size.height; i++)
         {
            memcpy(dst, src, len);
            dst += len;
            src += pitch;
         }
      }
      else
      {
         memcpy(dst, src, _pixels.length);
      }
      
      [_pixels didModifyRange:NSMakeRange(0, _pixels.length)];
      _pixelsDirty = YES;
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

- (void)_initHistory
{
   MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                 width:(NSUInteger)_size.width
                                                                                height:(NSUInteger)_size.height
                                                                             mipmapped:false];
   td.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;
   
   for (int i = 0; i < _shader->history_size + 1; i++)
   {
      [self _initTexture:&_engine.frame.texture[i] withDescriptor:td];
   }
   init_history = NO;
}

typedef struct vertex
{
   simd_float4 pos;
   simd_float2 tex;
} vertex_t;

static vertex_t vertex_bytes[] = {
   {{0, 1, 0, 1}, {0, 1}},
   {{1, 1, 0, 1}, {1, 1}},
   {{0, 0, 0, 1}, {0, 0}},
   {{1, 0, 0, 1}, {1, 0}},
};

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
   _texture = _engine.frame.texture[0].view;
   [self _convertFormat];
   
   if (!_shader || _shader->passes == 0)
   {
      return;
   }
   
   for (unsigned i = 0; i < _shader->passes; i++)
   {
      if (_shader->pass[i].feedback)
      {
         texture_t tmp = _engine.pass[i].feedback;
         _engine.pass[i].feedback = _engine.pass[i].rt;
         _engine.pass[i].rt = tmp;
      }
   }
   
   id<MTLCommandBuffer> cb = ctx.blitCommandBuffer;
   
   MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor new];
   rpd.colorAttachments[0].loadAction = MTLLoadActionDontCare;
   rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
   
   for (unsigned i = 0; i < _shader->passes; i++)
   {
      id<MTLRenderCommandEncoder> rce = nil;
      
      BOOL backBuffer = (_engine.pass[i].rt.view == nil);
      
      if (backBuffer)
      {
         rce = _context.rce;
      }
      else
      {
         rpd.colorAttachments[0].texture = _engine.pass[i].rt.view;
         rce = [cb renderCommandEncoderWithDescriptor:rpd];
      }

#if METAL_DEBUG
      rce.label = [NSString stringWithFormat:@"pass %d", i];
#endif
      
      [rce setRenderPipelineState:_engine.pass[i]._state];
      
      _engine.pass[i].frame_count = (uint32_t)_frameCount;
      if (_shader->pass[i].frame_count_mod)
         _engine.pass[i].frame_count %= _shader->pass[i].frame_count_mod;
      
      for (unsigned j = 0; j < SLANG_CBUFFER_MAX; j++)
      {
         id<MTLBuffer> buffer = _engine.pass[i].buffers[j];
         cbuffer_sem_t *buffer_sem = &_engine.pass[i].semantics.cbuffers[j];
         
         if (buffer_sem->stage_mask && buffer_sem->uniforms)
         {
            void *data = buffer.contents;
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
            [buffer didModifyRange:NSMakeRange(0, buffer.length)];
         }
      }
      
      __unsafe_unretained id<MTLTexture> textures[SLANG_NUM_BINDINGS] = {NULL};
      id<MTLSamplerState> samplers[SLANG_NUM_BINDINGS] = {NULL};
      
      texture_sem_t *texture_sem = _engine.pass[i].semantics.textures;
      while (texture_sem->stage_mask)
      {
         int binding = texture_sem->binding;
         id<MTLTexture> tex = (__bridge id<MTLTexture>)*(void **)texture_sem->texture_data;
         textures[binding] = tex;
         samplers[binding] = _samplers[texture_sem->filter][texture_sem->wrap];
         texture_sem++;
      }
      
      if (backBuffer)
      {
         [rce setViewport:_engine.frame.viewport];
      }
      else
      {
         [rce setViewport:_engine.pass[i].viewport];
      }
      
      [rce setFragmentTextures:textures withRange:NSMakeRange(0, SLANG_NUM_BINDINGS)];
      [rce setFragmentSamplerStates:samplers withRange:NSMakeRange(0, SLANG_NUM_BINDINGS)];
      [rce setVertexBytes:vertex_bytes length:sizeof(vertex_bytes) atIndex:4];
      [rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
      
      if (!backBuffer)
      {
         [rce endEncoding];
      }
      
      _texture = _engine.pass[i].rt.view;
   }
   
   if (_texture == nil)
      _drawState = ViewDrawStateContext;
   else
      _drawState = ViewDrawStateAll;
}

- (void)_updateRenderTargets
{
   if (!_shader || !resize_render_targets) return;
   
   // release existing targets
   for (int i = 0; i < _shader->passes; i++)
   {
      STRUCT_ASSIGN(_engine.pass[i].rt.view, nil);
      STRUCT_ASSIGN(_engine.pass[i].feedback.view, nil);
      memset(&_engine.pass[i].rt, 0, sizeof(_engine.pass[i].rt));
      memset(&_engine.pass[i].feedback, 0, sizeof(_engine.pass[i].feedback));
   }
   
   NSUInteger width = (NSUInteger)_size.width, height = (NSUInteger)_size.height;
   
   for (unsigned i = 0; i < _shader->passes; i++)
   {
      struct video_shader_pass *shader_pass = &_shader->pass[i];
      
      if (shader_pass->fbo.valid)
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
         width = _viewport->width;
         height = _viewport->height;
      }
      
      RARCH_LOG("[Metal]: Updating framebuffer size %u x %u.\n", width, height);
      
      MTLPixelFormat fmt = SelectOptimalPixelFormat(glslang_format_to_metal(_engine.pass[i].semantics.format));
      if ((i != (_shader->passes - 1)) ||
          (width != _viewport->width) || (height != _viewport->height) ||
          fmt != MTLPixelFormatBGRA8Unorm)
      {
         _engine.pass[i].viewport.width = width;
         _engine.pass[i].viewport.height = height;
         _engine.pass[i].viewport.znear = 0.0;
         _engine.pass[i].viewport.zfar = 1.0;
         
         MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:fmt
                                                                                       width:width
                                                                                      height:height
                                                                                   mipmapped:false];
         td.storageMode = MTLStorageModePrivate;
         td.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
         [self _initTexture:&_engine.pass[i].rt withDescriptor:td];
         
         if (shader_pass->feedback)
         {
            [self _initTexture:&_engine.pass[i].feedback withDescriptor:td];
         }
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
   if (!shader)
      return;
   
   for (int i = 0; i < GFX_MAX_SHADERS; i++)
   {
      STRUCT_ASSIGN(_engine.pass[i].rt.view, nil);
      STRUCT_ASSIGN(_engine.pass[i].feedback.view, nil);
      memset(&_engine.pass[i].rt, 0, sizeof(_engine.pass[i].rt));
      memset(&_engine.pass[i].feedback, 0, sizeof(_engine.pass[i].feedback));
      
      STRUCT_ASSIGN(_engine.pass[i]._state, nil);
      
      for (unsigned j = 0; j < SLANG_CBUFFER_MAX; j++)
      {
         STRUCT_ASSIGN(_engine.pass[i].buffers[j], nil);
      }
   }
   
   for (int i = 0; i < GFX_MAX_TEXTURES; i++)
   {
      STRUCT_ASSIGN(_engine.luts[i].view, nil);
   }
   
   free(shader);
}

- (BOOL)setShaderFromPath:(NSString *)path
{
   [self _freeVideoShader:_shader];
   _shader = nil;
   
   config_file_t *conf = config_file_new(path.UTF8String);
   struct video_shader *shader = (struct video_shader *)calloc(1, sizeof(*shader));
   
   @try
   {
      if (!video_shader_read_conf_cgp(conf, shader))
         return NO;
      
      video_shader_resolve_relative(shader, path.UTF8String);
      
      texture_t *source = &_engine.frame.texture[0];
      for (unsigned i = 0; i < shader->passes; source = &_engine.pass[i++].rt)
      {
         /* clang-format off */
         semantics_map_t semantics_map = {
            {
               /* Original */
               {&_engine.frame.texture[0].view, 0,
                  &_engine.frame.texture[0].size_data, 0},
               
               /* Source */
               {&source->view, 0,
                  &source->size_data, 0},
               
               /* OriginalHistory */
               {&_engine.frame.texture[0].view, sizeof(*_engine.frame.texture),
                  &_engine.frame.texture[0].size_data, sizeof(*_engine.frame.texture)},
               
               /* PassOutput */
               {&_engine.pass[0].rt.view, sizeof(*_engine.pass),
                  &_engine.pass[0].rt.size_data, sizeof(*_engine.pass)},
               
               /* PassFeedback */
               {&_engine.pass[0].feedback.view, sizeof(*_engine.pass),
                  &_engine.pass[0].feedback.size_data, sizeof(*_engine.pass)},
               
               /* User */
               {&_engine.luts[0].view, sizeof(*_engine.luts),
                  &_engine.luts[0].size_data, sizeof(*_engine.luts)},
            },
            {
               &_engine.mvp,                  /* MVP */
               &_engine.pass[i].rt.size_data, /* OutputSize */
               &_engine.frame.output_size,    /* FinalViewportSize */
               &_engine.pass[i].frame_count,  /* FrameCount */
            }
         };
         /* clang-format on */
         
         if (!slang_process(shader, i, RARCH_SHADER_METAL, 20000, &semantics_map, &_engine.pass[i].semantics))
            return NO;

#ifdef DEBUG
            bool save_msl = true;
#else
         bool save_msl = false;
#endif
         NSString *vs_src = [NSString stringWithUTF8String:shader->pass[i].source.string.vertex];
         NSString *fs_src = [NSString stringWithUTF8String:shader->pass[i].source.string.fragment];
         
         // vertex descriptor
         @try
         {
            MTLVertexDescriptor *vd = [MTLVertexDescriptor new];
            vd.attributes[0].offset = offsetof(vertex_t, pos);
            vd.attributes[0].format = MTLVertexFormatFloat4;
            vd.attributes[0].bufferIndex = 4;
            vd.attributes[1].offset = offsetof(vertex_t, tex);
            vd.attributes[1].format = MTLVertexFormatFloat2;
            vd.attributes[1].bufferIndex = 4;
            vd.layouts[4].stride = sizeof(vertex_t);
            vd.layouts[4].stepFunction = MTLVertexStepFunctionPerVertex;
            
            MTLRenderPipelineDescriptor *psd = [MTLRenderPipelineDescriptor new];
            psd.label = [NSString stringWithFormat:@"pass %d", i];
            
            MTLRenderPipelineColorAttachmentDescriptor *ca = psd.colorAttachments[0];
            
            ca.pixelFormat = SelectOptimalPixelFormat(glslang_format_to_metal(_engine.pass[i].semantics.format));
            
            // TODO(sgc): confirm we never need blending for render passes
            ca.blendingEnabled = NO;
            ca.sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
            ca.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
            ca.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            ca.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            
            psd.sampleCount = 1;
            psd.vertexDescriptor = vd;
            
            NSError *err;
            id<MTLLibrary> lib = [_context.device newLibraryWithSource:vs_src options:nil error:&err];
            if (err != nil)
            {
               if (lib == nil)
               {
                  save_msl = true;
                  RARCH_ERR("Metal]: unable to compile vertex shader: %s\n", err.localizedDescription.UTF8String);
                  return NO;
               }
#if DEBUG
               RARCH_WARN("[Metal]: warnings compiling vertex shader: %s\n", err.localizedDescription.UTF8String);
#endif
            }
            
            psd.vertexFunction = [lib newFunctionWithName:@"main0"];
            
            lib = [_context.device newLibraryWithSource:fs_src options:nil error:&err];
            if (err != nil)
            {
               if (lib == nil)
               {
                  save_msl = true;
                  RARCH_ERR("Metal]: unable to compile fragment shader: %s\n", err.localizedDescription.UTF8String);
                  return NO;
               }
#if DEBUG
               RARCH_WARN("[Metal]: warnings compiling fragment shader: %s\n", err.localizedDescription.UTF8String);
#endif
            }
            psd.fragmentFunction = [lib newFunctionWithName:@"main0"];
            
            STRUCT_ASSIGN(_engine.pass[i]._state,
                          [_context.device newRenderPipelineStateWithDescriptor:psd error:&err]);
            if (err != nil)
            {
               save_msl = true;
               RARCH_ERR("error creating pipeline state: %s", err.localizedDescription.UTF8String);
               return NO;
            }
            
            for (unsigned j = 0; j < SLANG_CBUFFER_MAX; j++)
            {
               unsigned int size = _engine.pass[i].semantics.cbuffers[j].size;
               if (size == 0)
               {
                  continue;
               }
               
               id<MTLBuffer> buf = [_context.device newBufferWithLength:size options:MTLResourceStorageModeManaged];
               STRUCT_ASSIGN(_engine.pass[i].buffers[j], buf);
            }
         } @finally
         {
            if (save_msl)
            {
               RARCH_LOG("[Metal]: saving metal shader files\n");
               
               NSError *err = nil;
               NSString *basePath = [[NSString stringWithUTF8String:shader->pass[i].source.path] stringByDeletingPathExtension];
               [vs_src writeToFile:[basePath stringByAppendingPathExtension:@"vs.metal"]
                        atomically:NO
                          encoding:NSStringEncodingConversionAllowLossy
                             error:&err];
               if (err != nil)
               {
                  RARCH_ERR("[Metal]: unable to save vertex shader source: %s\n", err.localizedDescription.UTF8String);
               }
               
               err = nil;
               [fs_src writeToFile:[basePath stringByAppendingPathExtension:@"fs.metal"]
                        atomically:NO
                          encoding:NSStringEncodingConversionAllowLossy
                             error:&err];
               if (err != nil)
               {
                  RARCH_ERR("[Metal]: unable to save fragment shader source: %s\n",
                            err.localizedDescription.UTF8String);
               }
            }
            
            free(shader->pass[i].source.string.vertex);
            free(shader->pass[i].source.string.fragment);
            
            shader->pass[i].source.string.vertex = NULL;
            shader->pass[i].source.string.fragment = NULL;
         }
      }
      
      for (unsigned i = 0; i < shader->luts; i++)
      {
         struct texture_image image = {0};
         image.supports_rgba = true;
         
         if (!image_texture_load(&image, shader->lut[i].path))
            return NO;
         
         MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                       width:image.width
                                                                                      height:image.height
                                                                                   mipmapped:shader->lut[i].mipmap];
         td.usage = MTLTextureUsageShaderRead;
         [self _initTexture:&_engine.luts[i] withDescriptor:td];
         
         [_engine.luts[i].view replaceRegion:MTLRegionMake2D(0, 0, image.width, image.height)
                                 mipmapLevel:0 withBytes:image.pixels
                                 bytesPerRow:4 * image.width];
         
         // TODO(sgc): generate mip maps
         image_texture_free(&image);
      }
      
      video_shader_resolve_current_parameters(conf, shader);
      _shader = shader;
      shader = nil;
   }
   @finally
   {
      if (shader)
      {
         [self _freeVideoShader:shader];
      }
      
      if (conf)
      {
         config_file_free(conf);
         conf = nil;
      }
   }
   
   resize_render_targets = YES;
   init_history = YES;
   
   return YES;
}

@end

MTLPixelFormat glslang_format_to_metal(glslang_format fmt)
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

MTLPixelFormat SelectOptimalPixelFormat(MTLPixelFormat fmt)
{
   switch (fmt)
   {
      case MTLPixelFormatRGBA8Unorm:
         return MTLPixelFormatBGRA8Unorm;
      
      case MTLPixelFormatRGBA8Unorm_sRGB:
         return MTLPixelFormatBGRA8Unorm_sRGB;
      
      default:
         return fmt;
   }
}
