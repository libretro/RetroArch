/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (memmap.h).
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

#ifndef _LIBRETRO_MEMMAP_H
#define _LIBRETRO_MEMMAP_H

#include <stdio.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#if defined(PSP) || defined(PS2) || defined(GEKKO) || defined(VITA) || defined(_XBOX) || defined(_3DS) || defined(WIIU) || defined(SWITCH) || defined(HAVE_LIBNX) || defined(__PS3__) || defined(__PSL1GHT__)
/* No mman available */
#elif defined(_WIN32) && !defined(_XBOX)
/* MSVC's minwindef.h defines min and max as macros in C++ as well as C
 * -- MinGW's is guarded by #ifndef __cplusplus -- and they then break
 * every std::numeric_limits<>::max() in any C++ file that reaches this
 * header. A public header must not leak them. */
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <errno.h>
#include <io.h>
#else
#define HAVE_MMAN
#include <sys/mman.h>
#endif

RETRO_BEGIN_DECLS

#if !defined(HAVE_MMAN) || defined(_WIN32)
/* PROT_/MAP_ request bits for the shim platforms (real <sys/mman.h>
 * provides them elsewhere).  Only the combinations the shim can
 * honour are defined. */
#ifndef PROT_NONE
#define PROT_NONE       0x0
#endif
#ifndef PROT_READ
#define PROT_READ       0x1
#endif
#ifndef PROT_WRITE
#define PROT_WRITE      0x2
#endif
#ifndef PROT_EXEC
#define PROT_EXEC       0x4
#endif
#ifndef MAP_PRIVATE
#define MAP_PRIVATE     0x02
#endif
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS   0x20
#endif
#ifndef MAP_ANON
#define MAP_ANON        MAP_ANONYMOUS
#endif
#ifndef MAP_FAILED
#define MAP_FAILED      ((void *)-1)
#endif
void* mmap(void *addr, size_t len, int mmap_prot, int mmap_flags, int fildes, size_t off);

int munmap(void *addr, size_t len);

int mprotect(void *addr, size_t len, int prot);
#endif

int memsync(void *start, void *end);

int memprotect(void *addr, size_t len);

/* --------------------------------------------------------------------
 * Reserve/commit
 *
 * A different model from mmap() above, and not expressible through it:
 * reserve a range of address space with no physical pages behind it,
 * then commit and decommit sub-ranges as they are needed. Windows has
 * this natively (MEM_RESERVE / MEM_COMMIT / MEM_DECOMMIT); on POSIX it
 * is PROT_NONE plus mprotect() and MADV_DONTNEED.
 *
 * The mmap() shim above cannot stand in for it: on Windows it maps
 * SEC_COMMIT sections, so the whole length is committed up front,
 * which is the one thing a reservation exists to avoid.
 *
 * Useful when a consumer needs one stable base covering a large range
 * while only a moving part of it is resident - a streaming window over
 * a file, say. Where the platform has no reservation at all,
 * memreserve() returns NULL and the caller is expected to fall back to
 * holding the range outright.
 * -------------------------------------------------------------------- */

/**
 * mempagesize:
 *
 * Returns: the granularity commit and decommit operate on. Ranges
 * passed to the functions below are rounded to it internally, so
 * callers only need it when they want to align their own bookkeeping.
 * Returns 0 if reservations are unsupported.
 */
size_t mempagesize(void);

/**
 * memreserve:
 * @len        : bytes of address space to reserve
 *
 * Reserves @len bytes with nothing committed behind them. The range is
 * unreadable and unwritable until memcommit() covers it.
 *
 * Returns: the base of the reservation, or NULL if reservations are
 * unsupported or the request failed.
 */
void *memreserve(size_t len);

/**
 * memreserve_at:
 * @hint       : preferred base, or NULL for any.
 * @len        : bytes; rounded up to a page.
 *
 * As memreserve, at a preferred address -- a fastmem window that must
 * sit where the recompiler expects it. A hint, never forced: the
 * platform may return another address, and the caller compares.
 */
void *memreserve_at(void *hint, size_t len);

/**
 * memcommit:
 * @addr       : start of the sub-range, within a memreserve() result
 * @len        : bytes to make readable and writable
 *
 * Returns: true on success.
 */
bool memcommit(void *addr, size_t len);

/**
 * memrearm:
 * @addr       : start of the sub-range, within a memreserve() result
 * @len        : length of the sub-range
 *
 * Make a committed range unreadable again while KEEPING its physical
 * pages.  This is the half of memdecommit() that costs nothing to
 * undo: memdecommit() also hands the pages back, so the next write to
 * the range faults them in again, whereas a range taken back with
 * memrearm() is re-armed by memcommit() without a fault.
 *
 * For recycling a reservation whose pages are about to be overwritten
 * anyway - the buffer pool in data_transfer - where the guard matters
 * and the page contents do not.  Use memdecommit() when the point is
 * to give the memory back.
 *
 * Returns false where reservations are unsupported.
 */
bool memrearm(void *addr, size_t len);

/**
 * memdecommit:
 * @addr       : start of the sub-range
 * @len        : bytes to release the physical pages of
 * @strict     : when true, also make the range fault on access rather
 *               than read back as zeroes. Costs an extra syscall on
 *               POSIX; useful for proving a consumer does not read
 *               behind itself.
 *
 * The address space stays reserved either way; only the pages go.
 */
void memdecommit(void *addr, size_t len, bool strict);

/**
 * memrelease:
 * @addr       : base returned by memreserve()
 * @len        : the length passed to memreserve()
 *
 * Releases the reservation and everything committed in it.
 */
void memrelease(void *addr, size_t len);

/**
 * memshm_create:
 * @name       : a name unique to this process, e.g. built from the pid.
 * @len        : size in bytes.
 *
 * Creates an anonymous-backed shared memory region that can be mapped
 * at more than one address with memshm_map -- how an emulator mirrors
 * a guest RAM into several places in a large reserved window. The name
 * is unlinked at once where the platform links it, so it never outlives
 * the process. Windows uses a pagefile-backed file mapping, Android a
 * memfd (Bionic has no shm_open), everything else shm_open.
 *
 * Darwin caps a name at 31 characters and returns ENAMETOOLONG past it,
 * where Linux takes one ten times longer; a name that will not fit is
 * shortened from the front there, keeping the tail that distinguishes
 * one process from another, so the same caller works on both.
 *
 * Returns: a handle for memshm_map / memshm_destroy, or NULL. It is the
 * platform's own object -- the file descriptor on POSIX, the HANDLE on
 * Windows -- so a caller that must map the region a way memshm_map does
 * not offer, such as MAP_FIXED into a reservation it owns, can use it.
 * Never 0 on POSIX. On platforms with no mman this is always NULL.
 */
void *memshm_create(const char *name, size_t len);

/**
 * memshm_destroy:
 * @handle     : from memshm_create.
 *
 * Closes the handle. Mappings made from it stay valid until unmapped.
 */
void memshm_destroy(void *handle);

/**
 * memshm_map:
 * @handle     : from memshm_create.
 * @offset     : byte offset into the region; page-aligned.
 * @hint       : preferred address, or NULL. A hint, never forced: on a
 *               taken address the platform picks another, and the
 *               caller compares the result against what it asked for.
 * @len        : bytes to map.
 * @prot       : PROT_READ | PROT_WRITE | PROT_EXEC as memmap.h defines.
 *
 * Returns: the mapped address, or NULL.
 */
void *memshm_map(void *handle, size_t offset, void *hint, size_t len, int prot);

/**
 * memshm_unmap:
 * @addr       : from memshm_map.
 * @len        : the length passed to memshm_map.
 */
void memshm_unmap(void *addr, size_t len);

/* ------------------------------------------------------------------ */
/* A reservation that shared memory is mapped into at several places   */
/* ------------------------------------------------------------------ */

/* A fastmem window: one contiguous reservation of address space that
 * pieces of a guest RAM are mapped into, at offsets the recompiler
 * computes, and unmapped from again as the guest remaps. The
 * reservation must stay reserved while individual pieces come and go,
 * which neither mmap nor VirtualAlloc gives directly:
 *
 *  - POSIX: the reservation is PROT_NONE anonymous memory, a piece is
 *    MAP_FIXED over it, and unmapping restores PROT_NONE anonymous
 *    memory in its place. The kernel splits and merges the VMAs.
 *  - Windows 10 1803 and later: the reservation is a placeholder, a
 *    piece replaces part of it with MEM_REPLACE_PLACEHOLDER, and
 *    unmapping restores the placeholder and coalesces it with its
 *    neighbours. The area tracks the ranges itself, because the API
 *    cannot be asked where they are.
 *  - Windows 2000 through 10 1709: no placeholder APIs, and
 *    MEM_RELEASE frees a whole reservation rather than part of one, so
 *    the span is reserved one allocation-granularity slot at a time.
 *    Mapping releases exactly the slots it covers and maps a view over
 *    them; unmapping restores those reservations at once.
 *
 * Runtime is the same either way: a mapped view is a mapped view, and a
 * load through the window is the same instruction. The legacy path
 * costs one extra system call per map and carries a brief window where
 * the address is free, which it retries through -- both on a path that
 * runs at setup and when the guest remaps, not per access.
 */
typedef struct memshm_area memshm_area_t;

/**
 * memshm_area_create:
 * @len        : size of the reservation in bytes.
 *
 * Returns: the area, or NULL where there is no address space for it, or
 * no way to reserve one. A caller that gets NULL runs without a fastmem
 * window rather than failing. @len must be a whole number of allocation
 * granularity units on Windows.
 *
 * On Windows the base is placed above 4 GB where possible: a 4 GB
 * window based below that either crosses the 32-bit boundary into the
 * system's own reservations or aliases mapped pages, and a fault there
 * cannot be told apart from a legitimate miss.
 */
memshm_area_t *memshm_area_create(size_t len);

/**
 * memshm_area_free:
 * Releases the reservation and everything still mapped in it.
 */
void memshm_area_free(memshm_area_t *area);

/**
 * memshm_area_base / memshm_area_size:
 */
unsigned char *memshm_area_base(const memshm_area_t *area);
size_t         memshm_area_size(const memshm_area_t *area);

/**
 * memshm_area_map:
 * @area       : from memshm_area_create.
 * @handle     : from memshm_create.
 * @offset     : byte offset into the shared region; page-aligned.
 * @at         : where in the area to map it, within the reservation.
 * @len        : bytes.
 * @prot       : PROT_READ | PROT_WRITE | PROT_EXEC.
 *
 * Returns: @at on success, NULL on failure.
 */
unsigned char *memshm_area_map(memshm_area_t *area, void *handle,
      size_t offset, void *at, size_t len, int prot);

/**
 * memshm_area_unmap:
 * @at         : an address a previous memshm_area_map returned.
 * @len        : the length it was mapped with.
 *
 * Restores the reservation over that range. Returns true on success.
 */
bool memshm_area_unmap(memshm_area_t *area, void *at, size_t len);

/**
 * memjit_write_begin / memjit_write_end:
 *
 * On a platform whose JIT pages are write-xor-execute per thread --
 * Apple Silicon, where MAP_JIT memory is executable until the thread
 * asks otherwise -- these switch the calling thread to writing and back
 * (pthread_jit_write_protect_np). Nesting is counted, so a writer that
 * calls another writer is fine. Everywhere else they do nothing.
 */
void memjit_write_begin(void);
void memjit_write_end(void);

RETRO_END_DECLS

#endif
