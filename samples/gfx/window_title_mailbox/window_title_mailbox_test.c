/* window_title_mailbox_test.c -- the three-slot title buffer of
 * gfx/video_driver.c, reproduced and beaten on.
 *
 * The protocol: the main thread hands the window title to the video
 * thread through three inline slots and one atomic word holding
 * (slot + 1), or 0 for empty - the frame path allocates and copies
 * nothing. The producer assembles the title straight into a free
 * slot, records its length and exchanges its index in; the consumer
 * exchanges the word to 0 and is lent the slot itself, which it
 * holds until it takes again. The producer's exchange return decides
 * its next slot: nonzero is the untaken previous publish (safe to
 * reuse - the consumer cannot hold it), zero means the consumer took
 * the previous publish and holds that slot, so the producer writes
 * the remaining third. The claims a regression would break:
 *
 *   1. No torn lend: every title the consumer is lent is internally
 *      consistent (its embedded checksum and recorded length hold)
 *      for as long as it holds it - including a hold kept across
 *      many publishes, which is what the third slot exists for. Two
 *      slots pass the fast lane and fail exactly that hold.
 *   2. Supersession only forward: the sequence numbers the consumer
 *      observes strictly increase, and the last published title is
 *      lent once the producer stops.
 *   3. The held lend: with nothing new, a take asking for the held
 *      title gets the one lent last, and a plain take gets nothing.
 *   4. Nothing on the heap: publish and take are one atomic word
 *      each; there is nothing to leak.
 *
 * The harness reproduces the protocol rather than including
 * video_driver.c (which drags the whole frontend in); the shape is a
 * few dozen lines (video_title_publish() and
 * video_driver_lend_window_title()) and drift is caught by eye at
 * those sites.
 *
 * Paced with short sleeps so a single-processor machine interleaves
 * the two threads; the protocol has no spin anywhere. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
static size_t             g_slot_len[3];
static retro_atomic_int_t g_slot;      /* (slot + 1), 0 = empty */
static unsigned           g_next_slot; /* producer-owned */
static unsigned           g_last_slot; /* producer-owned */
static int                g_held;      /* consumer-owned, (slot + 1) */
static retro_atomic_int_t g_stop;

static size_t title_make(char *s, size_t len, int seq);

/* video_title_publish(), with the assembly into the scratch slot */
static void title_publish(int seq)
{
   unsigned pub = g_next_slot;
   int prev;

   g_slot_len[pub] = title_make(g_slot_buf[pub], sizeof(g_slot_buf[pub]),
         seq);
   prev = retro_atomic_exchange_int(&g_slot, (int)(pub + 1u));

   if (prev != 0)
      g_next_slot = (unsigned)(prev - 1);
   else
      g_next_slot = (TITLE_SLOTS == 3)
            ? 3u - pub - g_last_slot
            : (pub ^ 1u);   /* the two-slot variant, for the red check */
   g_last_slot = pub;
}

/* video_driver_lend_window_title(): the new title, or with @held the
 * one lent last; NULL when there is none. Valid until the next call. */
static const char *title_lend(size_t *len, int held)
{
   int published = 0;
   if (retro_atomic_load_acquire_int(&g_slot))
      published = retro_atomic_exchange_int(&g_slot, 0);
   if (published)
      g_held = published;
   else if (!held || !g_held)
      return NULL;
   if (len)
      *len = g_slot_len[g_held - 1];
   return g_slot_buf[g_held - 1];
}

static void title_reset(void)
{
   retro_atomic_store_relaxed_int(&g_slot, 0);
   g_next_slot = 0;
   g_last_slot = 1;
   g_held      = 0;
   retro_atomic_store_relaxed_int(&g_stop, 0);
}

static unsigned title_checksum(const char *body)
{
   unsigned h = 2166136261u;
   for (; *body; body++)
      h = (h ^ (unsigned char)*body) * 16777619u;
   return h;
}

static size_t title_make(char *s, size_t len, int seq)
{
   char body[64];
   snprintf(body, sizeof(body), "core 1.0 || seq %d fps 60.0", seq);
   return (size_t)snprintf(s, len, "%s#%08x", body, title_checksum(body));
}

/* Returns the sequence number, or -1 on a torn/corrupt title or one
 * whose recorded length is not its own. */
static int title_check(const char *s, size_t len)
{
   char body[64];
   unsigned want;
   int seq;
   const char *hash = strrchr(s, '#');
   if (strlen(s) != len)
      return -1;
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
   (void)arg;
   for (i = 0; i < N_TITLES; i++)
   {
      title_publish(i);
      if ((i & 63) == 0)
         sleep_us(100);
   }
   retro_atomic_store_release_int(&g_stop, 1);
}

/* Fast lane: producer races a prompt consumer; checksum, order,
 * final-title and held-lend claims. */
static int lane_fast(void)
{
   sthread_t *p;
   size_t len;
   const char *t;
   int last_seq = -1, taken = 0, torn = 0, order = 0, held_bad = 0;

   title_reset();
   p = sthread_create(producer, NULL);

   for (;;)
   {
      int stopped = retro_atomic_load_acquire_int(&g_stop);
      if ((t = title_lend(&len, 0)))
      {
         int seq = title_check(t, len);
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

   /* Nothing new: a plain take gets nothing, a held one the last */
   if (title_lend(NULL, 0))
      held_bad++;
   t = title_lend(&len, 1);
   if (!t || title_check(t, len) != N_TITLES - 1)
      held_bad++;

   printf("window_title_mailbox fast lane: %d published, %d taken, "
          "last seq %d\n", N_TITLES, taken, last_seq);
   if (held_bad)
      printf("  FAIL: the held lend is not the last title lent\n");
   if (torn)
      printf("  FAIL: %d torn/corrupt title(s) taken\n", torn);
   if (order)
      printf("  FAIL: %d title(s) out of order\n", order);
   if (last_seq != N_TITLES - 1)
      printf("  FAIL: final title lost (last seq %d, want %d)\n",
            last_seq, N_TITLES - 1);
   return (torn || order || held_bad || last_seq != N_TITLES - 1) ? 1 : 0;
}

/* Hold lane: the consumer is lent a slot and keeps the lend while
 * the producer publishes hundreds of titles, reading it again over
 * and over, before it takes again. The held slot's content must not
 * move under the hold - that is precisely the guarantee the third
 * slot buys. With TITLE_SLOTS=2 the producer laps into the held slot
 * and this lane reports torn holds. */
static int lane_stall(void)
{
   sthread_t *p;
   int torn = 0, held_takes = 0;

   title_reset();
   p = sthread_create(producer, NULL);

   for (;;)
   {
      size_t len;
      int stopped   = retro_atomic_load_acquire_int(&g_stop);
      const char *t = title_lend(&len, 0);
      if (t)
      {
         int i, bad = 0;
         held_takes++;
         for (i = 0; i < 32; i++)
         {
            if (title_check(t, len) < 0)
               bad = 1;
            sleep_us(200);
         }
         torn += bad;
      }
      else if (stopped)
         break;
      sleep_us(50);
   }
   sthread_join(p);

   printf("window_title_mailbox hold lane: %d long hold(s), "
          "%d torn\n", held_takes, torn);
   if (!held_takes)
      printf("  FAIL: the hold lane never held a slot\n");
   if (torn)
      printf("  FAIL: %d hold(s) torn while held - the held slot "
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
