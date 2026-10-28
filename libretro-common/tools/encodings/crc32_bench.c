/* encoding_crc32() timed against one chain of the CPU's own CRC
 * instructions, on hardware that has them: AArch64's CRC32X, the three
 * lanes encoding_crc32.c runs them in against the one a plain loop
 * does. Elsewhere it times encoding_crc32() alone.
 *
 *   cc -O2 -o crc32_bench libretro-common/tools/encodings/crc32_bench.c \
 *      libretro-common/encodings/encoding_crc32.c \
 *      libretro-common/features/features_cpu.c \
 *      libretro-common/compat/compat_strl.c \
 *      libretro-common/time/rtime.c -Ilibretro-common/include -lpthread
 *   ./crc32_bench
 *
 * Prints MB/s for each buffer size, best of several runs. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include <encodings/crc32.h>

#if defined(__aarch64__) && defined(__GNUC__) \
   && ((defined(__clang__) && __clang_major__ >= 16) \
      || (!defined(__clang__) && __GNUC__ >= 9))
#include <arm_acle.h>
#define BENCH_ONE_CHAIN 1
#if defined(__clang__)
__attribute__((target("crc")))
#else
__attribute__((target("arch=armv8-a+crc")))
#endif
static uint32_t one_chain(uint32_t crc, const uint8_t *data, size_t len)
{
   crc = ~crc;
   while (len >= 8)
   {
      uint64_t v;
      memcpy(&v, data, sizeof(v));
      crc   = __crc32d(crc, v);
      data += 8;
      len  -= 8;
   }
   while (len--)
      crc = __crc32b(crc, *data++);
   return ~crc;
}
#endif

static double now(void)
{
   struct timespec t;
   clock_gettime(CLOCK_MONOTONIC, &t);
   return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

typedef uint32_t (*crc_fn)(uint32_t, const uint8_t*, size_t);

/* best MB/s of @fn over @len octets, about 64 MiB a run */
static double rate(crc_fn fn, const uint8_t *buf, size_t len)
{
   double   best = 0;
   unsigned run;
   size_t   reps = (64u << 20) / len;
   volatile uint32_t sink = 0;
   for (run = 0; run < 7; run++)
   {
      double t = now(), mbs;
      size_t i;
      for (i = 0; i < reps; i++)
         sink += fn(0, buf, len);
      t   = now() - t;
      mbs = (double)len * (double)reps / t / 1e6;
      if (mbs > best)
         best = mbs;
   }
   return best;
}

int main(void)
{
   static const size_t sizes[] = { 1024, 4096, 16384, 65536, 1048576 };
   size_t   max = 1048576, i;
   uint8_t *buf = (uint8_t*)malloc(max);
   unsigned k;
   if (!buf)
      return 1;
   for (i = 0; i < max; i++)
      buf[i] = (uint8_t)(i * 2654435761u >> 13);
#if defined(BENCH_ONE_CHAIN)
   if (one_chain(0, buf, max) != encoding_crc32(0, buf, max))
   {
      printf("the two disagree\n");
      return 1;
   }
#endif
   for (k = 0; k < sizeof(sizes) / sizeof(sizes[0]); k++)
   {
      double r = rate(encoding_crc32, buf, sizes[k]);
#if defined(BENCH_ONE_CHAIN)
      double o = rate(one_chain, buf, sizes[k]);
      printf("%8lu: encoding_crc32 %8.0f MB/s, one chain %8.0f MB/s, "
            "%.2fx\n", (unsigned long)sizes[k], r, o, r / o);
#else
      printf("%8lu: encoding_crc32 %8.0f MB/s\n", (unsigned long)sizes[k], r);
#endif
   }
   free(buf);
   return 0;
}
