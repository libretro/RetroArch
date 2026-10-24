/* SPDX-License-Identifier: MIT */
#define main legacy_container_test_main
#ifdef TERMINAL_WEBM
#include "rav1_webm_stream_test.c"
typedef rwebm_video_stream_t terminal_stream_t;
#define SET_AVAIL rwebm_video_stream_set_avail
#define SET_HW rwebm_video_stream_set_hw
#define NEXT rwebm_video_stream_next
#define HW_FRAME rwebm_video_stream_hw_frame
#define WAITING rwebm_video_stream_waiting
#define FRAME_TIME rwebm_video_stream_frame_time_ns
#define CLOSE rwebm_video_stream_close
#define DISCARD rwebm_video_stream_skip
#define REWIND rwebm_video_stream_rewind
#define COMPLETED rwebm_video_stream_completed
#else
#include "rav1_mp4_stream_test.c"
typedef rmp4_video_stream_t terminal_stream_t;
#define SET_AVAIL rmp4_video_stream_set_avail
#define SET_HW rmp4_video_stream_set_hw
#define NEXT rmp4_video_stream_next
#define HW_FRAME rmp4_video_stream_hw_frame
#define WAITING rmp4_video_stream_waiting
#define FRAME_TIME rmp4_video_stream_frame_time_ns
#define CLOSE rmp4_video_stream_close
#define DISCARD rmp4_video_stream_discard
#define REWIND rmp4_video_stream_rewind
#define COMPLETED rmp4_video_stream_completed
#endif
#undef main
static unsigned native_inputs, visible_inputs, last_visible, expected_inputs,
      last_slot;
static int drain_calls, drain_ready, missing_output, drain_failed, discard_mode;

static int terminal_decode(void *owner, const uint8_t *data, size_t size,
      const rav1_sequence_t *seq, const rav1_hw_picture_t *picture)
{
   int ret;
   if (!picture->header.show_existing)
      ret = decode(owner, data, size, seq, picture);
   else
   {
      CHECK(owner == &factory && data >= bytes + media_at
            && data + size <= bytes + used);
      decodes++;
      ret = decode_busy;
   }
   if (!ret)
   {
      native_inputs++;
      if (picture->header.show_existing || picture->header.show_frame)
      {
         visible_inputs++;
         last_slot = (unsigned)picture->slot;
      }
   }
   return ret;
}

static int terminal_output(void *owner, unsigned slot,
      const rav1_grain_t *grain, video_hw_frame_t *frame)
{
   CHECK(slot == last_slot);
   if (visible_inputs == last_visible && !drain_ready)
      return 1;
   return output(owner, slot, grain, frame);
}

static bool terminal_open(void *user, const rav1_sequence_t *seq,
      unsigned slots, rav1_hw_ops_t *ops)
{
   if (!open_backend(user, seq, slots, ops))
      return false;
   ops->decode = terminal_decode;
   ops->output = terminal_output;
   return true;
}

static int terminal_drain(void *owner, bool discard)
{
   CHECK(owner == &factory && native_inputs == expected_inputs);
   CHECK(discard == (discard_mode != 0));
   drain_calls++;
   if (drain_calls == 1)
      return 1;
   if (missing_output)
   {
      drain_failed = 1;
      return -2;
   }
   drain_ready = 1;
   return drain_calls == 2 ? 1 : 0;
}

static void terminal_case(int tail, int fail, int inband)
{
   terminal_stream_t *stream;
   video_hw_frame_t frame;
   unsigned i, displayed = 0;
   int ended = 0;
#ifdef TERMINAL_WEBM
   make_webm_clip(inband, 0, 0, 0, 3, 0, tail ? 3 : 0);
#else
   (void)inband;
   make_mp4_clip(1, tail ? 3 : 2);
#endif
   if (tail)
   {
      bytes[used - 2] = 0x7a;
      bytes[used - 1] = 0;
   }
   native_inputs = visible_inputs = 0;
   last_visible                   = tail ? 2 : 3;
   expected_inputs                = tail ? 5 : 6;
   drain_calls = drain_ready = drain_failed = 0;
   missing_output                           = fail;
   discard_mode                             = 0;
#ifdef TERMINAL_WEBM
   stream = rwebm_video_stream_open_hw_avail(bytes, used, used, NULL);
#else
   stream =
         rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL);
#endif
   CHECK(stream && SET_HW(stream, 1));
   if (!stream)
      return;
   SET_AVAIL(stream, used - sizeof(packet));
   for (i = 0; i < 512; i++)
   {
      if (NEXT(stream, NULL))
      {
         CHECK(HW_FRAME(stream, &frame));
         CHECK(FRAME_TIME(stream) == (int64_t)displayed * 40000000);
         if (tail && displayed == last_visible - 1)
            CHECK(frame.color.max_cll == 2000);
         video_hw_frame_abandon(&frame);
         displayed++;
      }
      else if (WAITING(stream) && native_inputs == 4)
         break;
      CHECK(!drain_calls);
   }
   CHECK(i < 512 && !drain_calls && displayed == (tail ? 1u : 2u));
   SET_AVAIL(stream, used);
   for (i = 0; i < 512; i++)
   {
      if (NEXT(stream, NULL))
      {
         CHECK(HW_FRAME(stream, &frame));
         CHECK(FRAME_TIME(stream) == (int64_t)displayed * 40000000);
         if (tail && displayed == last_visible - 1)
            CHECK(frame.color.max_cll == 2000);
         video_hw_frame_abandon(&frame);
         displayed++;
      }
      else if (!WAITING(stream) && (drain_calls >= 3 || drain_failed))
      {
         ended = 1;
         break;
      }
   }
   CHECK(ended && native_inputs == expected_inputs);
   CHECK(COMPLETED(stream) == !fail);
   CHECK(displayed == last_visible - (fail ? 1u : 0u));
   CHECK(fail ? drain_failed : drain_calls >= 3);
   CLOSE(stream);
   CHECK(opens == closes && exports == abandons);
}

static void terminal_discard_rewind(int inband)
{
   terminal_stream_t *stream;
   video_hw_frame_t frame;
   unsigned pass, i, displayed;
   int ended;
#ifdef TERMINAL_WEBM
   make_webm_clip(inband, 0, 0, 0, 3, 0, 3);
   stream = rwebm_video_stream_open_hw_avail(bytes, used, used, NULL);
#else
   (void)inband;
   make_mp4_clip(1, 3);
   stream =
         rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL);
#endif
   bytes[used - 2] = 0x7a;
   bytes[used - 1] = 0;
   CHECK(stream && SET_HW(stream, 1));
   if (!stream)
      return;
   for (pass = 0; pass < 2; pass++)
   {
      native_inputs = visible_inputs = 0;
      last_visible                   = 2;
      expected_inputs                = 5;
      drain_calls = drain_ready = drain_failed = missing_output = 0;
      discard_mode                                              = 1;
      displayed                                                 = 0;
      ended                                                     = 0;
      if (pass)
         REWIND(stream);
      for (i = 0; i < 512; i++)
      {
         if (DISCARD(stream, NULL) == 1)
         {
            CHECK(!HW_FRAME(stream, &frame));
            CHECK(FRAME_TIME(stream) == (int64_t)displayed * 40000000);
            displayed++;
         }
         else if (!WAITING(stream) && drain_calls >= 3)
         {
            ended = 1;
            break;
         }
      }
      CHECK(ended && displayed == 2 && native_inputs == 5);
      CHECK(COMPLETED(stream));
   }
   CLOSE(stream);
   CHECK(opens == closes && exports == abandons);
}

int main(void)
{
   factory.user           = &factory;
   factory.open           = terminal_open;
   factory.close          = close_backend;
   factory.packet_decoder = true;
   factory.drain          = terminal_drain;
   rav1_set_hw_backend(&factory);
   terminal_case(0, 0, 0);
   terminal_case(1, 0, 0);
   terminal_case(0, 1, 0);
   terminal_discard_rewind(0);
#ifdef TERMINAL_WEBM
   terminal_case(0, 0, 1);
   terminal_case(1, 0, 1);
   terminal_case(0, 1, 1);
   terminal_discard_rewind(1);
#endif
   printf("AV1 terminal container: %u failures\n", failures);
   return failures ? 1 : 0;
}
