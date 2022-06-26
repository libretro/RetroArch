/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2017 - Hans-Kristian Arntzen
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

#include <string.h>

#include <encodings/utf.h>
#include <compat/strl.h>

#include "../common/metal_common.h"

#include "../font_driver.h"

#include "../../configuration.h"
#include "../../verbosity.h"

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
   id<MTLBuffer> _vert;
   unsigned _capacity;
   unsigned _offset;
   unsigned _vertices;
}

@property (readonly) struct font_atlas *atlas;

- (void)deinit;
- (instancetype)initWithDriver:(MetalDriver *)driver fontPath:(const char *)font_path fontSize:(unsigned)font_size;

- (int)getWidthForMessage:(const char *)msg length:(NSUInteger)length scale:(float)scale;
- (const struct font_glyph *)getGlyph:(uint32_t)code;
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

      _driver = driver;
      _context = driver.context;
      if (!font_renderer_create_default(
               &_font_driver,
               &_font_data, font_path, font_size))
      {
         RARCH_WARN("Couldn't initialize font renderer.\n");
         return nil;
      }

      _uniforms.projectionMatrix = matrix_proj_ortho(0, 1, 0, 1);
      _atlas = _font_driver->get_atlas(_font_data);
      _stride = MTL_ALIGN_BUFFER(_atlas->width);
      if (_stride == _atlas->width)
      {
         _buffer = [_context.device newBufferWithBytes:_atlas->buffer
                                                length:(NSUInteger)(_stride * _atlas->height)
                                               options:PLATFORM_METAL_RESOURCE_STORAGE_MODE];

         // Even though newBufferWithBytes will copy the initial contents
         // from our atlas, it doesn't seem to invalidate the buffer when
         // doing so, causing corrupted text rendering if we hit this code
         // path. To work around it we manually invalidate the buffer.
#if !defined(HAVE_COCOATOUCH)
         [_buffer didModifyRange:NSMakeRange(0, _buffer.length)];
#endif
      }
      else
      {
         _buffer = [_context.device newBufferWithLength:(NSUInteger)(_stride * _atlas->height)
                                                options:PLATFORM_METAL_RESOURCE_STORAGE_MODE];
         void *dst = _buffer.contents;
         void *src = _atlas->buffer;
         for (unsigned i = 0; i < _atlas->height; i++)
         {
            memcpy(dst, src, _atlas->width);
            dst += _stride;
            src += _atlas->width;
         }
#if !defined(HAVE_COCOATOUCH)
          [_buffer didModifyRange:NSMakeRange(0, _buffer.length)];
#endif
      }

      MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                                                    width:_atlas->width
                                                                                   height:_atlas->height
                                                                                mipmapped:NO];

      _texture = [_buffer newTextureWithDescriptor:td offset:0 bytesPerRow:_stride];

      _capacity = 12000;
      _vert = [_context.device newBufferWithLength:sizeof(SpriteVertex) *
               _capacity options:PLATFORM_METAL_RESOURCE_STORAGE_MODE];
      if (![self _initializeState])
      {
         return nil;
      }
   }
   return self;
}

- (bool)_initializeState
{
   {
      MTLVertexDescriptor *vd = [MTLVertexDescriptor new];
      vd.attributes[0].offset = 0;
      vd.attributes[0].format = MTLVertexFormatFloat2;
      vd.attributes[1].offset = offsetof(SpriteVertex, texCoord);
      vd.attributes[1].format = MTLVertexFormatFloat2;
      vd.attributes[2].offset = offsetof(SpriteVertex, color);
      vd.attributes[2].format = MTLVertexFormatFloat4;
      vd.layouts[0].stride = sizeof(SpriteVertex);
      vd.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

      MTLRenderPipelineDescriptor *psd = [MTLRenderPipelineDescriptor new];
      psd.label = @"font pipeline";

      MTLRenderPipelineColorAttachmentDescriptor *ca = psd.colorAttachments[0];
      ca.pixelFormat = MTLPixelFormatBGRA8Unorm;
      ca.blendingEnabled = YES;
      ca.sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
      ca.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
      ca.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
      ca.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;

      psd.sampleCount = 1;
      psd.vertexDescriptor = vd;
      psd.vertexFunction = [_context.library newFunctionWithName:@"sprite_vertex"];
      psd.fragmentFunction = [_context.library newFunctionWithName:@"sprite_fragment_a8"];

      NSError *err;
      _state = [_context.device newRenderPipelineStateWithDescriptor:psd error:&err];
      if (err != nil)
      {
         RARCH_ERR("[MetalRaster]: error creating pipeline state: %s\n", err.localizedDescription.UTF8String);
         return NO;
      }
   }

   {
      MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
      sd.minFilter = MTLSamplerMinMagFilterLinear;
      sd.magFilter = MTLSamplerMinMagFilterLinear;
      _sampler = [_context.device newSamplerStateWithDescriptor:sd];
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
      NSUInteger offset = glyph->atlas_offset_y;
      NSUInteger len = glyph->height * _stride;
      [_buffer didModifyRange:NSMakeRange(offset, len)];
#endif

      _atlas->dirty = false;
   }
}

- (int)getWidthForMessage:(const char *)msg length:(NSUInteger)length scale:(float)scale
{
   int delta_x = 0;
   const struct font_glyph* glyph_q = _font_driver->get_glyph(_font_data, '?');

   for (NSUInteger i = 0; i < length; i++)
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
   if (!_font_driver->ident)
      return NULL;

   const struct font_glyph *glyph = _font_driver->get_glyph((void *)_font_driver, code);
   if (glyph)
   {
      [self updateGlyph:glyph];
   }

   return glyph;
}

static INLINE void write_quad6(SpriteVertex *pv,
      float x, float y, float width, float height,
      float tex_x, float tex_y, float tex_width, float tex_height,
      const vector_float4 *color)
{
   unsigned i;
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
   const char  *msg_end = msg + length;
   int                x = (int)roundf(posX * _driver.viewport->full_width);
   int                y = (int)roundf((1.0f - posY) * _driver.viewport->full_height);
   int          delta_x = 0;
   int          delta_y = 0;
   float inv_tex_size_x = 1.0f / _texture.width;
   float inv_tex_size_y = 1.0f / _texture.height;
   float inv_win_width  = 1.0f / _driver.viewport->full_width;
   float inv_win_height = 1.0f / _driver.viewport->full_height;

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

   SpriteVertex *v = (SpriteVertex *)_vert.contents;
   v              += _offset + _vertices;
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
   NSUInteger start = _offset * sizeof(SpriteVertex);
#if !defined(HAVE_COCOATOUCH)
   [_vert didModifyRange:NSMakeRange(start, sizeof(SpriteVertex) * _vertices)];
#endif

   id<MTLRenderCommandEncoder> rce = _context.rce;
   [rce pushDebugGroup:@"render fonts"];

   [_context resetRenderViewport:kFullscreenViewport];
   [rce setRenderPipelineState:_state];
   [rce setVertexBytes:&_uniforms length:sizeof(Uniforms) atIndex:BufferIndexUniforms];
   [rce setVertexBuffer:_vert offset:start atIndex:BufferIndexPositions];
   [rce setFragmentTexture:_texture atIndex:TextureIndexColor];
   [rce setFragmentSamplerState:_sampler atIndex:SamplerIndexDraw];
   [rce drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:_vertices];
   [rce popDebugGroup];

   _offset += _vertices;
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

   /* If font line metrics are not supported just draw as usual */
   if (!_font_driver->get_line_metrics ||
       !_font_driver->get_line_metrics(_font_data, &line_metrics))
   {
      [self _renderLine:msg length:strlen(msg) scale:scale color:color posX:posX posY:posY aligned:aligned];
      return;
   }

   line_height = line_metrics->height * scale / height;

   for (;;)
   {
      const char *delim  = strchr(msg, '\n');
      NSUInteger msg_len = delim ?
         (unsigned)(delim - msg) : strlen(msg);

      /* Draw the line */
      [self _renderLine:msg
                 length:msg_len
                  scale:scale
                  color:color
                   posX:posX
                   posY:posY - (float)lines * line_height
                aligned:aligned];

      if (!delim)
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

      NSUInteger max_glyphs = strlen(msg);
      if (drop_x || drop_y)
         max_glyphs *= 2;

      if (max_glyphs * 6 + _offset > _capacity)
         _offset = 0;

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
      unsigned msg_len, float scale)
{
   MetalRaster *r = (__bridge MetalRaster *)data;
   return [r getWidthForMessage:msg length:msg_len scale:scale];
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

font_renderer_t metal_raster_font = {
   metal_raster_font_init,
   metal_raster_font_free,
   metal_raster_font_render_msg,
   "metal_raster",
   metal_raster_font_get_glyph,
   NULL, /* bind_block  */
   NULL, /* flush_block */
   metal_raster_font_get_message_width,
   NULL  /* get_line_metrics */
};
