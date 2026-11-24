/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../libretro-common/samples/formats/pyrowave/gpu_d3d12_context.h"
#if defined(_WIN32)
#include "../../../record/record_pyrowave_d3d12.h"
#ifdef PYROWAVE_PLAYER_FACTORY
#include "pyrowave_d3d12_player_test.h"
#endif

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "D3D12 adapter failed line %u: %s\n", \
   (unsigned)__LINE__, #x); return 0; } } while (0)

typedef struct test_pyrowave_d3d12
{
   ID3D12Device *device;
   ID3D12InfoQueue *info;
   ID3D12CommandQueue *queue;
   ID3D12CommandAllocator *allocators[4];
   ID3D12GraphicsCommandList *lists[4];
   ID3D12Fence *fences[4];
   ID3D12Resource *source, *upload, *readback;
   record_pyrowave_d3d12_t *send, *receive;
   rpyrowave_decoder_t *cpu;
   record_pyrowave_d3d12_config_t cfg;
   D3D12_PLACED_SUBRESOURCE_FOOTPRINT source_footprint, output_footprint;
   HANDLE event;
   uint64_t values[4];
   bool open[4];
   ID3D12Fence fake_fence;
   ID3D12FenceVtbl fake_vtable;
   unsigned width, height, mode;
} test_pyrowave_d3d12_t;

static uint64_t test_pyrowave_d3d12_completion;

static UINT64 STDMETHODCALLTYPE test_pyrowave_d3d12_completed(ID3D12Fence *fence)
{
   (void)fence;
   return test_pyrowave_d3d12_completion;
}

static int test_pyrowave_d3d12_begin(test_pyrowave_d3d12_t *t, unsigned index)
{
   CHECK(SUCCEEDED(ID3D12CommandAllocator_Reset(t->allocators[index])));
   CHECK(SUCCEEDED(ID3D12GraphicsCommandList_Reset(t->lists[index], t->allocators[index], NULL)));
   t->values[index]++;
   t->open[index] = true;
   return 1;
}

static HRESULT test_pyrowave_d3d12_close(test_pyrowave_d3d12_t *t, unsigned index)
{
   HRESULT result = ID3D12GraphicsCommandList_Close(t->lists[index]);
   if (SUCCEEDED(result)) t->open[index] = false;
   return result;
}

static int test_pyrowave_d3d12_submit(test_pyrowave_d3d12_t *t, unsigned index)
{
   /* Offline oracle only; the shipping adapter never executes or waits. */
   t->open[index] = false;
   return rpy_d3d_submit(t->queue, t->lists[index], t->fences[index], t->event, t->values[index]);
}

static int test_pyrowave_d3d12_quiesce(test_pyrowave_d3d12_t *t)
{
   UINT64 completed, value;
   if (!t->queue || !t->fences[3] || !t->event) return 1;
   if (FAILED(ID3D12Device_GetDeviceRemovedReason(t->device))) return 1;
   value = ++t->values[3];
   if (FAILED(ID3D12CommandQueue_Signal(t->queue, t->fences[3], value)))
      return FAILED(ID3D12Device_GetDeviceRemovedReason(t->device));
   completed = ID3D12Fence_GetCompletedValue(t->fences[3]);
   if (completed != (UINT64)-1 && completed < value)
   {
      if (FAILED(ID3D12Fence_SetEventOnCompletion(t->fences[3], value, t->event))
            || WaitForSingleObject(t->event, 3000) != WAIT_OBJECT_0)
         return FAILED(ID3D12Device_GetDeviceRemovedReason(t->device));
      completed = ID3D12Fence_GetCompletedValue(t->fences[3]);
   }
   return (completed != (UINT64)-1 && completed >= value)
      || FAILED(ID3D12Device_GetDeviceRemovedReason(t->device));
}

static int test_pyrowave_d3d12_source(test_pyrowave_d3d12_t *t)
{
   D3D12_HEAP_PROPERTIES heap;
   D3D12_RESOURCE_DESC desc;
   D3D12_RANGE range;
   D3D12_TEXTURE_COPY_LOCATION from, to;
   UINT64 bytes;
   UINT rows;
   UINT64 row_bytes;
   uint8_t *mapped;
   unsigned x, y;
   memset(&heap, 0, sizeof(heap)); memset(&desc, 0, sizeof(desc));
   heap.Type = D3D12_HEAP_TYPE_DEFAULT; heap.CreationNodeMask = heap.VisibleNodeMask = 1;
   desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; desc.Width = t->width + 4; desc.Height = t->height + 6;
   desc.DepthOrArraySize = desc.MipLevels = 1; desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; desc.SampleDesc.Count = 1;
   CHECK(SUCCEEDED(ID3D12Device_CreateCommittedResource(t->device, &heap, D3D12_HEAP_FLAG_NONE, &desc,
               D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (void**)&t->source)));
   ID3D12Device_GetCopyableFootprints(t->device, &desc, 0, 1, 0, &t->source_footprint, &rows, &row_bytes, &bytes);
   t->upload = rpy_d3d_buffer(t->device, (size_t)bytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE);
   desc.Width = t->width; desc.Height = t->height;
   ID3D12Device_GetCopyableFootprints(t->device, &desc, 0, 1, 0, &t->output_footprint, &rows, &row_bytes, &bytes);
   t->readback = rpy_d3d_buffer(t->device, (size_t)bytes, D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE);
   CHECK(t->upload && t->readback);
   range.Begin = range.End = 0;
   CHECK(SUCCEEDED(ID3D12Resource_Map(t->upload, 0, &range, (void**)&mapped)));
   for (y = 0; y < t->height + 6; y++)
      for (x = 0; x < t->width + 4; x++)
      {
         float *pixel = (float*)(mapped + y * t->source_footprint.Footprint.RowPitch + x * 16);
         bool inside = x >= 2 && x < t->width + 2 && y >= 3 && y < t->height + 3;
         pixel[0] = inside ? (t->mode ? 20.756137525f : 0.8f) : 0.01f;
         pixel[1] = inside ? (t->mode ? -1.556880938f : 0.2f) : 0.01f;
         pixel[2] = inside ? (t->mode ? -0.226884538f : 0.05f) : 0.01f;
         pixel[3] = 0.25f;
      }
   ID3D12Resource_Unmap(t->upload, 0, NULL);
   CHECK(test_pyrowave_d3d12_begin(t, 3));
   memset(&from, 0, sizeof(from)); memset(&to, 0, sizeof(to));
   from.pResource = t->upload; from.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; from.PlacedFootprint = t->source_footprint;
   to.pResource = t->source; to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
   ID3D12GraphicsCommandList_CopyTextureRegion(t->lists[3], &to, 0, 0, 0, &from, NULL);
   rpy_d3d_transition(t->lists[3], t->source, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   CHECK(test_pyrowave_d3d12_submit(t, 3));
   return 1;
}

static int test_pyrowave_d3d12_pixels(test_pyrowave_d3d12_t *t, const record_pyrowave_d3d12_image_t *image)
{
   D3D12_TEXTURE_COPY_LOCATION from, to;
   D3D12_RANGE range;
   uint8_t *mapped;
   unsigned x, y, component;
   double expected[4];
   bool late = (image->frame.ready_value & 1) != 0;
   CHECK(image->texture_mode == t->mode && image->state == D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   expected[0] = t->mode ? 20.756137525 : 0.8;
   expected[1] = t->mode ? -1.556880938 : 0.2;
   expected[2] = t->mode ? -0.226884538 : 0.05; expected[3] = 1;
   CHECK(test_pyrowave_d3d12_begin(t, 3));
   rpy_d3d_transition(t->lists[3], image->resource, image->state, D3D12_RESOURCE_STATE_COPY_SOURCE);
   memset(&from, 0, sizeof(from)); memset(&to, 0, sizeof(to));
   from.pResource = image->resource; from.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
   to.pResource = t->readback; to.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; to.PlacedFootprint = t->output_footprint;
   ID3D12GraphicsCommandList_CopyTextureRegion(t->lists[3], &to, 0, 0, 0, &from, NULL);
   rpy_d3d_transition(t->lists[3], image->resource, D3D12_RESOURCE_STATE_COPY_SOURCE, image->state);
   if (!late) CHECK(record_pyrowave_d3d12_present(t->receive, image->frame.slot, t->fences[3], t->values[3]));
   CHECK(!record_pyrowave_d3d12_free(t->receive));
   CHECK(test_pyrowave_d3d12_submit(t, 3));
   if (late) CHECK(record_pyrowave_d3d12_present(t->receive, image->frame.slot, t->fences[3], t->values[3]));
   CHECK(record_pyrowave_d3d12_poll(t->receive));
   range.Begin = 0; range.End = (SIZE_T)t->output_footprint.Footprint.RowPitch * (t->height - 1) + t->width * 16;
   CHECK(SUCCEEDED(ID3D12Resource_Map(t->readback, 0, &range, (void**)&mapped)));
   for (y = 0; y < t->height; y++)
      for (x = 0; x < t->width; x++)
      {
         const float *pixel = (const float*)(mapped + y * t->output_footprint.Footprint.RowPitch + x * 16);
         for (component = 0; component < 4; component++)
         {
            double tolerance = t->mode ? 0.02 + fabs(expected[component]) * 0.003 : 0.003;
            CHECK(fabs(pixel[component] - expected[component]) < tolerance);
         }
      }
   range.End = 0; ID3D12Resource_Unmap(t->readback, 0, &range);
   return 1;
}

static uint32_t test_pyrowave_d3d12_hash(const record_pyrowave_packet_t *packet)
{
   size_t i;
   uint32_t hash = 2166136261U;
   for (i = 0; i < packet->bytes; i++) hash = (hash ^ ((const uint8_t*)packet->data)[i]) * 16777619U;
   return hash;
}

static int test_pyrowave_d3d12_packet(test_pyrowave_d3d12_t *t,
      const record_pyrowave_packet_t *packet, uint64_t ticket)
{
   rpyrowave_image_t image;
   double code = t->mode ? 0.751827096247 : 0.0;
   double luma = t->mode ? code * 0.2627 : 0.8 * 0.2126 + 0.2 * 0.7152 + 0.05 * 0.0722;
   double center = t->mode ? 512.0 / 1023.0 : 128.0 / 255.0;
   double cb = center + ((t->mode ? 0.0 : 0.05) - luma) / (t->mode ? 1.8814 : 1.8556);
   double cr = center + ((t->mode ? code : 0.8) - luma) / (t->mode ? 1.4746 : 1.5748);
   CHECK(!rpyrowave_decode_frame(t->cpu, packet->data, packet->bytes, 0, &image));
   CHECK(image.sequence.sequence == (ticket & 7) && image.sequence.transfer_pq == t->mode
         && image.sequence.primaries_bt2020 == t->mode);
   CHECK(fabs(image.planes[0][0] - luma) < 0.003 && fabs(image.planes[1][0] - cb) < 0.003
         && fabs(image.planes[2][0] - cr) < 0.003);
   return 1;
}

#include "pyrowave_d3d12_session_test.h"

static int test_pyrowave_d3d12_run(test_pyrowave_d3d12_t *t, unsigned iterations)
{
   static const uint8_t missing_sign[12] = { 1, 0, 3, 0, 128, 0, 0, 0, 1, 0, 0, 255 };
   record_gpu_frame_t held, frame;
   record_pyrowave_packet_t old, packet;
   record_pyrowave_d3d12_image_t image;
   void *span;
   size_t capacity;
   uint64_t ticket = 0, rejected;
   uint32_t hash;
   unsigned i;
   memset(&held, 0, sizeof(held));
   t->cfg.sequence.width = t->width; t->cfg.sequence.height = t->height;
   t->cfg.sequence.chroma_444 = ((t->width | t->height) & 1) != 0;
   t->cfg.sequence.primaries_bt2020 = t->cfg.sequence.transfer_pq = t->cfg.sequence.transform_bt2020 = t->mode;
   t->cfg.texture_width = t->width + 4; t->cfg.texture_height = t->height + 6;
   t->cfg.x = 2; t->cfg.y = 3; t->cfg.texture_mode = t->mode; t->cfg.bit_depth = t->mode ? 10 : 8;
   t->cfg.slots = 3; t->cfg.decode = false;
   t->send = record_pyrowave_d3d12_new(&t->cfg);
   t->cfg.slots = 1; t->cfg.decode = true;
   t->receive = record_pyrowave_d3d12_new(&t->cfg);
   t->cpu = rpyrowave_decoder_new(t->width, t->height, t->cfg.sequence.chroma_444, (size_t)(t->width + 127) * (t->height + 127));
   CHECK(t->send && t->receive && t->cpu && test_pyrowave_d3d12_source(t));
   CHECK(record_pyrowave_d3d12_idle(t->receive));
   CHECK(record_pyrowave_d3d12_texture(t->receive, 0));
   CHECK(!record_pyrowave_d3d12_texture(t->receive, 1));
   CHECK(!record_pyrowave_d3d12_texture(t->send, 0));
   for (i = 0; i < 3; i++)
   {
      CHECK(test_pyrowave_d3d12_begin(t, i));
      CHECK(record_pyrowave_d3d12_push(t->send, t->lists[i], t->fences[i], t->values[i], t->source,
               i * (uint64_t)16667, 16667, &ticket) == RECORD_GPU_QUEUE_OK);
      CHECK(record_pyrowave_d3d12_take(t->send, &frame) == RECORD_GPU_QUEUE_OK);
      CHECK(record_pyrowave_d3d12_packet(t->send, frame.slot, &packet) == RECORD_GPU_QUEUE_BUSY);
      if (!i) held = frame;
      else
      {
         CHECK(SUCCEEDED(test_pyrowave_d3d12_close(t, i)));
         CHECK(test_pyrowave_d3d12_begin(t, i));
         CHECK(SUCCEEDED(test_pyrowave_d3d12_close(t, i)));
         CHECK(record_pyrowave_d3d12_discard(t->send, t->fences[i], t->values[i] - 1));
         CHECK(record_pyrowave_d3d12_packet(t->send, frame.slot, &packet) == RECORD_GPU_QUEUE_ERROR);
         CHECK(record_pyrowave_d3d12_release(t->send, frame.slot));
         CHECK(!record_pyrowave_d3d12_source_retired(t->send));
      }
   }
   CHECK(test_pyrowave_d3d12_begin(t, 1)); rejected = ticket;
   CHECK(record_pyrowave_d3d12_push(t->send, t->lists[1], t->fences[1], t->values[1], t->source,
            0, 16667, &rejected) == RECORD_GPU_QUEUE_BUSY && rejected == ticket);
   CHECK(SUCCEEDED(test_pyrowave_d3d12_close(t, 1)));
   CHECK(test_pyrowave_d3d12_submit(t, 0) && record_pyrowave_d3d12_poll(t->send));
   CHECK(record_pyrowave_d3d12_source_retired(t->send) == 3);
   CHECK(record_pyrowave_d3d12_packet(t->send, held.slot, &old) == RECORD_GPU_QUEUE_OK);
   hash = test_pyrowave_d3d12_hash(&old);
   for (i = 0; i < iterations; i++)
   {
      CHECK(test_pyrowave_d3d12_begin(t, 1));
      CHECK(record_pyrowave_d3d12_push(t->send, t->lists[1], t->fences[1], t->values[1], t->source,
               ticket * 16667, 16667, &ticket) == RECORD_GPU_QUEUE_OK);
      CHECK(record_pyrowave_d3d12_take(t->send, &frame) == RECORD_GPU_QUEUE_OK);
      CHECK(record_pyrowave_d3d12_packet(t->send, frame.slot, &packet) == RECORD_GPU_QUEUE_BUSY);
      CHECK(test_pyrowave_d3d12_submit(t, 1) && record_pyrowave_d3d12_poll(t->send));
      CHECK(record_pyrowave_d3d12_packet(t->send, frame.slot, &packet) == RECORD_GPU_QUEUE_OK);
      CHECK(test_pyrowave_d3d12_packet(t, &packet, ticket));
      CHECK(test_pyrowave_d3d12_hash(&old) == hash);
      CHECK(record_pyrowave_d3d12_receive_acquire(t->receive, &span, &capacity) == RECORD_GPU_QUEUE_OK && capacity >= packet.bytes);
      memcpy(span, packet.data, packet.bytes);
      if (!i)
      {
         ((uint8_t*)span)[7] ^= 0x10;
         CHECK(record_pyrowave_d3d12_receive_submit(t->receive, packet.bytes, frame.timestamp, frame.duration) == RECORD_GPU_QUEUE_ERROR);
         CHECK(!record_pyrowave_d3d12_finish(t->receive));
         ((uint8_t*)span)[7] ^= 0x10;
      }
      CHECK(record_pyrowave_d3d12_receive_submit(t->receive, packet.bytes, frame.timestamp, frame.duration) == RECORD_GPU_QUEUE_OK);
      CHECK(test_pyrowave_d3d12_begin(t, 2));
      CHECK(record_pyrowave_d3d12_decode(t->receive, t->lists[2], t->fences[2], t->values[2]) == RECORD_GPU_QUEUE_OK);
      CHECK(!record_pyrowave_d3d12_idle(t->receive));
      CHECK(record_pyrowave_d3d12_image(t->receive, &image) == RECORD_GPU_QUEUE_BUSY);
      CHECK(test_pyrowave_d3d12_submit(t, 2) && record_pyrowave_d3d12_poll(t->receive));
      CHECK(record_pyrowave_d3d12_idle(t->receive));
      CHECK(record_pyrowave_d3d12_image(t->receive, &image) == RECORD_GPU_QUEUE_OK);
      CHECK(image.resource == record_pyrowave_d3d12_texture(t->receive, image.frame.slot));
      CHECK(record_pyrowave_d3d12_receive_acquire(t->receive, &span, &capacity) == RECORD_GPU_QUEUE_BUSY);
      CHECK(test_pyrowave_d3d12_pixels(t, &image));
      CHECK(record_pyrowave_d3d12_release(t->send, frame.slot));
   }
   CHECK(record_pyrowave_d3d12_receive_acquire(t->receive, &span, &capacity) == RECORD_GPU_QUEUE_OK);
   memcpy(span, old.data, 8);
   ((uint8_t*)span)[3] &= 0x8f;
   ((uint8_t*)span)[4] = 1;
   ((uint8_t*)span)[5] = ((uint8_t*)span)[6] = 0;
   memcpy((uint8_t*)span + 8, missing_sign, sizeof(missing_sign));
   CHECK(record_pyrowave_d3d12_receive_submit(t->receive, 20, 0, 16667) == RECORD_GPU_QUEUE_OK);
   CHECK(test_pyrowave_d3d12_begin(t, 2));
   CHECK(record_pyrowave_d3d12_decode(t->receive, t->lists[2], t->fences[2], t->values[2]) == RECORD_GPU_QUEUE_OK);
   CHECK(test_pyrowave_d3d12_submit(t, 2));
   CHECK(!record_pyrowave_d3d12_poll(t->receive));
   CHECK(record_pyrowave_d3d12_image(t->receive, &image) == RECORD_GPU_QUEUE_BUSY);
   CHECK(record_pyrowave_d3d12_receive_acquire(t->receive, &span, &capacity) == RECORD_GPU_QUEUE_OK);
   memcpy(span, old.data, old.bytes);
   CHECK(record_pyrowave_d3d12_receive_submit(t->receive, old.bytes, 0, 16667) == RECORD_GPU_QUEUE_OK);
   CHECK(record_pyrowave_d3d12_receive_drop(t->receive, &frame) == RECORD_GPU_QUEUE_OK);
   CHECK(record_pyrowave_d3d12_finish(t->send) && record_pyrowave_d3d12_finish(t->receive));
   CHECK(record_pyrowave_d3d12_take(t->send, &frame) == RECORD_GPU_QUEUE_END);
   CHECK(!record_pyrowave_d3d12_free(t->send));
   CHECK(record_pyrowave_d3d12_release(t->send, held.slot));
   CHECK(record_pyrowave_d3d12_free(t->send)); t->send = NULL;
   CHECK(record_pyrowave_d3d12_free(t->receive)); t->receive = NULL;
   t->cfg.decode = false;
   t->send = record_pyrowave_d3d12_new(&t->cfg);
   CHECK(t->send && test_pyrowave_d3d12_begin(t, 0));
   t->fake_vtable.GetCompletedValue = test_pyrowave_d3d12_completed;
   t->fake_fence.lpVtbl = &t->fake_vtable;
   test_pyrowave_d3d12_completion = 0;
   CHECK(record_pyrowave_d3d12_push(t->send, t->lists[0], &t->fake_fence, 1, t->source,
            0, 16667, &ticket) == RECORD_GPU_QUEUE_OK);
   CHECK(record_pyrowave_d3d12_take(t->send, &frame) == RECORD_GPU_QUEUE_OK);
   test_pyrowave_d3d12_completion = (uint64_t)-1;
   CHECK(!record_pyrowave_d3d12_poll(t->send));
   CHECK(!record_pyrowave_d3d12_source_retired(t->send));
   CHECK(!record_pyrowave_d3d12_free(t->send));
   CHECK(record_pyrowave_d3d12_packet(t->send, frame.slot, &packet) == RECORD_GPU_QUEUE_BUSY);
   CHECK(!record_pyrowave_d3d12_release(t->send, frame.slot));
   CHECK(SUCCEEDED(test_pyrowave_d3d12_close(t, 0)));
   CHECK(test_pyrowave_d3d12_begin(t, 0));
   CHECK(SUCCEEDED(test_pyrowave_d3d12_close(t, 0)));
   CHECK(record_pyrowave_d3d12_abandon(t->send)); t->send = NULL;
   CHECK(test_pyrowave_d3d12_session(t));
   CHECK(rpy_d3d_messages(t->info));
   printf("D3D12 async %ux%u mode%u: %u frames, source/display fences, immutable packets PASS\n", t->width, t->height, t->mode, iterations);
   return 1;
}

static void test_pyrowave_d3d12_case_free(test_pyrowave_d3d12_t *t)
{
   unsigned i;
   for (i = 0; i < 4; i++) if (t->lists[i])
   {
      if (t->open[i]) ID3D12GraphicsCommandList_Close(t->lists[i]);
      ID3D12GraphicsCommandList_Reset(t->lists[i], t->allocators[i], NULL);
      ID3D12GraphicsCommandList_Close(t->lists[i]);
      t->open[i] = false;
   }
   record_pyrowave_d3d12_abandon(t->send); record_pyrowave_d3d12_abandon(t->receive);
   t->send = t->receive = NULL;
   rpyrowave_decoder_free(t->cpu); t->cpu = NULL;
   RPY_D3D_RELEASE(t->source); RPY_D3D_RELEASE(t->upload); RPY_D3D_RELEASE(t->readback);
}

int main(int argc, char **argv)
{
   test_pyrowave_d3d12_t *t = (test_pyrowave_d3d12_t*)calloc(1, sizeof(*t));
   IDXGIFactory1 *factory = NULL;
   IDXGIAdapter1 *adapter = NULL;
   ID3D12Debug *debug = NULL;
   ID3D12Debug1 *debug1 = NULL;
   D3D12_COMMAND_QUEUE_DESC queue;
   unsigned i;
   int success = 0;
   (void)rpy_d3d_dispatch; (void)rpy_gpu_vector_load; (void)rpy_gpu_vector_compare;
#ifdef PYROWAVE_PLAYER_FACTORY
   if (!t || argc != 5) { free(t); return EXIT_FAILURE; }
   test_pyrowave_d3d12_directory = argv[4];
#else
   if (!t || argc != 4) { free(t); return EXIT_FAILURE; }
#endif
   RPY_D3D_CHECK(SUCCEEDED(D3D12GetDebugInterface(&IID_ID3D12Debug, (void**)&debug)));
   ID3D12Debug_EnableDebugLayer(debug);
   if (getenv("RPY_D3D12_GPU_VALIDATION"))
   {
      RPY_D3D_CHECK(SUCCEEDED(ID3D12Debug_QueryInterface(debug, &IID_ID3D12Debug1, (void**)&debug1)));
      ID3D12Debug1_SetEnableGPUBasedValidation(debug1, TRUE);
      puts("D3D12 GPU-based validation: enabled");
   }
   RPY_D3D_CHECK(SUCCEEDED(CreateDXGIFactory1(&IID_IDXGIFactory1, (void**)&factory)));
   for (i = 0; IDXGIFactory1_EnumAdapters1(factory, i, &adapter) == S_OK; i++)
   {
      DXGI_ADAPTER_DESC1 desc;
      if (SUCCEEDED(IDXGIAdapter1_GetDesc1(adapter, &desc)) && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            && SUCCEEDED(D3D12CreateDevice((IUnknown*)adapter, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void**)&t->device)))
      {
         char name[256];
         WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof(name), NULL, NULL);
         printf("D3D12 hardware: %s, debug layer enabled\n", name);
         break;
      }
      RPY_D3D_RELEASE(adapter);
   }
   RPY_D3D_CHECK(t->device);
   RPY_D3D_CHECK(SUCCEEDED(ID3D12Device_QueryInterface(t->device, &IID_ID3D12InfoQueue, (void**)&t->info)));
   memset(&queue, 0, sizeof(queue));
#ifdef PYROWAVE_PLAYER_FACTORY
   queue.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
#else
   queue.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
#endif
   RPY_D3D_CHECK(SUCCEEDED(ID3D12Device_CreateCommandQueue(t->device, &queue, &IID_ID3D12CommandQueue, (void**)&t->queue)));
   t->event = CreateEventA(NULL, FALSE, FALSE, NULL); RPY_D3D_CHECK(t->event);
   for (i = 0; i < 4; i++)
   {
      RPY_D3D_CHECK(SUCCEEDED(ID3D12Device_CreateCommandAllocator(t->device, queue.Type, &IID_ID3D12CommandAllocator, (void**)&t->allocators[i])));
      RPY_D3D_CHECK(SUCCEEDED(ID3D12Device_CreateCommandList(t->device, 0, queue.Type, t->allocators[i], NULL,
                  &IID_ID3D12GraphicsCommandList, (void**)&t->lists[i])));
      RPY_D3D_CHECK(SUCCEEDED(ID3D12GraphicsCommandList_Close(t->lists[i])));
      RPY_D3D_CHECK(SUCCEEDED(ID3D12Device_CreateFence(t->device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**)&t->fences[i])));
   }
   t->cfg.device = t->device; t->cfg.serialize = D3D12SerializeRootSignature;
   t->cfg.queue_type = queue.Type; t->cfg.source_format = DXGI_FORMAT_R32G32B32A32_FLOAT; t->cfg.maximum_bytes = 262144;
   for (i = 0; i < 3; i++)
   {
      t->cfg.shaders[i] = rpy_gpu_test_file(argv[i + 1], &t->cfg.shader_bytes[i]);
      RPY_D3D_CHECK(t->cfg.shaders[i]);
   }
   for (i = 0; i < 2; i++)
   {
      t->width = 130; t->height = 98; t->mode = i;
      RPY_D3D_CHECK(test_pyrowave_d3d12_run(t, 17)); test_pyrowave_d3d12_case_free(t);
   }
   /* Odd frontend window dimensions produce partial right/bottom packets. */
   t->width = 348; t->height = 297; t->mode = 0;
   RPY_D3D_CHECK(test_pyrowave_d3d12_run(t, 2)); test_pyrowave_d3d12_case_free(t);
   t->width = 1920; t->height = 1080; t->mode = 0;
   RPY_D3D_CHECK(test_pyrowave_d3d12_run(t, 2));
   success = 1;
end:
   if (!test_pyrowave_d3d12_quiesce(t))
   {
      fputs("D3D12 test queue did not quiesce; leaving resources to process teardown\n", stderr);
      return EXIT_FAILURE;
   }
   test_pyrowave_d3d12_case_free(t);
   for (i = 0; i < 3; i++) free((void*)t->cfg.shaders[i]);
   for (i = 0; i < 4; i++)
   {
      RPY_D3D_RELEASE(t->lists[i]); RPY_D3D_RELEASE(t->allocators[i]); RPY_D3D_RELEASE(t->fences[i]);
   }
   RPY_D3D_RELEASE(t->queue);
   if (!rpy_d3d_messages(t->info)) success = 0;
   RPY_D3D_RELEASE(t->info); RPY_D3D_RELEASE(t->device);
   RPY_D3D_RELEASE(adapter); RPY_D3D_RELEASE(factory); RPY_D3D_RELEASE(debug1); RPY_D3D_RELEASE(debug);
   if (t->event) CloseHandle(t->event);
   free(t);
   return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
#else
int main(void)
{
   puts("D3D12 native adapter validation requires Windows");
   return 77;
}
#endif
