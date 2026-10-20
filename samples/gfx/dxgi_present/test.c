#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int HRESULT;
typedef unsigned DWORD;
#define S_OK 0
typedef struct { int64_t QuadPart; } LARGE_INTEGER;
typedef struct { unsigned PresentCount, PresentRefreshCount, SyncRefreshCount; LARGE_INTEGER SyncQPCTime, SyncGPUTime; } DXGI_FRAME_STATISTICS;
typedef uint64_t UINT64;
#define WAIT_OBJECT_0 0u
#define WAIT_TIMEOUT 258u
#define WAIT_FAILED (~0u)
#define DXGI_PRESENT_TEST 1u
#define DXGI_STATUS_OCCLUDED 2
#define DXGI_PRESENT_ALLOW_TEARING 512u
#define FAILED(value) ((value) < 0)
#define SUCCEEDED(value) ((value) >= 0)

typedef struct queue ID3D12CommandQueue;
typedef struct chain IDXGISwapChain3;
typedef struct fence ID3D12Fence;
typedef void ID3D12CommandList;
struct queue_vtable {
   HRESULT (*Wait)(ID3D12CommandQueue *, void *, UINT64);
   void (*ExecuteCommandLists)(ID3D12CommandQueue *, unsigned, ID3D12CommandList **);
   HRESULT (*Signal)(ID3D12CommandQueue *, void *, UINT64);
};
struct chain_vtable {
   HRESULT (*Present)(IDXGISwapChain3 *, unsigned, unsigned);
   unsigned (*GetCurrentBackBufferIndex)(IDXGISwapChain3 *);
   HRESULT (*GetLastPresentCount)(IDXGISwapChain3 *, unsigned *);
   HRESULT (*GetFrameStatistics)(IDXGISwapChain3 *, DXGI_FRAME_STATISTICS *);
};
struct queue { struct queue_vtable *lpVtbl; };
struct chain { struct chain_vtable *lpVtbl; };
struct fence_vtable {
   UINT64 (*GetCompletedValue)(ID3D12Fence *);
   HRESULT (*SetEventOnCompletion)(ID3D12Fence *, UINT64, void *);
};
struct fence { struct fence_vtable *lpVtbl; };
typedef struct ra_dxgi_bridge {
   ID3D12CommandQueue *queue;
   IDXGISwapChain3 *chain;
   void *ready, *latency, *event, *device, *lists[2];
   ID3D12Fence *done;
   unsigned current_index, present_count;
   int64_t present_submitted_qpc;
   int failed, tearing, occluded, resized;
   UINT64 sequence, last_use[2];
} ra_dxgi_bridge_t;

static DWORD wait_result;
static HRESULT probe_result, present_result, queue_result, signal_result;
static unsigned waits, probes, copies, presents, order;
static UINT64 completed;
static UINT64 get_completed(ID3D12Fence *fence)
{ (void)fence; return completed; }
static UINT64 event_value;
static unsigned event_sets;
static HRESULT event_result;
static HRESULT set_event(ID3D12Fence *fence, UINT64 value, void *event)
{
   (void)fence; assert(event == (void *)&event_value);
   event_value = value; ++event_sets; return event_result;
}
static unsigned expected_interval = 1, expected_flags;
static HRESULT count_result, stats_result;
static DXGI_FRAME_STATISTICS frame_stats;
static int frequency_valid = 1, clock_valid = 1;
static int64_t clock_time = 998000;
static int QueryPerformanceFrequency(LARGE_INTEGER *value)
{ value->QuadPart = 1000000; return frequency_valid; }
static int QueryPerformanceCounter(LARGE_INTEGER *value)
{ value->QuadPart = clock_time; return clock_valid; }
static HRESULT get_count(IDXGISwapChain3 *chain, unsigned *count)
{ (void)chain; *count = presents; return count_result; }
static HRESULT get_stats(IDXGISwapChain3 *chain, DXGI_FRAME_STATISTICS *stats)
{ (void)chain; *stats = frame_stats; return stats_result; }
static UINT64 signalled, waited;
static DWORD WaitForSingleObject(void *handle, unsigned timeout)
{
   (void)handle; assert(timeout == 1000); ++waits; return wait_result;
}
static HRESULT queue_wait(ID3D12CommandQueue *queue, void *fence, UINT64 value)
{
   (void)queue; (void)fence; assert(order == 0); order = 1;
   waited = value; return queue_result;
}
static void copy(ID3D12CommandQueue *queue, unsigned count, ID3D12CommandList **list)
{
   (void)queue; (void)list; assert(order == 1 && count == 1); order = 2; ++copies;
}
static HRESULT signal_done(ID3D12CommandQueue *queue, void *fence, UINT64 value)
{
   (void)queue; (void)fence; assert(order == 2); order = 3;
   signalled = value; return signal_result;
}
static HRESULT present(IDXGISwapChain3 *chain, unsigned interval, unsigned flags)
{
   (void)chain;
   if (flags == DXGI_PRESENT_TEST) { ++probes; return probe_result; }
   assert(order == 3 && interval == expected_interval && flags == expected_flags); order = 4; ++presents;
   return present_result;
}
static unsigned current(IDXGISwapChain3 *chain) { (void)chain; return 1; }
#include "present.inc"

int main(void)
{
   struct queue_vtable queue_methods = { queue_wait, copy, signal_done };
   struct chain_vtable chain_methods = { present, current, get_count, get_stats };
   struct fence_vtable fence_methods = { get_completed, set_event };
   ID3D12Fence fence;
   ID3D12CommandQueue queue;
   IDXGISwapChain3 chain;
   ra_dxgi_bridge_t bridge;
   queue.lpVtbl = &queue_methods; chain.lpVtbl = &chain_methods;
   memset(&bridge, 0, sizeof(bridge)); bridge.queue = &queue; bridge.chain = &chain;
   fence.lpVtbl = &fence_methods; bridge.done = &fence;
   bridge.event = &event_value;
   assert(ra_dxgi_bridge_presentable(&bridge));
   assert(ra_dxgi_bridge_index(&bridge, 1) == 1 && waits == 1 && probes == 0);
   assert(ra_dxgi_bridge_present(&bridge, 7, 1));
   assert(order == 4 && copies == 1 && presents == 1 && waited == 7 && signalled == 1);
   assert(ra_dxgi_bridge_last_use(&bridge, 1) == 1 && bridge.sequence == 1);
   {
      unsigned i, before = waits;
      /* A target the GPU is still copying into is a busy window, not a
       * missing one: presentable() is what the runloop reads as "no
       * window" and sleeps a whole frame for, so it must stay true, and
       * must not wait or probe for it. */
      for (i = 0; i < 10000; i++)
         assert(ra_dxgi_bridge_presentable(&bridge));
      assert(waits == before && presents == 1 && copies == 1 && probes == 0);
      assert(event_sets == 0 && !bridge.failed);
      /* The wait for it is the producer's, at the target: armed on the
       * target's last use, and over when that completes. */
      assert(ra_dxgi_bridge_wait_target(&bridge, 1));
      assert(event_sets == 1 && event_value == 1 && waits == before + 1);
      wait_result = WAIT_TIMEOUT;
      assert(!ra_dxgi_bridge_wait_target(&bridge, 1) && !bridge.failed);
      wait_result = WAIT_OBJECT_0;
      event_result = -1;
      assert(!ra_dxgi_bridge_wait_target(&bridge, 1) && bridge.failed);
      bridge.failed = 0; event_result = 0;
      /* Free: one fence read, no event, no wait. */
      completed = 1; before = waits;
      assert(ra_dxgi_bridge_wait_target(&bridge, 1));
      assert(ra_dxgi_bridge_wait_target(&bridge, 0));
      assert(waits == before && event_sets == 3);
      assert(!ra_dxgi_bridge_wait_target(&bridge, 2));
      assert(ra_dxgi_bridge_presentable(&bridge));
      /* A removed device is not waited on, and reaches the fallback. */
      completed = ~(UINT64)0;
      assert(!ra_dxgi_bridge_wait_target(&bridge, 1) && bridge.failed);
      assert(ra_dxgi_bridge_presentable(&bridge) && waits == before);
      bridge.failed = 0; completed = 1;
      /* The lanes below count the frame-latency waits from here. */
      waits = 1;
   }
   probe_result = DXGI_STATUS_OCCLUDED;
   assert(ra_dxgi_bridge_index(&bridge, 1) == ~0u && waits == 1);
   assert(!ra_dxgi_bridge_failed(&bridge));
   assert(!ra_dxgi_bridge_presentable(&bridge));
   probe_result = 0;
   assert(ra_dxgi_bridge_presentable(&bridge));
   probe_result = 0; wait_result = WAIT_TIMEOUT;
   assert(ra_dxgi_bridge_index(&bridge, 1) == ~0u && !ra_dxgi_bridge_failed(&bridge));
   bridge.resized = 1;
   assert(ra_dxgi_bridge_index(&bridge, 1) == 1 && !bridge.resized);
   assert(ra_dxgi_bridge_index(&bridge, 1) == ~0u && !ra_dxgi_bridge_failed(&bridge));
   wait_result = WAIT_FAILED;
   assert(ra_dxgi_bridge_index(&bridge, 1) == ~0u && ra_dxgi_bridge_failed(&bridge));
   wait_result = WAIT_OBJECT_0; bridge.failed = 0; probe_result = -1;
   assert(ra_dxgi_bridge_index(&bridge, 1) == ~0u && ra_dxgi_bridge_failed(&bridge));
   probe_result = 0; bridge.failed = 0; bridge.tearing = 1;
   wait_result = WAIT_FAILED;
   {
      unsigned before = waits;
      assert(ra_dxgi_bridge_index(&bridge, 0) == 1 && waits == before);
      assert(!ra_dxgi_bridge_failed(&bridge));
      assert(ra_dxgi_bridge_index(&bridge, 1) == ~0u && waits == before + 1);
      assert(ra_dxgi_bridge_failed(&bridge));
      bridge.failed = 0; bridge.tearing = 0;
      assert(ra_dxgi_bridge_index(&bridge, 0) == ~0u && waits == before + 2);
      assert(ra_dxgi_bridge_failed(&bridge));
   }
   wait_result = WAIT_OBJECT_0; bridge.failed = 0;
   order = 0; queue_result = -1;
   assert(!ra_dxgi_bridge_present(&bridge, 8, 1) && copies == 1 && bridge.sequence == 1);
   order = 0; queue_result = 0; signal_result = -1;
   assert(!ra_dxgi_bridge_present(&bridge, 8, 1) && presents == 1 && bridge.sequence == 1);
   order = 0; signal_result = 0; present_result = -1;
   assert(!ra_dxgi_bridge_present(&bridge, 9, 1) && bridge.sequence == 2);
   assert(bridge.last_use[1] == 2);
   order = 0; present_result = 0; expected_interval = 0;
   assert(ra_dxgi_bridge_present(&bridge, 10, 0));
   order = 0; bridge.tearing = 1; expected_flags = DXGI_PRESENT_ALLOW_TEARING;
   assert(ra_dxgi_bridge_present(&bridge, 11, 0));
   order = 0; expected_interval = 4; expected_flags = 0;
   assert(ra_dxgi_bridge_present(&bridge, 12, 10));
   clock_time = 1000000;
   frame_stats.PresentCount = bridge.present_count;
   frame_stats.PresentRefreshCount = frame_stats.SyncRefreshCount = 100;
   frame_stats.SyncQPCTime.QuadPart = 999000;
   assert(ra_dxgi_bridge_present_time(&bridge) == 999000);
   frame_stats.SyncQPCTime.QuadPart = bridge.present_submitted_qpc - 1;
   assert(ra_dxgi_bridge_present_time(&bridge) == 0);
   frame_stats.SyncQPCTime.QuadPart = 999000;
   frame_stats.PresentCount--;
   assert(ra_dxgi_bridge_present_time(&bridge) == 0);
   frame_stats.PresentCount++;
   frame_stats.PresentRefreshCount++;
   assert(ra_dxgi_bridge_present_time(&bridge) == 0);
   frame_stats.PresentRefreshCount--;
   stats_result = -1;
   assert(ra_dxgi_bridge_present_time(&bridge) == 0);
   stats_result = 0; frame_stats.SyncQPCTime.QuadPart = 1000001;
   assert(ra_dxgi_bridge_present_time(&bridge) == 0);
   frame_stats.SyncQPCTime.QuadPart = 0;
   assert(ra_dxgi_bridge_present_time(&bridge) == 0);
   frame_stats.SyncQPCTime.QuadPart = 999000; frequency_valid = 0;
   assert(ra_dxgi_bridge_present_time(&bridge) == 0);
   frequency_valid = 1; clock_valid = 0;
   assert(ra_dxgi_bridge_present_time(&bridge) == 0);
   clock_valid = 1;
   order = 0; count_result = -1;
   assert(ra_dxgi_bridge_present(&bridge, 13, 10));
   assert(ra_dxgi_bridge_present_time(&bridge) == 0);
   count_result = 0; order = 0; expected_interval = 0;
   expected_flags = DXGI_PRESENT_ALLOW_TEARING;
   assert(ra_dxgi_bridge_present(&bridge, 14, 0));
   assert(ra_dxgi_bridge_present_time(&bridge) == 0);
   puts("PASS: DXGI queue ordering, fence lifetime, pacing, occlusion and failures");
   return 0;
}
