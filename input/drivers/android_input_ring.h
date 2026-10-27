/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _ANDROID_INPUT_RING_H
#define _ANDROID_INPUT_RING_H

#include <stddef.h>
#include <stdint.h>

#include <boolean.h>
#include <retro_inline.h>
#include <retro_spsc.h>

/* The input reader's ring (input/drivers/android_input.c): records of
 * the size each event needs, built by the reader where the poll reads
 * them, and applied by the poll where they lie. Nothing is copied
 * after the event is read from the input queue.
 *
 * A record is a multiple of 8 bytes and never crosses the ring's end:
 * where the next would not fit before it, the rest of the lap is a pad
 * record the poll steps over. The ring's capacity is a power of two,
 * so a header is never split.
 *
 * The reader builds a burst of records and publishes them together. A
 * record with a merge key replaces the one before it, while that one
 * is unpublished and has the same key: a stick's moves, whose last is
 * all the poll would keep. Records with no key (0) are never merged. */

typedef struct android_input_rec
{
   uint32_t size;   /* the whole record, a multiple of 8 */
   uint32_t merge;  /* 0, a merge key, or ANDROID_INPUT_REC_PAD */
} android_input_rec_t;

#define ANDROID_INPUT_REC_PAD  0xffffffffu
#define ANDROID_INPUT_REC_NONE ((size_t)-1)

static INLINE size_t android_input_rec_round(size_t bytes)
{
   return (bytes + 7) & ~(size_t)7;
}

/* ---- the reader's side ---- */

typedef struct android_input_ring_writer
{
   retro_spsc_t *q;
   uint8_t *span;   /* write_begin's, or NULL */
   size_t room;     /* its length */
   size_t used;     /* built in it, not yet published */
   size_t last;     /* the last record built, or ANDROID_INPUT_REC_NONE */
   size_t slot;     /* where the record being built is */
} android_input_ring_writer_t;

static INLINE void android_input_ring_writer_init(
      android_input_ring_writer_t *w, retro_spsc_t *q)
{
   w->q    = q;
   w->span = NULL;
   w->room = 0;
   w->used = 0;
   w->last = ANDROID_INPUT_REC_NONE;
   w->slot = 0;
}

/* What was built, to the poll */
static INLINE void android_input_ring_publish(android_input_ring_writer_t *w)
{
   if (w->span)
      retro_spsc_write_end(w->q, w->used);
   w->span = NULL;
   w->room = 0;
   w->used = 0;
   w->last = ANDROID_INPUT_REC_NONE;
}

/* Where to build a record of at most @max bytes with @merge: over the
 * last one built when it has the same key, else after it. NULL when
 * the ring has no room for @max; what was built is published then. */
static INLINE void *android_input_ring_slot(android_input_ring_writer_t *w,
      size_t max, uint32_t merge)
{
   void *ptr;

   if (     merge && w->last != ANDROID_INPUT_REC_NONE
         && ((android_input_rec_t*)(w->span + w->last))->merge == merge)
   {
      w->slot = w->last;
      return w->span + w->last;
   }
   if (!w->span || w->room - w->used < max)
   {
      android_input_ring_publish(w);
      w->room = retro_spsc_write_begin(w->q, &ptr);
      w->span = (uint8_t*)ptr;
      if (w->room < max)
      {
         /* at the end of the lap, with room after it: a pad to the end */
         if (     w->room
               && retro_spsc_write_avail(w->q) >= w->room + max)
         {
            android_input_rec_t *pad = (android_input_rec_t*)w->span;
            pad->size  = (uint32_t)w->room;
            pad->merge = ANDROID_INPUT_REC_PAD;
            retro_spsc_write_end(w->q, w->room);
            w->room    = retro_spsc_write_begin(w->q, &ptr);
            w->span    = (uint8_t*)ptr;
         }
         if (w->room < max)
         {
            if (w->span)
               retro_spsc_write_end(w->q, 0);
            w->span = NULL;
            w->room = 0;
            return NULL;
         }
      }
   }
   w->slot = w->used;
   return w->span + w->used;
}

/* The record at the last slot is built, @size bytes */
static INLINE void android_input_ring_built(android_input_ring_writer_t *w,
      size_t size, uint32_t merge)
{
   android_input_rec_t *rec = (android_input_rec_t*)(w->span + w->slot);
   rec->size  = (uint32_t)android_input_rec_round(size);
   rec->merge = merge;
   if (w->slot == w->used)
   {
      w->last  = w->used;
      w->used += rec->size;
   }
}

/* ---- the poll's side ---- */

typedef struct android_input_ring_reader
{
   retro_spsc_t *q;
   const uint8_t *span;
   size_t left;     /* read_begin's length */
   size_t done;     /* applied of it */
   bool took;       /* anything given back this poll */
} android_input_ring_reader_t;

static INLINE void android_input_ring_reader_begin(
      android_input_ring_reader_t *r, retro_spsc_t *q)
{
   const void *ptr;
   r->q    = q;
   r->left = retro_spsc_read_begin(q, &ptr);
   r->span = (const uint8_t*)ptr;
   r->done = 0;
   r->took = false;
}

/* The next record, where it lies, or NULL when there is none */
static INLINE const android_input_rec_t *android_input_ring_next(
      android_input_ring_reader_t *r)
{
   for (;;)
   {
      const android_input_rec_t *rec;
      if (r->left - r->done < sizeof(android_input_rec_t))
      {
         const void *ptr;
         if (!r->done)
            return NULL;
         /* the rest of the lap is done: on from the start */
         retro_spsc_read_end(r->q, r->done);
         r->took = true;
         r->left = retro_spsc_read_begin(r->q, &ptr);
         r->span = (const uint8_t*)ptr;
         r->done = 0;
         continue;
      }
      rec = (const android_input_rec_t*)(r->span + r->done);
      if (rec->merge != ANDROID_INPUT_REC_PAD)
         return rec;
      r->done += rec->size;
   }
}

/* The record android_input_ring_next() gave is applied */
static INLINE void android_input_ring_taken(android_input_ring_reader_t *r,
      const android_input_rec_t *rec)
{
   r->done += rec->size;
}

/* Gives back what was applied; true if anything was this poll */
static INLINE bool android_input_ring_reader_end(
      android_input_ring_reader_t *r)
{
   if (r->done)
   {
      retro_spsc_read_end(r->q, r->done);
      r->took = true;
   }
   r->done = 0;
   r->left = 0;
   return r->took;
}

#endif
