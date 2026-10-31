/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_DECODER_DEVICE_SLOT_H
#define RETROARCH_DECODER_DEVICE_SLOT_H
#include <stdint.h>
#include <stdlib.h>
#include <boolean.h>
#include <retro_atomic.h>
#include <retro_inline.h>

#ifdef RETRO_ATOMIC_HAS_PTR
#define DECODER_DEVICE_SLOT 1

/* The renderer's device as decoder setup sees it. The renderer's thread
 * replaces it; any thread may take a retained copy. A replaced device
 * is released once no thread is between loading it and retaining it,
 * by whichever thread is last out. */
typedef struct decoder_device_node
{
   struct decoder_device_node *next;
   void *device;
   uintptr_t generation;
   bool hdr;
} decoder_device_node_t;

typedef struct decoder_device_slot
{
   void *(*retain)(void *);
   void (*release)(void *);
   retro_atomic_ptr_t current;
   retro_atomic_ptr_t retired;
   retro_atomic_int_t readers;
   /* Bumped after each replacement: a reader that counts itself in and
    * then reads it sees the replacement, or is seen by the drain. */
   retro_atomic_int_t epoch;
} decoder_device_slot_t;

static INLINE void decoder_device_slot_init(decoder_device_slot_t *s,
      void *(*retain)(void *), void (*release)(void *))
{
   s->retain  = retain;
   s->release = release;
   retro_atomic_ptr_init(&s->current, NULL);
   retro_atomic_ptr_init(&s->retired, NULL);
   retro_atomic_int_init(&s->readers, 0);
   retro_atomic_int_init(&s->epoch, 0);
}

static INLINE void decoder_device_slot_push(decoder_device_slot_t *s,
      decoder_device_node_t *first, decoder_device_node_t *last)
{
   void *head;
   do
   {
      head       = retro_atomic_load_acquire_ptr(&s->retired);
      last->next = (decoder_device_node_t *)head;
   } while (!retro_atomic_cas_ptr(&s->retired, head, first));
}

/* Releases what was replaced while no thread is reading. One that is
 * gets it back and frees it on its way out. */
static INLINE void decoder_device_slot_drain(decoder_device_slot_t *s)
{
   for (;;)
   {
      decoder_device_node_t *list, *last;
      if (retro_atomic_load_seq_cst_int(&s->readers))
         return;
      if (!(list = (decoder_device_node_t *)
               retro_atomic_exchange_ptr(&s->retired, NULL)))
         return;
      if (retro_atomic_load_seq_cst_int(&s->readers))
      {
         for (last = list; last->next; last = last->next) { }
         decoder_device_slot_push(s, list, last);
         continue;
      }
      while (list)
      {
         last = list->next;
#ifndef DECODER_DEVICE_SLOT_TEST_RELEASE_EARLY
         s->release(list->device);
#endif
         free(list);
         list = last;
      }
   }
}

static INLINE void decoder_device_slot_retire(decoder_device_slot_t *s,
      decoder_device_node_t *old)
{
   if (!old)
      return;
#ifdef DECODER_DEVICE_SLOT_TEST_RELEASE_EARLY
   s->release(old->device);
#endif
   decoder_device_slot_push(s, old, old);
   decoder_device_slot_drain(s);
}

/* Renderer's thread. A NULL @device withdraws. */
static INLINE bool decoder_device_slot_set(decoder_device_slot_t *s,
      void *device, uintptr_t generation, bool hdr)
{
   decoder_device_node_t *node = NULL;
   if (device)
   {
      if (!(node = (decoder_device_node_t *)malloc(sizeof(*node))))
         return false;
      if (!(node->device = s->retain(device)))
      {
         free(node);
         return false;
      }
      node->next       = NULL;
      node->generation = generation;
      node->hdr        = hdr;
   }
   node = (decoder_device_node_t *)
         retro_atomic_exchange_ptr(&s->current, node);
   retro_atomic_fetch_add_seq_cst_int(&s->epoch, 1);
   decoder_device_slot_retire(s, node);
   return true;
}

/* Any thread: the current device retained, or NULL. */
static INLINE void *decoder_device_slot_acquire(decoder_device_slot_t *s,
      uintptr_t *generation, bool *hdr)
{
   void *device = NULL;
   const decoder_device_node_t *node;
   retro_atomic_fetch_add_seq_cst_int(&s->readers, 1);
   (void)retro_atomic_load_seq_cst_int(&s->epoch);
   if ((node = (const decoder_device_node_t *)
            retro_atomic_load_acquire_ptr(&s->current)))
   {
#ifdef DECODER_DEVICE_SLOT_TEST_LOADED
      DECODER_DEVICE_SLOT_TEST_LOADED(s);
#endif
      device = s->retain(node->device);
   }
   if (device)
   {
      if (generation)
         *generation = node->generation;
      if (hdr)
         *hdr = node->hdr;
   }
   if (retro_atomic_fetch_add_seq_cst_int(&s->readers, -1) == 1)
      decoder_device_slot_drain(s);
   return device;
}
#endif
#endif
