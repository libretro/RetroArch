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

static dylib_t d3d12_dll;
static const char *d3d12_dll_name = "d3d12.dll";

HRESULT WINAPI
D3D12CreateDevice(IUnknown *pAdapter,
                  D3D_FEATURE_LEVEL MinimumFeatureLevel,
                  REFIID riid, void **ppDevice)
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

HRESULT WINAPI
D3D12GetDebugInterface(REFIID riid, void **ppvDebug)
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

HRESULT WINAPI
D3D12SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC *pRootSignature,
                            D3D_ROOT_SIGNATURE_VERSION Version,
                            ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob)
{
   if (!d3d12_dll)
      d3d12_dll = dylib_load(d3d12_dll_name);

   if (d3d12_dll)
   {
      static PFN_D3D12_SERIALIZE_ROOT_SIGNATURE fp;

      if (!fp)
         fp = (PFN_D3D12_SERIALIZE_ROOT_SIGNATURE)dylib_proc(d3d12_dll, "D3D12SerializeRootSignature");

      if (fp)
         return fp(pRootSignature, Version, ppBlob, ppErrorBlob);
   }

   return TYPE_E_CANTLOADLIBRARY;
}

HRESULT WINAPI
D3D12SerializeVersionedRootSignature(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *pRootSignature,
                                     ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob)
{
   if (!d3d12_dll)
      d3d12_dll = dylib_load(d3d12_dll_name);

   if (d3d12_dll)
   {
      static PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE fp;

      if (!fp)
         fp = (PFN_D3D12_SERIALIZE_VERSIONED_ROOT_SIGNATURE)dylib_proc(d3d12_dll,
               "D3D12SerializeRootSignature");

      if (fp)
         return fp(pRootSignature, ppBlob, ppErrorBlob);
   }

   return TYPE_E_CANTLOADLIBRARY;
}


#include <wiiu/wiiu_dbg.h>

bool d3d12_init_context(d3d12_video_t *d3d12)
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

bool d3d12_init_queue(d3d12_video_t *d3d12)
{
   {
      D3D12_COMMAND_QUEUE_DESC desc =
      {
         .Type = D3D12_COMMAND_LIST_TYPE_DIRECT,
         .Flags = D3D12_COMMAND_QUEUE_FLAG_NONE,
      };
      D3D12CreateCommandQueue(d3d12->device, &desc, &d3d12->queue.handle);
   }

   D3D12CreateCommandAllocator(d3d12->device, D3D12_COMMAND_LIST_TYPE_DIRECT,
                               &d3d12->queue.allocator);

   D3D12CreateGraphicsCommandList(d3d12->device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                  d3d12->queue.allocator, d3d12->pipe.handle, &d3d12->queue.cmd);

   D3D12CloseGraphicsCommandList(d3d12->queue.cmd);

   D3D12CreateFence(d3d12->device, 0, D3D12_FENCE_FLAG_NONE, &d3d12->queue.fence);
   d3d12->queue.fenceValue = 1;
   d3d12->queue.fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

   return true;
}

bool d3d12_init_swapchain(d3d12_video_t *d3d12, int width, int height, HWND hwnd)
{
   {
      DXGI_SWAP_CHAIN_DESC desc =
      {
         .BufferCount = countof(d3d12->chain.renderTargets),
         .BufferDesc.Width = width,
         .BufferDesc.Height = height,
         .BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
         .SampleDesc.Count = 1,
//         .BufferDesc.RefreshRate.Numerator = 60,
//         .BufferDesc.RefreshRate.Denominator = 1,
//         .SampleDesc.Quality = 0,
         .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
         .OutputWindow = hwnd,
         .Windowed = TRUE,
         .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
//         .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
      };
      DXGICreateSwapChain(d3d12->factory, d3d12->queue.handle, &desc, &d3d12->chain.handle);
   }

   DXGIMakeWindowAssociation(d3d12->factory, hwnd, DXGI_MWA_NO_ALT_ENTER);

   d3d12->chain.frame_index = DXGIGetCurrentBackBufferIndex(d3d12->chain.handle);

   for (int i = 0; i < countof(d3d12->chain.renderTargets); i++)
   {
      d3d12->chain.desc_handles[i].ptr = d3d12->pipe.rtv_heap.cpu.ptr + i * d3d12->pipe.rtv_heap.stride;
      DXGIGetSwapChainBuffer(d3d12->chain.handle, i, &d3d12->chain.renderTargets[i]);
      D3D12CreateRenderTargetView(d3d12->device, d3d12->chain.renderTargets[i],
                                  NULL, d3d12->chain.desc_handles[i]);
   }

   d3d12->chain.viewport.Width = width;
   d3d12->chain.viewport.Height = height;
   d3d12->chain.scissorRect.right = width;
   d3d12->chain.scissorRect.bottom = height;

   return true;
}

static void d3d12_init_descriptor_heap(D3D12Device device, d3d12_descriptor_heap_t *out)
{
   D3D12CreateDescriptorHeap(device, &out->desc, &out->handle);
   out->cpu = D3D12GetCPUDescriptorHandleForHeapStart(out->handle);
   out->gpu = D3D12GetGPUDescriptorHandleForHeapStart(out->handle);
   out->stride = D3D12GetDescriptorHandleIncrementSize(device, out->desc.Type);
}


bool d3d12_init_descriptors(d3d12_video_t *d3d12)
{
   static const D3D12_DESCRIPTOR_RANGE desc_table[] =
   {
      {
         .RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
         .NumDescriptors = 1,
         .BaseShaderRegister = 0,
         .RegisterSpace = 0,
//            .Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC, /* version 1_1 only */
         .OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND,
      },
   };


   static const D3D12_ROOT_PARAMETER rootParameters[] =
   {
      {
         .ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE,
         .DescriptorTable = {countof(desc_table), desc_table},
         .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
      },
   };

   static const D3D12_STATIC_SAMPLER_DESC samplers[] =
   {
      {
         .Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
         .AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
         .AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
         .AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER,
         .MipLODBias = 0,
         .MaxAnisotropy = 0,
         .ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER,
         .BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
         .MinLOD = 0.0f,
         .MaxLOD = D3D12_FLOAT32_MAX,
         .ShaderRegister = 0,
         .RegisterSpace = 0,
         .ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL,
      },
   };

   static const D3D12_ROOT_SIGNATURE_DESC desc =
   {
      .NumParameters = countof(rootParameters), rootParameters,
      .NumStaticSamplers = countof(samplers), samplers,
      .Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,
   };

   {
      D3DBlob signature;
      D3DBlob error;
      D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

      if (error)
      {
         RARCH_ERR("[D3D12]: CreateRootSignature failed :\n%s\n", (const char *)D3DGetBufferPointer(error));
         Release(error);
         return false;
      }

      D3D12CreateRootSignature(d3d12->device, 0, D3DGetBufferPointer(signature),
                               D3DGetBufferSize(signature), &d3d12->pipe.rootSignature);
      Release(signature);
   }

   d3d12->pipe.rtv_heap.desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
   d3d12->pipe.rtv_heap.desc.NumDescriptors = countof(d3d12->chain.renderTargets);
   d3d12->pipe.rtv_heap.desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
   d3d12_init_descriptor_heap(d3d12->device, &d3d12->pipe.rtv_heap);

   d3d12->pipe.srv_heap.desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
   d3d12->pipe.srv_heap.desc.NumDescriptors = 16;
   d3d12->pipe.srv_heap.desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
   d3d12_init_descriptor_heap(d3d12->device, &d3d12->pipe.srv_heap);

   return true;
}

bool d3d12_init_pipeline(d3d12_video_t *d3d12)
{
   D3DBlob vs_code;
   D3DBlob ps_code;

   static const char stock [] =
#include "gfx/drivers/d3d_shaders/opaque_sm5.hlsl.h"
      ;

   D3D12_INPUT_ELEMENT_DESC inputElementDesc[] =
   {
      {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(d3d12_vertex_t, position), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof(d3d12_vertex_t, texcoord), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d12_vertex_t, color),    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
   };


   D3D12_RASTERIZER_DESC rasterizerDesc =
   {
      .FillMode = D3D12_FILL_MODE_SOLID,
      .CullMode = D3D12_CULL_MODE_BACK,
      .FrontCounterClockwise = FALSE,
      .DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
      .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
      .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
      .DepthClipEnable = TRUE,
      .MultisampleEnable = FALSE,
      .AntialiasedLineEnable = FALSE,
      .ForcedSampleCount = 0,
      .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
   };

   const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
   {
      .BlendEnable = FALSE, .LogicOpEnable = FALSE,
      D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
      D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
      D3D12_LOGIC_OP_NOOP,
      D3D12_COLOR_WRITE_ENABLE_ALL,
   };

   D3D12_BLEND_DESC blendDesc =
   {
      .AlphaToCoverageEnable = FALSE,
      .IndependentBlendEnable = FALSE,
      .RenderTarget[0] = defaultRenderTargetBlendDesc,
   };

   if (!d3d_compile(stock, sizeof(stock), "VSMain", "vs_5_0", &vs_code))
      return false;

   if (!d3d_compile(stock, sizeof(stock), "PSMain", "ps_5_0", &ps_code))
      return false;

   {
      D3D12_GRAPHICS_PIPELINE_STATE_DESC psodesc =
      {
         .pRootSignature = d3d12->pipe.rootSignature,
         .VS.pShaderBytecode = D3DGetBufferPointer(vs_code), D3DGetBufferSize(vs_code),
         .PS.pShaderBytecode = D3DGetBufferPointer(ps_code), D3DGetBufferSize(ps_code),
         .BlendState = blendDesc,
         .SampleMask = UINT_MAX,
         .RasterizerState = rasterizerDesc,
         .DepthStencilState.DepthEnable = FALSE,
         .DepthStencilState.StencilEnable = FALSE,
         .InputLayout.pInputElementDescs = inputElementDesc, countof(inputElementDesc),
         .PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
         .NumRenderTargets = 1,
         .RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM,
         .SampleDesc.Count = 1,
      };

      D3D12CreateGraphicsPipelineState(d3d12->device, &psodesc, &d3d12->pipe.handle);
   }

   Release(vs_code);
   Release(ps_code);

   return true;
}

void d3d12_create_vertex_buffer(D3D12Device device, D3D12_VERTEX_BUFFER_VIEW *view,
                                D3D12Resource *vbo)
{
   D3D12_HEAP_PROPERTIES heap_props =
   {
      .Type = D3D12_HEAP_TYPE_UPLOAD,
      .CreationNodeMask = 1,
      .VisibleNodeMask = 1,
   };

   D3D12_RESOURCE_DESC resource_desc =
   {
      .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
      .Width = view->SizeInBytes,
      .Height = 1,
      .DepthOrArraySize = 1,
      .MipLevels = 1,
      .SampleDesc.Count = 1,
      .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
   };

   D3D12CreateCommittedResource(device, &heap_props, D3D12_HEAP_FLAG_NONE, &resource_desc,
                                D3D12_RESOURCE_STATE_GENERIC_READ, NULL, vbo);
   view->BufferLocation = D3D12GetGPUVirtualAddress(*vbo);
}

void d3d12_create_texture(D3D12Device device, d3d12_descriptor_heap_t *heap, int heap_index,
                          d3d12_texture_t *tex)
{
   {
      tex->desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      tex->desc.DepthOrArraySize = 1;
      tex->desc.MipLevels = 1;
      tex->desc.SampleDesc.Count = 1;

      D3D12_HEAP_PROPERTIES heap_props = {D3D12_HEAP_TYPE_DEFAULT, 0, 0, 1, 1};
      D3D12CreateCommittedResource(device, &heap_props, D3D12_HEAP_FLAG_NONE, &tex->desc,
                                   D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, NULL, &tex->handle);
   }

   D3D12GetCopyableFootprints(device, &tex->desc, 0, 1, 0, &tex->layout, &tex->num_rows,
                              &tex->row_size_in_bytes, &tex->total_bytes);

   {
      D3D12_RESOURCE_DESC buffer_desc =
      {
         .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
         .Width = tex->total_bytes,
         .Height = 1,
         .DepthOrArraySize = 1,
         .MipLevels = 1,
         .SampleDesc.Count = 1,
         .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
      };
      D3D12_HEAP_PROPERTIES heap_props = {D3D12_HEAP_TYPE_UPLOAD, 0, 0, 1, 1};

      D3D12CreateCommittedResource(device, &heap_props, D3D12_HEAP_FLAG_NONE, &buffer_desc,
                                   D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &tex->upload_buffer);
   }

   {
      D3D12_SHADER_RESOURCE_VIEW_DESC view_desc =
      {
         .Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
         .Format = tex->desc.Format,
         .ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
         .Texture2D.MipLevels = tex->desc.MipLevels,
      };
      D3D12_CPU_DESCRIPTOR_HANDLE handle = {heap->cpu.ptr + heap_index *heap->stride};
      D3D12CreateShaderResourceView(device, tex->handle, &view_desc, handle);
      tex->gpu_descriptor.ptr = heap->gpu.ptr + heap_index * heap->stride;
   }

}

void d3d12_upload_texture(D3D12GraphicsCommandList cmd, d3d12_texture_t *texture)
{
   D3D12_TEXTURE_COPY_LOCATION src =
   {
      .pResource = texture->upload_buffer,
      .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
      .PlacedFootprint = texture->layout,
   };

   D3D12_TEXTURE_COPY_LOCATION dst =
   {
      .pResource = texture->handle,
      .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
      .SubresourceIndex = 0,
   };

   d3d12_transition(cmd, texture->handle,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);

   D3D12CopyTextureRegion(cmd, &dst, 0, 0, 0, &src, NULL);

   d3d12_transition(cmd, texture->handle,
                    D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

   texture->dirty = false;
}
