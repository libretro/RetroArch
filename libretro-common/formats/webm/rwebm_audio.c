/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rwebm_audio.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
 * LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Decodes the first supported audio track of a WebM file to interleaved
 * s16 PCM, for the animated-thumbnail preview audio feature.
 *
 * Opus (A_OPUS) goes straight through ropus: the CodecPrivate is the
 * OpusHead, packets decode independently, pre-skip is dropped from the
 * start of the stream and DiscardPadding from the end of the packets
 * that carry it.
 *
 * Vorbis (A_VORBIS) has no packet-level decoder here, so the packets
 * are wrapped into a well-formed in-memory Ogg stream (one packet per
 * page, correct CRCs, granule positions derived from the container
 * timestamps) and handed to rvorbis. Granules only influence edge
 * trimming in the puller, so container-derived values are exact enough
 * for preview purposes; the final page's granule is given a small
 * surplus so the decoder never under-delivers the tail. */

#include <stdlib.h>
#include <string.h>

#include <formats/rwebm.h>
#include <formats/rwebm_audio.h>

#ifdef HAVE_ROPUS
#include <formats/ropus.h>
#endif
#ifdef HAVE_RVORBIS
#include <formats/rvorbis.h>
#endif

/* ==================================================================== */
/* Growing PCM accumulator                                              */
/* ==================================================================== */

typedef struct
{
   int16_t *data;
   size_t   frames;     /* per-channel                                  */
   size_t   cap_frames; /* allocation size                              */
   size_t   max_frames; /* hard cap from max_ms (0 = none)              */
   unsigned channels;
} rwebm_pcm_acc;

static int rwebm_pcm_reserve(rwebm_pcm_acc *a, size_t add_frames)
{
   size_t need = a->frames + add_frames;
   if (need <= a->cap_frames)
      return 1;
   {
      size_t ncap = a->cap_frames ? a->cap_frames : 48000;
      int16_t *nd;
      while (ncap < need)
         ncap += ncap / 2;
      nd = (int16_t*)realloc(a->data,
            ncap * a->channels * sizeof(int16_t));
      if (!nd)
         return 0;
      a->data       = nd;
      a->cap_frames = ncap;
   }
   return 1;
}

/* ==================================================================== */
/* Opus                                                                 */
/* ==================================================================== */

#ifdef HAVE_ROPUS
/* Maximum frames one Opus packet can produce: 120 ms at 48 kHz. */
#define RWEBM_OPUS_MAX_FRAMES 5760

static int rwebm_audio_decode_opus(rwebm_t *m, const rwebm_track *t,
      int track_idx, rwebm_pcm_acc *a, unsigned *rate)
{
   ropus_t *o = ropus_open(t->codec_private, t->codec_private_size);
   size_t   skip;
   rwebm_packet pkt;

   if (!o)
      return 0;

   a->channels = ropus_channels(o);
   *rate       = 48000;
   skip        = ropus_preskip(o);

   while (rwebm_read_packet(m, &pkt) == 1)
   {
      int produced;
      if (pkt.track != track_idx)
         continue;
      if (!rwebm_pcm_reserve(a, RWEBM_OPUS_MAX_FRAMES))
         break;
      produced = ropus_decode_s16(o, pkt.data, pkt.size,
            a->data + a->frames * a->channels);
      if (produced < 0)
         break;                    /* malformed packet: keep what we have */
      if (pkt.discard_padding > 0)
      {
         int64_t drop = (pkt.discard_padding * 48000 + 999999999)
               / 1000000000;
         if (drop > produced)
            drop = produced;
         produced -= (int)drop;
      }
      if (skip)
      {
         size_t s = ((size_t)produced < skip) ? (size_t)produced : skip;
         if (s)
            memmove(a->data + a->frames * a->channels,
                  a->data + (a->frames + s) * a->channels,
                  ((size_t)produced - s) * a->channels * sizeof(int16_t));
         produced -= (int)s;
         skip     -= s;
      }
      a->frames += (size_t)produced;
      if (a->max_frames && a->frames >= a->max_frames)
      {
         a->frames = a->max_frames;
         break;
      }
   }
   ropus_close(o);
   return a->frames > 0;
}
#endif /* HAVE_ROPUS */

/* ==================================================================== */
/* Vorbis: in-memory Ogg synthesis feeding rvorbis                      */
/* ==================================================================== */

#ifdef HAVE_RVORBIS
static uint32_t rwebm_ogg_crc_table[256];
static int      rwebm_ogg_crc_ready = 0;

static void rwebm_ogg_crc_init(void)
{
   unsigned i, j;
   if (rwebm_ogg_crc_ready)
      return;
   for (i = 0; i < 256; i++)
   {
      uint32_t r = (uint32_t)i << 24;
      for (j = 0; j < 8; j++)
         r = (r << 1) ^ ((r & 0x80000000u) ? 0x04c11db7u : 0);
      rwebm_ogg_crc_table[i] = r;
   }
   rwebm_ogg_crc_ready = 1;
}

typedef struct
{
   uint8_t *data;
   size_t   size;
   size_t   cap;
   uint32_t serial;
   uint32_t seq;
} rwebm_ogg;

static int rwebm_ogg_reserve(rwebm_ogg *g, size_t add)
{
   if (g->size + add <= g->cap)
      return 1;
   {
      size_t ncap = g->cap ? g->cap : 65536;
      uint8_t *nd;
      while (ncap < g->size + add)
         ncap += ncap / 2;
      if (!(nd = (uint8_t*)realloc(g->data, ncap)))
         return 0;
      g->data = nd;
      g->cap  = ncap;
   }
   return 1;
}

/* Append one packet as one Ogg page. Packets of 255*255 bytes and up
 * would need continued pages; Vorbis packets never approach that, so
 * they are rejected instead. */
static int rwebm_ogg_page(rwebm_ogg *g, const uint8_t *pkt, size_t len,
      uint64_t granule, int bos, int eos)
{
   size_t   nseg = len / 255 + 1;
   size_t   head = 27 + nseg;
   size_t   page_off = g->size;
   uint8_t *p;
   size_t   k;
   uint32_t crc = 0;

   if (nseg > 255)
      return 0;
   if (!rwebm_ogg_reserve(g, head + len))
      return 0;

   p = g->data + page_off;
   memcpy(p, "OggS", 4);
   p[4] = 0;                                   /* stream structure v0   */
   p[5] = (uint8_t)((bos ? 2 : 0) | (eos ? 4 : 0));
   for (k = 0; k < 8; k++)
      p[6 + k]  = (uint8_t)(granule >> (8 * k));
   for (k = 0; k < 4; k++)
      p[14 + k] = (uint8_t)(g->serial >> (8 * k));
   for (k = 0; k < 4; k++)
      p[18 + k] = (uint8_t)(g->seq >> (8 * k));
   p[22] = p[23] = p[24] = p[25] = 0;          /* CRC placeholder       */
   p[26] = (uint8_t)nseg;
   for (k = 0; k + 1 < nseg; k++)
      p[27 + k] = 255;
   p[27 + nseg - 1] = (uint8_t)(len % 255);
   memcpy(p + head, pkt, len);
   g->size += head + len;
   g->seq++;

   for (k = 0; k < head + len; k++)
      crc = (crc << 8) ^ rwebm_ogg_crc_table[(uint8_t)(crc >> 24) ^ p[k]];
   for (k = 0; k < 4; k++)
      p[22 + k] = (uint8_t)(crc >> (8 * k));
   return 1;
}

/* Split a Xiph-laced CodecPrivate into its three Vorbis headers. */
static int rwebm_xiph_split(const uint8_t *cp, size_t cps,
      const uint8_t *out[3], size_t out_len[3])
{
   size_t pos = 0, i;
   size_t lens[3];
   if (!cp || cps < 3 || cp[0] != 2)
      return 0;
   pos = 1;
   for (i = 0; i < 2; i++)
   {
      size_t l = 0;
      while (pos < cps && cp[pos] == 255)
      {
         l += 255;
         pos++;
      }
      if (pos >= cps)
         return 0;
      l += cp[pos++];
      lens[i] = l;
   }
   if (pos + lens[0] + lens[1] > cps)
      return 0;
   lens[2]    = cps - pos - lens[0] - lens[1];
   out[0]     = cp + pos;
   out[1]     = out[0] + lens[0];
   out[2]     = out[1] + lens[1];
   out_len[0] = lens[0];
   out_len[1] = lens[1];
   out_len[2] = lens[2];
   return 1;
}

static int rwebm_audio_decode_vorbis(rwebm_t *m, const rwebm_track *t,
      int track_idx, rwebm_pcm_acc *a, unsigned *rate)
{
   const uint8_t *hdr[3];
   size_t         hdr_len[3];
   rwebm_ogg      g;
   rwebm_packet   pkt;
   rwebm_packet   pend;
   int            have_pend = 0;
   int            ok        = 0;
   unsigned       srate     = t->sample_rate ? t->sample_rate : 48000;
   uint64_t       dur_ns    = (uint64_t)rwebm_duration_ns(m);
   rvorbis       *v         = NULL;

   if (!rwebm_xiph_split(t->codec_private, t->codec_private_size,
         hdr, hdr_len))
      return 0;

   rwebm_ogg_crc_init();
   memset(&g, 0, sizeof(g));
   g.serial = 0x52415741;   /* arbitrary but fixed */

   if (!rwebm_ogg_page(&g, hdr[0], hdr_len[0], 0, 1, 0)) goto out;
   if (!rwebm_ogg_page(&g, hdr[1], hdr_len[1], 0, 0, 0)) goto out;
   if (!rwebm_ogg_page(&g, hdr[2], hdr_len[2], 0, 0, 0)) goto out;

   /* One packet per page. A page's granule is the stream position at
    * its end, i.e. the next packet's start; derive both from container
    * timestamps, and give the final page a small surplus so the
    * decoder's end trim never under-delivers. */
   while (rwebm_read_packet(m, &pkt) == 1)
   {
      if (pkt.track != track_idx)
         continue;
      if (have_pend)
      {
         uint64_t gran = ((uint64_t)pkt.timestamp * srate + 500000000)
               / 1000000000;
         if (!rwebm_ogg_page(&g, pend.data, pend.size, gran, 0, 0))
            goto out;
      }
      pend      = pkt;
      have_pend = 1;
   }
   if (have_pend)
   {
      uint64_t gran = (dur_ns * srate + 500000000) / 1000000000 + 8192;
      if (!rwebm_ogg_page(&g, pend.data, pend.size, gran, 0, 1))
         goto out;
   }
   else
      goto out;

   /* Decode the synthesized stream. */
   {
      int err = 0;
      v = rvorbis_open_memory(g.data, (int)g.size, &err, NULL);
      if (!v)
         goto out;
   }
   {
      rvorbis_info info = rvorbis_get_info(v);
      if (info.channels < 1 || info.channels > 2)
         goto out;
      a->channels = (unsigned)info.channels;
      *rate       = info.sample_rate;
   }
   for (;;)
   {
      int got;
      size_t chunk = 8192;
      if (a->max_frames)
      {
         if (a->frames >= a->max_frames)
            break;
         if (a->max_frames - a->frames < chunk)
            chunk = a->max_frames - a->frames;
      }
      if (!rwebm_pcm_reserve(a, chunk))
         break;
      got = rvorbis_get_samples_s16_interleaved(v, (int)a->channels,
            a->data + a->frames * a->channels,
            (int)(chunk * a->channels));
      if (got <= 0)
         break;
      a->frames += (size_t)got;
   }
   ok = a->frames > 0;
out:
   if (v)
      rvorbis_close(v);
   free(g.data);
   return ok;
}
#endif /* HAVE_RVORBIS */

/* ==================================================================== */
/* Public entry points                                                  */
/* ==================================================================== */

int rwebm_audio_decode(const void *buf, size_t len, int64_t max_ms,
      int16_t **pcm, size_t *frames, unsigned *rate, unsigned *channels)
{
   rwebm_t      *m;
   rwebm_pcm_acc a;
   int           i, ok = 0, track_idx = -1;
   const rwebm_track *t = NULL;

   *pcm = NULL;
   *frames = 0;
   *rate = 0;
   *channels = 0;

   if (!(m = rwebm_open_memory((const uint8_t*)buf, len)))
      return 0;

   for (i = 0; i < rwebm_num_tracks(m); i++)
   {
      const rwebm_track *c = rwebm_get_track(m, i);
      if (!c || c->type != RWEBM_TRACK_AUDIO)
         continue;
      if (c->codec == RWEBM_CODEC_OPUS || c->codec == RWEBM_CODEC_VORBIS)
      {
         t         = c;
         track_idx = i;
         break;
      }
   }
   if (!t)
      goto out;

   memset(&a, 0, sizeof(a));
   if (max_ms > 0)
   {
      unsigned r = (t->codec == RWEBM_CODEC_OPUS)
            ? 48000 : (t->sample_rate ? t->sample_rate : 48000);
      a.max_frames = (size_t)((uint64_t)max_ms * r / 1000);
   }

   switch (t->codec)
   {
#ifdef HAVE_ROPUS
      case RWEBM_CODEC_OPUS:
         ok = rwebm_audio_decode_opus(m, t, track_idx, &a, rate);
         break;
#endif
#ifdef HAVE_RVORBIS
      case RWEBM_CODEC_VORBIS:
         ok = rwebm_audio_decode_vorbis(m, t, track_idx, &a, rate);
         break;
#endif
      default:
         break;
   }

   if (ok)
   {
      *pcm      = a.data;
      *frames   = a.frames;
      *channels = a.channels;
   }
   else
      free(a.data);
out:
   rwebm_close(m);
   return ok;
}

int rwebm_audio_decode_wav(const void *buf, size_t len, int64_t max_ms,
      void **wav, size_t *wav_size)
{
   int16_t *pcm = NULL;
   size_t   frames = 0;
   unsigned rate = 0, channels = 0;
   size_t   data_size, total;
   uint8_t *w;
   uint32_t v32;
   uint16_t v16;

   *wav = NULL;
   *wav_size = 0;

   if (!rwebm_audio_decode(buf, len, max_ms, &pcm, &frames,
         &rate, &channels))
      return 0;

   data_size = frames * channels * sizeof(int16_t);
   total     = 44 + data_size;
   if (!(w = (uint8_t*)malloc(total)))
   {
      free(pcm);
      return 0;
   }

#define RWEBM_W32(off, val) \
   do { v32 = (uint32_t)(val); memcpy(w + (off), &v32, 4); } while (0)
#define RWEBM_W16(off, val) \
   do { v16 = (uint16_t)(val); memcpy(w + (off), &v16, 2); } while (0)

   /* RIFF/WAVE, PCM s16le. WAV is little-endian by definition; on
    * big-endian hosts both the header fields and the samples are
    * byte-swapped into place. */
   memcpy(w, "RIFF", 4);
   RWEBM_W32(4, 36 + data_size);
   memcpy(w + 8, "WAVEfmt ", 8);
   RWEBM_W32(16, 16);
   RWEBM_W16(20, 1);
   RWEBM_W16(22, channels);
   RWEBM_W32(24, rate);
   RWEBM_W32(28, (uint32_t)rate * channels * 2);
   RWEBM_W16(32, channels * 2);
   RWEBM_W16(34, 16);
   memcpy(w + 36, "data", 4);
   RWEBM_W32(40, data_size);
#if defined(MSB_FIRST)
   {
      size_t k, n = frames * channels;
      /* header fields were stored via memcpy of native words: redo them
       * little-endian, then swap the samples */
      static const int off32[] = { 4, 16, 24, 28, 40 };
      static const int off16[] = { 20, 22, 32, 34 };
      size_t oi;
      for (oi = 0; oi < sizeof(off32)/sizeof(off32[0]); oi++)
      {
         uint8_t *q = w + off32[oi], tmp;
         tmp = q[0]; q[0] = q[3]; q[3] = tmp;
         tmp = q[1]; q[1] = q[2]; q[2] = tmp;
      }
      for (oi = 0; oi < sizeof(off16)/sizeof(off16[0]); oi++)
      {
         uint8_t *q = w + off16[oi], tmp;
         tmp = q[0]; q[0] = q[1]; q[1] = tmp;
      }
      for (k = 0; k < n; k++)
      {
         uint16_t s = (uint16_t)pcm[k];
         w[44 + 2*k]     = (uint8_t)(s & 0xFF);
         w[44 + 2*k + 1] = (uint8_t)(s >> 8);
      }
   }
#else
   memcpy(w + 44, pcm, data_size);
#endif
#undef RWEBM_W32
#undef RWEBM_W16

   free(pcm);
   *wav      = w;
   *wav_size = total;
   return 1;
}
