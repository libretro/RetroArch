/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_spsc.c).
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

/* memfd_create goes through syscall(), and ftruncate and shm_open
 * are POSIX: strict C89 builds hide all three without these. */
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif

#include <stdlib.h>
#include <string.h>
#include <retro_inline.h>
#include <retro_spsc.h>

/* Where a buffer can be mapped twice, back to back. RETRO_SPSC_MIRROR_SHM
 * selects the POSIX shm path on any POSIX host, for its tests. */
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#define SPSC_MIRROR_WIN32
#elif defined(RETRO_SPSC_MIRROR_SHM)
#define SPSC_MIRROR_SHM
#elif defined(__linux__) && !defined(__EMSCRIPTEN__)
#include <sys/syscall.h>
#ifdef __NR_memfd_create
#define SPSC_MIRROR_MEMFD
#endif
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if !TARGET_OS_IPHONE
#define SPSC_MIRROR_SHM
#endif
#elif (defined(__FreeBSD__) && !defined(__ORBIS__)) || defined(__NetBSD__) \
      || defined(__OpenBSD__)
#define SPSC_MIRROR_SHM
#endif

#if defined(SPSC_MIRROR_WIN32)
#include <windows.h>
#elif defined(SPSC_MIRROR_MEMFD) || defined(SPSC_MIRROR_SHM)
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#endif

#if defined(SPSC_MIRROR_WIN32) || defined(SPSC_MIRROR_MEMFD) \
      || defined(SPSC_MIRROR_SHM)
#define SPSC_MIRROR
#endif

#ifdef SPSC_MIRROR_SHM
#include <stdio.h>
#endif

/* Bytes contiguous at @idx: to the buffer's end, or past it into the
 * mirror. */
static INLINE size_t spsc_contig(const retro_spsc_t *q, size_t idx)
{
   return q->mirror ? (size_t)-1 : q->mask + 1 - idx;
}

#ifdef SPSC_MIRROR
#if !defined(SPSC_MIRROR_WIN32) && !defined(MAP_ANONYMOUS)
#define MAP_ANONYMOUS MAP_ANON
#endif

/* What a mapping is made of: a page, or Windows' allocation granule. */
static size_t spsc_mirror_granule(void)
{
#ifdef SPSC_MIRROR_WIN32
   SYSTEM_INFO si;
   GetSystemInfo(&si);
   return (size_t)si.dwAllocationGranularity;
#else
   long page = sysconf(_SC_PAGESIZE);
   return page > 0 ? (size_t)page : 4096;
#endif
}

static void spsc_mirror_free(uint8_t *base, size_t size)
{
#ifdef SPSC_MIRROR_WIN32
   UnmapViewOfFile(base);
   UnmapViewOfFile(base + size);
#else
   munmap(base, size * 2);
#endif
}

#ifndef SPSC_MIRROR_WIN32
/* Both views of a descriptor's pages, inside one reservation so nothing
 * can land between them. Takes the descriptor. */
static uint8_t *spsc_mirror_map(int fd, size_t size)
{
   uint8_t *base = NULL;
   void    *res;
   if (ftruncate(fd, (off_t)size) == 0)
   {
      res = mmap(NULL, size * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS,
            -1, 0);
      if (res != MAP_FAILED)
      {
         base = (uint8_t*)res;
         if (     mmap(base, size, PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_FIXED, fd, 0) == MAP_FAILED
               || mmap(base + size, size, PROT_READ | PROT_WRITE,
                     MAP_SHARED | MAP_FIXED, fd, 0) == MAP_FAILED)
         {
            munmap(base, size * 2);
            base = NULL;
         }
      }
   }
   close(fd);
   return base;
}
#endif

static uint8_t *spsc_mirror_alloc(size_t size)
{
   uint8_t *base = NULL;
#if defined(SPSC_MIRROR_WIN32)
   unsigned tries;
   HANDLE   h = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL,
         PAGE_READWRITE, (DWORD)((uint64_t)size >> 32), (DWORD)size, NULL);
   if (!h)
      return NULL;
   /* The reservation is released before the views are placed in it,
    * so another thread can take the address meanwhile: try again. */
   for (tries = 0; tries < 16 && !base; tries++)
   {
      uint8_t *at = (uint8_t*)VirtualAlloc(NULL, size * 2, MEM_RESERVE,
            PAGE_NOACCESS);
      void    *lo, *hi;
      if (!at)
         break;
      VirtualFree(at, 0, MEM_RELEASE);
      if (!(lo = MapViewOfFileEx(h, FILE_MAP_ALL_ACCESS, 0, 0, size, at)))
         continue;
      hi = MapViewOfFileEx(h, FILE_MAP_ALL_ACCESS, 0, 0, size, at + size);
      if (hi == (void*)(at + size))
         base = at;
      else
      {
         if (hi)
            UnmapViewOfFile(hi);
         UnmapViewOfFile(lo);
      }
   }
   CloseHandle(h);
#elif defined(SPSC_MIRROR_MEMFD)
   int fd = (int)syscall(__NR_memfd_create, "retro_spsc", 1u /* MFD_CLOEXEC */);
   if (fd >= 0)
      base = spsc_mirror_map(fd, size);
#else
   static unsigned serial;
   unsigned tries;
   for (tries = 0; tries < 8 && !base; tries++)
   {
      char name[48];
      int  fd;
      sprintf(name, "/retro_spsc.%lu.%u", (unsigned long)getpid(),
            serial++);
      if ((fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600)) < 0)
         continue;
      shm_unlink(name);
      base = spsc_mirror_map(fd, size);
      break;
   }
#endif
   /* A write through one view must read back through the other. */
   if (base)
   {
      base[0] = 0x5a;
      if (base[size] != 0x5a)
      {
         spsc_mirror_free(base, size);
         return NULL;
      }
      base[0] = 0;
   }
   return base;
}
#endif

/* Round @v up to the next power of 2.  Returns 0 if @v == 0 or if
 * the next power of 2 would overflow (caller checks @v <= SIZE_MAX/2). */
static size_t spsc_round_up_pow2(size_t v)
{
   size_t r;
   if (v == 0)
      return 0;
   /* If already a power of 2, return as-is. */
   if ((v & (v - 1)) == 0)
      return v;
   /* Round up.  Loop terminates because v <= SIZE_MAX/2 means r
    * cannot overflow. */
   r = 1;
   while (r < v)
      r <<= 1;
   return r;
}

bool retro_spsc_init(retro_spsc_t *q, size_t min_capacity)
{
   size_t cap;

   if (!q || min_capacity == 0 || min_capacity > (SIZE_MAX / 2))
      return false;

   cap = spsc_round_up_pow2(min_capacity);
   if (cap == 0 || cap > (SIZE_MAX / 2))
      return false;

   q->buffer = (uint8_t*)malloc(cap);
   if (!q->buffer)
      return false;

   q->capacity    = cap;
   q->mask        = cap - 1;
   q->mirror      = 0;
   q->cached_tail = 0;
   q->cached_head = 0;
   retro_atomic_size_init(&q->head, 0);
   retro_atomic_size_init(&q->tail, 0);
   return true;
}

bool retro_spsc_init_mirrored(retro_spsc_t *q, size_t min_capacity)
{
#ifdef SPSC_MIRROR
   size_t   cap, size, granule;
   uint8_t *buf;

   if (!q || min_capacity == 0 || min_capacity > (SIZE_MAX / 2))
      return false;
   cap     = spsc_round_up_pow2(min_capacity);
   granule = spsc_mirror_granule();
   /* Positions wrap at the buffer's size, so it has to be a power of
    * 2 as well: the capacity, or the granule where that is larger. */
   size    = cap > granule ? cap : granule;
   if (     cap && !(granule & (granule - 1))
         && size <= (SIZE_MAX / 4)
         && (buf = spsc_mirror_alloc(size)))
   {
      q->buffer      = buf;
      q->capacity    = cap;
      q->mask        = size - 1;
      q->mirror      = size;
      q->cached_tail = 0;
      q->cached_head = 0;
      retro_atomic_size_init(&q->head, 0);
      retro_atomic_size_init(&q->tail, 0);
      return true;
   }
#endif
   return retro_spsc_init(q, min_capacity);
}

void retro_spsc_free(retro_spsc_t *q)
{
   if (!q)
      return;
   if (q->buffer)
   {
#ifdef SPSC_MIRROR
      if (q->mirror)
         spsc_mirror_free(q->buffer, q->mirror);
      else
#endif
         free(q->buffer);
      q->buffer = NULL;
   }
   q->capacity = 0;
   q->mirror   = 0;
}

void retro_spsc_clear(retro_spsc_t *q)
{
   if (!q)
      return;
   /* Quiescence is the caller's responsibility (documented).
    * Under that assumption, no other thread is touching head or
    * tail, so plain init is correct here -- and necessary, because
    * plain assignment to a retro_atomic_size_t is illegal under
    * the C11 stdatomic backend. */
   q->cached_tail = 0;
   q->cached_head = 0;
   retro_atomic_size_init(&q->head, 0);
   retro_atomic_size_init(&q->tail, 0);
}

size_t retro_spsc_write_avail(const retro_spsc_t *q)
{
   /* Producer query.  We read our own head (which we wrote) and the
    * consumer's tail (which they wrote with a release-store).
    * acquire-load on tail so any reads we do based on the freed
    * space are ordered after the consumer's tail publication.
    *
    * The header says producer-only, but audio_driver.c also calls
    * read_avail from the producer side as a fill-level query, and the
    * same could happen here.  Keep BOTH loads acquire so a cross-side
    * caller still gets a happens-before with the other thread's
    * release; the query functions are not hot enough to justify the
    * relaxed own-counter load used in the data-path functions below. */
   size_t head = retro_atomic_load_acquire_size(
         (retro_atomic_size_t*)&q->head);
   size_t tail = retro_atomic_load_acquire_size(
         (retro_atomic_size_t*)&q->tail);
   /* head - tail is well-defined modular arithmetic on size_t.
    * Invariant: 0 <= head - tail <= capacity. */
   return q->capacity - (head - tail);
}

size_t retro_spsc_read_avail(const retro_spsc_t *q)
{
   /* Consumer query.  acquire-load on head pairs with the producer's
    * release-store on head, so the data writes that preceded the
    * release are visible by the time we read them. */
   size_t head = retro_atomic_load_acquire_size(
         (retro_atomic_size_t*)&q->head);
   /* acquire on tail too: see retro_spsc_write_avail - this is called
    * from the producer side in audio_driver.c as a fill query. */
   size_t tail = retro_atomic_load_acquire_size(
         (retro_atomic_size_t*)&q->tail);
   return head - tail;
}

size_t retro_spsc_write(retro_spsc_t *q, const void *data, size_t bytes)
{
   size_t head_idx, first;
   const uint8_t *src = (const uint8_t*)data;
   /* head is ours (relaxed).  Room is first computed from our private
    * copy of tail, which is never ahead of the real one; only when it
    * says there is not enough do we acquire-load the consumer's tail
    * (their cache line), which pairs with their release-store so the
    * freed space is really free. */
   size_t head  = retro_atomic_load_relaxed_size(&q->head);
   size_t avail = q->capacity - (head - q->cached_tail);
   if (avail < bytes)
   {
      q->cached_tail = retro_atomic_load_acquire_size(&q->tail);
      avail          = q->capacity - (head - q->cached_tail);
   }
   if (bytes > avail)
      bytes = avail;
   if (bytes == 0)
      return 0;

   head_idx = head & q->mask;

   /* first = bytes from head_idx to end-of-buffer */
   first = spsc_contig(q, head_idx);
   if (first > bytes)
      first = bytes;

   memcpy(q->buffer + head_idx, src, first);
   memcpy(q->buffer, src + first, bytes - first);

   /* Publish: release-store ensures the memcpys above are globally
    * visible before the consumer observes the new head. */
   retro_atomic_store_release_size(&q->head, head + bytes);
   return bytes;
}

size_t retro_spsc_write_frames(retro_spsc_t *q, const void *data,
      size_t frames, size_t frame_bytes)
{
   size_t head, avail, bytes, index, first;
   const uint8_t *src = (const uint8_t*)data;
   if (!frames || !frame_bytes) return 0;
   head = retro_atomic_load_relaxed_size(&q->head);
   avail = (q->capacity - (head - q->cached_tail)) / frame_bytes;
   if (avail < frames)
   {
      q->cached_tail = retro_atomic_load_acquire_size(&q->tail);
      avail = (q->capacity - (head - q->cached_tail)) / frame_bytes;
   }
   if (frames > avail) frames = avail;
   if (!frames) return 0;
   /* Multiplication is bounded by capacity, even for an oversized request. */
   bytes = frames * frame_bytes;
   index = head & q->mask;
   first = spsc_contig(q, index);
   if (first > bytes) first = bytes;
   memcpy(q->buffer + index, src, first);
   memcpy(q->buffer, src + first, bytes - first);
   retro_atomic_store_release_size(&q->head, head + bytes);
   return frames;
}

size_t retro_spsc_read(retro_spsc_t *q, void *data, size_t bytes)
{
   size_t tail_idx, first;
   uint8_t *dst = (uint8_t*)data;
   /* acquire on head pairs with producer's release-store; this is
    * what makes the subsequent memcpys safe to read. */
   /* tail is ours (relaxed).  Available from our private copy of head
    * first, never ahead of the real one; only when it says too little
    * do we acquire-load the producer's head, which pairs with their
    * release-store so the bytes are really there. */
   size_t tail  = retro_atomic_load_relaxed_size(&q->tail);
   size_t avail = q->cached_head - tail;
   if (avail < bytes)
   {
      q->cached_head = retro_atomic_load_acquire_size(&q->head);
      avail          = q->cached_head - tail;
   }
   if (bytes > avail)
      bytes = avail;
   if (bytes == 0)
      return 0;

   tail_idx = tail & q->mask;

   first = spsc_contig(q, tail_idx);
   if (first > bytes)
      first = bytes;

   memcpy(dst, q->buffer + tail_idx, first);
   memcpy(dst + first, q->buffer, bytes - first);

   /* Publish: release-store so the producer can re-use this space. */
   retro_atomic_store_release_size(&q->tail, tail + bytes);
   return bytes;
}

size_t retro_spsc_peek(const retro_spsc_t *q, void *data, size_t bytes)
{
   size_t tail_idx, first;
   uint8_t *dst = (uint8_t*)data;
   /* Consumer side, same scheme as retro_spsc_read; the cast is only
    * because peek takes a const queue and the cached copy is state. */
   retro_spsc_t *w  = (retro_spsc_t*)q;
   size_t tail  = retro_atomic_load_relaxed_size(&w->tail);
   size_t avail = w->cached_head - tail;
   if (avail < bytes)
   {
      w->cached_head = retro_atomic_load_acquire_size(&w->head);
      avail          = w->cached_head - tail;
   }
   if (bytes > avail)
      bytes = avail;
   if (bytes == 0)
      return 0;

   tail_idx = tail & q->mask;

   first    = spsc_contig(q, tail_idx);
   if (first > bytes)
      first = bytes;

   memcpy(dst, q->buffer + tail_idx, first);
   memcpy(dst + first, q->buffer, bytes - first);
   /* No tail update: peek does not consume. */
   return bytes;
}

size_t retro_spsc_write_begin(retro_spsc_t *q, void **ptr)
{
   size_t head_idx, span;
   /* head is ours (relaxed).  Room from the private copy of tail
    * first; re-read the consumer's tail only when that copy, rather
    * than the wrap, is what limits the span - a stale copy would hand
    * back less than is free. See retro_spsc_write. */
   size_t head  = retro_atomic_load_relaxed_size(&q->head);
   size_t avail = q->capacity - (head - q->cached_tail);
   head_idx     = head & q->mask;
   span         = spsc_contig(q, head_idx);
   if (avail < span && avail < q->capacity)
   {
      q->cached_tail = retro_atomic_load_acquire_size(&q->tail);
      avail          = q->capacity - (head - q->cached_tail);
   }
   if (span > avail)
      span = avail;
   *ptr = q->buffer + head_idx;
   return span;
}

void retro_spsc_write_end(retro_spsc_t *q, size_t bytes)
{
   if (bytes == 0)
      return;
   /* head is written only by the producer, i.e. by this thread, so
    * reading our own last store needs no ordering: a relaxed load is
    * enough (and on the MSVC/Apple/__sync backends the acquire load is
    * a locked RMW, so this also removes a lock-prefixed instruction
    * from every span commit).
    * Release: the caller's stores into the span happen-before the
    * consumer's acquire-load of head, same pairing as retro_spsc_write. */
   retro_atomic_store_release_size(&q->head,
         retro_atomic_load_relaxed_size(&q->head) + bytes);
}

size_t retro_spsc_read_begin(retro_spsc_t *q, const void **ptr)
{
   size_t tail_idx, span;
   /* tail is ours (relaxed).  Available from the private copy of
    * head first; re-read the producer's head only when that copy,
    * rather than the wrap, is what limits the span.  See
    * retro_spsc_read. */
   size_t tail  = retro_atomic_load_relaxed_size(&q->tail);
   size_t avail = q->cached_head - tail;
   tail_idx     = tail & q->mask;
   span         = spsc_contig(q, tail_idx);
   if (avail < span && avail < q->capacity)
   {
      q->cached_head = retro_atomic_load_acquire_size(&q->head);
      avail          = q->cached_head - tail;
   }
   if (span > avail)
      span = avail;
   *ptr = q->buffer + tail_idx;
   return span;
}

void retro_spsc_read_end(retro_spsc_t *q, size_t bytes)
{
   if (bytes == 0)
      return;
   /* tail is written only by the consumer, i.e. by this thread, so a
    * relaxed load of our own counter suffices; see retro_spsc_write_end.
    * Release: our reads of the span happen-before the producer's
    * acquire-load of tail sees the space as free. */
   retro_atomic_store_release_size(&q->tail,
         retro_atomic_load_relaxed_size(&q->tail) + bytes);
}

size_t retro_spsc_skip(retro_spsc_t *q, size_t bytes)
{
   /* tail is ours (relaxed).  Available from our private copy of head
    * first, never ahead of the real one; only when it says too little
    * do we acquire-load the producer's head, which pairs with their
    * release-store so the bytes are really there. */
   size_t tail  = retro_atomic_load_relaxed_size(&q->tail);
   size_t avail = q->cached_head - tail;
   if (avail < bytes)
   {
      q->cached_head = retro_atomic_load_acquire_size(&q->head);
      avail          = q->cached_head - tail;
   }

   if (bytes > avail)
      bytes = avail;
   if (bytes == 0)
      return 0;

   retro_atomic_store_release_size(&q->tail, tail + bytes);
   return bytes;
}
