/* The AC-3 arm of audio_transfer against ffmpeg.
 *
 * ffmpeg encodes a 5.1 tone set and a stereo one to AC-3; the arm's
 * f32 read must match ffmpeg's own decode of the same file to within
 * the dither's few dB, info must report the stream, s16 must be the
 * f32 saturated, a seek must land where a playthrough would, and a
 * frontier (set_avail) must starve rather than end. Without ffmpeg
 * the check is skipped. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <formats/audio.h>

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static uint8_t *read_all(const char *path, size_t *len)
{
   FILE *f = fopen(path, "rb"); uint8_t *b; long n;
   if (!f) { *len = 0; return NULL; }
   fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
   b = (uint8_t*)malloc(n > 0 ? n : 1);
   *len = fread(b, 1, n, f); fclose(f);
   return b;
}

static double snr_db(const float *a, const float *b, size_t n)
{
   double aa = 0, err = 0; size_t i;
   for (i = 0; i < n; i++) { aa += (double)a[i] * a[i]; err += ((double)a[i] - b[i]) * ((double)a[i] - b[i]); }
   if (err <= 0) return 200.0;
   if (aa <= 0) return 0.0;
   return 10.0 * log10(aa / err);
}

static void one(const char *name, const char *lavfi, unsigned channels)
{
   char cmd[1024];
   size_t ac3_len = 0, ref_len = 0, got = 0, total_read = 0;
   uint8_t *ac3; float *ref, *ours;
   unsigned ch = 0, rate = 0; uint64_t total = 0;
   void *ctx;
   int r;

   printf("   %s\n", name);
   snprintf(cmd, sizeof(cmd), "ffmpeg -hide_banner -loglevel error -y -f lavfi -i '%s' -t 2 -c:a ac3 -b:a 448k /tmp/at_ac3_%s.ac3 && "
         "ffmpeg -hide_banner -loglevel error -y -i /tmp/at_ac3_%s.ac3 -f f32le -c:a pcm_f32le /tmp/at_ac3_%s.f32",
         lavfi, name, name, name);
   if (system(cmd) != 0) { CHECK(0, "%s: ffmpeg could not make the streams", name); return; }
   snprintf(cmd, sizeof(cmd), "/tmp/at_ac3_%s.ac3", name);
   ac3 = read_all(cmd, &ac3_len);
   snprintf(cmd, sizeof(cmd), "/tmp/at_ac3_%s.f32", name);
   ref = (float*)read_all(cmd, &ref_len);
   if (!ac3 || !ref) { CHECK(0, "%s: no streams", name); return; }

   CHECK(audio_decode_get_type("music.ac3") == AUDIO_TYPE_AC3, "the .ac3 extension maps to the arm");
   ctx = audio_transfer_new(AUDIO_TYPE_AC3);
   CHECK(ctx != NULL, "new");
   audio_transfer_set_buffer_ptr(ctx, AUDIO_TYPE_AC3, ac3, ac3_len);
   CHECK(audio_transfer_start(ctx, AUDIO_TYPE_AC3), "%s: start", name);
   CHECK(audio_transfer_is_valid(ctx, AUDIO_TYPE_AC3), "%s: valid", name);
   CHECK(audio_transfer_info(ctx, AUDIO_TYPE_AC3, &ch, &rate, &total), "%s: info", name);
   CHECK(ch == channels && rate == 48000, "%s: info says %u ch %u Hz", name, ch, rate);
   CHECK(total == ref_len / (sizeof(float) * channels), "%s: %u frames reported, ffmpeg decoded %u", name, (unsigned)total, (unsigned)(ref_len / (sizeof(float) * channels)));

   /* the whole stream in odd-sized reads, against ffmpeg's decode */
   ours = (float*)calloc(total * channels + 4096 * channels, sizeof(float));
   do
   {
      size_t n = 0;
      r = audio_transfer_read_f32(ctx, AUDIO_TYPE_AC3, ours + total_read * channels, 700, &n);
      total_read += n;
      CHECK(r != AUDIO_PROCESS_ERROR, "%s: read error at frame %u", name, (unsigned)total_read);
   } while (r == AUDIO_PROCESS_NEXT && total_read < total + 4096);
   CHECK(r == AUDIO_PROCESS_END, "%s: the stream did not end (%d)", name, r);
   CHECK(total_read == total, "%s: read %u frames of %u", name, (unsigned)total_read, (unsigned)total);
   {
      size_t n = total_read < total ? total_read : total;
      double s = snr_db(ref, ours, n * channels);
      printf("      f32 against ffmpeg: %.1f dB over %u frames\n", s, (unsigned)n);
      CHECK(s > 40.0, "%s: %.1f dB against ffmpeg", name, s);
   }
   CHECK(audio_transfer_buffer_tell(ctx, AUDIO_TYPE_AC3) == ac3_len, "%s: tell at the end is %u of %u", name,
         (unsigned)audio_transfer_buffer_tell(ctx, AUDIO_TYPE_AC3), (unsigned)ac3_len);

   /* seek into the middle of a frame: what comes out is the playthrough's */
   {
      uint64_t target = 1536 * 7 + 300;
      float *chunk = (float*)calloc(2000 * channels, sizeof(float));
      size_t n = 0;
      CHECK(audio_transfer_seek(ctx, AUDIO_TYPE_AC3, target), "%s: seek", name);
      r = audio_transfer_read_f32(ctx, AUDIO_TYPE_AC3, chunk, 2000, &n);
      CHECK(n == 2000, "%s: read after seek gave %u", name, (unsigned)n);
      {
         double s = snr_db(ours + target * channels, chunk, n * channels);
         printf("      after a seek to frame %u: %.1f dB against the playthrough\n", (unsigned)target, s);
         CHECK(s > 60.0, "%s: seek lands %.1f dB off", name, s);
      }
      /* and the head, the loop's rewind */
      CHECK(audio_transfer_seek(ctx, AUDIO_TYPE_AC3, 0), "%s: seek to 0", name);
      r = audio_transfer_read_f32(ctx, AUDIO_TYPE_AC3, chunk, 2000, &n);
      CHECK(n == 2000 && snr_db(ours, chunk, n * channels) > 60.0, "%s: the rewind differs from the first read", name);
      free(chunk);
   }
   /* s16 is the f32 saturated */
   {
      int16_t *s16 = (int16_t*)calloc(1536 * channels, sizeof(int16_t));
      size_t n = 0, i; unsigned bad = 0;
      audio_transfer_seek(ctx, AUDIO_TYPE_AC3, 0);
      audio_transfer_read_s16(ctx, AUDIO_TYPE_AC3, s16, 1536, &n);
      for (i = 0; i < n * channels; i++)
      {
         float v = ours[i] * 32768.0f;
         int   q = v > 32767.0f ? 32767 : v < -32768.0f ? -32768 : (int)floorf(v + 0.5f);
         if (abs(q - s16[i]) > 1) bad++;
      }
      CHECK(bad == 0, "%s: %u s16 samples differ from the f32 by more than one", name, bad);
      free(s16);
   }
   /* a frontier: reads starve at it, and go on once it is raised */
   {
      float *chunk = (float*)calloc(1536 * channels, sizeof(float));
      size_t n = 0;
      audio_transfer_set_avail(ctx, AUDIO_TYPE_AC3, 100);   /* less than a frame */
      audio_transfer_seek(ctx, AUDIO_TYPE_AC3, 0);
      r = audio_transfer_read_f32(ctx, AUDIO_TYPE_AC3, chunk, 1536, &n);
      CHECK(r == AUDIO_PROCESS_NEXT && n == 0, "%s: at a frontier the read gave %u frames, %d", name, (unsigned)n, r);
      audio_transfer_set_avail(ctx, AUDIO_TYPE_AC3, 0);
      r = audio_transfer_read_f32(ctx, AUDIO_TYPE_AC3, chunk, 1536, &n);
      CHECK(r == AUDIO_PROCESS_NEXT && n == 1536, "%s: with the frontier gone the read gave %u", name, (unsigned)n);
      free(chunk);
   }
   audio_transfer_free(ctx, AUDIO_TYPE_AC3);
   free(ours); free(ref); free(ac3);
   (void)got;
   snprintf(cmd, sizeof(cmd), "rm -f /tmp/at_ac3_%s.ac3 /tmp/at_ac3_%s.f32", name, name); if (system(cmd)) { }
}

int main(void)
{
   printf("audio_transfer ac3:\n");
   if (system("ffmpeg -version >/dev/null 2>&1") != 0)
   {
      printf("   (ffmpeg not installed: skipped)\n");
      return 0;
   }
   one("stereo", "sine=f=440:r=48000[a];sine=f=660:r=48000[b];[a][b]join=inputs=2:channel_layout=stereo", 2);
   one("5.1",    "sine=f=220:r=48000[a];sine=f=330:r=48000[b];sine=f=440:r=48000[c];sine=f=60:r=48000[d];"
                 "sine=f=550:r=48000[e];sine=f=770:r=48000[f];[a][b][c][d][e][f]join=inputs=6:channel_layout=5.1(side)", 6);
   if (failures) { printf("%u failure(s)\n", failures); return 1; }
   printf("audio_transfer ac3: reads, seeks and the frontier as ffmpeg decodes it\n");
   return 0;
}
