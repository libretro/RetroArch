/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rmpeg4_video.c).
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

/* MPEG-4 Part 2 video, Simple and Advanced Simple profiles, and the short
 * video header. See rmpeg4_video.h.
 *
 * Written from ISO/IEC 14496-2. Where the standard leaves an arithmetic
 * detail to the decoder - the quarter sample filter's two-stage averaging,
 * the chroma vector of a quarter sample macroblock, sample clamping at
 * the edge of a field - the choice is the one the encoders in use expect
 * (Xvid, DivX, FFmpeg's), so that their streams decode without drift. */

#include <stdlib.h>
#include <string.h>

#include <retro_inline.h>
#include <formats/rmpeg4_video.h>

#include "rmpeg4_tables.h"
#include "rmpeg4_idct.h"

#define M4_MAX_W      4096
#define M4_MAX_H      4096
#define M4_LUT_BITS   10
#define M4_LUT_SIZE   (1 << M4_LUT_BITS)

enum
{
   T_MCBPC_I = 0,
   T_MCBPC_P,
   T_CBPY,
   T_MVD,
   T_DC_LUM,
   T_DC_CHR,
   T_MB_TYPE_B,
   T_SPRITE,
   T_TC_INTRA,
   T_TC_INTER,
   T_RV_INTRA,
   T_RV_INTER,
   T_COUNT
};

static const rmpeg4_vlc_t * const m4_tables[T_COUNT] =
{
   rmpeg4_vlc_mcbpc_intra,
   rmpeg4_vlc_mcbpc_inter,
   rmpeg4_vlc_cbpy,
   rmpeg4_vlc_mvd,
   rmpeg4_vlc_dc_lum,
   rmpeg4_vlc_dc_chr,
   rmpeg4_vlc_mb_type_b,
   rmpeg4_vlc_sprite_dmv,
   rmpeg4_tcoef_intra,
   rmpeg4_tcoef_inter,
   rmpeg4_rvlc_intra,
   rmpeg4_rvlc_inter
};

typedef struct
{
   int16_t idx;   /* entry, or -1: no code this short starts here */
   uint8_t len;
} m4_lut_t;

/* Start codes, Table 6-3 */
#define SC_VO_LO        0x00
#define SC_VO_HI        0x1F
#define SC_VOL_LO       0x20
#define SC_VOL_HI       0x2F
#define SC_VOS          0xB0
#define SC_VOS_END      0xB1
#define SC_USER         0xB2
#define SC_GOV          0xB3
#define SC_VO_OBJ       0xB5
#define SC_VOP          0xB6

/* VOP coding types, as vop_coding_type + 1 */
#define VOP_I 1
#define VOP_P 2
#define VOP_B 3
#define VOP_S 4

/* Macroblock kinds kept for prediction and for the B-VOPs after */
#define MB_INTRA  0x01
#define MB_SKIP   0x02   /* not coded in a P-VOP: a B-VOP skips it too */
#define MB_4MV    0x04
#define MB_FIELD  0x08
#define MB_GMC    0x10

#define MV_16X16  0
#define MV_8X8    1
#define MV_FIELD  2

#define DIR_FWD   1
#define DIR_BWD   2

#define SPRITE_STATIC 1
#define SPRITE_GMC    2

/* A macroblock's forward vectors, kept for the vectors predicted from
 * it and, in the reference, for direct mode */
typedef struct
{
   int16_t mv[4][2];    /* block by block; a field macroblock's average */
   int16_t fmv[2][2];   /* field vectors */
   uint8_t fsel[2];     /* their reference fields */
   uint8_t type;
} m4_mbmv_t;

/* A macroblock's intra prediction state */
typedef struct
{
   uint32_t vop;         /* serial of the VOP it was intra in */
   int16_t  dc[6];       /* the reconstructed DC, clipped */
   int16_t  ac[6][14];   /* quantised: first column 1..7, first row 1..7 */
   uint8_t  qp;
} m4_intra_t;

/* What the first two partitions of a data partitioned packet carry */
typedef struct
{
   uint8_t type;
   uint8_t cbp;
   uint8_t qp;
   uint8_t ac_pred;
   uint8_t dirs;      /* DC prediction from above, a bit a block */
   uint8_t mcsel;
   uint8_t dc_coded;  /* the DCs were in the partition */
   uint8_t not_coded;
   int16_t dc[6];     /* their quantised levels */
} m4_part_t;

/* The macroblock being decoded */
typedef struct
{
   unsigned type;
   unsigned cbp;         /* bit 5 is block 0 */
   unsigned dirs;        /* DIR_FWD | DIR_BWD */
   unsigned mvtype;
   int      mv[2][4][2];
   unsigned fsel[2][2];
   bool     ac_pred;
   bool     field_dct;
   bool     mcsel;
} m4_mb_t;

typedef struct
{
   const uint8_t *p;
   size_t         bytes;
   size_t         pos;   /* bits */
} m4_bits_t;

struct rmpeg4_video
{
   m4_lut_t   lut[T_COUNT][M4_LUT_SIZE];

   m4_bits_t  gb;

   /* VOL */
   bool       has_vol;
   bool       vol_unsupported;
   bool       short_header;
   bool       low_delay;
   bool       interlaced;      /* !progressive_sequence */
   bool       mpeg_quant;
   bool       quarter_sample;
   bool       resync;
   bool       data_partitioned;
   bool       rvlc;
   bool       newpred;
   unsigned   width, height;
   unsigned   mb_w, mb_h, mb_count, mb_num_bits;
   unsigned   par_num, par_den;
   unsigned   tinc_res, tinc_bits;
   unsigned   sprite_usage, sprite_points, sprite_accuracy;
   unsigned   quant_bits;
   unsigned   cplx_i, cplx_p, cplx_b;
   uint8_t    intra_matrix[64];   /* raster order */
   uint8_t    inter_matrix[64];

   /* VOP */
   unsigned   pict;
   unsigned   qp;
   unsigned   fcode, bcode;
   unsigned   rounding;
   unsigned   dc_thr;
   bool       top_field_first;
   bool       alternate_scan;
   bool       partitioned;    /* this VOP's packets are partitioned */
   int64_t    time_base, last_time_base, time, last_non_b_time;
   int        pp_time, pb_time, pp_field_time, pb_field_time;
   int        t_frame;
   unsigned   sh_tr;           /* short header temporal reference */

   /* global motion */
   int        sprite_offset[2][2];
   int        sprite_delta[2][2];
   int        sprite_shift[2];
   int        real_points;

   /* planes: three slots, two references and a B-VOP */
   uint8_t   *mem;
   size_t     mem_size;
   uint8_t   *plane[3][3];
   unsigned   y_stride, c_stride;
   unsigned   alloc_mb_w, alloc_mb_h;
   int        ref_old, ref_new;     /* slots, or -1 */
   int        held;                 /* a reference not shown yet, or -1 */
   uint8_t    slot_type[3];
   uint32_t   slot_time[3];
   uint64_t   slot_tag[3];          /* the caller's tag when each began */
   uint64_t   tag;                  /* rmpeg4_video_set_tag's, for what comes next */
   int        cur;                  /* slot being decoded */

   m4_mbmv_t *mvs;        /* this VOP's vectors */
   m4_mbmv_t *col;        /* the backward reference's, for direct mode */
   m4_intra_t *intra;
   m4_part_t *part;

   uint32_t   serial;
   unsigned   packet_start;
   int        last_mv[2][2][2];     /* B-VOP predictors [dir][field][x,y] */

   uint32_t   skipped;
   uint32_t   errors;

   /* scratch, here rather than on the stack */
   m4_mb_t    mb;
   int16_t    blk[6][64];
   uint8_t    emu[18 * 32];
   uint8_t    pred[16 * 16 + 2 * 8 * 8];
   uint8_t    half_h[17 * 16];
   uint8_t    half_hv[16 * 16];
};

/* --------------------------------------------------------------------- */
/* Constants                                                             */
/* --------------------------------------------------------------------- */

static const uint8_t m4_zigzag[64] =
{
    0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
   12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
   35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
   58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

/* Figure 7-6 */
static const uint8_t m4_alt_horizontal[64] =
{
    0,  1,  2,  3,  8,  9, 16, 17, 10, 11,  4,  5,  6,  7, 15, 14,
   13, 12, 19, 18, 24, 25, 32, 33, 26, 27, 20, 21, 22, 23, 28, 29,
   30, 31, 34, 35, 40, 41, 48, 49, 42, 43, 36, 37, 38, 39, 44, 45,
   46, 47, 50, 51, 56, 57, 58, 59, 52, 53, 54, 55, 60, 61, 62, 63
};

static const uint8_t m4_alt_vertical[64] =
{
    0,  8, 16, 24,  1,  9,  2, 10, 17, 25, 32, 40, 48, 56, 57, 49,
   41, 33, 26, 18,  3, 11,  4, 12, 19, 27, 34, 42, 50, 58, 35, 43,
   51, 59, 20, 28,  5, 13,  6, 14, 21, 29, 36, 44, 52, 60, 37, 45,
   53, 61, 22, 30,  7, 15, 23, 31, 38, 46, 54, 62, 39, 47, 55, 63
};

/* aspect_ratio_info, Table 6-12 */
static const uint8_t m4_par[6][2] =
{
   { 1, 1 }, { 1, 1 }, { 12, 11 }, { 10, 11 }, { 16, 11 }, { 40, 33 }
};

/* --------------------------------------------------------------------- */
/* Bit reader, over the caller's bytes                                   */
/* --------------------------------------------------------------------- */

/* The next @n bits (1..32); reads past the end are zero */
static INLINE uint32_t show(const m4_bits_t *b, unsigned n)
{
   size_t   i = b->pos >> 3;
   uint64_t acc;

   if (i + 8 <= b->bytes)
   {
      const uint8_t *p = b->p + i;
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
         acc = (acc << 8) | (i + k < b->bytes ? b->p[i + k] : 0);
   }
   acc <<= (b->pos & 7);
   return (uint32_t)(acc >> (64 - n));
}

static INLINE void skip(m4_bits_t *b, unsigned n) { b->pos += n; }

static INLINE uint32_t get(m4_bits_t *b, unsigned n)
{
   uint32_t r = show(b, n);
   b->pos += n;
   return r;
}

static INLINE unsigned get1(m4_bits_t *b)
{
   return (unsigned)get(b, 1);
}

/* A signed n-bit field as dc_differential and dmv_code code it: the
 * leading bit set for positive values, clear for negative */
static INLINE int get_xbits(m4_bits_t *b, unsigned n)
{
   int v = (int)get(b, n);
   if (!(v >> (n - 1)))
      v -= (1 << n) - 1;
   return v;
}

static INLINE bool bits_over(const m4_bits_t *b)
{
   return b->pos > b->bytes * 8;
}

static INLINE long bits_left(const m4_bits_t *b)
{
   return (long)(b->bytes * 8) - (long)b->pos;
}

static void align(m4_bits_t *b)
{
   b->pos = (b->pos + 7) & ~(size_t)7;
}

/* --------------------------------------------------------------------- */
/* Variable length codes                                                 */
/* --------------------------------------------------------------------- */

static void build_luts(rmpeg4_video_t *v)
{
   int t;
   for (t = 0; t < T_COUNT; t++)
   {
      const rmpeg4_vlc_t *e;
      int                 i;
      for (i = 0; i < M4_LUT_SIZE; i++)
      {
         v->lut[t][i].idx = -1;
         v->lut[t][i].len = 0;
      }
      for (e = m4_tables[t], i = 0; e->len; e++, i++)
      {
         unsigned shift, base, k;
         if (e->len > M4_LUT_BITS)
            continue;
         shift = M4_LUT_BITS - e->len;
         base  = (unsigned)e->code << shift;
         for (k = 0; k < (1u << shift); k++)
         {
            v->lut[t][base + k].idx = (int16_t)i;
            v->lut[t][base + k].len = e->len;
         }
      }
   }
}

/* The entry the next code is, or NULL */
static const rmpeg4_vlc_t *vlc(rmpeg4_video_t *v, int t)
{
   const rmpeg4_vlc_t *tab = m4_tables[t];
   const m4_lut_t     *h   = &v->lut[t][show(&v->gb, M4_LUT_BITS)];
   const rmpeg4_vlc_t *e;

   if (h->len)
   {
      skip(&v->gb, h->len);
      return &tab[h->idx];
   }
   for (e = tab; e->len; e++)
      if (e->len > M4_LUT_BITS && show(&v->gb, e->len) == e->code)
      {
         skip(&v->gb, e->len);
         return e;
      }
   return NULL;
}

/* A one-value table's symbol, or -1 */
static INLINE int vlc_value(rmpeg4_video_t *v, int t)
{
   const rmpeg4_vlc_t *e = vlc(v, t);
   return e ? e->a : -1;
}



/* --------------------------------------------------------------------- */
/* Planes                                                                */
/* --------------------------------------------------------------------- */

/* Everything a size needs, in one block: allocated when the size changes,
 * never for a VOP */
static bool alloc_planes(rmpeg4_video_t *v)
{
   size_t ys, cs, n, total;
   uint8_t *p;
   int s;

   if (v->mem && v->alloc_mb_w == v->mb_w && v->alloc_mb_h == v->mb_h)
      return true;

   free(v->mem);
   v->mem        = NULL;
   v->alloc_mb_w = v->alloc_mb_h = 0;

   v->y_stride = v->mb_w * 16;
   v->c_stride = v->mb_w * 8;
   ys = (size_t)v->y_stride * v->mb_h * 16;
   cs = (size_t)v->c_stride * v->mb_h * 8;
   n  = (size_t)v->mb_w * v->mb_h;

   total = 3 * (ys + 2 * cs)
         + 2 * n * sizeof(m4_mbmv_t)
         + n * sizeof(m4_intra_t)
         + n * sizeof(m4_part_t) + 64;
   if (!(v->mem = (uint8_t*)calloc(1, total)))
      return false;
   v->mem_size = total;

   p = v->mem;
   for (s = 0; s < 3; s++)
   {
      v->plane[s][0] = p; p += ys;
      v->plane[s][1] = p; p += cs;
      v->plane[s][2] = p; p += cs;
      memset(v->plane[s][0], 16, ys);
      memset(v->plane[s][1], 128, 2 * cs);
   }
   p = v->mem + (((size_t)(p - v->mem) + 15) & ~(size_t)15);
   v->mvs   = (m4_mbmv_t*)p;  p += n * sizeof(m4_mbmv_t);
   v->col   = (m4_mbmv_t*)p;  p += n * sizeof(m4_mbmv_t);
   v->intra = (m4_intra_t*)p; p += n * sizeof(m4_intra_t);
   v->part  = (m4_part_t*)p;

   v->alloc_mb_w = v->mb_w;
   v->alloc_mb_h = v->mb_h;
   v->ref_old = v->ref_new = v->held = -1;
   return true;
}

static bool set_size(rmpeg4_video_t *v, unsigned w, unsigned h)
{
   unsigned n;
   if (!w || !h || w > M4_MAX_W || h > M4_MAX_H)
      return false;
   v->width    = w;
   v->height   = h;
   v->mb_w     = (w + 15) / 16;
   v->mb_h     = (h + 15) / 16;
   v->mb_count = v->mb_w * v->mb_h;
   for (n = 1; (1u << n) < v->mb_count; n++)
      ;
   v->mb_num_bits = n;
   return alloc_planes(v);
}

/* --------------------------------------------------------------------- */
/* Headers                                                               */
/* --------------------------------------------------------------------- */

static void load_matrix(m4_bits_t *b, uint8_t *m)
{
   unsigned i, last = 0;
   for (i = 0; i < 64; i++)
   {
      unsigned q = get(b, 8);
      if (!q)
         break;
      last = q;
      m[m4_zigzag[i]] = (uint8_t)q;
   }
   for (; i < 64; i++)
      m[m4_zigzag[i]] = (uint8_t)last;
}

/* video_object_layer(), 6.2.3 */
static void parse_vol(rmpeg4_video_t *v)
{
   m4_bits_t *b = &v->gb;
   unsigned verid = 1, ar, shape, w = 0, h = 0, vo_type;
   bool     ok = true;

   skip(b, 1);                     /* random_accessible_vol */
   vo_type = get(b, 8);
   if (vo_type == 0x22 || vo_type == 0x23)     /* studio profiles */
      ok = false;
   if (get1(b))                    /* is_object_layer_identifier */
   {
      verid = get(b, 4);
      skip(b, 3);
   }
   ar = get(b, 4);
   v->par_num = v->par_den = 1;
   if (ar == 15)
   {
      v->par_num = get(b, 8);
      v->par_den = get(b, 8);
      if (!v->par_num || !v->par_den)
         v->par_num = v->par_den = 1;
   }
   else if (ar < 6)
   {
      v->par_num = m4_par[ar][0];
      v->par_den = m4_par[ar][1];
   }

   /* B-VOPs may follow unless the VOL says otherwise; a Simple profile
    * object has none */
   v->low_delay = vo_type == 1;
   if (get1(b))                    /* vol_control_parameters */
   {
      if (get(b, 2) != 1)          /* chroma_format 4:2:0 */
         ok = false;
      v->low_delay = get1(b) != 0;
      if (get1(b))                 /* vbv_parameters */
         skip(b, 15 + 1 + 15 + 1 + 15 + 1 + 3 + 11 + 1 + 15 + 1);
   }
   shape = get(b, 2);
   if (shape != 0)
      ok = false;                  /* only rectangular VOLs */
   if (shape == 3 && verid != 1)
      skip(b, 4);
   skip(b, 1);
   v->tinc_res = get(b, 16);
   if (!v->tinc_res)
   {
      v->has_vol = false;
      return;
   }
   for (v->tinc_bits = 1; (1u << v->tinc_bits) < v->tinc_res; v->tinc_bits++)
      ;
   skip(b, 1);
   if (get1(b))                    /* fixed_vop_rate */
      skip(b, v->tinc_bits);

   if (shape != 2)
   {
      if (shape == 0)
      {
         skip(b, 1);
         w = get(b, 13);
         skip(b, 1);
         h = get(b, 13);
         skip(b, 1);
      }
      v->interlaced = get1(b) != 0;
      if (!get1(b))                /* obmc_disable */
         ok = false;
      v->sprite_usage = verid == 1 ? get1(b) : get(b, 2);
      if (v->sprite_usage == SPRITE_STATIC)
         ok = false;
      if (v->sprite_usage == SPRITE_STATIC || v->sprite_usage == SPRITE_GMC)
      {
         if (v->sprite_usage == SPRITE_STATIC)
            skip(b, 14 * 4);
         v->sprite_points   = get(b, 6);
         v->sprite_accuracy = get(b, 2);
         if (get1(b))              /* sprite_brightness_change */
            ok = false;
         if (v->sprite_usage == SPRITE_STATIC)
            skip(b, 1);
         if (v->sprite_points > 3)
            ok = false;
      }
      v->quant_bits = 5;
      if (get1(b))                 /* not_8_bit */
      {
         v->quant_bits = get(b, 4);
         if (get(b, 4) != 8)
            ok = false;
         if (v->quant_bits < 3 || v->quant_bits > 9)
            v->quant_bits = 5;
      }
      v->mpeg_quant = get1(b) != 0;
      memcpy(v->intra_matrix, rmpeg4_default_intra, 64);
      memcpy(v->inter_matrix, rmpeg4_default_inter, 64);
      if (v->mpeg_quant)
      {
         if (get1(b))
            load_matrix(b, v->intra_matrix);
         if (get1(b))
            load_matrix(b, v->inter_matrix);
      }
      v->quarter_sample = verid != 1 && get1(b);

      /* define_vop_complexity_estimation_header(): the VOP header then
       * carries these 4 and 8-bit counts, which are skipped */
      v->cplx_i = v->cplx_p = v->cplx_b = 0;
      if (!get1(b))
      {
         size_t   at     = b->pos;
         unsigned method = get(b, 2);
         if (method < 2)
         {
            if (!get1(b))
            {
               v->cplx_i += 8 * (get1(b) + get1(b) + get1(b) + get1(b) + get1(b) + get1(b));
            }
            if (!get1(b))
            {
               v->cplx_i += 8 * get1(b);
               v->cplx_p += 8 * get1(b);
               v->cplx_p += 8 * get1(b);
               v->cplx_i += 8 * get1(b);
            }
            if (!get1(b))
            {
               b->pos = at;
               v->cplx_i = v->cplx_p = v->cplx_b = 0;
               goto no_cplx;
            }
            if (!get1(b))
            {
               v->cplx_i += 8 * get1(b);
               v->cplx_i += 8 * get1(b);
               v->cplx_i += 8 * get1(b);
               v->cplx_i += 4 * get1(b);
            }
            if (!get1(b))
            {
               v->cplx_p += 8 * get1(b);
               v->cplx_p += 8 * get1(b);
               v->cplx_b += 8 * get1(b);
               v->cplx_p += 8 * get1(b);
               v->cplx_p += 8 * get1(b);
               v->cplx_p += 8 * get1(b);
            }
            if (!get1(b))
            {
               b->pos = at;
               v->cplx_i = v->cplx_p = v->cplx_b = 0;
               goto no_cplx;
            }
            if (method == 1)
            {
               v->cplx_i += 8 * get1(b);
               v->cplx_p += 8 * get1(b);
            }
         }
      }
no_cplx:
      v->resync           = !get1(b);
      v->data_partitioned = get1(b) != 0;
      v->rvlc             = v->data_partitioned && get1(b);
      v->newpred          = false;
      if (verid != 1)
      {
         if ((v->newpred = get1(b) != 0))
         {
            skip(b, 3);
            ok = false;
         }
         if (get1(b))              /* reduced_resolution_vop_enable */
            ok = false;
      }
      if (get1(b))                 /* scalability */
         ok = false;
   }
   else
      ok = false;

   v->short_header    = false;
   v->vol_unsupported = !ok;
   v->has_vol         = set_size(v, w, h);
   v->t_frame         = 0;
}

/* sprite_trajectory() and the warping parameters of 7.8.4, as integers,
 * for GMC with up to three points */
static void parse_sprite(rmpeg4_video_t *v)
{
   m4_bits_t *b    = &v->gb;
   int a           = 2 << v->sprite_accuracy;
   int rho         = 3 - (int)v->sprite_accuracy;
   int r           = 16 / a;
   int alpha       = 1, beta = 0;
   int w           = (int)v->width;
   int h           = (int)v->height;
   int i, w2, h2, w3, h3, min_ab;
   int d[4][2]     = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
   int vop_ref[4][2];
   int sprite_ref[3][2];
   int virtual_ref[2][2];
   int64_t so[2][2], sd[2][2];

   vop_ref[0][0] = 0; vop_ref[0][1] = 0;
   vop_ref[1][0] = w; vop_ref[1][1] = 0;
   vop_ref[2][0] = 0; vop_ref[2][1] = h;
   vop_ref[3][0] = w; vop_ref[3][1] = h;

   for (i = 0; i < (int)v->sprite_points; i++)
   {
      int len = vlc_value(v, T_SPRITE);
      int x = 0, y = 0;
      if (len > 0)
         x = get_xbits(b, (unsigned)len);
      skip(b, 1);
      len = vlc_value(v, T_SPRITE);
      if (len > 0)
         y = get_xbits(b, (unsigned)len);
      skip(b, 1);
      d[i][0] = x;
      d[i][1] = y;
   }

   while ((1 << alpha) < w)
      alpha++;
   while ((1 << beta) < h)
      beta++;
   w2 = 1 << alpha;
   h2 = 1 << beta;

   sprite_ref[0][0] = (a >> 1) * (2 * vop_ref[0][0] + d[0][0]);
   sprite_ref[0][1] = (a >> 1) * (2 * vop_ref[0][1] + d[0][1]);
   sprite_ref[1][0] = (a >> 1) * (2 * vop_ref[1][0] + d[0][0] + d[1][0]);
   sprite_ref[1][1] = (a >> 1) * (2 * vop_ref[1][1] + d[0][1] + d[1][1]);
   sprite_ref[2][0] = (a >> 1) * (2 * vop_ref[2][0] + d[0][0] + d[2][0]);
   sprite_ref[2][1] = (a >> 1) * (2 * vop_ref[2][1] + d[0][1] + d[2][1]);

#define RDIV(n, q) ((n) >= 0 ? ((n) + (q) / 2) / (q) : ((n) - (q) / 2) / (q))
   virtual_ref[0][0] = 16 * (vop_ref[0][0] + w2) + (int)RDIV(
         (int64_t)(w - w2) * (r * sprite_ref[0][0] - 16 * vop_ref[0][0])
       + (int64_t)w2 * (r * sprite_ref[1][0] - 16 * vop_ref[1][0]), (int64_t)w);
   virtual_ref[0][1] = 16 * vop_ref[0][1] + (int)RDIV(
         (int64_t)(w - w2) * (r * sprite_ref[0][1] - 16 * vop_ref[0][1])
       + (int64_t)w2 * (r * sprite_ref[1][1] - 16 * vop_ref[1][1]), (int64_t)w);
   virtual_ref[1][0] = 16 * vop_ref[0][0] + (int)RDIV(
         (int64_t)(h - h2) * (r * sprite_ref[0][0] - 16 * vop_ref[0][0])
       + (int64_t)h2 * (r * sprite_ref[2][0] - 16 * vop_ref[2][0]), (int64_t)h);
   virtual_ref[1][1] = 16 * (vop_ref[0][1] + h2) + (int)RDIV(
         (int64_t)(h - h2) * (r * sprite_ref[0][1] - 16 * vop_ref[0][1])
       + (int64_t)h2 * (r * sprite_ref[2][1] - 16 * vop_ref[2][1]), (int64_t)h);
#undef RDIV

   switch (v->sprite_points)
   {
      case 0:
         so[0][0] = so[0][1] = so[1][0] = so[1][1] = 0;
         sd[0][0] = a; sd[0][1] = 0; sd[1][0] = 0; sd[1][1] = a;
         v->sprite_shift[0] = v->sprite_shift[1] = 0;
         break;
      case 1:
         so[0][0] = sprite_ref[0][0] - a * vop_ref[0][0];
         so[0][1] = sprite_ref[0][1] - a * vop_ref[0][1];
         so[1][0] = ((sprite_ref[0][0] >> 1) | (sprite_ref[0][0] & 1)) - a * (vop_ref[0][0] / 2);
         so[1][1] = ((sprite_ref[0][1] >> 1) | (sprite_ref[0][1] & 1)) - a * (vop_ref[0][1] / 2);
         sd[0][0] = a; sd[0][1] = 0; sd[1][0] = 0; sd[1][1] = a;
         v->sprite_shift[0] = v->sprite_shift[1] = 0;
         break;
      case 2:
         so[0][0] = (int64_t)sprite_ref[0][0] * (1 << (alpha + rho))
                  + ((int64_t)-r * sprite_ref[0][0] + virtual_ref[0][0]) * -vop_ref[0][0]
                  + ((int64_t) r * sprite_ref[0][1] - virtual_ref[0][1]) * -vop_ref[0][1]
                  + (1 << (alpha + rho - 1));
         so[0][1] = (int64_t)sprite_ref[0][1] * (1 << (alpha + rho))
                  + ((int64_t)-r * sprite_ref[0][1] + virtual_ref[0][1]) * -vop_ref[0][0]
                  + ((int64_t)-r * sprite_ref[0][0] + virtual_ref[0][0]) * -vop_ref[0][1]
                  + (1 << (alpha + rho - 1));
         so[1][0] = ((int64_t)-r * sprite_ref[0][0] + virtual_ref[0][0]) * (-2 * vop_ref[0][0] + 1)
                  + ((int64_t) r * sprite_ref[0][1] - virtual_ref[0][1]) * (-2 * vop_ref[0][1] + 1)
                  + 2 * (int64_t)w2 * r * sprite_ref[0][0] - 16 * w2 + (1 << (alpha + rho + 1));
         so[1][1] = ((int64_t)-r * sprite_ref[0][1] + virtual_ref[0][1]) * (-2 * vop_ref[0][0] + 1)
                  + ((int64_t)-r * sprite_ref[0][0] + virtual_ref[0][0]) * (-2 * vop_ref[0][1] + 1)
                  + 2 * (int64_t)w2 * r * sprite_ref[0][1] - 16 * w2 + (1 << (alpha + rho + 1));
         sd[0][0] = -r * sprite_ref[0][0] + virtual_ref[0][0];
         sd[0][1] =  r * sprite_ref[0][1] - virtual_ref[0][1];
         sd[1][0] = -r * sprite_ref[0][1] + virtual_ref[0][1];
         sd[1][1] = -r * sprite_ref[0][0] + virtual_ref[0][0];
         v->sprite_shift[0] = alpha + rho;
         v->sprite_shift[1] = alpha + rho + 2;
         break;
      default:
         min_ab = alpha < beta ? alpha : beta;
         w3 = w2 >> min_ab;
         h3 = h2 >> min_ab;
         so[0][0] = (int64_t)sprite_ref[0][0] * ((int64_t)1 << (alpha + beta + rho - min_ab))
                  + ((int64_t)-r * sprite_ref[0][0] + virtual_ref[0][0]) * h3 * -vop_ref[0][0]
                  + ((int64_t)-r * sprite_ref[0][0] + virtual_ref[1][0]) * w3 * -vop_ref[0][1]
                  + ((int64_t)1 << (alpha + beta + rho - min_ab - 1));
         so[0][1] = (int64_t)sprite_ref[0][1] * ((int64_t)1 << (alpha + beta + rho - min_ab))
                  + ((int64_t)-r * sprite_ref[0][1] + virtual_ref[0][1]) * h3 * -vop_ref[0][0]
                  + ((int64_t)-r * sprite_ref[0][1] + virtual_ref[1][1]) * w3 * -vop_ref[0][1]
                  + ((int64_t)1 << (alpha + beta + rho - min_ab - 1));
         so[1][0] = ((int64_t)-r * sprite_ref[0][0] + virtual_ref[0][0]) * h3 * (-2 * vop_ref[0][0] + 1)
                  + ((int64_t)-r * sprite_ref[0][0] + virtual_ref[1][0]) * w3 * (-2 * vop_ref[0][1] + 1)
                  + (int64_t)2 * w2 * h3 * r * sprite_ref[0][0] - 16 * w2 * h3
                  + ((int64_t)1 << (alpha + beta + rho - min_ab + 1));
         so[1][1] = ((int64_t)-r * sprite_ref[0][1] + virtual_ref[0][1]) * h3 * (-2 * vop_ref[0][0] + 1)
                  + ((int64_t)-r * sprite_ref[0][1] + virtual_ref[1][1]) * w3 * (-2 * vop_ref[0][1] + 1)
                  + (int64_t)2 * w2 * h3 * r * sprite_ref[0][1] - 16 * w2 * h3
                  + ((int64_t)1 << (alpha + beta + rho - min_ab + 1));
         sd[0][0] = ((int64_t)-r * sprite_ref[0][0] + virtual_ref[0][0]) * h3;
         sd[0][1] = ((int64_t)-r * sprite_ref[0][0] + virtual_ref[1][0]) * w3;
         sd[1][0] = ((int64_t)-r * sprite_ref[0][1] + virtual_ref[0][1]) * h3;
         sd[1][1] = ((int64_t)-r * sprite_ref[0][1] + virtual_ref[1][1]) * w3;
         v->sprite_shift[0] = alpha + beta + rho - min_ab;
         v->sprite_shift[1] = alpha + beta + rho - min_ab + 2;
         break;
   }

   /* A pure translation takes the one-point path */
   if (sd[0][0] == ((int64_t)a << v->sprite_shift[0]) && sd[0][1] == 0
         && sd[1][0] == 0 && sd[1][1] == ((int64_t)a << v->sprite_shift[0]))
   {
      so[0][0] >>= v->sprite_shift[0];
      so[0][1] >>= v->sprite_shift[0];
      so[1][0] >>= v->sprite_shift[1];
      so[1][1] >>= v->sprite_shift[1];
      sd[0][0] = a; sd[0][1] = 0; sd[1][0] = 0; sd[1][1] = a;
      v->sprite_shift[0] = v->sprite_shift[1] = 0;
      v->real_points = 1;
   }
   else
   {
      int sy = 16 - v->sprite_shift[0];
      int sc = 16 - v->sprite_shift[1];
      bool bad = sy < 0 || sc < 0;
      for (i = 0; i < 2 && !bad; i++)
      {
         int64_t lim_y = (int64_t)0x7FFFFFFF >> sy, lim_c = (int64_t)0x7FFFFFFF >> sc;
         if (     (so[0][i] < 0 ? -so[0][i] : so[0][i]) >= lim_y
               || (so[1][i] < 0 ? -so[1][i] : so[1][i]) >= lim_c
               || (sd[0][i] < 0 ? -sd[0][i] : sd[0][i]) >= lim_y
               || (sd[1][i] < 0 ? -sd[1][i] : sd[1][i]) >= lim_y)
            bad = true;
      }
      if (bad)
      {
         memset(v->sprite_offset, 0, sizeof(v->sprite_offset));
         memset(v->sprite_delta, 0, sizeof(v->sprite_delta));
         v->real_points = 1;
         return;
      }
      for (i = 0; i < 2; i++)
      {
         so[0][i] *= (int64_t)1 << sy;
         so[1][i] *= (int64_t)1 << sc;
         sd[0][i] *= (int64_t)1 << sy;
         sd[1][i] *= (int64_t)1 << sy;
         v->sprite_shift[i] = 16;
      }
      v->real_points = (int)v->sprite_points;
   }
   for (i = 0; i < 4; i++)
   {
      v->sprite_offset[i & 1][i >> 1] = (int)so[i & 1][i >> 1];
      v->sprite_delta[i & 1][i >> 1]  = (int)sd[i & 1][i >> 1];
   }
}

/* What a VOP header gave */
enum
{
   VOP_DECODE = 0,
   VOP_NOT_CODED,
   VOP_SKIP,
   VOP_BAD
};

/* video_object_plane() up to the macroblocks, 6.2.5 */
static int parse_vop(rmpeg4_video_t *v)
{
   m4_bits_t *b = &v->gb;
   unsigned   incr = 0, tinc;

   v->pict = get(b, 2) + 1;
   while (get1(b))
   {
      incr++;
      if (bits_over(b))
         return VOP_BAD;
   }
   skip(b, 1);
   tinc = get(b, v->tinc_bits);

   if (v->pict != VOP_B)
   {
      v->last_time_base   = v->time_base;
      v->time_base       += incr;
      v->time             = v->time_base * (int64_t)v->tinc_res + tinc;
      v->pp_time          = (int)(v->time - v->last_non_b_time);
      v->last_non_b_time  = v->time;
   }
   else
   {
      v->time    = (v->last_time_base + incr) * (int64_t)v->tinc_res + tinc;
      v->pb_time = v->pp_time - (int)(v->last_non_b_time - v->time);
   }
   skip(b, 1);
   if (!get1(b))
      return VOP_NOT_CODED;

   if (v->vol_unsupported || (v->pict == VOP_S && v->sprite_usage != SPRITE_GMC))
      return VOP_SKIP;

   if (v->pict == VOP_P || v->pict == VOP_S)
      v->rounding = get1(b);
   else
      v->rounding = 0;

   skip(b, v->cplx_i);
   if (v->pict != VOP_I)
      skip(b, v->cplx_p);
   if (v->pict == VOP_B)
      skip(b, v->cplx_b);

   v->dc_thr = rmpeg4_dc_threshold[get(b, 3)];
   v->top_field_first = false;
   v->alternate_scan  = false;
   if (v->interlaced)
   {
      v->top_field_first = get1(b) != 0;
      v->alternate_scan  = get1(b) != 0;
   }
   if (v->pict == VOP_S)
      parse_sprite(v);

   v->qp = get(b, v->quant_bits);
   if (!v->qp)
      return VOP_BAD;
   v->fcode = v->bcode = 1;
   if (v->pict != VOP_I && !(v->fcode = get(b, 3)))
      return VOP_BAD;
   if (v->pict == VOP_B && !(v->bcode = get(b, 3)))
      return VOP_BAD;
   if (bits_over(b))
      return VOP_BAD;

   if (v->pict == VOP_B)
   {
      /* temporal distances for direct mode, 7.6.9.5 */
      if (v->pp_time <= v->pb_time || v->pp_time <= v->pp_time - v->pb_time
            || v->pp_time <= 0)
         return VOP_SKIP;
      if (!v->t_frame)
         v->t_frame = v->pb_time;
      if (!v->t_frame)
         v->t_frame = 1;
#define RDIV64(n, q) ((n) >= 0 ? ((n) + (q) / 2) / (q) : ((n) - (q) / 2) / (q))
      v->pp_field_time = (int)(RDIV64(v->last_non_b_time, v->t_frame)
            - RDIV64(v->last_non_b_time - v->pp_time, v->t_frame)) * 2;
      v->pb_field_time = (int)(RDIV64(v->time, v->t_frame)
            - RDIV64(v->last_non_b_time - v->pp_time, v->t_frame)) * 2;
#undef RDIV64
      if (v->pp_field_time <= v->pb_field_time || v->pb_field_time <= 1)
      {
         v->pb_field_time = 2;
         v->pp_field_time = 4;
         if (v->interlaced)
            return VOP_SKIP;
      }
   }

   v->partitioned = v->data_partitioned && v->pict != VOP_B;
   return VOP_DECODE;
}

/* short_video_header picture, 6.2.5.1: H.263's picture layer. Returns
 * VOP_DECODE, VOP_SKIP or VOP_BAD */
static int parse_short_header(rmpeg4_video_t *v)
{
   static const uint16_t fmt[6][2] =
   { { 0, 0 }, { 128, 96 }, { 176, 144 }, { 352, 288 }, { 704, 576 }, { 1408, 1152 } };
   m4_bits_t *b = &v->gb;
   unsigned   tr, src;

   skip(b, 22);
   tr = get(b, 8);
   skip(b, 1 + 1 + 1 + 1 + 1);    /* marker, zero, split, camera, freeze */
   src = get(b, 3);
   if (src < 1 || src > 5)
      return VOP_BAD;
   if (!v->short_header || v->width != fmt[src][0] || v->height != fmt[src][1])
   {
      v->short_header = true;
      v->has_vol      = set_size(v, fmt[src][0], fmt[src][1]);
      if (!v->has_vol)
         return VOP_BAD;
   }
   v->vol_unsupported  = false;
   v->low_delay        = true;
   v->interlaced       = false;
   v->mpeg_quant       = false;
   v->quarter_sample   = false;
   v->resync           = false;
   v->data_partitioned = false;
   v->rvlc             = false;
   v->sprite_usage     = 0;
   v->quant_bits       = 5;
   v->tinc_res         = 30000;
   v->par_num          = 12;
   v->par_den          = 11;

   v->pict = get1(b) ? VOP_P : VOP_I;
   if (get(b, 4))                 /* UMV, SAC, AP, PB-frames: not here */
      return VOP_SKIP;
   v->qp = get(b, 5);
   if (!v->qp)
      return VOP_BAD;
   skip(b, 1);                    /* CPM, zero */
   while (get1(b))                /* PEI and PSPARE */
   {
      skip(b, 8);
      if (bits_over(b))
         return VOP_BAD;
   }

   v->time    += (int64_t)((tr - v->sh_tr) & 0xFF) * 1001;
   v->sh_tr    = tr;
   v->fcode    = 1;
   v->rounding = 0;
   v->dc_thr   = 99;
   v->top_field_first = v->alternate_scan = v->partitioned = false;
   return VOP_DECODE;
}


/* --------------------------------------------------------------------- */
/* Motion compensation                                                   */
/* --------------------------------------------------------------------- */

/* A reference plane as prediction reads it: rows r of the virtual plane
 * are rows r * rs + ro of the plane (rs 2 and ro the field for a field
 * prediction), and every sample past the VOP's edge is the nearest edge
 * sample - the unrestricted vectors of 7.6.4. */
typedef struct
{
   const uint8_t *p;
   unsigned       stride;
   int            w, h;      /* the decoded area: whole macroblocks */
   int            cw, ch;    /* the VOP's own size in this plane */
   unsigned       rs, ro;
} m4_ref_t;

/* (bw + 1) x (bh + 1) samples from (x, y): in place when inside, else
 * gathered into the edge buffer. Returns the samples and their stride. */
static const uint8_t *fetch(rmpeg4_video_t *v, const m4_ref_t *r,
      int x, int y, unsigned bw, unsigned bh, unsigned *stride)
{
   unsigned i, j;
   if (     x >= 0 && y >= 0 && x + (int)bw < r->w
         && (y + (int)bh) * (int)r->rs + (int)r->ro < r->h)
   {
      *stride = r->stride * r->rs;
      return r->p + ((size_t)y * r->rs + r->ro) * r->stride + x;
   }
   for (j = 0; j <= bh; j++)
   {
      int row = (y + (int)j) * (int)r->rs + (int)r->ro;
      const uint8_t *src;
      row = row < 0 ? 0 : row >= r->h ? r->h - 1 : row;
      src = r->p + (size_t)row * r->stride;
      for (i = 0; i <= bw; i++)
      {
         int col = x + (int)i;
         col = col < 0 ? 0 : col >= r->w ? r->w - 1 : col;
         v->emu[j * 32 + i] = src[col];
      }
   }
   *stride = 32;
   return v->emu;
}

/* Half sample interpolation, 7.6.2.1, with rounding_control @rnd */
static void hpel(uint8_t *dst, unsigned ds, const uint8_t *s, unsigned ss,
      unsigned bw, unsigned bh, unsigned dxy, unsigned rnd)
{
   unsigned x, y;
   for (y = 0; y < bh; y++, dst += ds, s += ss)
      switch (dxy)
      {
         case 0:
            memcpy(dst, s, bw);
            break;
         case 1:
            for (x = 0; x < bw; x++)
               dst[x] = (uint8_t)((s[x] + s[x + 1] + 1 - rnd) >> 1);
            break;
         case 2:
            for (x = 0; x < bw; x++)
               dst[x] = (uint8_t)((s[x] + s[x + ss] + 1 - rnd) >> 1);
            break;
         default:
            for (x = 0; x < bw; x++)
               dst[x] = (uint8_t)((s[x] + s[x + 1] + s[x + ss] + s[x + ss + 1]
                        + 2 - rnd) >> 2);
            break;
      }
}

static INLINE uint8_t clip8(int x)
{
   return (uint8_t)(x < 0 ? 0 : x > 255 ? 255 : x);
}

/* The 8-tap half sample filter of 7.6.2.2 over @n samples spaced @step,
 * the block's samples mirrored at its edges: n outputs from n + 1 inputs */
static void qpel_filter(uint8_t *dst, unsigned dstep, const uint8_t *s,
      unsigned step, unsigned n, unsigned rnd)
{
   int i;
   int last = (int)n;
   for (i = 0; i < (int)n; i++)
   {
      int k[8], j, sum;
      for (j = 0; j < 8; j++)
      {
         int idx = i - 3 + j;
         if (idx < 0)
            idx = -1 - idx;
         else if (idx > last)
            idx = 2 * last + 1 - idx;
         k[j] = s[(size_t)idx * step];
      }
      sum = 20 * (k[3] + k[4]) - 6 * (k[2] + k[5]) + 3 * (k[1] + k[6]) - (k[0] + k[7]);
      dst[(size_t)i * dstep] = clip8((sum + 16 - (int)rnd) >> 5);
   }
}

static void avg2(uint8_t *dst, unsigned ds, const uint8_t *a, unsigned as,
      const uint8_t *b, unsigned bs, unsigned w, unsigned h, unsigned rnd)
{
   unsigned x, y;
   for (y = 0; y < h; y++, dst += ds, a += as, b += bs)
      for (x = 0; x < w; x++)
         dst[x] = (uint8_t)((a[x] + b[x] + 1 - rnd) >> 1);
}

/* Quarter sample prediction of an n x n block (n 8 or 16) from its
 * (n + 1) x (n + 1) samples: the half sample filter horizontally, then
 * vertically, and the quarter positions as averages of the neighbours on
 * that grid, taken in that order */
static void qpel(rmpeg4_video_t *v, uint8_t *dst, unsigned ds,
      const uint8_t *s, unsigned ss, unsigned n, unsigned dxy, unsigned rnd)
{
   unsigned qx = dxy & 3, qy = dxy >> 2, y;
   uint8_t *hh = v->half_h, *hv = v->half_hv;

   if (!qy)
   {
      if (!qx)
      {
         for (y = 0; y < n; y++)
            memcpy(dst + y * ds, s + y * ss, n);
         return;
      }
      for (y = 0; y < n; y++)
         qpel_filter(hh + y * n, 1, s + y * ss, 1, n, rnd);
      if (qx == 2)
         for (y = 0; y < n; y++)
            memcpy(dst + y * ds, hh + y * n, n);
      else
         avg2(dst, ds, s + (qx == 3), ss, hh, n, n, n, rnd);
      return;
   }
   if (!qx)
   {
      for (y = 0; y < n; y++)
         qpel_filter(hv + y, n, s + y, ss, n, rnd);
      if (qy == 2)
         for (y = 0; y < n; y++)
            memcpy(dst + y * ds, hv + y * n, n);
      else
         avg2(dst, ds, s + (qy == 3 ? ss : 0), ss, hv, n, n, n, rnd);
      return;
   }
   for (y = 0; y <= n; y++)
      qpel_filter(hh + y * n, 1, s + y * ss, 1, n, rnd);
   if (qx != 2)
      avg2(hh, n, hh, n, s + (qx == 3), ss, n, n + 1, rnd);
   for (y = 0; y < n; y++)
      qpel_filter(hv + y, n, hh + y, n, n, rnd);
   if (qy == 2)
      for (y = 0; y < n; y++)
         memcpy(dst + y * ds, hv + y * n, n);
   else
      avg2(dst, ds, hh + (qy == 3 ? n : 0), n, hv, n, n, n, rnd);
}

/* A block's source position, for the 8x8 predictions of four vector
 * macroblocks: kept within @lo of the VOP's own size on the left and top
 * and at its edge on the right and bottom, where the fraction is dropped
 * - what the encoders' reconstruction does there */
static void clip_src(const m4_ref_t *r, int lo, int *sx, int *sy,
      unsigned *dxy, unsigned xmask, unsigned ymask)
{
   if (*sx < -lo)
      *sx = -lo;
   else if (*sx >= r->cw)
   {
      *sx   = r->cw;
      *dxy &= ~xmask;
   }
   if (*sy < -lo)
      *sy = -lo;
   else if (*sy >= r->ch)
   {
      *sy   = r->ch;
      *dxy &= ~ymask;
   }
}

/* One block, half sample: (x, y) in the virtual plane's samples, @mv in
 * half samples; @clip the bound of clip_src(), 0 for none */
static void pred_hpel(rmpeg4_video_t *v, const m4_ref_t *r, uint8_t *dst,
      unsigned ds, int x, int y, int mx, int my, unsigned bw, unsigned bh,
      unsigned rnd, int clip)
{
   unsigned ss, dxy = (unsigned)((mx & 1) | ((my & 1) << 1));
   int sx = x + (mx >> 1), sy = y + (my >> 1);
   const uint8_t *s;
   if (clip)
      clip_src(r, clip, &sx, &sy, &dxy, 1, 2);
   s = fetch(v, r, sx, sy, bw, bh, &ss);
   hpel(dst, ds, s, ss, bw, bh, dxy, rnd);
}

static void pred_qpel(rmpeg4_video_t *v, const m4_ref_t *r, uint8_t *dst,
      unsigned ds, int x, int y, int mx, int my, unsigned n, unsigned rnd,
      int clip)
{
   unsigned ss, dxy = (unsigned)((mx & 3) | ((my & 3) << 2));
   int sx = x + (mx >> 2), sy = y + (my >> 2);
   const uint8_t *s;
   if (clip)
      clip_src(r, clip, &sx, &sy, &dxy, 3, 12);
   s = fetch(v, r, sx, sy, n, n, &ss);
   qpel(v, dst, ds, s, ss, n, dxy, rnd);
}

/* The chroma vector of four luma vectors' sum, Table 7-9 */
static int chroma_4mv(int sum)
{
   static const uint8_t tab[16] = { 0, 0, 0, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1 };
   return tab[sum & 15] + (sum >> 3);
}

static void set_ref(m4_ref_t *r, const rmpeg4_video_t *v, int slot, int plane,
      unsigned rs, unsigned ro)
{
   r->p      = v->plane[slot][plane];
   r->stride = plane ? v->c_stride : v->y_stride;
   r->w      = (int)(plane ? v->mb_w * 8 : v->mb_w * 16);
   r->h      = (int)(plane ? v->mb_h * 8 : v->mb_h * 16);
   r->cw     = plane ? (int)v->width  >> 1 : (int)v->width;
   r->ch     = plane ? (int)v->height >> 1 : (int)v->height;
   r->rs     = rs;
   r->ro     = ro;
}

/* Global motion compensation of a macroblock, 7.8.7 */
static void gmc1(rmpeg4_video_t *v, int slot, unsigned mbx, unsigned mby,
      uint8_t *dy, uint8_t *dcb, uint8_t *dcr)
{
   int acc = (int)v->sprite_accuracy;
   int pl;
   for (pl = 0; pl < 3; pl++)
   {
      int      n   = pl ? 8 : 16;
      int      off = pl ? 1 : 0;
      int      mx  = v->sprite_offset[off][0];
      int      my  = v->sprite_offset[off][1];
      int      sx  = (int)mbx * n + (mx >> (acc + 1));
      int      sy  = (int)mby * n + (my >> (acc + 1));
      int      lim_w = pl ? (int)v->width >> 1 : (int)v->width;
      int      lim_h = pl ? (int)v->height >> 1 : (int)v->height;
      uint8_t *dst = pl == 0 ? dy : pl == 1 ? dcb : dcr;
      unsigned ds  = pl ? v->c_stride : v->y_stride;
      unsigned ss;
      const uint8_t *s;
      m4_ref_t r;

      mx *= 1 << (3 - acc);
      my *= 1 << (3 - acc);
      sx  = sx < -n ? -n : sx > lim_w ? lim_w : sx;
      if (sx == lim_w)
         mx = 0;
      sy  = sy < -n ? -n : sy > lim_h ? lim_h : sy;
      if (sy == lim_h)
         my = 0;
      set_ref(&r, v, slot, pl, 1, 0);
      s = fetch(v, &r, sx, sy, (unsigned)n, (unsigned)n, &ss);
      if (pl == 0 && !((mx | my) & 7))
         hpel(dst, ds, s, ss, 16, 16,
               (unsigned)(((mx >> 3) & 1) | ((my >> 2) & 2)), v->rounding);
      else
      {
         int x16 = mx & 15, y16 = my & 15, x, y;
         int A = (16 - x16) * (16 - y16), B = x16 * (16 - y16);
         int C = (16 - x16) * y16, D = x16 * y16;
         int rounder = 128 - (int)v->rounding;
         for (y = 0; y < n; y++)
            for (x = 0; x < n; x++)
               dst[y * ds + x] = (uint8_t)((A * s[y * ss + x] + B * s[y * ss + x + 1]
                     + C * s[(y + 1) * ss + x] + D * s[(y + 1) * ss + x + 1]
                     + rounder) >> 8);
      }
   }
}

static void gmc_plane(uint8_t *dst, unsigned stride_d, const uint8_t *src,
      unsigned stride, int h, int ox, int oy, int dxx, int dxy, int dyx,
      int dyy, int shift, int r, int width, int height)
{
   const int s = 1 << shift;
   int y;
   width--;
   height--;
   for (y = 0; y < h; y++)
   {
      int x, vx = ox, vy = oy;
      for (x = 0; x < 8; x++)
      {
         int src_x  = vx >> 16;
         int src_y  = vy >> 16;
         int frac_x = src_x & (s - 1);
         int frac_y = src_y & (s - 1);
         int idx;
         src_x >>= shift;
         src_y >>= shift;
         if ((unsigned)src_x < (unsigned)width)
         {
            if ((unsigned)src_y < (unsigned)height)
            {
               idx = src_x + src_y * (int)stride;
               dst[y * stride_d + x] = (uint8_t)(((src[idx] * (s - frac_x) + src[idx + 1] * frac_x) * (s - frac_y)
                     + (src[idx + stride] * (s - frac_x) + src[idx + stride + 1] * frac_x) * frac_y
                     + r) >> (shift * 2));
            }
            else
            {
               idx = src_x + (src_y < 0 ? 0 : src_y > height ? height : src_y) * (int)stride;
               dst[y * stride_d + x] = (uint8_t)(((src[idx] * (s - frac_x) + src[idx + 1] * frac_x) * s
                     + r) >> (shift * 2));
            }
         }
         else
         {
            if ((unsigned)src_y < (unsigned)height)
            {
               idx = (src_x < 0 ? 0 : src_x > width ? width : src_x) + src_y * (int)stride;
               dst[y * stride_d + x] = (uint8_t)(((src[idx] * (s - frac_y) + src[idx + stride] * frac_y) * s
                     + r) >> (shift * 2));
            }
            else
            {
               idx = (src_x < 0 ? 0 : src_x > width ? width : src_x)
                   + (src_y < 0 ? 0 : src_y > height ? height : src_y) * (int)stride;
               dst[y * stride_d + x] = src[idx];
            }
         }
         vx += dxx;
         vy += dyx;
      }
      ox += dxy;
      oy += dyy;
   }
}

static void gmc(rmpeg4_video_t *v, int slot, unsigned mbx, unsigned mby,
      uint8_t *dy, uint8_t *dcb, uint8_t *dcr)
{
   int a = (int)v->sprite_accuracy;
   int r = (1 << (2 * a + 1)) - (int)v->rounding;
   int ox, oy;
   const int (*d)[2] = (const int (*)[2])v->sprite_delta;

   if (v->real_points == 1)
   {
      gmc1(v, slot, mbx, mby, dy, dcb, dcr);
      return;
   }
   ox = v->sprite_offset[0][0] + d[0][0] * (int)mbx * 16 + d[0][1] * (int)mby * 16;
   oy = v->sprite_offset[0][1] + d[1][0] * (int)mbx * 16 + d[1][1] * (int)mby * 16;
   gmc_plane(dy, v->y_stride, v->plane[slot][0], v->y_stride, 16, ox, oy,
         d[0][0], d[0][1], d[1][0], d[1][1], a + 1, r, (int)v->mb_w * 16, (int)v->mb_h * 16);
   gmc_plane(dy + 8, v->y_stride, v->plane[slot][0], v->y_stride, 16,
         ox + d[0][0] * 8, oy + d[1][0] * 8,
         d[0][0], d[0][1], d[1][0], d[1][1], a + 1, r, (int)v->mb_w * 16, (int)v->mb_h * 16);
   ox = v->sprite_offset[1][0] + d[0][0] * (int)mbx * 8 + d[0][1] * (int)mby * 8;
   oy = v->sprite_offset[1][1] + d[1][0] * (int)mbx * 8 + d[1][1] * (int)mby * 8;
   gmc_plane(dcb, v->c_stride, v->plane[slot][1], v->c_stride, 8, ox, oy,
         d[0][0], d[0][1], d[1][0], d[1][1], a + 1, r,
         (int)v->mb_w * 8, (int)v->mb_h * 8);
   gmc_plane(dcr, v->c_stride, v->plane[slot][2], v->c_stride, 8, ox, oy,
         d[0][0], d[0][1], d[1][0], d[1][1], a + 1, r,
         (int)v->mb_w * 8, (int)v->mb_h * 8);
}

/* a >> b rounded to nearest, halves away from zero's left: (a + 2^(b-1))
 * >> b for positive a, one less added for the rest */
static int rshift(int a, int b)
{
   if (!b)
      return a;
   return a > 0 ? (a + (1 << (b - 1))) >> b : (a + (1 << (b - 1)) - 1) >> b;
}

/* The average vector of a GMC macroblock, for the vectors predicted from
 * it, 7.8.7.4 */
static int gmc_mv(const rmpeg4_video_t *v, unsigned mbx, unsigned mby, int n)
{
   int len = 1 << (v->fcode + 4);
   int a   = (int)v->sprite_accuracy;
   int q   = v->quarter_sample ? 1 : 0;
   int sum;

   if (v->real_points == 1)
   {
      sum = rshift(v->sprite_offset[0][n] * (1 << q), a);
   }
   else
   {
      int dx = v->sprite_delta[n][0], dy = v->sprite_delta[n][1];
      int shift = v->sprite_shift[0], x, y;
      int mb_v;
      int sh = a + 8 - q;
      if (n)
         dy -= 1 << (shift + a + 1);
      else
         dx -= 1 << (shift + a + 1);
      mb_v = (int)((unsigned)v->sprite_offset[0][n] + (unsigned)dx * mbx * 16u
            + (unsigned)dy * mby * 16u);
      sum = 0;
      for (y = 0; y < 16; y++)
      {
         int t = (int)((unsigned)mb_v + (unsigned)dy * (unsigned)y);
         for (x = 0; x < 16; x++)
         {
            sum += t >> shift;
            t   += dx;
         }
      }
      sum = rshift(sum, sh);
   }
   if (sum < -len)
      sum = -len;
   else if (sum >= len)
      sum = len - 1;
   return sum;
}

/* The prediction of a macroblock from one reference, into the planes at
 * dy/dcb/dcr (the picture, or a buffer of the same strides) */
static void predict_dir(rmpeg4_video_t *v, const m4_mb_t *m, int dir, int slot,
      unsigned mbx, unsigned mby, uint8_t *dy, uint8_t *dcb, uint8_t *dcr,
      unsigned ys, unsigned cs)
{
   unsigned rnd = v->rounding;
   int      x   = (int)mbx * 16, y = (int)mby * 16;
   int      i, f;
   m4_ref_t r;

   if (m->mcsel)
   {
      gmc(v, slot, mbx, mby, dy, dcb, dcr);
      return;
   }

   switch (m->mvtype)
   {
      case MV_16X16:
      {
         int mx = m->mv[dir][0][0], my = m->mv[dir][0][1], cx, cy;
         set_ref(&r, v, slot, 0, 1, 0);
         if (v->quarter_sample)
         {
            pred_qpel(v, &r, dy, ys, x, y, mx, my, 16, rnd, 0);
            cx = mx / 2;
            cy = my / 2;
            cx = (cx >> 1) | (cx & 1);
            cy = (cy >> 1) | (cy & 1);
         }
         else
         {
            pred_hpel(v, &r, dy, ys, x, y, mx, my, 16, 16, rnd, 0);
            cx = (mx >> 1) | (mx & 1);
            cy = (my >> 1) | (my & 1);
         }
         set_ref(&r, v, slot, 1, 1, 0);
         pred_hpel(v, &r, dcb, cs, x >> 1, y >> 1, cx, cy, 8, 8, rnd, 0);
         set_ref(&r, v, slot, 2, 1, 0);
         pred_hpel(v, &r, dcr, cs, x >> 1, y >> 1, cx, cy, 8, 8, rnd, 0);
         break;
      }
      case MV_8X8:
      {
         int sx = 0, sy = 0, cx, cy;
         set_ref(&r, v, slot, 0, 1, 0);
         for (i = 0; i < 4; i++)
         {
            int      mx = m->mv[dir][i][0], my = m->mv[dir][i][1];
            int      bx = x + (i & 1) * 8, by = y + (i >> 1) * 8;
            uint8_t *d  = dy + (i & 1) * 8 + (size_t)(i >> 1) * 8 * ys;
            if (v->quarter_sample)
            {
               pred_qpel(v, &r, d, ys, bx, by, mx, my, 8, rnd, 16);
               sx += mx / 2;
               sy += my / 2;
            }
            else
            {
               pred_hpel(v, &r, d, ys, bx, by, mx, my, 8, 8, rnd, 16);
               sx += mx;
               sy += my;
            }
         }
         cx = chroma_4mv(sx);
         cy = chroma_4mv(sy);
         set_ref(&r, v, slot, 1, 1, 0);
         pred_hpel(v, &r, dcb, cs, x >> 1, y >> 1, cx, cy, 8, 8, rnd, 8);
         set_ref(&r, v, slot, 2, 1, 0);
         pred_hpel(v, &r, dcr, cs, x >> 1, y >> 1, cx, cy, 8, 8, rnd, 8);
         break;
      }
      default:   /* two field vectors, a field each */
         for (f = 0; f < 2; f++)
         {
            int mx = m->mv[dir][f][0], my = m->mv[dir][f][1], cx, cy;
            unsigned sel = m->fsel[dir][f];
            int fy = (int)mby * 8;
            set_ref(&r, v, slot, 0, 2, sel);
            if (v->quarter_sample)
            {
               pred_qpel(v, &r, dy + (size_t)f * ys, 2 * ys, x, fy, mx, my, 8, rnd, 0);
               pred_qpel(v, &r, dy + (size_t)f * ys + 8, 2 * ys, x + 8, fy, mx, my, 8, rnd, 0);
               cx = mx / 2;
               cy = my >> 1;
               cx = (cx >> 1) | (cx & 1);
               cy = (cy >> 1) | (cy & 1);
            }
            else
            {
               pred_hpel(v, &r, dy + (size_t)f * ys, 2 * ys, x, fy, mx, my, 16, 8, rnd, 0);
               cx = (mx >> 1) | (mx & 1);
               cy = (my >> 1) | (my & 1);
            }
            set_ref(&r, v, slot, 1, 2, sel);
            pred_hpel(v, &r, dcb + (size_t)f * cs, 2 * cs, x >> 1, fy >> 1, cx, cy, 8, 4, rnd, 0);
            set_ref(&r, v, slot, 2, 2, sel);
            pred_hpel(v, &r, dcr + (size_t)f * cs, 2 * cs, x >> 1, fy >> 1, cx, cy, 8, 4, rnd, 0);
         }
         break;
   }
}

/* The macroblock's prediction into the picture: forward, backward, or
 * the rounded average of the two */
static void predict_mb(rmpeg4_video_t *v, const m4_mb_t *m, unsigned mbx,
      unsigned mby, int fwd, int bwd)
{
   unsigned ys = v->y_stride, cs = v->c_stride;
   uint8_t *dy  = v->plane[v->cur][0] + (size_t)mby * 16 * ys + mbx * 16;
   uint8_t *dcb = v->plane[v->cur][1] + (size_t)mby * 8 * cs + mbx * 8;
   uint8_t *dcr = v->plane[v->cur][2] + (size_t)mby * 8 * cs + mbx * 8;

   if (m->dirs & DIR_FWD)
   {
      predict_dir(v, m, 0, fwd, mbx, mby, dy, dcb, dcr, ys, cs);
      if (m->dirs & DIR_BWD)
      {
         uint8_t *py = v->pred, *pcb = v->pred + 256, *pcr = v->pred + 320;
         unsigned x, y;
         unsigned save = v->rounding;
         v->rounding = 0;
         predict_dir(v, m, 1, bwd, mbx, mby, py, pcb, pcr, 16, 8);
         v->rounding = save;
         for (y = 0; y < 16; y++)
            for (x = 0; x < 16; x++)
               dy[y * ys + x] = (uint8_t)((dy[y * ys + x] + py[y * 16 + x] + 1) >> 1);
         for (y = 0; y < 8; y++)
            for (x = 0; x < 8; x++)
            {
               dcb[y * cs + x] = (uint8_t)((dcb[y * cs + x] + pcb[y * 8 + x] + 1) >> 1);
               dcr[y * cs + x] = (uint8_t)((dcr[y * cs + x] + pcr[y * 8 + x] + 1) >> 1);
            }
      }
   }
   else
      predict_dir(v, m, 1, bwd, mbx, mby, dy, dcb, dcr, ys, cs);
}


/* --------------------------------------------------------------------- */
/* Intra DC and AC prediction, 7.4.3                                     */
/* --------------------------------------------------------------------- */

/* The intra state of block @n's neighbour (dx, dy) blocks away, or NULL
 * when it is outside the VOP, in an earlier video packet or not intra
 * in this VOP. *nb receives the neighbour's block number. */
static m4_intra_t *intra_nb(rmpeg4_video_t *v, unsigned mb, int n,
      int dx, int dy, int *nb)
{
   int mbx = (int)(mb % v->mb_w), mby = (int)(mb / v->mb_w);
   int idx;
   m4_intra_t *e;

   if (n < 4)
   {
      int bx = 2 * mbx + (n & 1) + dx, by = 2 * mby + (n >> 1) + dy;
      if (bx < 0 || by < 0)
         return NULL;
      mbx = bx >> 1;
      mby = by >> 1;
      *nb = (bx & 1) | ((by & 1) << 1);
   }
   else
   {
      mbx += dx;
      mby += dy;
      if (mbx < 0 || mby < 0)
         return NULL;
      *nb = n;
   }
   if (mbx >= (int)v->mb_w)
      return NULL;
   idx = mby * (int)v->mb_w + mbx;
   if (idx < (int)v->packet_start)
      return NULL;
   e = &v->intra[idx];
   return e->vop == v->serial ? e : NULL;
}

/* The DC predictor of block @n (the neighbour's reconstructed DC, 1024
 * when there is none) and its direction: 1 from above, 0 from the left */
static int dc_predict(rmpeg4_video_t *v, unsigned mb, int n, unsigned *dir)
{
   int nb;
   m4_intra_t *ea = intra_nb(v, mb, n, -1,  0, &nb);
   int a = ea ? ea->dc[nb] : 1024;
   m4_intra_t *eb = intra_nb(v, mb, n, -1, -1, &nb);
   int b = eb ? eb->dc[nb] : 1024;
   m4_intra_t *ec = intra_nb(v, mb, n,  0, -1, &nb);
   int c = ec ? ec->dc[nb] : 1024;

   if (abs(a - b) < abs(b - c))
   {
      *dir = 1;
      return c;
   }
   *dir = 0;
   return a;
}

static unsigned dc_scale(unsigned qp, int n)
{
   return n < 4 ? rmpeg4_dc_scale_lum[qp] : rmpeg4_dc_scale_chr[qp];
}

/* The quantised DC from its differential: kept, reconstructed, for the
 * blocks predicted from this one */
static int dc_level(rmpeg4_video_t *v, unsigned mb, int n, int pred, int diff,
      unsigned qp)
{
   int scale = (int)dc_scale(qp, n);
   int level = diff + (pred + (scale >> 1)) / scale;
   int rec   = level * scale;
   v->intra[mb].dc[n] = (int16_t)(rec < 0 ? 0 : rec > 2047 ? 2047 : rec);
   return level;
}

/* AC prediction of the quantised block, and the block's first row and
 * column kept for its neighbours */
static void ac_predict(rmpeg4_video_t *v, unsigned mb, int n, int16_t *blk,
      unsigned dir, bool ac_pred, unsigned qp)
{
   int16_t *own = v->intra[mb].ac[n];
   int i;

   if (ac_pred)
   {
      int nb;
      m4_intra_t *e = dir ? intra_nb(v, mb, n, 0, -1, &nb)
                          : intra_nb(v, mb, n, -1, 0, &nb);
      if (e)
      {
         const int16_t *src = e->ac[nb] + (dir ? 7 : 0);
         unsigned pq = e->qp;
         for (i = 1; i < 8; i++)
         {
            int p = src[i - 1];
            int pos = dir ? i : i * 8;
            if (pq != qp && e != &v->intra[mb])
            {
               int t = p * (int)pq;
               p = t > 0 ? (t + (int)(qp >> 1)) / (int)qp : (t - (int)(qp >> 1)) / (int)qp;
            }
            blk[pos] = (int16_t)(blk[pos] + p);
         }
      }
   }
   for (i = 1; i < 8; i++)
   {
      own[i - 1]     = blk[i * 8];
      own[7 + i - 1] = blk[i];
   }
}

/* --------------------------------------------------------------------- */
/* Blocks                                                                */
/* --------------------------------------------------------------------- */

static INLINE int sat(int x)
{
   return x < -2048 ? -2048 : x > 2047 ? 2047 : x;
}

/* Inverse quantisation of a block of quantised levels, 7.4.4 */
static void dequant(const rmpeg4_video_t *v, int16_t *blk, bool intra,
      unsigned qp, int n)
{
   int i;
   if (!v->mpeg_quant)
   {
      int qmul = (int)qp * 2, qadd = ((int)qp - 1) | 1;
      for (i = intra ? 1 : 0; i < 64; i++)
      {
         int l = blk[i];
         if (l)
            blk[i] = (int16_t)sat(l < 0 ? l * qmul - qadd : l * qmul + qadd);
      }
      if (intra)
         blk[0] = (int16_t)sat(blk[0] * (int)dc_scale(qp, n));
   }
   else
   {
      const uint8_t *w = intra ? v->intra_matrix : v->inter_matrix;
      int sum = 0;
      for (i = intra ? 1 : 0; i < 64; i++)
      {
         int l = blk[i], f;
         if (!l)
            continue;
         if (intra)
            f = (abs(l) * w[i] * (int)qp) >> 3;
         else
            f = ((2 * abs(l) + 1) * w[i] * (int)qp) >> 4;
         f = sat(l < 0 ? -f : f);
         blk[i] = (int16_t)f;
         sum += f;
      }
      if (intra)
      {
         blk[0] = (int16_t)sat(blk[0] * (int)dc_scale(qp, n));
         sum += blk[0];
      }
      if (!(sum & 1))       /* mismatch control */
         blk[63] ^= 1;
   }
}

/* The coefficients of a block, 7.4.1 and 7.4.2: from position @i on, in
 * @scan order, into @blk (zeroed by the caller). Levels come out
 * quantised. */
static bool tcoef(rmpeg4_video_t *v, int16_t *blk, int i, const uint8_t *scan,
      bool intra, bool rvlc)
{
   m4_bits_t *b = &v->gb;
   int t = rvlc ? (intra ? T_RV_INTRA : T_RV_INTER) : (intra ? T_TC_INTRA : T_TC_INTER);
   const uint8_t (*lmax)[64] = intra ? rmpeg4_intra_lmax : rmpeg4_inter_lmax;
   const uint8_t (*rmax)[64] = intra ? rmpeg4_intra_rmax : rmpeg4_inter_rmax;

   for (;;)
   {
      const rmpeg4_vlc_t *e = vlc(v, t);
      int last, run, level;

      if (!e)
         return false;
      if (e->c)
      {
         last  = e->a;
         run   = e->b;
         level = e->c;
         if (get1(b))
            level = -level;
      }
      else if (v->short_header)
      {
         last  = (int)get1(b);
         run   = (int)get(b, 6);
         level = (int)get(b, 8);
         if (level > 127)
            level -= 256;
         if (!level || level == -128)
            return false;
      }
      else if (rvlc)
      {
         if (!get1(b))
            return false;
         last  = (int)get1(b);
         run   = (int)get(b, 6);
         if (!get1(b))
            return false;
         level = (int)get(b, 11);
         if (get(b, 5) != 0x10)
            return false;
         if (get1(b))
            level = -level;
      }
      else if (!get1(b))         /* escape 1: level + LMAX */
      {
         if (!(e = vlc(v, t)) || !e->c)
            return false;
         last  = e->a;
         run   = e->b;
         level = e->c + lmax[last][run];
         if (get1(b))
            level = -level;
      }
      else if (!get1(b))         /* escape 2: run + RMAX + 1 */
      {
         if (!(e = vlc(v, t)) || !e->c)
            return false;
         last  = e->a;
         run   = e->b + rmax[last][e->c];
         level = e->c;
         if (get1(b))
            level = -level;
      }
      else                       /* escape 3: fixed length */
      {
         last  = (int)get1(b);
         run   = (int)get(b, 6);
         skip(b, 1);
         level = (int)get(b, 12);
         if (level > 2047)
            level -= 4096;
         skip(b, 1);
         if (!level)
            return false;
      }
      i += run;
      if (i > 63)
         return false;
      blk[scan[i]] = (int16_t)level;
      i++;
      if (last)
         return !bits_over(b);
   }
}

/* dct_dc_size and dct_dc_differential */
static bool dc_diff(rmpeg4_video_t *v, int n, int *diff)
{
   int size = vlc_value(v, n < 4 ? T_DC_LUM : T_DC_CHR);
   if (size < 0)
      return false;
   *diff = 0;
   if (size)
   {
      *diff = get_xbits(&v->gb, (unsigned)size);
      if (size > 8)
         skip(&v->gb, 1);       /* marker */
   }
   return true;
}

/* An intra block: @have_dc when its DC differential was read ahead (data
 * partitioning, @dc_in with @dir_in), else read here if the threshold
 * says so. Leaves the dequantised block in @blk. */
static bool intra_block(rmpeg4_video_t *v, unsigned mb, int n, int16_t *blk,
      bool coded, bool use_dc_vlc, bool ac_pred, unsigned qp,
      bool have_dc, int dc_in, unsigned dir_in)
{
   const uint8_t *scan;
   unsigned dir;
   int pred = 0, diff = 0;

   memset(blk, 0, 64 * sizeof(*blk));
   if (have_dc)
   {
      dir = dir_in;
      blk[0] = (int16_t)dc_in;
   }
   else
   {
      pred = dc_predict(v, mb, n, &dir);
      if (use_dc_vlc && !dc_diff(v, n, &diff))
         return false;
   }

   if (v->alternate_scan)
      scan = m4_alt_vertical;
   else if (ac_pred)
      scan = dir ? m4_alt_horizontal : m4_alt_vertical;
   else
      scan = m4_zigzag;

   if (coded && !tcoef(v, blk, use_dc_vlc || have_dc ? 1 : 0, scan, true, v->rvlc && v->partitioned))
      return false;
   if (!have_dc)
      blk[0] = (int16_t)dc_level(v, mb, n, pred, use_dc_vlc ? diff : blk[0], qp);
   ac_predict(v, mb, n, blk, dir, ac_pred, qp);
   dequant(v, blk, true, qp, n);
   return true;
}

/* An inter block, dequantised */
static bool inter_block(rmpeg4_video_t *v, int16_t *blk, unsigned qp)
{
   memset(blk, 0, 64 * sizeof(*blk));
   if (!tcoef(v, blk, 0, v->alternate_scan ? m4_alt_vertical : m4_zigzag,
            false, v->rvlc && v->partitioned))
      return false;
   dequant(v, blk, false, qp, 0);
   return true;
}

/* Short header blocks, H.263 5.4: an 8-bit intra DC and no prediction */
static bool sh_block(rmpeg4_video_t *v, int16_t *blk, bool coded, bool intra,
      unsigned qp)
{
   int i;
   memset(blk, 0, 64 * sizeof(*blk));
   if (intra)
   {
      int dc = (int)get(&v->gb, 8);
      if (dc == 0 || dc == 128)
         return false;
      blk[0] = (int16_t)(dc == 255 ? 128 : dc);
   }
   if (coded && !tcoef(v, blk, intra ? 1 : 0, m4_zigzag, false, false))
      return false;
   if (intra)
   {
      int qmul = (int)qp * 2, qadd = ((int)qp - 1) | 1;
      for (i = 1; i < 64; i++)
         if (blk[i])
            blk[i] = (int16_t)sat(blk[i] < 0 ? blk[i] * qmul - qadd : blk[i] * qmul + qadd);
      blk[0] = (int16_t)(blk[0] * 8);
   }
   else
      dequant(v, blk, false, qp, 0);
   return true;
}

/* The residual of the macroblock onto its prediction (or, intra, the
 * blocks themselves), frame or field DCT */
static void put_blocks(rmpeg4_video_t *v, unsigned mbx, unsigned mby,
      unsigned cbp, bool intra, bool field_dct)
{
   unsigned ys = v->y_stride, cs = v->c_stride;
   uint8_t *dy = v->plane[v->cur][0] + (size_t)mby * 16 * ys + mbx * 16;
   int i;

   for (i = 0; i < 6; i++)
   {
      uint8_t *d;
      unsigned s;
      if (!intra && !(cbp & (32 >> i)))
         continue;
      if (i < 4)
      {
         if (field_dct)
         {
            d = dy + (i & 1) * 8 + (size_t)(i >> 1) * ys;
            s = 2 * ys;
         }
         else
         {
            d = dy + (i & 1) * 8 + (size_t)(i >> 1) * 8 * ys;
            s = ys;
         }
      }
      else
      {
         d = v->plane[v->cur][i - 3] + (size_t)mby * 8 * cs + mbx * 8;
         s = cs;
      }
      if (intra)
         idct_block(v->blk[i], d, s);
      else
         idct_block_add(v->blk[i], d, s);
   }
}

/* --------------------------------------------------------------------- */
/* Vectors                                                               */
/* --------------------------------------------------------------------- */

static int median3(int a, int b, int c)
{
   if (a > b) { int t = a; a = b; b = t; }
   return c < a ? a : c > b ? b : c;
}

/* The predictor of block @blk's vector, 7.6.5: the median of its left,
 * above and above-right candidates, one outside the VOP or the video
 * packet taken as zero, two as the third */
static void mv_predict(const rmpeg4_video_t *v, unsigned mb, int blk,
      int *px, int *py)
{
   static const int8_t c_off[4] = { 2, 1, 1, -1 };
   int bx = 2 * (int)(mb % v->mb_w) + (blk & 1);
   int by = 2 * (int)(mb / v->mb_w) + (blk >> 1);
   int cx[3], cy[3], k, valid = 0, which = 0;
   int xs[3], ys_[3];

   cx[0] = bx - 1;            cy[0] = by;
   cx[1] = bx;                cy[1] = by - 1;
   cx[2] = bx + c_off[blk];   cy[2] = by - 1;

   for (k = 0; k < 3; k++)
   {
      xs[k] = ys_[k] = 0;
      if (cx[k] >= 0 && cy[k] >= 0 && cx[k] < 2 * (int)v->mb_w)
      {
         int idx = (cy[k] >> 1) * (int)v->mb_w + (cx[k] >> 1);
         if (idx >= (int)v->packet_start)
         {
            const m4_mbmv_t *e = &v->mvs[idx];
            int b = (cx[k] & 1) | ((cy[k] & 1) << 1);
            xs[k]  = e->mv[b][0];
            ys_[k] = e->mv[b][1];
            valid |= 1 << k;
            which  = k;
         }
      }
   }
   if (!valid)
      *px = *py = 0;
   else if (valid == 1 || valid == 2 || valid == 4)
   {
      *px = xs[which];
      *py = ys_[which];
   }
   else
   {
      *px = median3(xs[0], xs[1], xs[2]);
      *py = median3(ys_[0], ys_[1], ys_[2]);
   }
}

/* motion_code and motion_residual of one component, 7.6.3 */
static bool mv_decode(rmpeg4_video_t *v, int pred, unsigned f, int *out)
{
   int code = vlc_value(v, T_MVD), val, bits;
   unsigned shift = f - 1;

   if (code < 0)
      return false;
   if (!code)
   {
      *out = pred;
      return true;
   }
   val = code;
   if (get1(&v->gb))
      val = -val;
   if (shift)
   {
      int mag = ((abs(val) - 1) << shift) | (int)get(&v->gb, shift);
      val = val < 0 ? -(mag + 1) : mag + 1;
   }
   val += pred;
   bits = 5 + (int)f;
   val  = ((val + (1 << (bits - 1))) & ((1 << bits) - 1)) - (1 << (bits - 1));
   *out = val;
   return true;
}

/* The vectors the macroblock leaves for prediction and direct mode */
static void store_mvs(rmpeg4_video_t *v, unsigned mb, const m4_mb_t *m)
{
   m4_mbmv_t *e = &v->mvs[mb];
   int i;
   e->type = (uint8_t)m->type;
   if (m->type & MB_INTRA)
      memset(e->mv, 0, sizeof(e->mv));
   else if (m->mvtype == MV_8X8)
      for (i = 0; i < 4; i++)
      {
         e->mv[i][0] = (int16_t)m->mv[0][i][0];
         e->mv[i][1] = (int16_t)m->mv[0][i][1];
      }
   else
   {
      int x = m->mv[0][0][0], y = m->mv[0][0][1];
      if (m->mvtype == MV_FIELD)
      {
         x = m->mv[0][0][0] + m->mv[0][1][0];
         y = m->mv[0][0][1] + m->mv[0][1][1];
         x = (x >> 1) | (x & 1);
         for (i = 0; i < 2; i++)
         {
            e->fmv[i][0] = (int16_t)m->mv[0][i][0];
            e->fmv[i][1] = (int16_t)m->mv[0][i][1];
            e->fsel[i]   = (uint8_t)m->fsel[0][i];
         }
      }
      for (i = 0; i < 4; i++)
      {
         e->mv[i][0] = (int16_t)x;
         e->mv[i][1] = (int16_t)y;
      }
   }
}

/* A P-VOP's vectors after mcbpc: GMC, field, four or one */
static bool p_vectors(rmpeg4_video_t *v, unsigned mb, m4_mb_t *m, bool four,
      bool field)
{
   int px, py, i;
   m4_mbmv_t *e = &v->mvs[mb];

   m->dirs = DIR_FWD;
   if (m->mcsel)
   {
      m->mvtype = MV_16X16;
      m->mv[0][0][0] = gmc_mv(v, mb % v->mb_w, mb / v->mb_w, 0);
      m->mv[0][0][1] = gmc_mv(v, mb % v->mb_w, mb / v->mb_w, 1);
      return true;
   }
   if (four)
   {
      m->mvtype = MV_8X8;
      for (i = 0; i < 4; i++)
      {
         mv_predict(v, mb, i, &px, &py);
         if (!mv_decode(v, px, v->fcode, &m->mv[0][i][0])
               || !mv_decode(v, py, v->fcode, &m->mv[0][i][1]))
            return false;
         /* the next block predicts from this one */
         e->mv[i][0] = (int16_t)m->mv[0][i][0];
         e->mv[i][1] = (int16_t)m->mv[0][i][1];
      }
      return true;
   }
   mv_predict(v, mb, 0, &px, &py);
   if (field)
   {
      m->mvtype = MV_FIELD;
      for (i = 0; i < 2; i++)
         if (!mv_decode(v, px, v->fcode, &m->mv[0][i][0])
               || !mv_decode(v, py / 2, v->fcode, &m->mv[0][i][1]))
            return false;
      return true;
   }
   m->mvtype = MV_16X16;
   return mv_decode(v, px, v->fcode, &m->mv[0][0][0])
       && mv_decode(v, py, v->fcode, &m->mv[0][0][1]);
}

/* Direct mode vectors, 7.6.9.5: the co-located macroblock's of the
 * backward reference, scaled by the temporal distances, plus the delta */
static void direct_mvs(rmpeg4_video_t *v, unsigned mb, m4_mb_t *m, int dx, int dy)
{
   const m4_mbmv_t *c = &v->col[mb];
   int i;

   m->dirs = DIR_FWD | DIR_BWD;
   if (c->type & MB_FIELD)
   {
      m->mvtype = MV_FIELD;
      for (i = 0; i < 2; i++)
      {
         int sel = c->fsel[i];
         int tpp, tpb, k;
         if (v->top_field_first)
         {
            tpp = v->pp_field_time - sel + i;
            tpb = v->pb_field_time - sel + i;
         }
         else
         {
            tpp = v->pp_field_time + sel - i;
            tpb = v->pb_field_time + sel - i;
         }
         if (tpp <= 0)
            tpp = 1;
         m->fsel[0][i] = (unsigned)sel;
         m->fsel[1][i] = (unsigned)i;
         for (k = 0; k < 2; k++)
         {
            int p = c->fmv[i][k], d = k ? dy : dx;
            m->mv[0][i][k] = p * tpb / tpp + d;
            m->mv[1][i][k] = d ? m->mv[0][i][k] - p : p * (tpb - tpp) / tpp;
         }
      }
      return;
   }
   for (i = 0; i < 4; i++)
   {
      int k;
      for (k = 0; k < 2; k++)
      {
         int p = c->mv[i][k], d = k ? dy : dx;
         m->mv[0][i][k] = p * v->pb_time / v->pp_time + d;
         m->mv[1][i][k] = d ? m->mv[0][i][k] - p : p * (v->pb_time - v->pp_time) / v->pp_time;
      }
   }
   /* one vector unless the co-located macroblock had four; with quarter
    * samples the blocks are predicted as four regardless */
   m->mvtype = (c->type & MB_4MV) || v->quarter_sample ? MV_8X8 : MV_16X16;
}

/* --------------------------------------------------------------------- */
/* Macroblocks                                                           */
/* --------------------------------------------------------------------- */

static const int8_t m4_dquant[4] = { -1, -2, 1, 2 };

static void set_qp(rmpeg4_video_t *v, int q)
{
   int max = (1 << v->quant_bits) - 1;
   v->qp = (unsigned)(q < 1 ? 1 : q > max ? max : q);
}

/* The intra blocks of a macroblock and their reconstruction */
static bool intra_mb(rmpeg4_video_t *v, unsigned mb, m4_mb_t *m, bool use_dc_vlc)
{
   int i;
   m4_intra_t *e = &v->intra[mb];

   e->vop = v->serial;
   e->qp  = (uint8_t)v->qp;
   for (i = 0; i < 6; i++)
      if (!intra_block(v, mb, i, v->blk[i], (m->cbp & (32 >> i)) != 0,
               use_dc_vlc, m->ac_pred, v->qp, false, 0, 0))
         return false;
   put_blocks(v, mb % v->mb_w, mb / v->mb_w, 0x3F, true, m->field_dct);
   return true;
}

static bool inter_texture(rmpeg4_video_t *v, unsigned mb, const m4_mb_t *m)
{
   int i;
   for (i = 0; i < 6; i++)
      if ((m->cbp & (32 >> i)) && !inter_block(v, v->blk[i], v->qp))
         return false;
   put_blocks(v, mb % v->mb_w, mb / v->mb_w, m->cbp, false, m->field_dct);
   return true;
}

/* A macroblock of an I-VOP, or the intra part of one in a P-VOP */
static bool mb_intra_rest(rmpeg4_video_t *v, unsigned mb, m4_mb_t *m,
      unsigned cbpc, bool dquant)
{
   int cbpy;
   bool use_dc_vlc;

   m->type    = MB_INTRA;
   m->ac_pred = get1(&v->gb) != 0;
   if ((cbpy = vlc_value(v, T_CBPY)) < 0)
      return false;
   m->cbp = (cbpc & 3) | ((unsigned)cbpy << 2);
   use_dc_vlc = v->qp < v->dc_thr;
   if (dquant)
      set_qp(v, (int)v->qp + m4_dquant[get(&v->gb, 2)]);
   m->field_dct = v->interlaced && get1(&v->gb);
   store_mvs(v, mb, m);
   return intra_mb(v, mb, m, use_dc_vlc);
}

static bool mb_i(rmpeg4_video_t *v, unsigned mb)
{
   m4_mb_t *m = &v->mb;
   int cbpc;
   do
   {
      if ((cbpc = vlc_value(v, T_MCBPC_I)) < 0)
         return false;
   } while (cbpc == 8);
   memset(m, 0, sizeof(*m));
   return mb_intra_rest(v, mb, m, (unsigned)cbpc, (cbpc & 4) != 0);
}

/* A not coded macroblock of a P- or S-VOP */
static void mb_p_skip(rmpeg4_video_t *v, unsigned mb)
{
   m4_mb_t *m = &v->mb;
   memset(m, 0, sizeof(*m));
   m->dirs = DIR_FWD;
   if (v->pict == VOP_S)
   {
      m->type  = MB_GMC;
      m->mcsel = true;
      p_vectors(v, mb, m, false, false);
   }
   else
      m->type = MB_SKIP;
   store_mvs(v, mb, m);
   predict_mb(v, m, mb % v->mb_w, mb / v->mb_w, v->ref_new, -1);
}

static bool mb_p(rmpeg4_video_t *v, unsigned mb)
{
   m4_bits_t *b = &v->gb;
   m4_mb_t   *m = &v->mb;
   int cbpc, cbpy;
   bool field = false;

   do
   {
      if (get1(b))
      {
         mb_p_skip(v, mb);
         return true;
      }
      if ((cbpc = vlc_value(v, T_MCBPC_P)) < 0)
         return false;
   } while (cbpc == 20);

   memset(m, 0, sizeof(*m));
   if (cbpc & 4)
      return mb_intra_rest(v, mb, m, (unsigned)cbpc, (cbpc & 8) != 0);

   if (v->pict == VOP_S && !(cbpc & 16))
      m->mcsel = get1(b) != 0;
   if ((cbpy = vlc_value(v, T_CBPY)) < 0)
      return false;
   m->cbp = ((unsigned)cbpc & 3) | (((unsigned)cbpy ^ 15) << 2);
   if (cbpc & 8)
      set_qp(v, (int)v->qp + m4_dquant[get(b, 2)]);
   if (v->interlaced && m->cbp)
      m->field_dct = get1(b) != 0;
   if (v->interlaced && !(cbpc & 16) && !m->mcsel && get1(b))
   {
      field = true;
      m->fsel[0][0] = get1(b);
      m->fsel[0][1] = get1(b);
   }
   m->type = (cbpc & 16) ? MB_4MV : field ? MB_FIELD : m->mcsel ? MB_GMC : 0;
   if (!p_vectors(v, mb, m, (cbpc & 16) != 0, field))
      return false;
   store_mvs(v, mb, m);
   predict_mb(v, m, mb % v->mb_w, mb / v->mb_w, v->ref_new, -1);
   return inter_texture(v, mb, m);
}

static bool mb_b(rmpeg4_video_t *v, unsigned mb)
{
   m4_bits_t *b   = &v->gb;
   m4_mb_t   *m   = &v->mb;
   unsigned   mbx = mb % v->mb_w, mby = mb / v->mb_w;
   int        type;
   bool       field = false;
   int        i, d;

   memset(m, 0, sizeof(*m));
   if (mbx == 0)
      memset(v->last_mv, 0, sizeof(v->last_mv));

   /* skipped in the backward reference: skipped here, forward, no vector */
   if (v->col[mb].type & MB_SKIP)
   {
      m->dirs = DIR_FWD;
      predict_mb(v, m, mbx, mby, v->ref_old, v->ref_new);
      return true;
   }

   if (get1(b))                       /* modb '1': direct, no delta */
   {
      direct_mvs(v, mb, m, 0, 0);
      predict_mb(v, m, mbx, mby, v->ref_old, v->ref_new);
      return true;
   }
   {
      bool no_cbp = get1(b) != 0;
      if ((type = vlc_value(v, T_MB_TYPE_B)) < 0)
         return false;
      if (!no_cbp)
         m->cbp = get(b, 6);
   }
   if (type != 0 && m->cbp && get1(b))   /* dbquant */
      set_qp(v, (int)v->qp + (get1(b) ? 2 : -2));
   if (v->interlaced)
   {
      if (m->cbp)
         m->field_dct = get1(b) != 0;
      if (type != 0 && get1(b))
      {
         field = true;
         if (type != 2)
         {
            m->fsel[0][0] = get1(b);
            m->fsel[0][1] = get1(b);
         }
         if (type != 3)
         {
            m->fsel[1][0] = get1(b);
            m->fsel[1][1] = get1(b);
         }
      }
   }

   if (type == 0)
   {
      int dx, dy;
      if (!mv_decode(v, 0, 1, &dx) || !mv_decode(v, 0, 1, &dy))
         return false;
      direct_mvs(v, mb, m, dx, dy);
   }
   else
   {
      m->dirs   = type == 1 ? DIR_FWD | DIR_BWD : type == 2 ? DIR_BWD : DIR_FWD;
      m->mvtype = field ? MV_FIELD : MV_16X16;
      for (d = 0; d < 2; d++)
      {
         unsigned f = d ? v->bcode : v->fcode;
         if (!(m->dirs & (d ? DIR_BWD : DIR_FWD)))
            continue;
         if (field)
            for (i = 0; i < 2; i++)
            {
               if (!mv_decode(v, v->last_mv[d][i][0], f, &m->mv[d][i][0])
                     || !mv_decode(v, v->last_mv[d][i][1] / 2, f, &m->mv[d][i][1]))
                  return false;
               v->last_mv[d][i][0] = m->mv[d][i][0];
               v->last_mv[d][i][1] = m->mv[d][i][1] * 2;
            }
         else
         {
            if (!mv_decode(v, v->last_mv[d][0][0], f, &m->mv[d][0][0])
                  || !mv_decode(v, v->last_mv[d][0][1], f, &m->mv[d][0][1]))
               return false;
            v->last_mv[d][0][0] = v->last_mv[d][1][0] = m->mv[d][0][0];
            v->last_mv[d][0][1] = v->last_mv[d][1][1] = m->mv[d][0][1];
         }
      }
   }
   predict_mb(v, m, mbx, mby, v->ref_old, v->ref_new);
   return inter_texture(v, mb, m);
}

/* A short header macroblock, H.263 5.3 */
static bool mb_short(rmpeg4_video_t *v, unsigned mb)
{
   m4_bits_t *b = &v->gb;
   m4_mb_t   *m = &v->mb;
   int cbpc, cbpy, i;
   bool intra;

   do
   {
      if (v->pict == VOP_P && get1(b))
      {
         mb_p_skip(v, mb);
         return true;
      }
      cbpc = vlc_value(v, v->pict == VOP_P ? T_MCBPC_P : T_MCBPC_I);
      if (cbpc < 0)
         return false;
   } while (cbpc == (v->pict == VOP_P ? 20 : 8));

   memset(m, 0, sizeof(*m));
   intra = v->pict == VOP_I || (cbpc & 4);
   if (v->pict == VOP_P && (cbpc & 16))
      return false;                   /* no four vectors in H.263 baseline */
   if ((cbpy = vlc_value(v, T_CBPY)) < 0)
      return false;
   if (!intra)
      cbpy ^= 15;
   m->cbp = ((unsigned)cbpc & 3) | ((unsigned)cbpy << 2);
   if (v->pict == VOP_P ? (cbpc & 8) : (cbpc & 4))
      set_qp(v, (int)v->qp + m4_dquant[get(b, 2)]);
   if (intra)
   {
      m->type = MB_INTRA;
      store_mvs(v, mb, m);
      for (i = 0; i < 6; i++)
         if (!sh_block(v, v->blk[i], (m->cbp & (32 >> i)) != 0, true, v->qp))
            return false;
      put_blocks(v, mb % v->mb_w, mb / v->mb_w, 0x3F, true, false);
      return true;
   }
   if (!p_vectors(v, mb, m, false, false))
      return false;
   store_mvs(v, mb, m);
   predict_mb(v, m, mb % v->mb_w, mb / v->mb_w, v->ref_new, -1);
   for (i = 0; i < 6; i++)
      if ((m->cbp & (32 >> i)) && !sh_block(v, v->blk[i], true, false, v->qp))
         return false;
   put_blocks(v, mb % v->mb_w, mb / v->mb_w, m->cbp, false, false);
   return true;
}

/* --------------------------------------------------------------------- */
/* Data partitioning, 6.2.6.1                                            */
/* --------------------------------------------------------------------- */

#define DC_MARKER      0x6B001   /* 19 bits */
#define MOTION_MARKER  0x1F001   /* 17 bits */

/* The first partition from @mb: returns the macroblocks it holds, or -1 */
static int part_a(rmpeg4_video_t *v, unsigned mb)
{
   m4_bits_t *b = &v->gb;
   unsigned   n = 0;

   while (mb + n < v->mb_count)
   {
      unsigned   at = mb + n;
      m4_part_t *p  = &v->part[at];
      m4_mb_t   *m  = &v->mb;
      int        cbpc;

      memset(p, 0, sizeof(*p));
      if (v->pict == VOP_I)
      {
         int i;
         if (show(b, 19) == DC_MARKER)
            break;
         do
         {
            if ((cbpc = vlc_value(v, T_MCBPC_I)) < 0)
               return -1;
         } while (cbpc == 8);
         p->type = MB_INTRA;
         p->cbp  = (uint8_t)(cbpc & 3);
         p->dc_coded = v->qp < v->dc_thr;
         if (cbpc & 4)
            set_qp(v, (int)v->qp + m4_dquant[get(b, 2)]);
         p->qp = (uint8_t)v->qp;
         v->intra[at].vop = v->serial;
         v->intra[at].qp  = p->qp;
         memset(m, 0, sizeof(*m));
         m->type = MB_INTRA;
         store_mvs(v, at, m);
         if (p->dc_coded)
            for (i = 0; i < 6; i++)
            {
               int diff, pred;
               unsigned dir;
               if (!dc_diff(v, i, &diff))
                  return -1;
               pred      = dc_predict(v, at, i, &dir);
               p->dc[i]  = (int16_t)dc_level(v, at, i, pred, diff, p->qp);
               p->dirs  |= (uint8_t)(dir << i);
            }
      }
      else
      {
         if (show(b, 17) == MOTION_MARKER)
            break;
         if (get1(b))
         {
            p->type      = MB_SKIP;
            p->not_coded = 1;
            memset(m, 0, sizeof(*m));
            if (v->pict == VOP_S)
            {
               m->mcsel = true;
               p->mcsel = 1;
               p->type  = MB_GMC;
               p_vectors(v, at, m, false, false);
            }
            m->type = p->type;
            store_mvs(v, at, m);
            n++;
            continue;
         }
         if ((cbpc = vlc_value(v, T_MCBPC_P)) < 0)
            return -1;
         if (cbpc == 20)
            continue;
         p->cbp = (uint8_t)(cbpc & 3);
         p->ac_pred = (uint8_t)((cbpc & 8) ? 1 : 0);   /* dquant follows, kept here */
         memset(m, 0, sizeof(*m));
         if (cbpc & 4)
         {
            p->type = MB_INTRA;
            m->type = MB_INTRA;
            store_mvs(v, at, m);
         }
         else
         {
            if (v->pict == VOP_S && !(cbpc & 16))
               m->mcsel = get1(b) != 0;
            p->mcsel = m->mcsel;
            p->type  = (cbpc & 16) ? MB_4MV : m->mcsel ? MB_GMC : 0;
            if (!p_vectors(v, at, m, (cbpc & 16) != 0, false))
               return -1;
            m->type = p->type;
            store_mvs(v, at, m);
         }
      }
      n++;
      if (bits_over(b))
         return -1;
   }
   if (v->pict == VOP_I)
   {
      if (get(b, 19) != DC_MARKER)
         return -1;
   }
   else if (get(b, 17) != MOTION_MARKER)
      return -1;
   return (int)n;
}

/* The second partition: ac_pred, cbpy, dquant and the P-VOPs' intra DCs */
static bool part_b(rmpeg4_video_t *v, unsigned mb, unsigned n)
{
   m4_bits_t *b = &v->gb;
   unsigned   k;
   for (k = 0; k < n; k++)
   {
      unsigned   at = mb + k;
      m4_part_t *p  = &v->part[at];
      int        cbpy;

      if (v->pict == VOP_I)
      {
         p->ac_pred = (uint8_t)get1(b);
         if ((cbpy = vlc_value(v, T_CBPY)) < 0)
            return false;
         p->cbp |= (uint8_t)(cbpy << 2);
      }
      else if (p->type & MB_INTRA)
      {
         bool dq = p->ac_pred != 0;
         bool use_dc_vlc = v->qp < v->dc_thr;
         int  i;
         p->ac_pred = (uint8_t)get1(b);
         if ((cbpy = vlc_value(v, T_CBPY)) < 0)
            return false;
         p->cbp |= (uint8_t)(cbpy << 2);
         if (dq)
            set_qp(v, (int)v->qp + m4_dquant[get(b, 2)]);
         p->qp = (uint8_t)v->qp;
         v->intra[at].vop = v->serial;
         v->intra[at].qp  = p->qp;
         p->dc_coded = use_dc_vlc;
         if (use_dc_vlc)
            for (i = 0; i < 6; i++)
            {
               int diff, pred;
               unsigned dir;
               if (!dc_diff(v, i, &diff))
                  return false;
               pred      = dc_predict(v, at, i, &dir);
               p->dc[i]  = (int16_t)dc_level(v, at, i, pred, diff, p->qp);
               p->dirs  |= (uint8_t)(dir << i);
            }
      }
      else if (p->not_coded)
         p->qp = (uint8_t)v->qp;
      else
      {
         bool dq = p->ac_pred != 0;
         if ((cbpy = vlc_value(v, T_CBPY)) < 0)
            return false;
         p->cbp |= (uint8_t)((cbpy ^ 15) << 2);
         if (dq)
            set_qp(v, (int)v->qp + m4_dquant[get(b, 2)]);
         p->qp = (uint8_t)v->qp;
      }
      if (bits_over(b))
         return false;
   }
   return true;
}

/* The third partition's texture for macroblock @at, and its
 * reconstruction */
static bool part_c(rmpeg4_video_t *v, unsigned at)
{
   m4_part_t *p   = &v->part[at];
   m4_mb_t   *m   = &v->mb;
   unsigned   mbx = at % v->mb_w, mby = at / v->mb_w;
   int        i;

   v->qp = p->qp;
   if (p->type & MB_INTRA)
   {
      bool have_dc = p->dc_coded != 0;
      for (i = 0; i < 6; i++)
      {
         unsigned dir = (p->dirs >> i) & 1;
         int      dc  = p->dc[i];
         if (!have_dc)
         {
            /* DC coded with the AC: predicted now */
            if (!intra_block(v, at, i, v->blk[i], (p->cbp & (32 >> i)) != 0,
                     false, p->ac_pred != 0, p->qp, false, 0, 0))
               return false;
         }
         else if (!intra_block(v, at, i, v->blk[i], (p->cbp & (32 >> i)) != 0,
                     false, p->ac_pred != 0, p->qp, true, dc, dir))
            return false;
      }
      put_blocks(v, mbx, mby, 0x3F, true, false);
      return true;
   }

   /* the vectors are where partition A left them */
   memset(m, 0, sizeof(*m));
   m->dirs = DIR_FWD;
   m->cbp  = p->cbp;
   if (p->type & MB_4MV)
   {
      m->mvtype = MV_8X8;
      for (i = 0; i < 4; i++)
      {
         m->mv[0][i][0] = v->mvs[at].mv[i][0];
         m->mv[0][i][1] = v->mvs[at].mv[i][1];
      }
   }
   else
   {
      m->mvtype      = MV_16X16;
      m->mcsel       = p->mcsel != 0;
      m->mv[0][0][0] = v->mvs[at].mv[0][0];
      m->mv[0][0][1] = v->mvs[at].mv[0][1];
   }
   predict_mb(v, m, mbx, mby, v->ref_new, -1);
   if (p->not_coded)
      return true;
   return inter_texture(v, at, m);
}


/* --------------------------------------------------------------------- */
/* Video packets, 6.2.5.2                                                */
/* --------------------------------------------------------------------- */

static unsigned resync_len(const rmpeg4_video_t *v)
{
   unsigned m;
   switch (v->pict)
   {
      case VOP_I: return 16;
      case VOP_B:
         m = v->fcode > v->bcode ? v->fcode : v->bcode;
         return 15 + (m > 2 ? m : 2);
      default:    return 15 + v->fcode;
   }
}

/* At a resync marker, after the stuffing that byte-aligns it (and any
 * macroblock stuffing before that)? Leaves the reader on the marker. */
static bool at_resync(rmpeg4_video_t *v)
{
   m4_bits_t b = v->gb;
   unsigned  n, z;

   if (v->pict != VOP_B && !v->partitioned)
   {
      unsigned step = v->pict == VOP_I ? 9 : 10;
      while (show(&b, step) == 1)
         skip(&b, step);
   }
   n = 8 - (unsigned)(b.pos & 7);
   if (show(&b, n) != (1u << (n - 1)) - 1)
      return false;
   skip(&b, n);
   for (z = 0; z < 32 && !show(&b, 1); z++)
      skip(&b, 1);
   if (z != resync_len(v) || bits_left(&b) <= 0)
      return false;
   b.pos -= z;
   v->gb = b;
   return true;
}

/* video_packet_header() from its marker: the first macroblock, or -1 */
static int packet_header(rmpeg4_video_t *v)
{
   m4_bits_t *b = &v->gb;
   unsigned   mb, q;

   skip(b, resync_len(v) + 1);
   mb = get(b, v->mb_num_bits);
   if (!mb || mb >= v->mb_count)
      return -1;
   q = get(b, v->quant_bits);
   if (q)
      v->qp = q;
   if (get1(b))                       /* header_extension_code */
   {
      while (get1(b))
         if (bits_over(b))
            return -1;
      skip(b, 1);
      skip(b, v->tinc_bits);
      skip(b, 1);
      skip(b, 2);                     /* vop_coding_type */
      skip(b, 3);                     /* intra_dc_vlc_thr */
      if (v->pict == VOP_S)
         parse_sprite(v);
      if (v->pict != VOP_I)
         skip(b, 3);
      if (v->pict == VOP_B)
         skip(b, 3);
   }
   if (bits_over(b))
      return -1;
   return (int)mb;
}

/* After a broken packet: the next resync marker's packet, or -1 */
static int seek_packet(rmpeg4_video_t *v)
{
   m4_bits_t *b   = &v->gb;
   unsigned   len = resync_len(v);
   size_t     i;

   align(b);
   for (i = b->pos >> 3; i + 3 <= b->bytes; i++)
   {
      if (b->p[i] || b->p[i + 1])
         continue;
      if (b->p[i + 2] == 1)
         return -1;                   /* a start code: the VOP is over */
      b->pos = i * 8;
      if (show(b, len + 1) == 1)
         return packet_header(v);
   }
   return -1;
}

/* A macroblock no packet reached: the forward reference's, unmoved */
static void conceal(rmpeg4_video_t *v, unsigned mb)
{
   m4_mb_t *m = &v->mb;
   int ref = v->pict == VOP_B ? v->ref_old : v->ref_new;
   memset(m, 0, sizeof(*m));
   if (v->pict != VOP_B)
      store_mvs(v, mb, m);
   if (ref < 0)
      return;
   m->dirs = DIR_FWD;
   {
      unsigned save = v->rounding;
      v->rounding = 0;
      predict_mb(v, m, mb % v->mb_w, mb / v->mb_w, ref, -1);
      v->rounding = save;
   }
}

static bool decode_mb(rmpeg4_video_t *v, unsigned mb)
{
   if (v->short_header)
      return mb_short(v, mb) && !bits_over(&v->gb);
   switch (v->pict)
   {
      case VOP_I: return mb_i(v, mb) && !bits_over(&v->gb);
      case VOP_B: return mb_b(v, mb) && !bits_over(&v->gb);
      default:    return mb_p(v, mb) && !bits_over(&v->gb);
   }
}

/* A new video packet at @mb: what predicts across it starts over */
static void new_packet(rmpeg4_video_t *v, unsigned mb)
{
   v->packet_start = mb;
   memset(v->last_mv, 0, sizeof(v->last_mv));
}

/* The short header's GOB headers, H.263 5.2: a GOB with one starts a new
 * prediction boundary; stuffing may align it */
static void sh_gob(rmpeg4_video_t *v, unsigned mb)
{
   unsigned rows = v->height <= 288 ? 1 : v->height <= 576 ? 2 : 4;
   m4_bits_t b = v->gb;
   unsigned  k;

   if (mb % (v->mb_w * rows))
      return;
   for (k = 0; k < 8; k++)
   {
      if (show(&b, 17) == 1)
      {
         skip(&b, 17);
         skip(&b, 5 + 2);            /* gob_number, gob_frame_id */
         v->qp = get(&b, 5);
         if (!v->qp)
            v->qp = 1;
         v->gb = b;
         new_packet(v, mb);
         return;
      }
      if ((b.pos & 7) == 0 || show(&b, 1))
         return;
      skip(&b, 1);
   }
}

static void decode_mbs(rmpeg4_video_t *v)
{
   unsigned mb = 0;
   new_packet(v, 0);

   while (mb < v->mb_count)
   {
      int next = -1;

      if (v->partitioned)
      {
         int n;
         unsigned k;
         if (mb && !(at_resync(v) && (next = packet_header(v)) >= 0))
            goto broken;
         if (mb)
         {
            while (mb < (unsigned)next)
               conceal(v, mb++);
            new_packet(v, mb);
         }
         if ((n = part_a(v, mb)) <= 0 || !part_b(v, mb, (unsigned)n))
            goto broken;
         for (k = 0; k < (unsigned)n; k++)
            if (!part_c(v, mb + k) || bits_over(&v->gb))
               goto broken;
         mb += (unsigned)n;
         continue;
      }

      if (v->short_header)
         sh_gob(v, mb);
      else if (v->resync && mb && at_resync(v))
      {
         if ((next = packet_header(v)) < 0 || (unsigned)next < mb)
            goto broken;
         while (mb < (unsigned)next)
         {
            /* only macroblocks skipped with the reference's may be left out */
            if (v->pict == VOP_B && (v->col[mb].type & MB_SKIP))
               decode_mb(v, mb);
            else
               conceal(v, mb);
            mb++;
         }
         new_packet(v, mb);
      }
      if (decode_mb(v, mb))
      {
         mb++;
         continue;
      }
broken:
      v->errors++;
      next = v->resync || v->partitioned ? seek_packet(v) : -1;
      if (next < 0 || (unsigned)next < mb)
         next = (int)v->mb_count;
      while (mb < (unsigned)next)
         conceal(v, mb++);
      if (mb < v->mb_count)
      {
         new_packet(v, mb);
         if (v->partitioned)
         {
            /* the header is read: the partitions follow */
            int n;
            unsigned k;
            if ((n = part_a(v, mb)) <= 0 || !part_b(v, mb, (unsigned)n))
               goto broken;
            for (k = 0; k < (unsigned)n; k++)
               if (!part_c(v, mb + k))
                  goto broken;
            mb += (unsigned)n;
         }
      }
   }
}

/* --------------------------------------------------------------------- */
/* VOPs and frames                                                       */
/* --------------------------------------------------------------------- */

static void fill_frame(const rmpeg4_video_t *v, int slot, rmpeg4_video_frame_t *out)
{
   out->y               = v->plane[slot][0];
   out->cb              = v->plane[slot][1];
   out->cr              = v->plane[slot][2];
   out->width           = v->width;
   out->height          = v->height;
   out->y_stride        = v->y_stride;
   out->c_stride        = v->c_stride;
   out->time            = v->slot_time[slot];
   out->tag             = v->slot_tag[slot];
   out->coding_type     = v->slot_type[slot];
   out->interlaced      = v->interlaced;
   out->top_field_first = (v->slot_type[slot] >> 4) & 1;
   out->coding_type    &= 15;
}

/* Decodes the VOP the reader is on. Returns 1 with *out filled when a
 * frame is ready. */
static int decode_vop(rmpeg4_video_t *v, bool sh, rmpeg4_video_frame_t *out)
{
   int r = sh ? parse_short_header(v) : parse_vop(v);
   int slot;

   if (r == VOP_NOT_CODED)
      return 0;
   if (r == VOP_BAD)
   {
      v->errors++;
      return 0;
   }
   if (r == VOP_SKIP || !v->mem
         || (v->pict == VOP_B && (v->ref_old < 0 || v->ref_new < 0))
         || (v->pict != VOP_I && v->ref_new < 0))
   {
      v->skipped++;
      return 0;
   }

   if (v->pict == VOP_B)
      slot = 3 - (v->ref_old + v->ref_new);  /* the slot neither uses */
   else
   {
      slot = v->ref_old >= 0 ? v->ref_old : (v->ref_new == 0 ? 1 : 0);
   }
   v->cur = slot;
   v->serial++;
   decode_mbs(v);

   v->slot_type[slot] = (uint8_t)(v->pict | (v->top_field_first ? 16 : 0));
   v->slot_time[slot] = (uint32_t)v->time;
   v->slot_tag[slot]  = v->tag;

   if (v->pict == VOP_B)
   {
      fill_frame(v, slot, out);
      return 1;
   }

   /* a new reference: its vectors become direct mode's */
   {
      m4_mbmv_t *t = v->col;
      v->col = v->mvs;
      v->mvs = t;
      if (v->pict == VOP_I)
      {
         unsigned i;
         for (i = 0; i < v->mb_count; i++)
            v->col[i].type = MB_INTRA;
      }
   }
   v->ref_old = v->ref_new;
   v->ref_new = slot;

   if (v->low_delay)
   {
      fill_frame(v, slot, out);
      return 1;
   }
   r = 0;
   if (v->held >= 0)
   {
      fill_frame(v, v->held, out);
      r = 1;
   }
   v->held = slot;
   return r;
}

rmpeg4_video_t *rmpeg4_video_init(void)
{
   rmpeg4_video_t *v = (rmpeg4_video_t*)calloc(1, sizeof(*v));
   if (!v)
      return NULL;
   build_luts(v);
   v->ref_old = v->ref_new = v->held = -1;
   return v;
}

void rmpeg4_video_free(rmpeg4_video_t *v)
{
   if (!v)
      return;
   free(v->mem);
   free(v);
}

void rmpeg4_video_reset(rmpeg4_video_t *v)
{
   if (!v)
      return;
   v->ref_old = v->ref_new = v->held = -1;
   v->time_base = v->last_time_base = v->time = v->last_non_b_time = 0;
   v->pp_time = v->pb_time = 0;
}

void rmpeg4_video_set_tag(rmpeg4_video_t *v, uint64_t tag)
{
   if (v)
      v->tag = tag;
}

int rmpeg4_video_flush(rmpeg4_video_t *v, rmpeg4_video_frame_t *out)
{
   if (!v || v->held < 0)
      return 0;
   fill_frame(v, v->held, out);
   v->held = -1;
   return 1;
}

/* The start code (or short header picture start) at or after @from,
 * byte aligned: its offset, or @len */
static size_t next_unit(const uint8_t *p, size_t from, size_t len, bool sh)
{
   size_t i;
   for (i = from; i + 3 <= len; i++)
   {
      if (p[i] || p[i + 1])
         continue;
      if (p[i + 2] == 1)
         return i;
      if (sh && ((p[i + 2] & 0xFC) == 0x80 || (p[i + 2] & 0xFC) == 0xFC))
         return i;
   }
   return len;
}

int rmpeg4_video_decode(rmpeg4_video_t *v, const uint8_t *data, size_t len,
      size_t *used, rmpeg4_video_frame_t *out)
{
   size_t at = 0;

   *used = len;
   if (!v || !data)
      return 0;

   while (at + 3 < len)
   {
      size_t   start = next_unit(data, at, len, true), end;
      unsigned code;
      bool     sh;
      int      got = 0;

      if (start + 3 >= len)
         break;
      sh   = data[start + 2] != 1;
      if (sh && (data[start + 2] & 0xFC) != 0x80)
      {
         at = start + 3;                /* short_video_end_marker */
         continue;
      }
      code = sh ? 0 : data[start + 3];
      end  = next_unit(data, start + (sh ? 3 : 4), len, sh || v->short_header);

      v->gb.p     = data + start;
      v->gb.bytes = end - start;
      v->gb.pos   = sh ? 0 : 32;

      if (sh)
         got = decode_vop(v, true, out);
      else if (code >= SC_VOL_LO && code <= SC_VOL_HI)
         parse_vol(v);
      else if (code == SC_VOP && v->has_vol && !v->short_header)
         got = decode_vop(v, false, out);
      at = end;
      if (got)
      {
         *used = at;
         return 1;
      }
   }
   return 0;
}

bool rmpeg4_video_has_vol(const rmpeg4_video_t *v)   { return v && v->has_vol; }
unsigned rmpeg4_video_width(const rmpeg4_video_t *v)  { return v ? v->width : 0; }
unsigned rmpeg4_video_height(const rmpeg4_video_t *v) { return v ? v->height : 0; }

unsigned rmpeg4_video_time_resolution(const rmpeg4_video_t *v)
{
   return v ? v->tinc_res : 0;
}

void rmpeg4_video_pixel_aspect(const rmpeg4_video_t *v, unsigned *num, unsigned *den)
{
   *num = v && v->par_num ? v->par_num : 1;
   *den = v && v->par_den ? v->par_den : 1;
}

uint32_t rmpeg4_video_skipped(const rmpeg4_video_t *v) { return v ? v->skipped : 0; }
uint32_t rmpeg4_video_errors(const rmpeg4_video_t *v)  { return v ? v->errors : 0; }
