/* Configured wide inline transport before native channel routing. */
static double raw_speed_adjust;
static unsigned raw_speed_calls;
static ssize_t raw_speed_accepted;
static int16_t raw_speed_samples[514];
static ssize_t raw_speed_write(void *data, const int16_t *samples,
      size_t frames, unsigned rate, double adjust, float gain)
{
   (void)data;
   CHECK(frames == 257 && rate == 44100 && gain == 1.0f,
         "raw speed changed native block metadata");
   if (frames == 257) memcpy(raw_speed_samples, samples, sizeof(raw_speed_samples));
   raw_speed_adjust = adjust;
   raw_speed_calls++;
   return raw_speed_accepted;
}

static void raw_speed_cases(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings = config_get_ptr();
   unsigned mode, f, before = failures;
   int16_t input[514], expected[514];
   for (f = 0; f < 514; f++) input[f] = (int16_t)((int)(f*719%12000)-6000);
   for (mode = 0; mode < 5; mode++)
   {
      bool complete = false;
      CHECK(pipe_up(false, false), "raw speed stand-up");
      dev_channels = st->out_channels = st->pipe_channels = 2;
      dev_layout = st->out_layout = AUDIO_LAYOUT_STEREO;
      st->pipe_frame_bytes = 2*sizeof(int16_t);
      scripted_threaded.write_raw = raw_speed_write;
      settings->bools.audio_fastforward_speedup = mode != 3;
      settings->bools.audio_time_stretch = false;
      settings->bools.audio_time_stretch_lowpass = mode == 4;
      settings->floats.slowmotion_ratio = 2;
      runloop_state_get_ptr()->flags = mode == 4 ? RUNLOOP_FLAG_FASTMOTION : 0;
      retro_atomic_store_release_int(&st->pipe_ff_mult_q16, 32768);
      audio_driver_publish_runloop();
      raw_speed_calls = 0;
      raw_speed_accepted = 257;
      memcpy(expected, input, sizeof(input));
      if (mode == 4)
      {
         audio_speed_lpf_t lpf;
         CHECK(audio_driver_transport_configure(settings) && st->pipe_transport,
               "raw filter-only preparation");
         audio_speed_lpf_init(&lpf, 44100, 2, false);
         audio_speed_lpf_set(&lpf, true, audio_speed_lpf_cutoff(44100, 131072));
         audio_speed_lpf_process(&lpf, expected, 257);
         audio_driver_submit(st, 1.0f, input, 514, false, false, true, true);
         audio_driver_publish_runloop();
         CHECK(audio_driver_pipeline_transport_step(st->pipe_transport,
               &st->pipe_transport_serial, 257, 257, false, &complete), "raw filter-only step");
      }
      else
      {
         if (!mode) { st->pipe_threaded = false; st->last_flush_time = 123; }
         audio_driver_flush(st, 2.0f, input, 514, false, mode == 2, mode != 0);
         CHECK(mode || !st->last_flush_time, "raw speed release retained cadence");
         st->pipe_threaded = true;
      }
      CHECK(raw_speed_calls == 1 && raw_speed_adjust == ((mode == 1 || mode == 4) ? 0.5 : 1.0),
            "raw speed multiplier: mode %u got %.8f", mode, raw_speed_adjust);
      CHECK(!memcmp(raw_speed_samples, expected, sizeof(expected)) && !st->stat_frontend_is_float,
            "raw speed changed native samples");
      CHECK(st->sink_offered_raw == (uint64_t)(257.0*48000.0/44100.0*raw_speed_adjust)
            && st->sink_accepted == st->sink_offered_raw,
            "fully accepted raw write reported dropped output frames");
      if (mode == 4)
      {
         static const ssize_t accepted[] = {128, 0, -1};
         unsigned a;
         for (a = 0; a < ARRAY_SIZE(accepted); a++)
         {
            st->sink_offered_raw = st->sink_accepted = 0;
            raw_speed_accepted = accepted[a];
            audio_driver_flush(st, 1.0f, input, 514, false, false, true);
            CHECK(st->sink_offered_raw == (uint64_t)(257.0*48000.0/44100.0*0.5)
                  && st->sink_accepted == (accepted[a] > 0
                     ? (uint64_t)(accepted[a]*48000.0/44100.0*0.5) : 0),
                  "partial/failed raw write counted the wrong output frames");
         }
      }
      audio_driver_deinit_internal(true);
   }
   settings->bools.audio_time_stretch_lowpass = settings->bools.audio_fastforward_speedup = false;
   settings->floats.slowmotion_ratio = 1;
   runloop_state_get_ptr()->flags = 0;
   audio_driver_publish_runloop();
   printf("raw native speed: 5 cases, %u failures\n", failures - before);
}

static void independent_lpf_cases(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings = config_get_ptr();
   unsigned native, speed, mode, before = failures;
   static const float durations[] = {1.0f, 0.5f, 2.0f};
   union { float f[514]; int16_t i[514]; } input, filtered;
   for (native = 0; native < 2; native++)
   {
      /* Real queued consumer, stereo/5.1 and short device writes. */
      transport_quality_case(native, false, false, 131072, true);
      transport_quality_case(native, true, true, 131072, true);
      for (speed = 0; speed < ARRAY_SIZE(durations); speed++)
      {
         float *reference = NULL;
         size_t reference_frames = 0;
         for (mode = 0; mode < 2; mode++)
         {
            audio_speed_lpf_t lpf;
            struct audio_inline_transport *saved;
            unsigned chunk, allocations;
            uint32_t tempo = (uint32_t)(65536.0 / durations[speed]);
            uint32_t cutoff;
            CHECK(up(native, AUDIO_LAYOUT_STEREO, native), "LPF-only stand-up");
            free(cap); cap = NULL; cap_cap = cap_frames = 0;
            if (!native)
            {
               settings->bools.audio_fastpath_s16 = true;
               st->resampler_data_int16 = audio_driver_int16_resampler_new(st);
               st->resampler_int16_process = sinc_resampler_int16_process;
               st->resampler_int16_free = sinc_resampler_int16_free;
               st->resampler_int16_reset = sinc_resampler_int16_reset;
            }
            snap_pause(false);
            settings->bools.audio_time_stretch = false;
            settings->bools.audio_time_stretch_lowpass = mode != 0;
            settings->floats.slowmotion_ratio = durations[speed];
            runloop_state_get_ptr()->flags = RUNLOOP_FLAG_SLOWMOTION;
            audio_driver_publish_runloop();
            CHECK(audio_driver_transport_configure(settings), "LPF-only configure");
            saved = st->inline_transport;
            CHECK((saved != NULL) == (mode != 0), "LPF-only preparation policy");
            audio_speed_lpf_init(&lpf, (unsigned)st->input, 2, native);
            cutoff = audio_speed_lpf_cutoff((unsigned)st->input, tempo);
            if (cutoff) audio_speed_lpf_set(&lpf, true, cutoff);
            allocations = transport_allocations; transport_track = true;
            for (chunk = 0; chunk < 32; chunk++)
            {
               size_t f;
               const void *source = &input;
               for (f = 0; f < 257; f++)
               {
                  int16_t v = (int16_t)(12000 * sin(2*M_PI*8000*(chunk*257+f)/44100.0));
                  if (native) { input.f[2*f] = v/32768.0f; input.f[2*f+1] = -v/32768.0f; }
                  else { input.i[2*f] = v; input.i[2*f+1] = -v; }
               }
               /* Independent composition: native LPF followed by the
                * ordinary frontend/SRC, with pitch preservation disabled. */
               if (!mode && cutoff)
               {
                  CHECK(audio_speed_lpf_process_into(&lpf, &input, &filtered, 257), "reference LPF");
                  source = &filtered;
               }
               CHECK((native ? audio_driver_sample_batch_float((const float*)source, 257)
                        : audio_driver_sample_batch((const int16_t*)source, 257)) == 257,
                     "LPF-only source accounting");
            }
            CHECK(st->stat_frontend_is_float == (bool)native && cap_frames > 2048,
                  "LPF-only lost native output");
            CHECK(allocations == transport_allocations, "LPF-only grew prepared storage");
            transport_track = false;
            if (!mode)
            {
               reference_frames = cap_frames;
               reference = (float*)malloc(cap_frames*2*sizeof(float));
               if (!reference) exit(1);
               memcpy(reference, cap, cap_frames*2*sizeof(float));
            }
            else
            {
               CHECK(cap_frames == reference_frames
                     && !memcmp(reference, cap, cap_frames*2*sizeof(float)),
                     "LPF-only differs from native LPF plus ordinary SRC");
               CHECK(saved && audio_stretch_stream_quiescent(saved->stream)
                     && saved->cutoff == cutoff, "LPF-only retained WSOLA history");
               CHECK(audio_driver_stop() && audio_driver_start(false), "LPF-only stop/restart");
            }
            audio_driver_deinit_internal(true);
         }
         free(reference);
      }
   }
   settings->bools.audio_time_stretch_lowpass = settings->bools.audio_fastpath_s16 = false;
   settings->floats.slowmotion_ratio = 1;
   runloop_state_get_ptr()->flags = 0;
   audio_driver_publish_runloop();
   printf("independent native lowpass: 10 cases, %u failures\n", failures - before);
}

extern bool test_frame_reversed;
extern struct state_manager_rewind_state *test_rewind_state;
extern uint32_t test_rewind_core_frame;
extern unsigned test_rewind_restores;
static retro_audio_sample_t rewind_core_sample;
static retro_audio_sample_batch_t rewind_core_batch;
static void rewind_set_sample(retro_audio_sample_t cb) { rewind_core_sample = cb; }
static void rewind_set_batch(retro_audio_sample_batch_t cb) { rewind_core_batch = cb; }

static void rewind_state_cases(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings = config_get_ptr();
   unsigned native, tick, before = failures;
   union { float f[257*6]; int16_t i[514]; } input;
   union { float f[514]; int16_t i[514]; } storage, fold, expected;
   for (native = 0; native < 2; native++)
   {
      struct state_manager_rewind_state rewind_st;
      struct retro_core_t core;
      struct audio_inline_transport *saved;
      char message[64];
      unsigned duration = 0, allocations;
      memset(&rewind_st, 0, sizeof(rewind_st));
      memset(&core, 0, sizeof(core));
      core.retro_set_audio_sample = rewind_set_sample;
      core.retro_set_audio_sample_batch = rewind_set_batch;
      rewind_core_sample = audio_driver_sample;
      rewind_core_batch = audio_driver_sample_batch;
      CHECK(up(native, AUDIO_LAYOUT_STEREO, native), "state rewind stand-up");
      free(cap); cap = NULL; cap_cap = cap_frames = 0;
      if (!native)
      {
         settings->bools.audio_fastpath_s16 = true;
         st->resampler_data_int16 = audio_driver_int16_resampler_new(st);
         st->resampler_int16_process = sinc_resampler_int16_process;
         st->resampler_int16_free = sinc_resampler_int16_free;
         st->resampler_int16_reset = sinc_resampler_int16_reset;
      }
      snap_pause(false);
      st->core_multi = native;
      settings->bools.audio_time_stretch = true;
      settings->floats.slowmotion_ratio = 2;
      runloop_state_get_ptr()->flags = RUNLOOP_FLAG_SLOWMOTION;
      audio_driver_publish_runloop();
      CHECK(audio_driver_transport_configure(settings), "state rewind transport");
      saved = st->inline_transport;
      if (!saved) exit(1);
      st->rewind_buf = native ? NULL : storage.i;
      st->rewind_buf_f = native ? storage.f : NULL;
      st->rewind_size = 514; st->rewind_ptr = 514;
      test_rewind_state = &rewind_st;
      test_rewind_core_frame = test_rewind_restores = 0;
      state_manager_event_init(&rewind_st, 4096);
      CHECK(rewind_st.state != NULL, "real state manager initialization");
      if (!rewind_st.state) exit(1);
      allocations = transport_allocations; transport_track = true;
      for (tick = 0; tick < 9; tick++)
      {
         bool reverse = tick >= 6;
         size_t f, c, sample = native ? sizeof(float) : sizeof(int16_t);
         state_manager_check_rewind(&rewind_st, &core, reverse, 1, false,
               message, sizeof(message), &duration);
         CHECK(state_manager_frame_is_reversed() == reverse, "state manager reversal flag");
         CHECK(rewind_core_batch == (reverse ? audio_driver_sample_batch_rewind : audio_driver_sample_batch)
               && rewind_core_sample == (reverse ? audio_driver_sample_rewind : audio_driver_sample),
               "state manager callback binding");
         CHECK(test_rewind_core_frame == (reverse ? 11-tick : tick), "savestate restored wrong source frame");
         for (f = 0; f < 257; f++)
            for (c = 0; c < (native ? 6u : 2u); c++)
            {
               int16_t v = (int16_t)((int)((test_rewind_core_frame*719 + f*127 + c*733)%12000)-6000);
               if (native) input.f[f*6+c] = v / 32768.0f + 0.000001f;
               else input.i[f*2+c] = v;
            }
         if (native) audio_downmix_f32(fold.f, input.f, 257, AUDIO_LAYOUT_5POINT1, 6);
         else memcpy(&fold, &input, 514*sample);
         for (f = 0; f < 257; f++)
            memcpy((uint8_t*)&expected + 2*f*sample, (uint8_t*)&fold + 2*(256-f)*sample, 2*sample);
         if (native) audio_driver_sample_batch_multi_float(input.f, 257, 6, AUDIO_LAYOUT_5POINT1);
         else rewind_core_batch(input.i, 257);
         if (reverse)
            CHECK(!st->rewind_ptr && !memcmp(&storage, &expected, 514*sample),
                  "restored core produced wrong native reverse capture");
         test_rewind_core_frame++;
         audio_driver_frame_end();
      }
      state_manager_check_rewind(&rewind_st, &core, false, 1, false,
            message, sizeof(message), &duration);
      CHECK(!state_manager_frame_is_reversed() && test_rewind_restores == 3
            && rewind_core_batch == audio_driver_sample_batch, "rewind release did not restore forward audio");
      if (native) audio_driver_sample_batch_multi_float(input.f, 257, 6, AUDIO_LAYOUT_5POINT1);
      else rewind_core_batch(input.i, 257);
      audio_driver_frame_end();
      CHECK(cap_frames > 2048 && st->stat_frontend_is_float == (bool)native,
            "state rewind lost native device output");
      CHECK(st->inline_transport == saved && !saved->bypassed
            && allocations == transport_allocations, "state rewind did not recover prepared transport");
      transport_track = false;
      state_manager_event_deinit(&rewind_st, &core);
      CHECK(!rewind_st.state && rewind_core_sample == audio_driver_sample
            && rewind_core_batch == audio_driver_sample_batch, "state manager teardown binding");
      test_rewind_state = NULL;
      st->rewind_buf = NULL; st->rewind_buf_f = NULL;
      audio_driver_deinit_internal(true);
      settings->bools.audio_time_stretch = settings->bools.audio_fastpath_s16 = false;
      settings->floats.slowmotion_ratio = 1;
      runloop_state_get_ptr()->flags = 0;
      audio_driver_publish_runloop();
   }
   printf("state manager native audio: 2 cases, %u failures\n", failures - before);
}

static void rewind_boundary_cases(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings = config_get_ptr();
   int16_t storage_i[32];
   float storage_f[32];
   unsigned native, mode, before = failures;
   bool enabled = settings->bools.audio_enable;
   for (native = 0; native < 2; native++)
      for (mode = 0; mode < 3; mode++)
      {
         CHECK(mode == 2 ? pipe_up(native, native) : up(native, AUDIO_LAYOUT_STEREO, native),
               "rewind boundary stand-up");
         if (mode == 1)
         {
            settings->bools.audio_time_stretch = true;
            CHECK(audio_driver_transport_configure(settings), "rewind boundary transport");
         }
         memset(storage_i, 0x5a, sizeof(storage_i));
         memset(storage_f, 0, sizeof(storage_f));
         st->rewind_buf = storage_i; st->rewind_buf_f = storage_f;
         st->rewind_size = 32;
         audio_driver_setup_rewind();
         audio_driver_sample_rewind(1234, -4321);
         audio_driver_set_core_float(native);
         CHECK(st->rewind_ptr == 30, "unchanged format discarded captured rewind");
         audio_driver_set_core_float(!native);
         CHECK(st->rewind_ptr == st->rewind_size, "changed format reinterpreted captured rewind");
         test_frame_reversed = true;
         audio_driver_frame_is_reverse();
         test_frame_reversed = false;
         CHECK(!cap_frames && !st->extra.pending
               && (mode != 2 || !retro_spsc_read_avail(&st->pipe_ring)),
               "empty rewind produced device or queued output");
         CHECK(mode != 1 || (st->inline_transport && !st->inline_transport->bypassed),
               "empty rewind reset prepared transport");
         audio_driver_sample_rewind(2345, -5432);
         audio_driver_set_core_float(!native);
         CHECK(st->rewind_ptr == 30 && (native
                  ? storage_i[30] == 2345 && storage_i[31] == -5432
                  : storage_f[30] == 2345 / 32768.0f && storage_f[31] == -5432 / 32768.0f),
               "new rewind format retained stale prefix");
         st->rewind_buf = NULL; st->rewind_buf_f = NULL;
         audio_driver_deinit_internal(true);
         CHECK(!st->rewind_ptr && !st->rewind_size, "rewind teardown retained a cursor");
         settings->bools.audio_time_stretch = false;
      }
   /* Exercise actual arena allocation without opening a physical device. */
   settings->bools.audio_enable = false;
   for (native = 0; native < 2; native++)
   {
      st->core_float = native;
      st->rewind_ptr = 14;
      CHECK(!audio_driver_init_internal(settings, false), "disabled audio unexpectedly started");
      CHECK(st->rewind_buf && st->rewind_buf_f && st->rewind_size
            && st->rewind_ptr == st->rewind_size, "new rewind arena is not empty");
      audio_driver_deinit_internal(true);
      CHECK(!st->rewind_ptr && !st->rewind_size, "allocated rewind teardown retained a cursor");
   }
   settings->bools.audio_enable = enabled;
   printf("rewind lifecycle boundaries: 8 cases, %u failures\n", failures - before);
}

static void rewind_mixed_cases(void)
{
   int16_t input[257*6], folded[514], storage_i[514];
   float input_f[514], expected[514], storage_f[514];
   audio_driver_state_t *st = &audio_driver_st;
   unsigned kind, capture, f, c, before = failures;
   float *reference = NULL;
   size_t reference_frames = 0;
   for (f = 0; f < 257; f++)
   {
      for (c = 0; c < 6; c++) input[f*6+c] = (int16_t)((int)((f*127+c*733)%10000)-5000);
      input_f[2*f] = f * 0.000321f + 0.000001f;
      input_f[2*f+1] = f * -0.000123f - 0.000002f;
   }
   audio_downmix_s16(folded, input, 257, AUDIO_LAYOUT_5POINT1, 6);
   for (f = 0; f < 257; f++)
      for (c = 0; c < 2; c++)
         expected[2*(256-f)+c] = (f < 7 || (f >= 120 && f < 127))
            ? input_f[2*f+c] : folded[2*f+c] / 32768.0f;
   for (kind = 0; kind < 3; kind++)
      for (capture = 0; capture < 2; capture++)
      {
         CHECK(up(true, AUDIO_LAYOUT_STEREO, true), "mixed rewind stand-up");
         free(cap); cap = NULL; cap_cap = cap_frames = 0;
         memset(storage_i, 0x5a, sizeof(storage_i));
         memset(storage_f, 0, sizeof(storage_f));
         st->rewind_buf = storage_i; st->rewind_buf_f = storage_f;
         st->rewind_size = 514;
         audio_driver_setup_rewind();
         test_frame_reversed = true;
         if (capture)
         {
            audio_driver_sample_batch_float(input_f, 7);
            AUDIO_FLAGS_SET(st, AUDIO_FLAG_SUSPENDED);
            if (!kind) audio_driver_sample_rewind(32000, -32000);
            else if (kind == 1)
               CHECK(audio_driver_sample_batch_rewind(folded, 17) == 17, "suspended rewind accounting");
            else audio_driver_sample_batch_multi_int16(input, 17, 6, AUDIO_LAYOUT_5POINT1);
            CHECK(st->rewind_ptr == 500 && !cap_frames, "suspended mixed rewind retained speculative input");
            AUDIO_FLAGS_CLEAR(st, AUDIO_FLAG_SUSPENDED);
            for (f = 7; f < 257; )
            {
               unsigned n = f == 7 ? 113 : 130;
               if (!kind)
                  for (c = 0; c < n; c++)
                     audio_driver_sample_rewind(folded[2*(f+c)], folded[2*(f+c)+1]);
               else if (kind == 1) audio_driver_sample_batch_rewind(folded + 2*f, n);
               else audio_driver_sample_batch_multi_int16(input + 6*f, n, 6, AUDIO_LAYOUT_5POINT1);
               f += n;
               if (f == 120)
               {
                  audio_driver_sample_batch_float(input_f + 2*f, 7);
                  f += 7;
               }
            }
            CHECK(!st->rewind_ptr && !cap_frames, "mixed rewind lost frames or wrote early");
            CHECK(!memcmp(storage_f, expected, sizeof(expected)), "mixed rewind kind %u split native history", kind);
            for (f = 0; f < 514; f++)
               if (storage_i[f] != 0x5a5a) break;
            CHECK(f == 514, "mixed rewind wrote the inactive int16 arena");
            audio_driver_frame_is_reverse();
         }
         else audio_driver_submit(st, 1.0f, expected, 514, true, false, false, true);
         test_frame_reversed = false;
         CHECK(cap_frames > 128, "mixed rewind produced no audible output");
         if (!capture)
         {
            reference_frames = cap_frames;
            reference = (float*)malloc(cap_frames * 2 * sizeof(float));
            if (!reference) exit(1);
            memcpy(reference, cap, cap_frames * 2 * sizeof(float));
         }
         else
         {
            CHECK(cap_frames == reference_frames
                  && !memcmp(reference, cap, cap_frames * 2 * sizeof(float)), "mixed rewind playback mismatch");
            free(reference); reference = NULL;
         }
         st->rewind_buf = NULL; st->rewind_buf_f = NULL;
         st->rewind_size = st->rewind_ptr = 0;
         audio_driver_deinit_internal(true);
      }
   printf("mixed rewind format: 3 cases, %u failures\n", failures - before);
}

static void multi_rewind_cases(void)
{
   static const unsigned layouts[] = { AUDIO_LAYOUT_STEREO, AUDIO_LAYOUT_5POINT1, AUDIO_LAYOUT_7POINT1 };
   union { float f[1057*8]; int16_t i[1057*8]; } input;
   union { float f[1057*2]; int16_t i[1057*2]; } folded;
   union { float f[1031*2+2]; int16_t i[1031*2+2]; } storage;
   union { float f[1031*2]; int16_t i[1031*2]; } expected;
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings = config_get_ptr();
   recording_state_t *rs = recording_state_get_ptr();
   unsigned native, mode, l, before = failures;
   for (native = 0; native < 2; native++)
      for (mode = 0; mode < 3; mode++)
         for (l = 0; l < 3; l++)
         {
            unsigned channels = audio_layout_channels(layouts[l]), c, allocations;
            size_t f, sample = native ? sizeof(float) : sizeof(int16_t);
            CHECK(mode == 2 ? pipe_up(native, native) : up(native, AUDIO_LAYOUT_7POINT1, native),
                  "multi rewind stand-up");
            free(cap); cap = NULL; cap_cap = cap_frames = 0;
            st->core_multi = true;
            if (mode == 1)
            {
               settings->bools.audio_time_stretch = true;
               CHECK(audio_driver_transport_configure(settings), "multi rewind prepare");
            }
            CHECK(audio_driver_multi_fold_room(st, 1024, sample), "multi rewind fold reserve");
            for (f = 0; f < 1057; f++)
               for (c = 0; c < channels; c++)
               {
                  int16_t v = (int16_t)((int)((f * 127 + c * 733) % 10000) - 5000);
                  if (native) input.f[f*channels+c] = v / 32768.0f + 0.000001f;
                  else input.i[f*channels+c] = v;
               }
            if (native) audio_downmix_f32(folded.f, input.f, 1057, layouts[l], channels);
            else audio_downmix_s16(folded.i, input.i, 1057, layouts[l], channels);
            for (f = 0; f < 1031; f++)
               memcpy((uint8_t*)&expected + f*2*sample,
                     (const uint8_t*)&folded + (1030-f)*2*sample, 2*sample);
            memset(&storage, 0x5a, sizeof(storage));
            st->rewind_buf = native ? NULL : storage.i + 1;
            st->rewind_buf_f = native ? storage.f + 1 : NULL;
            st->rewind_size = 1031*2;
            audio_driver_setup_rewind();
            rs->driver = &rec_driver; rs->data = rs;
            rs->layout = AUDIO_LAYOUT_STEREO; rs->channels = rec_channels = 2;
            rec_frames = 0;
            test_frame_reversed = true;
            canonical_track = true; allocations = canonical_reallocations;
            AUDIO_FLAGS_SET(st, AUDIO_FLAG_SUSPENDED);
            if (native) audio_driver_sample_batch_multi_float(input.f, 17, channels, layouts[l]);
            else audio_driver_sample_batch_multi_int16(input.i, 17, channels, layouts[l]);
            CHECK(st->rewind_ptr == st->rewind_size, "suspended multi rewind retained input");
            AUDIO_FLAGS_CLEAR(st, AUDIO_FLAG_SUSPENDED);
            CHECK((native ? audio_driver_sample_batch_multi_float(input.f, 113, channels, layouts[l])
                  : audio_driver_sample_batch_multi_int16(input.i, 113, channels, layouts[l])) == 113,
                  "multi rewind first batch accounting");
            CHECK((native ? audio_driver_sample_batch_multi_float(input.f + 113*channels, 944, channels, layouts[l])
                  : audio_driver_sample_batch_multi_int16(input.i + 113*channels, 944, channels, layouts[l])) == 944,
                  "multi rewind clipped batch accounting");
            canonical_track = false;
            CHECK(!st->rewind_ptr && !memcmp((const uint8_t*)&storage + sample, &expected, 1031*2*sample),
                  "multi rewind native %u mode %u layout %u lost folded reverse frames", native, mode, l);
            for (f = 0; f < sample; f++)
               CHECK(((const uint8_t*)&storage)[f] == 0x5a
                     && ((const uint8_t*)&storage)[(1031*2+1)*sample+f] == 0x5a,
                     "multi rewind buffer guard");
            CHECK(!cap_frames && !rec_frames && !st->extra.pending
                  && (mode != 2 || !retro_spsc_read_avail(&st->pipe_ring)),
                  "multi rewind leaked forward playback/recording");
            CHECK(canonical_reallocations == allocations, "multi rewind grew reserved staging");
            test_frame_reversed = false;
            rs->driver = NULL; rs->data = NULL;
            st->rewind_buf = NULL; st->rewind_buf_f = NULL;
            st->rewind_size = st->rewind_ptr = 0;
            audio_driver_deinit_internal(true);
            settings->bools.audio_time_stretch = false;
         }
   printf("cached multichannel rewind: 18 cases, %u failures\n", failures - before);
}

static void rewind_bounds_cases(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   int16_t input[] = { 12000, -3000, 8000, -2000, 7000, -1000 };
   float input_f[] = { 1.25f, -1.25f, 0.1234567f, -0.0312345f, 0.2f, -0.1f };
   int16_t storage[6], expected[4], converted[6];
   float storage_f[6], expected_f[4];
   unsigned kind, i, before = failures;
   convert_float_to_s16(converted, input_f, 6);
   for (kind = 0; kind < 4; kind++)
   {
      const int16_t *src = kind == 3 ? converted : input;
      for (i = 0; i < 6; i++) { storage[i] = 1234; storage_f[i] = 0.75f; }
      for (i = 0; i < 2; i++)
      {
         expected[2*i] = src[2*(1-i)]; expected[2*i+1] = src[2*(1-i)+1];
         expected_f[2*i] = input_f[2*(1-i)]; expected_f[2*i+1] = input_f[2*(1-i)+1];
      }
      st->core_float = kind == 2;
      st->rewind_buf = storage + 1;
      st->rewind_buf_f = kind == 2 ? storage_f + 1 : NULL;
      st->rewind_ptr = st->rewind_size = 4;
      test_frame_reversed = true;
      if (kind >= 2) CHECK(audio_driver_sample_batch_float(input_f, 3) == 3, "float rewind accounting");
      else if (kind) CHECK(audio_driver_sample_batch_rewind(input, 3) == 3, "int16 rewind accounting");
      else for (i = 0; i < 3; i++) audio_driver_sample_rewind(input[2*i], input[2*i+1]);
      CHECK(!st->rewind_ptr && (kind == 2
               ? !memcmp(storage_f + 1, expected_f, sizeof(expected_f))
               : !memcmp(storage + 1, expected, sizeof(expected))), "rewind bounds/order kind %u", kind);
      CHECK(storage[0] == 1234 && storage[5] == 1234
            && storage_f[0] == 0.75f && storage_f[5] == 0.75f, "rewind crossed buffer guard");
      /* A partial frame of room cannot accept either channel. */
      st->rewind_ptr = 1;
      if (kind >= 2) audio_driver_sample_batch_float(input_f, 1);
      else if (kind) audio_driver_sample_batch_rewind(input, 1);
      else audio_driver_sample_rewind(input[0], input[1]);
      CHECK(st->rewind_ptr == 1 && (kind == 2
               ? !memcmp(storage_f + 1, expected_f, sizeof(expected_f))
               : !memcmp(storage + 1, expected, sizeof(expected))), "rewind accepted a partial frame");
   }
   test_frame_reversed = false;
   st->rewind_buf = NULL; st->rewind_buf_f = NULL;
   st->rewind_size = st->rewind_ptr = 0;
   st->core_float = false;
   printf("rewind frame bounds: 4 cases, %u failures\n", failures - before);
}

/* Independent frame reversal, then compare the complete playback path. */
static void rewind_frame_cases(void)
{
   union { float f[514]; int16_t i[514]; } input, reverse, storage;
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings = config_get_ptr();
   unsigned kind, capture, batch, f, before = failures;
   float *reference = NULL;
   size_t reference_frames = 0;
   for (kind = 0; kind < 3; kind++)
      for (capture = 0; capture < 2; capture++)
      {
         bool floating = kind == 2;
         struct audio_inline_transport *saved;
         unsigned allocations;
         CHECK(up(floating, AUDIO_LAYOUT_STEREO, floating), "rewind stand-up");
         free(cap); cap = NULL; cap_cap = cap_frames = 0;
         st->resampler_hq = true;
         st->src_ratio_orig = st->src_ratio_curr = 96000.0 / st->input;
         settings->uints.audio_output_sample_rate = 96000;
         CHECK(retro_resampler_realloc_hq(&st->resampler_data, &st->resampler,
               "sinc", st->resampler_quality, st->src_ratio_orig, true), "rewind HQ SRC");
         if (!floating)
         {
            settings->bools.audio_fastpath_s16 = true;
            st->resampler_data_int16 = audio_driver_int16_resampler_new(st);
            st->resampler_int16_process = sinc_resampler_int16_process;
            st->resampler_int16_free = sinc_resampler_int16_free;
            st->resampler_int16_reset = sinc_resampler_int16_reset;
         }
         snap_pause(false);
         settings->bools.audio_time_stretch = true;
         settings->bools.audio_time_stretch_lowpass = false;
         settings->floats.slowmotion_ratio = 2;
         runloop_state_get_ptr()->flags = RUNLOOP_FLAG_SLOWMOTION;
         audio_driver_publish_runloop();
         CHECK(audio_driver_transport_configure(settings), "rewind prepare");
         saved = st->inline_transport;
         CHECK(saved != NULL, "rewind transport missing");
         if (!saved) exit(1);
         st->rewind_buf = floating ? NULL : storage.i;
         st->rewind_buf_f = floating ? storage.f : NULL;
         st->rewind_size = 514;
         transport_track = true;
         allocations = transport_allocations;
         for (batch = 0; batch < 16; batch++)
         {
            for (f = 0; f < 257; f++)
            {
               float v = 0.25f * sinf((float)(2 * M_PI * 440 * (batch*257+f) / 44100.0));
               if (floating)
               {
                  input.f[2*f] = v;
                  input.f[2*f+1] = v * -0.375f;
                  reverse.f[2*(256-f)] = input.f[2*f];
                  reverse.f[2*(256-f)+1] = input.f[2*f+1];
               }
               else
               {
                  input.i[2*f] = (int16_t)(v * 32767);
                  input.i[2*f+1] = (int16_t)(v * -12287);
                  reverse.i[2*(256-f)] = input.i[2*f];
                  reverse.i[2*(256-f)+1] = input.i[2*f+1];
               }
            }
            if (capture)
            {
               size_t frames = cap_frames;
               audio_driver_setup_rewind();
               test_frame_reversed = true;
               if (batch == 8)
               {
                  AUDIO_FLAGS_SET(st, AUDIO_FLAG_SUSPENDED);
                  if (floating) audio_driver_sample_batch_float(input.f, 7);
                  else if (kind)
                     CHECK(audio_driver_sample_batch_rewind(input.i, 7) == 7, "suspended native rewind accounting");
                  else for (f = 0; f < 7; f++) audio_driver_sample_rewind(32000, -32000);
                  CHECK(st->rewind_ptr == st->rewind_size && cap_frames == frames,
                        "suspended native rewind retained speculative input");
                  AUDIO_FLAGS_CLEAR(st, AUDIO_FLAG_SUSPENDED);
               }
               if (floating)
               {
                  audio_driver_sample_batch_float(input.f, 113);
                  audio_driver_sample_batch_float(input.f + 226, 144);
               }
               else if (kind)
               {
                  audio_driver_sample_batch_rewind(input.i, 113);
                  audio_driver_sample_batch_rewind(input.i + 226, 144);
               }
               else
                  for (f = 0; f < 257; f++)
                     audio_driver_sample_rewind(input.i[2*f], input.i[2*f+1]);
               CHECK(!st->rewind_ptr && cap_frames == frames,
                     "rewind capture advanced device or lost frames");
               CHECK(!memcmp(&storage, &reverse, 514 * (floating ? sizeof(float) : sizeof(int16_t))),
                     "rewind kind %u changed stereo frame order", kind);
               audio_driver_frame_is_reverse();
               test_frame_reversed = false;
            }
            else
            {
               test_frame_reversed = true;
               audio_driver_submit(st, 2.0f, &reverse, 514, floating, true, false, true);
               test_frame_reversed = false;
            }
            audio_driver_frame_end();
            CHECK(saved->bypassed, "rewind did not use ordinary playback");
         }
         if (floating) audio_driver_sample_batch_float(input.f, 257);
         else audio_driver_sample_batch(input.i, 257);
         audio_driver_frame_end();
         transport_track = false;
         CHECK(st->inline_transport == saved && !saved->bypassed
               && allocations == transport_allocations, "rewind replaced prepared transport");
         CHECK(cap_frames > 8192 && st->stat_core_is_float == floating
               && st->stat_frontend_is_float == floating, "rewind lost duration or native lane");
         if (!capture)
         {
            reference_frames = cap_frames;
            reference = (float*)malloc(cap_frames * 2 * sizeof(float));
            if (!reference) exit(1);
            memcpy(reference, cap, cap_frames * 2 * sizeof(float));
         }
         else
         {
            CHECK(cap_frames == reference_frames
                  && !memcmp(reference, cap, cap_frames * 2 * sizeof(float)),
                  "rewind kind %u changed WSOLA/HQ playback", kind);
            free(reference); reference = NULL;
         }
         st->rewind_buf = NULL; st->rewind_buf_f = NULL;
         st->rewind_size = st->rewind_ptr = 0;
         audio_driver_deinit_internal(true);
      }
   settings->bools.audio_time_stretch = false;
   settings->bools.audio_fastpath_s16 = false;
   settings->floats.slowmotion_ratio = 1;
   runloop_state_get_ptr()->flags = 0;
   audio_driver_publish_runloop();
   printf("native rewind frames: 3 cases, %u failures\n", failures - before);
   rewind_bounds_cases();
}

static void inline_slot_order(void)
{
   union { float f[3*8]; int16_t i[3*8]; } input;
   union { float f[3*11]; int16_t i[3*11]; } output;
   unsigned native, channels, c, f, slot;
   for (native = 0; native < 2; native++)
      for (channels = 6; channels <= 8; channels += 2)
      {
         for (f = 0; f < 3; f++)
            for (c = 0; c < channels; c++)
               if (native) input.f[f*channels+c] = (float)(f*8+c+1);
               else input.i[f*channels+c] = (int16_t)(f*8+c+1);
         CHECK(audio_driver_pipe_widen_prefix(&output, &input, 3, channels,
               channels == 6 ? AUDIO_LAYOUT_5POINT1 : AUDIO_LAYOUT_7POINT1, native), "canonical prefix rejected");
         for (f = 0; f < 3; f++)
            for (slot = 0; slot < 11; slot++)
            {
               int expected = slot < 6 ? (int)(f*8+slot+1)
                  : channels == 8 && slot >= 9 ? (int)(f*8+slot-2) : 0;
               CHECK((native ? output.f[f*11+slot] : output.i[f*11+slot]) == expected,
                     "canonical slot %u misplaced", slot);
            }
      }
}

static void inline_wide_cases(void)
{
   static const unsigned layouts[] = {AUDIO_LAYOUT_QUAD, AUDIO_LAYOUT_5POINT1,
      AUDIO_LAYOUT_5POINT1_SURROUND, AUDIO_LAYOUT_7POINT1};
   union { float f[257*8]; int16_t i[257*8]; } input;
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings = config_get_ptr();
   unsigned native, l, fold, speed, cases = 0, before = failures;
   inline_slot_order();
   for (native = 0; native < 2; native++)
      for (l = 0; l < 4; l++)
         for (fold = 0; fold < 2; fold++)
            for (speed = 0; speed < 2; speed++)
            {
               unsigned channels = audio_layout_channels(layouts[l]), c;
               unsigned rate = l & 1 ? 96000 : 48000;
               size_t total = speed ? 32768 : 16384, done = 0, f, pending;
               double duration = speed ? 0.5 : 2.0, expected;
               float gains[8], stereo[2];
               struct audio_inline_transport *saved;
               CHECK(up(native, fold ? AUDIO_LAYOUT_STEREO : layouts[l], native), "wide inline stand-up");
               free(cap); cap = NULL; cap_cap = cap_frames = 0;
               st->core_multi = true;
               st->resampler_hq = l & 1;
               st->src_ratio_orig = st->src_ratio_curr = rate / 44100.0;
               settings->uints.audio_output_sample_rate = rate;
               CHECK(retro_resampler_realloc_hq(&st->resampler_data, &st->resampler,
                     "sinc", st->resampler_quality, st->src_ratio_orig, st->resampler_hq), "wide inline SRC");
               if (!native)
               {
                  settings->bools.audio_fastpath_s16 = true;
                  st->resampler_data_int16 = audio_driver_int16_resampler_new(st);
                  st->resampler_int16_process = sinc_resampler_int16_process;
                  st->resampler_int16_free = sinc_resampler_int16_free;
                  st->resampler_int16_reset = sinc_resampler_int16_reset;
               }
               snap_pause(false);
               settings->bools.audio_time_stretch = true;
               settings->bools.audio_time_stretch_lowpass = false;
               settings->bools.audio_fastforward_speedup = false;
               settings->floats.slowmotion_ratio = (float)duration;
               runloop_state_get_ptr()->flags = RUNLOOP_FLAG_SLOWMOTION;
               audio_driver_publish_runloop();
               CHECK(audio_driver_transport_configure(settings), "wide inline prepare");
               saved = st->inline_transport;
               CHECK(saved && saved->channels == AUDIO_PIPE_CANON_CHANNELS, "wide inline storage missing");
               if (!saved) return;
               for (c = 0; c < channels; c++) gains[c] = (c & 1 ? -1.0f : 1.0f) * (c + 1) / 8;
               audio_downmix_f32(stereo, gains, 1, layouts[l], channels);
               while (done < total)
               {
                  size_t n = total - done;
                  if (n > 257) n = 257;
                  for (f = 0; f < n; f++)
                  {
                     int16_t v = (int16_t)(8000 * sin(2 * M_PI * 440 * (done + f) / 44100.0));
                     for (c = 0; c < channels; c++)
                        if (native) input.f[f*channels+c] = v / 32768.0f * gains[c];
                        else input.i[f*channels+c] = (int16_t)(v * gains[c]);
                  }
                  CHECK((native ? audio_driver_sample_batch_multi_float(input.f, n, channels, layouts[l])
                        : audio_driver_sample_batch_multi_int16(input.i, n, channels, layouts[l])) == n,
                        "wide inline source accounting");
                  CHECK(!audio_stretch_stream_peek(saved->stream, &pending) && !pending,
                        "wide inline retained source view");
                  done += n;
               }
               expected = total * duration * st->src_ratio_orig;
               CHECK(fabs((double)cap_frames - expected) < 1024 * (1 + duration) * st->src_ratio_orig,
                     "wide inline duration: %u expected %.1f", (unsigned)cap_frames, expected);
               CHECK(st->stat_core_is_float == (bool)native && st->stat_frontend_is_float == (bool)native,
                     "wide inline native lane changed");
               for (c = 0; c < dev_channels; c++)
               {
                  double first = 0, last = 0, error = 0, energy = 0;
                  float gain = fold ? stereo[c] : gains[c];
                  float gain0 = fold ? stereo[0] : gains[0];
                  unsigned crossings = 0;
                  size_t trim = cap_frames / 8;
                  for (f = trim; f < cap_frames - trim; f++)
                  {
                     double x = cap[(f-1)*dev_channels+c], y = cap[f*dev_channels+c];
                     double e = fabs(y / gain - cap[f*dev_channels] / gain0);
                     if (e > error) error = e;
                     energy += y*y;
                     if (x <= 0 && y > 0)
                     {
                        double at = f - 1 + (-x)/(y-x);
                        if (!crossings) first = at;
                        last = at; crossings++;
                     }
                  }
                  CHECK(crossings > 1 && fabs((crossings-1)*rate/(last-first)-440) < 8.8,
                        "wide inline pitch ch%u", c);
                  CHECK(error < 0.012 && energy > 0.001, "wide inline routing ch%u error %.5f", c, error);
               }
               {
                  unsigned next = layouts[l] == AUDIO_LAYOUT_QUAD
                     ? AUDIO_LAYOUT_5POINT1_SURROUND : AUDIO_LAYOUT_QUAD;
                  unsigned count = audio_layout_channels(next);
                  size_t previous = cap_frames;
                  memset(&input, 0, sizeof(input));
                  if (native) audio_driver_sample_batch_multi_float(input.f, 17, count, next);
                  else audio_driver_sample_batch_multi_int16(input.i, 17, count, next);
                  CHECK(saved->layout == next && cap_frames == previous,
                        "active layout handoff emitted an old tail");
               }
               settings->floats.slowmotion_ratio = 8;
               audio_driver_publish_runloop();
               if (native) audio_driver_sample_batch_multi_float(input.f, 17, channels, layouts[l]);
               else audio_driver_sample_batch_multi_int16(input.i, 17, channels, layouts[l]);
               CHECK(saved->bypassed, "wide inline unsupported speed did not fall back");
               runloop_state_get_ptr()->flags = 0;
               settings->floats.slowmotion_ratio = 1;
               audio_driver_publish_runloop();
               if (native) audio_driver_sample_batch_multi_float(input.f, 17, 2, AUDIO_LAYOUT_STEREO);
               else audio_driver_sample_batch_multi_int16(input.i, 17, 2, AUDIO_LAYOUT_STEREO);
               CHECK(!saved->bypassed && saved->layout == AUDIO_LAYOUT_STEREO && saved == st->inline_transport,
                     "wide inline stereo handoff failed");
               if (native) audio_driver_sample_batch_multi_float(input.f, 17, channels, layouts[l]);
               else audio_driver_sample_batch_multi_int16(input.i, 17, channels, layouts[l]);
               CHECK(saved->layout == layouts[l] && saved == st->inline_transport,
                     "wide inline layout recovery replaced storage");
               CHECK(audio_driver_stop() && audio_stretch_stream_quiescent(saved->stream), "wide inline stop retained tail");
               audio_driver_deinit_internal(true);
               CHECK(!st->inline_transport, "wide inline teardown retained storage");
               cases++;
            }
   settings->bools.audio_time_stretch = settings->bools.audio_fastpath_s16 = false;
   settings->floats.slowmotion_ratio = 1;
   runloop_state_get_ptr()->flags = 0;
   audio_driver_publish_runloop();
   printf("wide inline native transport: %u cases, %u failures\n", cases, failures - before);
}

/* Negotiate on the source owner with an unfinished old-format window. */
static void inline_format_cases(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings = config_get_ptr();
   union { float f[257*6]; int16_t i[257*6]; } input;
   int16_t accum[AUDIO_SAMPLE_ACCUM_INT16S];
   unsigned native, wide, hq, pass, cases = 0, before = failures;
   for (native = 0; native < 2; native++)
      for (wide = 0; wide < 2; wide++)
         for (hq = 0; hq < 2; hq++)
         {
            struct audio_inline_transport *saved;
            size_t f;
            CHECK(up(native, AUDIO_LAYOUT_5POINT1, true), "format stand-up");
            free(cap); cap = NULL; cap_cap = cap_frames = 0;
            st->resampler_hq = hq;
            CHECK(retro_resampler_realloc_hq(&st->resampler_data, &st->resampler,
                  "sinc", st->resampler_quality, st->src_ratio_orig, hq), "format SRC");
            settings->bools.audio_fastpath_s16 = true;
            st->resampler_data_int16 = audio_driver_int16_resampler_new(st);
            st->resampler_int16_process = sinc_resampler_int16_process;
            st->resampler_int16_free = sinc_resampler_int16_free;
            st->resampler_int16_reset = sinc_resampler_int16_reset;
            snap_pause(false);
            audio_driver_set_core_multi(wide);
            CHECK(!st->inline_transport, "negotiation enabled an unconfigured stage");
            settings->bools.audio_time_stretch = true;
            settings->bools.audio_time_stretch_lowpass = true;
            settings->floats.slowmotion_ratio = 2;
            runloop_state_get_ptr()->flags = RUNLOOP_FLAG_SLOWMOTION;
            audio_driver_publish_runloop();
            CHECK(audio_driver_transport_configure(settings), "format configure");
            saved = st->inline_transport;
            st->sample_accum = accum;
            CHECK(saved && saved->channels == (wide ? 11u : 2u), "initial format width");
            for (f = 0; f < 17*2; f++)
               if (native) input.f[f] = 0.3f;
               else input.i[f] = 9830;
            if (native) audio_driver_sample_batch_float(input.f, 17);
            else audio_driver_sample_batch(input.i, 17);
            CHECK(!audio_stretch_stream_quiescent(saved->stream), "missing old-format tail");
            audio_driver_set_core_float(native);
            audio_driver_set_core_multi(wide);
            CHECK(st->inline_transport == saved && !audio_stretch_stream_quiescent(saved->stream),
                  "repeated negotiation discarded history");
            audio_driver_set_core_multi(true);
            CHECK(st->inline_transport && st->inline_transport->channels == 11,
                  "late wide negotiation retained stereo stage");
            for (pass = 0; pass < 2; pass++)
            {
               bool floating = pass ? native : !native;
               size_t start = cap_frames;
               unsigned allocations;
               if (!st->core_float)
               {
                  audio_driver_sample(0, 0);
                  CHECK(st->data_ptr == 2, "missing single-sample input");
               }
               st->last_flush_time = st->avg_flush_delta = 123;
               audio_driver_set_core_float(floating);
               start = cap_frames; /* Accumulated old source was flushed before the boundary. */
               saved = st->inline_transport;
               CHECK(saved && saved->floating == floating && saved->channels == 11,
                     "native format rebind failed");
               CHECK(audio_stretch_stream_quiescent(saved->stream)
                     && !st->last_flush_time && !st->avg_flush_delta && !st->extra.pending
                     && !st->data_ptr,
                     "format change retained old cadence or DSP history");
               transport_track = true;
               allocations = transport_allocations;
               memset(&input, 0, sizeof(input));
               for (f = 0; f < 32; f++)
                  if (floating) audio_driver_sample_batch_multi_float(input.f, 257, 6, AUDIO_LAYOUT_5POINT1);
                  else audio_driver_sample_batch_multi_int16(input.i, 257, 6, AUDIO_LAYOUT_5POINT1);
               CHECK(!saved->bypassed && cap_frames > start + 8192,
                     "rebound stage lost slow-motion duration");
               for (f = start * dev_channels; f < cap_frames * dev_channels; f++)
                  CHECK(cap[f] == 0.0f, "old-format tail leaked into silence");
               audio_driver_set_core_multi(false);
               CHECK(st->inline_transport == saved, "stereo return reallocated canonical arena");
               audio_driver_set_core_multi(true);
               CHECK(st->inline_transport == saved, "wide return reallocated canonical arena");
               CHECK(transport_allocations == allocations, "steady playback allocated an arena");
               transport_track = false;
            }
            st->last_flush_time = 123;
            CHECK(audio_driver_stop() && audio_stretch_stream_quiescent(saved->stream), "rebound stop");
            CHECK(!st->last_flush_time, "rebound stop retained speed cadence");
            CHECK(audio_driver_start(false), "rebound restart");
            transport_fail_output = true;
            audio_driver_set_core_float(!native);
            transport_fail_output = false;
            CHECK(!st->inline_transport && st->core_float == !native, "failed rebind retained stale stage");
            runloop_state_get_ptr()->flags = 0;
            audio_driver_publish_runloop();
            f = cap_frames;
            if (!native) audio_driver_sample_batch_multi_float(input.f, 257, 6, AUDIO_LAYOUT_5POINT1);
            else audio_driver_sample_batch_multi_int16(input.i, 257, 6, AUDIO_LAYOUT_5POINT1);
            CHECK(cap_frames > f, "failed rebind muted ordinary playback");
            audio_driver_deinit_internal(true);
            audio_driver_set_core_float(!native);
            audio_driver_set_core_multi(false);
            CHECK(!st->inline_transport, "negotiation resurrected deinitialized stage");
            cases++;
         }
   settings->bools.audio_time_stretch = settings->bools.audio_time_stretch_lowpass = false;
   settings->bools.audio_fastpath_s16 = false;
   settings->floats.slowmotion_ratio = 1;
   runloop_state_get_ptr()->flags = 0;
   audio_driver_publish_runloop();
   printf("inline format negotiation: %u cases, %u failures\n", cases, failures - before);
}

static union { float f[257*2]; int16_t i[257*2]; } callback_pcm;
static unsigned callback_calls;
static bool callback_native, callback_empty;
static slock_t *callback_lock;
static scond_t *callback_cond;
static bool callback_request, callback_ready, callback_release;
static bool callback_done, callback_quit, callback_result;

static void callback_wait(void)
{
   if (!scond_wait_timeout(callback_cond, callback_lock, 30000000))
   {
      fprintf(stderr, "callback rendezvous timed out\n");
      exit(1);
   }
}

static void callback_worker(void *unused)
{
   (void)unused;
   slock_lock(callback_lock);
   for (;;)
   {
      while (!callback_request && !callback_quit) callback_wait();
      if (callback_quit) break;
      callback_request = false;
      slock_unlock(callback_lock);
      callback_result = audio_driver_callback();
      slock_lock(callback_lock);
      callback_done = true;
      scond_signal(callback_cond);
   }
   slock_unlock(callback_lock);
}

static bool callback_dispatch(void)
{
   bool result;
   slock_lock(callback_lock);
   callback_ready = callback_release = callback_done = false;
   callback_request = true;
   scond_signal(callback_cond);
   while (!callback_ready && !callback_done) callback_wait();
   if (callback_ready)
   {
      size_t pending = audio_driver_st.data_ptr;
      CHECK(pending == (!callback_native
               && !(AUDIO_FLAGS_GET(&audio_driver_st) & AUDIO_FLAG_SUSPENDED) ? 514u : 0u),
            "callback rendezvous missed pending source input");
      audio_driver_frame_end();
      CHECK(audio_driver_st.data_ptr == pending,
            "main frame touched callback-owned accumulator");
      callback_release = true;
      scond_signal(callback_cond);
   }
   while (!callback_done) callback_wait();
   result = callback_result;
   slock_unlock(callback_lock);
   return result;
}


static void inline_source_callback(void)
{
   unsigned f;
   callback_calls++;
   if (callback_empty) return;
   if (callback_native)
      audio_driver_sample_batch_float(callback_pcm.f, 257);
   else
      for (f = 0; f < 257; f++)
         audio_driver_sample(callback_pcm.i[2*f], callback_pcm.i[2*f+1]);
   /* Hold the source callback open while the main thread ends a frame. */
   slock_lock(callback_lock);
   callback_ready = true;
   scond_signal(callback_cond);
   while (!callback_release) callback_wait();
   slock_unlock(callback_lock);
}

/* Compare callback delivery and discarded frames against uninterrupted audio. */
static void inline_callback_cases(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings = config_get_ptr();
   int16_t accum[AUDIO_SAMPLE_ACCUM_INT16S];
   unsigned native, callbacks, batch, f, before = failures;
   float *reference = NULL;
   size_t reference_frames = 0;
   for (native = 0; native < 2; native++)
      for (callbacks = 0; callbacks < 2; callbacks++)
      {
         struct audio_inline_transport *saved;
         unsigned allocations;
         sthread_t *worker = NULL;
         CHECK(up(native, AUDIO_LAYOUT_STEREO, native), "callback stand-up");
         free(cap); cap = NULL; cap_cap = cap_frames = 0;
         st->resampler_hq = native;
         CHECK(retro_resampler_realloc_hq(&st->resampler_data, &st->resampler,
               "sinc", st->resampler_quality, st->src_ratio_orig, native), "callback SRC");
         if (!native)
         {
            settings->bools.audio_fastpath_s16 = true;
            st->resampler_data_int16 = audio_driver_int16_resampler_new(st);
            st->resampler_int16_process = sinc_resampler_int16_process;
            st->resampler_int16_free = sinc_resampler_int16_free;
            st->resampler_int16_reset = sinc_resampler_int16_reset;
         }
         snap_pause(false);
         settings->bools.audio_time_stretch = true;
         settings->bools.audio_time_stretch_lowpass = true;
         settings->floats.slowmotion_ratio = 2;
         runloop_state_get_ptr()->flags = RUNLOOP_FLAG_SLOWMOTION;
         audio_driver_publish_runloop();
         CHECK(audio_driver_transport_configure(settings), "callback prepare");
         saved = st->inline_transport;
         st->sample_accum = accum;
         st->callback.callback = callbacks ? inline_source_callback : NULL;
         callback_native = native;
         callback_calls = 0;
         callback_empty = false;
         transport_track = true;
         allocations = transport_allocations;
         if (callbacks)
         {
            callback_lock = slock_new();
            callback_cond = scond_new();
            if (!callback_lock || !callback_cond) exit(1);
            callback_request = callback_ready = callback_release = false;
            callback_done = callback_quit = callback_result = false;
            worker = sthread_create(callback_worker, NULL);
            if (!worker) exit(1);
         }
         for (batch = 0; batch < 32; batch++)
         {
            for (f = 0; f < 257; f++)
            {
               float value = 0.3f * sinf((float)(2 * M_PI * 440 * (batch*257+f) / 44100.0));
               if (native)
               { callback_pcm.f[2*f] = value; callback_pcm.f[2*f+1] = -value; }
               else
               { callback_pcm.i[2*f] = (int16_t)(value * 32767); callback_pcm.i[2*f+1] = -callback_pcm.i[2*f]; }
            }
            if (callbacks && batch == 16)
            {
               unsigned calls = callback_calls;
               size_t frames = cap_frames;
               runloop_state_get_ptr()->flags |= RUNLOOP_FLAG_PAUSED;
               audio_driver_publish_runloop();
               audio_driver_frame_end();
               CHECK(!callback_dispatch() && callback_calls == calls
                     && cap_frames == frames, "paused callback advanced source/output");
               runloop_state_get_ptr()->flags &= ~RUNLOOP_FLAG_PAUSED;
               audio_driver_publish_runloop();
               AUDIO_FLAGS_SET(st, AUDIO_FLAG_SUSPENDED);
               audio_driver_frame_end();
               CHECK(!callback_dispatch() && callback_calls == calls + 1
                     && !st->data_ptr && cap_frames == frames,
                     "suspended callback retained speculative audio");
               AUDIO_FLAGS_CLEAR(st, AUDIO_FLAG_SUSPENDED);
               callback_empty = true;
               CHECK(!callback_dispatch() && callback_calls == calls + 2
                     && !st->data_ptr && cap_frames == frames,
                     "empty callback reported device progress or changed output");
               callback_empty = false;
            }
            if (callbacks)
            {
               size_t frames = cap_frames;
               bool progress = callback_dispatch();
               CHECK(progress == (cap_frames != frames
                        || (st->inline_transport && st->inline_transport->source_progress)),
                     "callback progress disagrees with device writes");
               CHECK(!st->data_ptr, "callback retained accumulator input");
            }
            else if (native) audio_driver_sample_batch_float(callback_pcm.f, 257);
            else audio_driver_sample_batch(callback_pcm.i, 257);
            audio_driver_frame_end();
         }
         CHECK(st->inline_transport == saved && !saved->bypassed
               && transport_allocations == allocations, "callback replaced or bypassed prepared transport");
         transport_track = false;
         CHECK(cap_frames > 8192, "callback lost stretched output duration");
         if (!callbacks)
         {
            reference_frames = cap_frames;
            reference = (float*)malloc(cap_frames * 2 * sizeof(float));
            CHECK(reference != NULL, "callback reference allocation");
            if (reference) memcpy(reference, cap, cap_frames * 2 * sizeof(float));
         }
         else
         {
            CHECK(callback_calls == 34, "unexpected source callback count");
            CHECK(cap_frames == reference_frames && reference
                  && !memcmp(reference, cap, cap_frames * 2 * sizeof(float)),
                  "pause/suspension or callback delivery changed the audible stream");
            free(reference); reference = NULL;
         }
         if (worker)
         {
            slock_lock(callback_lock);
            callback_quit = true;
            scond_signal(callback_cond);
            slock_unlock(callback_lock);
            sthread_join(worker);
            scond_free(callback_cond);
            slock_free(callback_lock);
            callback_cond = NULL;
            callback_lock = NULL;
         }
         st->callback.callback = NULL;
         audio_driver_deinit_internal(true);
      }
   settings->bools.audio_time_stretch = settings->bools.audio_time_stretch_lowpass = false;
   settings->bools.audio_fastpath_s16 = false;
   settings->floats.slowmotion_ratio = 1;
   runloop_state_get_ptr()->flags = 0;
   audio_driver_publish_runloop();
   printf("inline callback continuity: 2 cases, %u failures\n", failures - before);
}

static size_t menu_recorded_frames;
static unsigned menu_recorded_calls;
static bool menu_timing_record(void *unused, const struct record_audio_data *data)
{
   (void)unused;
   menu_recorded_frames += data->frames;
   menu_recorded_calls++;
   return true;
}

static void menu_timing_cases(void)
{
   static const double rates[] = {0, -1, 48000, 48000, 1.0e300, 48000, 48000, 48000};
   static const double fps[] = {60, 60, 0, -1, 1, 60, 120, 96000};
   struct retro_system_timing saved = video_state_get_ptr()->av_info.timing;
   recording_state_t *record = recording_state_get_ptr();
   record_driver_t driver;
   uint64_t invalid[] = {UINT64_C(0x7ff0000000000000), UINT64_C(0x7ff8000000000001)};
   unsigned i, before = failures;
   memset(&driver, 0, sizeof(driver));
   driver.push_audio = menu_timing_record;
   CHECK(up(false, AUDIO_LAYOUT_STEREO, false), "menu timing stand-up");
   AUDIO_FLAGS_CLEAR(&audio_driver_st, AUDIO_FLAG_ACTIVE);
   record->data = &driver; record->driver = &driver;
   for (i = 0; i < sizeof(rates)/sizeof(rates[0]); i++)
   {
      size_t expected = i == 5 ? 800 : i == 6 ? 400 : 0;
      video_state_get_ptr()->av_info.timing.sample_rate = rates[i];
      video_state_get_ptr()->av_info.timing.fps = fps[i];
      menu_recorded_frames = menu_recorded_calls = 0;
      audio_driver_menu_sample();
      CHECK(menu_recorded_frames == expected && (i >= 5 ? menu_recorded_calls != 0 : menu_recorded_calls == 0),
            "invalid menu timing emitted audio, or valid timing changed");
   }
   for (i = 0; i < 4; i++)
   {
      video_state_get_ptr()->av_info.timing.sample_rate = 48000;
      video_state_get_ptr()->av_info.timing.fps = 60;
      memcpy(i < 2 ? &video_state_get_ptr()->av_info.timing.sample_rate
            : &video_state_get_ptr()->av_info.timing.fps, &invalid[i & 1], sizeof(double));
      menu_recorded_frames = menu_recorded_calls = 0;
      audio_driver_menu_sample();
      CHECK(!menu_recorded_calls, "nonfinite menu timing reached recorder");
   }
   record->data = NULL; record->driver = NULL;
   video_state_get_ptr()->av_info.timing = saved;
   audio_driver_deinit_internal(true);
   printf("menu timing bounds: 12 cases, %u failures\n", failures - before);
}


static union { float f[257*2]; int16_t i[257*2]; } progress_pcm;
static bool progress_float, progress_empty;
static unsigned progress_calls;
static void progress_source(void)
{
   progress_calls++;
   if (progress_empty) return;
   if (progress_float) audio_driver_sample_batch_float(progress_pcm.f, 257);
   else audio_driver_sample_batch(progress_pcm.i, 257);
}

static void callback_buffering_case(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings = config_get_ptr();
   unsigned native, i, before = failures;
   for (native = 0; native < 2; native++)
   {
      unsigned idle = 0, writes = 0, calls;
      size_t output;
      CHECK(up(native, AUDIO_LAYOUT_STEREO, native), "buffering callback stand-up");
      free(cap); cap = NULL; cap_cap = cap_frames = 0;
      if (!native)
      {
         settings->bools.audio_fastpath_s16 = true;
         st->resampler_data_int16 = audio_driver_int16_resampler_new(st);
         st->resampler_int16_process = sinc_resampler_int16_process;
         st->resampler_int16_free = sinc_resampler_int16_free;
         st->resampler_int16_reset = sinc_resampler_int16_reset;
      }
      snap_pause(false);
      settings->bools.audio_time_stretch = true;
      settings->bools.audio_time_stretch_lowpass = false;
      /* Exercise supported 32x tempo without wall-clock estimation. */
      settings->floats.slowmotion_ratio = 1.0f / 32.0f;
      runloop_state_get_ptr()->flags = RUNLOOP_FLAG_SLOWMOTION;
      audio_driver_publish_runloop();
      CHECK(audio_driver_transport_configure(settings), "buffering callback prepare");
      st->callback.callback = progress_source;
      progress_float = native; progress_empty = false; progress_calls = 0;
      for (i = 0; i < 256; i++)
      {
         size_t previous = cap_frames;
         if (!audio_driver_callback()) idle++;
         if (cap_frames != previous) writes++;
      }
      printf("buffering callback native=%u: %u idle passes, %u writes, %lu output frames\n",
            native, idle, writes, (unsigned long)cap_frames);
      CHECK(writes && writes < 256, "buffering probe did not exercise output-free passes");
      CHECK(!idle, "accepted transport input was mistaken for an idle callback");
      CHECK(!st->inline_transport->bypassed, "buffering probe bypassed transport");
      output = cap_frames;
      progress_empty = true;
      for (i = 0; i < 4; i++)
         CHECK(!audio_driver_callback(), "empty callback reused prior transport progress");
      progress_empty = false;
      AUDIO_FLAGS_SET(st, AUDIO_FLAG_SUSPENDED);
      CHECK(!audio_driver_callback(), "suspended callback reported transport progress");
      AUDIO_FLAGS_CLEAR(st, AUDIO_FLAG_SUSPENDED);
      calls = progress_calls;
      runloop_state_get_ptr()->flags |= RUNLOOP_FLAG_PAUSED;
      audio_driver_publish_runloop();
      CHECK(!audio_driver_callback() && progress_calls == calls,
            "paused callback ran the source or reported progress");
      CHECK(cap_frames == output, "idle callbacks changed device output");
      st->callback.callback = NULL;
      audio_driver_deinit_internal(true);
   }
   settings->bools.audio_fastpath_s16 = settings->bools.audio_time_stretch = false;
   settings->floats.slowmotion_ratio = 1;
   runloop_state_get_ptr()->flags = 0;
   audio_driver_publish_runloop();
   printf("callback transport progress: 2 cases, %u failures\n", failures - before);
}
