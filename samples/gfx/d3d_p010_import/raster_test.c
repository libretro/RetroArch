/* Production pixel entries rasterized through an FP16 UI render target. */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "raster.inc"
#ifdef _WIN32
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#define DROP(p) do { if (p) IUnknown_Release((IUnknown*)p); } while (0)
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "Raster FAIL %d: %s\n", __LINE__, #c); failures++; goto end; } } while (0)
static ID3DBlob *compile(const char *source, const char *entry,
      const char *profile, int *compile_failed)
{
   ID3DBlob *blob = NULL, *error = NULL;
   HRESULT hr = D3DCompile(source, strlen(source), "production", NULL, NULL,
         entry, profile, 0, 0, &blob, &error);
   if (FAILED(hr))
   {
      *compile_failed = 1;
      if (error)
         fprintf(stderr, "%s\n", (char*)ID3D10Blob_GetBufferPointer(error));
   }
   DROP(error);
   return blob;
}
static double pq_to_scrgb(double signal)
{
   double p = pow(signal > 0 ? signal : 0, 32.0 / 2523.0);
   double n = p - 3424.0 / 4096.0;
   return 125.0 * pow((n > 0 ? n : 0)
         / (2413.0 / 128.0 - 2392.0 / 128.0 * p), 16384.0 / 2610.0);
}
int main(void)
{
   static const float colors[8][3] = {{0,0,0},{1,1,1},{1,0,0},{0,1,0},
      {0,0,1},{0,1,1},{1,0,1},{.23f,.58f,.91f}};
   static const float nits[4] = {80,203,400,1000};
   ID3D11Device *device = NULL;
   ID3D11DeviceContext *ctx = NULL;
   ID3D11PixelShader *encode = NULL, *decode = NULL;
   ID3D11VertexShader *vs = NULL;
   ID3DBlob *blobs[3] = {NULL,NULL,NULL};
   ID3D11InputLayout *layout = NULL;
   ID3D11Buffer *vertices = NULL;
   ID3D11RasterizerState *raster = NULL;
   D3D11_RASTERIZER_DESC rd;
   D3D11_VIEWPORT viewport;
   static const D3D11_INPUT_ELEMENT_DESC elements[3] = {
      {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
      {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0},
      {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}};
   static const float triangle[3][10] = {
      {-1,1,0,1, 0,0, 1,1,1,1},
      {3,1,0,1, 2,0, 1,1,1,1},
      {-1,-3,0,1, 0,2, 1,1,1,1}};
   UINT stride = sizeof(triangle[0]), offset = 0;
   ID3D11Texture2D *input = NULL, *layer = NULL, *output = NULL, *readback = NULL;
   ID3D11ShaderResourceView *srv[2] = {NULL,NULL}, *none = NULL;
   ID3D11RenderTargetView *rtv[2] = {NULL,NULL}, *no_rtv = NULL;
   ID3D11Buffer *cb[2] = {NULL,NULL};
   ID3D11SamplerState *sampler = NULL;
   D3D_FEATURE_LEVEL level;
   D3D11_TEXTURE2D_DESC td;
   D3D11_BUFFER_DESC bd;
   D3D11_SAMPLER_DESC sd;
   D3D11_MAPPED_SUBRESOURCE map;
   float pixels[8][4], sprite[4];
   uint32_t globals[28];
   unsigned gamut, white, mode, direct, i, ch;
   int failures = 0, compile_failed = 0;
   HRESULT hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
         NULL, 0, D3D11_SDK_VERSION, &device, &level, &ctx);
   if (FAILED(hr))
      hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL, 0,
         NULL, 0, D3D11_SDK_VERSION, &device, &level, &ctx);
   if (FAILED(hr)
      || level < D3D_FEATURE_LEVEL_11_0)
   {
      puts("SKIP HDR UI raster: no SM5 graphics device");
      goto end;
   }
   blobs[0] = compile(sprite_source, "PSMainLinearHDR", "ps_4_0", &compile_failed);
   blobs[1] = compile(composite_source, "PSMain", "ps_5_0", &compile_failed);
   blobs[2] = compile(composite_source, "VSMain", "vs_5_0", &compile_failed);
   if (compile_failed && GetProcAddress(GetModuleHandleA("ntdll.dll"), "wine_get_version"))
   {
      puts("SKIP HDR UI raster: Wine shader compiler lacks required HLSL support");
      goto end;
   }
   CHECK(blobs[0] && blobs[1] && blobs[2]);
   CHECK(SUCCEEDED(ID3D11Device_CreatePixelShader(device,
         ID3D10Blob_GetBufferPointer(blobs[0]), ID3D10Blob_GetBufferSize(blobs[0]), NULL, &encode)));
   CHECK(SUCCEEDED(ID3D11Device_CreatePixelShader(device,
         ID3D10Blob_GetBufferPointer(blobs[1]), ID3D10Blob_GetBufferSize(blobs[1]), NULL, &decode)));
   CHECK(SUCCEEDED(ID3D11Device_CreateVertexShader(device,
         ID3D10Blob_GetBufferPointer(blobs[2]), ID3D10Blob_GetBufferSize(blobs[2]), NULL, &vs)));
   CHECK(SUCCEEDED(ID3D11Device_CreateInputLayout(device, elements, 3,
         ID3D10Blob_GetBufferPointer(blobs[2]), ID3D10Blob_GetBufferSize(blobs[2]), &layout)));
   memset(&bd, 0, sizeof(bd));
   bd.ByteWidth = sizeof(triangle); bd.Usage = D3D11_USAGE_DEFAULT;
   bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
   CHECK(SUCCEEDED(ID3D11Device_CreateBuffer(device, &bd, NULL, &vertices)));
   ID3D11DeviceContext_UpdateSubresource(ctx, (ID3D11Resource*)vertices, 0, NULL, triangle, 0, 0);
   memset(&rd, 0, sizeof(rd));
   rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE;
   rd.DepthClipEnable = TRUE;
   CHECK(SUCCEEDED(ID3D11Device_CreateRasterizerState(device, &rd, &raster)));
   memset(&viewport, 0, sizeof(viewport));
   viewport.Width = 8; viewport.Height = 1; viewport.MaxDepth = 1;
   ID3D11DeviceContext_RSSetViewports(ctx, 1, &viewport);
   ID3D11DeviceContext_RSSetState(ctx, raster);
   ID3D11DeviceContext_IASetInputLayout(ctx, layout);
   ID3D11DeviceContext_IASetPrimitiveTopology(ctx, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
   ID3D11DeviceContext_IASetVertexBuffers(ctx, 0, 1, &vertices, &stride, &offset);
   ID3D11DeviceContext_VSSetShader(ctx, vs, NULL, 0);
   td.Width = 8; td.Height = 1; td.MipLevels = td.ArraySize = 1;
   td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
   td.SampleDesc.Count = 1; td.SampleDesc.Quality = 0;
   td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
   td.CPUAccessFlags = td.MiscFlags = 0;
   CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(device, &td, NULL, &input)));
   td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
   td.BindFlags |= D3D11_BIND_RENDER_TARGET;
   CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(device, &td, NULL, &layer)));
   td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
   CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(device, &td, NULL, &output)));
   td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0;
   td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
   CHECK(SUCCEEDED(ID3D11Device_CreateTexture2D(device, &td, NULL, &readback)));
   CHECK(SUCCEEDED(ID3D11Device_CreateShaderResourceView(device, (ID3D11Resource*)input, NULL, &srv[0])));
   CHECK(SUCCEEDED(ID3D11Device_CreateShaderResourceView(device, (ID3D11Resource*)layer, NULL, &srv[1])));
   CHECK(SUCCEEDED(ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource*)layer, NULL, &rtv[0])));
   CHECK(SUCCEEDED(ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource*)output, NULL, &rtv[1])));
   bd.ByteWidth = sizeof(sprite); bd.Usage = D3D11_USAGE_DEFAULT;
   bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; bd.CPUAccessFlags = bd.MiscFlags = bd.StructureByteStride = 0;
   CHECK(SUCCEEDED(ID3D11Device_CreateBuffer(device, &bd, NULL, &cb[0])));
   bd.ByteWidth = sizeof(globals);
   CHECK(SUCCEEDED(ID3D11Device_CreateBuffer(device, &bd, NULL, &cb[1])));
   sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
   sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
   sd.MipLODBias = 0; sd.MaxAnisotropy = 1; sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
   for (i = 0; i < 4; i++)
      sd.BorderColor[i] = 0;
   sd.MinLOD = 0; sd.MaxLOD = D3D11_FLOAT32_MAX;
   CHECK(SUCCEEDED(ID3D11Device_CreateSamplerState(device, &sd, &sampler)));
   for (i = 0; i < 8; i++)
   {
      double r = colors[i][0] * 12.5, g = colors[i][1] * 12.5, b = colors[i][2] * 12.5;
      pixels[i][0] = (float)(1.660491*r - .5876411*g - .0728499*b);
      pixels[i][1] = (float)(-.1245505*r + 1.1328999*g - .0083494*b);
      pixels[i][2] = (float)(-.0181508*r - .1005789*g + 1.1187297*b);
      pixels[i][3] = (float)(i % 4 + 1) / 4;
   }
   ID3D11DeviceContext_UpdateSubresource(ctx, (ID3D11Resource*)input, 0, NULL, pixels, sizeof(pixels), 0);
   ID3D11DeviceContext_PSSetSamplers(ctx, 0, 1, &sampler);
   for (direct = 0; direct < 2; direct++)
   for (white = 0; white < 4; white++)
   for (gamut = 0; gamut < 4; gamut++)
   for (mode = 1; mode <= 2; mode++)
   {
      sprite[0] = direct ? (float)mode : 0; sprite[1] = nits[white]; sprite[2] = (float)gamut; sprite[3] = 0;
      memset(globals, 0, sizeof(globals));
      memcpy(&globals[20], &nits[white], sizeof(float));
      globals[23] = gamut; globals[26] = mode;
      if (mode == 1)
      {
         float one = 1;
         memcpy(&globals[25], &one, sizeof(one));
      }
      for (i = 0; i < 4; i++)
      {
         float one = 1;
         memcpy(&globals[i * 5], &one, sizeof(one));
      }
      ID3D11DeviceContext_UpdateSubresource(ctx, (ID3D11Resource*)cb[0], 0, NULL, sprite, 0, 0);
      ID3D11DeviceContext_UpdateSubresource(ctx, (ID3D11Resource*)cb[1], 0, NULL, globals, 0, 0);
      ID3D11DeviceContext_VSSetConstantBuffers(ctx, 0, 1, &cb[1]);
      ID3D11DeviceContext_PSSetShader(ctx, encode, NULL, 0);
      ID3D11DeviceContext_PSSetConstantBuffers(ctx, 1, 1, &cb[0]);
      ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &srv[0]);
      ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv[direct], NULL);
      ID3D11DeviceContext_Draw(ctx, 3, 0);
      ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &no_rtv, NULL);
      if (!direct)
      {
         ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &srv[1]);
         ID3D11DeviceContext_PSSetShader(ctx, decode, NULL, 0);
         ID3D11DeviceContext_PSSetConstantBuffers(ctx, 0, 1, &cb[1]);
         ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &rtv[1], NULL);
         ID3D11DeviceContext_Draw(ctx, 3, 0);
         ID3D11DeviceContext_OMSetRenderTargets(ctx, 1, &no_rtv, NULL);
      }
      ID3D11DeviceContext_PSSetShaderResources(ctx, 0, 1, &none);
      ID3D11DeviceContext_CopyResource(ctx, (ID3D11Resource*)readback, (ID3D11Resource*)output);
      CHECK(SUCCEEDED(ID3D11DeviceContext_Map(ctx, (ID3D11Resource*)readback, 0, D3D11_MAP_READ, 0, &map)));
      for (i = 0; i < 8; i++)
      for (ch = 0; ch < 4; ch++)
      {
         double expected = pixels[i][ch], actual = ((const float*)map.pData)[i * 4 + ch];
         double peak = fabs(pixels[i][0]);
         if (fabs(pixels[i][1])
            > peak) peak = fabs(pixels[i][1]);
         if (fabs(pixels[i][2])
            > peak) peak = fabs(pixels[i][2]);
         if (mode == 1 && ch < 3)
         {
            expected = colors[i][ch] * 12.5;
            actual = pq_to_scrgb(actual);
            peak = 12.5;
         }
         if (actual != actual || fabs(actual - expected)
            > (ch == 3 ? .001 : .002 + .003 * peak))
         {
            fprintf(stderr, "HDR raster direct %u gamut %u white %.0f mode %u pixel %u/%u: %.6f != %.6f\n",
                  direct, gamut, nits[white], mode, i, ch, actual, expected);
            failures++;
         }
      }
      ID3D11DeviceContext_Unmap(ctx, (ID3D11Resource*)readback, 0);
   }
   printf("D3D HDR UI raster: 64 cases, %d failures\n", failures);
end:
   if (ctx)
      ID3D11DeviceContext_ClearState(ctx);
   for (i = 0; i < 2; i++)
   {
      DROP(srv[i]);
      DROP(rtv[i]);
      DROP(cb[i]);
   }
   DROP(encode); DROP(decode); DROP(input); DROP(layer); DROP(output); DROP(readback);
   for (i = 0; i < 3; i++)
      DROP(blobs[i]);
   DROP(vs); DROP(layout); DROP(vertices); DROP(raster);
   DROP(sampler); DROP(ctx); DROP(device);
   return failures ? 1 : 0;
}
#else
int main(void)
{
   puts("SKIP HDR UI raster: Windows GPU required");
   return 0;
}
#endif
