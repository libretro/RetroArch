/* SPDX-License-Identifier: MIT */
#define main legacy_stream_test_main
#include "rav1_hw_stream_test.c"
#undef main
static int eos_calls, eos_ready, output_failure, drain_failure;
static unsigned expected_decodes;

static int terminal_output(void *owner, unsigned slot,
      const rav1_grain_t *grain, video_hw_frame_t *frame)
{
   if (output_failure)
      return -2;
   if (!eos_ready)
      return 1;
   return output(owner, slot, grain, frame);
}

static bool terminal_open(void *user, const rav1_sequence_t *seq,
      unsigned slots, rav1_hw_ops_t *ops)
{
   if (!open_backend(user, seq, slots, ops))
      return false;
   ops->output = terminal_output;
   return true;
}

static int drain(void *owner, bool discard)
{
   CHECK(owner == &factory && !stream.packet && decodes == expected_decodes);
   (void)discard;
   eos_calls++;
   if (drain_failure)
      return -2;
   if (eos_calls == 1)
      return 1;
   eos_ready = 1;
   return stream.session.output_slot >= 0 ? 1 : 0;
}

static void start(void)
{
   eos_calls = eos_ready = output_failure = drain_failure = 0;
   CHECK(rav1_hw_stream_init(&stream, &sequence));
}

static void terminal_tail(int separate)
{
   static const uint8_t hdr1[] = {42, 6, 1, 3, 232, 1, 144, 128};
   static const uint8_t hdr2[] = {42, 6, 1, 7, 208, 2, 88, 128};
   video_hw_frame_t frame;
   size_t visible, hidden;
   unsigned before = decodes;
   start();
   unit_size = 0;
   append(hdr1, sizeof(hdr1));
   make_frame(1);
   visible = unit_size;
   append(packet, packet_size);
   append(hdr2, sizeof(hdr2));
   make_frame(0);
   hidden = unit_size;
   if (!separate)
      append(packet, packet_size);
   CHECK(!rav1_hw_stream_begin(&stream, unit, unit_size));
   CHECK(rav1_hw_stream_step(&stream, &frame, false)
         == RAV1_HW_STREAM_PROGRESS);
   expected = unit + visible;
   CHECK(rav1_hw_stream_step(&stream, &frame, false) == RAV1_HW_STREAM_PENDING);
   CHECK(stream.output_hdr.max_cll == 1000 && !frame.image);
   CHECK(rav1_hw_stream_step(&stream, &frame, false)
         == RAV1_HW_STREAM_PROGRESS);
   if (!separate)
   {
      expected = unit + hidden;
      CHECK(rav1_hw_stream_step(&stream, &frame, false)
            == RAV1_HW_STREAM_PROGRESS);
   }
   CHECK(rav1_hw_stream_step(&stream, &frame, false) == RAV1_HW_STREAM_END);
   CHECK(!stream.packet && !eos_calls && stream.session.output_slot >= 0);
   if (separate)
   {
      expected = packet;
      CHECK(!rav1_hw_stream_begin(&stream, packet, packet_size));
      CHECK(rav1_hw_stream_step(&stream, &frame, false)
            == RAV1_HW_STREAM_PROGRESS);
      CHECK(rav1_hw_stream_step(&stream, &frame, false) == RAV1_HW_STREAM_END);
   }
   expected_decodes = before + 2;
   memset(unit, 0, sizeof(unit));
   memset(packet, 0, sizeof(packet));
   CHECK(rav1_hw_stream_finish(&stream, &frame, false) == RAV1_HW_STREAM_BUSY);
   CHECK(rav1_hw_stream_finish(&stream, &frame, false) == RAV1_HW_STREAM_FRAME);
   CHECK(frame.image && frame.color.max_cll == 1000
         && stream.hdr.max_cll == 2000);
   video_hw_frame_abandon(&frame);
   CHECK(rav1_hw_stream_finish(&stream, &frame, false) == RAV1_HW_STREAM_END);
   CHECK(rav1_hw_stream_begin(&stream, unit, 1) < 0);
   rav1_hw_stream_close(&stream);
}

static void terminal_discard_and_failure(void)
{
   video_hw_frame_t frame;
   unsigned before;
   int mode;
   for (mode = 0; mode < 2; mode++)
   {
      before = decodes;
      start();
      make_frame(1);
      expected = packet;
      CHECK(!rav1_hw_stream_begin(&stream, packet, packet_size));
      CHECK(rav1_hw_stream_step(&stream, &frame, false)
            == RAV1_HW_STREAM_PENDING);
      CHECK(rav1_hw_stream_finish(&stream, &frame, false) < 0 && !eos_calls);
      CHECK(rav1_hw_stream_step(&stream, &frame, false) == RAV1_HW_STREAM_END);
      expected_decodes = before + 1;
      if (mode)
      {
         drain_failure = 1;
         CHECK(rav1_hw_stream_finish(&stream, &frame, false) == -2
               && stream.error == -2);
         CHECK(rav1_hw_stream_finish(&stream, &frame, false) == -2);
      }
      else
      {
         CHECK(rav1_hw_stream_finish(&stream, &frame, true)
               == RAV1_HW_STREAM_DISCARDED);
         CHECK(!frame.image && stream.session.output_slot < 0);
         CHECK(rav1_hw_stream_finish(&stream, &frame, true)
               == RAV1_HW_STREAM_END);
      }
      rav1_hw_stream_close(&stream);
   }
}

static void terminal_retry_cancel(void)
{
   video_hw_frame_t frame;
   unsigned before = decodes;
   start();
   make_frame(1);
   expected = packet;
   CHECK(!rav1_hw_stream_begin(&stream, packet, packet_size));
   CHECK(rav1_hw_stream_step(&stream, &frame, false) == RAV1_HW_STREAM_PENDING);
   CHECK(rav1_hw_stream_step(&stream, &frame, false) == RAV1_HW_STREAM_END);
   make_frame(0);
   CHECK(!rav1_hw_stream_begin(&stream, packet, packet_size));
   decode_busy    = 1;
   output_failure = 1;
   CHECK(rav1_hw_stream_step(&stream, &frame, false) == -2);
   CHECK(!stream.packet && !stream.session.phase && decodes == before + 2);
   rav1_hw_stream_close(&stream);
   decode_busy = 0;
}

int main(void)
{
   CHECK(rav1_sequence_parse(
         &sequence, sequence_obu + 2, sizeof(sequence_obu) - 2));
   factory.user           = &factory;
   factory.open           = terminal_open;
   factory.close          = close_backend;
   factory.packet_decoder = true;
   factory.drain          = drain;
   rav1_set_hw_backend(&factory);
   terminal_tail(0);
   terminal_tail(1);
   terminal_discard_and_failure();
   terminal_retry_cancel();
   CHECK(opens == closes);
   printf("AV1 terminal packet stream: %u failures\n", failures);
   return failures ? 1 : 0;
}
