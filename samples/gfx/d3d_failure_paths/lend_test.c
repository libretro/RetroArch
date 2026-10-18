#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <boolean.h>
#define INLINE
#define FAILED(x) ((x) < 0)
#define DXGI_ERROR_WAS_STILL_DRAWING (-2)
#define D3D11_MAP_READ_WRITE 1
#define D3D11_MAP_WRITE 3
#define D3D11_MAP_FLAG_DO_NOT_WAIT 2
#define D3D11_USAGE_STAGING 3
#define D3D11_CPU_ACCESS_WRITE 4
#define D3D11_CPU_ACCESS_READ 8
#define D3D11_RESOURCE_MISC_GENERATE_MIPS 8
#define DXGI_FORMAT_B8G8R8A8_UNORM 1
#define DXGI_FORMAT_R16G16B16A16_FLOAT 2
#define DXGI_FORMAT_R10G10B10A2_UNORM 3
#define W 4
#define H 2
typedef int HRESULT;
typedef int DXGI_FORMAT;
typedef void *D3D11Resource;
typedef void *D3D11Texture2D;
typedef struct context *D3D11DeviceContext;
typedef struct device *D3D11Device;
typedef struct { void *pData; unsigned RowPitch; } D3D11_MAPPED_SUBRESOURCE;
typedef struct { unsigned left,top,front,right,bottom,back; } D3D11_BOX;
typedef struct {
   unsigned Width,Height,MipLevels,MiscFlags,BindFlags,Usage,CPUAccessFlags;
   DXGI_FORMAT Format;
} D3D11_TEXTURE2D_DESC;
#include "lend_type.inc"
typedef struct {
   D3D11Texture2D handle,staging;
   void *view,*rt_view,*upload_data;
   size_t upload_size;
   d3d11_texture_lend_t *lend;
   unsigned lend_bpp;
   D3D11_TEXTURE2D_DESC desc;
} d3d11_texture_t;
typedef struct planar { struct planar *next; d3d11_texture_t *texture; } d3d11_planar_t;
typedef struct {
   D3D11Device device;
   D3D11DeviceContext context;
   d3d11_planar_t *planar;
} d3d11_video_t;
struct texture_image {
   unsigned width,height;
   uint32_t *pixels;
   bool fp16,pix10,planar,supports_rgba;
};
enum video_texture_update { VIDEO_TEXTURE_UPDATE_REFUSED,
   VIDEO_TEXTURE_UPDATE_DONE, VIDEO_TEXTURE_UPDATE_DROPPED };
struct methods {
   HRESULT (*Map)(D3D11DeviceContext,D3D11Resource,unsigned,unsigned,unsigned,D3D11_MAPPED_SUBRESOURCE*);
   void (*Unmap)(D3D11DeviceContext,D3D11Resource,unsigned);
   void (*CopySubresourceRegion)(D3D11DeviceContext,D3D11Resource,unsigned,unsigned,unsigned,unsigned,D3D11Resource,unsigned,const D3D11_BOX*);
   void (*GenerateMips)(D3D11DeviceContext,void*);
   unsigned (*AddRef)(D3D11DeviceContext);
};
struct context { struct methods *lpVtbl; };
struct device_methods {
   HRESULT (*CreateTexture2D)(D3D11Device,const D3D11_TEXTURE2D_DESC*,const void*,D3D11Texture2D*);
};
struct device { struct device_methods *lpVtbl; };
struct resource {
   uint32_t cpu[2][H * 48 / 4];
   unsigned char gpu[H * W * 8];
   unsigned generation,pitch;
   bool mapped,released;
   int forced;
};
static struct resource stages[2],fallback,output;
static unsigned created,maps,unmaps,copies,cpu_copies,context_refs,bpp;
static bool fail_create,fail_alloc;
static void *context_pointer;
static void *checked_calloc(size_t n,size_t size)
{ return fail_alloc ? NULL : calloc(n,size); }
static void release(void *p)
{
   unsigned i;
   if (!p) return;
   if (p==context_pointer) { assert(context_refs); context_refs--; return; }
   for (i=0;i<2;i++)
      if (p==&stages[i]) { assert(!stages[i].mapped && !stages[i].released); stages[i].released=true; }
}
#define Release(p) do { release(p); (p)=NULL; } while (0)
static HRESULT create(D3D11Device d,const D3D11_TEXTURE2D_DESC *desc,
      const void *data,D3D11Texture2D *out)
{
   (void)d; (void)data;
   assert(desc->Usage==D3D11_USAGE_STAGING && desc->CPUAccessFlags==(D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE));
   assert(!desc->BindFlags && !desc->MiscFlags && desc->MipLevels==1);
   if (fail_create) { *out=NULL; return -1; }
   assert(created<2); *out=&stages[created++]; return 0;
}
static HRESULT map(D3D11DeviceContext c,D3D11Resource p,unsigned sub,
      unsigned mode,unsigned flags,D3D11_MAPPED_SUBRESOURCE *out)
{
   struct resource *r=(struct resource*)p;
   unsigned y;
   (void)c; (void)sub;
   assert(mode==(r==&fallback ? D3D11_MAP_WRITE : D3D11_MAP_READ_WRITE));
   assert(flags==D3D11_MAP_FLAG_DO_NOT_WAIT);
   maps++;
   if (r->forced) return r->forced;
   assert(!r->mapped);
   r->generation++;
   r->pitch=(r->generation&1) ? 32 : 48;
   memset(r->cpu[r->generation&1],0xcd,sizeof(r->cpu[0]));
   for (y=0;y<H;y++)
      memcpy((unsigned char*)r->cpu[r->generation&1]+y*r->pitch,r->gpu+y*W*bpp,W*bpp);
   r->mapped=true;
   out->pData=r->cpu[r->generation&1];out->RowPitch=r->pitch;
   return 0;
}
static void unmap(D3D11DeviceContext c,D3D11Resource p,unsigned sub)
{
   struct resource *r=(struct resource*)p;
   unsigned y;
   (void)c; (void)sub;
   assert(r->mapped); r->mapped=false; unmaps++;
   for (y=0;y<H;y++)
      memcpy(r->gpu+y*W*bpp,(unsigned char*)r->cpu[r->generation&1]+y*r->pitch,W*bpp);
   memset(r->cpu[r->generation&1],0xa5,sizeof(r->cpu[0]));
}
static void copy(D3D11DeviceContext c,D3D11Resource dst,unsigned sub,
      unsigned x,unsigned y,unsigned z,D3D11Resource src,unsigned index,const D3D11_BOX *box)
{
   struct resource *s=(struct resource*)src;
   (void)c;(void)sub;(void)x;(void)y;(void)z;(void)index;
   assert(!s->mapped && dst==&output && box->right==W && box->bottom==H);
   memcpy(output.gpu,s->gpu,H*W*bpp); copies++;
}
static unsigned addref(D3D11DeviceContext c)
{ assert(c==context_pointer); return ++context_refs; }
static void generate(D3D11DeviceContext c,void *v) { (void)c;(void)v;assert(0); }
static void d3d11_init_staging(D3D11Device d,d3d11_texture_t *t)
{ (void)d; t->staging=&fallback; }
static enum video_texture_update d3d11_planar_update(d3d11_video_t *v,
      d3d11_planar_t *p,const struct texture_image *i)
{ (void)v;(void)p;(void)i;return VIDEO_TEXTURE_UPDATE_REFUSED; }
static void dxgi_copy(int w,int h,DXGI_FORMAT sf,int sp,const void *src,
      DXGI_FORMAT df,int dp,void *dst)
{
   int y;
   (void)sf;(void)df;cpu_copies++;
   for (y=0;y<h;y++)
      memcpy((unsigned char*)dst+y*dp,(const unsigned char*)src+y*(sp ? sp : w*(int)bpp),w*bpp);
}
#define calloc checked_calloc
#include "lend.inc"
#undef calloc
static void fill(void *pixels,size_t pitch,unsigned value)
{
   unsigned y;
   for (y=0;y<H;y++) memset((unsigned char*)pixels+y*pitch,(int)value,W*bpp);
}
static void verify(unsigned value)
{
   unsigned i;
   for (i=0;i<H*W*bpp;i++) assert(output.gpu[i]==value);
}
int main(void)
{
   struct methods methods={map,unmap,copy,generate,addref};
   struct context context={&methods};
   struct device_methods device_methods={create};
   struct device device={&device_methods};
   d3d11_video_t video;
   unsigned format;
   memset(&video,0,sizeof(video));video.context=&context;video.device=&device;
   context_pointer=&context;
   for (format=0;format<2;format++)
   {
      d3d11_texture_t texture;
      struct texture_image image;
      void *pixels,*previous;
      size_t pitch,previous_pitch;
      unsigned before,slot;
      uint32_t owned[H*W*2];
      memset(&texture,0,sizeof(texture));memset(&image,0,sizeof(image));
      memset(stages,0,sizeof(stages));memset(&fallback,0,sizeof(fallback));created=0;
      bpp=format ? 8 : 4;
      texture.handle=&output;texture.lend_bpp=bpp;
      texture.desc.Width=W;texture.desc.Height=H;texture.desc.MipLevels=1;
      texture.desc.Format=format ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM;
      image.width=W;image.height=H;image.fp16=format!=0;
      assert(!d3d11_gfx_texture_lend(&video,(uintptr_t)&texture,2,W*bpp,&pitch));
      fail_alloc=true;
      assert(!d3d11_gfx_texture_lend(&video,(uintptr_t)&texture,0,W*bpp,&pitch));
      fail_alloc=false;fail_create=true;
      assert(!d3d11_gfx_texture_lend(&video,(uintptr_t)&texture,0,W*bpp,&pitch));
      fail_create=false;
      for (slot=0;slot<2;slot++)
      {
         pixels=d3d11_gfx_texture_lend(&video,(uintptr_t)&texture,slot,W*bpp,&pitch);
         assert(pixels && pitch>=W*bpp && context_refs==1);
         previous=pixels;previous_pitch=pitch;
         fill(pixels,pitch,20+slot);image.pixels=(uint32_t*)pixels;
         before=cpu_copies;
         assert(d3d11_gfx_update_texture_internal(&video,(uintptr_t)&texture,&image)==VIDEO_TEXTURE_UPDATE_DONE);
         assert(cpu_copies==before);verify(20+slot);
         stages[slot].forced=DXGI_ERROR_WAS_STILL_DRAWING;before=unmaps;
         assert(!d3d11_gfx_texture_lend_ready(&video,(uintptr_t)&texture,slot));
         assert(!d3d11_gfx_texture_lend(&video,(uintptr_t)&texture,slot,W*bpp,&pitch) && unmaps==before);
         stages[slot].forced=0;
         assert(d3d11_gfx_texture_lend_ready(&video,(uintptr_t)&texture,slot));
         pixels=d3d11_gfx_texture_lend(&video,(uintptr_t)&texture,slot,W*bpp,&pitch);
         assert(pixels && pixels!=previous && pitch!=previous_pitch);
         fill(pixels,pitch,40+slot);image.pixels=(uint32_t*)pixels;
         assert(d3d11_gfx_update_texture_internal(&video,(uintptr_t)&texture,&image)==VIDEO_TEXTURE_UPDATE_DONE);
         verify(40+slot);
      }
      stages[0].forced=-1;
      assert(d3d11_gfx_texture_lend_ready(&video,(uintptr_t)&texture,0));
      assert(!d3d11_gfx_texture_lend(&video,(uintptr_t)&texture,0,W*bpp,&pitch));
      assert(d3d11_gfx_texture_lend_ready(&video,(uintptr_t)&texture,1));
      pixels=d3d11_gfx_texture_lend(&video,(uintptr_t)&texture,1,W*bpp,&pitch);
      assert(pixels);fill(pixels,pitch,63);
      memset(owned,71,sizeof(owned));image.pixels=owned;before=cpu_copies;
      assert(d3d11_gfx_update_texture_internal(&video,(uintptr_t)&texture,&image)==VIDEO_TEXTURE_UPDATE_DONE);
      assert(cpu_copies==before+1 && stages[1].mapped);verify(71);
      image.pixels=(uint32_t*)pixels;
      image.pix10=true;
      assert(d3d11_gfx_update_texture_internal(&video,(uintptr_t)&texture,&image)==VIDEO_TEXTURE_UPDATE_REFUSED);
      assert(stages[1].mapped);image.pix10=false;
      before=unmaps;
      d3d11_release_texture(&texture);
      assert(unmaps==before+1 && !context_refs && stages[0].released && stages[1].released);
      assert(!texture.lend && !texture.lend_bpp);
      d3d11_release_texture(&texture);
   }
   assert(maps && copies && unmaps && cpu_copies);
   puts("PASS: D3D11 renewable maps, pitch/address changes, cancellation, busy/error fallback and no producer copy");
   return 0;
}
