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

static INLINE void D3D10SetResourceEvictionPriority(D3D10Resource resource, UINT eviction_priority)
{
#ifdef __cplusplus
   resource->SetEvictionPriority(eviction_priority);
#else
   resource->lpVtbl->SetEvictionPriority(resource, eviction_priority);
#endif
}
static INLINE UINT D3D10GetResourceEvictionPriority(D3D10Resource resource)
{
#ifdef __cplusplus
   return resource->GetEvictionPriority();
#else
   return resource->lpVtbl->GetEvictionPriority(resource);
#endif
}
static INLINE void D3D10SetBufferEvictionPriority(D3D10Buffer buffer, UINT eviction_priority)
{
#ifdef __cplusplus
   buffer->SetEvictionPriority(eviction_priority);
#else
   buffer->lpVtbl->SetEvictionPriority(buffer, eviction_priority);
#endif
}
static INLINE UINT D3D10GetBufferEvictionPriority(D3D10Buffer buffer)
{
#ifdef __cplusplus
   return buffer->GetEvictionPriority();
#else
   return buffer->lpVtbl->GetEvictionPriority(buffer);
#endif
}

static INLINE HRESULT
D3D10MapBuffer(D3D10Buffer buffer, D3D10_MAP map_type, UINT map_flags, void** data)
{
#ifdef __cplusplus
   return buffer->Map(map_type, map_flags, data);
#else
   return buffer->lpVtbl->Map(buffer, map_type, map_flags, data);
#endif
}
static INLINE void D3D10UnmapBuffer(D3D10Buffer buffer)
{
#ifdef __cplusplus
   buffer->Unmap();
#else
   buffer->lpVtbl->Unmap(buffer);
#endif
}
static INLINE void
D3D10SetTexture1DEvictionPriority(D3D10Texture1D texture1d,
      UINT eviction_priority)
{
#ifdef __cplusplus
   texture1d->SetEvictionPriority(eviction_priority);
#else
   texture1d->lpVtbl->SetEvictionPriority(texture1d, eviction_priority);
#endif
}
static INLINE UINT D3D10GetTexture1DEvictionPriority(D3D10Texture1D texture1d)
{
#ifdef __cplusplus
   return texture1d->GetEvictionPriority();
#else
   return texture1d->lpVtbl->GetEvictionPriority(texture1d);
#endif
}
static INLINE HRESULT D3D10MapTexture1D(
      D3D10Texture1D texture1d, UINT subresource, D3D10_MAP map_type, UINT map_flags, void** data)
{
#ifdef __cplusplus
   return texture1d->Map(subresource, map_type, map_flags, data);
#else
   return texture1d->lpVtbl->Map(texture1d, subresource,
         map_type, map_flags, data);
#endif
}
static INLINE void D3D10UnmapTexture1D(D3D10Texture1D texture1d, UINT subresource)
{
#ifdef __cplusplus
   texture1d->Unmap(subresource);
#else
   texture1d->lpVtbl->Unmap(texture1d, subresource);
#endif
}
static INLINE void
D3D10SetTexture2DEvictionPriority(D3D10Texture2D texture2d, UINT eviction_priority)
{
#ifdef __cplusplus
   texture2d->SetEvictionPriority(eviction_priority);
#else
   texture2d->lpVtbl->SetEvictionPriority(texture2d, eviction_priority);
#endif
}

static INLINE UINT D3D10GetTexture2DEvictionPriority(D3D10Texture2D texture2d)
{
#ifdef __cplusplus
   return texture2d->GetEvictionPriority();
#else
   return texture2d->lpVtbl->GetEvictionPriority(texture2d);
#endif
}

static INLINE HRESULT D3D10MapTexture2D(
      D3D10Texture2D          texture2d,
      UINT                    subresource,
      D3D10_MAP               map_type,
      UINT                    map_flags,
      D3D10_MAPPED_TEXTURE2D* mapped_tex2d)
{
#ifdef __cplusplus
   return texture2d->Map(subresource, map_type, map_flags, mapped_tex2d);
#else
   return texture2d->lpVtbl->Map(texture2d, subresource, map_type,
         map_flags, mapped_tex2d);
#endif
}

static INLINE void D3D10UnmapTexture2D(D3D10Texture2D texture2d, UINT subresource)
{
#ifdef __cplusplus
   texture2d->Unmap(subresource);
#else
   texture2d->lpVtbl->Unmap(texture2d, subresource);
#endif
}

static INLINE void
D3D10SetTexture3DEvictionPriority(D3D10Texture3D texture3d,
      UINT eviction_priority)
{
#ifdef __cplusplus
   texture3d->SetEvictionPriority(eviction_priority);
#else
   texture3d->lpVtbl->SetEvictionPriority(texture3d, eviction_priority);
#endif
}
static INLINE UINT D3D10GetTexture3DEvictionPriority(D3D10Texture3D texture3d)
{
#ifdef __cplusplus
   return texture3d->GetEvictionPriority();
#else
   return texture3d->lpVtbl->GetEvictionPriority(texture3d);
#endif
}
static INLINE HRESULT D3D10MapTexture3D(
      D3D10Texture3D          texture3d,
      UINT                    subresource,
      D3D10_MAP               map_type,
      UINT                    map_flags,
      D3D10_MAPPED_TEXTURE3D* mapped_tex3d)
{
#ifdef __cplusplus
   return texture3d->Map(subresource,
         map_type, map_flags, mapped_tex3d);
#else
   return texture3d->lpVtbl->Map(texture3d, subresource,
         map_type, map_flags, mapped_tex3d);
#endif
}
static INLINE void D3D10UnmapTexture3D(D3D10Texture3D texture3d,
      UINT subresource)
{
#ifdef __cplusplus
   texture3d->Unmap(subresource);
#else
   texture3d->lpVtbl->Unmap(texture3d, subresource);
#endif
}

static INLINE void D3D10GetViewResource(D3D10View view,
      D3D10Resource* resource)
{
#ifdef __cplusplus
   view->GetResource(resource);
#else
   view->lpVtbl->GetResource(view, resource);
#endif
}

static INLINE void D3D10GetShaderResourceViewResource(
      D3D10ShaderResourceView shader_resource_view, D3D10Resource* resource)
{
#ifdef __cplusplus
   shader_resource_view->GetResource(resource);
#else
   shader_resource_view->lpVtbl->GetResource(shader_resource_view, resource);
#endif
}

static INLINE void
D3D10GetRenderTargetViewResource(D3D10RenderTargetView render_target_view,
      D3D10Resource* resource)
{
#ifdef __cplusplus
   render_target_view->GetResource(resource);
#else
   render_target_view->lpVtbl->GetResource(render_target_view, resource);
#endif
}

static INLINE void
D3D10GetDepthStencilViewResource(D3D10DepthStencilView depth_stencil_view,
      D3D10Resource* resource)
{
#ifdef __cplusplus
   depth_stencil_view->GetResource(resource);
#else
   depth_stencil_view->lpVtbl->GetResource(depth_stencil_view, resource);
#endif
}

static INLINE void D3D10BeginAsynchronous(D3D10Asynchronous asynchronous)
{
#ifdef __cplusplus
   asynchronous->Begin();
#else
   asynchronous->lpVtbl->Begin(asynchronous);
#endif
}

static INLINE void D3D10EndAsynchronous(D3D10Asynchronous asynchronous)
{
#ifdef __cplusplus
   asynchronous->End();
#else
   asynchronous->lpVtbl->End(asynchronous);
#endif
}

static INLINE HRESULT D3D10GetAsynchronousData(
      D3D10Asynchronous asynchronous, void* data,
      UINT data_size, UINT get_data_flags)
{
#ifdef __cplusplus
   return asynchronous->GetData(data,
         data_size, get_data_flags);
#else
   return asynchronous->lpVtbl->GetData(asynchronous, data,
         data_size, get_data_flags);
#endif
}

static INLINE UINT D3D10GetAsynchronousDataSize(
      D3D10Asynchronous asynchronous)
{
#ifdef __cplusplus
   return asynchronous->GetDataSize();
#else
   return asynchronous->lpVtbl->GetDataSize(asynchronous);
#endif
}

static INLINE void D3D10BeginQuery(D3D10Query query)
{
#ifdef __cplusplus
   query->Begin();
#else
   query->lpVtbl->Begin(query);
#endif
}

static INLINE void D3D10EndQuery(D3D10Query query)
{
#ifdef __cplusplus
   query->End();
#else
   query->lpVtbl->End(query);
#endif
}

static INLINE HRESULT
D3D10GetQueryData(D3D10Query query, void* data,
      UINT data_size, UINT get_data_flags)
{
#ifdef __cplusplus
   return query->GetData(data, data_size, get_data_flags);
#else
   return query->lpVtbl->GetData(query, data, data_size, get_data_flags);
#endif
}

static INLINE UINT D3D10GetQueryDataSize(D3D10Query query)
{
#ifdef __cplusplus
   return query->GetDataSize();
#else
   return query->lpVtbl->GetDataSize(query);
#endif
}

static INLINE void D3D10BeginPredicate(D3D10Predicate predicate)
{
#ifdef __cplusplus
   predicate->Begin();
#else
   predicate->lpVtbl->Begin(predicate);
#endif
}

static INLINE void D3D10EndPredicate(D3D10Predicate predicate)
{
#ifdef __cplusplus
   predicate->End();
#else
   predicate->lpVtbl->End(predicate);
#endif
}

static INLINE HRESULT
D3D10GetPredicateData(D3D10Predicate predicate, void* data,
      UINT data_size, UINT get_data_flags)
{
#ifdef __cplusplus
   return predicate->GetData(data, data_size, get_data_flags);
#else
   return predicate->lpVtbl->GetData(predicate, data,
         data_size, get_data_flags);
#endif
}
static INLINE UINT D3D10GetPredicateDataSize(D3D10Predicate predicate)
{
#ifdef __cplusplus
   return predicate->GetDataSize();
#else
   return predicate->lpVtbl->GetDataSize(predicate);
#endif
}

static INLINE void D3D10BeginCounter(D3D10Counter counter)
{
#ifdef __cplusplus
   counter->Begin();
#else
   counter->lpVtbl->Begin(counter);
#endif
}

static INLINE void D3D10EndCounter(D3D10Counter counter)
{
#ifdef __cplusplus
   counter->End();
#else
   counter->lpVtbl->End(counter);
#endif
}

static INLINE HRESULT
D3D10GetCounterData(D3D10Counter counter, void* data,
      UINT data_size, UINT get_data_flags)
{
#ifdef __cplusplus
   return counter->GetData(data, data_size, get_data_flags);
#else
   return counter->lpVtbl->GetData(counter, data, data_size, get_data_flags);
#endif
}

static INLINE UINT D3D10GetCounterDataSize(D3D10Counter counter)
{
#ifdef __cplusplus
   return counter->GetDataSize();
#else
   return counter->lpVtbl->GetDataSize(counter);
#endif
}

static INLINE void D3D10SetVShaderConstantBuffers(
      D3D10Device device, UINT start_slot, UINT num_buffers,
      D3D10Buffer* const constant_buffers)
{
#ifdef __cplusplus
   device->VSSetConstantBuffers(start_slot, num_buffers, constant_buffers);
#else
   device->lpVtbl->VSSetConstantBuffers(device, start_slot,
         num_buffers, constant_buffers);
#endif
}
static INLINE void D3D10SetPShaderResources(
      D3D10Device                    device,
      UINT                           start_slot,
      UINT                           num_views,
      D3D10ShaderResourceView* const shader_resource_views)
{
#ifdef __cplusplus
   device->PSSetShaderResources(start_slot, num_views,
         shader_resource_views);
#else
   device->lpVtbl->PSSetShaderResources(device, start_slot,
         num_views, shader_resource_views);
#endif
}

static INLINE void D3D10SetPShader(D3D10Device device,
      D3D10PixelShader pixel_shader)
{
#ifdef __cplusplus
   device->PSSetShader(pixel_shader);
#else
   device->lpVtbl->PSSetShader(device, pixel_shader);
#endif
}

static INLINE void D3D10SetPShaderSamplers(
      D3D10Device device, UINT start_slot, UINT num_samplers,
      D3D10SamplerState* const samplers)
{
#ifdef __cplusplus
   device->PSSetSamplers(start_slot, num_samplers, samplers);
#else
   device->lpVtbl->PSSetSamplers(device, start_slot,
         num_samplers, samplers);
#endif
}

static INLINE void D3D10SetVShader(D3D10Device device,
      D3D10VertexShader vertex_shader)
{
#ifdef __cplusplus
   device->VSSetShader(vertex_shader);
#else
   device->lpVtbl->VSSetShader(device, vertex_shader);
#endif
}

static INLINE void D3D10DrawIndexed(
      D3D10Device device, UINT index_count,
      UINT start_index_location, INT base_vertex_location)
{
#ifdef __cplusplus
   device->DrawIndexed(index_count,
         start_index_location, base_vertex_location);
#else
   device->lpVtbl->DrawIndexed(device, index_count,
         start_index_location, base_vertex_location);
#endif
}
static INLINE void D3D10Draw(D3D10Device device,
      UINT vertex_count, UINT start_vertex_location)
{
#ifdef __cplusplus
   device->Draw(vertex_count, start_vertex_location);
#else
   device->lpVtbl->Draw(device, vertex_count, start_vertex_location);
#endif
}

static INLINE void D3D10SetPShaderConstantBuffers(
      D3D10Device device, UINT start_slot,
      UINT num_buffers, D3D10Buffer* const constant_buffers)
{
#ifdef __cplusplus
   device->PSSetConstantBuffers(start_slot,
         num_buffers, constant_buffers);
#else
   device->lpVtbl->PSSetConstantBuffers(device, start_slot,
         num_buffers, constant_buffers);
#endif
}

static INLINE void D3D10SetInputLayout(D3D10Device device,
      D3D10InputLayout input_layout)
{
#ifdef __cplusplus
   device->IASetInputLayout(input_layout);
#else
   device->lpVtbl->IASetInputLayout(device, input_layout);
#endif
}

static INLINE void D3D10SetVertexBuffers(
      D3D10Device        device,
      UINT               start_slot,
      UINT               num_buffers,
      D3D10Buffer* const vertex_buffers,
      UINT*              strides,
      UINT*              offsets)
{
#ifdef __cplusplus
   device->IASetVertexBuffers(
         start_slot, num_buffers,
         vertex_buffers, strides, offsets);
#else
   device->lpVtbl->IASetVertexBuffers(
         device, start_slot, num_buffers,
         vertex_buffers, strides, offsets);
#endif
}

static INLINE void
D3D10SetIndexBuffer(D3D10Device device,
      D3D10Buffer index_buffer, DXGI_FORMAT format, UINT offset)
{
#ifdef __cplusplus
   device->IASetIndexBuffer(index_buffer, format, offset);
#else
   device->lpVtbl->IASetIndexBuffer(device, index_buffer, format, offset);
#endif
}

static INLINE void D3D10DrawIndexedInstanced(
      D3D10Device device,
      UINT        index_count_per_instance,
      UINT        instance_count,
      UINT        start_index_location,
      INT         base_vertex_location,
      UINT        start_instance_location)
{
#ifdef __cplusplus
   device->DrawIndexedInstanced(
         index_count_per_instance,
         instance_count, start_index_location,
         base_vertex_location, start_instance_location);
#else
   device->lpVtbl->DrawIndexedInstanced(
         device, index_count_per_instance,
         instance_count, start_index_location,
         base_vertex_location, start_instance_location);
#endif
}

static INLINE void D3D10DrawInstanced(
      D3D10Device device,
      UINT        vertex_count_per_instance,
      UINT        instance_count,
      UINT        start_vertex_location,
      UINT        start_instance_location)
{
#ifdef __cplusplus
   device->DrawInstanced(
         vertex_count_per_instance,
         instance_count, start_vertex_location,
         start_instance_location);
#else
   device->lpVtbl->DrawInstanced(
         device, vertex_count_per_instance,
         instance_count, start_vertex_location,
         start_instance_location);
#endif
}

static INLINE void D3D10SetGShaderConstantBuffers(
      D3D10Device device, UINT start_slot,
      UINT num_buffers, D3D10Buffer* const constant_buffers)
{
#ifdef __cplusplus
   device->GSSetConstantBuffers(start_slot,
         num_buffers, constant_buffers);
#else
   device->lpVtbl->GSSetConstantBuffers(device, start_slot,
         num_buffers, constant_buffers);
#endif
}

static INLINE void D3D10SetGShader(D3D10Device device,
      D3D10GeometryShader shader)
{
#ifdef __cplusplus
   device->GSSetShader(shader);
#else
   device->lpVtbl->GSSetShader(device, shader);
#endif
}

static INLINE void D3D10SetPrimitiveTopology(D3D10Device device,
      D3D10_PRIMITIVE_TOPOLOGY topology)
{
#ifdef __cplusplus
   device->IASetPrimitiveTopology(topology);
#else
   device->lpVtbl->IASetPrimitiveTopology(device, topology);
#endif
}

static INLINE void D3D10SetVShaderResources(
      D3D10Device                    device,
      UINT                           start_slot,
      UINT                           num_views,
      D3D10ShaderResourceView* const shader_resource_views)
{
#ifdef __cplusplus
   device->VSSetShaderResources(start_slot,
         num_views, shader_resource_views);
#else
   device->lpVtbl->VSSetShaderResources(device, start_slot,
         num_views, shader_resource_views);
#endif
}

static INLINE void D3D10SetVShaderSamplers(
      D3D10Device device, UINT start_slot,
      UINT num_samplers, D3D10SamplerState* const samplers)
{
#ifdef __cplusplus
   device->VSSetSamplers(start_slot, num_samplers, samplers);
#else
   device->lpVtbl->VSSetSamplers(device, start_slot,
         num_samplers, samplers);
#endif
}

static INLINE void
D3D10SetPredication(D3D10Device device, D3D10Predicate predicate,
      BOOL predicate_value)
{
#ifdef __cplusplus
   device->SetPredication(predicate, predicate_value);
#else
   device->lpVtbl->SetPredication(device, predicate, predicate_value);
#endif
}

static INLINE void D3D10SetGShaderResources(
      D3D10Device                    device,
      UINT                           start_slot,
      UINT                           num_views,
      D3D10ShaderResourceView* const shader_resource_views)
{
#ifdef __cplusplus
   device->GSSetShaderResources(start_slot,
         num_views, shader_resource_views);
#else
   device->lpVtbl->GSSetShaderResources(device, start_slot,
         num_views, shader_resource_views);
#endif
}

static INLINE void D3D10SetGShaderSamplers(
      D3D10Device device, UINT start_slot,
      UINT num_samplers, D3D10SamplerState* const samplers)
{
#ifdef __cplusplus
   device->GSSetSamplers(start_slot,
         num_samplers, samplers);
#else
   device->lpVtbl->GSSetSamplers(device, start_slot,
         num_samplers, samplers);
#endif
}

static INLINE void D3D10SetRenderTargets(
      D3D10Device                  device,
      UINT                         num_views,
      D3D10RenderTargetView* const render_target_views,
      D3D10DepthStencilView        depth_stencil_view)
{
#ifdef __cplusplus
   device->OMSetRenderTargets(num_views,
         render_target_views, depth_stencil_view);
#else
   device->lpVtbl->OMSetRenderTargets(device, num_views,
         render_target_views, depth_stencil_view);
#endif
}

static INLINE void D3D10SetBlendState(
      D3D10Device device, D3D10BlendState blend_state,
      FLOAT blend_factor[4], UINT sample_mask)
{
#ifdef __cplusplus
   device->OMSetBlendState(blend_state,
         blend_factor, sample_mask);
#else
   device->lpVtbl->OMSetBlendState(device, blend_state,
         blend_factor, sample_mask);
#endif
}

static INLINE void D3D10SetDepthStencilState(
      D3D10Device device,
      D3D10DepthStencilState depth_stencil_state, UINT stencil_ref)
{
#ifdef __cplusplus
   device->OMSetDepthStencilState(depth_stencil_state, stencil_ref);
#else
   device->lpVtbl->OMSetDepthStencilState(device, depth_stencil_state,
         stencil_ref);
#endif
}

static INLINE void
D3D10SOSetTargets(D3D10Device device, UINT num_buffers,
      D3D10Buffer* const sotargets, UINT* offsets)
{
#ifdef __cplusplus
   device->SOSetTargets(num_buffers, sotargets, offsets);
#else
   device->lpVtbl->SOSetTargets(device, num_buffers, sotargets, offsets);
#endif
}

static INLINE void D3D10DrawAuto(D3D10Device device)
{
#ifdef __cplusplus
   device->DrawAuto();
#else
   device->lpVtbl->DrawAuto(device);
#endif
}

static INLINE void D3D10SetState(D3D10Device device,
      D3D10RasterizerState rasterizer_state)
{
#ifdef __cplusplus
   device->RSSetState(rasterizer_state);
#else
   device->lpVtbl->RSSetState(device, rasterizer_state);
#endif
}

static INLINE void
D3D10SetViewports(D3D10Device device,
      UINT num_viewports, D3D10_VIEWPORT* viewports)
{
#ifdef __cplusplus
   device->RSSetViewports(num_viewports, viewports);
#else
   device->lpVtbl->RSSetViewports(device, num_viewports, viewports);
#endif
}

static INLINE void D3D10SetScissorRects(D3D10Device device,
      UINT num_rects, D3D10_RECT* rects)
{
#ifdef __cplusplus
   device->RSSetScissorRects(num_rects, rects);
#else
   device->lpVtbl->RSSetScissorRects(device, num_rects, rects);
#endif
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
#ifdef __cplusplus
   device->CopySubresourceRegion(
         dst_resource,
         dst_subresource, dst_x, dst_y, dst_z,
         src_resource, src_subresource,
         src_box);
#else
   device->lpVtbl->CopySubresourceRegion(
         device, dst_resource,
         dst_subresource, dst_x, dst_y, dst_z,
         src_resource, src_subresource,
         src_box);
#endif
}

static INLINE void
D3D10CopyResource(D3D10Device device,
      D3D10Resource dst_resource, D3D10Resource src_resource)
{
#ifdef __cplusplus
   device->CopyResource(dst_resource, src_resource);
#else
   device->lpVtbl->CopyResource(device, dst_resource, src_resource);
#endif
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
#ifdef __cplusplus
   device->UpdateSubresource(
         dst_resource, dst_subresource,
         dst_box, src_data, src_row_pitch, src_depth_pitch);
#else
   device->lpVtbl->UpdateSubresource(
         device, dst_resource, dst_subresource,
         dst_box, src_data, src_row_pitch, src_depth_pitch);
#endif
}

static INLINE void D3D10ClearRenderTargetView(
      D3D10Device device, D3D10RenderTargetView render_target_view,
      FLOAT color_rgba[4])
{
#ifdef __cplusplus
   device->ClearRenderTargetView(render_target_view, color_rgba);
#else
   device->lpVtbl->ClearRenderTargetView(device,
         render_target_view, color_rgba);
#endif
}

static INLINE void D3D10ClearDepthStencilView(
      D3D10Device           device,
      D3D10DepthStencilView depth_stencil_view,
      UINT                  clear_flags,
      FLOAT                 depth,
      UINT8                 stencil)
{
#ifdef __cplusplus
   device->ClearDepthStencilView(depth_stencil_view,
         clear_flags, depth, stencil);
#else
   device->lpVtbl->ClearDepthStencilView(device,
         depth_stencil_view, clear_flags, depth, stencil);
#endif
}

static INLINE void
D3D10GenerateMips(D3D10Device device,
      D3D10ShaderResourceView shader_resource_view)
{
#ifdef __cplusplus
   device->GenerateMips(shader_resource_view);
#else
   device->lpVtbl->GenerateMips(device, shader_resource_view);
#endif
}

static INLINE void D3D10ResolveSubresource(
      D3D10Device   device,
      D3D10Resource dst_resource,
      UINT          dst_subresource,
      D3D10Resource src_resource,
      UINT          src_subresource,
      DXGI_FORMAT   format)
{
#ifdef __cplusplus
   device->ResolveSubresource(
         dst_resource, dst_subresource,
         src_resource, src_subresource, format);
#else
   device->lpVtbl->ResolveSubresource(
         device, dst_resource, dst_subresource,
         src_resource, src_subresource, format);
#endif
}

static INLINE void D3D10GetVShaderConstantBuffers(
      D3D10Device device, UINT start_slot,
      UINT num_buffers, D3D10Buffer* constant_buffers)
{
#ifdef __cplusplus
   device->VSGetConstantBuffers(start_slot,
         num_buffers, constant_buffers);
#else
   device->lpVtbl->VSGetConstantBuffers(device, start_slot,
         num_buffers, constant_buffers);
#endif
}

static INLINE void D3D10GetPShaderResources(
      D3D10Device              device,
      UINT                     start_slot,
      UINT                     num_views,
      D3D10ShaderResourceView* shader_resource_views)
{
#ifdef __cplusplus
   device->PSGetShaderResources(start_slot,
         num_views, shader_resource_views);
#else
   device->lpVtbl->PSGetShaderResources(device, start_slot,
         num_views, shader_resource_views);
#endif
}

static INLINE void D3D10GetPShader(D3D10Device device,
      D3D10PixelShader* pixel_shader)
{
#ifdef __cplusplus
   device->PSGetShader(pixel_shader);
#else
   device->lpVtbl->PSGetShader(device, pixel_shader);
#endif
}

static INLINE void D3D10GetPShaderSamplers(
      D3D10Device device, UINT start_slot,
      UINT num_samplers, D3D10SamplerState* samplers)
{
#ifdef __cplusplus
   device->PSGetSamplers(start_slot,
         num_samplers, samplers);
#else
   device->lpVtbl->PSGetSamplers(device, start_slot,
         num_samplers, samplers);
#endif
}

static INLINE void D3D10GetVShader(D3D10Device device,
      D3D10VertexShader* vertex_shader)
{
#ifdef __cplusplus
   device->VSGetShader(vertex_shader);
#else
   device->lpVtbl->VSGetShader(device, vertex_shader);
#endif
}

static INLINE void D3D10GetPShaderConstantBuffers(
      D3D10Device device, UINT start_slot,
      UINT num_buffers, D3D10Buffer* constant_buffers)
{
#ifdef __cplusplus
   device->PSGetConstantBuffers(start_slot,
         num_buffers, constant_buffers);
#else
   device->lpVtbl->PSGetConstantBuffers(device, start_slot,
         num_buffers, constant_buffers);
#endif
}

static INLINE void D3D10GetInputLayout(D3D10Device device,
      D3D10InputLayout* input_layout)
{
#ifdef __cplusplus
   device->IAGetInputLayout(input_layout);
#else
   device->lpVtbl->IAGetInputLayout(device, input_layout);
#endif
}

static INLINE void D3D10GetVertexBuffers(
      D3D10Device  device,
      UINT         start_slot,
      UINT         num_buffers,
      D3D10Buffer* vertex_buffers,
      UINT*        strides,
      UINT*        offsets)
{
#ifdef __cplusplus
   device->IAGetVertexBuffers(
         start_slot, num_buffers,
         vertex_buffers, strides, offsets);
#else
   device->lpVtbl->IAGetVertexBuffers(
         device, start_slot, num_buffers,
         vertex_buffers, strides, offsets);
#endif
}

static INLINE void D3D10GetIndexBuffer(
      D3D10Device device, D3D10Buffer* index_buffer,
      DXGI_FORMAT* format, UINT* offset)
{
#ifdef __cplusplus
   device->IAGetIndexBuffer(index_buffer, format, offset);
#else
   device->lpVtbl->IAGetIndexBuffer(device, index_buffer, format, offset);
#endif
}

static INLINE void D3D10GetGShaderConstantBuffers(
      D3D10Device device, UINT start_slot,
      UINT num_buffers, D3D10Buffer* constant_buffers)
{
#ifdef __cplusplus
   device->GSGetConstantBuffers(start_slot,
         num_buffers, constant_buffers);
#else
   device->lpVtbl->GSGetConstantBuffers(device, start_slot,
         num_buffers, constant_buffers);
#endif
}

static INLINE void D3D10GetGShader(D3D10Device device,
      D3D10GeometryShader* geometry_shader)
{
#ifdef __cplusplus
   device->GSGetShader(geometry_shader);
#else
   device->lpVtbl->GSGetShader(device, geometry_shader);
#endif
}

static INLINE void D3D10GetPrimitiveTopology(D3D10Device device,
      D3D10_PRIMITIVE_TOPOLOGY* topology)
{
#ifdef __cplusplus
   device->IAGetPrimitiveTopology(topology);
#else
   device->lpVtbl->IAGetPrimitiveTopology(device, topology);
#endif
}

static INLINE void D3D10GetVShaderResources(
      D3D10Device              device,
      UINT                     start_slot,
      UINT                     num_views,
      D3D10ShaderResourceView* shader_resource_views)
{
#ifdef __cplusplus
   device->VSGetShaderResources(start_slot,
         num_views, shader_resource_views);
#else
   device->lpVtbl->VSGetShaderResources(device, start_slot,
         num_views, shader_resource_views);
#endif
}
static INLINE void D3D10GetVShaderSamplers(
      D3D10Device device, UINT start_slot,
      UINT num_samplers, D3D10SamplerState* samplers)
{
#ifdef __cplusplus
   device->VSGetSamplers(start_slot, num_samplers, samplers);
#else
   device->lpVtbl->VSGetSamplers(device, start_slot,
         num_samplers, samplers);
#endif
}

static INLINE void
D3D10GetPredication(D3D10Device device,
      D3D10Predicate* predicate, BOOL* predicate_value)
{
#ifdef __cplusplus
   device->GetPredication(predicate, predicate_value);
#else
   device->lpVtbl->GetPredication(device, predicate, predicate_value);
#endif
}

static INLINE void D3D10GetGShaderResources(
      D3D10Device              device,
      UINT                     start_slot,
      UINT                     num_views,
      D3D10ShaderResourceView* shader_resource_views)
{
#ifdef __cplusplus
   device->GSGetShaderResources(start_slot,
         num_views, shader_resource_views);
#else
   device->lpVtbl->GSGetShaderResources(device, start_slot,
         num_views, shader_resource_views);
#endif
}

static INLINE void D3D10GetGShaderSamplers(
      D3D10Device device, UINT start_slot,
      UINT num_samplers, D3D10SamplerState* samplers)
{
#ifdef __cplusplus
   device->GSGetSamplers(start_slot, num_samplers, samplers);
#else
   device->lpVtbl->GSGetSamplers(device, start_slot,
         num_samplers, samplers);
#endif
}

static INLINE void D3D10GetRenderTargets(
      D3D10Device            device,
      UINT                   num_views,
      D3D10RenderTargetView* render_target_views,
      D3D10DepthStencilView* depth_stencil_view)
{
#ifdef __cplusplus
   device->OMGetRenderTargets(num_views,
         render_target_views, depth_stencil_view);
#else
   device->lpVtbl->OMGetRenderTargets(device, num_views,
         render_target_views, depth_stencil_view);
#endif
}

static INLINE void D3D10GetBlendState(
      D3D10Device device,
      D3D10BlendState* blend_state,
      FLOAT blend_factor[4], UINT* sample_mask)
{
#ifdef __cplusplus
   device->OMGetBlendState(blend_state, blend_factor, sample_mask);
#else
   device->lpVtbl->OMGetBlendState(device, blend_state,
         blend_factor, sample_mask);
#endif
}

static INLINE void D3D10GetDepthStencilState(
      D3D10Device device,
      D3D10DepthStencilState* depth_stencil_state, UINT* stencil_ref)
{
#ifdef __cplusplus
   device->OMGetDepthStencilState(depth_stencil_state, stencil_ref);
#else
   device->lpVtbl->OMGetDepthStencilState(device,
         depth_stencil_state, stencil_ref);
#endif
}

static INLINE void
D3D10SOGetTargets(D3D10Device device,
      UINT num_buffers, D3D10Buffer* sotargets, UINT* offsets)
{
#ifdef __cplusplus
   device->SOGetTargets(num_buffers, sotargets, offsets);
#else
   device->lpVtbl->SOGetTargets(device, num_buffers, sotargets, offsets);
#endif
}

static INLINE void D3D10GetState(D3D10Device device,
      D3D10RasterizerState* rasterizer_state)
{
#ifdef __cplusplus
   device->RSGetState(rasterizer_state);
#else
   device->lpVtbl->RSGetState(device, rasterizer_state);
#endif
}

static INLINE void
D3D10GetViewports(D3D10Device device,
      UINT* num_viewports, D3D10_VIEWPORT* viewports)
{
#ifdef __cplusplus
   device->RSGetViewports(num_viewports, viewports);
#else
   device->lpVtbl->RSGetViewports(device, num_viewports, viewports);
#endif
}

static INLINE void D3D10GetScissorRects(D3D10Device device,
      UINT* num_rects, D3D10_RECT* rects)
{
#ifdef __cplusplus
   device->RSGetScissorRects(num_rects, rects);
#else
   device->lpVtbl->RSGetScissorRects(device, num_rects, rects);
#endif
}

static INLINE HRESULT D3D10GetDeviceRemovedReason(D3D10Device device)
{
#ifdef __cplusplus
   return device->GetDeviceRemovedReason();
#else
   return device->lpVtbl->GetDeviceRemovedReason(device);
#endif
}

static INLINE HRESULT D3D10SetExceptionMode(D3D10Device device,
      UINT raise_flags)
{
#ifdef __cplusplus
   return device->SetExceptionMode(raise_flags);
#else
   return device->lpVtbl->SetExceptionMode(device, raise_flags);
#endif
}

static INLINE UINT D3D10GetExceptionMode(D3D10Device device)
{
#ifdef __cplusplus
   return device->GetExceptionMode();
#else
   return device->lpVtbl->GetExceptionMode(device);
#endif
}

static INLINE void D3D10ClearState(D3D10Device device)
{
#ifdef __cplusplus
   device->ClearState();
#else
   device->lpVtbl->ClearState(device);
#endif
}

static INLINE void D3D10Flush(D3D10Device device)
{
#ifdef __cplusplus
   device->Flush();
#else
   device->lpVtbl->Flush(device);
#endif
}

static INLINE HRESULT D3D10CreateBuffer(
      D3D10Device             device,
      D3D10_BUFFER_DESC*      desc,
      D3D10_SUBRESOURCE_DATA* initial_data,
      D3D10Buffer*            buffer)
{
#ifdef __cplusplus
   return device->CreateBuffer(
         desc, initial_data, buffer);
#else
   return device->lpVtbl->CreateBuffer(device,
         desc, initial_data, buffer);
#endif
}

static INLINE HRESULT D3D10CreateTexture1D(
      D3D10Device             device,
      D3D10_TEXTURE1D_DESC*   desc,
      D3D10_SUBRESOURCE_DATA* initial_data,
      D3D10Texture1D*         texture1d)
{
#ifdef __cplusplus
   return device->CreateTexture1D(
         desc, initial_data, texture1d);
#else
   return device->lpVtbl->CreateTexture1D(device,
         desc, initial_data, texture1d);
#endif
}

static INLINE HRESULT D3D10CreateTexture2D(
      D3D10Device             device,
      D3D10_TEXTURE2D_DESC*   desc,
      D3D10_SUBRESOURCE_DATA* initial_data,
      D3D10Texture2D*         texture2d)
{
#ifdef __cplusplus
   return device->CreateTexture2D(
         desc, initial_data, texture2d);
#else
   return device->lpVtbl->CreateTexture2D(device,
         desc, initial_data, texture2d);
#endif
}
static INLINE HRESULT D3D10CreateTexture3D(
      D3D10Device             device,
      D3D10_TEXTURE3D_DESC*   desc,
      D3D10_SUBRESOURCE_DATA* initial_data,
      D3D10Texture3D*         texture3d)
{
#ifdef __cplusplus
   return device->CreateTexture3D(
         desc, initial_data, texture3d);
#else
   return device->lpVtbl->CreateTexture3D(device,
         desc, initial_data, texture3d);
#endif
}

static INLINE HRESULT D3D10CreateShaderResourceViewDevice(
      D3D10Device                      device,
      D3D10Resource                    resource,
      D3D10_SHADER_RESOURCE_VIEW_DESC* desc,
      D3D10ShaderResourceView*         srview)
{
#ifdef __cplusplus
   return device->CreateShaderResourceView(
         resource, desc, srview);
#else
   return device->lpVtbl->CreateShaderResourceView(device,
         resource, desc, srview);
#endif
}
static INLINE HRESULT D3D10CreateRenderTargetViewDevice(
      D3D10Device                    device,
      D3D10Resource                  resource,
      D3D10_RENDER_TARGET_VIEW_DESC* desc,
      D3D10RenderTargetView*         rtview)
{
#ifdef __cplusplus
   return device->CreateRenderTargetView(
         resource, desc, rtview);
#else
   return device->lpVtbl->CreateRenderTargetView(device,
         resource, desc, rtview);
#endif
}

static INLINE HRESULT D3D10CreateDepthStencilView(
      D3D10Device                    device,
      D3D10Resource                  resource,
      D3D10_DEPTH_STENCIL_VIEW_DESC* desc,
      D3D10DepthStencilView*         depth_stencil_view)
{
#ifdef __cplusplus
   return device->CreateDepthStencilView(
         resource, desc, depth_stencil_view);
#else
   return device->lpVtbl->CreateDepthStencilView(device,
         resource, desc, depth_stencil_view);
#endif
}

static INLINE HRESULT D3D10CreateInputLayout(
      D3D10Device               device,
      D3D10_INPUT_ELEMENT_DESC* input_element_descs,
      UINT                      num_elements,
      void*                     shader_bytecode_with_input_signature,
      SIZE_T                    bytecode_length,
      D3D10InputLayout*         input_layout)
{
#ifdef __cplusplus
   return device->CreateInputLayout(
         input_element_descs,
         num_elements, shader_bytecode_with_input_signature,
         bytecode_length, input_layout);
#else
   return device->lpVtbl->CreateInputLayout(
         device, input_element_descs,
         num_elements, shader_bytecode_with_input_signature,
         bytecode_length, input_layout);
#endif
}

static INLINE HRESULT D3D10CreateVertexShader(
      D3D10Device        device,
      void*              shader_bytecode,
      SIZE_T             bytecode_length,
      D3D10VertexShader* vertex_shader)
{
#ifdef __cplusplus
   return device->CreateVertexShader(
         shader_bytecode, bytecode_length, vertex_shader);
#else
   return device->lpVtbl->CreateVertexShader(
         device, shader_bytecode, bytecode_length, vertex_shader);
#endif
}

static INLINE HRESULT D3D10CreateGeometryShader(
      D3D10Device          device,
      void*                shader_bytecode,
      SIZE_T               bytecode_length,
      D3D10GeometryShader* geometry_shader)
{
#ifdef __cplusplus
   return device->CreateGeometryShader(
         shader_bytecode, bytecode_length, geometry_shader);
#else
   return device->lpVtbl->CreateGeometryShader(
         device, shader_bytecode, bytecode_length, geometry_shader);
#endif
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
#ifdef __cplusplus
   return device->CreateGeometryShaderWithStreamOutput(
         shader_bytecode, bytecode_length,
         sodeclaration, num_entries, output_stream_stride,
         geometry_shader);
#else
   return device->lpVtbl->CreateGeometryShaderWithStreamOutput(
         device, shader_bytecode, bytecode_length,
         sodeclaration, num_entries, output_stream_stride,
         geometry_shader);
#endif
}

static INLINE HRESULT D3D10CreatePixelShader(
      D3D10Device       device,
      void*             shader_bytecode,
      SIZE_T            bytecode_length,
      D3D10PixelShader* pixel_shader)
{
#ifdef __cplusplus
   return device->CreatePixelShader(
         shader_bytecode, bytecode_length, pixel_shader);
#else
   return device->lpVtbl->CreatePixelShader(device,
         shader_bytecode, bytecode_length, pixel_shader);
#endif
}

static INLINE HRESULT D3D10CreateBlendState(
      D3D10Device device,
      D3D10_BLEND_DESC* blend_state_desc, D3D10BlendState* blend_state)
{
#ifdef __cplusplus
   return device->CreateBlendState(
         blend_state_desc, blend_state);
#else
   return device->lpVtbl->CreateBlendState(device,
         blend_state_desc, blend_state);
#endif
}

static INLINE HRESULT D3D10CreateDepthStencilState(
      D3D10Device               device,
      D3D10_DEPTH_STENCIL_DESC* depth_stencil_desc,
      D3D10DepthStencilState*   depth_stencil_state)
{
#ifdef __cplusplus
   return device->CreateDepthStencilState(
         depth_stencil_desc, depth_stencil_state);
#else
   return device->lpVtbl->CreateDepthStencilState(device,
         depth_stencil_desc, depth_stencil_state);
#endif
}

static INLINE HRESULT D3D10CreateRasterizerState(
      D3D10Device            device,
      D3D10_RASTERIZER_DESC* rasterizer_desc,
      D3D10RasterizerState*  rasterizer_state)
{
#ifdef __cplusplus
   return device->CreateRasterizerState(
         rasterizer_desc, rasterizer_state);
#else
   return device->lpVtbl->CreateRasterizerState(device,
         rasterizer_desc, rasterizer_state);
#endif
}

static INLINE HRESULT D3D10CreateSamplerState(
      D3D10Device device,
      D3D10_SAMPLER_DESC* sampler_desc,
      D3D10SamplerState* sampler_state)
{
#ifdef __cplusplus
   return device->CreateSamplerState(
         sampler_desc, sampler_state);
#else
   return device->lpVtbl->CreateSamplerState(device,
         sampler_desc, sampler_state);
#endif
}

static INLINE HRESULT
D3D10CreateQuery(D3D10Device device,
      D3D10_QUERY_DESC* query_desc, D3D10Query* query)
{
#ifdef __cplusplus
   return device->CreateQuery(query_desc, query);
#else
   return device->lpVtbl->CreateQuery(device, query_desc, query);
#endif
}

static INLINE HRESULT D3D10CreatePredicate(
      D3D10Device device,
      D3D10_QUERY_DESC* predicate_desc, D3D10Predicate* predicate)
{
#ifdef __cplusplus
   return device->CreatePredicate(
         predicate_desc, predicate);
#else
   return device->lpVtbl->CreatePredicate(device,
         predicate_desc, predicate);
#endif
}

static INLINE HRESULT
D3D10CreateCounter(D3D10Device device,
      D3D10_COUNTER_DESC* counter_desc, D3D10Counter* counter)
{
#ifdef __cplusplus
   return device->CreateCounter(counter_desc, counter);
#else
   return device->lpVtbl->CreateCounter(device, counter_desc, counter);
#endif
}

static INLINE HRESULT
D3D10CheckFormatSupport(D3D10Device device,
      DXGI_FORMAT format, UINT* format_support)
{
#ifdef __cplusplus
   return device->CheckFormatSupport(format, format_support);
#else
   return device->lpVtbl->CheckFormatSupport(device, format, format_support);
#endif
}

static INLINE HRESULT D3D10CheckMultisampleQualityLevels(
      D3D10Device device, DXGI_FORMAT format,
      UINT sample_count, UINT* num_quality_levels)
{
#ifdef __cplusplus
   return device->CheckMultisampleQualityLevels(
         format, sample_count, num_quality_levels);
#else
   return device->lpVtbl->CheckMultisampleQualityLevels(
         device, format, sample_count, num_quality_levels);
#endif
}

static INLINE void D3D10CheckCounterInfo(D3D10Device device,
      D3D10_COUNTER_INFO* counter_info)
{
#ifdef __cplusplus
   device->CheckCounterInfo(counter_info);
#else
   device->lpVtbl->CheckCounterInfo(device, counter_info);
#endif
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
#ifdef __cplusplus
   return device->CheckCounter(
         desc, type, active_counters,
         sz_name, name_length, sz_units, units_length,
         sz_description, description_length);
#else
   return device->lpVtbl->CheckCounter(
         device, desc, type, active_counters,
         sz_name, name_length, sz_units, units_length,
         sz_description, description_length);
#endif
}

static INLINE UINT D3D10GetCreationFlags(D3D10Device device)
{
#ifdef __cplusplus
   return device->GetCreationFlags();
#else
   return device->lpVtbl->GetCreationFlags(device);
#endif
}

static INLINE HRESULT
D3D10OpenSharedResource(D3D10Device device,
      HANDLE h_resource, ID3D10Resource** out)
{
#ifdef __cplusplus
   return device->OpenSharedResource(
         h_resource, uuidof(ID3D10Resource), (void**)out);
#else
   return device->lpVtbl->OpenSharedResource(
         device, h_resource, uuidof(ID3D10Resource), (void**)out);
#endif
}

static INLINE void D3D10SetTextFilterSize(D3D10Device device,
      UINT width, UINT height)
{
#ifdef __cplusplus
   device->SetTextFilterSize(width, height);
#else
   device->lpVtbl->SetTextFilterSize(device, width, height);
#endif
}

static INLINE void D3D10GetTextFilterSize(D3D10Device device,
      UINT* width, UINT* height)
{
#ifdef __cplusplus
   device->GetTextFilterSize(width, height);
#else
   device->lpVtbl->GetTextFilterSize(device, width, height);
#endif
}

static INLINE void D3D10Enter(D3D10Multithread multithread)
{
#ifdef __cplusplus
   multithread->Enter();
#else
   multithread->lpVtbl->Enter(multithread);
#endif
}

static INLINE void D3D10Leave(D3D10Multithread multithread)
{
#ifdef __cplusplus
   multithread->Leave();
#else
   multithread->lpVtbl->Leave(multithread);
#endif
}

static INLINE BOOL D3D10SetMultithreadProtected(
      D3D10Multithread multithread, BOOL mtprotect)
{
#ifdef __cplusplus
   return multithread->SetMultithreadProtected(
         mtprotect);
#else
   return multithread->lpVtbl->SetMultithreadProtected(
         multithread, mtprotect);
#endif
}

static INLINE BOOL D3D10GetMultithreadProtected(
      D3D10Multithread multithread)
{
#ifdef __cplusplus
   return multithread->GetMultithreadProtected();
#else
   return multithread->lpVtbl->GetMultithreadProtected(multithread);
#endif
}

static INLINE HRESULT D3D10SetDebugFeatureMask(
      D3D10Debug debug, UINT mask)
{
#ifdef __cplusplus
   return debug->SetFeatureMask(mask);
#else
   return debug->lpVtbl->SetFeatureMask(debug, mask);
#endif
}

static INLINE UINT D3D10GetDebugFeatureMask(D3D10Debug debug)
{
#ifdef __cplusplus
   return debug->GetFeatureMask();
#else
   return debug->lpVtbl->GetFeatureMask(debug);
#endif
}

static INLINE HRESULT D3D10SetPresentPerRenderOpDelay(
      D3D10Debug debug, UINT milliseconds)
{
#ifdef __cplusplus
   return debug->SetPresentPerRenderOpDelay(milliseconds);
#else
   return debug->lpVtbl->SetPresentPerRenderOpDelay(debug, milliseconds);
#endif
}

static INLINE UINT D3D10GetPresentPerRenderOpDelay(D3D10Debug debug)
{
#ifdef __cplusplus
   return debug->GetPresentPerRenderOpDelay();
#else
   return debug->lpVtbl->GetPresentPerRenderOpDelay(debug);
#endif
}

static INLINE HRESULT D3D10SetSwapChain(D3D10Debug debug,
      IDXGISwapChain* swap_chain)
{
#ifdef __cplusplus
   return debug->SetSwapChain((IDXGISwapChain*)swap_chain);
#else
   return debug->lpVtbl->SetSwapChain(debug, (IDXGISwapChain*)swap_chain);
#endif
}

static INLINE HRESULT D3D10GetSwapChain(D3D10Debug debug,
      IDXGISwapChain** swap_chain)
{
#ifdef __cplusplus
   return debug->GetSwapChain((IDXGISwapChain**)swap_chain);
#else
   return debug->lpVtbl->GetSwapChain(debug, (IDXGISwapChain**)swap_chain);
#endif
}

static INLINE HRESULT D3D10Validate(D3D10Debug debug)
{
#ifdef __cplusplus
   return debug->Validate();
#else
   return debug->lpVtbl->Validate(debug);
#endif
}

static INLINE BOOL    D3D10SetUseRef(D3D10SwitchToRef switch_to_ref,
      BOOL use_ref)
{
#ifdef __cplusplus
   return switch_to_ref->SetUseRef(use_ref);
#else
   return switch_to_ref->lpVtbl->SetUseRef(switch_to_ref, use_ref);
#endif
}

static INLINE BOOL D3D10GetUseRef(D3D10SwitchToRef switch_to_ref)
{
#ifdef __cplusplus
   return switch_to_ref->GetUseRef();
#else
   return switch_to_ref->lpVtbl->GetUseRef(switch_to_ref);
#endif
}

static INLINE HRESULT
D3D10SetMessageCountLimit(D3D10InfoQueue info_queue,
      UINT64 message_count_limit)
{
#ifdef __cplusplus
   return info_queue->SetMessageCountLimit(
         message_count_limit);
#else
   return info_queue->lpVtbl->SetMessageCountLimit(info_queue,
         message_count_limit);
#endif
}

static INLINE void D3D10ClearStoredMessages(D3D10InfoQueue info_queue)
{
#ifdef __cplusplus
   info_queue->ClearStoredMessages();
#else
   info_queue->lpVtbl->ClearStoredMessages(info_queue);
#endif
}

static INLINE HRESULT D3D10GetMessageA(
      D3D10InfoQueue info_queue,
      UINT64         message_index,
      D3D10_MESSAGE* message,
      SIZE_T*        message_byte_length)
{
#ifdef __cplusplus
   return info_queue->GetMessageA(
         message_index, message, message_byte_length);
#else
   return info_queue->lpVtbl->GetMessageA(info_queue,
         message_index, message, message_byte_length);
#endif
}

static INLINE UINT64 D3D10GetNumMessagesAllowedByStorageFilter(
      D3D10InfoQueue info_queue)
{
#ifdef __cplusplus
   return info_queue->GetNumMessagesAllowedByStorageFilter();
#else
   return info_queue->lpVtbl->GetNumMessagesAllowedByStorageFilter(
         info_queue);
#endif
}

static INLINE UINT64 D3D10GetNumMessagesDeniedByStorageFilter(
      D3D10InfoQueue info_queue)
{
#ifdef __cplusplus
   return info_queue->GetNumMessagesDeniedByStorageFilter();
#else
   return info_queue->lpVtbl->GetNumMessagesDeniedByStorageFilter(
         info_queue);
#endif
}

static INLINE UINT64 D3D10GetNumStoredMessages(D3D10InfoQueue info_queue)
{
#ifdef __cplusplus
   return info_queue->GetNumStoredMessages();
#else
   return info_queue->lpVtbl->GetNumStoredMessages(info_queue);
#endif
}

static INLINE UINT64 D3D10GetNumStoredMessagesAllowedByRetrievalFilter(
      D3D10InfoQueue info_queue)
{
#ifdef __cplusplus
   return info_queue->GetNumStoredMessagesAllowedByRetrievalFilter();
#else
   return info_queue->lpVtbl->GetNumStoredMessagesAllowedByRetrievalFilter(
         info_queue);
#endif
}

static INLINE UINT64 D3D10GetNumMessagesDiscardedByMessageCountLimit(
      D3D10InfoQueue info_queue)
{
#ifdef __cplusplus
   return info_queue->GetNumMessagesDiscardedByMessageCountLimit();
#else
   return info_queue->lpVtbl->GetNumMessagesDiscardedByMessageCountLimit(
         info_queue);
#endif
}

static INLINE UINT64 D3D10GetMessageCountLimit(D3D10InfoQueue info_queue)
{
#ifdef __cplusplus
   return info_queue->GetMessageCountLimit();
#else
   return info_queue->lpVtbl->GetMessageCountLimit(info_queue);
#endif
}

static INLINE HRESULT
D3D10AddStorageFilterEntries(D3D10InfoQueue info_queue,
      D3D10_INFO_QUEUE_FILTER* filter)
{
#ifdef __cplusplus
   return info_queue->AddStorageFilterEntries(filter);
#else
   return info_queue->lpVtbl->AddStorageFilterEntries(info_queue, filter);
#endif
}

static INLINE HRESULT D3D10GetStorageFilter(
      D3D10InfoQueue info_queue,
      D3D10_INFO_QUEUE_FILTER* filter, SIZE_T* filter_byte_length)
{
#ifdef __cplusplus
   return info_queue->GetStorageFilter(
         filter, filter_byte_length);
#else
   return info_queue->lpVtbl->GetStorageFilter(
         info_queue, filter, filter_byte_length);
#endif
}

static INLINE void D3D10ClearStorageFilter(D3D10InfoQueue info_queue)
{
#ifdef __cplusplus
   info_queue->ClearStorageFilter();
#else
   info_queue->lpVtbl->ClearStorageFilter(info_queue);
#endif
}

static INLINE HRESULT D3D10PushEmptyStorageFilter(D3D10InfoQueue info_queue)
{
#ifdef __cplusplus
   return info_queue->PushEmptyStorageFilter();
#else
   return info_queue->lpVtbl->PushEmptyStorageFilter(info_queue);
#endif
}

static INLINE HRESULT D3D10PushCopyOfStorageFilter(D3D10InfoQueue info_queue)
{
#ifdef __cplusplus
   return info_queue->PushCopyOfStorageFilter();
#else
   return info_queue->lpVtbl->PushCopyOfStorageFilter(info_queue);
#endif
}

static INLINE HRESULT
D3D10PushStorageFilter(D3D10InfoQueue info_queue,
      D3D10_INFO_QUEUE_FILTER* filter)
{
#ifdef __cplusplus
   return info_queue->PushStorageFilter(filter);
#else
   return info_queue->lpVtbl->PushStorageFilter(info_queue, filter);
#endif
}

static INLINE void D3D10PopStorageFilter(D3D10InfoQueue info_queue)
{
#ifdef __cplusplus
   info_queue->PopStorageFilter();
#else
   info_queue->lpVtbl->PopStorageFilter(info_queue);
#endif
}

static INLINE UINT D3D10GetStorageFilterStackSize(D3D10InfoQueue info_queue)
{
#ifdef __cplusplus
   return info_queue->GetStorageFilterStackSize();
#else
   return info_queue->lpVtbl->GetStorageFilterStackSize(info_queue);
#endif
}

static INLINE HRESULT
D3D10AddRetrievalFilterEntries(D3D10InfoQueue info_queue,
      D3D10_INFO_QUEUE_FILTER* filter)
{
#ifdef __cplusplus
   return info_queue->AddRetrievalFilterEntries(filter);
#else
   return info_queue->lpVtbl->AddRetrievalFilterEntries(info_queue, filter);
#endif
}

static INLINE HRESULT D3D10GetRetrievalFilter(
      D3D10InfoQueue info_queue,
      D3D10_INFO_QUEUE_FILTER* filter, SIZE_T* filter_byte_length)
{
#ifdef __cplusplus
   return info_queue->GetRetrievalFilter(
         filter, filter_byte_length);
#else
   return info_queue->lpVtbl->GetRetrievalFilter(info_queue,
         filter, filter_byte_length);
#endif
}

static INLINE void D3D10ClearRetrievalFilter(D3D10InfoQueue info_queue)
{
#ifdef __cplusplus
   info_queue->ClearRetrievalFilter();
#else
   info_queue->lpVtbl->ClearRetrievalFilter(info_queue);
#endif
}

static INLINE HRESULT D3D10PushEmptyRetrievalFilter(
      D3D10InfoQueue info_queue)
{
#ifdef __cplusplus
   return info_queue->PushEmptyRetrievalFilter();
#else
   return info_queue->lpVtbl->PushEmptyRetrievalFilter(info_queue);
#endif
}

static INLINE HRESULT D3D10PushCopyOfRetrievalFilter(
      D3D10InfoQueue info_queue)
{
#ifdef __cplusplus
   return info_queue->PushCopyOfRetrievalFilter();
#else
   return info_queue->lpVtbl->PushCopyOfRetrievalFilter(info_queue);
#endif
}

static INLINE HRESULT
D3D10PushRetrievalFilter(D3D10InfoQueue info_queue,
      D3D10_INFO_QUEUE_FILTER* filter)
{
#ifdef __cplusplus
   return info_queue->PushRetrievalFilter(filter);
#else
   return info_queue->lpVtbl->PushRetrievalFilter(info_queue, filter);
#endif
}

static INLINE void D3D10PopRetrievalFilter(D3D10InfoQueue info_queue)
{
#ifdef __cplusplus
   info_queue->PopRetrievalFilter();
#else
   info_queue->lpVtbl->PopRetrievalFilter(info_queue);
#endif
}

static INLINE UINT D3D10GetRetrievalFilterStackSize(D3D10InfoQueue info_queue)
{
#ifdef __cplusplus
   return info_queue->GetRetrievalFilterStackSize();
#else
   return info_queue->lpVtbl->GetRetrievalFilterStackSize(info_queue);
#endif
}

static INLINE HRESULT D3D10AddMessage(
      D3D10InfoQueue         info_queue,
      D3D10_MESSAGE_CATEGORY category,
      D3D10_MESSAGE_SEVERITY severity,
      D3D10_MESSAGE_ID       id,
      LPCSTR                 description)
{
#ifdef __cplusplus
   return info_queue->AddMessage(
         category, severity, id, description);
#else
   return info_queue->lpVtbl->AddMessage(info_queue,
         category, severity, id, description);
#endif
}

static INLINE HRESULT D3D10AddApplicationMessage(
      D3D10InfoQueue info_queue,
      D3D10_MESSAGE_SEVERITY severity, LPCSTR description)
{
#ifdef __cplusplus
   return info_queue->AddApplicationMessage(
         severity, description);
#else
   return info_queue->lpVtbl->AddApplicationMessage(
         info_queue, severity, description);
#endif
}

static INLINE HRESULT
D3D10SetBreakOnCategory(D3D10InfoQueue info_queue,
      D3D10_MESSAGE_CATEGORY category, BOOL enable)
{
#ifdef __cplusplus
   return info_queue->SetBreakOnCategory(
         category, enable);
#else
   return info_queue->lpVtbl->SetBreakOnCategory(
         info_queue, category, enable);
#endif
}

static INLINE HRESULT
D3D10SetBreakOnSeverity(D3D10InfoQueue info_queue,
      D3D10_MESSAGE_SEVERITY severity, BOOL enable)
{
#ifdef __cplusplus
   return info_queue->SetBreakOnSeverity(severity, enable);
#else
   return info_queue->lpVtbl->SetBreakOnSeverity(info_queue,
         severity, enable);
#endif
}

static INLINE HRESULT D3D10SetBreakOnID(D3D10InfoQueue info_queue,
      D3D10_MESSAGE_ID id, BOOL enable)
{
#ifdef __cplusplus
   return info_queue->SetBreakOnID(id, enable);
#else
   return info_queue->lpVtbl->SetBreakOnID(info_queue, id, enable);
#endif
}

static INLINE BOOL
D3D10GetBreakOnCategory(D3D10InfoQueue info_queue,
      D3D10_MESSAGE_CATEGORY category)
{
#ifdef __cplusplus
   return info_queue->GetBreakOnCategory(category);
#else
   return info_queue->lpVtbl->GetBreakOnCategory(info_queue, category);
#endif
}

static INLINE BOOL
D3D10GetBreakOnSeverity(D3D10InfoQueue info_queue,
      D3D10_MESSAGE_SEVERITY severity)
{
#ifdef __cplusplus
   return info_queue->GetBreakOnSeverity(severity);
#else
   return info_queue->lpVtbl->GetBreakOnSeverity(info_queue, severity);
#endif
}

static INLINE BOOL D3D10GetBreakOnID(D3D10InfoQueue info_queue,
      D3D10_MESSAGE_ID id)
{
#ifdef __cplusplus
   return info_queue->GetBreakOnID(id);
#else
   return info_queue->lpVtbl->GetBreakOnID(info_queue, id);
#endif
}

static INLINE void D3D10SetMuteDebugOutput(
      D3D10InfoQueue info_queue, BOOL mute)
{
#ifdef __cplusplus
   info_queue->SetMuteDebugOutput(mute);
#else
   info_queue->lpVtbl->SetMuteDebugOutput(info_queue, mute);
#endif
}

static INLINE BOOL D3D10GetMuteDebugOutput(D3D10InfoQueue info_queue)
{
#ifdef __cplusplus
   return info_queue->GetMuteDebugOutput();
#else
   return info_queue->lpVtbl->GetMuteDebugOutput(info_queue);
#endif
}

/* end of auto-generated */

static INLINE HRESULT
DXGIGetSwapChainBufferD3D10(DXGISwapChain swap_chain, UINT buffer,
      D3D10Texture2D* out)
{
#ifdef __cplusplus
   return swap_chain->GetBuffer(buffer,
         uuidof(ID3D10Texture2D), (void**)out);
#else
   return swap_chain->lpVtbl->GetBuffer(swap_chain, buffer,
         uuidof(ID3D10Texture2D), (void**)out);
#endif
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
#ifdef __cplusplus
   device->CopySubresourceRegion(
         (D3D10Resource)dst_texture,
         dst_subresource, dst_x, dst_y, dst_z,
         (D3D10Resource)src_texture, src_subresource, src_box);
#else
   device->lpVtbl->CopySubresourceRegion(
         device, (D3D10Resource)dst_texture,
         dst_subresource, dst_x, dst_y, dst_z,
         (D3D10Resource)src_texture, src_subresource, src_box);
#endif
}
static INLINE HRESULT D3D10CreateTexture2DRenderTargetView(
      D3D10Device                    device,
      D3D10Texture2D                 texture,
      D3D10_RENDER_TARGET_VIEW_DESC* desc,
      D3D10RenderTargetView*         rtview)
{
#ifdef __cplusplus
   return device->CreateRenderTargetView(
         (D3D10Resource)texture, desc, rtview);
#else
   return device->lpVtbl->CreateRenderTargetView(device,
         (D3D10Resource)texture, desc, rtview);
#endif
}
static INLINE HRESULT D3D10CreateTexture2DShaderResourceView(
      D3D10Device                      device,
      D3D10Texture2D                   texture,
      D3D10_SHADER_RESOURCE_VIEW_DESC* desc,
      D3D10ShaderResourceView*         srview)
{
#ifdef __cplusplus
   return device->CreateShaderResourceView((D3D10Resource)texture,
         desc, srview);
#else
   return device->lpVtbl->CreateShaderResourceView(device,
         (D3D10Resource)texture, desc, srview);
#endif
}

   /* internal */

#include <stdbool.h>

#include <retro_math.h>
#include <gfx/math/matrix_4x4.h>

#include "../video_driver.h"

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
   D3D10ShaderResourceView view;
   bool                    dirty;
   bool                    ignore_alpha;
} d3d10_texture_t;

typedef struct
{
   unsigned              cur_mon_id;
   DXGISwapChain         swapChain;
   D3D10Device           device;
   D3D10RenderTargetView renderTargetView;
   D3D10InputLayout      layout;
   D3D10Buffer           ubo;
   D3D10VertexShader     vs;
   D3D10PixelShader      ps;
   D3D10SamplerState     sampler_nearest;
   D3D10SamplerState     sampler_linear;
   D3D10BlendState       blend_enable;
   D3D10BlendState       blend_disable;
   math_matrix_4x4       mvp, mvp_no_rot;
   struct video_viewport vp;
   D3D10_VIEWPORT        viewport;
   DXGI_FORMAT           format;
   float                 clearcolor[4];
   bool                  vsync;
   bool                  resize_chain;
   bool                  keep_aspect;
   bool                  resize_viewport;
   struct
   {
      d3d10_texture_t   texture;
      D3D10Buffer       vbo;
      D3D10SamplerState sampler;
      bool              enabled;
      bool              fullscreen;
   } menu;
   struct
   {
      d3d10_texture_t   texture;
      D3D10Buffer       vbo;
      D3D10Buffer       ubo;
      D3D10SamplerState sampler;
      D3D10_VIEWPORT    viewport;
      int               rotation;
   } frame;
} d3d10_video_t;

void d3d10_init_texture(D3D10Device device, d3d10_texture_t* texture);

void d3d10_update_texture(
      int              width,
      int              height,
      int              pitch,
      DXGI_FORMAT      format,
      const void*      data,
      d3d10_texture_t* texture);

DXGI_FORMAT d3d10_get_closest_match(
      D3D10Device device, DXGI_FORMAT desired_format, UINT desired_format_support);

static INLINE DXGI_FORMAT
d3d10_get_closest_match_texture2D(D3D10Device device, DXGI_FORMAT desired_format)
{
   return d3d10_get_closest_match(
         device, desired_format,
         D3D10_FORMAT_SUPPORT_TEXTURE2D | D3D10_FORMAT_SUPPORT_SHADER_SAMPLE);
}
