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

#define CINTERFACE

#include <assert.h>
#include <stdbool.h>
#include <string/stdstring.h>

#include "../video_driver.h"
#include "../font_driver.h"
#include "../common/win32_common.h"
#include "../common/dxgi_common.h"
#include "../common/d3d12_common.h"
#include "../common/d3dcompiler_common.h"

//#include "../../menu/menu_driver.h"
#include "../../driver.h"
#include "../../verbosity.h"
#include "../../configuration.h"

static void d3d12_set_filtering(void* data, unsigned index, bool smooth)
{
   int            i;
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   for (i = 0; i < RARCH_WRAP_MAX; i++)
   {
      if (smooth)
         d3d12->samplers[RARCH_FILTER_UNSPEC][i] = d3d12->samplers[RARCH_FILTER_LINEAR][i];
      else
         d3d12->samplers[RARCH_FILTER_UNSPEC][i] = d3d12->samplers[RARCH_FILTER_NEAREST][i];
   }
}

static void d3d12_gfx_set_rotation(void* data, unsigned rotation)
{
   math_matrix_4x4  rot;
   math_matrix_4x4* mvp;
   D3D12_RANGE      read_range = { 0, 0 };
   d3d12_video_t*   d3d12      = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   d3d12->frame.rotation = rotation;

   matrix_4x4_rotate_z(rot, d3d12->frame.rotation * (M_PI / 2.0f));
   matrix_4x4_multiply(d3d12->mvp, rot, d3d12->mvp_no_rot);

   D3D12Map(d3d12->frame.ubo, 0, &read_range, (void**)&mvp);
   *mvp = d3d12->mvp;
   D3D12Unmap(d3d12->frame.ubo, 0, NULL);
}

static void d3d12_update_viewport(void* data, bool force_full)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   video_driver_update_viewport(&d3d12->vp, force_full, d3d12->keep_aspect);

   d3d12->frame.viewport.TopLeftX = (float)d3d12->vp.x;
   d3d12->frame.viewport.TopLeftY = (float)d3d12->vp.y;
   d3d12->frame.viewport.Width    = (float)d3d12->vp.width;
   d3d12->frame.viewport.Height   = (float)d3d12->vp.height;
   d3d12->frame.viewport.MaxDepth = 0.0f;
   d3d12->frame.viewport.MaxDepth = 1.0f;

   /* having to add vp.x and vp.y here doesn't make any sense */
   d3d12->frame.scissorRect.top    = 0;
   d3d12->frame.scissorRect.left   = 0;
   d3d12->frame.scissorRect.right  = d3d12->vp.x + d3d12->vp.width;
   d3d12->frame.scissorRect.bottom = d3d12->vp.y + d3d12->vp.height;

   d3d12->resize_viewport = false;
}

static bool d3d12_gfx_init_pipelines(d3d12_video_t* d3d12)
{
   D3DBlob                            vs_code = NULL;
   D3DBlob                            ps_code = NULL;
   D3DBlob                            gs_code = NULL;
   D3D12_GRAPHICS_PIPELINE_STATE_DESC desc    = { 0 };

   desc.BlendState.RenderTarget[0] = d3d12_blend_enable_desc;
   desc.pRootSignature             = d3d12->desc.rootSignature;
   desc.RTVFormats[0]              = DXGI_FORMAT_R8G8B8A8_UNORM;

   {
      static const char shader[] =
#include "../drivers/d3d_shaders/opaque_sm5.hlsl.h"
            ;

      static const D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, position),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, texcoord),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d12_vertex_t, color),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      };

      if (!d3d_compile(shader, sizeof(shader), NULL, "VSMain", "vs_5_0", &vs_code))
         goto error;
      if (!d3d_compile(shader, sizeof(shader), NULL, "PSMain", "ps_5_0", &ps_code))
         goto error;

      desc.PrimitiveTopologyType          = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
      desc.InputLayout.pInputElementDescs = inputElementDesc;
      desc.InputLayout.NumElements        = countof(inputElementDesc);

      if (!d3d12_init_pipeline(
                d3d12->device, vs_code, ps_code, NULL, &desc,
                &d3d12->pipes[VIDEO_SHADER_STOCK_BLEND]))
         goto error;

      Release(vs_code);
      Release(ps_code);
      vs_code = NULL;
      ps_code = NULL;
   }
   {
      static const char shader[] =
#include "d3d_shaders/sprite_sm4.hlsl.h"
            ;

      D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d12_sprite_t, pos),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d12_sprite_t, coords),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d12_sprite_t, colors[0]),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "COLOR", 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d12_sprite_t, colors[1]),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "COLOR", 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d12_sprite_t, colors[2]),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "COLOR", 3, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d12_sprite_t, colors[3]),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "PARAMS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_sprite_t, params),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      };

      if (!d3d_compile(shader, sizeof(shader), NULL, "VSMain", "vs_5_0", &vs_code))
         goto error;
      if (!d3d_compile(shader, sizeof(shader), NULL, "PSMain", "ps_5_0", &ps_code))
         goto error;
      if (!d3d_compile(shader, sizeof(shader), NULL, "GSMain", "gs_5_0", &gs_code))
         goto error;

      desc.PrimitiveTopologyType          = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
      desc.InputLayout.pInputElementDescs = inputElementDesc;
      desc.InputLayout.NumElements        = countof(inputElementDesc);

      if (!d3d12_init_pipeline(
                d3d12->device, vs_code, ps_code, gs_code, &desc, &d3d12->sprites.pipe))
         goto error;

      Release(ps_code);
      ps_code = NULL;

      if (!d3d_compile(shader, sizeof(shader), NULL, "PSMainA8", "ps_5_0", &ps_code))
         goto error;

      if (!d3d12_init_pipeline(
                d3d12->device, vs_code, ps_code, gs_code, &desc, &d3d12->sprites.pipe_font))
         goto error;

      Release(vs_code);
      Release(ps_code);
      Release(gs_code);
      vs_code = NULL;
      ps_code = NULL;
      gs_code = NULL;
   }

   {
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

      D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, position),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, texcoord),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      };

      desc.PrimitiveTopologyType          = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
      desc.InputLayout.pInputElementDescs = inputElementDesc;
      desc.InputLayout.NumElements        = countof(inputElementDesc);

      if (!d3d_compile(simple_snow, sizeof(simple_snow), NULL, "VSMain", "vs_5_0", &vs_code))
         goto error;
      if (!d3d_compile(simple_snow, sizeof(simple_snow), NULL, "PSMain", "ps_5_0", &ps_code))
         goto error;

      if (!d3d12_init_pipeline(
                d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pipes[VIDEO_SHADER_MENU_3]))
         goto error;

      Release(vs_code);
      Release(ps_code);
      vs_code = NULL;
      ps_code = NULL;

      if (!d3d_compile(snow, sizeof(snow), NULL, "VSMain", "vs_5_0", &vs_code))
         goto error;
      if (!d3d_compile(snow, sizeof(snow), NULL, "PSMain", "ps_5_0", &ps_code))
         goto error;

      if (!d3d12_init_pipeline(
                d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pipes[VIDEO_SHADER_MENU_4]))
         goto error;

      Release(vs_code);
      Release(ps_code);
      vs_code = NULL;
      ps_code = NULL;

      if (!d3d_compile(bokeh, sizeof(bokeh), NULL, "VSMain", "vs_5_0", &vs_code))
         goto error;
      if (!d3d_compile(bokeh, sizeof(bokeh), NULL, "PSMain", "ps_5_0", &ps_code))
         goto error;

      if (!d3d12_init_pipeline(
                d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pipes[VIDEO_SHADER_MENU_5]))
         goto error;

      Release(vs_code);
      Release(ps_code);
      vs_code = NULL;
      ps_code = NULL;

      if (!d3d_compile(snowflake, sizeof(snowflake), NULL, "VSMain", "vs_5_0", &vs_code))
         goto error;
      if (!d3d_compile(snowflake, sizeof(snowflake), NULL, "PSMain", "ps_5_0", &ps_code))
         goto error;

      if (!d3d12_init_pipeline(
                d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pipes[VIDEO_SHADER_MENU_6]))
         goto error;

      Release(vs_code);
      Release(ps_code);
      vs_code = NULL;
      ps_code = NULL;
   }

   {
      static const char ribbon[] =
#include "d3d_shaders/ribbon_sm4.hlsl.h"
            ;
      static const char ribbon_simple[] =
#include "d3d_shaders/ribbon_simple_sm4.hlsl.h"
            ;

      D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      };

      desc.BlendState.RenderTarget[0].SrcBlend  = D3D12_BLEND_ONE;
      desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
      desc.PrimitiveTopologyType                = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
      desc.InputLayout.pInputElementDescs       = inputElementDesc;
      desc.InputLayout.NumElements              = countof(inputElementDesc);

      if (!d3d_compile(ribbon, sizeof(ribbon), NULL, "VSMain", "vs_5_0", &vs_code))
         goto error;
      if (!d3d_compile(ribbon, sizeof(ribbon), NULL, "PSMain", "ps_5_0", &ps_code))
         goto error;

      if (!d3d12_init_pipeline(
                d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pipes[VIDEO_SHADER_MENU]))
         goto error;

      Release(vs_code);
      Release(ps_code);
      vs_code = NULL;
      ps_code = NULL;

      if (!d3d_compile(ribbon_simple, sizeof(ribbon), NULL, "VSMain", "vs_5_0", &vs_code))
         goto error;
      if (!d3d_compile(ribbon_simple, sizeof(ribbon), NULL, "PSMain", "ps_5_0", &ps_code))
         goto error;

      if (!d3d12_init_pipeline(
                d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pipes[VIDEO_SHADER_MENU_2]))
         goto error;

      Release(vs_code);
      Release(ps_code);
      vs_code = NULL;
      ps_code = NULL;
   }

   return true;

error:
   Release(vs_code);
   Release(ps_code);
   Release(gs_code);
   return false;
}

static void d3d12_gfx_free(void* data)
{
   unsigned       i;
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   font_driver_free_osd();

   Release(d3d12->sprites.vbo);

   Release(d3d12->frame.ubo);
   Release(d3d12->frame.vbo);
   Release(d3d12->frame.texture.handle);
   Release(d3d12->frame.texture.upload_buffer);
   Release(d3d12->menu.vbo);
   Release(d3d12->menu.texture.handle);
   Release(d3d12->menu.texture.upload_buffer);

   free(d3d12->desc.sampler_heap.map);
   free(d3d12->desc.srv_heap.map);
   free(d3d12->desc.rtv_heap.map);
   Release(d3d12->desc.sampler_heap.handle);
   Release(d3d12->desc.srv_heap.handle);
   Release(d3d12->desc.rtv_heap.handle);

   Release(d3d12->desc.sl_rootSignature);
   Release(d3d12->desc.rootSignature);

   Release(d3d12->ubo);

   for (i = 0; i < GFX_MAX_SHADERS; i++)
      Release(d3d12->pipes[i]);

   Release(d3d12->sprites.pipe);
   Release(d3d12->sprites.pipe_font);

   Release(d3d12->queue.fence);
   Release(d3d12->chain.renderTargets[0]);
   Release(d3d12->chain.renderTargets[1]);
   Release(d3d12->chain.handle);

   Release(d3d12->queue.cmd);
   Release(d3d12->queue.allocator);
   Release(d3d12->queue.handle);

   Release(d3d12->factory);
   Release(d3d12->device);
   Release(d3d12->adapter);

   win32_monitor_from_window();
   win32_destroy_window();

   free(d3d12);
}

static void*
d3d12_gfx_init(const video_info_t* video, const input_driver_t** input, void** input_data)
{
   WNDCLASSEX     wndclass = { 0 };
   settings_t*    settings = config_get_ptr();
   d3d12_video_t* d3d12    = (d3d12_video_t*)calloc(1, sizeof(*d3d12));

   if (!d3d12)
      return NULL;

   win32_window_reset();
   win32_monitor_init();
   wndclass.lpfnWndProc = WndProcD3D;
   win32_window_init(&wndclass, true, NULL);

   if (!win32_set_video_mode(d3d12, video->width, video->height, video->fullscreen))
   {
      RARCH_ERR("[D3D12]: win32_set_video_mode failed.\n");
      goto error;
   }

   dxgi_input_driver(settings->arrays.input_joypad_driver, input, input_data);

   if (!d3d12_init_base(d3d12))
      goto error;

   if (!d3d12_init_descriptors(d3d12))
      goto error;

   if (!d3d12_gfx_init_pipelines(d3d12))
      goto error;

   if (!d3d12_init_queue(d3d12))
      goto error;

   if (!d3d12_init_swapchain(d3d12, video->width, video->height, main_window.hwnd))
      goto error;

   d3d12_init_samplers(d3d12);
   d3d12_set_filtering(d3d12, 0, video->smooth);

   d3d12_create_fullscreen_quad_vbo(d3d12->device, &d3d12->frame.vbo_view, &d3d12->frame.vbo);
   d3d12_create_fullscreen_quad_vbo(d3d12->device, &d3d12->menu.vbo_view, &d3d12->menu.vbo);

   d3d12->sprites.capacity                = 4096;
   d3d12->sprites.vbo_view.SizeInBytes    = sizeof(d3d12_sprite_t) * d3d12->sprites.capacity;
   d3d12->sprites.vbo_view.StrideInBytes  = sizeof(d3d12_sprite_t);
   d3d12->sprites.vbo_view.BufferLocation = d3d12_create_buffer(
         d3d12->device, d3d12->sprites.vbo_view.SizeInBytes, &d3d12->sprites.vbo);

   d3d12->keep_aspect = video->force_aspect;
   d3d12->chain.vsync = video->vsync;
   d3d12->format      = video->rgb32 ? DXGI_FORMAT_B8G8R8X8_UNORM : DXGI_FORMAT_B5G6R5_UNORM;
   d3d12->frame.texture.desc.Format = d3d12->format;

   d3d12->ubo_view.SizeInBytes = sizeof(math_matrix_4x4);
   d3d12->ubo_view.BufferLocation =
         d3d12_create_buffer(d3d12->device, d3d12->ubo_view.SizeInBytes, &d3d12->ubo);

   d3d12->frame.ubo_view.SizeInBytes = sizeof(math_matrix_4x4);
   d3d12->frame.ubo_view.BufferLocation =
         d3d12_create_buffer(d3d12->device, d3d12->frame.ubo_view.SizeInBytes, &d3d12->frame.ubo);

   matrix_4x4_ortho(d3d12->mvp_no_rot, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);

   {
      math_matrix_4x4* mvp;
      D3D12_RANGE      read_range = { 0, 0 };
      D3D12Map(d3d12->ubo, 0, &read_range, (void**)&mvp);
      *mvp = d3d12->mvp_no_rot;
      D3D12Unmap(d3d12->ubo, 0, NULL);
   }

   d3d12_gfx_set_rotation(d3d12, 0);
   d3d12->vp.full_width   = video->width;
   d3d12->vp.full_height  = video->height;
   d3d12->resize_viewport = true;

   font_driver_init_osd(d3d12, false, video->is_threaded, FONT_DRIVER_RENDER_D3D12_API);

   return d3d12;

error:
   RARCH_ERR("[D3D12]: failed to init video driver.\n");
   d3d12_gfx_free(d3d12);
   return NULL;
}

static bool d3d12_gfx_frame(
      void*               data,
      const void*         frame,
      unsigned            width,
      unsigned            height,
      uint64_t            frame_count,
      unsigned            pitch,
      const char*         msg,
      video_frame_info_t* video_info)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   if (d3d12->resize_chain)
   {
      unsigned i;

      for (i = 0; i < countof(d3d12->chain.renderTargets); i++)
         Release(d3d12->chain.renderTargets[i]);

      DXGIResizeBuffers(d3d12->chain.handle, 0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);

      for (i = 0; i < countof(d3d12->chain.renderTargets); i++)
      {
         DXGIGetSwapChainBuffer(d3d12->chain.handle, i, &d3d12->chain.renderTargets[i]);
         D3D12CreateRenderTargetView(
               d3d12->device, d3d12->chain.renderTargets[i], NULL, d3d12->chain.desc_handles[i]);
      }

      d3d12->chain.frame_index = DXGIGetCurrentBackBufferIndex(d3d12->chain.handle);
      d3d12->resize_chain      = false;
      d3d12->resize_viewport   = true;
   }

   PERF_START();
   D3D12ResetCommandAllocator(d3d12->queue.allocator);

   D3D12ResetGraphicsCommandList(
         d3d12->queue.cmd, d3d12->queue.allocator, d3d12->pipes[VIDEO_SHADER_STOCK_BLEND]);
   D3D12SetGraphicsRootSignature(d3d12->queue.cmd, d3d12->desc.rootSignature);
   {
      D3D12DescriptorHeap desc_heaps[] = { d3d12->desc.srv_heap.handle,
                                           d3d12->desc.sampler_heap.handle };
      D3D12SetDescriptorHeaps(d3d12->queue.cmd, countof(desc_heaps), desc_heaps);
   }

   d3d12_resource_transition(
         d3d12->queue.cmd, d3d12->chain.renderTargets[d3d12->chain.frame_index],
         D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

   D3D12OMSetRenderTargets(
         d3d12->queue.cmd, 1, &d3d12->chain.desc_handles[d3d12->chain.frame_index], FALSE, NULL);
   D3D12ClearRenderTargetView(
         d3d12->queue.cmd, d3d12->chain.desc_handles[d3d12->chain.frame_index],
         d3d12->chain.clearcolor, 0, NULL);

   D3D12IASetPrimitiveTopology(d3d12->queue.cmd, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
   if (data && width && height)
   {
      if (d3d12->frame.texture.desc.Width != width || d3d12->frame.texture.desc.Height != height)
      {
         d3d12->frame.texture.desc.Width  = width;
         d3d12->frame.texture.desc.Height = height;
         d3d12->frame.texture.srv_heap    = &d3d12->desc.srv_heap;
         d3d12_init_texture(d3d12->device, &d3d12->frame.texture);
      }
      d3d12_update_texture(width, height, pitch, d3d12->format, frame, &d3d12->frame.texture);

      d3d12_upload_texture(d3d12->queue.cmd, &d3d12->frame.texture);
   }
#if 0 /* custom viewport doesn't call apply_state_changes, so we can't rely on this for now */
   if (d3d12->resize_viewport)
#endif
   d3d12_update_viewport(d3d12, false);

   D3D12RSSetViewports(d3d12->queue.cmd, 1, &d3d12->frame.viewport);
   D3D12RSSetScissorRects(d3d12->queue.cmd, 1, &d3d12->frame.scissorRect);

   D3D12SetGraphicsRootConstantBufferView(
         d3d12->queue.cmd, ROOT_ID_UBO, d3d12->frame.ubo_view.BufferLocation);
   d3d12_set_texture(d3d12->queue.cmd, &d3d12->frame.texture);
   d3d12_set_sampler(d3d12->queue.cmd, d3d12->samplers[RARCH_FILTER_UNSPEC][RARCH_WRAP_DEFAULT]);
   D3D12IASetVertexBuffers(d3d12->queue.cmd, 0, 1, &d3d12->frame.vbo_view);
   D3D12DrawInstanced(d3d12->queue.cmd, 4, 1, 0, 0);

   if (d3d12->menu.enabled && d3d12->menu.texture.handle)
   {
      if (d3d12->menu.texture.dirty)
         d3d12_upload_texture(d3d12->queue.cmd, &d3d12->menu.texture);

      D3D12SetGraphicsRootConstantBufferView(
            d3d12->queue.cmd, ROOT_ID_UBO, d3d12->ubo_view.BufferLocation);

      if (d3d12->menu.fullscreen)
      {
         D3D12RSSetViewports(d3d12->queue.cmd, 1, &d3d12->chain.viewport);
         D3D12RSSetScissorRects(d3d12->queue.cmd, 1, &d3d12->chain.scissorRect);
      }

      d3d12_set_texture_and_sampler(d3d12->queue.cmd, &d3d12->menu.texture);
      D3D12IASetVertexBuffers(d3d12->queue.cmd, 0, 1, &d3d12->menu.vbo_view);
      D3D12DrawInstanced(d3d12->queue.cmd, 4, 1, 0, 0);
   }

   D3D12RSSetViewports(d3d12->queue.cmd, 1, &d3d12->chain.viewport);
   D3D12RSSetScissorRects(d3d12->queue.cmd, 1, &d3d12->chain.scissorRect);

   D3D12SetPipelineState(d3d12->queue.cmd, d3d12->sprites.pipe);
   D3D12IASetPrimitiveTopology(d3d12->queue.cmd, D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
   D3D12IASetVertexBuffers(d3d12->queue.cmd, 0, 1, &d3d12->sprites.vbo_view);

   d3d12->sprites.enabled = true;
#if 0
   if (d3d12->menu.enabled)
      menu_driver_frame(video_info);
#endif
   if (msg && *msg)
   {
      font_driver_render_msg(video_info, NULL, msg, NULL);
      dxgi_update_title(video_info);
   }
   d3d12->sprites.enabled = false;


   d3d12_resource_transition(
         d3d12->queue.cmd, d3d12->chain.renderTargets[d3d12->chain.frame_index],
         D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
   D3D12CloseGraphicsCommandList(d3d12->queue.cmd);
   D3D12ExecuteGraphicsCommandLists(d3d12->queue.handle, 1, &d3d12->queue.cmd);

#if 1
   DXGIPresent(d3d12->chain.handle, !!d3d12->chain.vsync, 0);
#else
   DXGI_PRESENT_PARAMETERS pp = { 0 };
   DXGIPresent1(d3d12->swapchain, 0, 0, &pp);
#endif

   /* wait_for_previous_frame */
   D3D12SignalCommandQueue(d3d12->queue.handle, d3d12->queue.fence, d3d12->queue.fenceValue);

   if (D3D12GetCompletedValue(d3d12->queue.fence) < d3d12->queue.fenceValue)
   {
      D3D12SetEventOnCompletion(
            d3d12->queue.fence, d3d12->queue.fenceValue, d3d12->queue.fenceEvent);
      WaitForSingleObject(d3d12->queue.fenceEvent, INFINITE);
   }

   d3d12->queue.fenceValue++;
   d3d12->chain.frame_index = DXGIGetCurrentBackBufferIndex(d3d12->chain.handle);
   PERF_STOP();

   if (msg && *msg)
      dxgi_update_title(video_info);

   return true;
}

static void d3d12_gfx_set_nonblock_state(void* data, bool toggle)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;
   d3d12->chain.vsync   = !toggle;
}

static bool d3d12_gfx_alive(void* data)
{
   bool           quit;
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   win32_check_window(&quit, &d3d12->resize_chain, &d3d12->vp.full_width, &d3d12->vp.full_height);

   if (d3d12->resize_chain && d3d12->vp.full_width != 0 && d3d12->vp.full_height != 0)
      video_driver_set_size(&d3d12->vp.full_width, &d3d12->vp.full_height);

   return !quit;
}

static bool d3d12_gfx_focus(void* data) { return win32_has_focus(); }

static bool d3d12_gfx_suppress_screensaver(void* data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool d3d12_gfx_has_windowed(void* data)
{
   (void)data;
   return true;
}

static bool d3d12_gfx_set_shader(void* data, enum rarch_shader_type type, const char* path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void d3d12_gfx_viewport_info(void* data, struct video_viewport* vp)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   *vp = d3d12->vp;
}

static bool d3d12_gfx_read_viewport(void* data, uint8_t* buffer, bool is_idle)
{
   (void)data;
   (void)buffer;

   return true;
}

static void d3d12_set_menu_texture_frame(
      void* data, const void* frame, bool rgb32, unsigned width, unsigned height, float alpha)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;
   int            pitch = width * (rgb32 ? sizeof(uint32_t) : sizeof(uint16_t));
   DXGI_FORMAT    format =
         rgb32 ? DXGI_FORMAT_B8G8R8A8_UNORM : (DXGI_FORMAT)DXGI_FORMAT_EX_A4R4G4B4_UNORM;

   if (d3d12->menu.texture.desc.Width != width || d3d12->menu.texture.desc.Height != height)
   {
      d3d12->menu.texture.desc.Width  = width;
      d3d12->menu.texture.desc.Height = height;
      d3d12->menu.texture.desc.Format = format;
      d3d12->menu.texture.srv_heap    = &d3d12->desc.srv_heap;
      d3d12_init_texture(d3d12->device, &d3d12->menu.texture);
   }

   d3d12_update_texture(width, height, pitch, format, frame, &d3d12->menu.texture);

   d3d12->menu.alpha = alpha;

   {
      D3D12_RANGE     read_range = { 0, 0 };
      d3d12_vertex_t* v;

      D3D12Map(d3d12->menu.vbo, 0, &read_range, (void**)&v);
      v[0].color[3] = alpha;
      v[1].color[3] = alpha;
      v[2].color[3] = alpha;
      v[3].color[3] = alpha;
      D3D12Unmap(d3d12->menu.vbo, 0, NULL);
   }

   d3d12->menu.texture.sampler = config_get_ptr()->bools.menu_linear_filter
                                       ? d3d12->samplers[RARCH_FILTER_LINEAR][RARCH_WRAP_DEFAULT]
                                       : d3d12->samplers[RARCH_FILTER_NEAREST][RARCH_WRAP_DEFAULT];
}
static void d3d12_set_menu_texture_enable(void* data, bool state, bool full_screen)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   d3d12->menu.enabled    = state;
   d3d12->menu.fullscreen = full_screen;
}

static void d3d12_gfx_set_aspect_ratio(void* data, unsigned aspect_ratio_idx)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   d3d12->keep_aspect     = true;
   d3d12->resize_viewport = true;
}

static void d3d12_gfx_apply_state_changes(void* data)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   if (d3d12)
      d3d12->resize_viewport = true;
}

static void d3d12_gfx_set_osd_msg(
      void* data, video_frame_info_t* video_info, const char* msg, const void* params, void* font)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   if (d3d12)
   {
      if (d3d12->sprites.enabled)
         font_driver_render_msg(video_info, font, msg, params);
      else
         printf("OSD msg: %s\n", msg);
   }
}

static const video_poke_interface_t d3d12_poke_interface = {
   NULL, /* set_coords */
   NULL, /* set_mvp */
   NULL, /* load_texture */
   NULL, /* unload_texture */
   NULL, /* set_video_mode */
   d3d12_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   d3d12_gfx_set_aspect_ratio,
   d3d12_gfx_apply_state_changes,
   d3d12_set_menu_texture_frame,
   d3d12_set_menu_texture_enable,
   d3d12_gfx_set_osd_msg,
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
};

static void d3d12_gfx_get_poke_interface(void* data, const video_poke_interface_t** iface)
{
   *iface = &d3d12_poke_interface;
}

video_driver_t video_d3d12 = {
   d3d12_gfx_init,
   d3d12_gfx_frame,
   d3d12_gfx_set_nonblock_state,
   d3d12_gfx_alive,
   d3d12_gfx_focus,
   d3d12_gfx_suppress_screensaver,
   d3d12_gfx_has_windowed,
   d3d12_gfx_set_shader,
   d3d12_gfx_free,
   "d3d12",
   NULL, /* set_viewport */
   d3d12_gfx_set_rotation,
   d3d12_gfx_viewport_info,
   d3d12_gfx_read_viewport,
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
#endif
   d3d12_gfx_get_poke_interface,
};
