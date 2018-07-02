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

@interface FrameView()

@property (readwrite) video_viewport_t *viewport;

- (instancetype)initWithDescriptor:(ViewDescriptor *)td renderer:(Renderer *)renderer;
- (void)drawWithContext:(Context *)ctx;
- (void)drawWithEncoder:(id<MTLRenderCommandEncoder>)rce;

@end

#pragma mark - private categories

@interface MetalMenu()
@property (readwrite) Renderer *renderer;
@end

@implementation MetalDriver
{
   id<MTLDevice> _device;

   Renderer *_renderer;
   FrameView *_frameView;

   video_info_t _video;
}

- (instancetype)init
{
   if (self = [super init]) {
      _frameCount = 0;
      _viewport = (video_viewport_t *)calloc(1, sizeof(video_viewport_t));
      _menu = [MetalMenu new];
   }
   return self;
}

- (void)dealloc
{
   RARCH_LOG("[MetalDriver]: destroyed\n");
   if (_viewport) {
      free(_viewport);
      _viewport = nil;
   }
}

- (Context *)context {
   return _renderer.context;
}

#pragma mark - video

- (void)setVideo:(const video_info_t *)video
{
   _video = *video;

   if (!_renderer) {
      id<MTLDevice> device = MTLCreateSystemDefaultDevice();
      _device = device;
      MetalView *view = (MetalView *)apple_platform.renderView;
      view.device = device;
      CAMetalLayer *layer = (CAMetalLayer *)view.layer;
      //layer.device = device;
      _renderer = [[Renderer alloc] initWithDevice:device layer:layer];
      _menu.renderer = _renderer;
   }

   if (!_frameView) {
      ViewDescriptor *vd = [ViewDescriptor new];
      vd.format = _video.rgb32 ? RPixelFormatBGRX8Unorm : RPixelFormatB5G6R5Unorm;
      vd.size = CGSizeMake(video->width, video->height);
      vd.filter = _video.smooth ? RTextureFilterLinear : RTextureFilterNearest;
      _frameView = [[FrameView alloc] initWithDescriptor:vd renderer:_renderer];
      _frameView.viewport = _viewport;
      [_renderer addView:_frameView];
      [_renderer sendViewToBack:_frameView];
      [_frameView setFilteringIndex:0 smooth:video->smooth];
   }
}

- (void)beginFrame
{
   video_driver_update_viewport(_viewport, NO, _keepAspect);

   [_renderer beginFrame];
}

- (void)drawViews {
   [_renderer drawViews];
}

- (void)endFrame
{
   [_renderer endFrame];
}

- (void)setNeedsResize
{
   // TODO(sgc): resize all drawables
}

#pragma mark - MTKViewDelegate

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
   RARCH_LOG("[MetalDriver] drawableSizeWillChange: %s\n", NSStringFromSize(size).UTF8String);
   _viewport->full_width = (unsigned int)size.width;
   _viewport->full_height = (unsigned int)size.height;
   video_driver_set_size(&_viewport->full_width, &_viewport->full_height);
   [_renderer drawableSizeWillChange:size];
   video_driver_update_viewport(_viewport, NO, _keepAspect);
}

- (void)drawInMTKView:(MTKView *)view {

}

@end

@implementation MetalMenu
{
   Renderer *_renderer;
   TexturedView *_view;
   BOOL _enabled;
}

- (void)setEnabled:(BOOL)enabled
{
   if (_enabled == enabled) return;
   _enabled = enabled;
   _view.visible = enabled;
}

- (BOOL)enabled
{
   return _enabled;
}

- (void)updateWidth:(int)width
             height:(int)height
             format:(RPixelFormat)format
             filter:(RTextureFilter)filter
{
   CGSize size = CGSizeMake(width, height);

   if (_view) {
      if (!(CGSizeEqualToSize(_view.size, size) &&
            _view.format == format &&
            _view.filter == filter)) {
         [_renderer removeView:_view];
         _view = nil;
      }
   }

   if (!_view) {
      ViewDescriptor *vd = [ViewDescriptor new];
      vd.format = format;
      vd.filter = filter;
      vd.size = size;
      _view = [[TexturedView alloc] initWithDescriptor:vd renderer:_renderer];
      [_renderer addView:_view];
      _view.visible = _enabled;
   }
}

- (void)updateFrame:(void const *)source
{
   [_view updateFrame:source pitch:RPixelFormatToBPP(_view.format) * (NSUInteger)_view.size.width];
}

@end

#pragma mark - FrameView

#define ALIGN(x) __attribute__((aligned(x)))

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

typedef struct ALIGN(16)
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
   __weak Renderer *_renderer;
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
   id<MTLFence> _fence;

   engine_t _engine;

   bool resize_render_targets;
   bool init_history;
   video_viewport_t *_viewport;
}

- (instancetype)initWithDescriptor:(ViewDescriptor *)d renderer:(Renderer *)r
{
   self = [super init];
   if (self) {
      _renderer = r;
      _context = r.context;
      _format = d.format;
      _bpp = RPixelFormatToBPP(_format);
      _filter = d.filter;
      if (_format == RPixelFormatBGRA8Unorm || _format == RPixelFormatBGRX8Unorm) {
         _drawState = ViewDrawStateEncoder;
      } else {
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
   for (unsigned i = 0; i < RARCH_WRAP_MAX; i++) {
      switch (i) {
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
   for (int i = 0; i < RARCH_WRAP_MAX; i++) {
      if (smooth)
         _samplers[RARCH_FILTER_UNSPEC][i] = _samplers[RARCH_FILTER_LINEAR][i];
      else
         _samplers[RARCH_FILTER_UNSPEC][i] = _samplers[RARCH_FILTER_NEAREST][i];
   }
}

- (void)setSize:(CGSize)size
{
   if (CGSizeEqualToSize(_size, size)) {
      return;
   }

   _size = size;

   resize_render_targets = YES;

   if (_format != RPixelFormatBGRA8Unorm && _format != RPixelFormatBGRX8Unorm) {
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
   if (CGRectEqualToRect(_frame, frame)) {
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
      {{l, b, 0}, {0, 1}},
      {{r, b, 0}, {1, 1}},
      {{l, t, 0}, {0, 0}},
      {{r, t, 0}, {1, 0}},
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

   [_renderer.conv convertFormat:_format from:_pixels to:_texture];
   _pixelsDirty = NO;
}

- (void)_updateHistory
{
   if (_shader) {
      if (_shader->history_size) {
         if (init_history)
            [self _initHistory];
         else {
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
       _engine.frame.texture[0].size_data.y != _size.height) {
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
                   _engine.frame.output_size.y != _viewport->height)) {
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

   if (resize_render_targets) {
      [self _updateRenderTargets];
   }

   [self _updateHistory];

   if (_format == RPixelFormatBGRA8Unorm || _format == RPixelFormatBGRX8Unorm) {
      id<MTLTexture> tex = _engine.frame.texture[0].view;
      [tex replaceRegion:MTLRegionMake2D(0, 0, (NSUInteger)_size.width, (NSUInteger)_size.height)
             mipmapLevel:0 withBytes:src
             bytesPerRow:(NSUInteger)(4 * _size.width)];
   }
   else {
      void *dst = _pixels.contents;
      size_t len = (size_t)(_bpp * _size.width);
      assert(len <= pitch); // the length can't be larger?

      if (len < pitch) {
         for (int i = 0; i < _size.height; i++) {
            memcpy(dst, src, len);
            dst += len;
            src += pitch;
         }
      }
      else {
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

   for (int i = 0; i < _shader->history_size + 1; i++) {
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
   if (_texture) {
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

   if (!_shader || _shader->passes == 0) {
      return;
   }

   for (unsigned i = 0; i < _shader->passes; i++) {
      if (_shader->pass[i].feedback) {
         texture_t tmp = _engine.pass[i].feedback;
         _engine.pass[i].feedback = _engine.pass[i].rt;
         _engine.pass[i].rt = tmp;
      }
   }

   id<MTLCommandBuffer> cb = ctx.commandBuffer;

   MTLRenderPassDescriptor *rpd = [MTLRenderPassDescriptor new];
   rpd.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1.0);
   rpd.colorAttachments[0].loadAction = MTLLoadActionClear;
   rpd.colorAttachments[0].storeAction = MTLStoreActionStore;

   BOOL firstPass = YES;
   
   for (unsigned i = 0; i < _shader->passes; i++) {
      BOOL backBuffer = (_engine.pass[i].rt.view == nil);

      if (backBuffer) {
         rpd.colorAttachments[0].texture = _context.nextDrawable.texture;
      }
      else {
         rpd.colorAttachments[0].texture = _engine.pass[i].rt.view;
      }

      id<MTLRenderCommandEncoder> rce = [cb renderCommandEncoderWithDescriptor:rpd];
      if (firstPass) {
         firstPass = NO;
      } else {
         [rce waitForFence:_fence beforeStages:MTLRenderStageVertex];
      }
      
      [rce setRenderPipelineState:_engine.pass[i]._state];

      _engine.pass[i].frame_count = (uint32_t)_frameCount;
      if (_shader->pass[i].frame_count_mod)
         _engine.pass[i].frame_count %= _shader->pass[i].frame_count_mod;

      for (unsigned j = 0; j < SLANG_CBUFFER_MAX; j++) {
         id<MTLBuffer> buffer = _engine.pass[i].buffers[j];
         cbuffer_sem_t *buffer_sem = &_engine.pass[i].semantics.cbuffers[j];

         if (buffer_sem->stage_mask && buffer_sem->uniforms) {
            void *data = buffer.contents;
            uniform_sem_t *uniform = buffer_sem->uniforms;

            while (uniform->size) {
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
      while (texture_sem->stage_mask) {
         int binding = texture_sem->binding;
         id<MTLTexture> tex = (__bridge id<MTLTexture>)*(void **)texture_sem->texture_data;
         textures[binding] = tex;
         samplers[binding] = _samplers[texture_sem->filter][texture_sem->wrap];
         texture_sem++;
      }

      if (backBuffer) {
         [rce setViewport:_engine.frame.viewport];
      }
      else {
         [rce setViewport:_engine.pass[i].viewport];
      }

      [rce setFragmentTextures:textures withRange:NSMakeRange(0, SLANG_NUM_BINDINGS)];
      [rce setFragmentSamplerStates:samplers withRange:NSMakeRange(0, SLANG_NUM_BINDINGS)];
      [rce setVertexBytes:vertex_bytes length:sizeof(vertex_bytes) atIndex:4];
      [rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
      [rce updateFence:_fence afterStages:MTLRenderStageFragment];
      [rce endEncoding];
      _texture = _engine.pass[i].rt.view;
   }
   if (_texture == nil) {
      _drawState = ViewDrawStateContext;
   } else {
      _drawState = ViewDrawStateAll;
   }
}

- (void)_updateRenderTargets
{
   if (!_shader || !resize_render_targets) return;

   // release existing targets
   for (int i = 0; i < _shader->passes; i++) {
      STRUCT_ASSIGN(_engine.pass[i].rt.view, nil);
      STRUCT_ASSIGN(_engine.pass[i].feedback.view, nil);
      memset(&_engine.pass[i].rt, 0, sizeof(_engine.pass[i].rt));
      memset(&_engine.pass[i].feedback, 0, sizeof(_engine.pass[i].feedback));
   }

   NSUInteger width = (NSUInteger)_size.width, height = (NSUInteger)_size.height;

   for (unsigned i = 0; i < _shader->passes; i++) {
      struct video_shader_pass *shader_pass = &_shader->pass[i];

      if (shader_pass->fbo.valid) {
         switch (shader_pass->fbo.type_x) {
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

         switch (shader_pass->fbo.type_y) {
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
      else if (i == (_shader->passes - 1)) {
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
         td.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
         [self _initTexture:&_engine.pass[i].rt withDescriptor:td];

         if (shader_pass->feedback) {
            [self _initTexture:&_engine.pass[i].feedback withDescriptor:td];
         }
      }
      else {
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

   for (int i = 0; i < GFX_MAX_SHADERS; i++) {
      STRUCT_ASSIGN(_engine.pass[i].rt.view, nil);
      STRUCT_ASSIGN(_engine.pass[i].feedback.view, nil);
      memset(&_engine.pass[i].rt, 0, sizeof(_engine.pass[i].rt));
      memset(&_engine.pass[i].feedback, 0, sizeof(_engine.pass[i].feedback));

      STRUCT_ASSIGN(_engine.pass[i]._state, nil);

      for (unsigned j = 0; j < SLANG_CBUFFER_MAX; j++) {
         STRUCT_ASSIGN(_engine.pass[i].buffers[j], nil);
      }
   }

   for (int i = 0; i < GFX_MAX_TEXTURES; i++) {
      STRUCT_ASSIGN(_engine.luts[i].view, nil);
   }

   free(shader);
   _fence = nil;
}

- (BOOL)setShaderFromPath:(NSString *)path
{
   [self _freeVideoShader:_shader];
   _shader = nil;

   config_file_t *conf = config_file_new(path.UTF8String);
   struct video_shader *shader = (struct video_shader *)calloc(1, sizeof(*shader));

   @try {
      if (!video_shader_read_conf_cgp(conf, shader))
         return NO;

      video_shader_resolve_relative(shader, path.UTF8String);

      texture_t *source = &_engine.frame.texture[0];
      for (unsigned i = 0; i < shader->passes; source = &_engine.pass[i++].rt) {
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
         @try {
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
            if (err != nil) {
               if (lib == nil) {
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
            if (err != nil) {
               if (lib == nil) {
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
            if (err != nil) {
               save_msl = true;
               RARCH_ERR("error creating pipeline state: %s", err.localizedDescription.UTF8String);
               return NO;
            }

            for (unsigned j = 0; j < SLANG_CBUFFER_MAX; j++) {
               unsigned int size = _engine.pass[i].semantics.cbuffers[j].size;
               if (size == 0) {
                  continue;
               }

               id<MTLBuffer> buf = [_context.device newBufferWithLength:size options:MTLResourceStorageModeManaged];
               STRUCT_ASSIGN(_engine.pass[i].buffers[j], buf);
            }
         } @finally {
            if (save_msl) {
               RARCH_LOG("[Metal]: saving metal shader files\n");

               NSError *err = nil;
               NSString *basePath = [[NSString stringWithUTF8String:shader->pass[i].source.path] stringByDeletingPathExtension];
               [vs_src writeToFile:[basePath stringByAppendingPathExtension:@"vs.metal"]
                        atomically:NO
                          encoding:NSStringEncodingConversionAllowLossy
                             error:&err];
               if (err != nil) {
                  RARCH_ERR("[Metal]: unable to save vertex shader source: %s\n", err.localizedDescription.UTF8String);
               }
               
               err = nil;
               [fs_src writeToFile:[basePath stringByAppendingPathExtension:@"fs.metal"]
                        atomically:NO
                          encoding:NSStringEncodingConversionAllowLossy
                             error:&err];
               if (err != nil) {
                  RARCH_ERR("[Metal]: unable to save fragment shader source: %s\n", err.localizedDescription.UTF8String);
               }
            }

            free(shader->pass[i].source.string.vertex);
            free(shader->pass[i].source.string.fragment);

            shader->pass[i].source.string.vertex = NULL;
            shader->pass[i].source.string.fragment = NULL;
         }
      }

      for (unsigned i = 0; i < shader->luts; i++) {
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
      _fence = [_context.device newFence];
   }
   @finally {
      if (shader) {
         [self _freeVideoShader:shader];
      }

      if (conf) {
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
#define FMT2(x,y) case SLANG_FORMAT_##x: return MTLPixelFormat##y
   
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
