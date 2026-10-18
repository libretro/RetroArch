/* GPU oracle for shipping D3D11 packed software uploads. */
#include <stdio.h>
#ifdef _WIN32
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <gfx/scaler/pixconv.h>
#define W 257
#define H 256
#define COUNT (W * H)
#define DXGI_FORMAT_EX_A4R4G4B4_UNORM 1000
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %u: %s\n",(unsigned)__LINE__,#x); goto done; } } while (0)
#define Release(p) do { if (p) { (p)->lpVtbl->Release(p); (p)=NULL; } } while (0)
typedef ID3D11DeviceContext *D3D11DeviceContext;
typedef ID3D11Resource *D3D11Resource;
typedef struct {
   D3D11DeviceContext context;
   ID3D11Texture2D * staging[2];
   D3D11_MAPPED_SUBRESOURCE mapped[2];
   unsigned valid, failed;
} d3d11_texture_lend_t;
typedef struct {
   ID3D11Texture2D *handle, *staging;
   ID3D11ShaderResourceView *view;
   ID3D11RenderTargetView *rt_view;
   void *upload_data;
   size_t upload_size;
   d3d11_texture_lend_t *lend;
   unsigned lend_bpp;
   D3D11_TEXTURE2D_DESC desc;
} d3d11_texture_t;
#include "copy.inc"
#include "driver.inc"
#include "../d3d12_menu_mapping/shader.h"
static ID3D11DeviceContext *real_context;
static unsigned force_busy, queries;
static HRESULT STDMETHODCALLTYPE map(ID3D11DeviceContext *context,
      ID3D11Resource *resource,UINT sub,D3D11_MAP mode,UINT flags,
      D3D11_MAPPED_SUBRESOURCE *mapped)
{
   (void)context;
   if (flags != D3D11_MAP_FLAG_DO_NOT_WAIT)
      return E_INVALIDARG;
   queries++;
   if (force_busy) return DXGI_ERROR_WAS_STILL_DRAWING;
   return ID3D11DeviceContext_Map(real_context,resource,sub,mode,flags,mapped);
}
static void STDMETHODCALLTYPE unmap(ID3D11DeviceContext *context,
      ID3D11Resource *resource,UINT sub)
{ (void)context; ID3D11DeviceContext_Unmap(real_context,resource,sub); }
static void STDMETHODCALLTYPE update(ID3D11DeviceContext *context,
      ID3D11Resource *resource,UINT sub,const D3D11_BOX *box,
      const void *data,UINT pitch,UINT slice)
{ (void)context; ID3D11DeviceContext_UpdateSubresource(real_context,resource,sub,box,data,pitch,slice); }
static void STDMETHODCALLTYPE copy_region(ID3D11DeviceContext *context,
      ID3D11Resource *resource,UINT sub,UINT x,UINT y,UINT z,
      ID3D11Resource *source,UINT source_sub,const D3D11_BOX *box)
{ (void)context; ID3D11DeviceContext_CopySubresourceRegion(real_context,resource,sub,x,y,z,source,source_sub,box); }
static void STDMETHODCALLTYPE mips(ID3D11DeviceContext *context,ID3D11ShaderResourceView *view)
{ (void)context; ID3D11DeviceContext_GenerateMips(real_context,view); }
int main(void)
{
   ID3D11Device *device=NULL;
   ID3D11ComputeShader *shader=NULL;
   ID3D11SamplerState *sampler=NULL;
   ID3D11Buffer *output=NULL,*readback=NULL;
   ID3D11UnorderedAccessView *uav=NULL,*null_uav=NULL;
   ID3D11ShaderResourceView *null_srv=NULL;
   d3d11_texture_t texture;
   D3D11_TEXTURE2D_DESC staging_desc;
   D3D11_BUFFER_DESC buffer_desc;
   D3D11_SAMPLER_DESC sampler_desc;
   D3D11_MAPPED_SUBRESOURCE mapped;
   ID3D11DeviceContextVtbl methods;
   ID3D11DeviceContext proxy;
   D3D_FEATURE_LEVEL level;
   uint32_t *source=NULL;
   const float *samples=NULL;
   void *cached=NULL;
   unsigned mode,x,y,i,c,lane,pitch;
   int result=1;
   HRESULT hr;
   memset(&texture,0,sizeof(texture));
   memset(&methods,0,sizeof(methods));
   hr=D3D11CreateDevice(NULL,D3D_DRIVER_TYPE_HARDWARE,NULL,0,NULL,0,
         D3D11_SDK_VERSION,&device,&level,&real_context);
   if (FAILED(hr) || level < D3D_FEATURE_LEVEL_11_0)
   {
      if (getenv("REQUIRE_D3D11")) goto done;
      puts("SKIP: D3D11 compute device unavailable"); result=0; goto done;
   }
   methods.Map=map; methods.Unmap=unmap; methods.UpdateSubresource=update;
   methods.CopySubresourceRegion=copy_region; methods.GenerateMips=mips;
   proxy.lpVtbl=&methods;
   texture.desc.Width=W; texture.desc.Height=H;
   texture.desc.MipLevels=texture.desc.ArraySize=texture.desc.SampleDesc.Count=1;
   texture.desc.Format=DXGI_FORMAT_R10G10B10A2_UNORM;
   texture.desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
   CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(device,&texture.desc,NULL,&texture.handle)));
   CHECK(SUCCEEDED(ID3D11Device_CreateShaderResourceView(device,(ID3D11Resource*)texture.handle,NULL,&texture.view)));
   staging_desc=texture.desc; staging_desc.BindFlags=0;
   staging_desc.Usage=D3D11_USAGE_STAGING; staging_desc.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
   CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(device,&staging_desc,NULL,&texture.staging)));
   CHECK(SUCCEEDED(ID3D11Device_CreateComputeShader(device,menu_mapping_cs,sizeof(menu_mapping_cs),NULL,&shader)));
   memset(&sampler_desc,0,sizeof(sampler_desc));
   sampler_desc.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
   sampler_desc.AddressU=sampler_desc.AddressV=sampler_desc.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
   sampler_desc.MaxAnisotropy=1; sampler_desc.MaxLOD=D3D11_FLOAT32_MAX;
   sampler_desc.ComparisonFunc=D3D11_COMPARISON_ALWAYS;
   CHECK(SUCCEEDED(ID3D11Device_CreateSamplerState(device,&sampler_desc,&sampler)));
   memset(&buffer_desc,0,sizeof(buffer_desc));
   buffer_desc.ByteWidth=COUNT*8*sizeof(float);
   buffer_desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
   buffer_desc.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
   buffer_desc.StructureByteStride=4*sizeof(float);
   CHECK(SUCCEEDED(ID3D11Device_CreateBuffer(device,&buffer_desc,NULL,&output)));
   CHECK(SUCCEEDED(ID3D11Device_CreateUnorderedAccessView(device,(ID3D11Resource*)output,NULL,&uav)));
   buffer_desc.BindFlags=buffer_desc.MiscFlags=buffer_desc.StructureByteStride=0;
   buffer_desc.Usage=D3D11_USAGE_STAGING; buffer_desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
   CHECK(SUCCEEDED(ID3D11Device_CreateBuffer(device,&buffer_desc,NULL,&readback)));
   source=(uint32_t*)malloc((W+3)*H*sizeof(*source)); CHECK(source);
   for (mode=0;mode<3;mode++)
   {
      pitch=(mode ? W+3 : W)*4;
      memset(source,0xcd,(W+3)*H*sizeof(*source));
      for (y=0;y<H;y++) for (x=0;x<W;x++)
      {
         unsigned r,g,b,a;
         i=y*W+x;
         r=(i*37+3+mode*11)&1023; g=((i>>2)+mode*7)&1023;
         b=((i>>6)*19+7+mode*13)&1023; a=(i>>4)&3;
         source[y*(pitch/4)+x]=(r<<20)|(g<<10)|b|(a<<30);
      }
      force_busy=mode != 0;
      d3d11_update_texture(&proxy,W,H,mode ? pitch : 0,
            DXGI_FORMAT_R10G10B10A2_UNORM,source,&texture);
      CHECK(queries == mode+1);
      CHECK(mode ? texture.upload_data != NULL : texture.upload_data == NULL);
      if (mode==2) CHECK(texture.upload_data == cached);
      cached=texture.upload_data;
      ID3D11DeviceContext_CSSetShader(real_context,shader,NULL,0);
      ID3D11DeviceContext_CSSetShaderResources(real_context,0,1,&texture.view);
      ID3D11DeviceContext_CSSetSamplers(real_context,0,1,&sampler);
      ID3D11DeviceContext_CSSetUnorderedAccessViews(real_context,0,1,&uav,NULL);
      ID3D11DeviceContext_Dispatch(real_context,(W+7)/8,(H+7)/8,1);
      ID3D11DeviceContext_CSSetShaderResources(real_context,0,1,&null_srv);
      ID3D11DeviceContext_CSSetUnorderedAccessViews(real_context,0,1,&null_uav,NULL);
      ID3D11DeviceContext_CopyResource(real_context,(ID3D11Resource*)readback,(ID3D11Resource*)output);
      CHECK(SUCCEEDED(ID3D11DeviceContext_Map(real_context,(ID3D11Resource*)readback,0,D3D11_MAP_READ,0,&mapped)));
      samples=(const float*)mapped.pData;
      for (i=0;i<COUNT;i++) for (lane=0;lane<2;lane++) for (c=0;c<4;c++)
      {
         unsigned expected=c==0 ? (i*37+3+mode*11)&1023
            : c==1 ? ((i>>2)+mode*7)&1023
            : c==2 ? ((i>>6)*19+7+mode*13)&1023 : 1023;
         float wanted=expected/1023.0f;
         float actual=samples[i*8+lane*4+c];
         if (!(fabs(actual-wanted)<0.0001))
         {
            fprintf(stderr,"FAIL mode %u pixel %u lane %u channel %u: %.6f != %.6f\n",mode,i,lane,c,actual,wanted);
            goto done;
         }
      }
      ID3D11DeviceContext_Unmap(real_context,(ID3D11Resource*)readback,0); samples=NULL;
      printf("PASS: D3D11 GPU mode %u, all 10-bit channel/X codes, Load and linear SampleLevel\n",mode);
   }
   result=0;
done:
   if (samples) ID3D11DeviceContext_Unmap(real_context,(ID3D11Resource*)readback,0);
   if (real_context) ID3D11DeviceContext_ClearState(real_context);
   free(source); d3d11_release_texture(&texture);
   Release(readback); Release(output); Release(uav); Release(sampler); Release(shader);
   Release(real_context); Release(device);
   return result;
}
#else
int main(void)
{
   puts("SKIP: D3D11 GPU runtime requires Windows or mingw + Wine");
   return 0;
}
#endif
