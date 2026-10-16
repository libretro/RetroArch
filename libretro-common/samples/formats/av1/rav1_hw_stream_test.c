/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <formats/rav1_hw_stream.h>

static rav1_hw_stream_t stream;
static rav1_sequence_t sequence;
static rav1_hw_backend_t factory;
static uint8_t packet[128], unit[512];
static size_t packet_size, unit_size;
static unsigned position, failures, opens, closes, decodes, exports, abandons;
static int decode_busy, output_busy;
static const uint8_t *expected;
static const uint8_t sequence_obu[] = {0x0a, 0x09, 0x00, 0x00, 0x00, 0x02,
                                       0xaf, 0xff, 0x80, 0x00, 0x01};

#define CHECK(c)                                                               \
   do                                                                          \
   {                                                                           \
      if (!(c))                                                                \
      {                                                                        \
         fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #c);             \
         failures++;                                                           \
      }                                                                        \
   } while (0)

static void abandon(void *owner, uint64_t done, uint64_t value)
{
   CHECK(owner == &factory && done == 2 && value);
   abandons++;
}

static int decode(void *owner, const uint8_t *data, size_t size,
                  const rav1_sequence_t *seq, const rav1_hw_picture_t *picture)
{
   CHECK(owner == &factory && data == expected && size && seq->max_width == 64);
   CHECK(picture->tile_group.data >= data &&
         picture->tile_group.data + picture->tile_group.size <= data + size);
   decodes++;
   return decode_busy;
}

static int output(void *owner, unsigned slot, const rav1_grain_t *grain,
                  video_hw_frame_t *frame)
{
   CHECK(owner == &factory && slot < 9 && !grain->apply);
   if (output_busy)
      return 1;
   frame->image = 100 + ++exports;
   frame->width = frame->height = 64;
   frame->pixfmt = IMAGE_PIXFMT_NV12;
   frame->done = 2;
   frame->done_value = exports;
   frame->owner = &factory;
   frame->abandon = abandon;
   return 0;
}

static bool open_backend(void *user, const rav1_sequence_t *seq, unsigned slots,
                         rav1_hw_ops_t *ops)
{
   CHECK(user == &factory && seq->max_width == 64 && slots == 9);
   ops->decoder = &factory;
   ops->decode = decode;
   ops->output = output;
   opens++;
   return true;
}

static void close_backend(rav1_hw_ops_t *ops)
{
   CHECK(ops->decoder == &factory);
   closes++;
   memset(ops, 0, sizeof(*ops));
}

static void put(unsigned value, unsigned count)
{
   unsigned i;
   for (i = count; i; i--)
   {
      if ((value >> (i - 1)) & 1)
         packet[2 + position / 8] |= 1u << (7 - position % 8);
      position++;
   }
}

static void make_frame(unsigned show)
{
   memset(packet, 0, sizeof(packet));
   packet[0] = 0x32;
   position = 0;
   put(0, 3);
   put(show, 1);
   if (!show)
      put(3, 2);
   put(1, 1);
   put(0, 1);
   if (!show)
      put(255, 8);
   put(0, 1);
   put(1, 1);
   put(0, 8);
   put(0, 5);
   put(0, 1);
   while (position % 8)
      put(0, 1);
   put(128, 8);
   packet_size = 2 + position / 8;
   packet[1] = (uint8_t)(packet_size - 2);
}

static void append(const uint8_t *data, size_t size)
{
   CHECK(size <= sizeof(unit) - unit_size);
   memcpy(unit + unit_size, data, size);
   unit_size += size;
}

static void test_packet(void)
{
   static const uint8_t delimiter[] = {0x12, 0};
   static const uint8_t replay[] = {0x18, 0x88};
   video_hw_frame_t frame, held;
   size_t frame_offset, replay_offset;
   unsigned before;
   unit_size = 0;
   append(delimiter, sizeof(delimiter));
   append(sequence_obu, sizeof(sequence_obu));
   make_frame(0);
   frame_offset = unit_size;
   append(packet, packet_size);
   replay_offset = unit_size;
   append(replay, sizeof(replay));
   CHECK(rav1_hw_stream_init(&stream, NULL));
   CHECK(!rav1_hw_stream_begin(&stream, unit, unit_size));
   CHECK(rav1_hw_stream_step(&stream, &frame, false) ==
         RAV1_HW_STREAM_PROGRESS);
   CHECK(rav1_hw_stream_step(&stream, &frame, false) ==
         RAV1_HW_STREAM_PROGRESS);
   CHECK(opens == 1 && stream.offset == frame_offset);
   expected = unit + frame_offset;
   decode_busy = 1;
   CHECK(rav1_hw_stream_step(&stream, &frame, false) == RAV1_HW_STREAM_BUSY);
   CHECK(stream.offset == frame_offset && !frame.image && decodes == 1);
   CHECK(rav1_hw_stream_begin(&stream, packet, packet_size) == 1);
   decode_busy = 0;
   CHECK(rav1_hw_stream_step(&stream, &frame, false) ==
         RAV1_HW_STREAM_PROGRESS);
   CHECK(decodes == 2 && stream.offset == replay_offset);
   memset(unit + frame_offset, 0, packet_size);
   output_busy = 1;
   CHECK(rav1_hw_stream_step(&stream, &frame, false) == RAV1_HW_STREAM_BUSY);
   CHECK(decodes == 2 && stream.submitted && stream.offset == replay_offset);
   before = opens;
   rav1_set_hw_backend(NULL);
   output_busy = 0;
   CHECK(rav1_hw_stream_step(&stream, &held, false) == RAV1_HW_STREAM_FRAME);
   CHECK(held.image && opens == before && decodes == 2);
   CHECK(rav1_hw_stream_step(&stream, &frame, false) == RAV1_HW_STREAM_END);
   CHECK(!frame.image && !stream.packet);
   rav1_hw_stream_close(&stream);
   CHECK(closes == 1);
   video_hw_frame_abandon(&held);
   CHECK(abandons == 1);
   rav1_set_hw_backend(&factory);
}

static void test_discard_and_cancel(void)
{
   video_hw_frame_t frame;
   unsigned before;
   make_frame(1);
   expected = packet;
   CHECK(rav1_hw_stream_init(&stream, &sequence));
   CHECK(!rav1_hw_stream_begin(&stream, packet, packet_size));
   before = exports;
   output_busy = 1;
   CHECK(rav1_hw_stream_step(&stream, &frame, false) == RAV1_HW_STREAM_BUSY);
   CHECK(stream.submitted);
   CHECK(rav1_hw_stream_step(&stream, &frame, true) == RAV1_HW_STREAM_DISCARDED);
   CHECK(exports == before && !frame.image && stream.session.output_slot < 0);
   CHECK(rav1_hw_stream_step(&stream, &frame, true) == RAV1_HW_STREAM_END);
   CHECK(!rav1_hw_stream_begin(&stream, packet, packet_size));
   decode_busy = 1;
   CHECK(rav1_hw_stream_step(&stream, &frame, false) == RAV1_HW_STREAM_BUSY);
   CHECK(stream.session.phase);
   rav1_hw_stream_close(&stream);
   CHECK(!stream.packet && !stream.session.packet);
   decode_busy = output_busy = 0;
}

static void test_sequences(void)
{
   static const uint8_t skipped[] = {0x2a, 1, 0, 0x7a, 0};
   video_hw_frame_t frame;
   unsigned before = opens;
   CHECK(rav1_hw_stream_init(&stream, &sequence));
   CHECK(opens == before + 1);
   CHECK(!rav1_hw_stream_begin(&stream, sequence_obu, sizeof(sequence_obu)));
   CHECK(rav1_hw_stream_step(&stream, &frame, false) ==
         RAV1_HW_STREAM_PROGRESS);
   CHECK(opens == before + 1 && !frame.image);
   CHECK(rav1_hw_stream_step(&stream, &frame, false) == RAV1_HW_STREAM_END);
   CHECK(!rav1_hw_stream_begin(&stream, skipped, sizeof(skipped)));
   CHECK(rav1_hw_stream_step(&stream, &frame, false) ==
         RAV1_HW_STREAM_PROGRESS);
   CHECK(rav1_hw_stream_step(&stream, &frame, false) ==
         RAV1_HW_STREAM_PROGRESS);
   CHECK(rav1_hw_stream_step(&stream, &frame, false) == RAV1_HW_STREAM_END);
   memcpy(unit, sequence_obu, sizeof(sequence_obu));
   unit[6] ^= 1;
   CHECK(!rav1_hw_stream_begin(&stream, unit, sizeof(sequence_obu)));
   CHECK(rav1_hw_stream_step(&stream, &frame, false) == -2);
   CHECK(opens == before + 1 && !stream.packet);
   rav1_hw_stream_close(&stream);
}

static void test_initial_sequence(void)
{
   video_hw_frame_t frame;
   rav1_sequence_t config = sequence;
   unsigned before = opens, closed = closes;
   config.enable_filter_intra ^= 1;
   CHECK(rav1_hw_stream_init(&stream, &config));
   CHECK(!rav1_hw_stream_begin(&stream, sequence_obu, sizeof(sequence_obu)));
   CHECK(rav1_hw_stream_step(&stream, &frame, false) ==
         RAV1_HW_STREAM_PROGRESS);
   CHECK(opens == before + 2 && closes == closed + 1);
   CHECK(!memcmp(&stream.session.sequence, &sequence, sizeof(sequence)));
   CHECK(rav1_hw_stream_step(&stream, &frame, false) == RAV1_HW_STREAM_END);
   make_frame(1);
   expected = packet;
   CHECK(!rav1_hw_stream_begin(&stream, packet, packet_size));
   CHECK(rav1_hw_stream_step(&stream, &frame, false) == RAV1_HW_STREAM_FRAME);
   video_hw_frame_abandon(&frame);
   stream.sequence = config;
   CHECK(!rav1_hw_stream_open_sequence(&stream));
   CHECK(opens == before + 2 && closes == closed + 1);
   rav1_hw_stream_close(&stream);

   config = sequence;
   config.color_range ^= 1;
   CHECK(rav1_hw_stream_init(&stream, &config));
   before = opens;
   CHECK(!rav1_hw_stream_begin(&stream, sequence_obu, sizeof(sequence_obu)));
   CHECK(rav1_hw_stream_step(&stream, &frame, false) == -2);
   CHECK(opens == before);
   rav1_hw_stream_close(&stream);
}

static void test_decode_failure(void)
{
   video_hw_frame_t frame;
   unsigned before = exports;
   make_frame(1);
   expected = packet;
   CHECK(rav1_hw_stream_init(&stream, &sequence));
   CHECK(!rav1_hw_stream_begin(&stream, packet, packet_size));
   decode_busy = -2;
   CHECK(rav1_hw_stream_step(&stream, &frame, false) == -2);
   CHECK(stream.session.broken && !stream.session.packet && !stream.packet);
   CHECK(!frame.image && exports == before);
   CHECK(rav1_hw_stream_begin(&stream, packet, packet_size) == -2);
   rav1_hw_stream_close(&stream);
   before = closes;
   rav1_hw_stream_close(&stream);
   CHECK(closes == before);
   decode_busy = 0;
}

static void test_errors(void)
{
   static const uint8_t malformed[] = {0x32, 127};
   static const uint8_t layered[] = {0x36, 0x20, 0};
   static const uint8_t split[] = {0x22, 0};
   static const uint8_t bad_delimiter[] = {0x12, 1, 0};
   static const uint8_t *const cases[] = {malformed, layered, split,
                                          bad_delimiter};
   static const size_t sizes[] = {2, 3, 2, 3};
   static const int errors[] = {-1, -2, -2, -1};
   video_hw_frame_t frame;
   unsigned i, before = decodes;
   for (i = 0; i < 4; i++)
   {
      CHECK(rav1_hw_stream_init(&stream, &sequence));
      CHECK(!rav1_hw_stream_begin(&stream, cases[i], sizes[i]));
      CHECK(rav1_hw_stream_step(&stream, &frame, false) == errors[i]);
      CHECK(!stream.packet && !frame.image && decodes == before);
      CHECK(rav1_hw_stream_step(&stream, &frame, false) == errors[i]);
      rav1_hw_stream_close(&stream);
   }
   CHECK(rav1_hw_stream_init(&stream, NULL));
   make_frame(1);
   CHECK(!rav1_hw_stream_begin(&stream, packet, packet_size));
   CHECK(rav1_hw_stream_step(&stream, &frame, false) == -2);
   rav1_hw_stream_close(&stream);
   rav1_set_hw_backend(NULL);
   CHECK(!rav1_hw_stream_init(&stream, &sequence));
   rav1_hw_stream_close(&stream);
}

int main(void)
{
   CHECK(rav1_sequence_parse(&sequence, sequence_obu + 2,
                             sizeof(sequence_obu) - 2));
   factory.user = &factory;
   factory.open = open_backend;
   factory.close = close_backend;
   rav1_set_hw_backend(&factory);
   test_packet();
   test_discard_and_cancel();
   test_sequences();
   test_initial_sequence();
   test_decode_failure();
   test_errors();
   CHECK(opens == closes);
   printf("AV1 packet hardware stream: %u failures\n", failures);
   return failures ? 1 : 0;
}
