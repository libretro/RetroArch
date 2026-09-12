#ifndef TEST_GOERTZEL_H
#define TEST_GOERTZEL_H

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Goertzel magnitude of x[0..n-1] at digital frequency f (cycles/sample):
 * a single-bin DFT, cheaper than a full transform when only one known
 * frequency matters. Shared by the time-stretch and low-pass tests. */
static double goertzel_mag(const double *x, int n, double f)
{
   double w     = 2.0 * M_PI * f;
   double coeff = 2.0 * cos(w);
   double q0;
   double q1    = 0.0;
   double q2    = 0.0;
   double real;
   double imag;
   int    i;
   for (i = 0; i < n; i++)
   {
      q0 = (coeff * q1) - q2 + x[i];
      q2 = q1;
      q1 = q0;
   }
   real = q1 - (q2 * cos(w));
   imag = q2 * sin(w);
   return sqrt((real * real) + (imag * imag));
}

#endif
