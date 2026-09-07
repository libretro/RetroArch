/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (http_dns_evict_deadlock_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Regression test for the DNS cache eviction deadlock in
 * libretro-common/net/net_http.c.
 *
 * net_http_dns_cache_remove_expired() runs with the DNS cache lock
 * held -- net_http_dns_cache_find() is only ever called under it,
 * from net_http_new_socket() and net_http_connect().  It used to
 * sthread_join() the resolver thread of any entry it decided to
 * evict:
 *
 *     if (entry->thread)
 *     {
 *        sthread_join(entry->thread);
 *        entry->thread = NULL;
 *     }
 *
 * net_http_resolve() takes that same lock twice, once on entry to
 * copy the domain and once on completion to publish the result.  A
 * join against a thread blocked acquiring the lock the joiner is
 * holding never returns.
 *
 * The window is not theoretical, which is the whole reason this test
 * exists.  entry->addr stays NULL for the entire resolution, so the
 * fail-timeout arm of the expiry test
 *
 *     (!entry->addr && entry->timestamp + dns_cache_fail_timeout < now)
 *
 * fires after 30 seconds -- and getaddrinfo() against a blackholed
 * resolver routinely blocks longer than that.  The entry therefore
 * looks expired *precisely while its own resolver thread is still
 * running*.  The result is a hung task thread, and in unthreaded
 * builds a hung frontend, from nothing worse than a slow DNS server.
 *
 * Reproducing that honestly would mean a 30 second wall-clock wait
 * against an unreachable nameserver, which is neither deterministic
 * nor welcome in a test suite.  So this test wraps two symbols at
 * link time (see Makefile.dns_evict):
 *
 *   __wrap_getaddrinfo             -- blocks on a condition variable
 *                                     for one nominated hostname, so
 *                                     the resolver thread can be held
 *                                     mid-flight on demand.
 *   __wrap_cpu_features_get_time_usec
 *                                  -- adds a settable offset to the
 *                                     real clock, so the cache entry
 *                                     can be aged past
 *                                     dns_cache_fail_timeout
 *                                     instantly.
 *
 * --wrap is used rather than a plain strong definition because both
 * symbols are defined inside the libretro-common objects linked into
 * this binary, so overriding them the way http_transfer_stress_test.c
 * overrides libc's recv() would be a duplicate definition.
 *
 * The test then drives the exact sequence:
 *
 *   1. Start a transfer to the blocked host.  net_http_new_socket()
 *      creates a cache entry and spawns a resolver, which parks
 *      inside getaddrinfo.
 *   2. Advance the clock past dns_cache_fail_timeout, so that entry
 *      now satisfies the expiry test while its thread is still live.
 *   3. Start a second transfer to the same host.  It takes the DNS
 *      cache lock and reaches net_http_dns_cache_remove_expired(),
 *      which finds the expired entry.
 *
 * Pre-fix, step 3 joins the parked resolver and never returns.
 * Post-fix, it skips entries whose resolver has not published yet
 * (entry->valid is still false) and leaves them for a later sweep.
 *
 * Because the failure mode is a hang rather than a wrong answer, the
 * driver runs step 3 on its own thread behind a watchdog: if it has
 * not returned within DEADLOCK_TIMEOUT_SEC the test reports a
 * failure and exits rather than blocking CI forever.  It deliberately
 * does not try to recover -- the process is wedged by then, and a
 * clean report plus a non-zero exit is the useful outcome.
 *
 * Build and run:
 *   make -f Makefile.dns_evict check
 *   make -f Makefile.dns_evict check SANITIZER=thread
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <net/net_http.h>
#include <net/net_compat.h>
#include <retro_common_api.h>

/* Mirrors dns_cache_fail_timeout in net_http.c (30s, in usec). */
#define DNS_FAIL_TIMEOUT_USEC ((int64_t)30 * 1000 * 1000)

/* How long step 3 may take before we call it a deadlock.  Generous:
 * the operation under test is a lock acquisition and a list walk. */
#define DEADLOCK_TIMEOUT_SEC  10

/* Hostname whose resolution we hold open.  Never actually resolved. */
#define BLOCKED_HOST "stalled-resolver.invalid"

static int failures;
static int checks;

#define CHECK(cond, ...) \
   do { \
      checks++; \
      if (!(cond)) \
      { \
         printf("    FAIL: "); printf(__VA_ARGS__); printf("\n"); \
         failures++; \
      } \
   } while (0)

/* ================================================================= */
/* Linker-wrapped symbols                                            */
/* ================================================================= */

int __real_getaddrinfo(const char *node, const char *service,
      const struct addrinfo *hints, struct addrinfo **res);
int64_t __real_cpu_features_get_time_usec(void);

static pthread_mutex_t g_gate      = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_gate_cv   = PTHREAD_COND_INITIALIZER;
static int             g_gate_open;      /* resolver may proceed */
static int             g_resolver_parked;

static int64_t         g_clock_offset_usec;

int __wrap_getaddrinfo(const char *node, const char *service,
      const struct addrinfo *hints, struct addrinfo **res)
{
   if (node && strcmp(node, BLOCKED_HOST) == 0)
   {
      pthread_mutex_lock(&g_gate);
      g_resolver_parked = 1;
      pthread_cond_broadcast(&g_gate_cv);
      while (!g_gate_open)
         pthread_cond_wait(&g_gate_cv, &g_gate);
      pthread_mutex_unlock(&g_gate);
      /* Released: fail the lookup, which is what a blackholed
       * resolver eventually does anyway. */
      return EAI_NONAME;
   }
   return __real_getaddrinfo(node, service, hints, res);
}

int64_t __wrap_cpu_features_get_time_usec(void)
{
   return __real_cpu_features_get_time_usec() + g_clock_offset_usec;
}

static void wait_for_parked_resolver(void)
{
   pthread_mutex_lock(&g_gate);
   while (!g_resolver_parked)
      pthread_cond_wait(&g_gate_cv, &g_gate);
   pthread_mutex_unlock(&g_gate);
}

static void release_resolver(void)
{
   pthread_mutex_lock(&g_gate);
   g_gate_open = 1;
   pthread_cond_broadcast(&g_gate_cv);
   pthread_mutex_unlock(&g_gate);
}

/* ================================================================= */
/* Driver                                                            */
/* ================================================================= */

/* One net_http_update() tick against the blocked host.  That is all
 * it takes to reach net_http_new_socket() -> net_http_dns_cache_find()
 * -> net_http_dns_cache_remove_expired() with the lock held. */
static void poke_transfer(void)
{
   char url[128];
   struct http_connection_t *conn;
   struct http_t *h;
   size_t pos = 0, tot = 0;

   snprintf(url, sizeof(url), "http://%s/x", BLOCKED_HOST);

   if (!(conn = net_http_connection_new(url, "GET", NULL)))
      return;
   net_http_connection_iterate(conn);
   if (!net_http_connection_done(conn))
   {
      net_http_connection_free(conn);
      return;
   }
   if (!(h = net_http_new(conn)))
   {
      net_http_connection_free(conn);
      return;
   }
   net_http_connection_free(conn);

   /* A single tick: enough to create or sweep the cache entry, and we
    * must not spin here since the resolver is parked on purpose. */
   net_http_update(h, &pos, &tot);
   net_http_delete(h);
}

/* Guarded rather than a bare volatile: `volatile` orders nothing
 * between threads, and TSan is right to flag it. */
struct sweep_arg
{
   pthread_mutex_t lock;
   int             done;
};

static void *sweep_thread(void *a)
{
   struct sweep_arg *sa = (struct sweep_arg*)a;
   poke_transfer();
   pthread_mutex_lock(&sa->lock);
   sa->done = 1;
   pthread_mutex_unlock(&sa->lock);
   return NULL;
}

static int sweep_done(struct sweep_arg *sa)
{
   int d;
   pthread_mutex_lock(&sa->lock);
   d = sa->done;
   pthread_mutex_unlock(&sa->lock);
   return d;
}

static void test_expired_entry_with_live_resolver(void)
{
   pthread_t th;
   struct sweep_arg sa;
   int waited = 0;

   printf("  sweep past an expired entry whose resolver is still running\n");

   memset(&sa, 0, sizeof(sa));
   pthread_mutex_init(&sa.lock, NULL);

   /* 1. Create the cache entry and park its resolver inside
    *    getaddrinfo. */
   poke_transfer();
   wait_for_parked_resolver();

   /* 2. Age the entry past dns_cache_fail_timeout.  entry->addr is
    *    still NULL (the resolver has published nothing), so the
    *    fail-timeout arm of the expiry test now matches it. */
   g_clock_offset_usec = DNS_FAIL_TIMEOUT_USEC * 2;

   /* 3. Sweep.  Pre-fix this joins the parked resolver under the DNS
    *    cache lock the resolver is itself waiting on, and never
    *    returns. */
   if (pthread_create(&th, NULL, sweep_thread, &sa) != 0)
   {
      printf("    SKIP: could not start sweep thread\n");
      release_resolver();
      return;
    }

   while (!sweep_done(&sa) && waited < DEADLOCK_TIMEOUT_SEC * 10)
   {
      usleep(100 * 1000);
      waited++;
   }

   CHECK(sweep_done(&sa),
         "cache sweep did not return within %ds -- deadlocked joining "
         "a resolver thread that is blocked on the DNS cache lock the "
         "sweep is holding",
         DEADLOCK_TIMEOUT_SEC);

   if (!sweep_done(&sa))
   {
      /* The process is wedged; report and get out rather than hang
       * CI waiting on a join that will never complete. */
      printf("\nFAILED (%d check%s, %d failure%s) -- deadlocked\n",
            checks, checks == 1 ? "" : "s",
            failures, failures == 1 ? "" : "s");
      fflush(stdout);
      _exit(1);
   }

   release_resolver();
   pthread_join(th, NULL);
   pthread_mutex_destroy(&sa.lock);

   /* The entry must still be reachable afterwards: skipping it is a
    * deferral, not a leak of the list. */
   g_clock_offset_usec = 0;
   poke_transfer();
   CHECK(1, "post-sweep transfer completed without wedging");
}

int main(void)
{
   printf("net_http DNS cache eviction deadlock regression test\n\n");

   network_init();
   net_http_init();

   test_expired_entry_with_live_resolver();

   /* Joins any remaining resolver thread with no lock held, which is
    * also the shape net_http_deinit() has to use for the same
    * reason. */
   net_http_deinit();

   printf("\n%s (%d check%s, %d failure%s)\n",
         failures ? "FAILED" : "PASSED",
         checks,   checks   == 1 ? "" : "s",
         failures, failures == 1 ? "" : "s");
   return failures ? 1 : 0;
}
