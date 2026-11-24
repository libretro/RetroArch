/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
static bool pyrowave_test_position(void *data, audio_presentation_position_t *position)
{
   size_t fb = audio_layout_channels(dev_layout) * (device_float ? sizeof(float) : sizeof(int16_t));
   (void)data;
   memset(position, 0, sizeof(*position));
   position->epoch = pyrowave_test_epoch;
   position->written = captured_bytes / fb;
   position->played = pyrowave_test_played;
   position->valid = true;
   position->lend_complete = pyrowave_test_complete;
   return true;
}

static void pyrowave_output_case(unsigned rate, bool complete, bool mono)
{
   static int16_t loan[882];
   static float expected_input[882];
   static uint8_t expected[sizeof(captured)];
   size_t expected_bytes = 0, done = 0, i;
   unsigned pass;
   settings_t *settings = config_get_ptr();
   audio_driver_state_t *st = &audio_driver_st;
   record_pyrowave_audio_clock_t *clock;
   record_pyrowave_clock_position_t position;
   size_t fb = 2 * sizeof(float);
   memset(settings, 0, sizeof(*settings));
   settings->bools.audio_enable = true; settings->bools.audio_sync = true;
   settings->uints.audio_output_sample_rate = 48000; settings->uints.audio_latency = 64;
   settings->floats.slowmotion_ratio = 1.0f;
   strcpy(settings->arrays.audio_resampler, "sinc");
   settings->uints.audio_resampler_quality = RESAMPLER_QUALITY_NORMAL;
   dev_layout = AUDIO_LAYOUT_STEREO; virtual_surround = false; core_surround = false;
   device_float = true; span_cap = 0; ring_frames = ring_bytes = ring_cut = 0;
   audio_null.write_begin = test_begin; audio_null.write_end = test_end;
   audio_null.presentation_position = pyrowave_test_position;
   pyrowave_test_epoch = 1; pyrowave_test_played = 0; pyrowave_test_active = false;
   for (i = 0; i < 441; i++)
   {
      loan[i * (mono ? 1 : 2)] = (int16_t)((int)i - 220);
      if (!mono) loan[2 * i + 1] = (int16_t)(220 - (int)i);
      expected_input[2 * i] = loan[i * (mono ? 1 : 2)] * (1.0f / 32768.0f);
      expected_input[2 * i + 1] = loan[i * (mono ? 1 : 2) + (mono ? 0 : 1)] * (1.0f / 32768.0f);
   }
   st->input = rate; st->volume_gain = 1.0f; audio_driver_set_core_float(true);
   video_state_get_ptr()->av_info.timing.fps = 60.0;
   video_state_get_ptr()->av_info.timing.sample_rate = rate;
   if (!audio_driver_init_internal(settings, false)) abort();
   audio_driver_publish_runloop(); captured_bytes = 0;
   audio_driver_sample_batch_float(expected_input, 441);
   expected_bytes = captured_bytes; memcpy(expected, captured, expected_bytes);
   audio_driver_deinit();
   st->input = rate; st->volume_gain = 1.0f;
   if (!audio_driver_init_internal(settings, false)) abort();
   audio_driver_publish_runloop(); captured_bytes = 0;
   begin_calls = end_calls = write_calls = 0;
   pyrowave_test_active = true; pyrowave_test_complete = complete;
   pyrowave_test_limit = complete ? 0 : 23 * fb;
   span_cap = complete ? 19 * fb : 0;
   clock = record_pyrowave_audio_clock_new(1);
   if (!clock) abort();
   for (pass = 0; pass < 100 && done < 441; pass++)
   {
      ssize_t consumed = audio_driver_pyrowave_submit(clock,
            loan + done * (mono ? 1 : 2), 441 - done, mono ? 1 : 2, 1000 + done);
      if (consumed < 0) abort();
      done += (size_t)consumed;
   }
   /* The source loan is gone while the owned output tail is still pending. */
   memset(loan, 0x4a, sizeof(loan));
   for (pass = 0; pass < 100 && record_pyrowave_audio_clock_pending(clock); pass++)
      if (!audio_driver_pyrowave_poll(clock, &position)) abort();
   if (done != 441 || record_pyrowave_audio_clock_pending(clock)
         || captured_bytes != expected_bytes || memcmp(expected, captured, expected_bytes)
         || (complete ? begin_calls != end_calls : begin_calls != 0))
   {
      printf("FAIL PyroWave output %u/%s/%s: consumed %u bytes %u/%u, lends %u/%u\n",
            rate, complete ? "complete lend" : "partial write", mono ? "mono" : "stereo",
            (unsigned)done, (unsigned)captured_bytes, (unsigned)expected_bytes, begin_calls, end_calls);
      failures++;
   }
   if (record_pyrowave_audio_clock_drained(clock))
   { puts("FAIL PyroWave accepted output is not yet played"); failures++; }
   pyrowave_test_played = captured_bytes / fb;
   if (!audio_driver_pyrowave_poll(clock, &position) || !position.valid
         || position.source_sample != 1441 || !record_pyrowave_audio_clock_drained(clock))
   { puts("FAIL PyroWave accepted tail presentation"); failures++; }
   pyrowave_test_limit = 0;
   if (audio_driver_pyrowave_silence(clock) < 0) abort();
   pyrowave_test_played = captured_bytes / fb;
   if (!audio_driver_pyrowave_poll(clock, &position) || position.valid
         || !record_pyrowave_audio_clock_drained(clock))
   { puts("FAIL PyroWave silence must not extrapolate source time"); failures++; }
   pyrowave_test_epoch++;
   if (audio_driver_pyrowave_poll(clock, &position) || !record_pyrowave_audio_clock_failed(clock))
   { puts("FAIL PyroWave discarded native epoch"); failures++; }
   audio_driver_pyrowave_detach(clock); record_pyrowave_audio_clock_free(clock);
   audio_driver_deinit();
   audio_null.presentation_position = NULL; pyrowave_test_active = false;
}

static void pyrowave_output_cases(void)
{
   unsigned mode;
   for (mode = 0; mode < 8; mode++)
      pyrowave_output_case((mode & 1) ? 44100 : 48000, (mode & 2) != 0, (mode & 4) != 0);
   puts("PyroWave output: real resampler bytes, source loan retirement, short writes, native cursor, silence and epoch checks complete");
}
