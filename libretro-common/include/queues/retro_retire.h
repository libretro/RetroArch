/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------
 * The following license statement only applies to this file (retro_retire.h).
 * ---------------------------------------------------------------------
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

#ifndef __LIBRETRO_SDK_RETRO_RETIRE_H__
#define __LIBRETRO_SDK_RETRO_RETIRE_H__

/* retro_retire.h - read shared objects that another thread replaces,
 * with no lock on either side.
 *
 * A reader enters, loads the published pointers it wants and uses what
 * they point at, then leaves.  A writer swaps a new object in and
 * retires the old one; the old one is freed once no reader is in, by
 * whichever thread is last out, so nothing waits on anything.
 *
 * A reader counts itself in and then reads an epoch that a writer bumps
 * once its swap is made, both sequentially consistent: so the reader
 * loads the new object, or the drain that would free the old one sees
 * the reader counted.  Readers that never all leave at once hold the
 * retired objects back; they are freed at the next moment none is in.
 *
 * Only where the pointer atomics are lock-free (RETRO_RETIRE_LOCK_FREE);
 * elsewhere the caller keeps its own serialisation.  A zero-filled
 * retro_retire_t is ready, so a static one needs no setup. */

#include <retro_atomic.h>
#include <retro_inline.h>
#include <queues/mpsc_stack.h>

RETRO_BEGIN_DECLS

#if defined(RETRO_ATOMIC_HAS_PTR) && defined(RETRO_ATOMIC_LOCK_FREE)
#define RETRO_RETIRE_LOCK_FREE 1

typedef struct retro_retire
{
   mpsc_stack_t       retired;
   retro_atomic_int_t readers;
   retro_atomic_int_t epoch;
} retro_retire_t;

/* Frees one retired object, given its node. */
typedef void (*retro_retire_free_t)(mpsc_stack_node_t *node);

/* Frees what was retired while no reader is in. One that is gets it
 * back and frees it on its way out. */
static INLINE void retro_retire_drain(retro_retire_t *r,
      retro_retire_free_t fn)
{
   for (;;)
   {
      mpsc_stack_node_t *list;
      if (retro_atomic_load_seq_cst_int(&r->readers))
         return;
      if (!(list = mpsc_stack_drain(&r->retired)))
         return;
      if (retro_atomic_load_seq_cst_int(&r->readers))
      {
         while (list)
         {
            mpsc_stack_node_t *next = list->next;
            mpsc_stack_push(&r->retired, list);
            list = next;
         }
         continue;
      }
      while (list)
      {
         mpsc_stack_node_t *next = list->next;
         fn(list);
         list = next;
      }
   }
}

/* What is published may be loaded, and used, until the leave. */
static INLINE void retro_retire_enter(retro_retire_t *r)
{
   retro_atomic_fetch_add_seq_cst_int(&r->readers, 1);
   (void)retro_atomic_load_seq_cst_int(&r->epoch);
}

static INLINE void retro_retire_leave(retro_retire_t *r,
      retro_retire_free_t fn)
{
   if (retro_atomic_fetch_add_seq_cst_int(&r->readers, -1) == 1)
      retro_retire_drain(r, fn);
}

/* After a swap: @old (NULL: nothing) is freed once no reader is in. */
static INLINE void retro_retire(retro_retire_t *r, mpsc_stack_node_t *old,
      retro_retire_free_t fn)
{
   retro_atomic_fetch_add_seq_cst_int(&r->epoch, 1);
   if (!old)
      return;
   mpsc_stack_push(&r->retired, old);
   retro_retire_drain(r, fn);
}
#endif

RETRO_END_DECLS

#endif
