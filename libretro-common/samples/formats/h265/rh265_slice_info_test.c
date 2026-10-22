/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef RH265_SLICE_TEST_IMPL
#define RH265_SLICE_TEST_IMPL "../../../formats/h265/rh265.c"
#endif
#include RH265_SLICE_TEST_IMPL

static uint8_t rbsp[256], nal[512], preserved[512];
static unsigned bits;
static int chroma_bias = -1;
static size_t nal_size;
static rh265_slice_info_t basic;
#ifndef RH265_SLICE_WEIGHT_ONLY
static rh265_slice_decode_t decoded, empty;
static uint32_t entries[RH265_SLICE_ENTRY_POINTS_MAX];
static rh265_video_params_t before, after;
#endif

static void put_bit(unsigned bit)
{
   assert(bits < sizeof(rbsp) * 8 && bit <= 1);
   rbsp[bits >> 3] |= (uint8_t)(bit << (7 - (bits & 7)));
   bits++;
}

static void put_bits(unsigned value, unsigned count)
{
   while (count)
      put_bit((value >> --count) & 1);
}

static void put_ue(unsigned value)
{
   unsigned width = 0, code = value + 1, n = code;
   while (n >>= 1)
      width++;
   put_bits(0, width);
   put_bits(code, width + 1);
}

static void put_se(int value)
{
   put_ue(value > 0 ? (unsigned)(2 * value - 1) : (unsigned)(-2 * value));
}

static void put_weights(unsigned list, bool clipping)
{
   put_bit(1);
   if (!list)
      put_bit(0);
   put_bit(1);
   if (!list)
      put_bit(0);
   put_se(list ? -2 : 1);
   put_se(list ? 5 : -3);
   put_se(list ? -1 : (clipping ? 2 : 1));
   put_se(list ? 2 : chroma_bias);
   put_se(list ? 1 : -1);
   put_se(list ? -2 : 1);
}

static void make_slice(unsigned type, bool clipping, bool wpp, bool weighted)
{
   unsigned i, zeros = 0;
   memset(rbsp, 0, sizeof(rbsp));
   bits = 0;
   put_bit(1);
   if (type == 2)
      put_bit(0);
   put_ue(0);
   put_ue(type);
   if (type != 2)
   {
      put_bits(3, 4);
      put_bit(1);
      put_bit(1);
   }
   put_bit(1);
   put_bit(0);
   if (type != 2)
   {
      put_bit(1);
      put_ue(1);
      if (!type)
         put_ue(0);
      put_bit(1);
      put_bit(1);
      put_bit(0);
      if (!type)
      {
         put_bit(1);
         put_bit(0);
         put_bit(1);
      }
      put_bit(1);
      if (!type)
         put_bit(0);
      else
         put_ue(1);
      if (weighted)
      {
         put_ue(2);
         put_se(-1);
         put_weights(0, clipping);
         if (!type)
            put_weights(1, clipping);
      }
      put_ue(2);
   }
   put_se(1);
   put_se(-1);
   put_se(2);
   put_bit(1);
   put_bit(0);
   put_se(-2);
   put_se(3);
   put_bit(1);
   if (wpp)
   {
      put_ue(2);
      put_ue(2);
      put_bits(3, 3);
      put_bits(5, 3);
   }
   put_ue(5);
   put_bits(0, 8);
   put_bits(0, 8);
   put_bits(0, 8);
   put_bits(0, 8);
   put_bits(1, 8);
   put_bit(1);
   while (bits & 7)
      put_bit(0);
   nal[0]   = (uint8_t)((type == 2 ? 19 : 1) << 1);
   nal[1]   = 1;
   nal_size = 2;
   for (i = 0; i < bits / 8; i++)
   {
      if (zeros == 2 && rbsp[i] <= 3)
      {
         nal[nal_size++] = 3;
         zeros           = 0;
      }
      nal[nal_size++] = rbsp[i];
      zeros           = rbsp[i] ? 0 : zeros + 1;
   }
   memset(nal + nal_size, 0x55, 20);
   nal_size += 20;
   memcpy(preserved, nal, nal_size);
}

static rh265_video *make_decoder(bool wpp)
{
   rh265_video *v = rh265_video_open();
   rh265_sps *s;
   rh265_pps *p;
   assert(v);
   s        = &v->sps[0];
   p        = &v->pps[0];
   s->valid = p->valid = 1;
   s->width = s->height = 64;
   s->bit_depth_luma = s->bit_depth_chroma = 8;
   s->chroma_format_idc                    = 1;
   s->pic_size_ctbs                        = 4;
   s->log2_max_poc_lsb                     = 4;
   s->temporal_mvp_enabled = s->sao_enabled = 1;
   s->num_st_rps                            = 1;
   s->st_rps[0].num_negative = s->st_rps[0].num_positive = 1;
   s->st_rps[0].delta_poc_s0[0]                          = -1;
   s->st_rps[0].delta_poc_s1[0]                          = 1;
   s->st_rps[0].used_s0[0] = s->st_rps[0].used_s1[0] = 1;
   p->num_ref_idx_l0_default = p->num_ref_idx_l1_default = 1;
   p->init_qp                                            = 26;
   p->lists_modification_present = p->cabac_init_present = 1;
   p->weighted_pred = p->weighted_bipred     = 1;
   p->slice_chroma_qp_offsets_present        = 1;
   p->deblocking_filter_control_present      = 1;
   p->deblocking_filter_override_enabled     = 1;
   p->loop_filter_across_slices              = 1;
   p->entropy_coding_sync_enabled            = wpp;
   p->slice_segment_header_extension_present = 1;
   return v;
}

static void check_weights(rh265_video *v, unsigned type, bool clipping)
{
   const rh265_shdr *s = &v->sh_tmp;
   assert(!rh265_video_get_slice(v, nal, nal_size, &basic));
   assert(s->slice_type == (int)type && s->slice_qp == 27);
   assert(s->luma_w[0][0] == 5 && s->luma_o[0][0] == -3);
   assert(s->luma_w[0][1] == 4 && !s->luma_o[0][1]);
   assert(s->chroma_w[0][0][0] == (clipping ? 4 : 3));
   assert(s->chroma_o[0][0][0] == (clipping ? -128 : -65));
   assert(s->chroma_w[0][0][1] == 1 && s->chroma_o[0][0][1] == 65);
   if (!type)
   {
      assert(s->luma_w[1][0] == 2 && s->luma_o[1][0] == 5);
      assert(s->chroma_w[1][0][0] == 1 && s->chroma_o[1][0][0] == 66);
      assert(s->chroma_w[1][0][1] == 3 && s->chroma_o[1][0][1] == -66);
   }
}

int main(int argc, char **argv)
{
   rh265_video *v;
   unsigned type, wpp, depth;
#ifdef RH265_SLICE_WEIGHT_ONLY
   bool clipping = argc > 1 && !strcmp(argv[1], "clip");
   v             = make_decoder(true);
   make_slice(clipping ? 1 : 0, clipping, true, true);
   check_weights(v, clipping ? 1 : 0, clipping);
   rh265_video_close(v);
   (void)type;
   (void)wpp;
   (void)depth;
#else
   (void)argc;
   (void)argv;
   for (depth = 8; depth <= 10; depth += 2)
      for (wpp = 0; wpp < 2; wpp++)
         for (type = 0; type < 3; type++)
         {
            v                        = make_decoder(wpp != 0);
            v->sps[0].bit_depth_luma = v->sps[0].bit_depth_chroma = (int)depth;
            make_slice(type, true, wpp != 0, true);
            if (type != 2)
               check_weights(v, type, true);
            assert(!rh265_video_get_slice(v, nal, nal_size, &basic));
            assert(!rh265_video_get_params(v, 0, &before));
            assert(!rh265_video_get_slice_decode(v, nal, nal_size, &decoded,
                  wpp ? entries : NULL,
                  wpp ? RH265_SLICE_ENTRY_POINTS_MAX : 0));
            assert(!memcmp(&decoded.picture, &basic, sizeof(basic)));
            assert(decoded.qp == 27 && decoded.cb_qp_offset == -1
                  && decoded.cr_qp_offset == 2 && decoded.sao_luma
                  && !decoded.sao_chroma && !decoded.deblocking_filter_disabled
                  && decoded.beta_offset_div2 == -2
                  && decoded.tc_offset_div2 == 3
                  && decoded.loop_filter_across_slices);
            assert(decoded.num_entry_points == (wpp ? 2u : 0u));
            assert(decoded.picture.header_bytes
                  > 2 + decoded.picture.header_bits / 8);
            if (wpp)
               assert(entries[0] == 4 && entries[1] == 6);
            if (type != 2)
            {
               assert(decoded.weighted && decoded.temporal_mvp
                     && decoded.cabac_init);
               assert(decoded.luma_weight[0][0] == 5
                     && decoded.luma_offset[0][0] == -3);
               assert(decoded.chroma_offset[0][0][0] == -128);
               assert(decoded.num_refs[0] == 2 && decoded.list_modified[0]
                     && decoded.list_entry[0][0] == 1
                     && !decoded.list_entry[0][1]);
               assert(decoded.max_merge == 3);
               if (!type)
                  assert(decoded.num_refs[1] == 1 && decoded.mvd_l1_zero
                        && !decoded.collocated_from_l0
                        && !decoded.collocated_ref_idx
                        && decoded.luma_weight[1][0] == 2);
               else
                  assert(!decoded.num_refs[1] && decoded.collocated_from_l0
                        && decoded.collocated_ref_idx == 1);
            }
            else
               assert(!decoded.weighted && !decoded.num_refs[0]
                     && !decoded.num_refs[1]);
            assert(!rh265_video_get_params(v, 0, &after)
                  && !memcmp(&before, &after, sizeof(before)));
            assert(!v->out_count && !v->first_pic_decoded && !v->nal_scratch
                  && !memcmp(nal, preserved, nal_size));
            if (wpp)
            {
               entries[0] = entries[1] = 0x12345678;
               assert(rh265_video_get_slice_decode(
                           v, nal, nal_size, &decoded, entries, 1)
                     == -1);
               assert(!memcmp(&decoded, &empty, sizeof(decoded))
                     && entries[0] == 0x12345678 && entries[1] == 0x12345678);
               assert(rh265_video_get_slice_decode(
                           v, nal, nal_size, &decoded, NULL, 2)
                     == -1);
            }
            assert(rh265_video_get_slice_decode(v, nal, basic.header_bytes - 1,
                        &decoded, entries, RH265_SLICE_ENTRY_POINTS_MAX)
                  == -1);
            assert(!memcmp(&decoded, &empty, sizeof(decoded)));
            assert(rh265_video_get_slice_decode(NULL, nal, nal_size, &decoded,
                        entries, RH265_SLICE_ENTRY_POINTS_MAX)
                  == -1);
            nal[0] = 32 << 1;
            assert(rh265_video_get_slice_decode(v, nal, nal_size, &decoded,
                        entries, RH265_SLICE_ENTRY_POINTS_MAX)
                  == -2);
            assert(!memcmp(&decoded, &empty, sizeof(decoded)));
            rh265_video_close(v);
         }
   v                       = make_decoder(false);
   v->pps[0].weighted_pred = 0;
   make_slice(1, false, false, false);
   assert(!rh265_video_get_slice_decode(v, nal, nal_size, &decoded, NULL, 0));
   assert(!decoded.weighted && !decoded.luma_log2_denom
         && !decoded.chroma_log2_denom && decoded.num_refs[0] == 2);
   for (type = 0; type < 2; type++)
      assert(decoded.luma_weight[0][type] == 1
            && decoded.chroma_weight[0][type][0] == 1
            && decoded.chroma_weight[0][type][1] == 1
            && !decoded.luma_offset[0][type]);
   rh265_video_close(v);
   v           = make_decoder(false);
   chroma_bias = 511;
   make_slice(1, true, false, true);
   assert(!rh265_video_get_slice_decode(v, nal, nal_size, &decoded, NULL, 0));
   assert(decoded.chroma_offset[0][0][0] == 127);
   chroma_bias = 512;
   make_slice(1, true, false, true);
   assert(rh265_video_get_slice_decode(v, nal, nal_size, &decoded, NULL, 0)
         == -1);
   assert(!memcmp(&decoded, &empty, sizeof(decoded)));
   chroma_bias = -512;
   make_slice(1, true, false, true);
   assert(!rh265_video_get_slice_decode(v, nal, nal_size, &decoded, NULL, 0));
   assert(decoded.chroma_offset[0][0][0] == -128);
   chroma_bias = -513;
   make_slice(1, true, false, true);
   assert(rh265_video_get_slice_decode(v, nal, nal_size, &decoded, NULL, 0)
         == -1);
   assert(!memcmp(&decoded, &empty, sizeof(decoded)));
   rh265_video_close(v);
#endif
   puts("PASS: HEVC weighted lists, clipped chroma offsets and borrowed long-slice metadata without pixels or DPB advancement");
   return 0;
}
