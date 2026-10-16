/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../gfx/common/nvenc_d3d12.h"
#ifdef _WIN32
#define COBJMACROS
#ifdef __MINGW32__
#define WIDL_C_INLINE_WRAPPERS
#endif
#include <windows.h>
#include <d3d12.h>
#include "../../../gfx/common/nvenc_d3d12.c"
#define W 320
#define H 192
#define FRAMES 24
#define CHECK(x) do { if (!(x)) { printf("FAIL line %u: %s\n", (unsigned)__LINE__, #x); goto done; } } while (0)

static PNVENCENCODEPICTURE real_encode;
static PNVENCLOCKBITSTREAM real_lock;
static unsigned encode_faults, lock_faults;
static NVENCSTATUS NVENCAPI busy_encode(void *session, NV_ENC_PIC_PARAMS *params)
{
   if (encode_faults) { encode_faults--; return NV_ENC_ERR_ENCODER_BUSY; }
   return real_encode(session, params);
}
static NVENCSTATUS NVENCAPI busy_lock(void *session, NV_ENC_LOCK_BITSTREAM *params)
{
   if (lock_faults) { lock_faults--; return NV_ENC_ERR_LOCK_BUSY; }
   return real_lock(session, params);
}

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
   nvenc_d3d12_packet_t packet;
   FILE *out = NULL;
   unsigned i, x, y, submitted = 0, received = 0, idle = 0;
   int status, ok = 0;
   uint64_t value = 0;
   unsigned format;
   HMODULE runtime;
   memset(&config, 0, sizeof(config));
   if (argc != 4) { puts("usage: device_test codec format output"); return 1; }
   format = (unsigned)atoi(argv[2]);
   if (format < NVENC_D3D12_BGRA8 || format > NVENC_D3D12_P010) return 1;
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
      desc.Width = W; desc.Height = H;
      desc.DepthOrArraySize = desc.MipLevels = desc.SampleDesc.Count = 1;
      desc.Format = format == NVENC_D3D12_P010 ? DXGI_FORMAT_P010
         : format == NVENC_D3D12_NV12 ? DXGI_FORMAT_NV12 : DXGI_FORMAT_B8G8R8A8_UNORM;
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
         for (y = 0; y < (plane ? H / 2 : H); y++)
            for (x = 0; x < W; x++)
            {
               uint8_t *p = map + footprints[plane].Offset + y * footprints[plane].Footprint.RowPitch;
               if (format == NVENC_D3D12_BGRA8)
               {
                  p += x * 4;
                  p[0] = (uint8_t)(24 + i * 48); p[1] = (uint8_t)(32 + (x / 40) * 24);
                  p[2] = (uint8_t)(32 + (y / 24) * 24); p[3] = 255;
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
   {
      void *saved = config.textures[1];
      unsigned saved_den = config.fps_den;
      config.textures[1] = config.textures[0];
      CHECK(nvenc_d3d12_open(&config) == NULL);
      config.textures[1] = saved;
      config.fps_den = 0;
      CHECK(nvenc_d3d12_open(&config) == NULL);
      config.fps_den = saved_den;
   }
   encoder = nvenc_d3d12_open(&config);
   if (!encoder && !getenv("NVENC_TEST_REQUIRE_HW")) { puts("SKIP: codec/format/session unavailable"); ok = 77; goto done; }
   CHECK(encoder != NULL);
   real_encode = encoder->api.nvEncEncodePicture;
   real_lock = encoder->api.nvEncLockBitstream;
   encoder->api.nvEncEncodePicture = busy_encode;
   encoder->api.nvEncLockBitstream = busy_lock;
   encode_faults = 1;
   CHECK(nvenc_d3d12_submit(encoder, 0, 999, 1, ready, 1, false) == NVENC_D3D12_BUSY);
   CHECK(encoder->count == 0 && encoder->frame == 0 && !encoder->slots[0].busy && encoder->slots[0].value == 0);
   for (i = 0; i < 4; i++) { ID3D12Resource_Release(textures[i]); textures[i] = NULL; }
   lock_faults = 1;
   out = fopen(argv[3], "wb"); CHECK(out != NULL);
   CHECK(nvenc_d3d12_submit(encoder, 0, 0, 1, NULL, 1, false) == NVENC_D3D12_ERROR);
   CHECK(nvenc_d3d12_release(encoder) == NVENC_D3D12_ERROR);
   CHECK(nvenc_d3d12_submit(encoder, 4, 0, 1, ready, 1, false) == NVENC_D3D12_ERROR);
   CHECK(nvenc_d3d12_submit(encoder, 0, 0, 1, ready, 1, true) == NVENC_D3D12_OK);
   CHECK(!nvenc_d3d12_free(encoder));
   Sleep(30);
   CHECK(nvenc_d3d12_poll(encoder, &packet) == NVENC_D3D12_BUSY);
   for (i = 1; i < 4; i++)
      CHECK(nvenc_d3d12_submit(encoder, i, i, 1, ready, 1, false) == NVENC_D3D12_OK);
   CHECK(encoder->count == 4);
   CHECK(nvenc_d3d12_submit(encoder, 0, 999, 1, ready, 1, false) == NVENC_D3D12_BUSY);
   CHECK(SUCCEEDED(ID3D12CommandQueue_Signal(queue, ready, ++value)));
   submitted = 4;
   while (received < FRAMES && idle < 10000)
   {
      if (submitted < FRAMES)
      {
         status = nvenc_d3d12_submit(encoder, submitted % 4, submitted, 1, ready, value,
               submitted == 12);
         CHECK(status == NVENC_D3D12_OK || status == NVENC_D3D12_BUSY);
         if (status == NVENC_D3D12_OK) submitted++;
      }
      if (submitted == FRAMES && !encoder->finishing)
      {
         encode_faults = 1;
         CHECK(nvenc_d3d12_finish(encoder) == NVENC_D3D12_BUSY);
         CHECK(!encoder->finishing);
         CHECK(nvenc_d3d12_finish(encoder) == NVENC_D3D12_OK);
         CHECK(!nvenc_d3d12_free(encoder));
      }
      status = nvenc_d3d12_poll(encoder, &packet);
      CHECK(status == NVENC_D3D12_OK || status == NVENC_D3D12_BUSY);
      if (status == NVENC_D3D12_OK)
      {
         CHECK(packet.size && packet.timestamp == received && packet.duration == 1 && packet.slot == received % 4);
         CHECK(received != 0 || packet.keyframe);
         CHECK(received != 12 || packet.keyframe);
         CHECK(nvenc_d3d12_poll(encoder, &packet) == NVENC_D3D12_ERROR);
         CHECK(!nvenc_d3d12_free(encoder));
         CHECK(fwrite(packet.data, 1, packet.size, out) == packet.size);
         CHECK(nvenc_d3d12_release(encoder) == NVENC_D3D12_OK);
         received++; idle = 0;
      }
      else { Sleep(1); idle++; }
   }
   CHECK(received == FRAMES);
   CHECK(nvenc_d3d12_finish(encoder) == NVENC_D3D12_OK);
   CHECK(nvenc_d3d12_finish(encoder) == NVENC_D3D12_OK);
   CHECK(nvenc_d3d12_submit(encoder, 0, 0, 1, ready, value, false) == NVENC_D3D12_ERROR);
   for (idle = 0; idle < 10000; idle++)
   {
      status = nvenc_d3d12_poll(encoder, &packet);
      if (status != NVENC_D3D12_BUSY) break;
      Sleep(1);
   }
   CHECK(status == NVENC_D3D12_END);
   CHECK(nvenc_d3d12_free(encoder)); encoder = NULL;
   ok = 1;
done:
   if (queue && ready)
   {
      ID3D12CommandQueue_Signal(queue, ready, ++value);
      for (idle = 0; idle < 10000 && ID3D12Fence_GetCompletedValue(ready) < value; idle++) Sleep(1);
   }
   if (out) fclose(out);
   if (encoder) nvenc_d3d12_free(encoder);
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
int main(void)
{
   nvenc_d3d12_packet_t packet;
   memset(&packet, 0x5a, sizeof(packet));
   if (nvenc_d3d12_open(NULL) || !nvenc_d3d12_free(NULL)
         || nvenc_d3d12_submit(NULL, 0, 0, 1, NULL, 0, false) != NVENC_D3D12_ERROR
         || nvenc_d3d12_poll(NULL, &packet) != NVENC_D3D12_ERROR
         || nvenc_d3d12_release(NULL) != NVENC_D3D12_ERROR
         || nvenc_d3d12_finish(NULL) != NVENC_D3D12_ERROR) return 1;
   puts("SKIP: native D3D12/NVENC needs Windows");
   return 77;
}
#endif
