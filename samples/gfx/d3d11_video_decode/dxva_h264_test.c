/* The DXVA H.264 buffers gfx/common/dxva_h264.h writes for an rh264
 * picture, byte by byte.
 *
 *   - the picture parameters land where DXVA_PicParams_H264 has its
 *     fields, the bit fields at their bits, unused references 0xFF;
 *   - the scaling lists follow 7.4.2.1.1: Flat_16 with none sent, fall-
 *     back rule A for the sequence (and for a picture set on a sequence
 *     that sends none), rule B for a picture set on one that does, the
 *     use-default flags as Table 7-3/7-4, raster order when asked;
 *   - from real streams through rh264: the JVT matrices x264 signals
 *     come out as the default tables, and lists of x264's own as sent.
 *
 * Built for Linux and with mingw-w64; on Windows the layout is also
 * held against the toolchain's dxva.h structures. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#ifdef _WIN32
#include <windows.h>
#include <dxva.h>
#endif

#include <formats/rmp4.h>
#include <formats/rh264.h>

#include "../../../gfx/common/dxva_h264.h"
#include "d3d11_video_decode_test_data.h"

static int fails;

static void check(const char *what, int ok)
{
   printf("  %-64s %s\n", what, ok ? "ok" : "FAIL");
   if (!ok)
      fails++;
}

static unsigned get16(const uint8_t *p)
{
   return (unsigned)p[0] | ((unsigned)p[1] << 8);
}

static uint32_t get32(const uint8_t *p)
{
   return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16)
      | ((uint32_t)p[3] << 24);
}

static rh264_hw_sps_t sps;
static rh264_hw_pps_t pps;
static rh264_hw_ref_t refs[3];
static rh264_hw_pic_t pic;

static void make_pic(void)
{
   memset(&sps, 0, sizeof(sps));
   memset(&pps, 0, sizeof(pps));
   memset(&pic, 0, sizeof(pic));
   sps.profile_idc                 = 100;
   sps.level_idc                   = 31;
   sps.chroma_format_idc           = 1;
   sps.bit_depth                   = 8;
   sps.log2_max_frame_num          = 9;
   sps.poc_type                    = 0;
   sps.log2_max_poc_lsb            = 7;
   sps.max_num_ref_frames          = 4;
   sps.width_in_mbs                = 11;
   sps.height_in_mbs               = 9;
   sps.direct_8x8_inference        = 1;
   sps.delta_pic_order_always_zero = 0;
   pps.entropy_coding_mode         = 1;
   pps.bottom_field_pic_order_in_frame_present = 1;
   pps.num_ref_idx_l0_default      = 3;
   pps.num_ref_idx_l1_default      = 2;
   pps.weighted_pred               = 1;
   pps.weighted_bipred_idc         = 2;
   pps.pic_init_qp                 = 24;
   pps.pic_init_qs                 = 29;
   pps.chroma_qp_index_offset      = -2;
   pps.second_chroma_qp_index_offset = 3;
   pps.deblocking_filter_control_present = 1;
   pps.constrained_intra_pred      = 1;
   pps.transform_8x8_mode          = 1;
   refs[0].slot = 3; refs[0].frame_num = 7; refs[0].poc[0] = 14;
   refs[0].poc[1] = 15;
   refs[1].slot = 9; refs[1].frame_num = 6; refs[1].poc[0] = -4;
   refs[1].poc[1] = -3;
   refs[2].slot = 1; refs[2].frame_num = 2; refs[2].poc[0] = 2;
   refs[2].poc[1] = 2; refs[2].long_term = 1;
   pic.sps       = &sps;
   pic.pps       = &pps;
   pic.slot      = 5;
   pic.frame_num = 8;
   pic.poc[0]    = 20;
   pic.poc[1]    = 21;
   pic.reference = 1;
   pic.intra     = 0;
   pic.refs      = refs;
   pic.num_refs  = 3;
}

static void test_pic_params(void)
{
   uint8_t o[DXVA_H264_PP_SIZE];
   unsigned bits, i;
   int unused = 1;

   printf("picture parameters:\n");
   make_pic();
   memset(o, 0xcd, sizeof(o));
   dxva_h264_pic_params(o, &pic, 0x12345678u);
   bits = get16(o + DXVA_H264_PP_BITS);
   check("frame size in macroblocks, minus one",
         get16(o + 0) == 10 && get16(o + 2) == 8);
   check("the picture's slot, as a frame", o[4] == 5);
   check("num_ref_frames", o[5] == 4);
   check("chroma_format_idc at bits 4-5", ((bits >> 4) & 3) == 1);
   check("RefPicFlag, constrained intra, weighted, bipred 2",
         (bits & (1u << 6)) && (bits & (1u << 7)) && (bits & (1u << 8))
         && ((bits >> 9) & 3) == 2);
   check("MbsConsecutive, frame_mbs_only, 8x8, bipred 8x8 at 3.1",
         (bits & (1u << 11)) && (bits & (1u << 12)) && (bits & (1u << 13))
         && (bits & (1u << 14)));
   check("no field, MBAFF or intra bits", !(bits & 3) && !(bits & 0x8000)
         && !(bits & 0xc));
   check("bit depths and Reserved16Bits 0",
         o[8] == 0 && o[9] == 0 && get16(o + 10) == 0);
   check("status feedback number", get32(o + 12) == 0x12345678u);
   check("references: slot, long-term in the top bit",
         o[16] == 3 && o[17] == 9 && o[18] == (1 | 0x80));
   for (i = 3; i < 16; i++)
      if (o[16 + i] != 0xff || get32(o + 40 + i * 8) || get16(o + 176 + i * 2))
         unused = 0;
   check("unused references 0xFF, their counts 0", unused);
   check("the picture's order counts",
         get32(o + 32) == 20 && get32(o + 36) == 21);
   check("the references' order counts",
         get32(o + 40) == 14 && get32(o + 44) == 15
         && (int32_t)get32(o + 48) == -4 && (int32_t)get32(o + 52) == -3);
   check("frame numbers, the long-term index for a long-term one",
         get16(o + 176) == 7 && get16(o + 178) == 6 && get16(o + 180) == 2);
   check("UsedForReferenceFlags: both fields of three frames",
         get32(o + 208) == 0x3f);
   check("NonExistingFrameFlags 0, frame_num",
         get16(o + 212) == 0 && get16(o + 214) == 8);
   check("qs, chroma offsets, continuation, qp",
         (int8_t)o[168] == 3 && (int8_t)o[169] == -2 && o[170] == 3
         && o[171] == 1 && (int8_t)o[172] == -2);
   check("default reference counts, minus one", o[173] == 2 && o[174] == 1);
   check("log2 max frame num / poc lsb, minus four, poc type 0",
         o[216] == 5 && o[217] == 0 && o[218] == 3);
   check("direct 8x8, CABAC, pic order present, deblocking control",
         o[219] == 0 && o[220] == 1 && o[221] == 1 && o[222] == 1
         && o[223] == 0 && o[224] == 0 && o[225] == 1 && o[226] == 0);
   check("the slice group map and the rest zeroed",
         o[227] == 0 && get16(o + 228) == 0 && o[230] == 0
         && o[DXVA_H264_PP_SIZE - 1] == 0);

   pic.intra = 1;
   pic.reference = 0;
   sps.level_idc = 30;
   dxva_h264_pic_params(o, &pic, 1);
   bits = get16(o + DXVA_H264_PP_BITS);
   check("intra picture, not a reference, level 3.0",
         (bits & 0x8000) && !(bits & (1u << 6)) && !(bits & (1u << 14)));

#ifdef _WIN32
   {
      DXVA_PicParams_H264 pp;
      make_pic();
      dxva_h264_pic_params(o, &pic, 77);
      memcpy(&pp, o, sizeof(pp));
      printf("  against dxva.h:\n");
      check("sizeof DXVA_PicParams_H264",
            sizeof(DXVA_PicParams_H264) == DXVA_H264_PP_SIZE);
      check("sizes and the current picture",
            pp.wFrameWidthInMbsMinus1 == 10 && pp.wFrameHeightInMbsMinus1 == 8
            && pp.CurrPic.Index7Bits == 5 && !pp.CurrPic.AssociatedFlag
            && pp.num_ref_frames == 4);
      check("the bit fields by name",
            !pp.field_pic_flag && !pp.MbaffFrameFlag
            && !pp.residual_colour_transform_flag && !pp.sp_for_switch_flag
            && pp.chroma_format_idc == 1 && pp.RefPicFlag
            && pp.constrained_intra_pred_flag && pp.weighted_pred_flag
            && pp.weighted_bipred_idc == 2 && pp.MbsConsecutiveFlag
            && pp.frame_mbs_only_flag && pp.transform_8x8_mode_flag
            && pp.MinLumaBipredSize8x8Flag && !pp.IntraPicFlag);
      check("feedback, references and their counts",
            pp.StatusReportFeedbackNumber == 77
            && pp.RefFrameList[0].Index7Bits == 3
            && pp.RefFrameList[2].Index7Bits == 1
            && pp.RefFrameList[2].AssociatedFlag == 1
            && pp.RefFrameList[3].bPicEntry == 0xff
            && pp.FieldOrderCntList[1][0] == -4
            && pp.FieldOrderCntList[1][1] == -3
            && pp.CurrFieldOrderCnt[0] == 20 && pp.CurrFieldOrderCnt[1] == 21
            && pp.FrameNumList[0] == 7 && pp.FrameNumList[2] == 2
            && pp.UsedForReferenceFlags == 0x3f);
      check("the picture parameter set fields",
            pp.pic_init_qs_minus26 == 3 && pp.chroma_qp_index_offset == -2
            && pp.second_chroma_qp_index_offset == 3
            && pp.ContinuationFlag == 1 && pp.pic_init_qp_minus26 == -2
            && pp.num_ref_idx_l0_active_minus1 == 2
            && pp.num_ref_idx_l1_active_minus1 == 1
            && pp.entropy_coding_mode_flag == 1
            && pp.pic_order_present_flag == 1
            && pp.deblocking_filter_control_present_flag == 1);
      check("the sequence parameter set fields",
            pp.frame_num == 8 && pp.log2_max_frame_num_minus4 == 5
            && pp.pic_order_cnt_type == 0
            && pp.log2_max_pic_order_cnt_lsb_minus4 == 3
            && pp.delta_pic_order_always_zero_flag == 0
            && pp.direct_8x8_inference_flag == 1
            && pp.num_slice_groups_minus1 == 0);
      check("sizeof DXVA_Qmatrix_H264 and DXVA_Slice_H264_Short",
            sizeof(DXVA_Qmatrix_H264) == DXVA_H264_QM_SIZE
            && sizeof(DXVA_Slice_H264_Short) == DXVA_H264_SLICE_SIZE);
      {
         DXVA_Slice_H264_Short sl;
         dxva_h264_slice(o, 0x1234, 0x56789);
         memcpy(&sl, o, sizeof(sl));
         check("a short slice by name",
               sl.BSNALunitDataLocation == 0x1234
               && sl.SliceBytesInBuffer == 0x56789
               && sl.wBadSliceChopping == 0);
      }
   }
#endif
}

static int list_is(const uint8_t *o, const uint8_t *want, unsigned n)
{
   return !memcmp(o, want, n);
}

static int list_is_value(const uint8_t *o, uint8_t v, unsigned n)
{
   unsigned i;
   for (i = 0; i < n; i++)
      if (o[i] != v)
         return 0;
   return 1;
}

static void test_qmatrix(void)
{
   uint8_t o[DXVA_H264_QM_SIZE], r[DXVA_H264_QM_SIZE];
   uint8_t own4[16], own8[64], pps4[16];
   unsigned i, j;
   int ok;

   printf("scaling lists:\n");
   for (i = 0; i < 16; i++)
   {
      own4[i] = (uint8_t)(40 + i);
      pps4[i] = (uint8_t)(80 + i);
   }
   for (i = 0; i < 64; i++)
      own8[i] = (uint8_t)(100 + i);

   make_pic();
   dxva_h264_qmatrix(o, &pic, 0);
   check("none sent: Flat_16", list_is_value(o, 16, sizeof(o)));

   /* the sequence: list 0 its own, 1 and 2 fall back to it, 3 to the
    * default, 4 and 5 to it; 8x8 intra by its default flag, inter
    * not sent */
   sps.scaling_present = 1;
   sps.scaling.present_mask = (1u << 0) | (1u << 6);
   sps.scaling.default_mask = (1u << 6);
   memcpy(sps.scaling.list4x4[0], own4, 16);
   memset(sps.scaling.list8x8[0], 99, 64);
   dxva_h264_qmatrix(o, &pic, 0);
   check("rule A: list 0 as sent, 1 and 2 from it",
         list_is(o, own4, 16) && list_is(o + 16, own4, 16)
         && list_is(o + 32, own4, 16));
   check("rule A: list 3 the default inter, 4 and 5 from it",
         list_is(o + 48, dxva_h264_default4[1], 16)
         && list_is(o + 64, dxva_h264_default4[1], 16)
         && list_is(o + 80, dxva_h264_default4[1], 16));
   check("rule A: 8x8 intra by its default flag, inter the default",
         list_is(o + 96, dxva_h264_default8[0], 64)
         && list_is(o + 160, dxva_h264_default8[1], 64));

   /* the picture set over the same sequence: rule B */
   pps.scaling_present = 1;
   pps.scaling.present_mask = (1u << 3) | (1u << 4) | (1u << 7);
   pps.scaling.default_mask = (1u << 4);
   memcpy(pps.scaling.list4x4[3], pps4, 16);
   memcpy(pps.scaling.list8x8[1], own8, 64);
   dxva_h264_qmatrix(o, &pic, 0);
   check("rule B: list 0 the sequence's, 1 and 2 from it",
         list_is(o, own4, 16) && list_is(o + 16, own4, 16)
         && list_is(o + 32, own4, 16));
   check("rule B: list 3 as sent, 4 its default flag, 5 from 4",
         list_is(o + 48, pps4, 16)
         && list_is(o + 64, dxva_h264_default4[1], 16)
         && list_is(o + 80, dxva_h264_default4[1], 16));
   check("rule B: 8x8 intra the sequence's, inter as sent",
         list_is(o + 96, dxva_h264_default8[0], 64)
         && list_is(o + 160, own8, 64));

   /* the same picture set over a sequence that sends none: rule A */
   sps.scaling_present = 0;
   dxva_h264_qmatrix(o, &pic, 0);
   check("picture lists alone: rule A, defaults where none sent",
         list_is(o, dxva_h264_default4[0], 16)
         && list_is(o + 32, dxva_h264_default4[0], 16)
         && list_is(o + 48, pps4, 16)
         && list_is(o + 64, dxva_h264_default4[1], 16)
         && list_is(o + 96, dxva_h264_default8[0], 64)
         && list_is(o + 160, own8, 64));

   /* raster order: each coded position where the scan puts it */
   dxva_h264_qmatrix(r, &pic, 1);
   ok = 1;
   for (i = 0; i < 6; i++)
      for (j = 0; j < 16; j++)
         if (r[i * 16 + dxva_h264_zigzag4[j]] != o[i * 16 + j])
            ok = 0;
   for (i = 0; i < 2; i++)
      for (j = 0; j < 64; j++)
         if (r[96 + i * 64 + dxva_h264_zigzag8[j]] != o[96 + i * 64 + j])
            ok = 0;
   check("raster order: the zig-zag lists scattered by the scans", ok);
   ok = 1;
   for (i = 0; i < 16 && ok; i++)
      for (j = 0; j < i; j++)
         if (dxva_h264_zigzag4[i] == dxva_h264_zigzag4[j])
            ok = 0;
   for (i = 0; i < 64 && ok; i++)
      for (j = 0; j < i; j++)
         if (dxva_h264_zigzag8[i] == dxva_h264_zigzag8[j])
            ok = 0;
   check("the scans are permutations", ok);
}

/* ---- streams: what rh264 hands a backend ---- */

static uint8_t got_qm[DXVA_H264_QM_SIZE];
static int got_pics, got_scaling;

static void *cap_open(void *user, const rh264_hw_sps_t *s, unsigned slots)
{
   (void)user; (void)s; (void)slots;
   return &got_pics;
}

static int cap_decode(void *dec, const rh264_hw_pic_t *p)
{
   uint8_t pp[DXVA_H264_PP_SIZE];
   (void)dec;
   if (!got_pics++)
   {
      dxva_h264_qmatrix(got_qm, p, 0);
      got_scaling = p->sps->scaling_present || p->pps->scaling_present;
   }
   dxva_h264_pic_params(pp, p, 1);
   return 0;
}

static int cap_output(void *dec, unsigned slot, video_hw_frame_t *out)
{
   (void)dec;
   memset(out, 0, sizeof(*out));
   out->image = slot + 1;
   return 0;
}

static void cap_close(void *dec)
{
   (void)dec;
}

static rh264_hw_ops_t cap_ops = { NULL, cap_open, cap_decode, cap_output,
   cap_close };

static int capture(const unsigned char *mp4, size_t len)
{
   rmp4_t *m;
   rmp4_packet pkt;
   rh264_video *v;
   int i, trk = -1, hw;

   got_pics = got_scaling = 0;
   if (!(m = rmp4_open_memory(mp4, len)))
      return 0;
   for (i = 0; i < rmp4_num_tracks(m); i++)
      if (rmp4_get_track(m, i)->type == RMP4_TRACK_VIDEO)
      { trk = i; break; }
   if (trk < 0 || !(v = rh264_video_open()))
   {
      rmp4_close(m);
      return 0;
   }
   rh264_video_set_extradata(v, rmp4_get_track(m, trk)->codec_private,
         rmp4_get_track(m, trk)->codec_private_size);
   hw = rh264_video_set_hw(v, 1);
   while (hw && rmp4_read_packet(m, &pkt) == 1)
      if (pkt.track == trk && rh264_video_decode(v, pkt.data, pkt.size) < 0)
         break;
   rh264_video_close(v);
   rmp4_close(m);
   return hw && got_pics;
}

static int has_values(const uint8_t *o, const char *csv, unsigned n)
{
   unsigned i, j, want[64], used[64];
   const char *c = csv;
   for (i = 0; i < n; i++)
   {
      want[i] = (unsigned)strtoul(c, (char**)&c, 10);
      if (*c == ',')
         c++;
      used[i] = 0;
   }
   /* the same values, in whichever order they were written */
   for (i = 0; i < n; i++)
   {
      for (j = 0; j < n; j++)
         if (!used[j] && want[j] == o[i])
         {
            used[j] = 1;
            break;
         }
      if (j == n)
         return 0;
   }
   return 1;
}

static void test_streams(void)
{
   printf("streams:\n");
   rh264_set_hw_backend(&cap_ops);
   check("high_jvt: decodes through a backend",
         capture(stream_high_jvt, sizeof(stream_high_jvt)) && got_scaling);
   check("high_jvt: the JVT matrices are the default tables",
         list_is(got_qm, dxva_h264_default4[0], 16)
         && list_is(got_qm + 32, dxva_h264_default4[0], 16)
         && list_is(got_qm + 48, dxva_h264_default4[1], 16)
         && list_is(got_qm + 80, dxva_h264_default4[1], 16)
         && list_is(got_qm + 96, dxva_h264_default8[0], 64)
         && list_is(got_qm + 160, dxva_h264_default8[1], 64));
   check("high_cqm: decodes through a backend",
         capture(stream_high_cqm, sizeof(stream_high_cqm)) && got_scaling);
   check("high_cqm: x264's own lists, as sent",
         has_values(got_qm,
            "6,12,12,18,18,18,24,24,24,24,30,30,30,36,36,40", 16)
         && has_values(got_qm + 48,
            "12,14,14,18,18,18,22,22,22,22,26,26,26,30,30,32", 16)
         && !list_is(got_qm, dxva_h264_default4[0], 16)
         && got_qm[96] == 8 && got_qm[160] == 10);
   check("baseline: no lists, Flat_16",
         capture(stream_baseline, sizeof(stream_baseline))
         && !got_scaling && list_is_value(got_qm, 16, sizeof(got_qm)));
   rh264_set_hw_backend(NULL);
}

int main(void)
{
   test_pic_params();
   test_qmatrix();
   test_streams();
   printf("dxva_h264_test: %s (%d failure%s)\n", fails ? "FAIL" : "PASS",
         fails, fails == 1 ? "" : "s");
   return fails ? 1 : 0;
}
