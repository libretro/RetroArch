/* The AC-3 encoder against another decoder.
 *
 * Signals are encoded here and decoded by ffmpeg; the decode must
 * match the source (one transform block later) to an SNR that the
 * bit rate warrants, at unity gain. The decoder here decodes the
 * same stream, and must agree with ffmpeg's decode closely (no
 * dither is used, so the two have no random part). ffprobe walks
 * the stream and must count every frame. Without ffmpeg the checks
 * are skipped. */

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
   return system("ffmpeg -version >/dev/null 2>&1 && ffprobe -version >/dev/null 2>&1") == 0;
}

/* a against b after a best-fit gain; both strided */
static double snr_db(const float *a, const float *b, size_t n, unsigned stride, unsigned ch, double *gain_out)
{
   double ab = 0, bb = 0, aa = 0, err = 0, g; size_t i;
   for (i = 0; i < n; i++) { double x = a[i * stride + ch], y = b[i * stride + ch]; ab += x * y; bb += y * y; aa += x * x; }
   g = bb > 0 ? ab / bb : 1.0;
   for (i = 0; i < n; i++) { double x = a[i * stride + ch], y = b[i * stride + ch] * g; err += (x - y) * (x - y); }
   *gain_out = g;
   if (err <= 0) return 200.0;
   if (aa <= 0) return 0.0;
   return 10.0 * log10(aa / err);
}

static uint32_t rng = 0x2545F491u;
static float frand(void) { rng = rng * 1664525u + 1013904223u; return ((float)(rng >> 8) / 16777216.0f) * 2.0f - 1.0f; }

/* signal kinds */
enum { SIG_TONES, SIG_PINK };

/* The LFE carries seven coefficients, up to about 600 Hz, so its
 * slot gets a low tone whatever the kind. */
static void make_signal(float *buf, size_t frames, unsigned channels, unsigned rate, int kind, int lfe_slot)
{
   size_t i; unsigned ch;
   float b0 = 0, b1 = 0, b2 = 0;
   for (i = 0; i < frames; i++)
   {
      for (ch = 0; ch < channels; ch++)
      {
         float v;
         if ((int)ch == lfe_slot)
            v = 0.5f * (float)sin(2.0 * 3.14159265358979 * 50.0 * (double)i / rate);
         else if (kind == SIG_TONES)
         {
            double f = 220.0 * (ch + 1) + 110.0 * ch;
            v = 0.4f * (float)sin(2.0 * 3.14159265358979 * f * (double)i / rate)
              + 0.1f * (float)sin(2.0 * 3.14159265358979 * (f * 3.01) * (double)i / rate);
         }
         else
         {
            float w = frand();
            /* a three-pole pink approximation */
            b0 = 0.99765f * b0 + w * 0.0990460f;
            b1 = 0.96300f * b1 + w * 0.2965164f;
            b2 = 0.57000f * b2 + w * 1.0526913f;
            v = 0.12f * (b0 + b1 + b2 + w * 0.1848f);
         }
         buf[i * channels + ch] = v;
      }
   }
}

static void stream_case(const char *name, unsigned rate, uint32_t layout, unsigned channels, unsigned kbps, int kind, double floor_db, double self_db)
{
   int lfe_slot = (layout & 0x008u) ? (int)((layout & 0x004u) ? 3 : 2) : -1;
   char cmd[512], path[128], refpath[128];
   size_t frames = 1536 * 30, i, at = 0, out_len = 0, cap, nframes = 0, ref_len = 0, decoded = 0;
   float *src, *ref, *ours;
   uint8_t *stream;
   rac3_encoder_t *enc = rac3_encoder_new(rate, layout, kbps);
   rac3_decoder_t *dec = rac3_decoder_new();
   unsigned ch;

   if (!enc) { CHECK(0, "%s: encoder refused rate %u layout 0x%x %u kbps", name, rate, layout, kbps); return; }
   src = (float*)calloc(frames * channels, sizeof(float));
   make_signal(src, frames, channels, rate, kind, lfe_slot);
   cap = rac3_encoder_frame_bytes(enc) * (frames / 1536);
   stream = (uint8_t*)malloc(cap);
   for (i = 0; i + 1536 <= frames; i += 1536)
   {
      size_t n = rac3_encode_frame(enc, src + i * channels, stream + out_len, cap - out_len);
      CHECK(n > 0, "%s: frame %u not encoded", name, (unsigned)(i / 1536));
      if (!n) break;
      out_len += n;
      nframes++;
   }
   snprintf(path, sizeof(path), "/tmp/rac3e_%s.ac3", name);
   snprintf(refpath, sizeof(refpath), "/tmp/rac3e_%s.f32", name);
   {
      FILE *f = fopen(path, "wb"); fwrite(stream, 1, out_len, f); fclose(f);
   }
   /* ffprobe walks it */
   {
      char line[64]; FILE *p; unsigned counted = 0;
      snprintf(cmd, sizeof(cmd), "ffprobe -v error -show_entries packet=size -of csv=p=0 %s 2>/dev/null | wc -l", path);
      p = popen(cmd, "r");
      if (p && fgets(line, sizeof(line), p)) counted = (unsigned)atoi(line);
      if (p) pclose(p);
      CHECK(counted == nframes, "%s: ffprobe counts %u frames, %u were written", name, counted, (unsigned)nframes);
   }
   /* ffmpeg decodes it */
   snprintf(cmd, sizeof(cmd), "ffmpeg -hide_banner -loglevel error -y -i %s -f f32le -c:a pcm_f32le %s", path, refpath);
   if (system(cmd) != 0)
   {
      CHECK(0, "%s: ffmpeg would not decode the stream", name);
      goto done;
   }
   ref = (float*)read_all(refpath, &ref_len);
   ours = (float*)calloc(nframes * 1536 * channels, sizeof(float));
   {
      size_t n_ref = ref_len / (sizeof(float) * channels);
      size_t n = n_ref < nframes * 1536 ? n_ref : nframes * 1536;
      double worst = 1e9, gmin = 1e9, gmax = -1e9, worst_self = 1e9;
      CHECK(n_ref == nframes * 1536, "%s: ffmpeg decoded %u samples, expected %u", name, (unsigned)n_ref, (unsigned)(nframes * 1536));
      /* our decoder on the stream */
      while (at < out_len)
      {
         rac3_frame_info_t info;
         size_t k = rac3_decode_frame(dec, stream + at, out_len - at, ours + decoded * channels, &info);
         if (!k) { CHECK(0, "%s: our decoder refused frame %u", name, (unsigned)(decoded / 1536)); break; }
         decoded += k; at += info.frame_bytes;
      }
      /* ffmpeg's decode against the source, 256 samples later */
      for (ch = 0; ch < channels; ch++)
      {
         double g, s;
         if (n <= 256) break;
         s = snr_db(ref + 256 * channels, src, n - 256, channels, ch, &g);
         if (s < worst) worst = s;
         if (g < gmin) gmin = g;
         if (g > gmax) gmax = g;
         s = snr_db(ours, ref, n, channels, ch, &g);
         if (s < worst_self) worst_self = s;
      }
      printf("   %-22s %u frames; ffmpeg vs source: worst SNR %.1f dB, gain %.3f..%.3f; ours vs ffmpeg %.1f dB\n",
            name, (unsigned)nframes, worst, gmin, gmax, worst_self);
      CHECK(worst >= floor_db, "%s: SNR %.1f dB below the %.0f dB floor", name, worst, floor_db);
      CHECK(gmin > 0.9 && gmax < 1.1, "%s: gain %.3f..%.3f is not near one", name, gmin, gmax);
      CHECK(worst_self >= self_db, "%s: our decoder differs from ffmpeg's by more than %.0f dB (%.1f)", name, self_db, worst_self);
   }
   free(ref); free(ours);
done:
   rac3_encoder_free(enc);
   rac3_decoder_free(dec);
   free(src); free(stream);
   remove(path); remove(refpath);
}

int main(void)
{
   printf("rac3 encode:\n");
   if (!have_ffmpeg())
   {
      printf("   (ffmpeg not installed: skipped)\n");
      skipped++;
   }
   else
   {
      stream_case("tones-48k-stereo-192", 48000, 0x003, 2, 192, SIG_TONES, 40.0, 60.0);
      stream_case("tones-48k-mono-96",    48000, 0x004, 1,  96, SIG_TONES, 40.0, 60.0);
      stream_case("tones-44k-stereo-128", 44100, 0x003, 2, 128, SIG_TONES, 35.0, 60.0);
      stream_case("tones-32k-stereo-64",  32000, 0x003, 2,  64, SIG_TONES, 25.0, 60.0);
      stream_case("tones-48k-5.1-448",    48000, 0x60F, 6, 448, SIG_TONES, 40.0, 60.0);
      /* pink noise is the hard case for a perceptual codec's waveform
       * SNR: ffmpeg's own encoder gets 9.9 dB on this signal at 192k
       * stereo and 15 dB at 640k; a basic encoder should be close */
      stream_case("pink-48k-stereo-192",  48000, 0x003, 2, 192, SIG_PINK,   8.0, 60.0);
      stream_case("pink-48k-5.1-640",     48000, 0x60F, 6, 640, SIG_PINK,   8.0, 60.0);
      stream_case("pink-48k-2.1-256",     48000, 0x00B, 3, 256, SIG_PINK,   8.0, 60.0);
   }
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("rac3 encode: ffmpeg decodes our streams back to the source%s\n", skipped ? " (some skipped)" : "");
   return 0;
}
