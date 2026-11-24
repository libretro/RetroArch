#ifndef RPYROWAVE_D3D12_TEST_CONTEXT_H
#define RPYROWAVE_D3D12_TEST_CONTEXT_H
/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: MIT
 * Bounded waits and readback are confined to this offline executable.
 */
#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#if defined(_WIN32)
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_4.h>
#include "gpu_vectors.h"

#define RPY_D3D_CHECK(expr) do { if (!(expr)) { \
   fprintf(stderr, "D3D12 failed line %u: %s\n", (unsigned)__LINE__, #expr); \
   goto end; } } while (0)
#define RPY_D3D_RELEASE(object) do { if (object) { \
   (object)->lpVtbl->Release(object); (object) = NULL; } } while (0)

typedef struct rpy_d3d_test
{
   ID3D12GraphicsCommandList *list;
   ID3D12Resource *atlas[2];
} rpy_d3d_test_t;

static ID3D12Resource *rpy_d3d_buffer(ID3D12Device *device, size_t bytes,
      D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_FLAGS flags)
{
   D3D12_HEAP_PROPERTIES heap;
   D3D12_RESOURCE_DESC desc;
   ID3D12Resource *resource = NULL;
   heap.Type                 = type;
   heap.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
   heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
   heap.CreationNodeMask     = 1;
   heap.VisibleNodeMask      = 1;
   desc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
   desc.Alignment        = 0;
   desc.Width            = bytes;
   desc.Height           = 1;
   desc.DepthOrArraySize = 1;
   desc.MipLevels        = 1;
   desc.Format           = DXGI_FORMAT_UNKNOWN;
   desc.SampleDesc.Count = 1;
   desc.SampleDesc.Quality = 0;
   desc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
   desc.Flags            = flags;
   if (FAILED(ID3D12Device_CreateCommittedResource(device, &heap,
               D3D12_HEAP_FLAG_NONE, &desc, state, NULL, &IID_ID3D12Resource,
               (void**)&resource)))
      return NULL;
   return resource;
}

static void rpy_d3d_transition(ID3D12GraphicsCommandList *list, ID3D12Resource *resource,
      D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
   D3D12_RESOURCE_BARRIER barrier;
   barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
   barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
   barrier.Transition.pResource   = resource;
   barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
   barrier.Transition.StateBefore = before;
   barrier.Transition.StateAfter  = after;
   ID3D12GraphicsCommandList_ResourceBarrier(list, 1, &barrier);
}

static int rpy_d3d_dispatch(void *userdata, const rpyrowave_dwt_parameters_t *parameters,
      unsigned source, unsigned x, unsigned y, unsigned z)
{
   rpy_d3d_test_t *test = (rpy_d3d_test_t*)userdata;
   ID3D12GraphicsCommandList_SetComputeRootShaderResourceView(test->list, 0,
         ID3D12Resource_GetGPUVirtualAddress(test->atlas[source]));
   ID3D12GraphicsCommandList_SetComputeRootUnorderedAccessView(test->list, 1,
         ID3D12Resource_GetGPUVirtualAddress(test->atlas[source ^ 1]));
   ID3D12GraphicsCommandList_SetComputeRoot32BitConstants(test->list, 2, 8, parameters, 0);
   ID3D12GraphicsCommandList_Dispatch(test->list, x, y, z);
   rpy_d3d_transition(test->list, test->atlas[source],
         D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   rpy_d3d_transition(test->list, test->atlas[source ^ 1],
         D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   return 1;
}

static int rpy_d3d_submit(ID3D12CommandQueue *queue, ID3D12GraphicsCommandList *list,
      ID3D12Fence *fence, HANDLE event, UINT64 value)
{
   ID3D12CommandList *command = (ID3D12CommandList*)list;
   if (FAILED(ID3D12GraphicsCommandList_Close(list)))
      return 0;
   ID3D12CommandQueue_ExecuteCommandLists(queue, 1, &command);
   if (FAILED(ID3D12CommandQueue_Signal(queue, fence, value)))
      return 0;
   if (ID3D12Fence_GetCompletedValue(fence) < value)
   {
      if (FAILED(ID3D12Fence_SetEventOnCompletion(fence, value, event))
            || WaitForSingleObject(event, 30000) != WAIT_OBJECT_0)
         return 0;
   }
   return 1;
}

static int rpy_d3d_messages(ID3D12InfoQueue *info)
{
   UINT64 count;
   UINT64 i;
   int clean = 1;
   if (!info)
      return 1;
   count = ID3D12InfoQueue_GetNumStoredMessagesAllowedByRetrievalFilter(info);
   for (i = 0; i < count; i++)
   {
      SIZE_T length = 0;
      D3D12_MESSAGE *message;
      if (FAILED(ID3D12InfoQueue_GetMessage(info, i, NULL, &length)))
         return 0;
      message = (D3D12_MESSAGE*)malloc(length);
      if (!message)
         return 0;
      if (FAILED(ID3D12InfoQueue_GetMessage(info, i, message, &length)))
         clean = 0;
      else if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR)
      {
         fprintf(stderr, "D3D12 validation: %s\n", message->pDescription);
         clean = 0;
      }
      free(message);
   }
   return clean;
}


#endif
#endif
