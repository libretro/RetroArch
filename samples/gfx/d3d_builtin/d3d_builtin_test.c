#ifdef D3D_BUILTIN_TEST
/* Exercise the production source keys, immutable blobs and COM lifetime. */
#define main source_dump_main
#include "../../../tools/d3d_shader_sources.c"
#undef main
#include "../../../gfx/common/d3d_builtin.h"
#include <assert.h>
#include "../../../gfx/include/dxsdk/d3d11.h"
int main(void)
{
   struct shader_case { const char *program, *entry, *profile, *source; };
   struct shader_case cases[] = {
#include ".generated/d3d_builtin_cases.h"
   };
   size_t i;
   ID3D11Device *device = NULL;
   assert(SUCCEEDED(D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
            0, NULL, 0, D3D11_SDK_VERSION, &device, NULL, NULL)));
   for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
   {
      D3DBlob named = NULL, keyed = NULL;
      void *unknown = NULL;
      const void *data;
      assert(d3d_builtin_shader_named(cases[i].program, cases[i].entry, cases[i].profile, &named));
      data = named->lpVtbl->GetBufferPointer(named);
      assert(named->lpVtbl->GetBufferSize(named) > 16);
      assert(SUCCEEDED(named->lpVtbl->QueryInterface(named, &IID_IUnknown, &unknown)));
      assert(unknown == named);
      assert(named->lpVtbl->Release(named) == 1);
      if (cases[i].source)
      {
         assert(d3d_builtin_shader(cases[i].source, 0, cases[i].entry, cases[i].profile, &keyed));
         assert(keyed->lpVtbl->GetBufferPointer(keyed) == data);
         keyed->lpVtbl->Release(keyed);
         assert(d3d_builtin_shader(cases[i].source, strlen(cases[i].source)+1,
                  cases[i].entry, cases[i].profile, &keyed));
         keyed->lpVtbl->Release(keyed);
      }
      if (cases[i].profile[3] == '4' || cases[i].profile[3] == '5')
      {
         IUnknown *shader = NULL;
         size_t bytes = named->lpVtbl->GetBufferSize(named);
         HRESULT result;
         switch (cases[i].profile[0])
         {
            case 'v': result = device->lpVtbl->CreateVertexShader(device,
                         data, bytes, NULL, (ID3D11VertexShader**)&shader); break;
            case 'p': result = device->lpVtbl->CreatePixelShader(device,
                         data, bytes, NULL, (ID3D11PixelShader**)&shader); break;
            case 'g': result = device->lpVtbl->CreateGeometryShader(device,
                         data, bytes, NULL, (ID3D11GeometryShader**)&shader); break;
            default: result = device->lpVtbl->CreateComputeShader(device,
                         data, bytes, NULL, (ID3D11ComputeShader**)&shader); break;
         }
         assert(SUCCEEDED(result));
         shader->lpVtbl->Release(shader);
      }
      assert(named->lpVtbl->Release(named) == 0);
   }
   device->lpVtbl->Release(device);
   printf("PASS: %lu native stages, source lookup, borrowed bytes and COM lifetime\n",
         (unsigned long)(sizeof(cases) / sizeof(cases[0])));
   return 0;
}

#else
#include "../../../gfx/common/d3d_builtin.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
   D3DBlob blob = NULL;
   static const char *entries[] = {
      "VSMain", "PSRect", "PSLayer", "PSAnaglyph", "PSInterlaced"
   };
   unsigned model, i;
   for (model = 4; model <= 6; model++)
      for (i = 0; i < sizeof(entries) / sizeof(entries[0]); i++)
      {
         char profile[7];
         snprintf(profile, sizeof(profile), "%cs_%u_0", i ? 'p' : 'v', model);
         assert(d3d_builtin_shader_named("views", entries[i], profile, &blob));
         blob->lpVtbl->Release(blob);
      }
   assert(!d3d_builtin_shader_named("opaque", "VSMain", "vs_2_0", &blob));
   assert(!d3d_builtin_shader_named("opaque", "VSMain", "vs_3_0", &blob));
   assert(!d3d_builtin_shader_named("planar11", "main", "cs_6_0", &blob));
   assert(!d3d_builtin_shader_named("planar", "PSMain", "ps_4_0", &blob));
   assert(d3d_builtin_shader_named("opaque", "VSMain", "vs_4_0", &blob));
   blob->lpVtbl->Release(blob);
   assert(d3d_builtin_shader_named("planar11", "main", "cs_5_0", &blob));
   blob->lpVtbl->Release(blob);
   assert(d3d_builtin_shader_named("planar12", "main", "cs_6_0", &blob));
   blob->lpVtbl->Release(blob);
   puts("PASS: production catalog omits unused fallbacks and retains native paths");
   return 0;
}
#endif
