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
 * come back as it went in, whole and handed over in the pieces
 * rzstd_blocks_want() asks for, and a piece short is refused. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <encodings/rzstd.h>

#define BLK 0x20000

static unsigned seed = 7;

/* @z, @len octets, handed to rzstd_blocks_give() as it asks for them:
 * 1 when it comes out as @want, and is all there is */
static int pieces(const uint8_t *z, size_t len, const uint8_t *want,
      size_t n)
{
   rzstd_blocks_t *b  = rzstd_blocks_open();
   size_t          at = 0, out = 0, w, got;
   const uint8_t  *d;
   int             ok = b != NULL;

   while (ok && (w = rzstd_blocks_want(b)))
   {
      if (w > RZSTD_WANT_MAX || at + w > len
            || rzstd_blocks_give(b, z + at, w, &d, &got)
               != RZSTD_PROCESS_NEXT
            || out + got > n || (got && memcmp(d, want + out, got)))
         ok = 0;
      at  += w;
      out += got;
   }
   rzstd_blocks_free(b);
   return ok && at == len && out == n;
}
static uint8_t rnd(void)
{
   seed = seed * 1103515245u + 12345u;
   return (uint8_t)(seed >> 19);
}

/* @z, @len octets, through rzstd_blocks_begin(): 1 when it comes out
 * as @want, @n octets */
static int stepped(const uint8_t *z, size_t len, const uint8_t *want,
      size_t n)
{
   rzstd_blocks_t *b   = rzstd_blocks_begin(z, len);
   size_t          out = 0, got;
   const uint8_t  *d;
   int             e   = RZSTD_PROCESS_ERROR;
   int             ok  = b != NULL;

   while (ok && (e = rzstd_blocks_step(b, &d, &got)) == RZSTD_PROCESS_NEXT)
   {
      if (out + got > n || (got && memcmp(d, want + out, got)))
         ok = 0;
      out += got;
   }
   rzstd_blocks_free(b);
   return ok && e == RZSTD_PROCESS_END && out == n;
}

/* Many megabytes in a window of 2^@log: a single-segment frame of
 * rzstd_encode()'s, its header made one with a window descriptor and
 * no size, so that decoding a block at a time goes round the ring many
 * times, with matches across where it comes round */
static int ring(const uint8_t *src, size_t n, uint8_t *z, size_t cap,
      unsigned log)
{
   size_t wrote = 0;
   if (     rzstd_encode(z, cap, src, n, 9, &wrote) != RZSTD_PROCESS_END
         || wrote < 9 || z[4] != 0xa0)
      return 0;
   /* magic, descriptor 0, window, then the blocks after the old
    * header's four-octet size */
   z[4] = 0;
   z[5] = (uint8_t)((log - 10) << 3);
   memmove(z + 6, z + 9, wrote - 9);
   wrote -= 3;
   return stepped(z, wrote, src, n) && pieces(z, wrote, src, n);
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
      if (!pieces(z, wrote, src, n))
      {
         fprintf(stderr, "FAIL: level %d: not the same in pieces\n", level);
         fails++;
      }
      if (pieces(z, wrote - 1, src, n))
      {
         fprintf(stderr, "FAIL: level %d: a piece short and still whole\n",
               level);
         fails++;
      }
   }
   free(src);
   free(z);
   free(out);

   /* six megabytes of few letters, with runs copied from up to 120 KiB
    * back */
   n   = 6 * 1024 * 1024;
   cap = rzstd_compress_bound(n);
   src = (uint8_t*)malloc(n);
   z   = (uint8_t*)malloc(cap);
   for (i = 0; i < n; )
   {
      size_t run  = 64 + rnd() * 8;
      size_t back = 1024 + ((size_t)rnd() << 9);
      if (i > back && (rnd() & 1))
         for (; run && i < n; run--, i++)
            src[i] = src[i - back];
      else
         for (; run && i < n; run--, i++)
            src[i] = (uint8_t)('a' + (rnd() & 3));
   }
   if (!ring(src, n, z, cap, 17))
   {
      fprintf(stderr, "FAIL: a window of 128 KiB, a block at a time\n");
      fails++;
   }
   if (!ring(src, n, z, cap, 20))
   {
      fprintf(stderr, "FAIL: a window of 1 MiB, a block at a time\n");
      fails++;
   }
   free(src);
   free(z);
   if (fails)
      return 1;
   printf("ok: a raw block between compressed ones, levels 1 to 9, whole and in pieces; a ring gone round\n");
   return 0;
}
