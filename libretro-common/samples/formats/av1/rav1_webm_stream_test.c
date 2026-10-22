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
#include "rav1_webm_fixture.h"
#include <formats/rwebm_video.h>

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

static void test_stream(int inband)
{
   rwebm_video_stream_t *s;
   video_hw_frame_t frame = {0};
   unsigned before = opens, w = 0, h = 0;
   int duration = 0, frames = 0, loops = 0;
   make_webm(inband, 0, 0, 0);
   CHECK(!rwebm_video_stream_open(bytes, used));
   s = rwebm_video_stream_open_hw_avail(bytes, used, 8, &frames);
   CHECK(!s && frames);
   s = rwebm_video_stream_open_hw_avail(bytes, used, used, NULL);
   CHECK(s != NULL);
   if (!s)
      return;
   rwebm_video_stream_get_info(s, &w, &h, &frames, &loops);
   CHECK(w == 64 && h == 64 && frames == 2);
   CHECK(rwebm_video_stream_set_hw(s, 1) && opens == before + 1);
   rwebm_video_stream_set_avail(s, media_at);
   CHECK(!rwebm_video_stream_next(s, NULL) && rwebm_video_stream_waiting(s));
   rwebm_video_stream_set_avail(s, used);
   decode_busy = 1;
   CHECK(!rwebm_video_stream_next(s, NULL) && rwebm_video_stream_waiting(s));
   CHECK(rwebm_video_stream_consumed(s) <= media_at);
   rwebm_video_stream_set_avail(s, media_at);
   decode_busy = 0;
   CHECK(!rwebm_video_stream_next(s, NULL) && rwebm_video_stream_waiting(s));
   rwebm_video_stream_set_avail(s, used);
   output_busy = 1;
   CHECK(!rwebm_video_stream_next(s, NULL) && rwebm_video_stream_waiting(s));
   output_busy = 0;
   CHECK(rwebm_video_stream_next(s, &duration) != NULL);
   CHECK(duration == 40 && rwebm_video_stream_hw_frame(s, &frame));
   CHECK(!rwebm_video_stream_hw_frame(s, &frame));
   before = exports;
   CHECK(rwebm_video_stream_skip(s, &duration) == 1 && duration == 40);
   CHECK(exports == before && !rwebm_video_stream_hw_frame(s, &frame));
   CHECK(!rwebm_video_stream_next(s, NULL) && !rwebm_video_stream_waiting(s));
   rwebm_video_stream_rewind(s);
   decode_busy = 1;
   CHECK(!rwebm_video_stream_next(s, NULL));
   rwebm_video_stream_rewind(s);
   decode_busy = 0;
   CHECK(rwebm_video_stream_next(s, NULL) != NULL);
   before = abandons;
   rwebm_video_stream_close(s);
   CHECK(abandons == before + 1);
   video_hw_frame_abandon(&frame);
   CHECK(opens == closes);
}

static void test_seek(int inband)
{
   rwebm_video_stream_t *s;
   video_hw_frame_t frame = {0};
   unsigned before, n;
   int ret = 0;
   int64_t position = -1;
   make_webm(inband, 0, 0, 0);
   s = rwebm_video_stream_open_hw_avail(bytes, used, used, NULL);
   CHECK(s && rwebm_video_stream_set_hw(s, 1));
   if (!s) return;
   CHECK(rwebm_video_stream_seekable(s));
   if (!rwebm_video_stream_seekable(s)) goto done;
   CHECK(rwebm_video_stream_seek_begin_ns(s, 40000000));
   before = exports;
   decode_busy = 1;
   for (n = 0; n < 8 && !rwebm_video_stream_waiting(s); n++)
      CHECK(rwebm_video_stream_seek_step(s, NULL) == 0);
   CHECK(rwebm_video_stream_waiting(s));
   n = decodes;
   CHECK(rwebm_video_stream_seek_step(s, NULL) == 0 && decodes == n + 1);
   rwebm_video_stream_set_avail(s, media_at);
   decode_busy = 0;
   n = decodes;
   CHECK(rwebm_video_stream_seek_step(s, NULL) == 0 && decodes == n);
   rwebm_video_stream_set_avail(s, used);
   for (n = 0; n < 128 && ret == 0; n++)
      ret = rwebm_video_stream_seek_step(s, &position);
   CHECK(ret == 1 && position == 40000 && exports == before);
   CHECK(rwebm_video_stream_next(s, NULL));
   CHECK(rwebm_video_stream_hw_frame(s, &frame));
   CHECK(rwebm_video_stream_frame_time_ns(s) == 40000000);
   video_hw_frame_abandon(&frame);
   /* A new seek cancels a borrowed, busy preroll packet. */
   CHECK(rwebm_video_stream_seek_begin_ns(s, 40000000));
   decode_busy = 1;
   for (n = 0; n < 8 && !rwebm_video_stream_waiting(s); n++)
      CHECK(rwebm_video_stream_seek_step(s, NULL) == 0);
   CHECK(rwebm_video_stream_seek_begin_ns(s, -1));
   decode_busy = 0;
   ret = 0;
   for (n = 0; n < 128 && ret == 0; n++)
      ret = rwebm_video_stream_seek_step(s, &position);
   CHECK(ret == 1 && position == 0);
   CHECK(rwebm_video_stream_next(s, NULL));
   CHECK(rwebm_video_stream_hw_frame(s, &frame));
   CHECK(rwebm_video_stream_frame_time_ns(s) == 0);
   video_hw_frame_abandon(&frame);
   CHECK(rwebm_video_stream_seek_begin_ns(s, 1000000000));
   ret = 0;
   for (n = 0; n < 128 && ret == 0; n++)
      ret = rwebm_video_stream_seek_step(s, &position);
   CHECK(ret == 1 && position == 40000);
done:
   decode_busy = 0;
   rwebm_video_stream_close(s);
   CHECK(opens == closes);
}

static void test_seek_partial(int inband)
{
   rwebm_video_stream_t *s;
   video_hw_frame_t frame = {0};
   unsigned n, before = exports;
   int ret = 0;
   int64_t position = -1;
   make_webm_frames(inband, 0, 0, 0, 4);
   s = rwebm_video_stream_open_hw_avail(bytes, used, used - 1, NULL);
   CHECK(s && rwebm_video_stream_set_hw(s, 1));
   if (!s) return;
   CHECK(rwebm_video_stream_seek_begin_ns(s, 1000000000));
   for (n = 0; n < 128 && !rwebm_video_stream_waiting(s); n++)
      CHECK(rwebm_video_stream_seek_step(s, NULL) == 0);
   CHECK(rwebm_video_stream_waiting(s) && exports == before);
   rwebm_video_stream_set_avail(s, used);
   for (n = 0; n < 128 && ret == 0; n++)
      ret = rwebm_video_stream_seek_step(s, &position);
   CHECK(ret == 1 && position == 120000 && exports == before);
   CHECK(rwebm_video_stream_next(s, NULL));
   CHECK(rwebm_video_stream_hw_frame(s, &frame));
   CHECK(rwebm_video_stream_frame_time_ns(s) == 120000000);
   video_hw_frame_abandon(&frame);
   rwebm_video_stream_close(s);
   CHECK(opens == closes);
}

static void test_seek_early_hdr(int inband)
{
   rwebm_video_stream_t *s;
   video_hw_frame_t frame = {0};
   unsigned n, before = decodes;
   int ret = 0;
   int64_t position = -1;
   make_webm_clip(inband, 0, 0, 0, 4, 1, 1);
   s = rwebm_video_stream_open_hw_avail(bytes, used, used, NULL);
   CHECK(s && rwebm_video_stream_set_hw(s, 1));
   if (!s) return;
   CHECK(rwebm_video_stream_seek_begin_ns(s, 80000000));
   for (n = 0; n < 128 && ret == 0; n++)
      ret = rwebm_video_stream_seek_step(s, &position);
   CHECK(ret == 1 && position == 80000 && decodes == before);
   CHECK(rwebm_video_stream_next(s, NULL));
   CHECK(rwebm_video_stream_hw_frame(s, &frame) && decodes == before + 1);
   CHECK(frame.color.max_cll == 1000 && frame.color.max_fall == 400);
   video_hw_frame_abandon(&frame);
   rwebm_video_stream_close(s);
   CHECK(opens == closes);
}

static void test_seek_changed_hdr(int inband)
{
   rwebm_video_stream_t *s;
   video_hw_frame_t frame = {0};
   unsigned n, seek;
   const int64_t positions[3] = {0, 360000000, 80000000};
   int ret = 0;
   make_webm_clip(inband, 0, 0, 0, 12, 1, 2);
   s = rwebm_video_stream_open_hw_avail(bytes, used, used, NULL);
   CHECK(s && rwebm_video_stream_set_hw(s, 1));
   if (!s) return;
   CHECK(rwebm_video_stream_seek_begin_ns(s, 360000000));
   for (n = 0; n < 128 && !ret; n++) ret = rwebm_video_stream_seek_step(s, NULL);
   CHECK(ret == 1);
   CHECK(rwebm_video_stream_next(s, NULL));
   CHECK(rwebm_video_stream_hw_frame(s, &frame));
   CHECK(frame.color.max_cll == 2000 && frame.color.max_fall == 600);
   video_hw_frame_abandon(&frame);
   for (seek = 0; seek < 3; seek++)
   {
      ret = 0;
      CHECK(rwebm_video_stream_seek_begin_ns(s, positions[seek]));
      for (n = 0; n < 128 && !ret; n++) ret = rwebm_video_stream_seek_step(s, NULL);
      CHECK(ret == 1 && rwebm_video_stream_next(s, NULL));
      CHECK(rwebm_video_stream_hw_frame(s, &frame));
      CHECK(frame.color.max_cll == (seek == 1 ? 2000u : 1000u));
      CHECK(frame.color.max_fall == (seek == 1 ? 600u : 400u));
      video_hw_frame_abandon(&frame);
   }
   rwebm_video_stream_close(s);
   CHECK(opens == closes);
}

static void test_playback_hdr_cache(int inband)
{
   rwebm_video_stream_t *s;
   video_hw_frame_t frame = {0};
   unsigned n, before;
   int ret = 0;
   make_webm_clip(inband, 0, 0, 0, 12, 1, 2);
   s = rwebm_video_stream_open_hw_avail(bytes, used, used, NULL);
   CHECK(s && rwebm_video_stream_set_hw(s, 1));
   if (!s) return;
   for (n = 0; n < 12; n++)
   {
      CHECK(rwebm_video_stream_next(s, NULL));
      CHECK(rwebm_video_stream_hw_frame(s, &frame));
      CHECK(frame.color.max_cll == (n >= 4 ? 2000u : 1000u));
      video_hw_frame_abandon(&frame);
   }
   before = decodes;
   CHECK(rwebm_video_stream_seek_begin_ns(s, 240000000));
   for (n = 0; n < 128 && !ret; n++) ret = rwebm_video_stream_seek_step(s, NULL);
   CHECK(ret == 1 && n <= 6 && decodes == before);
   CHECK(rwebm_video_stream_next(s, NULL));
   CHECK(rwebm_video_stream_hw_frame(s, &frame));
   CHECK(frame.color.max_cll == 2000 && frame.color.max_fall == 600);
   video_hw_frame_abandon(&frame);
   rwebm_video_stream_close(s);
   CHECK(opens == closes);
}

static void test_changed_hdr_partial(int inband)
{
   rwebm_video_stream_t *s;
   video_hw_frame_t frame = {0};
   unsigned n, before = decodes;
   int ret = 0;
   make_webm_clip(inband, 0, 0, 0, 12, 1, 2);
   s = rwebm_video_stream_open_hw_avail(bytes, used, used, NULL);
   CHECK(s && rwebm_video_stream_set_hw(s, 1));
   if (!s) return;
   rwebm_video_stream_set_avail(s, media_at);
   CHECK(rwebm_video_stream_seek_begin_ns(s, 360000000));
   for (n = 0; n < 128 && !rwebm_video_stream_waiting(s); n++)
      CHECK(rwebm_video_stream_seek_step(s, NULL) == 0);
   CHECK(n < 128 && decodes == before);
   CHECK(rwebm_video_stream_seek_begin_ns(s, 0));
   rwebm_video_stream_set_avail(s, used);
   for (n = 0; n < 128 && !ret; n++) ret = rwebm_video_stream_seek_step(s, NULL);
   CHECK(ret == 1 && rwebm_video_stream_next(s, NULL));
   CHECK(rwebm_video_stream_hw_frame(s, &frame) && frame.color.max_cll == 1000);
   video_hw_frame_abandon(&frame);
   CHECK(rwebm_video_stream_seek_begin_ns(s, 360000000));
   ret = 0;
   for (n = 0; n < 128 && !ret; n++) ret = rwebm_video_stream_seek_step(s, NULL);
   CHECK(ret == 1 && rwebm_video_stream_next(s, NULL));
   CHECK(rwebm_video_stream_hw_frame(s, &frame));
   CHECK(frame.color.max_cll == 2000 && frame.color.max_fall == 600);
   video_hw_frame_abandon(&frame);
   rwebm_video_stream_close(s);
   CHECK(opens == closes);
}

static void test_changed_hdr_malformed(void)
{
   static const uint8_t record[] = {0x2a,6,1,7,208,2,88,128};
   rwebm_video_stream_t *s;
   size_t at;
   unsigned n, attempt, before = decodes;
   int ret;
   make_webm_clip(0, 0, 0, 0, 12, 1, 2);
   for (at = media_at; at + sizeof(record) <= used; at++)
      if (!memcmp(bytes + at, record, sizeof(record))) break;
   CHECK(at + sizeof(record) <= used);
   if (at + sizeof(record) > used) return;
   bytes[at + 7] = 0;
   s = rwebm_video_stream_open_hw_avail(bytes, used, used, NULL);
   CHECK(s && rwebm_video_stream_set_hw(s, 1));
   if (!s) return;
   for (attempt = 0; attempt < 2; attempt++)
   {
      if (attempt) rwebm_video_stream_set_avail(s, media_at);
      CHECK(rwebm_video_stream_seek_begin_ns(s, 360000000));
      if (attempt)
      {
         for (n = 0; n < 128 && !rwebm_video_stream_waiting(s); n++)
            CHECK(rwebm_video_stream_seek_step(s, NULL) == 0);
         CHECK(n < 128 && decodes == before);
         rwebm_video_stream_set_avail(s, used);
      }
      ret = 0;
      for (n = 0; n < 128 && !ret; n++) ret = rwebm_video_stream_seek_step(s, NULL);
      CHECK(ret == -1 && decodes == before);
      CHECK(!rwebm_video_stream_next(s, NULL));
   }
   rwebm_video_stream_close(s);
   CHECK(opens == closes);
}

static void test_seek_error(void)
{
   rwebm_video_stream_t *s;
   video_hw_frame_t held = {0}, frame = {0};
   unsigned n, before;
   int ret = 0;
   make_webm(1, 0, 0, 0);
   s = rwebm_video_stream_open_hw_avail(bytes, used, used, NULL);
   CHECK(s && rwebm_video_stream_set_hw(s, 1));
   if (!s) return;
   CHECK(rwebm_video_stream_next(s, NULL));
   CHECK(rwebm_video_stream_hw_frame(s, &held));
   before = abandons;
   CHECK(rwebm_video_stream_seek_begin_ns(s, 40000000));
   CHECK(abandons == before);
   decode_busy = -1;
   for (n = 0; n < 128 && ret == 0; n++)
      ret = rwebm_video_stream_seek_step(s, NULL);
   CHECK(ret == -1 && abandons == before && held.image);
   CHECK(!rwebm_video_stream_next(s, NULL) && !rwebm_video_stream_waiting(s));
   decode_busy = 0;
   CHECK(rwebm_video_stream_seek_begin_ns(s, 0));
   ret = 0;
   for (n = 0; n < 128 && ret == 0; n++)
      ret = rwebm_video_stream_seek_step(s, NULL);
   CHECK(ret == 1 && rwebm_video_stream_next(s, NULL));
   CHECK(rwebm_video_stream_hw_frame(s, &frame));
   rwebm_video_stream_close(s);
   CHECK(abandons == before);
   video_hw_frame_abandon(&held);
   video_hw_frame_abandon(&frame);
   CHECK(opens == closes);
}

static void test_bad_initial_hdr(int inband)
{
   rwebm_video_stream_t *s;
   unsigned before = opens;
   size_t cll;
   make_webm_clip(inband, 0, 0, 0, 4, 1, 1);
   cll = media_at + (inband ? sizeof(config) - 4 : 0);
   bytes[cll + 7] = 0;
   s = rwebm_video_stream_open_hw_avail(bytes, used, used, NULL);
   CHECK(s && !rwebm_video_stream_set_hw(s, 1) && opens == before);
   rwebm_video_stream_close(s);
}

static void test_config(int inband, unsigned matrix, unsigned transfer,
                        unsigned range, int allowed)
{
   rwebm_video_stream_t *s;
   video_hw_frame_t frame = {0};
   make_webm(inband, matrix, transfer, range);
   s = rwebm_video_stream_open_hw_avail(bytes, used, used, NULL);
   CHECK(s != NULL);
   if (!s)
      return;
   CHECK(rwebm_video_stream_set_hw(s, 1) == allowed);
   if (allowed)
   {
      CHECK(rwebm_video_stream_next(s, NULL) != NULL);
      CHECK(rwebm_video_stream_hw_frame(s, &frame));
      CHECK(((frame.yuv & IMAGE_YUV_FLAG_BT709) != 0) == (matrix == 1));
      CHECK(frame.color.transfer == transfer);
      CHECK(frame.color.range == 1);
      video_hw_frame_abandon(&frame);
   }
   rwebm_video_stream_close(s);
}

#define SEQUENCE_CACHE_WEBM
#include "rav1_sequence_cache_test.h"

int main(int argc, char **argv)
{
   rwebm_video_stream_t *s;
   factory.user = &factory;
   factory.open = open_backend;
   factory.close = close_backend;
   if (argc == 2 && !strcmp(argv[1], "--config"))
      factory.open_config = open_config;
   rav1_set_hw_backend(&factory);
   test_stream(0);
   test_stream(1);
   test_seek(0);
   test_seek(1);
   test_seek_partial(0);
   test_seek_partial(1);
   test_seek_error();
   test_seek_changed_hdr(0);
   test_seek_changed_hdr(1);
   test_playback_hdr_cache(0);
   test_playback_hdr_cache(1);
   test_changed_hdr_partial(0);
   test_changed_hdr_partial(1);
   test_changed_hdr_malformed();
   test_seek_early_hdr(0);
   test_seek_early_hdr(1);
   test_bad_initial_hdr(0);
   test_bad_initial_hdr(1);
   test_config(0, 1, 0, 1, 1);
   test_config(1, 0, 16, 0, 1);
   test_config(0, 0, 18, 0, 1);
   test_config(1, 9, 0, 0, 0);
   test_config(1, 10, 16, 0, 0);
   test_config(0, 10, 18, 0, 0);
   test_config(0, 0, 0, 2, 0);
   test_config(1, 0, 0, 3, 0);
   make_webm(1, 0, 0, 0);
   bytes[config_at + 2] ^= 0x40;
   s = rwebm_video_stream_open_hw_avail(bytes, used, used, NULL);
   CHECK(s && !rwebm_video_stream_set_hw(s, 1));
   rwebm_video_stream_close(s);
   rav1_set_hw_backend(NULL);
   make_webm(0, 0, 0, 0);
   s = rwebm_video_stream_open_hw_avail(bytes, used, used, NULL);
   CHECK(s && !rwebm_video_stream_set_hw(s, 1));
   CHECK(!rwebm_video_stream_next(s, NULL));
   rwebm_video_stream_close(s);
   CHECK(opens == closes && exports == abandons);
   rav1_set_hw_backend(&factory);
   if (factory.open_config)
   {
      test_sequence_reopen(0);
      test_sequence_reopen(1);
   }
   rav1_set_hw_backend(NULL);
   if (factory.open_config) CHECK(config_opens);
   printf("AV1 WebM hardware reader: %u failures\n", failures);
   return failures ? 1 : 0;
}
