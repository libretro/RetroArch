/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image_texture_set.c).
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
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <formats/image.h>
#include <formats/data_transfer.h>
#ifdef HAVE_GCD
#include <dispatch/dispatch.h>
#elif defined(HAVE_THREADS)
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <features/features_cpu.h>
#endif

#if !defined(HAVE_GCD) && defined(HAVE_THREADS)
/* The next index a worker takes */
typedef struct
{
   void (*fn)(unsigned i, void *ud);
   void (*thread_done)(void);
   void *ud;
   retro_atomic_int_t next;
   unsigned n;
} image_texture_set_t;

static void image_texture_set_worker(void *data)
{
   image_texture_set_t *set = (image_texture_set_t*)data;
   for (;;)
   {
      unsigned i = (unsigned)retro_atomic_fetch_add_int(&set->next, 1);
      if (i >= set->n)
         return;
      set->fn(i, set->ud);
   }
}

static void image_texture_set_thread(void *data)
{
   image_texture_set_t *set = (image_texture_set_t*)data;
   image_texture_set_worker(set);
   if (set->thread_done)
      set->thread_done();
}
#endif

void image_texture_set_run(unsigned n,
      void (*fn)(unsigned i, void *ud), void *ud)
{
   image_texture_set_run_ex(n, fn, ud, NULL);
}

void image_texture_set_run_ex(unsigned n,
      void (*fn)(unsigned i, void *ud), void *ud,
      void (*thread_done)(void))
{
   unsigned i;
   /* A dispatch pool's threads live on, and their pools with them:
    * bounded, as the task queue's own are */
   (void)thread_done;
   if (!n || !fn)
      return;
#ifdef HAVE_GCD
   if (n > 1)
   {
      dispatch_apply(n,
            dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
            ^(size_t at) { fn((unsigned)at, ud); });
      return;
   }
#elif defined(HAVE_THREADS)
   {
      unsigned workers = cpu_features_get_core_amount();
      if (workers > 8)
         workers = 8;
      if (workers > 1 && n > 1)
      {
         /* This thread works alongside the others; a worker that
          * could not be made costs nothing but its share */
         image_texture_set_t set;
         sthread_t *thread[8];
         unsigned t, spawned = workers - 1;
         set.fn          = fn;
         set.thread_done = thread_done;
         set.ud          = ud;
         set.n           = n;
         retro_atomic_int_init(&set.next, 0);
         if (spawned > n - 1)
            spawned = n - 1;
         for (t = 0; t < spawned; t++)
            thread[t] = sthread_create(image_texture_set_thread, &set);
         image_texture_set_worker(&set);
         for (t = 0; t < spawned; t++)
            if (thread[t])
               sthread_join(thread[t]);
         return;
      }
   }
#endif
   for (i = 0; i < n; i++)
      fn(i, ud);
}

typedef struct
{
   const char *const *paths;
   struct texture_image *imgs;
   const image_texture_request_t *req;
} image_texture_load_set_t;

static void image_texture_load_one(unsigned i, void *ud)
{
   image_texture_load_set_t *set = (image_texture_load_set_t*)ud;
   struct texture_image *img     = &set->imgs[i];
   if (     !set->paths[i] || !*set->paths[i]
         || !image_texture_load_request(img, set->paths[i], set->req,
            NULL, NULL))
      img->pixels = NULL;
}

unsigned image_texture_load_set(const char *const *paths,
      struct texture_image *imgs, unsigned n,
      const image_texture_request_t *req)
{
   image_texture_load_set_t set;
   unsigned i, done = 0;
   if (!paths || !imgs || !n)
      return 0;
   memset(imgs, 0, (size_t)n * sizeof(*imgs));
   set.paths = paths;
   set.imgs  = imgs;
   set.req   = req;
   image_texture_set_run_ex(n, image_texture_load_one, &set,
         data_transfer_pool_flush);
   for (i = 0; i < n; i++)
      if (imgs[i].pixels || imgs[i].compressed)
         done++;
   return done;
}
