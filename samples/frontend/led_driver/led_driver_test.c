/* led_driver_test.c -- led/led_driver.c, between a core's LED call and
 * the driver's write.
 *
 * A core's LED calls are stored, and the frontend flushes them to the
 * driver once the core has run. The claims:
 *
 *   1. Before any driver is chosen, setting, flushing and freeing do
 *      nothing.
 *   2. Choosing a driver runs its init once and nothing else.
 *   3. A core's call writes nothing. A flush writes each LED set since
 *      the last flush once, lowest first, with its last state, even
 *      when that state is the one it already had.
 *   4. A state reaches the driver as 0 or 1.
 *   5. An index outside 0 to MAX_LEDS - 1 is ignored. The store's
 *      slot range bounds it as well; led_driver.c asserts at compile
 *      time that MAX_LEDS fits in it.
 *   6. No name, "null" and an unknown name choose the null driver.
 *      Only a real driver is logged, under its own name.
 *   7. Choosing a driver forgets what was set and not yet flushed.
 *   8. With threads setting LEDs while the main thread flushes, every
 *      LED ends on its thread's last state, and the driver is only
 *      called on the flushing thread.
 *   9. Freeing writes what is pending, then frees the driver.
 *      Nothing reaches it after that.
 *
 * The recorder below stands in for "overlay": led_driver.c is built
 * with HAVE_OVERLAY, so led_driver_init("overlay") chooses it. */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "led/led_driver.h"
#include "led/led_defines.h"

static unsigned failures = 0;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

/* led_driver.c logs one line, naming the driver: its argument, and
 * how many lines. */
static const char *logged;
static unsigned    n_logs;

void RARCH_LOG(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   logged = va_arg(ap, const char*);
   va_end(ap);
   n_logs++;
}

/* The driver: every call it got, in order. */
enum { EV_INIT = 1, EV_FREE, EV_SET };

#define MAX_EVENTS 64

struct event
{
   int kind;
   int led;
   int value;
};

static struct event events[MAX_EVENTS];
static unsigned     n_events;
static unsigned     n_sets;
static int          last_value[MAX_LEDS];
static uintptr_t    flush_thread;
static bool         set_off_thread;

static void record(int kind, int led, int value)
{
   if (n_events < MAX_EVENTS)
   {
      events[n_events].kind  = kind;
      events[n_events].led   = led;
      events[n_events].value = value;
   }
   n_events++;
}

static void rec_init(void) { record(EV_INIT, 0, 0); }
static void rec_free(void) { record(EV_FREE, 0, 0); }

static void rec_set(int led, int value)
{
   if (sthread_get_current_thread_id() != flush_thread)
      set_off_thread = true;
   if (led >= 0 && led < MAX_LEDS)
      last_value[led] = value;
   n_sets++;
   record(EV_SET, led, value);
}

const led_driver_t overlay_led_driver = {
   rec_init,
   rec_free,
   rec_set,
   "recorder"
};

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
/* led_driver.c names it on Windows whatever is configured. */
const led_driver_t keyboard_led_driver = {
   rec_init,
   rec_free,
   rec_set,
   "keyboard"
};
#endif

static void reset(void)
{
   unsigned i;

   memset(events, 0, sizeof(events));
   n_events       = 0;
   n_sets         = 0;
   set_off_thread = false;
   for (i = 0; i < MAX_LEDS; i++)
      last_value[i] = -1;
}

static bool event_is(unsigned i, int kind, int led, int value)
{
   return i < n_events && i < MAX_EVENTS
      && events[i].kind  == kind
      && events[i].led   == led
      && events[i].value == value;
}

/* 1 */
static void test_before_init(void)
{
   reset();
   led_driver_set_led(3, 1);
   led_driver_flush();
   led_driver_free();
   CHECK(n_events == 0, "a call before init reached a driver");
}

/* 2, 7 */
static void test_init(void)
{
   reset();
   led_driver_set_led(4, 1);
   led_driver_init("overlay");
   CHECK(n_events == 1 && event_is(0, EV_INIT, 0, 0),
         "choosing a driver did not run its init alone, once");

   led_driver_flush();
   CHECK(n_sets == 0, "a state set before init reached the driver");
}

/* 3, 4, 5 */
static void test_flush(void)
{
   reset();
   led_driver_set_led(3, 1);
   CHECK(n_sets == 0, "a core's call wrote to the driver");
   led_driver_flush();
   CHECK(n_sets == 1 && event_is(0, EV_SET, 3, 1),
         "a flush did not write the LED that was set");

   reset();
   led_driver_set_led(3, 1);
   led_driver_set_led(3, 0);
   led_driver_flush();
   CHECK(n_sets == 1 && event_is(0, EV_SET, 3, 0),
         "an LED set twice in a frame was not written once, last state");

   reset();
   led_driver_set_led(1, 1);
   led_driver_set_led(2, 1);
   led_driver_set_led(3, 1);
   led_driver_flush();
   CHECK(   n_sets == 3
         && event_is(0, EV_SET, 1, 1)
         && event_is(1, EV_SET, 2, 1)
         && event_is(2, EV_SET, 3, 1),
         "three LEDs set were not one write each");
   reset();
   led_driver_flush();
   CHECK(n_sets == 0, "a flush with nothing set wrote");

   reset();
   led_driver_set_led(5, 1);
   led_driver_set_led(1, 1);
   led_driver_set_led(3, 1);
   led_driver_flush();
   CHECK(   n_sets == 3
         && event_is(0, EV_SET, 1, 1)
         && event_is(1, EV_SET, 3, 1)
         && event_is(2, EV_SET, 5, 1),
         "LEDs were not written lowest first");

   reset();
   led_driver_set_led(7, 5);
   led_driver_set_led(8, -1);
   led_driver_set_led(9, 0);
   led_driver_flush();
   CHECK(   n_sets == 3
         && event_is(0, EV_SET, 7, 1)
         && event_is(1, EV_SET, 8, 1)
         && event_is(2, EV_SET, 9, 0),
         "a state did not reach the driver as 0 or 1");

   reset();
   led_driver_set_led(3, 1);
   led_driver_flush();
   led_driver_set_led(3, 1);
   led_driver_flush();
   CHECK(n_sets == 2 && event_is(1, EV_SET, 3, 1),
         "an LED set to the state it had was not written");

   reset();
   led_driver_set_led(-1, 1);
   led_driver_set_led(MAX_LEDS, 1);
   led_driver_flush();
   CHECK(n_sets == 0, "an index outside the LEDs was written");
   led_driver_set_led(0, 1);
   led_driver_set_led(MAX_LEDS - 1, 1);
   led_driver_flush();
   CHECK(   n_sets == 2
         && event_is(0, EV_SET, 0, 1)
         && event_is(1, EV_SET, MAX_LEDS - 1, 1),
         "the first or last LED was not written");
}

/* 6, 7 */
static void test_null_driver(void)
{
   static const char *names[] = { NULL, "null", "bogus" };
   unsigned i;

   for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
   {
      led_driver_init("overlay");
      led_driver_set_led(4, 1);
      reset();
      led_driver_init(names[i]);
      led_driver_set_led(3, 1);
      led_driver_flush();
      CHECK(n_events == 0, "the recorder was written after a switch to null");
   }
}

/* 6 */
static void test_log(void)
{
   static const char *names[] = { NULL, "null", "bogus" };
   unsigned i;

   for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
   {
      n_logs = 0;
      led_driver_init(names[i]);
      CHECK(n_logs == 0, "choosing the null driver logged a driver");
   }

   n_logs    = 0;
   logged    = NULL;
   led_driver_init("overlay");
   CHECK(n_logs == 1 && logged && !strcmp(logged, "recorder"),
         "the driver chosen was not logged under its own name");
}

/* 7 */
static void test_drop_on_init(void)
{
   led_driver_init("overlay");
   led_driver_set_led(3, 1);
   reset();
   led_driver_init("overlay");
   led_driver_flush();
   CHECK(n_sets == 0, "choosing a driver kept what was set for the last");
}

/* 8 */
#define N_POSTS 1000

static retro_atomic_int_t posters_go;
static retro_atomic_int_t posters_done;

struct poster
{
   int led;
   int first; /* alternates from this */
   int last;
};

static void poster_thread(void *arg)
{
   struct poster *p = (struct poster*)arg;
   int i;

   while (!retro_atomic_load_acquire_int(&posters_go)) { }

   for (i = 0; i < N_POSTS; i++)
   {
      p->last = (p->first + i) & 1;
      led_driver_set_led(p->led, p->last);
   }
   retro_atomic_fetch_add_int(&posters_done, 1);
}

static void test_threads(void)
{
   struct poster post[2];
   sthread_t *threads[2];
   unsigned i;
   bool foreign = false;

   led_driver_init("overlay");
   reset();
   retro_atomic_store_release_int(&posters_go, 0);
   retro_atomic_store_release_int(&posters_done, 0);

   post[0].led   = 2;
   post[0].first = 0;
   post[1].led   = 9;
   post[1].first = 1;
   for (i = 0; i < 2; i++)
   {
      post[i].last = -1;
      threads[i]   = sthread_create(poster_thread, &post[i]);
   }
   retro_atomic_store_release_int(&posters_go, 1);

   /* the frame loop */
   while (retro_atomic_load_acquire_int(&posters_done) < 2)
      led_driver_flush();

   for (i = 0; i < 2; i++)
      sthread_join(threads[i]);

   led_driver_flush();

   for (i = 0; i < 2; i++)
      CHECK(last_value[post[i].led] == post[i].last,
            "an LED did not end on the last state its thread set");
   for (i = 0; i < MAX_LEDS; i++)
      if (i != 2 && i != 9 && last_value[i] != -1)
         foreign = true;
   CHECK(!foreign, "an LED nobody set was written");
   CHECK(!set_off_thread, "the driver was called off the flushing thread");
}

/* 9 */
static void test_free(void)
{
   led_driver_init("overlay");
   reset();
   led_driver_set_led(4, 1);
   led_driver_free();
   CHECK(   n_events == 2
         && event_is(0, EV_SET, 4, 1)
         && event_is(1, EV_FREE, 0, 0),
         "freeing did not write what was pending, then free once");

   reset();
   led_driver_set_led(3, 1);
   led_driver_flush();
   led_driver_free();
   CHECK(n_events == 0, "a freed driver was called");
}

int main(void)
{
   flush_thread = sthread_get_current_thread_id();

   test_before_init();
   test_init();
   test_flush();
   test_null_driver();
   test_log();
   test_drop_on_init();
   test_threads();
   test_free();

   if (failures)
   {
      fprintf(stderr, "FAIL led_driver_test: %u failures\n", failures);
      return 1;
   }

   fprintf(stderr, "PASS led_driver_test\n");
   return 0;
}
