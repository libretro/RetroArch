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

#pragma once

#include <retro_inline.h>

#include "dxgi_common.h"
#include <d3d10.h>

#include "../drivers_shader/slang_process.h"

typedef const ID3D10SamplerState*       D3D10SamplerStateRef;

typedef ID3D10InputLayout*       D3D10InputLayout;
typedef ID3D10RasterizerState*   D3D10RasterizerState;
typedef ID3D10DepthStencilState* D3D10DepthStencilState;
typedef ID3D10BlendState*        D3D10BlendState;
typedef ID3D10PixelShader*       D3D10PixelShader;
typedef ID3D10SamplerState*      D3D10SamplerState;
typedef ID3D10VertexShader*      D3D10VertexShader;
typedef ID3D10GeometryShader*    D3D10GeometryShader;

/* auto-generated */
typedef ID3DDestructionNotifier*  D3DDestructionNotifier;
typedef ID3D10Resource*           D3D10Resource;
typedef ID3D10Buffer*             D3D10Buffer;
typedef ID3D10Texture1D*          D3D10Texture1D;
typedef ID3D10Texture2D*          D3D10Texture2D;
typedef ID3D10Texture3D*          D3D10Texture3D;
typedef ID3D10View*               D3D10View;
typedef ID3D10ShaderResourceView* D3D10ShaderResourceView;
typedef ID3D10RenderTargetView*   D3D10RenderTargetView;
typedef ID3D10DepthStencilView*   D3D10DepthStencilView;
typedef ID3D10Asynchronous*       D3D10Asynchronous;
typedef ID3D10Query*              D3D10Query;
typedef ID3D10Predicate*          D3D10Predicate;
typedef ID3D10Counter*            D3D10Counter;
typedef ID3D10Device*             D3D10Device;
typedef ID3D10Multithread*        D3D10Multithread;
typedef ID3D10Debug*              D3D10Debug;
typedef ID3D10SwitchToRef*        D3D10SwitchToRef;
typedef ID3D10InfoQueue*          D3D10InfoQueue;

#if !defined(__cplusplus) || defined(CINTERFACE)
static INLINE void D3D10SetResourceEvictionPriority(D3D10Resource resource, UINT eviction_priority)
{
   resource->lpVtbl->SetEvictionPriority(resource, eviction_priority);
}
static INLINE UINT D3D10GetResourceEvictionPriority(D3D10Resource resource)
{
   return resource->lpVtbl->GetEvictionPriority(resource);
}
static INLINE void D3D10SetBufferEvictionPriority(D3D10Buffer buffer, UINT eviction_priority)
{
   buffer->lpVtbl->SetEvictionPriority(buffer, eviction_priority);
}
static INLINE UINT D3D10GetBufferEvictionPriority(D3D10Buffer buffer)
{
   return buffer->lpVtbl->GetEvictionPriority(buffer);
}
static INLINE HRESULT
D3D10MapBuffer(D3D10Buffer buffer, D3D10_MAP map_type, UINT map_flags, void** data)
{
   return buffer->lpVtbl->Map(buffer, map_type, map_flags, data);
}
static INLINE void D3D10UnmapBuffer(D3D10Buffer buffer) { buffer->lpVtbl->Unmap(buffer); }
static INLINE void
D3D10SetTexture1DEvictionPriority(D3D10Texture1D texture1d, UINT eviction_priority)
{
   texture1d->lpVtbl->SetEvictionPriority(texture1d, eviction_priority);
}
static INLINE UINT D3D10GetTexture1DEvictionPriority(D3D10Texture1D texture1d)
{
   return texture1d->lpVtbl->GetEvictionPriority(texture1d);
}
static INLINE HRESULT D3D10MapTexture1D(
      D3D10Texture1D texture1d, UINT subresource, D3D10_MAP map_type, UINT map_flags, void** data)
{
   return texture1d->lpVtbl->Map(texture1d, subresource, map_type, map_flags, data);
}
static INLINE void D3D10UnmapTexture1D(D3D10Texture1D texture1d, UINT subresource)
{
   texture1d->lpVtbl->Unmap(texture1d, subresource);
}
static INLINE void
D3D10SetTexture2DEvictionPriority(D3D10Texture2D texture2d, UINT eviction_priority)
{
   texture2d->lpVtbl->SetEvictionPriority(texture2d, eviction_priority);
}
static INLINE UINT D3D10GetTexture2DEvictionPriority(D3D10Texture2D texture2d)
{
   return texture2d->lpVtbl->GetEvictionPriority(texture2d);
}
static INLINE HRESULT D3D10MapTexture2D(
      D3D10Texture2D          texture2d,
      UINT                    subresource,
      D3D10_MAP               map_type,
      UINT                    map_flags,
      D3D10_MAPPED_TEXTURE2D* mapped_tex2d)
{
   return texture2d->lpVtbl->Map(texture2d, subresource, map_type, map_flags, mapped_tex2d);
}
static INLINE void D3D10UnmapTexture2D(D3D10Texture2D texture2d, UINT subresource)
{
   texture2d->lpVtbl->Unmap(texture2d, subresource);
}
static INLINE void
D3D10SetTexture3DEvictionPriority(D3D10Texture3D texture3d, UINT eviction_priority)
{
   texture3d->lpVtbl->SetEvictionPriority(texture3d, eviction_priority);
}
static INLINE UINT D3D10GetTexture3DEvictionPriority(D3D10Texture3D texture3d)
{
   return texture3d->lpVtbl->GetEvictionPriority(texture3d);
}
static INLINE HRESULT D3D10MapTexture3D(
      D3D10Texture3D          texture3d,
      UINT                    subresource,
      D3D10_MAP               map_type,
      UINT                    map_flags,
      D3D10_MAPPED_TEXTURE3D* mapped_tex3d)
{
   return texture3d->lpVtbl->Map(texture3d, subresource, map_type, map_flags, mapped_tex3d);
}
static INLINE void D3D10UnmapTexture3D(D3D10Texture3D texture3d, UINT subresource)
{
   texture3d->lpVtbl->Unmap(texture3d, subresource);
}
static INLINE void D3D10GetViewResource(D3D10View view, D3D10Resource* resource)
{
   view->lpVtbl->GetResource(view, resource);
}
static INLINE void D3D10GetShaderResourceViewResource(
      D3D10ShaderResourceView shader_resource_view, D3D10Resource* resource)
{
   shader_resource_view->lpVtbl->GetResource(shader_resource_view, resource);
}
static INLINE void
D3D10GetRenderTargetViewResource(D3D10RenderTargetView render_target_view, D3D10Resource* resource)
{
   render_target_view->lpVtbl->GetResource(render_target_view, resource);
}
static INLINE void
D3D10GetDepthStencilViewResource(D3D10DepthStencilView depth_stencil_view, D3D10Resource* resource)
{
   depth_stencil_view->lpVtbl->GetResource(depth_stencil_view, resource);
}
static INLINE void D3D10BeginAsynchronous(D3D10Asynchronous asynchronous)
{
   asynchronous->lpVtbl->Begin(asynchronous);
}
static INLINE void D3D10EndAsynchronous(D3D10Asynchronous asynchronous)
{
   asynchronous->lpVtbl->End(asynchronous);
}
static INLINE HRESULT D3D10GetAsynchronousData(
      D3D10Asynchronous asynchronous, void* data, UINT data_size, UINT get_data_flags)
{
   return asynchronous->lpVtbl->GetData(asynchronous, data, data_size, get_data_flags);
}
static INLINE UINT D3D10GetAsynchronousDataSize(D3D10Asynchronous asynchronous)
{
   return asynchronous->lpVtbl->GetDataSize(asynchronous);
}
static INLINE void D3D10BeginQuery(D3D10Query query) { query->lpVtbl->Begin(query); }
static INLINE void D3D10EndQuery(D3D10Query query) { query->lpVtbl->End(query); }
static INLINE HRESULT
D3D10GetQueryData(D3D10Query query, void* data, UINT data_size, UINT get_data_flags)
{
   return query->lpVtbl->GetData(query, data, data_size, get_data_flags);
}
static INLINE UINT D3D10GetQueryDataSize(D3D10Query query)
{
   return query->lpVtbl->GetDataSize(query);
}
static INLINE void D3D10BeginPredicate(D3D10Predicate predicate)
{
   predicate->lpVtbl->Begin(predicate);
}
static INLINE void D3D10EndPredicate(D3D10Predicate predicate)
{
   predicate->lpVtbl->End(predicate);
}
static INLINE HRESULT
D3D10GetPredicateData(D3D10Predicate predicate, void* data, UINT data_size, UINT get_data_flags)
{
   return predicate->lpVtbl->GetData(predicate, data, data_size, get_data_flags);
}
static INLINE UINT D3D10GetPredicateDataSize(D3D10Predicate predicate)
{
   return predicate->lpVtbl->GetDataSize(predicate);
}
static INLINE void D3D10BeginCounter(D3D10Counter counter) { counter->lpVtbl->Begin(counter); }
static INLINE void D3D10EndCounter(D3D10Counter counter) { counter->lpVtbl->End(counter); }
static INLINE HRESULT
D3D10GetCounterData(D3D10Counter counter, void* data, UINT data_size, UINT get_data_flags)
{
   return counter->lpVtbl->GetData(counter, data, data_size, get_data_flags);
}
static INLINE UINT D3D10GetCounterDataSize(D3D10Counter counter)
{
   return counter->lpVtbl->GetDataSize(counter);
}
static INLINE void D3D10SetVShaderConstantBuffers(
      D3D10Device device, UINT start_slot, UINT num_buffers, D3D10Buffer* const constant_buffers)
{
   device->lpVtbl->VSSetConstantBuffers(device, start_slot, num_buffers, constant_buffers);
}
static INLINE void D3D10SetPShaderResources(
      D3D10Device                    device,
      UINT                           start_slot,
      UINT                           num_views,
      D3D10ShaderResourceView* const shader_resource_views)
{
   device->lpVtbl->PSSetShaderResources(device, start_slot, num_views, shader_resource_views);
}
static INLINE void D3D10SetPShader(D3D10Device device, D3D10PixelShader pixel_shader)
{
   device->lpVtbl->PSSetShader(device, pixel_shader);
}
static INLINE void D3D10SetPShaderSamplers(
      D3D10Device device, UINT start_slot, UINT num_samplers, D3D10SamplerState* const samplers)
{
   device->lpVtbl->PSSetSamplers(device, start_slot, num_samplers, samplers);
}
static INLINE void D3D10SetVShader(D3D10Device device, D3D10VertexShader vertex_shader)
{
   device->lpVtbl->VSSetShader(device, vertex_shader);
}

static INLINE void D3D10DrawIndexed(
      D3D10Device device, UINT index_count, UINT start_index_location, INT base_vertex_location)
{
   device->lpVtbl->DrawIndexed(device, index_count, start_index_location, base_vertex_location);
}
static INLINE void D3D10Draw(D3D10Device device, UINT vertex_count, UINT start_vertex_location)
{
   device->lpVtbl->Draw(device, vertex_count, start_vertex_location);
}
static INLINE void D3D10SetPShaderConstantBuffers(
      D3D10Device device, UINT start_slot, UINT num_buffers, D3D10Buffer* const constant_buffers)
{
   device->lpVtbl->PSSetConstantBuffers(device, start_slot, num_buffers, constant_buffers);
}
static INLINE void D3D10SetInputLayout(D3D10Device device, D3D10InputLayout input_layout)
{
   device->lpVtbl->IASetInputLayout(device, input_layout);
}
static INLINE void D3D10SetVertexBuffers(
      D3D10Device        device,
      UINT               start_slot,
      UINT               num_buffers,
      D3D10Buffer* const vertex_buffers,
      UINT*              strides,
      UINT*              offsets)
{
   device->lpVtbl->IASetVertexBuffers(
         device, start_slot, num_buffers, vertex_buffers, strides, offsets);
}

static INLINE void D3D10SetVertexBuffer(
      D3D10Device device_context,
      UINT               slot,
      D3D10Buffer const  vertex_buffer,
      UINT               stride,
      UINT               offset)
{
   D3D10SetVertexBuffers(device_context, slot, 1, (D3D10Buffer* const)&vertex_buffer, &stride, &offset);
}
static INLINE void D3D10SetVShaderConstantBuffer(
      D3D10Device device_context, UINT slot, D3D10Buffer const constant_buffer)
{
   D3D10SetVShaderConstantBuffers(device_context, slot, 1, (ID3D10Buffer ** const)&constant_buffer);
}

static INLINE void D3D10SetPShaderConstantBuffer(
      D3D10Device device_context, UINT slot, D3D10Buffer const constant_buffer)
{
   D3D10SetPShaderConstantBuffers(device_context, slot, 1, (ID3D10Buffer** const)&constant_buffer);
}

static INLINE void
D3D10SetIndexBuffer(D3D10Device device, D3D10Buffer index_buffer, DXGI_FORMAT format, UINT offset)
{
   device->lpVtbl->IASetIndexBuffer(device, index_buffer, format, offset);
}
static INLINE void D3D10DrawIndexedInstanced(
      D3D10Device device,
      UINT        index_count_per_instance,
      UINT        instance_count,
      UINT        start_index_location,
      INT         base_vertex_location,
      UINT        start_instance_location)
{
   device->lpVtbl->DrawIndexedInstanced(
         device, index_count_per_instance, instance_count, start_index_location,
         base_vertex_location, start_instance_location);
}
static INLINE void D3D10DrawInstanced(
      D3D10Device device,
      UINT        vertex_count_per_instance,
      UINT        instance_count,
      UINT        start_vertex_location,
      UINT        start_instance_location)
{
   device->lpVtbl->DrawInstanced(
         device, vertex_count_per_instance, instance_count, start_vertex_location,
         start_instance_location);
}
static INLINE void D3D10SetGShaderConstantBuffers(
      D3D10Device device, UINT start_slot, UINT num_buffers, D3D10Buffer* const constant_buffers)
{
   device->lpVtbl->GSSetConstantBuffers(device, start_slot, num_buffers, constant_buffers);
}
static INLINE void D3D10SetGShader(D3D10Device device, D3D10GeometryShader shader)
{
   device->lpVtbl->GSSetShader(device, shader);
}
static INLINE void D3D10SetPrimitiveTopology(D3D10Device device, D3D10_PRIMITIVE_TOPOLOGY topology)
{
   device->lpVtbl->IASetPrimitiveTopology(device, topology);
}
static INLINE void D3D10SetVShaderResources(
      D3D10Device                    device,
      UINT                           start_slot,
      UINT                           num_views,
      D3D10ShaderResourceView* const shader_resource_views)
{
   device->lpVtbl->VSSetShaderResources(device, start_slot, num_views, shader_resource_views);
}
static INLINE void D3D10SetVShaderSamplers(
      D3D10Device device, UINT start_slot, UINT num_samplers, D3D10SamplerState* const samplers)
{
   device->lpVtbl->VSSetSamplers(device, start_slot, num_samplers, samplers);
}
static INLINE void
D3D10SetPredication(D3D10Device device, D3D10Predicate predicate, BOOL predicate_value)
{
   device->lpVtbl->SetPredication(device, predicate, predicate_value);
}
static INLINE void D3D10SetGShaderResources(
      D3D10Device                    device,
      UINT                           start_slot,
      UINT                           num_views,
      D3D10ShaderResourceView* const shader_resource_views)
{
   device->lpVtbl->GSSetShaderResources(device, start_slot, num_views, shader_resource_views);
}
static INLINE void D3D10SetGShaderSamplers(
      D3D10Device device, UINT start_slot, UINT num_samplers, D3D10SamplerState* const samplers)
{
   device->lpVtbl->GSSetSamplers(device, start_slot, num_samplers, samplers);
}
static INLINE void D3D10SetRenderTargets(
      D3D10Device                  device,
      UINT                         num_views,
      D3D10RenderTargetView* const render_target_views,
      D3D10DepthStencilView        depth_stencil_view)
{
   device->lpVtbl->OMSetRenderTargets(device, num_views, render_target_views, depth_stencil_view);
}
static INLINE void D3D10SetBlendState(
      D3D10Device device, D3D10BlendState blend_state, FLOAT blend_factor[4], UINT sample_mask)
{
   device->lpVtbl->OMSetBlendState(device, blend_state, blend_factor, sample_mask);
}
static INLINE void D3D10SetDepthStencilState(
      D3D10Device device, D3D10DepthStencilState depth_stencil_state, UINT stencil_ref)
{
   device->lpVtbl->OMSetDepthStencilState(device, depth_stencil_state, stencil_ref);
}
static INLINE void
D3D10SOSetTargets(D3D10Device device, UINT num_buffers, D3D10Buffer* const sotargets, UINT* offsets)
{
   device->lpVtbl->SOSetTargets(device, num_buffers, sotargets, offsets);
}
static INLINE void D3D10DrawAuto(D3D10Device device) { device->lpVtbl->DrawAuto(device); }
static INLINE void D3D10SetState(D3D10Device device, D3D10RasterizerState rasterizer_state)
{
   device->lpVtbl->RSSetState(device, rasterizer_state);
}
static INLINE void
D3D10SetViewports(D3D10Device device, UINT num_viewports, D3D10_VIEWPORT* viewports)
{
   device->lpVtbl->RSSetViewports(device, num_viewports, viewports);
}
static INLINE void D3D10SetScissorRects(D3D10Device device, UINT num_rects, D3D10_RECT* rects)
{
   device->lpVtbl->RSSetScissorRects(device, num_rects, rects);
}
static INLINE void D3D10CopySubresourceRegionDevice(
      D3D10Device   device,
      D3D10Resource dst_resource,
      UINT          dst_subresource,
      UINT          dst_x,
      UINT          dst_y,
      UINT          dst_z,
      D3D10Resource src_resource,
      UINT          src_subresource,
      D3D10_BOX*    src_box)
{
   device->lpVtbl->CopySubresourceRegion(
         device, dst_resource, dst_subresource, dst_x, dst_y, dst_z, src_resource, src_subresource,
         src_box);
}
static INLINE void
D3D10CopyResource(D3D10Device device, D3D10Resource dst_resource, D3D10Resource src_resource)
{
   device->lpVtbl->CopyResource(device, dst_resource, src_resource);
}
static INLINE void D3D10UpdateSubresource(
      D3D10Device   device,
      D3D10Resource dst_resource,
      UINT          dst_subresource,
      D3D10_BOX*    dst_box,
      void*         src_data,
      UINT          src_row_pitch,
      UINT          src_depth_pitch)
{
   device->lpVtbl->UpdateSubresource(
         device, dst_resource, dst_subresource, dst_box, src_data, src_row_pitch, src_depth_pitch);
}
static INLINE void D3D10ClearRenderTargetView(
      D3D10Device device, D3D10RenderTargetView render_target_view, FLOAT color_rgba[4])
{
   device->lpVtbl->ClearRenderTargetView(device, render_target_view, color_rgba);
}
static INLINE void D3D10ClearDepthStencilView(
      D3D10Device           device,
      D3D10DepthStencilView depth_stencil_view,
      UINT                  clear_flags,
      FLOAT                 depth,
      UINT8                 stencil)
{
   device->lpVtbl->ClearDepthStencilView(device, depth_stencil_view, clear_flags, depth, stencil);
}
static INLINE void
D3D10GenerateMips(D3D10Device device, D3D10ShaderResourceView shader_resource_view)
{
   device->lpVtbl->GenerateMips(device, shader_resource_view);
}
static INLINE void D3D10ResolveSubresource(
      D3D10Device   device,
      D3D10Resource dst_resource,
      UINT          dst_subresource,
      D3D10Resource src_resource,
      UINT          src_subresource,
      DXGI_FORMAT   format)
{
   device->lpVtbl->ResolveSubresource(
         device, dst_resource, dst_subresource, src_resource, src_subresource, format);
}
static INLINE void D3D10GetVShaderConstantBuffers(
      D3D10Device device, UINT start_slot, UINT num_buffers, D3D10Buffer* constant_buffers)
{
   device->lpVtbl->VSGetConstantBuffers(device, start_slot, num_buffers, constant_buffers);
}
static INLINE void D3D10GetPShaderResources(
      D3D10Device              device,
      UINT                     start_slot,
      UINT                     num_views,
      D3D10ShaderResourceView* shader_resource_views)
{
   device->lpVtbl->PSGetShaderResources(device, start_slot, num_views, shader_resource_views);
}
static INLINE void D3D10GetPShader(D3D10Device device, D3D10PixelShader* pixel_shader)
{
   device->lpVtbl->PSGetShader(device, pixel_shader);
}
static INLINE void D3D10GetPShaderSamplers(
      D3D10Device device, UINT start_slot, UINT num_samplers, D3D10SamplerState* samplers)
{
   device->lpVtbl->PSGetSamplers(device, start_slot, num_samplers, samplers);
}
static INLINE void D3D10GetVShader(D3D10Device device, D3D10VertexShader* vertex_shader)
{
   device->lpVtbl->VSGetShader(device, vertex_shader);
}
static INLINE void D3D10GetPShaderConstantBuffers(
      D3D10Device device, UINT start_slot, UINT num_buffers, D3D10Buffer* constant_buffers)
{
   device->lpVtbl->PSGetConstantBuffers(device, start_slot, num_buffers, constant_buffers);
}
static INLINE void D3D10GetInputLayout(D3D10Device device, D3D10InputLayout* input_layout)
{
   device->lpVtbl->IAGetInputLayout(device, input_layout);
}
static INLINE void D3D10GetVertexBuffers(
      D3D10Device  device,
      UINT         start_slot,
      UINT         num_buffers,
      D3D10Buffer* vertex_buffers,
      UINT*        strides,
      UINT*        offsets)
{
   device->lpVtbl->IAGetVertexBuffers(
         device, start_slot, num_buffers, vertex_buffers, strides, offsets);
}
static INLINE void D3D10GetIndexBuffer(
      D3D10Device device, D3D10Buffer* index_buffer, DXGI_FORMAT* format, UINT* offset)
{
   device->lpVtbl->IAGetIndexBuffer(device, index_buffer, format, offset);
}
static INLINE void D3D10GetGShaderConstantBuffers(
      D3D10Device device, UINT start_slot, UINT num_buffers, D3D10Buffer* constant_buffers)
{
   device->lpVtbl->GSGetConstantBuffers(device, start_slot, num_buffers, constant_buffers);
}
static INLINE void D3D10GetGShader(D3D10Device device, D3D10GeometryShader* geometry_shader)
{
   device->lpVtbl->GSGetShader(device, geometry_shader);
}
static INLINE void D3D10GetPrimitiveTopology(D3D10Device device, D3D10_PRIMITIVE_TOPOLOGY* topology)
{
   device->lpVtbl->IAGetPrimitiveTopology(device, topology);
}
static INLINE void D3D10GetVShaderResources(
      D3D10Device              device,
      UINT                     start_slot,
      UINT                     num_views,
      D3D10ShaderResourceView* shader_resource_views)
{
   device->lpVtbl->VSGetShaderResources(device, start_slot, num_views, shader_resource_views);
}
static INLINE void D3D10GetVShaderSamplers(
      D3D10Device device, UINT start_slot, UINT num_samplers, D3D10SamplerState* samplers)
{
   device->lpVtbl->VSGetSamplers(device, start_slot, num_samplers, samplers);
}
static INLINE void
D3D10GetPredication(D3D10Device device, D3D10Predicate* predicate, BOOL* predicate_value)
{
   device->lpVtbl->GetPredication(device, predicate, predicate_value);
}
static INLINE void D3D10GetGShaderResources(
      D3D10Device              device,
      UINT                     start_slot,
      UINT                     num_views,
      D3D10ShaderResourceView* shader_resource_views)
{
   device->lpVtbl->GSGetShaderResources(device, start_slot, num_views, shader_resource_views);
}
static INLINE void D3D10GetGShaderSamplers(
      D3D10Device device, UINT start_slot, UINT num_samplers, D3D10SamplerState* samplers)
{
   device->lpVtbl->GSGetSamplers(device, start_slot, num_samplers, samplers);
}
static INLINE void D3D10GetRenderTargets(
      D3D10Device            device,
      UINT                   num_views,
      D3D10RenderTargetView* render_target_views,
      D3D10DepthStencilView* depth_stencil_view)
{
   device->lpVtbl->OMGetRenderTargets(device, num_views, render_target_views, depth_stencil_view);
}
static INLINE void D3D10GetBlendState(
      D3D10Device device, D3D10BlendState* blend_state, FLOAT blend_factor[4], UINT* sample_mask)
{
   device->lpVtbl->OMGetBlendState(device, blend_state, blend_factor, sample_mask);
}
static INLINE void D3D10GetDepthStencilState(
      D3D10Device device, D3D10DepthStencilState* depth_stencil_state, UINT* stencil_ref)
{
   device->lpVtbl->OMGetDepthStencilState(device, depth_stencil_state, stencil_ref);
}
static INLINE void
D3D10SOGetTargets(D3D10Device device, UINT num_buffers, D3D10Buffer* sotargets, UINT* offsets)
{
   device->lpVtbl->SOGetTargets(device, num_buffers, sotargets, offsets);
}
static INLINE void D3D10GetState(D3D10Device device, D3D10RasterizerState* rasterizer_state)
{
   device->lpVtbl->RSGetState(device, rasterizer_state);
}
static INLINE void
D3D10GetViewports(D3D10Device device, UINT* num_viewports, D3D10_VIEWPORT* viewports)
{
   device->lpVtbl->RSGetViewports(device, num_viewports, viewports);
}
static INLINE void D3D10GetScissorRects(D3D10Device device, UINT* num_rects, D3D10_RECT* rects)
{
   device->lpVtbl->RSGetScissorRects(device, num_rects, rects);
}
static INLINE HRESULT D3D10GetDeviceRemovedReason(D3D10Device device)
{
   return device->lpVtbl->GetDeviceRemovedReason(device);
}
static INLINE HRESULT D3D10SetExceptionMode(D3D10Device device, UINT raise_flags)
{
   return device->lpVtbl->SetExceptionMode(device, raise_flags);
}
static INLINE UINT D3D10GetExceptionMode(D3D10Device device)
{
   return device->lpVtbl->GetExceptionMode(device);
}
static INLINE void    D3D10ClearState(D3D10Device device) { device->lpVtbl->ClearState(device); }
static INLINE void    D3D10Flush(D3D10Device device) { device->lpVtbl->Flush(device); }
static INLINE HRESULT D3D10CreateBuffer(
      D3D10Device             device,
      D3D10_BUFFER_DESC*      desc,
      D3D10_SUBRESOURCE_DATA* initial_data,
      D3D10Buffer*            buffer)
{
   return device->lpVtbl->CreateBuffer(device, desc, initial_data, buffer);
}
static INLINE HRESULT D3D10CreateTexture1D(
      D3D10Device             device,
      D3D10_TEXTURE1D_DESC*   desc,
      D3D10_SUBRESOURCE_DATA* initial_data,
      D3D10Texture1D*         texture1d)
{
   return device->lpVtbl->CreateTexture1D(device, desc, initial_data, texture1d);
}
static INLINE HRESULT D3D10CreateTexture2D(
      D3D10Device             device,
      D3D10_TEXTURE2D_DESC*   desc,
      D3D10_SUBRESOURCE_DATA* initial_data,
      D3D10Texture2D*         texture2d)
{
   return device->lpVtbl->CreateTexture2D(device, desc, initial_data, texture2d);
}
static INLINE HRESULT D3D10CreateTexture3D(
      D3D10Device             device,
      D3D10_TEXTURE3D_DESC*   desc,
      D3D10_SUBRESOURCE_DATA* initial_data,
      D3D10Texture3D*         texture3d)
{
   return device->lpVtbl->CreateTexture3D(device, desc, initial_data, texture3d);
}
static INLINE HRESULT D3D10CreateShaderResourceViewDevice(
      D3D10Device                      device,
      D3D10Resource                    resource,
      D3D10_SHADER_RESOURCE_VIEW_DESC* desc,
      D3D10ShaderResourceView*         srview)
{
   return device->lpVtbl->CreateShaderResourceView(device, resource, desc, srview);
}
static INLINE HRESULT D3D10CreateRenderTargetViewDevice(
      D3D10Device                    device,
      D3D10Resource                  resource,
      D3D10_RENDER_TARGET_VIEW_DESC* desc,
      D3D10RenderTargetView*         rtview)
{
   return device->lpVtbl->CreateRenderTargetView(device, resource, desc, rtview);
}
static INLINE HRESULT D3D10CreateDepthStencilView(
      D3D10Device                    device,
      D3D10Resource                  resource,
      D3D10_DEPTH_STENCIL_VIEW_DESC* desc,
      D3D10DepthStencilView*         depth_stencil_view)
{
   return device->lpVtbl->CreateDepthStencilView(device, resource, desc, depth_stencil_view);
}
static INLINE HRESULT D3D10CreateInputLayout(
      D3D10Device               device,
      D3D10_INPUT_ELEMENT_DESC* input_element_descs,
      UINT                      num_elements,
      void*                     shader_bytecode_with_input_signature,
      SIZE_T                    bytecode_length,
      D3D10InputLayout*         input_layout)
{
   return device->lpVtbl->CreateInputLayout(
         device, input_element_descs, num_elements, shader_bytecode_with_input_signature,
         bytecode_length, input_layout);
}
static INLINE HRESULT D3D10CreateVertexShader(
      D3D10Device        device,
      void*              shader_bytecode,
      SIZE_T             bytecode_length,
      D3D10VertexShader* vertex_shader)
{
   return device->lpVtbl->CreateVertexShader(
         device, shader_bytecode, bytecode_length, vertex_shader);
}
static INLINE HRESULT D3D10CreateGeometryShader(
      D3D10Device          device,
      void*                shader_bytecode,
      SIZE_T               bytecode_length,
      D3D10GeometryShader* geometry_shader)
{
   return device->lpVtbl->CreateGeometryShader(
         device, shader_bytecode, bytecode_length, geometry_shader);
}
static INLINE HRESULT D3D10CreateGeometryShaderWithStreamOutput(
      D3D10Device                 device,
      void*                       shader_bytecode,
      SIZE_T                      bytecode_length,
      D3D10_SO_DECLARATION_ENTRY* sodeclaration,
      UINT                        num_entries,
      UINT                        output_stream_stride,
      D3D10GeometryShader*        geometry_shader)
{
   return device->lpVtbl->CreateGeometryShaderWithStreamOutput(
         device, shader_bytecode, bytecode_length, sodeclaration, num_entries, output_stream_stride,
         geometry_shader);
}
static INLINE HRESULT D3D10CreatePixelShader(
      D3D10Device       device,
      void*             shader_bytecode,
      SIZE_T            bytecode_length,
      D3D10PixelShader* pixel_shader)
{
   return device->lpVtbl->CreatePixelShader(device, shader_bytecode, bytecode_length, pixel_shader);
}
static INLINE HRESULT D3D10CreateBlendState(
      D3D10Device device, D3D10_BLEND_DESC* blend_state_desc, D3D10BlendState* blend_state)
{
   return device->lpVtbl->CreateBlendState(device, blend_state_desc, blend_state);
}
static INLINE HRESULT D3D10CreateDepthStencilState(
      D3D10Device               device,
      D3D10_DEPTH_STENCIL_DESC* depth_stencil_desc,
      D3D10DepthStencilState*   depth_stencil_state)
{
   return device->lpVtbl->CreateDepthStencilState(device, depth_stencil_desc, depth_stencil_state);
}
static INLINE HRESULT D3D10CreateRasterizerState(
      D3D10Device            device,
      D3D10_RASTERIZER_DESC* rasterizer_desc,
      D3D10RasterizerState*  rasterizer_state)
{
   return device->lpVtbl->CreateRasterizerState(device, rasterizer_desc, rasterizer_state);
}
static INLINE HRESULT D3D10CreateSamplerState(
      D3D10Device device, D3D10_SAMPLER_DESC* sampler_desc, D3D10SamplerState* sampler_state)
{
   return device->lpVtbl->CreateSamplerState(device, sampler_desc, sampler_state);
}
static INLINE HRESULT
D3D10CreateQuery(D3D10Device device, D3D10_QUERY_DESC* query_desc, D3D10Query* query)
{
   return device->lpVtbl->CreateQuery(device, query_desc, query);
}
static INLINE HRESULT D3D10CreatePredicate(
      D3D10Device device, D3D10_QUERY_DESC* predicate_desc, D3D10Predicate* predicate)
{
   return device->lpVtbl->CreatePredicate(device, predicate_desc, predicate);
}
static INLINE HRESULT
D3D10CreateCounter(D3D10Device device, D3D10_COUNTER_DESC* counter_desc, D3D10Counter* counter)
{
   return device->lpVtbl->CreateCounter(device, counter_desc, counter);
}
static INLINE HRESULT
D3D10CheckFormatSupport(D3D10Device device, DXGI_FORMAT format, UINT* format_support)
{
   return device->lpVtbl->CheckFormatSupport(device, format, format_support);
}
static INLINE HRESULT D3D10CheckMultisampleQualityLevels(
      D3D10Device device, DXGI_FORMAT format, UINT sample_count, UINT* num_quality_levels)
{
   return device->lpVtbl->CheckMultisampleQualityLevels(
         device, format, sample_count, num_quality_levels);
}
static INLINE void D3D10CheckCounterInfo(D3D10Device device, D3D10_COUNTER_INFO* counter_info)
{
   device->lpVtbl->CheckCounterInfo(device, counter_info);
}
static INLINE HRESULT D3D10CheckCounter(
      D3D10Device         device,
      D3D10_COUNTER_DESC* desc,
      D3D10_COUNTER_TYPE* type,
      UINT*               active_counters,
      LPSTR               sz_name,
      UINT*               name_length,
      LPSTR               sz_units,
      UINT*               units_length,
      LPSTR               sz_description,
      UINT*               description_length)
{
   return device->lpVtbl->CheckCounter(
         device, desc, type, active_counters, sz_name, name_length, sz_units, units_length,
         sz_description, description_length);
}
static INLINE UINT D3D10GetCreationFlags(D3D10Device device)
{
   return device->lpVtbl->GetCreationFlags(device);
}
static INLINE HRESULT
D3D10OpenSharedResource(D3D10Device device, HANDLE h_resource, ID3D10Resource** out)
{
   return device->lpVtbl->OpenSharedResource(
         device, h_resource, uuidof(ID3D10Resource), (void**)out);
}
static INLINE void D3D10SetTextFilterSize(D3D10Device device, UINT width, UINT height)
{
   device->lpVtbl->SetTextFilterSize(device, width, height);
}
static INLINE void D3D10GetTextFilterSize(D3D10Device device, UINT* width, UINT* height)
{
   device->lpVtbl->GetTextFilterSize(device, width, height);
}
static INLINE void D3D10Enter(D3D10Multithread multithread)
{
   multithread->lpVtbl->Enter(multithread);
}
static INLINE void D3D10Leave(D3D10Multithread multithread)
{
   multithread->lpVtbl->Leave(multithread);
}
static INLINE BOOL D3D10SetMultithreadProtected(D3D10Multithread multithread, BOOL mtprotect)
{
   return multithread->lpVtbl->SetMultithreadProtected(multithread, mtprotect);
}
static INLINE BOOL D3D10GetMultithreadProtected(D3D10Multithread multithread)
{
   return multithread->lpVtbl->GetMultithreadProtected(multithread);
}
static INLINE HRESULT D3D10SetDebugFeatureMask(D3D10Debug debug, UINT mask)
{
   return debug->lpVtbl->SetFeatureMask(debug, mask);
}
static INLINE UINT D3D10GetDebugFeatureMask(D3D10Debug debug)
{
   return debug->lpVtbl->GetFeatureMask(debug);
}
static INLINE HRESULT D3D10SetPresentPerRenderOpDelay(D3D10Debug debug, UINT milliseconds)
{
   return debug->lpVtbl->SetPresentPerRenderOpDelay(debug, milliseconds);
}
static INLINE UINT D3D10GetPresentPerRenderOpDelay(D3D10Debug debug)
{
   return debug->lpVtbl->GetPresentPerRenderOpDelay(debug);
}
static INLINE HRESULT D3D10SetSwapChain(D3D10Debug debug, IDXGISwapChain* swap_chain)
{
   return debug->lpVtbl->SetSwapChain(debug, (IDXGISwapChain*)swap_chain);
}
static INLINE HRESULT D3D10GetSwapChain(D3D10Debug debug, IDXGISwapChain** swap_chain)
{
   return debug->lpVtbl->GetSwapChain(debug, (IDXGISwapChain**)swap_chain);
}
static INLINE HRESULT D3D10Validate(D3D10Debug debug) { return debug->lpVtbl->Validate(debug); }
static INLINE BOOL    D3D10SetUseRef(D3D10SwitchToRef switch_to_ref, BOOL use_ref)
{
   return switch_to_ref->lpVtbl->SetUseRef(switch_to_ref, use_ref);
}
static INLINE BOOL D3D10GetUseRef(D3D10SwitchToRef switch_to_ref)
{
   return switch_to_ref->lpVtbl->GetUseRef(switch_to_ref);
}
static INLINE HRESULT
D3D10SetMessageCountLimit(D3D10InfoQueue info_queue, UINT64 message_count_limit)
{
   return info_queue->lpVtbl->SetMessageCountLimit(info_queue, message_count_limit);
}
static INLINE void D3D10ClearStoredMessages(D3D10InfoQueue info_queue)
{
   info_queue->lpVtbl->ClearStoredMessages(info_queue);
}
static INLINE HRESULT D3D10GetMessageA(
      D3D10InfoQueue info_queue,
      UINT64         message_index,
      D3D10_MESSAGE* message,
      SIZE_T*        message_byte_length)
{
   return info_queue->lpVtbl->GetMessageA(info_queue, message_index, message, message_byte_length);
}
static INLINE UINT64 D3D10GetNumMessagesAllowedByStorageFilter(D3D10InfoQueue info_queue)
{
   return info_queue->lpVtbl->GetNumMessagesAllowedByStorageFilter(info_queue);
}
static INLINE UINT64 D3D10GetNumMessagesDeniedByStorageFilter(D3D10InfoQueue info_queue)
{
   return info_queue->lpVtbl->GetNumMessagesDeniedByStorageFilter(info_queue);
}
static INLINE UINT64 D3D10GetNumStoredMessages(D3D10InfoQueue info_queue)
{
   return info_queue->lpVtbl->GetNumStoredMessages(info_queue);
}
static INLINE UINT64 D3D10GetNumStoredMessagesAllowedByRetrievalFilter(D3D10InfoQueue info_queue)
{
   return info_queue->lpVtbl->GetNumStoredMessagesAllowedByRetrievalFilter(info_queue);
}
static INLINE UINT64 D3D10GetNumMessagesDiscardedByMessageCountLimit(D3D10InfoQueue info_queue)
{
   return info_queue->lpVtbl->GetNumMessagesDiscardedByMessageCountLimit(info_queue);
}
static INLINE UINT64 D3D10GetMessageCountLimit(D3D10InfoQueue info_queue)
{
   return info_queue->lpVtbl->GetMessageCountLimit(info_queue);
}
static INLINE HRESULT
D3D10AddStorageFilterEntries(D3D10InfoQueue info_queue, D3D10_INFO_QUEUE_FILTER* filter)
{
   return info_queue->lpVtbl->AddStorageFilterEntries(info_queue, filter);
}
static INLINE HRESULT D3D10GetStorageFilter(
      D3D10InfoQueue info_queue, D3D10_INFO_QUEUE_FILTER* filter, SIZE_T* filter_byte_length)
{
   return info_queue->lpVtbl->GetStorageFilter(info_queue, filter, filter_byte_length);
}
static INLINE void D3D10ClearStorageFilter(D3D10InfoQueue info_queue)
{
   info_queue->lpVtbl->ClearStorageFilter(info_queue);
}
static INLINE HRESULT D3D10PushEmptyStorageFilter(D3D10InfoQueue info_queue)
{
   return info_queue->lpVtbl->PushEmptyStorageFilter(info_queue);
}
static INLINE HRESULT D3D10PushCopyOfStorageFilter(D3D10InfoQueue info_queue)
{
   return info_queue->lpVtbl->PushCopyOfStorageFilter(info_queue);
}
static INLINE HRESULT
D3D10PushStorageFilter(D3D10InfoQueue info_queue, D3D10_INFO_QUEUE_FILTER* filter)
{
   return info_queue->lpVtbl->PushStorageFilter(info_queue, filter);
}
static INLINE void D3D10PopStorageFilter(D3D10InfoQueue info_queue)
{
   info_queue->lpVtbl->PopStorageFilter(info_queue);
}
static INLINE UINT D3D10GetStorageFilterStackSize(D3D10InfoQueue info_queue)
{
   return info_queue->lpVtbl->GetStorageFilterStackSize(info_queue);
}
static INLINE HRESULT
D3D10AddRetrievalFilterEntries(D3D10InfoQueue info_queue, D3D10_INFO_QUEUE_FILTER* filter)
{
   return info_queue->lpVtbl->AddRetrievalFilterEntries(info_queue, filter);
}
static INLINE HRESULT D3D10GetRetrievalFilter(
      D3D10InfoQueue info_queue, D3D10_INFO_QUEUE_FILTER* filter, SIZE_T* filter_byte_length)
{
   return info_queue->lpVtbl->GetRetrievalFilter(info_queue, filter, filter_byte_length);
}
static INLINE void D3D10ClearRetrievalFilter(D3D10InfoQueue info_queue)
{
   info_queue->lpVtbl->ClearRetrievalFilter(info_queue);
}
static INLINE HRESULT D3D10PushEmptyRetrievalFilter(D3D10InfoQueue info_queue)
{
   return info_queue->lpVtbl->PushEmptyRetrievalFilter(info_queue);
}
static INLINE HRESULT D3D10PushCopyOfRetrievalFilter(D3D10InfoQueue info_queue)
{
   return info_queue->lpVtbl->PushCopyOfRetrievalFilter(info_queue);
}
static INLINE HRESULT
D3D10PushRetrievalFilter(D3D10InfoQueue info_queue, D3D10_INFO_QUEUE_FILTER* filter)
{
   return info_queue->lpVtbl->PushRetrievalFilter(info_queue, filter);
}
static INLINE void D3D10PopRetrievalFilter(D3D10InfoQueue info_queue)
{
   info_queue->lpVtbl->PopRetrievalFilter(info_queue);
}
static INLINE UINT D3D10GetRetrievalFilterStackSize(D3D10InfoQueue info_queue)
{
   return info_queue->lpVtbl->GetRetrievalFilterStackSize(info_queue);
}
static INLINE HRESULT D3D10AddMessage(
      D3D10InfoQueue         info_queue,
      D3D10_MESSAGE_CATEGORY category,
      D3D10_MESSAGE_SEVERITY severity,
      D3D10_MESSAGE_ID       id,
      LPCSTR                 description)
{
   return info_queue->lpVtbl->AddMessage(info_queue, category, severity, id, description);
}
static INLINE HRESULT D3D10AddApplicationMessage(
      D3D10InfoQueue info_queue, D3D10_MESSAGE_SEVERITY severity, LPCSTR description)
{
   return info_queue->lpVtbl->AddApplicationMessage(info_queue, severity, description);
}
static INLINE HRESULT
D3D10SetBreakOnCategory(D3D10InfoQueue info_queue, D3D10_MESSAGE_CATEGORY category, BOOL enable)
{
   return info_queue->lpVtbl->SetBreakOnCategory(info_queue, category, enable);
}
static INLINE HRESULT
D3D10SetBreakOnSeverity(D3D10InfoQueue info_queue, D3D10_MESSAGE_SEVERITY severity, BOOL enable)
{
   return info_queue->lpVtbl->SetBreakOnSeverity(info_queue, severity, enable);
}
static INLINE HRESULT D3D10SetBreakOnID(D3D10InfoQueue info_queue, D3D10_MESSAGE_ID id, BOOL enable)
{
   return info_queue->lpVtbl->SetBreakOnID(info_queue, id, enable);
}
static INLINE BOOL
D3D10GetBreakOnCategory(D3D10InfoQueue info_queue, D3D10_MESSAGE_CATEGORY category)
{
   return info_queue->lpVtbl->GetBreakOnCategory(info_queue, category);
}
static INLINE BOOL
D3D10GetBreakOnSeverity(D3D10InfoQueue info_queue, D3D10_MESSAGE_SEVERITY severity)
{
   return info_queue->lpVtbl->GetBreakOnSeverity(info_queue, severity);
}
static INLINE BOOL D3D10GetBreakOnID(D3D10InfoQueue info_queue, D3D10_MESSAGE_ID id)
{
   return info_queue->lpVtbl->GetBreakOnID(info_queue, id);
}
static INLINE void D3D10SetMuteDebugOutput(D3D10InfoQueue info_queue, BOOL mute)
{
   info_queue->lpVtbl->SetMuteDebugOutput(info_queue, mute);
}
static INLINE BOOL D3D10GetMuteDebugOutput(D3D10InfoQueue info_queue)
{
   return info_queue->lpVtbl->GetMuteDebugOutput(info_queue);
}

/* end of auto-generated */

static INLINE HRESULT
DXGIGetSwapChainBufferD3D10(DXGISwapChain swap_chain, UINT buffer, D3D10Texture2D* out)
{
   return swap_chain->lpVtbl->GetBuffer(swap_chain, buffer, uuidof(ID3D10Texture2D), (void**)out);
}
static INLINE void D3D10CopyTexture2DSubresourceRegion(
      D3D10Device    device,
      D3D10Texture2D dst_texture,
      UINT           dst_subresource,
      UINT           dst_x,
      UINT           dst_y,
      UINT           dst_z,
      D3D10Texture2D src_texture,
      UINT           src_subresource,
      D3D10_BOX*     src_box)
{
   device->lpVtbl->CopySubresourceRegion(
         device, (D3D10Resource)dst_texture, dst_subresource, dst_x, dst_y, dst_z,
         (D3D10Resource)src_texture, src_subresource, src_box);
}
static INLINE HRESULT D3D10CreateTexture2DRenderTargetView(
      D3D10Device                    device,
      D3D10Texture2D                 texture,
      D3D10_RENDER_TARGET_VIEW_DESC* desc,
      D3D10RenderTargetView*         rtview)
{
   return device->lpVtbl->CreateRenderTargetView(device, (D3D10Resource)texture, desc, rtview);
}
static INLINE HRESULT D3D10CreateTexture2DShaderResourceView(
      D3D10Device                      device,
      D3D10Texture2D                   texture,
      D3D10_SHADER_RESOURCE_VIEW_DESC* desc,
      D3D10ShaderResourceView*         srview)
{
   return device->lpVtbl->CreateShaderResourceView(device, (D3D10Resource)texture, desc, srview);
}
#endif
   /* internal */

#include <boolean.h>

#include <retro_math.h>
#include <gfx/math/matrix_4x4.h>

#include "../../retroarch.h"

typedef struct d3d10_vertex_t
{
   float position[2];
   float texcoord[2];
   float color[4];
} d3d10_vertex_t;

typedef struct
{
   D3D10Texture2D          handle;
   D3D10Texture2D          staging;
   D3D10_TEXTURE2D_DESC    desc;
   D3D10RenderTargetView   rt_view;
   D3D10ShaderResourceView view;
   D3D10SamplerStateRef    sampler;
   float4_t                size_data;
} d3d10_texture_t;

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
} d3d10_sprite_t;

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
} d3d10_uniform_t;

static_assert(
      (!(sizeof(d3d10_uniform_t) & 0xF)), "sizeof(d3d10_uniform_t) must be a multiple of 16");

typedef struct d3d10_shader_t
{
   D3D10VertexShader   vs;
   D3D10PixelShader    ps;
   D3D10GeometryShader gs;
   D3D10InputLayout    layout;
} d3d10_shader_t;

typedef struct
{
   unsigned              cur_mon_id;
   DXGISwapChain         swapChain;
   D3D10Device           device;
   D3D10RasterizerState  state;
   D3D10RenderTargetView renderTargetView;
   D3D10Buffer           ubo;
   d3d10_uniform_t       ubo_values;
   D3D10SamplerState     samplers[RARCH_FILTER_MAX][RARCH_WRAP_MAX];
   D3D10BlendState       blend_enable;
   D3D10BlendState       blend_disable;
   D3D10BlendState       blend_pipeline;
   D3D10Buffer           menu_pipeline_vbo;
   math_matrix_4x4       mvp, mvp_no_rot;
   struct video_viewport vp;
   D3D10_VIEWPORT        viewport;
   DXGI_FORMAT           format;
   float                 clearcolor[4];
   bool                  vsync;
   bool                  resize_chain;
   bool                  keep_aspect;
   bool                  resize_viewport;
   bool                  resize_render_targets;
   bool                  init_history;
   d3d10_shader_t        shaders[GFX_MAX_SHADERS];
#ifdef __WINRT__
   DXGIFactory2 factory;
#else
   DXGIFactory factory;
#endif
   DXGIAdapter adapter;

	struct
   {
      d3d10_shader_t shader;
      d3d10_shader_t shader_font;
      D3D10Buffer    vbo;
      int            offset;
      int            capacity;
      bool           enabled;
   } sprites;

#ifdef HAVE_OVERLAY
   struct
   {
      D3D10Buffer      vbo;
      d3d10_texture_t* textures;
      bool             enabled;
      bool             fullscreen;
      int              count;
   } overlays;
#endif

   struct
   {
      d3d10_texture_t   texture;
      D3D10Buffer       vbo;
      bool              enabled;
      bool              fullscreen;
   } menu;
   struct
   {
      d3d10_texture_t texture[GFX_MAX_FRAME_HISTORY + 1];
      D3D10Buffer       vbo;
      D3D10Buffer       ubo;
      D3D10_VIEWPORT    viewport;
      float4_t          output_size;
      int               rotation;
   } frame;

   struct
   {
      d3d10_shader_t             shader;
      D3D10Buffer                buffers[SLANG_CBUFFER_MAX];
      d3d10_texture_t            rt;
      d3d10_texture_t            feedback;
      D3D10_VIEWPORT             viewport;
      pass_semantics_t           semantics;
      uint32_t                   frame_count;
      int32_t                    frame_direction;
   } pass[GFX_MAX_SHADERS];

   struct video_shader* shader_preset;
   d3d10_texture_t      luts[GFX_MAX_TEXTURES];
} d3d10_video_t;

void d3d10_init_texture(D3D10Device device, d3d10_texture_t* texture);
static INLINE void d3d10_release_texture(d3d10_texture_t* texture)
{
   Release(texture->handle);
   Release(texture->staging);
   Release(texture->view);
   Release(texture->rt_view);
}

void d3d10_update_texture(
      D3D10Device      ctx,
      int              width,
      int              height,
      int              pitch,
      DXGI_FORMAT      format,
      const void*      data,
      d3d10_texture_t* texture);

DXGI_FORMAT d3d10_get_closest_match(
      D3D10Device device, DXGI_FORMAT desired_format, UINT desired_format_support);

bool d3d10_init_shader(
      D3D10Device                     device,
      const char*                     src,
      size_t                          size,
      const void*                     src_name,
      LPCSTR                          vs_entry,
      LPCSTR                          ps_entry,
      LPCSTR                          gs_entry,
      const D3D10_INPUT_ELEMENT_DESC* input_element_descs,
      UINT                            num_elements,
      d3d10_shader_t*                 out);

static INLINE void d3d10_release_shader(d3d10_shader_t* shader)
{
   Release(shader->layout);
   Release(shader->vs);
   Release(shader->ps);
   Release(shader->gs);
}

static INLINE DXGI_FORMAT
d3d10_get_closest_match_texture2D(D3D10Device device, DXGI_FORMAT desired_format)
{
   return d3d10_get_closest_match(
         device, desired_format,
         D3D10_FORMAT_SUPPORT_TEXTURE2D | D3D10_FORMAT_SUPPORT_SHADER_SAMPLE);
}

static INLINE void d3d10_set_shader(D3D10Device ctx, d3d10_shader_t* shader)
{
   D3D10SetInputLayout(ctx, shader->layout);
   D3D10SetVShader(ctx, shader->vs);
   D3D10SetPShader(ctx, shader->ps);
   D3D10SetGShader(ctx, shader->gs);
}

#if !defined(__cplusplus) || defined(CINTERFACE)
static INLINE void
d3d10_set_texture_and_sampler(D3D10Device ctx, UINT slot, d3d10_texture_t* texture)
{
   D3D10SetPShaderResources(ctx, slot, 1, &texture->view);
   D3D10SetPShaderSamplers(ctx, slot, 1, (D3D10SamplerState*)&texture->sampler);
}
#endif
