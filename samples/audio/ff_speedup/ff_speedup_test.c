/* Fast-forward audio speedup: who measures the speed, and from what.
 *
 * audio_driver_fastforward_ratio_mult() tracks the wall-clock interval
 * between flushes against the interval a 1.0x core would produce, and
 * the resampler ratio is scaled by the result so fast-forward audio
 * pitches up with the speed instead of dropping out. The contract:
 *
 *  - The measurement is of the core's cadence. On the inline pipeline
 *    the flush runs on the core's thread, so the flush interval is
 *    that. On the threaded pipeline the flush runs on the audio
 *    thread, whose interval is the device draining the previous chunk
 *    - i.e. the previous multiplier played back, which measuring only
 *    confirms. There the producer counts source and measures at frame end,
 *    the consumer takes the figure from pipe_ff_mult_q16; nothing the
 *    consumer does can move it. Splitting a frame into publishes must not
 *    change the estimate, and source dropped by a full ring still counts.
 *  - The first sample of a fast-forward seeds the average at the
 *    configured ratio, and releasing fast-forward arms that seed again.
 *    The first inline flush plays at 1.0. The idle time between two fast-forwards is not a
 *    flush interval and must not enter the average: read as one, it
 *    pins the multiplier at AUDIO_MAX_RATIO and the audio plays back
 *    sixteen times too slow.
 *
 * Includes audio/audio_driver.c so the shipping functions run, with
 * the clock replaced by a counter the test advances. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <features/features_cpu.h>

static retro_time_t fake_now;
static retro_time_t fake_time_usec(void) { return fake_now; }
#define cpu_features_get_time_usec fake_time_usec

#include "../../../audio/audio_driver.c"

/* windows.h, reached through the driver, defines near as nothing. */
#ifdef near
#undef near
#endif

static int failures;

#define CHECK(cond, what) do { \
   if (!(cond)) { printf("FAIL: %s\n", what); failures++; } \
   else printf("   ok: %s\n", what); } while (0)

#define RATE   48000.0
#define FRAMES 800                 /* one 60 Hz frame of core audio */
#define ONE_X  ((retro_time_t)(FRAMES * 1000000.0 / RATE))

static bool near(double got, double want)
{
   return fabs(got - want) <= want * 0.05;
}

static void fresh(void)
{
   memset(&audio_driver_st, 0, sizeof(audio_driver_st));
   audio_driver_st.input = RATE;
   fake_now              = 1000000;
}

/* --- the measurement at the core's cadence ------------------------- */

static void test_inline_measures_speed(void)
{
   int i;
   double m;

   fresh();
   m = audio_driver_fastforward_ratio_mult(&audio_driver_st, FRAMES);
   CHECK(m == 1.0, "first sample of a fast-forward is unity");

   for (i = 0; i < 64; i++)
   {
      fake_now += ONE_X / 4;
      m = audio_driver_fastforward_ratio_mult(&audio_driver_st, FRAMES);
   }
   CHECK(near(m, 0.25), "64 flushes at 4x settle to a 0.25 multiplier");

   for (i = 0; i < 64; i++)
   {
      fake_now += ONE_X * 2;
      m = audio_driver_fastforward_ratio_mult(&audio_driver_st, FRAMES);
   }
   CHECK(near(m, 2.0), "and follow a core slower than realtime to 2.0");
}

/* N64 cores publish 185-551 frame batches at a steady cadence. A per-batch
 * figure swings with the batch size, and pitch-preserving playback run at
 * those figures falls short of real time. */
static void test_inline_uneven_batches(void)
{
   static const size_t sizes[2] = { 185, 551 };
   const retro_time_t step      = (retro_time_t)(
         (sizes[0] + sizes[1]) / 2 * 1000000.0 / RATE) / 4;
   int i;
   bool steady                  = true;

   fresh();
   audio_driver_fastforward_ratio_mult(&audio_driver_st, sizes[0]);
   for (i = 1; i < 256; i++)
   {
      double m;
      fake_now += step;
      m         = audio_driver_fastforward_ratio_mult(&audio_driver_st, sizes[i & 1]);
      if (i >= 128 && !near(m, 0.25))
         steady = false;
   }
   CHECK(steady, "uneven batches at 4x hold a 0.25 multiplier, not one per batch size");
}

/* The first flush of a hold starts at the configured ratio, not at 1.0:
 * the limiter has the core there within a frame, and the average would
 * otherwise spend the whole of a short hold catching up while the
 * non-blocking device overfills. */
static void test_inline_seeds_at_ratio(void)
{
   double m;

   fresh();
   config_get_ptr()->floats.fastforward_ratio = 3.0f;
   audio_driver_publish_runloop();
   m = audio_driver_fastforward_ratio_mult(&audio_driver_st, FRAMES);
   CHECK(near(m, 1.0 / 3.0), "the first flush of a hold starts at the configured ratio");
   fake_now += ONE_X / 3;
   m = audio_driver_fastforward_ratio_mult(&audio_driver_st, FRAMES);
   CHECK(near(m, 1.0 / 3.0), "and a core at that speed holds it from the first interval");
   fresh();
   audio_driver_publish_runloop();
   m = audio_driver_ff_mult(&audio_driver_st, FRAMES);
   CHECK(m == 1.0, "the first inline flush of a hold spans a 1.0x frame and plays as one");
   fake_now += ONE_X / 3;
   m = audio_driver_ff_mult(&audio_driver_st, FRAMES);
   CHECK(near(m, 1.0 / 3.0), "and the next plays at the seeded ratio");
   config_get_ptr()->floats.fastforward_ratio = 0.0f;
   fresh();
   audio_driver_publish_runloop();
   m = audio_driver_fastforward_ratio_mult(&audio_driver_st, FRAMES);
   CHECK(near(m, 1.0), "an uncapped ratio still starts at 1.0");
}

/* The threaded producer's first frame end publishes the configured
 * ratio: the consumer applies it to what the ring already holds, and
 * 1.0 there would drop the transport out of its tempo mid-engage. */
static void test_threaded_seed_publishes_ratio(void)
{
   int16_t block[FRAMES * 2];
   settings_t *settings = config_get_ptr();

   fresh();
   memset(block, 0, sizeof(block));
   settings->bools.audio_fastforward_speedup = true;
   settings->floats.fastforward_ratio        = 3.0f;
   audio_driver_publish_runloop();
   audio_driver_st.pipe_threaded    = true;
   audio_driver_st.pipe_frame_bytes = 2 * sizeof(int16_t);
   audio_driver_st.pipe_pass_frames = FRAMES;
   AUDIO_FLAGS_SET(&audio_driver_st, AUDIO_FLAG_PIPELINE_THREADED);
   if (!retro_spsc_init(&audio_driver_st.pipe_ring, 4096))
   {
      CHECK(false, "ring allocated");
      return;
   }
   retro_eventcount_init(&audio_driver_st.pipe_space);
   retro_eventcount_init(&audio_driver_st.pipe_data);
   retro_atomic_store_release_int(&audio_driver_st.pipe_ff_mult_q16, 65536);

   audio_driver_submit(&audio_driver_st, 1.0f, block, FRAMES * 2, false, false, true, true);
   audio_driver_frame_end();
   CHECK(near((double)retro_atomic_load_acquire_int(
               &audio_driver_st.pipe_ff_mult_q16) / 65536.0, 1.0 / 3.0),
         "the threaded first frame end publishes the configured ratio");

   settings->bools.audio_fastforward_speedup = false;
   settings->floats.fastforward_ratio        = 0.0f;
   audio_driver_publish_runloop();
   retro_spsc_free(&audio_driver_st.pipe_ring);
   retro_eventcount_free(&audio_driver_st.pipe_space);
   retro_eventcount_free(&audio_driver_st.pipe_data);
}


/* --- the pause tail and the resume ramp ---------------------------- */

/* A capturing float device for the fade fixtures: pause_fade() writes
 * the tail straight to the device, so the capture is the tail. */
#define PAUSE_CAP_MAX 4096
static float  pause_cap[PAUSE_CAP_MAX * 2];
static size_t pause_cap_frames;
static size_t pause_dev_room = PAUSE_CAP_MAX;

static ssize_t pause_dev_write(void *data, const void *buf, size_t len)
{
   size_t frames = len / (2 * sizeof(float));
   (void)data;
   if (frames > PAUSE_CAP_MAX - pause_cap_frames)
      frames = PAUSE_CAP_MAX - pause_cap_frames;
   memcpy(pause_cap + pause_cap_frames * 2, buf,
         frames * 2 * sizeof(float));
   pause_cap_frames += frames;
   return (ssize_t)len;
}
static size_t pause_dev_write_avail(void *data)
{
   (void)data;
   return pause_dev_room * 2 * sizeof(float);
}
static audio_driver_t pause_cap_driver;

#define PAUSE_SINE_HZ     200.0
#define PAUSE_SINE_AMP    0.5f
#define PAUSE_SINE_PERIOD 240 /* 48000 / 200 */

static void pause_up(void)
{
   unsigned i;
   fresh();
   memset(&pause_cap_driver, 0, sizeof(pause_cap_driver));
   pause_cap_driver.write       = pause_dev_write;
   pause_cap_driver.write_avail = pause_dev_write_avail;
   pause_cap_driver.ident       = "pause-cap";
   audio_driver_st.current_audio      = &pause_cap_driver;
   audio_driver_st.context_audio_data = (void*)&pause_cap_driver;
   AUDIO_FLAGS_SET(&audio_driver_st, AUDIO_FLAG_ACTIVE | AUDIO_FLAG_USE_FLOAT);
   pause_cap_frames = 0;
   pause_dev_room   = PAUSE_CAP_MAX;
   /* A device that has been playing a 200 Hz tone: the history a real
    * session accumulates through audio_driver_pause_track(). */
   for (i = 0; i < AUDIO_PAUSE_HIST_FRAMES; i++)
   {
      float v = PAUSE_SINE_AMP * (float)sin(2.0 * M_PI * PAUSE_SINE_HZ
            * (double)i / RATE);
      /* The last half period sits 0.3 above the cycles before it, the
       * way real content moves: the period the search picks continues
       * the older cycle, and only the join correction can carry the
       * level the stream actually stopped at. */
      if (i >= AUDIO_PAUSE_HIST_FRAMES - PAUSE_SINE_PERIOD / 2)
         v += 0.3f;
      audio_driver_st.pause_hist[(i * 2) + 0] = v;
      audio_driver_st.pause_hist[(i * 2) + 1] = v;
   }
   audio_driver_st.pause_hist_pos  = 0; /* next slot: i==HIST-1 is newest */
   audio_driver_st.pause_hist_fill = AUDIO_PAUSE_HIST_FRAMES;
   audio_driver_st.last_out[0] =
         audio_driver_st.pause_hist[(AUDIO_PAUSE_HIST_FRAMES - 1) * 2];
   audio_driver_st.last_out[1] = audio_driver_st.last_out[0];
}

static double pause_rms(const float *buf, size_t frames)
{
   double acc = 0.0;
   size_t i;
   for (i = 0; i < frames * 2; i++)
      acc += (double)buf[i] * (double)buf[i];
   return frames ? sqrt(acc / (double)(frames * 2)) : 0.0;
}

static void test_pause_tail_continues_the_waveform(void)
{
   unsigned crossings = 0;
   size_t i;
   double head, mid, tail_rms, hz;

   pause_up();
   audio_driver_pause_fade(true);

   CHECK(pause_cap_frames > 0 && pause_cap_frames <= AUDIO_PAUSE_TAIL_FRAMES,
         "the pause writes one tail, at most the tail length");

   CHECK(fabs((double)pause_cap[0] - (double)audio_driver_st.pause_hist[
            (AUDIO_PAUSE_HIST_FRAMES - 1) * 2]) < 0.3 * PAUSE_SINE_AMP,
         "the tail opens where the stream stopped, not at a level of its own");
   head = pause_rms(pause_cap, 32);
   mid  = pause_rms(pause_cap + (pause_cap_frames / 2) * 2, 32);
   tail_rms = pause_rms(pause_cap + (pause_cap_frames - 32) * 2, 32);
   CHECK(head > mid && mid > tail_rms,
         "the envelope only decays");
   CHECK(tail_rms < 0.05 * head,
         "and reaches silence by the end");
   for (i = 1; i < pause_cap_frames; i++)
      if ((pause_cap[(i - 1) * 2] <= 0.0f) != (pause_cap[i * 2] <= 0.0f))
         crossings++;
   hz = (double)crossings * 0.5 * RATE / (double)pause_cap_frames;
   CHECK(hz > PAUSE_SINE_HZ * 0.7 && hz < PAUSE_SINE_HZ * 1.3,
         "the tail keeps the waveform's own pitch, as concealment should");
   CHECK(audio_driver_st.pause_mute_frames == AUDIO_PAUSE_MUTE_FRAMES,
         "the pipeline's leftovers behind the tail are marked for dropping");
   CHECK(audio_driver_core_silenced(),
         "the core's audio is gated between the tail and the resume");
   CHECK(audio_driver_st.last_out[0] == 0.0f,
         "the stream ends at silence for the next tail to continue from");
}

static void test_pause_tail_fits_the_room_left(void)
{
   pause_up();
   AUDIO_FLAGS_SET(&audio_driver_st, AUDIO_FLAG_NONBLOCK);
   pause_dev_room = 64;
   audio_driver_pause_fade(true);
   CHECK(pause_cap_frames > 0 && pause_cap_frames <= 64,
         "a full non-blocking device gets a tail cut to the room left");
   CHECK(fabs((double)pause_cap[(pause_cap_frames - 1) * 2]) < 0.05,
         "which still reaches silence");
}

static void test_resume_ramp_waits_for_core_audio(void)
{
   float buf[AUDIO_PAUSE_TAIL_FRAMES * 2];
   size_t i;
   unsigned muted;

   pause_up();
   audio_driver_pause_fade(true);
   pause_cap_frames = 0;

   /* What the resampler still held comes out behind the tail: dropped. */
   for (i = 0; i < AUDIO_PAUSE_TAIL_FRAMES * 2; i++) buf[i] = 1.0f;
   audio_driver_pause_track(&audio_driver_st, buf, AUDIO_PAUSE_MUTE_FRAMES / 2, true);
   CHECK(buf[0] == 0.0f && buf[AUDIO_PAUSE_MUTE_FRAMES - 2] == 0.0f,
         "leftover frames after the tail are muted");
   muted = audio_driver_st.pause_mute_frames;
   CHECK(muted == AUDIO_PAUSE_MUTE_FRAMES / 2,
         "half the mute is spent on half the frames");

   audio_driver_pause_fade(false);
   CHECK(!audio_driver_core_silenced(), "a resume ungates the core's audio");
   CHECK(audio_driver_st.fade_in_pending && audio_driver_st.resume_topup_pending,
         "and owes a ramp and a top-up to the first core audio");
   CHECK(audio_driver_st.pause_mute_frames == 0,
         "but nothing of the old pipeline is dropped from it");

   /* The menu's silence in between must not spend the ramp. */
   for (i = 0; i < 64 * 2; i++) buf[i] = 0.0f;
   audio_driver_pause_track(&audio_driver_st, buf, 64, true);
   CHECK(audio_driver_st.fade_in_pending && !audio_driver_st.fade_in_frames,
         "the menu's own silence does not spend the resume ramp");

   /* The core's first audio does. */
   audio_driver_arm_resume(&audio_driver_st);
   CHECK(!audio_driver_st.fade_in_pending
         && audio_driver_st.fade_in_frames == AUDIO_PAUSE_TAIL_FRAMES,
         "the core's first audio arms the ramp");
   for (i = 0; i < AUDIO_PAUSE_TAIL_FRAMES * 2; i++) buf[i] = 1.0f;
   audio_driver_pause_track(&audio_driver_st, buf, AUDIO_PAUSE_TAIL_FRAMES, true);
   CHECK(buf[0] < 0.05f, "the ramp opens from silence");
   CHECK(buf[(AUDIO_PAUSE_TAIL_FRAMES / 2) * 2] > 0.3f
         && buf[(AUDIO_PAUSE_TAIL_FRAMES / 2) * 2] < 0.7f,
         "stands halfway at its middle");
   CHECK(buf[(AUDIO_PAUSE_TAIL_FRAMES - 1) * 2] > 0.99f,
         "and hands the stream over at full level");
   CHECK(audio_driver_st.fade_in_frames == 0, "spent in full");

   /* A resume that never had a pause ramps nothing: the audio never
    * stopped, and a ramp would be a dip in the middle of it. */
   audio_driver_pause_fade(false);
   CHECK(!audio_driver_st.fade_in_pending,
         "a resume without its pause owes no ramp");
}

static void test_jump_bracket_leaves_a_down_stream_down(void)
{
   bool ramped;
   pause_up();
   ramped = audio_driver_jump_fade_begin();
   CHECK(ramped, "the first bracket takes the stream down itself");
   CHECK(!audio_driver_jump_fade_begin(),
         "a bracket inside a pause leaves the stream to what took it down");
   audio_driver_jump_fade_end(false);
   CHECK(audio_driver_core_silenced(),
         "and its end does not bring the stream back up");
   audio_driver_jump_fade_end(ramped);
   CHECK(!audio_driver_core_silenced() && audio_driver_st.fade_in_pending,
         "the owning bracket's end does, on the core's next audio");
}

/* --- release and re-entry ------------------------------------------ */

static void test_reentry_ignores_idle_gap(void)
{
   int i;
   double m, peak = 0.0;

   fresh();
   audio_driver_fastforward_ratio_mult(&audio_driver_st, FRAMES);
   for (i = 0; i < 64; i++)
   {
      fake_now += ONE_X / 4;
      audio_driver_fastforward_ratio_mult(&audio_driver_st, FRAMES);
   }

   /* Fast-forward released; a minute at 1.0x; pressed again. */
   audio_driver_ff_mult_reset(&audio_driver_st);
   fake_now += 60 * 1000000;

   m = audio_driver_fastforward_ratio_mult(&audio_driver_st, FRAMES);
   CHECK(m == 1.0, "re-entry after a minute idle starts at unity, not the gap");

   for (i = 0; i < 64; i++)
   {
      fake_now += ONE_X / 4;
      m = audio_driver_fastforward_ratio_mult(&audio_driver_st, FRAMES);
      if (m > peak)
         peak = m;
   }
   CHECK(peak <= 1.0, "the multiplier never rises above unity on the way to 4x");
   CHECK(near(m, 0.25), "and settles to 0.25 again");
}

/* --- the threaded handoff ------------------------------------------ */

static void test_threaded_consumer_takes_producer_figure(void)
{
   int i;
   double m;
   retro_time_t before;

   fresh();
   audio_driver_st.pipe_threaded = true;
   retro_atomic_store_release_int(&audio_driver_st.pipe_ff_mult_q16,
         (int)(0.25 * 65536.0));

   /* The consumer flushes at the device's cadence: the previous chunk
    * at 0.25 played back is a quarter of the 1.0x interval, which is
    * what the old consumer-side measurement would have read as 4x -
    * and then, as the ratio it set drains, whatever it set last. */
   before = audio_driver_st.last_flush_time;
   for (i = 0; i < 64; i++)
   {
      fake_now += ONE_X * 4;
      m = audio_driver_ff_mult(&audio_driver_st, FRAMES);
   }
   CHECK(m == 0.25, "consumer reads 0.25 whatever its own cadence");
   CHECK(audio_driver_st.last_flush_time == before,
         "and its flushes do not enter the measurement");

   /* Inline: the same call measures. */
   audio_driver_st.pipe_threaded = false;
   m = audio_driver_ff_mult(&audio_driver_st, FRAMES);
   CHECK(m == 1.0 && audio_driver_st.last_flush_time == fake_now,
         "inline: the same call measures at the flush");
}

/* --- the producer end to end ---------------------------------------- */

static void test_producer_publishes_at_its_cadence(void)
{
   int i;
   int16_t block[FRAMES * 2];
   settings_t *settings = config_get_ptr();
   double got;

   fresh();
   memset(block, 0, sizeof(block));
   settings->bools.audio_fastforward_speedup = true;
   audio_driver_publish_runloop();
   audio_driver_st.pipe_threaded    = true;
   audio_driver_st.pipe_frame_bytes = 2 * sizeof(int16_t);
   audio_driver_st.pipe_pass_frames = FRAMES;
   AUDIO_FLAGS_SET(&audio_driver_st, AUDIO_FLAG_PIPELINE_THREADED);
   if (!retro_spsc_init(&audio_driver_st.pipe_ring, 4096))
   {
      CHECK(false, "ring allocated");
      return;
   }
   retro_eventcount_init(&audio_driver_st.pipe_space);
   retro_eventcount_init(&audio_driver_st.pipe_data);
   retro_atomic_store_release_int(&audio_driver_st.pipe_ff_mult_q16, 65536);

   /* A 1.0x publish first, so the re-entry seed is armed. */
   audio_driver_submit(&audio_driver_st, 1.0f, block, FRAMES * 2, false, false, false, true);

   /* Then fast-forward at 4x, with nobody draining the ring: every
    * block past the first is dropped, and still counts. */
   for (i = 0; i < 64; i++)
   {
      fake_now += ONE_X / 4;
      audio_driver_submit(&audio_driver_st, 1.0f, block, FRAMES * 2, false, false, true, true);
      audio_driver_frame_end();
   }
   got = (double)retro_atomic_load_acquire_int(
         &audio_driver_st.pipe_ff_mult_q16) / 65536.0;
   CHECK(near(got, 0.25), "producer at 4x into a full ring publishes 0.25");

   /* Released: a 1.0x publish re-arms the seed. */
   audio_driver_submit(&audio_driver_st, 1.0f, block, FRAMES * 2, false, false, false, true);
   CHECK(audio_driver_st.last_flush_time == 0,
         "a publish outside fast-forward re-arms the seed");

   retro_spsc_free(&audio_driver_st.pipe_ring);
   retro_eventcount_free(&audio_driver_st.pipe_space);
   retro_eventcount_free(&audio_driver_st.pipe_data);
}

static void test_fragmented_frame_cadence(void)
{
   static const unsigned batches[] = { 1, 2, 64, 262, 312 };
   int16_t block[FRAMES * 4];
   int reference = 0;
   unsigned b, frame;
   memset(block, 0, sizeof(block));
   for (b = 0; b < sizeof(batches) / sizeof(batches[0]); b++)
   {
      fresh();
      runloop_state_get_ptr()->flags |= RUNLOOP_FLAG_FASTMOTION;
      config_get_ptr()->bools.audio_fastforward_speedup = true;
      audio_driver_publish_runloop();
      audio_driver_st.pipe_threaded = true;
      audio_driver_st.pipe_channels = 2;
      audio_driver_st.pipe_frame_bytes = 2 * sizeof(int16_t);
      audio_driver_st.pipe_pass_frames = FRAMES;
      AUDIO_FLAGS_SET(&audio_driver_st, AUDIO_FLAG_PIPELINE_THREADED);
      if (!retro_spsc_init(&audio_driver_st.pipe_ring, 4096)) abort();
      if (!retro_eventcount_init(&audio_driver_st.pipe_space)
            || !retro_eventcount_init(&audio_driver_st.pipe_data)) abort();
      retro_atomic_store_release_int(&audio_driver_st.pipe_ff_mult_q16, 65536);
      for (frame = 0; frame < 128; frame++)
      {
         unsigned part;
         size_t used = 0;
         fake_now += ONE_X / 4;
         for (part = 0; part < batches[b]; part++)
         {
            size_t n = part + 1 == batches[b] ? FRAMES - used : FRAMES / batches[b];
            audio_driver_submit(&audio_driver_st, 1.0f, block + used * 2,
                  n * 2, false, false, true, true);
            used += n;
         }
         audio_driver_frame_end();
      }
      {
         int got = retro_atomic_load_acquire_int(&audio_driver_st.pipe_ff_mult_q16);
         if (!b) reference = got;
         CHECK(got == reference && near(got / 65536.0, 0.25),
               "4x frame estimate is invariant under fragmented/dropped publishes");
      }
      for (frame = 0; frame < 128; frame++)
      {
         fake_now += ONE_X / 4;
         if (frame & 1)
            audio_driver_submit(&audio_driver_st, 1.0f, block, FRAMES * 4,
                  false, false, true, true);
         audio_driver_frame_end();
      }
      CHECK(near(retro_atomic_load_acquire_int(&audio_driver_st.pipe_ff_mult_q16)
               / 65536.0, 0.25), "sparse audio frames retain the measured 4x cadence");
      runloop_state_get_ptr()->flags |= RUNLOOP_FLAG_PAUSED;
      fake_now += 60000000;
      audio_driver_frame_end();
      runloop_state_get_ptr()->flags &= ~RUNLOOP_FLAG_PAUSED;
      audio_driver_publish_runloop();
      audio_driver_submit(&audio_driver_st, 1.0f, block, FRAMES * 2, false, false, true, true);
      audio_driver_frame_end();
      CHECK(retro_atomic_load_acquire_int(&audio_driver_st.pipe_ff_mult_q16) == 65536,
            "paused fast-forward resumes from unity");
      runloop_state_get_ptr()->flags &= ~RUNLOOP_FLAG_FASTMOTION;
      fake_now += 10000000;
      audio_driver_frame_end(); /* silent frames re-arm the seed */
      audio_driver_submit(&audio_driver_st, 1.0f, block, FRAMES * 2, false, false, true, true);
      audio_driver_frame_end();
      CHECK(retro_atomic_load_acquire_int(&audio_driver_st.pipe_ff_mult_q16) == 65536,
            "source resumes from unity after an empty frame outside fast-forward");
      retro_spsc_free(&audio_driver_st.pipe_ring);
      retro_eventcount_free(&audio_driver_st.pipe_space);
      retro_eventcount_free(&audio_driver_st.pipe_data);
   }
}

/* --- the producer at a full ring in fast-forward -------------------- */

static void test_producer_holds_at_a_full_ring(void)
{
   int16_t block[FRAMES * 2];
   static int16_t stage_out[FRAMES * 8];
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings     = config_get_ptr();
   size_t cap, fill, tail;
   double trim, m;
   int i;

   fresh();
   memset(block, 0, sizeof(block));
   settings->bools.audio_fastforward_speedup = true;
   settings->bools.audio_sync                = true;
   settings->floats.fastforward_ratio        = 3.0f;
   runloop_state_get_ptr()->flags           |= RUNLOOP_FLAG_FASTMOTION;
   audio_driver_publish_runloop();
   st->pipe_threaded    = true;
   st->pipe_channels    = 2;
   st->pipe_frame_bytes = 2 * sizeof(int16_t);
   st->pipe_pass_frames = FRAMES;
   AUDIO_FLAGS_SET(st, AUDIO_FLAG_PIPELINE_THREADED | AUDIO_FLAG_STARTED);
   if (!retro_spsc_init(&st->pipe_ring, 4096)) abort();
   if (!retro_eventcount_init(&st->pipe_space)
         || !retro_eventcount_init(&st->pipe_data)) abort();
   audio_pipeline_layout_init(&st->pipe_layouts, AUDIO_LAYOUT_STEREO);
   retro_atomic_store_release_int(&st->pipe_ff_mult_q16, 65536);
   /* Nobody drains the ring here: a found stall ends the wait at once. */
   retro_atomic_store_release_int(&st->pipe_stalled, 1);

   /* The first frame of a hold publishes the seed before its frame end. */
   audio_driver_submit(st, 1.0f, block, FRAMES * 2, false, false, true, true);
   CHECK(retro_atomic_load_acquire_int(&st->pipe_ff_mult_q16) == (int)(65536.0 / 3.0),
         "the first fast-forward publish seeds the multiplier at the ratio");

   /* Waiting needs audio sync, a following transport and a limiter. */
   CHECK(audio_driver_pipe_ff_waits(st), "a limited hold with audio sync waits at a full ring");
   settings->floats.fastforward_ratio = 0.0f;
   audio_driver_publish_runloop();
   CHECK(!audio_driver_pipe_ff_waits(st), "an unlimited hold drops instead");
   settings->floats.fastforward_ratio = 3.0f;
   settings->bools.audio_sync         = false;
   audio_driver_publish_runloop();
   CHECK(!audio_driver_pipe_ff_waits(st), "and so does one without audio sync");
   settings->bools.audio_sync         = true;
   audio_driver_publish_runloop();

   /* The published figure carries the ring-fill trim, so the consumer's
    * pull and the ratio its pass is sized at are one number. */
   audio_driver_frame_end();
   cap  = st->pipe_ring.capacity;
   fill = retro_spsc_read_avail(&st->pipe_ring);
   trim = 1.0 + 0.5 * ((double)fill - cap * 0.5) / cap;
   if (trim > 1.08) trim = 1.08;
   m    = retro_atomic_load_acquire_int(&st->pipe_ff_mult_q16) / 65536.0;
   CHECK(fill > cap / 2 && fabs(m * trim * 3.0 - 1.0) < 0.005,
         "a filling ring trims the published multiplier down");
   CHECK(audio_driver_ff_mult(st, FRAMES) == m, "and the consumer takes it untouched");
   fake_now += ONE_X / 3;
   audio_driver_submit(st, 1.0f, block, FRAMES * 2, false, false, true, true);
   retro_spsc_skip(&st->pipe_ring, retro_spsc_read_avail(&st->pipe_ring));
   audio_driver_frame_end();
   m    = retro_atomic_load_acquire_int(&st->pipe_ff_mult_q16) / 65536.0;
   CHECK(fabs(m * 0.92 * 3.0 - 1.0) < 0.005, "an empty ring trims it up, within the bound");
   settings->bools.audio_sync = false;
   audio_driver_publish_runloop();
   for (i = 0; i < 2; i++)
   {
      fake_now += ONE_X / 3;
      audio_driver_submit(st, 1.0f, block, FRAMES * 2, false, false, true, true);
   }
   audio_driver_frame_end();
   m    = retro_atomic_load_acquire_int(&st->pipe_ff_mult_q16) / 65536.0;
   CHECK(retro_spsc_write_avail(&st->pipe_ring) < FRAMES * st->pipe_frame_bytes
         && fabs(m * 3.0 - 1.0) < 0.005, "without audio sync a full ring is not trimmed for");
   settings->bools.audio_sync = true;
   audio_driver_publish_runloop();

   /* The runloop's engage lands on what the ring holds; the release, and
    * any request made directly, stay behind it. */
   st->pipe_transport = audio_pipeline_stretch_new(48000, 2, false, 3,
         &st->pipe_ring, &st->pipe_layouts, stage_out, FRAMES * 4);
   if (!st->pipe_transport) abort();
   retro_spsc_skip(&st->pipe_ring, retro_spsc_read_avail(&st->pipe_ring));
   retro_spsc_write(&st->pipe_ring, block, FRAMES * st->pipe_frame_bytes);
   CHECK(audio_driver_pipeline_transport_request(3 * 65536, true, false, 0)
         && st->pipe_layouts.events[0].position
         == retro_atomic_load_relaxed_size(&st->pipe_ring.head),
         "a direct request lands behind the ring's source");
   audio_pipeline_layout_init(&st->pipe_layouts, AUDIO_LAYOUT_STEREO);
   tail = retro_atomic_load_relaxed_size(&st->pipe_ring.tail);
   CHECK(audio_driver_pipeline_transport_publish(3 * 65536, true, false, 0, true),
         "the engage tempo is published");
   CHECK(st->pipe_layouts.events[0].position == tail,
         "and lands at the consumer's position, ahead of the ring's source");
   CHECK(audio_driver_pipeline_transport_request(65536, false, false, 0),
         "the release is requested");
   CHECK(st->pipe_layouts.events[1].position
         == retro_atomic_load_relaxed_size(&st->pipe_ring.head),
         "and lands behind the source the hold produced");
   audio_pipeline_stretch_free(st->pipe_transport);
   st->pipe_transport = NULL;

   settings->floats.fastforward_ratio = 0.0f;
   runloop_state_get_ptr()->flags &= ~RUNLOOP_FLAG_FASTMOTION;
   audio_driver_publish_runloop();
   retro_spsc_free(&st->pipe_ring);
   retro_eventcount_free(&st->pipe_space);
   retro_eventcount_free(&st->pipe_data);
}

/* A core measured slow, then running at 3x into a ring nobody drains:
 * the estimate has to follow it there. Unlimited, the producer never
 * waits and the figure is untrimmed; limited, it is trimmed for a full
 * ring. */
static void test_full_ring_follows_the_core(void)
{
   static const float ratios[2] = { 0.0f, 3.0f };
   int16_t block[FRAMES * 2];
   audio_driver_state_t *st = &audio_driver_st;
   settings_t *settings     = config_get_ptr();
   unsigned r;
   int i;

   memset(block, 0, sizeof(block));
   for (r = 0; r < 2; r++)
   {
      double m;
      fresh();
      settings->bools.audio_fastforward_speedup = true;
      settings->bools.audio_sync                = true;
      settings->floats.fastforward_ratio        = ratios[r];
      runloop_state_get_ptr()->flags           |= RUNLOOP_FLAG_FASTMOTION;
      audio_driver_publish_runloop();
      st->pipe_threaded    = true;
      st->pipe_channels    = 2;
      st->pipe_frame_bytes = 2 * sizeof(int16_t);
      st->pipe_pass_frames = FRAMES;
      AUDIO_FLAGS_SET(st, AUDIO_FLAG_PIPELINE_THREADED | AUDIO_FLAG_STARTED);
      if (!retro_spsc_init(&st->pipe_ring, 4096)) abort();
      if (!retro_eventcount_init(&st->pipe_space)
            || !retro_eventcount_init(&st->pipe_data)) abort();
      audio_pipeline_layout_init(&st->pipe_layouts, AUDIO_LAYOUT_STEREO);
      retro_atomic_store_release_int(&st->pipe_ff_mult_q16, 65536);
      retro_atomic_store_release_int(&st->pipe_stalled, 1);

      for (i = 0; i < 64; i++)
      {
         retro_spsc_skip(&st->pipe_ring, retro_spsc_read_avail(&st->pipe_ring));
         fake_now += ONE_X * 2 / 3;
         audio_driver_submit(st, 1.0f, block, FRAMES * 2, false, false, true, true);
         audio_driver_frame_end();
      }
      for (i = 0; i < 256; i++)
      {
         fake_now += ONE_X / 3;
         audio_driver_submit(st, 1.0f, block, FRAMES * 2, false, false, true, true);
         audio_driver_frame_end();
      }
      m = retro_atomic_load_acquire_int(&st->pipe_ff_mult_q16) / 65536.0;
      if (r)
         CHECK(near(m, 1.0 / 3.0 / (1.0 + AUDIO_PIPE_FF_TRIM_MAX)),
               "a limited hold follows a faster core through a full ring, trimmed");
      else
         CHECK(near(m, 1.0 / 3.0),
               "an unlimited hold follows a faster core through a full ring, untrimmed");
      CHECK(audio_driver_ff_mult(st, FRAMES) * 65536.0
            == (double)retro_atomic_load_acquire_int(&st->pipe_ff_mult_q16) ,
            "the consumer takes the published figure as it is");

      settings->floats.fastforward_ratio = 0.0f;
      runloop_state_get_ptr()->flags &= ~RUNLOOP_FLAG_FASTMOTION;
      audio_driver_publish_runloop();
      retro_spsc_free(&st->pipe_ring);
      retro_eventcount_free(&st->pipe_space);
      retro_eventcount_free(&st->pipe_data);
   }
}

static unsigned silent_callbacks;
static void silent_callback(void) { silent_callbacks++; }

static void test_inline_silent_boundaries(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   unsigned callback, mode;
   for (callback = 0; callback < 2; callback++)
      for (mode = 0; mode < 3; mode++)
      {
         retro_time_t previous;
         fresh();
         silent_callbacks = 0;
         if (callback) st->callback.callback = silent_callback;
         runloop_state_get_ptr()->flags = RUNLOOP_FLAG_FASTMOTION;
         audio_driver_fastforward_ratio_mult(st, FRAMES);
         previous = st->last_flush_time;
         if (mode == 0) runloop_state_get_ptr()->flags = 0;
         if (mode == 1) runloop_state_get_ptr()->flags |= RUNLOOP_FLAG_PAUSED;
         fake_now += 60000000;
         audio_driver_frame_end();
         if (callback)
         {
            /* The frame boundary owns callback cadence now (see
             * audio_driver_ff_callback_frame_end()): opted out, the
             * published word never carries fastmotion, so the boundary
             * resets whatever the seed left in every mode. */
            CHECK(st->last_flush_time == 0,
                  "main frame resets opted-out callback cadence");
            audio_driver_callback();
            CHECK(silent_callbacks == (mode == 1 ? 0u : 1u), "paused callback does not run the core");
         }
         /* Fast-forward is never published while a callback core owns
          * audio (see audio_driver_publish_runloop()), so a callback
          * core's cadence resets in every mode - active fast-forward
          * included - and reentry always starts at unity. */
         CHECK((mode == 2 && !callback)
                  ? st->last_flush_time == previous : st->last_flush_time == 0,
               "silent boundaries reset released/paused cadence and preserve active batching");
         if (mode != 2 || callback)
            CHECK(audio_driver_fastforward_ratio_mult(st, FRAMES) == 1.0,
                  "inline silent-gap reentry starts at unity");
      }
   fresh();
   st->callback.callback = silent_callback;
   audio_driver_fastforward_ratio_mult(st, FRAMES);
   retro_atomic_store_release_int(&st->runloop_snapshot, AUDIO_SNAP_FASTMOTION
         | AUDIO_SNAP_MENU_ALIVE | AUDIO_SNAP_MENU_PAUSES);
   audio_driver_callback();
   CHECK(st->last_flush_time != 0, "menu without pause permission preserves cadence");
   retro_atomic_store_release_int(&st->runloop_snapshot, AUDIO_SNAP_FASTMOTION
         | AUDIO_SNAP_MENU_ALIVE | AUDIO_SNAP_MENU_PAUSES | AUDIO_SNAP_ALLOW_PAUSE);
   audio_driver_callback();
   CHECK(!st->last_flush_time, "menu-paused callback discards idle cadence");
   fresh();
   runloop_state_get_ptr()->flags = 0;
}

/* --- the opt-in: fast-forward affecting callback audio ------------- */

static void test_callback_optin(void)
{
   int i, m;
   settings_t *settings = config_get_ptr();

   /* Off (the default): publish withholds the fastmotion bit for a
    * callback core, the frame boundary keeps no cadence, and the
    * audio-thread flush reads unity. */
   fresh();
   settings->bools.audio_fastforward_callback = false;
   audio_driver_st.callback.callback = silent_callback;
   retro_atomic_store_release_int(&audio_driver_st.pipe_ff_mult_q16, 65536);
   runloop_state_get_ptr()->flags = RUNLOOP_FLAG_FASTMOTION;
   for (i = 0; i < 8; i++)
   {
      fake_now += ONE_X / 2;
      audio_driver_frame_end();
   }
   CHECK(!(retro_atomic_load_acquire_int(&audio_driver_st.runloop_snapshot)
            & AUDIO_SNAP_FASTMOTION),
         "callback opt-out leaves fastmotion unpublished");
   CHECK(!audio_driver_st.last_flush_time
         && retro_atomic_load_acquire_int(
            &audio_driver_st.pipe_ff_mult_q16) == 65536,
         "callback opt-out keeps fast-forward audio at unity");

   /* On: the bit publishes, the frame boundary is measured, and the
    * multiplier follows the achieved video speed - never the flush's
    * own device-drain cadence. Frames land at twice real time, so the
    * EMA settles near a half. */
   fresh();
   settings->bools.audio_fastforward_callback = true;
   audio_driver_st.callback.callback = silent_callback;
   retro_atomic_store_release_int(&audio_driver_st.pipe_ff_mult_q16, 65536);
   runloop_state_get_ptr()->flags = RUNLOOP_FLAG_FASTMOTION;
   for (i = 0; i < 60; i++)
   {
      fake_now += ONE_X / 2;
      audio_driver_frame_end();
   }
   CHECK(retro_atomic_load_acquire_int(&audio_driver_st.runloop_snapshot)
            & AUDIO_SNAP_FASTMOTION,
         "callback opt-in publishes fastmotion");
   m = retro_atomic_load_acquire_int(&audio_driver_st.pipe_ff_mult_q16);
   CHECK(m > 65536 / 4 && m < (65536 * 3) / 4,
         "callback opt-in multiplier follows the frame cadence");
   runloop_state_get_ptr()->flags = 0;
   audio_driver_frame_end();
   CHECK(!audio_driver_st.last_flush_time
         && retro_atomic_load_acquire_int(
            &audio_driver_st.pipe_ff_mult_q16) == 65536,
         "callback opt-in release returns to unity");
   settings->bools.audio_fastforward_callback = false;
   fresh();
   runloop_state_get_ptr()->flags = 0;
}

static bool lifecycle_stop_ok;
static bool lifecycle_stop(void *data) { (void)data; return lifecycle_stop_ok; }
static bool lifecycle_alive(void *data) { (void)data; return true; }
static bool lifecycle_start(void *data, bool shutdown)
{ (void)data; (void)shutdown; return true; }

static void test_stop_excludes_idle_gap(void)
{
   audio_driver_state_t *st = &audio_driver_st;
   audio_driver_t driver;
   unsigned threaded, success;
   memset(&driver, 0, sizeof(driver));
   driver.stop = lifecycle_stop;
   driver.start = lifecycle_start;
   driver.alive = lifecycle_alive;
   driver.ident = "cadence-lifecycle";
   for (threaded = 0; threaded < 2; threaded++)
      for (success = 0; success < 2; success++)
      {
         retro_time_t previous;
         fresh();
         st->pipe_threaded = threaded;
         st->current_audio = &driver;
         st->context_audio_data = &driver;
         AUDIO_FLAGS_SET(st, AUDIO_FLAG_ACTIVE | AUDIO_FLAG_STARTED);
         audio_driver_fastforward_ratio_mult(st, FRAMES);
         previous = st->last_flush_time;
         st->pipe_ff_frames = FRAMES;
         retro_atomic_store_release_int(&st->pipe_ff_mult_q16, 16384);
         lifecycle_stop_ok = success;
         CHECK(audio_driver_stop() == (bool)success, "stop reports the device result");
         if (!success)
         {
            CHECK(st->last_flush_time == previous && st->pipe_ff_frames == FRAMES
                  && retro_atomic_load_acquire_int(&st->pipe_ff_mult_q16) == 16384,
                  "failed stop preserves the running cadence");
            continue;
         }
         CHECK(!st->last_flush_time && !st->pipe_ff_frames,
               "successful stop discards the old source cadence");
         CHECK(retro_atomic_load_acquire_int(&st->pipe_ff_mult_q16) == 65536,
               "restart cannot consume an old speed multiplier");
         fake_now += 60000000;
         CHECK(audio_driver_start(false), "restart succeeds");
         CHECK(audio_driver_fastforward_ratio_mult(st, FRAMES) == 1.0,
               "first resumed source excludes the stopped interval");
      }
   fresh();
}

int main(void)
{
   printf("fast-forward audio speedup:\n");
   test_inline_measures_speed();
   test_inline_uneven_batches();
   test_inline_seeds_at_ratio();
   test_threaded_seed_publishes_ratio();
   test_pause_tail_continues_the_waveform();
   test_pause_tail_fits_the_room_left();
   test_resume_ramp_waits_for_core_audio();
   test_jump_bracket_leaves_a_down_stream_down();
   test_reentry_ignores_idle_gap();
   test_threaded_consumer_takes_producer_figure();
   test_producer_publishes_at_its_cadence();
   test_fragmented_frame_cadence();
   test_producer_holds_at_a_full_ring();
   test_full_ring_follows_the_core();
   test_stop_excludes_idle_gap();
   test_inline_silent_boundaries();
   test_callback_optin();

   if (failures)
   {
      printf("FAILED: %d\n", failures);
      return 1;
   }
   printf("ok: the speedup follows the core's cadence on both pipelines, "
         "never the device's, and re-entry starts from unity\n");
   return 0;
}
