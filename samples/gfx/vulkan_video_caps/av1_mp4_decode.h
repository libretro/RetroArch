/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include <formats/rmp4_video.h>
#include "av1_core_decode.h"

static int av1_mp4_decode_fixture(void)
{
   const char *path = getenv("AV1_TEST_MP4"),
              *raw_path = getenv("AV1_TEST_RAW");
   uint8_t *data = NULL, *raw = NULL;
   size_t size = 0, raw_size = 0, frame_size;
   rmp4_t *demux = NULL;
   rmp4_video_stream_t *stream = NULL;
   rav1_sequence_t *sequence = NULL;
   oracle_readback_t *reader = NULL;
   rav1_config_t config;
   hevc_fixture_t fixture;
   uint32_t *hash = NULL;
   video_hw_frame_t frame;
   const rmp4_track *track = NULL;
   unsigned count = 0, i, plane, x, y, bps, tries, probe;
   int ok = 0, ret;
   memset(&fixture, 0, sizeof(fixture));
   fixture.name = "av1_mp4";
   if (!path || !raw_path)
      goto done;
   data = oracle_load(path, &size);
   raw = oracle_load(raw_path, &raw_size);
   sequence = (rav1_sequence_t *)calloc(1, sizeof(*sequence));
   reader = (oracle_readback_t *)calloc(1, sizeof(*reader));
   if (!data || !raw || !sequence || !reader)
      goto done;
   demux = rmp4_open_memory(data, size);
   if (!demux)
      goto done;
   for (i = 0; i < (unsigned)rmp4_num_tracks(demux); i++)
   {
      track = rmp4_get_track(demux, (int)i);
      if (track && track->codec == RMP4_CODEC_AV1)
         break;
   }
   if (!track || track->codec != RMP4_CODEC_AV1 ||
       !rav1_config_parse(&config, track->codec_private,
                          track->codec_private_size) ||
       !rav1_config_sequence(sequence, &config))
      goto done;
   fixture.depth = sequence->bit_depth;
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
   stream = rmp4_video_stream_open_hw_avail(data, size, size, NULL, NULL, NULL);
   if (!stream || !rmp4_video_stream_set_hw(stream, 1))
      goto done;
   for (tries = 0; tries < 4096; tries++)
   {
      if (!rmp4_video_stream_next(stream, NULL))
      {
         if (!rmp4_video_stream_waiting(stream))
            break;
         oracle_release(reader);
         vkDeviceWaitIdle(vvd.device);
         continue;
      }
      if (!rmp4_video_stream_hw_frame(stream, &frame) ||
          !oracle_read_frame(reader, frame, &fixture, &count))
         goto done;
   }
   if (count != fixture.frames)
      goto done;
   if (count > 2)
   {
      oracle_release(reader);
      rmp4_video_stream_rewind(stream);
      for (probe = 0; probe < 2; probe++)
      {
         for (tries = 0; tries < 4096; tries++)
         {
            ret = rmp4_video_stream_discard(stream, NULL);
            if (ret != 2)
               break;
            oracle_release(reader);
            vkDeviceWaitIdle(vvd.device);
         }
         if (ret != 1 || rmp4_video_stream_hw_frame(stream, &frame))
            goto done;
      }
      for (tries = 0; tries < 4096; tries++)
      {
         if (rmp4_video_stream_next(stream, NULL))
            break;
         if (!rmp4_video_stream_waiting(stream))
            goto done;
         oracle_release(reader);
         vkDeviceWaitIdle(vvd.device);
      }
      probe = 2;
      if (!rmp4_video_stream_hw_frame(stream, &frame) ||
          !oracle_read_frame(reader, frame, &fixture, &probe) || probe != 3)
         goto done;
   }
   ok = 1;
done:
   vkDeviceWaitIdle(vvd.device);
   rmp4_video_stream_close(stream);
   if (ok && !getenv("AV1_TEST_CORE") && reader && reader->held_count && count)
   {
      unsigned replay = count > 2 ? 2 : count - 1;
      frame = reader->held[--reader->held_count];
      ok = oracle_read_frame(reader, frame, &fixture, &replay);
   }
   if (reader)
      oracle_free(reader);
   rmp4_close(demux);
   free(reader);
   free(sequence);
   free(hash);
   free(data);
   free(raw);
   printf("AV1 Vulkan MP4 %s: %s (%u frames)\n", path ? path : "missing",
          ok ? "PASS" : "FAIL", count);
   return ok;
}
