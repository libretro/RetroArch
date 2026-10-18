/* Sample shipping menu and packed 10-bit frame SRVs. */
#include <stdio.h>
#ifdef _WIN32
#define COBJMACROS
#define WIDL_C_INLINE_WRAPPERS
#include <windows.h>
#include <d3d12.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <gfx/scaler/pixconv.h>
#define W 257
#define H 256
#define COUNT (W * H)
#define DXGI_FORMAT_EX_A4R4G4B4_UNORM 1000
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL mode %u line %u: %s\n",mode,(unsigned)__LINE__,#x); goto done; } } while (0)
#define DROP(x) do { if (x) { (x)->lpVtbl->Release(x); (x)=NULL; } } while (0)
typedef int bool;
#define true 1
#define false 0
typedef ID3D12Device *D3D12Device;
typedef struct
{
   ID3D12Resource *handle;
   D3D12_RESOURCE_DESC desc;
   D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
   bool dirty;
   UINT srv_mapping;
   D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor[1];
} d3d12_texture_t;
#include "driver.inc"
#include "shader.h"

static ID3D12Resource *buffer(ID3D12Device *device, UINT64 size,
      D3D12_HEAP_TYPE type, D3D12_RESOURCE_STATES state,
      D3D12_RESOURCE_FLAGS flags)
{
   D3D12_HEAP_PROPERTIES heap;
   D3D12_RESOURCE_DESC desc;
   ID3D12Resource *resource = NULL;
   memset(&heap,0,sizeof(heap));
   memset(&desc,0,sizeof(desc));
   heap.Type = type;
   heap.CreationNodeMask = heap.VisibleNodeMask = 1;
   desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
   desc.Width = size;
   desc.Height = desc.DepthOrArraySize = desc.MipLevels = 1;
   desc.SampleDesc.Count = 1;
   desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
   desc.Flags = flags;
   if (FAILED(ID3D12Device_CreateCommittedResource(device,&heap,
            D3D12_HEAP_FLAG_NONE,&desc,state,NULL,&IID_ID3D12Resource,
            (void**)&resource)))
      return NULL;
   return resource;
}

static int run_case(ID3D12Device *device, ID3D12RootSignature *root,
      ID3D12PipelineState *pipeline, unsigned mode)
{
   ID3D12CommandQueue *queue = NULL;
   ID3D12CommandAllocator *allocator = NULL;
   ID3D12GraphicsCommandList *list = NULL;
   ID3D12Fence *fence = NULL;
   ID3D12DescriptorHeap *heap = NULL;
   ID3D12Resource *upload = NULL, *output = NULL, *readback = NULL;
   d3d12_texture_t texture;
   D3D12_COMMAND_QUEUE_DESC qdesc;
   D3D12_DESCRIPTOR_HEAP_DESC hdesc;
   D3D12_HEAP_PROPERTIES props;
   D3D12_SHADER_RESOURCE_VIEW_DESC srv;
   D3D12_UNORDERED_ACCESS_VIEW_DESC uav;
   D3D12_FEATURE_DATA_FORMAT_SUPPORT support;
   D3D12_TEXTURE_COPY_LOCATION src, dst;
   D3D12_RESOURCE_BARRIER barrier;
   D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
   D3D12_CPU_DESCRIPTOR_HANDLE cpu;
   D3D12_GPU_DESCRIPTOR_HANDLE gpu;
   D3D12_RANGE range;
   ID3D12CommandList *commands;
   HANDLE event = NULL;
   UINT64 bytes = 0;
   const UINT64 output_bytes = (UINT64)COUNT * 8 * sizeof(float);
   uint16_t *pixels = NULL;
   uint32_t *pixels32 = NULL;
   uint8_t *mapped = NULL;
   const float *sampled = NULL;
   unsigned x, y, i, channel, lane;
   HRESULT support_result;
   int result = 1;
   memset(&texture,0,sizeof(texture));
   memset(&support,0,sizeof(support));
   texture.desc.Format = mode >= 4 ? DXGI_FORMAT_R10G10B10A2_UNORM
      : (mode < 2 ? DXGI_FORMAT_B4G4R4A4_UNORM : DXGI_FORMAT_B8G8R8A8_UNORM);
   support.Format = texture.desc.Format;
   support_result = ID3D12Device_CheckFeatureSupport(device,
         D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support));
   CHECK(SUCCEEDED(support_result) || (mode < 2 && support_result == E_INVALIDARG));
   if (FAILED(support_result) || !(support.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D)
         || !(support.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE))
   {
      CHECK(mode < 2 && !getenv("REQUIRE_NATIVE16"));
      printf("SKIP: mode %u texture format unsupported\n",mode);
      return 0;
   }
   memset(&qdesc,0,sizeof(qdesc));
   CHECK(SUCCEEDED(ID3D12Device_CreateCommandQueue(device,&qdesc,
               &IID_ID3D12CommandQueue,(void**)&queue)));
   CHECK(SUCCEEDED(ID3D12Device_CreateCommandAllocator(device,
               D3D12_COMMAND_LIST_TYPE_DIRECT,&IID_ID3D12CommandAllocator,
               (void**)&allocator)));
   CHECK(SUCCEEDED(ID3D12Device_CreateCommandList(device,0,
               D3D12_COMMAND_LIST_TYPE_DIRECT,allocator,pipeline,
               &IID_ID3D12GraphicsCommandList,(void**)&list)));
   CHECK(SUCCEEDED(ID3D12Device_CreateFence(device,0,D3D12_FENCE_FLAG_NONE,
               &IID_ID3D12Fence,(void**)&fence)));
   memset(&hdesc,0,sizeof(hdesc));
   hdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
   hdesc.NumDescriptors = 2;
   hdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
   CHECK(SUCCEEDED(ID3D12Device_CreateDescriptorHeap(device,&hdesc,
               &IID_ID3D12DescriptorHeap,(void**)&heap)));
   cpu = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(heap);
   gpu = ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(heap);
   texture.cpu_descriptor[0] = cpu;
   texture.desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   texture.desc.Width = W;
   texture.desc.Height = H;
   texture.desc.DepthOrArraySize = texture.desc.MipLevels = 1;
   texture.desc.SampleDesc.Count = 1;
   memset(&props,0,sizeof(props));
   props.Type = D3D12_HEAP_TYPE_DEFAULT;
   props.CreationNodeMask = props.VisibleNodeMask = 1;
   CHECK(SUCCEEDED(ID3D12Device_CreateCommittedResource(device,&props,
               D3D12_HEAP_FLAG_NONE,&texture.desc,D3D12_RESOURCE_STATE_COPY_DEST,
               NULL,&IID_ID3D12Resource,(void**)&texture.handle)));
   memset(&srv,0,sizeof(srv));
   srv.Format = texture.desc.Format;
   srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
   srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
   srv.Texture2D.MipLevels = 1;
   ID3D12Device_CreateShaderResourceView(device,texture.handle,&srv,cpu);
   if (mode < 4)
      d3d12_menu_mapping(device,&texture,mode == 3);
   else
   {
      d3d12_frame_mapping(device,&texture,mode == 7);
      d3d12_frame_mapping(device,&texture,mode != 6 && mode != 7);
   }
   CHECK((texture.srv_mapping != 0) == (mode < 2 || mode == 4 || mode == 5 || mode == 8));
   ID3D12Device_GetCopyableFootprints(device,&texture.desc,0,1,0,
         &footprint,NULL,NULL,&bytes);
   CHECK(footprint.Footprint.RowPitch > W * (mode < 2 ? 2u : 4u));
   upload = buffer(device,bytes,D3D12_HEAP_TYPE_UPLOAD,
         D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
   output = buffer(device,output_bytes,D3D12_HEAP_TYPE_DEFAULT,
         D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
   readback = buffer(device,output_bytes,D3D12_HEAP_TYPE_READBACK,
         D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_FLAG_NONE);
   CHECK(upload && output && readback);
   pixels = (uint16_t*)malloc(COUNT * sizeof(*pixels));
   pixels32 = (uint32_t*)malloc(COUNT * sizeof(*pixels32));
   CHECK(pixels && pixels32);
   for (i=0;i<COUNT;i++)
   {
      unsigned v = i & 65535;
      pixels[i] = (uint16_t)v;
      if (mode >= 4)
      {
         unsigned r = (i * 37 + 3) & 1023;
         unsigned g = (i >> 2) & 1023;
         unsigned b = ((i >> 6) * 19 + 7) & 1023;
         unsigned a = (i >> 4) & 3;
         pixels32[i] = mode == 6 || mode == 7
            ? r | (g << 10) | (b << 20) | (a << 30)
            : (r << 20) | (g << 10) | b | (a << 30);
         continue;
      }
      pixels32[i] = (((v >> 12) & 15) * 17)
         | (((v >> 8) & 15) * 17 << 8)
         | (((v >> 4) & 15) * 17 << 16)
         | ((v & 15) * 17 << 24);
   }
   range.Begin = range.End = 0;
   CHECK(SUCCEEDED(ID3D12Resource_Map(upload,0,&range,(void**)&mapped)));
   memset(mapped,0xcd,(size_t)bytes);
   if (mode == 5)
   {
      ID3D12Resource_Unmap(upload,0,NULL);
      mapped = NULL;
      texture.layout = footprint;
      d3d12_update_texture_into(W,H,0,DXGI_FORMAT_R10G10B10A2_UNORM,
            pixels32,&texture,upload);
      CHECK(texture.dirty);
   }
   else if (mode >= 4)
   {
      for (y=0;y<H;y++)
         memcpy(mapped+y*footprint.Footprint.RowPitch,pixels32+y*W,W*4);
   }
   else if (!mode)
   {
      for (y=0;y<H;y++)
         for (x=0;x<W;x++)
            ((uint16_t*)(mapped+y*footprint.Footprint.RowPitch))[x] = pixels[y*W+x];
   }
   else
      dxgi_copy(W,H,mode == 3 ? DXGI_FORMAT_B8G8R8A8_UNORM :
            (mode == 2 ? (DXGI_FORMAT)DXGI_FORMAT_EX_A4R4G4B4_UNORM : DXGI_FORMAT_B4G4R4A4_UNORM),
            W * (mode == 3 ? 4 : 2), mode == 3 ? (const void*)pixels32 : (const void*)pixels,
            texture.desc.Format,footprint.Footprint.RowPitch,mapped);
   if (mapped) ID3D12Resource_Unmap(upload,0,NULL);
   mapped = NULL;
   memset(&uav,0,sizeof(uav));
   uav.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
   uav.Buffer.NumElements = COUNT * 2;
   uav.Buffer.StructureByteStride = 4 * sizeof(float);
   cpu.ptr += ID3D12Device_GetDescriptorHandleIncrementSize(device,
         D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
   ID3D12Device_CreateUnorderedAccessView(device,output,NULL,&uav,cpu);
   memset(&src,0,sizeof(src));
   memset(&dst,0,sizeof(dst));
   src.pResource = upload;
   src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
   src.PlacedFootprint = footprint;
   dst.pResource = texture.handle;
   dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
   ID3D12GraphicsCommandList_CopyTextureRegion(list,&dst,0,0,0,&src,NULL);
   memset(&barrier,0,sizeof(barrier));
   barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
   barrier.Transition.pResource = texture.handle;
   barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
   barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
   ID3D12GraphicsCommandList_ResourceBarrier(list,1,&barrier);
   ID3D12GraphicsCommandList_SetDescriptorHeaps(list,1,&heap);
   ID3D12GraphicsCommandList_SetComputeRootSignature(list,root);
   ID3D12GraphicsCommandList_SetComputeRootDescriptorTable(list,0,gpu);
   ID3D12GraphicsCommandList_Dispatch(list,(W+7)/8,(H+7)/8,1);
   barrier.Transition.pResource = output;
   barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
   barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
   ID3D12GraphicsCommandList_ResourceBarrier(list,1,&barrier);
   ID3D12GraphicsCommandList_CopyBufferRegion(list,readback,0,output,0,output_bytes);
   CHECK(SUCCEEDED(ID3D12GraphicsCommandList_Close(list)));
   commands = (ID3D12CommandList*)list;
   ID3D12CommandQueue_ExecuteCommandLists(queue,1,&commands);
   CHECK(SUCCEEDED(ID3D12CommandQueue_Signal(queue,fence,1)));
   event = CreateEvent(NULL,FALSE,FALSE,NULL);
   CHECK(event);
   CHECK(SUCCEEDED(ID3D12Fence_SetEventOnCompletion(fence,1,event)));
   CHECK(WaitForSingleObject(event,30000) == WAIT_OBJECT_0);
   range.Begin = 0;
   range.End = (SIZE_T)output_bytes;
   CHECK(SUCCEEDED(ID3D12Resource_Map(readback,0,&range,(void**)&sampled)));
   for (i=0;i<COUNT;i++)
   {
      unsigned v = i & 65535;
      unsigned expected[4];
      expected[0] = (v >> 4) & 15;
      expected[1] = (v >> 8) & 15;
      expected[2] = (v >> 12) & 15;
      expected[3] = v & 15;
      if (mode >= 4)
      {
         expected[0] = (i * 37 + 3) & 1023;
         expected[1] = (i >> 2) & 1023;
         expected[2] = ((i >> 6) * 19 + 7) & 1023;
         expected[3] = mode == 6 || mode == 7 ? ((i >> 4) & 3) * 341 : 1023;
      }
      for (lane=0;lane<2;lane++)
         for (channel=0;channel<4;channel++)
         {
            float actual = sampled[i*8+lane*4+channel];
            float wanted = expected[channel] / (mode >= 4 ? 1023.0f : 15.0f);
            if (!(fabs(actual-wanted) < (mode >= 4 ? 0.0001 : 0.001)))
            {
               fprintf(stderr,"FAIL mode %u pattern %04x lane %u channel %u: %.6f != %.6f\n",
                     mode,v,lane,channel,actual,wanted);
               goto done;
            }
         }
   }
   printf("PASS: mode %u, %s, padded rows, Load and linear SampleLevel\n",
         mode, mode >= 4 ? "all 10-bit channel values and X/alpha codes"
         : "all 65536 BGRA4444 values");
   result = 0;
done:
   if (sampled) ID3D12Resource_Unmap(readback,0,NULL);
   if (mapped) ID3D12Resource_Unmap(upload,0,NULL);
   if (event) CloseHandle(event);
   free(pixels); free(pixels32);
   DROP(readback); DROP(output); DROP(upload); DROP(texture.handle);
   DROP(heap); DROP(fence); DROP(list); DROP(allocator); DROP(queue);
   return result;
}

int main(void)
{
   ID3D12Device *device = NULL;
   ID3D12RootSignature *root = NULL;
   ID3D12PipelineState *pipeline = NULL;
   ID3DBlob *serialized = NULL, *errors = NULL;
   D3D12_DESCRIPTOR_RANGE ranges[2];
   D3D12_ROOT_PARAMETER param;
   D3D12_STATIC_SAMPLER_DESC sampler;
   D3D12_ROOT_SIGNATURE_DESC desc;
   D3D12_COMPUTE_PIPELINE_STATE_DESC pso;
   unsigned mode = 0;
   int result = 1;
   HRESULT hr = D3D12CreateDevice(NULL,D3D_FEATURE_LEVEL_11_0,
         &IID_ID3D12Device,(void**)&device);
   if (FAILED(hr))
   {
      if (getenv("REQUIRE_NATIVE16")) return 1;
      puts("SKIP: D3D12 device unavailable");
      return 0;
   }
   memset(ranges,0,sizeof(ranges));
   ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
   ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
   ranges[0].NumDescriptors = ranges[1].NumDescriptors = 1;
   ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
   memset(&param,0,sizeof(param));
   param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
   param.DescriptorTable.NumDescriptorRanges = 2;
   param.DescriptorTable.pDescriptorRanges = ranges;
   memset(&sampler,0,sizeof(sampler));
   sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
   sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
   sampler.MaxAnisotropy = 1;
   sampler.MaxLOD = D3D12_FLOAT32_MAX;
   sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
   memset(&desc,0,sizeof(desc));
   desc.NumParameters = desc.NumStaticSamplers = 1;
   desc.pParameters = &param;
   desc.pStaticSamplers = &sampler;
   CHECK(SUCCEEDED(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,
               &serialized,&errors)));
   CHECK(SUCCEEDED(ID3D12Device_CreateRootSignature(device,0,
               ID3D10Blob_GetBufferPointer(serialized),ID3D10Blob_GetBufferSize(serialized),
               &IID_ID3D12RootSignature,(void**)&root)));
   memset(&pso,0,sizeof(pso));
   pso.pRootSignature = root;
   pso.CS.pShaderBytecode = menu_mapping_cs;
   pso.CS.BytecodeLength = sizeof(menu_mapping_cs);
   CHECK(SUCCEEDED(ID3D12Device_CreateComputePipelineState(device,&pso,
               &IID_ID3D12PipelineState,(void**)&pipeline)));
   result = 0;
   for (mode=0;mode<9;mode++) result |= run_case(device,root,pipeline,mode);
done:
   DROP(errors); DROP(serialized); DROP(pipeline); DROP(root); DROP(device);
   return result;
}
#else
int main(void)
{
   puts("SKIP: D3D12 runtime requires Windows or mingw + Wine");
   return 0;
}
#endif
