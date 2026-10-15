/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
/* Optional OBU stream plus independent FFmpeg NV12/P010 oracle. */
static int av1_decode_fixture(void)
{
   const char *path = getenv("AV1_TEST_OBU"),
              *raw_path = getenv("AV1_TEST_RAW");
   uint8_t *data = NULL, *raw = NULL;
   rav1_hw_session_t *session = NULL;
   rav1_sequence_t *sequence = NULL;
   rav1_hw_ops_t ops;
   rav1_hw_backend_t backend;
   oracle_readback_t *reader = NULL;
   hevc_fixture_t fixture;
   rav1_obu_t obu;
   uint32_t *hash = NULL;
   size_t size = 0, raw_size = 0, offset = 0, frame_size;
   unsigned count = 0, replays = 0, decodes = 0, tries, i, plane, x, y;
   int ret = -1, ok = 0, started = 0;
   memset(&ops, 0, sizeof(ops));
   memset(&backend, 0, sizeof(backend));
   memset(&fixture, 0, sizeof(fixture));
   fixture.name = "av1";
   if (!path && !raw_path)
      return 1;
   if (!path || !raw_path)
      goto done;
   data = oracle_load(path, &size);
   raw = oracle_load(raw_path, &raw_size);
   sequence = (rav1_sequence_t *)calloc(1, sizeof(*sequence));
   session = (rav1_hw_session_t *)calloc(1, sizeof(*session));
   reader = (oracle_readback_t *)calloc(1, sizeof(*reader));
   if (!data || !raw || !sequence || !session || !reader)
      goto done;
   while ((ret = rav1_obu_next(&obu, data, size, &offset, false)) == 1)
   {
      if (obu.type == 1)
      {
         if (!rav1_sequence_parse(sequence, obu.payload, obu.payload_size))
            goto done;
         if (started)
         {
            if (memcmp(sequence, &session->sequence, sizeof(*sequence)))
               goto done;
            continue;
         }
         fixture.depth = sequence->bit_depth;
         frame_size = (size_t)sequence->max_width * sequence->max_height * 3 /
                      2 * (fixture.depth == 10 ? 2 : 1);
         if (!frame_size || raw_size % frame_size ||
             raw_size / frame_size > 1024)
            goto done;
         fixture.frames = (unsigned)(raw_size / frame_size);
         hash = (uint32_t *)calloc(fixture.frames, sizeof(*hash));
         fixture.hash = hash;
         for (i = 0; i < 6; i++)
         {
            rav1_hw_ops_t refused;
            rav1_hw_backend_t factory;
            session->sequence = *sequence;
            if (i == 0)
               session->sequence.film_grain_params_present = 1;
            else if (i == 1)
               session->sequence.profile = 1;
            else if (i == 2)
               session->sequence.bit_depth = 12;
            else if (i == 3)
               session->sequence.transfer_characteristics = 16;
            else if (i == 4)
               session->sequence.matrix_coefficients = 9;
            if (rav1_hw_backend_open(&session->sequence, i == 5 ? 33 : 9,
                                     &refused, &factory))
            {
               factory.close(&refused);
               goto done;
            }
            if (refused.decoder || refused.decode || refused.output ||
                factory.close)
               goto done;
         }
         if (!hash || !rav1_hw_backend_open(sequence, 9, &ops, &backend) ||
             !rav1_hw_session_init(session, sequence, &ops, 9) ||
             !oracle_init(reader, sequence->max_width, sequence->max_height,
                          fixture.depth))
            goto done;
         for (i = 0; i < fixture.frames; i++)
         {
            const uint8_t *pixels = raw + frame_size * i;
            unsigned bps = fixture.depth == 10 ? 2 : 1;
            hash[i] = UINT32_C(2166136261);
            for (plane = 0; plane < 3; plane++)
               for (y = 0; y < sequence->max_height >> (plane ? 1 : 0); y++)
                  for (x = 0; x < sequence->max_width >> (plane ? 1 : 0); x++)
                  {
                     size_t pos =
                         plane ? ((size_t)sequence->max_width *
                                      sequence->max_height +
                                  (y * (sequence->max_width / 2) + x) * 2 +
                                  plane - 1) *
                                     bps
                               : (y * sequence->max_width + x) * bps;
                     unsigned sample = pixels[pos];
                     if (bps == 2)
                        sample = (sample | (unsigned)pixels[pos + 1] << 8) >> 6;
                     hash[i] = (hash[i] ^ (sample & 255)) * UINT32_C(16777619);
                     if (bps == 2)
                        hash[i] =
                            (hash[i] ^ (sample >> 8)) * UINT32_C(16777619);
                  }
         }
         started = 1;
      }
      if (obu.type != 3 && obu.type != 6)
         continue;
      if (!started)
         goto done;
      {
         unsigned before = ((vvd_dec_t *)ops.decoder)->dring;
         for (tries = 0; tries < 1024; tries++)
         {
            ret = rav1_hw_session_submit(session, obu.data, obu.size);
            if (ret != 1)
               break;
            vkDeviceWaitIdle(vvd.device);
         }
         if (ret)
         {
            printf("AV1 submit at byte %lu: %d\n",
                   (unsigned long)(offset - obu.size), ret);
            goto done;
         }
         memset((void *)obu.data, 0, obu.size);
         if (session->pending.header.show_existing)
         {
            if (((vvd_dec_t *)ops.decoder)->dring != before)
               goto done;
            replays++;
         }
      }
      if (session->output_slot >= 0)
      {
         video_hw_frame_t frame;
         for (tries = 0; tries < 1024; tries++)
         {
            ret = rav1_hw_session_output(session, &frame);
            if (ret != 1)
               break;
            oracle_release(reader);
            reader->pressured++;
            vkDeviceWaitIdle(vvd.device);
         }
         if (ret)
            goto done;
         if (frame.yuv !=
             ((sequence->color_range ? IMAGE_YUV_FLAG_FULL_RANGE : 0) |
              (sequence->matrix_coefficients == 1 ? IMAGE_YUV_FLAG_BT709 : 0)))
         {
            video_hw_frame_abandon(&frame);
            goto done;
         }
         if (!oracle_read_frame(reader, frame, &fixture, &count))
            goto done;
      }
   }
   ok = !ret && started && count == fixture.frames;
#ifndef HEVC_RENDER_TEST
   if (ok && count > VVD_OUTS && !reader->pressured)
      ok = 0;
#endif
done:
   vkDeviceWaitIdle(vvd.device);
   if (ops.decoder)
      decodes = ((vvd_dec_t *)ops.decoder)->dring;
   if (backend.close)
      backend.close(&ops);
   if (ok && reader && reader->held_count && count)
   {
      unsigned replay = count - 1;
      video_hw_frame_t frame = reader->held[--reader->held_count];
      ok = oracle_read_frame(reader, frame, &fixture, &replay) &&
           replay == count;
   }
   if (reader)
      oracle_free(reader);
   free(reader);
   free(sequence);
   free(session);
   free(hash);
   free(data);
   free(raw);
   printf("AV1 Vulkan decoded/exported %s: %s (%u frames, %u decodes, %u "
          "replays)\n",
          path ? path : "missing", ok ? "PASS" : "FAIL", count, decodes,
          replays);
   return ok;
}
