/* Independent BT.709 reference for native GPU pixel acceptance. */
#ifndef VT_HEVC_MOVIE_REFERENCE_H
#define VT_HEVC_MOVIE_REFERENCE_H
#include <math.h>
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
#endif
