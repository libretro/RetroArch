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
#define DT_HAVE_RESERVE 1
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <file/nbio.h>
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
   void   *nbio;     /* strategy 1: wrapped nbio handle              */
   /* strategy 2: internal prefix reader (open_prefix)               */
   FILE   *f;
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
};

data_transfer_t *data_transfer_open(const char *path)
{
   data_transfer_t *dt;
   void *handle = nbio_open(path, NBIO_READ);
   if (!handle)
      return NULL;
   if (!(dt = (data_transfer_t*)calloc(1, sizeof(*dt))))
   {
      nbio_free(handle);
      return NULL;
   }
   dt->nbio = handle;
   nbio_get_ptr(handle, &dt->len);
   nbio_begin_read(handle);
   return dt;
}

data_transfer_t *data_transfer_open_prefix(const char *path,
      size_t commit_cap)
{
   data_transfer_t *dt;
   FILE *f;
   long l;
   if (!(f = fopen(path, "rb")))
      return NULL;
   if (fseek(f, 0, SEEK_END) != 0 || (l = ftell(f)) <= 0
         || fseek(f, 0, SEEK_SET) != 0)
   {
      fclose(f);
      return NULL;
   }
   if (!(dt = (data_transfer_t*)calloc(1, sizeof(*dt))))
   {
      fclose(f);
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
         fclose(f);
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
      got = fread(dt->map + dt->avail, 1, chunk, dt->f);
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
#if !defined(_WIN32) && defined(DT_HAVE_RESERVE)
   while (n)
   {
      ssize_t r = pread(fileno(dt->f), dst, n, (off_t)off);
      if (r <= 0)
         return 0;
      dst += (size_t)r; off += (size_t)r; n -= (size_t)r;
   }
   return 1;
#else
   long save = ftell(dt->f);
   size_t r;
   if (save < 0 || fseek(dt->f, (long)off, SEEK_SET) != 0)
      return 0;
   r = fread(dst, 1, n, dt->f);
   fseek(dt->f, save, SEEK_SET);
   return r == n;
#endif
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

data_transfer_t *data_transfer_adopt(void *nbio)
{
   data_transfer_t *dt;
   size_t done = 0, total = 0;
   if (!nbio)
      return NULL;
   if (!(dt = (data_transfer_t*)calloc(1, sizeof(*dt))))
      return NULL;
   dt->nbio = nbio;
   nbio_get_ptr(nbio, &dt->len);
   /* Fold in wherever the read already got to: mid-operation the
    * progress is the valid prefix; an already-finished operation
    * settles to complete or failed exactly as a fill would. */
   if (nbio_get_progress(nbio, &done, &total))
      dt->avail = done;
   else
   {
      dt->done = 1;
      if (total > 0 && done < total)
      {
         dt->failed = 1;
         dt->avail  = done;
      }
      else
         dt->avail = dt->len;
   }
   return dt;
}

/* Fold the backend's post-operation state into done/failed.  Backends
 * that do not track progress (the mmap family, whose iterate completes
 * immediately with the whole mapping) report zeros and count as
 * complete. */
static void data_transfer_settle(data_transfer_t *dt)
{
   size_t done = 0, total = 0;
   if (nbio_get_progress(dt->nbio, &done, &total))
   {
      /* still in flight */
      if (done > dt->avail)
         dt->avail = done;
      return;
   }
   dt->done = 1;
   if (total > 0 && done < total)
   {
      dt->failed = 1;
      if (done > dt->avail)
         dt->avail = done;
   }
   else
      dt->avail = dt->len;
}

size_t data_transfer_iterate(data_transfer_t *dt, size_t max_bytes)
{
   size_t start;
   if (!dt)
      return 0;
   if (dt->f)
      return data_transfer_prefix_iterate(dt, max_bytes);
   if (dt->done)
      return dt->avail;
   start = dt->avail;
   for (;;)
   {
      if (nbio_iterate(dt->nbio))
         break;
      data_transfer_settle(dt);
      if (max_bytes && dt->avail - start >= max_bytes)
         return dt->avail;
   }
   data_transfer_settle(dt);
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
   if (dt->f)
      return dt->map;
   return (const uint8_t*)nbio_get_ptr(dt->nbio, NULL);
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
   if (dt->nbio)
   {
      nbio_cancel(dt->nbio);
      nbio_free(dt->nbio);
   }
   if (dt->f)
      fclose(dt->f);
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
