/* The production D3D11/D3D12 import shaders, checked on synthetic P010.
 * Readback and blocking Map are test-only. */
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <boolean.h>
#include <formats/image_yuv_blit.h>
#include "hdr_reference.h"
#include "fp16_reference.h"
#include "../../../gfx/common/video_import_color.h"
#include <formats/image.h>
#include "../../../gfx/video_defines.h"
#include "../../../gfx/video_shader_parse.h"
#include "shaders.inc"
#ifdef _WIN32
#define COBJMACROS
#include <windows.h>
#include <d3d11_4.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>

typedef ID3D11Device *D3D11Device;
typedef ID3D11Texture2D *D3D11Texture2D;
#include "../../../gfx/common/media_history.h"
typedef ID3D11Resource *D3D11Resource;
typedef ID3D11DeviceContext *D3D11DeviceContext;
typedef ID3D11ShaderResourceView *D3D11ShaderResourceView;
typedef ID3D11UnorderedAccessView *D3D11UnorderedAccessView;
typedef ID3D11Buffer *D3D11Buffer;
typedef struct d3d11_planar d3d11_planar_t;
typedef struct
{
   ID3D11Texture2D *handle;
   ID3D11ShaderResourceView *view;
   ID3D11RenderTargetView *rt_view;
   D3D11ShaderResourceView *media_versions;
   unsigned media_version_count;
   bool media_borrowable;
   D3D11_TEXTURE2D_DESC desc;
   void *sampler;
} d3d11_texture_t;
typedef struct { unsigned history_size; } pool_preset_t;
typedef struct { pool_preset_t *shader_preset;bool history_borrowed;d3d11_texture_t history[2]; } d3d11_chain_t;
typedef struct
{
   d3d11_chain_t *media_shader_chain;
   void *samplers[2][1];
   d3d11_planar_t *planar;
   ID3D11Device *device;
   ID3D11Device5 *device5;
   ID3D11DeviceContext *context;
   ID3D11ComputeShader *planar_cs;
   ID3D11Buffer *planar_cb;
   ID3D11ComputeShader *planar_array_cs;
} d3d11_video_t;
#include "convert.inc"
#define D3D11_IMPORT_TEXTURES 64
#define Release(p) do { if (p) IUnknown_Release((IUnknown*)(p)); } while (0)
typedef struct
{
   unsigned num_tex;
   uint64_t tex_handle[D3D11_IMPORT_TEXTURES];
   ID3D11Texture2D *tex[D3D11_IMPORT_TEXTURES];
   bool array[D3D11_IMPORT_TEXTURES];
   unsigned slice[D3D11_IMPORT_TEXTURES];
   ID3D11ShaderResourceView *luma[D3D11_IMPORT_TEXTURES];
   ID3D11ShaderResourceView *chroma[D3D11_IMPORT_TEXTURES];
} d3d11_import_t;
static const GUID d3d11_iid_texture2d =
   {0x6f15aaf2,0xd208,0x4e89,{0x9a,0xb4,0x48,0x95,0x35,0xd3,0x4f,0x9c}};
static const GUID device5_iid =
   {0x8ffde202,0xa0e7,0x45df,{0x9e,0x01,0xe8,0x37,0x80,0x1b,0x5e,0xa0}};
static const GUID resource1_iid =
   {0x30961379,0x4609,0x4a41,{0x99,0x8e,0x54,0xfe,0x56,0x7e,0xe0,0xc1}};
#include "import.inc"
#undef Release
#define RARCH_FILTER_NEAREST 0
#define RARCH_FILTER_LINEAR 1
#define RARCH_WRAP_EDGE 0
struct d3d11_planar
{
   d3d11_texture_t *texture;
   d3d11_import_t *import;
   bool interleaved;
   ID3D11UnorderedAccessView *uav;
   ID3D11UnorderedAccessView *media_uavs[GFX_MAX_FRAME_HISTORY+2];
   d3d11_planar_t *next;
};
static bool d3d11_init_texture_ex(D3D11Device device, d3d11_texture_t *texture, bool mip)
{
   (void)mip;
   texture->desc.MipLevels = texture->desc.ArraySize = texture->desc.SampleDesc.Count = 1;
   texture->desc.Usage = D3D11_USAGE_DEFAULT;
   texture->desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
   if(FAILED(ID3D11Device_CreateTexture2D(device, &texture->desc, NULL, &texture->handle)))return false;
   return SUCCEEDED(ID3D11Device_CreateShaderResourceView(device,(ID3D11Resource*)texture->handle,NULL,&texture->view));
}
static void d3d11_release_texture(d3d11_texture_t *texture)
{
   unsigned i;
   for(i=0;i<texture->media_version_count;i++)ID3D11ShaderResourceView_Release(texture->media_versions[i]);
   free(texture->media_versions);
   if (texture->handle) ID3D11Texture2D_Release(texture->handle);
   if (texture->view) ID3D11ShaderResourceView_Release(texture->view);
   if (texture->rt_view) ID3D11RenderTargetView_Release(texture->rt_view);
}
static void d3d11_planar_destroy(d3d11_planar_t *p)
{
   unsigned i;
   for(i=0;i<GFX_MAX_FRAME_HISTORY+2;i++)if(p->media_uavs[i])ID3D11UnorderedAccessView_Release(p->media_uavs[i]);
   if (p->uav) ID3D11UnorderedAccessView_Release(p->uav);
   free(p->import); free(p);
}
#include "new11.inc"
#define Release(p) do { if (p) IUnknown_Release((IUnknown*)(p)); } while (0)
#include "pool11.inc"
#undef Release

static int failures;
static unsigned hdr_mode;
static unsigned output_fp16;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); failures++; goto end; } } while (0)
#define DROP(p) do { if (p) { IUnknown_Release((IUnknown*)(p)); p = NULL; } } while (0)

/* Exercise the real pool selector and production conversion on native COM
 * resources. Clearing each destination first proves old readers cannot alias it. */
static void pooled_conversion(d3d11_video_t *video,d3d11_planar_t *p,
      ID3D11Texture2D *readback,ID3D11ShaderResourceView **planes,
      unsigned flags,bool p010,const video_color_metadata_t *color,bool fp16,
      const unsigned *region)
{
   static bool checked[2][2];
   d3d11_chain_t chain;
   pool_preset_t preset={1};
   unsigned i,j,row,bytes=fp16?16:8,warm=0;
   uint8_t expected[32];
   D3D11_MAPPED_SUBRESOURCE mapped;
   if(checked[region?1:0][fp16?1:0])return;
   checked[region?1:0][fp16?1:0]=true;
   memset(&chain,0,sizeof(chain));chain.shader_preset=&preset;chain.history_borrowed=true;
   video->media_shader_chain=&chain;p->texture->media_borrowable=true;
   assert(d3d11_media_pool_begin(p->texture));
   for(i=0;i<2;i++){chain.history[i].view=p->texture->view;ID3D11ShaderResourceView_AddRef(chain.history[i].view);}
   ID3D11DeviceContext_CopyResource(video->context,(ID3D11Resource*)readback,(ID3D11Resource*)p->texture->handle);
   assert(SUCCEEDED(ID3D11DeviceContext_Map(video->context,(ID3D11Resource*)readback,0,D3D11_MAP_READ,0,&mapped)));
   for(row=0;row<2;row++)memcpy(expected+row*bytes,(const uint8_t*)mapped.pData+row*mapped.RowPitch,bytes);
   ID3D11DeviceContext_Unmap(video->context,(ID3D11Resource*)readback,0);
   for(i=0;i<12;i++)
   {
      D3D11ShaderResourceView destination;
      D3D11UnorderedAccessView target=d3d11_planar_writable(video,p,&destination);
      const float clear[4]={0.125f,0.75f,0.25f,0.5f};
      assert(target && destination);
      ID3D11DeviceContext_ClearUnorderedAccessViewFloat(video->context,target,clear);
      for(j=0;j<2;j++)
      {
         ID3D11Resource *reader=NULL;
         ID3D11ShaderResourceView_GetResource(chain.history[j].view,&reader);
         ID3D11DeviceContext_CopyResource(video->context,(ID3D11Resource*)readback,reader);
         ID3D11Resource_Release(reader);
         assert(SUCCEEDED(ID3D11DeviceContext_Map(video->context,(ID3D11Resource*)readback,0,D3D11_MAP_READ,0,&mapped)));
         for(row=0;row<2;row++)assert(!memcmp(expected+row*bytes,(const uint8_t*)mapped.pData+row*mapped.RowPitch,bytes));
         ID3D11DeviceContext_Unmap(video->context,(ID3D11Resource*)readback,0);
      }
      assert(d3d11_planar_convert(video,target,planes,2,2,flags,true,false,p010,color,fp16,region));
      d3d11_media_publish(p->texture,destination);
      ID3D11ShaderResourceView_Release(chain.history[1].view);
      chain.history[1].view=chain.history[0].view;chain.history[0].view=destination;ID3D11ShaderResourceView_AddRef(destination);
      if(i==4)warm=p->texture->media_version_count;
      if(i>4)assert(p->texture->media_version_count==warm);
   }
   assert(warm<=3);
   for(i=0;i<2;i++)ID3D11ShaderResourceView_Release(chain.history[i].view);
   video->media_shader_chain=NULL;
   puts(fp16?"D3D11 FP16 pooled conversion: immutable GPU readers and bounded reuse passed":"D3D11 RGBA8 pooled conversion: immutable GPU readers and bounded reuse passed");
}

/* Separate R8/RG8 or R16/RG16 planes exercise compute even on devices
 * without the optional native NV12/P010 texture formats. */
static void pooled_planes(ID3D11Device *device,ID3D11DeviceContext *ctx,
      ID3D11ComputeShader *shader,bool fp16)
{
   d3d11_video_t video;
   d3d11_planar_t *p;
   D3D11_TEXTURE2D_DESC td;
   D3D11_BUFFER_DESC bd;
   D3D11_SUBRESOURCE_DATA data;
   ID3D11Texture2D *planes[2]={NULL,NULL},*readback=NULL;
   ID3D11ShaderResourceView *views[3]={NULL,NULL,NULL};
   uint8_t bytes[4]={128,128,128,128};
   uint16_t words[4]={32768,32768,32768,32768};
   video_color_metadata_t color;
   unsigned i;
   memset(&video,0,sizeof(video));memset(&color,0,sizeof(color));
   video.device=device;video.context=ctx;video.planar_cs=shader;
   memset(&td,0,sizeof(td));td.Width=td.Height=2;
   td.MipLevels=td.ArraySize=td.SampleDesc.Count=1;
   td.BindFlags=D3D11_BIND_SHADER_RESOURCE;
   memset(&data,0,sizeof(data));data.pSysMem=fp16?(const void*)words:(const void*)bytes;
   for(i=0;i<2;i++)
   {
      td.Format=fp16?(i?DXGI_FORMAT_R16G16_UNORM:DXGI_FORMAT_R16_UNORM):(i?DXGI_FORMAT_R8G8_UNORM:DXGI_FORMAT_R8_UNORM);
      td.Width=td.Height=i?1:2;data.SysMemPitch=fp16?4:2;
      assert(SUCCEEDED(ID3D11Device_CreateTexture2D(device,&td,&data,&planes[i])));
      assert(SUCCEEDED(ID3D11Device_CreateShaderResourceView(device,(ID3D11Resource*)planes[i],NULL,&views[i])));
   }
   memset(&bd,0,sizeof(bd));bd.ByteWidth=64;bd.Usage=D3D11_USAGE_DYNAMIC;
   bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
   assert(SUCCEEDED(ID3D11Device_CreateBuffer(device,&bd,NULL,&video.planar_cb)));
   p=d3d11_import_new(&video,2,2,TEXTURE_FILTER_LINEAR,fp16?DXGI_FORMAT_R16G16B16A16_FLOAT:DXGI_FORMAT_R8G8B8A8_UNORM);
   assert(p);
   td=p->texture->desc;td.BindFlags=0;td.Usage=D3D11_USAGE_STAGING;td.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
   assert(SUCCEEDED(ID3D11Device_CreateTexture2D(device,&td,NULL,&readback)));
   if(fp16){color.transfer=16;color.primaries=color.matrix=9;color.max_cll=1000;}
   assert(d3d11_planar_convert(&video,p->uav,views,2,2,0,true,false,fp16,&color,fp16,NULL));
   pooled_conversion(&video,p,readback,views,0,fp16,&color,fp16,NULL);
   ID3D11DeviceContext_ClearState(ctx);
   for(i=0;i<2;i++){ID3D11ShaderResourceView_Release(views[i]);ID3D11Texture2D_Release(planes[i]);}
   ID3D11Texture2D_Release(readback);ID3D11Buffer_Release(video.planar_cb);
   d3d11_release_texture(p->texture);free(p->texture);d3d11_planar_destroy(p);
}

static void color(ID3D11Device *device, ID3D11DeviceContext *ctx,
      ID3D11ComputeShader *shader, unsigned api, unsigned p010,
      unsigned flags, unsigned yc, unsigned uc, unsigned vc)
{
   ID3D11Texture2D *input = NULL, *output = NULL, *readback = NULL;
   ID3D11ShaderResourceView *views[3] = { NULL, NULL, NULL };
   ID3D11UnorderedAccessView *uav = NULL, *no_uav = NULL;
   ID3D11Buffer *cb = NULL;
   IDXGIResource1 *shared = NULL;
   HANDLE handle = NULL;
   d3d11_import_t imported;
   D3D11_TEXTURE2D_DESC td;
   D3D11_BUFFER_DESC bd;
   D3D11_SUBRESOURCE_DATA data;
   D3D11_MAPPED_SUBRESOURCE mapped;
   uint16_t words[24];
   uint8_t empty[48] = {0};
   unsigned region[4] = {2, 2, 0, 0};
   bool array = api == 13;
   uint8_t bytes[24];
   uint32_t constants[20];
   video_color_metadata_t color_metadata;
   float hdr[4];
   float coefficients[6], expected[3], y, u, v;
   unsigned i, row, col, offset = api != 12 ? 0 : 8;
   float *values = (float*)constants + offset;
   HRESULT hr;
   d3d11_video_t video;
   d3d11_planar_t *allocated = NULL;

   memset(&video, 0, sizeof(video));
   memset(&imported, 0, sizeof(imported));
   video.device = device;
   CHECK(SUCCEEDED(ID3D11Device_QueryInterface(device, &device5_iid,
         (void**)&video.device5)));
   memset(&td, 0, sizeof(td));
   td.Width = td.Height = 2;
   td.MipLevels = td.ArraySize = td.SampleDesc.Count = 1;
   td.Format = p010 ? DXGI_FORMAT_P010 : DXGI_FORMAT_NV12;
   td.Usage = D3D11_USAGE_DEFAULT;
   td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
   td.MiscFlags = D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
   if (array)
   {
      td.Width = td.Height = 4;
      td.ArraySize = 3;
      td.BindFlags |= D3D11_BIND_DECODER;
   }
   memset(words, 0, sizeof(words)); memset(bytes, 0, sizeof(bytes));
   for (i = 0; i < 4; i++)
   {
      words[array ? 10 + (i / 2) * 4 + i % 2 : i] = (uint16_t)(yc << 6);
      bytes[array ? 10 + (i / 2) * 4 + i % 2 : i] = (uint8_t)yc;
   }
   words[array ? 22 : 4] = (uint16_t)(uc << 6); words[array ? 23 : 5] = (uint16_t)(vc << 6);
   bytes[array ? 22 : 4] = (uint8_t)uc; bytes[array ? 23 : 5] = (uint8_t)vc;
   memset(&data, 0, sizeof(data));
   data.pSysMem = p010 ? (const void*)words : (const void*)bytes;
   data.SysMemPitch = (p010 ? 4 : 2) * (array ? 2 : 1);
   CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(device, &td, array ? NULL : &data, &input)));
   if (array)
      for (i = 0; i < 3; i++)
         ID3D11DeviceContext_UpdateSubresource(ctx, (ID3D11Resource*)input,
               i, NULL, empty, data.SysMemPitch, 0);
   ID3D11DeviceContext_UpdateSubresource(ctx, (ID3D11Resource*)input,
         array ? 1 : 0, NULL, data.pSysMem, data.SysMemPitch, 0);
   ID3D11DeviceContext_Flush(ctx);
   CHECK(SUCCEEDED(ID3D11Texture2D_QueryInterface(input, &resource1_iid,
         (void**)&shared)));
   CHECK(SUCCEEDED(IDXGIResource1_CreateSharedHandle(shared, NULL,
         DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, NULL, &handle)));
   CHECK(d3d11_import_tex(&video, &imported, (uint64_t)(uintptr_t)handle,
         2, 2, p010 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12, array ? 1 : 0, array ? 2 : 0, array ? 2 : 0) == 0);
   CHECK(d3d11_import_tex(&video, &imported, (uint64_t)(uintptr_t)handle,
         2, 2, p010 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12, array ? 1 : 0, array ? 2 : 0, array ? 2 : 0) == 0
         && imported.num_tex == 1);
   CHECK(d3d11_import_tex(&video, &imported, (uint64_t)(uintptr_t)handle,
         4, 2, p010 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12, array ? 1 : 0, array ? 2 : 0, array ? 2 : 0) < 0);
   CHECK(d3d11_import_tex(&video, &imported, (uint64_t)(uintptr_t)handle,
         2, 2, p010 ? IMAGE_PIXFMT_NV12 : IMAGE_PIXFMT_P010, array ? 1 : 0, array ? 2 : 0, array ? 2 : 0) < 0);
   if (array)
   {
      CHECK(d3d11_import_tex(&video, &imported, (uint64_t)(uintptr_t)handle,
            2, 2, p010 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12, 0, 2, 2) == 1);
      CHECK(imported.num_tex == 2 && imported.slice[0] == 1 && imported.slice[1] == 0);
      CHECK(d3d11_import_tex(&video, &imported, (uint64_t)(uintptr_t)handle,
            2, 2, p010 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12, 3, 2, 2) < 0);
      CHECK(d3d11_import_tex(&video, &imported, (uint64_t)(uintptr_t)handle,
            2, 2, p010 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12, 1, 1, 2) < 0);
      CHECK(d3d11_import_tex(&video, &imported, (uint64_t)(uintptr_t)handle,
            2, 2, p010 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12, 1, ~0u, 2) < 0);
      CHECK(imported.num_tex == 2);
   }
   views[0] = imported.luma[0];
   views[1] = imported.chroma[0];
   td.Width = td.Height = 2;
   td.ArraySize = 1;
   td.MiscFlags = 0;
   td.Format = output_fp16 ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
   td.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
   allocated = d3d11_import_new(&video, 2, 2, TEXTURE_FILTER_LINEAR, td.Format);
   CHECK(allocated && allocated->texture->desc.Format == td.Format);
   output = allocated->texture->handle; ID3D11Texture2D_AddRef(output);
   uav = allocated->uav; ID3D11UnorderedAccessView_AddRef(uav);
   td.Usage = D3D11_USAGE_STAGING;
   td.BindFlags = 0;
   td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
   CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(device, &td, NULL, &readback)));
   memset(constants, 0, sizeof(constants));
   memset(&color_metadata, 0, sizeof(color_metadata));
   if (hdr_mode)
   {
      color_metadata.transfer = hdr_mode == 1 ? 16 : 18;
      color_metadata.primaries = color_metadata.matrix = 9;
      color_metadata.max_cll = 1000;
   }
   CHECK(video_import_color(&flags, &color_metadata, coefficients, hdr));
   memcpy(values, coefficients, sizeof(coefficients));
   if (api != 12)
   {
      values[6] = 1.0f;
      constants[8] = constants[9] = 2;
      constants[10] = p010;
      constants[11] = !!(flags & IMAGE_YUV_FLAG_FULL_RANGE);
   }
   else
   {
      constants[0] = constants[1] = 2;
      constants[2] = p010;
      constants[3] = !!(flags & IMAGE_YUV_FLAG_FULL_RANGE);
   }
   memset(&bd, 0, sizeof(bd));
   if (output_fp16) hdr[3] = -1.0f;
   memcpy((float*)constants + (api != 12 ? 12 : 16), hdr, sizeof(hdr));
   bd.ByteWidth = api == 11 ? 64 : 80;
   bd.Usage = api != 12 ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
   bd.CPUAccessFlags = api != 12 ? D3D11_CPU_ACCESS_WRITE : 0;
   bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
   data.pSysMem = constants;
   data.SysMemPitch = 0;
   CHECK(SUCCEEDED(ID3D11Device_CreateBuffer(device, &bd, &data, &cb)));
   if (api != 12)
   {
      video.context = ctx;
      video.planar_cs = shader;
      video.planar_cb = cb;
      video.planar_array_cs = array ? shader : NULL;
      CHECK(d3d11_planar_convert(&video, uav, views, 2, 2, flags,
            true, false, p010 != 0, &color_metadata, output_fp16 != 0, array ? region : NULL));
   }
   else
   {
      ID3D11DeviceContext_CSSetShader(ctx, shader, NULL, 0);
      ID3D11DeviceContext_CSSetConstantBuffers(ctx, 0, 1, &cb);
      ID3D11DeviceContext_CSSetShaderResources(ctx, 0, 3, views);
      ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx, 0, 1, &uav, NULL);
      ID3D11DeviceContext_Dispatch(ctx, 1, 1, 1);
      ID3D11DeviceContext_CSSetUnorderedAccessViews(ctx, 0, 1, &no_uav, NULL);
   }
   ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource*)readback,
         (ID3D11Resource*)output);
   hr = ID3D11DeviceContext_Map(ctx, (ID3D11Resource*)readback, 0,
         D3D11_MAP_READ, 0, &mapped);
   CHECK(SUCCEEDED(hr));
   /* Independent code-value oracle, before R16 UNORM sampling. */
   y = (float)yc / (p010 ? ((flags & IMAGE_YUV_FLAG_FULL_RANGE) ? 1023.0f : 1020.0f) : 255.0f);
   u = (float)uc / (p010 ? ((flags & IMAGE_YUV_FLAG_FULL_RANGE) ? 1023.0f : 1020.0f) : 255.0f);
   v = (float)vc / (p010 ? ((flags & IMAGE_YUV_FLAG_FULL_RANGE) ? 1023.0f : 1020.0f) : 255.0f);
   u -= p010 && (flags & IMAGE_YUV_FLAG_FULL_RANGE) ? 512.0f / 1023.0f : 128.0f / 255.0f;
   v -= p010 && (flags & IMAGE_YUV_FLAG_FULL_RANGE) ? 512.0f / 1023.0f : 128.0f / 255.0f;
   expected[0] = coefficients[0] * y + coefficients[1] + coefficients[2] * v;
   expected[1] = coefficients[0] * y + coefficients[1] + coefficients[3] * u + coefficients[4] * v;
   expected[2] = coefficients[0] * y + coefficients[1] + coefficients[5] * u;
   if (hdr_mode)
      CHECK(hdr_reference_pixel(p010, !!(flags & IMAGE_YUV_FLAG_FULL_RANGE),
            color_metadata.transfer, yc, uc, vc, expected));
   for (row = 0; row < 2; row++)
      for (col = 0; col < 2; col++)
      {
         const uint8_t *pixel = (const uint8_t*)mapped.pData + row * mapped.RowPitch + col * (output_fp16 ? 8 : 4);
         if (output_fp16)
         {
            fp16_reference(p010 ? 10 : 8, !!(flags & IMAGE_YUV_FLAG_FULL_RANGE),
                  color_metadata.transfer, flags, yc, uc, vc, expected);
            CHECK(fp16_reference_check((const uint16_t*)pixel, expected));
            continue;
         }
         for (i = 0; i < 3; i++)
         {
            float ref = expected[i] < 0 ? 0 : expected[i] > 1 ? 1 : expected[i];
            if (fabs((double)pixel[i] - ref * 255.0) > 1.1)
            {
               fprintf(stderr, "D3D%u P010=%u flags=%u channel=%u: %u != %.2f\n",
                     api, p010, flags, i, pixel[i], ref * 255.0);
               failures++;
            }
         }
         if (pixel[3] != 255) failures++;
      }
   ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource*)readback, 0);
   if(api!=12)pooled_conversion(&video,allocated,readback,views,flags,p010!=0,&color_metadata,output_fp16!=0,array?region:NULL);
end:
   ID3D11DeviceContext_ClearState(ctx);
   DROP(cb); DROP(uav);
   for (i = 0; i < imported.num_tex; i++)
   {
      DROP(imported.luma[i]); DROP(imported.chroma[i]); DROP(imported.tex[i]);
   }
   DROP(video.device5); DROP(shared);
   if (handle) CloseHandle(handle);
   DROP(input); DROP(output); DROP(readback);
   if (allocated)
   {
      d3d11_release_texture(allocated->texture);
      free(allocated->texture);
      d3d11_planar_destroy(allocated);
   }
}

int main(void)
{
   ID3D11Device *device = NULL;
   ID3D11DeviceContext *ctx = NULL;
   ID3D11ComputeShader *shader = NULL;
   ID3DBlob *blob = NULL, *errors = NULL;
   D3D_FEATURE_LEVEL level;
   unsigned api, flags, support = 0, depth;
   const char *source;
   HRESULT hr;
   for (api = 11; api <= 13; api++)
   {
      source = api == 11 ? d3d11_planar_cs_src
         : api == 12 ? d3d12_nv12_cs_src : d3d11_array_cs_src;
      hr = D3DCompile(source, strlen(source), "production", NULL, NULL,
            "main", "cs_5_0", 0, 0, &blob, &errors);
      if (FAILED(hr))
      {
         if (errors) fprintf(stderr, "%s\n", (char*)ID3D10Blob_GetBufferPointer(errors));
         failures++;
         goto end;
      }
      if (!device)
      {
         hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
               NULL, 0, D3D11_SDK_VERSION, &device, &level, &ctx);
         if (FAILED(hr)) hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP,
               NULL, 0, NULL, 0, D3D11_SDK_VERSION, &device, &level, &ctx);
      }
      if (device && level >= D3D_FEATURE_LEVEL_11_0)
      {
         CHECK(SUCCEEDED(ID3D11Device_CreateComputeShader(device,
               ID3D10Blob_GetBufferPointer(blob), ID3D10Blob_GetBufferSize(blob), NULL, &shader)));
         if(api==11){pooled_planes(device,ctx,shader,false);pooled_planes(device,ctx,shader,true);}
         if(getenv("POOL_ONLY")){DROP(shader);DROP(blob);DROP(errors);break;}
         for (depth = 0; depth < 2; depth++)
         {
            hr = ID3D11Device_CheckFormatSupport(device,
                  depth ? DXGI_FORMAT_P010 : DXGI_FORMAT_NV12, &support);
            if (FAILED(hr) || !(support & D3D11_FORMAT_SUPPORT_SHADER_LOAD))
            {
               printf("SKIP GPU D3D%u %s format\n", api, depth ? "P010" : "NV12");
               continue;
            }
            for (output_fp16 = 0; output_fp16 < 2; output_fp16++)
      for (hdr_mode = 0; hdr_mode < 3; hdr_mode++)
      for (flags = 0; flags < 4; flags++)
            {
               unsigned full = flags & IMAGE_YUV_FLAG_FULL_RANGE;
               unsigned scale = depth ? 4 : 1;
               unsigned center = 128 * scale;
               color(device, ctx, shader, api, depth, flags, full ? 0 : 16 * scale, center, center);
               color(device, ctx, shader, api, depth, flags, full ? (depth ? 1023 : 255) : 235 * scale, center, center);
               color(device, ctx, shader, api, depth, flags, 127 * scale, center, center);
               color(device, ctx, shader, api, depth, flags, 110 * scale, 90 * scale, 170 * scale);
            }
            printf("D3D%u%s %s GPU color oracle completed\n", api == 13 ? 11 : api,
                  api == 13 ? " decoder array" : "", depth ? "P010" : "NV12");
         }
         DROP(shader);
      }
      else
      {
         printf("SKIP GPU: no D3D11 compute device; shader compiled\n");
         if(getenv("REQUIRE_D3D11"))failures++;
      }
      DROP(blob); DROP(errors);
   }
end:
   DROP(shader); DROP(blob); DROP(errors); DROP(ctx); DROP(device);
   printf("P010 import shaders: %d failures\n", failures);
   return failures ? 1 : 0;
}
#else
int main(void)
{
   if (!strstr(d3d11_planar_cs_src, "65535.0 / 65280.0")
         || !strstr(d3d12_nv12_cs_src, "65535.0 / 65472.0")) return 1;
   puts("SKIP GPU P010 oracle: requires Windows; production shaders extracted");
   return 0;
}
#endif
