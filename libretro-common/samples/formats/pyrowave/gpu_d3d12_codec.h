/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef RPYROWAVE_D3D12_CODEC_TEST_H
#define RPYROWAVE_D3D12_CODEC_TEST_H
#include "gpu_packet_vectors.h"

typedef struct rpy_d3d_wire
{
   rpy_d3d_test_t *base;
   ID3D12RootSignature *root;
   ID3D12PipelineState *pipeline;
   ID3D12Resource *blocks;
   ID3D12Resource *workspace;
   rpyrowave_packet_parameters_t decode;
} rpy_d3d_wire_t;

static int rpy_d3d_packet_pipeline(ID3D12Device *device, rpy_d3d_wire_t *test,
      const char *path)
{
   D3D12_ROOT_PARAMETER parameters[5];
   D3D12_ROOT_SIGNATURE_DESC root;
   D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline;
   ID3DBlob *serialized = NULL;
   ID3DBlob *errors = NULL;
   uint8_t *shader;
   size_t bytes;
   unsigned i;
   int success = 0;
   shader = rpy_gpu_test_file(path, &bytes);
   if (!shader)
      return 0;
   for (i = 0; i < 4; i++)
   {
      parameters[i].ParameterType = i < 2 ? D3D12_ROOT_PARAMETER_TYPE_SRV
         : D3D12_ROOT_PARAMETER_TYPE_UAV;
      parameters[i].Descriptor.ShaderRegister = i & 1;
      parameters[i].Descriptor.RegisterSpace = 0;
      parameters[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
   }
   parameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
   parameters[4].Constants.ShaderRegister = 0;
   parameters[4].Constants.RegisterSpace = 0;
   parameters[4].Constants.Num32BitValues = 12;
   parameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
   root.NumParameters = 5;
   root.pParameters = parameters;
   root.NumStaticSamplers = 0;
   root.pStaticSamplers = NULL;
   root.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
   if (FAILED(D3D12SerializeRootSignature(&root, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors)))
      goto end;
   if (FAILED(ID3D12Device_CreateRootSignature(device, 0, ID3D10Blob_GetBufferPointer(serialized),
               ID3D10Blob_GetBufferSize(serialized), &IID_ID3D12RootSignature, (void**)&test->root)))
      goto end;
   pipeline.pRootSignature = test->root;
   pipeline.CS.pShaderBytecode = shader;
   pipeline.CS.BytecodeLength = bytes;
   pipeline.NodeMask = 0;
   pipeline.CachedPSO.pCachedBlob = NULL;
   pipeline.CachedPSO.CachedBlobSizeInBytes = 0;
   pipeline.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
   success = SUCCEEDED(ID3D12Device_CreateComputePipelineState(device, &pipeline,
            &IID_ID3D12PipelineState, (void**)&test->pipeline));
end:
   RPY_D3D_RELEASE(serialized);
   RPY_D3D_RELEASE(errors);
   free(shader);
   return success;
}

static int rpy_d3d_packet_dispatch(void *userdata, const rpyrowave_packet_parameters_t *parameters,
      unsigned x, unsigned y)
{
   rpy_d3d_wire_t *test = (rpy_d3d_wire_t*)userdata;
   ID3D12GraphicsCommandList *list = test->base->list;
   unsigned decode = parameters->operation == RPYROWAVE_PACKET_UNPACK;
   ID3D12GraphicsCommandList_SetPipelineState(list, test->pipeline);
   ID3D12GraphicsCommandList_SetComputeRootSignature(list, test->root);
   ID3D12GraphicsCommandList_SetComputeRootShaderResourceView(list, 0,
         ID3D12Resource_GetGPUVirtualAddress(test->base->atlas[decode ? 1 : 0]));
   ID3D12GraphicsCommandList_SetComputeRootShaderResourceView(list, 1,
         ID3D12Resource_GetGPUVirtualAddress(test->blocks));
   ID3D12GraphicsCommandList_SetComputeRootUnorderedAccessView(list, 2,
         ID3D12Resource_GetGPUVirtualAddress(test->workspace));
   ID3D12GraphicsCommandList_SetComputeRootUnorderedAccessView(list, 3,
         ID3D12Resource_GetGPUVirtualAddress(test->base->atlas[decode ? 0 : 1]));
   ID3D12GraphicsCommandList_SetComputeRoot32BitConstants(list, 4, 12, parameters, 0);
   ID3D12GraphicsCommandList_Dispatch(list, x, y, 1);
   {
      D3D12_RESOURCE_BARRIER barrier;
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
      barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      barrier.UAV.pResource = test->workspace;
      ID3D12GraphicsCommandList_ResourceBarrier(list, 1, &barrier);
   }
   test->decode = *parameters;
   return 1;
}

static int rpy_d3d_upload(ID3D12Resource *resource, const void *data, size_t bytes)
{
   D3D12_RANGE range;
   void *mapped;
   range.Begin = range.End = 0;
   if (FAILED(ID3D12Resource_Map(resource, 0, &range, &mapped)))
      return 0;
   memcpy(mapped, data, bytes);
   range.End = bytes;
   ID3D12Resource_Unmap(resource, 0, &range);
   return 1;
}

static int rpy_d3d_wire_decode(rpy_d3d_wire_t *test,
      const rpyrowave_gpu_layout_t *layout, const rpyrowave_packet_parameters_t *parameters)
{
   rpy_d3d_test_t *base = test->base;
   rpy_d3d_transition(base->list, base->atlas[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
         D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   rpy_d3d_transition(base->list, base->atlas[1], D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
         D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   if (rpyrowave_gpu_decode_record(layout, parameters, rpy_d3d_packet_dispatch, test))
      return 0;
   rpy_d3d_transition(base->list, base->atlas[0], D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
         D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   rpy_d3d_transition(base->list, base->atlas[1], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
         D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   return 1;
}

static int rpy_d3d_wire_inverse(rpy_d3d_wire_t *test, const rpy_gpu_vector_t *vector,
      const rpyrowave_gpu_layout_t *layout, const rpyrowave_packet_parameters_t *parameters,
      ID3D12RootSignature *root, ID3D12PipelineState *pipeline)
{
   rpy_d3d_test_t *base = test->base;
   if (!rpy_d3d_wire_decode(test, layout, parameters))
      return 0;
   ID3D12GraphicsCommandList_SetPipelineState(base->list, pipeline);
   ID3D12GraphicsCommandList_SetComputeRootSignature(base->list, root);
   return !rpyrowave_dwt_record(vector->aligned_width, vector->aligned_height,
         vector->sequence.chroma_444, 1, rpy_d3d_dispatch, base);
}

static void rpy_d3d_wire_copy(rpy_d3d_wire_t *test, const rpy_gpu_vector_t *vector,
      const rpyrowave_gpu_layout_t *layout, ID3D12Resource *wire, ID3D12Resource *pixels)
{
   rpy_d3d_test_t *base = test->base;
   rpy_d3d_transition(base->list, test->workspace, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
         D3D12_RESOURCE_STATE_COPY_SOURCE);
   ID3D12GraphicsCommandList_CopyBufferRegion(base->list, wire, 0, test->workspace, 0, layout->workspace_bytes);
   rpy_d3d_transition(base->list, test->workspace, D3D12_RESOURCE_STATE_COPY_SOURCE,
         D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   rpy_d3d_transition(base->list, base->atlas[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
         D3D12_RESOURCE_STATE_COPY_SOURCE);
   ID3D12GraphicsCommandList_CopyBufferRegion(base->list, pixels, 0, base->atlas[0], 0, vector->bytes);
   rpy_d3d_transition(base->list, base->atlas[0], D3D12_RESOURCE_STATE_COPY_SOURCE,
         D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
}

static int rpy_gpu_image_compare(const rpy_gpu_vector_t *vector, const float *samples,
      const rpyrowave_image_t *expected)
{
   size_t pixels = (size_t)vector->aligned_width * vector->aligned_height;
   unsigned component;
   double maximum = 0.0;
   for (component = 0; component < 3; component++)
   {
      unsigned shift = component && !vector->sequence.chroma_444 ? 1 : 0;
      unsigned x;
      unsigned y;
      for (y = 0; y < vector->sequence.height >> shift; y++)
         for (x = 0; x < vector->sequence.width >> shift; x++)
         {
            double value = samples[component * pixels + y * vector->aligned_width + x] + 0.5;
            double error;
            if (value < 0.0) value = 0.0;
            if (value > 1.0) value = 1.0;
            error = fabs(value - expected->planes[component][y * expected->stride + x]);
            if (!(error <= 0.0001))
               return 0;
            if (error > maximum)
               maximum = error;
         }
   }
   printf("GPU packet decode versus CPU: maximum error %.9f\n", maximum);
   return 1;
}

static int rpy_d3d_codec_run(ID3D12Device *device, ID3D12CommandQueue *queue,
      ID3D12CommandAllocator *allocator, ID3D12Fence *fence, HANDLE event,
      rpy_d3d_test_t *base, rpy_gpu_vector_t *vector, ID3D12Resource *pixel_readback,
      ID3D12RootSignature *dwt_root, ID3D12PipelineState *dwt_pipeline,
      const char *packet_path, const char *frame_path, const char *prefix)
{
   static const uint32_t budgets[6] = { 8, 512, 4096, 32768, 262144, 262144 };
   rpy_d3d_wire_t test;
   rpyrowave_gpu_layout_t layout;
   rpyrowave_gpu_block_t *blocks = NULL;
   rpyrowave_decoder_t *cpu = NULL;
   rpyrowave_image_t expected;
   rpyrowave_sequence_t sequence;
   ID3D12Resource *block_upload = NULL;
   ID3D12Resource *upload = NULL;
   ID3D12Resource *wire_readback = NULL;
   ID3D12Resource *backup = NULL;
   uint8_t *workspace = NULL;
   uint8_t *frame = NULL;
   void *wire = NULL;
   void *pixels = NULL;
   size_t frame_bytes;
   size_t block_bytes;
   D3D12_RANGE range;
   unsigned budget;
   int success = 0;
   memset(&test, 0, sizeof(test));
   test.base = base;
   frame = rpy_gpu_test_file(frame_path, &frame_bytes);
   RPY_D3D_CHECK(frame && !rpyrowave_gpu_layout(&vector->sequence, 262144, &layout));
   block_bytes = layout.block_count * sizeof(*blocks);
   blocks = (rpyrowave_gpu_block_t*)malloc(block_bytes);
   workspace = (uint8_t*)calloc(1, layout.workspace_bytes);
   cpu = rpyrowave_decoder_new(vector->sequence.width, vector->sequence.height,
         vector->sequence.chroma_444, 16 * 1024 * 1024);
   RPY_D3D_CHECK(blocks && workspace && cpu && rpy_d3d_packet_pipeline(device, &test, packet_path));
   RPY_D3D_CHECK(!rpyrowave_gpu_blocks(&layout, blocks, layout.block_count));
   memcpy(workspace + layout.wire_offset, frame, frame_bytes);
   RPY_D3D_CHECK(!rpyrowave_gpu_index_frame(&layout, workspace, layout.workspace_bytes,
            frame_bytes, 0, &test.decode));
   test.blocks = rpy_d3d_buffer(device, block_bytes, D3D12_HEAP_TYPE_DEFAULT,
         D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE);
   test.workspace = rpy_d3d_buffer(device, layout.workspace_bytes, D3D12_HEAP_TYPE_DEFAULT,
         D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   block_upload = rpy_d3d_buffer(device, block_bytes, D3D12_HEAP_TYPE_UPLOAD,
         D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE);
   upload = rpy_d3d_buffer(device, layout.workspace_bytes, D3D12_HEAP_TYPE_UPLOAD,
         D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE);
   wire_readback = rpy_d3d_buffer(device, layout.workspace_bytes, D3D12_HEAP_TYPE_READBACK,
         D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE);
   backup = rpy_d3d_buffer(device, vector->bytes, D3D12_HEAP_TYPE_DEFAULT,
         D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE);
   RPY_D3D_CHECK(test.blocks && test.workspace && block_upload && upload && wire_readback && backup);
   RPY_D3D_CHECK(rpy_d3d_upload(block_upload, blocks, block_bytes)
         && rpy_d3d_upload(upload, workspace, layout.workspace_bytes));
   RPY_D3D_CHECK(SUCCEEDED(ID3D12CommandAllocator_Reset(allocator)));
   RPY_D3D_CHECK(SUCCEEDED(ID3D12GraphicsCommandList_Reset(base->list, allocator, test.pipeline)));
   ID3D12GraphicsCommandList_CopyBufferRegion(base->list, test.blocks, 0, block_upload, 0, block_bytes);
   ID3D12GraphicsCommandList_CopyBufferRegion(base->list, test.workspace, 0, upload, 0, layout.workspace_bytes);
   rpy_d3d_transition(base->list, test.blocks, D3D12_RESOURCE_STATE_COPY_DEST,
         D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   rpy_d3d_transition(base->list, test.workspace, D3D12_RESOURCE_STATE_COPY_DEST,
         D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
   RPY_D3D_CHECK(rpy_d3d_wire_inverse(&test, vector, &layout, &test.decode, dwt_root, dwt_pipeline));
   rpy_d3d_transition(base->list, base->atlas[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
         D3D12_RESOURCE_STATE_COPY_SOURCE);
   ID3D12GraphicsCommandList_CopyBufferRegion(base->list, backup, 0, base->atlas[0], 0, vector->bytes);
   rpy_d3d_transition(base->list, backup, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
   rpy_d3d_transition(base->list, base->atlas[0], D3D12_RESOURCE_STATE_COPY_SOURCE,
         D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   rpy_d3d_wire_copy(&test, vector, &layout, wire_readback, pixel_readback);
   RPY_D3D_CHECK(rpy_d3d_submit(queue, base->list, fence, event, 2));
   range.Begin = 0;
   range.End = vector->bytes;
   RPY_D3D_CHECK(SUCCEEDED(ID3D12Resource_Map(pixel_readback, 0, &range, &pixels)));
   RPY_D3D_CHECK(rpy_gpu_vector_compare(vector, (const float*)pixels));
   range.End = 0;
   ID3D12Resource_Unmap(pixel_readback, 0, &range);
   pixels = NULL;
   sequence = vector->sequence;
   for (budget = 0; budget < 6; budget++)
   {
      uint32_t *control;
      int fixed = budget == 5 ? 128 : -1;
      sequence.sequence = budget;
      RPY_D3D_CHECK(SUCCEEDED(ID3D12CommandAllocator_Reset(allocator)));
      RPY_D3D_CHECK(SUCCEEDED(ID3D12GraphicsCommandList_Reset(base->list, allocator, dwt_pipeline)));
      rpy_d3d_transition(base->list, base->atlas[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_COPY_DEST);
      ID3D12GraphicsCommandList_CopyBufferRegion(base->list, base->atlas[0], 0, backup, 0, vector->bytes);
      rpy_d3d_transition(base->list, base->atlas[0], D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
      ID3D12GraphicsCommandList_SetComputeRootSignature(base->list, dwt_root);
      RPY_D3D_CHECK(!rpyrowave_dwt_record(vector->aligned_width, vector->aligned_height,
               sequence.chroma_444, 0, rpy_d3d_dispatch, base));
      RPY_D3D_CHECK(!rpyrowave_gpu_encode_record(&layout, &sequence, budgets[budget], fixed,
               rpy_d3d_packet_dispatch, &test));
      test.decode.operation = RPYROWAVE_PACKET_UNPACK;
      RPY_D3D_CHECK(rpy_d3d_wire_inverse(&test, vector, &layout, &test.decode, dwt_root, dwt_pipeline));
      rpy_d3d_wire_copy(&test, vector, &layout, wire_readback, pixel_readback);
      RPY_D3D_CHECK(rpy_d3d_submit(queue, base->list, fence, event, budget + 3));
      range.Begin = 0;
      range.End = layout.workspace_bytes;
      RPY_D3D_CHECK(SUCCEEDED(ID3D12Resource_Map(wire_readback, 0, &range, &wire)));
      control = (uint32_t*)wire;
      printf("GPU encoded budget %u: %u bytes, quantizer %u, %u active blocks\n",
            budgets[budget], control[1], control[0], control[2]);
      RPY_D3D_CHECK(!control[3] && control[1] >= 8 && control[1] <= budgets[budget]);
      RPY_D3D_CHECK(!rpyrowave_decode_frame(cpu, (const uint8_t*)wire + layout.wire_offset,
               control[1], 0, &expected));
      if (prefix)
      {
         char *path = (char*)malloc(strlen(prefix) + 40);
         FILE *file;
         int written;
         RPY_D3D_CHECK(path != NULL);
         sprintf(path, "%s.%u%s.pyro", prefix, budgets[budget], fixed >= 0 ? "-fixed" : "");
         file = fopen(path, "wb");
         free(path);
         RPY_D3D_CHECK(file != NULL);
         written = fwrite((const uint8_t*)wire + layout.wire_offset, 1, control[1], file) == control[1];
         written = fclose(file) == 0 && written;
         RPY_D3D_CHECK(written);
      }
      range.End = 0;
      ID3D12Resource_Unmap(wire_readback, 0, &range);
      wire = NULL;
      range.End = vector->bytes;
      RPY_D3D_CHECK(SUCCEEDED(ID3D12Resource_Map(pixel_readback, 0, &range, &pixels)));
      RPY_D3D_CHECK(rpy_gpu_image_compare(vector, (const float*)pixels, &expected));
      range.End = 0;
      ID3D12Resource_Unmap(pixel_readback, 0, &range);
      pixels = NULL;
   }
   for (budget = 0; budget < 2; budget++)
   {
      RPY_D3D_CHECK(rpy_gpu_fault_prepare(&layout, workspace, frame, frame_bytes, budget, &test.decode));
      RPY_D3D_CHECK(rpy_d3d_upload(upload, workspace, layout.workspace_bytes));
      RPY_D3D_CHECK(SUCCEEDED(ID3D12CommandAllocator_Reset(allocator)));
      RPY_D3D_CHECK(SUCCEEDED(ID3D12GraphicsCommandList_Reset(base->list, allocator, test.pipeline)));
      rpy_d3d_transition(base->list, test.workspace, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_DEST);
      ID3D12GraphicsCommandList_CopyBufferRegion(base->list, test.workspace, 0, upload, 0, layout.workspace_bytes);
      rpy_d3d_transition(base->list, test.workspace, D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      RPY_D3D_CHECK(rpy_d3d_wire_decode(&test, &layout, &test.decode));
      rpy_d3d_transition(base->list, test.workspace, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COPY_SOURCE);
      ID3D12GraphicsCommandList_CopyBufferRegion(base->list, wire_readback, 0, test.workspace, 0, layout.workspace_bytes);
      rpy_d3d_transition(base->list, test.workspace, D3D12_RESOURCE_STATE_COPY_SOURCE,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
      RPY_D3D_CHECK(rpy_d3d_submit(queue, base->list, fence, event, budget + 9));
      range.End = layout.workspace_bytes;
      RPY_D3D_CHECK(SUCCEEDED(ID3D12Resource_Map(wire_readback, 0, &range, &wire)));
      RPY_D3D_CHECK(((const uint32_t*)wire)[3] != 0);
      range.End = 0;
      ID3D12Resource_Unmap(wire_readback, 0, &range);
      wire = NULL;
   }
   puts("GPU magnitude/sign truncation guards: PASS");
   success = 1;
end:
   if (wire) ID3D12Resource_Unmap(wire_readback, 0, NULL);
   if (pixels) ID3D12Resource_Unmap(pixel_readback, 0, NULL);
   RPY_D3D_RELEASE(test.blocks);
   RPY_D3D_RELEASE(test.workspace);
   RPY_D3D_RELEASE(test.pipeline);
   RPY_D3D_RELEASE(test.root);
   RPY_D3D_RELEASE(block_upload);
   RPY_D3D_RELEASE(upload);
   RPY_D3D_RELEASE(wire_readback);
   RPY_D3D_RELEASE(backup);
   rpyrowave_decoder_free(cpu);
   free(blocks);
   free(workspace);
   free(frame);
   puts(success ? "D3D12 GPU PyroWave packets: PASS" : "D3D12 GPU PyroWave packets: FAIL");
   return success;
}
#endif
