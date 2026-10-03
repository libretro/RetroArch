/* The AC-3 decoder against another decoder's output.
 *
 * ffmpeg encodes tones and noise into AC-3 across rates, bit rates
 * and configurations, decodes each stream itself to float, and the
 * decoder here decodes the same stream. The two outputs must agree
 * to the noise of quantisation: after a best-fit gain (the two may
 * differ in how they treat dialogue normalisation), the SNR of ours
 * against theirs must exceed a floor on every channel, and the gain
 * must be near one. Without ffmpeg the checks are skipped. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include <formats/rac3.h>

static unsigned failures = 0, skipped = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static uint8_t *read_all(const char *path, size_t *len)
{
   FILE *f = fopen(path, "rb"); uint8_t *b; long n;
   if (!f) return NULL;
   fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
   b = (uint8_t*)malloc(n > 0 ? n : 1);
   *len = fread(b, 1, n, f); fclose(f);
   return b;
}

static int have_ffmpeg(void)
{
   return system("ffmpeg -version >/dev/null 2>&1") == 0;
}

/* SNR of a against b on one channel after the gain that best fits a
 * to b; returns the SNR in dB and the gain. */
static double snr_db(const float *a, const float *b, size_t n, unsigned stride, unsigned ch, double *gain_out)
{
   double ab = 0, bb = 0, aa = 0, err = 0, g; size_t i;
   for (i = 0; i < n; i++)
   {
      double x = a[i * stride + ch], y = b[i * stride + ch];
      ab += x * y; bb += y * y; aa += x * x;
   }
   g = bb > 0 ? ab / bb : 1.0;
   for (i = 0; i < n; i++)
   {
      double x = a[i * stride + ch], y = b[i * stride + ch] * g;
      err += (x - y) * (x - y);
   }
   *gain_out = g;
   if (err <= 0) return 200.0;
   if (aa <= 0) return 0.0;
   return 10.0 * log10(aa / err);
}

static const char *codec = "ac3";   /* ffmpeg's encoder for the case: ac3 or eac3 */

static void stream_case(const char *source, unsigned rate, unsigned channels, unsigned kbps, const char *name, double floor_db)
{
   char cmd[640], path[128], refpath[128];
   size_t len, at = 0, ref_len, frames = 0, decoded = 0, ref_frames;
   uint8_t *data; float *ref, *ours;
   rac3_decoder_t *dec;
   unsigned ch;

   snprintf(path, sizeof(path), "/tmp/rac3d_%s.ac3", name);
   snprintf(refpath, sizeof(refpath), "/tmp/rac3d_%s.f32", name);
   snprintf(cmd, sizeof(cmd),
         "ffmpeg -hide_banner -loglevel error -y -f lavfi -i \"%s\" -ac %u -c:a %s -b:a %uk -f %s %s && "
         "ffmpeg -hide_banner -loglevel error -y -i %s -f f32le -c:a pcm_f32le %s",
         source, channels, codec, kbps, codec, path, path, refpath);
   if (system(cmd) != 0)
   {
      printf("   %-26s (ffmpeg would not make it; skipped)\n", name);
      skipped++;
      return;
   }
   data = read_all(path, &len);
   ref  = (float*)read_all(refpath, &ref_len);
   if (!data || !ref) { CHECK(0, "%s: files missing", name); return; }
   ref_frames = ref_len / (sizeof(float) * channels);
   ours = (float*)calloc(ref_frames + 1536, sizeof(float) * channels);
   dec  = rac3_decoder_new();

   while (at < len)
   {
      rac3_frame_info_t info;
      size_t n;
      if (rac3_parse_frame_info(data + at, len - at, &info) != RAC3_OK) break;
      if (decoded + 1536 > ref_frames + 1536) break;
      CHECK((info.kind == RAC3_KIND_EAC3) == (codec[0] == 'e'), "%s: the stream is not what ffmpeg was asked for", name);
      n = rac3_decode_frame(dec, data + at, len - at, ours + decoded * channels, &info);
      if (!n)
      {
         CHECK(0, "%s: frame %u refused", name, (unsigned)frames);
         break;
      }
      CHECK(info.channels == channels, "%s: %u channels, expected %u", name, info.channels, channels);
      decoded += n;
      at      += info.frame_bytes;
      frames++;
   }
   {
      size_t n = decoded < ref_frames ? decoded : ref_frames;
      double worst = 1e9, gmin = 1e9, gmax = -1e9;
      for (ch = 0; ch < channels; ch++)
      {
         double g, s, ea = 0, eb = 0; size_t i;
         for (i = 0; i < n; i++) { ea += ours[i * channels + ch] * ours[i * channels + ch]; eb += ref[i * channels + ch] * ref[i * channels + ch]; }
         /* A channel silent in the reference (ffmpeg's upmix leaves
          * the LFE empty) must be silent here too, and is not scored. */
         if (eb / n < 1e-10)
         {
            CHECK(ea / n < 1e-8, "%s: channel %u is silent in the reference, %.2e here", name, ch, ea / n);
            continue;
         }
         s = snr_db(ours, ref, n, channels, ch, &g);
         if (s < worst) worst = s;
         if (g < gmin) gmin = g;
         if (g > gmax) gmax = g;
      }
      printf("   %-26s %u frames, %u samples; worst channel SNR %.1f dB, gain %.3f..%.3f\n",
            name, (unsigned)frames, (unsigned)n, worst, gmin, gmax);
      CHECK(n > 4096, "%s: only %u samples compared", name, (unsigned)n);
      CHECK(worst >= floor_db, "%s: SNR %.1f dB below the %.0f dB floor", name, worst, floor_db);
      CHECK(gmin > 0.5 && gmax < 2.0, "%s: gain %.3f..%.3f is not near one", name, gmin, gmax);
   }
   rac3_decoder_free(dec);
   free(data); free(ref); free(ours);
   remove(path); remove(refpath);
}

int main(void)
{
   printf("rac3 decode:\n");
   if (!have_ffmpeg())
   {
      printf("   (ffmpeg not installed: skipped)\n");
      skipped++;
   }
   else
   {
      const char *tone  = "sine=frequency=440:sample_rate=48000:duration=1";
      const char *tone2 = "sine=frequency=1000:sample_rate=44100:duration=1";
      const char *noise = "anoisesrc=color=pink:sample_rate=48000:duration=1:amplitude=0.3";
      stream_case(tone,  48000, 2, 192, "tone-48k-stereo-192", 30.0);
      stream_case(tone,  48000, 1,  96, "tone-48k-mono-96",    30.0);
      stream_case(tone2, 44100, 2, 128, "tone-44k-stereo-128", 30.0);
      stream_case(tone,  48000, 6, 448, "tone-48k-5.1-448",    30.0);
      stream_case(noise, 48000, 2, 192, "pink-48k-stereo-192", 12.0);
      stream_case(noise, 48000, 6, 640, "pink-48k-5.1-640",    12.0);
      stream_case(noise, 48000, 2,  64, "pink-48k-stereo-64",   6.0);
      /* E-AC-3 (Annex E) from ffmpeg's eac3 encoder: the same
       * decoder, the frame header's strategies and the block syntax
       * differences; no spectral extension, enhanced coupling or AHT
       * in these streams */
      printf("   E-AC-3:\n");
      codec = "eac3";
      stream_case(tone,  48000, 2, 192, "eac3-tone-48k-stereo-192", 30.0);
      stream_case(tone,  48000, 1,  96, "eac3-tone-48k-mono-96",    30.0);
      stream_case(tone,  48000, 6, 448, "eac3-tone-48k-5.1-448",    30.0);
      stream_case(noise, 48000, 2, 192, "eac3-pink-48k-stereo-192", 12.0);
      stream_case(noise, 48000, 6, 640, "eac3-pink-48k-5.1-640",    12.0);
      stream_case(noise, 48000, 2,  64, "eac3-pink-48k-stereo-64",   6.0);
   }
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("rac3 decode: agrees with ffmpeg's decoder to quantisation noise%s\n", skipped ? " (some skipped)" : "");
   return 0;
}
