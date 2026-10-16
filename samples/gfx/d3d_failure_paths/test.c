#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <boolean.h>

#define FAILED(hr) ((hr) < 0)
#define SUCCEEDED(hr) (!FAILED(hr))
#define D3D10_MAP_WRITE 1
#define D3D10_MAP_WRITE_DISCARD 2
#define D3D11_MAP_WRITE_DISCARD 2
#define D3D10_RESOURCE_MISC_GENERATE_MIPS 1
#define D3D10_ST_FLAG_RESIZE_CHAIN 1
#define D3D11_ST_FLAG_RESIZE_CHAIN 1
#define D3D10_ST_FLAG_RESIZE_VIEWPORT 2
#define D3D11_ST_FLAG_RESIZE_VIEWPORT 2
#define D3D11_ST_FLAG_HAS_ALLOW_TEARING 4
#define D3D11_ST_FLAG_WAITABLE_SWAPCHAINS 8
#define D3D11_ST_FLAG_HDR_ENABLE 16
#define DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING 4
#define DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT 8
#define DXGI_FORMAT_UNKNOWN 0
#define VIDEO_FLAG_GPU_DEVICE_LOST 32
#define VIDEO_SCALE_PACK(w,h) (((w) << 16) | (h))
#define uuidof(t) 0
typedef unsigned UINT;
typedef unsigned DXGI_FORMAT;
typedef int math_matrix_4x4;
typedef int dxgi_hdr_uniform_t;
typedef struct { void *pData; unsigned RowPitch; } D3D10_MAPPED_TEXTURE2D;
typedef D3D10_MAPPED_TEXTURE2D D3D11_MAPPED_SUBRESOURCE;
typedef struct { unsigned left, top, front, right, bottom, back; } D3D10_BOX;
static int fail_map, fail_resize, fail_get, fail_view;
static int memory, unmaps, copies, mips, lost, output_dims, buffer_gets, creates;
typedef struct object { int refs; } object;
typedef object *D3D10Resource;
typedef object *D3D11Resource;
typedef object *D3D10Texture2D;
typedef object *D3D11Texture2D;
typedef object *D3D11RenderTargetView;
static object back = {0}, target = {0};
#define Release(p) do { if (p) { assert((p)->refs > 0); --(p)->refs; (p) = NULL; } } while (0)
typedef struct staging staging;
struct staging
{
   struct staging_vtable
   {
      int (*Map)(staging*, unsigned, unsigned, unsigned, D3D10_MAPPED_TEXTURE2D*);
      void (*Unmap)(staging*, unsigned);
   } *lpVtbl;
};
typedef struct buffer buffer;
struct buffer
{
   struct buffer_vtable
   {
      int (*Map)(buffer*, unsigned, unsigned, void**);
      void (*Unmap)(buffer*);
   } *lpVtbl;
};
typedef struct device device;
struct device
{
   struct device_vtable
   {
      void (*CopySubresourceRegion)(device*, object*, unsigned, unsigned, unsigned,
            unsigned, object*, unsigned, const D3D10_BOX*);
      void (*GenerateMips)(device*, object*);
      int (*CreateRenderTargetView)(device*, object*, void*, object**);
      void (*OMSetRenderTargets)(device*, unsigned, object**, void*);
      void (*Flush)(device*);
      int (*Map)(device*, object*, unsigned, unsigned, unsigned, D3D11_MAPPED_SUBRESOURCE*);
      void (*Unmap)(device*, object*, unsigned);
   } *lpVtbl;
};
typedef device *D3D10Device;
typedef struct swapchain swapchain;
struct swapchain
{
   struct swap_vtable { int (*GetBuffer)(swapchain*, unsigned, int, void**); } *lpVtbl;
};
typedef struct
{
   staging *staging;
   object *handle, *view;
   struct { unsigned Format, MiscFlags; } desc;
} d3d10_texture_t;
typedef struct
{
   device *device, *context;
   swapchain *swapChain;
   object *renderTargetView;
   struct { buffer *ubo; } frame;
   struct { unsigned Width, Height; } viewport;
   struct { unsigned right, bottom; } scissor;
   struct { struct { unsigned width, height; } OutputSize; } ubo_values;
   struct { object *ubo; int ubo_values; } hdr;
   unsigned flags, chain_bit_depth, chain_formats[2];
   math_matrix_4x4 mvp;
} d3d10_video_t;
typedef d3d10_video_t d3d11_video_t;
static int stage_map(staging *s, unsigned a, unsigned b, unsigned c, D3D10_MAPPED_TEXTURE2D *m)
{ (void)s; (void)a; (void)b; (void)c; m->pData=&memory; m->RowPitch=4; return fail_map ? -1 : 0; }
static void stage_unmap(staging *s, unsigned a) { (void)s; (void)a; unmaps++; }
static int buffer_map(buffer *b, unsigned a, unsigned c, void **m)
{ (void)b; (void)a; (void)c; *m=&memory; return fail_map ? -1 : 0; }
static void buffer_unmap(buffer *b) { (void)b; unmaps++; }
static int context_map(device *d, object *b, unsigned a, unsigned c, unsigned e, D3D11_MAPPED_SUBRESOURCE *m)
{ (void)d; (void)b; (void)a; (void)c; (void)e; m->pData=&memory; return fail_map ? -1 : 0; }
static void context_unmap(device *d, object *b, unsigned a) { (void)d; (void)b; (void)a; unmaps++; }
static void copy(device *d, object *a, unsigned b, unsigned c, unsigned e, unsigned f, object *g, unsigned h, const D3D10_BOX *box)
{ (void)d; (void)a; (void)b; (void)c; (void)e; (void)f; (void)g; (void)h; (void)box; copies++; }
static void generate(device *d, object *v) { (void)d; (void)v; mips++; }
static int get_buffer(swapchain *s, unsigned a, int b, void **p)
{ (void)s; (void)a; (void)b; buffer_gets++; if(fail_get) return -1; back.refs++; *p=&back; return 0; }
static int create_view(device *d, object *b, void *desc, object **p)
{ (void)d; (void)desc; assert(b==&back); creates++; if(fail_view) return -1; target.refs++; *p=&target; return 0; }
static void bind(device *d, unsigned n, object **v, void *ds) { (void)d; (void)n; (void)v; (void)ds; }
static void flush(device *d) { (void)d; }
static int resize(swapchain *s, unsigned n, unsigned w, unsigned h, unsigned f, unsigned flags)
{ (void)s; (void)n; (void)w; (void)h; (void)f; (void)flags; return fail_resize ? -1 : 0; }
#define DXGIResizeBuffers resize
static void video_driver_modify_disp_flags(unsigned set, unsigned clear) { (void)clear; lost |= set; }
static void video_driver_set_output_dims(unsigned dims) { (void)dims; output_dims++; }
static void dxgi_copy(int w, int h, DXGI_FORMAT f, int pitch, const void *src, unsigned fmt, unsigned row, void *dst)
{ (void)w; (void)h; (void)f; (void)pitch; (void)src; (void)fmt; (void)row; assert(dst==&memory); memory=7; }
#include "upload.inc"
static void rotation(d3d10_video_t *d3d10)
{
   void *mapped_ubo = NULL;
#include "rotation.inc"
}
static void hdr0(d3d11_video_t *d3d11)
#include "hdr0.inc"
static void hdr1(d3d11_video_t *d3d11)
#include "hdr1.inc"
static void hdr2(d3d11_video_t *d3d11)
#include "hdr2.inc"
static bool resize10(d3d10_video_t *d3d10)
{
   unsigned video_width=800, video_height=600;
#include "resize10.inc"
   return true;
}
static bool resize11(d3d11_video_t *d3d11)
{
   unsigned video_width=800, video_height=600;
   device *context=d3d11->context;
#ifdef HAVE_DXGI_HDR
   unsigned desired_bit_depth=1;
   bool video_hdr_enable=true;
#endif
#include "resize11.inc"
   return true;
}
static bool buffer11(d3d11_video_t *d3d11)
{
   object *rtv=NULL;
#include "buffer11.inc"
   Release(rtv);
   return true;
}
int main(void)
{
   struct device_vtable dv={copy,generate,create_view,bind,flush,context_map,context_unmap};
   struct staging_vtable sv={stage_map,stage_unmap};
   struct buffer_vtable bv={buffer_map,buffer_unmap};
   struct swap_vtable swv={get_buffer};
   device dev={&dv}; staging stage={&sv}; buffer buf={&bv}; swapchain swap={&swv};
   d3d10_texture_t texture;
   d3d10_video_t drv;
   unsigned i;
   memset(&texture,0,sizeof(texture)); memset(&drv,0,sizeof(drv));
   texture.staging=&stage; texture.desc.MiscFlags=1;
   drv.device=&dev; drv.context=&dev; drv.swapChain=&swap; drv.frame.ubo=&buf;
   drv.mvp=9; drv.hdr.ubo_values=11;
   fail_map=1;
   d3d10_update_texture(&dev,1,1,4,0,NULL,&texture);
   rotation(&drv); hdr0(&drv); hdr1(&drv); hdr2(&drv);
   assert(memory==0 && unmaps==0 && copies==0 && mips==0);
   fail_map=0;
   d3d10_update_texture(&dev,1,1,4,0,NULL,&texture);
   assert(memory==7 && unmaps==1 && copies==1 && mips==1);
   rotation(&drv); assert(memory==9 && unmaps==2);
   hdr0(&drv); hdr1(&drv); hdr2(&drv); assert(memory==11 && unmaps==5);
   for(i=0;i<4;i++)
   {
      fail_resize=i==0; fail_get=i==1; fail_view=i==2;
      drv.flags=1; drv.viewport.Width=0; lost=output_dims=buffer_gets=creates=0;
      assert(resize10(&drv)==(i==3));
      assert((i==3) ? (output_dims==1 && drv.viewport.Width==800) :
            (lost==32 && output_dims==0 && drv.viewport.Width==0));
      assert(buffer_gets==(i==0 ? 0 : 1));
      assert(creates==(i<2 ? 0 : 1));
      assert(back.refs==0); Release(drv.renderTargetView);
   }
   fail_get=fail_view=0; fail_resize=1;
   drv.flags=0; drv.chain_bit_depth=0; drv.viewport.Width=0; lost=output_dims=0;
   assert(!resize11(&drv));
   assert(lost==32 && drv.viewport.Width==0 && output_dims==0 && drv.chain_bit_depth==0 && drv.flags==0);
   fail_resize=0; assert(resize11(&drv)); assert(output_dims==1 && drv.viewport.Width==800);
#ifdef HAVE_DXGI_HDR
   assert(drv.chain_bit_depth==1 && (drv.flags & D3D11_ST_FLAG_HDR_ENABLE));
#else
   assert(drv.chain_bit_depth==0 && !(drv.flags & D3D11_ST_FLAG_HDR_ENABLE));
#endif
   for(i=0;i<3;i++)
   {
      fail_get=i==0; fail_view=i==1; lost=0;
      assert(buffer11(&drv)==(i==2));
      assert(back.refs==0 && target.refs==0);
      if(i<2) assert(lost==32);
   }
   puts("D3D failure paths: maps, resize, back buffer and recovery PASS");
   return 0;
}
