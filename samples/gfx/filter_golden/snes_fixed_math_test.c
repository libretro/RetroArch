/* Arithmetic boundaries and independent wide-integer oracle. */
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "../../../gfx/video_filters/snes_ntsc/snes_ntsc_fixed_math.h"
static uint64_t state = UINT64_C(818131135413);
static uint64_t next_value(void)
{ state ^= state << 13; state ^= state >> 7; state ^= state << 17; return state; }
int main(void)
{
   static const int64_t edges[] = { INT64_MIN, INT64_MIN+1, -SNES_F_ONE,
      -1, 0, 1, SNES_F_ONE, INT64_MAX-1, INT64_MAX };
   unsigned n;
   assert(snes_f_setting(1.0) == SNES_F_ONE);
   assert(snes_f_setting(-1.0) == -SNES_F_ONE);
   assert(snes_f_setting(0.5) == SNES_F_ONE/2);
   assert(snes_f_setting(2.0) == SNES_F_ONE);
   assert(snes_f_decoder(-0.5f) == -SNES_F_ONE/2);
   assert(snes_f_decoder(32.0f) == 16*SNES_F_ONE);
   assert(snes_f_int(-3*SNES_F_ONE-SNES_F_ONE/2) == -3);
   assert(snes_f_int(INT64_MIN) == INT_MIN);
   assert(snes_f_int(INT64_MAX) == INT_MAX);
   assert(snes_f_div(0,0) == 0);
   assert(snes_f_div(1,0) == INT64_MAX);
   assert(snes_f_div(-1,0) == INT64_MIN);
#if defined(__SIZEOF_INT128__)
   for (n = 0; n < 100081; n++)
   {
      __extension__ typedef unsigned __int128 wide_t;
      int64_t a = n < 81 ? edges[n/9] : snes_f_sign(next_value()>>1, (int)(n&1));
      int64_t b = n < 81 ? edges[n%9] : snes_f_sign(next_value()>>1, (int)((n>>1)&1));
      uint64_t x = snes_f_abs(a), y = snes_f_abs(b);
      int neg = (a < 0) != (b < 0);
      uint64_t limit = (uint64_t)INT64_MAX + neg;
      wide_t product = ((wide_t)x*y + UINT64_C(2147483648)) >> 32;
      int64_t expected = snes_f_sign(product > limit ? limit : (uint64_t)product,neg);
      assert(snes_f_mul(a,b) == expected);
      if (y)
      {
         wide_t quotient = (((wide_t)x<<32) + y/2)/y;
         expected = snes_f_sign(quotient > limit ? limit : (uint64_t)quotient,neg);
         assert(snes_f_div(a,b) == expected);
      }
   }
#else
   (void)edges;
   (void)next_value;
#endif
   for (n = 1; n <= 1000; n++)
   {
      double x = n/1000.0;
      int64_t f = SNES_F_ONE*(int64_t)n/1000;
      assert(fabs((double)snes_f_log(f)/SNES_F_ONE-log(x)) < 0.000001);
      assert(fabs((double)snes_f_exp(-f*20)/SNES_F_ONE-exp(-x*20)) < 0.000001);
      assert(fabs((double)snes_f_pow(f,SNES_F(11333,10000))/SNES_F_ONE-pow(x,1.1333)) < 0.000001);
      assert(fabs((double)snes_f_cos(f*30)/SNES_F_ONE-cos(x*30)) < 0.000001);
      assert(fabs((double)snes_f_sin(-f*30)/SNES_F_ONE-sin(-x*30)) < 0.000001);
   }
   puts("SNES fixed arithmetic: boundaries, 100081 wide-oracle cases and function accuracy passed");
   return 0;
}
