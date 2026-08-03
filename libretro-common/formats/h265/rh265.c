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
 * (whole-picture vertical then horizontal, 8.7.2); sample-adaptive
 * offset, band and edge, classified on the fully deblocked picture
 * (8.7.3); multiple independent slice segments per picture; POC
 * derivation (8.3.1); conformance-window cropping; and both Annex-B
 * (start-code) and length-prefixed HVCC input, with the NAL length
 * size taken from the hvcC extradata when present (default 4).
 *
 * What it does not implement: P and B slices (the next milestone; the
 * short-term RPS syntax is already parsed), 4:2:2, 4:4:4 and
 * monochrome, bit depths above 8, tiles, wavefront parallel processing
 * (entropy_coding_sync), dependent slice segments, explicit scaling
 * lists, PCM, transquant bypass, constrained intra prediction, and
 * encoding.  Out-of-scope streams are refused at the parameter-set or
 * slice level rather than decoded wrongly.
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

/* Strip emulation-prevention bytes (00 00 03 -> 00 00). */
static uint8_t *rh265_unescape(const uint8_t *nal, size_t len, size_t *out_size)
{
   uint8_t *rbsp = (uint8_t*)malloc(len ? len : 1);
   size_t i, j = 0;
   int zeros = 0;
   if (!rbsp) return NULL;
   for (i = 0; i < len; i++)
   {
      if (zeros >= 2 && nal[i] == 3)
      {
         zeros = 0;
         continue;      /* emulation prevention byte */
      }
      if (nal[i] == 0) zeros++; else zeros = 0;
      rbsp[j++] = nal[i];
   }
   *out_size = j;
   return rbsp;
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
} rh265_pps;

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

/* scaling_list_data: parsed for its size only; SPS/PPS with
 * scaling_list_enabled are refused before this data is ever used. */
static void rh265_skip_scaling_list_data(rh265_bits *b)
{
   int size_id, matrix_id;
   for (size_id = 0; size_id < 4; size_id++)
      for (matrix_id = 0; matrix_id < 6; matrix_id += (size_id == 3) ? 3 : 1)
      {
         if (!rh265_u1(b))               /* scaling_list_pred_mode_flag */
            rh265_ue(b);                 /* scaling_list_pred_matrix_id_delta */
         else
         {
            int coef_num = rh265_min(64, 1 << (4 + (size_id << 1)));
            int i;
            if (size_id > 1)
               rh265_se(b);              /* scaling_list_dc_coef_minus8 */
            for (i = 0; i < coef_num; i++)
               rh265_se(b);              /* scaling_list_delta_coef */
         }
      }
}

/* st_ref_pic_set (7.3.7), resolving inter-RPS prediction against the
 * previously parsed sets so every set is stored in explicit form. */
static int rh265_parse_st_rps(rh265_bits *b, rh265_sps *s, int idx,
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
            int ri = ref->num_negative - 1 - k;
            if (d < 0 && use[ri])
            { if (n0 >= RH265_MAX_REFS) return -1;
              s0[n0] = d; u0[n0] = (uint8_t)used_by[ri]; n0++; }
         }
         /* positive half */
         for (k = ref->num_negative - 1; k >= 0; k--)
         {
            int32_t d = ref->delta_poc_s0[k] + delta_rps;
            int ri = ref->num_negative - 1 - k;
            if (d > 0 && use[ri])
            { if (n1 >= RH265_MAX_REFS) return -1;
              s1[n1] = d; u1[n1] = (uint8_t)used_by[ri]; n1++; }
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
   {
      if (rh265_u1(&b))                  /* sps_scaling_list_data_present */
         rh265_skip_scaling_list_data(&b);
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
   if (rh265_u1(&b))                    /* pps_scaling_list_data_present */
      rh265_skip_scaling_list_data(&b);
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
} rh265_shdr;

/* Parse one slice segment header for an I slice stream.  Returns 0 on
 * success, -1 on malformed data, -2 on valid-but-unsupported. */
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
         rh265_sps tmp;
         /* parse an explicit set; sets already in the SPS may be referenced
          * by inter-set prediction, so give the parser the SPS context with
          * the explicit set appended at index num_st_rps */
         memcpy(&tmp, s, sizeof(tmp));
         if (rh265_parse_st_rps(b, &tmp, s->num_st_rps, &sh->rps, 1) < 0)
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
         return -2;                     /* long-term refs: next milestone */
      if (s->temporal_mvp_enabled)
         rh265_u1(b);                   /* slice_temporal_mvp_enabled_flag */
   }
   if (s->sao_enabled)
   {
      sh->sao_luma   = (int)rh265_u1(b);
      sh->sao_chroma = (int)rh265_u1(b);
   }
   if (sh->slice_type != RH265_SLICE_I)
      return -2;                        /* P/B slices: next milestone */
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
   /* entry_point_offsets absent: tiles and WPP are refused */
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

typedef struct
{
   const rh265_sps *sps;
   const rh265_pps *pps;
   rh265_shdr sh;

   uint8_t *pl[3];            /* Y, U, V planes */
   int strd[3];
   int pw[3], ph[3];

   /* per-4x4 (luma coords >> 2) metadata */
   uint8_t *ipm;              /* luma intra prediction mode */
   uint8_t *ctd;              /* coding-tree depth */
   uint8_t *vedge, *hedge;    /* deblocking edge flags (TU/PU boundary) */
   int w4, h4;

   int8_t *qpy;               /* QpY per 8x8 (luma coords >> 3) */
   int w8, h8;

   rh265_sao_params *sao;     /* per CTB */

   rh265_cabac cb;

   /* current-slice decode state */
   int slice_start_zaddr;
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
} rh265_dec;

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
static void rh265_intra_pred(rh265_dec *d, int x0, int y0, int log2_size,
      int c_idx, int mode)
{
   const rh265_sps *sps = d->sps;
   int size = 1 << log2_size;
   int shift = c_idx ? 1 : 0;
   int px = x0 >> shift, py = y0 >> shift;
   int stride = d->strd[c_idx];
   uint8_t *dst = d->pl[c_idx] + py * stride + px;
   int cur = d->cur_zaddr;
   /* linear reference array: ref[0] = p[-1][2N-1] .. ref[2N] = p[-1][-1]
    * .. ref[4N] = p[2N-1][-1] */
   uint8_t ref[4 * RH265_MAX_TB + 1];
   uint8_t avail[4 * RH265_MAX_TB + 1];
   uint8_t left[2 * RH265_MAX_TB + 1];
   uint8_t *top;
   int n4 = 4 * size;
   int i, x, y;
   int any = 0;

   for (i = 0; i <= n4; i++)
   {
      int sx, sy;
      if (i < 2 * size)      { sx = -1; sy = 2 * size - 1 - i; }
      else if (i == 2 * size){ sx = -1; sy = -1; }
      else                   { sx = i - 2 * size - 1; sy = -1; }
      /* sx/sy are -1 for the left/top ring, so scale by multiplication:
       * left-shifting a negative value is undefined behaviour. */
      avail[i] = (uint8_t)rh265_avail(d,
            x0 + sx * (1 << shift), y0 + sy * (1 << shift), cur);
      if (avail[i])
      {
         ref[i] = d->pl[c_idx][(py + sy) * stride + (px + sx)];
         any = 1;
      }
   }
   if (!any)
      memset(ref, 128, (size_t)(n4 + 1));
   else
   {
      /* 8.4.4.2.2 substitution: bottom-left up, then left to right */
      if (!avail[0])
      {
         for (i = 1; i <= n4 && !avail[i]; i++) ;
         ref[0] = ref[i];
      }
      for (i = 1; i <= n4; i++)
         if (!avail[i])
            ref[i] = ref[i - 1];
   }

   /* split into left[-1..2N-1] (top-down) and top[-1..2N-1] */
   left[0] = ref[2 * size];                  /* corner at left[-1] slot */
   for (i = 0; i < 2 * size; i++)
      left[1 + i] = ref[2 * size - 1 - i];
   top = &ref[2 * size];                     /* top[-1] = corner .. */

   /* 8.4.4.2.3 reference smoothing (luma only) */
   if (c_idx == 0 && mode != 1 && size > 4)
   {
      int min_dist = rh265_min(abs(mode - 26), abs(mode - 10));
      int thres = (size == 8) ? 7 : ((size == 16) ? 1 : 0);
      if (mode == 0 || min_dist > thres)
      {
         int strong = 0;
         if (sps->strong_intra_smoothing && size == 32)
         {
            int c  = top[0];
            if (abs(c + top[64] - 2 * top[32]) < 8 &&
                abs(c + left[64] - 2 * left[32]) < 8)
               strong = 1;
         }
         if (strong)
         {
            uint8_t tl = top[0], tr = top[64], bl = left[64];
            for (i = 0; i < 63; i++)
            {
               top[1 + i]  = (uint8_t)(((63 - i) * tl + (i + 1) * tr + 32) >> 6);
               left[1 + i] = (uint8_t)(((63 - i) * tl + (i + 1) * bl + 32) >> 6);
            }
         }
         else
         {
            uint8_t ftop[2 * RH265_MAX_TB + 1];
            uint8_t fleft[2 * RH265_MAX_TB + 1];
            ftop[0] = fleft[0] =
               (uint8_t)((left[1] + 2 * top[0] + top[1] + 2) >> 2);
            for (i = 1; i < 2 * size; i++)
            {
               ftop[i]  = (uint8_t)((top[i - 1] + 2 * top[i] + top[i + 1] + 2) >> 2);
               fleft[i] = (uint8_t)((left[i - 1] + 2 * left[i] + left[i + 1] + 2) >> 2);
            }
            ftop[2 * size]  = top[2 * size];
            fleft[2 * size] = left[2 * size];
            memcpy(top, ftop, (size_t)(2 * size + 1));
            memcpy(left, fleft, (size_t)(2 * size + 1));
         }
      }
   }

   if (mode == 0)
   {
      /* 8.4.4.2.4 planar */
      for (y = 0; y < size; y++)
         for (x = 0; x < size; x++)
            dst[y * stride + x] = (uint8_t)(
               ((size - 1 - x) * left[1 + y] + (x + 1) * top[1 + size] +
                (size - 1 - y) * top[1 + x] + (y + 1) * left[1 + size] +
                size) >> (log2_size + 1));
   }
   else if (mode == 1)
   {
      /* 8.4.4.2.5 DC */
      int dc = size;
      for (i = 0; i < size; i++)
         dc += top[1 + i] + left[1 + i];
      dc >>= (log2_size + 1);
      for (y = 0; y < size; y++)
         memset(dst + y * stride, dc, (size_t)size);
      if (c_idx == 0 && size < 32)
      {
         dst[0] = (uint8_t)((left[1] + 2 * dc + top[1] + 2) >> 2);
         for (x = 1; x < size; x++)
            dst[x] = (uint8_t)((top[1 + x] + 3 * dc + 2) >> 2);
         for (y = 1; y < size; y++)
            dst[y * stride] = (uint8_t)((left[1 + y] + 3 * dc + 2) >> 2);
      }
   }
   else
   {
      /* 8.4.4.2.6 angular; rr[] carries the spec's ref[] array, where
       * ref[0] is the corner and ref[x] = p[x-1][-1] (vertical modes) or
       * p[-1][x-1] (horizontal modes); the local top[]/left[] arrays are
       * laid out the same way, so the main reference copies verbatim */
      int angle = rh265_pred_angle[mode];
      int last = (size * angle) >> 5;
      uint8_t mref[4 * RH265_MAX_TB + 2];
      uint8_t *rr = mref + RH265_MAX_TB;      /* rr[-N..2N] valid */
      if (mode >= 18)
      {
         memcpy(rr, top, (size_t)(2 * size + 1));
         if (angle < 0 && last < -1)
         {
            int inv = rh265_inv_angle[mode];
            for (i = last; i <= -1; i++)
               rr[i] = left[((i * inv + 128) >> 8)];
         }
         for (y = 0; y < size; y++)
         {
            int idx  = ((y + 1) * angle) >> 5;
            int fact = ((y + 1) * angle) & 31;
            if (fact)
               for (x = 0; x < size; x++)
                  dst[y * stride + x] = (uint8_t)(
                        ((32 - fact) * rr[x + idx + 1] +
                         fact * rr[x + idx + 2] + 16) >> 5);
            else
               for (x = 0; x < size; x++)
                  dst[y * stride + x] = rr[x + idx + 1];
         }
         if (mode == 26 && c_idx == 0 && size < 32)
            for (y = 0; y < size; y++)
               dst[y * stride] = rh265_clip8(top[1] +
                     ((left[1 + y] - top[0]) >> 1));
      }
      else
      {
         memcpy(rr, left, (size_t)(2 * size + 1));
         if (angle < 0 && last < -1)
         {
            int inv = rh265_inv_angle[mode];
            for (i = last; i <= -1; i++)
               rr[i] = top[((i * inv + 128) >> 8)];
         }
         for (x = 0; x < size; x++)
         {
            int idx  = ((x + 1) * angle) >> 5;
            int fact = ((x + 1) * angle) & 31;
            if (fact)
               for (y = 0; y < size; y++)
                  dst[y * stride + x] = (uint8_t)(
                        ((32 - fact) * rr[y + idx + 1] +
                         fact * rr[y + idx + 2] + 16) >> 5);
            else
               for (y = 0; y < size; y++)
                  dst[y * stride + x] = rr[y + idx + 1];
         }
         if (mode == 10 && c_idx == 0 && size < 32)
            for (x = 0; x < size; x++)
               dst[x] = rh265_clip8(left[1] + ((top[1 + x] - top[0]) >> 1));
      }
   }
}

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

static void rh265_idst4(int16_t *coeffs)
{
   int i;
   for (i = 0; i < 4; i++)
      rh265_idst4_pass(coeffs + i, coeffs + i, 4, 4, 7);
   for (i = 0; i < 4; i++)
      rh265_idst4_pass(coeffs + 4 * i, coeffs + 4 * i, 1, 1, 12);
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

static void rh265_idct(int16_t *coeffs, int log2_size)
{
   int size = 1 << log2_size;
   int i;
   for (i = 0; i < size; i++)
      rh265_idct_pass(coeffs + i, coeffs + i, size, size, size, 7);
   for (i = 0; i < size; i++)
      rh265_idct_pass(coeffs + size * i, coeffs + size * i, 1, 1, size, 12);
}

/* transform_skip rescale (FFmpeg FUNC(dequant)): shift = 15 - bd - log2 */
static void rh265_tskip_rescale(int16_t *coeffs, int log2_size)
{
   int shift = 15 - 8 - log2_size;
   int n = 1 << (2 * log2_size);
   int i, off = 1 << (shift - 1);
   for (i = 0; i < n; i++)
      coeffs[i] = (int16_t)((coeffs[i] + off) >> shift);
}

static void rh265_add_residual(uint8_t *dst, int stride,
      const int16_t *coeffs, int size)
{
   int x, y;
   for (y = 0; y < size; y++)
   {
      for (x = 0; x < size; x++)
         dst[x] = rh265_clip8(dst[x] + coeffs[x]);
      dst += stride;
      coeffs += size;
   }
}

/* chroma QP mapping for 4:2:0 (Table 8-10) */
static const uint8_t rh265_qp_c[14]={29,30,31,32,33,33,34,34,35,35,36,36,37,37};
static const uint8_t rh265_level_scale[6]={40,45,51,57,64,72};

static int rh265_chroma_qp(int qp_y, int offset)
{
   int qp_i = rh265_clip3(0, 57, qp_y + offset);
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
   int16_t coeffs[32 * 32];
   int qp, shift, add, scale;
   int i;
   int shiftc = c_idx ? 1 : 0;
   uint8_t *dst = d->pl[c_idx] + (y0 >> shiftc) * d->strd[c_idx]
         + (x0 >> shiftc);

   memset(sig_cg, 0, sizeof(sig_cg));
   memset(coeffs, 0, sizeof(int16_t) << (2 * log2_size));

   if (c_idx == 0)
      qp = d->qp_y;
   else
   {
      int off = (c_idx == 1)
            ? pps->cb_qp_offset + d->sh.cb_qp_offset
            : pps->cr_qp_offset + d->sh.cr_qp_offset;
      qp = rh265_chroma_qp(d->qp_y, off);
   }
   shift = 8 + log2_size - 5;
   add   = 1 << (shift - 1);
   scale = rh265_level_scale[qp % 6] << (qp / 6);

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
               int64_t t = ((int64_t)level * scale * 16 + add) >> shift;
               coeffs[y_c * trafo_size + x_c] = (int16_t)
                     rh265_clip3(-32768, 32767, (int)t);
            }
         }
      }
   }

   if (transform_skip_flag)
      rh265_tskip_rescale(coeffs, log2_size);
   else if (c_idx == 0 && log2_size == 2 && intra_mode >= 0)
      rh265_idst4(coeffs);
   else
      rh265_idct(coeffs, log2_size);

   rh265_add_residual(dst, d->strd[c_idx], coeffs, trafo_size);
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

static void rh265_mark_tu_edges(rh265_dec *d, int x0, int y0, int log2_size)
{
   int size4 = 1 << (log2_size - 2);
   int x4 = x0 >> 2, y4 = y0 >> 2;
   int i;
   for (i = 0; i < size4; i++)
   {
      if (y4 + i < d->h4)
         d->vedge[(y4 + i) * d->w4 + x4] = 1;
      if (x4 + i < d->w4)
         d->hedge[y4 * d->w4 + x4 + i] = 1;
   }
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
    * reconstructed neighbours are in place for the next TB */
   d->cur_zaddr = rh265_zaddr(d, x0, y0);
   rh265_intra_pred(d, x0, y0, log2_size, 0, luma_mode);

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

      if (log2_size < 4)
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
      d->cur_zaddr = rh265_zaddr(d, x0, y0);
      rh265_intra_pred(d, x0, y0, log2_size - 1, 1, chroma_mode);
      if (cbf_cb)
         if (rh265_residual_coding(d, x0, y0, log2_size - 1, scan_idx_c, 1,
               chroma_mode) < 0)
            return -1;
      d->cur_zaddr = rh265_zaddr(d, x0, y0);
      rh265_intra_pred(d, x0, y0, log2_size - 1, 2, chroma_mode);
      if (cbf_cr)
         if (rh265_residual_coding(d, x0, y0, log2_size - 1, scan_idx_c, 2,
               chroma_mode) < 0)
            return -1;
   }
   else if (blk_idx == 3)
   {
      d->cur_zaddr = rh265_zaddr(d, xBase, yBase);
      rh265_intra_pred(d, xBase, yBase, log2_size, 1, chroma_mode);
      if (cbf_cb)
         if (rh265_residual_coding(d, xBase, yBase, log2_size, scan_idx_c, 1,
               chroma_mode) < 0)
            return -1;
      d->cur_zaddr = rh265_zaddr(d, xBase, yBase);
      rh265_intra_pred(d, xBase, yBase, log2_size, 2, chroma_mode);
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
   int max_depth = sps->max_transform_hierarchy_depth_intra
         + (d->intra_split ? 1 : 0);

   if (log2_size <= sps->log2_max_tb &&
       log2_size >  sps->log2_min_tb &&
       depth < max_depth &&
       !(d->intra_split && depth == 0))
      split = rh265_cabac_decode(&d->cb,
            RH265_CTX_SPLIT_TRANSFORM_FLAG + 5 - log2_size);
   else
      split = (log2_size > sps->log2_max_tb) ||
              (d->intra_split && depth == 0);

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
      int cbf_luma = rh265_cabac_decode(&d->cb,
            RH265_CTX_CBF_LUMA + (depth ? 0 : 1));
      rh265_mark_tu_edges(d, x0, y0, log2_size);
      if (rh265_transform_unit(d, x0, y0, xBase, yBase, cb_x, cb_y,
            log2_cb, log2_size, blk_idx, cbf_luma, cbf_cb, cbf_cr) < 0)
         return -1;
   }
   return 0;
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

   d->intra_split = 0;
   d->intra_pred_mode[0] = d->intra_pred_mode[1] = 1;
   d->intra_pred_mode[2] = d->intra_pred_mode[3] = 1;

   if (log2_cb == sps->log2_min_cb)
   {
      /* part_mode: one context-coded bin for intra */
      part_nxn = !rh265_cabac_decode(&d->cb, RH265_CTX_PART_MODE);
      d->intra_split = part_nxn;
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

   if (rh265_transform_tree(d, x0, y0, x0, y0, x0, y0,
         log2_cb, log2_cb, 0, 0, 0, 0) < 0)
      return -1;

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
      if (rh265_coding_unit(d, x0, y0, log2_cb) < 0)
         return -1;
      /* record the coding-tree depth for split_cu_flag contexts */
      for (i = 0; i < (cb_size >> 2); i++)
         for (j = 0; j < (cb_size >> 2); j++)
         {
            int y4 = (y0 >> 2) + i, x4 = (x0 >> 2) + j;
            if (y4 < d->h4 && x4 < d->w4)
               d->ctd[y4 * d->w4 + x4] = (uint8_t)depth;
         }
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
static void rh265_filter_luma_edge(uint8_t *pix, int xs, int ys,
      int beta, int tc)
{
   int dp0, dq0, dp3, dq3, d0, d3;
   uint8_t *l0 = pix, *l3 = pix + 3 * ys;
   int j;
   if (!tc)
      return;
   dp0 = abs(l0[-3*xs] - 2*l0[-2*xs] + l0[-1*xs]);
   dq0 = abs(l0[ 2*xs] - 2*l0[ 1*xs] + l0[ 0]);
   dp3 = abs(l3[-3*xs] - 2*l3[-2*xs] + l3[-1*xs]);
   dq3 = abs(l3[ 2*xs] - 2*l3[ 1*xs] + l3[ 0]);
   d0 = dp0 + dq0;
   d3 = dp3 + dq3;
   if (d0 + d3 >= beta)
      return;
   if (abs(l0[-4*xs]-l0[-1*xs]) + abs(l0[3*xs]-l0[0]) < (beta >> 3) &&
       abs(l0[-1*xs]-l0[0]) < ((tc * 5 + 1) >> 1) &&
       abs(l3[-4*xs]-l3[-1*xs]) + abs(l3[3*xs]-l3[0]) < (beta >> 3) &&
       abs(l3[-1*xs]-l3[0]) < ((tc * 5 + 1) >> 1) &&
       (d0 << 1) < (beta >> 2) && (d3 << 1) < (beta >> 2))
   {
      /* strong */
      int tc2 = tc << 1;
      for (j = 0; j < 4; j++)
      {
         uint8_t *e = pix + j * ys;
         int p3 = e[-4*xs], p2 = e[-3*xs], p1 = e[-2*xs], p0 = e[-1*xs];
         int q0 = e[0], q1 = e[1*xs], q2 = e[2*xs], q3 = e[3*xs];
         e[-1*xs] = (uint8_t)rh265_clip3(p0-tc2, p0+tc2,
               (p2 + 2*p1 + 2*p0 + 2*q0 + q1 + 4) >> 3);
         e[-2*xs] = (uint8_t)rh265_clip3(p1-tc2, p1+tc2,
               (p2 + p1 + p0 + q0 + 2) >> 2);
         e[-3*xs] = (uint8_t)rh265_clip3(p2-tc2, p2+tc2,
               (2*p3 + 3*p2 + p1 + p0 + q0 + 4) >> 3);
         e[0]     = (uint8_t)rh265_clip3(q0-tc2, q0+tc2,
               (q2 + 2*q1 + 2*q0 + 2*p0 + p1 + 4) >> 3);
         e[1*xs]  = (uint8_t)rh265_clip3(q1-tc2, q1+tc2,
               (q2 + q1 + q0 + p0 + 2) >> 2);
         e[2*xs]  = (uint8_t)rh265_clip3(q2-tc2, q2+tc2,
               (2*q3 + 3*q2 + q1 + q0 + p0 + 4) >> 3);
      }
   }
   else
   {
      /* weak */
      int dEp = (dp0 + dp3 < ((beta + (beta >> 1)) >> 3));
      int dEq = (dq0 + dq3 < ((beta + (beta >> 1)) >> 3));
      for (j = 0; j < 4; j++)
      {
         uint8_t *e = pix + j * ys;
         int p2 = e[-3*xs], p1 = e[-2*xs], p0 = e[-1*xs];
         int q0 = e[0], q1 = e[1*xs], q2 = e[2*xs];
         int delta = (9 * (q0 - p0) - 3 * (q1 - p1) + 8) >> 4;
         if (abs(delta) < tc * 10)
         {
            delta = rh265_clip3(-tc, tc, delta);
            e[-1*xs] = rh265_clip8(p0 + delta);
            e[0]     = rh265_clip8(q0 - delta);
            if (dEp)
            {
               int d = rh265_clip3(-(tc >> 1), tc >> 1,
                     (((p2 + p0 + 1) >> 1) - p1 + delta) >> 1);
               e[-2*xs] = rh265_clip8(p1 + d);
            }
            if (dEq)
            {
               int d = rh265_clip3(-(tc >> 1), tc >> 1,
                     (((q2 + q0 + 1) >> 1) - q1 - delta) >> 1);
               e[1*xs] = rh265_clip8(q1 + d);
            }
         }
      }
   }
}

/* filter one 4-line chroma edge segment (8.7.2.5.8) */
static void rh265_filter_chroma_edge(uint8_t *pix, int xs, int ys, int tc)
{
   int j;
   if (!tc)
      return;
   for (j = 0; j < 4; j++)
   {
      uint8_t *e = pix + j * ys;
      int p1 = e[-2*xs], p0 = e[-1*xs], q0 = e[0], q1 = e[1*xs];
      int delta = rh265_clip3(-tc, tc,
            (((q0 - p0) * 4 + p1 - q1 + 4) >> 3));
      e[-1*xs] = rh265_clip8(p0 + delta);
      e[0]     = rh265_clip8(q0 - delta);
   }
}

static int rh265_deblock_chroma_tc(rh265_dec *d, int qp_y, int c_idx)
{
   int off = (c_idx == 1) ? d->pps->cb_qp_offset : d->pps->cr_qp_offset;
   int qp = rh265_chroma_qp(qp_y, off);
   /* bS is always 2 for intra: intra tc offset 2*(bS-1) = 2 */
   return rh265_tctable[rh265_clip3(0, 53, qp + 2
         + d->sh.tc_offset_div2 * 2)];
}

/* Deblock the whole picture: all marked TU/PU edges on the 8x8 luma grid
 * carry bS 2 in an intra picture.  Vertical edges over the full picture
 * first, then horizontal edges (8.7.2.1). */
static void rh265_deblock_frame(rh265_dec *d)
{
   const rh265_sps *sps = d->sps;
   /* the div2 offsets are se(v) values in -6..6; scale by
    * multiplication, not by a shift of a possibly negative value */
   int beta_off = d->sh.beta_offset_div2 * 2;
   int tc_off   = d->sh.tc_offset_div2 * 2;
   int x, y;

   if (d->sh.deblocking_filter_disabled)
      return;

   /* vertical edges */
   for (x = 8; x < sps->width; x += 8)
      for (y = 0; y < sps->height; y += 4)
      {
         if (!d->vedge[(y >> 2) * d->w4 + (x >> 2)])
            continue;
         {
            int qp = (d->qpy[(y >> 3) * d->w8 + ((x - 1) >> 3)]
                    + d->qpy[(y >> 3) * d->w8 + (x >> 3)] + 1) >> 1;
            int beta = rh265_betatable[rh265_clip3(0, 51, qp + beta_off)];
            int tc = rh265_tctable[rh265_clip3(0, 53, qp + 2 + tc_off)];
            rh265_filter_luma_edge(d->pl[0] + y * d->strd[0] + x,
                  1, d->strd[0], beta, tc);
         }
         if ((x & 15) == 0 && (y & 7) == 0 && (y >> 1) + 4 <= d->ph[1])
         {
            int qp = (d->qpy[(y >> 3) * d->w8 + ((x - 1) >> 3)]
                    + d->qpy[(y >> 3) * d->w8 + (x >> 3)] + 1) >> 1;
            rh265_filter_chroma_edge(
                  d->pl[1] + (y >> 1) * d->strd[1] + (x >> 1),
                  1, d->strd[1], rh265_deblock_chroma_tc(d, qp, 1));
            rh265_filter_chroma_edge(
                  d->pl[2] + (y >> 1) * d->strd[2] + (x >> 1),
                  1, d->strd[2], rh265_deblock_chroma_tc(d, qp, 2));
         }
      }

   /* horizontal edges */
   for (y = 8; y < sps->height; y += 8)
      for (x = 0; x < sps->width; x += 4)
      {
         if (!d->hedge[(y >> 2) * d->w4 + (x >> 2)])
            continue;
         {
            int qp = (d->qpy[((y - 1) >> 3) * d->w8 + (x >> 3)]
                    + d->qpy[(y >> 3) * d->w8 + (x >> 3)] + 1) >> 1;
            int beta = rh265_betatable[rh265_clip3(0, 51, qp + beta_off)];
            int tc = rh265_tctable[rh265_clip3(0, 53, qp + 2 + tc_off)];
            rh265_filter_luma_edge(d->pl[0] + y * d->strd[0] + x,
                  d->strd[0], 1, beta, tc);
         }
         if ((y & 15) == 0 && (x & 7) == 0 && (x >> 1) + 4 <= d->pw[1])
         {
            int qp = (d->qpy[((y - 1) >> 3) * d->w8 + (x >> 3)]
                    + d->qpy[(y >> 3) * d->w8 + (x >> 3)] + 1) >> 1;
            rh265_filter_chroma_edge(
                  d->pl[1] + (y >> 1) * d->strd[1] + (x >> 1),
                  d->strd[1], 1, rh265_deblock_chroma_tc(d, qp, 1));
            rh265_filter_chroma_edge(
                  d->pl[2] + (y >> 1) * d->strd[2] + (x >> 1),
                  d->strd[2], 1, rh265_deblock_chroma_tc(d, qp, 2));
         }
      }
}

/* ==================== rh265_sao.h ==================== */

static const int8_t rh265_sao_eo_dx[4]={-1, 0,-1, 1};
static const int8_t rh265_sao_eo_dy[4]={ 0,-1,-1,-1};

/* Apply SAO for one component of one CTB, reading the deblocked picture
 * from src (a copy) and writing into the frame plane (8.7.3). */
static void rh265_sao_ctb(rh265_dec *d, const uint8_t *src, int src_stride,
      int c_idx, int rx, int ry)
{
   const rh265_sao_params *sao = &d->sao[ry * d->sps->ctb_w + rx];
   int shift = c_idx ? 1 : 0;
   int ctb = 1 << (d->sps->log2_ctb - shift);
   int x0 = rx << (d->sps->log2_ctb - shift);
   int y0 = ry << (d->sps->log2_ctb - shift);
   int w = rh265_min(ctb, d->pw[c_idx] - x0);
   int h = rh265_min(ctb, d->ph[c_idx] - y0);
   uint8_t *dst = d->pl[c_idx] + y0 * d->strd[c_idx] + x0;
   const uint8_t *s = src + y0 * src_stride + x0;
   int x, y;

   if (sao->type_idx[c_idx] == 1)
   {
      /* band offset */
      int8_t band[32];
      int k;
      memset(band, 0, sizeof(band));
      for (k = 0; k < 4; k++)
         band[(sao->band_pos[c_idx] + k) & 31] = sao->off[c_idx][k];
      for (y = 0; y < h; y++)
         for (x = 0; x < w; x++)
            dst[y * d->strd[c_idx] + x] = rh265_clip8(
                  s[y * src_stride + x] + band[s[y * src_stride + x] >> 3]);
   }
   else if (sao->type_idx[c_idx] == 2)
   {
      /* edge offset; samples whose neighbours leave the picture keep
       * their value (loop-filter-across-slices is moot with one slice) */
      int dx = rh265_sao_eo_dx[sao->eo_class[c_idx]];
      int dy = rh265_sao_eo_dy[sao->eo_class[c_idx]];
      for (y = 0; y < h; y++)
         for (x = 0; x < w; x++)
         {
            int px = x0 + x, py = y0 + y;
            int a, b, cval, edge;
            if (px + dx < 0 || px + dx >= d->pw[c_idx] ||
                py + dy < 0 || py + dy >= d->ph[c_idx] ||
                px - dx < 0 || px - dx >= d->pw[c_idx] ||
                py - dy < 0 || py - dy >= d->ph[c_idx])
               continue;
            cval = s[y * src_stride + x];
            a = s[(y + dy) * src_stride + (x + dx)];
            b = s[(y - dy) * src_stride + (x - dx)];
            edge = 2 + ((cval > a) - (cval < a)) + ((cval > b) - (cval < b));
            if (edge == 2)
               continue;
            if (edge < 2)
               edge++;             /* 0->1, 1->2 */
            /* offsets: index 1..4 -> off[0..3], 3 and 4 negated */
            {
               int v = sao->off[c_idx][edge - 1];
               dst[y * d->strd[c_idx] + x] = rh265_clip8(cval + v);
            }
         }
   }
}

static int rh265_sao_frame(rh265_dec *d)
{
   const rh265_sps *sps = d->sps;
   int c_idx, rx, ry;
   uint8_t *copy[3];
   int need = 0;
   for (ry = 0; ry < sps->ctb_h && !need; ry++)
      for (rx = 0; rx < sps->ctb_w && !need; rx++)
      {
         const rh265_sao_params *p = &d->sao[ry * sps->ctb_w + rx];
         if (p->type_idx[0] || p->type_idx[1] || p->type_idx[2])
            need = 1;
      }
   if (!need)
      return 0;
   for (c_idx = 0; c_idx < 3; c_idx++)
   {
      copy[c_idx] = (uint8_t*)malloc((size_t)d->strd[c_idx] * d->ph[c_idx]);
      if (!copy[c_idx])
      {
         while (c_idx--) free(copy[c_idx]);
         return -1;
      }
      memcpy(copy[c_idx], d->pl[c_idx], (size_t)d->strd[c_idx] * d->ph[c_idx]);
   }
   for (ry = 0; ry < sps->ctb_h; ry++)
      for (rx = 0; rx < sps->ctb_w; rx++)
         for (c_idx = 0; c_idx < 3; c_idx++)
            rh265_sao_ctb(d, copy[c_idx], d->strd[c_idx], c_idx, rx, ry);
   for (c_idx = 0; c_idx < 3; c_idx++)
      free(copy[c_idx]);
   return 0;
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
      for (i = 0; i < 4; i++)
      {
         int v = 0;
         while (v < 7 && rh265_cabac_bypass(cb))
            v++;
         abs_off[i] = v;
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
   int have_pic;
   int poc;
   int prev_poc_tid0;         /* prevTid0Pic POC for 8.3.1 */
   int length_size;           /* NAL length prefix size from hvcC, else 0 */
   int saw_annexb;
};

static void rh265_free_frame(rh265_dec *d)
{
   int i;
   for (i = 0; i < 3; i++)
   {
      free(d->pl[i]);
      d->pl[i] = NULL;
   }
   free(d->ipm);   d->ipm = NULL;
   free(d->ctd);   d->ctd = NULL;
   free(d->vedge); d->vedge = NULL;
   free(d->hedge); d->hedge = NULL;
   free(d->qpy);   d->qpy = NULL;
   free(d->sao);   d->sao = NULL;
}

static int rh265_alloc_frame(rh265_video *v, const rh265_sps *s)
{
   rh265_dec *d = &v->d;
   int i;
   if (v->alloc_w == s->width && v->alloc_h == s->height && d->pl[0])
      return 0;
   rh265_free_frame(d);
   d->pw[0] = s->width;      d->ph[0] = s->height;
   d->pw[1] = s->width >> 1; d->ph[1] = s->height >> 1;
   d->pw[2] = d->pw[1];      d->ph[2] = d->ph[1];
   for (i = 0; i < 3; i++)
   {
      d->strd[i] = (d->pw[i] + 15) & ~15;
      d->pl[i] = (uint8_t*)malloc((size_t)d->strd[i] * d->ph[i]);
      if (!d->pl[i])
         goto fail;
      memset(d->pl[i], i ? 128 : 0, (size_t)d->strd[i] * d->ph[i]);
   }
   d->w4 = (s->width + 3) >> 2;
   d->h4 = (s->height + 3) >> 2;
   d->w8 = (s->width + 7) >> 3;
   d->h8 = (s->height + 7) >> 3;
   d->ipm   = (uint8_t*)malloc((size_t)d->w4 * d->h4);
   d->ctd   = (uint8_t*)malloc((size_t)d->w4 * d->h4);
   d->vedge = (uint8_t*)malloc((size_t)d->w4 * d->h4);
   d->hedge = (uint8_t*)malloc((size_t)d->w4 * d->h4);
   d->qpy   = (int8_t*)malloc((size_t)d->w8 * d->h8);
   d->sao   = (rh265_sao_params*)malloc(
         (size_t)s->pic_size_ctbs * sizeof(rh265_sao_params));
   if (!d->ipm || !d->ctd || !d->vedge || !d->hedge || !d->qpy || !d->sao)
      goto fail;
   v->alloc_w = s->width;
   v->alloc_h = s->height;
   return 0;
fail:
   rh265_free_frame(d);
   return -1;
}

/* Decode the slice_segment_data of one I slice.  rbsp/size cover the
 * whole slice NAL payload (unescaped); data_bit is the first bit after
 * the slice header's byte alignment. */
static int rh265_decode_slice_data(rh265_video *v, const uint8_t *rbsp,
      size_t size, size_t data_bit)
{
   rh265_dec *d = &v->d;
   const rh265_sps *sps = d->sps;
   int ctb_addr = d->sh.slice_segment_address;
   int end_of_slice = 0;

   if ((data_bit & 7) || data_bit / 8 > size)
      return -1;

   rh265_cabac_init_engine(&d->cb, rbsp + data_bit / 8, rbsp + size);
   rh265_cabac_init_contexts(&d->cb, d->sh.slice_qp, 0);

   d->qp_y = d->sh.slice_qp;
   d->qp_y_pred = d->sh.slice_qp;
   d->first_qg = 1;
   d->cu_qp_delta = 0;
   d->is_cu_qp_delta_coded = 0;
   d->slice_start_zaddr = rh265_zaddr(d,
         (ctb_addr % sps->ctb_w) << sps->log2_ctb,
         (ctb_addr / sps->ctb_w) << sps->log2_ctb);

   while (ctb_addr < sps->pic_size_ctbs)
   {
      int rx = ctb_addr % sps->ctb_w;
      int ry = ctb_addr / sps->ctb_w;
      int x0 = rx << sps->log2_ctb;
      int y0 = ry << sps->log2_ctb;
      d->cur_zaddr = rh265_zaddr(d, x0, y0);
      if (sps->sao_enabled)
         rh265_sao_param(d, rx, ry);
      if (rh265_coding_quadtree(d, x0, y0, sps->log2_ctb, 0) < 0)
         return -1;
      ctb_addr++;
      end_of_slice = rh265_cabac_terminate(&d->cb);
      if (end_of_slice)
         break;
   }
   /* the slice must end exactly with the last CTB of its coverage; a
    * terminate bin that fires early or fails to fire at the picture end
    * means the syntax decode ran off the rails somewhere */
   if (ctb_addr >= sps->pic_size_ctbs && !end_of_slice)
      return -1;
   return ctb_addr;
}

/* 8.3.1 picture order count (needed for correct output once inter
 * pictures land; computed and tracked already) */
static void rh265_compute_poc(rh265_video *v, int nal_type, int poc_lsb)
{
   const rh265_sps *sps = v->d.sps;
   int max_lsb = 1 << sps->log2_max_poc_lsb;
   int prev_lsb, prev_msb, msb;
   if (RH265_IS_IDR(nal_type))
   {
      v->poc = 0;
   }
   else if (RH265_IS_IRAP(nal_type))
   {
      /* BLA/CRA as the first picture in decode order: POC = lsb */
      v->poc = poc_lsb;
   }
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
   v->prev_poc_tid0 = v->poc;
}

/* Handle one NAL unit.  Returns 1 when a picture completed, 0 otherwise,
 * negative on error. */
static int rh265_handle_nal(rh265_video *v, const uint8_t *nal, size_t len)
{
   int nal_type, ret = 0;
   uint8_t *rbsp;
   size_t rbsp_size;
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

   rbsp = rh265_unescape(nal + 2, len - 2, &rbsp_size);
   if (!rbsp)
      return -1;

   if (nal_type == RH265_NAL_SPS)
   {
      rh265_sps s;
      int id;
      ret = rh265_parse_sps(rbsp, rbsp_size, &s, &id);
      if (ret == 0)
      {
         if (s.chroma_format_idc != 1 ||
             s.bit_depth_luma != 8 || s.bit_depth_chroma != 8 ||
             s.pcm_enabled || s.scaling_list_enabled)
            ret = -2;                   /* out of the supported profile */
         else
            memcpy(&v->sps[id], &s, sizeof(s));
      }
   }
   else if (nal_type == RH265_NAL_PPS)
   {
      rh265_pps p;
      int id;
      ret = rh265_parse_pps(rbsp, rbsp_size, &p, &id);
      if (ret == 0)
      {
         if (p.entropy_coding_sync_enabled || p.transquant_bypass_enabled ||
             p.constrained_intra_pred)
            ret = -2;
         else
            memcpy(&v->pps[id], &p, sizeof(p));
      }
   }
   else
   {
      /* slice segment */
      rh265_bits b;
      rh265_shdr sh;
      rh265_bits_init(&b, rbsp, rbsp_size);
      ret = rh265_parse_slice_header(&b, nal_type, NULL, NULL,
            v->pps, v->sps, &sh);
      if (ret == 0)
      {
         const rh265_pps *pps = &v->pps[sh.pps_id];
         const rh265_sps *sps = &v->sps[pps->sps_id];
         v->d.sps = sps;
         v->d.pps = pps;
         memcpy(&v->d.sh, &sh, sizeof(sh));
         if (rh265_alloc_frame(v, sps) < 0)
            ret = -1;
         else
         {
            if (sh.first_slice_in_pic)
            {
               memset(v->d.ipm, 1, (size_t)v->d.w4 * v->d.h4);
               memset(v->d.ctd, 0, (size_t)v->d.w4 * v->d.h4);
               memset(v->d.vedge, 0, (size_t)v->d.w4 * v->d.h4);
               memset(v->d.hedge, 0, (size_t)v->d.w4 * v->d.h4);
               memset(v->d.qpy, (uint8_t)sh.slice_qp,
                     (size_t)v->d.w8 * v->d.h8);
               memset(v->d.sao, 0,
                     (size_t)sps->pic_size_ctbs * sizeof(rh265_sao_params));
               rh265_compute_poc(v, nal_type, sh.poc_lsb);
            }
            ret = rh265_decode_slice_data(v, rbsp, rbsp_size, b.bitpos);
            if (ret >= 0)
            {
               if (ret >= sps->pic_size_ctbs)
               {
                  /* picture complete: run the loop filters */
                  rh265_deblock_frame(&v->d);
                  if (sps->sao_enabled)
                     if (rh265_sao_frame(&v->d) < 0)
                     {
                        free(rbsp);
                        return -1;
                     }
                  v->have_pic = 1;
                  ret = 1;
               }
               else
                  ret = 0;              /* more slices of this picture follow */
            }
         }
      }
   }
   free(rbsp);
   return ret;
}

/* ==================== API glue ==================== */

rh265_video *rh265_video_open(void)
{
   rh265_video *v = (rh265_video*)calloc(1, sizeof(*v));
   return v;
}

void rh265_video_close(rh265_video *v)
{
   if (!v)
      return;
   rh265_free_frame(&v->d);
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

int rh265_video_decode(rh265_video *v, const uint8_t *data, size_t len)
{
   size_t pos = 0;
   int got = 0, ret;
   int annexb;
   if (!v || !data || len < 4)
      return -1;

   /* Annex-B if a start code leads; otherwise length-prefixed */
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
               got |= (ret == 1);
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
         got |= (ret == 1);
         pos += nal_len;
      }
   }
   return got;
}

int rh265_video_drain(rh265_video *v)
{
   /* intra pictures leave in decode order; nothing is ever held back */
   (void)v;
   return -1;
}

const uint8_t *rh265_video_plane(const rh265_video *v, int plane,
      int *stride, int *width, int *height)
{
   const rh265_sps *s;
   int shift;
   if (!v || !v->have_pic || plane < 0 || plane > 2 || !v->d.pl[plane])
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
   return v->d.pl[plane]
         + ((s->crop_top << 1) >> shift) * v->d.strd[plane]
         + ((s->crop_left << 1) >> shift);
}
