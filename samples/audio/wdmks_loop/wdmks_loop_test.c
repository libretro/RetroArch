/* Regression test for the WaveRT loop in audio/drivers/wdmks.c.
 *
 * A WaveRT pin plays a mapped loop round and round; unlike a packet
 * pin it has no notion of running out. The frontend stops writing on
 * every pause, behind every menu the core is silent for, and on every
 * stall, and it does so on the understanding that a device with
 * nothing to play plays nothing. On this pin the loop kept playing
 * the last few milliseconds of audio over and over, at full level, for
 * as long as the pause lasted; and when writes resumed they landed
 * behind the hardware, to be played a whole loop late from then on.
 *
 * What this pins, with the driver's loop bookkeeping driven by hand:
 *
 *   played bytes are cleared     -> whatever the cursor passes is silence
 *                                   until it is written again
 *   audio runs out               -> the whole loop is silence, not a
 *                                   repeat of its tail
 *   writes resume after that     -> they land a little ahead of the
 *                                   hardware, not a loop behind it
 *   a full loop is not an overrun-> the cursor a frame behind the
 *                                   write cursor is left alone
 *   laps went by unobserved      -> the loop is cleared and resynced
 *   underruns                    -> one per gap in the audio: not the
 *                                   start, not every sample of a pause
 *   the margin                   -> the pin's reported FIFO plus half a
 *                                   millisecond where it reports one,
 *                                   two milliseconds where it does not
 *
 * The same contract on both paths: the caller-driven one and the
 * refill thread's pump, called directly, and with and without a FIFO
 * report. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* The driver whole, as wdmks_abi_check.c includes it. */
#include "audio/drivers/wdmks.c"

/* What the driver reaches for in RetroArch. */
void RARCH_LOG(const char *f, ...)  { (void)f; }
void RARCH_WARN(const char *f, ...) { (void)f; }
void RARCH_ERR(const char *f, ...)  { (void)f; }
void RARCH_DBG(const char *f, ...)  { (void)f; }
uint32_t audio_driver_requested_layout(void) { return AUDIO_LAYOUT_STEREO; }
void audio_driver_set_device_latency(size_t frames) { (void)frames; }

static unsigned failures = 0;

static void check(bool cond, const char *what)
{
   printf("  [%s] %s\n", cond ? "pass" : "FAIL", what);
   if (!cond)
      failures++;
}

#define RATE      48000
#define FRAME     4                      /* stereo s16 */
/* 200 ms: long enough that a descheduled test process does not trip
 * the driver's lost-lap watchdog between two of its own samples. */
#define LOOP_FRAMES (RATE / 5)
#define LOOP_BYTES  (LOOP_FRAMES * FRAME)

static ULONG fake_pos;

/* The hardware moves: the register goes forward by n bytes, wrapping
 * where the loop wraps. */
static void hw_play(size_t n)
{
   fake_pos = (ULONG)((fake_pos + n) % LOOP_BYTES);
}

/* What the pin reports as its FIFO for the run, in frames; 0 for a
 * pin that reports none. */
static unsigned fifo_frames;

static void loop_setup(wdmks_t *w)
{
   memset(w, 0, sizeof(*w));
   w->rt_fifo_bytes = fifo_frames * FRAME;
   w->stream.handle = (HANDLE)(uintptr_t)1;  /* not INVALID, never used */
   w->stream.looped = true;
   w->frame_bytes   = FRAME;
   w->rate          = RATE;
   w->running       = true;
   w->nonblock      = true;               /* no waits on a fake pin */
   w->clk_ppm       = AUDIO_CLOCK_PPM_NONE;
   w->rt_buf        = (unsigned char*)malloc(LOOP_BYTES);
   w->rt_size       = LOOP_BYTES;
   w->rt_pos        = &fake_pos;
   fake_pos         = 0;
   /* As wdmks_rt_get_buffer leaves it. */
   memset(w->rt_buf, 0, w->rt_size);
   w->rt_write         = 0;
   w->rt_played        = 0;
   w->rt_played_bytes  = 0;
   w->rt_written_bytes = 0;
   w->rt_have_last     = false;
   w->rt_origin        = true;
   w->rt_fed           = false;
   retro_atomic_size_init(&w->rt_underruns, 0);
   w->rt_ring_size     = w->rt_size;
   retro_spsc_init(&w->rt_ring, w->rt_ring_size);
   retro_eventcount_init(&w->rt_park);
   retro_atomic_int_init(&w->rt_run, 0);
   retro_atomic_64_init(&w->rt_frames_pub, 0);
   retro_atomic_int_init(&w->clk_ppm_pub, AUDIO_CLOCK_PPM_NONE);
}

static void loop_teardown(wdmks_t *w)
{
   retro_eventcount_free(&w->rt_park);
   retro_spsc_free(&w->rt_ring);
   free(w->rt_buf);
}

/* A byte pattern no silence matches: every byte non-zero. */
static void fill_tone(unsigned char *buf, size_t n, unsigned char seed)
{
   size_t i;
   for (i = 0; i < n; i++)
      buf[i] = (unsigned char)(seed + (i & 0x3f) + 1);
}

static bool region_is_silent(const wdmks_t *w, size_t from, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++)
      if (w->rt_buf[(from + i) % w->rt_size])
         return false;
   return true;
}

static bool region_matches(const wdmks_t *w, size_t from,
      const unsigned char *src, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++)
      if (w->rt_buf[(from + i) % w->rt_size] != src[i])
         return false;
   return true;
}

/* Where the first byte of a pattern sits in the loop, or the size if
 * it is nowhere. */
static size_t find_in_loop(const wdmks_t *w, const unsigned char *src,
      size_t n)
{
   size_t at;
   for (at = 0; at < w->rt_size; at++)
      if (region_matches(w, at, src, n))
         return at;
   return w->rt_size;
}

/* One write into the loop by whichever path is under test. Returns
 * what was placed. */
typedef size_t (*put_fn)(wdmks_t *w, const unsigned char *src, size_t n);

static size_t put_direct(wdmks_t *w, const unsigned char *src, size_t n)
{
   ssize_t r = wdmks_rt_write(w, src, n);
   return r < 0 ? 0 : (size_t)r;
}

/* The thread's path: the frontend ring, then one pump pass moves it
 * into the loop against the register. What the loop cannot take
 * would sit in the ring for a later pass, which is the thread's
 * normal life but not this test's: each step wants the loop as the
 * only place the audio is, so a pass settles the model first and the
 * write is cut to what the loop has room for. */
static size_t put_pump(wdmks_t *w, const unsigned char *src, size_t n)
{
   size_t room;
   wdmks_rt_pump_once(w);
   room = wdmks_rt_room(w);
   if (room > wdmks_rt_ring_room(w))
      room = wdmks_rt_ring_room(w);
   if (n > room)
      n = room;
   retro_spsc_write(&w->rt_ring, src, n);
   return wdmks_rt_pump_once(w);
}

/* A sample of the register on the path under test: the caller-driven
 * path samples on every room query, the thread on every pass. */
static void sample(wdmks_t *w, bool pump)
{
   if (pump)
      wdmks_rt_pump_once(w);
   else
      wdmks_rt_free(w);
}

static void run_contract(const char *name, put_fn put, bool pump)
{
   wdmks_t       w;
   unsigned char tone[LOOP_BYTES];
   unsigned char block[RATE / 250 * FRAME];   /* 4 ms */
   size_t        placed, margin, at, seen;

   printf("%s\n", name);
   loop_setup(&w);
   margin = wdmks_rt_margin(&w);
   check(margin % FRAME == 0 && margin >= FRAME && margin < LOOP_BYTES / 4 + 1,
         "the resync margin is whole frames, a frame or more, under a quarter loop");
   check(margin == (fifo_frames ? fifo_frames + RATE / 2000 : RATE * 2 / 1000) * FRAME,
         fifo_frames ? "sized from the reported FIFO plus half a millisecond"
                     : "two milliseconds where the pin reports no FIFO");

   /* The hardware has started and moved a little by the time the
    * first audio arrives. Fill the loop: the audio lands a margin
    * ahead of the hardware - not, as it used to, behind a cursor the
    * model took for full - and the write keeps a frame back, so this
    * is the loop less both. */
   hw_play(RATE / 125 * FRAME);                /* 8 ms */
   fill_tone(tone, sizeof(tone), 0x10);
   placed = put(&w, tone, sizeof(tone));
   at     = (fake_pos + margin) % LOOP_BYTES;
   check(placed == LOOP_BYTES - FRAME - margin,
         "the first write fills the loop from a margin ahead of the hardware");
   check(region_matches(&w, at, tone, placed), "and the audio is in it");
   check(region_is_silent(&w, fake_pos, margin), "behind that margin of silence");
   check(wdmks_underruns(&w) == 0, "and a start is not an underrun");

   /* Half a loop plays. What was played is silence now; what was not
    * is untouched. */
   hw_play(LOOP_BYTES / 2);
   sample(&w, pump);
   check(region_is_silent(&w, fake_pos + LOOP_BYTES / 2, LOOP_BYTES / 2),
         "half a loop played: the bytes behind the cursor are silence");
   check(region_matches(&w, fake_pos, tone + LOOP_BYTES / 2 - margin,
            placed + margin - LOOP_BYTES / 2),
         "and the bytes ahead of it are still the audio");
   check(w.rt_write == (at + placed) % LOOP_BYTES,
         "the write cursor was not moved: nothing overran");
   check(wdmks_rt_room(&w) == LOOP_BYTES / 2,
         "and half a loop is free again");

   /* Nothing more is written - a pause. The hardware plays out the
    * rest, wraps, and goes on for a quarter loop more. */
   hw_play(LOOP_BYTES / 2 + LOOP_BYTES / 4);
   sample(&w, pump);
   check(region_is_silent(&w, 0, LOOP_BYTES),
         "the audio ran out: the whole loop is silence, not its tail again");
   check(wdmks_underruns(&w) == 1, "and that is one underrun");
   hw_play(LOOP_BYTES / 2);
   sample(&w, pump);
   check(wdmks_underruns(&w) == 1,
         "the pause goes on: still one, not one per sample");

   /* Audio resumes. It has to land just ahead of where the hardware
    * is now, not where the write cursor was left - a loop behind. */
   fill_tone(block, sizeof(block), 0x40);
   placed = put(&w, block, sizeof(block));
   check(placed == sizeof(block), "the first write after the gap goes in");
   at = find_in_loop(&w, block, sizeof(block));
   check(at < LOOP_BYTES, "and is somewhere in the loop");
   check(at == (fake_pos + margin) % LOOP_BYTES,
         "a margin ahead of the hardware, not a loop behind it");
   check(region_is_silent(&w, fake_pos, margin),
         "with silence between the hardware and it");
   check(w.rt_written_bytes == w.rt_played_bytes + margin + placed,
         "and the count taken up with the cursor");

   /* From there it plays normally: the block plays, the bytes behind
    * the cursor go silent, and a further write follows on. */
   hw_play(margin + sizeof(block));
   sample(&w, pump);
   check(region_is_silent(&w, at, sizeof(block)),
         "the block is silence once played");
   seen = w.rt_write;
   placed = put(&w, block, sizeof(block));
   check(placed == sizeof(block) && find_in_loop(&w, block, sizeof(block)) == seen,
         "the next write follows on at the cursor: no resync while it keeps up");

   /* A loop that is completely full is not an overrun: the hardware a
    * frame behind the write cursor is exactly what full looks like. */
   memset(tone, 0x55, sizeof(tone));
   placed = put(&w, tone, sizeof(tone));
   check(wdmks_rt_free(&w) == 0, "the loop is full");
   seen = w.rt_write;
   hw_play(FRAME * 8);
   sample(&w, pump);
   check(w.rt_write == seen, "a full loop with the hardware a frame behind is left alone");
   check(!region_is_silent(&w, fake_pos, FRAME * 8),
         "and the audio ahead of the cursor is still there");

   /* The lost-lap watchdog: the register could not be watched round.
    * What played meanwhile was never seen, so the loop is cleared and
    * the cursor resynced. */
   w.rt_last_usec -= (retro_time_t)LOOP_FRAMES * 1000000 / RATE * 2;
   hw_play(LOOP_BYTES / 3);
   sample(&w, pump);
   check(region_is_silent(&w, 0, LOOP_BYTES),
         "laps lost to a stall: the loop is silence");
   check(w.rt_write == (size_t)((fake_pos + margin) % LOOP_BYTES),
         "and the write cursor resynced ahead of the hardware");
   check(wdmks_underruns(&w) == 2, "counted as the second underrun");

   /* A stop and a start: the pin goes through ACQUIRE and the
    * hardware is back at the top of the loop, wherever the model had
    * got to. What the loop held was for the old cursor. */
   fill_tone(tone, sizeof(tone), 0x20);
   put(&w, tone, LOOP_BYTES / 4);
   w.rt_origin = true;        /* as wdmks_start() leaves it */
   fake_pos    = 0;
   sample(&w, pump);
   check(region_is_silent(&w, 0, LOOP_BYTES),
         "after a start the loop is silence");
   check(w.rt_write == margin && wdmks_rt_room(&w) == LOOP_BYTES - FRAME - margin,
         "and the model starts over a margin ahead of the top");
   check(wdmks_underruns(&w) == 2, "without counting the start as an underrun");

   loop_teardown(&w);
}

int main(void)
{
   fifo_frames = 0;
   run_contract("caller-driven loop, no FIFO report", put_direct, false);
   run_contract("refill thread pump, no FIFO report", put_pump, true);
   fifo_frames = 48;      /* 1 ms at 48 kHz, as an HDA codec reports */
   run_contract("caller-driven loop, 1 ms FIFO", put_direct, false);
   run_contract("refill thread pump, 1 ms FIFO", put_pump, true);
   fifo_frames = 480;     /* 10 ms: a FIFO past the fixed guess */
   run_contract("caller-driven loop, 10 ms FIFO", put_direct, false);
   printf("%u failure%s\n", failures, failures == 1 ? "" : "s");
   return failures ? 1 : 0;
}
