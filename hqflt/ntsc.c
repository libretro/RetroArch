#include "ntsc.h"
#include <stdlib.h>
#include <stdbool.h>

void ntsc_filter(uint16_t * restrict out, const uint16_t * restrict in, int width, int height)
{
   static int phase = 0;
   static snes_ntsc_t ntsc;
   static bool inited = false;

   if (inited == false)
   {
      snes_ntsc_init(&ntsc, &snes_ntsc_composite);
      inited = true;
   }

   snes_ntsc_blit(&ntsc, in, width, phase, width, height, out, SNES_NTSC_OUT_WIDTH(width) * sizeof(uint16_t));
   phase ^= 1;
}
