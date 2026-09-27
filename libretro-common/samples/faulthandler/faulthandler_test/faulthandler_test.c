/* faulthandler_test.c - a page fault handed back, and the ones that are not.
 *
 * The thing this exists for: a thread touches a reserved-but-absent
 * page, the handler commits it and says it handled the fault, and the
 * faulting instruction re-executes and succeeds. That is one test, and
 * it either works or the process dies, so the harness reports what
 * happened rather than relying on a return code alone.
 *
 * Then the three cases the filter must NOT claim, each checked by
 * installing a chain handler of our own underneath and seeing it run:
 *   - a fault on a thread that never registered,
 *   - a fault after the handler is removed,
 *   - a callback that declines.
 * Each of those would otherwise be a silently swallowed crash.
 *
 * The recursion guard is not tested here: a fault inside the callback
 * is a real crash by construction, and a test that provokes one cannot
 * also survive to report it.
 *
 * Under TSan this reports "signal-unsafe call inside of a signal" for
 * the free() in rthreads' thread trampoline: the worker threads below
 * fault while inside their entry function, which is what the test is
 * for, and the trampoline's own cleanup is then inside a signal by
 * TSan's accounting. It reports no data race, which is the property
 * the lock-free dispatch is here to have.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include <signal.h>
#include <faulthandler.h>
#include <memmap.h>
#include <rthreads/rthreads.h>

static unsigned char *g_region;
static size_t         g_len;
static int            g_commits;
static int            g_declines;
static volatile int   g_chain_ran;
static sigjmp_buf     g_chain_jmp;

/* Commits the page that faulted: what a fastmem core does. */
static bool commit_handler(const retro_fault_info_t *info)
{
   if (info->addr < (uintptr_t)g_region || info->addr >= (uintptr_t)g_region + g_len)
      return false;
   g_commits++;
   if (!memcommit((void*)info->addr, mempagesize()))
      return false;
   return true;
}

static bool decline_handler(const retro_fault_info_t *info)
{
   (void)info;
   g_declines++;
   return false;
}

/* The handler that was there before ours: proves the fault chained. */
static void chain_handler(int sig, siginfo_t *si, void *ctx)
{
   (void)sig; (void)si; (void)ctx;
   g_chain_ran = 1;
   siglongjmp(g_chain_jmp, 1);
}

static void install_chain(void)
{
   struct sigaction sa;
   memset(&sa, 0, sizeof(sa));
   sigemptyset(&sa.sa_mask);
   sa.sa_flags     = SA_SIGINFO;
   sa.sa_sigaction = chain_handler;
   sigaction(SIGSEGV, &sa, NULL);
   sigaction(SIGBUS,  &sa, NULL);
}

/* Touch a page and report whether the access completed (1) or the
 * chain handler ran instead (0). */
static int touch(volatile unsigned char *p, unsigned char v)
{
   g_chain_ran = 0;
   if (sigsetjmp(g_chain_jmp, 1) == 0)
   {
      *p = v;
      return (*p == v);
   }
   return 0;
}

struct worker_arg { int registered; int completed; volatile unsigned char *p; };

static void worker(void *data)
{
   struct worker_arg *a = (struct worker_arg*)data;
   if (a->registered)
      retro_faulthandler_register_thread();
   a->completed = touch(a->p, 0x5A);
   if (a->registered)
      retro_faulthandler_unregister_thread();
}

int main(void)
{
   int ok = 1;
   size_t page;
   setvbuf(stdout, NULL, _IONBF, 0);
   printf("faulthandler\n");

   page  = mempagesize();
   g_len = page * 16;
   g_region = (unsigned char*)memreserve(g_len);
   if (!g_region)
   {
      printf("  memreserve unavailable on this platform; nothing to test\n");
      printf("faulthandler: ok (skipped)\n");
      return 0;
   }
   printf("  reserved %u bytes at %p, page %u\n", (unsigned)g_len, (void*)g_region, (unsigned)page);

   install_chain();

   /* 1: the point of the thing */
   if (!retro_faulthandler_install(commit_handler))
   {
      printf("  FAIL: install\n");
      return 1;
   }
   retro_faulthandler_register_thread();
   {
      int done = touch(g_region + page * 2, 0xA5);
      int good = done && g_commits == 1 && !g_chain_ran;
      printf("  %s: a fault on a reserved page was committed and the store completed (%d commit%s)\n",
             good ? "ok" : "FAIL", g_commits, g_commits == 1 ? "" : "s");
      ok &= good;
   }
   /* a second page: the handler is still live */
   {
      int done = touch(g_region + page * 5, 0x3C);
      int good = done && g_commits == 2;
      printf("  %s: a second page too\n", good ? "ok" : "FAIL");
      ok &= good;
   }
   /* 2: an unregistered thread chains */
   {
      struct worker_arg a;
      sthread_t *t;
      a.registered = 0; a.completed = -1; a.p = g_region + page * 8;
      t = sthread_create(worker, &a);
      if (t)
         sthread_join(t);
      printf("  %s: a fault on an unregistered thread was not claimed\n", (a.completed == 0) ? "ok" : "FAIL");
      ok &= (a.completed == 0);
   }
   /* and a registered one is */
   {
      struct worker_arg a;
      sthread_t *t;
      int before = g_commits;
      a.registered = 1; a.completed = -1; a.p = g_region + page * 9;
      t = sthread_create(worker, &a);
      if (t)
         sthread_join(t);
      printf("  %s: a fault on a registered thread was (%d commit%s)\n",
             (a.completed == 1 && g_commits == before + 1) ? "ok" : "FAIL",
             g_commits - before, (g_commits - before) == 1 ? "" : "s");
      ok &= (a.completed == 1 && g_commits == before + 1);
   }
   /* 3: a callback that declines chains */
   retro_faulthandler_remove(commit_handler);
   if (!retro_faulthandler_install(decline_handler))
   {
      printf("  FAIL: reinstall\n");
      return 1;
   }
   {
      int done = touch(g_region + page * 11, 0x11);
      int good = (done == 0) && g_declines == 1;
      printf("  %s: a callback that declines chains to the old handler\n", good ? "ok" : "FAIL");
      ok &= good;
   }
   /* 4: after removal, nothing is claimed */
   retro_faulthandler_remove(decline_handler);
   {
      int before = g_declines;
      int done   = touch(g_region + page * 13, 0x22);
      int good   = (done == 0) && g_declines == before;
      printf("  %s: after removal the filter is gone\n", good ? "ok" : "FAIL");
      ok &= good;
   }
   retro_faulthandler_unregister_thread();
   memrelease(g_region, g_len);
   printf(ok ? "faulthandler: ok\n" : "faulthandler: FAILED\n");
   return ok ? 0 : 1;
}
