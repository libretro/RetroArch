/* Small deterministic boundary coverage; exhaustive timing stays local. */
static void producer_span_cases(void)
{
   union { float f[37 * 8]; int16_t i[37 * 8]; } input, saved;
   union { float f[37 * AUDIO_PIPE_CANON_CHANNELS]; int16_t i[37 * AUDIO_PIPE_CANON_CHANNELS]; } expected, actual;
   static const unsigned layouts[] = {AUDIO_LAYOUT_STEREO, AUDIO_LAYOUT_5POINT1,
      AUDIO_LAYOUT_7POINT1, AUDIO_LAYOUT_STEREO | (1u << 8),
      (1u << 4) | (1u << 5)};
   audio_driver_state_t *st = &audio_driver_st;
   unsigned src, dst, layout, wrap, full, before = failures;
   for (src = 0; src < 2; src++)
      for (dst = 0; dst < 2; dst++)
         for (layout = 0; layout < ARRAY_SIZE(layouts); layout++)
            for (wrap = 0; wrap < 3; wrap++)
               for (full = 0; full < 2; full++)
               {
                  unsigned slots[AUDIO_PIPE_CANON_CHANNELS], c, bit, channels = 0;
                  size_t f, start, frames, bytes, used;
                  CHECK(pipe_up(src, dst), "span setup");
                  /* Force a small, non-frame-divisible ring and only seven
                   * frames of available space in the pressure variant. */
                  retro_spsc_free(&st->pipe_ring);
                  CHECK(retro_spsc_init(&st->pipe_ring, 2048), "span ring");
                  for (bit = 0; bit < AUDIO_PIPE_CANON_CHANNELS; bit++)
                     if (layouts[layout] & (1u << bit)) slots[channels++] = bit;
                  memset(&input, 0, sizeof(input));
                  for (f = 0; f < 37 * channels; f++)
                     if (src) input.f[f] = ((int)(f % 33) - 16) / 8.0f;
                     else input.i[f] = (int16_t)(f * 997);
                  saved = input;
                  memset(&expected, 0, sizeof(expected));
                  for (f = 0; f < 37; f++)
                     for (c = 0; c < channels; c++)
                     {
                        size_t in = f * channels + c, out = f * AUDIO_PIPE_CANON_CHANNELS + slots[c];
                        if (dst) expected.f[out] = src ? input.f[in] : input.i[in] / 32768.0f;
                        else if (src) convert_float_to_s16(&expected.i[out], &input.f[in], 1);
                        else expected.i[out] = input.i[in];
                     }
                  start = wrap == 0 ? 0 : st->pipe_ring.capacity - (wrap == 1 ? 2 : 1);
                  bytes = full ? st->pipe_ring.capacity - 7 * st->pipe_frame_bytes : 0;
                  retro_atomic_size_init(&st->pipe_ring.head, start + bytes);
                  retro_atomic_size_init(&st->pipe_ring.tail, start);
                  st->pipe_ring.cached_tail = start;
                  st->pipe_ring.cached_head = start + bytes;
                  CHECK(audio_driver_multi_pipe(st, &input, 37, channels,
                        layouts[layout], src), "span publish rejected");
                  used = retro_spsc_read_avail(&st->pipe_ring) - bytes;
                  frames = full ? 7 : 37;
                  CHECK(used == frames * st->pipe_frame_bytes, "span accepted wrong prefix");
                  CHECK(retro_spsc_skip(&st->pipe_ring, bytes) == bytes, "span pressure release");
                  CHECK(retro_spsc_read(&st->pipe_ring, &actual, used) == used, "span read");
                  CHECK(!memcmp(&actual, &expected, used), "span samples/layout differ");
                  CHECK(!memcmp(&input, &saved, sizeof(input)), "span modified source");
                  audio_driver_deinit_internal(true);
               }
   printf("producer spans: 120 cases, %u failures\n", failures - before);
}

static void unity_clamp_cases(void)
{
   static const uint32_t bits[] = {0, 0x80000000u, 0x7f800000u, 0xff800000u,
      0x7fc00001u, 0xffc00001u, 0x40000000u, 0xc0000000u, 0x3e800000u};
   float input[25], saved[25], expected[25], actual[27];
   unsigned n, i, before = failures;
   for (i = 0; i < 25; i++)
   {
      uint32_t b = bits[i % ARRAY_SIZE(bits)];
      memcpy(input + i, &b, sizeof(b));
      expected[i] = (b & 0x7fffffffu) > 0x7f800000u ? 0.0f
         : input[i] > 1.0f ? 1.0f : input[i] < -1.0f ? -1.0f : input[i];
   }
   memcpy(saved, input, sizeof(input));
   for (n = 0; n <= 25; n++)
   {
      for (i = 0; i < 27; i++) actual[i] = 17.0f;
      audio_driver_copy_clamp(actual + 1, input, n);
      CHECK(actual[0] == 17.0f && actual[n + 1] == 17.0f, "clamp exceeded output span");
      CHECK(!memcmp(actual + 1, expected, n * sizeof(float)), "clamp copy special values");
      CHECK(!memcmp(input, saved, sizeof(input)), "clamp modified borrowed input");
   }
   CHECK(up(true, AUDIO_LAYOUT_STEREO, true), "unity clamp frontend setup");
   audio_driver_st.src_ratio_orig = audio_driver_st.src_ratio_curr = 1.0;
   AUDIO_FLAGS_CLEAR(&audio_driver_st, AUDIO_FLAG_CONTROL | AUDIO_FLAG_MIXER_ACTIVE);
   audio_driver_flush(&audio_driver_st, 1.0f, input, 24, true, false, false);
   CHECK(cap_frames == 12 && !memcmp(cap, expected, 24 * sizeof(float)), "unity frontend sanitization");
   CHECK(!memcmp(input, saved, sizeof(input)), "unity frontend modified source");
   audio_driver_deinit_internal(true);
   printf("unity copy clamp: 27 cases, %u failures\n", failures - before);
}
