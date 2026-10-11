#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint64_t UINT64;
typedef int HRESULT;
#define SUCCEEDED(value) ((value) >= 0)
#define INFINITE (~0u)
typedef struct object object;
struct methods {
   unsigned (*Release)(object *);
   UINT64 (*GetCompletedValue)(object *);
   HRESULT (*SetEventOnCompletion)(object *, UINT64, void *);
   HRESULT (*Signal)(object *, object *, UINT64);
};
struct object { struct methods *lpVtbl; };
typedef struct ra_dxgi_bridge {
   object *queue, *done, *chain, *ready, *device, *factory;
   object *lists[2], *allocators[2], *buffers[2], *shared[2];
   void *resources[2], *ready_handle, *done_handle, *event, *latency;
   void *d3d_dll, *dxgi_dll;
   UINT64 sequence;
} ra_dxgi_bridge_t;
static UINT64 completed, expected;
static unsigned signals, events, waits, releases;
static int drained, require_drain;
static HRESULT signal_result, event_result;
static unsigned release(object *p)
{
   (void)p;
   assert(!require_drain || drained);
   ++releases;
   return 0;
}
static UINT64 get_completed(object *p) { (void)p; return completed; }
static HRESULT set_event(object *p, UINT64 value, void *event)
{
   (void)p; assert(event && value == expected && signals); ++events;
   return event_result;
}
static HRESULT signal_queue(object *p, object *fence, UINT64 value)
{
   (void)p; assert(fence && value == expected && !releases); ++signals;
   if (completed >= value) drained = 1;
   return signal_result;
}
static unsigned WaitForSingleObject(void *event, unsigned timeout)
{
   assert(event && timeout == INFINITE && events && !releases);
   drained = 1; ++waits; return 0;
}
static int CloseHandle(void *handle) { assert(handle); return 1; }
static int FreeLibrary(void *module) { assert(module); return 1; }
#define DROP(p) do { if (p) { (p)->lpVtbl->Release(p); (p) = NULL; } } while (0)
#include "destroy.inc"

static ra_dxgi_bridge_t *make_bridge(object *p, UINT64 sequence)
{
   unsigned i;
   ra_dxgi_bridge_t *b = (ra_dxgi_bridge_t *)calloc(1, sizeof(*b));
   assert(b);
   b->queue = b->done = b->ready = b->chain = b->device = b->factory = p;
   for (i = 0; i < 2; i++)
      b->lists[i] = b->allocators[i] = b->buffers[i] = b->shared[i] = p;
   b->event = b;
   b->sequence = sequence;
   expected = sequence + 1;
   completed = sequence;
   signals = events = waits = releases = 0;
   drained = 0; require_drain = 1;
   signal_result = event_result = 0;
   return b;
}
int main(void)
{
   struct methods methods = { release, get_completed, set_event, signal_queue };
   object p;
   ra_dxgi_bridge_t *b;
   p.lpVtbl = &methods;
   /* A completed copy fence does not cover the following Present. */
   b = make_bridge(&p, 7);
   ra_dxgi_bridge_destroy(b);
   assert(signals == 1 && events == 1 && waits == 1 && releases == 14);
   /* Drain even if copy submission failed before updating sequence. */
   b = make_bridge(&p, 0);
   ra_dxgi_bridge_destroy(b);
   assert(signals == 1 && waits == 1);
   b = make_bridge(&p, 7); completed = expected;
   ra_dxgi_bridge_destroy(b);
   assert(signals == 1 && events == 0 && waits == 0);
   b = make_bridge(&p, 7); signal_result = -1; require_drain = 0;
   ra_dxgi_bridge_destroy(b);
   assert(signals == 1 && events == 0 && waits == 0);
   b = make_bridge(&p, 7); event_result = -1; require_drain = 0;
   ra_dxgi_bridge_destroy(b);
   assert(signals == 1 && events == 1 && waits == 0);
   b = make_bridge(&p, 0); b->done = NULL; require_drain = 0;
   ra_dxgi_bridge_destroy(b);
   assert(signals == 0 && waits == 0);
   b = make_bridge(&p, 0); b->queue = NULL; require_drain = 0;
   ra_dxgi_bridge_destroy(b);
   assert(signals == 0 && waits == 0);
   b = make_bridge(&p, 0); b->event = NULL; require_drain = 0;
   ra_dxgi_bridge_destroy(b);
   assert(signals == 0 && waits == 0);
   ra_dxgi_bridge_destroy(NULL);
   puts("PASS: DXGI teardown drains presentation before releasing resources");
   return 0;
}
