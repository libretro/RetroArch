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

#include <compat/strl.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include <formats/image.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <retro_math.h>
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

#include "../common/metal_common.h"

#include "../../driver.h"
#include "../../configuration.h"

#include "../../retroarch.h"
#ifdef HAVE_REWIND
#include "../../state_manager.h"
#endif
#include "../../verbosity.h"

#include "../video_coord_array.h"

#include "../../ui/drivers/cocoa/apple_platform.h"
#include "../../ui/drivers/cocoa/cocoa_common.h"

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
}

- (instancetype)initWithVideo:(const video_info_t *)video
                        input:(input_driver_t **)input
                    inputData:(void **)inputData
{
   if (self = [super init])
   {
      _device                       = MTLCreateSystemDefaultDevice();
      MetalView *view               = (MetalView *)apple_platform.renderView;
      view.device                   = _device;
      view.delegate                 = self;
      _layer                        = (CAMetalLayer *)view.layer;

      if (![self _initMetal])
         return nil;

      _video                        = *video;
      _viewport                     = (video_viewport_t *)calloc(1, sizeof(video_viewport_t));
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
      }

      /* Overlay view */
      _overlay = [[Overlay alloc] initWithContext:_context];

      font_driver_init_osd((__bridge void *)self,
            video,
            false,
            video->is_threaded,
            FONT_DRIVER_RENDER_METAL_API);
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
      ca.pixelFormat                 = _layer.pixelFormat;
      ca.blendingEnabled             = YES;
      ca.sourceAlphaBlendFactor      = MTLBlendFactorSourceAlpha;
      ca.sourceRGBBlendFactor        = MTLBlendFactorSourceAlpha;
      ca.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
      ca.destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;

      psd.sampleCount                = 1;
      psd.vertexDescriptor           = vd;
      psd.vertexFunction             = [_library newFunctionWithName:@"basic_vertex_proj_tex"];
      psd.fragmentFunction           = [_library newFunctionWithName:@"basic_fragment_proj_tex"];


      _t_pipelineState = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
      if (err != nil)
      {
         RARCH_ERR("[Metal]: error creating pipeline state %s\n", err.localizedDescription.UTF8String);
         return NO;
      }

      psd.label               = @"Pipeline+No Alpha";
      ca.blendingEnabled      = NO;
      _t_pipelineStateNoAlpha = [_device newRenderPipelineStateWithDescriptor:psd error:&err];
      if (err != nil)
      {
         RARCH_ERR("[Metal]: error creating pipeline state (no alpha) %s\n", err.localizedDescription.UTF8String);
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
   video_driver_set_size(_viewport->full_width, _viewport->full_height);
   _layer.drawableSize     = CGSizeMake(width, height);
   video_driver_update_viewport(_viewport, forceFull, _keepAspect);
   _context.viewport       = _viewport; /* Update matrix */
   _viewportMVP.outputSize = simd_make_float2(_viewport->full_width, _viewport->full_height);
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

      [self _beginFrame];

      _frameView.frameCount = frameCount;
      if (frame && width && height)
      {
         _frameView.size = CGSizeMake(width, height);
         [_frameView updateFrame:frame pitch:pitch];
      }

      [self _drawCore];
      [self _drawMenu:video_info];

      id<MTLRenderCommandEncoder> rce = _context.rce;

#ifdef HAVE_OVERLAY
      if (_overlay.enabled)
      {
         [_context resetRenderViewport:_overlay.fullscreen ? kFullscreenViewport : kVideoViewport];
         [rce setRenderPipelineState:[_context getStockShader:VIDEO_SHADER_STOCK_BLEND blend:YES]];
         [rce setVertexBytes:_context.uniforms length:sizeof(*_context.uniforms) atIndex:BufferIndexUniforms];
         [rce setFragmentSamplerState:_samplerStateLinear atIndex:SamplerIndexDraw];
         [_overlay drawWithEncoder:rce];
      }
#endif

      if (statistics_show)
      {
         struct font_params *osd_params = (struct font_params *)&video_info->osd_stat_params;

         if (osd_params)
            font_driver_render_msg(data, video_info->stat_text, osd_params, NULL);
      }

#ifdef HAVE_GFX_WIDGETS
      if (video_info->widgets_active)
         gfx_widgets_frame(video_info);
#endif

      if (msg && *msg)
         [self _renderMessage:msg data:data];
      [self _endFrame];
   }

   return YES;
}

- (void)_renderMessage:(const char *)msg
                  data:(void*)data
{
   settings_t *settings     = config_get_ptr();
   bool msg_bgcolor_enable  = settings->bools.video_msg_bgcolor_enable;

   if (msg_bgcolor_enable)
   {
      float r, g, b, a;
      int msg_width         =
         font_driver_get_message_width(NULL, msg, strlen(msg), 1.0f);
      float font_size       = settings->floats.video_font_size;
      unsigned bgcolor_red 
                            = settings->uints.video_msg_bgcolor_red;
      unsigned bgcolor_green
                            = settings->uints.video_msg_bgcolor_green;
      unsigned bgcolor_blue 
                            = settings->uints.video_msg_bgcolor_blue;
      float bgcolor_opacity = settings->floats.video_msg_bgcolor_opacity;
      float x               = settings->floats.video_msg_pos_x;
      float y               = 1.0f - settings->floats.video_msg_pos_y;
      float width           = msg_width / (float)_viewport->full_width;
      float height          = font_size / (float)_viewport->full_height;

      float x2              = 0.005f; /* extend background around text */
      float y2              = 0.005f;

      y                    -= height;

      x                    -= x2;
      y                    -= y2;
      width                += x2;
      height               += y2;

      r                     = bgcolor_red / 255.0f;
      g                     = bgcolor_green / 255.0f;
      b                     = bgcolor_blue / 255.0f;
      a                     = bgcolor_opacity;

      [_context resetRenderViewport:kFullscreenViewport];
      [_context drawQuadX:x y:y w:width h:height r:r g:g b:b a:a];
   }

   font_driver_render_msg(data, msg, NULL, NULL);
}

- (void)_beginFrame
{
   video_viewport_t vp = *_viewport;
   video_driver_update_viewport(_viewport, NO, _keepAspect);

   if (memcmp(&vp, _viewport, sizeof(vp)) != 0)
      _context.viewport = _viewport;

   [_context begin];
}

- (void)_drawCore
{
   id<MTLRenderCommandEncoder> rce = _context.rce;

   /* draw back buffer */
   [_frameView drawWithContext:_context];

   if ((_frameView.drawState & ViewDrawStateEncoder) != 0)
   {
      [rce setVertexBytes:_context.uniforms length:sizeof(*_context.uniforms) atIndex:BufferIndexUniforms];
      [rce setRenderPipelineState:_t_pipelineStateNoAlpha];
      if (_frameView.filter == RTextureFilterNearest)
         [rce setFragmentSamplerState:_samplerStateNearest atIndex:SamplerIndexDraw];
      else
         [rce setFragmentSamplerState:_samplerStateLinear atIndex:SamplerIndexDraw];
      [_frameView drawWithEncoder:rce];
   }
}

- (void)_drawMenu:(video_frame_info_t *)video_info
{
   bool menu_is_alive = video_info->menu_is_alive;

   if (!_menu.enabled)
      return;

   id<MTLRenderCommandEncoder> rce = _context.rce;

   if (_menu.hasFrame)
   {
      [_menu.view drawWithContext:_context];
      [rce setVertexBytes:_context.uniforms length:sizeof(*_context.uniforms) atIndex:BufferIndexUniforms];
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

- (void)_convertFormat
{
   if (   _format == RPixelFormatBGRA8Unorm
       || _format == RPixelFormatBGRX8Unorm)
      return;

   if (!_srcDirty)
      return;

   [_context convertFormat:_format from:_src to:_texture];
   _srcDirty = NO;
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

- (void)_initHistory
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
   int i;
   _texture = _engine.frame.texture[0].view;
   [self _convertFormat];

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
         _engine.pass[i].frame_direction = 1;
#endif

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
   int i;
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
      if (   (i      != (_shader->passes - 1))
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

   struct video_shader *shader  = (struct video_shader *)calloc(1, sizeof(*shader));
   settings_t        *settings  = config_get_ptr();
   const char *dir_video_shader = settings->paths.directory_video_shader;
   NSString *shadersPath = [NSString stringWithFormat:@"%s/", dir_video_shader];

   @try
   {
      int i;
      texture_t *source = NULL;
      if (!video_shader_load_preset_into_shader(path.UTF8String, shader))
         return NO;

      source            = &_engine.frame.texture[0];

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

            ca.pixelFormat = SelectOptimalPixelFormat(glslang_format_to_metal(_engine.pass[i].semantics.format));

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
                  RARCH_ERR("[Metal]: unable to compile vertex shader: %s\n", err.localizedDescription.UTF8String);
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
                  RARCH_ERR("[Metal]: unable to compile fragment shader: %s\n", err.localizedDescription.UTF8String);
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
               RARCH_ERR("[Metal]: error creating pipeline state for pass %d: %s\n", i,
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

   resize_render_targets = YES;
   init_history          = YES;

   return YES;
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
   int i;
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
   int i;
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
         break;
   }

   return fmt;
}

static uint32_t metal_get_flags(void *data);

#pragma mark Graphics Context for Metal

/* The graphics context for the Metal driver is just a stubbed out version
 * It supports getting metrics such as DPI which is needed for iOS/tvOS */
#if defined(HAVE_COCOATOUCH)
static bool metal_gfx_ctx_get_metrics(
      void *data, enum display_metric_types type,
      float *value)
{
    CGRect  screen_rect          = [[UIScreen mainScreen] bounds];
    CGFloat scale                = [[UIScreen mainScreen] scale];
    float   physical_width       = screen_rect.size.width  * scale;
    float   physical_height      = screen_rect.size.height * scale;
    float   dpi                  = 160                     * scale;
    CGFloat max_size             = fmaxf(physical_width, physical_height);
    NSInteger idiom_type         = UI_USER_INTERFACE_IDIOM();

    switch (idiom_type)
    {
       case UIUserInterfaceIdiomPad:
          dpi = 132 * scale;
          break;
       case UIUserInterfaceIdiomPhone:
            if (max_size >= 2208.0)
                /* Larger iPhones: iPhone Plus, X, XR, XS, XS Max, 
                 * 11, 12, 13, 14, etc */
                dpi = 81 * scale;
            else
                dpi = 163 * scale;
          break;
       case UIUserInterfaceIdiomTV:
       case UIUserInterfaceIdiomCarPlay:
       case -1:
          /* TODO/FIXME */
          break;
    }

    switch (type)
    {
        case DISPLAY_METRIC_MM_WIDTH:
            *value = physical_width;
            break;
        case DISPLAY_METRIC_MM_HEIGHT:
            *value = physical_height;
            break;
        case DISPLAY_METRIC_DPI:
            *value = dpi;
            break;
        case DISPLAY_METRIC_NONE:
        default:
            *value = 0;
            return false;
    }
    return true;
}
#endif

/* Temporary workaround for metal not being able to poll flags during init */
static gfx_ctx_driver_t metal_fake_context = {
       NULL,
       NULL,
       NULL,
       NULL,
       NULL,
       NULL,
       NULL,
       NULL, /* get_refresh_rate */
       NULL, /* get_video_output_size */
       NULL, /* get_video_output_prev */
       NULL, /* get_video_output_next */
#ifdef HAVE_COCOATOUCH
       metal_gfx_ctx_get_metrics,
#else
       NULL,
#endif
       NULL, /* translate_aspect */
       NULL, /* update_title */
       NULL,
       NULL, /* set_resize */
       NULL,
       NULL,
       false,
       NULL,
       NULL,
       NULL,
       NULL, /* image_buffer_init */
       NULL, /* image_buffer_write */
       NULL, /* show_mouse */
       "metal",
       NULL,
       NULL,
       NULL,
       NULL, /* get_context_data */
       NULL  /* make_current */
};

static bool metal_set_shader(void *data,
      enum rarch_shader_type type, const char *path);

static void *metal_init(
      const video_info_t *video,
      input_driver_t **input,
      void **input_data)
{
   const char *shader_path;
   MetalDriver *md = nil;

   [apple_platform setViewType:APPLE_VIEW_TYPE_METAL];

   md = [[MetalDriver alloc] initWithVideo:video input:input inputData:input_data];
   if (md == nil)
      return NULL;

   metal_fake_context.get_flags = metal_get_flags;
   video_context_driver_set(&metal_fake_context);

   shader_path = video_shader_get_current_shader_preset();
   metal_set_shader((__bridge void *)md,
         video_shader_parse_type(shader_path), shader_path);

   return (__bridge_retained void *)md;
}

static bool metal_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height,
      uint64_t frame_count,
      unsigned pitch, const char *msg,
      video_frame_info_t *video_info)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   return [md renderFrame:frame
                     data:data
                    width:frame_width
                   height:frame_height
               frameCount:frame_count
                    pitch:pitch
                      msg:msg
                     info:video_info];
}

static void metal_set_nonblock_state(void *data, bool non_block,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   md.context.displaySyncEnabled = !non_block;
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
         if (!string_is_empty(path) && type != RARCH_SHADER_SLANG)
            RARCH_WARN("[Metal] Only Slang shaders are supported. Falling back to stock.\n");
         path = NULL;
      }

      /* TODO/FIXME - actually return to stock shader */
      if (string_is_empty(path))
         return true;

      if ([md.frameView setShaderFromPath:[NSString stringWithUTF8String:path]])
         return true;
   }
#endif
   return false;
}

static void metal_free(void *data)
{
   MetalDriver *md = (__bridge_transfer MetalDriver *)data;
   md = nil;
}

static void metal_set_viewport(void *data, unsigned viewport_width,
                               unsigned viewport_height, bool force_full, bool allow_rotate)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (md)
      [md setViewportWidth:viewport_width height:viewport_height forceFull:force_full allowRotate:allow_rotate];
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

static uintptr_t metal_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   MetalDriver           *md  = (__bridge MetalDriver *)video_data;
   struct texture_image *img  = (struct texture_image *)data;
   if (!img)
      return 0;

   struct texture_image image = *img;
   Texture *t = [md.context newTexture:image filter:filter_type];
   return (uintptr_t)(__bridge_retained void *)(t);
}

static void metal_unload_texture(void *data, 
      bool threaded, uintptr_t handle)
{
   if (!handle)
      return;
   Texture *t = (__bridge_transfer Texture *)(void *)handle;
   t = nil;
}

/* TODO/FIXME - implement */
static void metal_set_video_mode(void *data,
                                 unsigned width, unsigned height,
                                 bool fullscreen)
{
   RARCH_LOG("[Metal]: set_video_mode res=%dx%d fullscreen=%s\n",
             width, height,
             fullscreen ? "YES" : "NO");
}

/* TODO/FIXME - implement */
static float metal_get_refresh_rate(void *data) { return 0.0f; }

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
   settings_t *settings    = config_get_ptr();
   bool menu_linear_filter = settings->bools.menu_linear_filter;

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
   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);

#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif

   return flags;
}

static const video_poke_interface_t metal_poke_interface = {
   metal_get_flags,
   metal_load_texture,
   metal_unload_texture,
   metal_set_video_mode,
   metal_get_refresh_rate,
   metal_set_filtering,
   NULL,                      /* get_video_output_size */
   NULL,                      /* get_video_output_prev */
   NULL,                      /* get_video_output_next */
   NULL,                      /* get_current_framebuffer */
   NULL,                      /* get_proc_address */
   metal_set_aspect_ratio,
   metal_apply_state_changes,
   metal_set_texture_frame,
   metal_set_texture_enable,
   font_driver_render_msg,
   metal_show_mouse,
   NULL,                      /* grab_mouse_toggle */
   metal_get_current_shader,
   NULL,                      /* get_current_software_framebuffer */
   NULL,                      /* get_hw_render_interface */
   NULL,                      /* set_hdr_max_nits */
   NULL,                      /* set_hdr_paper_white_nits */
   NULL,                      /* set_hdr_contrast */
   NULL                       /* set_hdr_expand_gamut */
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
static bool metal_gfx_widgets_enabled(void *data) { return true; }
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
#ifdef HAVE_VIDEO_LAYOUT
  NULL,
#endif
   metal_get_poke_interface,
   NULL, /* metal_wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   metal_gfx_widgets_enabled
#endif
};
