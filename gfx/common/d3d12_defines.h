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
   D3D12_ST_FLAG_HW_IFACE_ENABLE       = (1 << 14),
   D3D12_ST_FLAG_FRAME_DUPE_LOCK       = (1 << 15)
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

#ifndef ALIGN
#ifdef _MSC_VER
#define ALIGN(x) __declspec(align(x))
#else
#define ALIGN(x) __attribute__((aligned(x)))
#endif
#endif

/* end of auto-generated */

#endif
