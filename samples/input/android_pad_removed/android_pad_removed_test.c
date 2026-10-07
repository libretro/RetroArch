/* android_pad_removed_test.c -- a controller Android has removed, from
 * the listener that hears of it to the port it comes back to.
 *
 * Android tells the activity when an input device goes away. Nothing
 * passed that on, and no input event marks a removal, so a pad that
 * was switched off stayed connected for as long as the app ran. The
 * listener now posts the device's id into a mailbox, the input thread
 * takes it out at its next poll and marks the pad's slot as removed,
 * and a device that returns is given the slot it left.
 *
 * The claims, for the mailbox:
 *
 *   1. No removal is lost and none is delivered twice, with several
 *      threads posting while the input thread polls.
 *   2. A full box refuses an id and keeps the ones it holds.
 *   3. Ids that are not a physical device's are refused.
 *   4. With nothing posted the poll takes nothing, and an id posted
 *      after a poll is taken by the next one.
 *
 * And for the pad table:
 *
 *   5. A removed pad's slot keeps its name, and the device gets that
 *      slot - its old port - back when it returns.
 *   6. Of two identical controllers, the one that was removed is the
 *      one whose slot a returning device takes; the one still
 *      connected is not disturbed.
 *   7. Removing an id no pad has changes nothing, and a device that
 *      never left has no slot to return to.
 *
 * Both are the driver's own, from android_pad_removed.h. The threads
 * around them - the listener and the polling input thread - are
 * reproduced here, because the driver only builds against the NDK.
 * A sabotage mode empties a removed slot's name, as the first version
 * of the change did, and is asserted to be caught: with the name gone
 * a returning pad finds no slot and would be given the next free port
 * on every reconnect.
 *
 * Paced with short sleeps so a single-processor machine interleaves
 * the threads. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <compat/strl.h>
#include <rthreads/rthreads.h>

#if defined(_WIN32)
#include <windows.h>
static void sleep_us(unsigned us) { Sleep(us / 1000u + 1u); }
#else
#include <unistd.h>
static void sleep_us(unsigned us) { usleep(us); }
#endif

#include "android_pad_removed.h"

static unsigned failures = 0;
static bool     quiet    = false;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         if (!quiet) \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

/* ------------------------------------------------------------------ */
/* The mailbox                                                        */
/* ------------------------------------------------------------------ */

#define N_PRODUCERS   3
#define IDS_PER_THREAD 2000
#define N_IDS         (N_PRODUCERS * IDS_PER_THREAD)

static android_removed_box_t g_box;
static retro_atomic_int_t    g_producers_done;

/* How often each id was delivered; written by the consumer alone. */
static unsigned char g_delivered[N_IDS];

struct producer
{
   unsigned first_id;
   unsigned posted;   /* accepted by the box */
   unsigned refused;  /* the box was full; retried */
};

/* The listener: one removal after another, each posted until the box
 * takes it. A real listener gives up on a full box; retrying here is
 * what lets the test demand that every id arrives. */
static void producer_thread(void *arg)
{
   struct producer *p = (struct producer*)arg;
   unsigned i;

   for (i = 0; i < IDS_PER_THREAD; i++)
   {
      while (!android_removed_post(&g_box, (int)(p->first_id + i)))
      {
         p->refused++;
         sleep_us(50);
      }
      p->posted++;
      if ((i & 63) == 0)
         sleep_us(20);
   }

   retro_atomic_fetch_add_int(&g_producers_done, 1);
}

static void lane_mailbox_concurrent(void)
{
   unsigned had   = failures;
   unsigned taken = 0;
   unsigned i;
   sthread_t *threads[N_PRODUCERS];
   struct producer prod[N_PRODUCERS];
   int ids[ANDROID_REMOVED_SLOTS];
   bool lost = false, twice = false, unknown = false;

   memset(&g_box, 0, sizeof(g_box));
   memset(g_delivered, 0, sizeof(g_delivered));
   retro_atomic_store_release_int(&g_producers_done, 0);

   for (i = 0; i < N_PRODUCERS; i++)
   {
      prod[i].first_id = i * IDS_PER_THREAD;
      prod[i].posted   = 0;
      prod[i].refused  = 0;
      threads[i]       = sthread_create(producer_thread, &prod[i]);
   }

   /* The input thread's poll. One more round after the producers are
    * done picks up whatever they left behind the last one. */
   for (;;)
   {
      bool done      = retro_atomic_load_acquire_int(&g_producers_done)
                       == N_PRODUCERS;
      unsigned count = android_removed_take(&g_box, ids);

      for (i = 0; i < count; i++)
      {
         if (ids[i] < 0 || ids[i] >= N_IDS)
            unknown = true;
         else if (g_delivered[ids[i]]++)
            twice = true;
         taken++;
      }

      if (done && !count)
         break;
      if (!count)
         sleep_us(30);
   }

   for (i = 0; i < N_PRODUCERS; i++)
      sthread_join(threads[i]);

   /* Anything a producer posted between the last take and its exit. */
   {
      unsigned count = android_removed_take(&g_box, ids);
      for (i = 0; i < count; i++)
      {
         if (ids[i] < 0 || ids[i] >= N_IDS)
            unknown = true;
         else if (g_delivered[ids[i]]++)
            twice = true;
         taken++;
      }
   }

   for (i = 0; i < N_IDS; i++)
      if (!g_delivered[i])
         lost = true;

   CHECK(!unknown, "the poll took an id nobody posted");
   CHECK(!twice,   "a removal was delivered twice");
   CHECK(!lost,    "a removal was lost");
   CHECK(taken == N_IDS, "the poll did not take as many ids as were posted");

   if (failures == had && !quiet)
      fprintf(stderr, "[pass] %u removals from %u threads, each delivered"
            " once (%u refusals on a full box)\n",
            taken, (unsigned)N_PRODUCERS,
            prod[0].refused + prod[1].refused + prod[2].refused);
}

static void lane_mailbox_rules(void)
{
   unsigned had = failures;
   int ids[ANDROID_REMOVED_SLOTS];
   unsigned i, count;
   android_removed_box_t box;

   memset(&box, 0, sizeof(box));

   /* 4: nothing posted, nothing taken. */
   CHECK(android_removed_take(&box, ids) == 0,
         "the poll took something from an empty box");

   /* 3: not a physical device. */
   CHECK(!android_removed_post(&box, -1),
         "the virtual keyboard's id (-1) was accepted");
   CHECK(!android_removed_post(&box, INT_MAX), "INT_MAX was accepted");
   CHECK(android_removed_take(&box, ids) == 0,
         "a refused id was delivered");

   /* Id 0 is a real id and must survive the plus-one encoding. */
   CHECK(android_removed_post(&box, 0), "id 0 was refused");
   count = android_removed_take(&box, ids);
   CHECK(count == 1 && ids[0] == 0, "id 0 did not come back as 0");

   /* 2: a full box. */
   for (i = 0; i < ANDROID_REMOVED_SLOTS; i++)
      CHECK(android_removed_post(&box, (int)(100 + i)),
            "the box refused an id before it was full");
   CHECK(!android_removed_post(&box, 999), "a full box accepted an id");
   count = android_removed_take(&box, ids);
   CHECK(count == ANDROID_REMOVED_SLOTS,
         "a full box did not hand over every id it held");
   for (i = 0; i < count; i++)
      CHECK(ids[i] >= 100 && ids[i] < 100 + ANDROID_REMOVED_SLOTS,
            "a full box handed over an id it had refused");

   /* 4: posted after a poll, taken by the next. */
   CHECK(android_removed_take(&box, ids) == 0,
         "the box was not empty after being emptied");
   CHECK(android_removed_post(&box, 7), "an emptied box refused an id");
   count = android_removed_take(&box, ids);
   CHECK(count == 1 && ids[0] == 7,
         "an id posted after a poll was not taken by the next");

   if (failures == had && !quiet)
      fprintf(stderr, "[pass] the mailbox refuses, fills and empties"
            " as it should\n");
}

/* ------------------------------------------------------------------ */
/* The pad table                                                      */
/* ------------------------------------------------------------------ */

/* What the first version of the change did: empty the slot. */
static bool sabotage_empty_name = false;

/* android_input_pad_removed() in the driver, as far as the table goes. */
static int pad_removed(state_device_t *pads, unsigned count, int id)
{
   int port = android_pad_mark_removed(pads, count, id);
   if (port >= 0 && sabotage_empty_name)
      pads[port].name[0] = '\0';
   return port;
}

/* android_input_recover_port(), as far as the table goes: the port a
 * device arriving as @id under @name is given, or -1 for "a new one". */
static int pad_returned(state_device_t *pads, unsigned count,
      int id, const char *name)
{
   int port = android_pad_find_removed(pads, count, name);
   if (port >= 0)
      pads[port].id = id;
   return port;
}

static void add_pad(state_device_t *pads, unsigned *count,
      int id, const char *name)
{
   pads[*count].id   = id;
   pads[*count].port = (int)*count;
   strlcpy(pads[*count].name, name, sizeof(pads[*count].name));
   (*count)++;
}

static void lane_pad_table(void)
{
   unsigned had   = failures;
   unsigned count = 0;
   state_device_t pads[8];

   memset(pads, 0, sizeof(pads));
   add_pad(pads, &count, 11, "Xbox Wireless Controller");
   add_pad(pads, &count, 12, "8BitDo Pro 2");
   add_pad(pads, &count, 13, "Xbox Wireless Controller");

   /* 7 */
   CHECK(pad_removed(pads, count, 99) == -1,
         "removing an id no pad has found a pad");
   CHECK(pads[0].id == 11 && pads[1].id == 12 && pads[2].id == 13,
         "removing an id no pad has changed the table");
   CHECK(pad_returned(pads, count, 50, "8BitDo Pro 2") == -1,
         "a pad that never left was given a slot to return to");

   /* 5: port 1 goes away and comes back under a new id. */
   CHECK(pad_removed(pads, count, 12) == 1,
         "the removed pad was not found on its port");
   CHECK(pads[1].id == ANDROID_PAD_ID_REMOVED,
         "the removed pad's slot still carries its id");
   CHECK(pad_returned(pads, count, 40, "8BitDo Pro 2") == 1,
         "a returning pad did not get its old port back");
   CHECK(pads[1].id == 40, "the returning pad's new id was not recorded");

   /* 6: the second of two identical pads goes away and comes back. */
   CHECK(pad_removed(pads, count, 13) == 2,
         "the second identical pad was not found on its port");
   CHECK(pad_returned(pads, count, 41, "Xbox Wireless Controller") == 2,
         "a returning pad was taken for its connected twin");
   CHECK(pads[0].id == 11,
         "the pad that stayed connected was disturbed");

   /* Nothing is marked removed any more: a third identical pad is new. */
   CHECK(pad_returned(pads, count, 42, "Xbox Wireless Controller") == -1,
         "a new identical pad was given a connected pad's slot");

   /* A removed slot is not found by another device's name. */
   CHECK(pad_removed(pads, count, 11) == 0, "port 0 was not found");
   CHECK(pad_returned(pads, count, 43, "DualSense") == -1,
         "a different device was given a removed pad's slot");
   CHECK(pad_returned(pads, count, 44, "Xbox Wireless Controller") == 0,
         "port 0 was not given back");

   if (failures == had && !quiet)
      fprintf(stderr, "[pass] a returning pad gets its old port back\n");
}

int main(void)
{
   lane_mailbox_rules();
   lane_mailbox_concurrent();
   lane_pad_table();

   if (failures)
   {
      fprintf(stderr, "FAIL android_pad_removed_test: %u failures\n",
            failures);
      return 1;
   }

   /* Emptying the slot's name has to be caught. */
   quiet               = true;
   sabotage_empty_name = true;
   lane_pad_table();
   quiet               = false;
   sabotage_empty_name = false;

   if (!failures)
   {
      fprintf(stderr, "FAIL android_pad_removed_test: emptying a removed"
            " slot's name went unnoticed\n");
      return 1;
   }

   fprintf(stderr, "PASS android_pad_removed_test (sabotage caught by"
         " %u checks)\n", failures);
   return 0;
}
