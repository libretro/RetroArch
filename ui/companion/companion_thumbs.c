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
#include <formats/data_transfer.h>
#include <streams/file_stream.h>
#include <time.h>          /* struct timespec, for retro_timers.h */
#include <retro_timers.h>

#include "../../gfx/gfx_anim_preview.h"
#include <formats/video_player.h>
#include <features/features_cpu.h>
#include <retro_atomic.h>
#include <retro_miscellaneous.h>
#include <retro_inline.h>

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>
#include <queues/retro_triple_buffer.h>
#endif

#include "companion_thumbs.h"

/* --- cache entry ------------------------------------------------------- */

struct ct_entry
{
   char *path;
   uint32_t *bits;        /* NULL while queued / decoding */
   unsigned dims;
   size_t bytes;
   /* LRU list of cached entries (bits != NULL); most recent at head. */
   struct ct_entry *lru_prev, *lru_next;
   /* hash chain */
   struct ct_entry *chain;
   /* The job last queued for it, while that job is the queue's: what an
    * animation of this file waits on. UI thread. */
   struct ct_job *job;
   bool queued;           /* in a request queue or being decoded */
   /* Jobs (queued, decoding, or finished but not yet polled) that point
    * at this entry. An entry is only ever freed at zero: two records
    * for one key exist when a key is re-requested while its earlier
    * decode is still in flight across a cancel(), or when a failed
    * decode's record is polled with another still pending. */
   unsigned refs;
};

/* --- request / result queues ------------------------------------------- */

/* A request, from the UI thread to a worker and back as its result:
 * the one record all the way, handed over without a lock. */
struct ct_job
{
   struct ct_entry *e;
   uintptr_t tag;
   uint32_t bg;
   unsigned epoch;
   uint32_t *bits;        /* result: NULL, decode failed (or aborted) */
   struct ct_job *next;   /* on the done list */
   bool aborted;          /* abandoned: not delivered, entry dropped */
#ifdef HAVE_THREADS
   /* The queue's reference, and the animation's while it waits for this
    * still; the last to let go frees the record. */
   retro_atomic_int_t refs;
   /* Animation requests waiting for this still: while any is, the
    * session it was decoded through is wanted. */
   retro_atomic_int_t anim;
   /* Decoded and its session parked, or dropped: either way the
    * animation waits no longer. */
   retro_atomic_int_t landed;
#endif
};

#ifdef HAVE_THREADS
/* One animation frame, scaled: a slot of the triple buffer the
 * animation thread hands frames to poll() through. */
struct ct_anim_frame
{
   uint32_t *bits;
   unsigned dims;
   unsigned gen;          /* the animation it belongs to */
   uintptr_t tag;
   char path[PATH_MAX_LENGTH];
};

/* What animate() asks of the animation thread, which takes it whole. */
struct ct_anim_req
{
   char *path;
   unsigned dims;
   uintptr_t tag;
   uint32_t bg;
   unsigned gen;
   struct ct_job *still;  /* its still, queued or decoding, held */
};

/* A still's session, parked for the animation to carry on from. */
struct ct_parked
{
   gfx_anim_preview_t *sess;
   char *path;
};

typedef retro_atomic_ptr_t ct_slot_t;
#else
typedef void *ct_slot_t;
#endif

/* A lane of requests.  The UI thread puts each in the slot at hi and
 * moves hi on; a worker takes one by swapping its slot to NULL, and the
 * UI thread takes one back the same way, so each goes to exactly one.
 * Slots outside lo..hi are empty; n counts the jobs inside.  urgent is
 * taken newest first, prefetch oldest first. */
#define CT_LANE_CAP 1024u

struct ct_lane
{
   ct_slot_t slot[CT_LANE_CAP];
#ifdef HAVE_THREADS
   retro_atomic_int_t lo, hi, n;
#else
   unsigned lo, hi, n;
#endif
};

struct companion_thumbs
{
   /* hash table of entries (both cached and queued), UI-thread owned;
    * a worker reads the path and size of the entry its job holds */
   struct ct_entry **ht;
   size_t ht_size, ht_count;

   struct ct_entry *lru_head, *lru_tail;
   size_t cached_bytes, cached_count, budget;

   struct ct_lane urgent, prefetch;
   /* For moving a lane's jobs together when gaps fill its span. */
   struct ct_job *lane_scratch[CT_LANE_CAP];
   unsigned epoch;        /* bumped by cancel(): an in-flight job from an
                           * older epoch no longer holds its entry's
                           * queued flag, so the key can be re-requested;
                           * a second decode of the same key is deduped
                           * at delivery */

   /* Results the UI thread has taken off the done list and not yet
    * delivered, oldest first. */
   struct ct_job *done_head, *done_tail;

   /* The one animation, as the UI thread last asked for it. */
   struct
   {
      char *path;
      unsigned gen;           /* the animation that should be playing */
   } anim;

#ifdef HAVE_THREADS
   sthread_t *workers[4];
   unsigned nworkers;

   /* Workers wait here for a job; the animation thread for a request,
    * a still to land or the end of a frame's hold. */
   retro_eventcount_t work_ec;
   retro_eventcount_t anim_ec;
   bool ecs_inited;

   /* Finished jobs, pushed by the workers, newest first. */
   retro_atomic_ptr_t done_list;

   retro_atomic_int_t quit;
   retro_atomic_int_t epoch_now;     /* epoch */
   retro_atomic_int_t jobs_queued;   /* jobs in the lanes */
   retro_atomic_int_t inflight;      /* jobs a worker holds */
   retro_atomic_int_t done_n;        /* finished, not yet delivered */
   retro_atomic_int_t anim_gen_now;  /* anim.gen */
   retro_atomic_int_t anim_asked;    /* an animate() has ever been made */

   /* The request the animation thread has yet to take. */
   retro_atomic_ptr_t anim_req;
   /* A still's session waiting for the animation of its file. */
   retro_atomic_ptr_t parked;

   sthread_t *anim_thread;

   /* Frames from the animation thread to poll(), newest wins. */
   struct ct_anim_frame anim_frames[3];
   retro_triple_buffer_t anim_tb;

   /* The open preview session, while the animation thread plays it:
    * poll() starts and feeds its audio, animate() and _stop() silence
    * it. A UI-thread user holds it with ct_sess_acquire(); the
    * animation thread takes it away with ct_sess_retire(), which waits
    * out any holder, before it closes it. anim_audio_gen: the
    * animation whose audio was started (UI thread only). */
   retro_atomic_ptr_t anim_sess;
   retro_atomic_int_t anim_sess_gen;
   retro_atomic_int_t anim_sess_users;
   unsigned anim_audio_gen;
#else
   size_t jobs_queued, inflight, done_n;
#endif
};

#ifdef HAVE_THREADS
#define CT_INT_LOAD(p)        retro_atomic_load_acquire_int(p)
#define CT_INT_STORE(p, v)    retro_atomic_store_release_int(p, v)
#define CT_INT_ADD(p, v)      retro_atomic_fetch_add_int(p, v)
#define CT_SLOT_LOAD(p)       retro_atomic_load_acquire_ptr(p)
#define CT_SLOT_STORE(p, v)   retro_atomic_store_release_ptr(p, v)
#define CT_SLOT_TAKE(p)       retro_atomic_exchange_ptr(p, NULL)
#define CT_COUNT(t, f)        ((size_t)retro_atomic_load_acquire_int(&(t)->f))
#define CT_COUNT_ADD(t, f, v) retro_atomic_fetch_add_int(&(t)->f, v)
#else
#define CT_INT_LOAD(p)        (*(p))
#define CT_INT_STORE(p, v)    (*(p) = (v))
#define CT_INT_ADD(p, v)      (*(p) += (v))
#define CT_SLOT_LOAD(p)       (*(p))
#define CT_SLOT_STORE(p, v)   (*(p) = (v))
#define CT_COUNT(t, f)        ((t)->f)
#define CT_COUNT_ADD(t, f, v) ((t)->f += (v))
#endif

#ifdef HAVE_THREADS
static bool ct_quitting(companion_thumbs_t *t)
{
   return retro_atomic_load_acquire_int(&t->quit) != 0;
}

/* The preview session, held for a UI-thread call into it: NULL, held
 * nothing, when none is playing. */
static gfx_anim_preview_t *ct_sess_acquire(companion_thumbs_t *t)
{
   gfx_anim_preview_t *sess;
   (void)retro_atomic_fetch_add_seq_cst_int(&t->anim_sess_users, 1);
   retro_atomic_thread_fence_seq_cst();
   if (!(sess = (gfx_anim_preview_t*)
            retro_atomic_load_acquire_ptr(&t->anim_sess)))
   {
      if (retro_atomic_fetch_sub_int(&t->anim_sess_users, 1) == 1)
         retro_eventcount_notify(&t->anim_ec);
   }
   return sess;
}

static void ct_sess_release(companion_thumbs_t *t)
{
   if (retro_atomic_fetch_sub_int(&t->anim_sess_users, 1) == 1)
      retro_eventcount_notify(&t->anim_ec);
}

/* Animation thread: the session is no longer published; returns once
 * no UI-thread call is inside it. */
static void ct_sess_retire(companion_thumbs_t *t)
{
   (void)retro_atomic_exchange_ptr(&t->anim_sess, NULL);
   retro_atomic_thread_fence_seq_cst();
   for (;;)
   {
      int key;
      if (!retro_atomic_load_seq_cst_int(&t->anim_sess_users))
         return;
      key = retro_eventcount_prepare_wait(&t->anim_ec);
      if (!retro_atomic_load_seq_cst_int(&t->anim_sess_users))
      {
         retro_eventcount_cancel_wait(&t->anim_ec);
         return;
      }
      retro_eventcount_commit_wait(&t->anim_ec, key);
   }
}
#else
static void *ct_slot_take(ct_slot_t *slot)
{
   void *v = *slot;
   *slot   = NULL;
   return v;
}
#define CT_SLOT_TAKE(p) ct_slot_take(p)
#endif

/* --- scaling (pure) ---------------------------------------------------- */

/* R,G,B,A memory order -> ARGB word. */
#define CT_RGBA_TO_ARGB(p) \
   (((p) & 0x0000FF00u) | (((p) & 0xFFu) << 16) | (((p) >> 16) & 0xFFu) | ((p) & 0xFF000000u))

/* Composite one source pixel over @bg (opaque result). A @bg with a
 * zero alpha byte means "keep transparency": the source pixel is
 * returned as-is, straight alpha and all, and the letterbox is filled
 * with @bg (transparent), so the backend's own window / cell background
 * shows through. */
static INLINE uint32_t ct_over(uint32_t p, uint32_t bg)
{
   unsigned a = (p >> 24) & 0xff;
   if (!(bg & 0xff000000u))
      return p;
   if (a == 0xff)
      return p | 0xff000000u;
   {
      unsigned ia = 255 - a;
      unsigned r  = (((p >> 16) & 0xff) * a + ((bg >> 16) & 0xff) * ia) / 255;
      unsigned g  = (((p >>  8) & 0xff) * a + ((bg >>  8) & 0xff) * ia) / 255;
      unsigned b  = (( p        & 0xff) * a + ( bg        & 0xff) * ia) / 255;
      return 0xff000000u | (r << 16) | (g << 8) | b;
   }
}

/* Per-channel blend of two pixels, @f/256 of @b, rounded. */
static INLINE uint32_t ct_lerp(uint32_t a, uint32_t b, unsigned f)
{
   uint32_t rb = (a & 0x00ff00ffu) * (256 - f) + (b & 0x00ff00ffu) * f;
   uint32_t ag = ((a >> 8) & 0x00ff00ffu) * (256 - f) + ((b >> 8) & 0x00ff00ffu) * f;
   return (((rb + 0x00800080u) >> 8) & 0x00ff00ffu)
        | ((ag + 0x00800080u) & 0xff00ff00u);
}

/* Straight -> premultiplied alpha (alpha is the top byte in both the
 * ARGB word and the R,G,B,A memory order, so this is order-agnostic). */
static INLINE uint32_t ct_premul(uint32_t p)
{
   unsigned a = p >> 24;
   if (a == 0xff)
      return p;
   if (a == 0)
      return 0;
   {
      unsigned c0 = (((p >> 16) & 0xff) * a + 127) / 255;
      unsigned c1 = (((p >>  8) & 0xff) * a + 127) / 255;
      unsigned c2 = (( p        & 0xff) * a + 127) / 255;
      return (a << 24) | (c0 << 16) | (c1 << 8) | c2;
   }
}

/* Premultiplied -> straight alpha. */
static INLINE uint32_t ct_unpremul(uint32_t p)
{
   unsigned a = p >> 24;
   if (a == 0xff)
      return p;
   if (a == 0)
      return 0;
   {
      unsigned c0 = (((p >> 16) & 0xff) * 255 + a / 2) / a;
      unsigned c1 = (((p >>  8) & 0xff) * 255 + a / 2) / a;
      unsigned c2 = (( p        & 0xff) * 255 + a / 2) / a;
      if (c0 > 0xff) c0 = 0xff;
      if (c1 > 0xff) c1 = 0xff;
      if (c2 > 0xff) c2 = 0xff;
      return (a << 24) | (c0 << 16) | (c1 << 8) | c2;
   }
}

/* Bilinear blend of four taps with alpha. Blending straight alpha lets
 * the colour of a transparent pixel (often black) bleed into its opaque
 * neighbour, a dark halo along every transparent edge; blend
 * premultiplied instead. Rows known to be fully opaque skip this and
 * lerp directly. */
static uint32_t ct_bilerp_alpha(uint32_t p0, uint32_t p1,
      uint32_t p2, uint32_t p3, unsigned fx, unsigned fy)
{
   return ct_unpremul(ct_lerp(
            ct_lerp(ct_premul(p0), ct_premul(p1), fx),
            ct_lerp(ct_premul(p2), ct_premul(p3), fx), fy));
}

/* One output row of the bilinear path: @fw pixels from rows @ra/@rb
 * with the column taps in @col and row weight @fy. The alpha handling
 * is picked once per row so the opaque loop stays tight. */
static void ct_row_bilinear(uint32_t *row, const uint32_t *ra,
      const uint32_t *rb, const unsigned *col, int fw, unsigned fy,
      bool opaque, uint32_t bg, bool src_rgba_order)
{
   int x;
   if (opaque)
   {
      for (x = 0; x < fw; x++)
      {
         const unsigned *c = col + x * 3;
         uint32_t p = ct_lerp(ct_lerp(ra[c[0]], ra[c[1]], c[2]),
               ct_lerp(rb[c[0]], rb[c[1]], c[2]), fy);
         if (src_rgba_order)
            p = CT_RGBA_TO_ARGB(p);
         row[x] = ct_over(p, bg);
      }
   }
   else
   {
      for (x = 0; x < fw; x++)
      {
         const unsigned *c = col + x * 3;
         uint32_t p = ct_bilerp_alpha(ra[c[0]], ra[c[1]],
               rb[c[0]], rb[c[1]], c[2], fy);
         if (src_rgba_order)
            p = CT_RGBA_TO_ARGB(p);
         row[x] = ct_over(p, bg);
      }
   }
}

uint32_t *companion_thumbs_scale_ex(const uint32_t *src,
      unsigned src_dims, unsigned dst_dims, uint32_t bg,
      bool src_rgba_order)
{
   uint32_t *buf;
   int fw, fh, ox, oy, x, y;
   bool taps4;
   bool opaque = false;
   /* Bilinear column taps (xa, xb, fx per output column), built once:
    * they depend only on x, and a 64-bit divide per output pixel was
    * the bulk of the scale time. */
   unsigned *col = NULL;
   unsigned sw = VIDEO_SCALE_W(src_dims);
   unsigned sh = VIDEO_SCALE_H(src_dims);
   int dw      = (int)VIDEO_SCALE_W(dst_dims);
   int dh      = (int)VIDEO_SCALE_H(dst_dims);

   if (!src || !sw || !sh || !dw || !dh)
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
   /* Four taps when every output pixel covers at least a 2 x 2 source
    * cell; bilinear when enlarging or nearly 1:1. */
   taps4 = (sw >= 2u * (unsigned)fw) && (sh >= 2u * (unsigned)fh);
   if (!taps4)
   {
      int i;
      col = (unsigned*)malloc((size_t)fw * 3 * sizeof(unsigned));
      if (!col)
      {
         free(buf);
         return NULL;
      }
      for (i = 0; i < fw; i++)
      {
         /* This column's centre in the source, in 1/256 px. */
         uint64_t px = (uint64_t)(2 * i + 1) * sw * 128 / fw;
         unsigned xa;
         px = (px > 128) ? px - 128 : 0;
         xa = (unsigned)(px >> 8);
         col[i * 3 + 0] = xa;
         col[i * 3 + 1] = (xa + 1 < sw) ? xa + 1 : xa;
         col[i * 3 + 2] = (unsigned)px & 0xff;
      }
      /* Whole source opaque (the usual case): one AND-reduce over it
       * lets the per-pixel blend skip the alpha handling entirely. */
      {
         uint32_t all = 0xffffffffu;
         size_t n = (size_t)sw * sh, k;
         for (k = 0; k < n; k++)
            all &= src[k];
         opaque = (all >> 24) == 0xff;
      }
   }

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
         int      sy   = y - oy;
         unsigned y0   = (unsigned)((uint64_t)sy * sh / fh);
         unsigned y1   = (unsigned)((uint64_t)(sy + 1) * sh / fh);
         unsigned fy   = 0;
         const uint32_t *ra, *rb;
         if (y1 <= y0) y1 = y0 + 1;
         if (y1 > sh)  y1 = sh;
         /* Taps at a third in from each edge of the cell: for a 4-px
          * cell rows 1 and 2, for 2 px rows 0 and 1 - always two
          * distinct rows of different parity, so a 1-px checkerboard
          * (the worst case for aliasing) really averages. */
         ra = src + (size_t)(y0 + (y1 - y0 - 1) / 3) * sw;
         rb = src + (size_t)(y1 - 1 - (y1 - y0 - 1) / 3) * sw;
         if (!taps4)
         {
            /* Bilinear: this row's centre in the source, in 1/256 px. */
            uint64_t py = (uint64_t)(2 * sy + 1) * sh * 128 / fh;
            py = (py > 128) ? py - 128 : 0;
            y0 = (unsigned)(py >> 8);
            fy = (unsigned)py & 0xff;
            ra = src + (size_t)y0 * sw;
            rb = (y0 + 1 < sh) ? ra + sw : ra;
            for (x = 0; x < ox; x++)
               row[x] = bg;
            ct_row_bilinear(row + ox, ra, rb, col, fw, fy, opaque, bg,
                  src_rgba_order);
            for (x = ox + fw; x < dw; x++)
               row[x] = bg;
            continue;
         }
         for (x = 0; x < dw; x++)
         {
            if (x < ox || x >= ox + fw)
               row[x] = bg;
            else
            {
               {
                  int      sx = x - ox;
                  unsigned x0 = (unsigned)((uint64_t)sx * sw / fw);
                  unsigned x1 = (unsigned)((uint64_t)(sx + 1) * sw / fw);
                  unsigned xa, xb;
                  uint32_t p0, p1, p2, p3, r, g, b, al;
                  if (x1 <= x0) x1 = x0 + 1;
                  if (x1 > sw)  x1 = sw;
                  xa = x0 + (x1 - x0 - 1) / 3;
                  xb = x1 - 1 - (x1 - x0 - 1) / 3;
                  p0 = ra[xa]; p1 = ra[xb]; p2 = rb[xa]; p3 = rb[xb];
                  if (src_rgba_order)
                  {
                     p0 = CT_RGBA_TO_ARGB(p0); p1 = CT_RGBA_TO_ARGB(p1);
                     p2 = CT_RGBA_TO_ARGB(p2); p3 = CT_RGBA_TO_ARGB(p3);
                  }
                  /* average the four (including alpha), then composite */
                  r  = (((p0 >> 16) & 0xff) + ((p1 >> 16) & 0xff) + ((p2 >> 16) & 0xff) + ((p3 >> 16) & 0xff)) >> 2;
                  g  = (((p0 >>  8) & 0xff) + ((p1 >>  8) & 0xff) + ((p2 >>  8) & 0xff) + ((p3 >>  8) & 0xff)) >> 2;
                  b  = (( p0        & 0xff) + ( p1        & 0xff) + ( p2        & 0xff) + ( p3        & 0xff)) >> 2;
                  al = (((p0 >> 24) & 0xff) + ((p1 >> 24) & 0xff) + ((p2 >> 24) & 0xff) + ((p3 >> 24) & 0xff)) >> 2;
                  row[x] = ct_over((al << 24) | (r << 16) | (g << 8) | b, bg);
               }
            }
         }
      }
   }
   free(col);
   return buf;
}

uint32_t *companion_thumbs_scale(const uint32_t *src,
      unsigned src_dims, unsigned dst_dims, uint32_t bg)
{
   return companion_thumbs_scale_ex(src, src_dims, dst_dims, bg, false);
}

#ifdef HAVE_THREADS
/* Does @path take the anim-first route - its still being the first
 * frame of a preview session that the animation then continues?  A
 * video always; a WEBP or PNG when its head says it animates (32
 * bytes / 4 KiB read); anything else never. Only the decode threads
 * ask: poll() decodes stills alone. */
static int ct_anim_first(const char *path)
{
   enum image_type_enum type = image_texture_get_type(path);
   if (type == IMAGE_TYPE_WEBM || type == IMAGE_TYPE_MP4)
      return 1;
   if (type == IMAGE_TYPE_WEBP || type == IMAGE_TYPE_PNG)
      return gfx_anim_preview_probe(path) == 1;
   return 0;
}
#endif

#ifdef HAVE_THREADS
static void ct_parked_free(struct ct_parked *pk)
{
   if (!pk)
      return;
   gfx_anim_preview_close(pk->sess);
   free(pk->path);
   free(pk);
}
#endif

/* A video's still is its first frame, taken through the same windowed
 * open the menu uses: image_texture_load would read the whole file
 * (a two-hour recording) to show one frame; this reads the head. */
static uint32_t *ct_decode_video_still(companion_thumbs_t *t,
      struct ct_job *job, const char *path, unsigned dims, uint32_t bg)
{
   gfx_anim_preview_t *sess = gfx_anim_preview_open(path, -1);
   const uint32_t *frame;
   uint32_t *bits = NULL;
   bool native_argb = false;
   int dur = 0;
   if (!sess)
      return NULL;
   if (!gfx_anim_preview_feed(sess)
         || !(frame = gfx_anim_preview_next(sess, &dur, &native_argb))
         || !sess->dims)
   {
      gfx_anim_preview_close(sess);
      return NULL;
   }
   bits = companion_thumbs_scale_ex(frame, sess->dims, dims, bg,
         !native_argb);
#ifdef HAVE_THREADS
   /* The animation for this path continues from this session (its
    * next frame is the second one) instead of opening its own and
    * decoding this frame again.  Only when it waits for this still: a
    * session nobody takes would sit on its window until the next park. */
   if (     t && job && !ct_quitting(t)
         && retro_atomic_load_acquire_int(&job->anim) > 0)
   {
      struct ct_parked *pk = (struct ct_parked*)malloc(sizeof(*pk));
      if (pk && (pk->path = strldup(path, strlen(path) + 1)))
      {
         pk->sess = sess;
         sess     = NULL;
         ct_parked_free((struct ct_parked*)
               retro_atomic_exchange_ptr(&t->parked, pk));
      }
      else
         free(pk);
   }
#else
   (void)t;
   (void)job;
#endif
   if (sess)
      gfx_anim_preview_close(sess);
   return bits;
}

/* Decode @path and scale to @dims. Runs on a worker; @should_abort
 * (may be NULL) is asked between decode steps so a giant image can be
 * abandoned at shutdown or once nobody wants it. */
static uint32_t *ct_decode(companion_thumbs_t *t, struct ct_job *job,
      const char *path, unsigned dims, uint32_t bg,
      bool (*should_abort)(void *ud), void *ud, int anim_first)
{
   struct texture_image img;
   uint32_t *bits = NULL;
   /* A video, an animated WEBP or an APNG (anim_first, decided by
    * ct_anim_first): the still is the first frame through the windowed
    * session, and the session is then handed to the animation.
    * image_texture_load would read and parse the whole file for that
    * one frame - 600 ms and 40 MB for a 40 MB animated WEBP, against
    * 16 ms through the session.  A still PNG / WEBP decodes as before;
    * so does an animated one the session would not admit. */
   if (anim_first)
   {
      uint32_t *b = ct_decode_video_still(t, job, path, dims, bg);
      if (b)
         return b;
   }
   memset(&img, 0, sizeof(img));
   if (image_texture_load_ex(&img, path, should_abort, ud))
   {
      if (img.pixels && VIDEO_SCALE_FITS(img.width, img.height))
         bits = companion_thumbs_scale(img.pixels,
               VIDEO_SCALE_PACK(img.width, img.height), dims, bg);
      image_texture_free(&img);
   }
   return bits;
}

/* --- hash table ---------------------------------------------------------- */

static size_t ct_hash(const char *path, unsigned dims)
{
   size_t k = 2166136261u;
   while (*path)
      k = (k ^ (unsigned char)*path++) * 16777619u;
   k = (k ^ dims) * 16777619u;
   return k;
}

static struct ct_entry *ct_find(companion_thumbs_t *t, const char *path,
      unsigned dims)
{
   struct ct_entry *e;
   if (!t->ht)
      return NULL;
   for (e = t->ht[ct_hash(path, dims) & (t->ht_size - 1)]; e; e = e->chain)
      if (e->dims == dims && string_is_equal(e->path, path))
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
         size_t k = ct_hash(e->path, e->dims) & (ns - 1);
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
      unsigned dims)
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
   e->dims  = dims;
   k        = ct_hash(path, dims) & (t->ht_size - 1);
   e->chain = t->ht[k];
   t->ht[k] = e;
   t->ht_count++;
   return e;
}

static void ct_unlink_ht(companion_thumbs_t *t, struct ct_entry *e)
{
   struct ct_entry **pp = &t->ht[ct_hash(e->path, e->dims) & (t->ht_size - 1)];
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

/* Drop least-recently-used cached entries until @need more bytes fit.
 * One a job still points at stays: its worker reads the path. */
static void ct_evict(companion_thumbs_t *t, size_t need)
{
   struct ct_entry *e = t->lru_tail;
   while (e && t->cached_bytes + need > t->budget)
   {
      struct ct_entry *prev = e->lru_prev;
      if (!e->refs)
      {
         ct_lru_remove(t, e);
         t->cached_bytes -= e->bytes;
         t->cached_count--;
         ct_entry_free(t, e);
      }
      e = prev;
   }
}

/* Cache decoded pixels on @e (takes ownership of @bits). */
static void ct_cache_put(companion_thumbs_t *t, struct ct_entry *e,
      uint32_t *bits)
{
   size_t bytes = (size_t)VIDEO_SCALE_W(e->dims)
      * VIDEO_SCALE_H(e->dims) * sizeof(uint32_t);
   ct_evict(t, bytes);
   e->bits  = bits;
   e->bytes = bytes;
   t->cached_bytes += bytes;
   t->cached_count++;
   ct_lru_push_front(t, e);
}

/* --- jobs and lanes ------------------------------------------------------ */

static void ct_job_release(struct ct_job *j)
{
#ifdef HAVE_THREADS
   if (retro_atomic_fetch_sub_int(&j->refs, 1) != 1)
      return;
#endif
   free(j);
}

/* The job is the queue's no longer: an animation waiting on it stops,
 * and its entry lets go of it. UI thread. */
static void ct_job_retire(companion_thumbs_t *t, struct ct_job *j)
{
   if (j->e->job == j)
      j->e->job = NULL;
#ifdef HAVE_THREADS
   if (retro_atomic_load_acquire_int(&j->anim) > 0)
   {
      retro_atomic_store_release_int(&j->landed, 1);
      if (t->ecs_inited)
         retro_eventcount_notify(&t->anim_ec);
   }
#else
   (void)t;
#endif
   ct_job_release(j);
}

/* A job taken back off a lane before any worker had it. UI thread. */
static void ct_job_drop(companion_thumbs_t *t, struct ct_job *j)
{
   CT_COUNT_ADD(t, jobs_queued, -1);
   j->e->queued = false;      /* may be requested again */
   if (j->e->refs)
      j->e->refs--;
   ct_job_retire(t, j);
}

/* UI thread: moves both ends in past the slots no job is in. */
static void ct_lane_trim(struct ct_lane *l)
{
   unsigned lo = (unsigned)CT_INT_LOAD(&l->lo);
   unsigned hi = (unsigned)CT_INT_LOAD(&l->hi);
   while (lo != hi && !CT_SLOT_LOAD(&l->slot[lo & (CT_LANE_CAP - 1)]))
      lo++;
   while (lo != hi && !CT_SLOT_LOAD(&l->slot[(hi - 1) & (CT_LANE_CAP - 1)]))
      hi--;
   CT_INT_STORE(&l->lo, (int)lo);
   CT_INT_STORE(&l->hi, (int)hi);
}

/* Any thread: takes the newest job (or with @oldest the oldest) still in
 * the lane. A slot swapped to NULL is the taker's alone. */
static struct ct_job *ct_lane_take(struct ct_lane *l, bool oldest)
{
   unsigned lo = (unsigned)CT_INT_LOAD(&l->lo);
   unsigned hi = (unsigned)CT_INT_LOAD(&l->hi);
   unsigned n  = hi - lo, k;
   if (n > CT_LANE_CAP)
      return NULL;
   for (k = 0; k < n; k++)
   {
      unsigned pos = oldest ? lo + k : hi - 1 - k;
      ct_slot_t *slot = &l->slot[pos & (CT_LANE_CAP - 1)];
      struct ct_job *j;
      if (!CT_SLOT_LOAD(slot))
         continue;
      if ((j = (struct ct_job*)CT_SLOT_TAKE(slot)))
      {
         CT_INT_ADD(&l->n, -1);
         return j;
      }
   }
   return NULL;
}

/* UI thread: the lane's jobs taken out and put back side by side, in
 * order, when gaps a worker left in the middle fill its span.  A worker
 * that misses one meanwhile is woken by the push that follows. */
static void ct_lane_compact(companion_thumbs_t *t, struct ct_lane *l)
{
   unsigned lo = (unsigned)CT_INT_LOAD(&l->lo);
   unsigned hi = (unsigned)CT_INT_LOAD(&l->hi);
   unsigned n  = 0, k;
   for (k = 0; k < hi - lo; k++)
   {
      struct ct_job *j = (struct ct_job*)CT_SLOT_TAKE(
            &l->slot[(lo + k) & (CT_LANE_CAP - 1)]);
      if (j)
         t->lane_scratch[n++] = j;
   }
   /* Every slot is empty now: the jobs go back from hi on. */
   CT_INT_STORE(&l->lo, (int)hi);
   for (k = 0; k < n; k++)
      CT_SLOT_STORE(&l->slot[(hi + k) & (CT_LANE_CAP - 1)],
            t->lane_scratch[k]);
   CT_INT_STORE(&l->hi, (int)(hi + n));
}

/* UI thread.  When the lane holds as many jobs as it has slots, urgent
 * drops its oldest and prefetch its newest. */
static void ct_lane_push(companion_thumbs_t *t, struct ct_lane *l,
      struct ct_job *j, bool drop_oldest)
{
   unsigned hi;
   if ((unsigned)CT_INT_LOAD(&l->n) >= CT_LANE_CAP)
   {
      struct ct_job *old = ct_lane_take(l, drop_oldest);
      if (old)
         ct_job_drop(t, old);
   }
   ct_lane_trim(l);
   if (     (unsigned)CT_INT_LOAD(&l->hi) - (unsigned)CT_INT_LOAD(&l->lo)
         >= CT_LANE_CAP)
      ct_lane_compact(t, l);
   hi = (unsigned)CT_INT_LOAD(&l->hi);
   CT_SLOT_STORE(&l->slot[hi & (CT_LANE_CAP - 1)], j);
   CT_INT_STORE(&l->hi, (int)(hi + 1));
   CT_INT_ADD(&l->n, 1);
}

/* Next job: urgent newest first, then prefetch oldest first. */
static struct ct_job *ct_next_job(companion_thumbs_t *t)
{
   struct ct_job *j = ct_lane_take(&t->urgent, false);
   if (!j)
      j = ct_lane_take(&t->prefetch, true);
   if (j)
   {
      CT_COUNT_ADD(t, inflight, 1);
      CT_COUNT_ADD(t, jobs_queued, -1);
   }
   return j;
}

/* A worker's result, for poll(): counted as finished before it stops
 * counting as in flight, so pending() never reads zero between. */
static void ct_push_done(companion_thumbs_t *t, struct ct_job *j)
{
   CT_COUNT_ADD(t, done_n, 1);
#ifdef HAVE_THREADS
   {
      void *head;
      do
      {
         head    = retro_atomic_load_acquire_ptr(&t->done_list);
         j->next = (struct ct_job*)head;
      } while (!retro_atomic_cas_ptr(&t->done_list, head, j));
   }
#else
   j->next = NULL;
   if (t->done_tail)
      t->done_tail->next = j;
   else
      t->done_head = j;
   t->done_tail = j;
#endif
   CT_COUNT_ADD(t, inflight, -1);
}

#ifdef HAVE_THREADS
/* The workers' results so far onto the end of the UI thread's list,
 * oldest first. */
static void ct_take_done(companion_thumbs_t *t)
{
   struct ct_job *j, *fifo = NULL, *last;
   if (!retro_atomic_load_acquire_ptr(&t->done_list))
      return;
   j = (struct ct_job*)retro_atomic_exchange_ptr(&t->done_list, NULL);
   last = j;
   while (j)
   {
      struct ct_job *next = j->next;
      j->next = fifo;
      fifo    = j;
      j       = next;
   }
   if (!fifo)
      return;
   if (t->done_tail)
      t->done_tail->next = fifo;
   else
      t->done_head = fifo;
   t->done_tail = last;
}
#endif

/* --- workers -------------------------------------------------------------- */

#ifdef HAVE_THREADS
/* Abort hook for a worker's decode: stop at shutdown, and once the
 * job's epoch is stale (a cancel() happened since it was queued - the
 * view moved on, so its result would be discarded anyway). */
struct ct_abort_ctx { companion_thumbs_t *t; unsigned epoch; };

static bool ct_should_abort(void *ud)
{
   struct ct_abort_ctx *a = (struct ct_abort_ctx*)ud;
   return ct_quitting(a->t)
      || (unsigned)retro_atomic_load_acquire_int(&a->t->epoch_now)
         != a->epoch;
}

static void ct_worker(void *ud)
{
   companion_thumbs_t *t = (companion_thumbs_t*)ud;
   for (;;)
   {
      struct ct_abort_ctx actx;
      struct ct_job *job;

      if (ct_quitting(t))
      {
         /* The reads' pool is this thread's, which ends here */
         data_transfer_pool_flush();
         return;
      }
      if (!(job = ct_next_job(t)))
      {
         /* nothing queued: wait for a request or for free() */
         int key = retro_eventcount_prepare_wait(&t->work_ec);
         if (ct_quitting(t) || (job = ct_next_job(t)))
            retro_eventcount_cancel_wait(&t->work_ec);
         else
         {
            retro_eventcount_commit_wait(&t->work_ec, key);
            continue;
         }
         if (!job)
            continue;
      }

      actx.t       = t;
      actx.epoch   = job->epoch;
      job->bits    = ct_decode(t, job, job->e->path, job->e->dims, job->bg,
            ct_should_abort, &actx, ct_anim_first(job->e->path));
      job->aborted = !job->bits && ct_should_abort(&actx);
      /* Landed before it is handed back: the session, if parked, is
       * already in place for a waiting animation. */
      if (retro_atomic_load_acquire_int(&job->anim) > 0)
      {
         retro_atomic_store_release_int(&job->landed, 1);
         retro_eventcount_notify(&t->anim_ec);
      }
      ct_push_done(t, job);
   }
}
#endif

/* --- animation ------------------------------------------------------------ */

#ifdef HAVE_THREADS
/* Whether the animation @gen plays on: not once another animate(), a
 * stop or free() has come. */
static bool ct_anim_stale(companion_thumbs_t *t, unsigned gen)
{
   return ct_quitting(t)
      || (unsigned)retro_atomic_load_acquire_int(&t->anim_gen_now) != gen;
}

/* The animation thread's sink: video_player decodes in line on this
 * thread and hands each frame over in the stream's canvas, which is
 * scaled - swizzled on the sampled pixels only - into the triple
 * buffer's back slot and published. */
struct ct_play
{
   companion_thumbs_t *t;
   gfx_anim_preview_t *sess;
   const char *path;
   uintptr_t tag;
   unsigned dims;
   unsigned gen;
   uint32_t bg;
};

static int ct_play_start(void *user, unsigned width, unsigned height,
      unsigned num_slots)
{
   (void)user;
   (void)width;
   (void)height;
   (void)num_slots;
   return 0; /* ARGB words */
}

static bool ct_play_feed(void *user)
{
   /* keep the window straddling the decoder's frontier */
   return gfx_anim_preview_feed(((struct ct_play*)user)->sess);
}

static enum video_player_present ct_play_present(void *user,
      const video_player_frame_t *frame)
{
   struct ct_play *pl = (struct ct_play*)user;
   struct ct_anim_frame *fr;
   uint32_t *bits;

   if (     !pl->sess->dims || !frame->pixels
         || !(bits = companion_thumbs_scale_ex(frame->pixels,
               pl->sess->dims, pl->dims, pl->bg, !frame->native)))
      return VIDEO_PLAYER_PRESENT_FAILED;
   /* The back slot is this thread's until it is published. */
   fr       = (struct ct_anim_frame*)retro_triple_buffer_back(
         &pl->t->anim_tb);
   free(fr->bits);
   fr->bits = bits;
   fr->dims = pl->dims;
   fr->gen  = pl->gen;
   fr->tag  = pl->tag;
   strlcpy(fr->path, pl->path, sizeof(fr->path));
   retro_triple_buffer_publish(&pl->t->anim_tb);
   return VIDEO_PLAYER_PRESENT_DONE;
}

/* One animation at a time, played exactly the way RetroArch's File
 * Browser plays its thumbnail: gfx_anim_preview opens the file as a
 * sliding window (frames start from the first resident bytes; a tail-
 * moov MP4 opens from a few MiB; memory admission scales with the
 * heap), feeds the window ahead of the decoder each frame, and hands
 * back frames; video_player keeps the container's clock, the loop
 * count and catch-up. This thread scales each frame into the triple
 * buffer's back slot, publishes it, and holds it until the next is
 * due - until superseded or quit. The session is published
 * (t->anim_sess) so the UI thread can start and feed the preview audio
 * through the mixer on its own ticks. */
static void ct_anim_req_free(struct ct_anim_req *req)
{
   if (!req)
      return;
   if (req->still)
   {
      retro_atomic_fetch_add_int(&req->still->anim, -1);
      ct_job_release(req->still);
   }
   free(req->path);
   free(req);
}

/* The parked session if it is @path's; one for another file goes back
 * for whoever wants it. */
static gfx_anim_preview_t *ct_parked_take(companion_thumbs_t *t,
      const char *path)
{
   gfx_anim_preview_t *sess;
   struct ct_parked   *pk = (struct ct_parked*)
      retro_atomic_exchange_ptr(&t->parked, NULL);
   if (!pk)
      return NULL;
   if (strcmp(pk->path, path))
   {
      ct_parked_free((struct ct_parked*)
            retro_atomic_exchange_ptr(&t->parked, pk));
      return NULL;
   }
   sess = pk->sess;
   free(pk->path);
   free(pk);
   return sess;
}

static void ct_anim_thread(void *ud)
{
   companion_thumbs_t *t = (companion_thumbs_t*)ud;
   for (;;)
   {
      char path[PATH_MAX_LENGTH];
      unsigned dims;
      uintptr_t tag;
      uint32_t bg;
      unsigned gen;
      gfx_anim_preview_t *sess = NULL;
      video_player_t *vp;
      video_player_sink_t sink;
      struct ct_play pl;
      struct ct_anim_req *req;

      /* a request */
      for (;;)
      {
         int key;
         if (ct_quitting(t) || retro_atomic_load_acquire_ptr(&t->anim_req))
            break;
         key = retro_eventcount_prepare_wait(&t->anim_ec);
         if (ct_quitting(t) || retro_atomic_load_acquire_ptr(&t->anim_req))
         {
            retro_eventcount_cancel_wait(&t->anim_ec);
            break;
         }
         retro_eventcount_commit_wait(&t->anim_ec, key);
      }
      if (ct_quitting(t))
         return;
      if (!(req = (struct ct_anim_req*)
               retro_atomic_exchange_ptr(&t->anim_req, NULL)))
         continue;

      strlcpy(path, req->path, sizeof(path));
      dims = req->dims;
      tag  = req->tag;
      bg   = req->bg;
      gen  = req->gen;

      /* A video's still is on its way through a worker (or already
       * parked): take that session rather than open a second one and
       * decode the same first frame in parallel with it.  Wait only
       * while that still is queued or decoding; a failed, aborted or
       * dropped one parks nothing and the wait ends with it. */
      if (req->still && ct_anim_first(path))
      {
         for (;;)
         {
            int key;
            if ((sess = ct_parked_take(t, path)))
               break;
            if (retro_atomic_load_acquire_int(&req->still->landed))
            {
               /* parked before it landed, if at all */
               sess = ct_parked_take(t, path);
               break;
            }
            if (ct_anim_stale(t, gen))
               break;
            /* The worker notifies once it has landed, after it
             * parked. */
            key = retro_eventcount_prepare_wait(&t->anim_ec);
            if (     retro_atomic_load_acquire_int(&req->still->landed)
                  || ct_anim_stale(t, gen))
               retro_eventcount_cancel_wait(&t->anim_ec);
            else
               retro_eventcount_commit_wait(&t->anim_ec, key);
         }
      }
      ct_anim_req_free(req);

      /* -1: no still-decode verdict to offer; the module probes the
       * PNG head itself. NULL: a still, or not admitted. */
      if (!sess && (ct_anim_stale(t, gen)
               || !(sess = gfx_anim_preview_open(path, -1))))
         continue;
      if (ct_anim_stale(t, gen))
      {
         gfx_anim_preview_close(sess);
         continue;
      }
      /* the UI thread may start the audio */
      retro_atomic_store_release_int(&t->anim_sess_gen, (int)gen);
      retro_atomic_store_release_ptr(&t->anim_sess, sess);

      pl.t            = t;
      pl.sess         = sess;
      pl.path         = path;
      pl.tag          = tag;
      pl.dims         = dims;
      pl.gen          = gen;
      pl.bg           = bg;
      sink.user       = &pl;
      sink.start      = ct_play_start;
      sink.slot_begin = NULL;
      sink.slot_end   = NULL;
      sink.present    = ct_play_present;
      sink.feed       = ct_play_feed;
      sink.decoded    = NULL;
      sink.start_hw   = NULL;
      sink.slot_stride = NULL;
      vp = video_player_new(sess->stream, sess->type, sess->loop_count,
            &sink, VIDEO_PLAYER_INLINE);

      while (vp && !ct_anim_stale(t, gen))
      {
         retro_time_t now = cpu_features_get_time_usec();
         retro_time_t due;
         if (video_player_poll(vp, now) == VIDEO_PLAYER_ENDED)
            break;
         /* Hold the frame until the next is due, on the player's
          * schedule rather than a sleep after each push, so decode and
          * scale time does not stretch every frame. A new animation, a
          * stop and free() all notify, which ends the hold at once. */
         due = video_player_due(vp);
         now = cpu_features_get_time_usec();
         while (now < due)
         {
            int key = retro_eventcount_prepare_wait(&t->anim_ec);
            if (ct_anim_stale(t, gen))
            {
               retro_eventcount_cancel_wait(&t->anim_ec);
               break;
            }
            retro_eventcount_commit_wait_timeout(&t->anim_ec, key,
                  due - now);
            now = cpu_features_get_time_usec();
         }
      }
      video_player_free(vp);

      /* Unpublish, and wait out a UI-thread call inside it, before
       * closing. */
      ct_sess_retire(t);
      gfx_anim_preview_close(sess);   /* audio too */
   }
}

/* Drops a parked session that is not @path's (any, with NULL). */
static void ct_parked_drop(companion_thumbs_t *t, const char *path)
{
   struct ct_parked *pk = (struct ct_parked*)
      retro_atomic_exchange_ptr(&t->parked, NULL);
   if (pk && path && !strcmp(pk->path, path))
      pk = (struct ct_parked*)retro_atomic_exchange_ptr(&t->parked, pk);
   ct_parked_free(pk);
}

/* Silence the session playing now, if any. UI thread. */
static void ct_sess_audio_stop(companion_thumbs_t *t)
{
   gfx_anim_preview_t *sess = ct_sess_acquire(t);
   if (sess)
   {
      gfx_anim_preview_audio_stop(sess);
      ct_sess_release(t);
   }
}
#endif

void companion_thumbs_animate(companion_thumbs_t *t, const char *path,
      unsigned dims, uintptr_t tag, uint32_t bg)
{
#ifdef HAVE_THREADS
   struct ct_anim_req *req;
   struct ct_entry    *e;
#endif
   if (     !t || string_is_empty(path)
         || !VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims))
      return;
#ifdef HAVE_THREADS
   if (!t->ecs_inited)
      return;
   if (!(req = (struct ct_anim_req*)calloc(1, sizeof(*req))))
      return;
   if (!(req->path = strldup(path, strlen(path) + 1)))
   {
      free(req);
      return;
   }
   ct_sess_audio_stop(t); /* the previous one, now */
   /* A session parked for the previous path is nobody's now. */
   ct_parked_drop(t, path);
   free(t->anim.path);
   t->anim.path = strldup(path, strlen(path) + 1);
   t->anim.gen++;
   req->dims    = dims;
   req->tag     = tag;
   req->bg      = bg;
   req->gen     = t->anim.gen;
   /* Its still, if one is on its way: the animation waits for it. */
   if ((e = ct_find(t, path, dims)) && e->queued && e->job)
   {
      req->still = e->job;
      retro_atomic_fetch_add_int(&req->still->refs, 1);
      retro_atomic_fetch_add_int(&req->still->anim, 1);
   }
   retro_atomic_store_release_int(&t->anim_gen_now, (int)t->anim.gen);
   retro_atomic_store_release_int(&t->anim_asked, 1);
   ct_anim_req_free((struct ct_anim_req*)
         retro_atomic_exchange_ptr(&t->anim_req, req));
   if (!t->anim_thread)
      t->anim_thread = sthread_create(ct_anim_thread, t);
   retro_eventcount_notify(&t->anim_ec);
#else
   (void)path; (void)dims; (void)tag; (void)bg;
#endif
}

void companion_thumbs_animate_stop(companion_thumbs_t *t)
{
   if (!t)
      return;
#ifdef HAVE_THREADS
   if (!t->ecs_inited)
      return;
   t->anim.gen++;
   retro_atomic_store_release_int(&t->anim_gen_now, (int)t->anim.gen);
   ct_anim_req_free((struct ct_anim_req*)
         retro_atomic_exchange_ptr(&t->anim_req, NULL));
   ct_parked_drop(t, NULL);
   /* Silence at once: the thread closes the session (and its audio)
    * at its next frame, but the mixer stream should not play on until
    * then. */
   ct_sess_audio_stop(t);
   /* Out of a frame's hold, or a wait for a still, at once. */
   retro_eventcount_notify(&t->anim_ec);
#endif
}

bool companion_thumbs_animating(companion_thumbs_t *t)
{
   if (!t)
      return false;
#ifdef HAVE_THREADS
   if (!t->ecs_inited)
      return false;
   return retro_atomic_load_acquire_ptr(&t->anim_req)
      || retro_atomic_load_acquire_ptr(&t->anim_sess) != NULL;
#else
   return false;
#endif
}

/* --- API ------------------------------------------------------------------ */

companion_thumbs_t *companion_thumbs_new(size_t budget_bytes, unsigned threads)
{
   companion_thumbs_t *t = (companion_thumbs_t*)calloc(1, sizeof(*t));
   if (!t)
      return NULL;
   t->budget   = budget_bytes ? budget_bytes : (64u * 1024 * 1024);
   if (!ct_grow(t))
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
      if (!retro_eventcount_init(&t->work_ec))
      {
         companion_thumbs_free(t);
         return NULL;
      }
      if (!retro_eventcount_init(&t->anim_ec))
      {
         retro_eventcount_free(&t->work_ec);
         companion_thumbs_free(t);
         return NULL;
      }
      t->ecs_inited = true;
      retro_triple_buffer_init(&t->anim_tb, &t->anim_frames[0],
            &t->anim_frames[1], &t->anim_frames[2]);
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

/* Every job taken back off both lanes. UI thread. */
static void ct_lanes_clear(companion_thumbs_t *t)
{
   struct ct_job *j;
   while ((j = ct_lane_take(&t->urgent, false)))
      ct_job_drop(t, j);
   while ((j = ct_lane_take(&t->prefetch, true)))
      ct_job_drop(t, j);
   ct_lane_trim(&t->urgent);
   ct_lane_trim(&t->prefetch);
}

void companion_thumbs_free(companion_thumbs_t *t)
{
   size_t i;
   if (!t)
      return;
#ifdef HAVE_THREADS
   if (t->ecs_inited)
   {
      retro_atomic_store_release_int(&t->quit, 1);
      retro_eventcount_notify(&t->work_ec);
      retro_eventcount_notify(&t->anim_ec);
      for (i = 0; i < t->nworkers; i++)
         sthread_join(t->workers[i]);
      if (t->anim_thread)
         sthread_join(t->anim_thread);
   }
   ct_anim_req_free((struct ct_anim_req*)
         retro_atomic_exchange_ptr(&t->anim_req, NULL));
   ct_parked_drop(t, NULL);
   for (i = 0; i < 3; i++)
      free(t->anim_frames[i].bits);
   ct_take_done(t);
#endif
   ct_lanes_clear(t);
   while (t->done_head)
   {
      struct ct_job *j = t->done_head;
      t->done_head     = j->next;
      free(j->bits);
      ct_job_retire(t, j);
   }
#ifdef HAVE_THREADS
   if (t->ecs_inited)
   {
      retro_eventcount_free(&t->anim_ec);
      retro_eventcount_free(&t->work_ec);
   }
#endif
   free(t->anim.path);
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
      unsigned dims)
{
   struct ct_entry *e;
   if (!t || string_is_empty(path))
      return NULL;
   e = ct_find(t, path, dims);
   if (!e || !e->bits)
      return NULL;
   /* touch */
   ct_lru_remove(t, e);
   ct_lru_push_front(t, e);
   return e->bits;
}

bool companion_thumbs_request(companion_thumbs_t *t, const char *path,
      unsigned dims, uintptr_t tag, bool urgent, uint32_t bg)
{
   struct ct_entry *e;
   struct ct_job *j;
   if (     !t || string_is_empty(path)
         || !VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims))
      return false;

   e = ct_find(t, path, dims);
   if (e && (e->bits || e->queued))
      return false;          /* cached or already on its way */
   if (!e && !(e = ct_insert(t, path, dims)))
      return false;
   if (!(j = (struct ct_job*)calloc(1, sizeof(*j))))
      return false;

   j->e     = e;
   j->tag   = tag;
   j->bg    = bg;
   j->epoch = t->epoch;
#ifdef HAVE_THREADS
   retro_atomic_int_init(&j->refs, 1);
   retro_atomic_int_init(&j->anim, 0);
   retro_atomic_int_init(&j->landed, 0);
#endif
   e->queued = true;
   e->refs++;
   e->job    = j;
   CT_COUNT_ADD(t, jobs_queued, 1);
   if (urgent)
      ct_lane_push(t, &t->urgent, j, true);
   else
      ct_lane_push(t, &t->prefetch, j, false);
#ifdef HAVE_THREADS
   if (t->ecs_inited)
      retro_eventcount_notify(&t->work_ec);
#endif
   return true;
}

void companion_thumbs_cancel(companion_thumbs_t *t)
{
   size_t i;
   if (!t)
      return;
   /* An animation waiting for a still dropped here is told so. */
   ct_lanes_clear(t);
   /* Jobs a worker already holds: release their entries' queued flag
    * too (they are from the old epoch), so those keys can be requested
    * again; when the old result lands it is cached or deduped. */
   t->epoch++;
#ifdef HAVE_THREADS
   retro_atomic_store_release_int(&t->epoch_now, (int)t->epoch);
#endif
   for (i = 0; i < t->ht_size; i++)
   {
      struct ct_entry *e = t->ht[i];
      for (; e; e = e->chain)
         e->queued = false;
   }
}

size_t companion_thumbs_poll(companion_thumbs_t *t,
      companion_thumbs_done_cb cb, void *ud, size_t max, unsigned budget_us)
{
   size_t delivered = 0;
   if (!t)
      return 0;

#ifdef HAVE_THREADS
   if (t->ecs_inited)
   {
      struct ct_anim_frame *fr;
      /* Preview audio lives on the UI thread (the mixer): start it once
       * the animation thread has published its session, and feed its
       * window every poll, as gfx_thumbnail_animate does per frame. */
      gfx_anim_preview_t *sess = ct_sess_acquire(t);
      if (sess)
      {
         unsigned gen = (unsigned)retro_atomic_load_acquire_int(
               &t->anim_gen_now);
         if ((unsigned)retro_atomic_load_acquire_int(&t->anim_sess_gen)
               == gen)
         {
            if (t->anim_audio_gen != gen)
            {
               t->anim_audio_gen = gen;
               gfx_anim_preview_audio_begin(sess);
            }
            gfx_anim_preview_audio_feed(sess);
         }
         ct_sess_release(t);
      }

      /* The newest animation frame, if it is the current animation's. */
      if (     (fr = (struct ct_anim_frame*)
               retro_triple_buffer_take(&t->anim_tb))
            && fr->bits
            && fr->gen == (unsigned)retro_atomic_load_acquire_int(
               &t->anim_gen_now))
      {
         if (cb)
            cb(ud, fr->path, fr->dims, fr->tag, fr->bits);
         delivered++;
      }
   }
   ct_take_done(t);
   (void)budget_us;
#else
   /* No workers: decode here, under the budget. */
   {
      retro_time_t end = cpu_features_get_time_usec() + budget_us;
      struct ct_job *job;
      while (     t->jobs_queued && cpu_features_get_time_usec() < end
               && (job = ct_next_job(t)))
      {
         job->bits = ct_decode(t, job, job->e->path, job->e->dims,
               job->bg, NULL, NULL, 0);
         ct_push_done(t, job);
      }
   }
#endif

   while (t->done_head && (!max || delivered < max))
   {
      struct ct_job   *j = t->done_head;
      struct ct_entry *e = j->e;
      uint32_t     *bits = j->bits;
      uintptr_t      tag = j->tag;
      if (!(t->done_head = j->next))
         t->done_tail = NULL;
      CT_COUNT_ADD(t, done_n, -1);
      if (e->refs)
         e->refs--;             /* this record's reference */
      if (j->aborted)
      {
         /* Abandoned mid-decode: nothing to deliver; forget the entry
          * unless something still points at it (a newer request, or
          * another finished record of this key). */
         ct_job_retire(t, j);
         if (!e->queued && !e->bits && !e->refs)
            ct_entry_free(t, e);
         continue;
      }
      if (j->epoch == t->epoch)
         e->queued = false; /* else a newer request owns the flag */
      ct_job_retire(t, j);
      if (bits)
      {
         /* Cached regardless of generation: the pixels are right for
          * the key; only the delivery may be stale. */
         if (e->bits)
         {
            free(bits); /* decoded twice; keep the first */
            bits = e->bits;
         }
         else
            ct_cache_put(t, e, bits);
      }
      /* Always delivered: a decode that was in flight across a
       * cancel() still lands, and the backend checks the tag against
       * its own view state (its row generation). */
      if (cb)
         cb(ud, e->path, e->dims, tag, bits);
      delivered++;
      if (!bits)
      {
         /* Undecodable: forget the entry so the file can be retried
          * later (e.g. after a download) without a stale marker -
          * once nothing else refers to it. */
         if (!e->bits && !e->queued && !e->refs)
            ct_entry_free(t, e);
      }
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
   return t ? CT_COUNT(t, jobs_queued) : 0;
}

size_t companion_thumbs_pending(companion_thumbs_t *t)
{
   size_t n;
   if (!t)
      return 0;
   /* A job counts in its next state before it stops counting in the
    * last: read in the order it moves, one on its way is never missed. */
   n  = CT_COUNT(t, jobs_queued);
   n += CT_COUNT(t, inflight);
   n += CT_COUNT(t, done_n);
#ifdef HAVE_THREADS
   if (     retro_atomic_load_acquire_ptr(&t->anim_req)
         || retro_atomic_load_acquire_int(&t->anim_asked))
      n++;
#endif
   return n;
}
