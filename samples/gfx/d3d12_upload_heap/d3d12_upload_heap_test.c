/* gfx/common/d3d12_upload_heap.h: which heap the D3D12 driver puts the
 * buffers the CPU writes in, and that a buffer made there carries what
 * the CPU wrote to the GPU.
 *
 *   choice      against a device that only answers the two calls the
 *               header makes: the GPU upload heap when the device says
 *               it has one, the upload heap when it says not or does
 *               not know the query (a runtime older than it)
 *   refusal     a device that offers the GPU upload heap and then
 *               refuses a buffer in it (its video memory full) gets
 *               the buffer in the upload heap, and one that refuses
 *               that too is an error
 *   round trip  on the D3D12 device there is, whichever heap it gives:
 *               a buffer made by the header, written through its
 *               mapping, copied by the GPU into a readback buffer,
 *               reads back as written; the heap it is in is the one
 *               asked for
 *
 * Windows only: cross-built with mingw-w64 and run under Wine, where
 * vkd3d implements D3D12 over Vulkan. Wine's own vkd3d has no GPU upload
 * heap, so there the round trip runs on the upload heap; vkd3d-proton
 * in Wine's place (its d3d12.dll and d3d12core.dll beside the test,
 * WINEDLLOVERRIDES=d3d12,d3d12core=n) has one on any device that maps
 * all of its memory, lavapipe included. REQUIRE_GPU_UPLOAD=1 fails the
 * run unless the round trip ran there. With no D3D12 device at all the
 * round trip is skipped and says so. */
#ifndef CINTERFACE
#define CINTERFACE
#endif
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <d3d12.h>

#include "../../../gfx/common/d3d12_upload_heap.h"

#define SIZE 65536

static int failures;

static void check(int ok, const char *fmt, ...)
{
   va_list ap;
   if (ok)
      return;
   va_start(ap, fmt);
   printf("FAIL: ");
   vprintf(fmt, ap);
   printf("\n");
   va_end(ap);
   failures++;
}

/* ---- a device that answers the header's two calls ---------------- */

typedef struct
{
   ID3D12DeviceVtbl *lpVtbl;
   HRESULT           options16;      /* what the query returns */
   BOOL              gpu_upload;     /* and what it says */
   int               refuse_gpu;     /* refuse a GPU upload heap buffer */
   int               refuse_upload;  /* refuse an upload heap buffer */
   int               creates;
   D3D12_HEAP_TYPE   last_type;
   D3D12_RESOURCE_STATES last_state;
} fake_device_t;

static HRESULT STDMETHODCALLTYPE fake_CheckFeatureSupport(ID3D12Device *dev,
      D3D12_FEATURE feature, void *data, UINT size)
{
   fake_device_t *f = (fake_device_t*)dev;
   if (feature != D3D12_UPLOAD_HEAP_FEATURE_OPTIONS16
         || size != sizeof(d3d12_upload_heap_options16_t))
      return E_INVALIDARG;
   if (SUCCEEDED(f->options16))
      ((d3d12_upload_heap_options16_t*)data)->GPUUploadHeapSupported = f->gpu_upload;
   return f->options16;
}

static HRESULT STDMETHODCALLTYPE fake_CreateCommittedResource(ID3D12Device *dev,
      const D3D12_HEAP_PROPERTIES *heap, D3D12_HEAP_FLAGS flags,
      const D3D12_RESOURCE_DESC *desc, D3D12_RESOURCE_STATES state,
      const D3D12_CLEAR_VALUE *clear, REFIID riid, void **out)
{
   static int resource;
   fake_device_t *f = (fake_device_t*)dev;
   (void)flags; (void)desc; (void)clear; (void)riid;
   f->creates++;
   f->last_type  = heap->Type;
   f->last_state = state;
   if (     (heap->Type == D3D12_UPLOAD_HEAP_TYPE_GPU_UPLOAD && f->refuse_gpu)
         || (heap->Type == D3D12_HEAP_TYPE_UPLOAD && f->refuse_upload))
      return E_OUTOFMEMORY;
   *out = &resource;
   return S_OK;
}

static void fake_init(fake_device_t *f, ID3D12DeviceVtbl *vtbl)
{
   memset(vtbl, 0, sizeof(*vtbl));
   vtbl->CheckFeatureSupport     = fake_CheckFeatureSupport;
   vtbl->CreateCommittedResource = fake_CreateCommittedResource;
   memset(f, 0, sizeof(*f));
   f->lpVtbl    = vtbl;
   f->options16 = S_OK;
}

static void test_choice(void)
{
   ID3D12DeviceVtbl vtbl;
   fake_device_t    f;
   int              had = failures;

   fake_init(&f, &vtbl);
   f.gpu_upload = TRUE;
   check(d3d12_cpu_write_heap_type((ID3D12Device*)&f)
         == D3D12_UPLOAD_HEAP_TYPE_GPU_UPLOAD,
         "a device with a GPU upload heap: not chosen");

   f.gpu_upload = FALSE;
   check(d3d12_cpu_write_heap_type((ID3D12Device*)&f)
         == D3D12_HEAP_TYPE_UPLOAD,
         "a device without a GPU upload heap: not the upload heap");

   f.gpu_upload = TRUE;
   f.options16  = E_INVALIDARG;
   check(d3d12_cpu_write_heap_type((ID3D12Device*)&f)
         == D3D12_HEAP_TYPE_UPLOAD,
         "a runtime that does not know the query: not the upload heap");
   if (failures == had)
      printf("[pass] choice\n");
}

static void test_refusal(void)
{
   ID3D12DeviceVtbl vtbl;
   fake_device_t    f;
   ID3D12Resource  *buf = NULL;
   HRESULT          hr;
   int              had = failures;

   fake_init(&f, &vtbl);
   hr = d3d12_create_cpu_write_buffer((ID3D12Device*)&f,
         D3D12_UPLOAD_HEAP_TYPE_GPU_UPLOAD, SIZE, &buf);
   check(SUCCEEDED(hr) && buf && f.creates == 1
         && f.last_type == D3D12_UPLOAD_HEAP_TYPE_GPU_UPLOAD
         && f.last_state == D3D12_RESOURCE_STATE_GENERIC_READ,
         "GPU upload heap granted: %d creates, last in heap %d",
         f.creates, (int)f.last_type);

   fake_init(&f, &vtbl);
   f.refuse_gpu = 1;
   buf = NULL;
   hr  = d3d12_create_cpu_write_buffer((ID3D12Device*)&f,
         D3D12_UPLOAD_HEAP_TYPE_GPU_UPLOAD, SIZE, &buf);
   check(SUCCEEDED(hr) && buf && f.creates == 2
         && f.last_type == D3D12_HEAP_TYPE_UPLOAD
         && f.last_state == D3D12_RESOURCE_STATE_GENERIC_READ,
         "GPU upload heap refused: %d creates, last in heap %d",
         f.creates, (int)f.last_type);

   fake_init(&f, &vtbl);
   f.refuse_gpu    = 1;
   f.refuse_upload = 1;
   buf = (ID3D12Resource*)&f;
   hr  = d3d12_create_cpu_write_buffer((ID3D12Device*)&f,
         D3D12_UPLOAD_HEAP_TYPE_GPU_UPLOAD, SIZE, &buf);
   check(FAILED(hr) && !buf && f.creates == 2,
         "both heaps refused: hr 0x%08lx, buffer %p, %d creates",
         (unsigned long)hr, (void*)buf, f.creates);

   fake_init(&f, &vtbl);
   f.refuse_upload = 1;
   hr  = d3d12_create_cpu_write_buffer((ID3D12Device*)&f,
         D3D12_HEAP_TYPE_UPLOAD, SIZE, &buf);
   check(FAILED(hr) && f.creates == 1,
         "upload heap refused: tried %d times, not once", f.creates);
   if (failures == had)
      printf("[pass] refusal\n");
}

/* ---- the device there is ----------------------------------------- */

static ID3D12Resource *make_readback(ID3D12Device *dev)
{
   D3D12_HEAP_PROPERTIES heap;
   D3D12_RESOURCE_DESC   desc;
   ID3D12Resource       *res = NULL;

   memset(&heap, 0, sizeof(heap));
   heap.Type             = D3D12_HEAP_TYPE_READBACK;
   heap.CreationNodeMask = 1;
   heap.VisibleNodeMask  = 1;
   memset(&desc, 0, sizeof(desc));
   desc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
   desc.Width            = SIZE;
   desc.Height           = 1;
   desc.DepthOrArraySize = 1;
   desc.MipLevels        = 1;
   desc.SampleDesc.Count = 1;
   desc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
   if (FAILED(dev->lpVtbl->CreateCommittedResource(dev, &heap,
               D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST,
               NULL, &IID_ID3D12Resource, (void**)&res)))
      return NULL;
   return res;
}

static int test_round_trip(void)
{
   ID3D12Device              *dev   = NULL;
   ID3D12CommandQueue        *queue = NULL;
   ID3D12CommandAllocator    *alloc = NULL;
   ID3D12GraphicsCommandList *list  = NULL;
   ID3D12Fence               *fence = NULL;
   ID3D12Resource            *buf   = NULL;
   ID3D12Resource            *rb    = NULL;
   ID3D12CommandList         *lists[1];
   D3D12_COMMAND_QUEUE_DESC   qdesc;
   D3D12_HEAP_PROPERTIES      props;
   D3D12_HEAP_FLAGS           hflags;
   D3D12_RANGE                none;
   D3D12_RANGE                all;
   D3D12_HEAP_TYPE            type;
   HANDLE                     event;
   unsigned                  *p     = NULL;
   unsigned                   i, bad = 0;
   const char                *require = getenv("REQUIRE_GPU_UPLOAD");
   int                        had     = failures;

   if (FAILED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0,
               &IID_ID3D12Device, (void**)&dev)))
   {
      printf("[skip] round trip (no D3D12 device)\n");
      check(!(require && *require), "no D3D12 device, and REQUIRE_GPU_UPLOAD is set");
      return 0;
   }

   type = d3d12_cpu_write_heap_type(dev);
   printf("[info] the device's heap for CPU-written buffers: %s\n",
         type == D3D12_UPLOAD_HEAP_TYPE_GPU_UPLOAD
         ? "GPU upload (video memory)" : "upload (system memory)");
   check(!(require && *require) || type == D3D12_UPLOAD_HEAP_TYPE_GPU_UPLOAD,
         "REQUIRE_GPU_UPLOAD is set and the device has no GPU upload heap");

   check(SUCCEEDED(d3d12_create_cpu_write_buffer(dev, type, SIZE, &buf)) && buf,
         "no buffer");
   rb = make_readback(dev);
   check(rb != NULL, "no readback buffer");
   if (!buf || !rb)
      goto done;

   memset(&props, 0, sizeof(props));
   check(SUCCEEDED(buf->lpVtbl->GetHeapProperties(buf, &props, &hflags))
         && props.Type == type,
         "the buffer is in heap %d, not %d", (int)props.Type, (int)type);

   /* Written as the driver writes its buffers: mapped with nothing to
    * read, every word stored, never loaded. */
   none.Begin = 0;
   none.End   = 0;
   check(SUCCEEDED(buf->lpVtbl->Map(buf, 0, &none, (void**)&p)) && p,
         "the buffer does not map");
   if (!p)
      goto done;
   for (i = 0; i < SIZE / sizeof(unsigned); i++)
      p[i] = 0x9e3779b9u * (i + 1);
   buf->lpVtbl->Unmap(buf, 0, NULL);

   memset(&qdesc, 0, sizeof(qdesc));
   qdesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
   dev->lpVtbl->CreateCommandQueue(dev, &qdesc,
         &IID_ID3D12CommandQueue, (void**)&queue);
   dev->lpVtbl->CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT,
         &IID_ID3D12CommandAllocator, (void**)&alloc);
   dev->lpVtbl->CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT,
         alloc, NULL, &IID_ID3D12GraphicsCommandList, (void**)&list);
   dev->lpVtbl->CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE,
         &IID_ID3D12Fence, (void**)&fence);
   check(queue && alloc && list && fence, "no queue, list or fence");
   if (!queue || !alloc || !list || !fence)
      goto done;

   list->lpVtbl->CopyBufferRegion(list, rb, 0, buf, 0, SIZE);
   list->lpVtbl->Close(list);
   lists[0] = (ID3D12CommandList*)list;
   queue->lpVtbl->ExecuteCommandLists(queue, 1, lists);
   queue->lpVtbl->Signal(queue, fence, 1);
   event = CreateEventA(NULL, FALSE, FALSE, NULL);
   fence->lpVtbl->SetEventOnCompletion(fence, 1, event);
   check(WaitForSingleObject(event, 10000) == WAIT_OBJECT_0,
         "the copy did not finish");
   CloseHandle(event);

   all.Begin = 0;
   all.End   = SIZE;
   p         = NULL;
   check(SUCCEEDED(rb->lpVtbl->Map(rb, 0, &all, (void**)&p)) && p,
         "the readback buffer does not map");
   if (p)
   {
      for (i = 0; i < SIZE / sizeof(unsigned); i++)
         if (p[i] != 0x9e3779b9u * (i + 1))
            bad++;
      none.End = 0;
      rb->lpVtbl->Unmap(rb, 0, &none);
      check(bad == 0, "%u of %u words read back wrong", bad,
            (unsigned)(SIZE / sizeof(unsigned)));
   }
   if (failures == had)
      printf("[pass] round trip (%s)\n",
            type == D3D12_UPLOAD_HEAP_TYPE_GPU_UPLOAD
            ? "GPU upload heap" : "upload heap");

done:
   if (rb)    rb->lpVtbl->Release(rb);
   if (buf)   buf->lpVtbl->Release(buf);
   if (fence) fence->lpVtbl->Release(fence);
   if (list)  list->lpVtbl->Release(list);
   if (alloc) alloc->lpVtbl->Release(alloc);
   if (queue) queue->lpVtbl->Release(queue);
   dev->lpVtbl->Release(dev);
   return 0;
}

int main(void)
{
   test_choice();
   test_refusal();
   test_round_trip();
   if (failures)
   {
      printf("d3d12_upload_heap: %d failure(s)\n", failures);
      return 1;
   }
   printf("d3d12_upload_heap: ok\n");
   return 0;
}
