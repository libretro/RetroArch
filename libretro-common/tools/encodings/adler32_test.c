/* Checks encoding_deflate.c's adler32 against a textbook reference over
 * the inputs that actually distinguish the vectorised kernel from the
 * scalar one: every length across the 16- and 32-byte block boundaries,
 * unaligned starts, the RD_ADLER_NMAX chunk bound and its multiples,
 * incremental feeding at every step size, and the worst-case input for
 * the 32-bit overflow argument (a run of 0xff seeded at the largest
 * legal adler, which is what fixes NMAX at 5552).
 *
 * Driving this through rinflate/rdeflate instead would only cover
 * whatever lengths and alignments a given stream happens to produce.
 *
 *   cc -I libretro-common/include -o adler32_test \
 *      libretro-common/tools/encodings/adler32_test.c \
 *      libretro-common/encodings/encoding_deflate.c
 *
 * Build it for the scalar fallback with -U__SSE2__ (or -U__ARM_NEON
 * -U__ARM_NEON__) to check that path too; all builds must agree on
 * every digest. On x86 every check runs again through each block width
 * the CPU takes, 16, 32 (AVX2) and 64 (AVX-512).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

uint32_t rd_probe_adler32(uint32_t adler, const uint8_t *buf, size_t len);
unsigned rd_probe_adler32_widest(void);
uint32_t rd_probe_adler32_on(uint32_t adler, const uint8_t *buf, size_t len,
      unsigned wide);

/* 0: the width rd_probe_adler32() chooses; else this one */
static unsigned wide;

static uint32_t probe(uint32_t adler, const uint8_t *buf, size_t len)
{
   return wide ? rd_probe_adler32_on(adler, buf, len, wide)
      : rd_probe_adler32(adler, buf, len);
}

static uint32_t ref_adler32(uint32_t adler, const uint8_t *buf, size_t len)
{
   uint32_t a = adler & 0xffff;
   uint32_t b = (adler >> 16) & 0xffff;
   while (len)
   {
      size_t n = len > 5552 ? 5552 : len;
      len -= n;
      do
      {
         a += *buf++;
         b += a;
      } while (--n);
      a %= 65521u;
      b %= 65521u;
   }
   return (b << 16) | a;
}

static unsigned long checks;
static unsigned      fails;

static void ck(uint32_t got, uint32_t want, const char *what,
      unsigned long p1, unsigned long p2)
{
   checks++;
   if (got == want)
      return;
   if (++fails <= 12)
      printf("FAIL %s (%lu, %lu): %08x != %08x\n", what, p1, p2, got, want);
}

static void run(void)
{
   static const size_t big[] = {
      5535, 5551, 5552, 5553, 5567, 5568, 11103, 11104, 11105,
      16655, 16656, 65535, 65536, 1048576, 4194304
   };
   size_t   n     = 4u * 1024 * 1024;
   /* the longest bound is read at up to 16 octets on */
   uint8_t *buf   = (uint8_t*)malloc(n + 16);
   size_t   len, off, k;

   if (!buf)
   {
      fails++;
      return;
   }
   for (len = 0; len < n + 16; len++)
      buf[len] = (uint8_t)(len * 1103515245u >> 13);

   /* every length across both block sizes, at three start alignments */
   for (len = 0; len <= 2048; len++)
      for (off = 0; off < 3; off++)
         ck(probe(1, buf + off, len),
            ref_adler32(1, buf + off, len), "len/off",
            (unsigned long)len, (unsigned long)off);

   /* chunk-bound neighbourhoods, every alignment within a block */
   for (k = 0; k < sizeof(big) / sizeof(big[0]); k++)
      for (off = 0; off < 17; off++)
         ck(probe(1, buf + off, big[k]),
            ref_adler32(1, buf + off, big[k]), "bound",
            (unsigned long)big[k], (unsigned long)off);

   /* the overflow worst case: 0xff seeded at the largest legal adler */
   memset(buf, 0xff, n + 16);
   for (k = 1; k <= 400; k++)
   {
      uint32_t seed = (65520u << 16) | 65520u;
      ck(probe(seed, buf, k * 16),
         ref_adler32(seed, buf, k * 16), "0xff run",
         (unsigned long)(k * 16), 0);
   }
   {
      uint32_t seed = (65520u << 16) | 65520u;
      uint32_t got  = probe(seed, buf, 5552);
      ck(got, ref_adler32(seed, buf, 5552), "NMAX worst case", 5552, 0);
      if (!wide)
         printf("NMAX worst case digest: %08x\n", got);
   }

   /* incremental feeding must match a single call */
   for (len = 0; len < n + 16; len++)
      buf[len] = (uint8_t)(len * 1103515245u >> 13);
   for (k = 1; k <= 64; k++)
   {
      uint32_t acc = 1;
      size_t   p;
      for (p = 0; p < 20000; p += k)
         acc = probe(acc, buf + p,
               (p + k > 20000) ? 20000 - p : k);
      ck(acc, ref_adler32(1, buf, 20000), "incremental step",
            (unsigned long)k, 0);
   }

   free(buf);
}

int main(void)
{
   unsigned widest = rd_probe_adler32_widest();
   run();
   for (wide = 16; wide <= widest; wide *= 2)
      run();
   printf("%lu checks through blocks of 16 to %u, %u failures\n", checks,
         widest, fails);
   return fails != 0;
}
