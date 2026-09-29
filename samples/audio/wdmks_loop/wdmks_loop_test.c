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
settings_t *config_get_ptr(void)
{
   static settings_t settings;
   return &settings;
}

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
   w->rt_ahead      = LOOP_BYTES;    /* held ahead: the whole loop, as before */
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
   w->rt_ring_size     = w->rt_ahead * 2;
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

/* The ranges a pin reports each bind their own bounds: a pin offering
 * 44100-48000 stereo and 44100-192000 8-channel PCM, and 48000 stereo
 * float, has not offered 192000 stereo PCM, nor float at 44100. */
static void run_ranges(void)
{
   wdmks_pin_t    pin;
   wdmks_format_t f;
   ra_ksmultiple_item_t   *item;
   ra_ksdatarange_audio_t *r;
   unsigned i;

   printf("pin ranges, kept apart\n");
   memset(&pin, 0, sizeof(pin));
   pin.range_count = 3;
   pin.ranges[0].max_channels = 2;  pin.ranges[0].min_bits = 16;
   pin.ranges[0].max_bits = 16;     pin.ranges[0].min_rate = 44100;
   pin.ranges[0].max_rate = 48000;
   pin.ranges[1].max_channels = 8;  pin.ranges[1].min_bits = 16;
   pin.ranges[1].max_bits = 24;     pin.ranges[1].min_rate = 44100;
   pin.ranges[1].max_rate = 192000;
   pin.ranges[2].max_channels = 2;  pin.ranges[2].min_bits = 32;
   pin.ranges[2].max_bits = 32;     pin.ranges[2].min_rate = 48000;
   pin.ranges[2].max_rate = 48000;  pin.ranges[2].is_float = true;

   memset(&f, 0, sizeof(f));
   f.rate = 48000; f.channels = 2; f.bits = 16;
   check(wdmks_format_in_pin(&pin, &f), "48000 stereo 16-bit: the first range");
   f.rate = 192000; f.channels = 8; f.bits = 24;
   check(wdmks_format_in_pin(&pin, &f), "192000 8-channel 24-bit: the second");
   f.rate = 192000; f.channels = 2; f.bits = 16;
   check(wdmks_format_in_pin(&pin, &f), "192000 stereo 16-bit: also the second");
   f.rate = 96000; f.channels = 2; f.bits = 32;
   check(!wdmks_format_in_pin(&pin, &f), "96000 stereo 32-bit integer: no range holds it");
   f.rate = 48000; f.channels = 2; f.bits = 32; f.is_float = true;
   check(wdmks_format_in_pin(&pin, &f), "48000 stereo float: the third");
   f.rate = 44100;
   check(!wdmks_format_in_pin(&pin, &f), "44100 float: not offered, though 44100 and float each are");
   f.rate = 48000; f.channels = 8; f.bits = 32;
   check(!wdmks_format_in_pin(&pin, &f), "8-channel float: not offered");

   /* Past the cap, the rest fold into the last kept range of their
    * own type, never the other's. */
   item = (ra_ksmultiple_item_t*)calloc(1, sizeof(*item)
         + (WDMKS_MAX_RANGES + 2) * sizeof(*r));
   item->Count = WDMKS_MAX_RANGES + 2;
   item->Size  = (ULONG)(sizeof(*item) + item->Count * sizeof(*r));
   r = (ra_ksdatarange_audio_t*)(item + 1);
   for (i = 0; i < item->Count; i++)
   {
      r[i].DataRange.f.FormatSize  = sizeof(*r);
      r[i].DataRange.f.MajorFormat = ra_ks_dataformat_type_audio;
      r[i].DataRange.f.SubFormat   = (i == 1)
         ? ra_ks_dataformat_subtype_float : ra_ks_dataformat_subtype_pcm;
      r[i].MaximumChannels         = 2;
      r[i].MinimumBitsPerSample    = (i == 1) ? 32 : 16;
      r[i].MaximumBitsPerSample    = (i == 1) ? 32 : 16;
      r[i].MinimumSampleFrequency  = 8000 + i * 1000;
      r[i].MaximumSampleFrequency  = 8000 + i * 1000;
   }
   /* The last two are the ones past the cap: one PCM at 176400, and
    * one float at 22050. */
   r[WDMKS_MAX_RANGES].MinimumSampleFrequency     = 176400;
   r[WDMKS_MAX_RANGES].MaximumSampleFrequency     = 176400;
   r[WDMKS_MAX_RANGES + 1].DataRange.f.SubFormat  = ra_ks_dataformat_subtype_float;
   r[WDMKS_MAX_RANGES + 1].MinimumBitsPerSample   = 32;
   r[WDMKS_MAX_RANGES + 1].MaximumBitsPerSample   = 32;
   r[WDMKS_MAX_RANGES + 1].MinimumSampleFrequency = 22050;
   r[WDMKS_MAX_RANGES + 1].MaximumSampleFrequency = 22050;

   memset(&pin, 0, sizeof(pin));
   wdmks_pin_fold_ranges(item, &pin);
   free(item);
   check(pin.range_count == WDMKS_MAX_RANGES, "the cap holds");
   memset(&f, 0, sizeof(f));
   f.channels = 2; f.bits = 16; f.rate = 176400;
   check(wdmks_format_in_pin(&pin, &f), "a PCM range past the cap widened the last PCM range");
   f.rate = 8000; f.bits = 32; f.is_float = true;
   check(!wdmks_format_in_pin(&pin, &f), "and not the float one");
   f.rate = 22050;
   check(wdmks_format_in_pin(&pin, &f), "a float range past the cap widened the float range");
   f.rate = 176400;
   check(!wdmks_format_in_pin(&pin, &f), "and not with the PCM one's rate");
}

/* A 64-bit position register: a byte count past 2^32, on a loop that
 * is not a power of two, lands where the count says and not where its
 * low word alone would. */
static void run_wide_register(void)
{
   wdmks_t  w;
   ULONG    reg[2];
   uint64_t count;

   printf("64-bit position register\n");
   memset(&w, 0, sizeof(w));
   w.rt_size     = LOOP_BYTES;
   w.frame_bytes = FRAME;
   w.rate        = RATE;
   w.rt_pos      = reg;
   w.rt_pos_wide = true;

   count  = ((uint64_t)1 << 32) + 4 * LOOP_BYTES + 1000;
   reg[0] = (ULONG)count;
   reg[1] = (ULONG)(count >> 32);
   check(wdmks_rt_register_read(&w) == (ULONG)(count % LOOP_BYTES),
         "the position is the whole count modulo the loop");
   check(wdmks_rt_register_read(&w) != (ULONG)(reg[0] % LOOP_BYTES),
         "which is not where the low word alone would put it");
   w.rt_pos_wide = false;
   reg[0]        = 1000;
   check(wdmks_rt_register_read(&w) == 1000, "a 32-bit register is read as it is");
}

/* The capture side's fold into the fifo: a stereo pin averaged to mono
 * in either width, a mono pin copied through, and never more than the
 * fifo takes. */
static void run_capture_fold(void)
{
   wdmks_mic_t m;
   int16_t     s16[8] = { 100, 300, -400, -200, 32767, 32767, -32768, 0 };
   float       f32[4] = { 0.5f, 0.25f, -1.0f, 1.0f };
   int16_t     out16[4];
   float       outf[2];
   size_t      took;

   printf("capture fold\n");
   memset(&m, 0, sizeof(m));
   m.fifo        = fifo_new(1024);
   m.frame_bytes = 4;
   m.out_bytes   = 2;
   took = wdmks_mic_fold(&m, (const unsigned char*)s16, sizeof(s16));
   check(took == sizeof(s16), "stereo s16: every frame taken");
   check(FIFO_READ_AVAIL(m.fifo) == 4 * sizeof(int16_t), "four mono samples out");
   fifo_read(m.fifo, out16, sizeof(out16));
   check(out16[0] == 200 && out16[1] == -300 && out16[2] == 32767
         && out16[3] == -16384, "each the average of its pair, without wrapping");

   m.frame_bytes = 8;
   m.out_bytes   = 4;
   m.is_float    = true;
   took = wdmks_mic_fold(&m, (const unsigned char*)f32, sizeof(f32));
   check(took == sizeof(f32), "stereo float: every frame taken");
   fifo_read(m.fifo, outf, sizeof(outf));
   check(outf[0] == 0.375f && outf[1] == 0.0f, "averaged as float");

   m.frame_bytes = 4;
   m.is_float    = true;
   took = wdmks_mic_fold(&m, (const unsigned char*)f32, sizeof(f32));
   check(took == sizeof(f32) && FIFO_READ_AVAIL(m.fifo) == sizeof(f32),
         "mono float: copied through");
   fifo_read(m.fifo, f32, sizeof(f32));

   /* A fifo with room for two samples takes two frames of the three. */
   fifo_free(m.fifo);
   m.fifo        = fifo_new(2 * 4);   /* two floats */
   took = wdmks_mic_fold(&m, (const unsigned char*)f32, 12);
   check(took == 8, "only what the fifo takes, in whole samples");
   fifo_free(m.fifo);
}

/* The WaveRT read model: what the hardware writes is read from behind
 * its position, wrapping with the loop; a lap unseen means what it held
 * is gone and the read cursor moves up to the hardware. */
static void run_capture_loop(void)
{
   wdmks_mic_t   m;
   unsigned char loop[LOOP_BYTES];
   unsigned char out[LOOP_BYTES];
   size_t        i;

   printf("capture loop\n");
   memset(&m, 0, sizeof(m));
   m.stream.looped = true;
   m.stream.handle = INVALID_HANDLE_VALUE;
   m.fifo        = fifo_new(2 * LOOP_BYTES);
   m.frame_bytes = FRAME;
   m.out_bytes   = FRAME;     /* a stereo-wide mono pin, no fold */
   m.rate        = RATE;
   m.rt_buf      = loop;
   m.rt_size     = LOOP_BYTES;
   m.rt_pos      = &fake_pos;
   for (i = 0; i < LOOP_BYTES; i++)
      loop[i] = (unsigned char)(i * 7);

   fake_pos = 0;
   check(wdmks_mic_rt_drain(&m) && FIFO_READ_AVAIL(m.fifo) == 0,
         "the first sample places the cursor and delivers nothing");

   hw_play(1000);
   check(wdmks_mic_rt_drain(&m) && FIFO_READ_AVAIL(m.fifo) == 1000,
         "what the hardware wrote is delivered");
   fifo_read(m.fifo, out, 1000);
   check(!memcmp(out, loop, 1000), "from the start of the loop");

   /* Round the wrap. */
   hw_play(LOOP_BYTES - 1000 + 500);
   check(wdmks_mic_rt_drain(&m) && FIFO_READ_AVAIL(m.fifo) == LOOP_BYTES - 500,
         "a delivery that wraps is whole");
   fifo_read(m.fifo, out, LOOP_BYTES - 500);
   check(!memcmp(out, loop + 1000, LOOP_BYTES - 1000)
         && !memcmp(out + LOOP_BYTES - 1000, loop, 500),
         "and in order across the wrap");

   /* A lap went by unobserved: the read cursor comes up to the
    * hardware and nothing stale is delivered. */
   m.rt_last_usec -= (retro_time_t)LOOP_FRAMES * 1000000 / RATE * 2;
   hw_play(LOOP_BYTES / 3);
   check(wdmks_mic_rt_drain(&m) && FIFO_READ_AVAIL(m.fifo) == 0
         && m.rt_read == fake_pos, "laps lost: the cursor moves up to the hardware");

   /* The hardware overruns: more written than the loop holds. */
   hw_play(LOOP_BYTES / 2);
   wdmks_mic_rt_drain(&m);
   fifo_read(m.fifo, out, FIFO_READ_AVAIL(m.fifo));
   fake_pos = (ULONG)((fake_pos + LOOP_BYTES / 2) % LOOP_BYTES);
   wdmks_mic_rt_sample(&m);   /* observed, but not drained */
   hw_play(LOOP_BYTES / 2 + FRAME);
   check(wdmks_mic_rt_drain(&m) && FIFO_READ_AVAIL(m.fifo) == 0
         && m.rt_read == fake_pos, "an overrun drops what was overwritten");

   fifo_free(m.fifo);
}

/* A pin whose only range is 32-bit PCM stereo, 44100-384000 - a
 * USB Audio Class 2 DAC as its class driver reports it: the integer
 * shapes are in the ranges, float is the last-resort proposal, and the
 * conversion into the pin's width is full scale. */
static void run_wide_pcm_pin(void)
{
   wdmks_pin_t    pin;
   wdmks_format_t f;
   float          in[6]  = { 0.0f, 0.5f, -0.5f, 1.0f, -1.0f, 2.0f };
   int32_t        out[6];
   wdmks_mic_t    m;
   int32_t        s32[2] = { 0x40000000, (int32_t)-0x40000000 };
   float          fo;

   printf("32-bit PCM pin\n");
   memset(&pin, 0, sizeof(pin));
   pin.range_count = 1;
   pin.ranges[0].max_channels = 2;
   pin.ranges[0].min_bits     = 32;
   pin.ranges[0].max_bits     = 32;
   pin.ranges[0].min_rate     = 44100;
   pin.ranges[0].max_rate     = 384000;

   memset(&f, 0, sizeof(f));
   f.rate = 48000; f.channels = 2;
   f.bits = 32; f.container_bits = 32; f.is_float = true;
   check(!wdmks_format_in_pin(&pin, &f), "float is not in the ranges");
   check(wdmks_pin_has_wide(&pin, 48000, 2), "but the pin is 32 bits wide, so it is proposed last");
   check(!wdmks_pin_has_wide(&pin, 32000, 2), "not at a rate outside the range");
   f.is_float = false;
   check(wdmks_format_in_pin(&pin, &f), "32-bit integer is in the ranges");
   f.bits = 24;
   check(!wdmks_format_in_pin(&pin, &f), "24 valid bits is not, on a 32/32 range");
   f.bits = 16; f.container_bits = 16;
   check(!wdmks_format_in_pin(&pin, &f), "nor 16");
   check(wdmks_shape_order[0].is_float && wdmks_shape_order[1].bits == 32
         && !wdmks_shape_order[1].is_float && wdmks_shape_order[2].bits == 24
         && wdmks_shape_order[3].container_bits == 16,
         "shapes: float, 32, 24-in-32, 16");

   convert_float_to_s32(out, in, 6, 32);
   check(out[0] == 0 && out[1] == 0x40000000 && out[2] == (int32_t)-0x40000000,
         "float to 32-bit: full scale");
   check(out[3] == 0x7FFFFFFF && out[4] == (int32_t)-0x7FFFFFFF - 1
         && out[5] == 0x7FFFFFFF, "and clamped at the ends");
   in[1] = 0.5f + 1.0f / 4096.0f;
   convert_float_to_s32(out, in, 6, 24);
   check(out[1] == 0x40080000 && (out[1] & 0xFF) == 0,
         "24 in 32: the low byte is clear");

   memset(&m, 0, sizeof(m));
   m.fifo        = fifo_new(64);
   m.frame_bytes = 8;
   m.out_bytes   = 4;
   m.pin_s32     = true;
   m.is_float    = true;
   check(wdmks_mic_fold(&m, (const unsigned char*)s32, sizeof(s32)) == 8, "capture: a stereo 32-bit frame taken");
   fifo_read(m.fifo, &fo, sizeof(fo));
   check(fo == 0.0f, "averaged as float");
   m.frame_bytes = 4;
   wdmks_mic_fold(&m, (const unsigned char*)s32, 4);
   fifo_read(m.fifo, &fo, sizeof(fo));
   check(fo == 0.5f, "mono 32-bit: full scale float out");
   fifo_free(m.fifo);
}

/* ------------------------------------------------------------------ */
/* The refill thread itself, on the fake register with no event: it   */
/* samples at a quarter of the loop, runs time-critical where the      */
/* setting asks for it, and keeps its own account of how it woke.      */
/* ------------------------------------------------------------------ */

static void run_refill_thread(bool priority, bool mmcss)
{
   wdmks_t      w;
   sthread_t   *t;
   retro_time_t loop_usec = (retro_time_t)LOOP_FRAMES * 1000000 / RATE;
   retro_time_t slice     = loop_usec / 4 < 500 ? 500 : loop_usec / 4;
   /* Eight slices: enough passes for a gap between two of them even
    * on a host that holds the process for a slice or two. */
   retro_time_t run_usec  = slice * 8;
   unsigned     expect    = 8;

   printf("-- refill thread, priority %s, Pro Audio class %s --\n",
         priority ? "on" : "off", mmcss ? "on" : "off");
   loop_setup(&w);
   w.thread_priority = priority;
   w.mmcss           = mmcss;
   retro_atomic_store_release_int(&w.rt_run, 1);
   t = sthread_create(wdmks_rt_refill_thread, &w);
   check(t != NULL, "the refill thread starts");
   retro_sleep_us((unsigned)run_usec);
   retro_atomic_store_release_int(&w.rt_run, 0);
   sthread_join(t);

   check(w.rt_sched && (strcmp(w.rt_sched, "normal priority") != 0) == priority,
         priority ? "it raised itself" : "it left its priority alone");
   if (priority)
      printf("  (ran under %s)\n", w.rt_sched);
   check(w.rt_passes >= expect / 4,
         "it passed at least a quarter as often as a quarter-loop cadence");
   check(w.rt_passes <= expect * 3,
         "and not more often than the slice floor allows");
   check(w.rt_gap_max_usec > 0 && w.rt_gap_max_usec < 1000000,
         "the worst gap between passes is recorded");
   loop_teardown(&w);
}

/* ------------------------------------------------------------------ */
/* Audio held ahead of the hardware is the latency setting; the loop  */
/* it sits in is larger.  A short setting on a large loop: the write  */
/* fills the margin and the setting's worth and no more, a pass late  */
/* by several settings' worth is an overrun on the held audio and not */
/* a lost lap - the loop is still there to follow the cursor round -  */
/* and the margin is the FIFO's, not a quarter of a tiny loop.        */
/* ------------------------------------------------------------------ */

static void run_held_ahead(void)
{
   wdmks_t       w;
   unsigned char tone[LOOP_BYTES];
   size_t        ahead = RATE / 125 * FRAME;    /* 8 ms on a 200 ms loop */
   size_t        margin, placed;

   printf("-- 8 ms held ahead on a 200 ms loop --\n");
   fifo_frames = 48;
   loop_setup(&w);
   w.rt_ahead = ahead;
   margin     = wdmks_rt_margin(&w);
   check(margin == (48 + RATE / 2000) * FRAME,
         "the margin is the FIFO's, not cut to a quarter of the audio held");
   /* The notification fires twice a loop - every two settings' worth
    * here - so it cannot pace the refill and must not be taken. */
   w.rt_event = (HANDLE)(uintptr_t)1;
   wdmks_rt_register_event(&w);
   check(w.rt_event == NULL,
         "the notification event is not taken: it fires too seldom to pace");

   hw_play(RATE / 125 * FRAME);
   fill_tone(tone, sizeof(tone), 0x20);
   placed = put_direct(&w, tone, sizeof(tone));
   check(placed == ahead,
         "a write fills the setting's worth beyond the margin and no more");
   check(wdmks_rt_room(&w) == 0, "and the loop then has no room");
   check(wdmks_buffer_size(&w) == ahead,
         "what the frontend is told it holds is what is held");

   /* Half of it plays: that much room again, and nothing lost */
   hw_play(ahead / 2);
   check(wdmks_rt_free(&w) == ahead / 2, "half played: half the room");
   check(wdmks_underruns(&w) == 0, "no underrun");

   /* The refill is late by three settings' worth: the hardware ran
    * past the held audio, which is an overrun - counted, resynced -
    * but the loop is far from a lap, so the position was followed
    * and the loop was not cleared whole. */
   fill_tone(w.rt_buf, LOOP_BYTES, 0x30);   /* whatever is in the loop */
   hw_play(ahead * 3);
   check(wdmks_rt_free(&w) > 0, "three settings late: room again");
   check(wdmks_underruns(&w) == 1, "counted as one underrun");
   check(w.rt_have_last, "and the position was followed, not lost");
   check(!region_is_silent(&w, (fake_pos + LOOP_BYTES / 2) % LOOP_BYTES, FRAME * 16),
         "the loop was not cleared whole");

   loop_teardown(&w);
   fifo_frames = 0;
}

/* ------------------------------------------------------------------ */
/* The production geometry, run.  For a latency setting, the loop and */
/* the ring are what wdmks_init makes, the position moves a fragment  */
/* per notification (the coarsest a driver reports it), the refill    */
/* runs once per notification as the thread does, and the consumer    */
/* writes chunks of half the buffer it is told, whenever the ring has */
/* room, as the pipeline does.  A pass is made late by a fragment and */
/* a half, once; the loop must heal from it and then never run dry.   */
/*                                                                    */
/* Three things this has caught, each a build that crackled:          */
/*   a loop four times the setting, notified twice a loop - a refill  */
/*   two settings apart;                                              */
/*   a ring of one fragment - supply exactly consumption, a deficit   */
/*   nothing repays, every fourth pass short for good;                */
/*   a margin cut to a quarter of a loop the size of the FIFO.        */
/* ------------------------------------------------------------------ */

static void run_geometry(unsigned latency_ms, unsigned fifo)
{
   wdmks_t       w;
   size_t        ahead, loop, chunk, fragment, margin;
   unsigned char tone[65536];
   unsigned      step, late_at = 200, first_under = 0;
   size_t        under_before = 0, under_late = 0, under_after = 0;
   char          what[96];

   wdmks_rt_geometry(latency_ms, RATE, FRAME, &ahead, &loop);
   snprintf(what, sizeof(what), "-- geometry: %u ms setting, %u-frame FIFO:"
         " loop %u bytes, %u held --", latency_ms, fifo,
         (unsigned)loop, (unsigned)ahead);
   printf("%s\n", what);

   fifo_frames = fifo;
   loop_setup(&w);
   free(w.rt_buf);
   w.rt_buf   = (unsigned char*)calloc(1, loop);
   w.rt_size  = loop;
   w.rt_ahead = ahead;
   retro_spsc_free(&w.rt_ring);
   w.rt_ring_size = wdmks_rt_ring_bytes(ahead);
   retro_spsc_init(&w.rt_ring, w.rt_ring_size);
   fake_pos = 0;

   margin   = wdmks_rt_margin(&w);
   fragment = ahead;
   chunk    = wdmks_buffer_size(&w) / 2;
   check(margin >= (size_t)fifo * FRAME,
         "the resync margin covers the FIFO the pin reported");
   check(wdmks_buffer_size(&w) == ahead,
         "the frontend is told the held amount");
   check(w.rt_ring_size > fragment,
         "the ring holds more than one fragment, so a deficit can be repaid");

   fill_tone(tone, sizeof(tone), 0x50);
   for (step = 0; step < 2000; step++)
   {
      size_t before = wdmks_underruns(&w);
      /* A notification: the hardware has gone half a loop - late,
       * once, by a fragment and a half. */
      fake_pos = (ULONG)((fake_pos + loop / 2) % loop);
      if (step == late_at)
         fake_pos = (ULONG)((fake_pos + fragment * 3 / 2) % loop);
      wdmks_rt_pump_once(&w);
      /* The consumer, woken by the pass: chunks while the ring has room */
      while (wdmks_rt_ring_room(&w) >= chunk)
         retro_spsc_write(&w.rt_ring, tone, chunk);
      if (wdmks_underruns(&w) != before)
      {
         if (step < late_at)        under_before++;
         else if (step <= late_at + 2) under_late++;
         else                       under_after++;
         if (!first_under)
            first_under = step + 1;
      }
   }
   check(under_before == 0, "the loop never runs dry while the passes are on time");
   check(under_late <= 2, "a pass a fragment and a half late costs at most one short pull");
   check(under_after == 0, "and the loop heals: no short pull after it, ever");
   if (under_before || under_after)
      printf("  (first short pull at pass %u; %u before the late pass, %u after)\n",
            first_under, (unsigned)under_before, (unsigned)under_after);

   loop_teardown(&w);
   fifo_frames = 0;
}

int main(void)
{
   run_ranges();
   run_wide_pcm_pin();
   run_wide_register();
   run_capture_fold();
   run_capture_loop();
   run_held_ahead();
   run_geometry(2, 46);
   run_geometry(2, 0);
   run_geometry(8, 46);
   run_geometry(16, 480);
   run_refill_thread(true, false);
   run_refill_thread(true, true);
   run_refill_thread(false, false);
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
