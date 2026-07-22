/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------
 * The following license statement only applies to this file (rwebm_video.c).
 * ---------------------------------------------------------------------
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
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* WebM video-to-image glue: rwebm demuxer + rvp8/rvp9 decoders exposed
 * through the still-image and streaming-animation contracts that
 * image_transfer.c dispatches on (see rwebm_video.h). */

#include <stdlib.h>
#include <string.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

#include <retro_inline.h>

#include <formats/image.h>
#include <formats/rwebm.h>
#include <formats/rvp8.h>
#ifdef HAVE_RVP9
#include <formats/rvp9.h>
#endif
#include <formats/rwebm_video.h>

/* Per-packet timestamps are pre-scanned at open so every frame's display
 * duration is known without lookahead decoding; cap the table so a
 * pathological stream cannot balloon it. Frames past the cap reuse the
 * last stored delta. */
#define RWEBM_VIDEO_MAX_TS       8192

/* A VP9 superframe carries at most 8 sub-frames. */
#define RWEBM_VIDEO_MAX_SUPER    8

struct rwebm_video_stream
{
   rwebm_t     *demux;
   rvp8_video  *vp8;
#ifdef HAVE_RVP9
   rvp9_dec    *vp9;
#endif
   uint32_t    *frame;      /* width * height ABGR words              */
   int64_t     *ts;         /* pre-scanned packet timestamps (ns)     */
   int          ts_count;   /* entries stored in ts                   */
   int          num_frames; /* total video packets in the stream      */
   int          pkt_idx;    /* ordinal of the next video packet       */
   int          track;      /* index of the chosen video track        */
   enum rwebm_codec codec;
   unsigned     width;
   unsigned     height;
   int          want10;     /* caller requested 10-bit output         */
   int          is10;       /* last decoded frame written as 10-bit    */
   int          emit_argb;  /* emit ARGB words instead of the default
                               R,G,B,A memory order (8-bit paths)     */
};

/* Still-image decode progress across sliced process calls. */
enum
{
   RWEBM_VIDEO_STILL_IDLE = 0,   /* nothing in progress (or done)      */
   RWEBM_VIDEO_STILL_SCAN,       /* stream open, pre-scan running      */
   RWEBM_VIDEO_STILL_DECODE      /* decoding to the first shown frame  */
};

/* Container packets walked per pre-scan slice of the sliced still
 * path: cheap header parses, sized so a slice stays well under a
 * display frame even on weak hardware while the full bounded scan
 * still completes in a handful of slices. */
#define RWEBM_VIDEO_SCAN_SLICE   1024

struct rwebm_video
{
   const uint8_t *buf;
   size_t         len;
   /* The stream opened for the still frame, kept so a caller that wants
    * to continue the video as an animation can detach it instead of
    * re-opening (and re-pre-scanning) the file.  Owned by this handle
    * until rwebm_video_detach_stream; closed by rwebm_video_free
    * otherwise.  Borrows 'buf', which must outlive it either way.
    * While still_stage is not IDLE this holds the partially-opened
    * stream the sliced still decode is building. */
   rwebm_video_stream_t *stream;
   int            still_stage; /* enum above */
   int            want10;     /* caller requested 10-bit thumbnail output */
   int            last_10bit; /* last processed frame was XRGB2101010 */
   /* Bytes of 'buf' actually read so far, for decoding a still from a
    * file whose read is in progress: 0 means fully resident (the
    * default), and the still decode returns IMAGE_PROCESS_WAIT
    * instead of erroring at the wall.  Raised by
    * rwebm_video_set_avail. */
   size_t         avail;
   int            partial;    /* set_avail was called: 'avail' is live
                                 (0 is a valid value - nothing read
                                 yet - so it cannot double as the
                                 unset sentinel) */
};

/* ------------------------------------------------------------------ */
/* 8-bit limited-range YCbCr coefficient sets, <<8: {re, gd, ge, bd}.  */
/* Untagged content defaults to BT.601 below 720 lines and BT.709 at   */
/* or above it, matching industry convention.                          */
/* ------------------------------------------------------------------ */
static const int16_t rwebm_video_coef_601[4]  = { 409, 100, 208, 516 };
static const int16_t rwebm_video_coef_709[4]  = { 459,  55, 136, 541 };
static const int16_t rwebm_video_coef_2020[4] = { 431,  48, 167, 548 };

static const int16_t *rwebm_video_coefs(unsigned matrix, unsigned height)
{
   switch (matrix)
   {
      case 1:            return rwebm_video_coef_709;
      case 5: case 6:    return rwebm_video_coef_601;
      case 9: case 10:   return rwebm_video_coef_2020;
      default:           return height >= 720
                            ? rwebm_video_coef_709 : rwebm_video_coef_601;
   }
}

/* ------------------------------------------------------------------ */
/* BT.601 limited-range I420 -> ABGR words (memory R,G,B,A on LE),     */
/* the packing the animated-WebP stream emits.                         */
/* ------------------------------------------------------------------ */
static INLINE uint32_t rwebm_video_yuv_px(int y, int u, int v,
      const int16_t *k, int argb)
{
   int c = 298 * (y - 16);
   int d = u - 128;
   int e = v - 128;
   int r = (c + k[0] * e + 128) >> 8;
   int g = (c - k[1] * d - k[2] * e + 128) >> 8;
   int b = (c + k[3] * d + 128) >> 8;
   if (r < 0)
      r = 0;
   else if (r > 255)
      r = 255;
   if (g < 0)
      g = 0;
   else if (g > 255)
      g = 255;
   if (b < 0)
      b = 0;
   else if (b > 255)
      b = 255;
   if (argb)
      return 0xFF000000u
           | ((uint32_t)r << 16)
           | ((uint32_t)g << 8)
           |  (uint32_t)b;
   return 0xFF000000u
        | ((uint32_t)b << 16)
        | ((uint32_t)g << 8)
        |  (uint32_t)r;
}

#if defined(__SSE2__)
/* 8 pixels per iteration with pmaddwd pairs. Bit-exact with the scalar
 * path: pmaddwd/paddd/psrad reproduce the integer arithmetic (psrad is
 * an arithmetic shift, as the scalar's >> is on int), and the
 * packs/packus saturation chain is exactly the scalar's clamp - the
 * pre-clamp channel range (about -223..481 for 8-bit input) fits int16
 * without distortion. */
static void rwebm_video_yuv_row_sse2(uint32_t *dr,
      const uint8_t *yr, const uint8_t *ur, const uint8_t *vr, unsigned w, const int16_t *k, int argb)
{
   const __m128i k16   = _mm_set1_epi16(16);
   const __m128i k128  = _mm_set1_epi16(128);
   const __m128i zero  = _mm_setzero_si128();
   const __m128i ones  = _mm_set1_epi16(1);
   const __m128i a255  = _mm_set1_epi8((char)0xFF);
   /* Packs two int16 coefficients into the int32 lane pmaddwd expects,
    * without shifting a negative value (all arithmetic unsigned). */
#define RWEBM_PAIR16(hi, lo) \
   ((int32_t)(((uint32_t)(uint16_t)(int16_t)(hi) << 16) \
            |  (uint32_t)(uint16_t)(int16_t)(lo)))
   const __m128i c_r   = _mm_set1_epi32(RWEBM_PAIR16( k[0], 298));
   const __m128i c_g1  = _mm_set1_epi32(RWEBM_PAIR16(-k[1], 298));
   const __m128i c_g2  = _mm_set1_epi32(RWEBM_PAIR16( 128, -k[2]));
   const __m128i c_b   = _mm_set1_epi32(RWEBM_PAIR16( k[3], 298));
#undef RWEBM_PAIR16
   const __m128i rnd   = _mm_set1_epi32(128);
   unsigned i;

   for (i = 0; i + 8 <= w; i += 8)
   {
      int32_t utmp, vtmp;
      __m128i y8, ysub, u4, v4, d, e;
      __m128i ye_lo, ye_hi, yd_lo, yd_hi, e1_lo, e1_hi;
      __m128i r_lo, r_hi, g_lo, g_hi, b_lo, b_hi;
      __m128i r16, g16, b16, r8, g8, b8, rg, ba;

      /* ysub: 8 x i16 = y - 16 */
      y8   = _mm_loadl_epi64((const __m128i*)(yr + i));
      ysub = _mm_sub_epi16(_mm_unpacklo_epi8(y8, zero), k16);
      /* d/e: 4 chroma samples each duplicated to 8 x i16, minus 128
       * (memcpy avoids an unaligned int load) */
      memcpy(&utmp, ur + (i >> 1), sizeof(utmp));
      memcpy(&vtmp, vr + (i >> 1), sizeof(vtmp));
      u4 = _mm_cvtsi32_si128(utmp);
      v4 = _mm_cvtsi32_si128(vtmp);
      d  = _mm_sub_epi16(
            _mm_unpacklo_epi8(_mm_unpacklo_epi8(u4, u4), zero), k128);
      e  = _mm_sub_epi16(
            _mm_unpacklo_epi8(_mm_unpacklo_epi8(v4, v4), zero), k128);

      ye_lo = _mm_unpacklo_epi16(ysub, e);
      ye_hi = _mm_unpackhi_epi16(ysub, e);
      yd_lo = _mm_unpacklo_epi16(ysub, d);
      yd_hi = _mm_unpackhi_epi16(ysub, d);
      e1_lo = _mm_unpacklo_epi16(e, ones);
      e1_hi = _mm_unpackhi_epi16(e, ones);

      /* r = (298*ysub + 409*e + 128) >> 8 */
      r_lo = _mm_srai_epi32(_mm_add_epi32(
            _mm_madd_epi16(ye_lo, c_r), rnd), 8);
      r_hi = _mm_srai_epi32(_mm_add_epi32(
            _mm_madd_epi16(ye_hi, c_r), rnd), 8);
      /* g = (298*ysub - 100*d - 208*e + 128) >> 8
       *   = (madd(ysub,d; 298,-100) + madd(e,1; -208,128)) >> 8 */
      g_lo = _mm_srai_epi32(_mm_add_epi32(
            _mm_madd_epi16(yd_lo, c_g1), _mm_madd_epi16(e1_lo, c_g2)), 8);
      g_hi = _mm_srai_epi32(_mm_add_epi32(
            _mm_madd_epi16(yd_hi, c_g1), _mm_madd_epi16(e1_hi, c_g2)), 8);
      /* b = (298*ysub + 516*d + 128) >> 8 */
      b_lo = _mm_srai_epi32(_mm_add_epi32(
            _mm_madd_epi16(yd_lo, c_b), rnd), 8);
      b_hi = _mm_srai_epi32(_mm_add_epi32(
            _mm_madd_epi16(yd_hi, c_b), rnd), 8);

      /* Saturating packs implement the 0..255 clamp */
      r16 = _mm_packs_epi32(r_lo, r_hi);
      g16 = _mm_packs_epi32(g_lo, g_hi);
      b16 = _mm_packs_epi32(b_lo, b_hi);
      r8  = _mm_packus_epi16(r16, r16);
      g8  = _mm_packus_epi16(g16, g16);
      b8  = _mm_packus_epi16(b16, b16);

      /* Interleave to memory order R,G,B,A (ABGR words), or B,G,R,A
       * (ARGB words) when the caller asked for ARGB: the swap costs
       * only operand selection, the arithmetic is shared. */
      rg  = _mm_unpacklo_epi8(argb ? b8 : r8, g8);
      ba  = _mm_unpacklo_epi8(argb ? r8 : b8, a255);
      _mm_storeu_si128((__m128i*)(dr + i),
            _mm_unpacklo_epi16(rg, ba));
      _mm_storeu_si128((__m128i*)(dr + i + 4),
            _mm_unpackhi_epi16(rg, ba));
   }
   for (; i < w; i++)
      dr[i] = rwebm_video_yuv_px(yr[i], ur[i >> 1], vr[i >> 1], k, argb);
}
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
/* NEON translation of the SSE2 kernel above: identical integer
 * arithmetic (widening multiply-accumulate into i32, arithmetic shift,
 * saturating narrows for the clamp), so results are byte-identical to
 * the scalar path. */
static void rwebm_video_yuv_row_neon(uint32_t *dr,
      const uint8_t *yr, const uint8_t *ur, const uint8_t *vr, unsigned w,
      const int16_t *kc, int argb)
{
   const int16x8_t k16  = vdupq_n_s16(16);
   const int16x8_t k128 = vdupq_n_s16(128);
   const int32x4_t rnd  = vdupq_n_s32(128);
   unsigned i;

   for (i = 0; i + 8 <= w; i += 8)
   {
      uint8x8_t y8, u8, v8;
      int16x8_t ysub, d, e;
      int32x4_t c_lo, c_hi, r_lo, r_hi, g_lo, g_hi, b_lo, b_hi;
      int16x8_t r16, g16, b16;
      uint8x8x4_t out;
      uint8_t utmp[8], vtmp[8];
      unsigned k;

      y8   = vld1_u8(yr + i);
      ysub = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(y8)), k16);
      for (k = 0; k < 4; k++)
      {
         utmp[2*k] = utmp[2*k+1] = ur[(i >> 1) + k];
         vtmp[2*k] = vtmp[2*k+1] = vr[(i >> 1) + k];
      }
      u8 = vld1_u8(utmp);
      v8 = vld1_u8(vtmp);
      d  = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(u8)), k128);
      e  = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(v8)), k128);

      /* c = 298*ysub + 128 (rounding folded in) */
      c_lo = vmlal_n_s16(rnd, vget_low_s16(ysub),  298);
      c_hi = vmlal_n_s16(rnd, vget_high_s16(ysub), 298);

      r_lo = vshrq_n_s32(vmlal_n_s16(c_lo, vget_low_s16(e),  kc[0]), 8);
      r_hi = vshrq_n_s32(vmlal_n_s16(c_hi, vget_high_s16(e), kc[0]), 8);
      g_lo = vshrq_n_s32(vmlsl_n_s16(vmlsl_n_s16(c_lo,
               vget_low_s16(d), kc[1]), vget_low_s16(e), kc[2]), 8);
      g_hi = vshrq_n_s32(vmlsl_n_s16(vmlsl_n_s16(c_hi,
               vget_high_s16(d), kc[1]), vget_high_s16(e), kc[2]), 8);
      b_lo = vshrq_n_s32(vmlal_n_s16(c_lo, vget_low_s16(d),  kc[3]), 8);
      b_hi = vshrq_n_s32(vmlal_n_s16(c_hi, vget_high_s16(d), kc[3]), 8);

      /* Saturating narrows implement the 0..255 clamp */
      r16 = vcombine_s16(vqmovn_s32(r_lo), vqmovn_s32(r_hi));
      g16 = vcombine_s16(vqmovn_s32(g_lo), vqmovn_s32(g_hi));
      b16 = vcombine_s16(vqmovn_s32(b_lo), vqmovn_s32(b_hi));

      /* R,G,B,A memory order, or B,G,R,A for ARGB words. */
      out.val[0] = vqmovun_s16(argb ? b16 : r16);
      out.val[1] = vqmovun_s16(g16);
      out.val[2] = vqmovun_s16(argb ? r16 : b16);
      out.val[3] = vdup_n_u8(0xFF);
      vst4_u8((uint8_t*)(dr + i), out);
   }
   for (; i < w; i++)
      dr[i] = rwebm_video_yuv_px(yr[i], ur[i >> 1], vr[i >> 1], kc, argb);
}
#endif

static void rwebm_video_blit_i420(uint32_t *dst, unsigned dst_stride,
      unsigned w, unsigned h,
      const uint8_t *y, int ys,
      const uint8_t *u, const uint8_t *v, int uvs,
      unsigned matrix, int argb)
{
   const int16_t *k = rwebm_video_coefs(matrix, h);
   unsigned j;
   for (j = 0; j < h; j++)
   {
      const uint8_t *yr = y + (size_t)j * ys;
      const uint8_t *ur = u + (size_t)(j >> 1) * uvs;
      const uint8_t *vr = v + (size_t)(j >> 1) * uvs;
      uint32_t      *dr = dst + (size_t)j * dst_stride;
#if defined(__SSE2__)
      rwebm_video_yuv_row_sse2(dr, yr, ur, vr, w, k, argb);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
      rwebm_video_yuv_row_neon(dr, yr, ur, vr, w, k, argb);
#else
      {
         unsigned i;
         for (i = 0; i < w; i++)
            dr[i] = rwebm_video_yuv_px(yr[i], ur[i >> 1], vr[i >> 1], k, argb);
      }
#endif
   }
}



/* ------------------------------------------------------------------ */
/* VP9 superframe index (parsed from the trailing marker byte).        */
/* Returns the number of sub-frames and their sizes; 1 = whole chunk.  */
/* ------------------------------------------------------------------ */
#ifdef HAVE_RVP9
static int rwebm_video_vp9_superframe(const uint8_t *data, size_t size,
      size_t *sizes, int max_frames)
{
   uint8_t marker;
   int frames, bytes_per_size, i, k;
   size_t index_size, total;
   const uint8_t *x;

   if (size < 2)
      goto whole;
   marker = data[size - 1];
   if ((marker & 0xe0) != 0xc0)
      goto whole;
   frames         = (marker & 0x7) + 1;
   bytes_per_size = ((marker >> 3) & 0x3) + 1;
   index_size     = 2 + (size_t)frames * bytes_per_size;
   if (size < index_size || data[size - index_size] != marker)
      goto whole;
   if (frames > max_frames)
      goto whole;
   x     = data + size - index_size + 1;
   total = 0;
   for (i = 0; i < frames; i++)
   {
      size_t sz = 0;
      for (k = 0; k < bytes_per_size; k++)
         sz |= (size_t)*x++ << (k * 8);
      sizes[i] = sz;
      total   += sz;
   }
   if (total > size - index_size)
      goto whole;
   return frames;

whole:
   sizes[0] = size;
   return 1;
}
#endif

/* ------------------------------------------------------------------ */
/* Decoder lifecycle                                                   */
/* ------------------------------------------------------------------ */
static bool rwebm_video_stream_open_decoder(rwebm_video_stream_t *s)
{
   switch (s->codec)
   {
      case RWEBM_CODEC_VP8:
         if (!(s->vp8 = rvp8_video_open()))
            return false;
         return true;
#ifdef HAVE_RVP9
      case RWEBM_CODEC_VP9:
         if (!(s->vp9 = (rvp9_dec*)calloc(1, sizeof(*s->vp9))))
            return false;
         return true;
#endif
      default:
         break;
   }
   return false;
}

static void rwebm_video_stream_close_decoder(rwebm_video_stream_t *s)
{
   if (s->vp8)
   {
      rvp8_video_close(s->vp8);
      s->vp8 = NULL;
   }
#ifdef HAVE_RVP9
   if (s->vp9)
   {
      rvp9_free(s->vp9);
      free(s->vp9);
      s->vp9 = NULL;
   }
#endif
}

/* ------------------------------------------------------------------ */
/* Streaming animation                                                 */
/* ------------------------------------------------------------------ */

/* Staged open, so a caller that must not stall (the sliced still-image
 * path) can spread the work over several calls:
 *   begin      demuxer + track selection + timestamp-table allocation
 *   scan_step  up to max_packets of the bounded pre-scan
 *   finish     validate, rewind, decoder + frame-canvas allocation
 * rwebm_video_stream_open composes all three for callers that want the
 * one-shot behaviour. */

static rwebm_video_stream_t *rwebm_video_stream_open_begin(
      const uint8_t *buf, size_t len, size_t avail, int *need_more)
{
   rwebm_video_stream_t *s;
   const rwebm_track *trk = NULL;
   int i, num_tracks;

   if (need_more)
      *need_more = 0;
   if (!buf || !len)
      return NULL;

   if (!(s = (rwebm_video_stream_t*)calloc(1, sizeof(*s))))
      return NULL;

   if (!(s->demux = rwebm_open_memory_avail(buf, len, avail, need_more)))
      goto fail;

   /* Pick the first video track whose codec we can decode. */
   s->track   = -1;
   num_tracks = rwebm_num_tracks(s->demux);
   for (i = 0; i < num_tracks; i++)
   {
      const rwebm_track *t = rwebm_get_track(s->demux, i);
      if (!t || t->type != RWEBM_TRACK_VIDEO)
         continue;
      if (t->width < 1 || t->height < 1)
         continue;
      if (t->codec != RWEBM_CODEC_VP8
#ifdef HAVE_RVP9
          && t->codec != RWEBM_CODEC_VP9
#endif
         )
         continue;
      s->track  = i;
      s->codec  = t->codec;
      trk       = t;
      break;
   }
   if (s->track < 0)
      goto fail;

   s->width  = trk->width;
   s->height = trk->height;

   if (!(s->ts = (int64_t*)malloc(RWEBM_VIDEO_MAX_TS * sizeof(int64_t))))
      goto fail;

   return s;

fail:
   rwebm_video_stream_close(s);
   return NULL;
}

/* Pre-scan: count the track's packets and record their timestamps so
 * frame durations come straight from the container. This walks block
 * headers only (no decode), but Matroska has no sample table, so the
 * walk touches every cluster in the file.  Stop at the timestamp
 * table's capacity: frames past the cap reuse the last stored delta
 * anyway, and the only external consumer of num_frames (the >= 2
 * animation admission in gfx_thumbnail) is unaffected by the count
 * saturating.  Unbounded, this loop swept the whole buffer of a
 * long recording on the thread that opened the stream.
 *
 * Walks at most max_packets container packets (any track; <= 0 means
 * no limit) and returns 1 when the scan is complete, 0 when there is
 * more to walk. */
static int rwebm_video_stream_scan_step(rwebm_video_stream_t *s,
      int max_packets)
{
   rwebm_packet pkt;
   int walked = 0;

   while (s->num_frames < RWEBM_VIDEO_MAX_TS)
   {
      int r;
      if (max_packets > 0 && walked >= max_packets)
         return 0;
      r = rwebm_read_packet(s->demux, &pkt);
      if (r == RWEBM_READ_AGAIN)
         return 2;   /* blocked at the partial-read wall; resumable */
      if (r != 1)
         break;
      walked++;
      if (pkt.track != s->track)
         continue;
      s->ts[s->ts_count++] = pkt.timestamp;
      s->num_frames++;
   }
   return 1;
}

static int rwebm_video_stream_open_finish(rwebm_video_stream_t *s)
{
   if (s->num_frames < 1)
      return -1;
   rwebm_rewind(s->demux);

   if (!rwebm_video_stream_open_decoder(s))
      return -1;

   if (!(s->frame = (uint32_t*)malloc(
         (size_t)s->width * s->height * sizeof(uint32_t))))
      return -1;

   return 0;
}

rwebm_video_stream_t *rwebm_video_stream_open(const uint8_t *buf,
      size_t len)
{
   rwebm_video_stream_t *s;

   if (!(s = rwebm_video_stream_open_begin(buf, len, len, NULL)))
      return NULL;
   rwebm_video_stream_scan_step(s, 0);
   if (rwebm_video_stream_open_finish(s) != 0)
   {
      rwebm_video_stream_close(s);
      return NULL;
   }
   return s;
}

void rwebm_video_stream_close(rwebm_video_stream_t *s)
{
   if (!s)
      return;
   rwebm_video_stream_close_decoder(s);
   if (s->demux)
      rwebm_close(s->demux);
   free(s->ts);
   free(s->frame);
   free(s);
}

void rwebm_video_stream_get_info(const rwebm_video_stream_t *s,
      unsigned *width, unsigned *height, int *num_frames, int *loop_count)
{
   if (!s)
      return;
   if (width)
      *width      = s->width;
   if (height)
      *height     = s->height;
   if (num_frames)
      *num_frames = s->num_frames;
   if (loop_count)
      *loop_count = 0;   /* video loops indefinitely */
}

void rwebm_video_stream_set_argb(rwebm_video_stream_t *s, int argb)
{
   if (s)
      s->emit_argb = argb ? 1 : 0;
}

void rwebm_video_stream_set_avail(rwebm_video_stream_t *s, size_t avail)
{
   if (s)
      rwebm_set_avail(s->demux, avail);
}

/* Display duration of packet 'idx', in ms, from the pre-scanned
 * timestamp table; 0 when unknown (caller applies its default). */
static int rwebm_video_duration_ms(const rwebm_video_stream_t *s, int idx)
{
   /* Quantise against the accumulated timeline, not per delta: flooring
    * each delta independently loses the fractional millisecond every
    * frame - a 33.333 ms (30 fps) stream came out 33+33+33..., running
    * one percent fast and drifting further each loop. Differencing the
    * floored absolute times emits 33/33/34 so the sum stays within a
    * millisecond of the container's timeline. */
   int64_t t0, t1;
   if (idx + 1 < s->ts_count)
   { t0 = s->ts[idx]; t1 = s->ts[idx + 1]; }
   else if (s->ts_count >= 2)
   { t0 = s->ts[s->ts_count - 2]; t1 = s->ts[s->ts_count - 1]; }
   else
      return 0;
   if (t1 <= t0)
      return 0;
   return (int)(t1 / 1000000 - t0 / 1000000);
}

/* Decode one demuxed packet into s->frame. Returns 1 when a picture was
 * produced, 0 when the packet decoded but is not displayed, -1 on a
 * decode error. */
static int rwebm_video_decode_packet(rwebm_video_stream_t *s,
      const rwebm_packet *pkt)
{
   /* A decoder can be absent if the re-open in rewind hit OOM. */
   if (!s->vp8
#ifdef HAVE_RVP9
       && !s->vp9
#endif
      )
      return -1;
#ifdef HAVE_RVP9
   if (s->codec == RWEBM_CODEC_VP9)
   {
      size_t sizes[RWEBM_VIDEO_MAX_SUPER];
      const uint8_t *frame = pkt->data;
      int nf = rwebm_video_vp9_superframe(pkt->data, pkt->size,
            sizes, RWEBM_VIDEO_MAX_SUPER);
      int i, r, show, last_show = -1;
      for (i = 0; i < nf; i++)
      {
         r = rvp9_decode_frame(s->vp9, frame, sizes[i], &show);
         if (r < 0)
            return -1;
         if (show >= 0)
            last_show = show;
         frame += sizes[i];
      }
      if (last_show >= 0)
      {
         const rvp9_fb *fb = &s->vp9->fbs[last_show];
         unsigned w = (unsigned)fb->w < s->width  ? (unsigned)fb->w : s->width;
         unsigned h = (unsigned)fb->h < s->height ? (unsigned)fb->h : s->height;
         if (s->vp9->hd.bit_depth == 10)
         {
            const rwebm_track *ct = rwebm_get_track(s->demux, s->track);
            if (s->want10)
            {
               /* Native 10-bit thumbnail: packed XRGB2101010, SDR-encoded
                * at 10-bit precision (same colour as the 8-bit path). */
               rwebm_video_blit_i420_10bit(s->frame, s->width, w, h,
                     (const uint16_t*)fb->y, s->vp9->ys,
                     (const uint16_t*)fb->u, (const uint16_t*)fb->v,
                     s->vp9->uvs,
                     ct ? ct->matrix_coefficients : 0,
                     ct ? ct->transfer_characteristics : 0,
                     ct ? ct->colour_range : 0,
                     ct ? ct->max_cll : 0);
               s->is10 = 1;
            }
            else
               rwebm_video_blit_i420_hbd(s->frame, s->width, w, h,
                     (const uint16_t*)fb->y, s->vp9->ys,
                     (const uint16_t*)fb->u, (const uint16_t*)fb->v,
                     s->vp9->uvs,
                     ct ? ct->matrix_coefficients : 0,
                     ct ? ct->transfer_characteristics : 0,
                     ct ? ct->colour_range : 0,
                     ct ? ct->max_cll : 0, s->emit_argb ? 0 : 1);
         }
         else
         {
            const rwebm_track *ct = rwebm_get_track(s->demux, s->track);
            rwebm_video_blit_i420(s->frame, s->width, w, h,
                  fb->y, s->vp9->ys, fb->u, fb->v, s->vp9->uvs,
                  ct ? ct->matrix_coefficients : 0, s->emit_argb);
         }
         return 1;
      }
      return 0;
   }
#endif
   if (s->codec == RWEBM_CODEC_VP8)
   {
      const uint8_t *y, *u, *v;
      int ys, uvs, w, h, cw, ch;
      /* VP8 frame tag: bit 4 of byte 0 is show_frame. */
      int shown = (pkt->size > 0) && (pkt->data[0] & 0x10);
      if (rvp8_video_decode(s->vp8, pkt->data, pkt->size) != 0)
         return -1;
      if (!shown)
         return 0;
      y = rvp8_video_plane(s->vp8, 0, &ys, &w, &h);
      u = rvp8_video_plane(s->vp8, 1, &uvs, &cw, &ch);
      v = rvp8_video_plane(s->vp8, 2, &uvs, &cw, &ch);
      if (!y || !u || !v)
         return -1;
      if ((unsigned)w > s->width)
         w = (int)s->width;
      if ((unsigned)h > s->height)
         h = (int)s->height;
      {
         const rwebm_track *ct = rwebm_get_track(s->demux, s->track);
         rwebm_video_blit_i420(s->frame, s->width,
               (unsigned)w, (unsigned)h, y, ys, u, v, uvs,
               ct ? ct->matrix_coefficients : 0, s->emit_argb);
      }
      return 1;
   }
   return -1;
}

/* Advance by exactly one of the chosen track's packets (skipping other
 * tracks' packets, which are header reads only).  Returns 1 when a
 * displayed picture is in s->frame (duration written), 0 when the
 * packet decoded but is not displayed, -1 at the end of a pass or on
 * a decode error. */
static int rwebm_video_stream_step(rwebm_video_stream_t *s,
      int *duration_ms)
{
   rwebm_packet pkt;

   for (;;)
   {
      int idx, r;
      r = rwebm_read_packet(s->demux, &pkt);
      if (r == RWEBM_READ_AGAIN)
         return 2;   /* not yet resident; nothing consumed - retry
                        after rwebm_video_stream_set_avail */
      if (r != 1)
         break;
      if (pkt.track != s->track)
         continue;
      idx = s->pkt_idx++;
      r   = rwebm_video_decode_packet(s, &pkt);
      if (r < 0)
         return -1;      /* decode error: end the animation */
      if (r == 0)
         return 0;       /* non-shown frame                  */
      if (duration_ms)
         *duration_ms = rwebm_video_duration_ms(s, idx);
      return 1;
   }
   return -1;             /* end of one pass */
}

const uint32_t *rwebm_video_stream_next(rwebm_video_stream_t *s,
      int *duration_ms)
{
   int r;

   if (!s)
      return NULL;

   while ((r = rwebm_video_stream_step(s, duration_ms)) == 0)
      ;                   /* non-shown frame: keep going      */
   return (r == 1) ? s->frame : NULL;
}

void rwebm_video_stream_rewind(rwebm_video_stream_t *s)
{
   if (!s)
      return;
   rwebm_rewind(s->demux);
   s->pkt_idx = 0;
   /* The stream restarts at a key frame, so a fresh decoder (empty
    * reference chain, default probabilities) is the correct state. */
   rwebm_video_stream_close_decoder(s);
   rwebm_video_stream_open_decoder(s);
}

/* ------------------------------------------------------------------ */
/* Still image (first displayed frame)                                 */
/* ------------------------------------------------------------------ */
rwebm_video_t *rwebm_video_alloc(void)
{
   return (rwebm_video_t*)calloc(1, sizeof(rwebm_video_t));
}

void rwebm_video_free(rwebm_video_t *webm)
{
   if (!webm)
      return;
   if (webm->stream)
      rwebm_video_stream_close(webm->stream);
   free(webm);
}

rwebm_video_stream_t *rwebm_video_detach_stream(rwebm_video_t *webm)
{
   rwebm_video_stream_t *s;
   if (!webm || !webm->stream)
      return NULL;
   /* A stream mid-build in the sliced still decode is not complete
    * (pre-scan or decoder setup may be pending); never hand it out. */
   if (webm->still_stage != RWEBM_VIDEO_STILL_IDLE)
      return NULL;
   s            = webm->stream;
   webm->stream = NULL;
   /* The animation consumers upload plain 8-bit frames; the still may
    * have been decoded 10-bit, so drop the request before handing the
    * stream over (each frame re-blits, no stale pixels survive).  The
    * channel order likewise resets to the documented default; adopters
    * re-select per frame via rwebm_video_stream_set_argb. */
   s->want10    = 0;
   s->is10      = 0;
   s->emit_argb = 0;
   return s;
}

bool rwebm_video_set_buf_ptr(rwebm_video_t *webm, void *data, size_t len)
{
   if (!webm)
      return false;
   webm->buf = (const uint8_t*)data;
   webm->len = len;
   return true;
}

/* Ask the thumbnail decoder to emit packed XRGB2101010 for 10-bit HDR
 * sources (it silently keeps 8-bit output for 8-bit sources). */
void rwebm_video_set_want_10bit(rwebm_video_t *webm, int want)
{
   if (webm)
      webm->want10 = want ? 1 : 0;
}

void rwebm_video_set_avail(rwebm_video_t *webm, size_t avail)
{
   if (!webm)
      return;
   webm->partial = 1;
   if (avail > webm->len)
      avail = webm->len;
   if (avail > webm->avail)   /* monotonic */
      webm->avail = avail;
   if (webm->stream)
      rwebm_video_stream_set_avail(webm->stream, webm->avail);
}

/* True if the last rwebm_video_process_image() wrote packed XRGB2101010. */
bool rwebm_video_is_10bit(const rwebm_video_t *webm)
{
   return webm && webm->last_10bit;
}

/* Sliced still-image decode: each call performs one bounded unit of
 * work and returns IMAGE_PROCESS_NEXT until the first displayed frame
 * is ready, at which point the pixels are handed out and
 * IMAGE_PROCESS_END is returned - the contract rpng established, which
 * task_image drives against a per-display-frame time budget.  The
 * slices are: stream open (demuxer + track selection), pre-scan in
 * RWEBM_VIDEO_SCAN_SLICE-packet chunks, decoder setup, then one coded
 * packet per call (VP9 alt-refs and other non-shown frames each take
 * their own slice) until a picture is displayed.  Previously all of
 * this ran in a single process call, which stalled every sibling task
 * behind the decode (and the main loop, on builds without a threaded
 * task queue). */
int rwebm_video_process_image(rwebm_video_t *webm, void **buf,
      size_t len, unsigned *width, unsigned *height, bool supports_rgba)
{
   rwebm_video_stream_t *s;
   const uint32_t *frame;
   uint32_t *out;
   size_t n;
   int r;
   int duration_ms = 0;

   (void)len;

   if (!webm || !webm->buf || !buf)
      return IMAGE_PROCESS_ERROR;

   /* A partial reader raises webm->avail between calls; push it down
    * so blocked stages can make progress this call. */
   if (webm->stream && webm->partial)
      rwebm_video_stream_set_avail(webm->stream, webm->avail);

   switch (webm->still_stage)
   {
      case RWEBM_VIDEO_STILL_IDLE:
      {
         int    need_more = 0;
         size_t avail     = webm->partial ? webm->avail : webm->len;
         /* Defensive: a repeated process call re-opens from scratch. */
         if (webm->stream)
         {
            rwebm_video_stream_close(webm->stream);
            webm->stream = NULL;
         }
         if (!(webm->stream = rwebm_video_stream_open_begin(
               webm->buf, webm->len, avail, &need_more)))
            /* The EBML header/Tracks are still arriving: wait for more
             * bytes rather than failing. */
            return need_more ? IMAGE_PROCESS_WAIT : IMAGE_PROCESS_ERROR;
         webm->still_stage = RWEBM_VIDEO_STILL_SCAN;
         return IMAGE_PROCESS_NEXT;
      }

      case RWEBM_VIDEO_STILL_SCAN:
      {
         int sr = rwebm_video_stream_scan_step(webm->stream,
               RWEBM_VIDEO_SCAN_SLICE);
         if (sr == 0)
            return IMAGE_PROCESS_NEXT;
         if (sr == 2)
         {
            /* The pre-scan hit the partial-read wall.  Timestamps live
             * in the block headers, so unlike MP4 the scan cannot run
             * ahead of the media bytes; rather than hold the still
             * hostage to the whole file, proceed once two frames are
             * known (enough to derive a first-frame duration) with a
             * table truncated at the wall - the per-frame duration
             * helper already degrades to its last-interval estimate
             * past the table's end.  Below two frames the first
             * displayed frame is not decodable anyway: wait. */
            if (webm->stream->num_frames < 2)
               return IMAGE_PROCESS_WAIT;
         }
         if (rwebm_video_stream_open_finish(webm->stream) != 0)
            goto fail;
         /* Request 10-bit output; the stream honours it only for
          * 10-bit sources.  For 8-bit output, have the blit emit the
          * caller's channel order directly (supports_rgba is sampled
          * once per transfer by task_image, so the value seen here
          * holds for the END slice's copy below). */
         webm->stream->want10    = webm->want10;
         webm->stream->emit_argb = supports_rgba ? 0 : 1;
         webm->still_stage       = RWEBM_VIDEO_STILL_DECODE;
         return IMAGE_PROCESS_NEXT;
      }

      case RWEBM_VIDEO_STILL_DECODE:
         break;

      default:
         return IMAGE_PROCESS_ERROR;
   }

   s = webm->stream;
   if ((r = rwebm_video_stream_step(s, &duration_ms)) == 0)
      return IMAGE_PROCESS_NEXT;   /* non-shown frame decoded */
   if (r == 2)
      /* The next block's bytes have not been read yet; nothing was
       * consumed - resume here once more of the file has arrived. */
      return IMAGE_PROCESS_WAIT;
   if (r < 0)
      goto fail;
   frame = s->frame;

   webm->last_10bit = s->is10;

   n = (size_t)s->width * s->height;
   if (!(out = (uint32_t*)malloc(n * sizeof(uint32_t))))
      goto fail;

   /* The canvas is already in the caller's channel order: 10-bit
    * output is packed XRGB2101010, and the 8-bit blit emitted ARGB or
    * ABGR words per supports_rgba (set at the scan/decode transition
    * above), so the copy is verbatim in every case - the per-pixel
    * R/B swizzle this replaced was a full extra pass over the frame. */
   memcpy(out, frame, n * sizeof(uint32_t));

   if (width)
      *width  = s->width;
   if (height)
      *height = s->height;
   *buf = out;

   /* Keep the stream (positioned just past the first displayed frame)
    * so the caller can detach it and continue the video as an
    * animation without re-opening the file.  If nobody detaches it,
    * rwebm_video_free closes it. */
   webm->still_stage = RWEBM_VIDEO_STILL_IDLE;
   return IMAGE_PROCESS_END;

fail:
   rwebm_video_stream_close(webm->stream);
   webm->stream      = NULL;
   webm->still_stage = RWEBM_VIDEO_STILL_IDLE;
   return IMAGE_PROCESS_ERROR;
}
