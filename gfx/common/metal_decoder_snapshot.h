/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_METAL_DECODER_SNAPSHOT_H
#define RETROARCH_METAL_DECODER_SNAPSHOT_H
#include <stdint.h>
#include <string.h>
#include <retro_atomic.h>

typedef struct metal_decoder_snapshot_view
{
   void *device;
   uintptr_t generation;
   bool hdr;
} metal_decoder_snapshot_view_t;

#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_HAS_PTR)                     \
      && defined(RETRO_ATOMIC_LOCK_FREE)
#include "decoder_device_slot.h"
#define METAL_DECODER_SNAPSHOT 1

typedef struct metal_decoder_snapshot
{
   decoder_device_slot_t slot;
   const retro_atomic_ptr_t *current_owner;
   retro_atomic_ptr_t owner;
   retro_atomic_size_t generation;
   bool initialized;
} metal_decoder_snapshot_t;

static INLINE bool metal_decoder_snapshot_init(metal_decoder_snapshot_t *s,
      void *(*retain)(void *), void (*release)(void *))
{
   if (!s)
      return false;
   memset(s, 0, sizeof(*s));
   if (!retain || !release)
      return false;
   decoder_device_slot_init(&s->slot, retain, release);
   retro_atomic_ptr_init(&s->owner, NULL);
   retro_atomic_size_init(&s->generation, 0);
   s->initialized = true;
   return true;
}

/* Cold setup, any thread. */
static INLINE bool metal_decoder_snapshot_acquire(
      void *opaque, metal_decoder_snapshot_view_t *out)
{
   metal_decoder_snapshot_t *s = (metal_decoder_snapshot_t *)opaque;
   if (!out)
      return false;
   memset(out, 0, sizeof(*out));
   if (!s || !s->initialized)
      return false;
   if (s->current_owner && retro_atomic_load_acquire_ptr(s->current_owner)
         != retro_atomic_load_acquire_ptr(&s->owner))
      return false;
   out->device = decoder_device_slot_acquire(
         &s->slot, &out->generation, &out->hdr);
   return out->device != NULL;
}

static INLINE bool metal_decoder_snapshot_current(
      void *opaque, uintptr_t generation)
{
   const metal_decoder_snapshot_t *s = (const metal_decoder_snapshot_t *)opaque;
   return s && s->initialized && (generation & 1)
          && retro_atomic_load_acquire_size(&s->generation)
                   == (size_t)generation
          && (!s->current_owner
                || retro_atomic_load_acquire_ptr(s->current_owner)
                      == retro_atomic_load_acquire_ptr(&s->owner));
}

/* Renderer lifecycle thread only, as is withdraw. */
static INLINE bool metal_decoder_snapshot_publish(metal_decoder_snapshot_t *s,
      void *owner, void *device, bool hdr, void (*publish)(void *), void *user)
{
   size_t generation;
   if (!s || !s->initialized || !owner || !device)
      return false;
   if (s->current_owner
         && retro_atomic_load_acquire_ptr(s->current_owner) != owner)
      return false;
   generation = retro_atomic_load_acquire_size(&s->generation);
   if (generation >= (size_t)-1 - 2)
      return false;
   generation += (generation & 1) ? 2 : 1;
   if (!decoder_device_slot_set(&s->slot, device, (uintptr_t)generation, hdr))
      return false;
   retro_atomic_store_release_size(&s->generation, generation);
   retro_atomic_store_release_ptr(&s->owner, owner);
   if (publish)
      publish(user);
   return true;
}

static INLINE bool metal_decoder_snapshot_withdraw(metal_decoder_snapshot_t *s,
      void *owner, void (*withdraw)(void *), void *user)
{
   if (!s || !s->initialized || !owner
         || retro_atomic_load_acquire_ptr(&s->owner) != owner)
      return false;
   if (withdraw)
      withdraw(user);
   retro_atomic_store_release_size(&s->generation,
         retro_atomic_load_acquire_size(&s->generation) + 1);
   retro_atomic_store_release_ptr(&s->owner, NULL);
   decoder_device_slot_set(&s->slot, NULL, 0, false);
   return true;
}

/* Frame-path capability query: no retain, allocation or wait. */
static INLINE bool metal_decoder_snapshot_is_owner(
      const metal_decoder_snapshot_t *s, const void *owner)
{
   return s && s->initialized && owner
          && retro_atomic_load_acquire_ptr(&s->owner) == owner;
}
#endif
#endif
