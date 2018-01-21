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

#include "d3d11_common.h"

#include <dynamic/dylib.h>

static dylib_t d3d11_dll;

HRESULT WINAPI D3D11CreateDeviceAndSwapChain( IDXGIAdapter* pAdapter,D3D_DRIVER_TYPE DriverType, HMODULE Software, UINT Flags,
    CONST D3D_FEATURE_LEVEL* pFeatureLevels, UINT FeatureLevels, UINT SDKVersion, CONST DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
    IDXGISwapChain** ppSwapChain, ID3D11Device** ppDevice, D3D_FEATURE_LEVEL* pFeatureLevel, ID3D11DeviceContext** ppImmediateContext)
{
   static PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN fp;

   if(!d3d11_dll)
      d3d11_dll = dylib_load("d3d11.dll");

   if(!d3d11_dll)
      return TYPE_E_CANTLOADLIBRARY;

   if(!fp)
      fp = (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)dylib_proc(d3d11_dll, "D3D11CreateDeviceAndSwapChain");

   if(!fp)
      return TYPE_E_CANTLOADLIBRARY;

   return fp(pAdapter,DriverType,Software, Flags,pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc,
             ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);

}
