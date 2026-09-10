/* The stereo-to-wider stage, at the last step before the device.
 *
 * A layout is a mask of speaker positions, interleaved in ascending
 * bit order; the count comes from the mask. What must hold: the front
 * pair carries the stereo unchanged; a centred mono voice lands in
 * the centre at the level a stereo pair would put it at; the rear
 * pair carries the stereo at -3 dB, at the back or at the sides as
 * the layout says - the same six-channel frame means two different
 * things under the two 5.1 masks, and the slots the signal lands in
 * differ; the LFE follows the bass and not the treble; stereo is a
 * copy; a mask the stage cannot fill is refused. And a frame in is a
 * frame out, whatever the count. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../../audio/audio_upmix.h"

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static float rms(const float *x, size_t n, unsigned stride, int ch)
{
   double s = 0; size_t i;
   if (ch < 0) return 0.0f;
   for (i = 0; i < n; i++) s += (double)x[i * stride + ch] * x[i * stride + ch];
   return (float)sqrt(s / n);
}

static void fill_tones(float *in, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++)
   {
      float t = (float)i / 48000.0f;
      float bass = 0.2f * sinf(2 * 3.14159265f * 40.0f * t);
      in[2 * i]     = 0.5f * sinf(2 * 3.14159265f * 1000.0f * t) + bass;
      in[2 * i + 1] = 0.5f * sinf(2 * 3.14159265f * 3000.0f * t) + bass;
   }
}

int main(void)
{
   audio_upmix_t up;
   size_t n = 48000, i;
   float *in  = (float*)malloc(n * 2 * sizeof(float));
   float *out = (float*)malloc(n * 8 * sizeof(float));
   struct { uint32_t layout; const char *name; unsigned ch; } cases[4] = {
      { AUDIO_LAYOUT_QUAD,             "quad",         4 },
      { AUDIO_LAYOUT_5POINT1,          "5.1 (back)",   6 },
      { AUDIO_LAYOUT_5POINT1_SURROUND, "5.1 (sides)",  6 },
      { AUDIO_LAYOUT_7POINT1,          "7.1",          8 },
   };
   unsigned k;

   printf("upmix:\n");
   printf("   layouts are masks: counts derive, positions are in ascending bit order\n");
   CHECK(audio_layout_channels(AUDIO_LAYOUT_STEREO) == 2, "stereo is not 2");
   CHECK(audio_layout_channels(AUDIO_LAYOUT_QUAD) == 4, "quad is not 4");
   CHECK(audio_layout_channels(AUDIO_LAYOUT_5POINT1) == 6, "5.1 is not 6");
   CHECK(audio_layout_channels(AUDIO_LAYOUT_5POINT1_SURROUND) == 6, "5.1 surround is not 6");
   CHECK(audio_layout_channels(AUDIO_LAYOUT_7POINT1) == 8, "7.1 is not 8");
   CHECK(AUDIO_LAYOUT_5POINT1 != AUDIO_LAYOUT_5POINT1_SURROUND, "the two 5.1 layouts are one value");
   /* 5.1: FL FR FC LFE BL BR - the SDL, WAVEFORMATEXTENSIBLE and
    * PipeWire order. Slot of BL is 4 in one, SL is 4 in the other. */
   audio_upmix_init(&up, AUDIO_LAYOUT_5POINT1, 48000);
   CHECK(up.fl == 0 && up.fr == 1 && up.fc == 2 && up.lfe == 3 && up.bl == 4 && up.br == 5 && up.sl < 0 && up.sr < 0,
         "5.1's slots are not FL FR FC LFE BL BR");
   audio_upmix_init(&up, AUDIO_LAYOUT_5POINT1_SURROUND, 48000);
   CHECK(up.fl == 0 && up.fr == 1 && up.fc == 2 && up.lfe == 3 && up.sl == 4 && up.sr == 5 && up.bl < 0 && up.br < 0,
         "5.1 surround's slots are not FL FR FC LFE SL SR");
   audio_upmix_init(&up, AUDIO_LAYOUT_7POINT1, 48000);
   CHECK(up.bl == 4 && up.br == 5 && up.sl == 6 && up.sr == 7, "7.1's rear slots are not BL BR SL SR");

   printf("   refusals\n");
   CHECK(!audio_upmix_init(&up, AUDIO_LAYOUT_STEREO | AUDIO_SPEAKER_FRONT_CENTER, 48000) && up.layout == AUDIO_LAYOUT_STEREO, "3.0 was accepted");
   CHECK(!audio_upmix_init(&up, AUDIO_LAYOUT_QUAD | AUDIO_SPEAKER_LOW_FREQUENCY, 48000), "4.1 was accepted");
   CHECK(!audio_upmix_init(&up, 0, 48000) && up.layout == AUDIO_LAYOUT_STEREO, "an empty mask was accepted");
   CHECK(audio_upmix_init(&up, AUDIO_LAYOUT_STEREO, 48000) && up.layout == AUDIO_LAYOUT_STEREO, "stereo was refused");

   fill_tones(in, n);
   printf("   stereo is a copy\n");
   audio_upmix_init(&up, AUDIO_LAYOUT_STEREO, 48000);
   audio_upmix_process(&up, out, in, n);
   CHECK(memcmp(out, in, n * 2 * sizeof(float)) == 0, "stereo changed the samples");

   for (k = 0; k < 4; k++)
   {
      unsigned ch = cases[k].ch;
      float l_in = rms(in, n, 2, 0), r_in = rms(in, n, 2, 1);
      printf("   %-12s fronts unchanged, rear pair at -3 dB in its own slots", cases[k].name);
      CHECK(audio_upmix_init(&up, cases[k].layout, 48000) && up.channels == ch, "%s refused", cases[k].name);
      memset(out, 0, n * 8 * sizeof(float));
      audio_upmix_process(&up, out, in, n);
      for (i = 0; i < n; i++)
         if (out[i * ch] != in[2 * i] || out[i * ch + 1] != in[2 * i + 1])
         { CHECK(0, "%s: the fronts differ at frame %u", cases[k].name, (unsigned)i); break; }
      {
         int rl = up.bl >= 0 ? up.bl : up.sl, rr = up.br >= 0 ? up.br : up.sr;
         float bl = rms(out, n, ch, rl), br = rms(out, n, ch, rr);
         CHECK(fabsf(bl / l_in - 0.7071f) < 0.01f, "%s: rear left is %.3f of front, not 0.707", cases[k].name, bl / l_in);
         CHECK(fabsf(br / r_in - 0.7071f) < 0.01f, "%s: rear right is %.3f of front, not 0.707", cases[k].name, br / r_in);
         /* The slots the layout does not have carry nothing. */
         if (up.bl < 0 && ch == 6)
            CHECK(rms(out, n, ch, 4) > 0.2f, "%s: slot 4 (SL) is empty", cases[k].name);
      }
      if (ch >= 6)
      {
         float c = rms(out, n, ch, up.fc);
         double sum2 = 0, bass2 = 0; float sum_rms, bass_rms, lfe;
         for (i = 0; i < n; i++) { double m = 0.5 * (in[2 * i] + in[2 * i + 1]); sum2 += m * m; }
         sum_rms = (float)sqrt(sum2 / n);
         CHECK(fabsf(c / sum_rms - 1.0f) < 0.01f, "%s: the centre is %.3f of the -6 dB sum", cases[k].name, c / sum_rms);
         for (i = 0; i < n; i++) { double b = 0.2 * sin(2 * 3.14159265 * 40.0 * i / 48000.0); bass2 += b * b; }
         bass_rms = (float)sqrt(bass2 / n);
         lfe = rms(out + (n / 2) * ch, n / 2, ch, up.lfe);
         printf(", LFE %.2f of the bass", lfe / bass_rms);
         CHECK(lfe / bass_rms > 0.8f && lfe / bass_rms < 1.05f, "%s: the LFE carries %.2f of the bass", cases[k].name, lfe / bass_rms);
         {
            audio_upmix_t up2; float *nb = (float*)malloc(n * 2 * sizeof(float)); float *o2 = (float*)malloc(n * 8 * sizeof(float));
            for (i = 0; i < n; i++) { float t = (float)i / 48000.0f; nb[2 * i] = 0.5f * sinf(2 * 3.14159265f * 3000.0f * t); nb[2 * i + 1] = nb[2 * i]; }
            audio_upmix_init(&up2, cases[k].layout, 48000);
            audio_upmix_process(&up2, o2, nb, n);
            CHECK(rms(o2 + (n / 2) * ch, n / 2, ch, up2.lfe) < 0.03f, "%s: 3 kHz reaches the LFE at %.3f", cases[k].name, rms(o2 + (n / 2) * ch, n / 2, ch, up2.lfe));
            free(nb); free(o2);
         }
      }
      if (ch == 8)
      {
         float sl = rms(out, n, 8, up.sl), sr = rms(out, n, 8, up.sr);
         CHECK(fabsf(sl / l_in - 0.7071f) < 0.01f, "7.1 sides: left is %.3f of front", sl / l_in);
         CHECK(fabsf(sr / r_in - 0.7071f) < 0.01f, "7.1 sides: right is %.3f of front", sr / r_in);
      }
      printf("\n");
   }

   free(in); free(out);
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("upmix: a layout is positions, not a count; the fronts are the stereo, the room is filled at -3 dB, the LFE follows the bass\n");
   return 0;
}
