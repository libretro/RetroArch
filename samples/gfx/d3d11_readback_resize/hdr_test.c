/* Shipping HDR admission prefix: cached redraw must own no backbuffer. */
#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <boolean.h>
#define D3D11_ST_FLAG_HDR_ENABLE 2
struct texture { int unused; };
typedef struct texture ID3D11Texture2D;
struct swapchain;
struct methods { long (*GetBuffer)(struct swapchain*,unsigned,const int*,void*); };
struct swapchain { struct methods *lpVtbl; };
typedef struct swapchain *DXGISwapChain;
typedef struct { struct {unsigned dims;} vp;unsigned flags;DXGISwapChain swapChain; } d3d11_video_t;
static unsigned held,acquires,redraws,next_dims;
static d3d11_video_t state;
static struct texture texture;
static const int IID_ID3D11Texture2D=0;
static long acquire(struct swapchain *s,unsigned index,const int *iid,void *out)
{ (void)s;(void)index;(void)iid;held++;acquires++;*(ID3D11Texture2D**)out=&texture;return 0; }
static struct methods methods={acquire};
static struct swapchain swapchain={&methods};
static void video_driver_cached_frame(void)
{ assert(!held);redraws++;state.vp.dims=next_dims; }
static bool hdr_admission(d3d11_video_t *d3d11,bool is_idle)
{
 ID3D11Texture2D *BackBuffer=NULL;
 DXGISwapChain m_SwapChain;
 unsigned read_dims=0;
 (void)read_dims;
#include "hdr_prefix.inc"
 assert(held==1);held--;return true;
}
static void reset(unsigned before,unsigned after)
{ state.vp.dims=before;state.flags=D3D11_ST_FLAG_HDR_ENABLE;state.swapChain=&swapchain;next_dims=after;held=acquires=redraws=0; }
int main(void)
{
 reset(2,4);assert(!hdr_admission(&state,false));assert(redraws==1 && !acquires && !held);
 reset(4,2);assert(!hdr_admission(&state,false));assert(redraws==1 && !acquires && !held);
 reset(2,2);assert(hdr_admission(&state,false));assert(redraws==1 && acquires==1 && !held);
 reset(2,4);assert(hdr_admission(&state,true));assert(!redraws && acquires==1 && !held);
 reset(2,4);state.flags=0;assert(!hdr_admission(&state,false));assert(!redraws && !acquires);
 assert(!hdr_admission(NULL,false));puts("D3D11 HDR readback admission: passed");return 0;
}
