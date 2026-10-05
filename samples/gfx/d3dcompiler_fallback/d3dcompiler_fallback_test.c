/* gfx/common/d3dcompiler_common.c: D3DCompile is taken from the first
 * D3DCompiler_*.dll that loads, newest first, and a system without the
 * newest still compiles with the next.
 *
 *   ./d3dcompiler_fallback_test.exe         a shader compiles
 *   ./d3dcompiler_fallback_test.exe none    no compiler loads: the call
 *                                           says it could not load one
 *
 * Under Wine, which has every D3DCompiler_*.dll built in, the Makefile
 * runs it three times: as Wine is, with D3DCompiler_47.dll disabled
 * (so D3DCompiler_46.dll has to be found), and with all of them
 * disabled. */
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "../../../gfx/common/d3dcompiler_common.h"

/* The frontend's log, as the loader's compile errors reach it. */
void RARCH_ERR(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);
}

int main(int argc, char **argv)
{
   static const char src[] =
      "float4 main() : SV_Target { return float4(1.0, 0.5, 0.25, 1.0); }";
   D3DBlob code = NULL;
   D3DBlob err  = NULL;
   int     none = argc > 1 && !strcmp(argv[1], "none");
   HRESULT hr   = D3DCompile(src, sizeof(src) - 1, NULL, NULL, NULL,
         "main", "ps_4_0", 0, 0, &code, &err);

   if (none)
   {
      if (hr == TYPE_E_CANTLOADLIBRARY && !code)
      {
         printf("d3dcompiler_fallback: ok (no compiler, and it says so)\n");
         return 0;
      }
      printf("FAIL: with no compiler to load: hr 0x%08lx, code %p\n",
            (unsigned long)hr, (void*)code);
      return 1;
   }
   if (SUCCEEDED(hr) && code && code->lpVtbl->GetBufferSize(code) > 0)
   {
      printf("d3dcompiler_fallback: ok (%lu bytes of bytecode)\n",
            (unsigned long)code->lpVtbl->GetBufferSize(code));
      code->lpVtbl->Release(code);
      return 0;
   }
   printf("FAIL: D3DCompile: hr 0x%08lx\n", (unsigned long)hr);
   return 1;
}
