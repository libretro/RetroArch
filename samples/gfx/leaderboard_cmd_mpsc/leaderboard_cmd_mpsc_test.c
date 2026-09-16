/* leaderboard_cmd_mpsc_test.c -- the achievement tracker command
 * stream from gfx/widgets/gfx_widget_leaderboard_display.c,
 * reproduced and beaten on.
 *
 * The protocol: every mutation of the tracker state - set or update
 * a display by id, hide one, clear all, the challenge and progress
 * variants, the two flags - crosses from the cheevos thread as a
 * whole command node on a lock-free MPSC stack, and the draw
 * thread's iterate drains and applies them in arrival order onto
 * state only it touches. The claims a regression would break:
 *
 *   1. Commands arrive whole (checksummed payloads) and in issue
 *      order - a value update for an id never applies before the
 *      set that created it or after the hide that removed it.
 *   2. The applied end-state equals an oracle running the same
 *      script single-threaded: for every id, the last visible
 *      value, and for the flags, the last write.
 *   3. Every node is freed exactly once (ASan holds this).
 *
 * The compacting-array mechanics themselves are draw-thread-only
 * plain code once the commands arrive; what this harness pins is
 * the handoff, which is the only concurrent part - and which the
 * in-tree lock never actually guarded: it was used without ever
 * being created, so every acquisition was slock_lock(NULL), a
 * silent no-op over a live race.
 *
 * Paced with short sleeps so a single-processor machine interleaves
 * the threads. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <boolean.h>
#include <retro_atomic.h>
#include <compat/strl.h>
#include <queues/mpsc_stack.h>
#include <rthreads/rthreads.h>

#if defined(_WIN32)
#include <windows.h>
static void sleep_us(unsigned us) { Sleep(us / 1000u + 1u); }
#else
#include <unistd.h>
static void sleep_us(unsigned us) { usleep(us); }
#endif

#define N_IDS   6
#define N_CMDS  4000

enum { K_SET, K_HIDE, K_CLEAR, K_FLAG };

struct cmd
{
   mpsc_stack_node_t link;
   int      seq;
   unsigned id;
   uint8_t  kind;
   bool     flag;
   char     value[32];
};

static mpsc_stack_t g_pending;
static retro_atomic_int_t g_done;

static unsigned checksum(const char *b)
{
   unsigned h = 2166136261u;
   for (; *b; b++)
      h = (h ^ (unsigned char)*b) * 16777619u;
   return h;
}

/* the oracle and the consumer share the semantic apply */
struct model
{
   char values[N_IDS][32]; /* [0]=='\0' means hidden */
   bool flag;
};

static void model_apply(struct model *m, const struct cmd *c)
{
   switch (c->kind)
   {
      case K_SET:
         strlcpy(m->values[c->id], c->value, sizeof(m->values[c->id]));
         break;
      case K_HIDE:
         m->values[c->id][0] = '\0';
         break;
      case K_CLEAR:
         memset(m->values, 0, sizeof(m->values));
         break;
      case K_FLAG:
         m->flag = c->flag;
         break;
   }
}

static void make_cmd(struct cmd *c, int i)
{
   unsigned r = (unsigned)(i * 2654435761u);
   c->seq  = i;
   c->id   = r % N_IDS;
   c->flag = (r & 8) != 0;
   c->value[0] = '\0';
   if ((i % 97) == 96)
      c->kind = K_CLEAR;
   else if ((i % 11) == 10)
      c->kind = K_HIDE;
   else if ((i % 29) == 28)
      c->kind = K_FLAG;
   else
   {
      char body[24];
      c->kind = K_SET;
      snprintf(body, sizeof(body), "v%u:%d", c->id, i);
      snprintf(c->value, sizeof(c->value), "%s#%08x",
            body, checksum(body));
   }
}

static void producer(void *arg)
{
   int i;
   (void)arg;
   for (i = 0; i < N_CMDS; i++)
   {
      struct cmd *c = (struct cmd *)malloc(sizeof(*c));
      if (!c)
         abort();
      make_cmd(c, i);
      mpsc_stack_push(&g_pending, &c->link);
      if ((i & 31) == 0)
         sleep_us(80);
   }
   retro_atomic_store_release_int(&g_done, 1);
}

int main(void)
{
   sthread_t *p;
   struct model applied, oracle;
   struct cmd tmp;
   int i, applied_n = 0, torn = 0, order = 0, last_seq = -1;

   memset(&applied, 0, sizeof(applied));
   memset(&oracle,  0, sizeof(oracle));
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
         struct cmd *c = (struct cmd *)link;
         link = link->next;
         if (c->seq != last_seq + 1)
            order++;
         last_seq = c->seq;
         if (c->kind == K_SET)
         {
            const char *hash = strrchr(c->value, '#');
            unsigned want;
            char body[32];
            if (   !hash || (size_t)(hash - c->value) >= sizeof(body)
                || sscanf(hash + 1, "%x", &want) != 1)
               torn++;
            else
            {
               memcpy(body, c->value, (size_t)(hash - c->value));
               body[hash - c->value] = '\0';
               if (checksum(body) != want)
                  torn++;
            }
         }
         model_apply(&applied, c);
         applied_n++;
         free(c);
      }
      if (done && mpsc_stack_empty(&g_pending))
         break;
      sleep_us(140);
   }
   sthread_join(p);

   for (i = 0; i < N_CMDS; i++)
   {
      make_cmd(&tmp, i);
      model_apply(&oracle, &tmp);
   }

   printf("leaderboard_cmd_mpsc: %d issued, %d applied\n",
         N_CMDS, applied_n);
   if (torn)
      printf("  FAIL: %d torn payload(s)\n", torn);
   if (order)
      printf("  FAIL: %d order break(s)\n", order);
   if (applied_n != N_CMDS)
      printf("  FAIL: %d command(s) unaccounted\n", N_CMDS - applied_n);
   if (memcmp(&applied, &oracle, sizeof(applied)))
      printf("  FAIL: end-state diverges from the single-threaded oracle\n");
   if (torn || order || applied_n != N_CMDS
       || memcmp(&applied, &oracle, sizeof(applied)))
   {
      printf("leaderboard_cmd_mpsc: FAILED\n");
      return 1;
   }
   printf("leaderboard_cmd_mpsc: ok\n");
   return 0;
}
