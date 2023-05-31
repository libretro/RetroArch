/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2018 - Ali Bouhlel
 *  Copyright (C) 2016-2019 - Brad Parker
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
#define COBJMACROS

#include <boolean.h>

#include "d3d_common.h"
#include "d3d12_defines.h"
#include "dxgi_common.h"
#include "d3dcompiler_common.h"

#if defined(HAVE_DYLIB) && !defined(__WINRT__)
#include <dynamic/dylib.h>
#endif

#include <encodings/utf.h>
#include <lists/string_list.h>
#include <dxgi.h>

#ifdef __MINGW32__
#if __GNUC__ < 12
/* clang-format off */
#ifdef __cplusplus
#define DEFINE_GUIDW(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) EXTERN_C const GUID DECLSPEC_SELECTANY name = { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } }
#else
#define DEFINE_GUIDW(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) const GUID DECLSPEC_SELECTANY name = { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } }
#endif

DEFINE_GUIDW(IID_ID3D12PipelineState, 0x765a30f3, 0xf624, 0x4c6f, 0xa8, 0x28, 0xac, 0xe9, 0x48, 0x62, 0x24, 0x45);
DEFINE_GUIDW(IID_ID3D12RootSignature, 0xc54a6b66, 0x72df, 0x4ee8, 0x8b, 0xe5, 0xa9, 0x46, 0xa1, 0x42, 0x92, 0x14);
DEFINE_GUIDW(IID_ID3D12Resource, 0x696442be, 0xa72e, 0x4059, 0xbc, 0x79, 0x5b, 0x5c, 0x98, 0x04, 0x0f, 0xad);
DEFINE_GUIDW(IID_ID3D12CommandAllocator, 0x6102dee4, 0xaf59, 0x4b09, 0xb9, 0x99, 0xb4, 0x4d, 0x73, 0xf0, 0x9b, 0x24);
DEFINE_GUIDW(IID_ID3D12Fence, 0x0a753dcf, 0xc4d8, 0x4b91, 0xad, 0xf6, 0xbe, 0x5a, 0x60, 0xd9, 0x5a, 0x76);
DEFINE_GUIDW(IID_ID3D12DescriptorHeap, 0x8efb471d, 0x616c, 0x4f49, 0x90, 0xf7, 0x12, 0x7b, 0xb7, 0x63, 0xfa, 0x51);
DEFINE_GUIDW(IID_ID3D12GraphicsCommandList, 0x5b160d0f, 0xac1b, 0x4185, 0x8b, 0xa8, 0xb3, 0xae, 0x42, 0xa5, 0xa4, 0x55);
DEFINE_GUIDW(IID_ID3D12CommandQueue, 0x0ec870a6, 0x5d7e, 0x4c22, 0x8c, 0xfc, 0x5b, 0xaa, 0xe0, 0x76, 0x16, 0xed);
DEFINE_GUIDW(IID_ID3D12Device, 0x189819f1, 0x1db6, 0x4b57, 0xbe, 0x54, 0x18, 0x21, 0x33, 0x9b, 0x85, 0xf7);
DEFINE_GUIDW(IID_ID3D12Object, 0xc4fec28f, 0x7966, 0x4e95, 0x9f, 0x94, 0xf4, 0x31, 0xcb, 0x56, 0xc3, 0xb8);
DEFINE_GUIDW(IID_ID3D12DeviceChild, 0x905db94b, 0xa00c, 0x4140, 0x9d, 0xf5, 0x2b, 0x64, 0xca, 0x9e, 0xa3, 0x57);
DEFINE_GUIDW(IID_ID3D12RootSignatureDeserializer, 0x34AB647B, 0x3CC8, 0x46AC, 0x84, 0x1B, 0xC0, 0x96, 0x56, 0x45, 0xC0, 0x46);
DEFINE_GUIDW(IID_ID3D12Pageable, 0x63ee58fb, 0x1268, 0x4835, 0x86, 0xda, 0xf0, 0x08, 0xce, 0x62, 0xf0, 0xd6);
DEFINE_GUIDW(IID_ID3D12Heap, 0x6b3b2502, 0x6e51, 0x45b3, 0x90, 0xee, 0x98, 0x84, 0x26, 0x5e, 0x8d, 0xf3);
DEFINE_GUIDW(IID_ID3D12QueryHeap, 0x0d9658ae, 0xed45, 0x469e, 0xa6, 0x1d, 0x97, 0x0e, 0xc5, 0x83, 0xca, 0xb4);
DEFINE_GUIDW(IID_ID3D12CommandSignature, 0xc36a797c, 0xec80, 0x4f0a, 0x89, 0x85, 0xa7, 0xb2, 0x47, 0x50, 0x82, 0xd1);
DEFINE_GUIDW(IID_ID3D12CommandList, 0x7116d91c, 0xe7e4, 0x47ce, 0xb8, 0xc6, 0xec, 0x81, 0x68, 0xf4, 0x37, 0xe5);
DEFINE_GUIDW(IID_ID3D12PipelineLibrary, 0xc64226a8, 0x9201, 0x46af, 0xb4, 0xcc, 0x53, 0xfb, 0x9f, 0xf7, 0x41, 0x4f);
DEFINE_GUIDW(IID_ID3D12Device1, 0x77acce80, 0x638e, 0x4e65, 0x88, 0x95, 0xc1, 0xf2, 0x33, 0x86, 0x86, 0x3e);
#ifdef DEBUG
DEFINE_GUIDW(IID_ID3D12Debug, 0x344488b7, 0x6846, 0x474b, 0xb9, 0x89, 0xf0, 0x27, 0x44, 0x82, 0x45, 0xe0);
DEFINE_GUIDW(IID_ID3D12Debug1, 0xaffaa4ca, 0x63fe, 0x4d8e, 0xb8, 0xad, 0x15, 0x90, 0x00, 0xaf, 0x43, 0x04);
DEFINE_GUIDW(IID_ID3D12DebugDevice1, 0xa9b71770, 0xd099, 0x4a65, 0xa6, 0x98, 0x3d, 0xee, 0x10, 0x02, 0x0f, 0x88);
DEFINE_GUIDW(IID_ID3D12DebugDevice, 0x3febd6dd, 0x4973, 0x4787, 0x81, 0x94, 0xe4, 0x5f, 0x9e, 0x28, 0x92, 0x3e);
DEFINE_GUIDW(IID_ID3D12DebugCommandQueue, 0x09e0bf36, 0x54ac, 0x484f, 0x88, 0x47, 0x4b, 0xae, 0xea, 0xb6, 0x05, 0x3a);
DEFINE_GUIDW(IID_ID3D12DebugCommandList1, 0x102ca951, 0x311b, 0x4b01, 0xb1, 0x1f, 0xec, 0xb8, 0x3e, 0x06, 0x1b, 0x37);
DEFINE_GUIDW(IID_ID3D12DebugCommandList, 0x09e0bf36, 0x54ac, 0x484f, 0x88, 0x47, 0x4b, 0xae, 0xea, 0xb6, 0x05, 0x3f);
#endif
/* clang-format on */
#endif
#endif

#if defined(HAVE_DYLIB) && !defined(__WINRT__)
static dylib_t     d3d12_dll;

HRESULT WINAPI D3D12CreateDevice(
      IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void** ppDevice)
{
   static PFN_D3D12_CREATE_DEVICE fp;
   if (!d3d12_dll)
      if (!(d3d12_dll = dylib_load("d3d12.dll")))
         return TYPE_E_CANTLOADLIBRARY;
   if (!fp)
      if (!(fp = (PFN_D3D12_CREATE_DEVICE)dylib_proc(d3d12_dll,
                  "D3D12CreateDevice")))
         return TYPE_E_DLLFUNCTIONNOTFOUND;
   return fp(pAdapter, MinimumFeatureLevel, riid, ppDevice);
}

HRESULT WINAPI D3D12GetDebugInterface(REFIID riid, void** ppvDebug)
{
   static PFN_D3D12_GET_DEBUG_INTERFACE fp;
   if (!d3d12_dll)
      if (!(d3d12_dll = dylib_load("d3d12.dll")))
         return TYPE_E_CANTLOADLIBRARY;
   if (!fp)
      if (!(fp = (PFN_D3D12_GET_DEBUG_INTERFACE)dylib_proc(d3d12_dll,
                  "D3D12GetDebugInterface")))
         return TYPE_E_DLLFUNCTIONNOTFOUND;
   return fp(riid, ppvDebug);
}

HRESULT WINAPI D3D12SerializeRootSignature(
      const D3D12_ROOT_SIGNATURE_DESC* pRootSignature,
      D3D_ROOT_SIGNATURE_VERSION       Version,
      ID3DBlob**                       ppBlob,
      ID3DBlob**                       ppErrorBlob)
{
   static PFN_D3D12_SERIALIZE_ROOT_SIGNATURE fp;
   if (!d3d12_dll)
      if (!(d3d12_dll = dylib_load("d3d12.dll")))
         return TYPE_E_CANTLOADLIBRARY;
   if (!fp)
      if (!(fp = (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)dylib_proc(d3d12_dll,
                  "D3D12SerializeRootSignature")))
         return TYPE_E_DLLFUNCTIONNOTFOUND;
   return fp(pRootSignature, Version, ppBlob, ppErrorBlob);
}

HRESULT WINAPI D3D12SerializeVersionedRootSignature(
      const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* pRootSignature,
      ID3DBlob**                                 ppBlob,
      ID3DBlob**                                 ppErrorBlob)
{
   static PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE fp;
   if (!d3d12_dll)
      if (!(d3d12_dll = dylib_load("d3d12.dll")))
         return TYPE_E_CANTLOADLIBRARY;
   if (!fp)
      if (!(fp = (PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)dylib_proc(
            d3d12_dll, "D3D12SerializeRootSignature")))
         return TYPE_E_DLLFUNCTIONNOTFOUND;
   return fp(pRootSignature, ppBlob, ppErrorBlob);
}
#endif
