/* android_block_timeout_test.c -- the input block timeout on Android,
 * without waiting.
 *
 * Some pads send one press of several buttons as separate events a few
 * milliseconds apart, and a poll can fall between them. The timeout
 * used to be waited by the poll - every frame stalled by it, pressed or
 * not - and then for a while it did nothing at all. Now the poll never
 * waits in the foreground, and the timeout keeps a press back instead:
 * a pad's presses whose newest came within it of the poll wait for the
 * next one, so the rest of the press is shown with them.
 *
 * The waits and the keeping back are the driver's own
 * (android_poll_wait.h, android_press_group.h); the times are Android's
 * event times, given here. The claims:
 *
 *   1. In the foreground the poll waits for nothing; backgrounded it
 *      waits for what wakes it, then drains the queue without waiting.
 *   2. A press older than the timeout is shown at the poll it came in.
 *   3. One that came within it waits for the next poll, and the rest of
 *      the press, coming before that, is shown with it.
 *   4. Nothing waits more than one poll: presses held over are shown at
 *      the next with whatever came since, however new.
 *   5. A press let go before it was shown is shown for one poll.
 *   6. With the timeout at 0 nothing is kept back, and what was is
 *      shown.
 *   7. A poll with no press in it needs no time: what was held over is
 *      shown, and a tap let go, without one. */
#include <stdio.h>
#include <string.h>

#include "../../../input/drivers/android_poll_wait.h"
#include "../../../input/drivers/android_press_group.h"

static unsigned failures;
#define CHECK(c, ...) do { if (!(c)) { printf("[FAIL] "); printf(__VA_ARGS__); \
   printf("\n"); failures++; } } while (0)

#define MS(x) ((int64_t)(x) * 1000000)
#define ROW_BYTES 28

static android_press_group_t g;
static uint8_t row[ROW_BYTES];

static void reset(void)
{
   memset(&g, 0, sizeof(g));
   memset(row, 0, sizeof(row));
}

/* a poll's end at @at_ms, with a window of @window_ms */
static void poll_end(int at_ms, int window_ms)
{
   android_press_group_end(&g, row, ROW_BYTES, MS(at_ms), MS(window_ms));
}

#define A 96 /* AKEYCODE_BUTTON_A */
#define B 97 /* AKEYCODE_BUTTON_B */

int main(void)
{
   int first, then;

   /* 1. */
   android_poll_waits(false, &first, &then);
   CHECK(first == 0 && then == 0, "in the foreground the poll waits %d then %d ms; want 0 and 0", first, then);
   android_poll_waits(true, &first, &then);
   CHECK(first == -1 && then == 0, "backgrounded the poll waits %d then %d; want -1 and 0", first, then);

   /* 2. a press 10 ms old at a 5 ms window: shown now */
   reset();
   android_press_group_down(&g, A, MS(0));
   poll_end(10, 5);
   CHECK(BIT_GET(row, A), "a press older than the window was kept back");

   /* 3. A 2 ms before the poll: kept; B comes 2 ms after it; both at
    *    the next poll, together */
   reset();
   android_press_group_down(&g, A, MS(8));
   poll_end(10, 5);
   CHECK(!BIT_GET(row, A), "a press within the window was shown before the rest could come");
   android_press_group_down(&g, B, MS(12));
   poll_end(26, 5);
   CHECK(BIT_GET(row, A) && BIT_GET(row, B), "the two halves of a press were not shown together");

   /* 4. held over and nothing else: shown at the next poll; held over
    *    and a new press right at the poll: both shown, no second wait */
   reset();
   android_press_group_down(&g, A, MS(9));
   poll_end(10, 5);
   poll_end(26, 5);
   CHECK(BIT_GET(row, A), "a press held over was not shown at the next poll");
   /* with no press that poll, the time is not read: held over and a
    * tap's end go by none */
   reset();
   android_press_group_down(&g, A, MS(9));
   android_press_group_up(&g, A);
   poll_end(10, 5);
   android_press_group_end(&g, row, ROW_BYTES, 0, MS(5));
   CHECK(BIT_GET(row, A), "a press held over needed the time to be shown");
   android_press_group_end(&g, row, ROW_BYTES, 0, MS(5));
   CHECK(!BIT_GET(row, A), "a tap's end needed the time");
   reset();
   android_press_group_down(&g, A, MS(9));
   poll_end(10, 5);
   android_press_group_down(&g, B, MS(25));
   poll_end(26, 5);
   CHECK(BIT_GET(row, A) && BIT_GET(row, B), "a press waited more than one poll");

   /* 5. pressed and let go before shown: shown for one poll, then let go */
   reset();
   android_press_group_down(&g, A, MS(9));
   CHECK(android_press_group_up(&g, A), "the release of a press kept back was not taken as a tap");
   poll_end(10, 5);
   poll_end(26, 5);
   CHECK(BIT_GET(row, A), "a tap was lost");
   poll_end(42, 5);
   CHECK(!BIT_GET(row, A), "a tap was held after its poll");
   /* a release of one shown already is the row's */
   CHECK(!android_press_group_up(&g, B), "a release of a press not kept back was taken");

   /* 6. the window at 0: shown at once; one held when it went to 0 too */
   reset();
   android_press_group_down(&g, A, MS(10));
   poll_end(10, 0);
   CHECK(BIT_GET(row, A), "with the window at 0 a press was kept back");
   reset();
   android_press_group_down(&g, A, MS(9));
   poll_end(10, 5);
   poll_end(26, 0);
   CHECK(BIT_GET(row, A), "a press held when the window went to 0 was not shown");

   printf("[%s] android block timeout: the poll never waits in the foreground; a press within the window waits one poll for the rest, older ones are shown at once, none waits longer, a tap is shown for a poll, 0 keeps nothing back\n",
         failures ? "fail" : "pass");
   return failures ? 1 : 0;
}
