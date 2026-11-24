/* Render deterministic fixtures through the current setup or its explicitly
 * selected historical floating-point reference. LGPL-2.1-or-later. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
#define SOFTFILTER_NO_SIMD
#include "../../../gfx/video_filters/snes_ntsc/snes_ntsc.c"
int main(void)
{
   enum { W = 256, H = 32, OW = SNES_NTSC_OUT_WIDTH(W) };
   snes_ntsc_t *ntsc = (snes_ntsc_t*)calloc(1,sizeof *ntsc);
   uint16_t *input = (uint16_t*)malloc(W*H*sizeof *input);
   uint16_t *output = (uint16_t*)malloc(OW*H*sizeof *output);
   uint32_t rng = 12345;
   int test, n, phase;
   if (!ntsc || !input || !output) return 1;
#ifdef _WIN32
   if (_setmode(_fileno(stdout),_O_BINARY) == -1) return 1;
#endif
   for (n = 0; n < W*H; n++)
   {
      rng = rng*UINT32_C(1103515245) + 12345;
      input[n] = n < W*H/2 ? (uint16_t)((n*31/W) ^ (n/W*2081)) : (uint16_t)(rng >> 8);
   }
   for (test = 0; test < 28; test++)
   {
      snes_ntsc_setup_t setup = retroarch_snes_ntsc_composite;
      if (test == 1) setup.merge_fields = 0;
      if (test == 2) setup = retroarch_snes_ntsc_rgb;
      if (test == 3) setup = retroarch_snes_ntsc_svideo;
      if (test == 4) setup = retroarch_snes_ntsc_monochrome;
      if (test >= 5 && test < 25)
      {
         /* One control at each endpoint, including the near-singular sinc. */
         double value = test & 1 ? -1 : 1;
         switch ((test-5)/2)
         {
            case 0: setup.hue = value; break;
            case 1: setup.saturation = value; break;
            case 2: setup.contrast = value; break;
            case 3: setup.brightness = value; break;
            case 4: setup.sharpness = value; break;
            case 5: setup.gamma = value; break;
            case 6: setup.resolution = value; break;
            case 7: setup.artifacts = value; break;
            case 8: setup.fringing = value; break;
            case 9: setup.bleed = value; break;
         }
      }
      if (test >= 25)
      {
         static const float matrix[6] = { .956f, .621f, -.272f, -.647f, -1.105f, 1.702f };
         setup.decoder_matrix = matrix;
         setup.hue = test == 25 ? -0.37 : 0.41;
         setup.sharpness = test == 27 ? 0.00001 : -0.18;
         setup.resolution = 0.73;
         setup.gamma = -0.29;
         setup.saturation = 0.17;
      }
      retroarch_snes_ntsc_init(ntsc,&setup);
      for (phase = 0; phase < 3; phase++)
      {
         retroarch_snes_ntsc_blit(ntsc,input,W,phase,W,H,output,OW*2,0,1);
         for (n = 0; n < OW*H; n++)
         {
            /* Canonical little-endian samples, also on big-endian hosts. */
            if (putchar(output[n]&255) == EOF || putchar(output[n]>>8) == EOF) return 1;
         }
      }
   }
   free(output); free(input); free(ntsc);
   return 0;
}
