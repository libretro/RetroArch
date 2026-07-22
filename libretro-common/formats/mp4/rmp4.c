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
   uint32_t    cap;            /* array capacity (fragment appends)  */
   uint32_t    cursor;
   /* Movie-fragment state: trex defaults and the running decode time. */
   uint32_t    trex_dur, trex_size, trex_flags;
   uint64_t    frag_dts;       /* ticks; tfdt resets it              */
   uint8_t     opus_head[RMP4_OPUS_HEAD_MAX];
} rmp4_itrack;

struct rmp4
{
   const uint8_t *buf;
   size_t         len;
   /* Bytes actually read into the buffer so far (== len for a fully
    * resident file); a partial reader raises it with rmp4_set_avail.
    * rmp4_read_packet returns RMP4_READ_AGAIN, consuming nothing, for
    * a sample whose bytes lie beyond it. */
   size_t         avail;
   rmp4_itrack    trk[RMP4_MAX_TRACKS];
   int            num_tracks;
   int            fragmented;      /* an mvex box was present        */
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
      /* colr carries the colour description for any visual sample entry:
       * colour_type 'nclx' (ISO, with a full-range flag) or 'nclc' (QTFF)
       * followed by primaries, transfer and matrix as 16-bit values. */
      if (e.size > 78)
      {
         rmp4_box colr;
         if (rmp4_find_child(e.body + 78, e.size - 78,
               RMP4_FOURCC('c','o','l','r'), &colr) && colr.size >= 10)
         {
            uint32_t ct = rmp4_be32(colr.body);
            if (ct == RMP4_FOURCC('n','c','l','x')
                  || ct == RMP4_FOURCC('n','c','l','c'))
            {
               t->pub.transfer_characteristics = rmp4_be16(colr.body + 6);
               t->pub.matrix_coefficients      = rmp4_be16(colr.body + 8);
               if (ct == RMP4_FOURCC('n','c','l','x') && colr.size >= 11)
                  t->pub.full_range = (colr.body[10] >> 7) & 1;
            }
         }
      }
      switch (e.type)
      {
         case RMP4_FOURCC('v','p','0','8'):
            t->pub.codec = RMP4_CODEC_VP8;
            break;
         case RMP4_FOURCC('v','p','0','9'):
            t->pub.codec = RMP4_CODEC_VP9;
            break;
         case RMP4_FOURCC('a','v','c','1'):
         case RMP4_FOURCC('a','v','c','3'):
         {
            /* AVCSampleEntry embeds an avcC (AVCDecoderConfigurationRecord)
             * child after the 78-byte VisualSampleEntry prefix; surface it
             * as codec_private so the decoder gets the SPS/PPS and NAL
             * length size. */
            rmp4_box avcc;
            if (e.size > 78
                && rmp4_find_child(e.body + 78, e.size - 78,
                      RMP4_FOURCC('a','v','c','C'), &avcc))
            {
               t->pub.codec              = RMP4_CODEC_H264;
               t->pub.codec_private      = avcc.body;
               t->pub.codec_private_size = (size_t)avcc.size;
            }
            break;
         }
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
            /* 0x40 is MPEG-4 Audio (AAC signalled through the
             * AudioSpecificConfig); 0x66..0x68 are the MPEG-2 AAC
             * profiles some muxers still emit, whose DSI uses the
             * same layout. */
            else if ((oti == 0x40 || (oti >= 0x66 && oti <= 0x68))
                  && dsi && dsi_size >= 2)
            {
               t->pub.codec              = RMP4_CODEC_AAC;
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
/* Append one sample to a track's parallel arrays, growing them as
 * movie fragments arrive. Returns 0 on allocation failure or once the
 * global sample cap is reached (further samples are dropped). */
static int rmp4_track_append(rmp4_itrack *t, uint64_t off, uint32_t size,
      int64_t pts_ns, uint8_t key)
{
   if (t->count >= RMP4_MAX_SAMPLES)
      return 0;
   if (t->count >= t->cap)
   {
      uint32_t  ncap = t->cap ? t->cap * 2 : 512;
      uint64_t *noff; uint32_t *nsize; int64_t *npts; uint8_t *nkey;
      if (ncap > RMP4_MAX_SAMPLES) ncap = RMP4_MAX_SAMPLES;
      noff  = (uint64_t*)realloc(t->off,  (size_t)ncap * sizeof(uint64_t));
      if (!noff)
         return 0;
      t->off  = noff;
      nsize = (uint32_t*)realloc(t->size, (size_t)ncap * sizeof(uint32_t));
      if (!nsize)
         return 0;
      t->size = nsize;
      npts  = (int64_t*) realloc(t->pts,  (size_t)ncap * sizeof(int64_t));
      if (!npts)
         return 0;
      t->pts  = npts;
      nkey  = (uint8_t*) realloc(t->key,  (size_t)ncap);
      if (!nkey)
         return 0;
      t->key  = nkey;
      t->cap = ncap;
   }
   t->off[t->count]  = off;
   t->size[t->count] = size;
   t->pts[t->count]  = pts_ns;
   t->key[t->count]  = key;
   t->count++;
   return 1;
}

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
   t->cap = count;

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

   /* edts/elst: the common single-entry edit that trims the codec's
    * startup samples (media_time > 0, e.g. the AAC encoder delay).
    * Recorded in media-timescale units for decoders to drop; edits
    * beyond that shape (multi-entry, dwell) stay unsupported. */
   if (rmp4_find_child(body, size, RMP4_FOURCC('e','d','t','s'), &b))
   {
      rmp4_box elst;
      if (rmp4_find_child(b.body, b.size,
            RMP4_FOURCC('e','l','s','t'), &elst) && elst.size >= 8)
      {
         unsigned version = elst.body[0];
         uint32_t count   = rmp4_be32(elst.body + 4);
         if (count == 1 && elst.size >= (version ? 28u : 20u))
         {
            int64_t media_time = version
                  ? (int64_t)rmp4_be64(elst.body + 16)
                  : (int32_t)rmp4_be32(elst.body + 12);
            if (media_time > 0)
               t->pub.media_skip = (uint64_t)media_time;
         }
      }
   }

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
   rmp4_box mvex;
   int have_mvhd = 0;

   mvex.body = NULL; mvex.size = 0; mvex.type = 0;

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
      else if (b.type == RMP4_FOURCC('m','v','e','x'))
         mvex = b;
      if (next <= pos)
         break;
      pos = next;
   }
   /* mvex marks a fragmented movie; its trex boxes carry per-track
    * defaults the fragments inherit. Applied after the walk since box
    * order inside moov is free. */
   if (mvex.body)
   {
      uint64_t tp = 0, tn;
      rmp4_box tx;
      m->fragmented = 1;
      while (tp < mvex.size && rmp4_box_at(mvex.body, mvex.size, tp, &tx, &tn))
      {
         if (tx.type == RMP4_FOURCC('t','r','e','x') && tx.size >= 24)
         {
            uint32_t tid = rmp4_be32(tx.body + 4);
            int i;
            for (i = 0; i < m->num_tracks; i++)
               if (m->trk[i].pub.number == (int)tid)
               {
                  m->trk[i].trex_dur   = rmp4_be32(tx.body + 12);
                  m->trk[i].trex_size  = rmp4_be32(tx.body + 16);
                  m->trk[i].trex_flags = rmp4_be32(tx.body + 20);
                  break;
               }
         }
         if (tn <= tp)
            break;
         tp = tn;
      }
   }
   return have_mvhd || m->num_tracks > 0;
}

/* ===== movie fragments (moof/traf/trun, 14496-12 8.8) ===== */

/* One traf: tfhd picks the track and its defaults, tfdt (re)bases the
 * decode time, each trun appends a run of samples. */
static void rmp4_parse_traf(rmp4_t *m, const uint8_t *body, uint64_t size,
      uint64_t moof_pos)
{
   uint64_t pos = 0, next;
   rmp4_box b;
   rmp4_itrack *t = NULL;
   uint64_t base = moof_pos;      /* default-base-is-moof and legacy   */
   uint32_t dflt_dur = 0, dflt_size = 0, dflt_flags = 0;
   uint64_t doff = 0;
   int      have_doff = 0;

   while (pos < size && rmp4_box_at(body, size, pos, &b, &next))
   {
      if (b.type == RMP4_FOURCC('t','f','h','d') && b.size >= 8)
      {
         uint32_t f   = rmp4_be32(b.body) & 0xffffff;
         uint32_t tid = rmp4_be32(b.body + 4);
         uint64_t off = 8;
         int i;
         t = NULL;
         for (i = 0; i < m->num_tracks; i++)
            if (m->trk[i].pub.number == (int)tid)
            { t = &m->trk[i]; break; }
         if (!t)
            return;
         dflt_dur   = t->trex_dur;
         dflt_size  = t->trex_size;
         dflt_flags = t->trex_flags;
         if (f & 0x000001)              /* base-data-offset            */
         {
            if (b.size < off + 8) return;
            base = rmp4_be64(b.body + off); off += 8;
         }
         if (f & 0x000002) off += 4;    /* sample-description-index    */
         if (f & 0x000008)              /* default-sample-duration     */
         {
            if (b.size < off + 4) return;
            dflt_dur = rmp4_be32(b.body + off); off += 4;
         }
         if (f & 0x000010)              /* default-sample-size         */
         {
            if (b.size < off + 4) return;
            dflt_size = rmp4_be32(b.body + off); off += 4;
         }
         if (f & 0x000020)              /* default-sample-flags        */
         {
            if (b.size < off + 4) return;
            dflt_flags = rmp4_be32(b.body + off);
         }
      }
      else if (b.type == RMP4_FOURCC('t','f','d','t') && t && b.size >= 8)
      {
         if (b.body[0] == 1 && b.size >= 12)
            t->frag_dts = rmp4_be64(b.body + 4);
         else
            t->frag_dts = rmp4_be32(b.body + 4);
      }
      else if (b.type == RMP4_FOURCC('t','r','u','n') && t && b.size >= 8)
      {
         uint32_t f = rmp4_be32(b.body) & 0xffffff;
         int      v = b.body[0];
         uint32_t n = rmp4_be32(b.body + 4);
         uint64_t off = 8;
         uint32_t first_flags = 0;
         int      have_first = 0;
         uint32_t i;
         if (f & 0x000001)              /* data-offset (signed)        */
         {
            if (b.size < off + 4) return;
            doff = base + (uint64_t)(int64_t)(int32_t)rmp4_be32(b.body + off);
            off += 4;
            have_doff = 1;
         }
         else if (!have_doff)
         {
            doff = base;
            have_doff = 1;
         }
         if (f & 0x000004)              /* first-sample-flags          */
         {
            if (b.size < off + 4) return;
            first_flags = rmp4_be32(b.body + off); off += 4;
            have_first  = 1;
         }
         for (i = 0; i < n; i++)
         {
            uint32_t dur = dflt_dur, sz = dflt_size, fl = dflt_flags;
            int32_t  cts = 0;
            int64_t  pts;
            if (f & 0x000100)
            {
               if (b.size < off + 4) return;
               dur = rmp4_be32(b.body + off); off += 4;
            }
            if (f & 0x000200)
            {
               if (b.size < off + 4) return;
               sz = rmp4_be32(b.body + off); off += 4;
            }
            if (f & 0x000400)
            {
               if (b.size < off + 4) return;
               fl = rmp4_be32(b.body + off); off += 4;
            }
            if (f & 0x000800)
            {
               if (b.size < off + 4) return;
               cts = (int32_t)rmp4_be32(b.body + off); off += 4;
               if (v == 0 && cts < 0)   /* v0 offsets are unsigned     */
                  cts = 0;
            }
            if (i == 0 && have_first)
               fl = first_flags;
            pts = rmp4_ticks_to_ns(
                  (int64_t)t->frag_dts + cts, t->timescale);
            if (sz && doff + sz <= m->len)
               rmp4_track_append(t, doff, sz, pts,
                     (uint8_t)!((fl >> 16) & 1));
            doff        += sz;
            t->frag_dts += dur;
         }
      }
      if (next <= pos)
         break;
      pos = next;
   }
}

static void rmp4_parse_moof(rmp4_t *m, const uint8_t *body, uint64_t size,
      uint64_t moof_pos)
{
   uint64_t pos = 0, next;
   rmp4_box b;
   while (pos < size && rmp4_box_at(body, size, pos, &b, &next))
   {
      if (b.type == RMP4_FOURCC('t','r','a','f'))
         rmp4_parse_traf(m, b.body, b.size, moof_pos);
      if (next <= pos)
         break;
      pos = next;
   }
}

/* ===== public API ===== */

rmp4_t *rmp4_open_memory_avail(const uint8_t *data, size_t size,
      size_t avail, int *need_more)
{
   rmp4_t  *m;
   uint64_t pos = 0, next;
   rmp4_box b;
   int      seen_known = 0, have_moov = 0, hit_wall = 0;

   if (need_more)
      *need_more = 0;
   if (avail > size)
      avail = size;
   if (!data || size < 16)
      return NULL;

   if (!(m = (rmp4_t*)calloc(1, sizeof(*m))))
      return NULL;
   m->buf   = data;
   m->len   = size;
   m->avail = avail;

   /* The top-level walk dereferences only box headers; bodies are
    * skipped arithmetically, so an mdat crossing the available prefix
    * costs nothing to hop over and a trailing moov becomes reachable
    * exactly when its bytes arrive.  Box sizes are validated against
    * the full file length; only actual reads (headers, and the moov
    * body when parsed) are gated on the prefix. */
   while (pos < size)
   {
      /* Gate BEFORE parsing: the box header read below must lie
       * wholly within the available prefix (16 covers the largest
       * header; at worst this costs one extra retry near the wall,
       * never a read past it). */
      if (avail < size && pos + 16 > avail)
      { hit_wall = 1; break; }
      if (!rmp4_box_at(data, (uint64_t)size, pos, &b, &next))
         break;
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
            if ((uint64_t)(b.body - data) + b.size > avail)
            { hit_wall = 1; break; }   /* moov body still arriving */
            if (rmp4_parse_moov(m, b.body, b.size))
               have_moov = 1;
            break;
         default:
            break;
      }
      if (hit_wall || have_moov)
         break;
      if (next <= pos)
         break;
      pos = next;
   }

   if (!have_moov)
   {
      rmp4_close(m);
      if ((hit_wall || avail < size) && need_more)
         *need_more = 1;
      return NULL;
   }
   if (!seen_known)
   {
      rmp4_close(m);
      return NULL;
   }

   /* Fragmented movies keep their samples in moof boxes following the
    * (usually empty) moov; walk them now that the tracks and their
    * trex defaults exist. */
   if (m->fragmented)
   {
      /* moof boxes interleave with the media data through to the end
       * of the file, so a fragmented movie effectively needs the whole
       * file resident before its sample tables exist. */
      if (avail < size)
      {
         rmp4_close(m);
         if (need_more)
            *need_more = 1;
         return NULL;
      }
      pos = 0;
      while (pos < size && rmp4_box_at(data, (uint64_t)size, pos, &b, &next))
      {
         if (b.type == RMP4_FOURCC('m','o','o','f'))
            rmp4_parse_moof(m, b.body, b.size, pos);
         if (next <= pos)
            break;
         pos = next;
      }
   }
   return m;
}

rmp4_t *rmp4_open_memory(const uint8_t *data, size_t size)
{
   return rmp4_open_memory_avail(data, size, size, NULL);
}

void rmp4_set_avail(rmp4_t *m, size_t avail)
{
   if (!m)
      return;
   if (avail > m->len)
      avail = m->len;
   if (avail > m->avail)   /* monotonic: bytes never un-arrive */
      m->avail = avail;
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
      uint32_t     c = t->cursor;
      /* Sample bytes not yet in the buffer: consume nothing and let
       * the caller retry once more of the file has been read. */
      if (t->off[c] + t->size[c] > (uint64_t)m->avail)
         return RMP4_READ_AGAIN;
      t->cursor++;
      pkt->data            = m->buf + t->off[c];
      pkt->size            = t->size[c];
      pkt->track           = best;
      pkt->timestamp       = t->pts[c];
      pkt->keyframe        = t->key[c];
      pkt->discard_padding = 0;
   }
   return 1;
}
