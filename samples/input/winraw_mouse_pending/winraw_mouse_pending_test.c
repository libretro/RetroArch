#include <stdio.h>
#include <retro_atomic.h>

static retro_atomic_int_t cursor_pending, absolute_pending;
static unsigned exchanges, inject_after;
static void after_drain(void)
{
   exchanges++;
   if (inject_after == exchanges)
      retro_atomic_store_release_int(&absolute_pending, 1);
}
static int test_exchange(retro_atomic_int_t *value, int replacement)
{
   int result = retro_atomic_exchange_int(value, replacement);
   after_drain();
   return result;
}
static void test_clear(retro_atomic_int_t *value, int replacement)
{
   retro_atomic_store_relaxed_int(value, replacement);
   after_drain();
}
#undef retro_atomic_exchange_int
#define retro_atomic_exchange_int test_exchange
#undef retro_atomic_store_relaxed_int
#define retro_atomic_store_relaxed_int test_clear
#include "input/drivers/winraw_mouse_pending.h"

static unsigned failures;
#define CHECK(c, msg) do { if (!(c)) { \
   fprintf(stderr, "FAIL %s\n", msg); failures++; } } while (0)

static void setup(int cursor, int absolute, unsigned inject)
{
   retro_atomic_store_release_int(&cursor_pending, cursor);
   retro_atomic_store_release_int(&absolute_pending, absolute);
   exchanges = 0;
   inject_after = inject;
}
static enum winraw_mouse_position_source take(void)
{ return winraw_mouse_take_position(&cursor_pending, &absolute_pending); }

int main(void)
{
   setup(1, 1, 0);
   CHECK(take() == WINRAW_MOUSE_CURSOR, "cursor priority lost");
   CHECK(retro_atomic_load_acquire_int(&absolute_pending) == 0,
         "superseded absolute report remains pending");
   CHECK(take() == WINRAW_MOUSE_DELTA, "idle poll replays stale absolute report");
   setup(0, 1, 0);
   CHECK(take() == WINRAW_MOUSE_ABSOLUTE, "absolute report lost");
   CHECK(take() == WINRAW_MOUSE_DELTA, "absolute report repeated");
   setup(1, 0, 0);
   CHECK(take() == WINRAW_MOUSE_CURSOR, "cursor report lost");
   CHECK(take() == WINRAW_MOUSE_DELTA, "cursor request repeated");
   setup(0, 0, 0);
   CHECK(take() == WINRAW_MOUSE_DELTA, "idle mouse changed source");
   setup(1, 0, 1);
   CHECK(take() == WINRAW_MOUSE_CURSOR, "cursor priority changed during drain");
   inject_after = 0;
   CHECK(take() == WINRAW_MOUSE_DELTA, "superseded during-drain report replayed");
   setup(1, 0, 2);
   CHECK(take() == WINRAW_MOUSE_CURSOR, "late report replaced cursor source");
   inject_after = 0;
   CHECK(take() == WINRAW_MOUSE_ABSOLUTE, "report after drain was lost");
   CHECK(take() == WINRAW_MOUSE_DELTA, "late report repeated");
   puts(failures ? "FAIL winraw_mouse_pending_test" : "PASS winraw_mouse_pending_test");
   return failures ? 1 : 0;
}
