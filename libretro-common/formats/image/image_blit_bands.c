/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image_blit_bands.c).
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
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>

#include <formats/image_blit_bands.h>

#ifdef HAVE_THREADS
#include <retro_atomic.h>
#include <rthreads/tpool.h>
#include <rthreads/retro_eventcount.h>
#endif

/* Fewer rows than this per band and the hand-off costs more than the
 * rows; a 480-line frame splits in two, a 1080-line one in up to
 * eight. */
#define IMAGE_BLIT_BAND_MIN_ROWS 128
#define IMAGE_BLIT_BANDS_MAX     8

#ifdef HAVE_THREADS
/* One blit's bands and their bookkeeping, on the heap so that a runner
 * the pool starts late - behind a picture decoding, say - can still
 * find it after the caller has gone: the last holder frees it.
 *
 * No band belongs to a runner. Whoever runs - the caller included -
 * claims the next band from an atomic index and does it, until none
 * are left; a runner that starts to find them all claimed simply
 * leaves. So the blit is never held for a runner still queued behind
 * other work, and the caller never sleeps while there is a band it
 * could be doing itself. Only the last few bands, in other hands,
 * are waited for, on a process-wide eventcount: no lock, and a
 * syscall only when there is nothing left to do. */
typedef struct
{
   image_blit_rows_t fn;
   void *ctx;
   unsigned row0;
   unsigned rows;
} image_blit_band_t;

typedef struct
{
   retro_atomic_int_t next;    /* the next band to claim */
   retro_atomic_int_t done;    /* bands finished */
   retro_atomic_int_t refs;    /* the caller and every posted runner */
   unsigned bands;
   image_blit_band_t band[IMAGE_BLIT_BANDS_MAX];
} image_blit_group_t;

static retro_eventcount_t image_blit_ec;
static retro_atomic_int_t image_blit_ec_state;   /* 0 none, 1 making, 2 ready */

static int image_blit_ec_ready(void)
{
   int st = retro_atomic_load_acquire_int(&image_blit_ec_state);
   if (st == 2)
      return 1;
   if (st == 0 && retro_atomic_cas_int(&image_blit_ec_state, 0, 1))
   {
      if (retro_eventcount_init(&image_blit_ec))
      {
         retro_atomic_store_release_int(&image_blit_ec_state, 2);
         return 1;
      }
      retro_atomic_store_release_int(&image_blit_ec_state, 0);
   }
   return 0;   /* another caller is making it, or it cannot be made */
}

/* Do bands until none are left to claim. */
static void image_blit_group_drain(image_blit_group_t *g)
{
   for (;;)
   {
      int i = retro_atomic_fetch_add_int(&g->next, 1);
      if ((unsigned)i >= g->bands)
         return;
      g->band[i].fn(g->band[i].ctx, g->band[i].row0, g->band[i].rows);
      if ((unsigned)retro_atomic_fetch_add_int(&g->done, 1) + 1 == g->bands)
         retro_eventcount_notify(&image_blit_ec);
   }
}

static void image_blit_group_release(image_blit_group_t *g)
{
   if (retro_atomic_fetch_sub_int(&g->refs, 1) == 1)
      free(g);
}

static void image_blit_band_run(void *arg)
{
   image_blit_group_t *g = (image_blit_group_t*)arg;
   image_blit_group_drain(g);
   image_blit_group_release(g);
}
#endif

void image_blit_bands(void *pool, unsigned bands, unsigned h,
      unsigned align, image_blit_rows_t fn, void *ctx)
{
#ifdef HAVE_THREADS
   image_blit_group_t *g;
   unsigned i, row, per;
   if (bands > IMAGE_BLIT_BANDS_MAX)
      bands = IMAGE_BLIT_BANDS_MAX;
   if (pool && bands > 1 && h / bands < IMAGE_BLIT_BAND_MIN_ROWS)
      bands = h / IMAGE_BLIT_BAND_MIN_ROWS;
   if (!pool || bands <= 1 || !image_blit_ec_ready())
   {
      fn(ctx, 0, h);
      return;
   }
   if (align < 1)
      align = 1;
   g = (image_blit_group_t*)malloc(sizeof(*g));
   if (!g)
   {
      fn(ctx, 0, h);
      return;
   }
   per = ((h / bands) / align) * align;
   if (per < 1)
      per = align;
   for (i = 0, row = 0; i < bands; i++)
   {
      g->band[i].fn   = fn;
      g->band[i].ctx  = ctx;
      g->band[i].row0 = row;
      g->band[i].rows = (i == bands - 1) ? h - row : per;
      row            += g->band[i].rows;
   }
   g->bands = bands;
   retro_atomic_int_init(&g->next, 0);
   retro_atomic_int_init(&g->done, 0);
   retro_atomic_int_init(&g->refs, 1);
   /* runners for all but one band; the caller is the other pair of
    * hands, and takes whatever the runners have not */
   for (i = 0; i < bands - 1; i++)
   {
      retro_atomic_fetch_add_int(&g->refs, 1);
      if (!tpool_add_work((tpool_t*)pool, image_blit_band_run, g))
         retro_atomic_fetch_sub_int(&g->refs, 1);
   }
   image_blit_group_drain(g);
   /* the bands in other hands: wait on the eventcount, re-checking */
   while ((unsigned)retro_atomic_load_acquire_int(&g->done) < bands)
   {
      int key = retro_eventcount_prepare_wait(&image_blit_ec);
      if ((unsigned)retro_atomic_load_acquire_int(&g->done) >= bands)
      {
         retro_eventcount_cancel_wait(&image_blit_ec);
         break;
      }
      retro_eventcount_commit_wait(&image_blit_ec, key);
   }
   image_blit_group_release(g);
#else
   (void)pool; (void)bands; (void)align;
   fn(ctx, 0, h);
#endif
}
