/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_gpu_capture_d3d12.h"
#if defined(_WIN32) && !defined(__WINRT__)
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <windows.h>
#include <d3d12.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "shaders/capture_rgb.h"
#include "shaders/capture_hdr.h"

typedef struct rgc_slot
{
   uint64_t value;
   ID3D12Resource *texture;
   ID3D12Resource *planes[2];
   ID3D12Resource *source;
   ID3D12Fence *source_fence;
   ID3D12CommandAllocator *allocator;
   ID3D12GraphicsCommandList *commands;
   bool hdr_direct;
} rgc_slot_t;

struct record_gpu_capture_d3d12
{
   uint64_t value;
   ID3D12Device *device;
   ID3D12CommandQueue *commands;
   ID3D12Fence *ready;
   ID3D12RootSignature *hdr_root;
   ID3D12PipelineState *hdr_pipeline;
   ID3D12DescriptorHeap *hdr_heap;
   ID3D12RootSignature *rgb_root;
   ID3D12PipelineState *rgb_pipeline;
   ID3D12DescriptorHeap *rgb_srv, *rgb_rtv;
   record_gpu_queue_t *queue;
   rgc_slot_t pool[RECORD_GPU_QUEUE_SLOTS];
   unsigned slots, width, height;
   unsigned srv_stride, rtv_stride;
   enum nvenc_d3d12_format format;
   DXGI_FORMAT dxgi;
   bool failed, unfenced, hdr, full_range, hdr_direct_capable;
};

static void rgc_source_release(rgc_slot_t *s)
{
   if (s->source) ID3D12Resource_Release(s->source);
   if (s->source_fence) ID3D12Fence_Release(s->source_fence);
   s->source = NULL;
   s->source_fence = NULL;
}

static bool rgc_reap(record_gpu_capture_d3d12_t *c, uint64_t completed)
{
   unsigned i;
   bool retired = true;
   for (i = 0; i < c->slots; i++)
   {
      rgc_slot_t *s = &c->pool[i];
      if (!s->source)
         continue;
      if (completed >= s->value
            && (!c->unfenced || s->value < c->value || completed == UINT64_MAX))
         rgc_source_release(s);
      else
         retired = false;
   }
   return retired;
}

bool record_gpu_capture_d3d12_reap(record_gpu_capture_d3d12_t *c)
{
   uint64_t completed;
   if (!c)
      return true;
   completed = ID3D12Fence_GetCompletedValue(c->ready);
   if (completed == UINT64_MAX
         && SUCCEEDED(ID3D12Device_GetDeviceRemovedReason(c->device)))
      return false;
   return rgc_reap(c, completed);
}

static void rgc_destroy(record_gpu_capture_d3d12_t *c)
{
   unsigned i;
   for (i = 0; i < c->slots; i++)
   {
      rgc_source_release(&c->pool[i]);
      if (c->pool[i].planes[0]) ID3D12Resource_Release(c->pool[i].planes[0]);
      if (c->pool[i].planes[1]) ID3D12Resource_Release(c->pool[i].planes[1]);
      if (c->pool[i].commands) ID3D12GraphicsCommandList_Release(c->pool[i].commands);
      if (c->pool[i].allocator) ID3D12CommandAllocator_Release(c->pool[i].allocator);
      if (c->pool[i].texture) ID3D12Resource_Release(c->pool[i].texture);
   }
   if (c->ready) ID3D12Fence_Release(c->ready);
   if (c->hdr_pipeline) ID3D12PipelineState_Release(c->hdr_pipeline);
   if (c->hdr_root) ID3D12RootSignature_Release(c->hdr_root);
   if (c->hdr_heap) ID3D12DescriptorHeap_Release(c->hdr_heap);
   if (c->rgb_pipeline) ID3D12PipelineState_Release(c->rgb_pipeline);
   if (c->rgb_root) ID3D12RootSignature_Release(c->rgb_root);
   if (c->rgb_srv) ID3D12DescriptorHeap_Release(c->rgb_srv);
   if (c->rgb_rtv) ID3D12DescriptorHeap_Release(c->rgb_rtv);
   if (c->commands) ID3D12CommandQueue_Release(c->commands);
   if (c->device) ID3D12Device_Release(c->device);
   free(c);
}

static bool rgc_rgb_new(record_gpu_capture_d3d12_t *c)
{
   D3D12_ROOT_SIGNATURE_DESC root;
   D3D12_ROOT_PARAMETER params[2];
   D3D12_DESCRIPTOR_RANGE range;
   D3D12_STATIC_SAMPLER_DESC sampler;
   D3D12_DESCRIPTOR_HEAP_DESC heap;
   D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeline;
   ID3DBlob *blob = NULL;
   HRESULT result;
   memset(&root, 0, sizeof(root)); memset(params, 0, sizeof(params));
   memset(&range, 0, sizeof(range)); memset(&sampler, 0, sizeof(sampler));
   range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; range.NumDescriptors = 1;
   params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
   params[0].Constants.Num32BitValues = 4;
   params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
   params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
   params[1].DescriptorTable.NumDescriptorRanges = 1;
   params[1].DescriptorTable.pDescriptorRanges = &range;
   params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
   sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
   sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
   sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
   sampler.MaxAnisotropy = 1;
   sampler.MaxLOD = D3D12_FLOAT32_MAX;
   sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
   root.NumParameters = 2; root.pParameters = params;
   root.NumStaticSamplers = 1; root.pStaticSamplers = &sampler;
   result = D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &blob, NULL);
   if (FAILED(result))
      return false;
   result = ID3D12Device_CreateRootSignature(c->device, 0,
         blob->lpVtbl->GetBufferPointer(blob), blob->lpVtbl->GetBufferSize(blob),
         &IID_ID3D12RootSignature, (void**)&c->rgb_root);
   blob->lpVtbl->Release(blob);
   if (FAILED(result))
      return false;
   memset(&heap, 0, sizeof(heap));
   heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
   heap.NumDescriptors = c->slots; heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
   if (FAILED(ID3D12Device_CreateDescriptorHeap(c->device, &heap,
            &IID_ID3D12DescriptorHeap, (void**)&c->rgb_srv)))
      return false;
   heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
   if (FAILED(ID3D12Device_CreateDescriptorHeap(c->device, &heap,
            &IID_ID3D12DescriptorHeap, (void**)&c->rgb_rtv)))
      return false;
   c->srv_stride = ID3D12Device_GetDescriptorHandleIncrementSize(c->device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
   c->rtv_stride = ID3D12Device_GetDescriptorHandleIncrementSize(c->device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
   memset(&pipeline, 0, sizeof(pipeline));
   pipeline.pRootSignature = c->rgb_root;
   pipeline.VS.pShaderBytecode = rgc_vs; pipeline.VS.BytecodeLength = sizeof(rgc_vs);
   pipeline.PS.pShaderBytecode = rgc_ps; pipeline.PS.BytecodeLength = sizeof(rgc_ps);
   pipeline.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
   pipeline.BlendState.RenderTarget[0].SrcBlend = pipeline.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
   pipeline.BlendState.RenderTarget[0].DestBlend = pipeline.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
   pipeline.BlendState.RenderTarget[0].BlendOp = pipeline.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
   pipeline.BlendState.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
   pipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
   pipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
   pipeline.RasterizerState.DepthClipEnable = TRUE;
   pipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
   pipeline.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
   pipeline.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
   pipeline.DepthStencilState.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
   pipeline.DepthStencilState.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
   pipeline.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
   pipeline.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
   pipeline.DepthStencilState.BackFace = pipeline.DepthStencilState.FrontFace;
   pipeline.SampleMask = UINT_MAX;
   pipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
   pipeline.NumRenderTargets = 1; pipeline.RTVFormats[0] = c->dxgi;
   pipeline.SampleDesc.Count = 1;
   return SUCCEEDED(ID3D12Device_CreateGraphicsPipelineState(c->device, &pipeline,
            &IID_ID3D12PipelineState, (void**)&c->rgb_pipeline));
}

static bool rgc_hdr_new(record_gpu_capture_d3d12_t *c)
{
   D3D12_ROOT_SIGNATURE_DESC root;
   D3D12_ROOT_PARAMETER params[2];
   D3D12_DESCRIPTOR_RANGE ranges[2];
   D3D12_DESCRIPTOR_HEAP_DESC heap;
   D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline;
   D3D12_FEATURE_DATA_FORMAT_SUPPORT support;
   ID3DBlob *blob = NULL;
   HRESULT result;
   unsigned i;
   for (i = 0; i < 2; i++)
   {
      memset(&support, 0, sizeof(support));
      support.Format = i ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R16_UNORM;
      if (FAILED(ID3D12Device_CheckFeatureSupport(c->device, D3D12_FEATURE_FORMAT_SUPPORT,
                  &support, sizeof(support)))
            || !(support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE))
         return false;
   }
   support.Format = DXGI_FORMAT_P010;
   c->hdr_direct_capable = SUCCEEDED(ID3D12Device_CheckFeatureSupport(c->device,
            D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)))
      && (support.Support1 & D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW)
      && (support.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);
   memset(&root, 0, sizeof(root)); memset(params, 0, sizeof(params));
   memset(ranges, 0, sizeof(ranges));
   ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
   ranges[0].NumDescriptors = 1;
   ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
   ranges[1].NumDescriptors = 2;
   ranges[1].OffsetInDescriptorsFromTableStart = 1;
   params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
   params[0].Constants.Num32BitValues = 6;
   params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
   params[1].DescriptorTable.NumDescriptorRanges = 2;
   params[1].DescriptorTable.pDescriptorRanges = ranges;
   root.NumParameters = 2; root.pParameters = params;
   result = D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &blob, NULL);
   if (FAILED(result)) return false;
   result = ID3D12Device_CreateRootSignature(c->device, 0,
         blob->lpVtbl->GetBufferPointer(blob), blob->lpVtbl->GetBufferSize(blob),
         &IID_ID3D12RootSignature, (void**)&c->hdr_root);
   blob->lpVtbl->Release(blob);
   if (FAILED(result)) return false;
   memset(&heap, 0, sizeof(heap));
   heap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
   heap.NumDescriptors = c->slots * 3;
   heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
   if (FAILED(ID3D12Device_CreateDescriptorHeap(c->device, &heap,
               &IID_ID3D12DescriptorHeap, (void**)&c->hdr_heap)))
      return false;
   c->srv_stride = ID3D12Device_GetDescriptorHandleIncrementSize(c->device, heap.Type);
   memset(&pipeline, 0, sizeof(pipeline));
   pipeline.pRootSignature = c->hdr_root;
   pipeline.CS.pShaderBytecode = rgh_cs; pipeline.CS.BytecodeLength = sizeof(rgh_cs);
   return SUCCEEDED(ID3D12Device_CreateComputePipelineState(c->device, &pipeline,
            &IID_ID3D12PipelineState, (void**)&c->hdr_pipeline));
}

record_gpu_capture_d3d12_t *record_gpu_capture_d3d12_new(
      const nvenc_d3d12_config_t *config, void *command_queue)
{
   static const DXGI_FORMAT formats[] = {DXGI_FORMAT_R8G8B8A8_UNORM,
      DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_NV12, DXGI_FORMAT_P010};
   record_gpu_capture_d3d12_t *c;
   ID3D12Device *device = NULL;
   D3D12_HEAP_PROPERTIES heap;
   D3D12_RESOURCE_DESC desc;
   D3D12_COMMAND_QUEUE_DESC qdesc;
   D3D12_COMMAND_QUEUE_DESC *(STDMETHODCALLTYPE *get_desc)(ID3D12CommandQueue*, D3D12_COMMAND_QUEUE_DESC*);
   unsigned i;
   if (!config || !config->device || !command_queue
         || config->slots < 4 || config->slots > RECORD_GPU_QUEUE_SLOTS
         || !config->width || !config->height || ((config->width | config->height) & 1)
         || config->width > 16384 || config->height > 16384
         || (unsigned)config->format > NVENC_D3D12_P010)
      return NULL;
   memcpy(&get_desc, &((ID3D12CommandQueue*)command_queue)->lpVtbl->GetDesc, sizeof(get_desc));
   get_desc((ID3D12CommandQueue*)command_queue, &qdesc);
   if (qdesc.Type != D3D12_COMMAND_LIST_TYPE_DIRECT
         || FAILED(ID3D12CommandQueue_GetDevice((ID3D12CommandQueue*)command_queue,
            &IID_ID3D12Device, (void**)&device)))
      return NULL;
   ID3D12Device_Release(device);
   if (device != config->device)
      return NULL;
   c = (record_gpu_capture_d3d12_t*)calloc(1, sizeof(*c));
   if (!c)
      return NULL;
   c->device = device; ID3D12Device_AddRef(device);
   c->commands = (ID3D12CommandQueue*)command_queue;
   ID3D12CommandQueue_AddRef(c->commands);
   c->slots = config->slots; c->width = config->width; c->height = config->height;
   c->format = config->format; c->dxgi = formats[config->format];
   c->hdr = config->format == NVENC_D3D12_P010 && config->color == NVENC_D3D12_COLOR_BT2020_PQ;
   c->full_range = config->full_range;
   if (c->hdr && !rgc_hdr_new(c))
      goto fail;
   c->queue = record_gpu_queue_new(c->slots);
   if (!c->queue || FAILED(ID3D12Device_CreateFence(device, 0,
            D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**)&c->ready)))
      goto fail;
   memset(&heap, 0, sizeof(heap)); heap.Type = D3D12_HEAP_TYPE_DEFAULT;
   memset(&desc, 0, sizeof(desc));
   desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   desc.Width = c->width; desc.Height = c->height; desc.Format = c->dxgi;
   desc.DepthOrArraySize = desc.MipLevels = desc.SampleDesc.Count = 1;
   if (c->format == NVENC_D3D12_BGRA8)
   {
      if (!rgc_rgb_new(c))
         goto fail;
      desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
   }
   for (i = 0; i < c->slots; i++)
   {
      rgc_slot_t *s = &c->pool[i];
      D3D12_RESOURCE_DESC output = desc;
      unsigned plane;
      output.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
      s->hdr_direct = c->hdr_direct_capable
         && SUCCEEDED(ID3D12Device_CreateCommittedResource(device, &heap,
               D3D12_HEAP_FLAG_NONE, &output, D3D12_RESOURCE_STATE_COMMON,
               NULL, &IID_ID3D12Resource, (void**)&s->texture));
      if (!s->hdr_direct && FAILED(ID3D12Device_CreateCommittedResource(device, &heap,
               D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON,
               NULL, &IID_ID3D12Resource, (void**)&s->texture)))
         goto fail;
      for (plane = 0; c->hdr && !s->hdr_direct && plane < 2; plane++)
      {
         D3D12_RESOURCE_DESC planar = desc;
         planar.Format = plane ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R16_UNORM;
         planar.Width >>= plane; planar.Height >>= plane;
         planar.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
         if (FAILED(ID3D12Device_CreateCommittedResource(device, &heap,
                     D3D12_HEAP_FLAG_NONE, &planar, D3D12_RESOURCE_STATE_COMMON,
                     NULL, &IID_ID3D12Resource, (void**)&s->planes[plane])))
            goto fail;
      }
      if (FAILED(ID3D12Device_CreateCommandAllocator(device, D3D12_COMMAND_LIST_TYPE_DIRECT,
               &IID_ID3D12CommandAllocator, (void**)&s->allocator))
            || FAILED(ID3D12Device_CreateCommandList(device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT,
               s->allocator, NULL, &IID_ID3D12GraphicsCommandList, (void**)&s->commands))
            || FAILED(ID3D12GraphicsCommandList_Close(s->commands)))
         goto fail;
   }
   return c;
fail:
   record_gpu_queue_free(c->queue);
   rgc_destroy(c);
   return NULL;
}

bool record_gpu_capture_d3d12_bind(record_gpu_capture_d3d12_t *c, nvenc_d3d12_config_t *config)
{
   unsigned i;
   if (!c || !config || (c->hdr && (config->color != NVENC_D3D12_COLOR_BT2020_PQ
               || config->full_range != c->full_range)))
      return false;
   config->device = c->device; config->slots = c->slots;
   config->width = c->width; config->height = c->height; config->format = c->format;
   for (i = 0; i < NVENC_D3D12_SLOTS; i++)
      config->textures[i] = i < c->slots ? c->pool[i].texture : NULL;
   return true;
}

record_gpu_queue_t *record_gpu_capture_d3d12_queue(record_gpu_capture_d3d12_t *c)
{
   return c ? c->queue : NULL;
}

void *record_gpu_capture_d3d12_fence(record_gpu_capture_d3d12_t *c)
{
   return c ? c->ready : NULL;
}

static int rgc_fail(record_gpu_capture_d3d12_t *c)
{
   c->failed = true;
   record_gpu_queue_cancel(c->queue);
   record_gpu_queue_finish(c->queue);
   return RECORD_GPU_QUEUE_ERROR;
}

static void rgc_rgb_draw(record_gpu_capture_d3d12_t *c, rgc_slot_t *s,
      unsigned index, ID3D12Resource *source, const D3D12_RESOURCE_DESC *desc,
      unsigned x, unsigned y)
{
   D3D12_SHADER_RESOURCE_VIEW_DESC srv;
   D3D12_CPU_DESCRIPTOR_HANDLE cpu_srv, rtv;
   D3D12_GPU_DESCRIPTOR_HANDLE gpu_srv;
   D3D12_VIEWPORT viewport;
   D3D12_RECT scissor;
   float crop[4];
   /* Descriptor-handle getters have COM's explicit return buffer ABI. */
   D3D12_CPU_DESCRIPTOR_HANDLE *(STDMETHODCALLTYPE *get_cpu)(ID3D12DescriptorHeap*, D3D12_CPU_DESCRIPTOR_HANDLE*);
   D3D12_GPU_DESCRIPTOR_HANDLE *(STDMETHODCALLTYPE *get_gpu)(ID3D12DescriptorHeap*, D3D12_GPU_DESCRIPTOR_HANDLE*);
   memcpy(&get_cpu, &c->rgb_srv->lpVtbl->GetCPUDescriptorHandleForHeapStart, sizeof(get_cpu));
   get_cpu(c->rgb_srv, &cpu_srv);
   memcpy(&get_gpu, &c->rgb_srv->lpVtbl->GetGPUDescriptorHandleForHeapStart, sizeof(get_gpu));
   get_gpu(c->rgb_srv, &gpu_srv);
   memcpy(&get_cpu, &c->rgb_rtv->lpVtbl->GetCPUDescriptorHandleForHeapStart, sizeof(get_cpu));
   get_cpu(c->rgb_rtv, &rtv);
   cpu_srv.ptr += index * c->srv_stride;
   gpu_srv.ptr += index * c->srv_stride;
   rtv.ptr += index * c->rtv_stride;
   memset(&srv, 0, sizeof(srv));
   srv.Format = desc->Format; srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
   srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   srv.Texture2D.MipLevels = 1;
   ID3D12Device_CreateShaderResourceView(c->device, source, &srv, cpu_srv);
   ID3D12Device_CreateRenderTargetView(c->device, s->texture, NULL, rtv);
   viewport.TopLeftX = viewport.TopLeftY = viewport.MinDepth = 0;
   viewport.Width = (float)c->width; viewport.Height = (float)c->height; viewport.MaxDepth = 1;
   scissor.left = scissor.top = 0; scissor.right = c->width; scissor.bottom = c->height;
   crop[0] = x / (float)desc->Width; crop[1] = y / (float)desc->Height;
   crop[2] = c->width / (float)desc->Width; crop[3] = c->height / (float)desc->Height;
   ID3D12GraphicsCommandList_SetPipelineState(s->commands, c->rgb_pipeline);
   ID3D12GraphicsCommandList_SetGraphicsRootSignature(s->commands, c->rgb_root);
   ID3D12GraphicsCommandList_SetDescriptorHeaps(s->commands, 1, &c->rgb_srv);
   ID3D12GraphicsCommandList_SetGraphicsRoot32BitConstants(s->commands, 0, 4, crop, 0);
   ID3D12GraphicsCommandList_SetGraphicsRootDescriptorTable(s->commands, 1, gpu_srv);
   ID3D12GraphicsCommandList_RSSetViewports(s->commands, 1, &viewport);
   ID3D12GraphicsCommandList_RSSetScissorRects(s->commands, 1, &scissor);
   ID3D12GraphicsCommandList_OMSetRenderTargets(s->commands, 1, &rtv, FALSE, NULL);
   ID3D12GraphicsCommandList_IASetPrimitiveTopology(s->commands, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   ID3D12GraphicsCommandList_DrawInstanced(s->commands, 3, 1, 0, 0);
}

static void rgc_hdr_dispatch(record_gpu_capture_d3d12_t *c, rgc_slot_t *s,
      unsigned index, ID3D12Resource *source, DXGI_FORMAT format, unsigned x, unsigned y)
{
   D3D12_SHADER_RESOURCE_VIEW_DESC srv;
   D3D12_UNORDERED_ACCESS_VIEW_DESC uav;
   D3D12_CPU_DESCRIPTOR_HANDLE cpu;
   D3D12_GPU_DESCRIPTOR_HANDLE gpu;
   D3D12_RESOURCE_BARRIER barriers[2];
   unsigned constants[6], i;
   D3D12_CPU_DESCRIPTOR_HANDLE *(STDMETHODCALLTYPE *get_cpu)(ID3D12DescriptorHeap*, D3D12_CPU_DESCRIPTOR_HANDLE*);
   D3D12_GPU_DESCRIPTOR_HANDLE *(STDMETHODCALLTYPE *get_gpu)(ID3D12DescriptorHeap*, D3D12_GPU_DESCRIPTOR_HANDLE*);
   memcpy(&get_cpu, &c->hdr_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart, sizeof(get_cpu));
   get_cpu(c->hdr_heap, &cpu);
   memcpy(&get_gpu, &c->hdr_heap->lpVtbl->GetGPUDescriptorHandleForHeapStart, sizeof(get_gpu));
   get_gpu(c->hdr_heap, &gpu);
   cpu.ptr += index * 3 * c->srv_stride; gpu.ptr += index * 3 * c->srv_stride;
   memset(&srv, 0, sizeof(srv));
   srv.Format = format; srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
   srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   srv.Texture2D.MipLevels = 1;
   ID3D12Device_CreateShaderResourceView(c->device, source, &srv, cpu);
   memset(barriers, 0, sizeof(barriers));
   for (i = 0; i < 2; i++)
   {
      cpu.ptr += c->srv_stride;
      memset(&uav, 0, sizeof(uav));
      uav.Format = i ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R16_UNORM;
      uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
      uav.Texture2D.PlaneSlice = s->hdr_direct ? i : 0;
      ID3D12Device_CreateUnorderedAccessView(c->device,
            s->hdr_direct ? s->texture : s->planes[i], NULL, &uav, cpu);
      if (!s->hdr_direct)
      {
         barriers[i].Transition.pResource = s->planes[i];
         barriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
         barriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
         barriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
      }
   }
   if (!s->hdr_direct)
      ID3D12GraphicsCommandList_ResourceBarrier(s->commands, 2, barriers);
   constants[0] = x; constants[1] = y; constants[2] = c->width; constants[3] = c->height;
   constants[4] = format == DXGI_FORMAT_R16G16B16A16_FLOAT; constants[5] = c->full_range;
   ID3D12GraphicsCommandList_SetPipelineState(s->commands, c->hdr_pipeline);
   ID3D12GraphicsCommandList_SetComputeRootSignature(s->commands, c->hdr_root);
   ID3D12GraphicsCommandList_SetDescriptorHeaps(s->commands, 1, &c->hdr_heap);
   ID3D12GraphicsCommandList_SetComputeRoot32BitConstants(s->commands, 0, 6, constants, 0);
   ID3D12GraphicsCommandList_SetComputeRootDescriptorTable(s->commands, 1, gpu);
   ID3D12GraphicsCommandList_Dispatch(s->commands, (c->width + 15) / 16, (c->height + 15) / 16, 1);
   if (s->hdr_direct)
      return;
   for (i = 0; i < 2; i++)
   {
      barriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
      barriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
   }
   ID3D12GraphicsCommandList_ResourceBarrier(s->commands, 2, barriers);
   for (i = 0; i < 2; i++)
   {
      D3D12_TEXTURE_COPY_LOCATION from, to;
      memset(&from, 0, sizeof(from)); memset(&to, 0, sizeof(to));
      from.pResource = s->planes[i]; to.pResource = s->texture;
      from.Type = to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      to.SubresourceIndex = i;
      ID3D12GraphicsCommandList_CopyTextureRegion(s->commands, &to, 0, 0, 0, &from, NULL);
      barriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
      barriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
   }
   ID3D12GraphicsCommandList_ResourceBarrier(s->commands, 2, barriers);
}

int record_gpu_capture_d3d12_push(record_gpu_capture_d3d12_t *c,
      void *source, unsigned source_state, unsigned x, unsigned y,
      void *source_fence, uint64_t source_value,
      uint64_t timestamp, uint64_t duration, bool keyframe, uint64_t *ready_value)
{
   D3D12_RESOURCE_DESC desc;
   D3D12_RESOURCE_DESC *(STDMETHODCALLTYPE *get_desc)(ID3D12Resource*, D3D12_RESOURCE_DESC*);
   D3D12_RESOURCE_BARRIER barriers[2];
   ID3D12Device *device = NULL;
   ID3D12Resource *input = (ID3D12Resource*)source;
   record_gpu_frame_t frame;
   rgc_slot_t *s;
   unsigned i, count = 0;
   bool convert, hdr;
   D3D12_RESOURCE_STATES input_state, output_state;
   uint64_t completed;
   int result;
   if (ready_value)
      *ready_value = 0;
   if (!c || !source || !duration || c->failed || (!source_fence && source_value)
         || c->value >= UINT64_MAX - 1)
      return RECORD_GPU_QUEUE_ERROR;
   for (i = 0; i < c->slots; i++)
      if (source == c->pool[i].texture)
         return RECORD_GPU_QUEUE_ERROR;
   memcpy(&get_desc, &input->lpVtbl->GetDesc, sizeof(get_desc));
   get_desc(input, &desc);
   hdr = c->hdr && (desc.Format == DXGI_FORMAT_R10G10B10A2_UNORM
         || desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT);
   convert = c->format == NVENC_D3D12_BGRA8 && desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM;
   if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || (!convert && !hdr && desc.Format != c->dxgi)
         || ((convert || hdr) && (desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE))
         || desc.DepthOrArraySize != 1 || desc.MipLevels != 1 || desc.SampleDesc.Count != 1
         || desc.Width < c->width || x > desc.Width - c->width
         || desc.Height < c->height || y > desc.Height - c->height
         || (!hdr && c->format >= NVENC_D3D12_NV12
            && (x || y || desc.Width != c->width || desc.Height != c->height)))
      return RECORD_GPU_QUEUE_ERROR;
   if (FAILED(ID3D12Resource_GetDevice(input, &IID_ID3D12Device, (void**)&device)))
      return RECORD_GPU_QUEUE_ERROR;
   ID3D12Device_Release(device);
   if (device != c->device)
      return RECORD_GPU_QUEUE_ERROR;
   if (source_fence)
   {
      if (FAILED(ID3D12Fence_GetDevice((ID3D12Fence*)source_fence,
               &IID_ID3D12Device, (void**)&device)))
         return RECORD_GPU_QUEUE_ERROR;
      ID3D12Device_Release(device);
      if (device != c->device || source_fence == c->ready)
         return RECORD_GPU_QUEUE_ERROR;
   }
   result = record_gpu_queue_acquire(c->queue, &frame.slot);
   if (result != RECORD_GPU_QUEUE_OK)
      return result;
   s = &c->pool[frame.slot];
   completed = ID3D12Fence_GetCompletedValue(c->ready);
   if (completed == UINT64_MAX)
      return rgc_fail(c);
   if (completed < s->value)
   {
      record_gpu_queue_cancel(c->queue);
      return RECORD_GPU_QUEUE_BUSY;
   }
   if (FAILED(ID3D12CommandAllocator_Reset(s->allocator))
         || FAILED(ID3D12GraphicsCommandList_Reset(s->commands, s->allocator, NULL)))
      return rgc_fail(c);
   memset(barriers, 0, sizeof(barriers));
   input_state = hdr ? D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
      : (convert ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_COPY_SOURCE);
   output_state = hdr && s->hdr_direct ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS
      : (convert ? D3D12_RESOURCE_STATE_RENDER_TARGET : D3D12_RESOURCE_STATE_COPY_DEST);
   if (source_state != (unsigned)input_state)
   {
      barriers[count].Transition.pResource = input;
      barriers[count].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      barriers[count].Transition.StateBefore = (D3D12_RESOURCE_STATES)source_state;
      barriers[count++].Transition.StateAfter = input_state;
   }
   barriers[count].Transition.pResource = s->texture;
   barriers[count].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
   barriers[count].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
   barriers[count++].Transition.StateAfter = output_state;
   ID3D12GraphicsCommandList_ResourceBarrier(s->commands, count, barriers);
   if (hdr)
      rgc_hdr_dispatch(c, s, frame.slot, input, desc.Format, x, y);
   else if (convert)
      rgc_rgb_draw(c, s, frame.slot, input, &desc, x, y);
   else if (!x && !y && desc.Width == c->width && desc.Height == c->height)
      ID3D12GraphicsCommandList_CopyResource(s->commands, s->texture, input);
   else
   {
      D3D12_TEXTURE_COPY_LOCATION from, to;
      D3D12_BOX box;
      memset(&from, 0, sizeof(from)); memset(&to, 0, sizeof(to));
      from.pResource = input; to.pResource = s->texture;
      from.Type = to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      box.left = x; box.top = y; box.front = 0;
      box.right = x + c->width; box.bottom = y + c->height; box.back = 1;
      ID3D12GraphicsCommandList_CopyTextureRegion(s->commands, &to, 0, 0, 0, &from, &box);
   }
   for (i = 0; i < count; i++)
   {
      D3D12_RESOURCE_STATES state = barriers[i].Transition.StateBefore;
      barriers[i].Transition.StateBefore = barriers[i].Transition.StateAfter;
      barriers[i].Transition.StateAfter = state;
   }
   ID3D12GraphicsCommandList_ResourceBarrier(s->commands, count, barriers);
   if (FAILED(ID3D12GraphicsCommandList_Close(s->commands)))
      return rgc_fail(c);
   if (source_fence && FAILED(ID3D12CommandQueue_Wait(c->commands,
            (ID3D12Fence*)source_fence, source_value)))
      return rgc_fail(c);
   ID3D12Resource_AddRef(input);
   if (source_fence) ID3D12Fence_AddRef((ID3D12Fence*)source_fence);
   rgc_reap(c, completed);
   s->source = input;
   s->source_fence = (ID3D12Fence*)source_fence;
   {
      ID3D12CommandList *list = (ID3D12CommandList*)s->commands;
      ID3D12CommandQueue_ExecuteCommandLists(c->commands, 1, &list);
   }
   s->value = ++c->value;
   if (ready_value)
      *ready_value = c->value;
   if (FAILED(ID3D12CommandQueue_Signal(c->commands, c->ready, c->value)))
   {
      c->unfenced = true;
      return rgc_fail(c);
   }
   frame.timestamp = timestamp; frame.duration = duration;
   frame.ready_value = c->value; frame.keyframe = keyframe;
   if (!record_gpu_queue_submit(c->queue, &frame))
      return rgc_fail(c);
   return RECORD_GPU_QUEUE_OK;
}

bool record_gpu_capture_d3d12_finish(record_gpu_capture_d3d12_t *c)
{
   return c && record_gpu_queue_finish(c->queue);
}

bool record_gpu_capture_d3d12_free(record_gpu_capture_d3d12_t *c)
{
   if (!c)
      return true;
   if (SUCCEEDED(ID3D12Device_GetDeviceRemovedReason(c->device)))
   {
      if (c->unfenced)
      {
         if (FAILED(ID3D12CommandQueue_Signal(c->commands, c->ready, c->value)))
            return false;
         c->unfenced = false;
      }
      if (ID3D12Fence_GetCompletedValue(c->ready) < c->value)
         return false;
   }
   if (!record_gpu_queue_free(c->queue))
      return false;
   rgc_destroy(c);
   return true;
}
#else
record_gpu_capture_d3d12_t *record_gpu_capture_d3d12_new(
      const nvenc_d3d12_config_t *config, void *commands)
{ (void)config; (void)commands; return NULL; }
bool record_gpu_capture_d3d12_bind(record_gpu_capture_d3d12_t *c, nvenc_d3d12_config_t *config)
{ (void)c; (void)config; return false; }
record_gpu_queue_t *record_gpu_capture_d3d12_queue(record_gpu_capture_d3d12_t *c)
{ (void)c; return NULL; }
void *record_gpu_capture_d3d12_fence(record_gpu_capture_d3d12_t *c)
{ (void)c; return NULL; }
int record_gpu_capture_d3d12_push(record_gpu_capture_d3d12_t *c,
      void *source, unsigned state, unsigned x, unsigned y,
      void *fence, uint64_t value, uint64_t timestamp, uint64_t duration, bool keyframe, uint64_t *ready_value)
{
   (void)c; (void)source; (void)state; (void)x; (void)y;
   (void)fence; (void)value; (void)timestamp; (void)duration; (void)keyframe;
   if (ready_value) *ready_value = 0;
   return RECORD_GPU_QUEUE_ERROR;
}
bool record_gpu_capture_d3d12_finish(record_gpu_capture_d3d12_t *c)
{ (void)c; return false; }
bool record_gpu_capture_d3d12_reap(record_gpu_capture_d3d12_t *c)
{ return c == NULL; }
bool record_gpu_capture_d3d12_free(record_gpu_capture_d3d12_t *c)
{ return c == NULL; }
#endif
