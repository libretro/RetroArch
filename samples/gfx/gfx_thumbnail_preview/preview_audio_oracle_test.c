/* preview_audio_oracle_test: the preview's audio chain against ffmpeg.
 *
 * The whole path a preview's sound takes - audio_mixer -> audio_transfer
 * -> rmp4 -> raac, and the mixer's resampler when the device rate is
 * not the track's - decoded from the first sample against ffmpeg's
 * decode of the same track. At the track's rate the two must agree to
 * the rounding of a float decoder; through the resampler, once the
 * filter's delay is aligned for, to a fraction of a percent RMS in
 * every half second from the first. A chain that were wrong in its
 * first seconds - a trim, a priming frame, a resampler warming up -
 * shows here as a first window worse than the rest.
 *
 * Usage: preview_audio_oracle_test file.mp4 [file.mp4 ...]
 * Needs ffmpeg in PATH. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <audio/audio_mixer.h>

#define MIX_FRAMES 1024
#define WINDOWS    12       /* half seconds compared per file and rate */

/* the resampler's config hooks the mixer needs, as the harness stubs them */
int config_userdata_get_float(void *u, const char *k, float *v, float d)
{ (void)u; (void)k; *v = d; return 0; }
int config_userdata_get_int(void *u, const char *k, int *v, int d)
{ (void)u; (void)k; *v = d; return 0; }
int config_userdata_get_float_array(void *u, const char *k, float **v,
      unsigned *n, const float *d, unsigned dn)
{ (void)u; (void)k; (void)v; (void)n; (void)d; (void)dn; return 0; }
int config_userdata_get_int_array(void *u, const char *k, int **v,
      unsigned *n, const int *d, unsigned dn)
{ (void)u; (void)k; (void)v; (void)n; (void)d; (void)dn; return 0; }
int config_userdata_get_string(void *u, const char *k, char **v, const char *d)
{ (void)u; (void)k; (void)v; (void)d; return 0; }
void config_userdata_free(void *u) { (void)u; }

static void *slurp(const char *p, size_t *n)
{
   FILE *f = fopen(p, "rb");
   void *b;
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   *n = (size_t)ftell(f);
   fseek(f, 0, SEEK_SET);
   b = malloc(*n ? *n : 1);
   if (b && fread(b, 1, *n, f) != *n)
   {
      free(b);
      b = NULL;
   }
   fclose(f);
   return b;
}

/* Decode @path through the mixer at @rate against @refpath (ffmpeg's
 * f32le stereo at that rate); every half-second window's RMS error
 * relative to the reference must stay under @limit. */
static int run_one(const char *path, const char *refpath, unsigned rate,
      double limit)
{
   size_t blen = 0, rlen = 0, nref, got = 0, k, pos;
   void *buf;
   float *ref, *ours;
   audio_mixer_sound_t *snd;
   audio_mixer_voice_t *v;
   float out[MIX_FRAMES * 2];
   long lag, best_lag = 0;
   double best = 1e30, se = 0, sr = 0, worst = 0;
   size_t cnt = 0;
   int win = 0, bad = 0;

   if (!(buf = slurp(path, &blen)) || !(ref = (float*)slurp(refpath, &rlen)))
   {
      printf("[FAIL] %s: cannot read the file or its reference\n", path);
      return 1;
   }
   nref = rlen / sizeof(float);
   audio_mixer_init(rate);
   if (!(snd = audio_mixer_load_m4a(buf, blen)))
   {
      printf("[FAIL] %s: audio_mixer_load_m4a\n", path);
      return 1;
   }
   if (!(v = audio_mixer_play(snd, false, 1.0f, "sinc",
               RESAMPLER_QUALITY_DONTCARE, NULL)))
   {
      printf("[FAIL] %s: audio_mixer_play\n", path);
      return 1;
   }
   /* our whole output first, then the lag that aligns it best with the
    * reference (a resampler's filter delay), then the comparison */
   if (!(ours = (float*)calloc(nref, sizeof(float))))
      return 1;
   while (got + MIX_FRAMES * 2 <= nref)
   {
      memset(out, 0, sizeof(out));
      audio_mixer_mix(out, MIX_FRAMES, 1.0f, false);
      memcpy(ours + got, out, sizeof(out));
      got += MIX_FRAMES * 2;
   }
   for (lag = -256; lag <= 256; lag += 2)
   {
      double e = 0;
      size_t n = 0;
      for (k = 4096; k < 4096 + (size_t)rate * 2; k++)
      {
         long i = (long)k + lag;
         double d;
         if (i < 0 || (size_t)i >= got)
            continue;
         d = ours[i] - ref[k];
         e += d * d;
         n++;
      }
      if (n && e / n < best)
      {
         best     = e / n;
         best_lag = lag;
      }
   }
   for (pos = 0; pos + MIX_FRAMES * 2 <= got && win < WINDOWS; pos += MIX_FRAMES * 2)
   {
      for (k = 0; k < MIX_FRAMES * 2; k++)
      {
         long i = (long)(pos + k) + best_lag;
         double d;
         if (i < 0 || (size_t)i >= got)
            continue;
         d   = ours[i] - ref[pos + k];
         se += d * d;
         sr += (double)ref[pos + k] * ref[pos + k];
         cnt++;
      }
      if (cnt >= (size_t)rate)              /* half a second of stereo */
      {
         double r = sr > 0 ? sqrt(se / sr) : 0;
         if (r > worst)
            worst = r;
         if (r > limit)
         {
            printf("[FAIL] %s at %u Hz: window ending %.2fs differs from "
                   "ffmpeg by %.4f RMS (limit %.4f)\n", path, rate,
                   (double)(pos / 2) / rate, r, limit);
            bad = 1;
         }
         se = sr = 0;
         cnt = 0;
         win++;
      }
   }
   audio_mixer_stop(v);
   audio_mixer_destroy(snd);            /* the mixer owns buf */
   audio_mixer_done();
   free(ours);
   free(ref);
   if (!bad)
      printf("[pass] %s at %u Hz: %d windows within %.4f RMS of ffmpeg "
             "(worst %.4f, lag %ld)\n", path, rate, win, limit, worst, best_lag);
   return bad;
}

int main(int argc, char **argv)
{
   int i, bad = 0;
   char cmd[1024];
   const char *r48 = "/tmp/preview_audio_oracle_48.f32";
   const char *r44 = "/tmp/preview_audio_oracle_44.f32";
   if (argc < 2)
   {
      printf("usage: %s file.mp4 ...\n", argv[0]);
      return 2;
   }
   for (i = 1; i < argc; i++)
   {
      /* the track's own rate, then a device rate that is not */
      snprintf(cmd, sizeof(cmd),
            "ffmpeg -v error -y -i '%s' -map 0:a:0 -f f32le -ac 2 -ar 48000 '%s' && "
            "ffmpeg -v error -y -i '%s' -map 0:a:0 -f f32le -ac 2 -ar 44100 '%s'",
            argv[i], r48, argv[i], r44);
      if (system(cmd) != 0)
      {
         printf("[FAIL] %s: ffmpeg could not make the reference\n", argv[i]);
         bad = 1;
         continue;
      }
      bad |= run_one(argv[i], r48, 48000, 0.001);
      bad |= run_one(argv[i], r44, 44100, 0.01);
   }
   remove(r48);
   remove(r44);
   printf(bad ? "FAIL\n" : "PASS\n");
   return bad;
}
