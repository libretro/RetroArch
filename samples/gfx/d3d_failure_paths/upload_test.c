/* Exercise shipping upload bytes and ownership under forced staging pressure. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <boolean.h>
#include <gfx/scaler/pixconv.h>
#include "formats.inc"
#define FAILED(x) ((x) < 0)
#define DXGI_ERROR_WAS_STILL_DRAWING (-2)
#define D3D11_MAP_WRITE 1
#define D3D11_MAP_FLAG_DO_NOT_WAIT 2
#define D3D11_RESOURCE_MISC_GENERATE_MIPS 1
#define RARCH_FILTER_LINEAR 0
#define RARCH_FILTER_NEAREST 1
#define RARCH_WRAP_EDGE 0
typedef unsigned UINT;
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef int HRESULT;
typedef void *D3D11Resource;
typedef void *D3D11Device;
typedef struct { void *pData; unsigned RowPitch; } D3D11_MAPPED_SUBRESOURCE;
typedef struct { unsigned left,top,front,right,bottom,back; } D3D11_BOX;
typedef struct context *D3D11DeviceContext;
typedef struct {
   D3D11DeviceContext context;
   void * staging[2];
   D3D11_MAPPED_SUBRESOURCE mapped[2];
   unsigned valid, failed;
} d3d11_texture_lend_t;
typedef struct {
   void *handle, *staging, *view, *rt_view, *sampler;
   void *upload_data;
   size_t upload_size;
   d3d11_texture_lend_t *lend;
   unsigned lend_bpp;
   struct { unsigned Width,Height,MiscFlags; DXGI_FORMAT Format; } desc;
} d3d11_texture_t;
enum texture_filter_type { TEXTURE_FILTER_MIPMAP_LINEAR, TEXTURE_FILTER_LINEAR,
   TEXTURE_FILTER_MIPMAP_NEAREST, TEXTURE_FILTER_NEAREST };
enum video_texture_update { VIDEO_TEXTURE_UPDATE_REFUSED, VIDEO_TEXTURE_UPDATE_DROPPED,
   VIDEO_TEXTURE_UPDATE_DONE };
typedef struct d3d11_planar {
   struct d3d11_planar *next;
   d3d11_texture_t *texture;
} d3d11_planar_t;
typedef struct {
   D3D11Device device;
   D3D11DeviceContext context;
   void *samplers[2][1];
   d3d11_planar_t *planar;
} d3d11_video_t;
struct texture_image { unsigned width,height; uint32_t *pixels; bool pix10,fp16,planar,supports_rgba; void *compressed; };
struct methods {
   HRESULT (*Map)(D3D11DeviceContext,void*,unsigned,unsigned,unsigned,D3D11_MAPPED_SUBRESOURCE*);
   void (*UpdateSubresource)(D3D11DeviceContext,void*,unsigned,const D3D11_BOX*,const void*,unsigned,unsigned);
   void (*GenerateMips)(D3D11DeviceContext,void*);
   void (*Unmap)(D3D11DeviceContext,void*,unsigned);
   void (*CopySubresourceRegion)(D3D11DeviceContext,void*,unsigned,unsigned,unsigned,unsigned,void*,unsigned,const D3D11_BOX*);
};
struct context { struct methods *lpVtbl; };
static unsigned char staging[4 * 64], output[4 * 64];
static unsigned maps, updates, copies, unmaps, mips, allocations, frees;
static unsigned out_width, out_height, out_bytes;
static unsigned stage_allocations;
static int map_result, fail_allocation;
static const void *updated_pointer;
static unsigned updated_pitch;
static DXGI_FORMAT substitute;
static void *checked_realloc(void *p, size_t n)
{
   allocations++;
   if (fail_allocation || n > 4096)
      return NULL;
   return realloc(p,n);
}
static void checked_free(void *p)
{
   if (p) frees++;
   free(p);
}
static unsigned bytes(DXGI_FORMAT format)
{
   switch (format)
   {
      case DXGI_FORMAT_R16G16B16A16_FLOAT: return 8;
      case DXGI_FORMAT_B4G4R4A4_UNORM:
      case DXGI_FORMAT_B5G6R5_UNORM:
      case DXGI_FORMAT_B5G5R5A1_UNORM:
      case DXGI_FORMAT_EX_A4R4G4B4_UNORM: return 2;
      case DXGI_FORMAT_A8_UNORM:
      case DXGI_FORMAT_R8_UNORM: return 1;
      default: return 4;
   }
}
static void capture(const void *p, unsigned pitch, unsigned w, unsigned h)
{
   unsigned y;
   assert(w == out_width && h == out_height);
   assert(w * out_bytes <= 32 && h <= 4);
   for (y=0;y<h;y++)
      memcpy(output+y*32,(const unsigned char*)p+y*pitch,w*out_bytes);
}
static HRESULT map(D3D11DeviceContext c, void *r, unsigned sub,
      unsigned mode, unsigned flags, D3D11_MAPPED_SUBRESOURCE *m)
{
   (void)c; (void)r; (void)sub;
   assert(mode == D3D11_MAP_WRITE && flags == D3D11_MAP_FLAG_DO_NOT_WAIT);
   maps++;
   if (!map_result)
   {
      m->pData=staging;
      m->RowPitch=64;
   }
   return map_result;
}
static void update(D3D11DeviceContext c,void *r,unsigned sub,
      const D3D11_BOX *box,const void *p,unsigned pitch,unsigned slice)
{
   (void)c; (void)r; (void)sub; (void)slice;
   if (box) assert(box->right == out_width && box->bottom == out_height);
   updated_pointer=p;
   updated_pitch=pitch;
   capture(p,pitch,out_width,out_height);
   updates++;
}
static void generate(D3D11DeviceContext c,void *view)
{ (void)c; (void)view; mips++; }
static void unmap(D3D11DeviceContext c,void *r,unsigned sub)
{ (void)c; (void)r; (void)sub; unmaps++; }
static void copy_region(D3D11DeviceContext c,void *r,unsigned sub,unsigned x,
      unsigned y,unsigned z,void *src,unsigned src_sub,const D3D11_BOX *box)
{
   (void)c; (void)r; (void)sub; (void)x; (void)y; (void)z; (void)src; (void)src_sub;
   capture(staging,64,box->right,box->bottom);
   copies++;
}
#define Release(p) do { (p)=NULL; } while (0)
static void d3d11_init_staging(D3D11Device d,d3d11_texture_t *t)
{ (void)d; t->staging=staging; stage_allocations++; }
static bool d3d11_init_texture_ex(D3D11Device d,d3d11_texture_t *t,bool stage)
{
   (void)d;
   if (substitute) t->desc.Format=substitute;
   t->handle=t; t->view=t;
   if (stage) d3d11_init_staging(d,t);
   return true;
}
static uintptr_t d3d11_planar_load(d3d11_video_t *v,
      struct texture_image *i,enum texture_filter_type f)
{ (void)v; (void)i; (void)f; assert(0); return 0; }
static enum video_texture_update d3d11_planar_update(d3d11_video_t *v,
      d3d11_planar_t *p,const struct texture_image *i)
{ (void)v; (void)p; (void)i; assert(0); return VIDEO_TEXTURE_UPDATE_REFUSED; }
#include "copy.inc"
#define realloc checked_realloc
#define free checked_free
#include "driver.inc"
#undef free
#undef realloc

static void parity(D3D11DeviceContext context, DXGI_FORMAT src, DXGI_FORMAT dst,
      unsigned pitch, bool convert)
{
   uint64_t source[16], saved[16];
   unsigned char reference[sizeof(output)];
   d3d11_texture_t texture;
   unsigned i, before, allocation_count;
   void *cached;
   memset(&texture,0,sizeof(texture));
   memset(source,0xa5,sizeof(source));
   out_width=3; out_height=2; out_bytes=bytes(dst);
   for (i=0;i<sizeof(source)/sizeof(source[0]);i++)
      source[i]=UINT64_C(0x027e093f59a17c03) ^ ((uint64_t)i << 34);
   memcpy(saved,source,sizeof(source));
   texture.staging=staging; texture.handle=&texture;
   texture.desc.Format=dst;
   texture.desc.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;
   map_result=0;
   memset(output,0xcd,sizeof(output));
   d3d11_update_texture(context,3,2,pitch,src,source,&texture);
   memcpy(reference,output,sizeof(output));
   assert(!texture.upload_data && !texture.upload_size);
   if (src == DXGI_FORMAT_R10G10B10A2_UNORM)
   {
      unsigned y,x;
      unsigned row_pitch=pitch ? pitch : 12;
      for (y=0;y<2;y++) for (x=0;x<3;x++)
      {
         uint32_t value,expected,actual;
         memcpy(&value,(const unsigned char*)source+y*row_pitch+x*4,4);
         expected=((value>>20)&1023) | (value&0xffc00)
            | ((value&1023)<<20) | 0xc0000000u;
         memcpy(&actual,output+y*32+x*4,4);
         assert(actual == expected);
      }
   }
   before=maps;
   map_result=DXGI_ERROR_WAS_STILL_DRAWING;
   allocation_count=allocations;
   d3d11_update_texture(context,3,2,pitch,src,source,&texture);
   assert(maps == before+1 && !memcmp(output,reference,sizeof(output)));
   assert(!memcmp(source,saved,sizeof(source)));
   assert((texture.upload_data != NULL) == convert);
   assert(allocations == allocation_count+(convert ? 1u : 0u));
   assert(updated_pointer == (convert ? texture.upload_data : (void*)source));
   assert(updated_pitch == (convert ? 3*out_bytes : (pitch ? pitch : 3*out_bytes)));
   cached=texture.upload_data;
   allocation_count=allocations;
   d3d11_update_texture(context,3,2,pitch,src,source,&texture);
   assert(texture.upload_data == cached && allocations == allocation_count);
   before=updates;
   map_result=-1;
   d3d11_update_texture(context,3,2,pitch,src,source,&texture);
   assert(updates == before && texture.upload_data == cached);
   d3d11_release_texture(&texture);
   assert(!texture.upload_data && !texture.upload_size);
   d3d11_release_texture(&texture);
}
static void failure(D3D11DeviceContext context)
{
   d3d11_texture_t texture;
   uint32_t source[16];
   unsigned before, allocs;
   void *old;
   memset(&texture,0,sizeof(texture)); memset(source,0,sizeof(source));
   texture.staging=staging; texture.handle=&texture;
   texture.desc.Format=DXGI_FORMAT_R10G10B10A2_UNORM;
   out_width=1; out_height=1; out_bytes=4;
   map_result=DXGI_ERROR_WAS_STILL_DRAWING;
   before=updates; fail_allocation=1;
   d3d11_update_texture(context,1,1,4,texture.desc.Format,source,&texture);
   assert(updates == before && !texture.upload_data && !texture.upload_size);
   fail_allocation=0;
   d3d11_update_texture(context,1,1,4,texture.desc.Format,source,&texture);
   old=texture.upload_data;
   assert(old && texture.upload_size == 4);
   fail_allocation=1; before=updates;
   d3d11_update_texture(context,3,2,12,texture.desc.Format,source,&texture);
   assert(updates == before && texture.upload_data == old && texture.upload_size == 4);
   fail_allocation=0;
   out_width=3; out_height=2;
   d3d11_update_texture(context,3,2,12,texture.desc.Format,source,&texture);
   assert(texture.upload_size == 24);
   allocs=allocations; before=updates;
   d3d11_update_texture(context,UINT_MAX,2,12,texture.desc.Format,source,&texture);
   d3d11_update_texture(context,3,UINT_MAX,12,texture.desc.Format,source,&texture);
   d3d11_update_texture(context,3,2,UINT_MAX,texture.desc.Format,source,&texture);
   d3d11_update_texture(context,0,2,0,texture.desc.Format,source,&texture);
   assert(allocations == allocs && updates == before);
   texture.desc.Format=DXGI_FORMAT_UNKNOWN;
   d3d11_update_texture(context,3,2,12,texture.desc.Format,source,&texture);
   assert(allocations == allocs && updates == before);
   d3d11_release_texture(&texture);
}
static void images(D3D11DeviceContext context)
{
   d3d11_video_t video;
   struct texture_image image;
   d3d11_texture_t *texture;
   uint64_t pixels[4];
   unsigned char first[sizeof(output)];
   unsigned mode,stage_count;
   memset(&video,0,sizeof(video)); video.device=&video; video.context=context;
   for (mode=0;mode<4;mode++)
   {
      memset(&image,0,sizeof(image));
      pixels[0]=UINT64_C(0x9100072000380300); pixels[1]=UINT64_C(0x027e093f59a17c03);
      image.width=2; image.height=1; image.pixels=(uint32_t*)pixels;
      image.pix10=mode<2; image.fp16=mode==3;
      out_width=2; out_height=1; out_bytes=image.fp16 ? 8 : 4;
      map_result=mode==1 ? DXGI_ERROR_WAS_STILL_DRAWING : 0;
      stage_count=stage_allocations;
      texture=(d3d11_texture_t*)d3d11_gfx_load_texture_internal(&video,&image,TEXTURE_FILTER_MIPMAP_LINEAR);
      assert(texture);
      assert(stage_allocations == stage_count+(image.pix10 ? 1u : 0u));
      if (image.pix10)
      {
         uint32_t actual;
         memcpy(&actual,output,4);
         assert(actual == 0xf0080003u);
      }
      memcpy(first,output,sizeof(output));
      map_result=0;
      assert(d3d11_gfx_update_texture_internal(&video,(uintptr_t)texture,&image)==VIDEO_TEXTURE_UPDATE_DONE);
      assert(!memcmp(first,output,out_width*out_bytes));
      map_result=DXGI_ERROR_WAS_STILL_DRAWING;
      assert(d3d11_gfx_update_texture_internal(&video,(uintptr_t)texture,&image)==VIDEO_TEXTURE_UPDATE_DROPPED);
      assert(!memcmp(first,output,out_width*out_bytes));
      d3d11_release_texture(texture); free(texture);
   }
}
int main(void)
{
   struct methods methods={map,update,generate,unmap,copy_region};
   struct context context={&methods};
   static const DXGI_FORMAT native[]={DXGI_FORMAT_B8G8R8A8_UNORM,
      DXGI_FORMAT_B8G8R8X8_UNORM,DXGI_FORMAT_R8G8B8A8_UNORM,
      DXGI_FORMAT_B5G6R5_UNORM,DXGI_FORMAT_B4G4R4A4_UNORM,
      DXGI_FORMAT_A8_UNORM,DXGI_FORMAT_R8_UNORM,
      DXGI_FORMAT_B5G5R5A1_UNORM,DXGI_FORMAT_R16G16B16A16_FLOAT};
   unsigned i;
   parity(&context,DXGI_FORMAT_R10G10B10A2_UNORM,DXGI_FORMAT_R10G10B10A2_UNORM,16,true);
   parity(&context,DXGI_FORMAT_R10G10B10A2_UNORM,DXGI_FORMAT_R10G10B10A2_UNORM,0,true);
   for (i=0;i<sizeof(native)/sizeof(native[0]);i++)
      parity(&context,native[i],native[i],0,false);
   parity(&context,DXGI_FORMAT_EX_A4R4G4B4_UNORM,DXGI_FORMAT_B4G4R4A4_UNORM,8,true);
   parity(&context,DXGI_FORMAT_EX_A4R4G4B4_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM,8,true);
   parity(&context,DXGI_FORMAT_B8G8R8A8_UNORM,DXGI_FORMAT_R8G8B8A8_UNORM,16,true);
   parity(&context,DXGI_FORMAT_B5G6R5_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM,8,true);
   failure(&context);
   images(&context);
   assert(mips && maps && unmaps && copies && updates && allocations && frees);
   puts("PASS: D3D11 packed upload parity, nonblocking conversion, cached growth/failure, image load/update and release");
   return 0;
}
