/* core_status_defer_test.c -- the SET_MESSAGE_EXT STATUS deferral of
 * runloop.c, reproduced and beaten on.
 *
 * The protocol: core_status_msg is main-thread state with no lock. A
 * rogue core calling SET_MESSAGE_EXT(STATUS) off the main thread has
 * its update captured whole - message copy, priority, duration - into
 * a node on the runloop's lock-free MPSC deferral stack, and the main
 * thread's drain applies it at the top of the next iterate under the
 * same overwrite rule the direct path uses. The claims a regression
 * would break:
 *
 *   1. Nothing is lost and nothing tears: every posted update is
 *      applied exactly once, whole (checksummed), in posting order -
 *      the stack chains newest-first and the drain replays reversed.
 *   2. The drain's rule matches the direct path's, byte for byte in
 *      effect: an oracle applies the direct path's rule to the same
 *      sequence and the final status must agree - including empty
 *      messages, which clear the slot, and including the guard,
 *      which now bites: a held status keeps its priority for its
 *      lifetime, and lower-priority updates and clears bounce.
 *   3. Every node and copy is freed exactly once (ASan holds this).
 *
 * Paced with short sleeps so a single-processor machine interleaves
 * the threads; the protocol has no spin anywhere. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>
#include <queues/mpsc_stack.h>
#include <compat/strl.h>
#include <rthreads/rthreads.h>

#if defined(_WIN32)
#include <windows.h>
static void sleep_us(unsigned us) { Sleep(us / 1000u + 1u); }
#else
#include <unistd.h>
static void sleep_us(unsigned us) { usleep(us); }
#endif

#define N_POSTS 4000

struct status_node
{
   mpsc_stack_node_t link;
   char *msg;
   unsigned prio;
   unsigned duration;
};

struct status_slot
{
   char str[128];
   unsigned priority;
   float duration;
   int set;
};

static mpsc_stack_t g_defer;
static retro_atomic_int_t g_done;

/* The rule, exactly as runloop.c applies it on both paths. */
static void status_apply(struct status_slot *s, const char *msg,
      unsigned prio, unsigned duration)
{
   if (!s->set || s->priority <= prio)
   {
      if (msg && *msg)
      {
         strlcpy(s->str, msg, sizeof(s->str));
         s->priority = prio;
         s->duration = (float)duration;
         s->set      = 1;
      }
      else
      {
         s->str[0]   = '\0';
         s->priority = 0;
         s->duration = 0.0f;
         s->set      = 0;
      }
   }
}

static unsigned checksum(const char *b)
{
   unsigned h = 2166136261u;
   for (; *b; b++)
      h = (h ^ (unsigned char)*b) * 16777619u;
   return h;
}

static char *post_make(int seq, unsigned prio)
{
   char body[64], *s;
   if ((seq % 7) == 3)          /* scripted clears */
      return strdup("");
   s = (char*)malloc(96);
   if (!s)
      abort();
   snprintf(body, sizeof(body), "status seq %d prio %u", seq, prio);
   snprintf(s, 96, "%s#%08x", body, checksum(body));
   return s;
}

static void producer(void *arg)
{
   int i;
   (void)arg;
   for (i = 0; i < N_POSTS; i++)
   {
      struct status_node *n = (struct status_node *)malloc(sizeof(*n));
      if (!n)
         abort();
      n->prio     = (unsigned)(i * 2654435761u) % 5u;
      n->duration = 60;
      n->msg      = post_make(i, n->prio);
      mpsc_stack_push(&g_defer, &n->link);
      if ((i & 31) == 0)
         sleep_us(80);
   }
   retro_atomic_store_release_int(&g_done, 1);
}

int main(void)
{
   sthread_t *p;
   struct status_slot slot, oracle;
   int applied = 0, torn = 0, order = 0, last_seq = -1, i;

   memset(&slot, 0, sizeof(slot));
   memset(&oracle, 0, sizeof(oracle));
   mpsc_stack_init(&g_defer);
   retro_atomic_store_relaxed_int(&g_done, 0);

   p = sthread_create(producer, NULL);

   for (;;)
   {
      int done = retro_atomic_load_acquire_int(&g_done);
      mpsc_stack_node_t *link =
            mpsc_stack_reverse(mpsc_stack_drain(&g_defer));
      while (link)
      {
         struct status_node *n = (struct status_node *)link;
         link = link->next;
         if (n->msg && *n->msg)
         {
            char body[128];
            unsigned want; int seq; unsigned prio;
            const char *hash = strrchr(n->msg, '#');
            if (   !hash
                || (size_t)(hash - n->msg) >= sizeof(body))
               torn++;
            else
            {
               memcpy(body, n->msg, (size_t)(hash - n->msg));
               body[hash - n->msg] = '\0';
               if (   sscanf(hash + 1, "%x", &want) != 1
                   || checksum(body) != want
                   || sscanf(body, "status seq %d prio %u",
                         &seq, &prio) != 2
                   || prio != n->prio)
                  torn++;
               else if (seq <= last_seq)
                  order++;
               else
                  last_seq = seq;
            }
         }
         status_apply(&slot, n->msg, n->prio, n->duration);
         applied++;
         free(n->msg);
         free(n);
      }
      if (done && mpsc_stack_empty(&g_defer))
         break;
      sleep_us(150);   /* a frame */
   }
   sthread_join(p);

   /* the oracle replays the same script through the direct rule */
   for (i = 0; i < N_POSTS; i++)
   {
      unsigned prio = (unsigned)(i * 2654435761u) % 5u;
      char *m = post_make(i, prio);
      status_apply(&oracle, m, prio, 60);
      free(m);
   }

   /* The guard, by hand: scripted updates whose outcome under a
    * biting guard differs from admit-everything at every step. */
   {
      struct status_slot g;
      int guard_ok;
      memset(&g, 0, sizeof(g));
      status_apply(&g, "urgent", 3, 60);    /* lands on empty       */
      status_apply(&g, "chatter", 1, 60);   /* bounces: 1 < 3       */
      status_apply(&g, "", 1, 60);          /* low clear bounces    */
      guard_ok =    g.set && g.priority == 3
                 && !strcmp(g.str, "urgent");
      status_apply(&g, "", 3, 60);          /* equal clear lands    */
      guard_ok = guard_ok && !g.set && g.priority == 0;
      status_apply(&g, "quiet", 0, 60);     /* lands on empty       */
      guard_ok = guard_ok && g.set && !strcmp(g.str, "quiet");
      if (!guard_ok)
      {
         printf("core_status_defer: FAILED (the priority guard "
                "does not bite as specified)\n");
         return 1;
      }
   }

   printf("core_status_defer: %d posted, %d applied, last seq %d\n",
         N_POSTS, applied, last_seq);
   if (torn)
      printf("  FAIL: %d torn/corrupt update(s)\n", torn);
   if (order)
      printf("  FAIL: %d update(s) out of order\n", order);
   if (applied != N_POSTS)
      printf("  FAIL: applied %d of %d\n", applied, N_POSTS);
   if (   strcmp(slot.str, oracle.str)
       || slot.set != oracle.set
       || slot.priority != oracle.priority)
      printf("  FAIL: final status diverges from the direct rule "
            "(\"%s\" set=%d vs \"%s\" set=%d)\n",
            slot.str, slot.set, oracle.str, oracle.set);
   if (   torn || order || applied != N_POSTS
       || strcmp(slot.str, oracle.str) || slot.set != oracle.set)
   {
      printf("core_status_defer: FAILED\n");
      return 1;
   }
   printf("core_status_defer: ok\n");
   return 0;
}
