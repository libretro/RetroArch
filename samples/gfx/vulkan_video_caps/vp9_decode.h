/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
/* Optional IVF plus independent FFmpeg NV12/P010 oracle. */
static uint32_t vp9_u32(const uint8_t *p)
{
   return (uint32_t)p[0] | (uint32_t)p[1] << 8
      | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int vp9_decode_fixture(void)
{
   const char *path = getenv("VP9_TEST_IVF"), *raw_path = getenv("VP9_TEST_RAW");
   uint8_t *ivf = NULL, *raw = NULL;
   rvp9_hw_session_t *session = NULL;
   rvp9_hw_ops_t ops = {0};
   rvp9_hw_backend_t backend = {0};
   rvp9_hdr header;
   oracle_readback_t reader = {0};
   hevc_fixture_t fixture;
   uint32_t *hash = NULL;
   size_t size, raw_size, length = 0, offset, frame_size;
   unsigned count = 0, tries, i, plane, x, y;
   int ret, ok = 0;
   memset(&fixture, 0, sizeof(fixture)); fixture.name = "vp9";
   if (!path && !raw_path) return 1;
   if (!path || !raw_path) goto done;
   ivf = oracle_load(path, &size); raw = oracle_load(raw_path, &raw_size);
   if (!ivf || !raw || size < 44 || memcmp(ivf, "DKIF", 4) || memcmp(ivf + 8, "VP90", 4)) goto done;
   length = vp9_u32(ivf + 32);
   if (length > size - 44 || rvp9_parse_frame_header(ivf + 44, length, NULL, NULL, &header)) goto done;
   if ((header.w | header.h) & 1) goto done;
   fixture.depth = header.bit_depth;
   frame_size = (size_t)header.w * header.h * 3 / 2 * (fixture.depth == 10 ? 2 : 1);
   if (!frame_size || raw_size % frame_size || raw_size / frame_size > 1024) goto done;
   fixture.frames = (unsigned)(raw_size / frame_size);
   hash = (uint32_t*)calloc(fixture.frames, sizeof(*hash)); fixture.hash = hash;
   session = (rvp9_hw_session_t*)calloc(1, sizeof(*session));
   if (!hash || !session || !rvp9_hw_backend_open(&header, 10, &ops, &backend)
         || !rvp9_hw_session_init(session, &ops, 10)
         || !oracle_init(&reader, (unsigned)header.w, (unsigned)header.h, fixture.depth)) goto done;
   for (i = 0; i < fixture.frames; i++)
   {
      const uint8_t *data = raw + frame_size * i;
      unsigned bps = fixture.depth == 10 ? 2 : 1;
      hash[i] = UINT32_C(2166136261);
      for (plane = 0; plane < 3; plane++)
         for (y = 0; y < (unsigned)header.h >> (plane ? 1 : 0); y++)
            for (x = 0; x < (unsigned)header.w >> (plane ? 1 : 0); x++)
            {
               size_t pos = plane ? ((size_t)header.w * header.h + (y * (header.w / 2) + x) * 2 + plane - 1) * bps
                  : (y * header.w + x) * bps;
               unsigned sample = data[pos];
               if (bps == 2) sample = (sample | (unsigned)data[pos + 1] << 8) >> 6;
               hash[i] = (hash[i] ^ (sample & 255)) * UINT32_C(16777619);
               if (bps == 2) hash[i] = (hash[i] ^ (sample >> 8)) * UINT32_C(16777619);
            }
   }
   for (offset = 32; offset < size; offset += length)
   {
      video_hw_frame_t frame;
      if (size - offset < 12) goto done;
      length = vp9_u32(ivf + offset); offset += 12;
      if (!length || length > size - offset) goto done;
      for (tries = 0; tries < 1024; tries++)
      {
         ret = rvp9_hw_session_submit(session, ivf + offset, length);
         if (ret != 1) break;
         vkDeviceWaitIdle(vvd.device);
      }
      if (ret) goto done;
      if (session->output_slot < 0) continue;
      for (tries = 0; tries < 1024; tries++)
      {
         ret = rvp9_hw_session_output(session, &frame);
         if (ret != 1) break;
         oracle_release(&reader); reader.pressured++;
         vkDeviceWaitIdle(vvd.device);
      }
      if (ret) goto done;
      if (frame.yuv != ((session->header.color_range ? IMAGE_YUV_FLAG_FULL_RANGE : 0)
               | (session->header.color_space == 2 ? IMAGE_YUV_FLAG_BT709 : 0)))
      { video_hw_frame_abandon(&frame); goto done; }
      if (!oracle_read_frame(&reader, frame, &fixture, &count)) goto done;
   }
   ok = count == fixture.frames;
   for (i = 0; i < RVP9_REF_FRAMES; i++)
      if (session->reference_slots[i] == session->previous_slot) break;
   if (ok && count && i < RVP9_REF_FRAMES)
   {
      video_hw_frame_t frame;
      uint8_t existing = (uint8_t)((header.profile == 2 ? 0x98 : 0x88) | i);
      unsigned replay = count - 1, before = ((vvd_dec_t*)ops.decoder)->dring;
      oracle_release(&reader);
      ret = rvp9_hw_session_submit(session, &existing, 1);
      if (!ret) ret = rvp9_hw_session_output(session, &frame);
      ok = !ret && before == ((vvd_dec_t*)ops.decoder)->dring
         && oracle_read_frame(&reader, frame, &fixture, &replay) && replay == count;
   }
done:
   vkDeviceWaitIdle(vvd.device);
   if (backend.close) backend.close(&ops);
#ifndef HEVC_RENDER_TEST
   if (ok && reader.held_count && count)
   {
      unsigned replay = count - 1;
      video_hw_frame_t frame = reader.held[--reader.held_count];
      ok = oracle_read_frame(&reader, frame, &fixture, &replay) && replay == count;
   }
#endif
   oracle_free(&reader);
   free(session); free(hash); free(ivf); free(raw);
   printf("VP9 Vulkan decoded/exported %s: %s (%u frames)\n", path ? path : "missing",
         ok ? "PASS" : "FAIL", count);
   return ok;
}
