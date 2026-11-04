/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vfs_cfg_snapshot.h).
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_VFS_CFG_SNAPSHOT_H__
#define __LIBRETRO_SDK_VFS_CFG_SNAPSHOT_H__

/* A network backend's settings, published as copies that never change.
 *
 * The menu thread hands new settings over while pool, prefetch and task
 * threads read them, some across a connect. A writer builds a whole new
 * copy and swaps it in; a reader enters, uses the copy it got until it
 * leaves, and the copy it replaced is freed once nothing holds it, by
 * whichever side is last. Nothing waits on a reader.
 *
 * Where the pointer atomics are lock-free and there are threads, entry
 * and leave are retro_retire.h's epochs. Elsewhere each copy counts its
 * holders under a flag held for a few loads and stores, never across a
 * call, as x509.c's store does.
 *
 * A copy is one allocation starting with a vfs_cfg_hdr_t, freed with
 * free(). A zero-filled vfs_cfg_slot_t is ready. */

#include <stdlib.h>

#include <retro_atomic.h>
#include <retro_inline.h>
#include <queues/retro_retire.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

RETRO_BEGIN_DECLS

#if defined(HAVE_THREADS) && defined(RETRO_RETIRE_LOCK_FREE)
#define VFS_CFG_LOCK_FREE 1
#endif

typedef struct vfs_cfg_hdr
{
   mpsc_stack_node_t node;      /* lock-free: on the retire list */
   unsigned          refs;      /* otherwise, under the flag: the
                                 * slot's own plus each holder's */
} vfs_cfg_hdr_t;

typedef struct vfs_cfg_slot
{
#ifdef VFS_CFG_LOCK_FREE
   retro_atomic_ptr_t cur;      /* vfs_cfg_hdr_t * */
   retro_retire_t     retired;
#else
   vfs_cfg_hdr_t     *cur;
   retro_atomic_int_t busy;
#endif
} vfs_cfg_slot_t;

#ifdef VFS_CFG_LOCK_FREE
static void vfs_cfg_free_node(mpsc_stack_node_t *node)
{
   free(node);
}

/* The published copy (NULL: none), valid until vfs_cfg_leave(). */
static INLINE vfs_cfg_hdr_t *vfs_cfg_enter(vfs_cfg_slot_t *s)
{
   retro_retire_enter(&s->retired);
   return (vfs_cfg_hdr_t*)retro_atomic_load_acquire_ptr(&s->cur);
}

static INLINE void vfs_cfg_leave(vfs_cfg_slot_t *s, vfs_cfg_hdr_t *h)
{
   (void)h;
   retro_retire_leave(&s->retired, vfs_cfg_free_node);
}

/* @h (NULL: none) replaces what is published; its payload is written. */
static INLINE void vfs_cfg_publish(vfs_cfg_slot_t *s, vfs_cfg_hdr_t *h)
{
   vfs_cfg_hdr_t *old;
   do
      old = (vfs_cfg_hdr_t*)retro_atomic_load_acquire_ptr(&s->cur);
   while (!retro_atomic_cas_ptr(&s->cur, old, h));
   retro_retire(&s->retired, old ? &old->node : NULL, vfs_cfg_free_node);
}
#else
static INLINE void vfs_cfg_lock(vfs_cfg_slot_t *s)
{
#ifdef HAVE_THREADS
   while (!retro_atomic_cas_int(&s->busy, 0, 1))
      sthread_yield();
#else
   (void)s;
#endif
}

static INLINE void vfs_cfg_unlock(vfs_cfg_slot_t *s)
{
#ifdef HAVE_THREADS
   retro_atomic_store_release_int(&s->busy, 0);
#else
   (void)s;
#endif
}

/* The published copy (NULL: none), valid until vfs_cfg_leave(). */
static INLINE vfs_cfg_hdr_t *vfs_cfg_enter(vfs_cfg_slot_t *s)
{
   vfs_cfg_hdr_t *h;
   vfs_cfg_lock(s);
   if ((h = s->cur))
      h->refs++;
   vfs_cfg_unlock(s);
   return h;
}

static INLINE void vfs_cfg_leave(vfs_cfg_slot_t *s, vfs_cfg_hdr_t *h)
{
   unsigned left;
   if (!h)
      return;
   vfs_cfg_lock(s);
   left = --h->refs;
   vfs_cfg_unlock(s);
   if (!left)
      free(h);
}

/* @h (NULL: none) replaces what is published; its payload is written. */
static INLINE void vfs_cfg_publish(vfs_cfg_slot_t *s, vfs_cfg_hdr_t *h)
{
   vfs_cfg_hdr_t *old;
   unsigned       left = 1;
   if (h)
      h->refs = 1;
   vfs_cfg_lock(s);
   old    = s->cur;
   s->cur = h;
   if (old)
      left = --old->refs;
   vfs_cfg_unlock(s);
   if (old && !left)
      free(old);
}
#endif

RETRO_END_DECLS

#endif
