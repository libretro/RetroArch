/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_DXVA_HEVC_H
#define RARCH_DXVA_HEVC_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <boolean.h>
#include <retro_inline.h>

#define DXVA_HEVC_PP_SIZE 232
#define DXVA_HEVC_QM_SIZE 1000
#define DXVA_HEVC_SLICE_SIZE 10
#define DXVA_HEVC_MAX_REFS 15

/* Initial Main/Main10 submission scope: 4:2:0, no PCM, tiles or dependent
 * slices. Flag words follow DXVA_PicParams_HEVC, with reserved bits zero.
 * Reference-set entries index refs, not texture slots. Scaling lists are
 * effective coefficients in DXVA scan order; defaults are resolved upstream. */
typedef struct dxva_hevc_picture
{
   uint32_t coding_tools, coding_settings;
   int32_t poc;
   unsigned width, height, min_cb_log2, ctb_log2;
   unsigned min_tb_log2, max_tb_log2, bit_depth, max_poc_lsb_log2;
   unsigned max_dec_pic_buffering, max_reorder_pics;
   unsigned hierarchy_inter, hierarchy_intra, num_st_rps, num_lt_rps;
   unsigned default_l0, default_l1, delta_pocs, rps_bits, slot;
   unsigned diff_qp_depth, parallel_merge_log2;
   int init_qp, cb_qp, cr_qp, beta_offset_div2, tc_offset_div2;
   unsigned no_bipred;
   unsigned num_refs;
   struct { int32_t poc; unsigned slot, long_term; } refs[15];
   unsigned num_before, num_after, num_longterm;
   uint8_t before[8], after[8], longterm[8];
} dxva_hevc_picture_t;

typedef struct dxva_hevc_scaling
{
   uint8_t list4[6][16], list8[6][64], list16[6][64], list32[2][64];
   uint8_t dc16[6], dc32[2];
} dxva_hevc_scaling_t;

static INLINE void dxva_hevc_put16(uint8_t *p, unsigned n)
{
   p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8);
}
static INLINE void dxva_hevc_put32(uint8_t *p, uint32_t n)
{
   p[0] = (uint8_t)n; p[1] = (uint8_t)(n >> 8);
   p[2] = (uint8_t)(n >> 16); p[3] = (uint8_t)(n >> 24);
}

/* Writes packed little-endian bytes without SDK headers, allocation or
 * reading pixels. Output must not overlap inputs. Failure clears the fixed output span. Feedback is nonzero. */
static INLINE bool dxva_hevc_pic_params(uint8_t *out,
      const dxva_hevc_picture_t *p, uint32_t feedback)
{
   unsigned i, j, bits, seen = 0;
   const uint8_t *sets[3];
   unsigned counts[3];
   if (!out) return false;
   memset(out, 0, DXVA_HEVC_PP_SIZE);
   if (!p || !feedback || (p->bit_depth != 8 && p->bit_depth != 10)
         || p->min_cb_log2 < 3 || p->min_cb_log2 > 6
         || p->ctb_log2 < p->min_cb_log2 || p->ctb_log2 > 6
         || p->min_tb_log2 < 2 || p->min_tb_log2 > 5
         || p->max_tb_log2 < p->min_tb_log2 || p->max_tb_log2 > 5 || p->max_tb_log2 > p->ctb_log2
         || !p->width || !p->height
         || (p->width & ((1u << p->min_cb_log2) - 1))
         || (p->height & ((1u << p->min_cb_log2) - 1))
         || (p->width >> p->min_cb_log2) > 65535
         || (p->height >> p->min_cb_log2) > 65535
         || p->max_poc_lsb_log2 < 4 || p->max_poc_lsb_log2 > 16
         || !p->max_dec_pic_buffering || p->max_dec_pic_buffering > 16
         || p->max_reorder_pics >= p->max_dec_pic_buffering
         || p->hierarchy_inter > p->ctb_log2 - p->min_tb_log2
         || p->hierarchy_intra > p->ctb_log2 - p->min_tb_log2
         || p->num_st_rps > 64 || p->num_lt_rps > 32
         || (p->num_lt_rps && !(p->coding_tools & (1u << 17)))
         || !p->default_l0 || p->default_l0 > 15 || !p->default_l1 || p->default_l1 > 15
         || p->delta_pocs > 255 || p->rps_bits > 65535 || p->slot >= 127
         || p->num_refs > 15 || p->num_refs >= p->max_dec_pic_buffering
         || p->num_before > 8 || p->num_after > 8 || p->num_longterm > 8
         || p->diff_qp_depth > p->ctb_log2 - p->min_cb_log2
         || p->parallel_merge_log2 < 2 || p->parallel_merge_log2 > p->ctb_log2
         || p->init_qp < -(int)(6 * (p->bit_depth - 8)) || p->init_qp > 51
         || p->cb_qp < -12 || p->cb_qp > 12 || p->cr_qp < -12 || p->cr_qp > 12
         || p->beta_offset_div2 < -6 || p->beta_offset_div2 > 6
         || p->tc_offset_div2 < -6 || p->tc_offset_div2 > 6 || p->no_bipred > 1
         || (p->coding_tools & ~((UINT32_C(0x3ff) << 17 & ~(UINT32_C(1) << 20)) | 7u))
         || (p->coding_settings & ~((UINT32_C(0x7ffff)) & ~((1u << 7) | (1u << 9) | (1u << 10)))))
      return false;
   if ((p->coding_settings & (1u << 17)) &&
         (!(p->coding_settings & (1u << 16)) || !(p->coding_settings & (1u << 18)) || p->num_refs))
      return false;
   for (i = 0; i < p->num_refs; i++)
   {
      if (p->refs[i].slot >= 127 || p->refs[i].slot == p->slot || p->refs[i].long_term > 1
            || (p->refs[i].long_term && !(p->coding_tools & (1u << 17))))
         return false;
      for (j = 0; j < i; j++)
         if (p->refs[j].slot == p->refs[i].slot) return false;
   }
   sets[0] = p->before; sets[1] = p->after; sets[2] = p->longterm;
   counts[0] = p->num_before; counts[1] = p->num_after; counts[2] = p->num_longterm;
   for (i = 0; i < 3; i++) for (j = 0; j < counts[i]; j++)
   {
      unsigned n = sets[i][j];
      if (n >= p->num_refs || (seen & (1u << n))
            || p->refs[n].long_term != (unsigned)(i == 2)
            || (i == 0 && p->refs[n].poc >= p->poc)
            || (i == 1 && p->refs[n].poc <= p->poc)) return false;
      seen |= 1u << n;
   }
   dxva_hevc_put16(out, p->width >> p->min_cb_log2);
   dxva_hevc_put16(out + 2, p->height >> p->min_cb_log2);
   bits = 1u | ((p->bit_depth - 8) << 3) | ((p->bit_depth - 8) << 6)
      | ((p->max_poc_lsb_log2 - 4) << 9)
      | ((!p->max_reorder_pics) << 13) | (p->no_bipred << 14);
   dxva_hevc_put16(out + 4, bits);
   out[6] = (uint8_t)p->slot;
   out[7] = (uint8_t)(p->max_dec_pic_buffering - 1);
   out[8] = (uint8_t)(p->min_cb_log2 - 3);
   out[9] = (uint8_t)(p->ctb_log2 - p->min_cb_log2);
   out[10] = (uint8_t)(p->min_tb_log2 - 2);
   out[11] = (uint8_t)(p->max_tb_log2 - p->min_tb_log2);
   out[12] = (uint8_t)p->hierarchy_inter; out[13] = (uint8_t)p->hierarchy_intra;
   out[14] = (uint8_t)p->num_st_rps; out[15] = (uint8_t)p->num_lt_rps;
   out[16] = (uint8_t)(p->default_l0 - 1); out[17] = (uint8_t)(p->default_l1 - 1);
   out[18] = (uint8_t)(p->init_qp - 26); out[19] = (uint8_t)p->delta_pocs;
   dxva_hevc_put16(out + 20, p->rps_bits);
   dxva_hevc_put32(out + 24, p->coding_tools);
   dxva_hevc_put32(out + 28, p->coding_settings);
   out[32] = (uint8_t)p->cb_qp; out[33] = (uint8_t)p->cr_qp;
   out[116] = (uint8_t)p->diff_qp_depth; out[117] = (uint8_t)p->beta_offset_div2;
   out[118] = (uint8_t)p->tc_offset_div2; out[119] = (uint8_t)(p->parallel_merge_log2 - 2);
   dxva_hevc_put32(out + 120, (uint32_t)p->poc);
   memset(out + 124, 0xff, 15);
   memset(out + 200, 0xff, 24);
   for (i = 0; i < p->num_refs; i++)
   {
      out[124 + i] = (uint8_t)(p->refs[i].slot | (p->refs[i].long_term << 7));
      dxva_hevc_put32(out + 140 + i * 4, (uint32_t)p->refs[i].poc);
   }
   for (i = 0; i < 3; i++) for (j = 0; j < counts[i]; j++)
      out[200 + i * 8 + j] = sets[i][j];
   dxva_hevc_put32(out + 228, feedback);
   return true;
}

/* Effective matrices, only when scaling_list_enabled_flag is set.
 * No matrix buffer is submitted for disabled scaling lists. */
static INLINE bool dxva_hevc_qmatrix(uint8_t *out, const dxva_hevc_scaling_t *s)
{
   unsigned i;
   const uint8_t *bytes = (const uint8_t*)s;
   if (!out) return false;
   memset(out, 0, DXVA_HEVC_QM_SIZE);
   if (!s) return false;
   for (i = 0; i < DXVA_HEVC_QM_SIZE; i++) if (!bytes[i]) return false;
   memcpy(out, bytes, DXVA_HEVC_QM_SIZE);
   return true;
}
/* Raster input uses the same size classes, with 32x32 matrix IDs 0 and 3
 * packed into list32[0] and list32[1]. Output must not overlap input. */
static INLINE bool dxva_hevc_qmatrix_raster(uint8_t *out,
      const dxva_hevc_scaling_t *s)
{
   unsigned size, matrix, diagonal, x, y, n;
   const uint8_t *src;
   uint8_t *dst;
   if (!out) return false;
   memset(out, 0, DXVA_HEVC_QM_SIZE);
   if (!s) return false;
   src = (const uint8_t*)s;
   for (n = 0; n < DXVA_HEVC_QM_SIZE; n++) if (!src[n]) return false;
   for (size = 0; size < 4; size++)
   {
      unsigned width = size ? 8 : 4;
      unsigned count = size == 3 ? 2 : 6;
      unsigned base = size == 0 ? 0 : size == 1 ? 96 : size == 2 ? 480 : 864;
      for (matrix = 0; matrix < count; matrix++)
      {
         src = size == 0 ? s->list4[matrix] : size == 1 ? s->list8[matrix]
            : size == 2 ? s->list16[matrix] : s->list32[matrix];
         dst = out + base + matrix * width * width;
         n = 0;
         for (diagonal = 0; diagonal < 2 * width - 1; diagonal++)
            for (x = 0; x < width; x++)
            {
               if (diagonal < x) continue;
               y = diagonal - x;
               if (y < width) dst[n++] = src[y * width + x];
            }
      }
   }
   memcpy(out + 992, s->dc16, 6);
   memcpy(out + 998, s->dc32, 2);
   return true;
}

/* Slice offsets point to Annex-B start codes. Data stays borrowed; only
 * slice descriptors are written. The backend supplies zero padding through
 * padded_bytes (a multiple of 128); the last slice accounts for it. */
static INLINE bool dxva_hevc_slices(uint8_t *out, size_t capacity,
      const uint8_t *data, size_t bytes, const uint32_t *offsets,
      unsigned count, size_t padded_bytes)
{
   unsigned i;
   size_t needed;
   if (!out) return false;
   if (!count || count > capacity / DXVA_HEVC_SLICE_SIZE) return false;
   needed = (size_t)count * DXVA_HEVC_SLICE_SIZE;
   memset(out, 0, needed);
   if (!data || !offsets || !bytes || bytes > UINT32_MAX
         || padded_bytes < bytes || padded_bytes > UINT32_MAX
         || (padded_bytes & 127) || offsets[0] != 0) return false;
   for (i = 0; i < count; i++)
   {
      size_t begin = offsets[i], end = i + 1 < count ? offsets[i+1] : bytes;
      size_t prefix;
      unsigned type;
      if (begin >= end || end > bytes || end - begin < 5) return false;
      if (data[begin] || data[begin+1]) return false;
      if (data[begin+2] == 1) prefix = 3;
      else if (!data[begin+2] && data[begin+3] == 1) prefix = 4;
      else return false;
      if (end - begin < prefix + 2) return false;
      type = (data[begin+prefix] >> 1) & 63;
      if ((data[begin+prefix] & 0x81) || (data[begin+prefix+1] >> 3)
            || !(data[begin+prefix+1] & 7)
            || !(type <= 9 || (type >= 16 && type <= 21))) return false;
   }
   for (i = 0; i < count; i++)
   {
      size_t end = i + 1 < count ? offsets[i+1] : padded_bytes;
      dxva_hevc_put32(out + i * 10, offsets[i]);
      dxva_hevc_put32(out + i * 10 + 4, (uint32_t)(end - offsets[i]));
   }
   return true;
}
#endif
