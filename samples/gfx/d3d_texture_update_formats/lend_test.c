#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <boolean.h>
#define INLINE
#define FAILED(x) ((x)<0)
#define DXGI_ERROR_WAS_STILL_DRAWING (-2)
#define D3D10_MAP_READ_WRITE 1
#define D3D10_MAP_WRITE 2
#define D3D10_MAP_FLAG_DO_NOT_WAIT 4
#define D3D10_USAGE_STAGING 3
#define D3D10_CPU_ACCESS_READ 1
#define D3D10_CPU_ACCESS_WRITE 2
#define D3D10_RESOURCE_MISC_GENERATE_MIPS 4
#define DXGI_FORMAT_B8G8R8A8_UNORM 1
#define DXGI_FORMAT_R8G8B8A8_UNORM 2
#define W 17
#define H 3
typedef unsigned UINT;
typedef int HRESULT;
typedef struct { void *pData; unsigned RowPitch; } D3D10_MAPPED_TEXTURE2D;
typedef struct { unsigned Width,Height,MipLevels,MiscFlags,BindFlags,Usage,CPUAccessFlags; int Format; } D3D10_TEXTURE2D_DESC;
typedef struct { unsigned left,top,front,right,bottom,back; } D3D10_BOX;
typedef struct resource resource_t;
typedef resource_t *D3D10Resource;
typedef resource_t *D3D10Texture2D;
typedef struct {
   HRESULT (*Map)(resource_t*,unsigned,unsigned,unsigned,D3D10_MAPPED_TEXTURE2D*);
   void (*Unmap)(resource_t*,unsigned);
   void (*Release)(resource_t*);
} resource_vtbl_t;
struct resource { resource_vtbl_t *lpVtbl; unsigned epoch,pitch; uint8_t bytes[2][H*128]; };
typedef struct device device_t;
typedef struct {
   HRESULT (*CreateTexture2D)(device_t*,const D3D10_TEXTURE2D_DESC*,const void*,resource_t**);
   void (*CopySubresourceRegion)(device_t*,resource_t*,unsigned,unsigned,unsigned,unsigned,resource_t*,unsigned,const D3D10_BOX*);
   void (*GenerateMips)(device_t*,void*);
} device_vtbl_t;
struct device { device_vtbl_t *lpVtbl; };
typedef device_t *D3D10Device;
#include "lend_type.inc"
typedef struct {
   resource_t *handle,*staging,*view,*rt_view;
   D3D10_TEXTURE2D_DESC desc;
   d3d10_texture_lend_t *lend;
} d3d10_texture_t;
typedef struct { device_t *device; } d3d10_video_t;
struct texture_image { unsigned width,height; uint32_t *pixels; bool pix10,supports_rgba; };
enum video_texture_update { VIDEO_TEXTURE_UPDATE_REFUSED,VIDEO_TEXTURE_UPDATE_DROPPED,VIDEO_TEXTURE_UPDATE_DONE };
#define Release(p) do {if(p){(p)->lpVtbl->Release(p);(p)=NULL;}}while(0)
static unsigned maps,unmaps,copies,cpu_copies,live;
static HRESULT status;
static bool refuse_create;
static HRESULT map(resource_t *r,unsigned sub,unsigned mode,unsigned flags,D3D10_MAPPED_TEXTURE2D *out)
{
   assert(!sub && (mode==D3D10_MAP_READ_WRITE || mode==D3D10_MAP_WRITE));
   assert(flags==D3D10_MAP_FLAG_DO_NOT_WAIT); maps++;
   if(status)return status;
   r->epoch^=1;r->pitch=W*4+16*r->epoch;
   out->pData=r->bytes[r->epoch];out->RowPitch=r->pitch;return 0;
}
static void unmap(resource_t *r,unsigned sub){(void)r;assert(!sub);unmaps++;}
static void release(resource_t *r){live--;free(r);}
static resource_vtbl_t resource_vtbl={map,unmap,release};
static resource_t *resource(void)
{resource_t *r=(resource_t*)calloc(1,sizeof(*r));assert(r);r->lpVtbl=&resource_vtbl;live++;return r;}
static HRESULT create(device_t *d,const D3D10_TEXTURE2D_DESC *desc,const void *init,resource_t **out)
{
   (void)d;assert(!init && desc->Usage==D3D10_USAGE_STAGING && !desc->BindFlags && !desc->MiscFlags);
   assert(desc->CPUAccessFlags==(D3D10_CPU_ACCESS_READ|D3D10_CPU_ACCESS_WRITE));
   *out=refuse_create?NULL:resource();return refuse_create?-1:0;
}
static void copy(device_t *d,resource_t *dst,unsigned sub,unsigned x,unsigned y,unsigned z,
      resource_t *src,unsigned srcsub,const D3D10_BOX *box)
{
   unsigned row;(void)d;assert(!sub&&!srcsub&&!x&&!y&&!z&&box->right==W&&box->bottom==H);
   copies++;for(row=0;row<H;row++)memcpy(dst->bytes[0]+row*W*4,src->bytes[src->epoch]+row*src->pitch,W*4);
}
static void mips(device_t *d,void *view){(void)d;(void)view;assert(0);}
static device_vtbl_t device_vtbl={create,copy,mips};
static void dxgi_copy(unsigned w,unsigned h,int fmt,unsigned sp,const void *src,int outfmt,unsigned pitch,void *dst)
{
   unsigned row;(void)fmt;(void)outfmt;assert(!sp);cpu_copies++;
   for(row=0;row<h;row++)memcpy((uint8_t*)dst+row*pitch,(const uint8_t*)src+row*w*4,w*4);
}
#include "lend.inc"
static void fill(void *p,size_t pitch,unsigned phase)
{unsigned x,y;for(y=0;y<H;y++)for(x=0;x<W*4;x++)((uint8_t*)p)[y*pitch+x]=(uint8_t)(phase+x+y*7);}
static void expect(const void *p,unsigned phase)
{unsigned x,y;for(y=0;y<H;y++)for(x=0;x<W*4;x++)assert(((const uint8_t*)p)[y*W*4+x]==(uint8_t)(phase+x+y*7));}
int main(void)
{
   device_t device={&device_vtbl};d3d10_video_t video;
   d3d10_texture_t texture;struct texture_image image;
   unsigned frame,slot,before;size_t pitch;void *pixels;
   uint32_t owned[W*H];
   memset(&texture,0,sizeof(texture));memset(&image,0,sizeof(image));video.device=&device;
   texture.handle=resource();texture.staging=resource();
   texture.desc.Width=W;texture.desc.Height=H;texture.desc.MipLevels=1;
   texture.desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;image.width=W;image.height=H;image.supports_rgba=true;
   assert(!d3d10_gfx_texture_lend(&video,(uintptr_t)&texture,2,W*4,&pitch));
   assert(!d3d10_gfx_texture_lend(&video,(uintptr_t)&texture,0,W*4+4,&pitch));
   texture.desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
   assert(!d3d10_gfx_texture_lend(&video,(uintptr_t)&texture,0,W*4,&pitch));
   texture.desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;texture.desc.MipLevels=2;
   assert(!d3d10_gfx_texture_lend(&video,(uintptr_t)&texture,0,W*4,&pitch));
   texture.desc.MipLevels=1;refuse_create=true;
   assert(!d3d10_gfx_texture_lend(&video,(uintptr_t)&texture,0,W*4,&pitch));refuse_create=false;
   for(frame=0;frame<24;frame++)
   {
      slot=frame%2;pixels=d3d10_gfx_texture_lend(&video,(uintptr_t)&texture,slot,W*4,&pitch);
      assert(pixels&&pitch>=W*4);fill(pixels,pitch,frame);image.pixels=(uint32_t*)pixels;
      if(!frame)
      {
         assert(!d3d10_gfx_texture_lend(&video,(uintptr_t)&texture,slot,W*4,NULL));
         image.supports_rgba=false;
         assert(d3d10_gfx_update_texture_internal(&video,(uintptr_t)&texture,&image)==VIDEO_TEXTURE_UPDATE_REFUSED);
         image.supports_rgba=true;texture.lend->valid&=~(1u<<slot);
         assert(d3d10_gfx_update_texture_internal(&video,(uintptr_t)&texture,&image)==VIDEO_TEXTURE_UPDATE_DROPPED);
         texture.lend->valid|=1u<<slot;
      }
      assert(d3d10_gfx_update_texture_internal(&video,(uintptr_t)&texture,&image)==VIDEO_TEXTURE_UPDATE_DONE);
      assert(!cpu_copies&&copies==frame+1);expect(texture.handle->bytes[0],frame);
      status=DXGI_ERROR_WAS_STILL_DRAWING;before=unmaps;
      assert(!d3d10_gfx_texture_lend_ready(&video,(uintptr_t)&texture,slot));
      assert(!d3d10_gfx_texture_lend(&video,(uintptr_t)&texture,slot,W*4,&pitch)&&unmaps==before);
      status=0;assert(d3d10_gfx_texture_lend_ready(&video,(uintptr_t)&texture,slot));
   }
   fill(owned,W*4,91);image.pixels=owned;
   assert(d3d10_gfx_update_texture_internal(&video,(uintptr_t)&texture,&image)==VIDEO_TEXTURE_UPDATE_DONE);
   assert(cpu_copies==1);expect(texture.handle->bytes[0],91);
   /* Finish a loan, then make its next refresh fail permanently. */
   pixels=d3d10_gfx_texture_lend(&video,(uintptr_t)&texture,0,W*4,&pitch);assert(pixels);
   image.pixels=(uint32_t*)pixels;
   assert(d3d10_gfx_update_texture_internal(&video,(uintptr_t)&texture,&image)==VIDEO_TEXTURE_UPDATE_DONE);
   status=-1;assert(d3d10_gfx_texture_lend_ready(&video,(uintptr_t)&texture,0));
   assert(!d3d10_gfx_texture_lend(&video,(uintptr_t)&texture,0,W*4,&pitch));status=0;
   assert(d3d10_gfx_texture_lend(&video,(uintptr_t)&texture,1,W*4,&pitch));
   before=unmaps;d3d10_release_texture(&texture);assert(!texture.lend&&!live&&unmaps==before+1);
   {
      d3d10_texture_t *retired=(d3d10_texture_t*)calloc(1,sizeof(*retired));
      assert(retired);retired->desc=texture.desc;
      retired->handle=resource();retired->staging=resource();
      assert(d3d10_gfx_texture_lend(&video,(uintptr_t)retired,0,W*4,&pitch));
      before=unmaps;d3d10_gfx_unload_texture_internal((uintptr_t)retired);
      assert(!live&&unmaps==before+1);
   }
   puts("PASS D3D10 texture loans: changing maps/pitches, no CPU copy, busy/failure fallback and cleanup");return 0;
}
