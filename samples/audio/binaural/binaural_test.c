/* A layout rendered to two ears.
 *
 * What must hold: a speaker on the left arrives at the left ear
 * first, louder, and less filtered than at the right; the rear pair
 * arrives duller than the front pair, which is what says 'behind';
 * the centre and the LFE reach both ears alike; a source in the
 * middle of the stereo leaves at the level it arrived; nothing is
 * louder than it came in; and the two 5.1 layouts render the same
 * signal in the same pair to different directions only in that one
 * is called back and one side - each renders. Stereo rendered is a
 * mild crossfeed and no more. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../../../audio/audio_binaural.h"

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static float rms(const float *x, size_t n, unsigned stride, unsigned ch)
{
   double s = 0; size_t i;
   for (i = 0; i < n; i++) s += (double)x[i * stride + ch] * x[i * stride + ch];
   return (float)sqrt(s / n);
}

/* The lag, in samples, at which the right ear best matches the left. */
static int lag_of(const float *out, size_t n)
{
   int best = 0; double bestc = -1e30; int d;
   for (d = -40; d <= 40; d++)
   {
      double c = 0; size_t i;
      for (i = 64; i < n - 64; i++)
         c += (double)out[2 * i] * out[2 * (i + d) + 1];
      if (c > bestc) { bestc = c; best = d; }
   }
   return best;
}

/* Treble share: rms above ~3 kHz over the whole, by a crude one-pole
 * high-pass on the ear. */
static float treble_share(const float *out, size_t n, unsigned ch)
{
   double hp = 0, all = 0, y = 0, prev = 0; size_t i;
   float a = 1.0f - expf(-2.0f * 3.14159265f * 3000.0f / 48000.0f);
   for (i = 0; i < n; i++)
   {
      float x = out[2 * i + ch];
      y += a * (x - y);           /* low-passed */
      hp  += (x - y) * (x - y);   /* what is above */
      all += x * x;
      prev = x;
   }
   (void)prev;
   return all > 0 ? (float)sqrt(hp / all) : 0.0f;
}

/* White-ish noise on one input channel of the layout. */
static void noise_on(float *in, size_t n, unsigned ch, int slot)
{
   uint32_t x = 0x1234567u; size_t i;
   memset(in, 0, n * ch * sizeof(float));
   for (i = 0; i < n; i++)
   {
      x ^= x << 13; x ^= x >> 17; x ^= x << 5;
      in[i * ch + slot] = ((int32_t)x / 2147483648.0f) * 0.3f;
   }
}

int main(void)
{
   audio_binaural_t b;
   size_t n = 48000;
   float *in  = (float*)malloc(n * 8 * sizeof(float));
   float *out = (float*)malloc(n * 2 * sizeof(float));
   audio_upmix_t slots;

   printf("binaural:\n");
   printf("   refusals, and the counts\n");
   CHECK(!audio_binaural_init(&b, 0, 48000), "an empty layout was accepted");
   CHECK(!audio_binaural_init(&b, AUDIO_LAYOUT_STEREO | AUDIO_SPEAKER_FRONT_CENTER, 48000), "3.0 was accepted");
   CHECK(audio_binaural_init(&b, AUDIO_LAYOUT_5POINT1, 48000) && b.channels == 6 && b.nspeakers == 6, "5.1 is not six speakers");
   CHECK(audio_binaural_init(&b, AUDIO_LAYOUT_7POINT1, 48000) && b.channels == 8 && b.nspeakers == 8, "7.1 is not eight speakers");

   audio_upmix_init(&slots, AUDIO_LAYOUT_5POINT1, 48000);

   printf("   a left front speaker: left ear first, louder, brighter\n");
   audio_binaural_init(&b, AUDIO_LAYOUT_5POINT1, 48000);
   noise_on(in, n, 6, slots.fl);
   audio_binaural_process(&b, out, in, n);
   {
      float l = rms(out, n, 2, 0), r = rms(out, n, 2, 1);
      int lag = lag_of(out, n);
      CHECK(l > r * 1.2f, "left front: left ear %.3f, right ear %.3f", l, r);
      CHECK(lag > 0 && lag < 40, "left front: the right ear lags by %d samples", lag);
      CHECK(treble_share(out, n, 1) < treble_share(out, n, 0), "left front: the far ear is not duller (%.3f vs %.3f)", treble_share(out, n, 1), treble_share(out, n, 0));
      printf("      left ear %.3f, right ear %.3f, right lags %d samples (%.2f ms)\n", l, r, lag, lag / 48.0f);
   }

   printf("   the rear pair is duller than the front pair\n");
   {
      float front_t, rear_t;
      audio_binaural_init(&b, AUDIO_LAYOUT_5POINT1, 48000);
      noise_on(in, n, 6, slots.fl);
      audio_binaural_process(&b, out, in, n);
      front_t = treble_share(out, n, 0);
      audio_binaural_init(&b, AUDIO_LAYOUT_5POINT1, 48000);
      noise_on(in, n, 6, slots.bl);
      audio_binaural_process(&b, out, in, n);
      rear_t = treble_share(out, n, 0);
      CHECK(rear_t < front_t * 0.9f, "rear left is not duller than front left at the near ear (%.3f vs %.3f)", rear_t, front_t);
      printf("      near-ear treble share: front %.3f, rear %.3f\n", front_t, rear_t);
   }

   printf("   centre and LFE reach both ears alike\n");
   {
      unsigned which[2]; unsigned k;
      which[0] = slots.fc; which[1] = slots.lfe;
      for (k = 0; k < 2; k++)
      {
         float l, r;
         audio_binaural_init(&b, AUDIO_LAYOUT_5POINT1, 48000);
         noise_on(in, n, 6, which[k]);
         audio_binaural_process(&b, out, in, n);
         l = rms(out, n, 2, 0); r = rms(out, n, 2, 1);
         CHECK(fabsf(l - r) < 0.001f, "%s: ears differ, %.4f vs %.4f", k ? "LFE" : "centre", l, r);
         CHECK(lag_of(out, n) == 0, "%s: the ears are not in time", k ? "LFE" : "centre");
      }
   }

   printf("   a source in the middle of the stereo keeps its level; nothing is louder than it came in\n");
   {
      float in_rms, l, r; size_t i;
      audio_binaural_init(&b, AUDIO_LAYOUT_5POINT1, 48000);
      noise_on(in, n, 6, slots.fl);
      for (i = 0; i < n; i++) in[i * 6 + slots.fr] = in[i * 6 + slots.fl];   /* centred */
      in_rms = rms(in, n, 6, slots.fl);
      audio_binaural_process(&b, out, in, n);
      l = rms(out, n, 2, 0); r = rms(out, n, 2, 1);
      CHECK(l < in_rms * 1.05f && l > in_rms * 0.6f, "centred source: left ear %.3f for an input of %.3f", l, in_rms);
      CHECK(fabsf(l - r) < 0.002f, "centred source: ears differ, %.4f vs %.4f", l, r);
      printf("      input %.3f, ears %.3f / %.3f\n", in_rms, l, r);
      /* A full-scale centred stereo source - the front pair at 1.0 and
       * nothing else, the loudest thing a stereo core can send - must
       * arrive at exactly full scale and no more: the level scaling
       * is set by that pair. Six channels all in phase at full scale
       * is not a signal the upmix produces and is left to the
       * frontend's clamp before the device. */
      audio_binaural_init(&b, AUDIO_LAYOUT_5POINT1, 48000);
      memset(in, 0, n * 6 * sizeof(float));
      for (i = 0; i < n; i++) in[i * 6 + slots.fl] = in[i * 6 + slots.fr] = 1.0f;
      audio_binaural_process(&b, out, in, n);
      for (i = 64; i < n * 2; i++) if (fabsf(out[i]) > 1.0001f) { CHECK(0, "a full-scale centred source leaves at %.3f", out[i]); break; }
      CHECK(fabsf(out[2 * (n - 1)] - 1.0f) < 0.01f, "a full-scale centred source settles at %.3f, not 1.0", out[2 * (n - 1)]);
   }

   printf("   the two 5.1 layouts render their rear pair from different directions\n");
   {
      float back_t, side_t; audio_upmix_t s2;
      audio_upmix_init(&s2, AUDIO_LAYOUT_5POINT1_SURROUND, 48000);
      audio_binaural_init(&b, AUDIO_LAYOUT_5POINT1, 48000);
      noise_on(in, n, 6, slots.bl);
      audio_binaural_process(&b, out, in, n);
      back_t = rms(out, n, 2, 1) / rms(out, n, 2, 0);    /* far/near ratio */
      audio_binaural_init(&b, AUDIO_LAYOUT_5POINT1_SURROUND, 48000);
      noise_on(in, n, 6, s2.sl);
      audio_binaural_process(&b, out, in, n);
      side_t = rms(out, n, 2, 1) / rms(out, n, 2, 0);
      CHECK(fabsf(back_t - side_t) > 0.01f, "back left and side left render alike (%.3f vs %.3f far/near)", back_t, side_t);
      printf("      far/near ear ratio: back %.3f, side %.3f\n", back_t, side_t);
   }

   printf("   stereo rendered is a mild crossfeed\n");
   {
      float l, r;
      audio_binaural_init(&b, AUDIO_LAYOUT_STEREO, 48000);
      noise_on(in, n, 2, 0);
      audio_binaural_process(&b, out, in, n);
      l = rms(out, n, 2, 0); r = rms(out, n, 2, 1);
      CHECK(r > 0.1f * l && r < 0.9f * l, "stereo: the far ear gets %.2f of the near", r / l);
      printf("      far ear %.2f of near\n", r / l);
   }

   free(in); free(out);
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("binaural: left arrives left first, the rears are behind, the middle stays put, nothing gets louder\n");
   return 0;
}
