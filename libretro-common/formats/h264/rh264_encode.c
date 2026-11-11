/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rh264_encode.c).
 * The CAVLC tables it includes, rh264_cavlc_tables.h, carry their own
 * (BSD-2-Clause, from OpenH264; see README.licenses).
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

/* H.264 Constrained Baseline encoder. See rh264_encode.h. Written from
 * ITU-T H.264 (08/2024); the CAVLC tables are rh264's.
 *
 * The pictures are kept with a border of copies of their edge samples,
 * so motion compensation reads them directly; before a P picture the
 * reference's half sample planes (8.4.2.2.1's b, h and j) are made
 * once, and every quarter sample position is one of those or the
 * rounded mean of two, as the standard's own derivation is. */

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <retro_inline.h>
#include <formats/rh264_encode.h>

#include "rh264_cavlc_tables.h"

#define PAD       32               /* luma border; the chroma one is half */
#define MV_LIMIT  (64 * 4)         /* the most a vector reaches, quarter samples */
#define MAX_LEVEL 2063             /* the largest level a Baseline escape holds */

enum
{
   MB_SKIP = 0,
   MB_P16,
   MB_I16,
   MB_I4
};

typedef struct
{
   int16_t mv[2];
   uint8_t type;
   uint8_t qp;
   uint8_t tc[24];                 /* total coefficients: luma raster 4x4, Cb, Cr */
   int8_t  i4[16];                 /* 4x4 modes, raster; 2 (DC) unless I4 */
} mb_info_t;

struct rh264_encoder
{
   rh264_encoder_config_t cfg;
   unsigned  mb_w, mb_h, w16, h16;
   unsigned  stride, c_stride;     /* of the bordered planes */
   unsigned  level;
   uint64_t  frame;
   unsigned  frame_num, idr_count;
   int       qp, qpc;
   unsigned  lambda;
   int       rc_debt;
   double    cplx_i, cplx_p;

   uint8_t  *mem;
   uint8_t  *pic[2][3];            /* sample (0, 0) of each plane */
   uint8_t  *half[3];              /* the reference's b, h, j */
   int16_t  *tmp;                  /* unclipped b, for j */
   int       cur;
   mb_info_t *mbs;
   unsigned  skip_run;

   uint8_t   sps[64], pps[16];
   size_t    sps_len, pps_len;

   /* output */
   uint8_t  *out;
   size_t    cap, pos, len_at;
   uint64_t  acc;
   unsigned  nbits, zeros;
   bool      escape, overflow;

   /* the macroblock being coded */
   uint8_t   src[384];             /* 16x16 luma, 8x8 Cb, 8x8 Cr */
   uint8_t   pred[384];
   uint8_t   trial[256];           /* a trial prediction */
   int       w[16][16];            /* a macroblock's coefficients */
   int16_t   lv[16][16];           /* luma levels, raster within each block */
   int16_t   dc[16];               /* Intra16x16 DC levels, raster of blocks */
   int16_t   cdc[2][4];
   int16_t   cac[2][4][16];
   int8_t    i4[16];               /* by block index */
   unsigned  i16_mode, c_mode;
   unsigned  cbp_l, cbp_c;
};

/* --------------------------------------------------------------------- */
/* Tables (H.264 clauses 8.5, 8.7 and 9.1.2)                             */
/* --------------------------------------------------------------------- */

static const uint8_t zigzag4[16] = { 0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15 };

/* block index to its 4x4 position in the macroblock */
static const uint8_t blk_x[16] = { 0, 1, 0, 1, 2, 3, 2, 3, 0, 1, 0, 1, 2, 3, 2, 3 };
static const uint8_t blk_y[16] = { 0, 0, 1, 1, 0, 0, 1, 1, 2, 2, 3, 3, 2, 2, 3, 3 };

/* forward quantisation and the decoder's scale, by qP % 6 and position
 * class: (even, even), (odd, odd), the rest */
static const uint16_t quant_mf[6][3] = {
   { 13107, 5243, 8066 }, { 11916, 4660, 7490 }, { 10082, 4194, 6554 },
   { 9362, 3647, 5825 },  { 8192, 3355, 5243 },  { 7282, 2893, 4559 } };
static const uint8_t dequant_v[6][3] = {
   { 10, 16, 13 }, { 11, 18, 14 }, { 13, 20, 16 },
   { 14, 23, 18 }, { 16, 25, 20 }, { 18, 29, 23 } };
static const uint8_t pos_class[16] = { 0, 2, 0, 2, 2, 1, 2, 1, 0, 2, 0, 2, 2, 1, 2, 1 };

static const uint8_t chroma_qp_tab[22] = {
   29, 30, 31, 32, 32, 33, 34, 34, 35, 35, 36, 36, 37, 37, 37, 38, 38, 38, 39, 39, 39, 39 };

/* coded_block_pattern to codeNum, Table 9-4: Intra_4x4 and Inter */
static const uint8_t cbp_code_intra[48] = {
    3, 29, 30, 17, 31, 18, 37,  8, 32, 38, 19,  9, 20, 10, 11,  2,
   16, 33, 34, 21, 35, 22, 39,  4, 36, 40, 23,  5, 24,  6,  7,  1,
   41, 42, 43, 25, 44, 26, 46, 12, 45, 47, 27, 13, 28, 14, 15,  0 };
static const uint8_t cbp_code_inter[48] = {
    0,  2,  3,  7,  4,  8, 17, 13,  5, 18,  9, 14, 10, 15, 16, 11,
    1, 32, 33, 36, 34, 37, 44, 40, 35, 45, 38, 41, 39, 42, 43, 19,
    6, 24, 25, 20, 26, 21, 46, 28, 27, 47, 22, 29, 23, 30, 31, 12 };

static const uint8_t alpha_tab[52] = {
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 4, 5, 6, 7, 8, 9, 10, 12, 13,
   15, 17, 20, 22, 25, 28, 32, 36, 40, 45, 50, 56, 63, 71, 80, 90, 101, 113, 127,
   144, 162, 182, 203, 226, 255, 255 };
static const uint8_t beta_tab[52] = {
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4,
   6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18 };
static const uint8_t tc0_tab[52][3] = {
   { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
   { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 },
   { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 1 },
   { 0, 0, 1 }, { 0, 0, 1 }, { 0, 0, 1 }, { 0, 1, 1 }, { 0, 1, 1 }, { 1, 1, 1 },
   { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 2 }, { 1, 1, 2 }, { 1, 1, 2 },
   { 1, 1, 2 }, { 1, 2, 3 }, { 1, 2, 3 }, { 2, 2, 3 }, { 2, 2, 4 }, { 2, 3, 4 },
   { 2, 3, 4 }, { 3, 3, 5 }, { 3, 4, 6 }, { 3, 4, 6 }, { 4, 5, 7 }, { 4, 5, 8 },
   { 4, 6, 9 }, { 5, 7, 10 }, { 6, 8, 11 }, { 6, 8, 13 }, { 7, 10, 14 }, { 8, 11, 16 },
   { 9, 12, 18 }, { 10, 13, 20 }, { 11, 15, 23 }, { 13, 17, 25 } };

/* Table A-1: level, MaxMBPS, MaxFS, MaxBR (kbit/s, Baseline) */
static const uint32_t level_tab[][4] = {
   { 10, 1485, 99, 64 },          { 11, 3000, 396, 192 },
   { 12, 6000, 396, 384 },        { 13, 11880, 396, 768 },
   { 20, 11880, 396, 2000 },      { 21, 19800, 792, 4000 },
   { 22, 20250, 1620, 4000 },     { 30, 40500, 1620, 10000 },
   { 31, 108000, 3600, 14000 },   { 32, 216000, 5120, 20000 },
   { 40, 245760, 8192, 20000 },   { 41, 245760, 8192, 50000 },
   { 42, 522240, 8704, 50000 },   { 50, 589824, 22080, 135000 },
   { 51, 983040, 36864, 240000 }, { 52, 2073600, 36864, 240000 },
   { 60, 4177920, 139264, 240000 }, { 61, 8355840, 139264, 480000 },
   { 62, 16711680, 139264, 800000 } };

/* the cost of a bit against SAD and SATD, 2^((qp - 12) / 6) */
static const uint8_t lambda_tab[52] = {
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 4, 4, 4,
   5, 6, 6, 7, 8, 9, 10, 11, 13, 14, 16, 18, 20, 23, 25, 29, 32, 36, 40, 45, 51,
   57, 64, 72, 81, 91 };

static INLINE int clip3(int lo, int hi, int v)
{
   return v < lo ? lo : v > hi ? hi : v;
}

static INLINE uint8_t clip1(int v)
{
   return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
}

static int chroma_qp(int qp)
{
   return qp < 30 ? qp : chroma_qp_tab[qp - 30];
}

/* --------------------------------------------------------------------- */
/* Bits                                                                  */
/* --------------------------------------------------------------------- */

static INLINE void out_byte(rh264_encoder_t *e, unsigned b)
{
   /* emulation prevention, 7.4.1: no 00 00 0x with x <= 3 */
   if (e->escape && e->zeros >= 2 && b <= 3)
   {
      if (e->pos < e->cap)
         e->out[e->pos++] = 3;
      else
         e->overflow = true;
      e->zeros = 0;
   }
   if (e->pos < e->cap)
      e->out[e->pos++] = (uint8_t)b;
   else
      e->overflow = true;
   e->zeros = b ? 0 : e->zeros + 1;
}

static void put_bits(rh264_encoder_t *e, uint32_t v, unsigned n)
{
   if (!n)
      return;
   e->acc    = (e->acc << n) | (v & (n == 32 ? 0xFFFFFFFFu : ((1u << n) - 1)));
   e->nbits += n;
   while (e->nbits >= 8)
   {
      e->nbits -= 8;
      out_byte(e, (unsigned)(e->acc >> e->nbits) & 0xFF);
   }
}

static unsigned ue_bits(unsigned v)
{
   unsigned n = 0, x = v + 1;
   while (x >> n)
      n++;
   return 2 * n - 1;
}

static void put_ue(rh264_encoder_t *e, unsigned v)
{
   unsigned n = 0, x = v + 1;
   while (x >> n)
      n++;
   put_bits(e, 0, n - 1);
   put_bits(e, x, n);
}

static INLINE unsigned se_code(int v)
{
   return v > 0 ? (unsigned)(2 * v - 1) : (unsigned)(-2 * v);
}

static void put_se(rh264_encoder_t *e, int v)
{
   put_ue(e, se_code(v));
}

static void begin(rh264_encoder_t *e, uint8_t *dst, size_t cap)
{
   e->out      = dst;
   e->cap      = cap;
   e->pos      = 0;
   e->acc      = 0;
   e->nbits    = 0;
   e->zeros    = 0;
   e->escape   = false;
   e->overflow = false;
}

/* A NAL unit's start: a start code or a length to fill in, the header */
static void nal_begin(rh264_encoder_t *e, unsigned ref_idc, unsigned type, bool annexb)
{
   e->escape = false;
   if (annexb)
      put_bits(e, 1, 32);
   else
   {
      e->len_at = e->pos;
      put_bits(e, 0, 32);
   }
   put_bits(e, (ref_idc << 5) | type, 8);
   e->escape = true;
   e->zeros  = 0;
}

/* rbsp_trailing_bits(), and the length */
static void nal_end(rh264_encoder_t *e, bool annexb)
{
   put_bits(e, 1, 1);
   put_bits(e, 0, (8 - e->nbits) & 7);
   e->escape = false;
   if (!annexb && !e->overflow)
   {
      size_t n = e->pos - e->len_at - 4;
      e->out[e->len_at]     = (uint8_t)(n >> 24);
      e->out[e->len_at + 1] = (uint8_t)(n >> 16);
      e->out[e->len_at + 2] = (uint8_t)(n >> 8);
      e->out[e->len_at + 3] = (uint8_t)n;
   }
}

/* --------------------------------------------------------------------- */
/* Parameter sets (7.3.2.1, 7.3.2.2, E.1)                                */
/* --------------------------------------------------------------------- */

static void write_sps(rh264_encoder_t *e)
{
   unsigned crop_r = (e->w16 - e->cfg.width) / 2, crop_b = (e->h16 - e->cfg.height) / 2;
   put_bits(e, 66, 8);                    /* Baseline */
   put_bits(e, 0xC0, 8);                  /* constraint_set0 and 1: Constrained Baseline */
   put_bits(e, e->level, 8);
   put_ue(e, 0);                          /* seq_parameter_set_id */
   put_ue(e, 0);                          /* log2_max_frame_num_minus4 */
   put_ue(e, 2);                          /* pic_order_cnt_type: output order is decode order */
   put_ue(e, 1);                          /* max_num_ref_frames */
   put_bits(e, 0, 1);                     /* gaps_in_frame_num_value_allowed */
   put_ue(e, e->mb_w - 1);
   put_ue(e, e->mb_h - 1);
   put_bits(e, 1, 1);                     /* frame_mbs_only */
   put_bits(e, 1, 1);                     /* direct_8x8_inference */
   put_bits(e, crop_r || crop_b, 1);
   if (crop_r || crop_b)
   {
      put_ue(e, 0);
      put_ue(e, crop_r);
      put_ue(e, 0);
      put_ue(e, crop_b);
   }
   put_bits(e, 1, 1);                     /* vui_parameters_present */
   put_bits(e, 1, 1);                     /* aspect_ratio_info_present */
   put_bits(e, 1, 8);                     /* square samples */
   put_bits(e, 0, 1);                     /* overscan_info_present */
   put_bits(e, 1, 1);                     /* video_signal_type_present */
   put_bits(e, 5, 3);                     /* unspecified format */
   put_bits(e, 0, 1);                     /* limited range */
   put_bits(e, 1, 1);                     /* colour_description_present */
   put_bits(e, 6, 8);                     /* BT.601 (SMPTE 170M) */
   put_bits(e, 6, 8);
   put_bits(e, 6, 8);
   put_bits(e, 0, 1);                     /* chroma_loc_info_present */
   put_bits(e, 1, 1);                     /* timing_info_present */
   put_bits(e, e->cfg.fps_den, 32);       /* num_units_in_tick */
   put_bits(e, 2 * e->cfg.fps_num, 32);   /* time_scale: two ticks a frame */
   put_bits(e, 1, 1);                     /* fixed_frame_rate */
   put_bits(e, 0, 1);                     /* nal_hrd_parameters_present */
   put_bits(e, 0, 1);                     /* vcl_hrd_parameters_present */
   put_bits(e, 0, 1);                     /* pic_struct_present */
   put_bits(e, 1, 1);                     /* bitstream_restriction */
   put_bits(e, 1, 1);                     /* motion_vectors_over_pic_boundaries */
   put_ue(e, 0);                          /* max_bytes_per_pic_denom */
   put_ue(e, 0);                          /* max_bits_per_mb_denom */
   put_ue(e, 9);                          /* log2_max_mv_length: 64 samples and a bit */
   put_ue(e, 9);
   put_ue(e, 0);                          /* max_num_reorder_frames: none */
   put_ue(e, 1);                          /* max_dec_frame_buffering */
}

static void write_pps(rh264_encoder_t *e)
{
   put_ue(e, 0);                          /* pic_parameter_set_id */
   put_ue(e, 0);                          /* seq_parameter_set_id */
   put_bits(e, 0, 1);                     /* CAVLC */
   put_bits(e, 0, 1);                     /* bottom_field_pic_order_in_frame_present */
   put_ue(e, 0);                          /* num_slice_groups_minus1 */
   put_ue(e, 0);                          /* num_ref_idx_l0_default_active_minus1 */
   put_ue(e, 0);
   put_bits(e, 0, 1);                     /* weighted_pred */
   put_bits(e, 0, 2);                     /* weighted_bipred_idc */
   put_se(e, 0);                          /* pic_init_qp_minus26 */
   put_se(e, 0);                          /* pic_init_qs_minus26 */
   put_se(e, 0);                          /* chroma_qp_index_offset */
   put_bits(e, 1, 1);                     /* deblocking_filter_control_present */
   put_bits(e, 0, 1);                     /* constrained_intra_pred */
   put_bits(e, 0, 1);                     /* redundant_pic_cnt_present */
}

/* A parameter set into its buffer, header and escapes included */
static size_t make_ps(rh264_encoder_t *e, uint8_t *buf, size_t cap, unsigned type)
{
   begin(e, buf, cap);
   put_bits(e, (3 << 5) | type, 8);
   e->escape = true;
   if (type == 7)
      write_sps(e);
   else
      write_pps(e);
   put_bits(e, 1, 1);
   put_bits(e, 0, (8 - e->nbits) & 7);
   return e->overflow ? 0 : e->pos;
}

/* --------------------------------------------------------------------- */
/* Setup                                                                 */
/* --------------------------------------------------------------------- */

static unsigned choose_level(const rh264_encoder_config_t *c, unsigned mbs)
{
   unsigned i;
   double   mbps = (double)mbs * c->fps_num / c->fps_den;
   for (i = 0; i < sizeof(level_tab) / sizeof(level_tab[0]); i++)
   {
      /* no side longer than sqrt(8 MaxFS) macroblocks (A.3.1 h) */
      double side = sqrt(8.0 * level_tab[i][2]);
      if (mbs <= level_tab[i][2] && mbps <= level_tab[i][1]
            && (c->width + 15) / 16 <= side && (c->height + 15) / 16 <= side
            && c->bitrate <= level_tab[i][3] * 1000.0)
         return level_tab[i][0];
   }
   return 62;
}

rh264_encoder_t *rh264_encoder_new(const rh264_encoder_config_t *c)
{
   rh264_encoder_t *e;
   size_t ys, cs, n, total;
   uint8_t *p;
   unsigned rows, c_rows;
   int s, k;

   if (!c || !c->width || !c->height || c->width > 4096 || c->height > 4096
         || (c->width & 1) || (c->height & 1) || !c->fps_num || !c->fps_den
         || c->fps_num > 0x7FFFFFFFu)
      return NULL;
   if (!(e = (rh264_encoder_t*)calloc(1, sizeof(*e))))
      return NULL;
   e->cfg = *c;
   if (!e->cfg.gop)
      e->cfg.gop = 60;
   e->qp = c->qp < 1 ? 26 : c->qp > 51 ? 51 : (int)c->qp;

   e->mb_w     = (c->width + 15) / 16;
   e->mb_h     = (c->height + 15) / 16;
   e->w16      = e->mb_w * 16;
   e->h16      = e->mb_h * 16;
   e->stride   = e->w16 + 2 * PAD;
   e->c_stride = e->w16 / 2 + PAD;
   rows        = e->h16 + 2 * PAD;
   c_rows      = e->h16 / 2 + PAD;
   ys    = (size_t)e->stride * rows;
   cs    = (size_t)e->c_stride * c_rows;
   n     = (size_t)e->mb_w * e->mb_h;
   total = 2 * (ys + 2 * cs) + 3 * ys + ys * sizeof(int16_t) + n * sizeof(mb_info_t) + 64;
   if (!(e->mem = (uint8_t*)calloc(1, total)))
   {
      free(e);
      return NULL;
   }
   p = e->mem;
   e->tmp = (int16_t*)p;                  p += ys * sizeof(int16_t);
   e->mbs = (mb_info_t*)p;                p += n * sizeof(mb_info_t);
   for (s = 0; s < 2; s++)
   {
      e->pic[s][0] = p + (size_t)PAD * e->stride + PAD;            p += ys;
      e->pic[s][1] = p + (size_t)(PAD / 2) * e->c_stride + PAD / 2; p += cs;
      e->pic[s][2] = p + (size_t)(PAD / 2) * e->c_stride + PAD / 2; p += cs;
   }
   for (k = 0; k < 3; k++)
   {
      e->half[k] = p + (size_t)PAD * e->stride + PAD;
      p += ys;
   }

   e->level   = choose_level(c, (unsigned)n);
   e->sps_len = make_ps(e, e->sps, sizeof(e->sps), 7);
   e->pps_len = make_ps(e, e->pps, sizeof(e->pps), 8);
   if (!e->sps_len || !e->pps_len)
   {
      rh264_encoder_free(e);
      return NULL;
   }
   return e;
}

void rh264_encoder_free(rh264_encoder_t *e)
{
   if (!e)
      return;
   free(e->mem);
   free(e);
}

size_t rh264_encoder_bound(const rh264_encoder_t *e)
{
   /* a macroblock's 27 blocks at the longest codes, every third byte
    * an escape, and the parameter sets */
   return e ? (size_t)e->mb_w * e->mb_h * 3300 + 1024 : 0;
}

size_t rh264_encoder_avcc(rh264_encoder_t *e, uint8_t *dst, size_t cap)
{
   size_t n;
   if (!e || !dst)
      return 0;
   n = 11 + e->sps_len + e->pps_len;
   if (cap < n)
      return 0;
   dst[0]  = 1;                           /* configurationVersion */
   dst[1]  = e->sps[1];                   /* profile, compatibility, level */
   dst[2]  = e->sps[2];
   dst[3]  = e->sps[3];
   dst[4]  = 0xFF;                        /* 4-byte lengths */
   dst[5]  = 0xE1;                        /* one SPS */
   dst[6]  = (uint8_t)(e->sps_len >> 8);
   dst[7]  = (uint8_t)e->sps_len;
   memcpy(dst + 8, e->sps, e->sps_len);
   dst[8 + e->sps_len]  = 1;              /* one PPS */
   dst[9 + e->sps_len]  = (uint8_t)(e->pps_len >> 8);
   dst[10 + e->sps_len] = (uint8_t)e->pps_len;
   memcpy(dst + 11 + e->sps_len, e->pps, e->pps_len);
   return n;
}

/* --------------------------------------------------------------------- */
/* Transform and quantisation (8.5.12, 8.6)                              */
/* --------------------------------------------------------------------- */

/* the forward core transform of a 4x4 difference, raster */
static void fdct4(const int *d, int *w)
{
   int t[16], i;
   for (i = 0; i < 4; i++)
   {
      const int *r = d + i * 4;
      int s0 = r[0] + r[3], s3 = r[0] - r[3], s1 = r[1] + r[2], s2 = r[1] - r[2];
      t[i * 4 + 0] = s0 + s1;
      t[i * 4 + 2] = s0 - s1;
      t[i * 4 + 1] = 2 * s3 + s2;
      t[i * 4 + 3] = s3 - 2 * s2;
   }
   for (i = 0; i < 4; i++)
   {
      int s0 = t[i] + t[12 + i], s3 = t[i] - t[12 + i];
      int s1 = t[4 + i] + t[8 + i], s2 = t[4 + i] - t[8 + i];
      w[i]      = s0 + s1;
      w[8 + i]  = s0 - s1;
      w[4 + i]  = 2 * s3 + s2;
      w[12 + i] = s3 - 2 * s2;
   }
}

/* the inverse, 8.5.12.2, of scaled coefficients, added to @dst */
static void idct4_add(const int *c, uint8_t *dst, unsigned stride)
{
   int t[16], i;
   for (i = 0; i < 4; i++)
   {
      const int *r = c + i * 4;
      int e0 = r[0] + r[2], e1 = r[0] - r[2];
      int e2 = (r[1] >> 1) - r[3], e3 = r[1] + (r[3] >> 1);
      t[i * 4 + 0] = e0 + e3;
      t[i * 4 + 1] = e1 + e2;
      t[i * 4 + 2] = e1 - e2;
      t[i * 4 + 3] = e0 - e3;
   }
   for (i = 0; i < 4; i++)
   {
      int e0 = t[i] + t[8 + i], e1 = t[i] - t[8 + i];
      int e2 = (t[4 + i] >> 1) - t[12 + i], e3 = t[4 + i] + (t[12 + i] >> 1);
      dst[i]              = clip1(dst[i]              + ((e0 + e3 + 32) >> 6));
      dst[stride + i]     = clip1(dst[stride + i]     + ((e1 + e2 + 32) >> 6));
      dst[2 * stride + i] = clip1(dst[2 * stride + i] + ((e1 - e2 + 32) >> 6));
      dst[3 * stride + i] = clip1(dst[3 * stride + i] + ((e0 - e3 + 32) >> 6));
   }
}

static INLINE int quant1(int w, unsigned mf, unsigned qbits, int f)
{
   int l = (int)(((unsigned)abs(w) * mf + (unsigned)f) >> qbits);
   if (l > MAX_LEVEL)
      l = MAX_LEVEL;
   return w < 0 ? -l : l;
}

/* A block's coefficients to levels from @first (1 when the DC goes
 * elsewhere); returns how many are not zero */
static unsigned quant4(const int *w, int16_t *lv, int qp, bool intra, unsigned first)
{
   unsigned qbits = 15 + (unsigned)qp / 6, i, nz = 0;
   int      f     = (1 << qbits) / (intra ? 3 : 6);
   for (i = 0; i < 16; i++)
   {
      lv[i] = 0;
      if (i < first)
         continue;
      lv[i] = (int16_t)quant1(w[i], quant_mf[qp % 6][pos_class[i]], qbits, f);
      nz   += lv[i] != 0;
   }
   return nz;
}

/* levels to the scaled coefficients the inverse takes, 8.5.12.1, the DC
 * as given when @dc */
static void dequant4(const int16_t *lv, int *c, int qp, const int *dc)
{
   unsigned i, sh = (unsigned)qp / 6;
   for (i = 0; i < 16; i++)
      c[i] = lv[i] * dequant_v[qp % 6][pos_class[i]] * (1 << sh);
   if (dc)
      c[0] = *dc;
}

static unsigned count_nz(const int16_t *lv, unsigned first)
{
   unsigned i, n = 0;
   for (i = first; i < 16; i++)
      n += lv[i] != 0;
   return n;
}

/* --------------------------------------------------------------------- */
/* CAVLC (9.2)                                                           */
/* --------------------------------------------------------------------- */

/* One block's levels, in scan order, @n of them; @nc -1 for chroma DC */
static void write_block(rh264_encoder_t *e, const int16_t *lv, unsigned n, int nc)
{
   int      lev[16];
   unsigned run[16];
   unsigned tc = 0, t1 = 0, tz = 0, i, sl, zl;
   int      last = -1;

   for (i = 0; i < n; i++)
      if (lv[i])
         last = (int)i;
   /* the levels from the last one down, with the zeros below each */
   if (last >= 0)
   {
      int k = last;
      while (k >= 0)
      {
         unsigned z = 0;
         lev[tc] = lv[k];
         k--;
         while (k >= 0 && !lv[k])
         {
            z++;
            k--;
         }
         run[tc++] = z;                    /* the lowest one's are its run to 0 */
      }
      for (i = 0; i < tc; i++)
         tz += run[i];
      for (i = 0; i < tc && i < 3 && (lev[i] == 1 || lev[i] == -1); i++)
         t1++;
   }

   /* coeff_token */
   if (nc < 0)
      put_bits(e, rh264_ctc_code[tc][t1], rh264_ctc_len[tc][t1]);
   else
   {
      unsigned t = nc < 2 ? 0 : nc < 4 ? 1 : nc < 8 ? 2 : 3;
      put_bits(e, rh264_ct_code[t][tc][t1], rh264_ct_len[t][tc][t1]);
   }
   if (!tc)
      return;

   for (i = 0; i < t1; i++)
      put_bits(e, lev[i] < 0, 1);
   sl = tc > 10 && t1 < 3;
   for (i = t1; i < tc; i++)
   {
      int      v    = lev[i];
      unsigned code = v > 0 ? (unsigned)(2 * v - 2) : (unsigned)(-2 * v - 1);
      if (i == t1 && t1 < 3)
         code -= 2;
      if (sl == 0)
      {
         if (code < 14)
            put_bits(e, 1, code + 1);
         else if (code < 30)
         {
            put_bits(e, 1, 15);
            put_bits(e, code - 14, 4);
         }
         else
         {
            put_bits(e, 1, 16);
            put_bits(e, code - 30, 12);
         }
      }
      else if ((code >> sl) < 15)
      {
         put_bits(e, 1, (code >> sl) + 1);
         put_bits(e, code & ((1u << sl) - 1), sl);
      }
      else
      {
         put_bits(e, 1, 16);
         put_bits(e, code - (15u << sl), 12);
      }
      if (sl == 0)
         sl = 1;
      if ((unsigned)abs(v) > (3u << (sl - 1)) && sl < 6)
         sl++;
   }

   if (tc < n)
   {
      if (nc < 0)
         put_bits(e, rh264_cdctz_code[tc][tz], rh264_cdctz_len[tc][tz]);
      else
         put_bits(e, rh264_tz_code[tc][tz], rh264_tz_len[tc][tz]);
   }
   zl = tz;
   for (i = 0; i + 1 < tc && zl; i++)
   {
      unsigned r = zl > 7 ? 7 : zl;
      put_bits(e, rh264_rb_code[r][run[i]], rh264_rb_len[r][run[i]]);
      zl -= run[i];
   }
}

/* nC for a luma 4x4 block at (bx, by) of the macroblock, or a chroma one
 * (@c 1 or 2, a 2x2 grid), 9.2.1 */
static int block_nc(const rh264_encoder_t *e, unsigned mbx, unsigned mby,
      unsigned bx, unsigned by, unsigned c)
{
   const mb_info_t *m = &e->mbs[mby * e->mb_w + mbx];
   unsigned base = c ? 12 + 4 * c : 0, w = c ? 2 : 4;
   int na = -1, nb = -1;
   if (bx)
      na = m->tc[base + by * w + bx - 1];
   else if (mbx)
      na = (m - 1)->tc[base + by * w + w - 1];
   if (by)
      nb = m->tc[base + (by - 1) * w + bx];
   else if (mby)
      nb = (m - e->mb_w)->tc[base + (w - 1) * w + bx];
   if (na >= 0 && nb >= 0)
      return (na + nb + 1) >> 1;
   return na >= 0 ? na : nb >= 0 ? nb : 0;
}

/* The macroblock's residual(), the luma blocks raster within each */
static void write_residual(rh264_encoder_t *e, unsigned mbx, unsigned mby, bool i16)
{
   int16_t  s[16];
   unsigned b, k, c;
   if (i16)
   {
      for (k = 0; k < 16; k++)
         s[k] = e->dc[zigzag4[k]];
      write_block(e, s, 16, block_nc(e, mbx, mby, 0, 0, 0));
   }
   for (b = 0; b < 16; b++)
   {
      unsigned bx = blk_x[b], by = blk_y[b];
      const int16_t *lv = e->lv[by * 4 + bx];
      if (!(e->cbp_l & (1u << (b >> 2))))
         continue;
      if (i16)
      {
         for (k = 1; k < 16; k++)
            s[k - 1] = lv[zigzag4[k]];
         write_block(e, s, 15, block_nc(e, mbx, mby, bx, by, 0));
      }
      else
      {
         for (k = 0; k < 16; k++)
            s[k] = lv[zigzag4[k]];
         write_block(e, s, 16, block_nc(e, mbx, mby, bx, by, 0));
      }
   }
   if (e->cbp_c)
      for (c = 0; c < 2; c++)
         write_block(e, e->cdc[c], 4, -1);
   if (e->cbp_c == 2)
      for (c = 0; c < 2; c++)
         for (b = 0; b < 4; b++)
         {
            for (k = 1; k < 16; k++)
               s[k - 1] = e->cac[c][b][zigzag4[k]];
            write_block(e, s, 15, block_nc(e, mbx, mby, b & 1, b >> 1, c + 1));
         }
}

/* --------------------------------------------------------------------- */
/* Samples                                                               */
/* --------------------------------------------------------------------- */

static uint8_t *mb_ptr(const rh264_encoder_t *e, int s, int pl, unsigned mbx, unsigned mby)
{
   return pl == 0 ? e->pic[s][0] + (size_t)mby * 16 * e->stride + mbx * 16
                  : e->pic[s][pl] + (size_t)mby * 8 * e->c_stride + mbx * 8;
}

/* the macroblock's source, edge samples repeated past the frame */
static void fetch_src(rh264_encoder_t *e, unsigned mbx, unsigned mby,
      const uint8_t *y, size_t ys, const uint8_t *cb, size_t cbs,
      const uint8_t *cr, size_t crs)
{
   unsigned w = e->cfg.width, h = e->cfg.height, i, j;
   for (j = 0; j < 16; j++)
   {
      unsigned sy = mby * 16 + j < h ? mby * 16 + j : h - 1;
      const uint8_t *r = y + sy * ys;
      if (mbx * 16 + 16 <= w)
         memcpy(e->src + j * 16, r + mbx * 16, 16);
      else
         for (i = 0; i < 16; i++)
            e->src[j * 16 + i] = r[mbx * 16 + i < w ? mbx * 16 + i : w - 1];
   }
   for (j = 0; j < 8; j++)
   {
      unsigned sy = mby * 8 + j < h / 2 ? mby * 8 + j : h / 2 - 1;
      const uint8_t *r0 = cb + sy * cbs, *r1 = cr + sy * crs;
      for (i = 0; i < 8; i++)
      {
         unsigned sx = mbx * 8 + i < w / 2 ? mbx * 8 + i : w / 2 - 1;
         e->src[256 + j * 8 + i] = r0[sx];
         e->src[320 + j * 8 + i] = r1[sx];
      }
   }
}

static unsigned satd4(const uint8_t *a, unsigned as, const uint8_t *b, unsigned bs)
{
   int d[16], t[16], i;
   unsigned s = 0;
   for (i = 0; i < 4; i++)
   {
      int d0 = a[i * as] - b[i * bs], d1 = a[i * as + 1] - b[i * bs + 1];
      int d2 = a[i * as + 2] - b[i * bs + 2], d3 = a[i * as + 3] - b[i * bs + 3];
      int s0 = d0 + d1, s1 = d0 - d1, s2 = d2 + d3, s3 = d2 - d3;
      d[i * 4 + 0] = s0 + s2;
      d[i * 4 + 1] = s1 + s3;
      d[i * 4 + 2] = s0 - s2;
      d[i * 4 + 3] = s1 - s3;
   }
   for (i = 0; i < 4; i++)
   {
      int s0 = d[i] + d[4 + i], s1 = d[i] - d[4 + i];
      int s2 = d[8 + i] + d[12 + i], s3 = d[8 + i] - d[12 + i];
      t[i] = s0 + s2;
      t[4 + i] = s1 + s3;
      t[8 + i] = s0 - s2;
      t[12 + i] = s1 - s3;
   }
   for (i = 0; i < 16; i++)
      s += (unsigned)abs(t[i]);
   return s >> 1;
}

static unsigned satd16(const uint8_t *src, const uint8_t *pred)
{
   unsigned s = 0, b;
   for (b = 0; b < 16; b++)
      s += satd4(src + (b >> 2) * 64 + (b & 3) * 4, 16, pred + (b >> 2) * 64 + (b & 3) * 4, 16);
   return s;
}

/* --------------------------------------------------------------------- */
/* Intra prediction (8.3)                                                */
/* --------------------------------------------------------------------- */

/* 16x16 (luma, @n 16) or 8x8 (a chroma plane) prediction in mode @mode
 * (luma: V, H, DC, plane; chroma: DC, H, V, plane) into @dst; returns
 * false when the mode needs samples that are not there */
static bool pred_big(const rh264_encoder_t *e, int pl, unsigned mbx, unsigned mby,
      unsigned mode, uint8_t *dst)
{
   unsigned n  = pl ? 8 : 16, st = pl ? e->c_stride : e->stride, i, j;
   const uint8_t *r = mb_ptr(e, e->cur, pl, mbx, mby);
   bool top = mby > 0, left = mbx > 0;
   if (pl)
      mode = mode == 0 ? 2 : mode == 2 ? 0 : mode;   /* to the luma numbering */
   switch (mode)
   {
      case 0:
         if (!top)
            return false;
         for (j = 0; j < n; j++)
            memcpy(dst + j * n, r - st, n);
         return true;
      case 1:
         if (!left)
            return false;
         for (j = 0; j < n; j++)
            memset(dst + j * n, r[(int)(j * st) - 1], n);
         return true;
      case 2:
         if (!pl)
         {
            unsigned s = 0, v;
            if (top)
               for (i = 0; i < 16; i++)
                  s += r[(int)i - (int)st];
            if (left)
               for (i = 0; i < 16; i++)
                  s += r[(int)(i * st) - 1];
            v = top && left ? (s + 16) >> 5 : top || left ? (s + 8) >> 4 : 128;
            memset(dst, (int)v, 256);
            return true;
         }
         /* chroma: each 4x4 its own, 8.3.4.1 to 8.3.4.3 */
         {
            unsigned bx, by;
            for (by = 0; by < 2; by++)
               for (bx = 0; bx < 2; bx++)
               {
                  unsigned st4 = 0, sl4 = 0, v;
                  for (i = 0; i < 4; i++)
                  {
                     if (top)
                        st4 += r[(int)(bx * 4 + i) - (int)st];
                     if (left)
                        sl4 += r[(int)((by * 4 + i) * st) - 1];
                  }
                  if (bx == by)        /* (0, 0) and (4, 4) */
                     v = top && left ? (st4 + sl4 + 4) >> 3 : top ? (st4 + 2) >> 2
                       : left ? (sl4 + 2) >> 2 : 128;
                  else if (bx)         /* (4, 0) */
                     v = top ? (st4 + 2) >> 2 : left ? (sl4 + 2) >> 2 : 128;
                  else                 /* (0, 4) */
                     v = left ? (sl4 + 2) >> 2 : top ? (st4 + 2) >> 2 : 128;
                  for (j = 0; j < 4; j++)
                     memset(dst + (by * 4 + j) * 8 + bx * 4, (int)v, 4);
               }
            return true;
         }
      default:
         if (!top || !left)
            return false;
         {
            int h = 0, v = 0, a, b, c, half = (int)n / 2;
            const uint8_t *t = r - (int)st;
            for (i = 0; i < (unsigned)half; i++)
            {
               int xr = half + (int)i, xl = half - 2 - (int)i;
               h += (int)(i + 1) * (t[xr] - (xl < 0 ? t[-1] : t[xl]));
               v += (int)(i + 1) * (r[xr * (int)st - 1]
                     - (xl < 0 ? t[-1] : r[xl * (int)st - 1]));
            }
            a = 16 * (r[(int)((n - 1) * st) - 1] + t[n - 1]);
            if (pl)
            {
               b = (34 * h + 32) >> 6;
               c = (34 * v + 32) >> 6;
            }
            else
            {
               b = (5 * h + 32) >> 6;
               c = (5 * v + 32) >> 6;
            }
            for (j = 0; j < n; j++)
               for (i = 0; i < n; i++)
                  dst[j * n + i] = clip1((a + b * ((int)i - half + 1)
                        + c * ((int)j - half + 1) + 16) >> 5);
         }
         return true;
   }
}

/* The neighbours of 4x4 block (bx, by): p[-1..7] above (p[-1] the
 * corner), l[0..3] left; returns which are there: 1 left, 2 top, 4
 * corner (top right is always filled, from top if not there) */
static unsigned i4_edges(const rh264_encoder_t *e, unsigned mbx, unsigned mby,
      unsigned bx, unsigned by, uint8_t *top, uint8_t *l)
{
   unsigned st = e->stride, have = 0, i;
   const uint8_t *r = mb_ptr(e, e->cur, 0, mbx, mby) + by * 4 * st + bx * 4;
   bool left = bx || mbx, up = by || mby, tr;
   if (left)
   {
      have |= 1;
      for (i = 0; i < 4; i++)
         l[i] = r[(int)(i * st) - 1];
   }
   if (up)
   {
      have |= 2;
      for (i = 0; i < 4; i++)
         top[1 + i] = r[(int)i - (int)st];
      /* top right: above the macroblock, or above right of it, or an
       * earlier block of this one */
      if (by == 0)
         tr = bx < 3 ? true : mbx + 1 < e->mb_w;
      else if (bx == 3)
         tr = false;
      else
      {
         /* the block at (bx + 1, by - 1) comes first in the scan */
         static const uint8_t idx[4][4] = { { 0, 1, 4, 5 }, { 2, 3, 6, 7 },
                                            { 8, 9, 12, 13 }, { 10, 11, 14, 15 } };
         tr = idx[by - 1][bx + 1] < idx[by][bx];
      }
      for (i = 0; i < 4; i++)
         top[5 + i] = tr ? r[(int)(4 + i) - (int)st] : top[4];
   }
   if (left && up)
   {
      have |= 4;
      top[0] = r[-1 - (int)st];
   }
   return have;
}

/* 4x4 prediction, 8.3.1.2, into @d (stride 4); false if not possible */
static bool pred4(unsigned mode, unsigned have, const uint8_t *top, const uint8_t *l,
      uint8_t *d)
{
   const uint8_t *t = top + 1;             /* t[-1] the corner */
   int x, y;
   int L[5];                               /* L[0] the corner, L[1..4] left */
   L[0] = top[0];
   for (y = 0; y < 4; y++)
      L[y + 1] = l[y];
   switch (mode)
   {
      case 0:
         if (!(have & 2))
            return false;
         for (y = 0; y < 4; y++)
            memcpy(d + y * 4, t, 4);
         return true;
      case 1:
         if (!(have & 1))
            return false;
         for (y = 0; y < 4; y++)
            memset(d + y * 4, l[y], 4);
         return true;
      case 2:
      {
         unsigned s = 0, v;
         for (x = 0; x < 4; x++)
            s += ((have & 2) ? t[x] : 0) + ((have & 1) ? l[x] : 0);
         v = (have & 3) == 3 ? (s + 4) >> 3 : (have & 3) ? (s + 2) >> 2 : 128;
         memset(d, (int)v, 16);
         return true;
      }
      case 3:
         if (!(have & 2))
            return false;
         for (y = 0; y < 4; y++)
            for (x = 0; x < 4; x++)
               d[y * 4 + x] = (uint8_t)(x == 3 && y == 3 ? (t[6] + 3 * t[7] + 2) >> 2
                     : (t[x + y] + 2 * t[x + y + 1] + t[x + y + 2] + 2) >> 2);
         return true;
      case 7:
         if (!(have & 2))
            return false;
         for (y = 0; y < 4; y++)
            for (x = 0; x < 4; x++)
            {
               int i = x + (y >> 1);
               d[y * 4 + x] = (uint8_t)((y & 1) ? (t[i] + 2 * t[i + 1] + t[i + 2] + 2) >> 2
                     : (t[i] + t[i + 1] + 1) >> 1);
            }
         return true;
      case 8:
         if (!(have & 1))
            return false;
         for (y = 0; y < 4; y++)
            for (x = 0; x < 4; x++)
            {
               int z = x + 2 * y, i = y + (x >> 1);
               d[y * 4 + x] = (uint8_t)(z > 5 ? l[3] : z == 5 ? (l[2] + 3 * l[3] + 2) >> 2
                     : (z & 1) ? (l[i] + 2 * l[i + 1] + l[i + 2] + 2) >> 2
                     : (l[i] + l[i + 1] + 1) >> 1);
            }
         return true;
      default:
         break;
   }
   if ((have & 7) != 7)
      return false;
   /* modes 4 to 6, from the corner both ways: t[-1] and L[0] are the
    * corner, L[1..4] the left column */
   for (y = 0; y < 4; y++)
      for (x = 0; x < 4; x++)
      {
         int v;
         if (mode == 4)
         {
            if (x > y)
               v = (t[x - y - 2] + 2 * t[x - y - 1] + t[x - y] + 2) >> 2;
            else if (x < y)
               v = (L[y - x - 1] + 2 * L[y - x] + L[y - x + 1] + 2) >> 2;
            else
               v = (t[0] + 2 * L[0] + L[1] + 2) >> 2;
         }
         else if (mode == 5)
         {
            int z = 2 * x - y, i = x - (y >> 1);
            if (z >= 0 && !(z & 1))
               v = (t[i - 1] + t[i] + 1) >> 1;
            else if (z >= 0)
               v = (t[i - 2] + 2 * t[i - 1] + t[i] + 2) >> 2;
            else if (z == -1)
               v = (L[1] + 2 * L[0] + t[0] + 2) >> 2;
            else
               v = (L[y] + 2 * L[y - 1] + L[y - 2] + 2) >> 2;
         }
         else
         {
            int z = 2 * y - x, i = y - (x >> 1);
            if (z >= 0 && !(z & 1))
               v = (L[i] + L[i + 1] + 1) >> 1;
            else if (z >= 0)
               v = (L[i - 1] + 2 * L[i] + L[i + 1] + 2) >> 2;
            else if (z == -1)
               v = (L[1] + 2 * L[0] + t[0] + 2) >> 2;
            else
               v = (t[x - 1] + 2 * t[x - 2] + t[x - 3] + 2) >> 2;
         }
         d[y * 4 + x] = (uint8_t)v;
      }
   return true;
}

/* --------------------------------------------------------------------- */
/* Intra macroblocks                                                     */
/* --------------------------------------------------------------------- */

/* the predicted 4x4 mode of block (bx, by), 8.3.1.1 */
static int i4_pred_mode(const rh264_encoder_t *e, unsigned mbx, unsigned mby,
      unsigned bx, unsigned by)
{
   const mb_info_t *m = &e->mbs[mby * e->mb_w + mbx];
   int a, b;
   if ((!bx && !mbx) || (!by && !mby))
      return 2;
   a = bx ? m->i4[by * 4 + bx - 1] : (m - 1)->i4[by * 4 + 3];
   b = by ? m->i4[(by - 1) * 4 + bx] : (m - e->mb_w)->i4[12 + bx];
   return a < b ? a : b;
}

/* A chroma plane's residual against e->pred: levels into cdc and cac;
 * returns 0, 1 (DC only) or 2 */
static unsigned chroma_residual(rh264_encoder_t *e, int c, bool intra)
{
   const uint8_t *s = e->src + 256 + c * 64, *p = e->pred + 256 + c * 64;
   int      w[4][16], f[4];
   unsigned b, i, j, ac = 0, dc = 0;
   int      qp = e->qpc, qbits = 15 + qp / 6;
   for (b = 0; b < 4; b++)
   {
      int d[16];
      for (j = 0; j < 4; j++)
         for (i = 0; i < 4; i++)
         {
            unsigned o = ((b >> 1) * 4 + j) * 8 + (b & 1) * 4 + i;
            d[j * 4 + i] = s[o] - p[o];
         }
      fdct4(d, w[b]);
      ac += quant4(w[b], e->cac[c][b], qp, intra, 1) != 0;
   }
   /* the DC: 2x2 transform, 8.5.11 */
   f[0] = w[0][0] + w[1][0] + w[2][0] + w[3][0];
   f[1] = w[0][0] - w[1][0] + w[2][0] - w[3][0];
   f[2] = w[0][0] + w[1][0] - w[2][0] - w[3][0];
   f[3] = w[0][0] - w[1][0] - w[2][0] + w[3][0];
   for (b = 0; b < 4; b++)
   {
      e->cdc[c][b] = (int16_t)quant1(f[b], quant_mf[qp % 6][0], (unsigned)qbits + 1,
            (1 << (qbits + 1)) / (intra ? 3 : 6));
      dc |= e->cdc[c][b] != 0;
   }
   return ac ? 2 : dc;
}

/* The chroma reconstruction once the macroblock's cbp_c is settled */
static void chroma_recon(rh264_encoder_t *e, unsigned mbx, unsigned mby)
{
   int c;
   for (c = 0; c < 2; c++)
   {
      const uint8_t *p = e->pred + 256 + c * 64;
      uint8_t *d = mb_ptr(e, e->cur, 1 + c, mbx, mby);
      unsigned b, j, st = e->c_stride;
      int qp = e->qpc, ls = 16 * dequant_v[qp % 6][0], dcs[4], ff[4];
      int c0 = e->cdc[c][0], c1 = e->cdc[c][1], c2 = e->cdc[c][2], c3 = e->cdc[c][3];
      for (j = 0; j < 8; j++)
         memcpy(d + j * st, p + j * 8, 8);
      if (!e->cbp_c)
         continue;
      ff[0] = c0 + c1 + c2 + c3;
      ff[1] = c0 - c1 + c2 - c3;
      ff[2] = c0 + c1 - c2 - c3;
      ff[3] = c0 - c1 - c2 + c3;
      for (b = 0; b < 4; b++)
      {
         int cf[16];
         dcs[b] = (ff[b] * ls * (1 << (qp / 6))) >> 5;
         if (e->cbp_c == 2)
            dequant4(e->cac[c][b], cf, qp, &dcs[b]);
         else
         {
            memset(cf, 0, sizeof(cf));
            cf[0] = dcs[b];
         }
         idct4_add(cf, d + (b >> 1) * 4 * st + (b & 1) * 4, st);
      }
   }
}

/* Chroma: the cheapest intra mode, its residual and reconstruction */
static void code_chroma_intra(rh264_encoder_t *e, unsigned mbx, unsigned mby)
{
   unsigned m, best = 0xFFFFFFFFu;
   uint8_t  tmp[128];
   e->c_mode = 0;
   for (m = 0; m < 4; m++)
   {
      unsigned s = 0, b;
      if (!pred_big(e, 1, mbx, mby, m, tmp) || !pred_big(e, 2, mbx, mby, m, tmp + 64))
         continue;
      for (b = 0; b < 8; b++)
         s += satd4(e->src + 256 + (b >> 2) * 64 + ((b >> 1) & 1) * 32 + (b & 1) * 4, 8,
               tmp + (b >> 2) * 64 + ((b >> 1) & 1) * 32 + (b & 1) * 4, 8);
      if (s < best)
      {
         best = s;
         e->c_mode = m;
         memcpy(e->pred + 256, tmp, 128);
      }
   }
}

static void chroma_finish(rh264_encoder_t *e, unsigned mbx, unsigned mby, bool intra)
{
   unsigned a = chroma_residual(e, 0, intra);
   unsigned b = chroma_residual(e, 1, intra);
   e->cbp_c = a > b ? a : b;
   chroma_recon(e, mbx, mby);
}

/* Intra 16x16 luma in mode @mode (prediction already in e->pred):
 * levels and reconstruction */
static void luma_i16(rh264_encoder_t *e, unsigned mbx, unsigned mby)
{
   int      (*w)[16] = e->w, ydc[16], f[16], t[16];
   unsigned b, i, j, ac = 0, st = e->stride;
   int      qp = e->qp, qbits = 15 + qp / 6;
   uint8_t *dst = mb_ptr(e, e->cur, 0, mbx, mby);

   for (b = 0; b < 16; b++)                /* raster blocks */
   {
      int d[16];
      unsigned bx = b & 3, by = b >> 2;
      for (j = 0; j < 4; j++)
         for (i = 0; i < 4; i++)
         {
            unsigned o = (by * 4 + j) * 16 + bx * 4 + i;
            d[j * 4 + i] = e->src[o] - e->pred[o];
         }
      fdct4(d, w[b]);
      ac += quant4(w[b], e->lv[b], qp, true, 1);
      ydc[b] = w[b][0];
   }
   /* the DC: 4x4 Hadamard, halved */
   for (i = 0; i < 4; i++)
   {
      int s0 = ydc[i * 4] + ydc[i * 4 + 3], s3 = ydc[i * 4] - ydc[i * 4 + 3];
      int s1 = ydc[i * 4 + 1] + ydc[i * 4 + 2], s2 = ydc[i * 4 + 1] - ydc[i * 4 + 2];
      t[i * 4 + 0] = s0 + s1;
      t[i * 4 + 1] = s3 + s2;
      t[i * 4 + 2] = s0 - s1;
      t[i * 4 + 3] = s3 - s2;
   }
   for (i = 0; i < 4; i++)
   {
      int s0 = t[i] + t[12 + i], s3 = t[i] - t[12 + i];
      int s1 = t[4 + i] + t[8 + i], s2 = t[4 + i] - t[8 + i];
      f[i]      = (s0 + s1) >> 1;
      f[4 + i]  = (s3 + s2) >> 1;
      f[8 + i]  = (s0 - s1) >> 1;
      f[12 + i] = (s3 - s2) >> 1;
   }
   for (b = 0; b < 16; b++)
      e->dc[b] = (int16_t)quant1(f[b], quant_mf[qp % 6][0], (unsigned)qbits + 1,
            (1 << (qbits + 1)) / 3);
   e->cbp_l = ac ? 15 : 0;

   /* the decoder's DC, 8.5.10 */
   for (i = 0; i < 4; i++)
   {
      int c0 = e->dc[i * 4], c1 = e->dc[i * 4 + 1], c2 = e->dc[i * 4 + 2], c3 = e->dc[i * 4 + 3];
      t[i * 4 + 0] = c0 + c1 + c2 + c3;
      t[i * 4 + 1] = c0 + c1 - c2 - c3;
      t[i * 4 + 2] = c0 - c1 - c2 + c3;
      t[i * 4 + 3] = c0 - c1 + c2 - c3;
   }
   for (i = 0; i < 4; i++)
   {
      int c0 = t[i], c1 = t[4 + i], c2 = t[8 + i], c3 = t[12 + i];
      int ls = 16 * dequant_v[qp % 6][0];
      int ff[4], k;
      ff[0] = c0 + c1 + c2 + c3;
      ff[1] = c0 + c1 - c2 - c3;
      ff[2] = c0 - c1 - c2 + c3;
      ff[3] = c0 - c1 + c2 - c3;
      for (k = 0; k < 4; k++)
         ydc[k * 4 + i] = qp >= 36 ? ff[k] * ls * (1 << (qp / 6 - 6))
                                   : (ff[k] * ls + (1 << (5 - qp / 6))) >> (6 - qp / 6);
   }
   for (j = 0; j < 16; j++)
      memcpy(dst + j * st, e->pred + j * 16, 16);
   for (b = 0; b < 16; b++)
   {
      int cf[16];
      if (!e->cbp_l)
         memset(e->lv[b], 0, sizeof(e->lv[b]));
      dequant4(e->lv[b], cf, qp, &ydc[b]);
      idct4_add(cf, dst + (b >> 2) * 4 * st + (b & 3) * 4, st);
   }
}

/* Intra 4x4 luma: each block's cheapest mode, coded and reconstructed
 * in turn; returns the cost */
static unsigned luma_i4(rh264_encoder_t *e, unsigned mbx, unsigned mby)
{
   mb_info_t *m    = &e->mbs[mby * e->mb_w + mbx];
   unsigned   cost = 0, b, st = e->stride;
   int        qp   = e->qp;
   e->cbp_l = 0;
   for (b = 0; b < 16; b++)
   {
      unsigned bx = blk_x[b], by = blk_y[b], mode, best = 0xFFFFFFFFu, bm = 2, have, i, j;
      uint8_t  top[9], l[4], p[16], bp[16];
      int      pm = i4_pred_mode(e, mbx, mby, bx, by), d[16], w[16], cf[16];
      const uint8_t *s = e->src + by * 64 + bx * 4;
      uint8_t *dst = mb_ptr(e, e->cur, 0, mbx, mby) + by * 4 * st + bx * 4;
      int16_t *lv  = e->lv[by * 4 + bx];
      memset(top, 128, sizeof(top));
      memset(l, 128, sizeof(l));
      have = i4_edges(e, mbx, mby, bx, by, top, l);
      for (mode = 0; mode < 9; mode++)
      {
         unsigned c;
         if (!pred4(mode, have, top, l, p))
            continue;
         c = satd4(s, 16, p, 4) + e->lambda * ((int)mode == pm ? 1 : 4);
         if (c < best)
         {
            best = c;
            bm   = mode;
            memcpy(bp, p, 16);
         }
      }
      cost += best;
      e->i4[b] = (int8_t)bm;
      m->i4[by * 4 + bx] = (int8_t)bm;
      for (j = 0; j < 4; j++)
         for (i = 0; i < 4; i++)
            d[j * 4 + i] = s[j * 16 + i] - bp[j * 4 + i];
      fdct4(d, w);
      if (quant4(w, lv, qp, true, 0))
         e->cbp_l |= 1u << (b >> 2);
      for (j = 0; j < 4; j++)
         memcpy(dst + j * st, bp + j * 4, 4);
      dequant4(lv, cf, qp, NULL);
      idct4_add(cf, dst, st);
   }
   return cost;
}

static void mb_finish_tc(rh264_encoder_t *e, mb_info_t *m, bool i16)
{
   unsigned b, c;
   for (b = 0; b < 16; b++)
   {
      unsigned bx = b & 3, by = b >> 2, b8 = (by >> 1) * 2 + (bx >> 1);
      m->tc[b] = (uint8_t)((e->cbp_l >> b8) & 1 ? count_nz(e->lv[b], i16 ? 1 : 0) : 0);
   }
   for (c = 0; c < 2; c++)
      for (b = 0; b < 4; b++)
         m->tc[16 + c * 4 + b] = (uint8_t)(e->cbp_c == 2 ? count_nz(e->cac[c][b], 1) : 0);
}

/* An intra macroblock: modes chosen, coded, reconstructed and written */
static void code_intra(rh264_encoder_t *e, unsigned mbx, unsigned mby, bool in_p)
{
   mb_info_t *m = &e->mbs[mby * e->mb_w + mbx];
   unsigned   mode, best16 = 0xFFFFFFFFu, cost4, b, cbp;
   bool       i16;
   uint8_t   *tmp = e->trial;

   m->qp    = (uint8_t)e->qp;
   m->mv[0] = m->mv[1] = 0;
   for (b = 0; b < 16; b++)
      m->i4[b] = 2;
   e->i16_mode = 2;
   for (mode = 0; mode < 4; mode++)
   {
      unsigned s;
      if (!pred_big(e, 0, mbx, mby, mode, tmp))
         continue;
      s = satd16(e->src, tmp);
      if (s < best16)
      {
         best16 = s;
         e->i16_mode = mode;
         memcpy(e->pred, tmp, 256);
      }
   }
   /* 4x4 blocks cost the modes' bits, about 24 more than 16x16's */
   cost4 = luma_i4(e, mbx, mby) + 6 * e->lambda;
   i16   = best16 + 16 * e->lambda < cost4;
   if (i16)
   {
      for (b = 0; b < 16; b++)
         m->i4[b] = 2;
      luma_i16(e, mbx, mby);
   }
   code_chroma_intra(e, mbx, mby);
   chroma_finish(e, mbx, mby, true);
   m->type = i16 ? MB_I16 : MB_I4;
   mb_finish_tc(e, m, i16);

   /* mb_layer */
   if (in_p)
   {
      put_ue(e, e->skip_run);
      e->skip_run = 0;
   }
   cbp = e->cbp_l | (e->cbp_c << 4);
   if (i16)
   {
      put_ue(e, (in_p ? 5 : 0) + 1 + e->i16_mode + 4 * e->cbp_c + (e->cbp_l ? 12 : 0));
      put_ue(e, e->c_mode);
      put_se(e, 0);                        /* mb_qp_delta */
   }
   else
   {
      put_ue(e, in_p ? 5 : 0);
      for (b = 0; b < 16; b++)
      {
         int pm = i4_pred_mode(e, mbx, mby, blk_x[b], blk_y[b]);
         /* the prediction from blocks of this macroblock uses their
          * final modes, which m->i4 already holds */
         if (e->i4[b] == pm)
            put_bits(e, 1, 1);
         else
            put_bits(e, (unsigned)(e->i4[b] < pm ? e->i4[b] : e->i4[b] - 1), 4);
      }
      put_ue(e, e->c_mode);
      put_ue(e, cbp_code_intra[cbp]);
      if (cbp)
         put_se(e, 0);
   }
   write_residual(e, mbx, mby, i16);
}

/* --------------------------------------------------------------------- */
/* Inter prediction (8.4.2.2)                                            */
/* --------------------------------------------------------------------- */

/* The reference's half sample planes: b and h, then j from b's
 * unclipped values (8.4.2.2.1) */
static void make_half(rh264_encoder_t *e)
{
   const uint8_t *g = e->pic[!e->cur][0];
   int  st = (int)e->stride, x, y;
   int  x0 = -PAD + 3, n = (int)e->w16 + 2 * PAD - 6;
   int  y0 = -PAD + 3, y1 = (int)e->h16 + PAD - 3;
   int16_t *tmp = e->tmp + (size_t)PAD * e->stride + PAD;
   /* rows, so each loop is one straight run the compiler can vectorise */
   for (y = -PAD; y < (int)e->h16 + PAD; y++)
   {
      const uint8_t *r = g + y * st + x0;
      int16_t       *t = tmp + y * st + x0;
      uint8_t       *b = e->half[0] + y * st + x0;
      for (x = 0; x < n; x++)
      {
         int v = r[x - 2] - 5 * r[x - 1] + 20 * r[x] + 20 * r[x + 1] - 5 * r[x + 2] + r[x + 3];
         t[x] = (int16_t)v;
         v    = (v + 16) >> 5;
         b[x] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
      }
   }
   for (y = y0; y < y1; y++)
   {
      const uint8_t *c0 = g + (y - 2) * st + x0, *c1 = c0 + st, *c2 = c1 + st;
      const uint8_t *c3 = c2 + st, *c4 = c3 + st, *c5 = c4 + st;
      uint8_t       *h  = e->half[1] + y * st + x0;
      for (x = 0; x < n; x++)
      {
         int v = c0[x] - 5 * c1[x] + 20 * c2[x] + 20 * c3[x] - 5 * c4[x] + c5[x];
         v    = (v + 16) >> 5;
         h[x] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
      }
   }
   for (y = y0; y < y1; y++)
   {
      const int16_t *t0 = tmp + (y - 2) * st + x0, *t1 = t0 + st, *t2 = t1 + st;
      const int16_t *t3 = t2 + st, *t4 = t3 + st, *t5 = t4 + st;
      uint8_t       *j  = e->half[2] + y * st + x0;
      for (x = 0; x < n; x++)
      {
         int v = t0[x] - 5 * t1[x] + 20 * t2[x] + 20 * t3[x] - 5 * t4[x] + t5[x];
         v    = (v + 512) >> 10;
         j[x] = (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v);
      }
   }
}

/* where each quarter sample position comes from: one or two of G, b, h,
 * j (0..3), each offset by (dx, dy) */
static const int8_t qpel_src[16][6] = {
   { 0, 0, 0, -1, 0, 0 }, { 0, 0, 0, 1, 0, 0 }, { 1, 0, 0, -1, 0, 0 }, { 0, 1, 0, 1, 0, 0 },
   { 0, 0, 0, 2, 0, 0 },  { 1, 0, 0, 2, 0, 0 }, { 1, 0, 0, 3, 0, 0 },  { 1, 0, 0, 2, 1, 0 },
   { 2, 0, 0, -1, 0, 0 }, { 2, 0, 0, 3, 0, 0 }, { 3, 0, 0, -1, 0, 0 }, { 3, 0, 0, 2, 1, 0 },
   { 0, 0, 1, 2, 0, 0 },  { 2, 0, 0, 1, 0, 1 }, { 3, 0, 0, 1, 0, 1 },  { 2, 1, 0, 1, 0, 1 } };

static const uint8_t *qplane(const rh264_encoder_t *e, int k)
{
   return k ? e->half[k - 1] : e->pic[!e->cur][0];
}

/* the 16x16 luma prediction at quarter sample vector (mx, my) */
static void pred_luma(const rh264_encoder_t *e, unsigned mbx, unsigned mby, int mx, int my,
      uint8_t *dst)
{
   int x = (int)mbx * 16 + (mx >> 2), y = (int)mby * 16 + (my >> 2);
   const int8_t *q = qpel_src[(my & 3) * 4 + (mx & 3)];
   int st = (int)e->stride, i, j;
   const uint8_t *a = qplane(e, q[0]) + (y + q[2]) * st + x + q[1];
   if (q[3] < 0)
   {
      for (j = 0; j < 16; j++)
         memcpy(dst + j * 16, a + j * st, 16);
      return;
   }
   {
      const uint8_t *b = qplane(e, q[3]) + (y + q[5]) * st + x + q[4];
      for (j = 0; j < 16; j++)
         for (i = 0; i < 16; i++)
            dst[j * 16 + i] = (uint8_t)((a[j * st + i] + b[j * st + i] + 1) >> 1);
   }
}

/* the chroma prediction, eighth samples (8.4.2.2.2) */
static void pred_chroma(const rh264_encoder_t *e, unsigned mbx, unsigned mby, int mx, int my,
      uint8_t *dst)
{
   int c, i, j, fx = mx & 7, fy = my & 7, st = (int)e->c_stride;
   int x = (int)mbx * 8 + (mx >> 3), y = (int)mby * 8 + (my >> 3);
   for (c = 0; c < 2; c++)
   {
      const uint8_t *r = e->pic[!e->cur][1 + c] + y * st + x;
      for (j = 0; j < 8; j++)
         for (i = 0; i < 8; i++)
         {
            const uint8_t *s = r + j * st + i;
            dst[c * 64 + j * 8 + i] = (uint8_t)(((8 - fx) * (8 - fy) * s[0] + fx * (8 - fy) * s[1]
                  + (8 - fx) * fy * s[st] + fx * fy * s[st + 1] + 32) >> 6);
         }
   }
}

/* a vector the borders hold, inside the limit */
static bool mv_ok(const rh264_encoder_t *e, unsigned mbx, unsigned mby, int mx, int my)
{
   int x = (int)mbx * 16 + (mx >> 2), y = (int)mby * 16 + (my >> 2);
   return mx >= -MV_LIMIT && mx <= MV_LIMIT && my >= -MV_LIMIT && my <= MV_LIMIT
       && x >= -PAD + 4 && y >= -PAD + 4
       && x + 18 <= (int)e->w16 + PAD - 4 && y + 18 <= (int)e->h16 + PAD - 4;
}

static unsigned sad16(const uint8_t *a, const uint8_t *b)
{
   unsigned i, s = 0;
   for (i = 0; i < 256; i++)
      s += (unsigned)abs(a[i] - b[i]);
   return s;
}

static int median3(int a, int b, int c)
{
   if (a > b) { int t = a; a = b; b = t; }
   return c < a ? a : c > b ? b : c;
}

/* neighbour (dx, dy)'s vector: false when outside the picture; ref -1
 * for intra (8.4.1.3.2) */
static bool nb(const rh264_encoder_t *e, unsigned mbx, unsigned mby, int dx, int dy,
      int *ref, int *mv)
{
   int x = (int)mbx + dx, y = (int)mby + dy;
   const mb_info_t *m;
   *ref = -1;
   mv[0] = mv[1] = 0;
   if (x < 0 || y < 0 || x >= (int)e->mb_w)
      return false;
   m = &e->mbs[y * (int)e->mb_w + x];
   if (m->type == MB_I16 || m->type == MB_I4)
      return true;
   *ref = 0;
   mv[0] = m->mv[0];
   mv[1] = m->mv[1];
   return true;
}

/* the 16x16 vector predictor, 8.4.1.3, and P_Skip's, 8.4.1.1 */
static void mv_pred(const rh264_encoder_t *e, unsigned mbx, unsigned mby, int *p, int *skip)
{
   int ra, rb, rc, a[2], b[2], c[2], k;
   bool ha = nb(e, mbx, mby, -1, 0, &ra, a);
   bool hb = nb(e, mbx, mby, 0, -1, &rb, b);
   bool hc = nb(e, mbx, mby, 1, -1, &rc, c);
   bool hb0 = hb;
   if (!hc)
      hc = nb(e, mbx, mby, -1, -1, &rc, c);
   if (!hb && !hc && ha)
   {
      rb = rc = ra;
      b[0] = c[0] = a[0];
      b[1] = c[1] = a[1];
   }
   k = (ra == 0) + (rb == 0) + (rc == 0);
   if (k == 1)
   {
      const int *s = ra == 0 ? a : rb == 0 ? b : c;
      p[0] = s[0];
      p[1] = s[1];
   }
   else
   {
      p[0] = median3(a[0], b[0], c[0]);
      p[1] = median3(a[1], b[1], c[1]);
   }
   if (!ha || !hb0)
      skip[0] = skip[1] = 0;
   else
   {
      int r2, a2[2], b2[2];
      nb(e, mbx, mby, -1, 0, &r2, a2);
      if (r2 == 0 && !a2[0] && !a2[1])
         skip[0] = skip[1] = 0;
      else
      {
         nb(e, mbx, mby, 0, -1, &r2, b2);
         if (r2 == 0 && !b2[0] && !b2[1])
            skip[0] = skip[1] = 0;
         else
         {
            skip[0] = p[0];
            skip[1] = p[1];
         }
      }
   }
}

/* Inter luma residual against e->pred: levels, cbp_l */
static void luma_inter(rh264_encoder_t *e)
{
   unsigned b, i, j;
   e->cbp_l = 0;
   for (b = 0; b < 16; b++)
   {
      int d[16], w[16];
      unsigned bx = b & 3, by = b >> 2;
      for (j = 0; j < 4; j++)
         for (i = 0; i < 4; i++)
         {
            unsigned o = (by * 4 + j) * 16 + bx * 4 + i;
            d[j * 4 + i] = e->src[o] - e->pred[o];
         }
      fdct4(d, w);
      if (quant4(w, e->lv[b], e->qp, false, 0))
         e->cbp_l |= 1u << ((by >> 1) * 2 + (bx >> 1));
   }
   /* an 8x8 with nothing but a lone small coefficient or two costs more
    * than it brings: dropped */
   for (b = 0; b < 4; b++)
   {
      unsigned k, n = 0, big = 0;
      if (!(e->cbp_l & (1u << b)))
         continue;
      for (k = 0; k < 4; k++)
      {
         const int16_t *lv = e->lv[((b >> 1) * 2 + (k >> 1)) * 4 + (b & 1) * 2 + (k & 1)];
         for (i = 0; i < 16; i++)
            if (lv[i])
            {
               n++;
               big |= lv[i] > 1 || lv[i] < -1;
            }
      }
      if (!big && n <= 2)
      {
         e->cbp_l &= ~(1u << b);
         for (k = 0; k < 4; k++)
            memset(e->lv[((b >> 1) * 2 + (k >> 1)) * 4 + (b & 1) * 2 + (k & 1)], 0,
                  sizeof(e->lv[0]));
      }
   }
}

static void luma_inter_recon(rh264_encoder_t *e, unsigned mbx, unsigned mby)
{
   uint8_t *dst = mb_ptr(e, e->cur, 0, mbx, mby);
   unsigned b, j, st = e->stride;
   for (j = 0; j < 16; j++)
      memcpy(dst + j * st, e->pred + j * 16, 16);
   for (b = 0; b < 16; b++)
   {
      int cf[16];
      if (!(e->cbp_l & (1u << (((b >> 2) >> 1) * 2 + ((b & 3) >> 1)))))
         continue;
      dequant4(e->lv[b], cf, e->qp, NULL);
      idct4_add(cf, dst + (b >> 2) * 4 * st + (b & 3) * 4, st);
   }
}

/* A P picture's macroblock */
static void code_p(rh264_encoder_t *e, unsigned mbx, unsigned mby)
{
   static const int8_t dia[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
   static const int8_t ring[8][2] = { { -1, -1 }, { 0, -1 }, { 1, -1 }, { -1, 0 },
                                      { 1, 0 }, { -1, 1 }, { 0, 1 }, { 1, 1 } };
   mb_info_t *m = &e->mbs[mby * e->mb_w + mbx];
   int        p[2], sk[2], bx = 0, by = 0, k, n, cand[6][2], step;
   unsigned   best = 0xFFFFFFFFu, cbp, intra_cost = 0xFFFFFFFFu, b;
   uint8_t   *tmp = e->trial;

   m->qp = (uint8_t)e->qp;
   for (b = 0; b < 16; b++)
      m->i4[b] = 2;
   mv_pred(e, mbx, mby, p, sk);

   /* P_Skip when its prediction leaves nothing to code */
   if (mv_ok(e, mbx, mby, sk[0], sk[1]))
   {
      pred_luma(e, mbx, mby, sk[0], sk[1], e->pred);
      pred_chroma(e, mbx, mby, sk[0], sk[1], e->pred + 256);
      luma_inter(e);
      if (!e->cbp_l)
      {
         unsigned a = chroma_residual(e, 0, false);
         unsigned c = chroma_residual(e, 1, false);
         if (!a && !c)
         {
            e->cbp_c = 0;
            luma_inter_recon(e, mbx, mby);
            chroma_recon(e, mbx, mby);
            m->type  = MB_SKIP;
            m->mv[0] = (int16_t)sk[0];
            m->mv[1] = (int16_t)sk[1];
            memset(m->tc, 0, sizeof(m->tc));
            e->skip_run++;
            return;
         }
      }
   }

   /* the search: candidates, a diamond in whole samples, then half and
    * quarter samples around the best */
   cand[0][0] = p[0];  cand[0][1] = p[1];
   cand[1][0] = 0;     cand[1][1] = 0;
   cand[2][0] = sk[0]; cand[2][1] = sk[1];
   cand[3][0] = mbx ? (m - 1)->mv[0] : 0;
   cand[3][1] = mbx ? (m - 1)->mv[1] : 0;
   cand[4][0] = mby ? (m - e->mb_w)->mv[0] : 0;
   cand[4][1] = mby ? (m - e->mb_w)->mv[1] : 0;
   cand[5][0] = mby && mbx + 1 < e->mb_w ? (m - e->mb_w + 1)->mv[0] : 0;
   cand[5][1] = mby && mbx + 1 < e->mb_w ? (m - e->mb_w + 1)->mv[1] : 0;
   for (k = 0; k < 6; k++)
   {
      int mx = cand[k][0] & ~3, my = cand[k][1] & ~3;
      unsigned s;
      if (!mv_ok(e, mbx, mby, mx, my))
         continue;
      pred_luma(e, mbx, mby, mx, my, tmp);
      s = sad16(e->src, tmp) + e->lambda * (ue_bits(se_code(mx - p[0])) + ue_bits(se_code(my - p[1])));
      if (s < best)
      {
         best = s;
         bx = mx;
         by = my;
      }
   }
   for (n = 0; n < 32; n++)
   {
      int nx = bx, ny = by;
      for (k = 0; k < 4; k++)
      {
         int mx = bx + 4 * dia[k][0], my = by + 4 * dia[k][1];
         unsigned s;
         if (!mv_ok(e, mbx, mby, mx, my))
            continue;
         pred_luma(e, mbx, mby, mx, my, tmp);
         s = sad16(e->src, tmp) + e->lambda * (ue_bits(se_code(mx - p[0])) + ue_bits(se_code(my - p[1])));
         if (s < best)
         {
            best = s;
            nx = mx;
            ny = my;
         }
      }
      if (nx == bx && ny == by)
         break;
      bx = nx;
      by = ny;
   }
   for (step = 2; step >= 1; step--)
   {
      int cx = bx, cy = by;
      for (k = 0; k < 8; k++)
      {
         int mx = cx + step * ring[k][0], my = cy + step * ring[k][1];
         unsigned s;
         if (!mv_ok(e, mbx, mby, mx, my))
            continue;
         pred_luma(e, mbx, mby, mx, my, tmp);
         s = sad16(e->src, tmp) + e->lambda * (ue_bits(se_code(mx - p[0])) + ue_bits(se_code(my - p[1])));
         if (s < best)
         {
            best = s;
            bx = mx;
            by = my;
         }
      }
   }

   /* intra if 16x16 prediction is clearly the cheaper */
   for (k = 0; k < 4; k++)
   {
      unsigned s;
      if (!pred_big(e, 0, mbx, mby, (unsigned)k, tmp))
         continue;
      s = sad16(e->src, tmp);
      if (s < intra_cost)
         intra_cost = s;
   }
   if (best == 0xFFFFFFFFu || intra_cost + 8 * e->lambda < best)
   {
      code_intra(e, mbx, mby, true);
      return;
   }

   pred_luma(e, mbx, mby, bx, by, e->pred);
   pred_chroma(e, mbx, mby, bx, by, e->pred + 256);
   luma_inter(e);
   chroma_finish(e, mbx, mby, false);
   luma_inter_recon(e, mbx, mby);
   m->mv[0] = (int16_t)bx;
   m->mv[1] = (int16_t)by;
   mb_finish_tc(e, m, false);
   cbp = e->cbp_l | (e->cbp_c << 4);
   if (!cbp && bx == sk[0] && by == sk[1] && mv_ok(e, mbx, mby, sk[0], sk[1]))
   {
      m->type = MB_SKIP;
      e->skip_run++;
      return;
   }
   m->type = MB_P16;
   put_ue(e, e->skip_run);
   e->skip_run = 0;
   put_ue(e, 0);                            /* P_L0_16x16 */
   put_se(e, bx - p[0]);
   put_se(e, by - p[1]);
   put_ue(e, cbp_code_inter[cbp]);
   if (cbp)
   {
      put_se(e, 0);
      write_residual(e, mbx, mby, false);
   }
}

/* --------------------------------------------------------------------- */
/* Deblocking (8.7)                                                      */
/* --------------------------------------------------------------------- */

static bool mb_intra(const mb_info_t *m)
{
   return m->type == MB_I16 || m->type == MB_I4;
}

/* bS of the edge between 4x4 luma blocks @bp of @p and @bq of @q */
static int edge_bs(const mb_info_t *p, unsigned bp, const mb_info_t *q, unsigned bq,
      bool mb_edge)
{
   if (mb_intra(p) || mb_intra(q))
      return mb_edge ? 4 : 3;
   if (p->tc[bp] || q->tc[bq])
      return 2;
   if (abs(p->mv[0] - q->mv[0]) >= 4 || abs(p->mv[1] - q->mv[1]) >= 4)
      return 1;
   return 0;
}

/* Filters @lines lines across an edge: @s points at q0, @step goes
 * across the edge, @along from line to line */
static void filter_lines(uint8_t *s, int step, int along, int lines, int bs,
      int qp, bool chroma)
{
   int alpha = alpha_tab[qp], beta = beta_tab[qp], l;
   int tc0   = bs < 4 ? tc0_tab[qp][bs - 1] : 0;
   for (l = 0; l < lines; l++, s += along)
   {
      int p0 = s[-step], p1 = s[-2 * step], q0 = s[0], q1 = s[step];
      if (abs(p0 - q0) >= alpha || abs(p1 - p0) >= beta || abs(q1 - q0) >= beta)
         continue;
      if (chroma)
      {
         if (bs < 4)
         {
            int tc = tc0 + 1;
            int d  = clip3(-tc, tc, ((q0 - p0) * 4 + (p1 - q1) + 4) >> 3);
            s[-step] = clip1(p0 + d);
            s[0]     = clip1(q0 - d);
         }
         else
         {
            s[-step] = (uint8_t)((2 * p1 + p0 + q1 + 2) >> 2);
            s[0]     = (uint8_t)((2 * q1 + q0 + p1 + 2) >> 2);
         }
         continue;
      }
      {
         int p2 = s[-3 * step], q2 = s[2 * step];
         int ap = abs(p2 - p0) < beta, aq = abs(q2 - q0) < beta;
         if (bs < 4)
         {
            int tc = tc0 + ap + aq;
            int d  = clip3(-tc, tc, ((q0 - p0) * 4 + (p1 - q1) + 4) >> 3);
            s[-step] = clip1(p0 + d);
            s[0]     = clip1(q0 - d);
            if (ap)
               s[-2 * step] = (uint8_t)(p1 + clip3(-tc0, tc0, (p2 + ((p0 + q0 + 1) >> 1) - (p1 << 1)) >> 1));
            if (aq)
               s[step] = (uint8_t)(q1 + clip3(-tc0, tc0, (q2 + ((p0 + q0 + 1) >> 1) - (q1 << 1)) >> 1));
         }
         else
         {
            int p3 = s[-4 * step], q3 = s[3 * step];
            bool strong = abs(p0 - q0) < ((alpha >> 2) + 2);
            if (ap && strong)
            {
               s[-step]     = (uint8_t)((p2 + 2 * p1 + 2 * p0 + 2 * q0 + q1 + 4) >> 3);
               s[-2 * step] = (uint8_t)((p2 + p1 + p0 + q0 + 2) >> 2);
               s[-3 * step] = (uint8_t)((2 * p3 + 3 * p2 + p1 + p0 + q0 + 4) >> 3);
            }
            else
               s[-step] = (uint8_t)((2 * p1 + p0 + q1 + 2) >> 2);
            if (aq && strong)
            {
               s[0]        = (uint8_t)((p1 + 2 * p0 + 2 * q0 + 2 * q1 + q2 + 4) >> 3);
               s[step]     = (uint8_t)((p0 + q0 + q1 + q2 + 2) >> 2);
               s[2 * step] = (uint8_t)((2 * q3 + 3 * q2 + q1 + q0 + p0 + 4) >> 3);
            }
            else
               s[0] = (uint8_t)((2 * q1 + q0 + p1 + 2) >> 2);
         }
      }
   }
}

static void deblock(rh264_encoder_t *e)
{
   unsigned mbx, mby;
   int      ys = (int)e->stride, cs = (int)e->c_stride;
   for (mby = 0; mby < e->mb_h; mby++)
      for (mbx = 0; mbx < e->mb_w; mbx++)
      {
         const mb_info_t *q = &e->mbs[mby * e->mb_w + mbx];
         int dir;
         for (dir = 0; dir < 2; dir++)        /* vertical edges, then horizontal */
         {
            int edge, bs[4][4], k;
            for (edge = 0; edge < 4; edge++)
            {
               const mb_info_t *p = q;
               if (edge == 0)
               {
                  if (dir == 0 ? !mbx : !mby)
                  {
                     for (k = 0; k < 4; k++)
                        bs[edge][k] = 0;
                     continue;
                  }
                  p = dir == 0 ? q - 1 : q - e->mb_w;
               }
               for (k = 0; k < 4; k++)
               {
                  unsigned bq = dir == 0 ? (unsigned)k * 4 + edge : (unsigned)edge * 4 + k;
                  unsigned bp = dir == 0 ? (edge ? bq - 1 : (unsigned)k * 4 + 3)
                                         : (edge ? bq - 4 : 12u + k);
                  bs[edge][k] = edge_bs(p, bp, q, bq, edge == 0);
               }
            }
            for (edge = 0; edge < 4; edge++)
            {
               const mb_info_t *p = edge ? q : dir == 0 ? (mbx ? q - 1 : q) : (mby ? q - e->mb_w : q);
               int qpl = (p->qp + q->qp + 1) >> 1;
               int qpc = (chroma_qp(p->qp) + chroma_qp(q->qp) + 1) >> 1;
               for (k = 0; k < 4; k++)
               {
                  uint8_t *s;
                  if (!bs[edge][k])
                     continue;
                  s = mb_ptr(e, e->cur, 0, mbx, mby);
                  if (dir == 0)
                     filter_lines(s + k * 4 * ys + edge * 4, 1, ys, 4, bs[edge][k], qpl, false);
                  else
                     filter_lines(s + edge * 4 * ys + k * 4, ys, 1, 4, bs[edge][k], qpl, false);
                  /* chroma: edges 0 and 2 of luma are its 0 and 4 */
                  if (!(edge & 1))
                  {
                     int c;
                     for (c = 1; c <= 2; c++)
                     {
                        uint8_t *t = mb_ptr(e, e->cur, c, mbx, mby);
                        if (dir == 0)
                           filter_lines(t + k * 2 * cs + edge * 2, 1, cs, 2, bs[edge][k], qpc, true);
                        else
                           filter_lines(t + edge * 2 * cs + k * 2, cs, 1, 2, bs[edge][k], qpc, true);
                     }
                  }
               }
            }
         }
      }
}

/* the border: copies of the edge samples */
static void pad(rh264_encoder_t *e)
{
   int pl;
   for (pl = 0; pl < 3; pl++)
   {
      int b  = pl ? PAD / 2 : PAD;
      int w  = (int)(pl ? e->w16 / 2 : e->w16), h = (int)(pl ? e->h16 / 2 : e->h16);
      int st = (int)(pl ? e->c_stride : e->stride), y;
      uint8_t *o = e->pic[e->cur][pl];
      for (y = 0; y < h; y++)
      {
         uint8_t *r = o + y * st;
         memset(r - b, r[0], (size_t)b);
         memset(r + w, r[w - 1], (size_t)b);
      }
      for (y = 1; y <= b; y++)
      {
         memcpy(o - y * st - b, o - b, (size_t)(w + 2 * b));
         memcpy(o + (h - 1 + y) * st - b, o + (h - 1) * st - b, (size_t)(w + 2 * b));
      }
   }
}

/* --------------------------------------------------------------------- */
/* Pictures                                                              */
/* --------------------------------------------------------------------- */

void rh264_encoder_reconstruction(const rh264_encoder_t *e,
      const uint8_t **y, const uint8_t **cb, const uint8_t **cr,
      unsigned *y_stride, unsigned *c_stride)
{
   *y        = e->pic[!e->cur][0];
   *cb       = e->pic[!e->cur][1];
   *cr       = e->pic[!e->cur][2];
   *y_stride = e->stride;
   *c_stride = e->c_stride;
}

static double qstep(int qp)
{
   return 0.625 * pow(2.0, qp / 6.0);
}

/* A P picture's share of the bit rate: a GOP's bits over its P pictures
 * and its IDR picture, which is given four P pictures' */
static int rc_share(const rh264_encoder_t *e)
{
   uint64_t gop = e->cfg.gop;
   int t = (int)((uint64_t)e->cfg.bitrate * e->cfg.fps_den * gop
         / ((uint64_t)e->cfg.fps_num * (gop + 3)));
   return t > 1 ? t : 1;
}

/* The quantiser that should bring the picture in at its share, the debt
 * paid off over the next eight: bits fall about as the quantiser step
 * rises, so the step is the content's measure over the bits wanted */
static void rc_choose(rh264_encoder_t *e, bool idr)
{
   int    t    = rc_share(e);
   int    want = (idr ? 4 * t : t) - e->rc_debt / 8;
   double c    = idr ? e->cplx_i : e->cplx_p;
   int    q;
   if (c <= 0)
      return;
   if (want < t / 4)
      want = t / 4 > 0 ? t / 4 : 1;
   q = (int)floor(6.0 * log(c / want / 0.625) / log(2.0) + 0.5);
   e->qp = q < 10 ? 10 : q > 51 ? 51 : q;
}

size_t rh264_encoder_encode(rh264_encoder_t *e, uint8_t *dst, size_t cap,
      const uint8_t *y, size_t y_stride, const uint8_t *cb, size_t cb_stride,
      const uint8_t *cr, size_t cr_stride, bool annexb, bool *key)
{
   bool     idr;
   unsigned mbx, mby;

   if (!e || !dst || !y || !cb || !cr)
      return 0;
   idr = e->frame % e->cfg.gop == 0;
   if (key)
      *key = idr;
   if (e->cfg.bitrate)
      rc_choose(e, idr);
   e->qpc    = chroma_qp(e->qp);
   e->lambda = lambda_tab[e->qp];
   if (idr)
      e->frame_num = 0;
   else
      make_half(e);

   begin(e, dst, cap);
   if (idr && annexb)
   {
      size_t k;
      put_bits(e, 1, 32);
      for (k = 0; k < e->sps_len; k++)
         put_bits(e, e->sps[k], 8);
      put_bits(e, 1, 32);
      for (k = 0; k < e->pps_len; k++)
         put_bits(e, e->pps[k], 8);
   }

   /* slice_header(), 7.3.3 */
   nal_begin(e, idr ? 3 : 2, idr ? 5 : 1, annexb);
   put_ue(e, 0);                            /* first_mb_in_slice */
   put_ue(e, idr ? 7 : 5);                  /* I or P, the whole picture */
   put_ue(e, 0);                            /* pic_parameter_set_id */
   put_bits(e, e->frame_num, 4);
   if (idr)
      put_ue(e, e->idr_count & 1);          /* idr_pic_id */
   else
   {
      put_bits(e, 0, 1);                    /* num_ref_idx_active_override */
      put_bits(e, 0, 1);                    /* ref_pic_list_modification_flag_l0 */
   }
   if (idr)
   {
      put_bits(e, 0, 1);                    /* no_output_of_prior_pics */
      put_bits(e, 0, 1);                    /* long_term_reference */
   }
   else
      put_bits(e, 0, 1);                    /* adaptive_ref_pic_marking_mode */
   put_se(e, e->qp - 26);                   /* slice_qp_delta */
   put_ue(e, 0);                            /* disable_deblocking_filter_idc */
   put_se(e, 0);                            /* slice_alpha_c0_offset_div2 */
   put_se(e, 0);                            /* slice_beta_offset_div2 */

   e->skip_run = 0;
   for (mby = 0; mby < e->mb_h; mby++)
      for (mbx = 0; mbx < e->mb_w; mbx++)
      {
         fetch_src(e, mbx, mby, y, y_stride, cb, cb_stride, cr, cr_stride);
         if (idr)
            code_intra(e, mbx, mby, false);
         else
            code_p(e, mbx, mby);
      }
   if (e->skip_run)
      put_ue(e, e->skip_run);
   nal_end(e, annexb);

   deblock(e);
   pad(e);
   e->cur       = !e->cur;
   e->frame_num = (e->frame_num + 1) & 15;
   if (idr)
      e->idr_count++;
   e->frame++;

   if (e->cfg.bitrate)
   {
      int     t = rc_share(e);
      double  c = (double)e->pos * 8 * qstep(e->qp);
      double *k = idr ? &e->cplx_i : &e->cplx_p;
      *k = *k > 0 ? (*k + c) / 2 : c;
      e->rc_debt += (int)(e->pos * 8) - (idr ? 4 * t : t);
      if (e->rc_debt > 64 * t)
         e->rc_debt = 64 * t;
      if (e->rc_debt < -64 * t)
         e->rc_debt = -64 * t;
   }
   return e->overflow ? 0 : e->pos;
}
