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

#include <assert.h>

#include <string/stdstring.h>
#include <gfx/scaler/pixconv.h>

#include "../../driver.h"
#include "../../verbosity.h"
#include "../configuration.h"
#include "../video_driver.h"
#include "../font_driver.h"
#include "../common/win32_common.h"
#include "../common/d3d11_common.h"
#include "../common/dxgi_common.h"
#include "../common/d3dcompiler_common.h"
#include "../../performance_counters.h"
#include "../../menu/menu_driver.h"

static void d3d11_set_filtering(void* data, unsigned index, bool smooth)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   if (smooth)
      d3d11->frame.texture.sampler = d3d11->sampler_linear;
   else
      d3d11->frame.texture.sampler = d3d11->sampler_nearest;
}

static void d3d11_gfx_set_rotation(void* data, unsigned rotation)
{
   math_matrix_4x4  rot;
   math_matrix_4x4* mvp;
   d3d11_video_t*   d3d11 = (d3d11_video_t*)data;

   d3d11->frame.rotation = 3 * rotation;

   matrix_4x4_rotate_z(rot, d3d11->frame.rotation * (M_PI / 2.0f));
   matrix_4x4_multiply(d3d11->mvp, rot, d3d11->mvp_no_rot);

   D3D11_MAPPED_SUBRESOURCE mapped_ubo;
   D3D11MapBuffer(d3d11->ctx, d3d11->frame.ubo, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_ubo);
   *(math_matrix_4x4*)mapped_ubo.pData = d3d11->mvp;
   D3D11UnmapBuffer(d3d11->ctx, d3d11->frame.ubo, 0);
}

static void d3d11_update_viewport(void* data, bool force_full)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   video_driver_update_viewport(&d3d11->vp, force_full, d3d11->keep_aspect);

   d3d11->frame.viewport.TopLeftX = d3d11->vp.x;
   d3d11->frame.viewport.TopLeftY = d3d11->vp.y;
   d3d11->frame.viewport.Width    = d3d11->vp.width;
   d3d11->frame.viewport.Height   = d3d11->vp.height;
   d3d11->frame.viewport.MaxDepth = 0.0f;
   d3d11->frame.viewport.MaxDepth = 1.0f;

   d3d11->resize_viewport = false;
}

static void d3d11_gfx_free(void* data)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   if (!d3d11)
      return;

   Release(d3d11->frame.ubo);
   Release(d3d11->frame.texture.view);
   Release(d3d11->frame.texture.handle);
   Release(d3d11->frame.texture.staging);
   Release(d3d11->frame.vbo);

   Release(d3d11->menu.texture.handle);
   Release(d3d11->menu.texture.staging);
   Release(d3d11->menu.texture.view);
   Release(d3d11->menu.vbo);

   Release(d3d11->sprites.vs);
   Release(d3d11->sprites.ps);
   Release(d3d11->sprites.ps_8bit);
   Release(d3d11->sprites.gs);
   Release(d3d11->sprites.vbo);
   Release(d3d11->sprites.layout);

   Release(d3d11->ubo);
   Release(d3d11->blend_enable);
   Release(d3d11->blend_disable);
   Release(d3d11->sampler_nearest);
   Release(d3d11->sampler_linear);
   Release(d3d11->ps);
   Release(d3d11->vs);
   Release(d3d11->layout);
   Release(d3d11->state);
   Release(d3d11->renderTargetView);
   Release(d3d11->swapChain);

   font_driver_free_osd();

   Release(d3d11->ctx);
   Release(d3d11->device);

   win32_monitor_from_window();
   win32_destroy_window();
   free(d3d11);
}

static void*
d3d11_gfx_init(const video_info_t* video, const input_driver_t** input, void** input_data)
{
   WNDCLASSEX      wndclass = { 0 };
   MONITORINFOEX   current_mon;
   HMONITOR        hm_to_use;
   settings_t*     settings = config_get_ptr();
   gfx_ctx_input_t inp      = { input, input_data };
   d3d11_video_t*  d3d11    = (d3d11_video_t*)calloc(1, sizeof(*d3d11));

   if (!d3d11)
      return NULL;

   win32_window_reset();
   win32_monitor_init();
   wndclass.lpfnWndProc = WndProcD3D;
   win32_window_init(&wndclass, true, NULL);

   win32_monitor_info(&current_mon, &hm_to_use, &d3d11->cur_mon_id);

   d3d11->vp.full_width  = video->width;
   d3d11->vp.full_height = video->height;

   if (!d3d11->vp.full_width)
      d3d11->vp.full_width = current_mon.rcMonitor.right - current_mon.rcMonitor.left;
   if (!d3d11->vp.full_height)
      d3d11->vp.full_height = current_mon.rcMonitor.bottom - current_mon.rcMonitor.top;

   if (!win32_set_video_mode(d3d11, d3d11->vp.full_width, d3d11->vp.full_height, video->fullscreen))
   {
      RARCH_ERR("[D3D11]: win32_set_video_mode failed.\n");
      goto error;
   }

   gfx_ctx_d3d.input_driver(NULL, settings->arrays.input_joypad_driver, input, input_data);

   {
      UINT                 flags                   = 0;
      D3D_FEATURE_LEVEL    requested_feature_level = D3D_FEATURE_LEVEL_11_0;
      DXGI_SWAP_CHAIN_DESC desc                    = {
         .BufferCount                        = 1,
         .BufferDesc.Width                   = d3d11->vp.full_width,
         .BufferDesc.Height                  = d3d11->vp.full_height,
         .BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM,
         .BufferDesc.RefreshRate.Numerator   = 60,
         .BufferDesc.RefreshRate.Denominator = 1,
         .BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT,
         .OutputWindow                       = main_window.hwnd,
         .SampleDesc.Count                   = 1,
         .SampleDesc.Quality                 = 0,
         .Windowed                           = TRUE,
         .SwapEffect                         = DXGI_SWAP_EFFECT_SEQUENTIAL,
#if 0
         .SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD,
         .SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
         .SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_DISCARD,
#endif
      };

#ifdef DEBUG
      flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

      D3D11CreateDeviceAndSwapChain(
            NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, &requested_feature_level, 1,
            D3D11_SDK_VERSION, &desc, (IDXGISwapChain**)&d3d11->swapChain, &d3d11->device,
            &d3d11->supportedFeatureLevel, &d3d11->ctx);
   }

   {
      D3D11Texture2D backBuffer;
      DXGIGetSwapChainBufferD3D11(d3d11->swapChain, 0, &backBuffer);
      D3D11CreateTexture2DRenderTargetView(
            d3d11->device, backBuffer, NULL, &d3d11->renderTargetView);
      Release(backBuffer);
   }

   D3D11SetRenderTargets(d3d11->ctx, 1, &d3d11->renderTargetView, NULL);

   video_driver_set_size(&d3d11->vp.full_width, &d3d11->vp.full_height);
   d3d11->viewport.Width  = d3d11->vp.full_width;
   d3d11->viewport.Height = d3d11->vp.full_height;
   d3d11->resize_viewport = true;
   d3d11->vsync           = video->vsync;
   d3d11->format          = video->rgb32 ? DXGI_FORMAT_B8G8R8X8_UNORM : DXGI_FORMAT_B5G6R5_UNORM;

   d3d11->frame.texture.desc.Format =
         d3d11_get_closest_match_texture2D(d3d11->device, d3d11->format);
   d3d11->frame.texture.desc.Usage = D3D11_USAGE_DEFAULT;

   d3d11->menu.texture.desc.Format = DXGI_FORMAT_B4G4R4A4_UNORM;
   d3d11->menu.texture.desc.Usage  = D3D11_USAGE_DEFAULT;

   matrix_4x4_ortho(d3d11->mvp_no_rot, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);

   {
      D3D11_BUFFER_DESC desc = {
         .ByteWidth      = sizeof(math_matrix_4x4),
         .Usage          = D3D11_USAGE_DYNAMIC,
         .BindFlags      = D3D11_BIND_CONSTANT_BUFFER,
         .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
      };
      D3D11_SUBRESOURCE_DATA ubo_data = { &d3d11->mvp_no_rot };
      D3D11CreateBuffer(d3d11->device, &desc, &ubo_data, &d3d11->ubo);
      D3D11CreateBuffer(d3d11->device, &desc, NULL, &d3d11->frame.ubo);
   }
   d3d11_gfx_set_rotation(d3d11, 0);

   {
      D3D11_SAMPLER_DESC desc = {
         .Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT,
         .AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP,
         .AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP,
         .AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP,
         .MaxAnisotropy  = 1,
         .ComparisonFunc = D3D11_COMPARISON_NEVER,
         .MinLOD         = -D3D11_FLOAT32_MAX,
         .MaxLOD         = D3D11_FLOAT32_MAX,
      };
      D3D11CreateSamplerState(d3d11->device, &desc, &d3d11->sampler_nearest);

      desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
      D3D11CreateSamplerState(d3d11->device, &desc, &d3d11->sampler_linear);
   }

   d3d11_set_filtering(d3d11, 0, video->smooth);

   {
      d3d11_vertex_t vertices[] = {
         { { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
         { { 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
         { { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
         { { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
      };

      {
         D3D11_BUFFER_DESC desc = {
            .Usage     = D3D11_USAGE_IMMUTABLE,
            .ByteWidth = sizeof(vertices),
            .BindFlags = D3D11_BIND_VERTEX_BUFFER,
         };
         D3D11_SUBRESOURCE_DATA vertexData = { vertices };
         D3D11CreateBuffer(d3d11->device, &desc, &vertexData, &d3d11->frame.vbo);
         desc.Usage          = D3D11_USAGE_DYNAMIC;
         desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
         D3D11CreateBuffer(d3d11->device, &desc, &vertexData, &d3d11->menu.vbo);

         d3d11->sprites.capacity = 4096;
         desc.ByteWidth          = sizeof(d3d11_sprite_t) * d3d11->sprites.capacity;
         D3D11CreateBuffer(d3d11->device, &desc, NULL, &d3d11->sprites.vbo);
      }
   }

   {
      D3DBlob vs_code;
      D3DBlob ps_code;

      static const char stock[] =
#include "d3d_shaders/opaque_sm5.hlsl.h"
            ;

      D3D11_INPUT_ELEMENT_DESC desc[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d11_vertex_t, position),
           D3D11_INPUT_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d11_vertex_t, texcoord),
           D3D11_INPUT_PER_VERTEX_DATA, 0 },
         { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d11_vertex_t, color),
           D3D11_INPUT_PER_VERTEX_DATA, 0 },
      };

      d3d_compile(stock, sizeof(stock), "VSMain", "vs_5_0", &vs_code);
      d3d_compile(stock, sizeof(stock), "PSMain", "ps_5_0", &ps_code);

      D3D11CreateVertexShader(
            d3d11->device, D3DGetBufferPointer(vs_code), D3DGetBufferSize(vs_code), NULL,
            &d3d11->vs);
      D3D11CreatePixelShader(
            d3d11->device, D3DGetBufferPointer(ps_code), D3DGetBufferSize(ps_code), NULL,
            &d3d11->ps);
      D3D11CreateInputLayout(
            d3d11->device, desc, countof(desc), D3DGetBufferPointer(vs_code),
            D3DGetBufferSize(vs_code), &d3d11->layout);

      Release(vs_code);
      Release(ps_code);
   }

   {
      D3DBlob vs_code;
      D3DBlob ps_code;
      D3DBlob ps_A8_code;
      D3DBlob gs_code;

      static const char sprite[] =
#include "d3d_shaders/sprite_sm4.hlsl.h"
            ;

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
#if 1
      d3d_compile(sprite, sizeof(sprite), "VSMain", "vs_5_0", &vs_code);
      d3d_compile(sprite, sizeof(sprite), "PSMain", "ps_5_0", &ps_code);
      d3d_compile(sprite, sizeof(sprite), "PSMainA8", "ps_5_0", &ps_A8_code);
      d3d_compile(sprite, sizeof(sprite), "GSMain", "gs_5_0", &gs_code);
#else
      if (!d3d_compile_from_file(
                L"gfx/drivers/d3d_shaders/sprite_sm4.hlsl", "VSMain", "vs_5_0", &vs_code))
         goto error;
      if (!d3d_compile_from_file(
                L"gfx/drivers/d3d_shaders/sprite_sm4.hlsl", "PSMain", "ps_5_0", &ps_code))
         goto error;
      if (!d3d_compile_from_file(
                L"gfx/drivers/d3d_shaders/sprite_sm4.hlsl", "PSMainA8", "ps_5_0", &ps_A8_code))
         goto error;
      if (!d3d_compile_from_file(
                L"gfx/drivers/d3d_shaders/sprite_sm4.hlsl", "GSMain", "gs_5_0", &gs_code))
         goto error;
#endif
      D3D11CreateVertexShader(
            d3d11->device, D3DGetBufferPointer(vs_code), D3DGetBufferSize(vs_code), NULL,
            &d3d11->sprites.vs);
      D3D11CreatePixelShader(
            d3d11->device, D3DGetBufferPointer(ps_code), D3DGetBufferSize(ps_code), NULL,
            &d3d11->sprites.ps);
      D3D11CreatePixelShader(
            d3d11->device, D3DGetBufferPointer(ps_A8_code), D3DGetBufferSize(ps_A8_code), NULL,
            &d3d11->sprites.ps_8bit);
      D3D11CreateGeometryShader(
            d3d11->device, D3DGetBufferPointer(gs_code), D3DGetBufferSize(gs_code), NULL,
            &d3d11->sprites.gs);
      D3D11CreateInputLayout(
            d3d11->device, desc, countof(desc), D3DGetBufferPointer(vs_code),
            D3DGetBufferSize(vs_code), &d3d11->sprites.layout);

      Release(vs_code);
      Release(ps_code);
      Release(ps_A8_code);
      Release(gs_code);
   }

   {
      D3D11_BLEND_DESC blend_desc = {
         .AlphaToCoverageEnable  = FALSE,
         .IndependentBlendEnable = FALSE,
         .RenderTarget[0] =
               {
                     .BlendEnable = TRUE,
                     D3D11_BLEND_SRC_ALPHA,
                     D3D11_BLEND_INV_SRC_ALPHA,
                     D3D11_BLEND_OP_ADD,
                     D3D11_BLEND_SRC_ALPHA,
                     D3D11_BLEND_INV_SRC_ALPHA,
                     D3D11_BLEND_OP_ADD,
                     D3D11_COLOR_WRITE_ENABLE_ALL,
               },
      };
      D3D11CreateBlendState(d3d11->device, &blend_desc, &d3d11->blend_enable);
      blend_desc.RenderTarget[0].BlendEnable = FALSE;
      D3D11CreateBlendState(d3d11->device, &blend_desc, &d3d11->blend_disable);
   }
   {
      D3D11_RASTERIZER_DESC desc = {
         .FillMode        = D3D11_FILL_SOLID,
         .CullMode        = D3D11_CULL_NONE,
         .DepthClipEnable = TRUE,
      };
      D3D11CreateRasterizerState(d3d11->device, &desc, &d3d11->state);
   }
   D3D11SetState(d3d11->ctx, d3d11->state);

   font_driver_init_osd(d3d11, false, video->is_threaded, FONT_DRIVER_RENDER_D3D11_API);

   return d3d11;

error:
   d3d11_gfx_free(d3d11);
   return NULL;
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
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   if (d3d11->resize_chain)
   {
      D3D11Texture2D backBuffer;

      Release(d3d11->renderTargetView);
      DXGIResizeBuffers(d3d11->swapChain, 0, 0, 0, 0, 0);

      DXGIGetSwapChainBufferD3D11(d3d11->swapChain, 0, &backBuffer);
      D3D11CreateTexture2DRenderTargetView(
            d3d11->device, backBuffer, NULL, &d3d11->renderTargetView);
      Release(backBuffer);

      D3D11SetRenderTargets(d3d11->ctx, 1, &d3d11->renderTargetView, NULL);
      d3d11->viewport.Width  = video_info->width;
      d3d11->viewport.Height = video_info->height;

      d3d11->resize_chain    = false;
      d3d11->resize_viewport = true;
      video_driver_set_size(&video_info->width, &video_info->height);
   }

   PERF_START();
   D3D11ClearRenderTargetView(d3d11->ctx, d3d11->renderTargetView, d3d11->clearcolor);
   D3D11SetVShader(d3d11->ctx, d3d11->vs, NULL, 0);
   D3D11SetPShader(d3d11->ctx, d3d11->ps, NULL, 0);
   D3D11SetGShader(d3d11->ctx, NULL, NULL, 0);
   D3D11SetInputLayout(d3d11->ctx, d3d11->layout);
   D3D11SetPrimitiveTopology(d3d11->ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

   if (frame && width && height)
   {
      if (d3d11->frame.texture.desc.Width != width || d3d11->frame.texture.desc.Height != height)
      {
         d3d11->frame.texture.desc.Width  = width;
         d3d11->frame.texture.desc.Height = height;
         d3d11_init_texture(d3d11->device, &d3d11->frame.texture);
      }

      d3d11_update_texture(
            d3d11->ctx, width, height, pitch, d3d11->format, frame, &d3d11->frame.texture);
   }

   {
      UINT stride = sizeof(d3d11_vertex_t);
      UINT offset = 0;

#if 0 /* custom viewport doesn't call apply_state_changes, so we can't rely on this for now */
   if (d3d11->resize_viewport)
#endif
      d3d11_update_viewport(d3d11, false);

      D3D11SetViewports(d3d11->ctx, 1, &d3d11->frame.viewport);
      d3d11_set_texture_and_sampler(d3d11->ctx, 0, &d3d11->frame.texture);

      D3D11SetVertexBuffers(d3d11->ctx, 0, 1, &d3d11->frame.vbo, &stride, &offset);
      D3D11SetVShaderConstantBuffers(d3d11->ctx, 0, 1, &d3d11->frame.ubo);
      D3D11SetBlendState(d3d11->ctx, d3d11->blend_disable, NULL, D3D11_DEFAULT_SAMPLE_MASK);
      D3D11Draw(d3d11->ctx, 4, 0);
      //      D3D11SetBlendState(d3d11->ctx, d3d11->blend_enable, NULL, D3D11_DEFAULT_SAMPLE_MASK);

      if (d3d11->menu.enabled && d3d11->menu.texture.handle)
      {
         if (d3d11->menu.fullscreen)
            D3D11SetViewports(d3d11->ctx, 1, &d3d11->viewport);
         D3D11SetVertexBuffers(d3d11->ctx, 0, 1, &d3d11->menu.vbo, &stride, &offset);
         D3D11SetVShaderConstantBuffers(d3d11->ctx, 0, 1, &d3d11->ubo);
         d3d11_set_texture_and_sampler(d3d11->ctx, 0, &d3d11->menu.texture);

         D3D11Draw(d3d11->ctx, 4, 0);
      }
   }
   {
      UINT sprite_stride = sizeof(d3d11_sprite_t);
      UINT offset        = 0;
      D3D11SetViewports(d3d11->ctx, 1, &d3d11->viewport);
      D3D11SetVShader(d3d11->ctx, d3d11->sprites.vs, NULL, 0);
      D3D11SetPShader(d3d11->ctx, d3d11->sprites.ps, NULL, 0);
      D3D11SetGShader(d3d11->ctx, d3d11->sprites.gs, NULL, 0);
      D3D11SetInputLayout(d3d11->ctx, d3d11->sprites.layout);
      D3D11SetPrimitiveTopology(d3d11->ctx, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
      D3D11SetVertexBuffers(d3d11->ctx, 0, 1, &d3d11->sprites.vbo, &sprite_stride, &offset);
      D3D11SetBlendState(d3d11->ctx, d3d11->blend_enable, NULL, D3D11_DEFAULT_SAMPLE_MASK);

      d3d11->sprites.enabled = true;

      if (d3d11->menu.enabled)
         menu_driver_frame(video_info);

      if (msg && *msg)
      {
         font_driver_render_msg(video_info, NULL, msg, NULL);
         gfx_ctx_d3d.update_window_title(NULL, video_info);
      }
      d3d11->sprites.enabled = false;
   }

   DXGIPresent(d3d11->swapChain, !!d3d11->vsync, 0);
   PERF_STOP();

   return true;
}

static void d3d11_gfx_set_nonblock_state(void* data, bool toggle)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   if (!d3d11)
      return;

   d3d11->vsync = !toggle;
}

static bool d3d11_gfx_alive(void* data)
{
   bool           quit;
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   win32_check_window(&quit, &d3d11->resize_chain, &d3d11->vp.full_width, &d3d11->vp.full_height);

   if (d3d11->resize_chain && d3d11->vp.full_width != 0 && d3d11->vp.full_height != 0)
      video_driver_set_size(&d3d11->vp.full_width, &d3d11->vp.full_height);

   return !quit;
}

static bool d3d11_gfx_focus(void* data) { return win32_has_focus(); }

static bool d3d11_gfx_suppress_screensaver(void* data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool d3d11_gfx_has_windowed(void* data)
{
   (void)data;
   return true;
}

static bool d3d11_gfx_set_shader(void* data, enum rarch_shader_type type, const char* path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void d3d11_gfx_viewport_info(void* data, struct video_viewport* vp)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   *vp = d3d11->vp;
}

static bool d3d11_gfx_read_viewport(void* data, uint8_t* buffer, bool is_idle)
{
   (void)data;
   (void)buffer;

   return true;
}

static void d3d11_set_menu_texture_frame(
      void* data, const void* frame, bool rgb32, unsigned width, unsigned height, float alpha)
{
   d3d11_video_t* d3d11  = (d3d11_video_t*)data;
   int            pitch  = width * (rgb32 ? sizeof(uint32_t) : sizeof(uint16_t));
   DXGI_FORMAT    format = rgb32 ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_EX_A4R4G4B4_UNORM;

   if (d3d11->menu.texture.desc.Width != width || d3d11->menu.texture.desc.Height != height)
   {
      d3d11->menu.texture.desc.Format = d3d11_get_closest_match_texture2D(d3d11->device, format);
      d3d11->menu.texture.desc.Width  = width;
      d3d11->menu.texture.desc.Height = height;
      d3d11_init_texture(d3d11->device, &d3d11->menu.texture);
   }

   d3d11_update_texture(d3d11->ctx, width, height, pitch, format, frame, &d3d11->menu.texture);
   d3d11->menu.texture.sampler = config_get_ptr()->bools.menu_linear_filter ? d3d11->sampler_linear
                                                                    : d3d11->sampler_nearest;
}
static void d3d11_set_menu_texture_enable(void* data, bool state, bool full_screen)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   if (!d3d11)
      return;

   d3d11->menu.enabled    = state;
   d3d11->menu.fullscreen = full_screen;
}

static void d3d11_gfx_set_aspect_ratio(void* data, unsigned aspect_ratio_idx)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   if (!d3d11)
      return;

   d3d11->keep_aspect     = true;
   d3d11->resize_viewport = true;
}

static void d3d11_gfx_apply_state_changes(void* data)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   if (d3d11)
      d3d11->resize_viewport = true;
}

static void d3d11_gfx_set_osd_msg(
      void* data, video_frame_info_t* video_info, const char* msg, const void* params, void* font)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   if (d3d11)
   {
      if (d3d11->sprites.enabled)
         font_driver_render_msg(video_info, font, msg, params);
      else
         printf("OSD msg: %s\n", msg);
   }
}
static uintptr_t d3d11_gfx_load_texture(
      void* video_data, void* data, bool threaded, enum texture_filter_type filter_type)
{
   d3d11_video_t*        d3d11     = (d3d11_video_t*)video_data;
   struct texture_image* image     = (struct texture_image*)data;

   if (!d3d11)
      return 0;

   d3d11_texture_t* texture = calloc(1, sizeof(*texture));

   switch(filter_type)
   {
   case TEXTURE_FILTER_MIPMAP_LINEAR:
      texture->desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
      /* fallthrough */
   case TEXTURE_FILTER_LINEAR:
      texture->sampler = d3d11->sampler_linear;
      break;
   case TEXTURE_FILTER_MIPMAP_NEAREST:
      texture->desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
      /* fallthrough */
   case TEXTURE_FILTER_NEAREST:
      texture->sampler = d3d11->sampler_nearest;
      break;
   }

   texture->desc.Width     = image->width;
   texture->desc.Height    = image->height;
   texture->desc.Format =
         d3d11_get_closest_match_texture2D(d3d11->device, DXGI_FORMAT_B8G8R8A8_UNORM);

   d3d11_init_texture(d3d11->device, texture);

   d3d11_update_texture(
         d3d11->ctx, image->width, image->height, 0, DXGI_FORMAT_B8G8R8A8_UNORM, image->pixels,
         texture);

   return (uintptr_t)texture;
}
static void d3d11_gfx_unload_texture(void* data, uintptr_t handle)
{
   d3d11_texture_t* texture = (d3d11_texture_t*)handle;

   if (!texture)
      return;

   Release(texture->view);
   Release(texture->staging);
   Release(texture->handle);
   free(texture);
}

static const video_poke_interface_t d3d11_poke_interface = {
   NULL, /* set_coords */
   NULL, /* set_mvp */
   d3d11_gfx_load_texture,
   d3d11_gfx_unload_texture,
   NULL, /* set_video_mode */
   d3d11_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   d3d11_gfx_set_aspect_ratio,
   d3d11_gfx_apply_state_changes,
   d3d11_set_menu_texture_frame,
   d3d11_set_menu_texture_enable,
   d3d11_gfx_set_osd_msg,
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
};

static void d3d11_gfx_get_poke_interface(void* data, const video_poke_interface_t** iface)
{
   *iface = &d3d11_poke_interface;
}

video_driver_t video_d3d11 = {
   d3d11_gfx_init,
   d3d11_gfx_frame,
   d3d11_gfx_set_nonblock_state,
   d3d11_gfx_alive,
   d3d11_gfx_focus,
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
   NULL, /* overlay_interface */
#endif
   d3d11_gfx_get_poke_interface,
};
