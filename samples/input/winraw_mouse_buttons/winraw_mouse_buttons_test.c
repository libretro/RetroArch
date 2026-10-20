#include <stdio.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>

static retro_atomic_int_t buttons, finished;
static unsigned injection, retries, failures;

static bool test_cas(retro_atomic_int_t *value, int old, int now)
{
   if (injection)
   {
      unsigned which = injection;
      injection = 0;
      if (which == 1)
         retro_atomic_exchange_int(value, 0);
      else
         retro_atomic_fetch_or_int(value, 4);
   }
   if (retro_atomic_cas_int(value, old, now))
      return true;
   retries++;
   return false;
}

#define retro_atomic_cas_int test_cas
#include "input/drivers/winraw_mouse_buttons.h"
#undef retro_atomic_cas_int

#define CHECK(c) do { if (!(c)) { \
   fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static void reports(void *data)
{
   unsigned i;
   (void)data;
   for (i = 0; i < 100000; i++)
   {
      winraw_mouse_buttons_update(&buttons, 3, 0);
      winraw_mouse_buttons_update(&buttons, 0, 3);
   }
   retro_atomic_store_release_int(&finished, 1);
}

int main(void)
{
   sthread_t *worker;
   unsigned i = 0, value;
   retro_atomic_int_init(&buttons, 0);
   retro_atomic_int_init(&finished, 0);

   winraw_mouse_buttons_update(&buttons, 1, 0);
   CHECK(winraw_mouse_buttons_snapshot(&buttons, true) == 1);
   winraw_mouse_buttons_update(&buttons, 2, 1);
   CHECK(winraw_mouse_buttons_snapshot(&buttons, true) == 2);
   winraw_mouse_buttons_update(&buttons, 0, 0);
   CHECK(winraw_mouse_buttons_snapshot(&buttons, true) == 2);
   CHECK(winraw_mouse_buttons_snapshot(&buttons, false) == 0);
   CHECK(winraw_mouse_buttons_snapshot(&buttons, true) == 0);

   /* Clear focus between the report's read and its publication. */
   retro_atomic_store_relaxed_int(&buttons, 1);
   injection = 1;
   retries = 0;
   winraw_mouse_buttons_update(&buttons, 2, 0);
   CHECK(retries == 1);
   CHECK(winraw_mouse_buttons_snapshot(&buttons, true) == 2);

   /* A competing button update must survive the retry as well. */
   injection = 2;
   retries = 0;
   winraw_mouse_buttons_update(&buttons, 1, 2);
   CHECK(retries == 1);
   CHECK(winraw_mouse_buttons_snapshot(&buttons, true) == 5);
   CHECK(winraw_mouse_buttons_snapshot(&buttons, false) == 0);

   /* A click shorter than a frame: pressed and let go before the
    * poll, it reads as down once, then up. */
   winraw_mouse_buttons_update(&buttons, 1, 0);
   winraw_mouse_buttons_update(&buttons, 0, 1);
   CHECK(winraw_mouse_buttons_snapshot(&buttons, true) == 1);
   CHECK(winraw_mouse_buttons_snapshot(&buttons, true) == 0);
   /* ... and one held across the poll reads as down until let go */
   winraw_mouse_buttons_update(&buttons, 2, 0);
   CHECK(winraw_mouse_buttons_snapshot(&buttons, true) == 2);
   CHECK(winraw_mouse_buttons_snapshot(&buttons, true) == 2);
   winraw_mouse_buttons_update(&buttons, 0, 2);
   CHECK(winraw_mouse_buttons_snapshot(&buttons, true) == 0);
   /* focus lost takes the latch with the rest */
   winraw_mouse_buttons_update(&buttons, 1, 0);
   winraw_mouse_buttons_update(&buttons, 0, 1);
   CHECK(winraw_mouse_buttons_snapshot(&buttons, false) == 0);
   CHECK(winraw_mouse_buttons_snapshot(&buttons, true) == 0);

   worker = sthread_create(reports, NULL);
   CHECK(worker != NULL);
   if (worker)
   {
      while (!retro_atomic_load_acquire_int(&finished))
      {
         bool focused = (++i % 7) != 0;
         value = winraw_mouse_buttons_snapshot(&buttons, focused);
         CHECK(value == 0 || (focused && value == 3));
      }
      sthread_join(worker);
   }
   /* the last click may still be latched: it reads once, then not */
   value = winraw_mouse_buttons_snapshot(&buttons, true);
   CHECK(value == 0 || value == 3);
   CHECK(winraw_mouse_buttons_snapshot(&buttons, true) == 0);
   winraw_mouse_buttons_update(&buttons, 16, 0);
   CHECK(winraw_mouse_buttons_snapshot(&buttons, true) == 16);
   puts(failures ? "FAIL winraw_mouse_buttons_test" : "PASS winraw_mouse_buttons_test");
   return failures ? 1 : 0;
}
