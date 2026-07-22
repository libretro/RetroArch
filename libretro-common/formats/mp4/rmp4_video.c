/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------
 * The following license statement only applies to this file (rmp4_video.c).
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

/* MP4 video-to-image glue: rmp4 demuxer + rvp8/rvp9 decoders exposed
 * through the still-image and streaming-animation contracts that
 * image_transfer.c dispatches on (see rmp4_video.h).  The structure
 * mirrors rwebm_video.c with the demuxer swapped. */

#include <stdlib.h>
#include <string.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

#include <retro_inline.h>

#include <formats/image.h>
#include <formats/rmp4.h>
#include <formats/rvp8.h>
#ifdef HAVE_RVP9
#include <formats/rvp9.h>
/* The 10-bit / HDR I420->RGB blits are shared (image_hdr_blit.c) but still
 * declared in rwebm_video.h; the implementation is demuxer-independent. */
#include <formats/rwebm_video.h>
#endif
#include <formats/rh264.h>
#include <formats/rmp4_video.h>

/* Per-packet timestamps are pre-scanned at open so every frame's display
 * duration is known without lookahead decoding; cap the table so a
 * pathological stream cannot balloon it. Frames past the cap reuse the
 * last stored delta. */
#define RMP4_VIDEO_MAX_TS       8192

/* A VP9 superframe carries at most 8 sub-frames. */
#define RMP4_VIDEO_MAX_SUPER    8

struct rmp4_video_stream
{
   rmp4_t      *demux;
   rvp8_video  *vp8;
#ifdef HAVE_RVP9
   rvp9_dec    *vp9;
#endif
   rh264_video *h264;
   uint32_t    *frame;      /* width * height ABGR words              */
   int64_t     *ts;         /* pre-scanned packet timestamps (ns)     */
   int          ts_count;   /* entries stored in ts                   */
   int          num_frames; /* total video packets in the stream      */
   int          disp_idx;   /* ordinal of the next displayed picture  */
   /* The displayed picture most recently produced by a step.  Its
    * planes stay inside the decoder until the next decode or drain
    * call, so colour conversion into 'frame' is deferred to
    * rmp4_video_stream_render and never happens at all for frames a
    * caller passes over with rmp4_video_stream_skip (this replaces
    * the old seek-only 'catchup' blit suppression). */
   int          rndr_kind;     /* 0 none, 1 vp8, 2 vp9, 3 h264       */
   int          rndr_vp9_show; /* fbs index of the vp9 picture       */
   int          wait_key;   /* a reference failed; hold out for a key */
   int          track;      /* index of the chosen video track        */
   unsigned     matrix;     /* colr matrix_coefficients; 0 untagged   */
   unsigned     transfer;   /* colr transfer_characteristics          */
   unsigned     range;      /* colr full-range flag                   */
   enum rmp4_codec codec;
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
   RMP4_VIDEO_STILL_IDLE = 0,    /* nothing in progress (or done)      */
   RMP4_VIDEO_STILL_SCAN,        /* stream open, pre-scan running      */
   RMP4_VIDEO_STILL_DECODE       /* decoding to the first shown frame  */
};

/* Container packets walked per pre-scan slice of the sliced still
 * path: cheap header parses, sized so a slice stays well under a
 * display frame even on weak hardware while the full bounded scan
 * still completes in a handful of slices. */
#define RMP4_VIDEO_SCAN_SLICE    1024

struct rmp4_video
{
   const uint8_t *buf;
   size_t         len;
   /* The stream opened for the still frame, kept so a caller that wants
    * to continue the video as an animation can detach it instead of
    * re-opening (and re-pre-scanning) the file.  Owned by this handle
    * until rmp4_video_detach_stream; closed by rmp4_video_free
    * otherwise.  Borrows 'buf', which must outlive it either way.
    * While still_stage is not IDLE this holds the partially-opened
    * stream the sliced still decode is building. */
   rmp4_video_stream_t *stream;
   int            still_stage; /* enum above */
   int            want10;     /* caller requested 10-bit thumbnail output */
   int            last_10bit; /* last processed frame was XRGB2101010 */
   /* Bytes of 'buf' actually read so far, for decoding a still from a
    * file whose read is in progress: 0 means fully resident (the
    * default; set_buf_ptr keeps it), and the still decode returns
    * IMAGE_PROCESS_WAIT instead of erroring when it runs into the
    * wall.  Raised by rmp4_video_set_avail. */
   size_t         avail;
   int            partial;    /* set_avail was called: 'avail' is live
                                 (0 is a valid value - nothing read
                                 yet - so it cannot double as the
                                 unset sentinel) */
};

static int rmp4_video_ts_cmp(const void *a, const void *b)
{
   int64_t x = *(const int64_t*)a, y = *(const int64_t*)b;
   return (x > y) - (x < y);
}

/* ------------------------------------------------------------------ */
/* Limited-range I420 -> ABGR words (memory R,G,B,A on LE).            */
/* 8-bit coefficient sets, <<8: {re, gd, ge, bd}, same values as the   */
/* webm converter. Untagged content defaults to BT.601 below 720 lines */
/* and BT.709 at or above, matching industry convention.               */
/* ------------------------------------------------------------------ */
static const int16_t rmp4_video_coef_601[4]  = { 409, 100, 208, 516 };
static const int16_t rmp4_video_coef_709[4]  = { 459,  55, 136, 541 };
static const int16_t rmp4_video_coef_2020[4] = { 431,  48, 167, 548 };

static const int16_t *rmp4_video_coefs(unsigned matrix, unsigned height)
{
   switch (matrix)
   {
      case 1:            return rmp4_video_coef_709;
      case 5: case 6:    return rmp4_video_coef_601;
      case 9: case 10:   return rmp4_video_coef_2020;
      default:           return height >= 720
                            ? rmp4_video_coef_709 : rmp4_video_coef_601;
   }
}

static INLINE uint32_t rmp4_video_yuv_px(int y, int u, int v,
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
static void rmp4_video_yuv_row_sse2(uint32_t *dr,
      const uint8_t *yr, const uint8_t *ur, const uint8_t *vr, unsigned w,
      const int16_t *k, int argb)
{
   const __m128i k16   = _mm_set1_epi16(16);
   const __m128i k128  = _mm_set1_epi16(128);
   const __m128i zero  = _mm_setzero_si128();
   const __m128i ones  = _mm_set1_epi16(1);
   const __m128i a255  = _mm_set1_epi8((char)0xFF);
   /* Packs two int16 coefficients into the int32 lane pmaddwd expects,
    * without shifting a negative value (all arithmetic unsigned). */
#define RMP4_PAIR16(hi, lo) \
   ((int32_t)(((uint32_t)(uint16_t)(int16_t)(hi) << 16) \
            |  (uint32_t)(uint16_t)(int16_t)(lo)))
   const __m128i c_r   = _mm_set1_epi32(RMP4_PAIR16( k[0], 298));
   const __m128i c_g1  = _mm_set1_epi32(RMP4_PAIR16(-k[1], 298));
   const __m128i c_g2  = _mm_set1_epi32(RMP4_PAIR16( 128, -k[2]));
   const __m128i c_b   = _mm_set1_epi32(RMP4_PAIR16( k[3], 298));
#undef RMP4_PAIR16
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
      dr[i] = rmp4_video_yuv_px(yr[i], ur[i >> 1], vr[i >> 1], k, argb);
}
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
/* NEON translation of the SSE2 kernel above: identical integer
 * arithmetic (widening multiply-accumulate into i32, arithmetic shift,
 * saturating narrows for the clamp), so results are byte-identical to
 * the scalar path. */
static void rmp4_video_yuv_row_neon(uint32_t *dr,
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

      r_lo = vshrq_n_s32(vmlal_n_s16(c_lo, vget_low_s16(e),   kc[0]), 8);
      r_hi = vshrq_n_s32(vmlal_n_s16(c_hi, vget_high_s16(e),  kc[0]), 8);
      g_lo = vshrq_n_s32(vmlsl_n_s16(vmlsl_n_s16(c_lo,
               vget_low_s16(d),  kc[1]), vget_low_s16(e),  kc[2]), 8);
      g_hi = vshrq_n_s32(vmlsl_n_s16(vmlsl_n_s16(c_hi,
               vget_high_s16(d), kc[1]), vget_high_s16(e), kc[2]), 8);
      b_lo = vshrq_n_s32(vmlal_n_s16(c_lo, vget_low_s16(d),   kc[3]), 8);
      b_hi = vshrq_n_s32(vmlal_n_s16(c_hi, vget_high_s16(d),  kc[3]), 8);

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
      dr[i] = rmp4_video_yuv_px(yr[i], ur[i >> 1], vr[i >> 1], kc, argb);
}
#endif

static void rmp4_video_blit_yuv(uint32_t *dst, unsigned dst_stride,
      unsigned w, unsigned h,
      const uint8_t *y, int ys,
      const uint8_t *u, const uint8_t *v, int uvs,
      unsigned matrix, int cvsh, int argb)
{
   const int16_t *k = rmp4_video_coefs(matrix, h);
   unsigned j;
   for (j = 0; j < h; j++)
   {
      const uint8_t *yr = y + (size_t)j * ys;
      const uint8_t *ur = u + (size_t)(j >> cvsh) * uvs;
      const uint8_t *vr = v + (size_t)(j >> cvsh) * uvs;
      uint32_t      *dr = dst + (size_t)j * dst_stride;
#if defined(__SSE2__)
      rmp4_video_yuv_row_sse2(dr, yr, ur, vr, w, k, argb);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
      rmp4_video_yuv_row_neon(dr, yr, ur, vr, w, k, argb);
#else
      {
         unsigned i;
         for (i = 0; i < w; i++)
            dr[i] = rmp4_video_yuv_px(yr[i], ur[i >> 1], vr[i >> 1], k, argb);
      }
#endif
   }
}

/* 4:2:0: two luma rows share a chroma row. */
static void rmp4_video_blit_i420(uint32_t *dst, unsigned dst_stride,
      unsigned w, unsigned h,
      const uint8_t *y, int ys,
      const uint8_t *u, const uint8_t *v, int uvs,
      unsigned matrix, int argb)
{
   rmp4_video_blit_yuv(dst, dst_stride, w, h, y, ys, u, v, uvs, matrix, 1,
         argb);
}

/* ------------------------------------------------------------------ */
/* VP9 superframe index (parsed from the trailing marker byte).        */
/* Returns the number of sub-frames and their sizes; 1 = whole chunk.  */
/* ------------------------------------------------------------------ */
#ifdef HAVE_RVP9
static int rmp4_video_vp9_superframe(const uint8_t *data, size_t size,
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
static bool rmp4_video_stream_open_decoder(rmp4_video_stream_t *s)
{
   switch (s->codec)
   {
      case RMP4_CODEC_VP8:
         if (!(s->vp8 = rvp8_video_open()))
            return false;
         return true;
#ifdef HAVE_RVP9
      case RMP4_CODEC_VP9:
         if (!(s->vp9 = (rvp9_dec*)calloc(1, sizeof(*s->vp9))))
            return false;
         return true;
#endif
      case RMP4_CODEC_H264:
      {
         const rmp4_track *t = rmp4_get_track(s->demux, s->track);
         if (!(s->h264 = rh264_video_open()))
            return false;
         if (t && t->codec_private && t->codec_private_size)
            rh264_video_set_extradata(s->h264, t->codec_private,
                  t->codec_private_size);
         return true;
      }
      default:
         break;
   }
   return false;
}

static void rmp4_video_stream_close_decoder(rmp4_video_stream_t *s)
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
   if (s->h264)
   {
      rh264_video_close(s->h264);
      s->h264 = NULL;
   }
}

/* ------------------------------------------------------------------ */
/* Streaming animation                                                 */
/* ------------------------------------------------------------------ */

/* Staged open, mirroring the WebM glue, so the sliced still-image path
 * can spread the work over several calls:
 *   begin      demuxer + track selection + timestamp-table allocation
 *   scan_step  up to max_packets of the bounded pre-scan
 *   finish     validate, sort composition times, rewind, decoder +
 *              frame-canvas allocation
 * rmp4_video_stream_open composes all three for callers that want the
 * one-shot behaviour. */

static rmp4_video_stream_t *rmp4_video_stream_open_begin(
      const uint8_t *buf, size_t len, size_t avail, int *need_more)
{
   rmp4_video_stream_t *s;
   const rmp4_track *trk = NULL;
   int i, num_tracks;

   if (need_more)
      *need_more = 0;
   if (!buf || !len)
      return NULL;

   if (!(s = (rmp4_video_stream_t*)calloc(1, sizeof(*s))))
      return NULL;

   if (!(s->demux = rmp4_open_memory_avail(buf, len, avail, need_more)))
      goto fail;

   /* Pick the first video track whose codec we can decode. */
   s->track   = -1;
   num_tracks = rmp4_num_tracks(s->demux);
   for (i = 0; i < num_tracks; i++)
   {
      const rmp4_track *t = rmp4_get_track(s->demux, i);
      if (!t || t->type != RMP4_TRACK_VIDEO)
         continue;
      if (t->width < 1 || t->height < 1)
         continue;
      if (t->codec != RMP4_CODEC_VP8
#ifdef HAVE_RVP9
          && t->codec != RMP4_CODEC_VP9
#endif
          && t->codec != RMP4_CODEC_H264
         )
         continue;
      s->track  = i;
      s->codec  = t->codec;
      s->matrix   = t->matrix_coefficients;
      s->transfer = t->transfer_characteristics;
      s->range    = t->full_range;
      trk       = t;
      break;
   }
   if (s->track < 0)
      goto fail;

   s->width  = trk->width;
   s->height = trk->height;

   if (!(s->ts = (int64_t*)malloc(RMP4_VIDEO_MAX_TS * sizeof(int64_t))))
      goto fail;

   return s;

fail:
   rmp4_video_stream_close(s);
   return NULL;
}

/* Pre-scan: count the track's packets and record their timestamps so
 * frame durations come straight from the container. This walks the
 * sample table only (no decode).  Stop at the table's capacity,
 * mirroring the WebM glue: frames past the cap reuse the last stored
 * delta, and num_frames saturates (its only external consumer is the
 * >= 2 animation admission).  For a plain MP4 the walk is cheap
 * (moov-resident sample tables), but a fragmented file interleaves
 * moof boxes with the media data, so an unbounded walk swept the
 * whole buffer there too.
 *
 * Walks at most max_packets container packets (any track; <= 0 means
 * no limit) and returns 1 when the scan is complete, 0 when there is
 * more to walk. */
static int rmp4_video_stream_scan_step(rmp4_video_stream_t *s,
      int max_packets)
{
   /* Timestamps come straight from the moov sample tables - the old
    * packet walk produced exactly these values (it recorded
    * pkt.timestamp for the video track only, in cursor order) but
    * went through rmp4_read_packet, which now gates on each sample's
    * media bytes being resident; the tables need no media bytes at
    * all, so a partially-read file can pre-scan the moment its moov
    * is parsed. */
   uint32_t       count = 0;
   const int64_t *pts   = rmp4_track_pts(s->demux, s->track, &count);
   int            walked = 0;

   if (!pts)
      return 1;
   while (s->num_frames < RMP4_VIDEO_MAX_TS
         && (uint32_t)s->ts_count < count)
   {
      if (max_packets > 0 && walked >= max_packets)
         return 0;
      s->ts[s->ts_count] = pts[s->ts_count];
      s->ts_count++;
      s->num_frames++;
      walked++;
   }
   return 1;
}

static int rmp4_video_stream_open_finish(rmp4_video_stream_t *s)
{
   if (s->num_frames < 1)
      return -1;
   /* The sample table lists composition times in decode order; with
    * B-frames the decoder hands pictures out reordered by those times,
    * so sort them into the presentation timeline the displayed frames
    * actually follow. Without this a 30 fps recording with a reorder
    * depth of two was paced 133/0/0/66 ms instead of 33 ms a frame. */
   qsort(s->ts, (size_t)s->ts_count, sizeof(*s->ts), rmp4_video_ts_cmp);
   rmp4_rewind(s->demux);

   if (!rmp4_video_stream_open_decoder(s))
      return -1;

   if (!(s->frame = (uint32_t*)malloc(
         (size_t)s->width * s->height * sizeof(uint32_t))))
      return -1;

   return 0;
}

rmp4_video_stream_t *rmp4_video_stream_open(const uint8_t *buf,
      size_t len)
{
   rmp4_video_stream_t *s;

   if (!(s = rmp4_video_stream_open_begin(buf, len, len, NULL)))
      return NULL;
   rmp4_video_stream_scan_step(s, 0);
   if (rmp4_video_stream_open_finish(s) != 0)
   {
      rmp4_video_stream_close(s);
      return NULL;
   }
   return s;
}

rmp4_video_stream_t *rmp4_video_stream_open_avail(const uint8_t *buf,
      size_t len, size_t avail, int *need_more)
{
   rmp4_video_stream_t *s;

   if (need_more)
      *need_more = 0;
   if (!(s = rmp4_video_stream_open_begin(buf, len, avail, need_more)))
      return NULL;
   /* The pre-scan reads the moov sample tables (no media bytes), so
    * once the open itself succeeded it always completes. */
   rmp4_video_stream_scan_step(s, 0);
   if (rmp4_video_stream_open_finish(s) != 0)
   {
      rmp4_video_stream_close(s);
      return NULL;
   }
   return s;
}

void rmp4_video_stream_close(rmp4_video_stream_t *s)
{
   if (!s)
      return;
   rmp4_video_stream_close_decoder(s);
   if (s->demux)
      rmp4_close(s->demux);
   free(s->ts);
   free(s->frame);
   free(s);
}

void rmp4_video_stream_get_info(const rmp4_video_stream_t *s,
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

void rmp4_video_stream_set_argb(rmp4_video_stream_t *s, int argb)
{
   if (s)
      s->emit_argb = argb ? 1 : 0;
}

/* Display duration of packet 'idx', in ms, from the pre-scanned
 * timestamp table; 0 when unknown (caller applies its default). */
static int rmp4_video_duration_ms(const rmp4_video_stream_t *s, int idx)
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
static int rmp4_video_decode_packet(rmp4_video_stream_t *s,
      const rmp4_packet *pkt)
{
   /* Any decode below can invalidate the previously recorded
    * picture's planes; re-record on display.  (The wait_key early
    * return clears too - harmless, callers only render after a
    * displayed step.) */
   s->rndr_kind = 0;
   /* A decoder can be absent if the re-open in rewind hit OOM. */
   if (!s->vp8
#ifdef HAVE_RVP9
       && !s->vp9
#endif
       && !s->h264
      )
      return -1;
#ifdef HAVE_RVP9
   if (s->codec == RMP4_CODEC_VP9)
   {
      size_t sizes[RMP4_VIDEO_MAX_SUPER];
      const uint8_t *frame = pkt->data;
      int nf = rmp4_video_vp9_superframe(pkt->data, pkt->size,
            sizes, RMP4_VIDEO_MAX_SUPER);
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
         s->rndr_kind     = 2;
         s->rndr_vp9_show = last_show;
         return 1;
      }
      return 0;
   }
#endif
   if (s->codec == RMP4_CODEC_VP8)
   {
      /* VP8 frame tag: bit 4 of byte 0 is show_frame. */
      int shown = (pkt->size > 0) && (pkt->data[0] & 0x10);
      if (rvp8_video_decode(s->vp8, pkt->data, pkt->size) != 0)
         return -1;
      if (!shown)
         return 0;
      s->rndr_kind = 1;
      return 1;
   }
   if (s->codec == RMP4_CODEC_H264)
   {
      /* rh264 reconstructs Baseline through High profile (CAVLC and
       * CABAC entropy coding), handing pictures out in display order.
       * Anything it still cannot handle is skipped rather than
       * aborting the stream, holding the last good picture until the
       * next key frame restarts the prediction chain. A key frame
       * that fails to decode is a real error. */
      {
         int dec;
         /* Once any frame in a prediction chain fails (e.g. an
          * entropy-coding mode this decoder refuses), the pictures the
          * following inter frames predict from are not the ones the
          * encoder used, and decoding them anyway produces drifting
          * garbage. Freeze on the last good picture until the next key
          * frame restarts the chain cleanly. */
         if (s->wait_key && !pkt->keyframe)
         {
            s->disp_idx++;  /* the sample's presentation slot is gone */
            return 0;
         }
         dec = rh264_video_decode(s->h264, pkt->data, pkt->size);
         if (dec < 0)
         {
            if (pkt->keyframe)
               return -1;
            s->wait_key = 1;
            return 0;
         }
         s->wait_key = 0;
         if (dec == 0)   /* consumed; picture held for display reordering */
            return 0;
      }
      /* Planes stay valid until the next decode; defer conversion. */
      if (!rh264_video_plane(s->h264, 0, NULL, NULL, NULL))
         return -1;
      s->rndr_kind = 3;
      return 1;
   }
   return -1;
}

/* Advance by exactly one of the chosen track's packets (skipping other
 * tracks' packets, which are header reads only), or by one drained
 * reorder-queue picture once the packets are exhausted.  Returns 1
 * when a displayed picture was consumed (render it on demand), 0 when
 * the packet decoded but is not displayed, 2 when the next packet's
 * bytes are not yet resident (partial read; nothing consumed - retry
 * after rmp4_video_stream_set_avail), -1 at the end of a pass or on a
 * decode error. */
static int rmp4_video_stream_step(rmp4_video_stream_t *s,
      int *duration_ms)
{
   rmp4_packet pkt;

   for (;;)
   {
      int r = rmp4_read_packet(s->demux, &pkt);
      if (r == RMP4_READ_AGAIN)
         /* The sample's bytes are not buffered yet.  Do NOT fall
          * through to the drain below: packets remain, and flushing
          * the H.264 reorder queue now would present its held
          * pictures early and out of order. */
         return 2;
      if (r != 1)
         break;
      if (pkt.track != s->track)
         continue;
      r = rmp4_video_decode_packet(s, &pkt);
      if (r < 0)
         return -1;      /* decode error: end the animation */
      if (r == 0)
         return 0;       /* non-shown frame                  */
      if (duration_ms)
         *duration_ms = rmp4_video_duration_ms(s, s->disp_idx);
      s->disp_idx++;
      return 1;
   }
   /* Out of packets. Display reordering can leave the last few pictures
    * queued inside the H.264 decoder; hand them out before ending the
    * pass, one per call, on the same presentation clock. */
   if (s->h264 && rh264_video_drain(s->h264) == 0)
   {
      s->rndr_kind = 0;   /* the drain replaced the current picture */
      if (rh264_video_plane(s->h264, 0, NULL, NULL, NULL))
      {
         s->rndr_kind = 3;
         if (duration_ms)
            *duration_ms = rmp4_video_duration_ms(s, s->disp_idx);
         s->disp_idx++;
         return 1;
      }
   }
   return -1;             /* end of one pass */
}

int rmp4_video_stream_skip(rmp4_video_stream_t *s, int *duration_ms)
{
   int r;

   if (!s)
      return -1;

   /* 2 (not yet resident) also ends the loop: nothing was consumed,
    * and the caller retries after feeding more bytes.  Fully-resident
    * streams never see it. */
   while ((r = rmp4_video_stream_step(s, duration_ms)) == 0)
      ;                   /* non-shown frame: keep going      */
   return r;
}

void rmp4_video_stream_set_avail(rmp4_video_stream_t *s, size_t avail)
{
   if (s)
      rmp4_set_avail(s->demux, avail);
}

size_t rmp4_video_stream_media_floor(rmp4_video_stream_t *s)
{
   return s ? rmp4_media_floor(s->demux) : 0;
}

size_t rmp4_video_stream_consumed(rmp4_video_stream_t *s)
{
   return s ? rmp4_consumed(s->demux) : 0;
}

const uint32_t *rmp4_video_stream_render(rmp4_video_stream_t *s)
{
   if (!s)
      return NULL;
   switch (s->rndr_kind)
   {
      case 1:  /* VP8: planes valid until the next decode call */
      {
         const uint8_t *y, *u, *v;
         int ys, uvs, w, h, cw, ch;
         y = rvp8_video_plane(s->vp8, 0, &ys, &w, &h);
         u = rvp8_video_plane(s->vp8, 1, &uvs, &cw, &ch);
         v = rvp8_video_plane(s->vp8, 2, &uvs, &cw, &ch);
         if (!y || !u || !v)
            return NULL;
         if ((unsigned)w > s->width)
            w = (int)s->width;
         if ((unsigned)h > s->height)
            h = (int)s->height;
         rmp4_video_blit_i420(s->frame, s->width,
               (unsigned)w, (unsigned)h, y, ys, u, v, uvs, s->matrix,
               s->emit_argb);
         return s->frame;
      }
#ifdef HAVE_RVP9
      case 2:  /* VP9: the recorded show buffer */
      {
         const rvp9_fb *fb = &s->vp9->fbs[s->rndr_vp9_show];
         unsigned w = (unsigned)fb->w < s->width  ? (unsigned)fb->w : s->width;
         unsigned h = (unsigned)fb->h < s->height ? (unsigned)fb->h : s->height;
         if (s->vp9->hd.bit_depth == 10)
         {
            /* Colour metadata comes from the sample entry's colr box when
             * present; untagged files pass zeros and the shared blit picks
             * HD-appropriate defaults (BT.2020-ncl for PQ, BT.709/601 by
             * resolution), matching untagged webm content. Without this
             * branch the 10-bit (uint16) planes were handed to the 8-bit
             * blit and mis-decoded. */
            if (s->want10)
            {
               rwebm_video_blit_i420_10bit(s->frame, s->width, w, h,
                     (const uint16_t*)fb->y, s->vp9->ys,
                     (const uint16_t*)fb->u, (const uint16_t*)fb->v,
                     s->vp9->uvs, s->matrix, s->transfer, s->range, 0);
               s->is10 = 1;
            }
            else
               rwebm_video_blit_i420_hbd(s->frame, s->width, w, h,
                     (const uint16_t*)fb->y, s->vp9->ys,
                     (const uint16_t*)fb->u, (const uint16_t*)fb->v,
                     s->vp9->uvs, s->matrix, s->transfer, s->range, 0,
                     s->emit_argb ? 0 : 1);
         }
         else
            rmp4_video_blit_i420(s->frame, s->width, w, h,
                  fb->y, s->vp9->ys, fb->u, fb->v, s->vp9->uvs, s->matrix,
                  s->emit_argb);
         return s->frame;
      }
#endif
      case 3:  /* H.264: planes valid until the next decode or drain */
      {
         const uint8_t *y, *u, *v;
         int ys, uvs, w, h, cw, ch;
         y = rh264_video_plane(s->h264, 0, &ys,  &w,  &h);
         u = rh264_video_plane(s->h264, 1, &uvs, &cw, &ch);
         v = rh264_video_plane(s->h264, 2, &uvs, &cw, &ch);
         if (!y || !u || !v)
            return NULL;
         if ((unsigned)w > s->width)
            w = (int)s->width;
         if ((unsigned)h > s->height)
            h = (int)s->height;
         rmp4_video_blit_yuv(s->frame, s->width,
               (unsigned)w, (unsigned)h, y, ys, u, v, uvs, s->matrix,
               (ch < h) ? 1 : 0, s->emit_argb);
         return s->frame;
      }
      default:
         break;
   }
   return NULL;
}

const uint32_t *rmp4_video_stream_next(rmp4_video_stream_t *s,
      int *duration_ms)
{
   if (!s)
      return NULL;
   if (rmp4_video_stream_skip(s, duration_ms) != 1)
      return NULL;
   return rmp4_video_stream_render(s);
}

void rmp4_video_stream_rewind(rmp4_video_stream_t *s)
{
   if (!s)
      return;
   s->rndr_kind = 0;   /* decoder reset invalidates the planes */
   rmp4_rewind(s->demux);
   s->disp_idx = 0;
   s->wait_key = 0;
   /* The stream restarts at a key frame, so a fresh decoder (empty
    * reference chain, default probabilities) is the correct state. */
   rmp4_video_stream_close_decoder(s);
   rmp4_video_stream_open_decoder(s);
}

int64_t rmp4_video_stream_span_ms(const rmp4_video_stream_t *s)
{
   if (!s || s->ts_count < 2)
      return 0;
   return (int64_t)((s->ts[s->ts_count - 1] - s->ts[0]) / 1000000);
}

int64_t rmp4_video_stream_seek_ms(rmp4_video_stream_t *s, int64_t ms)
{
   int64_t target_ns;
   int tidx, kf, n;
   rmp4_packet pkt;

   if (!s || s->ts_count <= 0)
      return -1;
   if (ms < 0)
      ms = 0;
   /* Callers clock this stream by summing the quantised per-frame
    * durations, which floors fractional milliseconds, so a position
    * taken from that clock sits up to a millisecond below the true
    * timestamp.  Bias the slot search by just under a millisecond so
    * a position on a frame boundary selects that frame, not the one
    * before it. */
   target_ns = ms * 1000000 + 999999;

   /* Target display slot: the last frame whose presentation time is at
    * or before the requested position. */
   tidx = 0;
   while (tidx + 1 < s->ts_count && s->ts[tidx + 1] <= target_ns)
      tidx++;

   /* Pass 1: the last key frame at or before the target, by sample
    * ordinal.  Decode order stands in for the display slot here; with
    * B-frame reordering that is off by at most the reorder depth
    * around the cut, and the key frame itself always displays before
    * everything decoded after it. */
   rmp4_video_stream_rewind(s);
   kf = 0;
   n  = 0;
   while (n <= tidx && rmp4_read_packet(s->demux, &pkt) == 1)
   {
      if (pkt.track != s->track)
         continue;
      if (pkt.keyframe)
         kf = n;
      n++;
   }

   /* Pass 2: skip undecoded up to the key frame - its slot count is
    * exactly the display slots consumed for H.264 and VP9 - then
    * decode forward, discarding pictures, until the target slot has
    * been presented. */
   rmp4_video_stream_rewind(s);
   n = 0;
   while (n < kf && rmp4_read_packet(s->demux, &pkt) == 1)
   {
      if (pkt.track != s->track)
         continue;
      n++;
   }
   s->disp_idx = kf;
   while (s->disp_idx < tidx)
      if (rmp4_video_stream_skip(s, NULL) != 1)
         break;
   /* The target slot itself is left for the caller's next
    * rmp4_video_stream_next call, so the frame at the position is the
    * one presented; the catch-up pictures above were never colour
    * converted, and none of them is renderable now. */
   s->rndr_kind = 0;
   return s->disp_idx < s->ts_count ? s->ts[s->disp_idx] / 1000000 : 0;
}

/* ------------------------------------------------------------------ */
/* Still image (first displayed frame)                                 */
/* ------------------------------------------------------------------ */
rmp4_video_t *rmp4_video_alloc(void)
{
   return (rmp4_video_t*)calloc(1, sizeof(rmp4_video_t));
}

void rmp4_video_free(rmp4_video_t *mp4)
{
   if (!mp4)
      return;
   if (mp4->stream)
      rmp4_video_stream_close(mp4->stream);
   free(mp4);
}

rmp4_video_stream_t *rmp4_video_detach_stream(rmp4_video_t *mp4)
{
   rmp4_video_stream_t *s;
   if (!mp4 || !mp4->stream)
      return NULL;
   /* A stream mid-build in the sliced still decode is not complete
    * (pre-scan or decoder setup may be pending); never hand it out. */
   if (mp4->still_stage != RMP4_VIDEO_STILL_IDLE)
      return NULL;
   s           = mp4->stream;
   mp4->stream = NULL;
   /* The animation consumers upload plain 8-bit frames; the still may
    * have been decoded 10-bit, so drop the request before handing the
    * stream over (each frame re-blits, no stale pixels survive).  The
    * channel order likewise resets to the documented default; adopters
    * re-select per frame via rmp4_video_stream_set_argb. */
   s->want10    = 0;
   s->is10      = 0;
   s->emit_argb = 0;
   return s;
}

bool rmp4_video_set_buf_ptr(rmp4_video_t *mp4, void *data, size_t len)
{
   if (!mp4)
      return false;
   mp4->buf = (const uint8_t*)data;
   mp4->len = len;
   return true;
}

/* Ask the thumbnail decoder to emit packed XRGB2101010 for 10-bit HDR
 * sources (it keeps 8-bit output for 8-bit sources). */
void rmp4_video_set_want_10bit(rmp4_video_t *mp4, int want)
{
   if (mp4)
      mp4->want10 = want ? 1 : 0;
}

void rmp4_video_set_avail(rmp4_video_t *mp4, size_t avail)
{
   if (!mp4)
      return;
   mp4->partial = 1;
   if (avail > mp4->len)
      avail = mp4->len;
   if (avail > mp4->avail)   /* monotonic */
      mp4->avail = avail;
   if (mp4->stream)
      rmp4_video_stream_set_avail(mp4->stream, mp4->avail);
}

/* True if the last rmp4_video_process_image() wrote packed XRGB2101010. */
bool rmp4_video_is_10bit(const rmp4_video_t *mp4)
{
   return mp4 && mp4->last_10bit;
}

/* Sliced still-image decode, mirroring the WebM glue: each call
 * performs one bounded unit of work and returns IMAGE_PROCESS_NEXT
 * until the first displayed frame is ready - stream open, pre-scan in
 * RMP4_VIDEO_SCAN_SLICE-packet chunks, decoder setup, then one coded
 * packet per call (an H.264 picture held for display reordering takes
 * its own slice) - so the still decode no longer monopolises the
 * cooperative task queue in a single process call. */
int rmp4_video_process_image(rmp4_video_t *mp4, void **buf,
      size_t len, unsigned *width, unsigned *height, bool supports_rgba)
{
   rmp4_video_stream_t *s;
   const uint32_t *frame;
   uint32_t *out;
   size_t n;
   int r;
   int duration_ms = 0;

   (void)len;

   if (!mp4 || !mp4->buf || !buf)
      return IMAGE_PROCESS_ERROR;

   /* A partial reader raises mp4->avail between calls; push it down
    * so blocked stages can make progress this call. */
   if (mp4->stream && mp4->partial)
      rmp4_video_stream_set_avail(mp4->stream, mp4->avail);

   switch (mp4->still_stage)
   {
      case RMP4_VIDEO_STILL_IDLE:
      {
         int    need_more = 0;
         size_t avail     = mp4->partial ? mp4->avail : mp4->len;
         /* Defensive: a repeated process call re-opens from scratch. */
         if (mp4->stream)
         {
            rmp4_video_stream_close(mp4->stream);
            mp4->stream = NULL;
         }
         if (!(mp4->stream = rmp4_video_stream_open_begin(
               mp4->buf, mp4->len, avail, &need_more)))
            /* The moov is still arriving (or, for a trailing moov or a
             * fragmented movie, most of the file is): wait for more
             * bytes rather than failing. */
            return need_more ? IMAGE_PROCESS_WAIT : IMAGE_PROCESS_ERROR;
         mp4->still_stage = RMP4_VIDEO_STILL_SCAN;
         return IMAGE_PROCESS_NEXT;
      }

      case RMP4_VIDEO_STILL_SCAN:
         if (!rmp4_video_stream_scan_step(mp4->stream,
               RMP4_VIDEO_SCAN_SLICE))
            return IMAGE_PROCESS_NEXT;
         if (rmp4_video_stream_open_finish(mp4->stream) != 0)
            goto fail;
         /* Request 10-bit output; the stream honours it only for
          * 10-bit sources.  For 8-bit output, have the blit emit the
          * caller's channel order directly (supports_rgba is sampled
          * once per transfer by task_image, so the value seen here
          * holds for the END slice's copy below). */
         mp4->stream->want10    = mp4->want10;
         mp4->stream->emit_argb = supports_rgba ? 0 : 1;
         mp4->still_stage       = RMP4_VIDEO_STILL_DECODE;
         return IMAGE_PROCESS_NEXT;

      case RMP4_VIDEO_STILL_DECODE:
         break;

      default:
         return IMAGE_PROCESS_ERROR;
   }

   s = mp4->stream;
   if ((r = rmp4_video_stream_step(s, &duration_ms)) == 0)
      return IMAGE_PROCESS_NEXT;   /* non-shown frame decoded */
   if (r == 2)
      /* The next sample's bytes have not been read yet; nothing was
       * consumed - resume here once more of the file has arrived. */
      return IMAGE_PROCESS_WAIT;
   if (r < 0)
      goto fail;
   if (!(frame = rmp4_video_stream_render(s)))
      goto fail;

   mp4->last_10bit = s->is10;

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
    * rmp4_video_free closes it. */
   mp4->still_stage = RMP4_VIDEO_STILL_IDLE;
   return IMAGE_PROCESS_END;

fail:
   rmp4_video_stream_close(mp4->stream);
   mp4->stream      = NULL;
   mp4->still_stage = RMP4_VIDEO_STILL_IDLE;
   return IMAGE_PROCESS_ERROR;
}
