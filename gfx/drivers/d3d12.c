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

/* Direct3D 12 driver.
 *
 * Minimum version : Direct3D 12.0 (2015)
 * Minimum OS      : Windows 7, Windows 8
 * Recommended OS  : Windows 10
 */

#define CINTERFACE

#include <string.h>
#include <malloc.h>
#include <math.h>

#include <boolean.h>

#include <string/stdstring.h>
#include <file/file_path.h>
#include <encodings/utf.h>
#include <formats/image.h>

#include <dxgi.h>

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include "../../verbosity.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../font_driver.h"
#include "../common/win32_common.h"
#include "../../performance_counters.h"
#include "../../menu/menu_driver.h"
#include "../video_shader_parse.h"
#include "../drivers_shader/slang_process.h"
#ifdef HAVE_REWIND
#include "../../state_manager.h"
#endif

#include "../common/d3d_common.h"
#include "../common/dxgi_common.h"
#include "../common/d3d12_defines.h"
#include "../common/d3dcompiler_common.h"
#ifdef HAVE_SLANG
#include "../drivers_shader/slang_process.h"
#endif

#ifdef __WINRT__
#include "../../uwp/uwp_func.h"
#endif

#define D3D12_GFX_SYNC() \
{ \
   D3D12Fence fence = d3d12->queue.fence; \
   d3d12->queue.handle->lpVtbl->Signal(d3d12->queue.handle, fence, ++d3d12->queue.fenceValue); \
   if (fence->lpVtbl->GetCompletedValue(fence) < d3d12->queue.fenceValue) \
   { \
      fence->lpVtbl->SetEventOnCompletion(fence, d3d12->queue.fenceValue, d3d12->queue.fenceEvent); \
      WaitForSingleObject(d3d12->queue.fenceEvent, INFINITE); \
   } \
}

#define D3D12_ROLLING_SCANLINE_SIMULATION

typedef struct
{
   d3d12_texture_t               texture;
   const font_renderer_driver_t* font_driver;
   void*                         font_data;
   struct font_atlas*            atlas;
} d3d12_font_t;

static D3D12_RENDER_TARGET_BLEND_DESC d3d12_blend_enable_desc = {
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

static D3D12_RENDER_TARGET_BLEND_DESC d3d12_blend_disable_desc = {
   FALSE,
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

/* Temporary workaround for d3d12 not being able to poll flags during init */
static gfx_ctx_driver_t d3d12_fake_context;

/*
 * D3D12 COMMON
 */
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

static D3D12_GPU_VIRTUAL_ADDRESS
d3d12_create_buffer(D3D12Device device, UINT size_in_bytes, D3D12Resource* buffer)
{
   D3D12_RESOURCE_DESC   resource_desc;
   D3D12_HEAP_PROPERTIES heap_props;

   heap_props.Type                     = D3D12_HEAP_TYPE_UPLOAD;
   heap_props.CPUPageProperty          = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
   heap_props.MemoryPoolPreference     = D3D12_MEMORY_POOL_UNKNOWN;
   heap_props.CreationNodeMask         = 1;
   heap_props.VisibleNodeMask          = 1;

   resource_desc.Dimension             = D3D12_RESOURCE_DIMENSION_BUFFER;
   resource_desc.Alignment             = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
   resource_desc.Width                 = size_in_bytes;
   resource_desc.Height                = 1;
   resource_desc.DepthOrArraySize      = 1;
   resource_desc.MipLevels             = 1;
   resource_desc.Format                = DXGI_FORMAT_UNKNOWN;
   resource_desc.SampleDesc.Count      = 1;
   resource_desc.SampleDesc.Quality    = 0;
   resource_desc.Layout                = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
   resource_desc.Flags                 = D3D12_RESOURCE_FLAG_NONE;

   device->lpVtbl->CreateCommittedResource(
         device, (D3D12_HEAP_PROPERTIES*)&heap_props, D3D12_HEAP_FLAG_NONE, &resource_desc,
         D3D12_RESOURCE_STATE_GENERIC_READ, NULL, uuidof(ID3D12Resource), (void**)buffer);

   return D3D12GetGPUVirtualAddress(*buffer);
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

static D3D12_CPU_DESCRIPTOR_HANDLE d3d12_descriptor_heap_slot_alloc(d3d12_descriptor_heap_t* heap)
{
   int i;
   D3D12_CPU_DESCRIPTOR_HANDLE handle;

   handle.ptr = 0;

   for (i = heap->start; i < (int)heap->desc.NumDescriptors; i++)
   {
      if (!heap->map[i])
      {
         heap->map[i] = true;
         handle.ptr   = heap->cpu.ptr + i * heap->stride;
         heap->start  = i + 1;
         break;
      }
   }
   return handle;
}

static void d3d12_release_texture(d3d12_texture_t* texture)
{
   if (!texture->handle)
      return;

   if (texture->srv_heap && texture->desc.MipLevels <= countof(texture->cpu_descriptor))
   {
      int i;
      d3d12_descriptor_heap_t *heap            = texture->srv_heap;
      for (i = 0; i < texture->desc.MipLevels; i++)
      {
         D3D12_CPU_DESCRIPTOR_HANDLE handle    = texture->cpu_descriptor[i];
         if (handle.ptr)
         {
            unsigned _i                        = (handle.ptr - heap->cpu.ptr) / heap->stride;
            heap->map[_i]                      = false;
            if (heap->start > (int)_i)
               heap->start                     = _i;
         }
         texture->cpu_descriptor[i].ptr = 0;
      }
   }

   Release(texture->handle);
   Release(texture->upload_buffer);
}

static DXGI_FORMAT d3d12_get_closest_match(D3D12Device device, D3D12_FEATURE_DATA_FORMAT_SUPPORT* desired)
{
   DXGI_FORMAT  default_list[] = { desired->Format, DXGI_FORMAT_UNKNOWN };
   DXGI_FORMAT* format         = dxgi_get_format_fallback_list(desired->Format);

   if (!format)
      format                   = default_list;

   while (*format != DXGI_FORMAT_UNKNOWN)
   {
      D3D12_FEATURE_DATA_FORMAT_SUPPORT format_support = { *format };
      if (SUCCEEDED(device->lpVtbl->CheckFeatureSupport(
                device, D3D12_FEATURE_FORMAT_SUPPORT, &format_support, sizeof(format_support)))
          && ((format_support.Support1 & desired->Support1) == desired->Support1)
          && ((format_support.Support2 & desired->Support2) == desired->Support2))
         break;
      format++;
   }
   return *format;
}

static void d3d12_init_texture(D3D12Device device, d3d12_texture_t* texture)
{
   int i;

   if (!texture->desc.MipLevels)
      texture->desc.MipLevels          = 1;

   if (   !(texture->desc.Width  >> (texture->desc.MipLevels - 1))
       && !(texture->desc.Height >> (texture->desc.MipLevels - 1)))
   {
      unsigned width                   = texture->desc.Width >> 5;
      unsigned height                  = texture->desc.Height >> 5;
      texture->desc.MipLevels          = 1;
      while (width && height)
      {
         width  >>= 1;
         height >>= 1;
         texture->desc.MipLevels++;
      }
   }

   {
      D3D12_FEATURE_DATA_FORMAT_SUPPORT format_support;
      D3D12_HEAP_PROPERTIES heap_props;

      format_support.Format          = texture->desc.Format;
      format_support.Support1        = D3D12_FORMAT_SUPPORT1_TEXTURE2D | D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE;
      format_support.Support2        = D3D12_FORMAT_SUPPORT2_NONE;

      heap_props.Type                = D3D12_HEAP_TYPE_DEFAULT;
      heap_props.CPUPageProperty     = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      heap_props.MemoryPoolPreference= D3D12_MEMORY_POOL_UNKNOWN;
      heap_props.CreationNodeMask    = 1;
      heap_props.VisibleNodeMask     = 1;

      if (texture->desc.MipLevels > 1)
      {
         texture->desc.Flags        |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
         format_support.Support1    |= D3D12_FORMAT_SUPPORT1_MIP;
         format_support.Support2    |= D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE;
      }

      if (texture->desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
         format_support.Support1    |= D3D12_FORMAT_SUPPORT1_RENDER_TARGET;

      texture->desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      texture->desc.DepthOrArraySize = 1;
      texture->desc.SampleDesc.Count = 1;
      texture->desc.Format           = d3d12_get_closest_match(device, &format_support);

      device->lpVtbl->CreateCommittedResource(
            device, &heap_props, D3D12_HEAP_FLAG_NONE, &texture->desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, NULL, uuidof(ID3D12Resource), (void**)&texture->handle);
   }

   {
      D3D12_SHADER_RESOURCE_VIEW_DESC desc = { texture->desc.Format };

      desc.Shader4ComponentMapping   = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      desc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
      desc.Texture2D.MipLevels       = texture->desc.MipLevels;

      texture->cpu_descriptor[0]     = d3d12_descriptor_heap_slot_alloc(texture->srv_heap);
      device->lpVtbl->CreateShaderResourceView(device, texture->handle, &desc, texture->cpu_descriptor[0]);
      texture->gpu_descriptor[0].ptr = texture->cpu_descriptor[0].ptr - texture->srv_heap->cpu.ptr +
                                       texture->srv_heap->gpu.ptr;
   }

   for (i = 1; i < texture->desc.MipLevels; i++)
   {
      D3D12_UNORDERED_ACCESS_VIEW_DESC desc = { texture->desc.Format };

      desc.ViewDimension             = D3D12_UAV_DIMENSION_TEXTURE2D;
      desc.Texture2D.MipSlice        = i;

      texture->cpu_descriptor[i]     = d3d12_descriptor_heap_slot_alloc(texture->srv_heap);
      device->lpVtbl->CreateUnorderedAccessView(
            device, (ID3D12Resource*)texture->handle, NULL, &desc, texture->cpu_descriptor[i]);
      texture->gpu_descriptor[i].ptr = texture->cpu_descriptor[i].ptr - texture->srv_heap->cpu.ptr +
                                       texture->srv_heap->gpu.ptr;
   }

   if (texture->desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
      device->lpVtbl->CreateRenderTargetView(device, (ID3D12Resource*)texture->handle, NULL, texture->rt_view);
   else
   {
      D3D12_HEAP_PROPERTIES heap_props;
      D3D12_RESOURCE_DESC   buffer_desc;

      heap_props.Type                     = D3D12_HEAP_TYPE_UPLOAD;
      heap_props.CPUPageProperty          = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
      heap_props.MemoryPoolPreference     = D3D12_MEMORY_POOL_UNKNOWN;
      heap_props.CreationNodeMask         = 1;
      heap_props.VisibleNodeMask          = 1;

      device->lpVtbl->GetCopyableFootprints(
            device, &texture->desc, 0, 1, 0, &texture->layout, &texture->num_rows,
            &texture->row_size_in_bytes, &texture->total_bytes);

      buffer_desc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
      buffer_desc.Alignment          = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
      buffer_desc.Width              = texture->total_bytes;
      buffer_desc.Height             = 1;
      buffer_desc.DepthOrArraySize   = 1;
      buffer_desc.MipLevels          = 1;
      buffer_desc.Format             = DXGI_FORMAT_UNKNOWN;
      buffer_desc.SampleDesc.Count   = 1;
      buffer_desc.SampleDesc.Quality = 0;
      buffer_desc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      buffer_desc.Flags              = D3D12_RESOURCE_FLAG_NONE;

      device->lpVtbl->CreateCommittedResource(
            device, &heap_props, D3D12_HEAP_FLAG_NONE, &buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, NULL, uuidof(ID3D12Resource), (void**)&texture->upload_buffer);
   }

   texture->size_data.x = texture->desc.Width;
   texture->size_data.y = texture->desc.Height;
   texture->size_data.z = 1.0f / texture->desc.Width;
   texture->size_data.w = 1.0f / texture->desc.Height;
}

static void d3d12_upload_texture(D3D12GraphicsCommandList cmd,
      d3d12_texture_t* texture, void *userdata)
{
   D3D12_TEXTURE_COPY_LOCATION src, dst;

   src.pResource        = texture->upload_buffer;
   src.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
   src.PlacedFootprint  = texture->layout;

   dst.pResource        = texture->handle;
   dst.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
   dst.SubresourceIndex = 0;

   D3D12_RESOURCE_TRANSITION(
         cmd,
         texture->handle,
         D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
         D3D12_RESOURCE_STATE_COPY_DEST);

   cmd->lpVtbl->CopyTextureRegion(cmd, &dst, 0, 0, 0, &src, NULL);

   D3D12_RESOURCE_TRANSITION(
         cmd,
         texture->handle,
         D3D12_RESOURCE_STATE_COPY_DEST,
         D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

   if (texture->desc.MipLevels > 1)
   {
      unsigned       i;
      d3d12_video_t* d3d12 = (d3d12_video_t*)userdata;

      cmd->lpVtbl->SetComputeRootSignature(cmd, d3d12->desc.cs_rootSignature);
      cmd->lpVtbl->SetPipelineState(cmd, (D3D12PipelineState)d3d12->mipmapgen_pipe);
      cmd->lpVtbl->SetComputeRootDescriptorTable(cmd, CS_ROOT_ID_TEXTURE_T, texture->gpu_descriptor[0]);

      for (i = 1; i < texture->desc.MipLevels; i++)
      {
         unsigned width  = texture->desc.Width  >> i;
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
            cmd->lpVtbl->ResourceBarrier(cmd, 1, &barrier);
         }

         {
            UINT thread_group_count_x      = (width  + 0x7) >> 3;
            UINT thread_group_count_y      = (height + 0x7) >> 3;
            cmd->lpVtbl->SetComputeRootDescriptorTable(cmd, CS_ROOT_ID_UAV_T, texture->gpu_descriptor[i]);
            cmd->lpVtbl->SetComputeRoot32BitConstants(
                  cmd, CS_ROOT_ID_CONSTANTS, sizeof(cbuffer) / sizeof(uint32_t), &cbuffer, 0);
            cmd->lpVtbl->Dispatch(cmd, thread_group_count_x, thread_group_count_y, 1);
         }

         {
            D3D12_RESOURCE_BARRIER barrier = { D3D12_RESOURCE_BARRIER_TYPE_UAV };
            barrier.UAV.pResource          = texture->handle;
            cmd->lpVtbl->ResourceBarrier(cmd, 1, &barrier);
         }

         {
            D3D12_RESOURCE_BARRIER barrier = { D3D12_RESOURCE_BARRIER_TYPE_TRANSITION };
            barrier.Transition.pResource   = texture->handle;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            barrier.Transition.Subresource = i;
            cmd->lpVtbl->ResourceBarrier(cmd, 1, &barrier);
         }
      }
   }

   texture->dirty = false;
}

static void d3d12_update_texture(
      int              width,
      int              height,
      int              pitch,
      DXGI_FORMAT      format,
      const void*      data,
      d3d12_texture_t* texture)
{
   uint8_t *dst;
   D3D12_RANGE read_range;
   ID3D12Resource *resource = (ID3D12Resource*)texture->upload_buffer;
   read_range.Begin         = 0;
   read_range.End           = 0;
   resource->lpVtbl->Map(resource, 0, &read_range, (void**)&dst);
   dxgi_copy(width, height, format, pitch, data, texture->desc.Format,
         texture->layout.Footprint.RowPitch, dst + texture->layout.Offset);
   resource->lpVtbl->Unmap(resource, 0, NULL);
   texture->dirty           = true;
}

/*
 * DISPLAY DRIVER
 */

static void gfx_display_d3d12_blend_begin(void *data)
{
   d3d12_video_t* d3d12         = (d3d12_video_t*)data;
   D3D12GraphicsCommandList cmd = d3d12->queue.cmd;
   d3d12->sprites.pipe          = d3d12->sprites.pipe_blend;
   cmd->lpVtbl->SetPipelineState(cmd, (D3D12PipelineState)d3d12->sprites.pipe);
}

static void gfx_display_d3d12_blend_end(void *data)
{
   d3d12_video_t* d3d12         = (d3d12_video_t*)data;
   D3D12GraphicsCommandList cmd = d3d12->queue.cmd;
   d3d12->sprites.pipe          = d3d12->sprites.pipe_noblend;
   cmd->lpVtbl->SetPipelineState(cmd, (D3D12PipelineState)d3d12->sprites.pipe);
}

static void gfx_display_d3d12_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   D3D12GraphicsCommandList cmd;
   int vertex_count     = 1;
   d3d12_video_t *d3d12 = (d3d12_video_t*)data;

   if (!d3d12 || !draw || !draw->texture)
      return;

   cmd                  = d3d12->queue.cmd;

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         cmd->lpVtbl->SetPipelineState(cmd, (D3D12PipelineState)d3d12->pipes[draw->pipeline_id]);
         cmd->lpVtbl->DrawInstanced(cmd, draw->coords->vertices, 1, 0, 0);
         cmd->lpVtbl->SetPipelineState(cmd, (D3D12PipelineState)d3d12->sprites.pipe);
         cmd->lpVtbl->IASetPrimitiveTopology(cmd, D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
         cmd->lpVtbl->IASetVertexBuffers(cmd, 0, 1, &d3d12->sprites.vbo_view);
         return;
   }

   if (draw->coords->vertex && draw->coords->tex_coord && draw->coords->color)
      vertex_count = draw->coords->vertices;

   if (   (!(d3d12->flags & D3D12_ST_FLAG_SPRITES_ENABLE))
         || (vertex_count > d3d12->sprites.capacity))
      return;

   if (d3d12->sprites.offset + vertex_count > d3d12->sprites.capacity)
      d3d12->sprites.offset = 0;

   {
      d3d12_sprite_t* sprite;
      D3D12_RANGE range;
      range.Begin           = 0;
      range.End             = 0;
      D3D12Map(d3d12->sprites.vbo, 0, &range, (void**)&sprite);
      sprite += d3d12->sprites.offset;

      if (vertex_count == 1)
      {

         sprite->pos.x    = draw->x      / (float)d3d12->chain.viewport.Width;
         sprite->pos.y    =
            (d3d12->chain.viewport.Height - draw->y - draw->height) /
            (float)d3d12->chain.viewport.Height;
         sprite->pos.w    = draw->width  / (float)d3d12->chain.viewport.Width;
         sprite->pos.h    = draw->height / (float)d3d12->chain.viewport.Height;

         sprite->coords.u = 0.0f;
         sprite->coords.v = 0.0f;
         sprite->coords.w = 1.0f;
         sprite->coords.h = 1.0f;

         if (draw->scale_factor)
            sprite->params.scaling = draw->scale_factor;
         else
            sprite->params.scaling = 1.0f;

         sprite->params.rotation   = draw->rotation;

         sprite->colors[3]         = DXGI_COLOR_RGBA(
               0xFF * draw->coords->color[0],  0xFF * draw->coords->color[1],
               0xFF * draw->coords->color[2],  0xFF * draw->coords->color[3]);
         sprite->colors[2]         = DXGI_COLOR_RGBA(
               0xFF * draw->coords->color[4],  0xFF * draw->coords->color[5],
               0xFF * draw->coords->color[6],  0xFF * draw->coords->color[7]);
         sprite->colors[1]         = DXGI_COLOR_RGBA(
               0xFF * draw->coords->color[8],  0xFF * draw->coords->color[9],
               0xFF * draw->coords->color[10], 0xFF * draw->coords->color[11]);
         sprite->colors[0]         = DXGI_COLOR_RGBA(
               0xFF * draw->coords->color[12], 0xFF * draw->coords->color[13],
               0xFF * draw->coords->color[14], 0xFF * draw->coords->color[15]);
      }
      else
      {
         int          i;
         const float* vertex    = draw->coords->vertex;
         const float* tex_coord = draw->coords->tex_coord;
         const float* color     = draw->coords->color;

         for (i = 0; i < vertex_count; i++)
         {
            d3d12_vertex_t* v = (d3d12_vertex_t*)sprite;
            v->position[0]    = *vertex++;
            v->position[1]    = *vertex++;
            v->texcoord[0]    = *tex_coord++;
            v->texcoord[1]    = *tex_coord++;
            v->color[0]       = *color++;
            v->color[1]       = *color++;
            v->color[2]       = *color++;
            v->color[3]       = *color++;

            sprite++;
         }
         cmd->lpVtbl->SetPipelineState(cmd,
              (D3D12PipelineState)d3d12->pipes[VIDEO_SHADER_STOCK_BLEND]);
         cmd->lpVtbl->IASetPrimitiveTopology(cmd,
               D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      }

      range.Begin = d3d12->sprites.offset * sizeof(*sprite);
      range.End   = (d3d12->sprites.offset + vertex_count) * sizeof(*sprite);
      D3D12Unmap(d3d12->sprites.vbo, 0, &range);
   }

   {
      d3d12_texture_t* texture = (d3d12_texture_t*)draw->texture;
      if (texture->dirty)
      {
         d3d12_upload_texture(cmd, texture, d3d12);

         if (vertex_count > 1)
            cmd->lpVtbl->SetPipelineState(cmd,
                 (D3D12PipelineState)d3d12->pipes[VIDEO_SHADER_STOCK_BLEND]);
         else
            cmd->lpVtbl->SetPipelineState(cmd,
                 (D3D12PipelineState)d3d12->sprites.pipe);
      }
      cmd->lpVtbl->SetGraphicsRootDescriptorTable(cmd, ROOT_ID_TEXTURE_T, texture->gpu_descriptor[0]);
      cmd->lpVtbl->SetGraphicsRootDescriptorTable(cmd, ROOT_ID_SAMPLER_T, texture->sampler);
   }

   cmd->lpVtbl->DrawInstanced(cmd, vertex_count, 1, d3d12->sprites.offset, 0);
   d3d12->sprites.offset += vertex_count;

   if (vertex_count > 1)
   {
      cmd->lpVtbl->SetPipelineState(cmd, (D3D12PipelineState)d3d12->sprites.pipe);
      cmd->lpVtbl->IASetPrimitiveTopology(cmd, D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
   }
}

static void gfx_display_d3d12_draw_pipeline(gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp,
      void *data, unsigned video_width, unsigned video_height)
{
   D3D12GraphicsCommandList cmd;
   d3d12_video_t *d3d12 = (d3d12_video_t*)data;

   if (!d3d12 || !draw)
      return;

   cmd                  = d3d12->queue.cmd;

   switch (draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
      {
         video_coord_array_t* ca   = &p_disp->dispca;

         if (!d3d12->menu_pipeline_vbo)
         {
            D3D12_RANGE read_range;
            void*       vertex_data_begin;

            d3d12->menu_pipeline_vbo_view.StrideInBytes  = 2 * sizeof(float);
            d3d12->menu_pipeline_vbo_view.SizeInBytes    =
                  ca->coords.vertices * d3d12->menu_pipeline_vbo_view.StrideInBytes;
            d3d12->menu_pipeline_vbo_view.BufferLocation = d3d12_create_buffer(
                  d3d12->device, d3d12->menu_pipeline_vbo_view.SizeInBytes,
                  &d3d12->menu_pipeline_vbo);

            read_range.Begin           = 0;
            read_range.End             = 0;
            D3D12Map(d3d12->menu_pipeline_vbo, 0,
                  &read_range, &vertex_data_begin);
            memcpy(vertex_data_begin, ca->coords.vertex,
                  d3d12->menu_pipeline_vbo_view.SizeInBytes);
            D3D12Unmap(d3d12->menu_pipeline_vbo, 0, NULL);
         }
         cmd->lpVtbl->IASetVertexBuffers(cmd, 0, 1,
               &d3d12->menu_pipeline_vbo_view);
         draw->coords->vertices = ca->coords.vertices;
         break;
      }

      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         cmd->lpVtbl->IASetVertexBuffers(cmd, 0, 1, &d3d12->frame.vbo_view);
         draw->coords->vertices = 4;
         break;
      default:
         return;
   }
   cmd->lpVtbl->IASetPrimitiveTopology(cmd,
         D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

   d3d12->ubo_values.time += 0.01f;

   {
      D3D12_RANGE read_range;
      d3d12_uniform_t* mapped_ubo;

      read_range.Begin     = 0;
      read_range.End       = 0;
      D3D12Map(d3d12->ubo, 0, &read_range, (void**)&mapped_ubo);
      *mapped_ubo = d3d12->ubo_values;
      D3D12Unmap(d3d12->ubo, 0, NULL);
   }
   cmd->lpVtbl->SetGraphicsRootConstantBufferView(cmd, ROOT_ID_UBO,
         d3d12->ubo_view.BufferLocation);
}

static void gfx_display_d3d12_scissor_begin(void *data,
      unsigned video_width, unsigned video_height,
      int x, int y, unsigned width, unsigned height)
{
   D3D12_RECT rect;
   D3D12GraphicsCommandList cmd;
   d3d12_video_t *d3d12 = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   cmd                  = d3d12->queue.cmd;

   rect.left            = x;
   rect.top             = y;
   rect.right           = width + x;
   rect.bottom          = height + y;

   cmd->lpVtbl->RSSetScissorRects(cmd, 1, &rect);
}

static void gfx_display_d3d12_scissor_end(void *data,
      unsigned video_width,
      unsigned video_height)
{
   D3D12_RECT rect;
   D3D12GraphicsCommandList cmd;
   d3d12_video_t *d3d12 = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   cmd                  = d3d12->queue.cmd;

   rect.left            = 0;
   rect.top             = 0;
   rect.right           = video_width;
   rect.bottom          = video_height;

   cmd->lpVtbl->RSSetScissorRects(cmd, 1, &rect);
}

gfx_display_ctx_driver_t gfx_display_ctx_d3d12 = {
   gfx_display_d3d12_draw,
   gfx_display_d3d12_draw_pipeline,
   gfx_display_d3d12_blend_begin,
   gfx_display_d3d12_blend_end,
   NULL,                                     /* get_default_mvp        */
   NULL,                                     /* get_default_vertices   */
   NULL,                                     /* get_default_tex_coords */
   FONT_DRIVER_RENDER_D3D12_API,
   GFX_VIDEO_DRIVER_DIRECT3D12,
   "d3d12",
   true,
   gfx_display_d3d12_scissor_begin,
   gfx_display_d3d12_scissor_end
};

/*
 * FONT DRIVER
 */

static void * d3d12_font_init(void* data, const char* font_path,
      float font_size, bool is_threaded)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;
   d3d12_font_t*  font  = (d3d12_font_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   if (!font_renderer_create_default(
             &font->font_driver,
             &font->font_data, font_path, font_size))
   {
      free(font);
      return NULL;
   }

   font->atlas               = font->font_driver->get_atlas(font->font_data);
   font->texture.sampler     = d3d12->samplers[RARCH_FILTER_LINEAR][RARCH_WRAP_BORDER];
   font->texture.desc.Width  = font->atlas->width;
   font->texture.desc.Height = font->atlas->height;
   font->texture.desc.Format = DXGI_FORMAT_A8_UNORM;
   font->texture.srv_heap    = &d3d12->desc.srv_heap;
   d3d12_release_texture(&font->texture);
   d3d12_init_texture(d3d12->device, &font->texture);
   if (font->texture.upload_buffer)
      d3d12_update_texture(
            font->atlas->width, font->atlas->height,
            font->atlas->width, DXGI_FORMAT_A8_UNORM,
            font->atlas->buffer, &font->texture);
   font->atlas->dirty = false;

   return font;
}

static void d3d12_font_free(void* data, bool is_threaded)
{
   d3d12_font_t* font = (d3d12_font_t*)data;

   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   d3d12_release_texture(&font->texture);

   free(font);
}

static int d3d12_font_get_message_width(void* data,
      const char* msg, size_t msg_len, float scale)
{
   size_t i;
   int delta_x                      = 0;
   const struct font_glyph* glyph_q = NULL;
   d3d12_font_t* font               = (d3d12_font_t*)data;

   if (!font)
      return 0;

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph* glyph;
      const char *msg_tmp = &msg[i];
      unsigned    code    = utf8_walk(&msg_tmp);
      unsigned    skip    = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      /* Do something smarter here ... */
      if (!(glyph = font->font_driver->get_glyph(font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      delta_x            += glyph->advance_x;
   }

   return delta_x * scale;
}

static void d3d12_font_render_line(
      d3d12_video_t *d3d12,
      D3D12GraphicsCommandList cmd,
      d3d12_font_t*       font,
      const struct font_glyph* glyph_q,
      const char*         msg,
      size_t              msg_len,
      float               scale,
      const unsigned int  color,
      float               pos_x,
      float               pos_y,
      int pre_x,
      unsigned            width,
      unsigned            height,
      unsigned            text_align)
{
   size_t i;
   D3D12_RANGE     range;
   unsigned        count;
   void*           mapped_vbo       = NULL;
   d3d12_sprite_t* v                = NULL;
   d3d12_sprite_t* vbo_start        = NULL;
   int x                            = pre_x;
   int y                            = roundf((1.0 - pos_y) * height);

   if (d3d12->sprites.offset + msg_len > (unsigned)d3d12->sprites.capacity)
      d3d12->sprites.offset = 0;

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= d3d12_font_get_message_width(font, msg, msg_len, scale);
         break;

      case TEXT_ALIGN_CENTER:
         x -= d3d12_font_get_message_width(font, msg, msg_len, scale) / 2;
         break;
   }

   range.Begin = 0;
   range.End   = 0;
   D3D12Map(d3d12->sprites.vbo, 0, &range, (void**)&vbo_start);

   v           = vbo_start + d3d12->sprites.offset;
   range.Begin = (uintptr_t)v - (uintptr_t)vbo_start;

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph* glyph;
      const char *msg_tmp= &msg[i];
      unsigned   code    = utf8_walk(&msg_tmp);
      unsigned   skip    = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      /* Do something smarter here ... */
      if (!(glyph = font->font_driver->get_glyph(font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      v->pos.x           = (x + (glyph->draw_offset_x * scale)) / (float)d3d12->chain.viewport.Width;
      v->pos.y           = (y + (glyph->draw_offset_y * scale)) / (float)d3d12->chain.viewport.Height;
      v->pos.w           = glyph->width * scale  / (float)d3d12->chain.viewport.Width;
      v->pos.h           = glyph->height * scale / (float)d3d12->chain.viewport.Height;

      v->coords.u        = glyph->atlas_offset_x / (float)font->texture.desc.Width;
      v->coords.v        = glyph->atlas_offset_y / (float)font->texture.desc.Height;
      v->coords.w        = glyph->width          / (float)font->texture.desc.Width;
      v->coords.h        = glyph->height         / (float)font->texture.desc.Height;

      v->params.scaling  = 1;
      v->params.rotation = 0;

      v->colors[0]       = color;
      v->colors[1]       = color;
      v->colors[2]       = color;
      v->colors[3]       = color;

      v++;

      x                 += glyph->advance_x * scale;
      y                 += glyph->advance_y * scale;
   }

   range.End = (uintptr_t)v - (uintptr_t)vbo_start;
   D3D12Unmap(d3d12->sprites.vbo, 0, &range);

   count = v - vbo_start - d3d12->sprites.offset;

   if (!count)
      return;

   if (font->atlas->dirty)
   {
      if (font->texture.upload_buffer)
         d3d12_update_texture(
               font->atlas->width, font->atlas->height,
               font->atlas->width, DXGI_FORMAT_A8_UNORM,
               font->atlas->buffer, &font->texture);
      font->atlas->dirty = false;
   }

   if (font->texture.dirty)
      d3d12_upload_texture(cmd, &font->texture, d3d12);

   cmd->lpVtbl->SetPipelineState(cmd, (D3D12PipelineState)d3d12->sprites.pipe_font);
   cmd->lpVtbl->SetGraphicsRootDescriptorTable(cmd, ROOT_ID_TEXTURE_T,
         font->texture.gpu_descriptor[0]);
   cmd->lpVtbl->SetGraphicsRootDescriptorTable(cmd, ROOT_ID_SAMPLER_T,
         font->texture.sampler);
   cmd->lpVtbl->DrawInstanced(cmd, count, 1, d3d12->sprites.offset, 0);

   cmd->lpVtbl->SetPipelineState(cmd, (D3D12PipelineState)d3d12->sprites.pipe);

   d3d12->sprites.offset += count;
}

static void d3d12_font_render_message(
      d3d12_video_t *d3d12,
      d3d12_font_t*       font,
      const char*         msg,
      float               scale,
      const unsigned int  color,
      float               pos_x,
      float               pos_y,
      unsigned            width,
      unsigned            height,
      unsigned            text_align)
{
   float line_height;
   D3D12GraphicsCommandList cmd           = d3d12->queue.cmd;
   struct font_line_metrics *line_metrics = NULL;
   int lines                              = 0;
   int x                                  = roundf(pos_x * width);
   const struct font_glyph* glyph_q       = font->font_driver->get_glyph(font->font_data, '?');
   font->font_driver->get_line_metrics(font->font_data, &line_metrics);
   line_height = line_metrics->height * scale / height;

   for (;;)
   {
      const char* delim = strchr(msg, '\n');
      size_t msg_len    = delim ? (size_t)(delim - msg) : strlen(msg);

      /* Draw the line */
      if (msg_len <= (size_t)d3d12->sprites.capacity)
         d3d12_font_render_line(d3d12, cmd,
               font, glyph_q, msg, msg_len, scale, color, pos_x,
               pos_y - (float)lines * line_height,
               x,
               width, height, text_align);

      if (!delim)
         break;

      msg += msg_len + 1;
      lines++;
   }
}

static void d3d12_font_render_msg(
      void *userdata,
      void* data,
      const char* msg,
      const struct font_params *params)
{
   float                     x, y, scale, drop_mod, drop_alpha;
   int                       drop_x, drop_y;
   enum text_alignment       text_align;
   unsigned                  color, r, g, b, alpha;
   d3d12_video_t           *d3d12   = (d3d12_video_t*)userdata;
   d3d12_font_t*             font   = (d3d12_font_t*)data;
   unsigned                  width  = d3d12->vp.full_width;
   unsigned                  height = d3d12->vp.full_height;

   if (!font || !msg || !*msg)
      return;
   if (!d3d12 || (!(d3d12->flags & D3D12_ST_FLAG_SPRITES_ENABLE)))
      return;

   if (params)
   {
      x          = params->x;
      y          = params->y;
      scale      = params->scale;
      text_align = params->text_align;
      drop_x     = params->drop_x;
      drop_y     = params->drop_y;
      drop_mod   = params->drop_mod;
      drop_alpha = params->drop_alpha;

      r          = FONT_COLOR_GET_RED(params->color);
      g          = FONT_COLOR_GET_GREEN(params->color);
      b          = FONT_COLOR_GET_BLUE(params->color);
      alpha      = FONT_COLOR_GET_ALPHA(params->color);
      color      = DXGI_COLOR_RGBA(r, g, b, alpha);
   }
   else
   {
      settings_t *settings      = config_get_ptr();
      float video_msg_pos_x     = settings->floats.video_msg_pos_x;
      float video_msg_pos_y     = settings->floats.video_msg_pos_y;
      float video_msg_color_r   = settings->floats.video_msg_color_r;
      float video_msg_color_g   = settings->floats.video_msg_color_g;
      float video_msg_color_b   = settings->floats.video_msg_color_b;
      x                         = video_msg_pos_x;
      y                         = video_msg_pos_y;
      scale                     = 1.0f;
      text_align                = TEXT_ALIGN_LEFT;

      r                         = (video_msg_color_r * 255);
      g                         = (video_msg_color_g * 255);
      b                         = (video_msg_color_b * 255);
      alpha                     = 255;
      color                     = DXGI_COLOR_RGBA(r, g, b, alpha);

      drop_x                    = -2;
      drop_y                    = -2;
      drop_mod                  = 0.3f;
      drop_alpha                = 1.0f;
   }

   if (drop_x || drop_y)
   {
      unsigned r_dark           = r * drop_mod;
      unsigned g_dark           = g * drop_mod;
      unsigned b_dark           = b * drop_mod;
      unsigned alpha_dark       = alpha * drop_alpha;
      unsigned color_dark       = DXGI_COLOR_RGBA(r_dark, g_dark, b_dark, alpha_dark);

      d3d12_font_render_message(d3d12,
            font, msg, scale, color_dark,
            x + scale * drop_x / width,
            y + scale * drop_y / height,
            width, height, text_align);
   }

   d3d12_font_render_message(d3d12, font,
         msg, scale, color, x, y,
         width, height, text_align);
}

static const struct font_glyph* d3d12_font_get_glyph(
      void* data, uint32_t code)
{
   d3d12_font_t* font = (d3d12_font_t*)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph((void*)font->font_driver, code);
   return NULL;
}

static bool d3d12_font_get_line_metrics(void* data, struct font_line_metrics **metrics)
{
   d3d12_font_t* font = (d3d12_font_t*)data;
   if (font && font->font_driver && font->font_data)
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

font_renderer_t d3d12_font = {
   d3d12_font_init,
   d3d12_font_free,
   d3d12_font_render_msg,
   "d3d12",
   d3d12_font_get_glyph,
   NULL, /* bind_block */
   NULL, /* flush */
   d3d12_font_get_message_width,
   d3d12_font_get_line_metrics
};

/*
 * VIDEO DRIVER
 */

static uint32_t d3d12_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_CUSTOMIZABLE_FRAME_LATENCY);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
   BIT32_SET(flags, GFX_CTX_FLAGS_OVERLAY_BEHIND_MENU_SUPPORTED);
   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
   BIT32_SET(flags, GFX_CTX_FLAGS_SUBFRAME_SHADERS);
#endif

   return flags;
}

#ifdef HAVE_OVERLAY
static void d3d12_free_overlays(d3d12_video_t* d3d12)
{
   size_t i;
   for (i = 0; i < (unsigned)d3d12->overlays.count; i++)
      d3d12_release_texture(&d3d12->overlays.textures[i]);

   Release(d3d12->overlays.vbo);
}

static void
d3d12_overlay_vertex_geom(void* data, unsigned index, float x, float y, float w, float h)
{
   D3D12_RANGE range;
   d3d12_sprite_t* sprites = NULL;
   d3d12_video_t*  d3d12   = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   range.Begin             = 0;
   range.End               = 0;
   D3D12Map(d3d12->overlays.vbo, 0, &range, (void**)&sprites);

   sprites[index].pos.x    = x;
   sprites[index].pos.y    = y;
   sprites[index].pos.w    = w;
   sprites[index].pos.h    = h;

   range.Begin             = index * sizeof(*sprites);
   range.End               = range.Begin + sizeof(*sprites);
   D3D12Unmap(d3d12->overlays.vbo, 0, &range);
}

static void d3d12_overlay_tex_geom(void* data, unsigned index, float u, float v, float w, float h)
{
   D3D12_RANGE range;
   d3d12_sprite_t* sprites = NULL;
   d3d12_video_t*  d3d12   = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   range.Begin             = 0;
   range.End               = 0;
   D3D12Map(d3d12->overlays.vbo, 0, &range, (void**)&sprites);

   sprites[index].coords.u = u;
   sprites[index].coords.v = v;
   sprites[index].coords.w = w;
   sprites[index].coords.h = h;

   range.Begin             = index * sizeof(*sprites);
   range.End               = range.Begin + sizeof(*sprites);
   D3D12Unmap(d3d12->overlays.vbo, 0, &range);
}

static void d3d12_overlay_set_alpha(void* data, unsigned index, float mod)
{
   D3D12_RANGE range;
   d3d12_sprite_t* sprites  = NULL;
   d3d12_video_t*  d3d12    = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   range.Begin              = 0;
   range.End                = 0;
   D3D12Map(d3d12->overlays.vbo, 0, &range, (void**)&sprites);

   sprites[index].colors[0] = DXGI_COLOR_RGBA(0xFF, 0xFF, 0xFF, mod * 0xFF);
   sprites[index].colors[1] = sprites[index].colors[0];
   sprites[index].colors[2] = sprites[index].colors[0];
   sprites[index].colors[3] = sprites[index].colors[0];

   range.Begin              = index * sizeof(*sprites);
   range.End                = range.Begin + sizeof(*sprites);
   D3D12Unmap(d3d12->overlays.vbo, 0, &range);
}

static bool d3d12_overlay_load(void* data, const void* image_data, unsigned num_images)
{
   size_t i;
   D3D12_RANGE range;
   d3d12_sprite_t*             sprites = NULL;
   d3d12_video_t*              d3d12   = (d3d12_video_t*)data;
   const struct texture_image* images  = (const struct texture_image*)image_data;

   if (!d3d12)
      return false;

   D3D12_GFX_SYNC();

   d3d12_free_overlays(d3d12);
   d3d12->overlays.count    = num_images;
   d3d12->overlays.textures = (d3d12_texture_t*)calloc(num_images, sizeof(d3d12_texture_t));

   d3d12->overlays.count                   = num_images;
   d3d12->overlays.vbo_view.SizeInBytes    = sizeof(d3d12_sprite_t) * d3d12->overlays.count;
   d3d12->overlays.vbo_view.StrideInBytes  = sizeof(d3d12_sprite_t);
   d3d12->overlays.vbo_view.BufferLocation = d3d12_create_buffer(
         d3d12->device, d3d12->overlays.vbo_view.SizeInBytes, &d3d12->overlays.vbo);

   range.Begin                             = 0;
   range.End                               = 0;
   D3D12Map(d3d12->overlays.vbo, 0, &range, (void**)&sprites);

   for (i = 0; i < num_images; i++)
   {
      d3d12->overlays.textures[i].desc.Width  = images[i].width;
      d3d12->overlays.textures[i].desc.Height = images[i].height;
      d3d12->overlays.textures[i].desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
      d3d12->overlays.textures[i].srv_heap    = &d3d12->desc.srv_heap;

      d3d12_release_texture(&d3d12->overlays.textures[i]);
      d3d12_init_texture(d3d12->device, &d3d12->overlays.textures[i]);
      if (d3d12->overlays.textures[i].upload_buffer)
         d3d12_update_texture(
               images[i].width, images[i].height,
               0, DXGI_FORMAT_B8G8R8A8_UNORM, images[i].pixels,
               &d3d12->overlays.textures[i]);

      sprites[i].pos.x           = 0.0f;
      sprites[i].pos.y           = 0.0f;
      sprites[i].pos.w           = 1.0f;
      sprites[i].pos.h           = 1.0f;

      sprites[i].coords.u        = 0.0f;
      sprites[i].coords.v        = 0.0f;
      sprites[i].coords.w        = 1.0f;
      sprites[i].coords.h        = 1.0f;

      sprites[i].params.scaling  = 1;
      sprites[i].params.rotation = 0;

      sprites[i].colors[0]       = 0xFFFFFFFF;
      sprites[i].colors[1]       = sprites[i].colors[0];
      sprites[i].colors[2]       = sprites[i].colors[0];
      sprites[i].colors[3]       = sprites[i].colors[0];
   }
   D3D12Unmap(d3d12->overlays.vbo, 0, NULL);

   return true;
}

static void d3d12_overlay_enable(void* data, bool state)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   if (state)
      d3d12->flags |=  D3D12_ST_FLAG_OVERLAYS_ENABLE;
   else
      d3d12->flags &= ~D3D12_ST_FLAG_OVERLAYS_ENABLE;
   win32_show_cursor(d3d12, state);
}

static void d3d12_overlay_full_screen(void* data, bool enable)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   if (d3d12)
   {
      if (enable)
         d3d12->flags |=  D3D12_ST_FLAG_OVERLAYS_FULLSCREEN;
      else
         d3d12->flags &= ~D3D12_ST_FLAG_OVERLAYS_FULLSCREEN;
   }
}

static void d3d12_get_overlay_interface(void* data, const video_overlay_interface_t** iface)
{
   static const video_overlay_interface_t overlay_interface = {
      d3d12_overlay_enable,      d3d12_overlay_load,        d3d12_overlay_tex_geom,
      d3d12_overlay_vertex_geom, d3d12_overlay_full_screen, d3d12_overlay_set_alpha,
   };

   *iface = &overlay_interface;
}

static void d3d12_render_overlay(d3d12_video_t *d3d12)
{
   size_t i;
   D3D12GraphicsCommandList cmd = d3d12->queue.cmd;

   if (d3d12->flags & D3D12_ST_FLAG_OVERLAYS_FULLSCREEN)
   {
      cmd->lpVtbl->RSSetViewports(cmd, 1, &d3d12->chain.viewport);
      cmd->lpVtbl->RSSetScissorRects(cmd, 1, &d3d12->chain.scissorRect);
   }
   else
   {
      cmd->lpVtbl->RSSetViewports(cmd, 1, &d3d12->frame.viewport);
      cmd->lpVtbl->RSSetScissorRects(cmd, 1, &d3d12->frame.scissorRect);

   }

   cmd->lpVtbl->IASetVertexBuffers(cmd, 0, 1, &d3d12->overlays.vbo_view);
   cmd->lpVtbl->SetPipelineState(cmd, d3d12->sprites.pipe_blend);

   cmd->lpVtbl->SetGraphicsRootDescriptorTable(
         cmd, ROOT_ID_SAMPLER_T,
         d3d12->samplers[RARCH_FILTER_UNSPEC][RARCH_WRAP_DEFAULT]);

   for (i = 0; i < (unsigned)d3d12->overlays.count; i++)
   {
      if (d3d12->overlays.textures[i].dirty)
         d3d12_upload_texture(cmd,
               &d3d12->overlays.textures[i],
               d3d12);

      cmd->lpVtbl->SetGraphicsRootDescriptorTable(
            cmd, ROOT_ID_TEXTURE_T,
            d3d12->overlays.textures[i].gpu_descriptor[0]);
      cmd->lpVtbl->DrawInstanced(cmd, 1, 1, i, 0);
   }
}
#endif

#ifdef HAVE_DXGI_HDR
static void d3d12_set_hdr_max_nits(void* data, float max_nits)
{
   D3D12_RANGE read_range;
   dxgi_hdr_uniform_t *mapped_ubo         = NULL;
   d3d12_video_t *d3d12                   = (d3d12_video_t*)data;

   d3d12->hdr.max_output_nits             = max_nits;
   d3d12->hdr.ubo_values.max_nits         = max_nits;

   read_range.Begin                       = 0;
   read_range.End                         = 0;
   D3D12Map(d3d12->hdr.ubo, 0, &read_range, (void**)&mapped_ubo);
   *mapped_ubo                            = d3d12->hdr.ubo_values;
   D3D12Unmap(d3d12->hdr.ubo, 0, NULL);

   dxgi_set_hdr_metadata(
         d3d12->chain.handle,
         (d3d12->flags & D3D12_ST_FLAG_HDR_SUPPORT) ? true : false,
         d3d12->chain.bit_depth,
         d3d12->chain.color_space,
         d3d12->hdr.max_output_nits,
         d3d12->hdr.min_output_nits,
         d3d12->hdr.max_cll,
         d3d12->hdr.max_fall);
}

static void d3d12_set_hdr_paper_white_nits(void* data, float paper_white_nits)
{
   D3D12_RANGE read_range;
   dxgi_hdr_uniform_t *mapped_ubo         = NULL;
   d3d12_video_t *d3d12                   = (d3d12_video_t*)data;

   d3d12->hdr.ubo_values.paper_white_nits = paper_white_nits;

   read_range.Begin                       = 0;
   read_range.End                         = 0;
   D3D12Map(d3d12->hdr.ubo, 0, &read_range, (void**)&mapped_ubo);
   *mapped_ubo = d3d12->hdr.ubo_values;
   D3D12Unmap(d3d12->hdr.ubo, 0, NULL);
}

static void d3d12_set_hdr_contrast(void* data, float contrast)
{
   D3D12_RANGE read_range;
   d3d12_video_t *d3d12                   = (d3d12_video_t*)data;
   dxgi_hdr_uniform_t *mapped_ubo         = NULL;

   d3d12->hdr.ubo_values.contrast         = contrast;

   read_range.Begin                       = 0;
   read_range.End                         = 0;
   D3D12Map(d3d12->hdr.ubo, 0, &read_range, (void**)&mapped_ubo);
   *mapped_ubo = d3d12->hdr.ubo_values;
   D3D12Unmap(d3d12->hdr.ubo, 0, NULL);
}

static void d3d12_set_hdr_expand_gamut(void* data, bool expand_gamut)
{
   D3D12_RANGE read_range;
   dxgi_hdr_uniform_t *mapped_ubo         = NULL;
   d3d12_video_t *d3d12                   = (d3d12_video_t*)data;

   d3d12->hdr.ubo_values.expand_gamut     = expand_gamut ? 1.0f : 0.0f;

   read_range.Begin                       = 0;
   read_range.End                         = 0;
   D3D12Map(d3d12->hdr.ubo, 0, &read_range, (void**)&mapped_ubo);
   *mapped_ubo = d3d12->hdr.ubo_values;
   D3D12Unmap(d3d12->hdr.ubo, 0, NULL);
}

static void d3d12_set_hdr_inverse_tonemap(d3d12_video_t* d3d12, bool inverse_tonemap)
{
   D3D12_RANGE read_range;
   dxgi_hdr_uniform_t *mapped_ubo         = NULL;

   d3d12->hdr.ubo_values.inverse_tonemap  = inverse_tonemap ? 1.0f : 0.0f;

   read_range.Begin                       = 0;
   read_range.End                         = 0;
   D3D12Map(d3d12->hdr.ubo, 0, &read_range, (void**)&mapped_ubo);
   *mapped_ubo = d3d12->hdr.ubo_values;
   D3D12Unmap(d3d12->hdr.ubo, 0, NULL);
}

static void d3d12_set_hdr10(d3d12_video_t* d3d12, bool hdr10)
{
   D3D12_RANGE read_range;
   dxgi_hdr_uniform_t *mapped_ubo         = NULL;

   d3d12->hdr.ubo_values.hdr10            = hdr10 ? 1.0f : 0.0f;

   read_range.Begin                       = 0;
   read_range.End                         = 0;
   D3D12Map(d3d12->hdr.ubo, 0, &read_range, (void**)&mapped_ubo);
   *mapped_ubo = d3d12->hdr.ubo_values;
   D3D12Unmap(d3d12->hdr.ubo, 0, NULL);
}
#endif

static void d3d12_set_filtering(void* data, unsigned index, bool smooth, bool ctx_scaling)
{
   int i;
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   for (i = 0; i < RARCH_WRAP_MAX; i++)
   {
      if (smooth)
         d3d12->samplers[RARCH_FILTER_UNSPEC][i] = d3d12->samplers[RARCH_FILTER_LINEAR][i];
      else
         d3d12->samplers[RARCH_FILTER_UNSPEC][i] = d3d12->samplers[RARCH_FILTER_NEAREST][i];
   }
}

static void d3d12_gfx_set_rotation(void* data, unsigned rotation)
{
   math_matrix_4x4* mvp;
   float radians, cosine, sine;
   static math_matrix_4x4 rot  = {
      { 0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    1.0f }
   };
   D3D12_RANGE      read_range;
   d3d12_video_t*   d3d12      = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   D3D12_GFX_SYNC();
   d3d12->frame.rotation = rotation;

   radians                 = d3d12->frame.rotation * (M_PI / 2.0f);
   cosine                  = cosf(radians);
   sine                    = sinf(radians);
   MAT_ELEM_4X4(rot, 0, 0) = cosine;
   MAT_ELEM_4X4(rot, 0, 1) = -sine;
   MAT_ELEM_4X4(rot, 1, 0) = sine;
   MAT_ELEM_4X4(rot, 1, 1) = cosine;
   matrix_4x4_multiply(d3d12->mvp, rot, d3d12->mvp_no_rot);

   read_range.Begin            = 0;
   read_range.End              = 0;
   D3D12Map(d3d12->frame.ubo, 0, &read_range, (void**)&mvp);
   *mvp                        = d3d12->mvp;
   D3D12Unmap(d3d12->frame.ubo, 0, NULL);
}

static void d3d12_update_viewport(d3d12_video_t *d3d12, bool force_full)
{
   video_driver_update_viewport(&d3d12->vp, force_full,
         (d3d12->flags & D3D12_ST_FLAG_KEEP_ASPECT) ? true : false);

   d3d12->frame.viewport.TopLeftX = d3d12->vp.x;
   d3d12->frame.viewport.TopLeftY = d3d12->vp.y;
   d3d12->frame.viewport.Width    = d3d12->vp.width;
   d3d12->frame.viewport.Height   = d3d12->vp.height;
   d3d12->frame.viewport.MaxDepth = 0.0f;
   d3d12->frame.viewport.MaxDepth = 1.0f;

   /* Needed for UWP to be happy */
   d3d12->frame.scissorRect.top    = d3d12->vp.y;
   d3d12->frame.scissorRect.left   = d3d12->vp.x;
   d3d12->frame.scissorRect.right  = d3d12->vp.x + d3d12->vp.width;
   d3d12->frame.scissorRect.bottom = d3d12->vp.y + d3d12->vp.height;

   if (d3d12->shader_preset
         && (  d3d12->frame.output_size.x != d3d12->vp.width
            || d3d12->frame.output_size.y != d3d12->vp.height))
      d3d12->flags           |= D3D12_ST_FLAG_RESIZE_RTS;

   d3d12->frame.output_size.x = d3d12->vp.width;
   d3d12->frame.output_size.y = d3d12->vp.height;
   d3d12->frame.output_size.z = 1.0f / d3d12->vp.width;
   d3d12->frame.output_size.w = 1.0f / d3d12->vp.height;

   d3d12->flags              &= ~D3D12_ST_FLAG_RESIZE_VIEWPORT;
}

static void d3d12_free_shader_preset(d3d12_video_t* d3d12)
{
   size_t i;

   if (!d3d12->shader_preset)
      return;

   for (i = 0; i < d3d12->shader_preset->passes; i++)
   {
      int j;

      free(d3d12->shader_preset->pass[i].source.string.vertex);
      free(d3d12->shader_preset->pass[i].source.string.fragment);
      free(d3d12->pass[i].semantics.textures);
      d3d12->shader_preset->pass[i].source.string.vertex   = NULL;
      d3d12->shader_preset->pass[i].source.string.fragment = NULL;
      d3d12->pass[i].semantics.textures                    = NULL;
      d3d12_release_texture(&d3d12->pass[i].rt);
      d3d12_release_texture(&d3d12->pass[i].feedback);

      for (j = 0; j < SLANG_CBUFFER_MAX; j++)
      {
         free(d3d12->pass[i].semantics.cbuffers[j].uniforms);
         d3d12->pass[i].semantics.cbuffers[j].uniforms = NULL;
         Release(d3d12->pass[i].buffers[j]);
      }

      Release(d3d12->pass[i].pipe);
   }

   memset(d3d12->pass, 0, sizeof(d3d12->pass));

   /* only free the history textures here */
   for (i = 1; i <= (unsigned)d3d12->shader_preset->history_size; i++)
      d3d12_release_texture(&d3d12->frame.texture[i]);

   memset(
         &d3d12->frame.texture[1], 0,
         sizeof(d3d12->frame.texture[1]) * d3d12->shader_preset->history_size);

   for (i = 0; i < d3d12->shader_preset->luts; i++)
      d3d12_release_texture(&d3d12->luts[i]);

   memset(d3d12->luts, 0, sizeof(d3d12->luts));

   free(d3d12->shader_preset);
   d3d12->shader_preset         = NULL;
   d3d12->flags                &= ~(D3D12_ST_FLAG_INIT_HISTORY
                                  | D3D12_ST_FLAG_RESIZE_RTS
                                   );
}

static void d3d12_init_pipeline(
      D3D12Device                         device,
      D3DBlob                             vs_code,
      D3DBlob                             ps_code,
      D3DBlob                             gs_code,
      D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc,
      D3D12PipelineState*                 out)
{
   if (vs_code)
   {
      desc->VS.pShaderBytecode = vs_code->lpVtbl->GetBufferPointer(vs_code);
      desc->VS.BytecodeLength  = vs_code->lpVtbl->GetBufferSize(vs_code);
   }
   else
   {
      desc->VS.pShaderBytecode = NULL;
      desc->VS.BytecodeLength  = 0;
   }

   if (ps_code)
   {
      desc->PS.pShaderBytecode = ps_code->lpVtbl->GetBufferPointer(ps_code);
      desc->PS.BytecodeLength  = ps_code->lpVtbl->GetBufferSize(ps_code);
   }
   else
   {
      desc->PS.pShaderBytecode = NULL;
      desc->PS.BytecodeLength  = 0;
   }

   if (gs_code)
   {
      desc->GS.pShaderBytecode = gs_code->lpVtbl->GetBufferPointer(gs_code);
      desc->GS.BytecodeLength  = gs_code->lpVtbl->GetBufferSize(gs_code);
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

   device->lpVtbl->CreateGraphicsPipelineState(device, desc, uuidof(ID3D12PipelineState), (void**)out);
}

static bool d3d12_gfx_set_shader(void* data, enum rarch_shader_type type, const char* path)
{
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   size_t i;
   d3d12_texture_t* source = NULL;
   d3d12_video_t*   d3d12  = (d3d12_video_t*)data;

   if (!d3d12)
      return false;

   D3D12_GFX_SYNC();
   d3d12_free_shader_preset(d3d12);

   if (string_is_empty(path))
      return true;

   if (type != RARCH_SHADER_SLANG)
   {
      RARCH_WARN("[D3D12]: Only Slang shaders are supported. Falling back to stock.\n");
      return false;
   }

   d3d12->shader_preset = (struct video_shader*)calloc(1, sizeof(*d3d12->shader_preset));

   if (!video_shader_load_preset_into_shader(path, d3d12->shader_preset))
      goto error;

   source = &d3d12->frame.texture[0];
   for (i = 0; i < d3d12->shader_preset->passes; source = &d3d12->pass[i++].rt)
   {
      unsigned j;
      /* clang-format off */
      semantics_map_t semantics_map = {
         {
            /* Original */
            { &d3d12->frame.texture[0], 0,
               &d3d12->frame.texture[0].size_data, 0},

            /* Source */
            { source, 0,
               &source->size_data, 0},

            /* OriginalHistory */
            { &d3d12->frame.texture[0], sizeof(*d3d12->frame.texture),
               &d3d12->frame.texture[0].size_data, sizeof(*d3d12->frame.texture)},

            /* PassOutput */
            { &d3d12->pass[0].rt, sizeof(*d3d12->pass),
               &d3d12->pass[0].rt.size_data, sizeof(*d3d12->pass)},

            /* PassFeedback */
            { &d3d12->pass[0].feedback, sizeof(*d3d12->pass),
               &d3d12->pass[0].feedback.size_data, sizeof(*d3d12->pass)},

            /* User */
            { &d3d12->luts[0], sizeof(*d3d12->luts),
               &d3d12->luts[0].size_data, sizeof(*d3d12->luts)},
         },
         {
            i == d3d12->shader_preset->passes - 1 ? &d3d12->mvp : &d3d12->identity,                     /* MVP */
            &d3d12->pass[i].rt.size_data,    /* OutputSize */
            &d3d12->frame.output_size,       /* FinalViewportSize */
            &d3d12->pass[i].frame_count,     /* FrameCount */
            &d3d12->pass[i].frame_direction, /* FrameDirection */
            &d3d12->pass[i].frame_time_delta,/* FrameTimeDelta */
            &d3d12->pass[i].original_fps,    /* OriginalFPS */
            &d3d12->pass[i].rotation,        /* Rotation */
            &d3d12->pass[i].core_aspect,     /* OriginalAspect */
            &d3d12->pass[i].core_aspect_rot, /* OriginalAspectRotated */
            &d3d12->pass[i].total_subframes, /* TotalSubFrames */
            &d3d12->pass[i].current_subframe,/* CurrentSubFrame */
         }
      };
      /* clang-format on */

      if (!slang_process(
               d3d12->shader_preset, i, RARCH_SHADER_HLSL, 50, &semantics_map,
               &d3d12->pass[i].semantics))
         goto error;

      {
         D3DBlob                            vs_code = NULL;
         D3DBlob                            ps_code = NULL;
         D3D12_GRAPHICS_PIPELINE_STATE_DESC desc    = { d3d12->desc.sl_rootSignature };

         static const D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, position),
              D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, texcoord),
              D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         };
         char _path[PATH_MAX_LENGTH];
         const char *slang_path = d3d12->shader_preset->pass[i].source.path;
         const char *vs_src     = d3d12->shader_preset->pass[i].source.string.vertex;
         const char *ps_src     = d3d12->shader_preset->pass[i].source.string.fragment;
         size_t _len            = strlcpy(_path, slang_path, sizeof(_path));
         strlcpy(_path + _len, ".vs.hlsl", sizeof(_path) - _len);
         if (!d3d_compile(vs_src, 0, _path, "main", "vs_5_0", &vs_code)){ }

         strlcpy(_path + _len, ".ps.hlsl", sizeof(_path) - _len);
         if (!d3d_compile(ps_src, 0, _path, "main", "ps_5_0", &ps_code)){ }

         desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
         if (i == d3d12->shader_preset->passes - 1)
            desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
         else
            desc.RTVFormats[0] = glslang_format_to_dxgi(d3d12->pass[i].semantics.format);

         desc.PrimitiveTopologyType          = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
         desc.InputLayout.pInputElementDescs = inputElementDesc;
         desc.InputLayout.NumElements        = countof(inputElementDesc);

         d3d12_init_pipeline(
                   d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pass[i].pipe);

         free(d3d12->shader_preset->pass[i].source.string.vertex);
         free(d3d12->shader_preset->pass[i].source.string.fragment);

         d3d12->shader_preset->pass[i].source.string.vertex   = NULL;
         d3d12->shader_preset->pass[i].source.string.fragment = NULL;

         Release(vs_code);
         Release(ps_code);

         if (!d3d12->pass[i].pipe)
            goto error;

#ifdef HAVE_DXGI_HDR
         d3d12->pass[i].rt.rt_view.ptr =
            d3d12->desc.rtv_heap.cpu.ptr         +
            (countof(d3d12->chain.renderTargets) + 1 + (2 * i))
            * d3d12->desc.rtv_heap.stride;
#else
         d3d12->pass[i].rt.rt_view.ptr =
            d3d12->desc.rtv_heap.cpu.ptr         +
            (countof(d3d12->chain.renderTargets) + (2 * i))
            * d3d12->desc.rtv_heap.stride;
#endif

         d3d12->pass[i].feedback.rt_view.ptr = d3d12->pass[i].rt.rt_view.ptr + d3d12->desc.rtv_heap.stride;

         d3d12->pass[i].textures.ptr =
               d3d12->desc.srv_heap.gpu.ptr + i * SLANG_NUM_SEMANTICS * d3d12->desc.srv_heap.stride;
         d3d12->pass[i].samplers.ptr = d3d12->desc.sampler_heap.gpu.ptr +
                                       i * SLANG_NUM_SEMANTICS * d3d12->desc.sampler_heap.stride;
      }

      for (j = 0; j < SLANG_CBUFFER_MAX; j++)
      {
         if (!d3d12->pass[i].semantics.cbuffers[j].size)
            continue;

         d3d12->pass[i].buffer_view[j].SizeInBytes    = d3d12->pass[i].semantics.cbuffers[j].size;
         d3d12->pass[i].buffer_view[j].BufferLocation = d3d12_create_buffer(
               d3d12->device, d3d12->pass[i].buffer_view[j].SizeInBytes,
               &d3d12->pass[i].buffers[j]);
      }
   }

#ifdef HAVE_DXGI_HDR
   if (d3d12->flags & D3D12_ST_FLAG_HDR_ENABLE)
   {
      if (d3d12->shader_preset && d3d12->shader_preset->passes && (d3d12->pass[d3d12->shader_preset->passes - 1].semantics.format == SLANG_FORMAT_A2B10G10R10_UNORM_PACK32))
      {
         /* If the last shader pass uses a RGB10A2 back buffer and hdr has been enabled assume we want to skip the inverse tonemapper and hdr10 conversion */
         d3d12_set_hdr_inverse_tonemap(d3d12, false);
         d3d12_set_hdr10(d3d12, false);
         d3d12->flags |= D3D12_ST_FLAG_RESIZE_CHAIN;
      }
      else if (d3d12->shader_preset && d3d12->shader_preset->passes && (d3d12->pass[d3d12->shader_preset->passes - 1].semantics.format == SLANG_FORMAT_R16G16B16A16_SFLOAT))
      {
         /* If the last shader pass uses a RGBA16 back buffer and hdr has been enabled assume we want to skip the inverse tonemapper */
         d3d12_set_hdr_inverse_tonemap(d3d12, false);
         d3d12_set_hdr10(d3d12, true);
         d3d12->flags |= D3D12_ST_FLAG_RESIZE_CHAIN;
      }
      else
      {
         d3d12_set_hdr_inverse_tonemap(d3d12, true);
         d3d12_set_hdr10(d3d12, true);
      }
   }
#endif /* HAVE_DXGI_HDR */

   for (i = 0; i < d3d12->shader_preset->luts; i++)
   {
      struct texture_image image;
      image.pixels               = NULL;
      image.width                = 0;
      image.height               = 0;
      image.supports_rgba        = true;

      if (!image_texture_load(&image, d3d12->shader_preset->lut[i].path))
         goto error;

      d3d12->luts[i].desc.Width  = image.width;
      d3d12->luts[i].desc.Height = image.height;
      d3d12->luts[i].desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      d3d12->luts[i].srv_heap    = &d3d12->desc.srv_heap;

      if (d3d12->shader_preset->lut[i].mipmap)
         d3d12->luts[i].desc.MipLevels = UINT16_MAX;

      d3d12_release_texture(&d3d12->luts[i]);
      d3d12_init_texture(d3d12->device, &d3d12->luts[i]);

      if (d3d12->luts[i].upload_buffer)
         d3d12_update_texture(
               image.width, image.height, 0, DXGI_FORMAT_R8G8B8A8_UNORM, image.pixels,
               &d3d12->luts[i]);

      image_texture_free(&image);
   }

   d3d12->flags |= D3D12_ST_FLAG_INIT_HISTORY | D3D12_ST_FLAG_RESIZE_RTS;

   return true;

error:
   d3d12_free_shader_preset(d3d12);
#endif
   return false;
}

static bool d3d12_gfx_init_pipelines(d3d12_video_t* d3d12)
{
   D3DBlob                            vs_code = NULL;
   D3DBlob                            ps_code = NULL;
   D3DBlob                            gs_code = NULL;
   D3DBlob                            cs_code = NULL;
   settings_t                  *     settings = config_get_ptr();
   D3D12_GRAPHICS_PIPELINE_STATE_DESC desc    = { d3d12->desc.rootSignature };

   desc.BlendState.RenderTarget[0] = d3d12_blend_disable_desc;
#ifdef HAVE_DXGI_HDR
   desc.RTVFormats[0]              = DXGI_FORMAT_R10G10B10A2_UNORM;

   {
      static const char shader[] =
#include "d3d_shaders/hdr_sm5.hlsl.h"
            ;

      static const D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, position),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, texcoord),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d12_vertex_t, color),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      };

      if (!d3d_compile(shader, sizeof(shader), NULL, "VSMain", "vs_5_0", &vs_code))
         goto error;
      if (!d3d_compile(shader, sizeof(shader), NULL, "PSMain", "ps_5_0", &ps_code))
         goto error;

      desc.PrimitiveTopologyType          = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
      desc.InputLayout.pInputElementDescs = inputElementDesc;
      desc.InputLayout.NumElements        = countof(inputElementDesc);

      d3d12_init_pipeline(
               d3d12->device, vs_code, ps_code, NULL, &desc,
               &d3d12->pipes[VIDEO_SHADER_STOCK_HDR]);

      Release(vs_code);
      Release(ps_code);
      vs_code = NULL;
      ps_code = NULL;
   }
#endif

   desc.BlendState.RenderTarget[0] = d3d12_blend_enable_desc;
   desc.RTVFormats[0]              = DXGI_FORMAT_R8G8B8A8_UNORM;

   {
      static const char shader[] =
#include "d3d_shaders/opaque_sm5.hlsl.h"
            ;

      static const D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, position),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, texcoord),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d12_vertex_t, color),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      };

      if (!d3d_compile(shader, sizeof(shader), NULL, "VSMain", "vs_5_0", &vs_code))
         goto error;
      if (!d3d_compile(shader, sizeof(shader), NULL, "PSMain", "ps_5_0", &ps_code))
         goto error;

      desc.PrimitiveTopologyType          = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
      desc.InputLayout.pInputElementDescs = inputElementDesc;
      desc.InputLayout.NumElements        = countof(inputElementDesc);

      d3d12_init_pipeline(
               d3d12->device, vs_code, ps_code, NULL, &desc,
               &d3d12->pipes[VIDEO_SHADER_STOCK_BLEND]);

      desc.BlendState.RenderTarget[0].BlendEnable = false;
      d3d12_init_pipeline(
               d3d12->device, vs_code, ps_code, NULL, &desc,
               &d3d12->pipes[VIDEO_SHADER_STOCK_NOBLEND]);


      Release(vs_code);
      Release(ps_code);
      vs_code = NULL;
      ps_code = NULL;
   }
   {
      static const char shader[] =
#include "d3d_shaders/sprite_sm4.hlsl.h"
            ;

      D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
         { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d12_sprite_t, pos),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(d3d12_sprite_t, coords),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d12_sprite_t, colors[0]),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "COLOR", 1, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d12_sprite_t, colors[1]),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "COLOR", 2, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d12_sprite_t, colors[2]),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "COLOR", 3, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(d3d12_sprite_t, colors[3]),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         { "PARAMS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_sprite_t, params),
           D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      };

      if (!d3d_compile(shader, sizeof(shader), NULL, "VSMain", "vs_5_0", &vs_code))
         goto error;
      if (!d3d_compile(shader, sizeof(shader), NULL, "PSMain", "ps_5_0", &ps_code))
         goto error;
      if (!d3d_compile(shader, sizeof(shader), NULL, "GSMain", "gs_5_0", &gs_code))
         goto error;

      desc.BlendState.RenderTarget[0].BlendEnable = false;
      desc.PrimitiveTopologyType                  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
      desc.InputLayout.pInputElementDescs         = inputElementDesc;
      desc.InputLayout.NumElements                = countof(inputElementDesc);

      d3d12_init_pipeline(
                d3d12->device, vs_code, ps_code, gs_code, &desc, &d3d12->sprites.pipe_noblend);

      desc.BlendState.RenderTarget[0].BlendEnable = true;
      d3d12_init_pipeline(
                d3d12->device, vs_code, ps_code, gs_code, &desc, &d3d12->sprites.pipe_blend);

      Release(ps_code);
      ps_code = NULL;

      if (!d3d_compile(shader, sizeof(shader), NULL, "PSMainA8", "ps_5_0", &ps_code))
         goto error;

      d3d12_init_pipeline(
                d3d12->device, vs_code, ps_code, gs_code, &desc, &d3d12->sprites.pipe_font);

      Release(vs_code);
      Release(ps_code);
      Release(gs_code);
      vs_code = NULL;
      ps_code = NULL;
      gs_code = NULL;
   }

   if (string_is_equal(settings->arrays.menu_driver, "xmb"))
   {
      {
         static const char ribbon[] =
#include "d3d_shaders/ribbon_sm4.hlsl.h"
            ;
         static const char ribbon_simple[] =
#include "d3d_shaders/ribbon_simple_sm4.hlsl.h"
            ;

         D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,
               D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         };

         desc.BlendState.RenderTarget[0].SrcBlend  = D3D12_BLEND_DEST_COLOR;
         desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
         desc.PrimitiveTopologyType                = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
         desc.InputLayout.pInputElementDescs       = inputElementDesc;
         desc.InputLayout.NumElements              = countof(inputElementDesc);

         if (!d3d_compile(ribbon, sizeof(ribbon), NULL, "VSMain", "vs_5_0", &vs_code))
            goto error;
         if (!d3d_compile(ribbon, sizeof(ribbon), NULL, "PSMain", "ps_5_0", &ps_code))
            goto error;

         d3d12_init_pipeline(
                  d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pipes[VIDEO_SHADER_MENU]);

         Release(vs_code);
         Release(ps_code);
         vs_code = NULL;
         ps_code = NULL;

         if (!d3d_compile(ribbon_simple, sizeof(ribbon_simple), NULL, "VSMain", "vs_5_0", &vs_code))
            goto error;
         if (!d3d_compile(ribbon_simple, sizeof(ribbon_simple), NULL, "PSMain", "ps_5_0", &ps_code))
            goto error;

         d3d12_init_pipeline(
                  d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pipes[VIDEO_SHADER_MENU_2]);

         Release(vs_code);
         Release(ps_code);
         vs_code = NULL;
         ps_code = NULL;
      }

      {
         static const char simple_snow[] =
#include "d3d_shaders/simple_snow_sm4.hlsl.h"
            ;
         static const char snow[] =
#include "d3d_shaders/snow_sm4.hlsl.h"
            ;
         static const char bokeh[] =
#include "d3d_shaders/bokeh_sm4.hlsl.h"
            ;
         static const char snowflake[] =
#include "d3d_shaders/snowflake_sm4.hlsl.h"
            ;

         D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, position),
               D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(d3d12_vertex_t, texcoord),
               D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
         };

         desc.PrimitiveTopologyType          = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
         desc.InputLayout.pInputElementDescs = inputElementDesc;
         desc.InputLayout.NumElements        = countof(inputElementDesc);

         if (!d3d_compile(simple_snow, sizeof(simple_snow), NULL, "VSMain", "vs_5_0", &vs_code))
            goto error;
         if (!d3d_compile(simple_snow, sizeof(simple_snow), NULL, "PSMain", "ps_5_0", &ps_code))
            goto error;

         d3d12_init_pipeline(
                  d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pipes[VIDEO_SHADER_MENU_3]);

         Release(vs_code);
         Release(ps_code);
         vs_code = NULL;
         ps_code = NULL;

         if (!d3d_compile(snow, sizeof(snow), NULL, "VSMain", "vs_5_0", &vs_code))
            goto error;
         if (!d3d_compile(snow, sizeof(snow), NULL, "PSMain", "ps_5_0", &ps_code))
            goto error;

         d3d12_init_pipeline(
                  d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pipes[VIDEO_SHADER_MENU_4]);

         Release(vs_code);
         Release(ps_code);
         vs_code = NULL;
         ps_code = NULL;

         if (!d3d_compile(bokeh, sizeof(bokeh), NULL, "VSMain", "vs_5_0", &vs_code))
            goto error;
         if (!d3d_compile(bokeh, sizeof(bokeh), NULL, "PSMain", "ps_5_0", &ps_code))
            goto error;

         d3d12_init_pipeline(
                  d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pipes[VIDEO_SHADER_MENU_5]);

         Release(vs_code);
         Release(ps_code);
         vs_code = NULL;
         ps_code = NULL;

         if (!d3d_compile(snowflake, sizeof(snowflake), NULL, "VSMain", "vs_5_0", &vs_code))
            goto error;
         if (!d3d_compile(snowflake, sizeof(snowflake), NULL, "PSMain", "ps_5_0", &ps_code))
            goto error;

         d3d12_init_pipeline(
                  d3d12->device, vs_code, ps_code, NULL, &desc, &d3d12->pipes[VIDEO_SHADER_MENU_6]);

         Release(vs_code);
         Release(ps_code);
         vs_code = NULL;
         ps_code = NULL;
      }
   }

   {
      static const char shader[] =
#include "d3d_shaders/mipmapgen_sm5.h"
            ;
      D3D12_COMPUTE_PIPELINE_STATE_DESC desc = { d3d12->desc.cs_rootSignature };
      if (!d3d_compile(shader, sizeof(shader), NULL, "CSMain", "cs_5_0", &cs_code))
         goto error;

      desc.CS.pShaderBytecode = cs_code->lpVtbl->GetBufferPointer(cs_code);
      desc.CS.BytecodeLength  = cs_code->lpVtbl->GetBufferSize(cs_code);
      if (FAILED(d3d12->device->lpVtbl->CreateComputePipelineState(d3d12->device, &desc,
                  uuidof(ID3D12PipelineState), (void**)&d3d12->mipmapgen_pipe)))
         Release(cs_code);
      cs_code = NULL;
   }

   return true;

error:
   Release(vs_code);
   Release(ps_code);
   Release(gs_code);
   Release(cs_code);
   return false;
}

static void d3d12_gfx_free(void* data)
{
   unsigned       i;
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   D3D12_GFX_SYNC();

   if (d3d12->flags & D3D12_ST_FLAG_WAITABLE_SWAPCHAINS)
      CloseHandle(d3d12->chain.frameLatencyWaitableObject);

   font_driver_free_osd();

#ifdef HAVE_OVERLAY
   d3d12_free_overlays(d3d12);
#endif

   d3d12_free_shader_preset(d3d12);

   Release(d3d12->sprites.vbo);
   Release(d3d12->menu_pipeline_vbo);

#ifdef HAVE_DXGI_HDR
   Release(d3d12->hdr.ubo);
#endif

   Release(d3d12->frame.ubo);
   Release(d3d12->frame.vbo);
   Release(d3d12->frame.texture[0].handle);
   Release(d3d12->frame.texture[0].upload_buffer);
   Release(d3d12->menu.vbo);
   Release(d3d12->menu.texture.handle);
   Release(d3d12->menu.texture.upload_buffer);

#ifdef HAVE_DXGI_HDR
   d3d12_release_texture(&d3d12->chain.back_buffer);
   d3d12->chain.back_buffer.handle = NULL;
#endif
   free(d3d12->desc.sampler_heap.map);
   free(d3d12->desc.srv_heap.map);
   free(d3d12->desc.rtv_heap.map);
   Release(d3d12->desc.sampler_heap.handle);
   Release(d3d12->desc.srv_heap.handle);
   Release(d3d12->desc.rtv_heap.handle);

   Release(d3d12->desc.cs_rootSignature);
   Release(d3d12->desc.sl_rootSignature);
   Release(d3d12->desc.rootSignature);

   Release(d3d12->ubo);

   for (i = 0; i < GFX_MAX_SHADERS; i++)
      Release(d3d12->pipes[i]);

   Release(d3d12->mipmapgen_pipe);
   Release(d3d12->sprites.pipe_blend);
   Release(d3d12->sprites.pipe_noblend);
   Release(d3d12->sprites.pipe_font);

   Release(d3d12->queue.fence);
   Release(d3d12->queue.cmd);
   Release(d3d12->queue.allocator);
   Release(d3d12->queue.handle);

   Release(d3d12->chain.renderTargets[0]);
   Release(d3d12->chain.renderTargets[1]);

#if 0
   /* Releasing this will crash eventually (?!) */
   Release(d3d12->chain.handle);
#endif

   Release(d3d12->factory);
   Release(d3d12->device);
   Release(d3d12->adapter);

   for (i = 0; i < D3D12_MAX_GPU_COUNT; i++)
   {
      if (d3d12->adapters[i])
      {
         Release(d3d12->adapters[i]);
         d3d12->adapters[i] = NULL;
      }
   }

#ifdef HAVE_DXGI_HDR
   video_driver_unset_hdr_support();
#endif

#ifdef HAVE_MONITOR
   win32_monitor_from_window();
#endif
#ifdef HAVE_WINDOW
   win32_destroy_window();
#endif

   free(d3d12);
}

static bool d3d12_init_swapchain(d3d12_video_t* d3d12,
      int width, int height, void* corewindow)
{
   unsigned i;
   HRESULT hr;
   HWND hwnd                               = (HWND)corewindow;
#ifdef __WINRT__
   DXGI_SWAP_CHAIN_DESC1 desc              = {{0}};
#else
   DXGI_SWAP_CHAIN_DESC desc               = {{0}};
#endif
#ifdef HAVE_DXGI_HDR
   DXGI_COLOR_SPACE_TYPE color_space;
#endif

#ifdef HAVE_DXGI_HDR
   d3d12->chain.formats[DXGI_SWAPCHAIN_BIT_DEPTH_8]    = DXGI_FORMAT_R8G8B8A8_UNORM;
   d3d12->chain.formats[DXGI_SWAPCHAIN_BIT_DEPTH_10]   = DXGI_FORMAT_R10G10B10A2_UNORM;
   d3d12->chain.formats[DXGI_SWAPCHAIN_BIT_DEPTH_16]   = DXGI_FORMAT_R16G16B16A16_UNORM;

   if (dxgi_check_display_hdr_support(d3d12->factory, hwnd))
      d3d12->flags          |=  D3D12_ST_FLAG_HDR_SUPPORT;
   else
      d3d12->flags          &= ~D3D12_ST_FLAG_HDR_SUPPORT;

   if (!(d3d12->flags & D3D12_ST_FLAG_HDR_SUPPORT))
      d3d12->flags          &= ~D3D12_ST_FLAG_HDR_ENABLE;

   d3d12->chain.bit_depth    = (d3d12->flags & D3D12_ST_FLAG_HDR_ENABLE)
      ? DXGI_SWAPCHAIN_BIT_DEPTH_10 : DXGI_SWAPCHAIN_BIT_DEPTH_8;
#endif

   desc.BufferCount          = countof(d3d12->chain.renderTargets);
   desc.BufferUsage          = DXGI_USAGE_RENDER_TARGET_OUTPUT;
#ifdef __WINRT__
   desc.Width                = width;
   desc.Height               = height;
#else
   desc.BufferDesc.Width     = width;
   desc.BufferDesc.Height    = height;
   desc.BufferDesc.RefreshRate.Numerator   = 0;
   desc.BufferDesc.RefreshRate.Denominator = 1;
#endif

#ifdef HAVE_DXGI_HDR
#ifdef __WINRT__
   desc.Format               = d3d12->chain.formats[d3d12->chain.bit_depth];
#else
   desc.BufferDesc.Format    = d3d12->chain.formats[d3d12->chain.bit_depth];
#endif
#else
#ifdef __WINRT__
   desc.Format               = DXGI_FORMAT_R8G8B8A8_UNORM;
#else
   desc.BufferDesc.Format    = DXGI_FORMAT_R8G8B8A8_UNORM;
#endif
#endif

   desc.SampleDesc.Count     = 1;
   desc.SampleDesc.Quality   = 0;
#ifdef HAVE_WINDOW
   desc.OutputWindow         = hwnd;
   desc.Windowed             = TRUE;
#endif
   desc.SwapEffect           = DXGI_SWAP_EFFECT_FLIP_DISCARD;
   desc.Flags                = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
   if (d3d12->flags & D3D12_ST_FLAG_WAITABLE_SWAPCHAINS)
      desc.Flags            |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

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

   if (     (d3d12->flags & D3D12_ST_FLAG_WAITABLE_SWAPCHAINS)
         && (d3d12->chain.frameLatencyWaitableObject = DXGIGetFrameLatencyWaitableObject(d3d12->chain.handle)))
   {
      settings_t* settings = config_get_ptr();
      UINT max_latency     = settings->uints.video_max_frame_latency;
      UINT cur_latency     = 0;

      if (max_latency == 0)
      {
         d3d12->flags     |=  D3D12_ST_FLAG_WAIT_FOR_VBLANK;
         max_latency       = 1;
      }
      else
         d3d12->flags     &= ~D3D12_ST_FLAG_WAIT_FOR_VBLANK;

      DXGISetMaximumFrameLatency(d3d12->chain.handle, max_latency);
      DXGIGetMaximumFrameLatency(d3d12->chain.handle, &cur_latency);
      RARCH_LOG("[D3D12]: Requesting %u maximum frame latency, using %u%s.\n",
            settings->uints.video_max_frame_latency,
            cur_latency,
            (d3d12->flags & D3D12_ST_FLAG_WAIT_FOR_VBLANK) ? " with WaitForVBlank" : "");
   }

#ifdef HAVE_WINDOW
   DXGIMakeWindowAssociation(d3d12->factory, hwnd, DXGI_MWA_NO_ALT_ENTER);
#endif

#ifdef HAVE_DXGI_HDR
   /* Check display HDR support and
      initialize ST.2084 support to match
      the display's support. */
   color_space                 =
        (d3d12->flags & D3D12_ST_FLAG_HDR_ENABLE)
      ? DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
      : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

   dxgi_swapchain_color_space(d3d12->chain.handle,
         &d3d12->chain.color_space, color_space);
   dxgi_set_hdr_metadata(
         d3d12->chain.handle,
         (d3d12->flags & D3D12_ST_FLAG_HDR_SUPPORT) ? true : false,
         d3d12->chain.bit_depth,
         d3d12->chain.color_space,
         d3d12->hdr.max_output_nits,
         d3d12->hdr.min_output_nits,
         d3d12->hdr.max_cll,
         d3d12->hdr.max_fall);
#endif

   d3d12->chain.frame_index = DXGIGetCurrentBackBufferIndex(d3d12->chain.handle);

   for (i = 0; i < countof(d3d12->chain.renderTargets); i++)
   {
      d3d12->chain.handle->lpVtbl->GetBuffer(d3d12->chain.handle, i, uuidof(ID3D12Resource), (void**)&d3d12->chain.renderTargets[i]);
      d3d12->device->lpVtbl->CreateRenderTargetView(
            d3d12->device, (ID3D12Resource*)d3d12->chain.renderTargets[i], NULL, d3d12->chain.desc_handles[i]);
   }

#ifdef HAVE_DXGI_HDR
   memset(&d3d12->chain.back_buffer,
         0, sizeof(d3d12->chain.back_buffer));
   d3d12->chain.back_buffer.desc.Width             = width;
   d3d12->chain.back_buffer.desc.Height            = height;
   d3d12->chain.back_buffer.desc.Format            =
      d3d12->shader_preset && d3d12->shader_preset->passes ? glslang_format_to_dxgi(d3d12->pass[d3d12->shader_preset->passes - 1].semantics.format) : DXGI_FORMAT_R8G8B8A8_UNORM;
   d3d12->chain.back_buffer.desc.Flags             =
      D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
   d3d12->chain.back_buffer.srv_heap               =
      &d3d12->desc.srv_heap;
   d3d12->chain.back_buffer.rt_view.ptr            =
        d3d12->desc.rtv_heap.cpu.ptr
      + (countof(d3d12->chain.renderTargets))
      * d3d12->desc.rtv_heap.stride;
   d3d12_release_texture(&d3d12->chain.back_buffer);
   d3d12_init_texture(d3d12->device, &d3d12->chain.back_buffer);
#endif

   d3d12->chain.viewport.Width                     = width;
   d3d12->chain.viewport.Height                    = height;

   d3d12->chain.scissorRect.left                   = d3d12->vp.x;
   d3d12->chain.scissorRect.top                    = d3d12->vp.y;
   d3d12->chain.scissorRect.right                  = d3d12->vp.x + width;
   d3d12->chain.scissorRect.bottom                 = d3d12->vp.y + height;

   return true;
}

static void d3d12_init_base(d3d12_video_t* d3d12)
{
   int i = 0;
   DXGIAdapter adapter = NULL;
#ifdef DEBUG
   if (SUCCEEDED(D3D12GetDebugInterface(uuidof(ID3D12Debug), (void**)&d3d12->debugController)))
      d3d12->debugController->lpVtbl->EnableDebugLayer(d3d12->debugController);
#endif

#ifdef __WINRT__
   DXGICreateFactory2(&d3d12->factory);
#else
   DXGICreateFactory1(&d3d12->factory);
#endif
   {
      settings_t *settings = config_get_ptr();
      int gpu_index        = settings->ints.d3d12_gpu_index;

      if (d3d12->gpu_list)
         string_list_free(d3d12->gpu_list);

      d3d12->gpu_list = string_list_new();

      for (;;)
      {
         char str[128];
         union string_list_elem_attr attr;
         DXGI_ADAPTER_DESC desc = {0};

         attr.i = 0;
         str[0] = '\0';

#ifdef __WINRT__
         if (FAILED(DXGIEnumAdapters2(d3d12->factory, i, &adapter)))
            break;
#else
         if (FAILED(DXGIEnumAdapters1(d3d12->factory, i, &adapter)))
            break;
#endif
         adapter->lpVtbl->GetDesc(adapter, &desc);

         utf16_to_char_string((const uint16_t*)desc.Description, str, sizeof(str));

         RARCH_LOG("[D3D12]: Found GPU at index %d: \"%s\".\n", i, str);

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

      if (0 <= gpu_index && gpu_index <= i && gpu_index < D3D12_MAX_GPU_COUNT)
      {
         d3d12->adapter = d3d12->adapters[gpu_index];
         AddRef(d3d12->adapter);
         RARCH_LOG("[D3D12]: Using GPU index %d.\n", gpu_index);
      }
      else
      {
         RARCH_WARN("[D3D12]: Invalid GPU index %d, using first device found.\n", gpu_index);
         d3d12->adapter = d3d12->adapters[0];
         AddRef(d3d12->adapter);
      }

      if (!SUCCEEDED(D3D12CreateDevice((IUnknown*)d3d12->adapter, D3D_FEATURE_LEVEL_11_0, uuidof(ID3D12Device), (void**)&d3d12->device)))
         RARCH_WARN("[D3D12]: Could not create D3D12 device.\n");
   }

#ifdef DEVICE_DEBUG
#ifdef DEBUG
   if (d3d12->device)
   {
      if (SUCCEEDED(d3d12->device->lpVtbl->QueryInterface(d3d12->device, uuidof(ID3D12DebugDevice), (void*)&d3d12->debug_device)))
         RARCH_WARN("[D3D12]: Could not create D3D12 debug device.\n");

      if (SUCCEEDED(d3d12->device->lpVtbl->QueryInterface(d3d12->device, uuidof(ID3D12InfoQueue), (void*)&d3d12->info_queue)))
      {
#if 0
         d3d12->info_queue->lpVtbl->SetBreakOnSeverity(d3d12->info_queue, D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
#endif
         d3d12->info_queue->lpVtbl->SetBreakOnSeverity(d3d12->info_queue, D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
#if 0
         d3d12->info_queue->lpVtbl->SetBreakOnSeverity(d3d12->info_queue, D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
#endif
      }
   }

   if (!SUCCEEDED(D3D12GetDebugInterface(uuidof(ID3D12DeviceRemovedExtendedDataSettings), (void*)&d3d12->device_removed_info)))
      RARCH_WARN("[D3D12]: Could not create D3D12 device removed info.\n");

   /* Turn on AutoBreadcrumbs and Page Fault reporting */
   d3d12->device_removed_info->lpVtbl->SetAutoBreadcrumbsEnablement(d3d12->device_removed_info, D3D12_DRED_ENABLEMENT_FORCED_ON);
   d3d12->device_removed_info->lpVtbl->SetPageFaultEnablement(d3d12->device_removed_info, D3D12_DRED_ENABLEMENT_FORCED_ON);
   d3d12->device_removed_info->lpVtbl->SetWatsonDumpEnablement(d3d12->device_removed_info, D3D12_DRED_ENABLEMENT_FORCED_ON);
#endif /* DEBUG */
#endif /* DEVICE_DEBUG */
}

static void d3d12_init_descriptor_heap(D3D12Device device, d3d12_descriptor_heap_t* out)
{
   device->lpVtbl->CreateDescriptorHeap(device, &out->desc, uuidof(ID3D12DescriptorHeap), (void**)&out->handle);
   out->cpu    = D3D12GetCPUDescriptorHandleForHeapStart(out->handle);
   out->gpu    = D3D12GetGPUDescriptorHandleForHeapStart(out->handle);
   out->stride = device->lpVtbl->GetDescriptorHandleIncrementSize(device, out->desc.Type);
   out->map    = (bool*)calloc(out->desc.NumDescriptors, sizeof(bool));
}

static bool d3d12_create_root_signature(
      D3D12Device device, D3D12_ROOT_SIGNATURE_DESC* desc, D3D12RootSignature* out)
{
   D3DBlob signature, error;
   D3D12SerializeRootSignature(desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);

   if (error)
   {
      RARCH_ERR(
            "[D3D12]: CreateRootSignature failed : %s\n", (const char*)error->lpVtbl->GetBufferPointer(error));
      Release(error);
      return false;
   }

   device->lpVtbl->CreateRootSignature(
         device, 0,
         signature->lpVtbl->GetBufferPointer(signature),
         signature->lpVtbl->GetBufferSize(signature),
         uuidof(ID3D12RootSignature),
         (void**)out);
   Release(signature);

   return true;
}


static void d3d12_init_descriptors(d3d12_video_t* d3d12)
{
   size_t i;
   int j;
   D3D12_ROOT_SIGNATURE_DESC desc;
   D3D12_ROOT_PARAMETER      root_params[ROOT_ID_MAX];
   D3D12_ROOT_PARAMETER      cs_root_params[CS_ROOT_ID_MAX];
   D3D12_DESCRIPTOR_RANGE    srv_tbl[1]            = { { D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1 } };
   D3D12_DESCRIPTOR_RANGE    uav_tbl[1]            = { { D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1 } };
   D3D12_DESCRIPTOR_RANGE    sampler_tbl[1]        = { { D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1 } };
   D3D12_STATIC_SAMPLER_DESC static_sampler        = { D3D12_FILTER_MIN_MAG_MIP_POINT };

   root_params[ROOT_ID_TEXTURE_T].ParameterType    = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
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

   root_params[ROOT_ID_PC].ParameterType              = D3D12_ROOT_PARAMETER_TYPE_CBV;
   root_params[ROOT_ID_PC].Descriptor.RegisterSpace   = 0;
   root_params[ROOT_ID_PC].Descriptor.ShaderRegister  = 1;
   root_params[ROOT_ID_PC].ShaderVisibility           = D3D12_SHADER_VISIBILITY_ALL;

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

   desc.NumParameters     = countof(cs_root_params);
   desc.pParameters       = cs_root_params;
   desc.NumStaticSamplers = 1;
   desc.pStaticSamplers   = &static_sampler;
   desc.Flags             =
                  D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS
                | D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

   d3d12_create_root_signature(d3d12->device, &desc, &d3d12->desc.cs_rootSignature);

   d3d12->desc.rtv_heap.desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
   d3d12->desc.rtv_heap.desc.NumDescriptors = countof(d3d12->chain.renderTargets) + GFX_MAX_SHADERS * 2;
   d3d12->desc.rtv_heap.desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
   d3d12_init_descriptor_heap(d3d12->device, &d3d12->desc.rtv_heap);

   d3d12->desc.srv_heap.desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
   d3d12->desc.srv_heap.desc.NumDescriptors = SLANG_NUM_BINDINGS * GFX_MAX_SHADERS + 2048;
   d3d12->desc.srv_heap.desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
   d3d12_init_descriptor_heap(d3d12->device, &d3d12->desc.srv_heap);

   d3d12->desc.sampler_heap.desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
   d3d12->desc.sampler_heap.desc.NumDescriptors =
         SLANG_NUM_BINDINGS * GFX_MAX_SHADERS + 2 * RARCH_WRAP_MAX;
   d3d12->desc.sampler_heap.desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
   d3d12_init_descriptor_heap(d3d12->device, &d3d12->desc.sampler_heap);

   for (i = 0; i < countof(d3d12->chain.renderTargets); i++)
      d3d12->chain.desc_handles[i].ptr =
            d3d12->desc.rtv_heap.cpu.ptr + i * d3d12->desc.rtv_heap.stride;

   for (i = 0; i < GFX_MAX_SHADERS; i++)
   {
      d3d12->pass[i].rt.rt_view.ptr       =
            d3d12->desc.rtv_heap.cpu.ptr +
            (countof(d3d12->chain.renderTargets) + (2 * i)) * d3d12->desc.rtv_heap.stride;
      d3d12->pass[i].feedback.rt_view.ptr = d3d12->pass[i].rt.rt_view.ptr + d3d12->desc.rtv_heap.stride;

      d3d12->pass[i].textures.ptr         = d3d12_descriptor_heap_slot_alloc(&d3d12->desc.srv_heap).ptr -
                                    d3d12->desc.srv_heap.cpu.ptr + d3d12->desc.srv_heap.gpu.ptr;
      d3d12->pass[i].samplers.ptr         =
            d3d12_descriptor_heap_slot_alloc(&d3d12->desc.sampler_heap).ptr -
            d3d12->desc.sampler_heap.cpu.ptr + d3d12->desc.sampler_heap.gpu.ptr;

      for (j = 1; j < SLANG_NUM_BINDINGS; j++)
      {
         d3d12_descriptor_heap_slot_alloc(&d3d12->desc.srv_heap);
         d3d12_descriptor_heap_slot_alloc(&d3d12->desc.sampler_heap);
      }
   }
}

static INLINE D3D12_GPU_DESCRIPTOR_HANDLE
              d3d12_create_sampler(D3D12Device device, D3D12_SAMPLER_DESC* desc, d3d12_descriptor_heap_t* heap)
{
   D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle;
   D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle = d3d12_descriptor_heap_slot_alloc(heap);
   gpu_handle.ptr = cpu_handle.ptr - heap->cpu.ptr + heap->gpu.ptr;

   device->lpVtbl->CreateSampler(device, desc, cpu_handle);
   return gpu_handle;
}

static void d3d12_init_samplers(d3d12_video_t* d3d12)
{
   int i;
   D3D12_SAMPLER_DESC desc;

   desc.Filter             = D3D12_FILTER_MIN_MAG_MIP_POINT;
   desc.MipLODBias         = 0.0f;
   desc.MaxAnisotropy      = 1;
   desc.ComparisonFunc     = D3D12_COMPARISON_FUNC_NEVER;
   desc.BorderColor[0]     =
   desc.BorderColor[1]     =
   desc.BorderColor[2]     =
   desc.BorderColor[3]     = 0.0f;
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

      desc.Filter   = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
      d3d12->samplers[RARCH_FILTER_LINEAR][i] =
            d3d12_create_sampler(d3d12->device, &desc, &d3d12->desc.sampler_heap);

      desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
      d3d12->samplers[RARCH_FILTER_NEAREST][i] =
            d3d12_create_sampler(d3d12->device, &desc, &d3d12->desc.sampler_heap);
   }
}

static void d3d12_init_queue(d3d12_video_t* d3d12)
{
   static const D3D12_COMMAND_QUEUE_DESC desc = {
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      D3D12_COMMAND_QUEUE_PRIORITY_NORMAL,
      D3D12_COMMAND_QUEUE_FLAG_NONE,
      0
   };

   d3d12->device->lpVtbl->CreateCommandQueue(
         d3d12->device,
         (D3D12_COMMAND_QUEUE_DESC*)&desc,
         uuidof(ID3D12CommandQueue),
         (void**)&d3d12->queue.handle);
   d3d12->device->lpVtbl->CreateCommandAllocator(
         d3d12->device,
         D3D12_COMMAND_LIST_TYPE_DIRECT,
         uuidof(ID3D12CommandAllocator),
         (void**)&d3d12->queue.allocator);

   d3d12->device->lpVtbl->CreateCommandList(
         d3d12->device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d12->queue.allocator,
         d3d12->pipes[VIDEO_SHADER_STOCK_BLEND],
         uuidof(ID3D12GraphicsCommandList),
         (void**)&d3d12->queue.cmd);

   d3d12->queue.cmd->lpVtbl->Close(d3d12->queue.cmd);
   d3d12->device->lpVtbl->CreateFence(d3d12->device, 0,
         D3D12_FENCE_FLAG_NONE, uuidof(ID3D12Fence), (void**)&d3d12->queue.fence);
   d3d12->queue.fenceValue = 0;
   d3d12->queue.fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

static void d3d12_create_fullscreen_quad_vbo(
      D3D12Device device, D3D12_VERTEX_BUFFER_VIEW* view, D3D12Resource* vbo)
{
   D3D12_RANGE read_range;
   void *vertex_data_begin                = NULL;
   static const d3d12_vertex_t vertices[] = {
      { { 0.0f, 0.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
      { { 0.0f, 1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
      { { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
      { { 1.0f, 1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

      { { -1.0f, -1.0f }, { 0.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
      { { -1.0f,  1.0f }, { 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
      { { 1.0f,  -1.0f }, { 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
      { { 1.0f,   1.0f }, { 1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
   };

   view->SizeInBytes    = sizeof(vertices);
   view->StrideInBytes  = sizeof(*vertices);
   view->BufferLocation = d3d12_create_buffer(device, view->SizeInBytes, vbo);

   read_range.Begin     = 0;
   read_range.End       = 0;

   D3D12Map(*vbo, 0, &read_range, &vertex_data_begin);
   memcpy(vertex_data_begin, vertices, sizeof(vertices));
   D3D12Unmap(*vbo, 0, NULL);
}

static void d3d12_set_hw_render_texture(void* data, ID3D12Resource* texture, DXGI_FORMAT format)
{
    d3d12_video_t* d3d12            = (d3d12_video_t*)data;
    d3d12->hw_render_texture        = texture;
    d3d12->hw_render_texture_format = format;
}

static void *d3d12_gfx_init(const video_info_t* video,
      input_driver_t** input, void** input_data)
{
#ifdef HAVE_MONITOR
   MONITORINFOEX  current_mon;
   HMONITOR       hm_to_use;
   WNDCLASSEX     wndclass = { 0 };
#endif
   settings_t*    settings = config_get_ptr();
   d3d12_video_t* d3d12    = (d3d12_video_t*)calloc(1, sizeof(*d3d12));

   if (!d3d12)
      return NULL;

#ifdef HAVE_WINDOW
   win32_window_reset();
#endif
#ifdef HAVE_MONITOR
   win32_monitor_init();
   wndclass.lpfnWndProc = wnd_proc_d3d_common;
#ifdef HAVE_DINPUT
   if (string_is_equal(settings->arrays.input_driver, "dinput"))
      wndclass.lpfnWndProc = wnd_proc_d3d_dinput;
#endif
#ifdef HAVE_WINRAWINPUT
   if (string_is_equal(settings->arrays.input_driver, "raw"))
      wndclass.lpfnWndProc = wnd_proc_d3d_winraw;
#endif
#ifdef HAVE_WINDOW
   win32_window_init(&wndclass, true, NULL);
#endif

   win32_monitor_info(&current_mon, &hm_to_use, &d3d12->cur_mon_id);
#endif

   d3d12->vp.full_width  = video->width;
   d3d12->vp.full_height = video->height;

#ifdef HAVE_MONITOR
   if (!d3d12->vp.full_width)
      d3d12->vp.full_width = current_mon.rcMonitor.right - current_mon.rcMonitor.left;
   if (!d3d12->vp.full_height)
      d3d12->vp.full_height = current_mon.rcMonitor.bottom - current_mon.rcMonitor.top;
#endif

   if (!win32_set_video_mode(d3d12, d3d12->vp.full_width, d3d12->vp.full_height, video->fullscreen))
   {
      RARCH_ERR("[D3D12]: win32_set_video_mode failed.\n");
      goto error;
   }

#ifdef HAVE_DXGI_HDR
   if (settings->bools.video_hdr_enable)
      d3d12->flags                       |=  D3D12_ST_FLAG_HDR_ENABLE;
   else
      d3d12->flags                       &= ~D3D12_ST_FLAG_HDR_ENABLE;
   d3d12->hdr.max_output_nits             = settings->floats.video_hdr_max_nits;
   d3d12->hdr.min_output_nits             = 0.001f;
   d3d12->hdr.max_cll                     = 0.0f;
   d3d12->hdr.max_fall                    = 0.0f;
#endif

   if (settings->bools.video_waitable_swapchains)
      d3d12->flags |=  D3D12_ST_FLAG_WAITABLE_SWAPCHAINS;
   else
      d3d12->flags &= ~D3D12_ST_FLAG_WAITABLE_SWAPCHAINS;

   d3d_input_driver(settings->arrays.input_driver,
         settings->arrays.input_joypad_driver, input, input_data);

   d3d12_init_base(d3d12);
   d3d12_init_descriptors(d3d12);

   if (!d3d12_gfx_init_pipelines(d3d12))
      goto error;

   d3d12_init_queue(d3d12);

#ifdef __WINRT__
   if (!d3d12_init_swapchain(d3d12, d3d12->vp.full_width, d3d12->vp.full_height, uwp_get_corewindow()))
      goto error;
#else
   if (!d3d12_init_swapchain(d3d12, d3d12->vp.full_width, d3d12->vp.full_height, main_window.hwnd))
      goto error;
#endif

   d3d12_init_samplers(d3d12);
   d3d12_set_filtering(d3d12, 0, video->smooth, video->ctx_scaling);

   d3d12_create_fullscreen_quad_vbo(d3d12->device, &d3d12->frame.vbo_view, &d3d12->frame.vbo);
   d3d12_create_fullscreen_quad_vbo(d3d12->device, &d3d12->menu.vbo_view, &d3d12->menu.vbo);

   d3d12->sprites.capacity                = 16 * 1024;
   d3d12->sprites.vbo_view.SizeInBytes    = sizeof(d3d12_sprite_t) * d3d12->sprites.capacity;
   d3d12->sprites.vbo_view.StrideInBytes  = sizeof(d3d12_sprite_t);
   d3d12->sprites.vbo_view.BufferLocation = d3d12_create_buffer(
         d3d12->device, d3d12->sprites.vbo_view.SizeInBytes, &d3d12->sprites.vbo);

   d3d12->ubo_view.SizeInBytes = sizeof(d3d12_uniform_t);
   d3d12->ubo_view.BufferLocation =
         d3d12_create_buffer(d3d12->device, d3d12->ubo_view.SizeInBytes, &d3d12->ubo);

   d3d12->frame.ubo_view.SizeInBytes = sizeof(d3d12_uniform_t);
   d3d12->frame.ubo_view.BufferLocation =
         d3d12_create_buffer(d3d12->device, d3d12->frame.ubo_view.SizeInBytes, &d3d12->frame.ubo);

   matrix_4x4_ortho(d3d12->mvp_no_rot, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 1.0f);

   d3d12->ubo_values.mvp               = d3d12->mvp_no_rot;
   d3d12->ubo_values.OutputSize.width  = d3d12->chain.viewport.Width;
   d3d12->ubo_values.OutputSize.height = d3d12->chain.viewport.Height;

   {
      math_matrix_4x4* mvp;
      D3D12_RANGE read_range;
      read_range.Begin            = 0;
      read_range.End              = 0;
      D3D12Map(d3d12->ubo, 0, &read_range, (void**)&mvp);
      *mvp = d3d12->mvp_no_rot;
      D3D12Unmap(d3d12->ubo, 0, NULL);
   }

#ifdef HAVE_DXGI_HDR
   d3d12->hdr.ubo_view.SizeInBytes        = sizeof(dxgi_hdr_uniform_t);
   d3d12->hdr.ubo_view.BufferLocation     =
         d3d12_create_buffer(d3d12->device, d3d12->hdr.ubo_view.SizeInBytes, &d3d12->hdr.ubo);

   d3d12->hdr.ubo_values.mvp              = d3d12->mvp_no_rot;
   d3d12->hdr.ubo_values.max_nits         = settings->floats.video_hdr_max_nits;
   d3d12->hdr.ubo_values.paper_white_nits = settings->floats.video_hdr_paper_white_nits;
   d3d12->hdr.ubo_values.contrast         = VIDEO_HDR_MAX_CONTRAST - settings->floats.video_hdr_display_contrast;
   d3d12->hdr.ubo_values.expand_gamut     = settings->bools.video_hdr_expand_gamut;
   d3d12->hdr.ubo_values.inverse_tonemap  = 1.0f;     /* Use this to turn on/off the inverse tonemap */
   d3d12->hdr.ubo_values.hdr10            = 1.0f;     /* Use this to turn on/off the hdr10 */

   {
      dxgi_hdr_uniform_t* mapped_ubo;
      D3D12_RANGE read_range;
      read_range.Begin            = 0;
      read_range.End              = 0;
      D3D12Map(d3d12->hdr.ubo, 0, &read_range, (void**)&mapped_ubo);
      *mapped_ubo = d3d12->hdr.ubo_values;
      D3D12Unmap(d3d12->hdr.ubo, 0, NULL);
   }
#endif

   matrix_4x4_identity(d3d12->identity);

   d3d12_gfx_set_rotation(d3d12, 0);
   video_driver_set_size(d3d12->vp.full_width, d3d12->vp.full_height);
   d3d12->chain.viewport.Width  = d3d12->vp.full_width;
   d3d12->chain.viewport.Height = d3d12->vp.full_height;

   d3d12->flags                |=  D3D12_ST_FLAG_RESIZE_VIEWPORT;

   if (video->force_aspect)
      d3d12->flags             |=  D3D12_ST_FLAG_KEEP_ASPECT;
   else
      d3d12->flags             &= ~D3D12_ST_FLAG_KEEP_ASPECT;

   if (video->vsync)
      d3d12->flags             |=  D3D12_ST_FLAG_VSYNC;
   else
      d3d12->flags             &= ~D3D12_ST_FLAG_VSYNC;

   d3d12->format                = (video->rgb32)
         ? DXGI_FORMAT_B8G8R8X8_UNORM : DXGI_FORMAT_B5G6R5_UNORM;

   d3d12->frame.texture[0].desc.Format = d3d12->format;
   d3d12->frame.texture[0].desc.Width  = 4;
   d3d12->frame.texture[0].desc.Height = 4;
   d3d12->frame.texture[0].srv_heap    = &d3d12->desc.srv_heap;
   d3d12_release_texture(&d3d12->frame.texture[0]);
   d3d12_init_texture(d3d12->device, &d3d12->frame.texture[0]);

   font_driver_init_osd(d3d12,
         video,
         false,
         video->is_threaded,
         FONT_DRIVER_RENDER_D3D12_API);

   {
      d3d12_fake_context.get_flags = d3d12_get_flags;
      d3d12_fake_context.get_metrics = win32_get_metrics;
      video_context_driver_set(&d3d12_fake_context);
      const char *shader_preset   = video_shader_get_current_shader_preset();
      enum rarch_shader_type type = video_shader_parse_type(shader_preset);
      d3d12_gfx_set_shader(d3d12, type, shader_preset);
   }

   if (video_driver_get_hw_context()->context_type  == RETRO_HW_CONTEXT_D3D12)
   {
      d3d12->flags                     |= D3D12_ST_FLAG_HW_IFACE_ENABLE;
      d3d12->hw_iface.interface_type    = RETRO_HW_RENDER_INTERFACE_D3D12;
      d3d12->hw_iface.interface_version = RETRO_HW_RENDER_INTERFACE_D3D12_VERSION;
      d3d12->hw_iface.handle            = d3d12;
      d3d12->hw_iface.device            = d3d12->device;
      d3d12->hw_iface.queue             = d3d12->queue.handle;
      d3d12->hw_iface.required_state    = D3D12_RESOURCE_STATE_COPY_SOURCE;
      d3d12->hw_iface.set_texture       = d3d12_set_hw_render_texture;
      d3d12->hw_iface.D3DCompile        = D3DCompile;
   }

   return d3d12;

error:
   RARCH_ERR("[D3D12]: Failed to init video driver.\n");
   d3d12_gfx_free(d3d12);
   return NULL;
}

static void d3d12_init_history(d3d12_video_t* d3d12, unsigned width, unsigned height)
{
   size_t i;
   /* TODO/FIXME: should we init history to max_width/max_height instead ?
    * to prevent out of memory errors happening several frames later
    * and to reduce memory fragmentation */
   for (i = 0; i < (unsigned)d3d12->shader_preset->history_size + 1; i++)
   {
      d3d12->frame.texture[i].desc.Width     = width;
      d3d12->frame.texture[i].desc.Height    = height;
      d3d12->frame.texture[i].desc.Format    = d3d12->frame.texture[0].desc.Format;
      d3d12->frame.texture[i].desc.MipLevels = d3d12->frame.texture[0].desc.MipLevels;
      d3d12->frame.texture[i].srv_heap       = &d3d12->desc.srv_heap;
      d3d12_release_texture(&d3d12->frame.texture[i]);
      d3d12_init_texture(d3d12->device, &d3d12->frame.texture[i]);
      /* TODO/FIXME: clear texture ?  */
   }
   d3d12->flags              &= ~D3D12_ST_FLAG_INIT_HISTORY;
}

static void d3d12_init_render_targets(d3d12_video_t* d3d12, unsigned width, unsigned height)
{
   size_t i;

   for (i = 0; i < d3d12->shader_preset->passes; i++)
   {
      struct video_shader_pass* pass = &d3d12->shader_preset->pass[i];

      if (pass->fbo.flags & FBO_SCALE_FLAG_VALID)
      {
         switch (pass->fbo.type_x)
         {
            case RARCH_SCALE_INPUT:
               width *= pass->fbo.scale_x;
               break;

            case RARCH_SCALE_VIEWPORT:
               width = d3d12->vp.width * pass->fbo.scale_x;
               break;

            case RARCH_SCALE_ABSOLUTE:
               width = pass->fbo.abs_x;
               break;

            default:
               break;
         }

         if (!width)
            width = d3d12->vp.width;

         switch (pass->fbo.type_y)
         {
            case RARCH_SCALE_INPUT:
               height *= pass->fbo.scale_y;
               break;

            case RARCH_SCALE_VIEWPORT:
               height = d3d12->vp.height * pass->fbo.scale_y;
               break;

            case RARCH_SCALE_ABSOLUTE:
               height = pass->fbo.abs_y;
               break;

            default:
               break;
         }

         if (!height)
            height = d3d12->vp.height;
      }
      else if (i == (d3d12->shader_preset->passes - 1))
      {
         width  = d3d12->vp.width;
         height = d3d12->vp.height;
      }

      RARCH_LOG("[D3D12]: Updating framebuffer size %ux%u.\n", width, height);

      if (i == (d3d12->shader_preset->passes - 1))
      {
         d3d12->pass[i].viewport.TopLeftX    = d3d12->vp.x;
         d3d12->pass[i].viewport.TopLeftY    = d3d12->vp.y;
         d3d12->pass[i].viewport.Width       = width;
         d3d12->pass[i].viewport.Height      = height;
         d3d12->pass[i].viewport.MinDepth    = 0.0f;
         d3d12->pass[i].viewport.MaxDepth    = 1.0f;
         d3d12->pass[i].scissorRect.left     = d3d12->vp.x;
         d3d12->pass[i].scissorRect.top      = d3d12->vp.y;
         d3d12->pass[i].scissorRect.right    = d3d12->vp.x + width;
         d3d12->pass[i].scissorRect.bottom   = d3d12->vp.y + height;
      }
      else
      {
         d3d12->pass[i].viewport.TopLeftX    = 0.0f;
         d3d12->pass[i].viewport.TopLeftY    = 0.0f;
         d3d12->pass[i].viewport.Width       = width;
         d3d12->pass[i].viewport.Height      = height;
         d3d12->pass[i].viewport.MinDepth    = 0.0f;
         d3d12->pass[i].viewport.MaxDepth    = 1.0f;
         d3d12->pass[i].scissorRect.left     = 0.0f;
         d3d12->pass[i].scissorRect.top      = 0.0f;
         d3d12->pass[i].scissorRect.right    = width;
         d3d12->pass[i].scissorRect.bottom   = height;
      }

      if (     (i != (d3d12->shader_preset->passes - 1))
            || (width  != d3d12->vp.width)
            || (height != d3d12->vp.height))
      {
         d3d12->pass[i].rt.desc.Width      = width;
         d3d12->pass[i].rt.desc.Height     = height;
         d3d12->pass[i].rt.desc.Flags      = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
         d3d12->pass[i].rt.srv_heap        = &d3d12->desc.srv_heap;
         d3d12->pass[i].rt.desc.Format = glslang_format_to_dxgi(d3d12->pass[i].semantics.format);
         d3d12_release_texture(&d3d12->pass[i].rt);
         d3d12_init_texture(d3d12->device, &d3d12->pass[i].rt);

         if (pass->feedback)
         {
            d3d12->pass[i].feedback.desc     = d3d12->pass[i].rt.desc;
            d3d12->pass[i].feedback.srv_heap = &d3d12->desc.srv_heap;
            d3d12_release_texture(&d3d12->pass[i].feedback);
            d3d12_init_texture(d3d12->device, &d3d12->pass[i].feedback);
            /* TODO/FIXME: do we need to clear it to black here ? */
         }
      }
      else
      {
         width = retroarch_get_rotation() % 2 ? height : width;
         height = retroarch_get_rotation() % 2 ? width : height;

         d3d12->pass[i].rt.size_data.x = width;
         d3d12->pass[i].rt.size_data.y = height;
         d3d12->pass[i].rt.size_data.z = 1.0f / width;
         d3d12->pass[i].rt.size_data.w = 1.0f / height;
      }

      d3d12->pass[i].sampler = d3d12->samplers[pass->filter][pass->wrap];
   }

   d3d12->flags &= ~D3D12_ST_FLAG_RESIZE_RTS;
}

static void dx12_inject_black_frame(d3d12_video_t* d3d12)
{
   D3D12GraphicsCommandList cmd   = d3d12->queue.cmd;


   D3D12_GFX_SYNC();

   d3d12->queue.allocator->lpVtbl->Reset(d3d12->queue.allocator);
   cmd->lpVtbl->Reset(cmd, d3d12->queue.allocator,
         d3d12->pipes[VIDEO_SHADER_STOCK_BLEND]);

   d3d12->chain.frame_index = DXGIGetCurrentBackBufferIndex(
         d3d12->chain.handle);

   D3D12_RESOURCE_TRANSITION(
         cmd,
         d3d12->chain.renderTargets[d3d12->chain.frame_index],
         D3D12_RESOURCE_STATE_PRESENT,
         D3D12_RESOURCE_STATE_RENDER_TARGET);

   cmd->lpVtbl->OMSetRenderTargets(
         cmd, 1, &d3d12->chain.desc_handles[d3d12->chain.frame_index],
         FALSE, NULL);
   cmd->lpVtbl->ClearRenderTargetView(
         cmd,
         d3d12->chain.desc_handles[d3d12->chain.frame_index],
         d3d12->chain.clearcolor,
         0, NULL);

   D3D12_RESOURCE_TRANSITION(
         cmd,
         d3d12->chain.renderTargets[d3d12->chain.frame_index],
         D3D12_RESOURCE_STATE_RENDER_TARGET,
         D3D12_RESOURCE_STATE_PRESENT);

   cmd->lpVtbl->Close(cmd);
   d3d12->queue.handle->lpVtbl->ExecuteCommandLists(d3d12->queue.handle, 1,
         (ID3D12CommandList* const*)&d3d12->queue.cmd);
   DXGIPresent(d3d12->chain.handle, d3d12->chain.swap_interval, 0);

}

static bool d3d12_gfx_frame(
      void*               data,
      const void*         frame,
      unsigned            width,
      unsigned            height,
      uint64_t            frame_count,
      unsigned            pitch,
      const char*         msg,
      video_frame_info_t* video_info)
{
   unsigned i, k, m;
   d3d12_texture_t* texture       = NULL;
   d3d12_video_t*   d3d12         = (d3d12_video_t*)data;
   bool vsync                     = (d3d12->flags & D3D12_ST_FLAG_VSYNC) ? true : false;
   bool wait_for_vblank           = (d3d12->flags & D3D12_ST_FLAG_WAIT_FOR_VBLANK) ? true : false;
   unsigned present_flags         = (vsync) ? 0 : DXGI_PRESENT_ALLOW_TEARING;
   const char *stat_text          = video_info->stat_text;
   bool statistics_show           = video_info->statistics_show;
   unsigned video_width           = video_info->width;
   unsigned video_height          = video_info->height;
   struct font_params *osd_params = (struct font_params*)
      &video_info->osd_stat_params;
   bool menu_is_alive             = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
   bool overlay_behind_menu       = video_info->overlay_behind_menu;
   unsigned black_frame_insertion = video_info->black_frame_insertion;
   int bfi_light_frames;
   unsigned n;
   bool nonblock_state            = video_info->input_driver_nonblock_state;
   bool runloop_is_slowmotion     = video_info->runloop_is_slowmotion;
   bool runloop_is_paused         = video_info->runloop_is_paused;
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active            = video_info->widgets_active;
#endif
#ifdef HAVE_DXGI_HDR
   bool d3d12_hdr_enable          = false;
   bool video_hdr_enable          = video_info->hdr_enable;
   DXGI_FORMAT back_buffer_format = d3d12->shader_preset && d3d12->shader_preset->passes ? glslang_format_to_dxgi(d3d12->pass[d3d12->shader_preset->passes - 1].semantics.format) : DXGI_FORMAT_R8G8B8A8_UNORM;
   bool use_back_buffer           = back_buffer_format != d3d12->chain.formats[d3d12->chain.bit_depth];
#endif
   D3D12GraphicsCommandList cmd   = d3d12->queue.cmd;

   if (d3d12->flags & D3D12_ST_FLAG_WAITABLE_SWAPCHAINS)
      WaitForSingleObjectEx(
            d3d12->chain.frameLatencyWaitableObject,
            1000,
            true);

   D3D12_GFX_SYNC();

#ifdef HAVE_DXGI_HDR
   d3d12_hdr_enable               = (d3d12->flags & D3D12_ST_FLAG_HDR_ENABLE) ? true : false;
   if (     (d3d12->flags & D3D12_ST_FLAG_RESIZE_CHAIN)
         || (d3d12_hdr_enable != video_hdr_enable))
#else
   if (d3d12->flags & D3D12_ST_FLAG_RESIZE_CHAIN)
#endif
   {
      UINT swapchain_flags        = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

      if (d3d12->flags & D3D12_ST_FLAG_WAITABLE_SWAPCHAINS)
         swapchain_flags         |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

#ifdef HAVE_DXGI_HDR
      if (video_hdr_enable)
         d3d12->flags |=  D3D12_ST_FLAG_HDR_ENABLE;
      else
         d3d12->flags &= ~D3D12_ST_FLAG_HDR_ENABLE;
#endif

      for (i = 0; i < countof(d3d12->chain.renderTargets); i++)
         Release(d3d12->chain.renderTargets[i]);

#ifdef HAVE_DXGI_HDR
      if (d3d12->flags & D3D12_ST_FLAG_HDR_ENABLE)
      {
         d3d12_release_texture(&d3d12->chain.back_buffer);
         d3d12->chain.back_buffer.handle = NULL;
      }

      DXGIResizeBuffers(d3d12->chain.handle,
            countof(d3d12->chain.renderTargets),
            video_width,
            video_height,
            d3d12->chain.formats[d3d12->chain.bit_depth],
            swapchain_flags);
#else
      DXGIResizeBuffers(d3d12->chain.handle,
            0,
            0,
            0,
            DXGI_FORMAT_UNKNOWN,
            swapchain_flags);
#endif

      for (i = 0; i < countof(d3d12->chain.renderTargets); i++)
      {
         d3d12->chain.handle->lpVtbl->GetBuffer(d3d12->chain.handle, i,
               uuidof(ID3D12Resource), (void**)&d3d12->chain.renderTargets[i]);
         d3d12->device->lpVtbl->CreateRenderTargetView(
               d3d12->device, (ID3D12Resource*)d3d12->chain.renderTargets[i],
               NULL, d3d12->chain.desc_handles[i]);
      }

      d3d12->chain.viewport.Width         = video_width;
      d3d12->chain.viewport.Height        = video_height;

      d3d12->chain.scissorRect.left       = 0;
      d3d12->chain.scissorRect.top        = 0;
      d3d12->chain.scissorRect.right      = d3d12->vp.full_width;
      d3d12->chain.scissorRect.bottom     = d3d12->vp.full_height;

      d3d12->ubo_values.OutputSize.width  = d3d12->chain.viewport.Width;
      d3d12->ubo_values.OutputSize.height = d3d12->chain.viewport.Height;

      d3d12->flags                       &= ~D3D12_ST_FLAG_RESIZE_CHAIN;
      d3d12->flags                       |=  D3D12_ST_FLAG_RESIZE_VIEWPORT;

      video_driver_set_size(video_width, video_height);

#ifdef HAVE_DXGI_HDR
#ifdef __WINRT__
      if (dxgi_check_display_hdr_support(d3d12->factory, uwp_get_corewindow()))
         d3d12->flags |=   D3D12_ST_FLAG_HDR_SUPPORT;
      else
         d3d12->flags &= ~(D3D12_ST_FLAG_HDR_SUPPORT
                         | D3D12_ST_FLAG_HDR_ENABLE
                          );
#else
      if (dxgi_check_display_hdr_support(d3d12->factory, main_window.hwnd))
         d3d12->flags |=   D3D12_ST_FLAG_HDR_SUPPORT;
      else
         d3d12->flags &= ~(D3D12_ST_FLAG_HDR_SUPPORT
                         | D3D12_ST_FLAG_HDR_ENABLE
                          );
#endif

      if (d3d12->flags & D3D12_ST_FLAG_HDR_ENABLE)
      {
         memset(&d3d12->chain.back_buffer,
               0, sizeof(d3d12->chain.back_buffer));
         d3d12->chain.back_buffer.desc.Width  = video_width;
         d3d12->chain.back_buffer.desc.Height = video_height;
         d3d12->chain.back_buffer.desc.Format = back_buffer_format;
         d3d12->chain.back_buffer.desc.Flags  =
               D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
         d3d12->chain.back_buffer.srv_heap    = &d3d12->desc.srv_heap;
         d3d12->chain.back_buffer.rt_view.ptr =
               d3d12->desc.rtv_heap.cpu.ptr
               + countof(d3d12->chain.renderTargets)
               * d3d12->desc.rtv_heap.stride;
         d3d12_release_texture(&d3d12->chain.back_buffer);
         d3d12_init_texture(d3d12->device, &d3d12->chain.back_buffer);

         dxgi_swapchain_color_space(d3d12->chain.handle,
               &d3d12->chain.color_space,
               DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);

         d3d12->chain.bit_depth  = DXGI_SWAPCHAIN_BIT_DEPTH_10;
      }
      else
      {
         dxgi_swapchain_color_space(d3d12->chain.handle,
               &d3d12->chain.color_space,
               DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);

         d3d12->chain.bit_depth  = DXGI_SWAPCHAIN_BIT_DEPTH_8;
      }

      dxgi_set_hdr_metadata(
            d3d12->chain.handle,
            (d3d12->flags & D3D12_ST_FLAG_HDR_SUPPORT) ? true : false,
            d3d12->chain.bit_depth,
            d3d12->chain.color_space,
            d3d12->hdr.max_output_nits,
            d3d12->hdr.min_output_nits,
            d3d12->hdr.max_cll,
            d3d12->hdr.max_fall);
#endif
   }

   d3d12->queue.allocator->lpVtbl->Reset(d3d12->queue.allocator);

   cmd->lpVtbl->Reset(cmd, d3d12->queue.allocator,
         d3d12->pipes[VIDEO_SHADER_STOCK_BLEND]);

   {
      D3D12DescriptorHeap desc_heaps[] = { d3d12->desc.srv_heap.handle,
                                           d3d12->desc.sampler_heap.handle };
      cmd->lpVtbl->SetDescriptorHeaps(cmd,
            countof(desc_heaps), desc_heaps);
   }

#if 0
   /* Custom viewport doesn't call apply_state_changes, so we can't rely on this for now */
   if (d3d12->resize_viewport)
#endif
      d3d12_update_viewport(d3d12, false);

   cmd->lpVtbl->IASetPrimitiveTopology(cmd,
         D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

   if (frame && width && height)
   {
      if (frame == RETRO_HW_FRAME_BUFFER_VALID)
      {
         if (d3d12->frame.texture[0].desc.Format != d3d12->hw_render_texture_format)
         {
             d3d12->frame.texture[0].desc.Width  = width;
             d3d12->frame.texture[0].desc.Height = height;
             d3d12->frame.texture[0].desc.Format = d3d12->hw_render_texture_format;
             d3d12_release_texture(&d3d12->frame.texture[0]);
             d3d12_init_texture(d3d12->device, &d3d12->frame.texture[0]);

             d3d12->flags |= D3D12_ST_FLAG_INIT_HISTORY;
         }
      }

      if (d3d12->shader_preset)
      {
         if (d3d12->shader_preset->luts && d3d12->luts[0].dirty)
            for (i = 0; i < d3d12->shader_preset->luts; i++)
               d3d12_upload_texture(cmd, &d3d12->luts[i],
                    d3d12);

         if (     (d3d12->frame.texture[0].desc.Width  != width)
               || (d3d12->frame.texture[0].desc.Height != height))
            d3d12->flags |= D3D12_ST_FLAG_RESIZE_RTS;

         if (d3d12->flags & D3D12_ST_FLAG_RESIZE_RTS)
         {
            /* Release all Render Targets (RT) first to avoid memory fragmentation */
            for (i = 0; i < d3d12->shader_preset->passes; i++)
            {
               d3d12_release_texture(&d3d12->pass[i].rt);
               d3d12->pass[i].rt.handle = NULL;
               d3d12_release_texture(&d3d12->pass[i].feedback);
               d3d12->pass[i].feedback.handle = NULL;
            }
         }

         if (d3d12->shader_preset->history_size)
         {
            if (d3d12->flags & D3D12_ST_FLAG_INIT_HISTORY)
               d3d12_init_history(d3d12, width, height);
            else
            {
               int k;
               /* TODO/FIXME: what about frame-duping ?
                * maybe clone d3d12_texture_t with AddRef */
               d3d12_texture_t tmp =
                  d3d12->frame.texture[d3d12->shader_preset->history_size];
               for (k = d3d12->shader_preset->history_size; k > 0; k--)
                  d3d12->frame.texture[k] = d3d12->frame.texture[k - 1];
               d3d12->frame.texture[0] = tmp;
            }
         }
      }

      /* Either no history, or we moved a texture of a different size in the front slot */
      if (     d3d12->frame.texture[0].desc.Width  != width
            || d3d12->frame.texture[0].desc.Height != height)
      {
         d3d12->frame.texture[0].desc.Width  = width;
         d3d12->frame.texture[0].desc.Height = height;
         d3d12->frame.texture[0].srv_heap    = &d3d12->desc.srv_heap;
         d3d12_release_texture(&d3d12->frame.texture[0]);
         d3d12_init_texture(d3d12->device, &d3d12->frame.texture[0]);
      }

      if (d3d12->flags & D3D12_ST_FLAG_RESIZE_RTS)
         d3d12_init_render_targets(d3d12, width, height);

      if (frame == RETRO_HW_FRAME_BUFFER_VALID)
      {
         D3D12_BOX src_box;
         D3D12_TEXTURE_COPY_LOCATION src, dst;

         src_box.left   = 0;
         src_box.top    = 0;
         src_box.front  = 0;
         src_box.right  = width;
         src_box.bottom = height;
         src_box.back   = 1;

         src.pResource        = d3d12->hw_render_texture;
         src.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
         src.SubresourceIndex = 0;

         dst.pResource        = d3d12->frame.texture[0].handle;
         dst.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
         dst.SubresourceIndex = 0;

         D3D12_RESOURCE_TRANSITION(
             cmd,
             d3d12->frame.texture[0].handle,
             D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
             D3D12_RESOURCE_STATE_COPY_DEST);

         cmd->lpVtbl->CopyTextureRegion(
               cmd, &dst, 0, 0, 0, &src, &src_box);

         D3D12_RESOURCE_TRANSITION(
             cmd,
             d3d12->frame.texture[0].handle,
             D3D12_RESOURCE_STATE_COPY_DEST,
             D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

         d3d12->hw_render_texture = NULL;
      }
      else
      {
         if (d3d12->frame.texture[0].upload_buffer)
            d3d12_update_texture(width, height, pitch, d3d12->format,
                  frame, &d3d12->frame.texture[0]);

         d3d12_upload_texture(cmd, &d3d12->frame.texture[0], d3d12);
      }
   }
   cmd->lpVtbl->IASetVertexBuffers(cmd, 0, 1, &d3d12->frame.vbo_view);

   texture = d3d12->frame.texture;

   if (d3d12->shader_preset)
   {
      cmd->lpVtbl->SetGraphicsRootSignature(cmd,
            d3d12->desc.sl_rootSignature);

      for (i = 0; i < d3d12->shader_preset->passes; i++)
      {
         if (d3d12->shader_preset->pass[i].feedback)
         {
            d3d12_texture_t tmp     = d3d12->pass[i].feedback;
            d3d12->pass[i].feedback = d3d12->pass[i].rt;
            d3d12->pass[i].rt       = tmp;
         }
      }

      for (i = 0; i < d3d12->shader_preset->passes; i++)
      {
         unsigned j;

         cmd->lpVtbl->SetPipelineState(cmd, d3d12->pass[i].pipe);

         if (d3d12->shader_preset->pass[i].frame_count_mod)
            d3d12->pass[i].frame_count = frame_count
                  % d3d12->shader_preset->pass[i].frame_count_mod;
         else
            d3d12->pass[i].frame_count = frame_count;

#ifdef HAVE_REWIND
         if (state_manager_frame_is_reversed())
            d3d12->pass[i].frame_direction = -1;
         else
#endif
         d3d12->pass[i].frame_direction    = 1;
         d3d12->pass[i].frame_time_delta   = (uint32_t)video_driver_get_frame_time_delta_usec();
         d3d12->pass[i].original_fps       = video_driver_get_original_fps();
         d3d12->pass[i].rotation           = retroarch_get_rotation();
         d3d12->pass[i].core_aspect        = video_driver_get_core_aspect();
         /* OriginalAspectRotated: return 1 / aspect for 90 and 270 rotated content */
         d3d12->pass[i].core_aspect_rot    = video_driver_get_core_aspect();
         uint32_t rot = retroarch_get_rotation();
         if (rot == 1 || rot == 3)
            d3d12->pass[i].core_aspect_rot = 1 / d3d12->pass[i].core_aspect_rot;

         /* Sub-frame info for multiframe shaders (per real content frame).
            Should always be 1 for non-use of subframes */
         if (!(d3d12->flags & D3D12_ST_FLAG_FRAME_DUPE_LOCK))
         {
           if (     black_frame_insertion
                 || nonblock_state
                 || runloop_is_slowmotion
                 || runloop_is_paused
                 || (d3d12->flags & D3D12_ST_FLAG_MENU_ENABLE))
              d3d12->pass[i].total_subframes = 1;
           else
              d3d12->pass[i].total_subframes = video_info->shader_subframes;

           d3d12->pass[i].current_subframe = 1;
         }

         for (j = 0; j < SLANG_CBUFFER_MAX; j++)
         {
            cbuffer_sem_t* buffer_sem = &d3d12->pass[i].semantics.cbuffers[j];

            if (buffer_sem->stage_mask && buffer_sem->uniforms)
            {
               D3D12_RANGE    range;
               uint8_t*       mapped_data = NULL;
               uniform_sem_t* uniform     = buffer_sem->uniforms;

               range.Begin                = 0;
               range.End                  = 0;

               D3D12Map(d3d12->pass[i].buffers[j], 0, &range,
                     (void**)&mapped_data);
               while (uniform->size)
               {
                  if (uniform->data)
                     memcpy(mapped_data + uniform->offset,
                           uniform->data, uniform->size);
                  uniform++;
               }
               D3D12Unmap(d3d12->pass[i].buffers[j], 0, NULL);

               cmd->lpVtbl->SetGraphicsRootConstantBufferView(
                     cmd, j == SLANG_CBUFFER_UBO
                     ? ROOT_ID_UBO
                     : ROOT_ID_PC,
                     d3d12->pass[i].buffer_view[j].BufferLocation);
            }
         }
#if 0
         cmd->lpVtbl->OMSetRenderTargets(cmd, 1, NULL, FALSE, NULL);
#endif

         {
            texture_sem_t* texture_sem = d3d12->pass[i].semantics.textures;
            while (texture_sem->stage_mask)
            {
               {
                  D3D12_CPU_DESCRIPTOR_HANDLE handle   = {
                          d3d12->pass[i].textures.ptr
                        - d3d12->desc.srv_heap.gpu.ptr
                        + d3d12->desc.srv_heap.cpu.ptr
                        + texture_sem->binding * d3d12->desc.srv_heap.stride
                  };
                  d3d12_texture_t*                tex  =
                     (d3d12_texture_t*)texture_sem->texture_data;
                  D3D12_SHADER_RESOURCE_VIEW_DESC desc = { tex->desc.Format };

                  desc.Shader4ComponentMapping         =
                     D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                  desc.ViewDimension                   =
                     D3D12_SRV_DIMENSION_TEXTURE2D;
                  desc.Texture2D.MipLevels             = tex->desc.MipLevels;

                  d3d12->device->lpVtbl->CreateShaderResourceView(d3d12->device,
                        tex->handle, &desc, handle);
               }

               {
                  D3D12_SAMPLER_DESC desc;
                  D3D12_CPU_DESCRIPTOR_HANDLE handle = {
                          d3d12->pass[i].samplers.ptr
                        - d3d12->desc.sampler_heap.gpu.ptr
                        + d3d12->desc.sampler_heap.cpu.ptr
                        + texture_sem->binding
                        * d3d12->desc.sampler_heap.stride
                  };

                  if (texture_sem->filter == RARCH_FILTER_NEAREST)
                     desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
                  else
                     desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;

                  switch (texture_sem->wrap)
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

                  desc.AddressV       = desc.AddressU;
                  desc.AddressW       = desc.AddressU;
                  desc.MipLODBias     = 0.0f;
                  desc.MaxAnisotropy  = 1;
                  desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
                  desc.BorderColor[0] = 0.0f;
                  desc.BorderColor[1] = 0.0f;
                  desc.BorderColor[2] = 0.0f;
                  desc.BorderColor[3] = 0.0f;
                  desc.MinLOD         = -D3D12_FLOAT32_MAX;
                  desc.MaxLOD         =  D3D12_FLOAT32_MAX;

                  d3d12->device->lpVtbl->CreateSampler(d3d12->device, &desc, handle);
               }

               texture_sem++;
            }

            cmd->lpVtbl->SetGraphicsRootDescriptorTable(
                  cmd, ROOT_ID_TEXTURE_T,
                  d3d12->pass[i].textures);
            cmd->lpVtbl->SetGraphicsRootDescriptorTable(
                  cmd, ROOT_ID_SAMPLER_T,
                  d3d12->pass[i].samplers);
         }

         if (d3d12->pass[i].rt.handle)
         {
            UINT start_vertex_location = 4;
            D3D12_RESOURCE_TRANSITION(
                  cmd,
                  d3d12->pass[i].rt.handle,
                  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                  D3D12_RESOURCE_STATE_RENDER_TARGET);

            cmd->lpVtbl->OMSetRenderTargets(cmd, 1,
                  &d3d12->pass[i].rt.rt_view, FALSE, NULL);
#if 0
            cmd->lpVtbl->ClearRenderTargetView(
                  cmd, d3d12->pass[i].rt.rt_view,
                  d3d12->chain.clearcolor, 0, NULL);
#endif
            cmd->lpVtbl->RSSetViewports(cmd, 1,
                  &d3d12->pass[i].viewport);

#ifdef D3D12_ROLLING_SCANLINE_SIMULATION
            if (      (video_info->shader_subframes > 1)
                  &&  (video_info->scan_subframes)
                  &&  !black_frame_insertion
                  &&  !nonblock_state
                  &&  !runloop_is_slowmotion
                  &&  !runloop_is_paused
                  &&  (!(d3d12->flags & D3D12_ST_FLAG_MENU_ENABLE)))
            {
               D3D12_RECT scissor_rect;

               scissor_rect.left   = 0;
               scissor_rect.top    = (unsigned int)(((float)d3d12->pass[i].viewport.Height / (float)video_info->shader_subframes)
                                       * (float)video_info->current_subframe);
               scissor_rect.right  = d3d12->pass[i].viewport.Width;
               scissor_rect.bottom = (unsigned int)(((float)d3d12->pass[i].viewport.Height / (float)video_info->shader_subframes)
                                       * (float)(video_info->current_subframe + 1));

               cmd->lpVtbl->RSSetScissorRects(cmd, 1, &scissor_rect);
            }
            else
#endif /* D3D12_ROLLING_SCANLINE_SIMULATION  */
            {
               cmd->lpVtbl->RSSetScissorRects(cmd, 1,
					&d3d12->pass[i].scissorRect);
            }

            if (i == d3d12->shader_preset->passes - 1)
               start_vertex_location = 0;

            cmd->lpVtbl->DrawInstanced(cmd, 4, 1, start_vertex_location, 0);
            D3D12_RESOURCE_TRANSITION(
                  cmd,
                  d3d12->pass[i].rt.handle,
                  D3D12_RESOURCE_STATE_RENDER_TARGET,
                  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            texture = &d3d12->pass[i].rt;
         }
         else
         {
            texture = NULL;
            break;
         }
      }
   }

   if (texture)
   {
      cmd->lpVtbl->SetPipelineState(cmd,
            d3d12->pipes[VIDEO_SHADER_STOCK_NOBLEND]);
      cmd->lpVtbl->SetGraphicsRootSignature(cmd,
            d3d12->desc.rootSignature);
      cmd->lpVtbl->SetGraphicsRootDescriptorTable(cmd,
            ROOT_ID_TEXTURE_T,
            d3d12->frame.texture[0].gpu_descriptor[0]);
      cmd->lpVtbl->SetGraphicsRootDescriptorTable(cmd,
            ROOT_ID_SAMPLER_T,
            d3d12->samplers[RARCH_FILTER_UNSPEC][RARCH_WRAP_DEFAULT]);
      cmd->lpVtbl->SetGraphicsRootConstantBufferView(
            cmd, ROOT_ID_UBO,
            d3d12->frame.ubo_view.BufferLocation);
   }

   d3d12->chain.frame_index = DXGIGetCurrentBackBufferIndex(
         d3d12->chain.handle);

#ifdef HAVE_DXGI_HDR
   if ((d3d12->flags & D3D12_ST_FLAG_HDR_ENABLE) && use_back_buffer)
   {
      D3D12_RESOURCE_TRANSITION(
            cmd,
            d3d12->chain.back_buffer.handle,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->lpVtbl->OMSetRenderTargets(
            cmd, 1,
            &d3d12->chain.back_buffer.rt_view,
            FALSE, NULL);
      /* TODO/FIXME - fix this warning that shows up with Debug logging
       * EXECUTIONWARNING #820: CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE
       * We need to set clear value during resource creation to NULL for
       * D3D12_RESOURCE_DIMENSION_BUFFER, yet we get spammed with this
       * warning
       */
      cmd->lpVtbl->ClearRenderTargetView(
            cmd,
            d3d12->chain.back_buffer.rt_view,
            d3d12->chain.clearcolor,
            0, NULL);
   }
   else
#endif
   {
      D3D12_RESOURCE_TRANSITION(
            cmd,
            d3d12->chain.renderTargets[d3d12->chain.frame_index],
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->lpVtbl->OMSetRenderTargets(
            cmd, 1,
            &d3d12->chain.desc_handles[d3d12->chain.frame_index],
            FALSE, NULL);
      cmd->lpVtbl->ClearRenderTargetView(
            cmd,
            d3d12->chain.desc_handles[d3d12->chain.frame_index],
            d3d12->chain.clearcolor,
            0, NULL);
   }

   cmd->lpVtbl->RSSetViewports(cmd, 1, &d3d12->frame.viewport);

#ifdef D3D12_ROLLING_SCANLINE_SIMULATION
   if (      (video_info->shader_subframes > 1)
         &&  (video_info->scan_subframes)
         &&  !black_frame_insertion
         &&  !nonblock_state
         &&  !runloop_is_slowmotion
         &&  !runloop_is_paused
         &&  (!(d3d12->flags & D3D12_ST_FLAG_MENU_ENABLE)))
   {
      D3D12_RECT scissor_rect;

      scissor_rect.left   = 0;
      scissor_rect.top    = (unsigned int)(((float)video_height / (float)video_info->shader_subframes)
                              * (float)video_info->current_subframe);
      scissor_rect.right  = video_width ;
      scissor_rect.bottom = (unsigned int)(((float)video_height / (float)video_info->shader_subframes)
                              * (float)(video_info->current_subframe + 1));

      cmd->lpVtbl->RSSetScissorRects(cmd, 1, &scissor_rect);
   }
   else
#endif /* D3D12_ROLLING_SCANLINE_SIMULATION  */
   {
      cmd->lpVtbl->RSSetScissorRects(cmd, 1, &d3d12->frame.scissorRect);
   }

   cmd->lpVtbl->DrawInstanced(cmd, 4, 1, 0, 0);

   cmd->lpVtbl->SetPipelineState(cmd, d3d12->pipes[VIDEO_SHADER_STOCK_BLEND]);
   cmd->lpVtbl->SetGraphicsRootSignature(cmd, d3d12->desc.rootSignature);

   if (    (d3d12->flags & D3D12_ST_FLAG_MENU_ENABLE)
         && d3d12->menu.texture.handle)
   {
      if (d3d12->menu.texture.dirty)
         d3d12_upload_texture(cmd, &d3d12->menu.texture,
               d3d12);

      cmd->lpVtbl->SetGraphicsRootConstantBufferView(
            cmd, ROOT_ID_UBO, d3d12->ubo_view.BufferLocation);

      if (d3d12->flags & D3D12_ST_FLAG_MENU_FULLSCREEN)
      {
         cmd->lpVtbl->RSSetViewports(cmd, 1, &d3d12->chain.viewport);
         cmd->lpVtbl->RSSetScissorRects(cmd, 1, &d3d12->chain.scissorRect);
      }

      cmd->lpVtbl->SetGraphicsRootDescriptorTable(cmd, ROOT_ID_TEXTURE_T,
            d3d12->menu.texture.gpu_descriptor[0]);
      cmd->lpVtbl->SetGraphicsRootDescriptorTable(cmd, ROOT_ID_SAMPLER_T,
            d3d12->menu.texture.sampler);
      cmd->lpVtbl->IASetVertexBuffers(cmd, 0, 1, &d3d12->menu.vbo_view);
      cmd->lpVtbl->DrawInstanced(cmd, 4, 1, 0, 0);
   }

   d3d12->sprites.pipe = d3d12->sprites.pipe_noblend;
   cmd->lpVtbl->SetPipelineState(cmd, (D3D12PipelineState)d3d12->sprites.pipe);
   cmd->lpVtbl->IASetPrimitiveTopology(cmd, D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

   d3d12->flags |= D3D12_ST_FLAG_SPRITES_ENABLE;

#ifdef HAVE_OVERLAY
   if ((d3d12->flags & D3D12_ST_FLAG_OVERLAYS_ENABLE) && overlay_behind_menu)
      d3d12_render_overlay(d3d12);
#endif

#ifdef HAVE_MENU
#ifndef HAVE_GFX_WIDGETS
   if (d3d12->flags & D3D12_ST_FLAG_MENU_ENABLE)
#endif
   {
      cmd->lpVtbl->RSSetViewports(cmd, 1, &d3d12->chain.viewport);
      cmd->lpVtbl->RSSetScissorRects(cmd, 1, &d3d12->chain.scissorRect);
      cmd->lpVtbl->IASetVertexBuffers(cmd, 0, 1, &d3d12->sprites.vbo_view);
   }
#endif

#ifdef HAVE_MENU
   if (d3d12->flags & D3D12_ST_FLAG_MENU_ENABLE)
      menu_driver_frame(menu_is_alive, video_info);
   else
#endif
      if (statistics_show)
      {
         if (osd_params)
         {
            cmd->lpVtbl->SetPipelineState(cmd, d3d12->sprites.pipe_blend);
            cmd->lpVtbl->RSSetViewports(cmd, 1, &d3d12->chain.viewport);
            cmd->lpVtbl->RSSetScissorRects(cmd, 1, &d3d12->chain.scissorRect);
            cmd->lpVtbl->IASetVertexBuffers(cmd, 0, 1, &d3d12->sprites.vbo_view);
            font_driver_render_msg(d3d12, stat_text,
                  (const struct font_params*)osd_params, NULL);
         }
      }
#ifdef HAVE_OVERLAY
   if ((d3d12->flags & D3D12_ST_FLAG_OVERLAYS_ENABLE) && !overlay_behind_menu)
      d3d12_render_overlay(d3d12);
#endif

#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
      gfx_widgets_frame(video_info);
#endif

   if (msg && *msg)
   {
      cmd->lpVtbl->SetPipelineState(cmd, d3d12->sprites.pipe_blend);
      cmd->lpVtbl->RSSetViewports(cmd, 1, &d3d12->chain.viewport);
      cmd->lpVtbl->RSSetScissorRects(cmd, 1, &d3d12->chain.scissorRect);
      cmd->lpVtbl->IASetVertexBuffers(cmd, 0, 1, &d3d12->sprites.vbo_view);
      font_driver_render_msg(d3d12, msg, NULL, NULL);
   }
   d3d12->flags &= ~D3D12_ST_FLAG_SPRITES_ENABLE;

#if defined(_WIN32) && !defined(__WINRT__)
   video_driver_update_title(NULL);
#endif

#ifdef HAVE_DXGI_HDR
   /* Copy over back buffer to swap chain render targets */
   if ((d3d12->flags & D3D12_ST_FLAG_HDR_ENABLE) && use_back_buffer)
   {
      D3D12_RESOURCE_TRANSITION(
            cmd,
            d3d12->chain.renderTargets[d3d12->chain.frame_index],
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_RESOURCE_TRANSITION(
            cmd,
            d3d12->chain.back_buffer.handle,
            D3D12_RESOURCE_STATE_RENDER_TARGET,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
      cmd->lpVtbl->SetPipelineState(cmd, d3d12->pipes[VIDEO_SHADER_STOCK_HDR]);

      cmd->lpVtbl->OMSetRenderTargets(
            cmd, 1, &d3d12->chain.desc_handles[d3d12->chain.frame_index],
            FALSE, NULL);
      cmd->lpVtbl->ClearRenderTargetView(
            cmd, d3d12->chain.desc_handles[d3d12->chain.frame_index],
            d3d12->chain.clearcolor, 0, NULL);

      cmd->lpVtbl->SetGraphicsRootSignature(cmd, d3d12->desc.rootSignature);
      cmd->lpVtbl->SetGraphicsRootDescriptorTable(cmd, ROOT_ID_TEXTURE_T,
            d3d12->chain.back_buffer.gpu_descriptor[0]);
      cmd->lpVtbl->SetGraphicsRootDescriptorTable(cmd, ROOT_ID_SAMPLER_T,
            d3d12->samplers[RARCH_FILTER_UNSPEC][RARCH_WRAP_DEFAULT]);
      cmd->lpVtbl->SetGraphicsRootConstantBufferView(cmd, ROOT_ID_UBO,
            d3d12->hdr.ubo_view.BufferLocation);
      cmd->lpVtbl->IASetVertexBuffers(cmd, 0, 1, &d3d12->frame.vbo_view);

      cmd->lpVtbl->IASetPrimitiveTopology(cmd, D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

      cmd->lpVtbl->RSSetViewports(cmd, 1, &d3d12->chain.viewport);
      cmd->lpVtbl->RSSetScissorRects(cmd, 1, &d3d12->chain.scissorRect);

      cmd->lpVtbl->DrawInstanced(cmd, 4, 1, 0, 0);
   }
#endif

   D3D12_RESOURCE_TRANSITION(
         cmd,
         d3d12->chain.renderTargets[d3d12->chain.frame_index],
         D3D12_RESOURCE_STATE_RENDER_TARGET,
         D3D12_RESOURCE_STATE_PRESENT);

   cmd->lpVtbl->Close(cmd);
   d3d12->queue.handle->lpVtbl->ExecuteCommandLists(d3d12->queue.handle, 1,
         (ID3D12CommandList* const*)&d3d12->queue.cmd);
   DXGIPresent(d3d12->chain.handle, d3d12->chain.swap_interval, present_flags);

   if (vsync && wait_for_vblank)
   {
      IDXGIOutput *pOutput;
      DXGIGetContainingOutput(d3d12->chain.handle, &pOutput);
      DXGIWaitForVBlank(pOutput);
      Release(pOutput);
   }

   if (
           black_frame_insertion
        && !(d3d12->flags & D3D12_ST_FLAG_MENU_ENABLE)
        && !nonblock_state
        && !runloop_is_slowmotion
        && !runloop_is_paused
        && (!(video_info->shader_subframes > 1)))
   {
      if (video_info->bfi_dark_frames > video_info->black_frame_insertion)
         video_info->bfi_dark_frames = video_info->black_frame_insertion;

      /* BFI now handles variable strobe strength, like on-off-off, vs on-on-off for 180hz.
         This needs to be done with duping frames instead of increased swap intervals for
         a couple reasons. Swap interval caps out at 4 in most all apis as of coding,
         and seems to be flat ignored >1 at least in modern Windows for some older APIs. */
      bfi_light_frames = video_info->black_frame_insertion - video_info->bfi_dark_frames;
      if (bfi_light_frames > 0 && !(d3d12->flags & D3D12_ST_FLAG_FRAME_DUPE_LOCK))
      {
         d3d12->flags |= D3D12_ST_FLAG_FRAME_DUPE_LOCK;
         while (bfi_light_frames > 0)
         {
            if (!(d3d12_gfx_frame(d3d12, NULL, 0, 0, frame_count, 0, msg, video_info)))
            {
               d3d12->flags &= ~D3D12_ST_FLAG_FRAME_DUPE_LOCK;
               return false;
            }
            --bfi_light_frames;
         }
         d3d12->flags &= ~D3D12_ST_FLAG_FRAME_DUPE_LOCK;
      }

      for (n = 0; n < video_info->bfi_dark_frames; ++n)
      {
         if (!(d3d12->flags & D3D12_ST_FLAG_FRAME_DUPE_LOCK))
         {
            dx12_inject_black_frame(d3d12);
         }
      }
   }

   /* Frame duping for Shader Subframes, don't combine with swap_interval > 1, BFI.
      Also, a major logical use of shader sub-frames will still be shader implemented BFI
      or even rolling scan bfi, so we need to protect the menu/ff/etc from bad flickering
      from improper settings, and unnecessary performance overhead for ff, screenshots etc. */
   if (      (video_info->shader_subframes > 1)
         &&  !black_frame_insertion
         &&  !nonblock_state
         &&  !runloop_is_slowmotion
         &&  !runloop_is_paused
         &&  (!(d3d12->flags & D3D12_ST_FLAG_MENU_ENABLE))
         &&  (!(d3d12->flags & D3D12_ST_FLAG_FRAME_DUPE_LOCK)))
   {
      d3d12->flags |= D3D12_ST_FLAG_FRAME_DUPE_LOCK;
      for (k = 1; k < video_info->shader_subframes; k++)
      {
#ifdef D3D12_ROLLING_SCANLINE_SIMULATION
         video_info->current_subframe = k;
#endif /* D3D12_ROLLING_SCANLINE_SIMULATION */

         if (d3d12->shader_preset)
            for (m = 0; m < d3d12->shader_preset->passes; m++)
            {
               d3d12->pass[m].total_subframes = video_info->shader_subframes;
               d3d12->pass[m].current_subframe = k+1;
            }
         if (!d3d12_gfx_frame(d3d12, NULL, 0, 0, frame_count, 0, msg,
                  video_info))
         {
            d3d12->flags &= ~D3D12_ST_FLAG_FRAME_DUPE_LOCK;
            return false;
         }
      }
      d3d12->flags &= ~D3D12_ST_FLAG_FRAME_DUPE_LOCK;
   }

   return true;
}

static void d3d12_gfx_set_nonblock_state(void* data,
      bool toggle,
      bool adaptive_vsync_enabled,
      unsigned swap_interval)
{
   d3d12_video_t* d3d12       = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   if (toggle)
      d3d12->flags           &= ~D3D12_ST_FLAG_VSYNC;
   else
      d3d12->flags           |=  D3D12_ST_FLAG_VSYNC;

   d3d12->chain.swap_interval = (!toggle) ? swap_interval : 0;
}

static bool d3d12_gfx_alive(void* data)
{
   bool quit;
   bool resize_chain    = false;
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   win32_check_window(NULL,
         &quit,
         &resize_chain,
         &d3d12->vp.full_width,
         &d3d12->vp.full_height);

   if (resize_chain)
      d3d12->flags |=  D3D12_ST_FLAG_RESIZE_CHAIN;
   else
      d3d12->flags &= ~D3D12_ST_FLAG_RESIZE_CHAIN;

   if (     (d3d12->flags & D3D12_ST_FLAG_RESIZE_CHAIN)
         && (d3d12->vp.full_width  != 0)
         && (d3d12->vp.full_height != 0))
      video_driver_set_size(d3d12->vp.full_width, d3d12->vp.full_height);

   return !quit;
}

static bool d3d12_gfx_suppress_screensaver(void* data, bool enable) { return false; }
static bool d3d12_gfx_has_windowed(void* data) { return true; }

static struct video_shader* d3d12_gfx_get_current_shader(void* data)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   if (!d3d12)
      return NULL;

   return d3d12->shader_preset;
}

static void d3d12_gfx_viewport_info(void* data, struct video_viewport* vp)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   *vp = d3d12->vp;
}

static void d3d12_set_menu_texture_frame(
      void* data, const void* frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   d3d12_video_t* d3d12    = (d3d12_video_t*)data;
   settings_t*    settings = config_get_ptr();
   int            pitch    = width *
      (rgb32 ? sizeof(uint32_t) : sizeof(uint16_t));
   DXGI_FORMAT    format   = rgb32 ? DXGI_FORMAT_B8G8R8A8_UNORM
      : (DXGI_FORMAT)DXGI_FORMAT_EX_A4R4G4B4_UNORM;

   if (
         d3d12->menu.texture.desc.Width  != width ||
         d3d12->menu.texture.desc.Height != height)
   {
      d3d12->menu.texture.desc.Width  = width;
      d3d12->menu.texture.desc.Height = height;
      d3d12->menu.texture.desc.Format = format;
      d3d12->menu.texture.srv_heap    = &d3d12->desc.srv_heap;
      d3d12_release_texture(&d3d12->menu.texture);
      d3d12_init_texture(d3d12->device, &d3d12->menu.texture);
   }

   if (d3d12->menu.texture.upload_buffer)
      d3d12_update_texture(width, height, pitch,
            format, frame, &d3d12->menu.texture);

   d3d12->menu.alpha = alpha;

   {
      D3D12_RANGE read_range;
      d3d12_vertex_t* v          = NULL;

      read_range.Begin           = 0;
      read_range.End             = 0;
      D3D12Map(d3d12->menu.vbo, 0, &read_range, (void**)&v);
      v[0].color[3]              = alpha;
      v[1].color[3]              = alpha;
      v[2].color[3]              = alpha;
      v[3].color[3]              = alpha;
      D3D12Unmap(d3d12->menu.vbo, 0, NULL);
   }

   d3d12->menu.texture.sampler = settings->bools.menu_linear_filter
      ? d3d12->samplers[RARCH_FILTER_LINEAR][RARCH_WRAP_DEFAULT]
      : d3d12->samplers[RARCH_FILTER_NEAREST][RARCH_WRAP_DEFAULT];
}

static void d3d12_set_menu_texture_enable(void* data,
      bool state, bool fullscreen)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   if (!d3d12)
      return;

   if (state)
      d3d12->flags |=  D3D12_ST_FLAG_MENU_ENABLE;
   else
      d3d12->flags &= ~D3D12_ST_FLAG_MENU_ENABLE;
   if (fullscreen)
      d3d12->flags |=  D3D12_ST_FLAG_MENU_FULLSCREEN;
   else
      d3d12->flags &= ~D3D12_ST_FLAG_MENU_FULLSCREEN;
}

static void d3d12_gfx_set_aspect_ratio(
      void* data, unsigned aspect_ratio_idx)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;

   if (d3d12)
      d3d12->flags |= D3D12_ST_FLAG_KEEP_ASPECT | D3D12_ST_FLAG_RESIZE_VIEWPORT;
}

static void d3d12_gfx_apply_state_changes(void* data)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;
   if (d3d12)
      d3d12->flags |= D3D12_ST_FLAG_RESIZE_VIEWPORT;
}

static void d3d12_gfx_set_osd_msg(
      void* data, const char *msg,
      const struct font_params *params,
      void* font)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;
   if (d3d12 && (d3d12->flags & D3D12_ST_FLAG_SPRITES_ENABLE))
      font_driver_render_msg(d3d12, msg, params, font);
}

static uintptr_t d3d12_gfx_load_texture(
      void* video_data, void* data, bool threaded,
      enum texture_filter_type filter_type)
{
   d3d12_texture_t*      texture = NULL;
   d3d12_video_t*        d3d12   = (d3d12_video_t*)video_data;
   struct texture_image* image   = (struct texture_image*)data;

   if (!d3d12)
      return 0;

   texture = (d3d12_texture_t*)calloc(1, sizeof(*texture));

   if (!texture)
      return 0;

   switch (filter_type)
   {
      case TEXTURE_FILTER_MIPMAP_LINEAR:
         texture->desc.MipLevels = UINT16_MAX;
      case TEXTURE_FILTER_LINEAR:
         texture->sampler = d3d12->samplers[
            RARCH_FILTER_LINEAR][RARCH_WRAP_EDGE];
         break;
      case TEXTURE_FILTER_MIPMAP_NEAREST:
         texture->desc.MipLevels = UINT16_MAX;
      case TEXTURE_FILTER_NEAREST:
         texture->sampler = d3d12->samplers[
            RARCH_FILTER_NEAREST][RARCH_WRAP_EDGE];
         break;
   }

   texture->desc.Width  = image->width;
   texture->desc.Height = image->height;
   texture->desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
   texture->srv_heap    = &d3d12->desc.srv_heap;

   d3d12_release_texture(texture);
   d3d12_init_texture(d3d12->device, texture);

   if (texture->upload_buffer)
      d3d12_update_texture(
            image->width, image->height, 0,
            DXGI_FORMAT_B8G8R8A8_UNORM, image->pixels, texture);

   return (uintptr_t)texture;
}

static void d3d12_gfx_unload_texture(void* data,
      bool threaded, uintptr_t handle)
{
   d3d12_texture_t* texture = (d3d12_texture_t*)handle;
   d3d12_video_t* d3d12     = (d3d12_video_t*)data;

   if (!texture)
      return;

   if (d3d12)
   {
      D3D12_GFX_SYNC();
   }

   d3d12_release_texture(texture);
   free(texture);
}

static bool d3d12_get_hw_render_interface(
      void* data, const struct retro_hw_render_interface** iface)
{
   d3d12_video_t* d3d12 = (d3d12_video_t*)data;
   *iface               = (const struct retro_hw_render_interface*)
      &d3d12->hw_iface;
   return ((d3d12->flags & D3D12_ST_FLAG_HW_IFACE_ENABLE) > 0);
}

#ifndef __WINRT__
static void d3d12_get_video_output_prev(void *data)
{
   unsigned width  = 0;
   unsigned height = 0;
   win32_get_video_output_prev(&width, &height);
}

static void d3d12_get_video_output_next(void *data)
{
   unsigned width  = 0;
   unsigned height = 0;
   win32_get_video_output_next(&width, &height);
}
#endif

static const video_poke_interface_t d3d12_poke_interface = {
   d3d12_get_flags,
   d3d12_gfx_load_texture,
   d3d12_gfx_unload_texture,
   NULL, /* set_video_mode */
#ifdef __WINRT__
   /* UWP does not expose this information easily */
   NULL,
#else
   win32_get_refresh_rate,
#endif
   d3d12_set_filtering,
#ifdef __WINRT__
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
#else
   win32_get_video_output_size,
   d3d12_get_video_output_prev,
   d3d12_get_video_output_next,
#endif
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   d3d12_gfx_set_aspect_ratio,
   d3d12_gfx_apply_state_changes,
   d3d12_set_menu_texture_frame,
   d3d12_set_menu_texture_enable,
   d3d12_gfx_set_osd_msg,
   win32_show_cursor,
   NULL, /* grab_mouse_toggle */
   d3d12_gfx_get_current_shader,
   NULL, /* get_current_software_framebuffer */
   d3d12_get_hw_render_interface,
#ifdef HAVE_DXGI_HDR
   d3d12_set_hdr_max_nits,
   d3d12_set_hdr_paper_white_nits,
   d3d12_set_hdr_contrast,
   d3d12_set_hdr_expand_gamut
#else
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
#endif
};

static void d3d12_gfx_get_poke_interface(void* data, const video_poke_interface_t** iface)
{
   *iface = &d3d12_poke_interface;
}

#ifdef HAVE_GFX_WIDGETS
static bool d3d12_gfx_widgets_enabled(void *data) { return true; }
#endif

video_driver_t video_d3d12 = {
   d3d12_gfx_init,
   d3d12_gfx_frame,
   d3d12_gfx_set_nonblock_state,
   d3d12_gfx_alive,
   win32_has_focus,
   d3d12_gfx_suppress_screensaver,
   d3d12_gfx_has_windowed,
   d3d12_gfx_set_shader,
   d3d12_gfx_free,
   "d3d12",
   NULL, /* set_viewport */
   d3d12_gfx_set_rotation,
   d3d12_gfx_viewport_info,
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   d3d12_get_overlay_interface,
#endif
   d3d12_gfx_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   d3d12_gfx_widgets_enabled
#endif
};
