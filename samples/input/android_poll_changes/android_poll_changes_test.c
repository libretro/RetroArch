/* android_poll_changes_test.c -- what one Android input poll may apply.
 *
 * The input queue is read on a thread of its own, so a poll is handed
 * every event since the last one, and a key, a finger or a mouse button
 * can go down and back up in between. The core reads the state once a
 * frame, after the poll. The poll applies events in order until one
 * would undo what an earlier one in the same poll did, and leaves it
 * and the rest for the next poll (android_poll_changes.h). The claims:
 *
 *   1. A tap that comes and goes between two polls is shown for one.
 *   2. A repeat changes nothing and never waits, and the release of a
 *      held key is not kept back by a repeat in the same poll.
 *   3. Nothing waits behind an event that has to: the order is kept.
 *   4. The same key on two rows is two keys.
 *   5. A key or a row outside what is tracked never waits.
 *   6. A finger that goes down and up between polls is shown down for
 *      one; a cancel of fingers one of which went down waits too; more
 *      fingers than a poll tracks wait for the next.
 *   7. A mouse click between polls is shown for one, and the buttons
 *      are told apart.
 *   8. A hat - a D-pad on many pads - tapped between polls is shown
 *      pressed for one; a roll from one direction to the next shows
 *      both held on its way; another pad's hat does not wait.
 *
 * A drain that ignores the answer, as one would that applies all it
 * has, loses the taps; it is put in and asserted to be caught. */
#include <stdio.h>
#include <string.h>

#include <boolean.h>

#include "android_poll_changes.h"

static unsigned failures = 0;
static bool     quiet    = false;
static bool     sabotage = false;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         if (!quiet) \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

enum kind { KEY, POINTERS, MOUSE, HAT };

struct event
{
   enum kind kind;
   unsigned  row;      /* a key's */
   unsigned  code;     /* a key's keycode; a mouse's buttons, a hat's
                        * directions after it */
   int32_t   device;   /* the pointers' */
   int32_t   ids[4];
   unsigned  count;
   bool      down;
};

#define ROWS 9

/* What the core reads after a poll */
struct state
{
   uint8_t  keys[ROWS][ANDROID_POLL_CHANGES_KEY_BYTES];
   bool     fingers[2][32];
   unsigned mouse;
   unsigned hats[ROWS];
};

static android_poll_changes_t changes;

static bool held(const struct state *st, unsigned row, unsigned code)
{
   if (row >= ROWS || code >= ANDROID_POLL_CHANGES_KEY_BYTES * 8)
      return false;
   return BIT_GET(st->keys[row], code) != 0;
}

/* the poll: the events in order until one has to wait; the index of the
 * first left for the next poll */
static unsigned poll_once(struct state *st, const struct event *ev,
      unsigned first, unsigned count)
{
   unsigned i, j;

   android_poll_changes_begin(&changes);
   for (i = first; i < count; i++)
   {
      const struct event *e = &ev[i];
      bool ok = true;

      switch (e->kind)
      {
         case KEY:
            ok = android_poll_changes_key(&changes, e->row, e->code,
                  held(st, e->row, e->code), e->down);
            break;
         case POINTERS:
            ok = android_poll_changes_pointers(&changes, e->device,
                  e->ids, sizeof(e->ids[0]), e->count, e->down);
            break;
         case MOUSE:
            ok = android_poll_changes_mouse(&changes, st->mouse, e->code);
            break;
         case HAT:
            ok = android_poll_changes_hat(&changes, e->row,
                  st->hats[e->row], e->code);
            break;
      }
      if (!ok && !sabotage)
         break;

      switch (e->kind)
      {
         case KEY:
            if (e->row < ROWS && e->code < ANDROID_POLL_CHANGES_KEY_BYTES * 8)
            {
               if (e->down)
                  BIT_SET(st->keys[e->row], e->code);
               else
                  BIT_CLEAR(st->keys[e->row], e->code);
            }
            break;
         case POINTERS:
            for (j = 0; j < e->count; j++)
               st->fingers[e->device][e->ids[j]] = e->down;
            break;
         case MOUSE:
            st->mouse = e->code;
            break;
         case HAT:
            st->hats[e->row] = e->code;
            break;
      }
   }
   return i;
}

static void state_reset(struct state *st)
{
   memset(st, 0, sizeof(*st));
}

static struct event key(unsigned row, unsigned code, bool down)
{
   struct event e;
   memset(&e, 0, sizeof(e));
   e.kind = KEY;
   e.row  = row;
   e.code = code;
   e.down = down;
   return e;
}

static struct event fingers(int32_t device, int32_t a, int32_t b,
      unsigned count, bool down)
{
   struct event e;
   memset(&e, 0, sizeof(e));
   e.kind   = POINTERS;
   e.device = device;
   e.ids[0] = a;
   e.ids[1] = b;
   e.count  = count;
   e.down   = down;
   return e;
}

static struct event mouse(unsigned buttons)
{
   struct event e;
   memset(&e, 0, sizeof(e));
   e.kind = MOUSE;
   e.code = buttons;
   return e;
}

#define A 96
#define B 97
#define N(a) (sizeof(a) / sizeof((a)[0]))

static void lane_tap(void)
{
   struct state st;
   struct event ev[2];
   unsigned next;

   ev[0] = key(0, A, true);
   ev[1] = key(0, A, false);
   state_reset(&st);
   next = poll_once(&st, ev, 0, 2);
   CHECK(BIT_GET(st.keys[0], A), "a tap between two polls was never shown");
   CHECK(next == 1, "the release went in the poll that showed the press");
   next = poll_once(&st, ev, next, 2);
   CHECK(!BIT_GET(st.keys[0], A) && next == 2,
         "a tap was not let go at the next poll");
}

static void lane_repeat(void)
{
   struct state st;
   struct event ev[3];

   ev[0] = key(0, A, true);
   ev[1] = key(0, A, true);
   ev[2] = key(0, A, true);
   state_reset(&st);
   CHECK(poll_once(&st, ev, 0, 3) == 3, "a repeat was kept for the next poll");

   /* A held since an earlier poll: its repeat, then its release */
   ev[0] = key(0, A, true);
   ev[1] = key(0, A, false);
   state_reset(&st);
   BIT_SET(st.keys[0], A);
   CHECK(poll_once(&st, ev, 0, 2) == 2 && !BIT_GET(st.keys[0], A),
         "a release was kept back by a repeat before it");
}

static void lane_order(void)
{
   struct state st;
   struct event ev[4];
   unsigned next;

   /* B is let go before A is: the release of A waits, and B's,
    * behind it, waits with it */
   ev[0] = key(0, A, true);
   ev[1] = key(0, B, true);
   ev[2] = key(0, A, false);
   ev[3] = key(0, B, false);
   state_reset(&st);
   next = poll_once(&st, ev, 0, 4);
   CHECK(next == 2 && BIT_GET(st.keys[0], A) && BIT_GET(st.keys[0], B),
         "the presses before a release that waits were not all shown");
   next = poll_once(&st, ev, next, 4);
   CHECK(next == 4 && !BIT_GET(st.keys[0], A) && !BIT_GET(st.keys[0], B),
         "the releases left for the next poll were not both shown");
}

static void lane_rows(void)
{
   struct state st;
   struct event ev[4];

   ev[0] = key(0, A, true);
   ev[1] = key(8, A, true);
   ev[2] = key(1, A, true);
   ev[3] = key(1, A, false);
   state_reset(&st);
   CHECK(poll_once(&st, ev, 0, 4) == 3, "a key on one row waited for another row");
}

static void lane_out_of_range(void)
{
   struct state st;
   struct event ev[4];

   ev[0] = key(ROWS, A, true);
   ev[1] = key(ROWS, A, false);
   ev[2] = key(0, ANDROID_POLL_CHANGES_KEY_BYTES * 8, true);
   ev[3] = key(0, ANDROID_POLL_CHANGES_KEY_BYTES * 8, false);
   state_reset(&st);
   CHECK(poll_once(&st, ev, 0, 4) == 4, "a key outside what is tracked waited");
}

static void lane_fingers(void)
{
   struct state st;
   struct event ev[4];
   unsigned i, next;
   struct event many[ANDROID_POLL_CHANGES_POINTERS + 1];

   /* one finger down and up: shown down for a poll */
   ev[0] = fingers(0, 3, 0, 1, true);
   ev[1] = fingers(0, 3, 0, 1, false);
   state_reset(&st);
   next = poll_once(&st, ev, 0, 2);
   CHECK(next == 1 && st.fingers[0][3], "a touch between two polls was never shown");
   next = poll_once(&st, ev, next, 2);
   CHECK(next == 2 && !st.fingers[0][3], "a touch was not let go at the next poll");

   /* the same id on two devices is two fingers; a cancel takes two
    * fingers up, one of which came down in this poll */
   ev[0] = fingers(0, 3, 0, 1, true);
   ev[1] = fingers(1, 3, 0, 1, true);
   ev[2] = fingers(1, 4, 0, 1, true);
   ev[3] = fingers(1, 3, 4, 2, false);
   state_reset(&st);
   next = poll_once(&st, ev, 0, 4);
   CHECK(next == 3 && st.fingers[1][3] && st.fingers[1][4],
         "a cancel of fingers that came down this poll was applied with them");

   /* more fingers than a poll tracks */
   for (i = 0; i < N(many); i++)
      many[i] = fingers(i & 1, (int32_t)(i >> 1), 0, 1, true);
   state_reset(&st);
   CHECK(poll_once(&st, many, 0, N(many)) == ANDROID_POLL_CHANGES_POINTERS,
         "more fingers than a poll tracks went in one poll");
}

static void lane_mouse(void)
{
   struct state st;
   struct event ev[4];
   unsigned next;

   /* a left click between polls, a right press with it */
   ev[0] = mouse(1);
   ev[1] = mouse(1 | 2);
   ev[2] = mouse(2);
   ev[3] = mouse(0);
   state_reset(&st);
   next = poll_once(&st, ev, 0, 4);
   CHECK(next == 2 && st.mouse == 3, "a click between two polls was never shown");
   next = poll_once(&st, ev, next, 4);
   CHECK(next == 4 && st.mouse == 0,
         "the buttons were not both let go at the next poll");
}

static struct event hat(unsigned row, unsigned dirs)
{
   struct event e;
   memset(&e, 0, sizeof(e));
   e.kind = HAT;
   e.row  = row;
   e.code = dirs;
   return e;
}

static void lane_hat(void)
{
   struct state st;
   struct event ev[4];
   unsigned up    = android_poll_changes_hat_bits(0, -1);
   unsigned right = android_poll_changes_hat_bits(1, 0);
   unsigned next;

   CHECK(up && right && !(up & right) && !android_poll_changes_hat_bits(0, 0),
         "a hat's directions are not four bits of their own");

   /* up tapped between polls */
   ev[0] = hat(2, up);
   ev[1] = hat(2, 0);
   state_reset(&st);
   next = poll_once(&st, ev, 0, 2);
   CHECK(next == 1 && st.hats[2] == up, "a hat tap between two polls was never shown");
   next = poll_once(&st, ev, next, 2);
   CHECK(next == 2 && st.hats[2] == 0, "the hat was not let go at the next poll");

   /* up, rolled through up-right to right, and let go: each shown */
   ev[0] = hat(2, up);
   ev[1] = hat(2, up | right);
   ev[2] = hat(2, right);
   ev[3] = hat(2, 0);
   state_reset(&st);
   next = poll_once(&st, ev, 0, 4);
   CHECK(next == 2 && st.hats[2] == (up | right),
         "a roll was taken past where up was let go in the same poll");
   /* right was shown held with up: up and then right let go together */
   next = poll_once(&st, ev, next, 4);
   CHECK(next == 4 && st.hats[2] == 0, "the hat was not let go");

   /* another pad's hat does not wait on this one's */
   ev[0] = hat(2, up);
   ev[1] = hat(3, up);
   ev[2] = hat(3, 0);
   state_reset(&st);
   next = poll_once(&st, ev, 0, 3);
   CHECK(next == 2 && st.hats[2] == up && st.hats[3] == up,
         "a second pad's hat was held back by the first's");
}

static void all_lanes(void)
{
   lane_tap();
   lane_repeat();
   lane_order();
   lane_rows();
   lane_out_of_range();
   lane_fingers();
   lane_mouse();
   lane_hat();
}

int main(void)
{
   all_lanes();
   if (failures)
   {
      fprintf(stderr, "FAIL android_poll_changes_test: %u failures\n",
            failures);
      return 1;
   }

   /* A drain that applies everything it has has to be caught. */
   quiet    = true;
   sabotage = true;
   all_lanes();
   quiet    = false;
   sabotage = false;

   if (!failures)
   {
      fprintf(stderr, "FAIL android_poll_changes_test: a drain that"
            " applies every event went unnoticed\n");
      return 1;
   }

   fprintf(stderr, "PASS android_poll_changes_test (sabotage caught by"
         " %u checks)\n", failures);
   return 0;
}
