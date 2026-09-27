/* libretro_d3d12.h version 2, run: a core and a frontend that presents
 * from another thread, sharing a device and a queue the way a hardware
 * core and RetroArch's threaded video do, with the handoff the header
 * describes and nothing else - in particular, no copy of the frame on
 * the core's thread, which is what version 1 forces.
 *
 *   core      draws frame n (a colour that encodes n) into the texture
 *             for the current sync index, submits, and hands it over
 *             with the fence that submit signals
 *   frontend  on its own thread, some time later: waits for that fence on
 *             the GPU, reads the texture, checks it holds frame n and
 *             not n+1 or n-3, and only then releases the index
 *
 * The frontend is deliberately slow and jittery, so the core laps it and
 * has to block in wait_sync_index. If the contract had a hole - the core
 * allowed back into a texture the frontend is still reading, or the
 * frontend reading before the core's work is done - a frame would read
 * as the wrong colour.
 *
 * The frontend below is the smallest thing that honours the contract; it
 * is not RetroArch's driver. What this proves is that the contract in
 * the header is enough, on a real D3D12 device.
 *
 * Windows only. run.sh builds it with mingw-w64 and runs it under Wine,
 * where vkd3d implements D3D12 over Vulkan. */
#ifndef CINTERFACE
#define CINTERFACE
#endif
#ifndef COBJMACROS
#define COBJMACROS
#endif
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <d3d12.h>
#include <libretro.h>
#include <libretro_d3d12.h>

#define SLOTS  3
#define FRAMES 240
#define W 64
#define H 64

/* ---- the frontend --------------------------------------------------- */
typedef struct
{
   ID3D12Device *device; ID3D12CommandQueue *queue;
   /* what the core handed over, per index */
   ID3D12Resource *tex[SLOTS]; ID3D12Fence *core_fence[SLOTS]; UINT64 core_value[SLOTS];
   unsigned frame_no[SLOTS];
   /* signalled when the frontend is done with an index */
   ID3D12Fence *done; UINT64 done_target[SLOTS]; HANDLE done_event;
   /* the "ring": indices published by the core, consumed by the thread */
   /* what was published travels with the frame, not with the index: a
    * core let back in too early overwrites the index, and must not be
    * able to overwrite what the frontend expects to find there */
   CRITICAL_SECTION lock; HANDLE have_work;
   struct { unsigned index, frame_no; UINT64 done_value; } pending[FRAMES + 8]; unsigned head, tail;
   unsigned index; unsigned next_frame_no; volatile LONG bad, shown, quit;
   /* the frontend thread's own objects */
   ID3D12CommandAllocator *alloc; ID3D12GraphicsCommandList *cmd; ID3D12Resource *readback;
   ID3D12Fence *read_fence; UINT64 read_value; HANDLE read_event;
   struct retro_hw_render_interface_d3d12 iface;
} frontend_t;

static unsigned fe_get_sync_index(void *h)      { return ((frontend_t*)h)->index; }
static unsigned fe_get_sync_index_mask(void *h) { (void)h; return (1u << SLOTS) - 1; }
static void fe_wait_sync_index(void *h)
{
   frontend_t *fe = (frontend_t*)h; UINT64 t = fe->done_target[fe->index];
   if (t && ID3D12Fence_GetCompletedValue(fe->done) < t)
   {
      ID3D12Fence_SetEventOnCompletion(fe->done, t, fe->done_event);
      WaitForSingleObject(fe->done_event, INFINITE);
   }
}
static void fe_set_texture_fenced(void *h, ID3D12Resource *tex, DXGI_FORMAT fmt, ID3D12Fence *fence, UINT64 value)
{
   frontend_t *fe = (frontend_t*)h; unsigned i = fe->index; (void)fmt;
   fe->tex[i] = tex; fe->core_fence[i] = fence; fe->core_value[i] = value;
}
/* video_refresh(RETRO_HW_FRAME_BUFFER_VALID): publish the index, move on.
 * No GPU work here: that is the point. */
static void fe_video_refresh(frontend_t *fe)
{
   static UINT64 done_counter;
   unsigned i = fe->index;
   fe->frame_no[i]    = fe->next_frame_no++;
   fe->done_target[i] = ++done_counter;
   EnterCriticalSection(&fe->lock);
   fe->pending[fe->tail].index      = i;
   fe->pending[fe->tail].frame_no   = fe->frame_no[i];
   fe->pending[fe->tail].done_value = fe->done_target[i];
   fe->tail++;
   LeaveCriticalSection(&fe->lock);
   SetEvent(fe->have_work);
   fe->index = (i + 1) % SLOTS;
}

static DWORD WINAPI frontend_thread(void *p)
{
   frontend_t *fe = (frontend_t*)p;
   for (;;)
   {
      unsigned i, want; UINT64 done_value; void *map = NULL; unsigned char *px; unsigned n;
      D3D12_RESOURCE_BARRIER b; D3D12_TEXTURE_COPY_LOCATION src, dst; D3D12_RANGE none = {0, 0};
      ID3D12CommandList *lists[1];

      EnterCriticalSection(&fe->lock);
      if (fe->head == fe->tail)
      {
         LeaveCriticalSection(&fe->lock);
         if (fe->quit)
            return 0;
         WaitForSingleObject(fe->have_work, 20);
         continue;
      }
      i          = fe->pending[fe->head].index;
      want       = fe->pending[fe->head].frame_no;
      done_value = fe->pending[fe->head].done_value;
      fe->head++;
      LeaveCriticalSection(&fe->lock);

      Sleep((fe->shown % 7) == 0 ? 15 : 5);           /* slower than the core, and unevenly so */

      /* not before the core's work is done - waited on the GPU, not here */
      if (fe->core_fence[i])
         ID3D12CommandQueue_Wait(fe->queue, fe->core_fence[i], fe->core_value[i]);

      ID3D12CommandAllocator_Reset(fe->alloc);
      ID3D12GraphicsCommandList_Reset(fe->cmd, fe->alloc, NULL);
      memset(&src, 0, sizeof(src)); memset(&dst, 0, sizeof(dst));
      src.pResource = fe->tex[i];   src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      dst.pResource = fe->readback; dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
      dst.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      dst.PlacedFootprint.Footprint.Width = W; dst.PlacedFootprint.Footprint.Height = H;
      dst.PlacedFootprint.Footprint.Depth = 1; dst.PlacedFootprint.Footprint.RowPitch = W * 4;
      ID3D12GraphicsCommandList_CopyTextureRegion(fe->cmd, &dst, 0, 0, 0, &src, NULL);
      (void)b;
      ID3D12GraphicsCommandList_Close(fe->cmd);
      lists[0] = (ID3D12CommandList*)fe->cmd;
      ID3D12CommandQueue_ExecuteCommandLists(fe->queue, 1, lists);
      ID3D12CommandQueue_Signal(fe->queue, fe->read_fence, ++fe->read_value);
      ID3D12Fence_SetEventOnCompletion(fe->read_fence, fe->read_value, fe->read_event);
      WaitForSingleObject(fe->read_event, INFINITE);

      ID3D12Resource_Map(fe->readback, 0, NULL, &map);
      px = (unsigned char*)map + (H / 2) * W * 4 + (W / 2) * 4;
      n  = px[0] | ((unsigned)px[1] << 8);
      if (n != want)
      {
         if (!fe->bad)
            printf("  FAIL: index %u should hold frame %u, holds frame %u\n", i, want, n);
         InterlockedExchange(&fe->bad, 1);
      }
      ID3D12Resource_Unmap(fe->readback, 0, &none);
      InterlockedIncrement(&fe->shown);

      /* done with index i: the core may have it back */
      ID3D12CommandQueue_Signal(fe->queue, fe->done, done_value);
   }
}

/* ---- the core ------------------------------------------------------- */
int main(int argc, char **argv)
{
   /* "nowait": a core that ignores wait_sync_index, to show the test can tell */
   int nowait = argc > 1 && !strcmp(argv[1], "nowait");
   frontend_t fe; const struct retro_hw_render_interface_d3d12 *d3d12;
   ID3D12Device *dev = NULL; ID3D12CommandQueue *queue = NULL; D3D12_COMMAND_QUEUE_DESC qd;
   ID3D12CommandAllocator *alloc[SLOTS]; ID3D12GraphicsCommandList *cmd; ID3D12Fence *fence; UINT64 value = 0;
   ID3D12DescriptorHeap *rtv_heap; D3D12_DESCRIPTOR_HEAP_DESC hd; D3D12_CPU_DESCRIPTOR_HANDLE rtv0; UINT rtv_step;
   ID3D12Resource *tex[SLOTS]; D3D12_HEAP_PROPERTIES hp; D3D12_RESOURCE_DESC rd; HANDLE thread; unsigned n, i;
   HANDLE core_event = CreateEvent(NULL, FALSE, FALSE, NULL); UINT64 alloc_value[SLOTS] = {0};

   setvbuf(stdout, NULL, _IONBF, 0);
   if (FAILED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void**)&dev)))
   { puts("no D3D12 device"); return 2; }
   memset(&qd, 0, sizeof(qd)); qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
   ID3D12Device_CreateCommandQueue(dev, &qd, &IID_ID3D12CommandQueue, (void**)&queue);

   memset(&fe, 0, sizeof(fe)); fe.device = dev; fe.queue = queue;
   InitializeCriticalSection(&fe.lock);
   fe.have_work = CreateEvent(NULL, FALSE, FALSE, NULL); fe.done_event = CreateEvent(NULL, FALSE, FALSE, NULL);
   fe.read_event = CreateEvent(NULL, FALSE, FALSE, NULL);
   ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**)&fe.done);
   ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**)&fe.read_fence);
   ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void**)&fe.alloc);
   ID3D12Device_CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, fe.alloc, NULL, &IID_ID3D12GraphicsCommandList, (void**)&fe.cmd);
   ID3D12GraphicsCommandList_Close(fe.cmd);
   memset(&hp, 0, sizeof(hp)); hp.Type = D3D12_HEAP_TYPE_READBACK;
   memset(&rd, 0, sizeof(rd)); rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; rd.Width = W * H * 4; rd.Height = 1;
   rd.DepthOrArraySize = 1; rd.MipLevels = 1; rd.SampleDesc.Count = 1; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
   ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, NULL, &IID_ID3D12Resource, (void**)&fe.readback);

   fe.iface.interface_type      = RETRO_HW_RENDER_INTERFACE_D3D12;
   fe.iface.interface_version   = RETRO_HW_RENDER_INTERFACE_D3D12_VERSION_2;
   fe.iface.handle              = &fe;
   fe.iface.device              = dev;
   fe.iface.queue               = queue;
   fe.iface.required_state      = D3D12_RESOURCE_STATE_COPY_SOURCE;
   fe.iface.get_sync_index      = fe_get_sync_index;
   fe.iface.get_sync_index_mask = fe_get_sync_index_mask;
   fe.iface.wait_sync_index     = fe_wait_sync_index;
   fe.iface.set_texture_fenced  = fe_set_texture_fenced;
   d3d12 = &fe.iface;
   thread = CreateThread(NULL, 0, frontend_thread, &fe, 0, NULL);

   /* the core's side: one present texture per bit in the mask */
   ID3D12Device_CreateFence(dev, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**)&fence);
   memset(&hd, 0, sizeof(hd)); hd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; hd.NumDescriptors = SLOTS;
   ID3D12Device_CreateDescriptorHeap(dev, &hd, &IID_ID3D12DescriptorHeap, (void**)&rtv_heap);
   /* struct-by-value in the C vtable: mingw declares the hidden-pointer form */
   rtv_heap->lpVtbl->GetCPUDescriptorHandleForHeapStart(rtv_heap, &rtv0);
   rtv_step = ID3D12Device_GetDescriptorHandleIncrementSize(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
   memset(&hp, 0, sizeof(hp)); hp.Type = D3D12_HEAP_TYPE_DEFAULT;
   memset(&rd, 0, sizeof(rd)); rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; rd.Width = W; rd.Height = H;
   rd.DepthOrArraySize = 1; rd.MipLevels = 1; rd.Format = DXGI_FORMAT_R8G8B8A8_UNORM; rd.SampleDesc.Count = 1;
   rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
   for (i = 0; i < SLOTS; i++)
   {
      D3D12_CPU_DESCRIPTOR_HANDLE hnd = rtv0; hnd.ptr += i * rtv_step;
      ID3D12Device_CreateCommittedResource(dev, &hp, D3D12_HEAP_FLAG_NONE, &rd, d3d12->required_state, NULL, &IID_ID3D12Resource, (void**)&tex[i]);
      ID3D12Device_CreateRenderTargetView(dev, tex[i], NULL, hnd);
      ID3D12Device_CreateCommandAllocator(dev, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void**)&alloc[i]);
   }
   ID3D12Device_CreateCommandList(dev, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, alloc[0], NULL, &IID_ID3D12GraphicsCommandList, (void**)&cmd);
   ID3D12GraphicsCommandList_Close(cmd);

   for (n = 0; n < FRAMES; n++)
   {
      D3D12_RESOURCE_BARRIER b; D3D12_CPU_DESCRIPTOR_HANDLE hnd = rtv0; ID3D12CommandList *lists[1];
      float col[4];
      i = d3d12->get_sync_index(d3d12->handle);
      if (!nowait)
         d3d12->wait_sync_index(d3d12->handle);
      if (alloc_value[i] && ID3D12Fence_GetCompletedValue(fence) < alloc_value[i])
      { ID3D12Fence_SetEventOnCompletion(fence, alloc_value[i], core_event); WaitForSingleObject(core_event, INFINITE); }

      hnd.ptr += i * rtv_step;
      col[0] = (float)(n & 0xff) / 255.0f; col[1] = (float)((n >> 8) & 0xff) / 255.0f; col[2] = 0.0f; col[3] = 1.0f;
      ID3D12CommandAllocator_Reset(alloc[i]);
      ID3D12GraphicsCommandList_Reset(cmd, alloc[i], NULL);
      memset(&b, 0, sizeof(b)); b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; b.Transition.pResource = tex[i];
      b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      b.Transition.StateBefore = d3d12->required_state; b.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
      ID3D12GraphicsCommandList_ResourceBarrier(cmd, 1, &b);
      ID3D12GraphicsCommandList_ClearRenderTargetView(cmd, hnd, col, 0, NULL);
      b.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET; b.Transition.StateAfter = d3d12->required_state;
      ID3D12GraphicsCommandList_ResourceBarrier(cmd, 1, &b);
      ID3D12GraphicsCommandList_Close(cmd);
      lists[0] = (ID3D12CommandList*)cmd;
      ID3D12CommandQueue_ExecuteCommandLists(queue, 1, lists);
      ID3D12CommandQueue_Signal(queue, fence, ++value);
      alloc_value[i] = value;

      d3d12->set_texture_fenced(d3d12->handle, tex[i], DXGI_FORMAT_R8G8B8A8_UNORM, fence, value);
      fe_video_refresh(&fe);
   }

   fe.quit = 1; SetEvent(fe.have_work);
   WaitForSingleObject(thread, 60000);
   printf("  %ld of %u frames reached the frontend\n", (long)fe.shown, FRAMES);
   if (fe.bad || fe.shown != FRAMES) { puts("d3d12_hw_interface_v2: FAILED"); return 1; }
   puts("d3d12_hw_interface_v2: ok");
   return 0;
}
