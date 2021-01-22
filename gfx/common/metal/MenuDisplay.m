/*
 * Created by Stuart Carnie on 6/24/18.
 */

#import <Metal/Metal.h>

#import "Context.h"
#import "MenuDisplay.h"
#import "ShaderTypes.h"

#include "../../../menu/menu_driver.h"

@implementation MenuDisplay
{
   Context *_context;
   MTLClearColor _clearColor;
   MTLScissorRect _scissorRect;
   BOOL _useScissorRect;
   Uniforms _uniforms;
   bool _clearNextRender;
}

- (instancetype)initWithContext:(Context *)context
{
   if (self = [super init])
   {
      _context                   = context;
      _clearColor                = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
      _uniforms.projectionMatrix = matrix_proj_ortho(0, 1, 0, 1);
      _useScissorRect            = NO;
   }
   return self;
}

+ (const float *)defaultVertices
{
   static float dummy[8] = {
      0.0f, 0.0f,
      1.0f, 0.0f,
      0.0f, 1.0f,
      1.0f, 1.0f,
   };
   return &dummy[0];
}

+ (const float *)defaultTexCoords
{
   static float dummy[8] = {
      0.0f, 1.0f,
      1.0f, 1.0f,
      0.0f, 0.0f,
      1.0f, 0.0f,
   };
   return &dummy[0];
}

+ (const float *)defaultColor
{
   static float dummy[16] = {
      1.0f, 0.0f, 1.0f, 1.0f,
      1.0f, 0.0f, 1.0f, 1.0f,
      1.0f, 0.0f, 1.0f, 1.0f,
      1.0f, 0.0f, 1.0f, 1.0f,
   };
   return &dummy[0];
}

- (void)setClearColor:(MTLClearColor)clearColor
{
   _clearColor      = clearColor;
   _clearNextRender = YES;
}

- (MTLClearColor)clearColor
{
   return _clearColor;
}

- (void)setScissorRect:(MTLScissorRect)rect
{
   _scissorRect = rect;
   _useScissorRect = YES;
}

- (void)clearScissorRect
{
   _useScissorRect = NO;
   [_context resetScissorRect];
}

- (MTLPrimitiveType)_toPrimitiveType:(enum gfx_display_prim_type)prim
{
   switch (prim)
   {
      case GFX_DISPLAY_PRIM_TRIANGLESTRIP:
         return MTLPrimitiveTypeTriangleStrip;
      case GFX_DISPLAY_PRIM_TRIANGLES:
      default:
         /* Unexpected primitive type, defaulting to triangle */
         break;
   }

   return MTLPrimitiveTypeTriangle;
}

- (void)drawPipeline:(gfx_display_ctx_draw_t *)draw
{
   static struct video_coords blank_coords;

   draw->x = 0;
   draw->y = 0;
   draw->matrix_data = NULL;

   _uniforms.outputSize = simd_make_float2(_context.viewport->full_width, _context.viewport->full_height);

   draw->backend_data = &_uniforms;
   draw->backend_data_size = sizeof(_uniforms);

   switch (draw->pipeline_id)
   {
      /* ribbon */
      default:
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      {
         gfx_display_t *p_disp   = disp_get_ptr();
         video_coord_array_t *ca = &p_disp->dispca;
         draw->coords            = (struct video_coords *)&ca->coords;
         break;
      }

      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
      {
         draw->coords          = &blank_coords;
         blank_coords.vertices = 4;
         draw->prim_type       = GFX_DISPLAY_PRIM_TRIANGLESTRIP;
         break;
      }
   }

   _uniforms.time += 0.01;
}

- (void)draw:(gfx_display_ctx_draw_t *)draw
{
   unsigned i;
   BufferRange range;
   NSUInteger vertex_count;
   SpriteVertex *pv;
   const float *vertex    = draw->coords->vertex ?: MenuDisplay.defaultVertices;
   const float *tex_coord = draw->coords->tex_coord ?: MenuDisplay.defaultTexCoords;
   const float *color     = draw->coords->color ?: MenuDisplay.defaultColor;
   NSUInteger needed      = draw->coords->vertices * sizeof(SpriteVertex);
   if (![_context allocRange:&range length:needed])
      return;

   vertex_count           = draw->coords->vertices;
   pv                     = (SpriteVertex *)range.data;

   for (i = 0; i < draw->coords->vertices; i++, pv++)
   {
      pv->position = simd_make_float2(vertex[0], 1.0f - vertex[1]);
      vertex += 2;

      pv->texCoord = simd_make_float2(tex_coord[0], tex_coord[1]);
      tex_coord += 2;

      pv->color = simd_make_float4(color[0], color[1], color[2], color[3]);
      color += 4;
   }

   id<MTLRenderCommandEncoder> rce = _context.rce;
   if (_clearNextRender)
   {
      [_context resetRenderViewport:kFullscreenViewport];
      [_context drawQuadX:0
                        y:0
                        w:1
                        h:1
                        r:(float)_clearColor.red
                        g:(float)_clearColor.green
                        b:(float)_clearColor.blue
                        a:(float)_clearColor.alpha
      ];
      _clearNextRender = NO;
   }

   MTLViewport vp = {
      .originX = draw->x,
      .originY = _context.viewport->full_height - draw->y - draw->height,
      .width   = draw->width,
      .height  = draw->height,
      .znear   = 0,
      .zfar    = 1,
   };
   [rce setViewport:vp];

   if (_useScissorRect)
      [rce setScissorRect:_scissorRect];

   switch (draw->pipeline_id)
   {
#if HAVE_SHADERPIPELINE
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         [rce setRenderPipelineState:[_context getStockShader:draw->pipeline_id blend:_blend]];
         [rce setVertexBytes:draw->backend_data length:draw->backend_data_size atIndex:BufferIndexUniforms];
         [rce setVertexBuffer:range.buffer offset:range.offset atIndex:BufferIndexPositions];
         [rce setFragmentBytes:draw->backend_data length:draw->backend_data_size atIndex:BufferIndexUniforms];
         [rce drawPrimitives:[self _toPrimitiveType:draw->prim_type] vertexStart:0 vertexCount:vertex_count];
         return;
#endif
      default:
         break;
   }

   Texture *tex = (__bridge Texture *)(void *)draw->texture;
   if (tex == nil)
      return;

   [rce setRenderPipelineState:[_context getStockShader:VIDEO_SHADER_STOCK_BLEND blend:_blend]];

   Uniforms uniforms = {
      .projectionMatrix = draw->matrix_data ? make_matrix_float4x4((const float *)draw->matrix_data)
                                            : _uniforms.projectionMatrix
   };
   [rce setVertexBytes:&uniforms length:sizeof(uniforms) atIndex:BufferIndexUniforms];
   [rce setVertexBuffer:range.buffer offset:range.offset atIndex:BufferIndexPositions];
   [rce setFragmentTexture:tex.texture atIndex:TextureIndexColor];
   [rce setFragmentSamplerState:tex.sampler atIndex:SamplerIndexDraw];
   [rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:vertex_count];
}
@end
