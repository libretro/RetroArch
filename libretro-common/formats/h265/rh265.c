/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rh265.c).
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
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* rh265 -- clean-room H.265/HEVC decoder (amalgamated single TU).
 * Public API: include/formats/rh265.h, mirroring rh264_video/rvp8_video
 * so a demuxer (e.g. the MP4 glue in rmp4_video.c) can dispatch H.265
 * the same way.
 *
 * What it implements: Main-profile intra pictures (IDR, CRA and TRAIL
 * I slices) in 8-bit 4:2:0; the CABAC engine (the arithmetic core is
 * the same M-coder as H.264, ported from rh264.c) with the full
 * context-init tables; the coding quadtree with intra 2Nx2N and NxN
 * partitions; all 35 intra prediction modes with reference-sample
 * substitution, [1 2 1] smoothing and the 32x32 strong-intra-smoothing
 * bilinear filter; the complete residual syntax -- last-significant
 * prefix/suffix, coded-sub-block and significance maps, greater1/
 * greater2, Rice-adapted remaining levels, sign-data hiding,
 * mode-dependent coefficient scans; cu_qp_delta quantisation groups
 * (8.6.1 predictor); the 4x4 DST and 4/8/16/32 partial-butterfly
 * inverse DCTs plus transform skip; the in-loop deblocking filter
 * (whole-picture vertical then horizontal, 8.7.2, with boundary
 * strengths from prediction and transform edges); sample-adaptive
 * offset, band and edge, classified on the fully deblocked picture
 * (8.7.3); multiple independent slice segments per picture; POC
 * derivation (8.3.1); conformance-window cropping; and both Annex-B
 * (start-code) and length-prefixed HVCC input, with the NAL length
 * size taken from the hvcC extradata when present (default 4).
 *
 * Inter coding is implemented in full for the Main profile: P and B
 * slices with all partition modes (2Nx2N through the four AMP
 * shapes), merge mode with spatial, temporal (TMVP) and combined
 * bi-predictive candidates, AMVP with POC-distance scaling, luma
 * quarter-pel 8-tap and chroma eighth-pel 4-tap interpolation,
 * weighted uni- and bi-prediction, the short-term reference picture
 * set machinery (8.3.2, including inter-RPS-predicted sets stored in
 * the SPS), reference list construction with list modification, a
 * decoded picture buffer with sps_max_num_reorder_pics output
 * bumping, CRA/BLA/IDR NoRaslOutputFlag handling with RASL skipping,
 * and prevTid0 POC prediction across temporal sub-layers.
 *
 * Both Main (8-bit) and Main10 (10-bit) streams decode; the
 * sample-touching layers are instantiated per bit depth from
 * rh265_bd.inc and everything above them is depth-independent apart
 * from the QpBdOffset domain, the transform scaling shifts and the
 * sao_offset_abs binarisation bound.  At 10 bits the picture planes
 * hold uint16_t samples: rh265_video_plane still returns a byte
 * pointer (cast it; the stride counts samples) and
 * rh265_video_bit_depth reports the active depth.
 *
 * Wavefront parallel processing (entropy_coding_sync) decodes
 * serially: slice-header entry points position each CTB row's
 * substream (offsets translated from the escaped byte domain), the
 * CABAC engine re-anchors per row with contexts carried from after
 * the second CTB of the row above (9.3.2.2-3, falling back to slice
 * initialisation across slice boundaries), end_of_subset bits close
 * each row, and qPY_PREV resets to SliceQpY at row starts (8.6.1).
 * Since WPP is x265's default, this is what most real-world HEVC
 * needs.  Multi-slice pictures honour
 * slice_loop_filter_across_slices: deblocking skips slice-boundary
 * edges and SAO treats cross-slice neighbours as unavailable when
 * the flag is off (8.7.2, 8.7.3).
 *
 * Scaling lists are implemented in full: the default matrices of
 * Table 7-5/7-6 (what hardware encoders typically signal), explicit
 * SPS/PPS-coded lists with DPCM coefficients in up-right diagonal
 * order and matrix-delta prediction, and per-coefficient
 * dequantisation with the sub-sampled 16x16/32x32 grids and their
 * separate DC terms (7.3.4, 7.4.5, 8.6.3).  Version-1 4x4
 * transform-skip blocks use the lists like any other TU; the m = 16
 * transform-skip carve-out in 8.6.3 only applies to the
 * range-extension sizes.
 *
 * What it does not implement: long-term reference pictures, 4:2:2,
 * 4:4:4 and monochrome, bit depths above 10, tiles, dependent slice
 * segments, PCM, transquant bypass, constrained intra prediction,
 * and encoding.  Out-of-scope streams are refused at the
 * parameter-set or slice level rather than decoded wrongly.
 *
 * The CABAC context-init tables, coefficient scan orders and the
 * significance-map context maps are extracted programmatically from
 * FFmpeg's libavcodec/hevc (which carries the initValue entries of
 * ITU-T H.265 tables 9-5..9-31); no table is hand-transcribed.  The
 * conformance oracle for this file is the HM reference decoder
 * (TAppDecoder): decoded pictures validate byte-exact against it
 * across CTB sizes 16/32/64, and against FFmpeg's software decoder
 * everywhere except one known FFmpeg corner case (its per-CTB SAO
 * classifies CTB-corner samples against a right-hand neighbour that
 * is only vertically deblocked, visible with 16px CTBs; rh265 follows
 * the spec and HM).
 *
 *   rh265_video *v = rh265_video_open();
 *   rh265_video_set_extradata(v, hvcc, hvcc_len);   (the hvcC box payload)
 *   for each frame:
 *      if (rh265_video_decode(v, data, len) < 0) fail;   (1 = picture ready)
 *      y = rh265_video_plane(v, 0, &ystride, &w, &h);    (also 1=U, 2=V)
 *   rh265_video_close(v);
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include <formats/rh265.h>

#if defined(_MSC_VER)
#define RH265_INLINE __forceinline
#elif defined(__GNUC__)
#define RH265_INLINE __inline__ __attribute__((always_inline))
#else
#define RH265_INLINE
#endif

static RH265_INLINE int rh265_min(int a, int b) { return a < b ? a : b; }
static RH265_INLINE int rh265_max(int a, int b) { return a > b ? a : b; }
static RH265_INLINE int rh265_clip3(int lo, int hi, int v)
{ return v < lo ? lo : (v > hi ? hi : v); }
static RH265_INLINE uint8_t rh265_clip8(int v)
{ return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v)); }

static RH265_INLINE uint32_t rh265_clz32(uint32_t x)
{
#if defined(__GNUC__)
   return (uint32_t)__builtin_clz(x);
#else
   uint32_t n = 0;
   if (!(x & 0xffff0000u)) { n += 16; x <<= 16; }
   if (!(x & 0xff000000u)) { n +=  8; x <<=  8; }
   if (!(x & 0xf0000000u)) { n +=  4; x <<=  4; }
   if (!(x & 0xc0000000u)) { n +=  2; x <<=  2; }
   if (!(x & 0x80000000u)) { n +=  1; }
   return n;
#endif
}

/* ==================== rh265_bits.h ==================== */
/* Plain (non-arithmetic) bitstream reader for NAL headers, parameter
 * sets and slice headers, with Exp-Golomb; same design as rh264_bits. */

typedef struct
{
   const uint8_t *buf;
   size_t size;
   size_t bitpos;
} rh265_bits;

static void rh265_bits_init(rh265_bits *b, const uint8_t *buf, size_t size)
{ b->buf = buf; b->size = size; b->bitpos = 0; }

static uint32_t rh265_u1(rh265_bits *b)
{
   uint32_t v;
   if (b->bitpos >= b->size * 8) { b->bitpos++; return 0; }
   v = (uint32_t)(b->buf[b->bitpos >> 3] >> (7 - (b->bitpos & 7))) & 1u;
   b->bitpos++;
   return v;
}

static uint32_t rh265_un(rh265_bits *b, int n)
{
   uint32_t v = 0;
   while (n--) v = (v << 1) | rh265_u1(b);
   return v;
}

static uint32_t rh265_ue(rh265_bits *b)
{
   int lz = 0;
   while (lz < 32 && !rh265_u1(b)) lz++;
   if (lz >= 32) return 0xffffffffu;
   if (!lz) return 0;
   return (uint32_t)((1u << lz) - 1u + rh265_un(b, lz));
}

static int32_t rh265_se(rh265_bits *b)
{
   uint32_t k = rh265_ue(b);
   uint32_t m = (k + 1) >> 1;
   return (k & 1) ? (int32_t)m : -(int32_t)m;
}

static int rh265_bits_overrun(const rh265_bits *b)
{ return b->bitpos > b->size * 8; }

/* Strip emulation-prevention bytes (00 00 03 -> 00 00).  When esc_pos
 * is non-NULL it receives a malloc'd array of the removed bytes'
 * positions in the escaped input (caller frees), for translating
 * WPP entry-point offsets, which count escaped bytes. */
static uint8_t *rh265_unescape_ex(const uint8_t *nal, size_t len,
      size_t *out_size, uint32_t **esc_pos, int *esc_count)
{
   uint8_t *rbsp = (uint8_t*)malloc(len ? len : 1);
   uint32_t *ep  = NULL;
   int       ec  = 0;
   size_t i, j = 0;
   int zeros = 0;
   if (!rbsp) return NULL;
   if (esc_pos)
   {
      *esc_pos = NULL;
      *esc_count = 0;
      ep = (uint32_t*)malloc((len / 3 + 1) * sizeof(uint32_t));
      if (!ep)
      {
         free(rbsp);
         return NULL;
      }
   }
   for (i = 0; i < len; i++)
   {
      if (zeros >= 2 && nal[i] == 3)
      {
         if (ep)
            ep[ec++] = (uint32_t)i;
         zeros = 0;
         continue;      /* emulation prevention byte */
      }
      if (nal[i] == 0) zeros++; else zeros = 0;
      rbsp[j++] = nal[i];
   }
   *out_size = j;
   if (esc_pos)
   {
      *esc_pos   = ep;
      *esc_count = ec;
   }
   return rbsp;
}

static uint8_t *rh265_unescape(const uint8_t *nal, size_t len,
      size_t *out_size)
{
   return rh265_unescape_ex(nal, len, out_size, NULL, NULL);
}

/* ==================== rh265_nal.h ==================== */

#define RH265_NAL_TRAIL_N        0
#define RH265_NAL_TRAIL_R        1
#define RH265_NAL_TSA_N          2
#define RH265_NAL_TSA_R          3
#define RH265_NAL_STSA_N         4
#define RH265_NAL_STSA_R         5
#define RH265_NAL_RADL_N         6
#define RH265_NAL_RADL_R         7
#define RH265_NAL_RASL_N         8
#define RH265_NAL_RASL_R         9
#define RH265_NAL_BLA_W_LP      16
#define RH265_NAL_BLA_W_RADL    17
#define RH265_NAL_BLA_N_LP      18
#define RH265_NAL_IDR_W_RADL    19
#define RH265_NAL_IDR_N_LP      20
#define RH265_NAL_CRA           21
#define RH265_NAL_VPS           32
#define RH265_NAL_SPS           33
#define RH265_NAL_PPS           34
#define RH265_NAL_AUD           35
#define RH265_NAL_EOS           36
#define RH265_NAL_EOB           37
#define RH265_NAL_FD            38
#define RH265_NAL_SEI_PREFIX    39
#define RH265_NAL_SEI_SUFFIX    40

#define RH265_IS_SLICE(t)  ((t) <= RH265_NAL_CRA)
#define RH265_IS_IRAP(t)   ((t) >= RH265_NAL_BLA_W_LP && (t) <= RH265_NAL_CRA)
#define RH265_IS_IDR(t)    ((t) == RH265_NAL_IDR_W_RADL || (t) == RH265_NAL_IDR_N_LP)

/* ==================== rh265_ps.h ==================== */

#define RH265_MAX_SPS       16
#define RH265_MAX_PPS       64
#define RH265_MAX_ST_RPS    65
/* one substream per CTB row: 16888 / 16 rows at the smallest CTB */
#define RH265_MAX_ENTRY_POINTS 1056
#define RH265_MAX_REFS      16
#define RH265_MAX_TB        32   /* MaxTbSizeY */

/* One short-term reference picture set, fully resolved (after inter-set
 * prediction) into delta POCs and used-by-curr flags. */
typedef struct
{
   int32_t delta_poc_s0[RH265_MAX_REFS];   /* negative deltas, closest first */
   int32_t delta_poc_s1[RH265_MAX_REFS];   /* positive deltas, closest first */
   uint8_t used_s0[RH265_MAX_REFS];
   uint8_t used_s1[RH265_MAX_REFS];
   int     num_negative;
   int     num_positive;
} rh265_st_rps;

/* Derived scaling lists in raster order per size class: 4x4 entries
 * are full, the 8x8 grids also serve 16x16 and 32x32 through the
 * (x >> s, y >> s) sub-sampling of 7-4x, with the coded DC values
 * kept separately for the two larger sizes. */
typedef struct
{
   uint8_t l4[6][16];
   uint8_t l8[6][64];
   uint8_t l16[6][64];
   uint8_t l32[6][64];
   uint8_t dc16[6];
   uint8_t dc32[6];
} rh265_scaling;

typedef struct
{
   int valid;
   int chroma_format_idc;               /* 1 = 4:2:0 (only accepted value) */
   int width, height;                   /* pic_{width,height}_in_luma_samples */
   int crop_left, crop_right, crop_top, crop_bottom;  /* conformance window, luma */
   int bit_depth_luma, bit_depth_chroma;
   int log2_max_poc_lsb;
   int log2_min_cb;                     /* MinCbLog2SizeY */
   int log2_ctb;                        /* CtbLog2SizeY */
   int log2_min_tb;                     /* MinTbLog2SizeY */
   int log2_max_tb;                     /* MaxTbLog2SizeY */
   int max_transform_hierarchy_depth_inter;
   int max_transform_hierarchy_depth_intra;
   int scaling_list_enabled;
   rh265_scaling sl;          /* defaults or the SPS-coded lists */
   int amp_enabled;
   int sao_enabled;
   int pcm_enabled;
   int num_st_rps;
   rh265_st_rps st_rps[RH265_MAX_ST_RPS];
   int long_term_ref_pics_present;
   int num_long_term_ref_pics;
   int lt_ref_poc_lsb[32];
   int lt_used_by_curr[32];
   int temporal_mvp_enabled;
   int strong_intra_smoothing;
   int max_dec_pic_buffering;           /* sps_max_dec_pic_buffering_minus1+1, highest sub-layer */
   int max_num_reorder_pics;

   /* derived */
   int ctb_w, ctb_h;                    /* PicWidthInCtbsY, PicHeightInCtbsY */
   int pic_size_ctbs;
} rh265_sps;

typedef struct
{
   int valid;
   int sps_id;
   int dependent_slice_segments_enabled;
   int output_flag_present;
   int num_extra_slice_header_bits;
   int sign_data_hiding;
   int cabac_init_present;
   int num_ref_idx_l0_default;
   int num_ref_idx_l1_default;
   int init_qp;                         /* init_qp_minus26 + 26 */
   int constrained_intra_pred;
   int transform_skip_enabled;
   int cu_qp_delta_enabled;
   int diff_cu_qp_delta_depth;
   int cb_qp_offset, cr_qp_offset;
   int slice_chroma_qp_offsets_present;
   int weighted_pred, weighted_bipred;
   int transquant_bypass_enabled;
   int tiles_enabled;
   int entropy_coding_sync_enabled;
   int loop_filter_across_slices;
   int deblocking_filter_control_present;
   int deblocking_filter_override_enabled;
   int deblocking_filter_disabled;
   int beta_offset_div2, tc_offset_div2;
   int lists_modification_present;
   int log2_parallel_merge_level;
   int slice_segment_header_extension_present;
   int has_scaling;           /* pps_scaling_list_data_present */
   rh265_scaling sl;
} rh265_pps;


/* ==================== scaling lists (7.3.4 / 7.4.5) ==================== */


/* up-right diagonal scan (6.5.3): coded scaling-list coefficients and
 * their raster positions */
static const uint8_t rh265_diag4[16] =
{
    0, 4, 1, 8, 5, 2,12, 9,
    6, 3,13,10, 7,14,11,15
};
static const uint8_t rh265_diag8[64] =
{
    0, 8, 1,16, 9, 2,24,17,
   10, 3,32,25,18,11, 4,40,
   33,26,19,12, 5,48,41,34,
   27,20,13, 6,56,49,42,35,
   28,21,14, 7,57,50,43,36,
   29,22,15,58,51,44,37,30,
   23,59,52,45,38,31,60,53,
   46,39,61,54,47,62,55,63
};

/* default matrices (Table 7-5/7-6): 4x4 is flat 16; the 8x8 pair also
 * seeds 16x16 and 32x32 */
static const uint8_t rh265_sl_default_intra8[64] =
{
   16,16,16,16,17,18,21,24,
   16,16,16,16,17,19,22,25,
   16,16,17,18,20,22,25,29,
   16,16,18,21,24,27,31,36,
   17,17,20,24,30,35,41,47,
   18,19,22,27,35,44,54,65,
   21,22,25,31,41,54,70,88,
   24,25,29,36,47,65,88,115
};
static const uint8_t rh265_sl_default_inter8[64] =
{
   16,16,16,16,17,18,20,24,
   16,16,16,17,18,20,24,25,
   16,16,17,18,20,24,25,28,
   16,17,18,20,24,25,28,33,
   17,18,20,24,25,28,33,41,
   18,20,24,25,28,33,41,54,
   20,24,25,28,33,41,54,71,
   24,25,28,33,41,54,71,91
};

static void rh265_sl_set_default(rh265_scaling *sl, int size_id,
      int matrix_id)
{
   const uint8_t *d8 = (matrix_id < 3)
         ? rh265_sl_default_intra8 : rh265_sl_default_inter8;
   switch (size_id)
   {
      case 0: memset(sl->l4[matrix_id], 16, 16); break;
      case 1: memcpy(sl->l8[matrix_id],  d8, 64); break;
      case 2: memcpy(sl->l16[matrix_id], d8, 64);
              sl->dc16[matrix_id] = 16; break;
      default: memcpy(sl->l32[matrix_id], d8, 64);
              sl->dc32[matrix_id] = 16; break;
   }
}

static void rh265_sl_defaults(rh265_scaling *sl)
{
   int size_id, matrix_id;
   for (size_id = 0; size_id < 4; size_id++)
      for (matrix_id = 0; matrix_id < 6; matrix_id++)
         rh265_sl_set_default(sl, size_id, matrix_id);
}

/* scaling_list_data, stored (7.3.4); the walker this replaces only
 * kept the bit position. */
static int rh265_parse_scaling_list_data(rh265_bits *b, rh265_scaling *sl)
{
   int size_id, matrix_id;
   for (size_id = 0; size_id < 4; size_id++)
      for (matrix_id = 0; matrix_id < 6;
            matrix_id += (size_id == 3) ? 3 : 1)
      {
         uint8_t *dst = (size_id == 0) ? sl->l4[matrix_id]
                      : (size_id == 1) ? sl->l8[matrix_id]
                      : (size_id == 2) ? sl->l16[matrix_id]
                      :                  sl->l32[matrix_id];
         if (!rh265_u1(b))               /* scaling_list_pred_mode_flag */
         {
            int delta = (int)rh265_ue(b);
            int step  = (size_id == 3) ? 3 : 1;
            int ref   = matrix_id - delta * step;
            if (delta == 0)
               rh265_sl_set_default(sl, size_id, matrix_id);
            else
            {
               if (ref < 0)
                  return -1;
               switch (size_id)
               {
                  case 0: memcpy(dst, sl->l4[ref], 16); break;
                  case 1: memcpy(dst, sl->l8[ref], 64); break;
                  case 2: memcpy(dst, sl->l16[ref], 64);
                          sl->dc16[matrix_id] = sl->dc16[ref]; break;
                  default: memcpy(dst, sl->l32[ref], 64);
                          sl->dc32[matrix_id] = sl->dc32[ref]; break;
               }
            }
         }
         else
         {
            int coef_num = rh265_min(64, 1 << (4 + (size_id << 1)));
            const uint8_t *scan = (size_id == 0)
                  ? rh265_diag4 : rh265_diag8;
            int next = 8;
            int i;
            if (size_id > 1)
            {
               int dc = 8 + (int)rh265_se(b);
               if (dc < 1 || dc > 255)
                  return -1;
               next = dc;
               if (size_id == 2)
                  sl->dc16[matrix_id] = (uint8_t)dc;
               else
                  sl->dc32[matrix_id] = (uint8_t)dc;
            }
            for (i = 0; i < coef_num; i++)
            {
               int delta = (int)rh265_se(b);
               if (delta < -128 || delta > 127)
                  return -1;
               next = (next + delta + 256) % 256;
               if (next == 0)
                  return -1;      /* zero scaling coefficients invalid */
               dst[scan[i]] = (uint8_t)next;
            }
         }
      }
   return rh265_bits_overrun(b) ? -1 : 0;
}

/* profile_tier_level: fixed layout; nothing in it is needed to decode,
 * so parse it for its exact size only (7.3.3). */
static void rh265_skip_ptl(rh265_bits *b, int max_sub_layers_minus1)
{
   int i;
   int sub_layer_profile_present[7];
   int sub_layer_level_present[7];
   rh265_un(b, 8);                       /* profile space/tier/idc */
   rh265_un(b, 32);                      /* compatibility flags */
   rh265_un(b, 32); rh265_un(b, 16);     /* progressive..reserved 44 bits */
   rh265_un(b, 8);                       /* general_level_idc */
   for (i = 0; i < max_sub_layers_minus1; i++)
   {
      sub_layer_profile_present[i] = (int)rh265_u1(b);
      sub_layer_level_present[i]   = (int)rh265_u1(b);
   }
   if (max_sub_layers_minus1 > 0)
      for (i = max_sub_layers_minus1; i < 8; i++)
         rh265_un(b, 2);
   for (i = 0; i < max_sub_layers_minus1; i++)
   {
      if (sub_layer_profile_present[i])
      {
         rh265_un(b, 8); rh265_un(b, 32);
         rh265_un(b, 32); rh265_un(b, 16);
      }
      if (sub_layer_level_present[i])
         rh265_un(b, 8);
   }
}


/* st_ref_pic_set (7.3.7), resolving inter-RPS prediction against the
 * previously parsed sets so every set is stored in explicit form. */
static int rh265_parse_st_rps(rh265_bits *b, const rh265_sps *s, int idx,
      rh265_st_rps *rps, int is_slice_header)
{
   int inter_pred = 0;
   if (idx > 0)
      inter_pred = (int)rh265_u1(b);   /* inter_ref_pic_set_prediction_flag */
   if (inter_pred)
   {
      const rh265_st_rps *ref;
      int delta_idx = 1, delta_rps_sign, abs_delta_rps_minus1, delta_rps;
      int num_delta_pocs;
      int32_t s0[RH265_MAX_REFS], s1[RH265_MAX_REFS];
      uint8_t u0[RH265_MAX_REFS], u1[RH265_MAX_REFS];
      int n0 = 0, n1 = 0;
      if (is_slice_header)
         delta_idx = (int)rh265_ue(b) + 1;
      if (delta_idx > idx) return -1;
      ref = &s->st_rps[idx - delta_idx];
      delta_rps_sign       = (int)rh265_u1(b);
      abs_delta_rps_minus1 = (int)rh265_ue(b);
      delta_rps = (1 - 2 * delta_rps_sign) * (abs_delta_rps_minus1 + 1);
      num_delta_pocs = ref->num_negative + ref->num_positive;
      if (num_delta_pocs > RH265_MAX_REFS * 2) return -1;
      /* 7.4.8: build the new set from used_by_curr/use_delta flags */
      {
         int use[RH265_MAX_REFS * 2 + 1];
         int used_by[RH265_MAX_REFS * 2 + 1];
         int k;
         for (k = 0; k <= num_delta_pocs; k++)
         {
            used_by[k] = (int)rh265_u1(b);
            use[k] = used_by[k] ? 1 : (int)rh265_u1(b);
         }
         /* negative half: iterate ref positive high->low then delta itself,
          * then ref negative (spec 7.4.8 derivation) */
         for (k = ref->num_positive - 1; k >= 0; k--)
         {
            int32_t d = ref->delta_poc_s1[k] + delta_rps;
            int ri = ref->num_negative + k;
            if (d < 0 && use[ri])
            { if (n0 >= RH265_MAX_REFS) return -1;
              s0[n0] = d; u0[n0] = (uint8_t)used_by[ri]; n0++; }
         }
         if (delta_rps < 0 && use[num_delta_pocs])
         { if (n0 >= RH265_MAX_REFS) return -1;
           s0[n0] = delta_rps; u0[n0] = (uint8_t)used_by[num_delta_pocs]; n0++; }
         for (k = 0; k < ref->num_negative; k++)
         {
            int32_t d = ref->delta_poc_s0[k] + delta_rps;
            if (d < 0 && use[k])
            { if (n0 >= RH265_MAX_REFS) return -1;
              s0[n0] = d; u0[n0] = (uint8_t)used_by[k]; n0++; }
         }
         /* positive half */
         for (k = ref->num_negative - 1; k >= 0; k--)
         {
            int32_t d = ref->delta_poc_s0[k] + delta_rps;
            if (d > 0 && use[k])
            { if (n1 >= RH265_MAX_REFS) return -1;
              s1[n1] = d; u1[n1] = (uint8_t)used_by[k]; n1++; }
         }
         if (delta_rps > 0 && use[num_delta_pocs])
         { if (n1 >= RH265_MAX_REFS) return -1;
           s1[n1] = delta_rps; u1[n1] = (uint8_t)used_by[num_delta_pocs]; n1++; }
         for (k = 0; k < ref->num_positive; k++)
         {
            int32_t d = ref->delta_poc_s1[k] + delta_rps;
            int ri = ref->num_negative + k;
            if (d > 0 && use[ri])
            { if (n1 >= RH265_MAX_REFS) return -1;
              s1[n1] = d; u1[n1] = (uint8_t)used_by[ri]; n1++; }
         }
      }
      memcpy(rps->delta_poc_s0, s0, sizeof(s0));
      memcpy(rps->delta_poc_s1, s1, sizeof(s1));
      memcpy(rps->used_s0, u0, sizeof(u0));
      memcpy(rps->used_s1, u1, sizeof(u1));
      rps->num_negative = n0;
      rps->num_positive = n1;
   }
   else
   {
      int num_neg = (int)rh265_ue(b);
      int num_pos = (int)rh265_ue(b);
      int i;
      int32_t prev;
      if (num_neg > RH265_MAX_REFS || num_pos > RH265_MAX_REFS) return -1;
      prev = 0;
      for (i = 0; i < num_neg; i++)
      {
         int32_t d = (int32_t)rh265_ue(b) + 1;
         prev -= d;
         rps->delta_poc_s0[i] = prev;
         rps->used_s0[i] = (uint8_t)rh265_u1(b);
      }
      prev = 0;
      for (i = 0; i < num_pos; i++)
      {
         int32_t d = (int32_t)rh265_ue(b) + 1;
         prev += d;
         rps->delta_poc_s1[i] = prev;
         rps->used_s1[i] = (uint8_t)rh265_u1(b);
      }
      rps->num_negative = num_neg;
      rps->num_positive = num_pos;
   }
   return 0;
}

static int rh265_parse_sps(const uint8_t *rbsp, size_t size, rh265_sps *s,
      int *out_id)
{
   rh265_bits b;
   int max_sub_layers_minus1, sps_id, i;
   int conformance_window;
   memset(s, 0, sizeof(*s));
   rh265_bits_init(&b, rbsp, size);
   rh265_un(&b, 4);                      /* sps_video_parameter_set_id */
   max_sub_layers_minus1 = (int)rh265_un(&b, 3);
   rh265_u1(&b);                         /* sps_temporal_id_nesting_flag */
   rh265_skip_ptl(&b, max_sub_layers_minus1);
   sps_id = (int)rh265_ue(&b);
   if (sps_id >= RH265_MAX_SPS) return -1;
   s->chroma_format_idc = (int)rh265_ue(&b);
   if (s->chroma_format_idc == 3)
      rh265_u1(&b);                      /* separate_colour_plane_flag */
   s->width  = (int)rh265_ue(&b);
   s->height = (int)rh265_ue(&b);
   if (s->width <= 0 || s->height <= 0 ||
       s->width > 16888 || s->height > 16888)
      return -1;
   conformance_window = (int)rh265_u1(&b);
   if (conformance_window)
   {
      s->crop_left   = (int)rh265_ue(&b);
      s->crop_right  = (int)rh265_ue(&b);
      s->crop_top    = (int)rh265_ue(&b);
      s->crop_bottom = (int)rh265_ue(&b);
   }
   s->bit_depth_luma   = (int)rh265_ue(&b) + 8;
   s->bit_depth_chroma = (int)rh265_ue(&b) + 8;
   s->log2_max_poc_lsb = (int)rh265_ue(&b) + 4;
   if (s->log2_max_poc_lsb > 16) return -1;
   {
      int sub_layer_ordering = (int)rh265_u1(&b);
      int first = sub_layer_ordering ? 0 : max_sub_layers_minus1;
      for (i = first; i <= max_sub_layers_minus1; i++)
      {
         s->max_dec_pic_buffering = (int)rh265_ue(&b) + 1;
         s->max_num_reorder_pics  = (int)rh265_ue(&b);
         rh265_ue(&b);                   /* sps_max_latency_increase_plus1 */
      }
   }
   s->log2_min_cb = (int)rh265_ue(&b) + 3;
   s->log2_ctb    = s->log2_min_cb + (int)rh265_ue(&b);
   s->log2_min_tb = (int)rh265_ue(&b) + 2;
   s->log2_max_tb = s->log2_min_tb + (int)rh265_ue(&b);
   if (s->log2_min_cb < 3 || s->log2_ctb > 6 || s->log2_ctb < s->log2_min_cb ||
       s->log2_min_tb < 2 || s->log2_max_tb > 5 ||
       s->log2_min_tb >= s->log2_min_cb)
      return -1;
   s->max_transform_hierarchy_depth_inter = (int)rh265_ue(&b);
   s->max_transform_hierarchy_depth_intra = (int)rh265_ue(&b);
   s->scaling_list_enabled = (int)rh265_u1(&b);
   if (s->scaling_list_enabled)
      rh265_sl_defaults(&s->sl);
   if (s->scaling_list_enabled)
   {
      if (rh265_u1(&b))                  /* sps_scaling_list_data_present */
      {
         if (rh265_parse_scaling_list_data(&b, &s->sl) < 0)
            return -1;
      }
   }
   s->amp_enabled = (int)rh265_u1(&b);
   s->sao_enabled = (int)rh265_u1(&b);
   s->pcm_enabled = (int)rh265_u1(&b);
   if (s->pcm_enabled)
   {
      rh265_un(&b, 4); rh265_un(&b, 4);  /* pcm bit depths */
      rh265_ue(&b); rh265_ue(&b);        /* pcm cb sizes */
      rh265_u1(&b);                      /* pcm_loop_filter_disabled */
   }
   s->num_st_rps = (int)rh265_ue(&b);
   if (s->num_st_rps > 64) return -1;
   for (i = 0; i < s->num_st_rps; i++)
      if (rh265_parse_st_rps(&b, s, i, &s->st_rps[i], 0) < 0)
         return -1;
   s->long_term_ref_pics_present = (int)rh265_u1(&b);
   if (s->long_term_ref_pics_present)
   {
      s->num_long_term_ref_pics = (int)rh265_ue(&b);
      if (s->num_long_term_ref_pics > 32) return -1;
      for (i = 0; i < s->num_long_term_ref_pics; i++)
      {
         s->lt_ref_poc_lsb[i]   = (int)rh265_un(&b, s->log2_max_poc_lsb);
         s->lt_used_by_curr[i]  = (int)rh265_u1(&b);
      }
   }
   s->temporal_mvp_enabled   = (int)rh265_u1(&b);
   s->strong_intra_smoothing = (int)rh265_u1(&b);
   /* vui_parameters and sps extensions carry nothing this decoder needs;
    * stop parsing here. */
   if (rh265_bits_overrun(&b)) return -1;

   /* pic dims must be multiples of MinCbSizeY (A.4.1) */
   if ((s->width  & ((1 << s->log2_min_cb) - 1)) ||
       (s->height & ((1 << s->log2_min_cb) - 1)))
      return -1;

   s->ctb_w = (s->width  + (1 << s->log2_ctb) - 1) >> s->log2_ctb;
   s->ctb_h = (s->height + (1 << s->log2_ctb) - 1) >> s->log2_ctb;
   s->pic_size_ctbs = s->ctb_w * s->ctb_h;
   s->valid = 1;
   *out_id = sps_id;
   return 0;
}

static int rh265_parse_pps(const uint8_t *rbsp, size_t size, rh265_pps *p,
      int *out_id)
{
   rh265_bits b;
   int pps_id;
   memset(p, 0, sizeof(*p));
   rh265_bits_init(&b, rbsp, size);
   pps_id = (int)rh265_ue(&b);
   if (pps_id >= RH265_MAX_PPS) return -1;
   p->sps_id = (int)rh265_ue(&b);
   if (p->sps_id >= RH265_MAX_SPS) return -1;
   p->dependent_slice_segments_enabled = (int)rh265_u1(&b);
   p->output_flag_present              = (int)rh265_u1(&b);
   p->num_extra_slice_header_bits      = (int)rh265_un(&b, 3);
   p->sign_data_hiding                 = (int)rh265_u1(&b);
   p->cabac_init_present               = (int)rh265_u1(&b);
   p->num_ref_idx_l0_default           = (int)rh265_ue(&b) + 1;
   p->num_ref_idx_l1_default           = (int)rh265_ue(&b) + 1;
   p->init_qp                          = 26 + (int)rh265_se(&b);
   p->constrained_intra_pred           = (int)rh265_u1(&b);
   p->transform_skip_enabled           = (int)rh265_u1(&b);
   p->cu_qp_delta_enabled              = (int)rh265_u1(&b);
   if (p->cu_qp_delta_enabled)
      p->diff_cu_qp_delta_depth        = (int)rh265_ue(&b);
   p->cb_qp_offset                     = (int)rh265_se(&b);
   p->cr_qp_offset                     = (int)rh265_se(&b);
   p->slice_chroma_qp_offsets_present  = (int)rh265_u1(&b);
   p->weighted_pred                    = (int)rh265_u1(&b);
   p->weighted_bipred                  = (int)rh265_u1(&b);
   p->transquant_bypass_enabled        = (int)rh265_u1(&b);
   p->tiles_enabled                    = (int)rh265_u1(&b);
   p->entropy_coding_sync_enabled      = (int)rh265_u1(&b);
   if (p->tiles_enabled)
      return -2;      /* refused: tile column/row parsing not implemented */
   p->loop_filter_across_slices        = (int)rh265_u1(&b);
   p->deblocking_filter_control_present = (int)rh265_u1(&b);
   if (p->deblocking_filter_control_present)
   {
      p->deblocking_filter_override_enabled = (int)rh265_u1(&b);
      p->deblocking_filter_disabled         = (int)rh265_u1(&b);
      if (!p->deblocking_filter_disabled)
      {
         p->beta_offset_div2 = (int)rh265_se(&b);
         p->tc_offset_div2   = (int)rh265_se(&b);
      }
   }
   if ((p->has_scaling = (int)rh265_u1(&b)))
   {
      rh265_sl_defaults(&p->sl);
      if (rh265_parse_scaling_list_data(&b, &p->sl) < 0)
         return -1;
   }
   p->lists_modification_present       = (int)rh265_u1(&b);
   p->log2_parallel_merge_level        = (int)rh265_ue(&b) + 2;
   p->slice_segment_header_extension_present = (int)rh265_u1(&b);
   if (rh265_bits_overrun(&b)) return -1;
   p->valid = 1;
   *out_id = pps_id;
   return 0;
}

/* ==================== rh265_slice.h ==================== */

#define RH265_SLICE_B 0
#define RH265_SLICE_P 1
#define RH265_SLICE_I 2

typedef struct
{
   int first_slice_in_pic;
   int pps_id;
   int slice_segment_address;
   int slice_type;
   int pic_output_flag;
   int colour_plane_id;
   int poc_lsb;
   rh265_st_rps rps;          /* resolved RPS for this slice */
   int short_term_rps_sps_idx;   /* -1 when coded in the slice header */
   int sao_luma, sao_chroma;
   int slice_qp;              /* SliceQpY */
   int cb_qp_offset, cr_qp_offset;   /* slice-level */
   int deblocking_filter_disabled;
   int beta_offset_div2, tc_offset_div2;
   int loop_filter_across_slices;

   /* P/B slices */
   int slice_tmvp;            /* slice_temporal_mvp_enabled_flag */
   int nb_refs[2];            /* num_ref_idx_lX_active */
   int rpl_mod[2];            /* ref_pic_list_modification_flag_lX */
   uint8_t list_entry[2][16];
   int mvd_l1_zero;
   int cabac_init;
   int collocated_from_l0;
   int collocated_ref_idx;
   int max_merge;             /* MaxNumMergeCand */
   /* pred_weight_table (weights already folded to (1<<denom) defaults) */
   int luma_log2_denom, chroma_log2_denom;
   int16_t luma_w[2][16], luma_o[2][16];
   int16_t chroma_w[2][16][2], chroma_o[2][16][2];

   /* WPP substream entry points: offsets between consecutive
    * substreams in escaped slice-data bytes (7.4.7.1) */
   int      num_entry_points;
   uint32_t entry_point[RH265_MAX_ENTRY_POINTS];
} rh265_shdr;

/* 7.3.6.3 pred_weight_table.  Weights fold to (1 << denom) and offsets
 * to 0 for references whose flag is absent, so the prediction stage can
 * apply the weighted formulas unconditionally when weighting is on. */
static int rh265_parse_pred_weight_table(rh265_bits *b, rh265_shdr *sh,
      int is_b)
{
   int list, i, c;
   int nlists = is_b ? 2 : 1;
   uint8_t lflag[2][16], cflag[2][16];
   sh->luma_log2_denom = (int)rh265_ue(b);
   if (sh->luma_log2_denom > 7) return -1;
   sh->chroma_log2_denom = sh->luma_log2_denom + (int)rh265_se(b);
   if (sh->chroma_log2_denom < 0 || sh->chroma_log2_denom > 7) return -1;
   for (list = 0; list < nlists; list++)
      for (i = 0; i < sh->nb_refs[list]; i++)
         lflag[list][i] = (uint8_t)rh265_u1(b);
   for (list = 0; list < nlists; list++)
      for (i = 0; i < sh->nb_refs[list]; i++)
         cflag[list][i] = (uint8_t)rh265_u1(b);
   for (list = 0; list < nlists; list++)
      for (i = 0; i < sh->nb_refs[list]; i++)
      {
         sh->luma_w[list][i] = (int16_t)(1 << sh->luma_log2_denom);
         sh->luma_o[list][i] = 0;
         if (lflag[list][i])
         {
            int dw = (int)rh265_se(b);
            int o  = (int)rh265_se(b);
            if (dw < -128 || dw > 127 || o < -128 || o > 127) return -1;
            sh->luma_w[list][i] = (int16_t)((1 << sh->luma_log2_denom) + dw);
            sh->luma_o[list][i] = (int16_t)o;
         }
         for (c = 0; c < 2; c++)
         {
            sh->chroma_w[list][i][c] =
                  (int16_t)(1 << sh->chroma_log2_denom);
            sh->chroma_o[list][i][c] = 0;
         }
         if (cflag[list][i])
            for (c = 0; c < 2; c++)
            {
               int dw = (int)rh265_se(b);
               int dof = (int)rh265_se(b);
               int w, o;
               if (dw < -128 || dw > 127) return -1;
               w = (1 << sh->chroma_log2_denom) + dw;
               /* 7.4.7.3: offset reconstructed against the midpoint */
               o = 128 + dof - ((128 * w) >> sh->chroma_log2_denom);
               if (o < -128 || o > 127) return -1;
               sh->chroma_w[list][i][c] = (int16_t)w;
               sh->chroma_o[list][i][c] = (int16_t)o;
            }
      }
   return 0;
}

/* Parse one slice segment header.  Returns 0 on success, -1 on
 * malformed data, -2 on valid-but-unsupported. */
static int rh265_parse_slice_header(rh265_bits *b, int nal_type,
      const rh265_sps *s, const rh265_pps *p, rh265_pps *pps_tab,
      rh265_sps *sps_tab, rh265_shdr *sh)
{
   int i;
   memset(sh, 0, sizeof(*sh));
   sh->short_term_rps_sps_idx = -1;
   sh->first_slice_in_pic = (int)rh265_u1(b);
   if (RH265_IS_IRAP(nal_type))
      rh265_u1(b);                      /* no_output_of_prior_pics_flag */
   sh->pps_id = (int)rh265_ue(b);
   if (sh->pps_id >= RH265_MAX_PPS || !pps_tab[sh->pps_id].valid)
      return -1;
   p = &pps_tab[sh->pps_id];
   s = &sps_tab[p->sps_id];
   if (!s->valid) return -1;
   if (!sh->first_slice_in_pic)
   {
      int addr_bits;
      if (p->dependent_slice_segments_enabled && rh265_u1(b))
         return -2;                     /* dependent slice segments refused */
      addr_bits = 0;
      while ((1 << addr_bits) < s->pic_size_ctbs) addr_bits++;
      sh->slice_segment_address = (int)rh265_un(b, addr_bits);
      if (sh->slice_segment_address >= s->pic_size_ctbs) return -1;
   }
   for (i = 0; i < p->num_extra_slice_header_bits; i++)
      rh265_u1(b);
   sh->slice_type = (int)rh265_ue(b);
   if (sh->slice_type > 2) return -1;
   sh->pic_output_flag = 1;
   if (p->output_flag_present)
      sh->pic_output_flag = (int)rh265_u1(b);
   if (!RH265_IS_IDR(nal_type))
   {
      sh->poc_lsb = (int)rh265_un(b, s->log2_max_poc_lsb);
      if (!rh265_u1(b))                 /* short_term_ref_pic_set_sps_flag */
      {
         /* parse an explicit set at virtual index num_st_rps; inter-set
          * prediction only reads the sets already stored in the SPS, so
          * the SPS is passed directly (and never written) */
         if (rh265_parse_st_rps(b, s, s->num_st_rps, &sh->rps, 1) < 0)
            return -1;
      }
      else if (s->num_st_rps > 0)
      {
         int idx = 0, nbits = 0;
         while ((1 << nbits) < s->num_st_rps) nbits++;
         if (nbits)
            idx = (int)rh265_un(b, nbits);
         if (idx >= s->num_st_rps) return -1;
         sh->short_term_rps_sps_idx = idx;
         memcpy(&sh->rps, &s->st_rps[idx], sizeof(sh->rps));
      }
      if (s->long_term_ref_pics_present)
         return -2;                     /* long-term refs: not supported */
      if (s->temporal_mvp_enabled)
         sh->slice_tmvp = (int)rh265_u1(b);
   }
   if (s->sao_enabled)
   {
      sh->sao_luma   = (int)rh265_u1(b);
      sh->sao_chroma = (int)rh265_u1(b);
   }
   if (sh->slice_type != RH265_SLICE_I)
   {
      int is_b = (sh->slice_type == RH265_SLICE_B);
      int num_pics_total_curr = 0;
      sh->nb_refs[0] = p->num_ref_idx_l0_default;
      sh->nb_refs[1] = is_b ? p->num_ref_idx_l1_default : 0;
      if (rh265_u1(b))                  /* num_ref_idx_active_override */
      {
         sh->nb_refs[0] = (int)rh265_ue(b) + 1;
         if (is_b)
            sh->nb_refs[1] = (int)rh265_ue(b) + 1;
      }
      if (sh->nb_refs[0] > RH265_MAX_REFS ||
          sh->nb_refs[1] > RH265_MAX_REFS)
         return -1;
      for (i = 0; i < sh->rps.num_negative; i++)
         if (sh->rps.used_s0[i])
            num_pics_total_curr++;
      for (i = 0; i < sh->rps.num_positive; i++)
         if (sh->rps.used_s1[i])
            num_pics_total_curr++;
      if (p->lists_modification_present && num_pics_total_curr > 1)
      {
         int nbits = 0, list, j;
         while ((1 << nbits) < num_pics_total_curr) nbits++;
         for (list = 0; list < (is_b ? 2 : 1); list++)
         {
            sh->rpl_mod[list] = (int)rh265_u1(b);
            if (sh->rpl_mod[list])
               for (j = 0; j < sh->nb_refs[list]; j++)
               {
                  int e = (int)rh265_un(b, nbits);
                  if (e >= num_pics_total_curr) return -1;
                  sh->list_entry[list][j] = (uint8_t)e;
               }
         }
      }
      if (is_b)
         sh->mvd_l1_zero = (int)rh265_u1(b);
      if (p->cabac_init_present)
         sh->cabac_init = (int)rh265_u1(b);
      sh->collocated_from_l0 = 1;
      if (sh->slice_tmvp)
      {
         if (is_b)
            sh->collocated_from_l0 = (int)rh265_u1(b);
         if ((sh->collocated_from_l0 && sh->nb_refs[0] > 1) ||
             (!sh->collocated_from_l0 && sh->nb_refs[1] > 1))
         {
            sh->collocated_ref_idx = (int)rh265_ue(b);
            if (sh->collocated_ref_idx >=
                  sh->nb_refs[sh->collocated_from_l0 ? 0 : 1])
               return -1;
         }
      }
      if ((p->weighted_pred && sh->slice_type == RH265_SLICE_P) ||
          (p->weighted_bipred && is_b))
      {
         if (rh265_parse_pred_weight_table(b, sh, is_b) < 0)
            return -1;
      }
      sh->max_merge = 5 - (int)rh265_ue(b);
      if (sh->max_merge < 1 || sh->max_merge > 5)
         return -1;
   }
   sh->slice_qp = p->init_qp + (int)rh265_se(b);
   if (sh->slice_qp < 0 || sh->slice_qp > 51) return -1;
   if (p->slice_chroma_qp_offsets_present)
   {
      sh->cb_qp_offset = (int)rh265_se(b);
      sh->cr_qp_offset = (int)rh265_se(b);
   }
   sh->deblocking_filter_disabled = p->deblocking_filter_disabled;
   sh->beta_offset_div2 = p->beta_offset_div2;
   sh->tc_offset_div2   = p->tc_offset_div2;
   {
      int override = 0;
      if (p->deblocking_filter_override_enabled)
         override = (int)rh265_u1(b);
      if (override)
      {
         sh->deblocking_filter_disabled = (int)rh265_u1(b);
         if (!sh->deblocking_filter_disabled)
         {
            sh->beta_offset_div2 = (int)rh265_se(b);
            sh->tc_offset_div2   = (int)rh265_se(b);
         }
      }
   }
   sh->loop_filter_across_slices = p->loop_filter_across_slices;
   if (p->loop_filter_across_slices &&
       (sh->sao_luma || sh->sao_chroma || !sh->deblocking_filter_disabled))
      sh->loop_filter_across_slices = (int)rh265_u1(b);
   sh->num_entry_points = 0;
   if (p->entropy_coding_sync_enabled)
   {
      /* tiles are refused at the PPS, so every entry point marks a CTB
       * row boundary within the slice */
      int n = (int)rh265_ue(b);
      if (n < 0 || n > RH265_MAX_ENTRY_POINTS)
         return -1;
      if (n > 0)
      {
         int olen = (int)rh265_ue(b) + 1;
         if (olen < 1 || olen > 32)
            return -1;
         sh->num_entry_points = n;
         for (i = 0; i < n; i++)
            sh->entry_point[i] = rh265_un(b, olen) + 1;
      }
   }
   if (p->slice_segment_header_extension_present)
   {
      int len = (int)rh265_ue(b);
      for (i = 0; i < len; i++)
         rh265_un(b, 8);
   }
   /* byte_alignment() */
   if (!rh265_u1(b)) return -1;         /* alignment_bit_equal_to_one */
   while (b->bitpos & 7)
      rh265_u1(b);
   if (rh265_bits_overrun(b)) return -1;
   return 0;
}

/* ==================== rh265_cabac_tables.h ==================== */
/* CABAC core tables (H.265 clause 9.3.4.3).  The arithmetic engine is
 * the same M-coder as H.264 (identical rangeTabLPS and state
 * transitions); the tables and the engine below are ported from
 * rh264.c. */

/* Table 9-46: rangeTabLps[pStateIdx][qRangeIdx] */
static const uint8_t rh265_rangeTabLPS[64][4]={
 {128,176,208,240},{128,167,197,227},{128,158,187,216},{123,150,178,205},
 {116,142,169,195},{111,135,160,185},{105,128,152,175},{100,122,144,166},
 { 95,116,137,158},{ 90,110,130,150},{ 85,104,123,142},{ 81, 99,117,135},
 { 77, 94,111,128},{ 73, 89,105,122},{ 69, 85,100,116},{ 66, 80, 95,110},
 { 62, 76, 90,104},{ 59, 72, 86, 99},{ 56, 69, 81, 94},{ 53, 65, 77, 89},
 { 51, 62, 73, 85},{ 48, 59, 69, 80},{ 46, 56, 66, 76},{ 43, 53, 63, 72},
 { 41, 50, 59, 69},{ 39, 48, 56, 65},{ 37, 45, 54, 62},{ 35, 43, 51, 59},
 { 33, 41, 48, 56},{ 32, 39, 46, 53},{ 30, 37, 43, 50},{ 29, 35, 41, 48},
 { 27, 33, 39, 45},{ 26, 31, 37, 43},{ 24, 30, 35, 41},{ 23, 28, 33, 39},
 { 22, 27, 32, 37},{ 21, 26, 30, 35},{ 20, 24, 29, 33},{ 19, 23, 27, 31},
 { 18, 22, 26, 30},{ 17, 21, 25, 28},{ 16, 20, 23, 27},{ 15, 19, 22, 25},
 { 14, 18, 21, 24},{ 14, 17, 20, 23},{ 13, 16, 19, 22},{ 12, 15, 18, 21},
 { 12, 14, 17, 20},{ 11, 14, 16, 19},{ 11, 13, 15, 18},{ 10, 12, 15, 17},
 { 10, 12, 14, 16},{  9, 11, 13, 15},{  9, 11, 12, 14},{  8, 10, 12, 14},
 {  8,  9, 11, 13},{  7,  9, 11, 12},{  7,  9, 10, 12},{  7,  8, 10, 11},
 {  6,  8,  9, 11},{  6,  7,  9, 10},{  6,  7,  8,  9},{  2,  2,  2,  2}};

/* Table 9-47: transIdxLps / transIdxMps */
static const uint8_t rh265_transIdxLPS[64]={
  0, 0, 1, 2, 2, 4, 4, 5, 6, 7, 8, 9, 9,11,11,12,
 13,13,15,15,16,16,18,18,19,19,21,21,22,22,23,24,
 24,25,26,26,27,27,28,29,29,30,30,30,31,32,32,33,
 33,33,34,34,35,35,35,36,36,36,37,37,37,38,38,63};
static const uint8_t rh265_transIdxMPS[64]={
  1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,
 17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,
 33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,
 49,50,51,52,53,54,55,56,57,58,59,60,61,62,62,63};

/* Context ordering and initValue tables extracted programmatically from
 * FFmpeg libavcodec/hevc/cabac.c (init_values[3][HEVC_CONTEXTS]); the
 * values are the initValue entries of ITU-T H.265 tables 9-5..9-31 laid
 * out per syntax element, indexed by initType 0..2.  No table is
 * hand-transcribed. */
enum
{
   RH265_CTX_SAO_MERGE_FLAG = 0,
   RH265_CTX_SAO_TYPE_IDX = 1,
   RH265_CTX_SAO_EO_CLASS = 2,
   RH265_CTX_SAO_BAND_POSITION = 2,
   RH265_CTX_SAO_OFFSET_ABS = 2,
   RH265_CTX_SAO_OFFSET_SIGN = 2,
   RH265_CTX_END_OF_SLICE_FLAG = 2,
   RH265_CTX_SPLIT_CODING_UNIT_FLAG = 2,
   RH265_CTX_CU_TRANSQUANT_BYPASS_FLAG = 5,
   RH265_CTX_SKIP_FLAG = 6,
   RH265_CTX_CU_QP_DELTA = 9,
   RH265_CTX_PRED_MODE_FLAG = 12,
   RH265_CTX_PART_MODE = 13,
   RH265_CTX_PCM_FLAG = 17,
   RH265_CTX_PREV_INTRA_LUMA_PRED_FLAG = 17,
   RH265_CTX_MPM_IDX = 18,
   RH265_CTX_REM_INTRA_LUMA_PRED_MODE = 18,
   RH265_CTX_INTRA_CHROMA_PRED_MODE = 18,
   RH265_CTX_MERGE_FLAG = 20,
   RH265_CTX_MERGE_IDX = 21,
   RH265_CTX_INTER_PRED_IDC = 22,
   RH265_CTX_REF_IDX_L0 = 27,
   RH265_CTX_REF_IDX_L1 = 29,
   RH265_CTX_ABS_MVD_GREATER0_FLAG = 31,
   RH265_CTX_ABS_MVD_GREATER1_FLAG = 33,
   RH265_CTX_ABS_MVD_MINUS2 = 35,
   RH265_CTX_MVD_SIGN_FLAG = 35,
   RH265_CTX_MVP_LX_FLAG = 35,
   RH265_CTX_NO_RESIDUAL_DATA_FLAG = 36,
   RH265_CTX_SPLIT_TRANSFORM_FLAG = 37,
   RH265_CTX_CBF_LUMA = 40,
   RH265_CTX_CBF_CB_CR = 42,
   RH265_CTX_TRANSFORM_SKIP_FLAG = 47,
   RH265_CTX_EXPLICIT_RDPCM_FLAG = 49,
   RH265_CTX_EXPLICIT_RDPCM_DIR_FLAG = 51,
   RH265_CTX_LAST_SIGNIFICANT_COEFF_X_PREFIX = 53,
   RH265_CTX_LAST_SIGNIFICANT_COEFF_Y_PREFIX = 71,
   RH265_CTX_LAST_SIGNIFICANT_COEFF_X_SUFFIX = 89,
   RH265_CTX_LAST_SIGNIFICANT_COEFF_Y_SUFFIX = 89,
   RH265_CTX_SIGNIFICANT_COEFF_GROUP_FLAG = 89,
   RH265_CTX_SIGNIFICANT_COEFF_FLAG = 93,
   RH265_CTX_COEFF_ABS_LEVEL_GREATER1_FLAG = 137,
   RH265_CTX_COEFF_ABS_LEVEL_GREATER2_FLAG = 161,
   RH265_CTX_COEFF_ABS_LEVEL_REMAINING = 167,
   RH265_CTX_COEFF_SIGN_FLAG = 167,
   RH265_CTX_LOG2_RES_SCALE_ABS = 167,
   RH265_CTX_RES_SCALE_SIGN_FLAG = 175,
   RH265_CTX_CU_CHROMA_QP_OFFSET_FLAG = 177,
   RH265_CTX_CU_CHROMA_QP_OFFSET_IDX = 178,
   RH265_CTX_COUNT = 179
};

static const uint8_t rh265_ctx_init[3][RH265_CTX_COUNT]={
   {
   153,200,139,141,157,154,154,154,154,154,154,154,154,184,154,
   154,154,184,63,139,154,154,154,154,154,154,154,154,154,154,
   154,154,154,154,154,154,154,153,138,138,111,141,94,138,182,
   154,154,139,139,139,139,139,139,110,110,124,125,140,153,125,
   127,140,109,111,143,127,111,79,108,123,63,110,110,124,125,
   140,153,125,127,140,109,111,143,127,111,79,108,123,63,91,
   171,134,141,111,111,125,110,110,94,124,108,124,107,125,141,
   179,153,125,107,125,141,179,153,125,107,125,141,179,153,125,
   140,139,182,182,152,136,152,136,153,136,139,111,136,139,111,
   141,111,140,92,137,138,140,152,138,139,153,74,149,92,139,
   107,122,152,140,179,166,182,140,227,122,197,138,153,136,167,
   152,152,154,154,154,154,154,154,154,154,154,154,154,154
   },
   {
   153,185,107,139,126,154,197,185,201,154,154,154,149,154,139,
   154,154,154,152,139,110,122,95,79,63,31,31,153,153,153,
   153,140,198,140,198,168,79,124,138,94,153,111,149,107,167,
   154,154,139,139,139,139,139,139,125,110,94,110,95,79,125,
   111,110,78,110,111,111,95,94,108,123,108,125,110,94,110,
   95,79,125,111,110,78,110,111,111,95,94,108,123,108,121,
   140,61,154,155,154,139,153,139,123,123,63,153,166,183,140,
   136,153,154,166,183,140,136,153,154,166,183,140,136,153,154,
   170,153,123,123,107,121,107,121,167,151,183,140,151,183,140,
   140,140,154,196,196,167,154,152,167,182,182,134,149,136,153,
   121,136,137,169,194,166,167,154,167,137,182,107,167,91,122,
   107,167,154,154,154,154,154,154,154,154,154,154,154,154
   },
   {
   153,160,107,139,126,154,197,185,201,154,154,154,134,154,139,
   154,154,183,152,139,154,137,95,79,63,31,31,153,153,153,
   153,169,198,169,198,168,79,224,167,122,153,111,149,92,167,
   154,154,139,139,139,139,139,139,125,110,124,110,95,94,125,
   111,111,79,125,126,111,111,79,108,123,93,125,110,124,110,
   95,94,125,111,111,79,125,126,111,111,79,108,123,93,121,
   140,61,154,170,154,139,153,139,123,123,63,124,166,183,140,
   136,153,154,166,183,140,136,153,154,166,183,140,136,153,154,
   170,153,138,138,122,121,122,121,167,151,183,140,151,183,140,
   140,140,154,196,167,167,154,152,167,182,182,134,149,136,153,
   121,136,122,169,208,166,167,154,152,167,182,107,167,91,107,
   107,167,154,154,154,154,154,154,154,154,154,154,154,154
   },
};

/* ---- rh265_cabac.h ---- */

typedef struct
{
   const uint8_t *buf, *end;
   uint32_t range;                       /* ivlCurrRange */
   uint32_t offset;                      /* ivlOffset */
   uint32_t bitbuf;
   int      bitcnt;
   uint8_t  state[RH265_CTX_COUNT];
   uint8_t  mps[RH265_CTX_COUNT];
} rh265_cabac;

static int rh265_cb_bit(rh265_cabac *c)
{
   int b;
   if (c->bitcnt == 0)
   {
      c->bitbuf = (uint32_t)((c->buf < c->end) ? *c->buf++ : 0);
      c->bitcnt = 8;
   }
   c->bitcnt--;
   b = (int)((c->bitbuf >> c->bitcnt) & 1);
   return b;
}

/* Pull n (1..7) bits at once for renormalisation: at most one byte of
 * refill is ever needed since bitcnt < 8 going in.  Bytes past the end
 * read as zero, matching rh265_cb_bit. */
static RH265_INLINE uint32_t rh265_cb_bits(rh265_cabac *c, int n)
{
   if (c->bitcnt < n)
   {
      c->bitbuf = (c->bitbuf << 8)
            | (uint32_t)((c->buf < c->end) ? *c->buf++ : 0);
      c->bitcnt += 8;
   }
   c->bitcnt -= n;
   return (c->bitbuf >> c->bitcnt) & ((1u << n) - 1u);
}

static void rh265_cabac_init_engine(rh265_cabac *c,
      const uint8_t *buf, const uint8_t *end)
{
   int i;
   c->buf = buf; c->end = end; c->bitbuf = 0; c->bitcnt = 0;
   c->range = 510;
   c->offset = 0;
   for (i = 0; i < 9; i++)
      c->offset = (c->offset << 1) | (uint32_t)rh265_cb_bit(c);
}

/* 9.3.2.2 context initialisation from the 8-bit initValue at SliceQpY.
 * initType is 0 for I slices, 1/2 for P/B (per cabac_init_flag). */
static void rh265_cabac_init_contexts(rh265_cabac *c, int sliceQP, int initType)
{
   int i;
   int qp = rh265_clip3(0, 51, sliceQP);
   for (i = 0; i < RH265_CTX_COUNT; i++)
   {
      int iv  = rh265_ctx_init[initType][i];
      int m   = ((iv >> 4) * 5) - 45;
      int n   = ((iv & 15) << 3) - 16;
      int pre = rh265_clip3(1, 126, ((m * qp) >> 4) + n);
      if (pre <= 63) { c->state[i] = (uint8_t)(63 - pre); c->mps[i] = 0; }
      else           { c->state[i] = (uint8_t)(pre - 64); c->mps[i] = 1; }
   }
}

static int rh265_cabac_decode(rh265_cabac *c, int ctxIdx)
{
   int pState = c->state[ctxIdx];
   int valMPS = c->mps[ctxIdx];
   uint32_t qIdx = (c->range >> 6) & 3;
   uint32_t rLPS = rh265_rangeTabLPS[pState][qIdx];
   int bin;
   c->range -= rLPS;
   if (c->offset >= c->range)
   {
      bin = 1 - valMPS;
      c->offset -= c->range;
      c->range   = rLPS;
      if (pState == 0) c->mps[ctxIdx] = (uint8_t)(1 - valMPS);
      c->state[ctxIdx] = rh265_transIdxLPS[pState];
   }
   else
   {
      bin = valMPS;
      c->state[ctxIdx] = rh265_transIdxMPS[pState];
   }
   if (c->range < 256)
   {
      /* range is 2..255 here, so 1..7 doublings restore it; take them
       * in one step instead of a bit at a time */
      int n = (int)rh265_clz32(c->range) - 23;
      c->range <<= n;
      c->offset  = (c->offset << n) | rh265_cb_bits(c, n);
   }
   return bin;
}

static int rh265_cabac_bypass(rh265_cabac *c)
{
   c->offset = (c->offset << 1) | (uint32_t)rh265_cb_bit(c);
   if (c->offset >= c->range) { c->offset -= c->range; return 1; }
   return 0;
}

static RH265_INLINE uint32_t rh265_cabac_bypass_bits(rh265_cabac *c, int n)
{
   uint32_t v = 0;
   while (n--)
      v = (v << 1) | (uint32_t)rh265_cabac_bypass(c);
   return v;
}

static int rh265_cabac_terminate(rh265_cabac *c)
{
   c->range -= 2;
   if (c->offset >= c->range) return 1;
   if (c->range < 256)
   {
      int n = (int)rh265_clz32(c->range) - 23;
      c->range <<= n;
      c->offset  = (c->offset << n) | rh265_cb_bits(c, n);
   }
   return 0;
}

/* ==================== rh265_scan.h ==================== */
/* Coefficient-group and in-group scan orders and the significance-map
 * context maps (H.265 clauses 6.5.3 and 9.3.4.2.5), extracted from
 * FFmpeg libavcodec/hevc/{data.c,cabac.c}. */

static const uint8_t rh265_scan_1x1[1]={0};
static const uint8_t rh265_diag4x4_x[16]={0,0,1,0,1,2,0,1,2,3,1,2,3,2,3,3};
static const uint8_t rh265_diag4x4_y[16]={0,1,0,2,1,0,3,2,1,0,3,2,1,3,2,3};
static const uint8_t rh265_diag2x2_x[4]={0,0,1,1};
static const uint8_t rh265_diag2x2_y[4]={0,1,0,1};
static const uint8_t rh265_diag2x2_inv[2][2]={{0,2},{1,3}};
static const uint8_t rh265_diag4x4_inv[4][4]={
   {0,2,5,9},{1,4,8,12},{3,7,11,14},{6,10,13,15}};
static const uint8_t rh265_diag8x8_x[64]={
   0,0,1,0,1,2,0,1,2,3,0,1,2,3,4,0,1,2,3,4,5,0,1,2,3,4,5,6,
   0,1,2,3,4,5,6,7,1,2,3,4,5,6,7,2,3,4,5,6,7,3,4,5,6,7,4,5,
   6,7,5,6,7,6,7,7};
static const uint8_t rh265_diag8x8_y[64]={
   0,1,0,2,1,0,3,2,1,0,4,3,2,1,0,5,4,3,2,1,0,6,5,4,3,2,1,0,
   7,6,5,4,3,2,1,0,7,6,5,4,3,2,1,7,6,5,4,3,2,7,6,5,4,3,7,6,
   5,4,7,6,5,7,6,7};
static const uint8_t rh265_diag8x8_inv[8][8]={
   { 0, 2, 5, 9,14,20,27,35},{ 1, 4, 8,13,19,26,34,42},
   { 3, 7,12,18,25,33,41,48},{ 6,11,17,24,32,40,47,53},
   {10,16,23,31,39,46,52,57},{15,22,30,38,45,51,56,60},
   {21,29,37,44,50,55,59,62},{28,36,43,49,54,58,61,63}};
static const uint8_t rh265_horiz2x2_x[4]={0,1,0,1};
static const uint8_t rh265_horiz2x2_y[4]={0,0,1,1};
static const uint8_t rh265_horiz4x4_x[16]={0,1,2,3,0,1,2,3,0,1,2,3,0,1,2,3};
static const uint8_t rh265_horiz4x4_y[16]={0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3};
static const uint8_t rh265_horiz8x8_inv[8][8]={
   { 0, 1, 2, 3,16,17,18,19},{ 4, 5, 6, 7,20,21,22,23},
   { 8, 9,10,11,24,25,26,27},{12,13,14,15,28,29,30,31},
   {32,33,34,35,48,49,50,51},{36,37,38,39,52,53,54,55},
   {40,41,42,43,56,57,58,59},{44,45,46,47,60,61,62,63}};

/* significance-map context maps composed with the intra-CG scan,
 * indexed by scan position; rows: 4x4 map, prev_sig 0/1/2, default */
static const uint8_t rh265_sig_ctx_map[3][5*16]={
   {  /* diagonal */
      0,2,1,6,3,4,7,6,4,5,7,8,5,8,8,8,
      1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,
      2,1,2,0,1,2,0,0,1,2,0,0,1,0,0,0,
      2,2,1,2,1,0,2,1,0,0,1,0,0,0,0,0,
      2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2
   },
   {  /* horizontal */
      0,1,4,5,2,3,4,5,6,6,8,8,7,7,8,8,
      1,1,1,0,1,1,0,0,1,0,0,0,0,0,0,0,
      2,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,
      2,1,0,0,2,1,0,0,2,1,0,0,2,1,0,0,
      2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2
   },
   {  /* vertical */
      0,2,6,7,1,3,6,7,4,4,8,8,5,5,8,8,
      1,1,1,0,1,1,0,0,1,0,0,0,0,0,0,0,
      2,1,0,0,2,1,0,0,2,1,0,0,2,1,0,0,
      2,2,2,2,1,1,1,1,0,0,0,0,0,0,0,0,
      2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2
   }
};

/* ==================== rh265_frame.h ==================== */

typedef struct
{
   uint8_t type_idx[3];       /* 0 off, 1 band, 2 edge */
   uint8_t band_pos[3];
   uint8_t eo_class[3];
   int8_t  off[3][4];
} rh265_sao_params;

/* ==================== rh265_pic.h ==================== */

typedef struct { int16_t x, y; } rh265_mv;

/* Motion metadata per 4x4 luma block.  The POC of each reference is
 * stored directly (rather than a list index) so TMVP by later pictures
 * and boundary-strength ref comparisons never need the originating
 * slice's reference lists. */
typedef struct
{
   rh265_mv mv[2];
   int32_t  poc[2];
   int8_t   ref_idx[2];
   uint8_t  pred;             /* 0 intra, 1 L0, 2 L1, 3 bi */
} rh265_mvfield;

#define RH265_MAX_DPB  18

typedef struct
{
   uint8_t *pl[3];            /* Y, U, V planes */
   rh265_mvfield *mvf;        /* per 4x4, kept for TMVP */
   int      poc;
   int      is_ref;           /* member of the active RPS */
   int      needed_out;       /* not yet emitted through the API */
   int      in_use;
} rh265_pic;

#define RH265_MAX_PB 64

typedef struct
{
   const rh265_sps *sps;
   const rh265_pps *pps;
   rh265_shdr sh;

   uint8_t *pl[3];            /* aliases of the current picture's planes */
   int strd[3];
   int pw[3], ph[3];

   /* current picture and its reference lists */
   rh265_pic *cur;
   rh265_pic *ref_list[2][RH265_MAX_REFS];
   int        ref_poc[2][RH265_MAX_REFS];
   int        nb_refs[2];
   rh265_pic *col_ref;        /* TMVP collocated picture */
   rh265_mvfield *mvf;        /* alias of cur->mvf */

   /* per-4x4 (luma coords >> 2) metadata */
   uint8_t *ipm;              /* luma intra prediction mode */
   uint8_t *ctd;              /* coding-tree depth */
   uint8_t *vedge, *hedge;    /* deblocking boundary strength (0..2) */
   uint8_t *nzc;              /* luma TB has nonzero coefficients */
   uint8_t *skipm;            /* cu_skip_flag per 4x4 */
   int w4, h4;

   int8_t *qpy;               /* QpY per 8x8 (luma coords >> 3) */
   int w8, h8;

   rh265_sao_params *sao;     /* per CTB */

   rh265_cabac cb;

   /* current-slice decode state */
   int slice_start_zaddr;
   int        slice_start_ctb;   /* raster CTB address of the slice start */

   /* Large per-call work buffers.  These live here rather than on the
    * stack because several console targets run threads on 8 KiB
    * stacks; the decode call tree is single-threaded and none of these
    * are live across the calls that use them. */
   int      mc_val0[RH265_MAX_PB * RH265_MAX_PB];
   int      mc_val1[RH265_MAX_PB * RH265_MAX_PB];
   /* element capacity for either pel width; the bit-depth template
    * views this as RH265_PEL[] */
   uint16_t mc_patch[(RH265_MAX_PB + 7) * (RH265_MAX_PB + 7)];
   int      mc_tmp[(RH265_MAX_PB + 7) * RH265_MAX_PB];
   int16_t  coeff_scratch[32 * 32];

   /* active scaling lists (PPS override, else SPS defaults/coded);
    * NULL when scaling_list_enabled is off, keeping the flat-16
    * dequantisation path untouched */
   const rh265_scaling *sl;

   /* per-CTB slice index and its loop_filter_across_slices flag, for
    * the 8.7.2/8.7.3 slice-boundary filtering restriction */
   uint16_t *ctb_slice;
   uint8_t  *ctb_lf_across;
   int       slice_seq;

   /* WPP row-context storage (9.3.2.3) */
   uint8_t  wpp_state[RH265_CTX_COUNT];
   uint8_t  wpp_mps[RH265_CTX_COUNT];

   /* active bit depth and its per-depth function table */
   int      bd;
   int      pel_bytes;
   const struct rh265_bd_fns_s *fns;
   int cur_zaddr;             /* z-scan address of the current TU/CU origin */
   int qp_y;                  /* current QpY */
   int qp_y_pred;             /* qPY_PREV of 8.6.1 */
   int first_qg;
   int cu_qp_delta;           /* pending delta for the current quant group */
   int is_cu_qp_delta_coded;
   int cu_transquant_bypass;  /* always 0: transquant bypass is refused */
   int intra_split;           /* PartMode == PART_NxN */
   int intra_pred_mode[4];    /* luma modes of the (up to 4) PUs */
   int intra_pred_mode_c;
   int pu_log2, pu_x0, pu_y0; /* current CU for chroma-mode DM lookup */

   /* current-CU inter state */
   int cu_pred_intra;         /* CuPredMode == MODE_INTRA */
   int part_mode;             /* RH265_PART_* */
   int cu_x, cu_y, cu_log2;   /* current CU geometry */
   int ct_depth_cur;          /* ctDepth of the current CU */
   int max_merge;             /* MaxNumMergeCand */
   int slice_tmvp;            /* slice_temporal_mvp_enabled_flag */
} rh265_dec;

/* Per-bit-depth entry points, instantiated from rh265_bd.inc.  Common
 * code never touches samples directly: everything pixel-typed goes
 * through this table, selected once per SPS activation. */
typedef struct rh265_bd_fns_s
{
   void (*intra_pred)(rh265_dec *d, int x0, int y0, int log2_size,
         int c_idx, int mode);
   void (*add_residual)(rh265_dec *d, int c_idx, int px, int py,
         const int16_t *coeffs, int size);
   void (*mc_pu)(rh265_dec *d, int x0, int y0, int w, int h,
         const rh265_mvfield *mv);
   void (*deblock_frame)(rh265_dec *d);
   int  (*sao_frame)(rh265_dec *d);
} rh265_bd_fns;

static const rh265_bd_fns *rh265_get_fns(int bd);

#define RH265_PF_L0 1
#define RH265_PF_L1 2
#define RH265_PF_BI 3

enum
{
   RH265_PART_2Nx2N = 0,
   RH265_PART_2NxN,
   RH265_PART_Nx2N,
   RH265_PART_NxN,
   RH265_PART_2NxnU,
   RH265_PART_2NxnD,
   RH265_PART_nLx2N,
   RH265_PART_nRx2N
};

static RH265_INLINE rh265_mvfield *rh265_mvf_at(const rh265_dec *d,
      int x, int y)
{
   return &d->mvf[(y >> 2) * d->w4 + (x >> 2)];
}

static RH265_INLINE int rh265_clip_int8(int v)
{
   return v < -128 ? -128 : (v > 127 ? 127 : v);
}
static RH265_INLINE int rh265_clip_int16(int v)
{
   return v < -32768 ? -32768 : (v > 32767 ? 32767 : v);
}


/* z-scan address of the min-TB containing luma sample (x,y): CTB raster
 * index scaled by the CTB area in 4x4 units, plus the Morton interleave
 * of the in-CTB 4x4 coordinates (6.5.2). */
static RH265_INLINE int rh265_zaddr(const rh265_dec *d, int x, int y)
{
   const rh265_sps *s = d->sps;
   int log2 = s->log2_ctb;
   int cx = x >> log2, cy = y >> log2;
   int ix = (x & ((1 << log2) - 1)) >> 2;
   int iy = (y & ((1 << log2) - 1)) >> 2;
   int m = 0, i;
   for (i = 0; i < 4; i++)
      m |= ((ix >> i) & 1) << (2 * i) | ((iy >> i) & 1) << (2 * i + 1);
   return (cy * s->ctb_w + cx) * (1 << (2 * (log2 - 2))) + m;
}

/* Availability of the block containing luma sample (x,y) for prediction
 * from the block at z-scan address cur (6.4.1, single slice, no tiles). */
static RH265_INLINE int rh265_avail(const rh265_dec *d, int x, int y, int cur)
{
   int z;
   if (x < 0 || y < 0 || x >= d->sps->width || y >= d->sps->height)
      return 0;
   z = rh265_zaddr(d, x, y);
   return z >= d->slice_start_zaddr && z < cur;
}

/* ==================== rh265_intra.h ==================== */

/* Table 8-4: intraPredAngle for modes 2..34 */
static const int8_t rh265_pred_angle[35]={
   0,0, 32,26,21,17,13,9,5,2,0,-2,-5,-9,-13,-17,-21,-26,-32,
   -26,-21,-17,-13,-9,-5,-2,0,2,5,9,13,17,21,26,32};
/* Table 8-5: invAngle for modes 11..25 */
static const int16_t rh265_inv_angle[35]={
   0,0,0,0,0,0,0,0,0,0,0,
   -4096,-1638,-910,-630,-482,-390,-315,-256,
   -315,-390,-482,-630,-910,-1638,-4096};

/* Predict one nTbS x nTbS intra block.  x0/y0 are luma sample
 * coordinates of the TB; for chroma the plane coordinates are x0>>1,
 * y0>>1.  Writes into the frame planes. */
/* ==================== rh265_xform.h ==================== */

/* HEVC inverse-transform basis (H.265 clause 8.6.4.2 constants), rows are
 * frequencies, columns sample positions; extracted programmatically from
 * FFmpeg libavcodec/hevc/dsp.c. */
static const int8_t rh265_t32[32][32]={
   {64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64},
   {90,90,88,85,82,78,73,67,61,54,46,38,31,22,13,4,-4,-13,-22,-31,-38,-46,-54,-61,-67,-73,-78,-82,-85,-88,-90,-90},
   {90,87,80,70,57,43,25,9,-9,-25,-43,-57,-70,-80,-87,-90,-90,-87,-80,-70,-57,-43,-25,-9,9,25,43,57,70,80,87,90},
   {90,82,67,46,22,-4,-31,-54,-73,-85,-90,-88,-78,-61,-38,-13,13,38,61,78,88,90,85,73,54,31,4,-22,-46,-67,-82,-90},
   {89,75,50,18,-18,-50,-75,-89,-89,-75,-50,-18,18,50,75,89,89,75,50,18,-18,-50,-75,-89,-89,-75,-50,-18,18,50,75,89},
   {88,67,31,-13,-54,-82,-90,-78,-46,-4,38,73,90,85,61,22,-22,-61,-85,-90,-73,-38,4,46,78,90,82,54,13,-31,-67,-88},
   {87,57,9,-43,-80,-90,-70,-25,25,70,90,80,43,-9,-57,-87,-87,-57,-9,43,80,90,70,25,-25,-70,-90,-80,-43,9,57,87},
   {85,46,-13,-67,-90,-73,-22,38,82,88,54,-4,-61,-90,-78,-31,31,78,90,61,4,-54,-88,-82,-38,22,73,90,67,13,-46,-85},
   {83,36,-36,-83,-83,-36,36,83,83,36,-36,-83,-83,-36,36,83,83,36,-36,-83,-83,-36,36,83,83,36,-36,-83,-83,-36,36,83},
   {82,22,-54,-90,-61,13,78,85,31,-46,-90,-67,4,73,88,38,-38,-88,-73,-4,67,90,46,-31,-85,-78,-13,61,90,54,-22,-82},
   {80,9,-70,-87,-25,57,90,43,-43,-90,-57,25,87,70,-9,-80,-80,-9,70,87,25,-57,-90,-43,43,90,57,-25,-87,-70,9,80},
   {78,-4,-82,-73,13,85,67,-22,-88,-61,31,90,54,-38,-90,-46,46,90,38,-54,-90,-31,61,88,22,-67,-85,-13,73,82,4,-78},
   {75,-18,-89,-50,50,89,18,-75,-75,18,89,50,-50,-89,-18,75,75,-18,-89,-50,50,89,18,-75,-75,18,89,50,-50,-89,-18,75},
   {73,-31,-90,-22,78,67,-38,-90,-13,82,61,-46,-88,-4,85,54,-54,-85,4,88,46,-61,-82,13,90,38,-67,-78,22,90,31,-73},
   {70,-43,-87,9,90,25,-80,-57,57,80,-25,-90,-9,87,43,-70,-70,43,87,-9,-90,-25,80,57,-57,-80,25,90,9,-87,-43,70},
   {67,-54,-78,38,85,-22,-90,4,90,13,-88,-31,82,46,-73,-61,61,73,-46,-82,31,88,-13,-90,-4,90,22,-85,-38,78,54,-67},
   {64,-64,-64,64,64,-64,-64,64,64,-64,-64,64,64,-64,-64,64,64,-64,-64,64,64,-64,-64,64,64,-64,-64,64,64,-64,-64,64},
   {61,-73,-46,82,31,-88,-13,90,-4,-90,22,85,-38,-78,54,67,-67,-54,78,38,-85,-22,90,4,-90,13,88,-31,-82,46,73,-61},
   {57,-80,-25,90,-9,-87,43,70,-70,-43,87,9,-90,25,80,-57,-57,80,25,-90,9,87,-43,-70,70,43,-87,-9,90,-25,-80,57},
   {54,-85,-4,88,-46,-61,82,13,-90,38,67,-78,-22,90,-31,-73,73,31,-90,22,78,-67,-38,90,-13,-82,61,46,-88,4,85,-54},
   {50,-89,18,75,-75,-18,89,-50,-50,89,-18,-75,75,18,-89,50,50,-89,18,75,-75,-18,89,-50,-50,89,-18,-75,75,18,-89,50},
   {46,-90,38,54,-90,31,61,-88,22,67,-85,13,73,-82,4,78,-78,-4,82,-73,-13,85,-67,-22,88,-61,-31,90,-54,-38,90,-46},
   {43,-90,57,25,-87,70,9,-80,80,-9,-70,87,-25,-57,90,-43,-43,90,-57,-25,87,-70,-9,80,-80,9,70,-87,25,57,-90,43},
   {38,-88,73,-4,-67,90,-46,-31,85,-78,13,61,-90,54,22,-82,82,-22,-54,90,-61,-13,78,-85,31,46,-90,67,4,-73,88,-38},
   {36,-83,83,-36,-36,83,-83,36,36,-83,83,-36,-36,83,-83,36,36,-83,83,-36,-36,83,-83,36,36,-83,83,-36,-36,83,-83,36},
   {31,-78,90,-61,4,54,-88,82,-38,-22,73,-90,67,-13,-46,85,-85,46,13,-67,90,-73,22,38,-82,88,-54,-4,61,-90,78,-31},
   {25,-70,90,-80,43,9,-57,87,-87,57,-9,-43,80,-90,70,-25,-25,70,-90,80,-43,-9,57,-87,87,-57,9,43,-80,90,-70,25},
   {22,-61,85,-90,73,-38,-4,46,-78,90,-82,54,-13,-31,67,-88,88,-67,31,13,-54,82,-90,78,-46,4,38,-73,90,-85,61,-22},
   {18,-50,75,-89,89,-75,50,-18,-18,50,-75,89,-89,75,-50,18,18,-50,75,-89,89,-75,50,-18,-18,50,-75,89,-89,75,-50,18},
   {13,-38,61,-78,88,-90,85,-73,54,-31,4,22,-46,67,-82,90,-90,82,-67,46,-22,-4,31,-54,73,-85,90,-88,78,-61,38,-13},
   {9,-25,43,-57,70,-80,87,-90,90,-87,80,-70,57,-43,25,-9,-9,25,-43,57,-70,80,-87,90,-90,87,-80,70,-57,43,-25,9},
   {4,-13,22,-31,38,-46,54,-61,67,-73,78,-82,85,-88,90,-90,90,-90,88,-85,82,-78,73,-67,61,-54,46,-38,31,-22,13,-4}
};

/* Inverse 4x4 DST-VII for intra luma (8.6.4.1), constants as used by the
 * two-pass butterfly in FFmpeg dsp_template.c. */
static RH265_INLINE void rh265_idst4_pass(int16_t *dst, const int16_t *src,
      int dstep, int sstep, int shift)
{
   int add = 1 << (shift - 1);
   int c0 = src[0 * sstep] + src[2 * sstep];
   int c1 = src[2 * sstep] + src[3 * sstep];
   int c2 = src[0 * sstep] - src[3 * sstep];
   int c3 = 74 * src[1 * sstep];
   int v;
   v = 74 * (src[0 * sstep] - src[2 * sstep] + src[3 * sstep]);
   dst[2 * dstep] = (int16_t)rh265_clip3(-32768, 32767, (v + add) >> shift);
   v = 29 * c0 + 55 * c1 + c3;
   dst[0 * dstep] = (int16_t)rh265_clip3(-32768, 32767, (v + add) >> shift);
   v = 55 * c2 - 29 * c1 + c3;
   dst[1 * dstep] = (int16_t)rh265_clip3(-32768, 32767, (v + add) >> shift);
   v = 55 * c0 + 29 * c2 - c3;
   dst[3 * dstep] = (int16_t)rh265_clip3(-32768, 32767, (v + add) >> shift);
}

static void rh265_idst4(int16_t *coeffs, int bd)
{
   int i;
   for (i = 0; i < 4; i++)
      rh265_idst4_pass(coeffs + i, coeffs + i, 4, 4, 7);
   for (i = 0; i < 4; i++)
      rh265_idst4_pass(coeffs + 4 * i, coeffs + 4 * i, 1, 1, 20 - bd);
}

/* One inverse-DCT pass of length size: dst[n] = sum_k src[k]*T[k][n],
 * with TN[k][n] = T32[k*32/size][n] (8.6.4.2). */
static void rh265_idct_pass(int16_t *dst, const int16_t *src,
      int dstep, int sstep, int size, int shift)
{
   int add = 1 << (shift - 1);
   int step = 32 / size;
   int k, n;
   int32_t acc[32];
   for (n = 0; n < size; n++)
      acc[n] = 0;
   for (k = 0; k < size; k++)
   {
      int32_t c = src[k * sstep];
      const int8_t *row;
      if (!c)
         continue;
      row = rh265_t32[k * step];
      for (n = 0; n < size; n++)
         acc[n] += c * row[n];
   }
   for (n = 0; n < size; n++)
      dst[n * dstep] = (int16_t)rh265_clip3(-32768, 32767,
            (acc[n] + add) >> shift);
}

static void rh265_idct(int16_t *coeffs, int log2_size, int bd)
{
   int size = 1 << log2_size;
   int i;
   for (i = 0; i < size; i++)
      rh265_idct_pass(coeffs + i, coeffs + i, size, size, size, 7);
   for (i = 0; i < size; i++)
      rh265_idct_pass(coeffs + size * i, coeffs + size * i, 1, 1, size,
            20 - bd);
}

/* transform_skip rescale (FFmpeg FUNC(dequant)): shift = 15 - bd - log2 */
static void rh265_tskip_rescale(int16_t *coeffs, int log2_size, int bd)
{
   int shift = 15 - bd - log2_size;
   int n = 1 << (2 * log2_size);
   int i, off = 1 << (shift - 1);
   for (i = 0; i < n; i++)
      coeffs[i] = (int16_t)((coeffs[i] + off) >> shift);
}

/* chroma QP mapping for 4:2:0 (Table 8-10) */
static const uint8_t rh265_qp_c[14]={29,30,31,32,33,33,34,34,35,35,36,36,37,37};
static const uint8_t rh265_level_scale[6]={40,45,51,57,64,72};

static int rh265_chroma_qp(int qp_y, int offset, int bd)
{
   int qp_i = rh265_clip3(-6 * (bd - 8), 57, qp_y + offset);
   if (qp_i < 30) return qp_i;
   if (qp_i > 43) return qp_i - 6;
   return rh265_qp_c[qp_i - 30];
}

/* ==================== rh265_residual.h ==================== */

#define RH265_SCAN_DIAG  0
#define RH265_SCAN_HORIZ 1
#define RH265_SCAN_VERT  2

static int rh265_last_sig_prefix(rh265_cabac *cb, int c_idx, int log2_size,
      int base_ctx)
{
   int i = 0;
   int max = (log2_size << 1) - 1;
   int ctx_offset, ctx_shift;
   if (!c_idx)
   {
      ctx_offset = 3 * (log2_size - 2) + ((log2_size - 1) >> 2);
      ctx_shift  = (log2_size + 1) >> 2;
   }
   else
   {
      ctx_offset = 15;
      ctx_shift  = log2_size - 2;
   }
   while (i < max &&
          rh265_cabac_decode(cb, base_ctx + (i >> ctx_shift) + ctx_offset))
      i++;
   return i;
}

static int rh265_last_sig_suffix(rh265_cabac *cb, int prefix)
{
   int i;
   int length = (prefix >> 1) - 1;
   int value = rh265_cabac_bypass(cb);
   for (i = 1; i < length; i++)
      value = (value << 1) | rh265_cabac_bypass(cb);
   return value;
}

/* coeff_abs_level_remaining: truncated-Rice prefix then EGk suffix */
static int rh265_coeff_abs_remaining(rh265_cabac *cb, int rice)
{
   int prefix = 0;
   int suffix = 0;
   int i;
   while (prefix < 31 && rh265_cabac_bypass(cb))
      prefix++;
   if (prefix < 3)
   {
      for (i = 0; i < rice; i++)
         suffix = (suffix << 1) | rh265_cabac_bypass(cb);
      return (prefix << rice) + suffix;
   }
   else
   {
      int k = prefix - 3 + rice;
      if (prefix == 31 || k > 22)
         return 0;                       /* malformed; the terminate check
                                          * at slice end will catch it */
      for (i = 0; i < k; i++)
         suffix = (suffix << 1) | rh265_cabac_bypass(cb);
      return ((((1 << (prefix - 3)) + 3 - 1)) << rice) + suffix;
   }
}

/* residual_coding (7.3.8.11 / 9.3.4.2.5-7), Main profile: no scaling
 * lists, no transquant bypass, no RDPCM, no persistent Rice adaptation.
 * Decodes, dequantises, inverse-transforms and adds one TB. */
static int rh265_residual_coding(rh265_dec *d, int x0, int y0,
      int log2_size, int scan_idx, int c_idx, int intra_mode)
{
   const rh265_pps *pps = d->pps;
   rh265_cabac *cb = &d->cb;
   int trafo_size = 1 << log2_size;
   int transform_skip_flag = 0;
   int last_x, last_y, last_scan_pos, n_end, num_coeff = 0;
   int greater1_ctx = 1;
   int num_last_subset;
   int x_cg_last, y_cg_last;
   const uint8_t *scan_x_cg, *scan_y_cg, *scan_x_off, *scan_y_off;
   uint8_t sig_cg[8][8];
   int16_t *coeffs = d->coeff_scratch;
   int qp, shift, add, scale;
   const uint8_t *sl_m;
   int sl_dc, sl_sub, sl_row;
   int i;
   int shiftc = c_idx ? 1 : 0;

   memset(sig_cg, 0, sizeof(sig_cg));
   memset(coeffs, 0, sizeof(int16_t) << (2 * log2_size));

   if (c_idx == 0)
      qp = d->qp_y + 6 * (d->bd - 8);
   else
   {
      int off = (c_idx == 1)
            ? pps->cb_qp_offset + d->sh.cb_qp_offset
            : pps->cr_qp_offset + d->sh.cr_qp_offset;
      qp = rh265_chroma_qp(d->qp_y, off, d->bd) + 6 * (d->bd - 8);
   }
   shift = d->bd + log2_size - 5;
   add   = 1 << (shift - 1);
   scale = rh265_level_scale[qp % 6] << (qp / 6);

   /* m[x][y] of 8.6.3: raster lists per size class, the two larger
    * grids sub-sampled from their 8x8 form with the coded DC at the
    * origin.  Flat 16 when lists are off or the block is transform
    * skip. */
   sl_m    = NULL;
   sl_dc   = 16;
   sl_sub  = 0;
   sl_row  = 8;
   if (d->sl)
   {
      int matrix_id = (d->cu_pred_intra ? 0 : 3) + c_idx;
      switch (log2_size)
      {
         case 2:  sl_m = d->sl->l4[matrix_id];  sl_row = 4; break;
         case 3:  sl_m = d->sl->l8[matrix_id];  break;
         case 4:  sl_m = d->sl->l16[matrix_id];
                  sl_dc = d->sl->dc16[matrix_id];
                  sl_sub = 1; break;
         default: sl_m = d->sl->l32[matrix_id];
                  sl_dc = d->sl->dc32[matrix_id];
                  sl_sub = 2; break;
      }
   }

   if (pps->transform_skip_enabled && log2_size <= 2)
      transform_skip_flag = rh265_cabac_decode(cb,
            RH265_CTX_TRANSFORM_SKIP_FLAG + (c_idx ? 1 : 0));

   last_x = rh265_last_sig_prefix(cb, c_idx, log2_size,
         RH265_CTX_LAST_SIGNIFICANT_COEFF_X_PREFIX);
   last_y = rh265_last_sig_prefix(cb, c_idx, log2_size,
         RH265_CTX_LAST_SIGNIFICANT_COEFF_Y_PREFIX);
   if (last_x > 3)
      last_x = (1 << ((last_x >> 1) - 1)) * (2 + (last_x & 1))
            + rh265_last_sig_suffix(cb, last_x);
   if (last_y > 3)
      last_y = (1 << ((last_y >> 1) - 1)) * (2 + (last_y & 1))
            + rh265_last_sig_suffix(cb, last_y);
   if (scan_idx == RH265_SCAN_VERT)
   {
      int t = last_x; last_x = last_y; last_y = t;
   }
   if (last_x >= trafo_size || last_y >= trafo_size)
      return -1;
   x_cg_last = last_x >> 2;
   y_cg_last = last_y >> 2;

   switch (scan_idx)
   {
      case RH265_SCAN_DIAG:
         scan_x_off = rh265_diag4x4_x;
         scan_y_off = rh265_diag4x4_y;
         num_coeff  = rh265_diag4x4_inv[last_y & 3][last_x & 3];
         if (trafo_size == 4)
         {
            scan_x_cg = rh265_scan_1x1;
            scan_y_cg = rh265_scan_1x1;
         }
         else if (trafo_size == 8)
         {
            num_coeff += rh265_diag2x2_inv[y_cg_last][x_cg_last] << 4;
            scan_x_cg = rh265_diag2x2_x;
            scan_y_cg = rh265_diag2x2_y;
         }
         else if (trafo_size == 16)
         {
            num_coeff += rh265_diag4x4_inv[y_cg_last][x_cg_last] << 4;
            scan_x_cg = rh265_diag4x4_x;
            scan_y_cg = rh265_diag4x4_y;
         }
         else
         {
            num_coeff += rh265_diag8x8_inv[y_cg_last][x_cg_last] << 4;
            scan_x_cg = rh265_diag8x8_x;
            scan_y_cg = rh265_diag8x8_y;
         }
         break;
      case RH265_SCAN_HORIZ:
         scan_x_cg  = rh265_horiz2x2_x;
         scan_y_cg  = rh265_horiz2x2_y;
         scan_x_off = rh265_horiz4x4_x;
         scan_y_off = rh265_horiz4x4_y;
         num_coeff  = rh265_horiz8x8_inv[last_y][last_x];
         break;
      default:
         scan_x_cg  = rh265_horiz2x2_y;
         scan_y_cg  = rh265_horiz2x2_x;
         scan_x_off = rh265_horiz4x4_y;
         scan_y_off = rh265_horiz4x4_x;
         num_coeff  = rh265_horiz8x8_inv[last_x][last_y];
         break;
   }
   num_coeff++;
   num_last_subset = (num_coeff - 1) >> 4;

   for (i = num_last_subset; i >= 0; i--)
   {
      int n, m;
      int x_cg, y_cg;
      int implicit_non_zero_coeff = 0;
      int prev_sig = 0;
      int offset = i << 4;
      uint8_t sig_idx[16];
      int nb_sig = 0;

      x_cg = scan_x_cg[i];
      y_cg = scan_y_cg[i];

      if (i < num_last_subset && i > 0)
      {
         int ctx_cg = 0;
         if (x_cg < (1 << (log2_size - 2)) - 1)
            ctx_cg += sig_cg[x_cg + 1][y_cg];
         if (y_cg < (1 << (log2_size - 2)) - 1)
            ctx_cg += sig_cg[x_cg][y_cg + 1];
         sig_cg[x_cg][y_cg] = (uint8_t)rh265_cabac_decode(cb,
               RH265_CTX_SIGNIFICANT_COEFF_GROUP_FLAG
               + rh265_min(ctx_cg, 1) + (c_idx ? 2 : 0));
         implicit_non_zero_coeff = 1;
      }
      else
         sig_cg[x_cg][y_cg] = (uint8_t)
               ((x_cg == x_cg_last && y_cg == y_cg_last) ||
                (x_cg == 0 && y_cg == 0));

      last_scan_pos = num_coeff - offset - 1;
      if (i == num_last_subset)
      {
         n_end = last_scan_pos - 1;
         sig_idx[0] = (uint8_t)last_scan_pos;
         nb_sig = 1;
      }
      else
         n_end = 15;

      if (x_cg < ((1 << log2_size) - 1) >> 2)
         prev_sig = sig_cg[x_cg + 1][y_cg] ? 1 : 0;
      if (y_cg < ((1 << log2_size) - 1) >> 2)
         prev_sig += sig_cg[x_cg][y_cg + 1] ? 2 : 0;

      if (sig_cg[x_cg][y_cg] && n_end >= 0)
      {
         const uint8_t *map;
         int scf_offset = 0;
         int nb0;
         if (c_idx != 0)
            scf_offset = 27;
         if (log2_size == 2)
            map = &rh265_sig_ctx_map[scan_idx][0];
         else
         {
            map = &rh265_sig_ctx_map[scan_idx][(prev_sig + 1) << 4];
            if (c_idx == 0)
            {
               if (x_cg > 0 || y_cg > 0)
                  scf_offset += 3;
               if (log2_size == 3)
                  scf_offset += (scan_idx == RH265_SCAN_DIAG) ? 9 : 15;
               else
                  scf_offset += 21;
            }
            else
            {
               if (log2_size == 3)
                  scf_offset += 9;
               else
                  scf_offset += 12;
            }
         }
         nb0 = nb_sig;
         for (n = n_end; n > 0; n--)
         {
            int sig = rh265_cabac_decode(cb,
                  RH265_CTX_SIGNIFICANT_COEFF_FLAG + map[n] + scf_offset);
            sig_idx[nb_sig] = (uint8_t)n;
            nb_sig += sig;
         }
         if (nb_sig != nb0)
            implicit_non_zero_coeff = 0;
         if (implicit_non_zero_coeff == 0)
         {
            if (i == 0)
               scf_offset = c_idx ? 27 : 0;
            else
               scf_offset = 2 + scf_offset;
            sig_idx[nb_sig] = 0;
            nb_sig += rh265_cabac_decode(cb,
                  RH265_CTX_SIGNIFICANT_COEFF_FLAG + scf_offset);
         }
         else
         {
            sig_idx[nb_sig] = 0;
            nb_sig++;
         }
      }

      n_end = nb_sig;
      if (n_end)
      {
         int first_nz, last_nz;
         int rice = 0;
         int first_gt1_idx = -1;
         uint8_t gt1[8];
         uint32_t sign_flags;
         int sum_abs = 0;
         int sign_hidden;
         int ctx_set = (i > 0 && c_idx == 0) ? 2 : 0;
         int nsigns;

         if (i != num_last_subset && greater1_ctx == 0)
            ctx_set++;
         greater1_ctx = 1;
         last_nz = sig_idx[0];

         for (m = 0; m < (n_end > 8 ? 8 : n_end); m++)
         {
            int inc  = (ctx_set << 2) + greater1_ctx;
            int flag = rh265_cabac_decode(cb,
                  RH265_CTX_COEFF_ABS_LEVEL_GREATER1_FLAG
                  + (c_idx ? 16 : 0) + inc);
            gt1[m] = (uint8_t)flag;
            if (flag)
            {
               if (first_gt1_idx == -1)
                  first_gt1_idx = m;
               greater1_ctx = 0;
            }
            else if (greater1_ctx > 0 && greater1_ctx < 3)
               greater1_ctx++;
         }
         first_nz = sig_idx[n_end - 1];
         sign_hidden = (last_nz - first_nz >= 4);
         if (first_gt1_idx != -1)
            gt1[first_gt1_idx] = (uint8_t)(gt1[first_gt1_idx]
                  + rh265_cabac_decode(cb,
                        RH265_CTX_COEFF_ABS_LEVEL_GREATER2_FLAG
                        + (c_idx ? 4 : 0) + ctx_set));
         nsigns = n_end;
         if (pps->sign_data_hiding && sign_hidden)
            nsigns--;
         sign_flags = rh265_cabac_bypass_bits(cb, nsigns) << (32 - nsigns);

         for (m = 0; m < n_end; m++)
         {
            int32_t level;
            int x_c, y_c;
            n = sig_idx[m];
            x_c = (x_cg << 2) + scan_x_off[n];
            y_c = (y_cg << 2) + scan_y_off[n];
            if (m < 8)
            {
               level = 1 + gt1[m];
               if (level == ((m == first_gt1_idx) ? 3 : 2))
               {
                  int rem = rh265_coeff_abs_remaining(cb, rice);
                  level += rem;
                  if (level > (3 << rice))
                     rice = rh265_min(rice + 1, 4);
               }
            }
            else
            {
               int rem = rh265_coeff_abs_remaining(cb, rice);
               level = 1 + rem;
               if (level > (3 << rice))
                  rice = rh265_min(rice + 1, 4);
            }
            if (pps->sign_data_hiding && sign_hidden)
            {
               sum_abs += level;
               if (n == first_nz && (sum_abs & 1))
                  level = -level;
            }
            if (sign_flags & 0x80000000u)
               level = -level;
            sign_flags <<= 1;
            {
               int m = 16;
               int64_t t;
               /* 8.6.3: m stays 16 for transform skip only when
                * nTbS > 4 (a range-extension case); the 4x4
                * transform-skip blocks of version-1 streams use the
                * scaling list like any other TU */
               if (sl_m)
               {
                  if (sl_sub && x_c == 0 && y_c == 0)
                     m = sl_dc;
                  else
                     m = sl_m[(y_c >> sl_sub) * sl_row
                           + (x_c >> sl_sub)];
               }
               t = ((int64_t)level * scale * m + add) >> shift;
               coeffs[y_c * trafo_size + x_c] = (int16_t)
                     rh265_clip3(-32768, 32767, (int)t);
            }
         }
      }
   }

   if (transform_skip_flag)
      rh265_tskip_rescale(coeffs, log2_size, d->bd);
   else if (c_idx == 0 && log2_size == 2 && intra_mode >= 0)
      rh265_idst4(coeffs, d->bd);
   else
      rh265_idct(coeffs, log2_size, d->bd);

   d->fns->add_residual(d, c_idx, x0 >> shiftc, y0 >> shiftc,
         coeffs, trafo_size);
   return 0;
}

/* ==================== rh265_qp.h ==================== */

/* 8.6.1 derivation of QpY for the current quantisation group. */
static void rh265_set_qpy(rh265_dec *d, int xBase, int yBase)
{
   const rh265_sps *sps = d->sps;
   const rh265_pps *pps = d->pps;
   int ctb_mask = (1 << sps->log2_ctb) - 1;
   int qg_mask  = (1 << (sps->log2_ctb - pps->diff_cu_qp_delta_depth)) - 1;
   int x_qg = xBase - (xBase & qg_mask);
   int y_qg = yBase - (yBase & qg_mask);
   int avail_a = (xBase & ctb_mask) && (x_qg & ctb_mask);
   int avail_b = (yBase & ctb_mask) && (y_qg & ctb_mask);
   int qpy_pred, qpy_a, qpy_b;

   if (d->first_qg || (!x_qg && !y_qg))
   {
      d->first_qg = !d->is_cu_qp_delta_coded;
      qpy_pred = d->sh.slice_qp;
   }
   else
      qpy_pred = d->qp_y_pred;

   qpy_a = avail_a ? d->qpy[(y_qg >> 3) * d->w8 + ((x_qg - 1) >> 3)] : qpy_pred;
   qpy_b = avail_b ? d->qpy[((y_qg - 1) >> 3) * d->w8 + (x_qg >> 3)] : qpy_pred;
   qpy_pred = (qpy_a + qpy_b + 1) >> 1;

   if (d->cu_qp_delta != 0)
      d->qp_y = ((qpy_pred + d->cu_qp_delta + 52) % 52);
   else
      d->qp_y = qpy_pred;
}

static int rh265_cu_qp_delta_abs(rh265_cabac *cb)
{
   int prefix = 0, suffix = 0;
   int inc = 0;
   while (prefix < 5 && rh265_cabac_decode(cb, RH265_CTX_CU_QP_DELTA + inc))
   {
      prefix++;
      inc = 1;
   }
   if (prefix >= 5)
   {
      int k = 0;
      while (k < 7 && rh265_cabac_bypass(cb))
      {
         suffix += 1 << k;
         k++;
      }
      if (k == 7)
         return -1000;
      while (k--)
         suffix += rh265_cabac_bypass(cb) << k;
   }
   return prefix + suffix;
}

/* ==================== rh265_tu.h ==================== */

/* 8.7.2.4 boundary strength for one edge between two inter 4x4 blocks
 * (neither side intra, no coded coefficients adjacent).  Reference
 * comparisons use the POCs stored in the motion field. */
static int rh265_mv_bs(const rh265_mvfield *c, const rh265_mvfield *n)
{
#define RH265_MVD4(a, b)    ((a).x - (b).x >= 4 || (b).x - (a).x >= 4 ||     (a).y - (b).y >= 4 || (b).y - (a).y >= 4)
   if (c->pred == RH265_PF_BI && n->pred == RH265_PF_BI)
   {
      if (c->poc[0] == n->poc[0] && c->poc[0] == c->poc[1] &&
          n->poc[0] == n->poc[1])
         return (RH265_MVD4(n->mv[0], c->mv[0]) ||
                 RH265_MVD4(n->mv[1], c->mv[1])) &&
                (RH265_MVD4(n->mv[1], c->mv[0]) ||
                 RH265_MVD4(n->mv[0], c->mv[1]));
      if (n->poc[0] == c->poc[0] && n->poc[1] == c->poc[1])
         return RH265_MVD4(n->mv[0], c->mv[0]) ||
                RH265_MVD4(n->mv[1], c->mv[1]);
      if (n->poc[1] == c->poc[0] && n->poc[0] == c->poc[1])
         return RH265_MVD4(n->mv[1], c->mv[0]) ||
                RH265_MVD4(n->mv[0], c->mv[1]);
      return 1;
   }
   if (c->pred != RH265_PF_BI && n->pred != RH265_PF_BI)
   {
      rh265_mv a, b;
      int poc_a, poc_b;
      if (c->pred & RH265_PF_L0) { a = c->mv[0]; poc_a = c->poc[0]; }
      else                       { a = c->mv[1]; poc_a = c->poc[1]; }
      if (n->pred & RH265_PF_L0) { b = n->mv[0]; poc_b = n->poc[0]; }
      else                       { b = n->mv[1]; poc_b = n->poc[1]; }
      if (poc_a == poc_b)
         return RH265_MVD4(a, b);
      return 1;
   }
   return 1;
#undef RH265_MVD4
}

/* Compute deblocking boundary strengths for one transform block (or a
 * whole CU when it carries no transform tree), mirroring the reference
 * structure: TU edges on the 8-grid combine the intra / coded-residual
 * / motion tests, and the block interior gets the pure motion test on
 * the 8-grid, which resolves PU boundaries inside inter TUs.  The
 * current block's nzc entries must be filled before the call. */
static void rh265_bs_edges(rh265_dec *d, int x0, int y0, int log2_size)
{
   int size = 1 << log2_size;
   int i, j;
   int is_intra = rh265_mvf_at(d, x0, y0)->pred == 0;

   if (y0 > 0 && !(y0 & 7))
   {
      int slice_ok = rh265_zaddr(d, x0, y0 - 1) >= d->slice_start_zaddr;
      if (d->sh.loop_filter_across_slices || slice_ok ||
          (y0 & ((1 << d->sps->log2_ctb) - 1)))
         for (i = 0; i < size && x0 + i < d->sps->width; i += 4)
         {
            const rh265_mvfield *top = rh265_mvf_at(d, x0 + i, y0 - 1);
            const rh265_mvfield *cu2 = rh265_mvf_at(d, x0 + i, y0);
            int bs;
            if (cu2->pred == 0 || top->pred == 0)
               bs = 2;
            else if (d->nzc[((y0 - 1) >> 2) * d->w4 + ((x0 + i) >> 2)] ||
                     d->nzc[(y0 >> 2) * d->w4 + ((x0 + i) >> 2)])
               bs = 1;
            else
               bs = rh265_mv_bs(cu2, top);
            d->hedge[(y0 >> 2) * d->w4 + ((x0 + i) >> 2)] = (uint8_t)bs;
         }
   }
   if (x0 > 0 && !(x0 & 7))
   {
      int slice_ok = rh265_zaddr(d, x0 - 1, y0) >= d->slice_start_zaddr;
      if (d->sh.loop_filter_across_slices || slice_ok ||
          (x0 & ((1 << d->sps->log2_ctb) - 1)))
         for (i = 0; i < size && y0 + i < d->sps->height; i += 4)
         {
            const rh265_mvfield *left = rh265_mvf_at(d, x0 - 1, y0 + i);
            const rh265_mvfield *cu2 = rh265_mvf_at(d, x0, y0 + i);
            int bs;
            if (cu2->pred == 0 || left->pred == 0)
               bs = 2;
            else if (d->nzc[((y0 + i) >> 2) * d->w4 + ((x0 - 1) >> 2)] ||
                     d->nzc[((y0 + i) >> 2) * d->w4 + (x0 >> 2)])
               bs = 1;
            else
               bs = rh265_mv_bs(cu2, left);
            d->vedge[((y0 + i) >> 2) * d->w4 + (x0 >> 2)] = (uint8_t)bs;
         }
   }
   if (log2_size > 2 && !is_intra)
   {
      for (j = 8; j < size; j += 8)
         for (i = 0; i < size && x0 + i < d->sps->width; i += 4)
         {
            if (y0 + j >= d->sps->height) break;
            d->hedge[((y0 + j) >> 2) * d->w4 + ((x0 + i) >> 2)] =
                  (uint8_t)rh265_mv_bs(rh265_mvf_at(d, x0 + i, y0 + j),
                        rh265_mvf_at(d, x0 + i, y0 + j - 1));
         }
      for (j = 0; j < size && y0 + j < d->sps->height; j += 4)
         for (i = 8; i < size; i += 8)
         {
            if (x0 + i >= d->sps->width) break;
            d->vedge[((y0 + j) >> 2) * d->w4 + ((x0 + i) >> 2)] =
                  (uint8_t)rh265_mv_bs(rh265_mvf_at(d, x0 + i, y0 + j),
                        rh265_mvf_at(d, x0 + i - 1, y0 + j));
         }
   }
}

static void rh265_fill_nzc(rh265_dec *d, int x0, int y0, int log2_size,
      int cbf)
{
   int size4 = 1 << (log2_size - 2);
   int x4 = x0 >> 2, y4 = y0 >> 2;
   int i, j;
   for (j = 0; j < size4 && y4 + j < d->h4; j++)
      for (i = 0; i < size4 && x4 + i < d->w4; i++)
         d->nzc[(y4 + j) * d->w4 + x4 + i] = (uint8_t)cbf;
}

static int rh265_transform_unit(rh265_dec *d, int x0, int y0,
      int xBase, int yBase, int cb_x, int cb_y, int log2_cb, int log2_size,
      int blk_idx, int cbf_luma, int cbf_cb, int cbf_cr)
{
   const rh265_pps *pps = d->pps;
   int scan_idx = RH265_SCAN_DIAG;
   int scan_idx_c = RH265_SCAN_DIAG;
   int luma_mode = d->intra_split
         ? d->intra_pred_mode[((y0 > cb_y) << 1) | (x0 > cb_x)]
         : d->intra_pred_mode[0];
   int chroma_mode = d->intra_pred_mode_c;
   (void)log2_cb;

   /* the luma prediction for this TB happens before its residual so the
    * reconstructed neighbours are in place for the next TB; inter CUs
    * were fully predicted at the PU stage */
   if (d->cu_pred_intra)
   {
      d->cur_zaddr = rh265_zaddr(d, x0, y0);
      d->fns->intra_pred(d, x0, y0, log2_size, 0, luma_mode);
   }
   else
      luma_mode = chroma_mode = -1;    /* DCT everywhere, no MDCS */

   if (cbf_luma || cbf_cb || cbf_cr)
   {
      if (pps->cu_qp_delta_enabled && !d->is_cu_qp_delta_coded)
      {
         int v = rh265_cu_qp_delta_abs(&d->cb);
         if (v == -1000) return -1;
         d->cu_qp_delta = v;
         if (v != 0 && rh265_cabac_bypass(&d->cb))
            d->cu_qp_delta = -v;
         d->is_cu_qp_delta_coded = 1;
         if (d->cu_qp_delta < -26 || d->cu_qp_delta > 25)
            return -1;
         rh265_set_qpy(d, cb_x, cb_y);
      }

      if (log2_size < 4 && d->cu_pred_intra)
      {
         if (luma_mode >= 6 && luma_mode <= 14)
            scan_idx = RH265_SCAN_VERT;
         else if (luma_mode >= 22 && luma_mode <= 30)
            scan_idx = RH265_SCAN_HORIZ;
         if (chroma_mode >= 6 && chroma_mode <= 14)
            scan_idx_c = RH265_SCAN_VERT;
         else if (chroma_mode >= 22 && chroma_mode <= 30)
            scan_idx_c = RH265_SCAN_HORIZ;
      }

      if (cbf_luma)
         if (rh265_residual_coding(d, x0, y0, log2_size, scan_idx, 0,
               luma_mode) < 0)
            return -1;
   }

   if (log2_size > 2)
   {
      if (d->cu_pred_intra)
      {
         d->cur_zaddr = rh265_zaddr(d, x0, y0);
         d->fns->intra_pred(d, x0, y0, log2_size - 1, 1, chroma_mode);
      }
      if (cbf_cb)
         if (rh265_residual_coding(d, x0, y0, log2_size - 1, scan_idx_c, 1,
               chroma_mode) < 0)
            return -1;
      if (d->cu_pred_intra)
      {
         d->cur_zaddr = rh265_zaddr(d, x0, y0);
         d->fns->intra_pred(d, x0, y0, log2_size - 1, 2, chroma_mode);
      }
      if (cbf_cr)
         if (rh265_residual_coding(d, x0, y0, log2_size - 1, scan_idx_c, 2,
               chroma_mode) < 0)
            return -1;
   }
   else if (blk_idx == 3)
   {
      if (d->cu_pred_intra)
      {
         d->cur_zaddr = rh265_zaddr(d, xBase, yBase);
         d->fns->intra_pred(d, xBase, yBase, log2_size, 1, chroma_mode);
      }
      if (cbf_cb)
         if (rh265_residual_coding(d, xBase, yBase, log2_size, scan_idx_c, 1,
               chroma_mode) < 0)
            return -1;
      if (d->cu_pred_intra)
      {
         d->cur_zaddr = rh265_zaddr(d, xBase, yBase);
         d->fns->intra_pred(d, xBase, yBase, log2_size, 2, chroma_mode);
      }
      if (cbf_cr)
         if (rh265_residual_coding(d, xBase, yBase, log2_size, scan_idx_c, 2,
               chroma_mode) < 0)
            return -1;
   }
   return 0;
}

static int rh265_transform_tree(rh265_dec *d, int x0, int y0,
      int xBase, int yBase, int cb_x, int cb_y, int log2_cb, int log2_size,
      int depth, int blk_idx, int base_cbf_cb, int base_cbf_cr)
{
   const rh265_sps *sps = d->sps;
   int split;
   int cbf_cb = base_cbf_cb;
   int cbf_cr = base_cbf_cr;
   int inter_split = !d->cu_pred_intra && depth == 0 &&
         d->part_mode != RH265_PART_2Nx2N &&
         sps->max_transform_hierarchy_depth_inter == 0;
   int max_depth = d->cu_pred_intra
         ? sps->max_transform_hierarchy_depth_intra
               + (d->intra_split ? 1 : 0)
         : sps->max_transform_hierarchy_depth_inter;

   if (log2_size <= sps->log2_max_tb &&
       log2_size >  sps->log2_min_tb &&
       depth < max_depth &&
       !(d->intra_split && depth == 0) && !inter_split)
      split = rh265_cabac_decode(&d->cb,
            RH265_CTX_SPLIT_TRANSFORM_FLAG + 5 - log2_size);
   else
      split = (log2_size > sps->log2_max_tb) ||
              (d->intra_split && depth == 0) || inter_split;

   if (log2_size > 2)
   {
      if (depth == 0 || cbf_cb)
         cbf_cb = rh265_cabac_decode(&d->cb, RH265_CTX_CBF_CB_CR + depth);
      if (depth == 0 || cbf_cr)
         cbf_cr = rh265_cabac_decode(&d->cb, RH265_CTX_CBF_CB_CR + depth);
   }

   if (split)
   {
      int half = 1 << (log2_size - 1);
      if (rh265_transform_tree(d, x0, y0, x0, y0, cb_x, cb_y,
            log2_cb, log2_size - 1, depth + 1, 0, cbf_cb, cbf_cr) < 0)
         return -1;
      if (rh265_transform_tree(d, x0 + half, y0, x0, y0, cb_x, cb_y,
            log2_cb, log2_size - 1, depth + 1, 1, cbf_cb, cbf_cr) < 0)
         return -1;
      if (rh265_transform_tree(d, x0, y0 + half, x0, y0, cb_x, cb_y,
            log2_cb, log2_size - 1, depth + 1, 2, cbf_cb, cbf_cr) < 0)
         return -1;
      if (rh265_transform_tree(d, x0 + half, y0 + half, x0, y0, cb_x, cb_y,
            log2_cb, log2_size - 1, depth + 1, 3, cbf_cb, cbf_cr) < 0)
         return -1;
   }
   else
   {
      int cbf_luma = 1;
      if (d->cu_pred_intra || depth != 0 || cbf_cb || cbf_cr)
         cbf_luma = rh265_cabac_decode(&d->cb,
               RH265_CTX_CBF_LUMA + (depth ? 0 : 1));
      rh265_fill_nzc(d, x0, y0, log2_size, cbf_luma);
      rh265_bs_edges(d, x0, y0, log2_size);
      if (rh265_transform_unit(d, x0, y0, xBase, yBase, cb_x, cb_y,
            log2_cb, log2_size, blk_idx, cbf_luma, cbf_cb, cbf_cr) < 0)
         return -1;
   }
   return 0;
}

/* ==================== rh265_mc.h ==================== */

/* Inter prediction (8.5.4).  The arithmetic mirrors FFmpeg's 8-bit
 * dsp_template kernels exactly: the intermediate domain is sample*64
 * held in int (the "14-bit" domain at bit depth 8), uni prediction
 * rounds with (+32 >> 6), bi prediction averages two intermediates
 * with (+64 >> 7), and the weighted variants follow 8.5.4.2.3. */

static const int8_t rh265_qpel_filt[4][8] =
{
   {  0,  0,  0, 64,  0,  0,  0,  0 },
   { -1,  4,-10, 58, 17, -5,  1,  0 },
   { -1,  4,-11, 40, 40,-11,  4, -1 },
   {  0,  1, -5, 17, 58,-10,  4, -1 }
};
static const int8_t rh265_epel_filt[8][4] =
{
   {  0, 64,  0,  0 },
   { -2, 58, 10, -2 },
   { -4, 54, 16, -2 },
   { -6, 46, 28, -4 },
   { -4, 36, 36, -4 },
   { -4, 28, 46, -6 },
   { -2, 16, 54, -4 },
   { -2, 10, 58, -2 }
};


/* Fetch a (w x h) source patch whose top-left is (x, y) in the given
 * reference plane, with picture-edge clamping, into a tightly packed
 * buffer.  Coordinates may be negative or beyond the plane. */
/* ==================== rh265_mv.h ==================== */

/* Merge and AMVP candidate derivation (8.5.3).  The structure follows
 * FFmpeg's mvs.c, which implements the clause faithfully; reference
 * comparisons use stored POCs so neighbouring slices with different
 * lists need no translation. */


/* 8.5.3.2.8 motion vector scaling by POC distance */
static void rh265_mv_scale(rh265_mv *dst, const rh265_mv *src,
      int td, int tb)
{
   int tx, sf, v;
   td = rh265_clip_int8(td);
   tb = rh265_clip_int8(tb);
   tx = (0x4000 + (td < 0 ? -td / 2 : td / 2)) / td;
   sf = (tb * tx + 32) >> 6;
   sf = sf < -4096 ? -4096 : (sf > 4095 ? 4095 : sf);
   v = sf * src->x;
   dst->x = (int16_t)rh265_clip_int16((v + 127 + (v < 0)) >> 8);
   v = sf * src->y;
   dst->y = (int16_t)rh265_clip_int16((v + 127 + (v < 0)) >> 8);
}

static int rh265_same_mer(const rh265_dec *d, int xN, int yN,
      int xP, int yP)
{
   int l = d->pps->log2_parallel_merge_level;
   return (xN >> l) == (xP >> l) && (yN >> l) == (yP >> l);
}

static int rh265_mvf_equal(const rh265_mvfield *a, const rh265_mvfield *b)
{
   if (a->pred != b->pred)
      return 0;
   if (a->pred == RH265_PF_BI)
      return a->ref_idx[0] == b->ref_idx[0]
          && a->mv[0].x == b->mv[0].x && a->mv[0].y == b->mv[0].y
          && a->ref_idx[1] == b->ref_idx[1]
          && a->mv[1].x == b->mv[1].x && a->mv[1].y == b->mv[1].y;
   if (a->pred == RH265_PF_L0)
      return a->ref_idx[0] == b->ref_idx[0]
          && a->mv[0].x == b->mv[0].x && a->mv[0].y == b->mv[0].y;
   if (a->pred == RH265_PF_L1)
      return a->ref_idx[1] == b->ref_idx[1]
          && a->mv[1].x == b->mv[1].x && a->mv[1].y == b->mv[1].y;
   return 0;
}

/* 8.5.3.1.8: map one collocated MvField entry onto the current
 * reference, scaling by POC distance when they differ */
static int rh265_tmvp_mvset(const rh265_dec *d, rh265_mv *out,
      const rh265_mvfield *col, int list_col, int X, int ref_idx,
      int col_poc)
{
   int col_diff = col_poc - col->poc[list_col];
   int cur_diff = d->cur->poc - d->ref_poc[X][ref_idx];
   if (col_diff == cur_diff || !col_diff)
      *out = col->mv[list_col];
   else
      rh265_mv_scale(out, &col->mv[list_col], col_diff, cur_diff);
   return 1;
}

static int rh265_tmvp_colocated(const rh265_dec *d,
      const rh265_mvfield *col, int ref_idx, rh265_mv *out, int X,
      int col_poc)
{
   if (col->pred == 0)
      return 0;
   if (!(col->pred & RH265_PF_L0))
      return rh265_tmvp_mvset(d, out, col, 1, X, ref_idx, col_poc);
   else if (col->pred == RH265_PF_L0)
      return rh265_tmvp_mvset(d, out, col, 0, X, ref_idx, col_poc);
   else
   {
      int has_future = 0;
      int i, l;
      for (l = 0; l < 2; l++)
         for (i = 0; i < d->nb_refs[l]; i++)
            if (d->ref_poc[l][i] > d->cur->poc)
            { has_future = 1; l = 2; break; }
      if (!has_future)
         return rh265_tmvp_mvset(d, out, col, X, X, ref_idx, col_poc);
      return rh265_tmvp_mvset(d, out, col,
            d->sh.collocated_from_l0 ? 1 : 0, X, ref_idx, col_poc);
   }
}

/* 8.5.3.1.7 temporal luma motion vector prediction */
static int rh265_temporal_mv(const rh265_dec *d, int x0, int y0,
      int nPbW, int nPbH, int ref_idx, rh265_mv *out, int X)
{
   const rh265_sps *sps = d->sps;
   const rh265_pic *ref = d->col_ref;
   int x, y;
   if (!ref || !ref->mvf)
   {
      out->x = out->y = 0;
      return 0;
   }
   /* bottom-right collocated block */
   x = x0 + nPbW;
   y = y0 + nPbH;
   if ((y0 >> sps->log2_ctb) == (y >> sps->log2_ctb) &&
       y < sps->height && x < sps->width)
   {
      const rh265_mvfield *col =
            &ref->mvf[((y & ~15) >> 2) * d->w4 + ((x & ~15) >> 2)];
      if (rh265_tmvp_colocated(d, col, ref_idx, out, X, ref->poc))
         return 1;
   }
   /* centre collocated block */
   x = (x0 + (nPbW >> 1)) & ~15;
   y = (y0 + (nPbH >> 1)) & ~15;
   {
      const rh265_mvfield *col =
            &ref->mvf[(y >> 2) * d->w4 + (x >> 2)];
      return rh265_tmvp_colocated(d, col, ref_idx, out, X, ref->poc);
   }
}

/* Prediction-block neighbour availability (6.4.2), following the
 * reference decoder split: the left, above and above-left neighbours of
 * a prediction block are always decoded when they fall inside the
 * picture and the current slice, because prediction units within a CU
 * reconstruct in coding order regardless of the 4x4 z-scan; only the
 * two diagonal positions (above-right, below-left) additionally need
 * the 6.4.1 z-scan check because they can point at undecoded blocks. */
typedef struct
{
   int left, up, up_left, up_right, bottom_left;
} rh265_pb_na;

/* has the CTB at raster position (rx, ry) been decoded in this slice */
static RH265_INLINE int rh265_ctb_avail(const rh265_dec *d, int rx, int ry)
{
   if (rx < 0 || ry < 0 || rx >= d->sps->ctb_w)
      return 0;
   return ry * d->sps->ctb_w + rx >= d->slice_start_ctb;
}

static void rh265_set_pb_na(const rh265_dec *d, int x0, int y0,
      int nPbW, int nPbH, rh265_pb_na *na)
{
   int log2 = d->sps->log2_ctb;
   int mask = (1 << log2) - 1;
   int x0b = x0 & mask, y0b = y0 & mask;
   int rx = x0 >> log2, ry = y0 >> log2;
   int ctb_left    = rh265_ctb_avail(d, rx - 1, ry);
   int ctb_up      = rh265_ctb_avail(d, rx, ry - 1);
   int ctb_up_left = rh265_ctb_avail(d, rx - 1, ry - 1);
   int ctb_up_rght = rh265_ctb_avail(d, rx + 1, ry - 1);
   int ctb_bottom  = (ry + 1) << log2;
   int end_y = ctb_bottom < d->sps->height ? ctb_bottom : d->sps->height;
   na->left    = ctb_left || x0b;
   na->up      = ctb_up || y0b;
   na->up_left = (x0b || y0b) ? (na->left && na->up) : ctb_up_left;
   na->up_right = (x0b + nPbW == 1 << log2)
         ? (ctb_up_rght && !y0b) : na->up;
   na->bottom_left = (y0 + nPbH >= end_y) ? 0 : na->left;
}

/* 6.4.1 z-scan order block availability for the diagonal candidates */
static RH265_INLINE int rh265_zscan_avail(const rh265_dec *d,
      int xc, int yc, int xn, int yn)
{
   int log2 = d->sps->log2_ctb;
   if ((yn >> log2) < (yc >> log2) || (xn >> log2) < (xc >> log2))
      return 1;
   return rh265_zaddr(d, xn, yn) <= rh265_zaddr(d, xc, yc);
}

/* gate flag plus the not-intra requirement; the flag must imply the
 * position is inside the picture before the motion field is read */
#define RH265_MV_CAND(gate, xx, yy) \
   ((gate) && rh265_mvf_at(d, xx, yy)->pred != 0)

static const uint8_t rh265_l0_l1_cand[12][2] =
{
   {0,1},{1,0},{0,2},{2,0},{1,2},{2,1},
   {0,3},{3,0},{1,3},{3,1},{2,3},{3,2}
};

/* 8.5.3.1.1/2 merge candidate list; early-outs once merge_idx is
 * reached, exactly like the reference structure it follows */
static void rh265_merge_mode(rh265_dec *d, int x0, int y0,
      int nPbW, int nPbH, int part_idx, int merge_idx, rh265_mvfield *mv)
{
   const int is_b = (d->sh.slice_type == RH265_SLICE_B);
   int singleMCL = 0;
   rh265_mvfield cand[5];
   int nb = 0, nb_orig, zero_idx = 0;
   int xA1, yA1, xB1, yB1, xB0, yB0, xA0, yA0, xB2, yB2;
   int avail_a1 = 0, avail_b1 = 0;
   rh265_pb_na na;
   int nb_refs = is_b
         ? (d->nb_refs[0] < d->nb_refs[1] ? d->nb_refs[0] : d->nb_refs[1])
         : d->nb_refs[0];
   int nPbW2 = nPbW, nPbH2 = nPbH;

   if (d->pps->log2_parallel_merge_level > 2 && d->cu_log2 == 3)
   {
      singleMCL = 1;
      x0 = d->cu_x;
      y0 = d->cu_y;
      nPbW = nPbH = 8;
      part_idx = 0;
   }
   rh265_set_pb_na(d, x0, y0, nPbW, nPbH, &na);

   xA1 = x0 - 1;        yA1 = y0 + nPbH - 1;
   xB1 = x0 + nPbW - 1; yB1 = y0 - 1;
   xB0 = x0 + nPbW;     yB0 = y0 - 1;
   xA0 = x0 - 1;        yA0 = y0 + nPbH;
   xB2 = x0 - 1;        yB2 = y0 - 1;

   memset(cand, 0, sizeof(cand));

   if ((!singleMCL && part_idx == 1 &&
        (d->part_mode == RH265_PART_Nx2N ||
         d->part_mode == RH265_PART_nLx2N ||
         d->part_mode == RH265_PART_nRx2N)) ||
       rh265_same_mer(d, xA1, yA1, x0, y0))
      avail_a1 = 0;
   else
   {
      avail_a1 = RH265_MV_CAND(na.left, xA1, yA1);
      if (avail_a1)
      {
         cand[nb] = *rh265_mvf_at(d, xA1, yA1);
         if (merge_idx == 0) goto done;
         nb++;
      }
   }

   if ((!singleMCL && part_idx == 1 &&
        (d->part_mode == RH265_PART_2NxN ||
         d->part_mode == RH265_PART_2NxnU ||
         d->part_mode == RH265_PART_2NxnD)) ||
       rh265_same_mer(d, xB1, yB1, x0, y0))
      avail_b1 = 0;
   else
   {
      avail_b1 = RH265_MV_CAND(na.up, xB1, yB1);
      if (avail_b1 &&
          !(avail_a1 && rh265_mvf_equal(rh265_mvf_at(d, xB1, yB1),
                rh265_mvf_at(d, xA1, yA1))))
      {
         cand[nb] = *rh265_mvf_at(d, xB1, yB1);
         if (merge_idx == nb) goto done;
         nb++;
      }
   }

   if (RH265_MV_CAND(na.up_right, xB0, yB0) &&
       xB0 < d->sps->width &&
       rh265_zscan_avail(d, x0, y0, xB0, yB0) &&
       !rh265_same_mer(d, xB0, yB0, x0, y0) &&
       !(avail_b1 && rh265_mvf_equal(rh265_mvf_at(d, xB0, yB0),
             rh265_mvf_at(d, xB1, yB1))))
   {
      cand[nb] = *rh265_mvf_at(d, xB0, yB0);
      if (merge_idx == nb) goto done;
      nb++;
   }

   if (RH265_MV_CAND(na.bottom_left, xA0, yA0) &&
       yA0 < d->sps->height &&
       rh265_zscan_avail(d, x0, y0, xA0, yA0) &&
       !rh265_same_mer(d, xA0, yA0, x0, y0) &&
       !(avail_a1 && rh265_mvf_equal(rh265_mvf_at(d, xA0, yA0),
             rh265_mvf_at(d, xA1, yA1))))
   {
      cand[nb] = *rh265_mvf_at(d, xA0, yA0);
      if (merge_idx == nb) goto done;
      nb++;
   }

   if (nb != 4 &&
       RH265_MV_CAND(na.up_left, xB2, yB2) &&
       !rh265_same_mer(d, xB2, yB2, x0, y0) &&
       !(avail_a1 && rh265_mvf_equal(rh265_mvf_at(d, xB2, yB2),
             rh265_mvf_at(d, xA1, yA1))) &&
       !(avail_b1 && rh265_mvf_equal(rh265_mvf_at(d, xB2, yB2),
             rh265_mvf_at(d, xB1, yB1))))
   {
      cand[nb] = *rh265_mvf_at(d, xB2, yB2);
      if (merge_idx == nb) goto done;
      nb++;
   }

   if (d->slice_tmvp && nb < d->max_merge)
   {
      rh265_mv c0, c1;
      int a0, a1;
      c0.x = c0.y = c1.x = c1.y = 0;
      a0 = rh265_temporal_mv(d, x0, y0, nPbW, nPbH, 0, &c0, 0);
      a1 = is_b ? rh265_temporal_mv(d, x0, y0, nPbW, nPbH, 0, &c1, 1) : 0;
      if (a0 || a1)
      {
         cand[nb].pred = (uint8_t)(a0 + (a1 << 1));
         cand[nb].ref_idx[0] = 0;
         cand[nb].ref_idx[1] = 0;
         cand[nb].mv[0] = c0;
         cand[nb].mv[1] = c1;
         cand[nb].poc[0] = d->ref_poc[0][0];
         cand[nb].poc[1] = is_b ? d->ref_poc[1][0] : 0;
         if (merge_idx == nb) goto done;
         nb++;
      }
   }

   nb_orig = nb;
   if (is_b && nb_orig > 1 && nb_orig < d->max_merge)
   {
      int ci;
      for (ci = 0; nb < d->max_merge &&
            ci < nb_orig * (nb_orig - 1); ci++)
      {
         const rh265_mvfield *l0c = &cand[rh265_l0_l1_cand[ci][0]];
         const rh265_mvfield *l1c = &cand[rh265_l0_l1_cand[ci][1]];
         if ((l0c->pred & RH265_PF_L0) && (l1c->pred & RH265_PF_L1) &&
             (l0c->poc[0] != l1c->poc[1] ||
              l0c->mv[0].x != l1c->mv[1].x ||
              l0c->mv[0].y != l1c->mv[1].y))
         {
            cand[nb].pred = RH265_PF_BI;
            cand[nb].ref_idx[0] = l0c->ref_idx[0];
            cand[nb].ref_idx[1] = l1c->ref_idx[1];
            cand[nb].mv[0]  = l0c->mv[0];
            cand[nb].mv[1]  = l1c->mv[1];
            cand[nb].poc[0] = l0c->poc[0];
            cand[nb].poc[1] = l1c->poc[1];
            if (merge_idx == nb) goto done;
            nb++;
         }
      }
   }

   while (nb <= merge_idx)
   {
      int zr = zero_idx < nb_refs ? zero_idx : 0;
      cand[nb].pred = (uint8_t)(RH265_PF_L0 + (is_b << 1));
      cand[nb].mv[0].x = cand[nb].mv[0].y = 0;
      cand[nb].mv[1].x = cand[nb].mv[1].y = 0;
      cand[nb].ref_idx[0] = (int8_t)zr;
      cand[nb].ref_idx[1] = (int8_t)zr;
      cand[nb].poc[0] = d->nb_refs[0] ? d->ref_poc[0][zr] : 0;
      cand[nb].poc[1] = is_b && d->nb_refs[1] ? d->ref_poc[1][zr] : 0;
      if (merge_idx == nb) break;
      nb++;
      zero_idx++;
   }
done:
   *mv = cand[merge_idx > nb ? nb : merge_idx];
   if (mv->pred == RH265_PF_BI && (nPbW2 + nPbH2) == 12)
      mv->pred = RH265_PF_L0;
}

/* one AMVP spatial probe: same reference picture, no scaling */
static int rh265_amvp_direct(const rh265_dec *d, int x, int y, int pfi,
      rh265_mv *mv, int lx, int ref_idx)
{
   const rh265_mvfield *f = rh265_mvf_at(d, x, y);
   if ((f->pred & (1 << pfi)) &&
       f->poc[pfi] == d->ref_poc[lx][ref_idx])
   {
      *mv = f->mv[pfi];
      return 1;
   }
   return 0;
}

/* one AMVP spatial probe with POC-distance scaling */
static int rh265_amvp_scaled(const rh265_dec *d, int x, int y, int pfi,
      rh265_mv *mv, int lx, int ref_idx)
{
   const rh265_mvfield *f = rh265_mvf_at(d, x, y);
   if (f->pred & (1 << pfi))
   {
      *mv = f->mv[pfi];
      if (f->poc[pfi] != d->ref_poc[lx][ref_idx])
      {
         int diff = d->cur->poc - f->poc[pfi];
         if (!diff) diff = 1;
         rh265_mv_scale(mv, mv, diff, d->cur->poc - d->ref_poc[lx][ref_idx]);
      }
      return 1;
   }
   return 0;
}

/* 8.5.3.1.5/6 AMVP: fill mv->mv[lx] from the predictor selected by
 * mvp_flag; mv->ref_idx[lx] must be set on entry */
static void rh265_amvp_mode(rh265_dec *d, int x0, int y0,
      int nPbW, int nPbH, rh265_mvfield *mv, int mvp_flag, int lx)
{
   rh265_pb_na na;
   int ref_idx = mv->ref_idx[lx];
   int pf0 = lx, pf1 = !lx;
   int is_scaled = 0, avail_a = 1, avail_b = 1, ncand = 0;
   int xA0, yA0, xA1, yA1, xB0, yB0, xB1, yB1, xB2, yB2;
   int is_a0, is_a1, is_b0, is_b1, is_b2;
   rh265_mv list[2];
   rh265_mv mxA, mxB;
   mxA.x = mxA.y = mxB.x = mxB.y = 0;
   list[0].x = list[0].y = list[1].x = list[1].y = 0;

   rh265_set_pb_na(d, x0, y0, nPbW, nPbH, &na);
   xA0 = x0 - 1; yA0 = y0 + nPbH;
   xA1 = x0 - 1; yA1 = y0 + nPbH - 1;
   is_a0 = RH265_MV_CAND(na.bottom_left, xA0, yA0) &&
         yA0 < d->sps->height &&
         rh265_zscan_avail(d, x0, y0, xA0, yA0);
   is_a1 = RH265_MV_CAND(na.left, xA1, yA1);
   if (is_a0 || is_a1)
      is_scaled = 1;

   if (is_a0 && (rh265_amvp_direct(d, xA0, yA0, pf0, &mxA, lx, ref_idx) ||
                 rh265_amvp_direct(d, xA0, yA0, pf1, &mxA, lx, ref_idx)))
      goto b_cand;
   if (is_a1 && (rh265_amvp_direct(d, xA1, yA1, pf0, &mxA, lx, ref_idx) ||
                 rh265_amvp_direct(d, xA1, yA1, pf1, &mxA, lx, ref_idx)))
      goto b_cand;
   if (is_a0 && (rh265_amvp_scaled(d, xA0, yA0, pf0, &mxA, lx, ref_idx) ||
                 rh265_amvp_scaled(d, xA0, yA0, pf1, &mxA, lx, ref_idx)))
      goto b_cand;
   if (is_a1 && (rh265_amvp_scaled(d, xA1, yA1, pf0, &mxA, lx, ref_idx) ||
                 rh265_amvp_scaled(d, xA1, yA1, pf1, &mxA, lx, ref_idx)))
      goto b_cand;
   avail_a = 0;

b_cand:
   xB0 = x0 + nPbW;     yB0 = y0 - 1;
   xB1 = x0 + nPbW - 1; yB1 = y0 - 1;
   xB2 = x0 - 1;        yB2 = y0 - 1;
   is_b0 = RH265_MV_CAND(na.up_right, xB0, yB0) &&
         xB0 < d->sps->width &&
         rh265_zscan_avail(d, x0, y0, xB0, yB0);
   is_b1 = RH265_MV_CAND(na.up, xB1, yB1);
   is_b2 = RH265_MV_CAND(na.up_left, xB2, yB2);

   if (is_b0 && (rh265_amvp_direct(d, xB0, yB0, pf0, &mxB, lx, ref_idx) ||
                 rh265_amvp_direct(d, xB0, yB0, pf1, &mxB, lx, ref_idx)))
      goto scalef;
   if (is_b1 && (rh265_amvp_direct(d, xB1, yB1, pf0, &mxB, lx, ref_idx) ||
                 rh265_amvp_direct(d, xB1, yB1, pf1, &mxB, lx, ref_idx)))
      goto scalef;
   if (is_b2 && (rh265_amvp_direct(d, xB2, yB2, pf0, &mxB, lx, ref_idx) ||
                 rh265_amvp_direct(d, xB2, yB2, pf1, &mxB, lx, ref_idx)))
      goto scalef;
   avail_b = 0;

scalef:
   if (!is_scaled)
   {
      if (avail_b)
      {
         avail_a = 1;
         mxA = mxB;
      }
      avail_b = 0;
      if (is_b0)
      {
         avail_b = rh265_amvp_scaled(d, xB0, yB0, pf0, &mxB, lx, ref_idx);
         if (!avail_b)
            avail_b = rh265_amvp_scaled(d, xB0, yB0, pf1, &mxB, lx, ref_idx);
      }
      if (is_b1 && !avail_b)
      {
         avail_b = rh265_amvp_scaled(d, xB1, yB1, pf0, &mxB, lx, ref_idx);
         if (!avail_b)
            avail_b = rh265_amvp_scaled(d, xB1, yB1, pf1, &mxB, lx, ref_idx);
      }
      if (is_b2 && !avail_b)
      {
         avail_b = rh265_amvp_scaled(d, xB2, yB2, pf0, &mxB, lx, ref_idx);
         if (!avail_b)
            avail_b = rh265_amvp_scaled(d, xB2, yB2, pf1, &mxB, lx, ref_idx);
      }
   }

   if (avail_a)
      list[ncand++] = mxA;
   if (avail_b && (!avail_a || mxA.x != mxB.x || mxA.y != mxB.y))
      list[ncand++] = mxB;
   if (ncand < 2 && d->slice_tmvp && mvp_flag == ncand)
   {
      rh265_mv col;
      if (rh265_temporal_mv(d, x0, y0, nPbW, nPbH, ref_idx, &col, lx))
         list[ncand++] = col;
   }
   mv->mv[lx] = list[mvp_flag];
}

/* ==================== rh265_cu.h ==================== */

/* 8.4.2 derivation of the luma intra prediction mode via the three most
 * probable modes. */
static int rh265_luma_intra_mode(rh265_dec *d, int x0, int y0, int pu_size,
      int prev_flag, int mpm_or_rem)
{
   const rh265_sps *sps = d->sps;
   int x4 = x0 >> 2, y4 = y0 >> 2;
   int cand_left = 1, cand_up = 1;
   int candidate[3];
   int mode;
   int size4, i, j;
   int cur = rh265_zaddr(d, x0, y0);

   if (rh265_avail(d, x0 - 1, y0, cur))
      cand_left = d->ipm[y4 * d->w4 + (x4 - 1)];
   if (y0 > 0 && ((y0 - 1) >> sps->log2_ctb) == (y0 >> sps->log2_ctb) &&
       rh265_avail(d, x0, y0 - 1, cur))
      cand_up = d->ipm[(y4 - 1) * d->w4 + x4];

   if (cand_left == cand_up)
   {
      if (cand_left < 2)
      {
         candidate[0] = 0;
         candidate[1] = 1;
         candidate[2] = 26;
      }
      else
      {
         candidate[0] = cand_left;
         candidate[1] = 2 + ((cand_left - 2 - 1 + 32) & 31);
         candidate[2] = 2 + ((cand_left - 2 + 1) & 31);
      }
   }
   else
   {
      candidate[0] = cand_left;
      candidate[1] = cand_up;
      if (candidate[0] != 0 && candidate[1] != 0)
         candidate[2] = 0;
      else if (candidate[0] != 1 && candidate[1] != 1)
         candidate[2] = 1;
      else
         candidate[2] = 26;
   }

   if (prev_flag)
      mode = candidate[mpm_or_rem];
   else
   {
      int t;
      if (candidate[0] > candidate[1])
      { t = candidate[0]; candidate[0] = candidate[1]; candidate[1] = t; }
      if (candidate[0] > candidate[2])
      { t = candidate[0]; candidate[0] = candidate[2]; candidate[2] = t; }
      if (candidate[1] > candidate[2])
      { t = candidate[1]; candidate[1] = candidate[2]; candidate[2] = t; }
      mode = mpm_or_rem;
      for (i = 0; i < 3; i++)
         if (mode >= candidate[i])
            mode++;
   }

   size4 = pu_size >> 2;
   if (!size4) size4 = 1;
   for (i = 0; i < size4; i++)
      for (j = 0; j < size4; j++)
         d->ipm[(y4 + i) * d->w4 + x4 + j] = (uint8_t)mode;
   return mode;
}

/* 7.3.8.6 prediction_unit: parse, derive the motion field, store it,
 * and run motion compensation. */
static int rh265_pred_unit(rh265_dec *d, int x0, int y0, int w, int h,
      int part_idx, int is_skip)
{
   rh265_cabac *cb = &d->cb;
   rh265_mvfield mv;
   int merge = 1;
   int i, j, l;

   memset(&mv, 0, sizeof(mv));
   if (!is_skip)
      merge = rh265_cabac_decode(cb, RH265_CTX_MERGE_FLAG);
   if (merge)
   {
      int merge_idx = 0;
      if (d->max_merge > 1)
      {
         merge_idx = rh265_cabac_decode(cb, RH265_CTX_MERGE_IDX);
         if (merge_idx)
            while (merge_idx < d->max_merge - 1 && rh265_cabac_bypass(cb))
               merge_idx++;
      }
      rh265_merge_mode(d, x0, y0, w, h, part_idx, merge_idx, &mv);
   }
   else
   {
      int idc = RH265_PF_L0;
      int mvd_x[2], mvd_y[2];
      mvd_x[0] = mvd_y[0] = mvd_x[1] = mvd_y[1] = 0;
      if (d->sh.slice_type == RH265_SLICE_B)
      {
         if (w + h != 12)
         {
            if (rh265_cabac_decode(cb,
                  RH265_CTX_INTER_PRED_IDC + d->ct_depth_cur))
               idc = RH265_PF_BI;
            else
               idc = rh265_cabac_decode(cb, RH265_CTX_INTER_PRED_IDC + 4)
                     ? RH265_PF_L1 : RH265_PF_L0;
         }
         else
            idc = rh265_cabac_decode(cb, RH265_CTX_INTER_PRED_IDC + 4)
                  ? RH265_PF_L1 : RH265_PF_L0;
      }
      mv.pred = (uint8_t)idc;
      for (l = 0; l < 2; l++)
      {
         int mvp_flag;
         if (!(idc & (1 << l)))
            continue;
         /* ref_idx_lX */
         if (d->nb_refs[l] > 1)
         {
            int ri = 0;
            int max = d->nb_refs[l] - 1;
            int max_ctx = max < 2 ? max : 2;
            /* both lists share the same two ref_idx contexts; the L1
             * element slots in the init table are unused padding */
            while (ri < max_ctx && rh265_cabac_decode(cb,
                  RH265_CTX_REF_IDX_L0 + ri))
               ri++;
            if (ri == 2)
               while (ri < max && rh265_cabac_bypass(cb))
                  ri++;
            mv.ref_idx[l] = (int8_t)ri;
         }
         /* mvd_coding, suppressed for L1 under mvd_l1_zero on BI */
         if (!(l == 1 && d->sh.mvd_l1_zero && idc == RH265_PF_BI))
         {
            int gx = rh265_cabac_decode(cb, RH265_CTX_ABS_MVD_GREATER0_FLAG);
            int gy = rh265_cabac_decode(cb, RH265_CTX_ABS_MVD_GREATER0_FLAG);
            /* the live greater1 context sits in the second slot of its
             * 2-entry element (first is unused padding, as in the
             * reference init tables) */
            if (gx)
               gx += rh265_cabac_decode(cb,
                     RH265_CTX_ABS_MVD_GREATER1_FLAG + 1);
            if (gy)
               gy += rh265_cabac_decode(cb,
                     RH265_CTX_ABS_MVD_GREATER1_FLAG + 1);
            if (gx == 2)
            {
               int v = 2, k = 1;
               while (k < 30 && rh265_cabac_bypass(cb))
               {
                  v += 1 << k;
                  k++;
               }
               while (k--)
                  v += (int)rh265_cabac_bypass(cb) << k;
               mvd_x[l] = rh265_cabac_bypass(cb) ? -v : v;
            }
            else if (gx == 1)
               mvd_x[l] = rh265_cabac_bypass(cb) ? -1 : 1;
            if (gy == 2)
            {
               int v = 2, k = 1;
               while (k < 30 && rh265_cabac_bypass(cb))
               {
                  v += 1 << k;
                  k++;
               }
               while (k--)
                  v += (int)rh265_cabac_bypass(cb) << k;
               mvd_y[l] = rh265_cabac_bypass(cb) ? -v : v;
            }
            else if (gy == 1)
               mvd_y[l] = rh265_cabac_bypass(cb) ? -1 : 1;
         }
         mvp_flag = rh265_cabac_decode(cb, RH265_CTX_MVP_LX_FLAG);
         rh265_amvp_mode(d, x0, y0, w, h, &mv, mvp_flag, l);
         mv.mv[l].x = (int16_t)(mv.mv[l].x + mvd_x[l]);
         mv.mv[l].y = (int16_t)(mv.mv[l].y + mvd_y[l]);
      }
   }

   /* reference POCs always come from the current slice's lists */
   for (l = 0; l < 2; l++)
      if (mv.pred & (1 << l))
      {
         if (mv.ref_idx[l] >= d->nb_refs[l])
            return -1;
         mv.poc[l] = d->ref_poc[l][mv.ref_idx[l]];
      }

   for (j = 0; j < (h >> 2); j++)
      for (i = 0; i < (w >> 2); i++)
      {
         int y4 = (y0 >> 2) + j, x4 = (x0 >> 2) + i;
         if (y4 < d->h4 && x4 < d->w4)
            d->mvf[y4 * d->w4 + x4] = mv;
      }

   d->fns->mc_pu(d, x0, y0, w, h, &mv);
   return merge;
}

/* prediction-unit geometry per part mode: offsets and sizes in
 * quarters of the CU side (part_geom[mode][pu] = {x, y, w, h}/4) */
static const uint8_t rh265_part_geom[8][4][4] =
{
   { {0,0,4,4}, {0,0,0,0}, {0,0,0,0}, {0,0,0,0} },   /* 2Nx2N */
   { {0,0,4,2}, {0,2,4,2}, {0,0,0,0}, {0,0,0,0} },   /* 2NxN  */
   { {0,0,2,4}, {2,0,2,4}, {0,0,0,0}, {0,0,0,0} },   /* Nx2N  */
   { {0,0,2,2}, {2,0,2,2}, {0,2,2,2}, {2,2,2,2} },   /* NxN   */
   { {0,0,4,1}, {0,1,4,3}, {0,0,0,0}, {0,0,0,0} },   /* 2NxnU */
   { {0,0,4,3}, {0,3,4,1}, {0,0,0,0}, {0,0,0,0} },   /* 2NxnD */
   { {0,0,1,4}, {1,0,3,4}, {0,0,0,0}, {0,0,0,0} },   /* nLx2N */
   { {0,0,3,4}, {3,0,1,4}, {0,0,0,0}, {0,0,0,0} }    /* nRx2N */
};
static const uint8_t rh265_part_count[8] = { 1, 2, 2, 4, 2, 2, 2, 2 };

static int rh265_coding_unit(rh265_dec *d, int x0, int y0, int log2_cb)
{
   const rh265_sps *sps = d->sps;
   static const uint8_t chroma_tab[4] = { 0, 26, 10, 1 };
   int cb_size = 1 << log2_cb;
   int part_nxn = 0;
   int side, pb_size;
   int i, j;
   uint8_t prev_flag[4];
   int mpm_or_rem[4];
   int is_pb = (d->sh.slice_type != RH265_SLICE_I);
   int skip = 0;
   int merge_2nx2n = 0;
   int rqt_root = 1;

   d->intra_split = 0;
   d->intra_pred_mode[0] = d->intra_pred_mode[1] = 1;
   d->intra_pred_mode[2] = d->intra_pred_mode[3] = 1;
   d->cu_pred_intra = 1;
   d->part_mode = RH265_PART_2Nx2N;
   d->cu_x = x0;
   d->cu_y = y0;
   d->cu_log2 = log2_cb;

   if (is_pb)
   {
      int cur = rh265_zaddr(d, x0, y0);
      int inc = 0;
      if (rh265_avail(d, x0 - 1, y0, cur) &&
          d->skipm[(y0 >> 2) * d->w4 + ((x0 - 1) >> 2)])
         inc++;
      if (rh265_avail(d, x0, y0 - 1, cur) &&
          d->skipm[((y0 - 1) >> 2) * d->w4 + (x0 >> 2)])
         inc++;
      skip = rh265_cabac_decode(&d->cb, RH265_CTX_SKIP_FLAG + inc);
      for (i = 0; i < (cb_size >> 2); i++)
         for (j = 0; j < (cb_size >> 2); j++)
         {
            int y4 = (y0 >> 2) + i, x4 = (x0 >> 2) + j;
            if (y4 < d->h4 && x4 < d->w4)
               d->skipm[y4 * d->w4 + x4] = (uint8_t)skip;
         }
   }

   if (skip)
   {
      d->cu_pred_intra = 0;
      if (rh265_pred_unit(d, x0, y0, cb_size, cb_size, 0, 1) < 0)
         return -1;
      rh265_fill_nzc(d, x0, y0, log2_cb, 0);
      rh265_bs_edges(d, x0, y0, log2_cb);
      goto qp_tail;
   }

   if (is_pb)
      d->cu_pred_intra = rh265_cabac_decode(&d->cb,
            RH265_CTX_PRED_MODE_FLAG);

   if (!d->cu_pred_intra)
   {
      /* inter part_mode, full binarization incl. AMP */
      int pm = RH265_PART_2Nx2N;
      if (!rh265_cabac_decode(&d->cb, RH265_CTX_PART_MODE))
      {
         if (log2_cb == sps->log2_min_cb)
         {
            if (rh265_cabac_decode(&d->cb, RH265_CTX_PART_MODE + 1))
               pm = RH265_PART_2NxN;
            else if (log2_cb == 3)
               pm = RH265_PART_Nx2N;
            else if (rh265_cabac_decode(&d->cb, RH265_CTX_PART_MODE + 2))
               pm = RH265_PART_Nx2N;
            else
               pm = RH265_PART_NxN;
         }
         else if (!sps->amp_enabled)
            pm = rh265_cabac_decode(&d->cb, RH265_CTX_PART_MODE + 1)
                  ? RH265_PART_2NxN : RH265_PART_Nx2N;
         else if (rh265_cabac_decode(&d->cb, RH265_CTX_PART_MODE + 1))
         {
            if (rh265_cabac_decode(&d->cb, RH265_CTX_PART_MODE + 3))
               pm = RH265_PART_2NxN;
            else
               pm = rh265_cabac_bypass(&d->cb)
                     ? RH265_PART_2NxnD : RH265_PART_2NxnU;
         }
         else
         {
            if (rh265_cabac_decode(&d->cb, RH265_CTX_PART_MODE + 3))
               pm = RH265_PART_Nx2N;
            else
               pm = rh265_cabac_bypass(&d->cb)
                     ? RH265_PART_nRx2N : RH265_PART_nLx2N;
         }
      }
      d->part_mode = pm;
      for (i = 0; i < rh265_part_count[pm]; i++)
      {
         const uint8_t *g = rh265_part_geom[pm][i];
         int px = x0 + ((cb_size * g[0]) >> 2);
         int py = y0 + ((cb_size * g[1]) >> 2);
         int pw = (cb_size * g[2]) >> 2;
         int ph = (cb_size * g[3]) >> 2;
         {
            int mrg = rh265_pred_unit(d, px, py, pw, ph, i, 0);
            if (mrg < 0)
               return -1;
            if (i == 0 && pm == RH265_PART_2Nx2N)
               merge_2nx2n = mrg;
         }
      }
      goto inter_residual;
   }

   if (log2_cb == sps->log2_min_cb)
   {
      /* part_mode: one context-coded bin for intra */
      part_nxn = !rh265_cabac_decode(&d->cb, RH265_CTX_PART_MODE);
      d->intra_split = part_nxn;
      d->part_mode = part_nxn ? RH265_PART_NxN : RH265_PART_2Nx2N;
   }

   side = part_nxn ? 2 : 1;
   pb_size = cb_size >> part_nxn;

   for (i = 0; i < side; i++)
      for (j = 0; j < side; j++)
         prev_flag[2 * i + j] = (uint8_t)rh265_cabac_decode(&d->cb,
               RH265_CTX_PREV_INTRA_LUMA_PRED_FLAG);
   for (i = 0; i < side; i++)
      for (j = 0; j < side; j++)
      {
         if (prev_flag[2 * i + j])
         {
            /* mpm_idx: TR cMax=2, bypass */
            int v = rh265_cabac_bypass(&d->cb);
            if (v)
               v += rh265_cabac_bypass(&d->cb);
            mpm_or_rem[2 * i + j] = v;
         }
         else
            mpm_or_rem[2 * i + j] = (int)rh265_cabac_bypass_bits(&d->cb, 5);
      }
   for (i = 0; i < side; i++)
      for (j = 0; j < side; j++)
         d->intra_pred_mode[2 * i + j] = rh265_luma_intra_mode(d,
               x0 + pb_size * j, y0 + pb_size * i, pb_size,
               prev_flag[2 * i + j], mpm_or_rem[2 * i + j]);

   {
      int chroma_mode;
      if (!rh265_cabac_decode(&d->cb, RH265_CTX_INTRA_CHROMA_PRED_MODE))
         d->intra_pred_mode_c = d->intra_pred_mode[0];   /* DM */
      else
      {
         chroma_mode = (int)rh265_cabac_bypass_bits(&d->cb, 2);
         if (d->intra_pred_mode[0] == chroma_tab[chroma_mode])
            d->intra_pred_mode_c = 34;
         else
            d->intra_pred_mode_c = chroma_tab[chroma_mode];
      }
   }

   d->pu_log2 = log2_cb;
   d->pu_x0 = x0;
   d->pu_y0 = y0;

inter_residual:
   if (!d->cu_pred_intra)
   {
      /* rqt_root_cbf, inferred 1 except after a non-merge parse or a
       * partitioned CU (7.3.8.5) */
      if (!(d->part_mode == RH265_PART_2Nx2N && merge_2nx2n))
         rqt_root = rh265_cabac_decode(&d->cb,
               RH265_CTX_NO_RESIDUAL_DATA_FLAG);
      if (!rqt_root)
      {
         rh265_fill_nzc(d, x0, y0, log2_cb, 0);
         rh265_bs_edges(d, x0, y0, log2_cb);
         goto qp_tail;
      }
   }

   if (rh265_transform_tree(d, x0, y0, x0, y0, x0, y0,
         log2_cb, log2_cb, 0, 0, 0, 0) < 0)
      return -1;

qp_tail:
   /* 8.6.1: QP bookkeeping for CUs with no coded delta, the per-8x8 QP
    * map (used by chroma derivation of later CUs and by deblocking), and
    * the previous-QG predictor update */
   if (d->pps->cu_qp_delta_enabled && !d->is_cu_qp_delta_coded)
      rh265_set_qpy(d, x0, y0);
   {
      int x8 = x0 >> 3, y8 = y0 >> 3;
      int n8 = cb_size >> 3;
      for (i = 0; i < n8; i++)
         for (j = 0; j < n8; j++)
            if (y8 + i < d->h8 && x8 + j < d->w8)
               d->qpy[(y8 + i) * d->w8 + x8 + j] = (int8_t)d->qp_y;
   }
   {
      int qg_mask = (1 << (sps->log2_ctb
            - d->pps->diff_cu_qp_delta_depth)) - 1;
      if (((x0 + cb_size) & qg_mask) == 0 && ((y0 + cb_size) & qg_mask) == 0)
         d->qp_y_pred = d->qp_y;
   }
   return 0;
}

static int rh265_coding_quadtree(rh265_dec *d, int x0, int y0,
      int log2_cb, int depth)
{
   const rh265_sps *sps = d->sps;
   const rh265_pps *pps = d->pps;
   int cb_size = 1 << log2_cb;
   int split;
   int i, j;

   if (x0 + cb_size <= sps->width && y0 + cb_size <= sps->height &&
       log2_cb > sps->log2_min_cb)
   {
      int cur = rh265_zaddr(d, x0, y0);
      int inc = 0;
      if (rh265_avail(d, x0 - 1, y0, cur) &&
          d->ctd[(y0 >> 2) * d->w4 + ((x0 - 1) >> 2)] > depth)
         inc++;
      if (rh265_avail(d, x0, y0 - 1, cur) &&
          d->ctd[((y0 - 1) >> 2) * d->w4 + (x0 >> 2)] > depth)
         inc++;
      split = rh265_cabac_decode(&d->cb,
            RH265_CTX_SPLIT_CODING_UNIT_FLAG + inc);
   }
   else
      split = (log2_cb > sps->log2_min_cb);

   if (pps->cu_qp_delta_enabled &&
       log2_cb >= sps->log2_ctb - pps->diff_cu_qp_delta_depth)
   {
      d->is_cu_qp_delta_coded = 0;
      d->cu_qp_delta = 0;
   }

   if (split)
   {
      int half = cb_size >> 1;
      int x1 = x0 + half, y1 = y0 + half;
      if (rh265_coding_quadtree(d, x0, y0, log2_cb - 1, depth + 1) < 0)
         return -1;
      if (x1 < sps->width)
         if (rh265_coding_quadtree(d, x1, y0, log2_cb - 1, depth + 1) < 0)
            return -1;
      if (y1 < sps->height)
         if (rh265_coding_quadtree(d, x0, y1, log2_cb - 1, depth + 1) < 0)
            return -1;
      if (x1 < sps->width && y1 < sps->height)
         if (rh265_coding_quadtree(d, x1, y1, log2_cb - 1, depth + 1) < 0)
            return -1;
      {
         int qg_mask = (1 << (sps->log2_ctb
               - pps->diff_cu_qp_delta_depth)) - 1;
         if (((x0 + cb_size) & qg_mask) == 0 &&
             ((y0 + cb_size) & qg_mask) == 0)
            d->qp_y_pred = d->qp_y;
      }
   }
   else
   {
      /* record the coding-tree depth for split_cu_flag contexts */
      for (i = 0; i < (cb_size >> 2); i++)
         for (j = 0; j < (cb_size >> 2); j++)
         {
            int y4 = (y0 >> 2) + i, x4 = (x0 >> 2) + j;
            if (y4 < d->h4 && x4 < d->w4)
               d->ctd[y4 * d->w4 + x4] = (uint8_t)depth;
         }
      d->ct_depth_cur = depth;
      if (rh265_coding_unit(d, x0, y0, log2_cb) < 0)
         return -1;
   }
   return 0;
}

/* ==================== rh265_deblock.h ==================== */

static const uint8_t rh265_betatable[52]={
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 7, 8,
    9,10,11,12,13,14,15,16,17,18,20,22,24,26,28,30,32,34,36,
   38,40,42,44,46,48,50,52,54,56,58,60,62,64};
static const uint8_t rh265_tctable[54]={
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
   1,1,1,1,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,
   5,5,6,6,7,8,9,10,11,13,14,16,18,20,22,24};

/* filter one 4-line luma edge segment (8.7.2.5.7); pix points at q0 of
 * the first line, xs steps across the edge, ys along it */
/* ==================== rh265_sao.h ==================== */

static const int8_t rh265_sao_eo_dx[4]={-1, 0,-1, 1};
static const int8_t rh265_sao_eo_dy[4]={ 0,-1,-1,-1};

/* Apply SAO for one component of one CTB, reading the deblocked picture
 * from src (a copy) and writing into the frame plane (8.7.3). */
/* ==================== bit-depth instantiations ==================== */

#define RH265_BD  8
#define RH265_PEL uint8_t
#define RH265_FN(name) rh265_ ## name ## _8
#include "rh265_bd.inc"
#undef RH265_BD
#undef RH265_PEL
#undef RH265_FN

#define RH265_BD  10
#define RH265_PEL uint16_t
#define RH265_FN(name) rh265_ ## name ## _10
#include "rh265_bd.inc"
#undef RH265_BD
#undef RH265_PEL
#undef RH265_FN

static const rh265_bd_fns rh265_bd8_fns =
{
   rh265_intra_pred_8, rh265_add_residual_8, rh265_mc_pu_8,
   rh265_deblock_frame_8, rh265_sao_frame_8
};
static const rh265_bd_fns rh265_bd10_fns =
{
   rh265_intra_pred_10, rh265_add_residual_10, rh265_mc_pu_10,
   rh265_deblock_frame_10, rh265_sao_frame_10
};

static const rh265_bd_fns *rh265_get_fns(int bd)
{
   return bd > 8 ? &rh265_bd10_fns : &rh265_bd8_fns;
}

/* sao syntax for one CTB (7.3.8.3) */
static void rh265_sao_param(rh265_dec *d, int rx, int ry)
{
   rh265_cabac *cb = &d->cb;
   rh265_sao_params *sao = &d->sao[ry * d->sps->ctb_w + rx];
   int merge_left = 0, merge_up = 0;
   int c_idx, i;

   memset(sao, 0, sizeof(*sao));
   if (!d->sh.sao_luma && !d->sh.sao_chroma)
      return;

   /* CTB neighbours are always in-slice here (single slice, no tiles) */
   if (rx > 0 && rh265_avail(d, (rx << d->sps->log2_ctb) - 1,
         ry << d->sps->log2_ctb, d->cur_zaddr))
      merge_left = rh265_cabac_decode(cb, RH265_CTX_SAO_MERGE_FLAG);
   if (ry > 0 && !merge_left && rh265_avail(d, rx << d->sps->log2_ctb,
         (ry << d->sps->log2_ctb) - 1, d->cur_zaddr))
      merge_up = rh265_cabac_decode(cb, RH265_CTX_SAO_MERGE_FLAG);
   if (merge_left)
   {
      memcpy(sao, &d->sao[ry * d->sps->ctb_w + rx - 1], sizeof(*sao));
      return;
   }
   if (merge_up)
   {
      memcpy(sao, &d->sao[(ry - 1) * d->sps->ctb_w + rx], sizeof(*sao));
      return;
   }

   for (c_idx = 0; c_idx < 3; c_idx++)
   {
      int enabled = c_idx ? d->sh.sao_chroma : d->sh.sao_luma;
      int abs_off[4];
      if (!enabled)
         continue;
      if (c_idx == 2)
      {
         sao->type_idx[2] = sao->type_idx[1];
         sao->eo_class[2] = sao->eo_class[1];
      }
      else
      {
         if (!rh265_cabac_decode(cb, RH265_CTX_SAO_TYPE_IDX))
            sao->type_idx[c_idx] = 0;
         else
            sao->type_idx[c_idx] = rh265_cabac_bypass(cb) ? 2 : 1;
      }
      if (!sao->type_idx[c_idx])
         continue;
      {
         /* sao_offset_abs is truncated-Rice with
          * cMax = (1 << (Min(bitDepth, 10) - 5)) - 1: 7 at 8-bit but
          * 31 at 10-bit, so the bound is bitstream syntax, not just a
          * value range */
         int cmax = (1 << (d->bd - 5)) - 1;
         for (i = 0; i < 4; i++)
         {
            int v = 0;
            while (v < cmax && rh265_cabac_bypass(cb))
               v++;
            abs_off[i] = v;
         }
      }
      if (sao->type_idx[c_idx] == 1)
      {
         for (i = 0; i < 4; i++)
         {
            int sign = 0;
            if (abs_off[i])
               sign = rh265_cabac_bypass(cb);
            sao->off[c_idx][i] = (int8_t)(sign ? -abs_off[i] : abs_off[i]);
         }
         sao->band_pos[c_idx] = (uint8_t)rh265_cabac_bypass_bits(cb, 5);
      }
      else
      {
         if (c_idx != 2)
            sao->eo_class[c_idx] = (uint8_t)rh265_cabac_bypass_bits(cb, 2);
         for (i = 0; i < 4; i++)
            sao->off[c_idx][i] = (int8_t)((i > 1) ? -abs_off[i] : abs_off[i]);
      }
   }
}

/* ==================== rh265_video.h ==================== */

struct rh265_video
{
   rh265_sps sps[RH265_MAX_SPS];
   rh265_pps pps[RH265_MAX_PPS];
   rh265_dec d;
   int alloc_w, alloc_h;      /* dimensions the frame buffers were sized for */
   int alloc_bd;
   int poc;
   int prev_poc_tid0;         /* prevTid0Pic POC for 8.3.1 */
   int length_size;           /* NAL length prefix size from hvcC, else 0 */
   int saw_annexb;

   rh265_pic dpb[RH265_MAX_DPB];
   int cur_slot;              /* DPB slot of the picture being decoded */
   int out_fifo[RH265_MAX_DPB];
   int out_count;
   int out_pic;               /* slot whose planes the API exposes, -1 */
   int first_pic_decoded;     /* a NoRaslOutputFlag anchor was seen */
   int rasl_skip;             /* drop RASL pictures after that anchor */

   /* RefPicSetStCurrBefore/After of the current picture (DPB slots) */
   int st_bef[RH265_MAX_REFS], st_aft[RH265_MAX_REFS];
   int nb_st_bef, nb_st_aft;

   /* Parameter-set and slice-header parse scratch.  rh265_sps alone is
    * ~11 KiB (the SPS-stored RPS list), far over the 8 KiB thread
    * stacks of some console targets, so these cannot be locals. */
   rh265_sps  sps_tmp;
   rh265_pps  pps_tmp;
   rh265_shdr sh_tmp;
};

static void rh265_free_frame(rh265_video *v)
{
   rh265_dec *d = &v->d;
   int i, p;
   for (p = 0; p < RH265_MAX_DPB; p++)
   {
      for (i = 0; i < 3; i++)
      {
         free(v->dpb[p].pl[i]);
         v->dpb[p].pl[i] = NULL;
      }
      free(v->dpb[p].mvf);
      v->dpb[p].mvf = NULL;
      v->dpb[p].in_use = 0;
   }
   for (i = 0; i < 3; i++)
      d->pl[i] = NULL;
   d->mvf = NULL;
   d->cur = NULL;
   d->col_ref = NULL;
   v->out_pic = -1;
   v->out_count = 0;
   free(d->ipm);   d->ipm = NULL;
   free(d->ctd);   d->ctd = NULL;
   free(d->vedge); d->vedge = NULL;
   free(d->hedge); d->hedge = NULL;
   free(d->nzc);   d->nzc = NULL;
   free(d->skipm); d->skipm = NULL;
   free(d->qpy);   d->qpy = NULL;
   free(d->sao);   d->sao = NULL;
   free(d->ctb_slice);     d->ctb_slice = NULL;
   free(d->ctb_lf_across); d->ctb_lf_across = NULL;
}

static int rh265_alloc_frame(rh265_video *v, const rh265_sps *s)
{
   rh265_dec *d = &v->d;
   d->bd        = s->bit_depth_luma;
   d->pel_bytes = 1 + (d->bd > 8);
   d->fns       = rh265_get_fns(d->bd);
   if (v->alloc_w == s->width && v->alloc_h == s->height
         && v->alloc_bd == d->bd && d->ipm)
      return 0;
   rh265_free_frame(v);
   v->alloc_bd = d->bd;
   d->pw[0] = s->width;      d->ph[0] = s->height;
   d->pw[1] = s->width >> 1; d->ph[1] = s->height >> 1;
   d->pw[2] = d->pw[1];      d->ph[2] = d->ph[1];
   d->strd[0] = (d->pw[0] + 15) & ~15;
   d->strd[1] = (d->pw[1] + 15) & ~15;
   d->strd[2] = d->strd[1];
   d->w4 = (s->width + 3) >> 2;
   d->h4 = (s->height + 3) >> 2;
   d->w8 = (s->width + 7) >> 3;
   d->h8 = (s->height + 7) >> 3;
   d->ipm   = (uint8_t*)malloc((size_t)d->w4 * d->h4);
   d->ctd   = (uint8_t*)malloc((size_t)d->w4 * d->h4);
   d->vedge = (uint8_t*)malloc((size_t)d->w4 * d->h4);
   d->hedge = (uint8_t*)malloc((size_t)d->w4 * d->h4);
   d->nzc   = (uint8_t*)malloc((size_t)d->w4 * d->h4);
   d->skipm = (uint8_t*)malloc((size_t)d->w4 * d->h4);
   d->qpy   = (int8_t*)malloc((size_t)d->w8 * d->h8);
   d->sao   = (rh265_sao_params*)malloc(
         (size_t)s->pic_size_ctbs * sizeof(rh265_sao_params));
   d->ctb_slice     = (uint16_t*)malloc(
         (size_t)s->pic_size_ctbs * sizeof(uint16_t));
   d->ctb_lf_across = (uint8_t*)malloc((size_t)s->pic_size_ctbs);
   if (!d->ipm || !d->ctd || !d->vedge || !d->hedge || !d->nzc ||
       !d->skipm || !d->qpy || !d->sao || !d->ctb_slice ||
       !d->ctb_lf_across)
      goto fail;
   v->alloc_w = s->width;
   v->alloc_h = s->height;
   v->out_pic = -1;
   return 0;
fail:
   rh265_free_frame(v);
   return -1;
}

/* Allocate (or reuse) planes and the motion field of one DPB slot. */
static int rh265_pic_alloc(rh265_video *v, int slot)
{
   rh265_dec *d = &v->d;
   rh265_pic *p = &v->dpb[slot];
   int i;
   for (i = 0; i < 3; i++)
      if (!p->pl[i])
      {
         size_t n = (size_t)d->strd[i] * d->ph[i];
         p->pl[i] = (uint8_t*)malloc(n * d->pel_bytes);
         if (!p->pl[i])
            return -1;
         if (!i)
            memset(p->pl[i], 0, n * d->pel_bytes);
         else if (d->bd == 8)
            memset(p->pl[i], 128, n);
         else
         {
            uint16_t *q = (uint16_t*)p->pl[i];
            size_t k;
            for (k = 0; k < n; k++)
               q[k] = (uint16_t)(1 << (d->bd - 1));
         }
      }
   if (!p->mvf)
   {
      p->mvf = (rh265_mvfield*)malloc(
            (size_t)d->w4 * d->h4 * sizeof(rh265_mvfield));
      if (!p->mvf)
         return -1;
   }
   return 0;
}

/* Free every slot that no longer serves any purpose. */
static void rh265_dpb_prune(rh265_video *v)
{
   int p, i;
   for (p = 0; p < RH265_MAX_DPB; p++)
   {
      rh265_pic *pic = &v->dpb[p];
      if (!pic->in_use || pic->is_ref || pic->needed_out ||
          p == v->out_pic || p == v->cur_slot)
         continue;
      for (i = 0; i < v->out_count; i++)
         if (v->out_fifo[i] == p)
            break;
      if (i < v->out_count)
         continue;
      pic->in_use = 0;
   }
}

/* Emit pictures whose output can no longer be affected: while more than
 * sps_max_num_reorder_pics pictures wait, the smallest POC leaves. */
static void rh265_dpb_bump(rh265_video *v, int reorder)
{
   for (;;)
   {
      int p, waiting = 0, best = -1;
      for (p = 0; p < RH265_MAX_DPB; p++)
         if (v->dpb[p].in_use && v->dpb[p].needed_out)
         {
            waiting++;
            if (best < 0 || v->dpb[p].poc < v->dpb[best].poc)
               best = p;
         }
      if (waiting <= reorder || best < 0 ||
          v->out_count >= RH265_MAX_DPB)
         break;
      v->dpb[best].needed_out = 0;
      v->out_fifo[v->out_count++] = best;
   }
   rh265_dpb_prune(v);
}

/* Decode the slice_segment_data of one I slice.  rbsp/size cover the
 * whole slice NAL payload (unescaped); data_bit is the first bit after
 * the slice header's byte alignment. */
static int rh265_decode_slice_data(rh265_video *v, const uint8_t *rbsp,
      size_t size, size_t data_bit, const uint32_t *esc_pos, int esc_count)
{
   rh265_dec *d = &v->d;
   const rh265_sps *sps = d->sps;
   int ctb_addr = d->sh.slice_segment_address;
   int end_of_slice = 0;
   int wpp = d->pps->entropy_coding_sync_enabled;
   int substream = 0;
   int have_save = 0;
   /* the escaped-domain position of the current unescaped slice-data
    * byte: entry-point offsets count escaped bytes, so each jump adds
    * the coded offset here and translates back by subtracting the
    * escapes before that point */
   size_t esc_base;
   int    esc_idx = 0;

   if ((data_bit & 7) || data_bit / 8 > size)
      return -1;

   /* escaped position of the first slice-data byte */
   esc_base = data_bit / 8;
   while (esc_idx < esc_count && esc_pos[esc_idx] <= esc_base)
   {
      esc_base++;                 /* removed byte sat at or before here */
      esc_idx++;
   }

   rh265_cabac_init_engine(&d->cb, rbsp + data_bit / 8, rbsp + size);
   {
      int init_type = 0;
      if (d->sh.slice_type == RH265_SLICE_P)
         init_type = d->sh.cabac_init ? 2 : 1;
      else if (d->sh.slice_type == RH265_SLICE_B)
         init_type = d->sh.cabac_init ? 1 : 2;
      rh265_cabac_init_contexts(&d->cb, d->sh.slice_qp, init_type);
   }

   d->sl = NULL;
   if (sps->scaling_list_enabled)
      d->sl = d->pps->has_scaling ? &d->pps->sl : &sps->sl;

   d->qp_y = d->sh.slice_qp;
   d->qp_y_pred = d->sh.slice_qp;
   d->first_qg = 1;
   d->cu_qp_delta = 0;
   d->is_cu_qp_delta_coded = 0;
   d->slice_start_ctb = ctb_addr;
   d->slice_start_zaddr = rh265_zaddr(d,
         (ctb_addr % sps->ctb_w) << sps->log2_ctb,
         (ctb_addr / sps->ctb_w) << sps->log2_ctb);

   while (ctb_addr < sps->pic_size_ctbs)
   {
      int rx = ctb_addr % sps->ctb_w;
      int ry = ctb_addr / sps->ctb_w;
      int x0 = rx << sps->log2_ctb;
      int y0 = ry << sps->log2_ctb;

      if (wpp && rx == 0 && ctb_addr != d->slice_start_ctb)
      {
         /* CTB-row start (9.3.1): the substream begins at the next
          * entry point, byte-positioned in the unescaped buffer by
          * removing the escapes the coded offset counted */
         size_t un;
         if (substream >= d->sh.num_entry_points)
            return -1;             /* fewer entry points than rows */
         esc_base += d->sh.entry_point[substream];
         substream++;
         while (esc_idx < esc_count && esc_pos[esc_idx] < esc_base)
            esc_idx++;
         un = esc_base - (size_t)esc_idx;
         if (un > size)
            return -1;
         rh265_cabac_init_engine(&d->cb, rbsp + un, rbsp + size);
         /* contexts continue from the stored state after the second
          * CTB of the row above when that CTB lies in this slice;
          * otherwise the row initialises like a slice (9.3.2.2) */
         if (have_save &&
             ctb_addr - sps->ctb_w + 1 >= d->slice_start_ctb)
         {
            memcpy(d->cb.state, d->wpp_state, sizeof(d->cb.state));
            memcpy(d->cb.mps,   d->wpp_mps,   sizeof(d->cb.mps));
         }
         else
         {
            int init_type = 0;
            if (d->sh.slice_type == RH265_SLICE_P)
               init_type = d->sh.cabac_init ? 2 : 1;
            else if (d->sh.slice_type == RH265_SLICE_B)
               init_type = d->sh.cabac_init ? 1 : 2;
            rh265_cabac_init_contexts(&d->cb, d->sh.slice_qp, init_type);
         }
         /* qPY_PREV resets to SliceQpY at the first quantisation group
          * of a WPP CTB row (8.6.1) */
         d->qp_y_pred = d->sh.slice_qp;
         d->first_qg  = 1;
      }

      d->ctb_slice[ctb_addr]     = (uint16_t)d->slice_seq;
      d->ctb_lf_across[ctb_addr] =
            (uint8_t)d->sh.loop_filter_across_slices;
      d->cur_zaddr = rh265_zaddr(d, x0, y0);
      if (sps->sao_enabled)
         rh265_sao_param(d, rx, ry);
      if (rh265_coding_quadtree(d, x0, y0, sps->log2_ctb, 0) < 0)
         return -1;
      if (wpp && rx == 1)
      {
         /* storage process (9.3.2.3): contexts after the second CTB of
          * a row seed the row below */
         memcpy(d->wpp_state, d->cb.state, sizeof(d->cb.state));
         memcpy(d->wpp_mps,   d->cb.mps,   sizeof(d->cb.mps));
         have_save = 1;
      }
      ctb_addr++;
      end_of_slice = rh265_cabac_terminate(&d->cb);
      if (end_of_slice)
         break;
      if (wpp && rx == sps->ctb_w - 1)
      {
         /* end_of_subset_one_bit closes the row's substream; the next
          * row re-anchors at its entry point above */
         if (!rh265_cabac_terminate(&d->cb))
            return -1;
      }
   }
   /* the slice must end exactly with the last CTB of its coverage; a
    * terminate bin that fires early or fails to fire at the picture end
    * means the syntax decode ran off the rails somewhere */
   if (ctb_addr >= sps->pic_size_ctbs && !end_of_slice)
      return -1;
   return ctb_addr;
}

/* 8.3.1 picture order count.  An IRAP with NoRaslOutputFlag equal to 1
 * (IDR or BLA always, CRA when it starts the decoded sequence) resets the
 * MSB; everything else predicts it from prevTid0Pic. */
static void rh265_compute_poc(rh265_video *v, const rh265_sps *sps,
      int nal_type, int poc_lsb, int tid)
{
   int max_lsb = 1 << sps->log2_max_poc_lsb;
   int prev_lsb, prev_msb, msb;
   if (RH265_IS_IDR(nal_type))
      v->poc = 0;
   else if (nal_type >= RH265_NAL_BLA_W_LP && nal_type <= RH265_NAL_BLA_N_LP)
      v->poc = poc_lsb;
   else if (nal_type == RH265_NAL_CRA && !v->first_pic_decoded)
      v->poc = poc_lsb;
   else
   {
      prev_lsb = v->prev_poc_tid0 & (max_lsb - 1);
      prev_msb = v->prev_poc_tid0 - prev_lsb;
      if (poc_lsb < prev_lsb && prev_lsb - poc_lsb >= max_lsb / 2)
         msb = prev_msb + max_lsb;
      else if (poc_lsb > prev_lsb && poc_lsb - prev_lsb > max_lsb / 2)
         msb = prev_msb - max_lsb;
      else
         msb = prev_msb;
      v->poc = msb + poc_lsb;
   }
   /* prevTid0Pic: TemporalId 0 and neither RASL, RADL nor sub-layer
    * non-reference (the _N types are the even values below 16) */
   if (tid == 0 &&
       !(nal_type >= RH265_NAL_RADL_N && nal_type <= RH265_NAL_RASL_R) &&
       !(nal_type < 16 && !(nal_type & 1)))
      v->prev_poc_tid0 = v->poc;
}

/* 8.3.2: resolve the slice short-term RPS against the DPB.  Pictures the
 * RPS does not mention stop being references; the ones it marks as used
 * by the current picture form StCurrBefore/StCurrAfter.  A used entry
 * that is not in the DPB is a broken reference: fail. */
static int rh265_apply_rps(rh265_video *v)
{
   const rh265_st_rps *rps = &v->d.sh.rps;
   uint8_t keep[RH265_MAX_DPB];
   int i, p;
   memset(keep, 0, sizeof(keep));
   v->nb_st_bef = v->nb_st_aft = 0;
   for (i = 0; i < rps->num_negative + rps->num_positive; i++)
   {
      int neg  = i < rps->num_negative;
      int poc  = v->poc + (int)(neg ? rps->delta_poc_s0[i]
                                    : rps->delta_poc_s1[i - rps->num_negative]);
      int used = neg ? rps->used_s0[i]
                     : rps->used_s1[i - rps->num_negative];
      int slot = -1;
      for (p = 0; p < RH265_MAX_DPB; p++)
         if (v->dpb[p].in_use && v->dpb[p].is_ref &&
             p != v->cur_slot && v->dpb[p].poc == poc)
         {
            slot = p;
            break;
         }
      if (slot < 0)
      {
         if (used)
            return -1;                  /* missing active reference */
         continue;
      }
      keep[slot] = 1;
      if (used)
      {
         if (neg)
            v->st_bef[v->nb_st_bef++] = slot;
         else
            v->st_aft[v->nb_st_aft++] = slot;
      }
   }
   for (p = 0; p < RH265_MAX_DPB; p++)
      if (p != v->cur_slot && !keep[p])
         v->dpb[p].is_ref = 0;
   rh265_dpb_prune(v);
   return 0;
}

/* 8.3.4: build RefPicList0/1 for the current slice from the picture's
 * StCurrBefore/StCurrAfter sets. */
static int rh265_build_ref_lists(rh265_video *v)
{
   rh265_dec *d = &v->d;
   int total = v->nb_st_bef + v->nb_st_aft;
   int l, i;
   d->nb_refs[0] = d->nb_refs[1] = 0;
   d->col_ref = NULL;
   d->max_merge  = d->sh.max_merge;
   d->slice_tmvp = d->sh.slice_tmvp;
   if (d->sh.slice_type == RH265_SLICE_I)
      return 0;
   if (total == 0)
      return -1;
   for (l = 0; l < (d->sh.slice_type == RH265_SLICE_B ? 2 : 1); l++)
   {
      int tmp[RH265_MAX_REFS * 3];
      int nb_tmp = 0;
      while (nb_tmp < d->sh.nb_refs[l] && nb_tmp < RH265_MAX_REFS * 3 - total)
      {
         const int *first = l ? v->st_aft : v->st_bef;
         const int *second = l ? v->st_bef : v->st_aft;
         int nb_first = l ? v->nb_st_aft : v->nb_st_bef;
         int nb_second = l ? v->nb_st_bef : v->nb_st_aft;
         for (i = 0; i < nb_first; i++)
            tmp[nb_tmp++] = first[i];
         for (i = 0; i < nb_second; i++)
            tmp[nb_tmp++] = second[i];
      }
      for (i = 0; i < d->sh.nb_refs[l]; i++)
      {
         int idx = d->sh.rpl_mod[l] ? d->sh.list_entry[l][i] : i;
         if (idx >= nb_tmp)
            return -1;
         d->ref_list[l][i] = &v->dpb[tmp[idx]];
         d->ref_poc[l][i]  = v->dpb[tmp[idx]].poc;
      }
      d->nb_refs[l] = d->sh.nb_refs[l];
   }
   if (d->sh.slice_tmvp)
   {
      int cl = d->sh.collocated_from_l0 ? 0 : 1;
      if (d->sh.collocated_ref_idx >= d->nb_refs[cl])
         return -1;
      d->col_ref = d->ref_list[cl][d->sh.collocated_ref_idx];
   }
   return 0;
}

/* Handle one NAL unit.  Returns 1 when a picture completed, 0 otherwise,
 * negative on error. */
static int rh265_handle_nal(rh265_video *v, const uint8_t *nal, size_t len)
{
   int nal_type, ret = 0;
   uint8_t *rbsp;
   size_t rbsp_size;
   uint32_t *esc_pos = NULL;
   int esc_count = 0;
   if (len < 3)
      return 0;
   if (nal[0] & 0x80)
      return -1;                        /* forbidden_zero_bit */
   nal_type = (nal[0] >> 1) & 0x3f;
   if ((nal[1] & 7) == 0)
      return -1;                        /* nuh_temporal_id_plus1 == 0 */

   if (nal_type == RH265_NAL_VPS || nal_type == RH265_NAL_AUD ||
       nal_type == RH265_NAL_SEI_PREFIX || nal_type == RH265_NAL_SEI_SUFFIX ||
       nal_type == RH265_NAL_EOS || nal_type == RH265_NAL_EOB ||
       nal_type == RH265_NAL_FD)
      return 0;                         /* nothing this decoder needs */
   if (!RH265_IS_SLICE(nal_type) &&
       nal_type != RH265_NAL_SPS && nal_type != RH265_NAL_PPS)
      return 0;                         /* reserved/unknown: skip */

   {
      /* slices need the escape positions to translate WPP entry-point
       * offsets; parameter sets never do */
      if (RH265_IS_SLICE(nal_type))
         rbsp = rh265_unescape_ex(nal + 2, len - 2, &rbsp_size,
               &esc_pos, &esc_count);
      else
         rbsp = rh265_unescape(nal + 2, len - 2, &rbsp_size);
   }
   if (!rbsp)
      return -1;

   if (nal_type == RH265_NAL_SPS)
   {
      rh265_sps *sp = &v->sps_tmp;
      int id;
      ret = rh265_parse_sps(rbsp, rbsp_size, sp, &id);
      if (ret == 0)
      {
         if (sp->chroma_format_idc != 1 ||
             (sp->bit_depth_luma != 8 && sp->bit_depth_luma != 10) ||
             sp->bit_depth_chroma != sp->bit_depth_luma ||
             sp->pcm_enabled)
            ret = -2;                   /* out of the supported profile */
         else
            memcpy(&v->sps[id], sp, sizeof(*sp));
      }
   }
   else if (nal_type == RH265_NAL_PPS)
   {
      rh265_pps *pp = &v->pps_tmp;
      int id;
      ret = rh265_parse_pps(rbsp, rbsp_size, pp, &id);
      if (ret == 0)
      {
         if (pp->transquant_bypass_enabled ||
             pp->constrained_intra_pred)
            ret = -2;
         else
            memcpy(&v->pps[id], pp, sizeof(*pp));
      }
   }
   else
   {
      /* slice segment */
      rh265_bits b;
      rh265_shdr *shp = &v->sh_tmp;
      rh265_bits_init(&b, rbsp, rbsp_size);
      ret = rh265_parse_slice_header(&b, nal_type, NULL, NULL,
            v->pps, v->sps, shp);
      if (ret == 0)
      {
         const rh265_pps *pps = &v->pps[shp->pps_id];
         const rh265_sps *sps = &v->sps[pps->sps_id];
         int tid = (nal[1] & 7) - 1;
         int is_rasl = nal_type == RH265_NAL_RASL_N ||
                       nal_type == RH265_NAL_RASL_R;
         int is_radl = nal_type == RH265_NAL_RADL_N ||
                       nal_type == RH265_NAL_RADL_R;
         v->d.sps = sps;
         v->d.pps = pps;
         memcpy(&v->d.sh, shp, sizeof(*shp));
         if (rh265_alloc_frame(v, sps) < 0)
            ret = -1;
         else if (is_rasl && v->rasl_skip)
            ret = 0;                    /* undecodable leading picture */
         else
         {
            if (!is_rasl && !is_radl && !RH265_IS_IRAP(nal_type))
               v->rasl_skip = 0;
            if (shp->first_slice_in_pic)
            {
               int slot, p;
               v->d.slice_seq = 0;
               rh265_compute_poc(v, sps, nal_type, shp->poc_lsb, tid);
               if (RH265_IS_IRAP(nal_type) &&
                   (nal_type != RH265_NAL_CRA || !v->first_pic_decoded))
               {
                  /* NoRaslOutputFlag: the previous sequence ends here.
                   * Flush its pending outputs (their POCs are not
                   * comparable across the discontinuity), drop its
                   * references, and skip any RASL that follows. */
                  rh265_dpb_bump(v, 0);
                  for (p = 0; p < RH265_MAX_DPB; p++)
                     if (p != v->out_pic)
                        v->dpb[p].is_ref = 0;
                  v->rasl_skip = 1;
               }
               v->first_pic_decoded = 1;
               slot = -1;
               for (p = 0; p < RH265_MAX_DPB; p++)
                  if (!v->dpb[p].in_use && p != v->out_pic)
                  {
                     slot = p;
                     break;
                  }
               if (slot < 0 || rh265_pic_alloc(v, slot) < 0)
                  ret = -1;
               else
               {
                  rh265_pic *cur = &v->dpb[slot];
                  int i;
                  v->cur_slot = slot;
                  v->d.cur = cur;
                  for (i = 0; i < 3; i++)
                     v->d.pl[i] = cur->pl[i];
                  v->d.mvf = cur->mvf;
                  memset(cur->mvf, 0,
                        (size_t)v->d.w4 * v->d.h4 * sizeof(rh265_mvfield));
                  memset(v->d.ipm, 1, (size_t)v->d.w4 * v->d.h4);
                  memset(v->d.ctd, 0, (size_t)v->d.w4 * v->d.h4);
                  memset(v->d.vedge, 0, (size_t)v->d.w4 * v->d.h4);
                  memset(v->d.hedge, 0, (size_t)v->d.w4 * v->d.h4);
                  memset(v->d.nzc, 0, (size_t)v->d.w4 * v->d.h4);
                  memset(v->d.skipm, 0, (size_t)v->d.w4 * v->d.h4);
                  memset(v->d.qpy, (uint8_t)shp->slice_qp,
                        (size_t)v->d.w8 * v->d.h8);
                  memset(v->d.sao, 0,
                        (size_t)sps->pic_size_ctbs * sizeof(rh265_sao_params));
                  cur->poc        = v->poc;
                  cur->is_ref     = 1;
                  cur->needed_out = shp->pic_output_flag;
                  cur->in_use     = 1;
                  if (!RH265_IS_IDR(nal_type))
                     ret = rh265_apply_rps(v);
                  else
                  {
                     v->nb_st_bef = v->nb_st_aft = 0;
                     for (p = 0; p < RH265_MAX_DPB; p++)
                        if (p != v->cur_slot)
                           v->dpb[p].is_ref = 0;
                     rh265_dpb_prune(v);
                  }
               }
            }
            if (ret == 0 && v->cur_slot < 0)
               ret = -1;                /* first slice was lost */
            if (ret == 0)
               ret = rh265_build_ref_lists(v);
            if (ret == 0)
            {
               if (!shp->first_slice_in_pic)
                  v->d.slice_seq++;
               ret = rh265_decode_slice_data(v, rbsp, rbsp_size,
                     b.bitpos, esc_pos, esc_count);
               if (ret >= 0)
               {
                  if (ret >= sps->pic_size_ctbs)
                  {
                     /* picture complete: run the loop filters */
                     v->d.fns->deblock_frame(&v->d);
                     if (sps->sao_enabled)
                        if (v->d.fns->sao_frame(&v->d) < 0)
                        {
                           free(esc_pos);
                           free(rbsp);
                           return -1;
                        }
                     v->cur_slot = -1;
                     rh265_dpb_bump(v, sps->max_num_reorder_pics);
                  }
                  ret = 0;
               }
            }
         }
      }
   }
   free(esc_pos);
   free(rbsp);
   return ret;
}

/* ==================== API glue ==================== */

rh265_video *rh265_video_open(void)
{
   rh265_video *v = (rh265_video*)calloc(1, sizeof(*v));
   if (v)
   {
      v->cur_slot = -1;
      v->out_pic  = -1;
   }
   return v;
}

void rh265_video_close(rh265_video *v)
{
   if (!v)
      return;
   rh265_free_frame(v);
   free(v);
}

/* hvcC: HEVCDecoderConfigurationRecord (ISO/IEC 14496-15 8.3.3.1) */
int rh265_video_set_extradata(rh265_video *v, const uint8_t *hvcc, size_t len)
{
   size_t pos;
   int num_arrays, i;
   if (!v || !hvcc)
      return -1;
   if (len >= 3 && (hvcc[0] || hvcc[1]) && hvcc[2] == 1)
   {
      /* Annex-B extradata: feed it through the NAL splitter */
      return rh265_video_decode(v, hvcc, len) < 0 ? -1 : 0;
   }
   if (len < 23)
      return -1;
   v->length_size = (hvcc[21] & 3) + 1;
   num_arrays = hvcc[22];
   pos = 23;
   for (i = 0; i < num_arrays; i++)
   {
      int j, num_nals;
      if (pos + 3 > len)
         return -1;
      num_nals = (hvcc[pos + 1] << 8) | hvcc[pos + 2];
      pos += 3;
      for (j = 0; j < num_nals; j++)
      {
         size_t nal_len;
         if (pos + 2 > len)
            return -1;
         nal_len = ((size_t)hvcc[pos] << 8) | hvcc[pos + 1];
         pos += 2;
         if (pos + nal_len > len)
            return -1;
         if (rh265_handle_nal(v, hvcc + pos, nal_len) < -1)
            return -1;
         pos += nal_len;
      }
   }
   return 0;
}

/* Hand the oldest queued picture to the caller.  Returns 1 when one was
 * available. */
static int rh265_pop_output(rh265_video *v)
{
   int i;
   if (v->out_count == 0)
      return 0;
   v->out_pic = v->out_fifo[0];
   for (i = 1; i < v->out_count; i++)
      v->out_fifo[i - 1] = v->out_fifo[i];
   v->out_count--;
   return 1;
}

int rh265_video_decode(rh265_video *v, const uint8_t *data, size_t len)
{
   size_t pos = 0;
   int got = 0, ret;
   int annexb;
   if (!v || !data || len < 4)
      return -1;

   /* hvcC extradata fixes the framing as length-prefixed; a 4-byte
    * length in 256..511 is byte-identical to a start code, so once the
    * length size is known the sniff below must not run.  Without
    * extradata, detect Annex-B by a leading start code. */
   if (v->length_size)
      annexb = 0;
   else
      annexb = (data[0] == 0 && data[1] == 0 &&
               (data[2] == 1 || (data[2] == 0 && len > 3 && data[3] == 1)));
   if (annexb)
   {
      size_t nal_start = 0, i;
      int have = 0;
      i = 0;
      while (i + 2 < len)
      {
         if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1)
         {
            if (have)
            {
               size_t end = i;
               if (end > nal_start && data[end - 1] == 0)
                  end--;                /* 4-byte start code of the next NAL */
               ret = rh265_handle_nal(v, data + nal_start, end - nal_start);
               if (ret < 0)
                  return ret;
            }
            nal_start = i + 3;
            have = 1;
            i += 3;
         }
         else
            i++;
      }
      if (have && nal_start < len)
      {
         ret = rh265_handle_nal(v, data + nal_start, len - nal_start);
         if (ret < 0)
            return ret;
         got |= (ret == 1);
      }
   }
   else
   {
      int lsz = v->length_size ? v->length_size : 4;
      while (pos + (size_t)lsz <= len)
      {
         size_t nal_len = 0;
         int i;
         for (i = 0; i < lsz; i++)
            nal_len = (nal_len << 8) | data[pos + i];
         pos += lsz;
         if (nal_len == 0 || pos + nal_len > len)
            return -1;
         ret = rh265_handle_nal(v, data + pos, nal_len);
         if (ret < 0)
            return ret;
         pos += nal_len;
      }
   }
   (void)got;
   return rh265_pop_output(v);
}

int rh265_video_drain(rh265_video *v)
{
   if (!v)
      return -1;
   /* everything still waiting can now leave in POC order */
   rh265_dpb_bump(v, 0);
   return rh265_pop_output(v) ? 0 : -1;
}

const uint8_t *rh265_video_plane(const rh265_video *v, int plane,
      int *stride, int *width, int *height)
{
   const rh265_sps *s;
   int shift;
   const rh265_pic *pic;
   if (!v || v->out_pic < 0 || plane < 0 || plane > 2)
      return NULL;
   pic = &v->dpb[v->out_pic];
   if (!pic->pl[plane])
      return NULL;
   s = v->d.sps;
   shift = plane ? 1 : 0;
   /* conformance-window offsets are coded in chroma units (SubWidthC =
    * SubHeightC = 2 for 4:2:0); the luma crop is twice the coded value */
   if (stride) *stride = v->d.strd[plane];
   if (width)  *width  = (s->width  - 2 * (s->crop_left + s->crop_right))
         >> shift;
   if (height) *height = (s->height - 2 * (s->crop_top + s->crop_bottom))
         >> shift;
   /* the returned pointer is bytes; at 10 bits the caller casts to
    * uint16_t and indexes with the sample stride, as with the VP9
    * high-bit-depth planes */
   return pic->pl[plane]
         + (((s->crop_top << 1) >> shift) * v->d.strd[plane]
            + ((s->crop_left << 1) >> shift)) * v->d.pel_bytes;
}

int rh265_video_bit_depth(const rh265_video *v)
{
   return (v && v->d.bd) ? v->d.bd : 8;
}
