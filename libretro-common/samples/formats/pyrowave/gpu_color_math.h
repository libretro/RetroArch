/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#ifndef RPYROWAVE_GPU_COLOR_MATH_H
#define RPYROWAVE_GPU_COLOR_MATH_H

static double rpy_color_to2020[9], rpy_color_to709[9];
static double rpy_color_inverse_scale[3];

static void rpy_color_invert(const double *a, double *b)
{
   double d = a[0] * (a[4] * a[8] - a[5] * a[7])
      - a[1] * (a[3] * a[8] - a[5] * a[6]) + a[2] * (a[3] * a[7] - a[4] * a[6]);
   b[0] = (a[4] * a[8] - a[5] * a[7]) / d;
   b[1] = (a[2] * a[7] - a[1] * a[8]) / d;
   b[2] = (a[1] * a[5] - a[2] * a[4]) / d;
   b[3] = (a[5] * a[6] - a[3] * a[8]) / d;
   b[4] = (a[0] * a[8] - a[2] * a[6]) / d;
   b[5] = (a[2] * a[3] - a[0] * a[5]) / d;
   b[6] = (a[3] * a[7] - a[4] * a[6]) / d;
   b[7] = (a[1] * a[6] - a[0] * a[7]) / d;
   b[8] = (a[0] * a[4] - a[1] * a[3]) / d;
}

static void rpy_color_xyz(const double *xy, double *matrix)
{
   double unit[9], inverse[9], scale[3];
   double white[3];
   unsigned row, col;
   white[0] = 0.3127 / 0.3290; white[1] = 1.0; white[2] = (1.0 - 0.3127 - 0.3290) / 0.3290;
   for (col = 0; col < 3; col++)
   {
      unit[col] = xy[col * 2] / xy[col * 2 + 1];
      unit[3 + col] = 1.0;
      unit[6 + col] = (1.0 - xy[col * 2] - xy[col * 2 + 1]) / xy[col * 2 + 1];
   }
   rpy_color_invert(unit, inverse);
   for (row = 0; row < 3; row++)
      scale[row] = inverse[row * 3] * white[0] + inverse[row * 3 + 1] * white[1]
         + inverse[row * 3 + 2] * white[2];
   for (row = 0; row < 3; row++)
      for (col = 0; col < 3; col++) matrix[row * 3 + col] = unit[row * 3 + col] * scale[col];
}

static void rpy_color_math_init(void)
{
   static const double xy709[6] = { 0.64, 0.33, 0.30, 0.60, 0.15, 0.06 };
   static const double xy2020[6] = { 0.708, 0.292, 0.170, 0.797, 0.131, 0.046 };
   double a[9], b[9], ia[9], ib[9];
   unsigned row, col, k;
   rpy_color_xyz(xy709, a); rpy_color_xyz(xy2020, b);
   rpy_color_invert(a, ia); rpy_color_invert(b, ib);
   for (row = 0; row < 3; row++)
      for (col = 0; col < 3; col++)
      {
         rpy_color_to2020[row * 3 + col] = rpy_color_to709[row * 3 + col] = 0.0;
         for (k = 0; k < 3; k++)
         {
            rpy_color_to2020[row * 3 + col] += ib[row * 3 + k] * a[k * 3 + col];
            rpy_color_to709[row * 3 + col] += ia[row * 3 + k] * b[k * 3 + col];
         }
      }
}

static double rpy_color_transfer(double value, unsigned pq, unsigned inverse)
{
   if (!pq)
   {
      value = rpy_color_clip(value);
      if (inverse) return value <= 0.04045 ? value / 12.92 : pow((value + 0.055) / 1.055, 2.4);
      return value <= 0.0031308 ? value * 12.92 : 1.055 * pow(value, 1.0 / 2.4) - 0.055;
   }
   if (inverse)
   {
      double power = pow(rpy_color_clip(value), 32.0 / 2523.0);
      double n = power - 3424.0 / 4096.0;
      if (n < 0.0) n = 0.0;
      return (10000.0 / 80.0) * pow(n / (2413.0 / 128.0 - (2392.0 / 128.0) * power), 16384.0 / 2610.0);
   }
   else
   {
      double power = pow(rpy_color_clip(value * 80.0 / 10000.0), 2610.0 / 16384.0);
      return pow((3424.0 / 4096.0 + (2413.0 / 128.0) * power)
            / (1.0 + (2392.0 / 128.0) * power), 2523.0 / 32.0);
   }
}

static void rpy_color_decode_light(double *rgb, const rpyrowave_color_parameters_t *p)
{
   double light[3];
   unsigned row, col;
   for (row = 0; row < 3; row++)
      light[row] = rpy_color_transfer(rgb[row], p->colorimetry & 2, 1);
   for (row = 0; row < 3; row++)
   {
      rgb[row] = light[row];
      rpy_color_inverse_scale[row] = fabs(light[row]);
      if (!(p->colorimetry & 1)) continue;
      rgb[row] = 0.0;
      rpy_color_inverse_scale[row] = 0.0;
      for (col = 0; col < 3; col++)
      {
         double term = rpy_color_to709[row * 3 + col] * light[col];
         rgb[row] += term;
         rpy_color_inverse_scale[row] += fabs(term);
      }
   }
}

static double rpy_color_sample(unsigned pixel, unsigned channel)
{
   static const double gray[9] = { 0.0, 1.0, 0.0031308, 0.0031307, 0.0031309, 12.5, 125.0, 250.0, -1.0 };
   unsigned pattern = pixel % 16;
   if (pattern < 9) return gray[pattern];
   if (pattern < 15) return rpy_color_to709[channel * 3 + (pattern - 9) % 3] * (pattern < 12 ? 12.5 : 0.5);
   return (double)((pixel * 13 + channel * 73) % 257) / 32.0 - 0.5;
}
#endif
