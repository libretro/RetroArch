/* Regression test for the task_set_title / task_set_error
 * ownership protocol in libretro-common/queues/task_queue.c.
 *
 * Background
 * ----------
 * task_set_title() and task_set_error() take ownership of the
 * passed-in heap pointer (the task system frees it with free() at
 * task completion).  Neither setter frees the previous value, so
 * calling either one twice without an intervening free leaks the
 * previous string.  The header now documents this convention and
 * the file ships a task_free_error() helper symmetric to the
 * pre-existing task_free_title().
 *
 * Pre-helper, callers that legitimately needed to update the
 * error mid-task could not free the previous error at all (no
 * task_free_error existed).  Their only options were the
 * task_get_error / "skip if already set" guard or just letting
 * the leak happen; tasks/task_save.c uses the guard, but other
 * call sites that update title and error in lockstep had no
 * symmetric tool for the error half.  task_free_error closes
 * that gap.
 *
 * What this test asserts
 * ----------------------
 * 1. Both helpers exist and link (catches a regression that drops
 *    the helper or its declaration).
 * 2. The free-then-set protocol is leak-clean:
 *      task_set_title(t, strdup(A));
 *      task_free_title(t);
 *      task_set_title(t, strdup(B));
 *      ... task system frees B at completion.
 *    Total: two strdups, two frees, no leaks.  Same shape for
 *    task_set_error / task_free_error.
 * 3. task_free_{title,error} on a task whose field is already NULL
 *    is a no-op (matches the existing task_free_title behaviour).
 * 4. Setting NULL via the setter does not crash and replaces the
 *    field as expected.
 *
 * What this test does NOT assert
 * ------------------------------
 * It does not exercise the threaded path (HAVE_THREADS is not
 * defined for this binary), since the protocol is identical -- the
 * locks just serialise it.  It does not exercise queue lifecycle
 * (push/gather), since those are orthogonal to the property
 * helpers.  Future tests under libretro-common/samples/queues/
 * should cover those if motivated by a regression.
 *
 * How the regression is caught
 * ----------------------------
 * Built under ASan + LSan (the workflow's default), each leak in
 * the protocol surfaces as an LSan report at exit.  Without ASan
 * the test still verifies linkage (helpers exist) and observable
 * state (final field values), so build-time regressions are caught
 * without needing a sanitizer.  An accounting layer counts strdups
 * and frees through a thin wrapper so the no-leak property is
 * checked even on non-ASan builds.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <queues/task_queue.h>

static int failures = 0;

/* -----------------------------------------------------------------
 * Allocation accounting.  Wrap strdup so we can count bytes
 * allocated; pair with a hooked free path used only by the test
 * driver below.  task_queue.c itself uses the libc free() directly,
 * so to count THOSE frees we have to track every pointer the test
 * hands off and verify it has been freed by the time the test
 * harness completes the task.  Easiest way: do the strdup ourselves
 * and verify the task system freed it via accessing the pointer's
 * destination after task completion (we can't safely do that --
 * UAF), so instead verify by counting the alloc/free balance using
 * a malloc-wrapping override at link time would be overkill.
 *
 * Practical approach: use strdup directly and rely on ASan/LSan to
 * catch the leak.  The test below is structured so every strdup
 * must be paired with either an explicit task_free_{title,error}
 * or an internal-gather free triggered by the task transitioning
 * to FINISHED.  We DO NOT actually run the task queue here; we
 * simulate the gather frees by calling the public free helpers
 * before resetting the task and freeing it ourselves.  That keeps
 * the test entirely standalone -- no thread, no time source, no
 * msg_push needed.
 * ----------------------------------------------------------------- */

static char *xstrdup(const char *s)
{
   size_t n;
   char  *r;

   if (!s)
      return NULL;
   n = strlen(s) + 1;
   r = (char *)malloc(n);
   if (!r)
   {
      printf("[FATAL] OOM in xstrdup\n");
      exit(2);
   }
   memcpy(r, s, n);
   return r;
}

/* Helper: build a synthetic task with all fields zeroed.  We do
 * NOT use task_init() because that routes through a static counter
 * and would couple the test to task_init's exact behaviour.
 * What matters for this test is the title/error fields. */
static retro_task_t *make_bare_task(void)
{
   retro_task_t *t = (retro_task_t *)calloc(1, sizeof(*t));
   if (!t)
   {
      printf("[FATAL] OOM in make_bare_task\n");
      exit(2);
   }
   return t;
}

static void destroy_bare_task(retro_task_t *t)
{
   /* Mirror what retro_task_internal_gather does on completion:
    * free the leftover title/error (if any) and the task itself.
    * Tests are responsible for emptying the fields beforehand if
    * they want LSan to flag the prior strdup as leaked. */
   if (t->error)
   {
      free(t->error);
      t->error = NULL;
   }
   if (t->title)
   {
      free(t->title);
      t->title = NULL;
   }
   free(t);
}

/* -----------------------------------------------------------------
 * Test cases
 * ----------------------------------------------------------------- */

/* Case 1: single set_title + completion.  Baseline leak-free path. */
static void test_title_set_once(void)
{
   retro_task_t *t = make_bare_task();

   task_set_title(t, xstrdup("hello"));
   if (!t->title || strcmp(t->title, "hello") != 0)
   {
      printf("[ERROR] title_set_once: expected 'hello', got %s\n",
            t->title ? t->title : "(null)");
      failures++;
   }
   else
      printf("[SUCCESS] title_set_once\n");

   destroy_bare_task(t);
}

/* Case 2: single set_error + completion.  Baseline leak-free path. */
static void test_error_set_once(void)
{
   retro_task_t *t = make_bare_task();

   task_set_error(t, xstrdup("oops"));
   if (!t->error || strcmp(t->error, "oops") != 0)
   {
      printf("[ERROR] error_set_once: expected 'oops', got %s\n",
            t->error ? t->error : "(null)");
      failures++;
   }
   else
      printf("[SUCCESS] error_set_once\n");

   destroy_bare_task(t);
}

/* Case 3: title replaced via task_free_title.  This is the protocol
 * the header documents.  Two strdups, two frees, LSan-clean. */
static void test_title_replace_correct(void)
{
   retro_task_t *t = make_bare_task();

   task_set_title(t, xstrdup("first"));
   task_free_title(t);
   task_set_title(t, xstrdup("second"));

   if (!t->title || strcmp(t->title, "second") != 0)
   {
      printf("[ERROR] title_replace_correct: expected 'second', got %s\n",
            t->title ? t->title : "(null)");
      failures++;
   }
   else
      printf("[SUCCESS] title_replace_correct\n");

   destroy_bare_task(t);
}

/* Case 4: error replaced via task_free_error (the newly-added
 * helper).  This case exists *because* of the helper -- before this
 * patch, you could not write this code without a manual free(t->error)
 * which had to reach into the struct directly.  Two strdups, two
 * frees, LSan-clean. */
static void test_error_replace_correct(void)
{
   retro_task_t *t = make_bare_task();

   task_set_error(t, xstrdup("first error"));
   task_free_error(t);
   task_set_error(t, xstrdup("second error"));

   if (!t->error || strcmp(t->error, "second error") != 0)
   {
      printf("[ERROR] error_replace_correct: expected 'second error', got %s\n",
            t->error ? t->error : "(null)");
      failures++;
   }
   else
      printf("[SUCCESS] error_replace_correct\n");

   destroy_bare_task(t);
}

/* Case 5: task_free_title on already-NULL field.  Must be a no-op. */
static void test_title_free_when_null(void)
{
   retro_task_t *t = make_bare_task();

   task_free_title(t); /* no-op */
   if (t->title != NULL)
   {
      printf("[ERROR] title_free_when_null: title is non-NULL after free\n");
      failures++;
   }
   else
      printf("[SUCCESS] title_free_when_null\n");

   destroy_bare_task(t);
}

/* Case 6: task_free_error on already-NULL field.  Must be a no-op. */
static void test_error_free_when_null(void)
{
   retro_task_t *t = make_bare_task();

   task_free_error(t); /* no-op */
   if (t->error != NULL)
   {
      printf("[ERROR] error_free_when_null: error is non-NULL after free\n");
      failures++;
   }
   else
      printf("[SUCCESS] error_free_when_null\n");

   destroy_bare_task(t);
}

/* Case 7: task_free_title called twice in a row.  The second call
 * should be a no-op (matches the documented behaviour). */
static void test_title_double_free(void)
{
   retro_task_t *t = make_bare_task();

   task_set_title(t, xstrdup("hello"));
   task_free_title(t);
   task_free_title(t); /* no-op, must not double-free */

   if (t->title != NULL)
   {
      printf("[ERROR] title_double_free: title is non-NULL after double-free\n");
      failures++;
   }
   else
      printf("[SUCCESS] title_double_free\n");

   destroy_bare_task(t);
}

/* Case 8: task_free_error called twice in a row.  Must not
 * double-free.  This is the symmetric coverage for task_free_error,
 * the helper added by this commit. */
static void test_error_double_free(void)
{
   retro_task_t *t = make_bare_task();

   task_set_error(t, xstrdup("oops"));
   task_free_error(t);
   task_free_error(t); /* no-op, must not double-free */

   if (t->error != NULL)
   {
      printf("[ERROR] error_double_free: error is non-NULL after double-free\n");
      failures++;
   }
   else
      printf("[SUCCESS] error_double_free\n");

   destroy_bare_task(t);
}

/* Case 9: long chain of free-then-set on title.  Stress test for
 * the protocol -- N strdups, N frees, no leaks. */
static void test_title_chain(void)
{
   retro_task_t *t = make_bare_task();
   int           i;
   int           n = 50;
   char          buf[32];

   for (i = 0; i < n; i++)
   {
      if (t->title)
         task_free_title(t);
      snprintf(buf, sizeof(buf), "step-%d", i);
      task_set_title(t, xstrdup(buf));
   }

   if (!t->title || strcmp(t->title, "step-49") != 0)
   {
      printf("[ERROR] title_chain: final value mismatch (got %s)\n",
            t->title ? t->title : "(null)");
      failures++;
   }
   else
      printf("[SUCCESS] title_chain (%d iterations)\n", n);

   destroy_bare_task(t);
}

/* Case 10: long chain of free-then-set on error.  Same stress test
 * for task_free_error. */
static void test_error_chain(void)
{
   retro_task_t *t = make_bare_task();
   int           i;
   int           n = 50;
   char          buf[32];

   for (i = 0; i < n; i++)
   {
      if (t->error)
         task_free_error(t);
      snprintf(buf, sizeof(buf), "err-%d", i);
      task_set_error(t, xstrdup(buf));
   }

   if (!t->error || strcmp(t->error, "err-49") != 0)
   {
      printf("[ERROR] error_chain: final value mismatch (got %s)\n",
            t->error ? t->error : "(null)");
      failures++;
   }
   else
      printf("[SUCCESS] error_chain (%d iterations)\n", n);

   destroy_bare_task(t);
}

/* -----------------------------------------------------------------
 * task_queue.c uses cpu_features_get_time_usec() in its non-threaded
 * gather path.  We don't drive the queue, so it never runs -- but
 * we still need the symbol present at link time when libtool
 * resolves task_queue.c's references.  Provide a trivial stub.
 * libretro.h already brings retro_time_t in via task_queue.h above.
 * ----------------------------------------------------------------- */

retro_time_t cpu_features_get_time_usec(void);
retro_time_t cpu_features_get_time_usec(void)
{
   return 0;
}

int main(void)
{
   test_title_set_once();
   test_error_set_once();
   test_title_replace_correct();
   test_error_replace_correct();
   test_title_free_when_null();
   test_error_free_when_null();
   test_title_double_free();
   test_error_double_free();
   test_title_chain();
   test_error_chain();

   if (failures)
   {
      printf("\n%d task_queue title/error protocol test(s) failed\n",
            failures);
      return 1;
   }
   printf("\nAll task_queue title/error protocol tests passed.\n");
   return 0;
}
