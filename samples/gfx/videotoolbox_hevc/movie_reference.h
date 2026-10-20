/* Independent BT.709 reference for native GPU pixel acceptance. */
#ifndef VT_HEVC_MOVIE_REFERENCE_H
#define VT_HEVC_MOVIE_REFERENCE_H
#include <math.h>
#include <stdint.h>
static void hevc_movie_reference(unsigned y, unsigned cb, unsigned cr,
      unsigned depth, unsigned full, unsigned transfer, unsigned char output[4])
{
   const double kr = 0.2126, kb = 0.0722, kg = 1.0 - kr - kb;
   double scale = depth == 10 ? 4.0 : 1.0;
   double maximum = depth == 10 ? 1023.0 : 255.0;
   double luma = (y - (full ? 0.0 : 16.0 * scale)) / (full ? maximum : 219.0 * scale);
   double blue = (cb - 128.0 * scale) / (full ? maximum : 224.0 * scale);
   double red = (cr - 128.0 * scale) / (full ? maximum : 224.0 * scale);
   double rgb[3];
   unsigned channel;
   rgb[0] = luma + 2.0 * (1.0 - kr) * red;
   rgb[1] = luma - 2.0 * kb * (1.0 - kb) / kg * blue - 2.0 * kr * (1.0 - kr) / kg * red;
   rgb[2] = luma + 2.0 * (1.0 - kb) * blue;
   for (channel = 0; channel < 3; channel++)
   {
      double value = rgb[channel] < 0.0 ? 0.0 : rgb[channel] > 1.0 ? 1.0 : rgb[channel];
      if (transfer == 1)
      {
         double linear = value < 0.081 ? value / 4.5 : pow((value + 0.099) / 1.099, 1.0 / 0.45);
         value = linear <= 0.0031308 ? 12.92 * linear : 1.055 * pow(linear, 1.0 / 2.4) - 0.055;
      }
      output[channel] = (unsigned char)floor(value * 255.0 + 0.5);
   }
   output[3] = 255;
}
static void hevc_movie_hdr_reference(unsigned y, unsigned cb, unsigned cr,
      unsigned depth, unsigned full, unsigned transfer, double output[4])
{
   const double kr = 0.2627, kb = 0.0593, kg = 1.0 - kr - kb;
   double scale = depth == 10 ? 4.0 : 1.0, maximum = depth == 10 ? 1023.0 : 255.0;
   double luma = (y - (full ? 0.0 : 16.0 * scale)) / (full ? maximum : 219.0 * scale);
   double blue = (cb - 128.0 * scale) / (full ? maximum : 224.0 * scale);
   double red = (cr - 128.0 * scale) / (full ? maximum : 224.0 * scale);
   double rgb[3], linear[3];
   unsigned channel;
   rgb[0] = luma + 2.0 * (1.0 - kr) * red;
   rgb[1] = luma - 2.0 * kb * (1.0 - kb) / kg * blue - 2.0 * kr * (1.0 - kr) / kg * red;
   rgb[2] = luma + 2.0 * (1.0 - kb) * blue;
   for (channel = 0; channel < 3; channel++)
   {
      double value = rgb[channel] < 0.0 ? 0.0 : rgb[channel] > 1.0 ? 1.0 : rgb[channel];
      if (transfer == 18)
         linear[channel] = value <= 0.5 ? value * value / 3.0
            : (exp((value - 0.55991073) / 0.17883277) + 0.28466892) / 12.0;
      else
      {
         double power = pow(value, 32.0 / 2523.0);
         double top = power - 3424.0 / 4096.0;
         linear[channel] = 125.0 * pow((top > 0.0 ? top : 0.0)
               / (2413.0 / 128.0 - 2392.0 / 128.0 * power), 16384.0 / 2610.0);
      }
   }
   if (transfer == 18)
   {
      double luminance = kr * linear[0] + kg * linear[1] + kb * linear[2];
      double gain = 12.5 * pow(luminance, 0.2);
      for (channel = 0; channel < 3; channel++) linear[channel] *= gain;
   }
   output[0] = 1.660491 * linear[0] - 0.587641 * linear[1] - 0.072850 * linear[2];
   output[1] = -0.124550 * linear[0] + 1.132900 * linear[1] - 0.008349 * linear[2];
   output[2] = -0.018151 * linear[0] - 0.100579 * linear[1] + 1.118730 * linear[2];
   output[3] = 1.0;
}
static double hevc_movie_half(uint16_t value)
{
   unsigned exponent = (value >> 10) & 31, fraction = value & 1023;
   double result = exponent ? ldexp(1.0 + fraction / 1024.0, (int)exponent - 15)
      : ldexp((double)fraction, -24);
   return value & 32768 ? -result : result;
}
#endif
