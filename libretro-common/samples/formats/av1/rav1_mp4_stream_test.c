/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <formats/rav1_sequence.h>
#include <formats/rmp4.h>
#include <formats/rwebm.h>
#include <formats/rmp4_video.h>
#include <formats/rav1_hw.h>
#include <formats/image_yuv_blit.h>

static unsigned failures;
#define CHECK(x)                                                               \
   do                                                                          \
   {                                                                           \
      if (!(x))                                                                \
      {                                                                        \
         fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #x);             \
         failures++;                                                           \
      }                                                                        \
   } while (0)
#include "rav1_mp4_fixture.h"

static rav1_hw_backend_t factory;
static unsigned opens, closes, decodes, exports, abandons;
static int decode_busy, output_busy;

static void abandon(void *owner, uint64_t done, uint64_t value)
{
   CHECK(owner == &factory && done == 2 && value);
   abandons++;
}

static int decode(void *owner, const uint8_t *data, size_t size,
                  const rav1_sequence_t *seq, const rav1_hw_picture_t *picture)
{
   CHECK(owner == &factory && seq->max_width == 64);
   CHECK(data >= bytes + media_at && data + size <= bytes + used);
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

static rav1_sequence_t admitted_sequence;
static unsigned config_opens;
static bool open_config(void *user, const rav1_sequence_t *seq,
      const rav1_obu_t *obu, const video_color_metadata_t *color,
      unsigned slots, rav1_hw_ops_t *ops)
{
   CHECK(obu && obu->type == 1 && obu->data && obu->payload);
   if (!obu) return false;
   CHECK(rav1_sequence_parse(&admitted_sequence, obu->payload, obu->payload_size));
   CHECK(!memcmp(seq, &admitted_sequence, sizeof(*seq)));
   CHECK(color && color->range == (seq->color_range ? 2u : 1u));
   if (seq->color_primaries != 2) CHECK(color->primaries == seq->color_primaries);
   if (seq->transfer_characteristics != 2) CHECK(color->transfer == seq->transfer_characteristics);
   if (seq->matrix_coefficients != 2) CHECK(color->matrix == seq->matrix_coefficients);
   config_opens++;
   return open_backend(user, seq, slots, ops);
}

static void close_backend(rav1_hw_ops_t *ops)
{
   CHECK(ops->decoder == &factory);
   closes++;
   memset(ops, 0, sizeof(*ops));
}

static void test_stream(void)
{
   rmp4_video_stream_t *s;
   video_hw_frame_t frame;
   unsigned i, w = 0, h = 0, before;
   int duration = 0, frames = 0, loops = 0, ret = 2;
   make_mp4(1);
   CHECK(!rmp4_video_stream_open(bytes, used));
   CHECK(!opens);
   s = rmp4_video_stream_open_hw_avail(
       bytes, used, media_at + sizeof(packet) - 1, NULL, NULL, NULL);
   CHECK(s != NULL);
   if (!s)
      return;
   rmp4_video_stream_get_info(s, &w, &h, &frames, &loops);
   CHECK(w == 64 && h == 64 && frames == 2);
   CHECK(rmp4_video_stream_set_hw(s, 1) && opens == 1);
   CHECK(!rmp4_video_stream_next(s, &duration));
   CHECK(rmp4_video_stream_wait_reason(s) == RMP4_VIDEO_WAIT_INPUT && !decodes);
   rmp4_video_stream_set_avail(s, used);
   decode_busy = 1;
   CHECK(!rmp4_video_stream_next(s, &duration));
   CHECK(rmp4_video_stream_wait_reason(s) == RMP4_VIDEO_WAIT_DECODER);
   CHECK(rmp4_video_stream_consumed(s) <= media_at && decodes == 1);
   decode_busy = 0;
   output_busy = 1;
   CHECK(!rmp4_video_stream_next(s, &duration));
   CHECK(decodes == 2 && !exports);
   output_busy = 0;
   CHECK(rmp4_video_stream_next(s, &duration) != NULL);
   CHECK(duration == 40 && rmp4_video_stream_hw_frame(s, &frame));
   CHECK(frame.image && exports == 1 && decodes == 2);
   CHECK(!rmp4_video_stream_hw_frame(s, &frame));
   for (i = 0; i < 8 && ret == 2; i++)
      ret = rmp4_video_stream_discard(s, &duration);
   CHECK(ret == 1 && duration == 40 && exports == 1 && decodes == 3);
   CHECK(!rmp4_video_stream_hw_frame(s, &frame));
   CHECK(rmp4_video_stream_skip(s, NULL) == -1);
   before = opens;
   rmp4_video_stream_rewind(s);
   CHECK(opens == before && closes == before - 1);
   decode_busy = 1;
   CHECK(!rmp4_video_stream_next(s, NULL));
   before = opens;
   rmp4_video_stream_rewind(s);
   CHECK(opens == before && closes == before - 1);
   decode_busy = 0;
   before = opens;
   rmp4_video_stream_stop_hw(s);
   CHECK(opens == before && closes == before);
   CHECK(!rmp4_video_stream_set_hw(s, 0));
   CHECK(rmp4_video_stream_set_hw(s, 1) && opens == before + 1);
   rav1_set_hw_backend(NULL);
   rmp4_video_stream_close(s);
   video_hw_frame_abandon(&frame);
   CHECK(opens == closes && abandons == 1);
   rav1_set_hw_backend(&factory);
}

static void test_seek(void)
{
   rmp4_video_stream_t *s;
   unsigned before, n;
   int ret = 0;
   int64_t position = -1;
   make_mp4(1);
   s = rmp4_video_stream_open_hw_avail(bytes, used,
         media_at + sizeof(packet) - 1, NULL, NULL, NULL);
   CHECK(s != NULL);
   if (!s)
      return;
   CHECK(rmp4_video_stream_set_hw(s, 1));
   CHECK(rmp4_video_stream_seek_begin_ns(s, 40000000));
   CHECK(rmp4_video_stream_seek_step(s, NULL) == 0);
   before = decodes;
   CHECK(rmp4_video_stream_seek_step(s, NULL) == 0);
   CHECK(decodes == before);
   CHECK(rmp4_video_stream_wait_reason(s) == RMP4_VIDEO_WAIT_INPUT);
   rmp4_video_stream_set_avail(s, used);
   decode_busy = 1;
   for (n = 0; n < 16 &&
         rmp4_video_stream_wait_reason(s) != RMP4_VIDEO_WAIT_DECODER; n++)
      CHECK(rmp4_video_stream_seek_step(s, NULL) == 0);
   CHECK(decodes == before + 1);
   CHECK(rmp4_video_stream_wait_reason(s) == RMP4_VIDEO_WAIT_DECODER);
   decode_busy = 0;
   before = exports;
   for (n = 0; n < 16 && !ret; n++)
      ret = rmp4_video_stream_seek_step(s, &position);
   CHECK(ret == 1);
   CHECK(position == 40000 && exports == before);
   rmp4_video_stream_close(s);
   CHECK(opens == closes);
}

static void test_color(void)
{
   rmp4_video_stream_t *s;
   video_hw_frame_t frame;
   unsigned i, transfer, before = opens;
   const uint32_t *marker = NULL;
   for (transfer = 16; transfer <= 18; transfer += 2)
   {
      make_mp4(1);
      bytes[color_at + 1] = 9;
      bytes[color_at + 3] = (uint8_t)transfer;
      bytes[color_at + 5] = 9;
      s = rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL);
      CHECK(s != NULL);
      if (!s)
         continue;
      CHECK(rmp4_video_stream_set_hw(s, 1));
      marker = NULL;
      memset(&frame, 0, sizeof(frame));
      for (i = 0; i < 8 && !marker; i++)
         marker = rmp4_video_stream_next(s, NULL);
      CHECK(marker && rmp4_video_stream_hw_frame(s, &frame));
      CHECK(frame.color.primaries == 9 && frame.color.matrix == 9
            && frame.color.transfer == transfer && frame.color.range == 1);
      video_hw_frame_abandon(&frame);
      rmp4_video_stream_close(s);
   }
   CHECK(opens == before + 2);
   before = opens;
   make_mp4(1);
   bytes[color_at + 6] = 128;
   s = rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL);
   CHECK(s != NULL);
   if (s)
   {
      CHECK(!rmp4_video_stream_set_hw(s, 1));
      rmp4_video_stream_close(s);
   }
   CHECK(opens == before);
   marker = NULL;
   make_mp4(1);
   bytes[color_at + 5] = 1;
   s = rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL);
   CHECK(s != NULL);
   if (!s)
      return;
   CHECK(rmp4_video_stream_set_hw(s, 1));
   for (i = 0; i < 8 && !marker; i++)
      marker = rmp4_video_stream_next(s, NULL);
   CHECK(marker && rmp4_video_stream_hw_frame(s, &frame));
   if (marker)
   {
      CHECK(frame.yuv & IMAGE_YUV_FLAG_BT709);
      CHECK(frame.color.max_cll == 1024 && frame.color.max_fall == 256
            && frame.color.mastering.present == VIDEO_MASTERING_DISPLAY_COMPLETE
            && frame.color.mastering.chromaticity[0] == 25000
            && frame.color.mastering.luminance_max == 10240000);
      CHECK(frame.color.primaries == 1 && frame.color.matrix == 1
            && frame.color.transfer == 2 && frame.color.range == 1);
      video_hw_frame_abandon(&frame);
   }
   rmp4_video_stream_close(s);
}

static void test_early_hdr_seek(void)
{
   rmp4_video_stream_t *s;
   video_hw_frame_t frame = {0};
   unsigned n, before = decodes;
   int ret = 0;
   make_mp4_clip(1, 1);
   s = rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL);
   CHECK(s && rmp4_video_stream_set_hw(s, 1));
   if (!s) return;
   CHECK(rmp4_video_stream_seek_begin_ns(s, 40000000));
   CHECK(rmp4_video_stream_seek_step(s, NULL) == 0 && decodes == before);
   CHECK(rmp4_video_stream_seek_step(s, NULL) == 0 && decodes == before);
   CHECK(rmp4_video_stream_wait_reason(s) == RMP4_VIDEO_WAIT_NONE);
   CHECK(rmp4_video_stream_consumed(s) <= media_at);
   rmp4_video_stream_set_avail(s, media_at);
   CHECK(rmp4_video_stream_seek_step(s, NULL) == 0 && decodes == before);
   CHECK(rmp4_video_stream_wait_reason(s) == RMP4_VIDEO_WAIT_INPUT);
   CHECK(rmp4_video_stream_seek_begin_ns(s, 0));
   CHECK(rmp4_video_stream_seek_begin_ns(s, 40000000));
   rmp4_video_stream_set_avail(s, used);
   for (n = 0; n < 128 && ret == 0; n++)
      ret = rmp4_video_stream_seek_step(s, NULL);
   CHECK(ret == 1 && decodes == before);
   CHECK(rmp4_video_stream_next(s, NULL));
   CHECK(rmp4_video_stream_hw_frame(s, &frame));
   CHECK(frame.color.max_cll == 1000 && frame.color.max_fall == 400);
   video_hw_frame_abandon(&frame);
   rmp4_video_stream_close(s);
   CHECK(opens == closes);
}

static void test_changed_hdr_seek(void)
{
   rmp4_video_stream_t *s;
   video_hw_frame_t frame = {0};
   unsigned n, seek, before;
   const int64_t positions[4] = {0, 80000000, 40000000, 80000000};
   int ret = 0;
   make_mp4_clip(1, 2);
   s = rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL);
   CHECK(s && rmp4_video_stream_set_hw(s, 1));
   if (!s) return;
   for (n = 0; n < 3; n++)
   {
      CHECK(rmp4_video_stream_next(s, NULL));
      CHECK(rmp4_video_stream_hw_frame(s, &frame));
      CHECK(frame.color.max_cll == (n ? 2000 : 1000));
      CHECK(frame.color.max_fall == (n ? 600 : 400));
      video_hw_frame_abandon(&frame);
   }
   before = decodes;
   CHECK(rmp4_video_stream_seek_begin_ns(s, 80000000));
   ret = 0;
   for (n = 0; n < 128 && !ret; n++) ret = rmp4_video_stream_seek_step(s, NULL);
   CHECK(ret == 1 && n <= 4 && decodes == before);
   rmp4_video_stream_close(s);
   s = rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL);
   ret = 0;
   CHECK(s && rmp4_video_stream_set_hw(s, 1));
   if (!s) return;
   CHECK(rmp4_video_stream_seek_begin_ns(s, 80000000));
   for (n = 0; n < 128 && ret == 0; n++)
      ret = rmp4_video_stream_seek_step(s, NULL);
   CHECK(ret == 1);
   CHECK(rmp4_video_stream_next(s, NULL));
   CHECK(rmp4_video_stream_hw_frame(s, &frame));
   CHECK(frame.color.max_cll == 2000 && frame.color.max_fall == 600);
   video_hw_frame_abandon(&frame);
   for (seek = 0; seek < 4; seek++)
   {
      before = decodes;
      ret = 0;
      CHECK(rmp4_video_stream_seek_begin_ns(s, positions[seek]));
      for (n = 0; n < 8 && !ret; n++)
         ret = rmp4_video_stream_seek_step(s, NULL);
      CHECK(ret == 1 && n <= 4 && decodes == before);
      CHECK(rmp4_video_stream_next(s, NULL));
      CHECK(rmp4_video_stream_hw_frame(s, &frame));
      CHECK(frame.color.max_cll == (seek ? 2000 : 1000));
      CHECK(frame.color.max_fall == (seek ? 600 : 400));
      video_hw_frame_abandon(&frame);
   }
   rmp4_video_stream_close(s);
   s = rmp4_video_stream_open_hw_avail(bytes, used,
         media_at + sizeof(packet) + 8 + 40 * 4, NULL, NULL, NULL);
   CHECK(s && rmp4_video_stream_set_hw(s, 1));
   if (!s) return;
   before = decodes;
   CHECK(rmp4_video_stream_seek_begin_ns(s, 80000000));
   for (n = 0; n < 128 &&
         rmp4_video_stream_wait_reason(s) != RMP4_VIDEO_WAIT_INPUT; n++)
      CHECK(rmp4_video_stream_seek_step(s, NULL) == 0);
   CHECK(n < 128 && decodes == before);
   CHECK(rmp4_video_stream_seek_begin_ns(s, 0));
   ret = 0;
   for (n = 0; n < 8 && !ret; n++) ret = rmp4_video_stream_seek_step(s, NULL);
   CHECK(ret == 1 && decodes == before);
   CHECK(rmp4_video_stream_next(s, NULL));
   CHECK(rmp4_video_stream_hw_frame(s, &frame) && frame.color.max_cll == 1000);
   video_hw_frame_abandon(&frame);
   rmp4_video_stream_set_avail(s, used);
   CHECK(rmp4_video_stream_seek_begin_ns(s, 80000000));
   ret = 0;
   for (n = 0; n < 128 && !ret; n++) ret = rmp4_video_stream_seek_step(s, NULL);
   CHECK(ret == 1);
   CHECK(rmp4_video_stream_next(s, NULL));
   CHECK(rmp4_video_stream_hw_frame(s, &frame));
   CHECK(frame.color.max_cll == 2000 && frame.color.max_fall == 600);
   video_hw_frame_abandon(&frame);
   rmp4_video_stream_close(s);
   CHECK(opens == closes);
}

static void test_changed_bad_hdr(void)
{
   rmp4_video_stream_t *s;
   size_t second;
   unsigned n, attempt, before = decodes;
   int ret;
   make_mp4_clip(1, 2);
   second = media_at + sizeof(packet) + 8 + 40 * 4;
   bytes[second + 7] = 0;
   s = rmp4_video_stream_open_hw_avail(bytes, used, second, NULL, NULL, NULL);
   CHECK(s && rmp4_video_stream_set_hw(s, 1));
   if (!s) return;
   CHECK(rmp4_video_stream_seek_begin_ns(s, 80000000));
   for (n = 0; n < 128 &&
         rmp4_video_stream_wait_reason(s) != RMP4_VIDEO_WAIT_INPUT; n++)
      CHECK(rmp4_video_stream_seek_step(s, NULL) == 0);
   CHECK(n < 128 && decodes == before);
   rmp4_video_stream_set_avail(s, used);
   for (attempt = 0; attempt < 2; attempt++)
   {
      if (attempt)
      {
         rmp4_video_stream_rewind(s);
         CHECK(rmp4_video_stream_seek_begin_ns(s, 80000000));
      }
      ret = 0;
      for (n = 0; n < 128 && !ret; n++) ret = rmp4_video_stream_seek_step(s, NULL);
      CHECK(ret == -1 && decodes == before);
      CHECK(!rmp4_video_stream_next(s, NULL));
   }
   rmp4_video_stream_close(s);
   CHECK(opens == closes);
}

static void test_early_bad_hdr(void)
{
   rmp4_video_stream_t *s;
   unsigned n, before = decodes;
   int ret = 0;
   make_mp4_clip(1, 1);
   bytes[media_at + 40 * 4 + 7] = 0;
   s = rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL);
   CHECK(s && rmp4_video_stream_set_hw(s, 1));
   if (!s) return;
   CHECK(rmp4_video_stream_seek_begin_ns(s, 40000000));
   for (n = 0; n < 128 && !ret; n++)
      ret = rmp4_video_stream_seek_step(s, NULL);
   CHECK(ret == -1 && decodes == before);
   CHECK(!rmp4_video_stream_next(s, NULL));
   rmp4_video_stream_close(s);
   CHECK(opens == closes);
}

static void test_probe_frontier(void)
{
   rmp4_video_stream_t *s;
   unsigned before = decodes;
   make_mp4_clip(1, 1);
   bytes[media_at] = 128;
   s = rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL);
   CHECK(s && rmp4_video_stream_set_hw(s, 1));
   if (!s) return;
   CHECK(rmp4_video_stream_seek_begin_ns(s, 40000000));
   CHECK(rmp4_video_stream_seek_step(s, NULL) == 0);
   CHECK(rmp4_video_stream_consumed(s) == media_at && decodes == before);
   CHECK(rmp4_video_stream_seek_step(s, NULL) == -1 && decodes == before);
   rmp4_video_stream_close(s);
   CHECK(opens == closes);
}

static void test_sparse_hdr_seek(void)
{
   rmp4_video_stream_t *s;
   video_hw_frame_t frame = {0};
   size_t lo, hi, second, last, i, previous = 0, skipped = 0;
   rmp4_t *demux;
   unsigned n, spans = 0, before;
   int ret = 0;
   make_mp4_clip(1, 2);
   demux = rmp4_open_memory(bytes, used);
   CHECK(demux && rmp4_track_sample_span(demux, 0, 1, &second, &last));
   if (!demux) return;
   rmp4_close(demux);
   memmove(bytes + second + 515, bytes + second, used - second);
   bytes[second] = 0x7a;
   bytes[second + 1] = 0x80;
   bytes[second + 2] = 4;
   memset(bytes + second + 3, 0xcc, 512);
   used += 515;
   patch32(media_at - 8, (uint32_t)(used - media_at + 8));
   for (i = 4; i + 24 <= media_at; i++)
      if (!memcmp(bytes + i, "stsz", 4))
      {
         patch32(i + 20, (uint32_t)(last - second + 515));
         break;
      }
   CHECK(i + 24 <= media_at);
   s = rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL);
   CHECK(s && rmp4_video_stream_set_hw(s, 1));
   if (!s) return;
   CHECK(rmp4_video_stream_next(s, NULL));
   CHECK(rmp4_video_stream_hw_frame(s, &frame));
   video_hw_frame_abandon(&frame);
   before = decodes;
   CHECK(rmp4_video_stream_seek_begin_ns(s, 80000000));
   for (n = 0; n < 128 && !ret; n++)
   {
      if (rmp4_video_stream_seek_span(s, &lo, &hi) && hi > lo)
      {
         CHECK(lo <= hi && hi <= used && hi - lo <= 10);
         if (previous && lo > previous) skipped += lo - previous;
         previous = hi;
         CHECK(rmp4_video_stream_consumed(s) == lo);
         rmp4_video_stream_set_avail(s, hi ? hi - 1 : 0);
         CHECK(rmp4_video_stream_seek_step(s, NULL) == 0);
         CHECK(rmp4_video_stream_wait_reason(s) == RMP4_VIDEO_WAIT_INPUT);
         CHECK(decodes == before);
         rmp4_video_stream_set_avail(s, hi);
         spans++;
      }
      else
         rmp4_video_stream_set_avail(s, used);
      ret = rmp4_video_stream_seek_step(s, NULL);
   }
   CHECK(ret == 1 && spans > 0 && skipped >= 500);
   CHECK(rmp4_video_stream_next(s, NULL));
   CHECK(rmp4_video_stream_hw_frame(s, &frame));
   CHECK(frame.color.max_cll == 2000 && frame.color.max_fall == 600);
   video_hw_frame_abandon(&frame);
   rmp4_video_stream_close(s);
   CHECK(opens == closes);
}

static void test_unavailable(void)
{
   rmp4_video_stream_t *s;
   unsigned before = opens;
   make_mp4(1);
   rav1_set_hw_backend(NULL);
   s = rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL);
   CHECK(s != NULL);
   if (s)
   {
      CHECK(!rmp4_video_stream_set_hw(s, 1));
      CHECK(!rmp4_video_stream_next(s, NULL));

      rmp4_video_stream_close(s);
   }
   rav1_set_hw_backend(&factory);
   bytes[config_at] = 0;
   s = rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL);
   if (s)
   {
      CHECK(!rmp4_video_stream_set_hw(s, 1));
      rmp4_video_stream_close(s);
   }
   CHECK(opens == before);
}

#include "rav1_sequence_cache_test.h"

int main(int argc, char **argv)
{
   factory.user = &factory;
   factory.open = open_backend;
   factory.close = close_backend;
   if (argc == 2 && !strcmp(argv[1], "--config"))
      factory.open_config = open_config;
   rav1_set_hw_backend(&factory);
   test_stream();
   test_seek();
   test_color();
   test_early_hdr_seek();
   test_changed_hdr_seek();
   test_changed_bad_hdr();
   test_early_bad_hdr();
   test_probe_frontier();
   test_sparse_hdr_seek();
   test_unavailable();
   rav1_set_hw_backend(NULL);
   rav1_set_hw_backend(&factory);
   if (factory.open_config)
   {
      test_sequence_reopen(0);
   }
   rav1_set_hw_backend(NULL);
   if (factory.open_config) CHECK(config_opens);
   printf("AV1 MP4 hardware reader: %u failures\n", failures);
   return failures ? 1 : 0;
}
