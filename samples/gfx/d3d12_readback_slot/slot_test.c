#include <assert.h>
typedef int bool;
#define true 1
#define false 0
#include <stdio.h>
#include <string.h>
#define D3D12_RECORD_RING 3
#define VIDEO_SHADER_STOCK_BLEND 0
struct object;
struct methods
{
   void (*Reset)(struct object*, ...);
   void (*Close)(struct object*);
   void (*ExecuteCommandLists)(struct object*, unsigned, void* const*);
};
struct object { struct methods *lpVtbl; };
typedef struct object *D3D12GraphicsCommandList;
typedef void ID3D12CommandList;
typedef struct
{
   struct { bool enable, streaming; unsigned index; } record;
   struct
   {
      D3D12GraphicsCommandList cmd, handle, allocator[1];
      unsigned slot;
   } queue;
   void *pipes[1];
} d3d12_video_t;
static unsigned captures, drains, cached, resets, closes, executes;
static void reset(struct object *o, ...) { (void)o; resets++; }
static void close_list(struct object *o) { (void)o; closes++; }
static void execute(struct object *o, unsigned n, void* const *lists)
{ (void)o; (void)lists; assert(n == 1); executes++; }
static void video_driver_cached_frame(void) { cached++; }
static void d3d12_queue_drain(d3d12_video_t *d) { (void)d; drains++; }
static void d3d12_record_capture(d3d12_video_t *d, D3D12GraphicsCommandList c)
{ (void)c; captures++; d->record.enable = true; }
static void d3d12_record_signal(d3d12_video_t *d)
{ d->record.index = (d->record.index + 1) % D3D12_RECORD_RING; }
#include "selection.h"
int main(void)
{
   struct methods methods = { reset, close_list, execute };
   struct object object;
   d3d12_video_t d;
   unsigned i;
   memset(&d, 0, sizeof(d));
   object.lpVtbl = &methods;
   d.queue.cmd = d.queue.handle = d.queue.allocator[0] = &object;
   for (i = 0; i < 8; i++)
   {
      assert(select_slot(&d, true) == i % D3D12_RECORD_RING);
      assert(captures == i + 1);
   }
   assert(drains == 16 && resets == 16 && closes == 8 && executes == 8);
   d.record.streaming = true;
   for (i = 0; i < D3D12_RECORD_RING; i++)
   {
      d.record.index = i;
      assert(select_slot(&d, false) == i);
   }
   assert(captures == 8 && drains == 16 && cached == 0);
   d.record.streaming = false;
   assert(select_slot(&d, false) == 2);
   assert(captures == 9 && drains == 18 && cached == 1);
   puts("PASS repeated screenshots and nonblocking recording slots");
   return 0;
}
