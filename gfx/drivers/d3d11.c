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
#include "gfx/common/d3d11_common.h"
#include "gfx/common/dxgi_common.h"
#include "gfx/common/d3dcompiler_common.h"

typedef struct d3d11_vertex_t
{
   float position[2];
   float texcoord[2];
   float color[4];
} d3d11_vertex_t;

typedef struct
{
   unsigned cur_mon_id;
   DXGISwapChain swapChain;
   D3D11Device device;
   D3D_FEATURE_LEVEL supportedFeatureLevel;
   D3D11DeviceContext context;
   D3D11RenderTargetView renderTargetView;
   D3D11Buffer vertexBuffer;
   D3D11InputLayout layout;
   D3D11VertexShader vs;
   D3D11PixelShader ps;
   D3D11SamplerState sampler_nearest;
   D3D11SamplerState sampler_linear;
   D3D11Texture2D frame_default;
   D3D11Texture2D frame_staging;
   struct
   {
      D3D11Texture2D tex;
      D3D11ShaderResourceView view;
      D3D11Buffer vbo;
      int width;
      int height;
      bool enabled;
      bool fullscreen;
   } menu;
   int frame_width;
   int frame_height;
   D3D11ShaderResourceView frame_view;
   float clearcolor[4];
   bool vsync;
   bool rgb32;
} d3d11_video_t;

static void* d3d11_gfx_init(const video_info_t* video,
      const input_driver_t** input, void** input_data)
{
   WNDCLASSEX wndclass  = {0};
   settings_t* settings = config_get_ptr();
   gfx_ctx_input_t inp  = {input, input_data};
   d3d11_video_t* d3d11 = (d3d11_video_t*)calloc(1, sizeof(*d3d11));

   if (!d3d11)
      return NULL;

   win32_window_reset();
   win32_monitor_init();
   wndclass.lpfnWndProc = WndProcD3D;
   win32_window_init(&wndclass, true, NULL);

   if (!win32_set_video_mode(d3d11, video->width,
            video->height, video->fullscreen))
   {
      RARCH_ERR("[D3D11]: win32_set_video_mode failed.\n");
      goto error;
   }

   gfx_ctx_d3d.input_driver(NULL, settings->arrays.input_joypad_driver,
         input, input_data);

   d3d11->rgb32 = video->rgb32;
   d3d11->vsync = video->vsync;

   {
      UINT flags = 0;
      D3D_FEATURE_LEVEL requested_feature_level = D3D_FEATURE_LEVEL_11_0;
      DXGI_SWAP_CHAIN_DESC desc =
      {
         .BufferCount                        = 1,
         .BufferDesc.Width                   = video->width,
         .BufferDesc.Height                  = video->height,
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
      flags                                 |= D3D11_CREATE_DEVICE_DEBUG;
#endif

      D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE,
            NULL, flags,
            &requested_feature_level, 1, D3D11_SDK_VERSION, &desc,
            (IDXGISwapChain**)&d3d11->swapChain, &d3d11->device,
            &d3d11->supportedFeatureLevel, &d3d11->context);
   }

   {
      D3D11Texture2D backBuffer;
      DXGIGetSwapChainBufferD3D11(d3d11->swapChain, 0, &backBuffer);
      D3D11CreateTexture2DRenderTargetView(d3d11->device, backBuffer,
            NULL, &d3d11->renderTargetView);
      Release(backBuffer);
   }

   D3D11SetRenderTargets(d3d11->context, 1,
         &d3d11->renderTargetView, NULL);
   {
      D3D11_VIEWPORT vp = {0, 0, video->width, video->height, 0.0f, 1.0f};
      D3D11SetViewports(d3d11->context, 1, &vp);
   }


   d3d11->frame_width  = video->input_scale * RARCH_SCALE_BASE;
   d3d11->frame_height = video->input_scale * RARCH_SCALE_BASE;
   {
      D3D11_TEXTURE2D_DESC desc =
      {
         .Width              = d3d11->frame_width,
         .Height             = d3d11->frame_height,
         .MipLevels          = 1,
         .ArraySize          = 1,
         .Format             = DXGI_FORMAT_B8G8R8A8_UNORM,
#if 0
         .Format             = d3d11->rgb32 ? 
            DXGI_FORMAT_B8G8R8X8_UNORM : DXGI_FORMAT_B5G6R5_UNORM,
#endif
         .SampleDesc.Count   = 1,
         .SampleDesc.Quality = 0,
         .Usage              = D3D11_USAGE_DEFAULT,
         .BindFlags          = D3D11_BIND_SHADER_RESOURCE,
         .CPUAccessFlags     = 0,
         .MiscFlags          = 0,
      };
      D3D11_SHADER_RESOURCE_VIEW_DESC view_desc =
      {
         .Format                    = desc.Format,
         .ViewDimension             = D3D_SRV_DIMENSION_TEXTURE2D,
         .Texture2D.MostDetailedMip = 0,
         .Texture2D.MipLevels       = -1,
      };

      D3D11CreateTexture2D(d3d11->device,
            &desc, NULL, &d3d11->frame_default);
      D3D11CreateTexture2DShaderResourceView(d3d11->device,
            d3d11->frame_default, &view_desc, &d3d11->frame_view);

      desc.Format         = desc.Format;
      desc.BindFlags      = 0;
      desc.Usage          = D3D11_USAGE_STAGING;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

      D3D11CreateTexture2D(d3d11->device, &desc,
            NULL, &d3d11->frame_staging);
   }

   {
      D3D11_SAMPLER_DESC desc =
      {
         .Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT,
         .AddressU       = D3D11_TEXTURE_ADDRESS_BORDER,
         .AddressV       = D3D11_TEXTURE_ADDRESS_BORDER,
         .AddressW       = D3D11_TEXTURE_ADDRESS_BORDER,
         .MaxAnisotropy  = 1,
         .ComparisonFunc = D3D11_COMPARISON_NEVER,
         .MinLOD         = -D3D11_FLOAT32_MAX,
         .MaxLOD         = D3D11_FLOAT32_MAX,
      };
      D3D11CreateSamplerState(d3d11->device, &desc, &d3d11->sampler_nearest);

      desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
      D3D11CreateSamplerState(d3d11->device, &desc, &d3d11->sampler_linear);
   }

   {
      d3d11_vertex_t vertices[] =
      {
         {{ -1.0f, -1.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
         {{ -1.0f,  1.0f}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
         {{ 1.0f, -1.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
         {{ 1.0f,  1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 1.0f}},
      };

      {
         D3D11_BUFFER_DESC desc =
         {
            .Usage               = D3D11_USAGE_DYNAMIC,
            .ByteWidth           = sizeof(vertices),
            .BindFlags           = D3D11_BIND_VERTEX_BUFFER,
            .StructureByteStride = 0, /* sizeof(Vertex) ? */
            .CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE,
         };
         D3D11_SUBRESOURCE_DATA vertexData = {vertices};
         D3D11CreateBuffer(d3d11->device, &desc, &vertexData, &d3d11->vertexBuffer);
         desc.Usage          = D3D11_USAGE_IMMUTABLE;
         desc.CPUAccessFlags = 0;

         D3D11CreateBuffer(d3d11->device, &desc,
               &vertexData, &d3d11->menu.vbo);
      }
      D3D11SetPrimitiveTopology(d3d11->context,
            D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
   }

   {
      D3DBlob vs_code;
      D3DBlob ps_code;

      static const char stock [] =
#include "d3d_shaders/opaque_sm5.hlsl.h"
      ;

      D3D11_INPUT_ELEMENT_DESC desc[] =
      {
         {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(d3d11_vertex_t, position), D3D11_INPUT_PER_VERTEX_DATA, 0},
         {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(d3d11_vertex_t, texcoord), D3D11_INPUT_PER_VERTEX_DATA, 0},
         {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d11_vertex_t, color),    D3D11_INPUT_PER_VERTEX_DATA, 0},
      };

      d3d_compile(stock, sizeof(stock), "VSMain", "vs_5_0", &vs_code);
      d3d_compile(stock, sizeof(stock), "PSMain", "ps_5_0", &ps_code);

      D3D11CreateVertexShader(d3d11->device,
            D3DGetBufferPointer(vs_code), D3DGetBufferSize(vs_code),
            NULL, &d3d11->vs);
      D3D11CreatePixelShader(d3d11->device,
            D3DGetBufferPointer(ps_code), D3DGetBufferSize(ps_code),
            NULL, &d3d11->ps);
      D3D11CreateInputLayout(d3d11->device, desc, countof(desc),
            D3DGetBufferPointer(vs_code),
            D3DGetBufferSize(vs_code), &d3d11->layout);

      Release(vs_code);
      Release(ps_code);
   }

   D3D11SetInputLayout(d3d11->context, d3d11->layout);
   D3D11SetVShader(d3d11->context, d3d11->vs, NULL, 0);
   D3D11SetPShader(d3d11->context, d3d11->ps, NULL, 0);

   return d3d11;

error:
   free(d3d11);
   return NULL;
}

static bool d3d11_gfx_frame(void* data, const void* frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char* msg, video_frame_info_t* video_info)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   (void)msg;

   if(frame && width && height)
   {
      D3D11_MAPPED_SUBRESOURCE mapped_frame;
      D3D11MapTexture2D(d3d11->context,
            d3d11->frame_staging, 0, D3D11_MAP_WRITE, 0, &mapped_frame);
      {
         unsigned i, j;

         if (d3d11->rgb32)
         {
            const uint8_t *in  = frame;
            uint8_t       *out = mapped_frame.pData;

            for (i = 0; i < height; i++)
            {
               memcpy(out, in, width * (d3d11->rgb32? 4 : 2));
               in  += pitch;
               out += mapped_frame.RowPitch;
            }
         }
         else
         {
            const uint16_t *in  = frame;
            uint32_t       *out = mapped_frame.pData;

            for (i = 0; i < height; i++)
            {
               for (j = 0; j < width; j++)
               {
                  unsigned b = ((in[j] >> 11) & 0x1F);
                  unsigned g = ((in[j] >>  5) & 0x3F);
                  unsigned r = ((in[j] >>  0) & 0x1F);

                  out[j] = ((r >> 2) << 0) | (r << 3) | 
                     ((g >> 4) << 8)  | (g << 10) | 
                     ((b >> 2) << 16) | (b << 19);

               }
               in  += width;
               out += mapped_frame.RowPitch / 4;
            }
         }

      }
      D3D11UnmapTexture2D(d3d11->context, d3d11->frame_staging, 0);
      D3D11_BOX frame_box = {0, 0, 0, width, height, 1};
      D3D11CopyTexture2DSubresourceRegion(d3d11->context,
            d3d11->frame_default, 0, 0 , 0 , 0,
            d3d11->frame_staging, 0, &frame_box);

      {
         D3D11_MAPPED_SUBRESOURCE mapped_vbo;
         D3D11MapBuffer(d3d11->context, d3d11->vertexBuffer,
               0, D3D11_MAP_WRITE_NO_OVERWRITE, 0,
               &mapped_vbo);
         {
            d3d11_vertex_t* vbo = mapped_vbo.pData;
            vbo[0].texcoord[0]  = 0.0f * (width / (float)d3d11->frame_width);
            vbo[0].texcoord[1]  = 1.0f * (height / (float)d3d11->frame_height);
            vbo[1].texcoord[0]  = 0.0f * (width / (float)d3d11->frame_width);
            vbo[1].texcoord[1]  = 0.0f * (height / (float)d3d11->frame_height);
            vbo[2].texcoord[0]  = 1.0f * (width / (float)d3d11->frame_width);
            vbo[2].texcoord[1]  = 1.0f * (height / (float)d3d11->frame_height);
            vbo[3].texcoord[0]  = 1.0f * (width / (float)d3d11->frame_width);
            vbo[3].texcoord[1]  = 0.0f * (height / (float)d3d11->frame_height);
         }
         D3D11UnmapBuffer(d3d11->context, d3d11->vertexBuffer, 0);
      }
   }

   D3D11ClearRenderTargetView(d3d11->context,
         d3d11->renderTargetView, d3d11->clearcolor);

   {
      UINT stride = sizeof(d3d11_vertex_t);
      UINT offset = 0;
      D3D11SetVertexBuffers(d3d11->context, 0, 1,
            &d3d11->vertexBuffer, &stride, &offset);
      D3D11SetPShaderResources(d3d11->context, 0, 1, &d3d11->frame_view);
      D3D11SetPShaderSamplers(d3d11->context, 0, 1, &d3d11->sampler_linear);

      D3D11Draw(d3d11->context, 4, 0);

      if (d3d11->menu.enabled)
      {
         D3D11SetVertexBuffers(d3d11->context, 0, 1,
               &d3d11->menu.vbo, &stride, &offset);
         D3D11SetPShaderResources(d3d11->context, 0, 1,
               &d3d11->menu.view);
         D3D11SetPShaderSamplers(d3d11->context, 0, 1,
               &d3d11->sampler_linear);

         D3D11Draw(d3d11->context, 4, 0);
      }
   }

   DXGIPresent(d3d11->swapChain, !!d3d11->vsync, 0);

   if(msg && *msg)
      gfx_ctx_d3d.update_window_title(NULL, video_info);

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
   bool quit;
   bool resize;
   unsigned width;
   unsigned height;

   (void)data;

   win32_check_window(&quit, &resize, &width, &height);

   if (width != 0 && height != 0)
      video_driver_set_size(&width, &height);

   return !quit;
}

static bool d3d11_gfx_focus(void* data)
{
   return win32_has_focus();
}

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

static void d3d11_gfx_free(void* data)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   if (!d3d11)
      return;

   if(d3d11->menu.tex)
      Release(d3d11->menu.tex);
   if(d3d11->menu.view)
      Release(d3d11->menu.view);

   Release(d3d11->menu.vbo);

   Release(d3d11->sampler_nearest);
   Release(d3d11->sampler_linear);
   Release(d3d11->frame_view);
   Release(d3d11->frame_default);
   Release(d3d11->frame_staging);
   Release(d3d11->ps);
   Release(d3d11->vs);
   Release(d3d11->vertexBuffer);
   Release(d3d11->layout);
   Release(d3d11->renderTargetView);
   Release(d3d11->swapChain);
   Release(d3d11->context);
   Release(d3d11->device);

   win32_monitor_from_window();
   win32_destroy_window();
   free(d3d11);
}

static bool d3d11_gfx_set_shader(void* data,
      enum rarch_shader_type type, const char* path)
{
   (void)data;
   (void)type;
   (void)path;

   return false;
}

static void d3d11_gfx_set_rotation(void* data,
      unsigned rotation)
{
   (void)data;
   (void)rotation;
}

static void d3d11_gfx_viewport_info(void* data,
      struct video_viewport* vp)
{
   (void)data;
   (void)vp;
}

static bool d3d11_gfx_read_viewport(void* data,
      uint8_t* buffer, bool is_idle)
{
   (void)data;
   (void)buffer;

   return true;
}

static void d3d11_set_menu_texture_frame(void* data,
      const void* frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   if (   d3d11->menu.tex && 
         (d3d11->menu.width != width || 
          d3d11->menu.height != height))
   {
      Release(d3d11->menu.tex);
      d3d11->menu.tex = NULL;
   }

   if (!d3d11->menu.tex)
   {
      d3d11->menu.width = width;
      d3d11->menu.height = height;

      D3D11_TEXTURE2D_DESC desc =
      {
         .Width              = d3d11->menu.width,
         .Height             = d3d11->menu.height,
         .MipLevels          = 1,
         .ArraySize          = 1,
         .Format             = DXGI_FORMAT_R8G8B8A8_UNORM,
         .SampleDesc.Count   = 1,
         .SampleDesc.Quality = 0,
         .Usage              = D3D11_USAGE_DYNAMIC,
         .BindFlags          = D3D11_BIND_SHADER_RESOURCE,
         .CPUAccessFlags     = D3D11_CPU_ACCESS_WRITE,
         .MiscFlags          = 0,
      };
      D3D11_SHADER_RESOURCE_VIEW_DESC view_desc =
      {
         .Format                    = desc.Format,
         .ViewDimension             = D3D_SRV_DIMENSION_TEXTURE2D,
         .Texture2D.MostDetailedMip = 0,
         .Texture2D.MipLevels       = -1,
      };

      D3D11CreateTexture2D(d3d11->device, &desc, NULL, &d3d11->menu.tex);
      D3D11CreateTexture2DShaderResourceView(d3d11->device,
            d3d11->menu.tex, &view_desc, &d3d11->menu.view);
   }

   {
      D3D11_MAPPED_SUBRESOURCE mapped_frame;
      D3D11MapTexture2D(d3d11->context,
            d3d11->menu.tex, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_frame);
      {
         unsigned i, j;

         if (rgb32)
         {
            const uint32_t *in  = frame;
            uint32_t       *out = mapped_frame.pData;

            for (i = 0; i < height; i++)
            {
               memcpy(out, in, width * 4);
               in  += width;
               out += mapped_frame.RowPitch / 4;
            }
         }
         else
         {
            const uint16_t *in  = frame;
            uint32_t       *out = mapped_frame.pData;

            for (i = 0; i < height; i++)
            {
               for (j = 0; j < width; j++)
               {
                  unsigned r = ((in[j] >> 12) & 0xF);
                  unsigned g = ((in[j] >>  8) & 0xF);
                  unsigned b = ((in[j] >>  4) & 0xF);
                  unsigned a = ((in[j] >>  0) & 0xF);

                  out[j] = (r << 0) | (r << 4) | (g << 8) 
                     | (g << 12) |(b << 16) | (b << 20) 
                     | (a << 24) | (a << 28);

               }
               in  += width;
               out += mapped_frame.RowPitch / 4;
            }
         }

      }
      D3D11UnmapTexture2D(d3d11->context, d3d11->menu.tex, 0);
   }
}
static void d3d11_set_menu_texture_enable(void* data,
      bool state, bool full_screen)
{
   d3d11_video_t* d3d11 = (d3d11_video_t*)data;

   if (!d3d11)
      return;

   d3d11->menu.enabled            = state;
   d3d11->menu.fullscreen         = full_screen;
}

static const video_poke_interface_t d3d11_poke_interface =
{
   NULL, /* set_coords */
   NULL, /* set_mvp */
   NULL, /* load_texture */
   NULL, /* unload_texture */
   NULL, /* set_video_mode */
   NULL, /* set_filtering */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   NULL, /* set_aspect_ratio */
   NULL, /* apply_state_changes */
   d3d11_set_menu_texture_frame, /* set_texture_frame */
   d3d11_set_menu_texture_enable, /* set_texture_enable */
   NULL, /* set_osd_msg */
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
};

static void d3d11_gfx_get_poke_interface(void* data,
      const video_poke_interface_t** iface)
{
   *iface = &d3d11_poke_interface;
}

video_driver_t video_d3d11 =
{
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
