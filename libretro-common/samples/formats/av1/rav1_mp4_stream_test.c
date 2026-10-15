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
static uint8_t bytes[4096];
static size_t used, config_at, media_at, color_at;
static const uint8_t config[] = {0x81, 0x00, 0x0c, 0x00, 0x0a, 0x09, 0x00, 0x00,
                                 0x00, 0x02, 0xaf, 0xff, 0x80, 0x00, 0x01};
static const uint8_t packet[] = {0x32, 0x05, 0x0e, 0xff, 0x40,
                                 0x00, 0x80, 0x18, 0x88};

static void put(unsigned n)
{
   bytes[used++] = (uint8_t)n;
}
static void be32(uint32_t n)
{
   put(n >> 24);
   put(n >> 16);
   put(n >> 8);
   put(n);
}
static void zeros(size_t n)
{
   while (n--)
      put(0);
}
static void copy(const void *p, size_t n)
{
   memcpy(bytes + used, p, n);
   used += n;
}
static void patch32(size_t at, uint32_t n)
{
   bytes[at] = n >> 24;
   bytes[at + 1] = n >> 16;
   bytes[at + 2] = n >> 8;
   bytes[at + 3] = n;
}
static size_t box(const char *name)
{
   size_t at = used;
   be32(0);
   copy(name, 4);
   return at;
}
static void end_box(size_t at)
{
   patch32(at, (uint32_t)(used - at));
}

static void make_mp4(int with_config)
{
   size_t b, moov, trak, mdia, minf, stbl, stsd, entry, chunk;
   used = 0;
   config_at = 0;
   b = box("ftyp");
   copy("isom", 4);
   be32(0);
   copy("av01", 4);
   end_box(b);
   moov = box("moov");
   b = box("mvhd");
   zeros(12);
   be32(1000);
   be32(80);
   zeros(80);
   end_box(b);
   trak = box("trak");
   b = box("tkhd");
   zeros(12);
   be32(1);
   zeros(68);
   end_box(b);
   mdia = box("mdia");
   b = box("mdhd");
   zeros(12);
   be32(1000);
   be32(80);
   zeros(4);
   end_box(b);
   b = box("hdlr");
   zeros(8);
   copy("vide", 4);
   zeros(13);
   end_box(b);
   minf = box("minf");
   stbl = box("stbl");
   stsd = box("stsd");
   be32(0);
   be32(1);
   entry = box("av01");
   zeros(24);
   put(0);
   put(64);
   put(0);
   put(64);
   zeros(50);
   if (with_config)
   {
      b = box("av1C");
      config_at = used;
      copy(config, sizeof(config));
      end_box(b);
   }
   b = box("colr");
   copy("nclx", 4);
   color_at = used;
   put(0);
   put(1);
   put(0);
   put(2);
   put(0);
   put(2);
   put(0);
   end_box(b);
   end_box(entry);
   end_box(stsd);
   b = box("stts");
   be32(0);
   be32(1);
   be32(2);
   be32(40);
   end_box(b);
   b = box("stsc");
   be32(0);
   be32(1);
   be32(1);
   be32(2);
   be32(1);
   end_box(b);
   b = box("stsz");
   be32(0);
   be32(sizeof(packet));
   be32(2);
   end_box(b);
   b = box("stco");
   be32(0);
   be32(1);
   chunk = used;
   be32(0);
   end_box(b);
   b = box("stss");
   be32(0);
   be32(1);
   be32(1);
   end_box(b);
   end_box(stbl);
   end_box(minf);
   end_box(mdia);
   end_box(trak);
   end_box(moov);
   b = box("mdat");
   media_at = used;
   copy(packet, sizeof(packet));
   copy(packet, sizeof(packet));
   end_box(b);
   patch32(chunk, (uint32_t)media_at);
}

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
   CHECK(!rmp4_video_stream_next(s, &duration));
   CHECK(decodes == 2 && !exports);
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
   CHECK(opens == before + 1 && closes == before);
   decode_busy = 1;
   CHECK(!rmp4_video_stream_next(s, NULL));
   before = opens;
   rmp4_video_stream_rewind(s);
   CHECK(opens == before + 1 && closes == before);
   decode_busy = 0;
   rav1_set_hw_backend(NULL);
   rmp4_video_stream_close(s);
   video_hw_frame_abandon(&frame);
   CHECK(opens == closes && abandons == 1);
   rav1_set_hw_backend(&factory);
}

static void test_color(void)
{
   rmp4_video_stream_t *s;
   video_hw_frame_t frame;
   unsigned i, before = opens;
   const uint32_t *marker = NULL;
   make_mp4(1);
   bytes[color_at + 3] = 16;
   s = rmp4_video_stream_open_hw_avail(bytes, used, used, NULL, NULL, NULL);
   CHECK(s != NULL);
   if (s)
   {
      CHECK(!rmp4_video_stream_set_hw(s, 1));
      rmp4_video_stream_close(s);
   }
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
      video_hw_frame_abandon(&frame);
   }
   rmp4_video_stream_close(s);
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

int main(void)
{
   factory.user = &factory;
   factory.open = open_backend;
   factory.close = close_backend;
   rav1_set_hw_backend(&factory);
   test_stream();
   test_color();
   test_unavailable();
   rav1_set_hw_backend(NULL);
   printf("AV1 MP4 hardware reader: %u failures\n", failures);
   return failures ? 1 : 0;
}
