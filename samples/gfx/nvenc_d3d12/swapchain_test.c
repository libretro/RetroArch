/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../record/record_gpu_capture_d3d12.h"
#if defined(_WIN32) && !defined(__WINRT__)
#define COBJMACROS
#ifdef __MINGW32__
#define WIDL_C_INLINE_WRAPPERS
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#define CHECK(x) do { if (!(x)) { printf("FAIL line %u: %s\n", (unsigned)__LINE__, #x); goto done; } } while (0)

static bool exercise(ID3D12Device *device, ID3D12CommandQueue *commands,
      IDXGIFactory2 *factory, HWND window, DXGI_FORMAT format)
{
   IDXGISwapChain1 *chain = NULL;
   ID3D12Resource *source = NULL;
   ID3D12Fence *gate = NULL;
   record_gpu_capture_d3d12_t *capture = NULL;
   record_gpu_queue_t *queue = NULL;
   record_gpu_frame_t frames[4];
   nvenc_d3d12_config_t config;
   DXGI_SWAP_CHAIN_DESC1 desc;
   uint64_t value = 0;
   unsigned i, idle, held = 0;
   bool ok = false;
   memset(&config, 0, sizeof(config));
   desc.Stereo = FALSE;
   desc.SampleDesc.Quality = 0;
   desc.Scaling = DXGI_SCALING_STRETCH;
   desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
   desc.Flags = 0;
   desc.Width = 320; desc.Height = 192;
   desc.Format = format; desc.SampleDesc.Count = 1;
   desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
   desc.BufferCount = 2; desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
   CHECK(SUCCEEDED(IDXGIFactory2_CreateSwapChainForHwnd(factory,
            (IUnknown*)commands, window, &desc, NULL, NULL, &chain)));
   config.device = device; config.width = 320; config.height = 192;
   config.slots = 4; config.format = NVENC_D3D12_BGRA8;
   if (format == DXGI_FORMAT_R10G10B10A2_UNORM || format == DXGI_FORMAT_R16G16B16A16_FLOAT)
   {
      config.format = NVENC_D3D12_P010;
      config.color = NVENC_D3D12_COLOR_BT2020_PQ;
   }
   capture = record_gpu_capture_d3d12_new(&config, commands); CHECK(capture);
   queue = record_gpu_capture_d3d12_queue(capture);
   CHECK(SUCCEEDED(ID3D12Device_CreateFence(device, 0, D3D12_FENCE_FLAG_NONE,
            &IID_ID3D12Fence, (void**)&gate)));
   CHECK(SUCCEEDED(IDXGISwapChain1_GetBuffer(chain, 0, &IID_ID3D12Resource, (void**)&source)));
   for (i = 0; i < 4; i++)
   {
      CHECK(record_gpu_capture_d3d12_push(capture, source, D3D12_RESOURCE_STATE_PRESENT,
               0, 0, gate, 1, i, 1, false, &value) == RECORD_GPU_QUEUE_OK);
      CHECK(record_gpu_queue_take(queue, &frames[held]) == RECORD_GPU_QUEUE_OK);
      held++;
   }
   ID3D12Resource_Release(source); source = NULL;
   CHECK(!record_gpu_capture_d3d12_reap(capture));
   CHECK(SUCCEEDED(ID3D12Fence_Signal(gate, 1)));
   for (idle = 0; idle < 10000 && ID3D12Fence_GetCompletedValue(
            (ID3D12Fence*)record_gpu_capture_d3d12_fence(capture)) < value; idle++) Sleep(1);
   CHECK(idle < 10000);
   /* GPU completion alone leaves the capture's COM references outstanding. */
   CHECK(IDXGISwapChain1_ResizeBuffers(chain, 2, 336, 208, format, 0) == DXGI_ERROR_INVALID_CALL);
   CHECK(record_gpu_capture_d3d12_reap(capture));
   CHECK(SUCCEEDED(IDXGISwapChain1_ResizeBuffers(chain, 2, 336, 208, format, 0)));
   CHECK(!record_gpu_capture_d3d12_free(capture));
   while (held)
   {
      CHECK(record_gpu_queue_release(queue, frames[held - 1].slot));
      held--;
   }
   /* The same capture pool can crop the replacement backbuffer. */
   CHECK(SUCCEEDED(IDXGISwapChain1_GetBuffer(chain, 0, &IID_ID3D12Resource, (void**)&source)));
   CHECK(record_gpu_capture_d3d12_push(capture, source, D3D12_RESOURCE_STATE_PRESENT,
            8, 8, NULL, 0, 4, 1, false, &value) == RECORD_GPU_QUEUE_OK);
   ID3D12Resource_Release(source); source = NULL;
   CHECK(record_gpu_queue_take(queue, &frames[0]) == RECORD_GPU_QUEUE_OK); held = 1;
   CHECK(record_gpu_capture_d3d12_finish(capture));
   for (idle = 0; idle < 10000 && !record_gpu_capture_d3d12_reap(capture); idle++) Sleep(1);
   CHECK(idle < 10000);
   CHECK(SUCCEEDED(IDXGISwapChain1_ResizeBuffers(chain, 2, 352, 224, format, 0)));
   CHECK(record_gpu_queue_release(queue, frames[0].slot)); held = 0;
   CHECK(record_gpu_capture_d3d12_free(capture)); capture = NULL;
   ok = true;
done:
   if (gate) ID3D12Fence_Signal(gate, 1);
   if (capture)
   {
      for (i = 0; i < held; i++) record_gpu_queue_release(queue, frames[i].slot);
      record_gpu_capture_d3d12_finish(capture);
      for (idle = 0; idle < 10000 && !record_gpu_capture_d3d12_free(capture); idle++) Sleep(1);
      if (idle == 10000) return false;
   }
   if (source) ID3D12Resource_Release(source);
   if (gate) ID3D12Fence_Release(gate);
   if (chain) IDXGISwapChain1_Release(chain);
   return ok;
}

int main(void)
{
   ID3D12Device *device = NULL;
   ID3D12CommandQueue *commands = NULL;
   IDXGIFactory2 *factory = NULL;
   D3D12_COMMAND_QUEUE_DESC desc;
   HWND window = NULL;
   nvenc_d3d12_config_t config;
   record_gpu_capture_d3d12_t *probe = NULL;
   int result = 1;
   if (FAILED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0,
               &IID_ID3D12Device, (void**)&device)))
   {
      puts("SKIP: D3D12 device unavailable");
      return 77;
   }
   desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
   desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
   desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
   desc.NodeMask = 0;
   CHECK(SUCCEEDED(ID3D12Device_CreateCommandQueue(device, &desc, &IID_ID3D12CommandQueue, (void**)&commands)));
   CHECK(SUCCEEDED(CreateDXGIFactory1(&IID_IDXGIFactory2, (void**)&factory)));
   window = CreateWindowExA(0, "STATIC", "Capture resize test", WS_OVERLAPPEDWINDOW,
         0, 0, 400, 300, NULL, NULL, GetModuleHandle(NULL), NULL);
   CHECK(window);
   CHECK(exercise(device, commands, factory, window, DXGI_FORMAT_B8G8R8A8_UNORM));
   CHECK(exercise(device, commands, factory, window, DXGI_FORMAT_R8G8B8A8_UNORM));
   memset(&config, 0, sizeof(config));
   config.device = device; config.slots = 4; config.width = 320; config.height = 192;
   config.format = NVENC_D3D12_P010; config.color = NVENC_D3D12_COLOR_BT2020_PQ;
   probe = record_gpu_capture_d3d12_new(&config, commands);
   if (probe)
   {
      CHECK(record_gpu_capture_d3d12_free(probe)); probe = NULL;
      CHECK(exercise(device, commands, factory, window, DXGI_FORMAT_R10G10B10A2_UNORM));
      CHECK(exercise(device, commands, factory, window, DXGI_FORMAT_R16G16B16A16_FLOAT));
   }
   else
   {
      CHECK(!getenv("NVENC_TEST_REQUIRE_HW"));
      puts("SKIP: HDR conversion unavailable; SDR resize checked");
   }
   puts("PASS: SDR/HDR swapchain resize with encoder destination leases held");
   result = 0;
done:
   if (probe) record_gpu_capture_d3d12_free(probe);
   if (factory) IDXGIFactory2_Release(factory);
   if (commands) ID3D12CommandQueue_Release(commands);
   if (device) ID3D12Device_Release(device);
   if (window) DestroyWindow(window);
   return result;
}
#else
int main(void)
{
   puts("SKIP: DXGI swapchain requires native Windows");
   return 77;
}
#endif
