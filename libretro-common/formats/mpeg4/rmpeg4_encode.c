/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rmpeg4_encode.c).
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

/* MPEG-4 Part 2 Simple profile encoder. See rmpeg4_encode.h. Written
 * from ISO/IEC 14496-2, the code tables shared with rmpeg4_video. */

#include <stdlib.h>
#include <string.h>

#include <retro_inline.h>
#include <formats/rmpeg4_encode.h>

#include "rmpeg4_tables.h"
#include "rmpeg4_idct.h"

#define ME_RANGE 15          /* whole samples; f_code 1 holds +-16 */

typedef struct
{
   uint16_t code;
   uint8_t  len;
} enc_vlc_t;

struct rmpeg4_encoder
{
   rmpeg4_encoder_config_t cfg;
   unsigned  mb_w, mb_h, stride, c_stride;
   unsigned  tinc_res, tinc_step, tinc_bits;
   uint64_t  frame;
   uint64_t  last_seconds;
   unsigned  qp;
   int       rc_debt;              /* bits over the target so far */
   unsigned  cplx_i, cplx_p;       /* bits x quantiser, by kind of VOP */

   uint8_t  *mem;
   uint8_t  *plane[2][3];          /* reconstructions: [ref, cur] */
   int       ref;

   int16_t  (*mv)[2];              /* this VOP's vectors, by macroblock */
   uint8_t  *intra;                /* macroblock coded intra in this VOP */
   int16_t  (*dc)[6];              /* reconstructed DC, for prediction */

   /* code tables by symbol */
   enc_vlc_t tc[2][2][64][28];     /* [intra][last][run][level] */
   enc_vlc_t mcbpc_i[8], mcbpc_p[24], cbpy[16], mvd[33], dcs[2][13];

   /* output */
   uint8_t  *out;
   size_t    cap, pos;
   uint64_t  acc;
   unsigned  nbits;
   bool      overflow;

   /* the macroblock being coded */
   uint8_t   src[384];             /* 16x16 luma, 8x8 Cb, 8x8 Cr */
   uint8_t   pred[384];
   int16_t   blk[6][64];
   int16_t   q[6][64];             /* its levels */
};

static const uint8_t enc_zigzag[64] =
{
    0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
   12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
   35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
   58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

/* --------------------------------------------------------------------- */
/* Bits                                                                  */
/* --------------------------------------------------------------------- */

static void put_bits(rmpeg4_encoder_t *e, uint32_t v, unsigned n)
{
   if (!n)
      return;
   e->acc    = (e->acc << n) | (v & (n == 32 ? 0xFFFFFFFFu : ((1u << n) - 1)));
   e->nbits += n;
   while (e->nbits >= 8)
   {
      e->nbits -= 8;
      if (e->pos < e->cap)
         e->out[e->pos++] = (uint8_t)(e->acc >> e->nbits);
      else
         e->overflow = true;
   }
}

static INLINE void put_vlc(rmpeg4_encoder_t *e, const enc_vlc_t *c)
{
   put_bits(e, c->code, c->len);
}

/* next_start_code(): a zero, then ones to the byte */
static void stuffing(rmpeg4_encoder_t *e)
{
   unsigned n = 8 - (e->nbits & 7);
   put_bits(e, (1u << (n - 1)) - 1, n);
}

static void start_code(rmpeg4_encoder_t *e, unsigned code)
{
   put_bits(e, 0x000001, 24);
   put_bits(e, code, 8);
}

static void begin(rmpeg4_encoder_t *e, uint8_t *dst, size_t cap)
{
   e->out      = dst;
   e->cap      = cap;
   e->pos      = 0;
   e->acc      = 0;
   e->nbits    = 0;
   e->overflow = false;
}

/* --------------------------------------------------------------------- */
/* Setup                                                                 */
/* --------------------------------------------------------------------- */

static void table(enc_vlc_t *dst, unsigned n, const rmpeg4_vlc_t *src)
{
   const rmpeg4_vlc_t *s;
   for (s = src; s->len; s++)
      if (s->a < n)
      {
         dst[s->a].code = s->code;
         dst[s->a].len  = s->len;
      }
}

static void tcoef_table(enc_vlc_t (*dst)[64][28], const rmpeg4_vlc_t *src)
{
   const rmpeg4_vlc_t *s;
   for (s = src; s->len; s++)
      if (s->c && s->c < 28)
      {
         dst[s->a][s->b][s->c].code = s->code;
         dst[s->a][s->b][s->c].len  = s->len;
      }
}

static unsigned gcd(unsigned a, unsigned b)
{
   while (b)
   {
      unsigned t = a % b;
      a = b;
      b = t;
   }
   return a;
}

rmpeg4_encoder_t *rmpeg4_encoder_new(const rmpeg4_encoder_config_t *c)
{
   rmpeg4_encoder_t *e;
   size_t ys, cs, n, total;
   unsigned g;
   uint8_t *p;
   int s;

   if (!c || !c->width || !c->height || c->width > 4096 || c->height > 4096
         || !c->fps_num || !c->fps_den)
      return NULL;
   if (!(e = (rmpeg4_encoder_t*)calloc(1, sizeof(*e))))
      return NULL;
   e->cfg = *c;
   if (!e->cfg.gop)
      e->cfg.gop = 12;
   e->qp = c->qp < 1 ? 4 : c->qp > 31 ? 31 : c->qp;
   if (c->bitrate && e->qp < 2)
      e->qp = 2;

   /* vop_time_increment_resolution: 16 bits */
   g = gcd(c->fps_num, c->fps_den);
   e->tinc_res  = c->fps_num / g;
   e->tinc_step = c->fps_den / g;
   while (e->tinc_res > 65535)
   {
      e->tinc_res  = (e->tinc_res + 1) / 2;
      e->tinc_step = (e->tinc_step + 1) / 2;
   }
   if (!e->tinc_step)
      e->tinc_step = 1;
   for (e->tinc_bits = 1; (1u << e->tinc_bits) < e->tinc_res; e->tinc_bits++)
      ;

   e->mb_w     = (c->width + 15) / 16;
   e->mb_h     = (c->height + 15) / 16;
   e->stride   = e->mb_w * 16;
   e->c_stride = e->mb_w * 8;
   ys = (size_t)e->stride * e->mb_h * 16;
   cs = (size_t)e->c_stride * e->mb_h * 8;
   n  = (size_t)e->mb_w * e->mb_h;
   total = 2 * (ys + 2 * cs) + n * (4 + 1 + 12) + 16;
   if (!(e->mem = (uint8_t*)calloc(1, total)))
   {
      free(e);
      return NULL;
   }
   p = e->mem;
   for (s = 0; s < 2; s++)
   {
      e->plane[s][0] = p; p += ys;
      e->plane[s][1] = p; p += cs;
      e->plane[s][2] = p; p += cs;
   }
   e->mv    = (int16_t (*)[2])p;   p += n * 4;
   e->dc    = (int16_t (*)[6])p;   p += n * 12;
   e->intra = p;

   table(e->mcbpc_i, 8, rmpeg4_vlc_mcbpc_intra);
   table(e->mcbpc_p, 24, rmpeg4_vlc_mcbpc_inter);
   table(e->cbpy, 16, rmpeg4_vlc_cbpy);
   table(e->mvd, 33, rmpeg4_vlc_mvd);
   table(e->dcs[0], 13, rmpeg4_vlc_dc_lum);
   table(e->dcs[1], 13, rmpeg4_vlc_dc_chr);
   tcoef_table(e->tc[1], rmpeg4_tcoef_intra);
   tcoef_table(e->tc[0], rmpeg4_tcoef_inter);
   return e;
}

void rmpeg4_encoder_free(rmpeg4_encoder_t *e)
{
   if (!e)
      return;
   free(e->mem);
   free(e);
}

size_t rmpeg4_encoder_bound(const rmpeg4_encoder_t *e)
{
   /* a block can take 64 escapes of 30 bits */
   return e ? (size_t)e->mb_w * e->mb_h * (6 * 64 * 30 / 8 + 16) + 256 : 0;
}

/* visual_object_sequence() to video_object_layer(), 6.2.2 and 6.2.3 */
static void headers(rmpeg4_encoder_t *e)
{
   start_code(e, 0xB0);
   put_bits(e, 0x03, 8);           /* Simple profile @ L3 */
   start_code(e, 0xB5);
   put_bits(e, 0, 1);              /* is_visual_object_identifier */
   put_bits(e, 1, 4);              /* video */
   put_bits(e, 0, 1);              /* video_signal_type */
   stuffing(e);
   start_code(e, 0x00);            /* video_object */
   start_code(e, 0x20);            /* video_object_layer */
   put_bits(e, 0, 1);              /* random_accessible_vol */
   put_bits(e, 1, 8);              /* Simple object type */
   put_bits(e, 0, 1);              /* is_object_layer_identifier */
   put_bits(e, 1, 4);              /* aspect: square */
   put_bits(e, 1, 1);              /* vol_control_parameters */
   put_bits(e, 1, 2);              /* chroma 4:2:0 */
   put_bits(e, 1, 1);              /* low_delay */
   put_bits(e, 0, 1);              /* vbv_parameters */
   put_bits(e, 0, 2);              /* rectangular */
   put_bits(e, 1, 1);
   put_bits(e, e->tinc_res, 16);
   put_bits(e, 1, 1);
   put_bits(e, 1, 1);              /* fixed_vop_rate */
   put_bits(e, e->tinc_step, e->tinc_bits);
   put_bits(e, 1, 1);
   put_bits(e, e->cfg.width, 13);
   put_bits(e, 1, 1);
   put_bits(e, e->cfg.height, 13);
   put_bits(e, 1, 1);
   put_bits(e, 0, 1);              /* interlaced */
   put_bits(e, 1, 1);              /* obmc_disable */
   put_bits(e, 0, 1);              /* sprite_enable */
   put_bits(e, 0, 1);              /* not_8_bit */
   put_bits(e, 0, 1);              /* quant_type: H.263 */
   put_bits(e, 1, 1);              /* complexity_estimation_disable */
   put_bits(e, 1, 1);              /* resync_marker_disable */
   put_bits(e, 0, 1);              /* data_partitioned */
   put_bits(e, 0, 1);              /* scalability */
   stuffing(e);
}

size_t rmpeg4_encoder_header(rmpeg4_encoder_t *e, uint8_t *dst, size_t cap)
{
   if (!e || !dst)
      return 0;
   begin(e, dst, cap);
   headers(e);
   return e->overflow ? 0 : e->pos;
}

/* --------------------------------------------------------------------- */
/* Transform and quantisation                                            */
/* --------------------------------------------------------------------- */

/* Arai, Agui and Nakajima in 12 fractional bits; outputs carry
 * 8 * aan[u] * aan[v] of gain, which enc_scale takes off */
#define C0_382683433 1567
#define C0_541196100 2217
#define C0_707106781 2896
#define C1_306562965 5352
#define MULC(x, c) (((x) * (c) + 2048) >> 12)

static void fdct_1d(int *d, unsigned step)
{
   int tmp0 = d[0 * step] + d[7 * step], tmp7 = d[0 * step] - d[7 * step];
   int tmp1 = d[1 * step] + d[6 * step], tmp6 = d[1 * step] - d[6 * step];
   int tmp2 = d[2 * step] + d[5 * step], tmp5 = d[2 * step] - d[5 * step];
   int tmp3 = d[3 * step] + d[4 * step], tmp4 = d[3 * step] - d[4 * step];
   int tmp10 = tmp0 + tmp3, tmp13 = tmp0 - tmp3;
   int tmp11 = tmp1 + tmp2, tmp12 = tmp1 - tmp2;
   int z1, z2, z3, z4, z5, z11, z13;

   d[0 * step] = tmp10 + tmp11;
   d[4 * step] = tmp10 - tmp11;
   z1 = MULC(tmp12 + tmp13, C0_707106781);
   d[2 * step] = tmp13 + z1;
   d[6 * step] = tmp13 - z1;
   tmp10 = tmp4 + tmp5;
   tmp11 = tmp5 + tmp6;
   tmp12 = tmp6 + tmp7;
   z5  = MULC(tmp10 - tmp12, C0_382683433);
   z2  = MULC(tmp10, C0_541196100) + z5;
   z4  = MULC(tmp12, C1_306562965) + z5;
   z3  = MULC(tmp11, C0_707106781);
   z11 = tmp7 + z3;
   z13 = tmp7 - z3;
   d[5 * step] = z13 + z2;
   d[3 * step] = z13 - z2;
   d[1 * step] = z11 + z4;
   d[7 * step] = z11 - z4;
}

/* 2^16 / (8 aan[u] aan[v]), u and v the row and column */
static const uint16_t enc_scale[64] =
{
   8192, 5906, 6270, 6967, 8192, 10426, 15137, 29692,
   5906, 4258, 4520, 5023, 5906,  7517, 10913, 21407,
   6270, 4520, 4799, 5332, 6270,  7980, 11585, 22725,
   6967, 5023, 5332, 5925, 6967,  8867, 12873, 25251,
   8192, 5906, 6270, 6967, 8192, 10426, 15137, 29692,
  10426, 7517, 7980, 8867,10426, 13270, 19266, 37791,
  15137,10913,11585,12873,15137, 19266, 27969, 54865,
  29692,21407,22725,25251,29692, 37791, 54865, 65535
};

/* The DCT of 8x8 samples (or residuals), true scale, rounded */
static void fdct(const int *in, int *out)
{
   int d[64], i;
   memcpy(d, in, sizeof(d));
   for (i = 0; i < 8; i++)
      fdct_1d(d + i * 8, 1);
   for (i = 0; i < 8; i++)
      fdct_1d(d + i, 8);
   for (i = 0; i < 64; i++)
   {
      int v = d[i] * (int)enc_scale[i];
      out[i] = v >= 0 ? (v + 32768) >> 16 : -((-v + 32768) >> 16);
   }
}

static unsigned dc_scale(unsigned qp, int n)
{
   return n < 4 ? rmpeg4_dc_scale_lum[qp] : rmpeg4_dc_scale_chr[qp];
}

static INLINE int sat(int x)
{
   return x < -2048 ? -2048 : x > 2047 ? 2047 : x;
}

/* --------------------------------------------------------------------- */
/* Coefficients                                                          */
/* --------------------------------------------------------------------- */

/* The run-level pairs of a block from @first in zigzag order, coded;
 * the last one marked */
static void code_coefs(rmpeg4_encoder_t *e, const int16_t *q, unsigned first,
      bool intra)
{
   int      last_pos = -1, i;
   unsigned run = 0;
   for (i = 63; i >= (int)first; i--)
      if (q[enc_zigzag[i]])
      {
         last_pos = i;
         break;
      }
   for (i = (int)first; i <= last_pos; i++)
   {
      int      lv = q[enc_zigzag[i]];
      unsigned last, mag;
      if (!lv)
      {
         run++;
         continue;
      }
      last = i == last_pos;
      mag  = (unsigned)(lv < 0 ? -lv : lv);
      if (mag < 28 && e->tc[intra][last][run][mag].len)
      {
         put_vlc(e, &e->tc[intra][last][run][mag]);
         put_bits(e, lv < 0, 1);
      }
      else
      {
         /* escape, type 3: last, run and a 12-bit level */
         put_bits(e, 0x03, 7);
         put_bits(e, 3, 2);
         put_bits(e, last, 1);
         put_bits(e, run, 6);
         put_bits(e, 1, 1);
         put_bits(e, (uint32_t)lv & 0xFFF, 12);
         put_bits(e, 1, 1);
      }
      run = 0;
   }
}

/* Quantised levels of an inter block, H.263 7.4.4.1's inverse: zero
 * when nothing survives */
static bool quant_inter(const int *f, int16_t *q, unsigned qp)
{
   int i;
   bool any = false;
   int  zero = (int)qp / 2 + 2 * (int)qp;   /* below it the level is 0 */
   for (i = 0; i < 64; i++)
   {
      int a = f[i] < 0 ? -f[i] : f[i];
      int l;
      if (a < zero)
      {
         q[i] = 0;
         continue;
      }
      l = (a - (int)qp / 2) / (int)(2 * qp);
      if (l > 2047)
         l = 2047;
      if (l <= 0)
         l = 0;
      q[i] = (int16_t)(f[i] < 0 ? -l : l);
      any |= l != 0;
   }
   return any;
}

/* What the decoder reconstructs from levels */
static void dequant(int16_t *blk, const int16_t *q, bool intra, unsigned qp, int n)
{
   int i, qmul = (int)qp * 2, qadd = ((int)qp - 1) | 1;
   for (i = intra ? 1 : 0; i < 64; i++)
   {
      int l = q[i];
      blk[i] = (int16_t)(l ? sat(l < 0 ? l * qmul - qadd : l * qmul + qadd) : 0);
   }
   if (intra)
      blk[0] = (int16_t)sat(q[0] * (int)dc_scale(qp, n));
}

/* --------------------------------------------------------------------- */
/* Macroblocks                                                           */
/* --------------------------------------------------------------------- */

static uint8_t *mb_ptr(rmpeg4_encoder_t *e, int s, int pl, unsigned mbx, unsigned mby)
{
   return pl ? e->plane[s][pl] + (size_t)mby * 8 * e->c_stride + mbx * 8
             : e->plane[s][0] + (size_t)mby * 16 * e->stride + mbx * 16;
}

/* The macroblock's input, past the picture's edge repeating the last
 * sample */
static void fetch_src(rmpeg4_encoder_t *e, unsigned mbx, unsigned mby,
      const uint8_t *y, size_t ys, const uint8_t *cb, size_t cbs,
      const uint8_t *cr, size_t crs)
{
   unsigned w = e->cfg.width, h = e->cfg.height;
   unsigned cw = (w + 1) / 2, ch = (h + 1) / 2;
   unsigned i, j;
   for (j = 0; j < 16; j++)
   {
      unsigned yy = mby * 16 + j < h ? mby * 16 + j : h - 1;
      const uint8_t *row = y + yy * ys;
      if (mbx * 16 + 16 <= w)
         memcpy(e->src + j * 16, row + mbx * 16, 16);
      else
         for (i = 0; i < 16; i++)
            e->src[j * 16 + i] = row[mbx * 16 + i < w ? mbx * 16 + i : w - 1];
   }
   for (j = 0; j < 8; j++)
   {
      unsigned yy = mby * 8 + j < ch ? mby * 8 + j : ch - 1;
      for (i = 0; i < 8; i++)
      {
         unsigned xx = mbx * 8 + i < cw ? mbx * 8 + i : cw - 1;
         e->src[256 + j * 8 + i] = cb[yy * cbs + xx];
         e->src[320 + j * 8 + i] = cr[yy * crs + xx];
      }
   }
}

/* Block @n of a 384-sample macroblock: its samples and stride */
static const uint8_t *blk_src(const uint8_t *mb, int n, unsigned *stride)
{
   if (n < 4)
   {
      *stride = 16;
      return mb + (n & 1) * 8 + (n >> 1) * 128;
   }
   *stride = 8;
   return mb + 256 + (n - 4) * 64;
}

/* The DC predictor, 7.4.3.1: from the left (0) or above (1), 1024 for a
 * neighbour outside the VOP or not intra */
static int dc_pred(rmpeg4_encoder_t *e, unsigned mbx, unsigned mby, int n, int *dir)
{
   int a = 1024, b = 1024, c = 1024;
   if (n < 4)
   {
      int bx = (int)mbx * 2 + (n & 1), by = (int)mby * 2 + (n >> 1);
#define LDC(x, y) (((x) < 0 || (y) < 0 || !e->intra[((y) >> 1) * e->mb_w + ((x) >> 1)]) ? 1024 \
      : e->dc[((y) >> 1) * e->mb_w + ((x) >> 1)][((x) & 1) | (((y) & 1) << 1)])
      a = LDC(bx - 1, by);
      b = LDC(bx - 1, by - 1);
      c = LDC(bx, by - 1);
#undef LDC
   }
   else
   {
#define CDC(x, y) (((x) < 0 || (y) < 0 || !e->intra[(y) * e->mb_w + (x)]) ? 1024 \
      : e->dc[(y) * e->mb_w + (x)][n])
      a = CDC((int)mbx - 1, (int)mby);
      b = CDC((int)mbx - 1, (int)mby - 1);
      c = CDC((int)mbx, (int)mby - 1);
#undef CDC
   }
   if (abs(a - b) < abs(b - c))
   {
      *dir = 1;
      return c;
   }
   *dir = 0;
   return a;
}

/* An intra macroblock: quantised, its header and blocks coded, and
 * reconstructed */
static void code_intra(rmpeg4_encoder_t *e, unsigned mbx, unsigned mby, bool in_p)
{
   int16_t  (*q)[64] = e->q;
   unsigned mb = mby * e->mb_w + mbx, cbp = 0, qp = e->qp;
   int      n;

   e->intra[mb] = 1;
   e->mv[mb][0] = e->mv[mb][1] = 0;
   for (n = 0; n < 6; n++)
   {
      unsigned       st, i, j;
      const uint8_t *s = blk_src(e->src, n, &st);
      int            in[64], f[64];
      int            scale = (int)dc_scale(qp, n), lvl;
      for (j = 0; j < 8; j++)
         for (i = 0; i < 8; i++)
            in[j * 8 + i] = (int)s[j * st + i] - 128;
      fdct(in, f);
      for (i = 1; i < 64; i++)
      {
         int a = f[i] < 0 ? -f[i] : f[i];
         int l = a / (int)(2 * qp);
         if (l > 2047)
            l = 2047;
         q[n][i] = (int16_t)(f[i] < 0 ? -l : l);
         if (l)
            cbp |= 32u >> n;
      }
      /* the DC: of the sample block, so offset by the 128 taken off */
      lvl = (f[0] + 1024 + scale / 2) / scale;
      if (lvl < 1)
         lvl = 1;
      if (lvl * scale > 2047)
         lvl = 2047 / scale;
      q[n][0] = (int16_t)lvl;
   }

   if (in_p)
   {
      put_bits(e, 0, 1);                       /* coded */
      put_vlc(e, &e->mcbpc_p[4 | (cbp & 3)]);
   }
   else
      put_vlc(e, &e->mcbpc_i[cbp & 3]);
   put_bits(e, 0, 1);                          /* ac_pred_flag */
   put_vlc(e, &e->cbpy[cbp >> 2]);

   for (n = 0; n < 6; n++)
   {
      int dir, pred, diff, mag, size, scale = (int)dc_scale(qp, n);
      unsigned st;
      uint8_t *d;
      /* predicted from the neighbours' reconstructed DCs - this
       * macroblock's own earlier blocks among them - as the decoder
       * will, then this one's kept */
      pred = dc_pred(e, mbx, mby, n, &dir);
      diff = q[n][0] - (pred + scale / 2) / scale;
      e->dc[mb][n] = (int16_t)(q[n][0] * scale > 2047 ? 2047 : q[n][0] * scale);
      mag  = diff < 0 ? -diff : diff;
      for (size = 0; mag >> size; size++)
         ;
      put_vlc(e, &e->dcs[n >= 4][size]);
      if (size)
      {
         put_bits(e, (uint32_t)(diff < 0 ? diff + (1 << size) - 1 : diff), (unsigned)size);
         if (size > 8)
            put_bits(e, 1, 1);
      }
      if (cbp & (32u >> n))
         code_coefs(e, q[n], 1, true);

      dequant(e->blk[n], q[n], true, qp, n);
      if (n < 4)
      {
         d  = mb_ptr(e, !e->ref, 0, mbx, mby) + (n & 1) * 8 + (size_t)(n >> 1) * 8 * e->stride;
         st = e->stride;
      }
      else
      {
         d  = mb_ptr(e, !e->ref, n - 3, mbx, mby);
         st = e->c_stride;
      }
      idct_block(e->blk[n], d, st);
   }
}

/* Half sample prediction of the macroblock from the reference, the
 * vector kept inside the reference (the search sees to that) */
static void predict(rmpeg4_encoder_t *e, unsigned mbx, unsigned mby, int mx, int my)
{
   int pl;
   for (pl = 0; pl < 3; pl++)
   {
      unsigned       n   = pl ? 8 : 16, st = pl ? e->c_stride : e->stride;
      int            vx  = pl ? (mx >> 1) | (mx & 1) : mx;
      int            vy  = pl ? (my >> 1) | (my & 1) : my;
      int            bx  = (int)mbx * (int)n + (vx >> 1), by = (int)mby * (int)n + (vy >> 1);
      int            pw  = (int)(pl ? e->mb_w * 8 : e->mb_w * 16);
      int            ph  = (int)(pl ? e->mb_h * 8 : e->mb_h * 16);
      unsigned       dxy = (unsigned)((vx & 1) | ((vy & 1) << 1)), i, j;
      const uint8_t *r   = e->plane[e->ref][pl];
      uint8_t       *d   = e->pred + (pl == 0 ? 0 : pl == 1 ? 256 : 320);
      if (bx >= 0 && by >= 0 && bx + (int)n < pw && by + (int)n < ph)
      {
         const uint8_t *s0 = r + (size_t)by * st + bx;
         for (j = 0; j < n; j++, s0 += st, d += n)
         {
            const uint8_t *s1 = s0 + st;
            switch (dxy)
            {
               case 0:  memcpy(d, s0, n); break;
               case 1:  for (i = 0; i < n; i++) d[i] = (uint8_t)((s0[i] + s0[i + 1] + 1) >> 1); break;
               case 2:  for (i = 0; i < n; i++) d[i] = (uint8_t)((s0[i] + s1[i] + 1) >> 1); break;
               default: for (i = 0; i < n; i++)
                           d[i] = (uint8_t)((s0[i] + s0[i + 1] + s1[i] + s1[i + 1] + 2) >> 2);
                        break;
            }
         }
         continue;
      }
      for (j = 0; j < n; j++)
         for (i = 0; i < n; i++)
         {
            int x0 = bx + (int)i, y0 = by + (int)j;
            int x1 = x0 + 1, y1 = y0 + 1, a, b, c, dd;
            x0 = x0 < 0 ? 0 : x0 >= pw ? pw - 1 : x0;
            x1 = x1 < 0 ? 0 : x1 >= pw ? pw - 1 : x1;
            y0 = y0 < 0 ? 0 : y0 >= ph ? ph - 1 : y0;
            y1 = y1 < 0 ? 0 : y1 >= ph ? ph - 1 : y1;
            a  = r[y0 * (int)st + x0];
            b  = r[y0 * (int)st + x1];
            c  = r[y1 * (int)st + x0];
            dd = r[y1 * (int)st + x1];
            d[j * n + i] = (uint8_t)(dxy == 0 ? a : dxy == 1 ? (a + b + 1) >> 1
                  : dxy == 2 ? (a + c + 1) >> 1 : (a + b + c + dd + 2) >> 2);
         }
   }
}

/* SAD of the luma against the reference at a half sample vector, or a
 * large number for one the search must not use */
static unsigned sad(rmpeg4_encoder_t *e, unsigned mbx, unsigned mby, int mx, int my,
      unsigned limit)
{
   int x = (int)mbx * 16 + (mx >> 1), y = (int)mby * 16 + (my >> 1);
   unsigned dxy = (unsigned)((mx & 1) | ((my & 1) << 1)), i, j, s = 0;
   const uint8_t *r = e->plane[e->ref][0];
   if (mx < -32 || mx > 31 || my < -32 || my > 31 || x < 0 || y < 0
         || x + 16 + (int)(dxy & 1) > (int)e->stride
         || y + 16 + (int)(dxy >> 1) > (int)(e->mb_h * 16))
      return 0xFFFFFFFFu;
   r += (size_t)y * e->stride + x;
   for (j = 0; j < 16; j++, r += e->stride)
   {
      const uint8_t *src = e->src + j * 16, *r1 = r + e->stride;
      switch (dxy)
      {
         case 0:
            for (i = 0; i < 16; i++)
               s += (unsigned)abs(src[i] - r[i]);
            break;
         case 1:
            for (i = 0; i < 16; i++)
               s += (unsigned)abs(src[i] - ((r[i] + r[i + 1] + 1) >> 1));
            break;
         case 2:
            for (i = 0; i < 16; i++)
               s += (unsigned)abs(src[i] - ((r[i] + r1[i] + 1) >> 1));
            break;
         default:
            for (i = 0; i < 16; i++)
               s += (unsigned)abs(src[i] - ((r[i] + r[i + 1] + r1[i] + r1[i + 1] + 2) >> 2));
            break;
      }
      if (s >= limit)
         return s;
   }
   return s;
}

static int median3(int a, int b, int c)
{
   if (a > b) { int t = a; a = b; b = t; }
   return c < a ? a : c > b ? b : c;
}

/* The decoder's vector predictor for a one-vector macroblock, 7.6.5,
 * the whole VOP one video packet */
static void mv_pred(rmpeg4_encoder_t *e, unsigned mbx, unsigned mby, int *px, int *py)
{
   int  xs[3], ys[3], k, valid = 0, which = 0;
   int  cx[3], cy[3];
   cx[0] = (int)mbx - 1; cy[0] = (int)mby;
   cx[1] = (int)mbx;     cy[1] = (int)mby - 1;
   cx[2] = (int)mbx + 1; cy[2] = (int)mby - 1;
   for (k = 0; k < 3; k++)
   {
      xs[k] = ys[k] = 0;
      if (cx[k] >= 0 && cy[k] >= 0 && cx[k] < (int)e->mb_w)
      {
         unsigned i = (unsigned)cy[k] * e->mb_w + (unsigned)cx[k];
         xs[k] = e->mv[i][0];
         ys[k] = e->mv[i][1];
         valid |= 1 << k;
         which = k;
      }
   }
   if (!valid)
      *px = *py = 0;
   else if (valid == 1 || valid == 2 || valid == 4)
   {
      *px = xs[which];
      *py = ys[which];
   }
   else
   {
      *px = median3(xs[0], xs[1], xs[2]);
      *py = median3(ys[0], ys[1], ys[2]);
   }
}

static void code_mvd(rmpeg4_encoder_t *e, int d)
{
   unsigned mag;
   if (d < -32)
      d += 64;
   else if (d > 31)
      d -= 64;
   mag = (unsigned)(d < 0 ? -d : d);
   put_vlc(e, &e->mvd[mag]);
   if (mag)
      put_bits(e, d < 0, 1);
}

/* A P-VOP's macroblock: the vector found, intra or inter or not coded
 * decided, coded and reconstructed */
static void code_p(rmpeg4_encoder_t *e, unsigned mbx, unsigned mby)
{
   static const int8_t diamond[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
   static const int8_t ring[8][2] = { { -1, -1 }, { 0, -1 }, { 1, -1 }, { -1, 0 },
                                      { 1, 0 }, { -1, 1 }, { 0, 1 }, { 1, 1 } };
   unsigned mb = mby * e->mb_w + mbx, best, i, mean = 0, dev = 0, cbp = 0;
   int      px, py, bx = 0, by = 0, k, n;
   int      cand[5][2];
   int16_t  (*q)[64] = e->q;
   bool     coded[6];

   mv_pred(e, mbx, mby, &px, &py);

   /* candidates: none, the predictor and the neighbours' vectors, in
    * whole samples */
   cand[0][0] = 0;                         cand[0][1] = 0;
   cand[1][0] = px & ~1;                   cand[1][1] = py & ~1;
   cand[2][0] = mbx ? e->mv[mb - 1][0] & ~1 : 0;
   cand[2][1] = mbx ? e->mv[mb - 1][1] & ~1 : 0;
   cand[3][0] = mby ? e->mv[mb - e->mb_w][0] & ~1 : 0;
   cand[3][1] = mby ? e->mv[mb - e->mb_w][1] & ~1 : 0;
   cand[4][0] = mby && mbx + 1 < e->mb_w ? e->mv[mb - e->mb_w + 1][0] & ~1 : 0;
   cand[4][1] = mby && mbx + 1 < e->mb_w ? e->mv[mb - e->mb_w + 1][1] & ~1 : 0;
   best = 0xFFFFFFFFu;
   for (k = 0; k < 5; k++)
   {
      unsigned s = sad(e, mbx, mby, cand[k][0], cand[k][1], best);
      if (s < best)
      {
         best = s;
         bx   = cand[k][0];
         by   = cand[k][1];
      }
   }
   /* a small diamond, whole samples, while it improves */
   for (n = 0; n < 2 * ME_RANGE; n++)
   {
      int nbx = bx, nby = by;
      for (k = 0; k < 4; k++)
      {
         unsigned s = sad(e, mbx, mby, bx + 2 * diamond[k][0], by + 2 * diamond[k][1], best);
         if (s < best)
         {
            best = s;
            nbx  = bx + 2 * diamond[k][0];
            nby  = by + 2 * diamond[k][1];
         }
      }
      if (nbx == bx && nby == by)
         break;
      bx = nbx;
      by = nby;
   }
   /* the half samples around it */
   {
      int cx = bx, cy = by;
      for (k = 0; k < 8; k++)
      {
         unsigned s = sad(e, mbx, mby, cx + ring[k][0], cy + ring[k][1], best);
         if (s < best)
         {
            best = s;
            bx   = cx + ring[k][0];
            by   = cy + ring[k][1];
         }
      }
   }

   /* intra if the macroblock's own variation is clearly the cheaper */
   for (i = 0; i < 256; i++)
      mean += e->src[i];
   mean = (mean + 128) / 256;
   for (i = 0; i < 256; i++)
      dev += (unsigned)abs((int)e->src[i] - (int)mean);
   if (best == 0xFFFFFFFFu || dev + 512 < best)
   {
      code_intra(e, mbx, mby, true);
      return;
   }

   e->intra[mb] = 0;
   predict(e, mbx, mby, bx, by);
   for (n = 0; n < 6; n++)
   {
      unsigned st, j;
      const uint8_t *s = blk_src(e->src, n, &st);
      const uint8_t *p = blk_src(e->pred, n, &st);
      int in[64], f[64];
      for (j = 0; j < 8; j++)
         for (i = 0; i < 8; i++)
            in[j * 8 + i] = (int)s[j * st + i] - (int)p[j * st + i];
      fdct(in, f);
      coded[n] = quant_inter(f, q[n], e->qp);
      if (coded[n])
         cbp |= 32u >> n;
   }

   if (!cbp && !bx && !by)
   {
      put_bits(e, 1, 1);                       /* not coded */
      e->mv[mb][0] = e->mv[mb][1] = 0;
   }
   else
   {
      put_bits(e, 0, 1);
      put_vlc(e, &e->mcbpc_p[cbp & 3]);
      put_vlc(e, &e->cbpy[(cbp >> 2) ^ 15]);
      code_mvd(e, bx - px);
      code_mvd(e, by - py);
      for (n = 0; n < 6; n++)
         if (coded[n])
            code_coefs(e, q[n], 0, false);
      e->mv[mb][0] = (int16_t)bx;
      e->mv[mb][1] = (int16_t)by;
   }

   /* the reconstruction: the prediction, and the residual on it */
   for (n = 0; n < 6; n++)
   {
      unsigned st, j, dst_st = n < 4 ? e->stride : e->c_stride;
      const uint8_t *p = blk_src(e->pred, n, &st);
      uint8_t *d = n < 4
         ? mb_ptr(e, !e->ref, 0, mbx, mby) + (n & 1) * 8 + (size_t)(n >> 1) * 8 * e->stride
         : mb_ptr(e, !e->ref, n - 3, mbx, mby);
      for (j = 0; j < 8; j++)
         memcpy(d + j * dst_st, p + j * st, 8);
      if (coded[n])
      {
         dequant(e->blk[n], q[n], false, e->qp, n);
         idct_block_add(e->blk[n], d, dst_st);
      }
   }
}

/* --------------------------------------------------------------------- */

void rmpeg4_encoder_reconstruction(const rmpeg4_encoder_t *e,
      const uint8_t **y, const uint8_t **cb, const uint8_t **cr,
      unsigned *y_stride, unsigned *c_stride)
{
   *y        = e->plane[e->ref][0];
   *cb       = e->plane[e->ref][1];
   *cr       = e->plane[e->ref][2];
   *y_stride = e->stride;
   *c_stride = e->c_stride;
}

/* A P-VOP's share of the bit rate: a GOP's bits over its P-VOPs and its
 * I-VOP, which is given three P-VOPs' */
static int rc_share(const rmpeg4_encoder_t *e)
{
   uint64_t gop = e->cfg.gop;
   int t = (int)((uint64_t)e->cfg.bitrate * e->cfg.fps_den * gop
         / ((uint64_t)e->cfg.fps_num * (gop + 2)));
   return t > 1 ? t : 1;
}

/* The quantiser that should bring the frame in at its share, the debt
 * paid off over the next eight: bits fall about as the quantiser rises,
 * so it is the content's measure over the bits wanted. An I-VOP's share
 * is three P-VOPs'. */
static void rc_choose(rmpeg4_encoder_t *e, bool intra)
{
   int      t    = rc_share(e);
   int      want = (intra ? 3 * t : t) - e->rc_debt / 8;
   unsigned c    = intra ? e->cplx_i : e->cplx_p, q;
   if (!c)
      return;                         /* nothing measured yet: as configured */
   if (want < t / 4)
      want = t / 4 > 0 ? t / 4 : 1;
   q = (c + (unsigned)want / 2) / (unsigned)want;
   e->qp = q < 2 ? 2 : q > 31 ? 31 : q;
}

size_t rmpeg4_encoder_encode(rmpeg4_encoder_t *e, uint8_t *dst, size_t cap,
      const uint8_t *y, size_t y_stride, const uint8_t *cb, size_t cb_stride,
      const uint8_t *cr, size_t cr_stride, bool *key)
{
   bool     intra;
   uint64_t time, seconds;
   unsigned mbx, mby;

   if (!e || !dst || !y || !cb || !cr)
      return 0;
   intra = e->frame % e->cfg.gop == 0;
   if (key)
      *key = intra;
   if (e->cfg.bitrate)
      rc_choose(e, intra);
   begin(e, dst, cap);

   if (intra)
      headers(e);
   start_code(e, 0xB6);
   put_bits(e, intra ? 0 : 1, 2);
   time    = e->frame * e->tinc_step;
   seconds = time / e->tinc_res;
   while (e->last_seconds < seconds)
   {
      put_bits(e, 1, 1);                       /* modulo_time_base */
      e->last_seconds++;
   }
   put_bits(e, 0, 1);
   put_bits(e, 1, 1);
   put_bits(e, (uint32_t)(time % e->tinc_res), e->tinc_bits);
   put_bits(e, 1, 1);
   put_bits(e, 1, 1);                          /* vop_coded */
   if (!intra)
      put_bits(e, 0, 1);                       /* vop_rounding_type */
   put_bits(e, 0, 3);                          /* intra_dc_vlc_thr: always */
   put_bits(e, e->qp, 5);
   if (!intra)
      put_bits(e, 1, 3);                       /* vop_fcode_forward */

   memset(e->intra, 0, (size_t)e->mb_w * e->mb_h);
   for (mby = 0; mby < e->mb_h; mby++)
      for (mbx = 0; mbx < e->mb_w; mbx++)
      {
         fetch_src(e, mbx, mby, y, y_stride, cb, cb_stride, cr, cr_stride);
         if (intra)
            code_intra(e, mbx, mby, false);
         else
            code_p(e, mbx, mby);
      }
   stuffing(e);

   e->ref = !e->ref;
   e->frame++;

   /* what the frame told about the content: bits times quantiser, for
    * its kind of VOP */
   if (e->cfg.bitrate)
   {
      int t = rc_share(e);
      unsigned c = (unsigned)(e->pos * 8) * e->qp;
      unsigned *k = intra ? &e->cplx_i : &e->cplx_p;
      *k = *k ? (*k + c) / 2 : c;
      e->rc_debt += (int)(e->pos * 8) - (intra ? 3 * t : t);
      if (e->rc_debt > 64 * t)
         e->rc_debt = 64 * t;
      if (e->rc_debt < -64 * t)
         e->rc_debt = -64 * t;
   }
   return e->overflow ? 0 : e->pos;
}
