/* Copyright (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------
 * The following license statement only applies to this file (rmpeg1_video.c).
 * ---------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/* MPEG-1 video decoder, intra pictures.
 *
 * Bitstream hierarchy (11172-2 clause 2.4.2):
 *
 *   sequence_header  B3h   geometry, frame rate, quantiser matrices
 *   group_of_pictures B8h  timecode; carries no decoding state
 *   picture          00h   temporal_reference, coding type
 *   slice         01h..AFh one row (or part of one) of macroblocks
 *   macroblock            address increment, type, then six blocks
 *   block                 DC differential then run/level AC pairs
 *
 * Every layer above macroblock is byte-aligned behind a start code, so the
 * parser resynchronises trivially; within a slice it is a pure bit stream.
 *
 * Style: C89, no declarations after statements, no // comments.
 */

#include <stdlib.h>
#include <string.h>

#include <retro_inline.h>

#include <formats/rmpeg1_video.h>

#include "rmpeg1_tables.h"

/* The generated tables stay the single source of truth; the decoder builds a
 * lookup index over them at init rather than carrying a second, hand-written
 * copy of the same data in a faster shape.
 *
 * A linear scan over up to 113 entries, one peek per entry, was 36% of decode
 * time. RMPEG1_LUT_BITS covers every code short enough to matter -- the long
 * ones are by construction the rare symbols -- and anything longer falls back
 * to the scan, restricted to the entries that can still match. */
#define RMPEG1_LUT_BITS 9
#define RMPEG1_LUT_SIZE (1 << RMPEG1_LUT_BITS)

enum
{
   RM_VLC_DCT_FIRST = 0,
   RM_VLC_DCT_NEXT,
   RM_VLC_MBA,
   RM_VLC_DC_LUM,
   RM_VLC_DC_CHR,
   RM_VLC_MB_TYPE_P,
   RM_VLC_MB_TYPE_B,
   RM_VLC_CBP,
   RM_VLC_MOTION,
   RM_VLC_COUNT
};

static const rmpeg1_vlc_t * const rmpeg1_vlc_tables[RM_VLC_COUNT] =
{
   rmpeg1_vlc_dct_first,
   rmpeg1_vlc_dct_next,
   rmpeg1_vlc_mba,
   rmpeg1_vlc_dc_lum,
   rmpeg1_vlc_dc_chr,
   rmpeg1_vlc_mb_type_p,
   rmpeg1_vlc_mb_type_b,
   rmpeg1_vlc_cbp,
   rmpeg1_vlc_motion
};

typedef struct
{
   int16_t idx;   /* index into the table, or -1 for no short code here */
   int8_t  len;
} rmpeg1_lut_ent;

#define RMPEG1_START_PICTURE   0x00
#define RMPEG1_START_SLICE_LO  0x01
#define RMPEG1_START_SLICE_HI  0xAF
#define RMPEG1_START_USER_DATA 0xB2
#define RMPEG1_START_SEQUENCE  0xB3
#define RMPEG1_START_EXTENSION 0xB5
#define RMPEG1_START_SEQ_END   0xB7
#define RMPEG1_START_GOP       0xB8

#define RMPEG1_PIC_I           1
#define RMPEG1_PIC_P           2
#define RMPEG1_PIC_B           3
#define RMPEG1_PIC_D           4

#define RMPEG1_WINDOW          (1024 * 1024)
#define RMPEG1_MAX_W           4095
#define RMPEG1_MAX_H           2800

/* --------------------------------------------------------------------- */
/* Constants from the specification                                      */
/* --------------------------------------------------------------------- */

/* scan[0][v][u], H.262 Figure 7-2: zigzag position -> raster index. */
static const uint8_t rmpeg1_zigzag[64] =
{
    0,  1,  8, 16,  9,  2,  3, 10,
   17, 24, 32, 25, 18, 11,  4,  5,
   12, 19, 26, 33, 40, 48, 41, 34,
   27, 20, 13,  6,  7, 14, 21, 28,
   35, 42, 49, 56, 57, 50, 43, 36,
   29, 22, 15, 23, 30, 37, 44, 51,
   58, 59, 52, 45, 38, 31, 39, 46,
   53, 60, 61, 54, 47, 55, 62, 63
};

/* Default intra quantiser matrix, in raster order. */
static const uint8_t rmpeg1_default_intra[64] =
{
    8, 16, 19, 22, 26, 27, 29, 34,
   16, 16, 22, 24, 27, 29, 34, 37,
   19, 22, 26, 27, 29, 34, 34, 38,
   22, 22, 26, 27, 29, 34, 37, 40,
   22, 26, 27, 29, 32, 35, 40, 48,
   26, 27, 29, 32, 35, 40, 48, 58,
   26, 27, 29, 34, 38, 46, 56, 69,
   27, 29, 35, 38, 46, 56, 69, 83
};

/* frame_rate_code -> exact rational, H.262 Table 6-4. */
static const unsigned rmpeg1_fps_num[16] =
{ 0, 24000, 24, 25, 30000, 30, 50, 60000, 60, 0, 0, 0, 0, 0, 0, 0 };
static const unsigned rmpeg1_fps_den[16] =
{ 1,  1001,  1,  1,  1001,  1,  1,  1001,  1, 1, 1, 1, 1, 1, 1, 1 };

/* --------------------------------------------------------------------- */
/* State                                                                 */
/* --------------------------------------------------------------------- */

struct rmpeg1_video
{
   uint8_t  *buf;
   size_t    cap;
   size_t    wr;
   size_t    rd;         /* byte cursor  */
   unsigned  bit;        /* 0..7 within buf[rd] */

   bool      have_seq;
   unsigned  width, height;
   unsigned  mb_w, mb_h;
   unsigned  y_stride, c_stride;
   unsigned  fps_code, aspect_code;

   rmpeg1_lut_ent lut[RM_VLC_COUNT][RMPEG1_LUT_SIZE];

   uint8_t   intra_q[64];
   uint8_t   non_intra_q[64];

   /* Three frame slots, rotated by pointer rather than copied.
    *
    *   cur  the picture being decoded
    *   fwd  the past reference   (forward prediction)
    *   bwd  the future reference (backward prediction)
    *
    * A B picture predicts from both and is never itself a reference, so it
    * is decoded into cur and emitted straight away. An I or P picture is
    * decoded into cur, then the slots rotate: the old bwd becomes
    * displayable and is emitted, fwd takes its place, and the new picture
    * becomes bwd. That one-picture delay is the reordering B pictures
    * require -- they are transmitted after the reference they predict
    * forward from, and displayed before it. */
   /* All nine planes are views into plane_arena, each starting on a
    * 64-byte boundary. */
   uint8_t  *plane_arena;
   uint8_t  *plane[3][3];     /* [slot][Y, Cb, Cr] */
   int       slot_cur, slot_fwd, slot_bwd;
   bool      have_fwd, have_bwd;
   unsigned  bwd_tref;
   unsigned  bwd_type;
   bool      eof;
   size_t    plane_bytes;

   /* per-picture */
   unsigned  temporal_ref;
   unsigned  coding_type;
   int       quant_scale;
   int       dc_pred[3];      /* Y, Cb, Cr */
   unsigned  mb_addr;

   /* Index 0 is forward, 1 is backward, matching the s subscript the
    * specification uses for f_code[s][t]. */
   int       f_code[2];
   bool      full_pel[2];
   int       mv_pred[2][2];   /* [direction][x, y] */

   /* A skipped macroblock in a B picture repeats the previous macroblock's
    * vectors and prediction mode, unlike in a P picture where it means a
    * zero vector. Remember them. */
   unsigned  last_flags;
   int       last_mv[2][2];

   uint32_t  skipped;
   uint32_t  errors;
};

/* --------------------------------------------------------------------- */
/* Bit reader                                                            */
/* --------------------------------------------------------------------- */

static INLINE size_t bits_left(const rmpeg1_video_t *v)
{
   return ((v->wr - v->rd) * 8) - v->bit;
}

static INLINE unsigned get_bit(rmpeg1_video_t *v)
{
   unsigned b;

   if (v->rd >= v->wr)
      return 0;

   b = (v->buf[v->rd] >> (7 - v->bit)) & 1u;

   if (++v->bit == 8)
   {
      v->bit = 0;
      v->rd++;
   }
   return b;
}

static uint32_t get_bits(rmpeg1_video_t *v, unsigned n)
{
   uint32_t r = 0;

   while (n--)
      r = (r << 1) | get_bit(v);
   return r;
}

/* Peek without consuming. Gathers eight bytes into a 64-bit accumulator and
 * shifts, rather than walking bit by bit: the VLC decoder peeks once per
 * symbol and a per-bit loop made that the hottest function in the decoder.
 * Reads past the write cursor return zero, which is what the start-code
 * hunt below relies on. */
static uint32_t peek_bits(const rmpeg1_video_t *v, unsigned n)
{
   uint64_t acc;
   size_t   i = v->rd;

   if (n == 0)
      return 0;

   if (i + 8 <= v->wr)
   {
      const uint8_t *p = v->buf + i;

      acc = ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48)
          | ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32)
          | ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16)
          | ((uint64_t)p[6] <<  8) |  (uint64_t)p[7];
   }
   else
   {
      unsigned k;

      acc = 0;
      for (k = 0; k < 8; k++)
         acc = (acc << 8) | ((i + k < v->wr) ? v->buf[i + k] : 0);
   }

   acc <<= v->bit;
   return (uint32_t)(acc >> (64 - n));
}

static void byte_align(rmpeg1_video_t *v)
{
   if (v->bit)
   {
      v->bit = 0;
      v->rd++;
   }
}

/* --------------------------------------------------------------------- */
/* VLC decode                                                            */
/* --------------------------------------------------------------------- */

static void build_luts(rmpeg1_video_t *v)
{
   int t;

   for (t = 0; t < RM_VLC_COUNT; t++)
   {
      const rmpeg1_vlc_t *tab = rmpeg1_vlc_tables[t];
      const rmpeg1_vlc_t *e;
      int i;

      for (i = 0; i < RMPEG1_LUT_SIZE; i++)
      {
         v->lut[t][i].idx = -1;
         v->lut[t][i].len = 0;
      }

      for (e = tab, i = 0; e->len; e++, i++)
      {
         unsigned shift, base, span, k;

         if ((unsigned)e->len > RMPEG1_LUT_BITS)
            continue;

         shift = RMPEG1_LUT_BITS - (unsigned)e->len;
         base  = (unsigned)e->code << shift;
         span  = 1u << shift;

         /* The tables are prefix-free, so no slot is ever written twice --
          * the generator proves that before emitting them. */
         for (k = 0; k < span; k++)
         {
            v->lut[t][base + k].idx = (int16_t)i;
            v->lut[t][base + k].len = e->len;
         }
      }
   }
}

/* Decode one symbol. A single peek indexes the table for any code up to
 * RMPEG1_LUT_BITS; longer codes fall through to a scan restricted to the
 * entries that can still match. Returns the table entry, or NULL. */
static const rmpeg1_vlc_t *vlc_decode(rmpeg1_video_t *v, int t)
{
   const rmpeg1_vlc_t   *tab = rmpeg1_vlc_tables[t];
   const rmpeg1_lut_ent *hit = &v->lut[t][peek_bits(v, RMPEG1_LUT_BITS)];
   const rmpeg1_vlc_t   *e;

   if (hit->len)
   {
      (void)get_bits(v, (unsigned)hit->len);
      return &tab[hit->idx];
   }

   for (e = tab; e->len; e++)
   {
      if ((unsigned)e->len <= RMPEG1_LUT_BITS)
         continue;
      if ((size_t)e->len > bits_left(v))
         continue;
      if (peek_bits(v, (unsigned)e->len) == e->code)
      {
         (void)get_bits(v, (unsigned)e->len);
         return e;
      }
   }
   return NULL;
}

/* --------------------------------------------------------------------- */
/* Start codes                                                           */
/* --------------------------------------------------------------------- */

/* Find the next start code at or after the byte cursor. Returns its value
 * and leaves the cursor just past the 4-byte code, or -1 if none is buffered
 * (leaving the last three bytes unconsumed, since a code may straddle). */
static int next_start_code(rmpeg1_video_t *v)
{
   byte_align(v);

   while (v->rd + 4 <= v->wr)
   {
      if (     v->buf[v->rd    ] == 0x00
            && v->buf[v->rd + 1] == 0x00
            && v->buf[v->rd + 2] == 0x01)
      {
         int code = v->buf[v->rd + 3];
         v->rd   += 4;
         return code;
      }
      v->rd++;
   }
   return -1;
}

/* Offset of the next start code at or after `from`, or (size_t)-1. */
static size_t scan_start_code(const rmpeg1_video_t *v, size_t from)
{
   while (from + 4 <= v->wr)
   {
      if (     v->buf[from] == 0x00 && v->buf[from + 1] == 0x00
            && v->buf[from + 2] == 0x01)
         return from;
      from++;
   }
   return (size_t)-1;
}

static bool at_start_code(const rmpeg1_video_t *v)
{
   /* 23 zero bits then a one: inside a slice this is how the macroblock loop
    * knows it has run out of macroblocks. Checked on the bit cursor, since
    * the last macroblock may not have ended byte-aligned. */
   return peek_bits(v, 23) == 0;
}

/* --------------------------------------------------------------------- */
/* Plane management                                                      */
/* --------------------------------------------------------------------- */

static bool alloc_planes(rmpeg1_video_t *v)
{
   size_t ysz, csz;

   v->y_stride = v->mb_w * 16;
   v->c_stride = v->mb_w * 8;

   ysz = (size_t)v->y_stride * (v->mb_h * 16);
   csz = (size_t)v->c_stride * (v->mb_h * 8);

   {
      int k, j;
      size_t yreg = (ysz + 63) & ~(size_t)63;
      size_t creg = (csz + 63) & ~(size_t)63;
      size_t cur  = 0;

      free(v->plane_arena);
      v->plane_arena = (uint8_t *)calloc(1, 3 * (yreg + 2 * creg));
      if (!v->plane_arena)
      {
         for (k = 0; k < 3; k++)
            for (j = 0; j < 3; j++)
               v->plane[k][j] = NULL;
         v->plane_bytes = 0;
         return false;
      }

      for (k = 0; k < 3; k++)
      {
         v->plane[k][0] = v->plane_arena + cur; cur += yreg;
         v->plane[k][1] = v->plane_arena + cur; cur += creg;
         v->plane[k][2] = v->plane_arena + cur; cur += creg;
      }
   }

   v->slot_cur = 0;
   v->slot_fwd = 1;
   v->slot_bwd = 2;
   v->have_fwd = false;
   v->have_bwd = false;

   v->plane_bytes = ysz;
   return true;
}

/* --------------------------------------------------------------------- */
/* IDCT                                                                  */
/* --------------------------------------------------------------------- */

/* Separable integer IDCT, AAN-style constants scaled to 11 fractional bits.
 * 11172-2 does not mandate a particular IDCT, only that it conform to
 * IEEE 1180-1990 accuracy; rows then columns with rounding at each stage is
 * within that. Deliberately integer: a float IDCT would make output depend
 * on host FPU rounding, which is not acceptable in a core whose determinism
 * is otherwise enforced. */

/* Scaling is written as multiplication rather than a left shift throughout:
 * shifting a negative int is undefined behaviour, and coefficients are
 * routinely negative. Every compiler in practice emits the same shift, but
 * UBSan is right to flag it and there is no reason to carry it. */
#define W1 2841   /* cos(1*pi/16) * sqrt(2) * 2048 */
#define W2 2676
#define W3 2408
#define W5 1609
#define W6 1108
#define W7 565

static void idct_row(const int16_t *in, int *b)
{
   int x0, x1, x2, x3, x4, x5, x6, x7, x8;

   x1 = (int)in[4] * 2048;
   x2 = in[6];
   x3 = in[2];
   x4 = in[1];
   x5 = in[7];
   x6 = in[5];
   x7 = in[3];

   /* All-zero AC row: the DC term alone, replicated. Common enough in real
    * content that skipping the butterflies is worth the branch. */
   if (!(x1 | x2 | x3 | x4 | x5 | x6 | x7))
   {
      int dc = (int)in[0] * 8;
      b[0] = b[1] = b[2] = b[3] = b[4] = b[5] = b[6] = b[7] = dc;
      return;
   }

   x0 = (int)in[0] * 2048 + 128;

   x8 = W7 * (x4 + x5);
   x4 = x8 + (W1 - W7) * x4;
   x5 = x8 - (W1 + W7) * x5;
   x8 = W3 * (x6 + x7);
   x6 = x8 - (W3 - W5) * x6;
   x7 = x8 - (W3 + W5) * x7;

   x8 = x0 + x1;
   x0 -= x1;
   x1 = W6 * (x3 + x2);
   x2 = x1 - (W2 + W6) * x2;
   x3 = x1 + (W2 - W6) * x3;
   x1 = x4 + x6;
   x4 -= x6;
   x6 = x5 + x7;
   x5 -= x7;

   x7 = x8 + x3;
   x8 -= x3;
   x3 = x0 + x2;
   x0 -= x2;
   x2 = (int)(((int64_t)181 * (x4 + x5) + 128) >> 8);
   x4 = (int)(((int64_t)181 * (x4 - x5) + 128) >> 8);

   b[0] = (x7 + x1) >> 8;
   b[1] = (x3 + x2) >> 8;
   b[2] = (x0 + x4) >> 8;
   b[3] = (x8 + x6) >> 8;
   b[4] = (x8 - x6) >> 8;
   b[5] = (x0 - x4) >> 8;
   b[6] = (x3 - x2) >> 8;
   b[7] = (x7 - x1) >> 8;
}

static void idct_col_store(const int *b, uint8_t *dst, unsigned stride)
{
   int x0, x1, x2, x3, x4, x5, x6, x7, x8;
   int i;
   int out[8];

   x1 = b[8 * 4] * 256;
   x2 = b[8 * 6];
   x3 = b[8 * 2];
   x4 = b[8 * 1];
   x5 = b[8 * 7];
   x6 = b[8 * 5];
   x7 = b[8 * 3];

   if (!(x1 | x2 | x3 | x4 | x5 | x6 | x7))
   {
      int dc = (b[0] + 32) >> 6;
      for (i = 0; i < 8; i++)
         out[i] = dc;
   }
   else
   {
      x0 = b[0] * 256 + 8192;

      x8 = W7 * (x4 + x5) + 4;
      x4 = (x8 + (W1 - W7) * x4) >> 3;
      x5 = (x8 - (W1 + W7) * x5) >> 3;
      x8 = W3 * (x6 + x7) + 4;
      x6 = (x8 - (W3 - W5) * x6) >> 3;
      x7 = (x8 - (W3 + W5) * x7) >> 3;

      x8 = x0 + x1;
      x0 -= x1;
      x1 = W6 * (x3 + x2) + 4;
      x2 = (x1 - (W2 + W6) * x2) >> 3;
      x3 = (x1 + (W2 - W6) * x3) >> 3;
      x1 = x4 + x6;
      x4 -= x6;
      x6 = x5 + x7;
      x5 -= x7;

      x7 = x8 + x3;
      x8 -= x3;
      x3 = x0 + x2;
      x0 -= x2;
      /* The rotation by 181/256 (i.e. sqrt(2)/2) is the one place where a
       * 32-bit product is not wide enough. By this point in the column pass
       * x4 and x5 have accumulated two stages of gain, so their difference
       * can reach ~1.3e7; multiplied by 181 that is ~2.3e9, past INT32_MAX.
       *
       * Legal MPEG-1 content never gets there -- the dequantiser saturates
       * and real coefficients are nothing like uniformly maximal -- which is
       * why the usual 32-bit formulation survives in practice. An IEEE 1180
       * sweep at L=300 reaches it in roughly one block in ten thousand, and
       * the wrap turns a saturated-white sample into black. Widen the two
       * products rather than hope. */
      x2 = (int)(((int64_t)181 * (x4 + x5) + 128) >> 8);
      x4 = (int)(((int64_t)181 * (x4 - x5) + 128) >> 8);

      out[0] = (x7 + x1) >> 14;
      out[1] = (x3 + x2) >> 14;
      out[2] = (x0 + x4) >> 14;
      out[3] = (x8 + x6) >> 14;
      out[4] = (x8 - x6) >> 14;
      out[5] = (x0 - x4) >> 14;
      out[6] = (x3 - x2) >> 14;
      out[7] = (x7 - x1) >> 14;
   }

   for (i = 0; i < 8; i++)
   {
      /* An intra block reconstructs the sample value itself, not a residual:
       * the DC coefficient already carries the block mean (predictor 1024,
       * which the IDCT scales back to 128). Adding a 128 bias here -- as a
       * non-intra block would need before summing with its prediction --
       * shifts the whole picture and clips the highlights. */
      int p = out[i];

      if (p < 0)
         p = 0;
      else if (p > 255)
         p = 255;

      dst[(size_t)i * stride] = (uint8_t)p;
   }
}

static void idct_block(const int16_t *blk, uint8_t *dst, unsigned stride)
{
   /* Row results go to a 32-bit scratch rather than back into the 16-bit
    * coefficient block. Storing them as int16 is the usual shortcut and is
    * safe for real content, but it overflows on pathological input -- an
    * IEEE 1180 sweep at L=300 turns bright blocks into dark ones -- and 256
    * bytes of scratch is cheap next to getting that wrong. */
   int tmp[64];
   int i;

   for (i = 0; i < 8; i++)
      idct_row(blk + i * 8, tmp + i * 8);
   for (i = 0; i < 8; i++)
      idct_col_store(tmp + i, dst + i, stride);
}

/* Same transform, but summed onto an existing prediction instead of
 * replacing it. A non-intra block codes the residual, not the sample. */
static void idct_col_add(const int *b, uint8_t *dst, unsigned stride)
{
   int x0, x1, x2, x3, x4, x5, x6, x7, x8;
   int i;
   int out[8];

   x1 = b[8 * 4] * 256;
   x2 = b[8 * 6];
   x3 = b[8 * 2];
   x4 = b[8 * 1];
   x5 = b[8 * 7];
   x6 = b[8 * 5];
   x7 = b[8 * 3];

   if (!(x1 | x2 | x3 | x4 | x5 | x6 | x7))
   {
      int dc = (b[0] + 32) >> 6;
      for (i = 0; i < 8; i++)
         out[i] = dc;
   }
   else
   {
      x0 = b[0] * 256 + 8192;

      x8 = W7 * (x4 + x5) + 4;
      x4 = (x8 + (W1 - W7) * x4) >> 3;
      x5 = (x8 - (W1 + W7) * x5) >> 3;
      x8 = W3 * (x6 + x7) + 4;
      x6 = (x8 - (W3 - W5) * x6) >> 3;
      x7 = (x8 - (W3 + W5) * x7) >> 3;

      x8 = x0 + x1;
      x0 -= x1;
      x1 = W6 * (x3 + x2) + 4;
      x2 = (x1 - (W2 + W6) * x2) >> 3;
      x3 = (x1 + (W2 - W6) * x3) >> 3;
      x1 = x4 + x6;
      x4 -= x6;
      x6 = x5 + x7;
      x5 -= x7;

      x7 = x8 + x3;
      x8 -= x3;
      x3 = x0 + x2;
      x0 -= x2;
      x2 = (int)(((int64_t)181 * (x4 + x5) + 128) >> 8);
      x4 = (int)(((int64_t)181 * (x4 - x5) + 128) >> 8);

      out[0] = (x7 + x1) >> 14;
      out[1] = (x3 + x2) >> 14;
      out[2] = (x0 + x4) >> 14;
      out[3] = (x8 + x6) >> 14;
      out[4] = (x8 - x6) >> 14;
      out[5] = (x0 - x4) >> 14;
      out[6] = (x3 - x2) >> 14;
      out[7] = (x7 - x1) >> 14;
   }

   for (i = 0; i < 8; i++)
   {
      int p = (int)dst[(size_t)i * stride] + out[i];

      if (p < 0)
         p = 0;
      else if (p > 255)
         p = 255;

      dst[(size_t)i * stride] = (uint8_t)p;
   }
}

static void idct_block_add(const int16_t *blk, uint8_t *dst, unsigned stride)
{
   int tmp[64];
   int i;

   for (i = 0; i < 8; i++)
      idct_row(blk + i * 8, tmp + i * 8);
   for (i = 0; i < 8; i++)
      idct_col_add(tmp + i, dst + i, stride);
}

/* --------------------------------------------------------------------- */
/* Motion compensation                                                   */
/* --------------------------------------------------------------------- */

/* Copy a bw x bh region from the reference with half-sample interpolation.
 *
 * Motion vectors are in half-sample units, so the integer part is an
 * arithmetic shift and the low bit selects interpolation. The shift must be
 * arithmetic rather than a division: -3 >> 1 is -2, which is the floor the
 * standard wants, whereas -3 / 2 truncates to -1 and shifts the whole block
 * by a sample.
 *
 * Source coordinates are clamped. A conforming stream never points a vector
 * outside the reference frame, but a damaged one can, and reading off the
 * end of the plane is not an acceptable way to find out. */
static void mc_predict(const uint8_t *ref, unsigned stride,
      unsigned pw, unsigned ph,
      uint8_t *dst, unsigned dstride,
      int x, int y, int mvx, int mvy, unsigned bw, unsigned bh)
{
   int      sx = x + (mvx >> 1);
   int      sy = y + (mvy >> 1);
   unsigned hx = (unsigned)(mvx & 1);
   unsigned hy = (unsigned)(mvy & 1);
   unsigned i, j;

   /* Fast path: the whole source block, including the extra row and column
    * a half-sample position touches, lies inside the reference. That is the
    * case for essentially every macroblock of a conforming stream, and it
    * lets the interpolation mode leave the inner loop -- with the mode
    * branch and the bounds clamp both inside it, this function was two
    * thirds of decode time. */
   if (     sx >= 0 && sy >= 0
         && (unsigned)(sx + (int)bw + (int)hx) <= pw
         && (unsigned)(sy + (int)bh + (int)hy) <= ph)
   {
      const uint8_t *src = ref + (size_t)sy * stride + sx;

      if (!hx && !hy)
      {
         for (j = 0; j < bh; j++)
            memcpy(dst + (size_t)j * dstride, src + (size_t)j * stride, bw);
      }
      else if (hx && !hy)
      {
         for (j = 0; j < bh; j++)
         {
            const uint8_t *r = src + (size_t)j * stride;
            uint8_t       *d = dst + (size_t)j * dstride;
            for (i = 0; i < bw; i++)
               d[i] = (uint8_t)((r[i] + r[i + 1] + 1) >> 1);
         }
      }
      else if (!hx && hy)
      {
         for (j = 0; j < bh; j++)
         {
            const uint8_t *r = src + (size_t)j * stride;
            uint8_t       *d = dst + (size_t)j * dstride;
            for (i = 0; i < bw; i++)
               d[i] = (uint8_t)((r[i] + r[i + stride] + 1) >> 1);
         }
      }
      else
      {
         for (j = 0; j < bh; j++)
         {
            const uint8_t *r = src + (size_t)j * stride;
            uint8_t       *d = dst + (size_t)j * dstride;
            for (i = 0; i < bw; i++)
               d[i] = (uint8_t)((r[i] + r[i + 1]
                               + r[i + stride] + r[i + stride + 1] + 2) >> 2);
         }
      }
      return;
   }

   /* Slow path, clamped per sample. A conforming stream never points a
    * vector outside the reference frame, but a damaged one can, and reading
    * off the end of the plane is not an acceptable way to find out. */
   for (j = 0; j < bh; j++)
   {
      for (i = 0; i < bw; i++)
      {
         int cx = sx + (int)i;
         int cy = sy + (int)j;
         int nx, ny, a2, b2, c2, d2;

         if (cx < 0) cx = 0;
         if (cy < 0) cy = 0;
         if (cx > (int)pw - 1) cx = (int)pw - 1;
         if (cy > (int)ph - 1) cy = (int)ph - 1;

         nx = (cx + 1 > (int)pw - 1) ? (int)pw - 1 : cx + 1;
         ny = (cy + 1 > (int)ph - 1) ? (int)ph - 1 : cy + 1;

         a2 = ref[(size_t)cy * stride + cx];
         b2 = ref[(size_t)cy * stride + nx];
         c2 = ref[(size_t)ny * stride + cx];
         d2 = ref[(size_t)ny * stride + nx];

         if (hx && hy)
            dst[(size_t)j * dstride + i] = (uint8_t)((a2 + b2 + c2 + d2 + 2) >> 2);
         else if (hx)
            dst[(size_t)j * dstride + i] = (uint8_t)((a2 + b2 + 1) >> 1);
         else if (hy)
            dst[(size_t)j * dstride + i] = (uint8_t)((a2 + c2 + 1) >> 1);
         else
            dst[(size_t)j * dstride + i] = (uint8_t)a2;
      }
   }
}

/* --------------------------------------------------------------------- */
/* Block layer                                                           */
/* --------------------------------------------------------------------- */

/* Decode one intra block into blk[64] in raster order.
 *
 * 11172-2 intra dequantisation:
 *   DC:  rec = 8 * QF[0]
 *   AC:  rec = (2 * QF * quant_scale * W) / 16, then made odd by moving one
 *        step toward zero when it comes out even and non-zero, then clamped
 *        to [-2048, 2047].
 *
 * The oddification is the mismatch control: it is what keeps encoder and
 * decoder IDCTs from drifting apart over a run of predicted pictures. It
 * applies to AC only; the DC term is exact.
 */
static bool decode_intra_block(rmpeg1_video_t *v, int16_t *blk, int cc)
{
   const rmpeg1_vlc_t *e;
   int      dc_size;
   int      diff = 0;
   unsigned idx  = 0;   /* zigzag position */

   memset(blk, 0, sizeof(int16_t) * 64);

   e = vlc_decode(v, cc == 0 ? RM_VLC_DC_LUM : RM_VLC_DC_CHR);
   if (!e)
      return false;
   dc_size = e->a;

   if (dc_size)
   {
      int val = (int)get_bits(v, (unsigned)dc_size);

      /* The differential is stored without a sign bit: the top bit being
       * clear means negative, and the value is biased accordingly. */
      if (val < (1 << (dc_size - 1)))
         val -= (1 << dc_size) - 1;
      diff = val;
   }

   v->dc_pred[cc] += diff;
   blk[0] = (int16_t)(v->dc_pred[cc] * 8);

   for (;;)
   {
      int run, level, sign, pos, rec;

      if (peek_bits(v, RMPEG1_DCT_EOB_LEN) == RMPEG1_DCT_EOB_CODE)
      {
         (void)get_bits(v, RMPEG1_DCT_EOB_LEN);
         break;
      }

      if (peek_bits(v, RMPEG1_DCT_ESCAPE_LEN) == RMPEG1_DCT_ESCAPE_CODE)
      {
         (void)get_bits(v, RMPEG1_DCT_ESCAPE_LEN);
         run   = (int)get_bits(v, 6);
         level = (int)get_bits(v, 8);

         /* 11172-2 escape levels: an 8-bit field, with 00h and 80h meaning
          * that a second 8-bit field follows to extend the range. */
         if (level == 0)
            level = (int)get_bits(v, 8);
         else if (level == 128)
            level = (int)get_bits(v, 8) - 256;
         else if (level > 128)
            level -= 256;
      }
      else
      {
         /* Only the second and later coefficients of an intra block use this
          * table; the DC term was handled above, so the "first coefficient"
          * spelling of (0,1) never occurs here. */
         e = vlc_decode(v, RM_VLC_DCT_NEXT);
         if (!e)
            return false;
         run   = e->a;
         level = e->b;
         sign  = (int)get_bit(v);
         if (sign)
            level = -level;
      }

      idx += (unsigned)run + 1;
      if (idx > 63)
         return false;

      pos = rmpeg1_zigzag[idx];

      rec = (2 * level * v->quant_scale * (int)v->intra_q[pos]) / 16;

      if (rec > 0 && !(rec & 1))
         rec--;
      else if (rec < 0 && !(rec & 1))
         rec++;

      if (rec >  2047) rec =  2047;
      if (rec < -2048) rec = -2048;

      blk[pos] = (int16_t)rec;
   }

   return true;
}

/* Decode one non-intra block. Unlike an intra block there is no separate DC
 * path: every coefficient including the first is run/level coded, and the
 * first uses the table where (0,1) is spelled '1s' rather than '11s'.
 *
 * 11172-2 non-intra dequantisation:
 *   rec = ((2 * QF + Sign(QF)) * quant_scale * W) / 16
 * then the same oddification and clamp as intra. The Sign term is what makes
 * the reconstruction levels sit at the centre of each quantiser bin rather
 * than its edge. */
static bool decode_inter_block(rmpeg1_video_t *v, int16_t *blk)
{
   const rmpeg1_vlc_t *e;
   int idx   = -1;
   int first = 1;

   memset(blk, 0, sizeof(int16_t) * 64);

   for (;;)
   {
      int run, level, pos, rec;

      if (!first && peek_bits(v, RMPEG1_DCT_EOB_LEN) == RMPEG1_DCT_EOB_CODE)
      {
         (void)get_bits(v, RMPEG1_DCT_EOB_LEN);
         break;
      }

      if (peek_bits(v, RMPEG1_DCT_ESCAPE_LEN) == RMPEG1_DCT_ESCAPE_CODE)
      {
         (void)get_bits(v, RMPEG1_DCT_ESCAPE_LEN);
         run   = (int)get_bits(v, 6);
         level = (int)get_bits(v, 8);

         if (level == 0)
            level = (int)get_bits(v, 8);
         else if (level == 128)
            level = (int)get_bits(v, 8) - 256;
         else if (level > 128)
            level -= 256;
      }
      else
      {
         e = vlc_decode(v, first ? RM_VLC_DCT_FIRST : RM_VLC_DCT_NEXT);
         if (!e)
            return false;
         run   = e->a;
         level = e->b;
         if (get_bit(v))
            level = -level;
      }

      first = 0;
      idx  += run + 1;
      if (idx > 63)
         return false;

      pos = rmpeg1_zigzag[idx];

      if (level > 0)
         rec = ((2 * level + 1) * v->quant_scale * (int)v->non_intra_q[pos]) / 16;
      else
         rec = ((2 * level - 1) * v->quant_scale * (int)v->non_intra_q[pos]) / 16;

      if (rec > 0 && !(rec & 1))
         rec--;
      else if (rec < 0 && !(rec & 1))
         rec++;

      if (rec >  2047) rec =  2047;
      if (rec < -2048) rec = -2048;

      blk[pos] = (int16_t)rec;
   }

   return true;
}

/* Reconstruct one motion vector component.
 *
 * motion_code carries the coarse step and, when f is greater than one, a
 * residual of f_code-1 bits refines it. The result is differential against
 * the previous vector in the slice and wraps within +/-16f, which is what
 * lets a long pan stay in range. */
static bool decode_motion(rmpeg1_video_t *v, int dir, int *pred, int *out)
{
   const rmpeg1_vlc_t *e = vlc_decode(v, RM_VLC_MOTION);
   int code, r = 0, f, delta, val;

   if (!e)
      return false;

   /* f_code is 1..7 by the specification. Zero reaches here two ways: a
    * slice decoded before any picture header has set it, and a header
    * whose f_code was rejected. Both are malformed input rather than
    * hypotheses - a byte-flip fuzzer over a conformant VCD stream hits
    * them in a few hundred iterations. The consequences of trusting it
    * are 1 << -1, which is undefined, and then get_bits() asked for
    * (unsigned)(0 - 1) == 4294967295 bits. get_bit() is bounds safe and
    * returns zero past the end, so that is not an overread, but it is a
    * 4.29-billion-iteration loop: measured at 2.7 seconds per occurrence
    * on a desktop, and it can recur once per macroblock. On the handheld
    * targets this core ships to it is indistinguishable from a hang, off
    * nothing more than a corrupt disc image. Refuse the macroblock. */
   if (v->f_code[dir] < 1)
      return false;

   code = e->a;
   f    = 1 << (v->f_code[dir] - 1);

   if (f != 1 && code != 0)
      r = (int)get_bits(v, (unsigned)(v->f_code[dir] - 1));

   if (code == 0)
      delta = 0;
   else
   {
      delta = ((code < 0 ? -code : code) - 1) * f + r + 1;
      if (code < 0)
         delta = -delta;
   }

   val = *pred + delta;

   if (val < -16 * f)
      val += 32 * f;
   else if (val >= 16 * f)
      val -= 32 * f;

   *pred = val;
   *out  = val;
   return true;
}

/* --------------------------------------------------------------------- */
/* Macroblock layer                                                      */
/* --------------------------------------------------------------------- */

#define RM_Y(v, slot)  ((v)->plane[(slot)][0])
#define RM_CB(v, slot) ((v)->plane[(slot)][1])
#define RM_CR(v, slot) ((v)->plane[(slot)][2])

static void mb_plane_ptrs(rmpeg1_video_t *v, unsigned addr,
      uint8_t **y, uint8_t **cb, uint8_t **cr)
{
   unsigned mx = addr % v->mb_w;
   unsigned my = addr / v->mb_w;

   *y  = RM_Y(v, v->slot_cur)  + (size_t)my * 16 * v->y_stride + (size_t)mx * 16;
   *cb = RM_CB(v, v->slot_cur) + (size_t)my *  8 * v->c_stride + (size_t)mx *  8;
   *cr = RM_CR(v, v->slot_cur) + (size_t)my *  8 * v->c_stride + (size_t)mx *  8;
}

/* Predict a whole macroblock from the reference, then add whatever residual
 * blocks the coded block pattern says are present. */
static bool decode_inter_macroblock(rmpeg1_video_t *v, unsigned addr,
      unsigned flags, const int mv[2][2], unsigned cbp)
{
   int16_t  blk[64];
   uint8_t *py, *pcb, *pcr;
   unsigned mx = addr % v->mb_w;
   unsigned my = addr / v->mb_w;
   unsigned ph = v->mb_h * 16;
   unsigned ch = v->mb_h * 8;
   int      i;
   int      fwd_on = (flags & RMPEG1_MB_FORWARD)  ? 1 : 0;
   int      bwd_on = (flags & RMPEG1_MB_BACKWARD) ? 1 : 0;

   /* Scratch for the second prediction when interpolating. 16x16 luma plus
    * two 8x8 chroma; small enough to keep automatic and still leave the
    * frame far inside the stack budget. */
   uint8_t  tmp_y[16 * 16];
   uint8_t  tmp_cb[8 * 8];
   uint8_t  tmp_cr[8 * 8];

   if (addr >= v->mb_w * v->mb_h)
      return false;

   /* A macroblock with neither direction set only occurs as a skipped
    * macroblock in a P picture, where forward prediction with a zero vector
    * is exactly what is wanted. */
   if (!fwd_on && !bwd_on)
      fwd_on = 1;

   mb_plane_ptrs(v, addr, &py, &pcb, &pcr);

   {
      int d;

      for (d = 0; d < 2; d++)
      {
         int      slot, cmvx, cmvy;
         uint8_t *dy, *dcb, *dcr;
         unsigned dsy, dsc;

         if (d == 0 && !fwd_on)
            continue;
         if (d == 1 && !bwd_on)
            continue;

         /* Which slot a direction refers to depends on the picture type.
          *
          * A B picture sits between two references: forward is the older
          * one (slot_fwd), backward the newer (slot_bwd).
          *
          * A P picture has only one reference, the most recent, and that is
          * whatever is in slot_bwd -- slot_fwd holds the reference before
          * it, which a P picture must not touch. Reading slot_fwd here
          * predicts every P picture from a frame two references old, which
          * looks plausible on static content and falls apart on motion. */
         if (d == 0)
            slot = (v->coding_type == RMPEG1_PIC_B) ? v->slot_fwd
                                                    : v->slot_bwd;
         else
            slot = v->slot_bwd;

         /* The first prediction goes straight to the frame; a second one
          * lands in scratch so the two can be averaged. */
         if (d == 1 && fwd_on)
         {
            dy = tmp_y;  dcb = tmp_cb; dcr = tmp_cr;
            dsy = 16;    dsc = 8;
         }
         else
         {
            dy = py;     dcb = pcb;    dcr = pcr;
            dsy = v->y_stride; dsc = v->c_stride;
         }

         mc_predict(RM_Y(v, slot), v->y_stride, v->y_stride, ph,
                    dy, dsy, (int)mx * 16, (int)my * 16,
                    mv[d][0], mv[d][1], 16, 16);

         /* Chroma is half resolution in both axes, so the vector halves.
          * The specification divides with truncation toward zero here, not
          * the arithmetic shift used for the integer part of a luma
          * vector. */
         cmvx = mv[d][0] / 2;
         cmvy = mv[d][1] / 2;

         mc_predict(RM_CB(v, slot), v->c_stride, v->c_stride, ch,
                    dcb, dsc, (int)mx * 8, (int)my * 8, cmvx, cmvy, 8, 8);
         mc_predict(RM_CR(v, slot), v->c_stride, v->c_stride, ch,
                    dcr, dsc, (int)mx * 8, (int)my * 8, cmvx, cmvy, 8, 8);
      }

      if (fwd_on && bwd_on)
      {
         unsigned r, c;

         for (r = 0; r < 16; r++)
            for (c = 0; c < 16; c++)
            {
               uint8_t *p = py + (size_t)r * v->y_stride + c;
               *p = (uint8_t)((*p + tmp_y[r * 16 + c] + 1) >> 1);
            }
         for (r = 0; r < 8; r++)
            for (c = 0; c < 8; c++)
            {
               uint8_t *p = pcb + (size_t)r * v->c_stride + c;
               uint8_t *q = pcr + (size_t)r * v->c_stride + c;
               *p = (uint8_t)((*p + tmp_cb[r * 8 + c] + 1) >> 1);
               *q = (uint8_t)((*q + tmp_cr[r * 8 + c] + 1) >> 1);
            }
      }
   }

   for (i = 0; i < 4; i++)
   {
      if (cbp & (1u << (5 - i)))
      {
         uint8_t *dst = py + (size_t)(i >> 1) * 8 * v->y_stride
                           + (size_t)(i & 1) * 8;
         if (!decode_inter_block(v, blk))
            return false;
         idct_block_add(blk, dst, v->y_stride);
      }
   }

   if (cbp & 0x02)
   {
      if (!decode_inter_block(v, blk))
         return false;
      idct_block_add(blk, pcb, v->c_stride);
   }
   if (cbp & 0x01)
   {
      if (!decode_inter_block(v, blk))
         return false;
      idct_block_add(blk, pcr, v->c_stride);
   }

   return true;
}

static bool decode_intra_macroblock(rmpeg1_video_t *v, unsigned addr)
{
   int16_t  blk[64];
   uint8_t *py, *pcb, *pcr;
   int      i;

   if (addr >= v->mb_w * v->mb_h)
      return false;

   mb_plane_ptrs(v, addr, &py, &pcb, &pcr);

   for (i = 0; i < 4; i++)
   {
      uint8_t *dst = py + (size_t)(i >> 1) * 8 * v->y_stride + (size_t)(i & 1) * 8;

      if (!decode_intra_block(v, blk, 0))
         return false;
      idct_block(blk, dst, v->y_stride);
   }

   if (!decode_intra_block(v, blk, 1))
      return false;
   idct_block(blk, pcb, v->c_stride);

   if (!decode_intra_block(v, blk, 2))
      return false;
   idct_block(blk, pcr, v->c_stride);

   return true;
}

/* --------------------------------------------------------------------- */
/* Slice layer                                                           */
/* --------------------------------------------------------------------- */

static void reset_dc_predictors(rmpeg1_video_t *v)
{
   /* 11172-2: predictors reset to 1024 (i.e. 128 << 3) at the start of every
    * slice, so a lost slice cannot propagate a DC error across the picture. */
   v->dc_pred[0] = v->dc_pred[1] = v->dc_pred[2] = 1024 / 8;
}

/* `end` is the byte offset of the start code that terminates this slice, so
 * the macroblock loop is bounded by the slice rather than by the buffer. A
 * damaged slice then cannot run on into the next one, and the final slice of
 * a stream -- which has no following start code at all -- ends normally
 * instead of being reported as an error. */
static bool decode_slice(rmpeg1_video_t *v, unsigned vertical_pos, size_t end)
{
   unsigned mb_row;

   if (vertical_pos < 1 || vertical_pos > v->mb_h)
      return false;

   mb_row       = vertical_pos - 1;
   v->quant_scale = (int)get_bits(v, 5);
   if (v->quant_scale == 0)
      return false;

   /* extra_bit_slice: a run of optional 8-bit extension bytes, each preceded
    * by a set bit, terminated by a clear bit. */
   while (get_bit(v))
      (void)get_bits(v, 8);

   reset_dc_predictors(v);
   v->mb_addr = mb_row * v->mb_w - 1;
   v->mv_pred[0][0] = v->mv_pred[0][1] = 0;
   v->mv_pred[1][0] = v->mv_pred[1][1] = 0;
   v->last_flags    = 0;
   memset(v->last_mv, 0, sizeof(v->last_mv));

   for (;;)
   {
      const rmpeg1_vlc_t *e;
      unsigned increment = 0;
      unsigned flags, cbp;
      int      mv[2][2];
      int      d;

      memset(mv, 0, sizeof(mv));

      if (v->rd >= end)
         break;
      if (bits_left(v) < 24)
      {
         /* Fewer than a start code's worth of bits left inside the slice:
          * the remainder is the byte-alignment padding that ends every
          * slice, not a truncated macroblock. */
         if (end >= v->wr)
            break;
         return false;
      }
      if (at_start_code(v))
         break;

      for (;;)
      {
         if (peek_bits(v, RMPEG1_MBA_STUFFING_LEN) == RMPEG1_MBA_STUFFING_CODE)
         {
            (void)get_bits(v, RMPEG1_MBA_STUFFING_LEN);
            continue;
         }
         if (peek_bits(v, RMPEG1_MBA_ESCAPE_LEN) == RMPEG1_MBA_ESCAPE_CODE)
         {
            (void)get_bits(v, RMPEG1_MBA_ESCAPE_LEN);
            increment += 33;
            continue;
         }
         break;
      }

      e = vlc_decode(v, RM_VLC_MBA);
      if (!e)
         return false;
      increment += (unsigned)e->a;

      /* Any gap is skipped macroblocks, and what that means depends on the
       * picture type.
       *
       * In a P picture: forward prediction with a zero vector, and the
       * vector predictors reset, so the next coded macroblock's vector is
       * differential against nothing rather than against the last one.
       *
       * In a B picture: repeat the previous macroblock's vectors and
       * prediction mode exactly, and do NOT reset the predictors. Treating
       * a B skip like a P skip produces a picture that looks almost right,
       * which is the worst kind of wrong.
       *
       * Either way the DC predictors reset. */
      if (increment > 1)
      {
         unsigned k;

         if (v->coding_type == RMPEG1_PIC_I)
            return false;   /* skipped macroblocks are illegal in I pictures */

         for (k = 1; k < increment; k++)
         {
            unsigned a2 = v->mb_addr + k;
            unsigned sflags;
            int      smv[2][2];

            if (a2 >= v->mb_w * v->mb_h)
               return false;

            if (v->coding_type == RMPEG1_PIC_B)
            {
               sflags = v->last_flags
                      & (RMPEG1_MB_FORWARD | RMPEG1_MB_BACKWARD);
               memcpy(smv, v->last_mv, sizeof(smv));
            }
            else
            {
               sflags = RMPEG1_MB_FORWARD;
               memset(smv, 0, sizeof(smv));
            }

            if (!decode_inter_macroblock(v, a2, sflags, (const int (*)[2])smv, 0))
               return false;
         }

         if (v->coding_type == RMPEG1_PIC_P)
         {
            v->mv_pred[0][0] = v->mv_pred[0][1] = 0;
            v->mv_pred[1][0] = v->mv_pred[1][1] = 0;
         }

         reset_dc_predictors(v);
      }

      v->mb_addr += increment;

      if (v->coding_type == RMPEG1_PIC_I)
      {
         /* Table B.2: '1' Intra, '01' Intra with a new quantiser scale. */
         if (get_bit(v))
            flags = RMPEG1_MB_INTRA;
         else
         {
            if (!get_bit(v))
               return false;
            flags = RMPEG1_MB_INTRA | RMPEG1_MB_QUANT;
         }
      }
      else
      {
         e = vlc_decode(v, v->coding_type == RMPEG1_PIC_B
                           ? RM_VLC_MB_TYPE_B : RM_VLC_MB_TYPE_P);
         if (!e)
            return false;
         flags = (unsigned)e->a;
      }

      if (flags & RMPEG1_MB_QUANT)
      {
         v->quant_scale = (int)get_bits(v, 5);
         if (v->quant_scale == 0)
            return false;
      }

      for (d = 0; d < 2; d++)
      {
         unsigned bit = (d == 0) ? RMPEG1_MB_FORWARD : RMPEG1_MB_BACKWARD;

         if (flags & bit)
         {
            if (!decode_motion(v, d, &v->mv_pred[d][0], &mv[d][0]))
               return false;
            if (!decode_motion(v, d, &v->mv_pred[d][1], &mv[d][1]))
               return false;

            /* A full-pel vector is stored in whole samples, so it doubles
             * into the half-sample units everything downstream works in. */
            if (v->full_pel[d])
            {
               mv[d][0] *= 2;
               mv[d][1] *= 2;
            }
         }
         else if (v->coding_type != RMPEG1_PIC_B)
         {
            /* No vector in a P picture: predict from the co-located block,
             * and reset the predictor for the same reason as a skip. In a B
             * picture an unused direction leaves its predictor alone. */
            v->mv_pred[d][0] = v->mv_pred[d][1] = 0;
         }
      }

      if (flags & RMPEG1_MB_INTRA)
      {
         if (!decode_intra_macroblock(v, v->mb_addr))
            return false;
         /* An intra macroblock breaks both vector prediction chains. */
         v->mv_pred[0][0] = v->mv_pred[0][1] = 0;
         v->mv_pred[1][0] = v->mv_pred[1][1] = 0;
      }
      else
      {
         cbp = 0;
         if (flags & RMPEG1_MB_PATTERN)
         {
            e = vlc_decode(v, RM_VLC_CBP);
            if (!e)
               return false;
            cbp = (unsigned)e->a;
         }

         if (!decode_inter_macroblock(v, v->mb_addr, flags,
                                      (const int (*)[2])mv, cbp))
            return false;

         /* DC predictors reset on any non-intra macroblock. */
         reset_dc_predictors(v);
      }

      v->last_flags = flags;
      memcpy(v->last_mv, mv, sizeof(mv));
   }

   return true;
}

/* --------------------------------------------------------------------- */
/* Headers                                                               */
/* --------------------------------------------------------------------- */

static bool parse_sequence_header(rmpeg1_video_t *v)
{
   unsigned w, h, i;

   if (bits_left(v) < 64)
      return false;

   w = get_bits(v, 12);
   h = get_bits(v, 12);

   if (w == 0 || h == 0 || w > RMPEG1_MAX_W || h > RMPEG1_MAX_H)
      return false;

   v->aspect_code = get_bits(v, 4);
   v->fps_code    = get_bits(v, 4);

   (void)get_bits(v, 18);        /* bit_rate            */
   (void)get_bit(v);             /* marker              */
   (void)get_bits(v, 10);        /* vbv_buffer_size     */
   (void)get_bit(v);             /* constrained_params  */

   if (get_bit(v))
   {
      for (i = 0; i < 64; i++)
         v->intra_q[rmpeg1_zigzag[i]] = (uint8_t)get_bits(v, 8);
   }
   if (get_bit(v))
   {
      for (i = 0; i < 64; i++)
         v->non_intra_q[rmpeg1_zigzag[i]] = (uint8_t)get_bits(v, 8);
   }

   if (!v->have_seq || w != v->width || h != v->height)
   {
      v->width  = w;
      v->height = h;
      v->mb_w   = (w + 15) / 16;
      v->mb_h   = (h + 15) / 16;

      if (!alloc_planes(v))
         return false;
   }

   v->have_seq = true;
   return true;
}

static bool parse_picture_header(rmpeg1_video_t *v)
{
   /* Parsed into locals and committed only once every field has been
    * validated. The caller rewinds the bitstream on failure (v->rd =
    * save_rd) and retries when more data arrives, so decoder state has to
    * be rewound with it. Assigning straight into v left a rejected
    * header's values behind: a forbidden f_code of 0 was stored, the
    * function returned false, the read cursor went back, and the zero
    * stayed. decode_motion then computed 1 << -1 and asked get_bits for
    * (unsigned)-1 bits. Only fields this picture type actually carries
    * are overwritten - an I picture keeps the previous f_code, exactly as
    * before - so the success path is unchanged. */
   int  temporal_ref, coding_type;
   int  f_code[2];
   bool full_pel[2];

   if (bits_left(v) < 29)
      return false;

   f_code[0]   = v->f_code[0];
   f_code[1]   = v->f_code[1];
   full_pel[0] = v->full_pel[0];
   full_pel[1] = v->full_pel[1];

   temporal_ref = (int)get_bits(v, 10);
   coding_type  = (int)get_bits(v, 3);
   (void)get_bits(v, 16);        /* vbv_delay */

   if (coding_type == RMPEG1_PIC_P || coding_type == RMPEG1_PIC_B)
   {
      full_pel[0] = get_bit(v) ? true : false;
      f_code[0]   = (int)get_bits(v, 3);
      if (f_code[0] < 1)
         return false;           /* forward_f_code 0 is forbidden */
   }
   if (coding_type == RMPEG1_PIC_B)
   {
      full_pel[1] = get_bit(v) ? true : false;
      f_code[1]   = (int)get_bits(v, 3);
      if (f_code[1] < 1)
         return false;           /* backward_f_code 0 is forbidden */
   }

   while (get_bit(v))
      (void)get_bits(v, 8);      /* extra_information_picture */

   v->temporal_ref = temporal_ref;
   v->coding_type  = coding_type;
   v->f_code[0]    = f_code[0];
   v->f_code[1]    = f_code[1];
   v->full_pel[0]  = full_pel[0];
   v->full_pel[1]  = full_pel[1];

   return true;
}

/* --------------------------------------------------------------------- */
/* Frame ordering                                                        */
/* --------------------------------------------------------------------- */

static bool picture_decodable(const rmpeg1_video_t *v)
{
   if (!v->have_seq)
      return false;

   switch (v->coding_type)
   {
      case RMPEG1_PIC_I:
         return true;
      case RMPEG1_PIC_P:
         return v->have_bwd;              /* predicts from the last ref */
      case RMPEG1_PIC_B:
         return v->have_fwd && v->have_bwd;
      default:
         break;
   }
   return false;
}

static void fill_frame(rmpeg1_video_t *v, int slot,
      rmpeg1_video_frame_t *out, unsigned tref, unsigned ctype)
{
   out->y            = RM_Y(v, slot);
   out->cb           = RM_CB(v, slot);
   out->cr           = RM_CR(v, slot);
   out->width        = v->width;
   out->height       = v->height;
   out->y_stride     = v->y_stride;
   out->c_stride     = v->c_stride;
   out->temporal_ref = tref;
   out->coding_type  = (uint8_t)ctype;
}

/* Emit in display order and rotate the reference slots.
 *
 * A B picture is never a reference, so it goes out as soon as it is decoded.
 * An I or P picture cannot: the B pictures that follow it in the bitstream
 * are displayed before it. So decoding one releases the *previous* future
 * reference instead, and the new picture takes its place. That is the whole
 * of the reordering -- a one-picture delay on references and none on B.
 *
 * Nothing is emitted for the very first reference picture of a stream, since
 * there is no older one to release yet; rmpeg1_video_flush() drains it at
 * the end.
 *
 * out->y is left NULL when there is nothing to display this call. */
static void emit_picture(rmpeg1_video_t *v, rmpeg1_video_frame_t *out)
{
   memset(out, 0, sizeof(*out));

   if (v->coding_type == RMPEG1_PIC_B)
   {
      fill_frame(v, v->slot_cur, out, v->temporal_ref, v->coding_type);
      return;
   }

   if (v->have_bwd)
      fill_frame(v, v->slot_bwd, out, v->bwd_tref, v->bwd_type);

   {
      int freed = v->slot_fwd;

      v->slot_fwd = v->slot_bwd;
      v->slot_bwd = v->slot_cur;
      v->slot_cur = freed;
   }

   v->have_fwd  = v->have_bwd;
   v->have_bwd  = true;
   v->bwd_tref  = v->temporal_ref;
   v->bwd_type  = v->coding_type;

   /* The frame just handed out now lives in slot_fwd and is not written
    * again until two reference pictures later, so the caller's pointers stay
    * valid for the lifetime the header promises. */
}

/* --------------------------------------------------------------------- */
/* Public entry points                                                   */
/* --------------------------------------------------------------------- */

rmpeg1_video_t *rmpeg1_video_init(void)
{
   rmpeg1_video_t *v = (rmpeg1_video_t *)calloc(1, sizeof(*v));

   if (!v)
      return NULL;

   v->buf = (uint8_t *)malloc(RMPEG1_WINDOW);
   if (!v->buf)
   {
      free(v);
      return NULL;
   }
   v->cap = RMPEG1_WINDOW;

   build_luts(v);

   memcpy(v->intra_q, rmpeg1_default_intra, 64);
   memset(v->non_intra_q, 16, 64);

   return v;
}

void rmpeg1_video_free(rmpeg1_video_t *v)
{
   if (!v)
      return;
   free(v->buf);
   free(v->plane_arena);
   free(v);
}

void rmpeg1_video_flush(rmpeg1_video_t *v)
{
   if (v)
      v->eof = true;
}

/* After the last picture, the future reference is still held back. Release
 * it once. */
static int drain_held(rmpeg1_video_t *v, rmpeg1_video_frame_t *out)
{
   if (!v->eof || !v->have_bwd)
      return 0;

   fill_frame(v, v->slot_bwd, out, v->bwd_tref, v->bwd_type);
   v->have_bwd = false;
   return 1;
}

void rmpeg1_video_reset(rmpeg1_video_t *v)
{
   if (!v)
      return;
   v->eof = false;
   v->rd = v->wr = 0;
   v->bit = 0;
   v->have_fwd = false;
   v->have_bwd = false;
}

static void compact(rmpeg1_video_t *v)
{
   if (v->rd == 0)
      return;
   if (v->wr > v->rd)
      memmove(v->buf, v->buf + v->rd, v->wr - v->rd);
   v->wr -= v->rd;
   v->rd  = 0;
}

size_t rmpeg1_video_write(rmpeg1_video_t *v, const uint8_t *data, size_t len)
{
   size_t room;

   if (!v || !data || len == 0)
      return 0;

   if (v->cap - v->wr < len)
   {
      /* Only safe when the bit cursor is byte aligned and nothing is
       * mid-parse; decode() always leaves it that way between pictures. */
      if (v->bit == 0)
         compact(v);
   }

   room = v->cap - v->wr;
   if (len > room)
      len = room;

   if (len)
   {
      memcpy(v->buf + v->wr, data, len);
      v->wr += len;
   }
   return len;
}

int rmpeg1_video_decode(rmpeg1_video_t *v, rmpeg1_video_frame_t *out)
{
   if (!v || !out)
      return 0;

   for (;;)
   {
      size_t save_rd;
      int    code;

      save_rd = v->rd;
      code    = next_start_code(v);

      if (code < 0)
      {
         v->rd = save_rd;
         return drain_held(v, out);
      }

      switch (code)
      {
         case RMPEG1_START_SEQUENCE:
            if (!parse_sequence_header(v))
            {
               v->rd = save_rd + 4;
               v->bit = 0;
               v->errors++;
            }
            break;

         case RMPEG1_START_GOP:
            if (bits_left(v) < 27)
            {
               v->rd = save_rd;
               return 0;
            }
            (void)get_bits(v, 27);
            break;

         case RMPEG1_START_PICTURE:
            if (!parse_picture_header(v))
            {
               v->rd = save_rd;
               return 0;
            }
            /* A predicted picture without its reference cannot be
             * reconstructed. This happens on entry mid-stream, before the
             * first I picture has been seen. */
            if (v->coding_type == RMPEG1_PIC_P && !v->have_bwd)
               v->skipped++;
            else if (     v->coding_type == RMPEG1_PIC_B
                       && (!v->have_fwd || !v->have_bwd))
               v->skipped++;
            else if (     v->coding_type != RMPEG1_PIC_I
                       && v->coding_type != RMPEG1_PIC_P
                       && v->coding_type != RMPEG1_PIC_B)
               v->skipped++;
            break;

         case RMPEG1_START_USER_DATA:
         case RMPEG1_START_EXTENSION:
            /* Consumed by the next start-code hunt. */
            break;

         case RMPEG1_START_SEQ_END:
            break;

         default:
            if (     code >= RMPEG1_START_SLICE_LO
                  && code <= RMPEG1_START_SLICE_HI)
            {
               size_t end;

               if (!v->have_seq)
                  break;

               /* A slice must be decoded atomically. The macroblock layer is
                * a pure bit stream with no length prefix anywhere, so there
                * is no way to suspend in the middle of one and resume later
                * -- and running off the end of the buffer would look exactly
                * like corrupt data. Wait until the whole slice is present,
                * which is known once the following start code is buffered.
                *
                * Getting this wrong is not a crash, it is worse: the decoder
                * reads zero-padding past the write cursor as though it were
                * bitstream, fails a VLC lookup deep inside a block, and
                * reports a table error for what is really a starved buffer. */
               end = scan_start_code(v, v->rd);
               if (end == (size_t)-1)
               {
                  if (!v->eof)
                  {
                     v->rd  = save_rd;
                     v->bit = 0;
                     return 0;
                  }
                  /* No more input is coming, so the tail of the buffer is
                   * the rest of the slice. */
                  end = v->wr;
               }

               if (!picture_decodable(v))
               {
                  v->rd  = end;
                  v->bit = 0;
                  break;
               }

               if (!decode_slice(v, (unsigned)code, end))
                  v->errors++;

               /* Resynchronise on the slice boundary we already found rather
                * than wherever the bit cursor stopped: a slice that ended
                * early must not drag the next one out of alignment. */
               v->rd  = end;
               v->bit = 0;

               {
                  size_t p = end;

                  if (p + 4 > v->wr && !v->eof)
                     return 0;      /* need more data to know */

                  if (     p + 4 <= v->wr
                        && v->buf[p + 3] >= RMPEG1_START_SLICE_LO
                        && v->buf[p + 3] <= RMPEG1_START_SLICE_HI)
                     break;         /* same picture continues */

                  emit_picture(v, out);
                  if (out->y)
                     return 1;
                  break;
               }
            }
            break;
      }
   }
}

bool rmpeg1_video_has_sequence(const rmpeg1_video_t *v)
{
   return v ? v->have_seq : false;
}

unsigned rmpeg1_video_width(const rmpeg1_video_t *v)
{
   return v ? v->width : 0;
}

unsigned rmpeg1_video_height(const rmpeg1_video_t *v)
{
   return v ? v->height : 0;
}

void rmpeg1_video_framerate(const rmpeg1_video_t *v,
      unsigned *num, unsigned *den)
{
   unsigned c = (v && v->fps_code < 16) ? v->fps_code : 0;

   if (num)
      *num = rmpeg1_fps_num[c];
   if (den)
      *den = rmpeg1_fps_den[c];
}

unsigned rmpeg1_video_aspect_code(const rmpeg1_video_t *v)
{
   return v ? v->aspect_code : 0;
}

uint32_t rmpeg1_video_skipped(const rmpeg1_video_t *v)
{
   return v ? v->skipped : 0;
}

uint32_t rmpeg1_video_errors(const rmpeg1_video_t *v)
{
   return v ? v->errors : 0;
}
