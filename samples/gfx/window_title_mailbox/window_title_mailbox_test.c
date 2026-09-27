/* window_title_mailbox_test.c -- the three-slot title buffer of
 * gfx/video_driver.c, reproduced and beaten on.
 *
 * The protocol: the main thread hands the window title to the video
 * thread through three inline slots and one atomic word holding
 * (slot + 1), or 0 for empty - the frame path allocates nothing.
 * Publish fills a free slot and exchanges its index in; take
 * exchanges the word to 0 and copies the slot out. The producer's
 * exchange return decides its next slot: nonzero is the untaken
 * previous publish (safe to reuse - no consumer can hold it), zero
 * means the consumer took the previous publish and may still be
 * copying that slot, so the producer writes the remaining third.
 * The claims a regression would break:
 *
 *   1. No torn copy: every title the consumer takes is internally
 *      consistent (its embedded checksum holds) - including a take
 *      whose copy is stalled across many publishes, which is what
 *      the third slot exists for. Two slots pass the fast lanes and
 *      fail exactly that stall.
 *   2. Supersession only forward: the sequence numbers the consumer
 *      observes strictly increase, and the last published title is
 *      taken once the producer stops.
 *   3. Nothing on the heap: publish and take are strlcpy plus one
 *      atomic word; there is nothing to leak.
 *
 * The harness reproduces the protocol rather than including
 * video_driver.c (which drags the whole frontend in); the shape is a
 * dozen lines and drift is caught by eye at the publish/take sites.
 *
 * Paced with short sleeps so a single-processor machine interleaves
 * the two threads; the protocol has no spin anywhere. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <compat/strl.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#ifndef RETRO_ATOMIC_HAS_PTR
int main(void)
{
   printf("window_title_mailbox: no pointer atomics on this backend; "
          "the lock protocol stands and this test does not apply\n");
   return 0;
}
#else

#if defined(_WIN32)
#include <windows.h>
static void sleep_us(unsigned us) { Sleep(us / 1000u + 1u); }
#else
#include <unistd.h>
static void sleep_us(unsigned us) { usleep(us); }
#endif

#define N_TITLES 20000

/* The protocol under test, byte for byte the video_driver.c shape.
 * TITLE_SLOTS is 3 in the shipping code; the stall lane demonstrates
 * why (build the model with -DTITLE_SLOTS=2 and it tears). */
#ifndef TITLE_SLOTS
#define TITLE_SLOTS 3
#endif

static char               g_slot_buf[3][96];
static retro_atomic_int_t g_slot;      /* (slot + 1), 0 = empty */
static unsigned           g_next_slot; /* producer-owned */
static unsigned           g_last_slot; /* producer-owned */
static retro_atomic_int_t g_stop;

static void title_publish(const char *title)
{
   unsigned pub = g_next_slot;
   int prev;

   strlcpy(g_slot_buf[pub], title, sizeof(g_slot_buf[pub]));
   prev = retro_atomic_exchange_int(&g_slot, (int)(pub + 1u));

   if (prev != 0)
      g_next_slot = (unsigned)(prev - 1);
   else
      g_next_slot = (TITLE_SLOTS == 3)
            ? 3u - pub - g_last_slot
            : (pub ^ 1u);   /* the two-slot variant, for the red check */
   g_last_slot = pub;
}

/* Take: returns the slot index the caller now holds, or -1. The copy
 * out of the slot is the caller's - the stall lane does it slowly on
 * purpose. */
static int title_take_begin(void)
{
   int published;
   if (!retro_atomic_load_acquire_int(&g_slot))
      return -1;
   published = retro_atomic_exchange_int(&g_slot, 0);
   return published ? (published - 1) : -1;
}

static unsigned title_checksum(const char *body)
{
   unsigned h = 2166136261u;
   for (; *body; body++)
      h = (h ^ (unsigned char)*body) * 16777619u;
   return h;
}

static void title_make(char *s, size_t len, int seq)
{
   char body[64];
   snprintf(body, sizeof(body), "core 1.0 || seq %d fps 60.0", seq);
   snprintf(s, len, "%s#%08x", body, title_checksum(body));
}

/* Returns the sequence number, or -1 on a torn/corrupt title. */
static int title_check(const char *s)
{
   char body[64];
   unsigned want;
   int seq;
   const char *hash = strrchr(s, '#');
   if (!hash || (size_t)(hash - s) >= sizeof(body))
      return -1;
   memcpy(body, s, (size_t)(hash - s));
   body[hash - s] = '\0';
   if (sscanf(hash + 1, "%x", &want) != 1)
      return -1;
   if (title_checksum(body) != want)
      return -1;
   if (sscanf(body, "core 1.0 || seq %d", &seq) != 1)
      return -1;
   return seq;
}

static void producer(void *arg)
{
   int i;
   char title[96];
   (void)arg;
   for (i = 0; i < N_TITLES; i++)
   {
      title_make(title, sizeof(title), i);
      title_publish(title);
      if ((i & 63) == 0)
         sleep_us(100);
   }
   retro_atomic_store_release_int(&g_stop, 1);
}

/* Fast lane: producer races a prompt consumer; checksum, order and
 * final-title claims. */
static int lane_fast(void)
{
   sthread_t *p;
   int last_seq = -1, taken = 0, torn = 0, order = 0;

   retro_atomic_store_relaxed_int(&g_slot, 0);
   g_next_slot = 0;
   g_last_slot = 1;
   retro_atomic_store_relaxed_int(&g_stop, 0);

   p = sthread_create(producer, NULL);

   for (;;)
   {
      int stopped = retro_atomic_load_acquire_int(&g_stop);
      int slot    = title_take_begin();
      if (slot >= 0)
      {
         char t[96];
         int seq;
         strlcpy(t, g_slot_buf[slot], sizeof(t));
         seq = title_check(t);
         if (seq < 0)
            torn++;
         else if (seq <= last_seq)
            order++;
         else
            last_seq = seq;
         taken++;
      }
      else if (stopped)
         break;
      sleep_us(50);
   }
   sthread_join(p);

   printf("window_title_mailbox fast lane: %d published, %d taken, "
          "last seq %d\n", N_TITLES, taken, last_seq);
   if (torn)
      printf("  FAIL: %d torn/corrupt title(s) taken\n", torn);
   if (order)
      printf("  FAIL: %d title(s) out of order\n", order);
   if (last_seq != N_TITLES - 1)
      printf("  FAIL: final title lost (last seq %d, want %d)\n",
            last_seq, N_TITLES - 1);
   return (torn || order || last_seq != N_TITLES - 1) ? 1 : 0;
}

/* Stall lane: the consumer exchanges a slot out and then copies it
 * one byte at a time while the producer publishes hundreds of
 * titles. The held slot's content must not move under the copy -
 * that is precisely the guarantee the third slot buys. With
 * TITLE_SLOTS=2 the producer laps into the held slot and this lane
 * reports torn holds. */
static int lane_stall(void)
{
   sthread_t *p;
   int reps, torn = 0, held_takes = 0;

   retro_atomic_store_relaxed_int(&g_slot, 0);
   g_next_slot = 0;
   g_last_slot = 1;
   retro_atomic_store_relaxed_int(&g_stop, 0);

   p = sthread_create(producer, NULL);

   for (reps = 0; ; reps++)
   {
      int stopped = retro_atomic_load_acquire_int(&g_stop);
      int slot    = title_take_begin();
      if (slot >= 0)
      {
         /* Byte-at-a-time copy with sleeps: hundreds of publishes
          * land while this hold is live. */
         char t[96];
         size_t i, len = strlen(g_slot_buf[slot]);
         if (len >= sizeof(t))
            len = sizeof(t) - 1;
         for (i = 0; i < len; i++)
         {
            t[i] = g_slot_buf[slot][i];
            if ((i & 7) == 0)
               sleep_us(200);
         }
         t[len] = '\0';
         held_takes++;
         if (title_check(t) < 0)
            torn++;
      }
      else if (stopped)
         break;
      sleep_us(50);
   }
   sthread_join(p);

   printf("window_title_mailbox stall lane: %d stalled hold(s), "
          "%d torn\n", held_takes, torn);
   if (!held_takes)
      printf("  FAIL: the stall lane never held a slot\n");
   if (torn)
      printf("  FAIL: %d hold(s) torn while stalled - the held slot "
             "was rewritten\n", torn);
   return (torn || !held_takes) ? 1 : 0;
}

int main(void)
{
   int bad = 0;
   bad += lane_fast();
   bad += lane_stall();
   if (bad)
   {
      printf("window_title_mailbox: FAILED\n");
      return 1;
   }
   printf("window_title_mailbox: ok (%d slots)\n", TITLE_SLOTS);
   return 0;
}
#endif /* RETRO_ATOMIC_HAS_PTR */
