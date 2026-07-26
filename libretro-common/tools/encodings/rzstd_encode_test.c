/* Round-trips many inputs, including real data, and compares the ratio
 * against the reference encoder. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <encodings/rzstd.h>
#include <zstd.h>
static int bad = 0, n_ok = 0;
static void one(const char *name, const uint8_t *in, size_t n, int show)
{
   size_t   bound = rzstd_compress_bound(n), w = 0, d = 0;
   uint8_t *enc = malloc(bound), *out = malloc(n + 16), *ref = malloc(bound);
   size_t   r, refsz;
   if (rzstd_encode(enc, bound, in, n, 3, &w) != RZSTD_PROCESS_END)
   { printf("  %-20s encode FAILED\n", name); bad++; goto done; }
   if (rzstd_decode(out, n + 16, enc, w, &d) != RZSTD_PROCESS_END
       || d != n || memcmp(out, in, n))
   { printf("  %-20s rzstd round-trip FAILED\n", name); bad++; goto done; }
   r = ZSTD_decompress(out, n + 16, enc, w);
   if (ZSTD_isError(r) || r != n || memcmp(out, in, n))
   { printf("  %-20s reference rejected: %s\n", name,
            ZSTD_isError(r) ? ZSTD_getErrorName(r) : "wrong bytes");
     bad++; goto done; }
   n_ok++;
   if (show)
   { refsz = ZSTD_compress(ref, bound, in, n, 3);
     printf("  %-20s %8lu -> %-8lu  reference %-8lu  (%.2fx its size)\n",
            name, (unsigned long)n, (unsigned long)w,
            (unsigned long)refsz, refsz ? (double)w / refsz : 0.0); }
done:
   free(enc); free(out); free(ref);
}
int main(int argc, char **argv)
{
   static uint8_t buf[400000];
   size_t i;
   if (argc > 1)
   {  /* a file, in chunks */
      FILE *f = fopen(argv[1], "rb"); size_t got; int k = 0;
      while ((got = fread(buf, 1, 65536, f)) > 0 && k < 40)
      { char nm[32]; sprintf(nm, "chunk %d", k); one(nm, buf, got, k < 3); k++; }
      fclose(f);
      printf("  %d ok, %d bad\n", n_ok, bad); return bad ? 1 : 0;
   }
   for (i = 0; i < 400000; i++) buf[i] = (uint8_t)(i * 31 + (i >> 9));
   one("structured 400k", buf, 400000, 1);
   { unsigned s = 12345;
     for (i = 0; i < 200000; i++) { s = s * 1103515245u + 12345u;
        buf[i] = (uint8_t)(s >> 16); }
     one("incompressible 200k", buf, 200000, 1); }
   for (i = 0; i < 150000; i++) buf[i] = (uint8_t)((i / 700) & 0x0f);
   one("long runs 150k", buf, 150000, 1);
   for (i = 1; i < 3000; i++) one("short", buf, i, 0);
   printf("  %d ok, %d bad\n", n_ok, bad);
   return bad ? 1 : 0;
}
