/* An fxc-shaped driver over D3DCompile, for tools/rslc-hlsl-check.py:
 * the compiler is whatever d3dcompiler_47 the process links, Wine's
 * own under Wine, Microsoft's on Windows.  Build with MinGW
 * (x86_64-w64-mingw32-gcc -o rslc-hlslc.exe rslc-hlslc.c -ld3dcompiler_47)
 * or MSVC.
 *   rslc-hlslc <profile> <entry> <file.hlsl> [out.dxbc]
 *   rslc-hlslc -l <list>        one "profile entry file" per line
 * One result line per file: "ok <file> <bytes>" or "FAIL <file>"
 * followed by the compiler's messages indented.  Exit status 0 when
 * every compile succeeded. */
#define COBJMACROS
#include <windows.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int compile_one(const char *profile, const char *entry,
      const char *path, const char *outpath)
{
   FILE *f;
   long len;
   char *src;
   ID3DBlob *code = NULL, *err = NULL;
   HRESULT hr;
   f = fopen(path, "rb");
   if (!f)
   {
      printf("FAIL %s\n   cannot open\n", path);
      return 1;
   }
   fseek(f, 0, SEEK_END);
   len = ftell(f);
   fseek(f, 0, SEEK_SET);
   src = malloc(len + 1);
   if (fread(src, 1, len, f) != (size_t)len)
   {
      fclose(f);
      free(src);
      printf("FAIL %s\n   short read\n", path);
      return 1;
   }
   fclose(f);
   hr = D3DCompile(src, len, path, NULL, NULL, entry, profile,
         D3DCOMPILE_OPTIMIZATION_LEVEL0, 0, &code, &err);
   free(src);
   if (FAILED(hr))
   {
      printf("FAIL %s\n", path);
      if (err)
      {
         const char *m = ID3D10Blob_GetBufferPointer(err);
         size_t n = ID3D10Blob_GetBufferSize(err), i, bol = 1;
         for (i = 0; i < n && m[i]; i++)
         {
            if (bol)
               fputs("   ", stdout);
            putchar(m[i]);
            bol = m[i] == '\n';
         }
         if (!bol)
            putchar('\n');
         ID3D10Blob_Release(err);
      }
      else
         printf("   hr=0x%08lx\n", (unsigned long)hr);
      fflush(stdout);
      return 1;
   }
   if (err)
      ID3D10Blob_Release(err);
   if (outpath)
   {
      FILE *o = fopen(outpath, "wb");
      if (o)
      {
         fwrite(ID3D10Blob_GetBufferPointer(code), 1, ID3D10Blob_GetBufferSize(code), o);
         fclose(o);
      }
   }
   printf("ok %s %lu\n", path, (unsigned long)ID3D10Blob_GetBufferSize(code));
   fflush(stdout);
   ID3D10Blob_Release(code);
   return 0;
}

int main(int argc, char **argv)
{
   int bad = 0;
   if (argc == 3 && !strcmp(argv[1], "-l"))
   {
      char line[4096];
      FILE *l = fopen(argv[2], "r");
      if (!l)
      {
         perror(argv[2]);
         return 2;
      }
      while (fgets(line, sizeof(line), l))
      {
         char *profile = strtok(line, " \t\r\n");
         char *entry   = profile ? strtok(NULL, " \t\r\n") : NULL;
         char *path    = entry ? strtok(NULL, "\r\n") : NULL;
         if (!path)
            continue;
         bad += compile_one(profile, entry, path, NULL);
      }
      fclose(l);
      return bad ? 1 : 0;
   }
   if (argc < 4)
   {
      fprintf(stderr, "usage: rslc-hlslc <profile> <entry> <file> [out] | rslc-hlslc -l <list>\n");
      return 2;
   }
   return compile_one(argv[1], argv[2], argv[3], argc > 4 ? argv[4] : NULL);
}
