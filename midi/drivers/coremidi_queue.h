/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2025 The RetroArch team
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

#ifndef __COREMIDI_QUEUE_H
#define __COREMIDI_QUEUE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_inline.h>

/* The CoreMIDI input queue: events from CoreMIDI's receive thread to
 * the thread that calls the driver's read(), without a lock.
 *
 * One producer and one consumer, each owning its own index. The
 * consumer hands out a pointer into the slot it read; the slot one
 * behind the read index is never written - a queue is full while the
 * producer's next slot is the read index - so that event stays intact
 * until the next read. Any thread may ask for the queue to be emptied;
 * the consumer does it at its next read, since only it may move the
 * read index. */

#define COREMIDI_QUEUE_SIZE      1024
#define COREMIDI_MAX_EVENT_SIZE  256

typedef struct
{
   uint8_t data[COREMIDI_MAX_EVENT_SIZE];
   size_t data_size;
   uint32_t delta_time;
} coremidi_event_t;

typedef struct
{
   coremidi_event_t events[COREMIDI_QUEUE_SIZE];
   retro_atomic_int_t read_index;   /* written by the consumer */
   retro_atomic_int_t write_index;  /* written by the producer */
   retro_atomic_int_t flush;        /* set by anyone, cleared by the consumer */
} coremidi_queue_t;

static INLINE void coremidi_queue_init(coremidi_queue_t *q)
{
   retro_atomic_store_release_int(&q->read_index, 0);
   retro_atomic_store_release_int(&q->write_index, 0);
   retro_atomic_store_release_int(&q->flush, 0);
}

/* Producer. False when the queue is full or the event does not fit. */
static INLINE bool coremidi_queue_write(coremidi_queue_t *q,
      const uint8_t *data, size_t size, uint32_t delta_time)
{
   int w    = retro_atomic_load_relaxed_int(&q->write_index);
   int next = (w + 1) % COREMIDI_QUEUE_SIZE;
   coremidi_event_t *ev;

   if (!data || size == 0 || size > COREMIDI_MAX_EVENT_SIZE)
      return false;
   if (next == retro_atomic_load_acquire_int(&q->read_index))
      return false;

   ev             = &q->events[w];
   memcpy(ev->data, data, size);
   ev->data_size  = size;
   ev->delta_time = delta_time;
   retro_atomic_store_release_int(&q->write_index, next);
   return true;
}

/* Any thread: empty the queue at the consumer's next read. */
static INLINE void coremidi_queue_clear(coremidi_queue_t *q)
{
   retro_atomic_store_release_int(&q->flush, 1);
}

/* Consumer. The event stays valid until the next call. */
static INLINE const coremidi_event_t *coremidi_queue_read(coremidi_queue_t *q)
{
   int r = retro_atomic_load_relaxed_int(&q->read_index);
   int w = retro_atomic_load_acquire_int(&q->write_index);
   if (     retro_atomic_load_acquire_int(&q->flush)
         && retro_atomic_exchange_int(&q->flush, 0))
   {
      r = w;
      retro_atomic_store_release_int(&q->read_index, r);
   }

   if (r == w)
      return NULL;
   retro_atomic_store_release_int(&q->read_index,
         (r + 1) % COREMIDI_QUEUE_SIZE);
   return &q->events[r];
}

#endif
