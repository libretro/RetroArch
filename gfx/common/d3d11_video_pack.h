/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
/* Included by d3d11_video_decode.c. */
#include <stdio.h>
#include <compat/msvc.h>

/* Views retain the decoder allocation; commands use its owning context.
 * Packing precedes any later decode into the same slice on that queue. */
static bool d3d11vd_pack_dpb_views(d3d11vd_dec_t *d)
{
   D3D11_SHADER_RESOURCE_VIEW_DESC view;
   unsigned i, plane;
   if (!d->pack_dpb) return false;
   memset(&view, 0, sizeof(view));
   view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
   view.Texture2DArray.MipLevels = view.Texture2DArray.ArraySize = 1;
   for (i = 0; i < d->slots; i++)
   {
      view.Texture2DArray.FirstArraySlice = i;
      for (plane = 0; plane < 2; plane++)
      {
         view.Format = d->bit_depth == 10
            ? (plane ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R16_UNORM)
            : (plane ? DXGI_FORMAT_R8G8_UNORM : DXGI_FORMAT_R8_UNORM);
         if (FAILED(d->dev->lpVtbl->CreateShaderResourceView(d->dev,
                  (ID3D11Resource*)d->dpb, &view, &d->pack_dpb_views[i][plane])))
            goto fallback;
      }
   }
   return true;
fallback:
   for (i = 0; i < D3D11VD_MAX_SLOTS; i++)
      for (plane = 0; plane < 2; plane++)
         D3D11VD_RELEASE(d->pack_dpb_views[i][plane]);
   d->pack_dpb = false;
   return false;
}

static bool d3d11vd_pack_init(d3d11vd_dec_t *d)
{
   static const char source[] =
      "T<float> Y:register(t0);"
      "T<float2> C:register(t1);"
      "RWTexture2D<float4> O:register(u0);"
      "[numthreads(8,8,1)]void main(uint3 p:SV_DispatchThreadID){"
      "uint w,h;O.GetDimensions(w,h);if(p.x>=w||p.y>=h)return;"
      "O[p.xy]=float4(round(float3(Y.Load(YP),"
      "C.Load(CP))*S),1);}";
   D3D_SHADER_MACRO macros[5];
   char y_position[80], c_position[80];
   D3D11_TEXTURE2D_DESC desc;
   D3D11_SHADER_RESOURCE_VIEW_DESC view;
   ID3DBlob *code = NULL;
   HMODULE compiler;
   pD3DCompile compile;
   HRESULT hr;

   if (d3d11vd_pack_dpb_views(d))
   {
      snprintf(y_position, sizeof(y_position), "int4(p.xy+uint2(%u,%u),0,0)",
            d->crop_x, d->crop_y);
      snprintf(c_position, sizeof(c_position), "int4((p.xy+uint2(%u,%u))/2,0,0)",
            d->crop_x, d->crop_y);
   }
   else
   {
      memset(&desc, 0, sizeof(desc));
      desc.Width = d->visible_w;
      desc.Height = d->visible_h;
      desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
      desc.Format = d->format;
      desc.Usage = D3D11_USAGE_DEFAULT;
      desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      if (FAILED(d->dev->lpVtbl->CreateTexture2D(d->dev, &desc, NULL,
               &d->pack_input)))
         return false;
      memset(&view, 0, sizeof(view));
      view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
      view.Texture2D.MipLevels = 1;
      view.Format = d->bit_depth == 10 ? DXGI_FORMAT_R16_UNORM : DXGI_FORMAT_R8_UNORM;
      if (FAILED(d->dev->lpVtbl->CreateShaderResourceView(d->dev,
               (ID3D11Resource*)d->pack_input, &view, &d->pack_views[0])))
         return false;
      view.Format = d->bit_depth == 10 ? DXGI_FORMAT_R16G16_UNORM : DXGI_FORMAT_R8G8_UNORM;
      if (FAILED(d->dev->lpVtbl->CreateShaderResourceView(d->dev,
               (ID3D11Resource*)d->pack_input, &view, &d->pack_views[1])))
         return false;
   }
   compiler = LoadLibraryA("d3dcompiler_47.dll");
   if (!compiler)
      return false;
   compile = (pD3DCompile)(void (*)(void))GetProcAddress(compiler, "D3DCompile");
   macros[0].Name = "S";
   macros[0].Definition = d->bit_depth == 10 ? "1023.984375" : "255.0";
   macros[1].Name = "T";
   macros[1].Definition = d->pack_dpb ? "Texture2DArray" : "Texture2D";
   macros[2].Name = "YP";
   macros[2].Definition = d->pack_dpb ? y_position : "int3(p.xy,0)";
   macros[3].Name = "CP";
   macros[3].Definition = d->pack_dpb ? c_position : "int3(p.xy/2,0)";
   macros[4].Name = macros[4].Definition = NULL;
   hr = compile ? compile(source, sizeof(source) - 1, NULL, macros, NULL,
         "main", "cs_5_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, NULL) : E_FAIL;
   if (SUCCEEDED(hr))
      hr = d->dev->lpVtbl->CreateComputeShader(d->dev,
            code->lpVtbl->GetBufferPointer(code), code->lpVtbl->GetBufferSize(code),
            NULL, &d->pack_shader);
   D3D11VD_RELEASE(code);
   FreeLibrary(compiler);
   return SUCCEEDED(hr);
}

static void d3d11vd_pack(d3d11vd_dec_t *d, unsigned output, unsigned slice)
{
   ID3D11ShaderResourceView *none[2] = { NULL, NULL };
   ID3D11UnorderedAccessView *no_output = NULL;
   d->ctx->lpVtbl->CSSetShader(d->ctx, d->pack_shader, NULL, 0);
   d->ctx->lpVtbl->CSSetShaderResources(d->ctx, 0, 2,
         d->pack_dpb ? d->pack_dpb_views[slice] : d->pack_views);
   d->ctx->lpVtbl->CSSetUnorderedAccessViews(d->ctx, 0, 1,
         &d->pack_output[output], NULL);
   d->ctx->lpVtbl->Dispatch(d->ctx,
         (d->visible_w + 7) / 8, (d->visible_h + 7) / 8, 1);
   d->ctx->lpVtbl->CSSetUnorderedAccessViews(d->ctx, 0, 1, &no_output, NULL);
   d->ctx->lpVtbl->CSSetShaderResources(d->ctx, 0, 2, none);
   d->ctx->lpVtbl->CSSetShader(d->ctx, NULL, NULL, 0);
}
