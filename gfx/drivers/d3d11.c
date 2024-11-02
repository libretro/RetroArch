/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2018 - Ali Bouhlel
 *  Copyright (C) 2016-2019 - Brad Parker
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

/* Direct3D 11 driver.
 *
 * Minimum version : Direct3D 11.0 (Feature Level 11.0) (2009)
 * Minimum OS      : Windows Vista, Windows 7
 * Recommended OS  : Windows 7 and/or later
 */

#define CINTERFACE
#define COBJMACROS

#include <string.h>
#include <malloc.h>
#include <math.h>

#include <string/stdstring.h>
#include <gfx/scaler/pixconv.h>
#include <retro_miscellaneous.h>
#include <file/file_path.h>
#include <encodings/utf.h>
#include <lists/string_list.h>
#include <formats/image.h>

#include <dxgi.h>

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include "../font_driver.h"
#include "../common/win32_common.h"
#include "../video_shader_parse.h"
#include "../drivers_shader/slang_process.h"
#include "../../verbosity.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../performance_counters.h"
#include "../../menu/menu_driver.h"
#ifdef HAVE_REWIND
#include "../../state_manager.h"
#endif

#include "../common/d3d_common.h"
#include "../common/d3d11_defines.h"
#include "../common/dxgi_common.h"
#include "../common/d3dcompiler_common.h"
#ifdef HAVE_SLANG
#include "../drivers_shader/slang_process.h"
#endif

#ifdef __WINRT__
#include "../../uwp/uwp_func.h"
#else
#ifdef __cplusplus
extern const GUID DECLSPEC_SELECTANY libretro_IID_IDXGIFactory5 = { 0x7632e1f5,0xee65,0x4dca, { 0x87,0xfd,0x84,0xcd,0x75,0xf8,0x83,0x8d } };
#else
const GUID DECLSPEC_SELECTANY libretro_IID_IDXGIFactory5 = { 0x7632e1f5,0xee65,0x4dca, { 0x87,0xfd,0x84,0xcd,0x75,0xf8,0x83,0x8d } };
#endif
#endif

#define D3D11_ROLLING_SCANLINE_SIMULATION

/* Temporary workaround for d3d11 not being able to poll flags during init */
static gfx_ctx_driver_t d3d11_fake_context;

static D3D11Device           cached_device_d3d11;
static D3D_FEATURE_LEVEL     cached_supportedFeatureLevel;
static D3D11DeviceContext    cached_context_d3d11;

/*
 * D3D11 COMMON
 */

static INLINE void d3d11_release_texture(d3d11_texture_t* texture)
{
   Release(texture->handle);
   Release(texture->staging);
   Release(texture->view);
   Release(texture->rt_view);
}

static bool d3d11_init_shader(
      D3D11Device                     device,
      const char*                     src,
      size_t                          size,
      const void*                     src_name,
      LPCSTR                          vs_entry,
      LPCSTR                          ps_entry,
      LPCSTR                          gs_entry,
      const D3D11_INPUT_ELEMENT_DESC* input_element_descs,
      UINT                            num_elements,
      d3d11_shader_t*                 out,
      enum d3d11_feature_level_hint   hint)
{
   D3DBlob vs_code    = NULL;
   D3DBlob ps_code    = NULL;
   D3DBlob gs_code    = NULL;
   bool success       = true;
   const char *vs_str = NULL;
   const char *ps_str = NULL;
   const char *gs_str = NULL;

   switch (hint)
   {
      case D3D11_FEATURE_LEVEL_HINT_11_0:
      case D3D11_FEATURE_LEVEL_HINT_11_1:
      case D3D11_FEATURE_LEVEL_HINT_12_0:
      case D3D11_FEATURE_LEVEL_HINT_12_1:
      case D3D11_FEATURE_LEVEL_HINT_12_2:
         vs_str       = "vs_5_0";
         ps_str       = "ps_5_0";
         gs_str       = "gs_5_0";
         break;
      case D3D11_FEATURE_LEVEL_HINT_DONTCARE:
      default:
         vs_str       = "vs_4_0";
         ps_str       = "ps_4_0";
         gs_str       = "gs_4_0";
         break;
   }

   if (!src) /* LPCWSTR filename */
   {
      if (vs_entry && !d3d_compile_from_file((LPCWSTR)src_name, vs_entry, vs_str, &vs_code))
         success = false;
      if (ps_entry && !d3d_compile_from_file((LPCWSTR)src_name, ps_entry, ps_str, &ps_code))
         success = false;
      if (gs_entry && !d3d_compile_from_file((LPCWSTR)src_name, gs_entry, gs_str, &gs_code))
         success = false;
   }
   else /* char array */
   {
      if (vs_entry && !d3d_compile(src, size, (LPCSTR)src_name, vs_entry, vs_str, &vs_code))
         success = false;
      if (ps_entry && !d3d_compile(src, size, (LPCSTR)src_name, ps_entry, ps_str, &ps_code))
         success = false;
      if (gs_entry && !d3d_compile(src, size, (LPCSTR)src_name, gs_entry, gs_str, &gs_code))
         success = false;
   }

   if (ps_code)
      device->lpVtbl->CreatePixelShader(
            device,
            ps_code->lpVtbl->GetBufferPointer(ps_code),
            ps_code->lpVtbl->GetBufferSize(ps_code),
            NULL, &out->ps);

   if (gs_code)
      device->lpVtbl->CreateGeometryShader(
            device,
            gs_code->lpVtbl->GetBufferPointer(gs_code),
            gs_code->lpVtbl->GetBufferSize(gs_code),
            NULL, &out->gs);

   if (vs_code)
   {
      LPVOID buf_ptr  = vs_code->lpVtbl->GetBufferPointer(vs_code);
      SIZE_T buf_size = vs_code->lpVtbl->GetBufferSize(vs_code);
      device->lpVtbl->CreateVertexShader(device, buf_ptr, buf_size, NULL, &out->vs);
      if (input_element_descs)
         device->lpVtbl->CreateInputLayout(device, input_element_descs, num_elements,
               buf_ptr, buf_size, &out->layout);
   }

   Release(vs_code);
   Release(ps_code);
   Release(gs_code);

   return success;
}

static DXGI_FORMAT
d3d11_get_closest_match(D3D11Device device, DXGI_FORMAT desired_format, UINT desired_format_support)
{
   DXGI_FORMAT default_list[] = {desired_format, DXGI_FORMAT_UNKNOWN};
   DXGI_FORMAT* format        = dxgi_get_format_fallback_list(desired_format);

   if (!format)
      format = default_list;

   while (*format != DXGI_FORMAT_UNKNOWN)
   {
      UINT         format_support;
      if (SUCCEEDED(device->lpVtbl->CheckFormatSupport(device, *format,
                  &format_support))
            && ((format_support & desired_format_support) == desired_format_support))
         break;
      format++;
   }
   return *format;
}

static void d3d11_init_texture(D3D11Device device, d3d11_texture_t* texture)
{
   bool is_render_target            = texture->desc.BindFlags & D3D11_BIND_RENDER_TARGET;
   UINT format_support              = D3D11_FORMAT_SUPPORT_TEXTURE2D | D3D11_FORMAT_SUPPORT_SHADER_SAMPLE;

   texture->desc.MipLevels          = 1;
   texture->desc.ArraySize          = 1;
   texture->desc.SampleDesc.Count   = 1;
   texture->desc.SampleDesc.Quality = 0;
   texture->desc.BindFlags         |= D3D11_BIND_SHADER_RESOURCE;
   texture->desc.CPUAccessFlags     =
      texture->desc.Usage == D3D11_USAGE_DYNAMIC ? D3D11_CPU_ACCESS_WRITE : 0;

   if (texture->desc.MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS)
   {
      unsigned width, height;

      texture->desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
      width                    = texture->desc.Width  >> 5;
      height                   = texture->desc.Height >> 5;

      while (width && height)
      {
         width  >>= 1;
         height >>= 1;
         texture->desc.MipLevels++;
      }
   }

   if (texture->desc.BindFlags & D3D11_BIND_RENDER_TARGET)
      format_support |= D3D11_FORMAT_SUPPORT_RENDER_TARGET;

   texture->desc.Format = d3d11_get_closest_match(device, texture->desc.Format, format_support);

   device->lpVtbl->CreateTexture2D(device, &texture->desc, NULL,
         &texture->handle);

   {
      D3D11_SHADER_RESOURCE_VIEW_DESC view_desc;
      view_desc.Format                          = texture->desc.Format;
      view_desc.ViewDimension                   = D3D_SRV_DIMENSION_TEXTURE2D;
      view_desc.Texture2D.MostDetailedMip       = 0;
      view_desc.Texture2D.MipLevels             = -1;
      device->lpVtbl->CreateShaderResourceView(device,
            (D3D11Resource)texture->handle, &view_desc, &texture->view);
   }

   if (is_render_target)
      device->lpVtbl->CreateRenderTargetView(device,
            (D3D11Resource)texture->handle, NULL, &texture->rt_view);
   else
   {
      D3D11_TEXTURE2D_DESC desc = texture->desc;
      desc.MipLevels            = 1;
      desc.BindFlags            = 0;
      desc.MiscFlags            = 0;
      desc.Usage                = D3D11_USAGE_STAGING;
      desc.CPUAccessFlags       = D3D11_CPU_ACCESS_WRITE;
      device->lpVtbl->CreateTexture2D(device, &desc, NULL, &texture->staging);
   }

   texture->size_data.x = texture->desc.Width;
   texture->size_data.y = texture->desc.Height;
   texture->size_data.z = 1.0f / texture->desc.Width;
   texture->size_data.w = 1.0f / texture->desc.Height;
}

static void d3d11_update_texture(
      D3D11DeviceContext ctx,
      unsigned           width,
      unsigned           height,
      unsigned           pitch,
      DXGI_FORMAT        format,
      const void*        data,
      d3d11_texture_t*   texture)
{
   D3D11_MAPPED_SUBRESOURCE mapped_texture;
   D3D11_BOX frame_box;

   ctx->lpVtbl->Map(
         ctx, (D3D11Resource)texture->staging, 0, D3D11_MAP_WRITE, 0, &mapped_texture);

   dxgi_copy(
         width, height, format, pitch, data,
         texture->desc.Format, mapped_texture.RowPitch,
         mapped_texture.pData);

   frame_box.left   = 0;
   frame_box.top    = 0;
   frame_box.front  = 0;
   frame_box.right  = width;
   frame_box.bottom = height;
   frame_box.back   = 1;
   ctx->lpVtbl->Unmap(ctx, (D3D11Resource)texture->staging, 0);
   ctx->lpVtbl->CopySubresourceRegion(
         ctx, (D3D11Resource)texture->handle, 0, 0, 0, 0,
         (D3D11Resource)texture->staging, 0, &frame_box);

   if (texture->desc.MiscFlags & D3D11_RESOURCE_MISC_GENERATE_MIPS)
      ctx->lpVtbl->GenerateMips(ctx, texture->view);
}

/*
 * DISPLAY DRIVER
 */

static void gfx_display_d3d11_blend_begin(void *data)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;
   d3d11->context->lpVtbl->OMSetBlendState(d3d11->context, d3d11->blend_enable, NULL, D3D11_DEFAULT_SAMPLE_MASK);
}

static void gfx_display_d3d11_blend_end(void *data)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;
   d3d11->context->lpVtbl->OMSetBlendState(d3d11->context, d3d11->blend_disable, NULL, D3D11_DEFAULT_SAMPLE_MASK);
}

static void gfx_display_d3d11_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   int vertex_count     = 1;
   d3d11_video_t *d3d11 = (d3d11_video_t*)data;

   if (!d3d11 || !draw || !draw->texture)
      return;

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         {
            d3d11_shader_t *shader = &d3d11->shaders[draw->pipeline_id];
            d3d11->context->lpVtbl->IASetInputLayout(d3d11->context, shader->layout);
            d3d11->context->lpVtbl->VSSetShader(d3d11->context, shader->vs, NULL, 0);
            d3d11->context->lpVtbl->PSSetShader(d3d11->context, shader->ps, NULL, 0);
            d3d11->context->lpVtbl->GSSetShader(d3d11->context, shader->gs, NULL, 0);
         }

         d3d11->context->lpVtbl->Draw(d3d11->context, draw->coords->vertices, 0);
         d3d11->context->lpVtbl->OMSetBlendState(d3d11->context, d3d11->blend_enable, NULL, D3D11_DEFAULT_SAMPLE_MASK);

         {
            d3d11_shader_t *shader = &d3d11->sprites.shader;
            d3d11->context->lpVtbl->IASetInputLayout(d3d11->context, shader->layout);
            d3d11->context->lpVtbl->VSSetShader(d3d11->context, shader->vs, NULL, 0);
            d3d11->context->lpVtbl->PSSetShader(d3d11->context, shader->ps, NULL, 0);
            d3d11->context->lpVtbl->GSSetShader(d3d11->context, shader->gs, NULL, 0);
         }
         {
            UINT stride = sizeof(d3d11_sprite_t);
            UINT offset = 0;
            d3d11->context->lpVtbl->IASetVertexBuffers(
                  d3d11->context, 0, 1,
                  &d3d11->sprites.vbo, &stride, &offset);
         }
         d3d11->context->lpVtbl->IASetPrimitiveTopology(d3d11->context, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
         return;
   }

   if (draw->coords->vertex && draw->coords->tex_coord && draw->coords->color)
      vertex_count = draw->coords->vertices;

   if (     (!(d3d11->flags & D3D11_ST_FLAG_SPRITES_ENABLE))
         || (vertex_count > d3d11->sprites.capacity))
      return;

   if (d3d11->sprites.offset + vertex_count > d3d11->sprites.capacity)
      d3d11->sprites.offset = 0;

   {
      D3D11_MAPPED_SUBRESOURCE mapped_vbo;
      d3d11_sprite_t*          sprite = NULL;

      d3d11->context->lpVtbl->Map(
            d3d11->context, (D3D11Resource)d3d11->sprites.vbo, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped_vbo);

      sprite = (d3d11_sprite_t*)mapped_vbo.pData + d3d11->sprites.offset;

      if (vertex_count == 1)
      {
         sprite->pos.x    = draw->x / (float)d3d11->viewport.Width;
         sprite->pos.y    =
            (d3d11->viewport.Height - draw->y - draw->height) / (float)d3d11->viewport.Height;
         sprite->pos.w    = draw->width / (float)d3d11->viewport.Width;
         sprite->pos.h    = draw->height / (float)d3d11->viewport.Height;

         sprite->coords.u = 0.0f;
         sprite->coords.v = 0.0f;
         sprite->coords.w = 1.0f;
         sprite->coords.h = 1.0f;

         if (draw->scale_factor)
            sprite->params.scaling = draw->scale_factor;
         else
            sprite->params.scaling = 1.0f;

         sprite->params.rotation   = draw->rotation;

         sprite->colors[3]         = DXGI_COLOR_RGBA(
               0xFF * draw->coords->color[0], 0xFF * draw->coords->color[1],
               0xFF * draw->coords->color[2], 0xFF * draw->coords->color[3]);
         sprite->colors[2]         = DXGI_COLOR_RGBA(
               0xFF * draw->coords->color[4], 0xFF * draw->coords->color[5],
               0xFF * draw->coords->color[6], 0xFF * draw->coords->color[7]);
         sprite->colors[1]         = DXGI_COLOR_RGBA(
               0xFF * draw->coords->color[8], 0xFF * draw->coords->color[9],
               0xFF * draw->coords->color[10], 0xFF * draw->coords->color[11]);
         sprite->colors[0]         = DXGI_COLOR_RGBA(
               0xFF * draw->coords->color[12], 0xFF * draw->coords->color[13],
               0xFF * draw->coords->color[14], 0xFF * draw->coords->color[15]);
      }
      else
      {
         int i;
         const float* vertex    = draw->coords->vertex;
         const float* tex_coord = draw->coords->tex_coord;
         const float* color     = draw->coords->color;

         for (i = 0; i < vertex_count; i++)
         {
            d3d11_vertex_t* v = (d3d11_vertex_t*)sprite;
            v->position[0]    = *vertex++;
            v->position[1]    = *vertex++;
            v->texcoord[0]    = *tex_coord++;
            v->texcoord[1]    = *tex_coord++;
            v->color[0]       = *color++;
            v->color[1]       = *color++;
            v->color[2]       = *color++;
            v->color[3]       = *color++;

            sprite++;
         }

         {
            d3d11_shader_t *shader = &d3d11->shaders[VIDEO_SHADER_STOCK_BLEND];
            d3d11->context->lpVtbl->IASetInputLayout(d3d11->context, shader->layout);
            d3d11->context->lpVtbl->VSSetShader(d3d11->context, shader->vs, NULL, 0);
            d3d11->context->lpVtbl->PSSetShader(d3d11->context, shader->ps, NULL, 0);
            d3d11->context->lpVtbl->GSSetShader(d3d11->context, shader->gs, NULL, 0);
         }
         d3d11->context->lpVtbl->IASetPrimitiveTopology(d3d11->context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      }

      d3d11->context->lpVtbl->Unmap(d3d11->context, (D3D11Resource)d3d11->sprites.vbo, 0);
   }

   {
      d3d11_texture_t *texture = (d3d11_texture_t*)draw->texture;
      d3d11->context->lpVtbl->PSSetShaderResources(
            d3d11->context, 0, 1, &texture->view);
      d3d11->context->lpVtbl->PSSetSamplers(
            d3d11->context, 0, 1, (D3D11SamplerState*)&texture->sampler);
   }

   d3d11->context->lpVtbl->Draw(d3d11->context, vertex_count,
         d3d11->sprites.offset);
   d3d11->sprites.offset += vertex_count;

   if (vertex_count > 1)
   {
      d3d11_shader_t *shader = &d3d11->sprites.shader;
      d3d11->context->lpVtbl->IASetInputLayout(d3d11->context, shader->layout);
      d3d11->context->lpVtbl->VSSetShader(d3d11->context, shader->vs, NULL, 0);
      d3d11->context->lpVtbl->PSSetShader(d3d11->context, shader->ps, NULL, 0);
      d3d11->context->lpVtbl->GSSetShader(d3d11->context, shader->gs, NULL, 0);
      d3d11->context->lpVtbl->IASetPrimitiveTopology(d3d11->context, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
   }
}

static void gfx_display_d3d11_draw_pipeline(gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp,
      void *data, unsigned video_width, unsigned video_height)
{
   d3d11_video_t *d3d11 = (d3d11_video_t*)data;

   if (!d3d11 || !draw)
      return;

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      {
         video_coord_array_t* ca   = &p_disp->dispca;

         if (!d3d11->menu_pipeline_vbo)
         {
            D3D11_BUFFER_DESC desc;
            desc.Usage               = D3D11_USAGE_IMMUTABLE;
            desc.ByteWidth           = ca->coords.vertices * 2 * sizeof(float);
            desc.BindFlags           = D3D11_BIND_VERTEX_BUFFER;
            desc.CPUAccessFlags      = 0;
            desc.MiscFlags           = 0;
            desc.StructureByteStride = 0;

            {
               D3D11_SUBRESOURCE_DATA vertex_data;
               vertex_data.pSysMem          = ca->coords.vertex;
               vertex_data.SysMemPitch      = 0;
               vertex_data.SysMemSlicePitch = 0;
               d3d11->device->lpVtbl->CreateBuffer(
                     d3d11->device, &desc, &vertex_data,
                     &d3d11->menu_pipeline_vbo);
            }
         }
         {
            UINT stride = 2 * sizeof(float);
            UINT offset = 0;
            d3d11->context->lpVtbl->IASetVertexBuffers(
                  d3d11->context, 0, 1,
                  &d3d11->menu_pipeline_vbo, &stride, &offset);
         }
         draw->coords->vertices = ca->coords.vertices;
         d3d11->context->lpVtbl->OMSetBlendState(
               d3d11->context, d3d11->blend_pipeline,
               NULL, D3D11_DEFAULT_SAMPLE_MASK);
         break;
      }

      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         {
            UINT stride = sizeof(d3d11_vertex_t);
            UINT offset = 0;
            d3d11->context->lpVtbl->IASetVertexBuffers(
                  d3d11->context, 0, 1,
                  &d3d11->frame.vbo, &stride, &offset);
         }
         draw->coords->vertices = 4;
         break;
      default:
         return;
   }

   d3d11->context->lpVtbl->IASetPrimitiveTopology(
         d3d11->context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

   d3d11->ubo_values.time += 0.01f;

   {
      D3D11_MAPPED_SUBRESOURCE mapped_ubo;
      d3d11->context->lpVtbl->Map(
         d3d11->context, (D3D11Resource)d3d11->ubo,
         0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_ubo);
      *(d3d11_uniform_t*)mapped_ubo.pData = d3d11->ubo_values;
      d3d11->context->lpVtbl->Unmap(d3d11->context,
            (D3D11Resource)d3d11->ubo, 0);
   }
}

void gfx_display_d3d11_scissor_begin(void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y, unsigned width, unsigned height)
{
   D3D11_RECT rect;
   d3d11_video_t *d3d11 = (d3d11_video_t*)data;

   if (!d3d11)
      return;

   rect.left            = x;
   rect.top             = y;
   rect.right           = width  + x;
   rect.bottom          = height + y;

   d3d11->context->lpVtbl->RSSetScissorRects(d3d11->context, 1, &rect);
}

void gfx_display_d3d11_scissor_end(void *data,
      unsigned video_width,
      unsigned video_height)
{
   D3D11_RECT rect;
   d3d11_video_t *d3d11  = (d3d11_video_t*)data;

   if (!d3d11)
      return;

   rect.left            = 0;
   rect.top             = 0;
   rect.right           = video_width;
   rect.bottom          = video_height;

   d3d11->context->lpVtbl->RSSetScissorRects(d3d11->context, 1, &rect);
}

gfx_display_ctx_driver_t gfx_display_ctx_d3d11 = {
   gfx_display_d3d11_draw,
   gfx_display_d3d11_draw_pipeline,
   gfx_display_d3d11_blend_begin,
   gfx_display_d3d11_blend_end,
   NULL,                                     /* get_default_mvp */
   NULL,                                     /* get_default_vertices */
   NULL,                                     /* get_default_tex_coords */
   FONT_DRIVER_RENDER_D3D11_API,
   GFX_VIDEO_DRIVER_DIRECT3D11,
   "d3d11",
   true,
   gfx_display_d3d11_scissor_begin,
   gfx_display_d3d11_scissor_end
};

/*
 * FONT DRIVER
 */

typedef struct
{
   d3d11_texture_t               texture;
   const font_renderer_driver_t* font_driver;
   void*                         font_data;
   struct font_atlas*            atlas;
} d3d11_font_t;

static void * d3d11_font_init(void* data, const char* font_path,
      float font_size, bool is_threaded)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;
   d3d11_font_t*  font  = (d3d11_font_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   if (!font_renderer_create_default(
             &font->font_driver, &font->font_data, font_path, font_size))
   {
      free(font);
      return NULL;
   }

   font->atlas               = font->font_driver->get_atlas(font->font_data);
   font->texture.sampler     = d3d11->samplers[RARCH_FILTER_LINEAR][RARCH_WRAP_BORDER];
   font->texture.desc.Width  = font->atlas->width;
   font->texture.desc.Height = font->atlas->height;
   font->texture.desc.Format = DXGI_FORMAT_A8_UNORM;
   d3d11_release_texture(&font->texture);
   d3d11_init_texture(d3d11->device, &font->texture);
   if (font->texture.staging)
      d3d11_update_texture(
            d3d11->context, font->atlas->width, font->atlas->height, font->atlas->width,
            DXGI_FORMAT_A8_UNORM, font->atlas->buffer, &font->texture);
   font->atlas->dirty = false;

   return font;
}

static void d3d11_font_free(void* data, bool is_threaded)
{
   d3d11_font_t* font = (d3d11_font_t*)data;

   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   Release(font->texture.handle);
   Release(font->texture.staging);
   Release(font->texture.view);
   free(font);
}

static int d3d11_font_get_message_width(void* data, const char* msg, size_t msg_len, float scale)
{
   size_t i;
   int delta_x                      = 0;
   const struct font_glyph* glyph_q = NULL;
   d3d11_font_t* font               = (d3d11_font_t*)data;

   if (!font)
      return 0;

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph *glyph;
      const char* msg_tmp = &msg[i];
      unsigned    code    = utf8_walk(&msg_tmp);
      unsigned    skip    = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      /* Do something smarter here ... */
      if (!(glyph = font->font_driver->get_glyph(font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

static void d3d11_font_render_line(
      d3d11_video_t* d3d11,
      d3d11_font_t*       font,
      const struct font_glyph* glyph_q,
      const char*         msg,
      size_t              msg_len,
      float               scale,
      const unsigned int  color,
      float               pos_x,
      float               pos_y,
      int                 pre_x,
      unsigned            width,
      unsigned            height,
      unsigned            text_align)
{
   size_t i;
   unsigned count;
   D3D11_MAPPED_SUBRESOURCE mapped_vbo;
   d3d11_sprite_t *v                = NULL;
   int x                            = pre_x;
   int y                            = roundf((1.0 - pos_y) * height);

   if (d3d11->sprites.offset + msg_len > (unsigned)d3d11->sprites.capacity)
      d3d11->sprites.offset = 0;

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= d3d11_font_get_message_width(font, msg, msg_len, scale);
         break;

      case TEXT_ALIGN_CENTER:
         x -= d3d11_font_get_message_width(font, msg, msg_len, scale) / 2;
         break;
   }

   d3d11->context->lpVtbl->Map(
         d3d11->context, (D3D11Resource)d3d11->sprites.vbo, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped_vbo);
   v       = (d3d11_sprite_t*)mapped_vbo.pData + d3d11->sprites.offset;

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph* glyph;
      const char *msg_tmp= &msg[i];
      unsigned   code    = utf8_walk(&msg_tmp);
      unsigned   skip    = msg_tmp - &msg[i];

      if (skip > 1)
         i              += skip - 1;

      /* Do something smarter here ... */
      if (!(glyph = font->font_driver->get_glyph(font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      v->pos.x           = (x + (glyph->draw_offset_x * scale)) / (float)d3d11->viewport.Width;
      v->pos.y           = (y + (glyph->draw_offset_y * scale)) / (float)d3d11->viewport.Height;
      v->pos.w           = glyph->width               * scale   / (float)d3d11->viewport.Width;
      v->pos.h           = glyph->height              * scale   / (float)d3d11->viewport.Height;

      v->coords.u        = glyph->atlas_offset_x / (float)font->texture.desc.Width;
      v->coords.v        = glyph->atlas_offset_y / (float)font->texture.desc.Height;
      v->coords.w        = glyph->width          / (float)font->texture.desc.Width;
      v->coords.h        = glyph->height         / (float)font->texture.desc.Height;

      v->params.scaling  = 1;
      v->params.rotation = 0;

      v->colors[0]       = color;
      v->colors[1]       = color;
      v->colors[2]       = color;
      v->colors[3]       = color;

      v++;

      x                 += glyph->advance_x * scale;
      y                 += glyph->advance_y * scale;
   }

   count = v - ((d3d11_sprite_t*)mapped_vbo.pData + d3d11->sprites.offset);
   d3d11->context->lpVtbl->Unmap(d3d11->context, (D3D11Resource)d3d11->sprites.vbo, 0);

   if (!count)
      return;

   if (font->atlas->dirty)
   {
      if (font->texture.staging)
         d3d11_update_texture(
               d3d11->context,
               font->atlas->width, font->atlas->height, font->atlas->width,
               DXGI_FORMAT_A8_UNORM, font->atlas->buffer, &font->texture);
      font->atlas->dirty = false;
   }

   {
      d3d11_texture_t *texture = (d3d11_texture_t*)&font->texture;
      d3d11->context->lpVtbl->PSSetShaderResources(
            d3d11->context, 0, 1, &texture->view);
      d3d11->context->lpVtbl->PSSetSamplers(
            d3d11->context, 0, 1,
            (D3D11SamplerState*)&texture->sampler);
   }
   d3d11->context->lpVtbl->OMSetBlendState(d3d11->context, d3d11->blend_enable,
         NULL, D3D11_DEFAULT_SAMPLE_MASK);

   d3d11->context->lpVtbl->PSSetShader(d3d11->context, d3d11->sprites.shader_font.ps, NULL, 0);
   d3d11->context->lpVtbl->Draw(d3d11->context, count, d3d11->sprites.offset);
   d3d11->context->lpVtbl->PSSetShader(d3d11->context, d3d11->sprites.shader.ps, NULL, 0);

   d3d11->sprites.offset += count;
}

static void d3d11_font_render_message(
      d3d11_video_t *d3d11,
      d3d11_font_t*       font,
      const char*         msg,
      float               scale,
      const unsigned int  color,
      float               pos_x,
      float               pos_y,
      unsigned            width,
      unsigned            height,
      unsigned            text_align)
{
   float line_height;
   struct font_line_metrics *line_metrics = NULL;
   int lines                              = 0;
   int x                                  = roundf(pos_x * width);
   const struct font_glyph* glyph_q       = font->font_driver->get_glyph(font->font_data, '?');

   font->font_driver->get_line_metrics(font->font_data, &line_metrics);
   line_height = line_metrics->height * scale / height;

   for (;;)
   {
      const char* delim = strchr(msg, '\n');
      size_t msg_len    = delim ? (size_t)(delim - msg) : strlen(msg);

      /* Draw the line */
      if (msg_len <= (unsigned)d3d11->sprites.capacity)
         d3d11_font_render_line(d3d11,
               font, glyph_q, msg, msg_len, scale, color, pos_x,
               pos_y - (float)lines * line_height,
               x,
               width, height, text_align);

      if (!delim)
         break;

      msg += msg_len + 1;
      lines++;
   }
}

static void d3d11_font_render_msg(
      void *userdata,
      void* data,
      const char* msg,
      const struct font_params *params)
{
   int drop_x, drop_y;
   enum text_alignment text_align;
   unsigned color, r, g, b, alpha;
   float x, y, scale, drop_mod, drop_alpha;
   d3d11_font_t *font         = (d3d11_font_t*)data;
   d3d11_video_t *d3d11       = (d3d11_video_t*)userdata;
   unsigned width             = d3d11->vp.full_width;
   unsigned height            = d3d11->vp.full_height;

   if (!font || !msg || !*msg)
      return;
   if (!(d3d11->flags & D3D11_ST_FLAG_SPRITES_ENABLE))
      return;

   if (params)
   {
      x                       = params->x;
      y                       = params->y;
      scale                   = params->scale;
      text_align              = params->text_align;
      drop_x                  = params->drop_x;
      drop_y                  = params->drop_y;
      drop_mod                = params->drop_mod;
      drop_alpha              = params->drop_alpha;

      r                       = FONT_COLOR_GET_RED(params->color);
      g                       = FONT_COLOR_GET_GREEN(params->color);
      b                       = FONT_COLOR_GET_BLUE(params->color);
      alpha                   = FONT_COLOR_GET_ALPHA(params->color);

      color                   = DXGI_COLOR_RGBA(r, g, b, alpha);
   }
   else
   {
      settings_t *settings    = config_get_ptr();
      float video_msg_color_r = settings->floats.video_msg_color_r;
      float video_msg_color_g = settings->floats.video_msg_color_g;
      float video_msg_color_b = settings->floats.video_msg_color_b;
      float video_msg_pos_x   = settings->floats.video_msg_pos_x;
      float video_msg_pos_y   = settings->floats.video_msg_pos_y;
      x                       = video_msg_pos_x;
      y                       = video_msg_pos_y;
      scale                   = 1.0f;
      text_align              = TEXT_ALIGN_LEFT;

      r                       = (video_msg_color_r * 255);
      g                       = (video_msg_color_g * 255);
      b                       = (video_msg_color_b * 255);
      alpha                   = 255;
      color                   = DXGI_COLOR_RGBA(r, g, b, alpha);

      drop_x                  = -2;
      drop_y                  = -2;
      drop_mod                = 0.3f;
      drop_alpha              = 1.0f;
   }

   if (drop_x || drop_y)
   {
      unsigned r_dark         = r * drop_mod;
      unsigned g_dark         = g * drop_mod;
      unsigned b_dark         = b * drop_mod;
      unsigned alpha_dark     = alpha * drop_alpha;
      unsigned color_dark     = DXGI_COLOR_RGBA(r_dark, g_dark, b_dark, alpha_dark);

      if (d3d11->flags & D3D11_ST_FLAG_SPRITES_ENABLE)
         d3d11_font_render_message(d3d11,
               font, msg, scale, color_dark,
               x + scale * drop_x / width,
               y + scale * drop_y / height,
               width, height, text_align);
   }

   d3d11_font_render_message(d3d11, font, msg, scale,
         color, x, y, width, height, text_align);
}

static const struct font_glyph* d3d11_font_get_glyph(void *data, uint32_t code)
{
   d3d11_font_t* font = (d3d11_font_t*)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph((void*)font->font_driver, code);
   return NULL;
}

static bool d3d11_font_get_line_metrics(void* data, struct font_line_metrics **metrics)
{
   d3d11_font_t* font = (d3d11_font_t*)data;
   if (font && font->font_driver && font->font_data)
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

font_renderer_t d3d11_font = {
   d3d11_font_init,
   d3d11_font_free,
   d3d11_font_render_msg,
   "d3d11",
   d3d11_font_get_glyph,
   NULL, /* bind_block */
   NULL, /* flush */
   d3d11_font_get_message_width,
   d3d11_font_get_line_metrics
};

/*
 * VIDEO DRIVER
 */

static INLINE void d3d11_release_shader(d3d11_shader_t* shader)
{
   Release(shader->layout);
   Release(shader->vs);
   Release(shader->ps);
   Release(shader->gs);
}

static uint32_t d3d11_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_CUSTOMIZABLE_FRAME_LATENCY);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
   BIT32_SET(flags, GFX_CTX_FLAGS_OVERLAY_BEHIND_MENU_SUPPORTED);
   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
   BIT32_SET(flags, GFX_CTX_FLAGS_SUBFRAME_SHADERS);
#endif

   return flags;
}

#ifdef HAVE_OVERLAY
static void d3d11_free_overlays(d3d11_video_t* d3d11)
{
   int i;
   for (i = 0; i < d3d11->overlays.count; i++)
      d3d11_release_texture(&d3d11->overlays.textures[i]);

   Release(d3d11->overlays.vbo);
}

static void d3d11_overlay_vertex_geom(
      void* data, unsigned index,
      float x, float y, float w, float h)
{
   D3D11_MAPPED_SUBRESOURCE mapped_vbo;
   d3d11_video_t*           d3d11 = (d3d11_video_t*)data;

   if (!d3d11)
      return;

   d3d11->context->lpVtbl->Map(
         d3d11->context, (D3D11Resource)d3d11->overlays.vbo, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped_vbo);
   {
      d3d11_sprite_t* sprites = (d3d11_sprite_t*)mapped_vbo.pData;
      sprites[index].pos.x    = x;
      sprites[index].pos.y    = y;
      sprites[index].pos.w    = w;
      sprites[index].pos.h    = h;
   }
   d3d11->context->lpVtbl->Unmap(d3d11->context, (D3D11Resource)d3d11->overlays.vbo, 0);
}

static void d3d11_overlay_tex_geom(
      void* data, unsigned index,
      float u, float v, float w, float h)
{
   D3D11_MAPPED_SUBRESOURCE mapped_vbo;
   d3d11_video_t*           d3d11 = (d3d11_video_t*)data;

   if (!d3d11)
      return;

   d3d11->context->lpVtbl->Map(
         d3d11->context, (D3D11Resource)d3d11->overlays.vbo, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped_vbo);
   {
      d3d11_sprite_t* sprites = (d3d11_sprite_t*)mapped_vbo.pData;
      sprites[index].coords.u = u;
      sprites[index].coords.v = v;
      sprites[index].coords.w = w;
      sprites[index].coords.h = h;
   }
   d3d11->context->lpVtbl->Unmap(d3d11->context, (D3D11Resource)d3d11->overlays.vbo, 0);
}

static void d3d11_overlay_set_alpha(void* data, unsigned index, float mod)
{
   D3D11_MAPPED_SUBRESOURCE mapped_vbo;
   d3d11_video_t*           d3d11 = (d3d11_video_t*)data;

   if (!d3d11)
      return;

   d3d11->context->lpVtbl->Map(
         d3d11->context, (D3D11Resource)d3d11->overlays.vbo, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped_vbo);
   {
      d3d11_sprite_t* sprites  = (d3d11_sprite_t*)mapped_vbo.pData;
      sprites[index].colors[0] = DXGI_COLOR_RGBA(0xFF, 0xFF, 0xFF, mod * 0xFF);
      sprites[index].colors[1] = sprites[index].colors[0];
      sprites[index].colors[2] = sprites[index].colors[0];
      sprites[index].colors[3] = sprites[index].colors[0];
   }
   d3d11->context->lpVtbl->Unmap(d3d11->context, (D3D11Resource)d3d11->overlays.vbo, 0);
}

static bool d3d11_overlay_load(void* data, const void* image_data, unsigned num_images)
{
   D3D11_BUFFER_DESC desc;
   D3D11_MAPPED_SUBRESOURCE    mapped_vbo;
   unsigned                    i;
   d3d11_sprite_t*             sprites;
   d3d11_video_t*              d3d11  = (d3d11_video_t*)data;
   const struct texture_image* images = (const struct texture_image*)image_data;

   if (!d3d11)
      return false;

   d3d11_free_overlays(d3d11);
   d3d11->overlays.count    = num_images;
   d3d11->overlays.textures = (d3d11_texture_t*)calloc(
         num_images, sizeof(d3d11_texture_t));

   desc.ByteWidth           = sizeof(d3d11_sprite_t) * num_images;
   desc.Usage               = D3D11_USAGE_DYNAMIC;
   desc.BindFlags           = D3D11_BIND_VERTEX_BUFFER;
   desc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
   desc.MiscFlags           = 0;
   desc.StructureByteStride = 0;
   d3d11->device->lpVtbl->CreateBuffer(d3d11->device, &desc, NULL,
         &d3d11->overlays.vbo);

   d3d11->context->lpVtbl->Map(
         d3d11->context, (D3D11Resource)d3d11->overlays.vbo, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_vbo);
   sprites                  = (d3d11_sprite_t*)mapped_vbo.pData;

   for (i = 0; i < num_images; i++)
   {

      d3d11->overlays.textures[i].desc.Width  = images[i].width;
      d3d11->overlays.textures[i].desc.Height = images[i].height;
      d3d11->overlays.textures[i].desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

      d3d11_release_texture(&d3d11->overlays.textures[i]);
      d3d11_init_texture(d3d11->device, &d3d11->overlays.textures[i]);

      if (d3d11->overlays.textures[i].staging)
         d3d11_update_texture(
               d3d11->context, images[i].width,
               images[i].height, 0, DXGI_FORMAT_B8G8R8A8_UNORM,
               images[i].pixels, &d3d11->overlays.textures[i]);

      sprites[i].pos.x           = 0.0f;
      sprites[i].pos.y           = 0.0f;
      sprites[i].pos.w           = 1.0f;
      sprites[i].pos.h           = 1.0f;

      sprites[i].coords.u        = 0.0f;
      sprites[i].coords.v        = 0.0f;
      sprites[i].coords.w        = 1.0f;
      sprites[i].coords.h        = 1.0f;

      sprites[i].params.scaling  = 1;
      sprites[i].params.rotation = 0;

      sprites[i].colors[0]       = 0xFFFFFFFF;
      sprites[i].colors[1]       = sprites[i].colors[0];
      sprites[i].colors[2]       = sprites[i].colors[0];
      sprites[i].colors[3]       = sprites[i].colors[0];
   }
   d3d11->context->lpVtbl->Unmap(d3d11->context, (D3D11Resource)d3d11->overlays.vbo, 0);

   return true;
}

static void d3d11_overlay_enable(void* data, bool state)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   if (!d3d11)
      return;

   if (state)
      d3d11->flags |=  D3D11_ST_FLAG_OVERLAYS_ENABLE;
   else
      d3d11->flags &= ~D3D11_ST_FLAG_OVERLAYS_ENABLE;
   win32_show_cursor(d3d11, state);
}

static void d3d11_overlay_full_screen(void* data, bool enable)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   if (!d3d11)
      return;

   if (enable)
      d3d11->flags |=  D3D11_ST_FLAG_OVERLAYS_FULLSCREEN;
   else
      d3d11->flags &= ~D3D11_ST_FLAG_OVERLAYS_FULLSCREEN;
}

static void d3d11_get_overlay_interface(
      void* data, const video_overlay_interface_t** iface)
{
   static const video_overlay_interface_t overlay_interface = {
      d3d11_overlay_enable,      d3d11_overlay_load,        d3d11_overlay_tex_geom,
      d3d11_overlay_vertex_geom, d3d11_overlay_full_screen, d3d11_overlay_set_alpha,
   };

   *iface = &overlay_interface;
}

static void d3d11_render_overlay(d3d11_video_t *d3d11)
{
   int i;

   if (d3d11->flags & D3D11_ST_FLAG_OVERLAYS_FULLSCREEN)
      d3d11->context->lpVtbl->RSSetViewports(d3d11->context, 1, &d3d11->viewport);
   else
      d3d11->context->lpVtbl->RSSetViewports(d3d11->context, 1, &d3d11->frame.viewport);

   d3d11->context->lpVtbl->OMSetBlendState(d3d11->context, d3d11->blend_enable,
         NULL, D3D11_DEFAULT_SAMPLE_MASK);
   {
      UINT stride = sizeof(d3d11_sprite_t);
      UINT offset = 0;
      d3d11->context->lpVtbl->IASetVertexBuffers(
            d3d11->context, 0, 1, &d3d11->overlays.vbo, &stride, &offset);
   }
   d3d11->context->lpVtbl->PSSetSamplers(
         d3d11->context, 0, 1,
         &d3d11->samplers[RARCH_FILTER_UNSPEC][RARCH_WRAP_DEFAULT]);

   for (i = 0; i < d3d11->overlays.count; i++)
   {
      d3d11->context->lpVtbl->PSSetShaderResources(
            d3d11->context, 0, 1,
            &d3d11->overlays.textures[i].view);
      d3d11->context->lpVtbl->Draw(d3d11->context, 1, i);
   }
}
#endif

#ifdef HAVE_DXGI_HDR
static void d3d11_set_hdr_max_nits(void *data, float max_nits)
{
   D3D11_MAPPED_SUBRESOURCE mapped_ubo;
   d3d11_video_t* d3d11                   = (d3d11_video_t*)data;

   d3d11->hdr.max_output_nits             = max_nits;
   d3d11->hdr.ubo_values.max_nits         = max_nits;

   d3d11->context->lpVtbl->Map(
         d3d11->context, (D3D11Resource)d3d11->hdr.ubo, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped_ubo);
   {
      dxgi_hdr_uniform_t *ubo = (dxgi_hdr_uniform_t*)mapped_ubo.pData;
      *ubo                    = d3d11->hdr.ubo_values;
   }
   d3d11->context->lpVtbl->Unmap(d3d11->context, (D3D11Resource)d3d11->hdr.ubo, 0);

   dxgi_set_hdr_metadata(
         d3d11->swapChain,
         (d3d11->flags & D3D11_ST_FLAG_HDR_SUPPORT) ? true : false,
         d3d11->chain_bit_depth,
         d3d11->chain_color_space,
         d3d11->hdr.max_output_nits,
         d3d11->hdr.min_output_nits,
         d3d11->hdr.max_cll,
         d3d11->hdr.max_fall);
}

static void d3d11_set_hdr_paper_white_nits(void* data, float paper_white_nits)
{
   D3D11_MAPPED_SUBRESOURCE mapped_ubo;
   dxgi_hdr_uniform_t *ubo                = NULL;
   d3d11_video_t      *d3d11              = (d3d11_video_t*)data;

   d3d11->hdr.ubo_values.paper_white_nits = paper_white_nits;

   d3d11->context->lpVtbl->Map(
         d3d11->context, (D3D11Resource)d3d11->hdr.ubo, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_ubo);
   ubo  = (dxgi_hdr_uniform_t*)mapped_ubo.pData;
   *ubo = d3d11->hdr.ubo_values;
   d3d11->context->lpVtbl->Unmap(d3d11->context, (D3D11Resource)d3d11->hdr.ubo, 0);
}

static void d3d11_set_hdr_contrast(void* data, float contrast)
{
   D3D11_MAPPED_SUBRESOURCE mapped_ubo;
   dxgi_hdr_uniform_t *ubo                = NULL;
   d3d11_video_t* d3d11                   = (d3d11_video_t*)data;

   d3d11->hdr.ubo_values.contrast         = contrast;

   d3d11->context->lpVtbl->Map(
         d3d11->context, (D3D11Resource)d3d11->hdr.ubo, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_ubo);
   ubo  = (dxgi_hdr_uniform_t*)mapped_ubo.pData;
   *ubo = d3d11->hdr.ubo_values;
   d3d11->context->lpVtbl->Unmap(d3d11->context, (D3D11Resource)d3d11->hdr.ubo, 0);
}

static void d3d11_set_hdr_expand_gamut(void* data, bool expand_gamut)
{
   D3D11_MAPPED_SUBRESOURCE mapped_ubo;
   dxgi_hdr_uniform_t *ubo                = NULL;
   d3d11_video_t* d3d11                   = (d3d11_video_t*)data;

   d3d11->hdr.ubo_values.expand_gamut     = expand_gamut ? 1.0f : 0.0f;

   d3d11->context->lpVtbl->Map(
         d3d11->context, (D3D11Resource)d3d11->hdr.ubo, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_ubo);
   ubo  = (dxgi_hdr_uniform_t*)mapped_ubo.pData;
   *ubo = d3d11->hdr.ubo_values;
   d3d11->context->lpVtbl->Unmap(d3d11->context, (D3D11Resource)d3d11->hdr.ubo, 0);
}

static void d3d11_set_hdr_inverse_tonemap(d3d11_video_t* d3d11, bool inverse_tonemap)
{
   D3D11_MAPPED_SUBRESOURCE mapped_ubo;
   dxgi_hdr_uniform_t *ubo                = NULL;

   d3d11->hdr.ubo_values.inverse_tonemap  = inverse_tonemap ? 1.0f : 0.0f;

   d3d11->context->lpVtbl->Map(
         d3d11->context, (D3D11Resource)d3d11->hdr.ubo, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_ubo);
   ubo  = (dxgi_hdr_uniform_t*)mapped_ubo.pData;
   *ubo = d3d11->hdr.ubo_values;
   d3d11->context->lpVtbl->Unmap(d3d11->context, (D3D11Resource)d3d11->hdr.ubo, 0);
}

static void d3d11_set_hdr10(d3d11_video_t* d3d11, bool hdr10)
{
   D3D11_MAPPED_SUBRESOURCE mapped_ubo;
   dxgi_hdr_uniform_t *ubo                = NULL;

   d3d11->hdr.ubo_values.hdr10  = hdr10 ? 1.0f : 0.0f;

   d3d11->context->lpVtbl->Map(
         d3d11->context, (D3D11Resource)d3d11->hdr.ubo, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_ubo);
   ubo  = (dxgi_hdr_uniform_t*)mapped_ubo.pData;
   *ubo = d3d11->hdr.ubo_values;
   d3d11->context->lpVtbl->Unmap(d3d11->context, (D3D11Resource)d3d11->hdr.ubo, 0);
}
#endif

static void d3d11_set_filtering(void* data, unsigned index,
      bool smooth, bool ctx_scaling)
{
   int i;
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   if (smooth)
   {
      for (i = 0; i < RARCH_WRAP_MAX; i++)
         d3d11->samplers[RARCH_FILTER_UNSPEC][i] = d3d11->samplers[RARCH_FILTER_LINEAR][i];
   }
   else
   {
      for (i = 0; i < RARCH_WRAP_MAX; i++)
         d3d11->samplers[RARCH_FILTER_UNSPEC][i] = d3d11->samplers[RARCH_FILTER_NEAREST][i];
   }
}

static void d3d11_gfx_set_rotation(void* data, unsigned rotation)
{
   float radians, cosine, sine;
   D3D11_MAPPED_SUBRESOURCE mapped_ubo;
   static math_matrix_4x4 rot     = {
      { 0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    1.0f }
   };
   d3d11_video_t*  d3d11   = (d3d11_video_t*)data;

   if (!d3d11)
      return;

   radians                 = rotation * (M_PI / 2.0f);
   cosine                  = cosf(radians);
   sine                    = sinf(radians);
   MAT_ELEM_4X4(rot, 0, 0) = cosine;
   MAT_ELEM_4X4(rot, 0, 1) = -sine;
   MAT_ELEM_4X4(rot, 1, 0) = sine;
   MAT_ELEM_4X4(rot, 1, 1) = cosine;
   matrix_4x4_multiply(d3d11->mvp, rot, d3d11->ubo_values.mvp);

   d3d11->context->lpVtbl->Map(
         d3d11->context, (D3D11Resource)d3d11->frame.ubo, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_ubo);
   *(math_matrix_4x4*)mapped_ubo.pData = d3d11->mvp;
   d3d11->context->lpVtbl->Unmap(d3d11->context, (D3D11Resource)d3d11->frame.ubo, 0);
}

static void d3d11_update_viewport(d3d11_video_t *d3d11, bool force_full)
{
   video_driver_update_viewport(&d3d11->vp, force_full,
         (d3d11->flags & D3D11_ST_FLAG_KEEP_ASPECT) ? true : false);

   d3d11->frame.viewport.TopLeftX = d3d11->vp.x;
   d3d11->frame.viewport.TopLeftY = d3d11->vp.y;
   d3d11->frame.viewport.Width    = d3d11->vp.width;
   d3d11->frame.viewport.Height   = d3d11->vp.height;
   d3d11->frame.viewport.MaxDepth = 0.0f;
   d3d11->frame.viewport.MaxDepth = 1.0f;

   if (d3d11->shader_preset
         && (  d3d11->frame.output_size.x != d3d11->vp.width
            || d3d11->frame.output_size.y != d3d11->vp.height))
      d3d11->flags           |= D3D11_ST_FLAG_RESIZE_RTS;

   d3d11->frame.output_size.x = d3d11->vp.width;
   d3d11->frame.output_size.y = d3d11->vp.height;
   d3d11->frame.output_size.z = 1.0f / d3d11->vp.width;
   d3d11->frame.output_size.w = 1.0f / d3d11->vp.height;

   d3d11->flags              &= ~D3D11_ST_FLAG_RESIZE_VIEWPORT;
}

static void d3d11_free_shader_preset(d3d11_video_t* d3d11)
{
   int i;

   if (!d3d11->shader_preset)
      return;

   for (i = 0; i < (int)d3d11->shader_preset->passes; i++)
   {
      int j;

      free(d3d11->shader_preset->pass[i].source.string.vertex);
      free(d3d11->shader_preset->pass[i].source.string.fragment);
      free(d3d11->pass[i].semantics.textures);
      d3d11->shader_preset->pass[i].source.string.vertex   = NULL;
      d3d11->shader_preset->pass[i].source.string.fragment = NULL;
      d3d11->pass[i].semantics.textures                    = NULL;
      d3d11_release_shader(&d3d11->pass[i].shader);
      d3d11_release_texture(&d3d11->pass[i].rt);
      d3d11_release_texture(&d3d11->pass[i].feedback);

      for (j = 0; j < SLANG_CBUFFER_MAX; j++)
      {
         free(d3d11->pass[i].semantics.cbuffers[j].uniforms);
         d3d11->pass[i].semantics.cbuffers[j].uniforms = NULL;
         Release(d3d11->pass[i].buffers[j]);
      }
   }

   memset(d3d11->pass, 0, sizeof(d3d11->pass));

   /* only free the history textures here */
   for (i = 1; i <= d3d11->shader_preset->history_size; i++)
      d3d11_release_texture(&d3d11->frame.texture[i]);

   memset(
         &d3d11->frame.texture[1], 0,
         sizeof(d3d11->frame.texture[1]) * d3d11->shader_preset->history_size);

   for (i = 0; i < (int)d3d11->shader_preset->luts; i++)
      d3d11_release_texture(&d3d11->luts[i]);

   memset(d3d11->luts, 0, sizeof(d3d11->luts));

   free(d3d11->shader_preset);
   d3d11->shader_preset         = NULL;
   d3d11->flags                &= ~(D3D11_ST_FLAG_INIT_HISTORY
                                  | D3D11_ST_FLAG_RESIZE_RTS
                                   );
}

static bool d3d11_gfx_set_shader(void* data, enum rarch_shader_type type, const char* path)
{
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   enum d3d11_feature_level_hint
      feat_level_hint      = D3D11_FEATURE_LEVEL_HINT_DONTCARE;
   unsigned         i;
   d3d11_texture_t* source = NULL;
   d3d11_video_t*   d3d11  = (d3d11_video_t*)data;
   unsigned shader_model   = 40;

   if (!d3d11)
      return false;

   /* Feature Level 11.0 and up should all support Shader Model 5.0 */
   switch (d3d11->supportedFeatureLevel)
   {
      case D3D_FEATURE_LEVEL_11_0:
         shader_model    = 50;
         feat_level_hint = D3D11_FEATURE_LEVEL_HINT_11_0;
         break;
      case D3D_FEATURE_LEVEL_11_1:
         shader_model    = 50;
         feat_level_hint = D3D11_FEATURE_LEVEL_HINT_11_1;
         break;
      case D3D_FEATURE_LEVEL_12_0:
         shader_model    = 50;
         feat_level_hint = D3D11_FEATURE_LEVEL_HINT_12_0;
         break;
      case D3D_FEATURE_LEVEL_12_1:
         shader_model    = 50;
         feat_level_hint = D3D11_FEATURE_LEVEL_HINT_12_1;
         break;
      default:
         break;
   }

   d3d11->context->lpVtbl->Flush(d3d11->context);
   d3d11_free_shader_preset(d3d11);

   if (string_is_empty(path))
      return true;

   if (type != RARCH_SHADER_SLANG)
   {
      RARCH_WARN("[D3D11]: Only Slang shaders are supported. Falling back to stock.\n");
      return false;
   }

   d3d11->shader_preset = (struct video_shader*)calloc(1, sizeof(*d3d11->shader_preset));

   if (!video_shader_load_preset_into_shader(path, d3d11->shader_preset))
      goto error;

   source = &d3d11->frame.texture[0];
   for (i = 0; i < d3d11->shader_preset->passes; source = &d3d11->pass[i++].rt)
   {
      int j;
      /* clang-format off */
      semantics_map_t semantics_map = {
         {
            /* Original */
            { &d3d11->frame.texture[0].view, 0,
               &d3d11->frame.texture[0].size_data, 0},

            /* Source */
            { &source->view, 0,
               &source->size_data, 0},

            /* OriginalHistory */
            { &d3d11->frame.texture[0].view, sizeof(*d3d11->frame.texture),
               &d3d11->frame.texture[0].size_data, sizeof(*d3d11->frame.texture)},

            /* PassOutput */
            { &d3d11->pass[0].rt.view, sizeof(*d3d11->pass),
               &d3d11->pass[0].rt.size_data, sizeof(*d3d11->pass)},

            /* PassFeedback */
            { &d3d11->pass[0].feedback.view, sizeof(*d3d11->pass),
               &d3d11->pass[0].feedback.size_data, sizeof(*d3d11->pass)},

            /* User */
            { &d3d11->luts[0].view, sizeof(*d3d11->luts),
               &d3d11->luts[0].size_data, sizeof(*d3d11->luts)},
         },
         {
            i == d3d11->shader_preset->passes - 1 ? &d3d11->mvp : &d3d11->identity, /* MVP */
            &d3d11->pass[i].rt.size_data,    /* OutputSize */
            &d3d11->frame.output_size,       /* FinalViewportSize */
            &d3d11->pass[i].frame_count,     /* FrameCount */
            &d3d11->pass[i].frame_direction, /* FrameDirection */
            &d3d11->pass[i].rotation,        /* Rotation */
            &d3d11->pass[i].core_aspect,     /* OriginalAspect */
            &d3d11->pass[i].core_aspect_rot, /* OriginalAspectRotated */
            &d3d11->pass[i].total_subframes, /* TotalSubFrames */
            &d3d11->pass[i].current_subframe,/* CurrentSubFrame */
         }
      };
      /* clang-format on */

      if (!slang_process(
               d3d11->shader_preset, i, RARCH_SHADER_HLSL, shader_model,
               &semantics_map,
               &d3d11->pass[i].semantics))
         goto error;

      {
         static const D3D11_INPUT_ELEMENT_DESC desc[] = {
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d11_vertex_t, position),
               D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d11_vertex_t, texcoord),
               D3D11_INPUT_PER_VERTEX_DATA, 0 },
         };
         char _path[PATH_MAX_LENGTH];
         const char *slang_path = d3d11->shader_preset->pass[i].source.path;
         const char *vs_src     = d3d11->shader_preset->pass[i].source.string.vertex;
         const char *ps_src     = d3d11->shader_preset->pass[i].source.string.fragment;
         size_t _len            = strlcpy(_path, slang_path, sizeof(_path));
         strlcpy(_path + _len, ".vs.hlsl", sizeof(_path) - _len);

         if (!d3d11_init_shader(
                  d3d11->device, vs_src, 0, _path, "main", NULL, NULL, desc, countof(desc),
                  &d3d11->pass[i].shader,
                  feat_level_hint
                  )) { }

         strlcpy(_path + _len, ".ps.hlsl", sizeof(_path) - _len);

         if (!d3d11_init_shader(
                  d3d11->device, ps_src, 0, _path, NULL, "main", NULL, NULL, 0,
                  &d3d11->pass[i].shader,
                  feat_level_hint
                  )) { }

         free(d3d11->shader_preset->pass[i].source.string.vertex);
         free(d3d11->shader_preset->pass[i].source.string.fragment);

         d3d11->shader_preset->pass[i].source.string.vertex   = NULL;
         d3d11->shader_preset->pass[i].source.string.fragment = NULL;

         if (!d3d11->pass[i].shader.vs || !d3d11->pass[i].shader.ps)
            goto error;
      }

      for (j = 0; j < SLANG_CBUFFER_MAX; j++)
      {
         D3D11_BUFFER_DESC desc;
         desc.ByteWidth           = d3d11->pass[i].semantics.cbuffers[j].size;
         desc.Usage               = D3D11_USAGE_DYNAMIC;
         desc.BindFlags           = D3D11_BIND_CONSTANT_BUFFER;
         desc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
         desc.MiscFlags           = 0;
         desc.StructureByteStride = 0;

         if (!desc.ByteWidth)
            continue;

         d3d11->device->lpVtbl->CreateBuffer(d3d11->device, &desc, NULL, &d3d11->pass[i].buffers[j]);
      }
   }

#ifdef HAVE_DXGI_HDR
   if (d3d11->flags & D3D11_ST_FLAG_HDR_ENABLE)
   {
      if (d3d11->shader_preset && d3d11->shader_preset->passes && (d3d11->pass[d3d11->shader_preset->passes - 1].semantics.format == SLANG_FORMAT_A2B10G10R10_UNORM_PACK32))
      {
         /* If the last shader pass uses a RGB10A2 back buffer and hdr has been enabled assume we want to skip the inverse tonemapper and hdr10 conversion */
         d3d11_set_hdr_inverse_tonemap(d3d11, false);
         d3d11_set_hdr10(d3d11, false);
         d3d11->flags |= D3D11_ST_FLAG_RESIZE_CHAIN;
      }
      else if (d3d11->shader_preset && d3d11->shader_preset->passes && (d3d11->pass[d3d11->shader_preset->passes - 1].semantics.format == SLANG_FORMAT_R16G16B16A16_SFLOAT))
      {
         /* If the last shader pass uses a RGBA16 back buffer and hdr has been enabled assume we want to skip the inverse tonemapper */
         d3d11_set_hdr_inverse_tonemap(d3d11, false);
         d3d11_set_hdr10(d3d11, true);
         d3d11->flags |= D3D11_ST_FLAG_RESIZE_CHAIN;
      }
      else
      {
         d3d11_set_hdr_inverse_tonemap(d3d11, true);
         d3d11_set_hdr10(d3d11, true);
      }
   }
#endif /* HAVE_DXGI_HDR */

   for (i = 0; i < d3d11->shader_preset->luts; i++)
   {
      struct texture_image image;
      image.pixels               = NULL;
      image.width                = 0;
      image.height               = 0;
      image.supports_rgba        = true;

      if (!image_texture_load(&image, d3d11->shader_preset->lut[i].path))
         goto error;

      d3d11->luts[i].desc.Width  = image.width;
      d3d11->luts[i].desc.Height = image.height;
      d3d11->luts[i].desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

      if (d3d11->shader_preset->lut[i].mipmap)
         d3d11->luts[i].desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

      d3d11_release_texture(&d3d11->luts[i]);
      d3d11_init_texture(d3d11->device, &d3d11->luts[i]);

      if (d3d11->luts[i].staging)
         d3d11_update_texture(
               d3d11->context, image.width, image.height, 0, DXGI_FORMAT_R8G8B8A8_UNORM, image.pixels,
               &d3d11->luts[i]);

      image_texture_free(&image);
   }

   d3d11->flags |= D3D11_ST_FLAG_INIT_HISTORY | D3D11_ST_FLAG_RESIZE_RTS;

   return true;

error:
   d3d11_free_shader_preset(d3d11);
#endif
   return false;
}

static void d3d11_gfx_free(void* data)
{
   int i;
   uint32_t video_st_flags;
   d3d11_video_t* d3d11           = (d3d11_video_t*)data;
   video_driver_state_t *video_st = video_state_get_ptr();

   if (!d3d11)
      return;

   if (d3d11->flags & D3D11_ST_FLAG_WAITABLE_SWAPCHAINS)
      CloseHandle(d3d11->frameLatencyWaitableObject);

   font_driver_free_osd();

#ifdef HAVE_OVERLAY
   d3d11_free_overlays(d3d11);
#endif

   d3d11_free_shader_preset(d3d11);

   d3d11_release_texture(&d3d11->frame.texture[0]);
   Release(d3d11->frame.ubo);
   Release(d3d11->frame.vbo);

   d3d11_release_texture(&d3d11->menu.texture);
   Release(d3d11->menu.vbo);

#ifdef HAVE_DXGI_HDR
   Release(d3d11->hdr.ubo);
#endif

   d3d11_release_shader(&d3d11->sprites.shader);
   d3d11_release_shader(&d3d11->sprites.shader_font);
   Release(d3d11->sprites.vbo);

   for (i = 0; i < GFX_MAX_SHADERS; i++)
      d3d11_release_shader(&d3d11->shaders[i]);

   Release(d3d11->menu_pipeline_vbo);
   Release(d3d11->blend_pipeline);

   Release(d3d11->ubo);

   Release(d3d11->blend_enable);
   Release(d3d11->blend_disable);

   for (i = 0; i < RARCH_WRAP_MAX; i++)
   {
      Release(d3d11->samplers[RARCH_FILTER_LINEAR][i]);
      Release(d3d11->samplers[RARCH_FILTER_NEAREST][i]);
   }

#ifdef HAVE_DXGI_HDR
   d3d11_release_texture(&d3d11->back_buffer);
#endif

   Release(d3d11->scissor_enabled);
   Release(d3d11->scissor_disabled);
   Release(d3d11->swapChain);

   video_st_flags                  = video_st->flags;
   if (video_st_flags & VIDEO_FLAG_CACHE_CONTEXT)
   {
      cached_device_d3d11          = d3d11->device;
      cached_context_d3d11         = d3d11->context;
      cached_supportedFeatureLevel = d3d11->supportedFeatureLevel;
   }
   else
   {
      Release(d3d11->context);
      Release(d3d11->device);
   }

   for (i = 0; i < D3D11_MAX_GPU_COUNT; i++)
   {
      if (d3d11->adapters[i])
      {
         Release(d3d11->adapters[i]);
         d3d11->adapters[i] = NULL;
      }
   }

#ifdef HAVE_DXGI_HDR
   video_driver_unset_hdr_support();
#endif

#ifdef HAVE_MONITOR
   win32_monitor_from_window();
#endif
#ifdef HAVE_WINDOW
   win32_destroy_window();
#endif
   free(d3d11);
}

static bool d3d11_init_swapchain(d3d11_video_t* d3d11,
      int width, int height,
      D3D11Device        *cached_device,
      D3D11DeviceContext *cached_context,
      void *corewindow)
{
   HWND hwnd                               = (HWND)corewindow;
#ifdef __WINRT__
   IDXGIFactory2* dxgiFactory              = NULL;
#else
   IDXGIFactory*  dxgiFactory              = NULL;
   IDXGIFactory5* dxgiFactory5             = NULL;
#endif
   IDXGIDevice* dxgiDevice                 = NULL;
   IDXGIAdapter* adapter                   = NULL;
   UINT                 flags              = 0;
   D3D_FEATURE_LEVEL
      requested_feature_levels[]           =
      {
         D3D_FEATURE_LEVEL_11_0,
         D3D_FEATURE_LEVEL_10_1,
         D3D_FEATURE_LEVEL_10_0
      };
#ifdef __WINRT__
   /* UWP requires the use of newer version of the factory which requires newer version of this struct */
   DXGI_SWAP_CHAIN_DESC1 desc              = {{0}};
#else
   DXGI_SWAP_CHAIN_DESC desc               = {{0}};
#endif
   UINT number_feature_levels              = ARRAY_SIZE(requested_feature_levels);

#ifdef HAVE_DXGI_HDR
   DXGI_COLOR_SPACE_TYPE color_space;

   d3d11->chain_formats[DXGI_SWAPCHAIN_BIT_DEPTH_8]    = DXGI_FORMAT_R8G8B8A8_UNORM;
   d3d11->chain_formats[DXGI_SWAPCHAIN_BIT_DEPTH_10]   = DXGI_FORMAT_R10G10B10A2_UNORM;
   d3d11->chain_formats[DXGI_SWAPCHAIN_BIT_DEPTH_16]   = DXGI_FORMAT_R16G16B16A16_UNORM;
#endif

#ifdef HAVE_DXGI_HDR
   if (dxgi_check_display_hdr_support(d3d11->factory, hwnd))
      d3d11->flags                        |=  D3D11_ST_FLAG_HDR_SUPPORT;
   else
      d3d11->flags                        &= ~D3D11_ST_FLAG_HDR_SUPPORT;

   if (!(d3d11->flags & D3D11_ST_FLAG_HDR_SUPPORT))
      d3d11->flags                        &= ~D3D11_ST_FLAG_HDR_ENABLE;

   d3d11->chain_bit_depth                  =
      (d3d11->flags & D3D11_ST_FLAG_HDR_ENABLE)
      ? DXGI_SWAPCHAIN_BIT_DEPTH_10
      : DXGI_SWAPCHAIN_BIT_DEPTH_8;
#endif

#ifdef __WINRT__
   /* Flip model forces us to do double-buffering */
   desc.BufferCount                        = 2;

   desc.Width                              = width;
   desc.Height                             = height;
#ifdef HAVE_DXGI_HDR
   if (d3d11->flags & D3D11_ST_FLAG_HDR_SUPPORT)
      desc.Format                          = d3d11->chain_formats[
         d3d11->chain_bit_depth];
   else
#endif
      desc.Format                          = DXGI_FORMAT_R8G8B8A8_UNORM;
#else
   desc.BufferCount                        = 2;

   desc.BufferDesc.Width                   = width;
   desc.BufferDesc.Height                  = height;
#ifdef HAVE_DXGI_HDR
   if (d3d11->flags & D3D11_ST_FLAG_HDR_SUPPORT)
      desc.BufferDesc.Format               = d3d11->chain_formats[
         d3d11->chain_bit_depth];
   else
#endif
      desc.BufferDesc.Format               = DXGI_FORMAT_R8G8B8A8_UNORM;
   desc.BufferDesc.RefreshRate.Numerator   = 60;
   desc.BufferDesc.RefreshRate.Denominator = 1;
#endif
   desc.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
#ifdef HAVE_WINDOW
   desc.OutputWindow                       = hwnd;
#endif
   desc.SampleDesc.Count                   = 1;
   desc.SampleDesc.Quality                 = 0;
#ifdef HAVE_WINDOW
   desc.Windowed                           = TRUE;
#endif

#ifdef DEBUG
   flags                                  |= D3D11_CREATE_DEVICE_DEBUG;
#endif

   if (*cached_device && *cached_context)
   {
      d3d11->device                = *cached_device;
      d3d11->context               = *cached_context;
      d3d11->supportedFeatureLevel = cached_supportedFeatureLevel;
      *cached_device               = NULL;
      *cached_context              = NULL;
   }
   else
   {
      if (FAILED(D3D11CreateDevice(
                  (IDXGIAdapter*)d3d11->adapter,
                  D3D_DRIVER_TYPE_HARDWARE, NULL, flags,
                  requested_feature_levels, number_feature_levels,
                  D3D11_SDK_VERSION, &d3d11->device,
                  &d3d11->supportedFeatureLevel, &d3d11->context)))
         return false;
      switch (d3d11->supportedFeatureLevel)
      {
         case D3D_FEATURE_LEVEL_9_1:
            RARCH_LOG("[D3D11]: Device created (Feature Level: 9.1)\n");
            break;
         case D3D_FEATURE_LEVEL_9_2:
            RARCH_LOG("[D3D11]: Device created (Feature Level: 9.2)\n");
            break;
         case D3D_FEATURE_LEVEL_9_3:
            RARCH_LOG("[D3D11]: Device created (Feature Level: 9.3)\n");
            break;
         case D3D_FEATURE_LEVEL_10_0:
            RARCH_LOG("[D3D11]: Device created (Feature Level: 10.0)\n");
            break;
         case D3D_FEATURE_LEVEL_10_1:
            RARCH_LOG("[D3D11]: Device created (Feature Level: 10.1)\n");
            break;
         case D3D_FEATURE_LEVEL_11_0:
            RARCH_LOG("[D3D11]: Device created (Feature Level: 11.0)\n");
            break;
         case D3D_FEATURE_LEVEL_11_1:
            RARCH_LOG("[D3D11]: Device created (Feature Level: 11.1)\n");
            break;
         case D3D_FEATURE_LEVEL_12_0:
            RARCH_LOG("[D3D11]: Device created (Feature Level: 12.0)\n");
            break;
         case D3D_FEATURE_LEVEL_12_1:
            RARCH_LOG("[D3D11]: Device created (Feature Level: 12.1)\n");
            break;
         default:
            RARCH_LOG("[D3D11]: Device created (Feature Level: N/A)\n");
            break;
      }
   }

   d3d11->device->lpVtbl->QueryInterface(
         d3d11->device, uuidof(IDXGIDevice), (void**)&dxgiDevice);
   dxgiDevice->lpVtbl->GetAdapter(dxgiDevice, &adapter);

#ifdef __WINRT__
#if defined(WINAPI_FAMILY) && (WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
   /* On phone, no swap effects are supported. */
   /* TODO/FIXME - need to verify if this is needed and if
    * flip model cannot be used here */
   desc.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;
#else
   d3d11->flags |= D3D11_ST_FLAG_HAS_FLIP_MODEL
      | D3D11_ST_FLAG_HAS_ALLOW_TEARING;
   desc.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
   if (d3d11->flags & D3D11_ST_FLAG_WAITABLE_SWAPCHAINS)
      desc.Flags                          |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
   desc.SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
#endif

   adapter->lpVtbl->GetParent(
         adapter, uuidof(IDXGIFactory2), (void**)&dxgiFactory);
   if (FAILED(dxgiFactory->lpVtbl->CreateSwapChainForCoreWindow(
               dxgiFactory, (IUnknown*)d3d11->device, corewindow,
               &desc, NULL, (IDXGISwapChain1**)&d3d11->swapChain)))
      return false;
#else
   if (d3d11->flags & D3D11_ST_FLAG_WAITABLE_SWAPCHAINS)
      desc.Flags                          |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
   desc.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

   adapter->lpVtbl->GetParent(
         adapter, uuidof(IDXGIFactory1), (void**)&dxgiFactory);

   /* Check for ALLOW_TEARING support before trying to use it.
    * Also don't use the flip model if it's not supported, because then we can't uncap our
    * present rate. */
#ifdef __cplusplus
   if (SUCCEEDED(dxgiFactory->lpVtbl->QueryInterface(dxgiFactory,
      libretro_IID_IDXGIFactory5, (void**)&dxgiFactory5)))
#else
   if (SUCCEEDED(dxgiFactory->lpVtbl->QueryInterface(
      dxgiFactory, &libretro_IID_IDXGIFactory5, (void**)&dxgiFactory5)))
#endif
   {
      BOOL allow_tearing_supported = FALSE;
      if (SUCCEEDED(dxgiFactory5->lpVtbl->CheckFeatureSupport(
         dxgiFactory5, DXGI_FEATURE_PRESENT_ALLOW_TEARING,
         &allow_tearing_supported, sizeof(allow_tearing_supported))) &&
         allow_tearing_supported)
      {
         desc.SwapEffect            = DXGI_SWAP_EFFECT_FLIP_DISCARD;
         desc.Flags                |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
         d3d11->flags              |= D3D11_ST_FLAG_HAS_FLIP_MODEL
                                    | D3D11_ST_FLAG_HAS_ALLOW_TEARING;

         RARCH_LOG("[D3D11]: Flip model and tear control supported and enabled.\n");
      }

      dxgiFactory5->lpVtbl->Release(dxgiFactory5);
   }

   if (FAILED(dxgiFactory->lpVtbl->CreateSwapChain(
               dxgiFactory, (IUnknown*)d3d11->device,
               &desc, (IDXGISwapChain**)&d3d11->swapChain)))
   {
      RARCH_WARN("[D3D11]: Failed to create swapchain with flip model, try non-flip model.\n");

      /* Failed to create swapchain, try non-flip model */
      desc.SwapEffect           =  DXGI_SWAP_EFFECT_DISCARD;
      desc.Flags               &= ~DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
      d3d11->flags             &= ~(D3D11_ST_FLAG_HAS_FLIP_MODEL
                                  | D3D11_ST_FLAG_HAS_ALLOW_TEARING
                                   );

      if (FAILED(dxgiFactory->lpVtbl->CreateSwapChain(
                  dxgiFactory, (IUnknown*)d3d11->device,
                  &desc, (IDXGISwapChain**)&d3d11->swapChain)))
         return false;
   }

#ifdef HAVE_WINDOW
   /* Don't let DXGI mess with the full screen state,
    * because otherwise we end up with a mismatch
    * between the window size and the buffers.
    * RetroArch only uses windowed mode (see above). */
   if (FAILED(dxgiFactory->lpVtbl->MakeWindowAssociation(dxgiFactory, desc.OutputWindow, DXGI_MWA_NO_ALT_ENTER)))
   {
      RARCH_ERR("[D3D11]: Failed to make disable DXGI ALT+ENTER handling.\n");
   }
#endif

#endif    /* __WINRT__ */

   if    (   (d3d11->flags & D3D11_ST_FLAG_WAITABLE_SWAPCHAINS)
          && (d3d11->frameLatencyWaitableObject = DXGIGetFrameLatencyWaitableObject(d3d11->swapChain)))
   {
      settings_t* settings      =  config_get_ptr();
      UINT max_latency          =  settings->uints.video_max_frame_latency;
      UINT cur_latency          =  0;

      if (max_latency == 0)
      {
         max_latency            =  1;
         d3d11->flags          |=  D3D11_ST_FLAG_WAIT_FOR_VBLANK;
      }
      else
         d3d11->flags          &= ~D3D11_ST_FLAG_WAIT_FOR_VBLANK;

      DXGISetMaximumFrameLatency(d3d11->swapChain, max_latency);
      DXGIGetMaximumFrameLatency(d3d11->swapChain, &cur_latency);
      RARCH_LOG("[D3D11]: Requesting %u maximum frame latency, using %u%s.\n",
            settings->uints.video_max_frame_latency,
            cur_latency,
            (d3d11->flags & D3D11_ST_FLAG_WAIT_FOR_VBLANK) ? " with WaitForVBlank" : "");
   }

#ifdef HAVE_DXGI_HDR
  /* Check display HDR support and
     initialize ST.2084 support to match
     the display's support. */
   color_space                 =
        (d3d11->flags & D3D11_ST_FLAG_HDR_ENABLE)
      ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
      : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

   dxgi_swapchain_color_space(
         d3d11->swapChain,
         &d3d11->chain_color_space,
         color_space);
   dxgi_set_hdr_metadata(
         d3d11->swapChain,
         (d3d11->flags & D3D11_ST_FLAG_HDR_SUPPORT) ? true : false,
         d3d11->chain_bit_depth,
         d3d11->chain_color_space,
         d3d11->hdr.max_output_nits,
         d3d11->hdr.min_output_nits,
         d3d11->hdr.max_cll,
         d3d11->hdr.max_fall);

   memset(&d3d11->back_buffer, 0, sizeof(d3d11->back_buffer));
   d3d11->back_buffer.desc.Width              = width;
   d3d11->back_buffer.desc.Height             = height;
   d3d11->back_buffer.desc.Format             = d3d11->shader_preset && d3d11->shader_preset->passes ? glslang_format_to_dxgi(d3d11->pass[d3d11->shader_preset->passes - 1].semantics.format) : DXGI_FORMAT_R8G8B8A8_UNORM;
   d3d11->back_buffer.desc.BindFlags          = D3D11_BIND_RENDER_TARGET;
   d3d11_release_texture(&d3d11->back_buffer);
   d3d11_init_texture(d3d11->device, &d3d11->back_buffer);
#endif

   dxgiFactory->lpVtbl->Release(dxgiFactory);
   adapter->lpVtbl->Release(adapter);
   dxgiDevice->lpVtbl->Release(dxgiDevice);

   return true;
}

static void *d3d11_gfx_init(const video_info_t* video,
      input_driver_t** input, void** input_data)
{
   unsigned       i;
#ifdef HAVE_MONITOR
   MONITORINFOEX  current_mon;
   HMONITOR       hm_to_use;
   WNDCLASSEX     wndclass = { 0 };
#endif
   settings_t*    settings = config_get_ptr();
   d3d11_video_t* d3d11    = (d3d11_video_t*)calloc(1, sizeof(*d3d11));

   if (!d3d11)
      return NULL;

#ifdef HAVE_WINDOW
   win32_window_reset();
#endif
#ifdef HAVE_MONITOR
   win32_monitor_init();
   wndclass.lpfnWndProc = wnd_proc_d3d_common;
#ifdef HAVE_DINPUT
   if (string_is_equal(settings->arrays.input_driver, "dinput"))
      wndclass.lpfnWndProc = wnd_proc_d3d_dinput;
#endif
#ifdef HAVE_WINRAWINPUT
   if (string_is_equal(settings->arrays.input_driver, "raw"))
      wndclass.lpfnWndProc = wnd_proc_d3d_winraw;
#endif
#ifdef HAVE_WINDOW
   win32_window_init(&wndclass, true, NULL);
#endif

   win32_monitor_info(&current_mon, &hm_to_use, &d3d11->cur_mon_id);
#endif

   d3d11->vp.full_width  = video->width;
   d3d11->vp.full_height = video->height;

#ifdef HAVE_MONITOR
   if (!d3d11->vp.full_width)
      d3d11->vp.full_width = current_mon.rcMonitor.right - current_mon.rcMonitor.left;
   if (!d3d11->vp.full_height)
      d3d11->vp.full_height = current_mon.rcMonitor.bottom - current_mon.rcMonitor.top;
#endif

   if (!win32_set_video_mode(d3d11, d3d11->vp.full_width, d3d11->vp.full_height, video->fullscreen))
   {
      RARCH_ERR("[D3D11]: win32_set_video_mode failed.\n");
      goto error;
   }

   d3d_input_driver(settings->arrays.input_driver, settings->arrays.input_joypad_driver, input, input_data);

#ifdef __WINRT__
   DXGICreateFactory2(&d3d11->factory);
#else
   DXGICreateFactory1(&d3d11->factory);
#endif
#ifdef HAVE_DXGI_HDR
   d3d11->hdr.max_output_nits             = settings->floats.video_hdr_max_nits;
   d3d11->hdr.min_output_nits             = 0.001f;
   d3d11->hdr.max_cll                     = 0.0f;
   d3d11->hdr.max_fall                    = 0.0f;
   if (settings->bools.video_hdr_enable)
      d3d11->flags |=  D3D11_ST_FLAG_HDR_ENABLE;
   else
      d3d11->flags &= ~D3D11_ST_FLAG_HDR_ENABLE;
#endif

   if (settings->bools.video_waitable_swapchains)
      d3d11->flags |=  D3D11_ST_FLAG_WAITABLE_SWAPCHAINS;

#ifdef __WINRT__
   if (!d3d11_init_swapchain(d3d11,
            d3d11->vp.full_width,
            d3d11->vp.full_height,
            &cached_device_d3d11,
            &cached_context_d3d11,
            uwp_get_corewindow()
            ))
      goto error;
#else
   if (!d3d11_init_swapchain(d3d11,
            d3d11->vp.full_width,
            d3d11->vp.full_height,
            &cached_device_d3d11,
            &cached_context_d3d11,
            main_window.hwnd
            ))
      goto error;
#endif

   matrix_4x4_identity(d3d11->identity);

   video_driver_set_size(d3d11->vp.full_width, d3d11->vp.full_height);
   d3d11->viewport.Width  = d3d11->vp.full_width;
   d3d11->viewport.Height = d3d11->vp.full_height;
   d3d11->scissor.right   = d3d11->vp.full_width;
   d3d11->scissor.bottom  = d3d11->vp.full_height;

   d3d11->flags          |=  D3D11_ST_FLAG_RESIZE_VIEWPORT;

   if (video->force_aspect)
      d3d11->flags       |=  D3D11_ST_FLAG_KEEP_ASPECT;
   else
      d3d11->flags       &= ~D3D11_ST_FLAG_KEEP_ASPECT;

   if (video->vsync)
      d3d11->flags       |=  D3D11_ST_FLAG_VSYNC;
   else
      d3d11->flags       &= ~D3D11_ST_FLAG_VSYNC;

   d3d11->format          = (video->rgb32)
         ? DXGI_FORMAT_B8G8R8X8_UNORM : DXGI_FORMAT_B5G6R5_UNORM;

   d3d11->frame.texture[0].desc.Format = d3d11->format;
   d3d11->frame.texture[0].desc.Usage  = D3D11_USAGE_DEFAULT;
   d3d11->frame.texture[0].desc.Width  = 4;
   d3d11->frame.texture[0].desc.Height = 4;

   d3d11_release_texture(&d3d11->frame.texture[0]);
   d3d11_init_texture(d3d11->device, &d3d11->frame.texture[0]);

   d3d11->menu.texture.desc.Usage = D3D11_USAGE_DEFAULT;

   matrix_4x4_ortho(d3d11->ubo_values.mvp, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);

   d3d11->ubo_values.OutputSize.width  = d3d11->viewport.Width;
   d3d11->ubo_values.OutputSize.height = d3d11->viewport.Height;

   {
      D3D11_SUBRESOURCE_DATA ubo_data;
      D3D11_BUFFER_DESC      desc;
      desc.ByteWidth           = sizeof(d3d11->ubo_values);
      desc.Usage               = D3D11_USAGE_DYNAMIC;
      desc.BindFlags           = D3D11_BIND_CONSTANT_BUFFER;
      desc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
      desc.MiscFlags           = 0;
      desc.StructureByteStride = 0;

      ubo_data.pSysMem          = &d3d11->ubo_values.mvp;
      ubo_data.SysMemPitch      = 0;
      ubo_data.SysMemSlicePitch = 0;

      d3d11->device->lpVtbl->CreateBuffer(d3d11->device, &desc, &ubo_data,
            &d3d11->ubo);
      d3d11->device->lpVtbl->CreateBuffer(d3d11->device, &desc, NULL,
            &d3d11->frame.ubo);
   }

   d3d11_gfx_set_rotation(d3d11, 0);

#ifdef HAVE_DXGI_HDR
   if (d3d11->flags & D3D11_ST_FLAG_HDR_ENABLE)
   {
      D3D11_BUFFER_DESC      desc;
      D3D11_SUBRESOURCE_DATA ubo_data;
      matrix_4x4_ortho(d3d11->mvp_no_rot, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);

      d3d11->hdr.ubo_values.mvp              =
         d3d11->mvp_no_rot;
      d3d11->hdr.ubo_values.max_nits         =
         settings->floats.video_hdr_max_nits;
      d3d11->hdr.ubo_values.paper_white_nits =
         settings->floats.video_hdr_paper_white_nits;
      d3d11->hdr.ubo_values.contrast         =
         VIDEO_HDR_MAX_CONTRAST - settings->floats.video_hdr_display_contrast;
      d3d11->hdr.ubo_values.expand_gamut    =
         settings->bools.video_hdr_expand_gamut;
      d3d11->hdr.ubo_values.inverse_tonemap = 1.0f;  /* Use this to turn on/off the inverse tonemap */
      d3d11->hdr.ubo_values.hdr10           = 1.0f;  /* Use this to turn on/off the hdr10 */

      desc.ByteWidth                       = sizeof(dxgi_hdr_uniform_t);
      desc.Usage                           = D3D11_USAGE_DYNAMIC;
      desc.BindFlags                       = D3D11_BIND_CONSTANT_BUFFER;
      desc.CPUAccessFlags                  = D3D11_CPU_ACCESS_WRITE;
      desc.MiscFlags                       = 0;
      desc.StructureByteStride             = 0;

      ubo_data.pSysMem                     = &d3d11->hdr.ubo_values.mvp;
      ubo_data.SysMemPitch                 = 0;
      ubo_data.SysMemSlicePitch            = 0;

      d3d11->device->lpVtbl->CreateBuffer(d3d11->device, &desc, &ubo_data,
            &d3d11->hdr.ubo);
   }
#endif

   {
      D3D11_SAMPLER_DESC desc = { D3D11_FILTER_MIN_MAG_MIP_POINT };
      desc.MaxAnisotropy      = 1;
      desc.ComparisonFunc     = D3D11_COMPARISON_NEVER;
      desc.MinLOD             = -D3D11_FLOAT32_MAX;
      desc.MaxLOD             = D3D11_FLOAT32_MAX;

      /* Initialize samplers */
      for (i = 0; i < RARCH_WRAP_MAX; i++)
      {
         switch (i)
         {
            case RARCH_WRAP_BORDER:
               desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
               break;

            case RARCH_WRAP_EDGE:
               desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
               break;

            case RARCH_WRAP_REPEAT:
               desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
               break;

            case RARCH_WRAP_MIRRORED_REPEAT:
               desc.AddressU = D3D11_TEXTURE_ADDRESS_MIRROR;
               break;
         }

         desc.AddressV = desc.AddressU;
         desc.AddressW = desc.AddressU;
         desc.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;

         d3d11->device->lpVtbl->CreateSamplerState(d3d11->device, &desc,
               &d3d11->samplers[RARCH_FILTER_LINEAR][i]);

         desc.Filter   = D3D11_FILTER_MIN_MAG_MIP_POINT;

         d3d11->device->lpVtbl->CreateSamplerState(d3d11->device, &desc,
               &d3d11->samplers[RARCH_FILTER_NEAREST][i]);
      }
   }

   d3d11_set_filtering(d3d11, 0, video->smooth, video->ctx_scaling);

   {
      D3D11_BUFFER_DESC desc;
      d3d11_vertex_t vertices[] = {
         { { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
         { { 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
         { { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
         { { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

         { { -1.0f, -1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
         { { -1.0f,  1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
         { { 1.0f,  -1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
         { { 1.0f,   1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
      };
      D3D11_SUBRESOURCE_DATA vertex_data;

      vertex_data.pSysMem          = vertices;
      vertex_data.SysMemPitch      = 0;
      vertex_data.SysMemSlicePitch = 0;

      desc.ByteWidth               = sizeof(vertices);
      desc.Usage                   = D3D11_USAGE_IMMUTABLE;
      desc.BindFlags               = D3D11_BIND_VERTEX_BUFFER;
      desc.CPUAccessFlags          = 0;
      desc.MiscFlags               = 0;
      desc.StructureByteStride     = 0;

      d3d11->device->lpVtbl->CreateBuffer(d3d11->device, &desc, &vertex_data,
            &d3d11->frame.vbo);
      desc.Usage                   = D3D11_USAGE_DYNAMIC;
      desc.CPUAccessFlags          = D3D11_CPU_ACCESS_WRITE;
      d3d11->device->lpVtbl->CreateBuffer(d3d11->device, &desc, &vertex_data,
            &d3d11->menu.vbo);

      d3d11->sprites.capacity  = 16 * 1024;
      desc.ByteWidth           = sizeof(d3d11_sprite_t) * d3d11->sprites.capacity;
      d3d11->device->lpVtbl->CreateBuffer(d3d11->device, &desc, NULL,
            &d3d11->sprites.vbo);
   }

#ifdef HAVE_DXGI_HDR
   if (d3d11->flags & D3D11_ST_FLAG_HDR_ENABLE)
   {
      D3D11_INPUT_ELEMENT_DESC desc[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
            offsetof(d3d11_vertex_t, position),
            D3D11_INPUT_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
            offsetof(d3d11_vertex_t, texcoord),
            D3D11_INPUT_PER_VERTEX_DATA, 0 },
         { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
            offsetof(d3d11_vertex_t, color),
            D3D11_INPUT_PER_VERTEX_DATA, 0 },
      };

      static const char shader[] =
#include "d3d_shaders/hdr_sm5.hlsl.h"
         ;

      if (!d3d11_init_shader(
               d3d11->device, shader, sizeof(shader),
               NULL, "VSMain", "PSMain", NULL, desc,
               countof(desc), &d3d11->shaders[VIDEO_SHADER_STOCK_HDR],
               D3D11_FEATURE_LEVEL_HINT_DONTCARE
               ))
         goto error;
   }
#endif

   {
      D3D11_INPUT_ELEMENT_DESC desc[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d11_vertex_t, position),
            D3D11_INPUT_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d11_vertex_t, texcoord),
            D3D11_INPUT_PER_VERTEX_DATA, 0 },
         { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d11_vertex_t, color),
            D3D11_INPUT_PER_VERTEX_DATA, 0 },
      };

      static const char shader[] =
#include "d3d_shaders/opaque_sm5.hlsl.h"
         ;

      if (!d3d11_init_shader(
               d3d11->device, shader, sizeof(shader), NULL, "VSMain", "PSMain", NULL, desc,
               countof(desc), &d3d11->shaders[VIDEO_SHADER_STOCK_BLEND],
               D3D11_FEATURE_LEVEL_HINT_DONTCARE
               ))
         goto error;
   }

   {
      D3D11_INPUT_ELEMENT_DESC desc[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d11_sprite_t, pos),
            D3D11_INPUT_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d11_sprite_t, coords),
            D3D11_INPUT_PER_VERTEX_DATA, 0 },
         { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d11_sprite_t, colors[0]),
            D3D11_INPUT_PER_VERTEX_DATA, 0 },
         { "COLOR", 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d11_sprite_t, colors[1]),
            D3D11_INPUT_PER_VERTEX_DATA, 0 },
         { "COLOR", 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d11_sprite_t, colors[2]),
            D3D11_INPUT_PER_VERTEX_DATA, 0 },
         { "COLOR", 3, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d11_sprite_t, colors[3]),
            D3D11_INPUT_PER_VERTEX_DATA, 0 },
         { "PARAMS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d11_sprite_t, params),
            D3D11_INPUT_PER_VERTEX_DATA, 0 },
      };

      static const char shader[] =
#include "d3d_shaders/sprite_sm4.hlsl.h"
         ;

      if (!d3d11_init_shader(
               d3d11->device, shader,
               sizeof(shader), NULL, "VSMain", "PSMain", "GSMain", desc,
               countof(desc), &d3d11->sprites.shader,
               D3D11_FEATURE_LEVEL_HINT_DONTCARE))
         goto error;
      if (!d3d11_init_shader(
               d3d11->device, shader,
               sizeof(shader), NULL, "VSMain", "PSMainA8", "GSMain", desc,
               countof(desc), &d3d11->sprites.shader_font,
               D3D11_FEATURE_LEVEL_HINT_DONTCARE))
         goto error;
   }

   if (string_is_equal(settings->arrays.menu_driver, "xmb"))
   {
      {
         D3D11_INPUT_ELEMENT_DESC desc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
         };

         static const char ribbon[] =
#include "d3d_shaders/ribbon_sm4.hlsl.h"
            ;
         static const char ribbon_simple[] =
#include "d3d_shaders/ribbon_simple_sm4.hlsl.h"
            ;

         if (!d3d11_init_shader(
                  d3d11->device, ribbon,
                  sizeof(ribbon), NULL, "VSMain", "PSMain", NULL, desc,
                  countof(desc), &d3d11->shaders[VIDEO_SHADER_MENU],
                  D3D11_FEATURE_LEVEL_HINT_DONTCARE))
            goto error;

         if (!d3d11_init_shader(
                  d3d11->device, ribbon_simple,
                  sizeof(ribbon_simple), NULL, "VSMain", "PSMain", NULL,
                  desc, countof(desc), &d3d11->shaders[VIDEO_SHADER_MENU_2],
                  D3D11_FEATURE_LEVEL_HINT_DONTCARE))
            goto error;
      }

      {
         D3D11_INPUT_ELEMENT_DESC desc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,
               0, offsetof(d3d11_vertex_t, position),
               D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,
               0, offsetof(d3d11_vertex_t, texcoord),
               D3D11_INPUT_PER_VERTEX_DATA, 0 },
         };

         static const char simple_snow[] =
#include "d3d_shaders/simple_snow_sm4.hlsl.h"
            ;
         static const char snow[] =
#include "d3d_shaders/snow_sm4.hlsl.h"
            ;
         static const char bokeh[] =
#include "d3d_shaders/bokeh_sm4.hlsl.h"
            ;
         static const char snowflake[] =
#include "d3d_shaders/snowflake_sm4.hlsl.h"
            ;

         if (!d3d11_init_shader(
                  d3d11->device, simple_snow,
                  sizeof(simple_snow), NULL, "VSMain", "PSMain", NULL,
                  desc, countof(desc), &d3d11->shaders[VIDEO_SHADER_MENU_3],
                  D3D11_FEATURE_LEVEL_HINT_DONTCARE))
            goto error;
         if (!d3d11_init_shader(
                  d3d11->device, snow,
                  sizeof(snow), NULL, "VSMain", "PSMain", NULL, desc,
                  countof(desc), &d3d11->shaders[VIDEO_SHADER_MENU_4],
                  D3D11_FEATURE_LEVEL_HINT_DONTCARE))
            goto error;

         if (!d3d11_init_shader(
                  d3d11->device, bokeh,
                  sizeof(bokeh), NULL, "VSMain", "PSMain", NULL, desc,
                  countof(desc), &d3d11->shaders[VIDEO_SHADER_MENU_5],
                  D3D11_FEATURE_LEVEL_HINT_DONTCARE))
            goto error;

         if (!d3d11_init_shader(
                  d3d11->device, snowflake,
                  sizeof(snowflake), NULL, "VSMain", "PSMain", NULL, desc,
                  countof(desc), &d3d11->shaders[VIDEO_SHADER_MENU_6],
                  D3D11_FEATURE_LEVEL_HINT_DONTCARE))
            goto error;
      }
   }

   {
      D3D11_BLEND_DESC blend_desc = { 0 };

      blend_desc.AlphaToCoverageEnable                 = FALSE;
      blend_desc.IndependentBlendEnable                = FALSE;
      blend_desc.RenderTarget[0].BlendEnable           = TRUE;
      blend_desc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
      blend_desc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
      blend_desc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
      blend_desc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_SRC_ALPHA;
      blend_desc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
      blend_desc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
      blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
      d3d11->device->lpVtbl->CreateBlendState(d3d11->device, &blend_desc,
            &d3d11->blend_enable);

      blend_desc.RenderTarget[0].SrcBlend              = D3D11_BLEND_DEST_COLOR;
      blend_desc.RenderTarget[0].DestBlend             = D3D11_BLEND_ONE;
      d3d11->device->lpVtbl->CreateBlendState(d3d11->device, &blend_desc,
            &d3d11->blend_pipeline);

      blend_desc.RenderTarget[0].BlendEnable           = FALSE;
      d3d11->device->lpVtbl->CreateBlendState(d3d11->device, &blend_desc,
            &d3d11->blend_disable);
   }
   {
      D3D11_RASTERIZER_DESC desc;

      desc.FillMode              = D3D11_FILL_SOLID;
      desc.CullMode              = D3D11_CULL_NONE;
      desc.FrontCounterClockwise = false;
      desc.DepthBias             = 0;
      desc.DepthBiasClamp        = 0.0f;
      desc.SlopeScaledDepthBias  = 0.0f;
      desc.DepthClipEnable       = FALSE;
      desc.MultisampleEnable     = FALSE;
      desc.AntialiasedLineEnable = FALSE;

      desc.ScissorEnable         = TRUE;
      d3d11->device->lpVtbl->CreateRasterizerState(d3d11->device, &desc,
            &d3d11->scissor_enabled);
      desc.ScissorEnable         = FALSE;
      d3d11->device->lpVtbl->CreateRasterizerState(d3d11->device, &desc,
            &d3d11->scissor_disabled);
   }

   font_driver_init_osd(d3d11,
         video,
         false,
         video->is_threaded,
         FONT_DRIVER_RENDER_D3D11_API);

   {
      d3d11_fake_context.get_flags   = d3d11_get_flags;
      d3d11_fake_context.get_metrics = win32_get_metrics;
      video_context_driver_set(&d3d11_fake_context);
      const char *shader_preset      = video_shader_get_current_shader_preset();
      enum rarch_shader_type type    = video_shader_parse_type(shader_preset);
      d3d11_gfx_set_shader(d3d11, type, shader_preset);
   }

   if (video_driver_get_hw_context()->context_type  == RETRO_HW_CONTEXT_D3D11)
   {
      d3d11->flags                     |= D3D11_ST_FLAG_HW_IFACE_ENABLE;
      d3d11->hw_iface.interface_type    = RETRO_HW_RENDER_INTERFACE_D3D11;
      d3d11->hw_iface.interface_version = RETRO_HW_RENDER_INTERFACE_D3D11_VERSION;
      d3d11->hw_iface.handle            = d3d11;
      d3d11->hw_iface.device            = d3d11->device;
      d3d11->hw_iface.context           = d3d11->context;
      d3d11->hw_iface.featureLevel      = d3d11->supportedFeatureLevel;
      d3d11->hw_iface.D3DCompile        = D3DCompile;
   }

   {
      int         i = 0;
      int gpu_index = settings->ints.d3d11_gpu_index;

      if (d3d11->gpu_list)
         string_list_free(d3d11->gpu_list);

      d3d11->gpu_list = string_list_new();

      for (;;)
      {
         char str[128];
         union string_list_elem_attr attr;
         DXGI_ADAPTER_DESC desc = {0};

         attr.i = 0;
         str[0] = '\0';

#ifdef __WINRT__
         if (FAILED(DXGIEnumAdapters2(d3d11->factory, i, &d3d11->adapter)))
            break;
#else
         if (FAILED(DXGIEnumAdapters1(d3d11->factory, i, &d3d11->adapter)))
            break;
#endif

         IDXGIAdapter_GetDesc(d3d11->adapter, &desc);

         utf16_to_char_string((const uint16_t*)
               desc.Description, str, sizeof(str));

         RARCH_LOG("[D3D11]: Found GPU at index %d: \"%s\".\n", i, str);

         string_list_append(d3d11->gpu_list, str, attr);

         if (i < D3D11_MAX_GPU_COUNT)
            d3d11->adapters[i] = d3d11->adapter;

         i++;
      }

      video_driver_set_gpu_api_devices(GFX_CTX_DIRECT3D11_API, d3d11->gpu_list);

      if (0 <= gpu_index && gpu_index <= i && gpu_index < D3D11_MAX_GPU_COUNT)
      {
         d3d11->current_adapter = d3d11->adapters[gpu_index];
         d3d11->adapter         = d3d11->current_adapter;
         RARCH_LOG("[D3D11]: Using GPU index %d.\n", gpu_index);
      }
      else
      {
         RARCH_WARN("[D3D11]: Invalid GPU index %d, using first device found.\n", gpu_index);
         d3d11->current_adapter = d3d11->adapters[0];
         d3d11->adapter         = d3d11->current_adapter;
      }
   }

   return d3d11;

error:
   d3d11_gfx_free(d3d11);

#ifdef HAVE_OPENGL
   video_driver_force_fallback("gl");
#elif !defined(__WINRT__)
#ifdef HAVE_OPENGL1
   video_driver_force_fallback("gl1");
#else
   video_driver_force_fallback("gdi");
#endif
#endif

   return NULL;
}

static void d3d11_init_history(d3d11_video_t* d3d11, unsigned width, unsigned height)
{
   int i;

   /* TODO/FIXME: Should we init history to max_width/max_height instead ?
    * to prevent out of memory errors happening several frames later
    * and to reduce memory fragmentation */
   for (i = 0; i < d3d11->shader_preset->history_size + 1; i++)
   {
      d3d11->frame.texture[i].desc.Width  = width;
      d3d11->frame.texture[i].desc.Height = height;
      d3d11->frame.texture[i].desc.Format = d3d11->frame.texture[0].desc.Format;
      d3d11->frame.texture[i].desc.Usage  = d3d11->frame.texture[0].desc.Usage;
      d3d11_release_texture(&d3d11->frame.texture[i]);
      d3d11_init_texture(d3d11->device, &d3d11->frame.texture[i]);
      /* TODO/FIXME: clear texture ?  */
   }
   d3d11->flags &= ~D3D11_ST_FLAG_INIT_HISTORY;
}

static void d3d11_init_render_targets(d3d11_video_t* d3d11, unsigned width, unsigned height)
{
   size_t i;

   for (i = 0; i < d3d11->shader_preset->passes; i++)
   {
      struct video_shader_pass* pass = &d3d11->shader_preset->pass[i];

      if (pass->fbo.flags & FBO_SCALE_FLAG_VALID)
      {
         switch (pass->fbo.type_x)
         {
            case RARCH_SCALE_INPUT:
               width *= pass->fbo.scale_x;
               break;

            case RARCH_SCALE_VIEWPORT:
               width = d3d11->vp.width * pass->fbo.scale_x;
               break;

            case RARCH_SCALE_ABSOLUTE:
               width = pass->fbo.abs_x;
               break;

            default:
               break;
         }

         if (!width)
            width = d3d11->vp.width;

         switch (pass->fbo.type_y)
         {
            case RARCH_SCALE_INPUT:
               height *= pass->fbo.scale_y;
               break;

            case RARCH_SCALE_VIEWPORT:
               height = d3d11->vp.height * pass->fbo.scale_y;
               break;

            case RARCH_SCALE_ABSOLUTE:
               height = pass->fbo.abs_y;
               break;

            default:
               break;
         }

         if (!height)
            height = d3d11->vp.height;
      }
      else if (i == (d3d11->shader_preset->passes - 1))
      {
         width  = d3d11->vp.width;
         height = d3d11->vp.height;
      }

      RARCH_LOG("[D3D11]: Updating framebuffer size %ux%u.\n", width, height);

      if (     (i != (d3d11->shader_preset->passes - 1))
            || (width  != d3d11->vp.width)
            || (height != d3d11->vp.height))
      {
         d3d11->pass[i].viewport.Width    = width;
         d3d11->pass[i].viewport.Height   = height;
         d3d11->pass[i].viewport.MaxDepth = 1.0;
         d3d11->pass[i].rt.desc.Width     = width;
         d3d11->pass[i].rt.desc.Height    = height;
         d3d11->pass[i].rt.desc.BindFlags = D3D11_BIND_RENDER_TARGET;
         d3d11->pass[i].rt.desc.Format    = glslang_format_to_dxgi(d3d11->pass[i].semantics.format);
         d3d11_release_texture(&d3d11->pass[i].rt);
         d3d11_init_texture(d3d11->device, &d3d11->pass[i].rt);

         if (pass->feedback)
         {
            d3d11->pass[i].feedback.desc = d3d11->pass[i].rt.desc;
            d3d11_release_texture(&d3d11->pass[i].feedback);
            d3d11_init_texture(d3d11->device, &d3d11->pass[i].feedback);
            /* TODO/FIXME: do we need to clear it to black here ? */
         }
      }
      else
      {
         width = retroarch_get_rotation() % 2 ? height : width;
         height = retroarch_get_rotation() % 2 ? width : height;

         d3d11->pass[i].rt.size_data.x = width;
         d3d11->pass[i].rt.size_data.y = height;
         d3d11->pass[i].rt.size_data.z = 1.0f / width;
         d3d11->pass[i].rt.size_data.w = 1.0f / height;
      }
   }

   d3d11->flags &= ~D3D11_ST_FLAG_RESIZE_RTS;
}

static bool d3d11_gfx_frame(
      void*               data,
      const void*         frame,
      unsigned            width,
      unsigned            height,
      uint64_t            frame_count,
      unsigned            pitch,
      const char*         msg,
      video_frame_info_t* video_info)
{
   unsigned i, k, m;
   d3d11_texture_t* texture       = NULL;
   D3D11RenderTargetView rtv      = NULL;
   d3d11_video_t* d3d11           = (d3d11_video_t*)data;
   D3D11DeviceContext context     = d3d11->context;
   bool vsync                     = (d3d11->flags & D3D11_ST_FLAG_VSYNC) ? true : false;
   bool wait_for_vblank           = (d3d11->flags & D3D11_ST_FLAG_WAIT_FOR_VBLANK) ? true : false;
   unsigned present_flags         = (vsync || !(d3d11->flags & D3D11_ST_FLAG_HAS_ALLOW_TEARING))
         ? 0 : DXGI_PRESENT_ALLOW_TEARING;
   const char *stat_text          = video_info->stat_text;
   unsigned video_width           = video_info->width;
   unsigned video_height          = video_info->height;
   bool statistics_show           = video_info->statistics_show;
   struct font_params* osd_params = (struct font_params*)&video_info->osd_stat_params;
   bool menu_is_alive             = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
   bool overlay_behind_menu       = video_info->overlay_behind_menu;
   unsigned black_frame_insertion = video_info->black_frame_insertion;
   int bfi_light_frames;
   unsigned n;
   bool nonblock_state            = video_info->input_driver_nonblock_state;
   bool runloop_is_slowmotion     = video_info->runloop_is_slowmotion;
   bool runloop_is_paused         = video_info->runloop_is_paused;
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active            = video_info->widgets_active;
#endif
#ifdef HAVE_DXGI_HDR
   bool d3d11_hdr_enable          = (d3d11->flags & D3D11_ST_FLAG_HDR_ENABLE) ? true : false;
   bool video_hdr_enable          = video_info->hdr_enable;
   DXGI_FORMAT back_buffer_format = d3d11->shader_preset && d3d11->shader_preset->passes ? glslang_format_to_dxgi(d3d11->pass[d3d11->shader_preset->passes - 1].semantics.format) : DXGI_FORMAT_R8G8B8A8_UNORM;
   bool use_back_buffer           = back_buffer_format != d3d11->chain_formats[d3d11->chain_bit_depth];
#endif

   if (d3d11->flags & D3D11_ST_FLAG_WAITABLE_SWAPCHAINS)
      WaitForSingleObjectEx(
            d3d11->frameLatencyWaitableObject,
            1000,
            true);

#ifdef HAVE_DXGI_HDR
   if (     (d3d11->flags & D3D11_ST_FLAG_RESIZE_CHAIN)
         || (d3d11_hdr_enable != video_hdr_enable))
#else
   if (d3d11->flags & D3D11_ST_FLAG_RESIZE_CHAIN)
#endif
   {
      UINT swapchain_flags        = (d3d11->flags & D3D11_ST_FLAG_HAS_ALLOW_TEARING)
            ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

      if (d3d11->flags & D3D11_ST_FLAG_WAITABLE_SWAPCHAINS)
         swapchain_flags         |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

#ifdef HAVE_DXGI_HDR
      if (video_hdr_enable)
         d3d11->flags |=  D3D11_ST_FLAG_HDR_ENABLE;
      else
         d3d11->flags &= ~D3D11_ST_FLAG_HDR_ENABLE;

      if (d3d11->flags & D3D11_ST_FLAG_HDR_ENABLE)
         d3d11_release_texture(&d3d11->back_buffer);

      DXGIResizeBuffers(d3d11->swapChain, 0, 0, 0,
            d3d11->chain_formats[d3d11->chain_bit_depth],
            swapchain_flags);
#else
      DXGIResizeBuffers(d3d11->swapChain, 0, 0, 0,
            DXGI_FORMAT_UNKNOWN,
            swapchain_flags);
#endif

      d3d11->viewport.Width               = video_width;
      d3d11->viewport.Height              = video_height;
      d3d11->scissor.right                = video_width;
      d3d11->scissor.bottom               = video_height;

      d3d11->ubo_values.OutputSize.width  = d3d11->viewport.Width;
      d3d11->ubo_values.OutputSize.height = d3d11->viewport.Height;

      d3d11->flags                       &= ~D3D11_ST_FLAG_RESIZE_CHAIN;
      d3d11->flags                       |=  D3D11_ST_FLAG_RESIZE_VIEWPORT;

      video_driver_set_size(video_width, video_height);

#ifdef HAVE_DXGI_HDR
#ifdef __WINRT__
      if (dxgi_check_display_hdr_support(d3d11->factory, uwp_get_corewindow()))
         d3d11->flags |=  D3D11_ST_FLAG_HDR_SUPPORT;
      else
         d3d11->flags &= ~D3D11_ST_FLAG_HDR_SUPPORT;
#else
      if (dxgi_check_display_hdr_support(d3d11->factory, main_window.hwnd))
         d3d11->flags |=  D3D11_ST_FLAG_HDR_SUPPORT;
      else
         d3d11->flags &= ~D3D11_ST_FLAG_HDR_SUPPORT;
#endif
      if (!(d3d11->flags & D3D11_ST_FLAG_HDR_SUPPORT))
         d3d11->flags &= ~D3D11_ST_FLAG_HDR_ENABLE;

      if (d3d11->flags & D3D11_ST_FLAG_HDR_ENABLE)
      {
         memset(&d3d11->back_buffer, 0, sizeof(d3d11->back_buffer));
         d3d11->back_buffer.desc.Width              = video_width;
         d3d11->back_buffer.desc.Height             = video_height;
         d3d11->back_buffer.desc.Format             = back_buffer_format;
         d3d11->back_buffer.desc.BindFlags          = D3D11_BIND_RENDER_TARGET;
         d3d11_release_texture(&d3d11->back_buffer);
         d3d11_init_texture(d3d11->device, &d3d11->back_buffer);

         dxgi_swapchain_color_space(
               d3d11->swapChain,
               &d3d11->chain_color_space,
               DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);

         d3d11->chain_bit_depth  = DXGI_SWAPCHAIN_BIT_DEPTH_10;
      }
      else
      {
         dxgi_swapchain_color_space(
               d3d11->swapChain,
               &d3d11->chain_color_space,
               DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);

         d3d11->chain_bit_depth  = DXGI_SWAPCHAIN_BIT_DEPTH_8;
      }

      dxgi_set_hdr_metadata(
            d3d11->swapChain,
            (d3d11->flags & D3D11_ST_FLAG_HDR_SUPPORT) ? true : false,
            d3d11->chain_bit_depth,
            d3d11->chain_color_space,
            d3d11->hdr.max_output_nits,
            d3d11->hdr.min_output_nits,
            d3d11->hdr.max_cll,
            d3d11->hdr.max_fall);
#endif
   }

   {
       D3D11Texture2D back_buffer;
       d3d11->swapChain->lpVtbl->GetBuffer(d3d11->swapChain, 0,
             uuidof(ID3D11Texture2D), (void**)&back_buffer);
       d3d11->device->lpVtbl->CreateRenderTargetView(d3d11->device,
             (D3D11Resource)back_buffer, NULL, &rtv);
       Release(back_buffer);
   }

#if 0
   /* Custom viewport doesn't call apply_state_changes, so we can't rely on this for now */
   if (d3d11->resize_viewport)
#endif
      d3d11_update_viewport(d3d11, false);

   if (frame && width && height)
   {
      D3D11Texture2D hw_texture = NULL;
      if (frame == RETRO_HW_FRAME_BUFFER_VALID)
      {
          D3D11_SHADER_RESOURCE_VIEW_DESC hw_desc;
          D3D11ShaderResourceView hw_view = NULL;
          context->lpVtbl->PSGetShaderResources(context, 0, 1, &hw_view);
          hw_view->lpVtbl->GetDesc(hw_view, &hw_desc);
          hw_view->lpVtbl->GetResource(hw_view, (D3D11Resource*)&hw_texture);

          if (d3d11->frame.texture[0].desc.Format != hw_desc.Format)
          {
              d3d11->frame.texture[0].desc.Width  = width;
              d3d11->frame.texture[0].desc.Height = height;
              d3d11->frame.texture[0].desc.Format = hw_desc.Format;
              d3d11_release_texture(&d3d11->frame.texture[0]);
              d3d11_init_texture(d3d11->device, &d3d11->frame.texture[0]);

              d3d11->flags       |= D3D11_ST_FLAG_INIT_HISTORY;
          }

          Release(hw_view);
      }

      if (d3d11->shader_preset)
      {
         if (     (d3d11->frame.texture[0].desc.Width  != width)
               || (d3d11->frame.texture[0].desc.Height != height))
            d3d11->flags |= D3D11_ST_FLAG_RESIZE_RTS;

         if (d3d11->flags & D3D11_ST_FLAG_RESIZE_RTS)
         {
            /* Release all Render Targets (RT) first to avoid memory fragmentation */
            for (i = 0; i < d3d11->shader_preset->passes; i++)
            {
               d3d11_release_texture(&d3d11->pass[i].rt);
               d3d11_release_texture(&d3d11->pass[i].feedback);
               memset(&d3d11->pass[i].rt, 0, sizeof(d3d11->pass[i].rt));
               memset(&d3d11->pass[i].feedback, 0, sizeof(d3d11->pass[i].feedback));
            }
         }

         if (d3d11->shader_preset->history_size)
         {
            if (d3d11->flags & D3D11_ST_FLAG_INIT_HISTORY)
               d3d11_init_history(d3d11, width, height);
            else
            {
               int k;
               d3d11_texture_t tmp = d3d11->frame.texture[d3d11->shader_preset->history_size];
               for (k = d3d11->shader_preset->history_size; k > 0; k--)
                  d3d11->frame.texture[k] = d3d11->frame.texture[k - 1];
               d3d11->frame.texture[0] = tmp;
            }
         }
      }

      /* Either no history, or we moved a texture of a different size in the front slot */
      if (     d3d11->frame.texture[0].desc.Width  != width
            || d3d11->frame.texture[0].desc.Height != height)
      {
         d3d11->frame.texture[0].desc.Width  = width;
         d3d11->frame.texture[0].desc.Height = height;
         d3d11_release_texture(&d3d11->frame.texture[0]);
         d3d11_init_texture(d3d11->device, &d3d11->frame.texture[0]);
      }

      if (d3d11->flags & D3D11_ST_FLAG_RESIZE_RTS)
         d3d11_init_render_targets(d3d11, width, height);

      if (hw_texture)
      {
          D3D11_BOX frame_box;
          frame_box.left   = 0;
          frame_box.top    = 0;
          frame_box.front  = 0;
          frame_box.right  = width;
          frame_box.bottom = height;
          frame_box.back   = 1;
          context->lpVtbl->CopySubresourceRegion(
                context,
                (D3D11Resource)d3d11->frame.texture[0].handle,
                0, 0, 0, 0,
                (D3D11Resource)hw_texture, 0, &frame_box);
          Release(hw_texture);
          hw_texture = NULL;
      }
      else if (d3d11->frame.texture[0].staging)
         d3d11_update_texture(
               context, width, height, pitch, d3d11->format, frame, &d3d11->frame.texture[0]);
   }

#ifdef D3D11_ROLLING_SCANLINE_SIMULATION
   if (      (video_info->shader_subframes > 1)
         &&  (video_info->scan_subframes)
         &&  !black_frame_insertion
         &&  !nonblock_state
         &&  !runloop_is_slowmotion
         &&  !runloop_is_paused
         &&  (!(d3d11->flags & D3D11_ST_FLAG_MENU_ENABLE)))
      context->lpVtbl->RSSetState(context, d3d11->scissor_enabled);
   else
#endif /* D3D11_ROLLING_SCANLINE_SIMULATION  */
   {
      context->lpVtbl->RSSetState(context, d3d11->scissor_disabled);
   }

   d3d11->context->lpVtbl->OMSetBlendState(
         d3d11->context, d3d11->blend_disable,
         NULL, D3D11_DEFAULT_SAMPLE_MASK);
   context->lpVtbl->IASetPrimitiveTopology(context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
   {
      UINT stride = sizeof(d3d11_vertex_t);
      UINT offset = 0;
      d3d11->context->lpVtbl->IASetVertexBuffers(
            d3d11->context, 0, 1, &d3d11->frame.vbo, &stride, &offset);
   }

   texture = d3d11->frame.texture;

   if (d3d11->shader_preset)
   {
      for (i = 0; i < d3d11->shader_preset->passes; i++)
      {
         if (d3d11->shader_preset->pass[i].feedback)
         {
            d3d11_texture_t tmp     = d3d11->pass[i].feedback;
            d3d11->pass[i].feedback = d3d11->pass[i].rt;
            d3d11->pass[i].rt       = tmp;
         }
      }

      for (i = 0; i < d3d11->shader_preset->passes; i++)
      {
         unsigned j;

         {
            d3d11_shader_t *shader = &d3d11->pass[i].shader;
            context->lpVtbl->IASetInputLayout(context, shader->layout);
            context->lpVtbl->VSSetShader(context, shader->vs, NULL, 0);
            context->lpVtbl->PSSetShader(context, shader->ps, NULL, 0);
            context->lpVtbl->GSSetShader(context, shader->gs, NULL, 0);
         }

         if (d3d11->shader_preset->pass[i].frame_count_mod)
            d3d11->pass[i].frame_count =
               frame_count % d3d11->shader_preset->pass[i].frame_count_mod;
         else
            d3d11->pass[i].frame_count = frame_count;

#ifdef HAVE_REWIND
         d3d11->pass[i].frame_direction = state_manager_frame_is_reversed() ? -1 : 1;
#else
         d3d11->pass[i].frame_direction = 1;
#endif

         d3d11->pass[i].rotation = retroarch_get_rotation();

         d3d11->pass[i].core_aspect = video_driver_get_core_aspect();

         /* OriginalAspectRotated: return 1/aspect for 90 and 270 rotated content */
         d3d11->pass[i].core_aspect_rot = video_driver_get_core_aspect();
         uint32_t rot = retroarch_get_rotation();
         if (rot == 1 || rot == 3)
            d3d11->pass[i].core_aspect_rot = 1/d3d11->pass[i].core_aspect_rot;

         /* Sub-frame info for multiframe shaders (per real content frame).
            Should always be 1 for non-use of subframes */
         if (!(d3d11->flags & D3D11_ST_FLAG_FRAME_DUPE_LOCK))
         {
           if (     black_frame_insertion
                 || nonblock_state
                 || runloop_is_slowmotion
                 || runloop_is_paused
                 || (d3d11->flags & D3D11_ST_FLAG_MENU_ENABLE))
              d3d11->pass[i].total_subframes = 1;
           else
              d3d11->pass[i].total_subframes = video_info->shader_subframes;

           d3d11->pass[i].current_subframe = 1;
         }

         for (j = 0; j < SLANG_CBUFFER_MAX; j++)
         {
            D3D11Buffer    buffer     = d3d11->pass[i].buffers[j];
            cbuffer_sem_t* buffer_sem = &d3d11->pass[i].semantics.cbuffers[j];

            if (buffer_sem->stage_mask && buffer_sem->uniforms)
            {
               D3D11_MAPPED_SUBRESOURCE res;
               uniform_sem_t*           uniform = buffer_sem->uniforms;

               d3d11->context->lpVtbl->Map(
                     d3d11->context, (D3D11Resource)buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
               while (uniform->size)
               {
                  if (uniform->data)
                     memcpy((uint8_t*)res.pData + uniform->offset, uniform->data, uniform->size);
                  uniform++;
               }
               context->lpVtbl->Unmap(context, (D3D11Resource)buffer, 0);

               if (buffer_sem->stage_mask & SLANG_STAGE_VERTEX_MASK)
                  context->lpVtbl->VSSetConstantBuffers(context,
                        buffer_sem->binding, 1, &buffer);

               if (buffer_sem->stage_mask & SLANG_STAGE_FRAGMENT_MASK)
                  context->lpVtbl->PSSetConstantBuffers(
                        context, buffer_sem->binding, 1, &buffer);
            }
         }

         {
            D3D11RenderTargetView null_rt = NULL;
            context->lpVtbl->OMSetRenderTargets(context, 1, &null_rt, NULL);
         }

         {
            D3D11ShaderResourceView textures[SLANG_NUM_BINDINGS] = { NULL };
            D3D11SamplerState       samplers[SLANG_NUM_BINDINGS] = { NULL };

            texture_sem_t *texture_sem = d3d11->pass[i].semantics.textures;
            while (texture_sem->stage_mask)
            {
               int binding       = texture_sem->binding;
               textures[binding] = *(D3D11ShaderResourceView*)texture_sem->texture_data;
               samplers[binding] = d3d11->samplers[texture_sem->filter][texture_sem->wrap];
               texture_sem++;
            }

            context->lpVtbl->PSSetShaderResources(
                  context, 0, SLANG_NUM_BINDINGS,
                  textures);
            context->lpVtbl->PSSetSamplers(
                  context, 0, SLANG_NUM_BINDINGS, samplers);
         }

         if (!d3d11->pass[i].rt.handle)
         {
            texture = NULL;
            break;
         }

         context->lpVtbl->OMSetRenderTargets(context, 1,
               &d3d11->pass[i].rt.rt_view, NULL);
         context->lpVtbl->RSSetViewports(context, 1, &d3d11->pass[i].viewport);

#ifdef D3D11_ROLLING_SCANLINE_SIMULATION
         if (      (video_info->shader_subframes > 1)
               &&  (video_info->scan_subframes)
               &&  !black_frame_insertion
               &&  !nonblock_state
               &&  !runloop_is_slowmotion
               &&  !runloop_is_paused
               &&  (!(d3d11->flags & D3D11_ST_FLAG_MENU_ENABLE)))
         {
            D3D11_RECT scissor_rect;

            scissor_rect.left   = 0;
            scissor_rect.top    = (unsigned int)(((float)d3d11->pass[i].viewport.Height / (float)video_info->shader_subframes)
                                    * (float)video_info->current_subframe);
            scissor_rect.right  = d3d11->pass[i].viewport.Width ;
            scissor_rect.bottom = (unsigned int)(((float)d3d11->pass[i].viewport.Height / (float)video_info->shader_subframes)
                                    * (float)(video_info->current_subframe + 1));

            d3d11->context->lpVtbl->RSSetScissorRects(d3d11->context, 1,
                  &scissor_rect);
         }
         else
         {
            D3D11_RECT scissor_rect;

            scissor_rect.left   = 0;
            scissor_rect.top    = 0;
            scissor_rect.right  = d3d11->pass[i].viewport.Width;
            scissor_rect.bottom = d3d11->pass[i].viewport.Height;

            d3d11->context->lpVtbl->RSSetScissorRects(d3d11->context, 1,
                  &scissor_rect);
         }
#endif /* D3D11_ROLLING_SCANLINE_SIMULATION */

         if (i == d3d11->shader_preset->passes - 1)
            context->lpVtbl->Draw(context, 4, 0);
         else
            context->lpVtbl->Draw(context, 4, 4);

         texture = &d3d11->pass[i].rt;
      }
   }


#ifdef HAVE_DXGI_HDR
   if (    (d3d11->flags & D3D11_ST_FLAG_HDR_ENABLE)
         && use_back_buffer)
   {
      context->lpVtbl->OMSetRenderTargets(context, 1,
            &d3d11->back_buffer.rt_view, NULL);
      context->lpVtbl->ClearRenderTargetView(context,
             d3d11->back_buffer.rt_view, d3d11->clearcolor);
   }
   else
#endif
   {
      context->lpVtbl->OMSetRenderTargets(context, 1, &rtv, NULL);
      context->lpVtbl->ClearRenderTargetView(context, rtv, d3d11->clearcolor);
   }

   context->lpVtbl->RSSetViewports(context, 1, &d3d11->frame.viewport);

   if (texture)
   {
      {
         d3d11_shader_t *shader = &d3d11->shaders[VIDEO_SHADER_STOCK_BLEND];
         context->lpVtbl->IASetInputLayout(context, shader->layout);
         context->lpVtbl->VSSetShader(context, shader->vs, NULL, 0);
         context->lpVtbl->PSSetShader(context, shader->ps, NULL, 0);
         context->lpVtbl->GSSetShader(context, shader->gs, NULL, 0);
      }
      context->lpVtbl->PSSetShaderResources(
            context, 0, 1,
            &texture->view);
      context->lpVtbl->PSSetSamplers(
            context, 0, 1,
            &d3d11->samplers[RARCH_FILTER_UNSPEC][RARCH_WRAP_DEFAULT]);
      context->lpVtbl->VSSetConstantBuffers(context, 0, 1, &d3d11->frame.ubo);
   }

#ifdef D3D11_ROLLING_SCANLINE_SIMULATION
   if (      (video_info->shader_subframes > 1)
         &&  (video_info->scan_subframes)
         &&  !black_frame_insertion
         &&  !nonblock_state
         &&  !runloop_is_slowmotion
         &&  !runloop_is_paused
         &&  (!(d3d11->flags & D3D11_ST_FLAG_MENU_ENABLE)))
   {
      D3D11_RECT scissor_rect;

      scissor_rect.left   = 0;
      scissor_rect.top    = (unsigned int)(((float)video_height / (float)video_info->shader_subframes)
                              * (float)video_info->current_subframe);
      scissor_rect.right  = video_width ;
      scissor_rect.bottom = (unsigned int)(((float)video_height / (float)video_info->shader_subframes)
                              * (float)(video_info->current_subframe + 1));

      d3d11->context->lpVtbl->RSSetScissorRects(d3d11->context, 1,
            &scissor_rect);
   }
   else
      d3d11->context->lpVtbl->RSSetScissorRects(d3d11->context, 1, &d3d11->scissor);
#endif /* D3D11_ROLLING_SCANLINE_SIMULATION */

   context->lpVtbl->Draw(context, 4, 0);
   context->lpVtbl->RSSetState(context, d3d11->scissor_enabled);
   d3d11->context->lpVtbl->RSSetScissorRects(d3d11->context, 1, &d3d11->scissor);
   context->lpVtbl->OMSetBlendState(context, d3d11->blend_enable,
         NULL, D3D11_DEFAULT_SAMPLE_MASK);

#ifdef HAVE_MENU
   if (    (d3d11->flags & D3D11_ST_FLAG_MENU_ENABLE)
         && d3d11->menu.texture.handle)
   {
      if (d3d11->flags & D3D11_ST_FLAG_MENU_FULLSCREEN)
         context->lpVtbl->RSSetViewports(context, 1, &d3d11->viewport);

      {
         d3d11_shader_t *shader = &d3d11->shaders[VIDEO_SHADER_STOCK_BLEND];
         context->lpVtbl->IASetInputLayout(context, shader->layout);
         context->lpVtbl->VSSetShader(context, shader->vs, NULL, 0);
         context->lpVtbl->PSSetShader(context, shader->ps, NULL, 0);
         context->lpVtbl->GSSetShader(context, shader->gs, NULL, 0);
      }
      {
         UINT stride = sizeof(d3d11_vertex_t);
         UINT offset = 0;
         d3d11->context->lpVtbl->IASetVertexBuffers(
               d3d11->context, 0, 1, &d3d11->menu.vbo, &stride, &offset);
      }
      context->lpVtbl->VSSetConstantBuffers(context, 0, 1, &d3d11->ubo);
      {
         d3d11_texture_t *texture = (d3d11_texture_t*)&d3d11->menu.texture;
         d3d11->context->lpVtbl->PSSetShaderResources(
               d3d11->context, 0, 1,
               &texture->view);
         d3d11->context->lpVtbl->PSSetSamplers(
               d3d11->context, 0, 1,
               (D3D11SamplerState*)&texture->sampler);
      }
      context->lpVtbl->Draw(context, 4, 0);
   }
#endif

   context->lpVtbl->RSSetViewports(context, 1, &d3d11->viewport);
   {
      d3d11_shader_t *shader = &d3d11->sprites.shader;
      context->lpVtbl->IASetInputLayout(context, shader->layout);
      context->lpVtbl->VSSetShader(context, shader->vs, NULL, 0);
      context->lpVtbl->PSSetShader(context, shader->ps, NULL, 0);
      context->lpVtbl->GSSetShader(context, shader->gs, NULL, 0);
   }
   context->lpVtbl->IASetPrimitiveTopology(context,
         D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
   context->lpVtbl->VSSetConstantBuffers(context, 0, 1, &d3d11->ubo);
   context->lpVtbl->PSSetConstantBuffers(
         context, 0, 1, &d3d11->ubo);
   {
         UINT stride = sizeof(d3d11_sprite_t);
         UINT offset = 0;
         context->lpVtbl->IASetVertexBuffers(
               context, 0, 1, &d3d11->sprites.vbo, &stride, &offset);
   }
   d3d11->flags          |= D3D11_ST_FLAG_SPRITES_ENABLE;

#ifdef HAVE_OVERLAY
   if ((d3d11->flags & D3D11_ST_FLAG_OVERLAYS_ENABLE) && overlay_behind_menu)
      d3d11_render_overlay(d3d11);
#endif

#ifdef HAVE_MENU
#ifndef HAVE_GFX_WIDGETS
   if (d3d11->flags & D3D11_ST_FLAG_MENU_ENABLE)
#endif
   {
      context->lpVtbl->RSSetViewports(context, 1, &d3d11->viewport);
      {
         UINT stride = sizeof(d3d11_sprite_t);
         UINT offset = 0;
         context->lpVtbl->IASetVertexBuffers(
               context, 0, 1, &d3d11->sprites.vbo, &stride, &offset);
      }
   }
#endif

#ifdef HAVE_MENU
   if (d3d11->flags & D3D11_ST_FLAG_MENU_ENABLE)
      menu_driver_frame(menu_is_alive, video_info);
   else
#endif
      if (statistics_show)
   {
      if (osd_params)
      {
         context->lpVtbl->RSSetViewports(context, 1, &d3d11->viewport);
         d3d11->context->lpVtbl->OMSetBlendState(d3d11->context, d3d11->blend_enable,
               NULL, D3D11_DEFAULT_SAMPLE_MASK);
         {
            UINT stride = sizeof(d3d11_sprite_t);
            UINT offset = 0;
            context->lpVtbl->IASetVertexBuffers(
                  context, 0, 1, &d3d11->sprites.vbo, &stride, &offset);
         }
         font_driver_render_msg(d3d11,
               stat_text,
               (const struct font_params*)osd_params, NULL);
      }
   }

#ifdef HAVE_OVERLAY
   if ((d3d11->flags & D3D11_ST_FLAG_OVERLAYS_ENABLE) && !overlay_behind_menu)
      d3d11_render_overlay(d3d11);
#endif

#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
   {
      context->lpVtbl->RSSetViewports(context, 1, &d3d11->viewport);
      gfx_widgets_frame(video_info);
   }
#endif

   if (msg && *msg)
   {
      UINT stride = sizeof(d3d11_sprite_t);
      UINT offset = 0;
      context->lpVtbl->RSSetViewports(context, 1, &d3d11->viewport);
      d3d11->context->lpVtbl->OMSetBlendState(d3d11->context, d3d11->blend_enable,
         NULL, D3D11_DEFAULT_SAMPLE_MASK);
      context->lpVtbl->IASetVertexBuffers(
            context, 0, 1, &d3d11->sprites.vbo, &stride, &offset);
      font_driver_render_msg(d3d11, msg, NULL, NULL);
   }
   d3d11->flags &= ~D3D11_ST_FLAG_SPRITES_ENABLE;

#if defined(_WIN32) && !defined(__WINRT__)
   video_driver_update_title(NULL);
#endif

#ifdef HAVE_DXGI_HDR
   /* Copy over back buffer to swap chain render targets */
   if (    (d3d11->flags & D3D11_ST_FLAG_HDR_ENABLE)
         && use_back_buffer)
   {
      ID3D11ShaderResourceView* nullSRV[1] = {NULL};
      context->lpVtbl->OMSetRenderTargets(context, 1, &rtv, NULL);
      context->lpVtbl->ClearRenderTargetView(context, rtv, d3d11->clearcolor);
      context->lpVtbl->RSSetViewports(context, 1, &d3d11->viewport);
      context->lpVtbl->RSSetScissorRects(context, 1, &d3d11->scissor);

      {
         d3d11_shader_t *shader = &d3d11->shaders[VIDEO_SHADER_STOCK_HDR];
         context->lpVtbl->IASetInputLayout(context, shader->layout);
         context->lpVtbl->VSSetShader(context, shader->vs, NULL, 0);
         context->lpVtbl->PSSetShader(context, shader->ps, NULL, 0);
         context->lpVtbl->GSSetShader(context, shader->gs, NULL, 0);
      }
      context->lpVtbl->VSSetConstantBuffers(context, 0, 1, &d3d11->hdr.ubo);
      context->lpVtbl->PSSetShaderResources(
            context, 0, 1,
            &d3d11->back_buffer.view);
      context->lpVtbl->PSSetSamplers(
            context, 0, 1,
            &d3d11->samplers[RARCH_FILTER_UNSPEC][RARCH_WRAP_DEFAULT]);
      context->lpVtbl->PSSetConstantBuffers(
            context, 0, 1, &d3d11->hdr.ubo);
      {
         UINT stride = sizeof(d3d11_vertex_t);
         UINT offset = 0;
         context->lpVtbl->IASetVertexBuffers(
               context, 0, 1, &d3d11->frame.vbo, &stride, &offset);
      }

      context->lpVtbl->RSSetState(context, d3d11->scissor_disabled);
      d3d11->context->lpVtbl->OMSetBlendState(d3d11->context, d3d11->blend_disable,
         NULL, D3D11_DEFAULT_SAMPLE_MASK);
      context->lpVtbl->IASetPrimitiveTopology(context,
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

      context->lpVtbl->Draw(context, 4, 0);

      context->lpVtbl->PSSetShaderResources(
            context, 0, 1, nullSRV);
      context->lpVtbl->RSSetState(context, d3d11->scissor_enabled);
      d3d11->context->lpVtbl->OMSetBlendState(d3d11->context, d3d11->blend_enable,
         NULL, D3D11_DEFAULT_SAMPLE_MASK);
      context->lpVtbl->IASetPrimitiveTopology(context,
            D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
   }
#endif

   DXGIPresent(d3d11->swapChain, d3d11->swap_interval, present_flags);

   if (vsync && wait_for_vblank)
   {
      IDXGIOutput *pOutput;
      DXGIGetContainingOutput(d3d11->swapChain, &pOutput);
      DXGIWaitForVBlank(pOutput);
      Release(pOutput);
   }

   if (
           black_frame_insertion
        && !(d3d11->flags & D3D11_ST_FLAG_MENU_ENABLE)
        && !nonblock_state
        && !runloop_is_slowmotion
        && !runloop_is_paused
        && (!(video_info->shader_subframes > 1)))
   {
      if (video_info->bfi_dark_frames > video_info->black_frame_insertion)
         video_info->bfi_dark_frames = video_info->black_frame_insertion;

      /* BFI now handles variable strobe strength, like on-off-off, vs on-on-off for 180hz.
         This needs to be done with duping frames instead of increased swap intervals for
         a couple reasons. Swap interval caps out at 4 in most all apis as of coding,
         and seems to be flat ignored >1 at least in modern Windows for some older APIs. */
      bfi_light_frames = video_info->black_frame_insertion - video_info->bfi_dark_frames;
      if (bfi_light_frames > 0 && !(d3d11->flags & D3D11_ST_FLAG_FRAME_DUPE_LOCK))
      {
         d3d11->flags |= D3D11_ST_FLAG_FRAME_DUPE_LOCK;
         while (bfi_light_frames > 0)
         {
            if (!(d3d11_gfx_frame(d3d11, NULL, 0, 0, frame_count, 0, msg, video_info)))
            {
               d3d11->flags &= ~D3D11_ST_FLAG_FRAME_DUPE_LOCK;
               return false;
            }
            --bfi_light_frames;
         }
         d3d11->flags &= ~D3D11_ST_FLAG_FRAME_DUPE_LOCK;
      }

      for (n = 0; n < video_info->bfi_dark_frames; ++n)
      {
         if (!(d3d11->flags & D3D11_ST_FLAG_FRAME_DUPE_LOCK))
         {
            context->lpVtbl->OMSetRenderTargets(context, 1, &rtv, NULL);
            context->lpVtbl->ClearRenderTargetView(context, rtv, d3d11->clearcolor);
            DXGIPresent(d3d11->swapChain, d3d11->swap_interval, present_flags);
         }
      }
   }

   /* Frame duping for Shader Subframes, don't combine with swap_interval > 1, BFI.
      Also, a major logical use of shader sub-frames will still be shader implemented BFI
      or even rolling scan bfi, so we need to protect the menu/ff/etc from bad flickering
      from improper settings, and unnecessary performance overhead for ff, screenshots etc. */
   if (      (video_info->shader_subframes > 1)
         &&  !black_frame_insertion
         &&  !nonblock_state
         &&  !runloop_is_slowmotion
         &&  !runloop_is_paused
         &&  (!(d3d11->flags & D3D11_ST_FLAG_MENU_ENABLE))
         &&  (!(d3d11->flags & D3D11_ST_FLAG_FRAME_DUPE_LOCK)))
   {
      d3d11->flags |= D3D11_ST_FLAG_FRAME_DUPE_LOCK;
      for (k = 1; k < video_info->shader_subframes; k++)
      {
#ifdef D3D11_ROLLING_SCANLINE_SIMULATION
         video_info->current_subframe = k;
#endif /* D3D11_ROLLING_SCANLINE_SIMULATION */

         if (d3d11->shader_preset)
            for (m = 0; m < d3d11->shader_preset->passes; m++)
            {
               d3d11->pass[m].total_subframes = video_info->shader_subframes;
               d3d11->pass[m].current_subframe = k+1;
            }
         if (!d3d11_gfx_frame(d3d11, NULL, 0, 0, frame_count, 0, msg,
                  video_info))
         {
            d3d11->flags &= ~D3D11_ST_FLAG_FRAME_DUPE_LOCK;
            return false;
         }
      }
      d3d11->flags &= ~D3D11_ST_FLAG_FRAME_DUPE_LOCK;
   }

   Release(rtv);

   return true;
}

static void d3d11_gfx_set_nonblock_state(void* data,
      bool toggle,
      bool adaptive_vsync_enabled,
      unsigned swap_interval)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   if (!d3d11)
      return;

   if (toggle)
      d3d11->flags     &= ~D3D11_ST_FLAG_VSYNC;
   else
      d3d11->flags     |=  D3D11_ST_FLAG_VSYNC;

   d3d11->swap_interval = (!toggle) ? swap_interval : 0;
}

static bool d3d11_gfx_alive(void* data)
{
   bool quit;
   bool resize_chain    = false;
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   win32_check_window(NULL,
         &quit,
         &resize_chain,
         &d3d11->vp.full_width,
         &d3d11->vp.full_height);

   if (resize_chain)
      d3d11->flags |=  D3D11_ST_FLAG_RESIZE_CHAIN;
   else
      d3d11->flags &= ~D3D11_ST_FLAG_RESIZE_CHAIN;

   if (     (d3d11->flags & D3D11_ST_FLAG_RESIZE_CHAIN)
         && (d3d11->vp.full_width  != 0)
         && (d3d11->vp.full_height != 0))
      video_driver_set_size(d3d11->vp.full_width, d3d11->vp.full_height);

   return !quit;
}

static bool d3d11_gfx_suppress_screensaver(void* data, bool enable) { return false; }
static bool d3d11_gfx_has_windowed(void* data) { return true; }

static struct video_shader* d3d11_gfx_get_current_shader(void* data)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   if (!d3d11)
      return NULL;

   return d3d11->shader_preset;
}

static void d3d11_gfx_viewport_info(void* data, struct video_viewport* vp)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   *vp = d3d11->vp;
}

static bool d3d11_gfx_read_viewport(void* data, uint8_t* buffer, bool is_idle)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;
   ID3D11Texture2D* BackBuffer;
   DXGISwapChain m_SwapChain;
   ID3D11Texture2D* BackBufferStagingTexture;
   ID3D11Resource* BackBufferStaging = NULL;
   ID3D11Resource* BackBufferResource = NULL;
   D3D11_TEXTURE2D_DESC StagingDesc;
   D3D11_MAPPED_SUBRESOURCE Map;
   const uint8_t* BackBufferData;
   uint8_t* bufferRow;
   uint32_t y;
   uint32_t x;
   bool ret;

   if (!d3d11)
      return false;

   /*This implementation produces wrong result when using HDR*/
#ifdef HAVE_DXGI_HDR
   if ((d3d11->flags & D3D11_ST_FLAG_HDR_ENABLE))
   {
      RARCH_ERR("[D3D11]: HDR screenshot not supported.\n");
      return false;
   }
#endif

   /* Get the back buffer. */
   m_SwapChain = d3d11->swapChain;
#ifdef __cplusplus
   m_SwapChain->lpVtbl->GetBuffer(m_SwapChain, 0, IID_ID3D11Texture2D, (void**)(&BackBuffer));
#else
   m_SwapChain->lpVtbl->GetBuffer(m_SwapChain, 0, &IID_ID3D11Texture2D, (void*)(&BackBuffer));
#endif

   if (!BackBuffer)
      return false;

   if (!is_idle)
   {
      video_driver_cached_frame();
   }

   /* Set the staging desc. */
   BackBuffer->lpVtbl->GetDesc(BackBuffer, &StagingDesc);
   StagingDesc.Usage = D3D11_USAGE_STAGING;
   StagingDesc.BindFlags = 0;
   StagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

   /* Create the back buffer staging texture. */
   d3d11->device->lpVtbl->CreateTexture2D(d3d11->device, &StagingDesc, NULL, &BackBufferStagingTexture);

#ifdef __cplusplus
   BackBufferStagingTexture->lpVtbl->QueryInterface(BackBufferStagingTexture, IID_ID3D11Resource, (void**)&BackBufferStaging);
   BackBuffer->lpVtbl->QueryInterface(BackBuffer, IID_ID3D11Resource, (void**)&BackBufferResource);
#else
   BackBufferStagingTexture->lpVtbl->QueryInterface(BackBufferStagingTexture, &IID_ID3D11Resource, (void**)&BackBufferStaging);
   BackBuffer->lpVtbl->QueryInterface(BackBuffer, &IID_ID3D11Resource, (void**)&BackBufferResource);
#endif

   /* Copy back buffer to back buffer staging. */
   d3d11->context->lpVtbl->CopyResource(d3d11->context, BackBufferStaging, BackBufferResource);

   /* Create the image. */
   d3d11->context->lpVtbl->Map(d3d11->context, BackBufferStaging, 0, D3D11_MAP_READ, 0, &Map);
   BackBufferData = (const uint8_t*)Map.pData;

   /* Assuming format is DXGI_FORMAT_R8G8B8A8_UNORM */
   if (StagingDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM)
   {
      unsigned vp_y      = (d3d11->vp.y > 0) ? d3d11->vp.y : 0;
      unsigned vp_width  = (d3d11->vp.width  > d3d11->vp.full_width)  ? d3d11->vp.full_width  : d3d11->vp.width;
      unsigned vp_height = (d3d11->vp.height > d3d11->vp.full_height) ? d3d11->vp.full_height : d3d11->vp.height;

      BackBufferData += Map.RowPitch * vp_y;

      for (y = 0; y < vp_height; y++, BackBufferData += Map.RowPitch)
      {
         bufferRow = buffer + 3 * (vp_height - y - 1) * vp_width;

         for (x = 0; x < vp_width; x++)
         {
            bufferRow[3 * x + 2] = BackBufferData[4 * (x + d3d11->vp.x) + 0];
            bufferRow[3 * x + 1] = BackBufferData[4 * (x + d3d11->vp.x) + 1];
            bufferRow[3 * x + 0] = BackBufferData[4 * (x + d3d11->vp.x) + 2];
         }
      }
      ret = true;
   }
   else
   {
      RARCH_ERR("[D3D11]: Unexpected swapchain format.\n");
      ret = false;
   }

   d3d11->context->lpVtbl->Unmap(d3d11->context, BackBufferStaging, 0);

   /* Release the backbuffer staging. */
   BackBufferStaging->lpVtbl->Release(BackBufferStaging);
   BackBufferResource->lpVtbl->Release(BackBufferResource);
   BackBufferStagingTexture->lpVtbl->Release(BackBufferStagingTexture);
   BackBuffer->lpVtbl->Release(BackBuffer);

   return ret;
}

static void d3d11_set_menu_texture_frame(
      void* data, const void* frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   d3d11_video_t* d3d11    = (d3d11_video_t*)data;
   settings_t*    settings = config_get_ptr();
   bool menu_linear_filter = settings->bools.menu_linear_filter;
   DXGI_FORMAT    format   = rgb32 ? DXGI_FORMAT_B8G8R8A8_UNORM :
      (DXGI_FORMAT)DXGI_FORMAT_EX_A4R4G4B4_UNORM;

   if (
         d3d11->menu.texture.desc.Width  != width ||
         d3d11->menu.texture.desc.Height != height)
   {
      d3d11->menu.texture.desc.Format = format;
      d3d11->menu.texture.desc.Width  = width;
      d3d11->menu.texture.desc.Height = height;
      d3d11_release_texture(&d3d11->menu.texture);
      d3d11_init_texture(d3d11->device, &d3d11->menu.texture);
   }

   if (d3d11->menu.texture.staging)
      d3d11_update_texture(d3d11->context, width, height, 0,
            format, frame, &d3d11->menu.texture);
   d3d11->menu.texture.sampler = d3d11->samplers
      [menu_linear_filter
         ? RARCH_FILTER_LINEAR
         : RARCH_FILTER_NEAREST][RARCH_WRAP_DEFAULT];
}

static void d3d11_set_menu_texture_enable(void* data, bool state, bool full_screen)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   if (!d3d11)
      return;

   if (state)
      d3d11->flags |=  D3D11_ST_FLAG_MENU_ENABLE;
   else
      d3d11->flags &= ~D3D11_ST_FLAG_MENU_ENABLE;

   if (full_screen)
      d3d11->flags |=  D3D11_ST_FLAG_MENU_FULLSCREEN;
   else
      d3d11->flags &= ~D3D11_ST_FLAG_MENU_FULLSCREEN;
}

static void d3d11_gfx_set_aspect_ratio(void* data, unsigned aspect_ratio_idx)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   if (d3d11)
      d3d11->flags |= D3D11_ST_FLAG_KEEP_ASPECT | D3D11_ST_FLAG_RESIZE_VIEWPORT;
}

static void d3d11_gfx_apply_state_changes(void* data)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;
   if (d3d11)
      d3d11->flags |= D3D11_ST_FLAG_RESIZE_VIEWPORT;
}

static void d3d11_gfx_set_osd_msg(
      void* data, const char *msg,
      const struct font_params *params, void *font)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   if (d3d11 && (d3d11->flags & D3D11_ST_FLAG_SPRITES_ENABLE))
      font_driver_render_msg(d3d11, msg, params, font);
}

static uintptr_t d3d11_gfx_load_texture(
      void* video_data, void* data, bool threaded, enum texture_filter_type filter_type)
{
   d3d11_texture_t*      texture = NULL;
   d3d11_video_t*        d3d11   = (d3d11_video_t*)video_data;
   struct texture_image* image   = (struct texture_image*)data;

   if (!d3d11)
      return 0;

   texture = (d3d11_texture_t*)calloc(1, sizeof(*texture));

   if (!texture)
      return 0;

   switch (filter_type)
   {
      case TEXTURE_FILTER_MIPMAP_LINEAR:
#ifndef __WINRT__
         texture->desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
#endif
         /* fallthrough */
      case TEXTURE_FILTER_LINEAR:
         texture->sampler = d3d11->samplers[RARCH_FILTER_LINEAR][RARCH_WRAP_EDGE];
         break;
      case TEXTURE_FILTER_MIPMAP_NEAREST:
#ifndef __WINRT__
         texture->desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
#endif
         /* fallthrough */
      case TEXTURE_FILTER_NEAREST:
         texture->sampler = d3d11->samplers[RARCH_FILTER_NEAREST][RARCH_WRAP_EDGE];
         break;
   }

   texture->desc.Width  = image->width;
   texture->desc.Height = image->height;
   texture->desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

   d3d11_release_texture(texture);
   d3d11_init_texture(d3d11->device, texture);

   if (texture->staging)
      d3d11_update_texture(
            d3d11->context, image->width, image->height, 0, DXGI_FORMAT_B8G8R8A8_UNORM, image->pixels,
            texture);

   return (uintptr_t)texture;
}

static void d3d11_gfx_unload_texture(void* data,
      bool threaded, uintptr_t handle)
{
   d3d11_texture_t* texture = (d3d11_texture_t*)handle;

   if (!texture)
      return;

   Release(texture->view);
   Release(texture->staging);
   Release(texture->handle);
   free(texture);
}

static bool d3d11_get_hw_render_interface(
      void* data, const struct retro_hw_render_interface** iface)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;
   *iface               = (const struct retro_hw_render_interface*)
      &d3d11->hw_iface;
   return ((d3d11->flags & D3D11_ST_FLAG_HW_IFACE_ENABLE) > 0);
}

#ifndef __WINRT__
static void d3d11_get_video_output_prev(void *data)
{
   unsigned width  = 0;
   unsigned height = 0;
   win32_get_video_output_prev(&width, &height);
}

static void d3d11_get_video_output_next(void *data)
{
   unsigned width  = 0;
   unsigned height = 0;
   win32_get_video_output_next(&width, &height);
}
#endif

static const video_poke_interface_t d3d11_poke_interface = {
   d3d11_get_flags,
   d3d11_gfx_load_texture,
   d3d11_gfx_unload_texture,
   NULL, /* set_video_mode */
#ifdef __WINRT__
   /* UWP does not expose this information easily */
   NULL, /* get_refresh_rate */
#else
   win32_get_refresh_rate,
#endif
   d3d11_set_filtering,
#ifdef __WINRT__
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
#else
   win32_get_video_output_size,
   d3d11_get_video_output_prev,
   d3d11_get_video_output_next,
#endif
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   d3d11_gfx_set_aspect_ratio,
   d3d11_gfx_apply_state_changes,
   d3d11_set_menu_texture_frame,
   d3d11_set_menu_texture_enable,
   d3d11_gfx_set_osd_msg,
   win32_show_cursor,
   NULL, /* grab_mouse_toggle */
   d3d11_gfx_get_current_shader,
   NULL, /* get_current_software_framebuffer */
   d3d11_get_hw_render_interface,
#ifdef HAVE_DXGI_HDR
   d3d11_set_hdr_max_nits,
   d3d11_set_hdr_paper_white_nits,
   d3d11_set_hdr_contrast,
   d3d11_set_hdr_expand_gamut
#else
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
#endif
};

static void d3d11_gfx_get_poke_interface(void* data,
      const video_poke_interface_t** iface)
{
   *iface = &d3d11_poke_interface;
}

#if defined(HAVE_GFX_WIDGETS)
static bool d3d11_gfx_widgets_enabled(void *data) { return true; }
#endif

video_driver_t video_d3d11 = {
   d3d11_gfx_init,
   d3d11_gfx_frame,
   d3d11_gfx_set_nonblock_state,
   d3d11_gfx_alive,
   win32_has_focus,
   d3d11_gfx_suppress_screensaver,
   d3d11_gfx_has_windowed,
   d3d11_gfx_set_shader,
   d3d11_gfx_free,
   "d3d11",
   NULL, /* set_viewport */
   d3d11_gfx_set_rotation,
   d3d11_gfx_viewport_info,
   d3d11_gfx_read_viewport,
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   d3d11_get_overlay_interface,
#endif
   d3d11_gfx_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#if defined(HAVE_GFX_WIDGETS)
   d3d11_gfx_widgets_enabled
#endif
};
