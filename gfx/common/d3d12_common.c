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

#include <dynamic/dylib.h>

#include "d3d12_common.h"
#include "dxgi_common.h"
#include "d3dcompiler_common.h"

#include "verbosity.h"

#ifdef __MINGW32__
/* clang-format off */
#define DEFINE_GUIDW(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) const GUID DECLSPEC_SELECTANY name = { l, w1, w2, { b1, b2, b3, b4, b5, b6, b7, b8 } }

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

static dylib_t     d3d12_dll;
static const char* d3d12_dll_name = "d3d12.dll";

HRESULT WINAPI D3D12CreateDevice(
      IUnknown* pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void** ppDevice)
{
   if (!d3d12_dll)
      d3d12_dll = dylib_load(d3d12_dll_name);

   if (d3d12_dll)
   {
      static PFN_D3D12_CREATE_DEVICE fp;

      if (!fp)
         fp = (PFN_D3D12_CREATE_DEVICE)dylib_proc(d3d12_dll, "D3D12CreateDevice");

      if (fp)
         return fp(pAdapter, MinimumFeatureLevel, riid, ppDevice);
   }

   return TYPE_E_CANTLOADLIBRARY;
}

HRESULT WINAPI D3D12GetDebugInterface(REFIID riid, void** ppvDebug)
{
   if (!d3d12_dll)
      d3d12_dll = dylib_load(d3d12_dll_name);

   if (d3d12_dll)
   {
      static PFN_D3D12_GET_DEBUG_INTERFACE fp;

      if (!fp)
         fp = (PFN_D3D12_GET_DEBUG_INTERFACE)dylib_proc(d3d12_dll, "D3D12GetDebugInterface");

      if (fp)
         return fp(riid, ppvDebug);
   }

   return TYPE_E_CANTLOADLIBRARY;
}

HRESULT WINAPI D3D12SerializeRootSignature(
      const D3D12_ROOT_SIGNATURE_DESC* pRootSignature,
      D3D_ROOT_SIGNATURE_VERSION       Version,
      ID3DBlob**                       ppBlob,
      ID3DBlob**                       ppErrorBlob)
{
   if (!d3d12_dll)
      d3d12_dll = dylib_load(d3d12_dll_name);

   if (d3d12_dll)
   {
      static PFN_D3D12_SERIALIZE_ROOT_SIGNATURE fp;

      if (!fp)
         fp = (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)dylib_proc(
               d3d12_dll, "D3D12SerializeRootSignature");

      if (fp)
         return fp(pRootSignature, Version, ppBlob, ppErrorBlob);
   }

   return TYPE_E_CANTLOADLIBRARY;
}

HRESULT WINAPI D3D12SerializeVersionedRootSignature(
      const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* pRootSignature,
      ID3DBlob**                                 ppBlob,
      ID3DBlob**                                 ppErrorBlob)
{
   if (!d3d12_dll)
      d3d12_dll = dylib_load(d3d12_dll_name);

   if (d3d12_dll)
   {
      static PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE fp;

      if (!fp)
         fp = (PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)dylib_proc(
               d3d12_dll, "D3D12SerializeRootSignature");

      if (fp)
         return fp(pRootSignature, ppBlob, ppErrorBlob);
   }

   return TYPE_E_CANTLOADLIBRARY;
}

#include <wiiu/wiiu_dbg.h>

bool d3d12_init_base(d3d12_video_t* d3d12)
{

#ifdef DEBUG
   D3D12GetDebugInterface_(&d3d12->debugController);
   D3D12EnableDebugLayer(d3d12->debugController);
#endif

   DXGICreateFactory(&d3d12->factory);

   {
      int i = 0;

      while (true)
      {
         if (FAILED(DXGIEnumAdapters(d3d12->factory, i++, &d3d12->adapter)))
            return false;

         if (SUCCEEDED(D3D12CreateDevice_(d3d12->adapter, D3D_FEATURE_LEVEL_11_0, &d3d12->device)))
            break;

         Release(d3d12->adapter);
      }
   }

   return true;
}

bool d3d12_init_queue(d3d12_video_t* d3d12)
{
   {
      static const D3D12_COMMAND_QUEUE_DESC desc = {
         .Type  = D3D12_COMMAND_LIST_TYPE_DIRECT,
         .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
      };
      D3D12CreateCommandQueue(
            d3d12->device, (D3D12_COMMAND_QUEUE_DESC*)&desc, &d3d12->queue.handle);
   }

   D3D12CreateCommandAllocator(
         d3d12->device, D3D12_COMMAND_LIST_TYPE_DIRECT, &d3d12->queue.allocator);

   D3D12CreateGraphicsCommandList(
         d3d12->device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12->queue.allocator,
         d3d12->pipe.handle, &d3d12->queue.cmd);

   D3D12CloseGraphicsCommandList(d3d12->queue.cmd);

   D3D12CreateFence(d3d12->device, 0, D3D12_FENCE_FLAG_NONE, &d3d12->queue.fence);
   d3d12->queue.fenceValue = 1;
   d3d12->queue.fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

   return true;
}

bool d3d12_init_swapchain(d3d12_video_t* d3d12, int width, int height, HWND hwnd)
{
   {
      DXGI_SWAP_CHAIN_DESC desc = {
         .BufferCount       = countof(d3d12->chain.renderTargets),
         .BufferDesc.Width  = width,
         .BufferDesc.Height = height,
         .BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
         .SampleDesc.Count  = 1,
#if 0
         .BufferDesc.RefreshRate.Numerator = 60,
         .BufferDesc.RefreshRate.Denominator = 1,
         .SampleDesc.Quality = 0,
#endif
         .BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT,
         .OutputWindow = hwnd,
         .Windowed     = TRUE,
#if 0
         .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
#else
         .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
#endif
      };
      DXGICreateSwapChain(d3d12->factory, d3d12->queue.handle, &desc, &d3d12->chain.handle);
   }

   DXGIMakeWindowAssociation(d3d12->factory, hwnd, DXGI_MWA_NO_ALT_ENTER);

   d3d12->chain.frame_index = DXGIGetCurrentBackBufferIndex(d3d12->chain.handle);

   for (int i = 0; i < countof(d3d12->chain.renderTargets); i++)
   {
      d3d12->chain.desc_handles[i].ptr =
            d3d12->pipe.rtv_heap.cpu.ptr + i * d3d12->pipe.rtv_heap.stride;
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
}

static void d3d12_init_sampler(
      D3D12Device                  device,
      d3d12_descriptor_heap_t*     heap,
      descriptor_heap_slot_t       heap_index,
      D3D12_FILTER                 filter,
      D3D12_TEXTURE_ADDRESS_MODE   address_mode,
      D3D12_GPU_DESCRIPTOR_HANDLE* dst)
{
   D3D12_SAMPLER_DESC sampler_desc = {
      .Filter         = filter,
      .AddressU       = address_mode,
      .AddressV       = address_mode,
      .AddressW       = address_mode,
      .MipLODBias     = 0,
      .MaxAnisotropy  = 0,
      .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
      .BorderColor    = { 0.0f },
      .MinLOD         = 0.0f,
      .MaxLOD         = D3D12_FLOAT32_MAX,
   };
   D3D12_CPU_DESCRIPTOR_HANDLE handle = { heap->cpu.ptr + heap_index * heap->stride };
   D3D12CreateSampler(device, &sampler_desc, handle);
   dst->ptr = heap->gpu.ptr + heap_index * heap->stride;
}

bool d3d12_init_descriptors(d3d12_video_t* d3d12)
{
   static const D3D12_DESCRIPTOR_RANGE srv_table[] = {
      {
            .RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
            .NumDescriptors     = 1,
            .BaseShaderRegister = 0,
            .RegisterSpace      = 0,
#if 0
            .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC, /* version 1_1 only */
#endif
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
      },
   };
   static const D3D12_DESCRIPTOR_RANGE sampler_table[] = {
      {
            .RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
            .NumDescriptors                    = 1,
            .BaseShaderRegister                = 0,
            .RegisterSpace                     = 0,
            .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
      },
   };

   static const D3D12_ROOT_PARAMETER rootParameters[ROOT_INDEX_MAX] = {
      [ROOT_INDEX_TEXTURE_TABLE] =
            {
                  .ParameterType    = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                  .DescriptorTable  = { countof(srv_table), srv_table },
                  .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
            },
      [ROOT_INDEX_SAMPLER_TABLE] =
            {
                  .ParameterType    = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
                  .DescriptorTable  = { countof(sampler_table), sampler_table },
                  .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
            },
      [ROOT_INDEX_UBO] =
            {
                  .ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV,
                  .Descriptor.ShaderRegister = 0,
                  .Descriptor.RegisterSpace  = 0,
                  .ShaderVisibility          = D3D12_SHADER_VISIBILITY_VERTEX,
            },
   };
   static const D3D12_ROOT_SIGNATURE_DESC desc = {
      .NumParameters = countof(rootParameters),
      rootParameters,
      .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
   };

   {
      D3DBlob signature;
      D3DBlob error;
      D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

      if (error)
      {
         RARCH_ERR(
               "[D3D12]: CreateRootSignature failed :\n%s\n",
               (const char*)D3DGetBufferPointer(error));
         Release(error);
         return false;
      }

      D3D12CreateRootSignature(
            d3d12->device, 0, D3DGetBufferPointer(signature), D3DGetBufferSize(signature),
            &d3d12->pipe.rootSignature);
      Release(signature);
   }

   d3d12->pipe.rtv_heap.desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
   d3d12->pipe.rtv_heap.desc.NumDescriptors = countof(d3d12->chain.renderTargets);
   d3d12->pipe.rtv_heap.desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
   d3d12_init_descriptor_heap(d3d12->device, &d3d12->pipe.rtv_heap);

   d3d12->pipe.srv_heap.desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
   d3d12->pipe.srv_heap.desc.NumDescriptors = SRV_HEAP_SLOT_MAX;
   d3d12->pipe.srv_heap.desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
   d3d12_init_descriptor_heap(d3d12->device, &d3d12->pipe.srv_heap);

   d3d12->pipe.sampler_heap.desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
   d3d12->pipe.sampler_heap.desc.NumDescriptors = SAMPLER_HEAP_SLOT_MAX;
   d3d12->pipe.sampler_heap.desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
   d3d12_init_descriptor_heap(d3d12->device, &d3d12->pipe.sampler_heap);

   d3d12_init_sampler(
         d3d12->device, &d3d12->pipe.sampler_heap, SAMPLER_HEAP_SLOT_LINEAR,
         D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_BORDER,
         &d3d12->sampler_linear);
   d3d12_init_sampler(
         d3d12->device, &d3d12->pipe.sampler_heap, SAMPLER_HEAP_SLOT_NEAREST,
         D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_BORDER,
         &d3d12->sampler_nearest);
   return true;
}

bool d3d12_init_pipeline(d3d12_video_t* d3d12)
{
   D3DBlob vs_code;
   D3DBlob ps_code;

   static const char stock[] =
#include "gfx/drivers/d3d_shaders/opaque_sm5.hlsl.h"
         ;

   static const D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
      { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, position),
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, texcoord),
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d12_vertex_t, color),
        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
   };

   static const D3D12_RASTERIZER_DESC rasterizerDesc = {
      .FillMode              = D3D12_FILL_MODE_SOLID,
      .CullMode              = D3D12_CULL_MODE_BACK,
      .FrontCounterClockwise = FALSE,
      .DepthBias             = D3D12_DEFAULT_DEPTH_BIAS,
      .DepthBiasClamp        = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
      .SlopeScaledDepthBias  = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
      .DepthClipEnable       = TRUE,
      .MultisampleEnable     = FALSE,
      .AntialiasedLineEnable = FALSE,
      .ForcedSampleCount     = 0,
      .ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
   };

   static const D3D12_BLEND_DESC blendDesc = {
      .AlphaToCoverageEnable  = FALSE,
      .IndependentBlendEnable = FALSE,
      .RenderTarget[0] =
            {
                  .BlendEnable   = TRUE,
                  .LogicOpEnable = FALSE,
                  D3D12_BLEND_SRC_ALPHA,
                  D3D12_BLEND_INV_SRC_ALPHA,
                  D3D12_BLEND_OP_ADD,
                  D3D12_BLEND_SRC_ALPHA,
                  D3D12_BLEND_INV_SRC_ALPHA,
                  D3D12_BLEND_OP_ADD,
                  D3D12_LOGIC_OP_NOOP,
                  D3D12_COLOR_WRITE_ENABLE_ALL,
            },
   };

   if (!d3d_compile(stock, sizeof(stock), "VSMain", "vs_5_0", &vs_code))
      return false;

   if (!d3d_compile(stock, sizeof(stock), "PSMain", "ps_5_0", &ps_code))
      return false;

   {
      D3D12_GRAPHICS_PIPELINE_STATE_DESC psodesc = {
         .pRootSignature     = d3d12->pipe.rootSignature,
         .VS.pShaderBytecode = D3DGetBufferPointer(vs_code),
         D3DGetBufferSize(vs_code),
         .PS.pShaderBytecode = D3DGetBufferPointer(ps_code),
         D3DGetBufferSize(ps_code),
         .BlendState                      = blendDesc,
         .SampleMask                      = UINT_MAX,
         .RasterizerState                 = rasterizerDesc,
         .DepthStencilState.DepthEnable   = FALSE,
         .DepthStencilState.StencilEnable = FALSE,
         .InputLayout.pInputElementDescs  = inputElementDesc,
         countof(inputElementDesc),
         .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
         .NumRenderTargets      = 1,
         .RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM,
         .SampleDesc.Count      = 1,
      };

      D3D12CreateGraphicsPipelineState(d3d12->device, &psodesc, &d3d12->pipe.handle);
   }

   Release(vs_code);
   Release(ps_code);

   return true;
}

D3D12_GPU_VIRTUAL_ADDRESS
d3d12_create_buffer(D3D12Device device, UINT size_in_bytes, D3D12Resource* buffer)
{
   static const D3D12_HEAP_PROPERTIES heap_props = {
      .Type             = D3D12_HEAP_TYPE_UPLOAD,
      .CreationNodeMask = 1,
      .VisibleNodeMask  = 1,
   };

   D3D12_RESOURCE_DESC resource_desc = {
      .Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER,
      .Width            = size_in_bytes,
      .Height           = 1,
      .DepthOrArraySize = 1,
      .MipLevels        = 1,
      .SampleDesc.Count = 1,
      .Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
   };

   D3D12CreateCommittedResource(
         device, (D3D12_HEAP_PROPERTIES*)&heap_props, D3D12_HEAP_FLAG_NONE, &resource_desc,
         D3D12_RESOURCE_STATE_GENERIC_READ, NULL, buffer);

   return D3D12GetGPUVirtualAddress(*buffer);
}

void d3d12_init_texture(
      D3D12Device              device,
      d3d12_descriptor_heap_t* heap,
      descriptor_heap_slot_t   heap_index,
      d3d12_texture_t*         texture)
{
   Release(texture->handle);
   Release(texture->upload_buffer);

   {
      texture->desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      texture->desc.DepthOrArraySize = 1;
      texture->desc.MipLevels        = 1;
      texture->desc.SampleDesc.Count = 1;

      D3D12_HEAP_PROPERTIES heap_props = { D3D12_HEAP_TYPE_DEFAULT, 0, 0, 1, 1 };
      D3D12CreateCommittedResource(
            device, &heap_props, D3D12_HEAP_FLAG_NONE, &texture->desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, NULL, &texture->handle);
   }

   D3D12GetCopyableFootprints(
         device, &texture->desc, 0, 1, 0, &texture->layout, &texture->num_rows,
         &texture->row_size_in_bytes, &texture->total_bytes);

   {
      D3D12_RESOURCE_DESC buffer_desc = {
         .Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER,
         .Width            = texture->total_bytes,
         .Height           = 1,
         .DepthOrArraySize = 1,
         .MipLevels        = 1,
         .SampleDesc.Count = 1,
         .Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
      };
      D3D12_HEAP_PROPERTIES heap_props = { D3D12_HEAP_TYPE_UPLOAD, 0, 0, 1, 1 };

      D3D12CreateCommittedResource(
            device, &heap_props, D3D12_HEAP_FLAG_NONE, &buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &texture->upload_buffer);
   }

   {
      D3D12_SHADER_RESOURCE_VIEW_DESC view_desc = {
         .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
         .Format                  = texture->desc.Format,
         .ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D,
         .Texture2D.MipLevels     = texture->desc.MipLevels,
      };
      D3D12_CPU_DESCRIPTOR_HANDLE handle = { heap->cpu.ptr + heap_index * heap->stride };
      D3D12CreateShaderResourceView(device, texture->handle, &view_desc, handle);
      texture->gpu_descriptor.ptr = heap->gpu.ptr + heap_index * heap->stride;
   }
}

void d3d12_upload_texture(D3D12GraphicsCommandList cmd, d3d12_texture_t* texture)
{
   D3D12_TEXTURE_COPY_LOCATION src = {
      .pResource       = texture->upload_buffer,
      .Type            = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
      .PlacedFootprint = texture->layout,
   };

   D3D12_TEXTURE_COPY_LOCATION dst = {
      .pResource        = texture->handle,
      .Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
      .SubresourceIndex = 0,
   };

   d3d12_resource_transition(
         cmd, texture->handle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
         D3D12_RESOURCE_STATE_COPY_DEST);

   D3D12CopyTextureRegion(cmd, &dst, 0, 0, 0, &src, NULL);

   d3d12_resource_transition(
         cmd, texture->handle, D3D12_RESOURCE_STATE_COPY_DEST,
         D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

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

DXGI_FORMAT d3d12_get_closest_match(
      D3D12Device device, DXGI_FORMAT desired_format, D3D12_FORMAT_SUPPORT1 desired_format_support)
{
   DXGI_FORMAT* format = dxgi_get_format_fallback_list(desired_format);

   while (*format != DXGI_FORMAT_UNKNOWN)
   {
      D3D12_FEATURE_DATA_FORMAT_SUPPORT format_support = { *format };
      if (SUCCEEDED(D3D12CheckFeatureSupport(
                device, D3D12_FEATURE_FORMAT_SUPPORT, &format_support, sizeof(format_support))) &&
          ((format_support.Support1 & desired_format_support) == desired_format_support))
         break;
      format++;
   }
   assert(*format);
   return *format;
}
