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

#include <gfx/scaler/pixconv.h>

#include "d3d10_common.h"

#ifdef HAVE_DYNAMIC
#include <dynamic/dylib.h>

typedef HRESULT(WINAPI* PFN_D3D10_CREATE_DEVICE_AND_SWAP_CHAIN)(
      IDXGIAdapter*         pAdapter,
      D3D10_DRIVER_TYPE     DriverType,
      HMODULE               Software,
      UINT                  Flags,
      UINT                  SDKVersion,
      DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
      IDXGISwapChain**      ppSwapChain,
      ID3D10Device**        ppDevice);

HRESULT WINAPI D3D10CreateDeviceAndSwapChain(
      IDXGIAdapter*         pAdapter,
      D3D10_DRIVER_TYPE     DriverType,
      HMODULE               Software,
      UINT                  Flags,
      UINT                  SDKVersion,
      DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
      IDXGISwapChain**      ppSwapChain,
      ID3D10Device**        ppDevice)

{
   static dylib_t d3d10_dll;
   static PFN_D3D10_CREATE_DEVICE_AND_SWAP_CHAIN fp;

   if (!d3d10_dll)
      d3d10_dll = dylib_load("d3d10.dll");

   if (!d3d10_dll)
      return TYPE_E_CANTLOADLIBRARY;

   if (!fp)
      fp = (PFN_D3D10_CREATE_DEVICE_AND_SWAP_CHAIN)dylib_proc(
            d3d10_dll, "D3D10CreateDeviceAndSwapChain");

   if (!fp)
      return TYPE_E_CANTLOADLIBRARY;

   return fp(
         pAdapter, DriverType, Software, Flags, SDKVersion,
         pSwapChainDesc, ppSwapChain, ppDevice);
}
#endif

void d3d10_init_texture(D3D10Device device, d3d10_texture_t* texture)
{
   Release(texture->handle);
   Release(texture->staging);
   Release(texture->view);

   //   .Usage = D3D10_USAGE_DYNAMIC,
   //   .CPUAccessFlags = D3D10_CPU_ACCESS_WRITE,

   texture->desc.MipLevels          = 1;
   texture->desc.ArraySize          = 1;
   texture->desc.SampleDesc.Count   = 1;
   texture->desc.SampleDesc.Quality = 0;
   texture->desc.BindFlags          = D3D10_BIND_SHADER_RESOURCE;
   texture->desc.CPUAccessFlags     = 0;
   texture->desc.MiscFlags          = 0;
   D3D10CreateTexture2D(device, &texture->desc, NULL, &texture->handle);

   {
      D3D10_SHADER_RESOURCE_VIEW_DESC view_desc = { 0 };
      view_desc.Format                    = texture->desc.Format;
      view_desc.ViewDimension             = D3D_SRV_DIMENSION_TEXTURE2D;
      view_desc.Texture2D.MostDetailedMip = 0;
      view_desc.Texture2D.MipLevels       = -1;

      D3D10CreateTexture2DShaderResourceView(device,
            texture->handle, &view_desc, &texture->view);
   }

   {
      D3D10_TEXTURE2D_DESC desc = texture->desc;
      desc.BindFlags            = 0;
      desc.Usage                = D3D10_USAGE_STAGING;
      desc.CPUAccessFlags       = D3D10_CPU_ACCESS_WRITE;
      D3D10CreateTexture2D(device, &desc, NULL, &texture->staging);
   }
}

void d3d10_update_texture(
      int              width,
      int              height,
      int              pitch,
      DXGI_FORMAT      format,
      const void*      data,
      d3d10_texture_t* texture)
{
   D3D10_MAPPED_TEXTURE2D mapped_texture;

   D3D10MapTexture2D(texture->staging, 0, D3D10_MAP_WRITE, 0,
         &mapped_texture);

#if 0
   PERF_START();
   conv_rgb565_argb8888(mapped_texture.pData, data, width, height,
         mapped_texture.RowPitch, pitch);
   PERF_STOP();
#else
   dxgi_copy(
         width, height, format, pitch, data, texture->desc.Format,
         mapped_texture.RowPitch,
         mapped_texture.pData);
#endif

   D3D10UnmapTexture2D(texture->staging, 0);

   if (texture->desc.Usage == D3D10_USAGE_DEFAULT)
      texture->dirty = true;
}

DXGI_FORMAT
d3d10_get_closest_match(D3D10Device device,
      DXGI_FORMAT desired_format, UINT desired_format_support)
{
   DXGI_FORMAT default_list[] = {desired_format, DXGI_FORMAT_UNKNOWN};
   DXGI_FORMAT* format = dxgi_get_format_fallback_list(desired_format);

   if(!format)
      format = default_list;

   while (*format != DXGI_FORMAT_UNKNOWN)
   {
      UINT format_support;
      if (SUCCEEDED(D3D10CheckFormatSupport(device, *format, &format_support)) &&
          ((format_support & desired_format_support) == desired_format_support))
         break;
      format++;
   }
   assert(*format);
   return *format;
}
