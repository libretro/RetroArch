/* image_blit_bands_test: the row-band blit covers every row exactly
 * once, from several threads at once, and does not touch the heap per
 * blit. A decoder calls it once per video frame; it used to malloc and
 * free a group each time, which is what the wrapped malloc counts. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <rthreads/tpool.h>
#include <formats/image_blit_bands.h>

/* --- heap counters through the linker's wrap --- */
void *__real_malloc(size_t);
void  __real_free(void *);
static retro_atomic_int_t mallocs;
void *__wrap_malloc(size_t n)
{
   retro_atomic_fetch_add_int(&mallocs, 1);
   return __real_malloc(n);
}
void __wrap_free(void *p) { __real_free(p); }

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL: " __VA_ARGS__); printf("\n"); fails++; } else { printf("ok   " __VA_ARGS__); printf("\n"); } } while (0)

#define ROWS 1080
typedef struct { uint8_t hit[ROWS]; } canvas_t;

static void fill_rows(void *ctx, unsigned row0, unsigned rows)
{
   canvas_t *c = (canvas_t*)ctx;
   unsigned r;
   for (r = row0; r < row0 + rows && r < ROWS; r++)
      c->hit[r]++;
}

static bool canvas_ok(const canvas_t *c)
{
   unsigned r;
   for (r = 0; r < ROWS; r++)
      if (c->hit[r] != 1)
         return false;
   return true;
}

/* Several callers blitting at once, each on its own canvas. */
typedef struct { tpool_t *pool; unsigned blits; bool ok; } caller_t;
static void caller_run(void *arg)
{
   caller_t *c = (caller_t*)arg;
   unsigned i;
   c->ok = true;
   for (i = 0; i < c->blits; i++)
   {
      canvas_t cv;
      memset(&cv, 0, sizeof(cv));
      image_blit_bands(c->pool, 8, ROWS, 2, fill_rows, &cv);
      if (!canvas_ok(&cv))
         c->ok = false;
   }
}

int main(void)
{
   tpool_t *pool = tpool_create(4);
   canvas_t cv;
   int m0, m1;
   unsigned i;

   CHECK(pool != NULL, "thread pool created");
   if (!pool)
      return 1;

   /* One blit: every row once, whatever band split the pool made. */
   memset(&cv, 0, sizeof(cv));
   image_blit_bands(pool, 8, ROWS, 2, fill_rows, &cv);
   CHECK(canvas_ok(&cv), "one blit covers every row exactly once");

   /* Odd sizes and alignment: 4:2:0 material needs even band starts. */
   {
      canvas_t c2;
      memset(&c2, 0, sizeof(c2));
      image_blit_bands(pool, 8, 719, 2, fill_rows, &c2);
      for (i = 0; i < 719; i++)
         if (c2.hit[i] != 1)
            break;
      CHECK(i == 719 && c2.hit[719] == 0, "719 rows, align 2: covered once, nothing past the end");
   }

   /* Warm up, then count: 500 blits must not allocate. */
   for (i = 0; i < 16; i++)
      image_blit_bands(pool, 8, ROWS, 2, fill_rows, &cv);
   m0 = retro_atomic_load_acquire_int(&mallocs);
   for (i = 0; i < 500; i++)
   {
      memset(&cv, 0, sizeof(cv));
      image_blit_bands(pool, 8, ROWS, 2, fill_rows, &cv);
   }
   m1 = retro_atomic_load_acquire_int(&mallocs);
   CHECK(m1 - m0 == 0, "500 blits: %d mallocs (want 0)", m1 - m0);

   /* Four callers at once, 200 blits each: the group pool is shared,
    * and every canvas still comes out whole. */
   {
      caller_t c[4];
      sthread_t *t[4];
      bool all_ok = true;
      for (i = 0; i < 4; i++)
      {
         c[i].pool = pool; c[i].blits = 200; c[i].ok = false;
         t[i] = sthread_create(caller_run, &c[i]);
      }
      for (i = 0; i < 4; i++)
      {
         if (t[i])
            sthread_join(t[i]);
         else
            all_ok = false;
         if (!c[i].ok)
            all_ok = false;
      }
      CHECK(all_ok, "four concurrent callers, 200 blits each, every row once");
   }

   /* No pool: the whole frame in one call, no allocation. */
   m0 = retro_atomic_load_acquire_int(&mallocs);
   memset(&cv, 0, sizeof(cv));
   image_blit_bands(NULL, 8, ROWS, 2, fill_rows, &cv);
   m1 = retro_atomic_load_acquire_int(&mallocs);
   CHECK(canvas_ok(&cv) && m1 - m0 == 0, "no pool: one call, no allocation");

   tpool_destroy(pool);
   printf("%s\n", fails ? "FAILED" : "PASSED");
   return fails ? 1 : 0;
}
