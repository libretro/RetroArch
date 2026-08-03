/* Round-trips periodic data across block boundaries.
 *
 * The repeated offsets a decoder remembers belong to the frame, not the
 * block. An encoder that resets them per block agrees with the decoder
 * until the first repeat code that resolves differently -- which was
 * two blocks in, so nothing shorter than 256 KB caught it.
 *
 *   cc -I libretro-common/include -I deps/zstd/lib -DZSTD_DISABLE_ASM \
 *      -o rzstd_multiblock_test <this> \
 *      libretro-common/encodings/encoding_rzstd.c deps/zstd/lib/*/*.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <encodings/rzstd.h>
#include <zstd.h>
static int try_size(size_t n, int verbose)
{
   uint8_t *raw = malloc(n), *cmp, *out = malloc(n + 64);
   size_t bd, w = 0, got = 0, r; unsigned s = 11; size_t i, first = (size_t)-1;
   int bad = 0;
   for (i = 0; i < n; i++)
   { s = s*1103515245u+12345u;
     raw[i] = (uint8_t)((i % 64) ? raw[i - (i % 64)] : (uint8_t)(s >> 19)); }
   bd = rzstd_compress_bound(n); cmp = malloc(bd);
   if (rzstd_encode(cmp, bd, raw, n, 3, &w) != RZSTD_PROCESS_END) { bad = 1; goto out; }
   if (rzstd_decode(out, n + 64, cmp, w, &got) != RZSTD_PROCESS_END) { bad = 2; goto out; }
   if (got != n) { bad = 3; goto out; }
   for (i = 0; i < n; i++) if (out[i] != raw[i]) { first = i; bad = 4; break; }
   /* does the reference agree with us about our own frame? */
   r = ZSTD_decompress(out, n + 64, cmp, w);
   if (!bad && (ZSTD_isError(r) || r != n)) bad = 5;
   if (verbose && bad)
      printf("    first difference at byte %lu of %lu (block %lu, offset in block %lu)\n",
             (unsigned long)first, (unsigned long)n,
             (unsigned long)(first / (128*1024)),
             (unsigned long)(first % (128*1024)));
out:
   free(raw); free(cmp); free(out);
   return bad;
}
int main(void)
{
   size_t sizes[] = {130000, 131072, 131073, 140000, 200000, 262144, 300000,
                     400000, 524288, 600000, 1000000, 0};
   int i;
   for (i = 0; sizes[i]; i++)
   {
      int b = try_size(sizes[i], 0);
      printf("  %8lu bytes: %s\n", (unsigned long)sizes[i],
             b == 0 ? "ok" : b == 4 ? "CONTENT MISMATCH" :
             b == 3 ? "WRONG LENGTH" : b == 5 ? "reference rejects our frame" :
             b == 1 ? "encode failed" : "decode failed");
   }
   printf("\n  detail for the first failing size:\n");
   for (i = 0; sizes[i]; i++)
      if (try_size(sizes[i], 0)) { try_size(sizes[i], 1); break; }
   return 0;
}
