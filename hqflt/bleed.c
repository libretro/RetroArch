#include "bleed.h"
#include <stdlib.h>
#include <string.h>

void bleed_filter(uint16_t *data, int width, int height)
{
   uint16_t tmp[4];
   int ptr = 0;
   uint16_t r[4];
   uint16_t g[4];
   uint16_t b[4];
   uint16_t bleed_r;
   uint16_t bleed_g;
   uint16_t bleed_b;
   float rand_map[4];

   for (int h = 0; h < height; h++ )
   {
      memcpy(tmp, data, sizeof(tmp));
      ptr = 3;

      for (int i = 0; i < 4; i++)
      {
         rand_map[i] = (rand() % 20)/1000.0; // 0.02 * 4 = 0.08
      }

      for (int i = 3; i < width; i++, ptr++)
      {
         tmp[ptr & 0x3] = data[i];
         for (int y = 0; y < 4; y++)
         {
            r[y] = (tmp[(ptr + y) & 0x3] >> 10) & 0x1F;
            g[y] = (tmp[(ptr + y) & 0x3] >>  5) & 0x1F;
            b[y] = (tmp[(ptr + y) & 0x3] >>  0) & 0x1F;
         }

         bleed_r = r[0] * (0.05 + rand_map[0]  )  + r[1] * (0.10 + rand_map[1]  )  + r[2] * (0.20 + rand_map[2])   + r[3] * (0.57 + rand_map[3]  ); // 0.92
         bleed_g = g[0] * (0.03 + rand_map[2]/3)  + g[1] * (0.10 + rand_map[0]/3)  + g[2] * (0.20 + rand_map[3]/3) + g[3] * (0.63 + rand_map[1]/3); // 0.96
         bleed_b = b[0] * (0.05 + rand_map[3]  )  + b[1] * (0.10 + rand_map[2]  )  + b[2] * (0.20 + rand_map[1])   + b[3] * (0.57 + rand_map[0]  ); // 0.92

         data[i] = (bleed_r << 10) | (bleed_g << 5) | (bleed_b);
      }
      data += width;
   }
}
