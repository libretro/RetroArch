//
// Created by Stuart Carnie on 6/24/18.
//

#import "Context.h"
#import "MenuDisplay.h"
#import "ShaderTypes.h"
#import "menu_driver.h"
#import <Metal/Metal.h>
// TODO(sgc): this dependency is incorrect
#import "../metal_common.h"

@implementation MenuDisplay
{
   __weak MetalDriver *_driver;
   Context *_context;
   MTLClearColor _clearColor;
   bool _clearNextRender;
   Uniforms _uniforms;
}

- (instancetype)initWithDriver:(MetalDriver *)driver
{
   if (self = [super init])
   {
      _driver = driver;
      _context = driver.context;
      _clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
      _uniforms.projectionMatrix = matrix_proj_ortho(0, 1, 0, 1);
   }
   return self;
}

+ (const float *)defaultVertices
{
   static float dummy[] = {
      0.0f, 1.0f,
      1.0f, 1.0f,
      0.0f, 0.0f,
      1.0f, 0.0f,
   };
   return &dummy[0];
}

+ (const float *)defaultTexCoords
{
   static float dummy[] = {
      0.0f, 1.0f,
      1.0f, 1.0f,
      0.0f, 0.0f,
      1.0f, 0.0f,
   };
   return &dummy[0];
}

+ (const float *)defaultColor
{
   static float dummy[] = {
      1.0f, 0.0f, 1.0f, 1.0f,
      1.0f, 0.0f, 1.0f, 1.0f,
      1.0f, 0.0f, 1.0f, 1.0f,
      1.0f, 0.0f, 1.0f, 1.0f,
   };
   return &dummy[0];
}

+ (const float *)defaultMatrix
{
   static matrix_float4x4 dummy;
   
   static dispatch_once_t onceToken;
   dispatch_once(&onceToken, ^{
      dummy = matrix_proj_ortho(0, 1, 0, 1);
   });
   return &dummy;
}

- (void)setClearColor:(MTLClearColor)clearColor
{
   _clearColor = clearColor;
   _clearNextRender = YES;
}

- (MTLClearColor)clearColor
{
   return _clearColor;
}

- (MTLPrimitiveType)_toPrimitiveType:(enum menu_display_prim_type)prim
{
   switch (prim)
   {
      case MENU_DISPLAY_PRIM_TRIANGLESTRIP:
         return MTLPrimitiveTypeTriangleStrip;
      case MENU_DISPLAY_PRIM_TRIANGLES:
         return MTLPrimitiveTypeTriangle;
      default:
         RARCH_LOG("unexpected primitive type %d\n", prim);
         return MTLPrimitiveTypeTriangle;
   }
}

static INLINE void write_quad4(SpriteVertex *pv,
                               float x, float y, float width, float height, float scale,
                               float tex_x, float tex_y, float tex_width, float tex_height,
                               const float *color)
{
   unsigned i;
   static const float strip[2 * 4] = {
      0.0f, 1.0f,
      1.0f, 1.0f,
      0.0f, 0.0f,
      1.0f, 0.0f,
   };
   
   float swidth = width * scale;
   float sheight = height * scale;
   
   x += (width - swidth) / 2;
   y += (height - sheight) / 2;
   
   for (i = 0; i < 4; i++)
   {
      pv[i].position = simd_make_float2(x + strip[2 * i + 0] * swidth,
                                        y + strip[2 * i + 1] * sheight);
      pv[i].texCoord = simd_make_float2(tex_x + strip[2 * i + 0] * tex_width,
                                        tex_y + strip[2 * i + 1] * tex_height);
      pv[i].color = simd_make_float4(color[0], color[1], color[2], color[3]);
      color += 4;
   }
}

static INLINE void write_quad4a(SpriteVertex *pv,
                                float x, float y, float width, float height, float scale,
                                float tex_x, float tex_y, float tex_width, float tex_height,
                                const float *color)
{
   unsigned i;
   static const float vert[2 * 4] = {
      0.0f, 1.0f,
      1.0f, 1.0f,
      0.0f, 0.0f,
      1.0f, 0.0f,
   };
   static const float tex[2 * 4] = {
      0.0f, 0.0f,
      1.0f, 0.0f,
      0.0f, 1.0f,
      1.0f, 1.0f,
   };
   
   float swidth = width * scale;
   float sheight = height * scale;
   
   x += (width - swidth) / 2;
   y += (height - sheight) / 2;
   
   for (i = 0; i < 4; i++)
   {
      pv[i].position = simd_make_float2(x + vert[2 * i + 0] * swidth,
                                        y + vert[2 * i + 1] * sheight);
      pv[i].texCoord = simd_make_float2(tex_x + tex[2 * i + 0] * tex_width,
                                        tex_y + tex[2 * i + 1] * tex_height);
      pv[i].color = simd_make_float4(color[0], color[1], color[2], color[3]);
      color += 4;
   }
}

- (void)drawPipeline:(menu_display_ctx_draw_t *)draw video:(video_frame_info_t *)video
{

}

- (void)draw:(menu_display_ctx_draw_t *)draw video:(video_frame_info_t *)video
{
   Texture *tex = (__bridge Texture *)(void *)draw->texture;
   const float *vertex = draw->coords->vertex;
   const float *tex_coord = draw->coords->tex_coord;
   const float *color = draw->coords->color;
   
   if (!vertex)
      vertex = MenuDisplay.defaultVertices;
   if (!tex_coord)
      tex_coord = MenuDisplay.defaultTexCoords;
   if (!draw->coords->lut_tex_coord)
      draw->coords->lut_tex_coord = MenuDisplay.defaultTexCoords;
   
   // TODO(sgc): is this necessary?
   //   if (!texture)
   //      texture         = &vk->display.blank_texture;
   if (!color)
      color = MenuDisplay.defaultColor;
   
   assert(draw->coords->vertices <= 4);
   SpriteVertex buf[4];
   SpriteVertex *pv = buf;
   Uniforms *uniforms;
   if (draw->coords->vertex == NULL)
   {
      write_quad4a(pv,
                   draw->x,
                   draw->y,
                   draw->width,
                   draw->height,
                   draw->scale_factor,
                   0.0, 0.0, 1.0, 1.0, color);
      
      uniforms = _driver.viewportMVP;
   }
   else
   {
      for (unsigned i = 0; i < draw->coords->vertices; i++, pv++)
      {
         /* Y-flip. We're using top-left coordinates */
         pv->position = simd_make_float2(vertex[0], vertex[1]);
         vertex += 2;
         
         pv->texCoord = simd_make_float2(tex_coord[0], tex_coord[1]);
         tex_coord += 2;
         
         pv->color = simd_make_float4(color[0], color[1], color[2], color[3]);
         color += 4;
      }
      uniforms = &_uniforms;
   }
   
   switch (draw->pipeline.id)
   {
#ifdef HAVE_SHADERPIPELINE
#endif
      default:
      {
         if (_clearNextRender)
         {
            // TODO(sgc): draw quad to clear
            _clearNextRender = NO;
         }
   
         id<MTLRenderCommandEncoder> rce = _context.rce;
         
         [rce setRenderPipelineState:[_driver getStockShader:VIDEO_SHADER_STOCK_BLEND blend:_blend]];
         [rce setVertexBytes:uniforms length:sizeof(*uniforms) atIndex:BufferIndexUniforms];
         [rce setVertexBytes:buf length:sizeof(buf) atIndex:BufferIndexPositions];
         [rce setFragmentTexture:tex.texture atIndex:TextureIndexColor];
         [rce setFragmentSamplerState:tex.sampler atIndex:SamplerIndexDraw];
         [rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
         
         break;
      }
   }
}
@end
