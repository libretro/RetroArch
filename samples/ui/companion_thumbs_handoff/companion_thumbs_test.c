/* ui/companion/companion_thumbs.c - the companion UI's thumbnail cache,
 * its decode workers and its animation thread - against fake decoders:
 * image_texture_load_ex() paints a still in a colour derived from its
 * path, and a fake gfx_anim_preview plays frames whose colour says which
 * animation and which frame they are. Nothing here touches a file.
 *
 * What is asserted:
 *  - every requested still is delivered once, in the colour of its own
 *    path, across a cancel() in the middle of the queue;
 *  - an animation's frames arrive in order, all of them its own: after
 *    animate() of another file, once that file's first frame has been
 *    delivered no frame of the old one follows; after animate_stop()
 *    returns, no frame of anything is delivered;
 *  - the preview audio is started once per animation and is never fed
 *    or stopped on a session that has been closed (a closed fake
 *    session is poisoned and every audio call checks it);
 *  - a still of an animated file hands its session to the animation of
 *    that file: one open for both;
 *  - with one worker held, urgent requests start newest first and
 *    prefetch ones after them, oldest first;
 *  - an entry a decode still in flight points at outlives an eviction
 *    of its cached pixels (the duplicate decoded after a cancel);
 *  - an animation asked for twice, or again after another one, still
 *    plays on from its still's session;
 *  - an urgent request left under a thousand newer ones, each taken
 *    ahead of it, is still decoded;
 *  - free() with workers decoding and an animation playing returns.
 * Under ASan (leaks) and TSan; a watchdog fails a hang. */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef _WIN32
#include <retro_timers.h>
#endif

#include <boolean.h>
#include <retro_atomic.h>
#include <formats/image.h>
#include <features/features_cpu.h>
#include <rthreads/rthreads.h>

#include "../../../gfx/gfx_anim_preview.h"
#include "../../../ui/companion/companion_thumbs.h"

static unsigned failures;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL line %d: ", __LINE__); \
   printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

#define DIMS VIDEO_SCALE_PACK(16, 12)

/* ---- fake still decoder ---- */

static uint32_t path_colour(const char *path)
{
   uint32_t h = 2166136261u;
   for (; *path; path++)
      h = (h ^ (unsigned char)*path) * 16777619u;
   return 0xFF000000u | (h & 0x00FFFFFFu);
}

static retro_atomic_int_t still_decodes;

enum image_type_enum image_texture_get_type(const char *path)
{
   size_t n = strlen(path);
   if (n > 5 && !strcmp(path + n - 5, ".webm"))
      return IMAGE_TYPE_WEBM;
   return IMAGE_TYPE_PNG;
}

/* A worker that decoded hands back its read pool before it ends:
 * noted by thread, against the threads that decoded */
#define MAX_THREADS 16
static uintptr_t decode_thread[MAX_THREADS], flush_thread[MAX_THREADS];
static unsigned  decode_threads, flush_threads;
static slock_t  *thread_lock;
static void note_thread(uintptr_t *list, unsigned *n)
{
   uintptr_t id = sthread_get_current_thread_id();
   unsigned i;
   slock_lock(thread_lock);
   for (i = 0; i < *n; i++)
      if (list[i] == id)
         break;
   if (i == *n && *n < MAX_THREADS)
      list[(*n)++] = id;
   slock_unlock(thread_lock);
}
void data_transfer_pool_flush(void)
{
   note_thread(flush_thread, &flush_threads);
}

/* "hold*": waits for hold_release.  "slow*": the first one waits for
 * slow_release.  "ord*": noted in the order decodes start.  "stepN":
 * waits for step_at to pass N.  "under*": counted. */
static retro_atomic_int_t hold_release, hold_inside;
static retro_atomic_int_t step_at, under_decodes;
static retro_atomic_int_t slow_release, slow_entered;
static char     ord_seen[32][16];
static unsigned ord_n;

static void wait_flag(retro_atomic_int_t *flag)
{
   retro_time_t end = cpu_features_get_time_usec() + 10000000;
   while (!retro_atomic_load_acquire_int(flag)
         && cpu_features_get_time_usec() < end)
      usleep(200);
}

bool image_texture_load_ex(struct texture_image *img, const char *path,
      bool (*should_abort)(void *ud), void *ud)
{
   unsigned i;
   uint32_t c = path_colour(path);
   if (!strncmp(path, "hold", 4))
   {
      retro_atomic_fetch_add_int(&hold_inside, 1);
      wait_flag(&hold_release);
      should_abort = NULL;
   }
   else if (!strncmp(path, "slow", 4))
   {
      if (!retro_atomic_fetch_add_int(&slow_entered, 1))
         wait_flag(&slow_release);
      should_abort = NULL;
   }
   else if (!strncmp(path, "step", 4))
   {
      int n = atoi(path + 4);
      retro_time_t end = cpu_features_get_time_usec() + 10000000;
      while (retro_atomic_load_acquire_int(&step_at) <= n
            && cpu_features_get_time_usec() < end)
         usleep(20);
      should_abort = NULL;
   }
   else if (!strncmp(path, "under", 5))
      retro_atomic_fetch_add_int(&under_decodes, 1);
   else if (!strncmp(path, "ord", 3))
   {
      slock_lock(thread_lock);
      if (ord_n < 32)
         strncpy(ord_seen[ord_n++], path, 15);
      slock_unlock(thread_lock);
   }
   for (i = 0; i < 3; i++)
   {
      if (should_abort && should_abort(ud))
         return false;
      usleep(300);
   }
   img->width  = 40;
   img->height = 30;
   img->pixels = (uint32_t*)malloc(40 * 30 * sizeof(uint32_t));
   for (i = 0; i < 40 * 30; i++)
      img->pixels[i] = c;
   retro_atomic_fetch_add_int(&still_decodes, 1);
   note_thread(decode_thread, &decode_threads);
   return true;
}

void image_texture_free(struct texture_image *img)
{
   free(img->pixels);
   img->pixels = NULL;
}

/* ---- fake animation sessions ---- */

#define SESS_LIVE   0x5E55104Eu
#define SESS_CLOSED 0xDEADDEADu
#define FRAMES      7

typedef struct fake_sess
{
   gfx_anim_preview_t p;            /* first: what the library sees */
   retro_atomic_int_t magic;
   unsigned id;                     /* animN.webm */
   unsigned frame;
   uint32_t canvas[32 * 24];
} fake_sess_t;

static retro_atomic_int_t opens[8];
static retro_atomic_int_t audio_begins[8];
static retro_atomic_int_t audio_bad;

static unsigned anim_id(const char *path)
{
   const char *a = strstr(path, "anim");
   return a ? (unsigned)(a[4] - '0') & 7 : 0;
}

int gfx_anim_preview_probe(const char *path)
{
   return image_texture_get_type(path) == IMAGE_TYPE_WEBM;
}

/* Set: opens of the gated animation wait until it is cleared, so the
 * test decides what has happened before its still is decoded. */
static retro_atomic_int_t open_gate;
#define GATED_ANIM 5

gfx_anim_preview_t *gfx_anim_preview_open(const char *path, int png_probe)
{
   fake_sess_t *s;
   (void)png_probe;
   if (image_texture_get_type(path) != IMAGE_TYPE_WEBM)
      return NULL;
   if (anim_id(path) == GATED_ANIM)
      while (retro_atomic_load_acquire_int(&open_gate))
         usleep(500);
   s = (fake_sess_t*)calloc(1, sizeof(*s));
   s->p.dims       = VIDEO_SCALE_PACK(32, 24);
   s->p.loop_count = 0;
   s->p.stream     = s;
   s->p.type       = IMAGE_TYPE_WEBM;
   s->id           = anim_id(path);
   retro_atomic_store_release_int(&s->magic, (int)SESS_LIVE);
   retro_atomic_fetch_add_int(&opens[s->id], 1);
   return &s->p;
}

bool gfx_anim_preview_feed(gfx_anim_preview_t *p) { (void)p; return true; }

const uint32_t *gfx_anim_preview_next(gfx_anim_preview_t *p,
      int *duration_ms, bool *native_argb)
{
   fake_sess_t *s = (fake_sess_t*)p;
   unsigned i;
   uint32_t c;
   if (s->frame >= FRAMES)
      return NULL;
   c = 0xFF000000u | (s->id << 16) | s->frame;
   for (i = 0; i < 32 * 24; i++)
      s->canvas[i] = c;
   s->frame++;
   *duration_ms = 4;
   *native_argb = true;
   return s->canvas;
}

void gfx_anim_preview_rewind(gfx_anim_preview_t *p)
{
   ((fake_sess_t*)p)->frame = 0;
}

/* The stream behind the session, as video_player plays it: the fake
 * session is its own stream. */
const uint32_t *image_transfer_anim_stream_next(void *stream,
      enum image_type_enum type, int *duration_ms)
{
   bool native;
   (void)type;
   return gfx_anim_preview_next((gfx_anim_preview_t*)stream, duration_ms,
         &native);
}
bool image_transfer_anim_stream_completed(const void *stream,
      enum image_type_enum type)
{
   const fake_sess_t *s = (const fake_sess_t*)stream;
   return type == IMAGE_TYPE_WEBM && s->frame >= FRAMES;
}

bool image_transfer_anim_stream_waiting(const void *stream,
      enum image_type_enum type)
{
   (void)stream; (void)type;
   return false;
}
int64_t image_transfer_anim_stream_duration_ns(void *stream,
      enum image_type_enum type)
{
   (void)stream; (void)type;
   return 0;
}
int64_t image_transfer_anim_stream_frame_time_ns(void *stream,
      enum image_type_enum type)
{
   (void)stream; (void)type;
   return -1;
}
void image_transfer_anim_stream_rewind(void *stream,
      enum image_type_enum type)
{
   (void)type;
   gfx_anim_preview_rewind((gfx_anim_preview_t*)stream);
}
void image_transfer_anim_stream_get_info(void *stream,
      enum image_type_enum type, unsigned *width, unsigned *height,
      int *num_frames, int *loop_count)
{
   (void)stream;
   (void)type;
   *width      = 32;
   *height     = 24;
   *num_frames = FRAMES;
   *loop_count = 0;
}
bool image_transfer_anim_stream_set_argb(void *stream,
      enum image_type_enum type, int argb)
{
   (void)stream; (void)type;
   return argb != 0;
}
bool image_transfer_anim_stream_set_output_stride(void *stream,
      enum image_type_enum type, uint32_t *out, unsigned stride)
{
   (void)stride;
   (void)stream; (void)type; (void)out;
   return false;
}
void image_transfer_anim_stream_set_catchup(void *stream,
      enum image_type_enum type, int on)
{
   (void)stream; (void)type; (void)on;
}
void image_transfer_anim_stream_set_want_fp16(void *stream,
      enum image_type_enum type, bool want)
{
   (void)stream; (void)type; (void)want;
}
bool image_transfer_anim_stream_is_fp16(const void *stream,
      enum image_type_enum type)
{
   (void)stream; (void)type;
   return false;
}
bool image_transfer_anim_stream_set_blit_pool(void *stream,
      enum image_type_enum type, void *pool, unsigned bands)
{
   (void)stream; (void)type; (void)pool; (void)bands;
   return false;
}
bool image_transfer_anim_stream_set_hw(void *stream,
      enum image_type_enum type, bool enable)
{
   (void)stream; (void)type; (void)enable;
   return false;
}
bool image_transfer_anim_stream_hw_frame(void *stream,
      enum image_type_enum type, video_hw_frame_t *out)
{
   (void)stream; (void)type; (void)out;
   return false;
}

/* Live on the way in and still live on the way out: a close while the
 * call is inside it counts too. */
static void audio_check(gfx_anim_preview_t *p)
{
   retro_atomic_int_t *magic = &((fake_sess_t*)p)->magic;
   if ((unsigned)retro_atomic_load_acquire_int(magic) != SESS_LIVE)
      retro_atomic_fetch_add_int(&audio_bad, 1);
   usleep(100);
   if ((unsigned)retro_atomic_load_acquire_int(magic) != SESS_LIVE)
      retro_atomic_fetch_add_int(&audio_bad, 1);
}

void gfx_anim_preview_audio_begin(gfx_anim_preview_t *p)
{
   audio_check(p);
   retro_atomic_fetch_add_int(&audio_begins[((fake_sess_t*)p)->id], 1);
}
bool gfx_anim_preview_audio_feed(gfx_anim_preview_t *p) { audio_check(p); return true; }
void gfx_anim_preview_audio_stop(gfx_anim_preview_t *p) { audio_check(p); }

void gfx_anim_preview_close(gfx_anim_preview_t *p)
{
   fake_sess_t *s = (fake_sess_t*)p;
   if (!s)
      return;
   /* poison, then give the audio checks a window to see it */
   retro_atomic_store_release_int(&s->magic, (int)SESS_CLOSED);
   usleep(200);
   free(s);
}

/* ---- delivery ---- */

typedef struct
{
   unsigned stills;
   unsigned still_wrong;
   unsigned anim_frames[8];
   int      last_frame[8];
   unsigned out_of_order;
   unsigned seen_order;      /* animations in the order frames came */
   int      cur_anim;        /* -1: none expected */
   unsigned stale;           /* a frame of an animation not current */
   bool     stopped;
   unsigned after_stop;
} sink_t;

static void done_cb(void *ud, const char *path, unsigned dims,
      uintptr_t tag, const uint32_t *bits)
{
   sink_t *k = (sink_t*)ud;
   (void)dims;
   if (tag >= 100000)
   {
      /* an animation frame: tag = 100000 + id */
      unsigned id = (unsigned)(tag - 100000) & 7;
      int frame;
      if (!bits)
         return;
      if (k->stopped)
         k->after_stop++;
      if (k->cur_anim >= 0 && (int)id != k->cur_anim)
         k->stale++;
      frame = (int)(bits[0] & 0xFFFF);
      CHECK(((bits[0] >> 16) & 0xFF) == id, "a frame of animation %u on %s",
            (unsigned)((bits[0] >> 16) & 0xFF), path);
      if (k->last_frame[id] >= 0 && frame != (k->last_frame[id] + 1) % FRAMES
            && frame <= k->last_frame[id])
         ; /* frames may be skipped, never replayed out of turn */
      if (k->last_frame[id] >= 0 && frame == k->last_frame[id])
         k->out_of_order++;
      k->last_frame[id] = frame;
      k->anim_frames[id]++;
      return;
   }
   k->stills++;
   if (tag == 900)
   {
      /* the still of an animation is its first frame */
      if (!bits || bits[0] != (0xFF000000u | (anim_id(path) << 16)))
         k->still_wrong++;
      return;
   }
   if (!bits || bits[0] != path_colour(path))
      k->still_wrong++;
}

static void pump(companion_thumbs_t *t, sink_t *k, unsigned ms)
{
   retro_time_t end = cpu_features_get_time_usec() + (retro_time_t)ms * 1000;
   while (cpu_features_get_time_usec() < end)
   {
      companion_thumbs_poll(t, done_cb, k, 0, 4000);
      usleep(1000);
   }
}

static void watchdog(int sig)
{
   static const char m[] = "FAIL: hung (watchdog)\n";
   (void)sig;
   if (write(1, m, sizeof(m) - 1) < 0) { }
   _exit(1);
}

#ifdef _WIN32
static void watchdog_thread(void *unused)
{
   (void)unused;
   retro_sleep(120000);
   watchdog(0);
}
#endif

int main(void)
{
   companion_thumbs_t *t;
   sink_t k;
   char path[64];
   unsigned i, round;
#ifdef _WIN32
   sthread_t *timer = sthread_create(watchdog_thread, NULL);
   if (!timer)
      return 1;
   sthread_detach(timer);
#else
   signal(SIGALRM, watchdog);
   alarm(120);
#endif
   thread_lock = slock_new();
   memset(&k, 0, sizeof(k));
   for (i = 0; i < 8; i++)
      k.last_frame[i] = -1;
   k.cur_anim = -1;

   t = companion_thumbs_new(0, 4);
   CHECK(t != NULL, "no instance");
   if (!t)
      return 1;

   /* stills: 200 requested, a cancel half way, the rest re-requested */
   for (i = 0; i < 200; i++)
   {
      snprintf(path, sizeof(path), "still%03u.png", i);
      companion_thumbs_request(t, path, DIMS, i, (i & 1) != 0, 0);
      if (i == 100)
      {
         pump(t, &k, 5);
         companion_thumbs_cancel(t);
      }
   }
   for (round = 0; round < 3; round++)
   {
      for (i = 0; i < 200; i++)
      {
         snprintf(path, sizeof(path), "still%03u.png", i);
         if (!companion_thumbs_get(t, path, DIMS))
            companion_thumbs_request(t, path, DIMS, i, false, 0);
      }
      while (companion_thumbs_pending(t))
         pump(t, &k, 5);
   }
   for (i = 0; i < 200; i++)
   {
      const uint32_t *b;
      snprintf(path, sizeof(path), "still%03u.png", i);
      b = companion_thumbs_get(t, path, DIMS);
      CHECK(b && b[0] == path_colour(path), "%s not cached right", path);
   }
   CHECK(!k.still_wrong, "%u stills delivered in another path's colour",
         k.still_wrong);
   printf("ok    200 stills through 4 workers and a cancel\n");

   /* animations: one after another, each frame its own */
   for (round = 0; round < 6; round++)
   {
      unsigned id = 1 + round % 3;
      unsigned before;
      snprintf(path, sizeof(path), "anim%u.webm", id);
      companion_thumbs_animate(t, path, DIMS, 100000 + id, 0);
      /* frames of the old one may still be on their way until the new
       * one's first frame has arrived */
      k.cur_anim = -1;
      k.last_frame[id] = -1;  /* a new animation starts from its start */
      before     = k.anim_frames[id];
      {
         retro_time_t end = cpu_features_get_time_usec() + 2000000;
         while (k.anim_frames[id] == before
               && cpu_features_get_time_usec() < end)
         {
            companion_thumbs_poll(t, done_cb, &k, 0, 4000);
            usleep(500);
         }
      }
      CHECK(k.anim_frames[id] > before, "animation %u never delivered", id);
      k.cur_anim = (int)id;
      pump(t, &k, 60);
      CHECK(companion_thumbs_animating(t), "not animating while playing");
   }
   CHECK(!k.stale, "%u frames of an older animation after the new one's",
         k.stale);
   CHECK(!k.out_of_order, "%u frames repeated out of turn", k.out_of_order);
   companion_thumbs_animate_stop(t);
   k.stopped = true;
   pump(t, &k, 60);
   CHECK(!k.after_stop, "%u frames delivered after animate_stop()",
         k.after_stop);
   for (i = 1; i <= 3; i++)
      CHECK(retro_atomic_load_acquire_int(&audio_begins[i]) >= 1,
            "audio never started for animation %u", i);
   printf("ok    six animations in turn, then a stop\n");

   /* a still of an animated file hands its session to the animation */
   {
      int before = retro_atomic_load_acquire_int(&opens[5]);
      k.stopped  = false;
      /* the animation is asked for before the still's decode gets far */
      retro_atomic_store_release_int(&open_gate, 1);
      companion_thumbs_request(t, "anim5.webm", DIMS, 900, true, 0);
      companion_thumbs_animate(t, "anim5.webm", DIMS, 100005, 0);
      retro_atomic_store_release_int(&open_gate, 0);
      k.cur_anim = -1;
      pump(t, &k, 150);
      CHECK(k.anim_frames[5] > 0, "the parked animation never played");
      CHECK(!k.still_wrong, "the still of an animation is not its first frame");
      CHECK(retro_atomic_load_acquire_int(&opens[5]) - before == 1,
            "a still and its animation opened %d sessions, not 1",
            retro_atomic_load_acquire_int(&opens[5]) - before);
   }
   printf("ok    a still and its animation share one session\n");

   /* switch animations as fast as the UI can, polling in between */
   for (round = 0; round < 300; round++)
   {
      snprintf(path, sizeof(path), "anim%u.webm", 1 + round % 4);
      if (round % 7 == 6)
         companion_thumbs_animate_stop(t);
      else
         companion_thumbs_animate(t, path, DIMS, 100000 + 1 + round % 4, 0);
      k.cur_anim = -1;
      companion_thumbs_poll(t, done_cb, &k, 0, 4000);
   }
   pump(t, &k, 50);
   CHECK(!retro_atomic_load_acquire_int(&audio_bad),
         "%d audio calls on a closed session",
         retro_atomic_load_acquire_int(&audio_bad));
   printf("ok    300 animation switches: audio only on live sessions\n");

   /* more requests than a ring holds: the oldest are dropped, and once
    * the rest are done nothing is left pending */
   {
      retro_time_t end;
      companion_thumbs_cancel(t);
      for (i = 0; i < 1500; i++)
      {
         snprintf(path, sizeof(path), "many%04u.png", i);
         companion_thumbs_request(t, path, DIMS, i, false, 0);
      }
      end = cpu_features_get_time_usec() + 20000000;
      while (companion_thumbs_pending(t) > 1
            && cpu_features_get_time_usec() < end)
         pump(t, &k, 5);
      CHECK(companion_thumbs_pending(t) <= 1,
            "%u still pending after an overfull queue drained",
            (unsigned)companion_thumbs_pending(t));
      CHECK(companion_thumbs_queued(t) == 0, "%u queued with the rings empty",
            (unsigned)companion_thumbs_queued(t));
   }
   printf("ok    an overfull queue drains to nothing pending\n");

   /* one worker, held: urgent newest first, then prefetch oldest first */
   {
      companion_thumbs_t *one = companion_thumbs_new(0, 1);
      static const char *want[] = { "ord_u2", "ord_u1", "ord_u0",
         "ord_p0", "ord_p1", "ord_p2", "ord_p3" };
      unsigned bad = 7;
      CHECK(one != NULL, "no single-worker instance");
      retro_atomic_store_release_int(&hold_release, 0);
      retro_atomic_store_release_int(&hold_inside, 0);
      companion_thumbs_request(one, "hold0.png", DIMS, 1, true, 0);
      wait_flag(&hold_inside);
      for (i = 0; i < 4; i++)
      {
         snprintf(path, sizeof(path), "ord_p%u", i);
         companion_thumbs_request(one, path, DIMS, 10 + i, false, 0);
      }
      for (i = 0; i < 3; i++)
      {
         snprintf(path, sizeof(path), "ord_u%u", i);
         companion_thumbs_request(one, path, DIMS, 20 + i, true, 0);
      }
      retro_atomic_store_release_int(&hold_release, 1);
      while (companion_thumbs_pending(one))
         pump(one, &k, 5);
      CHECK(ord_n == 7, "%u of 7 ordered decodes ran", ord_n);
      for (i = 0; i < 7 && i < ord_n && bad == 7; i++)
         if (strcmp(ord_seen[i], want[i]))
            bad = i;
      CHECK(bad == 7, "decode %u started was %s, not %s", bad,
            bad < ord_n ? ord_seen[bad] : "-", bad < 7 ? want[bad] : "-");
      companion_thumbs_free(one);
   }
   printf("ok    urgent newest first, then prefetch oldest first\n");

   /* the cached copy of a key evicted while its first decode, from
    * before a cancel, is still running: the entry stays for it */
   {
      companion_thumbs_t *ev = companion_thumbs_new(0, 2);
      retro_time_t end;
      CHECK(ev != NULL, "no eviction instance");
      companion_thumbs_request(ev, "slow0.png", DIMS, 1, true, 0);
      end = cpu_features_get_time_usec() + 5000000;
      while (!retro_atomic_load_acquire_int(&slow_entered)
            && cpu_features_get_time_usec() < end)
         usleep(200);
      companion_thumbs_cancel(ev);
      companion_thumbs_request(ev, "slow0.png", DIMS, 2, true, 0);
      end = cpu_features_get_time_usec() + 5000000;
      while (!companion_thumbs_get(ev, "slow0.png", DIMS)
            && cpu_features_get_time_usec() < end)
         pump(ev, &k, 2);
      CHECK(companion_thumbs_get(ev, "slow0.png", DIMS) != NULL,
            "the second decode was never cached");
      /* a budget of one byte evicts everything evictable */
      companion_thumbs_set_budget(ev, 1);
      retro_atomic_store_release_int(&slow_release, 1);
      while (companion_thumbs_pending(ev))
         pump(ev, &k, 2);
      companion_thumbs_free(ev);
   }
   printf("ok    an entry outlives an eviction while a decode holds it\n");

   /* an animation asked for twice, then again after another while it
    * waits: each time it plays on from its still's session */
   {
      unsigned variant;
      for (variant = 0; variant < 3; variant++)
      {
         companion_thumbs_t *an = companion_thumbs_new(0, 1);
         int before = retro_atomic_load_acquire_int(&opens[5]);
         unsigned frames = k.anim_frames[5];
         CHECK(an != NULL, "no animation instance");
         retro_atomic_store_release_int(&hold_release, 0);
         retro_atomic_store_release_int(&hold_inside, 0);
         companion_thumbs_request(an, "hold1.png", DIMS, 1, true, 0);
         wait_flag(&hold_inside);
         companion_thumbs_request(an, "anim5.webm", DIMS, 900, true, 0);
         companion_thumbs_animate(an, "anim5.webm", DIMS, 100005, 0);
         if (variant == 1)
            companion_thumbs_animate(an, "anim5.webm", DIMS, 100005, 0);
         else if (variant == 2)
         {
            usleep(20000);
            companion_thumbs_animate(an, "anim6.webm", DIMS, 100006, 0);
            companion_thumbs_animate(an, "anim5.webm", DIMS, 100005, 0);
         }
         retro_atomic_store_release_int(&hold_release, 1);
         k.cur_anim = -1;
         k.last_frame[5] = -1;
         pump(an, &k, 300);
         CHECK(k.anim_frames[5] > frames,
               "animation asked for again (case %u) never played", variant);
         CHECK(retro_atomic_load_acquire_int(&opens[5]) - before == 1,
               "case %u: a still and its animation opened %d sessions",
               variant, retro_atomic_load_acquire_int(&opens[5]) - before);
         companion_thumbs_free(an);
      }
   }
   printf("ok    an animation asked for again plays from its still\n");

   /* one urgent request under a thousand newer ones, each taken ahead
    * of it while the one worker is busy with the last */
   {
      companion_thumbs_t *lane = companion_thumbs_new(0, 1);
      retro_time_t end;
      CHECK(lane != NULL, "no lane instance");
      retro_atomic_store_release_int(&step_at, 0);
      companion_thumbs_request(lane, "step0.png", DIMS, 0, true, 0);
      end = cpu_features_get_time_usec() + 5000000;
      while (companion_thumbs_queued(lane)
            && cpu_features_get_time_usec() < end)
         usleep(20);
      companion_thumbs_request(lane, "under.png", DIMS, 1, true, 0);
      for (i = 1; i < 1100; i++)
      {
         snprintf(path, sizeof(path), "step%u.png", i);
         companion_thumbs_request(lane, path, DIMS, i, true, 0);
         retro_atomic_store_release_int(&step_at, (int)i);
         /* until the worker has taken it: only the old one is queued */
         end = cpu_features_get_time_usec() + 5000000;
         while (companion_thumbs_queued(lane) > 1
               && cpu_features_get_time_usec() < end)
            usleep(20);
         companion_thumbs_poll(lane, done_cb, &k, 0, 0);
      }
      retro_atomic_store_release_int(&step_at, 1 << 30);
      end = cpu_features_get_time_usec() + 10000000;
      while (companion_thumbs_pending(lane)
            && cpu_features_get_time_usec() < end)
         pump(lane, &k, 2);
      CHECK(retro_atomic_load_acquire_int(&under_decodes) == 1,
            "the request under the others was decoded %d times, not once",
            retro_atomic_load_acquire_int(&under_decodes));
      companion_thumbs_free(lane);
   }
   printf("ok    a request under a thousand newer ones is still decoded\n");

   /* free with work in flight */
   for (i = 0; i < 100; i++)
   {
      snprintf(path, sizeof(path), "late%03u.png", i);
      companion_thumbs_request(t, path, DIMS, i, true, 0);
   }
   companion_thumbs_animate(t, "anim2.webm", DIMS, 100002, 0);
   usleep(5000);
   companion_thumbs_free(t);
   printf("ok    free with decodes and an animation in flight\n");
   {
      unsigned i2, j, missing = 0;
      for (i2 = 0; i2 < decode_threads; i2++)
      {
         bool found = false;
         for (j = 0; j < flush_threads; j++)
            if (flush_thread[j] == decode_thread[i2])
               found = true;
         if (!found)
            missing++;
      }
      CHECK(decode_threads > 0 && !missing,
            "a worker that decoded ended holding its read pool");
      if (decode_threads > 0 && !missing)
         printf("ok    %u decode workers handed back their read pools\n",
               decode_threads);
   }
   slock_free(thread_lock);

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] companion_thumbs_test\n");
   return 0;
}
