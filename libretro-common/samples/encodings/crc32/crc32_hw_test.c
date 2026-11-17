/* encoding_crc32()'s instruction path against its table path and a
 * bitwise reference, on whichever instructions this build and CPU
 * have. encoding_crc32.c is included whole so its paths are called
 * directly.
 *
 *    crc32_hw_test              every path there is
 *    crc32_hw_test --require-hw fails when no instruction path runs:
 *                               the cross lanes, where one must */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../../encodings/encoding_crc32.c"

static unsigned checks;
static unsigned failures;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#if defined(CRC32_HAVE_ARM_PATH)
/* The raw register form the paths share: no complement either side */
static uint32_t crc_bitwise_raw(uint32_t crc, const uint8_t *data,
      size_t len)
{
   size_t i;
   int b;
   for (i = 0; i < len; i++)
   {
      crc ^= data[i];
      for (b = 0; b < 8; b++)
         crc = (crc & 1) ? ((crc >> 1) ^ 0xEDB88320U) : (crc >> 1);
   }
   return crc;
}

/* Every length to 1200 at eight alignments, past the three-lane loop
 * (3 KiB) at its seams, and a 64 KiB run, from random registers */
static unsigned check_arm(void)
{
   static const size_t seams[] = { 3071, 3072, 3073, 3079, 3080,
      6143, 6144, 6145, 9216, 65536 };
   size_t big = 65536 + 16, n, off, k;
   uint8_t *m = (uint8_t*)malloc(big);
   uint32_t seed = 0x9E3779B9U;
#if defined(CRC32_ARM_DISPATCH)
   if (!(cpu_features_get() & RETRO_SIMD_CRC32))
   {
      free(m);
      return 0;
   }
#endif
   if (!m)
   {
      CHECK(m != NULL);
      return 0;
   }
   for (n = 0; n < big; n++)
      m[n] = (uint8_t)(n * 2654435761U >> 13);
   for (n = 0; n <= 1200; n++)
      for (off = 0; off < 8; off++)
      {
         seed = seed * 1664525U + 1013904223U;
         CHECK(crc32_arm(seed, m + off, n)
               == crc_bitwise_raw(seed, m + off, n));
      }
   for (k = 0; k < sizeof(seams) / sizeof(seams[0]); k++)
      for (off = 0; off < 8; off++)
      {
         seed = seed * 1664525U + 1013904223U;
         CHECK(crc32_arm(seed, m + off, seams[k])
               == crc_bitwise_raw(seed, m + off, seams[k]));
#if defined(CRC32_ARM_DISPATCH)
         CHECK(crc32_arm(seed, m + off, seams[k])
               == crc32_slice_by_8(seed, m + off, seams[k]));
#endif
      }
   free(m);
   printf("   ARMv8 CRC32 instructions: agree\n");
   return 1;
}
#endif

int main(int argc, char **argv)
{
   unsigned hw = 0;
   int require_hw = argc > 1 && !strcmp(argv[1], "--require-hw");
   setvbuf(stdout, NULL, _IONBF, 0);
   printf("crc32_hw_test:\n");
#if defined(CRC32_HAVE_ARM_PATH)
   hw = check_arm();
#endif
   CHECK(encoding_crc32(0, (const uint8_t*)"123456789", 9) == 0xCBF43926U);
   if (require_hw)
      CHECK(hw == 1);
   printf("%u checks, %u failures\n", checks, failures);
   return failures ? 1 : 0;
}
