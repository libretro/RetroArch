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

#include "driver.h"
#include "verbosity.h"
#include "configuration.h"
#include "gfx/video_driver.h"
#include "gfx/common/win32_common.h"
#include "gfx/common/d3d10_common.h"
#include "gfx/common/dxgi_common.h"
#include "gfx/common/d3dcompiler_common.h"

static void*
d3d10_gfx_init(const video_info_t* video, const input_driver_t** input, void** input_data)
{
   WNDCLASSEX      wndclass = { 0 };
   settings_t*     settings = config_get_ptr();
   gfx_ctx_input_t inp      = { input, input_data };
   d3d10_video_t*  d3d10    = (d3d10_video_t*)calloc(1, sizeof(*d3d10));

   if (!d3d10)
      return NULL;

   win32_window_reset();
   win32_monitor_init();
   wndclass.lpfnWndProc = WndProcD3D;
   win32_window_init(&wndclass, true, NULL);

   if (!win32_set_video_mode(d3d10, video->width, video->height, video->fullscreen))
   {
      RARCH_ERR("[D3D10]: win32_set_video_mode failed.\n");
      goto error;
   }

   gfx_ctx_d3d.input_driver(NULL, settings->arrays.input_joypad_driver, input, input_data);

   {
      UINT                 flags = 0;
      DXGI_SWAP_CHAIN_DESC desc  = {
         .BufferCount       = 2,
         .BufferDesc.Width  = video->width,
         .BufferDesc.Height = video->height,
         .BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
         //         .BufferDesc.RefreshRate.Numerator = 60,
         //         .BufferDesc.RefreshRate.Denominator = 1,
         .BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT,
         .OutputWindow       = main_window.hwnd,
         .SampleDesc.Count   = 1,
         .SampleDesc.Quality = 0,
         .Windowed           = TRUE,
         .SwapEffect         = DXGI_SWAP_EFFECT_DISCARD,
#if 0
         .SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL,
         .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
         .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
#endif
      };

#ifdef DEBUG
      flags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

      D3D10CreateDeviceAndSwapChain(
            NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL, flags, D3D10_SDK_VERSION, &desc,
            (IDXGISwapChain**)&d3d10->swapChain, &d3d10->device);
   }

   {
      D3D10Texture2D backBuffer;
      DXGIGetSwapChainBufferD3D10(d3d10->swapChain, 0, &backBuffer);
      D3D10CreateTexture2DRenderTargetView(
            d3d10->device, backBuffer, NULL, &d3d10->renderTargetView);
      Release(backBuffer);
   }

   D3D10SetRenderTargets(d3d10->device, 1, &d3d10->renderTargetView, NULL);

   {
      D3D10_VIEWPORT vp = { 0, 0, video->width, video->height, 0.0f, 1.0f };
      D3D10SetViewports(d3d10->device, 1, &vp);
   }

   d3d10->vsync  = video->vsync;
   d3d10->format = video->rgb32 ? DXGI_FORMAT_B8G8R8X8_UNORM : DXGI_FORMAT_B5G6R5_UNORM;

   d3d10->frame.texture.desc.Format =
         d3d10_get_closest_match_texture2D(d3d10->device, d3d10->format);
   d3d10->frame.texture.desc.Usage = D3D10_USAGE_DEFAULT;
   d3d10->menu.texture.desc.Usage  = D3D10_USAGE_DEFAULT;

   {
      D3D10_SAMPLER_DESC desc = {
         .Filter         = D3D10_FILTER_MIN_MAG_MIP_POINT,
         .AddressU       = D3D10_TEXTURE_ADDRESS_BORDER,
         .AddressV       = D3D10_TEXTURE_ADDRESS_BORDER,
         .AddressW       = D3D10_TEXTURE_ADDRESS_BORDER,
         .MaxAnisotropy  = 1,
         .ComparisonFunc = D3D10_COMPARISON_NEVER,
         .MinLOD         = -D3D10_FLOAT32_MAX,
         .MaxLOD         = D3D10_FLOAT32_MAX,
      };
      D3D10CreateSamplerState(d3d10->device, &desc, &d3d10->sampler_nearest);

      desc.Filter = D3D10_FILTER_MIN_MAG_MIP_LINEAR;
      D3D10CreateSamplerState(d3d10->device, &desc, &d3d10->sampler_linear);
   }

   {
      d3d10_vertex_t vertices[] = {
         { { -1.0f, -1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
         { { -1.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
         { { 1.0f, -1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
         { { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
      };

      {
         D3D10_BUFFER_DESC desc = {
            .ByteWidth      = sizeof(vertices),
            .Usage          = D3D10_USAGE_DYNAMIC,
            .BindFlags      = D3D10_BIND_VERTEX_BUFFER,
            .CPUAccessFlags = D3D10_CPU_ACCESS_WRITE,
         };
         D3D10_SUBRESOURCE_DATA vertexData = { vertices };
         D3D10CreateBuffer(d3d10->device, &desc, &vertexData, &d3d10->frame.vbo);
         desc.Usage          = D3D10_USAGE_IMMUTABLE;
         desc.CPUAccessFlags = 0;
         D3D10CreateBuffer(d3d10->device, &desc, &vertexData, &d3d10->menu.vbo);
      }
      D3D10SetPrimitiveTopology(d3d10->device, D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
   }

   {
      D3DBlob vs_code;
      D3DBlob ps_code;

      static const char stock[] =
#include "d3d_shaders/opaque_sm5.hlsl.h"
            ;

      D3D10_INPUT_ELEMENT_DESC desc[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d10_vertex_t, position),
           D3D10_INPUT_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d10_vertex_t, texcoord),
           D3D10_INPUT_PER_VERTEX_DATA, 0 },
         { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d10_vertex_t, color),
           D3D10_INPUT_PER_VERTEX_DATA, 0 },
      };

      d3d_compile(stock, sizeof(stock), "VSMain", "vs_4_0", &vs_code);
      d3d_compile(stock, sizeof(stock), "PSMain", "ps_4_0", &ps_code);

      D3D10CreateVertexShader(
            d3d10->device, D3DGetBufferPointer(vs_code), D3DGetBufferSize(vs_code), &d3d10->vs);
      D3D10CreatePixelShader(
            d3d10->device, D3DGetBufferPointer(ps_code), D3DGetBufferSize(ps_code), &d3d10->ps);
      D3D10CreateInputLayout(
            d3d10->device, desc, countof(desc), D3DGetBufferPointer(vs_code),
            D3DGetBufferSize(vs_code), &d3d10->layout);

      Release(vs_code);
      Release(ps_code);
   }

   D3D10SetInputLayout(d3d10->device, d3d10->layout);
   D3D10SetVShader(d3d10->device, d3d10->vs);
   D3D10SetPShader(d3d10->device, d3d10->ps);

   {
      D3D10_BLEND_DESC blend_desc = {
         .AlphaToCoverageEnable = FALSE,
         .BlendEnable           = { TRUE },
         D3D10_BLEND_SRC_ALPHA,
         D3D10_BLEND_INV_SRC_ALPHA,
         D3D10_BLEND_OP_ADD,
         D3D10_BLEND_SRC_ALPHA,
         D3D10_BLEND_INV_SRC_ALPHA,
         D3D10_BLEND_OP_ADD,
         { D3D10_COLOR_WRITE_ENABLE_ALL },
      };
      D3D10CreateBlendState(d3d10->device, &blend_desc, &d3d10->blend_enable);
      blend_desc.BlendEnable[0] = FALSE;
      D3D10CreateBlendState(d3d10->device, &blend_desc, &d3d10->blend_disable);
   }

   return d3d10;

error:
   free(d3d10);
   return NULL;
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
   (void)msg;

   d3d10_video_t* d3d10 = (d3d10_video_t*)data;
   PERF_START();
   D3D10ClearRenderTargetView(d3d10->device, d3d10->renderTargetView, d3d10->clearcolor);

   if (frame && width && height)
   {
      D3D10_BOX frame_box = { 0, 0, 0, width, height, 1 };

      if (d3d10->frame.texture.desc.Width != width || d3d10->frame.texture.desc.Height != height)
      {
         d3d10->frame.texture.desc.Width  = width;
         d3d10->frame.texture.desc.Height = height;
         d3d10_init_texture(d3d10->device, &d3d10->frame.texture);
      }

      d3d10_update_texture(width, height, pitch, d3d10->format, frame, &d3d10->frame.texture);
      D3D10CopyTexture2DSubresourceRegion(
            d3d10->device, d3d10->frame.texture.handle, 0, 0, 0, 0, d3d10->frame.texture.staging, 0,
            &frame_box);
   }

   {
      UINT stride = sizeof(d3d10_vertex_t);
      UINT offset = 0;
      D3D10SetVertexBuffers(d3d10->device, 0, 1, &d3d10->frame.vbo, &stride, &offset);
      D3D10SetPShaderResources(d3d10->device, 0, 1, &d3d10->frame.texture.view);
      D3D10SetPShaderSamplers(d3d10->device, 0, 1, &d3d10->sampler_linear);

      D3D10SetBlendState(d3d10->device, d3d10->blend_disable, NULL, D3D10_DEFAULT_SAMPLE_MASK);
      D3D10Draw(d3d10->device, 4, 0);
      D3D10SetBlendState(d3d10->device, d3d10->blend_enable, NULL, D3D10_DEFAULT_SAMPLE_MASK);

      if (d3d10->menu.enabled)
      {
         if (d3d10->menu.texture.dirty)
            D3D10CopyTexture2DSubresourceRegion(
                  d3d10->device, d3d10->menu.texture.handle, 0, 0, 0, 0,
                  d3d10->menu.texture.staging, 0, NULL);

         D3D10SetVertexBuffers(d3d10->device, 0, 1, &d3d10->menu.vbo, &stride, &offset);
         D3D10SetPShaderResources(d3d10->device, 0, 1, &d3d10->menu.texture.view);
         D3D10SetPShaderSamplers(d3d10->device, 0, 1, &d3d10->sampler_linear);

         D3D10Draw(d3d10->device, 4, 0);
      }
   }

   DXGIPresent(d3d10->swapChain, !d3d10->vsync, 0);
   PERF_STOP();

   if (msg && *msg)
      gfx_ctx_d3d.update_window_title(NULL, video_info);

   return true;
}

static void d3d10_gfx_set_nonblock_state(void* data, bool toggle)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;
   d3d10->vsync         = !toggle;
}

static bool d3d10_gfx_alive(void* data)
{
   (void)data;
   bool     quit;
   bool     resize;
   unsigned width;
   unsigned height;

   win32_check_window(&quit, &resize, &width, &height);

   if (width != 0 && height != 0)
      video_driver_set_size(&width, &height);

   return !quit;
}

static bool d3d10_gfx_focus(void* data) { return win32_has_focus(); }

static bool d3d10_gfx_suppress_screensaver(void* data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool d3d10_gfx_has_windowed(void* data)
{
   (void)data;
   return true;
}

static void d3d10_gfx_free(void* data)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   Release(d3d10->frame.texture.view);
   Release(d3d10->frame.texture.handle);
   Release(d3d10->frame.texture.staging);
   Release(d3d10->frame.vbo);

   Release(d3d10->menu.texture.handle);
   Release(d3d10->menu.texture.staging);
   Release(d3d10->menu.texture.view);
   Release(d3d10->menu.vbo);

   Release(d3d10->blend_enable);
   Release(d3d10->blend_disable);
   Release(d3d10->sampler_nearest);
   Release(d3d10->sampler_linear);
   Release(d3d10->ps);
   Release(d3d10->vs);
   Release(d3d10->layout);
   Release(d3d10->renderTargetView);
   Release(d3d10->swapChain);
   Release(d3d10->device);

   win32_monitor_from_window();
   win32_destroy_window();
   free(d3d10);
}

static bool d3d10_gfx_set_shader(void* data, enum rarch_shader_type type, const char* path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void d3d10_gfx_set_rotation(void* data, unsigned rotation)
{
   (void)data;
   (void)rotation;
}

static void d3d10_gfx_viewport_info(void* data, struct video_viewport* vp)
{
   (void)data;
   (void)vp;
}

static bool d3d10_gfx_read_viewport(void* data, uint8_t* buffer, bool is_idle)
{
   (void)data;
   (void)buffer;

   return true;
}

static void d3d10_set_menu_texture_frame(
      void* data, const void* frame, bool rgb32, unsigned width, unsigned height, float alpha)
{
   d3d10_video_t* d3d10  = (d3d10_video_t*)data;
   int            pitch  = width * (rgb32 ? sizeof(uint32_t) : sizeof(uint16_t));
   DXGI_FORMAT    format = rgb32 ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_EX_A4R4G4B4_UNORM;

   if (d3d10->menu.texture.desc.Width != width || d3d10->menu.texture.desc.Height != height)
   {
      d3d10->menu.texture.desc.Format = d3d10_get_closest_match_texture2D(d3d10->device, format);
      d3d10->menu.texture.desc.Width  = width;
      d3d10->menu.texture.desc.Height = height;
      d3d10_init_texture(d3d10->device, &d3d10->menu.texture);
   }

   d3d10_update_texture(width, height, pitch, format, frame, &d3d10->menu.texture);
}
static void d3d10_set_menu_texture_enable(void* data, bool state, bool full_screen)
{
   d3d10_video_t* d3d10 = (d3d10_video_t*)data;

   d3d10->menu.enabled    = state;
   d3d10->menu.fullscreen = full_screen;
}

static const video_poke_interface_t d3d10_poke_interface = {
   NULL,                          /* set_coords */
   NULL,                          /* set_mvp */
   NULL,                          /* load_texture */
   NULL,                          /* unload_texture */
   NULL,                          /* set_video_mode */
   NULL,                          /* set_filtering */
   NULL,                          /* get_video_output_size */
   NULL,                          /* get_video_output_prev */
   NULL,                          /* get_video_output_next */
   NULL,                          /* get_current_framebuffer */
   NULL,                          /* get_proc_address */
   NULL,                          /* set_aspect_ratio */
   NULL,                          /* apply_state_changes */
   d3d10_set_menu_texture_frame,  /* set_texture_frame */
   d3d10_set_menu_texture_enable, /* set_texture_enable */
   NULL,                          /* set_osd_msg */
   NULL,                          /* show_mouse */
   NULL,                          /* grab_mouse_toggle */
   NULL,                          /* get_current_shader */
   NULL,                          /* get_current_software_framebuffer */
   NULL,                          /* get_hw_render_interface */
};

static void d3d10_gfx_get_poke_interface(void* data, const video_poke_interface_t** iface)
{
   *iface = &d3d10_poke_interface;
}

video_driver_t video_d3d10 = {
   d3d10_gfx_init,
   d3d10_gfx_frame,
   d3d10_gfx_set_nonblock_state,
   d3d10_gfx_alive,
   d3d10_gfx_focus,
   d3d10_gfx_suppress_screensaver,
   d3d10_gfx_has_windowed,
   d3d10_gfx_set_shader,
   d3d10_gfx_free,
   "d3d10",
   NULL, /* set_viewport */
   d3d10_gfx_set_rotation,
   d3d10_gfx_viewport_info,
   d3d10_gfx_read_viewport,
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
#endif
   d3d10_gfx_get_poke_interface,
};
