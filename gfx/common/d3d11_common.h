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

#include <lists/string_list.h>

#include "dxgi_common.h"
#ifdef CINTERFACE
#define D3D11_NO_HELPERS
#endif
#include <d3d11.h>

#include <boolean.h>
#include <retro_math.h>
#include <gfx/math/matrix_4x4.h>
#include <libretro_d3d.h>

#include "../drivers_shader/slang_process.h"

#define D3D11_MAX_GPU_COUNT 16

typedef const ID3D11ShaderResourceView* D3D11ShaderResourceViewRef;
typedef const ID3D11SamplerState*       D3D11SamplerStateRef;
typedef const ID3D11BlendState*         D3D11BlendStateRef;

typedef ID3D11InputLayout*              D3D11InputLayout;
typedef ID3D11RasterizerState*          D3D11RasterizerState;
typedef ID3D11DepthStencilState*        D3D11DepthStencilState;
typedef ID3D11BlendState*               D3D11BlendState;
typedef ID3D11PixelShader*              D3D11PixelShader;
typedef ID3D11SamplerState*             D3D11SamplerState;
typedef ID3D11VertexShader*             D3D11VertexShader;
typedef ID3D11DomainShader*             D3D11DomainShader;
typedef ID3D11HullShader*               D3D11HullShader;
typedef ID3D11ComputeShader*            D3D11ComputeShader;
typedef ID3D11GeometryShader*           D3D11GeometryShader;

/* auto-generated */

typedef ID3D11Resource*                 D3D11Resource;
typedef ID3D11Buffer*                   D3D11Buffer;
typedef ID3D11Texture1D*                D3D11Texture1D;
typedef ID3D11Texture2D*                D3D11Texture2D;
typedef ID3D11Texture3D*                D3D11Texture3D;
typedef ID3D11View*                     D3D11View;
typedef ID3D11ShaderResourceView*       D3D11ShaderResourceView;
typedef ID3D11RenderTargetView*         D3D11RenderTargetView;
typedef ID3D11DepthStencilView*         D3D11DepthStencilView;
typedef ID3D11UnorderedAccessView*      D3D11UnorderedAccessView;
typedef ID3D11Asynchronous*             D3D11Asynchronous;
typedef ID3D11Query*                    D3D11Query;
typedef ID3D11Predicate*                D3D11Predicate;
typedef ID3D11Counter*                  D3D11Counter;
typedef ID3D11ClassInstance*            D3D11ClassInstance;
typedef ID3D11ClassLinkage*             D3D11ClassLinkage;
typedef ID3D11CommandList*              D3D11CommandList;
typedef ID3D11DeviceContext*            D3D11DeviceContext;
typedef ID3D11VideoDecoder*             D3D11VideoDecoder;
typedef ID3D11VideoProcessorEnumerator* D3D11VideoProcessorEnumerator;
typedef ID3D11VideoProcessor*           D3D11VideoProcessor;
typedef ID3D11AuthenticatedChannel*     D3D11AuthenticatedChannel;
typedef ID3D11CryptoSession*            D3D11CryptoSession;
typedef ID3D11VideoDecoderOutputView*   D3D11VideoDecoderOutputView;
typedef ID3D11VideoProcessorInputView*  D3D11VideoProcessorInputView;
typedef ID3D11VideoProcessorOutputView* D3D11VideoProcessorOutputView;
typedef ID3D11VideoContext*             D3D11VideoContext;
typedef ID3D11VideoDevice*              D3D11VideoDevice;
typedef ID3D11Device*                   D3D11Device;
#ifdef DEBUG
typedef ID3D11Debug*                    D3D11Debug;
#endif
typedef ID3D11SwitchToRef*              D3D11SwitchToRef;
typedef ID3D11TracingDevice*            D3D11TracingDevice;
typedef ID3D11InfoQueue*                D3D11InfoQueue;


enum d3d11_feature_level_hint
{
   D3D11_FEATURE_LEVEL_HINT_DONTCARE,
   D3D11_FEATURE_LEVEL_HINT_1_0_CORE,
   D3D11_FEATURE_LEVEL_HINT_9_1,
   D3D11_FEATURE_LEVEL_HINT_9_2,
   D3D11_FEATURE_LEVEL_HINT_9_3,
   D3D11_FEATURE_LEVEL_HINT_10_0,
   D3D11_FEATURE_LEVEL_HINT_10_1,
   D3D11_FEATURE_LEVEL_HINT_11_0,
   D3D11_FEATURE_LEVEL_HINT_11_1,
   D3D11_FEATURE_LEVEL_HINT_12_0,
   D3D11_FEATURE_LEVEL_HINT_12_1,
   D3D11_FEATURE_LEVEL_HINT_12_2
};

typedef struct d3d11_vertex_t
{
   float position[2];
   float texcoord[2];
   float color[4];
} d3d11_vertex_t;

typedef struct
{
   D3D11Texture2D          handle;
   D3D11Texture2D          staging;
   D3D11_TEXTURE2D_DESC    desc;
   D3D11RenderTargetView   rt_view;
   D3D11ShaderResourceView view;
   D3D11SamplerStateRef    sampler;
   float4_t                size_data;
} d3d11_texture_t;

typedef struct
{
   UINT32 colors[4];
   struct
   {
      float x, y, w, h;
   } pos;
   struct
   {
      float u, v, w, h;
   } coords;
   struct
   {
      float scaling;
      float rotation;
   } params;
} d3d11_sprite_t;

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
} d3d11_uniform_t;

typedef struct d3d11_shader_t
{
   D3D11VertexShader   vs;
   D3D11PixelShader    ps;
   D3D11GeometryShader gs;
   D3D11InputLayout    layout;
} d3d11_shader_t;

enum d3d11_state_flags
{
   D3D11_ST_FLAG_VSYNC               = (1 << 0),
   D3D11_ST_FLAG_WAITABLE_SWAPCHAINS = (1 << 1),
   D3D11_ST_FLAG_WAIT_FOR_VBLANK     = (1 << 2),
   D3D11_ST_FLAG_RESIZE_CHAIN        = (1 << 3),
   D3D11_ST_FLAG_KEEP_ASPECT         = (1 << 4),
   D3D11_ST_FLAG_RESIZE_VIEWPORT     = (1 << 5),
   D3D11_ST_FLAG_RESIZE_RTS          = (1 << 6), /* RT = Render Target */
   D3D11_ST_FLAG_INIT_HISTORY        = (1 << 7),
   D3D11_ST_FLAG_HAS_FLIP_MODEL      = (1 << 8),
   D3D11_ST_FLAG_HAS_ALLOW_TEARING   = (1 << 9),
   D3D11_ST_FLAG_HW_IFACE_ENABLE     = (1 << 10),
   D3D11_ST_FLAG_HDR_SUPPORT         = (1 << 11),
   D3D11_ST_FLAG_HDR_ENABLE          = (1 << 12),
   D3D11_ST_FLAG_SPRITES_ENABLE      = (1 << 13),
   D3D11_ST_FLAG_OVERLAYS_ENABLE     = (1 << 14),
   D3D11_ST_FLAG_OVERLAYS_FULLSCREEN = (1 << 15),
   D3D11_ST_FLAG_MENU_ENABLE         = (1 << 16),
   D3D11_ST_FLAG_MENU_FULLSCREEN     = (1 << 17)
};

typedef struct
{
   unsigned              cur_mon_id;
   HANDLE                frameLatencyWaitableObject;
   DXGISwapChain         swapChain;
   D3D11Device           device;
   D3D_FEATURE_LEVEL     supportedFeatureLevel;
   D3D11DeviceContext    context;
   D3D11RasterizerState  scissor_enabled;
   D3D11RasterizerState  scissor_disabled;
   D3D11Buffer           ubo;
   d3d11_uniform_t       ubo_values;
#ifdef HAVE_DXGI_HDR
   d3d11_texture_t       back_buffer;
#endif
   D3D11SamplerState     samplers[RARCH_FILTER_MAX][RARCH_WRAP_MAX];
   D3D11BlendState       blend_enable;
   D3D11BlendState       blend_disable;
   D3D11BlendState       blend_pipeline;
   D3D11Buffer           menu_pipeline_vbo;
   math_matrix_4x4       mvp, mvp_no_rot, identity;
   struct video_viewport vp;
   D3D11_VIEWPORT        viewport;
   D3D11_RECT            scissor;
   DXGI_FORMAT           format;
   float                 clearcolor[4];
   unsigned              swap_interval;
   uint32_t              flags;
   d3d11_shader_t        shaders[GFX_MAX_SHADERS];
#ifdef HAVE_DXGI_HDR
   enum dxgi_swapchain_bit_depth 
                         chain_bit_depth;
   DXGI_COLOR_SPACE_TYPE chain_color_space;
   DXGI_FORMAT           chain_formats[DXGI_SWAPCHAIN_BIT_DEPTH_COUNT];
#endif
#ifdef __WINRT__
   DXGIFactory2 factory;
#else
   DXGIFactory1 factory;
#endif
   DXGIAdapter adapter;

   struct retro_hw_render_interface_d3d11 hw_iface;

#ifdef HAVE_DXGI_HDR
   struct
   {
      dxgi_hdr_uniform_t               ubo_values;
      D3D11Buffer                      ubo;
      float                            max_output_nits;
      float                            min_output_nits;
      float                            max_cll;
      float                            max_fall;
   } hdr;
#endif

	struct
   {
      d3d11_shader_t shader;
      d3d11_shader_t shader_font;
      D3D11Buffer    vbo;
      int            offset;
      int            capacity;
   } sprites;

#ifdef HAVE_OVERLAY
   struct
   {
      D3D11Buffer      vbo;
      d3d11_texture_t* textures;
      int              count;
   } overlays;
#endif

   struct
   {
      d3d11_texture_t texture;
      D3D11Buffer     vbo;
   } menu;

   struct
   {
      d3d11_texture_t texture[GFX_MAX_FRAME_HISTORY + 1];
      D3D11Buffer     vbo;
      D3D11Buffer     ubo;
      D3D11_VIEWPORT  viewport;
      float4_t        output_size;
      int             rotation;
   } frame;

   struct
   {
      d3d11_shader_t             shader;
      D3D11Buffer                buffers[SLANG_CBUFFER_MAX];
      d3d11_texture_t            rt;
      d3d11_texture_t            feedback;
      D3D11_VIEWPORT             viewport;
      pass_semantics_t           semantics;
      uint32_t                   frame_count;
      int32_t                    frame_direction;
   } pass[GFX_MAX_SHADERS];

   struct video_shader* shader_preset;
   struct string_list *gpu_list;
   IDXGIAdapter1 *current_adapter;
   IDXGIAdapter1 *adapters[D3D11_MAX_GPU_COUNT];
   d3d11_texture_t      luts[GFX_MAX_TEXTURES];
} d3d11_video_t;

static INLINE void d3d11_release_texture(d3d11_texture_t* texture)
{
   Release(texture->handle);
   Release(texture->staging);
   Release(texture->view);
   Release(texture->rt_view);
}

void d3d11_init_texture(D3D11Device device, d3d11_texture_t* texture);

void d3d11_update_texture(
      D3D11DeviceContext ctx,
      unsigned           width,
      unsigned           height,
      unsigned           pitch,
      DXGI_FORMAT        format,
      const void*        data,
      d3d11_texture_t*   texture);

DXGI_FORMAT d3d11_get_closest_match(
      D3D11Device device, DXGI_FORMAT desired_format, UINT desired_format_support);

bool d3d11_init_shader(
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
      enum d3d11_feature_level_hint   hint);
