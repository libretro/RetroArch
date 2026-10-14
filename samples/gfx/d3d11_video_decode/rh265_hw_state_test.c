/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include <formats/rh265_hw.h>
static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)
static rh265_hw_state_t state, saved;
static rh265_hw_picture_t picture, retry;
static rh265_video_params_t params;
static rh265_access_unit_t au;

static void setup(unsigned slots)
{
   CHECK(rh265_hw_state_init(&state, slots));
   memset(&params, 0, sizeof(params)); memset(&au, 0, sizeof(au));
   params.width = params.height = 64; params.bit_depth = 8; params.chroma_format_idc = 1;
   params.log2_max_poc_lsb = 4; params.max_dec_pic_buffering = 4;
   params.max_sub_layers_minus1 = 1;
   au.num_slices = 1; au.intra = 1; au.first.first_slice_in_pic = 1;
   au.first.nal_type = 19; au.first.slice_type = 2; au.first.pic_output_flag = 1;
}

static void frame(int lsb, int delta, int used)
{
   memset(&au.first, 0, sizeof(au.first)); au.intra = 0;
   au.first.first_slice_in_pic = 1; au.first.nal_type = 1;
   au.first.slice_type = 1; au.first.pic_output_flag = 1; au.first.poc_lsb = lsb;
   if (delta)
   {
      if (delta < 0)
      {
         au.first.num_negative = 1; au.first.delta_poc_before[0] = delta;
         au.first.used_before[0] = (uint8_t)used;
      }
      else
      {
         au.first.num_positive = 1; au.first.delta_poc_after[0] = delta;
         au.first.used_after[0] = (uint8_t)used;
      }
   }
}

static void test_transaction(void)
{
   int slot;
   setup(4); saved = state;
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture));
   CHECK(!memcmp(&state, &saved, sizeof(state)) && picture.poc == 0 && picture.slot == 0);
   retry = picture;
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture));
   CHECK(!memcmp(&picture, &retry, sizeof(picture)) && !memcmp(&state, &saved, sizeof(state)));
   CHECK(rh265_hw_commit(&state, &picture)); saved = state;
   CHECK(!rh265_hw_commit(&state, &picture) && !memcmp(&state, &saved, sizeof(state)));
   frame(1, -1, 1);
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture));
   CHECK(picture.slot == 1 && picture.num_refs == 1 && picture.refs[0].slot == 0);
   slot = rh265_hw_output_pick(&state, 0, false);
   CHECK(slot == 0 && rh265_hw_output_release(&state, (unsigned)slot));
   CHECK(rh265_hw_commit(&state, &picture));
   CHECK(state.references == 3 && state.outputs == 2);
   CHECK(rh265_hw_output_release(&state, 1));
   frame(2, -1, 1);
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture));
   CHECK(picture.slot == 0 && picture.keep_references == 2);
   CHECK(rh265_hw_commit(&state, &picture));
   CHECK(state.poc[0] == 2 && state.references == 3);
}

static void test_pressure(void)
{
   setup(1); params.max_dec_pic_buffering = 1;
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture) && rh265_hw_commit(&state, &picture));
   frame(1, 0, 0); saved = state;
   CHECK(rh265_hw_prepare(&state, &params, &au, &picture) == 1);
   CHECK(!memcmp(&state, &saved, sizeof(state)) && !picture.serial);
   CHECK(rh265_hw_output_release(&state, 0));
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture) && picture.slot == 0);
   CHECK(rh265_hw_commit(&state, &picture));
}

static void test_reorder(void)
{
   int order[] = {0,3,1,2}, expected[] = {0,1,2,3};
   unsigned i;
   int slot;
   setup(8); params.max_dec_pic_buffering = 8;
   for (i = 0; i < 4; i++)
   {
      if (i) frame(order[i], 0, 0);
      CHECK(!rh265_hw_prepare(&state, &params, &au, &picture));
      CHECK(rh265_hw_commit(&state, &picture));
   }
   CHECK(rh265_hw_output_pick(&state, 4, false) == -1);
   for (i = 0; i < 4; i++)
   {
      slot = rh265_hw_output_pick(&state, 2, true);
      CHECK(slot >= 0);
      if (slot < 0) break;
      CHECK(state.poc[slot] == expected[i]);
      CHECK(rh265_hw_output_release(&state, (unsigned)slot));
   }
   CHECK(rh265_hw_output_pick(&state, 0, true) == -1);
}

static void test_wrap_and_epochs(void)
{
   int slot;
   setup(8); params.max_dec_pic_buffering = 8;
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture) && rh265_hw_commit(&state, &picture));
   frame(7, 0, 0);
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture) && picture.poc == 7);
   CHECK(rh265_hw_commit(&state, &picture));
   frame(14, 0, 0);
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture) && picture.poc == 14);
   CHECK(rh265_hw_commit(&state, &picture));
   frame(1, 0, 0);
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture) && picture.poc == 17);
   CHECK(rh265_hw_commit(&state, &picture));
   frame(15, 0, 0); au.first.temporal_id = 1;
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture) && picture.poc == 15);
   CHECK(picture.prev_poc_tid0 == 17 && rh265_hw_commit(&state, &picture));
   frame(0, 0, 0); au.intra = 1; au.first.slice_type = 2; au.first.nal_type = 19;
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture) && picture.epoch == 2);
   CHECK(rh265_hw_commit(&state, &picture));
   slot = rh265_hw_output_pick(&state, 7, false);
   CHECK(slot >= 0 && state.output_epoch[slot] == 1);
   frame(0, 0, 0); au.intra = 1; au.first.slice_type = 2; au.first.nal_type = 19;
   au.first.no_output_of_prior_pics = 1;
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture) && !picture.keep_outputs);
   CHECK(rh265_hw_commit(&state, &picture));
   CHECK(state.outputs == (UINT32_C(1) << picture.slot));
   frame(15, 0, 0); au.first.nal_type = 9; saved = state;
   CHECK(rh265_hw_prepare(&state, &params, &au, &picture) == 2);
   CHECK(!memcmp(&state, &saved, sizeof(state)));
}

static void test_empty_long_term(void)
{
   setup(4);
   params.long_term_ref_pics_present = 1;
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture));
   CHECK(rh265_hw_commit(&state, &picture));
   frame(1, -1, 1);
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture));
   CHECK(rh265_hw_commit(&state, &picture));
   params.num_long_term_ref_pics = 1;
   frame(2, -1, 1);
   saved = state;
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture));
   CHECK(!memcmp(&state, &saved, sizeof(state)));
}

static void test_long_term(void)
{
   setup(4); params.long_term_ref_pics_present = 1;
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture) && rh265_hw_commit(&state, &picture));
   frame(1, 0, 0);
   au.first.long_term.count = 1; au.first.long_term.used[0] = 1;
   au.first.long_term.msb_present[0] = 1;
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture));
   CHECK(picture.num_refs == 1 && picture.refs[0].poc == 0 && picture.refs[0].long_term);
   CHECK(rh265_hw_commit(&state, &picture));
   frame(2, 0, 0); au.first.long_term.count = 1; au.first.long_term.used[0] = 1;
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture));
   CHECK(picture.keep_references == 1 && picture.refs[0].long_term);
   state.poc[0] = -16; state.prev_poc_tid0 = 17;
   au.first.poc_lsb = 2; au.first.long_term.msb_present[0] = 1; au.first.long_term.msb_cycle[0] = 2;
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture) && picture.refs[0].poc == -16);
   au.first.long_term.msb_cycle[0] = 1; saved = state;
   CHECK(rh265_hw_prepare(&state, &params, &au, &picture) == -1);
   CHECK(!memcmp(&state, &saved, sizeof(state)));
   state.references |= 4; state.poc[2] = 0; au.first.long_term.msb_present[0] = 0;
   CHECK(rh265_hw_prepare(&state, &params, &au, &picture) == -1);
   CHECK(!picture.serial);
}

static void test_reference_capacity(void)
{
   setup(4); params.long_term_ref_pics_present = 1;
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture) && rh265_hw_commit(&state, &picture));
   frame(1, -1, 1);
   au.first.num_negative = 2; au.first.delta_poc_before[1] = -2;
   au.first.long_term.count = 1; au.first.long_term.poc_lsb[0] = 15;
   saved = state;
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture));
   CHECK(picture.num_refs == 1 && !memcmp(&state, &saved, sizeof(state)));
   au.first.long_term.count = 2; au.first.long_term.poc_lsb[1] = 14;
   CHECK(rh265_hw_prepare(&state, &params, &au, &picture) == -1);
   CHECK(!picture.serial && !memcmp(&state, &saved, sizeof(state)));
   au.first.long_term.count = 0;
   au.first.num_positive = 2;
   au.first.delta_poc_after[0] = 1; au.first.delta_poc_after[1] = 2;
   CHECK(rh265_hw_prepare(&state, &params, &au, &picture) == -1);
   CHECK(!picture.serial && !memcmp(&state, &saved, sizeof(state)));
}

static void test_invalid(void)
{
   setup(1);
   CHECK(rh265_hw_prepare(&state, &params, &au, &picture) == -2);
   setup(4);
   frame(1, 0, 0);
   CHECK(rh265_hw_prepare(&state, &params, &au, &picture) == -2);
   au.first.nal_type = 19; au.first.poc_lsb = 0; au.intra = 1;
   CHECK(!rh265_hw_prepare(&state, &params, &au, &picture) && rh265_hw_commit(&state, &picture));
   frame(1, -2, 1); saved = state;
   CHECK(rh265_hw_prepare(&state, &params, &au, &picture) == -1);
   CHECK(!memcmp(&state, &saved, sizeof(state)) && !picture.serial);
   frame(1, -1, 1); params.width = 128;
   CHECK(rh265_hw_prepare(&state, &params, &au, &picture) == -2);
   params.width = 64; state.prev_poc_tid0 = INT32_MAX;
   frame(0, 0, 0);
   CHECK(rh265_hw_prepare(&state, &params, &au, &picture) == -1);
   CHECK(!rh265_hw_state_init(&state, 33));
   CHECK(!rh265_hw_state_init(NULL, 1));
}

int main(void)
{
   test_transaction(); test_pressure(); test_reorder(); test_wrap_and_epochs(); test_empty_long_term(); test_long_term(); test_reference_capacity(); test_invalid();
   printf("HEVC hardware owner state: %u failures\n", failures);
   return failures ? 1 : 0;
}
