/* Copyright  (C) 2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vfs_prefetch.c).
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

#include <stdlib.h>
#include <string.h>

#include <vfs/vfs_prefetch.h>

#ifdef HAVE_THREADS

#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <rthreads/retro_eventcount.h>

/* Slot states. The thread moves EMPTY -> FILLING -> READY; only the
 * reader moves READY -> EMPTY (a window it has passed, or one from a
 * stream it left), so every write the thread makes to a slot comes
 * after the reader's CAS and every read the reader makes comes after
 * the thread's release: no field is ever touched from both sides at
 * once, and neither side holds a lock. */
#define SLOT_EMPTY   0
#define SLOT_FILLING 1
#define SLOT_READY   2

struct vfs_prefetch_slot
{
   uint8_t           *buf;
   uint64_t           off;
   size_t             len;
   int                gen;       /* the stream this window belongs to */
   int                eof;       /* the file ends inside this window */
   retro_atomic_int_t state;
};

struct vfs_prefetch
{
   vfs_prefetch_fetch_t     fetch;
   void                    *user;
   size_t                   window;
   unsigned                 nslots;
   struct vfs_prefetch_slot *slot;

   /* The stream: a generation and the window index it starts at
    * (windows are aligned to their size, so an index is enough and it
    * fits an atomic int: 2^31 windows of 1 MiB). The reader stores the
    * target index then bumps gen (release); the thread loads gen
    * (acquire) and then the index. Windows carry the gen they were
    * fetched under, so the reader ignores stale ones and the thread
    * recycles them when it notices the bump. */
   retro_atomic_int_t       gen;
   retro_atomic_int_t       target_idx;
   /* the window index (offset / window) the thread fetches next, so the
    * reader can tell "not yet fetched" from "elsewhere" without a lock */
   retro_atomic_int_t       next_idx;
   retro_atomic_int_t       stop;

   retro_eventcount_t       wake;      /* the thread parks here */
   sthread_t               *thread;
   int                      thread_gen;
   uint64_t                 next_off;
   int                      at_eof;
};

static void vfs_prefetch_thread(void *data)
{
   struct vfs_prefetch *p = (struct vfs_prefetch*)data;

   for (;;)
   {
      unsigned i;
      int      gen;
      struct vfs_prefetch_slot *s = NULL;
      int64_t  n;

      if (retro_atomic_load_acquire_int(&p->stop))
         return;

      /* a retarget: start over there; the reader recycles the windows
       * of the old stream itself as it finds them */
      gen = retro_atomic_load_acquire_int(&p->gen);
      if (gen != p->thread_gen)
      {
         p->thread_gen = gen;
         p->next_off   = (uint64_t)retro_atomic_load_acquire_int(&p->target_idx) * p->window;
         p->at_eof     = 0;
         retro_atomic_store_release_int(&p->next_idx, (int)(p->next_off / p->window));
      }

      /* work: an empty slot while the stream has not ended */
      if (!p->at_eof)
      {
         for (i = 0; i < p->nslots; i++)
         {
            if (retro_atomic_cas_int(&p->slot[i].state, SLOT_EMPTY, SLOT_FILLING))
            {
               s = &p->slot[i];
               break;
            }
         }
      }

      if (!s)
      {
         /* nothing to do until the reader frees a window or seeks */
         int key = retro_eventcount_prepare_wait(&p->wake);
         if (retro_atomic_load_acquire_int(&p->stop)
               || retro_atomic_load_acquire_int(&p->gen) != p->thread_gen)
         {
            retro_eventcount_cancel_wait(&p->wake);
            continue;
         }
         if (!p->at_eof)
         {
            for (i = 0; i < p->nslots; i++)
               if (retro_atomic_load_acquire_int(&p->slot[i].state) == SLOT_EMPTY)
                  break;
            if (i < p->nslots)
            {
               retro_eventcount_cancel_wait(&p->wake);
               continue;
            }
         }
         retro_eventcount_commit_wait(&p->wake, key);
         continue;
      }

      s->off = p->next_off;
      s->gen = p->thread_gen;
      n      = p->fetch(p->user, s->off, s->buf, p->window);
      if (n < 0)
      {
         /* a failed fetch: give the slot back; the reader's own read
          * will report the failure, and this thread tries again later */
         retro_atomic_store_release_int(&s->state, SLOT_EMPTY);
         p->at_eof = 1;              /* stop hammering; a seek restarts */
         continue;
      }
      s->len = (size_t)n;
      s->eof = (size_t)n < p->window;
      if (s->eof)
         p->at_eof = 1;
      p->next_off += (uint64_t)n;
      retro_atomic_store_release_int(&p->next_idx, (int)(p->next_off / p->window));
      retro_atomic_store_release_int(&s->state, SLOT_READY);
   }
}

struct vfs_prefetch *vfs_prefetch_new(vfs_prefetch_fetch_t fetch, void *user,
      size_t window, unsigned slots, uint64_t start)
{
   struct vfs_prefetch *p;
   unsigned i;

   if (!fetch || !window || slots < 2)
      return NULL;
   if (!(p = (struct vfs_prefetch*)calloc(1, sizeof(*p))))
      return NULL;
   p->fetch  = fetch;
   p->user   = user;
   p->window = window;
   p->nslots = slots;
   if (!(p->slot = (struct vfs_prefetch_slot*)calloc(slots, sizeof(*p->slot))))
   {
      free(p);
      return NULL;
   }
   for (i = 0; i < slots; i++)
   {
      if (!(p->slot[i].buf = (uint8_t*)malloc(window)))
      {
         while (i--)
            free(p->slot[i].buf);
         free(p->slot);
         free(p);
         return NULL;
      }
      retro_atomic_store_release_int(&p->slot[i].state, SLOT_EMPTY);
   }
   retro_atomic_store_release_int(&p->target_idx, (int)(start / window));
   /* gen 1 with the target set: the thread sees a retarget and starts there */
   p->thread_gen = 0;
   retro_atomic_store_release_int(&p->gen, 1);
   retro_atomic_store_release_int(&p->next_idx, (int)(start / window));
   retro_atomic_store_release_int(&p->stop, 0);
   if (!retro_eventcount_init(&p->wake))
   {
      for (i = 0; i < slots; i++)
         free(p->slot[i].buf);
      free(p->slot);
      free(p);
      return NULL;
   }
   if (!(p->thread = sthread_create(vfs_prefetch_thread, p)))
   {
      retro_eventcount_free(&p->wake);
      for (i = 0; i < slots; i++)
         free(p->slot[i].buf);
      free(p->slot);
      free(p);
      return NULL;
   }
   return p;
}

int64_t vfs_prefetch_read(struct vfs_prefetch *p, uint64_t off, uint8_t *buf, size_t len)
{
   int      gen;
   unsigned i;
   size_t   done = 0;
   int      freed = 0;

   if (!p)
      return -1;
   gen = retro_atomic_load_acquire_int(&p->gen);

   /* recycle the windows the reader has moved past, and any left from
    * a stream it seeked away from */
   for (i = 0; i < p->nslots; i++)
   {
      struct vfs_prefetch_slot *s = &p->slot[i];
      if (retro_atomic_load_acquire_int(&s->state) == SLOT_READY
            && (s->gen != gen || (!s->eof && s->off + s->len <= off)))
      {
         if (retro_atomic_cas_int(&s->state, SLOT_READY, SLOT_EMPTY))
            freed = 1;
      }
   }
   if (freed)
      retro_eventcount_notify(&p->wake);

   /* copy from ready windows covering @off, across as many as touch */
   while (done < len)
   {
      struct vfs_prefetch_slot *hit = NULL;
      uint64_t at = off + done;
      for (i = 0; i < p->nslots; i++)
      {
         struct vfs_prefetch_slot *s = &p->slot[i];
         if (retro_atomic_load_acquire_int(&s->state) == SLOT_READY
               && s->gen == gen && at >= s->off && at < s->off + s->len)
         {
            hit = s;
            break;
         }
         /* a ready window that says the file ends before @at */
         if (retro_atomic_load_acquire_int(&s->state) == SLOT_READY
               && s->gen == gen && s->eof && at >= s->off + s->len)
            return (int64_t)done;
      }
      if (!hit)
      {
         /* a miss: if the thread is behind or elsewhere, it starts over
          * here rather than fetching what the reader pulls itself */
         if (!done)
            vfs_prefetch_seek(p, off);
         break;
      }
      {
         size_t take = (size_t)(hit->off + hit->len - at);
         if (take > len - done)
            take = len - done;
         memcpy(buf + done, hit->buf + (at - hit->off), take);
         done += take;
         if (hit->eof && at + take >= hit->off + hit->len)
            return (int64_t)done;
      }
   }
   return done ? (int64_t)done : -1;
}

void vfs_prefetch_seek(struct vfs_prefetch *p, uint64_t off)
{
   int      gen;
   unsigned i;
   int      next;

   if (!p)
      return;
   gen = retro_atomic_load_acquire_int(&p->gen);
   /* covered by a ready window: nothing to do */
   for (i = 0; i < p->nslots; i++)
   {
      struct vfs_prefetch_slot *s = &p->slot[i];
      if (retro_atomic_load_acquire_int(&s->state) == SLOT_READY
            && s->gen == gen && off >= s->off && off < s->off + s->len)
         return;
   }
   /* about to be fetched (the window being filled or the next one) */
   next = retro_atomic_load_acquire_int(&p->next_idx);
   if (off / p->window + 1 >= (uint64_t)next && off / p->window <= (uint64_t)next + 1)
      return;
   retro_atomic_store_release_int(&p->target_idx, (int)(off / p->window));
   retro_atomic_store_release_int(&p->gen, gen + 1);
   /* the old stream's ready windows go back now, so the thread has
    * room to start; ones still filling come back through read() */
   for (i = 0; i < p->nslots; i++)
   {
      struct vfs_prefetch_slot *s = &p->slot[i];
      if (retro_atomic_load_acquire_int(&s->state) == SLOT_READY)
         retro_atomic_cas_int(&s->state, SLOT_READY, SLOT_EMPTY);
   }
   retro_eventcount_notify(&p->wake);
}

void vfs_prefetch_free(struct vfs_prefetch *p)
{
   unsigned i;
   if (!p)
      return;
   retro_atomic_store_release_int(&p->stop, 1);
   retro_eventcount_notify(&p->wake);
   sthread_join(p->thread);
   retro_eventcount_free(&p->wake);
   for (i = 0; i < p->nslots; i++)
      free(p->slot[i].buf);
   free(p->slot);
   free(p);
}

#else /* !HAVE_THREADS */

struct vfs_prefetch *vfs_prefetch_new(vfs_prefetch_fetch_t fetch, void *user,
      size_t window, unsigned slots, uint64_t start)
{
   (void)fetch; (void)user; (void)window; (void)slots; (void)start;
   return NULL;
}

int64_t vfs_prefetch_read(struct vfs_prefetch *p, uint64_t off, uint8_t *buf, size_t len)
{
   (void)p; (void)off; (void)buf; (void)len;
   return -1;
}

void vfs_prefetch_seek(struct vfs_prefetch *p, uint64_t off)
{
   (void)p; (void)off;
}

void vfs_prefetch_free(struct vfs_prefetch *p)
{
   (void)p;
}

#endif
