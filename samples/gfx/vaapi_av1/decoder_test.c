/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define VAAPI_AV1_EXPORT_TEST_MAIN av1_export_contract_suite
#include "export_test.c"
#undef VAAPI_AV1_EXPORT_TEST_MAIN
#include "../../../gfx/common/vaapi_av1_decoder.c"
static rav1_hw_ops_t adapter;
static video_hw_frame_t first, second, refused_frame, saved_frame;
static vaapi_frame_view_t first_view, second_view;
static rav1_grain_t wrong_grain;
static video_color_metadata_t container;
static void hdr_metadata(void)
{
   static const vaapi_av1_export_ops_t exports
         = {export_surface, cloexec, close_fd, identity};
   static video_color_metadata_t hdr;
   vaapi_av1_decoder_t *detached;
   unsigned transfer, i;
   for (transfer = 16; transfer <= 18; transfer += 2)
   {
      reset();
      assert(vaapi_av1_pool_close(&pool));
      session.slots = 32;
      for (i = 0; i < 32; i++)
         session.surfaces[i] = 1000 + i;
      session.depth = sequence.bit_depth = 10;
      assert(vaapi_av1_pool_init(&pool, &session, &ops));
      test_depth = 10;
      fd_closed = duplicate_fd = release_fail = 0;
      memset(fd_live, 0, sizeof(fd_live));
      memset(&hdr, 0, sizeof(hdr));
      sequence.color_primaries = sequence.matrix_coefficients = 9;
      sequence.transfer_characteristics = transfer;
      hdr.primaries = hdr.matrix = 9;
      hdr.transfer = transfer;
      hdr.range = 1;
      hdr.max_cll = 1500;
      hdr.max_fall = 400;
      hdr.mastering.present = VIDEO_MASTERING_DISPLAY_COMPLETE;
      hdr.mastering.luminance_max = 10000000;
      hdr.mastering.chromaticity[0] = 34000;
      sequence.chroma_sample_position = 1;
      assert(!vaapi_av1_decoder_open(
            &pool, &sequence, &hdr, &exports, &adapter));
      sequence.chroma_sample_position = 0;
      hdr.transfer = 13;
      assert(!vaapi_av1_decoder_open(
            &pool, &sequence, &hdr, &exports, &adapter));
      assert(
            !adapter.decoder && !adapter.decode && !adapter.output && !created);
      hdr.transfer = transfer;
      assert(
            vaapi_av1_decoder_open(&pool, &sequence, &hdr, &exports, &adapter));
      assert(!adapter.decode(
            adapter.decoder, packet, sizeof(packet), &sequence, &picture));
      expected_surface = 1001;
      assert(
            !adapter.output(adapter.decoder, 0, &picture.grain.params, &first));
      assert(vaapi_frame_claim(first.owner, first.done, first.done_value,
            first.image, &first_view));
      assert(first_view.pixfmt == IMAGE_PIXFMT_P010);
      assert(first_view.color.transfer == transfer);
      assert(first_view.color.primaries == 9 && first_view.color.matrix == 9);
      assert(first_view.color.range == 1 && first_view.color.max_cll == 1500);
      assert(first_view.color.max_fall == 400);
      assert(!memcmp(&first_view.color.mastering, &hdr.mastering,
            sizeof(hdr.mastering)));
      assert(vaapi_frame_finish(
            first.owner, first.done, first.done_value, NULL));
      detached = vaapi_av1_decoder_detach(&adapter);
      assert(detached && vaapi_av1_decoder_retire(detached));
      assert(vaapi_av1_pool_close(&pool) && fd_closed == 2);
   }
}
#ifndef VAAPI_AV1_DECODER_TEST_MAIN
#define VAAPI_AV1_DECODER_TEST_MAIN main
#endif
int VAAPI_AV1_DECODER_TEST_MAIN(void)
{
   static const vaapi_av1_export_ops_t exports
         = {export_surface, cloexec, close_fd, identity};
   vaapi_av1_decoder_t *detached;
   unsigned i, before;
   assert(!av1_export_contract_suite());
   hdr_metadata();
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
   container.primaries = container.matrix = container.range = 1;
   container.transfer = 13;
   assert(vaapi_av1_decoder_open(
         &pool, &sequence, &container, &exports, &adapter));
   assert(!adapter.decode(
         adapter.decoder, packet, sizeof(packet), &sequence, &picture));
   expected_surface = 1001;
   assert(!adapter.output(adapter.decoder, 0, &picture.grain.params, &first));
   assert(vaapi_frame_claim(
         first.owner, first.done, first.done_value, first.image, &first_view));
   assert(first_view.source->slot == 1 && first_view.width == 128);
   assert(vaapi_frame_finish(first.owner, first.done, first.done_value, NULL));
   picture.slot = 1;
   picture.header.frame_type = RAV1_FRAME_INTER;
   picture.header.frame_is_intra = 0;
   picture.reference_slots[0] = 0;
   picture.reference_info[0].valid = 1;
   assert(!adapter.decode(
         adapter.decoder, packet, sizeof(packet), &sequence, &picture));
   assert(vaapi_av1_decoder_poll((vaapi_av1_decoder_t *)adapter.decoder) == 1);
   expected_surface = 1001;
   assert(!adapter.output(adapter.decoder, 0, &picture.grain.params, &first));
   assert(vaapi_frame_claim(
         first.owner, first.done, first.done_value, first.image, &first_view));
   assert(first_view.source->slot == 1);
   picture.slot = 0;
   picture.header.frame_type = RAV1_FRAME_KEY;
   picture.header.frame_is_intra = 1;
   picture.reference_slots[0] = -1;
   picture.reference_info[0].valid = 0;
   picture.grain.params.seed = 1234;
   assert(!adapter.decode(
         adapter.decoder, packet, sizeof(packet), &sequence, &picture));
   expected_surface = 1002;
   assert(!adapter.output(adapter.decoder, 0, &picture.grain.params, &second));
   assert(vaapi_frame_claim(second.owner, second.done, second.done_value,
         second.image, &second_view));
   assert(second_view.source->slot == 2 && first_view.source->slot == 1);
   assert(vaapi_export_current(first_view.source));
   assert(vaapi_export_current(second_view.source));
   assert((pool.exports & 6) == 6);
   wrong_grain = picture.grain.params;
   wrong_grain.seed++;
   memset(&refused_frame, 0xa5, sizeof(refused_frame));
   saved_frame = refused_frame;
   assert(adapter.output(adapter.decoder, 0, &wrong_grain, &refused_frame)
         == -1);
   assert(!memcmp(&refused_frame, &saved_frame, sizeof(saved_frame)));
   before = created;
   sequence.transfer_characteristics = 16;
   assert(adapter.decode(
                adapter.decoder, packet, sizeof(packet), &sequence, &picture)
         == -1);
   assert(created == before);
   sequence.transfer_characteristics = 13;
   detached = vaapi_av1_decoder_detach(&adapter);
   assert(detached && !adapter.decoder && !adapter.decode && !adapter.output);
   assert(!vaapi_av1_decoder_retire(detached));
   assert(vaapi_frame_finish(first.owner, first.done, first.done_value, NULL));
   assert(!vaapi_av1_decoder_retire(detached));
   assert(vaapi_frame_finish(
         second.owner, second.done, second.done_value, NULL));
   assert(vaapi_av1_decoder_retire(detached));
   assert(!pool.exports && fd_closed == 6);
   assert(vaapi_av1_pool_close(&pool));
   puts("PASS: AV1 logical reuse preserves old grain exports, color conflicts "
        "refuse and detach retains renderer ownership; no device execution");
   return 0;
}
