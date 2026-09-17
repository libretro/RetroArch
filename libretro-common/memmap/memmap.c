/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (memmap.c).
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

#include <stdint.h>
#include <stdlib.h>
#include <memmap.h>

#ifndef PROT_READ
#define PROT_READ         0x1  /* Page can be read */
#endif

#ifndef PROT_WRITE
#define PROT_WRITE        0x2  /* Page can be written. */
#endif

#ifndef PROT_READWRITE
#define PROT_READWRITE    0x3  /* Page can be written to and read from. */
#endif

#ifndef PROT_EXEC
#define PROT_EXEC         0x4  /* Page can be executed. */
#endif

#ifndef PROT_NONE
#define PROT_NONE         0x0  /* Page can not be accessed. */
#endif

#ifndef MAP_FAILED
#define MAP_FAILED        ((void *) -1)
#endif

#ifdef _WIN32
/* FILE_MAP_EXECUTE is XP SP2 / Server 2003 SP1: the kernel rejects it
 * before that, and an older SDK may not spell it at all. The value is
 * SECTION_MAP_EXECUTE_EXPLICIT, declared here so the oldest SDK builds;
 * on a kernel that refuses it MapViewOfFileEx fails and the call reports
 * NULL rather than mapping without execute. */
#ifndef FILE_MAP_EXECUTE
#define FILE_MAP_EXECUTE 0x0020
#endif

/* Map POSIX prot bits to a PAGE_* protection constant.  Windows has
 * no write-only or exec-only protections; those requests take the
 * nearest expressible superset, as every mman shim does. */
static DWORD win32_page_prot(int prot)
{
   if (prot == PROT_NONE)
      return PAGE_NOACCESS;
   if (prot & PROT_EXEC)
      return (prot & PROT_WRITE) ? PAGE_EXECUTE_READWRITE
                                 : PAGE_EXECUTE_READ;
   return (prot & PROT_WRITE) ? PAGE_READWRITE : PAGE_READONLY;
}

/* The FILE_MAP_* access for a view of a section created with the
 * protection above. */
static DWORD win32_view_access(int prot)
{
   DWORD access = FILE_MAP_READ;
   if (prot & PROT_WRITE)
      access = FILE_MAP_ALL_ACCESS;
   if (prot & PROT_EXEC)
      access |= FILE_MAP_EXECUTE;
   return access;
}

void* mmap(void *addr, size_t len, int prot, int flags,
      int fildes, size_t offset)
{
   void  *map    = NULL;
   HANDLE handle;
   /* Sections are created with the maximum protection a later
    * mprotect may need: VirtualProtect on a view cannot exceed the
    * section's protection, so an anonymous mapping opened without
    * PROT_EXEC can never later become executable (request PROT_EXEC
    * up front, as every JIT does), and PROT_NONE starts from a
    * readable section and is locked down after mapping. */
   int    sect_prot = (prot == PROT_NONE) ? PROT_READ : prot;

   (void)addr; /* placement hint not honoured, as before */

   if ((flags & MAP_ANONYMOUS) || fildes < 0)
   {
      /* Anonymous: a pagefile-backed section, so munmap stays
       * UnmapViewOfFile for every mapping this function returns. */
      handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL,
            win32_page_prot(sect_prot) | SEC_COMMIT,
            (DWORD)((uint64_t)len >> 32), (DWORD)len, NULL);
      if (!handle)
         return MAP_FAILED;
      map = MapViewOfFile(handle, win32_view_access(sect_prot),
            0, 0, len);
      CloseHandle(handle);
   }
   else
   {
      /* File-backed.  The file offset must be a multiple of the
       * allocation granularity (64 KiB): MapViewOfFile accepts
       * nothing finer, and the historical behaviour here - mapping
       * from 0 and adding the offset to the returned pointer - both
       * read the wrong bytes past the mapped length and broke the
       * later UnmapViewOfFile, which needs the view base. */
      SYSTEM_INFO si;
      uint64_t    end = (uint64_t)offset + len;

      GetSystemInfo(&si);
      if (offset & (si.dwAllocationGranularity - 1))
         return MAP_FAILED;

      handle = CreateFileMapping((HANDLE)_get_osfhandle(fildes), NULL,
            win32_page_prot(sect_prot),
            (DWORD)(end >> 32), (DWORD)end, NULL);
      if (!handle)
         return MAP_FAILED;
      map = MapViewOfFile(handle, win32_view_access(sect_prot),
            (DWORD)((uint64_t)offset >> 32), (DWORD)offset, len);
      CloseHandle(handle);
   }

   if (!map)
      return MAP_FAILED;

   if (prot == PROT_NONE)
   {
      DWORD old;
      VirtualProtect(map, len, PAGE_NOACCESS, &old);
   }

   return map;
}

int munmap(void *addr, size_t len)
{
   (void)len;
   return (UnmapViewOfFile(addr)) ? 0 : -1;
}

int mprotect(void *addr, size_t len, int prot)
{
   /* The previous version dead-stored prot to 0 on entry and passed
    * a NULL old-protection pointer, so VirtualProtect failed on
    * every call (protection 0 is invalid and lpflOldProtect is
    * mandatory) - and the raw BOOL return inverted the POSIX
    * convention on top.  Nothing in this tree exercised it on
    * Windows, which is how it survived. */
   DWORD old;
   if (!VirtualProtect(addr, len, win32_page_prot(prot), &old))
      return -1;
   return 0;
}

#elif !defined(HAVE_MMAN)
void* mmap(void *addr, size_t len, int prot, int flags,
      int fildes, size_t offset)
{
   return malloc(len);
}

int munmap(void *addr, size_t len)
{
   free(addr);
   return 0;
}

int mprotect(void *addr, size_t len, int prot)
{
   /* stub - not really needed at this point
    * since this codepath has no dynarecs. */
   return 0;
}

#endif

#if defined(__MACH__) && (defined(__arm__) || defined(__aarch64__))
#include <libkern/OSCacheControl.h>
#endif

int memsync(void *start, void *end)
{
   size_t len = (char*)end - (char*)start;
#if defined(_WIN32) && !defined(_XBOX)
   /* Coherent on x86; required on ARM64 Windows, where the JIT's writes
    * are not seen by instruction fetch until this. */
   return FlushInstructionCache(GetCurrentProcess(), start, len) ? 0 : -1;
#elif defined(__MACH__) && (defined(__arm__) || defined(__aarch64__))
   sys_icache_invalidate(start, len);
   return 0;
#elif (defined(__arm__) || defined(__aarch64__)) && !defined(__QNX__)
   /* __builtin___clear_cache, not bare __clear_cache: the builtin is
    * what both GCC and clang provide, and it is the instruction cache
    * these callers need flushed -- a JIT that just wrote code. aarch64
    * does not define __arm__, so it is named here: before, it fell
    * through to msync below, which flushes data and not instructions. */
   __builtin___clear_cache((char*)start, (char*)end);
   return 0;
#elif defined(HAVE_MMAN) && !defined(__EMSCRIPTEN__) && defined(MS_SYNC) && defined(MS_INVALIDATE)
   return msync(start, len, MS_SYNC | MS_INVALIDATE
#ifdef __QNX__
         | MS_CACHE_ONLY
#endif
         );
#else
   (void)start; (void)end; (void)len;
   return 0;
#endif
}

int memprotect(void *addr, size_t len)
{
   return mprotect(addr, len, PROT_READ | PROT_WRITE | PROT_EXEC);
}

/* --------------------------------------------------------------------
 * Reserve/commit. See memmap.h for why this cannot go through mmap().
 * -------------------------------------------------------------------- */

#if defined(_WIN32)
#define MEMMAP_HAVE_RESERVE 1
/* Emscripten is named here rather than left to the capability checks
 * below, which it would pass: it has MAP_PRIVATE, MAP_ANONYMOUS and
 * _SC_PAGESIZE. What it does not have is a reservation - an anonymous
 * mmap allocates and zeroes the whole length there, so the range is
 * committed the moment it is asked for, and mprotect and madvise are
 * no-ops, so the guards memrearm() and memdecommit() exist to install
 * would report success without arming anything. Reporting no support
 * keeps consumers on their fallback, which is the honest answer. */
#elif defined(HAVE_MMAN) && !defined(__EMSCRIPTEN__)
/* memmap.h has already included <sys/mman.h> in this case; sysconf and
 * _SC_PAGESIZE need <unistd.h> as well. */
#include <unistd.h>
/* Gate on what the headers provide rather than the platform list:
 * DJGPP defines __unix__ and ships stub mman/unistd headers that
 * include cleanly but declare neither mmap nor the MAP_ constants nor
 * _SC_PAGESIZE. Older BSD-family headers spell MAP_ANONYMOUS as
 * MAP_ANON. */
#if !defined(MAP_ANONYMOUS) && defined(MAP_ANON)
#define MAP_ANONYMOUS MAP_ANON
#endif
#if defined(MAP_PRIVATE) && defined(MAP_ANONYMOUS) && defined(_SC_PAGESIZE)
#define MEMMAP_HAVE_RESERVE 1
#endif
#endif

size_t mempagesize(void)
{
#if !defined(MEMMAP_HAVE_RESERVE)
   return 0;
#elif defined(_WIN32)
   SYSTEM_INFO si;
   GetSystemInfo(&si);
   return (size_t)si.dwPageSize;
#else
   long ps = sysconf(_SC_PAGESIZE);
   return (ps > 0) ? (size_t)ps : 0;
#endif
}

void *memreserve(size_t len)
{
#if defined(MEMMAP_TEST_NO_RESERVE)
   /* Test hook: behave like a platform that cannot reserve address
    * space (memreserve refused), so the data_transfer whole-file path -
    * and everything that must agree with it - can be exercised on a
    * host that normally can. Off unless the env var is set, so a build
    * with the hook still reserves by default. */
   if (getenv("MEMMAP_NO_RESERVE"))
      return NULL;
#endif
#if !defined(MEMMAP_HAVE_RESERVE)
   (void)len;
   return NULL;
#else
   size_t ps = mempagesize();
   size_t r;

   if (!len || !ps)
      return NULL;
   r = (len + ps - 1) & ~(ps - 1);

#if defined(_WIN32)
   return VirtualAlloc(NULL, r, MEM_RESERVE, PAGE_NOACCESS);
#else
   {
      void *m = mmap(NULL, r, PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      return (m == MAP_FAILED) ? NULL : m;
   }
#endif
#endif
}

/* As memreserve, at a preferred address. The hint is a hint: a taken
 * address gets another, and the caller compares. On Windows the whole
 * reservation is one VirtualAlloc at the hint; on mman platforms an
 * mmap of PROT_NONE at the hint, which the kernel may move. */
void *memreserve_at(void *hint, size_t len)
{
#if !defined(MEMMAP_HAVE_RESERVE)
   (void)hint; (void)len;
   return NULL;
#else
   size_t page = mempagesize();
   size_t r    = (len + page - 1) & ~(page - 1);
   if (!len)
      return NULL;
#if defined(_WIN32)
   return VirtualAlloc(hint, r, MEM_RESERVE, PAGE_NOACCESS);
#else
   {
      void *m = mmap(hint, r, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      return (m == MAP_FAILED) ? NULL : m;
   }
#endif
#endif
}

bool memcommit(void *addr, size_t len)
{
#if !defined(MEMMAP_HAVE_RESERVE)
   (void)addr;
   (void)len;
   return false;
#else
   if (!addr || !len)
      return true;
#if defined(_WIN32)
   return VirtualAlloc(addr, len, MEM_COMMIT, PAGE_READWRITE) != NULL;
#else
   return mprotect(addr, len, PROT_READ | PROT_WRITE) == 0;
#endif
#endif
}

bool memrearm(void *addr, size_t len)
{
#if !defined(MEMMAP_HAVE_RESERVE)
   (void)addr;
   (void)len;
   return false;
#else
   if (!addr || !len)
      return true;
#if defined(_WIN32)
   {
      DWORD old;
      /* Not MEM_DECOMMIT: the pages stay committed and keep their
       * backing, so re-committing them later costs no fault. */
      return VirtualProtect(addr, len, PAGE_NOACCESS, &old) != 0;
   }
#else
   /* mprotect alone.  memdecommit() pairs this with MADV_DONTNEED,
    * which is what frees the page and forces the refault. */
   return mprotect(addr, len, PROT_NONE) == 0;
#endif
#endif
}

void memdecommit(void *addr, size_t len, bool strict)
{
#if !defined(MEMMAP_HAVE_RESERVE)
   (void)addr;
   (void)len;
   (void)strict;
#else
   if (!addr || !len)
      return;
#if defined(_WIN32)
   /* MEM_DECOMMIT already leaves the range inaccessible, so strict
    * costs nothing extra here. */
   (void)strict;
   VirtualFree(addr, len, MEM_DECOMMIT);
#else
   if (strict)
      mprotect(addr, len, PROT_NONE);
   madvise(addr, len, MADV_DONTNEED);
#endif
#endif
}

void memrelease(void *addr, size_t len)
{
#if !defined(MEMMAP_HAVE_RESERVE)
   (void)addr;
   (void)len;
#else
   if (!addr)
      return;
#if defined(_WIN32)
   (void)len;
   VirtualFree(addr, 0, MEM_RELEASE);
#else
   {
      /* memreserve() rounded the request up to a whole number of pages
       * and mapped that, so the same rounding has to be applied here:
       * munmap() is specified against the pages the range covers, and
       * an implementation that takes the length literally rather than
       * rounding it - and rejects a partial unmap - leaks the whole
       * reservation when len is not already a multiple. */
      size_t ps = mempagesize();
      size_t r  = ps ? ((len + ps - 1) & ~(ps - 1)) : len;
      munmap(addr, r);
   }
#endif
#endif
}

/* ------------------------------------------------------------------ */
/* Named shared memory, mappable at more than one address              */
/* ------------------------------------------------------------------ */

#if defined(_WIN32) && !defined(_XBOX)

void *memshm_create(const char *name, size_t len)
{
   HANDLE h;
   wchar_t wname[128];
   int i;
   /* The name is ASCII by contract; widen it byte-for-byte. */
   for (i = 0; i < 127 && name[i]; i++)
      wname[i] = (wchar_t)(unsigned char)name[i];
   wname[i] = 0;
   h = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE,
         (DWORD)((uint64_t)len >> 32), (DWORD)(len & 0xFFFFFFFFu), wname);
   return (h == NULL) ? NULL : (void*)h;
}

void memshm_destroy(void *handle)
{
   if (handle)
      CloseHandle((HANDLE)handle);
}

void *memshm_map(void *handle, size_t offset, void *hint, size_t len, int prot)
{
   DWORD access = FILE_MAP_READ;
   void *p;
   if (prot & PROT_WRITE)
      access |= FILE_MAP_WRITE;
   if (prot & PROT_EXEC)
      access |= FILE_MAP_EXECUTE;
   p = MapViewOfFileEx((HANDLE)handle, access,
         (DWORD)((uint64_t)offset >> 32), (DWORD)(offset & 0xFFFFFFFFu), len, hint);
   if (!p && hint)
      p = MapViewOfFileEx((HANDLE)handle, access,
            (DWORD)((uint64_t)offset >> 32), (DWORD)(offset & 0xFFFFFFFFu), len, NULL);
   return p;
}

void memshm_unmap(void *addr, size_t len)
{
   (void)len;
   if (addr)
      UnmapViewOfFile(addr);
}

/* Gated on MAP_SHARED, as reserve/commit gates on its constants: DJGPP
 * defines __unix__ and HAVE_MMAN but ships a stub <sys/mman.h> with no
 * MAP_SHARED and no shm_open, and falls through to the stubs below. */
#elif defined(HAVE_MMAN) && !defined(__EMSCRIPTEN__) && defined(MAP_SHARED)

#include <unistd.h>
#include <fcntl.h>
#if defined(__ANDROID__)
#include <sys/syscall.h>
#endif

/* The handle is the file descriptor itself, carried in the pointer --
 * as on Windows it is the HANDLE itself. A caller that maps the region
 * some way memshm_map does not offer (MAP_FIXED into a reservation it
 * owns) can use it directly. Descriptor 0 would read as NULL, so a
 * region that lands there is moved off it at create. */
#define MEMSHM_FD(h)   ((int)(intptr_t)(h))
#define MEMSHM_H(fd)   ((void*)(intptr_t)(fd))

void *memshm_create(const char *name, size_t len)
{
   int fd;
#if defined(__ANDROID__)
   /* Bionic has no shm_open. A memfd is anonymous and needs no name in
    * the filesystem, so nothing to unlink. */
   fd = (int)syscall(__NR_memfd_create, name, 1u /* MFD_CLOEXEC */);
   if (fd < 0)
      return NULL;
#else
   fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
   if (fd < 0)
      return NULL;
   /* Unlink at once: the fd keeps the object alive, the name does not
    * outlive the process. */
   shm_unlink(name);
#endif
   /* off_t is 32 bits where _FILE_OFFSET_BITS is not set, and a cast
    * would wrap a 2 GB length to a negative one. Refuse rather than
    * truncate; a caller that needs more on a 32-bit off_t has no way to
    * get it through this call. */
   if (sizeof(off_t) < 8 && len > (size_t)0x7FFFFFFFu)
   {
      close(fd);
      return NULL;
   }
   if (ftruncate(fd, (off_t)len) < 0)
   {
      close(fd);
      return NULL;
   }
   if (fd == 0)
   {
      /* Only if stdin was closed; keep 0 out of the handle anyway. */
      int moved = fcntl(fd, F_DUPFD_CLOEXEC, 1);
      close(fd);
      if (moved < 0)
         return NULL;
      fd = moved;
   }
   return MEMSHM_H(fd);
}

void memshm_destroy(void *handle)
{
   if (handle)
      close(MEMSHM_FD(handle));
}

void *memshm_map(void *handle, size_t offset, void *hint, size_t len, int prot)
{
   void *p;
   if (!handle)
      return NULL;
   /* A hint, never MAP_FIXED: MAP_FIXED silently replaces whatever is
    * there, and a caller that wanted an address it did not get should
    * find out by comparing, not by corrupting a neighbour. */
   p = mmap(hint, len, prot, MAP_SHARED, MEMSHM_FD(handle), (off_t)offset);
   return (p == MAP_FAILED) ? NULL : p;
}

void memshm_unmap(void *addr, size_t len)
{
   if (addr)
      munmap(addr, len);
}

#else

void *memshm_create(const char *name, size_t len)
{
   (void)name; (void)len;
   return NULL;
}
void memshm_destroy(void *handle) { (void)handle; }
void *memshm_map(void *handle, size_t offset, void *hint, size_t len, int prot)
{
   (void)handle; (void)offset; (void)hint; (void)len; (void)prot;
   return NULL;
}
void memshm_unmap(void *addr, size_t len) { (void)addr; (void)len; }

#endif

/* ------------------------------------------------------------------ */
/* JIT write toggle for per-thread W^X                                 */
/* ------------------------------------------------------------------ */

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
/* pthread_jit_write_protect_np is macOS on Apple Silicon and nothing
 * else: iOS and tvOS are arm64 and __APPLE__ too, and do not have it.
 * TARGET_OS_OSX is the test, not the architecture. */
#if defined(__APPLE__) && defined(__aarch64__) && defined(TARGET_OS_OSX) && TARGET_OS_OSX
#include <pthread.h>
/* pthread_jit_write_protect_np is per thread, and so is this depth:
 * one thread's nesting must not flip another's pages. */
static __thread int memjit_depth;

void memjit_write_begin(void)
{
   if (memjit_depth++ == 0)
      pthread_jit_write_protect_np(0);
}

void memjit_write_end(void)
{
   if (--memjit_depth == 0)
      pthread_jit_write_protect_np(1);
}
#else
void memjit_write_begin(void) { }
void memjit_write_end(void)   { }
#endif
