/* Independent frontend PCM onset conformance. Expected sample positions
 * come from ffmpeg's timestamp-preserving decode, not another mixer. */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <audio/audio_mixer.h>
#include <formats/audio.h>

/* Default resampler configuration, matching the existing PCM oracle. */
int config_userdata_get_float(void *u, const char *k, float *v, float d)
{ (void)u; (void)k; *v=d; return 0; }
int config_userdata_get_int(void *u, const char *k, int *v, int d)
{ (void)u; (void)k; *v=d; return 0; }
int config_userdata_get_float_array(void *u, const char *k, float **v,
      unsigned *n, const float *d, unsigned dn)
{ (void)u;(void)k;(void)v;(void)n;(void)d;(void)dn;return 0; }
int config_userdata_get_int_array(void *u, const char *k, int **v,
      unsigned *n, const int *d, unsigned dn)
{ (void)u;(void)k;(void)v;(void)n;(void)d;(void)dn;return 0; }
int config_userdata_get_string(void *u, const char *k, char **v, const char *d)
{ (void)u;(void)k;(void)v;(void)d;return 0; }
void config_userdata_free(void *u) { (void)u; }

static int edge_conformance(const char *path, long declared, int empty,
      long expected, int s16)
{
   FILE *f = fopen(path, "rb");
   unsigned char *data = NULL;
   void *decoder = NULL;
   enum audio_type_enum type = AUDIO_TYPE_NONE;
   union { float f32[512]; int16_t s16[512]; } block;
   unsigned rate = 0, channels = 0, pass, ticks;
   uint64_t total = 0, prior_frames = 0;
   uint32_t prior_hash = 0;
   long length;
   int failed = 1;
   if (!f) return 1;
   if (fseek(f, 0, SEEK_END) || (length = ftell(f)) <= 0 || fseek(f, 0, SEEK_SET)) goto done;
   data = (unsigned char*)malloc((size_t)length);
   if (!data || fread(data, 1, (size_t)length, f) != (size_t)length) goto done;
   type = audio_transfer_webm_audio_type(data, (size_t)length);
   decoder = audio_transfer_new(type);
   if (!decoder) goto done;
   audio_transfer_set_buffer_ptr(decoder, type, data, (size_t)length);
   if (!audio_transfer_start(decoder, type)
         || !audio_transfer_info(decoder, type, &channels, &rate, &total)
         || !channels || channels > 8 || rate != 48000 || total != (uint64_t)declared) goto done;
   for (pass = 0; pass < 2; pass++)
   {
      uint64_t frames = 0;
      uint32_t hash = 2166136261u;
      int end = 0, active = 0;
      if (pass)
      {
         if (!audio_transfer_seek(decoder, type, 0)) goto done;
         {
            int result = 0;
            if (!audio_transfer_seek_begin_format(decoder, type, 0, s16)) goto done;
            for (ticks = 0; ticks < 200000 && !result; ticks++)
               result = audio_transfer_seek_step(decoder, type);
            if (result != 1) goto done;
         }
      }
      for (ticks = 0; ticks < 200000; ticks++)
      {
         size_t got = 0, i, n;
         int result = s16 ? audio_transfer_read_s16(decoder, type, block.s16, 64, &got)
            : audio_transfer_read_f32(decoder, type, block.f32, 64, &got);
         if (result < 0 || got > 64) goto done;
         frames += got;
         if (frames > 48000 * 60) goto done;
         n = got * channels * (s16 ? sizeof(int16_t) : sizeof(float));
         for (i = 0; i < n; i++) { hash ^= ((unsigned char*)&block)[i]; hash *= 16777619u; }
         for (i = 0; i < got * channels; i++)
            if (s16 ? block.s16[i] != 0 : block.f32[i] != 0.0f) active = 1;
         if (result == AUDIO_PROCESS_END) { end = 1; break; }
      }
      printf("audio edge codec=%d s16=%d pass=%u frames=%lu expected=%ld active=%d EOF=%d\n",
            (int)type, s16, pass, (unsigned long)frames, expected, active, end);
      if (!end || (empty ? active : !active) || frames != (uint64_t)expected
            || (pass && (frames != prior_frames || hash != prior_hash))) goto done;
      prior_frames = frames; prior_hash = hash;
   }
   failed = 0;
done:
   if (decoder) audio_transfer_free(decoder, type);
   free(data); fclose(f);
   return failed;
}

static int seek_conformance(const char *path, int s16, long period)
{
   FILE *f = fopen(path, "rb");
   enum audio_type_enum type = AUDIO_TYPE_AAC;
   unsigned char *data = NULL;
   void *linear = NULL, *seek = NULL, *reference = NULL;
   union { float f32[512]; int16_t s16[512]; } actual;
   unsigned channels = 0, rate = 0, pass, steps;
   uint64_t total = 0;
   long length;
   size_t decoded = 0, got = 0, bytes = s16 ? sizeof(int16_t) : sizeof(float);
   int failed = 1, result;
   double peak = 0;
   if (!f) return 1;
   if (fseek(f, 0, SEEK_END) || (length = ftell(f)) <= 0
         || fseek(f, 0, SEEK_SET)) goto done;
   data = (unsigned char*)malloc((size_t)length);
   if (!data || fread(data, 1, (size_t)length, f) != (size_t)length) goto done;
   if (strstr(path, ".webm") || strstr(path, ".mkv"))
      type = audio_transfer_webm_audio_type(data, (size_t)length);
   linear = audio_transfer_new(type);
   seek = audio_transfer_new(type);
   if (!linear || !seek) goto done;
   audio_transfer_set_buffer_ptr(linear, type, data, (size_t)length);
   audio_transfer_set_buffer_ptr(seek, type, data, (size_t)length);
   if (!audio_transfer_start(linear, type)
         || !audio_transfer_start(seek, type)
         || !audio_transfer_info(linear, type, &channels, &rate, &total)
         || !channels || channels > 8 || rate != 48000 || total != (uint64_t)period)
      goto done;
   reference = calloc((size_t)total * channels, bytes);
   if (!reference) goto done;
   for (steps = 0; decoded < (size_t)total && steps < 100000; steps++)
   {
      size_t want = (size_t)total - decoded;
      if (want > 64) want = 64;
      result = s16 ? audio_transfer_read_s16(linear, type,
            (int16_t*)reference + decoded * channels, want, &got)
         : audio_transfer_read_f32(linear, type,
               (float*)reference + decoded * channels, want, &got);
      if (result < 0 || (!got && result == AUDIO_PROCESS_END)) goto done;
      decoded += got;
   }
   if (decoded != total) goto done;
   for (pass = 0; pass < 9; pass++)
   {
      uint64_t target = pass == 0 ? 0 : pass == 1 ? 113 : pass == 2 ? 24000
         : pass == 3 ? 48000 : pass == 4 ? 72013 : pass == 5 ? 96000
         : pass == 6 ? 144000 : pass == 7 ? total - 64 : total;
      size_t i;
      if (target > total) continue;
      audio_transfer_set_avail(seek, type, (size_t)length);
      if (!audio_transfer_seek_begin_format(seek, type, target, s16)) goto done;
      result = 0;
      for (steps = 0; steps < 4096 && !result; steps++)
      {
         audio_transfer_set_avail(seek, type, (size_t)length);
         result = audio_transfer_seek_step(seek, type);
      }
      if (result != 1 || audio_transfer_seek_begin_format(seek,
               type, (uint64_t)-1, s16)) goto done;
      result = s16 ? audio_transfer_read_s16(seek, type, actual.s16, 64, &got)
         : audio_transfer_read_f32(seek, type, actual.f32, 64, &got);
      if (result < 0 || got != (target == total ? 0 : 64)) goto done;
      for (i = 0; i < got * channels; i++)
      {
         double error = s16 ? (double)actual.s16[i] - ((int16_t*)reference)[target*channels+i]
            : (double)actual.f32[i] - ((float*)reference)[target*channels+i];
         if (error < 0) error = -error;
         if (error > peak) peak = error;
      }
   }
   failed = peak > (s16 ? 33.0 : 0.001);
done:
   if (linear) audio_transfer_free(linear, type);
   if (seek) audio_transfer_free(seek, type);
   free(data); free(reference); fclose(f);
   printf("movie seek codec=%d %s: peak=%g %s\n", (int)type, s16 ? "s16" : "float", peak,
         failed ? "FAIL" : "PASS");
   return failed;
}

static int run(const char *path, long expected, int s16,
      const unsigned char *activity, long period)
{
   FILE *f = fopen(path, "rb");
   long size, first = -1, frame;
   unsigned char *data;
   audio_mixer_sound_t *sound;
   audio_mixer_voice_t *voice;
   float out[128];
   long bucket = -1, bad = 0, checked = 0;
   unsigned ticks, stalls = 0;
   int clock_failed = 0;
   long loud[2] = {0, 0}, quiet[2] = {0, 0};
   unsigned lap = 0;
   double peak = 0;
   if (!f) return 1;
   if (fseek(f, 0, SEEK_END) || (size = ftell(f)) <= 0
         || fseek(f, 0, SEEK_SET)) { fclose(f); return 1; }
   data = (unsigned char*)malloc((size_t)size);
   if (!data) { fclose(f); return 1; }
   if (fread(data, 1, (size_t)size, f) != (size_t)size)
   { free(data); fclose(f); return 1; }
   fclose(f);
   audio_mixer_init(48000);
   sound = (strstr(path, ".webm") || strstr(path, ".mkv")) ? audio_mixer_load_weba(data, (size_t)size)
      : audio_mixer_load_m4a(data, (size_t)size);
   if (!sound) { audio_mixer_done(); free(data); return 1; }
   voice = s16 ? audio_mixer_play_s16(sound, activity != NULL, AUDIO_MIXER_GAIN_UNITY,
         "sinc", RESAMPLER_QUALITY_DONTCARE, NULL)
      : audio_mixer_play(sound, activity != NULL, 1.0f, "sinc", RESAMPLER_QUALITY_DONTCARE, NULL);
   if (!voice) { audio_mixer_destroy(sound); audio_mixer_done(); return 1; }
   for (frame = 0, ticks = 0; ticks < 300000
         && frame < (activity ? period * 2 : 144000)
         && (activity || first < 0); ticks++)
   {
      unsigned i;
      size_t mixed = 0;
      long before = frame;
      memset(out, 0, sizeof(out));
      if (s16) audio_mixer_mix_s16((int16_t*)out, 64, AUDIO_MIXER_GAIN_UNITY, false);
      else audio_mixer_mix(out, 64, 1.0f, false);
      if (!audio_mixer_voice_clock(voice, &mixed, NULL, NULL)
            || mixed < (size_t)before || mixed - (size_t)before > 64)
      { clock_failed = 1; break; }
      frame = (long)mixed;
      if (frame == before) { stalls++; continue; }
      for (i = 0; i < (unsigned)(frame - before) * 2; i++)
      {
         double value = s16 ? ((int16_t*)out)[i] : out[i] * 32768.0;
         long at = before + i / 2;
         if (first < 0 && (value > 64 || value < -64)) first = at;
         if (activity && at < period * 2)
         {
            long b = (at % period) / 960;
            if (bucket != b)
            {
               /* Ignore two buckets on either side of a reference
                * transition. This tolerates packet/window boundaries
                * while exposing sustained missing gaps and short loops. */
               if (bucket >= 2 && bucket + 2 < (period + 959) / 960)
               {
                  unsigned char a = activity[bucket];
                  if (a != 1 && activity[bucket-2] == a
                        && activity[bucket-1] == a && activity[bucket+1] == a
                        && activity[bucket+2] == a)
                  {
                     checked++;
                     if (!a && peak > 128) { loud[lap]++; bad++; }
                     if (a == 2 && peak <= 64) { quiet[lap]++; bad++; }
                  }
               }
               bucket = b;
               lap = (unsigned)(at / period);
               peak = 0;
            }
            if (value < 0) value = -value;
            if (value > peak) peak = value;
         }
      }
   }
   audio_mixer_stop(voice);audio_mixer_destroy(sound);audio_mixer_done();
   printf("frontend audio timeline %s: first=%ld expected=%ld delta=%ld\n",
         s16 ? "s16" : "float", first, expected, first - expected);
   if (activity) printf("movie period=%ld samples: %ld/%ld stable activity buckets wrong over two laps\n",
         period, bad, checked);
   if (activity) printf("lap1: %ld unexpectedly loud, %ld unexpectedly silent; lap2: %ld loud, %ld silent\n",
         loud[0], quiet[0], loud[1], quiet[1]);
   printf("media clock: %u zero-output retries, clock failure=%d\n", stalls, clock_failed);
   return clock_failed || first < 0 || first < expected - 128 || first > expected + 128
      || bad || (activity && (!checked || frame < period * 2));
}

/* A two-hour seek must not require one worker wakeup per 256 AUs.
 * Compare the resulting PCM with the independent synchronous seek. */
static int long_seek_conformance(const char *path)
{
   FILE *file = fopen(path, "rb");
   unsigned char *data = NULL;
   void *seek = NULL, *reference = NULL;
   float actual[128], expected[128];
   long length;
   unsigned rate, channels, steps, pass;
   size_t got, wanted, i;
   int result, failed = 1;
   if (!file) return 1;
   if (fseek(file, 0, SEEK_END) || (length = ftell(file)) <= 0
         || fseek(file, 0, SEEK_SET)) goto done;
   data = (unsigned char*)malloc((size_t)length);
   if (!data || fread(data, 1, (size_t)length, file) != (size_t)length) goto done;
   seek = audio_transfer_new(AUDIO_TYPE_AAC);
   reference = audio_transfer_new(AUDIO_TYPE_AAC);
   if (!seek || !reference) goto done;
   audio_transfer_set_buffer_ptr(seek, AUDIO_TYPE_AAC, data, (size_t)length);
   audio_transfer_set_buffer_ptr(reference, AUDIO_TYPE_AAC, data, (size_t)length);
   if (!audio_transfer_start(seek, AUDIO_TYPE_AAC)
         || !audio_transfer_start(reference, AUDIO_TYPE_AAC)
         || !audio_transfer_info(seek, AUDIO_TYPE_AAC, &channels, &rate, NULL)
         || !channels || channels > 2 || !rate) goto done;
   for (pass = 0; pass < 3; pass++)
   {
      uint64_t target = pass == 1 ? 113 : (uint64_t)7020 * rate + 13;
      if (!audio_transfer_seek_begin(seek, AUDIO_TYPE_AAC, target)) goto done;
      result = 0;
      for (steps = 0; steps < 128 && !result; steps++)
         result = audio_transfer_seek_step(seek, AUDIO_TYPE_AAC);
      printf("long AAC seek: pass=%u steps=%u result=%d\n", pass, steps, result);
      if (result != 1 || !audio_transfer_seek(reference, AUDIO_TYPE_AAC, target)) goto done;
      if (audio_transfer_read_f32(seek, AUDIO_TYPE_AAC, actual, 64, &got) < 0
            || audio_transfer_read_f32(reference, AUDIO_TYPE_AAC, expected, 64, &wanted) < 0
            || got != 64 || wanted != got) goto done;
      for (i = 0; i < got * channels; i++)
         if (fabs(actual[i] - expected[i]) > 0.001) goto done;
   }
   failed = 0;
done:
   if (seek) audio_transfer_free(seek, AUDIO_TYPE_AAC);
   if (reference) audio_transfer_free(reference, AUDIO_TYPE_AAC);
   free(data);
   fclose(file);
   return failed;
}

int main(int argc, char **argv)
{
   long expected;
   int failed;
   unsigned char *activity = NULL;
   long period = 0;
   setvbuf(stdout, NULL, _IONBF, 0);
   if (argc == 3 && !strcmp(argv[1], "--long-seek"))
      return long_seek_conformance(argv[2]);
   if (argc == 6 && !strcmp(argv[1], "--edge"))
   {
      long declared = strtol(argv[3], NULL, 10);
      int empty = atoi(argv[4]);
      long frames = strtol(argv[5], NULL, 10);
      if (declared < 0 || frames < 0) return 2;
      return edge_conformance(argv[2], declared, empty, frames, 0)
         | edge_conformance(argv[2], declared, empty, frames, 1);
   }
   if (argc != 3 && argc != 5) return 2;
   expected = strtol(argv[2], NULL, 10);
   if (argc == 5)
   {
      FILE *f;
      size_t n;
      period = strtol(argv[4], NULL, 10);
      if (period <= 0 || period > 48000 * 60) return 2;
      n = (size_t)((period + 959) / 960);
      activity = (unsigned char*)malloc(n);
      f = fopen(argv[3], "rb");
      if (!activity || !f) { free(activity); if (f) fclose(f); return 2; }
      if (fread(activity, 1, n, f) != n || fgetc(f) != EOF)
      { free(activity); fclose(f); return 2; }
      fclose(f);
   }
   failed = run(argv[1], expected, 0, activity, period);
   failed |= run(argv[1], expected, 1, activity, period);
   if (activity)
   {
      failed |= seek_conformance(argv[1], 0, period);
      failed |= seek_conformance(argv[1], 1, period);
   }
   free(activity);
   return failed;
}
