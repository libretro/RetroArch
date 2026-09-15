/* The standard's tables, checked against each other.
 *
 * bndtab and bndsz must tile the 256 bins and masktab must be the
 * band each bin falls in; hth must be nonzero and within the range
 * the allocation compares it in; baptab must be nondecreasing and
 * end at 15; latab must start at 0x40 and fall to 0; the window must
 * rise from near zero to one and satisfy the Princen-Bradley
 * condition, w[n]^2 + w[255-n]^2 = 1, which the overlap-add depends
 * on. A transcription slip in any one shows here. */

#include <stdio.h>
#include <math.h>
#include "../../../formats/ac3/rac3_tables.h"

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

int main(void)
{
   unsigned b, i;
   printf("rac3 tables:\n");
   printf("   bands tile the bins and masktab agrees\n");
   for (b = 0; b + 1 < 50; b++)
      CHECK(rac3_bndtab[b] + rac3_bndsz[b] == rac3_bndtab[b + 1], "band %u ends at %u, band %u starts at %u", b, rac3_bndtab[b] + rac3_bndsz[b], b + 1, rac3_bndtab[b + 1]);
   CHECK(rac3_bndtab[49] + rac3_bndsz[49] == 253, "the last band ends at %u, not 253", rac3_bndtab[49] + rac3_bndsz[49]);
   for (i = 0; i < 253; i++)
   {
      unsigned band = 0;
      while (band + 1 < 50 && rac3_bndtab[band + 1] <= i) band++;
      CHECK(rac3_masktab[i] == band, "masktab[%u] is %u, the band is %u", i, rac3_masktab[i], band);
   }
   printf("   latab, baptab, hth\n");
   CHECK(rac3_latab[0] == 0x40 && rac3_latab[255] == 0, "latab ends: 0x%02x .. 0x%02x", rac3_latab[0], rac3_latab[255]);
   for (i = 1; i < 256; i++)
      CHECK(rac3_latab[i] <= rac3_latab[i - 1], "latab rises at %u", i);
   for (i = 1; i < 64; i++)
      CHECK(rac3_baptab[i] >= rac3_baptab[i - 1], "baptab falls at %u", i);
   CHECK(rac3_baptab[63] == 15 && rac3_baptab[0] == 0, "baptab ends: %u .. %u", rac3_baptab[0], rac3_baptab[63]);
   for (b = 0; b < 3; b++)
      for (i = 0; i < 50; i++)
         CHECK(rac3_hth[b][i] >= 0x200 && rac3_hth[b][i] <= 0x900, "hth[%u][%u] = 0x%x is out of range", b, i, rac3_hth[b][i]);
   printf("   the window: monotone, and Princen-Bradley\n");
   CHECK(rac3_window[0] > 0.0f && rac3_window[0] < 0.001f, "w[0] = %f", rac3_window[0]);
   CHECK(rac3_window[255] == 1.0f, "w[255] = %f", rac3_window[255]);
   for (i = 1; i < 256; i++)
      CHECK(rac3_window[i] >= rac3_window[i - 1], "the window falls at %u", i);
   for (i = 0; i < 256; i++)
   {
      float s = rac3_window[i] * rac3_window[i] + rac3_window[255 - i] * rac3_window[255 - i];
      CHECK(fabsf(s - 1.0f) < 0.0005f, "w[%u]^2 + w[%u]^2 = %f", i, 255 - i, s);
   }
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("rac3 tables: the standard's tables agree with each other\n");
   return 0;
}
