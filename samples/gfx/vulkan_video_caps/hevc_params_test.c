/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <formats/rmp4.h>
#include "../../../gfx/common/vulkan_hevc_params.h"
#include "../../../gfx/common/vulkan_hevc_picture.h"
#include "../d3d11_video_decode/hevc_device_data.h"

typedef struct hevc_fixture
{
   const char *name;
   const uint8_t *data;
   size_t size;
   unsigned depth;
   const uint32_t *hash;
   unsigned frames;
} hevc_fixture_t;
#define FIXTURE(kind, depth) {#kind #depth, hevc_##kind##depth, sizeof(hevc_##kind##depth), depth, hevc_##kind##depth##_hash, 12}
static const hevc_fixture_t fixtures[] = {
   FIXTURE(ip, 8), FIXTURE(bpyr, 8), FIXTURE(wpp, 8), FIXTURE(crop, 8), FIXTURE(repeat, 8),
   FIXTURE(ip, 10), FIXTURE(bpyr, 10), FIXTURE(wpp, 10), FIXTURE(crop, 10), FIXTURE(repeat, 10)
};

static int build_fixture(const hevc_fixture_t *f, vulkan_hevc_params_t *out)
{
   rmp4_t *mp4 = rmp4_open_memory(f->data, f->size);
   rh265_video *video = rh265_video_open();
   int i, ok = 0;
   if (!mp4 || !video) goto done;
   for (i = 0; i < rmp4_num_tracks(mp4); i++)
   {
      const rmp4_track *t = rmp4_get_track(mp4, i);
      if (t->codec != RMP4_CODEC_H265) continue;
      if (!rh265_video_set_extradata(video, t->codec_private, t->codec_private_size))
         ok = vulkan_hevc_params_build(out, video, 0);
      break;
   }
done:
   rh265_video_close(video); rmp4_close(mp4);
   return ok;
}

#ifndef HEVC_SESSION_TEST
int main(void)
{
   vulkan_hevc_params_t *p = (vulkan_hevc_params_t*)calloc(1, sizeof(*p));
   unsigned i, m, j, failures = 0;
   StdVideoH265ShortTermRefPicSet st;
   rh265_short_term_set_t set;
   StdVideoH265ProfileTierLevel ptl;
   rh265_sequence_info_t seq;
   rh265_hw_picture_t picture;
   rh265_access_unit_t au;
   StdVideoDecodeH265PictureInfo std_pic;
   if (!p) return 1;
   for (i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); i++)
   {
      int ok = build_fixture(&fixtures[i], p);
      if (ok)
         ok = p->sps.bit_depth_luma_minus8 == fixtures[i].depth - 8
            && p->sps.pProfileTierLevel == &p->sps_ptl
            && p->vps.pDecPicBufMgr == &p->vps_dpb
            && p->pps.pps_seq_parameter_set_id == p->sps.sps_seq_parameter_set_id
            && p->vps.vps_video_parameter_set_id == p->sps.sps_video_parameter_set_id
            && p->sps.pic_width_in_luma_samples == (unsigned)p->params.width
            && p->sps.conf_win_right_offset * 2 == (unsigned)p->params.crop_right
            && p->sps_ptl.general_profile_idc == (fixtures[i].depth == 8
               ? STD_VIDEO_H265_PROFILE_IDC_MAIN : STD_VIDEO_H265_PROFILE_IDC_MAIN_10);
      printf("HEVC Vulkan parameters %s: %s\n", fixtures[i].name, ok ? "PASS" : "FAIL");
      if (!ok) failures++;
   }
   for (m = 0; m < 6; m++)
      for (j = 0; j < 64; j++)
      {
         if (j < 16) p->params.list4[m][j] = (uint8_t)j;
         p->params.list8[m][j] = p->params.list16[m][j] = (uint8_t)j;
         if (m < 2) p->params.list32[m][j] = (uint8_t)j;
      }
   vulkan_hevc_scaling(p);
   /* Independently enumerated starts of the HEVC diagonal scans. */
   if (p->scaling.ScalingList4x4[0][1] != 4 || p->scaling.ScalingList4x4[0][2] != 1
         || p->scaling.ScalingList8x8[5][3] != 16 || p->scaling.ScalingList16x16[2][4] != 9
         || p->scaling.ScalingList32x32[1][5] != 2) failures++;
   memset(&set, 0, sizeof(set));
   set.num_before = set.num_after = 2;
   set.delta_before[0] = -2; set.delta_before[1] = -65538;
   set.delta_after[0] = 3; set.delta_after[1] = 5;
   set.used_before[1] = set.used_after[0] = 1;
   if (!vulkan_hevc_short_term(&st, &set) || st.delta_poc_s0_minus1[0] != 1
         || st.delta_poc_s0_minus1[1] != 65535 || st.delta_poc_s1_minus1[0] != 2
         || st.delta_poc_s1_minus1[1] != 1 || st.used_by_curr_pic_s0_flag != 2
         || st.used_by_curr_pic_s1_flag != 1 || st.flags.inter_ref_pic_set_prediction_flag) failures++;
   set.delta_before[1]--;
   if (vulkan_hevc_short_term(&st, &set)) failures++;
   set.delta_before[1] = INT32_MIN;
   if (vulkan_hevc_short_term(&st, &set)) failures++;
   set.delta_before[1] = -2;
   if (vulkan_hevc_short_term(&st, &set)) failures++;
   set.num_before = 17;
   if (vulkan_hevc_short_term(&st, &set)) failures++;
   memset(&seq, 0, sizeof(seq)); seq.profile_idc = 1; seq.level_idc = 93;
   if (!vulkan_hevc_ptl(&ptl, &seq) || ptl.general_level_idc != STD_VIDEO_H265_LEVEL_IDC_3_1) failures++;
   seq.level_idc = 94;
   if (vulkan_hevc_ptl(&ptl, &seq)) failures++;
   seq.level_idc = 93; seq.profile_space = 1;
   if (vulkan_hevc_ptl(&ptl, &seq)) failures++;
   memset(&picture, 0, sizeof(picture)); memset(&au, 0, sizeof(au));
   picture.poc = 10; picture.reference = 1; picture.num_refs = 3;
   picture.refs[0].poc = 8; picture.refs[0].slot = 9;
   picture.refs[1].poc = 12; picture.refs[1].slot = 2;
   picture.refs[2].poc = 4; picture.refs[2].slot = 12; picture.refs[2].long_term = 1;
   au.first.poc_lsb = 10; au.first.short_term_rps_sps_idx = -1;
   au.first.num_negative = au.first.num_positive = 1;
   au.first.delta_poc_before[0] = -2; au.first.delta_poc_after[0] = 2;
   au.first.used_before[0] = au.first.used_after[0] = 1;
   au.first.long_term.count = 1; au.first.long_term.poc_lsb[0] = 4; au.first.long_term.used[0] = 1;
   if (!vulkan_hevc_picture(&std_pic, 0, &p->params, &au, &picture)
         || std_pic.RefPicSetStCurrBefore[0] != 9 || std_pic.RefPicSetStCurrAfter[0] != 2
         || std_pic.RefPicSetLtCurr[0] != 12 || std_pic.RefPicSetLtCurr[1] != 255
         || std_pic.PicOrderCntVal != 10 || !std_pic.flags.IsReference) failures++;
   picture.refs[2].poc = 5;
   if (vulkan_hevc_picture(&std_pic, 0, &p->params, &au, &picture)) failures++;
   picture.refs[2].poc = 4; picture.refs[0].slot = 16;
   if (vulkan_hevc_picture(&std_pic, 0, &p->params, &au, &picture)) failures++;
   picture.refs[0].slot = 9; au.first.short_term_rps_bits = 65536;
   if (vulkan_hevc_picture(&std_pic, 0, &p->params, &au, &picture)) failures++;
   memset(p, 0xaa, sizeof(*p));
   if (vulkan_hevc_params_build(p, NULL, 0)) failures++;
   for (j = 0; j < sizeof(*p); j++)
      if (((uint8_t*)p)[j]) { failures++; break; }
   free(p);
   printf("HEVC Vulkan parameter mapping: %u failures\n", failures);
   return failures ? 1 : 0;
}
#endif
