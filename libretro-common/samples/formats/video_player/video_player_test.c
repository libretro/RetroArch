/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (video_player_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* video_player against a scripted stream: the frames in order, each
 * on its container duration, the loop count, catch-up switched on when
 * the decode falls behind and off again, the canvas and slot paths and
 * their byte order, half floats refused, a queued present holding its
 * slot until released, and - with HAVE_THREADS - the worker's two jobs
 * keeping the order while players are freed with decodes in flight.
 * Frames decoded on the GPU go to a sink that takes them, in order, and
 * every one the sink does not take is given back to the decoder. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <time.h>
#include <retro_timers.h>
#include <retro_atomic.h>
#include <formats/image.h>
#include <formats/video_player.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#define W 4
#define H 2
#define N (W * H)

static int failures = 0;

#ifdef VP_TEST_CALLOC_FAULTS
static retro_atomic_int_t alloc_count;
static retro_atomic_int_t alloc_fail;
static retro_atomic_int_t alloc_enabled;
#ifdef HAVE_THREADS
static retro_atomic_int_t fail_worker;

sthread_t *__real_sthread_create(void (*func)(void*), void *user);
sthread_t *__wrap_sthread_create(void (*func)(void*), void *user)
{
   if (retro_atomic_cas_int(&fail_worker, 1, 0))
      return NULL;
   return __real_sthread_create(func, user);
}
#endif

void *__real_calloc(size_t count, size_t size);
void *__wrap_calloc(size_t count, size_t size)
{
   if (retro_atomic_load_acquire_int(&alloc_enabled))
   {
      int ordinal = retro_atomic_fetch_add_int(&alloc_count, 1) + 1;
      if (ordinal == retro_atomic_load_acquire_int(&alloc_fail))
         return NULL;
   }
   return __real_calloc(count, size);
}
#endif

static void CHECK(int cond, const char *fmt, ...)
{
   va_list ap;
   if (cond)
      return;
   va_start(ap, fmt);
   printf("FAIL: ");
   vprintf(fmt, ap);
   printf("\n");
   va_end(ap);
   failures++;
}

/* ---- the stream: frame k is every pixel 0xAA000000 | k, emitted as
 * ARGB when asked or with red and blue exchanged ---- */

typedef struct
{
   uint32_t canvas[N];
   uint32_t *out;
   unsigned out_stride;
   int frames;
   int pos;
   int wait_left;
   int wait_at;
   bool waiting;
   int duration_ms;
   int64_t movie_ns, lead_ns, frame_step_ns, stamp_jitter_ns;
   int64_t hw_failure_ns;
   bool stamp_quantized, failed_stream;
   int catchup;        /* last value set */
   int catchup_sets;
   int argb;
   bool direct;        /* decodes into the output when given one */
   bool honours_order;
   bool fp16;
   /* GPU frames: whether the stream can, whether asked, how many were
    * handed out and given back, and from which frame on one fails */
   bool hw_can;
   bool hw_on;
   int hw_made;
   int hw_abandoned;
   int hw_fail_from;   /* 0 never */
   bool hw_pending;
   retro_atomic_int_t startup_hold, startup_entered;
   int startup_calls;
#ifdef HAVE_THREADS
   uintptr_t startup_poll, startup_thread;
#endif
} fake_t;

int64_t image_transfer_anim_stream_duration_ns(void *stream,
      enum image_type_enum type)
{
   (void)type;
   return ((fake_t*)stream)->movie_ns;
}

int64_t image_transfer_anim_stream_frame_time_ns(void *stream,
      enum image_type_enum type)
{
   fake_t *f = (fake_t*)stream;
   int64_t time_ns;
   (void)type;
   if (f->hw_on && f->hw_failure_ns && f->hw_fail_from
         && f->pos >= f->hw_fail_from)
      return f->hw_failure_ns;
   time_ns = f->pos ? f->lead_ns + (int64_t)(f->pos - 1)
      * (f->frame_step_ns ? f->frame_step_ns
            : (int64_t)f->duration_ms * 1000000)
      + ((f->pos & 1) ? 0 : f->stamp_jitter_ns) : -1;
   return time_ns >= 0 && f->stamp_quantized
      ? time_ns / 1000000 * 1000000 : time_ns;
}

static uint32_t frame_px(int k, bool argb)
{
   uint32_t px = 0xAA000000u | (uint32_t)(k + 1);
   if (!argb)
      px = (px & 0xFF00FF00u) | ((px & 0xFF) << 16) | ((px >> 16) & 0xFF);
   return px;
}

void image_transfer_anim_stream_get_info(void *stream,
      enum image_type_enum type, unsigned *width, unsigned *height,
      int *num_frames, int *loop_count)
{
   fake_t *f = (fake_t*)stream;
   (void)type;
   *width      = W;
   *height     = H;
   *num_frames = f->frames;
   *loop_count = 0;
}

bool image_transfer_anim_stream_completed(const void *stream,
      enum image_type_enum type)
{
   const fake_t *f = (const fake_t*)stream;
   (void)type;
   return f->pos >= f->frames && !f->waiting;
}

const uint32_t *image_transfer_anim_stream_next(void *stream,
      enum image_type_enum type, int *duration_ms)
{
   fake_t *f = (fake_t*)stream;
   uint32_t *dst;
   unsigned i;
   (void)type;
   f->waiting = f->wait_left > 0 && f->pos == f->wait_at;
   if (f->waiting)
   {
      f->wait_left--;
      return NULL;
   }
   if (f->failed_stream || f->pos >= f->frames)
      return NULL;
   if (f->hw_on)
   {
      f->hw_pending = true;
      f->pos++;
      *duration_ms = f->duration_ms;
      return f->canvas;
   }
   dst = (f->direct && f->out) ? f->out : f->canvas;
   for (i = 0; i < N; i++)
      dst[(f->direct && f->out) ? (i / W) * f->out_stride + i % W : i]
         = frame_px(f->pos, f->honours_order ? f->argb : 0);
   f->pos++;
   *duration_ms = f->duration_ms;
   return dst;
}

static void fake_abandon(void *owner, uint64_t done, uint64_t value)
{
   fake_t *f = (fake_t*)owner;
   (void)done;
   (void)value;
   f->hw_abandoned++;
}

bool image_transfer_anim_stream_waiting(const void *stream,
      enum image_type_enum type)
{
   (void)type;
   return ((const fake_t*)stream)->waiting;
}

bool image_transfer_anim_stream_set_hw(void *stream,
      enum image_type_enum type, bool enable)
{
   fake_t *f = (fake_t*)stream;
   (void)type;
   if(!enable)return f->hw_on;
   f->startup_calls++;
#ifdef HAVE_THREADS
   f->startup_thread = sthread_get_current_thread_id();
   if (f->startup_poll && f->startup_thread == f->startup_poll)
   {
      CHECK(0,"startup: native constructor ran on polling thread");
      return false;
   }
#endif
   retro_atomic_store_release_int(&f->startup_entered,1);
   while (retro_atomic_load_acquire_int(&f->startup_hold)) retro_sleep(1);
   if (enable && f->hw_can)
      f->hw_on = true;
   return f->hw_on;
}

bool image_transfer_anim_stream_hw_frame(void *stream,
      enum image_type_enum type, video_hw_frame_t *out)
{
   fake_t *f = (fake_t*)stream;
   (void)type;
   if (!f->hw_pending || (f->hw_fail_from && f->pos >= f->hw_fail_from))
      return false;
   f->hw_pending = false;
   memset(out, 0, sizeof(*out));
   out->image      = (uint64_t)f->pos;   /* frame k is image k + 1 */
   out->done_value = (uint64_t)++f->hw_made;
   out->abandon    = fake_abandon;
   out->owner      = f;
   out->width      = W;
   out->height     = H;
   return true;
}

void image_transfer_anim_stream_rewind(void *stream,
      enum image_type_enum type)
{
   (void)type;
   ((fake_t*)stream)->pos = 0;
}

bool image_transfer_anim_stream_set_argb(void *stream,
      enum image_type_enum type, int argb)
{
   fake_t *f = (fake_t*)stream;
   (void)type;
   f->argb = argb;
   return f->honours_order;
}

bool image_transfer_anim_stream_set_output_stride(void *stream,
      enum image_type_enum type, uint32_t *out, unsigned stride)
{
   fake_t *f = (fake_t*)stream;
   (void)type;
   f->out = out;
   f->out_stride = stride ? stride : W;
   return f->direct;
}

void image_transfer_anim_stream_set_catchup(void *stream,
      enum image_type_enum type, int behind)
{
   fake_t *f = (fake_t*)stream;
   (void)type;
   f->catchup = behind;
   f->catchup_sets++;
}

void image_transfer_anim_stream_set_want_fp16(void *stream,
      enum image_type_enum type, bool want)
{
   (void)stream;
   (void)type;
   (void)want;
}

bool image_transfer_anim_stream_is_fp16(const void *stream,
      enum image_type_enum type)
{
   (void)type;
   return ((const fake_t*)stream)->fp16;
}

bool image_transfer_anim_stream_set_blit_pool(void *stream,
      enum image_type_enum type, void *pool, unsigned bands)
{
   (void)stream;
   (void)type;
   (void)pool;
   (void)bands;
   return true;
}

/* ---- the sink ---- */

#define MAX_SEEN 256

typedef struct
{
   uint32_t slots[2][(W + 3) * H];
   unsigned stride[2];
   int64_t times[4];
   int seen[MAX_SEEN];
   int nseen;
   int bad_px;
   int begun[2];         /* slot_begin minus slot_end, per slot */
   int held;             /* slot of a QUEUED present, -1 none */
   int queue_every;      /* present QUEUED every n-th frame, 0 never */
   int start_flags;
   bool slots_on;
   /* GPU frames: taken (DONE or QUEUED), BUSY every n-th offer */
   int hw_taken;
   int hw_busy_every;
   int hw_offers;
   int hw_started, started, ended;
   int hw_start_waits;
   fake_t *fallback_stream;
   int fallback_calls, fallback_waits;
   retro_atomic_int_t fallback_entered;
   bool fallback_error;
   bool hw_start_refused, fallback_threaded, fallback_async;
   video_hw_frame_t fallback_held;
#ifdef HAVE_THREADS
   uintptr_t fallback_caller;
#endif
   bool hw_twice;        /* the same frame offered after it was taken */
} sink_t;

static int sink_start(void *user, unsigned width, unsigned height,
      unsigned num_slots)
{
   sink_t *k = (sink_t*)user;
   k->started++;
   CHECK(width == W && height == H, "start: %ux%u", width, height);
   CHECK(num_slots == (k->slots_on ? num_slots : 0u),
         "start: %u slots for a sink without", num_slots);
   return k->start_flags;
}

static uint32_t *sink_slot_begin(void *user, unsigned slot)
{
   sink_t *k = (sink_t*)user;
   if (k->held == (int)slot)
      return NULL;
   if (!k->begun[slot])
      k->begun[slot] = 1;
   return k->slots[slot];
}

static void sink_slot_end(void *user, unsigned slot)
{
   ((sink_t*)user)->ended++;
   ((sink_t*)user)->begun[slot] = 0;
}

static enum video_player_present sink_present(void *user,
      const video_player_frame_t *fr)
{
   sink_t *k            = (sink_t*)user;
   const uint32_t *px   = fr->pixels ? fr->pixels : k->slots[fr->slot];
   uint32_t want_argb;
   int idx;
   unsigned i;

   CHECK(fr->width == W && fr->height == H, "present: dims");
   CHECK(!k->begun[fr->slot], "present: slot still being written");
   idx = (int)(px[0] & 0xFF) - 1;
   if (!fr->native)
      idx = (int)((px[0] >> 16) & 0xFF) - 1;
   want_argb = frame_px(idx, true);
   for (i = 0; i < N; i++)
   {
      unsigned stride = !fr->pixels && k->stride[fr->slot]
         ? k->stride[fr->slot] : W;
      uint32_t p = px[(i / W) * stride + i % W];
      if (!fr->native)
         p = (p & 0xFF00FF00u) | ((p & 0xFF) << 16) | ((p >> 16) & 0xFF);
      if (p != want_argb)
         k->bad_px++;
   }
   if (k->nseen < 4)
      k->times[k->nseen] = fr->time_us;
   if (k->nseen < MAX_SEEN)
      k->seen[k->nseen] = idx;
   k->nseen++;
   if (k->queue_every && !fr->pixels && (k->nseen % k->queue_every) == 0)
   {
      k->held = (int)fr->slot;
      return VIDEO_PLAYER_PRESENT_QUEUED;
   }
   return VIDEO_PLAYER_PRESENT_DONE;
}

static int sink_start_hw(void *user, unsigned width, unsigned height,
      unsigned jobs)
{
   sink_t *k = (sink_t*)user;
   CHECK(jobs == 1 || jobs == 2, "start_hw: %u jobs", jobs);
   CHECK(width == W && height == H, "start_hw: %ux%u", width, height);
   k->hw_started++;
   if (k->hw_start_waits)
   {
      k->hw_start_waits--;
      return -1;
   }
   return k->hw_start_refused ? VIDEO_PLAYER_HW_REFUSED : 0;
}

static enum video_player_present sink_present_hw(void *user,
      const video_player_frame_t *fr)
{
   sink_t *k = (sink_t*)user;
   int idx;
   if (!fr->hw)
   {
      CHECK(0, "present_hw: a frame without its GPU frame");
      return VIDEO_PLAYER_PRESENT_FAILED;
   }
   CHECK(fr->width == W && fr->height == H && fr->hw->width == W,
         "present_hw: dims");
   k->hw_offers++;
   if (k->hw_busy_every && (k->hw_offers % k->hw_busy_every) == 0)
      return VIDEO_PLAYER_PRESENT_BUSY;
   idx = (int)fr->hw->image - 1;
   if (k->nseen && k->nseen <= MAX_SEEN && k->seen[k->nseen - 1] == idx
         && k->held < 0)
      k->hw_twice = true;
   if (k->nseen < 4)
      k->times[k->nseen] = fr->time_us;
   if (k->nseen < MAX_SEEN)
      k->seen[k->nseen] = idx;
   k->nseen++;
   k->hw_taken++;
   if (k->queue_every && (k->nseen % k->queue_every) == 0)
   {
      k->held = (int)fr->slot;
      return VIDEO_PLAYER_PRESENT_QUEUED;
   }
   return VIDEO_PLAYER_PRESENT_DONE;
}

static void sink_init(sink_t *k, video_player_sink_t *s, bool slots)
{
   memset(k, 0, sizeof(*k));
   k->held       = -1;
   retro_atomic_int_init(&k->fallback_entered,0);
   k->slots_on   = slots;
   s->user       = k;
   s->start      = sink_start;
   s->slot_begin = slots ? sink_slot_begin : NULL;
   s->slot_end   = slots ? sink_slot_end : NULL;
   s->present    = sink_present;
   s->feed       = NULL;
   s->decoded    = NULL;
   s->start_hw   = NULL;
   s->slot_stride = NULL;
}

/* A sink that takes GPU frames, and slots for any other. */
static void sink_init_hw(sink_t *k, video_player_sink_t *s)
{
   sink_init(k, s, true);
   s->start_hw = sink_start_hw;
   s->present  = sink_present_hw;
}

static void fake_init(fake_t *f, int frames, int duration_ms)
{
   memset(f, 0, sizeof(*f));
   retro_atomic_int_init(&f->startup_hold,0);
   retro_atomic_int_init(&f->startup_entered,0);
   f->frames        = frames;
   f->duration_ms   = duration_ms;
   f->honours_order = true;
}

/* Pacing lanes keep one clock origin while cold admission is scheduled. */
static void settle_hw_start(video_player_t *p, int64_t now)
{
   unsigned i;
   enum video_player_state state = VIDEO_PLAYER_PLAYING;
   for (i = 0;
        i < 1000 && !video_player_started(p) && state == VIDEO_PLAYER_PLAYING;
        i++)
   {
      state = video_player_poll(p, now);
      if (!video_player_started(p))
         retro_sleep(1);
   }
   CHECK(video_player_started(p), "startup: sink did not settle");
}

static bool seen_in_order(const sink_t *k, int frames)
{
   int i;
   for (i = 0; i < k->nseen && i < MAX_SEEN; i++)
      if (k->seen[i] != i % frames)
         return false;
   return true;
}

/* Paced on time: every frame once, in order, each due its duration
 * after the last; the loop count ends it; catch-up never asked. */
static void lane_inline_pacing(bool slots)
{
   fake_t f;
   sink_t k;
   video_player_sink_t s;
   video_player_t *p;
   int64_t now = 1000000;
   int polls   = 0;
   enum video_player_state st;

   fake_init(&f, 5, 40);
   sink_init(&k, &s, slots);
   f.direct = slots;
   p = video_player_new(&f, IMAGE_TYPE_WEBM, 2, &s, VIDEO_PLAYER_INLINE);
   CHECK(p != NULL, "inline: new");
   do
   {
      int64_t due;
      st  = video_player_poll(p, now);
      due = video_player_due(p);
      if (st == VIDEO_PLAYER_PLAYING && k.nseen > 0)
         CHECK(due > now, "inline: due %lld not after now %lld",
               (long long)due, (long long)now);
      now = due > now ? due : now + 1000;
   } while (st == VIDEO_PLAYER_PLAYING && ++polls < 100);
   CHECK(k.nseen == 10, "inline%s: %d frames for two loops of 5",
         slots ? " slots" : "", k.nseen);
   CHECK(seen_in_order(&k, 5), "inline: out of order");
   CHECK(k.bad_px == 0, "inline: %d pixels wrong", k.bad_px);
   CHECK(f.catchup_sets == 0, "inline: catch-up asked on time");
   video_player_free(p);
}

/* Polled late: the frames still come in order, catch-up goes on, and
 * goes off once a frame lands on time again. */
static void lane_catchup(void)
{
   fake_t f;
   sink_t k;
   video_player_sink_t s;
   video_player_t *p;
   int64_t now = 5000000;
   int i;

   fake_init(&f, 4, 10);
   sink_init(&k, &s, false);
   p = video_player_new(&f, IMAGE_TYPE_WEBM, 0, &s, VIDEO_PLAYER_INLINE);
   video_player_poll(p, now);           /* first frame: sets the clock */
   now += 100000;                       /* ten durations late */
   video_player_poll(p, now);
   CHECK(f.catchup == 1, "catch-up: not asked when behind");
   for (i = 0; i < 6; i++)
   {
      now = video_player_due(p);        /* exactly on time */
      video_player_poll(p, now);
   }
   CHECK(f.catchup == 0 && f.catchup_sets == 2,
         "catch-up: still on (%d) after landing on time, %d sets",
         f.catchup, f.catchup_sets);
   CHECK(seen_in_order(&k, 4) && k.nseen == 8, "catch-up: %d frames",
         k.nseen);
   video_player_free(p);
}

/* A zero duration is 100 ms; the canvas of a stream that cannot emit
 * the asked order comes as R,G,B,A and is swizzled into a slot. */
static void lane_order_and_default(void)
{
   fake_t f;
   sink_t k;
   video_player_sink_t s;
   video_player_t *p;

   fake_init(&f, 3, 0);
   f.honours_order = false;
   sink_init(&k, &s, true);
   p = video_player_new(&f, IMAGE_TYPE_PNG, 1, &s, VIDEO_PLAYER_INLINE);
   video_player_poll(p, 0);
   CHECK(video_player_due(p) == 100000, "default duration: due %lld",
         (long long)video_player_due(p));
   video_player_poll(p, 100000);
   CHECK(k.nseen == 2 && k.bad_px == 0,
         "swizzle: %d frames, %d pixels wrong", k.nseen, k.bad_px);
   CHECK(k.times[0] == -1 && k.times[1] == -1,
         "animation: no fabricated movie timestamp");
   video_player_free(p);

   /* the same without slots: handed over as is, marked non-native */
   fake_init(&f, 3, 10);
   f.honours_order = false;
   sink_init(&k, &s, false);
   p = video_player_new(&f, IMAGE_TYPE_PNG, 1, &s, VIDEO_PLAYER_INLINE);
   video_player_poll(p, 0);
   CHECK(k.nseen == 1 && k.bad_px == 0, "canvas: order not reported");
   video_player_free(p);
}

/* Half floats come only from a decode into the slot itself. */
static void lane_fp16(void)
{
   fake_t f;
   sink_t k;
   video_player_sink_t s;
   video_player_t *p;

   fake_init(&f, 3, 10);
   sink_init(&k, &s, true);
   k.start_flags = VIDEO_PLAYER_FP16;
   p = video_player_new(&f, IMAGE_TYPE_MP4, 0, &s, VIDEO_PLAYER_INLINE);
   CHECK(video_player_poll(p, 0) == VIDEO_PLAYER_ENDED && k.nseen == 0,
         "fp16: a canvas frame was shown");
   CHECK(!k.begun[0], "fp16: refused frame holds its slot");
   video_player_free(p);

   fake_init(&f, 3, 10);
   f.direct = true;
   f.fp16   = true;
   sink_init(&k, &s, true);
   k.start_flags = VIDEO_PLAYER_FP16;
   p = video_player_new(&f, IMAGE_TYPE_MP4, 0, &s, VIDEO_PLAYER_INLINE);
   CHECK(video_player_poll(p, 0) == VIDEO_PLAYER_PLAYING && k.nseen == 1,
         "fp16: a direct frame was refused");
   video_player_free(p);
}

/* GPU frames in line: in order on time, a frame not taken (BUSY) given
 * back, the loop count kept; a sink without start_hw decodes into its
 * slots; a frame that comes without its GPU frame ends the playback. */
static void lane_hw_inline(void)
{
   fake_t f;
   sink_t k;
   video_player_sink_t s;
   video_player_t *p;
   int64_t now = 0;
   int polls   = 0;
   enum video_player_state st;

   fake_init(&f, 6, 10);
   f.hw_can = true;
   f.wait_left = 3;
   f.wait_at = 2;
   sink_init_hw(&k, &s);
   k.hw_busy_every = 4;
   p = video_player_new(&f, IMAGE_TYPE_MP4, 2, &s, VIDEO_PLAYER_INLINE);
   settle_hw_start(p,now);
   do
   {
      st  = video_player_poll(p, now);
      now = video_player_due(p) > now ? video_player_due(p) : now + 1000;
   } while (st == VIDEO_PLAYER_PLAYING && ++polls < 200);
   video_player_free(p);
   CHECK(k.hw_started == 1, "hw inline: started %d times", k.hw_started);
   CHECK(f.hw_made == k.hw_taken + f.hw_abandoned,
         "hw inline: %d made, %d taken, %d given back", f.hw_made,
         k.hw_taken, f.hw_abandoned);
   CHECK(f.hw_abandoned > 0, "hw inline: a BUSY frame was kept");
   CHECK(f.hw_made == 12, "hw inline: %d made for two loops of 6",
         f.hw_made);

   /* the same stream to a sink that takes no GPU frames */
   fake_init(&f, 3, 10);
   f.hw_can = true;
   f.direct = true;
   sink_init(&k, &s, true);
   p = video_player_new(&f, IMAGE_TYPE_MP4, 1, &s, VIDEO_PLAYER_INLINE);
   video_player_poll(p, 0);
   CHECK(!f.hw_on && k.nseen == 1 && k.bad_px == 0,
         "hw inline: a sink without start_hw got no software frame");
   video_player_free(p);

   /* the GPU frame fails at the third: the playback ends there */
   fake_init(&f, 6, 10);
   f.hw_can       = true;
   f.hw_fail_from = 3;
   sink_init_hw(&k, &s);
   p     = video_player_new(&f, IMAGE_TYPE_MP4, 0, &s, VIDEO_PLAYER_INLINE);
   settle_hw_start(p,0);
   now   = 0;
   polls = 0;
   do
   {
      st  = video_player_poll(p, now);
      now = video_player_due(p) > now ? video_player_due(p) : now + 1000;
   } while (st == VIDEO_PLAYER_PLAYING && ++polls < 50);
   CHECK(st == VIDEO_PLAYER_ENDED && k.nseen == 2,
         "hw inline: %d frames before the failing one", k.nseen);
   CHECK(!video_player_completed(p), "hw failure is not successful EOF");
   video_player_free(p);
}

#ifdef HAVE_THREADS
/* GPU frames on the worker: in order across loops, a QUEUED frame
 * holding its job until released, a BUSY one offered again, and every
 * frame made either taken or given back - also when players are freed
 * with frames decoded and not shown. */
static void lane_hw_threaded(void)
{
   int i;
   fake_t f;
   sink_t k;
   video_player_sink_t s;
   video_player_t *p;
   int64_t now = 0;
   int polls   = 0;
   enum video_player_state st;

   fake_init(&f, 7, 5);
   f.hw_can = true;
   f.wait_left = 3;
   f.wait_at = 2;
   sink_init_hw(&k, &s);
   k.queue_every   = 3;
   k.hw_busy_every = 5;
   p = video_player_new(&f, IMAGE_TYPE_MP4, 3, &s, 0);
   do
   {
      st = video_player_poll(p, now);
      if (k.held >= 0)
      {
         video_player_slot_released(p, (unsigned)k.held);
         k.held = -1;
      }
      now += 6000;
      retro_sleep(1);
   } while (st == VIDEO_PLAYER_PLAYING && ++polls < 5000);
   video_player_free(p);
   CHECK(st == VIDEO_PLAYER_ENDED, "hw threaded: never ended");
   CHECK(k.nseen == 21 && seen_in_order(&k, 7),
         "hw threaded: %d frames for three loops of 7, or out of order",
         k.nseen);
   CHECK(f.hw_made == k.hw_taken + f.hw_abandoned && !f.hw_abandoned,
         "hw threaded: %d made, %d taken, %d given back", f.hw_made,
         k.hw_taken, f.hw_abandoned);

   for (i = 0; i < 300; i++)
   {
      int j;
      fake_init(&f, 40, 1);
      f.hw_can = true;
      sink_init_hw(&k, &s);
      p = video_player_new(&f, IMAGE_TYPE_MP4, 0, &s, 0);
      for (j = 0; j < (i % 5); j++)
      {
         video_player_poll(p, (int64_t)j * 3000);
         if (i & 1)
            retro_sleep(1);
      }
      video_player_free(p);
      if (f.hw_made != k.hw_taken + f.hw_abandoned)
      {
         CHECK(0, "hw churn: %d made, %d taken, %d given back", f.hw_made,
               k.hw_taken, f.hw_abandoned);
         break;
      }
   }
}

/* On the worker: the frames in order across loops, a QUEUED present
 * holding its slot until released, every slot ended at the free. */
static void lane_threaded(bool direct)
{
   fake_t f;
   sink_t k;
   video_player_sink_t s;
   video_player_t *p;
   int64_t now = 0;
   int polls   = 0;
   enum video_player_state st;

   fake_init(&f, 7, 5);
   f.direct = direct;
   sink_init(&k, &s, true);
   k.queue_every = 3;
   p = video_player_new(&f, IMAGE_TYPE_WEBM, 3, &s, 0);
   do
   {
      st = video_player_poll(p, now);
      if (k.held >= 0)
      {
         /* the sink lets go on a later tick, as the video thread does */
         video_player_slot_released(p, (unsigned)k.held);
         k.held = -1;
      }
      now += 6000;
      retro_sleep(1);
   } while (st == VIDEO_PLAYER_PLAYING && ++polls < 5000);
   CHECK(st == VIDEO_PLAYER_ENDED, "threaded: never ended");
   CHECK(k.nseen == 21, "threaded%s: %d frames for three loops of 7",
         direct ? " direct" : "", k.nseen);
   CHECK(seen_in_order(&k, 7), "threaded: out of order");
   CHECK(k.bad_px == 0, "threaded: %d pixels wrong", k.bad_px);
   video_player_free(p);
   CHECK(!k.begun[0] && !k.begun[1], "threaded: a slot not ended");
}

/* Players freed with their decodes queued or running, many times over,
 * and a player started after them still plays. */
static void lane_churn(void)
{
   int i;
   for (i = 0; i < 400; i++)
   {
      fake_t f;
      sink_t k;
      video_player_sink_t s;
      video_player_t *p;
      int j;

      fake_init(&f, 50, 1);
      sink_init(&k, &s, true);
      video_player_set_threads((unsigned)(1 + i % 3));
      p = video_player_new(&f, IMAGE_TYPE_WEBM, 0, &s, 0);
      for (j = 0; j < (i % 4); j++)
         video_player_poll(p, (int64_t)j * 2000);
      video_player_free(p);
      CHECK(!k.begun[0] && !k.begun[1], "churn: a slot not ended");
   }
   video_player_set_threads(1);
   lane_threaded(true);
   video_player_worker_deinit();
   /* torn down: the next player makes the worker again */
   lane_threaded(false);
   video_player_worker_deinit();
}
#endif

static unsigned sink_slot_stride(void *user, unsigned slot)
{
   return ((sink_t*)user)->stride[slot];
}

static bool sink_feed_stop(void *user)
{
   (void)user;
   return false;
}

static void lane_inline_release(void)
{
   unsigned mode;
   for (mode = 0; mode < 3; mode++)
   {
      fake_t f;
      sink_t k;
      video_player_sink_t s;
      video_player_t *p;
      fake_init(&f, 1, 10);
      f.direct = true;
      sink_init(&k, &s, true);
      if (mode == 0)
         s.feed = sink_feed_stop;
      else if (mode == 1)
         f.pos = f.frames;
      else
         s.slot_stride = sink_slot_stride;
      p = video_player_new(&f, IMAGE_TYPE_MP4, 1, &s,
            VIDEO_PLAYER_INLINE);
      CHECK(p != NULL, "inline release: allocation");
      if (!p)
         continue;
      CHECK(video_player_poll(p, 0) == VIDEO_PLAYER_ENDED,
            "inline release: mode %u did not end", mode);
      CHECK(!k.begun[0] && !k.nseen,
            "inline release: mode %u holds or presents its slot", mode);
      video_player_free(p);
   }
}

static void lane_padded(void)
{
   unsigned threaded, direct, order;
   for (threaded = 0; threaded < 2; threaded++)
   for (direct = 0; direct < 2; direct++)
   for (order = 0; order < 2; order++)
   {
      fake_t f;
      sink_t k;
      video_player_sink_t s;
      video_player_t *p;
      unsigned tick, slot, y, x;
      int state = VIDEO_PLAYER_PLAYING;
      fake_init(&f, 6, 10);
      f.direct = direct != 0;
      f.honours_order = order != 0;
      sink_init(&k, &s, true);
      k.stride[0] = W + 1;
      k.stride[1] = W + 3;
      s.slot_stride = sink_slot_stride;
      for (slot = 0; slot < 2; slot++)
         for (x = 0; x < (W + 3) * H; x++)
            k.slots[slot][x] = 0xdeadbeef;
      p = video_player_new(&f, IMAGE_TYPE_MP4, 1, &s,
            threaded ? 0 : VIDEO_PLAYER_INLINE);
      CHECK(p != NULL, "padded: allocation");
      if (!p) continue;
      for (tick = 0; tick < 1000 && state != VIDEO_PLAYER_ENDED; tick++)
      {
         state = video_player_poll(p, (int64_t)tick * 1000);
         retro_sleep(1);
      }
      video_player_free(p);
      CHECK(state == VIDEO_PLAYER_ENDED && k.nseen == 6 && !k.bad_px,
            "padded: mode %u/%u/%u showed %d frames, %d wrong pixels",
            threaded, direct, order, k.nseen, k.bad_px);
      CHECK(!k.begun[0] && !k.begun[1], "padded: unreleased slot");
      for (slot = 0; slot < 2; slot++)
         for (y = 0; y < H; y++)
            for (x = W; x < k.stride[slot]; x++)
               CHECK(k.slots[slot][y * k.stride[slot] + x] == 0xdeadbeef,
                     "padded: slot %u row %u guard", slot, y);
   }
}

/* A long pause must retain the remainder of the displayed picture,
 * without catch-up, skipped ordinals or presenting a ready worker job. */
static void lane_pause(bool threaded)
{
   fake_t f;
   sink_t k;
   video_player_sink_t s;
   video_player_t *p;
   int i;
   int64_t now = 1000000;
   fake_init(&f, 5, 40);
   sink_init(&k, &s, true);
   f.direct = true;
   p = video_player_new(&f, IMAGE_TYPE_WEBM, 1, &s,
         threaded ? 0 : VIDEO_PLAYER_INLINE);
   CHECK(p != NULL, "pause: allocation");
   if (!p) return;
   video_player_set_paused(p, true, now);
   video_player_set_paused(p, true, now + 10000);
   CHECK(video_player_paused(p), "pause: before first poll");
   CHECK(!video_player_has_presented(p), "pause: no accepted picture yet");
   for (i = 0; i < 10; i++)
      CHECK(video_player_poll(p, now + i * 1000) == VIDEO_PLAYER_PLAYING,
            "pause: stays live");
   CHECK(k.nseen == 0, "pause: no initial presentation");
   CHECK(video_player_due(p) == INT64_MAX, "pause: no deadline");
   video_player_set_paused(p, false, now);
   for (i = 0; i < 1000 && k.nseen == 0; i++)
   {
      video_player_poll(p, now);
      if (threaded) retro_sleep(1);
   }
   CHECK(k.nseen == 1, "pause: first picture");
   CHECK(video_player_has_presented(p), "pause: first picture accepted");
   CHECK(video_player_due(p) == now + 40000, "pause: initial duration");
   video_player_set_paused(p, true, now + 10000);
   for (i = 0; i < 10; i++)
   {
      video_player_poll(p, now + 10000000 + i * 1000);
      if (threaded) retro_sleep(1);
   }
   CHECK(k.nseen == 1, "pause: pending picture not presented");
   video_player_set_paused(p, false, now + 10000000);
   CHECK(video_player_due(p) == now + 10030000,
         "pause: remaining 30ms preserved");
   video_player_set_paused(p, false, now + 10010000);
   CHECK(video_player_due(p) == now + 10030000,
         "pause: repeated resume does not shift clock");
   video_player_poll(p, now + 10029999);
   CHECK(k.nseen == 1, "pause: not before deadline");
   for (i = 0; i < 1000 && k.nseen == 1; i++)
   {
      video_player_poll(p, now + 10030000);
      if (threaded) retro_sleep(1);
   }
   CHECK(k.nseen == 2 && seen_in_order(&k, 5), "pause: resumes next ordinal");
   CHECK(f.catchup_sets == 0, "pause: elapsed wall time is not catch-up");
   video_player_set_paused(p, true, now + 10040000);
   while (!video_player_stop(p)) retro_sleep(1);
   video_player_set_paused(p, false, now + 20000000);
   CHECK(video_player_poll(p, now + 20000000) == VIDEO_PLAYER_ENDED,
         "pause: stop cannot resume");
   CHECK(!video_player_completed(p), "stop is not successful EOF");
   video_player_free(p);
}

static void lane_input_wait(bool threaded, bool slots)
{
   fake_t f;
   sink_t k;
   video_player_sink_t s;
   video_player_t *p;
   int tick, state = VIDEO_PLAYER_PLAYING;
   bool finite = false;
   fake_init(&f, 5, 10);
   f.direct = true;
   f.wait_at = 2;
   f.wait_left = 3;
   sink_init(&k, &s, slots);
   p = video_player_new(&f, IMAGE_TYPE_MP4, 0, &s,
         threaded ? 0 : VIDEO_PLAYER_INLINE);
   CHECK(p != NULL, "input wait: allocation");
   if (!p) return;
   for (tick = 0; tick < 1000 && state != VIDEO_PLAYER_ENDED; tick++)
   {
      if (k.nseen >= 2 && !finite)
      {
         CHECK(video_player_set_repeat(p, false), "set one-pass movie policy");
         finite = true;
      }
      state = video_player_poll(p, (int64_t)tick * 1000);
      if (threaded) retro_sleep(1);
   }
   CHECK(video_player_completed(p), "input wait: clean movie EOF");
   video_player_free(p);
   CHECK(state == VIDEO_PLAYER_ENDED && k.nseen == 5
         && seen_in_order(&k, 5) && !k.bad_px,
         "input wait: mode %d/%d ended after %d frames", threaded, slots, k.nseen);
   CHECK(!k.begun[0] && !k.begun[1], "input wait: slots released");
}

typedef struct
{
   fake_t *stream;
   retro_atomic_int_t gate, steps;
   int restarts;
   int fail;
   bool progress;
   int64_t target;
} seek_test_t;

static int seek_step(void *user, int64_t target, bool restart,
      int64_t *position)
{
   seek_test_t *seek = (seek_test_t*)user;
   if (restart)
   {
      seek->restarts++;
      seek->target = target;
   }
   retro_atomic_fetch_add_int(&seek->steps, 1);
   if (!retro_atomic_load_acquire_int(&seek->gate))
      return seek->progress ? VIDEO_PLAYER_SEEK_PROGRESS : 0;
   if (seek->fail)
      return -1;
   seek->stream->pos = (int)(target / 10000);
   *position = target;
   return 1;
}

static void lane_seek(bool threaded, bool hw)
{
   fake_t f;
   sink_t k;
   video_player_sink_t s;
   video_player_t *p;
   seek_test_t seek;
   int i, state = VIDEO_PLAYER_PLAYING;
   int64_t position = -1, now = 1000000;
   fake_init(&f, 12, 10);
   sink_init(&k, &s, true);
   memset(&seek, 0, sizeof(seek));
   seek.stream = &f;
   f.direct = true;
   f.wait_at = 5;
   f.wait_left = 3;
   f.hw_can = hw;
   if (hw) sink_init_hw(&k, &s);
   k.queue_every = threaded ? 1 : 0;
   p = video_player_new(&f, IMAGE_TYPE_WEBM, 1, &s,
         threaded ? 0 : VIDEO_PLAYER_INLINE);
   CHECK(p != NULL, "seek: allocation");
   if (!p) return;
   CHECK(!video_player_seek(p, 0), "seek: handler required");
   CHECK(video_player_set_seek_handler(p, seek_step, &seek),
         "seek: handler installation");
   if(hw)settle_hw_start(p,now);
   for (i = 0; i < 1000 && !k.nseen; i++)
   {
      video_player_poll(p, now);
      if (threaded) retro_sleep(1);
   }
   CHECK(k.nseen == 1, "seek: initial picture");
   CHECK(!video_player_set_seek_handler(p, NULL, NULL),
         "seek: running handler immutable");
   video_player_set_paused(p, true, now);
   CHECK(video_player_seek(p, 20000), "seek: first request");
   if (threaded)
   {
      for (i = 0; i < 10; i++)
      {
         video_player_poll(p, now);
         retro_sleep(1);
      }
      CHECK(!retro_atomic_load_acquire_int(&seek.steps),
            "seek: accepted old slot remains sink-owned");
      CHECK(k.held >= 0, "seek: initial slot held");
      video_player_slot_released(p, (unsigned)k.held);
      k.held = -1;
      k.queue_every = 0;
   }
   for (i = 0; i < 1000
         && !retro_atomic_load_acquire_int(&seek.steps); i++)
   {
      video_player_poll(p, now);
      if (threaded) retro_sleep(1);
   }
   CHECK(retro_atomic_load_acquire_int(&seek.steps) > 0,
         "seek: bounded step ran");
   CHECK(k.nseen == 1 && !video_player_seek_position(p, &position),
         "seek: old picture retained, result pending");
   CHECK(video_player_seek(p, 30000) && video_player_seek(p, 50000),
         "seek: newest target accepted");
   retro_atomic_store_release_int(&seek.gate, 1);
   for (i = 0; i < 1000 && (video_player_seeking(p) || k.nseen < 2); i++)
   {
      video_player_poll(p, now);
      if (threaded) retro_sleep(1);
   }
   CHECK(k.nseen == 2 && k.seen[1] == 5 && !k.bad_px,
         "seek: newest target picture, no stale frame (%d/%d)",
         k.nseen, k.seen[1]);
   CHECK(video_player_seek_position(p, &position) && position == 50000,
         "seek: latest completion position");
   CHECK(seek.restarts == 2 && seek.target == 50000,
         "seek: intermediate request coalesced");
   CHECK(video_player_paused(p), "seek: paused state preserved");
   for (i = 0; i < 10; i++) video_player_poll(p, now + 1000000);
   CHECK(k.nseen == 2, "seek: paused seek shows only one picture");
   video_player_set_paused(p, false, now + 1000000);
   CHECK(video_player_due(p) == now + 1010000,
         "seek: resumed picture retains its full duration");
   for (i = 0; i < 1000 && k.nseen < 3; i++)
   {
      video_player_poll(p, now + 1010000);
      if (threaded) retro_sleep(1);
   }
   CHECK(k.nseen == 3 && k.seen[2] == 6, "seek: next frame follows target");
   video_player_set_paused(p, true, now + 1010000);
   CHECK(video_player_seek(p, -1), "seek: negative target accepted");
   for (i = 0; i < 1000 && (video_player_seeking(p) || k.nseen < 4); i++)
   {
      video_player_poll(p, now + 1010000);
      if (threaded) retro_sleep(1);
   }
   CHECK(k.nseen == 4 && k.seen[3] == 0
         && video_player_seek_position(p, &position) && !position,
         "seek: target clamped to zero");
   seek.fail = 1;
   CHECK(video_player_seek(p, 40000), "seek: failing request accepted");
   for (i = 0; i < 1000 && state != VIDEO_PLAYER_SEEK_FAILED; i++)
   {
      state = video_player_poll(p, now + 1010000);
      if (threaded) retro_sleep(1);
   }
   CHECK(state == VIDEO_PLAYER_SEEK_FAILED && k.nseen == 4,
         "seek: failure retains the last picture");
   CHECK(video_player_poll(p, now + 1010000) == VIDEO_PLAYER_SEEK_FAILED
         && !video_player_seeking(p), "seek: failure is stable without decoding");
   seek.fail = 0;
   CHECK(video_player_seek(p, 0), "seek: failed player accepts recovery");
   for (i = 0; i < 1000 && (video_player_seeking(p) || k.nseen < 5); i++)
   {
      video_player_poll(p, now + 1010000);
      if (threaded) retro_sleep(1);
   }
   CHECK(k.nseen == 5 && k.seen[4] == 0
         && video_player_seek_position(p, &position) && !position,
         "seek: recovery presents a replacement picture");
   video_player_free(p);
   if (hw)
      CHECK(f.hw_made == k.hw_taken + f.hw_abandoned,
            "seek: each GPU lease taken or abandoned exactly once");
}

static void lane_seek_progress(bool threaded)
{
   fake_t f;
   sink_t k;
   video_player_sink_t sink;
   video_player_t *p;
   seek_test_t seek;
   int tick;
   int64_t position = -1;
   fake_init(&f, 12, 10);
   sink_init(&k, &sink, true);
   memset(&seek, 0, sizeof(seek));
   seek.stream = &f;
   seek.progress = true;
   p = video_player_new(&f, IMAGE_TYPE_WEBM, 1, &sink,
         threaded ? 0 : VIDEO_PLAYER_INLINE);
   CHECK(p != NULL, "seek progress: allocation");
   if (!p)
      return;
   video_player_set_seek_handler(p, seek_step, &seek);
   video_player_set_paused(p, true, 1000000);
   CHECK(video_player_seek(p, 40000), "seek progress: initial request");
   video_player_poll(p, 1000000);
   video_player_poll(p, 1000000);
   for (tick = 0; tick < 1000
         && retro_atomic_load_acquire_int(&seek.steps) < 64; tick++)
   {
      if (threaded)
         retro_sleep(1);
      else
         video_player_poll(p, 1000000);
   }
   CHECK(retro_atomic_load_acquire_int(&seek.steps) >= 64,
         "seek progress: worker continues without polling the player");
   CHECK(video_player_seeking(p) && !k.nseen,
         "seek progress: progress is not completion");
   CHECK(video_player_seek(p, 20000), "seek progress: replace running target");
   retro_atomic_store_release_int(&seek.gate, 1);
   for (tick = 0; tick < 1000
         && !video_player_seek_position(p, &position); tick++)
   {
      video_player_poll(p, 1000000);
      if (threaded)
         retro_sleep(1);
   }
   CHECK(video_player_seek_position(p, &position) && position == 20000,
         "seek progress: the latest request completes");
   video_player_free(p);
   video_player_worker_deinit();
}

typedef struct
{
   fake_t stream[2];
   sink_t output[2];
   video_player_sink_t sink[2];
   seek_test_t seek;
} seek_peer_t;

static void lane_seek_peer(bool threaded, bool progress)
{
   seek_peer_t *test = (seek_peer_t*)calloc(1, sizeof(*test));
   video_player_t *pending, *peer;
   int tick;
   int64_t position = -1;
   int64_t now = 1000000;
   CHECK(test != NULL, "seek peer: allocation");
   if (!test)
      return;
   fake_init(&test->stream[0], 12, 10);
   fake_init(&test->stream[1], 5, 10);
   sink_init(&test->output[0], &test->sink[0], true);
   sink_init(&test->output[1], &test->sink[1], true);
   test->seek.stream = &test->stream[0];
   test->seek.progress = progress;
   pending = video_player_new(&test->stream[0], IMAGE_TYPE_WEBM, 1,
         &test->sink[0], threaded ? 0 : VIDEO_PLAYER_INLINE);
   peer = video_player_new(&test->stream[1], IMAGE_TYPE_WEBM, 1,
         &test->sink[1], threaded ? 0 : VIDEO_PLAYER_INLINE);
   CHECK(pending && peer, "seek peer: players");
   if (!pending || !peer)
      goto cleanup;
   CHECK(video_player_set_seek_handler(pending, seek_step, &test->seek),
         "seek peer: handler");
   video_player_set_paused(pending, true, now);
   CHECK(video_player_seek(pending, 40000), "seek peer: request");
   for (tick = 0; tick < 1000
         && !retro_atomic_load_acquire_int(&test->seek.steps); tick++)
   {
      video_player_poll(pending, now);
      if (threaded)
         retro_sleep(1);
   }
   CHECK(retro_atomic_load_acquire_int(&test->seek.steps) > 0,
         "seek peer: pending work started");
   for (tick = 0; tick < 1000 && !video_player_completed(peer); tick++)
   {
      video_player_poll(pending, now);
      video_player_poll(peer, now);
      now += 10000;
      if (threaded)
         retro_sleep(1);
   }
   CHECK(video_player_completed(peer) && test->output[1].nseen == 5
         && seen_in_order(&test->output[1], 5) && !test->output[1].bad_px,
         "seek peer: other player reaches EOF (%d/%d)", threaded, progress);
   CHECK(video_player_seeking(pending) && !test->output[0].nseen
         && !video_player_seek_position(pending, &position),
         "seek peer: pending seek publishes nothing");
   CHECK(video_player_seek(pending, 20000), "seek peer: replace target");
   retro_atomic_store_release_int(&test->seek.gate, 1);
   for (tick = 0; tick < 1000 && (video_player_seeking(pending)
            || !test->output[0].nseen); tick++)
   {
      video_player_poll(pending, now);
      if (threaded)
         retro_sleep(1);
   }
   CHECK(video_player_seek_position(pending, &position) && position == 20000
         && test->output[0].nseen == 1 && test->output[0].seen[0] == 2,
         "seek peer: only replacement picture published");
cleanup:
   video_player_free(pending);
   video_player_free(peer);
   video_player_worker_deinit();
   CHECK(!test->output[0].begun[0] && !test->output[0].begun[1]
         && !test->output[1].begun[0] && !test->output[1].begun[1],
         "seek peer: presentation slots retired");
   free(test);
}

static void lane_seek_stop(bool threaded)
{
   fake_t f;
   sink_t k;
   video_player_sink_t s;
   video_player_t *p;
   seek_test_t seek;
   int i, steps;
   fake_init(&f, 12, 10);
   sink_init(&k, &s, true);
   memset(&seek, 0, sizeof(seek));
   seek.stream = &f;
   p = video_player_new(&f, IMAGE_TYPE_WEBM, 1, &s,
         threaded ? 0 : VIDEO_PLAYER_INLINE);
   CHECK(p != NULL, "seek stop: allocation");
   if (!p) return;
   video_player_set_seek_handler(p, seek_step, &seek);
   video_player_set_paused(p, true, 1000000);
   CHECK(video_player_seek(p, 40000), "seek stop: before first poll");
   for (i = 0; i < 1000
         && !retro_atomic_load_acquire_int(&seek.steps); i++)
   {
      video_player_poll(p, 1000000);
      if (threaded) retro_sleep(1);
   }
   CHECK(!k.nseen && video_player_seeking(p),
         "seek stop: no picture from incomplete initial seek");
   while (!video_player_stop(p)) retro_sleep(1);
   CHECK(!video_player_seek(p, 0) && !video_player_seeking(p),
         "seek stop: cannot resume stopped request");
   steps = retro_atomic_load_acquire_int(&seek.steps);
   video_player_free(p);
   video_player_worker_deinit();
   CHECK(retro_atomic_load_acquire_int(&seek.steps) == steps,
         "seek stop: callback not run after stream released");
}

/* A short video track inside a longer movie must preserve both its
 * leading interval and the audio tail on every loop. Test the clock
 * independently of decode speed by polling each timestamp repeatedly. */
static void lane_movie(bool threaded, bool hardware, bool pause_lead)
{
   fake_t f;
   sink_t k;
   video_player_sink_t s;
   video_player_t *p;
   enum video_player_state state = VIDEO_PLAYER_PLAYING;
   int tick, retry;
   const int64_t origin = 1000000;
   int64_t now;
   fake_init(&f, 2, 100);
   f.direct = true;
   f.movie_ns = (int64_t)4000000 * 1000;
   f.lead_ns = 1000000000;
   if (hardware)
   {
      f.hw_can = true;
      sink_init_hw(&k, &s);
      k.hw_busy_every = 2;
   }
   else sink_init(&k, &s, true);
   p = video_player_new(&f, IMAGE_TYPE_MP4, 2, &s,
         threaded ? 0 : VIDEO_PLAYER_INLINE);
   CHECK(p != NULL, "movie: allocation");
   if (!p) return;
   CHECK(!video_player_started(p), "movie: sink not ready before poll");
   if(hardware)settle_hw_start(p,origin);
   for (tick = 0; tick <= 800; tick++)
   {
      int expected = tick < 100 ? 0 : tick < 110 ? 1
         : tick < 500 ? 2 : tick < 510 ? 3 : 4;
      now = origin + (int64_t)tick * 10000;
      if (pause_lead && tick == 50)
      {
         video_player_set_paused(p, true, now);
         for (retry = 0; retry < 8; retry++)
            video_player_poll(p, now + 1000000);
         CHECK(!k.nseen, "movie: pause preserves leading gap");
         video_player_set_paused(p, false, now + 2000000);
      }
      if (pause_lead && tick >= 50) now += 2000000;
      for (retry = 0; retry < 12; retry++)
      {
         state = video_player_poll(p, now);
         if (threaded) retro_sleep(1);
      }
      CHECK(k.nseen == expected, "movie: tick %d pictures %d expected %d",
            tick, k.nseen, expected);
      CHECK(video_player_has_presented(p) == (expected != 0),
            "movie: due time is not a presentation");
      CHECK(video_player_started(p), "movie: ready during leading interval");
      CHECK(state == (tick < 800 ? VIDEO_PLAYER_PLAYING : VIDEO_PLAYER_ENDED),
            "movie: complete period at tick %d", tick);
      CHECK(video_player_completed(p) == (tick >= 800),
            "movie: clean EOF waits for final presentation duration");
   }
   CHECK(k.seen[0] == 0 && k.seen[1] == 1
         && k.seen[2] == 0 && k.seen[3] == 1 && !k.bad_px,
         "movie: repeated picture sequence");
   CHECK(k.times[0] == 1000000 && k.times[1] == 1100000
         && k.times[2] == 5000000 && k.times[3] == 5100000,
         "movie: published timestamps preserve lead and loop epoch");
   video_player_free(p);
   video_player_worker_deinit();
   CHECK(!hardware || f.hw_made == f.hw_abandoned + k.hw_taken,
         "movie: every hardware lease accounted for");
}

static void lane_timestamp_precision(bool threaded)
{
   fake_t f;
   sink_t k;
   video_player_sink_t s;
   video_player_t *p;
   int tick;
   fake_init(&f, 3, 42);
   f.movie_ns = 200000000;
   f.lead_ns = 1234567;
   f.frame_step_ns = 41708333;
   sink_init(&k, &s, true);
   p = video_player_new(&f, IMAGE_TYPE_MP4, 1, &s,
         threaded ? 0 : VIDEO_PLAYER_INLINE);
   CHECK(p != NULL, "timestamp: allocation");
   if (!p)
      return;
   for (tick = 0; tick < 250 && k.nseen < 3; tick++)
   {
      video_player_poll(p, (int64_t)tick * 1000);
      if (threaded)
         retro_sleep(1);
   }
   CHECK(k.nseen == 3 && k.times[0] == 1234 && k.times[1] == 42942
         && k.times[2] == 84651,
         "timestamp: fractional cadence survives millisecond frame duration");
   video_player_free(p);
   video_player_worker_deinit();
}

static void lane_cadence(bool threaded, unsigned mode, bool hardware)
{
   fake_t f;
   sink_t k;
   video_player_sink_t s;
   video_player_t *p;
   seek_test_t seek;
   unsigned num = 0, den = 0;
   int tick;
   fake_init(&f, 60, 42);
   f.movie_ns = (int64_t)3000000 * 1000;
   f.frame_step_ns = 41708333;
   f.stamp_jitter_ns = mode == 1 ? 7000000 : 0;
   f.stamp_quantized = mode == 2;
   memset(&seek, 0, sizeof(seek));
   seek.stream = &f;
   retro_atomic_int_init(&seek.gate, 1);
   retro_atomic_int_init(&seek.steps, 0);
   if (hardware)
   {
      f.hw_can = true;
      sink_init_hw(&k, &s);
      k.hw_busy_every = 2;
   }
   else
      sink_init(&k, &s, true);
   p = video_player_new(&f, IMAGE_TYPE_MP4, 1, &s,
         threaded ? 0 : VIDEO_PLAYER_INLINE);
   CHECK(p != NULL, "cadence: allocation");
   if (!p) return;
   CHECK(video_player_set_seek_handler(p, seek_step, &seek), "cadence: seek setup");
   if (hardware)
      settle_hw_start(p, 0);
   for (tick = 0; tick < 3000 && k.nseen < (mode == 1 ? 60 : 33); tick++)
   {
      video_player_poll(p, (int64_t)tick * 1000);
      if (k.nseen < 33 || mode == 1)
         CHECK(!video_player_cadence(p, &num, &den),
               "cadence: insufficient or variable timing");
      if (threaded) retro_sleep(1);
   }
   if (mode != 1)
   {
      CHECK(k.nseen == 33 && video_player_cadence(p, &num, &den)
            && num == 32000000 && (mode == 2 ? den == 1334000
               : den >= 1334666 && den <= 1334667),
            "cadence: measured rate thread=%d hardware=%d mode=%u seen=%d rate=%u/%u",
            threaded, hardware, mode, k.nseen, num, den);
      CHECK(!video_player_cadence_unstable(p), "cadence: stable evidence");
      CHECK(video_player_seek(p, 0), "cadence: seek accepted");
      CHECK(!video_player_cadence(p, &num, &den), "cadence: seek clears evidence");
      CHECK(!video_player_cadence_unstable(p), "cadence: seek is not variable timing");
   }
   else
   {
      CHECK(k.nseen == 60, "cadence: variable stream completes");
      CHECK(video_player_cadence_unstable(p), "cadence: variable evidence");
   }
   video_player_stop(p);
   CHECK(!video_player_cadence(p, &num, &den), "cadence: stopped player has no rate");
   video_player_free(p);
   video_player_worker_deinit();
}

static void lane_cadence_loops(bool threaded)
{
   fake_t f;
   sink_t k;
   video_player_sink_t s;
   video_player_t *p;
   unsigned num = 0, den = 0;
   int tick, previous = 0;
   fake_init(&f, 40, 42);
   f.movie_ns = 2000000000;
   f.frame_step_ns = 41708333;
   sink_init(&k, &s, true);
   p = video_player_new(&f, IMAGE_TYPE_MP4, 2, &s,
         threaded ? 0 : VIDEO_PLAYER_INLINE);
   CHECK(p != NULL, "cadence loops: allocation");
   if (!p)
      return;
   for (tick = 0; tick < 4500 && k.nseen < 80; tick++)
   {
      video_player_poll(p, (int64_t)tick * 1000);
      if (k.nseen != previous && k.nseen >= 33)
         CHECK(video_player_cadence(p, &num, &den)
               && !video_player_cadence_unstable(p)
               && num == 32000000 && den >= 1334666 && den <= 1334667,
               "cadence loops: audio tail is not variable picture timing");
      previous = k.nseen;
      if (threaded)
         retro_sleep(1);
   }
   CHECK(k.nseen == 80, "cadence loops: both complete picture sequences");
   video_player_free(p);
   video_player_worker_deinit();
}

static void lane_movie_stop(bool threaded)
{
   fake_t f;
   sink_t k;
   video_player_sink_t s;
   video_player_t *p;
   unsigned i;
   fake_init(&f, 2, 100);
   f.movie_ns = (int64_t)4000000 * 1000;
   f.lead_ns = 1000000000;
   f.hw_can = true;
   sink_init_hw(&k, &s);
   p = video_player_new(&f, IMAGE_TYPE_MP4, 0, &s,
         threaded ? 0 : VIDEO_PLAYER_INLINE);
   CHECK(p != NULL, "movie stop: allocation");
   if (!p) return;
   settle_hw_start(p,1000000);
   for (i = 0; i < 30; i++)
   {
      video_player_poll(p, 1000000);
      if (threaded) retro_sleep(1);
   }
   while (!video_player_stop(p)) retro_sleep(1);
   video_player_free(p);
   video_player_worker_deinit();
   CHECK(!k.nseen && f.hw_made && f.hw_abandoned == f.hw_made,
         "movie stop: future hardware leases returned once");
}

static int fallback_restart(void *user)
{
   sink_t *k = (sink_t *)user;
   fake_t *f = k->fallback_stream;
#ifdef HAVE_THREADS
   bool worker = sthread_get_current_thread_id() != k->fallback_caller;
   CHECK(worker == (k->fallback_threaded || k->hw_start_refused),
         "fallback: restart ran on the wrong stream owner");
#endif
   k->fallback_calls++;
   retro_atomic_store_release_int(&k->fallback_entered, 1);
   if (k->fallback_error)
      return -1;
   if (k->fallback_waits)
   {
      k->fallback_waits--;
      return 0;
   }
   f->hw_on = f->hw_pending = false;
   f->pos = 0;
   return 1;
}
static enum video_player_present
fallback_present(void *user, const video_player_frame_t *frame)
{
   sink_t *k = (sink_t *)user;
   if (frame->hw)
   {
      if (!k->fallback_async)
         return VIDEO_PLAYER_PRESENT_FAILED;
      k->fallback_held = *frame->hw;
      k->held = (int)frame->slot;
      return VIDEO_PLAYER_PRESENT_QUEUED;
   }
   return sink_present(user, frame);
}
static void lane_hw_fallback(bool threaded, bool refused, bool decode_failed,
                             bool async)
{
   fake_t f;
   sink_t k;
   video_player_sink_t sink;
   video_player_t *p;
   enum video_player_state state = VIDEO_PLAYER_PLAYING;
   int polls;
   int64_t now = 1000000;
   fake_init(&f, 6, 5);
   f.hw_can = true;
   f.movie_ns = 1000000000;
   if (decode_failed)
      f.hw_fail_from = 1;
   sink_init_hw(&k, &sink);
   k.fallback_stream = &f;
   k.hw_start_refused = refused;
   k.fallback_waits = 2;
   k.fallback_threaded = threaded;
   k.fallback_async = async;
#ifdef HAVE_THREADS
   k.fallback_caller = sthread_get_current_thread_id();
#endif
   sink.present = fallback_present;
   p = video_player_new(&f, IMAGE_TYPE_MP4, 1, &sink,
                        threaded ? 0 : VIDEO_PLAYER_INLINE);
   CHECK(p && video_player_set_hw_fallback(p, fallback_restart, &k),
         "fallback: setup failed");
   settle_hw_start(p, now);
   for (polls = 0; polls < 3000 && state == VIDEO_PLAYER_PLAYING; polls++)
   {
      state = video_player_poll(p, now);
      if (k.held >= 0)
      {
         CHECK(video_player_request_software(p) &&
                     video_player_request_software(p),
               "fallback async: refusal request was not idempotent");
         video_player_poll(p, now);
         CHECK(!k.fallback_calls,
               "fallback async: restarted before releasing the held slot");
         video_hw_frame_abandon(&k.fallback_held);
         video_player_slot_released(p, (unsigned)k.held);
         k.held = -1;
      }
      now += 6000;
      if (threaded)
         retro_sleep(1);
   }
   CHECK(state == VIDEO_PLAYER_ENDED && k.nseen == 6 && seen_in_order(&k, 6),
         "fallback: %d frames, state %d (thread %d refused %d decode %d)",
         k.nseen, state, threaded, refused, decode_failed);
   CHECK(f.startup_calls == 1, "fallback: hardware admission repeated");
   CHECK(k.hw_started == 1 && k.fallback_calls == 3 && !f.hw_on,
         "fallback: selected hardware again or failed to finish bounded "
         "restart");
   if (!refused)
      CHECK(video_player_due(p) == 2000000,
            "fallback: movie clock origin reset during recovery");
   video_player_free(p);
   CHECK(f.hw_made == f.hw_abandoned,
         "fallback: unused GPU lease lost or returned twice");
}

#ifdef VP_TEST_CALLOC_FAULTS
static int lane_hw_alloc_failure(int fail_at)
{
   fake_t f;
   sink_t k;
   video_player_sink_t sink;
   video_player_t *p;
   unsigned i;
   int count;

   video_player_worker_deinit();
   fake_init(&f, 3, 10);
   f.hw_can = true;
   sink_init_hw(&k, &sink);
   retro_atomic_store_relaxed_int(&alloc_count, 0);
   retro_atomic_store_relaxed_int(&alloc_fail, fail_at);
   retro_atomic_store_release_int(&alloc_enabled, 1);
   p = video_player_new(&f, IMAGE_TYPE_MP4, 1, &sink, VIDEO_PLAYER_INLINE);
   if (p)
   {
      for (i = 0; i < 1000; i++)
      {
         enum video_player_state state = video_player_poll(p, 1000000);
         if (video_player_started(p) || state == VIDEO_PLAYER_ENDED)
            break;
         retro_sleep(1);
      }
      CHECK(i < 1000, "allocation %d: admission never settled", fail_at);
      for (i = 0; i < 1000 && !video_player_stop(p); i++)
         retro_sleep(1);
      CHECK(i < 1000, "allocation %d: cancellation never settled", fail_at);
      video_player_free(p);
   }
   video_player_worker_deinit();
   retro_atomic_store_release_int(&alloc_enabled, 0);
   count = retro_atomic_load_acquire_int(&alloc_count);
   CHECK(!fail_at || count >= fail_at,
         "allocation %d: fault was not reached", fail_at);
   CHECK(p || fail_at == 1, "allocation %d: unexpected player failure", fail_at);
   CHECK(!k.begun[0] && !k.begun[1],
         "allocation %d: sink slot lost", fail_at);
   CHECK(f.hw_made == f.hw_abandoned + k.hw_taken,
         "allocation %d: native lease lost", fail_at);
   return count;
}

static void lane_hw_alloc_failures(void)
{
   int ordinal;
   int count = lane_hw_alloc_failure(0);
   CHECK(count > 0, "allocation faults: no setup allocations observed");
   for (ordinal = 1; ordinal <= count; ordinal++)
      lane_hw_alloc_failure(ordinal);
   printf("hardware admission allocation failures: %d sites\n", count);
}

#ifdef HAVE_THREADS
static void lane_hw_worker_failure(void)
{
   fake_t f;
   sink_t k;
   video_player_sink_t sink;
   video_player_t *p;

   video_player_worker_deinit();
   fake_init(&f, 3, 10);
   f.hw_can = true;
   f.startup_poll = sthread_get_current_thread_id();
   sink_init_hw(&k, &sink);
   sink.present = sink_present;
   p = video_player_new(&f, IMAGE_TYPE_MP4, 1, &sink, VIDEO_PLAYER_INLINE);
   CHECK(p != NULL, "worker failure: player allocation");
   if (!p)
      return;
   retro_atomic_store_release_int(&fail_worker, 1);
   video_player_poll(p, 1000000);
   CHECK(!retro_atomic_load_acquire_int(&fail_worker),
         "worker failure: thread creation fault was not reached");
   CHECK(video_player_started(p) && k.started == 1 && !k.hw_started,
         "worker failure: software playback did not start");
   CHECK(!f.startup_calls,
         "worker failure: native constructor ran without its worker");
   CHECK(video_player_stop(p), "worker failure: stop did not settle");
   video_player_free(p);
   CHECK(!k.begun[0] && !k.begun[1], "worker failure: sink slot lost");
   puts("hardware admission worker creation failure: passed");
}
#endif
#endif

#ifdef HAVE_THREADS
static void lane_hw_failure_deadline(bool recover)
{
   fake_t f;
   sink_t k;
   video_player_sink_t sink;
   video_player_t *p;
   unsigned i;
   enum video_player_state state = VIDEO_PLAYER_PLAYING;
   fake_init(&f, 6, 5);
   f.hw_can = true;
   f.hw_fail_from = 1;
   f.movie_ns = f.hw_failure_ns = (int64_t)30000 * 1000000;
   sink_init_hw(&k, &sink);
   sink.present = fallback_present;
   k.fallback_stream = &f;
   k.fallback_threaded = true;
   k.fallback_caller = sthread_get_current_thread_id();
   p = video_player_new(&f, IMAGE_TYPE_MP4, 1, &sink, 0);
   CHECK(p && (!recover || video_player_set_hw_fallback(p, fallback_restart, &k)),
         "failure deadline: setup");
   if (!p)
      return;
   settle_hw_start(p, 1000000);
   for (i = 0; i < 1000 && !k.nseen; i++)
   {
      state = video_player_poll(p, 1000000);
      if (state == VIDEO_PLAYER_ENDED)
         break;
      retro_sleep(1);
   }
   CHECK(recover ? state == VIDEO_PLAYER_PLAYING && k.nseen == 1
         && k.seen[0] == 0 && k.fallback_calls == 1
         : state == VIDEO_PLAYER_ENDED && !k.nseen && !k.fallback_calls,
         "failure deadline: waited for movie end (recovery %d)", recover);
   for (i = 0; i < 1000 && !video_player_stop(p); i++)
      retro_sleep(1);
   CHECK(i < 1000, "failure deadline: stop did not settle");
   video_player_free(p);
   CHECK(f.hw_made == f.hw_abandoned, "failure deadline: native lease lost");
}
#endif

static void lane_empty_movie_deadline(bool threaded, bool completed)
{
   fake_t f;
   sink_t k;
   video_player_sink_t sink;
   video_player_t *p;
   unsigned i;
   enum video_player_state state = VIDEO_PLAYER_PLAYING;

   fake_init(&f, completed ? 0 : 6, 5);
   f.failed_stream = !completed;
   f.movie_ns = (int64_t)30000 * 1000000;
   sink_init(&k, &sink, true);
   p = video_player_new(&f, IMAGE_TYPE_MP4, 1, &sink,
         threaded ? 0 : VIDEO_PLAYER_INLINE);
   CHECK(p != NULL, "empty movie: setup");
   if (!p)
      return;
   for (i = 0; i < 1000 && state == VIDEO_PLAYER_PLAYING; i++)
   {
      state = video_player_poll(p, 1000000);
      if (threaded)
         retro_sleep(1);
   }
   CHECK(state == VIDEO_PLAYER_ENDED && !k.nseen
         && !video_player_completed(p),
         "empty movie: waited for movie end (thread %d, completed %d)",
         threaded, completed);
   for (i = 0; i < 1000 && !video_player_stop(p); i++)
      retro_sleep(1);
   CHECK(i < 1000, "empty movie: stop did not settle");
   video_player_free(p);
}

static void lane_hw_start_result(bool refused)
{
   fake_t f;
   sink_t k;
   video_player_sink_t sink;
   video_player_t *p;
   enum video_player_state state = VIDEO_PLAYER_PLAYING;
   unsigned i;

   fake_init(&f, 3, 10);
   f.hw_can = true;
   sink_init_hw(&k, &sink);
   k.hw_start_refused = refused;
   k.hw_start_waits = refused ? 0 : 2;
   p = video_player_new(&f, IMAGE_TYPE_MP4, 1, &sink, VIDEO_PLAYER_INLINE);
   CHECK(p != NULL, "admission result: allocation");
   if (!p)
      return;
   for (i = 0; i < 1000; i++)
   {
      state = video_player_poll(p, 1000000);
      if (state == VIDEO_PLAYER_ENDED || video_player_started(p))
         break;
      retro_sleep(1);
   }
   if (refused)
      CHECK(state == VIDEO_PLAYER_ENDED && k.hw_started == 1 && !k.started,
            "permanent admission refusal without recovery did not end");
   else
      CHECK(state == VIDEO_PLAYER_PLAYING && video_player_started(p) &&
                  k.hw_started == 3 && !k.started,
            "temporary admission retry did not start hardware");
#ifdef HAVE_THREADS
   CHECK(f.startup_calls == 1, "admission result: constructor repeated");
#endif
   for (i = 0; i < 1000 && !video_player_stop(p); i++)
      retro_sleep(1);
   CHECK(i < 1000, "admission result: stop did not settle");
   video_player_free(p);
   CHECK(f.hw_made == f.hw_abandoned + k.hw_taken,
         "admission result: native lease lost");
}

static void lane_hw_recovery_state(bool threaded, bool paused, bool seek_first)
{
   fake_t f;
   sink_t k;
   video_player_sink_t sink;
   video_player_t *p;
   seek_test_t seek;
   enum video_player_state state = VIDEO_PLAYER_PLAYING;
   int64_t now = 1000000;
   int64_t position = -1;
   unsigned i;

   fake_init(&f, 12, 10);
   f.hw_can = true;
   f.movie_ns = 1000000000;
   sink_init_hw(&k, &sink);
   k.fallback_stream = &f;
   k.fallback_threaded = threaded;
   k.fallback_async = true;
#ifdef HAVE_THREADS
   k.fallback_caller = sthread_get_current_thread_id();
#endif
   sink.present = fallback_present;
   memset(&seek, 0, sizeof(seek));
   seek.stream = &f;
   retro_atomic_int_init(&seek.steps, 0);
   retro_atomic_int_init(&seek.gate, 1);
   p = video_player_new(&f, IMAGE_TYPE_MP4, 1, &sink,
                        threaded ? 0 : VIDEO_PLAYER_INLINE);
   CHECK(p && video_player_set_hw_fallback(p, fallback_restart, &k),
         "recovery state: setup");
   if (!p)
      return;
   if (seek_first)
      CHECK(video_player_set_seek_handler(p, seek_step, &seek) &&
                  video_player_seek(p, 40000),
            "recovery state: initial seek");
   settle_hw_start(p, now);
   for (i = 0; i < 1000 && k.held < 0; i++)
   {
      video_player_poll(p, now);
      retro_sleep(1);
   }
   CHECK(k.held >= 0 && !k.nseen, "recovery state: initial GPU lease");
   if (seek_first)
      CHECK(video_player_seek_position(p, &position) && position == 40000,
            "recovery state: initial seek did not complete");
   video_player_set_paused(p, paused, now);
   CHECK(video_player_request_software(p), "recovery state: refusal request");
   video_hw_frame_abandon(&k.fallback_held);
   video_player_slot_released(p, (unsigned)k.held);
   k.held = -1;
   for (i = 0; i < 1000 && !k.nseen; i++)
   {
      state = video_player_poll(p, now);
      retro_sleep(1);
   }
   CHECK(state == VIDEO_PLAYER_PLAYING && k.nseen == 1 &&
               k.seen[0] == (seek_first ? 4 : 0),
         "recovery state: first software frame %d, count %d (thread %d pause "
         "%d seek %d)",
         k.seen[0], k.nseen, threaded, paused, seek_first);
   CHECK(video_player_paused(p) == paused, "recovery state: pause changed");
   if (seek_first)
      CHECK(seek.restarts == 2 && video_player_seek_position(p, &position) &&
                  position == 40000,
            "recovery state: target not restored");
   for (i = 0; i < 1000 && !video_player_stop(p); i++)
      retro_sleep(1);
   CHECK(i < 1000, "recovery state: stop did not settle");
   video_player_free(p);
   CHECK(f.hw_made == f.hw_abandoned, "recovery state: native lease lost");
}

static void lane_hw_fallback_stop(bool threaded, bool cancel)
{
   fake_t f;
   sink_t k;
   video_player_sink_t sink;
   video_player_t *p;
   enum video_player_state state = VIDEO_PLAYER_PLAYING;
   int polls;
   int64_t now = 0;
   fake_init(&f, 6, 5);
   f.hw_can = true;
   sink_init_hw(&k, &sink);
   k.fallback_stream = &f;
   k.fallback_threaded = threaded;
   k.fallback_error = !cancel;
   k.fallback_waits = cancel ? 10000 : 0;
#ifdef HAVE_THREADS
   k.fallback_caller = sthread_get_current_thread_id();
#endif
   sink.present = fallback_present;
   p = video_player_new(&f, IMAGE_TYPE_MP4, 1, &sink,
                        threaded ? 0 : VIDEO_PLAYER_INLINE);
   CHECK(p && video_player_set_hw_fallback(p, fallback_restart, &k),
         "fallback stop: setup");
   for (polls = 0; polls < 1000 && state == VIDEO_PLAYER_PLAYING; polls++)
   {
      state = video_player_poll(p, now);
      now += 6000;
      if (cancel && retro_atomic_load_acquire_int(&k.fallback_entered))
         break;
      if (threaded || !video_player_started(p))
         retro_sleep(1);
   }
   if (cancel)
   {
      CHECK(retro_atomic_load_acquire_int(&k.fallback_entered),
            "fallback stop: callback never ran");
      for (polls = 0; polls < 1000 && !video_player_stop(p); polls++)
         retro_sleep(1);
      CHECK(polls < 1000, "fallback stop: decoder owner never settled");
   }
   else
      CHECK(state == VIDEO_PLAYER_ENDED && k.fallback_calls == 1,
            "fallback stop: failed restart retried forever");
   CHECK(video_player_poll(p, now) == VIDEO_PLAYER_ENDED && !k.nseen,
         "fallback stop: stopped player resumed");
   video_player_free(p);
   CHECK(f.hw_made == f.hw_abandoned,
         "fallback stop: GPU lease lost or returned twice");
}

static void lane_nonmovie_hw_sink(void)
{
   fake_t f;
   sink_t k;
   video_player_sink_t sink;
   video_player_t *p;
   fake_init(&f, 2, 10);
   f.hw_can = true;
   sink_init_hw(&k, &sink);
   sink.present = fallback_present;
   p = video_player_new(&f, IMAGE_TYPE_WEBP, 1, &sink, VIDEO_PLAYER_INLINE);
   CHECK(p != NULL, "nonmovie startup: allocation");
   if (!p)
      return;
   video_player_poll(p, 1000000);
   CHECK(!f.startup_calls && !k.hw_started && k.started == 1 && k.nseen == 1,
         "nonmovie startup: deferred an unsupported hardware probe");
   video_player_free(p);
}

#ifdef HAVE_THREADS
static enum video_player_present
startup_present(void *user, const video_player_frame_t *frame)
{
   return frame->hw ? sink_present_hw(user, frame) : sink_present(user, frame);
}
static void lane_hw_startup(bool inline_frames, bool cancel, bool available)
{
   fake_t f;
   sink_t k;
   video_player_sink_t sink;
   video_player_t *p;
   unsigned i;
   fake_init(&f, 3, 5);
   f.hw_can = available;
   sink_init_hw(&k, &sink);
   sink.present = startup_present;
   sink.slot_end = sink_slot_end;
   f.startup_poll = sthread_get_current_thread_id();
   retro_atomic_store_release_int(&f.startup_hold, 1);
   p = video_player_new(&f, IMAGE_TYPE_MP4, 1, &sink,
                        inline_frames ? VIDEO_PLAYER_INLINE : 0);
   CHECK(p != NULL, "startup gate: allocation");
   if (!p)
      return;
   CHECK(video_player_poll(p, 1000000) == VIDEO_PLAYER_PLAYING,
         "startup gate: first poll");
   for (i = 0; i < 1000 && !retro_atomic_load_acquire_int(&f.startup_entered);
        i++)
      retro_sleep(1);
   CHECK(retro_atomic_load_acquire_int(&f.startup_entered),
         "startup gate: constructor never entered");
   CHECK(f.startup_thread != f.startup_poll,
         "startup gate: polling constructor");
   CHECK(!video_player_started(p) && !k.started && !k.hw_started && !k.nseen,
         "startup gate: sink or decode ran before native setup settled");
   CHECK(video_player_poll(p, 1000000) == VIDEO_PLAYER_PLAYING,
         "startup gate: pending poll");
   if (cancel)
      CHECK(!video_player_stop(p),
            "startup gate: running constructor not retained");
   retro_atomic_store_release_int(&f.startup_hold, 0);
   if (cancel)
   {
      for (i = 0; i < 1000 && !video_player_stop(p); i++)
         retro_sleep(1);
      CHECK(i < 1000 && video_player_poll(p, 1000000) == VIDEO_PLAYER_ENDED,
            "startup gate: cancelled constructor did not settle");
      CHECK(!k.started && !k.hw_started && !k.nseen,
            "startup gate: stopped sink resumed");
   }
   else
   {
      settle_hw_start(p, 1000000);
      CHECK((k.hw_started != 0) == available,
            "startup gate: incorrect hardware admission");
   }
   CHECK(f.startup_calls == 1, "startup gate: constructor repeated");
   video_player_free(p);
   if (cancel || available)
      CHECK(!k.ended, "startup gate: ended an unlent sink slot");
   CHECK(f.hw_made == f.hw_abandoned + k.hw_taken, "startup gate: lease lost");
}
static void lane_hw_startup_seek(bool inline_frames)
{
   fake_t f;
   sink_t k;
   video_player_sink_t sink;
   video_player_t *p;
   seek_test_t seek;
   unsigned i;
   int64_t position = -1;
   fake_init(&f, 12, 10);
   f.hw_can = true;
   f.startup_poll = sthread_get_current_thread_id();
   sink_init_hw(&k, &sink);
   memset(&seek, 0, sizeof(seek));
   seek.stream = &f;
   retro_atomic_int_init(&seek.steps, 0);
   retro_atomic_int_init(&seek.gate, 1);
   retro_atomic_store_release_int(&f.startup_hold, 1);
   p = video_player_new(&f, IMAGE_TYPE_MP4, 1, &sink,
                        inline_frames ? VIDEO_PLAYER_INLINE : 0);
   CHECK(p && video_player_set_seek_handler(p, seek_step, &seek),
         "startup seek: setup");
   if (!p)
      return;
   video_player_poll(p, 1000000);
   for (i = 0; i < 1000 && !retro_atomic_load_acquire_int(&f.startup_entered);
        i++)
      retro_sleep(1);
   CHECK(retro_atomic_load_acquire_int(&f.startup_entered),
         "startup seek: admission never entered");
   CHECK(video_player_seek(p, 20000) && video_player_seek(p, 40000),
         "startup seek: requests rejected");
   video_player_poll(p, 1000000);
   CHECK(!retro_atomic_load_acquire_int(&seek.steps) && !k.nseen,
         "startup seek: transport raced native admission");
   retro_atomic_store_release_int(&f.startup_hold, 0);
   for (i = 0; i < 1000 && !k.nseen; i++)
   {
      video_player_poll(p, 1000000);
      retro_sleep(1);
   }
   CHECK(k.nseen && k.seen[0] == 4 && seek.restarts == 1,
         "startup seek: latest target was not the first picture");
   CHECK(video_player_seek_position(p, &position) && position == 40000,
         "startup seek: target did not complete");
   while (!video_player_stop(p))
      retro_sleep(1);
   video_player_free(p);
   CHECK(f.startup_calls == 1 && f.hw_made == f.hw_abandoned + k.hw_taken,
         "startup seek: repeated admission or lost frame");
}
static void lane_hw_startup_queued_cancel(void)
{
   static fake_t f[2];
   static sink_t k[2];
   video_player_sink_t sink[2];
   video_player_t *p[2];
   unsigned i;
   for (i = 0; i < 2; i++)
   {
      fake_init(&f[i], 3, 5);
      f[i].hw_can = true;
      f[i].startup_poll = sthread_get_current_thread_id();
      sink_init_hw(&k[i], &sink[i]);
      sink[i].slot_end = sink_slot_end;
      p[i] = video_player_new(&f[i], IMAGE_TYPE_MP4, 1, &sink[i], 0);
      CHECK(p[i] != NULL, "startup queue: allocation");
   }
   retro_atomic_store_release_int(&f[0].startup_hold, 1);
   video_player_poll(p[0], 1000000);
   for (i = 0;
        i < 1000 && !retro_atomic_load_acquire_int(&f[0].startup_entered); i++)
      retro_sleep(1);
   CHECK(retro_atomic_load_acquire_int(&f[0].startup_entered),
         "startup queue: owner never entered");
   video_player_poll(p[1], 1000000);
   CHECK(video_player_stop(p[1]), "startup queue: queued cancellation waited");
   video_player_free(p[1]);
   retro_atomic_store_release_int(&f[0].startup_hold, 0);
   for (i = 0; i < 1000 && !video_player_stop(p[0]); i++)
      retro_sleep(1);
   CHECK(i < 1000, "startup queue: owner never settled");
   video_player_free(p[0]);
   video_player_worker_deinit();
   CHECK(!f[1].startup_calls && !k[1].hw_started && !k[1].nseen,
         "startup queue: cancelled constructor ran");
   CHECK(!k[0].ended && !k[1].ended,
         "startup queue: ended an unlent sink slot");
}
#endif

#ifdef HAVE_THREADS
static void lane_delayed_slot_releases(void)
{
   fake_t f;
   sink_t k;
   video_player_sink_t sink;
   video_player_t *player;
   unsigned tick, released = 4, i;
   enum video_player_state state = VIDEO_PLAYER_PLAYING;
   fake_init(&f, 8, 10);
   sink_init(&k, &sink, true);
   k.queue_every = 1;
   player = video_player_new(&f, IMAGE_TYPE_WEBM, 2, &sink, 0);
   CHECK(player != NULL, "delayed release: allocation");
   if (!player) return;
   for (tick = 0; tick < 1000 && k.nseen < 2; tick++)
   {
      video_player_poll(player, (int64_t)tick * 10000);
      retro_sleep(1);
   }
   CHECK(k.nseen == 2, "delayed release: both slots retained");
   k.held = -1;
   video_player_slot_released(player, 0);
   video_player_slot_released(player, 1);
   for (; tick < 2000 && k.nseen < 4; tick++)
   {
      video_player_poll(player, (int64_t)tick * 10000);
      retro_sleep(1);
   }
   CHECK(k.nseen == 4 && !k.bad_px,
         "delayed release: decoding resumes after both releases");
   k.held = -1;
   video_player_slot_released(player, 0);
   video_player_slot_released(player, 1);
   for (; tick < 4000 && state != VIDEO_PLAYER_ENDED; tick++)
   {
      state = video_player_poll(player, (int64_t)tick * 10000);
      if (k.nseen >= (int)released + 2)
      {
         released = (unsigned)k.nseen;
         k.held = -1;
         video_player_slot_released(player, 0);
         video_player_slot_released(player, 1);
      }
      retro_sleep(1);
   }
   CHECK(state == VIDEO_PLAYER_ENDED && k.nseen == 16,
         "delayed release: exactly two complete loops");
   for (i = 0; i < 16 && i < (unsigned)k.nseen; i++)
      CHECK(k.seen[i] == (int)(i % 8), "delayed release: frame order");
   k.held = -1;
   video_player_slot_released(player, 0);
   video_player_slot_released(player, 1);
   while (!video_player_stop(player)) retro_sleep(1);
   video_player_free(player);
   video_player_worker_deinit();
}
#endif

int main(void)
{
#ifdef VP_TEST_CALLOC_FAULTS
   retro_atomic_int_init(&alloc_count, 0);
   retro_atomic_int_init(&alloc_fail, 0);
   retro_atomic_int_init(&alloc_enabled, 0);
#ifdef HAVE_THREADS
   retro_atomic_int_init(&fail_worker, 0);
   lane_hw_worker_failure();
#endif
   lane_hw_alloc_failures();
#endif
   if (getenv("VP_TEST_RECOVERY_ONLY"))
      goto recovery_only;
   lane_nonmovie_hw_sink();
#ifdef HAVE_THREADS
   lane_hw_startup_seek(false);
   lane_hw_startup_seek(true);
   lane_hw_startup_queued_cancel();
   lane_hw_startup(false,false,true);
   lane_hw_startup(true,false,true);
   lane_hw_startup(false,false,false);
   lane_hw_startup(true,false,false);
   lane_hw_startup(false,true,true);
   lane_hw_startup(true,true,true);
#endif
   lane_hw_fallback_stop(false,false);
   lane_hw_fallback_stop(false,true);
   lane_hw_fallback(false,false,false,false);
   lane_hw_fallback(false,true,false,false);
   lane_hw_fallback(false,false,true,false);
   lane_hw_fallback(false,false,false,true);
#ifdef HAVE_THREADS
   lane_hw_fallback_stop(true,false);
   lane_hw_fallback_stop(true,true);
   lane_hw_fallback(true,false,false,false);
   lane_hw_fallback(true,true,false,false);
   lane_hw_fallback(true,false,true,false);
   lane_hw_fallback(true,false,false,true);
#endif
   lane_movie_stop(false);
   lane_movie(false, false, false);
   lane_movie(false, true, true);
#ifdef HAVE_THREADS
   lane_movie_stop(true);
   lane_movie(true, false, true);
   lane_movie(true, true, false);
#endif
   lane_input_wait(false, false);
   lane_input_wait(false, true);
   lane_seek(false, false);
   lane_seek(false, true);
   lane_seek_stop(false);
   lane_seek_progress(false);
   lane_seek_peer(false, false);
   lane_seek_peer(false, true);
   lane_pause(false);
#ifdef HAVE_THREADS
   lane_input_wait(true, true);
   lane_seek(true, false);
   lane_seek(true, true);
   lane_seek_stop(true);
   lane_seek_progress(true);
   lane_seek_peer(true, false);
   lane_seek_peer(true, true);
   lane_pause(true);
#endif
   lane_cadence_loops(false);
   lane_timestamp_precision(false);
   lane_cadence(false, 0, false);
   lane_cadence(false, 1, false);
   lane_cadence(false, 2, false);
   lane_cadence(false, 0, true);
#ifdef HAVE_THREADS
   lane_cadence_loops(true);
   lane_timestamp_precision(true);
   lane_cadence(true, 0, false);
   lane_cadence(true, 1, false);
   lane_cadence(true, 2, false);
   lane_cadence(true, 0, true);
#endif
   lane_inline_release();
   lane_padded();
   lane_inline_pacing(false);
   lane_inline_pacing(true);
   lane_catchup();
   lane_order_and_default();
   lane_fp16();
   lane_hw_inline();
#ifdef HAVE_THREADS
   lane_delayed_slot_releases();
   lane_hw_threaded();
   lane_threaded(true);
   lane_threaded(false);
   lane_churn();
#endif
recovery_only:
   lane_empty_movie_deadline(false, false);
   lane_empty_movie_deadline(false, true);
#ifdef HAVE_THREADS
   lane_empty_movie_deadline(true, false);
   lane_empty_movie_deadline(true, true);
   lane_hw_failure_deadline(true);
   lane_hw_failure_deadline(false);
#endif
   lane_hw_start_result(true);
   lane_hw_start_result(false);
   lane_hw_recovery_state(false, true, false);
   lane_hw_recovery_state(false, false, true);
   lane_hw_recovery_state(false, true, true);
#ifdef HAVE_THREADS
   lane_hw_recovery_state(true, true, false);
   lane_hw_recovery_state(true, false, true);
   lane_hw_recovery_state(true, true, true);
#endif
   video_player_worker_deinit();
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("video_player: all tests passed\n");
   return 0;
}
