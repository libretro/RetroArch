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
#include <formats/image.h>
#include <formats/video_player.h>

#define W 4
#define H 2
#define N (W * H)

static int failures = 0;

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
} fake_t;

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
   if (f->pos >= f->frames)
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
   int hw_started;
   bool hw_twice;        /* the same frame offered after it was taken */
} sink_t;

static int sink_start(void *user, unsigned width, unsigned height,
      unsigned num_slots)
{
   sink_t *k = (sink_t*)user;
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
   return 0;
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
   f->frames        = frames;
   f->duration_ms   = duration_ms;
   f->honours_order = true;
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
   now   = 0;
   polls = 0;
   do
   {
      st  = video_player_poll(p, now);
      now = video_player_due(p) > now ? video_player_due(p) : now + 1000;
   } while (st == VIDEO_PLAYER_PLAYING && ++polls < 50);
   CHECK(st == VIDEO_PLAYER_ENDED && k.nseen == 2,
         "hw inline: %d frames before the failing one", k.nseen);
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

int main(void)
{
   lane_inline_release();
   lane_padded();
   lane_inline_pacing(false);
   lane_inline_pacing(true);
   lane_catchup();
   lane_order_and_default();
   lane_fp16();
   lane_hw_inline();
#ifdef HAVE_THREADS
   lane_hw_threaded();
   lane_threaded(true);
   lane_threaded(false);
   lane_churn();
#endif
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("video_player: all tests passed\n");
   return 0;
}
