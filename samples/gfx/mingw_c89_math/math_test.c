#include <math.h>
#include <assert.h>
int main(void)
{
   volatile float zero = 0.0f, fraction = 1.5f, two = 2.0f;
   assert(fabs(cosf(zero) - 1.0f) < 0.0001);
   assert(fabs(sinf(zero)) < 0.0001);
   assert(fabs(roundf(fraction) - 2.0f) < 0.0001);
   assert(fabs(floorf(fraction) - 1.0f) < 0.0001);
   assert(fabs(ceilf(fraction) - 2.0f) < 0.0001);
   assert(fabs(powf(two, two) - 4.0f) < 0.0001);
   assert(fabs(sqrtf(two * two) - 2.0f) < 0.0001);
   assert(fabs(expf(zero) - 1.0f) < 0.0001);
   assert(fabs(fabsf(-fraction) - fraction) < 0.0001);
   assert(lroundf(fraction) == 2);
   return 0;
}
