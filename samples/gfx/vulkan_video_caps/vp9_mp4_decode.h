/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <formats/rmp4_video.h>

static unsigned vp9_mp4_executed;

static int vp9_mp4_decode_fixture(void)
{
   const char *path = getenv("VP9_TEST_MP4");
   const char *raw_path = getenv("VP9_TEST_MP4_RAW");
   const char *depth = getenv("VP9_TEST_MP4_DEPTH");
   rmp4_video_stream_t *stream = NULL;
   uint8_t *mp4 = NULL, *raw = NULL;
   uint32_t *hash = NULL;
   size_t size, raw_size, frame_size;
   unsigned width, height, bps, i, plane, x, y, count = 0, seen = 0, tries;
   oracle_readback_t reader = {0};
   hevc_fixture_t fixture;
   video_hw_frame_t held;
   int ret, ok = 0;
   if (!path && !raw_path && !depth)
      return 1;
   memset(&fixture, 0, sizeof(fixture));
   memset(&held, 0, sizeof(held));
   fixture.name = "vp9-mp4";
   if (!path || !raw_path || !depth
         || (strcmp(depth, "8") && strcmp(depth, "10")))
      goto done;
   fixture.depth = (unsigned)atoi(depth);
   bps = fixture.depth == 10 ? 2 : 1;
   mp4 = oracle_load(path, &size);
   raw = oracle_load(raw_path, &raw_size);
   if (!mp4 || !raw || !(stream = rmp4_video_stream_open(mp4, size)))
      goto done;
   rmp4_video_stream_get_info(stream, &width, &height, NULL, NULL);
   if (!width || !height || ((width | height) & 1) || width > 16384
         || height > 16384)
      goto done;
   if (!rmp4_video_stream_set_hw(stream, 1))
   {
      printf("VP9 MP4 hardware admission refused: %ux%u, depth %u\n",
            width, height, fixture.depth);
      goto done;
   }
   vp9_mp4_executed++;
   frame_size = (size_t)width * height * 3 / 2 * bps;
   if (!frame_size || raw_size % frame_size || !raw_size
         || raw_size / frame_size > 1024)
      goto done;
   fixture.frames = (unsigned)(raw_size / frame_size);
   hash = (uint32_t *)calloc(fixture.frames, sizeof(*hash));
   fixture.hash = hash;
   if (!hash || !oracle_init(&reader, width, height, fixture.depth))
      goto done;
   for (i = 0; i < fixture.frames; i++)
   {
      const uint8_t *data = raw + frame_size * i;
      hash[i] = UINT32_C(2166136261);
      for (plane = 0; plane < 3; plane++)
         for (y = 0; y < height >> (plane ? 1 : 0); y++)
            for (x = 0; x < width >> (plane ? 1 : 0); x++)
            {
               size_t at = plane
                     ? ((size_t)width * height
                           + (y * (width / 2) + x) * 2 + plane - 1) * bps
                     : (y * width + x) * bps;
               unsigned sample = data[at];
               if (bps == 2)
                  sample = (sample | (unsigned)data[at + 1] << 8) >> 6;
               hash[i] = (hash[i] ^ (sample & 255)) * UINT32_C(16777619);
               if (bps == 2)
                  hash[i] = (hash[i] ^ (sample >> 8)) * UINT32_C(16777619);
            }
   }
   for (tries = 0; tries < 4096; tries++)
   {
      video_hw_frame_t frame;
      ret = rmp4_video_stream_skip(stream, NULL);
      if (ret < 0)
         break;
      if (ret == 2)
      {
         oracle_release(&reader);
         vkDeviceWaitIdle(vvd.device);
         continue;
      }
      if (ret != 1 || !rmp4_video_stream_hw_frame(stream, &frame))
         goto done;
      if (++seen == fixture.frames)
         held = frame;
      else if (!oracle_read_frame(&reader, frame, &fixture, &count))
         goto done;
   }
   if (tries == 4096 || seen != fixture.frames
         || !rmp4_video_stream_completed(stream) || !held.image)
      goto done;
   for (i = 0; i < fixture.frames; i++)
   {
      video_hw_frame_t frame;
      int64_t position;
      unsigned replay = i;
      if (!rmp4_video_stream_seek_begin_ns(stream, (int64_t)i * 250000000))
         goto done;
      for (tries = 0; tries < 4096; tries++)
      {
         ret = rmp4_video_stream_seek_step(stream, &position);
         if (ret)
            break;
         oracle_release(&reader);
         vkDeviceWaitIdle(vvd.device);
      }
      if (ret != 1 || position != (int64_t)i * 250000)
         goto done;
      for (tries = 0; tries < 4096; tries++)
      {
         ret = rmp4_video_stream_skip(stream, NULL);
         if (ret != 2)
            break;
         oracle_release(&reader);
         vkDeviceWaitIdle(vvd.device);
      }
      if (ret != 1 || !rmp4_video_stream_hw_frame(stream, &frame)
            || !oracle_read_frame(&reader, frame, &fixture, &replay)
            || replay != i + 1)
         goto done;
   }
   rmp4_video_stream_close(stream);
   stream = NULL;
   if (!oracle_read_frame(&reader, held, &fixture, &count))
   {
      memset(&held, 0, sizeof(held));
      goto done;
   }
   memset(&held, 0, sizeof(held));
   ok = count == fixture.frames;
done:
   video_hw_frame_abandon(&held);
   if (stream)
      rmp4_video_stream_close(stream);
   vkDeviceWaitIdle(vvd.device);
   oracle_free(&reader);
   free(hash);
   free(raw);
   free(mp4);
   printf("VP9 MP4 Vulkan %s: %s (%u pictures, seeks, last lease after close)\n",
         path ? path : "missing", ok ? "PASS" : "FAIL", count);
   return ok;
}
