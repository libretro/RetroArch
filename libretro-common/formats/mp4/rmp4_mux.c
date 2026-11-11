/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rmp4_mux.c).
 * ---------------------------------------------------------------------------------------
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

/* MP4 writer. See rmp4_mux.h. Written from ISO/IEC 14496-12 (the
 * boxes), 14496-14 (esds) and 14496-15 (avcC), and QuickTime's sound
 * sample descriptions for PCM.
 *
 * Every box's size is worked out before it is written, from the counts
 * the tables hold, so the moov streams out through a small staging
 * buffer; the sizes are checked against what was written as each box
 * closes, and a mismatch fails the close. */

#include <stdlib.h>
#include <string.h>

#include <formats/rmp4_mux.h>

#define MUX_MOVIE_TIMESCALE 1000

typedef struct
{
   uint64_t offset;     /* in the file */
   uint32_t size;
   uint32_t count;      /* samples: 1, or PCM frames */
   int64_t  dts;
   int32_t  cto;        /* pts - dts */
   uint8_t  sync;
} mux_entry_t;

typedef struct
{
   rmp4_mux_track_t info;
   uint8_t         *config;
   mux_entry_t     *e;
   uint32_t         n;
   unsigned         frame_bytes;   /* PCM; 0 for the rest */

   /* worked out when closing */
   uint64_t         samples;
   uint64_t         duration;      /* track timescale */
   int64_t          media_start;   /* the edit list's media_time */
   uint64_t         pres_end;      /* the last frame's end, media time */
   uint32_t         stts_runs, ctts_runs, stss_n, stsc_runs;
   bool             has_ctts, co64;
} mux_track_t;

struct rmp4_mux
{
   struct rmp4_mux_io io;
   mux_track_t       *t;
   unsigned           count;
   uint64_t           pos;          /* bytes written */
   uint64_t           mdat_at;
   bool               failed;
   uint8_t            stage[4096];
   size_t             fill;
};

/* --------------------------------------------------------------------- */
/* Output                                                                */
/* --------------------------------------------------------------------- */

static void flush(rmp4_mux_t *m)
{
   if (m->fill && !m->failed && !m->io.write(m->io.user, m->stage, m->fill))
      m->failed = true;
   m->fill = 0;
}

static void put(rmp4_mux_t *m, const void *data, size_t len)
{
   const uint8_t *p = (const uint8_t*)data;
   m->pos += len;
   while (len)
   {
      size_t n = sizeof(m->stage) - m->fill;
      if (n > len)
         n = len;
      memcpy(m->stage + m->fill, p, n);
      m->fill += n;
      p       += n;
      len     -= n;
      if (m->fill == sizeof(m->stage))
         flush(m);
   }
}

static void put8(rmp4_mux_t *m, unsigned v)
{
   uint8_t b = (uint8_t)v;
   put(m, &b, 1);
}

static void put16(rmp4_mux_t *m, unsigned v)
{
   uint8_t b[2];
   b[0] = (uint8_t)(v >> 8);
   b[1] = (uint8_t)v;
   put(m, b, 2);
}

static void put24(rmp4_mux_t *m, uint32_t v)
{
   uint8_t b[3];
   b[0] = (uint8_t)(v >> 16);
   b[1] = (uint8_t)(v >> 8);
   b[2] = (uint8_t)v;
   put(m, b, 3);
}

static void put32(rmp4_mux_t *m, uint32_t v)
{
   uint8_t b[4];
   b[0] = (uint8_t)(v >> 24);
   b[1] = (uint8_t)(v >> 16);
   b[2] = (uint8_t)(v >> 8);
   b[3] = (uint8_t)v;
   put(m, b, 4);
}

static void put64(rmp4_mux_t *m, uint64_t v)
{
   put32(m, (uint32_t)(v >> 32));
   put32(m, (uint32_t)v);
}

static void put_zeros(rmp4_mux_t *m, size_t n)
{
   static const uint8_t z[32] = { 0 };
   while (n)
   {
      size_t k = n < sizeof(z) ? n : sizeof(z);
      put(m, z, k);
      n -= k;
   }
}

/* A box's header, and where it must end */
static uint64_t box(rmp4_mux_t *m, uint32_t size, const char *type)
{
   put32(m, size);
   put(m, type, 4);
   return m->pos - 8 + size;
}

static uint64_t full_box(rmp4_mux_t *m, uint32_t size, const char *type,
      unsigned version, uint32_t flags)
{
   uint64_t end = box(m, size, type);
   put8(m, version);
   put24(m, flags);
   return end;
}

static void box_end(rmp4_mux_t *m, uint64_t end)
{
   if (m->pos != end)
      m->failed = true;
}

/* --------------------------------------------------------------------- */
/* Tracks                                                                */
/* --------------------------------------------------------------------- */

static bool is_video(enum rmp4_mux_codec c)
{
   return c == RMP4_MUX_MPEG4_VIDEO || c == RMP4_MUX_H264 || c == RMP4_MUX_MJPEG;
}

static unsigned pcm_frame(const rmp4_mux_track_t *t)
{
   switch (t->codec)
   {
      case RMP4_MUX_PCM_S16LE:
      case RMP4_MUX_PCM_S16BE: return 2 * t->channels;
      case RMP4_MUX_PCM_F32BE: return 4 * t->channels;
      case RMP4_MUX_ULAW:      return t->channels;
      default:                 return 0;
   }
}

static const char *entry_type(enum rmp4_mux_codec c)
{
   switch (c)
   {
      case RMP4_MUX_H264:      return "avc1";
      case RMP4_MUX_AAC:       return "mp4a";
      case RMP4_MUX_PCM_S16LE: return "sowt";
      case RMP4_MUX_PCM_S16BE: return "twos";
      case RMP4_MUX_PCM_F32BE: return "fl32";
      case RMP4_MUX_ULAW:      return "ulaw";
      default:                 return "mp4v";
   }
}

static uint64_t movie_time(uint64_t v, uint32_t timescale)
{
   return timescale ? v * MUX_MOVIE_TIMESCALE / timescale : 0;
}

/* The tables' counts, the duration and the edit */
static void summarise(mux_track_t *t)
{
   uint32_t i;
   int64_t  first = t->n ? t->e[0].dts : 0;
   int64_t  min_pts = 0, max_pts = 0;
   uint64_t last_dur = 1;

   t->samples = 0;
   t->stts_runs = t->ctts_runs = t->stss_n = t->stsc_runs = 0;
   t->has_ctts = false;
   t->co64 = false;
   for (i = 0; i < t->n; i++)
   {
      const mux_entry_t *e = &t->e[i];
      int64_t pts = e->dts - first + e->cto;
      t->samples += e->count;
      if (e->offset > 0xFFFFFFFFu)
         t->co64 = true;
      if (!i || e->count != t->e[i - 1].count)
         t->stsc_runs++;
      if (!e->sync)
         t->stss_n++;            /* counted the other way round below */
      if (e->cto)
         t->has_ctts = true;
      if (!i || pts < min_pts)
         min_pts = pts;
      if (!i || pts > max_pts)
         max_pts = pts;
   }

   if (t->frame_bytes)
   {
      t->stts_runs = t->n ? 1 : 0;
      t->duration  = t->samples;
      t->pres_end  = t->samples;
      t->stss_n    = 0;
      t->media_start = t->info.skip < t->pres_end ? t->info.skip : 0;
      return;
   }

   /* sync samples: listed only if some sample is not one */
   if (t->stss_n)
   {
      t->stss_n = 0;
      for (i = 0; i < t->n; i++)
         t->stss_n += t->e[i].sync;
   }
   else
      t->stss_n = (uint32_t)-1;  /* all sync: no stss */

   for (i = 0; i < t->n; i++)
   {
      uint64_t d = i + 1 < t->n ? (uint64_t)(t->e[i + 1].dts - t->e[i].dts) : last_dur;
      if (i + 1 < t->n)
         last_dur = d;
      if (!i)
         t->stts_runs = 1;
      else
      {
         uint64_t prev = (uint64_t)(t->e[i].dts - t->e[i - 1].dts);
         if (d != prev)
            t->stts_runs++;
      }
      if (t->has_ctts && (!i || t->e[i].cto != t->e[i - 1].cto))
         t->ctts_runs++;
   }
   /* the last sample lasts as long as the one before it */
   t->duration    = t->n ? (uint64_t)(t->e[t->n - 1].dts - first) + last_dur : 0;
   t->media_start = min_pts > 0 ? min_pts : 0;
   t->pres_end    = t->n ? (uint64_t)max_pts + last_dur : 0;
   if (t->pres_end < t->duration)
      t->pres_end = t->duration;
   /* the priming, past the first sample shown, if the track has more */
   if ((uint64_t)t->media_start + t->info.skip < t->pres_end)
      t->media_start += t->info.skip;
}

/* What the track shows, in the movie's timescale: from the first frame
 * shown to the end of the last */
static uint64_t shown_duration(const mux_track_t *t)
{
   return movie_time(t->pres_end - (uint64_t)t->media_start, t->info.timescale);
}

static uint32_t descr_size(uint32_t content)
{
   return 5 + content;            /* tag, four length bytes */
}

static uint32_t esds_size(const mux_track_t *t)
{
   uint32_t dsi = t->info.config_len ? descr_size((uint32_t)t->info.config_len) : 0;
   uint32_t dec = descr_size(13 + dsi);
   return 12 + descr_size(3 + dec + descr_size(1));
}

static bool has_esds(const mux_track_t *t)
{
   return t->info.codec == RMP4_MUX_MPEG4_VIDEO || t->info.codec == RMP4_MUX_MJPEG
       || t->info.codec == RMP4_MUX_AAC;
}

static uint32_t entry_size(const mux_track_t *t)
{
   uint32_t child = has_esds(t) ? esds_size(t)
                  : t->info.codec == RMP4_MUX_H264 ? 8 + (uint32_t)t->info.config_len : 0;
   return (is_video(t->info.codec) ? 8 + 78 : 8 + 28) + child;
}

static uint32_t stbl_size(const mux_track_t *t)
{
   uint32_t s = 8;
   s += 16 + entry_size(t);                                          /* stsd */
   s += 16 + 8 * t->stts_runs;                                       /* stts */
   if (t->has_ctts)
      s += 16 + 8 * t->ctts_runs;                                    /* ctts */
   if (t->stss_n != (uint32_t)-1 && !t->frame_bytes)
      s += 16 + 4 * t->stss_n;                                       /* stss */
   s += 16 + 12 * t->stsc_runs;                                      /* stsc */
   s += 20 + (t->frame_bytes ? 0 : 4 * t->n);                        /* stsz */
   s += 16 + (t->co64 ? 8 : 4) * t->n;                               /* stco */
   return s;
}

static uint32_t minf_size(const mux_track_t *t)
{
   return 8 + (is_video(t->info.codec) ? 20 : 16) + 36 + stbl_size(t);
}

static uint32_t mdia_size(const mux_track_t *t)
{
   return 8 + 32 + 33 + minf_size(t);
}

static bool has_edit(const mux_track_t *t)
{
   return t->media_start > 0;
}

static uint32_t trak_size(const mux_track_t *t)
{
   return 8 + 92 + (has_edit(t) ? 36 : 0) + mdia_size(t);
}

static void put_descr(rmp4_mux_t *m, unsigned tag, uint32_t content)
{
   put8(m, tag);
   put8(m, 0x80 | ((content >> 21) & 0x7F));
   put8(m, 0x80 | ((content >> 14) & 0x7F));
   put8(m, 0x80 | ((content >> 7) & 0x7F));
   put8(m, content & 0x7F);
}

static void write_esds(rmp4_mux_t *m, const mux_track_t *t, unsigned id)
{
   uint32_t dsi = t->info.config_len ? descr_size((uint32_t)t->info.config_len) : 0;
   uint32_t dec = descr_size(13 + dsi);
   uint64_t end = full_box(m, esds_size(t), "esds", 0, 0);
   unsigned oti = t->info.codec == RMP4_MUX_AAC ? 0x40
                : t->info.codec == RMP4_MUX_MJPEG ? 0x6C : 0x20;

   put_descr(m, 3, 3 + dec + descr_size(1));
   put16(m, id);
   put8(m, 0);
   put_descr(m, 4, 13 + dsi);
   put8(m, oti);
   put8(m, ((t->info.codec == RMP4_MUX_AAC ? 5 : 4) << 2) | 1);
   put24(m, 0);                          /* bufferSizeDB */
   put32(m, t->info.bitrate);            /* max */
   put32(m, t->info.bitrate);            /* average */
   if (t->info.config_len)
   {
      put_descr(m, 5, (uint32_t)t->info.config_len);
      put(m, t->config, t->info.config_len);
   }
   put_descr(m, 6, 1);
   put8(m, 2);
   box_end(m, end);
}

static void write_entry(rmp4_mux_t *m, const mux_track_t *t, unsigned id)
{
   uint64_t end = box(m, entry_size(t), entry_type(t->info.codec));
   put_zeros(m, 6);
   put16(m, 1);                          /* data_reference_index */
   if (is_video(t->info.codec))
   {
      static const char name[32] = "\x07rmp4mux";
      put_zeros(m, 16);
      put16(m, t->info.width);
      put16(m, t->info.height);
      put32(m, 0x00480000);
      put32(m, 0x00480000);
      put32(m, 0);
      put16(m, 1);                       /* frame_count */
      put(m, name, 32);
      put16(m, 0x0018);
      put16(m, 0xFFFF);
   }
   else
   {
      unsigned bits = t->info.codec == RMP4_MUX_PCM_F32BE ? 32
                    : t->info.codec == RMP4_MUX_ULAW ? 8 : 16;
      put_zeros(m, 8);                   /* version, revision, vendor */
      put16(m, t->info.channels);
      put16(m, bits);
      put16(m, 0);
      put16(m, 0);
      put32(m, (t->info.sample_rate & 0xFFFF) << 16);
   }
   if (has_esds(t))
      write_esds(m, t, id);
   else if (t->info.codec == RMP4_MUX_H264)
   {
      uint64_t e2 = box(m, 8 + (uint32_t)t->info.config_len, "avcC");
      put(m, t->config, t->info.config_len);
      box_end(m, e2);
   }
   box_end(m, end);
}

static void write_stbl(rmp4_mux_t *m, const mux_track_t *t, unsigned id)
{
   uint64_t end = box(m, stbl_size(t), "stbl"), e2;
   uint32_t i;

   e2 = full_box(m, 16 + entry_size(t), "stsd", 0, 0);
   put32(m, 1);
   write_entry(m, t, id);
   box_end(m, e2);

   e2 = full_box(m, 16 + 8 * t->stts_runs, "stts", 0, 0);
   put32(m, t->stts_runs);
   if (t->frame_bytes)
   {
      if (t->n)
      {
         put32(m, (uint32_t)t->samples);
         put32(m, 1);
      }
   }
   else
   {
      uint32_t run = 0;
      uint64_t cur = 0, last = 1;
      for (i = 0; i < t->n; i++)
      {
         uint64_t d = i + 1 < t->n ? (uint64_t)(t->e[i + 1].dts - t->e[i].dts) : last;
         if (i + 1 < t->n)
            last = d;
         if (run && d != cur)
         {
            put32(m, run);
            put32(m, (uint32_t)cur);
            run = 0;
         }
         cur = d;
         run++;
      }
      if (run)
      {
         put32(m, run);
         put32(m, (uint32_t)cur);
      }
   }
   box_end(m, e2);

   if (t->has_ctts)
   {
      uint32_t run = 0;
      int32_t  cur = 0;
      e2 = full_box(m, 16 + 8 * t->ctts_runs, "ctts", 1, 0);
      put32(m, t->ctts_runs);
      for (i = 0; i < t->n; i++)
      {
         if (run && t->e[i].cto != cur)
         {
            put32(m, run);
            put32(m, (uint32_t)cur);
            run = 0;
         }
         cur = t->e[i].cto;
         run++;
      }
      if (run)
      {
         put32(m, run);
         put32(m, (uint32_t)cur);
      }
      box_end(m, e2);
   }

   if (t->stss_n != (uint32_t)-1 && !t->frame_bytes)
   {
      e2 = full_box(m, 16 + 4 * t->stss_n, "stss", 0, 0);
      put32(m, t->stss_n);
      for (i = 0; i < t->n; i++)
         if (t->e[i].sync)
            put32(m, i + 1);
      box_end(m, e2);
   }

   e2 = full_box(m, 16 + 12 * t->stsc_runs, "stsc", 0, 0);
   put32(m, t->stsc_runs);
   for (i = 0; i < t->n; i++)
      if (!i || t->e[i].count != t->e[i - 1].count)
      {
         put32(m, i + 1);
         put32(m, t->e[i].count);
         put32(m, 1);
      }
   box_end(m, e2);

   e2 = full_box(m, 20 + (t->frame_bytes ? 0 : 4 * t->n), "stsz", 0, 0);
   if (t->frame_bytes)
   {
      put32(m, t->frame_bytes);
      put32(m, (uint32_t)t->samples);
   }
   else
   {
      put32(m, 0);
      put32(m, t->n);
      for (i = 0; i < t->n; i++)
         put32(m, t->e[i].size);
   }
   box_end(m, e2);

   e2 = full_box(m, 16 + (t->co64 ? 8 : 4) * t->n, t->co64 ? "co64" : "stco", 0, 0);
   put32(m, t->n);
   for (i = 0; i < t->n; i++)
   {
      if (t->co64)
         put64(m, t->e[i].offset);
      else
         put32(m, (uint32_t)t->e[i].offset);
   }
   box_end(m, e2);
   box_end(m, end);
}

static void write_trak(rmp4_mux_t *m, const mux_track_t *t, unsigned id)
{
   bool     video = is_video(t->info.codec);
   uint64_t mdur  = shown_duration(t);
   uint64_t end   = box(m, trak_size(t), "trak"), e2, e3;

   e2 = full_box(m, 92, "tkhd", 0, 3);
   put32(m, 0);
   put32(m, 0);
   put32(m, id);
   put32(m, 0);
   put32(m, (uint32_t)mdur);
   put_zeros(m, 8);
   put16(m, 0);                          /* layer */
   put16(m, 0);                          /* alternate_group */
   put16(m, video ? 0 : 0x0100);         /* volume */
   put16(m, 0);
   put32(m, 0x00010000); put32(m, 0); put32(m, 0);
   put32(m, 0); put32(m, 0x00010000); put32(m, 0);
   put32(m, 0); put32(m, 0); put32(m, 0x40000000);
   put32(m, video ? t->info.width << 16 : 0);
   put32(m, video ? t->info.height << 16 : 0);
   box_end(m, e2);

   if (has_edit(t))
   {
      e2 = box(m, 36, "edts");
      e3 = full_box(m, 28, "elst", 0, 0);
      put32(m, 1);
      put32(m, (uint32_t)mdur);
      put32(m, (uint32_t)t->media_start);
      put32(m, 0x00010000);
      box_end(m, e3);
      box_end(m, e2);
   }

   e2 = box(m, mdia_size(t), "mdia");
   e3 = full_box(m, 32, "mdhd", 0, 0);
   put32(m, 0);
   put32(m, 0);
   put32(m, t->info.timescale);
   put32(m, (uint32_t)t->duration);
   put16(m, 0x55C4);                     /* "und" */
   put16(m, 0);
   box_end(m, e3);

   e3 = full_box(m, 33, "hdlr", 0, 0);
   put32(m, 0);
   put(m, video ? "vide" : "soun", 4);
   put_zeros(m, 12);
   put8(m, 0);                           /* empty name */
   box_end(m, e3);

   {
      uint64_t e4 = box(m, minf_size(t), "minf"), e5;
      if (video)
      {
         e5 = full_box(m, 20, "vmhd", 0, 1);
         put_zeros(m, 8);
      }
      else
      {
         e5 = full_box(m, 16, "smhd", 0, 0);
         put_zeros(m, 4);
      }
      box_end(m, e5);
      e5 = box(m, 36, "dinf");
      {
         uint64_t e6 = full_box(m, 28, "dref", 0, 0), e7;
         put32(m, 1);
         e7 = full_box(m, 12, "url ", 0, 1);
         box_end(m, e7);
         box_end(m, e6);
      }
      box_end(m, e5);
      write_stbl(m, t, id);
      box_end(m, e4);
   }
   box_end(m, e2);
   box_end(m, end);
}

/* --------------------------------------------------------------------- */

static void free_mux(rmp4_mux_t *m)
{
   unsigned i;
   if (!m)
      return;
   if (m->t)
      for (i = 0; i < m->count; i++)
      {
         free(m->t[i].config);
         free(m->t[i].e);
      }
   free(m->t);
   free(m);
}

rmp4_mux_t *rmp4_mux_open(const struct rmp4_mux_io *io,
      const rmp4_mux_track_t *tracks, unsigned count)
{
   rmp4_mux_t *m;
   unsigned    i;

   if (!io || !io->write || !io->seek || !tracks || !count || count > 32)
      return NULL;
   if (!(m = (rmp4_mux_t*)calloc(1, sizeof(*m))))
      return NULL;
   m->io    = *io;
   m->count = count;
   if (!(m->t = (mux_track_t*)calloc(count, sizeof(*m->t))))
      goto fail;

   for (i = 0; i < count; i++)
   {
      mux_track_t *t = &m->t[i];
      t->info = tracks[i];
      if (t->info.codec > RMP4_MUX_ULAW || !t->info.timescale || !t->info.max_samples)
         goto fail;
      if (is_video(t->info.codec) ? (!t->info.width || !t->info.height
               || t->info.width > 0xFFFF || t->info.height > 0xFFFF)
            : (!t->info.channels || !t->info.sample_rate || t->info.sample_rate > 0xFFFF))
         goto fail;
      if (t->info.codec == RMP4_MUX_H264 && !t->info.config_len)
         goto fail;
      t->frame_bytes = pcm_frame(&t->info);
      if (t->info.config_len)
      {
         if (!t->info.config || t->info.config_len > 0xFFFFFF
               || !(t->config = (uint8_t*)malloc(t->info.config_len)))
            goto fail;
         memcpy(t->config, t->info.config, t->info.config_len);
      }
      if (!(t->e = (mux_entry_t*)malloc((size_t)t->info.max_samples * sizeof(*t->e))))
         goto fail;
   }

   /* ftyp, then an mdat with a 64-bit size given when closing */
   {
      uint64_t end = box(m, 28, "ftyp");
      put(m, "isom", 4);
      put32(m, 0x200);
      put(m, "isomiso2mp41", 12);
      box_end(m, end);
   }
   m->mdat_at = m->pos;
   put32(m, 1);
   put(m, "mdat", 4);
   put64(m, 0);
   flush(m);
   if (m->failed)
      goto fail;
   return m;

fail:
   free_mux(m);
   return NULL;
}

bool rmp4_mux_write(rmp4_mux_t *m, unsigned track, const void *data,
      size_t len, int64_t pts, int64_t dts, bool sync)
{
   mux_track_t *t;
   mux_entry_t *e;
   int64_t      cto;

   if (!m || m->failed || track >= m->count || !data || !len || len > 0xFFFFFFFFu)
      return false;
   t = &m->t[track];
   if (t->n >= t->info.max_samples)
      return false;
   if (t->n && dts <= t->e[t->n - 1].dts)
      return false;
   cto = t->frame_bytes ? 0 : pts - dts;
   if (cto < 0 || cto > 0x7FFFFFFF)
      return false;
   if (t->frame_bytes && len % t->frame_bytes)
      return false;

   if (!m->io.write(m->io.user, data, len))
   {
      m->failed = true;
      return false;
   }
   e         = &t->e[t->n++];
   e->offset = m->pos;
   e->size   = (uint32_t)len;
   e->count  = t->frame_bytes ? (uint32_t)(len / t->frame_bytes) : 1;
   e->dts    = t->frame_bytes ? pts : dts;
   e->cto    = (int32_t)cto;
   e->sync   = (uint8_t)(t->frame_bytes || sync);
   m->pos   += len;
   return true;
}

bool rmp4_mux_close(rmp4_mux_t *m)
{
   uint64_t mdat_end, movie = 0, end;
   unsigned i;
   uint32_t size = 8 + 108;
   bool     ok;

   if (!m)
      return false;
   mdat_end = m->pos;
   for (i = 0; i < m->count; i++)
   {
      mux_track_t *t = &m->t[i];
      uint64_t     d;
      summarise(t);
      d = shown_duration(t);
      if (d > movie)
         movie = d;
      size += trak_size(t);
   }

   end = box(m, size, "moov");
   {
      uint64_t e2 = full_box(m, 108, "mvhd", 0, 0);
      put32(m, 0);
      put32(m, 0);
      put32(m, MUX_MOVIE_TIMESCALE);
      put32(m, (uint32_t)movie);
      put32(m, 0x00010000);              /* rate */
      put16(m, 0x0100);                  /* volume */
      put_zeros(m, 10);
      put32(m, 0x00010000); put32(m, 0); put32(m, 0);
      put32(m, 0); put32(m, 0x00010000); put32(m, 0);
      put32(m, 0); put32(m, 0); put32(m, 0x40000000);
      put_zeros(m, 24);
      put32(m, m->count + 1);            /* next_track_ID */
      box_end(m, e2);
   }
   for (i = 0; i < m->count; i++)
      write_trak(m, &m->t[i], i + 1);
   box_end(m, end);
   flush(m);

   /* the mdat's size, in its largesize field */
   if (!m->failed)
   {
      uint8_t  b[8];
      uint64_t s = mdat_end - m->mdat_at;
      unsigned k;
      for (k = 0; k < 8; k++)
         b[k] = (uint8_t)(s >> (56 - 8 * k));
      if (!m->io.seek(m->io.user, m->mdat_at + 8) || !m->io.write(m->io.user, b, 8))
         m->failed = true;
   }
   ok = !m->failed;
   free_mux(m);
   return ok;
}
