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
#include "d3d12_common.h"
#include "dxgi_common.h"
#include "d3dcompiler_common.h"

#include "../verbosity.h"
#include "../../configuration.h"

#ifdef HAVE_DYNAMIC
#include <dynamic/dylib.h>
#endif

#include <encodings/utf.h>
#include <lists/string_list.h>
#include <dxgi.h>

#ifdef __MINGW32__
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
DEFINE_GUIDW(IID_ID3D12VersionedRootSignatureDeserializer, 0x7F91CE67, 0x090C, 0x4BB7, 0xB7, 0x8E, 0xED, 0x8F, 0xF2, 0xE3, 0x1D, 0xA0);
DEFINE_GUIDW(IID_ID3D12Pageable, 0x63ee58fb, 0x1268, 0x4835, 0x86, 0xda, 0xf0, 0x08, 0xce, 0x62, 0xf0, 0xd6);
DEFINE_GUIDW(IID_ID3D12Heap, 0x6b3b2502, 0x6e51, 0x45b3, 0x90, 0xee, 0x98, 0x84, 0x26, 0x5e, 0x8d, 0xf3);
DEFINE_GUIDW(IID_ID3D12QueryHeap, 0x0d9658ae, 0xed45, 0x469e, 0xa6, 0x1d, 0x97, 0x0e, 0xc5, 0x83, 0xca, 0xb4);
DEFINE_GUIDW(IID_ID3D12CommandSignature, 0xc36a797c, 0xec80, 0x4f0a, 0x89, 0x85, 0xa7, 0xb2, 0x47, 0x50, 0x82, 0xd1);
DEFINE_GUIDW(IID_ID3D12CommandList, 0x7116d91c, 0xe7e4, 0x47ce, 0xb8, 0xc6, 0xec, 0x81, 0x68, 0xf4, 0x37, 0xe5);
DEFINE_GUIDW(IID_ID3D12PipelineLibrary, 0xc64226a8, 0x9201, 0x46af, 0xb4, 0xcc, 0x53, 0xfb, 0x9f, 0xf7, 0x41, 0x4f);
DEFINE_GUIDW(IID_ID3D12Device1, 0x77acce80, 0x638e, 0x4e65, 0x88, 0x95, 0xc1, 0xf2, 0x33, 0x86, 0x86, 0x3e);
DEFINE_GUIDW(IID_ID3D12Debug, 0x344488b7, 0x6846, 0x474b, 0xb9, 0x89, 0xf0, 0x27, 0x44, 0x82, 0x45, 0xe0);
DEFINE_GUIDW(IID_ID3D12Debug1, 0xaffaa4ca, 0x63fe, 0x4d8e, 0xb8, 0xad, 0x15, 0x90, 0x00, 0xaf, 0x43, 0x04);
DEFINE_GUIDW(IID_ID3D12DebugDevice1, 0xa9b71770, 0xd099, 0x4a65, 0xa6, 0x98, 0x3d, 0xee, 0x10, 0x02, 0x0f, 0x88);
DEFINE_GUIDW(IID_ID3D12DebugDevice, 0x3febd6dd, 0x4973, 0x4787, 0x81, 0x94, 0xe4, 0x5f, 0x9e, 0x28, 0x92, 0x3e);
DEFINE_GUIDW(IID_ID3D12DebugCommandQueue, 0x09e0bf36, 0x54ac, 0x484f, 0x88, 0x47, 0x4b, 0xae, 0xea, 0xb6, 0x05, 0x3a);
DEFINE_GUIDW(IID_ID3D12DebugCommandList1, 0x102ca951, 0x311b, 0x4b01, 0xb1, 0x1f, 0xec, 0xb8, 0x3e, 0x06, 0x1b, 0x37);
DEFINE_GUIDW(IID_ID3D12DebugCommandList, 0x09e0bf36, 0x54ac, 0x484f, 0x88, 0x47, 0x4b, 0xae, 0xea, 0xb6, 0x05, 0x3f);
/* clang-format on */
#endif

#if defined(HAVE_DYNAMIC) && !defined(__WINRT__)
static dylib_t     d3d12_dll;
static const char* d3d12_dll_name = "d3d12.dll";

HRESULT WINAPI D3D12CreateDevice(
      IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void** ppDevice)
{
   static PFN_D3D12_CREATE_DEVICE fp;
   if (!d3d12_dll)
      d3d12_dll = dylib_load(d3d12_dll_name);

   if (!d3d12_dll)
      return TYPE_E_CANTLOADLIBRARY;

   if (!fp)
      fp = (PFN_D3D12_CREATE_DEVICE)dylib_proc(d3d12_dll, "D3D12CreateDevice");

   if (!fp)
      return TYPE_E_DLLFUNCTIONNOTFOUND;

   return fp(pAdapter, MinimumFeatureLevel, riid, ppDevice);
}

HRESULT WINAPI D3D12GetDebugInterface(REFIID riid, void** ppvDebug)
{
   static PFN_D3D12_GET_DEBUG_INTERFACE fp;
   if (!d3d12_dll)
      d3d12_dll = dylib_load(d3d12_dll_name);

   if (!d3d12_dll)
      return TYPE_E_CANTLOADLIBRARY;

   if (!fp)
      fp = (PFN_D3D12_GET_DEBUG_INTERFACE)dylib_proc(d3d12_dll, "D3D12GetDebugInterface");

   if (!fp)
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
      d3d12_dll = dylib_load(d3d12_dll_name);

   if (!d3d12_dll)
      return TYPE_E_CANTLOADLIBRARY;

   if (!fp)
      fp = (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)dylib_proc(d3d12_dll, "D3D12SerializeRootSignature");

   if (!fp)
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
      d3d12_dll = dylib_load(d3d12_dll_name);

   if (!d3d12_dll)
      return TYPE_E_CANTLOADLIBRARY;

   if (!fp)
      fp = (PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)dylib_proc(
            d3d12_dll, "D3D12SerializeRootSignature");

   if (!fp)
      return TYPE_E_DLLFUNCTIONNOTFOUND;

   return fp(pRootSignature, ppBlob, ppErrorBlob);
}
#endif

bool d3d12_init_base(d3d12_video_t* d3d12)
{
   DXGIAdapter adapter = NULL;
#ifdef DEBUG
   D3D12GetDebugInterface_(&d3d12->debugController);
   D3D12EnableDebugLayer(d3d12->debugController);
#endif

#ifdef __WINRT__
   DXGICreateFactory2(&d3d12->factory);
#else
   DXGICreateFactory(&d3d12->factory);
#endif

   {
      int i = 0;
      settings_t *settings = config_get_ptr();

      if (d3d12->gpu_list)
         string_list_free(d3d12->gpu_list);

      d3d12->gpu_list = string_list_new();

      while (true)
      {
         char str[128];
         union string_list_elem_attr attr = {0};
         DXGI_ADAPTER_DESC desc = {0};

         str[0] = '\0';

#ifdef __WINRT__
         if (FAILED(DXGIEnumAdapters2(d3d12->factory, i, &adapter)))
            break;
#else
         if (FAILED(DXGIEnumAdapters(d3d12->factory, i, &adapter)))
            break;
#endif

         IDXGIAdapter_GetDesc(adapter, &desc);

         utf16_to_char_string((const uint16_t*)desc.Description, str, sizeof(str));

         RARCH_LOG("[D3D12]: Found GPU at index %d: %s\n", i, str);

         string_list_append(d3d12->gpu_list, str, attr);

         if (i < D3D12_MAX_GPU_COUNT)
         {
            AddRef(adapter);
            d3d12->adapters[i] = adapter;
         }
         Release(adapter);
         adapter = NULL;

         i++;
         if (i >= D3D12_MAX_GPU_COUNT)
            break;
      }

      video_driver_set_gpu_api_devices(GFX_CTX_DIRECT3D12_API, d3d12->gpu_list);

      if (0 <= settings->ints.d3d12_gpu_index && settings->ints.d3d12_gpu_index <= i && settings->ints.d3d12_gpu_index < D3D12_MAX_GPU_COUNT)
      {
         d3d12->adapter = d3d12->adapters[settings->ints.d3d12_gpu_index];
         AddRef(d3d12->adapter);
         RARCH_LOG("[D3D12]: Using GPU index %d.\n", settings->ints.d3d12_gpu_index);
         video_driver_set_gpu_device_string(d3d12->gpu_list->elems[settings->ints.d3d12_gpu_index].data);
      }
      else
      {
         RARCH_WARN("[D3D12]: Invalid GPU index %d, using first device found.\n", settings->ints.d3d12_gpu_index);
         d3d12->adapter = d3d12->adapters[0];
         AddRef(d3d12->adapter);
      }

      if (!SUCCEEDED(D3D12CreateDevice_(d3d12->adapter, D3D_FEATURE_LEVEL_11_0, &d3d12->device)))
         RARCH_WARN("[D3D12]: Could not create D3D12 device.\n");
   }

   return true;
}

bool d3d12_init_queue(d3d12_video_t* d3d12)
{
   {
      static const D3D12_COMMAND_QUEUE_DESC desc = { D3D12_COMMAND_LIST_TYPE_DIRECT, 0,
                                                     D3D12_COMMAND_QUEUE_FLAG_NONE, 0 };
      D3D12CreateCommandQueue(
            d3d12->device, (D3D12_COMMAND_QUEUE_DESC*)&desc, &d3d12->queue.handle);
   }

   D3D12CreateCommandAllocator(
         d3d12->device, D3D12_COMMAND_LIST_TYPE_DIRECT, &d3d12->queue.allocator);

   D3D12CreateGraphicsCommandList(
         d3d12->device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12->queue.allocator,
         d3d12->pipes[VIDEO_SHADER_STOCK_BLEND], &d3d12->queue.cmd);

   D3D12CloseGraphicsCommandList(d3d12->queue.cmd);

   D3D12CreateFence(d3d12->device, 0, D3D12_FENCE_FLAG_NONE, &d3d12->queue.fence);
   d3d12->queue.fenceValue = 1;
   d3d12->queue.fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

   D3D12SignalCommandQueue(d3d12->queue.handle, d3d12->queue.fence, d3d12->queue.fenceValue);

   return true;
}

bool d3d12_init_swapchain(d3d12_video_t* d3d12,
      int width, int height, void* corewindow)
{
   unsigned i;
   HRESULT hr;
#ifdef __WINRT__
   DXGI_SWAP_CHAIN_DESC1 desc;
   memset(&desc, 0, sizeof(DXGI_SWAP_CHAIN_DESC1));
#else
   DXGI_SWAP_CHAIN_DESC desc;
   HWND hwnd                 = (HWND)corewindow;
   memset(&desc, 0, sizeof(DXGI_SWAP_CHAIN_DESC));
#endif

   desc.BufferCount          = countof(d3d12->chain.renderTargets);
#ifdef __WINRT__
   desc.Width                = width;
   desc.Height               = height;
   desc.Format               = DXGI_FORMAT_R8G8B8A8_UNORM;
#else
   desc.BufferDesc.Width     = width;
   desc.BufferDesc.Height    = height;
   desc.BufferDesc.Format    = DXGI_FORMAT_R8G8B8A8_UNORM;
#endif
   desc.SampleDesc.Count     = 1;
#if 0
   desc.BufferDesc.RefreshRate.Numerator   = 60;
   desc.BufferDesc.RefreshRate.Denominator = 1;
   desc.SampleDesc.Quality                 = 0;
#endif
   desc.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
#ifdef HAVE_WINDOW
   desc.OutputWindow = hwnd;
   desc.Windowed     = TRUE;
#endif
#if 0
   desc.SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
#else
   desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
#endif

#ifdef __WINRT__
   hr = DXGICreateSwapChainForCoreWindow(d3d12->factory, d3d12->queue.handle, corewindow, &desc, NULL, &d3d12->chain.handle);
#else
   hr = DXGICreateSwapChain(d3d12->factory, d3d12->queue.handle, &desc, &d3d12->chain.handle);
#endif
   if (FAILED(hr))
   {
      RARCH_ERR("[D3D12]: Failed to create the swap chain (0x%08X)\n", hr);
      return false;
   }

#ifdef HAVE_WINDOW
   DXGIMakeWindowAssociation(d3d12->factory, hwnd, DXGI_MWA_NO_ALT_ENTER);
#endif

   d3d12->chain.frame_index = DXGIGetCurrentBackBufferIndex(d3d12->chain.handle);

   for (i = 0; i < countof(d3d12->chain.renderTargets); i++)
   {
      DXGIGetSwapChainBuffer(d3d12->chain.handle, i, &d3d12->chain.renderTargets[i]);
      D3D12CreateRenderTargetView(
            d3d12->device, d3d12->chain.renderTargets[i], NULL, d3d12->chain.desc_handles[i]);
   }

   d3d12->chain.viewport.Width     = width;
   d3d12->chain.viewport.Height    = height;
   d3d12->chain.scissorRect.right  = width;
   d3d12->chain.scissorRect.bottom = height;

   return true;
}

static void d3d12_init_descriptor_heap(D3D12Device device, d3d12_descriptor_heap_t* out)
{
   D3D12CreateDescriptorHeap(device, &out->desc, &out->handle);
   out->cpu    = D3D12GetCPUDescriptorHandleForHeapStart(out->handle);
   out->gpu    = D3D12GetGPUDescriptorHandleForHeapStart(out->handle);
   out->stride = D3D12GetDescriptorHandleIncrementSize(device, out->desc.Type);
   out->map    = (bool*)calloc(out->desc.NumDescriptors, sizeof(bool));
}

static inline void d3d12_release_descriptor_heap(d3d12_descriptor_heap_t* heap)
{
   free(heap->map);
   Release(heap->handle);
}

static D3D12_CPU_DESCRIPTOR_HANDLE d3d12_descriptor_heap_slot_alloc(d3d12_descriptor_heap_t* heap)
{
   int                         i;
   D3D12_CPU_DESCRIPTOR_HANDLE handle = { 0 };

   for (i = heap->start; i < (int)heap->desc.NumDescriptors; i++)
   {
      if (!heap->map[i])
      {
         heap->map[i] = true;
         handle.ptr   = heap->cpu.ptr + i * heap->stride;
         heap->start  = i + 1;
         return handle;
      }
   }
   /* if you get here try increasing NumDescriptors for this heap */
   assert(0);
   return handle;
}

static void
d3d12_descriptor_heap_slot_free(d3d12_descriptor_heap_t* heap, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
   unsigned i;

   if (!handle.ptr)
      return;

   assert(((handle.ptr - heap->cpu.ptr) % heap->stride) == 0);

   i = (handle.ptr - heap->cpu.ptr) / heap->stride;
   assert(i >= 0 && i < heap->desc.NumDescriptors);
   assert(heap->map[i]);

   heap->map[i] = false;
   if (heap->start > (int)i)
      heap->start = i;
}

bool d3d12_create_root_signature(
      D3D12Device device, D3D12_ROOT_SIGNATURE_DESC* desc, D3D12RootSignature* out)
{
   D3DBlob signature;
   D3DBlob error;
   D3D12SerializeRootSignature(desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

   if (error)
   {
      RARCH_ERR(
            "[D3D12]: CreateRootSignature failed : %s", (const char*)D3DGetBufferPointer(error));
      Release(error);
      return false;
   }

   D3D12CreateRootSignature(
         device, 0, D3DGetBufferPointer(signature), D3DGetBufferSize(signature), out);
   Release(signature);

   return true;
}

bool d3d12_init_descriptors(d3d12_video_t* d3d12)
{
   int                       i, j;
   D3D12_ROOT_SIGNATURE_DESC desc;
   D3D12_DESCRIPTOR_RANGE    srv_tbl[1]     = { { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1 } };
   D3D12_DESCRIPTOR_RANGE    uav_tbl[1]     = { { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1 } };
   D3D12_DESCRIPTOR_RANGE    sampler_tbl[1] = { { D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1 } };
   D3D12_STATIC_SAMPLER_DESC static_sampler = { D3D12_FILTER_MIN_MAG_MIP_POINT };
   D3D12_ROOT_PARAMETER      root_params[ROOT_ID_MAX];
   D3D12_ROOT_PARAMETER      cs_root_params[CS_ROOT_ID_MAX];

   root_params[ROOT_ID_TEXTURE_T].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
   root_params[ROOT_ID_TEXTURE_T].DescriptorTable.NumDescriptorRanges = countof(srv_tbl);
   root_params[ROOT_ID_TEXTURE_T].DescriptorTable.pDescriptorRanges   = srv_tbl;
   root_params[ROOT_ID_TEXTURE_T].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

   root_params[ROOT_ID_SAMPLER_T].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
   root_params[ROOT_ID_SAMPLER_T].DescriptorTable.NumDescriptorRanges = countof(sampler_tbl);
   root_params[ROOT_ID_SAMPLER_T].DescriptorTable.pDescriptorRanges   = sampler_tbl;
   root_params[ROOT_ID_SAMPLER_T].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

   root_params[ROOT_ID_UBO].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
   root_params[ROOT_ID_UBO].Descriptor.RegisterSpace  = 0;
   root_params[ROOT_ID_UBO].Descriptor.ShaderRegister = 0;
   root_params[ROOT_ID_UBO].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

   root_params[ROOT_ID_PC].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
   root_params[ROOT_ID_PC].Descriptor.RegisterSpace  = 0;
   root_params[ROOT_ID_PC].Descriptor.ShaderRegister = 1;
   root_params[ROOT_ID_PC].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

   desc.NumParameters     = countof(root_params);
   desc.pParameters       = root_params;
   desc.NumStaticSamplers = 0;
   desc.pStaticSamplers   = NULL;
   desc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

   d3d12_create_root_signature(d3d12->device, &desc, &d3d12->desc.rootSignature);

   srv_tbl[0].NumDescriptors     = SLANG_NUM_BINDINGS;
   sampler_tbl[0].NumDescriptors = SLANG_NUM_BINDINGS;
   d3d12_create_root_signature(d3d12->device, &desc, &d3d12->desc.sl_rootSignature);

   cs_root_params[CS_ROOT_ID_TEXTURE_T].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
   cs_root_params[CS_ROOT_ID_TEXTURE_T].DescriptorTable.NumDescriptorRanges = countof(srv_tbl);
   cs_root_params[CS_ROOT_ID_TEXTURE_T].DescriptorTable.pDescriptorRanges   = srv_tbl;
   cs_root_params[CS_ROOT_ID_TEXTURE_T].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;

   cs_root_params[CS_ROOT_ID_UAV_T].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
   cs_root_params[CS_ROOT_ID_UAV_T].DescriptorTable.NumDescriptorRanges = countof(uav_tbl);
   cs_root_params[CS_ROOT_ID_UAV_T].DescriptorTable.pDescriptorRanges   = uav_tbl;
   cs_root_params[CS_ROOT_ID_UAV_T].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_ALL;

   cs_root_params[CS_ROOT_ID_CONSTANTS].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
   cs_root_params[CS_ROOT_ID_CONSTANTS].Constants.Num32BitValues = 3;
   cs_root_params[CS_ROOT_ID_CONSTANTS].Constants.RegisterSpace  = 0;
   cs_root_params[CS_ROOT_ID_CONSTANTS].Constants.ShaderRegister = 0;
   cs_root_params[CS_ROOT_ID_CONSTANTS].ShaderVisibility         = D3D12_SHADER_VISIBILITY_ALL;

   static_sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
   static_sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
   static_sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
#if 0
   static_sampler.MaxAnisotropy             = 1;
   static_sampler.ComparisonFunc            = D3D12_COMPARISON_FUNC_NEVER;
   static_sampler.MinLOD                    = -D3D12_FLOAT32_MAX;
   static_sampler.MaxLOD                    = D3D12_FLOAT32_MAX;
#endif

   desc.NumParameters     = countof(cs_root_params);
   desc.pParameters       = cs_root_params;
   desc.NumStaticSamplers = 1;
   desc.pStaticSamplers   = &static_sampler;
   desc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

   d3d12_create_root_signature(d3d12->device, &desc, &d3d12->desc.cs_rootSignature);

   d3d12->desc.rtv_heap.desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
   d3d12->desc.rtv_heap.desc.NumDescriptors = countof(d3d12->chain.renderTargets) + GFX_MAX_SHADERS;
   d3d12->desc.rtv_heap.desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
   d3d12_init_descriptor_heap(d3d12->device, &d3d12->desc.rtv_heap);

   d3d12->desc.srv_heap.desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
   d3d12->desc.srv_heap.desc.NumDescriptors = SLANG_NUM_BINDINGS * GFX_MAX_SHADERS + 1024;
   d3d12->desc.srv_heap.desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
   d3d12_init_descriptor_heap(d3d12->device, &d3d12->desc.srv_heap);

   d3d12->desc.sampler_heap.desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
   d3d12->desc.sampler_heap.desc.NumDescriptors =
         SLANG_NUM_BINDINGS * GFX_MAX_SHADERS + 2 * RARCH_WRAP_MAX;
   d3d12->desc.sampler_heap.desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
   d3d12_init_descriptor_heap(d3d12->device, &d3d12->desc.sampler_heap);

   for (i = 0; i < countof(d3d12->chain.renderTargets); i++)
   {
      d3d12->chain.desc_handles[i].ptr =
            d3d12->desc.rtv_heap.cpu.ptr + i * d3d12->desc.rtv_heap.stride;
   }

   for (i = 0; i < GFX_MAX_SHADERS; i++)
   {
      d3d12->pass[i].rt.rt_view.ptr =
            d3d12->desc.rtv_heap.cpu.ptr +
            (countof(d3d12->chain.renderTargets) + i) * d3d12->desc.rtv_heap.stride;

      d3d12->pass[i].textures.ptr = d3d12_descriptor_heap_slot_alloc(&d3d12->desc.srv_heap).ptr -
                                    d3d12->desc.srv_heap.cpu.ptr + d3d12->desc.srv_heap.gpu.ptr;
      d3d12->pass[i].samplers.ptr =
            d3d12_descriptor_heap_slot_alloc(&d3d12->desc.sampler_heap).ptr -
            d3d12->desc.sampler_heap.cpu.ptr + d3d12->desc.sampler_heap.gpu.ptr;

      for (j = 1; j < SLANG_NUM_BINDINGS; j++)
      {
         d3d12_descriptor_heap_slot_alloc(&d3d12->desc.srv_heap);
         d3d12_descriptor_heap_slot_alloc(&d3d12->desc.sampler_heap);
      }
   }

   return true;
}

static INLINE D3D12_GPU_DESCRIPTOR_HANDLE
              d3d12_create_sampler(D3D12Device device, D3D12_SAMPLER_DESC* desc, d3d12_descriptor_heap_t* heap)
{
   D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = d3d12_descriptor_heap_slot_alloc(heap);
   D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle = { cpu_handle.ptr - heap->cpu.ptr + heap->gpu.ptr };

   D3D12CreateSampler(device, desc, cpu_handle);
   return gpu_handle;
}

void d3d12_init_samplers(d3d12_video_t* d3d12)
{
   int                i;
   D3D12_SAMPLER_DESC desc = { D3D12_FILTER_MIN_MAG_MIP_POINT };
   desc.MaxAnisotropy      = 1;
   desc.ComparisonFunc     = D3D12_COMPARISON_FUNC_NEVER;
   desc.MinLOD             = -D3D12_FLOAT32_MAX;
   desc.MaxLOD             = D3D12_FLOAT32_MAX;

   for (i = 0; i < RARCH_WRAP_MAX; i++)
   {
      switch (i)
      {
         default:
         case RARCH_WRAP_BORDER:
            desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            break;

         case RARCH_WRAP_EDGE:
            desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            break;

         case RARCH_WRAP_REPEAT:
            desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            break;

         case RARCH_WRAP_MIRRORED_REPEAT:
            desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            break;
      }
      desc.AddressV = desc.AddressU;
      desc.AddressW = desc.AddressU;

      desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
      d3d12->samplers[RARCH_FILTER_LINEAR][i] =
            d3d12_create_sampler(d3d12->device, &desc, &d3d12->desc.sampler_heap);

      desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
      d3d12->samplers[RARCH_FILTER_NEAREST][i] =
            d3d12_create_sampler(d3d12->device, &desc, &d3d12->desc.sampler_heap);
   }
}

D3D12_RENDER_TARGET_BLEND_DESC d3d12_blend_enable_desc = {
   TRUE,
   FALSE,
   D3D12_BLEND_SRC_ALPHA,
   D3D12_BLEND_INV_SRC_ALPHA,
   D3D12_BLEND_OP_ADD,
   D3D12_BLEND_SRC_ALPHA,
   D3D12_BLEND_INV_SRC_ALPHA,
   D3D12_BLEND_OP_ADD,
   D3D12_LOGIC_OP_NOOP,
   D3D12_COLOR_WRITE_ENABLE_ALL,
};

bool d3d12_init_pipeline(
      D3D12Device                         device,
      D3DBlob                             vs_code,
      D3DBlob                             ps_code,
      D3DBlob                             gs_code,
      D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc,
      D3D12PipelineState*                 out)
{
   if (vs_code)
   {
      desc->VS.pShaderBytecode = D3DGetBufferPointer(vs_code);
      desc->VS.BytecodeLength  = D3DGetBufferSize(vs_code);
   }
   else
   {
      desc->VS.pShaderBytecode = NULL;
      desc->VS.BytecodeLength  = 0;
   }

   if (ps_code)
   {
      desc->PS.pShaderBytecode = D3DGetBufferPointer(ps_code);
      desc->PS.BytecodeLength  = D3DGetBufferSize(ps_code);
   }
   else
   {
      desc->PS.pShaderBytecode = NULL;
      desc->PS.BytecodeLength  = 0;
   }

   if (gs_code)
   {
      desc->GS.pShaderBytecode = D3DGetBufferPointer(gs_code);
      desc->GS.BytecodeLength  = D3DGetBufferSize(gs_code);
   }
   else
   {
      desc->GS.pShaderBytecode = NULL;
      desc->GS.BytecodeLength  = 0;
   }

   desc->SampleMask               = UINT_MAX;
   desc->RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
   desc->RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
   desc->NumRenderTargets         = 1;
   desc->SampleDesc.Count         = 1;

   D3D12CreateGraphicsPipelineState(device, desc, out);

   return true;
}

D3D12_GPU_VIRTUAL_ADDRESS
d3d12_create_buffer(D3D12Device device, UINT size_in_bytes, D3D12Resource* buffer)
{
   D3D12_HEAP_PROPERTIES heap_props    = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                        D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
   D3D12_RESOURCE_DESC   resource_desc = { D3D12_RESOURCE_DIMENSION_BUFFER };

   resource_desc.Width            = size_in_bytes;
   resource_desc.Height           = 1;
   resource_desc.DepthOrArraySize = 1;
   resource_desc.MipLevels        = 1;
   resource_desc.SampleDesc.Count = 1;
   resource_desc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

   D3D12CreateCommittedResource(
         device, (D3D12_HEAP_PROPERTIES*)&heap_props, D3D12_HEAP_FLAG_NONE, &resource_desc,
         D3D12_RESOURCE_STATE_GENERIC_READ, NULL, buffer);

   return D3D12GetGPUVirtualAddress(*buffer);
}

void d3d12_release_texture(d3d12_texture_t* texture)
{
   if (!texture->handle)
      return;

   if (texture->srv_heap && texture->desc.MipLevels <= countof(texture->cpu_descriptor))
   {
      int i;
      for (i = 0; i < texture->desc.MipLevels; i++)
      {
         d3d12_descriptor_heap_slot_free(texture->srv_heap, texture->cpu_descriptor[i]);
         texture->cpu_descriptor[i].ptr = 0;
      }
   }

   Release(texture->handle);
   Release(texture->upload_buffer);
}
void d3d12_init_texture(D3D12Device device, d3d12_texture_t* texture)
{
   int i;
   d3d12_release_texture(texture);

   if (!texture->desc.MipLevels)
      texture->desc.MipLevels = 1;

   if (!(texture->desc.Width >> (texture->desc.MipLevels - 1)) &&
       !(texture->desc.Height >> (texture->desc.MipLevels - 1)))
   {
      unsigned width          = texture->desc.Width >> 5;
      unsigned height         = texture->desc.Height >> 5;
      texture->desc.MipLevels = 1;
      while (width && height)
      {
         width >>= 1;
         height >>= 1;
         texture->desc.MipLevels++;
      }
   }

   {
      D3D12_FEATURE_DATA_FORMAT_SUPPORT format_support = {
         texture->desc.Format, D3D12_FORMAT_SUPPORT1_TEXTURE2D | D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE
      };
      D3D12_HEAP_PROPERTIES heap_props = { D3D12_HEAP_TYPE_DEFAULT, D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                           D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

      if (texture->desc.MipLevels > 1)
      {
         texture->desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
         format_support.Support1 |= D3D12_FORMAT_SUPPORT1_MIP;
         format_support.Support2 |= D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE;
      }

      if (texture->desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
         format_support.Support1 |= D3D12_FORMAT_SUPPORT1_RENDER_TARGET;

      texture->desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      texture->desc.DepthOrArraySize = 1;
      texture->desc.SampleDesc.Count = 1;
      texture->desc.Format           = d3d12_get_closest_match(device, &format_support);

      D3D12CreateCommittedResource(
            device, &heap_props, D3D12_HEAP_FLAG_NONE, &texture->desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, NULL, &texture->handle);
   }

   assert(texture->srv_heap);

   {
      D3D12_SHADER_RESOURCE_VIEW_DESC desc = { texture->desc.Format };

      desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      desc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
      desc.Texture2D.MipLevels     = texture->desc.MipLevels;

      texture->cpu_descriptor[0] = d3d12_descriptor_heap_slot_alloc(texture->srv_heap);
      D3D12CreateShaderResourceView(device, texture->handle, &desc, texture->cpu_descriptor[0]);
      texture->gpu_descriptor[0].ptr = texture->cpu_descriptor[0].ptr - texture->srv_heap->cpu.ptr +
                                       texture->srv_heap->gpu.ptr;
   }

   for (i = 1; i < texture->desc.MipLevels; i++)
   {
      D3D12_UNORDERED_ACCESS_VIEW_DESC desc = { texture->desc.Format };

      desc.ViewDimension      = D3D12_UAV_DIMENSION_TEXTURE2D;
      desc.Texture2D.MipSlice = i;

      texture->cpu_descriptor[i] = d3d12_descriptor_heap_slot_alloc(texture->srv_heap);
      D3D12CreateUnorderedAccessView(
            device, texture->handle, NULL, &desc, texture->cpu_descriptor[i]);
      texture->gpu_descriptor[i].ptr = texture->cpu_descriptor[i].ptr - texture->srv_heap->cpu.ptr +
                                       texture->srv_heap->gpu.ptr;
   }

   if (texture->desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
   {
      assert(texture->rt_view.ptr);
      D3D12CreateRenderTargetView(device, texture->handle, NULL, texture->rt_view);
   }
   else
   {
      D3D12_HEAP_PROPERTIES heap_props  = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
                                           D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
      D3D12_RESOURCE_DESC   buffer_desc = { D3D12_RESOURCE_DIMENSION_BUFFER };

      D3D12GetCopyableFootprints(
            device, &texture->desc, 0, 1, 0, &texture->layout, &texture->num_rows,
            &texture->row_size_in_bytes, &texture->total_bytes);

      buffer_desc.Width            = texture->total_bytes;
      buffer_desc.Height           = 1;
      buffer_desc.DepthOrArraySize = 1;
      buffer_desc.MipLevels        = 1;
      buffer_desc.SampleDesc.Count = 1;
      buffer_desc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
#if 0
      buffer_desc.Flags            = D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;
#endif

      D3D12CreateCommittedResource(
            device, &heap_props, D3D12_HEAP_FLAG_NONE, &buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &texture->upload_buffer);
   }

   texture->size_data.x = texture->desc.Width;
   texture->size_data.y = texture->desc.Height;
   texture->size_data.z = 1.0f / texture->desc.Width;
   texture->size_data.w = 1.0f / texture->desc.Height;
}

void d3d12_update_texture(
      int              width,
      int              height,
      int              pitch,
      DXGI_FORMAT      format,
      const void*      data,
      d3d12_texture_t* texture)
{
   uint8_t*    dst;
   D3D12_RANGE read_range = { 0, 0 };

   if (!texture || !texture->upload_buffer)
      return;

   D3D12Map(texture->upload_buffer, 0, &read_range, (void**)&dst);

   dxgi_copy(
         width, height, format, pitch, data, texture->desc.Format,
         texture->layout.Footprint.RowPitch, dst + texture->layout.Offset);

   D3D12Unmap(texture->upload_buffer, 0, NULL);

   texture->dirty = true;
}
void d3d12_upload_texture(D3D12GraphicsCommandList cmd,
      d3d12_texture_t* texture, void *userdata)
{
   D3D12_TEXTURE_COPY_LOCATION src = { 0 };
   D3D12_TEXTURE_COPY_LOCATION dst = { 0 };

   src.pResource       = texture->upload_buffer;
   src.Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
   src.PlacedFootprint = texture->layout;

   dst.pResource        = texture->handle;
   dst.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
   dst.SubresourceIndex = 0;

   d3d12_resource_transition(
         cmd, texture->handle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
         D3D12_RESOURCE_STATE_COPY_DEST);

   D3D12CopyTextureRegion(cmd, &dst, 0, 0, 0, &src, NULL);

   d3d12_resource_transition(
         cmd, texture->handle, D3D12_RESOURCE_STATE_COPY_DEST,
         D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

   if (texture->desc.MipLevels > 1)
   {
      unsigned       i;
      d3d12_video_t* d3d12 = (d3d12_video_t*)userdata;

      D3D12SetComputeRootSignature(cmd, d3d12->desc.cs_rootSignature);
      D3D12SetPipelineState(cmd, d3d12->mipmapgen_pipe);
      D3D12SetComputeRootDescriptorTable(cmd, CS_ROOT_ID_TEXTURE_T, texture->gpu_descriptor[0]);

      for (i = 1; i < texture->desc.MipLevels; i++)
      {
         unsigned width  = texture->desc.Width >> i;
         unsigned height = texture->desc.Height >> i;
         struct
         {
            uint32_t src_level;
            float    texel_size[2];
         } cbuffer = { i - 1, { 1.0f / width, 1.0f / height } };

         {
            D3D12_RESOURCE_BARRIER barrier = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
            barrier.Transition.pResource   = texture->handle;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            barrier.Transition.Subresource = i;
            D3D12ResourceBarrier(cmd, 1, &barrier);
         }

         D3D12SetComputeRootDescriptorTable(cmd, CS_ROOT_ID_UAV_T, texture->gpu_descriptor[i]);
         D3D12SetComputeRoot32BitConstants(
               cmd, CS_ROOT_ID_CONSTANTS, sizeof(cbuffer) / sizeof(uint32_t), &cbuffer, 0);
         D3D12Dispatch(cmd, (width + 0x7) >> 3, (height + 0x7) >> 3, 1);

         {
            D3D12_RESOURCE_BARRIER barrier = { D3D12_RESOURCE_BARRIER_TYPE_UAV };
            barrier.UAV.pResource          = texture->handle;
            D3D12ResourceBarrier(cmd, 1, &barrier);
         }

         {
            D3D12_RESOURCE_BARRIER barrier = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
            barrier.Transition.pResource   = texture->handle;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.Subresource = i;
            D3D12ResourceBarrier(cmd, 1, &barrier);
         }
      }
   }

   texture->dirty = false;
}

void d3d12_create_fullscreen_quad_vbo(
      D3D12Device device, D3D12_VERTEX_BUFFER_VIEW* view, D3D12Resource* vbo)
{
   static const d3d12_vertex_t vertices[] = {
      { { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
      { { 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
      { { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
      { { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
   };

   view->SizeInBytes    = sizeof(vertices);
   view->StrideInBytes  = sizeof(*vertices);
   view->BufferLocation = d3d12_create_buffer(device, view->SizeInBytes, vbo);

   {
      void*       vertex_data_begin;
      D3D12_RANGE read_range = { 0, 0 };

      D3D12Map(*vbo, 0, &read_range, &vertex_data_begin);
      memcpy(vertex_data_begin, vertices, sizeof(vertices));
      D3D12Unmap(*vbo, 0, NULL);
   }
}

DXGI_FORMAT d3d12_get_closest_match(D3D12Device device, D3D12_FEATURE_DATA_FORMAT_SUPPORT* desired)
{
   DXGI_FORMAT  default_list[] = { desired->Format, DXGI_FORMAT_UNKNOWN };
   DXGI_FORMAT* format         = dxgi_get_format_fallback_list(desired->Format);

   if (!format)
      format = default_list;

   while (*format != DXGI_FORMAT_UNKNOWN)
   {
      D3D12_FEATURE_DATA_FORMAT_SUPPORT format_support = { *format };
      if (SUCCEEDED(D3D12CheckFeatureSupport(
                device, D3D12_FEATURE_FORMAT_SUPPORT, &format_support, sizeof(format_support))) &&
          ((format_support.Support1 & desired->Support1) == desired->Support1) &&
          ((format_support.Support2 & desired->Support2) == desired->Support2))
         break;
      format++;
   }
   assert(*format);
   return *format;
}
