/* Round-trips data through rzstd's encoder and decodes the result with
 * both rzstd and the reference implementation.
 *
 * Checking against the reference matters more than checking against
 * ourselves: an encoder and decoder that share a misreading of the
 * format agree with each other perfectly.
 *
 *   cc -I libretro-common/include -I deps/zstd/lib -DZSTD_DISABLE_ASM \
 *      -o rzstd_encode_test <this> \
 *      libretro-common/encodings/encoding_rzstd.c <zstd decompress>
 */
/* Round-trips data through rzstd's encoder, then decodes it with both
 * rzstd and the reference implementation. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <encodings/rzstd.h>
#include <zstd.h>
static int one(const char *name, const uint8_t *in, size_t n)
{
   size_t bound = rzstd_compress_bound(n), w = 0, d = 0;
   uint8_t *enc = malloc(bound), *out = malloc(n + 16);
   size_t r;
   int ok_self, ok_ref;
   if (rzstd_encode(enc, bound, in, n, 3, &w) != RZSTD_PROCESS_END)
   { printf("  %-22s encode FAILED\n", name); return 1; }
   ok_self = (rzstd_decode(out, n + 16, enc, w, &d) == RZSTD_PROCESS_END
              && d == n && !memcmp(out, in, n));
   r = ZSTD_decompress(out, n + 16, enc, w);
   ok_ref = (!ZSTD_isError(r) && r == n && !memcmp(out, in, n));
   printf("  %-22s %7lu -> %-7lu (%5.1f%%)  rzstd:%s  reference:%s\n",
          name, (unsigned long)n, (unsigned long)w,
          n ? 100.0 * w / n : 0.0,
          ok_self ? "ok" : "FAIL", ok_ref ? "ok" : "FAIL");
   free(enc); free(out);
   return !(ok_self && ok_ref);
}
int main(void)
{
   static uint8_t buf[300000];
   size_t i; int bad = 0;
   bad |= one("empty", buf, 0);
   for (i = 0; i < 40; i++) buf[i] = (uint8_t)i;
   bad |= one("40 counting bytes", buf, 40);
   memset(buf, 0x5a, 100000);
   bad |= one("100k identical", buf, 100000);
   for (i = 0; i < 200000; i++) buf[i] = (uint8_t)(i * 7);
   bad |= one("200k patterned", buf, 200000);
   /* Something shaped like a replay: a state that mostly repeats. */
   { uint8_t st[16]; memset(st, 0, sizeof(st));
     for (i = 0; i < 60000; i += 16) {
        if ((i / 16) % 37 == 0) st[(i / 16) % 16] ^= 0x11;
        memcpy(buf + i, st, 16); }
     bad |= one("60k replay-shaped", buf, 60000); }
   for (i = 0; i < 70000; i++) buf[i] = (uint8_t)(i % 251);
   bad |= one("70k modular", buf, 70000);
   return bad;
}
