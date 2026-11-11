/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_seqlock.h).
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

#ifndef __LIBRETRO_SDK_SEQLOCK_H
#define __LIBRETRO_SDK_SEQLOCK_H

/* retro_seqlock.h - a record one thread updates and any thread reads.
 *
 * The writer never waits: it bumps a sequence word to odd, writes the
 * record in place and bumps it back to even.  A reader copies the
 * record out between two loads of the word and copies again if the
 * word was odd or moved, so it never sees a half-written record and
 * never holds the writer up.  It suits a status record (counters, time
 * stamps) that the writer updates often and readers sample.
 *
 *   writer:
 *      retro_seqlock_write_begin(&lock);
 *      status.count++;
 *      status.time = now;
 *      retro_seqlock_write_end(&lock);
 *
 *   reader:
 *      do
 *      {
 *         seq  = retro_seqlock_read_begin(&lock);
 *         copy = status;
 *      } while (retro_seqlock_read_retry(&lock, seq));
 *
 * Rules:
 *   - One writer at a time.  Writers that can overlap need their own
 *     arrangement (one thread owning the record, say).
 *   - A reader only copies inside the loop and uses the copy after it:
 *     what it reads before the retry check may be torn.  Nothing read
 *     there may be dereferenced.
 *   - The record is read while it may be written, as plain memory;
 *     the fences order the sequence word around it, which is what
 *     seqlocks rely on everywhere (the Linux kernel's among them).  A
 *     race detector flags those reads.
 *   - A reader that comes in while a write is under way spins until it
 *     is done: a write should be a handful of stores.
 *
 * Header only.  Needs lock-free atomics (RETRO_ATOMIC_LOCK_FREE) to be
 * of use between threads.
 */

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_inline.h>

typedef struct retro_seqlock
{
   retro_atomic_int_t seq; /* odd while a write is under way */
} retro_seqlock_t;

static INLINE void retro_seqlock_init(retro_seqlock_t *lock)
{
   retro_atomic_int_init(&lock->seq, 0);
}

/* Writer: before writing the record. */
static INLINE void retro_seqlock_write_begin(retro_seqlock_t *lock)
{
   retro_atomic_store_relaxed_int(&lock->seq,
         retro_atomic_load_relaxed_int(&lock->seq) + 1);
   /* The odd word before any of the record's stores */
   retro_atomic_thread_fence_release();
}

/* Writer: after writing the record. */
static INLINE void retro_seqlock_write_end(retro_seqlock_t *lock)
{
   /* The record's stores before the even word */
   retro_atomic_store_release_int(&lock->seq,
         retro_atomic_load_relaxed_int(&lock->seq) + 1);
}

/* Reader: before copying.  Waits out a write under way.
 * @return the key for retro_seqlock_read_retry. */
static INLINE int retro_seqlock_read_begin(retro_seqlock_t *lock)
{
   int seq;
   while ((seq = retro_atomic_load_acquire_int(&lock->seq)) & 1)
      ;
   return seq;
}

/* Reader: after copying.
 * @return true if a write overlapped the copy, which must then be
 * made again. */
static INLINE bool retro_seqlock_read_retry(retro_seqlock_t *lock, int seq)
{
   /* The record's loads before the second look at the word */
   retro_atomic_thread_fence_acquire();
   return retro_atomic_load_relaxed_int(&lock->seq) != seq;
}

#endif
