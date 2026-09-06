/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <compat/posix_string.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <formats/image.h>
#include <streams/file_stream.h>
#include <time.h>          /* struct timespec, for retro_timers.h */
#include <retro_timers.h>
#ifdef HAVE_RPNG
#include <formats/rpng.h>
#endif
#include <features/features_cpu.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "companion_thumbs.h"

/* --- cache entry ------------------------------------------------------- */

struct ct_entry
{
   char *path;
   uint32_t *bits;        /* NULL while queued / decoding */
   int w, h;
   size_t bytes;
   /* LRU list of cached entries (bits != NULL); most recent at head. */
   struct ct_entry *lru_prev, *lru_next;
   /* hash chain */
   struct ct_entry *chain;
   bool queued;           /* in a request queue or being decoded */
   /* Jobs (queued, decoding, or finished but not yet polled) that point
    * at this entry. An entry is only ever freed at zero: two records
    * for one key exist when a key is re-requested while its earlier
    * decode is still in flight across a cancel(), or when a failed
    * decode's record is polled with another still pending. */
   unsigned refs;
};

/* --- request / result queues ------------------------------------------- */

struct ct_job
{
   struct ct_entry *e;
   uintptr_t tag;
   uint32_t bg;
   unsigned epoch;
};

struct ct_done
{
   struct ct_entry *e;    /* NULL for an animation frame */
   uintptr_t tag;
   uint32_t *bits;        /* NULL: decode failed (or aborted) */
   unsigned epoch;
   bool aborted;          /* abandoned: not delivered, entry dropped */
   /* animation frame: delivered when anim_gen is current */
   bool anim;
   unsigned anim_gen;
   char *anim_path;
   int anim_w, anim_h;
};

/* A ring with both ends usable: urgent jobs are pushed to and popped
 * from the top (LIFO, most-recent-first); prefetch jobs are pushed to
 * the bottom and popped from the top only once the urgent ones are
 * gone, i.e. after them, oldest-first among themselves. */
struct ct_ring
{
   struct ct_job *v;
   size_t cap, head, len;  /* head = index of the bottom (oldest) */
};

struct companion_thumbs
{
   /* hash table of entries (both cached and queued), UI-thread owned
    * except that workers read e->path and write e->bits via the done
    * ring under the lock */
   struct ct_entry **ht;
   size_t ht_size, ht_count;

   struct ct_entry *lru_head, *lru_tail;
   size_t cached_bytes, cached_count, budget;

   struct ct_ring urgent, prefetch;
   size_t queued;         /* jobs in either ring */
   size_t inflight;       /* jobs a worker holds right now */
   unsigned epoch;        /* bumped by cancel(): an in-flight job from an
                           * older epoch no longer holds its entry's
                           * queued flag, so the key can be re-requested;
                           * a second decode of the same key is deduped
                           * at delivery */

   struct ct_done *done;
   size_t done_cap, done_len;


#ifdef HAVE_THREADS
   slock_t *lock;
   scond_t *cond;
   sthread_t *workers[4];
   unsigned nworkers;
   bool quit;
#endif

   /* The one animation: set up by companion_thumbs_animate() under the
    * lock, played by its own thread, which pushes each frame as a done
    * record (anim flag) and sleeps the frame's duration. gen changes
    * on every animate() / stop(): a frame from an older gen is dropped
    * at poll. */
   struct
   {
      char *path;
      int w, h;
      uintptr_t tag;
      uint32_t bg;
      unsigned gen;           /* the animation that should be playing */
      bool wanted;            /* an animate() is pending or playing */
   } anim;
#ifdef HAVE_THREADS
   sthread_t *anim_thread;
   scond_t   *anim_cond;
#endif
};

#ifdef HAVE_THREADS
#define CT_LOCK(t)   slock_lock((t)->lock)
#define CT_UNLOCK(t) slock_unlock((t)->lock)
#else
#define CT_LOCK(t)   ((void)0)
#define CT_UNLOCK(t) ((void)0)
#endif

/* --- scaling (pure) ---------------------------------------------------- */

uint32_t *companion_thumbs_scale(const uint32_t *src, unsigned sw,
      unsigned sh, int dw, int dh, uint32_t bg)
{
   uint32_t *buf;
   int fw, fh, ox, oy, x, y;

   if (!src || !sw || !sh || dw < 1 || dh < 1)
      return NULL;
   buf = (uint32_t*)malloc((size_t)dw * dh * sizeof(uint32_t));
   if (!buf)
      return NULL;

   /* Fit: the relatively larger dimension fills the box. */
   if ((uint64_t)sw * (unsigned)dh >= (uint64_t)sh * (unsigned)dw)
   {
      fw = dw;
      fh = (int)((uint64_t)dw * sh / sw);
   }
   else
   {
      fh = dh;
      fw = (int)((uint64_t)dh * sw / sh);
   }
   if (fw < 1) fw = 1;
   if (fh < 1) fh = 1;
   ox = (dw - fw) / 2;
   oy = (dh - fh) / 2;

   for (y = 0; y < dh; y++)
   {
      uint32_t *row = buf + (size_t)y * dw;
      if (y < oy || y >= oy + fh)
      {
         for (x = 0; x < dw; x++)
            row[x] = bg;
         continue;
      }
      {
         const uint32_t *s = src + (size_t)((y - oy) * sh / fh) * sw;
         for (x = 0; x < dw; x++)
         {
            if (x < ox || x >= ox + fw)
               row[x] = bg;
            else
            {
               /* Composite the source alpha over @bg so transparent
                * images sit on the view colour; the result is opaque. */
               uint32_t p = s[(x - ox) * sw / fw];
               unsigned a = (p >> 24) & 0xff;
               if (a == 0xff)
                  row[x] = p | 0xff000000u;
               else
               {
                  unsigned ia = 255 - a;
                  unsigned r  = (((p >> 16) & 0xff) * a + ((bg >> 16) & 0xff) * ia) / 255;
                  unsigned g  = (((p >>  8) & 0xff) * a + ((bg >>  8) & 0xff) * ia) / 255;
                  unsigned bb = (( p        & 0xff) * a + ( bg        & 0xff) * ia) / 255;
                  row[x] = 0xff000000u | (r << 16) | (g << 8) | bb;
               }
            }
         }
      }
   }
   return buf;
}

/* Decode @path and scale to @w x @h. Runs on a worker; @should_abort
 * (may be NULL) is asked between decode steps so a giant image can be
 * abandoned at shutdown or once nobody wants it. */
static uint32_t *ct_decode(const char *path, int w, int h, uint32_t bg,
      bool (*should_abort)(void *ud), void *ud)
{
   struct texture_image img;
   uint32_t *bits = NULL;
   memset(&img, 0, sizeof(img));
   if (image_texture_load_ex(&img, path, should_abort, ud))
   {
      if (img.pixels)
         bits = companion_thumbs_scale(img.pixels, img.width, img.height,
               w, h, bg);
      image_texture_free(&img);
   }
   return bits;
}

/* --- hash table ---------------------------------------------------------- */

static size_t ct_hash(const char *path, int w, int h)
{
   size_t k = 2166136261u;
   while (*path)
      k = (k ^ (unsigned char)*path++) * 16777619u;
   k = (k ^ (unsigned)w) * 16777619u;
   k = (k ^ (unsigned)h) * 16777619u;
   return k;
}

static struct ct_entry *ct_find(companion_thumbs_t *t, const char *path,
      int w, int h)
{
   struct ct_entry *e;
   if (!t->ht)
      return NULL;
   for (e = t->ht[ct_hash(path, w, h) & (t->ht_size - 1)]; e; e = e->chain)
      if (e->w == w && e->h == h && string_is_equal(e->path, path))
         return e;
   return NULL;
}

static bool ct_grow(companion_thumbs_t *t)
{
   size_t ns = t->ht_size ? t->ht_size * 2 : 1024, i;
   struct ct_entry **nh = (struct ct_entry**)calloc(ns, sizeof(*nh));
   if (!nh)
      return false;
   for (i = 0; i < t->ht_size; i++)
   {
      struct ct_entry *e = t->ht[i];
      while (e)
      {
         struct ct_entry *next = e->chain;
         size_t k = ct_hash(e->path, e->w, e->h) & (ns - 1);
         e->chain = nh[k];
         nh[k]    = e;
         e        = next;
      }
   }
   free(t->ht);
   t->ht      = nh;
   t->ht_size = ns;
   return true;
}

static struct ct_entry *ct_insert(companion_thumbs_t *t, const char *path,
      int w, int h)
{
   struct ct_entry *e;
   size_t k;
   if (t->ht_count * 2 >= t->ht_size && !ct_grow(t))
      return NULL;
   e = (struct ct_entry*)calloc(1, sizeof(*e));
   if (!e)
      return NULL;
   /* strldup(s, n) copies n - 1 characters (a buffer size, strlcpy
    * style): pass the length plus one. Declared for any -std, unlike
    * strdup under -ansi, where the implicit int would truncate the
    * pointer on 64-bit. */
   e->path = strldup(path, strlen(path) + 1);
   if (!e->path)
   {
      free(e);
      return NULL;
   }
   e->w     = w;
   e->h     = h;
   k        = ct_hash(path, w, h) & (t->ht_size - 1);
   e->chain = t->ht[k];
   t->ht[k] = e;
   t->ht_count++;
   return e;
}

static void ct_unlink_ht(companion_thumbs_t *t, struct ct_entry *e)
{
   struct ct_entry **pp = &t->ht[ct_hash(e->path, e->w, e->h) & (t->ht_size - 1)];
   while (*pp && *pp != e)
      pp = &(*pp)->chain;
   if (*pp)
   {
      *pp = e->chain;
      t->ht_count--;
   }
}

/* --- LRU ------------------------------------------------------------------ */

static void ct_lru_remove(companion_thumbs_t *t, struct ct_entry *e)
{
   if (e->lru_prev) e->lru_prev->lru_next = e->lru_next;
   else             t->lru_head           = e->lru_next;
   if (e->lru_next) e->lru_next->lru_prev = e->lru_prev;
   else             t->lru_tail           = e->lru_prev;
   e->lru_prev = e->lru_next = NULL;
}

static void ct_lru_push_front(companion_thumbs_t *t, struct ct_entry *e)
{
   e->lru_prev = NULL;
   e->lru_next = t->lru_head;
   if (t->lru_head)
      t->lru_head->lru_prev = e;
   t->lru_head = e;
   if (!t->lru_tail)
      t->lru_tail = e;
}

static void ct_entry_free(companion_thumbs_t *t, struct ct_entry *e)
{
   ct_unlink_ht(t, e);
   free(e->path);
   free(e->bits);
   free(e);
}

/* Drop least-recently-used cached entries until @need more bytes fit. */
static void ct_evict(companion_thumbs_t *t, size_t need)
{
   while (t->lru_tail && t->cached_bytes + need > t->budget)
   {
      struct ct_entry *e = t->lru_tail;
      ct_lru_remove(t, e);
      t->cached_bytes -= e->bytes;
      t->cached_count--;
      ct_entry_free(t, e);
   }
}

/* Cache decoded pixels on @e (takes ownership of @bits). */
static void ct_cache_put(companion_thumbs_t *t, struct ct_entry *e,
      uint32_t *bits)
{
   size_t bytes = (size_t)e->w * e->h * sizeof(uint32_t);
   ct_evict(t, bytes);
   e->bits  = bits;
   e->bytes = bytes;
   t->cached_bytes += bytes;
   t->cached_count++;
   ct_lru_push_front(t, e);
}

/* --- rings ---------------------------------------------------------------- */

static bool ct_ring_init(struct ct_ring *r, size_t cap)
{
   r->v    = (struct ct_job*)calloc(cap, sizeof(*r->v));
   r->cap  = cap;
   r->head = r->len = 0;
   return r->v != NULL;
}

/* Push on top (newest). A full ring drops its oldest job. */
static struct ct_job *ct_ring_push_top(struct ct_ring *r)
{
   size_t at;
   if (r->len == r->cap)
   {
      r->v[r->head].e->queued = false; /* dropped: may be requested again */
      if (r->v[r->head].e->refs)
         r->v[r->head].e->refs--;
      r->head = (r->head + 1) % r->cap;
      r->len--;
   }
   at = (r->head + r->len) % r->cap;
   r->len++;
   return &r->v[at];
}

/* Push at the bottom (oldest end). A full ring drops the top (newest). */
static struct ct_job *ct_ring_push_bottom(struct ct_ring *r)
{
   if (r->len == r->cap)
   {
      size_t top = (r->head + r->len - 1) % r->cap;
      r->v[top].e->queued = false;
      if (r->v[top].e->refs)
         r->v[top].e->refs--;
      r->len--;
   }
   r->head = (r->head + r->cap - 1) % r->cap;
   r->len++;
   return &r->v[r->head];
}

/* Pop from the top (newest). */
static bool ct_ring_pop_top(struct ct_ring *r, struct ct_job *out)
{
   if (!r->len)
      return false;
   *out = r->v[(r->head + r->len - 1) % r->cap];
   r->len--;
   return true;
}

/* Pop from the bottom (oldest). */
static bool ct_ring_pop_bottom(struct ct_ring *r, struct ct_job *out)
{
   if (!r->len)
      return false;
   *out    = r->v[r->head];
   r->head = (r->head + 1) % r->cap;
   r->len--;
   return true;
}

/* Next job: urgent (newest first), then prefetch (oldest first). Lock
 * held. */
static bool ct_next_job(companion_thumbs_t *t, struct ct_job *out)
{
   if (ct_ring_pop_top(&t->urgent, out) || ct_ring_pop_bottom(&t->prefetch, out))
   {
      t->queued--;
      t->inflight++;
      return true;
   }
   return false;
}

/* Park a result. Lock held. */
static void ct_push_done(companion_thumbs_t *t, const struct ct_job *j,
      uint32_t *bits)
{
   struct ct_done *d;
   if (t->inflight)
      t->inflight--;
   if (t->done_len == t->done_cap)
   {
      /* Full: grow, or as a last resort drop the oldest (its entry is
       * re-requestable). */
      size_t nc = t->done_cap * 2;
      struct ct_done *nd = (struct ct_done*)realloc(t->done, nc * sizeof(*nd));
      if (!nd)
      {
         free(bits);
         j->e->queued = false;
         return;
      }
      t->done     = nd;
      t->done_cap = nc;
   }
   d          = &t->done[t->done_len++];
   d->e       = j->e;
   d->tag     = j->tag;
   d->bits    = bits;
   d->epoch   = j->epoch;
   d->aborted = false;
   d->anim    = false;
   d->anim_gen = 0;
   d->anim_path = NULL;
   d->anim_w  = d->anim_h = 0;
}

/* --- workers -------------------------------------------------------------- */

#ifdef HAVE_THREADS
/* Abort hook for a worker's decode: stop at shutdown, and once the
 * job's epoch is stale (a cancel() happened since it was queued - the
 * view moved on, so its result would be discarded anyway). */
struct ct_abort_ctx { companion_thumbs_t *t; unsigned epoch; };

static bool ct_should_abort(void *ud)
{
   struct ct_abort_ctx *a = (struct ct_abort_ctx*)ud;
   bool stop;
   slock_lock(a->t->lock);
   stop = a->t->quit || a->t->epoch != a->epoch;
   slock_unlock(a->t->lock);
   return stop;
}

static void ct_worker(void *ud)
{
   companion_thumbs_t *t = (companion_thumbs_t*)ud;
   for (;;)
   {
      struct ct_job job;
      struct ct_abort_ctx actx;
      uint32_t *bits;
      bool aborted;
      slock_lock(t->lock);
      /* Timed wait: a lost wake-up can never keep a worker parked past
       * quit, so shutdown cannot hang on the join. */
      while (!t->quit && !t->queued)
         scond_wait_timeout(t->cond, t->lock, 100000);
      if (t->quit)
      {
         slock_unlock(t->lock);
         return;
      }
      if (!ct_next_job(t, &job))
      {
         slock_unlock(t->lock);
         continue;
      }
      slock_unlock(t->lock);

      actx.t     = t;
      actx.epoch = job.epoch;
      bits       = ct_decode(job.e->path, job.e->w, job.e->h, job.bg,
            ct_should_abort, &actx);

      slock_lock(t->lock);
      aborted = !bits && (t->quit || t->epoch != job.epoch);
      ct_push_done(t, &job, bits);
      if (aborted && t->done_len)
         t->done[t->done_len - 1].aborted = true;
      slock_unlock(t->lock);
   }
}
#endif

/* --- animation ------------------------------------------------------------ */

/* Is this file something the stream API can play? Cheap gate first
 * (container types with an animation decoder), then for PNG the APNG
 * probe over the head of the buffer, as gfx_thumbnail does. */
static bool ct_anim_type_ok(enum image_type_enum type, const uint8_t *buf,
      size_t len)
{
   switch (type)
   {
      case IMAGE_TYPE_PNG:
#ifdef HAVE_RPNG
         {
            int more = 0;
            return rpng_is_apng_ex(buf, len < 4096 ? len : 4096, &more);
         }
#else
         return false;
#endif
      case IMAGE_TYPE_WEBP:
      case IMAGE_TYPE_WEBM:
      case IMAGE_TYPE_MP4:
         return true;
      default:
         return false;
   }
}

#ifdef HAVE_THREADS
/* Push one animation frame (already scaled) for the current
 * animation. Lock held. */
static void ct_anim_push(companion_thumbs_t *t, uint32_t *bits,
      unsigned gen, uintptr_t tag, const char *path, int w, int h)
{
   struct ct_done *d;
   if (t->done_len == t->done_cap)
   {
      size_t nc = t->done_cap * 2;
      struct ct_done *nd = (struct ct_done*)realloc(t->done, nc * sizeof(*nd));
      if (!nd)
      {
         free(bits);
         return;
      }
      t->done     = nd;
      t->done_cap = nc;
   }
   d            = &t->done[t->done_len++];
   memset(d, 0, sizeof(*d));
   d->tag       = tag;
   d->bits      = bits;
   d->anim      = true;
   d->anim_gen  = gen;
   d->anim_path = strldup(path, strlen(path) + 1);
   d->anim_w    = w;
   d->anim_h    = h;
}

/* One animation at a time: open the file, open the stream, then loop
 * decoding a frame, scaling it, pushing it, sleeping its duration -
 * until the animation is superseded or the engine quits. Between
 * frames the thread checks for that, so a change lands within one
 * frame duration. */
static void ct_anim_thread(void *ud)
{
   companion_thumbs_t *t = (companion_thumbs_t*)ud;
   for (;;)
   {
      char path[PATH_MAX_LENGTH];
      int w, h;
      uintptr_t tag;
      uint32_t bg;
      unsigned gen;
      void *buf = NULL;
      int64_t len = 0;
      void *stream = NULL;
      enum image_type_enum type;
      unsigned fw = 0, fh = 0;
      int nframes = 0, loops = 0, loops_left;
      bool native_argb;

      slock_lock(t->lock);
      while (!t->quit && !t->anim.wanted)
         scond_wait_timeout(t->anim_cond, t->lock, 100000);
      if (t->quit)
      {
         slock_unlock(t->lock);
         return;
      }
      /* take the request */
      strlcpy(path, t->anim.path ? t->anim.path : "", sizeof(path));
      w   = t->anim.w;
      h   = t->anim.h;
      tag = t->anim.tag;
      bg  = t->anim.bg;
      gen = t->anim.gen;
      t->anim.wanted = false;
      slock_unlock(t->lock);

      if (!path[0])
         continue;
      type = image_texture_get_type(path);
      if (type == IMAGE_TYPE_NONE)
         continue;
      /* Whole file in memory (the stream borrows it). A preview is a
       * preview: refuse anything over 256 MiB rather than swallow it. */
      if (!filestream_read_file(path, &buf, &len) || !buf || len <= 0)
         continue;
      if (len > (int64_t)256 * 1024 * 1024
            || !ct_anim_type_ok(type, (const uint8_t*)buf, (size_t)len))
      {
         free(buf);
         continue;
      }
      stream = image_transfer_anim_stream_new(buf, (size_t)len, type);
      if (!stream)
      {
         free(buf);
         continue;             /* a still after all */
      }
      image_transfer_anim_stream_get_info(stream, type, &fw, &fh, &nframes, &loops);
      /* Ask for ARGB words directly; if the stream cannot, swizzle. */
      native_argb = image_transfer_anim_stream_set_argb(stream, type, 1);
      loops_left  = loops; /* 0 = forever */

      for (;;)
      {
         const uint32_t *frame;
         int duration_ms = 0;
         uint32_t *src, *bits;
         bool stale;

         slock_lock(t->lock);
         stale = t->quit || t->anim.gen != gen;
         slock_unlock(t->lock);
         if (stale)
            break;

         frame = image_transfer_anim_stream_next(stream, type, &duration_ms);
         if (!frame)
         {
            if (loops_left > 0 && --loops_left == 0)
               break;
            image_transfer_anim_stream_rewind(stream, type);
            frame = image_transfer_anim_stream_next(stream, type, &duration_ms);
            if (!frame)
               break;
         }
         if (!fw || !fh)
            break;
         /* the scaler composites ARGB; convert R,G,B,A memory order first
          * when the stream would not emit ARGB itself */
         src = (uint32_t*)frame;
         if (!native_argb)
         {
            size_t i, n = (size_t)fw * fh;
            src = (uint32_t*)malloc(n * sizeof(uint32_t));
            if (!src)
               break;
            for (i = 0; i < n; i++)
            {
               uint32_t px = frame[i];
               src[i] = (px & 0xFF00FF00u) | ((px & 0xFF) << 16) | ((px >> 16) & 0xFF);
            }
         }
         bits = companion_thumbs_scale(src, fw, fh, w, h, bg);
         if (src != frame)
            free(src);
         if (!bits)
            break;

         slock_lock(t->lock);
         if (t->quit || t->anim.gen != gen)
            free(bits);
         else
            ct_anim_push(t, bits, gen, tag, path, w, h);
         slock_unlock(t->lock);

         if (duration_ms < 10)
            duration_ms = 10;   /* a 0 ms frame would spin */
         retro_sleep(duration_ms);
      }
      image_transfer_anim_stream_free(stream, type);
      free(buf);
   }
}
#endif

void companion_thumbs_animate(companion_thumbs_t *t, const char *path,
      int w, int h, uintptr_t tag, uint32_t bg)
{
   if (!t || string_is_empty(path) || w < 1 || h < 1)
      return;
#ifdef HAVE_THREADS
   if (!t->lock)
      return;
   slock_lock(t->lock);
   free(t->anim.path);
   t->anim.path   = strldup(path, strlen(path) + 1);
   t->anim.w      = w;
   t->anim.h      = h;
   t->anim.tag    = tag;
   t->anim.bg     = bg;
   t->anim.gen++;
   t->anim.wanted = true;
   if (!t->anim_cond)
      t->anim_cond = scond_new();
   if (!t->anim_thread && t->anim_cond)
      t->anim_thread = sthread_create(ct_anim_thread, t);
   if (t->anim_cond)
      scond_signal(t->anim_cond);
   slock_unlock(t->lock);
#else
   (void)path; (void)w; (void)h; (void)tag; (void)bg;
#endif
}

void companion_thumbs_animate_stop(companion_thumbs_t *t)
{
   if (!t)
      return;
#ifdef HAVE_THREADS
   if (!t->lock)
      return;
   slock_lock(t->lock);
   t->anim.gen++;
   t->anim.wanted = false;
   slock_unlock(t->lock);
#endif
}

bool companion_thumbs_animating(companion_thumbs_t *t)
{
   bool on = false;
   if (!t)
      return false;
#ifdef HAVE_THREADS
   if (!t->lock)
      return false;
   slock_lock(t->lock);
   on = t->anim.wanted || (t->anim_thread != NULL && t->anim.path != NULL);
   slock_unlock(t->lock);
#endif
   return on;
}

/* --- API ------------------------------------------------------------------ */

companion_thumbs_t *companion_thumbs_new(size_t budget_bytes, unsigned threads)
{
   companion_thumbs_t *t = (companion_thumbs_t*)calloc(1, sizeof(*t));
   if (!t)
      return NULL;
   t->budget   = budget_bytes ? budget_bytes : (64u * 1024 * 1024);
   t->done_cap = 256;
   t->done     = (struct ct_done*)calloc(t->done_cap, sizeof(*t->done));
   if (!t->done || !ct_grow(t)
         || !ct_ring_init(&t->urgent, 1024) || !ct_ring_init(&t->prefetch, 1024))
   {
      companion_thumbs_free(t);
      return NULL;
   }
#ifdef HAVE_THREADS
   {
      unsigned i, n = threads;
      if (!n)
      {
         n = cpu_features_get_core_amount();
         n = (n > 1) ? n - 1 : 1;
      }
      if (n > 4)
         n = 4;
      t->lock = slock_new();
      t->cond = scond_new();
      if (t->lock && t->cond)
         for (i = 0; i < n; i++)
         {
            t->workers[i] = sthread_create(ct_worker, t);
            if (!t->workers[i])
               break;
            t->nworkers++;
         }
   }
#else
   (void)threads;
#endif
   return t;
}

void companion_thumbs_free(companion_thumbs_t *t)
{
   size_t i;
   if (!t)
      return;
#ifdef HAVE_THREADS
   if (t->lock)
   {
      slock_lock(t->lock);
      t->quit = true;
      if (t->cond)
         scond_broadcast(t->cond);
      slock_unlock(t->lock);
      for (i = 0; i < t->nworkers; i++)
         sthread_join(t->workers[i]);
      if (t->anim_thread)
      {
         slock_lock(t->lock);
         if (t->anim_cond)
            scond_broadcast(t->anim_cond);
         slock_unlock(t->lock);
         sthread_join(t->anim_thread);
      }
      if (t->anim_cond)
         scond_free(t->anim_cond);
      if (t->cond)
         scond_free(t->cond);
      slock_free(t->lock);
   }
#endif
   free(t->anim.path);
   for (i = 0; i < t->done_len; i++)
   {
      free(t->done[i].bits);
      free(t->done[i].anim_path);
   }
   free(t->done);
   free(t->urgent.v);
   free(t->prefetch.v);
   for (i = 0; i < t->ht_size; i++)
   {
      struct ct_entry *e = t->ht[i];
      while (e)
      {
         struct ct_entry *next = e->chain;
         free(e->path);
         free(e->bits);
         free(e);
         e = next;
      }
   }
   free(t->ht);
   free(t);
}

const uint32_t *companion_thumbs_get(companion_thumbs_t *t, const char *path,
      int w, int h)
{
   struct ct_entry *e;
   if (!t || string_is_empty(path))
      return NULL;
   e = ct_find(t, path, w, h);
   if (!e || !e->bits)
      return NULL;
   /* touch */
   ct_lru_remove(t, e);
   ct_lru_push_front(t, e);
   return e->bits;
}

bool companion_thumbs_request(companion_thumbs_t *t, const char *path,
      int w, int h, uintptr_t tag, bool urgent, uint32_t bg)
{
   struct ct_entry *e;
   struct ct_job *j;
   if (!t || string_is_empty(path) || w < 1 || h < 1)
      return false;

   e = ct_find(t, path, w, h);
   if (e && (e->bits || e->queued))
      return false;          /* cached or already on its way */
   if (!e && !(e = ct_insert(t, path, w, h)))
      return false;

   CT_LOCK(t);
   e->queued = true;
   e->refs++;
   j      = urgent ? ct_ring_push_top(&t->urgent) : ct_ring_push_bottom(&t->prefetch);
   j->e   = e;
   j->tag = tag;
   j->bg    = bg;
   j->epoch = t->epoch;
   t->queued++;
#ifdef HAVE_THREADS
   if (t->cond)
      scond_signal(t->cond);
#endif
   CT_UNLOCK(t);
   return true;
}

void companion_thumbs_cancel(companion_thumbs_t *t)
{
   struct ct_job j;
   if (!t)
      return;
   CT_LOCK(t);
   while (ct_ring_pop_top(&t->urgent, &j))
   {
      j.e->queued = false;
      if (j.e->refs)
         j.e->refs--;
   }
   while (ct_ring_pop_bottom(&t->prefetch, &j))
   {
      j.e->queued = false;
      if (j.e->refs)
         j.e->refs--;
   }
   t->queued = 0;
   /* Jobs a worker already holds: release their entries' queued flag
    * too (they are from the old epoch), so those keys can be requested
    * again; when the old result lands it is cached or deduped. */
   t->epoch++;
   {
      size_t i;
      for (i = 0; i < t->ht_size; i++)
      {
         struct ct_entry *e = t->ht[i];
         for (; e; e = e->chain)
            e->queued = false;
      }
   }
   CT_UNLOCK(t);
}

size_t companion_thumbs_poll(companion_thumbs_t *t,
      companion_thumbs_done_cb cb, void *ud, size_t max, unsigned budget_us)
{
   struct ct_done batch[64];
   size_t n, i, delivered = 0;
   if (!t)
      return 0;

#ifndef HAVE_THREADS
   /* No workers: decode here, under the budget. */
   {
      retro_time_t end = cpu_features_get_time_usec() + budget_us;
      struct ct_job job;
      while (t->queued && cpu_features_get_time_usec() < end
            && ct_next_job(t, &job))
         ct_push_done(t, &job, ct_decode(job.e->path, job.e->w, job.e->h, job.bg,
               NULL, NULL));
   }
#else
   (void)budget_us;
#endif

   for (;;)
   {
      CT_LOCK(t);
      n = t->done_len < 64 ? t->done_len : 64;
      if (max && delivered + n > max)
         n = max - delivered;
      memcpy(batch, t->done, n * sizeof(*batch));
      if (n < t->done_len)
         memmove(t->done, t->done + n, (t->done_len - n) * sizeof(*t->done));
      t->done_len -= n;
      CT_UNLOCK(t);
      if (!n)
         break;

      for (i = 0; i < n; i++)
      {
         struct ct_entry *e = batch[i].e;
         if (batch[i].anim)
         {
            /* An animation frame: only the current animation's. */
            bool current;
            CT_LOCK(t);
            current = (batch[i].anim_gen == t->anim.gen);
            CT_UNLOCK(t);
            if (current && cb && batch[i].bits)
               cb(ud, batch[i].anim_path, batch[i].anim_w, batch[i].anim_h,
                     batch[i].tag, batch[i].bits);
            free(batch[i].bits);
            free(batch[i].anim_path);
            delivered++;
            continue;
         }
         if (e->refs)
            e->refs--;             /* this record's reference */
         if (batch[i].aborted)
         {
            /* Abandoned mid-decode: nothing to deliver; forget the
             * entry unless something still points at it (a newer
             * request, or another finished record of this key). */
            if (!e->queued && !e->bits && !e->refs)
               ct_entry_free(t, e);
            continue;
         }
         if (batch[i].epoch == t->epoch)
            e->queued = false; /* else a newer request owns the flag */
         if (batch[i].bits)
         {
            /* Cached regardless of generation: the pixels are right for
             * the key; only the delivery may be stale. */
            if (e->bits)
            {
               free(batch[i].bits); /* decoded twice; keep the first */
               batch[i].bits = e->bits;
            }
            else
               ct_cache_put(t, e, batch[i].bits);
         }
         /* Always delivered: a decode that was in flight across a
          * cancel() still lands, and the backend checks the tag against
          * its own view state (its row generation). */
         if (cb)
            cb(ud, e->path, e->w, e->h, batch[i].tag, batch[i].bits);
         delivered++;
         if (!batch[i].bits)
         {
            /* Undecodable: forget the entry so the file can be retried
             * later (e.g. after a download) without a stale marker -
             * once nothing else refers to it. */
            if (!e->bits && !e->queued && !e->refs)
               ct_entry_free(t, e);
         }
      }
      if (max && delivered >= max)
         break;
   }
   return delivered;
}

void companion_thumbs_set_budget(companion_thumbs_t *t, size_t budget_bytes)
{
   if (!t)
      return;
   t->budget = budget_bytes ? budget_bytes : (64u * 1024 * 1024);
   ct_evict(t, 0);
}

size_t companion_thumbs_forget(companion_thumbs_t *t, const char *path)
{
   size_t i, dropped = 0;
   if (!t || !t->ht || string_is_empty(path))
      return 0;
   for (i = 0; i < t->ht_size; i++)
   {
      struct ct_entry *e = t->ht[i];
      while (e)
      {
         struct ct_entry *next = e->chain;
         if (e->bits && !e->queued && !e->refs && string_is_equal(e->path, path))
         {
            ct_lru_remove(t, e);
            t->cached_bytes -= e->bytes;
            t->cached_count--;
            ct_entry_free(t, e);
            dropped++;
            /* the chain changed under us: restart this bucket */
            e = t->ht[i];
            continue;
         }
         e = next;
      }
   }
   return dropped;
}

size_t companion_thumbs_cached_count(companion_thumbs_t *t) { return t ? t->cached_count : 0; }
size_t companion_thumbs_cached_bytes(companion_thumbs_t *t) { return t ? t->cached_bytes : 0; }
size_t companion_thumbs_queued(companion_thumbs_t *t)
{
   size_t n;
   if (!t)
      return 0;
   CT_LOCK(t);
   n = t->queued;
   CT_UNLOCK(t);
   return n;
}

size_t companion_thumbs_pending(companion_thumbs_t *t)
{
   size_t n;
   if (!t)
      return 0;
   CT_LOCK(t);
   n = t->queued + t->inflight + t->done_len;
   if (t->anim.wanted || t->anim.path)
      n++;
   CT_UNLOCK(t);
   return n;
}
