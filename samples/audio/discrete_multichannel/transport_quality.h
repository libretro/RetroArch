/* End-to-end oracle for configured native transport, SRC and short writes.
 * Included after the scripted frontend/device fixture. */
static void transport_quality_case(bool floating, bool wide, bool hq,
      uint32_t tempo_q16, bool lpf_only)
{
   union { float f[257 * 6]; int16_t i[257 * 6]; } input;
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings = config_get_ptr();
   unsigned channels = wide ? 6 : 2, c, iterations = 0;
   unsigned output_rate = hq ? 96000 : 44100;
   double tempo = tempo_q16 / 65536.0, expected, tolerance;
   size_t total = (size_t)(16384 * (tempo > 1.0 ? tempo : 1.0));
   size_t submitted = 0, f, trim;
   bool complete = false;
   CHECK(pipe_up(floating, floating), "quality stand-up");
   free(cap); cap = NULL; cap_cap = cap_frames = 0;
   if (!wide)
   {
      st->pipe_channels = dev_channels = st->out_channels = 2;
      st->pipe_frame_bytes = 2 * (floating ? sizeof(float) : sizeof(int16_t));
      dev_layout = st->out_layout = AUDIO_LAYOUT_STEREO;
   }
   st->input = 48000;
   settings->uints.audio_output_sample_rate = output_rate;
   st->out_rate = output_rate;
   st->src_ratio_orig = st->src_ratio_curr = output_rate / 48000.0;
   st->resampler_hq = hq;
   CHECK(retro_resampler_realloc_hq(&st->resampler_data, &st->resampler,
         "sinc", st->resampler_quality, st->src_ratio_orig, hq), "quality SRC");
   if (!floating)
   {
      settings->bools.audio_fastpath_s16 = true;
      st->resampler_data_int16 = audio_driver_int16_resampler_new(st);
      st->resampler_int16_process = sinc_resampler_int16_process;
      st->resampler_int16_free = sinc_resampler_int16_free;
      st->resampler_int16_reset = sinc_resampler_int16_reset;
      snap_pause(false);
      CHECK(st->resampler_data_int16 != NULL, "quality native SRC");
   }
   settings->bools.audio_time_stretch = !lpf_only;
   settings->bools.audio_time_stretch_lowpass = lpf_only;
   settings->bools.audio_fastforward_speedup = true;
   settings->bools.audio_sync = true;
   settings->floats.slowmotion_ratio = tempo < 1.0 ? (float)(1.0 / tempo) : 1.0f;
   runloop_state_get_ptr()->flags = tempo < 1.0 ? RUNLOOP_FLAG_SLOWMOTION
      : tempo > 1.0 ? RUNLOOP_FLAG_FASTMOTION : 0;
   retro_atomic_store_release_int(&st->pipe_ff_mult_q16, (int)(65536.0 / tempo));
   audio_driver_publish_runloop();
   CHECK(audio_driver_transport_configure(settings), "quality configured startup");
   if (!st->pipe_transport) return;
   CHECK(!lpf_only || (st->transport_lpf_only
         && !(st->pipe_layouts.published_control & AUDIO_PIPELINE_STRETCH)
         && st->pipe_layouts.published_cutoff), "filter-only enabled WSOLA or omitted LPF");
   scripted_threaded.write = short_device_write;
   scripted_threaded.wait_writable = short_device_wait;
   short_calls = 0;
   short_zero = true;
   short_no_room = short_fail = false;
   while (!complete || st->pipe_pending_bytes)
   {
      if (++iterations > 200000)
      {
         CHECK(false, "quality transport did not finish");
         break;
      }
      if (submitted < total && retro_spsc_write_avail(&st->pipe_ring)
            >= 257 * st->pipe_frame_bytes)
      {
         size_t n = total - submitted;
         if (n > 257) n = 257;
         for (f = 0; f < n; f++)
         {
            int16_t v = (int16_t)(12000 * sin(2 * M_PI * 440 * (submitted + f) / 48000.0));
            for (c = 0; c < channels; c++)
            {
               int16_t value = c & 1 ? -v : v;
               if (floating) input.f[f * channels + c] = value / 32768.0f;
               else input.i[f * channels + c] = value;
            }
         }
         /* Keep the known achieved speed independent of fixture wall time. */
         retro_atomic_store_release_int(&st->pipe_ff_mult_q16, (int)(65536.0 / tempo));
         if (wide)
            CHECK(audio_driver_multi_pipe(st, &input, n, channels,
                  AUDIO_LAYOUT_5POINT1, floating), "quality wide publish");
         else
            audio_driver_submit(st, 1.0f, &input, n * 2, floating, false, false, true);
         submitted += n;
         audio_driver_frame_end();
      }
      audio_driver_publish_runloop();
      /* Wide publication updates the achieved-speed estimator. Keep the
       * ordinary SRC's consumer-side multiplier deterministic as well. */
      retro_atomic_store_release_int(&st->pipe_ff_mult_q16, (int)(65536.0 / tempo));
      if (st->pipe_pending_bytes) short_zero = false;
      CHECK(audio_driver_pipeline_transport_step(st->pipe_transport,
            &st->pipe_transport_serial, 257, iterations & 1 ? 97 : 257,
            submitted == total && !retro_spsc_read_avail(&st->pipe_ring), &complete),
            "quality transport step");
   }
   expected = total / tempo * st->src_ratio_orig;
   tolerance = tempo == 1.0 ? 2 : 1280 * (1 + 1 / tempo) * st->src_ratio_orig;
   CHECK(fabs((double)cap_frames - expected) <= tolerance,
         "quality duration: tempo %.2f, got %u, expected %.1f +/- %.1f",
         tempo, (unsigned)cap_frames, expected, tolerance);
   CHECK(retro_atomic_load_relaxed_size(&st->pipe_ring.head) == total * st->pipe_frame_bytes
         && !retro_spsc_read_avail(&st->pipe_ring), "quality source accounting");
   CHECK(short_calls > 2 && st->stat_core_is_float == floating
         && st->stat_frontend_is_float == floating, "quality native short-write lane");
   trim = cap_frames / 8;
   CHECK(trim > 128, "quality capture too short");
   for (c = 0; c < channels && trim > 128; c++)
   {
      double first = 0, last = 0, energy = 0, peak = 0, hz = 0;
      unsigned crossings = 0;
      for (f = trim; f < cap_frames - trim; f++)
      {
         double x = cap[(f - 1) * channels + c], y = cap[f * channels + c];
         double error = y - (c & 1 ? -cap[f * channels] : cap[f * channels]);
         CHECK(y == y && fabs(y) <= 1, "quality nonfinite/clipped sample");
         if (fabs(error) > peak) peak = fabs(error);
         energy += y * y;
         if (x <= 0 && y > 0)
         {
            double position = f - 1 + (-x) / (y - x);
            if (!crossings) first = position;
            last = position;
            crossings++;
         }
      }
      if (crossings > 1) hz = (crossings - 1) * output_rate / (last - first);
      CHECK(fabs(hz - 440 * (lpf_only ? tempo : 1)) < 8.8,
            "quality pitch: tempo %.2f ch%u %.2f Hz", tempo, c, hz);
      CHECK(energy / (cap_frames - 2 * trim) > 0.03 && peak < 0.002,
            "quality energy/coherence: tempo %.2f ch%u error %.6f", tempo, c, peak);
   }
   audio_driver_deinit_internal(true);
   settings->bools.audio_time_stretch = settings->bools.audio_fastpath_s16 = false;
   settings->bools.audio_time_stretch_lowpass = false;
   settings->bools.audio_fastforward_speedup = false;
   settings->floats.slowmotion_ratio = 1;
   runloop_state_get_ptr()->flags = 0;
   audio_driver_publish_runloop();
}

static void transport_quality_cases(void)
{
   static const uint32_t tempos[] = {16384, 32768, 65536, 131072, 524288, 2097152};
   unsigned floating, wide, hq, t, before = failures;
   for (floating = 0; floating < 2; floating++)
      for (wide = 0; wide < 2; wide++)
         for (hq = 0; hq < 2; hq++)
         {
            for (t = 0; t < ARRAY_SIZE(tempos); t++)
               transport_quality_case(floating, wide, hq, tempos[t], false);
            printf("   transport quality: %s %s %s complete\n", floating ? "float" : "int16",
                  wide ? "5.1" : "stereo", hq ? "HQ" : "normal");
            fflush(stdout);
         }
   printf("native transport quality: 48 cases, %u failures\n", failures - before);
}
