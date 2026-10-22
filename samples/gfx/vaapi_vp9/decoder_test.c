/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define VAAPI_VP9_EXPORT_TEST_MAIN vp9_export_contract_suite
#include "export_test.c"
#include "../../../gfx/common/vaapi_vp9_decoder.h"
static const vaapi_vp9_export_ops_t decoder_exports
      = {export_surface, cloexec, close_fd, identity};
static unsigned pos;
static void bits(unsigned v, unsigned n)
{
   while (n--)
   {
      packet[pos >> 3] |= ((v >> n) & 1) << (7 - (pos & 7));
      pos++;
   }
}

/* Header-complete packets; the mock accepts opaque entropy/tile bytes. */
static size_t coded(int key, int show, unsigned refresh, unsigned ref,
      int depth, int resilient, int updates)
{
   unsigned i;
   memset(packet, 0, sizeof(packet));
   pos = 0;
   bits(2, 2);
   bits(depth == 8 ? 0 : 1, 2);
   bits(0, 1);
   bits(!key, 1);
   bits(show, 1);
   bits(resilient, 1);
   if (key)
   {
      bits(0x49, 8);
      bits(0x83, 8);
      bits(0x42, 8);
      if (depth != 8)
         bits(depth == 12, 1);
      bits(2, 3);
      bits(1, 1); /* BT709 full range */
      bits(63, 16);
      bits(63, 16);
   }
   else
   {
      if (!show)
         bits(0, 1); /* not intra only */
      if (!resilient)
         bits(0, 2);
      bits(refresh, 8);
      for (i = 0; i < 3; i++)
      {
         bits(ref, 3);
         bits(0, 1);
      }
      bits(1, 1); /* size from LAST */
   }
   bits(0, 1); /* render dimensions */
   if (!key)
   {
      bits(0, 1);
      bits(1, 1);
   }
   if (!resilient)
   {
      bits(1, 1);
      bits(1, 1);
   }
   bits(0, 2);
   bits(0, 6);
   bits(0, 3);
   bits(1, 1);
   bits(updates, 1);
   if (updates)
   {
      bits(1, 1);
      bits(3, 6);
      bits(1, 1); /* ref0 = -3 */
      bits(0, 3);
      bits(0, 2);
   }
   bits(0, 8);
   bits(0, 3);
   bits(updates, 1);
   if (updates)
   {
      bits(0, 1);
      bits(1, 1);
      bits(1, 1); /* map, data, abs */
      for (i = 0; i < 32; i++)
      {
         bits(i == 0, 1);
         if (!i)
         {
            bits(7, 8);
            bits(0, 1);
         }
      }
   }
   bits(0, 1);
   bits(1, 16);
   return (pos + 7) / 8 + 2;
}

static size_t existing(unsigned ref, int depth)
{
   memset(packet, 0, sizeof(packet));
   pos = 0;
   bits(2, 2);
   bits(depth == 8 ? 0 : 1, 2);
   bits(1, 1);
   bits(ref, 3);
   return (pos + 7) / 8;
}

static void parsed_session(void)
{
   static rvp9_hw_session_t state;
   rvp9_hw_ops_t ops;
   rvp9_hdr header;
   video_hw_frame_t frame, repeated;
   vaapi_vp9_decoder_t *decoder;
   size_t size;
   unsigned before;
   memset(&pool, 0, sizeof(pool));
   init(8);
   size = coded(1, 1, 255, 0, 8, 0, 0);
   assert(!rvp9_parse_frame_header(packet, size, NULL, NULL, &header));
   memset(&ops, 0, sizeof(ops));
   assert(vaapi_vp9_decoder_open(&pool, &header, NULL, &decoder_exports, &ops));
   assert(rvp9_hw_session_init(&state, &ops, 10));
   target = 0;
   assert(rvp9_hw_session_submit(&state, packet, size) == 0);
   before = made;
   assert(rvp9_hw_session_output(&state, &frame) == 1);
   done[0] = 1;
   assert(rvp9_hw_session_output(&state, &frame) == 0);
   size = existing(3, 8);
   assert(rvp9_hw_session_submit(&state, packet, size) == 0 && made == before);
   assert(rvp9_hw_session_output(&state, &repeated) == 1);
   video_hw_frame_abandon(&frame);
   assert(rvp9_hw_session_output(&state, &repeated) == 0 && made == before);
   assert(repeated.width == 64 && repeated.color.range == 2);
   /* The repeated export is still owned by the caller. */
   decoder = (vaapi_vp9_decoder_t *)ops.decoder;
   assert(vaapi_vp9_decoder_detach(&ops) == decoder);
   assert(!vaapi_vp9_decoder_retire(decoder));
   video_hw_frame_abandon(&repeated);
   assert(vaapi_vp9_decoder_retire(decoder));
   assert(vaapi_vp9_pool_close(&pool));
}

int main(void)
{
   rvp9_hw_ops_t ops, untouched;
   vaapi_vp9_decoder_t *decoder;
   video_color_metadata_t color;
   video_hw_frame_t frame, other;
   static vaapi_frame_view_t view;
   static vaapi_vp9_export_t taken;
   unsigned depth, before;
   assert(!vp9_export_contract_suite());
   duplicate_fd = 0;
   for (depth = 8; depth <= 10; depth += 2)
   {
      memset(&pool, 0, sizeof(pool));
      init(depth);
      select_picture(0, false);
      picture.header.color_space = depth == 10 ? 5 : 2;
      picture.header.color_range = depth == 10;
      memset(&color, 0, sizeof(color));
      color.transfer = depth == 10 ? 16 : 13;
      color.max_cll = depth == 10 ? 1000 : 0;
      memset(&ops, 0, sizeof(ops));
      untouched = ops;
      picture.header.subsampling_x = 0;
      assert(!vaapi_vp9_decoder_open(
                   &pool, &picture.header, &color, &decoder_exports, &ops)
            && !memcmp(&ops, &untouched, sizeof(ops)));
      picture.header.subsampling_x = 1;
      assert(vaapi_vp9_decoder_open(
            &pool, &picture.header, &color, &decoder_exports, &ops));
      decoder = (vaapi_vp9_decoder_t *)ops.decoder;
      assert(!vaapi_vp9_decoder_retire(decoder));
      assert(ops.decode(ops.decoder, packet, sizeof(packet), &picture) == 0);
      memset(&frame, 0xa5, sizeof(frame));
      other = frame;
      assert(ops.output(ops.decoder, 0, &frame) == 1
            && !memcmp(&frame, &other, sizeof(frame)));
      done[0] = 1;
      assert(ops.output(ops.decoder, 0, &frame) == 0);
      assert(frame.api == VIDEO_HW_API_VULKAN && frame.width == 64
            && frame.height == 64 && frame.color.transfer == color.transfer
            && frame.color.max_cll == color.max_cll
            && frame.color.range == (depth == 10 ? 2u : 1u)
            && frame.pixfmt
                  == (depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12));
      assert(ops.output(ops.decoder, 0, &other) == 1);
      picture.previous_slot = pool.previous_slot;
      before = made;
      assert(ops.decode(ops.decoder, packet, sizeof(packet), &picture) == 1
            && made == before);
      assert(vaapi_frame_claim(
            frame.owner, frame.done, frame.done_value, frame.image, &view));
      memset(&taken, 0, sizeof(taken));
      assert(vaapi_frame_take(
            frame.owner, frame.done, frame.done_value, &taken));
      assert(vaapi_vp9_decoder_detach(&ops) == decoder && !ops.decoder);
      assert(!vaapi_vp9_decoder_retire(decoder) && pool.exports);
      assert(!vaapi_vp9_pool_close(&pool));
      assert(vaapi_frame_finish(
            frame.owner, frame.done, frame.done_value, &taken));
      assert(vaapi_vp9_decoder_retire(decoder) && !pool.exports);
      assert(vaapi_vp9_pool_close(&pool) && made == dropped);
      assert(!fd_live[31] && !fd_live[32]);
   }
   memset(&pool, 0, sizeof(pool));
   init(8);
   select_picture(0, false);
   memset(&ops, 0, sizeof(ops));
   assert(vaapi_vp9_decoder_open(
         &pool, &picture.header, NULL, &decoder_exports, &ops));
   decoder = (vaapi_vp9_decoder_t *)ops.decoder;
   fault = 2;
   assert(ops.decode(ops.decoder, packet, sizeof(packet), &picture) < 0);
   assert(vaapi_vp9_decoder_poll(decoder) < 0);
   assert(vaapi_vp9_decoder_detach(&ops) == decoder);
   assert(vaapi_vp9_decoder_retire(decoder));
   assert(!vaapi_vp9_pool_close(&pool));
   assert(vaapi_vp9_pool_terminated(&pool, session.display));
   made = dropped;
   fault = 0;
   parsed_session();
   puts("PASS: VP9 VA decoder backpressure, NV12/P010 color, retained opaque "
        "output and owner retirement; mock native calls");
   return 0;
}
