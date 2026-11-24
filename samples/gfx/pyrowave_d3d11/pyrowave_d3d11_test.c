/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11sdklayers.h>
#include "../../../record/record_pyrowave_d3d11.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "D3D11 adapter failed at %u: %s\n", (unsigned)__LINE__, #x); return 0; } } while (0)
typedef struct test_pyrowave_d11
{
   record_pyrowave_d3d11_config_t cfg;
   ID3D11Device *device;
   ID3D11DeviceContext *context;
   record_pyrowave_d3d11_t *send, *receive;
   rpyrowave_decoder_t *cpu;
   ID3D11Texture2D *texture, *readback;
   ID3D11ShaderResourceView *view;
   float pixels[32 * 24 * 4];
} test_pyrowave_d11_t;
/* Bounded waits/readback are confined to this offline oracle. */
static int test_pyrowave_d11_progress(test_pyrowave_d11_t *t, record_pyrowave_d3d11_t *codec)
{
   unsigned i;
   ID3D11DeviceContext_Flush(t->context);
   for (i = 0; i < 5000; i++)
   {
      CHECK(record_pyrowave_d3d11_poll(codec));
      if (record_pyrowave_d3d11_idle(codec)) return 1;
      Sleep(1);
   }
   CHECK(0); return 0;
}
static int test_pyrowave_d11_case(test_pyrowave_d11_t *t, unsigned width, unsigned height, unsigned chroma)
{
   D3D11_TEXTURE2D_DESC desc;
   D3D11_SUBRESOURCE_DATA initial;
   D3D11_MAPPED_SUBRESOURCE mapped;
   record_gpu_frame_t frame;
   record_pyrowave_packet_t packet;
   record_pyrowave_d3d11_image_t image;
   rpyrowave_image_t decoded;
   ID3D11Resource *resource = NULL;
   uint64_t ticket;
   void *span;
   size_t capacity;
   unsigned i, n;
   float y = 0.2126f * 0.8f + 0.7152f * 0.2f + 0.0722f * 0.4f;
   t->cfg.sequence.width = width; t->cfg.sequence.height = height;
   t->cfg.sequence.chroma_444 = chroma;
   t->cfg.texture_width = width; t->cfg.texture_height = height;
   t->cfg.decode = false; t->send = record_pyrowave_d3d11_new(&t->cfg);
   t->cfg.decode = true; t->receive = record_pyrowave_d3d11_new(&t->cfg);
   t->cpu = rpyrowave_decoder_new(width, height, chroma, 128 * 128);
   CHECK(t->send && t->receive && t->cpu);
   for (i = 0; i < width * height; i++)
   { t->pixels[i*4] = 0.8f; t->pixels[i*4+1] = 0.2f; t->pixels[i*4+2] = 0.4f; t->pixels[i*4+3] = 1.0f; }
   memset(&desc, 0, sizeof(desc)); memset(&initial, 0, sizeof(initial));
   desc.Width = width; desc.Height = height; desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
   desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
   desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
   initial.pSysMem = t->pixels; initial.SysMemPitch = width * 16;
   CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(t->device, &desc, &initial, &t->texture)));
   CHECK(SUCCEEDED(ID3D11Device_CreateShaderResourceView(t->device, (ID3D11Resource*)t->texture, NULL, &t->view)));
   desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = 0; desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
   CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(t->device, &desc, NULL, &t->readback)));
   CHECK(record_pyrowave_d3d11_push(t->send, t->view, 0, 1000, &ticket) == RECORD_GPU_QUEUE_OK && ticket == 1);
   CHECK(!record_pyrowave_d3d11_free(t->send));
   CHECK(record_pyrowave_d3d11_push(t->send, t->view, 1000, 1000, &ticket) == RECORD_GPU_QUEUE_OK && ticket == 2);
   CHECK(record_pyrowave_d3d11_push(t->send, t->view, 2000, 1000, &ticket) == RECORD_GPU_QUEUE_BUSY);
   CHECK(test_pyrowave_d11_progress(t, t->send));
   CHECK(record_pyrowave_d3d11_source_retired(t->send) == 2);
   for (n = 0; n < 2; n++)
   {
      CHECK(record_pyrowave_d3d11_take(t->send, &frame) == RECORD_GPU_QUEUE_OK);
      CHECK(record_pyrowave_d3d11_packet(t->send, frame.slot, &packet) == RECORD_GPU_QUEUE_OK);
      CHECK(!rpyrowave_decode_frame(t->cpu, (const uint8_t*)packet.data, packet.bytes, 0, &decoded));
      CHECK(fabs(decoded.planes[0][0] - y) < 0.004);
      CHECK(fabs(decoded.planes[1][0] - (0.5 + (0.4-y)/(2.0*(1.0-0.0722)))) < 0.004);
      CHECK(fabs(decoded.planes[2][0] - (0.5 + (0.8-y)/(2.0*(1.0-0.2126)))) < 0.004);
      CHECK(record_pyrowave_d3d11_receive_acquire(t->receive, &span, &capacity) == RECORD_GPU_QUEUE_OK && capacity >= packet.bytes);
      memcpy(span, packet.data, packet.bytes);
      CHECK(record_pyrowave_d3d11_receive_submit(t->receive, packet.bytes, n*1000, 1000) == RECORD_GPU_QUEUE_OK);
      CHECK(record_pyrowave_d3d11_release(t->send, frame.slot));
      CHECK(record_pyrowave_d3d11_decode(t->receive) == RECORD_GPU_QUEUE_OK);
   }
   CHECK(test_pyrowave_d11_progress(t, t->receive));
   for (n = 0; n < 2; n++)
   {
      CHECK(record_pyrowave_d3d11_image(t->receive, &image) == RECORD_GPU_QUEUE_OK);
      CHECK(!record_pyrowave_d3d11_free(t->receive));
      ID3D11ShaderResourceView_GetResource(image.view, &resource);
      ID3D11DeviceContext_CopyResource(t->context, (ID3D11Resource*)t->readback, resource);
      ID3D11Resource_Release(resource); resource = NULL;
      CHECK(SUCCEEDED(ID3D11DeviceContext_Map(t->context, (ID3D11Resource*)t->readback, 0, D3D11_MAP_READ, 0, &mapped)));
      for (i = 0; i < height; i++)
      {
         const float *row = (const float*)((const uint8_t*)mapped.pData + i * mapped.RowPitch);
         unsigned x;
         for (x = 0; x < width; x++)
         {
            CHECK(fabs(row[x*4] - 0.8) < 0.01 && fabs(row[x*4+1] - 0.2) < 0.01 && fabs(row[x*4+2] - 0.4) < 0.01);
            CHECK(row[x*4+3] == 1.0f);
         }
      }
      ID3D11DeviceContext_Unmap(t->context, (ID3D11Resource*)t->readback, 0);
      CHECK(record_pyrowave_d3d11_present(t->receive, image.frame.slot));
   }
   CHECK(test_pyrowave_d11_progress(t, t->receive));
   CHECK(record_pyrowave_d3d11_poll(t->send));
   CHECK(record_pyrowave_d3d11_receive_acquire(t->receive, &span, &capacity) == RECORD_GPU_QUEUE_OK);
   memset(span, 0, 8);
   CHECK(record_pyrowave_d3d11_receive_submit(t->receive, 8, 2000, 1000) == RECORD_GPU_QUEUE_ERROR);
   CHECK(record_pyrowave_d3d11_receive_cancel(t->receive));
   CHECK(record_pyrowave_d3d11_finish(t->send) && record_pyrowave_d3d11_finish(t->receive));
   CHECK(record_pyrowave_d3d11_free(t->send) && record_pyrowave_d3d11_free(t->receive));
   t->send = t->receive = NULL;
   t->cfg.decode = false; t->send = record_pyrowave_d3d11_new(&t->cfg);
   CHECK(t->send);
   CHECK(record_pyrowave_d3d11_push(t->send, t->view, 0, 1000, &ticket) == RECORD_GPU_QUEUE_OK);
   CHECK(!record_pyrowave_d3d11_abandon(t->send));
   for (i = 0; i < 5000 && !record_pyrowave_d3d11_quiesce(t->send); i++) Sleep(1);
   CHECK(i < 5000);
   CHECK(record_pyrowave_d3d11_push(t->send, t->view, 1000, 1000, &ticket) == RECORD_GPU_QUEUE_ERROR);
   CHECK(record_pyrowave_d3d11_abandon(t->send)); t->send = NULL;
   rpyrowave_decoder_free(t->cpu); t->cpu = NULL;
   ID3D11ShaderResourceView_Release(t->view); ID3D11Texture2D_Release(t->texture); ID3D11Texture2D_Release(t->readback);
   t->view = NULL; t->texture = t->readback = NULL;
   return 1;
}
int main(int argc, char **argv)
{
   test_pyrowave_d11_t *t = (test_pyrowave_d11_t*)calloc(1, sizeof(*t));
   unsigned i;
   D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
   unsigned flags = getenv("PYROWAVE_D3D11_DEBUG") ? D3D11_CREATE_DEVICE_DEBUG : 0;
   ID3D11InfoQueue *info = NULL;
   if (!t || argc != 4) return EXIT_FAILURE;
   if (FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, flags, &level, 1,
               D3D11_SDK_VERSION, &t->device, NULL, &t->context))
         && FAILED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, flags, &level, 1,
               D3D11_SDK_VERSION, &t->device, NULL, &t->context))) return EXIT_FAILURE;
   if (flags && FAILED(ID3D11Device_QueryInterface(t->device, &IID_ID3D11InfoQueue, (void**)&info))) return EXIT_FAILURE;
   t->cfg.device = t->device; t->cfg.context = t->context;
   t->cfg.slots = 2; t->cfg.bit_depth = 8; t->cfg.maximum_bytes = 262144;
   for (i = 0; i < 3; i++)
   {
      FILE *f = fopen(argv[i+1], "rb");
      long bytes;
      void *data;
      if (!f || fseek(f, 0, SEEK_END) || (bytes = ftell(f)) <= 0 || fseek(f, 0, SEEK_SET)) return EXIT_FAILURE;
      data = malloc((size_t)bytes);
      if (!data || fread(data, 1, (size_t)bytes, f) != (size_t)bytes) return EXIT_FAILURE;
      fclose(f); t->cfg.shaders[i] = data; t->cfg.shader_bytes[i] = (size_t)bytes;
   }
   if (!test_pyrowave_d11_case(t, 32, 24, 0) || !test_pyrowave_d11_case(t, 31, 23, 1)) return EXIT_FAILURE;
   if (info)
   {
      UINT64 count = ID3D11InfoQueue_GetNumStoredMessages(info), index;
      for (index = 0; index < count; index++)
      {
         SIZE_T bytes = 0;
         D3D11_MESSAGE *message;
         if (FAILED(ID3D11InfoQueue_GetMessage(info, index, NULL, &bytes))) return EXIT_FAILURE;
         message = (D3D11_MESSAGE*)malloc(bytes);
         if (!message || FAILED(ID3D11InfoQueue_GetMessage(info, index, message, &bytes))) return EXIT_FAILURE;
         if (message->Severity <= D3D11_MESSAGE_SEVERITY_ERROR)
         { fprintf(stderr, "%s\n", message->pDescription); free(message); return EXIT_FAILURE; }
         free(message);
      }
      ID3D11InfoQueue_Release(info);
      puts("PASS: D3D11 debug layer reported no errors");
   }
   for (i = 0; i < 3; i++) free((void*)t->cfg.shaders[i]);
   ID3D11DeviceContext_Release(t->context); ID3D11Device_Release(t->device); free(t);
   puts("PASS: D3D11 encode/CPU decode/GPU decode, padded extents, backpressure and packet/image loans");
   return EXIT_SUCCESS;
}
#else
int main(void) { puts("D3D11 GPU oracle requires Windows or Wine"); return 0; }
#endif
