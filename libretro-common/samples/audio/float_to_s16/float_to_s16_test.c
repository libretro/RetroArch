/* convert_float_to_s16 against the scalar rule it documents: x * 32768,
 * NaN -> 0, round half away from zero, saturate to the s16 range. Every
 * tier this CPU has (SSE2, AVX2, AVX-512) runs pinned in turn, on the
 * edges above all - exact .5 points after scaling, full scale and past
 * it, infinities, NaNs of either sign, denormals - and at every length
 * up to 100, so each vector body, the AVX-512 masked tail and the scalar
 * remainder are exercised. A guard word after each buffer must survive.
 * Every target must give the same s16 for the same float. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <audio/conversion/float_to_s16.h>

#ifdef FLOAT_TO_S16_TEST_HOOKS
extern int float_to_s16_tier_cap;
#endif

static unsigned failures;

static int16_t reference(float v)
{
   float    scaled = v * 0x8000;
   uint32_t bits;
   memcpy(&bits, &scaled, sizeof(bits));
   if ((bits & 0x7FFFFFFFu) > 0x7F800000u)
      return 0;
   scaled += (scaled >= 0.0f ? 0.5f : -0.5f);
   if (scaled >  32767.0f)
      return 0x7FFF;
   if (scaled < -32768.0f)
      return -0x8000;
   return (int16_t)(int32_t)scaled;
}

static float from_bits(uint32_t b)
{
   float f;
   memcpy(&f, &b, sizeof(f));
   return f;
}

static void check(const float *in, size_t n, const char *what, int tier)
{
   int16_t *out = (int16_t*)malloc((n + 1) * sizeof(*out));
   size_t   i;
   unsigned bad = 0;
   out[n]       = 0x5A5A;
   convert_float_to_s16(out, in, n);
   for (i = 0; i < n; i++)
   {
      int16_t  want = reference(in[i]);
      uint32_t bits;
      memcpy(&bits, &in[i], sizeof(bits));
      if (out[i] != want && bad++ < 4)
         printf("FAIL tier %d %s n=%u [%u]: in %.9g (0x%08x) got %d want %d\n",
               tier, what, (unsigned)n, (unsigned)i, in[i],
               (unsigned)bits, out[i], want);
   }
   if (out[n] != 0x5A5A)
   {
      printf("FAIL tier %d %s n=%u: wrote past the end\n", tier, what,
            (unsigned)n);
      bad++;
   }
   failures += bad ? 1 : 0;
   free(out);
}

int main(void)
{
   static const uint32_t special[] = {
      0x7FC00000u, 0xFFC00000u, 0x7F800001u, 0xFFFFFFFFu, /* NaNs */
      0x7F800000u, 0xFF800000u,                           /* +-inf */
      0x00000001u, 0x80000001u, 0x007FFFFFu,              /* denormals */
      0x00000000u, 0x80000000u                            /* +-0 */
   };
   static const float edges[] = {
      1.0f, -1.0f, 1.0000001f, -1.0000001f, 0.99998474f, -0.99998474f,
      2.0f, -2.0f, 1e30f, -1e30f,
      0.5f / 32768, -0.5f / 32768, 1.5f / 32768, -1.5f / 32768,
      2.5f / 32768, -2.5f / 32768, 32766.5f / 32768, -32767.5f / 32768,
      32767.5f / 32768, -32768.5f / 32768, 0.49999997f / 32768
   };
   float   *in = (float*)malloc(4096 * sizeof(float));
   size_t   i, n, k = 0;
   uint32_t seed = 0x1234567u;
   int      tier;

   convert_float_to_s16_init_simd();

   /* Edges and specials first, then random values of all magnitudes. */
   for (i = 0; i < sizeof(special) / sizeof(special[0]); i++)
      in[k++] = from_bits(special[i]);
   for (i = 0; i < sizeof(edges) / sizeof(edges[0]); i++)
      in[k++] = edges[i];
   for (; k < 4096; k++)
   {
      seed    = seed * 1664525u + 1013904223u;
      in[k]   = (k & 1) ? from_bits(seed)                     /* any bits */
         : (float)((int32_t)seed) / 2147483648.0f * 1.25f;    /* audio */
   }

   for (tier = 0; tier <= 2; tier++)
   {
#ifdef FLOAT_TO_S16_TEST_HOOKS
      float_to_s16_tier_cap = tier;
#else
      if (tier)
         break;
#endif
      check(in, 4096, "corpus", tier);
      for (n = 0; n <= 100; n++)
      {
         check(in, n, "length", tier);
         check(in + 1 + (n % 7), n, "offset", tier);
      }
   }

   free(in);
   if (failures)
   {
      printf("float_to_s16_test: %u failing cases\n", failures);
      return 1;
   }
   printf("float_to_s16_test: every tier matches the scalar rule\n");
   return 0;
}
