/* audio_transfer_sniff_type and the fold to stereo.
 *
 *   audio_transfer_sniff_test
 *      files built here byte by byte, one for each container and raw
 *      stream syntax the sniffer knows (ID3v2 tags and junk in front),
 *      and look-alikes it must turn down; every prefix of each, and each
 *      byte corrupted, read inside the buffer; the fold tables.
 *   audio_transfer_sniff_test --file FILE TYPE [--tones] [--pcm REF]
 *                             [--frames N]
 *      FILE sniffed as TYPE (wav flac vorbis opus mp3 aac ac3 wma mod),
 *      started where the sniffer says and decoded to the end. With
 *      --pcm the decode is FFmpeg's (REF, f32le at the file's rate):
 *      the same length, sample for sample within 1e-4 - where the audio
 *      starts and ends, gapless trims and all. With --frames the length
 *      audio_transfer_info states is N. With
 *      --tones FILE is 5.1 with a tone in each channel - FL 300 Hz,
 *      FR 500, FC 700, LFE 80, BL 1100, BR 1300 - and the decode folded
 *      with audio_transfer_stereo_matrix has the left tones on the left
 *      side only, the right ones on the right only, the centre on both
 *      and the LFE on neither: the arm's channel order and its fold
 *      agree.
 * C99. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <formats/audio.h>

static int failures;

#define CHECK(x) do { if (!(x)) { failures++; \
   fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); } } while (0)

static const char *type_name(enum audio_type_enum t)
{
   switch (t)
   {
      case AUDIO_TYPE_WAV:    return "wav";
      case AUDIO_TYPE_FLAC:   return "flac";
      case AUDIO_TYPE_VORBIS: return "vorbis";
      case AUDIO_TYPE_MP3:    return "mp3";
      case AUDIO_TYPE_MOD:    return "mod";
      case AUDIO_TYPE_OPUS:   return "opus";
      case AUDIO_TYPE_AAC:    return "aac";
      case AUDIO_TYPE_LPCM:   return "lpcm";
      case AUDIO_TYPE_AC3:    return "ac3";
      case AUDIO_TYPE_WMA:    return "wma";
      default:                return "none";
   }
}

/* ---------------------------------------------------------------- */

struct fx
{
   const char          *name;
   enum audio_type_enum want;
   size_t               skip;
   uint8_t              d[2048];
   size_t               n;
};

static struct fx *cur;

static void put(const void *p, size_t n) { memcpy(cur->d + cur->n, p, n); cur->n += n; }
static void put8(unsigned v) { cur->d[cur->n++] = (uint8_t)v; }
static void put16(unsigned v) { put8(v & 0xff); put8(v >> 8); }
static void put32(uint32_t v) { put16(v & 0xffff); put16(v >> 16); }
static void fill(unsigned v, size_t n) { while (n--) put8(v); }

static void id3(size_t body)
{
   put("ID3\x03\x00\x00", 6);
   put8((unsigned)(body >> 21) & 0x7f); put8((unsigned)(body >> 14) & 0x7f);
   put8((unsigned)(body >> 7) & 0x7f);  put8((unsigned)body & 0x7f);
   fill(0, body);
}

static void wave(const char *form, unsigned tag, unsigned sub)
{
   put("RIFF", 4); put32(100); put(form, 4);
   put("fmt ", 4);
   if (tag == 0xFFFE)
   {
      put32(40); put16(tag); put16(6); put32(48000); put32(48000 * 12);
      put16(12); put16(16); put16(22); put16(16); put32(0x3f);
      put16(sub); put("\x00\x00\x00\x00\x10\x00\x80\x00\x00\xaa\x00\x38\x9b\x71", 14);
   }
   else
   {
      put32(16); put16(tag); put16(2); put32(44100); put32(44100 * 4);
      put16(4); put16(16);
   }
   put("data", 4); put32(8); fill(0, 8);
}

/* MPEG-1 layer III, 128 kb/s, 44.1 kHz: 417 bytes a frame */
static void mp3_frame(void) { put("\xFF\xFB\x90\x00", 4); fill(0, 413); }
/* ADTS, AAC-LC, 44.1 kHz stereo, 100 bytes a frame */
static void adts_frame(void) { put("\xFF\xF1\x50\x80\x0C\x9F\xFC", 7); fill(0, 93); }

static struct fx fxs[32];
static unsigned  nfx;

static void add(const char *name, enum audio_type_enum want)
{
   cur = &fxs[nfx++];
   cur->name = name;
   cur->want = want;
   cur->n    = 0;
   cur->skip = 0;
}

static void build(void)
{
   static const uint8_t asf[16] = { 0x30,0x26,0xB2,0x75,0x8E,0x66,0xCF,0x11,
                                    0xA6,0xD9,0x00,0xAA,0x00,0x62,0xCE,0x6C };
   add("WAVE PCM", AUDIO_TYPE_WAV);              wave("WAVE", 1, 0);
   add("WAVE float", AUDIO_TYPE_WAV);            wave("WAVE", 3, 0);
   add("WAVE extensible PCM", AUDIO_TYPE_WAV);   wave("WAVE", 0xFFFE, 1);
   add("WAVE WMA2", AUDIO_TYPE_WMA);             wave("WAVE", 0x0161, 0);
   add("xWMA", AUDIO_TYPE_WMA);                  wave("XWMA", 0x0161, 0);
   add("WAVE MP3, not an arm's", AUDIO_TYPE_NONE); wave("WAVE", 0x0055, 0);
   add("RIFF AVI", AUDIO_TYPE_NONE);             wave("AVI ", 1, 0);

   add("FLAC", AUDIO_TYPE_FLAC);
   put("fLaC", 4); put8(0x80); put8(0); put8(0); put8(34); fill(0, 34);
   add("FLAC behind ID3v2", AUDIO_TYPE_FLAC);
   id3(20); cur->skip = 30;
   put("fLaC", 4); put8(0x80); put8(0); put8(0); put8(34); fill(0, 34);

   add("Ogg Opus", AUDIO_TYPE_OPUS);
   put("OggS\0\x02", 6); fill(0, 20); put8(1); put8(19); put("OpusHead", 8); fill(1, 11);
   add("Ogg Vorbis", AUDIO_TYPE_VORBIS);
   put("OggS\0\x02", 6); fill(0, 20); put8(1); put8(30); put("\x01vorbis", 7); fill(0, 23);
   add("Ogg Theora", AUDIO_TYPE_NONE);
   put("OggS\0\x02", 6); fill(0, 20); put8(1); put8(30); put("\x80theora", 7); fill(0, 23);

   add("ASF", AUDIO_TYPE_WMA); put(asf, 16); fill(0, 30);
   add("MP4", AUDIO_TYPE_AAC); put("\0\0\0\x18" "ftypM4A ", 12); fill(0, 12);

   add("XM", AUDIO_TYPE_MOD); put("Extended Module: song", 21); fill(0, 60);
   add("S3M", AUDIO_TYPE_MOD); fill(0, 44); put("SCRM", 4); fill(0, 16);
   add("MOD M.K.", AUDIO_TYPE_MOD); fill(0, 1080); put("M.K.", 4); fill(0, 8);
   add("MOD 16CH", AUDIO_TYPE_MOD); fill(0, 1080); put("16CH", 4); fill(0, 8);

   add("MP3 behind ID3v2 and junk", AUDIO_TYPE_MP3);
   id3(10); fill(0, 5); cur->skip = 25;
   mp3_frame(); mp3_frame(); put("\xFF\xFB\x90\x00", 4);
   add("one MPEG audio header, junk where the next would be", AUDIO_TYPE_NONE);
   mp3_frame(); fill(0x11, 64);
   add("ADTS", AUDIO_TYPE_AAC);
   adts_frame(); adts_frame(); adts_frame();
   add("ADTS, one frame to the end", AUDIO_TYPE_AAC);
   adts_frame();
   add("junk", AUDIO_TYPE_NONE);
   fill(0x5a, 300);
}

/* every prefix and each byte set to 0, 0xff and flipped: no read
 * outside the buffer (each copy is allocated to its size, for ASan) */
static void harden(const struct fx *f)
{
   size_t len, i;
   int    m;
   for (len = 0; len <= f->n; len++)
   {
      uint8_t *c = (uint8_t*)malloc(len ? len : 1);
      size_t   skip = 0;
      memcpy(c, f->d, len);
      audio_transfer_sniff_type(c, len, &skip);
      CHECK(skip <= len);
      free(c);
   }
   for (i = 0; i < f->n; i++)
      for (m = 0; m < 3; m++)
      {
         uint8_t *c = (uint8_t*)malloc(f->n);
         size_t   skip = 0;
         memcpy(c, f->d, f->n);
         c[i] = (uint8_t)(m == 0 ? 0 : m == 1 ? 0xff : ~c[i]);
         audio_transfer_sniff_type(c, f->n, &skip);
         CHECK(skip <= f->n);
         free(c);
      }
}

static int run_checks(void)
{
   const int32_t (*f)[2];
   unsigned i;

   build();
   for (i = 0; i < nfx; i++)
   {
      size_t               skip = 99999;
      enum audio_type_enum t    = audio_transfer_sniff_type(fxs[i].d, fxs[i].n, &skip);
      if (t != fxs[i].want || (t != AUDIO_TYPE_NONE && skip != fxs[i].skip))
      {
         failures++;
         fprintf(stderr, "%s: sniffed %s at %u, not %s at %u\n", fxs[i].name,
               type_name(t), (unsigned)skip, type_name(fxs[i].want), (unsigned)fxs[i].skip);
      }
      harden(&fxs[i]);
   }
   CHECK(audio_transfer_sniff_type(NULL, 10, NULL) == AUDIO_TYPE_NONE);
   CHECK(audio_transfer_sniff_type(fxs[0].d, 0, NULL) == AUDIO_TYPE_NONE);

   /* the folds: 5.1 in each order, mono, and past the tables */
   f = audio_transfer_fold_q15(AUDIO_TYPE_FLAC, 6);    /* FL FR FC LFE BL BR */
   CHECK(f && f[0][0] == 32768 && f[0][1] == 0 && f[1][1] == 32768 && f[1][0] == 0
         && f[2][0] == 23170 && f[2][1] == 23170 && !f[3][0] && !f[3][1]
         && f[4][0] == 23170 && !f[4][1] && !f[5][0] && f[5][1] == 23170);
   f = audio_transfer_fold_q15(AUDIO_TYPE_OPUS, 6);    /* L C R RL RR LFE */
   CHECK(f && f[0][0] == 32768 && f[1][0] == 23170 && f[1][1] == 23170
         && f[2][1] == 32768 && !f[2][0] && f[3][0] == 23170 && f[4][1] == 23170
         && !f[5][0] && !f[5][1]);
   f = audio_transfer_fold_q15(AUDIO_TYPE_VORBIS, 7);  /* ... BC LFE */
   CHECK(f && f[5][0] == 16384 && f[5][1] == 16384 && !f[6][0]);
   f = audio_transfer_fold_q15(AUDIO_TYPE_MP3, 1);
   CHECK(f && f[0][0] == 32768 && f[0][1] == 32768);
   CHECK(!audio_transfer_fold_q15(AUDIO_TYPE_WAV, 0) && !audio_transfer_fold_q15(AUDIO_TYPE_WAV, 9));

   if (failures)
   {
      fprintf(stderr, "audio_transfer_sniff_test: %d check(s) failed\n", failures);
      return 1;
   }
   printf("audio_transfer_sniff_test: ok (%u files, every prefix and byte corrupted; the folds)\n", nfx);
   return 0;
}

/* ---------------------------------------------------------------- */

/* power at @hz in @n samples of @x (stride 2), by Goertzel */
static double tone(const float *x, size_t n, double hz, double rate)
{
   double w = 2.0 * 3.14159265358979323846 * hz / rate;
   double c = 2.0 * cos(w), s1 = 0, s2 = 0;
   size_t i;
   for (i = 0; i < n; i++)
   {
      double s0 = x[2 * i] + c * s1 - s2;
      s2 = s1;
      s1 = s0;
   }
   return (s1 * s1 + s2 * s2 - c * s1 * s2) / ((double)n * n);
}

static int run_file(const char *path, const char *want, int tones,
      const char *pcm_path, long want_frames)
{
   FILE                *fp = fopen(path, "rb");
   uint8_t             *buf;
   long                 len;
   size_t               skip = 0, frames = 0, cap;
   enum audio_type_enum t;
   void                *ctx;
   unsigned             ch = 0, rate = 0;
   float               *pcm, *st;
   uint64_t             total = 0;
   int                  bad = 0;

   if (!fp)
   {
      fprintf(stderr, "cannot open %s\n", path);
      return 1;
   }
   fseek(fp, 0, SEEK_END);
   len = ftell(fp);
   fseek(fp, 0, SEEK_SET);
   buf = (uint8_t*)malloc((size_t)len);
   if (!buf || fread(buf, 1, (size_t)len, fp) != (size_t)len)
      return 1;
   fclose(fp);

   t = audio_transfer_sniff_type(buf, (size_t)len, &skip);
   if (strcmp(type_name(t), want))
   {
      fprintf(stderr, "%s: sniffed %s, not %s\n", path, type_name(t), want);
      return 1;
   }
   ctx = audio_transfer_new(t);
   audio_transfer_set_buffer_ptr(ctx, t, buf + skip, (size_t)len - skip);
   if (!audio_transfer_start(ctx, t) || !audio_transfer_info(ctx, t, &ch, &rate, &total)
         || !ch || ch > 8 || !rate)
   {
      fprintf(stderr, "%s: does not start\n", path);
      return 1;
   }
   if (want_frames >= 0 && total != (uint64_t)want_frames)
   {
      fprintf(stderr, "%s: states %lu frames, not %ld\n", path, (unsigned long)total, want_frames);
      bad = 1;
   }
   cap = (size_t)rate * 4;   /* the files are shorter than four seconds */
   pcm = (float*)malloc(cap * ch * sizeof(float));
   for (;;)
   {
      size_t got = 0;
      int    r   = audio_transfer_read_f32(ctx, t, pcm + frames * ch,
            cap - frames < 4096 ? cap - frames : 4096, &got);
      frames += got;
      if (r == AUDIO_PROCESS_ERROR)
      {
         fprintf(stderr, "%s: decode error after %u frames\n", path, (unsigned)frames);
         return 1;
      }
      if (r == AUDIO_PROCESS_END || !got || frames == cap)
         break;
   }

   if (pcm_path)
   {
      FILE  *rf = fopen(pcm_path, "rb");
      long   rlen;
      float *ref;
      size_t i, rn;
      double worst = 0;
      if (!rf)
         return 1;
      fseek(rf, 0, SEEK_END);
      rlen = ftell(rf);
      fseek(rf, 0, SEEK_SET);
      ref = (float*)malloc((size_t)rlen + 1);
      if (fread(ref, 1, (size_t)rlen, rf) != (size_t)rlen)
         return 1;
      fclose(rf);
      rn = (size_t)rlen / sizeof(float) / ch;
      for (i = 0; i < (rn < frames ? rn : frames) * ch; i++)
         if (fabs((double)pcm[i] - ref[i]) > worst)
            worst = fabs((double)pcm[i] - ref[i]);
      if (rn != frames || worst > 1e-4)
      {
         fprintf(stderr, "%s: %u frames, FFmpeg's %u, samples within %g of FFmpeg's\n",
               path, (unsigned)frames, (unsigned)rn, worst);
         bad = 1;
      }
      free(ref);
   }

   if (tones)
   {
      static const double hz[6]   = { 300, 500, 700, 80, 1100, 1300 };
      static const char   side[6] = { 'L', 'R', 'C', '-', 'L', 'R' };
      float    m[8][2];
      size_t   i, a = frames / 4, n = frames / 2;   /* past any priming */
      unsigned c, k;
      double   pl[6], pr[6], top = 0;
      if (ch != 6 || !audio_transfer_stereo_matrix_f32(ctx, t, m))
      {
         fprintf(stderr, "%s: %u channels, or no fold\n", path, ch);
         return 1;
      }
      st = (float*)malloc(frames * 2 * sizeof(float));
      for (i = 0; i < frames; i++)
      {
         float l = 0, r = 0;
         for (c = 0; c < ch; c++)
         {
            l += pcm[i * ch + c] * m[c][0];
            r += pcm[i * ch + c] * m[c][1];
         }
         st[2 * i] = l;
         st[2 * i + 1] = r;
      }
      for (k = 0; k < 6; k++)
      {
         pl[k] = tone(st + 2 * a, n, hz[k], rate);
         pr[k] = tone(st + 2 * a + 1, n, hz[k], rate);
         if (pl[k] > top) top = pl[k];
         if (pr[k] > top) top = pr[k];
      }
      for (k = 0; k < 6; k++)
      {
         int lw = side[k] == 'L' || side[k] == 'C';
         int rw = side[k] == 'R' || side[k] == 'C';
         if (     (lw ? pl[k] < 0.2 * top : pl[k] > 0.02 * top)
               || (rw ? pr[k] < 0.2 * top : pr[k] > 0.02 * top))
         {
            fprintf(stderr, "%s: the %.0f Hz tone is %.3f left, %.3f right of the loudest\n",
                  path, hz[k], pl[k] / top, pr[k] / top);
            bad = 1;
         }
      }
      free(st);
   }
   if (!bad)
      printf("audio_transfer_sniff_test: %s: %s, %u channels at %u Hz, %u frames%s%s\n",
            path, want, ch, rate, (unsigned)frames, tones ? ", each tone where it belongs" : "",
            pcm_path ? ", FFmpeg's samples" : "");
   audio_transfer_free(ctx, t);
   free(pcm);
   free(buf);
   return bad;
}

int main(int argc, char **argv)
{
   if (argc >= 4 && !strcmp(argv[1], "--file"))
   {
      int         i, tones = 0;
      const char *pcm = NULL;
      long        frames = -1;
      for (i = 4; i < argc; i++)
      {
         if (!strcmp(argv[i], "--tones"))
            tones = 1;
         else if (!strcmp(argv[i], "--pcm") && i + 1 < argc)
            pcm = argv[++i];
         else if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            frames = atol(argv[++i]);
      }
      return run_file(argv[2], argv[3], tones, pcm, frames);
   }
   return run_checks();
}
