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
#include <string.h>   /* strlen, memcpy, memset */
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
   /* MADV_DONTNEED is BSD/Linux; a libc built to a strict POSIX
    * profile has only the posix_madvise() spelling, and one with
    * neither keeps the pages, which is correct, just not free. */
#if defined(MADV_DONTNEED)
   madvise(addr, len, MADV_DONTNEED);
#elif defined(POSIX_MADV_DONTNEED)
   posix_madvise(addr, len, POSIX_MADV_DONTNEED);
#endif
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
   /* Darwin caps a shm name at PSHMNAMLEN, 31 characters including the
    * leading slash, and returns ENAMETOOLONG past it where Linux takes
    * a name ten times longer. Rather than let the same caller work on
    * one and fail on the other, a name that will not fit is shortened
    * from the front: the tail carries the part that varies between
    * processes, which is what has to survive. */
#if defined(__APPLE__)
   {
      size_t nlen = strlen(name);
      if (nlen > 30)
      {
         char  short_name[32];
         short_name[0] = '/';
         memcpy(short_name + 1, name + (nlen - 30), 30);
         short_name[31] = '\0';
         fd = shm_open(short_name, O_CREAT | O_EXCL | O_RDWR, 0600);
         if (fd >= 0)
            shm_unlink(short_name);
         goto have_fd;
      }
   }
#endif
   fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
   if (fd < 0)
      return NULL;
   /* Unlink at once: the fd keeps the object alive, the name does not
    * outlive the process. */
   shm_unlink(name);
#if defined(__APPLE__)
have_fd:
   if (fd < 0)
      return NULL;
#endif
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
      /* Only if stdin was closed; keep 0 out of the handle anyway.
       * F_DUPFD_CLOEXEC is POSIX 2008 and missing from the Orbis libc
       * and the older Apple SDKs; F_DUPFD and a separate F_SETFD are
       * everywhere, and the gap between them cannot leak the
       * descriptor to a child this process has not forked yet. */
      int moved = fcntl(fd, F_DUPFD, 1);
      close(fd);
      if (moved < 0)
         return NULL;
      fcntl(moved, F_SETFD, FD_CLOEXEC);
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

/* ------------------------------------------------------------------ */
/* A reservation that shared memory is mapped into at several places   */
/* ------------------------------------------------------------------ */

/* The placeholder path needs the flags as well as the entry points, and
 * an SDK older than the Windows 10 1803 one does not define them --
 * MXE's mingw-w64 among them. Without them only the legacy path is
 * compiled, which reaches every Windows this file supports anyway; with
 * them the runtime check decides. */
#if defined(_WIN32) && !defined(_XBOX) && defined(MEM_RESERVE_PLACEHOLDER) \
 && defined(MEM_PRESERVE_PLACEHOLDER) && defined(MEM_REPLACE_PLACEHOLDER) \
 && defined(MEM_COALESCE_PLACEHOLDERS)
#define MEMSHM_HAVE_PLACEHOLDERS 1
#endif

#if defined(_WIN32) && !defined(_XBOX)

#if defined(MEMSHM_HAVE_PLACEHOLDERS)
/* The placeholder APIs are Windows 10 1803. Resolved once at first use;
 * a system without them gets no area, and its caller runs without a
 * fastmem window. */
typedef PVOID (WINAPI *memshm_VirtualAlloc2_t)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
typedef PVOID (WINAPI *memshm_MapViewOfFile3_t)(HANDLE, HANDLE, PVOID, ULONG64, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
typedef BOOL  (WINAPI *memshm_UnmapViewOfFile2_t)(HANDLE, PVOID, ULONG);

static memshm_VirtualAlloc2_t    s_VirtualAlloc2;
static memshm_MapViewOfFile3_t   s_MapViewOfFile3;
static memshm_UnmapViewOfFile2_t s_UnmapViewOfFile2;
static int                       s_placeholder_state;   /* 0 unknown, 1 yes, -1 no */

static bool memshm_placeholder_apis(void)
{
   HMODULE k;
   if (s_placeholder_state)
      return s_placeholder_state > 0;
   /* A build or a test can force the legacy path on a system that has
    * the placeholder APIs, which is the only way to exercise it without
    * a pre-1803 machine. */
#if defined(MEMSHM_AREA_FORCE_LEGACY)
   s_placeholder_state = -1;
   return false;
#endif
   k = GetModuleHandleW(L"kernelbase.dll");
   if (k)
   {
      /* Through a union: C89 forbids casting between object and
       * function pointers, and this file builds under that lane. */
      union { FARPROC p; memshm_VirtualAlloc2_t va2;
              memshm_MapViewOfFile3_t mv3; memshm_UnmapViewOfFile2_t uv2; } u;
      u.p = GetProcAddress(k, "VirtualAlloc2");    s_VirtualAlloc2    = u.va2;
      u.p = GetProcAddress(k, "MapViewOfFile3");   s_MapViewOfFile3   = u.mv3;
      u.p = GetProcAddress(k, "UnmapViewOfFile2"); s_UnmapViewOfFile2 = u.uv2;
   }
   /* All three, not any one: Windows 10 1709 exports UnmapViewOfFile2
    * and neither of the others, so resolving one and assuming the rest
    * calls a NULL pointer on a shipping system. Verified against the
    * 1709 and 1803 binaries. */
   s_placeholder_state = (s_VirtualAlloc2 && s_MapViewOfFile3 && s_UnmapViewOfFile2) ? 1 : -1;
   return s_placeholder_state > 0;
}
#else
/* No placeholder flags in this SDK: the legacy path is the only one. */
#define memshm_placeholder_apis() (0)
#endif   /* MEMSHM_HAVE_PLACEHOLDERS */

/* The placeholder ranges, as [start, end) offsets into the area, kept
 * sorted. The API cannot be asked where its placeholders are, so the
 * area tracks them: Map splits one, Unmap restores and coalesces. A
 * flat array is right for the few tens of ranges a fastmem window
 * splits into, and keeps the whole thing allocation-light. */
#define MEMSHM_MAX_PLACEHOLDERS 256

typedef struct
{
   size_t start;
   size_t end;
} memshm_range_t;
#endif

struct memshm_area
{
   unsigned char *base;
   size_t         len;
   size_t         mappings;
#if defined(_WIN32) && !defined(_XBOX)
   memshm_range_t ranges[MEMSHM_MAX_PLACEHOLDERS];
   int            range_count;
   /* Legacy path: the span is reserved one allocation-granularity slot
    * at a time, because MEM_RELEASE frees a whole reservation and never
    * part of one. slot is that granularity; reserved[] says which slots
    * are still reservations rather than mapped views. */
   int            legacy;
   size_t         slot;
   unsigned char *reserved;
   size_t         slot_count;
#endif
};

#if defined(_WIN32) && !defined(_XBOX)
/* Index of the placeholder containing this offset, or -1. */
static int memshm_find_range(const memshm_area_t *a, size_t off)
{
   int i;
   for (i = 0; i < a->range_count; i++)
      if (off >= a->ranges[i].start && off < a->ranges[i].end)
         return i;
   return -1;
}

static void memshm_erase_range(memshm_area_t *a, int i)
{
   int j;
   for (j = i; j < a->range_count - 1; j++)
      a->ranges[j] = a->ranges[j + 1];
   a->range_count--;
}

static bool memshm_insert_range(memshm_area_t *a, size_t start, size_t end)
{
   int i, j;
   if (a->range_count >= MEMSHM_MAX_PLACEHOLDERS)
      return false;
   for (i = 0; i < a->range_count; i++)
      if (a->ranges[i].start > start)
         break;
   for (j = a->range_count; j > i; j--)
      a->ranges[j] = a->ranges[j - 1];
   a->ranges[i].start = start;
   a->ranges[i].end   = end;
   a->range_count++;
   return true;
}
#endif

#if defined(_WIN32) && !defined(_XBOX)
/* ------------------------------------------------------------------ */
/* The legacy path: Windows 2000 through 10 1709                        */
/* ------------------------------------------------------------------ */
/* Without the placeholder APIs a range cannot be carved out of a
 * reservation: MEM_RELEASE takes the base of a whole VirtualAlloc and
 * frees all of it, and MapViewOfFileEx refuses an address that is
 * already reserved. So the span is reserved one allocation-granularity
 * slot at a time -- each slot its own VirtualAlloc, each individually
 * releasable -- and mapping a range releases exactly the slots it
 * covers and maps a view over them.
 *
 * Between that release and the map, the address is briefly free, and
 * another thread in the process could allocate there. The window is a
 * few microseconds at a specific address a general allocator has no
 * reason to pick, but it is real: the map is retried, and on a second
 * failure the slots are re-reserved and the call fails cleanly rather
 * than leaving a hole in the span.
 *
 * Runtime is unaffected either way. Once a view is mapped these are
 * ordinary page-table entries and a fastmem load is the same
 * instruction; the difference is one extra syscall per map, on a path
 * that runs at setup and when the guest remaps. */

static size_t memshm_granularity(void)
{
   static size_t g;
   if (!g)
   {
      SYSTEM_INFO si;
      GetSystemInfo(&si);
      g = (size_t)si.dwAllocationGranularity;
      if (!g)
         g = 65536;
   }
   return g;
}

/* Reserve every slot in [first, first + count). Rolls back on failure so
 * the span is never left partly reserved. */
static bool memshm_legacy_reserve(memshm_area_t *a, size_t first, size_t count)
{
   size_t i;
   for (i = 0; i < count; i++)
   {
      if (a->reserved[first + i])
         continue;
      if (!VirtualAlloc(a->base + (first + i) * a->slot, a->slot,
               MEM_RESERVE, PAGE_NOACCESS))
      {
         while (i--)
         {
            VirtualFree(a->base + (first + i) * a->slot, 0, MEM_RELEASE);
            a->reserved[first + i] = 0;
         }
         return false;
      }
      a->reserved[first + i] = 1;
   }
   return true;
}

static void memshm_legacy_release(memshm_area_t *a, size_t first, size_t count)
{
   size_t i;
   for (i = 0; i < count; i++)
   {
      if (!a->reserved[first + i])
         continue;
      VirtualFree(a->base + (first + i) * a->slot, 0, MEM_RELEASE);
      a->reserved[first + i] = 0;
   }
}

#endif

memshm_area_t *memshm_area_create(size_t len)
{
#if (defined(_WIN32) && !defined(_XBOX)) || (defined(HAVE_MMAN) && !defined(__EMSCRIPTEN__) && defined(MAP_ANONYMOUS))
   memshm_area_t *a;
#endif
#if defined(_WIN32) && !defined(_XBOX)
#if defined(MEMSHM_HAVE_PLACEHOLDERS)
   void *alloc = NULL;
   uintptr_t floor;
#endif

   if (!memshm_placeholder_apis())
   {
      /* Legacy: find a free span, then reserve it slot by slot. The
       * probe reserves the whole span to learn an address the system
       * considers free, releases it, and immediately re-reserves the
       * slots; anything that steals part of it in between makes the
       * re-reservation fail and the call reports none. */
      size_t gran = memshm_granularity();
      size_t slots, i;
      void  *probe;
      if (len % gran)
         return NULL;              /* a span must be whole slots */
      slots = len / gran;
      probe = VirtualAlloc(NULL, len, MEM_RESERVE, PAGE_NOACCESS);
      if (!probe)
         return NULL;
      VirtualFree(probe, 0, MEM_RELEASE);
      a = (memshm_area_t*)calloc(1, sizeof(*a));
      if (!a)
         return NULL;
      a->reserved = (unsigned char*)calloc(slots, 1);
      if (!a->reserved)
      {
         free(a);
         return NULL;
      }
      a->base       = (unsigned char*)probe;
      a->len        = len;
      a->slot       = gran;
      a->slot_count = slots;
      a->legacy     = 1;
      for (i = 0; i < slots; i++)
      {
         if (!VirtualAlloc(a->base + i * gran, gran, MEM_RESERVE, PAGE_NOACCESS))
         {
            memshm_legacy_release(a, 0, i);
            free(a->reserved);
            free(a);
            return NULL;
         }
         a->reserved[i] = 1;
      }
      return a;
   }

#if defined(MEMSHM_HAVE_PLACEHOLDERS)
   /* Ask for a base above 4 GB. Below that, a 4 GB window either
    * crosses the 32-bit boundary into the system's own reservations
    * around KUSER_SHARED_DATA, or on a fragmented address space starts
    * low and aliases mapped DLL and heap pages -- and then a fault in
    * the window cannot be told from a real one, because the window's
    * end is its start plus 0xFFFFFFFF and both are in range.
    *
    * A single constrained request can fail outright on a fragmented
    * address space even with room higher up, and falling straight back
    * to an unconstrained one tends to land low and be rejected below.
    * Sweeping a ladder of floors recovers a usable placement that one
    * attempt gives up on. MEM_ADDRESS_REQUIREMENTS arrived in the same
    * Windows release as the placeholder flags, so it narrows nothing. */
   /* Only on a 64-bit address space: on 32-bit Windows there is no
    * "above 4 GB", the floor constants do not fit a pointer, and the
    * window a 32-bit process reserves cannot overlap the way the
    * comment above describes. */
#if defined(_WIN64)
   for (floor = 0x100000000ULL; floor <= 0x900000000ULL; floor += 0x100000000ULL)
   {
      MEM_ADDRESS_REQUIREMENTS req;
      MEM_EXTENDED_PARAMETER   param;
      memset(&req, 0, sizeof(req));
      memset(&param, 0, sizeof(param));
      req.LowestStartingAddress = (PVOID)floor;
      param.Type    = MemExtendedParameterAddressRequirements;
      param.Pointer = &req;
      alloc = s_VirtualAlloc2(GetCurrentProcess(), NULL, len,
            MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, &param, 1);
      if (alloc)
         break;
   }
#else
   (void)floor;
#endif
   if (!alloc)
      alloc = s_VirtualAlloc2(GetCurrentProcess(), NULL, len,
            MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, NULL, 0);
   if (!alloc)
      return NULL;
#if defined(_WIN64)
   if ((uintptr_t)alloc < 0x100000000ULL)
   {
      /* Unusable placement, as above: release it and report none. */
      VirtualFree(alloc, 0, MEM_RELEASE);
      return NULL;
   }
#endif
   a = (memshm_area_t*)calloc(1, sizeof(*a));
   if (!a)
   {
      VirtualFree(alloc, 0, MEM_RELEASE);
      return NULL;
   }
   a->base = (unsigned char*)alloc;
   a->len  = len;
   a->ranges[0].start = 0;
   a->ranges[0].end   = len;
   a->range_count     = 1;
   return a;
#else
   return NULL;   /* no placeholders in this SDK, and legacy handled above */
#endif
#elif defined(HAVE_MMAN) && !defined(__EMSCRIPTEN__) && defined(MAP_ANONYMOUS)
   void *alloc = mmap(NULL, len, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
   if (alloc == MAP_FAILED)
      return NULL;
   a = (memshm_area_t*)calloc(1, sizeof(*a));
   if (!a)
   {
      munmap(alloc, len);
      return NULL;
   }
   a->base = (unsigned char*)alloc;
   a->len  = len;
   return a;
#else
   (void)len;
   return NULL;
#endif
}

void memshm_area_free(memshm_area_t *area)
{
   if (!area)
      return;
#if defined(_WIN32) && !defined(_XBOX)
   if (area->legacy)
   {
      /* Each slot is its own reservation or a mapped view; both have to
       * go one at a time. */
      size_t i;
      for (i = 0; i < area->slot_count; i++)
      {
         if (area->reserved[i])
            VirtualFree(area->base + i * area->slot, 0, MEM_RELEASE);
         else
            UnmapViewOfFile(area->base + i * area->slot);
      }
      free(area->reserved);
      free(area);
      return;
   }
   /* Every placeholder and every mapping goes with the reservation. */
   VirtualFree(area->base, 0, MEM_RELEASE);
#elif defined(HAVE_MMAN) && !defined(__EMSCRIPTEN__) && defined(MAP_FIXED) && defined(MAP_ANONYMOUS)
   munmap(area->base, area->len);
#endif
   free(area);
}

unsigned char *memshm_area_base(const memshm_area_t *area)
{
   return area ? area->base : NULL;
}

size_t memshm_area_size(const memshm_area_t *area)
{
   return area ? area->len : 0;
}

#if (defined(_WIN32) && !defined(_XBOX)) || (defined(HAVE_MMAN) && !defined(__EMSCRIPTEN__) && defined(MAP_FIXED) && defined(MAP_ANONYMOUS))
/* Offset of [at, at + len) within the reservation, or false if any part
 * of it falls outside. The address is only known to belong to the area
 * once this says so, so it is carried as uintptr_t: subtracting an
 * unrelated pointer from base is undefined, and the map is destructive
 * enough - MAP_FIXED replaces whatever is already there - that the
 * check cannot be left to the caller. Ordered so neither the addition
 * nor the subtraction can wrap. */
static bool memshm_area_offset(const memshm_area_t *area, const void *at,
      size_t len, size_t *off)
{
   uintptr_t base;
   uintptr_t addr;

   if (!area || !at || !len)
      return false;
   base = (uintptr_t)area->base;
   addr = (uintptr_t)at;
   if (     addr < base
       ||   len > area->len
       ||  (addr - base) > (area->len - len))
      return false;
   *off = (size_t)(addr - base);
   return true;
}
#endif

unsigned char *memshm_area_map(memshm_area_t *area, void *handle,
      size_t offset, void *at, size_t len, int prot)
{
#if defined(_WIN32) && !defined(_XBOX)
   size_t map_off;
#if defined(MEMSHM_HAVE_PLACEHOLDERS)
   size_t old_end;
   size_t range_start;
   int    idx;
   int    need;
   DWORD  page_prot;
#endif

   if (!memshm_area_offset(area, at, len, &map_off))
      return NULL;

   if (area->legacy)
   {
      size_t first = map_off / area->slot;
      size_t count = (len + area->slot - 1) / area->slot;
      void  *view;
      DWORD  access;
      int    attempt;
      if ((map_off % area->slot) || (first + count) > area->slot_count)
         return NULL;              /* must start and end on a slot */
      access = FILE_MAP_READ;
      if (prot & PROT_WRITE)
         access |= FILE_MAP_WRITE;
      if (prot & PROT_EXEC)
         access |= FILE_MAP_EXECUTE;
      /* Release the slots, then map over them. If something takes the
       * address in between, retry once; if that fails too, put the
       * reservations back so the span stays whole. */
      for (attempt = 0; attempt < 2; attempt++)
      {
         memshm_legacy_release(area, first, count);
         view = MapViewOfFileEx((HANDLE)handle, access,
               (DWORD)(((uint64_t)offset) >> 32),
               (DWORD)(offset & 0xFFFFFFFFu), len, at);
         if (view == at)
         {
            area->mappings++;
            return (unsigned char*)view;
         }
         if (view)
            UnmapViewOfFile(view);   /* landed elsewhere: not usable */
         if (!memshm_legacy_reserve(area, first, count))
            return NULL;
      }
      return NULL;
   }

#if defined(MEMSHM_HAVE_PLACEHOLDERS)
   idx         = memshm_find_range(area, map_off);
   if (idx < 0)
      return NULL;   /* not a placeholder: something is still mapped there */
   range_start = area->ranges[idx].start;
   old_end     = area->ranges[idx].end;
   /* The whole range has to be inside that placeholder, not just its
    * first byte: a len reaching past the end would split at an offset
    * the kernel does not have a placeholder for, and record a range
    * running backwards. */
   if (len > old_end - map_off)
      return NULL;

   /* Each split the kernel makes has to be recordable, so refuse a full
    * table before touching the OS rather than after. */
   need = 0;
   if (map_off != range_start)
      need++;
   if ((map_off + len) != old_end)
      need++;
   if ((area->range_count + need) > MEMSHM_MAX_PLACEHOLDERS)
      return NULL;

   /* Split off anything to the left of this range, then anything to the
    * right; what is left is exactly the range being replaced. The OS
    * call comes first in each step and the table records the split only
    * once the kernel has made it, so every failure here returns with
    * the two still describing the same thing. */
   if (map_off != range_start)
   {
      if (!VirtualFree(area->base + range_start, map_off - range_start,
               MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER))
         return NULL;
      area->ranges[idx].end = map_off;
      if (    !memshm_insert_range(area, map_off, old_end)
          || ((idx = memshm_find_range(area, map_off)) < 0))
         return NULL;
   }

   if ((map_off + len) != old_end)
   {
      if (!VirtualFree(area->base + map_off, len,
               MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER))
         return NULL;
      area->ranges[idx].start = map_off + len;
      if (    !memshm_insert_range(area, map_off, map_off + len)
          || ((idx = memshm_find_range(area, map_off)) < 0))
         return NULL;
   }

   /* ranges[idx] is now exactly the placeholder being replaced. */
   if (!s_MapViewOfFile3((HANDLE)handle, GetCurrentProcess(), at,
            (ULONG64)offset, len, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0))
      return NULL;   /* still a free placeholder, and still recorded as one */
   memshm_erase_range(area, idx);

   if (prot & PROT_READ)
      page_prot = (prot & PROT_EXEC)
         ? ((prot & PROT_WRITE) ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ)
         : ((prot & PROT_WRITE) ? PAGE_READWRITE : PAGE_READONLY);
   else
      page_prot = PAGE_NOACCESS;
   if (page_prot != PAGE_READWRITE)
   {
      DWORD old_prot;
      VirtualProtect(at, len, page_prot, &old_prot);
   }
   area->mappings++;
   return (unsigned char*)at;
#else
   return NULL;
#endif
#elif defined(HAVE_MMAN) && !defined(__EMSCRIPTEN__) && defined(MAP_FIXED) && defined(MAP_ANONYMOUS)
   void  *p;
   size_t map_off;
   size_t span = len;
   size_t page = mempagesize();

   /* mmap rounds the length up to a page and MAP_FIXED replaces
    * whatever is mapped over the whole of it, so the rounded span is
    * what has to fit inside the reservation. */
   if (page && (span % page))
   {
      if (span > (((size_t)-1) - page))
         return NULL;
      span += page - (span % page);
   }
   if (!memshm_area_offset(area, at, span, &map_off))
      return NULL;
   p = mmap(at, len, prot, MAP_SHARED | MAP_FIXED,
         MEMSHM_FD(handle), (off_t)offset);
   if (p == MAP_FAILED)
      return NULL;
   area->mappings++;
   return (unsigned char*)p;
#else
   (void)area; (void)handle; (void)offset; (void)at; (void)len; (void)prot;
   return NULL;
#endif
}

bool memshm_area_unmap(memshm_area_t *area, void *at, size_t len)
{
#if defined(_WIN32) && !defined(_XBOX)
   size_t map_off;
#if defined(MEMSHM_HAVE_PLACEHOLDERS)
   int    cur, left, right;
#endif

   if (!memshm_area_offset(area, at, len, &map_off))
      return false;
   /* Nothing is mapped, so this address cannot be one this returned:
    * an unbalanced unmap would otherwise wrap the count and take the
    * area's own teardown with it. */
   if (!area->mappings)
      return false;

   if (area->legacy)
   {
      size_t first = map_off / area->slot;
      size_t count = (len + area->slot - 1) / area->slot;
      if ((map_off % area->slot) || (first + count) > area->slot_count)
         return false;
      if (!UnmapViewOfFile(at))
         return false;
      /* Put the reservations back at once: an unreserved hole in the
       * span is an address another allocation can take, and then the
       * next map there fails for good. */
      if (!memshm_legacy_reserve(area, first, count))
         return false;
      area->mappings--;
      return true;
   }

#if defined(MEMSHM_HAVE_PLACEHOLDERS)
   if (!s_UnmapViewOfFile2(GetCurrentProcess(), at, MEM_PRESERVE_PLACEHOLDER))
      return false;
   area->mappings--;

   /* The range is a free placeholder of its own now, whether or not it
    * goes on to coalesce with a neighbour, so record that before either
    * attempt: each VirtualFree below can fail, and the table has to
    * describe the kernel either way. */
   if (    !memshm_insert_range(area, map_off, map_off + len)
       || ((cur = memshm_find_range(area, map_off)) < 0))
      return false;

   /* Coalesce with the placeholder on the left, if the range that ends
    * where this one starts is one. */
   left = (map_off > 0) ? memshm_find_range(area, map_off - 1) : -1;
   if (left >= 0 && left != cur)
   {
      /* Checked: KernelBase validates the flags and forwards, so the
       * kernel is what decides whether this really is two adjacent
       * placeholders. A disagreement between these ranges and the VADs
       * comes back here, and silently ignoring it would let the two
       * drift apart until a later map fails for no visible reason. */
      if (!VirtualFree(area->base + area->ranges[left].start,
            area->ranges[cur].end - area->ranges[left].start,
            MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS))
         return false;
      area->ranges[left].end = area->ranges[cur].end;
      memshm_erase_range(area, cur);
      /* left sorts below cur, so erasing cur leaves its index alone. */
      cur = left;
   }

   /* And with the one on the right, re-found because the merge above
    * shifts every index past it down by one. */
   right = ((map_off + len) < area->len)
      ? memshm_find_range(area, map_off + len) : -1;
   if (right >= 0 && right != cur)
   {
      if (!VirtualFree(area->base + area->ranges[cur].start,
            area->ranges[right].end - area->ranges[cur].start,
            MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS))
         return false;
      area->ranges[cur].end = area->ranges[right].end;
      memshm_erase_range(area, right);
   }
   return true;
/* MAP_ANONYMOUS as well as MAP_FIXED: the body below needs both, and
 * memshm_area_create's guard requires both, so a platform with only one
 * of them must reach the stub in every one of these functions rather
 * than compile a body that names a macro it does not have. */
#else
   return false;
#endif
#elif defined(HAVE_MMAN) && !defined(__EMSCRIPTEN__) && defined(MAP_FIXED) && defined(MAP_ANONYMOUS)
   size_t map_off;
   size_t span = len;
   size_t page = mempagesize();

   if (page && (span % page))
   {
      if (span > (((size_t)-1) - page))
         return false;
      span += page - (span % page);
   }
   if (!memshm_area_offset(area, at, span, &map_off))
      return false;
   if (!area->mappings)
      return false;
   /* Anonymous PROT_NONE back over the range: the reservation is whole
    * again and the kernel merges the VMAs. */
   if (mmap(at, len, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == MAP_FAILED)
      return false;
   area->mappings--;
   return true;
#else
   (void)area; (void)at; (void)len;
   return false;
#endif
}
