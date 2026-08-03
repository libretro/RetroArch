#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "snes_ntsc.h"
static uint32_t fnv1a(uint32_t h, const void *p, size_t n)
{ const uint8_t *b=p; size_t i; for(i=0;i<n;i++){h^=b[i];h*=0x01000193u;} return h; }
int main(void)
{
   static snes_ntsc_t ntsc;
   uint32_t h = 0x811c9dc5u;
   snes_ntsc_setup_t s;
   memset(&ntsc, 0, sizeof(ntsc));
   retroarch_snes_ntsc_init(&ntsc, 0);                       /* default */
   h = fnv1a(h, &ntsc, sizeof(ntsc));
   retroarch_snes_ntsc_init(&ntsc, &retroarch_snes_ntsc_svideo);
   h = fnv1a(h, &ntsc, sizeof(ntsc));
   retroarch_snes_ntsc_init(&ntsc, &retroarch_snes_ntsc_rgb);
   h = fnv1a(h, &ntsc, sizeof(ntsc));
   s = retroarch_snes_ntsc_composite;
   s.hue = 0.1f; s.saturation = -0.3f; s.artifacts = 0.5f;
   s.merge_fields = 0; s.sharpness = 0.7f;
   retroarch_snes_ntsc_init(&ntsc, &s);
   h = fnv1a(h, &ntsc, sizeof(ntsc));
   printf("ntsc table hash=%08x size=%u\n", h, (unsigned)sizeof(ntsc));
   return 0;
}
