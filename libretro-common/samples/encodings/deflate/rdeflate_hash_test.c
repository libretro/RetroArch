/* The CRC32C match-finder hash in encodings/encoding_deflate.c, as a
 * build without SSE4.2 (-march=core2) reaches it at run time:
 * rd_hash_crc() must give what the CRC32C of the three-byte key does,
 * computed here bit by bit, for each value of each key byte and for
 * random keys.
 *
 * Without the runtime path (another architecture, an old compiler) or
 * on a CPU without SSE4.2 there is nothing to check, and it says so. */

#include <stdio.h>
#include <stdlib.h>

#include "../../../encodings/encoding_deflate.c"

/* CRC32C (Castagnoli, reflected) of the 32-bit little-endian word @v,
 * from @crc, with no pre- or post-inversion: what the crc32 instruction
 * computes */
static uint32_t crc32c_word(uint32_t crc, uint32_t v)
{
   int i;
   crc ^= v;
   for (i = 0; i < 32; i++)
      crc = (crc >> 1) ^ (0x82F63B78u & (0u - (crc & 1u)));
   return crc;
}

int main(void)
{
#if defined(RD_CRC32_HASH_RUNTIME)
   uint32_t seed = 0x12345678u;
   unsigned i, fails = 0;

   if (!(cpu_features_get() & RETRO_SIMD_SSE42))
   {
      printf("rdeflate_hash: no SSE4.2 on this CPU, nothing to check\n");
      return 0;
   }

   for (i = 0; i < 200000; i++)
   {
      uint8_t  p[3];
      uint32_t k, want, got;
      seed = seed * 1664525u + 1013904223u;
      p[0] = (uint8_t)(seed >> 8);
      p[1] = (uint8_t)(seed >> 16);
      p[2] = (uint8_t)(seed >> 24);
      if (i < 256)
      {
         /* every value of each byte on its own */
         p[0] = (uint8_t)i;
         p[1] = p[2] = (uint8_t)(i * 7);
      }
      k    = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
      want = crc32c_word(0, k) & RD_HASH_MASK;
      got  = rd_hash_crc(p);
      if (got != want && fails++ < 5)
         printf("FAIL: key %02x %02x %02x hashes to %04x, CRC32C says %04x\n",
               p[0], p[1], p[2], (unsigned)got, (unsigned)want);
   }
   if (fails)
   {
      printf("rdeflate_hash: %u of 200000 keys wrong\n", fails);
      return 1;
   }
   printf("rdeflate_hash: 200000 keys hash as CRC32C\n");
#else
   printf("rdeflate_hash: no runtime CRC32 hash in this build, nothing to check\n");
#endif
   return 0;
}
