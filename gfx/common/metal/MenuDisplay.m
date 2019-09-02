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
      _context = context;
      _clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);
      _uniforms.projectionMatrix = matrix_proj_ortho(0, 1, 0, 1);
      _useScissorRect = NO;
   }
   return self;
}

+ (const float *)defaultVertices
{
   static float dummy[] = {
      0.0f, 0.0f,
      1.0f, 0.0f,
      0.0f, 1.0f,
      1.0f, 1.0f,
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

- (void)setClearColor:(MTLClearColor)clearColor
{
   _clearColor = clearColor;
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

- (void)drawPipeline:(menu_display_ctx_draw_t *)draw video:(video_frame_info_t *)video
{
   static struct video_coords blank_coords;

   draw->x = 0;
   draw->y = 0;
   draw->matrix_data = NULL;

   _uniforms.outputSize = simd_make_float2(_context.viewport->full_width, _context.viewport->full_height);

   draw->pipeline.backend_data = &_uniforms;
   draw->pipeline.backend_data_size = sizeof(_uniforms);

   switch (draw->pipeline.id)
   {
      // ribbon
      default:
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      {
         video_coord_array_t *ca = menu_display_get_coords_array();
         draw->coords = (struct video_coords *)&ca->coords;
         break;
      }

      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
      {
         draw->coords = &blank_coords;
         blank_coords.vertices = 4;
         draw->prim_type = MENU_DISPLAY_PRIM_TRIANGLESTRIP;
         break;
      }
   }

   _uniforms.time += 0.01;
}

- (void)draw:(menu_display_ctx_draw_t *)draw video:(video_frame_info_t *)video
{
   const float *vertex = draw->coords->vertex ?: MenuDisplay.defaultVertices;
   const float *tex_coord = draw->coords->tex_coord ?: MenuDisplay.defaultTexCoords;
   const float *color = draw->coords->color ?: MenuDisplay.defaultColor;

   NSUInteger needed = draw->coords->vertices * sizeof(SpriteVertex);
   BufferRange range;
   if (![_context allocRange:&range length:needed])
   {
      RARCH_ERR("[Metal]: MenuDisplay unable to allocate buffer of %d bytes", needed);
      return;
   }

   NSUInteger vertexCount = draw->coords->vertices;
   SpriteVertex *pv = (SpriteVertex *)range.data;
   for (unsigned i = 0; i < draw->coords->vertices; i++, pv++)
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

   if (_useScissorRect) {
      [rce setScissorRect:_scissorRect];
   }

   switch (draw->pipeline.id)
   {
#if HAVE_SHADERPIPELINE
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         [rce setRenderPipelineState:[_context getStockShader:draw->pipeline.id blend:_blend]];
         [rce setVertexBytes:draw->pipeline.backend_data length:draw->pipeline.backend_data_size atIndex:BufferIndexUniforms];
         [rce setVertexBuffer:range.buffer offset:range.offset atIndex:BufferIndexPositions];
         [rce setFragmentBytes:draw->pipeline.backend_data length:draw->pipeline.backend_data_size atIndex:BufferIndexUniforms];
         [rce drawPrimitives:[self _toPrimitiveType:draw->prim_type] vertexStart:0 vertexCount:vertexCount];
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
   [rce drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:vertexCount];
}
@end
