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

#include "dxgi_common.h"
#include <d3d12.h>

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
typedef ID3D12Debug*                              D3D12Debug;
typedef ID3D12DebugDevice*                        D3D12DebugDevice;
typedef ID3D12DebugCommandQueue*                  D3D12DebugCommandQueue;
typedef ID3D12DebugCommandList*                   D3D12DebugCommandList;
typedef ID3D12InfoQueue*                          D3D12InfoQueue;

static INLINE ULONG D3D12Release(void* object)
{
   return ((ID3D12Object*)object)->lpVtbl->Release((ID3D12Object*)object);
}
static INLINE ULONG D3D12ReleaseDeviceChild(D3D12DeviceChild device_child)
{
   return device_child->lpVtbl->Release(device_child);
}
static INLINE ULONG D3D12ReleaseRootSignature(D3D12RootSignature root_signature)
{
   return root_signature->lpVtbl->Release(root_signature);
}
static INLINE ULONG
D3D12ReleaseRootSignatureDeserializer(D3D12RootSignatureDeserializer root_signature_deserializer)
{
   return root_signature_deserializer->lpVtbl->Release(root_signature_deserializer);
}
static INLINE const D3D12_ROOT_SIGNATURE_DESC*
D3D12GetRootSignatureDesc(D3D12RootSignatureDeserializer root_signature_deserializer)
{
   return root_signature_deserializer->lpVtbl->GetRootSignatureDesc(root_signature_deserializer);
}
static INLINE ULONG D3D12ReleaseVersionedRootSignatureDeserializer(
      D3D12VersionedRootSignatureDeserializer versioned_root_signature_deserializer)
{
   return versioned_root_signature_deserializer->lpVtbl->Release(
         versioned_root_signature_deserializer);
}
static INLINE HRESULT D3D12GetRootSignatureDescAtVersion(
      D3D12VersionedRootSignatureDeserializer     versioned_root_signature_deserializer,
      D3D_ROOT_SIGNATURE_VERSION                  convert_to_version,
      const D3D12_VERSIONED_ROOT_SIGNATURE_DESC** desc)
{
   return versioned_root_signature_deserializer->lpVtbl->GetRootSignatureDescAtVersion(
         versioned_root_signature_deserializer, convert_to_version, desc);
}
static INLINE const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* D3D12GetUnconvertedRootSignatureDesc(
      D3D12VersionedRootSignatureDeserializer versioned_root_signature_deserializer)
{
   return versioned_root_signature_deserializer->lpVtbl->GetUnconvertedRootSignatureDesc(
         versioned_root_signature_deserializer);
}
static INLINE ULONG D3D12ReleasePageable(D3D12Pageable pageable)
{
   return pageable->lpVtbl->Release(pageable);
}
static INLINE ULONG D3D12ReleaseHeap(D3D12Heap heap) { return heap->lpVtbl->Release(heap); }
static INLINE ULONG D3D12ReleaseResource(void* resource)
{
   return ((ID3D12Resource*)resource)->lpVtbl->Release((ID3D12Resource*)resource);
}
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
static INLINE HRESULT D3D12WriteToSubresource(
      void*      resource,
      UINT       dst_subresource,
      D3D12_BOX* dst_box,
      void*      src_data,
      UINT       src_row_pitch,
      UINT       src_depth_pitch)
{
   return ((ID3D12Resource*)resource)
         ->lpVtbl->WriteToSubresource(
               (ID3D12Resource*)resource, dst_subresource, dst_box, src_data, src_row_pitch,
               src_depth_pitch);
}
static INLINE HRESULT D3D12ReadFromSubresource(
      void*      resource,
      void*      dst_data,
      UINT       dst_row_pitch,
      UINT       dst_depth_pitch,
      UINT       src_subresource,
      D3D12_BOX* src_box)
{
   return ((ID3D12Resource*)resource)
         ->lpVtbl->ReadFromSubresource(
               (ID3D12Resource*)resource, dst_data, dst_row_pitch, dst_depth_pitch, src_subresource,
               src_box);
}
static INLINE HRESULT D3D12GetHeapProperties(
      void* resource, D3D12_HEAP_PROPERTIES* heap_properties, D3D12_HEAP_FLAGS* heap_flags)
{
   return ((ID3D12Resource*)resource)
         ->lpVtbl->GetHeapProperties((ID3D12Resource*)resource, heap_properties, heap_flags);
}
static INLINE ULONG D3D12ReleaseCommandAllocator(D3D12CommandAllocator command_allocator)
{
   return command_allocator->lpVtbl->Release(command_allocator);
}
static INLINE HRESULT D3D12ResetCommandAllocator(D3D12CommandAllocator command_allocator)
{
   return command_allocator->lpVtbl->Reset(command_allocator);
}
static INLINE ULONG  D3D12ReleaseFence(D3D12Fence fence) { return fence->lpVtbl->Release(fence); }
static INLINE UINT64 D3D12GetCompletedValue(D3D12Fence fence)
{
   return fence && fence->lpVtbl->GetCompletedValue(fence);
}
static INLINE HRESULT D3D12SetEventOnCompletion(D3D12Fence fence, UINT64 value, HANDLE h_event)
{
   return fence->lpVtbl->SetEventOnCompletion(fence, value, h_event);
}
static INLINE HRESULT D3D12SignalFence(D3D12Fence fence, UINT64 value)
{
   return fence->lpVtbl->Signal(fence, value);
}
static INLINE ULONG D3D12ReleasePipelineState(D3D12PipelineState pipeline_state)
{
   return pipeline_state->lpVtbl->Release(pipeline_state);
}
static INLINE HRESULT D3D12GetCachedBlob(D3D12PipelineState pipeline_state, ID3DBlob** blob)
{
   return pipeline_state->lpVtbl->GetCachedBlob(pipeline_state, blob);
}
static INLINE ULONG D3D12ReleaseDescriptorHeap(D3D12DescriptorHeap descriptor_heap)
{
   return descriptor_heap->lpVtbl->Release(descriptor_heap);
}
static INLINE ULONG D3D12ReleaseQueryHeap(D3D12QueryHeap query_heap)
{
   return query_heap->lpVtbl->Release(query_heap);
}
static INLINE ULONG D3D12ReleaseCommandSignature(D3D12CommandSignature command_signature)
{
   return command_signature->lpVtbl->Release(command_signature);
}
static INLINE ULONG D3D12ReleaseCommandList(D3D12CommandList command_list)
{
   return command_list->lpVtbl->Release(command_list);
}
static INLINE ULONG D3D12ReleaseGraphicsCommandList(D3D12GraphicsCommandList graphics_command_list)
{
   return graphics_command_list->lpVtbl->Release(graphics_command_list);
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
static INLINE void
D3D12ClearState(D3D12GraphicsCommandList graphics_command_list, D3D12PipelineState pipeline_state)
{
   graphics_command_list->lpVtbl->ClearState(graphics_command_list, pipeline_state);
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
static INLINE void D3D12DrawIndexedInstanced(
      D3D12GraphicsCommandList graphics_command_list,
      UINT                     index_count_per_instance,
      UINT                     instance_count,
      UINT                     start_index_location,
      INT                      base_vertex_location,
      UINT                     start_instance_location)
{
   graphics_command_list->lpVtbl->DrawIndexedInstanced(
         graphics_command_list, index_count_per_instance, instance_count, start_index_location,
         base_vertex_location, start_instance_location);
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
static INLINE void D3D12CopyBufferRegion(
      D3D12GraphicsCommandList graphics_command_list,
      D3D12Resource            dst_buffer,
      UINT64                   dst_offset,
      D3D12Resource            src_buffer,
      UINT64                   src_offset,
      UINT64                   num_bytes)
{
   graphics_command_list->lpVtbl->CopyBufferRegion(
         graphics_command_list, dst_buffer, dst_offset, (ID3D12Resource*)src_buffer, src_offset,
         num_bytes);
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
static INLINE void D3D12CopyResource(
      D3D12GraphicsCommandList graphics_command_list, void* dst_resource, void* src_resource)
{
   graphics_command_list->lpVtbl->CopyResource(
         graphics_command_list, (ID3D12Resource*)dst_resource, (ID3D12Resource*)src_resource);
}
static INLINE void D3D12CopyTiles(
      D3D12GraphicsCommandList         graphics_command_list,
      void*                            tiled_resource,
      D3D12_TILED_RESOURCE_COORDINATE* tile_region_start_coordinate,
      D3D12_TILE_REGION_SIZE*          tile_region_size,
      void*                            buffer,
      UINT64                           buffer_start_offset_in_bytes,
      D3D12_TILE_COPY_FLAGS            flags)
{
   graphics_command_list->lpVtbl->CopyTiles(
         graphics_command_list, (ID3D12Resource*)tiled_resource, tile_region_start_coordinate,
         tile_region_size, (ID3D12Resource*)buffer, buffer_start_offset_in_bytes, flags);
}
static INLINE void D3D12ResolveSubresource(
      D3D12GraphicsCommandList graphics_command_list,
      void*                    dst_resource,
      UINT                     dst_subresource,
      void*                    src_resource,
      UINT                     src_subresource,
      DXGI_FORMAT              format)
{
   graphics_command_list->lpVtbl->ResolveSubresource(
         graphics_command_list, (ID3D12Resource*)dst_resource, dst_subresource,
         (ID3D12Resource*)src_resource, src_subresource, format);
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
static INLINE void
D3D12OMSetStencilRef(D3D12GraphicsCommandList graphics_command_list, UINT stencil_ref)
{
   graphics_command_list->lpVtbl->OMSetStencilRef(graphics_command_list, stencil_ref);
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
static INLINE void D3D12ExecuteBundle(
      D3D12GraphicsCommandList graphics_command_list, D3D12GraphicsCommandList command_list)
{
   graphics_command_list->lpVtbl->ExecuteBundle(graphics_command_list, command_list);
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
static INLINE void D3D12SetComputeRoot32BitConstant(
      D3D12GraphicsCommandList graphics_command_list,
      UINT                     root_parameter_index,
      UINT                     src_data,
      UINT                     dest_offset_in32_bit_values)
{
   graphics_command_list->lpVtbl->SetComputeRoot32BitConstant(
         graphics_command_list, root_parameter_index, src_data, dest_offset_in32_bit_values);
}
static INLINE void D3D12SetGraphicsRoot32BitConstant(
      D3D12GraphicsCommandList graphics_command_list,
      UINT                     root_parameter_index,
      UINT                     src_data,
      UINT                     dest_offset_in32_bit_values)
{
   graphics_command_list->lpVtbl->SetGraphicsRoot32BitConstant(
         graphics_command_list, root_parameter_index, src_data, dest_offset_in32_bit_values);
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
static INLINE void D3D12SetGraphicsRoot32BitConstants(
      D3D12GraphicsCommandList graphics_command_list,
      UINT                     root_parameter_index,
      UINT                     num32_bit_values_to_set,
      void*                    src_data,
      UINT                     dest_offset_in32_bit_values)
{
   graphics_command_list->lpVtbl->SetGraphicsRoot32BitConstants(
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
static INLINE void D3D12SetComputeRootShaderResourceView(
      D3D12GraphicsCommandList  graphics_command_list,
      UINT                      root_parameter_index,
      D3D12_GPU_VIRTUAL_ADDRESS buffer_location)
{
   graphics_command_list->lpVtbl->SetComputeRootShaderResourceView(
         graphics_command_list, root_parameter_index, buffer_location);
}
static INLINE void D3D12SetGraphicsRootShaderResourceView(
      D3D12GraphicsCommandList  graphics_command_list,
      UINT                      root_parameter_index,
      D3D12_GPU_VIRTUAL_ADDRESS buffer_location)
{
   graphics_command_list->lpVtbl->SetGraphicsRootShaderResourceView(
         graphics_command_list, root_parameter_index, buffer_location);
}
static INLINE void D3D12SetComputeRootUnorderedAccessView(
      D3D12GraphicsCommandList  graphics_command_list,
      UINT                      root_parameter_index,
      D3D12_GPU_VIRTUAL_ADDRESS buffer_location)
{
   graphics_command_list->lpVtbl->SetComputeRootUnorderedAccessView(
         graphics_command_list, root_parameter_index, buffer_location);
}
static INLINE void D3D12SetGraphicsRootUnorderedAccessView(
      D3D12GraphicsCommandList  graphics_command_list,
      UINT                      root_parameter_index,
      D3D12_GPU_VIRTUAL_ADDRESS buffer_location)
{
   graphics_command_list->lpVtbl->SetGraphicsRootUnorderedAccessView(
         graphics_command_list, root_parameter_index, buffer_location);
}
static INLINE void
D3D12IASetIndexBuffer(D3D12GraphicsCommandList graphics_command_list, D3D12_INDEX_BUFFER_VIEW* view)
{
   graphics_command_list->lpVtbl->IASetIndexBuffer(graphics_command_list, view);
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
static INLINE void D3D12SOSetTargets(
      D3D12GraphicsCommandList         graphics_command_list,
      UINT                             start_slot,
      UINT                             num_views,
      D3D12_STREAM_OUTPUT_BUFFER_VIEW* views)
{
   graphics_command_list->lpVtbl->SOSetTargets(graphics_command_list, start_slot, num_views, views);
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
static INLINE void D3D12ClearDepthStencilView(
      D3D12GraphicsCommandList    graphics_command_list,
      D3D12_CPU_DESCRIPTOR_HANDLE depth_stencil_view,
      D3D12_CLEAR_FLAGS           clear_flags,
      FLOAT                       depth,
      UINT8                       stencil,
      UINT                        num_rects,
      D3D12_RECT*                 rects)
{
   graphics_command_list->lpVtbl->ClearDepthStencilView(
         graphics_command_list, depth_stencil_view, clear_flags, depth, stencil, num_rects, rects);
}
static INLINE void D3D12DiscardResource(
      D3D12GraphicsCommandList graphics_command_list, void* resource, D3D12_DISCARD_REGION* region)
{
   graphics_command_list->lpVtbl->DiscardResource(
         graphics_command_list, (ID3D12Resource*)resource, region);
}
static INLINE void D3D12BeginQuery(
      D3D12GraphicsCommandList graphics_command_list,
      D3D12QueryHeap           query_heap,
      D3D12_QUERY_TYPE         type,
      UINT                     index)
{
   graphics_command_list->lpVtbl->BeginQuery(graphics_command_list, query_heap, type, index);
}
static INLINE void D3D12EndQuery(
      D3D12GraphicsCommandList graphics_command_list,
      D3D12QueryHeap           query_heap,
      D3D12_QUERY_TYPE         type,
      UINT                     index)
{
   graphics_command_list->lpVtbl->EndQuery(graphics_command_list, query_heap, type, index);
}
static INLINE void D3D12ResolveQueryData(
      D3D12GraphicsCommandList graphics_command_list,
      D3D12QueryHeap           query_heap,
      D3D12_QUERY_TYPE         type,
      UINT                     start_index,
      UINT                     num_queries,
      void*                    destination_buffer,
      UINT64                   aligned_destination_buffer_offset)
{
   graphics_command_list->lpVtbl->ResolveQueryData(
         graphics_command_list, query_heap, type, start_index, num_queries,
         (ID3D12Resource*)destination_buffer, aligned_destination_buffer_offset);
}
static INLINE void D3D12SetPredication(
      D3D12GraphicsCommandList graphics_command_list,
      void*                    buffer,
      UINT64                   aligned_buffer_offset,
      D3D12_PREDICATION_OP     operation)
{
   graphics_command_list->lpVtbl->SetPredication(
         graphics_command_list, (ID3D12Resource*)buffer, aligned_buffer_offset, operation);
}
static INLINE void D3D12SetGraphicsCommandListMarker(
      D3D12GraphicsCommandList graphics_command_list, UINT metadata, void* data, UINT size)
{
   graphics_command_list->lpVtbl->SetMarker(graphics_command_list, metadata, data, size);
}
static INLINE void D3D12BeginGraphicsCommandListEvent(
      D3D12GraphicsCommandList graphics_command_list, UINT metadata, void* data, UINT size)
{
   graphics_command_list->lpVtbl->BeginEvent(graphics_command_list, metadata, data, size);
}
static INLINE void D3D12EndGraphicsCommandListEvent(D3D12GraphicsCommandList graphics_command_list)
{
   graphics_command_list->lpVtbl->EndEvent(graphics_command_list);
}
static INLINE void D3D12ExecuteIndirect(
      D3D12GraphicsCommandList graphics_command_list,
      D3D12CommandSignature    command_signature,
      UINT                     max_command_count,
      void*                    argument_buffer,
      UINT64                   argument_buffer_offset,
      void*                    count_buffer,
      UINT64                   count_buffer_offset)
{
   graphics_command_list->lpVtbl->ExecuteIndirect(
         graphics_command_list, command_signature, max_command_count,
         (ID3D12Resource*)argument_buffer, argument_buffer_offset, (ID3D12Resource*)count_buffer,
         count_buffer_offset);
}
static INLINE ULONG D3D12ReleaseCommandQueue(D3D12CommandQueue command_queue)
{
   return command_queue->lpVtbl->Release(command_queue);
}
static INLINE void D3D12UpdateTileMappings(
      D3D12CommandQueue                command_queue,
      void*                            resource,
      UINT                             num_resource_regions,
      D3D12_TILED_RESOURCE_COORDINATE* resource_region_start_coordinates,
      D3D12_TILE_REGION_SIZE*          resource_region_sizes,
      D3D12Heap                        heap,
      UINT                             num_ranges,
      D3D12_TILE_RANGE_FLAGS*          range_flags,
      UINT*                            heap_range_start_offsets,
      UINT*                            range_tile_counts,
      D3D12_TILE_MAPPING_FLAGS         flags)
{
   command_queue->lpVtbl->UpdateTileMappings(
         command_queue, (ID3D12Resource*)resource, num_resource_regions,
         resource_region_start_coordinates, resource_region_sizes, heap, num_ranges, range_flags,
         heap_range_start_offsets, range_tile_counts, flags);
}
static INLINE void D3D12CopyTileMappings(
      D3D12CommandQueue                command_queue,
      void*                            dst_resource,
      D3D12_TILED_RESOURCE_COORDINATE* dst_region_start_coordinate,
      void*                            src_resource,
      D3D12_TILED_RESOURCE_COORDINATE* src_region_start_coordinate,
      D3D12_TILE_REGION_SIZE*          region_size,
      D3D12_TILE_MAPPING_FLAGS         flags)
{
   command_queue->lpVtbl->CopyTileMappings(
         command_queue, (ID3D12Resource*)dst_resource, dst_region_start_coordinate,
         (ID3D12Resource*)src_resource, src_region_start_coordinate, region_size, flags);
}
static INLINE void
D3D12SetCommandQueueMarker(D3D12CommandQueue command_queue, UINT metadata, void* data, UINT size)
{
   command_queue->lpVtbl->SetMarker(command_queue, metadata, data, size);
}
static INLINE void
D3D12BeginCommandQueueEvent(D3D12CommandQueue command_queue, UINT metadata, void* data, UINT size)
{
   command_queue->lpVtbl->BeginEvent(command_queue, metadata, data, size);
}
static INLINE void D3D12EndCommandQueueEvent(D3D12CommandQueue command_queue)
{
   command_queue->lpVtbl->EndEvent(command_queue);
}
static INLINE HRESULT
D3D12SignalCommandQueue(D3D12CommandQueue command_queue, D3D12Fence fence, UINT64 value)
{
   return command_queue->lpVtbl->Signal(command_queue, fence, value);
}
static INLINE HRESULT D3D12Wait(D3D12CommandQueue command_queue, D3D12Fence fence, UINT64 value)
{
   return command_queue->lpVtbl->Wait(command_queue, fence, value);
}
static INLINE HRESULT D3D12GetTimestampFrequency(D3D12CommandQueue command_queue, UINT64* frequency)
{
   return command_queue->lpVtbl->GetTimestampFrequency(command_queue, frequency);
}
static INLINE HRESULT D3D12GetClockCalibration(
      D3D12CommandQueue command_queue, UINT64* gpu_timestamp, UINT64* cpu_timestamp)
{
   return command_queue->lpVtbl->GetClockCalibration(command_queue, gpu_timestamp, cpu_timestamp);
}
static INLINE ULONG D3D12ReleaseDevice(D3D12Device device)
{
   return device->lpVtbl->Release(device);
}
static INLINE UINT D3D12GetNodeCount(D3D12Device device)
{
   return device->lpVtbl->GetNodeCount(device);
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
static INLINE HRESULT D3D12CreateCommandList(
      D3D12Device             device,
      UINT                    node_mask,
      D3D12_COMMAND_LIST_TYPE type,
      D3D12CommandAllocator   command_allocator,
      D3D12PipelineState      initial_state,
      ID3D12CommandList**     out)
{
   return device->lpVtbl->CreateCommandList(
         device, node_mask, type, command_allocator, initial_state, uuidof(ID3D12CommandList),
         (void**)out);
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
static INLINE void D3D12CreateConstantBufferView(
      D3D12Device                      device,
      D3D12_CONSTANT_BUFFER_VIEW_DESC* desc,
      D3D12_CPU_DESCRIPTOR_HANDLE      dest_descriptor)
{
   device->lpVtbl->CreateConstantBufferView(device, desc, dest_descriptor);
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
static INLINE void D3D12CopyDescriptors(
      D3D12Device                  device,
      UINT                         num_dest_descriptor_ranges,
      D3D12_CPU_DESCRIPTOR_HANDLE* dest_descriptor_range_starts,
      UINT*                        dest_descriptor_range_sizes,
      UINT                         num_src_descriptor_ranges,
      D3D12_CPU_DESCRIPTOR_HANDLE* src_descriptor_range_starts,
      UINT*                        src_descriptor_range_sizes,
      D3D12_DESCRIPTOR_HEAP_TYPE   descriptor_heaps_type)
{
   device->lpVtbl->CopyDescriptors(
         device, num_dest_descriptor_ranges, dest_descriptor_range_starts,
         dest_descriptor_range_sizes, num_src_descriptor_ranges, src_descriptor_range_starts,
         src_descriptor_range_sizes, descriptor_heaps_type);
}
static INLINE void D3D12CopyDescriptorsSimple(
      D3D12Device                 device,
      UINT                        num_descriptors,
      D3D12_CPU_DESCRIPTOR_HANDLE dest_descriptor_range_start,
      D3D12_CPU_DESCRIPTOR_HANDLE src_descriptor_range_start,
      D3D12_DESCRIPTOR_HEAP_TYPE  descriptor_heaps_type)
{
   device->lpVtbl->CopyDescriptorsSimple(
         device, num_descriptors, dest_descriptor_range_start, src_descriptor_range_start,
         descriptor_heaps_type);
}
static INLINE D3D12_RESOURCE_ALLOCATION_INFO D3D12GetResourceAllocationInfo(
      D3D12Device          device,
      UINT                 visible_mask,
      UINT                 num_resource_descs,
      D3D12_RESOURCE_DESC* resource_descs)
{
   return device->lpVtbl->GetResourceAllocationInfo(
         device, visible_mask, num_resource_descs, resource_descs);
}
static INLINE D3D12_HEAP_PROPERTIES
D3D12GetCustomHeapProperties(D3D12Device device, UINT node_mask, D3D12_HEAP_TYPE heap_type)
{
   return device->lpVtbl->GetCustomHeapProperties(device, node_mask, heap_type);
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
static INLINE HRESULT D3D12CreateHeap(D3D12Device device, D3D12_HEAP_DESC* desc, ID3D12Heap** out)
{
   return device->lpVtbl->CreateHeap(device, desc, uuidof(ID3D12Heap), (void**)out);
}
static INLINE HRESULT D3D12CreatePlacedResource(
      D3D12Device           device,
      D3D12Heap             heap,
      UINT64                heap_offset,
      D3D12_RESOURCE_DESC*  desc,
      D3D12_RESOURCE_STATES initial_state,
      D3D12_CLEAR_VALUE*    optimized_clear_value,
      ID3D12Resource**      out)
{
   return device->lpVtbl->CreatePlacedResource(
         device, heap, heap_offset, desc, initial_state, optimized_clear_value,
         uuidof(ID3D12Resource), (void**)out);
}
static INLINE HRESULT D3D12CreateReservedResource(
      D3D12Device           device,
      D3D12_RESOURCE_DESC*  desc,
      D3D12_RESOURCE_STATES initial_state,
      D3D12_CLEAR_VALUE*    optimized_clear_value,
      ID3D12Resource**      out)
{
   return device->lpVtbl->CreateReservedResource(
         device, desc, initial_state, optimized_clear_value, uuidof(ID3D12Resource), (void**)out);
}
static INLINE HRESULT D3D12CreateFence(
      D3D12Device device, UINT64 initial_value, D3D12_FENCE_FLAGS flags, ID3D12Fence** out)
{
   return device->lpVtbl->CreateFence(
         device, initial_value, flags, uuidof(ID3D12Fence), (void**)out);
}
static INLINE HRESULT D3D12GetDeviceRemovedReason(D3D12Device device)
{
   return device->lpVtbl->GetDeviceRemovedReason(device);
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
static INLINE HRESULT
D3D12CreateQueryHeap(D3D12Device device, D3D12_QUERY_HEAP_DESC* desc, ID3D12Heap** out)
{
   return device->lpVtbl->CreateQueryHeap(device, desc, uuidof(ID3D12Heap), (void**)out);
}
static INLINE HRESULT D3D12SetStablePowerState(D3D12Device device, BOOL enable)
{
   return device->lpVtbl->SetStablePowerState(device, enable);
}
static INLINE HRESULT D3D12CreateCommandSignature(
      D3D12Device                   device,
      D3D12_COMMAND_SIGNATURE_DESC* desc,
      D3D12RootSignature            root_signature,
      ID3D12CommandSignature**      out)
{
   return device->lpVtbl->CreateCommandSignature(
         device, desc, root_signature, uuidof(ID3D12CommandSignature), (void**)out);
}
static INLINE void D3D12GetResourceTiling(
      D3D12Device               device,
      void*                     tiled_resource,
      UINT*                     num_tiles_for_entire_resource,
      D3D12_PACKED_MIP_INFO*    packed_mip_desc,
      D3D12_TILE_SHAPE*         standard_tile_shape_for_non_packed_mips,
      UINT*                     num_subresource_tilings,
      UINT                      first_subresource_tiling_to_get,
      D3D12_SUBRESOURCE_TILING* subresource_tilings_for_non_packed_mips)
{
   device->lpVtbl->GetResourceTiling(
         device, (ID3D12Resource*)tiled_resource, num_tiles_for_entire_resource, packed_mip_desc,
         standard_tile_shape_for_non_packed_mips, num_subresource_tilings,
         first_subresource_tiling_to_get, subresource_tilings_for_non_packed_mips);
}
static INLINE LUID D3D12GetAdapterLuid(D3D12Device device)
{
   return device->lpVtbl->GetAdapterLuid(device);
}
static INLINE ULONG D3D12ReleasePipelineLibrary(D3D12PipelineLibrary pipeline_library)
{
   return pipeline_library->lpVtbl->Release(pipeline_library);
}
static INLINE HRESULT
D3D12StorePipeline(D3D12PipelineLibrary pipeline_library, LPCWSTR name, D3D12PipelineState pipeline)
{
   return pipeline_library->lpVtbl->StorePipeline(pipeline_library, name, pipeline);
}
static INLINE HRESULT D3D12LoadGraphicsPipeline(
      D3D12PipelineLibrary                pipeline_library,
      LPCWSTR                             name,
      D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc,
      ID3D12PipelineState**               out)
{
   return pipeline_library->lpVtbl->LoadGraphicsPipeline(
         pipeline_library, name, desc, uuidof(ID3D12PipelineState), (void**)out);
}
static INLINE HRESULT D3D12LoadComputePipeline(
      D3D12PipelineLibrary               pipeline_library,
      LPCWSTR                            name,
      D3D12_COMPUTE_PIPELINE_STATE_DESC* desc,
      ID3D12PipelineState**              out)
{
   return pipeline_library->lpVtbl->LoadComputePipeline(
         pipeline_library, name, desc, uuidof(ID3D12PipelineState), (void**)out);
}
static INLINE SIZE_T D3D12GetSerializedSize(D3D12PipelineLibrary pipeline_library)
{
   return pipeline_library->lpVtbl->GetSerializedSize(pipeline_library);
}
static INLINE HRESULT
D3D12Serialize(D3D12PipelineLibrary pipeline_library, void* data, SIZE_T data_size_in_bytes)
{
   return pipeline_library->lpVtbl->Serialize(pipeline_library, data, data_size_in_bytes);
}
static INLINE ULONG D3D12ReleaseDebug(D3D12Debug debug) { return debug->lpVtbl->Release(debug); }
static INLINE void  D3D12EnableDebugLayer(D3D12Debug debug)
{
   debug->lpVtbl->EnableDebugLayer(debug);
}
static INLINE ULONG D3D12ReleaseDebugDevice(D3D12DebugDevice debug_device)
{
   return debug_device->lpVtbl->Release(debug_device);
}
static INLINE HRESULT
D3D12SetDebugDeviceFeatureMask(D3D12DebugDevice debug_device, D3D12_DEBUG_FEATURE mask)
{
   return debug_device->lpVtbl->SetFeatureMask(debug_device, mask);
}
static INLINE D3D12_DEBUG_FEATURE D3D12GetDebugDeviceFeatureMask(D3D12DebugDevice debug_device)
{
   return debug_device->lpVtbl->GetFeatureMask(debug_device);
}
static INLINE HRESULT
D3D12ReportLiveDeviceObjects(D3D12DebugDevice debug_device, D3D12_RLDO_FLAGS flags)
{
   return debug_device->lpVtbl->ReportLiveDeviceObjects(debug_device, flags);
}
static INLINE ULONG D3D12ReleaseDebugCommandQueue(D3D12DebugCommandQueue debug_command_queue)
{
   return debug_command_queue->lpVtbl->Release(debug_command_queue);
}
static INLINE BOOL D3D12AssertDebugCommandQueueResourceState(
      D3D12DebugCommandQueue debug_command_queue, void* resource, UINT subresource, UINT state)
{
   return debug_command_queue->lpVtbl->AssertResourceState(
         debug_command_queue, (ID3D12Resource*)resource, subresource, state);
}
static INLINE ULONG D3D12ReleaseDebugCommandList(D3D12DebugCommandList debug_command_list)
{
   return debug_command_list->lpVtbl->Release(debug_command_list);
}
static INLINE BOOL D3D12AssertDebugCommandListResourceState(
      D3D12DebugCommandList debug_command_list, void* resource, UINT subresource, UINT state)
{
   return debug_command_list->lpVtbl->AssertResourceState(
         debug_command_list, (ID3D12Resource*)resource, subresource, state);
}
static INLINE HRESULT D3D12SetDebugCommandListFeatureMask(
      D3D12DebugCommandList debug_command_list, D3D12_DEBUG_FEATURE mask)
{
   return debug_command_list->lpVtbl->SetFeatureMask(debug_command_list, mask);
}
static INLINE D3D12_DEBUG_FEATURE
D3D12GetDebugCommandListFeatureMask(D3D12DebugCommandList debug_command_list)
{
   return debug_command_list->lpVtbl->GetFeatureMask(debug_command_list);
}
static INLINE ULONG D3D12ReleaseInfoQueue(D3D12InfoQueue info_queue)
{
   return info_queue->lpVtbl->Release(info_queue);
}
static INLINE HRESULT
D3D12SetMessageCountLimit(D3D12InfoQueue info_queue, UINT64 message_count_limit)
{
   return info_queue->lpVtbl->SetMessageCountLimit(info_queue, message_count_limit);
}
static INLINE void D3D12ClearStoredMessages(D3D12InfoQueue info_queue)
{
   info_queue->lpVtbl->ClearStoredMessages(info_queue);
}
#ifndef __WINRT__
static INLINE HRESULT D3D12GetMessageA(
      D3D12InfoQueue info_queue,
      UINT64         message_index,
      D3D12_MESSAGE* message,
      SIZE_T*        message_byte_length)
{
   return info_queue->lpVtbl->GetMessageA(info_queue, message_index, message, message_byte_length);
}
#endif
static INLINE UINT64 D3D12GetNumMessagesAllowedByStorageFilter(D3D12InfoQueue info_queue)
{
   return info_queue->lpVtbl->GetNumMessagesAllowedByStorageFilter(info_queue);
}
static INLINE UINT64 D3D12GetNumMessagesDeniedByStorageFilter(D3D12InfoQueue info_queue)
{
   return info_queue->lpVtbl->GetNumMessagesDeniedByStorageFilter(info_queue);
}
static INLINE UINT64 D3D12GetNumStoredMessages(D3D12InfoQueue info_queue)
{
   return info_queue->lpVtbl->GetNumStoredMessages(info_queue);
}
static INLINE UINT64 D3D12GetNumStoredMessagesAllowedByRetrievalFilter(D3D12InfoQueue info_queue)
{
   return info_queue->lpVtbl->GetNumStoredMessagesAllowedByRetrievalFilter(info_queue);
}
static INLINE UINT64 D3D12GetNumMessagesDiscardedByMessageCountLimit(D3D12InfoQueue info_queue)
{
   return info_queue->lpVtbl->GetNumMessagesDiscardedByMessageCountLimit(info_queue);
}
static INLINE UINT64 D3D12GetMessageCountLimit(D3D12InfoQueue info_queue)
{
   return info_queue->lpVtbl->GetMessageCountLimit(info_queue);
}
static INLINE HRESULT
D3D12AddStorageFilterEntries(D3D12InfoQueue info_queue, D3D12_INFO_QUEUE_FILTER* filter)
{
   return info_queue->lpVtbl->AddStorageFilterEntries(info_queue, filter);
}
static INLINE HRESULT D3D12GetStorageFilter(
      D3D12InfoQueue info_queue, D3D12_INFO_QUEUE_FILTER* filter, SIZE_T* filter_byte_length)
{
   return info_queue->lpVtbl->GetStorageFilter(info_queue, filter, filter_byte_length);
}
static INLINE void D3D12ClearStorageFilter(D3D12InfoQueue info_queue)
{
   info_queue->lpVtbl->ClearStorageFilter(info_queue);
}
static INLINE HRESULT D3D12PushEmptyStorageFilter(D3D12InfoQueue info_queue)
{
   return info_queue->lpVtbl->PushEmptyStorageFilter(info_queue);
}
static INLINE HRESULT D3D12PushCopyOfStorageFilter(D3D12InfoQueue info_queue)
{
   return info_queue->lpVtbl->PushCopyOfStorageFilter(info_queue);
}
static INLINE HRESULT
D3D12PushStorageFilter(D3D12InfoQueue info_queue, D3D12_INFO_QUEUE_FILTER* filter)
{
   return info_queue->lpVtbl->PushStorageFilter(info_queue, filter);
}
static INLINE void D3D12PopStorageFilter(D3D12InfoQueue info_queue)
{
   info_queue->lpVtbl->PopStorageFilter(info_queue);
}
static INLINE UINT D3D12GetStorageFilterStackSize(D3D12InfoQueue info_queue)
{
   return info_queue->lpVtbl->GetStorageFilterStackSize(info_queue);
}
static INLINE HRESULT
D3D12AddRetrievalFilterEntries(D3D12InfoQueue info_queue, D3D12_INFO_QUEUE_FILTER* filter)
{
   return info_queue->lpVtbl->AddRetrievalFilterEntries(info_queue, filter);
}
static INLINE HRESULT D3D12GetRetrievalFilter(
      D3D12InfoQueue info_queue, D3D12_INFO_QUEUE_FILTER* filter, SIZE_T* filter_byte_length)
{
   return info_queue->lpVtbl->GetRetrievalFilter(info_queue, filter, filter_byte_length);
}
static INLINE void D3D12ClearRetrievalFilter(D3D12InfoQueue info_queue)
{
   info_queue->lpVtbl->ClearRetrievalFilter(info_queue);
}
static INLINE HRESULT D3D12PushEmptyRetrievalFilter(D3D12InfoQueue info_queue)
{
   return info_queue->lpVtbl->PushEmptyRetrievalFilter(info_queue);
}
static INLINE HRESULT D3D12PushCopyOfRetrievalFilter(D3D12InfoQueue info_queue)
{
   return info_queue->lpVtbl->PushCopyOfRetrievalFilter(info_queue);
}
static INLINE HRESULT
D3D12PushRetrievalFilter(D3D12InfoQueue info_queue, D3D12_INFO_QUEUE_FILTER* filter)
{
   return info_queue->lpVtbl->PushRetrievalFilter(info_queue, filter);
}
static INLINE void D3D12PopRetrievalFilter(D3D12InfoQueue info_queue)
{
   info_queue->lpVtbl->PopRetrievalFilter(info_queue);
}
static INLINE UINT D3D12GetRetrievalFilterStackSize(D3D12InfoQueue info_queue)
{
   return info_queue->lpVtbl->GetRetrievalFilterStackSize(info_queue);
}
static INLINE HRESULT D3D12AddMessage(
      D3D12InfoQueue         info_queue,
      D3D12_MESSAGE_CATEGORY category,
      D3D12_MESSAGE_SEVERITY severity,
      D3D12_MESSAGE_ID       i_d,
      LPCSTR                 description)
{
   return info_queue->lpVtbl->AddMessage(info_queue, category, severity, i_d, description);
}
static INLINE HRESULT D3D12AddApplicationMessage(
      D3D12InfoQueue info_queue, D3D12_MESSAGE_SEVERITY severity, LPCSTR description)
{
   return info_queue->lpVtbl->AddApplicationMessage(info_queue, severity, description);
}
static INLINE HRESULT
D3D12SetBreakOnCategory(D3D12InfoQueue info_queue, D3D12_MESSAGE_CATEGORY category, BOOL b_enable)
{
   return info_queue->lpVtbl->SetBreakOnCategory(info_queue, category, b_enable);
}
static INLINE HRESULT
D3D12SetBreakOnSeverity(D3D12InfoQueue info_queue, D3D12_MESSAGE_SEVERITY severity, BOOL b_enable)
{
   return info_queue->lpVtbl->SetBreakOnSeverity(info_queue, severity, b_enable);
}
static INLINE HRESULT
D3D12SetBreakOnID(D3D12InfoQueue info_queue, D3D12_MESSAGE_ID i_d, BOOL b_enable)
{
   return info_queue->lpVtbl->SetBreakOnID(info_queue, i_d, b_enable);
}
static INLINE BOOL
D3D12GetBreakOnCategory(D3D12InfoQueue info_queue, D3D12_MESSAGE_CATEGORY category)
{
   return info_queue->lpVtbl->GetBreakOnCategory(info_queue, category);
}
static INLINE BOOL
D3D12GetBreakOnSeverity(D3D12InfoQueue info_queue, D3D12_MESSAGE_SEVERITY severity)
{
   return info_queue->lpVtbl->GetBreakOnSeverity(info_queue, severity);
}
static INLINE BOOL D3D12GetBreakOnID(D3D12InfoQueue info_queue, D3D12_MESSAGE_ID i_d)
{
   return info_queue->lpVtbl->GetBreakOnID(info_queue, i_d);
}
static INLINE void D3D12SetMuteDebugOutput(D3D12InfoQueue info_queue, BOOL b_mute)
{
   info_queue->lpVtbl->SetMuteDebugOutput(info_queue, b_mute);
}
static INLINE BOOL D3D12GetMuteDebugOutput(D3D12InfoQueue info_queue)
{
   return info_queue->lpVtbl->GetMuteDebugOutput(info_queue);
}

/* end of auto-generated */

static INLINE HRESULT D3D12GetDebugInterface_(D3D12Debug* out)
{
   return D3D12GetDebugInterface(uuidof(ID3D12Debug), (void**)out);
}

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

static INLINE void D3D12ExecuteCommandLists(
      D3D12CommandQueue       command_queue,
      UINT                    num_command_lists,
      const D3D12CommandList* command_lists)
{
   command_queue->lpVtbl->ExecuteCommandLists(command_queue, num_command_lists, command_lists);
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
#if 0 /* function prototype is wrong ... */
static INLINE D3D12_CPU_DESCRIPTOR_HANDLE D3D12GetCPUDescriptorHandleForHeapStart(
   D3D12DescriptorHeap descriptor_heap)
{
   return descriptor_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(descriptor_heap);
}
static INLINE D3D12_GPU_DESCRIPTOR_HANDLE D3D12GetGPUDescriptorHandleForHeapStart(
   D3D12DescriptorHeap descriptor_heap)
{
   return descriptor_heap->lpVtbl->GetGPUDescriptorHandleForHeapStart(descriptor_heap);
}
#else
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
#endif

   /* internal */

#include <retro_math.h>
#include <retro_common_api.h>
#include <gfx/math/matrix_4x4.h>

#include "../common/d3dcompiler_common.h"
#include "../../retroarch.h"
#include "../drivers_shader/slang_process.h"

#define D3D12_MAX_GPU_COUNT 16

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
   UINT32 colors[4];
   struct
   {
      float scaling;
      float rotation;
   } params;
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
   bool                               dirty;
   float4_t                           size_data;
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

static_assert(
      (!(sizeof(d3d12_uniform_t) & 0xF)), "sizeof(d3d12_uniform_t) must be a multiple of 16");

typedef struct
{
   unsigned    cur_mon_id;
#ifdef __WINRT__
   DXGIFactory2 factory;
#else
   DXGIFactory factory;
#endif
   DXGIAdapter adapter;
   D3D12Device device;

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
      DXGISwapChain               handle;
      D3D12Resource               renderTargets[2];
      D3D12_CPU_DESCRIPTOR_HANDLE desc_handles[2];
      D3D12_VIEWPORT              viewport;
      D3D12_RECT                  scissorRect;
      float                       clearcolor[4];
      int                         frame_index;
      bool                        vsync;
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

   struct
   {
      D3D12Resource            vbo;
      D3D12_VERTEX_BUFFER_VIEW vbo_view;
      d3d12_texture_t          texture;

      float alpha;
      bool  enabled;
      bool  fullscreen;
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
      bool                     enabled;
   } sprites;

#ifdef HAVE_OVERLAY
   struct
   {
      D3D12Resource            vbo;
      D3D12_VERTEX_BUFFER_VIEW vbo_view;
      d3d12_texture_t*         textures;
      bool                     enabled;
      bool                     fullscreen;
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
   math_matrix_4x4                 mvp, mvp_no_rot;
   struct video_viewport           vp;
   bool                            resize_chain;
   bool                            keep_aspect;
   bool                            resize_viewport;
   bool                            resize_render_targets;
   bool                            init_history;
   D3D12Resource                   menu_pipeline_vbo;
   D3D12_VERTEX_BUFFER_VIEW        menu_pipeline_vbo_view;

#ifdef DEBUG
   D3D12Debug debugController;
#endif
} d3d12_video_t;

typedef enum {
   ROOT_ID_TEXTURE_T = 0,
   ROOT_ID_SAMPLER_T,
   ROOT_ID_UBO,
   ROOT_ID_PC,
   ROOT_ID_MAX,
} root_signature_parameter_index_t;

typedef enum {
   CS_ROOT_ID_TEXTURE_T = 0,
   CS_ROOT_ID_UAV_T,
   CS_ROOT_ID_CONSTANTS,
   CS_ROOT_ID_MAX,
} compute_root_index_t;

RETRO_BEGIN_DECLS

extern D3D12_RENDER_TARGET_BLEND_DESC d3d12_blend_enable_desc;

bool d3d12_init_base(d3d12_video_t* d3d12);

bool d3d12_init_descriptors(d3d12_video_t* d3d12);
void d3d12_init_samplers(d3d12_video_t* d3d12);

bool d3d12_init_pipeline(
      D3D12Device                         device,
      D3DBlob                             vs_code,
      D3DBlob                             ps_code,
      D3DBlob                             gs_code,
      D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc,
      D3D12PipelineState*                 out);

bool d3d12_init_swapchain(d3d12_video_t* d3d12, int width, int height, void *corewindow);

bool d3d12_init_queue(d3d12_video_t* d3d12);

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

void d3d12_create_fullscreen_quad_vbo(
      D3D12Device device, D3D12_VERTEX_BUFFER_VIEW* view, D3D12Resource* vbo);

DXGI_FORMAT d3d12_get_closest_match(D3D12Device device, D3D12_FEATURE_DATA_FORMAT_SUPPORT* desired);

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
