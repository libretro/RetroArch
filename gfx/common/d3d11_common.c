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
#define WIN32_LEAN_AND_MEAN

#include <string.h>

#include "d3d11_defines.h"
#include "d3dcompiler_common.h"

#if defined(HAVE_DYLIB) && !defined(__WINRT__)
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
