/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_HEVC_REGISTRY_H
#define RETROARCH_VIDEOTOOLBOX_HEVC_REGISTRY_H
#include "videotoolbox_hevc_backend.h"
#include <rthreads/rthreads.h>

#if defined(VIDEOTOOLBOX_HEVC_SLOTS) && defined(RETRO_ATOMIC_HAS_PTR)
#define VIDEOTOOLBOX_HEVC_REGISTRY 1
typedef struct videotoolbox_hevc_registry
{
   videotoolbox_hevc_backend_t backend;
   slock_t *lock;
   retro_atomic_ptr_t owner;
   retro_atomic_int_t hdr_enabled;
   void *device, *user;
   void *(*retain)(void*);
   void (*release)(void*);
   bool (*enabled)(void*);
} videotoolbox_hevc_registry_t;

/* Decoder setup only. The lock protects snapshot acquisition, not frames. */
static void *videotoolbox_hevc_registry_acquire(void *opaque)
{
   videotoolbox_hevc_registry_t *registry = (videotoolbox_hevc_registry_t*)opaque;
   void *device;
   slock_lock(registry->lock);
   device = registry->device ? registry->retain(registry->device) : NULL;
   slock_unlock(registry->lock);
   return device;
}
static bool videotoolbox_hevc_registry_enabled(void *opaque)
{
   videotoolbox_hevc_registry_t *registry = (videotoolbox_hevc_registry_t*)opaque;
   return !registry->enabled || registry->enabled(registry->user);
}

/* Setup capability snapshot; no renderer/object access from codec setup. */
static bool videotoolbox_hevc_registry_hdr_enabled(void *opaque)
{
   videotoolbox_hevc_registry_t *registry = (videotoolbox_hevc_registry_t*)opaque;
   return retro_atomic_load_acquire_int(&registry->hdr_enabled) != 0;
}

/* Single initialization. Storage/lock/callbacks outlive all opened decoders;
 * withdraw and stop setup callers before freeing any of them. */
static INLINE VIDEOTOOLBOX_HEVC_AVAILABLE bool videotoolbox_hevc_registry_init(videotoolbox_hevc_registry_t *registry,
      slock_t *lock, void *(*retain)(void*), void (*release)(void*),
      bool (*enabled)(void*), void *user, videotoolbox_hevc_enqueue_t enqueue,
      void *enqueue_user, bool sdr_only)
{
   if (!registry) return false;
   memset(registry, 0, sizeof(*registry));
   if (!lock || !retain || !release || !enqueue) return false;
   registry->lock = lock; registry->retain = retain; registry->release = release;
   registry->enabled = enabled; registry->user = user;
   retro_atomic_ptr_init(&registry->owner, NULL);
   retro_atomic_int_init(&registry->hdr_enabled, 0);
   videotoolbox_hevc_backend_init(&registry->backend, registry,
         videotoolbox_hevc_registry_acquire, release,
         videotoolbox_hevc_registry_enabled, enqueue, enqueue_user, sdr_only);
   registry->backend.hdr_enabled = videotoolbox_hevc_registry_hdr_enabled;
   return true;
}

/* Driver setup only. Replacement releases its old device after unlocking. */
static INLINE bool videotoolbox_hevc_registry_publish_support(videotoolbox_hevc_registry_t *registry,
      void *owner, void *device, bool hdr_enabled)
{
   void *old, *owned;
   if (!registry || !registry->lock || !owner || !device) return false;
   owned = registry->retain(device);
   if (!owned) return false;
   slock_lock(registry->lock);
   old = registry->device; registry->device = owned;
   retro_atomic_store_release_int(&registry->hdr_enabled, hdr_enabled ? 1 : 0);
   retro_atomic_store_release_ptr(&registry->owner, owner);
   rh265_set_hw_backend(&registry->backend.offer);
   slock_unlock(registry->lock);
   if (old) registry->release(old);
   return true;
}

static INLINE bool videotoolbox_hevc_registry_publish(videotoolbox_hevc_registry_t *registry,
      void *owner, void *device)
{
   return videotoolbox_hevc_registry_publish_support(registry, owner, device, false);
}

/* Driver teardown only. Old owners cannot withdraw a replacement driver,
 * and a foreign codec offer is preserved by compare-and-withdraw. */
static INLINE bool videotoolbox_hevc_registry_withdraw(videotoolbox_hevc_registry_t *registry,
      void *owner)
{
   void *old;
   if (!registry || !registry->lock || !owner) return false;
   slock_lock(registry->lock);
   if (retro_atomic_load_acquire_ptr(&registry->owner) != owner)
   { slock_unlock(registry->lock); return false; }
   rh265_unset_hw_backend(&registry->backend.offer);
   old = registry->device; registry->device = NULL;
   retro_atomic_store_release_int(&registry->hdr_enabled, 0);
   retro_atomic_store_release_ptr(&registry->owner, NULL);
   slock_unlock(registry->lock);
   if (old) registry->release(old);
   return true;
}

/* Capability query only: no lock, retain, allocation or wait. */
static INLINE bool videotoolbox_hevc_registry_is_owner(
      const videotoolbox_hevc_registry_t *registry, const void *owner)
{
   return registry && registry->lock && owner
      && retro_atomic_load_acquire_ptr(&registry->owner) == owner;
}
#endif
#endif
