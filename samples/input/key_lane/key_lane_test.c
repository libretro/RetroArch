/* Oracle for input/input_key_lane.h, the queue keyboard events
 * reported off the frontend's thread wait in for the poll. It includes
 * the shipping header and drives it directly:
 *
 * 1. a writer stopped after claiming a slot, the lane filled behind it,
 *    a release that does not fit: the key's release still comes, after
 *    its press, once the stopped writer goes on - the key is not left
 *    held (the schedule from the 2026-10 audit);
 * 2. a release kept from a full lane is taken back by a newer press,
 *    accepted into the lane or reported on the reader's thread: it
 *    never releases the newer press;
 * 3. positions and sequences across the wrap of their counters, under
 *    UndefinedBehaviorSanitizer: every event through, in order;
 * 4. four writers at once, 20,000 events each: every event exactly
 *    once, each writer's in order (the TSan target).
 *
 * Build with SANITIZER=undefined,address and SANITIZER=thread too. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>

#include <libretro.h>
#include <retro_atomic.h>

/* the pause point: a writer that has marked itself stops right after it
 * has claimed its slot, until told to go on */
static retro_atomic_int_t stopped, go_on;
static __thread int stop_me;
static __thread int stop_release;
static retro_atomic_int_t release_stopped, release_go;
#define INPUT_KEY_LANE_BEFORE_RELEASE(lane, code, pos) do { \
   if (stop_release) \
   { \
      retro_atomic_store_release_int(&release_stopped, 1); \
      while (!retro_atomic_load_acquire_int(&release_go)) \
         sched_yield(); \
   } } while (0)
#define INPUT_KEY_LANE_AFTER_CLAIM(lane, pos) do { \
   if (stop_me) \
   { \
      retro_atomic_store_release_int(&stopped, 1); \
      while (!retro_atomic_load_acquire_int(&go_on)) \
         sched_yield(); \
   } } while (0)

#include "input/input_key_lane.h"

static unsigned failures;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; \
   fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
   fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static input_key_lane_t lane;

/* what the reader gives out: per key, how many downs and ups, and
 * whether it is held at the end; and per writer, the order */
static unsigned downs[RETROK_LAST], ups[RETROK_LAST];
static unsigned got[4], bad[4];
static bool newer_f1_on_delivery;
static unsigned reposts_left;
static void deliver(bool down, unsigned code, uint32_t character,
      uint16_t mod, unsigned device)
{
   (void)mod; (void)device;
   input_key_lane_note(&lane, down, code, false);
   if (code < RETROK_LAST)
   {
      if (down)
         downs[code]++;
      else
         ups[code]++;
   }
   if (down && code == RETROK_F1 && newer_f1_on_delivery)
   {
      newer_f1_on_delivery = false;
      input_key_lane_push(&lane, true, RETROK_F1, 0, 0, RETRO_DEVICE_KEYBOARD);
   }
   if (down && code == RETROK_F6 && reposts_left)
   {
      reposts_left--;
      input_key_lane_push(&lane, true, RETROK_F6, 0, 0, RETRO_DEVICE_KEYBOARD);
   }
   /* the stress writers send keys 10..13, presses only, numbered */
   if (down && code >= 10 && code < 14)
   {
      unsigned w = code - 10;
      if (character != got[w])
         bad[w]++;
      got[w]++;
   }
}

static bool held(unsigned code)
{
   return (lane.keys_down[code >> 5] & (1u << (code & 31))) != 0;
}

static void reset(void)
{
   memset(&lane, 0, sizeof(lane));
   memset(downs, 0, sizeof(downs));
   memset(ups, 0, sizeof(ups));
}

/* ---- 1 ---------------------------------------------------------- */

static void *stopped_writer(void *data)
{
   (void)data;
   stop_me = 1;
   input_key_lane_push(&lane, true, RETROK_F1, 0, 0, RETRO_DEVICE_KEYBOARD);
   return NULL;
}

static void lane_stopped_writer(void)
{
   unsigned had = failures;
   pthread_t a;
   unsigned i;

   reset();
   retro_atomic_store_release_int(&stopped, 0);
   retro_atomic_store_release_int(&go_on, 0);
   pthread_create(&a, NULL, stopped_writer, NULL);
   /* wait until it has claimed position 0 and stopped there */
   while (!retro_atomic_load_acquire_int(&stopped))
      sched_yield();

   /* A has position 0 and stops there. This thread: key 1 down, 62
    * others - the lane is full - then key 1 up, which does not fit */
   CHECK(input_key_lane_push(&lane, true, RETROK_a, 0, 0, RETRO_DEVICE_KEYBOARD),
         "stopped writer: key down did not fit in an empty lane");
   for (i = 0; i < INPUT_KEY_LANE_SIZE - 2; i++)
      input_key_lane_push(&lane, (i & 1) == 0, RETROK_F2, 0, 0, RETRO_DEVICE_KEYBOARD);
   CHECK(!input_key_lane_push(&lane, false, RETROK_a, 0, 0, RETRO_DEVICE_KEYBOARD),
         "stopped writer: the lane was not full");

   /* a poll while A has not filled its slot: nothing can be given out */
   input_key_lane_take(&lane, deliver);
   CHECK(downs[RETROK_a] == 0, "stopped writer: an event behind the unfilled slot was given out");

   retro_atomic_store_release_int(&go_on, 1);
   pthread_join(a, NULL);

   input_key_lane_take(&lane, deliver);
   input_key_lane_take(&lane, deliver);
   CHECK(downs[RETROK_a] == 1 && ups[RETROK_a] == 1,
         "stopped writer: key down %u time(s), up %u time(s), not once each",
         downs[RETROK_a], ups[RETROK_a]);
   CHECK(!held(RETROK_a), "stopped writer: the key was left held");
   if (failures == had)
      fprintf(stderr, "[pass] a writer stopped after claiming a slot: the"
            " release that did not fit still comes, after its press\n");
}

/* ---- 2 ---------------------------------------------------------- */

static void fill_full(void)
{
   unsigned i;
   for (i = 0; i < INPUT_KEY_LANE_SIZE; i++)
      input_key_lane_push(&lane, (i & 1) == 0, RETROK_F2, 0, 0, RETRO_DEVICE_KEYBOARD);
}

static void lane_newer_press(void)
{
   unsigned had = failures;

   /* b down given out; the lane fills; b up does not fit and is kept;
    * then b is pressed again on the reader's thread: the kept release
    * must not undo that press */
   reset();
   input_key_lane_push(&lane, true, RETROK_b, 0, 0, RETRO_DEVICE_KEYBOARD);
   input_key_lane_take(&lane, deliver);
   fill_full();
   input_key_lane_push(&lane, false, RETROK_b, 0, 0, RETRO_DEVICE_KEYBOARD);
   input_key_lane_take_back_release(&lane, RETROK_b);   /* the reader's own press */
   deliver(true, RETROK_b, 0, 0, RETRO_DEVICE_KEYBOARD);
   input_key_lane_take(&lane, deliver);
   input_key_lane_take(&lane, deliver);
   CHECK(held(RETROK_b) && ups[RETROK_b] == 0,
         "newer press: a kept release undid a press made on the reader's thread");

   /* the same with the newer press accepted into the lane */
   reset();
   input_key_lane_push(&lane, true, RETROK_c, 0, 0, RETRO_DEVICE_KEYBOARD);
   input_key_lane_take(&lane, deliver);
   fill_full();
   input_key_lane_push(&lane, false, RETROK_c, 0, 0, RETRO_DEVICE_KEYBOARD);
   input_key_lane_take(&lane, deliver);  /* drains; the kept up is given out (c is down) */
   CHECK(!held(RETROK_c) && ups[RETROK_c] == 1, "newer press: the kept release was not given out");
   fill_full();
   input_key_lane_push(&lane, false, RETROK_c, 0, 0, RETRO_DEVICE_KEYBOARD); /* kept again (c up: waits) */
   input_key_lane_take(&lane, deliver);                                       /* drains */
   input_key_lane_push(&lane, true, RETROK_c, 0, 0, RETRO_DEVICE_KEYBOARD);   /* newer press, accepted */
   input_key_lane_take(&lane, deliver);
   input_key_lane_take(&lane, deliver);
   CHECK(held(RETROK_c) && ups[RETROK_c] == 1,
         "newer press: a kept release undid a newer press accepted into the lane (ups %u)",
         ups[RETROK_c]);
   if (failures == had)
      fprintf(stderr, "[pass] a kept release never undoes a newer press\n");
}

/* ---- 3 ---------------------------------------------------------- */

static void lane_wrap(void)
{
   unsigned had = failures;
   unsigned start = UINT_MAX - 500u, i, n = 0, expect = 0;

   /* the lane as it is after `start` events: empty, every slot free for
    * the position it will next take */
   reset();
   lane.head = start;
   lane.position = start;
   retro_atomic_store_release_int(&lane.tail, (int)start);
   for (i = 0; i < INPUT_KEY_LANE_SIZE; i++)
   {
      unsigned p = start + ((i - start) & (INPUT_KEY_LANE_SIZE - 1));
      retro_atomic_store_release_int(&lane.slot[i].seq, (int)(p - i));
   }
   memset(got, 0, sizeof(got));
   memset(bad, 0, sizeof(bad));
   /* 2000 events across the wrap, 40 a poll */
   while (n < 2000)
   {
      unsigned k;
      for (k = 0; k < 40; k++, n++)
         CHECK(input_key_lane_push(&lane, true, 10, n, 0, RETRO_DEVICE_KEYBOARD),
               "wrap: event %u did not fit", n);
      input_key_lane_take(&lane, deliver);
   }
   CHECK(got[0] == 2000 && bad[0] == 0, "wrap: %u of 2000 through, %u out of order",
         got[0], bad[0]);
   CHECK(lane.head == start + 2000u, "wrap: the reader is at %u, not %u",
         lane.head, start + 2000u);
   (void)expect;
   if (failures == had)
      fprintf(stderr, "[pass] across the wrap of the counters: every event"
            " through, in order\n");
}

/* ---- 4 ---------------------------------------------------------- */

#define STRESS_N 20000
static void *stress_writer(void *data)
{
   unsigned k = (unsigned)(uintptr_t)data, i;
   for (i = 0; i < STRESS_N; )
      if (input_key_lane_push(&lane, true, 10 + k, i, 0, RETRO_DEVICE_KEYBOARD))
         i++;
      else
         sched_yield();
   return NULL;
}

static void lane_stress(void)
{
   unsigned had = failures;
   pthread_t t[4];
   unsigned k;
   bool done;

   reset();
   memset(got, 0, sizeof(got));
   memset(bad, 0, sizeof(bad));
   for (k = 0; k < 4; k++)
      pthread_create(&t[k], NULL, stress_writer, (void*)(uintptr_t)k);
   do
   {
      input_key_lane_take(&lane, deliver);
      done = true;
      for (k = 0; k < 4; k++)
         if (got[k] < STRESS_N)
            done = false;
   } while (!done);
   for (k = 0; k < 4; k++)
      pthread_join(t[k], NULL);
   input_key_lane_take(&lane, deliver);
   for (k = 0; k < 4; k++)
      CHECK(got[k] == STRESS_N && bad[k] == 0,
            "four writers: writer %u: %u of %u through, %u out of order",
            k, got[k], STRESS_N, bad[k]);
   if (failures == had)
      fprintf(stderr, "[pass] four writers at once: every event exactly once,"
            " each writer's in order\n");
}

static void lane_stopped_key_release(void)
{
   unsigned had = failures, i;
   pthread_t writer;

   reset();
   retro_atomic_store_release_int(&stopped, 0);
   retro_atomic_store_release_int(&go_on, 0);
   pthread_create(&writer, NULL, stopped_writer, NULL);
   while (!retro_atomic_load_acquire_int(&stopped))
      sched_yield();
   for (i = 1; i < INPUT_KEY_LANE_SIZE; i++)
      input_key_lane_push(&lane, true, RETROK_F2, 0, 0, RETRO_DEVICE_KEYBOARD);
   CHECK(!input_key_lane_push(&lane, false, RETROK_F1, 0, 0, RETRO_DEVICE_KEYBOARD),
         "paused key release: the queue was not full");
   input_key_lane_take(&lane, deliver);
   CHECK(!downs[RETROK_F1] && !ups[RETROK_F1],
         "paused key release: recovery overtook an unpublished press");
   retro_atomic_store_release_int(&go_on, 1);
   pthread_join(writer, NULL);
   input_key_lane_take(&lane, deliver);
   input_key_lane_take(&lane, deliver);
   CHECK(downs[RETROK_F1] == 1 && ups[RETROK_F1] == 1 && !held(RETROK_F1),
         "an older paused press erased a newer retained release");
   if (failures == had)
      fprintf(stderr, "[pass] a paused press cannot erase its newer overflow release\n");
}

static void empty_at(unsigned start)
{
   unsigned i;
   lane.head = start;
   lane.position = start;
   retro_atomic_store_release_int(&lane.tail, (int)start);
   for (i = 0; i < INPUT_KEY_LANE_SIZE; i++)
   {
      unsigned p = start + ((i - start) & (INPUT_KEY_LANE_SIZE - 1));
      retro_atomic_store_release_int(&lane.slot[i].seq, (int)(p - i));
   }
}

static void lane_recovery_positions(void)
{
   unsigned had = failures, test, i;
   static const unsigned starts[] = {0x7ffffffdu, UINT_MAX - 2u};

   for (test = 0; test < 2; test++)
   {
      reset();
      empty_at(starts[test]);
      input_key_lane_push(&lane, true, RETROK_F1, 0, 0, RETRO_DEVICE_KEYBOARD);
      for (i = 1; i < INPUT_KEY_LANE_SIZE; i++)
         input_key_lane_push(&lane, true, RETROK_F2, 0, 0, RETRO_DEVICE_KEYBOARD);
      input_key_lane_push(&lane, false, RETROK_F1, 0, 0, RETRO_DEVICE_KEYBOARD);
      input_key_lane_take(&lane, deliver);
      CHECK(downs[RETROK_F1] == 1 && ups[RETROK_F1] == 1 && !held(RETROK_F1),
            "overflow release changed across a position wrap");
   }

   reset();
   input_key_lane_take_back_release(&lane, RETROK_F1);
   deliver(true, RETROK_F1, 0, 0, RETRO_DEVICE_KEYBOARD);
   empty_at(0x80000400u);
   fill_full();
   input_key_lane_push(&lane, false, RETROK_F1, 0, 0, RETRO_DEVICE_KEYBOARD);
   input_key_lane_take(&lane, deliver);
   CHECK(ups[RETROK_F1] == 1 && !held(RETROK_F1),
         "a long-held key was mistaken for a newer press after a stamp lap");

   reset();
   input_key_lane_push(&lane, true, RETROK_F1, 0, 0, RETRO_DEVICE_KEYBOARD);
   for (i = 1; i < INPUT_KEY_LANE_SIZE; i++)
      input_key_lane_push(&lane, true, RETROK_F2, 0, 0, RETRO_DEVICE_KEYBOARD);
   input_key_lane_push(&lane, false, RETROK_F1, 0, 0, RETRO_DEVICE_KEYBOARD);
   newer_f1_on_delivery = true;
   input_key_lane_take(&lane, deliver);
   input_key_lane_take(&lane, deliver);
   CHECK(downs[RETROK_F1] == 2 && ups[RETROK_F1] == 1 && held(RETROK_F1),
         "recovery undid a queued press claimed after its cutoff");
   if (failures == had)
      fprintf(stderr, "[pass] recovery positions wrap and do not undo a newer press\n");
}

static void *delayed_release(void *data)
{
   (void)data;
   stop_release = 1;
   input_key_lane_push(&lane, false, RETROK_F1, 0, 0, RETRO_DEVICE_KEYBOARD);
   return NULL;
}

static void lane_delayed_release(void)
{
   unsigned had = failures, test;
   pthread_t writer;

   for (test = 0; test < 2; test++)
   {
      reset();
      deliver(true, RETROK_F1, 0, 0, RETRO_DEVICE_KEYBOARD);
      fill_full();
      retro_atomic_store_release_int(&release_stopped, 0);
      retro_atomic_store_release_int(&release_go, 0);
      pthread_create(&writer, NULL, delayed_release, NULL);
      while (!retro_atomic_load_acquire_int(&release_stopped))
         sched_yield();
      input_key_lane_take(&lane, deliver);
      input_key_lane_push(&lane, true, RETROK_F1, 0, 0, RETRO_DEVICE_KEYBOARD);
      input_key_lane_take(&lane, deliver);
      if (test)
      {
         fill_full();
         input_key_lane_push(&lane, false, RETROK_F1, 0, 0, RETRO_DEVICE_KEYBOARD);
      }
      retro_atomic_store_release_int(&release_go, 1);
      pthread_join(writer, NULL);
      input_key_lane_take(&lane, deliver);
      input_key_lane_take(&lane, deliver);
      CHECK(test ? (ups[RETROK_F1] == 1 && !held(RETROK_F1))
                 : (ups[RETROK_F1] == 0 && held(RETROK_F1)),
            "a delayed older overflow release changed newer key state");
   }
   if (failures == had)
      fprintf(stderr, "[pass] delayed release publication cannot supersede newer state\n");
}

static void lane_direct_order(void)
{
   unsigned had = failures;
   unsigned i;
   pthread_t writer;

   reset();
   input_key_lane_push(&lane, true, RETROK_a, 0, 0, RETRO_DEVICE_KEYBOARD);
   input_key_lane_dispatch(&lane, false, RETROK_a, 0, 0,
         RETRO_DEVICE_KEYBOARD, deliver);
   input_key_lane_take(&lane, deliver);
   CHECK(downs[RETROK_a] == 1 && ups[RETROK_a] == 1 && !held(RETROK_a),
         "direct release overtook an older queued press");

   reset();
   deliver(true, RETROK_b, 0, 0, RETRO_DEVICE_KEYBOARD);
   input_key_lane_push(&lane, false, RETROK_b, 0, 0, RETRO_DEVICE_KEYBOARD);
   input_key_lane_dispatch(&lane, true, RETROK_b, 0, 0,
         RETRO_DEVICE_KEYBOARD, deliver);
   input_key_lane_take(&lane, deliver);
   CHECK(downs[RETROK_b] == 2 && ups[RETROK_b] == 1 && held(RETROK_b),
         "direct press overtook an older queued release");

   reset();
   retro_atomic_store_release_int(&stopped, 0);
   retro_atomic_store_release_int(&go_on, 0);
   pthread_create(&writer, NULL, stopped_writer, NULL);
   while (!retro_atomic_load_acquire_int(&stopped))
      sched_yield();
   input_key_lane_dispatch(&lane, false, RETROK_F1, 0, 0,
         RETRO_DEVICE_KEYBOARD, deliver);
   CHECK(ups[RETROK_F1] == 0,
         "direct release overtook a claimed but unpublished press");
   retro_atomic_store_release_int(&go_on, 1);
   pthread_join(writer, NULL);
   input_key_lane_take(&lane, deliver);
   CHECK(downs[RETROK_F1] == 1 && ups[RETROK_F1] == 1 && !held(RETROK_F1),
         "direct release left a key held after the writer resumed");

   reset();
   retro_atomic_store_release_int(&stopped, 0);
   retro_atomic_store_release_int(&go_on, 0);
   pthread_create(&writer, NULL, stopped_writer, NULL);
   while (!retro_atomic_load_acquire_int(&stopped))
      sched_yield();
   for (i = 1; i < INPUT_KEY_LANE_SIZE; i++)
      input_key_lane_push(&lane, true, RETROK_F2, 0, 0, RETRO_DEVICE_KEYBOARD);
   input_key_lane_dispatch(&lane, false, RETROK_F1, 0, 0,
         RETRO_DEVICE_KEYBOARD, deliver);
   CHECK(ups[RETROK_F1] == 0 && input_key_lane_load(&lane.dropped) == 1,
         "full blocked lane did not retain the direct release");
   retro_atomic_store_release_int(&go_on, 1);
   pthread_join(writer, NULL);
   input_key_lane_take(&lane, deliver);
   CHECK(downs[RETROK_F1] == 1 && ups[RETROK_F1] == 1 && !held(RETROK_F1),
         "full blocked lane left a key held");

   reset();
   input_key_lane_dispatch(&lane, true, RETROK_a, 0, 0,
         RETRO_DEVICE_KEYBOARD, deliver);
   CHECK(downs[RETROK_a] == 1 && held(RETROK_a)
         && lane.head == 0 && input_key_lane_load(&lane.tail) == 0,
         "empty lane did not deliver immediately without queueing");
   if (failures == had)
      fprintf(stderr, "[pass] reader events preserve queued order and the empty-lane fast path\n");
}

static retro_atomic_int_t mixed_done;
static void *mixed_writer(void *data)
{
   unsigned code = RETROK_F1 + (unsigned)(uintptr_t)data, i;
   for (i = 0; i < 5000; i++)
   {
      while (!input_key_lane_push(&lane, true, code, 0, 0, RETRO_DEVICE_KEYBOARD))
         sched_yield();
      input_key_lane_push(&lane, false, code, 0, 0, RETRO_DEVICE_KEYBOARD);
   }
   retro_atomic_fetch_add_int(&mixed_done, 1);
   return NULL;
}

static void lane_mixed_stress(void)
{
   unsigned had = failures, i;
   pthread_t writers[4];
   reset();
   retro_atomic_store_release_int(&mixed_done, 0);
   for (i = 0; i < 4; i++)
      pthread_create(&writers[i], NULL, mixed_writer, (void*)(uintptr_t)i);
   while (input_key_lane_load(&mixed_done) != 4)
   {
      input_key_lane_dispatch(&lane, true, RETROK_F5, 0, 0,
            RETRO_DEVICE_KEYBOARD, deliver);
      input_key_lane_dispatch(&lane, false, RETROK_F5, 0, 0,
            RETRO_DEVICE_KEYBOARD, deliver);
      input_key_lane_take(&lane, deliver);
   }
   for (i = 0; i < 4; i++)
      pthread_join(writers[i], NULL);
   input_key_lane_take(&lane, deliver);
   input_key_lane_dispatch(&lane, false, RETROK_F5, 0, 0,
         RETRO_DEVICE_KEYBOARD, deliver);
   input_key_lane_take(&lane, deliver);
   for (i = 0; i < 5; i++)
      CHECK(!held(RETROK_F1 + i), "mixed queue/direct stress left a key held");
   CHECK(lane.head == input_key_lane_load(&lane.tail),
         "mixed stress left a claimed event undelivered");
   if (failures == had)
      fprintf(stderr, "[pass] four press/release writers and direct reader events leave no keys held\n");
}

static void lane_drain_budget(void)
{
   unsigned had = failures;
   reset();
   reposts_left = INPUT_KEY_LANE_SIZE * 2;
   input_key_lane_push(&lane, true, RETROK_F6, 0, 0, RETRO_DEVICE_KEYBOARD);
   input_key_lane_take(&lane, deliver);
   CHECK(downs[RETROK_F6] == 1 && lane.head == 1
         && input_key_lane_load(&lane.tail) == 2,
         "the drain consumed events posted after its entry watermark");
   while (lane.head != input_key_lane_load(&lane.tail))
      input_key_lane_take(&lane, deliver);
   CHECK(downs[RETROK_F6] == INPUT_KEY_LANE_SIZE * 2 + 1,
         "bounded drains lost events posted during delivery");
   if (failures == had)
      fprintf(stderr, "[pass] a drain stops at its entry watermark without losing later posts\n");
}

int main(void)
{
   lane_stopped_writer();
   lane_newer_press();
   lane_wrap();
   lane_stress();
   lane_stopped_key_release();
   lane_recovery_positions();
   lane_delayed_release();
   lane_direct_order();
   lane_mixed_stress();
   lane_drain_budget();
   if (failures)
   {
      fprintf(stderr, "FAIL key_lane_test: %u failure(s)\n", failures);
      return 1;
   }
   fprintf(stderr, "PASS key_lane_test\n");
   return 0;
}
