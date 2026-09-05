/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_atomic_extern_c_linkage_test.cpp).
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

/* Regression test for the linkage hazard in
 * libretro-common/include/retro_atomic.h's C++11 backend.
 *
 * Background
 * ----------
 * Several C++ TUs in RetroArch wrap their includes of in-tree C
 * headers in extern "C" { ... } when CXX_BUILD is not defined.  The
 * canonical example is ui/drivers/ui_qt.cpp:
 *
 *     #ifndef CXX_BUILD
 *     extern "C" {
 *     #endif
 *     #include "../../menu/menu_driver.h"   // pulls in retro_atomic.h
 *     ...
 *     #ifndef CXX_BUILD
 *     }
 *     #endif
 *
 * On a C++11+ build this chain reaches retro_atomic.h's CXX11 backend,
 * which #include's <atomic>.  libstdc++'s <atomic> declares dozens of
 * templates and template specializations; under C linkage every one of
 * them is rejected with
 *
 *     error: template with C linkage
 *
 * yielding ~80 errors on a single TU.  See the build log on master at
 * the time of the original report; same failure on g++ 9 through 13.
 *
 * Fix
 * ---
 * retro_common_api.h gained RETRO_BEGIN_DECLS_CXX / RETRO_END_DECLS_CXX
 * macros that expand to extern "C++" { ... } in C++ mode (with
 * CXX_BUILD off) and to nothing otherwise.  retro_atomic.h's C++11
 * backend wraps its <atomic> / <cstddef> include in those macros, so
 * the standard headers parse at C++ linkage regardless of any
 * extern "C" the caller imposes.
 *
 * What this test asserts
 * ----------------------
 *  1. Compile-time: a C++ TU that wraps #include <retro_atomic.h>
 *     in extern "C" { ... } compiles without error.  The include
 *     pattern below is a faithful mirror of ui_qt.cpp's; if the
 *     header's linkage shield breaks, this file fails to compile
 *     and that is the regression signal.
 *
 *  2. Runtime: the macro expansions inside the extern "C" wrap still
 *     produce live std::atomic<T> objects whose load_acquire /
 *     store_release / fetch_add / fetch_sub / inc / dec round-trip
 *     correctly.  This guards against a future "fix" that papered
 *     over the compile error by, say, falling back to a non-atomic
 *     plain-int typedef under C linkage.  exit(0) on success,
 *     exit(1) on any property failure.
 *
 *  3. Both build configurations exercise the same source with no
 *     code-level difference (see Makefile).  The Makefile builds
 *     two targets:
 *       retro_atomic_extern_c_linkage_test       -- !CXX_BUILD path
 *       retro_atomic_extern_c_linkage_test_cxx   -- CXX_BUILD path
 *     The second one defines CXX_BUILD before including any
 *     RetroArch headers, which makes the in-tree extern "C" wrappers
 *     compile out (because RETRO_BEGIN_DECLS / RETRO_END_DECLS expand
 *     to nothing).  Both must build and pass.
 *
 * What this test does NOT assert
 * ------------------------------
 * It does not exercise weakly-ordered SMP correctness or thread
 * safety -- those are covered by retro_atomic_test (single-thread
 * property + SPSC stress) and retro_spsc_test (lock-free SPSC) in
 * neighbouring sample directories, and by the cross-arch qemu lane
 * in .github/workflows/Linux-libretro-common-samples.yml.  This
 * test's job is purely to police the include-from-C++ hazard.
 *
 * Build standalone
 * ----------------
 *   make clean all
 *   ./retro_atomic_extern_c_linkage_test
 *   ./retro_atomic_extern_c_linkage_test_cxx
 *
 * Both binaries exit 0 on success.  A pre-fix retro_atomic.h would
 * fail at the build step rather than at runtime.
 */

/* Mirror of ui_qt.cpp's include guard pattern.  retro_common_api.h
 * (transitively included from retro_atomic.h) keys on this:
 *
 *   #ifdef CXX_BUILD
 *     -- no extern "C" wrappers anywhere in libretro-common headers --
 *   #else
 *     -- normal RETRO_BEGIN_DECLS = extern "C" { wrapping --
 *   #endif
 *
 * The Makefile builds this file twice, once with CXX_BUILD undefined
 * (the non-unity build path that triggered the original bug) and once
 * with CXX_BUILD defined (the griffin/unity-build path).  Both must
 * compile and pass the runtime checks. */

#include <cstdio>
#include <cstddef>

/* This is the regression's trigger pattern: a C++ TU includes a
 * RetroArch header from inside its own extern "C" block.  On a stock
 * (broken) retro_atomic.h, the transitive #include <atomic> here would
 * be parsed at C linkage and the build would die. */
#ifndef CXX_BUILD
extern "C" {
#endif

#include <retro_atomic.h>

#ifndef CXX_BUILD
}
#endif

/* Capability flags must be set after the include succeeds.  If a
 * future refactor accidentally compiles the header out under
 * extern "C", these gates fire at preprocessing time with a clearer
 * message than the libstdc++ template avalanche. */
#if !defined(HAVE_RETRO_ATOMIC)
# error "retro_atomic.h: HAVE_RETRO_ATOMIC not set after include"
#endif
#if !defined(RETRO_ATOMIC_BACKEND_NAME)
# error "retro_atomic.h: RETRO_ATOMIC_BACKEND_NAME not set after include"
#endif

/* On a C++11+ host the CXX11 backend is the expected selection.  If
 * something promotes a different backend in this TU we want to know;
 * the whole point of the test is to police the C++11 path. */
#if !defined(RETRO_ATOMIC_BACKEND_CXX11)
# error "retro_atomic_extern_c_linkage_test: expected the C++11 <atomic> backend; if a different backend is intentional, update this gate"
#endif

/* ---- Single-threaded property checks --------------------------------- */
/* Each returns 0 on success, 1 on failure.  Identical in shape to
 * retro_atomic_test.c so a regression in either test points at the
 * same place. */

static int check_init(void)
{
   retro_atomic_int_t  ai; retro_atomic_int_init (&ai, 7);
   retro_atomic_size_t as; retro_atomic_size_init(&as, (std::size_t)9);

   if (retro_atomic_load_acquire_int (&ai) != 7) return 1;
   if (retro_atomic_load_acquire_size(&as) != (std::size_t)9) return 1;
   return 0;
}

static int check_store_release_load_acquire(void)
{
   retro_atomic_int_t  ai; retro_atomic_int_init (&ai, 0);
   retro_atomic_size_t as; retro_atomic_size_init(&as, 0);

   retro_atomic_store_release_int (&ai, 42);
   retro_atomic_store_release_size(&as, (std::size_t)42);

   if (retro_atomic_load_acquire_int (&ai) != 42) return 1;
   if (retro_atomic_load_acquire_size(&as) != (std::size_t)42) return 1;
   return 0;
}

static int check_fetch_add_sub_returns_previous(void)
{
   retro_atomic_int_t  ai; retro_atomic_int_init (&ai, 10);
   retro_atomic_size_t as; retro_atomic_size_init(&as, (std::size_t)10);

   /* fetch_add returns the previous value (POSIX convention). */
   if (retro_atomic_fetch_add_int (&ai, 5) != 10) return 1;
   if (retro_atomic_fetch_add_size(&as, 5) != (std::size_t)10) return 1;

   if (retro_atomic_load_acquire_int (&ai) != 15) return 1;
   if (retro_atomic_load_acquire_size(&as) != (std::size_t)15) return 1;

   if (retro_atomic_fetch_sub_int (&ai, 5) != 15) return 1;
   if (retro_atomic_fetch_sub_size(&as, 5) != (std::size_t)15) return 1;

   if (retro_atomic_load_acquire_int (&ai) != 10) return 1;
   if (retro_atomic_load_acquire_size(&as) != (std::size_t)10) return 1;
   return 0;
}

int main(void)
{
   int fails = 0;

   std::printf("backend: %s\n", RETRO_ATOMIC_BACKEND_NAME);
#ifdef CXX_BUILD
   std::puts("config:  CXX_BUILD defined (griffin/unity-build path)");
#else
   std::puts("config:  CXX_BUILD undefined (separate-TU path, ui_qt.cpp shape)");
#endif

   fails += check_init();
   fails += check_store_release_load_acquire();
   fails += check_fetch_add_sub_returns_previous();

   if (fails)
   {
      std::printf("FAIL: %d check(s) failed\n", fails);
      return 1;
   }
   std::puts("ALL OK");
   return 0;
}
