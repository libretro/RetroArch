#if defined(_WIN32) && !defined(__WINRT__)
#define CINTERFACE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi1_5.h>
#include <d3d12.h>
#include <stdlib.h>
#include <string.h>
#include "win_dxgi_present.h"

static const GUID bridge_IDXGISwapChain4 = {0x3d585d5a, 0xbd4a, 0x489e, {0xb1, 0xf4, 0x3d, 0xbc, 0xb6, 0x45, 0x2f, 0xfb}};
static const GUID bridge_IDXGIFactory5 = {0x7632e1f5, 0xee65, 0x4dca, {0x87, 0xfd, 0x84, 0xcd, 0x75, 0xf8, 0x83, 0x8d}};
static const GUID bridge_ID3D12CommandAllocator = {0x6102dee4, 0xaf59, 0x4b09, {0xb9, 0x99, 0xb4, 0x4d, 0x73, 0xf0, 0x9b, 0x24}};
static const GUID bridge_ID3D12CommandQueue = {0x0ec870a6, 0x5d7e, 0x4c22, {0x8c, 0xfc, 0x5b, 0xaa, 0xe0, 0x76, 0x16, 0xed}};
static const GUID bridge_ID3D12Device = {0x189819f1, 0x1db6, 0x4b57, {0xbe, 0x54, 0x18, 0x21, 0x33, 0x9b, 0x85, 0xf7}};
static const GUID bridge_ID3D12Fence = {0x0a753dcf, 0xc4d8, 0x4b91, {0xad, 0xf6, 0xbe, 0x5a, 0x60, 0xd9, 0x5a, 0x76}};
static const GUID bridge_ID3D12GraphicsCommandList = {0x5b160d0f, 0xac1b, 0x4185, {0x8b, 0xa8, 0xb3, 0xae, 0x42, 0xa5, 0xa4, 0x55}};
static const GUID bridge_ID3D12Resource = {0x696442be, 0xa72e, 0x4059, {0xbc, 0x79, 0x5b, 0x5c, 0x98, 0x04, 0x0f, 0xad}};
static const GUID bridge_IDXGIFactory2 = {0x50c83a1c, 0xe072, 0x4c48, {0x87, 0xb0, 0x36, 0x30, 0xfa, 0x36, 0xa6, 0xd0}};
static const GUID bridge_IDXGISwapChain3 = {0x94d99bdb, 0xf1f8, 0x4ab0, {0xb2, 0x36, 0x7d, 0xa0, 0x17, 0x0e, 0xda, 0xb1}};

struct ra_dxgi_bridge
{
   HMODULE dxgi_dll, d3d_dll;
   IDXGIFactory2 *factory;
   ID3D12Device *device;
   ID3D12CommandQueue *queue;
   IDXGISwapChain3 *chain;
   ID3D12Fence *ready, *done;
   ID3D12Resource *shared[2], *buffers[2];
   ID3D12CommandAllocator *allocators[2];
   ID3D12GraphicsCommandList *lists[2];
   HANDLE resources[2], ready_handle, done_handle, event, latency;
   unsigned current_index, width, height, format;
   UINT chain_flags;
   int failed;
   BOOL tearing;
   int occluded, resized;
   UINT64 sequence, last_use[2];
};

typedef HRESULT (WINAPI *create_factory_t)(REFIID, void **);
typedef HRESULT (WINAPI *create_device_t)(IUnknown *, D3D_FEATURE_LEVEL,
      REFIID, void **);

#define DROP(p) do { if (p) { (p)->lpVtbl->Release(p); (p) = NULL; } } while (0)
#define CHECK(expr) do { if (FAILED(expr)) goto error; } while (0)

static int ra_dxgi_bridge_wait_idle(ra_dxgi_bridge_t *b)
{
   if (!b->queue || !b->done || !b->event)
      return 1;
   /* The frame fence precedes Present. */
   if (FAILED(b->queue->lpVtbl->Signal(b->queue, b->done, b->sequence + 1)))
      return 0;
   ++b->sequence;
   if (b->done->lpVtbl->GetCompletedValue(b->done) < b->sequence)
   {
      if (FAILED(b->done->lpVtbl->SetEventOnCompletion(
                  b->done, b->sequence, b->event)))
         return 0;
      if (WaitForSingleObject(b->event, INFINITE) != WAIT_OBJECT_0)
         return 0;
   }
   return 1;
}

static void ra_dxgi_bridge_destroy_buffers(ra_dxgi_bridge_t *b)
{
   unsigned i;
   for (i = 0; i < 2; i++)
   {
      DROP(b->buffers[i]);
      DROP(b->shared[i]);
      if (b->resources[i]) CloseHandle(b->resources[i]);
      b->resources[i] = NULL;
   }
}

void ra_dxgi_bridge_destroy(ra_dxgi_bridge_t *b)
{
   unsigned i;
   if (!b)
      return;
   ra_dxgi_bridge_wait_idle(b);
   for (i = 0; i < 2; i++)
   {
      DROP(b->lists[i]);
      DROP(b->allocators[i]);
   }
   ra_dxgi_bridge_destroy_buffers(b);
   DROP(b->chain);
   DROP(b->ready);
   DROP(b->done);
   DROP(b->queue);
   DROP(b->device);
   DROP(b->factory);
   if (b->ready_handle) CloseHandle(b->ready_handle);
   if (b->done_handle) CloseHandle(b->done_handle);
   if (b->event) CloseHandle(b->event);
   if (b->latency) CloseHandle(b->latency);
   if (b->d3d_dll) FreeLibrary(b->d3d_dll);
   if (b->dxgi_dll) FreeLibrary(b->dxgi_dll);
   free(b);
}

static int ra_dxgi_bridge_create_buffers(ra_dxgi_bridge_t *b,
      unsigned width, unsigned height, unsigned format)
{
   D3D12_HEAP_PROPERTIES heap;
   D3D12_RESOURCE_DESC resource_desc;
   unsigned i;
   memset(&heap, 0, sizeof(heap));
   heap.Type = D3D12_HEAP_TYPE_DEFAULT;
   heap.CreationNodeMask = heap.VisibleNodeMask = 1;
   memset(&resource_desc, 0, sizeof(resource_desc));
   resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   resource_desc.Width = width;
   resource_desc.Height = height;
   resource_desc.DepthOrArraySize = resource_desc.MipLevels = 1;
   resource_desc.Format = (DXGI_FORMAT)format;
   resource_desc.SampleDesc.Count = 1;
   resource_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
   for (i = 0; i < 2; i++)
   {
      D3D12_RESOURCE_BARRIER barriers[2];
      CHECK(b->device->lpVtbl->CreateCommittedResource(b->device, &heap,
               D3D12_HEAP_FLAG_SHARED, &resource_desc, D3D12_RESOURCE_STATE_COMMON,
               NULL, &bridge_ID3D12Resource, (void **)&b->shared[i]));
      CHECK(b->device->lpVtbl->CreateSharedHandle(b->device,
               (ID3D12DeviceChild *)b->shared[i], NULL, GENERIC_ALL, NULL,
               &b->resources[i]));
      CHECK(b->chain->lpVtbl->GetBuffer(b->chain, i, &bridge_ID3D12Resource,
               (void **)&b->buffers[i]));
      if (b->allocators[i])
         CHECK(b->allocators[i]->lpVtbl->Reset(b->allocators[i]));
      else
         CHECK(b->device->lpVtbl->CreateCommandAllocator(b->device,
                  D3D12_COMMAND_LIST_TYPE_DIRECT, &bridge_ID3D12CommandAllocator,
                  (void **)&b->allocators[i]));
      if (b->lists[i])
         CHECK(b->lists[i]->lpVtbl->Reset(b->lists[i], b->allocators[i], NULL));
      else
         CHECK(b->device->lpVtbl->CreateCommandList(b->device, 0,
                  D3D12_COMMAND_LIST_TYPE_DIRECT, b->allocators[i], NULL,
                  &bridge_ID3D12GraphicsCommandList, (void **)&b->lists[i]));
      memset(barriers, 0, sizeof(barriers));
      barriers[0].Type = barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barriers[0].Transition.pResource = b->shared[i];
      barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
      barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
      barriers[1].Transition.pResource = b->buffers[i];
      barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
      barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
      b->lists[i]->lpVtbl->ResourceBarrier(b->lists[i], 2, barriers);
      b->lists[i]->lpVtbl->CopyResource(b->lists[i], b->buffers[i], b->shared[i]);
      barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
      barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
      barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
      barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
      b->lists[i]->lpVtbl->ResourceBarrier(b->lists[i], 2, barriers);
      CHECK(b->lists[i]->lpVtbl->Close(b->lists[i]));
   }
   return 1;
error:
   return 0;
}

static int ra_dxgi_bridge_color_space(ra_dxgi_bridge_t *b, unsigned format)
{
   DXGI_COLOR_SPACE_TYPE space = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
   if (format == DXGI_FORMAT_R16G16B16A16_FLOAT)
      space = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
   else if (format == DXGI_FORMAT_R10G10B10A2_UNORM)
      space = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
   return SUCCEEDED(b->chain->lpVtbl->SetColorSpace1(b->chain, space));
}

int ra_dxgi_bridge_resize(ra_dxgi_bridge_t *b,
      unsigned width, unsigned height, unsigned format)
{
   if (!b || !width || !height || b->failed)
      return 0;
   if (b->width == width && b->height == height && b->format == format)
      return 1;
   if (!ra_dxgi_bridge_wait_idle(b)) goto error;
   ra_dxgi_bridge_destroy_buffers(b);
   if (FAILED(b->chain->lpVtbl->ResizeBuffers(b->chain, 2,
               width, height, (DXGI_FORMAT)format, b->chain_flags))) goto error;
   if (!ra_dxgi_bridge_color_space(b, format)) goto error;
   if (!ra_dxgi_bridge_create_buffers(b, width, height, format)) goto error;
   b->width = width; b->height = height; b->format = format;
   b->last_use[0] = b->last_use[1] = 0;
   b->current_index = 0;
   b->occluded = 0;
   b->resized = 1;
   return 1;
error:
   b->failed = 1;
   return 0;
}

ra_dxgi_bridge_t *ra_dxgi_bridge_create(HWND window, unsigned width,
      unsigned height, unsigned format, const unsigned char luid[8])
{
   ra_dxgi_bridge_t *b;
   create_factory_t create_factory;
   create_device_t create_device;
   IDXGIAdapter1 *adapter = NULL;
   IDXGISwapChain1 *chain = NULL;
   IDXGIFactory5 *factory5 = NULL;
   D3D12_COMMAND_QUEUE_DESC queue_desc;
   DXGI_SWAP_CHAIN_DESC1 chain_desc;
   unsigned i;
   if (!window || !width || !height || !luid)
      return NULL;
   b = (ra_dxgi_bridge_t *)calloc(1, sizeof(*b));
   if (!b) return NULL;
   b->dxgi_dll = LoadLibraryA("dxgi.dll");
   b->d3d_dll  = LoadLibraryA("d3d12.dll");
   if (!b->dxgi_dll || !b->d3d_dll) goto error;
   create_factory = (create_factory_t)GetProcAddress(b->dxgi_dll, "CreateDXGIFactory1");
   create_device  = (create_device_t)GetProcAddress(b->d3d_dll, "D3D12CreateDevice");
   if (!create_factory || !create_device) goto error;
   CHECK(create_factory(&bridge_IDXGIFactory2, (void **)&b->factory));
   if (SUCCEEDED(b->factory->lpVtbl->QueryInterface(b->factory,
               &bridge_IDXGIFactory5, (void **)&factory5)))
   {
      if (FAILED(factory5->lpVtbl->CheckFeatureSupport(factory5,
                  DXGI_FEATURE_PRESENT_ALLOW_TEARING, &b->tearing,
                  sizeof(b->tearing))))
         b->tearing = FALSE;
      DROP(factory5);
   }
   for (i = 0; ; i++)
   {
      DXGI_ADAPTER_DESC1 desc;
      CHECK(b->factory->lpVtbl->EnumAdapters1(b->factory, i, &adapter));
      CHECK(adapter->lpVtbl->GetDesc1(adapter, &desc));
      if (memcmp(&desc.AdapterLuid, luid, 8) == 0) break;
      DROP(adapter);
   }
   CHECK(create_device((IUnknown *)adapter, D3D_FEATURE_LEVEL_11_0,
            &bridge_ID3D12Device, (void **)&b->device));
   DROP(adapter);
   memset(&queue_desc, 0, sizeof(queue_desc));
   queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
   CHECK(b->device->lpVtbl->CreateCommandQueue(b->device, &queue_desc,
            &bridge_ID3D12CommandQueue, (void **)&b->queue));
   memset(&chain_desc, 0, sizeof(chain_desc));
   chain_desc.Width = width;
   chain_desc.Height = height;
   chain_desc.Format = (DXGI_FORMAT)format;
   chain_desc.SampleDesc.Count = 1;
   chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
   chain_desc.BufferCount = 2;
   chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
   chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
   if (b->tearing)
      chain_desc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
   b->chain_flags = chain_desc.Flags;
   CHECK(b->factory->lpVtbl->CreateSwapChainForHwnd(b->factory,
            (IUnknown *)b->queue, window, &chain_desc, NULL, NULL, &chain));
   CHECK(chain->lpVtbl->QueryInterface(chain, &bridge_IDXGISwapChain3,
            (void **)&b->chain));
   DROP(chain);
   CHECK(b->chain->lpVtbl->SetMaximumFrameLatency(b->chain, 1));
   b->latency = b->chain->lpVtbl->GetFrameLatencyWaitableObject(b->chain);
   if (!b->latency) goto error;
   if (!ra_dxgi_bridge_color_space(b, format)) goto error;
   CHECK(b->factory->lpVtbl->MakeWindowAssociation(b->factory,
            window, DXGI_MWA_NO_ALT_ENTER));
   CHECK(b->device->lpVtbl->CreateFence(b->device, 0, D3D12_FENCE_FLAG_SHARED,
            &bridge_ID3D12Fence, (void **)&b->ready));
   CHECK(b->device->lpVtbl->CreateFence(b->device, 0, D3D12_FENCE_FLAG_SHARED,
            &bridge_ID3D12Fence, (void **)&b->done));
   CHECK(b->device->lpVtbl->CreateSharedHandle(b->device, (ID3D12DeviceChild *)b->ready,
            NULL, GENERIC_ALL, NULL, &b->ready_handle));
   CHECK(b->device->lpVtbl->CreateSharedHandle(b->device, (ID3D12DeviceChild *)b->done,
            NULL, GENERIC_ALL, NULL, &b->done_handle));
   b->event = CreateEventA(NULL, FALSE, FALSE, NULL);
   if (!b->event) goto error;
   if (!ra_dxgi_bridge_create_buffers(b, width, height, format)) goto error;
   b->width = width; b->height = height; b->format = format;
   return b;
error:
   DROP(adapter);
   DROP(chain);
   ra_dxgi_bridge_destroy(b);
   return NULL;
}

HANDLE ra_dxgi_bridge_resource(ra_dxgi_bridge_t *b, unsigned index)
{ return index < 2 ? b->resources[index] : NULL; }
HANDLE ra_dxgi_bridge_ready_fence(ra_dxgi_bridge_t *b) { return b->ready_handle; }
HANDLE ra_dxgi_bridge_done_fence(ra_dxgi_bridge_t *b) { return b->done_handle; }
unsigned ra_dxgi_bridge_index(ra_dxgi_bridge_t *b)
{
   DWORD wait;
   HRESULT result;
   if (b->failed) return ~0u;
   if (b->sequence)
   {
      result = b->chain->lpVtbl->Present(b->chain, 0, DXGI_PRESENT_TEST);
      b->occluded = result == DXGI_STATUS_OCCLUDED;
      if (b->occluded)
         return ~0u;
      if (FAILED(result))
      {
         b->failed = 1;
         return ~0u;
      }
   }
   /* ResizeBuffers resets presentation; submit its first frame before pacing. */
   if (!b->resized)
   {
      wait = WaitForSingleObject(b->latency, 1000);
      if (wait != WAIT_OBJECT_0)
      {
         b->failed = wait == WAIT_FAILED;
         return ~0u;
      }
   }
   b->resized = 0;
   b->current_index = b->chain->lpVtbl->GetCurrentBackBufferIndex(b->chain);
   return b->current_index;
}
int ra_dxgi_bridge_presentable(ra_dxgi_bridge_t *b)
{
   HRESULT result;
   if (!b->occluded || b->failed) return 1;
   result = b->chain->lpVtbl->Present(b->chain, 0, DXGI_PRESENT_TEST);
   b->occluded = result == DXGI_STATUS_OCCLUDED;
   if (FAILED(result)) b->failed = 1;
   return !b->occluded;
}
int ra_dxgi_bridge_failed(ra_dxgi_bridge_t *b) { return b->failed; }
uint64_t ra_dxgi_bridge_last_use(ra_dxgi_bridge_t *b, unsigned index)
{ return b->last_use[index]; }
int ra_dxgi_bridge_present(ra_dxgi_bridge_t *b, uint64_t ready, unsigned interval)
{
   unsigned index = b->current_index;
   ID3D12CommandList *list = (ID3D12CommandList *)b->lists[index];
   HRESULT hr;
   hr = b->queue->lpVtbl->Wait(b->queue, b->ready, ready);
   if (FAILED(hr)) return 0;
   b->queue->lpVtbl->ExecuteCommandLists(b->queue, 1, &list);
   hr = b->queue->lpVtbl->Signal(b->queue, b->done, b->sequence + 1);
   if (FAILED(hr)) return 0;
   ++b->sequence;
   b->last_use[index] = b->sequence;
   hr = b->chain->lpVtbl->Present(b->chain, interval > 4 ? 4 : interval,
         !interval && b->tearing ? DXGI_PRESENT_ALLOW_TEARING : 0);
   return SUCCEEDED(hr);
}

void ra_dxgi_bridge_hdr_metadata(ra_dxgi_bridge_t *b, float peak)
{
   IDXGISwapChain4 *chain = NULL;
   DXGI_HDR_METADATA_HDR10 metadata;
   if (FAILED(b->chain->lpVtbl->QueryInterface(b->chain,
               &bridge_IDXGISwapChain4, (void **)&chain)))
      return;
   if (peak <= 0.0f) peak = 1000.0f;
   if (peak > 65535.0f) peak = 65535.0f;
   memset(&metadata, 0, sizeof(metadata));
   metadata.RedPrimary[0] = 35400; metadata.RedPrimary[1] = 14600;
   metadata.GreenPrimary[0] = 8500; metadata.GreenPrimary[1] = 39850;
   metadata.BluePrimary[0] = 6550; metadata.BluePrimary[1] = 2300;
   metadata.WhitePoint[0] = 15635; metadata.WhitePoint[1] = 16450;
   metadata.MaxMasteringLuminance = (UINT)peak;
   metadata.MinMasteringLuminance = 10;
   metadata.MaxContentLightLevel = metadata.MaxFrameAverageLightLevel = (UINT16)peak;
   chain->lpVtbl->SetHDRMetaData(chain, DXGI_HDR_METADATA_TYPE_HDR10,
         sizeof(metadata), &metadata);
   DROP(chain);
}

#endif
