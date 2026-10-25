/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../video_fixture_stream.h"

static int h26x_webm_executed;
static int h26x_webm_skipped;
static void h26x_fixture_progress(oracle_readback_t *reader)
{
   oracle_release(reader);
   vkDeviceWaitIdle(vvd.device);
}

static int h26x_webm_hash(video_fixture_stream_t *stream, unsigned depth,
      uint32_t *out)
{
   rh264_video *avc = (rh264_video*)video_fixture_stream_h264(stream);
   rh265_video *hevc = (rh265_video*)video_fixture_stream_h265(stream);
   unsigned plane;
   uint32_t hash = UINT32_C(2166136261);
   for (plane = 0; plane < 3; plane++)
   {
      int stride, width, height, x, y;
      const uint8_t *data = avc
         ? rh264_video_plane(avc, (int)plane, &stride, &width, &height)
         : rh265_video_plane(hevc, (int)plane, &stride, &width, &height);
      if (!data) return 0;
      for (y = 0; y < height; y++)
         for (x = 0; x < width; x++)
         {
            unsigned sample = depth == 10
               ? ((const uint16_t*)data)[y * stride + x] : data[y * stride + x];
            hash = (hash ^ (sample & 255)) * UINT32_C(16777619);
            if (depth == 10) hash = (hash ^ (sample >> 8)) * UINT32_C(16777619);
         }
   }
   *out = hash;
   return 1;
}

static int h26x_webm_decode_fixture(void)
{
   const char *path = getenv("H26X_TEST_MP4");
   const char *container = path ? "MP4" : "Matroska";
   const char *color = getenv("H26X_TEST_COLOR");
   uint8_t *data;
   size_t size;
   video_fixture_stream_t *stream = NULL;
   oracle_readback_t *reader = (oracle_readback_t*)calloc(1, sizeof(*reader));
   hevc_fixture_t fixture;
   uint32_t hash[24];
   int64_t timestamp[24];
   int duration[24];
   unsigned pass, count = 0, polls, width = 0, height = 0;
   int ok = 0;
   if (!path) path = getenv("H26X_TEST_WEBM");
   h26x_webm_executed = 1;
   memset(&fixture, 0, sizeof(fixture));
   fixture.name = container;
   fixture.hash = hash;
   fixture.frames = 24;
   data = oracle_load(path, &size);
   if (!data || !reader) goto done;
   for (pass = 0; pass < 2; pass++)
   {
      stream = video_fixture_stream_open(data, size);
      if (!stream || (!video_fixture_stream_h264(stream)
               && !video_fixture_stream_h265(stream))) goto done;
      if (color && (!strcmp(color, "pq") != !!video_fixture_stream_is_hdr(stream)))
         goto done;
      video_fixture_stream_get_info(stream, &width, &height, NULL, NULL);
      if (pass && !video_fixture_stream_set_hw(stream, 1))
      {
#ifdef _WIN32
         if (video_fixture_stream_h264(stream) && vvd.h264_broken)
         {
            puts("SKIP Matroska AVC Vulkan: existing NVIDIA driver corruption guard");
            h26x_webm_skipped = 1;
            goto done;
         }
#endif
         puts("FAIL Matroska Vulkan: requested hardware decoder not admitted");
         goto done;
      }
      count = 0;
      for (polls = 0; polls < 8192; polls++)
      {
         int span;
         if (video_fixture_stream_next(stream, &span))
         {
            if (count >= 24) goto done;
            if (!pass)
            {
               rh264_video *avc = (rh264_video*)video_fixture_stream_h264(stream);
               rh265_video *hevc = (rh265_video*)video_fixture_stream_h265(stream);
               unsigned depth = avc ? (unsigned)rh264_video_bit_depth(avc)
                  : (unsigned)rh265_video_bit_depth(hevc);
               if (depth != 8 && depth != 10) goto done;
               if (!count) fixture.depth = depth;
               if (depth != fixture.depth || !h26x_webm_hash(stream, depth, &hash[count]))
                  goto done;
               timestamp[count] = video_fixture_stream_frame_time_ns(stream);
               if (span <= 0 || (count && timestamp[count] <= timestamp[count - 1]))
                  goto done;
               duration[count++] = span;
            }
            else
            {
               video_hw_frame_t frame;
               if (timestamp[count] != video_fixture_stream_frame_time_ns(stream)
                     || duration[count] != span
                     || !video_fixture_stream_hw_frame(stream, &frame)) goto done;
               if (frame.width != width || frame.height != height
                     || (frame.api && frame.api != VIDEO_HW_API_VULKAN)
                     || frame.pixfmt != (fixture.depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12)
                     || (color && !strcmp(color, "pq")
                        && (frame.color.primaries != 9 || frame.color.transfer != 16
                           || frame.color.matrix != 9))
                     || (color && !strcmp(color, "709")
                        && (frame.color.primaries != 1 || frame.color.transfer != 1
                           || frame.color.matrix != 1)))
               {
                  fprintf(stderr, "Matroska Vulkan lease: %ux%u api=%u format=%u color=%u/%u/%u\n",
                        frame.width, frame.height, frame.api, frame.pixfmt,
                        frame.color.primaries, frame.color.transfer, frame.color.matrix);
                  video_hw_frame_abandon(&frame);
                  goto done;
               }
               if (reader->held_count == 4) oracle_release(reader);
               if (!oracle_read_frame(reader, frame, &fixture, &count)) goto done;
            }
         }
         else if (video_fixture_stream_completed(stream)) break;
         else if (video_fixture_stream_waiting(stream))
         {
            h26x_fixture_progress(reader);
         }
         else goto done;
      }
      if (polls == 8192 || count != 24) goto done;
      if (!pass && !oracle_init(reader, width, height, fixture.depth)) goto done;
      video_fixture_stream_close(stream);
      stream = NULL;
   }
   if (!strcmp(container, "MP4"))
   {
      static const unsigned targets[] = {12, 3, 23};
      unsigned seek;
      oracle_release(reader);
      stream = video_fixture_stream_open(data, size);
      if (!stream || !video_fixture_stream_set_hw(stream, 1)) goto done;
      for (seek = 0; seek < 3; seek++)
      {
         unsigned index = targets[seek];
         int ret = 0, span;
         int64_t position = -1;
         video_hw_frame_t frame;
         if (!video_fixture_stream_seek_begin(stream, timestamp[index])) goto done;
         for (polls = 0; polls < 8192 && !ret; polls++)
         {
            ret = video_fixture_stream_seek_step(stream, &position);
            if (!ret && video_fixture_stream_waiting(stream)) h26x_fixture_progress(reader);
         }
         if (ret != 1 || position != timestamp[index] / 1000)
         {
            fprintf(stderr, "MP4 Vulkan seek %u: ret=%d polls=%u position=%.3f expected=%.3f\n",
                  seek, ret, polls, (double)position / 1000, (double)timestamp[index] / 1000000);
            goto done;
         }
         for (polls = 0; polls < 8192; polls++)
         {
            if (video_fixture_stream_next(stream, &span)) break;
            if (!video_fixture_stream_waiting(stream)) goto done;
            h26x_fixture_progress(reader);
         }
         if (polls == 8192 || span != duration[index]
               || video_fixture_stream_frame_time_ns(stream) != timestamp[index])
         {
            fprintf(stderr, "MP4 Vulkan seek picture %u: polls=%u time=%.3f expected=%.3f duration=%d expected=%d\n",
                  seek, polls, (double)video_fixture_stream_frame_time_ns(stream) / 1000000,
                  (double)timestamp[index] / 1000000, span, duration[index]);
            goto done;
         }
         if (!video_fixture_stream_hw_frame(stream, &frame)) goto done;
         if (!oracle_read_frame(reader, frame, &fixture, &index)) goto done;
         oracle_release(reader);
      }
      puts("MP4 native Vulkan: 3 seek pictures, exact planes and timing PASS");
   }
   ok = 1;
done:
   video_fixture_stream_close(stream);
   if (reader) { oracle_free(reader); free(reader); }
   free(data);
   if (h26x_webm_skipped)
      puts("Matroska native Vulkan fixture: SKIP");
   else
      printf("%s native Vulkan: %u displayed pictures, exact planes and timing %s\n",
            container, count, ok ? "PASS" : "FAIL");
   return ok || h26x_webm_skipped;
}
