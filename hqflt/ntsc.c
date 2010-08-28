#include "ntsc.h"
#include <stdlib.h>

void ntsc_filter(uint16_t * restrict out, const uint16_t * restrict in, int width, int height)
{
   static int phase = 0;
   snes_ntsc_t *ntsc = calloc(1, sizeof(snes_ntsc_t));
   snes_ntsc_init(ntsc, &snes_ntsc_composite);

   snes_ntsc_blit(ntsc, in, width, phase, width, height, out, SNES_NTSC_OUT_WIDTH(width) * sizeof(uint16_t));
   free(ntsc);
   phase = (phase + 1) % 4;
}
