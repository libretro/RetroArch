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
 * keeping the order while players are freed with decodes in flight. */

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
   int frames;
   int pos;
   int duration_ms;
   int catchup;        /* last value set */
   int catchup_sets;
   int argb;
   bool direct;        /* decodes into the output when given one */
   bool honours_order;
   bool fp16;
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
   if (f->pos >= f->frames)
      return NULL;
   dst = (f->direct && f->out) ? f->out : f->canvas;
   for (i = 0; i < N; i++)
      dst[i] = frame_px(f->pos,
            f->honours_order ? f->argb : 0);
   f->pos++;
   *duration_ms = f->duration_ms;
   return dst;
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

bool image_transfer_anim_stream_set_output(void *stream,
      enum image_type_enum type, uint32_t *out)
{
   fake_t *f = (fake_t*)stream;
   (void)type;
   f->out = out;
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
   uint32_t slots[2][N];
   int seen[MAX_SEEN];
   int nseen;
   int bad_px;
   int begun[2];         /* slot_begin minus slot_end, per slot */
   int held;             /* slot of a QUEUED present, -1 none */
   int queue_every;      /* present QUEUED every n-th frame, 0 never */
   int start_flags;
   bool slots_on;
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
   idx = (int)(px[0] & 0xFF) - 1;
   if (!fr->native)
      idx = (int)((px[0] >> 16) & 0xFF) - 1;
   want_argb = frame_px(idx, true);
   for (i = 0; i < N; i++)
   {
      uint32_t p = px[i];
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

#ifdef HAVE_THREADS
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

int main(void)
{
   lane_inline_pacing(false);
   lane_inline_pacing(true);
   lane_catchup();
   lane_order_and_default();
   lane_fp16();
#ifdef HAVE_THREADS
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
