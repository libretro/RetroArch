/* libretro_d3d11.h version 2's turns on the immediate context, as
 * gfx/common/d3d11_turns.h takes them: a core thread locks and unlocks
 * as fast as it can, the way a core does around each piece of work,
 * while a frontend thread asks for the context again and again.
 *
 * Held to: every time the frontend asks, it gets in within a few of the
 * core's turns, and the core never spins for it.
 *
 * And, first, without timing: a frontend that got in while a core was
 * about to park and nobody was asleep leaves nothing behind, so the next
 * core to give way still parks until the frontend it saw is in, and
 * that frontend gets the context before it.
 *
 * D3D11_TURNS_TEST_UNFAIR builds a core that does not give way, and the
 * test must fail: the frontend waits behind many of the core's turns.
 *
 * Only an ask made while the core is running on another CPU tells the
 * two apart: on a host that has the core off the CPU just then, either
 * core lets the frontend straight in. So an ask is counted only when
 * the core was seen taking turns just before it, and a run with too
 * few of those says so (exit 2) rather than passing.
 *
 * Windows only; under Wine elsewhere. */
#include <stdio.h>
#include "../../../gfx/common/d3d11_turns.h"

#define ASKS        200
#define MOST_TURNS  4
/* asks made beside a running core, fewer of which tell nothing */
#define MIN_COUNTED 50

static d3d11_turns_t      turns;
static retro_atomic_int_t core_turns, quit;
static volatile LONG      frontend_asking;

#ifndef D3D11_TURNS_TEST_UNFAIR
static d3d11_turns_t      st;
static retro_atomic_int_t st_order, st_frontend_at, st_core_at;

static DWORD WINAPI st_frontend(void *arg)
{
   (void)arg;
   d3d11_turns_frontend_enter(&st);
   retro_atomic_store_release_int(&st_frontend_at,
         retro_atomic_fetch_add_int(&st_order, 1) + 1);
   d3d11_turns_frontend_leave(&st);
   return 0;
}

static DWORD WINAPI st_core(void *arg)
{
   (void)arg;
   d3d11_turns_core_enter(&st);
   retro_atomic_store_release_int(&st_core_at,
         retro_atomic_fetch_add_int(&st_order, 1) + 1);
   d3d11_turns_core_leave(&st);
   return 0;
}

static bool st_join(HANDLE h)
{
   bool ok = WaitForSingleObject(h, 5000) == WAIT_OBJECT_0;
   CloseHandle(h);
   return ok;
}

/* The order is fixed by holding the critical section here, as a third
 * party: the frontend waits on it, so a core that gives way must park
 * rather than queue on it beside the frontend. */
static bool stale_check(void)
{
   HANDLE fe, core;
   DWORD  began;
   int    parked;
   bool   ok;

   d3d11_turns_init(&st);
   retro_atomic_int_init(&st_order, 0);
   retro_atomic_int_init(&st_frontend_at, 0);
   retro_atomic_int_init(&st_core_at, 0);

   /* A core about to park, and a frontend that gets in and leaves
    * before it does: nobody is asleep for that frontend's wake. */
   retro_atomic_fetch_add_seq_cst_int(&st.core_parked, 1);
   d3d11_turns_frontend_enter(&st);
   d3d11_turns_frontend_leave(&st);
   retro_atomic_fetch_add_seq_cst_int(&st.core_parked, -1);

   EnterCriticalSection(&st.lock);
   if (!(fe = CreateThread(NULL, 0, st_frontend, NULL, 0, NULL)))
   {
      LeaveCriticalSection(&st.lock);
      puts("FAIL: no thread");
      return false;
   }
   began = GetTickCount();
   while (!(retro_atomic_load_acquire_int(&st.frontend) & D3D11_TURNS_WAITING)
         && GetTickCount() - began < 5000)
      Sleep(1);
   if (!(core = CreateThread(NULL, 0, st_core, NULL, 0, NULL)))
   {
      LeaveCriticalSection(&st.lock);
      st_join(fe);
      puts("FAIL: no thread");
      return false;
   }
   Sleep(100);
   parked = retro_atomic_load_seq_cst_int(&st.core_parked);
   LeaveCriticalSection(&st.lock);
   ok = st_join(fe);
   ok = st_join(core) && ok;
   d3d11_turns_deinit(&st);

   printf("left-over wake: core %s while the frontend waited, "
         "frontend in %s\n", parked ? "parked" : "did not park",
         retro_atomic_load_acquire_int(&st_frontend_at)
         < retro_atomic_load_acquire_int(&st_core_at)
         ? "first" : "second");
   if (!ok || !parked
         || retro_atomic_load_acquire_int(&st_frontend_at)
         >= retro_atomic_load_acquire_int(&st_core_at))
   {
      puts("FAIL");
      return false;
   }
   return true;
}
#endif

static DWORD WINAPI core_thread(void *arg)
{
   (void)arg;
   while (!retro_atomic_load_acquire_int(&quit))
   {
      volatile unsigned i;
      d3d11_turns_core_enter(&turns);
      /* nested, as a core's helpers lock again */
      d3d11_turns_core_enter(&turns);
      for (i = 0; i < 200; i++) { }
      d3d11_turns_core_leave(&turns);
      d3d11_turns_core_leave(&turns);
      retro_atomic_fetch_add_int(&core_turns, 1);
   }
   return 0;
}

int main(void)
{
   HANDLE thread;
   unsigned ask, worst = 0, over = 0, counted = 0;
#ifndef D3D11_TURNS_TEST_UNFAIR
   if (!stale_check())
      return 1;
#endif
   d3d11_turns_init(&turns);
   retro_atomic_int_init(&core_turns, 0);
   retro_atomic_int_init(&quit, 0);
   if (!(thread = CreateThread(NULL, 0, core_thread, NULL, 0, NULL)))
   {
      puts("FAIL: no thread");
      return 1;
   }
   while (retro_atomic_load_acquire_int(&core_turns) < 1000)
      Sleep(0);
   for (ask = 0; ask < ASKS; ask++)
   {
      int before;
      unsigned took = 0, tries;
      bool running = false;
      /* An ask that finds the core far ahead is asked again: the frontend
       * thread taken off its CPU between reading the count and asking
       * lets the core run on, which a fair core cannot prevent, but not
       * twice running. A core that does not give way is far ahead on the
       * second ask too. */
      for (tries = 0; tries < 2; tries++)
      {
         /* The core taking turns right now, beside this thread: watched
          * without sleeping, for at most 20 ms */
         {
            DWORD began = GetTickCount();
            int seen    = retro_atomic_load_acquire_int(&core_turns);
            running     = false;
            while (GetTickCount() - began < 20)
               if (retro_atomic_load_acquire_int(&core_turns) >= seen + 2)
               {
                  running = true;
                  break;
               }
         }
         before = retro_atomic_load_acquire_int(&core_turns);
         d3d11_turns_frontend_enter(&turns);
         took = (unsigned)(retro_atomic_load_acquire_int(&core_turns) - before);
         d3d11_turns_frontend_leave(&turns);
         if (!running || took <= MOST_TURNS)
            break;
      }
      if (running)
      {
         counted++;
         if (took > worst)
            worst = took;
         if (took > MOST_TURNS)
            over++;
      }
      /* let the core run between asks */
      before = retro_atomic_load_acquire_int(&core_turns);
      while (retro_atomic_load_acquire_int(&core_turns) < before + 8)
         Sleep(0);
   }
   retro_atomic_store_release_int(&quit, 1);
   WaitForSingleObject(thread, INFINITE);
   CloseHandle(thread);
   d3d11_turns_deinit(&turns);
   printf("frontend asked %u times, %u beside a running core: at most %u "
         "core turns ahead of it, %u times more than %u\n",
         ASKS, counted, worst, over, MOST_TURNS);
   if (over)
   {
      puts("FAIL");
      return 1;
   }
   if (counted < MIN_COUNTED)
   {
      puts("INCONCLUSIVE: the core was rarely running beside the frontend");
      return 2;
   }
   puts("PASS: d3d11 turns");
   return 0;
}
