/* A block the encoder searched and then stored raw, because its
 * matches did not pay for themselves, must leave the recent offsets as
 * the decoder sees them: a raw block does not move them. The search
 * had already moved them, so the next compressed block named an offset
 * by a repeat code the decoder resolved to a different one, and the
 * frame decoded - by this decoder and by the reference - to the wrong
 * octets, with no error.
 *
 * Three 128 KiB blocks: one whose last match is four back; one of
 * noise holding a copy from a thousand back, not worth compressing;
 * one repeating every thousand octets. At every level the frame must
 * come back as it went in. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <encodings/rzstd.h>

#define BLK 0x20000

static unsigned seed = 7;
static uint8_t rnd(void)
{
   seed = seed * 1103515245u + 12345u;
   return (uint8_t)(seed >> 19);
}

int main(void)
{
   size_t   n = 3 * BLK, i, cap = rzstd_compress_bound(3 * BLK), wrote, got;
   uint8_t *src = (uint8_t*)malloc(n), *z = (uint8_t*)malloc(cap);
   uint8_t *out = (uint8_t*)malloc(n);
   int      level, fails = 0;

   /* a period of four to the end of the first block */
   for (i = 0; i < BLK; i++)
      src[i] = i < 4 ? rnd() : src[i - 4];
   /* noise, early in which eight octets from a thousand back: a
    * match the search takes, too short to pay for the block */
   for (i = BLK; i < 2 * BLK; i++)
      src[i] = rnd();
   memcpy(src + BLK + 1001, src + BLK + 1, 8);
   /* a thousand octets of noise, then repeated */
   for (i = 2 * BLK; i < 3 * BLK; i++)
      src[i] = i < 2 * BLK + 1000 ? rnd() : src[i - 1000];

   for (level = 1; level <= 9; level++)
   {
      wrote = got = 0;
      if (     rzstd_encode(z, cap, src, n, level, &wrote) != RZSTD_PROCESS_END
            || rzstd_decode(out, n, z, wrote, &got) != RZSTD_PROCESS_END
            || got != n || memcmp(out, src, n))
      {
         for (i = 0; i < got && out[i] == src[i]; i++) ;
         fprintf(stderr, "FAIL: level %d: decoded %lu, first difference at %lu\n",
               level, (unsigned long)got, (unsigned long)i);
         fails++;
      }
   }
   free(src);
   free(z);
   free(out);
   if (fails)
      return 1;
   printf("ok: a raw block between compressed ones, levels 1 to 9\n");
   return 0;
}
