#include <assert.h>
#include <stdio.h>
#include "movie_reference.h"
#include "../metal_frame/sdr_pixels.h"

static unsigned code(unsigned f, unsigned x, unsigned y, unsigned plane, unsigned channel)
{
   unsigned scale = f >= 2 ? 4 : 1, maximum = f >= 2 ? 1023 : 255;
   unsigned black = f & 1 ? 0 : 16 * scale, white = f & 1 ? maximum : 235 * scale;
   unsigned center = 128 * scale, low = f & 1 ? 0 : 16 * scale, high = f & 1 ? maximum : 240 * scale;
   unsigned ys[10], cb[5], cr[5];
   ys[0] = black; ys[1] = black + 1; ys[2] = black + 4 * scale; ys[3] = black + 18 * scale;
   ys[4] = (black + white) / 2; ys[5] = white - 1; ys[6] = white;
   ys[7] = white < maximum ? white + 1 : maximum; ys[8] = maximum; ys[9] = 0;
   cb[0] = center; cb[1] = center - 40 * scale; cb[2] = center + 40 * scale; cb[3] = low; cb[4] = high;
   cr[0] = center; cr[1] = center + 24 * scale; cr[2] = center - 48 * scale; cr[3] = high; cr[4] = low;
   return !plane ? ys[(x + y * 3) % 10] : channel ? cr[(x * 2 + y * 3) % 5] : cb[(x + y) % 5];
}
int main(void)
{
   unsigned f, transfer, x, y, channel;
   for (f = 0; f < 4; f++) for (transfer = 0; transfer < 2; transfer++)
      for (y = 0; y < 5; y++) for (x = 0; x < 7; x++)
      {
         unsigned char actual[4];
         hevc_movie_reference(code(f, x + 2, y + 2, 0, 0),
               code(f, (x + 2) / 2, (y + 2) / 2, 1, 0),
               code(f, (x + 2) / 2, (y + 2) / 2, 1, 1),
               f < 2 ? 8 : 10, f & 1, transfer ? 13 : 1, actual);
         for (channel = 0; channel < 4; channel++)
            assert(actual[channel] == sdr_expected[f * 2 + transfer][(y * 7 + x) * 4 + channel]);
      }
   puts("HEVC movie reference: 1120 fixed BT.709/sRGB RGBA components passed");
   return 0;
}
