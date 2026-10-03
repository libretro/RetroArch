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
   /* The int16 form against the float form: the same gains and the
    * same LFE filter, within the rounding of an int16, on a stereo
    * tone set through 5.1 - what an int16 pipeline into an int16
    * device now runs instead of widening to float and back. */
   printf("   int16 upmix agrees with the float one to the LSB\n");
   {
      audio_upmix_t uf, ui;
      size_t m = 4800, j;
      int16_t *in16 = (int16_t*)malloc(m * 2 * sizeof(int16_t));
      int16_t *o16  = (int16_t*)malloc(m * 6 * sizeof(int16_t));
      float   *inf  = (float*)malloc(m * 2 * sizeof(float));
      float   *of   = (float*)malloc(m * 6 * sizeof(float));
      int worst = 0;
      audio_upmix_init(&uf, AUDIO_LAYOUT_5POINT1, 48000);
      audio_upmix_init(&ui, AUDIO_LAYOUT_5POINT1, 48000);
      for (j = 0; j < m; j++)
      {
         in16[2 * j]     = (int16_t)(20000.0 * sin(2.0 * 3.14159265358979 * 60.0 * (double)j / 48000.0)
                                  + 8000.0 * sin(2.0 * 3.14159265358979 * 3000.0 * (double)j / 48000.0));
         in16[2 * j + 1] = (int16_t)(15000.0 * sin(2.0 * 3.14159265358979 * 90.0 * (double)j / 48000.0));
         inf[2 * j]      = in16[2 * j] / 32768.0f;
         inf[2 * j + 1]  = in16[2 * j + 1] / 32768.0f;
      }
      audio_upmix_process(&uf, of, inf, m);
      audio_upmix_process_s16(&ui, o16, in16, m);
      {
         /* the LFE against a double-precision run of the same filter:
          * the float form's own rounding is a few LSB there, the
          * int16 form's state is Q30 */
         double lfe = 0.0, a = uf.lfe_coeff;
         int worst_f = 0, worst_i = 0;
         for (j = 0; j < m; j++)
         {
            double mm = ((double)in16[2 * j] + in16[2 * j + 1]) * 0.5 / 32768.0;
            int ref, df, di;
            lfe += a * (mm - lfe);
            ref = (int)floor(lfe * 32768.0 + 0.5);
            df  = abs(ref - (int)floor(of[j * 6 + 3] * 32768.0f + 0.5f));
            di  = abs(ref - (int)o16[j * 6 + 3]);
            if (df > worst_f) worst_f = df;
            if (di > worst_i) worst_i = di;
         }
         printf("      LFE against double precision: float form %d LSB, int16 form %d LSB\n", worst_f, worst_i);
         CHECK(worst_i <= 1, "the int16 LFE is %d LSB from the double-precision filter", worst_i);
      }
      for (j = 0; j < m * 6; j++)
      {
         int ref, d;
         if (j % 6 == 3) continue;   /* the LFE is judged above */
         ref = (int)floor(of[j] * 32768.0f + 0.5f);
         d   = abs(ref - (int)o16[j]);
         if (d > worst) worst = d;
      }
      printf("      worst difference elsewhere: %d LSB over %u samples of 5.1\n", worst, (unsigned)(m * 6));
      CHECK(worst <= 1, "int16 upmix is %d LSB from the float one", worst);
      free(in16); free(o16); free(inf); free(of);
   }

   /* The fold the multi-channel batch entry uses: a core's wider
    * frame to the stereo pipeline. Each position at its BS.775 gain,
    * the LFE dropped, int16 the float to the bit, mono to both. */
   printf("   downmix: a wider frame to stereo at the ITU gains\n");
   {
      float  f6[6]  = { 0.5f, -0.25f, 0.4f, 0.9f, 0.2f, -0.3f };  /* FL FR FC LFE BL BR */
      float  o[2]   = { 0, 0 };
      int16_t i6[6], oi[2];
      float  el, er;
      unsigned c;
      audio_downmix_f32(o, f6, 1, AUDIO_LAYOUT_5POINT1, 6);
      el = 0.5f + 0.70710678f * 0.4f + 0.70710678f * 0.2f;
      er = -0.25f + 0.70710678f * 0.4f + 0.70710678f * -0.3f;
      CHECK(fabsf(o[0] - el) < 1e-6f && fabsf(o[1] - er) < 1e-6f,
            "5.1 folds to %.4f %.4f, expected %.4f %.4f", o[0], o[1], el, er);
      for (c = 0; c < 6; c++) i6[c] = (int16_t)(f6[c] * 32767.0f);
      audio_downmix_s16(oi, i6, 1, AUDIO_LAYOUT_5POINT1, 6);
      CHECK(abs((int)oi[0] - (int)(el * 32767.0f)) <= 2 && abs((int)oi[1] - (int)(er * 32767.0f)) <= 2,
            "int16 fold gives %d %d, expected about %d %d", oi[0], oi[1], (int)(el * 32767.0f), (int)(er * 32767.0f));
      /* sides fold as the back pair; a lone back centre at -6 dB each */
      audio_downmix_f32(o, f6, 1, AUDIO_LAYOUT_5POINT1_SURROUND, 6);
      CHECK(fabsf(o[0] - el) < 1e-6f, "5.1 sides folds differently from 5.1 back");
      {
         float f3[3] = { 0.5f, -0.25f, 0.8f };   /* FL FR BC */
         audio_downmix_f32(o, f3, 1, AUDIO_LAYOUT_STEREO | AUDIO_SPEAKER_BACK_CENTER, 3);
         CHECK(fabsf(o[0] - (0.5f + 0.4f)) < 1e-6f && fabsf(o[1] - (-0.25f + 0.4f)) < 1e-6f,
               "a back centre folds at %.3f into left, expected -6 dB", o[0] - 0.5f);
      }
      {
         float m = 0.3f;
         audio_downmix_f32(o, &m, 1, AUDIO_SPEAKER_FRONT_CENTER, 1);
         CHECK(o[0] == 0.3f && o[1] == 0.3f, "mono is not both sides at unity");
      }
      /* saturation on correlated int16 content */
      {
         int16_t loud[6] = { 30000, 30000, 30000, 0, 30000, 30000 };
         audio_downmix_s16(oi, loud, 1, AUDIO_LAYOUT_5POINT1, 6);
         CHECK(oi[0] == 32767 && oi[1] == 32767, "correlated loud content did not saturate (%d)", oi[0]);
      }
      CHECK(audio_layout_known(AUDIO_LAYOUT_7POINT1) && !audio_layout_known(0x800u) && !audio_layout_known(0),
            "layout_known is wrong at the edges");
   }

   /* The remap the recorder's push uses: a batch of one layout to a
    * recording of another. */
   printf("   remap: a batch to a recording of another layout\n");
   {
      int16_t s51[6] = { 1000, -2000, 3000, 4000, 5000, -6000 };   /* FL FR FC LFE BL BR */
      int16_t st[2]  = { 700, -800 };
      int16_t out[8];
      /* 5.1 to 5.1: a copy */
      audio_layout_remap_s16(out, AUDIO_LAYOUT_5POINT1, s51, AUDIO_LAYOUT_5POINT1, 1);
      CHECK(memcmp(out, s51, sizeof(s51)) == 0, "equal layouts are not a copy");
      /* stereo to 5.1: fronts, the rest zero */
      audio_layout_remap_s16(out, AUDIO_LAYOUT_5POINT1, st, AUDIO_LAYOUT_STEREO, 1);
      CHECK(out[0] == 700 && out[1] == -800 && !out[2] && !out[3] && !out[4] && !out[5],
            "stereo into 5.1 is not fronts and silence (%d %d %d %d %d %d)", out[0], out[1], out[2], out[3], out[4], out[5]);
      /* 5.1 to stereo: the fold, as audio_downmix_s16 */
      audio_layout_remap_s16(out, AUDIO_LAYOUT_STEREO, s51, AUDIO_LAYOUT_5POINT1, 1);
      CHECK(abs(out[0] - (1000 + (int)(0.70710678f * 3000) + (int)(0.70710678f * 5000))) <= 2,
            "5.1 into stereo left is %d", out[0]);
      CHECK(abs(out[1] - (-2000 + (int)(0.70710678f * 3000) + (int)(0.70710678f * -6000))) <= 2,
            "5.1 into stereo right is %d", out[1]);
      /* 5.1 to 7.1: the six in place, the side pair zero */
      audio_layout_remap_s16(out, AUDIO_LAYOUT_7POINT1, s51, AUDIO_LAYOUT_5POINT1, 1);
      CHECK(memcmp(out, s51, sizeof(s51)) == 0 && !out[6] && !out[7], "5.1 into 7.1 is not in place");
      /* 5.1 sides to 5.1 back: the side pair has no slot, folds to the fronts */
      audio_layout_remap_s16(out, AUDIO_LAYOUT_5POINT1, s51, AUDIO_LAYOUT_5POINT1_SURROUND, 1);
      CHECK(out[4] == 0 && out[5] == 0 && abs(out[0] - (1000 + (int)(0.70710678f * 5000))) <= 2,
            "sides into a back recording: back %d %d, left %d", out[4], out[5], out[0]);
   }

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("upmix: a layout is positions, not a count; the fronts are the stereo, the room is filled at -3 dB, the LFE follows the bass\n");
   return 0;
}
