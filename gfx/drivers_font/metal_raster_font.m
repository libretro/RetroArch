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

- (instancetype)initWithDriver:(MetalDriver *)driver fontPath:(const char *)font_path fontSize:(unsigned)font_size;

- (int)getWidthForMessage:(const char *)msg length:(NSUInteger)length scale:(float)scale;
- (const struct font_glyph *)getGlyph:(uint32_t)code;
@end

@implementation MetalRaster

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
                                               options:MTLResourceStorageModeManaged];

         // Even though newBufferWithBytes will copy the initial contents
         // from our atlas, it doesn't seem to invalidate the buffer when
         // doing so, causing corrupted text rendering if we hit this code
         // path. To work around it we manually invalidate the buffer.
         [_buffer didModifyRange:NSMakeRange(0, _buffer.length)];
      }
      else
      {
         _buffer = [_context.device newBufferWithLength:(NSUInteger)(_stride * _atlas->height)
                                                options:MTLResourceStorageModeManaged];
         void *dst = _buffer.contents;
         void *src = _atlas->buffer;
         for (unsigned i = 0; i < _atlas->height; i++)
         {
            memcpy(dst, src, _atlas->width);
            dst += _stride;
            src += _atlas->width;
         }
         [_buffer didModifyRange:NSMakeRange(0, _buffer.length)];
      }

      MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                                                    width:_atlas->width
                                                                                   height:_atlas->height
                                                                                mipmapped:NO];

      _texture = [_buffer newTextureWithDescriptor:td offset:0 bytesPerRow:_stride];

      _capacity = 12000;
      _vert = [_context.device newBufferWithLength:sizeof(SpriteVertex) *
                                                   _capacity options:MTLResourceStorageModeManaged];
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

      NSUInteger offset = glyph->atlas_offset_y;
      NSUInteger len = glyph->height * _stride;
      [_buffer didModifyRange:NSMakeRange(offset, len)];

      _atlas->dirty = false;
   }
}

- (int)getWidthForMessage:(const char *)msg length:(NSUInteger)length scale:(float)scale
{
   int delta_x = 0;

   for (NSUInteger i = 0; i < length; i++)
   {
      const struct font_glyph *glyph = _font_driver->get_glyph(_font_data, (uint8_t)msg[i]);
      if (!glyph) /* Do something smarter here ... */
         glyph = _font_driver->get_glyph(_font_data, '?');

      if (glyph)
      {
         [self updateGlyph:glyph];
         delta_x += glyph->advance_x;
      }
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
      pv[i].position = simd_make_float2(x + strip[2 * i + 0] * width,
                                        y + strip[2 * i + 1] * height);
      pv[i].texCoord = simd_make_float2(tex_x + strip[2 * i + 0] * tex_width,
                                        tex_y + strip[2 * i + 1] * tex_height);
      pv[i].color = *color;
   }
}

- (void)_renderLine:(const char *)msg
              video:(video_frame_info_t *)video
             length:(NSUInteger)length
              scale:(float)scale
              color:(vector_float4)color
               posX:(float)posX
               posY:(float)posY
            aligned:(unsigned)aligned
{
   const char *msg_end = msg + length;
   int x = (int)roundf(posX * _driver.viewport->full_width);
   int y = (int)roundf((1.0f - posY) * _driver.viewport->full_height);
   int delta_x = 0;
   int delta_y = 0;
   float inv_tex_size_x = 1.0f / _texture.width;
   float inv_tex_size_y = 1.0f / _texture.height;
   float inv_win_width = 1.0f / _driver.viewport->full_width;
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
   v += _offset + _vertices;

   while (msg < msg_end)
   {
      unsigned code = utf8_walk(&msg);
      const struct font_glyph *glyph = _font_driver->get_glyph(_font_data, code);

      if (!glyph) /* Do something smarter here ... */
         glyph = _font_driver->get_glyph(_font_data, '?');

      if (!glyph)
         continue;

      [self updateGlyph:glyph];

      int off_x, off_y, tex_x, tex_y, width, height;
      off_x = glyph->draw_offset_x;
      off_y = glyph->draw_offset_y;
      tex_x = glyph->atlas_offset_x;
      tex_y = glyph->atlas_offset_y;
      width = glyph->width;
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
      v += 6;

      delta_x += glyph->advance_x;
      delta_y += glyph->advance_y;
   }
}

- (void)_flush
{
   NSUInteger start = _offset * sizeof(SpriteVertex);
   [_vert didModifyRange:NSMakeRange(start, sizeof(SpriteVertex) * _vertices)];

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
                video:(video_frame_info_t *)video
                scale:(float)scale
                color:(vector_float4)color
                 posX:(float)posX
                 posY:(float)posY
              aligned:(unsigned)aligned
{
   /* If the font height is not supported just draw as usual */
   if (!_font_driver->get_line_height)
   {
      [self _renderLine:msg video:video length:strlen(msg) scale:scale color:color posX:posX posY:posY aligned:aligned];
      return;
   }

   int lines = 0;
   float line_height = _font_driver->get_line_height(_font_data) * scale / video->height;

   for (;;)
   {
      const char *delim = strchr(msg, '\n');

      /* Draw the line */
      if (delim)
      {
         NSUInteger msg_len = delim - msg;
         [self _renderLine:msg
                     video:video
                    length:msg_len
                     scale:scale
                     color:color
                      posX:posX
                      posY:posY - (float)lines * line_height
                   aligned:aligned];
         msg += msg_len + 1;
         lines++;
      }
      else
      {
         NSUInteger msg_len = strlen(msg);
         [self _renderLine:msg
                     video:video
                    length:msg_len
                     scale:scale
                     color:color
                      posX:posX
                      posY:posY - (float)lines * line_height
                   aligned:aligned];
         break;
      }
   }
}

- (void)renderMessage:(const char *)msg
                video:(video_frame_info_t *)video
               params:(const struct font_params *)params
{

   if (!msg || !*msg)
      return;

   float x, y, scale, drop_mod, drop_alpha;
   int drop_x, drop_y;
   enum text_alignment text_align;
   vector_float4 color, color_dark;
   unsigned width = video->width;
   unsigned height = video->height;

   if (params)
   {
      x = params->x;
      y = params->y;
      scale = params->scale;
      text_align = params->text_align;
      drop_x = params->drop_x;
      drop_y = params->drop_y;
      drop_mod = params->drop_mod;
      drop_alpha = params->drop_alpha;

      color = simd_make_float4(
         FONT_COLOR_GET_RED(params->color) / 255.0f,
         FONT_COLOR_GET_GREEN(params->color) / 255.0f,
         FONT_COLOR_GET_BLUE(params->color) / 255.0f,
         FONT_COLOR_GET_ALPHA(params->color) / 255.0f);

   }
   else
   {
      x = video->font_msg_pos_x;
      y = video->font_msg_pos_y;
      scale = 1.0f;
      text_align = TEXT_ALIGN_LEFT;

      color = simd_make_float4(
         video->font_msg_color_r,
         video->font_msg_color_g,
         video->font_msg_color_b,
         1.0f);

      drop_x = -2;
      drop_y = -2;
      drop_mod = 0.3f;
      drop_alpha = 1.0f;
   }

   @autoreleasepool
   {

      NSUInteger max_glyphs = strlen(msg);
      if (drop_x || drop_y)
         max_glyphs *= 2;

      if (max_glyphs * 6 + _offset > _capacity)
      {
         _offset = 0;
      }

      if (drop_x || drop_y)
      {
         color_dark.x = color.x * drop_mod;
         color_dark.y = color.y * drop_mod;
         color_dark.z = color.z * drop_mod;
         color_dark.w = color.w * drop_alpha;

         [self renderMessage:msg
                       video:video
                       scale:scale
                       color:color_dark
                        posX:x + scale * drop_x / width
                        posY:y + scale * drop_y / height
                     aligned:text_align];
      }

      [self renderMessage:msg
                    video:video
                    scale:scale
                    color:color
                     posX:x
                     posY:y
                  aligned:text_align];

      [self _flush];
   }
}

@end

static void metal_raster_font_free_font(void *data, bool is_threaded);

static void *metal_raster_font_init_font(void *data,
                                         const char *font_path, float font_size,
                                         bool is_threaded)
{
   MetalRaster *r = [[MetalRaster alloc] initWithDriver:(__bridge MetalDriver *)data fontPath:font_path fontSize:(unsigned)font_size];

   if (!r)
      return NULL;

   return (__bridge_retained void *)r;
}

static void metal_raster_font_free_font(void *data, bool is_threaded)
{
   MetalRaster *r = (__bridge_transfer MetalRaster *)data;
   r = nil;
}

static int metal_get_message_width(void *data, const char *msg,
                                   unsigned msg_len, float scale)
{
   MetalRaster *r = (__bridge MetalRaster *)data;
   return [r getWidthForMessage:msg length:msg_len scale:scale];
}

static void metal_raster_font_render_msg(
   video_frame_info_t *video_info,
   void *data, const char *msg,
   const struct font_params *params)
{
   MetalRaster *r = (__bridge MetalRaster *)data;
   [r renderMessage:msg video:video_info params:params];
}

static const struct font_glyph *metal_raster_font_get_glyph(
   void *data, uint32_t code)
{
   MetalRaster *r = (__bridge MetalRaster *)data;
   return [r getGlyph:code];
}

static void metal_raster_font_flush_block(unsigned width, unsigned height,
      void *data, video_frame_info_t *video_info)
{
   (void)data;
}

static void metal_raster_font_bind_block(void *data, void *userdata)
{
   (void)data;
}

font_renderer_t metal_raster_font = {
   .init              = metal_raster_font_init_font,
   .free              = metal_raster_font_free_font,
   .render_msg        = metal_raster_font_render_msg,
   .ident             = "Metal raster",
   .get_glyph         = metal_raster_font_get_glyph,
   .bind_block        = metal_raster_font_bind_block,
   .flush             = metal_raster_font_flush_block,
   .get_message_width = metal_get_message_width
};
