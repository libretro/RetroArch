/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <formats/rav1_hw.h>

static rav1_hw_session_t session;
static rav1_sequence_t sequence;
static rav1_reference_state_t saved;
static rav1_hw_picture_t snapshot;
static video_hw_frame_t output, zero, held;
static uint8_t packet[128], different[128];
static unsigned position, failures, decode_calls, accepted, exports, abandons;
static unsigned widths[32], heights[32];
static uint8_t saved_hints[32][8];
static rav1_grain_t saved_grains[32];
static int decode_result, output_result, bad_output, verify_retry;
static size_t packet_size;

#define CHECK(x)                                                               \
   do                                                                          \
   {                                                                           \
      if (!(x))                                                                \
      {                                                                        \
         fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #x);             \
         failures++;                                                           \
      }                                                                        \
   } while (0)

static void abandon(void *owner, uint64_t done, uint64_t value)
{
   CHECK(owner == &session && done == 2 && value);
   abandons++;
}

static int decode(void *owner, const uint8_t *bytes, size_t size,
                  const rav1_sequence_t *seq, const rav1_hw_picture_t *picture)
{
   rav1_tile_group_t group = picture->tile_group;
   rav1_tile_t tile;
   unsigned i, j, count = 0;
   int ret;
   CHECK(owner == &session && seq == &session.sequence);
   CHECK(bytes == session.packet && size == session.packet_size);
   CHECK(group.data >= bytes && group.data + group.size <= bytes + size);
   CHECK(picture->slot >= 0 && picture->slot < (int)session.slots);
   for (i = 0; i < 8; i++)
   {
      CHECK(picture->reference_slots[i] != picture->slot);
      if (picture->reference_slots[i] >= 0)
      {
         CHECK(picture->reference_types[i] == session.state.type[i]);
         CHECK(picture->reference_info[i].order_hint ==
               session.state.refs[i].order_hint);
         for (j = 0; j < 8; j++)
            CHECK(picture->reference_order_hints[i][j] ==
                  saved_hints[picture->reference_slots[i]][j]);
      }
      CHECK(picture->reference_info[i].valid ==
            (unsigned)(picture->reference_slots[i] >= 0));
   }
   while ((ret = rav1_tile_group_next(&tile, &group)) == 1)
   {
      CHECK(tile.data >= bytes && tile.data + tile.size <= bytes + size &&
            tile.size);
      count++;
   }
   CHECK(ret == 0 &&
         count == picture->tiles.layout.columns * picture->tiles.layout.rows);
   if (verify_retry)
      CHECK(!memcmp(&snapshot, picture, sizeof(snapshot)));
   if (decode_result > 0)
   {
      snapshot = *picture;
      verify_retry = 1;
   }
   decode_calls++;
   if (decode_result)
      return decode_result;
   widths[picture->slot] = picture->setup.size.upscaled_width;
   heights[picture->slot] = picture->setup.size.height;
   saved_grains[picture->slot] = picture->grain.params;
   saved_hints[picture->slot][0] = 0;
   for (j = 1; j < 8; j++)
   {
      unsigned expected =
          picture->header.frame_is_intra
              ? 0
              : session.state.refs[picture->setup.ref_frame_idx[j - 1]]
                    .order_hint;
      CHECK(picture->order_hints[j] == expected);
      saved_hints[picture->slot][j] = (uint8_t)expected;
   }
   accepted++;
   verify_retry = 0;
   return 0;
}

static int export_frame(void *owner, unsigned slot, const rav1_grain_t *grain,
                        video_hw_frame_t *frame)
{
   CHECK(owner == &session && slot < session.slots &&
         grain == &session.output_grain);
   CHECK(!memcmp(grain, &saved_grains[slot], sizeof(*grain)));
   if (output_result)
      return output_result;
   frame->image = ++exports + 1000;
   frame->width = widths[slot] + bad_output;
   frame->height = heights[slot];
   frame->pixfmt =
       sequence.bit_depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
   frame->wait = 1;
   frame->wait_value = accepted;
   frame->done = 2;
   frame->done_value = exports;
   frame->owner = &session;
   frame->abandon = abandon;
   return 0;
}

static bool initialize(unsigned slots)
{
   rav1_hw_ops_t ops;
   ops.decoder = &session;
   ops.decode = decode;
   ops.output = export_frame;
   return rav1_hw_session_init(&session, &sequence, &ops, slots);
}

static void reset(void)
{
   memset(&sequence, 0, sizeof(sequence));
   sequence.operating_point_count = 1;
   sequence.bit_depth = 8;
   sequence.subsampling_x = sequence.subsampling_y = 1;
   sequence.max_width = sequence.max_height = 64;
   sequence.frame_width_bits = sequence.frame_height_bits = 6;
   memset(saved_hints, 0, sizeof(saved_hints));
   memset(saved_grains, 0, sizeof(saved_grains));
   decode_calls = accepted = exports = abandons = 0;
   decode_result = output_result = bad_output = verify_retry = 0;
   CHECK(initialize(9));
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

static void make_packet(unsigned inter, unsigned show, unsigned refresh)
{
   unsigned i;
   memset(packet, 0, sizeof(packet));
   packet[0] = 0x32;
   position = 0;
   put(0, 1);
   put(inter ? 1 : 0, 2);
   put(show, 1);
   if (!show)
      put(1, 1);
   if (inter || !show)
      put(1, 1); /* error resilient */
   put(1, 1);    /* disable CDF */
   put(0, 1);    /* size override */
   if (inter || !show)
      put(refresh, 8);
   if (inter)
      for (i = 0; i < 7; i++)
         put(0, 3);
   put(0, 1); /* render size */
   if (inter)
   {
      put(0, 1); /* high precision */
      put(1, 1); /* switchable interpolation */
      put(0, 1); /* motion mode */
   }
   put(1, 1); /* uniform tiles */
   put(0, 8); /* lossless quantizer */
   put(0, 5); /* quant deltas, matrix, segmentation */
   if (inter)
      put(0, 1); /* reference select */
   put(0, 1);    /* reduced TX */
   if (inter)
      put(0, 7); /* identity global models */
   while (position % 8)
      put(0, 1);
   put(128, 8);
   packet_size = position / 8 + 2;
   packet[1] = (uint8_t)(packet_size - 2);
}

static void test_busy_and_output(void)
{
   unsigned i;
   reset();
   make_packet(0, 1, 255);
   saved = session.state;
   decode_result = 1;
   CHECK(rav1_hw_session_submit(&session, packet, packet_size) == 1);
   CHECK(session.phase && !memcmp(&saved, &session.state, sizeof(saved)));
   memcpy(different, packet, packet_size);
   CHECK(rav1_hw_session_submit(&session, different, packet_size) == -1);
   CHECK(decode_calls == 1);
   decode_result = 0;
   CHECK(rav1_hw_session_submit(&session, packet, packet_size) == 0);
   CHECK(decode_calls == 2 && accepted == 1 && !session.packet &&
         !session.phase);
   memset(packet, 0, packet_size); /* accepted input is no longer read */
   CHECK(rav1_hw_session_submit(&session, packet, packet_size) == 1);
   output_result = 1;
   memset(&output, 255, sizeof(output));
   CHECK(rav1_hw_session_output(&session, &output) == 1);
   CHECK(!memcmp(&output, &zero, sizeof(output)) && session.output_slot >= 0);
   output_result = 0;
   CHECK(rav1_hw_session_output(&session, &held) == 0);
   CHECK(held.image && session.output_slot < 0);
   for (i = 0; i < 24; i++)
   {
      make_packet(1, 1, 1u << (i % 8));
      CHECK(rav1_hw_session_submit(&session, packet, packet_size) == 0);
      CHECK(rav1_hw_session_output(&session, &output) == 0);
      CHECK(output.image != held.image);
      video_hw_frame_abandon(&output);
   }
   CHECK(held.image == 1001 && abandons == 24);
   video_hw_frame_abandon(&held);
   CHECK(abandons == 25);
}

static void test_replay_and_cancel(void)
{
   static const uint8_t replay[] = {0x1a, 1, 0x88};
   reset();
   make_packet(0, 0, 255);
   saved = session.state;
   decode_result = 1;
   CHECK(rav1_hw_session_submit(&session, packet, packet_size) == 1);
   CHECK(rav1_hw_session_cancel(&session));
   CHECK(!rav1_hw_session_cancel(&session));
   CHECK(!memcmp(&saved, &session.state, sizeof(saved)));
   decode_result = verify_retry = 0;
   CHECK(rav1_hw_session_submit(&session, packet, packet_size) == 0);
   CHECK(rav1_hw_session_output(&session, &output) == 1);
   CHECK(rav1_hw_session_submit(&session, replay, sizeof(replay)) == 0);
   CHECK(accepted == 1 && decode_calls == 2);
   CHECK(rav1_hw_session_output(&session, &output) == 0);
   video_hw_frame_abandon(&output);
   CHECK(rav1_hw_session_submit(&session, replay, sizeof(replay)) == -1);
   CHECK(accepted == 1);
   make_packet(1, 1, 1);
   CHECK(rav1_hw_session_submit(&session, packet, packet_size) == 0);
   CHECK(rav1_hw_session_discard_output(&session));
   CHECK(!rav1_hw_session_discard_output(&session));
   CHECK(exports == 1);
}

static void test_unsized(void)
{
   static const uint8_t replay[] = {0x18, 0x88};
   size_t i;
   reset();
   make_packet(0, 0, 255);
   packet_size--;
   memmove(packet + 1, packet + 2, packet_size - 1);
   packet[0] &= ~2;
   saved = session.state;
   for (i = 0; i < packet_size; i++)
   {
      CHECK(rav1_hw_session_submit(&session, packet, i) < 0);
      CHECK(!decode_calls && !memcmp(&saved, &session.state, sizeof(saved)));
   }
   decode_result = 1;
   CHECK(rav1_hw_session_submit(&session, packet, packet_size) == 1);
   CHECK(session.packet == packet && session.packet_size == packet_size);
   memcpy(different, packet, packet_size);
   CHECK(rav1_hw_session_submit(&session, different, packet_size) == -1);
   CHECK(rav1_hw_session_submit(&session, packet, packet_size - 1) == -1);
   CHECK(decode_calls == 1);
   decode_result = 0;
   CHECK(rav1_hw_session_submit(&session, packet, packet_size) == 0);
   CHECK(accepted == 1 && session.output_slot < 0);
   CHECK(rav1_hw_session_submit(&session, replay, sizeof(replay)) == 0);
   CHECK(accepted == 1 && decode_calls == 2);
   CHECK(rav1_hw_session_output(&session, &output) == 0);
   video_hw_frame_abandon(&output);
}

static void test_failures(void)
{
   size_t i;
   reset();
   CHECK(!initialize(8));
   CHECK(!initialize(33));
   sequence.bit_depth = 12;
   CHECK(!initialize(9));
   sequence.bit_depth = 8;
   sequence.profile = 1;
   CHECK(!initialize(9));
   sequence.profile = 0;
   sequence.monochrome = 1;
   CHECK(!initialize(9));
   reset();
   make_packet(0, 1, 255);
   saved = session.state;
   for (i = 0; i < packet_size; i++)
   {
      CHECK(rav1_hw_session_submit(&session, packet, i) < 0);
      CHECK(!memcmp(&saved, &session.state, sizeof(saved)) && !decode_calls);
   }
   decode_result = -2;
   CHECK(rav1_hw_session_submit(&session, packet, packet_size) == -2);
   CHECK(session.broken && !session.packet &&
         !memcmp(&saved, &session.state, sizeof(saved)));
   CHECK(rav1_hw_session_submit(&session, packet, packet_size) == -1);
   reset();
   sequence.bit_depth = 10;
   CHECK(initialize(9));
   make_packet(0, 1, 255);
   CHECK(rav1_hw_session_submit(&session, packet, packet_size) == 0);
   CHECK(rav1_hw_session_output(&session, &output) == 0 &&
         output.pixfmt == IMAGE_PIXFMT_P010);
   video_hw_frame_abandon(&output);
   CHECK(rav1_hw_session_submit(&session, packet, packet_size) == 0);
   bad_output = 1;
   CHECK(rav1_hw_session_output(&session, &output) == -1);
   CHECK(session.broken && abandons == 2 &&
         !memcmp(&output, &zero, sizeof(output)));
}

static int inspect(const char *path)
{
   FILE *file = fopen(path, "rb");
   uint8_t *data = NULL;
   size_t offset = 0;
   long size;
   rav1_obu_t obu;
   unsigned count = 0;
   int ret = -1, result = 1;
   bool initialized = false;
   if (!file)
      return 1;
   if (fseek(file, 0, SEEK_END) || (size = ftell(file)) <= 0 ||
       size > 32 * 1024 * 1024 || fseek(file, 0, SEEK_SET))
      goto done;
   data = (uint8_t *)malloc((size_t)size);
   if (!data || fread(data, 1, (size_t)size, file) != (size_t)size)
      goto done;
   reset();
   while ((ret = rav1_obu_next(&obu, data, size, &offset, false)) == 1)
   {
      if (obu.type == 1)
      {
         if (!rav1_sequence_parse(&sequence, obu.payload, obu.payload_size))
            goto done;
         if (!initialized)
         {
            if (!initialize(9))
               goto done;
            initialized = true;
         }
         else if (memcmp(&sequence, &session.sequence, sizeof(sequence)))
            goto done;
      }
      if (obu.type != 3 && obu.type != 6)
         continue;
      if (!initialized)
         goto done;
      saved = session.state;
      decode_result = 1;
      ret = rav1_hw_session_submit(&session, obu.data, obu.size);
      if (ret == 1)
      {
         CHECK(!memcmp(&saved, &session.state, sizeof(saved)));
         decode_result = 0;
         ret = rav1_hw_session_submit(&session, obu.data, obu.size);
      }
      if (ret)
         goto done;
      if (session.output_slot >= 0)
      {
         output_result = 1;
         CHECK(rav1_hw_session_output(&session, &output) == 1 && !output.image);
         output_result = 0;
         if (rav1_hw_session_output(&session, &output))
            goto done;
         video_hw_frame_abandon(&output);
      }
      count++;
   }
   if (!ret && count && !failures)
   {
      printf("HW %u %u %u %u\n", count, accepted, exports, abandons);
      result = 0;
   }
done:
   free(data);
   fclose(file);
   return result;
}

int main(int argc, char **argv)
{
   if (argc == 2)
      return inspect(argv[1]);
   test_busy_and_output();
   test_replay_and_cancel();
   test_failures();
   test_unsized();
   printf("AV1 hardware session: %u failures\n", failures);
   return failures ? 1 : 0;
}
