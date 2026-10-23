/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define VAAPI_AV1_DECODER_TEST_MAIN av1_decoder_contract_suite
#include "decoder_test.c"
#undef VAAPI_AV1_DECODER_TEST_MAIN
static video_hw_frame_t held_frames[31];
static vaapi_frame_view_t held_views[31];
int main(void)
{
   static const vaapi_av1_export_ops_t exports
         = {export_surface, cloexec, close_fd, identity};
   vaapi_av1_decoder_t *detached;
   unsigned i, before;
   assert(!av1_decoder_contract_suite());
   reset();
   assert(vaapi_av1_pool_close(&pool));
   session.slots = 32;
   for (i = 0; i < 32; i++)
      session.surfaces[i] = 1000 + i;
   assert(vaapi_av1_pool_init(&pool, &session, &ops));
   test_depth = 8;
   fd_closed = duplicate_fd = release_fail = 0;
   memset(fd_live, 0, sizeof(fd_live));
   sequence.color_primaries = sequence.matrix_coefficients = 1;
   sequence.transfer_characteristics = 13;
   assert(vaapi_av1_decoder_open(
         &pool, &sequence, &container, &exports, &adapter));
   for (i = 0; i < 31; i++)
   {
      assert(!adapter.decode(
            adapter.decoder, packet, sizeof(packet), &sequence, &picture));
      expected_surface = 1001 + i;
      assert(!adapter.output(
            adapter.decoder, 0, &picture.grain.params, &held_frames[i]));
      assert(vaapi_frame_claim(held_frames[i].owner, held_frames[i].done,
            held_frames[i].done_value, held_frames[i].image, &held_views[i]));
      assert(held_views[i].source->slot == 1 + i);
   }
   assert(pool.exports == UINT32_C(0xfffffffe));
   before = created;
   assert(adapter.decode(
                adapter.decoder, packet, sizeof(packet), &sequence, &picture)
         == 1);
   assert(created == before);
   assert(vaapi_frame_finish(held_frames[0].owner, held_frames[0].done,
         held_frames[0].done_value, NULL));
   assert(vaapi_av1_decoder_poll((vaapi_av1_decoder_t *)adapter.decoder) == 1);
   assert(!adapter.decode(
         adapter.decoder, packet, sizeof(packet), &sequence, &picture));
   expected_surface = 1001;
   assert(!adapter.output(
         adapter.decoder, 0, &picture.grain.params, &held_frames[0]));
   assert(vaapi_frame_claim(held_frames[0].owner, held_frames[0].done,
         held_frames[0].done_value, held_frames[0].image, &held_views[0]));
   detached = vaapi_av1_decoder_detach(&adapter);
   assert(detached && !vaapi_av1_decoder_retire(detached));
   for (i = 0; i < 31; i++)
      assert(vaapi_frame_finish(held_frames[i].owner, held_frames[i].done,
            held_frames[i].done_value, NULL));
   assert(vaapi_av1_decoder_retire(detached));
   assert(!pool.exports && fd_closed == 64);
   assert(vaapi_av1_pool_close(&pool));
   puts("PASS: 31 retained AV1 grain outputs, BUSY without submission, "
        "identical packet retry and complete owner return; no device "
        "execution");
   return 0;
}
