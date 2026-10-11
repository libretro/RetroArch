#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef int HRESULT;
typedef unsigned DWORD;
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
typedef void ID3D12CommandList;
struct queue_vtable {
   HRESULT (*Wait)(ID3D12CommandQueue *, void *, UINT64);
   void (*ExecuteCommandLists)(ID3D12CommandQueue *, unsigned, ID3D12CommandList **);
   HRESULT (*Signal)(ID3D12CommandQueue *, void *, UINT64);
};
struct chain_vtable {
   HRESULT (*Present)(IDXGISwapChain3 *, unsigned, unsigned);
   unsigned (*GetCurrentBackBufferIndex)(IDXGISwapChain3 *);
};
struct queue { struct queue_vtable *lpVtbl; };
struct chain { struct chain_vtable *lpVtbl; };
typedef struct ra_dxgi_bridge {
   ID3D12CommandQueue *queue;
   IDXGISwapChain3 *chain;
   void *ready, *done, *latency, *lists[2];
   unsigned current_index;
   int failed, tearing, occluded, resized;
   UINT64 sequence, last_use[2];
} ra_dxgi_bridge_t;

static DWORD wait_result;
static HRESULT probe_result, present_result, queue_result, signal_result;
static unsigned waits, probes, copies, presents, order;
static unsigned expected_interval = 1, expected_flags;
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
   struct chain_vtable chain_methods = { present, current };
   ID3D12CommandQueue queue;
   IDXGISwapChain3 chain;
   ra_dxgi_bridge_t bridge;
   queue.lpVtbl = &queue_methods; chain.lpVtbl = &chain_methods;
   memset(&bridge, 0, sizeof(bridge)); bridge.queue = &queue; bridge.chain = &chain;
   assert(ra_dxgi_bridge_index(&bridge) == 1 && waits == 1 && probes == 0);
   assert(ra_dxgi_bridge_present(&bridge, 7, 1));
   assert(order == 4 && copies == 1 && presents == 1 && waited == 7 && signalled == 1);
   assert(ra_dxgi_bridge_last_use(&bridge, 1) == 1 && bridge.sequence == 1);
   probe_result = DXGI_STATUS_OCCLUDED;
   assert(ra_dxgi_bridge_index(&bridge) == ~0u && waits == 1);
   assert(!ra_dxgi_bridge_failed(&bridge));
   assert(!ra_dxgi_bridge_presentable(&bridge));
   probe_result = 0;
   assert(ra_dxgi_bridge_presentable(&bridge));
   probe_result = 0; wait_result = WAIT_TIMEOUT;
   assert(ra_dxgi_bridge_index(&bridge) == ~0u && !ra_dxgi_bridge_failed(&bridge));
   bridge.resized = 1;
   assert(ra_dxgi_bridge_index(&bridge) == 1 && !bridge.resized);
   assert(ra_dxgi_bridge_index(&bridge) == ~0u && !ra_dxgi_bridge_failed(&bridge));
   wait_result = WAIT_FAILED;
   assert(ra_dxgi_bridge_index(&bridge) == ~0u && ra_dxgi_bridge_failed(&bridge));
   wait_result = WAIT_OBJECT_0; bridge.failed = 0; probe_result = -1;
   assert(ra_dxgi_bridge_index(&bridge) == ~0u && ra_dxgi_bridge_failed(&bridge));
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
   puts("PASS: DXGI queue ordering, fence lifetime, pacing, occlusion and failures");
   return 0;
}
