/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_atomic_cxx11_old_stl_test.cpp).
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

/* Every value-taking macro of retro_atomic.h's C++11 backend, given
 * an enumerator or an unsigned where a core would write one.  Built
 * twice by the Makefile: against the host <atomic>, and run; and with
 * -fsyntax-only against old_stl/atomic, the libstdc++ 8 shape whose
 * free functions deduce the value parameter too.  A macro that hands
 * the argument through uncast fails the second build. */

#include <cstdio>
#include <cstddef>
#include <retro_atomic.h>

#if !defined(RETRO_ATOMIC_BACKEND_CXX11)
#error "this test is for the C++11 backend"
#endif

enum baton_state
{
   BATON_NONE = 0,
   BATON_HELD,
   BATON_FRAME
};

struct baton
{
   enum { IDLE = 0, BUSY = 3 };
   retro_atomic_int_t  state;
   retro_atomic_int_t  flags;
   retro_atomic_size_t count;
   retro_atomic_ptr_t  work;
#ifdef RETRO_ATOMIC_HAS_64
   retro_atomic_64_t   stamp;
#endif

   baton()
   {
      retro_atomic_int_init(&state, BATON_NONE);
      retro_atomic_int_init(&flags, IDLE);
      retro_atomic_size_init(&count, BUSY);
      retro_atomic_ptr_init(&work, NULL);
#ifdef RETRO_ATOMIC_HAS_64
      retro_atomic_64_init(&stamp, BUSY);
#endif
   }
};

static int fails = 0;

static void want(const char *what, long got, long exp)
{
   if (got == exp)
      printf("ok    %s\n", what);
   else
   {
      printf("FAIL  %s: got %ld, want %ld\n", what, got, exp);
      fails++;
   }
}

int main(void)
{
   baton b;
   unsigned u = 1u << 4;
   int work   = 0;

   want("int init from enumerator",
         retro_atomic_load_acquire_int(&b.state), BATON_NONE);
   want("int init from member enumerator",
         retro_atomic_load_relaxed_int(&b.flags), baton::IDLE);
   want("size init from enumerator",
         (long)retro_atomic_load_acquire_size(&b.count), baton::BUSY);

   retro_atomic_store_release_int(&b.state, BATON_HELD);
   want("store_release from enumerator",
         retro_atomic_load_acquire_int(&b.state), BATON_HELD);
   retro_atomic_store_relaxed_int(&b.state, BATON_FRAME);
   want("store_relaxed from enumerator",
         retro_atomic_load_relaxed_int(&b.state), BATON_FRAME);
   retro_atomic_store_release_int(&b.state, baton::BUSY);
   want("store_release from member enumerator",
         retro_atomic_load_seq_cst_int(&b.state), baton::BUSY);

   want("fetch_add of unsigned",
         retro_atomic_fetch_add_int(&b.flags, u), baton::IDLE);
   want("fetch_add_seq_cst of enumerator",
         retro_atomic_fetch_add_seq_cst_int(&b.flags, BATON_HELD), (long)u);
   want("fetch_sub of enumerator",
         retro_atomic_fetch_sub_int(&b.flags, BATON_HELD), (long)u + 1);
   want("fetch_or of unsigned",
         retro_atomic_fetch_or_int(&b.flags, 1u), (long)u);
   want("fetch_and of unsigned",
         retro_atomic_fetch_and_int(&b.flags, ~u), (long)u + 1);
   want("flags after or and and",
         retro_atomic_load_acquire_int(&b.flags), 1);

   retro_atomic_store_release_size(&b.count, baton::IDLE);
   want("size store_release from enumerator",
         (long)retro_atomic_load_relaxed_size(&b.count), baton::IDLE);
   want("size fetch_add of enumerator",
         (long)retro_atomic_fetch_add_size(&b.count, BATON_FRAME), baton::IDLE);
   want("size fetch_sub of enumerator",
         (long)retro_atomic_fetch_sub_size(&b.count, BATON_HELD), BATON_FRAME);

   want("exchange from enumerator",
         retro_atomic_exchange_int(&b.state, BATON_NONE), baton::BUSY);
   want("cas from enumerators",
         retro_atomic_cas_int(&b.state, BATON_NONE, BATON_HELD), 1);
   want("state after cas",
         retro_atomic_load_acquire_int(&b.state), BATON_HELD);

   retro_atomic_store_release_ptr(&b.work, &work);
   want("ptr round trip",
         retro_atomic_load_acquire_ptr(&b.work) == &work, 1);

#ifdef RETRO_ATOMIC_HAS_64
   retro_atomic_store_release_64(&b.stamp, BATON_FRAME);
   want("64 store from enumerator",
         (long)retro_atomic_load_acquire_64(&b.stamp), BATON_FRAME);
   want("64 exchange from enumerator",
         (long)retro_atomic_exchange_64(&b.stamp, BATON_NONE), BATON_FRAME);
#endif

   printf("%s: %s\n", RETRO_ATOMIC_BACKEND_NAME, fails ? "FAIL" : "all ok");
   return fails ? 1 : 0;
}
