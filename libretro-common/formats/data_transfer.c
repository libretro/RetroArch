/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (data_transfer.c).
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

/* What this file implements, and what it deliberately does not.
 *
 * IMPLEMENTED
 *
 * One handle (data_transfer_t) with three ways to fill it, sharing
 * one tick-sliced surface: iterate() with a byte budget, ptr() and
 * avail() for the buffer and how much of it is real, and the
 * complete()/failed()/capped() terminals.
 *
 *  - prefix (data_transfer_open_prefix): a file whose length is known
 *    at open.  Address space for the whole length is reserved up
 *    front so the base never moves, and physical pages are committed
 *    DT_COMMIT_STEP at a time as the fill advances, in DT_READ_CHUNK
 *    reads.  commit_cap bounds the committed bytes and turns overrun
 *    into the capped() terminal, distinct from complete and failed.
 *    discard()/refill() trade residency for a re-read, so a
 *    sequential consumer holds a constant window over a file of any
 *    size.
 *
 *  - window (data_transfer_open_window): the same reservation used
 *    cyclically, for sequential consumers that loop.  A permanent
 *    head of 'keep' bytes plus a moving [wlo, whi) that feed()
 *    advances behind the consumer and extends ahead of it;
 *    rewind() on a backwards tell, grow_keep() to raise the head once
 *    the loop landing is known, peek() for positioned metadata reads
 *    that commit nothing, punch() to release inert ranges inside the
 *    head.  A short read while extending settles the handle into the
 *    same terminal the fill uses - failed() true, complete() never -
 *    and freezes the whole window surface, because two of the live
 *    call sites discard the return value.
 *
 *  - source (data_transfer_open_source): bytes from a producer
 *    callback instead of a file, into one exact malloc that
 *    source_detach() hands to the caller to free().
 *
 * Plus data_transfer_arena_*, an independent realloc-doubling
 * growable buffer that shares this file and header but touches no
 * data_transfer_t and no page machinery.
 *
 * File access goes through filestream/VFS throughout, so 64-bit
 * offsets and VFS-only paths work.  Where the platform cannot reserve
 * address space, every mode degrades to a plain allocation of
 * min(len, commit_cap) - the whole file when there is no cap - and a
 * window simply holds the whole file; reserve_supported() and
 * window_is_reserved() say which happened.
 *
 * NOT IMPLEMENTED
 *
 *  - Asynchrony.  Every read is a synchronous filestream_read on the
 *    calling thread: iterate() blocks for the length of its budget,
 *    and there is nothing in flight for free() to cancel.  Pacing is
 *    by byte budget and the caller does the time-slicing.  The module
 *    began as a wrapper over nbio, which is where its vocabulary came
 *    from; no nbio strategy exists here, nbio is no longer part of
 *    the RetroArch build, and the nbio_ names surviving in the task
 *    layer are historical only.
 *
 *  - Locking.  Nothing is synchronised.  The window contract is
 *    single owner, single thread by construction (see the header);
 *    prefix and source handles are merely unguarded, so one owner
 *    iterates and any other reader of ptr() is ordered by the caller.
 *
 *  - Writing, in any form: no create, no append, no truncate.
 *
 *  - Length changes.  len is fixed at open.  A file that grows is
 *    never noticed; a file that shrinks under the fill freezes the
 *    transfer into failed() with an honest avail(), and complete()
 *    never becomes true.
 *
 *  - Empty inputs.  open_prefix rejects a zero or unknown size and
 *    open_source rejects len == 0, both returning NULL rather than a
 *    handle that is complete on arrival.
 *
 *  - Random access.  avail() is monotonic and the consumer reads the
 *    front of the buffer: no seek, no consumer-visible positioned
 *    read outside window_peek(), and deliberately no accessor for the
 *    window's committed frontier.
 *
 *  - Meaningful prefix behaviour on a window.  iterate(), discard()
 *    and refill() are declined on a window handle rather than run
 *    against it - see the note above iterate() for what they used to
 *    do - so mixing the surfaces is inert rather than corrupting.
 *    What is still absent is any use for them there: avail() is the
 *    full length from open, and complete() reports whole-file
 *    residency, so it stays false on a reserved window and is true on
 *    the no-reservation fallback.  Drive a window with feed/extend/
 *    advance/rewind.
 *
 *  - Restoration of the exact state a failed extension found.  What
 *    settling does instead is release everything above the frontier,
 *    which is not the same thing but is the part that matters: it is
 *    the invariant keep <= wlo <= whi that makes [whi, map_len) out
 *    of contract, so taking it back is exact for extend() and
 *    grow_keep() alike and needs no record of what was resident
 *    before.  Pages below the frontier keep whatever they hold, and
 *    a settled window is terminal anyway; free it.
 *
 *  - Cancellation and progress reporting.  No cancel token, no
 *    progress callback, no time budget.  A producer can only stop
 *    itself by returning negative, an abandoned handle is freed
 *    rather than aborted, and a source transfer that stopped short
 *    cannot be adopted at all: detach() returns NULL unless the fill
 *    completed.
 *
 *  - A promise from reserve_supported().  It answers a build
 *    question - whether this build can reserve at all - not an
 *    allocation one.  memreserve() can still fail for a particular
 *    length, and only window_is_reserved() on an open handle reports
 *    that it did.
 *
 *  - Arithmetic hardening beyond the caller-supplied ranges.  The two
 *    entry points that take a range from the caller - window_peek()
 *    and window_punch() - bound it, and the arena's doubling no
 *    longer wraps, but the rest still trusts its inputs: every
 *    rounding here assumes a power-of-two page, and window_feed()'s
 *    tell + lookahead can still wrap.  That last one is left alone
 *    deliberately - a wrapped lookahead loses read-ahead, whereas
 *    clamping it to the length would turn an absurd argument into a
 *    whole-file commit, which is the worse of the two.
 *
 *  - A reason to pass NULL.  Every entry point tolerates it - bool
 *    returns false, pointer returns NULL, void does nothing - so the
 *    surface is uniform, but nothing here treats a NULL handle as
 *    meaningful.  It is an accepted mistake, not an idiom.
 *
 *  - A runtime choice of strictness.  DT_STRICT (DT_WINDOW_STRICT is
 *    an alias) is build-time.  It now covers discard() as well as the
 *    window decommits, so the prefix streaming path gets the same
 *    fault-instead-of-zeros diagnostic under a strict build, which is
 *    what makes a consumer's look-back margin testable rather than
 *    assumed.  The release in the settle path is strict in every
 *    build and does not depend on it.
 *
 *  - Anything about the bytes themselves: no hashing, no validation,
 *    no decompression (source mode's producer does that), and nothing
 *    to do with a network, the name notwithstanding.
 */

#include <stdio.h>
#include <stdlib.h>

#include <memmap.h>
#include <streams/file_stream.h>
#include <string.h>
#include <formats/data_transfer.h>

/* Prefix strategy: commit physical pages in steps of this as the
 * fill advances (one protection syscall per step). */
#define DT_COMMIT_STEP  (1024 * 1024)
/* Read granularity inside iterate. */
#define DT_READ_CHUNK   (64 * 1024)

/* ---- reservation pool ----
 *
 * Reserving and releasing per load is not free, and the cost is not
 * the syscalls: measured, memreserve and memcommit are well under a
 * microsecond each, memrelease about fifteen, and the rest - 140 us
 * for a 512 KiB load, over a millisecond for 4 MiB - is first-touch
 * faults on fresh anonymous pages.  A recycled reservation has none,
 * because its pages are already resident.
 *
 * The guard is what makes recycling delicate: a slot handed back must
 * be unreadable again before it is handed out, or the next consumer
 * inherits a readable buffer and a read past avail() returns the
 * previous file's bytes.  memdecommit() would do that, but it also
 * frees the pages, which is the cost being avoided.  memrearm() is
 * the mprotect without the madvise, so the slot comes back armed and
 * still resident.  Measured against a fresh reservation: 16.1 us
 * against 171.0 for 512 KiB, 973.8 against 9531.5 for 16 MiB.
 *
 * A slot is only usable for a file that fits in it, so the size is
 * the point.  DT_POOL_SLOT is NBIO_SMALL_FILE_THRESHOLD, and for the
 * same reason: it is the size below which a load finishes in one
 * tick, which is every thumbnail.  A file larger than a slot takes a
 * fresh reservation, where the pool would not have paid anyway.
 *
 * Residency is DT_POOL_SLOT * DT_POOL_SLOTS per thread that uses one,
 * held between loads.  That is the whole cost, it is bounded, and
 * data_transfer_pool_flush() gives it back.
 *
 * Thread-local rather than shared, so this module still synchronises
 * nothing (see the scope comment).  A slot opened on one thread and
 * freed on another simply migrates; ownership of a handle is
 * exclusive either way, and a pool that overflows releases instead.
 * Build with DT_NO_POOL to compile it out entirely. */
#if !defined(DT_NO_POOL)
#define DT_POOL_SLOT    (1024 * 1024)
#define DT_POOL_SLOTS   4

/* Some Darwin deployment targets have no thread-local storage: clang
 * rejects __thread outright below macOS 10.7, iOS 9 on 32-bit devices
 * (iOS 8 on 64-bit), iOS 10 in the 32-bit simulator, and watchOS 2
 * (clang/lib/Basic/Targets/OSTargets.h).  This is a property of the
 * -m*-version-min value, not of the architecture: the libretro ios9
 * platform trips it because it builds armv7 with
 * -miphoneos-version-min=8.0, while the same armv7 with a 9.0 minimum
 * compiles __thread fine.  clang's __has_feature(tls) reports exactly
 * this predicate, so ask it; without TLS the pool would be
 * unsynchronised shared state, which this module promises not to
 * have, so it is compiled out instead -- every load then falls
 * through to a fresh reservation, the same behaviour as a pool miss.
 * (GCC also answers __has_feature(tls), unconditionally true from
 * GCC 14, which is correct there: GCC lowers __thread through emutls
 * on targets without native TLS.) */
#if defined(__has_feature)
#if !__has_feature(tls)
#define DT_NO_POOL
#endif
#endif
#endif

#if !defined(DT_NO_POOL)

#if !defined(HAVE_THREADS)
#define DT_TLS
#elif defined(_MSC_VER)
#define DT_TLS __declspec(thread)
#else
#define DT_TLS __thread
#endif

static DT_TLS uint8_t *dt_pool[DT_POOL_SLOTS];
static DT_TLS size_t   dt_pool_slot;   /* slot length, page-rounded */
static DT_TLS int      dt_pool_n;

/* A slot, already armed: either recycled or freshly reserved.  NULL
 * if the platform cannot reserve or the reservation was refused. */
static uint8_t *data_transfer_pool_take(size_t pg, size_t *slot_len)
{
   size_t slot = ((size_t)DT_POOL_SLOT + pg - 1) & ~(pg - 1);
   if (dt_pool_n > 0 && dt_pool_slot == slot)
   {
      *slot_len = slot;
      return dt_pool[--dt_pool_n];
   }
   {
      void *m = memreserve(slot);
      if (!m)
         return NULL;
      *slot_len = slot;
      return (uint8_t*)m;
   }
}

/* Re-arm and keep, or decline and let the caller release. */
static bool data_transfer_pool_give(uint8_t *m, size_t slot)
{
   if (dt_pool_n >= DT_POOL_SLOTS)
      return false;
   if (dt_pool_n > 0 && dt_pool_slot != slot)
      return false;                /* different page size: not ours */
   if (!memrearm(m, slot))
      return false;
   dt_pool_slot          = slot;
   dt_pool[dt_pool_n++]  = m;
   return true;
}

void data_transfer_pool_flush(void)
{
   while (dt_pool_n > 0)
      memrelease(dt_pool[--dt_pool_n], dt_pool_slot);
}
#else
void data_transfer_pool_flush(void) { }
#endif


struct data_transfer
{
   /* file-backed reader (open_prefix, and open_window over it)      */
   RFILE  *f;
   uint8_t *map;     /* reserved (or fallback-allocated) buffer      */
   size_t  map_len;  /* reserved bytes (page-rounded), 0 = fallback  */
   size_t  committed;/* bytes with physical backing                  */
   size_t  cap;      /* commit ceiling (buffer size in fallback)     */
   size_t  len;      /* full file length, fixed at open              */
   size_t  avail;    /* bytes valid at the front; monotonic          */
   size_t  low;      /* lowest readable offset after discards       */
   size_t  page;     /* page size for the discard granularity        */
   uint8_t done;     /* the operation is over                        */
   uint8_t failed;   /* ...but delivered less than the file          */
   uint8_t capped;   /* stopped at the commit ceiling                */
   /* window mode (open_window): cyclic streaming state */
   uint8_t pooled;   /* map came from the slot pool                  */
   uint8_t window;   /* this dt is a cyclic window                   */
   size_t  keep;     /* head bytes always resident                   */
   size_t  wlo, whi; /* the moving window [wlo, whi)                 */
   size_t  wtell;    /* last consumer position seen by feed()        */
   data_transfer_source_read_t src_cb; /* producer-backed transfer   */
   void   *src_ud;
   size_t  wfreed;   /* page-aligned decommit frontier: everything in
                        [keep-page-ceil, wfreed) is released.  Kept
                        across calls because the feeder advances in
                        sub-page steps - per-call rounding of a tiny
                        interval decommits nothing, ever.            */
   size_t  scrubbed; /* pooled slots only: every byte below this is
                        accounted for - overwritten by the fill or
                        zeroed - so a consumer over-reading past
                        avail() cannot see the previous load.  See
                        data_transfer_pool_scrub().                  */
};

bool data_transfer_arena_init(data_transfer_arena_t *a, size_t ceiling)
{
   /* The ceiling is a hint about the eventual size, not a promise to
    * reserve it. Growth is realloc doubling, which is portable and,
    * measured against a reserve-then-commit implementation, neither
    * slower in the steady state nor heavier: untouched pages of a
    * fresh allocation are no more resident than uncommitted ones. */
   (void)ceiling;
   a->base      = NULL;
   a->committed = 0;
   a->cap       = 0;
   return true;
}

bool data_transfer_arena_ensure(data_transfer_arena_t *a, size_t need)
{
   size_t nc;
   uint8_t *nb;

   if (need <= a->committed)
      return true;

   nc = a->cap ? a->cap : (256 * 1024);
   while (nc < need)
   {
      size_t nx = nc * 2;
      /* No power of two at or above 'need' fits in a size_t: ask for
       * exactly what was wanted and let realloc refuse it.  Left
       * unchecked the doubling reaches 0 and the loop never ends. */
      if (nx <= nc)
      {
         nc = need;
         break;
      }
      nc = nx;
   }
   if (!(nb = (uint8_t*)realloc(a->base, nc)))
      return false;
   a->base      = nb;
   a->cap       = nc;
   a->committed = nc;
   return true;
}

void data_transfer_arena_release(data_transfer_arena_t *a)
{
   free(a->base);
   a->base      = NULL;
   a->committed = 0;
   a->cap       = 0;
}


/* ---- cyclic streaming window ---- */

static int data_transfer_read_at(data_transfer_t *dt, size_t off,
      uint8_t *dst, size_t n);
/* open_prefix's body.  allow_pool is 0 for open_window, which builds
 * on a prefix handle but reads map_len in punch/peek and so needs it
 * to be the file's own rounded length rather than a pool slot's. */
static data_transfer_t *data_transfer_open_prefix_ex(const char *path,
      size_t commit_cap, int allow_pool);
/* The fill proper.  data_transfer_iterate() refuses to run it on a
 * window; open_window's no-reservation path genuinely needs it and
 * calls it here directly. */
static size_t data_transfer_prefix_iterate(data_transfer_t *dt,
      size_t max_bytes, data_transfer_continue_t cb, void *ud);

/* Release whole pages from the decommit frontier up to (not
 * including) the page containing 'to'.  The frontier survives calls,
 * so sub-page feeder steps accumulate into page releases. */
static void data_transfer_wdecommit_to(data_transfer_t *dt, size_t to)
{
   size_t from = dt->wfreed;
   to   = (to / dt->page) * dt->page;
   if (to <= from || !dt->map_len)
      return;
   dt->wfreed = to;
#ifdef DT_STRICT
   memdecommit(dt->map + from, to - from, true);
#else
   memdecommit(dt->map + from, to - from, false);
#endif
}

/* Settle a window into the same terminal the fill uses: failed() true,
 * complete() never, and every window mutator inert from here.
 *
 * The freeze is the point.  Two of the live call sites discard the
 * return - the audio mixer raises the head to cover the decoder's loop
 * landing and feeds the window each tick without looking - so the
 * value alone was never going to be noticed.  A settled handle stops
 * committing pages it cannot fill, stops moving wlo/whi/wfreed away
 * from what is actually resident, and answers failed() to anyone who
 * asks later.
 *
 * What it does not do is undo the commit that just failed.  Rolling
 * one back is exact for extend(), whose range is always above the
 * frontier, but not for grow_keep(), whose range can overlap both the
 * live window and the span advance() already released - the state
 * kept here cannot say which pages were resident before the call.  A
 * rollback that is right in one case and wrong in the other is worse
 * than none, so the pages keep whatever they hold and the handle is
 * terminal: free it. */
static void data_transfer_wfail(data_transfer_t *dt)
{
   dt->done   = 1;
   dt->failed = 1;
   /* Take back everything above the frontier.  The failed call had
    * already committed its range, and committed-but-unfilled pages
    * read as zeros where every other byte past the frontier faults -
    * a hole in the one guarantee the mode rests on.
    *
    * Restoring the exact pre-call state is not expressible from what
    * is kept here, but that is the wrong target.  The invariant is
    * simply that nothing above whi is readable: the head is
    * [0, keep), the window is [wlo, whi), and keep <= wlo <= whi
    * always, so [whi, map_len) is out of contract by construction.
    * Releasing it is a no-op on a healthy window and a repair on a
    * failed one, in both the extend() and grow_keep() cases.
    *
    * Strict unconditionally, not just under DT_STRICT: the handle is
    * terminal, so a fault here is a caller reading what it was told
    * not to, and that is worth surfacing in any build. */
   if (dt->map_len)
   {
      size_t f = (dt->whi + dt->page - 1) & ~(dt->page - 1);
      if (f < dt->map_len)
         memdecommit(dt->map + f, dt->map_len - f, true);
   }
}

static int data_transfer_wcommit(data_transfer_t *dt,
      size_t from, size_t to)
{
   size_t f = (from / dt->page) * dt->page;
   size_t t = (to + dt->page - 1) & ~(dt->page - 1);
   if (t > dt->map_len)
      t = dt->map_len;
   if (t <= f)
      return 1;
   if (!dt->map_len)
      return 1;                  /* fallback: whole file resident */
   if (!memcommit(dt->map + f, t - f))
      return 0;
   return 1;
}

data_transfer_t *data_transfer_open_source(size_t len,
      data_transfer_source_read_t read_cb, void *ud)
{
   data_transfer_t *dt;
   if (!len || !read_cb)
      return NULL;
   if (!(dt = (data_transfer_t*)calloc(1, sizeof(*dt))))
      return NULL;
   dt->len       = len;
   dt->src_cb    = read_cb;
   dt->src_ud    = ud;
   dt->committed = len;   /* the exact buffer exists in full: the
                           * fill's commit step must be a no-op */
   /* exact plain buffer: no reservation, adoptable via free() */
   if (!(dt->map = (uint8_t*)malloc(len)))
   {
      free(dt);
      return NULL;
   }
   return dt;
}

uint8_t *data_transfer_source_detach(data_transfer_t *dt, size_t *len)
{
   uint8_t *out;
   if (!dt || !dt->src_cb || !dt->done || dt->failed)
      return NULL;
   out     = dt->map;
   if (len)
      *len = dt->len;
   dt->map = NULL;               /* free() must not release it */
   data_transfer_free(dt);
   return out;
}

data_transfer_t *data_transfer_open_window(const char *path, size_t keep)
{
   data_transfer_t *dt = data_transfer_open_prefix_ex(path, 0, 0);
   if (!dt)
      return NULL;
   dt->window = 1;
   if (keep > dt->len)
      keep = dt->len;
   dt->keep   = keep;
   dt->wlo    = keep;
   dt->whi    = keep;
   dt->wfreed = dt->page
         ? ((keep + dt->page - 1) & ~(dt->page - 1)) : keep;
   /* the head is resident from the start */
   if (!data_transfer_wcommit(dt, 0, keep)
         || !data_transfer_read_at(dt, 0, dt->map, keep))
   {
      data_transfer_free(dt);
      return NULL;
   }
   if (!dt->map_len)
   {
      /* no reservation on this platform: the fallback path of
       * open_prefix holds a plain buffer - fill it all; the window
       * calls become no-ops and the file is simply resident.  The
       * fill is invoked directly because dt->window is already set
       * and the public iterate() declines windows. */
      data_transfer_prefix_iterate(dt, 0, NULL, NULL);
      if (!data_transfer_complete(dt))
      {
         data_transfer_free(dt);
         return NULL;
      }
   }
   dt->avail = dt->len;          /* the consumer sees the full length */
   return dt;
}

bool data_transfer_reserve_supported(void)
{
   return mempagesize() != 0;
}

bool data_transfer_window_is_reserved(data_transfer_t *dt)
{
   return dt && dt->window && dt->map_len != 0;
}

const uint8_t *data_transfer_window_base(data_transfer_t *dt, size_t *len)
{
   if (!dt)
   {
      if (len)
         *len = 0;
      return NULL;
   }
   if (len)
      *len = dt->len;
   return dt->map;
}

bool data_transfer_window_extend(data_transfer_t *dt, size_t hi)
{
   if (!dt || !dt->window || dt->failed)
      return false;
   if (hi > dt->len)
      hi = dt->len;
   if (hi <= dt->whi)
      return true;
   if (!dt->map_len)
      return true;               /* fallback: already whole */
   /* A refused commit settles too.  The fill has a capped() terminal
    * for it because a prefix has a ceiling to reach; a window has
    * none, so running out of pages to commit is simply the window
    * failing. */
   if (!data_transfer_wcommit(dt, dt->whi, hi))
   {
      data_transfer_wfail(dt);
      return false;
   }
   if (!data_transfer_read_at(dt, dt->whi, dt->map + dt->whi,
         hi - dt->whi))
   {
      data_transfer_wfail(dt);
      return false;
   }
   dt->whi = hi;
   return true;
}

bool data_transfer_window_ensure(data_transfer_t *dt, size_t lo,
      size_t hi)
{
   if (!dt || !dt->window || dt->failed)
      return false;
   if (hi > dt->len)
      hi = dt->len;
   if (lo >= hi)
      return true;
   if (!dt->map_len)
      return true;               /* fallback: whole file resident */
   if (!data_transfer_wcommit(dt, lo, hi))
   {
      data_transfer_wfail(dt);
      return false;
   }
   if (!data_transfer_read_at(dt, lo, dt->map + lo, hi - lo))
   {
      data_transfer_wfail(dt);
      return false;
   }
   return true;
}

void data_transfer_window_rebase(data_transfer_t *dt, size_t pos)
{
   size_t p;
   if (!dt || !dt->window || !dt->map_len || dt->failed)
      return;
   if (pos > dt->len)
      pos = dt->len;
   p = (pos / dt->page) * dt->page;
   if (p <= dt->whi)
      return;                    /* frontier already covers it */
   dt->wlo    = p;
   dt->whi    = p;
   dt->wfreed = p;
   return;
}

void data_transfer_window_advance(data_transfer_t *dt, size_t lo)
{
   if (!dt || !dt->window || !dt->map_len || dt->failed)
      return;
   if (lo > dt->whi)
      lo = dt->whi;
   if (lo <= dt->wlo)
      return;
   data_transfer_wdecommit_to(dt, lo);
   dt->wlo = lo;
}

void data_transfer_window_rewind(data_transfer_t *dt)
{
   if (!dt || !dt->window || !dt->map_len || dt->failed)
      return;
   /* drop the old window entirely (frontier through its end; the
    * partial last page goes with the round-up), then restart the
    * frontier at the head boundary for the next lap */
   data_transfer_wdecommit_to(dt,
         (dt->whi + dt->page - 1) & ~(dt->page - 1));
   dt->wlo    = dt->keep;
   dt->whi    = dt->keep;
   dt->wfreed = (dt->keep + dt->page - 1) & ~(dt->page - 1);
}

bool data_transfer_window_grow_keep(data_transfer_t *dt, size_t keep)
{
   if (!dt || !dt->window || dt->failed)
      return false;
   if (keep > dt->len)
      keep = dt->len;
   if (keep <= dt->keep)
      return true;
   if (dt->map_len)
   {
      /* commit and read the extension; pages already inside the
       * window are re-read harmlessly */
      if (!data_transfer_wcommit(dt, dt->keep, keep))
      {
         data_transfer_wfail(dt);
         return false;
      }
      if (!data_transfer_read_at(dt, dt->keep, dt->map + dt->keep,
            keep - dt->keep))
      {
         data_transfer_wfail(dt);
         return false;
      }
   }
   dt->keep = keep;
   if (dt->wlo < keep)
      dt->wlo = keep;
   if (dt->whi < keep)
      dt->whi = keep;
   if (dt->wfreed < ((keep + dt->page - 1) & ~(dt->page - 1)))
      dt->wfreed = (keep + dt->page - 1) & ~(dt->page - 1);
   return true;
}

bool data_transfer_window_peek(data_transfer_t *dt, size_t off,
      void *dst, size_t n)
{
   /* off + n is computed as a subtraction: the sum can wrap, and a
    * wrapped sum reads as comfortably in range. */
   if (!dt || !dt->window || dt->failed
         || off > dt->len || n > dt->len - off)
      return false;
   if (!dt->map_len)
   {
      /* fallback: whole file resident - copy out */
      memcpy(dst, dt->map + off, n);
      return true;
   }
   return data_transfer_read_at(dt, off, (uint8_t*)dst, n) != 0;
}

void data_transfer_window_punch(data_transfer_t *dt, size_t from,
      size_t to)
{
   size_t f, t;
   if (!dt || !dt->window || !dt->map_len || dt->failed)
      return;
   /* Bound the range to the reservation before rounding it.  'to' is
    * caller-supplied and unclamped everywhere else here, and a
    * non-strict decommit is madvise(MADV_DONTNEED): it does not fail
    * politely on a range running off the end of the mapping, it
    * zeroes whatever anonymous pages it does cover, which past this
    * reservation is somebody else's heap.  The round-up of 'from' is
    * only safe once from is bounded too. */
   if (from > dt->map_len)
      return;
   if (to > dt->map_len)
      to = dt->map_len;
   f = (from + dt->page - 1) & ~(dt->page - 1);
   t = (to / dt->page) * dt->page;
   if (t <= f)
      return;
#ifdef DT_STRICT
   memdecommit(dt->map + f, t - f, true);
#else
   memdecommit(dt->map + f, t - f, false);
#endif
}

bool data_transfer_window_feed_budget(data_transfer_t *dt, size_t tell,
      size_t lookahead, size_t margin, size_t budget,
      size_t *resident_hi)
{
   bool ok;
   size_t hi;
   if (resident_hi)
      *resident_hi = 0;
   if (!dt || !dt->window || dt->failed)
      return false;
   if (tell < dt->wtell)
      data_transfer_window_rewind(dt);   /* the consumer looped */
   dt->wtell = tell;
   if (tell > margin && tell - margin > dt->wlo)
      data_transfer_window_advance(dt, tell - margin);
   hi = tell + lookahead;
   /* The ceiling applies only while the frontier covers the consumer.
    * A frontier behind tell means the consumer's next read lands on
    * unresident pages, and pacing THAT read over ticks is not a
    * smaller burst, it is a fault: close the gap in one extend, as
    * the unbudgeted feed always has. */
   if (budget && dt->whi >= tell)
   {
      size_t cap = dt->whi + budget;
      if (cap < dt->whi)                 /* wrapped: no ceiling */
         cap = (size_t)-1;
      if (hi > cap)
         hi = cap;
   }
   ok = data_transfer_window_extend(dt, hi);
   if (resident_hi)
      *resident_hi = dt->map_len ? dt->whi : dt->len;
   return ok;
}

bool data_transfer_window_feed(data_transfer_t *dt, size_t tell,
      size_t lookahead, size_t margin)
{
   return data_transfer_window_feed_budget(dt, tell, lookahead,
         margin, 0, NULL);
}

static data_transfer_t *data_transfer_open_prefix_ex(const char *path,
      size_t commit_cap, int allow_pool)
{
   data_transfer_t *dt;
   RFILE *f;
   int64_t l;
   /* filestream: 64-bit lengths and offsets everywhere (a plain long
    * ftell caps at 2 GB on LLP64 and 32-bit targets), and the read
    * routes through the VFS interface when one is registered, so
    * paths only the VFS can open - archive members, Android
    * content:// documents, frontend overrides - work like any
    * other. */
   if (!(f = filestream_open(path, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE)))
      return NULL;
   if ((l = filestream_get_size(f)) <= 0)
   {
      filestream_close(f);
      return NULL;
   }
   if (!(dt = (data_transfer_t*)calloc(1, sizeof(*dt))))
   {
      filestream_close(f);
      return NULL;
   }
   dt->f   = f;
   dt->len = (size_t)l;
   dt->cap = commit_cap;

   {
      size_t pg = mempagesize();

      /* page is used to round offsets whether or not a reservation
       * happened, and several of those roundings divide by it, so it
       * must be non-zero even on platforms that cannot reserve.
       * mempagesize() reports 0 there; 4096 is the conventional
       * stand-in and only affects arithmetic, since without map_len
       * nothing is committed or decommitted. */
      dt->page = pg ? pg : 4096;

      if (pg)
      {
         size_t rl = ((dt->len + pg - 1) / pg) * pg;
         void  *m  = NULL;

#if !defined(DT_NO_POOL)
         /* A file that fits a slot takes one: same reservation, same
          * guard, without the fresh-page faults.  map_len is then the
          * slot rather than the rounded file length, which the prefix
          * surface does not mind - every offset it derives is bounded
          * by avail, which is bounded by len.  Declined for windows,
          * whose punch/peek bounds do read map_len. */
         if (allow_pool && rl <= (((size_t)DT_POOL_SLOT + pg - 1)
                  & ~(pg - 1)))
         {
            size_t slot = 0;
            if ((m = data_transfer_pool_take(pg, &slot)))
            {
               rl          = slot;
               dt->pooled  = 1;
            }
         }
#else
         (void)allow_pool;
#endif
         if (!m)
            m = memreserve(rl);

         if (m)
         {
            dt->map     = (uint8_t*)m;
            dt->map_len = rl;
         }
      }
   }
   if (!dt->map)
   {
      /* No reservation (32-bit address space, or a platform without
       * virtual memory): a plain allocation.  The cap is the buffer
       * when the caller set one; with no cap it is the whole file.
       *
       * There used to be a built-in 32 MiB ceiling for the uncapped
       * case, so a caller that never asked for a cap got capped()
       * anyway - and every console target takes this path, because
       * none of them has mman.  A file past the ceiling failed after
       * reading 32 MiB of it, and open_window failed outright, since
       * its no-reservation degradation fills the file and requires
       * complete(): background music over 32 MiB did not play on any
       * of them.
       *
       * Sizing to the file lets the allocator answer instead.  Never
       * worse - a file too big for memory failed before too, just
       * later and after 32 MiB of pointless I/O - and better wherever
       * the file would have fitted.  A caller wanting a bound still
       * passes commit_cap, so capped() now means the caller's ceiling
       * and nothing else. */
      size_t w = dt->cap ? dt->cap : dt->len;
      if (w > dt->len)
         w = dt->len;
      dt->cap = w;
      if (!(dt->map = (uint8_t*)malloc(w ? w : 1)))
      {
         filestream_close(f);
         free(dt);
         return NULL;
      }
      dt->committed = w;
   }
   return dt;
}

data_transfer_t *data_transfer_open_prefix(const char *path,
      size_t commit_cap)
{
   return data_transfer_open_prefix_ex(path, commit_cap, 1);
}

/* Grow the physical backing to cover at least 'need' bytes.
 *
 * Only the new span is committed, not the whole prefix from base.
 * The difference is not performance - mprotect short-circuits a range
 * whose flags already match, and the two spellings measured the same
 * on a warm run - it is that committing from base re-arms every page
 * below the frontier, including the ones discard() just released.
 *
 * That silently undid the one guarantee discard() rests on.  Under
 * DT_STRICT a look-back at a discarded byte faulted immediately after
 * the discard and then stopped faulting as soon as the fill crossed
 * the next DT_COMMIT_STEP boundary, reading as a zero instead - the
 * exact silent-zeros failure the header argues against elsewhere, and
 * invisible to a test that discards only after the fill is over.  On
 * Windows the re-commit also brought the physical pages back, so a
 * consumer discarding behind its read position held the whole file
 * anyway.
 *
 * dt->committed only ever grows and 'need' is above it here, so the
 * span is non-empty and never overlaps anything already released. */
static int data_transfer_commit(data_transfer_t *dt, size_t need)
{
   size_t target;
   if (need <= dt->committed)
      return 1;
   if (!dt->map_len)         /* fallback buffer: fixed */
      return 0;
   target = dt->committed + DT_COMMIT_STEP;
   if (target < need)
      target = need;
   if (target > dt->map_len)
      target = dt->map_len;
   /* Never past the file.  map_len can exceed the rounded length on a
    * pooled slot, and arming slot space beyond the file is both
    * pointless and, below, more to scrub. */
   if (dt->page)
   {
      size_t end = (dt->len + dt->page - 1) & ~(dt->page - 1);
      if (target > end)
         target = end;
   }
   if (target > dt->map_len)
      target = dt->map_len;
   if (!memcommit(dt->map + dt->committed, target - dt->committed))
      return 0;
   dt->committed = target;
   return 1;
}

#if !defined(DT_NO_POOL)
/* A recycled slot's pages still hold the previous load's bytes, where
 * a fresh reservation's are zero - and consumers can tell.  Without a
 * scrub, a thumbnail over-reading its own buffer got the previous
 * thumbnail rather than zeros: a leak across loads that the pool
 * would have introduced by itself.
 *
 * This used to be a memset in data_transfer_commit() over the whole
 * newly armed span, which for the pool's own population - files under
 * a slot, armed in full by the first commit - zeroed the entire file
 * length and then read the file over it.  Measured at 8 us for a
 * 512 KiB thumbnail against ~30 us for the whole open+read: a quarter
 * of the transfer spent scrubbing bytes the very next reads
 * overwrite.
 *
 * Scrub lazily instead, at the moments the consumer can actually
 * look.  The consumer only sees the buffer between iterate() calls
 * (the budget callback receives counts, not the pointer), so zeroing
 * [max(avail, scrubbed), committed) as the fill returns keeps the
 * guarantee exact: below avail is the current file, from avail to
 * committed is zeros, past committed faults.  'scrubbed' is the
 * high-water mark of bytes either overwritten or zeroed, so nothing
 * is scrubbed twice; a fill that completes in one call - every
 * thumbnail, which is what the slot size is chosen for - scrubs only
 * the sub-page tail above the file length, at most a page.
 *
 * Nothing is needed at free: handing a slot back re-arms it
 * unreadable, and the next consumer's own exit scrub governs what its
 * over-reads say. */
static void data_transfer_pool_scrub(data_transfer_t *dt)
{
   size_t from;
   if (!dt->pooled || dt->committed <= dt->avail)
      return;
   from = (dt->scrubbed > dt->avail) ? dt->scrubbed : dt->avail;
   if (from >= dt->committed)
      return;
   memset(dt->map + from, 0, dt->committed - from);
   dt->scrubbed = dt->committed;
}
#endif

static size_t data_transfer_prefix_iterate(data_transfer_t *dt,
      size_t max_bytes, data_transfer_continue_t cb, void *ud)
{
   size_t start = dt->avail;
   while (!dt->done && !dt->capped)
   {
      size_t chunk = DT_READ_CHUNK, got;
      if (dt->avail >= dt->len)
      {
         dt->done = 1;
         break;
      }
      if (dt->cap && dt->avail >= dt->cap)
      {
         dt->capped = 1;
         break;
      }
      /* Asked before each read, the first one included: a caller
       * already out of budget does no work at all.  A stop here
       * settles nothing - the fill simply pauses and the next
       * iterate carries on from the same place. */
      if (cb && !cb(ud, dt->avail, dt->len))
         break;
      if (chunk > dt->len - dt->avail)
         chunk = dt->len - dt->avail;
      if (dt->cap && chunk > dt->cap - dt->avail)
         chunk = dt->cap - dt->avail;
      if (!data_transfer_commit(dt, dt->avail + chunk))
      {
         dt->capped = 1;   /* commit refused: treat as the ceiling */
         break;
      }
      if (dt->src_cb)
      {
         /* producer: n is a pacing hint; production up to the room
          * remaining is legitimate (chunk-granular decoders) */
         int64_t r = dt->src_cb(dt->src_ud, dt->map + dt->avail,
               chunk);
         if (r < 0)
         {
            dt->done   = 1;
            dt->failed = 1;
            break;
         }
         got = (size_t)r;
         if (got > dt->len - dt->avail)
         {
            /* produced past the declared end: broken producer */
            dt->done   = 1;
            dt->failed = 1;
            break;
         }
         dt->avail += got;
         if (got == 0)
         {
            if (dt->avail < dt->len)
            {
               dt->done   = 1;
               dt->failed = 1;   /* ended short: frozen honestly */
            }
            break;
         }
         if (max_bytes && dt->avail - start >= max_bytes)
            break;
         continue;
      }
      {
         int64_t r = filestream_read(dt->f, dt->map + dt->avail,
               (int64_t)chunk);
         got = r > 0 ? (size_t)r : 0;
      }
      dt->avail += got;
      if (got < chunk)
      {
         /* The file ended short of its opening length (I/O error,
          * the file shrank): frozen honestly, never complete. */
         dt->done   = 1;
         dt->failed = 1;
         break;
      }
      if (max_bytes && dt->avail - start >= max_bytes)
         break;
   }
   if (dt->avail >= dt->len && !dt->failed)
      dt->done = 1;
#if !defined(DT_NO_POOL)
   /* Every return from the fill is a point the consumer may read:
    * make the armed-but-unfilled span answer zeros before then. */
   data_transfer_pool_scrub(dt);
#endif
   return dt->avail;
}

/* Positioned read that leaves the fill's own file cursor alone. */
static int data_transfer_read_at(data_transfer_t *dt, size_t off,
      uint8_t *dst, size_t n)
{
   int64_t save = filestream_tell(dt->f);
   int64_t r;
   if (save < 0
         || filestream_seek(dt->f, (int64_t)off,
               RETRO_VFS_SEEK_POSITION_START) < 0)
      return 0;
   r = filestream_read(dt->f, dst, (int64_t)n);
   filestream_seek(dt->f, save, RETRO_VFS_SEEK_POSITION_START);
   return r == (int64_t)n;
}

void data_transfer_discard(data_transfer_t *dt, size_t up_to)
{
   size_t lo;
   if (!dt || !dt->f || !dt->map_len || !dt->page)
      return;                     /* source or fallback: bytes stay */
   if (dt->window)
      return;                     /* not this surface - see below */
   if (up_to > dt->avail)
      up_to = dt->avail;
   lo = (up_to / dt->page) * dt->page;
   if (lo <= dt->low)
      return;
#ifdef DT_STRICT
   memdecommit(dt->map + dt->low, lo - dt->low, true);
#else
   memdecommit(dt->map + dt->low, lo - dt->low, false);
#endif
   dt->low = lo;
}

bool data_transfer_refill(data_transfer_t *dt, size_t from)
{
   size_t lo, end;
   if (!dt)
      return false;
   if (!dt->f || !dt->map_len || dt->window || from >= dt->low)
      return !dt->failed;         /* nothing was released there */
   lo  = (from / dt->page) * dt->page;
   end = dt->low;
   if (end > dt->avail)
      end = dt->avail;
   /* Windows needs the range committing again, and so does any build
    * where discard() released strictly, which leaves the pages
    * unreadable.  Only on POSIX without DT_STRICT is this a cheap
    * no-op: MADV_DONTNEED alone leaves them committed to refault as
    * zeros. */
   if (!memcommit(dt->map + lo, end - lo))
      return false;
   if (end > lo && !data_transfer_read_at(dt, lo, dt->map + lo, end - lo))
   {
      /* the file shrank or errored underneath a re-read: the honest
       * frozen-short state, same as a fill ending early */
      dt->done   = 1;
      dt->failed = 1;
      return false;
   }
   dt->low = lo;
   return true;
}

/* The prefix surface is inert on a window handle.
 *
 * A window keeps a live RFILE and a reservation, so these three would
 * otherwise pass their own guards on one and then disagree with the
 * window's bookkeeping.  discard() is the dangerous one: its 'low'
 * frontier knows nothing of keep/wlo/whi/wfreed, so it releases from
 * offset 0 through the permanently resident head, non-strictly - the
 * head comes back as zeros rather than faulting, which is silent
 * corruption of exactly the bytes a loop lands on.  iterate() merely
 * finds avail == len and settles done, making complete() answer yes
 * about a file that was never read.
 *
 * Declining is the whole fix.  A window's bytes are managed by
 * feed/extend/advance/rewind and its length is known from open, so
 * there is nothing these can usefully do that the window surface does
 * not already do correctly. */
size_t data_transfer_iterate_while(data_transfer_t *dt, size_t max_bytes,
      data_transfer_continue_t should_continue, void *ud)
{
   if (!dt)
      return 0;
   if (dt->window)
      return dt->avail;           /* not this surface - see above */
   if (dt->f || dt->src_cb)
      return data_transfer_prefix_iterate(dt, max_bytes,
            should_continue, ud);
   return dt->avail;
}

size_t data_transfer_iterate(data_transfer_t *dt, size_t max_bytes)
{
   return data_transfer_iterate_while(dt, max_bytes, NULL, NULL);
}

const uint8_t *data_transfer_ptr(data_transfer_t *dt, size_t *len)
{
   if (!dt)
   {
      if (len)
         *len = 0;
      return NULL;
   }
   if (len)
      *len = dt->len;
   return dt->map;
}

size_t data_transfer_avail(data_transfer_t *dt)
{
   return dt ? dt->avail : 0;
}

bool data_transfer_complete(data_transfer_t *dt)
{
   return dt && dt->done && !dt->failed;
}

bool data_transfer_capped(data_transfer_t *dt)
{
   return dt && dt->capped;
}

bool data_transfer_failed(data_transfer_t *dt)
{
   return dt && dt->failed;
}

void data_transfer_free(data_transfer_t *dt)
{
   if (!dt)
      return;
   if (dt->f)
      filestream_close(dt->f);
   if (dt->map)
   {
      if (dt->map_len)
      {
#if !defined(DT_NO_POOL)
         if (!dt->pooled
               || !data_transfer_pool_give(dt->map, dt->map_len))
#endif
            memrelease(dt->map, dt->map_len);
      }
      else
         free(dt->map);
   }
   free(dt);
}
