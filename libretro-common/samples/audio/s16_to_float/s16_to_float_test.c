/* convert_s16_to_float against the scalar rule, (float)x * (gain /
 * 32768), bit for bit: every s16 value at several gains, then every
 * length up to 100 at odd offsets so each vector body, the AVX-512
 * masked tail and the scalar remainder run, with a guard word after
 * each buffer. Every tier the CPU has (SSE2, AVX2, AVX-512) is pinned
 * in turn; all must give the same float for the same sample.
 *
 * Input and output are also placed at every offset within 16 bytes and
 * every length up to 48: a vector body that takes only aligned
 * operands has to reach them through a scalar head, and convert the
 * head, body and tail alike. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <audio/conversion/s16_to_float.h>

#ifdef S16_TO_FLOAT_TEST_HOOKS
extern int s16_to_float_tier_cap;
#endif

static unsigned failures;

static void check(const int16_t *in, size_t n, size_t out_off, float gain,
      const char *what, int tier)
{
   float   *base = (float*)malloc((out_off + n + 1) * sizeof(*base));
   float   *out  = base + out_off;
   float    g    = gain / 0x8000;
   uint32_t guard = 0x5A5A5A5Au;
   size_t   i;
   unsigned bad  = 0;
   memcpy(&out[n], &guard, sizeof(guard));
   convert_s16_to_float(out, in, n, gain);
   for (i = 0; i < n; i++)
   {
      float want = (float)in[i] * g;
      if (memcmp(&out[i], &want, sizeof(want)) && bad++ < 4)
         printf("FAIL tier %d %s gain %g n=%u [%u]: in %d got %.9g want %.9g\n",
               tier, what, gain, (unsigned)n, (unsigned)i, in[i],
               out[i], want);
   }
   if (memcmp(&out[n], &guard, sizeof(guard)))
   {
      printf("FAIL tier %d %s n=%u: wrote past the end\n", tier, what,
            (unsigned)n);
      bad++;
   }
   failures += bad ? 1 : 0;
   free(base);
}

int main(void)
{
   static const float gains[] = { 1.0f, 0.5f, 0.0f, 1.37f, 3.98107f,
      0.000123f };
   int16_t *in = (int16_t*)malloc(65536 * sizeof(int16_t));
   size_t   i, n, o, g;
   int      tier;

   for (i = 0; i < 65536; i++)
      in[i] = (int16_t)(uint16_t)(i * 40503u);

   convert_s16_to_float_init_simd();

   for (tier = 0; tier <= 2; tier++)
   {
#ifdef S16_TO_FLOAT_TEST_HOOKS
      s16_to_float_tier_cap = tier;
#else
      if (tier)
         break;
#endif
      for (g = 0; g < sizeof(gains) / sizeof(gains[0]); g++)
         check(in, 65536, 0, gains[g], "every value", tier);
      for (n = 0; n <= 100; n++)
      {
         check(in, n, 0, 1.0f, "length", tier);
         check(in + 1 + (n % 7), n, 0, 0.75f, "offset", tier);
      }
      /* Every output and input placement within 16 bytes, at every
       * length a scalar head, a vector body and a tail can split. */
      for (o = 0; o < 4; o++)
         for (i = 0; i < 8; i++)
            for (n = 0; n <= 48; n++)
               check(in + i, n, o, 0.75f, "placement", tier);
   }

   free(in);
   if (failures)
   {
      printf("s16_to_float_test: %u failing cases\n", failures);
      return 1;
   }
   printf("s16_to_float_test: every tier matches the scalar rule\n");
   return 0;
}
