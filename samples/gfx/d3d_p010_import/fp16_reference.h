/* Independent double-precision scRGB oracle; test readback only. */
#include <math.h>
static double fp16_reference_half(uint16_t bits)
{
   unsigned exponent = (bits >> 10) & 31;
   double value = exponent ? ldexp(1.0 + (bits & 1023) / 1024.0,
         (int)exponent - 15) : ldexp((double)(bits & 1023), -24);
   return bits & 32768 ? -value : value;
}
static void fp16_reference(unsigned depth, unsigned full, unsigned transfer,
      unsigned flags, unsigned y, unsigned u, unsigned v, float out[3])
{
   double scale = depth == 10 ? 4.0 : 1.0;
   double ym = full ? (depth == 10 ? 1023.0 : 255.0) : 219.0 * scale;
   double cm = full ? ym : 224.0 * scale;
   double yy = (y - (full ? 0.0 : 16.0 * scale)) / ym;
   double cb = (u - 128.0 * scale) / cm;
   double cr = (v - 128.0 * scale) / cm;
   double rgb[3], linear[3];
   unsigned i;
   if (transfer == 16 || transfer == 18)
   {
      rgb[0] = yy + 1.4746 * cr;
      rgb[1] = yy - .16455313 * cb - .57135313 * cr;
      rgb[2] = yy + 1.8814 * cb;
   }
   else
   {
      rgb[0] = yy + (flags & IMAGE_YUV_FLAG_BT709 ? 1.5748 : 1.402) * cr;
      rgb[1] = yy - (flags & IMAGE_YUV_FLAG_BT709 ? .187324 : .344136) * cb
         - (flags & IMAGE_YUV_FLAG_BT709 ? .468124 : .714136) * cr;
      rgb[2] = yy + (flags & IMAGE_YUV_FLAG_BT709 ? 1.8556 : 1.772) * cb;
   }
   for (i = 0; i < 3; i++)
   {
      double x = rgb[i] < 0 ? 0 : rgb[i] > 1 ? 1 : rgb[i];
      if (transfer == 16)
      {
         double ep = pow(x, 1.0 / 78.84375);
         double n = ep > .8359375 ? ep - .8359375 : 0;
         linear[i] = pow(n / (18.8515625 - 18.6875 * ep),
               1.0 / .1593017578125) * 125.0;
      }
      else if (transfer == 18)
      {
         double scene = x <= .5 ? x * x / 3.0
            : (exp((x - .55991073) / .17883277) + .28466892) / 12.0;
         linear[i] = pow(scene, 1.2) * 12.5;
      }
      else
         linear[i] = x <= .04045 ? x / 12.92 : pow((x + .055) / 1.055, 2.4);
   }
   if (transfer == 16 || transfer == 18)
   {
      out[0] = (float)((6801 * linear[0] - 2407 * linear[1] - 298 * linear[2]) / 4096);
      out[1] = (float)((-510 * linear[0] + 4640 * linear[1] - 34 * linear[2]) / 4096);
      out[2] = (float)((-75 * linear[0] - 412 * linear[1] + 4583 * linear[2]) / 4096);
   }
   else
      for (i = 0; i < 3; i++) out[i] = (float)linear[i];
}
static int fp16_reference_check(const uint16_t *pixel, const float expected[3])
{
   unsigned i;
   for (i = 0; i < 4; i++)
   {
      double ref = i == 3 ? 1.0 : expected[i];
      double actual = fp16_reference_half(pixel[i]);
      if ((actual != actual || fabs(actual) > 65504.0) || fabs(actual - ref) > .001 + .002 * fabs(ref))
      {
         fprintf(stderr, "FP16 channel %u: %.8f != %.8f\n", i, actual, ref);
         return 0;
      }
   }
   return 1;
}
