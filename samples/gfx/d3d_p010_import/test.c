/* The production D3D11/D3D12 import shaders, checked on synthetic P010.
 * Readback and blocking Map are test-only. */
#include <stdio.h>
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
#include "shaders.inc"
#ifdef _WIN32
#define COBJMACROS
#include <windows.h>
#include <d3d11_4.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>

typedef ID3D11Device *D3D11Device;
typedef ID3D11Resource *D3D11Resource;
typedef ID3D11DeviceContext *D3D11DeviceContext;
typedef ID3D11ShaderResourceView *D3D11ShaderResourceView;
typedef ID3D11UnorderedAccessView *D3D11UnorderedAccessView;
typedef ID3D11Buffer *D3D11Buffer;
typedef struct d3d11_planar d3d11_planar_t;
typedef struct
{
   ID3D11Texture2D *handle;
   D3D11_TEXTURE2D_DESC desc;
   void *sampler;
} d3d11_texture_t;
typedef struct
{
   void *samplers[2][1];
   d3d11_planar_t *planar;
   ID3D11Device *device;
   ID3D11Device5 *device5;
   ID3D11DeviceContext *context;
   ID3D11ComputeShader *planar_cs;
   ID3D11Buffer *planar_cb;
} d3d11_video_t;
#include "convert.inc"
#define D3D11_IMPORT_TEXTURES 8
#define Release(p) do { if (p) IUnknown_Release((IUnknown*)(p)); } while (0)
typedef struct
{
   unsigned num_tex;
   uint64_t tex_handle[D3D11_IMPORT_TEXTURES];
   ID3D11Texture2D *tex[D3D11_IMPORT_TEXTURES];
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
   d3d11_planar_t *next;
};
static bool d3d11_init_texture_ex(D3D11Device device, d3d11_texture_t *texture, bool mip)
{
   (void)mip;
   texture->desc.MipLevels = texture->desc.ArraySize = texture->desc.SampleDesc.Count = 1;
   texture->desc.Usage = D3D11_USAGE_DEFAULT;
   texture->desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
   return SUCCEEDED(ID3D11Device_CreateTexture2D(device, &texture->desc, NULL, &texture->handle));
}
static void d3d11_release_texture(d3d11_texture_t *texture)
{
   if (texture->handle) ID3D11Texture2D_Release(texture->handle);
}
static void d3d11_planar_destroy(d3d11_planar_t *p)
{
   if (p->uav) ID3D11UnorderedAccessView_Release(p->uav);
   free(p->import); free(p);
}
#include "new11.inc"

static int failures;
static unsigned hdr_mode;
static unsigned output_fp16;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); failures++; goto end; } } while (0)
#define DROP(p) do { if (p) { IUnknown_Release((IUnknown*)(p)); p = NULL; } } while (0)

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
   uint16_t words[6];
   uint8_t bytes[6];
   uint32_t constants[20];
   video_color_metadata_t color_metadata;
   float hdr[4];
   float coefficients[6], expected[3], y, u, v;
   unsigned i, row, col, offset = api == 11 ? 0 : 8;
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
   for (i = 0; i < 4; i++)
   {
      words[i] = (uint16_t)(yc << 6);
      bytes[i] = (uint8_t)yc;
   }
   words[4] = (uint16_t)(uc << 6); words[5] = (uint16_t)(vc << 6);
   bytes[4] = (uint8_t)uc; bytes[5] = (uint8_t)vc;
   memset(&data, 0, sizeof(data));
   data.pSysMem = p010 ? (const void*)words : (const void*)bytes;
   data.SysMemPitch = p010 ? 4 : 2;
   CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(device, &td, &data, &input)));
   ID3D11DeviceContext_UpdateSubresource(ctx, (ID3D11Resource*)input,
         0, NULL, data.pSysMem, data.SysMemPitch, 0);
   ID3D11DeviceContext_Flush(ctx);
   CHECK(SUCCEEDED(ID3D11Texture2D_QueryInterface(input, &resource1_iid,
         (void**)&shared)));
   CHECK(SUCCEEDED(IDXGIResource1_CreateSharedHandle(shared, NULL,
         DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, NULL, &handle)));
   CHECK(d3d11_import_tex(&video, &imported, (uint64_t)(uintptr_t)handle,
         2, 2, p010 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12) == 0);
   CHECK(d3d11_import_tex(&video, &imported, (uint64_t)(uintptr_t)handle,
         2, 2, p010 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12) == 0
         && imported.num_tex == 1);
   CHECK(d3d11_import_tex(&video, &imported, (uint64_t)(uintptr_t)handle,
         4, 2, p010 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12) < 0);
   CHECK(d3d11_import_tex(&video, &imported, (uint64_t)(uintptr_t)handle,
         2, 2, p010 ? IMAGE_PIXFMT_NV12 : IMAGE_PIXFMT_P010) < 0);
   views[0] = imported.luma[0];
   views[1] = imported.chroma[0];
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
   if (api == 11)
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
   memcpy((float*)constants + (api == 11 ? 12 : 16), hdr, sizeof(hdr));
   bd.ByteWidth = api == 11 ? 64 : 80;
   bd.Usage = api == 11 ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
   bd.CPUAccessFlags = api == 11 ? D3D11_CPU_ACCESS_WRITE : 0;
   bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
   data.pSysMem = constants;
   data.SysMemPitch = 0;
   CHECK(SUCCEEDED(ID3D11Device_CreateBuffer(device, &bd, &data, &cb)));
   if (api == 11)
   {
      video.context = ctx;
      video.planar_cs = shader;
      video.planar_cb = cb;
      CHECK(d3d11_planar_convert(&video, uav, views, 2, 2, flags,
            true, false, p010 != 0, &color_metadata, output_fp16 != 0));
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
   for (api = 11; api <= 12; api++)
   {
      source = api == 11 ? d3d11_planar_cs_src : d3d12_nv12_cs_src;
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
            printf("D3D%u %s GPU color oracle completed\n", api, depth ? "P010" : "NV12");
         }
         DROP(shader);
      }
      else printf("SKIP GPU: no D3D11 compute device; shader compiled\n");
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
