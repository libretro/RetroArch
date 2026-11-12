/* Copyright (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------
 * The following license statement only applies to this file (raac_encode_test.c).
 * ---------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* raac_encode, checked by raac. Signals made here - tones, tones with
 * a quiet one under them, noise, a loud start after silence - mono and
 * stereo, at several rates:
 *   - raac decodes every frame without error, from the ASC the encoder
 *     gives, which the ADTS header in front of each frame matches;
 *   - what it decodes is the input a frame late, at the level it went
 *     in and within the signal-to-noise ratio the rate gives (noise
 *     keeps none of its detail at these rates, tones nearly all);
 *   - noise, which takes every bit offered, lands within 3% of the rate,
 *     and nothing goes over it by more;
 *   - the f32 entry point writes what the s16 one does for the same
 *     samples;
 *   - the flush call gives the last frame's samples;
 *   - a buffer too small gives 0, a bad configuration no encoder.
 *
 * With a directory argument the streams are written there as ADTS, with
 * raac's decoding next to each, for the CI workflow to decode with
 * FFmpeg; "--compare a.s16 b.s16" then checks FFmpeg's output against
 * raac's.
 *
 * Build and run:  make && ./raac_encode_test [dir]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <boolean.h>
#include <formats/raac.h>
#include <formats/raac_encode.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static unsigned failures;

#define CHECK(c) do { if (!(c)) { printf("  FAIL  %s (line %u)\n", #c, \
      (unsigned)__LINE__); failures++; } } while (0)

enum { SIG_TONES, SIG_QUIET, SIG_NOISE, SIG_ATTACK };

#define MAX_FRAMES 200
static int16_t  pcm[(MAX_FRAMES + 1) * 1024 * 2];
static int16_t  dec[(MAX_FRAMES + 2) * RAAC_MAX_FRAME * 2];
static float    pcm_f[1024 * 2];

/* @frames of signal @sig, left and right different */
static void make_signal(int sig, unsigned rate, unsigned ch, unsigned frames)
{
   unsigned long seed = 12345;
   unsigned      i, c, n = frames * 1024;
   double        lp[2] = { 0, 0 };
   for (i = 0; i < n; i++)
      for (c = 0; c < ch; c++)
      {
         double t = (double)i / rate, v = 0;
         switch (sig)
         {
            case SIG_TONES:
               v = 0.25 * sin(2 * M_PI * (220 + 110 * c) * t)
                 + 0.12 * sin(2 * M_PI * 1330 * t)
                 + 0.06 * sin(2 * M_PI * 3100 * t) * sin(2 * M_PI * 0.7 * t);
               break;
            case SIG_QUIET:
               /* a tone 40 dB under another, both to be kept */
               v = 0.3 * sin(2 * M_PI * 500 * t)
                 + 0.003 * sin(2 * M_PI * (2000 + 300 * c) * t);
               break;
            case SIG_NOISE:
               /* white noise through a one-pole low pass: darker, like
                * most programme */
               seed  = seed * 1103515245UL + 12345UL;
               lp[c] = 0.8 * lp[c] + 0.2 * (((double)((seed >> 8) & 0xFFFF) / 32768.0) - 1.0);
               v     = 1.2 * lp[c];
               break;
            case SIG_ATTACK:
               v = i < n / 2 ? 0 : 0.5 * sin(2 * M_PI * 700 * t)
                  * exp(-(double)(i - n / 2) / rate * 3);
               break;
         }
         pcm[i * ch + c] = (int16_t)floor(v * 32767 + 0.5);
      }
   memset(pcm + n * ch, 0, 1024 * ch * sizeof(int16_t));
}

static unsigned rate_index(unsigned rate)
{
   static const unsigned rates[] = { 96000, 88200, 64000, 48000, 44100,
      32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350 };
   unsigned i;
   for (i = 0; i < 13; i++)
      if (rates[i] == rate)
         return i;
   return 15;
}

static void run(const char *dir, const char *name, int sig, unsigned rate,
      unsigned ch, uint32_t bitrate, unsigned frames, double min_snr,
      bool rate_met)
{
   raac_encoder_config_t c;
   raac_encoder_t *e, *ef;
   raac_t   *d;
   uint8_t   out[RAAC_ENCODE_MAX_FRAME], out_f[RAAC_ENCODE_MAX_FRAME], asc[2];
   unsigned  f, errors = 0, adts_ok = 1, same_f32 = 1, i;
   size_t    n, total = 0, decoded = 0, peak = 0;
   double    se = 0, ss = 0, sx = 0, snr, kbps, gain;
   FILE     *file = NULL, *file_d = NULL;

   memset(&c, 0, sizeof(c));
   c.sample_rate = rate;
   c.channels    = ch;
   c.bitrate     = bitrate;
   e  = raac_encoder_new(&c);
   ef = raac_encoder_new(&c);
   CHECK(e && ef);
   if (!e || !ef)
      return;
   make_signal(sig, rate, ch, frames);

   /* the ASC: AAC-LC, the rate's index, the channels */
   CHECK(raac_encoder_asc(e, asc, 1) == 0);
   CHECK(raac_encoder_asc(e, asc, 2) == 2);
   CHECK(asc[0] >> 3 == 2 && ((asc[0] & 7) << 1 | asc[1] >> 7) == (int)rate_index(rate)
         && ((asc[1] >> 3) & 15) == (int)ch && (asc[1] & 7) == 0);
   d  = raac_open(asc, 2);
   CHECK(d && raac_channels(d) == ch && raac_frame_len(d) == 1024
         && raac_sample_rate(d) == rate);
   if (!d)
      return;

   if (dir)
   {
      char path[1024];
      sprintf(path, "%.900s/%s.aac", dir, name);
      file   = fopen(path, "wb");
      sprintf(path, "%.900s/%s.s16", dir, name);
      file_d = fopen(path, "wb");
   }

   /* one more call than frames, without samples, for the last */
   for (f = 0; f <= frames; f++)
   {
      const int16_t *in = f < frames ? pcm + (size_t)f * 1024 * ch : NULL;
      int            r;
      size_t         nf;

      n = raac_encoder_encode_s16(e, in, out, sizeof(out), true);
      CHECK(n > 7);
      if (n <= 7)
         break;
      if (n - 7 > peak)
         peak = n - 7;

      /* the same samples as floats: the same bytes */
      if (in)
         for (i = 0; i < 1024 * ch; i++)
            pcm_f[i] = in[i] / 32768.0f;
      nf = raac_encoder_encode_f32(ef, in ? pcm_f : NULL, out_f, sizeof(out_f), false);
      if (nf != n - 7 || memcmp(out_f, out + 7, nf))
         same_f32 = 0;

      /* the ADTS header: sync, LC, the rate, the channels, the length */
      if (out[0] != 0xFF || (out[1] & 0xF6) != 0xF0 || (out[1] & 1) != 1
            || out[2] >> 6 != 1 || ((out[2] >> 2) & 15) != rate_index(rate)
            || (((out[2] & 1) << 2) | out[3] >> 6) != (int)ch
            || (((size_t)(out[3] & 3) << 11) | ((size_t)out[4] << 3) | (out[5] >> 5)) != n)
         adts_ok = 0;
      /* raac reads the header the same way, and its ASC is the encoder's */
      {
         raac_adts_t h;
         if (raac_adts_parse(out, n, &h) || h.frame_size != n || h.header_size != 7
               || h.raw_blocks != 1 || memcmp(h.asc, asc, 2))
            adts_ok = 0;
      }

      total += n - 7;
      if (file)
         fwrite(out, 1, n, file);
      r = raac_decode_s16(d, out + 7, n - 7, dec + decoded * ch);
      if (r != 1024)
         errors++;
      else
         decoded += 1024;
   }
   if (file)
      fclose(file);
   if (file_d)
   {
      fwrite(dec, sizeof(int16_t) * ch, decoded, file_d);
      fclose(file_d);
   }

   /* the output is the input a frame late (the level holds only when
    * the two are lined up) */
   CHECK(decoded == (size_t)(frames + 1) * 1024);
   for (i = 0; i < frames * 1024 * ch; i++)
   {
      double a = pcm[i], b = dec[1024 * ch + i];
      se += (a - b) * (a - b);
      ss += a * a;
      sx += a * b;
   }
   snr  = 10 * log10(ss / (se > 1 ? se : 1));
   gain = sx / ss;
   kbps = total * 8.0 * rate / 1024 / (frames + 1) / 1000;
   printf("%s: %u Hz, %u ch, %u bit/s, %u frames, %.1f kbit/s, largest %u bytes, "
         "SNR %.2f dB, gain %.3f\n", name, rate, ch, (unsigned)bitrate, frames,
         kbps, (unsigned)peak, snr, gain);
   CHECK(errors == 0);
   CHECK(adts_ok);
   CHECK(same_f32);
   CHECK(snr >= min_snr);
   /* coarse steps round towards zero: a little quieter, by what the
    * noise comes to */
   CHECK(fabs(gain - 1) < 0.02 + pow(10, -snr / 10));
   CHECK(kbps * 1000 <= bitrate * 1.03);
   if (rate_met)
      CHECK(kbps * 1000 >= bitrate * 0.97);

   /* a buffer smaller than any frame */
   n = raac_encoder_encode_s16(e, pcm, out, 3, false);
   CHECK(n == 0);

   raac_encoder_free(e);
   raac_encoder_free(ef);
   raac_close(d);
}

static int compare(const char *pa, const char *pb)
{
   FILE   *fa = fopen(pa, "rb"), *fb = fopen(pb, "rb");
   long    na, nb, i;
   int16_t a, b;
   int     worst = 0;
   if (!fa || !fb)
      return 1;
   fseek(fa, 0, SEEK_END);
   fseek(fb, 0, SEEK_END);
   na = ftell(fa);
   nb = ftell(fb);
   rewind(fa);
   rewind(fb);
   for (i = 0; i < na / 2 && i < nb / 2; i++)
   {
      int dd;
      if (fread(&a, 2, 1, fa) != 1 || fread(&b, 2, 1, fb) != 1)
         return 1;
      dd = a > b ? a - b : b - a;
      if (dd > worst)
         worst = dd;
   }
   fclose(fa);
   fclose(fb);
   printf("%s: %ld samples, FFmpeg's %ld, most apart %d\n", pa, na / 2, nb / 2, worst);
   return na != nb || worst > 2;
}

int main(int argc, char **argv)
{
   const char *dir = argc > 1 ? argv[1] : NULL;
   raac_encoder_config_t c;

   if (argc == 4 && !strcmp(argv[1], "--compare"))
      return compare(argv[2], argv[3]);

   /* a rate and channel counts not AAC's, no bit rate */
   c.sample_rate = 44000;
   c.channels    = 2;
   c.bitrate     = 128000;
   CHECK(!raac_encoder_new(&c));
   c.sample_rate = 44100;
   c.channels    = 3;
   CHECK(!raac_encoder_new(&c));
   c.channels    = 0;
   CHECK(!raac_encoder_new(&c));
   c.channels    = 1;
   c.bitrate     = 0;
   CHECK(!raac_encoder_new(&c));

   run(dir, "tones",       SIG_TONES,  44100, 2, 128000, 120, 40.0, false);
   run(dir, "tones_mono",  SIG_TONES,  48000, 1,  64000, 120, 40.0, false);
   run(dir, "quiet",       SIG_QUIET,  44100, 2, 128000,  80, 45.0, false);
   run(dir, "noise",       SIG_NOISE,  48000, 2, 128000, 120,  7.0, true);
   run(dir, "noise_low",   SIG_NOISE,  44100, 2,  64000, 120,  4.0, true);
   run(dir, "noise_mono",  SIG_NOISE,  32000, 1,  48000, 120,  7.0, true);
   run(dir, "attack",      SIG_ATTACK, 44100, 2, 128000,  60, 25.0, false);
   run(dir, "narrow",      SIG_TONES,   8000, 1,  16000,  40, 30.0, false);
   run(dir, "wide",        SIG_TONES,  96000, 2, 256000,  80, 40.0, false);

   if (failures)
      printf("\n%u check(s) failed\n", failures);
   else
      printf("\nall checks passed\n");
   return failures ? 1 : 0;
}
