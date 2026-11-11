/* Copyright (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------
 * The following license statement only applies to this file (rmp4_mux_test.c).
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

/* rmp4_mux: files written and read back with rmp4's demuxer.
 *
 *   mpeg4  MPEG-4 Part 2 video with B-VOPs (the rmpeg4_test fixture, its
 *          VOL headers as the decoder configuration) and 16-bit PCM
 *   mjpeg  motion JPEG from rjpeg_encode and big-endian float PCM
 *   avc    H.264 and AAC sample entries (avcC, an AudioSpecificConfig)
 *          around placeholder samples, for the structure alone
 *
 * Each must open in rmp4 with its tracks, sizes, rates, decoder
 * configurations and esds object types (MPEG-4 Visual and JPEG behind
 * the same mp4v), and give back every sample - its bytes, its key flag,
 * its place in presentation order - and the video's first frame must be
 * shown at time zero (B-VOPs delay it in decode order: the edit list
 * takes that off). A track full at max_samples must refuse the next one,
 * a write failing must fail the file, and the samples must have gone
 * out without the writer staging them.
 *
 * With a directory argument the files are written there too, for the
 * CI workflow to decode with FFmpeg.
 *
 * Build and run:  make && ./rmp4_mux_test [dir]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <formats/rmp4.h>
#include <formats/rmp4_mux.h>
#include <formats/rjpeg_encode.h>

#include "../mpeg4/rmpeg4_fixtures.h"

static unsigned failures;

#define CHECK(c) do { if (!(c)) { printf("  FAIL  %s (line %u)\n", #c, \
      (unsigned)__LINE__); failures++; } } while (0)

/* A seekable file in memory */
typedef struct
{
   unsigned char *p;
   size_t         size, cap, at;
   unsigned       writes, mark;
   int            fail_after;   /* fail the nth write; 0 never */
} membuf_t;

static bool mem_write(void *user, const void *data, size_t len)
{
   membuf_t *b = (membuf_t*)user;
   b->writes++;
   if (b->fail_after && (int)b->writes >= b->fail_after)
      return false;
   if (b->at + len > b->cap)
   {
      size_t cap = (b->at + len) * 2 + 4096;
      unsigned char *p = (unsigned char*)realloc(b->p, cap);
      if (!p)
         return false;
      b->p   = p;
      b->cap = cap;
   }
   memcpy(b->p + b->at, data, len);
   b->at += len;
   if (b->at > b->size)
      b->size = b->at;
   return true;
}

static bool mem_seek(void *user, uint64_t offset)
{
   membuf_t *b = (membuf_t*)user;
   if (offset > b->size)
      return false;
   b->at = (size_t)offset;
   return true;
}

static void save(const char *dir, const char *name, const membuf_t *b)
{
   char  path[1024];
   FILE *f;
   if (!dir)
      return;
   sprintf(path, "%.900s/%s", dir, name);
   if ((f = fopen(path, "wb")))
   {
      fwrite(b->p, 1, b->size, f);
      fclose(f);
   }
}

/* --------------------------------------------------------------------- */

typedef struct
{
   const unsigned char *data;
   size_t               len;
   int64_t              pts, dts;
   bool                 sync;
} sample_t;

#define MAX_SAMPLES 64

typedef struct
{
   sample_t s[MAX_SAMPLES];
   unsigned n;
   unsigned seen;
} track_samples_t;

/* The fixture's VOPs, with their times: the VOL headers before the first
 * VOP are the configuration. A reference shows after the B-VOPs that
 * follow it in the stream; dts runs one behind, so that no VOP is shown
 * before it is decoded. */
static size_t split_m4v(const unsigned char *d, size_t len, track_samples_t *t)
{
   size_t   i, first = len, starts[MAX_SAMPLES + 1];
   unsigned n = 0, k, bcount = 0;
   int64_t  prev_ref = 0, cur_ref = -1;

   for (i = 0; i + 4 <= len && n < MAX_SAMPLES; i++)
      if (!d[i] && !d[i + 1] && d[i + 2] == 1 && d[i + 3] == 0xB6)
      {
         if (first == len)
            first = i;
         starts[n++] = i;
      }
   starts[n] = len;
   t->n = n;
   for (k = 0; k < n; k++)
   {
      sample_t *s = &t->s[k];
      unsigned  type = d[starts[k] + 4] >> 6;
      s->data = d + starts[k];
      s->len  = starts[k + 1] - starts[k];
      s->dts  = (int64_t)k - 1;
      s->sync = type == 0;
      if (type == 2)
         s->pts = prev_ref + (int64_t)++bcount;
      else
      {
         unsigned b = 0, j;
         for (j = k + 1; j < n && (d[starts[j] + 4] >> 6) == 2; j++)
            b++;
         prev_ref = cur_ref;
         cur_ref += (int64_t)b + 1;
         s->pts   = cur_ref;
         bcount   = 0;
      }
   }
   return first;
}

/* Read the file back: every sample, in file order, matched to what was
 * written, and every one seen */
static void read_back(const membuf_t *b, track_samples_t *tracks, unsigned count,
      const uint32_t *timescale, int64_t *first_ts)
{
   rmp4_t     *mp4 = rmp4_open_memory(b->p, b->size);
   rmp4_packet pkt;
   unsigned    i;

   CHECK(mp4 != NULL);
   if (!mp4)
      return;
   CHECK(rmp4_num_tracks(mp4) == (int)count);
   for (i = 0; i < count; i++)
   {
      tracks[i].seen = 0;
      first_ts[i]    = -1;
   }
   while (rmp4_read_packet(mp4, &pkt) == 1)
   {
      track_samples_t *t;
      unsigned k;
      CHECK(pkt.track >= 0 && pkt.track < (int)count);
      if (pkt.track < 0 || pkt.track >= (int)count)
         break;
      t = &tracks[pkt.track];
      k = t->seen++;
      CHECK(k < t->n);
      if (k >= t->n)
         break;
      CHECK(pkt.size == t->s[k].len && !memcmp(pkt.data, t->s[k].data, pkt.size));
      CHECK(!pkt.keyframe == !t->s[k].sync);
      if (first_ts[pkt.track] < 0 || pkt.timestamp < first_ts[pkt.track])
         first_ts[pkt.track] = pkt.timestamp;
      (void)timescale;
   }
   for (i = 0; i < count; i++)
      CHECK(tracks[i].seen == tracks[i].n);
   rmp4_close(mp4);
}

static rmp4_mux_t *open_mux(membuf_t *b, const rmp4_mux_track_t *t, unsigned n)
{
   struct rmp4_mux_io io;
   io.write = mem_write;
   io.seek  = mem_seek;
   io.user  = b;
   return rmp4_mux_open(&io, t, n);
}

/* 16-bit or float PCM, in packets of 1024 frames: placed in the samples
 * as one sample a packet */
static void make_pcm(unsigned char *buf, unsigned frames, unsigned bytes_per,
      track_samples_t *t, uint32_t rate)
{
   unsigned i;
   for (i = 0; i < frames * bytes_per; i++)
      buf[i] = (unsigned char)((i * 37u) ^ (i >> 7));
   t->n = frames / 1024;
   for (i = 0; i < t->n; i++)
   {
      t->s[i].data = buf + (size_t)i * 1024 * bytes_per;
      t->s[i].len  = 1024 * bytes_per;
      t->s[i].pts  = t->s[i].dts = (int64_t)i * 1024;
      t->s[i].sync = true;
   }
   (void)rate;
}

/* rmp4 hands PCM back a frame at a time: what matters is that the bytes
 * and their count come back */
static void read_back_pcm_track(const membuf_t *b, unsigned track,
      const unsigned char *pcm, size_t bytes)
{
   rmp4_t     *mp4 = rmp4_open_memory(b->p, b->size);
   rmp4_packet pkt;
   size_t      at = 0;
   bool        same = true;

   if (!mp4)
      return;
   while (rmp4_read_packet(mp4, &pkt) == 1)
   {
      if (pkt.track != (int)track)
         continue;
      if (at + pkt.size > bytes || memcmp(pcm + at, pkt.data, pkt.size))
         same = false;
      at += pkt.size;
   }
   CHECK(same && at == bytes);
   rmp4_close(mp4);
}

static void test_mpeg4(const char *dir)
{
   static unsigned char pcm[1024 * 2 * 24];
   track_samples_t  v, a;
   rmp4_mux_track_t t[2];
   membuf_t         b;
   rmp4_mux_t      *m;
   size_t           cfg = split_m4v(m4_asp, sizeof(m4_asp), &v);
   unsigned         i;
   int64_t          first[2];
   uint32_t         ts[2] = { 25, 48000 };
   rmp4_t          *mp4;

   memset(&b, 0, sizeof(b));
   memset(t, 0, sizeof(t));
   t[0].codec = RMP4_MUX_MPEG4_VIDEO;
   t[0].timescale = 25;
   t[0].width = 64;
   t[0].height = 48;
   t[0].config = m4_asp;
   t[0].config_len = cfg;
   t[0].max_samples = v.n;
   t[1].codec = RMP4_MUX_PCM_S16LE;
   t[1].timescale = 48000;
   t[1].sample_rate = 48000;
   t[1].channels = 1;
   t[1].max_samples = MAX_SAMPLES;
   make_pcm(pcm, 1024 * 24, 2, &a, 48000);

   m = open_mux(&b, t, 2);
   CHECK(m != NULL);
   if (!m)
      return;
   b.mark = b.writes;
   for (i = 0; i < v.n || i < a.n; i++)
   {
      if (i < v.n)
         CHECK(rmp4_mux_write(m, 0, v.s[i].data, v.s[i].len, v.s[i].pts, v.s[i].dts, v.s[i].sync));
      if (i < a.n)
         CHECK(rmp4_mux_write(m, 1, a.s[i].data, a.s[i].len, a.s[i].pts, a.s[i].dts, true));
   }
   /* every sample went out in a write of its own, from where it lies */
   CHECK(b.writes - b.mark == v.n + a.n);
   /* the track is full */
   CHECK(!rmp4_mux_write(m, 0, v.s[0].data, v.s[0].len, 100, 100, true));
   CHECK(rmp4_mux_close(m));
   save(dir, "mpeg4_pcm.mp4", &b);
   printf("mpeg4 + PCM: %u VOPs, %u audio packets, %lu bytes\n", v.n, a.n,
         (unsigned long)b.size);

   mp4 = rmp4_open_memory(b.p, b.size);
   CHECK(mp4 != NULL);
   if (mp4)
   {
      const rmp4_track *tv = rmp4_get_track(mp4, 0), *ta = rmp4_get_track(mp4, 1);
      CHECK(tv && tv->type == RMP4_TRACK_VIDEO && tv->width == 64 && tv->height == 48
            && !strcmp(tv->codec_id, "mp4v"));
      /* MPEG-4 Visual by the esds, its VOL headers the codec_private */
      CHECK(tv && tv->object_type == 0x20 && tv->codec_private_size == cfg
            && !memcmp(tv->codec_private, m4_asp, cfg));
      CHECK(ta && ta->object_type == 0);
      /* the edit list takes off the tick B-VOPs delay the first frame by */
      CHECK(tv && tv->media_skip == 1);
      CHECK(ta && ta->type == RMP4_TRACK_AUDIO && ta->sample_rate == 48000
            && ta->channels == 1 && !strcmp(ta->codec_id, "sowt"));
      CHECK(rmp4_duration_ns(mp4) > 0);
      rmp4_close(mp4);
   }
   {
      rmp4_t     *r = rmp4_open_memory(b.p, b.size);
      rmp4_packet pkt;
      unsigned    k = 0;
      int64_t     min_ts = -1;
      bool        same = true;
      if (r)
      {
         while (rmp4_read_packet(r, &pkt) == 1)
         {
            if (pkt.track != 0)
               continue;
            if (k >= v.n || pkt.size != v.s[k].len || memcmp(pkt.data, v.s[k].data, pkt.size)
                  || !pkt.keyframe != !v.s[k].sync)
               same = false;
            /* presentation times, in decode order, follow the pts given:
             * rmp4 counts them from the first decode time (one tick
             * before the first frame shows) and reports the edit apart */
            if (k < v.n && pkt.timestamp != (v.s[k].pts - v.s[0].dts) * 40000000LL)
               same = false;
            if (min_ts < 0 || pkt.timestamp < min_ts)
               min_ts = pkt.timestamp;
            k++;
         }
         rmp4_close(r);
      }
      CHECK(same && k == v.n);
      CHECK(min_ts == 40000000LL);
   }
   read_back_pcm_track(&b, 1, pcm, sizeof(pcm));
   (void)first;
   (void)ts;
   free(b.p);
}

static void test_mjpeg(const char *dir)
{
   static rjpeg_encoder_t e;
   static unsigned char   y[96 * 64], cb[48 * 32], cr[48 * 32];
   static unsigned char   jpg[6][8192];
   static unsigned char   pcm[1024 * 8 * 6];
   track_samples_t  v, a, all[2];
   rmp4_mux_track_t t[2];
   membuf_t         b;
   rmp4_mux_t      *m;
   unsigned         i, k;
   int64_t          first[2];
   uint32_t         ts[2] = { 30, 32000 };

   rjpeg_encoder_init(&e, 80);
   v.n = 6;
   for (i = 0; i < v.n; i++)
   {
      for (k = 0; k < sizeof(y); k++)
         y[k] = (unsigned char)((k % 96) * 2 + i * 20);
      memset(cb, (int)(100 + i * 10), sizeof(cb));
      memset(cr, 150, sizeof(cr));
      v.s[i].len  = rjpeg_encode(&e, jpg[i], sizeof(jpg[i]), 96, 64, RJPEG_ENCODE_420,
            y, 96, cb, 48, cr, 48);
      v.s[i].data = jpg[i];
      v.s[i].pts  = v.s[i].dts = i;
      v.s[i].sync = true;
      CHECK(v.s[i].len > 0);
   }
   memset(t, 0, sizeof(t));
   t[0].codec = RMP4_MUX_MJPEG;
   t[0].timescale = 30;
   t[0].width = 96;
   t[0].height = 64;
   t[0].max_samples = v.n;
   t[1].codec = RMP4_MUX_PCM_F32BE;
   t[1].timescale = 32000;
   t[1].sample_rate = 32000;
   t[1].channels = 2;
   t[1].max_samples = MAX_SAMPLES;
   make_pcm(pcm, 1024 * 6, 8, &a, 32000);

   memset(&b, 0, sizeof(b));
   m = open_mux(&b, t, 2);
   CHECK(m != NULL);
   if (!m)
      return;
   for (i = 0; i < v.n; i++)
   {
      CHECK(rmp4_mux_write(m, 0, v.s[i].data, v.s[i].len, v.s[i].pts, v.s[i].dts, true));
      CHECK(rmp4_mux_write(m, 1, a.s[i].data, a.s[i].len, a.s[i].pts, a.s[i].dts, true));
   }
   /* not a whole number of frames */
   CHECK(!rmp4_mux_write(m, 1, pcm, 7, 99999, 99999, true));
   /* times going backwards */
   CHECK(!rmp4_mux_write(m, 0, jpg[0], v.s[0].len, 1, 1, true));
   CHECK(rmp4_mux_close(m));
   save(dir, "mjpeg_f32.mp4", &b);
   printf("mjpeg + float PCM: %lu bytes\n", (unsigned long)b.size);

   all[0] = v;
   all[1].n = 0;
   {
      rmp4_t *r = rmp4_open_memory(b.p, b.size);
      CHECK(r != NULL);
      if (r)
      {
         const rmp4_track *ta = rmp4_get_track(r, 1), *tv = rmp4_get_track(r, 0);
         CHECK(ta && !strcmp(ta->codec_id, "fl32") && ta->channels == 2
               && ta->sample_rate == 32000);
         /* the same mp4v fourcc, told apart as JPEG by the esds */
         CHECK(tv && !strcmp(tv->codec_id, "mp4v") && tv->object_type == 0x6C
               && tv->codec == RMP4_CODEC_UNKNOWN && !tv->codec_private_size);
         rmp4_close(r);
      }
   }
   {
      rmp4_t     *r = rmp4_open_memory(b.p, b.size);
      rmp4_packet pkt;
      unsigned    n = 0;
      bool        same = true;
      if (r)
      {
         while (rmp4_read_packet(r, &pkt) == 1)
            if (pkt.track == 0)
            {
               if (n >= v.n || pkt.size != v.s[n].len || memcmp(pkt.data, v.s[n].data, pkt.size)
                     || !pkt.keyframe)
                  same = false;
               n++;
            }
         rmp4_close(r);
      }
      CHECK(same && n == v.n);
   }
   read_back_pcm_track(&b, 1, pcm, 1024 * 8 * 6);
   (void)all;
   (void)first;
   (void)ts;
   free(b.p);
}

static void test_avc_aac(const char *dir)
{
   static const unsigned char avcc[] =
   { 0x01, 0x42, 0xC0, 0x1E, 0xFF, 0xE1, 0x00, 0x04, 0x67, 0x42, 0xC0, 0x1E,
     0x01, 0x00, 0x02, 0x68, 0xCE };
   static const unsigned char asc[] = { 0x12, 0x10 };   /* AAC-LC, 44.1 kHz, stereo */
   static unsigned char samples[16][100];
   track_samples_t  s[2];
   rmp4_mux_track_t t[2];
   membuf_t         b;
   rmp4_mux_t      *m;
   unsigned         i;
   int64_t          first[2];
   uint32_t         ts[2] = { 90000, 44100 };

   memset(t, 0, sizeof(t));
   t[0].codec = RMP4_MUX_H264;
   t[0].timescale = 90000;
   t[0].width = 320;
   t[0].height = 240;
   t[0].config = avcc;
   t[0].config_len = sizeof(avcc);
   t[0].max_samples = 8;
   t[1].codec = RMP4_MUX_AAC;
   t[1].timescale = 44100;
   t[1].sample_rate = 44100;
   t[1].channels = 2;
   t[1].config = asc;
   t[1].config_len = sizeof(asc);
   t[1].max_samples = 8;
   t[1].bitrate = 128000;
   for (i = 0; i < 16; i++)
      memset(samples[i], (int)(i + 1), sizeof(samples[i]));
   for (i = 0; i < 8; i++)
   {
      s[0].s[i].data = samples[i];
      s[0].s[i].len  = 50 + i;
      s[0].s[i].dts  = (int64_t)i * 3000;
      s[0].s[i].pts  = (int64_t)i * 3000;
      s[0].s[i].sync = i == 0 || i == 4;
      s[1].s[i].data = samples[8 + i];
      s[1].s[i].len  = 80 + i;
      s[1].s[i].dts  = s[1].s[i].pts = (int64_t)i * 1024;
      s[1].s[i].sync = true;
   }
   s[0].n = s[1].n = 8;

   memset(&b, 0, sizeof(b));
   m = open_mux(&b, t, 2);
   CHECK(m != NULL);
   if (!m)
      return;
   for (i = 0; i < 8; i++)
   {
      CHECK(rmp4_mux_write(m, 0, s[0].s[i].data, s[0].s[i].len, s[0].s[i].pts, s[0].s[i].dts, s[0].s[i].sync));
      CHECK(rmp4_mux_write(m, 1, s[1].s[i].data, s[1].s[i].len, s[1].s[i].pts, s[1].s[i].dts, true));
   }
   CHECK(rmp4_mux_close(m));

   read_back(&b, s, 2, ts, first);
   {
      rmp4_t *r = rmp4_open_memory(b.p, b.size);
      if (r)
      {
         const rmp4_track *tv = rmp4_get_track(r, 0), *ta = rmp4_get_track(r, 1);
         CHECK(tv && tv->codec == RMP4_CODEC_H264 && tv->width == 320 && tv->height == 240
               && tv->codec_private_size == sizeof(avcc)
               && !memcmp(tv->codec_private, avcc, sizeof(avcc)));
         CHECK(ta && ta->codec == RMP4_CODEC_AAC && ta->sample_rate == 44100
               && ta->channels == 2 && ta->codec_private_size == sizeof(asc)
               && !memcmp(ta->codec_private, asc, sizeof(asc)));
         /* MPEG-4 Audio by the esds; avc1 has none */
         CHECK(ta && ta->object_type == 0x40);
         CHECK(tv && tv->object_type == 0);
         rmp4_close(r);
      }
   }
   CHECK(first[0] == 0 && first[1] == 0);
   free(b.p);

   /* a write that fails fails the file */
   memset(&b, 0, sizeof(b));
   m = open_mux(&b, t, 2);
   CHECK(m != NULL);
   if (m)
   {
      b.fail_after = (int)b.writes + 2;
      CHECK(rmp4_mux_write(m, 0, samples[0], 50, 0, 0, true));
      CHECK(!rmp4_mux_write(m, 0, samples[1], 50, 3000, 3000, false));
      CHECK(!rmp4_mux_close(m));
   }
   free(b.p);

   /* tracks rmp4_mux cannot write */
   memset(&b, 0, sizeof(b));
   t[0].config_len = 0;               /* avc1 without avcC */
   CHECK(open_mux(&b, t, 2) == NULL);
   free(b.p);
}

int main(int argc, char **argv)
{
   const char *dir = argc > 1 ? argv[1] : NULL;

   test_mpeg4(dir);
   test_mjpeg(dir);
   test_avc_aac(dir);

   if (failures)
      printf("\n%u check(s) failed\n", failures);
   else
      printf("\nall checks passed\n");
   return failures ? 1 : 0;
}
