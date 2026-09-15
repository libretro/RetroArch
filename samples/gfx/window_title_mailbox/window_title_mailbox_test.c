/* window_title_mailbox_test.c -- the one-slot atomic title mailbox of
 * gfx/video_driver.c, reproduced and beaten on.
 *
 * The protocol: the main thread hands the window title to the video
 * thread as an immutable heap copy through one retro_atomic_ptr_t
 * slot.  Publish is exchange-in (freeing whatever was never taken),
 * take is exchange-out.  The claims a regression would break:
 *
 *   1. No torn copy: every title the consumer takes is internally
 *      consistent (its embedded checksum holds), because a published
 *      copy is never written again.
 *   2. No leak and no double free: every copy is freed exactly once,
 *      by whichever side superseded or took it (ASan holds this).
 *   3. Supersession only forward: the sequence numbers the consumer
 *      observes strictly increase, and the last published title is
 *      taken once the producer stops.
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

static retro_atomic_ptr_t g_pending;
static retro_atomic_int_t g_stop;

static unsigned title_checksum(const char *body)
{
   unsigned h = 2166136261u;
   for (; *body; body++)
      h = (h ^ (unsigned char)*body) * 16777619u;
   return h;
}

static char *title_make(int seq)
{
   char body[64], *s = (char*)malloc(96);
   if (!s)
      abort();
   snprintf(body, sizeof(body), "core 1.0 || seq %d fps 60.0", seq);
   snprintf(s, 96, "%s#%08x", body, title_checksum(body));
   return s;
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
   (void)arg;
   for (i = 0; i < N_TITLES; i++)
   {
      char *copy  = title_make(i);
      char *stale = (char*)retro_atomic_exchange_ptr(&g_pending, copy);
      if (stale)
         free(stale);
      if ((i & 63) == 0)
         sleep_us(100);
   }
   retro_atomic_store_release_int(&g_stop, 1);
}

int main(void)
{
   sthread_t *p;
   int last_seq = -1, taken = 0, torn = 0, order = 0;

   retro_atomic_ptr_init(&g_pending, NULL);
   retro_atomic_store_relaxed_int(&g_stop, 0);

   p = sthread_create(producer, NULL);

   for (;;)
   {
      int stopped = retro_atomic_load_acquire_int(&g_stop);
      if (retro_atomic_load_acquire_ptr(&g_pending))
      {
         char *t = (char*)retro_atomic_exchange_ptr(&g_pending, NULL);
         if (t)
         {
            int seq = title_check(t);
            if (seq < 0)
               torn++;
            else if (seq <= last_seq)
               order++;
            else
               last_seq = seq;
            free(t);
            taken++;
         }
      }
      else if (stopped)
         break;
      sleep_us(50);
   }
   sthread_join(p);

   printf("window_title_mailbox: %d published, %d taken, last seq %d\n",
         N_TITLES, taken, last_seq);
   if (torn)
      printf("  FAIL: %d torn/corrupt title(s) taken\n", torn);
   if (order)
      printf("  FAIL: %d title(s) out of order\n", order);
   if (last_seq != N_TITLES - 1)
      printf("  FAIL: final title lost (last seq %d, want %d)\n",
            last_seq, N_TITLES - 1);
   if (torn || order || last_seq != N_TITLES - 1)
   {
      printf("window_title_mailbox: FAILED\n");
      return 1;
   }
   printf("window_title_mailbox: ok\n");
   return 0;
}
#endif /* RETRO_ATOMIC_HAS_PTR */
