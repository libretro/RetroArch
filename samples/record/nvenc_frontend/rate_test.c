#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include "rate_fragment.c"
static void check(double fps, unsigned expected_num, unsigned expected_den)
{
   unsigned num = 0, den = 0;
   assert(record_nvenc_rate(fps, &num, &den));
   assert(num == expected_num && den == expected_den);
}
int main(void)
{
   unsigned num = 17, den = 19;
   check(60, 60, 1);
   check(50, 50, 1);
   check(60000.0 / 1001, 60000, 1001);
   check(24000.0 / 1001, 24000, 1001);
   check(240000.0 / 1001, 240000, 1001);
   check(59.94, 2997, 50);
   check(1, 1, 1);
   check(1000, 1000, 1);
   assert(!record_nvenc_rate(0, &num, &den));
   assert(!record_nvenc_rate(-1, &num, &den));
   assert(!record_nvenc_rate(1001, &num, &den));
   assert(!record_nvenc_rate(HUGE_VAL, &num, &den));
   assert(num == 17 && den == 19);
   puts("PASS: native frontend frame-rate rationalization");
   return 0;
}
