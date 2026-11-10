/* rmp3's one-frame-at-a-time API against its pull API.
 *
 * rmp3dec_decode_s16/_f32 are the frame decoder the pull API runs,
 * handed the caller's buffer. Walked over a file frame by frame they
 * must give the samples rmp3_read_s16/_f32 give, to the bit, and
 * account for every byte, also when only part of the next frame
 * follows; junk ahead of a frame is skipped and counted in
 * frame_bytes, a buffer with no whole frame gives frame_bytes 0, and
 * rmp3dec_init forgets the bit reservoir. Streams from ffmpeg (libmp3lame): MPEG-1
 * stereo and mono at several rates, MPEG-2 at 22 kHz.
 *
 * Needs ffmpeg with libmp3lame in PATH; a missing tool is a failure. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>

#include <formats/rmp3.h>

static int fails;
static char dir[256];

static void check(const char *what, int ok)
{
   printf("  %-64s %s\n", what, ok ? "ok" : "FAIL");
   if (!ok)
      fails++;
}

static int run(const char *fmt, ...)
{
   char cmd[1024];
   va_list ap;
   va_start(ap, fmt);
   vsnprintf(cmd, sizeof(cmd), fmt, ap);
   va_end(ap);
   return system(cmd);
}

static uint8_t *slurp(const char *path, size_t *len)
{
   FILE *f = fopen(path, "rb");
   uint8_t *b;
   long n;
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   n = ftell(f);
   fseek(f, 0, SEEK_SET);
   if (n <= 0 || !(b = (uint8_t*)malloc((size_t)n + 64)))
   {
      fclose(f);
      return NULL;
   }
   if (fread(b, 1, (size_t)n, f) != (size_t)n)
   {
      fclose(f);
      free(b);
      return NULL;
   }
   fclose(f);
   *len = (size_t)n;
   return b;
}

static void frame_case(const char *name, const char *args, unsigned want_ch)
{
   char path[512], label[160];
   size_t len = 0, pos = 0, total = 0, ref_n, k;
   uint8_t *buf;
   int16_t *ref, *got, pcm[RMP3_MAX_SAMPLES_PER_FRAME];
   rmp3 *pull;
   rmp3dec *dec;
   unsigned ch = 0;
   int frames = 0, skipped = 0, same = 1;

   snprintf(path, sizeof(path), "%s/%s.mp3", dir, name);
   if (run("ffmpeg -v error -y -f lavfi -i \"sine=f=440:r=44100,volume=0.5\" "
           "-f lavfi -i \"anoisesrc=a=0.2:r=44100\" -filter_complex amix=inputs=2 "
           "-t 3 -c:a libmp3lame -write_xing 0 -id3v2_version 0 %s '%s'",
           args, path) != 0 || !(buf = slurp(path, &len)))
   {
      snprintf(label, sizeof(label), "%s: encoded", name);
      check(label, 0);
      return;
   }
   printf("%s (%s):\n", name, args);

   /* the reference: everything through the pull API */
   pull = (rmp3*)calloc(1, sizeof(*pull));
   ref  = (int16_t*)malloc(44100 * 4 * 2 * sizeof(int16_t));
   got  = (int16_t*)malloc(44100 * 4 * 2 * sizeof(int16_t));
   dec  = (rmp3dec*)malloc(sizeof(*dec));
   if (!pull || !ref || !got || !dec || !rmp3_init_memory(pull, buf, len))
   {
      check("allocated", 0);
      return;
   }
   ref_n = (size_t)rmp3_read_s16(pull, 44100 * 4, ref) * pull->channels;
   ch    = pull->channels;
   rmp3_uninit(pull);

   /* the same frame by frame, each call handed the rest of the file */
   rmp3dec_init(dec);
   while (pos < len)
   {
      rmp3dec_frame_info info;
      int n = rmp3dec_decode_s16(dec, buf + pos, (int)(len - pos), pcm, &info);
      if (info.frame_bytes <= 0)
         break;
      pos += (size_t)info.frame_bytes;
      if (n == 0)
      {
         skipped++;
         continue;
      }
      if (total + (size_t)n * info.channels > (size_t)44100 * 4 * 2)
         break;
      memcpy(got + total, pcm, (size_t)n * info.channels * sizeof(int16_t));
      total += (size_t)n * info.channels;
      frames++;
   }
   if (total != ref_n)
      same = 0;
   for (k = 0; same && k < total; k++)
      if (got[k] != ref[k])
         same = 0;
   printf("      %d frames, %d without samples, %u channel(s), %lu samples\n",
         frames, skipped, ch, (unsigned long)total);
   snprintf(label, sizeof(label), "%s: frame by frame is the pull API's output", name);
   check(label, same && total > 0 && ch == want_ch);
   snprintf(label, sizeof(label), "%s: every byte accounted for", name);
   check(label, pos == len);

   /* a frame handed over with just the first two bytes of the next
    * behind it, as a caller framing the stream itself has it, decodes
    * the same */
   {
      size_t   at    = 0, t = 0;
      int      ok    = 1;
      rmp3dec *probe = (rmp3dec*)malloc(sizeof(*probe));
      rmp3dec_init(dec);
      while (ok && at < len)
      {
         rmp3dec_frame_info info;
         size_t win;
         /* this frame's length, from a decoder handed the rest */
         rmp3dec_init(probe);
         rmp3dec_decode_s16(probe, buf + at, (int)(len - at), pcm, &info);
         win = (size_t)info.frame_bytes + 2;
         if (win > len - at)
            win = len - at;
         int    n   = rmp3dec_decode_s16(dec, buf + at, (int)win, pcm, &info);
         if (info.frame_bytes <= 0)
            break;
         at += (size_t)info.frame_bytes;
         if (t + (size_t)n * info.channels > ref_n
               || memcmp(pcm, ref + t, (size_t)n * info.channels * sizeof(int16_t)))
            ok = 0;
         t += (size_t)n * info.channels;
      }
      free(probe);
      snprintf(label, sizeof(label), "%s: a frame and part of the next", name);
      check(label, ok && at == len && t == ref_n);
   }

   /* junk ahead of the first frame is skipped and counted */
   {
      uint8_t *junk = (uint8_t*)malloc(len + 100);
      rmp3dec_frame_info info;
      int n;
      memset(junk, 0x55, 100);
      memcpy(junk + 100, buf, len);
      rmp3dec_init(dec);
      n = rmp3dec_decode_s16(dec, junk, (int)(len + 100), pcm, &info);
      snprintf(label, sizeof(label), "%s: junk before the sync skipped", name);
      check(label, n > 0 && info.frame_bytes > 100
            && !memcmp(pcm, ref, (size_t)n * info.channels * sizeof(int16_t)));
      free(junk);
   }

   /* no whole frame: nothing taken */
   {
      rmp3dec_frame_info info;
      rmp3dec_init(dec);
      rmp3dec_decode_s16(dec, buf, 3, pcm, &info);
      snprintf(label, sizeof(label), "%s: a partial header takes nothing", name);
      check(label, info.frame_bytes == 0);
   }

   /* init forgets the reservoir: decoding from the middle and from the
    * start of the same frame differ only while the reservoir refills */
   {
      rmp3dec_frame_info info;
      int n;
      rmp3dec_init(dec);
      n = rmp3dec_decode_s16(dec, buf, (int)len, pcm, &info);
      snprintf(label, sizeof(label), "%s: init starts the stream afresh", name);
      check(label, n > 0
            && !memcmp(pcm, ref, (size_t)n * info.channels * sizeof(int16_t)));
   }

   /* the float pipeline, frame by frame against rmp3_read_f32 */
   {
      float *fref = (float*)malloc(44100 * 4 * 2 * sizeof(float));
      float *fgot = (float*)malloc(44100 * 4 * 2 * sizeof(float));
      float  fpcm[RMP3_MAX_SAMPLES_PER_FRAME];
      size_t fref_n = 0, ftotal = 0;
      memset(pull, 0, sizeof(*pull));
      if (fref && fgot && rmp3_init_memory(pull, buf, len))
      {
         fref_n = (size_t)rmp3_read_f32(pull, 44100 * 4, fref) * pull->channels;
         rmp3_uninit(pull);
      }
      rmp3dec_init(dec);
      pos  = 0;
      same = fref && fgot;
      while (same && pos < len)
      {
         rmp3dec_frame_info info;
         int n = rmp3dec_decode_f32(dec, buf + pos, (int)(len - pos), fpcm, &info);
         if (info.frame_bytes <= 0)
            break;
         pos += (size_t)info.frame_bytes;
         if (ftotal + (size_t)n * info.channels > (size_t)44100 * 4 * 2)
            break;
         memcpy(fgot + ftotal, fpcm, (size_t)n * info.channels * sizeof(float));
         ftotal += (size_t)n * info.channels;
      }
      if (ftotal != fref_n || !ftotal)
         same = 0;
      for (k = 0; same && k < ftotal; k++)
         if (memcmp(&fgot[k], &fref[k], sizeof(float)))
            same = 0;
      snprintf(label, sizeof(label), "%s: f32 frame by frame is rmp3_read_f32's", name);
      check(label, same && pos == len);
      free(fref);
      free(fgot);
   }

   free(buf);
   free(pull);
   free(ref);
   free(got);
   free(dec);
}

int main(void)
{
   if (system("ffmpeg -version >/dev/null 2>&1") != 0)
   {
      printf("rmp3_frame_test: ffmpeg is required (with libmp3lame)\n");
      return 1;
   }
   snprintf(dir, sizeof(dir), "/tmp/rmp3_frame_%ld", (long)getpid());
   if (run("mkdir -p '%s'", dir) != 0)
      return 2;

   frame_case("stereo_128k", "-ac 2 -b:a 128k", 2);
   frame_case("stereo_320k", "-ac 2 -b:a 320k", 2);
   frame_case("mono_64k",    "-ac 1 -b:a 64k", 1);
   frame_case("vbr",         "-ac 2 -q:a 4", 2);
   frame_case("mpeg2_22k",   "-ac 2 -ar 22050 -b:a 32k", 2);

   run("rm -rf '%s'", dir);
   printf("rmp3_frame_test: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
         fails, fails == 1 ? "" : "s");
   return fails ? 1 : 0;
}
