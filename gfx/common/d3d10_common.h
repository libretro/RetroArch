/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2018 - Ali Bouhlel
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

#pragma once

#include <retro_inline.h>

#include "dxgi_common.h"
#include <d3d10.h>

#include <boolean.h>

#include <retro_math.h>
#include <gfx/math/matrix_4x4.h>

#include "../drivers_shader/slang_process.h"

#define D3D10_MAX_GPU_COUNT 16

typedef const ID3D10SamplerState*       D3D10SamplerStateRef;

typedef ID3D10InputLayout*       D3D10InputLayout;
typedef ID3D10RasterizerState*   D3D10RasterizerState;
typedef ID3D10DepthStencilState* D3D10DepthStencilState;
typedef ID3D10BlendState*        D3D10BlendState;
typedef ID3D10PixelShader*       D3D10PixelShader;
typedef ID3D10SamplerState*      D3D10SamplerState;
typedef ID3D10VertexShader*      D3D10VertexShader;
typedef ID3D10GeometryShader*    D3D10GeometryShader;

/* auto-generated */
typedef ID3DDestructionNotifier*  D3DDestructionNotifier;
typedef ID3D10Resource*           D3D10Resource;
typedef ID3D10Buffer*             D3D10Buffer;
typedef ID3D10Texture1D*          D3D10Texture1D;
typedef ID3D10Texture2D*          D3D10Texture2D;
typedef ID3D10Texture3D*          D3D10Texture3D;
typedef ID3D10View*               D3D10View;
typedef ID3D10ShaderResourceView* D3D10ShaderResourceView;
typedef ID3D10RenderTargetView*   D3D10RenderTargetView;
typedef ID3D10DepthStencilView*   D3D10DepthStencilView;
typedef ID3D10Asynchronous*       D3D10Asynchronous;
typedef ID3D10Query*              D3D10Query;
typedef ID3D10Predicate*          D3D10Predicate;
typedef ID3D10Counter*            D3D10Counter;
typedef ID3D10Device*             D3D10Device;
typedef ID3D10Multithread*        D3D10Multithread;
#ifdef DEBUG
typedef ID3D10Debug*              D3D10Debug;
#endif
typedef ID3D10SwitchToRef*        D3D10SwitchToRef;
typedef ID3D10InfoQueue*          D3D10InfoQueue;

typedef struct d3d10_vertex_t
{
   float position[2];
   float texcoord[2];
   float color[4];
} d3d10_vertex_t;

typedef struct
{
   D3D10Texture2D          handle;
   D3D10Texture2D          staging;
   D3D10_TEXTURE2D_DESC    desc;
   D3D10RenderTargetView   rt_view;
   D3D10ShaderResourceView view;
   D3D10SamplerStateRef    sampler;
   float4_t                size_data;
} d3d10_texture_t;

typedef struct
{
   struct
   {
      float x, y, w, h;
   } pos;
   struct
   {
      float u, v, w, h;
   } coords;
   UINT32 colors[4];
   struct
   {
      float scaling;
      float rotation;
   } params;
} d3d10_sprite_t;

#ifndef ALIGN
#ifdef _MSC_VER
#define ALIGN(x) __declspec(align(x))
#else
#define ALIGN(x) __attribute__((aligned(x)))
#endif
#endif

typedef struct ALIGN(16)
{
   math_matrix_4x4 mvp;
   struct
   {
      float width;
      float height;
   } OutputSize;
   float time;
} d3d10_uniform_t;

typedef struct d3d10_shader_t
{
   D3D10VertexShader   vs;
   D3D10PixelShader    ps;
   D3D10GeometryShader gs;
   D3D10InputLayout    layout;
} d3d10_shader_t;

enum d3d10_video_flags
{
   D3D10_ST_FLAG_VSYNC               = (1 << 0),
   D3D10_ST_FLAG_RESIZE_CHAIN        = (1 << 1),
   D3D10_ST_FLAG_KEEP_ASPECT         = (1 << 2),
   D3D10_ST_FLAG_RESIZE_VIEWPORT     = (1 << 3),
   D3D10_ST_FLAG_RESIZE_RTS          = (1 << 4), /* RT = Render Target */
   D3D10_ST_FLAG_INIT_HISTORY        = (1 << 5),
   D3D10_ST_FLAG_SPRITES_ENABLE      = (1 << 6),
   D3D10_ST_FLAG_OVERLAYS_ENABLE     = (1 << 7),
   D3D10_ST_FLAG_OVERLAYS_FULLSCREEN = (1 << 8),
   D3D10_ST_FLAG_MENU_ENABLE         = (1 << 9),
   D3D10_ST_FLAG_MENU_FULLSCREEN     = (1 << 10)
};

typedef struct
{
   unsigned              cur_mon_id;
   DXGISwapChain         swapChain;
   D3D10Device           device;
   D3D10RasterizerState  state;
   D3D10RenderTargetView renderTargetView;
   D3D10Buffer           ubo;
   d3d10_uniform_t       ubo_values;
   D3D10SamplerState     samplers[RARCH_FILTER_MAX][RARCH_WRAP_MAX];
   D3D10BlendState       blend_enable;
   D3D10BlendState       blend_disable;
   D3D10BlendState       blend_pipeline;
   D3D10Buffer           menu_pipeline_vbo;
   math_matrix_4x4       mvp, mvp_no_rot;
   struct video_viewport vp;
   D3D10_VIEWPORT        viewport;
   DXGI_FORMAT           format;
   float                 clearcolor[4];
   unsigned              swap_interval;
   d3d10_shader_t        shaders[GFX_MAX_SHADERS];
#ifdef __WINRT__
   DXGIFactory2 factory;
#else
   DXGIFactory1 factory;
#endif
   DXGIAdapter adapter;

	struct
   {
      d3d10_shader_t shader;
      d3d10_shader_t shader_font;
      D3D10Buffer    vbo;
      int            offset;
      int            capacity;
   } sprites;

#ifdef HAVE_OVERLAY
   struct
   {
      D3D10Buffer      vbo;
      d3d10_texture_t* textures;
      int              count;
   } overlays;
#endif

   struct
   {
      d3d10_texture_t   texture;
      D3D10Buffer       vbo;
   } menu;
   struct
   {
      d3d10_texture_t texture[GFX_MAX_FRAME_HISTORY + 1];
      D3D10Buffer       vbo;
      D3D10Buffer       ubo;
      D3D10_VIEWPORT    viewport;
      float4_t          output_size;
      int               rotation;
   } frame;

   struct
   {
      d3d10_shader_t             shader;
      D3D10Buffer                buffers[SLANG_CBUFFER_MAX];
      d3d10_texture_t            rt;
      d3d10_texture_t            feedback;
      D3D10_VIEWPORT             viewport;
      pass_semantics_t           semantics;
      uint32_t                   frame_count;
      int32_t                    frame_direction;
   } pass[GFX_MAX_SHADERS];

   struct video_shader* shader_preset;
   d3d10_texture_t      luts[GFX_MAX_TEXTURES];
   struct string_list *gpu_list;
   IDXGIAdapter1 *adapters[D3D10_MAX_GPU_COUNT];
   IDXGIAdapter1 *current_adapter;
   uint16_t flags;
} d3d10_video_t;

#if !defined(__cplusplus) || defined(CINTERFACE)
static INLINE void D3D10SetVertexBuffer(
      D3D10Device device_context,
      UINT               slot,
      D3D10Buffer const  vertex_buffer,
      UINT               stride,
      UINT               offset)
{
   device_context->lpVtbl->IASetVertexBuffers(
         device_context, slot, 1, (D3D10Buffer* const)&vertex_buffer, &stride, &offset);
}
/* end of auto-generated */
#endif

void d3d10_init_texture(D3D10Device device, d3d10_texture_t* texture);

static INLINE void d3d10_release_texture(d3d10_texture_t* texture)
{
   Release(texture->handle);
   Release(texture->staging);
   Release(texture->view);
   Release(texture->rt_view);
}

void d3d10_update_texture(
      D3D10Device      ctx,
      int              width,
      int              height,
      int              pitch,
      DXGI_FORMAT      format,
      const void*      data,
      d3d10_texture_t* texture);

DXGI_FORMAT d3d10_get_closest_match(
      D3D10Device device, DXGI_FORMAT desired_format, UINT desired_format_support);

bool d3d10_init_shader(
      D3D10Device                     device,
      const char*                     src,
      size_t                          size,
      const void*                     src_name,
      LPCSTR                          vs_entry,
      LPCSTR                          ps_entry,
      LPCSTR                          gs_entry,
      const D3D10_INPUT_ELEMENT_DESC* input_element_descs,
      UINT                            num_elements,
      d3d10_shader_t*                 out);

static INLINE void d3d10_release_shader(d3d10_shader_t* shader)
{
   Release(shader->layout);
   Release(shader->vs);
   Release(shader->ps);
   Release(shader->gs);
}

static INLINE DXGI_FORMAT
d3d10_get_closest_match_texture2D(D3D10Device device, DXGI_FORMAT desired_format)
{
   return d3d10_get_closest_match(
         device, desired_format,
           D3D10_FORMAT_SUPPORT_TEXTURE2D 
         | D3D10_FORMAT_SUPPORT_SHADER_SAMPLE);
}

static INLINE void d3d10_set_shader(D3D10Device ctx, d3d10_shader_t* shader)
{
   ctx->lpVtbl->IASetInputLayout(ctx, shader->layout);
   ctx->lpVtbl->VSSetShader(ctx, shader->vs);
   ctx->lpVtbl->PSSetShader(ctx, shader->ps);
   ctx->lpVtbl->GSSetShader(ctx, shader->gs);
}

#if !defined(__cplusplus) || defined(CINTERFACE)
static INLINE void
d3d10_set_texture_and_sampler(D3D10Device ctx, UINT slot, d3d10_texture_t* texture)
{
   ctx->lpVtbl->PSSetShaderResources(ctx, slot, 1, &texture->view);
   ctx->lpVtbl->PSSetSamplers(ctx, slot, 1, (D3D10SamplerState*)&texture->sampler);
}
#endif
