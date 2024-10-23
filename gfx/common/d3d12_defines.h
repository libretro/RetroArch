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

#ifndef _D3D12_DEFINES_H_
#define _D3D12_DEFINES_H_

#include <retro_inline.h>
#include <retro_math.h>
#include <retro_common_api.h>

#include <gfx/math/matrix_4x4.h>

#include "dxgi_common.h"
#include <d3d12.h>

#include <libretro_d3d.h>

#include "../common/d3dcompiler_common.h"
#include "../drivers_shader/slang_process.h"

#define D3D12_MAX_GPU_COUNT 16

#define D3D12_RESOURCE_TRANSITION(cmd, resource, state_before, state_after) \
{ \
   D3D12_RESOURCE_BARRIER _barrier; \
   _barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; \
   _barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE; \
   _barrier.Transition.pResource   = resource; \
   _barrier.Transition.StateBefore = state_before; \
   _barrier.Transition.StateAfter  = state_after; \
   _barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; \
   cmd->lpVtbl->ResourceBarrier(cmd, 1, &_barrier); \
}

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
   D3D12_ST_FLAG_WAIT_FOR_VBLANK       = (1 << 14),
   D3D12_ST_FLAG_HW_IFACE_ENABLE       = (1 << 15),
   D3D12_ST_FLAG_FRAME_DUPE_LOCK       = (1 << 16)
};

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

   struct retro_hw_render_interface_d3d12 hw_iface;
   D3D12Resource hw_render_texture;
   DXGI_FORMAT hw_render_texture_format;

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
      d3d12_descriptor_heap_t srv_heap;         /* ShaderResourceView descriptor heap */
      d3d12_descriptor_heap_t rtv_heap;         /* RenderTargetView descriptor heap */
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
      uint32_t                        rotation;
      uint32_t                        total_subframes;
      uint32_t                        current_subframe;
      float                           core_aspect;
      float                           core_aspect_rot;
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
   uint32_t flags;
} d3d12_video_t;

/* end of auto-generated */

#endif
