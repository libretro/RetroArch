/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (retro_waitaddr.c).
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

#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>
#include <rthreads/retro_waitaddr.h>

/* See retro_waitaddr.h for what this is.  How it holds together:
 *
 * A bucket's word carries two numbers.  The low WA_COUNT_BITS count the
 * records registered in the bucket; the rest is an epoch a notifier
 * bumps.  Both sides change it with one sequentially-consistent
 * read-modify-write, and that is the whole of the protocol between a
 * waiter that is on its way to sleep and a notifier that finds the
 * bucket empty:
 *
 *   waiter                         notifier (after its store)
 *   ------                         --------
 *   word += 1, epoch read          old = word; word += 1 epoch
 *   record published               count in old zero?  done
 *   own eventcount prepared        else: claim and wake the records
 *   words checked                        that name the address
 *   bucket epochs checked again
 *   sleep on own eventcount
 *
 * If the notifier's bump comes before the waiter's second look at the
 * epoch, the waiter sees it and does not sleep.  If it comes after,
 * the waiter's registration came before it, so the notifier reads a
 * count that is not zero, finds the record and notifies its eventcount,
 * which the waiter prepared before that second look - so the commit
 * returns.  Neither order loses the wake, and none of it leans on how
 * the caller stored the word.
 *
 * A record's state is a generation and two bits: free, owned, armed,
 * claimed.  A notifier wakes a record only by moving it from armed to
 * claimed with a compare-and-swap on the whole state, generation
 * included, after reading the addresses the record names.  The owner
 * writes those only while the record is not armed and bumps the
 * generation when it arms it, so a swap that succeeds proves the
 * addresses read were this wait's, however the record was recycled in
 * between; and one that succeeds is what "woke one waiter" means for
 * notify_one.  The wake itself is a notify of the record's eventcount,
 * which is lost on a waiter that has not prepared it yet - so a waiter
 * looks at its own state once more after preparing, and does not sleep
 * on a record that is claimed.  Records are never returned to the allocator while
 * anything runs, so a stale pointer read out of a slot is still a
 * record.
 */

#if defined(RETRO_ATOMIC_HAS_PTR) && defined(HAVE_THREADS)

#define WA_BUCKET_BITS   12
#define WA_BUCKETS       (1 << WA_BUCKET_BITS)
#define WA_SLOTS         7
#define WA_COUNT_BITS    12
#define WA_COUNT_MASK    ((1 << WA_COUNT_BITS) - 1)
#define WA_EPOCH_ONE     (1 << WA_COUNT_BITS)

#define WA_CHUNKS        64
#define WA_PER_CHUNK     64

/* The two low bits of a record's state */
#define WA_FREE          0
#define WA_OWNED         1
#define WA_ARMED         2
#define WA_CLAIMED       3

/* A wait that found no free slot in a bucket naps for this long
 * instead of parking: the notifier would not find it */
#define WA_OVERFLOW_NAP_US 200

struct wa_bucket
{
   retro_atomic_int_t word;
   retro_atomic_ptr_t slots[WA_SLOTS];
};

struct wa_record
{
   retro_eventcount_t ec;
   retro_atomic_int_t state;
   retro_atomic_ptr_t addrs[RETRO_WAITADDR_MAX];
};

static struct wa_bucket   wa_buckets[WA_BUCKETS];
static retro_atomic_ptr_t wa_chunks[WA_CHUNKS];

static struct wa_bucket *wa_bucket_of(const void *addr)
{
   /* Words are four bytes apart at least; Fibonacci hashing on the rest */
   uint32_t h = (uint32_t)((uintptr_t)addr >> 2) * 2654435769u;
   return &wa_buckets[h >> (32 - WA_BUCKET_BITS)];
}

static struct wa_record *wa_chunk_get(unsigned c)
{
   struct wa_record *chunk = (struct wa_record*)
      retro_atomic_load_acquire_ptr(&wa_chunks[c]);
   unsigned i;

   if (chunk)
      return chunk;

   chunk = (struct wa_record*)calloc(WA_PER_CHUNK, sizeof(*chunk));
   if (!chunk)
      return NULL;

   for (i = 0; i < WA_PER_CHUNK; i++)
   {
      if (!retro_eventcount_init(&chunk[i].ec))
         break;
   }

   if (i == WA_PER_CHUNK && retro_atomic_cas_ptr(&wa_chunks[c], NULL, chunk))
      return chunk;

   /* Another thread made this chunk first, or an eventcount would not
    * come up */
   while (i--)
      retro_eventcount_free(&chunk[i].ec);
   free(chunk);
   return (struct wa_record*)retro_atomic_load_acquire_ptr(&wa_chunks[c]);
}

/* A record of the caller's own until wa_release.  The search starts
 * where the thread's stack says, so a thread finds the record it had
 * last time with one compare-and-swap. */
static struct wa_record *wa_acquire(void)
{
   int      here;
   unsigned start = (unsigned)((uintptr_t)&here >> 14);
   unsigned n;

   for (n = 0; n < WA_CHUNKS * WA_PER_CHUNK; n++)
   {
      unsigned at = (start + n) % (WA_CHUNKS * WA_PER_CHUNK);
      struct wa_record *chunk = wa_chunk_get(at / WA_PER_CHUNK);
      struct wa_record *rec;
      int state;

      if (!chunk)
         return NULL;

      rec   = &chunk[at % WA_PER_CHUNK];
      state = retro_atomic_load_acquire_int(&rec->state);
      if ((state & 3) == WA_FREE
            && retro_atomic_cas_int(&rec->state, state, state | WA_OWNED))
         return rec;
   }

   return NULL;
}

bool retro_waitaddr_wait(const retro_waitaddr_t *list, unsigned count,
      int64_t timeout_us)
{
   struct wa_bucket   *buckets[RETRO_WAITADDR_MAX];
   retro_atomic_ptr_t *slots[RETRO_WAITADDR_MAX];
   int                 epochs[RETRO_WAITADDR_MAX];
   struct wa_record   *rec;
   unsigned            i, s;
   int                 state, key;
   bool                changed  = false;
   bool                overflow = false;
   bool                notified = true;

   if (!count || count > RETRO_WAITADDR_MAX)
      return true;

   /* A word that already differs needs none of what follows */
   for (i = 0; i < count; i++)
   {
      if ((uint32_t)retro_atomic_load_acquire_int(
               (retro_atomic_int_t*)list[i].addr) != list[i].old)
         return true;
   }

   if (!(rec = wa_acquire()))
      return true;

   /* The addresses, then armed under a new generation: a notifier that
    * read an older generation's addresses cannot claim this one */
   for (i = 0; i < RETRO_WAITADDR_MAX; i++)
      retro_atomic_store_release_ptr(&rec->addrs[i],
            i < count ? (void*)list[i].addr : NULL);
   state = retro_atomic_load_relaxed_int(&rec->state);
   state = (int)((((unsigned)state >> 2) + 1) << 2) | WA_ARMED;
   retro_atomic_store_release_int(&rec->state, state);

   /* Registered in each address's bucket */
   for (i = 0; i < count; i++)
   {
      buckets[i] = wa_bucket_of(list[i].addr);
      slots[i]   = NULL;

      for (s = 0; s < WA_SLOTS; s++)
      {
         if (retro_atomic_cas_ptr(&buckets[i]->slots[s], NULL, rec))
         {
            slots[i] = &buckets[i]->slots[s];
            break;
         }
      }

      if (!slots[i])
         overflow = true;

      epochs[i] = (int)((unsigned)retro_atomic_fetch_add_int(
               &buckets[i]->word, 1) >> WA_COUNT_BITS);
   }

   key = retro_eventcount_prepare_wait(&rec->ec);

   for (i = 0; i < count && !changed; i++)
   {
      if ((uint32_t)retro_atomic_load_acquire_int(
               (retro_atomic_int_t*)list[i].addr) != list[i].old)
         changed = true;
   }

   /* A notify since this wait registered: it may have found the bucket
    * empty */
   for (i = 0; i < count && !changed; i++)
   {
      if ((int)((unsigned)retro_atomic_load_acquire_int(&buckets[i]->word)
               >> WA_COUNT_BITS) != epochs[i])
         changed = true;
   }

   /* Claimed already: the notifier's wake of the eventcount may have come
    * before it was prepared, and this is the only trace of it.  A claim
    * that comes after this look comes with a wake after the prepare. */
   if (!changed && (retro_atomic_load_acquire_int(&rec->state) & 3)
         == WA_CLAIMED)
      changed = true;

   if (changed)
      retro_eventcount_cancel_wait(&rec->ec);
   else if (overflow)
   {
      int64_t nap = WA_OVERFLOW_NAP_US;
      if (timeout_us >= 0 && timeout_us < nap)
         notified = retro_eventcount_commit_wait_timeout(&rec->ec, key,
               timeout_us);
      else
         retro_eventcount_commit_wait_timeout(&rec->ec, key, nap);
   }
   else if (timeout_us < 0)
      retro_eventcount_commit_wait(&rec->ec, key);
   else
      notified = retro_eventcount_commit_wait_timeout(&rec->ec, key,
            timeout_us);

   for (i = 0; i < count; i++)
   {
      if (slots[i])
         retro_atomic_store_release_ptr(slots[i], NULL);
      retro_atomic_fetch_sub_int(&buckets[i]->word, 1);
   }

   /* Free again, the generation kept: a notifier still holding this
    * record's old state fails its swap */
   retro_atomic_store_release_int(&rec->state, state & ~3);
   return notified;
}

static void wa_notify(const void *addr, bool all)
{
   struct wa_bucket *bucket = wa_bucket_of(addr);
   unsigned s;

   /* The epoch bumped and the count read in one step */
   if (!(retro_atomic_fetch_add_int(&bucket->word, WA_EPOCH_ONE)
            & WA_COUNT_MASK))
      return;

   for (s = 0; s < WA_SLOTS; s++)
   {
      struct wa_record *rec = (struct wa_record*)
         retro_atomic_load_acquire_ptr(&bucket->slots[s]);
      unsigned i;
      int state;

      if (!rec)
         continue;

      state = retro_atomic_load_acquire_int(&rec->state);
      if ((state & 3) != WA_ARMED)
         continue;

      for (i = 0; i < RETRO_WAITADDR_MAX; i++)
      {
         if (retro_atomic_load_acquire_ptr(&rec->addrs[i]) == addr)
            break;
      }

      if (i == RETRO_WAITADDR_MAX)
         continue;

      /* Armed to claimed: this wait's, and no other notifier's */
      if (!retro_atomic_cas_int(&rec->state, state, state | WA_CLAIMED))
         continue;

      retro_eventcount_notify(&rec->ec);

      if (!all)
         return;
   }
}

void retro_waitaddr_notify_one(const void *addr)
{
   wa_notify(addr, false);
}

void retro_waitaddr_notify_all(const void *addr)
{
   wa_notify(addr, true);
}

void retro_waitaddr_free_all(void)
{
   unsigned c, i;

   for (c = 0; c < WA_CHUNKS; c++)
   {
      struct wa_record *chunk = (struct wa_record*)
         retro_atomic_exchange_ptr(&wa_chunks[c], NULL);

      if (!chunk)
         continue;

      for (i = 0; i < WA_PER_CHUNK; i++)
         retro_eventcount_free(&chunk[i].ec);
      free(chunk);
   }

   memset(wa_buckets, 0, sizeof(wa_buckets));
}

#endif
