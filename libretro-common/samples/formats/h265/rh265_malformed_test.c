/* Parameter sets whose ue(v) codes run past what any field holds.
 *
 * An SPS is read for its values and then refused when its bit depth
 * is outside the supported profile (9 here), which set_extradata
 * reports as -1; an SPS refused as malformed leaves set_extradata at 0.
 * That tells, without looking inside the decoder, whether a crafted
 * SPS was taken as a parameter set at all:
 *  - an sps_id coded as 32 zero bits, or of 2^32 - 2, or a
 *    picture width of 2^25 - 1, is malformed. Read as an int those
 *    ids were negative, passed the id bound, and were stored at
 *    v->sps[id];
 *  - sps_max_latency_increase_plus1 at 2^32 - 2, which the spec
 *    allows and nothing reads, still parses.
 * Exit status is the outcome. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <formats/rh265.h>

static int failures;

/* An RBSP built a bit at a time */
typedef struct
{
   uint8_t buf[512];
   size_t  bits;
} bitw;

static void put1(bitw *w, unsigned bit)
{
   if (bit)
      w->buf[w->bits >> 3] |= (uint8_t)(0x80 >> (w->bits & 7));
   w->bits++;
}

static void putn(bitw *w, uint32_t v, int n)
{
   while (n--)
      put1(w, (v >> n) & 1);
}

/* ue(v) of @v; v up to 2^32 - 2 */
static void putue(bitw *w, uint32_t v)
{
   uint64_t x = (uint64_t)v + 1;
   int lz     = 0;
   while ((x >> (lz + 1)) != 0)
      lz++;
   putn(w, 0, lz);
   put1(w, 1);
   while (lz--)
      put1(w, (unsigned)((x >> lz) & 1));
}

/* 32 zero bits: a code no 32-bit value has. A reader that gives up
 * after them as 2^32 - 1 carries on at the next field. */
static void putue_overlong(bitw *w)
{
   putn(w, 0, 32);
}

static void trailing(bitw *w)
{
   put1(w, 1);
   while (w->bits & 7)
      put1(w, 0);
}

/* A NAL unit of @type around the RBSP, emulation-prevented */
static size_t nal(uint8_t *out, int type, const bitw *w)
{
   size_t i, n = 0, len = w->bits >> 3;
   int zeros = 0;
   out[n++] = (uint8_t)(type << 1);
   out[n++] = 1;
   for (i = 0; i < len; i++)
   {
      if (zeros >= 2 && w->buf[i] <= 3)
      {
         out[n++] = 3;
         zeros    = 0;
      }
      out[n++] = w->buf[i];
      zeros    = w->buf[i] ? 0 : zeros + 1;
   }
   return n;
}

/* hvcC extradata (as an MP4 carries it) holding the NAL units given,
 * each in an array of its own */
static size_t hvcc(uint8_t *out, const uint8_t *const *nals,
      const size_t *lens, int count)
{
   size_t n = 23;
   int i;
   memset(out, 0, 23);
   out[0]  = 1;                       /* configurationVersion */
   out[21] = 3;                       /* lengthSizeMinusOne */
   out[22] = (uint8_t)count;          /* numOfArrays */
   for (i = 0; i < count; i++)
   {
      out[n++] = (uint8_t)(nals[i][0] >> 1);
      out[n++] = 0; out[n++] = 1;     /* numNalus */
      out[n++] = (uint8_t)(lens[i] >> 8);
      out[n++] = (uint8_t)lens[i];
      memcpy(out + n, nals[i], lens[i]);
      n       += lens[i];
   }
   return n;
}

enum field { F_NONE = 0, F_ID_OVERLONG, F_ID_HUGE, F_WIDTH_HUGE,
   F_LATENCY_HUGE, F_SCALING, F_CROP, F_BAD_CROP, F_RPS, F_LT, F_LT_TABLE,
   F_SEQUENCE, F_INFERRED, F_BAD_LAYERS, F_DPB_HUGE, F_BAD_REORDER, F_RPS_PRED,
   F_VUI_SDR, F_VUI_HDR, F_VUI_RANGE, F_VUI_FIELD, F_VUI_SHORT };

/* An SPS for a 64x64 8-bit-or-@depth 4:2:0 picture, one field crafted */
static void sps(bitw *w, enum field f, int depth)
{
   int i, layers = f == F_SEQUENCE ? 2 : f == F_INFERRED ? 6 : f == F_BAD_LAYERS ? 7 : 0;
   memset(w, 0, sizeof(*w));
   putn(w, f == F_SEQUENCE ? 3 : 0, 4);
   putn(w, (unsigned)layers, 3);
   put1(w, 1);                       /* temporal_id_nesting */
   putn(w, f == F_SEQUENCE ? 0x62 : 0x01, 8);
   putn(w, 0x60000000u, 32);
   putn(w, f == F_SEQUENCE ? 0xb1234567u : 0, 32);
   putn(w, f == F_SEQUENCE ? 0x89ab : 0, 16);
   putn(w, 93, 8);                   /* level 3.1 */
   for (i = 0; i < layers; i++)
   {
      put1(w, i == 0); put1(w, i == 1);
   }
   if (layers)
      for (i = layers; i < 8; i++) putn(w, 0, 2);
   for (i = 0; i < layers; i++)
   {
      if (i == 0)
      {
         putn(w, 1, 8); putn(w, 0, 32);
         putn(w, 0, 32); putn(w, 0, 16);
      }
      if (i == 1) putn(w, 60, 8);
   }
   if (f == F_ID_OVERLONG)
      putue_overlong(w);
   else
      putue(w, f == F_ID_HUGE ? 0xfffffffeu : 0);   /* sps_id */
   putue(w, 1);                      /* chroma_format_idc 4:2:0 */
   putue(w, f == F_WIDTH_HUGE ? 0x1ffffffu : 64);
   putue(w, 64);
   put1(w, f == F_CROP || f == F_BAD_CROP);
   if (f == F_CROP || f == F_BAD_CROP)
   {
      putue(w, f == F_CROP ? 1 : 32); putue(w, f == F_CROP ? 2 : 0);
      putue(w, 3); putue(w, 4);
   }
   putue(w, (uint32_t)(depth - 8));  /* bit_depth_luma_minus8 */
   putue(w, (uint32_t)(depth - 8));  /* bit_depth_chroma_minus8 */
   putue(w, 4);                      /* log2_max_poc_lsb_minus4 */
   put1(w, f != F_INFERRED);
   for (i = f == F_INFERRED ? layers : 0; i <= layers; i++)
   {
      putue(w, f == F_DPB_HUGE ? 0xfffffffeu : (unsigned)i);
      putue(w, f == F_BAD_REORDER ? 1 : (unsigned)i);
      putue(w, f == F_LATENCY_HUGE ? 0xfffffffeu : (unsigned)(i * 2));
   }
   putue(w, 0);                      /* log2_min_cb_minus3 */
   putue(w, 1);                      /* log2_diff_max_min_cb */
   putue(w, 0);                      /* log2_min_tb_minus2 */
   putue(w, 2);                      /* log2_diff_max_min_tb */
   putue(w, 0);                      /* max_transform_hierarchy_depth_inter */
   putue(w, 0);                      /* ..._intra */
   put1(w, f == F_SCALING);
   if (f == F_SCALING) put1(w, 0);   /* default SPS matrices */
   put1(w, 0);                       /* amp */
   put1(w, 0);                       /* sao */
   put1(w, 0);                       /* pcm */
   putue(w, f == F_RPS_PRED ? 64 : f == F_RPS ? 1 : 0);
   if (f == F_RPS || f == F_RPS_PRED)
   {
      putue(w, 1); putue(w, 0); putue(w, 1); put1(w, 1);
      if (f == F_RPS_PRED)
         for (i = 1; i < 64; i++)
         {
            put1(w, 1); put1(w, 1); putue(w, 0);
            put1(w, 1); put1(w, 0); put1(w, 0);
         }
   }
   put1(w, f == F_LT || f == F_LT_TABLE);
   if (f == F_LT || f == F_LT_TABLE) putue(w, f == F_LT_TABLE ? 2 : 0);
   if (f == F_LT_TABLE)
   {
      putn(w, 3, 8); put1(w, 1);
      putn(w, 4, 8); put1(w, 1);
   }
   put1(w, 0);                       /* temporal_mvp */
   put1(w, 0);                       /* strong_intra_smoothing */
   put1(w, f >= F_VUI_SDR);           /* vui_parameters_present */
   if (f >= F_VUI_SDR)
   {
      put1(w, 1); putn(w, 255, 8);    /* extended SAR */
      putn(w, 1, 16); putn(w, 1, 16);
      put1(w, 1); put1(w, 0);         /* overscan */
      put1(w, 1); putn(w, 5, 3);      /* video signal */
      put1(w, f != F_VUI_HDR);
      put1(w, f != F_VUI_RANGE);
      if (f != F_VUI_RANGE)
      {
         putn(w, f == F_VUI_HDR ? 9 : 1, 8);
         if (f != F_VUI_SHORT)
         {
            putn(w, f == F_VUI_HDR ? 16 : 1, 8);
            putn(w, f == F_VUI_HDR ? 9 : 1, 8);
         }
      }
      if (f != F_VUI_SHORT)
      {
         put1(w, f == F_VUI_FIELD);
         if (f == F_VUI_FIELD) { putue(w, 2); putue(w, 3); }
         put1(w, 0); put1(w, f == F_VUI_FIELD); put1(w, 0);
      }
   }
   put1(w, 0);                       /* sps_extension_present */
   trailing(w);
}

/* set_extradata's answer for an SPS: -1 when it was read (and refused
 * for its bit depth), 0 when it was refused as malformed */
static int feed_sps(enum field f, int depth)
{
   bitw w;
   uint8_t unit[512], buf[600];
   const uint8_t *units[1];
   size_t lens[1], n;
   int ret;
   rh265_video *v = rh265_video_open();
   if (!v)
      abort();
   sps(&w, f, depth);
   units[0] = unit;
   lens[0]  = nal(unit, 33, &w);
   n        = hvcc(buf, units, lens, 1);
   ret      = rh265_video_set_extradata(v, buf, n);
   rh265_video_close(v);
   return ret;
}

static void expect(const char *what, int got, int want)
{
   if (got != want)
   {
      printf("[FAIL] %s: set_extradata %d, want %d\n", what, got, want);
      failures++;
   }
   else
      printf("[pass] %s\n", what);
}


static void putse(bitw *w, int value)
{
   putue(w, (uint32_t)(value <= 0 ? -2 * value : 2 * value - 1));
}

static void pps(bitw *w, int override, unsigned sps_id)
{
   unsigned size, m, i;
   memset(w, 0, sizeof(*w));
   putue(w, 0); putue(w, sps_id);
   put1(w, 0); put1(w, 1); putn(w, 2, 3);
   put1(w, 1); put1(w, 1); putue(w, 1); putue(w, 2); putse(w, -2);
   put1(w, 1); put1(w, 1); put1(w, 1); putue(w, 1);
   putse(w, -2); putse(w, 3); put1(w, 1);
   put1(w, 1); put1(w, 1); put1(w, 1);
   put1(w, 0); put1(w, 1); put1(w, 1);
   put1(w, 1); put1(w, 1); put1(w, 0); putse(w, -1); putse(w, 2);
   put1(w, override);
   if (override)
      for (size = 0; size < 4; size++)
         for (m = 0; m < 6; m += size == 3 ? 3 : 1)
         {
            int value = 20 + (int)(size * 6 + m);
            put1(w, 1);
            if (size >= 2) putse(w, value - 8);
            for (i = 0; i < (size ? 64u : 16u); i++)
               putse(w, !i && size < 2 ? value - 8 : 0);
         }
   put1(w, 1); putue(w, 1); put1(w, 1);
   trailing(w);
}

static void test_params(void)
{
   static rh265_video_params_t out, saved;
   bitw w;
   static uint8_t units_buf[2][1024], buf[2200];
   const uint8_t *units[2];
   size_t lens[2], n;
   unsigned depth, m, i;
   rh265_video *v = rh265_video_open();
   if (!v) abort();
   units[0] = units_buf[0]; units[1] = units_buf[1];
   memset(&out, 0xaa, sizeof(out));
   expect("missing PPS snapshot", rh265_video_get_params(v, 0, &out), -1);
   for (i = 0; i < sizeof(out); i++)
      if (((uint8_t*)&out)[i]) { failures++; break; }
   for (depth = 8; depth <= 10; depth += 2)
   {
      sps(&w, F_SCALING, (int)depth); lens[0] = nal(units_buf[0], 33, &w);
      pps(&w, 0, 0); lens[1] = nal(units_buf[1], 34, &w);
      n = hvcc(buf, units, lens, 2);
      expect("snapshot parameter input", rh265_video_set_extradata(v, buf, n), 0);
      expect("snapshot parsed PPS/SPS", rh265_video_get_params(v, 0, &out), 0);
      if (out.bit_depth != (int)depth || out.width != 64 || out.height != 64
            || out.log2_min_cb != 3 || out.log2_ctb != 4 || out.log2_max_tb != 4
            || out.log2_max_poc_lsb != 8 || out.init_qp != 24
            || out.num_ref_idx_l0_default != 2 || out.num_ref_idx_l1_default != 3
            || out.cb_qp_offset != -2 || out.cr_qp_offset != 3
            || out.beta_offset_div2 != -1 || out.tc_offset_div2 != 2
            || out.log2_parallel_merge_level != 3 || !out.entropy_coding_sync_enabled
            || !out.scaling_list_enabled || out.list8[0][63] != 115
            || out.list8[3][63] != 91 || out.dc32[0] != 16 || out.dc32[1] != 16)
         failures++;
      saved = out;
      out.width = 999; out.list8[0][0] = 255;
      expect("snapshot independent ownership", rh265_video_get_params(v, 0, &out), 0);
      if (memcmp(&saved, &out, sizeof(out))) failures++;
      pps(&w, 1, 0); lens[1] = nal(units_buf[1], 34, &w);
      n = hvcc(buf, units, lens, 2);
      expect("PPS scaling override input", rh265_video_set_extradata(v, buf, n), 0);
      expect("PPS scaling override snapshot", rh265_video_get_params(v, 0, &out), 0);
      for (m = 0; m < 6; m++)
      {
         for (i = 0; i < 16; i++) if (out.list4[m][i] != 20 + m) failures++;
         for (i = 0; i < 64; i++)
            if (out.list8[m][i] != 26 + m || out.list16[m][i] != 32 + m) failures++;
         if (out.dc16[m] != 32 + m) failures++;
      }
      for (m = 0; m < 2; m++)
      {
         for (i = 0; i < 64; i++) if (out.list32[m][i] != 38 + 3 * m) failures++;
         if (out.dc32[m] != 38 + 3 * m) failures++;
      }
   }
   sps(&w, F_CROP, 8); lens[0] = nal(units_buf[0], 33, &w);
   pps(&w, 0, 0); lens[1] = nal(units_buf[1], 34, &w);
   n = hvcc(buf, units, lens, 2);
   expect("crop input", rh265_video_set_extradata(v, buf, n), 0);
   expect("crop snapshot", rh265_video_get_params(v, 0, &out), 0);
   if (out.crop_left != 2 || out.crop_right != 4 || out.crop_top != 6
         || out.crop_bottom != 8 || out.scaling_list_enabled) failures++;
   for (i = 0; i < sizeof(out.list8); i++)
      if (((uint8_t*)out.list8)[i]) { failures++; break; }
   sps(&w, F_BAD_CROP, 8); lens[0] = nal(units_buf[0], 33, &w);
   n = hvcc(buf, units, lens, 2);
   expect("invalid crop input", rh265_video_set_extradata(v, buf, n), 0);
   expect("invalid crop snapshot", rh265_video_get_params(v, 0, &out), -1);
   pps(&w, 0, 1); lens[0] = nal(units_buf[0], 34, &w);
   n = hvcc(buf, units, lens, 1);
   expect("missing SPS input", rh265_video_set_extradata(v, buf, n), 0);
   expect("missing SPS snapshot", rh265_video_get_params(v, 0, &out), -1);
   expect("invalid PPS ID", rh265_video_get_params(v, ~0u, &out), -1);
   expect("null decoder", rh265_video_get_params(NULL, 0, &out), -1);
   expect("null snapshot", rh265_video_get_params(v, 0, NULL), -1);
   rh265_video_close(v);
}


static void slice_pps(bitw *w, int dependent, int extension)
{
   memset(w, 0, sizeof(*w));
   putue(w, 0); putue(w, 0);
   put1(w, dependent); put1(w, 0); putn(w, 0, 3);
   put1(w, 0); put1(w, 0); putue(w, 0); putue(w, 0); putse(w, 0);
   put1(w, 0); put1(w, 0); put1(w, 0); putse(w, 0); putse(w, 0);
   put1(w, 0); put1(w, 0); put1(w, 0); put1(w, 0);
   put1(w, 0); put1(w, 0); put1(w, 0); put1(w, 0); put1(w, 0);
   put1(w, 0); putue(w, 0); put1(w, extension);
   trailing(w);
}

static void slice_header(bitw *w, int type, int first, int slice_type, int rps_mode,
      int extension)
{
   memset(w, 0, sizeof(*w));
   put1(w, first);
   if (type >= 16) put1(w, 1);
   putue(w, 0);
   if (!first) putn(w, 2, 4);
   putue(w, (unsigned)slice_type);
   if (type != 19 && type != 20)
   {
      putn(w, 6, 8);
      put1(w, rps_mode == 1);
      if (rps_mode == 2)
      {
         put1(w, 1); putue(w, 0); put1(w, 1); putue(w, 0);
         put1(w, 1); put1(w, 1);
      }
      else if (!rps_mode || rps_mode >= 3)
      {
         putue(w, 2); putue(w, 1);
         putue(w, 0); put1(w, 1); putue(w, 1); put1(w, 0);
         putue(w, 2); put1(w, 1);
      }
      if (rps_mode >= 3)
      {
         int n, count = rps_mode == 7 ? 3 : rps_mode - 3;
         if (rps_mode == 7) putue(w, 2);
         putue(w, rps_mode == 7 ? 1 : (unsigned)count);
         for (n = 0; n < count; n++)
         {
            if (rps_mode == 7 && n < 2) putn(w, n, 1);
            else { putn(w, 5 + n, 8); put1(w, 1); }
            put1(w, !(rps_mode == 6 && n == 1));
            if (!(rps_mode == 6 && n == 1)) putue(w, n == 0 || n == 2 ? 2 : 1);
         }
      }
   }
   if (slice_type != 2)
   {
      put1(w, 0);
      if (!slice_type) put1(w, 0);
      putue(w, 0);
   }
   putse(w, 0);
   if (extension)
   {
      putue(w, 4);
      putn(w, 0, 24); putn(w, 1, 8);
   }
   trailing(w);
}

static void test_slice_info(void)
{
   static rh265_slice_info_t out, saved;
   static uint8_t units_buf[2][1024], buf[2200], unit[1024], original[1024];
   const uint8_t *units[2];
   bitw w;
   size_t lens[2], n, len, i;
   int mode, kind;
   rh265_video *v = rh265_video_open();
   if (!v) abort();
   units[0] = units_buf[0]; units[1] = units_buf[1];
   for (mode = 0; mode < 3; mode++)
   {
      sps(&w, mode ? F_RPS : F_NONE, 8); lens[0] = nal(units_buf[0], 33, &w);
      slice_pps(&w, 0, 1); lens[1] = nal(units_buf[1], 34, &w);
      n = hvcc(buf, units, lens, 2);
      expect("slice parameter input", rh265_video_set_extradata(v, buf, n), 0);
      for (kind = 0; kind < 3; kind++)
      {
         slice_header(&w, 1, 1, kind, mode, 1);
         len = nal(unit, 1, &w); memcpy(original, unit, len);
         expect("borrowed slice snapshot", rh265_video_get_slice(v, unit, len, &out), 0);
         if (out.header_bytes != len || out.header_bits != w.bits || !out.first_slice_in_pic
               || out.slice_type != kind || out.poc_lsb != 6 || !out.reference
               || out.pps_id || out.nal_type != 1 || out.temporal_id || out.irap || out.idr
               || memcmp(unit, original, len) || rh265_video_plane(v, 0, NULL, NULL, NULL))
            failures++;
         if (!mode && (out.num_negative != 2 || out.num_positive != 1
                  || out.delta_poc_before[0] != -1 || out.delta_poc_before[1] != -3
                  || out.delta_poc_after[0] != 3 || !out.used_before[0] || out.used_before[1]
                  || !out.used_after[0] || out.short_term_rps_bits != 16
                  || out.short_term_rps_sps_idx != -1 || out.predictor_delta_pocs)) failures++;
         if (mode == 1 && (out.num_negative != 1 || out.num_positive
                  || out.delta_poc_before[0] != -2 || out.short_term_rps_bits
                  || out.short_term_rps_sps_idx || out.predictor_delta_pocs)) failures++;
         if (mode == 2 && (out.num_negative != 2 || out.num_positive
                  || out.delta_poc_before[0] != -1 || out.delta_poc_before[1] != -3
                  || out.short_term_rps_bits != 6 || out.predictor_delta_pocs != 1)) failures++;
         saved = out;
         expect("repeat slice snapshot", rh265_video_get_slice(v, unit, len, &out), 0);
         if (memcmp(&out, &saved, sizeof(out))) failures++;
         if (len <= 2 + w.bits / 8) failures++;
         for (i = 2; i + 1 < len; i++)
            if (!unit[i-2] && !unit[i-1] && unit[i] == 3)
            {
               uint8_t byte = unit[i+1];
               unit[i+1] = 4;
               expect("invalid emulation prevention", rh265_video_get_slice(v, unit, len, &out), -1);
               unit[i+1] = byte;
               break;
            }
         if (!(unit[len-1] & 1))
         {
            unit[len-1] |= 1;
            expect("nonzero alignment padding", rh265_video_get_slice(v, unit, len, &out), -1);
            unit[len-1] &= (uint8_t)~1u;
         }
         for (i = 0; i < len; i++)
         {
            memset(&out, 0xaa, sizeof(out));
            if (rh265_video_get_slice(v, unit, i, &out) != -1) failures++;
            for (n = 0; n < sizeof(out); n++) if (((uint8_t*)&out)[n]) { failures++; break; }
         }
      }
   }
   slice_header(&w, 19, 1, 2, 0, 1); len = nal(unit, 19, &w);
   expect("IDR slice snapshot", rh265_video_get_slice(v, unit, len, &out), 0);
   if (!out.irap || !out.idr || !out.no_output_of_prior_pics || out.poc_lsb
         || out.num_negative || out.short_term_rps_bits) failures++;
   slice_header(&w, 1, 0, 2, 1, 1); len = nal(unit, 1, &w);
   expect("later slice snapshot", rh265_video_get_slice(v, unit, len, &out), 0);
   if (out.first_slice_in_pic || out.slice_segment_address != 2) failures++;
   unit[0] |= 0x80;
   expect("forbidden NAL bit", rh265_video_get_slice(v, unit, len, &out), -1);
   unit[0] = 2; unit[1] = 9;
   expect("multilayer NAL", rh265_video_get_slice(v, unit, len, &out), -1);
   unit[1] = 0;
   expect("zero temporal ID", rh265_video_get_slice(v, unit, len, &out), -1);
   unit[0] = 20; unit[1] = 1;
   expect("reserved VCL type", rh265_video_get_slice(v, unit, len, &out), -2);
   unit[0] = 66;
   expect("non-VCL NAL", rh265_video_get_slice(v, unit, len, &out), -2);
   slice_pps(&w, 1, 0); lens[0] = nal(units_buf[0], 34, &w);
   n = hvcc(buf, units, lens, 1);
   expect("dependent PPS input", rh265_video_set_extradata(v, buf, n), 0);
   memset(&w, 0, sizeof(w)); put1(&w, 0); putue(&w, 0); put1(&w, 1); trailing(&w);
   len = nal(unit, 1, &w);
   expect("unsupported dependent slice", rh265_video_get_slice(v, unit, len, &out), -2);
   slice_pps(&w, 0, 1); lens[1] = nal(units_buf[1], 34, &w);
   sps(&w, F_LT, 8); lens[0] = nal(units_buf[0], 33, &w);
   n = hvcc(buf, units, lens, 2);
   expect("long-term SPS input", rh265_video_set_extradata(v, buf, n), 0);
   slice_header(&w, 1, 1, 2, 3, 1); len = nal(unit, 1, &w);
   expect("empty long-term set", rh265_video_get_slice(v, unit, len, &out), 0);
   expect("empty long-term set preserves short-term RPS", out.num_negative, 2);
   slice_header(&w, 1, 1, 2, 4, 1); len = nal(unit, 1, &w);
   expect("hardware long-term slice", rh265_video_get_slice(v, unit, len, &out), 0);
   expect("hardware long-term count", out.long_term.count, 1);
   expect("hardware long-term lsb", out.long_term.poc_lsb[0], 5);
   buf[0] = buf[1] = 0; buf[2] = (uint8_t)(len >> 8); buf[3] = (uint8_t)len;
   memcpy(buf + 4, unit, len);
   expect("software long-term missing anchor", rh265_video_decode(v, buf, len + 4), -1);
   slice_header(&w, 1, 1, 2, 6, 1); len = nal(unit, 1, &w);
   expect("long-term MSB sequence", rh265_video_get_slice(v, unit, len, &out), 0);
   expect("long-term first cycle", out.long_term.msb_cycle[0], 2);
   expect("long-term absent cycle", out.long_term.msb_cycle[1], 0);
   expect("long-term continued cycle", out.long_term.msb_cycle[2], 4);
   sps(&w, F_LT_TABLE, 8); lens[0] = nal(units_buf[0], 33, &w);
   n = hvcc(buf, units, lens, 2);
   expect("long-term SPS table", rh265_video_set_extradata(v, buf, n), 0);
   slice_header(&w, 1, 1, 2, 7, 1); len = nal(unit, 1, &w);
   expect("long-term SPS indices", rh265_video_get_slice(v, unit, len, &out), 0);
   expect("long-term table lsb", out.long_term.poc_lsb[1], 4);
   expect("long-term table cycle", out.long_term.msb_cycle[1], 3);
   expect("long-term slice group resets cycle", out.long_term.msb_cycle[2], 2);
   expect("null slice decoder", rh265_video_get_slice(NULL, unit, len, &out), -1);
   expect("null slice input", rh265_video_get_slice(v, NULL, len, &out), -1);
   expect("null slice output", rh265_video_get_slice(v, unit, len, NULL), -1);
   rh265_video_close(v);
}


static void test_nal_iterator(void)
{
   uint8_t annex[] = {0,0,0,0,1,2,1,0xab,0,0,0,0,1,4,1,0xcd,0,0};
   uint8_t lengths[] = {0,0,1,0,2,1};
   uint8_t framed[20], saved[20];
   rh265_nal_t out;
   size_t pos = 0, at;
   unsigned lsz, i;
   expect("Annex-B first NAL", rh265_nal_next(annex, sizeof(annex), 0, &pos, &out), 1);
   if (out.data != annex + 5 || out.size != 3) failures++;
   expect("Annex-B next NAL", rh265_nal_next(annex, sizeof(annex), 0, &pos, &out), 1);
   if (out.data != annex + 13 || out.size != 3) failures++;
   expect("Annex-B iterator end", rh265_nal_next(annex, sizeof(annex), 0, &pos, &out), 0);
   if (out.data || out.size) failures++;
   for (lsz = 1; lsz <= 4; lsz++)
   {
      memset(framed, 0, sizeof(framed)); framed[lsz-1] = 3;
      framed[lsz] = 2; framed[lsz+1] = 1; framed[lsz+2] = 0xab;
      memcpy(framed + lsz + 3, framed, lsz + 3);
      memcpy(saved, framed, sizeof(saved));
      pos = 0;
      for (i = 0; i < 2; i++)
      {
         expect("length-prefixed borrowed NAL", rh265_nal_next(framed, 2 * (lsz + 3), lsz, &pos, &out), 1);
         if (out.data != framed + i * (lsz + 3) + lsz || out.size != 3) failures++;
      }
      if (memcmp(saved, framed, sizeof(saved))) failures++;
      expect("length-prefixed iterator end", rh265_nal_next(framed, pos, lsz, &pos, &out), 0);
      pos = 0; framed[lsz-1] = 20;
      expect("oversized NAL length", rh265_nal_next(framed, lsz + 3, lsz, &pos, &out), -1);
      if (pos || out.data || out.size) failures++;
      framed[lsz-1] = 0;
      expect("zero NAL length", rh265_nal_next(framed, lsz + 3, lsz, &pos, &out), -1);
   }
   pos = 0;
   expect("HVCC length resembling start code", rh265_nal_next(lengths, sizeof(lengths), 4, &pos, &out), -1);
   expect("invalid framing size", rh265_nal_next(annex, sizeof(annex), 5, &pos, &out), -1);
   at = 1;
   expect("truncated length prefix", rh265_nal_next(lengths, 2, 4, &at, &out), -1);
   if (at != 1) failures++;
   annex[5] |= 0x80;
   expect("invalid NAL header", rh265_nal_next(annex, sizeof(annex), 0, &pos, &out), -1);
   expect("null iterator input", rh265_nal_next(NULL, 1, 0, &pos, &out), -1);
   expect("null iterator position", rh265_nal_next(lengths, 0, 0, NULL, &out), -1);
   expect("null iterator output", rh265_nal_next(lengths, 0, 0, &pos, NULL), -1);
}

static void test_access_unit(void)
{
   static uint8_t unit[2][1024], buf[2200], packet[2200], original[2200];
   static rh265_access_unit_t out, saved;
   static rh265_video_params_t before_params, after_params;
   const uint8_t *units[2];
   rh265_nal_t slices[2];
   bitw w;
   size_t lens[2], n, bytes, i;
   rh265_video *v = rh265_video_open();
   if (!v) abort();
   units[0] = unit[0]; units[1] = unit[1];
   sps(&w, F_NONE, 8); lens[0] = nal(unit[0], 33, &w);
   slice_pps(&w, 0, 0); lens[1] = nal(unit[1], 34, &w);
   n = hvcc(buf, units, lens, 2);
   expect("AU parameter input", rh265_video_set_extradata(v, buf, n), 0);
   expect("AU initial parameters", rh265_video_get_params(v, 0, &before_params), 0);
   slice_header(&w, 1, 1, 2, 0, 0); lens[0] = nal(unit[0], 1, &w);
   slice_header(&w, 1, 0, 1, 0, 0); lens[1] = nal(unit[1], 1, &w);
   packet[0] = 0; packet[1] = 0; packet[2] = 1;
   memcpy(packet + 3, unit[0], lens[0]);
   n = 3 + lens[0]; packet[n++] = 0; packet[n++] = 0; packet[n++] = 1;
   memcpy(packet + n, unit[1], lens[1]); n += lens[1];
   memcpy(original, packet, n);
   expect("mixed-slice borrowed AU", rh265_video_get_access_unit(v, packet, n, 0, slices, 2, &out), 0);
   if (out.num_slices != 2 || out.intra || out.coded_bytes != lens[0] + lens[1]
         || slices[0].data != packet + 3 || slices[1].data != packet + 6 + lens[0]
         || memcmp(packet, original, n) || rh265_video_plane(v, 0, NULL, NULL, NULL)) failures++;
   saved = out;
   expect("AU busy retry metadata", rh265_video_get_access_unit(v, packet, n, 0, slices, 2, &out), 0);
   if (memcmp(&saved, &out, sizeof(out))) failures++;
   expect("AU slice capacity", rh265_video_get_access_unit(v, packet, n, 0, slices, 1, &out), -1);
   for (i = 0; i < sizeof(out); i++) if (((uint8_t*)&out)[i]) { failures++; break; }
   bytes = lens[0];
   memcpy(packet + 3, unit[1], lens[1]);
   expect("AU missing first slice", rh265_video_get_access_unit(v, packet, 3 + lens[1], 0, slices, 2, &out), -1);
   memcpy(packet, original, n);
   memcpy(packet + 6 + bytes, unit[0], lens[0]);
   expect("AU containing two pictures", rh265_video_get_access_unit(v, packet, 6 + 2 * bytes, 0, slices, 2, &out), -1);
   slice_header(&w, 1, 0, 2, 0, 0); lens[1] = nal(unit[1], 1, &w);
   memcpy(packet, original, 3 + bytes + 3);
   memcpy(packet + 6 + bytes, unit[1], lens[1]); n = 6 + bytes + lens[1];
   expect("all-intra borrowed AU", rh265_video_get_access_unit(v, packet, n, 0, slices, 2, &out), 0);
   if (!out.intra) failures++;
   slice_header(&w, 1, 0, 2, 0, 0); w.buf[2] |= 0x80;
   lens[1] = nal(unit[1], 1, &w);
   memcpy(packet + 6 + bytes, unit[1], lens[1]); n = 6 + bytes + lens[1];
   expect("AU mismatched POC", rh265_video_get_access_unit(v, packet, n, 0, slices, 2, &out), -1);
   sps(&w, F_NONE, 8); lens[0] = nal(unit[0], 33, &w);
   packet[0] = 0; packet[1] = 0; packet[2] = 1; memcpy(packet + 3, unit[0], lens[0]);
   expect("matching in-band SPS", rh265_video_get_access_unit(v, packet, 3 + lens[0], 0, slices, 2, &out), 0);
   if (out.num_slices || out.coded_bytes) failures++;
   sps(&w, F_NONE, 10); lens[0] = nal(unit[0], 33, &w);
   memcpy(packet + 3, unit[0], lens[0]);
   expect("changed in-band SPS refused", rh265_video_get_access_unit(v, packet, 3 + lens[0], 0, slices, 2, &out), -2);
   slice_pps(&w, 0, 0); lens[0] = nal(unit[0], 34, &w);
   memcpy(packet + 3, unit[0], lens[0]);
   expect("matching in-band PPS", rh265_video_get_access_unit(v, packet, 3 + lens[0], 0, slices, 2, &out), 0);
   slice_pps(&w, 0, 1); lens[0] = nal(unit[0], 34, &w);
   memcpy(packet + 3, unit[0], lens[0]);
   expect("changed in-band PPS refused", rh265_video_get_access_unit(v, packet, 3 + lens[0], 0, slices, 2, &out), -2);
   expect("AU unchanged parameters", rh265_video_get_params(v, 0, &after_params), 0);
   if (memcmp(&before_params, &after_params, sizeof(before_params))) failures++;
   packet[3] = 70; packet[4] = 1; packet[5] = 0x80;
   expect("AUD-only AU", rh265_video_get_access_unit(v, packet, 6, 0, slices, 2, &out), 0);
   if (out.num_slices || out.intra || out.coded_bytes) failures++;
   expect("null AU input", rh265_video_get_access_unit(v, NULL, n, 0, slices, 2, &out), -1);
   expect("null AU decoder", rh265_video_get_access_unit(NULL, packet, n, 0, slices, 2, &out), -1);
   expect("null AU output", rh265_video_get_access_unit(v, packet, n, 0, slices, 2, NULL), -1);
   rh265_video_close(v);
}

static void test_color_info(void)
{
   static uint8_t units_buf[2][1024], buf[2200];
   const uint8_t *units[2];
   size_t lens[2], size;
   bitw w;
   rh265_video_color_t color;
   rh265_video *v = rh265_video_open();
   int f;
   if (!v) abort();
   units[0] = units_buf[0]; units[1] = units_buf[1];
   slice_pps(&w, 0, 0); lens[1] = nal(units_buf[1], 34, &w);
   for (f = F_VUI_SDR; f <= F_VUI_SHORT; f++)
   {
      sps(&w, (enum field)f, 10); lens[0] = nal(units_buf[0], 33, &w);
      size = hvcc(buf, units, lens, 2);
      rh265_video_close(v); v = rh265_video_open();
      if (!v) abort();
      expect("VUI extradata", rh265_video_set_extradata(v, buf, size), 0);
      expect("VUI getter", rh265_video_get_color(v, 0, &color), f == F_VUI_SHORT ? -1 : 0);
      if (f == F_VUI_SHORT) continue;
      expect("VUI signal", color.video_signal_present, 1);
      expect("VUI description", color.description_present, f != F_VUI_RANGE);
      expect("VUI range", color.full_range, f != F_VUI_HDR);
      expect("VUI matrix", color.matrix, f == F_VUI_HDR ? 9 : f == F_VUI_RANGE ? 2 : 1);
      expect("VUI transfer", color.transfer, f == F_VUI_HDR ? 16 : f == F_VUI_RANGE ? 2 : 1);
      {
         rh265_sequence_info_t sequence;
         expect("VUI sequence", rh265_video_get_sequence(v, 0, &sequence), 0);
         expect("VUI field sequence", sequence.field_seq, f == F_VUI_FIELD);
      }
   }
   memset(&color, 0xa5, sizeof(color));
   expect("null color decoder", rh265_video_get_color(NULL, 0, &color), -1);
   expect("failed color cleared", color.matrix, 0);
   expect("null color output", rh265_video_get_color(v, 0, NULL), -1);
   rh265_video_close(v);
}

static void test_sequence_info(void)
{
   bitw w;
   static uint8_t units_buf[2][1024], buf[2200];
   const uint8_t *units[2];
   size_t lens[2], n;
   unsigned i, j;
   rh265_sequence_info_t out;
   rh265_short_term_set_t rps;
   enum field fields[6] = { F_SEQUENCE, F_INFERRED, F_LATENCY_HUGE, F_RPS, F_LT_TABLE, F_RPS_PRED };
   rh265_video *v = rh265_video_open();
   if (!v) abort();
   units[0] = units_buf[0]; units[1] = units_buf[1];
   expect("missing sequence PPS", rh265_video_get_sequence(v, 0, &out), -1);
   for (j = 0; j < 6; j++)
   {
      sps(&w, fields[j], 8); lens[0] = nal(units_buf[0], 33, &w);
      pps(&w, 0, 0); lens[1] = nal(units_buf[1], 34, &w);
      n = hvcc(buf, units, lens, 2);
      expect("sequence parameter input", rh265_video_set_extradata(v, buf, n), 0);
      expect("sequence query", rh265_video_get_sequence(v, 0, &out), 0);
      if (out.level_idc != 93 || out.profile_compatibility_flags != 0x60000000u
            || !out.temporal_id_nesting) failures++;
      if (fields[j] == F_SEQUENCE)
      {
         if (out.vps_id != 3 || out.profile_space != 1 || out.profile_idc != 2
               || out.tier_flag != 1 || out.constraint_flags_hi != 0xb1234567u
               || out.constraint_flags_lo != 0x89ab || out.max_sub_layers_minus1 != 2)
            failures++;
         for (i = 0; i <= 2; i++)
            if (out.max_dec_pic_buffering[i] != i + 1
                  || out.max_num_reorder_pics[i] != i
                  || out.max_latency_increase_plus1[i] != 2 * i) failures++;
         for (i = 3; i < 7; i++)
            if (out.max_dec_pic_buffering[i] || out.max_latency_increase_plus1[i]) failures++;
      }
      if (fields[j] == F_INFERRED)
         for (i = 0; i < 7; i++)
            if (out.max_dec_pic_buffering[i] != 7 || out.max_num_reorder_pics[i] != 6
                  || out.max_latency_increase_plus1[i] != 12) failures++;
      if (fields[j] == F_LATENCY_HUGE && out.max_latency_increase_plus1[0] != 0xfffffffeu)
         failures++;
      if (fields[j] == F_LT_TABLE && (out.num_long_term_pics != 2
            || out.lt_poc_lsb[0] != 3 || out.lt_poc_lsb[1] != 4
            || !out.lt_used[0] || !out.lt_used[1])) failures++;
      if (fields[j] == F_RPS)
      {
         expect("short-term set query", rh265_video_get_short_term_set(v, 0, 0, &rps), 0);
         if (out.num_short_term_sets != 1 || rps.num_before != 1 || rps.num_after
               || rps.delta_before[0] != -2 || !rps.used_before[0]) failures++;
         rps.delta_before[0] = 123;
         expect("short-term set ownership", rh265_video_get_short_term_set(v, 0, 0, &rps), 0);
         if (rps.delta_before[0] != -2) failures++;
      }
      if (fields[j] == F_RPS_PRED)
      {
         expect("last predicted short-term set", rh265_video_get_short_term_set(v, 0, 63, &rps), 0);
         if (out.num_short_term_sets != 64 || rps.num_before != 1 || rps.num_after
               || rps.delta_before[0] != -65 || !rps.used_before[0]) failures++;
      }
      expect("short-term set bounds", rh265_video_get_short_term_set(v, 0, 64, &rps), -1);
      for (i = 0; i < sizeof(rps); i++) if (((uint8_t*)&rps)[i]) { failures++; break; }
   }
   memset(&out, 0xaa, sizeof(out));
   expect("sequence invalid PPS", rh265_video_get_sequence(v, ~0u, &out), -1);
   for (i = 0; i < sizeof(out); i++) if (((uint8_t*)&out)[i]) { failures++; break; }
   expect("sequence null decoder", rh265_video_get_sequence(NULL, 0, &out), -1);
   expect("sequence null output", rh265_video_get_sequence(v, 0, NULL), -1);
   expect("short-term null decoder", rh265_video_get_short_term_set(NULL, 0, 0, &rps), -1);
   expect("short-term null output", rh265_video_get_short_term_set(v, 0, 0, NULL), -1);
   pps(&w, 0, 15); lens[1] = nal(units_buf[1], 34, &w);
   n = hvcc(buf, units, lens, 2);
   expect("sequence missing SPS input", rh265_video_set_extradata(v, buf, n), 0);
   expect("sequence missing SPS", rh265_video_get_sequence(v, 0, &out), -1);
   rh265_video_close(v);
}

static void test_vps_info(void)
{
   bitw w;
   static uint8_t units_buf[3][1024], buf[3300];
   const uint8_t *units[3];
   size_t lens[3], n;
   rh265_vps_info_t out;
   unsigned mode, i;
   rh265_video *v = rh265_video_open();
   if (!v) abort();
   for (i = 0; i < 3; i++) units[i] = units_buf[i];
   sps(&w, F_NONE, 8); lens[1] = nal(units_buf[1], 33, &w);
   pps(&w, 0, 0); lens[2] = nal(units_buf[2], 34, &w);
   expect("missing VPS", rh265_video_get_vps(v, 0, &out), -1);
   for (mode = 0; mode < 9; mode++)
   {
      memset(&w, 0, sizeof(w));
      putn(&w, 0, 4); putn(&w, 3, 2); putn(&w, mode == 2, 6);
      putn(&w, mode == 6 ? 7 : 0, 3); put1(&w, 1); putn(&w, 65535, 16);
      putn(&w, 1, 8); putn(&w, 0x60000000u, 32);
      putn(&w, 0xb0000000u, 32); putn(&w, 0, 16); putn(&w, 93, 8);
      put1(&w, 0); putue(&w, mode == 7 ? 16 : 4); putue(&w, mode == 8 ? 5 : 2);
      putue(&w, 0xfffffffeu);
      putn(&w, 0, 6); putue(&w, 0);
      put1(&w, mode == 1 || mode == 3);
      if (mode == 1 || mode == 3)
      {
         putn(&w, 1001, 32); putn(&w, 60000, 32);
         put1(&w, 1); putue(&w, 0xfffffffeu); putue(&w, mode == 3);
      }
      put1(&w, mode == 4); trailing(&w);
      lens[0] = nal(units_buf[0], 32, &w);
      if (mode == 5) lens[0] = 3;
      n = hvcc(buf, units, lens, 3);
      expect("VPS metadata input preserves software", rh265_video_set_extradata(v, buf, n), 0);
      memset(&out, 0xaa, sizeof(out));
      expect("VPS metadata availability", rh265_video_get_vps(v, 0, &out), mode < 2 ? 0 : -1);
      if (mode < 2)
      {
         if (out.sequence.profile_idc != 1 || out.sequence.level_idc != 93
               || out.sequence.constraint_flags_hi != 0xb0000000u
               || out.sequence.max_dec_pic_buffering[0] != 5
               || out.sequence.max_num_reorder_pics[0] != 2
               || out.sequence.max_latency_increase_plus1[0] != 0xfffffffeu
               || out.timing_present != mode) failures++;
         if (mode && (out.num_units_in_tick != 1001 || out.time_scale != 60000
               || !out.poc_proportional || out.num_ticks_poc_diff_one_minus1 != 0xfffffffeu)) failures++;
      }
      else
         for (i = 0; i < sizeof(out); i++) if (((uint8_t*)&out)[i]) { failures++; break; }
   }
   expect("VPS invalid PPS", rh265_video_get_vps(v, ~0u, &out), -1);
   expect("VPS null decoder", rh265_video_get_vps(NULL, 0, &out), -1);
   expect("VPS null output", rh265_video_get_vps(v, 0, NULL), -1);
   rh265_video_close(v);
}

int main(void)
{
   test_nal_iterator(); test_access_unit();
   test_slice_info();
   test_color_info();
   test_params();
   test_sequence_info();
   test_vps_info();
   expect("a well-formed SPS is read",
         feed_sps(F_NONE, 9), -1);
   expect("sps_id coded as 32 zero bits is malformed",
         feed_sps(F_ID_OVERLONG, 9), 0);
   expect("sps_id 2^32 - 2 is malformed",
         feed_sps(F_ID_HUGE, 9), 0);
   expect("a width of 2^25 - 1 is malformed",
         feed_sps(F_WIDTH_HUGE, 9), 0);
   expect("sps_max_latency_increase_plus1 at 2^32 - 2 is read",
         feed_sps(F_LATENCY_HUGE, 9), -1);
   expect("eight temporal layers are malformed", feed_sps(F_BAD_LAYERS, 9), 0);
   expect("oversized DPB is malformed", feed_sps(F_DPB_HUGE, 9), 0);
   expect("reorder limit exceeding DPB is malformed", feed_sps(F_BAD_REORDER, 9), 0);

   if (failures)
      return 1;
   printf("[pass] rh265_malformed_test\n");
   return 0;
}
