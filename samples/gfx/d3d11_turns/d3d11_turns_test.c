/* libretro_d3d11.h version 2's turns on the immediate context, as
 * gfx/common/d3d11_turns.h takes them: a core thread locks and unlocks
 * as fast as it can, the way a core does around each piece of work,
 * while a frontend thread asks for the context again and again.
 *
 * Held to: every time the frontend asks, it gets in within a few of the
 * core's turns, and the core never spins for it.
 *
 * D3D11_TURNS_TEST_UNFAIR builds a core that does not give way, and the
 * test must fail: the frontend waits behind many of the core's turns.
 *
 * Windows only; under Wine elsewhere. */
#include <stdio.h>
#include "../../../gfx/common/d3d11_turns.h"

#define ASKS        200
#define MOST_TURNS  4

static d3d11_turns_t      turns;
static retro_atomic_int_t core_turns, quit;
static volatile LONG      frontend_asking;

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
   unsigned ask, worst = 0, over = 0;
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
      int before = retro_atomic_load_acquire_int(&core_turns);
      unsigned took;
      d3d11_turns_frontend_enter(&turns);
      took = (unsigned)(retro_atomic_load_acquire_int(&core_turns) - before);
      d3d11_turns_frontend_leave(&turns);
      if (took > worst)
         worst = took;
      if (took > MOST_TURNS)
         over++;
      /* let the core run between asks */
      before = retro_atomic_load_acquire_int(&core_turns);
      while (retro_atomic_load_acquire_int(&core_turns) < before + 8)
         Sleep(0);
   }
   retro_atomic_store_release_int(&quit, 1);
   WaitForSingleObject(thread, INFINITE);
   CloseHandle(thread);
   d3d11_turns_deinit(&turns);
   printf("frontend asked %u times: at most %u core turns ahead of it, "
         "%u times more than %u\n", ASKS, worst, over, MOST_TURNS);
   if (over)
   {
      puts("FAIL");
      return 1;
   }
   puts("PASS: d3d11 turns");
   return 0;
}
