/* rwebm -- WebM (Matroska subset) demuxer for libretro-common.
 *
 * A pure demuxer: it parses the EBML/Matroska container and hands out the
 * elementary-stream packets, without decoding.  See rwebm.h for the API.
 *
 * Scope: WebM restricts Matroska to VP8/VP9 video and Vorbis/Opus audio,
 * SimpleBlock/BlockGroup lacing, and a Segment/Cluster/Block layout.  The
 * parser reads the elements it needs (EBML head just far enough to accept
 * the stream, Segment > Info for the timestamp scale and duration,
 * Segment > Tracks for the track table) and then streams SimpleBlocks and
 * Block(Group)s out of the clusters in file order.
 *
 * The whole input is supplied up front and borrowed, not copied; returned
 * packet pointers alias into it.
 *
 * SPDX-License-Identifier: MIT  (RetroArch libretro-common)
 */
#include <stdlib.h>
#include <string.h>

#include <formats/rwebm.h>

/* ===== EBML primitives ===== */

/* Matroska element IDs (the ones we act on). */
#define ID_EBML            0x1A45DFA3u
#define ID_SEGMENT         0x18538067u
#define ID_INFO            0x1549A966u
#define ID_TIMESTAMPSCALE  0x2AD7B1u
#define ID_DURATION        0x4489u
#define ID_TRACKS          0x1654AE6Bu
#define ID_TRACKENTRY      0xAEu
#define ID_TRACKNUMBER     0xD7u
#define ID_TRACKTYPE       0x83u
#define ID_CODECID         0x86u
#define ID_CODECPRIVATE    0x63A2u
#define ID_CODECDELAY      0x56AAu
#define ID_VIDEO           0xE0u
#define ID_PIXELWIDTH      0xB0u
#define ID_PIXELHEIGHT     0xBAu
#define ID_COLOUR          0x55B0u
#define ID_MATRIXCOEFF     0x55B1u
#define ID_COLOURRANGE     0x55B9u
#define ID_TRANSFERCHAR    0x55BAu
#define ID_PRIMARIES       0x55BBu
#define ID_MAXCLL          0x55BCu
#define ID_MAXFALL         0x55BDu
#define ID_AUDIO           0xE1u
#define ID_SAMPLINGFREQ    0xB5u
#define ID_CHANNELS        0x9Fu
#define ID_CLUSTER         0x1F43B675u
#define ID_TIMESTAMP       0xE7u
#define ID_SIMPLEBLOCK     0xA3u
#define ID_BLOCKGROUP      0xA0u
#define ID_BLOCK           0xA1u
#define ID_DISCARDPADDING  0x75A2u

#define TRACKTYPE_VIDEO    1
#define TRACKTYPE_AUDIO    2

#define RWEBM_MAX_TRACKS   8

typedef struct
{
   const uint8_t *p;   /* cursor                  */
   const uint8_t *end; /* one past the last byte  */
} ebml_reader;

/* Read an EBML element ID (keeps the length-descriptor bits, per the ID
 * encoding). Returns 0 and does not advance on truncation. */
static uint32_t ebml_read_id(ebml_reader *r)
{
   uint8_t  first;
   int      len, i;
   uint32_t v;
   if (r->p >= r->end)
      return 0;
   first = *r->p;
   len   = 1;
   {
      uint8_t m = 0x80;
      while (len <= 4 && !(first & m)) { m >>= 1; len++; }
   }
   if (len > 4 || r->p + len > r->end)
      return 0;
   v = 0;
   for (i = 0; i < len; i++)
      v = (v << 8) | r->p[i];
   r->p += len;
   return v;
}

/* Read an EBML variable-length integer (the data size / unsigned value).
 * When strip_marker is set the length-descriptor bit is masked off (as for
 * sizes and most values); leave it clear only if a raw form is needed.
 * Returns the value; *ok is 0 on truncation. Advances the cursor. */
static uint64_t ebml_read_vint(ebml_reader *r, int strip_marker, int *ok)
{
   uint8_t  first;
   int      len, i;
   uint64_t v;
   *ok = 0;
   if (r->p >= r->end)
      return 0;
   first = *r->p;
   len   = 1;
   {
      uint8_t m = 0x80;
      while (len <= 8 && !(first & m)) { m >>= 1; len++; }
      if (len > 8 || r->p + len > r->end)
         return 0;
      v = strip_marker ? (uint64_t)(first & (m - 1)) : first;
   }
   for (i = 1; i < len; i++)
      v = (v << 8) | r->p[i];
   r->p += len;
   *ok = 1;
   return v;
}

/* Read a big-endian unsigned of 'n' bytes (Matroska uint element). */
static uint64_t be_uint(const uint8_t *p, size_t n)
{
   uint64_t v = 0;
   size_t   i;
   for (i = 0; i < n; i++)
      v = (v << 8) | p[i];
   return v;
}

/* Signed big-endian integer element payload (sign-extended). */
/* Multiply a tick count by the ns-per-tick scale, saturating instead of
 * overflowing on corrupt input (timestamps are informational). */
static int64_t webm_scale_ts(int64_t ticks, int64_t scale)
{
   if (ticks == 0 || scale == 0)
      return 0;
   if (ticks > 0 && ticks > INT64_MAX / scale)
      return INT64_MAX;
   if (ticks < 0 && ticks < INT64_MIN / scale)
      return INT64_MIN;
   return ticks * scale;
}

static int64_t ebml_sint(const uint8_t *p, size_t n)
{
   uint64_t v = be_uint(p, n);
   int shift  = (int)(64 - 8 * n);
   return ((int64_t)(v << shift)) >> shift;
}

/* Read a big-endian IEEE float/double (Matroska float element). */
static double be_float(const uint8_t *p, size_t n)
{
   if (n == 4)
   {
      uint32_t u = (uint32_t)be_uint(p, 4);
      float    f;
      memcpy(&f, &u, 4);
      return (double)f;
   }
   if (n == 8)
   {
      uint64_t u = be_uint(p, 8);
      double   d;
      memcpy(&d, &u, 8);
      return d;
   }
   return 0.0;
}

/* ===== Demuxer state ===== */

struct rwebm
{
   const uint8_t *data;
   size_t         size;
   rwebm_track    tracks[RWEBM_MAX_TRACKS];
   int            num_tracks;
   int64_t        timestamp_scale;  /* ns per tick; default 1000000        */
   double         duration_ticks;   /* raw Duration element (scaled later) */
   const uint8_t *clusters_begin;   /* first byte after Tracks (first data)*/
   const uint8_t *segment_end;
   /* Packet-read cursor. */
   const uint8_t *cur;              /* walk position inside the segment    */
   int64_t        cluster_ts;       /* current cluster timestamp (ticks)   */
   /* Lacing: a SimpleBlock may carry several frames; we emit them one at a
    * time, so remember where we are within a block. */
   int64_t pending_discard;         /* DiscardPadding for the next Block   */
   int64_t lace_discard;            /* padding owed by the laced block     */
   const uint8_t *lace_data;        /* payload after the block header      */
   const uint32_t *lace_sizes;      /* not used (see note); kept simple    */
   int            lace_count;
   int            lace_index;
   uint32_t       lace_frame[256];  /* per-frame sizes when laced          */
   const uint8_t *lace_frame_ptr[256];
   int            lace_track;
   int64_t        lace_ts;
   int            lace_keyframe;
};

static enum rwebm_codec codec_from_id(const char *id)
{
   if (!strcmp(id, "V_VP8"))    return RWEBM_CODEC_VP8;
   if (!strcmp(id, "V_VP9"))    return RWEBM_CODEC_VP9;
   if (!strcmp(id, "A_VORBIS")) return RWEBM_CODEC_VORBIS;
   if (!strcmp(id, "A_OPUS"))   return RWEBM_CODEC_OPUS;
   if (!strcmp(id, "V_MPEG4/ISO/AVC")) return RWEBM_CODEC_H264;
   /* A_AAC with a profile suffix (e.g. A_AAC/MPEG4/LC) appears in older
    * muxes; the CodecPrivate AudioSpecificConfig is authoritative
    * either way. */
   if (!strncmp(id, "A_AAC", 5)) return RWEBM_CODEC_AAC;
   return RWEBM_CODEC_UNKNOWN;
}

/* Parse the Video/Audio sub-element's fields into an existing track. */
static void parse_track_av(const uint8_t *p, const uint8_t *end,
      rwebm_track *trk)
{
   ebml_reader r;
   r.p = p; r.end = end;
   while (r.p < r.end)
   {
      int      ok;
      uint32_t id  = ebml_read_id(&r);
      uint64_t sz  = ebml_read_vint(&r, 1, &ok);
      const uint8_t *body = r.p;
      if (!id || !ok || body + sz > r.end)
         return;
      switch (id)
      {
         case ID_PIXELWIDTH:
            trk->width       = (unsigned)be_uint(body, (size_t)sz);
            break;
         case ID_PIXELHEIGHT:
            trk->height      = (unsigned)be_uint(body, (size_t)sz);
            break;
         case ID_SAMPLINGFREQ:
            trk->sample_rate = (unsigned)(be_float(body, (size_t)sz) + 0.5);
            break;
         case ID_CHANNELS:
            trk->channels    = (unsigned)be_uint(body, (size_t)sz);
            break;
         case ID_COLOUR:
         {
            /* Colour master element: pick out the code points we use */
            ebml_reader cr;
            cr.p = body; cr.end = body + sz;
            while (cr.p < cr.end)
            {
               int       cok;
               uint32_t  cid   = ebml_read_id(&cr);
               uint64_t  csz   = ebml_read_vint(&cr, 1, &cok);
               const uint8_t *cb = cr.p;
               if (!cid || !cok || cb + csz > cr.end)
                  break;
               switch (cid)
               {
                  case ID_MATRIXCOEFF:
                     trk->matrix_coefficients     = (unsigned)be_uint(cb, (size_t)csz);
                     break;
                  case ID_COLOURRANGE:
                     trk->colour_range            = (unsigned)be_uint(cb, (size_t)csz);
                     break;
                  case ID_TRANSFERCHAR:
                     trk->transfer_characteristics = (unsigned)be_uint(cb, (size_t)csz);
                     break;
                  case ID_PRIMARIES:
                     trk->primaries               = (unsigned)be_uint(cb, (size_t)csz);
                     break;
                  case ID_MAXCLL:
                     trk->max_cll                 = (unsigned)be_uint(cb, (size_t)csz);
                     break;
                  case ID_MAXFALL:
                     trk->max_fall                = (unsigned)be_uint(cb, (size_t)csz);
                     break;
                  default:
                     break;
               }
               cr.p = cb + csz;
            }
            break;
         }
         default:
            break;
      }
      r.p = body + sz;
   }
}

/* Parse one TrackEntry into 'trk'. Range is [p,end). */
static void parse_track_entry(const uint8_t *p, const uint8_t *end,
      rwebm_track *trk)
{
   ebml_reader r;
   r.p = p; r.end = end;
   memset(trk, 0, sizeof(*trk));
   while (r.p < r.end)
   {
      int      ok;
      uint32_t id  = ebml_read_id(&r);
      uint64_t sz  = ebml_read_vint(&r, 1, &ok);
      const uint8_t *body = r.p;
      if (!id || !ok || body + sz > r.end)
         return;
      switch (id)
      {
         case ID_TRACKNUMBER:
            trk->number = (int)be_uint(body, (size_t)sz);
            break;
         case ID_TRACKTYPE:
         {
            uint64_t t = be_uint(body, (size_t)sz);
            trk->type  = (t == TRACKTYPE_VIDEO) ? RWEBM_TRACK_VIDEO
                       : (t == TRACKTYPE_AUDIO) ? RWEBM_TRACK_AUDIO
                       : RWEBM_TRACK_OTHER;
            break;
         }
         case ID_CODECID:
         {
            size_t n = (size_t)sz;
            if (n >= sizeof(trk->codec_id))
               n = sizeof(trk->codec_id) - 1;
            memcpy(trk->codec_id, body, n);
            trk->codec_id[n] = '\0';
            trk->codec       = codec_from_id(trk->codec_id);
            break;
         }
         case ID_CODECPRIVATE:
            trk->codec_private      = body;
            trk->codec_private_size = (size_t)sz;
            break;
         case ID_CODECDELAY:
            /* nanoseconds of decoded output to drop from the stream
             * start (the encoder delay; AAC priming in mkv muxes) */
            trk->codec_delay_ns = be_uint(body, (size_t)sz);
            break;
         case ID_VIDEO:
         case ID_AUDIO:
            parse_track_av(body, body + sz, trk);
            break;
         default:
            break;
      }
      r.p = body + sz;
   }
}

int64_t rwebm_duration_ns(const rwebm_t *webm)
{
   if (!webm || webm->duration_ticks <= 0.0)
      return 0;
   return (int64_t)(webm->duration_ticks * (double)webm->timestamp_scale);
}

int rwebm_num_tracks(const rwebm_t *webm)
{
   return webm ? webm->num_tracks : 0;
}

const rwebm_track *rwebm_get_track(const rwebm_t *webm, int index)
{
   if (!webm || index < 0 || index >= webm->num_tracks)
      return NULL;
   return &webm->tracks[index];
}

void rwebm_close(rwebm_t *webm)
{
   free(webm);
}

void rwebm_rewind(rwebm_t *webm)
{
   if (!webm)
      return;
   webm->cur        = webm->clusters_begin;
   webm->cluster_ts = 0;
   webm->lace_count = 0;
   webm->lace_index = 0;
}

/* DiscardPadding for the Block just parsed: the element may sit before
 * the Block (already captured in pending_discard) or after it within
 * the same BlockGroup, so scan the remaining group siblings. */
static int64_t webm_group_discard(rwebm_t *webm, const uint8_t *p,
      const uint8_t *group_end)
{
   int64_t v = webm->pending_discard;
   ebml_reader s;
   webm->pending_discard = 0;
   s.p   = p;
   s.end = group_end;
   while (s.p < s.end)
   {
      int ok;
      uint32_t id = ebml_read_id(&s);
      uint64_t sz = ebml_read_vint(&s, 1, &ok);
      if (!id || !ok || s.p + sz > s.end)
         break;
      if (id == ID_DISCARDPADDING && sz >= 1 && sz <= 8)
         v = ebml_sint(s.p, (size_t)sz);
      s.p += sz;
   }
   return v;
}

int rwebm_read_packet(rwebm_t *webm, rwebm_packet *pkt)
{
   ebml_reader r;
   if (!webm || !pkt)
      return -1;

   /* Drain a laced block first. */
   if (webm->lace_index < webm->lace_count)
   {
      int i = webm->lace_index++;
      pkt->data      = webm->lace_frame_ptr[i];
      pkt->size      = webm->lace_frame[i];
      pkt->track     = webm->lace_track;
      pkt->timestamp = webm->lace_ts;
      pkt->keyframe  = webm->lace_keyframe;
      pkt->discard_padding = (webm->lace_index == webm->lace_count)
         ? webm->lace_discard : 0;
      return 1;
   }

   r.p = webm->cur; r.end = webm->segment_end;
   while (r.p < r.end)
   {
      int      ok;
      uint32_t id  = ebml_read_id(&r);
      uint64_t sz  = ebml_read_vint(&r, 1, &ok);
      const uint8_t *body = r.p;
      if (!id || !ok || body + sz > r.end)
      {
         webm->cur = r.end;
         return 0;
      }
      if (id == ID_CLUSTER || id == ID_BLOCKGROUP)
      {
         /* Descend: Cluster holds Timestamp + blocks; BlockGroup wraps a
          * single Block (plus DiscardPadding/duration/refs). Scan
          * children. */
         if (id == ID_BLOCKGROUP)
            webm->pending_discard = 0;
         r.p   = body;
         r.end = body + sz;
         continue;
      }
      if (id == ID_DISCARDPADDING)
      {
         /* Signed big-endian integer, nanoseconds; may precede the
          * Block within its BlockGroup. */
         if (sz >= 1 && sz <= 8)
            webm->pending_discard = ebml_sint(body, (size_t)sz);
         r.p = body + sz;
         continue;
      }
      if (id == ID_TIMESTAMP)
      {
         webm->cluster_ts = (int64_t)be_uint(body, (size_t)sz);
         r.p = body + sz;
         continue;
      }
      if (id == ID_SIMPLEBLOCK || id == ID_BLOCK)
      {
         ebml_reader br;
         int         bok;
         uint64_t    tracknum;
         int16_t     rel_ts;
         uint8_t     flags;
         const uint8_t *frame;
         size_t      frame_len;
         br.p = body; br.end = body + sz;
         tracknum = ebml_read_vint(&br, 1, &bok);
         if (!bok || br.p + 3 > br.end)
         { r.p = body + sz; continue; }
         rel_ts = (int16_t)((br.p[0] << 8) | br.p[1]);
         flags  = br.p[2];
         br.p  += 3;
         frame     = br.p;
         frame_len = (size_t)(br.end - br.p);
         {
            /* Map Matroska track number to our 0-based index. */
            int ti, tidx = -1;
            for (ti = 0; ti < webm->num_tracks; ti++)
               if (webm->tracks[ti].number == (int)tracknum)
               { tidx = ti; break; }
            if (tidx < 0)
            { r.p = body + sz; continue; }

            /* Lacing: bits 1-2 of flags. 0 = no lacing (one frame). */
            if ((flags & 0x06) == 0)
            {
               pkt->data      = frame;
               pkt->size      = frame_len;
               pkt->track     = tidx;
               pkt->timestamp = webm_scale_ts(webm->cluster_ts + rel_ts,
                              webm->timestamp_scale);
               pkt->keyframe  = (id == ID_SIMPLEBLOCK)
                              ? ((flags & 0x80) != 0) : 1;
               pkt->discard_padding = (id == ID_BLOCK)
                  ? webm_group_discard(webm, body + sz, r.end) : 0;
               webm->cur      = body + sz;
               return 1;
            }
            else
            {
               /* Laced block: parse the frame count + sizes, stash them,
                * then return the first frame; subsequent calls drain. */
               int      nframes = frame[0] + 1;
               int      lace_type = (flags & 0x06) >> 1;
               const uint8_t *fp = frame + 1;
               const uint8_t *fend = frame + frame_len;
               int      k;
               uint32_t sizes[256];
               if (nframes > 256)
               { r.p = body + sz; continue; }
               if (lace_type == 1) /* Xiph */
               {
                  int bad = 0;
                  for (k = 0; k < nframes - 1; k++)
                  {
                     uint32_t l = 0;
                     while (fp < fend && *fp == 255) { l += 255; fp++; }
                     if (fp >= fend) { bad = 1; break; }
                     l += *fp++;
                     sizes[k] = l;
                  }
                  if (bad)
                  { r.p = body + sz; continue; }
               }
               else if (lace_type == 3) /* EBML */
               {
                  ebml_reader lr; int lok = 0;
                  uint64_t first;
                  int bad = 0;
                  int64_t prev;
                  lr.p = fp; lr.end = fend;
                  first = ebml_read_vint(&lr, 1, &lok);
                  if (!lok || first > (uint64_t)(fend - lr.p))
                  { r.p = body + sz; continue; }
                  sizes[0] = (uint32_t)first;
                  prev = (int64_t)first;
                  for (k = 1; k < nframes - 1; k++)
                  {
                     /* signed vint delta; decode as vint then unbias */
                     const uint8_t *sp = lr.p;
                     uint8_t fb; int len = 1; uint8_t m = 0x80;
                     int64_t bias, val;
                     uint64_t raw;
                     if (sp >= lr.end) { bad = 1; break; }
                     fb = *sp;
                     while (len <= 8 && !(fb & m)) { m >>= 1; len++; }
                     raw  = ebml_read_vint(&lr, 1, &lok);
                     if (!lok) { bad = 1; break; }
                     bias = (int64_t)((((uint64_t)1) << (7 * len - 1)) - 1);
                     val  = prev + ((int64_t)raw - bias);
                     if (val < 0) { bad = 1; break; }
                     sizes[k] = (uint32_t)val;
                     prev = val;
                  }
                  if (bad)
                  { r.p = body + sz; continue; }
                  fp = lr.p;
               }
               else /* fixed-size (lace_type == 2) */
               {
                  uint32_t each;
                  if (nframes <= 0 || fend < fp)
                  { r.p = body + sz; continue; }
                  each = (uint32_t)((size_t)(fend - fp) / (size_t)nframes);
                  for (k = 0; k < nframes; k++)
                     sizes[k] = each;
               }
               /* last frame size = remainder */
               {
                  uint32_t used = 0;
                  const uint8_t *data_start = fp;
                  const uint8_t *cursor;
                  int j;
                  if (lace_type != 2)
                  {
                     size_t avail = (size_t)(fend - data_start);
                     for (k = 0; k < nframes - 1; k++) used += sizes[k];
                     if ((size_t)used > avail)
                     { r.p = body + sz; continue; }
                     sizes[nframes - 1] = (uint32_t)(avail - used);
                  }
                  /* Every frame must lie within [data_start, fend). */
                  {
                     size_t total = 0;
                     int bad = 0;
                     for (k = 0; k < nframes; k++)
                     {
                        total += sizes[k];
                        if (total > (size_t)(fend - data_start))
                        { bad = 1; break; }
                     }
                     if (bad)
                     { r.p = body + sz; continue; }
                  }
                  cursor = data_start;
                  webm->lace_count = nframes;
                  webm->lace_index = 0;
                  webm->lace_track = tidx;
                  webm->lace_ts    = webm_scale_ts(
                                     webm->cluster_ts + rel_ts,
                                     webm->timestamp_scale);
               webm->lace_discard = (id == ID_BLOCK)
                  ? webm_group_discard(webm, body + sz, r.end) : 0;
                  webm->lace_keyframe = (id == ID_SIMPLEBLOCK)
                                      ? ((flags & 0x80) != 0) : 1;
                  for (j = 0; j < nframes; j++)
                  {
                     webm->lace_frame[j]     = sizes[j];
                     webm->lace_frame_ptr[j] = cursor;
                     cursor += sizes[j];
                  }
                  webm->cur = body + sz;
                  /* emit first frame now */
                  {
                     int i = webm->lace_index++;
                     pkt->data      = webm->lace_frame_ptr[i];
                     pkt->size      = webm->lace_frame[i];
                     pkt->track     = webm->lace_track;
                     pkt->timestamp = webm->lace_ts;
                     pkt->keyframe  = webm->lace_keyframe;
                     pkt->discard_padding =
                        (webm->lace_index == webm->lace_count)
                        ? webm->lace_discard : 0;
                     return 1;
                  }
               }
            }
         }
      }
      /* Any other element: skip its body. */
      r.p = body + sz;
   }
   webm->cur = r.end;
   return 0;
}

/* ===== Open ===== */

rwebm_t *rwebm_open_memory(const uint8_t *data, size_t size)
{
   rwebm_t    *w;
   ebml_reader r;
   int         ok;
   uint32_t    id;
   uint64_t    sz;
   const uint8_t *seg_body;

   if (!data || size < 8)
      return NULL;

   r.p = data; r.end = data + size;

   /* EBML header must come first. */
   id = ebml_read_id(&r);
   if (id != ID_EBML)
      return NULL;
   sz = ebml_read_vint(&r, 1, &ok);
   if (!ok || r.p + sz > r.end)
      return NULL;
   r.p += sz;   /* skip the EBML header body (DocType etc.) */

   /* Segment. */
   id = ebml_read_id(&r);
   if (id != ID_SEGMENT)
      return NULL;
   sz = ebml_read_vint(&r, 1, &ok);
   if (!ok)
      return NULL;
   seg_body = r.p;
   if (seg_body + sz > r.end)
      sz = (uint64_t)(r.end - seg_body);   /* tolerate unknown/overlong    */

   w = (rwebm_t*)calloc(1, sizeof(*w));
   if (!w)
      return NULL;
   w->data            = data;
   w->size            = size;
   w->timestamp_scale = 1000000;   /* Matroska default: 1 ms per tick      */
   w->segment_end     = seg_body + sz;

   /* Walk the segment's top-level children for Info and Tracks; remember
    * where the first non-metadata element (usually the first Cluster)
    * begins so packet reading can start there. */
   {
      ebml_reader s;
      s.p = seg_body; s.end = w->segment_end;
      w->clusters_begin = seg_body;
      while (s.p < s.end)
      {
         uint32_t eid = ebml_read_id(&s);
         uint64_t esz = ebml_read_vint(&s, 1, &ok);
         const uint8_t *ebody = s.p;
         if (!eid || !ok || ebody + esz > s.end)
            break;
         if (eid == ID_INFO)
         {
            ebml_reader ir;
            ir.p = ebody; ir.end = ebody + esz;
            while (ir.p < ir.end)
            {
               int      iok;
               uint32_t iid = ebml_read_id(&ir);
               uint64_t isz = ebml_read_vint(&ir, 1, &iok);
               const uint8_t *ibody = ir.p;
               if (!iid || !iok || ibody + isz > ir.end)
                  break;
               if (iid == ID_TIMESTAMPSCALE)
               {
                  int64_t ts = (int64_t)be_uint(ibody, (size_t)isz);
                  /* Matroska caps this at 10 ms/tick; reject nonsense and
                   * keep the 1 ms default so timestamp math stays sane. */
                  if (ts > 0 && ts <= 10000000)
                     w->timestamp_scale = ts;
               }
               else if (iid == ID_DURATION)
                  w->duration_ticks = be_float(ibody, (size_t)isz);
               ir.p = ibody + isz;
            }
         }
         else if (eid == ID_TRACKS)
         {
            ebml_reader tr;
            tr.p = ebody; tr.end = ebody + esz;
            while (tr.p < tr.end && w->num_tracks < RWEBM_MAX_TRACKS)
            {
               int      tok;
               uint32_t tid = ebml_read_id(&tr);
               uint64_t tsz = ebml_read_vint(&tr, 1, &tok);
               const uint8_t *tbody = tr.p;
               if (!tid || !tok || tbody + tsz > tr.end)
                  break;
               if (tid == ID_TRACKENTRY)
                  parse_track_entry(tbody, tbody + tsz,
                        &w->tracks[w->num_tracks++]);
               tr.p = tbody + tsz;
            }
            /* First cluster/data starts right after Tracks. */
            w->clusters_begin = ebody + esz;
         }
         s.p = ebody + esz;
      }
   }

   if (w->num_tracks == 0)
   {
      free(w);
      return NULL;
   }

   w->cur        = w->clusters_begin;
   w->cluster_ts = 0;
   return w;
}
