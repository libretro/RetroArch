/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (tls_cache_threads_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* The TLS session cache from several threads at once, under TSan and
 * ASan. Threads store and read back entries for more hosts than there
 * are slots, so entries are built, replaced and evicted while others
 * read them: a read must find a whole entry for its host or none.
 *
 * Built with TLS_TEST_HOLD_LANE, a read is held just as it finds an
 * entry, and a store for another host meanwhile must still finish: no
 * thread waits on another. TLS_CACHE_TEST_FLAG builds the cache on its
 * flag instead, where the held read keeps the flag, and that must
 * fail. */

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>

#ifdef TLS_TEST_HOLD_LANE
static volatile int hold_on, held, let_go;
#define TLS_CACHE_TEST_HOLD() do { if (hold_on && !held) { held = 1; \
   while (!let_go) sched_yield(); } } while (0)
#endif

#include "../../net/net_socket_ssl_retro.c"

#define NTHREADS 4
#define ROUNDS   4000
#define HOSTS    12

static int bad;
static pthread_mutex_t bad_lock = PTHREAD_MUTEX_INITIALIZER;

static void host_name(char *out, size_t len, unsigned h)
{
   snprintf(out, len, "host%u.example", h);
}

/* An entry that says which host it is for, all through. */
static void put(unsigned h, unsigned round)
{
   struct tls_session_put p;
   uint8_t master[48], ticket[24];
   char host[64];
   host_name(host, sizeof(host), h);
   memset(master, (int)h, sizeof(master));
   memset(ticket, (int)h, sizeof(ticket));
   memset(&p, 0, sizeof(p));
   if (round & 1)
   {
      p.what    = TLS_PUT_MASTER;
      p.master  = master;
      p.suite   = h;
   }
   else
   {
      p.what       = TLS_PUT_TICKET;
      p.ticket     = ticket;
      p.ticket_len = sizeof(ticket) - (h % 4);
   }
   tls_cache_put(host, &p);
}

static void check(unsigned h)
{
   const struct tls_session *e;
   char host[64];
   int wrong = 0;
   host_name(host, sizeof(host), h);
   TLS_CACHE_READ_BEGIN();
   if ((e = tls_cache_find(host)))
   {
      size_t i;
      if (strcmp(e->host, host))
         wrong = 1;
      for (i = 0; i < e->ticket_len && !wrong; i++)
         if (e->ticket[i] != (uint8_t)h)
            wrong = 1;
      if (e->ticket && e->ticket_len != 24 - (h % 4))
         wrong = 1;
      if (e->suite && (e->suite != h || e->master[47] != (uint8_t)h))
         wrong = 1;
   }
   TLS_CACHE_READ_END();
   if (wrong)
   {
      pthread_mutex_lock(&bad_lock);
      bad++;
      pthread_mutex_unlock(&bad_lock);
   }
}

static void *worker(void *arg)
{
   unsigned i, seed = (unsigned)(size_t)arg * 2654435761u;
   for (i = 0; i < ROUNDS; i++)
   {
      unsigned h;
      seed = seed * 1103515245u + 12345u;
      h    = (seed >> 16) % HOSTS;
      if (seed & 0x100)
         put(h, seed >> 9);
      else
         check(h);
   }
   return NULL;
}

#ifdef TLS_TEST_HOLD_LANE
static void *held_read(void *arg)
{
   (void)arg;
   check(0);
   return NULL;
}

static void *other_put(void *arg)
{
   (void)arg;
   put(1, 1);
   return NULL;
}
#endif

int main(void)
{
   pthread_t t[NTHREADS];
   int i, fails = 0;

   for (i = 0; i < NTHREADS; i++)
      pthread_create(&t[i], NULL, worker, (void*)(size_t)(i + 1));
   for (i = 0; i < NTHREADS; i++)
      pthread_join(t[i], NULL);
   if (bad)
   {
      printf("FAIL: %d reads found an entry not whole for its host\n", bad);
      fails++;
   }
   else
      printf("ok:   %d threads x %d stores and reads over %d hosts in %d slots\n",
            NTHREADS, ROUNDS, HOSTS, TLS_CACHE_SLOTS);

#ifdef TLS_TEST_HOLD_LANE
   {
      pthread_t rd, wr;
      struct timespec ts;
      int done;
      put(0, 1);
      hold_on = 1;
      pthread_create(&rd, NULL, held_read, NULL);
      while (!held)
         sched_yield();
      pthread_create(&wr, NULL, other_put, NULL);
      clock_gettime(CLOCK_REALTIME, &ts);
      ts.tv_sec += 2;
      done = pthread_timedjoin_np(wr, NULL, &ts) == 0;
      let_go = 1;
      pthread_join(rd, NULL);
      if (!done)
         pthread_join(wr, NULL);
      hold_on = 0;
#ifdef TLS_CACHE_TEST_FLAG
      if (done)
      {
         printf("FAIL: a store finished while a read held the flag\n");
         fails++;
      }
      else
         printf("ok:   the flag-guarded cache kept a store waiting on a held read\n");
#else
      if (!done)
      {
         printf("FAIL: a store waited on a read held as it found its entry\n");
         fails++;
      }
      else
         printf("ok:   a store finished while a read was held on its entry\n");
#endif
   }
#endif
   return fails ? 1 : 0;
}
