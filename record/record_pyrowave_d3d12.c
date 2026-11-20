/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include "record_pyrowave_d3d12.h"
#include "record_pyrowave_session_internal.h"
#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>

#define RPY_D12_RELEASE(p) do { if (p) { (p)->lpVtbl->Release(p); (p) = NULL; } } while (0)
#define RPY_D12_IMAGE_STATE D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE

enum record_pyrowave_d12_state
{
   RPY_D12_IDLE, RPY_D12_DECODING, RPY_D12_READY, RPY_D12_LENT, RPY_D12_DISPLAYING
};

typedef struct record_pyrowave_d12_buffer
{
   ID3D12Resource *resource;
   uint8_t *mapped;
   size_t bytes;
} record_pyrowave_d12_buffer_t;

typedef struct record_pyrowave_d12_slot
{
   record_pyrowave_d12_buffer_t transfer, readback;
   ID3D12Resource *texture;
   ID3D12Fence *fence;
   uint8_t *host;
   uint64_t value;
   record_gpu_frame_t frame;
   rpyrowave_packet_parameters_t parameters;
   retro_atomic_int_t ready;
   unsigned state;
} record_pyrowave_d12_slot_t;

struct record_pyrowave_d3d12
{
   ID3D12Device *device;
   ID3D12DescriptorHeap *heap;
   ID3D12RootSignature *roots[3];
   ID3D12PipelineState *pipelines[3];
   ID3D12GraphicsCommandList *commands;
   record_gpu_queue_t *queue;
   record_pyrowave_d12_buffer_t atlas[2], blocks, workspace;
   record_pyrowave_d12_slot_t slots[RECORD_GPU_QUEUE_SLOTS];
   D3D12_CPU_DESCRIPTOR_HANDLE cpu;
   D3D12_GPU_DESCRIPTOR_HANDLE gpu;
   rpyrowave_sequence_t sequence;
   rpyrowave_gpu_layout_t layout;
   rpyrowave_color_parameters_t color;
   DXGI_FORMAT source_format;
   D3D12_COMMAND_LIST_TYPE queue_type;
   uint64_t next_ticket, retired;
   unsigned count, reservation, owned, descriptor_stride, command_slot, usability;
   unsigned char retired_marks[RECORD_GPU_QUEUE_SLOTS];
   bool decode, failed;
};

static uint32_t record_pyrowave_d12_load32(const uint8_t *p)
{
   return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static bool record_pyrowave_d12_buffer_new(record_pyrowave_d3d12_t *c,
      record_pyrowave_d12_buffer_t *b, size_t bytes, D3D12_HEAP_TYPE type,
      D3D12_RESOURCE_STATES state, D3D12_RESOURCE_FLAGS flags)
{
   D3D12_HEAP_PROPERTIES heap;
   D3D12_RESOURCE_DESC desc;
   D3D12_RANGE range;
   memset(&heap, 0, sizeof(heap)); memset(&desc, 0, sizeof(desc));
   heap.Type = type; heap.CreationNodeMask = heap.VisibleNodeMask = 1;
   desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; desc.Width = bytes;
   desc.Height = desc.DepthOrArraySize = desc.MipLevels = 1;
   desc.SampleDesc.Count = 1; desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; desc.Flags = flags;
   if (FAILED(ID3D12Device_CreateCommittedResource(c->device, &heap, D3D12_HEAP_FLAG_NONE,
               &desc, state, NULL, &IID_ID3D12Resource, (void**)&b->resource))) return false;
   b->bytes = bytes;
   if (type == D3D12_HEAP_TYPE_DEFAULT) return true;
   range.Begin = 0; range.End = type == D3D12_HEAP_TYPE_READBACK ? bytes : 0;
   return SUCCEEDED(ID3D12Resource_Map(b->resource, 0, &range, (void**)&b->mapped));
}

static void record_pyrowave_d12_buffer_free(record_pyrowave_d12_buffer_t *b)
{
   D3D12_RANGE range;
   range.Begin = range.End = 0;
   if (b->mapped) ID3D12Resource_Unmap(b->resource, 0, &range);
   RPY_D12_RELEASE(b->resource);
}

static void record_pyrowave_d12_destroy(record_pyrowave_d3d12_t *c)
{
   unsigned i;
   for (i = 0; i < c->count; i++)
   {
      record_pyrowave_d12_slot_t *s = &c->slots[i];
      if (c->decode) free(s->host);
      record_pyrowave_d12_buffer_free(&s->transfer);
      record_pyrowave_d12_buffer_free(&s->readback);
      RPY_D12_RELEASE(s->texture);
   }
   for (i = 0; i < 3; i++)
   {
      RPY_D12_RELEASE(c->pipelines[i]); RPY_D12_RELEASE(c->roots[i]);
   }
   RPY_D12_RELEASE(c->heap);
   for (i = 0; i < 2; i++) record_pyrowave_d12_buffer_free(&c->atlas[i]);
   record_pyrowave_d12_buffer_free(&c->blocks);
   record_pyrowave_d12_buffer_free(&c->workspace);
   free(c);
}

static D3D12_CPU_DESCRIPTOR_HANDLE record_pyrowave_d12_cpu(record_pyrowave_d3d12_t *c, unsigned index)
{
   D3D12_CPU_DESCRIPTOR_HANDLE handle = c->cpu;
   handle.ptr += (SIZE_T)index * c->descriptor_stride;
   return handle;
}

static D3D12_GPU_DESCRIPTOR_HANDLE record_pyrowave_d12_gpu(record_pyrowave_d3d12_t *c, unsigned index)
{
   D3D12_GPU_DESCRIPTOR_HANDLE handle = c->gpu;
   handle.ptr += (UINT64)index * c->descriptor_stride;
   return handle;
}

static bool record_pyrowave_d12_texture_new(record_pyrowave_d3d12_t *c, unsigned slot)
{
   D3D12_HEAP_PROPERTIES heap;
   D3D12_RESOURCE_DESC desc;
   D3D12_UNORDERED_ACCESS_VIEW_DESC view;
   memset(&heap, 0, sizeof(heap)); memset(&desc, 0, sizeof(desc)); memset(&view, 0, sizeof(view));
   heap.Type = D3D12_HEAP_TYPE_DEFAULT; heap.CreationNodeMask = heap.VisibleNodeMask = 1;
   desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   desc.Width = c->layout.width; desc.Height = c->layout.height;
   desc.DepthOrArraySize = desc.MipLevels = 1; desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
   desc.SampleDesc.Count = 1; desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
   if (FAILED(ID3D12Device_CreateCommittedResource(c->device, &heap, D3D12_HEAP_FLAG_NONE, &desc,
               RPY_D12_IMAGE_STATE, NULL, &IID_ID3D12Resource, (void**)&c->slots[slot].texture))) return false;
   view.Format = desc.Format; view.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
   ID3D12Device_CreateUnorderedAccessView(c->device, c->slots[slot].texture, NULL,
         &view, record_pyrowave_d12_cpu(c, slot * 2 + 1));
   return true;
}

static bool record_pyrowave_d12_pipeline(record_pyrowave_d3d12_t *c,
      const record_pyrowave_d3d12_config_t *cfg, unsigned kind)
{
   D3D12_ROOT_PARAMETER parameters[5];
   D3D12_DESCRIPTOR_RANGE ranges[2];
   D3D12_ROOT_SIGNATURE_DESC root;
   D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline;
   ID3DBlob *blob = NULL, *errors = NULL;
   HRESULT result;
   unsigned i, count = kind == 1 ? 3 : 5;
   memset(parameters, 0, sizeof(parameters)); memset(ranges, 0, sizeof(ranges));
   memset(&root, 0, sizeof(root)); memset(&pipeline, 0, sizeof(pipeline));
   for (i = 0; i < count; i++) parameters[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
   if (!kind)
   {
      for (i = 0; i < 2; i++)
      {
         unsigned index = i ? 3 : 0;
         ranges[i].RangeType = i ? D3D12_DESCRIPTOR_RANGE_TYPE_UAV : D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
         ranges[i].NumDescriptors = 1; ranges[i].BaseShaderRegister = i ? 1 : 0;
         parameters[index].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
         parameters[index].DescriptorTable.NumDescriptorRanges = 1;
         parameters[index].DescriptorTable.pDescriptorRanges = &ranges[i];
      }
      parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; parameters[1].Descriptor.ShaderRegister = 1;
      parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
   }
   else
   {
      parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
      parameters[1].ParameterType = kind == 1 ? D3D12_ROOT_PARAMETER_TYPE_UAV : D3D12_ROOT_PARAMETER_TYPE_SRV;
      parameters[1].Descriptor.ShaderRegister = kind == 1 ? 0 : 1;
      if (kind == 2)
      {
         parameters[2].ParameterType = parameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
         parameters[3].Descriptor.ShaderRegister = 1;
      }
   }
   parameters[count - 1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
   parameters[count - 1].Constants.Num32BitValues = !kind ? 20 : (kind == 1 ? 8 : 12);
   root.NumParameters = count; root.pParameters = parameters;
   result = cfg->serialize(&root, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors);
   RPY_D12_RELEASE(errors);
   if (FAILED(result))
   {
      RPY_D12_RELEASE(blob);
      return false;
   }
   result = ID3D12Device_CreateRootSignature(c->device, 0,
         ID3D10Blob_GetBufferPointer(blob), ID3D10Blob_GetBufferSize(blob),
         &IID_ID3D12RootSignature, (void**)&c->roots[kind]);
   RPY_D12_RELEASE(blob);
   if (FAILED(result)) return false;
   pipeline.pRootSignature = c->roots[kind];
   pipeline.CS.pShaderBytecode = cfg->shaders[kind]; pipeline.CS.BytecodeLength = cfg->shader_bytes[kind];
   return SUCCEEDED(ID3D12Device_CreateComputePipelineState(c->device, &pipeline,
            &IID_ID3D12PipelineState, (void**)&c->pipelines[kind]));
}

static int record_pyrowave_d12_usability(void *user, const rpyrowave_packet_parameters_t *p, unsigned x, unsigned y)
{
   record_pyrowave_d3d12_t *c = (record_pyrowave_d3d12_t*)user;
   (void)x; (void)y;
   c->usability = p->usability;
   return 1;
}

record_pyrowave_d3d12_t *record_pyrowave_d3d12_new(const record_pyrowave_d3d12_config_t *cfg)
{
   record_pyrowave_d3d12_t *c;
   D3D12_DESCRIPTOR_HEAP_DESC heap;
   D3D12_SHADER_RESOURCE_VIEW_DESC srv;
   D3D12_UNORDERED_ACCESS_VIEW_DESC uav;
   D3D12_FEATURE_DATA_FORMAT_SUPPORT format;
   rpyrowave_color_parameters_t color;
   D3D12_CPU_DESCRIPTOR_HANDLE *(STDMETHODCALLTYPE *cpu_start)(ID3D12DescriptorHeap*, D3D12_CPU_DESCRIPTOR_HANDLE*);
   D3D12_GPU_DESCRIPTOR_HANDLE *(STDMETHODCALLTYPE *gpu_start)(ID3D12DescriptorHeap*, D3D12_GPU_DESCRIPTOR_HANDLE*);
   size_t atlas_bytes;
   unsigned i;
   if (!cfg || !cfg->device || !cfg->serialize || !cfg->slots || cfg->slots > RECORD_GPU_QUEUE_SLOTS
         || cfg->sequence.width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION
         || cfg->sequence.height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION
         || cfg->texture_mode > (unsigned)(cfg->decode ? RPYROWAVE_COLOR_SDR709 : RPYROWAVE_COLOR_SCRGB)
         || (cfg->queue_type != D3D12_COMMAND_LIST_TYPE_DIRECT && cfg->queue_type != D3D12_COMMAND_LIST_TYPE_COMPUTE)
         || rpyrowave_gpu_color_parameters_ex(&cfg->sequence, cfg->bit_depth, cfg->decode ? 1 : 0,
            cfg->decode ? 0 : cfg->flip_y, cfg->decode ? cfg->sequence.width : cfg->texture_width,
            cfg->decode ? cfg->sequence.height : cfg->texture_height,
            cfg->decode ? 0 : cfg->x, cfg->decode ? 0 : cfg->y, cfg->scale_source ? 1 : 0, &color)) return NULL;
   for (i = 0; i < 3; i++) if (!cfg->shaders[i] || !cfg->shader_bytes[i]) return NULL;
   if (!cfg->decode && cfg->source_format != DXGI_FORMAT_R32G32B32A32_FLOAT
         && cfg->source_format != DXGI_FORMAT_R16G16B16A16_FLOAT
         && cfg->source_format != DXGI_FORMAT_R8G8B8A8_UNORM
         && cfg->source_format != DXGI_FORMAT_B8G8R8A8_UNORM
         && cfg->source_format != DXGI_FORMAT_R10G10B10A2_UNORM) return NULL;
   memset(&format, 0, sizeof(format)); format.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
   if (FAILED(ID3D12Device_CheckFeatureSupport(cfg->device, D3D12_FEATURE_FORMAT_SUPPORT, &format, sizeof(format)))
         || !(format.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D)
         || !(format.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_LOAD)
         || !(format.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE)) return NULL;
   c = (record_pyrowave_d3d12_t*)calloc(1, sizeof(*c));
   if (!c) return NULL;
   c->device = cfg->device; c->sequence = cfg->sequence; c->color = color;
   c->color.texture_mode = cfg->texture_mode; c->source_format = cfg->source_format;
   c->count = cfg->slots; c->decode = cfg->decode; c->queue_type = cfg->queue_type;
   c->next_ticket = 1; c->reservation = RECORD_GPU_QUEUE_SLOTS;
   if (rpyrowave_gpu_layout(&cfg->sequence, cfg->maximum_bytes, &c->layout)) goto error;
   atlas_bytes = (size_t)c->layout.image_width * c->layout.image_height * 3 * sizeof(float);
   c->queue = record_gpu_queue_new(c->count);
   if (!c->queue) goto error;
   for (i = 0; i < 2; i++)
      if (!record_pyrowave_d12_buffer_new(c, &c->atlas[i], atlas_bytes, D3D12_HEAP_TYPE_DEFAULT,
               i ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
               D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)) goto error;
   if (!record_pyrowave_d12_buffer_new(c, &c->blocks, (size_t)c->layout.block_count * sizeof(rpyrowave_gpu_block_t),
            D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE)
         || !record_pyrowave_d12_buffer_new(c, &c->workspace, c->layout.workspace_bytes,
            D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
         || rpyrowave_gpu_blocks(&c->layout, (rpyrowave_gpu_block_t*)c->blocks.mapped, c->layout.block_count)
         || rpyrowave_gpu_encode_record(&c->layout, &c->sequence, cfg->maximum_bytes, -1,
            record_pyrowave_d12_usability, c)) goto error;
   memset(&heap, 0, sizeof(heap)); heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
   heap.NumDescriptors = c->count * 2; heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
   if (FAILED(ID3D12Device_CreateDescriptorHeap(c->device, &heap, &IID_ID3D12DescriptorHeap, (void**)&c->heap))) goto error;
   /* Older MSVC C headers spell these COM structure returns by value. */
   memcpy(&cpu_start, &c->heap->lpVtbl->GetCPUDescriptorHandleForHeapStart, sizeof(cpu_start));
   memcpy(&gpu_start, &c->heap->lpVtbl->GetGPUDescriptorHandleForHeapStart, sizeof(gpu_start));
   cpu_start(c->heap, &c->cpu); gpu_start(c->heap, &c->gpu);
   c->descriptor_stride = ID3D12Device_GetDescriptorHandleIncrementSize(c->device, heap.Type);
   memset(&srv, 0, sizeof(srv)); memset(&uav, 0, sizeof(uav));
   srv.Format = uav.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
   srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   srv.Texture2D.MipLevels = 1; uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
   for (i = 0; i < c->count; i++)
   {
      record_pyrowave_d12_slot_t *s = &c->slots[i];
      retro_atomic_int_init(&s->ready, 0);
      if (c->decode)
      {
         s->host = (uint8_t*)malloc(c->layout.workspace_bytes);
         if (!s->host || !record_pyrowave_d12_buffer_new(c, &s->transfer, c->layout.workspace_bytes,
                  D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE)
               || !record_pyrowave_d12_buffer_new(c, &s->readback, 16, D3D12_HEAP_TYPE_READBACK,
                  D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE)
               || !record_pyrowave_d12_texture_new(c, i)) goto error;
         ID3D12Device_CreateShaderResourceView(c->device, NULL, &srv, record_pyrowave_d12_cpu(c, i * 2));
      }
      else
      {
         if (!record_pyrowave_d12_buffer_new(c, &s->readback, 16 + ((size_t)cfg->maximum_bytes + 3) / 4 * 4,
                  D3D12_HEAP_TYPE_READBACK, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE)) goto error;
         s->host = s->readback.mapped;
         ID3D12Device_CreateUnorderedAccessView(c->device, NULL, NULL, &uav, record_pyrowave_d12_cpu(c, i * 2 + 1));
      }
   }
   for (i = 0; i < 3; i++) if (!record_pyrowave_d12_pipeline(c, cfg, i)) goto error;
   return c;
error:
   record_gpu_queue_free(c->queue); record_pyrowave_d12_destroy(c);
   return NULL;
}

static void record_pyrowave_d12_transition(record_pyrowave_d3d12_t *c, ID3D12Resource *resource,
      D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
   D3D12_RESOURCE_BARRIER barrier;
   memset(&barrier, 0, sizeof(barrier)); barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
   barrier.Transition.pResource = resource; barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
   barrier.Transition.StateBefore = before; barrier.Transition.StateAfter = after;
   ID3D12GraphicsCommandList_ResourceBarrier(c->commands, 1, &barrier);
}

static void record_pyrowave_d12_uav(record_pyrowave_d3d12_t *c)
{
   D3D12_RESOURCE_BARRIER barrier;
   memset(&barrier, 0, sizeof(barrier)); barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
   ID3D12GraphicsCommandList_ResourceBarrier(c->commands, 1, &barrier);
}

static void record_pyrowave_d12_pipeline_bind(record_pyrowave_d3d12_t *c, unsigned kind)
{
   ID3D12GraphicsCommandList_SetComputeRootSignature(c->commands, c->roots[kind]);
   ID3D12GraphicsCommandList_SetPipelineState(c->commands, c->pipelines[kind]);
}

static void record_pyrowave_d12_color(record_pyrowave_d3d12_t *c, unsigned slot)
{
   record_pyrowave_d12_pipeline_bind(c, 0);
   ID3D12GraphicsCommandList_SetDescriptorHeaps(c->commands, 1, &c->heap);
   ID3D12GraphicsCommandList_SetComputeRootDescriptorTable(c->commands, 0, record_pyrowave_d12_gpu(c, slot * 2));
   ID3D12GraphicsCommandList_SetComputeRootShaderResourceView(c->commands, 1,
         ID3D12Resource_GetGPUVirtualAddress(c->decode ? c->atlas[0].resource : c->blocks.resource));
   ID3D12GraphicsCommandList_SetComputeRootUnorderedAccessView(c->commands, 2,
         ID3D12Resource_GetGPUVirtualAddress(c->atlas[c->decode ? 1 : 0].resource));
   ID3D12GraphicsCommandList_SetComputeRootDescriptorTable(c->commands, 3, record_pyrowave_d12_gpu(c, slot * 2 + 1));
   ID3D12GraphicsCommandList_SetComputeRoot32BitConstants(c->commands, 4, 20, &c->color, 0);
   ID3D12GraphicsCommandList_Dispatch(c->commands,
         ((c->decode ? c->layout.width : c->layout.image_width) + 7) / 8,
         ((c->decode ? c->layout.height : c->layout.image_height) + 7) / 8, c->decode ? 1 : 3);
   record_pyrowave_d12_uav(c);
}

static int record_pyrowave_d12_dwt(void *user, const rpyrowave_dwt_parameters_t *p,
      unsigned source, unsigned x, unsigned y, unsigned z)
{
   record_pyrowave_d3d12_t *c = (record_pyrowave_d3d12_t*)user;
   ID3D12GraphicsCommandList_SetComputeRootShaderResourceView(c->commands, 0, ID3D12Resource_GetGPUVirtualAddress(c->atlas[source].resource));
   ID3D12GraphicsCommandList_SetComputeRootUnorderedAccessView(c->commands, 1, ID3D12Resource_GetGPUVirtualAddress(c->atlas[source ^ 1].resource));
   ID3D12GraphicsCommandList_SetComputeRoot32BitConstants(c->commands, 2, 8, p, 0);
   ID3D12GraphicsCommandList_Dispatch(c->commands, x, y, z);
   record_pyrowave_d12_transition(c, c->atlas[source].resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   record_pyrowave_d12_transition(c, c->atlas[source ^ 1].resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   return 1;
}

static int record_pyrowave_d12_packet_dispatch(void *user, const rpyrowave_packet_parameters_t *p, unsigned x, unsigned y)
{
   record_pyrowave_d3d12_t *c = (record_pyrowave_d3d12_t*)user;
   ID3D12GraphicsCommandList_SetComputeRoot32BitConstants(c->commands, 4, 12, p, 0);
   ID3D12GraphicsCommandList_Dispatch(c->commands, x, y, 1);
   record_pyrowave_d12_uav(c);
   return 1;
}

static void record_pyrowave_d12_packet_bind(record_pyrowave_d3d12_t *c)
{
   record_pyrowave_d12_pipeline_bind(c, 2);
   ID3D12GraphicsCommandList_SetComputeRootShaderResourceView(c->commands, 0, ID3D12Resource_GetGPUVirtualAddress(c->atlas[c->decode ? 1 : 0].resource));
   ID3D12GraphicsCommandList_SetComputeRootShaderResourceView(c->commands, 1, ID3D12Resource_GetGPUVirtualAddress(c->blocks.resource));
   ID3D12GraphicsCommandList_SetComputeRootUnorderedAccessView(c->commands, 2, ID3D12Resource_GetGPUVirtualAddress(c->workspace.resource));
   ID3D12GraphicsCommandList_SetComputeRootUnorderedAccessView(c->commands, 3, ID3D12Resource_GetGPUVirtualAddress(c->atlas[c->decode ? 0 : 1].resource));
}

static bool record_pyrowave_d12_fence_available(record_pyrowave_d3d12_t *c, ID3D12Fence *fence, uint64_t value)
{
   unsigned i;
   if (!fence || !value || value == (uint64_t)-1 || ID3D12Fence_GetCompletedValue(fence) >= value) return false;
   for (i = 0; i < c->count; i++) if (c->slots[i].fence == fence && c->slots[i].value >= value) return false;
   return true;
}

static void record_pyrowave_d12_retire(record_pyrowave_d3d12_t *c, uint64_t ticket)
{
   unsigned i;
   c->retired_marks[ticket % RECORD_GPU_QUEUE_SLOTS] = 1;
   for (i = 0; i < c->count; i++)
   {
      unsigned index = (unsigned)((c->retired + 1) % RECORD_GPU_QUEUE_SLOTS);
      if (!c->retired_marks[index]) break;
      c->retired_marks[index] = 0; c->retired++;
   }
}

int record_pyrowave_d3d12_push(record_pyrowave_d3d12_t *c, ID3D12GraphicsCommandList *commands,
      ID3D12Fence *completion, uint64_t value, ID3D12Resource *source,
      uint64_t timestamp, uint64_t duration, uint64_t *ticket)
{
   record_pyrowave_d12_slot_t *s;
   D3D12_SHADER_RESOURCE_VIEW_DESC view;
   rpyrowave_sequence_t sequence;
   unsigned slot;
   int result;
   if (!c || c->decode || c->failed || !commands || !source || !ticket || !c->next_ticket
         || !duration || timestamp > (uint64_t)-1 - duration
         || ID3D12GraphicsCommandList_GetType(commands) != c->queue_type
         || !record_pyrowave_d12_fence_available(c, completion, value)) return RECORD_GPU_QUEUE_ERROR;
   if (c->next_ticket - c->retired > c->count) return RECORD_GPU_QUEUE_BUSY;
   result = record_gpu_queue_acquire(c->queue, &slot);
   if (result != RECORD_GPU_QUEUE_OK) return result;
   c->reservation = slot; s = &c->slots[slot];
   s->frame.timestamp = timestamp; s->frame.duration = duration; s->frame.ready_value = c->next_ticket++;
   s->frame.slot = slot; s->frame.keyframe = true; s->fence = completion; s->value = value;
   *ticket = s->frame.ready_value; retro_atomic_store_release_int(&s->ready, 0);
   memset(&view, 0, sizeof(view)); view.Format = c->source_format; view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
   view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; view.Texture2D.MipLevels = 1;
   ID3D12Device_CreateShaderResourceView(c->device, source, &view, record_pyrowave_d12_cpu(c, slot * 2));
   c->commands = commands;
   record_pyrowave_d12_transition(c, c->atlas[0].resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   record_pyrowave_d12_color(c, slot);
   record_pyrowave_d12_transition(c, c->atlas[0].resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   record_pyrowave_d12_pipeline_bind(c, 1);
   rpyrowave_dwt_record(c->layout.image_width, c->layout.image_height, c->layout.chroma_444, 0, record_pyrowave_d12_dwt, c);
   record_pyrowave_d12_packet_bind(c);
   sequence = c->sequence; sequence.sequence = (unsigned)(*ticket & 7);
   rpyrowave_gpu_encode_record(&c->layout, &sequence, c->layout.maximum_bytes, -1, record_pyrowave_d12_packet_dispatch, c);
   record_pyrowave_d12_transition(c, c->workspace.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
   ID3D12GraphicsCommandList_CopyBufferRegion(commands, s->readback.resource, 0, c->workspace.resource, 0, 16);
   ID3D12GraphicsCommandList_CopyBufferRegion(commands, s->readback.resource, 16, c->workspace.resource,
         c->layout.wire_offset, s->readback.bytes - 16);
   record_pyrowave_d12_transition(c, c->workspace.resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   if (!record_gpu_queue_submit(c->queue, &s->frame))
   {
      c->failed = true;
      return RECORD_GPU_QUEUE_ERROR;
   }
   c->reservation = RECORD_GPU_QUEUE_SLOTS;
   return RECORD_GPU_QUEUE_OK;
}

int record_pyrowave_d3d12_decode(record_pyrowave_d3d12_t *c,
      ID3D12GraphicsCommandList *commands, ID3D12Fence *completion, uint64_t value)
{
   record_gpu_frame_t frame;
   record_pyrowave_d12_slot_t *s;
   int result;
   if (!c || !c->decode || c->failed || !commands
         || ID3D12GraphicsCommandList_GetType(commands) != c->queue_type
         || !record_pyrowave_d12_fence_available(c, completion, value)) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_take(c->queue, &frame);
   if (result != RECORD_GPU_QUEUE_OK) return result;
   s = &c->slots[frame.slot];
   memcpy(s->transfer.mapped, s->host, c->layout.wire_offset + s->parameters.maximum_bytes);
   s->state = RPY_D12_DECODING; s->fence = completion; s->value = value; c->commands = commands;
   record_pyrowave_d12_transition(c, c->workspace.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_DEST);
   ID3D12GraphicsCommandList_CopyBufferRegion(commands, c->workspace.resource, 0, s->transfer.resource, 0,
         c->layout.wire_offset + s->parameters.maximum_bytes);
   record_pyrowave_d12_transition(c, c->workspace.resource, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   record_pyrowave_d12_transition(c, c->atlas[0].resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   record_pyrowave_d12_transition(c, c->atlas[1].resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   record_pyrowave_d12_packet_bind(c);
   rpyrowave_gpu_decode_record(&c->layout, &s->parameters, record_pyrowave_d12_packet_dispatch, c);
   record_pyrowave_d12_transition(c, c->atlas[0].resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   record_pyrowave_d12_transition(c, c->atlas[1].resource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   record_pyrowave_d12_pipeline_bind(c, 1);
   rpyrowave_dwt_record(c->layout.image_width, c->layout.image_height, c->layout.chroma_444, 1, record_pyrowave_d12_dwt, c);
   record_pyrowave_d12_transition(c, s->texture, RPY_D12_IMAGE_STATE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   record_pyrowave_d12_color(c, frame.slot);
   record_pyrowave_d12_transition(c, s->texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, RPY_D12_IMAGE_STATE);
   record_pyrowave_d12_transition(c, c->workspace.resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
   ID3D12GraphicsCommandList_CopyBufferRegion(commands, s->readback.resource, 0, c->workspace.resource, 0, 16);
   record_pyrowave_d12_transition(c, c->workspace.resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   return RECORD_GPU_QUEUE_OK;
}

uint64_t record_pyrowave_d3d12_source_retired(const record_pyrowave_d3d12_t *c)
{
   return c ? c->retired : 0;
}

int record_pyrowave_d3d12_take(record_pyrowave_d3d12_t *c, record_gpu_frame_t *frame)
{
   int result;
   if (!c || c->decode || !frame) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_take(c->queue, frame);
   if (result == RECORD_GPU_QUEUE_OK) c->owned |= (unsigned)1 << frame->slot;
   return result;
}

int record_pyrowave_d3d12_packet(record_pyrowave_d3d12_t *c, unsigned slot, record_pyrowave_packet_t *packet)
{
   record_pyrowave_d12_slot_t *s;
   int ready;
   uint32_t bytes;
   if (!c || c->decode || !packet || slot >= c->count || !(c->owned & ((unsigned)1 << slot))) return RECORD_GPU_QUEUE_ERROR;
   s = &c->slots[slot]; ready = retro_atomic_load_acquire_int(&s->ready);
   if (!ready) return RECORD_GPU_QUEUE_BUSY;
   if (ready != 1) return RECORD_GPU_QUEUE_ERROR;
   bytes = record_pyrowave_d12_load32(s->host + 4);
   if (bytes < 8 || bytes > c->layout.maximum_bytes || record_pyrowave_d12_load32(s->host + 12)) return RECORD_GPU_QUEUE_ERROR;
   packet->data = s->host + 16; packet->bytes = bytes;
   return RECORD_GPU_QUEUE_OK;
}

bool record_pyrowave_d3d12_release(record_pyrowave_d3d12_t *c, unsigned slot)
{
   if (!c || c->decode || slot >= c->count || !(c->owned & ((unsigned)1 << slot))
         || !retro_atomic_load_acquire_int(&c->slots[slot].ready)) return false;
   retro_atomic_store_release_int(&c->slots[slot].ready, 0);
   c->owned &= ~((unsigned)1 << slot);
   return record_gpu_queue_release(c->queue, slot);
}

int record_pyrowave_d3d12_receive_acquire(record_pyrowave_d3d12_t *c, void **data, size_t *capacity)
{
   int result;
   if (data) *data = NULL;
   if (capacity) *capacity = 0;
   if (!c || !c->decode || !data || !capacity) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_acquire(c->queue, &c->reservation);
   if (result != RECORD_GPU_QUEUE_OK) return result;
   *data = c->slots[c->reservation].host + c->layout.wire_offset;
   *capacity = c->layout.maximum_bytes;
   return RECORD_GPU_QUEUE_OK;
}

int record_pyrowave_d3d12_receive_submit(record_pyrowave_d3d12_t *c,
      size_t bytes, uint64_t timestamp, uint64_t duration)
{
   record_pyrowave_d12_slot_t *s;
   if (!c || !c->decode || c->reservation >= c->count || !c->next_ticket
         || !duration || timestamp > (uint64_t)-1 - duration) return RECORD_GPU_QUEUE_ERROR;
   s = &c->slots[c->reservation];
   if (rpyrowave_gpu_index_frame(&c->layout, s->host, c->layout.workspace_bytes, bytes, 0, &s->parameters)
         || s->parameters.usability != c->usability) return RECORD_GPU_QUEUE_ERROR;
   s->frame.timestamp = timestamp; s->frame.duration = duration; s->frame.ready_value = c->next_ticket;
   s->frame.slot = c->reservation; s->frame.keyframe = true;
   if (!record_gpu_queue_submit(c->queue, &s->frame)) return RECORD_GPU_QUEUE_ERROR;
   c->next_ticket++; c->reservation = RECORD_GPU_QUEUE_SLOTS;
   return RECORD_GPU_QUEUE_OK;
}

bool record_pyrowave_d3d12_receive_cancel(record_pyrowave_d3d12_t *c)
{
   if (!c || !c->decode || !record_gpu_queue_cancel(c->queue)) return false;
   c->reservation = RECORD_GPU_QUEUE_SLOTS;
   return true;
}

int record_pyrowave_d3d12_receive_peek(record_pyrowave_d3d12_t *c, record_gpu_frame_t *frame)
{
   return c && c->decode ? record_gpu_queue_peek(c->queue, frame) : RECORD_GPU_QUEUE_ERROR;
}

int record_pyrowave_d3d12_receive_drop(record_pyrowave_d3d12_t *c, record_gpu_frame_t *frame)
{
   int result;
   if (!c || !c->decode || !frame) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_take(c->queue, frame);
   if (result == RECORD_GPU_QUEUE_OK && !record_gpu_queue_release(c->queue, frame->slot)) return RECORD_GPU_QUEUE_ERROR;
   return result;
}

int record_pyrowave_d3d12_image(record_pyrowave_d3d12_t *c, record_pyrowave_d3d12_image_t *image)
{
   unsigned i, selected = RECORD_GPU_QUEUE_SLOTS;
   if (!c || !c->decode || !image) return RECORD_GPU_QUEUE_ERROR;
   for (i = 0; i < c->count; i++)
      if (c->slots[i].state == RPY_D12_READY && (selected == RECORD_GPU_QUEUE_SLOTS
               || c->slots[i].frame.ready_value < c->slots[selected].frame.ready_value)) selected = i;
   if (selected == RECORD_GPU_QUEUE_SLOTS) return RECORD_GPU_QUEUE_BUSY;
   image->frame = c->slots[selected].frame; image->resource = c->slots[selected].texture;
   image->state = RPY_D12_IMAGE_STATE; image->texture_mode = c->color.texture_mode;
   c->slots[selected].state = RPY_D12_LENT;
   return RECORD_GPU_QUEUE_OK;
}

bool record_pyrowave_d3d12_image_drop(record_pyrowave_d3d12_t *c, unsigned slot)
{
   if (!c || !c->decode || slot >= c->count || c->slots[slot].state != RPY_D12_LENT) return false;
   c->slots[slot].state = RPY_D12_IDLE;
   return record_gpu_queue_release(c->queue, slot);
}

bool record_pyrowave_d3d12_present(record_pyrowave_d3d12_t *c, unsigned slot, ID3D12Fence *completion, uint64_t value)
{
   if (!c || !c->decode || c->failed || slot >= c->count || c->slots[slot].state != RPY_D12_LENT
         || !completion || !value || value == (uint64_t)-1
         || ID3D12Fence_GetCompletedValue(completion) == (uint64_t)-1) return false;
   /* A frontend may keep an image across duplicate draws and only return it
    * after replacing it. Its final-use label can already have completed or
    * precede another slot's decode; the timeline still protects that use. */
   c->slots[slot].fence = completion; c->slots[slot].value = value; c->slots[slot].state = RPY_D12_DISPLAYING;
   return true;
}

static bool record_pyrowave_d12_encode_complete(record_pyrowave_d3d12_t *c, unsigned slot, bool discard)
{
   record_pyrowave_d12_slot_t *s = &c->slots[slot];
   uint32_t bytes;
   int ready = 2;
   if (!discard)
   {
      bytes = record_pyrowave_d12_load32(s->host + 4);
      if (bytes >= 8 && bytes <= c->layout.maximum_bytes && !record_pyrowave_d12_load32(s->host + 12)) ready = 1;
   }
   record_pyrowave_d12_retire(c, s->frame.ready_value);
   if (c->reservation == slot)
   {
      record_gpu_queue_cancel(c->queue); c->reservation = RECORD_GPU_QUEUE_SLOTS;
   }
   else retro_atomic_store_release_int(&s->ready, ready);
   return ready == 1;
}

bool record_pyrowave_d3d12_poll(record_pyrowave_d3d12_t *c)
{
   unsigned i;
   bool valid = true;
   if (!c) return false;
   for (i = 0; i < c->count; i++)
   {
      record_pyrowave_d12_slot_t *s = &c->slots[i];
      uint64_t completed;
      if (!s->fence) continue;
      completed = ID3D12Fence_GetCompletedValue(s->fence);
      if (completed == (uint64_t)-1)
      {
         c->failed = true;
         continue;
      }
      if (completed < s->value) continue;
      s->fence = NULL;
      if (!c->decode)
      {
         if (!record_pyrowave_d12_encode_complete(c, i, false)) valid = false;
      }
      else if (s->state == RPY_D12_DECODING && !record_pyrowave_d12_load32(s->readback.mapped + 12))
         s->state = RPY_D12_READY;
      else
      {
         if (s->state == RPY_D12_DECODING) valid = false;
         s->state = RPY_D12_IDLE; record_gpu_queue_release(c->queue, i);
      }
   }
   return valid && !c->failed;
}

bool record_pyrowave_d3d12_discard(record_pyrowave_d3d12_t *c, ID3D12Fence *completion, uint64_t value)
{
   unsigned i;
   if (!c || !completion || !value) return false;
   for (i = 0; i < c->count; i++)
   {
      record_pyrowave_d12_slot_t *s = &c->slots[i];
      if (s->fence != completion || s->value != value) continue;
      s->fence = NULL;
      if (!c->decode) record_pyrowave_d12_encode_complete(c, i, true);
      else
      {
         s->state = RPY_D12_IDLE;
         record_gpu_queue_release(c->queue, i);
      }
      return true;
   }
   return false;
}

bool record_pyrowave_d3d12_idle(const record_pyrowave_d3d12_t *c)
{
   unsigned i;
   if (!c) return false;
   for (i = 0; i < c->count; i++) if (c->slots[i].fence) return false;
   return true;
}

ID3D12Resource *record_pyrowave_d3d12_texture(const record_pyrowave_d3d12_t *c, unsigned slot)
{
   return c && c->decode && slot < c->count ? c->slots[slot].texture : NULL;
}

bool record_pyrowave_d3d12_finish(record_pyrowave_d3d12_t *c)
{
   return c && record_gpu_queue_finish(c->queue);
}

bool record_pyrowave_d3d12_free(record_pyrowave_d3d12_t *c)
{
   unsigned i;
   if (!c) return true;
   for (i = 0; i < c->count; i++) if (c->slots[i].fence) return false;
   if (!record_gpu_queue_free(c->queue)) return false;
   record_pyrowave_d12_destroy(c);
   return true;
}

bool record_pyrowave_d3d12_abandon(record_pyrowave_d3d12_t *c)
{
   record_gpu_frame_t frame;
   unsigned i;
   if (!c) return true;
   if (c->reservation != RECORD_GPU_QUEUE_SLOTS) record_gpu_queue_cancel(c->queue);
   for (i = 0; i < c->count; i++) if (record_gpu_queue_take(c->queue, &frame) != RECORD_GPU_QUEUE_OK) break;
   for (i = 0; i < c->count; i++) record_gpu_queue_release(c->queue, i);
   if (!record_gpu_queue_free(c->queue)) return false;
   record_pyrowave_d12_destroy(c);
   return true;
}

RECORD_PYROWAVE_SESSION_BINDING(record_pyrowave_d3d12, record_pyrowave_d3d12_t, record_pyrowave_d3d12_receive_acquire)
