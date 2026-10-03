/* rvl/ave_gamma.c: gamma 1.0 is the encoder's linear default, and
 * every curve rises through the video range with slopes matching its
 * outputs. */

#include <stdio.h>

#include "../../rvl/ave_gamma.h"

static int failures;

#define CHECK(cond) do { if (!(cond)) { \
   printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
   failures++; } } while (0)

static const uint8_t linear[AVE_GAMMA_LEN] = {
   0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00, 0x10, 0x00,
   0x10, 0x20, 0x40, 0x60, 0x80, 0xa0, 0xeb,
   0x10, 0x00, 0x20, 0x00, 0x40, 0x00, 0x60, 0x00, 0x80, 0x00, 0xa0, 0x00,
   0xeb, 0x00 };

static unsigned be16(const uint8_t *p)
{
   return (unsigned)p[0] << 8 | p[1];
}

int main(void)
{
   uint8_t t[AVE_GAMMA_LEN], lo[AVE_GAMMA_LEN], hi[AVE_GAMMA_LEN];
   unsigned g, i;

   ave_gamma_table(10, t);
   for (i = 0; i < AVE_GAMMA_LEN; i++)
      CHECK(t[i] == linear[i]);

   for (g = 1; g <= 30; g++)
   {
      ave_gamma_table(g, t);
      CHECK(be16(t + 19) == 16 * 256 && be16(t + 31) == 235 * 256);
      for (i = 0; i < 6; i++)
      {
         unsigned y0 = be16(t + 19 + i * 2), y1 = be16(t + 21 + i * 2);
         unsigned dx = t[13 + i] - t[12 + i];
         /* The slope reaches the next output to within rounding. */
         unsigned long reach = (unsigned long)y0
            + (unsigned long)be16(t + i * 2) * dx / 16;
         CHECK(y1 >= y0);
         CHECK(reach + dx >= y1 && reach <= y1 + dx);
      }
   }

   /* Higher gamma lifts the darks. */
   ave_gamma_table(5, lo);
   ave_gamma_table(20, hi);
   for (i = 1; i < 6; i++)
      CHECK(be16(lo + 19 + i * 2) < be16(hi + 19 + i * 2));
   /* Out of range clamps. */
   ave_gamma_table(0, lo);
   ave_gamma_table(1, hi);
   for (i = 0; i < AVE_GAMMA_LEN; i++)
      CHECK(lo[i] == hi[i]);
   ave_gamma_table(99, lo);
   ave_gamma_table(30, hi);
   for (i = 0; i < AVE_GAMMA_LEN; i++)
      CHECK(lo[i] == hi[i]);

   printf("ave_gamma: %s\n", failures ? "FAILED" : "ok");
   return failures ? 1 : 0;
}
