/* The D3D12 driver's frame ring (gfx/drivers/d3d12.c, extracted by
 * run.py) against a fake queue: the GPU finishes only what a wait asks
 * of it, so every wait the ring makes shows as the GPU work it forces
 * done, and every allocator reset and slot taken is checked against
 * the lists still in flight. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned UINT;
typedef unsigned long long UINT64;
typedef long HRESULT;
typedef UINT64 D3D12_GPU_VIRTUAL_ADDRESS;
typedef void *D3D12PipelineState;
#define FAILED(hr) ((hr) < 0)
typedef struct { size_t Begin, End; } D3D12_RANGE;
typedef struct
{
   D3D12_GPU_VIRTUAL_ADDRESS BufferLocation;
   UINT SizeInBytes;
} D3D12_CONSTANT_BUFFER_VIEW_DESC;

/* The fake GPU: what has completed, what was signalled last */
static UINT64 completed, signalled;
static unsigned waits;

typedef struct resource resource_t;
struct resource
{
   struct resource_vtbl
   {
      HRESULT (*Map)(resource_t*, UINT, const D3D12_RANGE*, void**);
      void (*Unmap)(resource_t*, UINT, const D3D12_RANGE*);
   } *lpVtbl;
   uint8_t data[1024];
};
typedef resource_t *D3D12Resource;
static HRESULT res_map(resource_t *r, UINT s, const D3D12_RANGE *n, void **p)
{
   (void)s; (void)n;
   *p = r->data;
   return 0;
}
static void res_unmap(resource_t *r, UINT s, const D3D12_RANGE *n)
{
   (void)r; (void)s; (void)n;
}
static struct resource_vtbl res_vtbl = { res_map, res_unmap };

/* An allocator remembers the last signal behind a list recorded from
 * it: resetting it before the GPU passes that is the device error the
 * ring must not make */
typedef struct allocator allocator_t;
struct allocator
{
   struct allocator_vtbl { HRESULT (*Reset)(allocator_t*); } *lpVtbl;
   UINT64 last_signal;
   unsigned resets;
   bool recorded;
};
typedef allocator_t *D3D12CommandAllocator;
static HRESULT alloc_reset(allocator_t *a)
{
   assert(completed >= a->last_signal);
   a->resets++;
   return 0;
}
static struct allocator_vtbl alloc_vtbl = { alloc_reset };

typedef struct cmdlist cmdlist_t;
struct cmdlist
{
   struct cmd_vtbl
   {
      HRESULT (*Reset)(cmdlist_t*, allocator_t*, D3D12PipelineState);
      HRESULT (*Close)(cmdlist_t*);
   } *lpVtbl;
   allocator_t *allocator;
};
typedef cmdlist_t *D3D12GraphicsCommandList;
typedef cmdlist_t ID3D12CommandList;
static HRESULT cmd_reset(cmdlist_t *c, allocator_t *a, D3D12PipelineState p)
{
   (void)p;
   c->allocator = a;
   return 0;
}
static HRESULT cmd_close(cmdlist_t *c) { (void)c; return 0; }
static struct cmd_vtbl cmd_vtbl = { cmd_reset, cmd_close };

typedef struct fence fence_t;
struct fence
{
   struct fence_vtbl { UINT64 (*GetCompletedValue)(fence_t*); } *lpVtbl;
};
typedef fence_t *D3D12Fence;
static UINT64 fence_completed(fence_t *f) { (void)f; return completed; }
static struct fence_vtbl fence_vtbl = { fence_completed };

typedef struct queue queue_t;
static allocator_t *executing;
struct queue
{
   struct queue_vtbl
   {
      void (*ExecuteCommandLists)(queue_t*, UINT, ID3D12CommandList* const*);
      HRESULT (*Signal)(queue_t*, fence_t*, UINT64);
   } *lpVtbl;
};
typedef queue_t *D3D12CommandQueue;
static void queue_execute(queue_t *q, UINT n, ID3D12CommandList* const *l)
{
   (void)q; (void)n;
   executing = l[0]->allocator;
}
static HRESULT queue_signal(queue_t *q, fence_t *f, UINT64 v)
{
   (void)q; (void)f;
   assert(v > signalled);
   signalled = v;
   if (executing)
      executing->last_signal = v;
   executing = NULL;
   return 0;
}
static struct queue_vtbl queue_vtbl = { queue_execute, queue_signal };

typedef struct { float mvp[4]; float time, alpha; } d3d12_uniform_t;
typedef struct { size_t ptr; } D3D12_CPU_DESCRIPTOR_HANDLE;
typedef struct
{
   D3D12_CPU_DESCRIPTOR_HANDLE cpu;
   unsigned stride;
   int start;
   bool map[64];
} d3d12_descriptor_heap_t;
typedef struct
{
   D3D12_CPU_DESCRIPTOR_HANDLE cpu_uav, cpu_src;
   D3D12Resource planes;
} d3d12_planar_t;
typedef struct
{
   D3D12Resource handle, upload_buffer, upload_buffer2;
   d3d12_descriptor_heap_t *srv_heap;
   struct { unsigned Width; unsigned short MipLevels; } desc;
   D3D12_CPU_DESCRIPTOR_HANDLE cpu_descriptor[4];
   d3d12_planar_t *planar;
   uint8_t *lend_mapped[2];
   UINT64 upload_fence, lend_fence[2];
   unsigned lent, pending;
   bool dirty;
} d3d12_texture_t;
#define countof(a) (sizeof(a) / sizeof((a)[0]))
/* What Release() let go of */
static unsigned released;
#define Release(p) do { if (p) { released++; (p) = NULL; } } while (0)
typedef struct
{
   struct
   {
      D3D12CommandQueue handle;
      D3D12CommandAllocator allocator[2];
      D3D12GraphicsCommandList cmd;
      D3D12Fence fence;
      UINT64 fenceValue, frameSignal, slotSignal[2];
      unsigned slot, depth;
   } queue;
   struct { int offset, capacity; } sprites;
   D3D12Resource ubo;
   D3D12_CONSTANT_BUFFER_VIEW_DESC ubo_view;
   d3d12_uniform_t ubo_written;
   unsigned ubo_stale;
} d3d12_video_t;

/* The GPU finishes what the wait is for, and no more */
static void d3d12_fence_wait(d3d12_video_t *d3d12, UINT64 value)
{
   (void)d3d12;
   if (completed >= value)
      return;
   assert(value <= signalled);
   completed = value;
   waits++;
}

static resource_t second_buffer;
static bool second_fails;
static bool d3d12_texture_upload_buffer2(d3d12_video_t *d3d12,
      d3d12_texture_t *texture)
{
   (void)d3d12;
   if (!texture->upload_buffer2 && !second_fails)
      texture->upload_buffer2 = &second_buffer;
   return texture->upload_buffer2 != NULL;
}

/* Retired behind @fence, as d3d12_retire() keeps them */
static d3d12_texture_t *retired_texture;
static UINT64 retired_fence;
static void d3d12_retire(d3d12_video_t *d3d12, d3d12_texture_t *texture,
      void *object, UINT64 fence)
{
   (void)d3d12; (void)object;
   assert(!retired_texture);
   retired_texture = texture;
   retired_fence   = fence;
}
typedef void IUnknown;

#include "ring.inc"

static d3d12_video_t d3d12;
static allocator_t allocators[2];
static cmdlist_t cmd;
static fence_t fence;
static queue_t queue;
static resource_t ubo;

/* A frame as d3d12_gfx_frame_body() records one: its slot, its list,
 * then @extra lists of its own (black frames, repeats) */
static void frame(unsigned depth, unsigned extra)
{
   d3d12.queue.depth = depth;
   d3d12_slot_begin(&d3d12);
   d3d12_cmd_begin(&d3d12, NULL);
   d3d12_cmd_execute(&d3d12);
   while (extra--)
   {
      d3d12_cmd_begin(&d3d12, NULL);
      d3d12_cmd_execute(&d3d12);
   }
}

static void setup(void)
{
   memset(&d3d12, 0, sizeof(d3d12));
   memset(allocators, 0, sizeof(allocators));
   completed = signalled = 0;
   waits = 0;
   allocators[0].lpVtbl = allocators[1].lpVtbl = &alloc_vtbl;
   cmd.lpVtbl   = &cmd_vtbl;
   fence.lpVtbl = &fence_vtbl;
   queue.lpVtbl = &queue_vtbl;
   ubo.lpVtbl   = &res_vtbl;
   memset(ubo.data, 0, sizeof(ubo.data));
   d3d12.queue.handle       = &queue;
   d3d12.queue.allocator[0] = &allocators[0];
   d3d12.queue.allocator[1] = &allocators[1];
   d3d12.queue.cmd          = &cmd;
   d3d12.queue.fence        = &fence;
   d3d12.sprites.capacity   = 100;
   d3d12.ubo                = &ubo;
   d3d12.ubo_view.SizeInBytes    = sizeof(d3d12_uniform_t);
   d3d12.ubo_view.BufferLocation = 0x10000;
}

int main(void)
{
   unsigned i;

   /* One frame in flight, as without Driver Pacing: every frame, and
    * every list after a frame's own, waits for the last list. */
   setup();
   for (i = 0; i < 6; i++)
   {
      UINT64 last = d3d12.queue.frameSignal;
      frame(1, i & 1);
      assert(completed >= last);
      assert(d3d12.queue.slot == (i + 1) % D3D12_FRAMES);
      assert(d3d12.sprites.offset == (int)d3d12.queue.slot * 100);
   }
   assert(completed == signalled - 1);
   /* The allocators are reset with every list, as one was */
   assert(allocators[0].resets + allocators[1].resets == 9);

   /* Two in flight, under Driver Pacing: a frame waits only for the
    * frame before the last, and the last stays the GPU's. */
   setup();
   frame(2, 0);
   frame(2, 0);
   assert(waits == 0 && completed == 0 && signalled == 2);
   for (i = 0; i < 6; i++)
   {
      UINT64 last = d3d12.queue.frameSignal;
      frame(2, 0);
      assert(completed == last - 1);
      assert(completed < d3d12.queue.frameSignal - 1);
   }
   /* A black frame or a repeat follows its frame without waiting for
    * it, the allocator kept until the slot comes round again */
   {
      unsigned w = waits;
      UINT64 c   = completed;
      unsigned r = allocators[d3d12.queue.slot].resets;
      d3d12_cmd_begin(&d3d12, NULL);
      d3d12_cmd_execute(&d3d12);
      d3d12_cmd_begin(&d3d12, NULL);
      d3d12_cmd_execute(&d3d12);
      assert(waits == w && completed == c);
      assert(allocators[d3d12.queue.slot].resets == r);
      /* the slot's next frame waits for all of them */
      frame(2, 0);
      frame(2, 0);
      assert(completed == signalled - 2);
   }
   /* What a frame lets go of or changes in place waits for all */
   d3d12_queue_settle(&d3d12);
   assert(completed == signalled);
   frame(2, 0);
   frame(2, 0);
   /* Back to one: everything before is waited for */
   frame(1, 0);
   assert(completed == signalled - 1);

   /* The constants draws bind without writing: a slot taken again has
    * the last written, written once */
   setup();
   frame(1, 0);
   {
      d3d12_uniform_t u;
      memset(&u, 0, sizeof(u));
      u.alpha              = 0.5f;
      d3d12.ubo_written    = u;
      d3d12.ubo_stale      = 3u & ~(1u << d3d12.queue.slot);
      assert(d3d12_cb_write_slot(d3d12.ubo, &d3d12.ubo_view,
               d3d12.queue.slot, &u, sizeof(u)));
      frame(2, 0);
      assert(d3d12.ubo_stale == 0);
      assert(!memcmp(ubo.data + D3D12_CB_STRIDE(sizeof(u)), &u, sizeof(u)));
      assert(!memcmp(ubo.data, &u, sizeof(u)));
      assert(d3d12_cb_slot(&d3d12.ubo_view, 1) == 0x10000 + 256);
   }

   /* An upload buffer still read by a frame in flight */
   {
      resource_t first;
      d3d12_texture_t t;
      setup();
      frame(1, 0);
      frame(1, 0);
      memset(&t, 0, sizeof(t));
      t.upload_buffer = &first;
      /* the last frame copied from it, still running */
      t.upload_fence  = signalled;
      t.lend_fence[0] = signalled;
      completed       = signalled - 1;
      /* one frame in flight: dropped, or waited for, as before */
      d3d12.queue.depth = 1;
      assert(!d3d12_texture_upload_turn(&d3d12, &t, false));
      assert(t.upload_buffer == &first && !t.upload_buffer2);
      waits = 0;
      assert(d3d12_texture_upload_turn(&d3d12, &t, true));
      assert(waits == 1 && completed == signalled && t.upload_buffer == &first);
      /* two: the second buffer takes its place, nothing waited for */
      completed         = signalled - 1;
      d3d12.queue.depth = 2;
      waits             = 0;
      assert(d3d12_texture_upload_turn(&d3d12, &t, false));
      assert(waits == 0 && t.upload_buffer == &second_buffer);
      assert(t.upload_buffer2 == &first && t.lend_fence[1] == signalled);
      /* written, not yet copied: the same buffer again */
      t.dirty = true;
      assert(d3d12_texture_upload_turn(&d3d12, &t, false));
      assert(t.upload_buffer == &second_buffer);
      /* copied by a frame in flight too: both busy */
      t.dirty         = false;
      t.upload_fence  = signalled;
      assert(!d3d12_texture_upload_turn(&d3d12, &t, false));
      assert(t.upload_buffer == &second_buffer);
      assert(d3d12_texture_upload_turn(&d3d12, &t, true));
      assert(waits == 1 && completed == signalled);
      /* lent buffers are their producer's */
      t.lent         = 1;
      t.upload_fence = signalled + 1;
      assert(d3d12_texture_upload_turn(&d3d12, &t, false));
      /* no second buffer to be had */
      memset(&t, 0, sizeof(t));
      t.upload_buffer = &first;
      t.upload_fence  = signalled;
      completed       = signalled - 1;
      second_fails    = true;
      assert(!d3d12_texture_upload_turn(&d3d12, &t, false));
      second_fails    = false;
   }
   /* A texture let go while a frame in flight may draw from it goes
    * behind the frame being recorded, its views kept until then and
    * its description left for the texture made next; with nothing in
    * flight, at once, as before */
   {
      static resource_t res[2];
      static d3d12_descriptor_heap_t heap;
      d3d12_texture_t t;
      setup();
      frame(2, 0);
      frame(2, 0);
      memset(&heap, 0, sizeof(heap));
      heap.cpu.ptr = 1000;
      heap.stride  = 10;
      heap.map[3]  = true;
      memset(&t, 0, sizeof(t));
      t.handle            = &res[0];
      t.upload_buffer     = &res[1];
      t.srv_heap          = &heap;
      t.desc.Width        = 64;
      t.desc.MipLevels    = 1;
      t.cpu_descriptor[0].ptr = 1030;
      completed           = signalled - 1;
      released            = 0;
      d3d12_release_texture(&d3d12, &t);
      assert(retired_texture && retired_fence == d3d12.queue.fenceValue + 1);
      assert(!released && heap.map[3]);
      assert(!t.handle && !t.upload_buffer && !t.cpu_descriptor[0].ptr);
      assert(t.desc.Width == 64);
      assert(retired_texture->handle == &res[0]
            && retired_texture->cpu_descriptor[0].ptr == 1030);
      /* its fence passed, it goes for good */
      completed = signalled;
      d3d12_release_texture(NULL, retired_texture);
      assert(released == 2 && !heap.map[3]);
      free(retired_texture);
      retired_texture = NULL;
      /* nothing in flight: at once */
      t.handle                = &res[0];
      t.cpu_descriptor[0].ptr = 1030;
      heap.map[3]             = true;
      released                = 0;
      d3d12_release_texture(&d3d12, &t);
      assert(!retired_texture && released == 1 && !heap.map[3] && !t.handle);
   }
   puts("D3D12 frame ring: one frame in flight as before, two under"
         " Driver Pacing, slot constants, upload turns and textures let"
         " go behind the frame PASS");
   return 0;
}
