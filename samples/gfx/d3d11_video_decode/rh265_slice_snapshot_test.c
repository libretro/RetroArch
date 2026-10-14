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
   d->max_merge = 3; d->slice_tmvp = 1; d->col_ref = &refs[1];
   d->slice_seq = 0; d->sh.slice_type = RH265_SLICE_B;
   v->nal_scratch = (uint8_t*)calloc(1, 16); v->nal_scratch_cap = 16;
   if (!v->nal_scratch) { CHECK(v->nal_scratch); goto done; }
   first = rh265_ctx_queue_slice(v, d, v->nal_scratch, 16, 0, NULL, 0);
   if (!first) { CHECK(first); goto done; }
   d->ref_list[0][0] = &refs[2]; d->ref_poc[0][0] = 10;
   d->ref_list[1][0] = NULL; d->ref_poc[1][0] = 0;
   d->nb_refs[0] = 1; d->nb_refs[1] = 0;
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

int main(void)
{
   test_snapshots();
   printf("HEVC slice reference snapshots: %u failures\n", failures);
   return failures ? 1 : 0;
}
