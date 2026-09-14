/* Configured wide inline transport before native channel routing. */
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
               settings->bools.audio_time_stretch = true;
               settings->bools.audio_time_stretch_lowpass = false;
               settings->bools.audio_fastforward_speedup = false;
               settings->floats.slowmotion_ratio = (float)duration;
               runloop_state_get_ptr()->flags = RUNLOOP_FLAG_SLOWMOTION;
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
               if (native) audio_driver_sample_batch_multi_float(input.f, 17, channels, layouts[l]);
               else audio_driver_sample_batch_multi_int16(input.i, 17, channels, layouts[l]);
               CHECK(saved->bypassed, "wide inline unsupported speed did not fall back");
               runloop_state_get_ptr()->flags = 0;
               settings->floats.slowmotion_ratio = 1;
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
            audio_driver_set_core_multi(wide);
            CHECK(!st->inline_transport, "negotiation enabled an unconfigured stage");
            settings->bools.audio_time_stretch = true;
            settings->bools.audio_time_stretch_lowpass = true;
            settings->floats.slowmotion_ratio = 2;
            runloop_state_get_ptr()->flags = RUNLOOP_FLAG_SLOWMOTION;
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
            CHECK(audio_driver_stop() && audio_stretch_stream_quiescent(saved->stream), "rebound stop");
            CHECK(audio_driver_start(false), "rebound restart");
            transport_fail_output = true;
            audio_driver_set_core_float(!native);
            transport_fail_output = false;
            CHECK(!st->inline_transport && st->core_float == !native, "failed rebind retained stale stage");
            runloop_state_get_ptr()->flags = 0;
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
   printf("inline format negotiation: %u cases, %u failures\n", cases, failures - before);
}
