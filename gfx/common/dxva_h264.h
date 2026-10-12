/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __DXVA_H264_H
#define __DXVA_H264_H

/* The DXVA H.264 buffers of an rh264 picture, written byte by byte in
 * the layout of DXVA_PicParams_H264, DXVA_Qmatrix_H264 and
 * DXVA_Slice_H264_Short (packed, little-endian) so that no dxva.h is
 * needed. samples/gfx/dxva_h264 checks the offsets against one. */

#include <stdint.h>
#include <string.h>

#include <retro_inline.h>
#include <formats/rh264.h>

#define DXVA_H264_PP_SIZE      1040
#define DXVA_H264_QM_SIZE      224
#define DXVA_H264_SLICE_SIZE   10

/* DXVA_PicParams_H264 */
#define DXVA_H264_PP_WIDTH_MBS     0
#define DXVA_H264_PP_HEIGHT_MBS    2
#define DXVA_H264_PP_CURR_PIC      4
#define DXVA_H264_PP_NUM_REF       5
#define DXVA_H264_PP_BITS          6
#define DXVA_H264_PP_FEEDBACK      12
#define DXVA_H264_PP_REF_LIST      16
#define DXVA_H264_PP_CURR_POC      32
#define DXVA_H264_PP_POC_LIST      40
#define DXVA_H264_PP_INIT_QS       168
#define DXVA_H264_PP_CHROMA_QP     169
#define DXVA_H264_PP_CHROMA_QP2    170
#define DXVA_H264_PP_CONTINUATION  171
#define DXVA_H264_PP_INIT_QP       172
#define DXVA_H264_PP_REF_IDX_L0    173
#define DXVA_H264_PP_REF_IDX_L1    174
#define DXVA_H264_PP_FRAME_NUMS    176
#define DXVA_H264_PP_USED_FOR_REF  208
#define DXVA_H264_PP_FRAME_NUM     214
#define DXVA_H264_PP_LOG2_FN       216
#define DXVA_H264_PP_POC_TYPE      217
#define DXVA_H264_PP_LOG2_POC      218
#define DXVA_H264_PP_DPOAZ         219
#define DXVA_H264_PP_DIRECT8X8     220
#define DXVA_H264_PP_CABAC         221
#define DXVA_H264_PP_POC_PRESENT   222
#define DXVA_H264_PP_DEBLOCK_CTL   225

/* DXVA_PicParams_H264's bit fields, from the low bit */
#define DXVA_H264_BIT_CHROMA_SHIFT 4
#define DXVA_H264_BIT_REF_PIC      (1u << 6)
#define DXVA_H264_BIT_CONSTR_INTRA (1u << 7)
#define DXVA_H264_BIT_WEIGHTED     (1u << 8)
#define DXVA_H264_BIT_BIPRED_SHIFT 9
#define DXVA_H264_BIT_MBS_CONSEC   (1u << 11)
#define DXVA_H264_BIT_FRAME_MBS    (1u << 12)
#define DXVA_H264_BIT_T8X8         (1u << 13)
#define DXVA_H264_BIT_BIPRED8X8    (1u << 14)
#define DXVA_H264_BIT_INTRA        (1u << 15)

static INLINE void dxva_h264_put16(uint8_t *o, unsigned v)
{
   o[0] = (uint8_t)v;
   o[1] = (uint8_t)(v >> 8);
}

static INLINE void dxva_h264_put32(uint8_t *o, uint32_t v)
{
   o[0] = (uint8_t)v;
   o[1] = (uint8_t)(v >> 8);
   o[2] = (uint8_t)(v >> 16);
   o[3] = (uint8_t)(v >> 24);
}

/* @pic's picture parameters into @o, DXVA_H264_PP_SIZE bytes. */
static INLINE void dxva_h264_pic_params(uint8_t *o,
      const rh264_hw_pic_t *pic, uint32_t feedback)
{
   const rh264_hw_sps_t *s = pic->sps;
   const rh264_hw_pps_t *p = pic->pps;
   unsigned bits;
   uint32_t used = 0;
   unsigned i;

   memset(o, 0, DXVA_H264_PP_SIZE);
   dxva_h264_put16(o + DXVA_H264_PP_WIDTH_MBS,  (unsigned)s->width_in_mbs  - 1);
   dxva_h264_put16(o + DXVA_H264_PP_HEIGHT_MBS, (unsigned)s->height_in_mbs - 1);
   o[DXVA_H264_PP_CURR_PIC] = (uint8_t)(pic->slot & 0x7f);
   o[DXVA_H264_PP_NUM_REF]  = (uint8_t)s->max_num_ref_frames;
   bits = ((unsigned)s->chroma_format_idc << DXVA_H264_BIT_CHROMA_SHIFT)
        | ((unsigned)p->weighted_bipred_idc << DXVA_H264_BIT_BIPRED_SHIFT)
        | DXVA_H264_BIT_MBS_CONSEC | DXVA_H264_BIT_FRAME_MBS;
   if (pic->reference)
      bits |= DXVA_H264_BIT_REF_PIC;
   if (p->constrained_intra_pred)
      bits |= DXVA_H264_BIT_CONSTR_INTRA;
   if (p->weighted_pred)
      bits |= DXVA_H264_BIT_WEIGHTED;
   if (p->transform_8x8_mode)
      bits |= DXVA_H264_BIT_T8X8;
   /* no bi-prediction of blocks under 8x8 from level 3.1 (A.3.3) */
   if (s->level_idc >= 31)
      bits |= DXVA_H264_BIT_BIPRED8X8;
   if (pic->intra)
      bits |= DXVA_H264_BIT_INTRA;
   dxva_h264_put16(o + DXVA_H264_PP_BITS, bits);
   /* bit depths minus 8 are 0, and Reserved16Bits */
   dxva_h264_put32(o + DXVA_H264_PP_FEEDBACK, feedback);

   for (i = 0; i < 16; i++)
   {
      uint8_t *poc = o + DXVA_H264_PP_POC_LIST + i * 8;
      if (i < pic->num_refs)
      {
         const rh264_hw_ref_t *r = &pic->refs[i];
         o[DXVA_H264_PP_REF_LIST + i] = (uint8_t)((r->slot & 0x7f)
               | (r->long_term ? 0x80 : 0));
         dxva_h264_put32(poc,     (uint32_t)r->poc[0]);
         dxva_h264_put32(poc + 4, (uint32_t)r->poc[1]);
         dxva_h264_put16(o + DXVA_H264_PP_FRAME_NUMS + i * 2,
               (unsigned)r->frame_num);
         used |= 3u << (2 * i);
      }
      else
         o[DXVA_H264_PP_REF_LIST + i] = 0xff;
   }
   dxva_h264_put32(o + DXVA_H264_PP_CURR_POC,     (uint32_t)pic->poc[0]);
   dxva_h264_put32(o + DXVA_H264_PP_CURR_POC + 4, (uint32_t)pic->poc[1]);

   o[DXVA_H264_PP_INIT_QS]      = (uint8_t)(p->pic_init_qs - 26);
   o[DXVA_H264_PP_CHROMA_QP]    = (uint8_t)p->chroma_qp_index_offset;
   o[DXVA_H264_PP_CHROMA_QP2]   = (uint8_t)p->second_chroma_qp_index_offset;
   o[DXVA_H264_PP_CONTINUATION] = 1;
   o[DXVA_H264_PP_INIT_QP]      = (uint8_t)(p->pic_init_qp - 26);
   o[DXVA_H264_PP_REF_IDX_L0]   = (uint8_t)(p->num_ref_idx_l0_default - 1);
   o[DXVA_H264_PP_REF_IDX_L1]   = (uint8_t)(p->num_ref_idx_l1_default - 1);
   dxva_h264_put32(o + DXVA_H264_PP_USED_FOR_REF, used);
   dxva_h264_put16(o + DXVA_H264_PP_FRAME_NUM, (unsigned)pic->frame_num);
   o[DXVA_H264_PP_LOG2_FN]      = (uint8_t)(s->log2_max_frame_num - 4);
   o[DXVA_H264_PP_POC_TYPE]     = (uint8_t)s->poc_type;
   o[DXVA_H264_PP_LOG2_POC]     = (uint8_t)(s->poc_type == 0
         ? s->log2_max_poc_lsb - 4 : 0);
   o[DXVA_H264_PP_DPOAZ]        = (uint8_t)(s->delta_pic_order_always_zero ? 1 : 0);
   o[DXVA_H264_PP_DIRECT8X8]    = (uint8_t)(s->direct_8x8_inference ? 1 : 0);
   o[DXVA_H264_PP_CABAC]        = (uint8_t)(p->entropy_coding_mode ? 1 : 0);
   o[DXVA_H264_PP_POC_PRESENT]  = (uint8_t)(
         p->bottom_field_pic_order_in_frame_present ? 1 : 0);
   o[DXVA_H264_PP_DEBLOCK_CTL]  = (uint8_t)(
         p->deblocking_filter_control_present ? 1 : 0);
   /* one slice group, no redundant pictures: the rest stays 0 */
}

/* Table 7-3 and 7-4, in zig-zag order as the lists are kept */
static const uint8_t dxva_h264_default4[2][16] = {
   {  6, 13, 13, 20, 20, 20, 28, 28, 28, 28, 32, 32, 32, 37, 37, 42 },
   { 10, 14, 14, 20, 20, 20, 24, 24, 24, 24, 27, 27, 27, 30, 30, 34 }
};
static const uint8_t dxva_h264_default8[2][64] = {
   {  6, 10, 10, 13, 11, 13, 16, 16, 16, 16, 18, 18, 18, 18, 18, 23,
     23, 23, 23, 23, 23, 25, 25, 25, 25, 25, 25, 25, 27, 27, 27, 27,
     27, 27, 27, 27, 29, 29, 29, 29, 29, 29, 29, 31, 31, 31, 31, 31,
     31, 33, 33, 33, 33, 33, 36, 36, 36, 36, 38, 38, 38, 40, 40, 42 },
   {  9, 13, 13, 15, 13, 15, 17, 17, 17, 17, 19, 19, 19, 19, 19, 21,
     21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 24, 24, 24, 24,
     24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 27, 27, 27, 27, 27,
     27, 28, 28, 28, 28, 28, 30, 30, 30, 30, 32, 32, 32, 33, 33, 35 }
};

/* The lists one parameter set gives: its own where sent, else the
 * fall-back of 7.4.2.1.1 - @fb4 for lists 0 and 3, @fb8 for the 8x8
 * ones, the list before for the rest. */
static INLINE void dxva_h264_lists_of(uint8_t l4[6][16], uint8_t l8[2][64],
      const rh264_hw_scaling_t *sc, const uint8_t fb4[2][16],
      const uint8_t fb8[2][64])
{
   unsigned i;
   for (i = 0; i < 6; i++)
   {
      if (sc->present_mask & (1u << i))
         memcpy(l4[i], (sc->default_mask & (1u << i))
               ? dxva_h264_default4[i / 3] : sc->list4x4[i], 16);
      else if (i == 0 || i == 3)
         memcpy(l4[i], fb4[i / 3], 16);
      else
         memmove(l4[i], l4[i - 1], 16);
   }
   for (i = 0; i < 2; i++)
   {
      if (sc->present_mask & (1u << (6 + i)))
         memcpy(l8[i], (sc->default_mask & (1u << (6 + i)))
               ? dxva_h264_default8[i] : sc->list8x8[i], 64);
      else
         memcpy(l8[i], fb8[i], 64);
   }
}

/* The frame scans: where each coded position lies in raster order */
static const uint8_t dxva_h264_zigzag4[16] = {
   0, 1, 4, 8, 5, 2, 3, 6, 9, 12, 13, 10, 7, 11, 14, 15
};
static const uint8_t dxva_h264_zigzag8[64] = {
    0,  1,  8, 16,  9,  2,  3, 10, 17, 24, 32, 25, 18, 11,  4,  5,
   12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13,  6,  7, 14, 21, 28,
   35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
   58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

/* The scaling lists @pic decodes with into @o, DXVA_H264_QM_SIZE
 * bytes: 4x4 then the two luma 8x8, each in zig-zag order, or in
 * raster order with @raster. */
static INLINE void dxva_h264_qmatrix(uint8_t *o, const rh264_hw_pic_t *pic,
      int raster)
{
   const rh264_hw_sps_t *s = pic->sps;
   const rh264_hw_pps_t *p = pic->pps;
   uint8_t s4[6][16], s8[2][64];
   uint8_t l4[6][16], l8[2][64];
   uint8_t fb4[2][16], fb8[2][64];

   /* the sequence's: Flat_16 when it sends none, rule A when it does */
   if (s->scaling_present)
      dxva_h264_lists_of(s4, s8, &s->scaling,
            dxva_h264_default4, dxva_h264_default8);
   else
   {
      memset(s4, 16, sizeof(s4));
      memset(s8, 16, sizeof(s8));
   }
   if (!p->scaling_present)
   {
      memcpy(l4, s4, sizeof(l4));
      memcpy(l8, s8, sizeof(l8));
   }
   else if (!s->scaling_present)
      dxva_h264_lists_of(l4, l8, &p->scaling,
            dxva_h264_default4, dxva_h264_default8);
   else
   {
      /* rule B: the sequence's lists in place of the defaults */
      memcpy(fb4[0], s4[0], 16);
      memcpy(fb4[1], s4[3], 16);
      memcpy(fb8, s8, sizeof(fb8));
      dxva_h264_lists_of(l4, l8, &p->scaling,
            (const uint8_t (*)[16])fb4, (const uint8_t (*)[64])fb8);
   }
   if (raster)
   {
      unsigned i, j;
      for (i = 0; i < 6; i++)
         for (j = 0; j < 16; j++)
            o[i * 16 + dxva_h264_zigzag4[j]] = l4[i][j];
      for (i = 0; i < 2; i++)
         for (j = 0; j < 64; j++)
            o[96 + i * 64 + dxva_h264_zigzag8[j]] = l8[i][j];
      return;
   }
   memcpy(o,      l4, 96);
   memcpy(o + 96, l8, 128);
}

/* One DXVA_Slice_H264_Short. */
static INLINE void dxva_h264_slice(uint8_t *o, uint32_t location,
      uint32_t bytes)
{
   dxva_h264_put32(o,     location);
   dxva_h264_put32(o + 4, bytes);
   dxva_h264_put16(o + 8, 0);
}

#endif
