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

#include "d3d10_defines.h"
#include "d3dcompiler_common.h"

#if defined(HAVE_DYLIB) && !defined(__WINRT__)
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
      if (!(d3d10_dll = dylib_load("d3d10.dll")))
         return TYPE_E_CANTLOADLIBRARY;
   if (!fp)
      if (!(fp = (PFN_D3D10_CREATE_DEVICE_AND_SWAP_CHAIN)dylib_proc(
            d3d10_dll, "D3D10CreateDeviceAndSwapChain")))
         return TYPE_E_DLLFUNCTIONNOTFOUND;
   return fp(
         pAdapter, DriverType, Software, Flags, SDKVersion,
         pSwapChainDesc, ppSwapChain, ppDevice);
}
#endif
