/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_gpu_session_vulkan_hevc.h"
static record_gpu_session_vulkan_t *test_hevc_session_new(bool audio);
#define SESSION_HEVC_TEST
#define main h264_shared_session_contracts
#include "vulkan_session_test.c"
#undef main
#include "../../../record/record_gpu_session_vulkan_hevc.c"
static unsigned hevc_opens, hevc_drains, hevc_frees;
static unsigned test_depth = 8, test_color = VULKAN_ENCODE_HEVC_BT709;
static bool test_full;
static const uint8_t hevc_headers[] = {
   0x00,0x00,0x00,0x01,0x40,0x01,0x0c,0x01,0xff,0xff,0x01,0x60,
   0x00,0x00,0x03,0x00,0xb0,0x00,0x00,0x03,0x00,0x00,0x03,0x00,
   0x3f,0x2c,0x0c,0x00,0x00,0x03,0x00,0x04,0x00,0x00,0x03,0x00,
   0x7b,0xa0,0x00,0x00,0x00,0x01,0x42,0x01,0x01,0x01,0x60,0x00,
   0x00,0x03,0x00,0xb0,0x00,0x00,0x03,0x00,0x00,0x03,0x00,0x3f,
   0xa0,0x18,0x20,0x20,0x59,0x4b,0xb9,0x08,0x46,0x4b,0x8c,0x05,
   0xa8,0x08,0x08,0x08,0x20,0x00,0x00,0x03,0x00,0x20,0x00,0x00,
   0x03,0x03,0xd8,0x80,0x00,0x00,0x00,0x01,0x44,0x01,0xc0,0x73,
   0xc0,0xcc,0x90,
};
vulkan_encode_hevc_t *vulkan_encode_hevc_open(const vulkan_encode_hevc_open_t *c,
      const vulkan_encode_hevc_ops_t *ops)
{
   vulkan_encode_h264_open_t mock;
   static const vulkan_encode_h264_ops_t dispatch = {{0}, {0}, {0}};
   assert(ops);
   memset(&mock, 0, sizeof(mock));
   mock.slots = c->slots;
   hevc_opens++;
   return (vulkan_encode_hevc_t*)vulkan_encode_h264_open(&mock, &dispatch);
}
const vulkan_encode_pool_t *vulkan_encode_hevc_pool(const vulkan_encode_hevc_t *e)
{
   return vulkan_encode_h264_pool((const vulkan_encode_h264_t*)e);
}
bool vulkan_encode_hevc_headers(vulkan_encode_hevc_t *e, void *data,
      size_t capacity, size_t *size)
{
   assert(e && capacity >= sizeof(hevc_headers));
   if (headers_fail)
      return false;
   memcpy(data, hevc_headers, sizeof(hevc_headers));
   *size = sizeof(hevc_headers);
   return true;
}
int vulkan_encode_hevc_submit(vulkan_encode_hevc_t *e, unsigned slot,
      uint64_t pts, uint64_t duration, uint64_t ready, bool keyframe)
{
   static const uint8_t picture[] = {0,0,0,1,0x26,1,0x80};
   vulkan_encode_h264_t *mock = (vulkan_encode_h264_t*)e;
   int result = vulkan_encode_h264_submit(mock, slot, pts, duration, ready, keyframe);
   if (result == VULKAN_ENCODE_OK)
   {
      vulkan_encode_packet_t *p = &mock->packet[(mock->head + mock->count - 1) % 16];
      p->data = picture;
      p->size = sizeof(picture);
   }
   return result;
}
int vulkan_encode_hevc_poll(vulkan_encode_hevc_t *e, vulkan_encode_packet_t *p)
{
   return vulkan_encode_h264_poll((vulkan_encode_h264_t*)e, p);
}
bool vulkan_encode_hevc_release(vulkan_encode_hevc_t *e)
{
   return vulkan_encode_h264_release((vulkan_encode_h264_t*)e);
}
int vulkan_encode_hevc_finish(vulkan_encode_hevc_t *e)
{
   return vulkan_encode_h264_finish((vulkan_encode_h264_t*)e);
}
bool vulkan_encode_hevc_drain(vulkan_encode_hevc_t *e)
{
   hevc_drains++;
   return vulkan_encode_h264_drain((vulkan_encode_h264_t*)e);
}
bool vulkan_encode_hevc_free(vulkan_encode_hevc_t *e)
{
   hevc_frees++;
   return vulkan_encode_h264_free((vulkan_encode_h264_t*)e);
}
static record_gpu_session_vulkan_t *test_hevc_session_new(bool audio)
{
   static const vulkan_encode_hevc_ops_t ops = {{0}, {0}, {0}};
   vulkan_encode_hevc_open_t c;
   record_gpu_output_vulkan_config_t output;
   memset(&c, 0, sizeof(c));
   memset(&output, 0, sizeof(output));
   c.slots = 2;
   c.session.video.width = 192;
   c.session.video.height = 128;
   c.session.video.bit_depth = test_depth;
   c.session.video.color = (enum vulkan_encode_hevc_color)test_color;
   c.session.video.full_range = test_full;
   c.session.video.fps_num = 30000;
   c.session.video.fps_den = 1001;
   output.video.max_packet_size = 65536;
   if (audio)
   {
      output.audio_rate = 48000;
      output.audio_layout = 3;
      output.audio_kbps = 192;
      output.audio_fifo_frames = 1536;
   }
   return record_gpu_session_vulkan_new_hevc(&c, &ops, &output, "unused");
}
static void metadata(void)
{
   static const uint8_t colors[4][3] = {{2,2,2},{1,1,1},{9,16,9},{9,18,9}};
   record_gpu_session_vulkan_t *s;
   const uint8_t *data;
   size_t i, size;
   unsigned seen;
   for (test_color = 0; test_color < 4; test_color++)
      for (test_depth = 8; test_depth <= 10; test_depth += 2)
      {
         if (test_depth == 8 && test_color >= VULKAN_ENCODE_HEVC_PQ)
            continue;
         for (test_full = false; ; test_full = true)
         {
            reset();
            s = test_hevc_session_new(false);
            assert(s && record_gpu_session_vulkan_error(s, NULL) == RECORD_VULKAN_SESSION_NONE);
            assert(record_gpu_session_vulkan_step(s) == RECORD_VULKAN_OUTPUT_PROGRESS);
            data = (const uint8_t*)s->output->file->data;
            size = s->output->file->size;
            seen = 0;
            for (i = 4; i + 16 <= size; i++)
               if (!memcmp(data + i, "colr", 4))
               {
                  assert(!memcmp(data + i + 4, "nclx", 4));
                  assert(data[i + 9] == colors[test_color][0]);
                  assert(data[i + 11] == colors[test_color][1]);
                  assert(data[i + 13] == colors[test_color][2]);
                  assert(data[i + 14] == (test_full ? 128 : 0));
                  seen++;
               }
            assert(seen == 1);
            assert(record_gpu_session_vulkan_audio_finish(s));
            assert(record_gpu_session_vulkan_video_finish(s));
            assert(terminal(s) == RECORD_VULKAN_OUTPUT_END);
            assert(record_gpu_session_vulkan_free(s));
            if (test_full)
               break;
         }
      }
}
int main(void)
{
   assert(h264_shared_session_contracts() == 0);
   metadata();
   assert(hevc_opens && hevc_drains && hevc_frees);
   puts("HEVC session factory: shared audio, cancellation and retirement contracts PASS");
   return 0;
}
