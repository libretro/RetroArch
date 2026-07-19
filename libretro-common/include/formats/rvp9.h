/* rvp9 -- self-contained VP9 video decoder for libretro-common.
 *
 * Decodes VP9 profile 0 (8-bit 4:2:0) video streams: intra and inter
 * frames, all transform sizes and intra/inter prediction modes,
 * switchable interpolation filters, compound prediction, high-precision
 * motion vectors, backward probability adaptation, reference-frame
 * management (including show_existing_frame) and the full loop filter.
 * Every path is verified byte-identical against libvpx.
 *
 * Deliberately unsupported (rvp9_decode_frame returns an error):
 * segmentation, scaled (different-size) reference frames, and
 * profiles 1-3 (profile 2/3 are the 10/12-bit streams used for HDR;
 * these return -15 specifically so callers can report them as such).
 * Tiled streams (tile columns and tile rows) decode
 * fully, so encoder defaults at any resolution are covered.
 *
 * Usage: zero-initialise an rvp9_dec (it is large; heap allocation is
 * recommended), feed each coded frame to rvp9_decode_frame(), display
 * the returned frame buffer when show_fb >= 0, and call rvp9_free()
 * when done.  Frame dimensions are fixed after the first frame.
 *
 * SPDX-License-Identifier: MIT  (RetroArch libretro-common)
 */
#ifndef __LIBRETRO_SDK_FORMAT_RVP9_H__
#define __LIBRETRO_SDK_FORMAT_RVP9_H__

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define RVP9_REFS_PER_FRAME 3
#define RVP9_REF_FRAMES     8
#define RVP9_MAX_SEGMENTS   8
#define RVP9_SEG_TREE_PROBS 7
#define RVP9_PRED_PROBS     3
#define RVP9_SEG_LVL_MAX    4
#define RVP9_FRAME_CONTEXTS 4

typedef struct
{
   /* uncompressed header */
   int profile;
   int show_existing_frame, frame_to_show;
   int frame_type;          /* 0 = key, 1 = inter */
   int show_frame;
   int error_resilient;
   int intra_only;
   int reset_frame_context;
   int color_space, color_range;
   int bit_depth;           /* 8; 10/12 for profile 2 (parsed, not yet decodable) */
   int subsampling_x, subsampling_y;
   int refresh_frame_flags;
   int ref_idx[RVP9_REFS_PER_FRAME];
   int ref_sign_bias[4];     /* indexed 1=last 2=golden 3=altref */
   int allow_high_precision_mv;
   int interp_filter;        /* 0-3 fixed, 4 = switchable */
   int w, h;                 /* frame dimensions */
   int render_w, render_h;
   int refresh_frame_context;
   int frame_parallel_decoding;
   int frame_context_idx;
   /* loop filter */
   int lf_level, lf_sharpness;
   int lf_delta_enabled, lf_delta_update;
   int lf_ref_deltas[4], lf_mode_deltas[2];
   int lf_ref_delta_upd[4], lf_mode_delta_upd[2];
   /* quant */
   int base_qindex, y_dc_delta_q, uv_dc_delta_q, uv_ac_delta_q;
   int lossless;
   /* segmentation */
   int seg_enabled, seg_update_map, seg_temporal_update, seg_update_data;
   int seg_abs_delta;
   uint8_t seg_tree_probs[RVP9_SEG_TREE_PROBS];
   uint8_t seg_pred_probs[RVP9_PRED_PROBS];
   int seg_feature_enabled[RVP9_MAX_SEGMENTS][RVP9_SEG_LVL_MAX];
   int seg_feature_data[RVP9_MAX_SEGMENTS][RVP9_SEG_LVL_MAX];
   /* tiles */
   int log2_tile_cols, log2_tile_rows;
   /* compressed header size */
   int first_partition_size;
   /* derived */
   int mi_cols, mi_rows;     /* in 8x8 mode-info units */
   int sb_cols, sb_rows;     /* in 64x64 superblocks */
   size_t uncomp_size;       /* bytes consumed by the uncompressed header */
} rvp9_hdr;

typedef struct
{
   const uint8_t *buf, *end;
   uint64_t value;
   int      count;    /* bits valid in value minus 8 */
   unsigned range;
} rvp9_br;

#define RVP9_TX_SIZES         4
#define RVP9_TX_SIZE_CONTEXTS 2
#define RVP9_PLANE_TYPES      2
#define RVP9_REF_TYPES        2
#define RVP9_COEF_BANDS       6
#define RVP9_COEFF_CONTEXTS   6
#define RVP9_UNCONSTRAINED    3
#define RVP9_SKIP_CONTEXTS    3
#define RVP9_INTRA_MODES      10
#define RVP9_PARTITION_TYPES  4
#define RVP9_PARTITION_CTXS   16

typedef struct
{
   uint8_t tx8[RVP9_TX_SIZE_CONTEXTS][1];
   uint8_t tx16[RVP9_TX_SIZE_CONTEXTS][2];
   uint8_t tx32[RVP9_TX_SIZE_CONTEXTS][3];
   uint8_t coef[RVP9_TX_SIZES][RVP9_PLANE_TYPES][RVP9_REF_TYPES]
               [RVP9_COEF_BANDS][RVP9_COEFF_CONTEXTS][RVP9_UNCONSTRAINED];
   uint8_t skip[RVP9_SKIP_CONTEXTS];
   /* inter */
   uint8_t inter_mode[7][3];
   uint8_t interp[4][2];
   uint8_t intra_inter[4];
   uint8_t comp_inter[5];
   uint8_t single_ref[5][2];
   uint8_t comp_ref[5];
   uint8_t y_mode[4][9];
   uint8_t uv_mode[10][9];
   uint8_t partition[16][3];
   /* nmv context */
   uint8_t mv_joints[3];
   struct
   {
      uint8_t sign, classes[10], class0[1], bits[10];
      uint8_t class0_fp[2][3], fp[3];
      uint8_t class0_hp, hp;
   } mv_comp[2];
} rvp9_frame_ctx;

typedef struct
{
   unsigned coef[RVP9_TX_SIZES][RVP9_PLANE_TYPES][RVP9_REF_TYPES]
                [RVP9_COEF_BANDS][RVP9_COEFF_CONTEXTS][4];
   unsigned eob_branch[RVP9_TX_SIZES][RVP9_PLANE_TYPES][RVP9_REF_TYPES]
                      [RVP9_COEF_BANDS][RVP9_COEFF_CONTEXTS];
   unsigned tx8[2][2], tx16[2][3], tx32[2][4];
   unsigned skip[3][2];
   unsigned inter_mode[7][4];
   unsigned interp[4][3];
   unsigned intra_inter[4][2];
   unsigned comp_inter[5][2];
   unsigned single_ref[5][2][2];
   unsigned comp_ref[5][2];
   unsigned y_mode[4][10];
   unsigned uv_mode[10][10];
   unsigned partition[16][4];
   unsigned mv_joints[4];
   struct
   {
      unsigned sign[2], classes[11], class0[2], bits[10][2];
      unsigned class0_fp[2][4], fp[4];
      unsigned class0_hp[2], hp[2];
   } mv_comp[2];
} rvp9_counts;

typedef int32_t rvp9_tran;

typedef struct { int16_t row, col; } rvp9_mv;

typedef struct
{
   uint8_t sb_type;      /* block size 0..12                       */
   uint8_t mode;         /* y mode / inter mode (bmi[3] for <8x8)  */
   uint8_t uv_mode;
   uint8_t tx_size;
   uint8_t skip;
   uint8_t segment_id;
   uint8_t bmodes[4];    /* sub-8x8 y modes / b inter modes        */
   int8_t  ref_frame[2]; /* 0=intra 1=last 2=golden 3=alt -1=none  */
   uint8_t interp_filter;
   rvp9_mv mv[2];        /* block MVs (== bmi[3] for sub-8x8)      */
   rvp9_mv bmv[4][2];    /* sub-8x8 MVs                            */
} rvp9_mi;

typedef struct
{
   int8_t  ref_frame[2];
   rvp9_mv mv[2];
} rvp9_mv_ref;

#define RVP9_FRAME_BUFS 10

typedef struct
{
   uint8_t *y, *u, *v;
   int      w, h;         /* crop dims */
   int      ref_count;
   int      valid;
} rvp9_fb;

typedef struct
{
   rvp9_hdr hd;
   rvp9_frame_ctx fc;                /* working context             */
   rvp9_frame_ctx frame_ctxs[4];     /* persistent contexts         */
   int      frame_ctxs_init;
   rvp9_counts cnt;
   rvp9_br  r;                       /* tile bool decoder           */
   int      tx_mode;
   int      reference_mode;          /* 0 single 1 compound 2 select*/
   int      comp_fixed_ref, comp_var_ref[2];

   /* reference management */
   rvp9_fb  fbs[RVP9_FRAME_BUFS];
   int      ref_frame_map[8];        /* slot -> fb index, -1 empty  */
   int      new_fb;                  /* fb being decoded            */
   int      frame_refs[3];           /* LAST/GOLDEN/ALT -> fb index */

   /* prev-frame MV prediction */
   rvp9_mv_ref *prev_mvs;            /* last decoded frame's MVs    */
   rvp9_mv_ref *cur_mvs;
   int      use_prev_frame_mvs;
   int      last_w, last_h, last_show, last_intra_only, last_key;

   int      mi_cols, mi_rows;
   int      tile_col_start, tile_col_end;  /* current tile, mi units */
   rvp9_mi *mi;                      /* mi_rows * mi_cols           */

   /* partition context (per mi col / row-within-SB) */
   uint8_t *above_seg;               /* mi_cols aligned to 8        */
   uint8_t  left_seg[8];

   /* entropy (token) context per plane, per 4x4 column/row */
   uint8_t *above_ctx[3];            /* aligned mi_cols * 2 entries */
   uint8_t  left_ctx[3][16];

   /* planes */
   uint8_t *buf_y, *buf_u, *buf_v;
   /* geometry the buffers above were sized for; every frame header can
    * change mi_cols/mi_rows, so this is what decides whether they still
    * fit */
   int      alloc_mi_cols, alloc_mi_rows;
   int      ys, uvs;
   int      yw, yh, uvw, uvh;        /* padded plane dims           */

   /* dequant per segment: [seg][0]=dc [1]=ac, per plane group      */
   int16_t  y_dq[8][2], uv_dq[8][2];

   /* cat6 escape token: probability pointer + extra-bit count for the
    * current frame's bit depth (14/16/18 bits for 8/10/12-bit). */
   const uint8_t *cat6_prob;
   int      cat6_bits;

   /* pixel width the frame buffers were allocated for (8 or 10);
    * set on first frame, streams may not switch mid-way */
   int      fb_bit_depth;

   rvp9_tran dqcoeff[32 * 32];
   int      max_blocks_wide, max_blocks_high;  /* token ctx trunc  */
   int      mb_to_right_edge, mb_to_bottom_edge; /* in 1/8 pel *8  */
   int      mb_to_left_edge, mb_to_top_edge;
   int      corrupted;
   int      lf_ref_deltas[4], lf_mode_deltas[2];   /* persistent */
} rvp9_dec;

/* Decode one coded VP9 frame (one WebM block / IVF frame payload).
 * Returns 0 on success, 1 for show_existing_frame, negative on error.
 * On success *show_fb is the index into d->fbs of the frame to display
 * (-1 when the frame is not shown).  The visible picture is the top-left
 * fb->w x fb->h region of d->fbs[*show_fb].y (stride d->ys) with chroma
 * planes .u/.v of (w+1)/2 x (h+1)/2 (stride d->uvs). */
int rvp9_decode_frame(rvp9_dec *d, const uint8_t *data, size_t len,
      int *show_fb);

/* Release all buffers owned by the decoder.  The rvp9_dec itself is
 * caller-owned.  Safe on a zero-initialised or partially set-up state. */
void rvp9_free(rvp9_dec *d);

RETRO_END_DECLS

#endif
