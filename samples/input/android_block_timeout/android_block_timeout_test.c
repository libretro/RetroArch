/* android_block_timeout_test.c -- the input block timeout, as the
 * Android input poll waits by it.
 *
 * Some pads send one press of several buttons as separate events a few
 * milliseconds apart. With "Input Block Timeout" at N, the poll waits
 * up to N ms for an event and again up to N ms after each one, so that
 * the rest of such a press is taken in the same poll. A change that
 * stopped the poll draining the battery in the background waited only
 * for the first event and took nothing after it, and the setting did
 * nothing any more.
 *
 * The waits are the driver's own (android_poll_wait.h); the looper is a
 * queue of events, each due at a time, that ALooper_pollOnce's way of
 * waiting takes from. The claims:
 *
 *   1. At 0, the default, a poll takes what is queued and waits for
 *      nothing.
 *   2. At N, events each within N ms of the last are taken in one poll,
 *      and one past that is left for the next.
 *   3. Backgrounded, the poll waits for whatever comes, however late,
 *      then takes only what is already queued.
 *   4. The broken way - waiting for the first event only - is caught:
 *      at N it takes one event of a press sent as three. */
#include <stdio.h>
#include <string.h>

#include "../../../input/drivers/android_poll_wait.h"

static unsigned failures;
#define CHECK(c, ...) do { if (!(c)) { printf("[FAIL] "); printf(__VA_ARGS__); \
   printf("\n"); failures++; } } while (0)

/* the looper: events due at these times, ms; now moves as it waits */
static int due[8];
static unsigned n_due, next_due;
static int now;

static void looper(const int *at, unsigned n, int start)
{
   memcpy(due, at, n * sizeof(*at));
   n_due    = n;
   next_due = 0;
   now      = start;
}

/* as ALooper_pollOnce: the next event if it comes within @timeout ms
 * (-1: however long), else -1 once the time is up */
static int poll_once(int timeout)
{
   if (next_due < n_due)
   {
      int at = due[next_due];
      if (at <= now || timeout < 0 || at <= now + timeout)
      {
         if (at > now)
            now = at;
         next_due++;
         return 1;
      }
   }
   if (timeout > 0)
      now += timeout;
   return -1;
}

/* one input poll, as android_input_poll() runs its loop; @broken waits
 * for the first event only. The events it took. */
static unsigned input_poll(int block_ms, bool idle, bool broken)
{
   int timeout, then;
   unsigned taken = 0;
   android_poll_waits(block_ms, idle, &timeout, &then);
   if (broken && !idle)
      then = 0;
   while (poll_once(timeout) >= 0)
   {
      timeout = then;
      taken++;
   }
   return taken;
}

int main(void)
{
   /* a press sent as three events, 3 ms apart, then one 6 ms later */
   static const int press[4] = { 0, 3, 6, 12 };
   static const int late[2]  = { 1000, 1001 };
   unsigned got;

   /* 1. the default: what is queued at the poll, nothing waited for */
   looper(press, 4, 0);
   got = input_poll(0, false, false);
   CHECK(got == 1 && now == 0, "at 0, the poll took %u events and waited %d ms; want 1 and 0", got, now);

   /* 2. at 5 ms: the three, each within 5 ms of the last; not the
    *    fourth, 6 ms after the third, which the next poll has */
   looper(press, 4, 0);
   got = input_poll(5, false, false);
   CHECK(got == 3, "at 5 ms the poll took %u events of the press; want 3", got);
   got = input_poll(5, false, false);
   CHECK(got == 1, "the event past the window was not left for the next poll (%u)", got);

   /* no events at all: the poll waits the window once and goes on */
   looper(press, 0, 0);
   got = input_poll(5, false, false);
   CHECK(got == 0 && now == 5, "with nothing coming the poll took %u and waited %d ms; want 0 and 5", got, now);

   /* 3. backgrounded: waits for what comes, then takes no more than
    *    is queued */
   looper(late, 2, 0);
   got = input_poll(5, true, false);
   CHECK(got == 1 && now == 1000, "backgrounded, the poll took %u events at %d ms; want 1 at 1000", got, now);

   /* 4. the broken way is caught */
   looper(press, 4, 0);
   got = input_poll(5, false, true);
   CHECK(got == 1, "waiting for the first event only was not caught (%u)", got);

   printf("[%s] android block timeout: nothing waited for at 0; at N a press sent as several events taken in one poll, one past the window left for the next; backgrounded, woken by what comes and the queue drained\n",
         failures ? "fail" : "pass");
   return failures ? 1 : 0;
}
