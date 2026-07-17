/* rmp4 -- MP4 (ISO Base Media File Format) demuxer for libretro-common.
 *
 * A pure demuxer: it parses the ISO-BMFF box tree and hands out the
 * elementary-stream packets, without decoding.  See rmp4.h for the API.
 *
 * Scope: progressive (non-fragmented) files whose sample tables live in
 * moov/trak/mdia/minf/stbl.  The parser reads the boxes it needs (mvhd
 * for the movie timescale and duration, per-trak mdhd/hdlr for the
 * track timescale and kind, stsd for the codec and its setup data, and
 * the stts/ctts/stss/stsz/stsc/stco|co64 tables to place every sample)
 * and then streams the samples out in file order.
 *
 * The whole input is supplied up front and borrowed, not copied;
 * returned packet pointers alias into it.  Opus setup data is the one
 * exception: the dOps payload is repacked into an OpusHead held by the
 * demuxer (see rmp4.h).
 *
 * SPDX-License-Identifier: MIT  (RetroArch libretro-common)
 */
#include <stdlib.h>
#include <string.h>

#include <formats/rmp4.h>

#define RMP4_MAX_TRACKS       8
/* Hard cap on samples stored per track: bounds the table memory a
 * hostile file can demand (24 bytes per sample). */
#define RMP4_MAX_SAMPLES      262144

/* Repacked OpusHead: 19-byte fixed part + up to 2 + 8 mapping bytes. */
#define RMP4_OPUS_HEAD_MAX    32
#define RMP4_OPUS_MAX_CH      8

/* ===== byte readers (all big-endian, as ISO-BMFF stores them) ===== */

static uint32_t rmp4_be16(const uint8_t *p)
{
   return ((uint32_t)p[0] << 8) | p[1];
}

static uint32_t rmp4_be32(const uint8_t *p)
{
   return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
        | ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static uint64_t rmp4_be64(const uint8_t *p)
{
   return ((uint64_t)rmp4_be32(p) << 32) | rmp4_be32(p + 4);
}

/* ===== box walker ===== */

typedef struct
{
   const uint8_t *body;  /* first byte of the box payload            */
   uint64_t       size;  /* payload size                             */
   uint32_t       type;  /* fourcc                                   */
} rmp4_box;

/* Read the box starting at 'pos' inside [base, base+len). On success
 * fills *box, stores the offset of the following box in *next and
 * returns 1; returns 0 on truncation or a malformed header. */
static int rmp4_box_at(const uint8_t *base, uint64_t len, uint64_t pos,
      rmp4_box *box, uint64_t *next)
{
   uint64_t size, hdr = 8;
   if (pos + 8 > len)
      return 0;
   size      = rmp4_be32(base + pos);
   box->type = rmp4_be32(base + pos + 4);
   if (size == 1)
   {
      if (pos + 16 > len)
         return 0;
      size = rmp4_be64(base + pos + 8);
      hdr  = 16;
   }
   else if (size == 0)
      size = len - pos;                    /* extends to end of file   */
   if (size < hdr || size > len - pos)
      return 0;
   box->body = base + pos + hdr;
   box->size = size - hdr;
   *next     = pos + size;
   return 1;
}

/* Find the first child box of the given fourcc inside a parent payload.
 * Returns 1 and fills *out when found. */
static int rmp4_find_child(const uint8_t *body, uint64_t size,
      uint32_t fourcc, rmp4_box *out)
{
   uint64_t pos = 0, next;
   rmp4_box b;
   while (pos < size && rmp4_box_at(body, size, pos, &b, &next))
   {
      if (b.type == fourcc)
      {
         *out = b;
         return 1;
      }
      if (next <= pos)
         break;
      pos = next;
   }
   return 0;
}

#define RMP4_FOURCC(a,b,c,d) \
   (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) \
  | ((uint32_t)(c) << 8)  |  (uint32_t)(d))

/* ===== per-track state ===== */

typedef struct
{
   rmp4_track  pub;
   uint32_t    timescale;      /* mdhd media timescale               */
   /* Per-sample tables (parallel arrays, 'count' entries). */
   uint64_t   *off;
   uint32_t   *size;
   int64_t    *pts;            /* nanoseconds                        */
   uint8_t    *key;
   uint32_t    count;
   uint32_t    cursor;
   uint8_t     opus_head[RMP4_OPUS_HEAD_MAX];
} rmp4_itrack;

struct rmp4
{
   const uint8_t *buf;
   size_t         len;
   rmp4_itrack    trk[RMP4_MAX_TRACKS];
   int            num_tracks;
   uint32_t       movie_timescale;
   uint64_t       movie_duration;   /* in movie timescale ticks       */
};

/* ticks (may be negative via ctts v1) -> nanoseconds, without overflow:
 * timescale fits 32 bits, so the remainder term stays below 2^62. */
static int64_t rmp4_ticks_to_ns(int64_t ticks, uint32_t timescale)
{
   uint64_t t, q, r;
   int      neg = 0;
   if (!timescale)
      return 0;
   if (ticks < 0)
   {
      neg = 1;
      t   = (uint64_t)(-ticks);
   }
   else
      t = (uint64_t)ticks;
   q = t / timescale;
   r = t % timescale;
   t = q * 1000000000u + (r * 1000000000u) / timescale;
   return neg ? -(int64_t)t : (int64_t)t;
}

/* ===== esds descriptor parsing (for mp4a sample entries) ===== */

/* Expandable length: 1-4 bytes, 7 bits each, MSB is a continuation. */
static int rmp4_desc_len(const uint8_t *p, uint64_t avail,
      uint64_t *len, uint64_t *used)
{
   uint64_t v = 0, i;
   for (i = 0; i < 4 && i < avail; i++)
   {
      v = (v << 7) | (p[i] & 0x7F);
      if (!(p[i] & 0x80))
      {
         *len  = v;
         *used = i + 1;
         return 1;
      }
   }
   return 0;
}

/* Walk the esds payload (after the FullBox header) down to the
 * DecoderSpecificInfo. Returns the objectTypeIndication (0 on failure)
 * and, when present, the specific-info pointer/size. */
static unsigned rmp4_parse_esds(const uint8_t *p, uint64_t size,
      const uint8_t **dsi, uint64_t *dsi_size)
{
   uint64_t len, used, pos = 0;
   unsigned oti = 0;

   *dsi      = NULL;
   *dsi_size = 0;

   /* ES_Descriptor (tag 0x03) */
   if (pos >= size || p[pos] != 0x03)
      return 0;
   pos++;
   if (!rmp4_desc_len(p + pos, size - pos, &len, &used))
      return 0;
   pos += used;
   if (pos + 3 > size)
      return 0;
   pos += 2;                                 /* ES_ID                  */
   {
      uint8_t flags = p[pos++];
      if (flags & 0x80)                      /* streamDependenceFlag   */
         pos += 2;
      if (flags & 0x40)                      /* URL_Flag               */
      {
         if (pos >= size)
            return 0;
         pos += 1 + p[pos];
      }
      if (flags & 0x20)                      /* OCRstreamFlag          */
         pos += 2;
   }

   /* DecoderConfigDescriptor (tag 0x04) */
   if (pos >= size || p[pos] != 0x04)
      return 0;
   pos++;
   if (!rmp4_desc_len(p + pos, size - pos, &len, &used))
      return 0;
   pos += used;
   if (pos + 13 > size)
      return 0;
   oti  = p[pos];
   pos += 13;              /* OTI + streamType/buffer + bitrates      */

   /* DecoderSpecificInfo (tag 0x05), optional */
   if (pos < size && p[pos] == 0x05)
   {
      pos++;
      if (!rmp4_desc_len(p + pos, size - pos, &len, &used))
         return oti;
      pos += used;
      if (len && pos + len <= size)
      {
         *dsi      = p + pos;
         *dsi_size = len;
      }
   }
   return oti;
}

/* ===== stsd sample-entry parsing ===== */

/* Repack a dOps payload into the OpusHead layout ropus expects.
 * dOps: Version(1) OutputChannelCount(1) PreSkip(be16)
 *       InputSampleRate(be32) OutputGain(be16) ChannelMappingFamily(1)
 *       [StreamCount(1) CoupledCount(1) ChannelMapping(ch)]
 * OpusHead: "OpusHead" ver=1 ch preskip(le16) rate(le32) gain(le16)
 *       family [streams coupled mapping] */
static int rmp4_build_opus_head(rmp4_itrack *t,
      const uint8_t *d, uint64_t size)
{
   unsigned ch, family;
   uint8_t *h = t->opus_head;
   if (size < 11 || d[0] != 0)
      return 0;
   ch     = d[1];
   family = d[10];
   if (ch < 1 || ch > RMP4_OPUS_MAX_CH)
      return 0;
   memcpy(h, "OpusHead", 8);
   h[8]  = 1;
   h[9]  = (uint8_t)ch;
   h[10] = d[3];  h[11] = d[2];                    /* PreSkip -> LE    */
   h[12] = d[7];  h[13] = d[6];                    /* rate    -> LE    */
   h[14] = d[5];  h[15] = d[4];
   h[16] = d[9];  h[17] = d[8];                    /* gain    -> LE    */
   h[18] = (uint8_t)family;
   if (family == 0)
   {
      t->pub.codec_private_size = 19;
   }
   else
   {
      if (size < 13 + ch)
         return 0;
      h[19] = d[11];                               /* StreamCount      */
      h[20] = d[12];                               /* CoupledCount     */
      memcpy(h + 21, d + 13, ch);
      t->pub.codec_private_size = 21 + ch;
   }
   t->pub.codec_private = t->opus_head;
   return 1;
}

static void rmp4_store_fourcc(char *dst, size_t cap, uint32_t fourcc)
{
   if (cap < 5)
      return;
   dst[0] = (char)(fourcc >> 24);
   dst[1] = (char)(fourcc >> 16);
   dst[2] = (char)(fourcc >> 8);
   dst[3] = (char)fourcc;
   dst[4] = '\0';
}

/* First sample entry of the stsd box. Fills the codec fields of the
 * track.  Unknown codecs still record their fourcc so callers can see
 * what was skipped. */
static void rmp4_parse_stsd(rmp4_itrack *t, const uint8_t *p, uint64_t size)
{
   uint64_t next;
   rmp4_box e;
   if (size < 8)
      return;
   /* FullBox header + entry_count, then the first sample entry. */
   if (!rmp4_box_at(p, size, 8, &e, &next))
      return;
   rmp4_store_fourcc(t->pub.codec_id, sizeof(t->pub.codec_id), e.type);

   if (t->pub.type == RMP4_TRACK_VIDEO)
   {
      /* VisualSampleEntry: 8 (reserved+dri) + 16 pre_defined/reserved,
       * width/height at 24/26, children after the 78-byte prefix. */
      if (e.size >= 28)
      {
         t->pub.width  = rmp4_be16(e.body + 24);
         t->pub.height = rmp4_be16(e.body + 26);
      }
      switch (e.type)
      {
         case RMP4_FOURCC('v','p','0','8'):
            t->pub.codec = RMP4_CODEC_VP8;
            break;
         case RMP4_FOURCC('v','p','0','9'):
            t->pub.codec = RMP4_CODEC_VP9;
            break;
         default:
            break;
      }
      /* vpcC codecIntializationData is empty for VP8/VP9, matching the
       * empty CodecPrivate WebM carries; nothing further to extract. */
   }
   else if (t->pub.type == RMP4_TRACK_AUDIO)
   {
      uint64_t child_off = 28;
      /* AudioSampleEntry: 8 (reserved+dri) + 8 reserved + channelcount
       * + samplesize + 4 + samplerate(16.16); children at 28.  A QTFF
       * version-1 entry inserts 16 extra bytes before the children. */
      if (e.size >= 28)
      {
         t->pub.channels    = rmp4_be16(e.body + 16);
         t->pub.sample_rate = rmp4_be32(e.body + 24) >> 16;
      }
      {
         rmp4_box probe;
         uint64_t pnext;
         if (!rmp4_box_at(e.body, e.size, child_off, &probe, &pnext)
             && e.size >= 10 && rmp4_be16(e.body + 8) == 1)
            child_off = 44;
      }
      if (e.type == RMP4_FOURCC('O','p','u','s'))
      {
         rmp4_box dops;
         if (child_off < e.size
             && rmp4_find_child(e.body + child_off, e.size - child_off,
                   RMP4_FOURCC('d','O','p','s'), &dops)
             && rmp4_build_opus_head(t, dops.body, dops.size))
            t->pub.codec = RMP4_CODEC_OPUS;
      }
      else if (e.type == RMP4_FOURCC('m','p','4','a'))
      {
         rmp4_box esds;
         if (child_off < e.size
             && rmp4_find_child(e.body + child_off, e.size - child_off,
                   RMP4_FOURCC('e','s','d','s'), &esds)
             && esds.size > 4)
         {
            const uint8_t *dsi;
            uint64_t       dsi_size;
            /* skip the FullBox version/flags word */
            unsigned oti = rmp4_parse_esds(esds.body + 4, esds.size - 4,
                  &dsi, &dsi_size);
            /* 0xDD is the (de-facto) Xiph Vorbis object type; its
             * DecoderSpecificInfo is the xiph-laced 3 headers, the
             * same layout WebM's CodecPrivate uses. */
            if (oti == 0xDD && dsi && dsi_size > 3 && dsi[0] == 2)
            {
               t->pub.codec              = RMP4_CODEC_VORBIS;
               t->pub.codec_private      = dsi;
               t->pub.codec_private_size = (size_t)dsi_size;
            }
         }
      }
   }
}

/* ===== sample-table construction ===== */

typedef struct
{
   const uint8_t *stts;  uint64_t stts_size;
   const uint8_t *ctts;  uint64_t ctts_size;
   const uint8_t *stss;  uint64_t stss_size;
   const uint8_t *stsz;  uint64_t stsz_size;
   const uint8_t *stsc;  uint64_t stsc_size;
   const uint8_t *stco;  uint64_t stco_size;
   int            co64;
} rmp4_stbl;

/* Build the per-sample offset/size/pts/key arrays for one track.
 * Returns 1 on success (possibly with zero samples), 0 on OOM or a
 * table too malformed to trust. */
static int rmp4_build_samples(rmp4_itrack *t, const rmp4_stbl *s,
      uint64_t file_len)
{
   uint32_t count, uniform, i;
   uint32_t nchunks;

   /* --- sizes (stsz) give the authoritative sample count --- */
   if (!s->stsz || s->stsz_size < 12)
      return 1;                      /* no samples (e.g. fragmented)   */
   uniform = rmp4_be32(s->stsz + 4);
   count   = rmp4_be32(s->stsz + 8);
   if (!count)
      return 1;
   if (count > RMP4_MAX_SAMPLES)
      count = RMP4_MAX_SAMPLES;
   if (!uniform && s->stsz_size < 12 + (uint64_t)count * 4)
      return 0;

   t->off  = (uint64_t*)malloc((size_t)count * sizeof(uint64_t));
   t->size = (uint32_t*)malloc((size_t)count * sizeof(uint32_t));
   t->pts  = (int64_t*) malloc((size_t)count * sizeof(int64_t));
   t->key  = (uint8_t*) malloc((size_t)count);
   if (!t->off || !t->size || !t->pts || !t->key)
      return 0;

   for (i = 0; i < count; i++)
      t->size[i] = uniform ? uniform : rmp4_be32(s->stsz + 12 + i * 4);

   /* --- decode times (stts), then composition offsets (ctts) --- */
   {
      uint64_t dts = 0;
      uint32_t n_ent, e, filled = 0;
      if (!s->stts || s->stts_size < 8)
         return 0;
      n_ent = rmp4_be32(s->stts + 4);
      if (s->stts_size < 8 + (uint64_t)n_ent * 8)
         return 0;
      for (e = 0; e < n_ent && filled < count; e++)
      {
         uint32_t sc = rmp4_be32(s->stts + 8 + e * 8);
         uint32_t sd = rmp4_be32(s->stts + 8 + e * 8 + 4);
         while (sc-- && filled < count)
         {
            t->pts[filled++] = (int64_t)dts;
            dts             += sd;
         }
      }
      while (filled < count)
         t->pts[filled++] = (int64_t)dts;
   }
   if (s->ctts && s->ctts_size >= 8)
   {
      uint32_t version = s->ctts[0];
      uint32_t n_ent   = rmp4_be32(s->ctts + 4);
      uint32_t e, idx  = 0;
      if (s->ctts_size >= 8 + (uint64_t)n_ent * 8)
      {
         for (e = 0; e < n_ent && idx < count; e++)
         {
            uint32_t sc  = rmp4_be32(s->ctts + 8 + e * 8);
            uint32_t off = rmp4_be32(s->ctts + 8 + e * 8 + 4);
            int64_t  soff = (version == 0)
                  ? (int64_t)off : (int64_t)(int32_t)off;
            while (sc-- && idx < count)
               t->pts[idx++] += soff;
         }
      }
   }
   for (i = 0; i < count; i++)
      t->pts[i] = rmp4_ticks_to_ns(t->pts[i], t->timescale);

   /* --- key frames (stss); absent means every sample syncs --- */
   if (s->stss && s->stss_size >= 8)
   {
      uint32_t n_ent = rmp4_be32(s->stss + 4);
      uint32_t e;
      memset(t->key, 0, count);
      if (s->stss_size >= 8 + (uint64_t)n_ent * 4)
         for (e = 0; e < n_ent; e++)
         {
            uint32_t sn = rmp4_be32(s->stss + 8 + e * 4);
            if (sn >= 1 && sn <= count)
               t->key[sn - 1] = 1;
         }
   }
   else
      memset(t->key, 1, count);

   /* --- offsets: walk chunks via stsc x stco/co64 --- */
   if (!s->stco || s->stco_size < 8 || !s->stsc || s->stsc_size < 8)
      return 0;
   nchunks = rmp4_be32(s->stco + 4);
   {
      uint32_t entsz = s->co64 ? 8 : 4;
      if (s->stco_size < 8 + (uint64_t)nchunks * entsz)
         return 0;
   }
   {
      uint32_t n_stsc = rmp4_be32(s->stsc + 4);
      uint32_t run, sample = 0;
      if (s->stsc_size < 8 + (uint64_t)n_stsc * 12)
         return 0;
      for (run = 0; run < n_stsc && sample < count; run++)
      {
         uint32_t first = rmp4_be32(s->stsc + 8 + run * 12);
         uint32_t per   = rmp4_be32(s->stsc + 8 + run * 12 + 4);
         uint32_t last  = (run + 1 < n_stsc)
               ? rmp4_be32(s->stsc + 8 + (run + 1) * 12) - 1
               : nchunks;
         uint32_t c;
         if (first < 1 || last > nchunks || first > last || !per)
            return 0;
         for (c = first; c <= last && sample < count; c++)
         {
            uint64_t off = s->co64
                  ? rmp4_be64(s->stco + 8 + (uint64_t)(c - 1) * 8)
                  : rmp4_be32(s->stco + 8 + (uint64_t)(c - 1) * 4);
            uint32_t k;
            for (k = 0; k < per && sample < count; k++)
            {
               t->off[sample] = off;
               off           += t->size[sample];
               sample++;
            }
         }
      }
      /* Every stored sample must have received an offset and must lie
       * inside the file; drop the tail past the first bad one. */
      for (i = 0; i < sample; i++)
         if (t->off[i] > file_len || t->size[i] > file_len - t->off[i])
            break;
      t->count = i;
   }
   return 1;
}

/* ===== moov parsing ===== */

static void rmp4_parse_trak(rmp4_t *m, const uint8_t *body, uint64_t size)
{
   rmp4_box mdia, mdhd, hdlr, minf, stbl, b;
   rmp4_itrack *t;
   rmp4_stbl tabs;
   uint64_t pos, next;

   if (m->num_tracks >= RMP4_MAX_TRACKS)
      return;
   if (!rmp4_find_child(body, size, RMP4_FOURCC('m','d','i','a'), &mdia))
      return;
   if (!rmp4_find_child(mdia.body, mdia.size,
         RMP4_FOURCC('m','d','h','d'), &mdhd))
      return;
   if (!rmp4_find_child(mdia.body, mdia.size,
         RMP4_FOURCC('h','d','l','r'), &hdlr))
      return;
   if (!rmp4_find_child(mdia.body, mdia.size,
         RMP4_FOURCC('m','i','n','f'), &minf))
      return;
   if (!rmp4_find_child(minf.body, minf.size,
         RMP4_FOURCC('s','t','b','l'), &stbl))
      return;

   t = &m->trk[m->num_tracks];
   memset(t, 0, sizeof(*t));
   t->pub.number = m->num_tracks + 1;

   /* tkhd carries the official track_ID; go find it if present. */
   if (rmp4_find_child(body, size, RMP4_FOURCC('t','k','h','d'), &b)
       && b.size >= 4)
   {
      uint64_t idoff = (b.body[0] == 1) ? 20 : 12;
      if (b.size >= idoff + 4)
         t->pub.number = (int)rmp4_be32(b.body + idoff);
   }

   /* mdhd: media timescale (v0 at 12, v1 at 20). */
   if (mdhd.size >= 4)
   {
      uint64_t off = (mdhd.body[0] == 1) ? 20 : 12;
      if (mdhd.size >= off + 4)
         t->timescale = rmp4_be32(mdhd.body + off);
   }
   if (!t->timescale)
      return;

   /* hdlr: handler_type at offset 8 of the payload. */
   if (hdlr.size >= 12)
   {
      uint32_t h = rmp4_be32(hdlr.body + 8);
      if (h == RMP4_FOURCC('v','i','d','e'))
         t->pub.type = RMP4_TRACK_VIDEO;
      else if (h == RMP4_FOURCC('s','o','u','n'))
         t->pub.type = RMP4_TRACK_AUDIO;
      else
         t->pub.type = RMP4_TRACK_OTHER;
   }
   else
      return;

   if (rmp4_find_child(stbl.body, stbl.size,
         RMP4_FOURCC('s','t','s','d'), &b))
      rmp4_parse_stsd(t, b.body, b.size);

   /* Collect the sample tables. */
   memset(&tabs, 0, sizeof(tabs));
   pos = 0;
   while (pos < stbl.size && rmp4_box_at(stbl.body, stbl.size, pos, &b, &next))
   {
      switch (b.type)
      {
         case RMP4_FOURCC('s','t','t','s'):
            tabs.stts = b.body; tabs.stts_size = b.size; break;
         case RMP4_FOURCC('c','t','t','s'):
            tabs.ctts = b.body; tabs.ctts_size = b.size; break;
         case RMP4_FOURCC('s','t','s','s'):
            tabs.stss = b.body; tabs.stss_size = b.size; break;
         case RMP4_FOURCC('s','t','s','z'):
            tabs.stsz = b.body; tabs.stsz_size = b.size; break;
         case RMP4_FOURCC('s','t','s','c'):
            tabs.stsc = b.body; tabs.stsc_size = b.size; break;
         case RMP4_FOURCC('s','t','c','o'):
            tabs.stco = b.body; tabs.stco_size = b.size; tabs.co64 = 0;
            break;
         case RMP4_FOURCC('c','o','6','4'):
            tabs.stco = b.body; tabs.stco_size = b.size; tabs.co64 = 1;
            break;
         default:
            break;
      }
      if (next <= pos)
         break;
      pos = next;
   }

   if (!rmp4_build_samples(t, &tabs, (uint64_t)m->len))
   {
      free(t->off);  t->off  = NULL;
      free(t->size); t->size = NULL;
      free(t->pts);  t->pts  = NULL;
      free(t->key);  t->key  = NULL;
      t->count = 0;
      return;
   }
   m->num_tracks++;
}

static int rmp4_parse_moov(rmp4_t *m, const uint8_t *body, uint64_t size)
{
   uint64_t pos = 0, next;
   rmp4_box b;
   int have_mvhd = 0;

   while (pos < size && rmp4_box_at(body, size, pos, &b, &next))
   {
      if (b.type == RMP4_FOURCC('m','v','h','d') && b.size >= 4)
      {
         if (b.body[0] == 1)
         {
            if (b.size >= 32)
            {
               m->movie_timescale = rmp4_be32(b.body + 20);
               m->movie_duration  = rmp4_be64(b.body + 24);
            }
         }
         else if (b.size >= 24)
         {
            m->movie_timescale = rmp4_be32(b.body + 12);
            m->movie_duration  = rmp4_be32(b.body + 16);
         }
         have_mvhd = 1;
      }
      else if (b.type == RMP4_FOURCC('t','r','a','k'))
         rmp4_parse_trak(m, b.body, b.size);
      if (next <= pos)
         break;
      pos = next;
   }
   return have_mvhd || m->num_tracks > 0;
}

/* ===== public API ===== */

rmp4_t *rmp4_open_memory(const uint8_t *data, size_t size)
{
   rmp4_t  *m;
   uint64_t pos = 0, next;
   rmp4_box b;
   int      seen_known = 0, have_moov = 0;

   if (!data || size < 16)
      return NULL;

   if (!(m = (rmp4_t*)calloc(1, sizeof(*m))))
      return NULL;
   m->buf = data;
   m->len = size;

   while (pos < size && rmp4_box_at(data, (uint64_t)size, pos, &b, &next))
   {
      switch (b.type)
      {
         case RMP4_FOURCC('f','t','y','p'):
         case RMP4_FOURCC('m','d','a','t'):
         case RMP4_FOURCC('f','r','e','e'):
         case RMP4_FOURCC('s','k','i','p'):
         case RMP4_FOURCC('w','i','d','e'):
         case RMP4_FOURCC('m','o','o','f'):
            seen_known = 1;
            break;
         case RMP4_FOURCC('m','o','o','v'):
            seen_known = 1;
            if (rmp4_parse_moov(m, b.body, b.size))
               have_moov = 1;
            break;
         default:
            break;
      }
      if (next <= pos)
         break;
      pos = next;
   }

   if (!seen_known || !have_moov)
   {
      rmp4_close(m);
      return NULL;
   }
   return m;
}

void rmp4_close(rmp4_t *m)
{
   int i;
   if (!m)
      return;
   for (i = 0; i < RMP4_MAX_TRACKS; i++)
   {
      free(m->trk[i].off);
      free(m->trk[i].size);
      free(m->trk[i].pts);
      free(m->trk[i].key);
   }
   free(m);
}

int rmp4_num_tracks(const rmp4_t *m)
{
   return m ? m->num_tracks : 0;
}

const rmp4_track *rmp4_get_track(const rmp4_t *m, int index)
{
   if (!m || index < 0 || index >= m->num_tracks)
      return NULL;
   return &m->trk[index].pub;
}

int64_t rmp4_duration_ns(const rmp4_t *m)
{
   if (!m || !m->movie_timescale)
      return 0;
   return rmp4_ticks_to_ns((int64_t)m->movie_duration,
         m->movie_timescale);
}

void rmp4_rewind(rmp4_t *m)
{
   int i;
   if (!m)
      return;
   for (i = 0; i < m->num_tracks; i++)
      m->trk[i].cursor = 0;
}

int rmp4_read_packet(rmp4_t *m, rmp4_packet *pkt)
{
   int      i, best = -1;
   uint64_t best_off = 0;

   if (!m || !pkt)
      return -1;

   /* Merge the per-track cursors by file offset: with ascending chunk
    * offsets (every real muxer) this reproduces file order, and it is
    * per-track decode order regardless. */
   for (i = 0; i < m->num_tracks; i++)
   {
      rmp4_itrack *t = &m->trk[i];
      if (t->cursor >= t->count)
         continue;
      if (best < 0 || t->off[t->cursor] < best_off)
      {
         best     = i;
         best_off = t->off[t->cursor];
      }
   }
   if (best < 0)
      return 0;

   {
      rmp4_itrack *t = &m->trk[best];
      uint32_t     c = t->cursor++;
      pkt->data            = m->buf + t->off[c];
      pkt->size            = t->size[c];
      pkt->track           = best;
      pkt->timestamp       = t->pts[c];
      pkt->keyframe        = t->key[c];
      pkt->discard_padding = 0;
   }
   return 1;
}
