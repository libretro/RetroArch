/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <formats/rwebm_video.h>

static int av1_webm_seek_frame(rwebm_video_stream_t *stream,
      oracle_readback_t *reader, const hevc_fixture_t *fixture,
      const char *path, unsigned expected)
{
   video_hw_frame_t frame;
   unsigned tries;
   int ret = 0;
   int64_t target = (int64_t)expected * 1000000000 / 25;
   oracle_release(reader);
   if (!rwebm_video_stream_seek_begin_ns(stream, target)) return 0;
   for (tries = 0; tries < 8192; tries++)
   {
      ret = rwebm_video_stream_seek_step(stream, NULL);
      if (ret) break;
      vkDeviceWaitIdle(vvd.device);
   }
   if (ret != 1 || tries == 8192) return 0;
   for (tries = 0; tries < 8192; tries++)
   {
      if (rwebm_video_stream_next(stream, NULL)) break;
      if (!rwebm_video_stream_waiting(stream)) return 0;
      vkDeviceWaitIdle(vvd.device);
   }
   if (tries == 8192 || !rwebm_video_stream_hw_frame(stream, &frame)) return 0;
   if (!av1_hdr_frame_matches(path, &frame))
   {
      video_hw_frame_abandon(&frame);
      return 0;
   }
   if (getenv("AV1_TEST_HDR_TIMELINE") &&
         (frame.color.max_cll != (expected >= 5 ? 2000u : 1000u) ||
          frame.color.max_fall != (expected >= 5 ? 600u : 400u)))
   {
      fprintf(stderr, "FAIL AV1 seek HDR timeline at picture %u: %u/%u\n",
            expected, frame.color.max_cll, frame.color.max_fall);
      video_hw_frame_abandon(&frame);
      return 0;
   }
   return oracle_read_frame(reader, frame, fixture, &expected);
}

static int av1_webm_decode_fixture(void)
{
   const char *path = getenv("AV1_TEST_WEBM"),
              *raw_path = getenv("AV1_TEST_RAW");
   uint8_t *data = NULL, *raw = NULL;
   size_t size = 0, raw_size = 0, frame_size;
   rwebm_t *demux = NULL;
   rwebm_video_stream_t *stream = NULL;
   rav1_sequence_t *sequence = NULL;
   oracle_readback_t *reader = NULL;
   rav1_config_t config;
   hevc_fixture_t fixture;
   uint32_t *hash = NULL;
   video_hw_frame_t frame;
   const rwebm_track *track = NULL;
   unsigned count = 0, i, plane, x, y, bps, tries, probe, retire_index = 0;
   int ok = 0, ret;
   memset(&fixture, 0, sizeof(fixture));
   fixture.name = "av1_webm";
   if (!path || !raw_path)
      goto done;
   data = oracle_load(path, &size);
   raw = oracle_load(raw_path, &raw_size);
   sequence = (rav1_sequence_t *)calloc(1, sizeof(*sequence));
   reader = (oracle_readback_t *)calloc(1, sizeof(*reader));
   if (!data || !raw || !sequence || !reader)
      goto done;
   demux = rwebm_open_memory(data, size);
   if (!demux)
      goto done;
   for (i = 0; i < (unsigned)rwebm_num_tracks(demux); i++)
   {
      track = rwebm_get_track(demux, (int)i);
      if (track && track->codec == RWEBM_CODEC_AV1)
         break;
   }
   if (!track || track->codec != RWEBM_CODEC_AV1 ||
       !rav1_config_parse(&config, track->codec_private,
                          track->codec_private_size) ||
       (config.bit_depth != 8 && config.bit_depth != 10))
      goto done;
   sequence->max_width = track->width;
   sequence->max_height = track->height;
   fixture.depth = config.bit_depth;
   bps = fixture.depth == 10 ? 2 : 1;
   frame_size =
       (size_t)sequence->max_width * sequence->max_height * 3 / 2 * bps;
   if (!frame_size || raw_size % frame_size || raw_size / frame_size > 1024)
      goto done;
   fixture.frames = (unsigned)(raw_size / frame_size);
   hash = (uint32_t *)calloc(fixture.frames, sizeof(*hash));
   if (!hash || !oracle_init(reader, sequence->max_width, sequence->max_height,
                             fixture.depth))
      goto done;
   fixture.hash = hash;
   for (i = 0; i < fixture.frames; i++)
   {
      const uint8_t *pixels = raw + frame_size * i;
      hash[i] = UINT32_C(2166136261);
      for (plane = 0; plane < 3; plane++)
         for (y = 0; y < sequence->max_height >> (plane ? 1 : 0); y++)
            for (x = 0; x < sequence->max_width >> (plane ? 1 : 0); x++)
            {
               size_t pos =
                   plane
                       ? ((size_t)sequence->max_width * sequence->max_height +
                          (y * (sequence->max_width / 2) + x) * 2 + plane - 1) *
                             bps
                       : (y * sequence->max_width + x) * bps;
               unsigned sample = pixels[pos];
               if (bps == 2)
                  sample = (sample | (unsigned)pixels[pos + 1] << 8) >> 6;
               hash[i] = (hash[i] ^ (sample & 255)) * UINT32_C(16777619);
               if (bps == 2)
                  hash[i] = (hash[i] ^ (sample >> 8)) * UINT32_C(16777619);
            }
   }
   if (getenv("AV1_TEST_CORE"))
   {
      ok = av1_core_decode_fixture(path, reader, &fixture, &count);
      goto done;
   }
   stream = rwebm_video_stream_open_hw_avail(data, size, size, NULL);
   if (!stream || !rwebm_video_stream_set_hw(stream, 1))
      goto done;
   if (getenv("AV1_TEST_SEEK"))
   {
      if (fixture.frames < 10 ||
          !av1_webm_seek_frame(stream, reader, &fixture, path, 9)) goto done;
      puts("AV1 WebM early seek before first display: pixels and metadata PASS");
      oracle_release(reader);
      rwebm_video_stream_rewind(stream);
   }
   for (tries = 0; tries < 4096; tries++)
   {
      if (!rwebm_video_stream_next(stream, NULL))
      {
         if (!rwebm_video_stream_waiting(stream))
            break;
         oracle_release(reader);
         vkDeviceWaitIdle(vvd.device);
         continue;
      }
      if (!rwebm_video_stream_hw_frame(stream, &frame) ||
          !av1_hdr_frame_matches(path, &frame) ||
          !oracle_read_frame(reader, frame, &fixture, &count))
         goto done;
   }
   if (count != fixture.frames)
      goto done;
   retire_index = count ? count - 1 : 0;
   if (count > 2)
   {
      oracle_release(reader);
      rwebm_video_stream_rewind(stream);
      for (probe = 0; probe < 2; probe++)
      {
         for (tries = 0; tries < 4096; tries++)
         {
            ret = rwebm_video_stream_skip(stream, NULL);
            if (ret != 2)
               break;
            oracle_release(reader);
            vkDeviceWaitIdle(vvd.device);
         }
         if (ret != 1 || rwebm_video_stream_hw_frame(stream, &frame))
            goto done;
      }
      for (tries = 0; tries < 4096; tries++)
      {
         if (rwebm_video_stream_next(stream, NULL))
            break;
         if (!rwebm_video_stream_waiting(stream))
            goto done;
         oracle_release(reader);
         vkDeviceWaitIdle(vvd.device);
      }
      probe = 2;
      if (!rwebm_video_stream_hw_frame(stream, &frame) ||
          !oracle_read_frame(reader, frame, &fixture, &probe) || probe != 3)
         goto done;
      retire_index = probe - 1;
   }
   if (getenv("AV1_TEST_SEEK"))
   {
      unsigned attempt;
      if (fixture.frames < 10 || !rwebm_video_stream_seekable(stream)) goto done;
      for (attempt = 0; attempt < 3; attempt++)
      {
         unsigned expected = attempt == 1 ? 0 : 9;
         if (!av1_webm_seek_frame(stream, reader, &fixture, path, expected)) goto done;
         retire_index = expected;
      }
      puts("AV1 WebM seeks 9 to 0 to 9: independent pixels and metadata PASS");
   }
   ok = 1;
done:
   vkDeviceWaitIdle(vvd.device);
   rwebm_video_stream_close(stream);
   if (ok && !getenv("AV1_TEST_CORE") && reader && reader->held_count && count)
   {
      unsigned replay = retire_index;
      frame = reader->held[--reader->held_count];
      ok = oracle_read_frame(reader, frame, &fixture, &replay);
   }
   if (reader)
      oracle_free(reader);
   rwebm_close(demux);
   free(reader);
   free(sequence);
   free(hash);
   free(data);
   free(raw);
   printf("AV1 Vulkan WebM %s: %s (%u frames)\n", path ? path : "missing",
          ok ? "PASS" : "FAIL", count);
   return ok;
}
