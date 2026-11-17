/* rflac's 16-bit output, against the PCM each fixture was encoded from:
 *
 *   - a stream of 16 bits comes out exactly as it went in;
 *   - one of 24 or 32 bits comes out rounded to the nearest 16-bit step
 *     (halves up) and clamped at the top - not cut down, which left every
 *     sample half a step low on average;
 *   - both, through every way a stereo frame's channels are coded
 *     (independent, left/side, side/right, mid/side, which the stereo
 *     fixtures hold each of), a 32-bit stream's 33-bit side channel, mono
 *     and six channels, and the SIMD loops and their scalar tails (the
 *     fixtures are 5003 frames);
 *   - a FIXED or LPC subframe whose predictor order exceeds its block is
 *     rejected before its warm-up samples are stored, with and without a
 *     header and on the 33-bit side channel, and one whose order equals
 *     its block decodes;
 *   - rflac_set_out_s16_order stores each of the above little and big
 *     endian, whatever the host.
 *
 * Built twice: as the target builds (SSE2 on x86-64, NEON on ARM) and with
 * RFLAC_NO_SIMD, the scalar loops. The PCM is rebuilt here exactly as
 * mkfixtures.py builds it, with integers only. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <formats/rflac.h>
#include <streams/file_stream.h>

#define FRAMES 5003

static unsigned failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; \
   fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
   fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static int64_t tri(int64_t i, int64_t period, int64_t amp)
{
   int64_t p    = i % period;
   int64_t half = period / 2;
   int64_t v    = p < half ? p : period - p;
   return (v * 2 * amp) / half - amp;
}

/* mkfixtures.py's sample(), step for step */
static int64_t sample(const char *kind, unsigned bps, unsigned c, int64_t i,
      uint32_t *seed)
{
   int64_t top = ((int64_t)1 << (bps - 1)) - 1;
   int64_t n, v;
   *seed = *seed * 1103515245u + 12345u;
   n     = (int64_t)(*seed >> 20);
   if (!strcmp(kind, "stereo"))
   {
      int64_t base = tri(i, 397, top / 2);
      int64_t seg  = i / 1024;
      if (seg == 0)
         v = base + (c ? n : -n);
      else if (seg == 1)
         v = c == 0 ? base : base + n;
      else if (seg == 2)
         v = c == 0 ? base + n : base;
      else
         v = tri(i, 397 + 113 * c, top / 3) + n * (c + 1);
   }
   else if (!strcmp(kind, "wide"))
   {
      v = c == 0 ? top - (n & 255) : -top + (n & 255);
      if (i % 7 == 0)
         v = -v;
   }
   else
      v = tri(i, 211 + 37 * c, top / 2) + n;
   if (v > top)
      v = top;
   if (v < -top - 1)
      v = -top - 1;
   return v;
}

/* The 16-bit value a sample of @bps bits should give: its value at 16
 * bits, rounded to the nearest step, halves up, the top clamped */
static int16_t expect(int64_t v, unsigned bps)
{
   int64_t r;
   if (bps <= 16)
      return (int16_t)(v * ((int64_t)1 << (16 - bps)));
   r = (v + ((int64_t)1 << (bps - 17))) >> (bps - 16);
   return (int16_t)(r > 32767 ? 32767 : r);
}

/* @order: -1 host order through rflac_set_out_s16, else 0 little and 1
 * big endian through rflac_set_out_s16_order */
static void run(const char *name, const char *kind, unsigned bps, unsigned ch,
      int order)
{
   char      path[256];
   void     *buf = NULL;
   int64_t   len = 0;
   rflac_t  *f   = rflac_new();
   int16_t  *out = (int16_t*)calloc((size_t)(FRAMES + 64) * ch, sizeof(int16_t));
   size_t    got = 0, i;
   uint32_t  seed = bps * 131 + ch;
   unsigned  bad = 0, idle = 0;
   long      sum_err = 0;
   int       result;

   snprintf(path, sizeof(path), "fixtures/%s", name);
   if (!filestream_read_file(path, &buf, &len) || len <= 0)
   {
      fprintf(stderr, "cannot read %s (run from the sample directory)\n", path);
      exit(2);
   }
   CHECK(f != NULL, "%s: rflac_new", name);
   rflac_set_in(f, (const uint8_t*)buf, (size_t)len);
   rflac_set_eof(f);
   do
   {
      size_t wrote = 0;
      if (order < 0)
         rflac_set_out_s16(f, out + got * ch, FRAMES + 64 - got);
      else
         rflac_set_out_s16_order(f, out + got * ch, FRAMES + 64 - got, order);
      result = rflac_process(f, NULL, &wrote);
      got   += wrote;
      idle   = wrote ? 0 : idle + 1;
   } while (result == RFLAC_PROCESS_NEXT && idle < 4);

   CHECK(got == FRAMES, "%s: %lu frames, %d wanted", name, (unsigned long)got,
         FRAMES);
   for (i = 0; i < got * ch && i < (size_t)FRAMES * ch; i++)
   {
      int64_t v      = sample(kind, bps, (unsigned)(i % ch), (int64_t)(i / ch), &seed);
      int16_t want   = expect(v, bps);
      const uint8_t *b = (const uint8_t*)&out[i];
      int16_t have   = out[i];
      if (order >= 0)
         have = (int16_t)(order ? (b[0] << 8 | b[1]) : (b[1] << 8 | b[0]));
      if (have != want)
      {
         if (!bad)
            fprintf(stderr, "  %s/%d: sample %lu is %d, %d wanted (from %lld)\n",
                  name, order, (unsigned long)i, have, want, (long long)v);
         bad++;
      }
      sum_err += have - want;
   }
   CHECK(bad == 0, "%s: %u of %lu samples off", name, bad,
         (unsigned long)(got * ch));
   (void)sum_err;
   rflac_free(f);
   free(out);
   free(buf);
}

/* Streams built bit by bit, for frames no encoder writes. */
typedef struct
{
   uint8_t b[512];
   size_t  bit;
} bits_t;

static void put(bits_t *w, uint64_t v, unsigned n)
{
   while (n--)
   {
      if ((v >> n) & 1)
         w->b[w->bit >> 3] |= (uint8_t)(0x80 >> (w->bit & 7));
      w->bit++;
   }
}

static unsigned crc8(const uint8_t *p, size_t n)
{
   unsigned c = 0;
   size_t   i;
   int      k;
   for (i = 0; i < n; i++)
   {
      c ^= p[i];
      for (k = 0; k < 8; k++)
         c = (c & 0x80) ? ((c << 1) ^ 0x07) & 0xff : (c << 1) & 0xff;
   }
   return c;
}

static void packet_headers(void)
{
   static const int expected[16] = {0, 192, 576, 1152, 2304, 4608,
      38, 4661, 256, 512, 1024, 2048, 4096, 8192, 16384, 32768};
   static const uint8_t number[7] = {0, 0xc2, 0xe1, 0xf1, 0xf9, 0xfd, 0xfe};
   uint8_t header[20];
   unsigned code, rate, count, i;
   size_t at, truncated;
   CHECK(rflac_packet_frames(NULL, 100) == -1, "null packet header");
   for (code = 1; code < 16; code++)
      for (rate = 0; rate < 15; rate++)
         for (count = 1; count <= 7; count++)
         {
            memset(header, 0, sizeof(header));
            header[0] = 0xff; header[1] = 0xf9;
            header[2] = (uint8_t)((code << 4) | rate);
            header[3] = 8;
            header[4] = number[count - 1];
            for (i = 1; i < count; i++) header[4 + i] = 0x90;
            at = 4 + count;
            if (code == 6) header[at++] = 37;
            if (code == 7) { header[at++] = 0x12; header[at++] = 0x34; }
            if (rate == 12) header[at++] = 44;
            if (rate >= 13) { header[at++] = 0x12; header[at++] = 0xc0; }
            header[at] = (uint8_t)crc8(header, at);
            CHECK(rflac_packet_frames(header, at + 1) == expected[code],
                  "packet code %u rate %u number %u", code, rate, count);
            for (truncated = 0; truncated <= at; truncated++)
               CHECK(rflac_packet_frames(header, truncated) == -1,
                     "truncated packet header %lu", (unsigned long)truncated);
            header[at] ^= 1;
            CHECK(rflac_packet_frames(header, at + 1) == -1, "packet CRC");
            if (count > 1)
            {
               header[5] = 0;
               header[at] = (uint8_t)crc8(header, at);
               CHECK(rflac_packet_frames(header, at + 1) == -1, "number continuation");
            }
         }
   memset(header, 0, sizeof(header));
   header[0] = 0xff; header[1] = 0xf8; header[2] = 0x70; header[3] = 8;
   header[5] = header[6] = 0xff;
   header[7] = (uint8_t)crc8(header, 7);
   CHECK(rflac_packet_frames(header, 8) == -1, "65536-frame packet rejected");
   header[2] = 0x10;
   for (i = 0; i < 6; i++)
   {
      header[1] = 0xf8; header[2] = 0x10; header[3] = 8;
      if (i == 0) header[1] |= 2;
      if (i == 1) header[2] = 0;
      if (i == 2) header[2] |= 15;
      if (i == 3) header[3] = 0xb8;
      if (i == 4) header[3] = 6;
      if (i == 5) header[3] |= 1;
      header[5] = (uint8_t)crc8(header, 5);
      CHECK(rflac_packet_frames(header, 6) == -1, "reserved header field %u", i);
   }
}

static unsigned crc16(const uint8_t *p, size_t n)
{
   unsigned c = 0;
   size_t   i;
   int      k;
   for (i = 0; i < n; i++)
   {
      c ^= (unsigned)p[i] << 8;
      for (k = 0; k < 8; k++)
         c = (c & 0x8000) ? ((c << 1) ^ 0x8005) & 0xffff : (c << 1) & 0xffff;
   }
   return c;
}

enum { SUB_FIXED, SUB_LPC };

/* One frame of @block samples whose last channel is a FIXED or LPC
 * subframe of @order, warm-up samples all @warm; the channels before it
 * are CONSTANT 0. @bps 32 stereo codes left/side, so the predicted
 * channel is the 33-bit side one. With @hdr the stream opens with a
 * STREAMINFO stating @max_block. Returns the frame's offset. */
static size_t build(bits_t *w, int hdr, unsigned max_block, unsigned ch,
      unsigned bps, unsigned block, int sub, unsigned order, int64_t warm)
{
   size_t   at;
   unsigned c, i;
   unsigned sbps = (bps == 32 && ch == 2) ? 33 : bps;

   memset(w, 0, sizeof(*w));
   if (hdr)
   {
      put(w, 0x664C6143u, 32);            /* fLaC */
      put(w, 1, 1); put(w, 0, 7); put(w, 34, 24);
      put(w, max_block, 16); put(w, max_block, 16);
      put(w, 0, 24); put(w, 0, 24);
      put(w, 44100, 20); put(w, ch - 1, 3); put(w, bps - 1, 5);
      put(w, 0, 36);
      put(w, 0, 64); put(w, 0, 64);       /* MD5 */
   }
   at = w->bit >> 3;
   put(w, 0x3FFE, 14); put(w, 0, 2);
   put(w, 6, 4);                          /* 8-bit block size follows */
   put(w, 9, 4);                          /* 44.1 kHz */
   put(w, (bps == 32 && ch == 2) ? 8 : ch - 1, 4);
   put(w, bps == 32 ? 7 : 4, 3);          /* 32 or 16 bits */
   put(w, 0, 1);
   put(w, 0, 8);                          /* frame 0 */
   put(w, block - 1, 8);
   put(w, crc8(w->b + at, (w->bit >> 3) - at), 8);
   for (c = 0; c + 1 < ch; c++)
   {
      put(w, 0, 8);
      put(w, 0, bps);
   }
   put(w, sub == SUB_LPC ? (0x20 | (order - 1)) << 1 : (0x08 | order) << 1, 8);
   for (i = 0; i < order; i++)
      put(w, (uint64_t)warm, sbps);
   if (sub == SUB_LPC)
   {
      put(w, 14, 4);                      /* precision 15 */
      put(w, 0, 5);                       /* shift 0 */
      put(w, 1, 15);
      for (i = 1; i < order; i++)
         put(w, 0, 15);
   }
   put(w, 0, 2); put(w, 0, 4); put(w, 0, 4);   /* rice, order 0, k 0 */
   for (i = order; i < block; i++)
      put(w, 1, 1);                       /* residual 0 */
   w->bit = (w->bit + 7) & ~(size_t)7;
   put(w, crc16(w->b + at, w->bit >> 3), 16);
   return at;
}

static size_t decode(const uint8_t *in, size_t len, const rflac_format_t *raw,
      int16_t *out, size_t cap)
{
   rflac_t *f   = raw ? rflac_new_raw(raw) : rflac_new();
   size_t   got = 0;
   unsigned idle = 0;
   int      result;

   if (!f)
      return (size_t)-1;
   rflac_set_in(f, in, len);
   rflac_set_eof(f);
   do
   {
      size_t wrote = 0;
      rflac_set_out_s16(f, out + got, cap - got);
      result = rflac_process(f, NULL, &wrote);
      got   += wrote;
      idle   = wrote ? 0 : idle + 1;
   } while (result == RFLAC_PROCESS_NEXT && idle < 4);
   rflac_free(f);
   return got;
}

/* A predictor's warm-up samples are the first of its block, so its order
 * cannot exceed the block size. Every case below breaks that in a stream
 * whose stated maximum block is the 16 frames STREAMINFO allows at least,
 * so warm-up past the block would also be past the sample plane: these
 * run under ASan in CI. An order equal to the block size is legal and
 * decodes. */
static void predictor_order(void)
{
   bits_t         w;
   rflac_format_t raw;
   int16_t        out[64];
   size_t         at, got;

   at  = build(&w, 1, 16, 1, 16, 1, SUB_LPC, 32, 0x4141);
   got = decode(w.b, sizeof(w.b), NULL, out, 16);
   CHECK(got == 0, "LPC order 32 in a 1-frame block: %lu frames",
         (unsigned long)got);

   raw.sample_rate     = 44100;
   raw.channels        = 1;
   raw.bits_per_sample = 16;
   raw.block_size      = 1;
   at  = build(&w, 0, 0, 1, 16, 1, SUB_LPC, 32, 0x4141);
   got = decode(w.b + at, sizeof(w.b) - at, &raw, out, 16);
   CHECK(got == 0, "headerless LPC order 32 in a 1-frame block: %lu frames",
         (unsigned long)got);

   at  = build(&w, 1, 16, 1, 16, 2, SUB_FIXED, 4, 0x4141);
   got = decode(w.b, sizeof(w.b), NULL, out, 16);
   CHECK(got == 0, "FIXED order 4 in a 2-frame block: %lu frames",
         (unsigned long)got);

   at  = build(&w, 1, 16, 2, 32, 1, SUB_LPC, 32, 0x41414141);
   got = decode(w.b, sizeof(w.b), NULL, out, 16);
   CHECK(got == 0, "33-bit side LPC order 32 in a 1-frame block: %lu frames",
         (unsigned long)got);

   at  = build(&w, 0, 0, 1, 16, 1, SUB_LPC, 1, 0x1234);
   got = decode(w.b + at, sizeof(w.b) - at, &raw, out, 16);
   CHECK(got == 1 && out[0] == 0x1234,
         "LPC order 1 in a 1-frame block: %lu frames, sample %d",
         (unsigned long)got, got ? out[0] : 0);

   raw.block_size = 4;
   at  = build(&w, 0, 0, 1, 16, 4, SUB_FIXED, 4, -77);
   got = decode(w.b + at, sizeof(w.b) - at, &raw, out, 16);
   CHECK(got == 4 && out[0] == -77 && out[3] == -77,
         "FIXED order 4 in a 4-frame block: %lu frames", (unsigned long)got);
   (void)at;
}

int main(void)
{
   int order;

   packet_headers();
   predictor_order();

   for (order = -1; order <= 1; order++)
   {
      run("s16_stereo.flac", "stereo", 16, 2, order);
      run("s24_stereo.flac", "stereo", 24, 2, order);
      run("s24_mono.flac",   "mono",   24, 1, order);
      run("s24_6ch.flac",    "multi",  24, 6, order);
      run("s32_wide.flac",   "wide",   32, 2, order);
   }

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] rflac_s16_test\n");
   return 0;
}
