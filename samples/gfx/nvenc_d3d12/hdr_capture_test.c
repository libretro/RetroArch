/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../../../record/record_gpu_capture_d3d12.h"
#include "../../../record/record_gpu_worker_d3d12.h"
#if defined(_WIN32) && !defined(__WINRT__)
#define COBJMACROS
#ifdef __MINGW32__
#define WIDL_C_INLINE_WRAPPERS
#endif
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>

static unsigned path_mode, direct_attempts, direct_textures, plane_textures, capture_copies;
static bool p010_uav_capable;

static HRESULT capture_support(ID3D12Device *device, D3D12_FEATURE feature,
      void *data, UINT size)
{
   HRESULT result = device->lpVtbl->CheckFeatureSupport(device, feature, data, size);
   if (feature == D3D12_FEATURE_FORMAT_SUPPORT
         && ((D3D12_FEATURE_DATA_FORMAT_SUPPORT*)data)->Format == DXGI_FORMAT_P010)
   {
      D3D12_FEATURE_DATA_FORMAT_SUPPORT *support = (D3D12_FEATURE_DATA_FORMAT_SUPPORT*)data;
      p010_uav_capable = SUCCEEDED(result)
         && (support->Support1 & D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW)
         && (support->Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
      if (path_mode == 1)
         return E_FAIL;
      if (path_mode == 2 && SUCCEEDED(result))
         support->Support2 &= ~D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE;
   }
   return result;
}

static HRESULT capture_texture(ID3D12Device *device, const D3D12_HEAP_PROPERTIES *heap,
      D3D12_HEAP_FLAGS flags, const D3D12_RESOURCE_DESC *desc,
      D3D12_RESOURCE_STATES state, const D3D12_CLEAR_VALUE *clear,
      REFIID iid, void **texture)
{
   HRESULT result;
   bool direct = desc->Format == DXGI_FORMAT_P010
      && (desc->Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   if (direct)
   {
      direct_attempts++;
      if (path_mode == 3 || (path_mode == 4 && direct_attempts == 2))
      {
         *texture = NULL;
         return E_OUTOFMEMORY;
      }
   }
   result = device->lpVtbl->CreateCommittedResource(device, heap, flags,
         desc, state, clear, iid, texture);
   if (SUCCEEDED(result))
   {
      if (direct)
         direct_textures++;
      if (desc->Format == DXGI_FORMAT_R16_UNORM || desc->Format == DXGI_FORMAT_R16G16_UNORM)
         plane_textures++;
   }
   return result;
}

static void capture_copy(ID3D12GraphicsCommandList *list,
      const D3D12_TEXTURE_COPY_LOCATION *to, UINT x, UINT y, UINT z,
      const D3D12_TEXTURE_COPY_LOCATION *from, const D3D12_BOX *box)
{
   capture_copies++;
   list->lpVtbl->CopyTextureRegion(list, to, x, y, z, from, box);
}

#pragma push_macro("ID3D12Device_CheckFeatureSupport")
#pragma push_macro("ID3D12Device_CreateCommittedResource")
#pragma push_macro("ID3D12GraphicsCommandList_CopyTextureRegion")
#undef ID3D12Device_CheckFeatureSupport
#undef ID3D12Device_CreateCommittedResource
#undef ID3D12GraphicsCommandList_CopyTextureRegion
#define ID3D12Device_CheckFeatureSupport capture_support
#define ID3D12Device_CreateCommittedResource capture_texture
#define ID3D12GraphicsCommandList_CopyTextureRegion capture_copy
#include "../../../record/record_gpu_capture_d3d12.c"
#pragma pop_macro("ID3D12GraphicsCommandList_CopyTextureRegion")
#pragma pop_macro("ID3D12Device_CreateCommittedResource")
#pragma pop_macro("ID3D12Device_CheckFeatureSupport")
#define W 162
#define H 130
#define SW 168
#define SH 136
#define CHECK(x) do { if (!(x)) { printf("FAIL line %u: %s\n", (unsigned)__LINE__, #x); goto done; } } while (0)

static void rgb_at(unsigned x, unsigned y, bool scrgb, double *rgb)
{
   static const double colors[6][3] = {
      {0,0,0}, {1,1,1}, {1,0,0}, {0,1,0}, {0,0,1}, {0.5,0.25,0.75}
   };
   unsigned i, band = ((x / 24) + (y / 32)) % 6;
   for (i = 0; i < 3; i++)
      rgb[i] = colors[band][i];
   if (scrgb)
   {
      double level = y < 32 ? 1.0 : (y < 64 ? 12.5 : (y < 96 ? 125.0 : 2.0));
      for (i = 0; i < 3; i++) rgb[i] *= level;
      if (band == 5) rgb[0] = -0.5;
   }
   if (x >= 150)
      rgb[(x + y) % 3] = 0;
}
static double pq(double light)
{
   double p;
   if (light < 0) light = 0;
   if (light > 1) light = 1;
   p = pow(light, 2610.0 / 16384.0);
   return pow((3424.0/4096.0 + (2413.0/128.0)*p) / (1 + (2392.0/128.0)*p), 2523.0/32.0);
}
static void reference(unsigned x, unsigned y, bool scrgb, double *yuv)
{
   double rgb[3], wide[3];
   unsigned i;
   rgb_at(x + 3, y + 1, scrgb, rgb);
   if (scrgb)
   {
      wide[0] = .627404*rgb[0] + .329283*rgb[1] + .043313*rgb[2];
      wide[1] = .069097*rgb[0] + .919540*rgb[1] + .011362*rgb[2];
      wide[2] = .016391*rgb[0] + .088013*rgb[1] + .895595*rgb[2];
      for (i = 0; i < 3; i++) rgb[i] = pq(wide[i]*.008);
   }
   else
      for (i = 0; i < 3; i++) rgb[i] = floor(rgb[i]*1023 + .5)/1023;
   yuv[0] = .2627*rgb[0] + .6780*rgb[1] + .0593*rgb[2];
   yuv[1] = (rgb[2]-yuv[0])/1.8814;
   yuv[2] = (rgb[0]-yuv[0])/1.4746;
}
static unsigned half(double value)
{
   int exponent, sign = value < 0 ? 32768 : 0;
   double mantissa;
   if (!value) return 0;
   mantissa = frexp(fabs(value), &exponent);
   return sign | ((exponent + 14) << 10) | (unsigned)floor((mantissa*2-1)*1024+.5);
}
static bool wait_fence(ID3D12Fence *fence, uint64_t value)
{
   unsigned idle;
   for (idle = 0; idle < 10000 && ID3D12Fence_GetCompletedValue(fence) < value; idle++) Sleep(1);
   return idle < 10000 && ID3D12Fence_GetCompletedValue(fence) != UINT64_MAX;
}
int main(int argc, char **argv)
{
   ID3D12Device *device = NULL;
   ID3D12Debug *debug = NULL;
   ID3D12InfoQueue *messages = NULL;
   ID3D12CommandQueue *commands = NULL;
   ID3D12CommandAllocator *allocator = NULL;
   ID3D12GraphicsCommandList *list = NULL;
   ID3D12Resource *source = NULL, *upload = NULL, *readback = NULL;
   ID3D12Fence *fence = NULL;
   record_gpu_capture_d3d12_t *capture = NULL;
   record_gpu_worker_d3d12_t *worker = NULL;
   nvenc_d3d12_t *encoder = NULL;
   nvenc_d3d12_config_t config;
   D3D12_COMMAND_QUEUE_DESC queue_desc;
   D3D12_RESOURCE_DESC desc, buffer;
   D3D12_HEAP_PROPERTIES heap;
   D3D12_PLACED_SUBRESOURCE_FOOTPRINT input, planes[2];
   D3D12_RESOURCE_BARRIER barrier;
   D3D12_TEXTURE_COPY_LOCATION from, to;
   D3D12_RANGE range;
   UINT64 bytes, read_bytes;
   uint64_t ready = 0;
   unsigned x, y, i, frame, idle, made = 4, received = 0, failures = 0;
   bool scrgb, full;
   void *mapped = NULL;
   FILE *out = NULL, *raw = NULL;
   char *raw_path = NULL;
   HMODULE runtime;
   int result = 1, status;
   if (argc != 5 && argc != 6) { puts("usage: hdr_capture_test scrgb full codec output [path-mode]"); return 1; }
   if (argc == 6) path_mode = (unsigned)atoi(argv[5]);
   if (path_mode > 4) return 1;
   runtime = LoadLibraryExA("nvEncodeAPI64.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
   if (!runtime) { puts("SKIP: NVENC runtime unavailable"); return 77; }
   FreeLibrary(runtime);
   scrgb = atoi(argv[1]) != 0; full = atoi(argv[2]) != 0;
   if (SUCCEEDED(D3D12GetDebugInterface(&IID_ID3D12Debug, (void**)&debug)))
   {
      ID3D12Debug_EnableDebugLayer(debug);
      ID3D12Debug_Release(debug); debug = NULL;
   }
   if (FAILED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void**)&device)))
   { puts("SKIP: D3D12 device unavailable"); return 77; }
   ID3D12Device_QueryInterface(device, &IID_ID3D12InfoQueue, (void**)&messages);
   printf("D3D12 validation: %s\n", messages ? "enabled" : "unavailable");
   memset(&config, 0, sizeof(config)); memset(&queue_desc, 0, sizeof(queue_desc));
   CHECK(SUCCEEDED(ID3D12Device_CreateCommandQueue(device, &queue_desc, &IID_ID3D12CommandQueue, (void**)&commands)));
   CHECK(SUCCEEDED(ID3D12Device_CreateCommandAllocator(device, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void**)&allocator)));
   CHECK(SUCCEEDED(ID3D12Device_CreateCommandList(device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator, NULL, &IID_ID3D12GraphicsCommandList, (void**)&list)));
   CHECK(SUCCEEDED(ID3D12Device_CreateFence(device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**)&fence)));
   config.device = device; config.slots = 4; config.width = W; config.height = H;
   config.format = NVENC_D3D12_P010; config.color = NVENC_D3D12_COLOR_BT2020_PQ;
   config.full_range = full; config.codec = (enum nvenc_d3d12_codec)atoi(argv[3]);
   config.fps_num = 60; config.fps_den = 1; config.bitrate = 16000000;
   capture = record_gpu_capture_d3d12_new(&config, commands); CHECK(capture);
   if (path_mode == 0 && getenv("NVENC_TEST_REQUIRE_DIRECT_HDR")) CHECK(direct_textures == 4);
   if (path_mode >= 1 && path_mode <= 3) CHECK(!direct_textures);
   if (path_mode == 4 && p010_uav_capable) CHECK(direct_textures == 3);
   CHECK(plane_textures == 2*(4-direct_textures));
   config.full_range = !full;
   CHECK(!record_gpu_capture_d3d12_bind(capture, &config));
   config.full_range = full; config.color = NVENC_D3D12_COLOR_BT2020_HLG;
   CHECK(!record_gpu_capture_d3d12_bind(capture, &config));
   config.color = NVENC_D3D12_COLOR_BT2020_PQ;
   CHECK(record_gpu_capture_d3d12_bind(capture, &config));
   memset(&desc, 0, sizeof(desc)); memset(&heap, 0, sizeof(heap));
   heap.Type = D3D12_HEAP_TYPE_DEFAULT;
   desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; desc.Width = SW; desc.Height = SH;
   desc.DepthOrArraySize = desc.MipLevels = desc.SampleDesc.Count = 1;
   desc.Format = scrgb ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R10G10B10A2_UNORM;
   CHECK(SUCCEEDED(ID3D12Device_CreateCommittedResource(device, &heap, D3D12_HEAP_FLAG_NONE, &desc,
               D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (void**)&source)));
   ID3D12Device_GetCopyableFootprints(device, &desc, 0, 1, 0, &input, NULL, NULL, &bytes);
   memset(&buffer, 0, sizeof(buffer)); buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
   buffer.Width = bytes; buffer.Height = buffer.DepthOrArraySize = buffer.MipLevels = buffer.SampleDesc.Count = 1;
   buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; heap.Type = D3D12_HEAP_TYPE_UPLOAD;
   CHECK(SUCCEEDED(ID3D12Device_CreateCommittedResource(device, &heap, D3D12_HEAP_FLAG_NONE, &buffer,
               D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, (void**)&upload)));
   range.Begin = range.End = 0;
   CHECK(SUCCEEDED(ID3D12Resource_Map(upload, 0, &range, &mapped)));
   for (y = 0; y < SH; y++) for (x = 0; x < SW; x++)
   {
      double rgb[3];
      unsigned char *pixel = (unsigned char*)mapped + input.Offset + y*input.Footprint.RowPitch + x*(scrgb ? 8 : 4);
      rgb_at(x, y, scrgb, rgb);
      if (scrgb)
      {
         uint16_t *p = (uint16_t*)pixel;
         for (i = 0; i < 3; i++) p[i] = (uint16_t)half(rgb[i]);
         p[3] = 0x3c00;
      }
      else
         *(uint32_t*)pixel = (uint32_t)floor(rgb[0]*1023+.5) | ((uint32_t)floor(rgb[1]*1023+.5)<<10)
            | ((uint32_t)floor(rgb[2]*1023+.5)<<20) | 0xc0000000u;
   }
   ID3D12Resource_Unmap(upload, 0, NULL); mapped = NULL;
   memset(&from, 0, sizeof(from)); memset(&to, 0, sizeof(to));
   from.pResource = upload; from.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; from.PlacedFootprint = input;
   to.pResource = source;
   ID3D12GraphicsCommandList_CopyTextureRegion(list, &to, 0, 0, 0, &from, NULL);
   memset(&barrier, 0, sizeof(barrier)); barrier.Transition.pResource = source;
   barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
   barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
   barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
   ID3D12GraphicsCommandList_ResourceBarrier(list, 1, &barrier);
   CHECK(SUCCEEDED(ID3D12GraphicsCommandList_Close(list)));
   { ID3D12CommandList *command = (ID3D12CommandList*)list; ID3D12CommandQueue_ExecuteCommandLists(commands, 1, &command); }
   CHECK(SUCCEEDED(ID3D12CommandQueue_Signal(commands, fence, 1)));
   CHECK(wait_fence(fence, 1));
   CHECK(record_gpu_capture_d3d12_push(capture, source, 0, SW, 1, NULL, 0, 0, 1, false, &ready) == RECORD_GPU_QUEUE_ERROR && !ready);
   for (frame = 0; frame < 4; frame++)
      CHECK(record_gpu_capture_d3d12_push(capture, source, 0, 3, 1, NULL, 0, frame, 1, frame == 0, &ready) == RECORD_GPU_QUEUE_OK);
   CHECK(capture_copies == 2*(4-direct_textures));
   printf("HDR path %u: %u direct textures, %u intermediate planes, %u GPU copies for four frames\n",
         path_mode, direct_textures, plane_textures, capture_copies);
   CHECK(!record_gpu_capture_d3d12_free(capture));
   CHECK(wait_fence((ID3D12Fence*)record_gpu_capture_d3d12_fence(capture), ready));
   CHECK(record_gpu_capture_d3d12_reap(capture));
   CHECK(SUCCEEDED(ID3D12CommandAllocator_Reset(allocator)));
   CHECK(SUCCEEDED(ID3D12GraphicsCommandList_Reset(list, allocator, NULL)));
   desc.Width = W; desc.Height = H; desc.Format = DXGI_FORMAT_P010;
   ID3D12Device_GetCopyableFootprints(device, &desc, 0, 2, 0, planes, NULL, NULL, &read_bytes);
   buffer.Width = read_bytes*4; heap.Type = D3D12_HEAP_TYPE_READBACK;
   CHECK(SUCCEEDED(ID3D12Device_CreateCommittedResource(device, &heap, D3D12_HEAP_FLAG_NONE, &buffer,
               D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (void**)&readback)));
   for (frame = 0; frame < 4; frame++)
   {
      barrier.Transition.pResource = (ID3D12Resource*)config.textures[frame];
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON; barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
      ID3D12GraphicsCommandList_ResourceBarrier(list, 1, &barrier);
      for (i = 0; i < 2; i++)
      {
         from.pResource = (ID3D12Resource*)config.textures[frame]; from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; from.SubresourceIndex = i;
         to.pResource = readback; to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; to.PlacedFootprint = planes[i]; to.PlacedFootprint.Offset += frame*read_bytes;
         ID3D12GraphicsCommandList_CopyTextureRegion(list, &to, 0, 0, 0, &from, NULL);
      }
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE; barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
      ID3D12GraphicsCommandList_ResourceBarrier(list, 1, &barrier);
   }
   CHECK(SUCCEEDED(ID3D12GraphicsCommandList_Close(list)));
   { ID3D12CommandList *command = (ID3D12CommandList*)list; ID3D12CommandQueue_ExecuteCommandLists(commands, 1, &command); }
   CHECK(SUCCEEDED(ID3D12CommandQueue_Signal(commands, fence, 2))); CHECK(wait_fence(fence, 2));
   range.Begin = 0; range.End = (SIZE_T)buffer.Width;
   CHECK(SUCCEEDED(ID3D12Resource_Map(readback, 0, &range, &mapped)));
   raw_path = (char*)malloc(strlen(argv[4])+5); CHECK(raw_path); sprintf(raw_path, "%s.raw", argv[4]);
   raw = fopen(raw_path, "wb"); CHECK(raw);
   for (frame = 0; frame < 4; frame++) for (i = 0; i < 2; i++)
      for (y = 0; y < ((unsigned)H >> i); y++) for (x = 0; x < W; x++)
      {
         uint16_t value = ((uint16_t*)((unsigned char*)mapped + frame*read_bytes + planes[i].Offset + y*planes[i].Footprint.RowPitch))[x];
         double v[3], expected;
         unsigned code;
         if (!i)
         {
            reference(x, y, scrgb, v); expected = (full ? 0 : 64) + (full ? 1023 : 876)*v[0];
         }
         else
         {
            double total = 0;
            unsigned dx, dy, base = (x/2)*2;
            for (dy = 0; dy < 2; dy++) for (dx = 0; dx < 3; dx++)
            {
               unsigned sample = dx ? base+dx-1 : (base ? base-1 : 0);
               reference(sample, y*2+dy, scrgb, v);
               total += (dx == 1 ? 2 : 1)*v[1+(x&1)];
            }
            expected = 512 + (full ? 1023 : 896)*total*.125;
         }
         if (expected < 0) expected = 0;
         if (expected > 1023) expected = 1023;
         code = (unsigned)floor(expected+.5);
         if ((value & 63) || abs((int)(value>>6)-(int)code) > 1)
         {
            if (!failures) printf("pixel mismatch frame %u plane %u x %u y %u: %u expected %u\n",frame,i,x,y,value,code);
            failures++;
         }
         if (!frame) CHECK(fwrite(&value, 2, 1, raw) == 1);
      }
   CHECK(!failures);
   CHECK(fclose(raw) == 0); raw = NULL;
   ID3D12Resource_Unmap(readback, 0, NULL); mapped = NULL;
   encoder = nvenc_d3d12_open(&config); CHECK(encoder);
   worker = record_gpu_worker_d3d12_new(record_gpu_capture_d3d12_queue(capture), encoder, record_gpu_capture_d3d12_fence(capture)); CHECK(worker);
   out = fopen(argv[4], "wb"); CHECK(out);
   for (idle = 0; idle < 10000; idle++)
   {
      nvenc_d3d12_packet_t packet;
      if (made < 12)
      {
         status = record_gpu_capture_d3d12_push(capture, source, 0, 3, 1, NULL, 0, made, 1, false, &ready);
         CHECK(status == RECORD_GPU_QUEUE_OK || status == RECORD_GPU_QUEUE_BUSY);
         if (status == RECORD_GPU_QUEUE_OK) made++;
         if (made == 12) CHECK(record_gpu_capture_d3d12_finish(capture));
      }
      status = record_gpu_worker_d3d12_step(worker, &packet);
      CHECK(status != RECORD_GPU_WORKER_ERROR);
      if (status == RECORD_GPU_WORKER_END) break;
      if (status == RECORD_GPU_WORKER_PACKET)
      {
         CHECK(packet.timestamp == received && packet.duration == 1);
         CHECK(fwrite(packet.data, 1, packet.size, out) == packet.size);
         CHECK(record_gpu_worker_d3d12_release(worker)); received++;
      }
      else Sleep(1);
   }
   CHECK(idle < 10000 && received == 12);
   CHECK(record_gpu_worker_d3d12_free(worker)); worker = NULL;
   CHECK(nvenc_d3d12_free(encoder)); encoder = NULL;
   CHECK(record_gpu_capture_d3d12_free(capture)); capture = NULL;
   if (messages)
   {
      UINT64 count = ID3D12InfoQueue_GetNumStoredMessages(messages), n;
      for (n = 0; n < count; n++)
      {
         D3D12_MESSAGE *message;
         SIZE_T length = 0;
         CHECK(SUCCEEDED(ID3D12InfoQueue_GetMessage(messages, n, NULL, &length)));
         message = (D3D12_MESSAGE*)malloc(length); CHECK(message);
         CHECK(SUCCEEDED(ID3D12InfoQueue_GetMessage(messages, n, message, &length)));
         if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR)
         { printf("D3D12: %s\n", message->pDescription); failures++; }
         free(message);
      }
      CHECK(!failures);
   }
   puts("PASS: cropped HDR GPU planes match double reference; twelve frames encoded"); result = 0;
done:
   if (mapped && readback) ID3D12Resource_Unmap(readback, 0, NULL);
   if (raw) fclose(raw);
   if (out && fclose(out)) result = 1;
   free(raw_path);
   if (result) return result;
   if (source) ID3D12Resource_Release(source);
   if (upload) ID3D12Resource_Release(upload);
   if (readback) ID3D12Resource_Release(readback);
   if (fence) ID3D12Fence_Release(fence);
   if (list) ID3D12GraphicsCommandList_Release(list);
   if (allocator) ID3D12CommandAllocator_Release(allocator);
   if (commands) ID3D12CommandQueue_Release(commands);
   if (messages) ID3D12InfoQueue_Release(messages);
   if (device) ID3D12Device_Release(device);
   return result;
}
#else
int main(void) { puts("SKIP: HDR capture requires Windows D3D12"); return 77; }
#endif
