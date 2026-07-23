/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rpng_apng.c).
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

/* APNG (Animated PNG) streaming decoder.
 *
 * A companion to rpng.c that reads an animated PNG's control chunks
 * (acTL / fcTL / fdAT) and emits composited RGBA frames one at a time,
 * mirroring the rwebp_anim_stream / video-stream contract so the shared
 * image_transfer animation surface can drive it.  Memory use is
 * independent of the frame count: a single persistent canvas is kept
 * and each frame is blended onto it.
 *
 * Each APNG frame is decoded by synthesising a minimal standalone PNG -
 * the file's own IHDR (with the frame's fcTL sub-dimensions), the
 * frame's IDAT/fdAT data payload re-tagged as IDAT, and IEND - and
 * running it through the ordinary rpng decode path.  That reuses all of
 * rpng's inflate/unfilter/colour handling instead of duplicating it;
 * the only APNG-specific logic here is chunk indexing and the
 * dispose/blend compositing rules from the APNG specification.
 *
 * A still (non-animated) PNG has no acTL and decodes as its single IDAT
 * image through rpng.c as before; this module is engaged only when acTL
 * is present.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <retro_inline.h>
#include <boolean.h>
#include <formats/image.h>
#include <formats/rpng.h>

/* APNG dispose_op / blend_op (from the spec). */
enum apng_dispose
{
   APNG_DISPOSE_NONE       = 0,  /* leave the frame region as-is       */
   APNG_DISPOSE_BACKGROUND = 1,  /* clear the frame region to 0        */
   APNG_DISPOSE_PREVIOUS   = 2   /* restore the region to pre-frame    */
};

enum apng_blend
{
   APNG_BLEND_SOURCE = 0,        /* overwrite the region               */
   APNG_BLEND_OVER   = 1         /* alpha-composite onto the region    */
};

/* One IDAT/fdAT payload run belonging to a frame.  Named (rather than
 * anonymous) so the realloc results below can be cast explicitly, which
 * C++ requires. */
struct apng_part
{
   size_t off;
   size_t len;
};

/* One animation frame: its fcTL geometry/timing plus the byte range of
 * its data chunks (IDAT for the default image when it is also the first
 * frame, otherwise fdAT).  data_off/data_len describe the concatenated
 * *payloads* to re-tag as a single IDAT stream. */
struct apng_frame
{
   uint32_t width, height;   /* fcTL frame dimensions           */
   uint32_t x_off, y_off;    /* fcTL frame position on canvas   */
   uint16_t delay_num;       /* fcTL delay numerator            */
   uint16_t delay_den;       /* fcTL delay denominator (0 => 100) */
   uint8_t  dispose;         /* enum apng_dispose               */
   uint8_t  blend;           /* enum apng_blend                 */
   /* Each frame may carry several IDAT/fdAT chunks; store their
    * individual (payload offset, payload length) so the synthesised
    * PNG concatenates exactly the compressed bytes with no chunk
    * framing or fdAT sequence numbers. */
   struct apng_part *parts;
   int      num_parts;
};

struct rpng_apng_stream
{
   const uint8_t *buf;       /* borrowed; must outlive the stream */
   size_t         len;

   /* Bytes 8..(ihdr_end): the file signature and IHDR chunk, copied
    * verbatim into each synthesised per-frame PNG (with the width/height
    * fields overwritten to the frame's sub-dimensions). */
   const uint8_t *ihdr;      /* points at the IHDR length field   */
   size_t         ihdr_total; /* signature(8) + IHDR chunk bytes  */

   /* PLTE/tRNS, if present, must be replayed into every frame PNG for
    * palette images; store their full-chunk byte ranges. */
   size_t         plte_off, plte_len;   /* 0 len => absent */
   size_t         trns_off, trns_len;

   struct apng_frame *frames;
   int       num_frames;
   int       loop_count;     /* acTL num_plays; 0 = infinite       */
   int       canvas_w, canvas_h;

   int       cursor;         /* next frame index to emit           */
   int       emitted;        /* frames emitted since open/rewind   */

   uint32_t *canvas;         /* persistent RGBA canvas             */
   /* Lazy disposal: remember the previous frame's rectangle and its
    * dispose op so the region can be cleared/restored when the next
    * frame is prepared, without a second full canvas except when
    * DISPOSE_PREVIOUS demands it. */
   uint32_t *prev_save;      /* saved region for DISPOSE_PREVIOUS  */
   /* Reusable scratch for the per-frame synthesised PNG.  Frames are
    * decoded one at a time and the buffer is only ever handed to the
    * throwaway rpng decode below, so a single grown-as-needed buffer
    * replaces a malloc/free pair per frame - meaningful on the
    * memory-constrained handhelds this path targets, where a long
    * animation would otherwise churn the heap every loop. */
   uint8_t  *synth;
   size_t    synth_cap;

   /* Progressive indexing over a still-arriving buffer.  avail is the
    * resident byte count (== len once the read completes); scan_pos is
    * where the chunk walk stopped, so raising avail resumes from there
    * instead of re-walking.  indexed is how many frames are fully
    * described so far - next() will not run past it, and returns NULL
    * (end of pass) rather than decoding a frame whose fdAT has not
    * arrived. */
   size_t    avail;
   size_t    scan_pos;
   int       indexed;      /* frames fully described so far          */
   int       scan_frame;   /* fcTL index the walk is currently on;
                              persisted because the walk can stop
                              between a frame's fcTL and its data     */
   int       index_done;
   int       prev_x, prev_y, prev_w, prev_h;
   uint8_t   prev_dispose;
   int       have_prev;

   int       emit_argb;      /* 0 = R,G,B,A memory order; 1 = ARGB */
};

static INLINE uint32_t apng_be32(const uint8_t *p)
{
   return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
        | ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}
static INLINE uint16_t apng_be16(const uint8_t *p)
{
   return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static INLINE void apng_wr32(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
   p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

/* CRC32 (PNG polynomial) for the synthesised chunks. */
static uint32_t apng_crc(const uint8_t *p, size_t n)
{
   static uint32_t tbl[256];
   static int init = 0;
   uint32_t c = 0xFFFFFFFFu;
   size_t i;
   if (!init)
   {
      uint32_t k, j, v;
      for (k = 0; k < 256; k++)
      {
         v = k;
         for (j = 0; j < 8; j++)
            v = (v >> 1) ^ (0xEDB88320u & (0u - (v & 1)));
         tbl[k] = v;
      }
      init = 1;
   }
   for (i = 0; i < n; i++)
      c = (c >> 8) ^ tbl[(c ^ p[i]) & 0xFF];
   return c ^ 0xFFFFFFFFu;
}

static const uint8_t apng_png_sig[8] =
   { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };

void rpng_apng_stream_close(rpng_apng_stream_t *s)
{
   int i;
   if (!s)
      return;
   if (s->frames)
   {
      for (i = 0; i < s->num_frames; i++)
         free(s->frames[i].parts);
      free(s->frames);
   }
   free(s->canvas);
   free(s->prev_save);
   free(s->synth);
   free(s);
}

/* Detect whether a buffer is an APNG (has an acTL chunk before the
 * first IDAT).  Cheap scan, no allocation - lets the caller pick the
 * animation path only for real APNGs and fall back to the still
 * decoder otherwise.
 *
 * 'need_more' (may be NULL) distinguishes a conclusive "no" from "the
 * answer is not in these bytes yet": a caller probing a short header
 * prefix sets it and reads further only when it is set, instead of
 * mistaking a truncated prefix for a still PNG. */
bool rpng_is_apng_ex(const uint8_t *buf, size_t len, int *need_more)
{
   size_t p = 8;
   if (need_more)
      *need_more = 0;
   if (!buf || len < 8)
   {
      if (need_more && len < 8)
         *need_more = 1;
      return false;
   }
   if (memcmp(buf, apng_png_sig, 8) != 0)
      return false;   /* not a PNG at all: conclusive */
   for (;;)
   {
      uint32_t clen;
      const uint8_t *typ;
      if (p + 8 > len)
      {
         if (need_more)
            *need_more = 1;   /* chunk header not resident */
         return false;
      }
      clen = apng_be32(buf + p);
      typ  = buf + p + 4;
      if (!memcmp(typ, "acTL", 4))
         return true;
      if (!memcmp(typ, "IDAT", 4))
         return false;   /* acTL must precede IDAT: conclusive "no" */
      if (!memcmp(typ, "IEND", 4))
         return false;   /* end of file, no acTL: conclusive */
      /* Skip this chunk; if it runs past the resident bytes the answer
       * is still ahead. */
      if (clen > len - p - 8)
      {
         if (need_more)
            *need_more = 1;
         return false;
      }
      p += (size_t)clen + 12;
   }
}

bool rpng_is_apng(const uint8_t *buf, size_t len)
{
   return rpng_is_apng_ex(buf, len, NULL);
}

/* Index the file: capture IHDR, optional PLTE/tRNS, acTL, and every
 * fcTL and its following IDAT/fdAT run.  Returns false on malformed or
 * non-APNG input. */
/* Walk chunks from the last stop up to the resident frontier, filling
 * in whatever frames have arrived.  Called once for a whole-buffer open
 * and again from rpng_apng_stream_set_avail as the read progresses.
 * Returns false only on malformed input - an incomplete index is not an
 * error, it just leaves s->indexed short of s->num_frames. */
static bool apng_index(rpng_apng_stream_t *s)
{
   size_t p = s->scan_pos ? s->scan_pos : 8;
   int cur_frame = s->scan_pos ? s->scan_frame : -1;
   int frame_cap = s->num_frames;
   uint32_t ihdr_w = (uint32_t)s->canvas_w;
   uint32_t ihdr_h = (uint32_t)s->canvas_h;

   if (s->index_done)
      return true;
   if (s->len < 8 || memcmp(s->buf, apng_png_sig, 8) != 0)
      return false;

   while (p + 12 <= s->avail)
   {
      uint32_t clen = apng_be32(s->buf + p);
      const uint8_t *typ = s->buf + p + 4;
      const uint8_t *pay = s->buf + p + 8;
      size_t chunk_total;

      /* Not all here yet: stop and resume when more arrives.  Only a
       * chunk that cannot fit even in the complete file is malformed. */
      if (clen > s->len - p - 12)
         return false;
      chunk_total = (size_t)clen + 12;
      if (p + chunk_total > s->avail)
         break;

      if (!memcmp(typ, "IHDR", 4))
      {
         if (clen != 13)
            return false;
         s->ihdr       = s->buf + p;      /* at the length field */
         s->ihdr_total = 8 + chunk_total; /* signature + IHDR    */
         ihdr_w        = apng_be32(pay + 0);
         ihdr_h        = apng_be32(pay + 4);
         /* Cap the canvas the same way rpng caps a still image
          * (0x4000 per side).  These are attacker-controlled 32-bit
          * fields and the canvas is allocated from them, so without a
          * cap a malformed file asks for tens of gigabytes; the
          * multiply below would also overflow size_t on 32-bit. */
         if (   ihdr_w == 0 || ihdr_h == 0
             || ihdr_w > 0x4000u || ihdr_h > 0x4000u)
            return false;
         s->canvas_w   = (int)ihdr_w;
         s->canvas_h   = (int)ihdr_h;
      }
      else if (!memcmp(typ, "PLTE", 4))
      {
         s->plte_off = p;
         s->plte_len = chunk_total;
      }
      else if (!memcmp(typ, "tRNS", 4))
      {
         s->trns_off = p;
         s->trns_len = chunk_total;
      }
      else if (!memcmp(typ, "acTL", 4))
      {
         if (clen != 8)
            return false;
         s->num_frames = (int)apng_be32(pay + 0);
         s->loop_count = (int)apng_be32(pay + 4);
         if (s->num_frames <= 0 || s->num_frames > 100000)
            return false;
         s->frames = (struct apng_frame*)calloc(
               (size_t)s->num_frames, sizeof(*s->frames));
         if (!s->frames)
            return false;
         frame_cap = s->num_frames;
      }
      else if (!memcmp(typ, "fcTL", 4))
      {
         struct apng_frame *f;
         if (clen != 26 || !s->frames)
            return false;
         cur_frame++;
         if (cur_frame >= frame_cap)
            return false;   /* more fcTL than acTL declared */
         f = &s->frames[cur_frame];
         f->width     = apng_be32(pay + 4);
         f->height    = apng_be32(pay + 8);
         f->x_off     = apng_be32(pay + 12);
         f->y_off     = apng_be32(pay + 16);
         f->delay_num = apng_be16(pay + 20);
         f->delay_den = apng_be16(pay + 22);
         f->dispose   = pay[24];
         f->blend     = pay[25];
         if (f->width == 0 || f->height == 0
               || f->x_off + f->width  > ihdr_w
               || f->y_off + f->height > ihdr_h)
            return false;
      }
      else if (!memcmp(typ, "IDAT", 4))
      {
         /* The default image.  It belongs to frame 0 only when a fcTL
          * precedes it (APNG allows an IDAT that is NOT part of the
          * animation, in which case frame 0's data is the first fdAT).
          * If cur_frame == 0 here, this IDAT is frame 0's data. */
         if (cur_frame == 0)
         {
            struct apng_frame *f = &s->frames[0];
            struct apng_part *np = (struct apng_part*)realloc(f->parts,
                  sizeof(*f->parts) * (size_t)(f->num_parts + 1));
            if (!np)
               return false;
            f->parts = np;
            f->parts[f->num_parts].off = (size_t)(pay - s->buf);
            f->parts[f->num_parts].len = clen;
            f->num_parts++;
         }
         /* else: non-animated default image, skipped for animation */
      }
      else if (!memcmp(typ, "fdAT", 4))
      {
         /* fdAT: 4-byte sequence number, then IDAT-equivalent bytes. */
         struct apng_frame *f;
         if (clen < 4 || cur_frame < 0)
            return false;
         f = &s->frames[cur_frame];
         {
            struct apng_part *np = (struct apng_part*)realloc(f->parts,
                  sizeof(*f->parts) * (size_t)(f->num_parts + 1));
            if (!np)
               return false;
            f->parts = np;
            /* Skip the 4-byte sequence number; the rest is data. */
            f->parts[f->num_parts].off = (size_t)(pay - s->buf) + 4;
            f->parts[f->num_parts].len = clen - 4;
            f->num_parts++;
         }
      }
      else if (!memcmp(typ, "IEND", 4))
      {
         s->index_done = 1;
         p += chunk_total;
         break;
      }

      p += chunk_total;
      /* A frame is usable once its fcTL and at least one data chunk are
       * in; the next fcTL (or IEND) proves no more data chunks follow
       * the previous frame. */
      if (cur_frame >= 0 && s->frames && s->frames[cur_frame].num_parts > 0)
         s->indexed = cur_frame + 1;
   }
   s->scan_pos   = p;
   s->scan_frame = cur_frame;

   /* IHDR and acTL must be present to have a usable stream at all; the
    * frame index may still be partial (s->indexed < s->num_frames) when
    * the read is in flight. */
   if (!s->ihdr || !s->frames)
      return false;
   if (s->index_done && s->indexed != s->num_frames)
      return false;   /* file ended with fewer frames than acTL declared */
   return true;
}

rpng_apng_stream_t *rpng_apng_stream_open(const uint8_t *buf, size_t len)
{
   rpng_apng_stream_t *s;
   if (!rpng_is_apng(buf, len))
      return NULL;
   s = (rpng_apng_stream_t*)calloc(1, sizeof(*s));
   if (!s)
      return NULL;
   s->buf        = buf;
   s->len        = len;
   s->avail      = len;   /* whole buffer resident */
   s->loop_count = 0;
   if (!apng_index(s))
   {
      rpng_apng_stream_close(s);
      return NULL;
   }
   s->canvas = (uint32_t*)calloc(
         (size_t)s->canvas_w * (size_t)s->canvas_h, sizeof(uint32_t));
   if (!s->canvas)
   {
      rpng_apng_stream_close(s);
      return NULL;
   }
   return s;
}

/* Progressive open over a partially-resident buffer.  Succeeds as soon
 * as the signature, IHDR and acTL are in and at least one frame is
 * fully described; the rest of the index fills in through
 * rpng_apng_stream_set_avail as the read advances.  Returns NULL with
 * *need_more set when the header is not resident yet (retry on a larger
 * prefix), NULL with it clear when the data is not an APNG at all. */
rpng_apng_stream_t *rpng_apng_stream_open_avail(const uint8_t *buf,
      size_t len, size_t avail, int *need_more)
{
   rpng_apng_stream_t *s;
   int nm = 0;
   if (need_more)
      *need_more = 0;
   if (avail > len)
      avail = len;
   if (!rpng_is_apng_ex(buf, avail, &nm))
   {
      if (nm && need_more)
         *need_more = 1;
      return NULL;
   }
   if (!(s = (rpng_apng_stream_t*)calloc(1, sizeof(*s))))
      return NULL;
   s->buf   = buf;
   s->len   = len;
   s->avail = avail;
   if (!apng_index(s))
   {
      /* apng_index fails both for malformed input and for a prefix
       * that has not reached IHDR/acTL yet.  Only the latter is worth
       * retrying: it is distinguishable because the walk stopped short
       * of the resident frontier with the file still incomplete. */
      int retry = (avail < len);
      rpng_apng_stream_close(s);
      if (retry && need_more)
         *need_more = 1;
      return NULL;
   }
   /* Need the geometry and at least one decodable frame to start. */
   if (s->canvas_w <= 0 || s->canvas_h <= 0 || s->indexed < 1)
   {
      int retry = (avail < len);
      rpng_apng_stream_close(s);
      if (retry && need_more)
         *need_more = 1;
      return NULL;
   }
   s->canvas = (uint32_t*)calloc(
         (size_t)s->canvas_w * (size_t)s->canvas_h, sizeof(uint32_t));
   if (!s->canvas)
   {
      rpng_apng_stream_close(s);
      return NULL;
   }
   return s;
}

/* Declare how many leading bytes are resident; monotonic.  Resumes the
 * chunk walk so frames that have since arrived become playable. */
void rpng_apng_stream_set_avail(rpng_apng_stream_t *s, size_t avail)
{
   if (!s)
      return;
   if (avail > s->len)
      avail = s->len;
   if (avail <= s->avail)
      return;
   s->avail = avail;
   apng_index(s);   /* extends s->indexed; malformed input just stops it */
}

void rpng_apng_stream_get_info(const rpng_apng_stream_t *s,
      unsigned *width, unsigned *height, int *num_frames, int *loop_count)
{
   if (!s)
      return;
   if (width)      *width      = (unsigned)s->canvas_w;
   if (height)     *height     = (unsigned)s->canvas_h;
   if (num_frames) *num_frames = s->num_frames;
   if (loop_count) *loop_count = s->loop_count;
}

/* Build a standalone single-image PNG for frame f in *out (malloc'd),
 * returning its length, or 0 on failure.  Layout: signature, IHDR (with
 * the frame's sub-dimensions), optional PLTE/tRNS copied verbatim, one
 * IDAT with all of the frame's concatenated data payloads, IEND. */
static uint8_t *apng_build_frame_png(rpng_apng_stream_t *s,
      const struct apng_frame *f, size_t *out_len)
{
   size_t data_len = 0;
   size_t total, pos;
   uint8_t *o;
   int i;

   for (i = 0; i < f->num_parts; i++)
      data_len += f->parts[i].len;

   total  = 8;                             /* signature            */
   total += s->ihdr_total - 8;             /* IHDR chunk           */
   if (s->plte_len) total += s->plte_len;
   if (s->trns_len) total += s->trns_len;
   total += 12 + data_len;                 /* IDAT chunk           */
   total += 12;                            /* IEND chunk           */

   if (total > s->synth_cap)
   {
      uint8_t *ns = (uint8_t*)realloc(s->synth, total);
      if (!ns)
         return NULL;
      s->synth     = ns;
      s->synth_cap = total;
   }
   o   = s->synth;
   pos = 0;

   /* signature + IHDR, then patch IHDR width/height to the frame's. */
   memcpy(o, apng_png_sig, 8);
   pos = 8;
   memcpy(o + pos, s->ihdr, s->ihdr_total - 8);
   /* IHDR data begins at (chunk start + 8); width/height are its first
    * 8 bytes. The IHDR chunk here starts at o+8. */
   apng_wr32(o + pos + 8 + 0, f->width);
   apng_wr32(o + pos + 8 + 4, f->height);
   /* recompute IHDR CRC over type+data (13+4 bytes) */
   {
      uint32_t crc = apng_crc(o + pos + 4, 4 + 13);
      apng_wr32(o + pos + 8 + 13, crc);
   }
   pos += s->ihdr_total - 8;

   if (s->plte_len)
   {
      memcpy(o + pos, s->buf + s->plte_off, s->plte_len);
      pos += s->plte_len;
   }
   if (s->trns_len)
   {
      memcpy(o + pos, s->buf + s->trns_off, s->trns_len);
      pos += s->trns_len;
   }

   /* IDAT: length, "IDAT", concatenated data, CRC. */
   apng_wr32(o + pos, (uint32_t)data_len);
   memcpy(o + pos + 4, "IDAT", 4);
   {
      size_t dp = pos + 8;
      for (i = 0; i < f->num_parts; i++)
      {
         memcpy(o + dp, s->buf + f->parts[i].off, f->parts[i].len);
         dp += f->parts[i].len;
      }
   }
   {
      uint32_t crc = apng_crc(o + pos + 4, 4 + data_len);
      apng_wr32(o + pos + 8 + data_len, crc);
   }
   pos += 12 + data_len;

   /* IEND */
   apng_wr32(o + pos, 0);
   memcpy(o + pos + 4, "IEND", 4);
   apng_wr32(o + pos + 8, apng_crc(o + pos + 4, 4));
   pos += 12;

   *out_len = total;
   return o;
}

/* Decode one frame to an RGBA sub-image via the ordinary rpng path. */
static uint32_t *apng_decode_frame(rpng_apng_stream_t *s,
      const struct apng_frame *f, unsigned *fw, unsigned *fh)
{
   size_t   png_len = 0;
   uint8_t *png     = apng_build_frame_png(s, f, &png_len);
   rpng_t  *r;
   uint32_t *data   = NULL;
   int ret;

   if (!png)
      return NULL;
   r = rpng_alloc();
   if (!r || !rpng_set_buf_ptr(r, png, png_len) || !rpng_start(r))
   {
      if (r) rpng_free(r);
      return NULL;   /* png is the stream's reusable scratch: not freed */
   }
   while (rpng_iterate_image(r)) ;
   /* Channel order: the animation contract's default (emit_argb == 0)
    * is memory R,G,B,A, which rpng produces with supports_rgba = true
    * (ABGR32 word on LE).  emit_argb == 1 wants ARGB words (BGRA
    * memory), rpng's supports_rgba = false.  So pass the inverse. */
   do { ret = rpng_process_image(r, (void**)&data, png_len,
              fw, fh, s->emit_argb ? false : true); }
   while (ret == IMAGE_PROCESS_NEXT);
   rpng_free(r);
   if (ret != IMAGE_PROCESS_END)
   {
      free(data);
      return NULL;
   }
   return data;
}

/* Apply the previous frame's disposal before compositing the next. */
static void apng_apply_prev_dispose(rpng_apng_stream_t *s)
{
   int y;
   if (!s->have_prev)
      return;
   if (s->prev_dispose == APNG_DISPOSE_BACKGROUND)
   {
      for (y = 0; y < s->prev_h; y++)
         memset(s->canvas + (size_t)(s->prev_y + y) * s->canvas_w
               + s->prev_x, 0, (size_t)s->prev_w * sizeof(uint32_t));
   }
   else if (s->prev_dispose == APNG_DISPOSE_PREVIOUS && s->prev_save)
   {
      for (y = 0; y < s->prev_h; y++)
         memcpy(s->canvas + (size_t)(s->prev_y + y) * s->canvas_w
               + s->prev_x,
               s->prev_save + (size_t)y * s->prev_w,
               (size_t)s->prev_w * sizeof(uint32_t));
   }
   /* DISPOSE_NONE: leave as-is. */
}

/* Composite a decoded frame sub-image onto the canvas at its offset. */
static void apng_composite(rpng_apng_stream_t *s,
      const struct apng_frame *f, const uint32_t *frame)
{
   unsigned x, y;
   for (y = 0; y < f->height; y++)
   {
      uint32_t *dst = s->canvas
         + (size_t)(f->y_off + y) * s->canvas_w + f->x_off;
      const uint32_t *src = frame + (size_t)y * f->width;
      if (f->blend == APNG_BLEND_SOURCE)
      {
         memcpy(dst, src, (size_t)f->width * sizeof(uint32_t));
      }
      else /* APNG_BLEND_OVER: alpha-composite src over dst */
      {
         for (x = 0; x < f->width; x++)
         {
            uint32_t sp = src[x];
            unsigned sa = s->emit_argb ? (sp >> 24)
                                       : (sp >> 24); /* A is top byte both orders */
            if (sa == 255)
               dst[x] = sp;
            else if (sa != 0)
            {
               uint32_t dp = dst[x];
               unsigned ia = 255 - sa;
               /* Per-channel over; channel positions are identical for
                * the arithmetic since we operate on all four bytes. */
               unsigned s0 =  sp        & 0xFF, d0 =  dp        & 0xFF;
               unsigned s1 = (sp >> 8)  & 0xFF, d1 = (dp >> 8)  & 0xFF;
               unsigned s2 = (sp >> 16) & 0xFF, d2 = (dp >> 16) & 0xFF;
               unsigned s3 = (sp >> 24) & 0xFF, d3 = (dp >> 24) & 0xFF;
               unsigned o0 = (s0 * sa + d0 * ia + 127) / 255;
               unsigned o1 = (s1 * sa + d1 * ia + 127) / 255;
               unsigned o2 = (s2 * sa + d2 * ia + 127) / 255;
               unsigned o3 = s3 + (d3 * ia + 127) / 255;
               if (o3 > 255) o3 = 255;
               dst[x] = o0 | (o1 << 8) | (o2 << 16) | (o3 << 24);
            }
            /* sa == 0: fully transparent source, leave dst. */
         }
      }
   }
}

const uint32_t *rpng_apng_stream_next(rpng_apng_stream_t *s,
      int *duration_ms)
{
   const struct apng_frame *f;
   uint32_t *frame;
   unsigned fw = 0, fh = 0;

   if (!s)
      return NULL;
   if (s->cursor >= s->num_frames)
      return NULL;   /* end of one pass; caller rewinds to loop */
   /* Frame not indexed yet: the read has not delivered its fcTL/fdAT.
    * Report end-of-pass rather than decode from bytes that have not
    * arrived - the caller loops what it has, and the frame becomes
    * available once rpng_apng_stream_set_avail admits it. */
   if (s->cursor >= s->indexed)
      return NULL;

   f = &s->frames[s->cursor];

   /* Apply the previous frame's disposal to the canvas. */
   apng_apply_prev_dispose(s);

   /* For DISPOSE_PREVIOUS, save this frame's target region before it is
    * overwritten so it can be restored after. */
   if (f->dispose == APNG_DISPOSE_PREVIOUS)
   {
      int y;
      uint32_t *ns = (uint32_t*)realloc(s->prev_save,
            (size_t)f->width * f->height * sizeof(uint32_t));
      if (ns)
      {
         s->prev_save = ns;
         for (y = 0; y < (int)f->height; y++)
            memcpy(s->prev_save + (size_t)y * f->width,
                  s->canvas + (size_t)(f->y_off + y) * s->canvas_w
                  + f->x_off, (size_t)f->width * sizeof(uint32_t));
      }
   }

   frame = apng_decode_frame(s, f, &fw, &fh);
   if (!frame || fw != f->width || fh != f->height)
   {
      free(frame);
      return NULL;
   }

   apng_composite(s, f, frame);
   free(frame);

   /* Record disposal state for the next call. */
   s->prev_x       = (int)f->x_off;
   s->prev_y       = (int)f->y_off;
   s->prev_w       = (int)f->width;
   s->prev_h       = (int)f->height;
   s->prev_dispose = f->dispose;
   s->have_prev    = 1;

   if (duration_ms)
   {
      unsigned den = f->delay_den ? f->delay_den : 100;
      *duration_ms = (int)((unsigned)f->delay_num * 1000u / den);
   }

   s->cursor++;
   s->emitted++;
   return s->canvas;
}

bool rpng_apng_stream_set_argb(rpng_apng_stream_t *s, int argb)
{
   /* Honoured only at a clean boundary (before any frame emitted or
    * right after a rewind); the canvas is composited in whatever order
    * frames are decoded in, and switching mid-animation would require
    * re-swizzling the persistent canvas.  Report success only when it
    * can be applied cleanly so the caller keeps converting otherwise. */
   if (!s)
      return false;
   if (s->emitted == 0)
   {
      s->emit_argb = argb ? 1 : 0;
      return true;
   }
   return false;
}

void rpng_apng_stream_rewind(rpng_apng_stream_t *s)
{
   if (!s)
      return;
   s->cursor   = 0;
   s->have_prev = 0;
   memset(s->canvas, 0,
         (size_t)s->canvas_w * (size_t)s->canvas_h * sizeof(uint32_t));
}
