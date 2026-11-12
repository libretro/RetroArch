/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_cas_record.h).
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

#ifndef __LIBRETRO_SDK_CAS_RECORD_H
#define __LIBRETRO_SDK_CAS_RECORD_H

/* retro_cas_record.h - a record any thread reads and any thread
 * changes, with no lock.
 *
 * retro_seqlock.h is for one writer.  This is for several: where a
 * mutex would otherwise make read-change-write of a small record one
 * step.  Nobody writes the record in place.  A change is made on a
 * copy in a spare slot and put in the record's place with one
 * compare-and-swap; if another thread got there first the swap fails
 * and the change is made again on the newer record.  One of the
 * threads racing always gets through, so none can hold the others up,
 * whatever becomes of it (preempted, stopped, killed).
 *
 *   read:
 *      retro_cas_record_read(&rec, &copy);
 *
 *   change (say, count a hit and note the time):
 *      do
 *      {
 *         status_t *s = (status_t*)retro_cas_record_begin(&rec, &txn);
 *         s->hits++;
 *         s->last = now;
 *      } while (!retro_cas_record_commit(&rec, &txn));
 *
 *   read, work on the copy, and put it back only if it changed (most
 *   reads change nothing and then write nothing shared at all):
 *      for (;;)
 *      {
 *         version = retro_cas_record_read_versioned(&rec, &copy);
 *         if (!work(&copy))
 *            break;                               (nothing to put back)
 *         if (retro_cas_record_replace(&rec, version, &copy))
 *            break;
 *      }
 *
 *   look, and change only if need be:
 *      s = (status_t*)retro_cas_record_begin(&rec, &txn);
 *      if (s->flag)
 *      {
 *         retro_cas_record_abort(&rec, &txn);    (nothing changes)
 *         ...
 *
 * What a reader gets is always one whole record, as some commit left
 * it; two changes never mix, and none is lost.
 *
 * Rules:
 *   - What is done between begin and commit is done again when the
 *     commit fails: it only works on the copy and on what the caller
 *     can overwrite.  Anything else (waking a thread, telling a device)
 *     waits for the commit to succeed.
 *   - The record is plain data: it is copied with memcpy and may hold
 *     no pointer to itself.
 *   - A slot is copied while another thread may be reusing it, as
 *     plain memory, and the copy is thrown away when that happened
 *     (the head moved): what seqlocks rely on.  A race detector flags
 *     those reads.
 *   - A thread holds one slot from begin to commit or abort, and the
 *     record itself holds one: RETRO_CAS_RECORD_SLOTS - 1 threads can
 *     be in a change of one record at once.  One more spins until a
 *     slot is given back.
 *   - The storage is the caller's: RETRO_CAS_RECORD_SLOTS records in a
 *     row, the first holding the record's first value.
 *
 * Header only.  Needs lock-free atomics with compare-and-swap
 * (RETRO_CAS_RECORD_LOCK_FREE says they are there); elsewhere the
 * caller keeps its own serialisation.
 */

#include <stdint.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_inline.h>

#if defined(RETRO_ATOMIC_HAS_CAS) && defined(RETRO_ATOMIC_LOCK_FREE)
#define RETRO_CAS_RECORD_LOCK_FREE 1

#define RETRO_CAS_RECORD_SLOTS 16

/* The head: which slot is the record, under a count of the commits so
 * that a head seen twice the same is the same record.  64 bits where
 * there are 64-bit atomics; elsewhere the count wraps after 2^27
 * commits, and a thread would have to sleep through exactly that many
 * to be fooled. */
#if defined(RETRO_ATOMIC_HAS_64)
typedef retro_atomic_64_t retro_cas_record_head_t;
typedef int64_t retro_cas_record_word_t;
typedef uint64_t retro_cas_record_uword_t;
#define RETRO_CAS_RECORD_HEAD_INIT(p, v) retro_atomic_64_init((p), (v))
#define RETRO_CAS_RECORD_HEAD_LOAD(p)    retro_atomic_load_acquire_64(p)
#define RETRO_CAS_RECORD_HEAD_CAS(p, o, n) retro_atomic_cas_64((p), (o), (n))
#else
typedef retro_atomic_int_t retro_cas_record_head_t;
typedef int retro_cas_record_word_t;
typedef unsigned retro_cas_record_uword_t;
#define RETRO_CAS_RECORD_HEAD_INIT(p, v) retro_atomic_int_init((p), (v))
#define RETRO_CAS_RECORD_HEAD_LOAD(p)    retro_atomic_load_acquire_int(p)
#define RETRO_CAS_RECORD_HEAD_CAS(p, o, n) retro_atomic_cas_int((p), (o), (n))
#endif

typedef struct retro_cas_record
{
   retro_cas_record_head_t head; /* commits << 4 | the record's slot */
   retro_atomic_int_t busy;      /* a bit a slot: the record's, and those
                                  * changes are being made in */
   unsigned char *slots;
   size_t size;
} retro_cas_record_t;

/* A change under way: from begin to commit or abort, one thread's. */
typedef struct retro_cas_record_txn
{
   retro_cas_record_word_t seen; /* the head the copy was made from */
   unsigned slot;                /* where the copy is */
} retro_cas_record_txn_t;

/* @storage: RETRO_CAS_RECORD_SLOTS * @size bytes, the first @size of
 * them the record's first value (now, or written before any other
 * thread has the record). */
static INLINE void retro_cas_record_init(retro_cas_record_t *rec,
      void *storage, size_t size)
{
   rec->slots = (unsigned char*)storage;
   rec->size  = size;
   RETRO_CAS_RECORD_HEAD_INIT(&rec->head, 0);
   retro_atomic_int_init(&rec->busy, 1);
}

/* Copies the record as it is into @out. Never waits on a change. */
static INLINE void retro_cas_record_read(retro_cas_record_t *rec, void *out)
{
   for (;;)
   {
      retro_cas_record_word_t head = RETRO_CAS_RECORD_HEAD_LOAD(&rec->head);
      memcpy(out, rec->slots + (size_t)(head & 15) * rec->size, rec->size);
      /* The copy's loads before the second look at the head */
      retro_atomic_thread_fence_acquire();
      if (RETRO_CAS_RECORD_HEAD_LOAD(&rec->head) == head)
         return;
   }
}

/* Copies the record as it is into @out, as retro_cas_record_read does.
 * @return which record that was, for retro_cas_record_replace. */
static INLINE retro_cas_record_word_t retro_cas_record_read_versioned(
      retro_cas_record_t *rec, void *out)
{
   for (;;)
   {
      retro_cas_record_word_t head = RETRO_CAS_RECORD_HEAD_LOAD(&rec->head);
      memcpy(out, rec->slots + (size_t)(head & 15) * rec->size, rec->size);
      retro_atomic_thread_fence_acquire();
      if (RETRO_CAS_RECORD_HEAD_LOAD(&rec->head) == head)
         return head;
   }
}

/* A spare slot for this thread: the lowest free bit, taken if nobody
 * else took it meanwhile. */
static INLINE unsigned retro_cas_record_take_slot_(retro_cas_record_t *rec)
{
   unsigned slot;
   for (;;)
   {
      unsigned busy = (unsigned)retro_atomic_load_acquire_int(&rec->busy);
      for (slot = 0; slot < RETRO_CAS_RECORD_SLOTS; slot++)
         if (!(busy & (1u << slot)))
            break;
      if (slot == RETRO_CAS_RECORD_SLOTS)
         continue;
      if (!((unsigned)retro_atomic_fetch_or_int(&rec->busy, (int)(1u << slot))
               & (1u << slot)))
         return slot;
   }
}

/* Starts a change.
 * @return a copy of the record to change in place; the caller's until
 * commit or abort. */
static INLINE void *retro_cas_record_begin(retro_cas_record_t *rec,
      retro_cas_record_txn_t *txn)
{
   unsigned slot       = retro_cas_record_take_slot_(rec);
   unsigned char *copy = rec->slots + (size_t)slot * rec->size;
   for (;;)
   {
      retro_cas_record_word_t head = RETRO_CAS_RECORD_HEAD_LOAD(&rec->head);
      memcpy(copy, rec->slots + (size_t)(head & 15) * rec->size, rec->size);
      retro_atomic_thread_fence_acquire();
      if (RETRO_CAS_RECORD_HEAD_LOAD(&rec->head) == head)
      {
         txn->seen = head;
         txn->slot = slot;
         return copy;
      }
   }
}

/* Gives the change up: the record stays as it is. */
static INLINE void retro_cas_record_abort(retro_cas_record_t *rec,
      retro_cas_record_txn_t *txn)
{
   retro_atomic_fetch_and_int(&rec->busy, (int)~(1u << txn->slot));
}

/* Puts the changed copy in the record's place.
 * @return false if the record changed since begin: nothing was done,
 * and the change is to be made again from begin. */
static INLINE bool retro_cas_record_commit(retro_cas_record_t *rec,
      retro_cas_record_txn_t *txn)
{
   /* The count up by one (it wraps, unsigned), the new slot under it */
   retro_cas_record_word_t next = (retro_cas_record_word_t)(
         (((retro_cas_record_uword_t)txn->seen | 15) + 1) | txn->slot);
   if (RETRO_CAS_RECORD_HEAD_CAS(&rec->head, txn->seen, next))
   {
      /* The slot the record was in is spare now */
      retro_atomic_fetch_and_int(&rec->busy,
            (int)~(1u << (unsigned)(txn->seen & 15)));
      return true;
   }
   retro_atomic_fetch_and_int(&rec->busy, (int)~(1u << txn->slot));
   return false;
}

/* Puts @value in the record's place, if the record is still the one
 * retro_cas_record_read_versioned gave as @version.
 * @return false if the record changed since: nothing was done, and the
 * caller reads it again. */
static INLINE bool retro_cas_record_replace(retro_cas_record_t *rec,
      retro_cas_record_word_t version, const void *value)
{
   retro_cas_record_txn_t txn;
   /* Changed already: not worth a slot */
   if (RETRO_CAS_RECORD_HEAD_LOAD(&rec->head) != version)
      return false;
   txn.seen = version;
   txn.slot = retro_cas_record_take_slot_(rec);
   memcpy(rec->slots + (size_t)txn.slot * rec->size, value, rec->size);
   return retro_cas_record_commit(rec, &txn);
}

#endif /* lock-free */

#endif
