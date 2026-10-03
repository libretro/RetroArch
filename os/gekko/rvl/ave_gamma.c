/* The A/V encoder's gamma curve: out = in ^ (1 / gamma) over the
 * video range, as straight segments between fixed knees. */

#include <math.h>

#include "ave_gamma.h"

static const uint8_t knee[7] = { 16, 32, 64, 96, 128, 160, 235 };

static double curve(double v, double gamma)
{
   return 16.0 + 219.0 * pow((v - 16.0) / 219.0, 1.0 / gamma);
}

static unsigned fixed(double v, double one)
{
   double f = v * one + 0.5;
   return f >= 65535.0 ? 65535u : f <= 0.0 ? 0u : (unsigned)f;
}

void ave_gamma_table(unsigned tenths, uint8_t out[AVE_GAMMA_LEN])
{
   double y[7];
   double gamma;
   unsigned i;
   if (tenths < 1)
      tenths = 1;
   else if (tenths > 30)
      tenths = 30;
   gamma = tenths / 10.0;
   for (i = 0; i < 7; i++)
      y[i] = curve(knee[i], gamma);
   for (i = 0; i < 6; i++)
   {
      unsigned s = fixed((y[i + 1] - y[i]) / (knee[i + 1] - knee[i]),
            4096.0);
      out[i * 2]     = (uint8_t)(s >> 8);
      out[i * 2 + 1] = (uint8_t)s;
   }
   for (i = 0; i < 7; i++)
   {
      unsigned o = fixed(y[i], 256.0);
      out[12 + i]         = knee[i];
      out[19 + i * 2]     = (uint8_t)(o >> 8);
      out[19 + i * 2 + 1] = (uint8_t)o;
   }
}
