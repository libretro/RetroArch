/* Production D3D12 imports on a real queue. Blocking waits/readback belong
 * to this oracle only, never to the import function under test. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <boolean.h>
#include <formats/image.h>
#include <formats/image_yuv_blit.h>
#include "hdr_reference.h"
#include "fp16_reference.h"
#include "../../../gfx/common/video_import_color.h"
#include "shaders.inc"
#ifdef _WIN32
#define COBJMACROS
#define WIDL_C_INLINE_WRAPPERS
#include <windows.h>
#include <initguid.h>
#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include "gfx/video_defines.h"
#include "frame.inc"
#define D3D12_IMPORT_TEXTURES 8
#define D3D12_IMPORT_FENCES 8
#define D3D12_IMPORT_LISTS 4
#define uuidof(type) (&IID_##type)
#define CS_ROOT_ID_TEXTURE_T 0
#define CS_ROOT_ID_UAV_T 1
#define CS_ROOT_ID_CONSTANTS 2
#include "types12.inc"
typedef ID3D12GraphicsCommandList *D3D12GraphicsCommandList;
typedef struct
{
   ID3D12Resource *handle;
   D3D12_RESOURCE_DESC desc;
   d3d12_import_t *import;
} d3d12_texture_t;
typedef struct
{
   ID3D12Device *device;
   struct { ID3D12CommandQueue *handle; UINT64 fenceValue; } queue;
   struct
   {
      struct { ID3D12DescriptorHeap *handle; unsigned stride; } srv_heap;
      ID3D12RootSignature *cs_rootSignature;
   } desc;
   ID3D12PipelineState *nv12_pipe;
   ID3D12Fence *import_done;
   ID3D12CommandAllocator *import_alloc[D3D12_IMPORT_LISTS];
   ID3D12GraphicsCommandList *import_cmd[D3D12_IMPORT_LISTS];
   UINT64 import_fence[D3D12_IMPORT_LISTS];
   UINT64 import_value;
   unsigned import_next, import_formats;
   bool video_decode;
   bool movie_importing;
} d3d12_video_t;
enum video_texture_update
{
   VIDEO_TEXTURE_UPDATE_REFUSED,
   VIDEO_TEXTURE_UPDATE_DONE,
   VIDEO_TEXTURE_UPDATE_DROPPED
};
static int failures;
static unsigned hdr_mode;
static unsigned output_fp16;
static unsigned abandoned;
static void unexpected_abandon(void *owner, uint64_t done, uint64_t value)
{
   (void)owner; (void)done; (void)value; abandoned++;
}
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "D3D12 FAIL %d: %s\n", __LINE__, #x); failures++; goto end; } } while (0)
#define DROP(p) do { if (p) { IUnknown_Release((IUnknown*)(p)); p = NULL; } } while (0)

static void d3d12_resource_get_desc(ID3D12Resource *r, D3D12_RESOURCE_DESC *d)
{
   D3D12_RESOURCE_DESC *(STDMETHODCALLTYPE *get)(ID3D12Resource*, D3D12_RESOURCE_DESC*);
   get = (D3D12_RESOURCE_DESC *(STDMETHODCALLTYPE *)(ID3D12Resource*,
         D3D12_RESOURCE_DESC*))(void (*)(void))r->lpVtbl->GetDesc;
   get(r, d);
}
static void transition(ID3D12GraphicsCommandList *cmd, ID3D12Resource *r,
      D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
   D3D12_RESOURCE_BARRIER bar;
   memset(&bar, 0, sizeof(bar));
   bar.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
   bar.Transition.pResource = r;
   bar.Transition.StateBefore = before;
   bar.Transition.StateAfter = after;
   bar.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
   ID3D12GraphicsCommandList_ResourceBarrier(cmd, 1, &bar);
}
#define D3D12_RESOURCE_TRANSITION transition
/* The fixture supplies output/descriptors and retains replaced inputs. */
static d3d12_texture_t *d3d12_import_new(d3d12_video_t *v, unsigned w,
      unsigned h, enum texture_filter_type filter, DXGI_FORMAT output_format)
{
   (void)v; (void)w; (void)h; (void)filter; (void)output_format; failures++; return NULL;
}
static void d3d12_release_texture(d3d12_video_t *v, d3d12_texture_t *t)
{
   (void)v; (void)t; failures++;
}
static IUnknown *retired[16];
static unsigned num_retired;
static void d3d12_retire(d3d12_video_t *v, void *p, IUnknown *obj, UINT64 fence)
{
   (void)v; (void)p; (void)fence;
   if (num_retired < 16) retired[num_retired++] = obj;
   else { failures++; IUnknown_Release(obj); }
}
#define Release(p) do { if (p) IUnknown_Release((IUnknown*)(p)); } while (0)
#include "import12.inc"
#undef Release

static bool wait_for(ID3D12Fence *f, UINT64 value)
{
   HANDLE event = CreateEventA(NULL, FALSE, FALSE, NULL);
   bool ok;
   if (!event) return false;
   ok = SUCCEEDED(ID3D12Fence_SetEventOnCompletion(f, value, event))
      && WaitForSingleObject(event, 10000) == WAIT_OBJECT_0;
   CloseHandle(event);
   return ok;
}

static bool pipeline(d3d12_video_t *v)
{
   D3D12_DESCRIPTOR_RANGE ranges[2];
   D3D12_ROOT_PARAMETER roots[3];
   D3D12_ROOT_SIGNATURE_DESC desc;
   D3D12_COMPUTE_PIPELINE_STATE_DESC pipe;
   ID3DBlob *signature = NULL, *code = NULL, *errors = NULL;
   bool ok = false;
   memset(ranges, 0, sizeof(ranges));
   ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
   ranges[0].NumDescriptors = 2;
   ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
   ranges[1].NumDescriptors = 1;
   memset(roots, 0, sizeof(roots));
   roots[0].ParameterType = roots[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
   roots[0].DescriptorTable.NumDescriptorRanges = roots[1].DescriptorTable.NumDescriptorRanges = 1;
   roots[0].DescriptorTable.pDescriptorRanges = &ranges[0];
   roots[1].DescriptorTable.pDescriptorRanges = &ranges[1];
   roots[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
   roots[2].Constants.Num32BitValues = 20;
   memset(&desc, 0, sizeof(desc));
   desc.NumParameters = 3;
   desc.pParameters = roots;
   if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
         &signature, &errors))) goto end;
   if (FAILED(ID3D12Device_CreateRootSignature(v->device, 0,
         ID3D10Blob_GetBufferPointer(signature), ID3D10Blob_GetBufferSize(signature),
         &IID_ID3D12RootSignature, (void**)&v->desc.cs_rootSignature))) goto end;
   DROP(errors);
   if (FAILED(D3DCompile(d3d12_nv12_cs_src, strlen(d3d12_nv12_cs_src),
         "production", NULL, NULL, "main", "cs_5_0", 0, 0, &code, &errors))) goto end;
   memset(&pipe, 0, sizeof(pipe));
   pipe.pRootSignature = v->desc.cs_rootSignature;
   pipe.CS.pShaderBytecode = ID3D10Blob_GetBufferPointer(code);
   pipe.CS.BytecodeLength = ID3D10Blob_GetBufferSize(code);
   ok = SUCCEEDED(ID3D12Device_CreateComputePipelineState(v->device, &pipe,
         &IID_ID3D12PipelineState, (void**)&v->nv12_pipe));
end:
   if (!ok && errors) fprintf(stderr, "%s\n", (char*)ID3D10Blob_GetBufferPointer(errors));
   DROP(signature); DROP(code); DROP(errors);
   return ok;
}

static unsigned array_case;
static void color(d3d12_video_t *v, ID3D11Device5 *producer,
      ID3D11DeviceContext4 *context, unsigned p010, unsigned flags,
      unsigned yc, unsigned uc, unsigned vc)
{
   ID3D11Texture2D *input = NULL;
   IDXGIResource1 *shared = NULL;
   ID3D11Fence *ready = NULL, *done = NULL;
   HANDLE image_handle = NULL, ready_handle = NULL, done_handle = NULL;
   ID3D12DescriptorHeap *heap = NULL;
   ID3D12Resource *readback = NULL;
   ID3D12CommandAllocator *alloc = NULL;
   ID3D12GraphicsCommandList *cmd = NULL;
   d3d12_import_t imp;
   d3d12_texture_t output;
   video_frame_import_t f;
   uintptr_t id;
   D3D11_TEXTURE2D_DESC td;
   D3D11_SUBRESOURCE_DATA data;
   D3D12_DESCRIPTOR_HEAP_DESC hd;
   D3D12_CPU_DESCRIPTOR_HANDLE cpu;
   D3D12_GPU_DESCRIPTOR_HANDLE gpu;
   D3D12_HEAP_PROPERTIES hp;
   D3D12_RESOURCE_DESC rd;
   D3D12_UNORDERED_ACCESS_VIEW_DESC ud;
   D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp;
   D3D12_TEXTURE_COPY_LOCATION src, dst;
   D3D12_RANGE range;
   uint16_t words[54];
   uint8_t bytes[54], zero[108], *pixels = NULL;
   float c[6], expected[3], y, u, cr, divisor, center;
   UINT64 size, submitted, value;
   unsigned i, row, col, k;

   memset(&imp, 0, sizeof(imp));
   imp.output_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
   memset(&output, 0, sizeof(output));
   memset(&f, 0, sizeof(f));
   f.array_slice = array_case ? 1 : 0;
   f.crop_x = f.crop_y = array_case ? 2 : 0;
   memset(&td, 0, sizeof(td));
   td.Width = td.Height = array_case ? 6 : 2;
   td.MipLevels = td.SampleDesc.Count = 1;
   td.ArraySize = array_case ? 2 : 1;
   td.Format = p010 ? DXGI_FORMAT_P010 : DXGI_FORMAT_NV12;
   td.Usage = D3D11_USAGE_DEFAULT;
   td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
   td.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
   for (i = 0; i < td.Width * td.Height; i++)
   { words[i] = (uint16_t)((p010 ? 64 : 16) << 6); bytes[i] = 16; }
   for (; i < td.Width * td.Height * 3 / 2; i++)
   { words[i] = (uint16_t)(512 << 6); bytes[i] = 128; }
   for (row = 0; row < 2; row++)
      for (col = 0; col < 2; col++)
      {
         i = (row + (array_case ? 2 : 0)) * td.Width + col + (array_case ? 2 : 0);
         words[i] = (uint16_t)(yc << 6); bytes[i] = (uint8_t)yc;
      }
   i = td.Width * td.Height + (array_case ? td.Width + 2 : 0);
   words[i] = (uint16_t)(uc << 6); words[i + 1] = (uint16_t)(vc << 6);
   bytes[i] = (uint8_t)uc; bytes[i + 1] = (uint8_t)vc;
   memset(&data, 0, sizeof(data));
   data.pSysMem = p010 ? (void*)words : (void*)bytes;
   data.SysMemPitch = td.Width * (p010 ? 2 : 1);
   CHECK(SUCCEEDED(ID3D11Device5_CreateTexture2D(producer, &td, NULL, &input)));
   if (array_case)
   {
      memset(zero, 0, sizeof(zero));
      ID3D11DeviceContext4_UpdateSubresource(context, (ID3D11Resource*)input,
            0, NULL, zero, data.SysMemPitch, 0);
   }
   ID3D11DeviceContext4_UpdateSubresource(context, (ID3D11Resource*)input,
         array_case ? 1 : 0, NULL, data.pSysMem, data.SysMemPitch, 0);
   CHECK(SUCCEEDED(ID3D11Texture2D_QueryInterface(input, &IID_IDXGIResource1, (void**)&shared)));
   CHECK(SUCCEEDED(IDXGIResource1_CreateSharedHandle(shared, NULL,
         DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, NULL, &image_handle)));
   CHECK(SUCCEEDED(ID3D11Device5_CreateFence(producer, 0, D3D11_FENCE_FLAG_SHARED,
         &IID_ID3D11Fence, (void**)&ready)));
   CHECK(SUCCEEDED(ID3D11Device5_CreateFence(producer, 0, D3D11_FENCE_FLAG_SHARED,
         &IID_ID3D11Fence, (void**)&done)));
   CHECK(SUCCEEDED(ID3D11Fence_CreateSharedHandle(ready, NULL, GENERIC_ALL, NULL, &ready_handle)));
   CHECK(SUCCEEDED(ID3D11Fence_CreateSharedHandle(done, NULL, GENERIC_ALL, NULL, &done_handle)));
   memset(&hd, 0, sizeof(hd));
   hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
   hd.NumDescriptors = 2 * D3D12_IMPORT_TEXTURES + 1;
   hd.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
   CHECK(SUCCEEDED(ID3D12Device_CreateDescriptorHeap(v->device, &hd,
         &IID_ID3D12DescriptorHeap, (void**)&heap)));
   v->desc.srv_heap.handle = heap;
   v->desc.srv_heap.stride = ID3D12Device_GetDescriptorHandleIncrementSize(v->device, hd.Type);
   cpu = ID3D12DescriptorHeap_GetCPUDescriptorHandleForHeapStart(heap);
   gpu = ID3D12DescriptorHeap_GetGPUDescriptorHandleForHeapStart(heap);
   imp.cpu_uav = cpu; imp.gpu_uav = gpu;
   for (i = 0; i < D3D12_IMPORT_TEXTURES; i++)
   {
      imp.cpu_planes[i].ptr = cpu.ptr + (1 + 2 * i) * v->desc.srv_heap.stride;
      imp.gpu_planes[i].ptr = gpu.ptr + (1 + 2 * i) * v->desc.srv_heap.stride;
   }
   memset(&hp, 0, sizeof(hp)); hp.Type = D3D12_HEAP_TYPE_DEFAULT;
   memset(&rd, 0, sizeof(rd));
   rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
   rd.Width = rd.Height = 2;
   rd.DepthOrArraySize = rd.MipLevels = rd.SampleDesc.Count = 1;
   rd.Format = output_fp16 ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
   rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
   CHECK(SUCCEEDED(ID3D12Device_CreateCommittedResource(v->device, &hp,
         D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
         NULL, &IID_ID3D12Resource, (void**)&output.handle)));
   output.desc = rd; output.import = &imp;
   memset(&ud, 0, sizeof(ud)); ud.Format = rd.Format;
   ud.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
   ID3D12Device_CreateUnorderedAccessView(v->device, output.handle, NULL, &ud, cpu);
   f.image = (uint64_t)(uintptr_t)image_handle;
   f.wait = (uint64_t)(uintptr_t)ready_handle; f.wait_value = 1;
   f.done = (uint64_t)(uintptr_t)done_handle; f.done_value = 1;
   f.output_pixfmt = output_fp16 ? IMAGE_PIXFMT_FP16 : 0;
   f.pixfmt = p010 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
   f.dims = VIDEO_SCALE_PACK(2, 2);
   f.yuv = flags;
   if (hdr_mode)
   {
      f.color.transfer = hdr_mode == 1 ? 16 : 18;
      f.color.primaries = f.color.matrix = 9;
      f.color.max_cll = 1000;
   }
   f.owner = &imp; imp.owner = f.owner;
   f.abandon = unexpected_abandon;
   id = (uintptr_t)&output;
   submitted = v->import_value;
   k = v->import_next % D3D12_IMPORT_LISTS;
   f.output_pixfmt = IMAGE_PIXFMT_P010;
   CHECK(d3d12_import_texture(v, &id, &f, TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_REFUSED
         && v->import_value == submitted && !ID3D11Fence_GetCompletedValue(done));
   f.output_pixfmt = output_fp16 ? IMAGE_PIXFMT_8888 : IMAGE_PIXFMT_FP16;
   CHECK(d3d12_import_texture(v, &id, &f, TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_REFUSED
         && v->import_value == submitted && !ID3D11Fence_GetCompletedValue(done));
   f.output_pixfmt = output_fp16 ? IMAGE_PIXFMT_FP16 : 0;
   /* An occupied list or descriptor pool must return without queuing work. */
   value = v->import_fence[k];
   v->import_fence[k] = ~(UINT64)0 - 1;
   CHECK(d3d12_import_texture(v, &id, &f, TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_DROPPED
         && v->import_value == submitted && !ID3D11Fence_GetCompletedValue(done));
   v->import_fence[k] = value;
   for (i = 0; i < D3D12_IMPORT_TEXTURES; i++) imp.tex_fence[i] = ~(UINT64)0 - 1;
   CHECK(d3d12_import_texture(v, &id, &f, TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_DROPPED
         && v->import_value == submitted);
   memset(imp.tex_fence, 0, sizeof(imp.tex_fence));
   f.image = ~(uint64_t)0;
   CHECK(d3d12_import_texture(v, &id, &f, TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_REFUSED
         && v->import_value == submitted);
   f.image = (uint64_t)(uintptr_t)image_handle;
   f.wait = ~(uint64_t)0;
   CHECK(d3d12_import_texture(v, &id, &f, TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_REFUSED
         && v->import_value == submitted);
   f.wait = (uint64_t)(uintptr_t)ready_handle;
   imp.num_fences = D3D12_IMPORT_FENCES;
   CHECK(d3d12_import_texture(v, &id, &f, TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_REFUSED
         && v->import_value == submitted);
   imp.num_fences = 0;
   CHECK(!abandoned && !ID3D11Fence_GetCompletedValue(done));
   if (array_case)
      CHECK(d3d12_import_tex(v, &imp, f.image, 2, 2, f.pixfmt, 0,
            f.crop_x, f.crop_y, 0) >= 0);
   CHECK(d3d12_import_texture(v, &id, &f, TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_DONE);
   /* The importer returned even though the producer has not signalled. */
   CHECK(v->import_value == submitted + 1
         && ID3D12Fence_GetCompletedValue(v->import_done) < v->import_value
         && !ID3D11Fence_GetCompletedValue(done));
   CHECK(SUCCEEDED(ID3D11DeviceContext4_Signal(context, ready, 1)));
   ID3D11DeviceContext4_Flush(context);
   CHECK(wait_for(v->import_done, v->import_value));
   CHECK(ID3D11Fence_GetCompletedValue(done) == 1);
   CHECK(d3d12_import_tex(v, &imp, f.image, 2, 2, f.pixfmt, f.array_slice, f.crop_x, f.crop_y, v->import_value) == 0);
   CHECK(d3d12_import_tex(v, &imp, f.image, td.Width + 2, 2, f.pixfmt, f.array_slice, f.crop_x, f.crop_y, v->import_value) < 0);
   CHECK(d3d12_import_tex(v, &imp, f.image, 2, 2,
         p010 ? IMAGE_PIXFMT_NV12 : IMAGE_PIXFMT_P010, f.array_slice, f.crop_x, f.crop_y, v->import_value) < 0);
   f.array_slice = td.ArraySize;
   CHECK(d3d12_import_texture(v, &id, &f, TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_REFUSED);
   f.array_slice = array_case ? 1 : 0;
   f.crop_x = 1;
   CHECK(d3d12_import_texture(v, &id, &f, TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_REFUSED);
   f.crop_x = array_case ? 2 : 0;
   f.crop_y = td.Height + 2;
   CHECK(d3d12_import_texture(v, &id, &f, TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_REFUSED);
   f.crop_y = array_case ? 2 : 0;
   f.pixfmt = IMAGE_PIXFMT_I420;
   CHECK(d3d12_import_texture(v, &id, &f, TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_REFUSED);
   f.output_pixfmt = output_fp16 ? IMAGE_PIXFMT_FP16 : 0;
   f.pixfmt = p010 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
   v->import_formats &= ~f.pixfmt;
   CHECK(d3d12_import_texture(v, &id, &f, TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_REFUSED);
   v->import_formats |= f.pixfmt;
   f.output_pixfmt = output_fp16 ? IMAGE_PIXFMT_FP16 : 0;
   f.pixfmt = p010 ? IMAGE_PIXFMT_NV12 : IMAGE_PIXFMT_P010;
   CHECK(d3d12_import_texture(v, &id, &f, TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_REFUSED);
   f.output_pixfmt = output_fp16 ? IMAGE_PIXFMT_FP16 : 0;
   f.pixfmt = p010 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
   f.dims = VIDEO_SCALE_PACK(4, 2);
   CHECK(d3d12_import_texture(v, &id, &f, TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_REFUSED);
   f.dims = VIDEO_SCALE_PACK(2, 2);
   CHECK(v->import_value == submitted + 1 && !abandoned);
   f.done_value = 2;
   CHECK(d3d12_import_texture(v, &id, &f, TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_DONE
         && imp.num_fences == 2 && v->import_value == submitted + 2);
   CHECK(wait_for(v->import_done, v->import_value)
         && ID3D11Fence_GetCompletedValue(done) == 2 && !abandoned);

   CHECK(imp.output_state == (v->movie_importing ? D3D12_RESOURCE_STATE_COPY_SOURCE
         : D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
   ID3D12Device_GetCopyableFootprints(v->device, &rd, 0, 1, 0, &fp, NULL, NULL, &size);
   hp.Type = D3D12_HEAP_TYPE_READBACK;
   memset(&rd, 0, sizeof(rd)); rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
   rd.Width = size; rd.Height = rd.DepthOrArraySize = rd.MipLevels = rd.SampleDesc.Count = 1;
   rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
   CHECK(SUCCEEDED(ID3D12Device_CreateCommittedResource(v->device, &hp,
         D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST,
         NULL, &IID_ID3D12Resource, (void**)&readback)));
   CHECK(SUCCEEDED(ID3D12Device_CreateCommandAllocator(v->device, D3D12_COMMAND_LIST_TYPE_DIRECT,
         &IID_ID3D12CommandAllocator, (void**)&alloc)));
   CHECK(SUCCEEDED(ID3D12Device_CreateCommandList(v->device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT,
         alloc, NULL, &IID_ID3D12GraphicsCommandList, (void**)&cmd)));
   if (imp.output_state != D3D12_RESOURCE_STATE_COPY_SOURCE)
      transition(cmd, output.handle, imp.output_state, D3D12_RESOURCE_STATE_COPY_SOURCE);
   memset(&src, 0, sizeof(src)); memset(&dst, 0, sizeof(dst));
   src.pResource = output.handle; src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
   dst.pResource = readback; dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
   dst.PlacedFootprint = fp;
   ID3D12GraphicsCommandList_CopyTextureRegion(cmd, &dst, 0, 0, 0, &src, NULL);
   CHECK(SUCCEEDED(ID3D12GraphicsCommandList_Close(cmd)));
   ID3D12CommandQueue_ExecuteCommandLists(v->queue.handle, 1, (ID3D12CommandList* const*)&cmd);
   CHECK(SUCCEEDED(ID3D12CommandQueue_Signal(v->queue.handle, v->import_done, ++v->import_value)));
   CHECK(wait_for(v->import_done, v->import_value));
   range.Begin = 0; range.End = (SIZE_T)size;
   CHECK(SUCCEEDED(ID3D12Resource_Map(readback, 0, &range, (void**)&pixels)));
   divisor = p010 ? ((flags & IMAGE_YUV_FLAG_FULL_RANGE) ? 1023.0f : 1020.0f) : 255.0f;
   center = p010 && (flags & IMAGE_YUV_FLAG_FULL_RANGE) ? 512.0f / 1023.0f : 128.0f / 255.0f;
   y = yc / divisor; u = uc / divisor - center; cr = vc / divisor - center;
   image_yuv_coefficients(flags, c);
   expected[0] = c[0] * y + c[1] + c[2] * cr;
   expected[1] = c[0] * y + c[1] + c[3] * u + c[4] * cr;
   expected[2] = c[0] * y + c[1] + c[5] * u;
   if (hdr_mode)
      CHECK(hdr_reference_pixel(p010, !!(flags & IMAGE_YUV_FLAG_FULL_RANGE),
            f.color.transfer, yc, uc, vc, expected));
   for (row = 0; row < 2; row++) for (col = 0; col < 2; col++)
   {
      const uint8_t *pixel = pixels + fp.Offset + row * fp.Footprint.RowPitch + col * (output_fp16 ? 8 : 4);
      if (output_fp16)
      {
         fp16_reference(p010 ? 10 : 8, !!(flags & IMAGE_YUV_FLAG_FULL_RANGE),
               f.color.transfer, flags, yc, uc, vc, expected);
         CHECK(fp16_reference_check((const uint16_t*)pixel, expected));
         continue;
      }
      for (i = 0; i < 3; i++)
      {
         float ref = expected[i] < 0 ? 0 : expected[i] > 1 ? 1 : expected[i];
         if (fabs(pixel[i] - ref * 255.0) > 1.1)
         { fprintf(stderr, "D3D12 P010=%u flags=%u channel=%u: %u != %.2f\n", p010, flags, i, pixel[i], ref * 255.0); failures++; }
      }
      if (pixel[3] != 255) failures++;
   }
   range.Begin = range.End = 0;
   ID3D12Resource_Unmap(readback, 0, &range); pixels = NULL;
end:
   /* Release the producer gate on failures before destroying queue resources. */
   if (ready) { ID3D11DeviceContext4_Signal(context, ready, 1); ID3D11DeviceContext4_Flush(context); }
   if (v->import_value) wait_for(v->import_done, v->import_value);
   for (i = 0; i < num_retired; i++) DROP(retired[i]);
   num_retired = 0;
   for (i = 0; i < D3D12_IMPORT_TEXTURES; i++) DROP(imp.tex[i]);
   for (i = 0; i < imp.num_fences; i++) DROP(imp.fence[i]);
   DROP(cmd); DROP(alloc); DROP(readback); DROP(output.handle); DROP(heap);
   v->desc.srv_heap.handle = NULL;
   DROP(shared); DROP(input); DROP(ready); DROP(done);
   if (image_handle) CloseHandle(image_handle);
   if (ready_handle) CloseHandle(ready_handle);
   if (done_handle) CloseHandle(done_handle);
}

int main(void)
{
   d3d12_video_t v;
   IDXGIFactory4 *factory = NULL;
   IDXGIAdapter *adapter = NULL;
   ID3D11Device *device = NULL;
   ID3D11Device5 *producer = NULL;
   ID3D11DeviceContext *ctx = NULL;
   ID3D11DeviceContext4 *context = NULL;
   D3D12_COMMAND_QUEUE_DESC qd;
   D3D12_FEATURE_DATA_FORMAT_SUPPORT fs;
   unsigned depth, flags, i, movie;
   memset(&v, 0, sizeof(v));
   if (FAILED(CreateDXGIFactory1(&IID_IDXGIFactory4, (void**)&factory))
         || FAILED(IDXGIFactory4_EnumAdapters(factory, 0, &adapter))
         || FAILED(D3D12CreateDevice((IUnknown*)adapter, D3D_FEATURE_LEVEL_11_0,
               &IID_ID3D12Device, (void**)&v.device))
         || FAILED(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL, 0,
               NULL, 0, D3D11_SDK_VERSION, &device, NULL, &ctx))
         || FAILED(ID3D11Device_QueryInterface(device, &IID_ID3D11Device5, (void**)&producer))
         || FAILED(ID3D11DeviceContext_QueryInterface(ctx, &IID_ID3D11DeviceContext4, (void**)&context)))
   { puts("SKIP D3D12 shared-frame oracle: no compatible device"); goto end; }
   memset(&qd, 0, sizeof(qd)); qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
   CHECK(SUCCEEDED(ID3D12Device_CreateCommandQueue(v.device, &qd,
         &IID_ID3D12CommandQueue, (void**)&v.queue.handle)));
   CHECK(SUCCEEDED(ID3D12Device_CreateFence(v.device, 0, D3D12_FENCE_FLAG_NONE,
         &IID_ID3D12Fence, (void**)&v.import_done)));
   CHECK(pipeline(&v));
   v.video_decode = true;
   v.import_formats = IMAGE_PIXFMT_NV12 | IMAGE_PIXFMT_P010;
   for (array_case = 0; array_case < 2; array_case++)
   for (depth = 0; depth < 2; depth++)
   {
      memset(&fs, 0, sizeof(fs)); fs.Format = depth ? DXGI_FORMAT_P010 : DXGI_FORMAT_NV12;
      if (FAILED(ID3D12Device_CheckFeatureSupport(v.device, D3D12_FEATURE_FORMAT_SUPPORT, &fs, sizeof(fs)))
            || !(fs.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_LOAD))
      { printf("SKIP D3D12 %s format\n", depth ? "P010" : "NV12"); continue; }
      for (movie = 0; movie < 2; movie++)
      for (output_fp16 = 0; output_fp16 < 2; output_fp16++)
      for (hdr_mode = 0; hdr_mode < 3; hdr_mode++)
      for (flags = 0; flags < 4; flags++)
      {
         unsigned scale = depth ? 4 : 1, center = 128 * scale;
         unsigned full = flags & IMAGE_YUV_FLAG_FULL_RANGE;
         v.movie_importing = movie != 0;
         color(&v, producer, context, depth, flags, full ? 0 : 16 * scale, center, center);
         color(&v, producer, context, depth, flags, full ? (depth ? 1023 : 255) : 235 * scale, center, center);
         color(&v, producer, context, depth, flags, 127 * scale, center, center);
         color(&v, producer, context, depth, flags, 110 * scale, 90 * scale, 170 * scale);
         if (depth)
            color(&v, producer, context, depth, flags, 441, 361, 681);
      }
      printf("D3D12 %s production import/queue/fence oracle completed\n", depth ? "P010" : "NV12");
   }
end:
   for (i = 0; i < D3D12_IMPORT_LISTS; i++) { DROP(v.import_cmd[i]); DROP(v.import_alloc[i]); }
   DROP(v.import_done); DROP(v.nv12_pipe); DROP(v.desc.cs_rootSignature);
   DROP(v.queue.handle); DROP(v.device);
   DROP(context); DROP(ctx); DROP(producer); DROP(device); DROP(adapter); DROP(factory);
   printf("D3D12 import oracle: %d failures\n", failures);
   return failures ? 1 : 0;
}
#else
int main(void)
{
   (void)d3d11_planar_cs_src; (void)d3d12_nv12_cs_src;
   puts("SKIP D3D12 shared-frame oracle: requires Windows"); return 0;
}
#endif
