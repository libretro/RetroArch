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
   uint8_t buf[256];
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
   F_LATENCY_HUGE };

/* An SPS for a 64x64 8-bit-or-@depth 4:2:0 picture, one field crafted */
static void sps(bitw *w, enum field f, int depth)
{
   memset(w, 0, sizeof(*w));
   putn(w, 0, 4);                    /* sps_video_parameter_set_id */
   putn(w, 0, 3);                    /* sps_max_sub_layers_minus1 */
   put1(w, 1);                       /* temporal_id_nesting */
   putn(w, 0x01, 8);                 /* profile_tier_level: Main */
   putn(w, 0x60000000u, 32);
   putn(w, 0, 32); putn(w, 0, 16);
   putn(w, 93, 8);                   /* level 3.1 */
   if (f == F_ID_OVERLONG)
      putue_overlong(w);
   else
      putue(w, f == F_ID_HUGE ? 0xfffffffeu : 0);   /* sps_id */
   putue(w, 1);                      /* chroma_format_idc 4:2:0 */
   putue(w, f == F_WIDTH_HUGE ? 0x1ffffffu : 64);
   putue(w, 64);
   put1(w, 0);                       /* conformance_window */
   putue(w, (uint32_t)(depth - 8));  /* bit_depth_luma_minus8 */
   putue(w, (uint32_t)(depth - 8));  /* bit_depth_chroma_minus8 */
   putue(w, 4);                      /* log2_max_poc_lsb_minus4 */
   put1(w, 1);                       /* sub_layer_ordering_info */
   putue(w, 0);                      /* max_dec_pic_buffering_minus1 */
   putue(w, 0);                      /* max_num_reorder_pics */
   putue(w, f == F_LATENCY_HUGE ? 0xfffffffeu : 0);
   putue(w, 0);                      /* log2_min_cb_minus3 */
   putue(w, 1);                      /* log2_diff_max_min_cb */
   putue(w, 0);                      /* log2_min_tb_minus2 */
   putue(w, 2);                      /* log2_diff_max_min_tb */
   putue(w, 0);                      /* max_transform_hierarchy_depth_inter */
   putue(w, 0);                      /* ..._intra */
   put1(w, 0);                       /* scaling_list_enabled */
   put1(w, 0);                       /* amp */
   put1(w, 0);                       /* sao */
   put1(w, 0);                       /* pcm */
   putue(w, 0);                      /* num_short_term_ref_pic_sets */
   put1(w, 0);                       /* long_term_ref_pics_present */
   put1(w, 0);                       /* temporal_mvp */
   put1(w, 0);                       /* strong_intra_smoothing */
   put1(w, 0);                       /* vui_parameters_present */
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

int main(void)
{
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

   if (failures)
      return 1;
   printf("[pass] rh265_malformed_test\n");
   return 0;
}
