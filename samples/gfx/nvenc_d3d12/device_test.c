/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../gfx/common/nvenc_d3d12.h"
#include "../../../record/record_gpu_queue.h"
#include "../../../record/record_gpu_capture_d3d12.h"
#include "../../../record/record_gpu_worker_d3d12.h"
#include <retro_atomic.h>
#include "../../../record/record_gpu_dispatch_d3d12.h"
#if defined(_WIN32) && defined(HAVE_THREADS)
#define COBJMACROS
#ifdef __MINGW32__
#define WIDL_C_INLINE_WRAPPERS
#endif
#include <windows.h>
#include <d3d12.h>
#include "../../../gfx/common/nvenc_d3d12.c"
static struct
{
   ID3D12Resource *source;
   ID3D12Fence *fence;
   unsigned source_add, source_release, fence_add, fence_release;
   bool replacing, source_gap, fence_gap;
} cap_refs;
static ULONG cap_resource_add(ID3D12Resource *p)
{
   if (p == cap_refs.source) cap_refs.source_add++;
   return p->lpVtbl->AddRef(p);
}
static ULONG cap_resource_release(ID3D12Resource *p)
{
   if (p == cap_refs.source)
   {
      cap_refs.source_release++;
      if (cap_refs.replacing && cap_refs.source_release == cap_refs.source_add) cap_refs.source_gap = true;
   }
   return p->lpVtbl->Release(p);
}
static ULONG cap_fence_add(ID3D12Fence *p)
{
   if (p == cap_refs.fence) cap_refs.fence_add++;
   return p->lpVtbl->AddRef(p);
}
static ULONG cap_fence_release(ID3D12Fence *p)
{
   if (p == cap_refs.fence)
   {
      cap_refs.fence_release++;
      if (cap_refs.replacing && cap_refs.fence_release == cap_refs.fence_add) cap_refs.fence_gap = true;
   }
   return p->lpVtbl->Release(p);
}
#pragma push_macro("ID3D12Resource_AddRef")
#pragma push_macro("ID3D12Resource_Release")
#pragma push_macro("ID3D12Fence_AddRef")
#pragma push_macro("ID3D12Fence_Release")
#undef ID3D12Resource_AddRef
#undef ID3D12Resource_Release
#undef ID3D12Fence_AddRef
#undef ID3D12Fence_Release
#define ID3D12Resource_AddRef cap_resource_add
#define ID3D12Resource_Release cap_resource_release
#define ID3D12Fence_AddRef cap_fence_add
#define ID3D12Fence_Release cap_fence_release
#include "../../../record/record_gpu_capture_d3d12.c"
#pragma pop_macro("ID3D12Fence_Release")
#pragma pop_macro("ID3D12Fence_AddRef")
#pragma pop_macro("ID3D12Resource_Release")
#pragma pop_macro("ID3D12Resource_AddRef")
#include "../../../record/record_gpu_worker_d3d12.c"
#include "../../../record/record_mux_mp4.c"
#define W 320
#define H 192
#define FRAMES 24
#define CHECK(x) do { if (!(x)) { printf("FAIL line %u: %s\n", (unsigned)__LINE__, #x); goto done; } } while (0)

static PNVENCENCODEPICTURE real_encode;
static PNVENCLOCKBITSTREAM real_lock;
static unsigned encode_faults, lock_faults;
static bool periodic_fault, eos_fault;
static NVENCSTATUS NVENCAPI busy_encode(void *session, NV_ENC_PIC_PARAMS *params)
{
   NVENCSTATUS status;
   if ((params->encodePicFlags & NV_ENC_PIC_FLAG_EOS) && !eos_fault)
   {
      eos_fault = true;
      return NV_ENC_ERR_ENCODER_BUSY;
   }
   if (!(params->encodePicFlags & NV_ENC_PIC_FLAG_EOS)
         && params->inputTimeStamp == 72 && !periodic_fault)
   {
      periodic_fault = true;
      return NV_ENC_ERR_ENCODER_BUSY;
   }
   if (encode_faults) { encode_faults--; return NV_ENC_ERR_ENCODER_BUSY; }
   status = real_encode(session, params);
   if (status != NV_ENC_SUCCESS && status != NV_ENC_ERR_NEED_MORE_INPUT)
      printf("NVENC status %d, frame %u, flags %u\n", status, params->frameIdx, params->encodePicFlags);
   return status;
}
static NVENCSTATUS NVENCAPI busy_lock(void *session, NV_ENC_LOCK_BITSTREAM *params)
{
   if (lock_faults) { lock_faults--; return NV_ENC_ERR_LOCK_BUSY; }
   return real_lock(session, params);
}

struct cap_proxy
{
   ID3D12CommandQueue iface;
   ID3D12CommandQueue *real;
   ID3D12CommandQueueVtbl table;
   unsigned faults, signals;
   bool reject_wait;
};
static ULONG STDMETHODCALLTYPE cap_release(ID3D12CommandQueue *queue)
{
   return ID3D12CommandQueue_Release(((struct cap_proxy*)queue)->real);
}
static HRESULT STDMETHODCALLTYPE cap_signal(ID3D12CommandQueue *queue, ID3D12Fence *fence, UINT64 value)
{
   struct cap_proxy *p = (struct cap_proxy*)queue;
   p->signals++;
   if (p->faults) { p->faults--; return E_OUTOFMEMORY; }
   return ID3D12CommandQueue_Signal(p->real, fence, value);
}
static HRESULT STDMETHODCALLTYPE cap_wait(ID3D12CommandQueue *queue, ID3D12Fence *fence, UINT64 value)
{
   if (((struct cap_proxy*)queue)->reject_wait) return E_FAIL;
   return ID3D12CommandQueue_Wait(((struct cap_proxy*)queue)->real, fence, value);
}
static void STDMETHODCALLTYPE cap_execute(ID3D12CommandQueue *queue, UINT count, ID3D12CommandList *const *lists)
{
   ID3D12CommandQueue_ExecuteCommandLists(((struct cap_proxy*)queue)->real, count, lists);
}
static bool cap_failure(const nvenc_d3d12_config_t *config, ID3D12CommandQueue *queue,
      ID3D12Resource *source, unsigned crop)
{
   struct cap_proxy proxy;
   record_gpu_capture_d3d12_t *capture = NULL;
   ID3D12Fence *gate = NULL;
   ID3D12CommandQueue *copy_queue = NULL;
   D3D12_COMMAND_QUEUE_DESC queue_desc = {0};
   nvenc_d3d12_config_t invalid = *config;
   record_gpu_frame_t frame;
   uint64_t submitted = 99;
   unsigned idle;
   int ok = 0;
   memset(&proxy, 0, sizeof(proxy));
   proxy.iface.lpVtbl = &proxy.table;
   proxy.real = queue; proxy.table = *queue->lpVtbl;
   proxy.table.Release = cap_release; proxy.table.Signal = cap_signal;
   proxy.table.Wait = cap_wait; proxy.table.ExecuteCommandLists = cap_execute;
   proxy.faults = 1;
   invalid.slots = 3;
   CHECK(record_gpu_capture_d3d12_new(&invalid, queue) == NULL);
   invalid = *config; invalid.width = 0;
   CHECK(record_gpu_capture_d3d12_new(&invalid, queue) == NULL);
   invalid = *config; invalid.format = (enum nvenc_d3d12_format)99;
   CHECK(record_gpu_capture_d3d12_new(&invalid, queue) == NULL);
   queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
   CHECK(SUCCEEDED(ID3D12Device_CreateCommandQueue((ID3D12Device*)config->device,
            &queue_desc, &IID_ID3D12CommandQueue, (void**)&copy_queue)));
   CHECK(record_gpu_capture_d3d12_new(config, copy_queue) == NULL);
   CHECK(SUCCEEDED(ID3D12Device_CreateFence((ID3D12Device*)config->device, 0,
            D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**)&gate)));
   capture = record_gpu_capture_d3d12_new(config, queue);
   CHECK(capture != NULL);
   capture->commands = &proxy.iface;
   memset(&cap_refs, 0, sizeof(cap_refs));
   cap_refs.source = source; cap_refs.fence = gate;
   CHECK(record_gpu_capture_d3d12_push(capture, source, 0, crop, crop, gate, 1,
            0, 1, false, &submitted) == RECORD_GPU_QUEUE_ERROR);
   CHECK(submitted == 1);
   CHECK(proxy.signals == 1 && capture->failed && capture->unfenced);
   CHECK(cap_refs.source_add == 1 && cap_refs.fence_add == 1);
   CHECK(!record_gpu_capture_d3d12_reap(capture));
   CHECK(!cap_refs.source_release && !cap_refs.fence_release);
   CHECK(record_gpu_queue_take(capture->queue, &frame) == RECORD_GPU_QUEUE_END);
   CHECK(!record_gpu_capture_d3d12_free(capture));
   CHECK(proxy.signals == 2 && !capture->unfenced);
   CHECK(!record_gpu_capture_d3d12_reap(capture));
   CHECK(!cap_refs.source_release && !cap_refs.fence_release);
   CHECK(SUCCEEDED(ID3D12Fence_Signal(gate, 1)));
   for (idle = 0; idle < 10000; idle++)
   {
      if (record_gpu_capture_d3d12_free(capture)) { capture = NULL; break; }
      Sleep(1);
   }
   CHECK(capture == NULL && proxy.signals == 2);
   CHECK(cap_refs.source_release == 1 && cap_refs.fence_release == 1);
   ok = 1;
done:
   if (gate) ID3D12Fence_Signal(gate, 1);
   if (capture) record_gpu_capture_d3d12_free(capture);
   if (gate) ID3D12Fence_Release(gate);
   if (copy_queue) ID3D12CommandQueue_Release(copy_queue);
   memset(&cap_refs, 0, sizeof(cap_refs));
   return ok != 0;
}

static bool cap_sources(const nvenc_d3d12_config_t *config, ID3D12CommandQueue *queue,
      unsigned mode)
{
   struct cap_proxy proxy;
   record_gpu_capture_d3d12_t *capture = NULL;
   ID3D12Resource *source = NULL;
   ID3D12Fence *gate = NULL;
   D3D12_HEAP_PROPERTIES heap = {0};
   D3D12_RESOURCE_DESC desc = {0};
   record_gpu_frame_t frame;
   uint64_t submitted = 99;
   unsigned i, idle, expected;
   int result;
   bool held = false, ok = false;
   memset(&proxy, 0, sizeof(proxy));
   memset(&cap_refs, 0, sizeof(cap_refs));
   proxy.iface.lpVtbl = &proxy.table;
   proxy.real = queue; proxy.table = *queue->lpVtbl;
   proxy.table.Release = cap_release; proxy.table.Signal = cap_signal;
   proxy.table.Wait = cap_wait; proxy.table.ExecuteCommandLists = cap_execute;
   proxy.reject_wait = mode == 1;
   heap.Type = D3D12_HEAP_TYPE_DEFAULT;
   desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   desc.Width = config->width; desc.Height = config->height;
   desc.DepthOrArraySize = desc.MipLevels = desc.SampleDesc.Count = 1;
   desc.Format = config->format == NVENC_D3D12_P010 ? DXGI_FORMAT_P010
      : config->format == NVENC_D3D12_NV12 ? DXGI_FORMAT_NV12 : DXGI_FORMAT_B8G8R8A8_UNORM;
   CHECK(SUCCEEDED(ID3D12Device_CreateCommittedResource((ID3D12Device*)config->device,
            &heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON,
            NULL, &IID_ID3D12Resource, (void**)&source)));
   CHECK(SUCCEEDED(ID3D12Device_CreateFence((ID3D12Device*)config->device,
            0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**)&gate)));
   capture = record_gpu_capture_d3d12_new(config, queue); CHECK(capture);
   capture->commands = &proxy.iface;
   cap_refs.source = source; cap_refs.fence = gate;
   CHECK(record_gpu_capture_d3d12_reap(capture));
   if (mode == 1)
   {
      CHECK(record_gpu_capture_d3d12_push(capture, source, 0, 0, 0, gate, 1,
               0, 1, false, &submitted) == RECORD_GPU_QUEUE_ERROR);
      CHECK(!submitted && !cap_refs.source_add && !cap_refs.fence_add);
      CHECK(record_gpu_capture_d3d12_reap(capture));
   }
   else
   {
      for (i = 0; i < config->slots; i++)
         CHECK(record_gpu_capture_d3d12_push(capture, source, 0, 0, 0, gate, 1,
                  i, 1, false, &submitted) == RECORD_GPU_QUEUE_OK);
      CHECK(record_gpu_capture_d3d12_push(capture, source, 0, 0, 0, gate, 1,
               i, 1, false, &submitted) == RECORD_GPU_QUEUE_BUSY);
      CHECK(!submitted && cap_refs.source_add == config->slots && cap_refs.fence_add == config->slots);
      CHECK(!record_gpu_capture_d3d12_reap(capture));
      CHECK(!cap_refs.source_release && !cap_refs.fence_release);
      CHECK(record_gpu_queue_take(capture->queue, &frame) == RECORD_GPU_QUEUE_OK);
      held = true;
      expected = config->slots;
      if (mode == 2)
      {
         CHECK(SUCCEEDED(ID3D12Fence_Signal(gate, 1)));
         for (idle = 0; idle < 10000 && ID3D12Fence_GetCompletedValue(capture->ready) < config->slots; idle++) Sleep(1);
         CHECK(idle < 10000);
         CHECK(record_gpu_queue_release(capture->queue, frame.slot)); held = false;
         /* Test-owned guards turn a broken replacement into a safe failure. */
         cap_refs.replacing = true;
         result = record_gpu_capture_d3d12_push(capture, source, 0, 0, 0, gate, 1,
               config->slots, 1, false, &submitted);
         cap_refs.replacing = false;
         CHECK(result == RECORD_GPU_QUEUE_OK);
         CHECK(!cap_refs.source_gap && !cap_refs.fence_gap);
         CHECK(cap_refs.source_add == config->slots + 1 && cap_refs.fence_add == config->slots + 1);
         CHECK(cap_refs.source_release == config->slots && cap_refs.fence_release == config->slots);
         CHECK(record_gpu_queue_take(capture->queue, &frame) == RECORD_GPU_QUEUE_OK); held = true;
         expected++;
      }
      ID3D12Resource_Release(source); source = NULL;
      ID3D12Fence_Release(gate);
      i = SUCCEEDED(ID3D12Fence_Signal(gate, 1)); gate = NULL;
      CHECK(i);
      for (idle = 0; idle < 10000 && !record_gpu_capture_d3d12_reap(capture); idle++) Sleep(1);
      CHECK(idle < 10000);
      CHECK(cap_refs.source_release == expected && cap_refs.fence_release == expected);
      CHECK(!record_gpu_capture_d3d12_free(capture));
      CHECK(record_gpu_queue_release(capture->queue, frame.slot)); held = false;
      for (i = 1; i < config->slots; i++)
      {
         CHECK(record_gpu_queue_take(capture->queue, &frame) == RECORD_GPU_QUEUE_OK);
         CHECK(record_gpu_queue_release(capture->queue, frame.slot));
      }
   }
   CHECK(record_gpu_capture_d3d12_finish(capture));
   CHECK(record_gpu_capture_d3d12_free(capture)); capture = NULL;
   ok = true;
done:
   if (gate) ID3D12Fence_Signal(gate, 1);
   if (capture)
   {
      if (held) record_gpu_queue_release(capture->queue, frame.slot);
      while (record_gpu_queue_take(capture->queue, &frame) == RECORD_GPU_QUEUE_OK)
         record_gpu_queue_release(capture->queue, frame.slot);
      record_gpu_capture_d3d12_finish(capture);
      for (idle = 0; idle < 10000 && !record_gpu_capture_d3d12_free(capture); idle++) Sleep(1);
      if (idle == 10000) abort();
   }
   if (source) ID3D12Resource_Release(source);
   if (gate) ID3D12Fence_Release(gate);
   memset(&cap_refs, 0, sizeof(cap_refs));
   return ok;
}

struct nve_producer
{
   record_gpu_dispatch_d3d12_t *dispatch;
   record_gpu_queue_t *queue;
   record_gpu_capture_d3d12_t *capture;
   ID3D12Resource *sources[4];
   ID3D12Fence *gate;
   HANDLE notify;
   unsigned crop;
   retro_atomic_int_t stop;
   unsigned made;
   unsigned frames;
};
static DWORD WINAPI nve_produce(void *data)
{
   struct nve_producer *p = (struct nve_producer*)data;
   record_gpu_frame_t frame = {0};
   unsigned idle = 0;
   while (p->made < p->frames && idle < 10000
         && !retro_atomic_load_acquire_int(&p->stop))
   {
      int result;
      if (p->dispatch)
      {
         record_gpu_dispatch_d3d12_video_reap(p->dispatch);
         result = record_gpu_dispatch_d3d12_video(p->dispatch, p->sources[p->made % 4],
               D3D12_RESOURCE_STATE_COMMON, p->crop, p->crop, p->gate, 1,
               p->made, 1, p->made == 0 || p->made == 12, NULL);
      }
      else if (p->capture)
         result = record_gpu_capture_d3d12_push(p->capture, p->sources[p->made % 4],
               D3D12_RESOURCE_STATE_COMMON, p->crop, p->crop, p->gate, 1,
               p->made, 1, p->made == 0 || p->made == 12, NULL);
      else
         result = record_gpu_queue_acquire(p->queue, &frame.slot);
      if (result == RECORD_GPU_QUEUE_BUSY)
      {
         Sleep(1); idle++;
         continue;
      }
      if (result != RECORD_GPU_QUEUE_OK)
         break;
      if (p->capture)
      {
         p->made++; idle = 0;
         if (p->dispatch && p->made == 4)
            if (FAILED(ID3D12Fence_Signal(p->gate, 1))) break;
         if (p->notify) SetEvent(p->notify);
         continue;
      }
      frame.timestamp = p->made; frame.duration = frame.ready_value = 1;
      frame.keyframe = p->made == 12 || (p->made == 0 && p->frames == FRAMES);
      if (!record_gpu_queue_submit(p->queue, &frame))
      {
         record_gpu_queue_cancel(p->queue);
         break;
      }
      p->made++; idle = 0;
      if (p->notify) SetEvent(p->notify);
   }
   if (p->dispatch) record_gpu_dispatch_d3d12_video_finish(p->dispatch);
   else if (p->capture) record_gpu_capture_d3d12_finish(p->capture);
   else record_gpu_queue_finish(p->queue);
   if (p->notify) SetEvent(p->notify);
   return 0;
}

#include "output_pipeline.h"

int main(int argc, char **argv)
{
   ID3D12Device *device = NULL;
   ID3D12CommandQueue *queue = NULL;
   ID3D12CommandAllocator *allocator = NULL;
   ID3D12GraphicsCommandList *commands = NULL;
   ID3D12Fence *ready = NULL;
   ID3D12Resource *upload[4] = {0}, *textures[4] = {0};
   D3D12_COMMAND_QUEUE_DESC qdesc = {0};
   D3D12_HEAP_PROPERTIES heap = {0};
   D3D12_RESOURCE_DESC desc = {0};
   nvenc_d3d12_config_t config;
   nvenc_d3d12_t *encoder = NULL;
   record_gpu_capture_d3d12_t *capture = NULL;
   nvenc_d3d12_packet_t packet;
   record_gpu_worker_d3d12_t *worker = NULL;
   record_mux_mp4_t *mux = NULL;
   record_mp4_config_t mux_config = {0};
   void *headers = NULL;
   size_t headers_size = 0;
   FILE *mp4 = NULL;
   char *mp4_path = NULL;
   struct nve_producer producer = {0};
   HANDLE producer_thread = NULL;
   bool signalled = false;
   FILE *out = NULL, *restart = NULL, *periodic = NULL;
   char *restart_path = NULL;
   unsigned i, x, y, received = 0, idle = 0;
   int status, ok = 0;
   uint64_t value = 0;
   unsigned format, metadata, capturing, source_w, source_h;
   HMODULE runtime;
   memset(&config, 0, sizeof(config));
   if ((argc < 4 || argc > 8)) { puts("usage: device_test codec format output [color [full_range [metadata [capture]]]]"); return 1; }
   capturing = argc > 7 ? (unsigned)atoi(argv[7]) : 0;
   if (capturing > 2) return 1;
   metadata = argc > 6 ? (unsigned)atoi(argv[6]) : 0;
   if (metadata > 4) return 1;
   format = (unsigned)atoi(argv[2]);
   if (format < NVENC_D3D12_BGRA8 || format > NVENC_D3D12_P010) return 1;
   source_w = W + (capturing && format == NVENC_D3D12_BGRA8 ? 16 : 0);
   source_h = H + (capturing && format == NVENC_D3D12_BGRA8 ? 16 : 0);
   runtime = LoadLibraryExA("nvEncodeAPI64.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
   if (!runtime) { puts("SKIP: NVENC runtime unavailable"); return getenv("NVENC_TEST_REQUIRE_HW") ? 1 : 77; }
   FreeLibrary(runtime);
   if (FAILED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void**)&device)))
      return 77;
   CHECK(SUCCEEDED(ID3D12Device_CreateCommandQueue(device, &qdesc, &IID_ID3D12CommandQueue, (void**)&queue)));
   CHECK(SUCCEEDED(ID3D12Device_CreateCommandAllocator(device, D3D12_COMMAND_LIST_TYPE_DIRECT,
            &IID_ID3D12CommandAllocator, (void**)&allocator)));
   CHECK(SUCCEEDED(ID3D12Device_CreateCommandList(device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator, NULL, &IID_ID3D12GraphicsCommandList, (void**)&commands)));
   CHECK(SUCCEEDED(ID3D12Device_CreateFence(device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**)&ready)));
   config.device = device; config.slots = 4; config.width = W; config.height = H;
   config.fps_num = 60; config.fps_den = 1; config.bitrate = 8000000;
   config.format = (enum nvenc_d3d12_format)format; config.codec = (enum nvenc_d3d12_codec)atoi(argv[1]);
   for (i = 0; i < 4; i++)
   {
      D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprints[2];
      D3D12_TEXTURE_COPY_LOCATION source = {0}, target = {0};
      D3D12_RESOURCE_BARRIER barrier = {0};
      unsigned planes = format >= NVENC_D3D12_NV12 ? 2 : 1, plane;
      uint64_t bytes;
      uint8_t *map;
      heap.Type = D3D12_HEAP_TYPE_DEFAULT;
      desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      desc.Width = source_w; desc.Height = source_h;
      desc.DepthOrArraySize = desc.MipLevels = desc.SampleDesc.Count = 1;
      desc.Format = format == NVENC_D3D12_P010 ? DXGI_FORMAT_P010
         : format == NVENC_D3D12_NV12 ? DXGI_FORMAT_NV12 : DXGI_FORMAT_B8G8R8A8_UNORM;
      if (capturing == 1 && format == NVENC_D3D12_BGRA8)
         desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
      CHECK(SUCCEEDED(ID3D12Device_CreateCommittedResource(device, &heap, D3D12_HEAP_FLAG_NONE,
               &desc, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (void**)&textures[i])));
      ID3D12Device_GetCopyableFootprints(device, &desc, 0, planes, 0, footprints, NULL, NULL, &bytes);
      heap.Type = D3D12_HEAP_TYPE_UPLOAD;
      memset(&desc, 0, sizeof(desc)); desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
      desc.Width = bytes; desc.Height = desc.DepthOrArraySize = desc.MipLevels = desc.SampleDesc.Count = 1;
      desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
      CHECK(SUCCEEDED(ID3D12Device_CreateCommittedResource(device, &heap, D3D12_HEAP_FLAG_NONE,
               &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, (void**)&upload[i])));
      CHECK(SUCCEEDED(ID3D12Resource_Map(upload[i], 0, NULL, (void**)&map)));
      memset(map, 0, (size_t)bytes);
      for (plane = 0; plane < planes; plane++)
         for (y = 0; y < (plane ? source_h / 2 : source_h); y++)
            for (x = 0; x < source_w; x++)
            {
               uint8_t *p = map + footprints[plane].Offset + y * footprints[plane].Footprint.RowPitch;
               if (format == NVENC_D3D12_BGRA8)
               {
                  p += x * 4;
                  p[0] = (uint8_t)(24 + i * 48); p[1] = (uint8_t)(32 + ((x >= 8 && capturing ? x - 8 : x) / 40) * 24);
                  p[2] = (uint8_t)(32 + ((y >= 8 && capturing ? y - 8 : y) / 24) * 24); p[3] = 255;
                  if (capturing == 1)
                  {
                     uint8_t red = p[2]; p[2] = p[0]; p[0] = red;
                  }
               }
               else
               {
                  unsigned sample = plane ? ((x & 1) ? 144 : 112) : 32 + i * 40 + (x / 40) * 3;
                  if (format == NVENC_D3D12_P010)
                  {
                     uint16_t value16 = (uint16_t)((sample * 4 + (plane ? 0 : 1)) << 6);
                     memcpy(p + x * 2, &value16, sizeof(value16));
                  }
                  else p[x] = (uint8_t)sample;
               }
            }
      ID3D12Resource_Unmap(upload[i], 0, NULL);
      for (plane = 0; plane < planes; plane++)
      {
         source.pResource = upload[i]; source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
         source.PlacedFootprint = footprints[plane];
         target.pResource = textures[i]; target.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
         target.SubresourceIndex = plane;
         ID3D12GraphicsCommandList_CopyTextureRegion(commands, &target, 0, 0, 0, &source, NULL);
      }
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Transition.pResource = textures[i];
      barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
      ID3D12GraphicsCommandList_ResourceBarrier(commands, 1, &barrier);
      config.textures[i] = textures[i];
   }
   CHECK(SUCCEEDED(ID3D12GraphicsCommandList_Close(commands)));
   { ID3D12CommandList *list = (ID3D12CommandList*)commands; ID3D12CommandQueue_ExecuteCommandLists(queue, 1, &list); }
   if (capturing)
   {
      CHECK(cap_failure(&config, queue, textures[0], format == NVENC_D3D12_BGRA8 ? 8 : 0));
      CHECK(cap_sources(&config, queue, 0));
      CHECK(cap_sources(&config, queue, 1));
      CHECK(cap_sources(&config, queue, 2));
      capture = record_gpu_capture_d3d12_new(&config, queue);
      CHECK(capture != NULL);
      CHECK(record_gpu_capture_d3d12_bind(capture, &config));
      producer.capture = capture;
      producer.gate = ready;
      producer.crop = format == NVENC_D3D12_BGRA8 ? 8 : 0;
      for (i = 0; i < 4; i++) producer.sources[i] = textures[i];
      CHECK(record_gpu_capture_d3d12_push(capture, textures[0], 0,
               (unsigned)-1, 0, NULL, 0, 0, 1, false, NULL) == RECORD_GPU_QUEUE_ERROR);
      CHECK(record_gpu_capture_d3d12_push(capture, config.textures[0], 0,
               0, 0, NULL, 0, 0, 1, false, NULL) == RECORD_GPU_QUEUE_ERROR);
      CHECK(record_gpu_capture_d3d12_push(capture, textures[0], 0,
               0, 0, NULL, 1, 0, 1, false, NULL) == RECORD_GPU_QUEUE_ERROR);
   }
   {
      void *saved = config.textures[1];
      unsigned saved_den = config.fps_den;
      config.textures[1] = config.textures[0];
      CHECK(nvenc_d3d12_open(&config) == NULL);
      config.textures[1] = saved;
      config.fps_den = 0;
      CHECK(nvenc_d3d12_open(&config) == NULL);
      config.fps_den = saved_den;
      config.color = (enum nvenc_d3d12_color)99;
      CHECK(nvenc_d3d12_open(&config) == NULL);
      config.color = NVENC_D3D12_COLOR_DEFAULT;
      config.full_range = true;
      CHECK(nvenc_d3d12_open(&config) == NULL);
      config.full_range = false;
      if (format != NVENC_D3D12_P010)
      {
         config.color = NVENC_D3D12_COLOR_BT2020_PQ;
         CHECK(nvenc_d3d12_open(&config) == NULL);
      }
      config.color = NVENC_D3D12_COLOR_DEFAULT;
   }
   config.color = format == NVENC_D3D12_P010
      ? NVENC_D3D12_COLOR_BT2020_PQ : NVENC_D3D12_COLOR_BT709;
   if (argc > 4) config.color = (enum nvenc_d3d12_color)atoi(argv[4]);
   config.full_range = argc > 5 && atoi(argv[5]) != 0;
   {
      unsigned saved;
      config.mastering.present = 1;
      CHECK(nvenc_d3d12_open(&config) == NULL);
      config.mastering.present = VIDEO_MASTERING_DISPLAY_COMPLETE;
      CHECK(nvenc_d3d12_open(&config) == NULL);
      config.mastering.luminance_max = 100000001;
      CHECK(nvenc_d3d12_open(&config) == NULL);
      config.mastering.luminance_max = 10000000;
      config.mastering.luminance_min = 10000001;
      CHECK(nvenc_d3d12_open(&config) == NULL);
      config.mastering.luminance_min = 50;
      config.mastering.chromaticity[0] = 50001;
      CHECK(nvenc_d3d12_open(&config) == NULL);
      memset(&config.mastering, 0, sizeof(config.mastering));
      config.max_cll = 65536;
      CHECK(nvenc_d3d12_open(&config) == NULL);
      config.max_cll = 1000; config.max_fall = 1001;
      CHECK(nvenc_d3d12_open(&config) == NULL);
      config.max_fall = 400;
      saved = config.color; config.color = NVENC_D3D12_COLOR_BT709;
      CHECK(nvenc_d3d12_open(&config) == NULL);
      config.color = (enum nvenc_d3d12_color)saved;
      config.max_cll = config.max_fall = 0;
   }
   if ((metadata & 1) || metadata == 4)
   {
      static const uint16_t xy[8] = {34000, 16000, 13250, 34500, 7500, 3000, 15635, 16450};
      config.mastering.present = VIDEO_MASTERING_DISPLAY_COMPLETE;
      config.mastering.luminance_max = 10000000;
      config.mastering.luminance_min = 50;
      memcpy(config.mastering.chromaticity, xy, sizeof(xy));
      if (metadata == 4)
      {
         static const uint16_t edge[8] = {50000, 0, 0, 50000, 0, 0, 0, 0};
         memcpy(config.mastering.chromaticity, edge, sizeof(edge));
         config.mastering.luminance_max = 100000000;
         config.mastering.luminance_min = 0;
      }
   }
   if ((metadata & 2) || metadata == 4)
   {
      config.max_cll = metadata == 4 ? 65535 : 1000;
      config.max_fall = metadata == 4 ? 0 : 400;
   }
   if (getenv("NVENC_TEST_DISPATCH"))
   {
      CHECK(capture && !metadata);
      producer.frames = FRAMES;
      out = fopen(argv[3], "wb"); CHECK(out != NULL);
      mux_config.timescale = config.fps_num;
      mux_config.max_packet_size = 1024 * 1024;
      status = nve_pipeline_run(NULL, NULL, &mux_config, argv[3], out, &received,
            queue, ready, &value, true, FRAMES, NULL, &config, &producer);
      capture = producer.capture;
      CHECK(status);
      CHECK(record_gpu_capture_d3d12_free(capture)); capture = NULL;
      ok = 1; goto done;
   }
   encoder = nvenc_d3d12_open(&config);
   if (!encoder && !getenv("NVENC_TEST_REQUIRE_HW")) { puts("SKIP: codec/format/session unavailable"); ok = 77; goto done; }
   CHECK(encoder != NULL);
   CHECK(nvenc_d3d12_wait_handle(encoder) == NULL && nvenc_d3d12_wait_handle(NULL) == NULL);
   headers = malloc(65536); CHECK(headers != NULL);
   CHECK(nvenc_d3d12_headers(encoder, headers, 65536, &headers_size));
   mux_config.headers = headers; mux_config.headers_size = headers_size;
   mux_config.max_packet_size = 1024 * 1024;
   mux_config.codec = (enum record_mp4_codec)config.codec;
   mux_config.width = W; mux_config.height = H;
   mux_config.timescale = config.fps_num;
   mux_config.bit_depth = format == NVENC_D3D12_P010 ? 10 : 8;
   mux = record_mux_mp4_new(&mux_config); CHECK(mux != NULL);
   memset(headers, 0, headers_size); free(headers); headers = NULL;
   mp4_path = (char*)malloc(strlen(argv[3]) + 5); CHECK(mp4_path != NULL);
   strcpy(mp4_path, argv[3]); strcat(mp4_path, ".mp4");
   mp4 = fopen(mp4_path, "wb"); CHECK(mp4 != NULL);
   {
      const void *bytes;
      size_t length;
      CHECK(record_mux_mp4_get(mux, &bytes, &length));
      CHECK(!record_mux_mp4_finish(mux));
      CHECK(record_mux_mp4_push(mux, 1, headers, 0, 0, 1, true) == RECORD_MP4_BUSY);
      CHECK(fwrite(bytes, 1, length, mp4) == length);
      CHECK(record_mux_mp4_release(mux));
   }
   memset(&config.mastering, 0, sizeof(config.mastering));
   config.max_cll = config.max_fall = 0;
   real_encode = encoder->api.nvEncEncodePicture;
   real_lock = encoder->api.nvEncLockBitstream;
   encoder->api.nvEncEncodePicture = busy_encode;
   encoder->api.nvEncLockBitstream = busy_lock;
   encode_faults = 1;
   CHECK(nvenc_d3d12_submit(encoder, 0, 999, 1, ready, 1, false) == NVENC_D3D12_BUSY);
   CHECK(encoder->count == 0 && encoder->frame == 0 && !encoder->slots[0].busy && encoder->slots[0].value == 0);
   if (!capturing)
      for (i = 0; i < 4; i++) { ID3D12Resource_Release(textures[i]); textures[i] = NULL; }
   lock_faults = 1;
   out = fopen(argv[3], "wb"); CHECK(out != NULL);
   if (metadata)
   {
      size_t length = strlen(argv[3]);
      restart_path = (char*)malloc(length + 5);
      CHECK(restart_path != NULL);
      memcpy(restart_path, argv[3], length);
      memcpy(restart_path + length, ".key", 5);
      restart = fopen(restart_path, "wb"); CHECK(restart != NULL);
      memcpy(restart_path + length, ".gop", 5);
      periodic = fopen(restart_path, "wb"); CHECK(periodic != NULL);
   }
   CHECK(nvenc_d3d12_submit(encoder, 0, 0, 1, NULL, 1, false) == NVENC_D3D12_ERROR);
   CHECK(nvenc_d3d12_release(encoder) == NVENC_D3D12_ERROR);
   CHECK(nvenc_d3d12_submit(encoder, 4, 0, 1, ready, 1, false) == NVENC_D3D12_ERROR);
   producer.queue = capturing ? record_gpu_capture_d3d12_queue(capture) : record_gpu_queue_new(4);
   producer.frames = metadata ? 84 : FRAMES;
   CHECK(producer.queue != NULL);
   retro_atomic_int_init(&producer.stop, 0);
   encode_faults = 1;
   worker = record_gpu_worker_d3d12_new(producer.queue, encoder,
         capturing ? record_gpu_capture_d3d12_fence(capture) : ready);
   CHECK(worker != NULL);
   CHECK(!record_gpu_worker_d3d12_release(worker));
   if (getenv("NVENC_TEST_GPU_AV"))
   {
      producer.notify = CreateEvent(NULL, FALSE, FALSE, NULL);
      CHECK(producer.notify != NULL);
   }
   producer_thread = CreateThread(NULL, 0, nve_produce, &producer, 0, NULL);
   CHECK(producer_thread != NULL);
   if (getenv("NVENC_TEST_GPU_AV"))
   {
      CHECK(!metadata);
      CHECK(nve_pipeline_run(worker, encoder, &mux_config, argv[3], out, &received,
               queue, ready, &value, capturing != 0, producer.frames, producer.notify, NULL, NULL));
      status = RECORD_GPU_WORKER_END;
   }
   else
   while (idle < 10000)
   {
      bool progress = false;
      status = record_gpu_worker_d3d12_step(worker, &packet);
      CHECK(status != RECORD_GPU_WORKER_ERROR);
      progress = status == RECORD_GPU_WORKER_PROGRESS;
      if (!signalled && encoder->count == 4)
      {
         CHECK(!nvenc_d3d12_free(encoder));
         if (capturing) CHECK(!record_gpu_capture_d3d12_free(capture));
         Sleep(30);
         CHECK(nvenc_d3d12_wait_handle(encoder) != NULL);
         CHECK(WaitForSingleObject(nvenc_d3d12_wait_handle(encoder), 0) == WAIT_TIMEOUT);
         CHECK(nvenc_d3d12_poll(encoder, &packet) == NVENC_D3D12_BUSY);
         CHECK(SUCCEEDED(capturing ? ID3D12Fence_Signal(ready, ++value)
               : ID3D12CommandQueue_Signal(queue, ready, ++value)));
         signalled = true;
      }
      if (status == RECORD_GPU_WORKER_END)
         break;
      if (status == RECORD_GPU_WORKER_PACKET)
      {
         nvenc_d3d12_packet_t held;
         CHECK(!record_gpu_worker_d3d12_free(worker));
         CHECK(record_gpu_worker_d3d12_step(worker, &held) == RECORD_GPU_WORKER_PACKET);
         CHECK(held.data == packet.data && held.size == packet.size
               && held.slot == packet.slot && held.timestamp == packet.timestamp);
         CHECK(packet.size && packet.timestamp == received && packet.duration == 1 && packet.slot == received % 4);
         CHECK(received != 0 || packet.keyframe);
         CHECK(received != 12 || packet.keyframe);
         CHECK(received != 72 || packet.keyframe);
         CHECK(nvenc_d3d12_poll(encoder, &packet) == NVENC_D3D12_ERROR);
         CHECK(!nvenc_d3d12_free(encoder));
         CHECK(fwrite(packet.data, 1, packet.size, out) == packet.size);
         if (restart && received >= 12)
            CHECK(fwrite(packet.data, 1, packet.size, restart) == packet.size);
         if (periodic && received >= 72)
            CHECK(fwrite(packet.data, 1, packet.size, periodic) == packet.size);
         {
            const void *bytes, *held_bytes;
            size_t length, held_length;
            CHECK(record_mux_mp4_push(mux, 1, packet.data, packet.size,
                     packet.timestamp, (uint32_t)packet.duration, packet.keyframe) == RECORD_MP4_OK);
            CHECK(record_mux_mp4_get(mux, &bytes, &length));
            CHECK(record_mux_mp4_push(mux, 1, packet.data, packet.size,
                     packet.timestamp, (uint32_t)packet.duration, packet.keyframe) == RECORD_MP4_BUSY);
            CHECK(record_mux_mp4_get(mux, &held_bytes, &held_length));
            CHECK(bytes == held_bytes && length == held_length);
            CHECK(fwrite(bytes, 1, length, mp4) == length);
            CHECK(record_mux_mp4_release(mux));
         }
         CHECK(record_gpu_worker_d3d12_release(worker));
         CHECK(!record_gpu_worker_d3d12_release(worker));
         received++; progress = true;
      }
      if (progress) idle = 0;
      else { Sleep(1); idle++; }
   }
   CHECK(status == RECORD_GPU_WORKER_END && received == producer.frames && eos_fault);
   CHECK(record_gpu_worker_d3d12_step(worker, &packet) == RECORD_GPU_WORKER_END);
   CHECK(record_gpu_worker_d3d12_free(worker)); worker = NULL;
   CHECK(record_mux_mp4_finish(mux));
   CHECK(record_mux_mp4_finish(mux));
   CHECK(!nvenc_d3d12_headers(encoder, mp4_path, 1, &headers_size));
   CHECK(headers_size == 0);
   CHECK(WaitForSingleObject(producer_thread, 10000) == WAIT_OBJECT_0);
   CloseHandle(producer_thread); producer_thread = NULL;
   CHECK(producer.made == producer.frames);
   CHECK(!metadata || periodic_fault);
   if (!capturing) CHECK(record_gpu_queue_free(producer.queue));
   producer.queue = NULL;
   CHECK(nvenc_d3d12_finish(encoder) == NVENC_D3D12_OK);
   CHECK(nvenc_d3d12_finish(encoder) == NVENC_D3D12_OK);
   CHECK(nvenc_d3d12_submit(encoder, 0, 0, 1, ready, value, false) == NVENC_D3D12_ERROR);
   CHECK(nvenc_d3d12_wait_handle(encoder) != NULL);
   CHECK(WaitForSingleObject(nvenc_d3d12_wait_handle(encoder), 0) == WAIT_OBJECT_0);
   CHECK(nvenc_d3d12_free(encoder)); encoder = NULL;
   CHECK(record_gpu_capture_d3d12_free(capture)); capture = NULL;
   ok = 1;
done:
   if (producer_thread)
   {
      retro_atomic_store_release_int(&producer.stop, 1);
      if (WaitForSingleObject(producer_thread, 10000) != WAIT_OBJECT_0)
         return 1;
      CloseHandle(producer_thread);
   }
   if (producer.notify) CloseHandle(producer.notify);
   if (queue && ready)
   {
      if (capturing) ID3D12Fence_Signal(ready, value + 1);
      ID3D12CommandQueue_Signal(queue, ready, ++value);
      for (idle = 0; idle < 10000 && ID3D12Fence_GetCompletedValue(ready) < value; idle++) Sleep(1);
   }
   if (out) fclose(out);
   if (restart) fclose(restart);
   if (periodic) fclose(periodic);
   free(restart_path);
   if (mp4 && fclose(mp4) != 0) ok = 0;
   free(headers); free(mp4_path); record_mux_mp4_free(mux);
   if (encoder && nvenc_d3d12_free(encoder))
   {
      encoder = NULL;
      if (worker && record_gpu_worker_d3d12_abandon(worker)) worker = NULL;
   }
   if (producer.queue && !capturing && !worker) record_gpu_queue_free(producer.queue);
   if (capture) record_gpu_capture_d3d12_free(capture);
   for (i = 0; i < 4; i++)
   {
      if (textures[i]) ID3D12Resource_Release(textures[i]);
      if (upload[i]) ID3D12Resource_Release(upload[i]);
   }
   if (commands) ID3D12GraphicsCommandList_Release(commands);
   if (allocator) ID3D12CommandAllocator_Release(allocator);
   if (ready) ID3D12Fence_Release(ready);
   if (queue) ID3D12CommandQueue_Release(queue);
   if (device) ID3D12Device_Release(device);
   printf("codec %s: %s (%u packets)\n", argv[1], ok ? "PASS" : "FAIL", received);
   return ok == 77 ? 77 : !ok;
}
#else
#include "../../../gfx/common/nvenc_d3d12.c"
#include "../../../record/record_gpu_capture_d3d12.c"
#include "../../../record/record_gpu_worker_d3d12.c"
#include "../../../record/record_mux_mp4.c"
int main(void)
{
   nvenc_d3d12_packet_t packet;
   memset(&packet, 0x5a, sizeof(packet));
   if (record_gpu_capture_d3d12_new(NULL, NULL)
         || record_gpu_capture_d3d12_bind(NULL, NULL)
         || record_gpu_capture_d3d12_queue(NULL) || record_gpu_capture_d3d12_fence(NULL)
         || !record_gpu_capture_d3d12_free(NULL) || !record_gpu_capture_d3d12_reap(NULL)
         || record_gpu_capture_d3d12_finish(NULL)
         || record_gpu_capture_d3d12_push(NULL, NULL, 0, 0, 0, NULL, 0, 0, 1, false, NULL) != RECORD_GPU_QUEUE_ERROR)
      return 1;
   if (nvenc_d3d12_wait_handle(NULL) || nvenc_d3d12_open(NULL) || !nvenc_d3d12_free(NULL)
         || nvenc_d3d12_submit(NULL, 0, 0, 1, NULL, 0, false) != NVENC_D3D12_ERROR
         || nvenc_d3d12_poll(NULL, &packet) != NVENC_D3D12_ERROR
         || nvenc_d3d12_release(NULL) != NVENC_D3D12_ERROR
         || nvenc_d3d12_finish(NULL) != NVENC_D3D12_ERROR) return 1;
   puts("SKIP: native D3D12/NVENC needs Windows and HAVE_THREADS");
   return 77;
}
#endif
