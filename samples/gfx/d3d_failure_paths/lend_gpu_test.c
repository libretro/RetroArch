#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <stdint.h>
#include <string.h>
#include <boolean.h>
#include <retro_atomic.h>
#include <formats/video_player.h>
#define INLINE
#define Release(p) do { if (p) { (p)->lpVtbl->Release(p); (p)=NULL; } } while (0)
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %u: %s\n",(unsigned)__LINE__,#x); goto done; } } while (0)
typedef ID3D11Device *D3D11Device;
typedef ID3D11DeviceContext *D3D11DeviceContext;
typedef ID3D11Texture2D *D3D11Texture2D;
typedef ID3D11Resource *D3D11Resource;
#include "lend_type.inc"
typedef struct {
   ID3D11Texture2D *handle,*staging;
   ID3D11ShaderResourceView *view;
   ID3D11RenderTargetView *rt_view;
   void *upload_data;
   size_t upload_size;
   d3d11_texture_lend_t *lend;
   unsigned lend_bpp;
   D3D11_TEXTURE2D_DESC desc;
} d3d11_texture_t;
typedef struct planar { struct planar *next; d3d11_texture_t *texture; } d3d11_planar_t;
typedef struct { D3D11Device device; D3D11DeviceContext context; d3d11_planar_t *planar; } d3d11_video_t;
enum video_texture_update { VIDEO_TEXTURE_UPDATE_REFUSED,VIDEO_TEXTURE_UPDATE_DONE,VIDEO_TEXTURE_UPDATE_DROPPED };
static unsigned cpu_copies,bpp,context_refs,unmaps;
static ID3D11DeviceContext *real_context;
static HRESULT forced;
#include "states.inc"
typedef struct { retro_atomic_int_t status; bool gpu; } vp_job_t;
static vp_job_t job;
#include "publish.inc"
static retro_atomic_int_t work_command,work_stop;
static struct { void *pixels; size_t pitch; unsigned phase; int alive; } work_frame;
static enum video_texture_update d3d11_planar_update(d3d11_video_t *v,d3d11_planar_t *p,const struct texture_image *i)
{ (void)v;(void)p;(void)i;return VIDEO_TEXTURE_UPDATE_REFUSED; }
static void dxgi_copy(int w,int h,DXGI_FORMAT sf,int sp,const void *src,DXGI_FORMAT df,int dp,void *dst)
{
   int y;
   (void)sf;(void)df;cpu_copies++;
   for (y=0;y<h;y++) memcpy((unsigned char*)dst+y*dp,(const unsigned char*)src+y*(sp ? sp : w*(int)bpp),w*bpp);
}
#include "lend.inc"
static HRESULT STDMETHODCALLTYPE map(ID3D11DeviceContext *ctx,ID3D11Resource *r,UINT sub,
      D3D11_MAP mode,UINT flags,D3D11_MAPPED_SUBRESOURCE *out)
{
   (void)ctx;
   if (mode!=D3D11_MAP_READ_WRITE || flags!=D3D11_MAP_FLAG_DO_NOT_WAIT) return E_INVALIDARG;
   return forced ? forced : ID3D11DeviceContext_Map(real_context,r,sub,mode,flags,out);
}
static void STDMETHODCALLTYPE unmap(ID3D11DeviceContext *ctx,ID3D11Resource *r,UINT sub)
{ (void)ctx;unmaps++;ID3D11DeviceContext_Unmap(real_context,r,sub); }
static void STDMETHODCALLTYPE copy(ID3D11DeviceContext *ctx,ID3D11Resource *r,UINT sub,
      UINT x,UINT y,UINT z,ID3D11Resource *src,UINT index,const D3D11_BOX *box)
{ (void)ctx;ID3D11DeviceContext_CopySubresourceRegion(real_context,r,sub,x,y,z,src,index,box); }
static ULONG STDMETHODCALLTYPE addref(ID3D11DeviceContext *ctx)
{ (void)ctx;context_refs++;return ID3D11DeviceContext_AddRef(real_context); }
static ULONG STDMETHODCALLTYPE release(ID3D11DeviceContext *ctx)
{ (void)ctx;context_refs--;return (real_context->lpVtbl->Release)(real_context); }
static void fill(void *pixels,size_t pitch,unsigned phase)
{
   unsigned x,y;
   for (y=0;y<3;y++)
      for (x=0;x<17*bpp;x++) ((unsigned char*)pixels)[y*pitch+x]=(unsigned char)(phase+x+7*y);
}
static bool matches(const void *pixels,size_t pitch,unsigned phase)
{
   unsigned x,y;
   for (y=0;y<3;y++)
      for (x=0;x<17*bpp;x++)
         if (((const unsigned char*)pixels)[y*pitch+x]!=(unsigned char)(phase+x+7*y)) return false;
   return true;
}
static DWORD WINAPI producer(void *unused)
{
   int seen=0;
   (void)unused;
   while (!retro_atomic_load_acquire_int(&work_stop))
   {
      int command=retro_atomic_load_acquire_int(&work_command);
      if (command==seen) { SwitchToThread();continue; }
      fill(work_frame.pixels,work_frame.pitch,work_frame.phase);
      seen=command;
      vp_job_publish(&job,work_frame.alive);
   }
   return 0;
}
static bool verify(ID3D11Texture2D *source,ID3D11Texture2D *readback,unsigned phase)
{
   D3D11_MAPPED_SUBRESOURCE mapped;
   bool ok;
   ID3D11DeviceContext_CopyResource(real_context,(ID3D11Resource*)readback,(ID3D11Resource*)source);
   if (FAILED(ID3D11DeviceContext_Map(real_context,(ID3D11Resource*)readback,0,D3D11_MAP_READ,0,&mapped))) return false;
   ok=matches(mapped.pData,mapped.RowPitch,phase);
   ID3D11DeviceContext_Unmap(real_context,(ID3D11Resource*)readback,0);
   return ok;
}
int main(void)
{
   ID3D11Device *device=NULL;
   ID3D11Texture2D *readback=NULL;
   HANDLE thread=NULL;
   ID3D11DeviceContextVtbl methods;
   ID3D11DeviceContext proxy;
   D3D11_TEXTURE2D_DESC desc;
   D3D_FEATURE_LEVEL level;
   d3d11_texture_t texture;
   d3d11_video_t video;
   struct texture_image image;
   unsigned format,slot,round,before,serial=0;
   int result=1;
   void *pixels;
   size_t pitch;
   memset(&texture,0,sizeof(texture));
   if (FAILED(D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,0,NULL,0,D3D11_SDK_VERSION,&device,&level,&real_context)))
   {
      if (getenv("REQUIRE_D3D11")) goto done;
      puts("SKIP: no D3D11 device");result=0;goto done;
   }
   memset(&methods,0,sizeof(methods));methods.Map=map;methods.Unmap=unmap;
   methods.CopySubresourceRegion=copy;methods.AddRef=addref;methods.Release=release;
   proxy.lpVtbl=&methods;memset(&video,0,sizeof(video));video.device=device;video.context=&proxy;
   retro_atomic_int_init(&work_command,0);retro_atomic_int_init(&job.status,VP_JOB_IDLE);job.gpu=false;retro_atomic_int_init(&work_stop,0);
   thread=CreateThread(NULL,0,producer,NULL,0,NULL);CHECK(thread);
   for (format=0;format<2;format++)
   {
      memset(&texture.desc,0,sizeof(texture.desc));
      texture.desc.Width=17;texture.desc.Height=3;
      texture.desc.MipLevels=texture.desc.ArraySize=texture.desc.SampleDesc.Count=1;
      texture.desc.Format=format ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM;
      texture.desc.Usage=D3D11_USAGE_DEFAULT;
      CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(device,&texture.desc,NULL,&texture.handle)));
      desc=texture.desc;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
      CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(device,&desc,NULL,&readback)));
      bpp=format ? 8 : 4;texture.lend_bpp=bpp;
      memset(&image,0,sizeof(image));image.width=17;image.height=3;image.fp16=format!=0;
      for (round=0;round<3;round++)
         for (slot=0;slot<2;slot++)
         {
            DWORD began;
            pixels=d3d11_gfx_texture_lend(&video,(uintptr_t)&texture,slot,17*bpp,&pitch);
            CHECK(pixels && pitch>=17*bpp && context_refs==1);
            if (round) CHECK(matches(pixels,pitch,23+(round-1)*2+slot));
            work_frame.pixels=pixels;work_frame.pitch=pitch;work_frame.phase=23+round*2+slot;
            work_frame.alive=1;retro_atomic_store_release_int(&job.status,VP_JOB_RUNNING);
            retro_atomic_store_release_int(&work_command,(int)++serial);
            began=GetTickCount();
            while (retro_atomic_load_acquire_int(&job.status)!=VP_JOB_READY)
            {
               CHECK(GetTickCount()-began<5000);SwitchToThread();
            }
            image.pixels=(uint32_t*)pixels;before=cpu_copies;
            CHECK(d3d11_gfx_update_texture_internal(&video,(uintptr_t)&texture,&image)==VIDEO_TEXTURE_UPDATE_DONE);
            CHECK(cpu_copies==before && verify(texture.handle,readback,23+round*2+slot));
            forced=DXGI_ERROR_WAS_STILL_DRAWING;before=unmaps;
            CHECK(!d3d11_gfx_texture_lend_ready(&video,(uintptr_t)&texture,slot) && unmaps==before);
            forced=0;
         }
      forced=E_FAIL;
      CHECK(d3d11_gfx_texture_lend_ready(&video,(uintptr_t)&texture,0));
      CHECK(!d3d11_gfx_texture_lend(&video,(uintptr_t)&texture,0,17*bpp,&pitch));
      forced=0;
      pixels=d3d11_gfx_texture_lend(&video,(uintptr_t)&texture,1,17*bpp,&pitch);
      CHECK(pixels && matches(pixels,pitch,28));
      work_frame.pixels=pixels;work_frame.pitch=pitch;work_frame.phase=99;work_frame.alive=0;
      retro_atomic_store_release_int(&job.status,VP_JOB_RUNNING);
      retro_atomic_store_release_int(&work_command,(int)++serial);
      {
         DWORD began=GetTickCount();
         while (retro_atomic_load_acquire_int(&job.status)!=VP_JOB_FINISHED)
         {
            CHECK(GetTickCount()-began<5000);SwitchToThread();
         }
      }
      CHECK(verify(texture.handle,readback,28));
      before=unmaps;d3d11_release_texture(&texture);
      CHECK(unmaps==before+1 && !context_refs);
      Release(readback);
   }
   puts("PASS: native D3D11 BGRA8/FP16 loans, repeated remaps, busy/error refusal and cancelled texture preservation");
   result=0;
done:
   if (thread)
   {
      retro_atomic_store_release_int(&work_stop,1);
      if (WaitForSingleObject(thread,10000)!=WAIT_OBJECT_0) ExitProcess(1);
      CloseHandle(thread);
   }
   forced=0;
   d3d11_release_texture(&texture);Release(readback);Release(real_context);Release(device);
   return result;
}
#else
int main(void) { puts("SKIP: D3D11 GPU runtime is Windows-only");return 0; }
#endif
