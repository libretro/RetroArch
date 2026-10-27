/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_METAL_DECODER_SNAPSHOT_H
#define RETROARCH_METAL_DECODER_SNAPSHOT_H
#include <stdint.h>
#include <string.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>

typedef struct metal_decoder_snapshot_view
{
   void *device;
   uintptr_t generation;
   bool hdr;
} metal_decoder_snapshot_view_t;

#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_HAS_PTR)                     \
      && defined(RETRO_ATOMIC_LOCK_FREE)
#define METAL_DECODER_SNAPSHOT 1

typedef struct metal_decoder_snapshot
{
   slock_t *lock;
   const retro_atomic_ptr_t *current_owner;
   void *device;
   void *(*retain)(void *);
   void (*release)(void *);
   retro_atomic_ptr_t owner;
   retro_atomic_size_t generation;
   retro_atomic_int_t hdr;
} metal_decoder_snapshot_t;

static INLINE bool metal_decoder_snapshot_init(metal_decoder_snapshot_t *s,
      slock_t *lock, void *(*retain)(void *), void (*release)(void *))
{
   if (!s)
      return false;
   memset(s, 0, sizeof(*s));
   if (!lock || !retain || !release)
      return false;
   s->lock    = lock;
   s->retain  = retain;
   s->release = release;
   retro_atomic_ptr_init(&s->owner, NULL);
   retro_atomic_size_init(&s->generation, 0);
   retro_atomic_int_init(&s->hdr, 0);
   return true;
}

/* Cold setup only. Reuse the renderer's existing snapshot lock. */
static INLINE bool metal_decoder_snapshot_acquire(
      void *opaque, metal_decoder_snapshot_view_t *out)
{
   metal_decoder_snapshot_t *s = (metal_decoder_snapshot_t *)opaque;
   if (!out)
      return false;
   memset(out, 0, sizeof(*out));
   if (!s || !s->lock)
      return false;
   slock_lock(s->lock);
   if (s->current_owner && retro_atomic_load_acquire_ptr(s->current_owner)
         != retro_atomic_load_acquire_ptr(&s->owner))
   {
      slock_unlock(s->lock);
      return false;
   }
   out->device = s->device ? s->retain(s->device) : NULL;
   if (out->device)
   {
      out->generation =
            (uintptr_t)retro_atomic_load_acquire_size(&s->generation);
      out->hdr = retro_atomic_load_acquire_int(&s->hdr) != 0;
   }
   slock_unlock(s->lock);
   return out->device != NULL;
}

static INLINE bool metal_decoder_snapshot_current(
      void *opaque, uintptr_t generation)
{
   const metal_decoder_snapshot_t *s = (const metal_decoder_snapshot_t *)opaque;
   return s && s->lock && (generation & 1)
          && retro_atomic_load_acquire_size(&s->generation)
                   == (size_t)generation
          && (!s->current_owner
                || retro_atomic_load_acquire_ptr(s->current_owner)
                      == retro_atomic_load_acquire_ptr(&s->owner));
}

/* Lifecycle only. Codec offer hooks run under the same existing setup lock. */
static INLINE bool metal_decoder_snapshot_publish(metal_decoder_snapshot_t *s,
      void *owner, void *device, bool hdr, void (*publish)(void *), void *user)
{
   void *old, *owned;
   size_t generation;
   if (!s || !s->lock || !owner || !device)
      return false;
   owned = s->retain(device);
   if (!owned)
      return false;
   slock_lock(s->lock);
   if (s->current_owner
         && retro_atomic_load_acquire_ptr(s->current_owner) != owner)
   {
      slock_unlock(s->lock);
      s->release(owned);
      return false;
   }
   generation = retro_atomic_load_acquire_size(&s->generation);
   if (generation >= (size_t)-1 - 2)
   {
      slock_unlock(s->lock);
      s->release(owned);
      return false;
   }
   old       = s->device;
   s->device = owned;
   retro_atomic_store_release_int(&s->hdr, hdr ? 1 : 0);
   retro_atomic_store_release_size(
         &s->generation, generation + ((generation & 1) ? 2 : 1));
   retro_atomic_store_release_ptr(&s->owner, owner);
   if (publish)
      publish(user);
   slock_unlock(s->lock);
   if (old)
      s->release(old);
   return true;
}

static INLINE bool metal_decoder_snapshot_withdraw(metal_decoder_snapshot_t *s,
      void *owner, void (*withdraw)(void *), void *user)
{
   void *old;
   size_t generation;
   if (!s || !s->lock || !owner)
      return false;
   slock_lock(s->lock);
   if (retro_atomic_load_acquire_ptr(&s->owner) != owner)
   {
      slock_unlock(s->lock);
      return false;
   }
   if (withdraw)
      withdraw(user);
   old        = s->device;
   s->device  = NULL;
   generation = retro_atomic_load_acquire_size(&s->generation);
   retro_atomic_store_release_size(&s->generation, generation + 1);
   retro_atomic_store_release_int(&s->hdr, 0);
   retro_atomic_store_release_ptr(&s->owner, NULL);
   slock_unlock(s->lock);
   if (old)
      s->release(old);
   return true;
}

/* Frame-path capability query: no lock, retain, allocation or wait. */
static INLINE bool metal_decoder_snapshot_is_owner(
      const metal_decoder_snapshot_t *s, const void *owner)
{
   return s && s->lock && owner
          && retro_atomic_load_acquire_ptr(&s->owner) == owner;
}
#endif
#endif
