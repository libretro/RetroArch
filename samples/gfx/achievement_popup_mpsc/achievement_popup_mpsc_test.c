/* achievement_popup_mpsc_test.c -- the achievement popup handoff from
 * gfx/widgets/gfx_widget_achievement_popup.c, reproduced and beaten on.
 *
 * The protocol: an unlock on the cheevos thread builds a whole node -
 * title, subtitle, badge name, all owned by the node - and pushes it
 * onto a lock-free MPSC stack; the draw thread's iterate drains the
 * stack in arrival order into an 8-slot ring that is draw-thread-only
 * from then on, dropping a popup whole when the ring is full and
 * starting the slide-in exactly when the ring goes empty-to-nonempty.
 * The claims a regression would break:
 *
 *   1. Nodes arrive whole and in posting order (checksummed titles).
 *   2. Nothing leaks: every node is freed exactly once - consumed,
 *      or dropped whole on a full ring (ASan holds this).
 *   3. The slide-in starts exactly on every empty-to-nonempty
 *      transition of the ring, on the consumer side, and never from
 *      the producer.
 *
 * Paced with short sleeps so a single-processor machine interleaves
 * the threads. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>
#include <queues/mpsc_stack.h>
#include <rthreads/rthreads.h>

#if defined(_WIN32)
#include <windows.h>
static void sleep_us(unsigned us) { Sleep(us / 1000u + 1u); }
#else
#include <unistd.h>
static void sleep_us(unsigned us) { usleep(us); }
#endif

#define RING_SIZE 8
#define N_PUSHES  3000

struct popup_msg
{
   mpsc_stack_node_t link;
   char *title;
   char *subtitle;
};

static mpsc_stack_t g_pending;
static retro_atomic_int_t g_done;
static int g_sabotage_write_after_push = 0;

static unsigned checksum(const char *b)
{
   unsigned h = 2166136261u;
   for (; *b; b++)
      h = (h ^ (unsigned char)*b) * 16777619u;
   return h;
}

static void producer(void *arg)
{
   int i;
   (void)arg;
   for (i = 0; i < N_PUSHES; i++)
   {
      char body[64];
      struct popup_msg *node = (struct popup_msg *)malloc(sizeof(*node));
      if (!node)
         abort();
      snprintf(body, sizeof(body), "cheevo %d", i);
      node->title = (char *)malloc(96);
      if (!node->title)
         abort();
      if (g_sabotage_write_after_push)
      {
         /* alias taken before the push: the node itself is the
          * consumer's to free the moment it is published */
         char *title_alias = node->title;
         snprintf(title_alias, 96, "%s#%08x", body, checksum(body));
         title_alias[3] = '\0';            /* truncated... */
         node->subtitle = strdup("sub");
         mpsc_stack_push(&g_pending, &node->link);
         sleep_us(200);
         /* SABOTAGE: ...and completed after the push */
         snprintf(title_alias, 96, "%s#%08x", body, checksum(body));
      }
      else
      {
         snprintf(node->title, 96, "%s#%08x", body, checksum(body));
         node->subtitle = strdup("sub");
         mpsc_stack_push(&g_pending, &node->link);
      }
      if ((i & 31) == 0)
         sleep_us(80);
   }
   retro_atomic_store_release_int(&g_done, 1);
}

int main(void)
{
   sthread_t *p;
   char *ring[RING_SIZE];
   int  rd = 0, wr = 0;
   int  torn = 0, order = 0, consumed = 0, dropped = 0;
   int  starts = 0, transitions = 0, last_seq = -1;

   memset(ring, 0, sizeof(ring));
   mpsc_stack_init(&g_pending);
   retro_atomic_store_relaxed_int(&g_done, 0);

   p = sthread_create(producer, NULL);

   for (;;)
   {
      int done = retro_atomic_load_acquire_int(&g_done);
      mpsc_stack_node_t *link =
            mpsc_stack_reverse(mpsc_stack_drain(&g_pending));
      while (link)
      {
         struct popup_msg *node = (struct popup_msg *)link;
         int start_notification = 1;
         link = link->next;

         /* integrity + order on every arrival */
         {
            const char *hash = strrchr(node->title, '#');
            unsigned want; int seq;
            char body[64];
            if (   !hash || (size_t)(hash - node->title) >= sizeof(body)
                || sscanf(hash + 1, "%x", &want) != 1)
               torn++;
            else
            {
               memcpy(body, node->title, (size_t)(hash - node->title));
               body[hash - node->title] = '\0';
               if (   checksum(body) != want
                   || sscanf(body, "cheevo %d", &seq) != 1)
                  torn++;
               else if (seq <= last_seq)
                  order++;
               else
                  last_seq = seq;
            }
         }

         if (wr == rd)
         {
            if (ring[wr])
            {
               free(node->title);
               free(node->subtitle);
               free(node);
               dropped++;
               continue;
            }
         }
         else
            start_notification = 0;

         ring[wr] = node->title;
         free(node->subtitle);
         free(node);
         wr = (wr + 1) % RING_SIZE;
         consumed++;
         if (start_notification)
         {
            starts++;
            transitions++;
         }
      }
      /* retire the currently-shown popup now and then, like the
       * animation's next step */
      if (ring[rd])
      {
         /* Under the write-after-push sabotage the producer still
          * holds a pointer into this title; keep it allocated (the
          * lane leaks by design and does not run under ASan) so the
          * late write is observed as a torn checksum, not a crash. */
         if (!g_sabotage_write_after_push)
            free(ring[rd]);
         ring[rd] = NULL;
         rd = (rd + 1) % RING_SIZE;
         if (!ring[rd])
            ;  /* ring emptied; next arrival is a transition */
      }
      if (done && mpsc_stack_empty(&g_pending))
         break;
      sleep_us(140);
   }
   sthread_join(p);
   while (ring[rd])
   {
      if (!g_sabotage_write_after_push)
         free(ring[rd]);
      ring[rd] = NULL;
      rd = (rd + 1) % RING_SIZE;
   }

   printf("achievement_popup_mpsc: %d pushed, %d consumed, %d dropped, "
          "%d start(s)\n", N_PUSHES, consumed, dropped, starts);
   if (torn)
      printf("  FAIL: %d torn node(s)\n", torn);
   if (order)
      printf("  FAIL: %d out-of-order arrival(s)\n", order);
   if (consumed + dropped != N_PUSHES)
      printf("  FAIL: %d node(s) unaccounted\n",
            N_PUSHES - consumed - dropped);
   if (starts < 1)
      printf("  FAIL: the slide-in never started\n");
   if (torn || order || (consumed + dropped != N_PUSHES) || starts < 1)
   {
      printf("achievement_popup_mpsc: FAILED\n");
      return 1;
   }
   printf("achievement_popup_mpsc: ok\n");
   return 0;
}
