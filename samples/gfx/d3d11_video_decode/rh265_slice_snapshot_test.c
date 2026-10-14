/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "../../../libretro-common/formats/h265/rh265.c"

static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)
static rh265_pic current, refs[3];
static rh265_mvfield field;

static void test_snapshots(void)
{
   rh265_video *v = rh265_video_open();
   rh265_dec *d;
   rh265_slice_job *first, *second;
   rh265_mv mv;
   if (!v) { CHECK(v); return; }
   d = &v->ctx[0];
   d->cur = &current; current.poc = 12;
   d->ref_list[0][0] = &refs[0]; d->ref_poc[0][0] = 4;
   d->ref_list[1][0] = &refs[1]; d->ref_poc[1][0] = 6;
   d->nb_refs[0] = d->nb_refs[1] = 1;
   d->ref_long_term[1][0] = 1;
   d->max_merge = 3; d->slice_tmvp = 1; d->col_ref = &refs[1];
   d->slice_seq = 0; d->sh.slice_type = RH265_SLICE_B;
   v->nal_scratch = (uint8_t*)calloc(1, 16); v->nal_scratch_cap = 16;
   if (!v->nal_scratch) { CHECK(v->nal_scratch); goto done; }
   first = rh265_ctx_queue_slice(v, d, v->nal_scratch, 16, 0, NULL, 0);
   if (!first) { CHECK(first); goto done; }
   d->ref_list[0][0] = &refs[2]; d->ref_poc[0][0] = 10;
   d->ref_list[1][0] = NULL; d->ref_poc[1][0] = 0;
   d->nb_refs[0] = 1; d->nb_refs[1] = 0;
   d->ref_long_term[1][0] = 0;
   d->max_merge = 5; d->slice_tmvp = 0; d->col_ref = NULL;
   d->slice_seq = 1; d->sh.slice_type = RH265_SLICE_P;
   v->nal_scratch = (uint8_t*)calloc(1, 16); v->nal_scratch_cap = 16;
   if (!v->nal_scratch) { CHECK(v->nal_scratch); goto done; }
   second = rh265_ctx_queue_slice(v, d, v->nal_scratch, 16, 0, NULL, 0);
   if (!second) { CHECK(second); goto done; }
   CHECK(first->next == second && d->jobs == first && d->jobs_tail == second);
   rh265_ctx_restore_slice(d, first);
   CHECK(d->ref_list[0][0] == &refs[0] && d->ref_list[1][0] == &refs[1]);
   CHECK(d->ref_poc[0][0] == 4 && d->ref_poc[1][0] == 6);
   CHECK(d->nb_refs[0] == 1 && d->nb_refs[1] == 1);
   CHECK(!d->ref_long_term[0][0] && d->ref_long_term[1][0]);
   CHECK(d->max_merge == 3 && d->slice_tmvp == 1 && d->col_ref == &refs[1]);
   CHECK(d->slice_seq == 0 && d->sh.slice_type == RH265_SLICE_B);
   memset(&field, 0, sizeof(field));
   field.pred = RH265_PF_L0; field.poc[0] = 4; field.mv[0].x = 16;
   CHECK(rh265_tmvp_mvset(d, &mv, &field, 0, 0, 0, 8));
   CHECK(mv.x == 32 && mv.y == 0);
   rh265_ctx_restore_slice(d, second);
   CHECK(d->ref_list[0][0] == &refs[2] && !d->ref_list[1][0]);
   CHECK(d->ref_poc[0][0] == 10 && !d->ref_poc[1][0]);
   CHECK(d->nb_refs[0] == 1 && !d->nb_refs[1]);
   CHECK(!d->ref_long_term[1][0]);
   CHECK(d->max_merge == 5 && !d->slice_tmvp && !d->col_ref);
   CHECK(d->slice_seq == 1 && d->sh.slice_type == RH265_SLICE_P);
   CHECK(rh265_tmvp_mvset(d, &mv, &field, 0, 0, 0, 8));
   CHECK(mv.x == 8 && mv.y == 0);
done:
   while (d->jobs)
   {
      rh265_slice_job *j = d->jobs;
      d->jobs = j->next; free(j->buf); free(j);
   }
   d->jobs_tail = NULL; d->cur = NULL;
   rh265_video_close(v);
}

static void test_reference_union(void)
{
   rh265_video *v = rh265_video_open();
   rh265_dec *d;
   unsigned i;
   if (!v) { CHECK(v); return; }
   d = v->cur = &v->ctx[0];
   d->sh.slice_type = RH265_SLICE_B;
   d->sh.nb_refs[0] = d->sh.nb_refs[1] = 1;
   v->nb_st_bef = 2; v->nb_st_aft = 1;
   v->st_bef[0] = 0; v->st_bef[1] = 2; v->st_aft[0] = 1;
   d->ref_list[0][RH265_MAX_REFS - 1] = &v->dpb[4];
   CHECK(!rh265_build_ref_lists(v));
   CHECK(d->npinned == 2);
   CHECK(retro_atomic_load_acquire_int(&v->dpb[0].readers) == 1);
   CHECK(retro_atomic_load_acquire_int(&v->dpb[1].readers) == 1);
   CHECK(!retro_atomic_load_acquire_int(&v->dpb[4].readers));
   d->sh.slice_type = RH265_SLICE_P;
   d->sh.rpl_mod[0] = 1; d->sh.list_entry[0][0] = 1;
   CHECK(!rh265_build_ref_lists(v));
   CHECK(d->npinned == 3 && d->nb_refs[1] == 0);
   for (i = 0; i < 3; i++)
      CHECK(retro_atomic_load_acquire_int(&v->dpb[i].readers) == 1);
   CHECK(!rh265_build_ref_lists(v));
   CHECK(d->npinned == 3);
   for (i = 0; i < 3; i++)
      CHECK(retro_atomic_load_acquire_int(&v->dpb[i].readers) == 1);
   rh265_ctx_unpin(d);
   CHECK(!d->npinned);
   for (i = 0; i < RH265_MAX_DPB; i++)
      CHECK(!retro_atomic_load_acquire_int(&v->dpb[i].readers));
   rh265_video_close(v);
}

static void test_long_term_prediction(void)
{
   static rh265_dec d;
   rh265_mv mv;
   memset(&d, 0, sizeof(d)); memset(&field, 0, sizeof(field));
   d.cur = &current; current.poc = 40;
   d.ref_poc[0][0] = 2; d.ref_long_term[0][0] = 1;
   field.poc[0] = 2; field.mv[0].x = 16; field.mv[0].y = -8;
   CHECK(!rh265_tmvp_mvset(&d, &mv, &field, 0, 0, 0, 12));
   field.long_term = 1;
   CHECK(rh265_tmvp_mvset(&d, &mv, &field, 0, 0, 0, 12));
   CHECK(mv.x == 16 && mv.y == -8);
   d.ref_long_term[0][0] = 0;
   CHECK(!rh265_tmvp_mvset(&d, &mv, &field, 0, 0, 0, 12));
   d.ref_long_term[0][0] = 1;
   d.mvf = &field; d.w4 = d.h4 = 1;
   field.pred = RH265_PF_L0; field.poc[0] = 4;
   CHECK(rh265_amvp_scaled(&d, 0, 0, 0, &mv, 0, 0));
   CHECK(mv.x == 16 && mv.y == -8);
   field.long_term = 0;
   CHECK(!rh265_amvp_scaled(&d, 0, 0, 0, &mv, 0, 0));
   field.poc[0] = 2;
   CHECK(!rh265_amvp_direct(&d, 0, 0, 0, &mv, 0, 0));
   field.long_term = 1;
   CHECK(rh265_amvp_direct(&d, 0, 0, 0, &mv, 0, 0));
}

static void test_long_term_sets(void)
{
   rh265_video *v = rh265_video_open();
   rh265_dec *d;
   if (!v) { CHECK(v); return; }
   d = v->cur = &v->ctx[0]; d->sps = &v->sps[0];
   v->sps[0].log2_max_poc_lsb = 4; v->sps[0].max_dec_pic_buffering = 4;
   v->cur_slot = 2; v->poc = 17; d->sh.poc_lsb = 1;
   v->dpb[0].in_use = v->dpb[0].is_ref = 1;
   v->dpb[1].in_use = v->dpb[1].is_ref = 1; v->dpb[1].poc = -16;
   d->sh.long_term.count = 1; d->sh.long_term.used[0] = 1;
   d->sh.long_term.msb_present[0] = 1; d->sh.long_term.msb_cycle[0] = 2;
   CHECK(!rh265_apply_rps(v));
   CHECK(v->nb_lt_curr == 1 && v->lt_curr[0] == 1 && v->dpb[1].long_term);
   CHECK(!v->dpb[0].is_ref && v->dpb[1].is_ref);
   d->sh.slice_type = RH265_SLICE_B; d->sh.nb_refs[0] = d->sh.nb_refs[1] = 1;
   CHECK(!rh265_build_ref_lists(v));
   CHECK(d->ref_list[0][0] == &v->dpb[1] && d->ref_list[1][0] == &v->dpb[1]);
   CHECK(d->ref_long_term[0][0] && d->ref_long_term[1][0]);
   rh265_ctx_unpin(d);
   d->sh.long_term.used[0] = 0;
   CHECK(!rh265_apply_rps(v));
   CHECK(!v->nb_lt_curr && v->dpb[1].is_ref && v->dpb[1].long_term);
   d->sh.long_term.used[0] = 1; d->sh.long_term.msb_cycle[0] = 3;
   CHECK(rh265_apply_rps(v) == -1 && v->dpb[1].is_ref);
   d->sh.long_term.msb_present[0] = 0;
   CHECK(!rh265_apply_rps(v) && v->lt_curr[0] == 1);
   v->dpb[0].in_use = v->dpb[0].is_ref = 1;
   CHECK(rh265_apply_rps(v) == -1 && v->dpb[1].is_ref);
   rh265_video_close(v);
}

int main(void)
{
   test_snapshots(); test_reference_union(); test_long_term_prediction(); test_long_term_sets();
   printf("HEVC slice reference snapshots: %u failures\n", failures);
   return failures ? 1 : 0;
}
