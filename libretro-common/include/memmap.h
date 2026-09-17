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
