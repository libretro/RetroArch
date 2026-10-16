/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_gpu_queue.h"
#include <stdlib.h>
#include <retro_spsc.h>

struct record_gpu_queue
{
   retro_spsc_t ready;
   retro_spsc_t available;
   retro_atomic_int_t closed;
   uint32_t owned;
   unsigned slots;
   unsigned reserved;
   unsigned spare;
};

record_gpu_queue_t *record_gpu_queue_new(unsigned slots)
{
   record_gpu_queue_t *q;
   unsigned i;
   if (!slots || slots > RECORD_GPU_QUEUE_SLOTS)
      return NULL;
   q = (record_gpu_queue_t*)calloc(1, sizeof(*q));
   if (!q)
      return NULL;
   if (!retro_spsc_init(&q->ready, slots * sizeof(record_gpu_frame_t))
         || !retro_spsc_init(&q->available, slots * sizeof(unsigned)))
   {
      retro_spsc_free(&q->ready);
      retro_spsc_free(&q->available);
      free(q);
      return NULL;
   }
   retro_atomic_int_init(&q->closed, 0);
   q->slots = slots;
   q->reserved = q->spare = RECORD_GPU_QUEUE_SLOTS;
   for (i = 0; i < slots; i++)
      retro_spsc_write(&q->available, &i, sizeof(i));
   return q;
}

int record_gpu_queue_acquire(record_gpu_queue_t *q, unsigned *slot)
{
   if (!q || !slot || q->reserved != RECORD_GPU_QUEUE_SLOTS)
      return RECORD_GPU_QUEUE_ERROR;
   if (retro_atomic_load_acquire_int(&q->closed))
      return RECORD_GPU_QUEUE_END;
   if (q->spare != RECORD_GPU_QUEUE_SLOTS)
   {
      q->reserved = q->spare;
      q->spare = RECORD_GPU_QUEUE_SLOTS;
   }
   else
   {
      if (retro_spsc_read_avail(&q->available) < sizeof(q->reserved))
         return RECORD_GPU_QUEUE_BUSY;
      retro_spsc_read(&q->available, &q->reserved, sizeof(q->reserved));
   }
   *slot = q->reserved;
   return RECORD_GPU_QUEUE_OK;
}

bool record_gpu_queue_submit(record_gpu_queue_t *q,
      const record_gpu_frame_t *frame)
{
   if (!q || !frame || q->reserved == RECORD_GPU_QUEUE_SLOTS
         || frame->slot != q->reserved || retro_atomic_load_acquire_int(&q->closed)
         || retro_spsc_write_avail(&q->ready) < sizeof(*frame))
      return false;
   retro_spsc_write(&q->ready, frame, sizeof(*frame));
   q->reserved = RECORD_GPU_QUEUE_SLOTS;
   return true;
}

bool record_gpu_queue_cancel(record_gpu_queue_t *q)
{
   if (!q || q->reserved == RECORD_GPU_QUEUE_SLOTS)
      return false;
   q->spare = q->reserved;
   q->reserved = RECORD_GPU_QUEUE_SLOTS;
   return true;
}

bool record_gpu_queue_finish(record_gpu_queue_t *q)
{
   if (!q || q->reserved != RECORD_GPU_QUEUE_SLOTS)
      return false;
   retro_atomic_store_release_int(&q->closed, 1);
   return true;
}

int record_gpu_queue_take(record_gpu_queue_t *q, record_gpu_frame_t *frame)
{
   int closed;
   if (!q || !frame)
      return RECORD_GPU_QUEUE_ERROR;
   /* Observe the final publication before testing for empty. */
   closed = retro_atomic_load_acquire_int(&q->closed);
   if (retro_spsc_read_avail(&q->ready) < sizeof(*frame))
      return closed ? RECORD_GPU_QUEUE_END : RECORD_GPU_QUEUE_BUSY;
   retro_spsc_read(&q->ready, frame, sizeof(*frame));
   q->owned |= (uint32_t)1 << frame->slot;
   return RECORD_GPU_QUEUE_OK;
}

bool record_gpu_queue_release(record_gpu_queue_t *q, unsigned slot)
{
   if (!q || slot >= q->slots || !(q->owned & ((uint32_t)1 << slot))
         || retro_spsc_write_avail(&q->available) < sizeof(slot))
      return false;
   q->owned &= ~((uint32_t)1 << slot);
   retro_spsc_write(&q->available, &slot, sizeof(slot));
   return true;
}

bool record_gpu_queue_free(record_gpu_queue_t *q)
{
   if (!q)
      return true;
   if (q->reserved != RECORD_GPU_QUEUE_SLOTS || q->owned
         || retro_spsc_read_avail(&q->ready))
      return false;
   retro_spsc_free(&q->ready);
   retro_spsc_free(&q->available);
   free(q);
   return true;
}
