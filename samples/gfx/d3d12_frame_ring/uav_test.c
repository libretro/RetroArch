/* Shipping descriptor construction: level-zero conversion vs mip UAVs. */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
typedef struct {uintptr_t ptr;} D3D12_CPU_DESCRIPTOR_HANDLE;
typedef struct {uintptr_t ptr;} D3D12_GPU_DESCRIPTOR_HANDLE;
typedef struct {D3D12_CPU_DESCRIPTOR_HANDLE cpu;D3D12_GPU_DESCRIPTOR_HANDLE gpu;} d3d12_descriptor_heap_t;
typedef struct {unsigned Format,ViewDimension;struct {unsigned MipSlice,PlaneSlice;} Texture2D;} D3D12_UNORDERED_ACCESS_VIEW_DESC;
typedef void ID3D12Resource;
#define D3D12_UAV_DIMENSION_TEXTURE2D 1
typedef struct {
   struct {unsigned Format,MipLevels;} desc;
   unsigned media_compute;
   ID3D12Resource *handle;
   d3d12_descriptor_heap_t *srv_heap;
   D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor[8];
   D3D12_GPU_DESCRIPTOR_HANDLE gpu_descriptor[8];
} d3d12_texture_t;
struct device;
typedef struct device *D3D12Device;
static unsigned allocations,fail_at,calls,slices[8];
static D3D12_CPU_DESCRIPTOR_HANDLE d3d12_descriptor_heap_slot_alloc(d3d12_descriptor_heap_t *heap)
{D3D12_CPU_DESCRIPTOR_HANDLE h;allocations++;h.ptr=allocations==fail_at?0:heap->cpu.ptr+allocations*10;return h;}
struct device {struct methods {void (*CreateUnorderedAccessView)(D3D12Device,ID3D12Resource*,void*,const D3D12_UNORDERED_ACCESS_VIEW_DESC*,D3D12_CPU_DESCRIPTOR_HANDLE);} *lpVtbl;};
static void create(D3D12Device device,ID3D12Resource *resource,void *counter,const D3D12_UNORDERED_ACCESS_VIEW_DESC *desc,D3D12_CPU_DESCRIPTOR_HANDLE handle)
{(void)device;assert(resource && !counter && handle.ptr && desc->Format==9 && desc->ViewDimension==1 && !desc->Texture2D.PlaneSlice);slices[calls++]=desc->Texture2D.MipSlice;}
#include "uavs.inc"
int main(void)
{
   struct methods methods={create};struct device device={&methods};
   d3d12_descriptor_heap_t heap;
   d3d12_texture_t texture;
   memset(&heap,0,sizeof(heap));heap.cpu.ptr=1000;heap.gpu.ptr=5000;
   memset(&texture,0,sizeof(texture));texture.desc.Format=9;texture.desc.MipLevels=1;
   texture.srv_heap=&heap;texture.handle=&texture;
   init_uavs(&device,&texture);assert(!calls && !allocations);
   texture.media_compute=1;
   init_uavs(&device,&texture);assert(calls==1 && slices[0]==0);
   assert(texture.cpu_descriptor[1].ptr==1010 && texture.gpu_descriptor[1].ptr==5010);
   memset(texture.cpu_descriptor,0,sizeof(texture.cpu_descriptor));calls=allocations=0;fail_at=1;
   init_uavs(&device,&texture);assert(!calls && allocations==1 && !texture.cpu_descriptor[1].ptr);
   calls=allocations=fail_at=0;texture.media_compute=0;texture.desc.MipLevels=4;
   init_uavs(&device,&texture);assert(calls==3 && slices[0]==1 && slices[1]==2 && slices[2]==3);
   puts("D3D12 UAV descriptors: immutable level-zero outputs, mip fallback and heap exhaustion passed");return 0;
}
