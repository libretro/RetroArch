/* audio_transfer's ATRAC arm: ATRAC3 and ATRAC3plus frames from the
 * decoders' own test streams, and one ATRAC3plus frame of each of 4.0,
 * 5.1, 6.1 and 7.1, wrapped here in WAVE (.at3) and OpenMG (.oma, with
 * an "ea3" tag) files.
 *
 *   audio_transfer_atrac_test
 *      each file sniffed as AUDIO_TYPE_ATRAC, started, and read in odd
 *      chunks as floats and as s16: the decoders' own output for the
 *      frames, in the WAV shape for the count (5.1, 6.1, 7.1 reordered,
 *      4.0's back centre a phantom pair); a seek into the middle reads
 *      what a playthrough does from there; the fold for 5.1; encrypted
 *      OpenMG refused; a cut last frame left out; every prefix of two
 *      files, and each of their bytes corrupted, read inside the buffer.
 *   audio_transfer_atrac_test --write DIR
 *      the files, for FFmpeg.
 *   audio_transfer_atrac_test --compare FILE PCM
 *      FILE decoded against FFmpeg's decode of it (f32le, FFmpeg's
 *      channel layout): the same samples, the WAV shape put back into
 *      FFmpeg's layout, ATRAC3 allowed the opposite polarity.
 * C99. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <formats/audio.h>
#include <formats/ratrac3.h>
#include <formats/ratrac3p.h>

#include "../atrac3/ratrac3_test_data.h"
#include "../atrac3p/ratrac3p_test_data.h"
#include "atrac_multichannel_data.h"

static int failures;

#define CHECK(x) do { if (!(x)) { failures++; \
   fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); } } while (0)

/* ---------------------------------------------------------------- */

static uint8_t *out;
static size_t   on;

static void put(const void *p, size_t n) { memcpy(out + on, p, n); on += n; }
static void put8(unsigned v) { out[on++] = (uint8_t)v; }
static void put16(unsigned v) { put8(v & 0xff); put8((v >> 8) & 0xff); }
static void put32(uint32_t v) { put16(v & 0xffff); put16(v >> 16); }
static void fill(unsigned v, size_t n) { while (n--) put8(v); }
static void be16(unsigned v) { put8((v >> 8) & 0xff); put8(v & 0xff); }

struct file
{
   const char *name;
   uint8_t    *d;
   size_t      n;
   int         plus;
   unsigned    channels;   /* decoded */
   unsigned    ba;
   int         joint;
   const uint8_t *frames;
   unsigned    nframes;
};

static void begin(struct file *f, size_t cap)
{
   f->d = out = (uint8_t*)malloc(cap);
   on = 0;
}

static void riff(struct file *f, const char *name, int plus, unsigned ch,
      unsigned ba, int joint, const uint8_t *frames, unsigned n, size_t extra)
{
   static const uint8_t guid[16] = { 0xBF,0xAA,0x23,0xE9,0x58,0xCB,0x71,0x44,
                                     0xA1,0x19,0xFF,0xFA,0x01,0xE4,0xCE,0x62 };
   size_t data = (size_t)ba * n + extra;
   f->name = name; f->plus = plus; f->channels = ch; f->ba = ba; f->joint = joint;
   f->frames = frames; f->nframes = n;
   begin(f, data + 128);
   put("RIFF", 4); put32((uint32_t)(4 + 8 + (plus ? 52 : 32) + 8 + data)); put("WAVE", 4);
   put("fmt ", 4);
   if (plus)
   {
      put32(52); put16(0xFFFE); put16(ch); put32(44100);
      put32((uint32_t)((unsigned long)ba * 44100 / 2048)); put16(ba); put16(0);
      put16(34); put16(0); put32(0); put(guid, 16); fill(0, 12);
   }
   else
   {
      put32(32); put16(0x270); put16(ch); put32(44100);
      put32((uint32_t)((unsigned long)ba * 44100 / 1024)); put16(ba); put16(0);
      put16(14); put16(1); put32(0x1000); put16(joint); put16(joint); put16(1); put16(0);
   }
   put("data", 4); put32((uint32_t)data);
   put(frames, (size_t)ba * n + extra);
   f->n = on;
}

/* OpenMG: an "ea3" tag with a title, then the EA3 header */
static void oma(struct file *f, const char *name, int plus, unsigned ch_id,
      unsigned ch, unsigned ba, int joint, const uint8_t *frames, unsigned n,
      unsigned eid)
{
   static const char title[] = "\x03Oma Title";   /* UTF-8 */
   uint32_t params;
   f->name = name; f->plus = plus; f->channels = ch; f->ba = ba; f->joint = joint;
   f->frames = frames; f->nframes = n;
   begin(f, (size_t)ba * n + 256);
   put("ea3\x03\x00\x00", 6); put8(0); put8(0); put8(0); put8(10 + 10 + 10);
   put("TIT2", 4); put32(0); out[on - 1] = 10; out[on - 2] = 0; out[on - 3] = 0; out[on - 4] = 0;
   be16(0); put(title, 10); fill(0, 10);
   put("EA3\x01", 4); be16(96); be16(eid); fill(0, 24);
   params = (1u << 13) | (plus ? (ch_id << 10) | ((ba - 8) / 8) : ((unsigned)joint << 17) | (ba / 8));
   put8(plus ? 1 : 0); put8((params >> 16) & 0xff); put8((params >> 8) & 0xff); put8(params & 0xff);
   fill(0, 96 - 36);
   put(frames, (size_t)ba * n);
   f->n = on;
}

/* ---------------------------------------------------------------- */

/* what the arm hands over for a decoded frame, in the WAV shape */
static unsigned shape(const struct file *f, const float *dec, float *wav)
{
   static const signed char maps[9][8] = {
      { 0 }, { 0 }, { 0, 1 }, { 0, 1, 2 }, { 0, 1, 2, 3, 3 }, { 0 },
      { 0, 1, 2, 5, 3, 4 }, { 0, 1, 2, 6, 5, 3, 4 }, { 0, 1, 2, 7, 3, 4, 5, 6 } };
   unsigned spf = f->plus ? 2048 : 1024, ch = f->channels, oc = ch == 4 && f->plus ? 5 : ch;
   unsigned i, c;
   for (i = 0; i < spf; i++)
      for (c = 0; c < oc; c++)
         wav[i * oc + c] = f->plus
            ? dec[i * ch + maps[ch][c]] * (ch == 4 && c >= 3 ? 0.70710678f : 1.0f)
            : dec[i * ch + c];
   return oc;
}

/* the whole stream as the arm should hand it over */
static float *expected(const struct file *f, unsigned *oc)
{
   unsigned spf = f->plus ? 2048 : 1024, k;
   float   *dec = (float*)malloc(sizeof(float) * spf * 8);
   float   *all = (float*)malloc(sizeof(float) * spf * 8 * f->nframes);
   ratrac3_t  *d3  = f->plus ? NULL : ratrac3_new(f->channels, f->ba, f->joint != 0);
   ratrac3p_t *d3p = f->plus ? ratrac3p_new(f->channels, f->ba) : NULL;
   for (k = 0; k < f->nframes; k++)
   {
      if (f->plus)
         ratrac3p_decode_float(d3p, f->frames + (size_t)k * f->ba, f->ba, dec);
      else
         ratrac3_decode_float(d3, f->frames + (size_t)k * f->ba, f->ba, dec);
      *oc = shape(f, dec, all + (size_t)k * spf * (f->channels == 4 && f->plus ? 5 : f->channels));
   }
   ratrac3_free(d3);
   ratrac3p_free(d3p);
   free(dec);
   return all;
}

static int16_t to_s16(float v)
{
   float s = v * 32768.0f;
   s += s >= 0 ? 0.5f : -0.5f;
   if (s > 32767.0f) s = 32767.0f;
   if (s < -32768.0f) s = -32768.0f;
   return (int16_t)(int)s;
}

static void check_file(struct file *f)
{
   unsigned spf = f->plus ? 2048 : 1024, oc = 0, ch = 0, rate = 0;
   uint64_t total = 0, n;
   size_t   skip = 1, at = 0, got;
   float   *want = expected(f, &oc);
   float   *pcm  = (float*)malloc(sizeof(float) * spf * 8 * (f->nframes + 1));
   int16_t *s16  = (int16_t*)malloc(sizeof(int16_t) * spf * 8 * (f->nframes + 1));
   void    *ctx;
   double   worst = 0;
   int      bad16 = 0;
   size_t   i;

   CHECK(audio_transfer_sniff_type(f->d, f->n, &skip) == AUDIO_TYPE_ATRAC && skip == 0);
   ctx = audio_transfer_new(AUDIO_TYPE_ATRAC);
   audio_transfer_set_buffer_ptr(ctx, AUDIO_TYPE_ATRAC, f->d, f->n);
   if (!audio_transfer_start(ctx, AUDIO_TYPE_ATRAC))
   {
      failures++;
      fprintf(stderr, "%s: does not start\n", f->name);
      return;
   }
   CHECK(audio_transfer_info(ctx, AUDIO_TYPE_ATRAC, &ch, &rate, &total));
   if (ch != oc || rate != 44100 || total != (uint64_t)spf * f->nframes)
   {
      failures++;
      fprintf(stderr, "%s: %u channels at %u Hz, %u frames\n", f->name, ch, rate, (unsigned)total);
   }

   /* floats in odd chunks, then the end */
   n = (uint64_t)spf * f->nframes;
   while (at < n)
   {
      int r = audio_transfer_read_f32(ctx, AUDIO_TYPE_ATRAC, pcm + at * oc, 1000, &got);
      CHECK(r == AUDIO_PROCESS_NEXT);
      if (!got)
         break;
      at += got;
   }
   CHECK(at == n);
   CHECK(audio_transfer_read_f32(ctx, AUDIO_TYPE_ATRAC, pcm, 1000, &got) == AUDIO_PROCESS_END && !got);
   for (i = 0; i < at * oc; i++)
   {
      double d = fabs((double)pcm[i] - want[i]);
      if (d > worst) worst = d;
   }
   if (worst > 0)
   {
      failures++;
      fprintf(stderr, "%s: float samples off by %g\n", f->name, worst);
   }

   /* s16, from the start again */
   CHECK(audio_transfer_seek(ctx, AUDIO_TYPE_ATRAC, 0));
   at = 0;
   while (at < n && audio_transfer_read_s16(ctx, AUDIO_TYPE_ATRAC, s16 + at * oc, 777, &got)
         == AUDIO_PROCESS_NEXT && got)
      at += got;
   CHECK(at == n);
   for (i = 0; i < at * oc; i++)
      if (s16[i] != to_s16(want[i]))
         bad16++;
   CHECK(!bad16);

   /* a seek past the first frame: what a playthrough gives from there */
   if (f->nframes > 2)
   {
      uint64_t from = spf + 100;
      double   sw   = 0;
      CHECK(audio_transfer_seek(ctx, AUDIO_TYPE_ATRAC, from));
      CHECK(audio_transfer_read_f32(ctx, AUDIO_TYPE_ATRAC, pcm, spf, &got) == AUDIO_PROCESS_NEXT
            && got == spf);
      for (i = 0; i < got * oc; i++)
      {
         double d = fabs((double)pcm[i] - want[from * oc + i]);
         if (d > sw) sw = d;
      }
      if (sw > 1e-3)
      {
         failures++;
         fprintf(stderr, "%s: after a seek, off by %g\n", f->name, sw);
      }
      CHECK(!audio_transfer_seek(ctx, AUDIO_TYPE_ATRAC, n));
   }

   if (oc == 6)
   {
      int32_t m[8][2];
      const int32_t (*fold)[2] = audio_transfer_fold_q15(AUDIO_TYPE_ATRAC, 6);
      CHECK(audio_transfer_stereo_matrix(ctx, AUDIO_TYPE_ATRAC, m)
            && !memcmp(m, fold, sizeof(int32_t) * 12) && !m[6][0] && !m[7][1]);
   }

   printf("audio_transfer_atrac_test: %s: %u channels out, %u frames, as the decoder gives them\n",
         f->name, oc, (unsigned)n);
   audio_transfer_free(ctx, AUDIO_TYPE_ATRAC);
   free(want);
   free(pcm);
   free(s16);
}

static void play(const uint8_t *src, size_t len, size_t pos, int m)
{
   static float pcm[2048 * 8];
   uint8_t *c   = (uint8_t*)malloc(len ? len : 1);
   void    *ctx = audio_transfer_new(AUDIO_TYPE_ATRAC);
   size_t   got;
   memcpy(c, src, len);
   if (m >= 0)
      c[pos] = (uint8_t)(m == 0 ? 0 : m == 1 ? 0xff : ~c[pos]);
   audio_transfer_set_buffer_ptr(ctx, AUDIO_TYPE_ATRAC, c, len);
   if (audio_transfer_start(ctx, AUDIO_TYPE_ATRAC))
      while (audio_transfer_read_f32(ctx, AUDIO_TYPE_ATRAC, pcm, 2048, &got)
            == AUDIO_PROCESS_NEXT && got) { }
   audio_transfer_free(ctx, AUDIO_TYPE_ATRAC);
   free(c);
}

/* every prefix, and each header byte set to 0, 0xff and flipped:
 * started and read to the end inside the buffer, each copy allocated
 * to its size for ASan */
static void harden(const struct file *f)
{
   size_t len, i;
   int    m;
   for (len = 0; len <= f->n; len++)
      play(f->d, len, 0, -1);
   for (i = 0; i < f->n && i < 200; i++)
      for (m = 0; m < 3; m++)
         play(f->d, f->n, i, m);
}

static struct file files[16];
static unsigned    nfiles;

static void build(void)
{
   riff(&files[nfiles++], "at3_stereo.wav", 0, 2, 384, 0, stream_a, 12, 0);
   riff(&files[nfiles++], "at3_joint.wav", 0, 2, 192, 1, stream_b, 12, 0);
   oma(&files[nfiles++], "at3_joint.oma", 0, 0, 2, 192, 1, stream_b, 12, 0xFFFF);
   riff(&files[nfiles++], "at3p_stereo.wav", 1, 2, STEREO_BA, 0, stream_st, STEREO_N, 0);
   oma(&files[nfiles++], "at3p_stereo.oma", 1, 2, 2, STEREO_BA, 0, stream_st, STEREO_N, 0xFFFF);
   riff(&files[nfiles++], "at3p_mono.wav", 1, 1, MONO_BA, 0, stream_mo, MONO_N, 0);
   riff(&files[nfiles++], "at3p_4.0.wav", 1, 4, 4008, 0, frame_cfg4, 1, 0);
   riff(&files[nfiles++], "at3p_5.1.wav", 1, 6, 5896, 0, frame_cfg5, 1, 0);
   oma(&files[nfiles++], "at3p_5.1.oma", 1, 5, 6, 5896, 0, frame_cfg5, 1, 0xFFFF);
   riff(&files[nfiles++], "at3p_6.1.wav", 1, 7, 6960, 0, frame_cfg6, 1, 0);
   riff(&files[nfiles++], "at3p_7.1.wav", 1, 8, 7776, 0, frame_cfg7, 1, 0);
}

static int run_checks(void)
{
   struct file f;
   unsigned    i;
   build();
   for (i = 0; i < nfiles; i++)
      check_file(&files[i]);

   /* encrypted OpenMG */
   oma(&f, "encrypted", 1, 2, 2, STEREO_BA, 0, stream_st, 1, 0x0001);
   {
      void *ctx = audio_transfer_new(AUDIO_TYPE_ATRAC);
      audio_transfer_set_buffer_ptr(ctx, AUDIO_TYPE_ATRAC, f.d, f.n);
      CHECK(!audio_transfer_start(ctx, AUDIO_TYPE_ATRAC));
      audio_transfer_free(ctx, AUDIO_TYPE_ATRAC);
      free(f.d);
   }
   /* a cut last frame is left out */
   riff(&f, "cut", 0, 2, 384, 0, stream_a, 3, 200);
   {
      void    *ctx = audio_transfer_new(AUDIO_TYPE_ATRAC);
      uint64_t total = 0;
      audio_transfer_set_buffer_ptr(ctx, AUDIO_TYPE_ATRAC, f.d, f.n);
      CHECK(audio_transfer_start(ctx, AUDIO_TYPE_ATRAC)
            && audio_transfer_info(ctx, AUDIO_TYPE_ATRAC, NULL, NULL, &total)
            && total == 3 * 1024);
      audio_transfer_free(ctx, AUDIO_TYPE_ATRAC);
      free(f.d);
   }

   riff(&f, "harden.wav", 0, 2, 192, 1, stream_b, 2, 0);
   harden(&f);
   free(f.d);
   oma(&f, "harden.oma", 1, 1, 1, MONO_BA + 5, 0, stream_mo, 1, 0xFFFF);
   harden(&f);
   free(f.d);

   if (failures)
   {
      fprintf(stderr, "audio_transfer_atrac_test: %d check(s) failed\n", failures);
      return 1;
   }
   printf("audio_transfer_atrac_test: ok\n");
   return 0;
}

/* ---------------------------------------------------------------- */

static uint8_t *slurp(const char *path, size_t *len)
{
   FILE    *fp = fopen(path, "rb");
   uint8_t *p;
   long     n;
   if (!fp)
      return NULL;
   fseek(fp, 0, SEEK_END);
   n = ftell(fp);
   fseek(fp, 0, SEEK_SET);
   p = (uint8_t*)malloc(n > 0 ? (size_t)n : 1);
   if (p && fread(p, 1, (size_t)n, fp) != (size_t)n)
   {
      free(p);
      p = NULL;
   }
   fclose(fp);
   *len = (size_t)n;
   return p;
}

/* FILE through the arm against FFmpeg's decode of it */
static int run_compare(const char *file, const char *pcm_path)
{
   size_t   len = 0, flen = 0, got, at = 0, frames, i;
   uint8_t *d  = slurp(file, &len);
   float   *ff = (float*)slurp(pcm_path, &flen);
   void    *ctx;
   unsigned ch = 0, fch, c;
   uint64_t total = 0;
   float   *pcm;
   double   pos = 0, neg = 0, worst;
   if (!d || !ff)
      return 1;
   ctx = audio_transfer_new(AUDIO_TYPE_ATRAC);
   audio_transfer_set_buffer_ptr(ctx, AUDIO_TYPE_ATRAC, d, len);
   if (!audio_transfer_start(ctx, AUDIO_TYPE_ATRAC)
         || !audio_transfer_info(ctx, AUDIO_TYPE_ATRAC, &ch, NULL, &total))
   {
      fprintf(stderr, "%s: does not start\n", file);
      return 1;
   }
   fch    = ch == 5 ? 4 : ch;   /* 4.0 comes out as five */
   frames = flen / sizeof(float) / fch;
   if (frames != total)
   {
      fprintf(stderr, "%s: %u frames, FFmpeg's %u\n", file, (unsigned)total, (unsigned)frames);
      return 1;
   }
   pcm = (float*)malloc(sizeof(float) * (size_t)total * ch);
   while (at < total && audio_transfer_read_f32(ctx, AUDIO_TYPE_ATRAC, pcm + at * ch,
            4096, &got) == AUDIO_PROCESS_NEXT && got)
      at += got;
   for (i = 0; i < frames; i++)
      for (c = 0; c < ch; c++)
      {
         /* where FFmpeg has this channel: 6.1's back centre is its last,
          * the arm's back pair sits in WAV 7's side slots; 4.0's phantom
          * pair is FFmpeg's back centre at -3 dB */
         unsigned fc = c;
         float    g  = 1.0f, a = pcm[i * ch + c], b;
         if (ch == 7 && c >= 4)
            fc = c == 4 ? 6 : c - 1;
         if (ch == 5 && c >= 3)
         {
            fc = 3;
            g  = 0.70710678f;
         }
         b = ff[i * fch + fc] * g;
         if (fabs(a - b) > pos) pos = fabs(a - b);
         if (fabs(a + b) > neg) neg = fabs(a + b);
      }
   worst = pos < neg ? pos : neg;
   printf("audio_transfer_atrac_test: %s: %u frames, within %.1e of FFmpeg's%s\n",
         file, (unsigned)frames, worst, neg < pos ? " (opposite polarity)" : "");
   audio_transfer_free(ctx, AUDIO_TYPE_ATRAC);
   free(pcm);
   free(d);
   free(ff);
   return worst <= 1e-4 ? 0 : 1;
}

int main(int argc, char **argv)
{
   (void)ref_a; (void)ref_b; (void)ref_st; (void)ref_mo; (void)stream_gha; (void)ref_gha;
   if (argc == 3 && !strcmp(argv[1], "--write"))
   {
      unsigned i;
      build();
      for (i = 0; i < nfiles; i++)
      {
         char  path[1024];
         FILE *fp;
         snprintf(path, sizeof(path), "%s/%s", argv[2], files[i].name);
         if (!(fp = fopen(path, "wb")) || fwrite(files[i].d, 1, files[i].n, fp) != files[i].n)
            return 1;
         fclose(fp);
      }
      return 0;
   }
   if (argc == 4 && !strcmp(argv[1], "--compare"))
      return run_compare(argv[2], argv[3]);
   return run_checks();
}
