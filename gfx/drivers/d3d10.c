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

/* Direct3D 10 driver.
 *
 * Minimum version : Direct3D 10.0 (2006)
 * Minimum OS      : Windows Vista
 * Recommended OS  : Windows Vista and/or later
 */

#define CINTERFACE
#define COBJMACROS

#include <string.h>
#include <malloc.h>
#include <math.h>

#include <string/stdstring.h>
#include <file/file_path.h>
#include <encodings/utf.h>
#include <lists/string_list.h>
#include <formats/image.h>

#include <dxgi.h>

#include "../../verbosity.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#ifdef HAVE_REWIND
#include "../../state_manager.h"
#endif

#include "../font_driver.h"
#include "../common/d3d_common.h"
#include "../common/win32_common.h"
#include "../common/d3d10_defines.h"
#include "../common/dxgi_common.h"
#include "../common/d3dcompiler_common.h"
#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#ifdef __WINRT__
#error "UWP does not support D3D10"
#endif

#define D3D10_ROLLING_SCANLINE_SIMULATION

typedef struct
{
   d3d10_texture_t               texture;
   const font_renderer_driver_t* font_driver;
   void*                         font_data;
   struct font_atlas*            atlas;
} d3d10_font_t;

/* Temporary workaround for d3d10 not being able to poll flags during init */
static gfx_ctx_driver_t d3d10_fake_context;
static uint32_t d3d10_get_flags(void *data);

/*
 * D3D10 COMMON
 */
static INLINE void d3d10_set_shader(D3D10Device ctx, d3d10_shader_t* shader)
{
   ctx->lpVtbl->IASetInputLayout(ctx, shader->layout);
   ctx->lpVtbl->VSSetShader(ctx, shader->vs);
   ctx->lpVtbl->PSSetShader(ctx, shader->ps);
   ctx->lpVtbl->GSSetShader(ctx, shader->gs);
}

static INLINE void
d3d10_set_texture_and_sampler(D3D10Device ctx, UINT slot,
      d3d10_texture_t* texture)
{
   ctx->lpVtbl->PSSetShaderResources(ctx, slot, 1, &texture->view);
   ctx->lpVtbl->PSSetSamplers(ctx, slot, 1,
         (D3D10SamplerState*)&texture->sampler);
}

static INLINE void d3d10_release_texture(d3d10_texture_t* texture)
{
   Release(texture->handle);
   Release(texture->staging);
   Release(texture->view);
   Release(texture->rt_view);
}

static INLINE void d3d10_release_shader(d3d10_shader_t* shader)
{
   Release(shader->layout);
   Release(shader->vs);
   Release(shader->ps);
   Release(shader->gs);
}

static DXGI_FORMAT
d3d10_get_closest_match(D3D10Device device,
      DXGI_FORMAT desired_format, UINT desired_format_support)
{
   DXGI_FORMAT default_list[] = {desired_format, DXGI_FORMAT_UNKNOWN};
   DXGI_FORMAT* format = dxgi_get_format_fallback_list(desired_format);

   if (!format)
      format = default_list;

   while (*format != DXGI_FORMAT_UNKNOWN)
   {
      UINT format_support;
      if (SUCCEEDED(device->lpVtbl->CheckFormatSupport(device, *format,
                  &format_support))
            && ((format_support & desired_format_support) ==
               desired_format_support))
         break;
      format++;
   }
   return *format;
}

static void d3d10_init_texture(D3D10Device device, d3d10_texture_t* texture)
{
   bool is_render_target   = texture->desc.BindFlags
                           & D3D10_BIND_RENDER_TARGET;
   UINT format_support     = D3D10_FORMAT_SUPPORT_TEXTURE2D
                           | D3D10_FORMAT_SUPPORT_SHADER_SAMPLE;

   texture->desc.MipLevels          = 1;
   texture->desc.ArraySize          = 1;
   texture->desc.SampleDesc.Count   = 1;
   texture->desc.SampleDesc.Quality = 0;
   texture->desc.BindFlags         |= D3D10_BIND_SHADER_RESOURCE;
   texture->desc.CPUAccessFlags     =
      texture->desc.Usage == D3D10_USAGE_DYNAMIC
      ? D3D10_CPU_ACCESS_WRITE
      : 0;

   if (texture->desc.MiscFlags & D3D10_RESOURCE_MISC_GENERATE_MIPS)
   {
      unsigned width, height;

      texture->desc.BindFlags |= D3D10_BIND_RENDER_TARGET;
      width                    = texture->desc.Width  >> 5;
      height                   = texture->desc.Height >> 5;

      while (width && height)
      {
         width  >>= 1;
         height >>= 1;
         texture->desc.MipLevels++;
      }
   }

   if (texture->desc.BindFlags & D3D10_BIND_RENDER_TARGET)
      format_support   |= D3D10_FORMAT_SUPPORT_RENDER_TARGET;

   texture->desc.Format = d3d10_get_closest_match(
         device, texture->desc.Format, format_support);

   device->lpVtbl->CreateTexture2D(device, &texture->desc, NULL,
         &texture->handle);

   {
      D3D10_SHADER_RESOURCE_VIEW_DESC view_desc;
      view_desc.Format                          = texture->desc.Format;
      view_desc.ViewDimension                   = D3D_SRV_DIMENSION_TEXTURE2D;
      view_desc.Texture2D.MostDetailedMip       = 0;
      view_desc.Texture2D.MipLevels             = -1;
      device->lpVtbl->CreateShaderResourceView(device,
            (D3D10Resource)texture->handle, &view_desc, &texture->view);
   }

   if (is_render_target)
      device->lpVtbl->CreateRenderTargetView(device,
            (D3D10Resource)texture->handle, NULL, &texture->rt_view);
   else
   {
      D3D10_TEXTURE2D_DESC desc = texture->desc;
      desc.MipLevels            = 1;
      desc.BindFlags            = 0;
      desc.MiscFlags            = 0;
      desc.Usage                = D3D10_USAGE_STAGING;
      desc.CPUAccessFlags       = D3D10_CPU_ACCESS_WRITE;
      device->lpVtbl->CreateTexture2D(device, &desc, NULL,
            &texture->staging);
   }

   texture->size_data.x = texture->desc.Width;
   texture->size_data.y = texture->desc.Height;
   texture->size_data.z = 1.0f / texture->desc.Width;
   texture->size_data.w = 1.0f / texture->desc.Height;
}

static void d3d10_update_texture(
      D3D10Device      ctx,
      int              width,
      int              height,
      int              pitch,
      DXGI_FORMAT      format,
      const void*      data,
      d3d10_texture_t* texture)
{
   D3D10_MAPPED_TEXTURE2D mapped_texture;
   D3D10_BOX                frame_box;

   texture->staging->lpVtbl->Map(texture->staging, 0, D3D10_MAP_WRITE, 0,
         &mapped_texture);

   dxgi_copy(
         width, height, format, pitch, data, texture->desc.Format,
         mapped_texture.RowPitch,
         mapped_texture.pData);

   texture->staging->lpVtbl->Unmap(texture->staging, 0);
   frame_box.left   = 0;
   frame_box.top    = 0;
   frame_box.front  = 0;
   frame_box.right  = (UINT)width;
   frame_box.bottom = (UINT)height;
   frame_box.back   = 1;
   ctx->lpVtbl->CopySubresourceRegion(
         ctx, (D3D10Resource)texture->handle, 0, 0, 0, 0,
         (D3D10Resource)texture->staging, 0, &frame_box);

   if (texture->desc.MiscFlags & D3D10_RESOURCE_MISC_GENERATE_MIPS)
      ctx->lpVtbl->GenerateMips(ctx, texture->view);
}

static bool d3d10_init_shader(
      D3D10Device                     device,
      const char*                     src,
      size_t                          size,
      const void*                     src_name,
      LPCSTR                          vs_entry,
      LPCSTR                          ps_entry,
      LPCSTR                          gs_entry,
      const D3D10_INPUT_ELEMENT_DESC* input_element_descs,
      UINT                            num_elements,
      d3d10_shader_t*                 out)
{
   D3DBlob vs_code = NULL;
   D3DBlob ps_code = NULL;
   D3DBlob gs_code = NULL;

   bool success    = true;

   if (!src) /* LPCWSTR filename */
   {
      if (vs_entry && !d3d_compile_from_file((LPCWSTR)src_name, vs_entry, "vs_4_0", &vs_code))
         success = false;
      if (ps_entry && !d3d_compile_from_file((LPCWSTR)src_name, ps_entry, "ps_4_0", &ps_code))
         success = false;
      if (gs_entry && !d3d_compile_from_file((LPCWSTR)src_name, gs_entry, "gs_4_0", &gs_code))
         success = false;
   }
   else /* char array */
   {
      if (vs_entry && !d3d_compile(src, size, (LPCSTR)src_name, vs_entry, "vs_4_0", &vs_code))
         success = false;
      if (ps_entry && !d3d_compile(src, size, (LPCSTR)src_name, ps_entry, "ps_4_0", &ps_code))
         success = false;
      if (gs_entry && !d3d_compile(src, size, (LPCSTR)src_name, gs_entry, "gs_4_0", &gs_code))
         success = false;
   }


   if (ps_code)
      device->lpVtbl->CreatePixelShader(device,
            ps_code->lpVtbl->GetBufferPointer(ps_code),
            ps_code->lpVtbl->GetBufferSize(ps_code),
            &out->ps);

   if (gs_code)
      device->lpVtbl->CreateGeometryShader(
            device,
            gs_code->lpVtbl->GetBufferPointer(gs_code),
            gs_code->lpVtbl->GetBufferSize(gs_code),
            &out->gs);

   if (vs_code)
   {
      LPVOID buf_ptr  = vs_code->lpVtbl->GetBufferPointer(vs_code);
      SIZE_T buf_size = vs_code->lpVtbl->GetBufferSize(vs_code);
      device->lpVtbl->CreateVertexShader(device, buf_ptr, buf_size, &out->vs);
      if (input_element_descs)
         device->lpVtbl->CreateInputLayout(
               device, (D3D10_INPUT_ELEMENT_DESC*)input_element_descs,
               num_elements, buf_ptr, buf_size, &out->layout);
   }

   Release(vs_code);
   Release(ps_code);
   Release(gs_code);

   return success;
}

/*
 * DISPLAY DRIVER
 */

static void gfx_display_d3d10_blend_begin(void *data)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;
   d3d10->device->lpVtbl->OMSetBlendState(d3d10->device,
         d3d10->blend_enable,
         NULL, D3D10_DEFAULT_SAMPLE_MASK);
}

static void gfx_display_d3d10_blend_end(void *data)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;
   d3d10->device->lpVtbl->OMSetBlendState(d3d10->device,
         d3d10->blend_disable,
         NULL, D3D10_DEFAULT_SAMPLE_MASK);
}

static void gfx_display_d3d10_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   UINT offset = 0, stride = 0;
   int vertex_count        = 1;
   d3d10_video_t* d3d10    = (d3d10_video_t*)data;

   if (!d3d10 || !draw || !draw->texture)
      return;

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         d3d10_set_shader(d3d10->device, &d3d10->shaders[draw->pipeline_id]);
         d3d10->device->lpVtbl->Draw(d3d10->device,
               draw->coords->vertices, 0);
         d3d10->device->lpVtbl->OMSetBlendState(d3d10->device,
               d3d10->blend_enable,
               NULL, D3D10_DEFAULT_SAMPLE_MASK);
         d3d10_set_shader(d3d10->device, &d3d10->sprites.shader);
         stride = sizeof(d3d10_sprite_t);
         d3d10->device->lpVtbl->IASetVertexBuffers(
               d3d10->device, 0, 1, (D3D10Buffer* const)&d3d10->sprites.vbo,
               &stride, &offset);
         d3d10->device->lpVtbl->IASetPrimitiveTopology(d3d10->device,
               D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
         return;
      default:
         break;
   }

   if (draw->coords->vertex && draw->coords->tex_coord && draw->coords->color)
      vertex_count = draw->coords->vertices;

   if (     (!(d3d10->flags & D3D10_ST_FLAG_SPRITES_ENABLE))
         || (vertex_count > d3d10->sprites.capacity))
      return;

   if (d3d10->sprites.offset + vertex_count > d3d10->sprites.capacity)
      d3d10->sprites.offset = 0;

   {
      void*           mapped_vbo = NULL;
      d3d10_sprite_t* sprite     = NULL;

      d3d10->sprites.vbo->lpVtbl->Map(d3d10->sprites.vbo,
            D3D10_MAP_WRITE_NO_OVERWRITE, 0,
            (void**)&mapped_vbo);

      sprite = (d3d10_sprite_t*)mapped_vbo + d3d10->sprites.offset;

      if (vertex_count == 1)
      {
         sprite->pos.x       = draw->x / (float)d3d10->viewport.Width;
         sprite->pos.y       =
               (d3d10->viewport.Height - draw->y - draw->height)
               / (float)d3d10->viewport.Height;
         sprite->pos.w       = draw->width  / (float)d3d10->viewport.Width;
         sprite->pos.h       = draw->height / (float)d3d10->viewport.Height;

         sprite->coords.u    = 0.0f;
         sprite->coords.v    = 0.0f;
         sprite->coords.w    = 1.0f;
         sprite->coords.h    = 1.0f;

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
            d3d10_vertex_t* v = (d3d10_vertex_t*)sprite;
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

         d3d10_set_shader(d3d10->device,
               &d3d10->shaders[VIDEO_SHADER_STOCK_BLEND]);
         d3d10->device->lpVtbl->IASetPrimitiveTopology(d3d10->device,
               D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      }

      d3d10->sprites.vbo->lpVtbl->Unmap(d3d10->sprites.vbo);
   }

   d3d10_set_texture_and_sampler(d3d10->device, 0,
         (d3d10_texture_t*)draw->texture);
   d3d10->device->lpVtbl->Draw(d3d10->device, vertex_count,
         d3d10->sprites.offset);
   d3d10->sprites.offset += vertex_count;

   if (vertex_count > 1)
   {
      d3d10_set_shader(d3d10->device, &d3d10->sprites.shader);
      d3d10->device->lpVtbl->IASetPrimitiveTopology(d3d10->device,
            D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
   }
}

static void gfx_display_d3d10_draw_pipeline(gfx_display_ctx_draw_t* draw,
      gfx_display_t *p_disp,
      void *data, unsigned video_width, unsigned video_height)
{
   UINT stride = 0, offset   = 0;
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (!d3d10 || !draw)
      return;

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      {
         video_coord_array_t* ca   = &p_disp->dispca;

         if (!d3d10->menu_pipeline_vbo)
         {
            D3D10_BUFFER_DESC desc;
            D3D10_SUBRESOURCE_DATA vertex_data;

            desc.ByteWidth               = ca->coords.vertices * 2 * sizeof(float);
            desc.Usage                   = D3D10_USAGE_IMMUTABLE;
            desc.BindFlags               = D3D10_BIND_VERTEX_BUFFER;
            desc.CPUAccessFlags          = 0;
            desc.MiscFlags               = 0;

            vertex_data.pSysMem          = ca->coords.vertex;
            vertex_data.SysMemPitch      = 0;
            vertex_data.SysMemSlicePitch = 0;
            d3d10->device->lpVtbl->CreateBuffer(d3d10->device, &desc,
                  &vertex_data, &d3d10->menu_pipeline_vbo);
         }
         stride = 2 * sizeof(float);
         d3d10->device->lpVtbl->IASetVertexBuffers(
               d3d10->device, 0, 1,
               (D3D10Buffer* const)&d3d10->menu_pipeline_vbo,
               &stride, &offset);
         draw->coords->vertices = ca->coords.vertices;
         d3d10->device->lpVtbl->OMSetBlendState(d3d10->device,
               d3d10->blend_pipeline,
               NULL, D3D10_DEFAULT_SAMPLE_MASK);
         break;
      }

      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         stride = sizeof(d3d10_vertex_t);
         d3d10->device->lpVtbl->IASetVertexBuffers(
               d3d10->device, 0, 1,
               (D3D10Buffer* const)&d3d10->frame.vbo, &stride, &offset);
         draw->coords->vertices = 4;
         break;
      default:
         return;
   }

   d3d10->device->lpVtbl->IASetPrimitiveTopology(d3d10->device,
         D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

   d3d10->ubo_values.time += 0.01f;

   {
      void *mapped_ubo              = NULL;
      d3d10->ubo->lpVtbl->Map(d3d10->ubo, D3D10_MAP_WRITE_DISCARD, 0,
            (void**)&mapped_ubo);
      *(d3d10_uniform_t*)mapped_ubo = d3d10->ubo_values;
      d3d10->ubo->lpVtbl->Unmap(d3d10->ubo);
   }
}

void gfx_display_d3d10_scissor_begin(void *data,
      unsigned video_width, unsigned video_height,
      int x, int y, unsigned width, unsigned height)
{
   D3D10_RECT rect;
   d3d10_video_t *d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   rect.left            = x;
   rect.top             = y;
   rect.right           = width + x;
   rect.bottom          = height + y;

   d3d10->device->lpVtbl->RSSetScissorRects(d3d10->device, 1, &rect);
}

void gfx_display_d3d10_scissor_end(void *data,
      unsigned video_width, unsigned video_height)
{
   D3D10_RECT rect;
   d3d10_video_t *d3d10  = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   rect.left            = 0;
   rect.top             = 0;
   rect.right           = video_width;
   rect.bottom          = video_height;

   d3d10->device->lpVtbl->RSSetScissorRects(d3d10->device, 1, &rect);
}

gfx_display_ctx_driver_t gfx_display_ctx_d3d10 = {
   gfx_display_d3d10_draw,
   gfx_display_d3d10_draw_pipeline,
   gfx_display_d3d10_blend_begin,
   gfx_display_d3d10_blend_end,
   NULL,                                     /* get_default_mvp        */
   NULL,                                     /* get_default_vertices   */
   NULL,                                     /* get_default_tex_coords */
   FONT_DRIVER_RENDER_D3D10_API,
   GFX_VIDEO_DRIVER_DIRECT3D10,
   "d3d10",
   true,
   gfx_display_d3d10_scissor_begin,
   gfx_display_d3d10_scissor_end
};

/*
 * FONT DRIVER
 */

static void *d3d10_font_init(void* data, const char* font_path,
      float font_size, bool is_threaded)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;
   d3d10_font_t*  font  = (d3d10_font_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   if (!font_renderer_create_default(
             &font->font_driver, &font->font_data, font_path, font_size))
   {
      free(font);
      return NULL;
   }

   font->atlas               = font->font_driver->get_atlas(font->font_data);
   font->texture.sampler     = d3d10->samplers[RARCH_FILTER_LINEAR][RARCH_WRAP_BORDER];
   font->texture.desc.Width  = font->atlas->width;
   font->texture.desc.Height = font->atlas->height;
   font->texture.desc.Format = DXGI_FORMAT_A8_UNORM;
   d3d10_release_texture(&font->texture);
   d3d10_init_texture(d3d10->device, &font->texture);
   if (font->texture.staging)
      d3d10_update_texture(
            d3d10->device,
            font->atlas->width, font->atlas->height, font->atlas->width,
            DXGI_FORMAT_A8_UNORM, font->atlas->buffer, &font->texture);
   font->atlas->dirty        = false;

   return font;
}

static void d3d10_font_free(void* data, bool is_threaded)
{
   d3d10_font_t* font = (d3d10_font_t*)data;

   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   Release(font->texture.handle);
   Release(font->texture.staging);
   Release(font->texture.view);
   free(font);
}

static int d3d10_font_get_message_width(void* data,
      const char* msg, size_t msg_len, float scale)
{
   size_t i;
   int      delta_x                 = 0;
   const struct font_glyph* glyph_q = NULL;
   d3d10_font_t* font               = (d3d10_font_t*)data;

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

static void d3d10_font_render_line(
      d3d10_video_t *d3d10,
      d3d10_font_t*       font,
      const struct font_glyph* glyph_q,
      const char*         msg,
      size_t              msg_len,
      float               scale,
      const unsigned int  color,
      float               pos_x,
      float               pos_y,
      int pre_x,
      unsigned            width,
      unsigned            height,
      unsigned            text_align)
{
   unsigned i, count;
   void *mapped_vbo;
   d3d10_sprite_t *v;
   int x                    = pre_x;
   int y                    = roundf((1.0 - pos_y) * height);

   if (d3d10->sprites.offset + msg_len > (unsigned)d3d10->sprites.capacity)
      d3d10->sprites.offset = 0;

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= d3d10_font_get_message_width(font, msg, msg_len, scale);
         break;

      case TEXT_ALIGN_CENTER:
         x -= d3d10_font_get_message_width(font, msg, msg_len, scale) / 2;
         break;
   }

   d3d10->sprites.vbo->lpVtbl->Map(d3d10->sprites.vbo,
         D3D10_MAP_WRITE_NO_OVERWRITE, 0, (void**)&mapped_vbo);

   v       = (d3d10_sprite_t*)mapped_vbo + d3d10->sprites.offset;

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph *glyph;
      const char *msg_tmp = &msg[i];
      unsigned   code     = utf8_walk(&msg_tmp);
      unsigned   skip     = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      /* Do something smarter here ... */
      if (!(glyph = font->font_driver->get_glyph(font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      v->pos.x           = (x + (glyph->draw_offset_x * scale)) / (float)d3d10->viewport.Width;
      v->pos.y           = (y + (glyph->draw_offset_y * scale)) / (float)d3d10->viewport.Height;
      v->pos.w           = glyph->width * scale  / (float)d3d10->viewport.Width;
      v->pos.h           = glyph->height * scale / (float)d3d10->viewport.Height;

      v->coords.u        = glyph->atlas_offset_x / (float)font->texture.desc.Width;
      v->coords.v        = glyph->atlas_offset_y / (float)font->texture.desc.Height;
      v->coords.w        = glyph->width  / (float)font->texture.desc.Width;
      v->coords.h        = glyph->height / (float)font->texture.desc.Height;

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

   count = v - ((d3d10_sprite_t*)mapped_vbo + d3d10->sprites.offset);
   d3d10->sprites.vbo->lpVtbl->Unmap(d3d10->sprites.vbo);

   if (!count)
      return;

   if (font->atlas->dirty)
   {
      if (font->texture.staging)
         d3d10_update_texture(
               d3d10->device,
               font->atlas->width, font->atlas->height, font->atlas->width,
               DXGI_FORMAT_A8_UNORM, font->atlas->buffer, &font->texture);
      font->atlas->dirty = false;
   }

   d3d10_set_texture_and_sampler(d3d10->device, 0, &font->texture);
   d3d10->device->lpVtbl->OMSetBlendState(d3d10->device,
         d3d10->blend_enable,
         NULL, D3D10_DEFAULT_SAMPLE_MASK);

   d3d10->device->lpVtbl->PSSetShader(d3d10->device, d3d10->sprites.shader_font.ps);
   d3d10->device->lpVtbl->Draw(d3d10->device, count, d3d10->sprites.offset);
   d3d10->device->lpVtbl->PSSetShader(d3d10->device, d3d10->sprites.shader.ps);

   d3d10->sprites.offset += count;
}

static void d3d10_font_render_message(
      d3d10_video_t *d3d10,
      d3d10_font_t*       font,
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
   const struct font_glyph* glyph_q       =
      font->font_driver->get_glyph(font->font_data, '?');
   int x                                  = roundf(pos_x * width);
   font->font_driver->get_line_metrics(font->font_data, &line_metrics);
   line_height = line_metrics->height * scale / height;

   for (;;)
   {
      const char* delim = strchr(msg, '\n');
      size_t msg_len    = delim ? (size_t)((delim - msg)) : strlen(msg);

      /* Draw the line */
      if (msg_len <= (unsigned)d3d10->sprites.capacity)
         d3d10_font_render_line(d3d10,
               font, glyph_q,
               msg, msg_len, scale, color, pos_x,
               pos_y - (float)lines * line_height,
               x,
               width, height, text_align);

      if (!delim)
         break;

      msg += msg_len + 1;
      lines++;
   }
}

static void d3d10_font_render_msg(
      void *userdata,
      void* data,
      const char* msg,
      const struct font_params *params)
{
   int drop_x, drop_y;
   enum text_alignment text_align;
   unsigned color, r, g, b, alpha;
   float x, y, scale, drop_mod, drop_alpha;
   d3d10_font_t *font          = (d3d10_font_t*)data;
   d3d10_video_t *d3d10        = (d3d10_video_t*)userdata;
   unsigned width              = d3d10->vp.full_width;
   unsigned height             = d3d10->vp.full_height;

   if (!font || !msg || !*msg)
      return;
   if (!d3d10 || (!(d3d10->flags & D3D10_ST_FLAG_SPRITES_ENABLE)))
      return;

   if (params)
   {
      x                        = params->x;
      y                        = params->y;
      scale                    = params->scale;
      text_align               = params->text_align;
      drop_x                   = params->drop_x;
      drop_y                   = params->drop_y;
      drop_mod                 = params->drop_mod;
      drop_alpha               = params->drop_alpha;

      r                        = FONT_COLOR_GET_RED(params->color);
      g                        = FONT_COLOR_GET_GREEN(params->color);
      b                        = FONT_COLOR_GET_BLUE(params->color);
      alpha                    = FONT_COLOR_GET_ALPHA(params->color);

      color                    = DXGI_COLOR_RGBA(r, g, b, alpha);
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

      r                        = (video_msg_color_r * 255);
      g                        = (video_msg_color_g * 255);
      b                        = (video_msg_color_b * 255);
      alpha                    = 255;
      color                    = DXGI_COLOR_RGBA(r, g, b, alpha);

      drop_x                   = -2;
      drop_y                   = -2;
      drop_mod                 = 0.3f;
      drop_alpha               = 1.0f;
   }

   if (drop_x || drop_y)
   {
      unsigned r_dark          = r * drop_mod;
      unsigned g_dark          = g * drop_mod;
      unsigned b_dark          = b * drop_mod;
      unsigned alpha_dark      = alpha * drop_alpha;
      unsigned color_dark      = DXGI_COLOR_RGBA(r_dark, g_dark, b_dark, alpha_dark);

      d3d10_font_render_message(d3d10,
            font, msg, scale, color_dark,
            x + scale * drop_x / width,
            y + scale * drop_y / height,
            width, height, text_align);
   }

   d3d10_font_render_message(d3d10, font, msg, scale,
         color, x, y,
         width, height, text_align);
}

static const struct font_glyph* d3d10_font_get_glyph(void *data,
      uint32_t code)
{
   d3d10_font_t* font = (d3d10_font_t*)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph((void*)font->font_driver, code);
   return NULL;
}

static bool d3d10_font_get_line_metrics(void* data,
      struct font_line_metrics **metrics)
{
   d3d10_font_t* font = (d3d10_font_t*)data;
   if (font && font->font_driver && font->font_data)
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

font_renderer_t d3d10_font = {
   d3d10_font_init,
   d3d10_font_free,
   d3d10_font_render_msg,
   "d3d10",
   d3d10_font_get_glyph,
   NULL, /* bind_block */
   NULL, /* flush */
   d3d10_font_get_message_width,
   d3d10_font_get_line_metrics
};

/*
 * VIDEO DRIVER
 */

#ifdef HAVE_OVERLAY
static void d3d10_free_overlays(d3d10_video_t* d3d10)
{
   size_t i;
   for (i = 0; i < (unsigned)d3d10->overlays.count; i++)
      d3d10_release_texture(&d3d10->overlays.textures[i]);

   Release(d3d10->overlays.vbo);
}

static void
d3d10_overlay_vertex_geom(void* data,
      unsigned index, float x, float y, float w, float h)
{
   d3d10_sprite_t* sprites = NULL;
   d3d10_video_t*  d3d10   = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   d3d10->overlays.vbo->lpVtbl->Map(d3d10->overlays.vbo,
         D3D10_MAP_WRITE_NO_OVERWRITE, 0, (void**)&sprites);

   if (sprites)
   {
      sprites[index].pos.x = x;
      sprites[index].pos.y = y;
      sprites[index].pos.w = w;
      sprites[index].pos.h = h;
   }

   d3d10->overlays.vbo->lpVtbl->Unmap(d3d10->overlays.vbo);
}

static void d3d10_overlay_tex_geom(void* data,
      unsigned index, float u, float v, float w, float h)
{
   d3d10_sprite_t* sprites = NULL;
   d3d10_video_t*  d3d10   = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   d3d10->overlays.vbo->lpVtbl->Map(d3d10->overlays.vbo,
         D3D10_MAP_WRITE_NO_OVERWRITE, 0, (void**)&sprites);
   sprites[index].coords.u = u;
   sprites[index].coords.v = v;
   sprites[index].coords.w = w;
   sprites[index].coords.h = h;
   d3d10->overlays.vbo->lpVtbl->Unmap(d3d10->overlays.vbo);
}

static void d3d10_overlay_set_alpha(void* data, unsigned index, float mod)
{
   d3d10_sprite_t* sprites = NULL;
   d3d10_video_t*  d3d10   = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   d3d10->overlays.vbo->lpVtbl->Map(d3d10->overlays.vbo,
         D3D10_MAP_WRITE_NO_OVERWRITE, 0, (void**)&sprites);

   if (sprites)
   {
      sprites[index].colors[0] = DXGI_COLOR_RGBA(0xFF, 0xFF, 0xFF, mod * 0xFF);
      sprites[index].colors[1] = sprites[index].colors[0];
      sprites[index].colors[2] = sprites[index].colors[0];
      sprites[index].colors[3] = sprites[index].colors[0];
   }
   d3d10->overlays.vbo->lpVtbl->Unmap(d3d10->overlays.vbo);
}

static bool d3d10_overlay_load(void* data,
      const void* image_data, unsigned num_images)
{
   size_t i;
   D3D10_BUFFER_DESC desc;
   d3d10_sprite_t*             sprites = NULL;
   d3d10_video_t*              d3d10   = (d3d10_video_t*)data;
   const struct texture_image* images  = (const struct texture_image*)
      image_data;

   if (!d3d10)
      return false;

   d3d10_free_overlays(d3d10);
   d3d10->overlays.count    = num_images;
   d3d10->overlays.textures = (d3d10_texture_t*)calloc(
         num_images, sizeof(d3d10_texture_t));

   desc.ByteWidth           = sizeof(d3d10_sprite_t) * num_images;
   desc.Usage               = D3D10_USAGE_DYNAMIC;
   desc.BindFlags           = D3D10_BIND_VERTEX_BUFFER;
   desc.CPUAccessFlags      = D3D10_CPU_ACCESS_WRITE;
   desc.MiscFlags           = 0;
   d3d10->device->lpVtbl->CreateBuffer(d3d10->device, &desc,
         NULL, &d3d10->overlays.vbo);
   d3d10->overlays.vbo->lpVtbl->Map(d3d10->overlays.vbo,
         D3D10_MAP_WRITE_DISCARD, 0, (void**)&sprites);

   for (i = 0; i < (unsigned)num_images; i++)
   {

      d3d10->overlays.textures[i].desc.Width  = images[i].width;
      d3d10->overlays.textures[i].desc.Height = images[i].height;
      d3d10->overlays.textures[i].desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

      d3d10_release_texture(&d3d10->overlays.textures[i]);
      d3d10_init_texture(d3d10->device, &d3d10->overlays.textures[i]);

      if (d3d10->overlays.textures[i].staging)
         d3d10_update_texture(
               d3d10->device,
               images[i].width,
               images[i].height, 0, DXGI_FORMAT_B8G8R8A8_UNORM,
               images[i].pixels, &d3d10->overlays.textures[i]);

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
   d3d10->overlays.vbo->lpVtbl->Unmap(d3d10->overlays.vbo);

   return true;
}

static void d3d10_overlay_enable(void* data, bool state)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   if (state)
      d3d10->flags |=  D3D10_ST_FLAG_OVERLAYS_ENABLE;
   else
      d3d10->flags &= ~D3D10_ST_FLAG_OVERLAYS_ENABLE;
   win32_show_cursor(d3d10, state);
}

static void d3d10_overlay_full_screen(void* data, bool enable)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   if (enable)
      d3d10->flags |=  D3D10_ST_FLAG_OVERLAYS_FULLSCREEN;
   else
      d3d10->flags &= ~D3D10_ST_FLAG_OVERLAYS_FULLSCREEN;
}

static void d3d10_get_overlay_interface(void* data, const video_overlay_interface_t** iface)
{
   static const video_overlay_interface_t overlay_interface = {
      d3d10_overlay_enable,      d3d10_overlay_load,        d3d10_overlay_tex_geom,
      d3d10_overlay_vertex_geom, d3d10_overlay_full_screen, d3d10_overlay_set_alpha,
   };

   *iface = &overlay_interface;
}

static void d3d10_render_overlay(d3d10_video_t *d3d10)
{
   int i;
   UINT offset = 0, stride = 0;

   if (d3d10->flags & D3D10_ST_FLAG_OVERLAYS_FULLSCREEN)
      d3d10->device->lpVtbl->RSSetViewports(d3d10->device, 1, &d3d10->viewport);
   else
      d3d10->device->lpVtbl->RSSetViewports(d3d10->device, 1, &d3d10->frame.viewport);

   d3d10->device->lpVtbl->OMSetBlendState(d3d10->device,
         d3d10->blend_enable, NULL,
         D3D10_DEFAULT_SAMPLE_MASK);
   stride = sizeof(d3d10_sprite_t);
   d3d10->device->lpVtbl->IASetVertexBuffers(
         d3d10->device, 0, 1, (D3D10Buffer* const)&d3d10->overlays.vbo, &stride, &offset);
   d3d10->device->lpVtbl->PSSetSamplers(d3d10->device, 0, 1,
         &d3d10->samplers[RARCH_FILTER_UNSPEC][RARCH_WRAP_DEFAULT]);

   for (i = 0; i < d3d10->overlays.count; i++)
   {
      d3d10->device->lpVtbl->PSSetShaderResources(d3d10->device, 0, 1, &d3d10->overlays.textures[i].view);
      d3d10->device->lpVtbl->Draw(d3d10->device, 1, i);
   }
}
#endif

static void d3d10_set_filtering(void* data, unsigned index, bool smooth, bool ctx_scaling)
{
   int i;
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (smooth)
   {
      for (i = 0; i < RARCH_WRAP_MAX; i++)
         d3d10->samplers[RARCH_FILTER_UNSPEC][i] = d3d10->samplers[RARCH_FILTER_LINEAR][i];
   }
   else
   {
      for (i = 0; i < RARCH_WRAP_MAX; i++)
         d3d10->samplers[RARCH_FILTER_UNSPEC][i] = d3d10->samplers[RARCH_FILTER_NEAREST][i];
   }
}

static void d3d10_gfx_set_rotation(void* data, unsigned rotation)
{
   static math_matrix_4x4 rot     = {
      { 0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    1.0f }
   };
   float radians, cosine, sine;
   void* mapped_ubo      = NULL;
   d3d10_video_t*  d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   radians                 = rotation * (M_PI / 2.0f);
   cosine                  = cosf(radians);
   sine                    = sinf(radians);
   MAT_ELEM_4X4(rot, 0, 0) = cosine;
   MAT_ELEM_4X4(rot, 0, 1) = -sine;
   MAT_ELEM_4X4(rot, 1, 0) = sine;
   MAT_ELEM_4X4(rot, 1, 1) = cosine;
   matrix_4x4_multiply(d3d10->mvp, rot, d3d10->ubo_values.mvp);

   d3d10->frame.ubo->lpVtbl->Map(d3d10->frame.ubo,
         D3D10_MAP_WRITE_DISCARD, 0, (void**)&mapped_ubo);
   *(math_matrix_4x4*)mapped_ubo = d3d10->mvp;
   d3d10->frame.ubo->lpVtbl->Unmap(d3d10->frame.ubo);
}

static void d3d10_update_viewport(d3d10_video_t *d3d10, bool force_full)
{
   video_driver_update_viewport(&d3d10->vp, force_full,
         (d3d10->flags & D3D10_ST_FLAG_KEEP_ASPECT) ? true : false);

   d3d10->frame.viewport.TopLeftX  = d3d10->vp.x;
   d3d10->frame.viewport.TopLeftY  = d3d10->vp.y;
   d3d10->frame.viewport.Width     = d3d10->vp.width;
   d3d10->frame.viewport.Height    = d3d10->vp.height;
   d3d10->frame.viewport.MaxDepth  = 0.0f;
   d3d10->frame.viewport.MaxDepth  = 1.0f;

   if (d3d10->shader_preset && (d3d10->frame.output_size.x != d3d10->vp.width
            || d3d10->frame.output_size.y != d3d10->vp.height))
      d3d10->flags                |= D3D10_ST_FLAG_RESIZE_RTS;

   d3d10->frame.output_size.x      = d3d10->vp.width;
   d3d10->frame.output_size.y      = d3d10->vp.height;
   d3d10->frame.output_size.z      = 1.0f / d3d10->vp.width;
   d3d10->frame.output_size.w      = 1.0f / d3d10->vp.height;
   d3d10->flags                   &= ~D3D10_ST_FLAG_RESIZE_VIEWPORT;
}

static void d3d10_free_shader_preset(d3d10_video_t* d3d10)
{
   size_t i;
   if (!d3d10->shader_preset)
      return;

   for (i = 0; i < d3d10->shader_preset->passes; i++)
   {
      int j;

      free(d3d10->shader_preset->pass[i].source.string.vertex);
      free(d3d10->shader_preset->pass[i].source.string.fragment);
      free(d3d10->pass[i].semantics.textures);
      d3d10->shader_preset->pass[i].source.string.vertex   = NULL;
      d3d10->shader_preset->pass[i].source.string.fragment = NULL;
      d3d10->pass[i].semantics.textures                    = NULL;
      d3d10_release_shader(&d3d10->pass[i].shader);
      d3d10_release_texture(&d3d10->pass[i].rt);
      d3d10_release_texture(&d3d10->pass[i].feedback);

      for (j = 0; j < SLANG_CBUFFER_MAX; j++)
      {
         free(d3d10->pass[i].semantics.cbuffers[j].uniforms);
         Release(d3d10->pass[i].buffers[j]);
      }
   }

   memset(d3d10->pass, 0, sizeof(d3d10->pass));

   /* only free the history textures here */
   for (i = 1; i <= (unsigned)d3d10->shader_preset->history_size; i++)
      d3d10_release_texture(&d3d10->frame.texture[i]);

   memset(
         &d3d10->frame.texture[1], 0,
         sizeof(d3d10->frame.texture[1]) * d3d10->shader_preset->history_size);

   for (i = 0; i < d3d10->shader_preset->luts; i++)
      d3d10_release_texture(&d3d10->luts[i]);

   memset(d3d10->luts, 0, sizeof(d3d10->luts));

   free(d3d10->shader_preset);
   d3d10->shader_preset         = NULL;
   d3d10->flags                &= ~(D3D10_ST_FLAG_INIT_HISTORY
                                  | D3D10_ST_FLAG_RESIZE_RTS);
}

static bool d3d10_gfx_set_shader(void* data,
      enum rarch_shader_type type, const char* path)
{
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   unsigned         i;
   d3d10_texture_t* source = NULL;
   d3d10_video_t*   d3d10  = (d3d10_video_t*)data;

   if (!d3d10)
      return false;

   d3d10->device->lpVtbl->Flush(d3d10->device);
   d3d10_free_shader_preset(d3d10);

   if (string_is_empty(path))
      return true;

   if (type != RARCH_SHADER_SLANG)
   {
      RARCH_WARN("[D3D10]: Only Slang shaders are supported. Falling back to stock.\n");
      return false;
   }

   d3d10->shader_preset = (struct video_shader*)calloc(1, sizeof(*d3d10->shader_preset));

   if (!video_shader_load_preset_into_shader(path, d3d10->shader_preset))
      goto error;

   source = &d3d10->frame.texture[0];
   for (i = 0; i < d3d10->shader_preset->passes; source = &d3d10->pass[i++].rt)
   {
      unsigned j;
      /* clang-format off */
      semantics_map_t semantics_map = {
         {
            /* Original */
            { &d3d10->frame.texture[0].view, 0,
               &d3d10->frame.texture[0].size_data, 0},

            /* Source */
            { &source->view, 0,
               &source->size_data, 0},

            /* OriginalHistory */
            { &d3d10->frame.texture[0].view, sizeof(*d3d10->frame.texture),
               &d3d10->frame.texture[0].size_data, sizeof(*d3d10->frame.texture)},

            /* PassOutput */
            { &d3d10->pass[0].rt.view, sizeof(*d3d10->pass),
               &d3d10->pass[0].rt.size_data, sizeof(*d3d10->pass)},

            /* PassFeedback */
            { &d3d10->pass[0].feedback.view, sizeof(*d3d10->pass),
               &d3d10->pass[0].feedback.size_data, sizeof(*d3d10->pass)},

            /* User */
            { &d3d10->luts[0].view, sizeof(*d3d10->luts),
               &d3d10->luts[0].size_data, sizeof(*d3d10->luts)},
         },
         {
            &d3d10->mvp,                     /* MVP */
            &d3d10->pass[i].rt.size_data,    /* OutputSize */
            &d3d10->frame.output_size,       /* FinalViewportSize */
            &d3d10->pass[i].frame_count,     /* FrameCount */
            &d3d10->pass[i].frame_direction, /* FrameDirection */
            &d3d10->pass[i].frame_time_delta,/* FrameTimeDelta */
            &d3d10->pass[i].original_fps,    /* OriginalFPS */
            &d3d10->pass[i].rotation,        /* Rotation */
            &d3d10->pass[i].core_aspect,     /* OriginalAspect */
            &d3d10->pass[i].core_aspect_rot, /* OriginalAspectRotated */
            &d3d10->pass[i].total_subframes, /* TotalSubFrames */
            &d3d10->pass[i].current_subframe,/* CurrentSubFrame */
         }
      };
      /* clang-format on */

      if (!slang_process(
               d3d10->shader_preset, i, RARCH_SHADER_HLSL,
               40, &semantics_map,
               &d3d10->pass[i].semantics))
         goto error;

      {
         static const D3D10_INPUT_ELEMENT_DESC desc[] = {
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d10_vertex_t, position),
               D3D10_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d10_vertex_t, texcoord),
               D3D10_INPUT_PER_VERTEX_DATA, 0 },
         };
         char _path[PATH_MAX_LENGTH];
         const char *slang_path = d3d10->shader_preset->pass[i].source.path;
         const char *vs_src     = d3d10->shader_preset->pass[i].source.string.vertex;
         const char *ps_src     = d3d10->shader_preset->pass[i].source.string.fragment;
         size_t _len            = strlcpy(_path, slang_path, sizeof(_path));
         strlcpy(_path + _len, ".vs.hlsl", sizeof(_path) - _len);

         d3d10_init_shader(d3d10->device, vs_src, 0,
               _path, "main", NULL, NULL, desc,
               countof(desc), &d3d10->pass[i].shader);

         strlcpy(_path + _len, ".ps.hlsl", sizeof(_path) - _len);

         d3d10_init_shader(d3d10->device, ps_src,
               0, _path, NULL, "main",
               NULL, NULL, 0,
               &d3d10->pass[i].shader);

         free(d3d10->shader_preset->pass[i].source.string.vertex);
         free(d3d10->shader_preset->pass[i].source.string.fragment);

         d3d10->shader_preset->pass[i].source.string.vertex   = NULL;
         d3d10->shader_preset->pass[i].source.string.fragment = NULL;

         if (!d3d10->pass[i].shader.vs || !d3d10->pass[i].shader.ps)
            goto error;
      }

      for (j = 0; j < SLANG_CBUFFER_MAX; j++)
      {
         D3D10_BUFFER_DESC desc;
         desc.ByteWidth           = d3d10->pass[i].semantics.cbuffers[j].size;
         desc.Usage               = D3D10_USAGE_DYNAMIC;
         desc.BindFlags           = D3D10_BIND_CONSTANT_BUFFER;
         desc.CPUAccessFlags      = D3D10_CPU_ACCESS_WRITE;
         desc.MiscFlags           = 0;

         if (!desc.ByteWidth)
            continue;

         d3d10->device->lpVtbl->CreateBuffer(d3d10->device, &desc,
               NULL, &d3d10->pass[i].buffers[j]);
      }
   }

   for (i = 0; i < d3d10->shader_preset->luts; i++)
   {
      struct texture_image image;
      image.pixels               = NULL;
      image.width                = 0;
      image.height               = 0;
      image.supports_rgba        = true;

      if (!image_texture_load(&image, d3d10->shader_preset->lut[i].path))
         goto error;

      d3d10->luts[i].desc.Width  = image.width;
      d3d10->luts[i].desc.Height = image.height;
      d3d10->luts[i].desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

      if (d3d10->shader_preset->lut[i].mipmap)
         d3d10->luts[i].desc.MiscFlags = D3D10_RESOURCE_MISC_GENERATE_MIPS;

      d3d10_release_texture(&d3d10->luts[i]);
      d3d10_init_texture(d3d10->device, &d3d10->luts[i]);

      if (d3d10->luts[i].staging)
         d3d10_update_texture(
               d3d10->device,
               image.width, image.height, 0,
               DXGI_FORMAT_R8G8B8A8_UNORM, image.pixels,
               &d3d10->luts[i]);

      image_texture_free(&image);
   }

   d3d10->flags                |=  (D3D10_ST_FLAG_INIT_HISTORY
                                |   D3D10_ST_FLAG_RESIZE_RTS);

   return true;

error:
   d3d10_free_shader_preset(d3d10);
#endif

   return false;
}

static void d3d10_gfx_free(void* data)
{
   int i;
#if 0
   video_driver_state_t *video_st = video_state_get_ptr();
   uint32_t video_st_flags;
#endif
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return;

#ifdef HAVE_OVERLAY
   d3d10_free_overlays(d3d10);
#endif

   d3d10_free_shader_preset(d3d10);

   d3d10_release_texture(&d3d10->frame.texture[0]);
   Release(d3d10->frame.ubo);
   Release(d3d10->frame.vbo);

   d3d10_release_texture(&d3d10->menu.texture);
   Release(d3d10->menu.vbo);

   d3d10_release_shader(&d3d10->sprites.shader);
   d3d10_release_shader(&d3d10->sprites.shader_font);
   Release(d3d10->sprites.vbo);

   for (i = 0; i < GFX_MAX_SHADERS; i++)
      d3d10_release_shader(&d3d10->shaders[i]);

   Release(d3d10->menu_pipeline_vbo);
   Release(d3d10->blend_pipeline);

   Release(d3d10->ubo);

   Release(d3d10->blend_enable);
   Release(d3d10->blend_disable);

   for (i = 0; i < RARCH_WRAP_MAX; i++)
   {
      Release(d3d10->samplers[RARCH_FILTER_LINEAR][i]);
      Release(d3d10->samplers[RARCH_FILTER_NEAREST][i]);
   }

   Release(d3d10->state);
   Release(d3d10->renderTargetView);
   Release(d3d10->swapChain);

   font_driver_free_osd();

#if 0
   video_st_flags = video_st->flags;
   if (video_st_flags & VIDEO_FLAG_CACHE_CONTEXT)
   {
      cached_device_d3d10 = d3d10->device;
      cached_context      = d3d10->context;
   }
   else
#endif
   {
      Release(d3d10->device);
   }

   for (i = 0; i < D3D10_MAX_GPU_COUNT; i++)
   {
      if (d3d10->adapters[i])
      {
         Release(d3d10->adapters[i]);
         d3d10->adapters[i] = NULL;
      }
   }

#ifdef HAVE_MONITOR
   win32_monitor_from_window();
#endif
#ifdef HAVE_WINDOW
   win32_destroy_window();
#endif
   free(d3d10);
}

static bool d3d10_init_swapchain(d3d10_video_t *d3d10,
      int width, int height, void *corewindow)
{
   UINT                 flags              = 0;
   DXGI_SWAP_CHAIN_DESC desc               = {{0}};

   desc.BufferCount                        = 1;
   desc.BufferDesc.Width                   = width;
   desc.BufferDesc.Height                  = height;
   desc.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
   desc.BufferDesc.RefreshRate.Numerator   = 60;
   desc.BufferDesc.RefreshRate.Denominator = 1;
   desc.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
#ifdef HAVE_WINDOW
   desc.OutputWindow                       = (HWND)corewindow;
#endif
   desc.SampleDesc.Count                   = 1;
   desc.SampleDesc.Quality                 = 0;
   desc.Windowed                           = TRUE;
   desc.SwapEffect                         = DXGI_SWAP_EFFECT_SEQUENTIAL;

#ifdef DEBUG
   flags                                  |= D3D10_CREATE_DEVICE_DEBUG;
#endif

   if (FAILED(D3D10CreateDeviceAndSwapChain(
               (IDXGIAdapter*)d3d10->adapter, D3D10_DRIVER_TYPE_HARDWARE,
               NULL, flags, D3D10_SDK_VERSION, &desc,
               (IDXGISwapChain**)&d3d10->swapChain, &d3d10->device)))
      return false;
   return true;
}

static void *d3d10_gfx_init(const video_info_t* video,
      input_driver_t** input, void** input_data)
{
   unsigned i;
#ifdef HAVE_MONITOR
   MONITORINFOEX   current_mon;
   HMONITOR        hm_to_use;
   WNDCLASSEX      wndclass = { 0 };
#endif
   settings_t*     settings = config_get_ptr();
   d3d10_video_t*  d3d10    = (d3d10_video_t*)calloc(1, sizeof(*d3d10));

   if (!d3d10)
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

   win32_monitor_info(&current_mon, &hm_to_use, &d3d10->cur_mon_id);
#endif

   d3d10->vp.full_width  = video->width;
   d3d10->vp.full_height = video->height;

#ifdef HAVE_MONITOR
   if (!d3d10->vp.full_width)
      d3d10->vp.full_width =
         current_mon.rcMonitor.right - current_mon.rcMonitor.left;
   if (!d3d10->vp.full_height)
      d3d10->vp.full_height =
         current_mon.rcMonitor.bottom - current_mon.rcMonitor.top;
#endif

   if (!win32_set_video_mode(d3d10,
            d3d10->vp.full_width, d3d10->vp.full_height, video->fullscreen))
   {
      RARCH_ERR("[D3D10]: win32_set_video_mode failed.\n");
      goto error;
   }

   d3d_input_driver(settings->arrays.input_driver, settings->arrays.input_joypad_driver, input, input_data);

   if (!d3d10_init_swapchain(d3d10,
            d3d10->vp.full_width,
            d3d10->vp.full_height,
#ifdef HAVE_WINDOW
            main_window.hwnd
#else
            NULL
#endif
            ))
      goto error;

   {
      D3D10Texture2D backBuffer;
      d3d10->swapChain->lpVtbl->GetBuffer(d3d10->swapChain, 0,
            uuidof(ID3D10Texture2D), (void**)&backBuffer);
      d3d10->device->lpVtbl->CreateRenderTargetView(d3d10->device,
            (D3D10Resource)backBuffer, NULL, &d3d10->renderTargetView);
      Release(backBuffer);
   }

   d3d10->device->lpVtbl->OMSetRenderTargets(d3d10->device, 1,
         &d3d10->renderTargetView, NULL);

   video_driver_set_size(d3d10->vp.full_width, d3d10->vp.full_height);
   d3d10->viewport.Width  = d3d10->vp.full_width;
   d3d10->viewport.Height = d3d10->vp.full_height;
   d3d10->flags          |= D3D10_ST_FLAG_RESIZE_VIEWPORT;
   if (video->force_aspect)
      d3d10->flags       |=  D3D10_ST_FLAG_KEEP_ASPECT;
   else
      d3d10->flags       &= ~D3D10_ST_FLAG_KEEP_ASPECT;
   if (video->vsync)
      d3d10->flags       |=  D3D10_ST_FLAG_VSYNC;
   else
      d3d10->flags       &= ~D3D10_ST_FLAG_VSYNC;
   d3d10->format          = video->rgb32 ?
      DXGI_FORMAT_B8G8R8X8_UNORM : DXGI_FORMAT_B5G6R5_UNORM;

   d3d10->frame.texture[0].desc.Format = d3d10->format;
   d3d10->frame.texture[0].desc.Usage  = D3D10_USAGE_DEFAULT;
   d3d10->frame.texture[0].desc.Width  = 4;
   d3d10->frame.texture[0].desc.Height = 4;

   d3d10_release_texture(&d3d10->frame.texture[0]);
   d3d10_init_texture(d3d10->device, &d3d10->frame.texture[0]);

   d3d10->menu.texture.desc.Usage  = D3D10_USAGE_DEFAULT;

   matrix_4x4_ortho(d3d10->ubo_values.mvp, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);

   d3d10->ubo_values.OutputSize.width  = d3d10->viewport.Width;
   d3d10->ubo_values.OutputSize.height = d3d10->viewport.Height;

   {
      D3D10_SUBRESOURCE_DATA ubo_data;
      D3D10_BUFFER_DESC desc;

      desc.ByteWidth            = sizeof(d3d10->ubo_values);
      desc.Usage                = D3D10_USAGE_DYNAMIC;
      desc.BindFlags            = D3D10_BIND_CONSTANT_BUFFER;
      desc.CPUAccessFlags       = D3D10_CPU_ACCESS_WRITE;
      desc.MiscFlags            = 0;

      ubo_data.pSysMem          = &d3d10->ubo_values.mvp;
      ubo_data.SysMemPitch      = 0;
      ubo_data.SysMemSlicePitch = 0;

      d3d10->device->lpVtbl->CreateBuffer(d3d10->device, &desc,
            &ubo_data, &d3d10->ubo);
      d3d10->device->lpVtbl->CreateBuffer(d3d10->device, &desc,
            NULL, &d3d10->frame.ubo);
   }

   d3d10_gfx_set_rotation(d3d10, 0);

   {
      D3D10_SAMPLER_DESC desc = { D3D10_FILTER_MIN_MAG_MIP_POINT };
      desc.MaxAnisotropy      = 1;
      desc.ComparisonFunc     = D3D10_COMPARISON_NEVER;
      desc.MinLOD             = -D3D10_FLOAT32_MAX;
      desc.MaxLOD             = D3D10_FLOAT32_MAX;

      /* Initialize samplers */
      for (i = 0; i < RARCH_WRAP_MAX; i++)
      {
         switch (i)
         {
            case RARCH_WRAP_BORDER:
               desc.AddressU = D3D10_TEXTURE_ADDRESS_BORDER;
               break;

            case RARCH_WRAP_EDGE:
               desc.AddressU = D3D10_TEXTURE_ADDRESS_CLAMP;
               break;

            case RARCH_WRAP_REPEAT:
               desc.AddressU = D3D10_TEXTURE_ADDRESS_WRAP;
               break;

            case RARCH_WRAP_MIRRORED_REPEAT:
               desc.AddressU = D3D10_TEXTURE_ADDRESS_MIRROR;
               break;
         }
         desc.AddressV = desc.AddressU;
         desc.AddressW = desc.AddressU;

         desc.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
         d3d10->device->lpVtbl->CreateSamplerState(d3d10->device, &desc,
               &d3d10->samplers[RARCH_FILTER_LINEAR][i]);

         desc.Filter = D3D10_FILTER_MIN_MAG_MIP_POINT;
         d3d10->device->lpVtbl->CreateSamplerState(d3d10->device, &desc,
               &d3d10->samplers[RARCH_FILTER_NEAREST][i]);
      }
   }

   d3d10_set_filtering(d3d10, 0, video->smooth, video->ctx_scaling);

   {
      D3D10_BUFFER_DESC desc;
      D3D10_SUBRESOURCE_DATA vertex_data;
      d3d10_vertex_t vertices[]    = {
         { { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
         { { 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
         { { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
         { { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
      };
      vertex_data.pSysMem          = vertices;
      vertex_data.SysMemPitch      = 0;
      vertex_data.SysMemSlicePitch = 0;

      desc.ByteWidth               = sizeof(vertices);
      desc.Usage                   = D3D10_USAGE_IMMUTABLE;
      desc.BindFlags               = D3D10_BIND_VERTEX_BUFFER;
      desc.CPUAccessFlags          = 0;
      desc.MiscFlags               = 0;

      d3d10->device->lpVtbl->CreateBuffer(d3d10->device, &desc,
            &vertex_data, &d3d10->frame.vbo);
      desc.Usage                   = D3D10_USAGE_DYNAMIC;
      desc.CPUAccessFlags          = D3D10_CPU_ACCESS_WRITE;
      d3d10->device->lpVtbl->CreateBuffer(d3d10->device, &desc,
            &vertex_data, &d3d10->menu.vbo);

      d3d10->sprites.capacity  = 16 * 1024;
      desc.ByteWidth           = sizeof(d3d10_sprite_t) * d3d10->sprites.capacity;
      d3d10->device->lpVtbl->CreateBuffer(d3d10->device, &desc,
            NULL, &d3d10->sprites.vbo);
   }

   {
      D3D10_INPUT_ELEMENT_DESC desc[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d10_vertex_t, position),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d10_vertex_t, texcoord),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
         { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d10_vertex_t, color),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
      };

      static const char shader[] =
#include "d3d_shaders/opaque_sm5.hlsl.h"
         ;

      if (!d3d10_init_shader(
               d3d10->device, shader, sizeof(shader), NULL, "VSMain", "PSMain", NULL, desc,
               countof(desc), &d3d10->shaders[VIDEO_SHADER_STOCK_BLEND]))
         goto error;
   }

   {
      D3D10_INPUT_ELEMENT_DESC desc[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d10_sprite_t, pos),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d10_sprite_t, coords),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
         { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d10_sprite_t, colors[0]),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
         { "COLOR", 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d10_sprite_t, colors[1]),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
         { "COLOR", 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d10_sprite_t, colors[2]),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
         { "COLOR", 3, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d10_sprite_t, colors[3]),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
         { "PARAMS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d10_sprite_t, params),
            D3D10_INPUT_PER_VERTEX_DATA, 0 },
      };

      static const char shader[] =
#include "d3d_shaders/sprite_sm4.hlsl.h"
         ;

      if (!d3d10_init_shader(
               d3d10->device, shader, sizeof(shader), NULL, "VSMain", "PSMain", "GSMain", desc,
               countof(desc), &d3d10->sprites.shader))
         goto error;
      if (!d3d10_init_shader(
               d3d10->device, shader, sizeof(shader), NULL, "VSMain", "PSMainA8", "GSMain", desc,
               countof(desc), &d3d10->sprites.shader_font))
         goto error;
   }

   if (string_is_equal(settings->arrays.menu_driver, "xmb"))
   {
      {
         D3D10_INPUT_ELEMENT_DESC desc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
         };

         static const char ribbon[] =
#include "d3d_shaders/ribbon_sm4.hlsl.h"
            ;
         static const char ribbon_simple[] =
#include "d3d_shaders/ribbon_simple_sm4.hlsl.h"
            ;

         if (!d3d10_init_shader(
                  d3d10->device, ribbon, sizeof(ribbon), NULL, "VSMain", "PSMain", NULL, desc,
                  countof(desc), &d3d10->shaders[VIDEO_SHADER_MENU]))
            goto error;

         if (!d3d10_init_shader(
                  d3d10->device, ribbon_simple, sizeof(ribbon_simple), NULL, "VSMain", "PSMain", NULL,
                  desc, countof(desc), &d3d10->shaders[VIDEO_SHADER_MENU_2]))
            goto error;
      }

      {
         D3D10_INPUT_ELEMENT_DESC desc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d10_vertex_t, position),
               D3D10_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d10_vertex_t, texcoord),
               D3D10_INPUT_PER_VERTEX_DATA, 0 },
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

         if (!d3d10_init_shader(
                  d3d10->device, simple_snow, sizeof(simple_snow), NULL, "VSMain", "PSMain", NULL,
                  desc, countof(desc), &d3d10->shaders[VIDEO_SHADER_MENU_3]))
            goto error;
         if (!d3d10_init_shader(
                  d3d10->device, snow, sizeof(snow), NULL, "VSMain", "PSMain", NULL, desc,
                  countof(desc), &d3d10->shaders[VIDEO_SHADER_MENU_4]))
            goto error;

         if (!d3d10_init_shader(
                  d3d10->device, bokeh, sizeof(bokeh), NULL, "VSMain", "PSMain", NULL, desc,
                  countof(desc), &d3d10->shaders[VIDEO_SHADER_MENU_5]))
            goto error;

         if (!d3d10_init_shader(
                  d3d10->device, snowflake, sizeof(snowflake), NULL, "VSMain", "PSMain", NULL, desc,
                  countof(desc), &d3d10->shaders[VIDEO_SHADER_MENU_6]))
            goto error;
      }
   }

   {
      D3D10_BLEND_DESC blend_desc = { 0 };

      blend_desc.AlphaToCoverageEnable    = FALSE;
      blend_desc.BlendEnable[0]           = TRUE;
      blend_desc.SrcBlend                 = D3D10_BLEND_SRC_ALPHA;
      blend_desc.DestBlend                = D3D10_BLEND_INV_SRC_ALPHA;
      blend_desc.BlendOp                  = D3D10_BLEND_OP_ADD;
      blend_desc.SrcBlendAlpha            = D3D10_BLEND_SRC_ALPHA;
      blend_desc.DestBlendAlpha           = D3D10_BLEND_INV_SRC_ALPHA;
      blend_desc.BlendOpAlpha             = D3D10_BLEND_OP_ADD;
      blend_desc.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;
      d3d10->device->lpVtbl->CreateBlendState(d3d10->device, &blend_desc,
            &d3d10->blend_enable);

      blend_desc.SrcBlend  = D3D10_BLEND_DEST_COLOR;
      blend_desc.DestBlend = D3D10_BLEND_ONE;
      d3d10->device->lpVtbl->CreateBlendState(d3d10->device, &blend_desc,
            &d3d10->blend_pipeline);

      blend_desc.BlendEnable[0] = FALSE;
      d3d10->device->lpVtbl->CreateBlendState(d3d10->device, &blend_desc,
            &d3d10->blend_disable);
   }

   {
      D3D10_RASTERIZER_DESC desc;

      desc.FillMode              = D3D10_FILL_SOLID;
      desc.CullMode              = D3D10_CULL_NONE;
      desc.FrontCounterClockwise = FALSE;
      desc.DepthBias             = 0;
      desc.DepthBiasClamp        = 0.0f;
      desc.SlopeScaledDepthBias  = 0.0f;
      desc.DepthClipEnable       = FALSE;
      desc.ScissorEnable         = TRUE;
      desc.MultisampleEnable     = FALSE;
      desc.AntialiasedLineEnable = FALSE;

      d3d10->device->lpVtbl->CreateRasterizerState(d3d10->device, &desc,
            &d3d10->state);
   }

   d3d10->device->lpVtbl->RSSetState(d3d10->device, d3d10->state);

   font_driver_init_osd(d3d10,
         video,
         false,
         video->is_threaded,
         FONT_DRIVER_RENDER_D3D10_API);

   {
      d3d10_fake_context.get_flags = d3d10_get_flags;
      d3d10_fake_context.get_metrics = win32_get_metrics;
      video_context_driver_set(&d3d10_fake_context);
#ifdef HAVE_SLANG
      const char *shader_preset   = video_shader_get_current_shader_preset();
      enum rarch_shader_type type = video_shader_parse_type(shader_preset);
      d3d10_gfx_set_shader(d3d10, type, shader_preset);
#endif
   }

#if 0
   if (video_driver_get_hw_context()->context_type == RETRO_HW_CONTEXT_D3D10)
   {
      d3d10->hw.enable                  = true;
      d3d10->hw.iface.interface_type    = RETRO_HW_RENDER_INTERFACE_D3D10;
      d3d10->hw.iface.interface_version = RETRO_HW_RENDER_INTERFACE_D3D10_VERSION;
      d3d10->hw.iface.handle            = d3d10;
      d3d10->hw.iface.device            = d3d10->device;
      d3d10->hw.iface.context           = d3d10->context;
      d3d10->hw.iface.featureLevel      = d3d10->supportedFeatureLevel;
      d3d10->hw.iface.D3DCompile        = D3DCompile;
   }
#endif

#ifdef __WINRT__
   DXGICreateFactory2(&d3d10->factory);
#else
   DXGICreateFactory1(&d3d10->factory);
#endif

   {
      int         i = 0;
      int gpu_index = settings->ints.d3d10_gpu_index;

      if (d3d10->gpu_list)
         string_list_free(d3d10->gpu_list);

      d3d10->gpu_list = string_list_new();

      for (;;)
      {
         DXGI_ADAPTER_DESC desc = {0};
         union string_list_elem_attr attr = {0};
         char str[128];

         str[0] = '\0';

#ifdef __WINRT__
         if (FAILED(DXGIEnumAdapters2(d3d10->factory, i, &d3d10->adapter)))
            break;
#else
         if (FAILED(DXGIEnumAdapters1(d3d10->factory, i, &d3d10->adapter)))
            break;
#endif

         IDXGIAdapter_GetDesc(d3d10->adapter, &desc);

         utf16_to_char_string((const uint16_t*)
               desc.Description, str, sizeof(str));

         RARCH_LOG("[D3D10]: Found GPU at index %d: \"%s\".\n", i, str);

         string_list_append(d3d10->gpu_list, str, attr);

         if (i < D3D10_MAX_GPU_COUNT)
            d3d10->adapters[i] = d3d10->adapter;

         i++;
      }

      video_driver_set_gpu_api_devices(GFX_CTX_DIRECT3D10_API, d3d10->gpu_list);

      if (0 <= gpu_index && gpu_index <= i && (gpu_index < D3D10_MAX_GPU_COUNT))
      {
         d3d10->current_adapter = d3d10->adapters[gpu_index];
         d3d10->adapter         = d3d10->current_adapter;
         RARCH_LOG("[D3D10]: Using GPU index %d.\n", gpu_index);
      }
      else
      {
         RARCH_WARN("[D3D10]: Invalid GPU index %d, using first device found.\n", gpu_index);
         d3d10->current_adapter = d3d10->adapters[0];
         d3d10->adapter         = d3d10->current_adapter;
      }
   }

   return d3d10;

error:
   d3d10_gfx_free(d3d10);
   return NULL;
}

static void d3d10_init_history(d3d10_video_t* d3d10,
      unsigned width, unsigned height)
{
   int i;

   /* TODO/FIXME: should we init history to max_width/max_height instead ?
    * to prevent out of memory errors happening several frames later
    * and to reduce memory fragmentation */
   for (i = 0; i < (int)d3d10->shader_preset->history_size + 1; i++)
   {
      d3d10->frame.texture[i].desc.Width  = width;
      d3d10->frame.texture[i].desc.Height = height;
      d3d10->frame.texture[i].desc.Format = d3d10->frame.texture[0].desc.Format;
      d3d10->frame.texture[i].desc.Usage  = d3d10->frame.texture[0].desc.Usage;
      d3d10_release_texture(&d3d10->frame.texture[i]);
      d3d10_init_texture(d3d10->device, &d3d10->frame.texture[i]);
      /* TODO/FIXME: clear texture ?  */
   }
   d3d10->flags &= ~D3D10_ST_FLAG_INIT_HISTORY;
}

static void d3d10_init_render_targets(d3d10_video_t* d3d10,
      unsigned width, unsigned height)
{
   size_t i;
   for (i = 0; i < d3d10->shader_preset->passes; i++)
   {
      struct video_shader_pass* pass = &d3d10->shader_preset->pass[i];

      if (pass->fbo.flags & FBO_SCALE_FLAG_VALID)
      {
         switch (pass->fbo.type_x)
         {
            case RARCH_SCALE_INPUT:
               width *= pass->fbo.scale_x;
               break;

            case RARCH_SCALE_VIEWPORT:
               width = d3d10->vp.width * pass->fbo.scale_x;
               break;

            case RARCH_SCALE_ABSOLUTE:
               width = pass->fbo.abs_x;
               break;

            default:
               break;
         }

         if (!width)
            width = d3d10->vp.width;

         switch (pass->fbo.type_y)
         {
            case RARCH_SCALE_INPUT:
               height *= pass->fbo.scale_y;
               break;

            case RARCH_SCALE_VIEWPORT:
               height = d3d10->vp.height * pass->fbo.scale_y;
               break;

            case RARCH_SCALE_ABSOLUTE:
               height = pass->fbo.abs_y;
               break;

            default:
               break;
         }

         if (!height)
            height = d3d10->vp.height;
      }
      else if (i == (d3d10->shader_preset->passes - 1))
      {
         width  = d3d10->vp.width;
         height = d3d10->vp.height;
      }

      RARCH_LOG("[D3D10]: Updating framebuffer size %ux%u.\n", width, height);

      if (     (i != (d3d10->shader_preset->passes - 1))
            || (width  != d3d10->vp.width)
            || (height != d3d10->vp.height))
      {
         d3d10->pass[i].viewport.Width    = width;
         d3d10->pass[i].viewport.Height   = height;
         d3d10->pass[i].viewport.MaxDepth = 1.0;
         d3d10->pass[i].rt.desc.Width     = width;
         d3d10->pass[i].rt.desc.Height    = height;
         d3d10->pass[i].rt.desc.BindFlags = D3D10_BIND_RENDER_TARGET;
         d3d10->pass[i].rt.desc.Format    = glslang_format_to_dxgi(
               d3d10->pass[i].semantics.format);
         d3d10_release_texture(&d3d10->pass[i].rt);
         d3d10_init_texture(d3d10->device, &d3d10->pass[i].rt);

         if (pass->feedback)
         {
            d3d10->pass[i].feedback.desc = d3d10->pass[i].rt.desc;
            d3d10_release_texture(&d3d10->pass[i].feedback);
            d3d10_init_texture(d3d10->device, &d3d10->pass[i].feedback);
            /* TODO/FIXME: Do we need to clear it to black here ? */
         }
      }
      else
      {
         d3d10->pass[i].rt.size_data.x = width;
         d3d10->pass[i].rt.size_data.y = height;
         d3d10->pass[i].rt.size_data.z = 1.0f / width;
         d3d10->pass[i].rt.size_data.w = 1.0f / height;
      }
   }

   d3d10->flags &= ~D3D10_ST_FLAG_RESIZE_RTS;
}

static bool d3d10_gfx_frame(
      void*               data,
      const void*         frame,
      unsigned            width,
      unsigned            height,
      uint64_t            frame_count,
      unsigned            pitch,
      const char*         msg,
      video_frame_info_t* video_info)
{
   unsigned           i, k, m;
   UINT offset = 0, stride    = 0;
   d3d10_texture_t*   texture = NULL;
   d3d10_video_t      * d3d10 = (d3d10_video_t*)data;
   D3D10Device       context  = d3d10->device;
   unsigned video_width       = video_info->width;
   unsigned video_height      = video_info->height;
   bool statistics_show       = video_info->statistics_show;
   struct font_params
      *osd_params             = (struct font_params*)
      &video_info->osd_stat_params;
   const char *stat_text      = video_info->stat_text;
#ifdef HAVE_MENU
   bool menu_is_alive         = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#else
   bool menu_is_alive         = false;
#endif
   bool overlay_behind_menu   = video_info->overlay_behind_menu;
   unsigned black_frame_insertion = video_info->black_frame_insertion;
   int bfi_light_frames;
   unsigned n;
   bool nonblock_state            = video_info->input_driver_nonblock_state;
   bool runloop_is_slowmotion     = video_info->runloop_is_slowmotion;
   bool runloop_is_paused         = video_info->runloop_is_paused;

#ifdef HAVE_GFX_WIDGETS
   bool widgets_active        = video_info->widgets_active;
#endif

   if (d3d10->flags & D3D10_ST_FLAG_RESIZE_CHAIN)
   {
      D3D10Texture2D backBuffer;

      Release(d3d10->renderTargetView);
      DXGIResizeBuffers(d3d10->swapChain, 0, 0, 0, (DXGI_FORMAT)0, 0);

      d3d10->swapChain->lpVtbl->GetBuffer(d3d10->swapChain, 0,
            uuidof(ID3D10Texture2D), (void**)&backBuffer);
      d3d10->device->lpVtbl->CreateRenderTargetView(d3d10->device,
            (D3D10Resource)backBuffer, NULL, &d3d10->renderTargetView);
      Release(backBuffer);

      d3d10->device->lpVtbl->OMSetRenderTargets(d3d10->device, 1,
            &d3d10->renderTargetView, NULL);
      d3d10->viewport.Width               = video_width;
      d3d10->viewport.Height              = video_height;

      d3d10->ubo_values.OutputSize.width  = d3d10->viewport.Width;
      d3d10->ubo_values.OutputSize.height = d3d10->viewport.Height;

      d3d10->flags                       &= ~D3D10_ST_FLAG_RESIZE_CHAIN;
      d3d10->flags                       |=  D3D10_ST_FLAG_RESIZE_VIEWPORT;

      video_driver_set_size(video_width, video_height);
   }

#if 0
   /* custom viewport doesn't call apply_state_changes,
      so we can't rely on this for now */
   if (d3d10->resize_viewport)
#endif
      d3d10_update_viewport(d3d10, false);

   context->lpVtbl->IASetPrimitiveTopology(context,
         D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

#if 0
   if (d3d10->hw.enable)
   {
      context->lpVtbl->OMSetRenderTargets(context, 1,
            &d3d10->renderTargetView, NULL);
      context->lpVtbl->RSSetState(context, d3d10->state);
   }
#endif

   if (frame && width && height)
   {
      if (d3d10->shader_preset)
      {
         if (     (d3d10->frame.texture[0].desc.Width  != width)
               || (d3d10->frame.texture[0].desc.Height != height))
            d3d10->flags |= D3D10_ST_FLAG_RESIZE_RTS;

         if (d3d10->flags & D3D10_ST_FLAG_RESIZE_RTS)
         {
            int i;
            /* Release all render targets first to avoid
             * memory fragmentation */
            for (i = 0; i < (int) d3d10->shader_preset->passes; i++)
            {
               d3d10_release_texture(&d3d10->pass[i].rt);
               d3d10_release_texture(&d3d10->pass[i].feedback);
               memset(&d3d10->pass[i].rt, 0, sizeof(d3d10->pass[i].rt));
               memset(&d3d10->pass[i].feedback, 0, sizeof(d3d10->pass[i].feedback));
            }
         }

         if (d3d10->shader_preset->history_size)
         {
            if (d3d10->flags & D3D10_ST_FLAG_INIT_HISTORY)
               d3d10_init_history(d3d10, width, height);
            else
            {
               int k;
               /* TODO/FIXME: what about frame-duping ?
                * maybe clone d3d10_texture_t with AddRef */
               d3d10_texture_t tmp = d3d10->frame.texture[d3d10->shader_preset->history_size];
               for (k = d3d10->shader_preset->history_size; k > 0; k--)
                  d3d10->frame.texture[k] = d3d10->frame.texture[k - 1];
               d3d10->frame.texture[0] = tmp;
            }
         }
      }

      /* either no history, or we moved a texture of
       * a different size in the front slot */
      if (     (d3d10->frame.texture[0].desc.Width  != width)
            || (d3d10->frame.texture[0].desc.Height != height))
      {
         d3d10->frame.texture[0].desc.Width  = width;
         d3d10->frame.texture[0].desc.Height = height;
         d3d10_release_texture(&d3d10->frame.texture[0]);
         d3d10_init_texture(d3d10->device, &d3d10->frame.texture[0]);
      }

      if (d3d10->flags & D3D10_ST_FLAG_RESIZE_RTS)
         d3d10_init_render_targets(d3d10, width, height);

      if (frame != RETRO_HW_FRAME_BUFFER_VALID)
         if (d3d10->frame.texture[0].staging)
            d3d10_update_texture(
                  d3d10->device,
                  width, height, pitch, d3d10->format,
                  frame, &d3d10->frame.texture[0]);
   }

   stride = sizeof(d3d10_vertex_t);
   context->lpVtbl->IASetVertexBuffers(
         context, 0, 1, (D3D10Buffer* const)&d3d10->frame.vbo,
         &stride, &offset);
   context->lpVtbl->OMSetBlendState(context, d3d10->blend_disable, NULL,
         D3D10_DEFAULT_SAMPLE_MASK);

   texture = d3d10->frame.texture;

   if (d3d10->shader_preset)
   {
      for (i = 0; i < d3d10->shader_preset->passes; i++)
      {
         if (d3d10->shader_preset->pass[i].feedback)
         {
            d3d10_texture_t tmp     = d3d10->pass[i].feedback;
            d3d10->pass[i].feedback = d3d10->pass[i].rt;
            d3d10->pass[i].rt       = tmp;
         }
      }

      for (i = 0; i < d3d10->shader_preset->passes; i++)
      {
         int j;

         d3d10_set_shader(context, &d3d10->pass[i].shader);

         if (d3d10->shader_preset->pass[i].frame_count_mod)
            d3d10->pass[i].frame_count   =
               frame_count % d3d10->shader_preset->pass[i].frame_count_mod;
         else
            d3d10->pass[i].frame_count   = frame_count;

#ifdef HAVE_REWIND
         d3d10->pass[i].frame_direction  = state_manager_frame_is_reversed()
            ? -1 : 1;
#else
         d3d10->pass[i].frame_direction  = 1;
#endif
         d3d10->pass[i].frame_time_delta = (uint32_t)video_driver_get_frame_time_delta_usec();
         d3d10->pass[i].original_fps     = video_driver_get_original_fps();
         d3d10->pass[i].rotation         = retroarch_get_rotation();
         d3d10->pass[i].core_aspect      = video_driver_get_core_aspect();
         /* OriginalAspectRotated: return 1 / aspect for 90 and 270 rotated content */
         d3d10->pass[i].core_aspect_rot  = video_driver_get_core_aspect();
         uint32_t rot = retroarch_get_rotation();
         if (rot == 1 || rot == 3)
            d3d10->pass[i].core_aspect_rot = 1/d3d10->pass[i].core_aspect_rot;

         /* Sub-frame info for multiframe shaders (per real content frame).
            Should always be 1 for non-use of subframes */
         if (!(d3d10->flags & D3D10_ST_FLAG_FRAME_DUPE_LOCK))
         {
           if (     black_frame_insertion
                 || nonblock_state
                 || runloop_is_slowmotion
                 || runloop_is_paused
                 || (d3d10->flags & D3D10_ST_FLAG_MENU_ENABLE))
              d3d10->pass[i].total_subframes = 1;
           else
              d3d10->pass[i].total_subframes = video_info->shader_subframes;

           d3d10->pass[i].current_subframe = 1;
         }

         for (j = 0; j < SLANG_CBUFFER_MAX; j++)
         {
            D3D10Buffer    buffer     = d3d10->pass[i].buffers[j];
            cbuffer_sem_t* buffer_sem = &d3d10->pass[i].semantics.cbuffers[j];

            if (buffer_sem->stage_mask && buffer_sem->uniforms)
            {
               void*               uniform_data = NULL;
               uniform_sem_t*           uniform = buffer_sem->uniforms;

               buffer->lpVtbl->Map(buffer,
                     D3D10_MAP_WRITE_DISCARD, 0, (void**)&uniform_data);

               while (uniform->size)
               {
                  if (uniform->data)
                     memcpy((uint8_t*)uniform_data + uniform->offset,
                           uniform->data, uniform->size);
                  uniform++;
               }
               buffer->lpVtbl->Unmap(buffer);

               if (buffer_sem->stage_mask & SLANG_STAGE_VERTEX_MASK)
                  context->lpVtbl->VSSetConstantBuffers(context,
                        buffer_sem->binding, 1, &buffer);

               if (buffer_sem->stage_mask & SLANG_STAGE_FRAGMENT_MASK)
                  context->lpVtbl->PSSetConstantBuffers(context,
                        buffer_sem->binding, 1, &buffer);
            }
         }

         {
            D3D10RenderTargetView null_rt = NULL;
            context->lpVtbl->OMSetRenderTargets(context, 1,
                  &null_rt, NULL);
         }

         {
            D3D10ShaderResourceView textures[SLANG_NUM_BINDINGS] = { NULL };
            D3D10SamplerState       samplers[SLANG_NUM_BINDINGS] = { NULL };

            texture_sem_t* texture_sem = d3d10->pass[i].semantics.textures;
            while (texture_sem->stage_mask)
            {
               int binding       = texture_sem->binding;
               textures[binding] = *(D3D10ShaderResourceView*)
                  texture_sem->texture_data;
               samplers[binding] = d3d10->samplers[texture_sem->filter][texture_sem->wrap];
               texture_sem++;
            }

#if 0
            if (d3d10->hw.enable && (i == 0))
               context->lpVtbl->PSSetShaderResources(context, 1,
                     SLANG_NUM_BINDINGS - 1, textures + 1);
            else
#endif
               context->lpVtbl->PSSetShaderResources(context, 0,
                     SLANG_NUM_BINDINGS, textures);

            context->lpVtbl->PSSetSamplers(context, 0,
                  SLANG_NUM_BINDINGS, samplers);
         }

         if (d3d10->pass[i].rt.handle)
         {
            context->lpVtbl->OMSetRenderTargets(context, 1,
                  &d3d10->pass[i].rt.rt_view, NULL);
            context->lpVtbl->RSSetViewports(context, 1,
                  &d3d10->pass[i].viewport);

#ifdef D3D10_ROLLING_SCANLINE_SIMULATION
            if (      (video_info->shader_subframes > 1)
                  &&  (video_info->scan_subframes)
                  &&  !black_frame_insertion
                  &&  !nonblock_state
                  &&  !runloop_is_slowmotion
                  &&  !runloop_is_paused
                  &&  (!(d3d10->flags & D3D10_ST_FLAG_MENU_ENABLE)))
            {
               D3D10_RECT scissor_rect;

               scissor_rect.left   = 0;
               scissor_rect.top    = (unsigned int)(((float)d3d10->pass[i].viewport.Height / (float)video_info->shader_subframes)
                                       * (float)video_info->current_subframe);
               scissor_rect.right  = d3d10->pass[i].viewport.Width ;
               scissor_rect.bottom = (unsigned int)(((float)d3d10->pass[i].viewport.Height / (float)video_info->shader_subframes)
                                       * (float)(video_info->current_subframe + 1));

               d3d10->device->lpVtbl->RSSetScissorRects(d3d10->device, 1,
                     &scissor_rect);
            }
            else
            {
               D3D10_RECT scissor_rect;

               scissor_rect.left   = 0;
               scissor_rect.top    = 0;
               scissor_rect.right  = d3d10->pass[i].viewport.Width;
               scissor_rect.bottom = d3d10->pass[i].viewport.Height;

               d3d10->device->lpVtbl->RSSetScissorRects(d3d10->device, 1,
                     &scissor_rect);
            }
#endif /* D3D10_ROLLING_SCANLINE_SIMULATION */

            context->lpVtbl->Draw(context, 4, 0);
            texture = &d3d10->pass[i].rt;
         }
         else
         {
            texture = NULL;
            break;
         }
      }
      context->lpVtbl->OMSetRenderTargets(context, 1,
            &d3d10->renderTargetView, NULL);
   }

   if (texture)
   {
      d3d10_set_shader(context, &d3d10->shaders[VIDEO_SHADER_STOCK_BLEND]);
#if 0
      /* TODO/FIXME */
      if (!d3d10->hw.enable || d3d10->shader_preset)
#endif
         context->lpVtbl->PSSetShaderResources(context, 0, 1, &texture->view);
      context->lpVtbl->PSSetSamplers(context, 0, 1,
            &d3d10->samplers[RARCH_FILTER_UNSPEC][RARCH_WRAP_DEFAULT]);
      context->lpVtbl->VSSetConstantBuffers(context, 0, 1,
            &d3d10->frame.ubo);
   }

   context->lpVtbl->ClearRenderTargetView(context, d3d10->renderTargetView,
         d3d10->clearcolor);
   context->lpVtbl->RSSetViewports(context, 1, &d3d10->frame.viewport);

#ifdef D3D10_ROLLING_SCANLINE_SIMULATION
   if (      (video_info->shader_subframes > 1)
         &&  (video_info->scan_subframes)
         &&  !black_frame_insertion
         &&  !nonblock_state
         &&  !runloop_is_slowmotion
         &&  !runloop_is_paused
         &&  (!(d3d10->flags & D3D10_ST_FLAG_MENU_ENABLE)))
   {
      D3D10_RECT scissor_rect;

      scissor_rect.left   = 0;
      scissor_rect.top    = (unsigned int)(((float)video_height / (float)video_info->shader_subframes)
                              * (float)video_info->current_subframe);
      scissor_rect.right  = video_width ;
      scissor_rect.bottom = (unsigned int)(((float)video_height / (float)video_info->shader_subframes)
                              * (float)(video_info->current_subframe + 1));

      d3d10->device->lpVtbl->RSSetScissorRects(d3d10->device, 1,
            &scissor_rect);
   }
   else
#endif /* D3D10_ROLLING_SCANLINE_SIMULATION */
   {
      D3D10_RECT scissor_rect;

      scissor_rect.left   = 0;
      scissor_rect.top    = 0;
      scissor_rect.right  = video_width;
      scissor_rect.bottom = video_height;

      d3d10->device->lpVtbl->RSSetScissorRects(d3d10->device, 1,
            &scissor_rect);
   }

   context->lpVtbl->Draw(context, 4, 0);
   context->lpVtbl->OMSetBlendState(context, d3d10->blend_enable, NULL,
         D3D10_DEFAULT_SAMPLE_MASK);

#ifdef D3D10_ROLLING_SCANLINE_SIMULATION
   {
      D3D10_RECT scissor_rect;

      scissor_rect.left   = 0;
      scissor_rect.top    = 0;
      scissor_rect.right  = video_width;
      scissor_rect.bottom = video_height;

      d3d10->device->lpVtbl->RSSetScissorRects(d3d10->device, 1,
            &scissor_rect);
   }
#endif /* D3D10_ROLLING_SCANLINE_SIMULATION */

   if (    (d3d10->flags & D3D10_ST_FLAG_MENU_ENABLE)
         && d3d10->menu.texture.handle)
   {
      UINT offset = 0, stride = sizeof(d3d10_vertex_t);
      if (d3d10->flags & D3D10_ST_FLAG_MENU_FULLSCREEN)
         context->lpVtbl->RSSetViewports(context, 1, &d3d10->viewport);

      d3d10_set_shader(context, &d3d10->shaders[VIDEO_SHADER_STOCK_BLEND]);
      context->lpVtbl->IASetVertexBuffers(
            context, 0, 1, (D3D10Buffer* const)&d3d10->menu.vbo,
            &stride, &offset);
      context->lpVtbl->VSSetConstantBuffers(context, 0, 1, &d3d10->ubo);
      d3d10_set_texture_and_sampler(context, 0, &d3d10->menu.texture);
      context->lpVtbl->Draw(context, 4, 0);
   }

   d3d10_set_shader(context, &d3d10->sprites.shader);
   context->lpVtbl->IASetPrimitiveTopology(context,
         D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);
   context->lpVtbl->VSSetConstantBuffers(context, 0, 1, (ID3D10Buffer **
            const)&d3d10->ubo);
   context->lpVtbl->PSSetConstantBuffers(context, 0, 1, (ID3D10Buffer **
            const)&d3d10->ubo);

   d3d10->flags |= D3D10_ST_FLAG_SPRITES_ENABLE;

#ifdef HAVE_OVERLAY
   if (    (d3d10->flags & D3D10_ST_FLAG_OVERLAYS_ENABLE)
         && overlay_behind_menu)
      d3d10_render_overlay(d3d10);
#endif

#ifdef HAVE_MENU
#ifndef HAVE_GFX_WIDGETS
   if (d3d10->flags & D3D10_ST_FLAG_MENU_ENABLE)
#endif
   {
      UINT offset = 0;
      UINT stride = sizeof(d3d10_sprite_t);
      context->lpVtbl->RSSetViewports(context, 1, &d3d10->viewport);
      context->lpVtbl->IASetVertexBuffers(
            context, 0, 1, (D3D10Buffer* const)&d3d10->sprites.vbo,
            &stride, &offset);
   }
#endif

#ifdef HAVE_MENU
   if (d3d10->flags & D3D10_ST_FLAG_MENU_ENABLE)
      menu_driver_frame(menu_is_alive, video_info);
   else
#endif
      if (statistics_show)
      {
         if (osd_params)
         {
            UINT stride = 0, offset = 0;
            context->lpVtbl->RSSetViewports(context, 1, &d3d10->viewport);
            d3d10->device->lpVtbl->OMSetBlendState(d3d10->device,
                  d3d10->blend_enable, NULL,
                  D3D10_DEFAULT_SAMPLE_MASK);
            stride = sizeof(d3d10_sprite_t);
            context->lpVtbl->IASetVertexBuffers(
                  context, 0, 1, (D3D10Buffer* const)&d3d10->sprites.vbo,
                  &stride, &offset);
            font_driver_render_msg(d3d10,
                  stat_text,
                  (const struct font_params*)osd_params, NULL);
         }
      }

#ifdef HAVE_OVERLAY
   if (     (d3d10->flags & D3D10_ST_FLAG_OVERLAYS_ENABLE)
         && !overlay_behind_menu)
      d3d10_render_overlay(d3d10);
#endif

#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
      gfx_widgets_frame(video_info);
#endif

   if (msg && *msg)
   {
      UINT offset = 0, stride = 0;
      d3d10->device->lpVtbl->RSSetViewports(d3d10->device,
            1, &d3d10->viewport);
      d3d10->device->lpVtbl->OMSetBlendState(d3d10->device,
            d3d10->blend_enable, NULL,
            D3D10_DEFAULT_SAMPLE_MASK);
      stride = sizeof(d3d10_sprite_t);
      d3d10->device->lpVtbl->IASetVertexBuffers(
            d3d10->device, 0, 1,
            (D3D10Buffer* const)&d3d10->sprites.vbo,
            &stride, &offset);
      font_driver_render_msg(d3d10, msg, NULL, NULL);
   }
   d3d10->flags &= ~D3D10_ST_FLAG_SPRITES_ENABLE;

#ifndef __WINRT__
   video_driver_update_title(NULL);
#endif
   DXGIPresent(d3d10->swapChain, d3d10->swap_interval, 0);

   if (
           black_frame_insertion
        && !(d3d10->flags & D3D10_ST_FLAG_MENU_ENABLE)
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
      if (bfi_light_frames > 0 && !(d3d10->flags & D3D10_ST_FLAG_FRAME_DUPE_LOCK))
      {
         d3d10->flags |= D3D10_ST_FLAG_FRAME_DUPE_LOCK;
         while (bfi_light_frames > 0)
         {
            if (!(d3d10_gfx_frame(d3d10, NULL, 0, 0, frame_count, 0, msg, video_info)))
            {
               d3d10->flags &= ~D3D10_ST_FLAG_FRAME_DUPE_LOCK;
               return false;
            }
            --bfi_light_frames;
         }
         d3d10->flags &= ~D3D10_ST_FLAG_FRAME_DUPE_LOCK;
      }

      for (n = 0; n < video_info->bfi_dark_frames; ++n)
      {
         if (!(d3d10->flags & D3D10_ST_FLAG_FRAME_DUPE_LOCK))
         {
            context->lpVtbl->OMSetRenderTargets(context, 1, &d3d10->renderTargetView, NULL);
            context->lpVtbl->ClearRenderTargetView(context, d3d10->renderTargetView, d3d10->clearcolor);
            DXGIPresent(d3d10->swapChain, d3d10->swap_interval, 0);
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
         &&  (!(d3d10->flags & D3D10_ST_FLAG_MENU_ENABLE))
         &&  (!(d3d10->flags & D3D10_ST_FLAG_FRAME_DUPE_LOCK)))
   {
      d3d10->flags |= D3D10_ST_FLAG_FRAME_DUPE_LOCK;

      for (k = 1; k < video_info->shader_subframes; k++)
      {
#ifdef D3D10_ROLLING_SCANLINE_SIMULATION
         video_info->current_subframe = k;
#endif /* D3D10_ROLLING_SCANLINE_SIMULATION */

         if (d3d10->shader_preset)
            for (m = 0; m < d3d10->shader_preset->passes; m++)
            {
               d3d10->pass[m].total_subframes = video_info->shader_subframes;
               d3d10->pass[m].current_subframe = k+1;
            }
         if (!d3d10_gfx_frame(d3d10, NULL, 0, 0, frame_count, 0, msg,
                  video_info))
         {
            d3d10->flags &= ~D3D10_ST_FLAG_FRAME_DUPE_LOCK;
            return false;
         }
      }
      d3d10->flags &= ~D3D10_ST_FLAG_FRAME_DUPE_LOCK;
   }

   return true;
}

static void d3d10_gfx_set_nonblock_state(void* data, bool toggle,
      bool adaptive_vsync_enabled,
      unsigned swap_interval)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   if (toggle)
   {
      d3d10->flags         &= ~D3D10_ST_FLAG_VSYNC;
      d3d10->swap_interval  =  0;
   }
   else
   {
      d3d10->flags         |=  D3D10_ST_FLAG_VSYNC;
      d3d10->swap_interval  =  swap_interval;
   }
}

static bool d3d10_gfx_alive(void* data)
{
   bool quit;
   bool resize_chain    = false;
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   win32_check_window(NULL,
         &quit, &resize_chain, &d3d10->vp.full_width,
         &d3d10->vp.full_height);

   if (resize_chain)
      d3d10->flags |=  D3D10_ST_FLAG_RESIZE_CHAIN;
   else
      d3d10->flags &= ~D3D10_ST_FLAG_RESIZE_CHAIN;

   if (     (d3d10->flags & D3D10_ST_FLAG_RESIZE_CHAIN)
         && (d3d10->vp.full_width  != 0)
         && (d3d10->vp.full_height != 0))
      video_driver_set_size(d3d10->vp.full_width, d3d10->vp.full_height);

   return !quit;
}

static bool d3d10_gfx_suppress_screensaver(void* data, bool enable) { return false; }
static bool d3d10_gfx_has_windowed(void* data) { return true; }

static struct video_shader* d3d10_gfx_get_current_shader(void* data)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return NULL;

   return d3d10->shader_preset;
}

static void d3d10_gfx_viewport_info(void* data, struct video_viewport* vp)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   *vp = d3d10->vp;
}

static void d3d10_set_menu_texture_frame(
      void* data, const void* frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   d3d10_video_t* d3d10    = (d3d10_video_t*)data;
   settings_t*    settings = config_get_ptr();
   DXGI_FORMAT    format   = rgb32 ? DXGI_FORMAT_B8G8R8A8_UNORM :
      (DXGI_FORMAT)DXGI_FORMAT_EX_A4R4G4B4_UNORM;

   if (
            (d3d10->menu.texture.desc.Width  != width)
         || (d3d10->menu.texture.desc.Height != height))
   {
      d3d10->menu.texture.desc.Format = format;
      d3d10->menu.texture.desc.Width  = width;
      d3d10->menu.texture.desc.Height = height;
      d3d10_release_texture(&d3d10->menu.texture);
      d3d10_init_texture(d3d10->device, &d3d10->menu.texture);
   }

   if (d3d10->menu.texture.staging)
      d3d10_update_texture(d3d10->device, width, height, 0,
            format, frame, &d3d10->menu.texture);
   d3d10->menu.texture.sampler = d3d10->samplers
      [settings->bools.menu_linear_filter
      ? RARCH_FILTER_LINEAR
         : RARCH_FILTER_NEAREST][RARCH_WRAP_DEFAULT];
}
static void d3d10_set_menu_texture_enable(
      void* data, bool state, bool fullscreen)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (!d3d10)
      return;

   if (state)
      d3d10->flags |=  D3D10_ST_FLAG_MENU_ENABLE;
   else
      d3d10->flags &= ~D3D10_ST_FLAG_MENU_ENABLE;
   if (fullscreen)
      d3d10->flags |=  D3D10_ST_FLAG_MENU_FULLSCREEN;
   else
      d3d10->flags &= ~D3D10_ST_FLAG_MENU_FULLSCREEN;
}

static void d3d10_gfx_set_aspect_ratio(void* data,
      unsigned aspect_ratio_idx)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;
   if (d3d10)
      d3d10->flags |= D3D10_ST_FLAG_RESIZE_VIEWPORT
                    | D3D10_ST_FLAG_KEEP_ASPECT;
}

static void d3d10_gfx_apply_state_changes(void* data)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;
   if (d3d10)
	   d3d10->flags |= D3D10_ST_FLAG_RESIZE_VIEWPORT;
}

static void d3d10_gfx_set_osd_msg(
      void* data, const char *msg,
      const struct font_params *params, void* font)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   if (d3d10)
   {
      if (d3d10->flags & D3D10_ST_FLAG_SPRITES_ENABLE)
         font_driver_render_msg(d3d10, msg, params, font);
   }
}

static uintptr_t d3d10_gfx_load_texture(
      void* video_data, void* data, bool threaded,
      enum texture_filter_type filter_type)
{
   d3d10_texture_t*      texture = NULL;
   d3d10_video_t*        d3d10   = (d3d10_video_t*)video_data;
   struct texture_image* image   = (struct texture_image*)data;

   if (!d3d10)
      return 0;

   texture = (d3d10_texture_t*)calloc(1, sizeof(*texture));

   if (!texture)
      return 0;

   switch (filter_type)
   {
      case TEXTURE_FILTER_MIPMAP_LINEAR:
         texture->desc.MiscFlags = D3D10_RESOURCE_MISC_GENERATE_MIPS;
         /* fallthrough */
      case TEXTURE_FILTER_LINEAR:
         texture->sampler = d3d10->samplers[RARCH_FILTER_LINEAR][RARCH_WRAP_EDGE];
         break;
      case TEXTURE_FILTER_MIPMAP_NEAREST:
         texture->desc.MiscFlags = D3D10_RESOURCE_MISC_GENERATE_MIPS;
         /* fallthrough */
      case TEXTURE_FILTER_NEAREST:
         texture->sampler = d3d10->samplers[RARCH_FILTER_NEAREST][RARCH_WRAP_EDGE];
         break;
   }

   texture->desc.Width  = image->width;
   texture->desc.Height = image->height;
   texture->desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

   d3d10_release_texture(texture);
   d3d10_init_texture(d3d10->device, texture);

   if (texture->staging)
      d3d10_update_texture(
            d3d10->device,
            image->width, image->height, 0,
            DXGI_FORMAT_B8G8R8A8_UNORM, image->pixels,
            texture);

   return (uintptr_t)texture;
}
static void d3d10_gfx_unload_texture(void* data,
      bool threaded, uintptr_t handle)
{
   d3d10_texture_t* texture = (d3d10_texture_t*)handle;

   if (!texture)
      return;

   Release(texture->view);
   Release(texture->staging);
   Release(texture->handle);
   free(texture);
}

#if 0
   static bool
d3d10_get_hw_render_interface(void* data,
      const struct retro_hw_render_interface** iface)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;
   *iface               = (const struct retro_hw_render_interface*)
      &d3d10->hw.iface;
   return d3d10->hw.enable;
}
#endif

static uint32_t d3d10_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
   BIT32_SET(flags, GFX_CTX_FLAGS_OVERLAY_BEHIND_MENU_SUPPORTED);
   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
   BIT32_SET(flags, GFX_CTX_FLAGS_SUBFRAME_SHADERS);
#endif

   return flags;
}

#ifndef __WINRT__
static void d3d10_get_video_output_prev(void *data)
{
   unsigned width  = 0;
   unsigned height = 0;
   win32_get_video_output_prev(&width, &height);
}

static void d3d10_get_video_output_next(void *data)
{
   unsigned width  = 0;
   unsigned height = 0;
   win32_get_video_output_next(&width, &height);
}
#endif

static const video_poke_interface_t d3d10_poke_interface = {
   d3d10_get_flags,
   d3d10_gfx_load_texture,
   d3d10_gfx_unload_texture,
   NULL, /* set_video_mode */
#ifdef __WINRT__
   /* UWP does not expose this information easily */
   NULL, /* get_refresh_rate */
#else
   win32_get_refresh_rate,
#endif
   d3d10_set_filtering,
#ifdef __WINRT__
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
#else
   win32_get_video_output_size,
   d3d10_get_video_output_prev,
   d3d10_get_video_output_next,
#endif
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   d3d10_gfx_set_aspect_ratio,
   d3d10_gfx_apply_state_changes,
   d3d10_set_menu_texture_frame,
   d3d10_set_menu_texture_enable,
   d3d10_gfx_set_osd_msg,
   win32_show_cursor,
   NULL, /* grab_mouse_toggle */
   d3d10_gfx_get_current_shader,
   NULL, /* get_current_software_framebuffer */
#if 0
   d3d10_get_hw_render_interface,
#else
   NULL, /* get_hw_render_interface */
#endif
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
};

static void d3d10_gfx_get_poke_interface(void* data, const video_poke_interface_t** iface)
{
   *iface = &d3d10_poke_interface;
}

#if defined(HAVE_GFX_WIDGETS)
static bool d3d10_gfx_widgets_enabled(void *data)
{
   (void)data;
   return true;
}
#endif

video_driver_t video_d3d10 = {
   d3d10_gfx_init,
   d3d10_gfx_frame,
   d3d10_gfx_set_nonblock_state,
   d3d10_gfx_alive,
   win32_has_focus,
   d3d10_gfx_suppress_screensaver,
   d3d10_gfx_has_windowed,
   d3d10_gfx_set_shader,
   d3d10_gfx_free,
   "d3d10",
   NULL, /* set_viewport */
   d3d10_gfx_set_rotation,
   d3d10_gfx_viewport_info,
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   d3d10_get_overlay_interface,
#endif
   d3d10_gfx_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#if defined(HAVE_GFX_WIDGETS)
   d3d10_gfx_widgets_enabled
#endif
};
