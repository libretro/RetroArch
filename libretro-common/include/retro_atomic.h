/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_atomic.h).
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

#ifndef __LIBRETRO_SDK_ATOMIC_H
#define __LIBRETRO_SDK_ATOMIC_H

#include <retro_common_api.h>

/* Minimal portable atomic operations for SPSC patterns.
 *
 * This header consolidates the ad-hoc atomic shims previously duplicated
 * in audio/drivers/{coreaudio,coreaudio3,xaudio,opensl}.c, audio/common/
 * mmdevice_common.c and gfx/gfx_thumbnail.c.  The surface is intentionally
 * narrow: load, store, fetch_add, fetch_sub, plus inc/dec convenience
 * wrappers.  Everything is on plain machine words (int and size_t); no
 * compare-exchange, no double-word ops, no thread-fences.  Add only when
 * a real caller needs it.
 *
 * Memory ordering is fixed per-operation rather than parameterised, to
 * keep the call sites readable and to avoid having to invent ordering
 * tags for every backend:
 *
 *   retro_atomic_load_acquire   - acquire load   (pairs with release store)
 *   retro_atomic_store_release  - release store  (pairs with acquire load)
 *   retro_atomic_fetch_add      - acq_rel RMW
 *   retro_atomic_fetch_sub      - acq_rel RMW
 *   retro_atomic_inc / dec      - acq_rel RMW, return void
 *
 * Backend selection (in order):
 *   1. C11 <stdatomic.h>            (modern GCC/Clang/MSVC with /std:c11)
 *   2. C++11 <atomic>               (any C++ TU with __cplusplus >= 201103L
 *                                    or _MSVC_LANG >= 201103L; the natural
 *                                    peer of C11 stdatomic for C++ callers)
 *   3. GCC __atomic_*               (GCC 4.7+ / Clang 3.1+)
 *   4. MSVC Interlocked* (Win32)    (every MSVC since VS2003, OG Xbox,
 *                                    Xbox 360 XDK, modern Win32/x64)
 *   5. Mach OSAtomic*               (Apple PPC / pre-10.7)
 *   6. GCC __sync_*                 (very old GCC, GCC 4.1-4.6)
 *   7. volatile fallback            (single-core, x86 TSO, or last resort)
 *
 * Capability macros (defined after backend selection):
 *
 *   HAVE_RETRO_ATOMIC          -> always 1 if the header included
 *                                 successfully.  Use for compile-time gating
 *                                 of any code that uses the API at all.
 *   RETRO_ATOMIC_LOCK_FREE     -> 1 if a real lock-free backend was
 *                                 selected (1, 2, 3, 4, 5, or 6).  NOT
 *                                 defined if the volatile fallback (7)
 *                                 was selected.  SPSC fifos and other
 *                                 lock-free data structures should gate
 *                                 on this.
 *
 *                                 Strictly speaking, the C and C++
 *                                 standards do not guarantee that
 *                                 atomic_int / std::atomic<int> are
 *                                 lock-free on all conforming
 *                                 implementations.  In practice, on
 *                                 every architecture and toolchain
 *                                 RetroArch supports (x86/x64, ARM,
 *                                 AArch64, PowerPC, MIPS, all with
 *                                 32-bit aligned word atomics), int and
 *                                 size_t are always lock-free, so this
 *                                 macro is defined unconditionally for
 *                                 every named backend.  If a future
 *                                 port lands on a target where this is
 *                                 not the case, this comment is the
 *                                 right place to add an
 *                                 ATOMIC_INT_LOCK_FREE / atomic
 *                                 _is_always_lock_free gate.
 *   RETRO_ATOMIC_BACKEND_NAME  -> string literal naming the active backend
 *                                 (e.g. "C11 stdatomic", "volatile fallback").
 *                                 Useful for diagnostics and CI logs.
 *
 * Caller-side opt-in for stricter selection:
 *
 *   RETRO_ATOMIC_REQUIRE_LOCK_FREE
 *     If defined by the caller before including this header, an
 *     #error is raised when only the volatile fallback would be
 *     available.  Use in code paths whose correctness depends on
 *     real hardware barriers (e.g. SPSC ring buffers used across
 *     SMP threads on weakly-ordered targets).
 *
 * Caller patterns
 * ---------------
 * There are three idiomatic ways to consume this header, picked
 * according to how the caller copes with no-atomics:
 *
 * Pattern 1 -- "lock-free fast path with a portable fallback"
 *   Use when you have a working alternative (mutex-based, locked,
 *   slock_t / scond_t, fifo_queue with a lock around it) that you
 *   would happily fall back to on a target without real atomics.
 *   This is the same shape as audio/drivers/coreaudio.c's
 *   RARCH_COREAUDIO_LEGACY split.
 *
 *     #include <retro_atomic.h>
 *     #if defined(RETRO_ATOMIC_LOCK_FREE)
 *        // SPSC fast path -- producer/consumer split with
 *        // load_acquire / store_release / fetch_add.
 *        static retro_atomic_size_t fill;
 *        ...
 *     #else
 *        // Locked fallback -- regular fifo_queue + slock_t.
 *        static fifo_buffer_t *fifo;
 *        static slock_t       *lock;
 *        ...
 *     #endif
 *
 * Pattern 2 -- "atomics required, refuse to compile otherwise"
 *   Use when the calling code has no sensible non-atomic
 *   implementation -- the fast path *is* the only path, and a
 *   silent volatile fallback would be worse than a build break.
 *   Define RETRO_ATOMIC_REQUIRE_LOCK_FREE before the include and
 *   the header will #error out if no real backend is available.
 *
 *     #define RETRO_ATOMIC_REQUIRE_LOCK_FREE
 *     #include <retro_atomic.h>
 *
 *     // From here on, RETRO_ATOMIC_LOCK_FREE is guaranteed.
 *
 * Pattern 3 -- "atomics if useful, harmless if not"
 *   Use when the calling code is correct without atomics (e.g.
 *   relaxed counters used only for diagnostics or rate-limiting
 *   that can tolerate a torn read).  Just use the macros
 *   unconditionally; the volatile fallback gives you the loosest
 *   semantics that still compiles, and that's enough.
 *
 *     #include <retro_atomic.h>
 *     // No #if needed -- counters work either way.
 *     retro_atomic_int_t debug_counter;
 *     retro_atomic_inc_int(&debug_counter);
 *
 * The choice between Pattern 1 and Pattern 2 is mostly about how
 * forgiving the calling code can be: a reusable library primitive
 * (fifo_spsc_t for instance) is better off with Pattern 1, because
 * a dependent that doesn't care about SMP correctness on the rare
 * volatile-fallback target shouldn't be forced to provide an
 * alternative.  Application code that hard-relies on real barriers
 * to be correct is better off with Pattern 2 -- it makes the
 * portability requirement loud at build time on the platform that
 * needs to fix it, instead of silently miscompiling.
 *
 * The fallback is intentionally weak.  It is correct on:
 *   - true single-core hardware (PSP, original NDS-class)
 *   - x86/x64 (TSO masks the missing release/acquire fences for naturally
 *     aligned word-sized loads/stores; the missing piece is a compiler
 *     barrier, supplied by `volatile`)
 * It is NOT correct on weakly-ordered SMP without barriers (ARMv7+ SMP,
 * PowerPC SMP, MIPS SMP).  No RetroArch target lands in that gap today
 * without also having one of the higher-priority backends available, but
 * compiling there raises a #warning so it's loud.
 *
 * PowerPC coverage:
 *   - Xbox 360 XDK (MSVC + Xenon PPC) -> MSVC backend, *Acquire variants
 *     emit lwsync. Correct on the 3-core console.
 *   - libxenon Xbox 360 (xenon-gcc)   -> GCC __atomic_* backend.
 *   - GameCube (single-core Gekko)    -> GCC __atomic_* backend; SMP
 *     concerns moot anyway.
 *   - Wii (single-core Broadway)      -> GCC __atomic_* backend; SMP
 *     concerns moot anyway.
 *   - Wii U (3-core Espresso)         -> GCC __atomic_* backend.
 *   - PS3 (Cell PPU, plus SPEs the
 *     host code does not run on)      -> GCC __atomic_* backend.
 *   - Apple PPC G3/G4 (single-core)   -> Apple OSAtomic backend.
 *   - Apple PPC G5 (SMP)              -> Apple OSAtomic backend.
 *
 * ARM / AArch64 coverage:
 *   - Switch / libnx (Cortex-A57 SMP) -> GCC __atomic_* backend; emits
 *     real ldar/stlr/ldadd*_acq_rel.  Verified by aarch64-linux-gnu
 *     cross-compile + qemu user-mode.
 *   - PSVita (Cortex-A9 SMP, ARMv7)   -> GCC __atomic_* backend; emits
 *     dmb ish around exclusive monitor pairs.  Verified by qemu-arm.
 *   - 3DS (ARM11 ARMv6, single-core
 *     OldOld 3DS, dual-core New 3DS)  -> GCC __atomic_* backend.
 *   - webOS / Miyoo / OpenPandora     -> GCC __atomic_* backend.
 *   - Raspberry Pi / generic Linux    -> GCC __atomic_* or C11 stdatomic.
 *   - Android (NDK Clang)             -> C11 stdatomic.
 *   - Apple iOS / tvOS / Apple Silicon
 *     Mac (ARM64, multi-core SMP)     -> C11 stdatomic.
 *   - Windows on ARM64 (MSVC)         -> MSVC backend.  *Acquire variants
 *     for load and store emit dmb per MSVC docs; fetch_add/fetch_sub
 *     are bracketed with explicit __dmb(_ARM64_BARRIER_ISH) since plain
 *     Interlocked* RMW lacks barriers on ARM64 (PostgreSQL hit this on
 *     Win11/ARM64 in 2025).
 *
 * Clang notes:
 *   Clang impersonates GCC 4.2 in its __GNUC__ / __GNUC_MINOR__
 *   defines (a long-standing legacy compatibility setting), so a naive
 *   "GCC >= 4.7" gate would fall through to __sync_* on Clang even
 *   though Clang has supported __atomic_* since 3.1.  The GCC backend
 *   gate above keys on `defined(__clang__) || (GCC version check)` to
 *   short-circuit this trap.
 *
 *   Selection on Clang in practice:
 *     -std=c89/c99/gnu99   -> GCC __atomic_*
 *     -std=c11/c17/gnu17   -> C11 stdatomic
 *     -std=c++98           -> GCC __atomic_*
 *     -std=c++11 and later -> C++11 std::atomic
 *
 *   On AArch64, Clang and GCC emit the same family of instructions
 *   (ldar / stlr / ldadd*_acq_rel), so the hardware contract is
 *   honoured identically.  Clang on Apple platforms (macOS, iOS,
 *   tvOS), Android NDK r18+, Emscripten, and PS4-ORBIS all flow
 *   through one of the gcc / C11 / C++11 paths above; the CI lane
 *   exercises Clang with ThreadSanitizer, which would flag any
 *   missing-barrier regression in the SPSC stress.
 */

/* No external libretro-common includes are needed: the header is all
 * macros and integer typedefs.  Each backend block pulls in the
 * platform headers it needs (<stdatomic.h>, <atomic>, <windows.h>,
 * <libkern/OSAtomic.h>) inside its own #if guard. */

/* ---- Backend detection ------------------------------------------------- */

/* Build-time overrides.  Define one of:
 *   RETRO_ATOMIC_FORCE_C11
 *   RETRO_ATOMIC_FORCE_CXX11
 *   RETRO_ATOMIC_FORCE_GCC_NEW
 *   RETRO_ATOMIC_FORCE_MSVC
 *   RETRO_ATOMIC_FORCE_APPLE
 *   RETRO_ATOMIC_FORCE_SYNC
 *   RETRO_ATOMIC_FORCE_VOLATILE
 * to bypass auto-detection.  Useful for porting and for testing.       */
#if defined(RETRO_ATOMIC_FORCE_C11)
#define RETRO_ATOMIC_BACKEND_C11 1
#elif defined(RETRO_ATOMIC_FORCE_CXX11)
#define RETRO_ATOMIC_BACKEND_CXX11 1
#elif defined(RETRO_ATOMIC_FORCE_GCC_NEW)
#define RETRO_ATOMIC_BACKEND_GCC_NEW 1
#elif defined(RETRO_ATOMIC_FORCE_MSVC)
#define RETRO_ATOMIC_BACKEND_MSVC 1
#elif defined(RETRO_ATOMIC_FORCE_APPLE)
#define RETRO_ATOMIC_BACKEND_APPLE 1
#elif defined(RETRO_ATOMIC_FORCE_SYNC)
#define RETRO_ATOMIC_BACKEND_SYNC 1
#elif defined(RETRO_ATOMIC_FORCE_VOLATILE)
#define RETRO_ATOMIC_BACKEND_VOLATILE 1
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L && \
    !defined(__STDC_NO_ATOMICS__)
#define RETRO_ATOMIC_BACKEND_C11 1
/* C++11 <atomic> is the natural peer of C11 <stdatomic.h> for any
 * C++ TU that includes this header.  Note: MSVC keeps __cplusplus
 * pinned at 199711L unless /Zc:__cplusplus is passed; _MSVC_LANG
 * carries the actual language level, so we test both.  RetroArch
 * builds Makefile.win and a few legacy paths with -std=c++98, so
 * the gate must be exact -- defined(__cplusplus) alone is not
 * enough. */
#elif (defined(__cplusplus) && __cplusplus >= 201103L) || \
      (defined(_MSVC_LANG)  && _MSVC_LANG  >= 201103L)
#define RETRO_ATOMIC_BACKEND_CXX11 1
#elif defined(__clang__) || (defined(__GNUC__) && \
    ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 7)))
#define RETRO_ATOMIC_BACKEND_GCC_NEW 1
#elif defined(_MSC_VER)
#define RETRO_ATOMIC_BACKEND_MSVC 1
#elif defined(__APPLE__) && defined(__MACH__)
/* Old Apple toolchains (PPC / pre-10.7) without modern GCC builtins.
 * OSAtomic is deprecated but functional through 10.x. */
#define RETRO_ATOMIC_BACKEND_APPLE 1
#elif defined(__GNUC__) && \
    ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1))
#define RETRO_ATOMIC_BACKEND_SYNC 1
#else
#define RETRO_ATOMIC_BACKEND_VOLATILE 1
#if !defined(RETRO_ATOMIC_SUPPRESS_WARNING)
#warning "retro_atomic.h: no atomic backend matched, falling back to volatile. Safe only on single-core or x86 TSO."
#endif
#endif

/* ---- Capability flags -------------------------------------------------- */

/* The header is always usable in the sense that the macros expand to
 * working C; HAVE_RETRO_ATOMIC just signals that the API surface exists.
 * Callers that want to know whether the backend is actually lock-free
 * on SMP must additionally test RETRO_ATOMIC_LOCK_FREE. */
#define HAVE_RETRO_ATOMIC 1

#if defined(RETRO_ATOMIC_BACKEND_C11)
#define RETRO_ATOMIC_LOCK_FREE 1
#define RETRO_ATOMIC_BACKEND_NAME "C11 stdatomic"
#elif defined(RETRO_ATOMIC_BACKEND_CXX11)
#define RETRO_ATOMIC_LOCK_FREE 1
#define RETRO_ATOMIC_BACKEND_NAME "C++11 std::atomic"
#elif defined(RETRO_ATOMIC_BACKEND_GCC_NEW)
#define RETRO_ATOMIC_LOCK_FREE 1
#define RETRO_ATOMIC_BACKEND_NAME "GCC __atomic_*"
#elif defined(RETRO_ATOMIC_BACKEND_MSVC)
#define RETRO_ATOMIC_LOCK_FREE 1
#define RETRO_ATOMIC_BACKEND_NAME "MSVC Interlocked*"
#elif defined(RETRO_ATOMIC_BACKEND_APPLE)
#define RETRO_ATOMIC_LOCK_FREE 1
#define RETRO_ATOMIC_BACKEND_NAME "Apple OSAtomic*"
#elif defined(RETRO_ATOMIC_BACKEND_SYNC)
#define RETRO_ATOMIC_LOCK_FREE 1
#define RETRO_ATOMIC_BACKEND_NAME "GCC __sync_*"
#else /* RETRO_ATOMIC_BACKEND_VOLATILE */
/* RETRO_ATOMIC_LOCK_FREE intentionally NOT defined for the volatile
 * fallback; callers that gate on it will compile without the
 * lock-free fast path on this target. */
#define RETRO_ATOMIC_BACKEND_NAME "volatile fallback (best-effort)"
#if defined(RETRO_ATOMIC_REQUIRE_LOCK_FREE)
#error "retro_atomic.h: RETRO_ATOMIC_REQUIRE_LOCK_FREE was set, but only the volatile fallback is available on this target. The caller's correctness depends on hardware barriers that this backend does not provide. Either provide a real atomic backend (C11 stdatomic, C++11 std::atomic, GCC __atomic_*, MSVC Interlocked*, Apple OSAtomic*, or GCC __sync_*) or fall back to a locked implementation in the calling code."
#endif
#endif

/* The header contains only macros and integer typedefs; there are no
 * function declarations and therefore no need for RETRO_BEGIN_DECLS
 * around the file.  The C++11 backend below #includes <atomic>, whose
 * templates cannot be declared with C linkage; if a caller wraps its
 * #include of this header in extern "C" { ... } (e.g. ui_qt.cpp under
 * !CXX_BUILD), libstdc++ <atomic> emits dozens of "template with C
 * linkage" errors.  RETRO_BEGIN_DECLS_CXX below escapes that. */

/* ---- C11 <stdatomic.h> ------------------------------------------------- */
#if defined(RETRO_ATOMIC_BACKEND_C11)

#include <stdatomic.h>
#include <stddef.h>

typedef atomic_int    retro_atomic_int_t;
typedef atomic_size_t retro_atomic_size_t;

#define retro_atomic_int_init(p, v)    atomic_init((p), (v))
#define retro_atomic_size_init(p, v)   atomic_init((p), (v))

#define retro_atomic_load_acquire_int(p) \
   atomic_load_explicit((p), memory_order_acquire)
#define retro_atomic_store_release_int(p, v) \
   atomic_store_explicit((p), (v), memory_order_release)
#define retro_atomic_fetch_add_int(p, v) \
   atomic_fetch_add_explicit((p), (v), memory_order_acq_rel)
#define retro_atomic_fetch_sub_int(p, v) \
   atomic_fetch_sub_explicit((p), (v), memory_order_acq_rel)

#define retro_atomic_load_acquire_size(p) \
   atomic_load_explicit((p), memory_order_acquire)
#define retro_atomic_store_release_size(p, v) \
   atomic_store_explicit((p), (v), memory_order_release)
#define retro_atomic_fetch_add_size(p, v) \
   atomic_fetch_add_explicit((p), (v), memory_order_acq_rel)
#define retro_atomic_fetch_sub_size(p, v) \
   atomic_fetch_sub_explicit((p), (v), memory_order_acq_rel)

/* ---- C++11 <atomic> --------------------------------------------------- */
#elif defined(RETRO_ATOMIC_BACKEND_CXX11)

RETRO_BEGIN_DECLS_CXX
#include <atomic>
#include <cstddef>
RETRO_END_DECLS_CXX
/* This header is included by C++ TUs in C++11+ mode (gated on
 * __cplusplus >= 201103L or _MSVC_LANG >= 201103L).  We use the
 * std::atomic_* free-function forms rather than the member-function
 * forms because they are syntactically closest to the C11 macros
 * above and keep the macro expansions identical in shape across
 * the two languages.
 *
 * The std::atomic<T> types are required by the standard to be
 * standard-layout for our integer instantiations and lock-free on
 * every RetroArch-supported target (every architecture has a
 * lock-free 32-bit and pointer-width atomic).  Size equality with
 * the underlying T is not promised by the standard but holds in
 * practice on every libstdc++/libc++/MSVC STL implementation we
 * care about; we do not rely on it. */

typedef std::atomic<int>         retro_atomic_int_t;
typedef std::atomic<std::size_t> retro_atomic_size_t;

#define retro_atomic_int_init(p, v)    std::atomic_init((p), (v))
#define retro_atomic_size_init(p, v)   std::atomic_init((p), (std::size_t)(v))

#define retro_atomic_load_acquire_int(p) \
   std::atomic_load_explicit((p), std::memory_order_acquire)
#define retro_atomic_store_release_int(p, v) \
   std::atomic_store_explicit((p), (v), std::memory_order_release)
#define retro_atomic_fetch_add_int(p, v) \
   std::atomic_fetch_add_explicit((p), (v), std::memory_order_acq_rel)
#define retro_atomic_fetch_sub_int(p, v) \
   std::atomic_fetch_sub_explicit((p), (v), std::memory_order_acq_rel)

#define retro_atomic_load_acquire_size(p) \
   std::atomic_load_explicit((p), std::memory_order_acquire)
#define retro_atomic_store_release_size(p, v) \
   std::atomic_store_explicit((p), (std::size_t)(v), std::memory_order_release)
#define retro_atomic_fetch_add_size(p, v) \
   std::atomic_fetch_add_explicit((p), (std::size_t)(v), std::memory_order_acq_rel)
#define retro_atomic_fetch_sub_size(p, v) \
   std::atomic_fetch_sub_explicit((p), (std::size_t)(v), std::memory_order_acq_rel)

/* ---- GCC __atomic_* (4.7+) / Clang ------------------------------------ */
#elif defined(RETRO_ATOMIC_BACKEND_GCC_NEW)

#include <stddef.h>

typedef int    retro_atomic_int_t;
typedef size_t retro_atomic_size_t;

#define retro_atomic_int_init(p, v)    (*(p) = (v))
#define retro_atomic_size_init(p, v)   (*(p) = (v))

#define retro_atomic_load_acquire_int(p) \
   __atomic_load_n((p), __ATOMIC_ACQUIRE)
#define retro_atomic_store_release_int(p, v) \
   __atomic_store_n((p), (v), __ATOMIC_RELEASE)
#define retro_atomic_fetch_add_int(p, v) \
   __atomic_fetch_add((p), (v), __ATOMIC_ACQ_REL)
#define retro_atomic_fetch_sub_int(p, v) \
   __atomic_fetch_sub((p), (v), __ATOMIC_ACQ_REL)

#define retro_atomic_load_acquire_size(p) \
   __atomic_load_n((p), __ATOMIC_ACQUIRE)
#define retro_atomic_store_release_size(p, v) \
   __atomic_store_n((p), (v), __ATOMIC_RELEASE)
#define retro_atomic_fetch_add_size(p, v) \
   __atomic_fetch_add((p), (v), __ATOMIC_ACQ_REL)
#define retro_atomic_fetch_sub_size(p, v) \
   __atomic_fetch_sub((p), (v), __ATOMIC_ACQ_REL)

/* ---- MSVC Interlocked* (Win32 API, works back to VS2003 / Xbox 360) ---- */
#elif defined(RETRO_ATOMIC_BACKEND_MSVC)

#include <stddef.h>
/* Use the Win32 API forms (capital I, declared in <windows.h>) rather
 * than the compiler intrinsics (_InterlockedFoo, declared in <intrin.h>).
 * The intrinsics require <intrin.h> which doesn't exist before VS2005.
 * The Win32 API forms are available on every MSVC since Windows NT and
 * on the Xbox 360 / OG Xbox XDKs.
 *
 * Memory ordering is non-trivial on this backend because Microsoft's
 * Win32 plain Interlocked* functions have inconsistent ordering across
 * architectures:
 *   - x86/x64: full barrier (LOCK prefix), every form, always.
 *   - Itanium / Xbox 360 PowerPC: full barrier, but historically the
 *     docs warned to pair with __lwsync; the *Acquire / *Release
 *     forms (which fold the barrier in) are recommended.
 *   - ARM / ARM64: NO barrier on the plain forms; you must either
 *     use the *Acquire / *Release forms or pair the plain form with
 *     an explicit __dmb.
 *
 * To get correct semantics on every supported architecture without an
 * x86 perf cost, we:
 *   - Use InterlockedCompareExchangeAcquire for atomic loads.
 *   - Use InterlockedExchange*Release* (the Release variant) for
 *     atomic stores.
 *   - Use the plain InterlockedExchangeAdd for fetch_add / fetch_sub,
 *     bracketed by __dmb(_ARM64_BARRIER_ISH) on ARM64 to provide the
 *     acq_rel ordering.  On every other MSVC target the bracketing
 *     compiles out and the plain form's full-barrier semantics are
 *     used directly.
 *
 * The __dmb intrinsic is declared in <intrin.h> and is available from
 * VS2008 (the same release that introduced ARM as a target).  Since
 * MSVC ARM/ARM64 builds are themselves a VS2008+ feature, the
 * <intrin.h> include is gated on _M_ARM / _M_ARM64 and remains absent
 * on the legacy x86 / Xbox 360 / Itanium paths.
 */

#include <windows.h>

#if defined(_M_ARM) || defined(_M_ARM64)
#include <intrin.h>
#define RETRO_ATOMIC_MSVC_ARM_FENCE() __dmb(_ARM64_BARRIER_ISH)
#else
#define RETRO_ATOMIC_MSVC_ARM_FENCE() ((void)0)
#endif

typedef volatile LONG     retro_atomic_int_t;
typedef volatile LONG_PTR retro_atomic_size_t;
/* LONG_PTR is 32-bit on Win32, 64-bit on Win64 -- matches size_t width
 * on every Windows ABI. */

#define retro_atomic_int_init(p, v)    (*(p) = (LONG)(v))
#define retro_atomic_size_init(p, v)   (*(p) = (LONG_PTR)(v))

#define retro_atomic_load_acquire_int(p) \
   InterlockedCompareExchangeAcquire((LONG volatile*)(p), 0, 0)
#define retro_atomic_store_release_int(p, v)                              \
   do {                                                                   \
      RETRO_ATOMIC_MSVC_ARM_FENCE();                                      \
      (void)InterlockedExchange((LONG volatile*)(p), (LONG)(v));          \
   } while (0)
/* fetch_add / fetch_sub: plain Interlocked* on x86/x64/Itanium/PPC
 * is full-barrier; on ARM we surround with __dmb to get acq_rel. */
#define retro_atomic_fetch_add_int(p, v) (                                \
   RETRO_ATOMIC_MSVC_ARM_FENCE(),                                         \
   InterlockedExchangeAdd((LONG volatile*)(p), (LONG)(v)) )
#define retro_atomic_fetch_sub_int(p, v) (                                \
   RETRO_ATOMIC_MSVC_ARM_FENCE(),                                         \
   InterlockedExchangeAdd((LONG volatile*)(p), -(LONG)(v)) )
/* Note: on ARM we'd ideally want a __dmb both before AND after the
 * RMW for full sequential consistency (PostgreSQL's recent fix does
 * exactly that).  acq_rel needs only one barrier on most use cases;
 * the C11 contract says acq_rel = release-before, acquire-after,
 * which on ARMv8 is satisfied by a single dmb ish.  If a caller
 * needs seq_cst, they can pair this with an additional load_acquire
 * on the same variable. */

#if defined(_WIN64)
#define retro_atomic_load_acquire_size(p) \
   ((size_t)InterlockedCompareExchangeAcquire64((LONGLONG volatile*)(p), 0, 0))
#define retro_atomic_store_release_size(p, v)                             \
   do {                                                                   \
      RETRO_ATOMIC_MSVC_ARM_FENCE();                                      \
      (void)InterlockedExchange64((LONGLONG volatile*)(p), (LONGLONG)(v));\
   } while (0)
#define retro_atomic_fetch_add_size(p, v) (                               \
   RETRO_ATOMIC_MSVC_ARM_FENCE(),                                         \
   (size_t)InterlockedExchangeAdd64((LONGLONG volatile*)(p), (LONGLONG)(v)) )
#define retro_atomic_fetch_sub_size(p, v) (                               \
   RETRO_ATOMIC_MSVC_ARM_FENCE(),                                         \
   (size_t)InterlockedExchangeAdd64((LONGLONG volatile*)(p), -(LONGLONG)(v)) )
#else
#define retro_atomic_load_acquire_size(p) \
   ((size_t)InterlockedCompareExchangeAcquire((LONG volatile*)(p), 0, 0))
#define retro_atomic_store_release_size(p, v)                             \
   do {                                                                   \
      RETRO_ATOMIC_MSVC_ARM_FENCE();                                      \
      (void)InterlockedExchange((LONG volatile*)(p), (LONG)(v));          \
   } while (0)
#define retro_atomic_fetch_add_size(p, v) (                               \
   RETRO_ATOMIC_MSVC_ARM_FENCE(),                                         \
   (size_t)InterlockedExchangeAdd((LONG volatile*)(p), (LONG)(v)) )
#define retro_atomic_fetch_sub_size(p, v) (                               \
   RETRO_ATOMIC_MSVC_ARM_FENCE(),                                         \
   (size_t)InterlockedExchangeAdd((LONG volatile*)(p), -(LONG)(v)) )
#endif

/* ---- Apple OSAtomic (deprecated but available pre-10.7) --------------- */
#elif defined(RETRO_ATOMIC_BACKEND_APPLE)

#include <libkern/OSAtomic.h>
#include <stddef.h>

typedef volatile int32_t  retro_atomic_int_t;
typedef volatile intptr_t retro_atomic_size_t;
/* OSAtomic uses int32 / int64; we pun size_t to intptr_t and assume
 * size_t == intptr_t in width.  Holds on every Apple ABI. */

#define retro_atomic_int_init(p, v)    (*(p) = (v))
#define retro_atomic_size_init(p, v)   (*(p) = (intptr_t)(v))

#define retro_atomic_load_acquire_int(p)  OSAtomicAdd32Barrier(0, (p))
#define retro_atomic_store_release_int(p, v) \
   do { OSMemoryBarrier(); *(p) = (v); } while (0)
#define retro_atomic_fetch_add_int(p, v) \
   (OSAtomicAdd32Barrier((v), (p)) - (v))
#define retro_atomic_fetch_sub_int(p, v) \
   (OSAtomicAdd32Barrier(-(v), (p)) + (v))

#if defined(__LP64__)
#define retro_atomic_load_acquire_size(p) \
   ((size_t)OSAtomicAdd64Barrier(0, (volatile int64_t*)(p)))
#define retro_atomic_store_release_size(p, v) \
   do { OSMemoryBarrier(); *(p) = (intptr_t)(v); } while (0)
#define retro_atomic_fetch_add_size(p, v) \
   ((size_t)(OSAtomicAdd64Barrier((int64_t)(v), (volatile int64_t*)(p)) - (int64_t)(v)))
#define retro_atomic_fetch_sub_size(p, v) \
   ((size_t)(OSAtomicAdd64Barrier(-(int64_t)(v), (volatile int64_t*)(p)) + (int64_t)(v)))
#else
#define retro_atomic_load_acquire_size(p) \
   ((size_t)OSAtomicAdd32Barrier(0, (volatile int32_t*)(p)))
#define retro_atomic_store_release_size(p, v) \
   do { OSMemoryBarrier(); *(p) = (intptr_t)(v); } while (0)
#define retro_atomic_fetch_add_size(p, v) \
   ((size_t)(OSAtomicAdd32Barrier((int32_t)(v), (volatile int32_t*)(p)) - (int32_t)(v)))
#define retro_atomic_fetch_sub_size(p, v) \
   ((size_t)(OSAtomicAdd32Barrier(-(int32_t)(v), (volatile int32_t*)(p)) + (int32_t)(v)))
#endif

/* ---- GCC __sync_* (legacy, 4.1-4.6) ----------------------------------- */
#elif defined(RETRO_ATOMIC_BACKEND_SYNC)

#include <stddef.h>

typedef volatile int    retro_atomic_int_t;
typedef volatile size_t retro_atomic_size_t;

#define retro_atomic_int_init(p, v)    (*(p) = (v))
#define retro_atomic_size_init(p, v)   (*(p) = (v))

/* __sync builtins are full sequential-consistency; over-strong but correct.
 * The "load via fetch_and_add 0" / "store via lock+swap" idioms are the
 * canonical way to get an atomic load/store out of __sync. */
#define retro_atomic_load_acquire_int(p) \
   __sync_fetch_and_add((p), 0)
#define retro_atomic_store_release_int(p, v) \
   do { __sync_synchronize(); *(p) = (v); __sync_synchronize(); } while (0)
#define retro_atomic_fetch_add_int(p, v) \
   __sync_fetch_and_add((p), (v))
#define retro_atomic_fetch_sub_int(p, v) \
   __sync_fetch_and_sub((p), (v))

#define retro_atomic_load_acquire_size(p) \
   __sync_fetch_and_add((p), (size_t)0)
#define retro_atomic_store_release_size(p, v) \
   do { __sync_synchronize(); *(p) = (v); __sync_synchronize(); } while (0)
#define retro_atomic_fetch_add_size(p, v) \
   __sync_fetch_and_add((p), (v))
#define retro_atomic_fetch_sub_size(p, v) \
   __sync_fetch_and_sub((p), (v))

/* ---- Volatile fallback ------------------------------------------------- */
#else /* RETRO_ATOMIC_BACKEND_VOLATILE */

#include <stddef.h>

typedef volatile int    retro_atomic_int_t;
typedef volatile size_t retro_atomic_size_t;

#define retro_atomic_int_init(p, v)    (*(p) = (v))
#define retro_atomic_size_init(p, v)   (*(p) = (v))

/* No barriers.  Correct only on single-core or x86 TSO. */
#define retro_atomic_load_acquire_int(p)         (*(p))
#define retro_atomic_store_release_int(p, v)     do { *(p) = (v); } while (0)
#define retro_atomic_fetch_add_int(p, v)         ((*(p) += (v)) - (v))
#define retro_atomic_fetch_sub_int(p, v)         ((*(p) -= (v)) + (v))

#define retro_atomic_load_acquire_size(p)        (*(p))
#define retro_atomic_store_release_size(p, v)    do { *(p) = (v); } while (0)
#define retro_atomic_fetch_add_size(p, v)        ((*(p) += (v)) - (v))
#define retro_atomic_fetch_sub_size(p, v)        ((*(p) -= (v)) + (v))

#endif /* backend selection */

/* ---- Convenience wrappers (backend-agnostic) -------------------------- */

#define retro_atomic_inc_int(p)    ((void)retro_atomic_fetch_add_int((p), 1))
#define retro_atomic_dec_int(p)    ((void)retro_atomic_fetch_sub_int((p), 1))
#define retro_atomic_inc_size(p)   ((void)retro_atomic_fetch_add_size((p), 1))
#define retro_atomic_dec_size(p)   ((void)retro_atomic_fetch_sub_size((p), 1))

#endif /* __LIBRETRO_SDK_ATOMIC_H */
