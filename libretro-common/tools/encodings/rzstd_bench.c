/* Times rzstd against the reference implementation on the same data. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <encodings/rzstd.h>
#include <zstd.h>

static double now(void)
{
   struct timespec t;
   clock_gettime(CLOCK_MONOTONIC, &t);
   return t.tv_sec + t.tv_nsec * 1e-9;
}

int main(int argc, char **argv)
{
   size_t   n = 4 * 1024 * 1024, bound, w = 0, i, reps;
   uint8_t *in = malloc(n), *enc, *ref, *out = malloc(n + 64);
   double   t0, t_rz_e, t_zs_e, t_rz_d, t_zs_d;
   size_t   rzsz, zssz;

   if (argc > 1)
   {  FILE *f = fopen(argv[1], "rb");
      n = fread(in, 1, n, f); fclose(f);
      printf("  input: %s, %lu bytes\n", argv[1], (unsigned long)n); }
   else
   {  /* something compressible but not trivially so */
      unsigned s = 1;
      for (i = 0; i < n; i++)
      { s = s * 1103515245u + 12345u;
        in[i] = (uint8_t)((i / 64) ^ ((s >> 20) & ((i & 4095) < 200 ? 0xff : 0x03))); }
      printf("  input: synthetic, %lu bytes\n", (unsigned long)n); }

   bound = rzstd_compress_bound(n);
   enc = malloc(bound); ref = malloc(ZSTD_compressBound(n));

   reps = 3;
   t0 = now();
   for (i = 0; i < reps; i++) rzstd_encode(enc, bound, in, n, 3, &w);
   t_rz_e = (now() - t0) / reps; rzsz = w;

   t0 = now();
   for (i = 0; i < reps; i++) zssz = ZSTD_compress(ref, ZSTD_compressBound(n), in, n, 3);
   t_zs_e = (now() - t0) / reps;

   reps = 10;
   t0 = now();
   for (i = 0; i < reps; i++) rzstd_decode(out, n + 64, enc, rzsz, NULL);
   t_rz_d = (now() - t0) / reps;

   t0 = now();
   for (i = 0; i < reps; i++) ZSTD_decompress(out, n + 64, ref, zssz);
   t_zs_d = (now() - t0) / reps;

   printf("\n  encode   rzstd %7.1f MB/s -> %-9lu   reference %7.1f MB/s -> %-9lu\n",
          n / t_rz_e / 1e6, (unsigned long)rzsz,
          n / t_zs_e / 1e6, (unsigned long)zssz);
   printf("  decode   rzstd %7.1f MB/s              reference %7.1f MB/s\n",
          n / t_rz_d / 1e6, n / t_zs_d / 1e6);
   printf("\n  rzstd encode is %.2fx the reference's speed, output %.2fx its size\n",
          t_zs_e / t_rz_e, (double)rzsz / zssz);
   printf("  rzstd decode is %.2fx the reference's speed\n", t_zs_d / t_rz_d);
   return 0;
}
