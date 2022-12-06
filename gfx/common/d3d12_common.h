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

#pragma once

#include <retro_inline.h>
#include <retro_math.h>
#include <retro_common_api.h>

#include <gfx/math/matrix_4x4.h>

#include "dxgi_common.h"
#include <d3d12.h>

#include "../common/d3dcompiler_common.h"
#include "../drivers_shader/slang_process.h"

#define D3D12_MAX_GPU_COUNT 16

typedef const ID3D12PipelineState* D3D12PipelineStateRef;

/* auto-generated */

typedef ID3D12Object*                             D3D12Object;
typedef ID3D12DeviceChild*                        D3D12DeviceChild;
typedef ID3D12RootSignature*                      D3D12RootSignature;
typedef ID3D12RootSignatureDeserializer*          D3D12RootSignatureDeserializer;
typedef ID3D12VersionedRootSignatureDeserializer* D3D12VersionedRootSignatureDeserializer;
typedef ID3D12Pageable*                           D3D12Pageable;
typedef ID3D12Heap*                               D3D12Heap;
typedef ID3D12Resource*                           D3D12Resource;
typedef ID3D12CommandAllocator*                   D3D12CommandAllocator;
typedef ID3D12Fence*                              D3D12Fence;
typedef ID3D12PipelineState*                      D3D12PipelineState;
typedef ID3D12DescriptorHeap*                     D3D12DescriptorHeap;
typedef ID3D12QueryHeap*                          D3D12QueryHeap;
typedef ID3D12CommandSignature*                   D3D12CommandSignature;
typedef ID3D12CommandList*                        D3D12CommandList;
typedef ID3D12GraphicsCommandList*                D3D12GraphicsCommandList;
typedef ID3D12CommandQueue*                       D3D12CommandQueue;
typedef ID3D12Device*                             D3D12Device;
typedef ID3D12PipelineLibrary*                    D3D12PipelineLibrary;
#ifdef DEBUG
typedef ID3D12Debug*                              D3D12Debug;
typedef ID3D12DebugDevice*                        D3D12DebugDevice;
typedef ID3D12DebugCommandQueue*                  D3D12DebugCommandQueue;
typedef ID3D12DebugCommandList*                   D3D12DebugCommandList;
#ifdef DEVICE_DEBUG
typedef ID3D12DeviceRemovedExtendedDataSettings*  D3D12DeviceRemovedExtendedDataSettings;
#endif /* DEVICE_DEBUG */
#endif
typedef ID3D12InfoQueue*                          D3D12InfoQueue;

typedef struct d3d12_vertex_t
{
   float position[2];
   float texcoord[2];
   float color[4];
} d3d12_vertex_t;

typedef struct
{
   struct
   {
      float x, y, w, h;
   } pos;
   struct
   {
      float u, v, w, h;
   } coords;
   struct
   {
      float scaling;
      float rotation;
   } params;
   UINT32 colors[4];
} d3d12_sprite_t;

typedef struct
{
   D3D12DescriptorHeap         handle; /* descriptor pool */
   D3D12_DESCRIPTOR_HEAP_DESC  desc;
   D3D12_CPU_DESCRIPTOR_HANDLE cpu; /* descriptor */
   D3D12_GPU_DESCRIPTOR_HANDLE gpu; /* descriptor */
   UINT                        stride;
   bool*                       map;
   int                         start;
} d3d12_descriptor_heap_t;

typedef struct
{
   D3D12Resource                      handle;
   D3D12Resource                      upload_buffer;
   D3D12_RESOURCE_DESC                desc;
   /* the first view is srv, the rest are mip levels uavs */
   D3D12_CPU_DESCRIPTOR_HANDLE        cpu_descriptor[D3D12_MAX_TEXTURE_DIMENSION_2_TO_EXP - 5];
   D3D12_GPU_DESCRIPTOR_HANDLE        gpu_descriptor[D3D12_MAX_TEXTURE_DIMENSION_2_TO_EXP - 5];
   D3D12_GPU_DESCRIPTOR_HANDLE        sampler;
   D3D12_CPU_DESCRIPTOR_HANDLE        rt_view;
   D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
   UINT                               num_rows;
   UINT64                             row_size_in_bytes;
   UINT64                             total_bytes;
   d3d12_descriptor_heap_t*           srv_heap;
   float4_t                           size_data;
   bool                               dirty;
} d3d12_texture_t;

#ifndef ALIGN
#ifdef _MSC_VER
#define ALIGN(x) __declspec(align(x))
#else
#define ALIGN(x) __attribute__((aligned(x)))
#endif
#endif

typedef struct ALIGN(16)
{
   math_matrix_4x4 mvp;
   struct
   {
      float width;
      float height;
   } OutputSize;
   float time;
} d3d12_uniform_t;

enum d3d12_video_flags
{
   D3D12_ST_FLAG_RESIZE_CHAIN          = (1 << 0),
   D3D12_ST_FLAG_KEEP_ASPECT           = (1 << 1),
   D3D12_ST_FLAG_RESIZE_VIEWPORT       = (1 << 2),
   D3D12_ST_FLAG_RESIZE_RTS            = (1 << 3),
   D3D12_ST_FLAG_INIT_HISTORY          = (1 << 4),
   D3D12_ST_FLAG_OVERLAYS_ENABLE       = (1 << 5),
   D3D12_ST_FLAG_OVERLAYS_FULLSCREEN   = (1 << 6),
   D3D12_ST_FLAG_SPRITES_ENABLE        = (1 << 7),
   D3D12_ST_FLAG_MENU_ENABLE           = (1 << 8),
   D3D12_ST_FLAG_MENU_FULLSCREEN       = (1 << 9),
   D3D12_ST_FLAG_HDR_SUPPORT           = (1 << 10),
   D3D12_ST_FLAG_HDR_ENABLE            = (1 << 11),
   D3D12_ST_FLAG_VSYNC                 = (1 << 12),
   D3D12_ST_FLAG_WAITABLE_SWAPCHAINS   = (1 << 13),
   D3D12_ST_FLAG_WAIT_FOR_VBLANK       = (1 << 14)
};

typedef struct
{
   unsigned    cur_mon_id;
#ifdef __WINRT__
   DXGIFactory2 factory;
#else
   DXGIFactory1 factory;
#endif
   DXGIAdapter adapter;
   D3D12Device device;
 
#ifdef DEVICE_DEBUG
#ifdef DEBUG
   D3D12DebugDevice debug_device;
   D3D12InfoQueue info_queue;
   D3D12DeviceRemovedExtendedDataSettings device_removed_info;
#endif /* DEBUG */
#endif /* DEVICE_DEBUG */

   IDXGIAdapter1 *adapters[D3D12_MAX_GPU_COUNT];
   struct string_list *gpu_list;

   struct
   {
      D3D12CommandQueue        handle;
      D3D12CommandAllocator    allocator;
      D3D12GraphicsCommandList cmd;
      D3D12Fence               fence;
      HANDLE                   fenceEvent;
      UINT64                   fenceValue;
   } queue;

   struct
   {
      D3D12RootSignature      cs_rootSignature; /* descriptor layout */
      D3D12RootSignature      sl_rootSignature; /* descriptor layout */
      D3D12RootSignature      rootSignature;    /* descriptor layout */
      d3d12_descriptor_heap_t srv_heap;         /* ShaderResouceView descritor heap */
      d3d12_descriptor_heap_t rtv_heap;         /* RenderTargetView descritor heap */
      d3d12_descriptor_heap_t sampler_heap;
   } desc;

   struct
   {
      HANDLE                      frameLatencyWaitableObject;
      DXGISwapChain               handle;
      D3D12Resource               renderTargets[2];
#ifdef HAVE_DXGI_HDR
      d3d12_texture_t             back_buffer;
#endif
      D3D12_CPU_DESCRIPTOR_HANDLE desc_handles[2];
      D3D12_VIEWPORT              viewport;
      D3D12_RECT                  scissorRect;
      float                       clearcolor[4];
      int                         frame_index;
      unsigned                    swap_interval;
#ifdef HAVE_DXGI_HDR
      enum dxgi_swapchain_bit_depth bit_depth;
      DXGI_COLOR_SPACE_TYPE       color_space;
      DXGI_FORMAT                 formats[DXGI_SWAPCHAIN_BIT_DEPTH_COUNT];
#endif
   } chain;

   struct
   {
      d3d12_texture_t                 texture[GFX_MAX_FRAME_HISTORY + 1];
      D3D12Resource                   ubo;
      D3D12_CONSTANT_BUFFER_VIEW_DESC ubo_view;
      D3D12Resource                   vbo;
      D3D12_VERTEX_BUFFER_VIEW        vbo_view;
      D3D12_VIEWPORT                  viewport;
      D3D12_RECT                      scissorRect;
      float4_t                        output_size;
      int                             rotation;
   } frame;

#ifdef HAVE_DXGI_HDR
   struct
   {
      dxgi_hdr_uniform_t               ubo_values;
      D3D12Resource                    ubo;
      D3D12_CONSTANT_BUFFER_VIEW_DESC  ubo_view;
      float                            max_output_nits;
      float                            min_output_nits;
      float                            max_cll;
      float                            max_fall;
   } hdr;
#endif

   struct
   {
      D3D12Resource            vbo;
      D3D12_VERTEX_BUFFER_VIEW vbo_view;
      d3d12_texture_t          texture;

      float alpha;
   } menu;

   struct
   {
      D3D12PipelineStateRef    pipe;
      D3D12PipelineState       pipe_blend;
      D3D12PipelineState       pipe_noblend;
      D3D12PipelineState       pipe_font;
      D3D12Resource            vbo;
      D3D12_VERTEX_BUFFER_VIEW vbo_view;
      int                      offset;
      int                      capacity;
   } sprites;

#ifdef HAVE_OVERLAY
   struct
   {
      D3D12Resource            vbo;
      D3D12_VERTEX_BUFFER_VIEW vbo_view;
      d3d12_texture_t*         textures;
      int                      count;
   } overlays;
#endif

   struct
   {
      D3D12PipelineState              pipe;
      D3D12_GPU_DESCRIPTOR_HANDLE     sampler;
      D3D12Resource                   buffers[SLANG_CBUFFER_MAX];
      D3D12_CONSTANT_BUFFER_VIEW_DESC buffer_view[SLANG_CBUFFER_MAX];
      d3d12_texture_t                 rt;
      d3d12_texture_t                 feedback;
      D3D12_VIEWPORT                  viewport;
      D3D12_RECT                      scissorRect;
      pass_semantics_t                semantics;
      uint32_t                        frame_count;
      int32_t                         frame_direction;
      D3D12_GPU_DESCRIPTOR_HANDLE     textures;
      D3D12_GPU_DESCRIPTOR_HANDLE     samplers;
   } pass[GFX_MAX_SHADERS];

   struct video_shader* shader_preset;
   d3d12_texture_t      luts[GFX_MAX_TEXTURES];

   D3D12PipelineState              pipes[GFX_MAX_SHADERS];
   D3D12PipelineState              mipmapgen_pipe;
   d3d12_uniform_t                 ubo_values;
   D3D12Resource                   ubo;
   D3D12_CONSTANT_BUFFER_VIEW_DESC ubo_view;
   DXGI_FORMAT                     format;
   D3D12_GPU_DESCRIPTOR_HANDLE     samplers[RARCH_FILTER_MAX][RARCH_WRAP_MAX];
   math_matrix_4x4                 mvp, mvp_no_rot, identity;
   struct video_viewport           vp;
   D3D12Resource                   menu_pipeline_vbo;
   D3D12_VERTEX_BUFFER_VIEW        menu_pipeline_vbo_view;

#ifdef DEBUG
   D3D12Debug debugController;
#endif
   uint16_t flags;
} d3d12_video_t;

typedef enum
{
   ROOT_ID_TEXTURE_T = 0,
   ROOT_ID_SAMPLER_T,
   ROOT_ID_UBO,
   ROOT_ID_PC,
   ROOT_ID_MAX
} root_signature_parameter_index_t;

typedef enum
{
   CS_ROOT_ID_TEXTURE_T = 0,
   CS_ROOT_ID_UAV_T,
   CS_ROOT_ID_CONSTANTS,
   CS_ROOT_ID_MAX
} compute_root_index_t;

static INLINE HRESULT
D3D12Map(void* resource, UINT subresource, D3D12_RANGE* read_range, void** data)
{
   return ((ID3D12Resource*)resource)
         ->lpVtbl->Map((ID3D12Resource*)resource, subresource, read_range, data);
}
static INLINE void D3D12Unmap(void* resource, UINT subresource, D3D12_RANGE* written_range)
{
   ((ID3D12Resource*)resource)
         ->lpVtbl->Unmap((ID3D12Resource*)resource, subresource, written_range);
}
static INLINE D3D12_GPU_VIRTUAL_ADDRESS D3D12GetGPUVirtualAddress(void* resource)
{
   return ((ID3D12Resource*)resource)->lpVtbl->GetGPUVirtualAddress((ID3D12Resource*)resource);
}

static INLINE HRESULT D3D12ResetCommandAllocator(D3D12CommandAllocator command_allocator)
{
   return command_allocator->lpVtbl->Reset(command_allocator);
}
static INLINE ULONG  D3D12ReleaseFence(D3D12Fence fence) { return fence->lpVtbl->Release(fence); }
static INLINE UINT64 D3D12GetCompletedValue(D3D12Fence fence)
{
   return fence->lpVtbl->GetCompletedValue(fence);
}
static INLINE HRESULT D3D12SetEventOnCompletion(D3D12Fence fence, UINT64 value, HANDLE h_event)
{
   return fence->lpVtbl->SetEventOnCompletion(fence, value, h_event);
}

static INLINE HRESULT D3D12CloseGraphicsCommandList(D3D12GraphicsCommandList graphics_command_list)
{
   return graphics_command_list->lpVtbl->Close(graphics_command_list);
}
static INLINE HRESULT D3D12ResetGraphicsCommandList(
      D3D12GraphicsCommandList graphics_command_list,
      D3D12CommandAllocator    allocator,
      D3D12PipelineState       initial_state)
{
   return graphics_command_list->lpVtbl->Reset(graphics_command_list, allocator, initial_state);
}

static INLINE void D3D12DrawInstanced(
      D3D12GraphicsCommandList graphics_command_list,
      UINT                     vertex_count_per_instance,
      UINT                     instance_count,
      UINT                     start_vertex_location,
      UINT                     start_instance_location)
{
   graphics_command_list->lpVtbl->DrawInstanced(
         graphics_command_list, vertex_count_per_instance, instance_count, start_vertex_location,
         start_instance_location);
}

static INLINE void D3D12Dispatch(
      D3D12GraphicsCommandList graphics_command_list,
      UINT                     thread_group_count_x,
      UINT                     thread_group_count_y,
      UINT                     thread_group_count_z)
{
   graphics_command_list->lpVtbl->Dispatch(
         graphics_command_list, thread_group_count_x, thread_group_count_y, thread_group_count_z);
}

static INLINE void D3D12CopyTextureRegion(
      D3D12GraphicsCommandList     graphics_command_list,
      D3D12_TEXTURE_COPY_LOCATION* dst,
      UINT                         dst_x,
      UINT                         dst_y,
      UINT                         dst_z,
      D3D12_TEXTURE_COPY_LOCATION* src,
      D3D12_BOX*                   src_box)
{
   graphics_command_list->lpVtbl->CopyTextureRegion(
         graphics_command_list, dst, dst_x, dst_y, dst_z, src, src_box);
}

static INLINE void D3D12IASetPrimitiveTopology(
      D3D12GraphicsCommandList graphics_command_list, D3D12_PRIMITIVE_TOPOLOGY primitive_topology)
{
   graphics_command_list->lpVtbl->IASetPrimitiveTopology(graphics_command_list, primitive_topology);
}

static INLINE void D3D12RSSetViewports(
      D3D12GraphicsCommandList graphics_command_list, UINT num_viewports, D3D12_VIEWPORT* viewports)
{
   graphics_command_list->lpVtbl->RSSetViewports(graphics_command_list, num_viewports, viewports);
}

static INLINE void D3D12RSSetScissorRects(
      D3D12GraphicsCommandList graphics_command_list, UINT num_rects, D3D12_RECT* rects)
{
   graphics_command_list->lpVtbl->RSSetScissorRects(graphics_command_list, num_rects, rects);
}

static INLINE void D3D12SetPipelineState(
      D3D12GraphicsCommandList graphics_command_list, D3D12PipelineStateRef pipeline_state)
{
   graphics_command_list->lpVtbl->SetPipelineState(graphics_command_list, (D3D12PipelineState)pipeline_state);
}

static INLINE void D3D12ResourceBarrier(
      D3D12GraphicsCommandList graphics_command_list,
      UINT                     num_barriers,
      D3D12_RESOURCE_BARRIER*  barriers)
{
   graphics_command_list->lpVtbl->ResourceBarrier(graphics_command_list, num_barriers, barriers);
}

static INLINE void D3D12SetComputeRootSignature(
      D3D12GraphicsCommandList graphics_command_list, D3D12RootSignature root_signature)
{
   graphics_command_list->lpVtbl->SetComputeRootSignature(graphics_command_list, root_signature);
}

static INLINE void D3D12SetGraphicsRootSignature(
      D3D12GraphicsCommandList graphics_command_list, D3D12RootSignature root_signature)
{
   graphics_command_list->lpVtbl->SetGraphicsRootSignature(graphics_command_list, root_signature);
}

static INLINE void D3D12SetComputeRootDescriptorTable(
      D3D12GraphicsCommandList    graphics_command_list,
      UINT                        root_parameter_index,
      D3D12_GPU_DESCRIPTOR_HANDLE base_descriptor)
{
   graphics_command_list->lpVtbl->SetComputeRootDescriptorTable(
         graphics_command_list, root_parameter_index, base_descriptor);
}

static INLINE void D3D12SetGraphicsRootDescriptorTable(
      D3D12GraphicsCommandList    graphics_command_list,
      UINT                        root_parameter_index,
      D3D12_GPU_DESCRIPTOR_HANDLE base_descriptor)
{
   graphics_command_list->lpVtbl->SetGraphicsRootDescriptorTable(
         graphics_command_list, root_parameter_index, base_descriptor);
}

static INLINE void D3D12SetComputeRoot32BitConstants(
      D3D12GraphicsCommandList graphics_command_list,
      UINT                     root_parameter_index,
      UINT                     num32_bit_values_to_set,
      void*                    src_data,
      UINT                     dest_offset_in32_bit_values)
{
   graphics_command_list->lpVtbl->SetComputeRoot32BitConstants(
         graphics_command_list, root_parameter_index, num32_bit_values_to_set, src_data,
         dest_offset_in32_bit_values);
}

static INLINE void D3D12SetComputeRootConstantBufferView(
      D3D12GraphicsCommandList  graphics_command_list,
      UINT                      root_parameter_index,
      D3D12_GPU_VIRTUAL_ADDRESS buffer_location)
{
   graphics_command_list->lpVtbl->SetComputeRootConstantBufferView(
         graphics_command_list, root_parameter_index, buffer_location);
}
static INLINE void D3D12SetGraphicsRootConstantBufferView(
      D3D12GraphicsCommandList  graphics_command_list,
      UINT                      root_parameter_index,
      D3D12_GPU_VIRTUAL_ADDRESS buffer_location)
{
   graphics_command_list->lpVtbl->SetGraphicsRootConstantBufferView(
         graphics_command_list, root_parameter_index, buffer_location);
}

static INLINE void D3D12IASetVertexBuffers(
      D3D12GraphicsCommandList  graphics_command_list,
      UINT                      start_slot,
      UINT                      num_views,
      D3D12_VERTEX_BUFFER_VIEW* views)
{
   graphics_command_list->lpVtbl->IASetVertexBuffers(
         graphics_command_list, start_slot, num_views, views);
}

static INLINE void D3D12OMSetRenderTargets(
      D3D12GraphicsCommandList     graphics_command_list,
      UINT                         num_render_target_descriptors,
      D3D12_CPU_DESCRIPTOR_HANDLE* render_target_descriptors,
      BOOL                         r_ts_single_handle_to_descriptor_range,
      D3D12_CPU_DESCRIPTOR_HANDLE* depth_stencil_descriptor)
{
   graphics_command_list->lpVtbl->OMSetRenderTargets(
         graphics_command_list, num_render_target_descriptors, render_target_descriptors,
         r_ts_single_handle_to_descriptor_range, depth_stencil_descriptor);
}

static INLINE HRESULT
D3D12SignalCommandQueue(D3D12CommandQueue command_queue, D3D12Fence fence, UINT64 value)
{
   return command_queue->lpVtbl->Signal(command_queue, fence, value);
}

static INLINE HRESULT D3D12CreateCommandQueue(
      D3D12Device device, D3D12_COMMAND_QUEUE_DESC* desc, ID3D12CommandQueue** out)
{
   return device->lpVtbl->CreateCommandQueue(device, desc, uuidof(ID3D12CommandQueue), (void**)out);
}

static INLINE HRESULT D3D12CreateCommandAllocator(
      D3D12Device device, D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator** out)
{
   return device->lpVtbl->CreateCommandAllocator(
         device, type, uuidof(ID3D12CommandAllocator), (void**)out);
}

static INLINE HRESULT D3D12CreateGraphicsPipelineState(
      D3D12Device device, D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc, ID3D12PipelineState** out)
{
   return device->lpVtbl->CreateGraphicsPipelineState(
         device, desc, uuidof(ID3D12PipelineState), (void**)out);
}

static INLINE HRESULT D3D12CreateComputePipelineState(
      D3D12Device device, D3D12_COMPUTE_PIPELINE_STATE_DESC* desc, ID3D12PipelineState** out)
{
   return device->lpVtbl->CreateComputePipelineState(
         device, desc, uuidof(ID3D12PipelineState), (void**)out);
}

static INLINE HRESULT D3D12CheckFeatureSupport(
      D3D12Device   device,
      D3D12_FEATURE feature,
      void*         feature_support_data,
      UINT          feature_support_data_size)
{
   return device->lpVtbl->CheckFeatureSupport(
         device, feature, feature_support_data, feature_support_data_size);
}

static INLINE HRESULT D3D12CreateDescriptorHeap(
      D3D12Device                 device,
      D3D12_DESCRIPTOR_HEAP_DESC* descriptor_heap_desc,
      D3D12DescriptorHeap*        out)
{
   return device->lpVtbl->CreateDescriptorHeap(
         device, descriptor_heap_desc, uuidof(ID3D12DescriptorHeap), (void**)out);
}

static INLINE UINT D3D12GetDescriptorHandleIncrementSize(
      D3D12Device device, D3D12_DESCRIPTOR_HEAP_TYPE descriptor_heap_type)
{
   return device->lpVtbl->GetDescriptorHandleIncrementSize(device, descriptor_heap_type);
}

static INLINE HRESULT D3D12CreateRootSignature(
      D3D12Device           device,
      UINT                  node_mask,
      void*                 blob_with_root_signature,
      SIZE_T                blob_length_in_bytes,
      ID3D12RootSignature** out)
{
   return device->lpVtbl->CreateRootSignature(
         device, node_mask, blob_with_root_signature, blob_length_in_bytes,
         uuidof(ID3D12RootSignature), (void**)out);
}

static INLINE void D3D12CreateShaderResourceView(
      D3D12Device                      device,
      D3D12Resource                    resource,
      D3D12_SHADER_RESOURCE_VIEW_DESC* desc,
      D3D12_CPU_DESCRIPTOR_HANDLE      dest_descriptor)
{
   device->lpVtbl->CreateShaderResourceView(device, resource, desc, dest_descriptor);
}

static INLINE void D3D12CreateUnorderedAccessView(
      D3D12Device                       device,
      void*                             resource,
      void*                             counter_resource,
      D3D12_UNORDERED_ACCESS_VIEW_DESC* desc,
      D3D12_CPU_DESCRIPTOR_HANDLE       dest_descriptor)
{
   device->lpVtbl->CreateUnorderedAccessView(
         device, (ID3D12Resource*)resource, (ID3D12Resource*)counter_resource, desc,
         dest_descriptor);
}

static INLINE void D3D12CreateRenderTargetView(
      D3D12Device                    device,
      void*                          resource,
      D3D12_RENDER_TARGET_VIEW_DESC* desc,
      D3D12_CPU_DESCRIPTOR_HANDLE    dest_descriptor)
{
   device->lpVtbl->CreateRenderTargetView(device, (ID3D12Resource*)resource, desc, dest_descriptor);
}
static INLINE void D3D12CreateDepthStencilView(
      D3D12Device                    device,
      void*                          resource,
      D3D12_DEPTH_STENCIL_VIEW_DESC* desc,
      D3D12_CPU_DESCRIPTOR_HANDLE    dest_descriptor)
{
   device->lpVtbl->CreateDepthStencilView(device, (ID3D12Resource*)resource, desc, dest_descriptor);
}

static INLINE void D3D12CreateSampler(
      D3D12Device device, D3D12_SAMPLER_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE dest_descriptor)
{
   device->lpVtbl->CreateSampler(device, desc, dest_descriptor);
}

static INLINE HRESULT D3D12CreateCommittedResource(
      D3D12Device            device,
      D3D12_HEAP_PROPERTIES* heap_properties,
      D3D12_HEAP_FLAGS       heap_flags,
      D3D12_RESOURCE_DESC*   desc,
      D3D12_RESOURCE_STATES  initial_resource_state,
      D3D12_CLEAR_VALUE*     optimized_clear_value,
      ID3D12Resource**       out)
{
   return device->lpVtbl->CreateCommittedResource(
         device, heap_properties, heap_flags, desc, initial_resource_state, optimized_clear_value,
         uuidof(ID3D12Resource), (void**)out);
}

static INLINE HRESULT D3D12CreateFence(
      D3D12Device device, UINT64 initial_value, D3D12_FENCE_FLAGS flags, ID3D12Fence** out)
{
   return device->lpVtbl->CreateFence(
         device, initial_value, flags, uuidof(ID3D12Fence), (void**)out);
}

static INLINE void D3D12GetCopyableFootprints(
      D3D12Device                         device,
      D3D12_RESOURCE_DESC*                resource_desc,
      UINT                                first_subresource,
      UINT                                num_subresources,
      UINT64                              base_offset,
      D3D12_PLACED_SUBRESOURCE_FOOTPRINT* layouts,
      UINT*                               num_rows,
      UINT64*                             row_size_in_bytes,
      UINT64*                             total_bytes)
{
   device->lpVtbl->GetCopyableFootprints(
         device, resource_desc, first_subresource, num_subresources, base_offset, layouts, num_rows,
         row_size_in_bytes, total_bytes);
}

/* end of auto-generated */
#ifdef DEBUG
static INLINE HRESULT D3D12GetDebugInterface_(D3D12Debug* out)
{
   return D3D12GetDebugInterface(uuidof(ID3D12Debug), (void**)out);
}
#endif

static INLINE HRESULT
D3D12CreateDevice_(DXGIAdapter adapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, D3D12Device* out)
{
   return D3D12CreateDevice(
         (IUnknown*)adapter, MinimumFeatureLevel, uuidof(ID3D12Device), (void**)out);
}

static INLINE HRESULT D3D12CreateGraphicsCommandList(
      D3D12Device               device,
      UINT                      node_mask,
      D3D12_COMMAND_LIST_TYPE   type,
      D3D12CommandAllocator     command_allocator,
      D3D12PipelineState        initial_state,
      D3D12GraphicsCommandList* out)
{
   return device->lpVtbl->CreateCommandList(
         device, node_mask, type, command_allocator, initial_state,
         uuidof(ID3D12GraphicsCommandList), (void**)out);
}

static INLINE void D3D12ClearRenderTargetView(
      D3D12GraphicsCommandList    command_list,
      D3D12_CPU_DESCRIPTOR_HANDLE render_target_view,
      const FLOAT                 colorRGBA[4],
      UINT                        num_rects,
      const D3D12_RECT*           rects)
{
   command_list->lpVtbl->ClearRenderTargetView(
         command_list, render_target_view, colorRGBA, num_rects, rects);
}

static INLINE void D3D12ExecuteGraphicsCommandLists(
      D3D12CommandQueue               command_queue,
      UINT                            num_command_lists,
      const D3D12GraphicsCommandList* command_lists)
{
   command_queue->lpVtbl->ExecuteCommandLists(
         command_queue, num_command_lists, (ID3D12CommandList* const*)command_lists);
}

static INLINE HRESULT
DXGIGetSwapChainBuffer(DXGISwapChain swapchain, UINT buffer, D3D12Resource* surface)
{
   return swapchain->lpVtbl->GetBuffer(swapchain, buffer, uuidof(ID3D12Resource), (void**)surface);
}

static INLINE void D3D12SetDescriptorHeaps(
      D3D12GraphicsCommandList   command_list,
      UINT                       num_descriptor_heaps,
      const D3D12DescriptorHeap* descriptor_heaps)
{
   command_list->lpVtbl->SetDescriptorHeaps(command_list, num_descriptor_heaps, descriptor_heaps);
}

static INLINE D3D12_CPU_DESCRIPTOR_HANDLE
D3D12GetCPUDescriptorHandleForHeapStart(D3D12DescriptorHeap descriptor_heap)
{
   D3D12_CPU_DESCRIPTOR_HANDLE out;
   ((void(STDMETHODCALLTYPE*)(ID3D12DescriptorHeap*, D3D12_CPU_DESCRIPTOR_HANDLE*))
          descriptor_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart)(descriptor_heap, &out);
   return out;
}

static INLINE D3D12_GPU_DESCRIPTOR_HANDLE
D3D12GetGPUDescriptorHandleForHeapStart(D3D12DescriptorHeap descriptor_heap)
{
   D3D12_GPU_DESCRIPTOR_HANDLE out;
   ((void(STDMETHODCALLTYPE*)(ID3D12DescriptorHeap*, D3D12_GPU_DESCRIPTOR_HANDLE*))
          descriptor_heap->lpVtbl->GetGPUDescriptorHandleForHeapStart)(descriptor_heap, &out);
   return out;
}

RETRO_BEGIN_DECLS

D3D12_CPU_DESCRIPTOR_HANDLE d3d12_descriptor_heap_slot_alloc(d3d12_descriptor_heap_t* heap);

D3D12_GPU_VIRTUAL_ADDRESS
d3d12_create_buffer(D3D12Device device, UINT size_in_bytes, D3D12Resource* buffer);

void d3d12_init_texture(D3D12Device device, d3d12_texture_t* tex);
void d3d12_release_texture(d3d12_texture_t* texture);

void d3d12_update_texture(
      int              width,
      int              height,
      int              pitch,
      DXGI_FORMAT      format,
      const void*      data,
      d3d12_texture_t* texture);

void d3d12_upload_texture(D3D12GraphicsCommandList cmd,
      d3d12_texture_t* texture, void *userdata);

#if !defined(__cplusplus) || defined(CINTERFACE)
static INLINE void d3d12_resource_transition(
      D3D12GraphicsCommandList cmd,
      D3D12Resource            resource,
      D3D12_RESOURCE_STATES    state_before,
      D3D12_RESOURCE_STATES    state_after)
{
   D3D12_RESOURCE_BARRIER barrier;
   barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
   barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
   barrier.Transition.pResource   = resource;
   barrier.Transition.StateBefore = state_before;
   barrier.Transition.StateAfter  = state_after;
   barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
   D3D12ResourceBarrier(cmd, 1, &barrier);
}

static INLINE void d3d12_set_texture(D3D12GraphicsCommandList cmd, const d3d12_texture_t* texture)
{
   D3D12SetGraphicsRootDescriptorTable(cmd, ROOT_ID_TEXTURE_T, texture->gpu_descriptor[0]);
}

static INLINE void
d3d12_set_sampler(D3D12GraphicsCommandList cmd, D3D12_GPU_DESCRIPTOR_HANDLE sampler)
{
   D3D12SetGraphicsRootDescriptorTable(cmd, ROOT_ID_SAMPLER_T, sampler);
}

static INLINE void
d3d12_set_texture_and_sampler(D3D12GraphicsCommandList cmd, const d3d12_texture_t* texture)
{
   D3D12SetGraphicsRootDescriptorTable(cmd, ROOT_ID_TEXTURE_T, texture->gpu_descriptor[0]);
   D3D12SetGraphicsRootDescriptorTable(cmd, ROOT_ID_SAMPLER_T, texture->sampler);
}

#endif

RETRO_END_DECLS
