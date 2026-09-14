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
            st->last_flush_time = 123;
            CHECK(audio_driver_stop() && audio_stretch_stream_quiescent(saved->stream), "rebound stop");
            CHECK(!st->last_flush_time, "rebound stop retained speed cadence");
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
               audio_driver_frame_end();
               CHECK(!callback_dispatch() && callback_calls == calls
                     && cap_frames == frames, "paused callback advanced source/output");
               runloop_state_get_ptr()->flags &= ~RUNLOOP_FLAG_PAUSED;
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
               CHECK(progress == (cap_frames != frames),
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
