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

#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#elif defined(HAVE_MMAN) || defined(__unix__) || defined(__APPLE__)
#include <sys/mman.h>
#include <unistd.h>
/* Gate on what the headers actually provide, not on the platform
 * list above: DJGPP defines __unix__ and ships stub mman/unistd
 * headers that include cleanly but declare neither mmap nor the
 * MAP_ constants nor _SC_PAGESIZE. Without the reservation the code
 * already falls back to plain growth, so absence is graceful. Older
 * BSD-family headers spell MAP_ANONYMOUS as MAP_ANON. */
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif
#if defined(MAP_PRIVATE) && defined(MAP_ANONYMOUS) && defined(_SC_PAGESIZE)
#define DT_HAVE_RESERVE 1
#endif
#endif

#include <streams/file_stream.h>
#include <string.h>
#include <formats/data_transfer.h>

/* Prefix strategy: commit physical pages in steps of this as the
 * fill advances (one protection syscall per step). */
#define DT_COMMIT_STEP  (1024 * 1024)
/* Read granularity inside iterate. */
#define DT_READ_CHUNK   (64 * 1024)
/* Fallback window when the platform cannot reserve address space and
 * the caller gave no cap. */
#define DT_FALLBACK_WINDOW  (32 * 1024 * 1024)

struct data_transfer
{
   /* strategy 2: internal prefix reader (open_prefix)               */
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
};

bool data_transfer_arena_init(data_transfer_arena_t *a, size_t ceiling)
{
   a->base      = NULL;
   a->committed = 0;
   a->cap       = 0;
   a->reserved  = 0;
   if (!ceiling)
      return true;               /* pure fallback growth */
#if defined(_WIN32)
   {
      SYSTEM_INFO si;
      size_t r;
      GetSystemInfo(&si);
      r = (ceiling + si.dwPageSize - 1) & ~((size_t)si.dwPageSize - 1);
      a->base = (uint8_t*)VirtualAlloc(NULL, r, MEM_RESERVE, PAGE_NOACCESS);
      if (a->base)
      {
         a->cap      = r;
         a->reserved = 1;
      }
   }
#elif defined(DT_HAVE_RESERVE)
   {
      long ps  = sysconf(_SC_PAGESIZE);
      size_t r = (ceiling + (size_t)ps - 1) & ~((size_t)ps - 1);
      void *m  = mmap(NULL, r, PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      if (m != MAP_FAILED)
      {
         a->base     = (uint8_t*)m;
         a->cap      = r;
         a->reserved = 1;
      }
   }
#endif
   return true;                  /* reservation failure: fallback */
}

bool data_transfer_arena_ensure(data_transfer_arena_t *a, size_t need)
{
   if (need <= a->committed)
      return true;
   if (a->reserved)
   {
      size_t to;
      if (need > a->cap)
         return false;           /* past the declared ceiling */
      to = (need + (DT_COMMIT_STEP - 1)) & ~((size_t)DT_COMMIT_STEP - 1);
      if (to > a->cap)
         to = a->cap;
#if defined(_WIN32)
      if (!VirtualAlloc(a->base + a->committed, to - a->committed,
            MEM_COMMIT, PAGE_READWRITE))
         return false;
#elif defined(DT_HAVE_RESERVE)
      if (mprotect(a->base + a->committed, to - a->committed,
            PROT_READ | PROT_WRITE) != 0)
         return false;
#endif
      a->committed = to;
      return true;
   }
   {
      size_t nc = a->cap ? a->cap : (256 * 1024);
      uint8_t *nb;
      while (nc < need)
         nc *= 2;
      if (!(nb = (uint8_t*)realloc(a->base, nc)))
         return false;
      a->base      = nb;
      a->cap       = nc;
      a->committed = nc;
      return true;
   }
}

void data_transfer_arena_release(data_transfer_arena_t *a)
{
   if (a->reserved && a->base)
#if defined(_WIN32)
      VirtualFree(a->base, 0, MEM_RELEASE);
#elif defined(DT_HAVE_RESERVE)
      munmap(a->base, a->cap);
#else
      free(a->base);
#endif
   else if (a->base)
      free(a->base);
   a->base      = NULL;
   a->committed = 0;
   a->cap       = 0;
   a->reserved  = 0;
}


/* ---- cyclic streaming window ---- */

static int data_transfer_read_at(data_transfer_t *dt, size_t off,
      uint8_t *dst, size_t n);

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
#if defined(_WIN32)
   VirtualFree(dt->map + from, to - from, MEM_DECOMMIT);
#elif defined(DT_HAVE_RESERVE)
#ifdef DT_WINDOW_STRICT
   /* oracle mode: touching an advanced-past page faults loudly */
   mprotect(dt->map + from, to - from, PROT_NONE);
   madvise(dt->map + from, to - from, MADV_DONTNEED);
#else
   madvise(dt->map + from, to - from, MADV_DONTNEED);
#endif
#endif
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
#if defined(_WIN32)
   if (!VirtualAlloc(dt->map + f, t - f, MEM_COMMIT, PAGE_READWRITE))
      return 0;
#elif defined(DT_HAVE_RESERVE)
   if (mprotect(dt->map + f, t - f, PROT_READ | PROT_WRITE) != 0)
      return 0;
#endif
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
   data_transfer_t *dt = data_transfer_open_prefix(path, 0);
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
       * calls become no-ops and the file is simply resident */
      data_transfer_iterate(dt, 0);
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
#if defined(_WIN32) || defined(DT_HAVE_RESERVE)
   return true;
#else
   return false;
#endif
}

bool data_transfer_window_is_reserved(data_transfer_t *dt)
{
   return dt && dt->window && dt->map_len != 0;
}

const uint8_t *data_transfer_window_base(data_transfer_t *dt, size_t *len)
{
   if (len)
      *len = dt->len;
   return dt->map;
}

bool data_transfer_window_extend(data_transfer_t *dt, size_t hi)
{
   if (!dt->window)
      return false;
   if (hi > dt->len)
      hi = dt->len;
   if (hi <= dt->whi)
      return true;
   if (!dt->map_len)
      return true;               /* fallback: already whole */
   if (!data_transfer_wcommit(dt, dt->whi, hi))
      return false;
   if (!data_transfer_read_at(dt, dt->whi, dt->map + dt->whi,
         hi - dt->whi))
      return false;
   dt->whi = hi;
   return true;
}

void data_transfer_window_advance(data_transfer_t *dt, size_t lo)
{
   if (!dt->window || !dt->map_len)
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
   if (!dt->window || !dt->map_len)
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
   if (!dt->window)
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
         return false;
      if (!data_transfer_read_at(dt, dt->keep, dt->map + dt->keep,
            keep - dt->keep))
         return false;
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
   if (!dt->window || off + n > dt->len)
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
   if (!dt->window || !dt->map_len)
      return;
   f = (from + dt->page - 1) & ~(dt->page - 1);
   t = (to / dt->page) * dt->page;
   if (t <= f)
      return;
#if defined(_WIN32)
   VirtualFree(dt->map + f, t - f, MEM_DECOMMIT);
#elif defined(DT_HAVE_RESERVE)
#ifdef DT_WINDOW_STRICT
   mprotect(dt->map + f, t - f, PROT_NONE);
   madvise(dt->map + f, t - f, MADV_DONTNEED);
#else
   madvise(dt->map + f, t - f, MADV_DONTNEED);
#endif
#endif
}

bool data_transfer_window_feed(data_transfer_t *dt, size_t tell,
      size_t lookahead, size_t margin)
{
   if (!dt->window)
      return false;
   if (tell < dt->wtell)
      data_transfer_window_rewind(dt);   /* the consumer looped */
   dt->wtell = tell;
   if (tell > margin && tell - margin > dt->wlo)
      data_transfer_window_advance(dt, tell - margin);
   return data_transfer_window_extend(dt, tell + lookahead);
}

data_transfer_t *data_transfer_open_prefix(const char *path,
      size_t commit_cap)
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

#if defined(_WIN32)
   {
      SYSTEM_INFO si;
      size_t pg, rl;
      GetSystemInfo(&si);
      pg = (size_t)si.dwPageSize;
      rl = ((dt->len + pg - 1) / pg) * pg;
      dt->map = (uint8_t*)VirtualAlloc(NULL, rl, MEM_RESERVE,
            PAGE_NOACCESS);
      if (dt->map)
      {
         dt->map_len = rl;
         dt->page    = pg;
      }
   }
#elif defined(DT_HAVE_RESERVE)
   {
      long pgl = sysconf(_SC_PAGESIZE);
      size_t pg = pgl > 0 ? (size_t)pgl : 4096;
      size_t rl = ((dt->len + pg - 1) / pg) * pg;
      void *m   = mmap(NULL, rl, PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      if (m != MAP_FAILED)
      {
         dt->map     = (uint8_t*)m;
         dt->map_len = rl;
         dt->page    = pg;
      }
   }
#endif
   if (!dt->map)
   {
      /* No reservation (32-bit address space, or a platform without
       * virtual memory): a bounded plain allocation.  The cap - or
       * the built-in window - is the buffer. */
      size_t w = dt->cap ? dt->cap : (size_t)DT_FALLBACK_WINDOW;
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

/* Grow the physical backing to cover at least 'need' bytes. */
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
#if defined(_WIN32)
   if (!VirtualAlloc(dt->map, target, MEM_COMMIT, PAGE_READWRITE))
      return 0;
#elif defined(DT_HAVE_RESERVE)
   if (mprotect(dt->map, target, PROT_READ | PROT_WRITE) != 0)
      return 0;
#endif
   dt->committed = target;
   return 1;
}

static size_t data_transfer_prefix_iterate(data_transfer_t *dt,
      size_t max_bytes)
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
      return;                     /* nbio or fallback: bytes stay */
   if (up_to > dt->avail)
      up_to = dt->avail;
   lo = (up_to / dt->page) * dt->page;
   if (lo <= dt->low)
      return;
#if defined(_WIN32)
   VirtualFree(dt->map + dt->low, lo - dt->low, MEM_DECOMMIT);
#elif defined(DT_HAVE_RESERVE)
   madvise(dt->map + dt->low, lo - dt->low, MADV_DONTNEED);
#endif
   dt->low = lo;
}

bool data_transfer_refill(data_transfer_t *dt, size_t from)
{
   size_t lo, end;
   if (!dt)
      return false;
   if (!dt->f || !dt->map_len || from >= dt->low)
      return !dt->failed;         /* nothing was released there */
   lo  = (from / dt->page) * dt->page;
   end = dt->low;
   if (end > dt->avail)
      end = dt->avail;
#if defined(_WIN32)
   if (!VirtualAlloc(dt->map + lo, end - lo, MEM_COMMIT,
         PAGE_READWRITE))
      return false;
#endif
   /* (POSIX: MADV_DONTNEED left the pages committed; they refault as
    * zeros and are simply written over.) */
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

size_t data_transfer_iterate(data_transfer_t *dt, size_t max_bytes)
{
   if (!dt)
      return 0;
   if (dt->f || dt->src_cb)
      return data_transfer_prefix_iterate(dt, max_bytes);
   return dt->avail;
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
#if defined(_WIN32)
      if (dt->map_len)
         VirtualFree(dt->map, 0, MEM_RELEASE);
      else
         free(dt->map);
#elif defined(DT_HAVE_RESERVE)
      if (dt->map_len)
         munmap(dt->map, dt->map_len);
      else
         free(dt->map);
#else
      free(dt->map);
#endif
   }
   free(dt);
}
