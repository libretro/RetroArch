/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef D3D12_UPLOAD_HEAP_H__
#define D3D12_UPLOAD_HEAP_H__

/* Where a buffer the CPU only writes and the GPU reads goes: the GPU
 * upload heap - video memory the CPU maps, which a device offers when
 * it maps all of its memory (resizable BAR) - or the upload heap in
 * system memory everywhere else. The includer brings <d3d12.h> in C
 * interface form; samples/gfx/d3d12_upload_heap runs these against a
 * device. */

#include <string.h>

#include <retro_inline.h>

/* Named here rather than taken from <d3d12.h>, which has them only
 * from the Agility SDK 1.613 headers on: the values are the API's. */
#define D3D12_UPLOAD_HEAP_TYPE_GPU_UPLOAD       ((D3D12_HEAP_TYPE)5)
#define D3D12_UPLOAD_HEAP_FEATURE_OPTIONS16     ((D3D12_FEATURE)45)

typedef struct d3d12_upload_heap_options16
{
   BOOL DynamicDepthBiasSupported;
   BOOL GPUUploadHeapSupported;
} d3d12_upload_heap_options16_t;

/* The heap for CPU-written buffers on @device. A runtime that predates
 * the query refuses it, and that is the upload heap too. */
static INLINE D3D12_HEAP_TYPE d3d12_cpu_write_heap_type(ID3D12Device *device)
{
   d3d12_upload_heap_options16_t opts;
   memset(&opts, 0, sizeof(opts));
   if (     SUCCEEDED(device->lpVtbl->CheckFeatureSupport(device,
               D3D12_UPLOAD_HEAP_FEATURE_OPTIONS16, &opts, sizeof(opts)))
         && opts.GPUUploadHeapSupported)
      return D3D12_UPLOAD_HEAP_TYPE_GPU_UPLOAD;
   return D3D12_HEAP_TYPE_UPLOAD;
}

/* A buffer of @size bytes in a heap of @type, in GENERIC_READ, which
 * every heap takes for a buffer and the upload heap requires. One the
 * GPU upload heap refuses - its video memory full - is made in the
 * upload heap instead. */
static INLINE HRESULT d3d12_create_cpu_write_buffer(ID3D12Device *device,
      D3D12_HEAP_TYPE type, UINT64 size, ID3D12Resource **buffer)
{
   D3D12_RESOURCE_DESC   desc;
   D3D12_HEAP_PROPERTIES heap;
   HRESULT               hr;

   memset(&heap, 0, sizeof(heap));
   heap.Type                 = type;
   heap.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
   heap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
   heap.CreationNodeMask     = 1;
   heap.VisibleNodeMask      = 1;

   memset(&desc, 0, sizeof(desc));
   desc.Dimension            = D3D12_RESOURCE_DIMENSION_BUFFER;
   desc.Alignment            = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
   desc.Width                = size;
   desc.Height               = 1;
   desc.DepthOrArraySize     = 1;
   desc.MipLevels            = 1;
   desc.Format               = DXGI_FORMAT_UNKNOWN;
   desc.SampleDesc.Count     = 1;
   desc.SampleDesc.Quality   = 0;
   desc.Layout               = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
   desc.Flags                = D3D12_RESOURCE_FLAG_NONE;

   *buffer = NULL;
   hr      = device->lpVtbl->CreateCommittedResource(device, &heap,
         D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
         NULL, &IID_ID3D12Resource, (void**)buffer);
   if (FAILED(hr) && type != D3D12_HEAP_TYPE_UPLOAD)
   {
      *buffer   = NULL;
      heap.Type = D3D12_HEAP_TYPE_UPLOAD;
      hr        = device->lpVtbl->CreateCommittedResource(device, &heap,
            D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
            NULL, &IID_ID3D12Resource, (void**)buffer);
   }
   return hr;
}

#endif
