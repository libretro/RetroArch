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

#include <string.h>

#include "d3d11_common.h"
#include "d3dcompiler_common.h"

#if defined(HAVE_DYNAMIC) && !defined(__WINRT__)
#include <dynamic/dylib.h>

HRESULT WINAPI D3D11CreateDevice(
      IDXGIAdapter*   pAdapter,
      D3D_DRIVER_TYPE DriverType,
      HMODULE         Software,
      UINT            Flags,
      CONST D3D_FEATURE_LEVEL* pFeatureLevels,
      UINT                     FeatureLevels,
      UINT                     SDKVersion,
      ID3D11Device**              ppDevice,
      D3D_FEATURE_LEVEL*          pFeatureLevel,
      ID3D11DeviceContext**       ppImmediateContext)
{
   static dylib_t                                d3d11_dll;
   static PFN_D3D11_CREATE_DEVICE                fp;

   if (!d3d11_dll)
      if (!(d3d11_dll = dylib_load("d3d11.dll")))
         return TYPE_E_CANTLOADLIBRARY;
   if (!fp)
      if (!(fp = (PFN_D3D11_CREATE_DEVICE)dylib_proc(
            d3d11_dll, "D3D11CreateDevice")))
         return TYPE_E_DLLFUNCTIONNOTFOUND;
   return fp(
         pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
         ppDevice, pFeatureLevel, ppImmediateContext);
}

HRESULT WINAPI D3D11CreateDeviceAndSwapChain(
      IDXGIAdapter*   pAdapter,
      D3D_DRIVER_TYPE DriverType,
      HMODULE         Software,
      UINT            Flags,
      CONST D3D_FEATURE_LEVEL* pFeatureLevels,
      UINT                     FeatureLevels,
      UINT                     SDKVersion,
      CONST DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
      IDXGISwapChain**            ppSwapChain,
      ID3D11Device**              ppDevice,
      D3D_FEATURE_LEVEL*          pFeatureLevel,
      ID3D11DeviceContext**       ppImmediateContext)
{
   static dylib_t                                d3d11_dll;
   static PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN fp;

   if (!d3d11_dll)
      if (!(d3d11_dll = dylib_load("d3d11.dll")))
         return TYPE_E_CANTLOADLIBRARY;
   if (!fp)
      if (!(fp = (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)dylib_proc(
            d3d11_dll, "D3D11CreateDeviceAndSwapChain")))
         return TYPE_E_DLLFUNCTIONNOTFOUND;
   return fp(
         pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion,
         pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);
}
#endif

void d3d11_init_texture(D3D11Device device, d3d11_texture_t* texture)
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

void d3d11_update_texture(
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

   DXGI_FORMAT
d3d11_get_closest_match(D3D11Device device, DXGI_FORMAT desired_format, UINT desired_format_support)
{
   DXGI_FORMAT default_list[] = {desired_format, DXGI_FORMAT_UNKNOWN};
   DXGI_FORMAT* format = dxgi_get_format_fallback_list(desired_format);

   if(!format)
      format = default_list;

   while (*format != DXGI_FORMAT_UNKNOWN)
   {
      UINT         format_support;
      if (SUCCEEDED(device->lpVtbl->CheckFormatSupport(device, *format,
                  &format_support)) &&
            ((format_support & desired_format_support) == desired_format_support))
         break;
      format++;
   }
   assert(*format);
   return *format;
}

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
