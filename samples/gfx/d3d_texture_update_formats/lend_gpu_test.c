#include <stdio.h>
#ifdef _WIN32
#define COBJMACROS
#include <windows.h>
#include <d3d10.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <boolean.h>
#define INLINE
#define W 17
#define H 3
#define Release(p) do {if(p){(p)->lpVtbl->Release(p);(p)=NULL;}}while(0)
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %u: %s\n",(unsigned)__LINE__,#x);goto done;}}while(0)
typedef ID3D10Texture2D *D3D10Texture2D;
typedef ID3D10Resource *D3D10Resource;
#include "lend_type.inc"
typedef struct {
   ID3D10Texture2D *handle,*staging;
   ID3D10ShaderResourceView *view;
   ID3D10RenderTargetView *rt_view;
   D3D10_TEXTURE2D_DESC desc;
   d3d10_texture_lend_t *lend;
} d3d10_texture_t;
typedef struct { ID3D10Device *device; } d3d10_video_t;
struct texture_image { unsigned width,height; uint32_t *pixels; bool pix10,supports_rgba; };
enum video_texture_update { VIDEO_TEXTURE_UPDATE_REFUSED,VIDEO_TEXTURE_UPDATE_DROPPED,VIDEO_TEXTURE_UPDATE_DONE };
static unsigned cpu_copies;
static void dxgi_copy(unsigned w,unsigned h,DXGI_FORMAT fmt,unsigned sp,const void *src,
      DXGI_FORMAT outfmt,unsigned pitch,void *dst)
{
   unsigned row;(void)fmt;(void)outfmt;(void)sp;cpu_copies++;
   for(row=0;row<h;row++)memcpy((uint8_t*)dst+row*pitch,(const uint8_t*)src+row*w*4,w*4);
}
#include "lend.inc"
static void fill(void *p,size_t pitch,unsigned phase)
{unsigned x,y;for(y=0;y<H;y++)for(x=0;x<W*4;x++)((uint8_t*)p)[y*pitch+x]=(uint8_t)(phase+x+y*7);}
static bool matches(const void *p,size_t pitch,unsigned phase)
{unsigned x,y;for(y=0;y<H;y++)for(x=0;x<W*4;x++)if(((const uint8_t*)p)[y*pitch+x]!=(uint8_t)(phase+x+y*7))return false;return true;}
int main(void)
{
   d3d10_video_t video;d3d10_texture_t texture;struct texture_image image;
   ID3D10Texture2D *read=NULL;D3D10_TEXTURE2D_DESC desc;
   unsigned frame;size_t pitch;void *pixels;int result=1;
   uint32_t owned[W*H];
   memset(&video,0,sizeof(video));memset(&texture,0,sizeof(texture));memset(&image,0,sizeof(image));
   if(FAILED(D3D10CreateDevice(NULL,D3D10_DRIVER_TYPE_HARDWARE,NULL,0,D3D10_SDK_VERSION,&video.device)))
   {puts("SKIP D3D10 texture loan GPU oracle: no device");return 0;}
   memset(&desc,0,sizeof(desc));desc.Width=W;desc.Height=H;
   desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
   desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BindFlags=D3D10_BIND_SHADER_RESOURCE;
   CHECK(SUCCEEDED(video.device->lpVtbl->CreateTexture2D(video.device,&desc,NULL,&texture.handle)));
   texture.desc=desc;desc.BindFlags=0;desc.Usage=D3D10_USAGE_STAGING;
   desc.CPUAccessFlags=D3D10_CPU_ACCESS_WRITE;
   CHECK(SUCCEEDED(video.device->lpVtbl->CreateTexture2D(video.device,&desc,NULL,&texture.staging)));
   desc.CPUAccessFlags=D3D10_CPU_ACCESS_READ;
   CHECK(SUCCEEDED(video.device->lpVtbl->CreateTexture2D(video.device,&desc,NULL,&read)));
   image.width=W;image.height=H;image.supports_rgba=true;
   for(frame=0;frame<24;frame++)
   {
      D3D10_MAPPED_TEXTURE2D mapped;
      CHECK(d3d10_gfx_texture_lend_ready(&video,(uintptr_t)&texture,frame%2));
      pixels=d3d10_gfx_texture_lend(&video,(uintptr_t)&texture,frame%2,W*4,&pitch);
      CHECK(pixels&&pitch>=W*4);fill(pixels,pitch,frame);image.pixels=(uint32_t*)pixels;
      CHECK(d3d10_gfx_update_texture_internal(&video,(uintptr_t)&texture,&image)==VIDEO_TEXTURE_UPDATE_DONE);
      CHECK(!cpu_copies);
      video.device->lpVtbl->CopyResource(video.device,(ID3D10Resource*)read,(ID3D10Resource*)texture.handle);
      CHECK(SUCCEEDED(read->lpVtbl->Map(read,0,D3D10_MAP_READ,0,&mapped)));
      if(!matches(mapped.pData,mapped.RowPitch,frame)){read->lpVtbl->Unmap(read,0);CHECK(false);}
      read->lpVtbl->Unmap(read,0);
   }
   {
      D3D10_MAPPED_TEXTURE2D mapped;
      fill(owned,W*4,99);image.pixels=owned;
      CHECK(d3d10_gfx_update_texture_internal(&video,(uintptr_t)&texture,&image)==VIDEO_TEXTURE_UPDATE_DONE);
      CHECK(cpu_copies==1);
      video.device->lpVtbl->CopyResource(video.device,(ID3D10Resource*)read,(ID3D10Resource*)texture.handle);
      CHECK(SUCCEEDED(read->lpVtbl->Map(read,0,D3D10_MAP_READ,0,&mapped)));
      if(!matches(mapped.pData,mapped.RowPitch,99)){read->lpVtbl->Unmap(read,0);CHECK(false);}
      read->lpVtbl->Unmap(read,0);
   }
   CHECK(!d3d10_gfx_texture_lend(&video,(uintptr_t)&texture,2,W*4,&pitch));
   texture.desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;
   CHECK(!d3d10_gfx_texture_lend(&video,(uintptr_t)&texture,0,W*4,&pitch));
   texture.desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
   CHECK(d3d10_gfx_texture_lend(&video,(uintptr_t)&texture,0,W*4,&pitch));
   {
      d3d10_texture_t *retired=(d3d10_texture_t*)malloc(sizeof(*retired));
      CHECK(retired);*retired=texture;memset(&texture,0,sizeof(texture));
      d3d10_gfx_unload_texture_internal((uintptr_t)retired);
   }
   result=0;
done:
   d3d10_release_texture(&texture);Release(read);Release(video.device);
   if(!result)puts("PASS D3D10 texture loan GPU pixels: 24 padded frames, no CPU copy, fallback and retirement");
   return result;
}
#else
int main(void){puts("SKIP D3D10 texture loan GPU oracle: Windows-only");return 0;}
#endif
