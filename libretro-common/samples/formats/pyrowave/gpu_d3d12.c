/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include "gpu_d3d12_context.h"
#if defined(_WIN32)
#include "gpu_d3d12_codec.h"
int main(int argc, char **argv)
{
   IDXGIFactory1 *factory = NULL;
   IDXGIAdapter1 *adapter = NULL;
   ID3D12Device *device = NULL;
   ID3D12Debug *debug = NULL;
   ID3D12InfoQueue *info = NULL;
   ID3D12RootSignature *root = NULL;
   ID3D12PipelineState *pipeline = NULL;
   ID3D12CommandQueue *queue = NULL;
   ID3D12CommandAllocator *allocator = NULL;
   ID3D12Fence *fence = NULL;
   ID3D12Resource *upload = NULL;
   ID3D12Resource *readback = NULL;
   ID3DBlob *serialized = NULL;
   ID3DBlob *errors = NULL;
   rpy_d3d_test_t test;
   rpy_gpu_vector_t *vector = NULL;
   D3D12_ROOT_PARAMETER parameters[3];
   D3D12_ROOT_SIGNATURE_DESC root_desc;
   D3D12_COMPUTE_PIPELINE_STATE_DESC pipeline_desc;
   D3D12_COMMAND_QUEUE_DESC queue_desc;
   D3D12_RANGE range;
   uint8_t *shader = NULL;
   void *mapped = NULL;
   size_t shader_size;
   size_t i;
   unsigned index;
   HANDLE event = NULL;
   int success = 0;
   double maximum = 0.0;
   memset(&test, 0, sizeof(test));
   if (argc != 4 && argc != 6)
   {
      fprintf(stderr, "Usage: gpu_d3d12 dwt.dxbc frame.pyro reference.u16 [packet.dxbc output-prefix]\n");
      return EXIT_FAILURE;
   }
   vector = rpy_gpu_vector_load(argv[2], argv[3]);
   shader = rpy_gpu_test_file(argv[1], &shader_size);
   RPY_D3D_CHECK(vector && shader);
   if (SUCCEEDED(D3D12GetDebugInterface(&IID_ID3D12Debug, (void**)&debug)))
   {
      ID3D12Debug_EnableDebugLayer(debug);
      puts("D3D12 debug layer: enabled");
   }
   else
      puts("D3D12 debug layer: unavailable");
   RPY_D3D_CHECK(SUCCEEDED(CreateDXGIFactory1(&IID_IDXGIFactory1, (void**)&factory)));
   for (index = 0; IDXGIFactory1_EnumAdapters1(factory, index, &adapter) == S_OK; index++)
   {
      DXGI_ADAPTER_DESC1 desc;
      if (SUCCEEDED(IDXGIAdapter1_GetDesc1(adapter, &desc))
            && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            && SUCCEEDED(D3D12CreateDevice((IUnknown*)adapter, D3D_FEATURE_LEVEL_11_0,
                  &IID_ID3D12Device, (void**)&device)))
      {
         char name[256];
         WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name, sizeof(name), NULL, NULL);
         printf("D3D12 hardware: %s\n", name);
         break;
      }
      RPY_D3D_RELEASE(adapter);
   }
   RPY_D3D_CHECK(device != NULL);
   if (debug)
      RPY_D3D_CHECK(SUCCEEDED(ID3D12Device_QueryInterface(device, &IID_ID3D12InfoQueue, (void**)&info)));
   for (index = 0; index < 2; index++)
   {
      parameters[index].ParameterType = index ? D3D12_ROOT_PARAMETER_TYPE_UAV
         : D3D12_ROOT_PARAMETER_TYPE_SRV;
      parameters[index].Descriptor.ShaderRegister = 0;
      parameters[index].Descriptor.RegisterSpace = 0;
      parameters[index].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
   }
   parameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
   parameters[2].Constants.ShaderRegister = 0;
   parameters[2].Constants.RegisterSpace = 0;
   parameters[2].Constants.Num32BitValues = 8;
   parameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
   root_desc.NumParameters = 3;
   root_desc.pParameters = parameters;
   root_desc.NumStaticSamplers = 0;
   root_desc.pStaticSamplers = NULL;
   root_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
   RPY_D3D_CHECK(SUCCEEDED(D3D12SerializeRootSignature(&root_desc,
               D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors)));
   RPY_D3D_CHECK(SUCCEEDED(ID3D12Device_CreateRootSignature(device, 0,
               ID3D10Blob_GetBufferPointer(serialized), ID3D10Blob_GetBufferSize(serialized),
               &IID_ID3D12RootSignature, (void**)&root)));
   pipeline_desc.pRootSignature = root;
   pipeline_desc.CS.pShaderBytecode = shader;
   pipeline_desc.CS.BytecodeLength = shader_size;
   pipeline_desc.NodeMask = 0;
   pipeline_desc.CachedPSO.pCachedBlob = NULL;
   pipeline_desc.CachedPSO.CachedBlobSizeInBytes = 0;
   pipeline_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
   RPY_D3D_CHECK(SUCCEEDED(ID3D12Device_CreateComputePipelineState(device,
               &pipeline_desc, &IID_ID3D12PipelineState, (void**)&pipeline)));
   queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
   queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
   queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
   queue_desc.NodeMask = 0;
   RPY_D3D_CHECK(SUCCEEDED(ID3D12Device_CreateCommandQueue(device, &queue_desc,
               &IID_ID3D12CommandQueue, (void**)&queue)));
   RPY_D3D_CHECK(SUCCEEDED(ID3D12Device_CreateCommandAllocator(device,
               D3D12_COMMAND_LIST_TYPE_COMPUTE, &IID_ID3D12CommandAllocator, (void**)&allocator)));
   RPY_D3D_CHECK(SUCCEEDED(ID3D12Device_CreateCommandList(device, 0,
               D3D12_COMMAND_LIST_TYPE_COMPUTE, allocator, pipeline,
               &IID_ID3D12GraphicsCommandList, (void**)&test.list)));
   RPY_D3D_CHECK(SUCCEEDED(ID3D12Device_CreateFence(device, 0, D3D12_FENCE_FLAG_NONE,
               &IID_ID3D12Fence, (void**)&fence)));
   event = CreateEventA(NULL, FALSE, FALSE, NULL);
   RPY_D3D_CHECK(event != NULL);
   test.atlas[0] = rpy_d3d_buffer(device, vector->bytes, D3D12_HEAP_TYPE_DEFAULT,
         D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   test.atlas[1] = rpy_d3d_buffer(device, vector->bytes, D3D12_HEAP_TYPE_DEFAULT,
         D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   upload = rpy_d3d_buffer(device, vector->bytes, D3D12_HEAP_TYPE_UPLOAD,
         D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE);
   readback = rpy_d3d_buffer(device, vector->bytes, D3D12_HEAP_TYPE_READBACK,
         D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE);
   RPY_D3D_CHECK(test.atlas[0] && test.atlas[1] && upload && readback);
   range.Begin = range.End = 0;
   RPY_D3D_CHECK(SUCCEEDED(ID3D12Resource_Map(upload, 0, &range, &mapped)));
   memcpy(mapped, vector->atlas, vector->bytes);
   range.End = vector->bytes;
   ID3D12Resource_Unmap(upload, 0, &range);
   mapped = NULL;
   ID3D12GraphicsCommandList_CopyBufferRegion(test.list, test.atlas[0], 0, upload, 0, vector->bytes);
   rpy_d3d_transition(test.list, test.atlas[0], D3D12_RESOURCE_STATE_COPY_DEST,
         D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   ID3D12GraphicsCommandList_SetComputeRootSignature(test.list, root);
   RPY_D3D_CHECK(!rpyrowave_dwt_record(vector->aligned_width, vector->aligned_height,
            vector->sequence.chroma_444, 1, rpy_d3d_dispatch, &test));
   rpy_d3d_transition(test.list, test.atlas[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
         D3D12_RESOURCE_STATE_COPY_SOURCE);
   ID3D12GraphicsCommandList_CopyBufferRegion(test.list, readback, 0, test.atlas[0], 0, vector->bytes);
   rpy_d3d_transition(test.list, test.atlas[0], D3D12_RESOURCE_STATE_COPY_SOURCE,
         D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
   RPY_D3D_CHECK(rpy_d3d_submit(queue, test.list, fence, event, 1));
   range.Begin = 0;
   range.End = vector->bytes;
   RPY_D3D_CHECK(SUCCEEDED(ID3D12Resource_Map(readback, 0, &range, &mapped)));
   RPY_D3D_CHECK(rpy_gpu_vector_compare(vector, (const float*)mapped));
   memcpy(vector->atlas, mapped, vector->bytes);
   range.End = 0;
   ID3D12Resource_Unmap(readback, 0, &range);
   mapped = NULL;
   if (argc == 6)
   {
      RPY_D3D_CHECK(rpy_d3d_codec_run(device, queue, allocator, fence, event, &test,
               vector, readback, root, pipeline, argv[4], argv[2], argv[5]));
      RPY_D3D_CHECK(rpy_d3d_messages(info));
      success = 1;
      goto end;
   }
   RPY_D3D_CHECK(SUCCEEDED(ID3D12CommandAllocator_Reset(allocator)));
   RPY_D3D_CHECK(SUCCEEDED(ID3D12GraphicsCommandList_Reset(test.list, allocator, pipeline)));
   ID3D12GraphicsCommandList_SetComputeRootSignature(test.list, root);
   for (index = 0; index < 2; index++)
      RPY_D3D_CHECK(!rpyrowave_dwt_record(vector->aligned_width, vector->aligned_height,
               vector->sequence.chroma_444, index, rpy_d3d_dispatch, &test));
   rpy_d3d_transition(test.list, test.atlas[0], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
         D3D12_RESOURCE_STATE_COPY_SOURCE);
   ID3D12GraphicsCommandList_CopyBufferRegion(test.list, readback, 0, test.atlas[0], 0, vector->bytes);
   RPY_D3D_CHECK(rpy_d3d_submit(queue, test.list, fence, event, 2));
   range.End = vector->bytes;
   RPY_D3D_CHECK(SUCCEEDED(ID3D12Resource_Map(readback, 0, &range, &mapped)));
   for (i = 0; i < vector->bytes / sizeof(float); i++)
   {
      double error = fabs((double)vector->atlas[i] - ((const float*)mapped)[i]);
      RPY_D3D_CHECK(error <= 0.0001);
      if (error > maximum)
         maximum = error;
   }
   range.End = 0;
   ID3D12Resource_Unmap(readback, 0, &range);
   mapped = NULL;
   RPY_D3D_CHECK(SUCCEEDED(ID3D12Device_GetDeviceRemovedReason(device)));
   RPY_D3D_CHECK(rpy_d3d_messages(info));
   printf("GPU forward/inverse round trip: maximum error %.9f\n", maximum);
   success = 1;
end:
   if (mapped && readback)
      ID3D12Resource_Unmap(readback, 0, NULL);
   if (event)
      CloseHandle(event);
   RPY_D3D_RELEASE(readback);
   RPY_D3D_RELEASE(upload);
   RPY_D3D_RELEASE(test.atlas[0]);
   RPY_D3D_RELEASE(test.atlas[1]);
   RPY_D3D_RELEASE(fence);
   RPY_D3D_RELEASE(test.list);
   RPY_D3D_RELEASE(allocator);
   RPY_D3D_RELEASE(queue);
   RPY_D3D_RELEASE(pipeline);
   RPY_D3D_RELEASE(root);
   RPY_D3D_RELEASE(serialized);
   RPY_D3D_RELEASE(errors);
   RPY_D3D_RELEASE(info);
   RPY_D3D_RELEASE(device);
   RPY_D3D_RELEASE(adapter);
   RPY_D3D_RELEASE(factory);
   RPY_D3D_RELEASE(debug);
   free(shader);
   rpy_gpu_vector_free(vector);
   puts(success ? "D3D12 compute DWT: PASS" : "D3D12 compute DWT: FAIL");
   return success ? EXIT_SUCCESS : EXIT_FAILURE;
}
#else
int main(void)
{
   puts("D3D12 compute DWT: unavailable on this platform");
   return 77;
}
#endif
