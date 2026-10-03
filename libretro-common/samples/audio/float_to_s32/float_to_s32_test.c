/* convert_float_to_s32 against a plain reference: every sample the
 * vector arms produce must equal what the scalar rule gives, on the
 * edges above all - 1.0, values past full scale, NaN, infinities,
 * the 24-in-32 mask - and at every tail length so the vector body and
 * the scalar remainder are both exercised. Build natively for the SSE2
 * arm; cross-build for aarch64 and run under qemu for the NEON one. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include <audio/conversion/float_to_s32.h>

static unsigned failures;

static int32_t reference(float v, unsigned valid_bits)
{
   int32_t mask = (valid_bits >= 32)
      ? (int32_t)~0 : (int32_t)~((1 << (32 - valid_bits)) - 1);
   double  d;
   if (v != v)
      return 0;
   d = (double)v * 2147483648.0;
   if (d >= 2147483648.0)
      return 0x7FFFFFFF & mask;
   if (d <= -2147483648.0)
      return (int32_t)(-0x7FFFFFFF - 1);
   return (int32_t)d & mask;
}

static void check_block(const float *in, size_t n, unsigned bits,
      const char *what)
{
   int32_t *out = (int32_t*)calloc(n + 8, sizeof(*out));
   size_t   i;
   unsigned bad = 0;

   out[n] = (int32_t)0x5A5A5A5A;
   convert_float_to_s32(out, in, n, bits);
   for (i = 0; i < n; i++)
      if (out[i] != reference(in[i], bits))
      {
         if (bad < 3)
            printf("    in[%u]=%g: got %ld, expected %ld\n", (unsigned)i,
                  in[i], (long)out[i], (long)reference(in[i], bits));
         bad++;
      }
   if (out[n] != (int32_t)0x5A5A5A5A)
   {
      printf("    wrote past the end\n");
      bad++;
   }
   printf("  [%s] %s (%u samples, %u bits)\n", bad ? "FAIL" : "pass",
         what, (unsigned)n, bits);
   if (bad)
      failures++;
   free(out);
}

int main(void)
{
   float    edge[16];
   float    ramp[1027];
   uint32_t nanbits = 0x7FC00000u;
   size_t   i, n;

   convert_float_to_s32_init_simd();

   edge[0]  = 0.0f;         edge[1]  = -0.0f;
   edge[2]  = 1.0f;         edge[3]  = -1.0f;
   edge[4]  = 0.5f;         edge[5]  = -0.5f;
   edge[6]  = 2.0f;         edge[7]  = -2.0f;
   edge[8]  = 1e30f;        edge[9]  = -1e30f;
   edge[10] = 0.99999994f;  edge[11] = -0.99999994f;
   edge[12] = 1.0f / 3.0f;  edge[13] = -1.0f / 4096.0f;
   memcpy(&edge[14], &nanbits, sizeof(float));
   edge[15] = (float)HUGE_VAL;

   check_block(edge, 16, 32, "edges");
   check_block(edge, 16, 24, "edges");
   /* Every tail length, so the remainder after the vector body is hit
    * for each of 0..7 leftovers. */
   for (n = 1; n <= 16; n++)
      check_block(edge, n, 32, "edges, short");

   for (i = 0; i < 1027; i++)
      ramp[i] = -1.2f + 2.4f * (float)i / 1026.0f;
   check_block(ramp, 1027, 32, "ramp through full scale");
   check_block(ramp, 1027, 24, "ramp through full scale");

   printf("%u failure%s\n", failures, failures == 1 ? "" : "s");
   return failures ? 1 : 0;
}
