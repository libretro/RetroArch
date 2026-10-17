#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <boolean.h>
#include "../../../audio/audio_upmix.h"
#include <formats/rac3.h>
#include "layout_fragment.c"
int main(void)
{
   static const uint32_t bases[] = {4, 3, 7, 0x103, 0x107, 0x603, 0x607};
   static const uint32_t refused[] = {0, 8, 0x33, 0x3f, 0x63f, 0x1003};
   unsigned i, lfe;
   for (i = 0; i < sizeof(bases) / sizeof(bases[0]); i++)
      for (lfe = 0; lfe < 2; lfe++)
      {
         uint32_t mask = bases[i] | (lfe ? 8 : 0);
         unsigned channels = audio_layout_channels(mask);
         rac3_encoder_t *encoder;
         assert(record_nvenc_layout(mask, channels));
         assert(!record_nvenc_layout(mask, channels + 1));
         encoder = rac3_encoder_new(48000, mask, 448);
         assert(encoder);
         rac3_encoder_free(encoder);
      }
   for (i = 0; i < sizeof(refused) / sizeof(refused[0]); i++)
      assert(!record_nvenc_layout(refused[i], audio_layout_channels(refused[i])));
   puts("PASS: native speaker-mask admission agrees with the AC-3 encoder");
   return 0;
}
