/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 The RetroArch team
 */
#ifndef __LIBRETRO_SDK_MPSC_H
#define __LIBRETRO_SDK_MPSC_H

#include <stddef.h>
#include <retro_atomic.h>

#if !defined(RETRO_ATOMIC_LOCK_FREE) || !defined(RETRO_ATOMIC_HAS_PTR)
#error retro_mpsc requires lock-free pointer atomics
#endif

/* Intrusive multi-producer, single-consumer FIFO, drained in batches.
 * A producer owns its node until push publishes it. It must never touch
 * that node afterwards: the consumer may immediately reclaim it.
 * Only the consumer calls take_all; it owns every node returned and must
 * finish that batch before taking another to preserve FIFO order.
 * Producers never dereference the shared head, so detached nodes can be
 * reclaimed immediately, without hazard pointers or a stub node.
 * Init and destruction require all users to be quiescent.
 */
typedef struct retro_mpsc_node
{
   struct retro_mpsc_node *next;
} retro_mpsc_node_t;

typedef struct retro_mpsc
{
   retro_atomic_ptr_t head;
} retro_mpsc_t;

static INLINE void retro_mpsc_init(retro_mpsc_t *queue)
{
   retro_atomic_ptr_init(&queue->head, NULL);
}

static INLINE void retro_mpsc_push(retro_mpsc_t *queue,
      retro_mpsc_node_t *node)
{
   void *head;
   do
   {
      head = retro_atomic_load_acquire_ptr(&queue->head);
      node->next = (retro_mpsc_node_t *)head;
   } while (!retro_atomic_cas_ptr(&queue->head, head, node));
}

/* Snapshot only; producers can publish immediately after this returns. */
static INLINE int retro_mpsc_empty(retro_mpsc_t *queue)
{
   return retro_atomic_load_acquire_ptr(&queue->head) == NULL;
}

static INLINE retro_mpsc_node_t *retro_mpsc_take_all(retro_mpsc_t *queue)
{
   retro_mpsc_node_t *node = (retro_mpsc_node_t *)
      retro_atomic_exchange_ptr(&queue->head, NULL);
   retro_mpsc_node_t *fifo = NULL;
   while (node)
   {
      retro_mpsc_node_t *next = node->next;
      node->next = fifo;
      fifo = node;
      node = next;
   }
   return fifo;
}

#endif
