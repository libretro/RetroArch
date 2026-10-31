/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_HEVC_REGISTRY_H
#define RETROARCH_VIDEOTOOLBOX_HEVC_REGISTRY_H
#include "videotoolbox_hevc_backend.h"
#include "decoder_device_slot.h"

#if defined(VIDEOTOOLBOX_HEVC_SLOTS) && defined(DECODER_DEVICE_SLOT)
#define VIDEOTOOLBOX_HEVC_REGISTRY 1
typedef struct videotoolbox_hevc_registry
{
   videotoolbox_hevc_backend_t backend;
   decoder_device_slot_t slot;
   retro_atomic_ptr_t owner;
   retro_atomic_int_t hdr_enabled;
   void *user;
   bool (*enabled)(void*);
   bool initialized;
} videotoolbox_hevc_registry_t;

/* Decoder setup, any thread. */
static void *videotoolbox_hevc_registry_acquire(void *opaque)
{
   videotoolbox_hevc_registry_t *registry = (videotoolbox_hevc_registry_t*)opaque;
   return decoder_device_slot_acquire(&registry->slot, NULL, NULL);
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

/* Single initialization. Storage/callbacks outlive all opened decoders;
 * withdraw and stop setup callers before freeing any of them. */
static INLINE VIDEOTOOLBOX_HEVC_AVAILABLE bool videotoolbox_hevc_registry_init(videotoolbox_hevc_registry_t *registry,
      void *(*retain)(void*), void (*release)(void*),
      bool (*enabled)(void*), void *user, videotoolbox_hevc_enqueue_t enqueue,
      void *enqueue_user, bool sdr_only)
{
   if (!registry) return false;
   memset(registry, 0, sizeof(*registry));
   if (!retain || !release || !enqueue) return false;
   decoder_device_slot_init(&registry->slot, retain, release);
   registry->enabled = enabled; registry->user = user;
   retro_atomic_ptr_init(&registry->owner, NULL);
   retro_atomic_int_init(&registry->hdr_enabled, 0);
   videotoolbox_hevc_backend_init(&registry->backend, registry,
         videotoolbox_hevc_registry_acquire, release,
         videotoolbox_hevc_registry_enabled, enqueue, enqueue_user, sdr_only);
   registry->backend.hdr_enabled = videotoolbox_hevc_registry_hdr_enabled;
   registry->initialized = true;
   return true;
}

/* Driver lifecycle thread only, as is withdraw. */
static INLINE bool videotoolbox_hevc_registry_publish_support(videotoolbox_hevc_registry_t *registry,
      void *owner, void *device, bool hdr_enabled)
{
   if (!registry || !registry->initialized || !owner || !device) return false;
   if (!decoder_device_slot_set(&registry->slot, device, 0, hdr_enabled))
      return false;
   retro_atomic_store_release_int(&registry->hdr_enabled, hdr_enabled ? 1 : 0);
   retro_atomic_store_release_ptr(&registry->owner, owner);
   rh265_set_hw_backend(&registry->backend.offer);
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
   if (!registry || !registry->initialized || !owner
         || retro_atomic_load_acquire_ptr(&registry->owner) != owner)
      return false;
   rh265_unset_hw_backend(&registry->backend.offer);
   retro_atomic_store_release_int(&registry->hdr_enabled, 0);
   retro_atomic_store_release_ptr(&registry->owner, NULL);
   decoder_device_slot_set(&registry->slot, NULL, 0, false);
   return true;
}

/* Capability query only: no retain, allocation or wait. */
static INLINE bool videotoolbox_hevc_registry_is_owner(
      const videotoolbox_hevc_registry_t *registry, const void *owner)
{
   return registry && registry->initialized && owner
      && retro_atomic_load_acquire_ptr(&registry->owner) == owner;
}
#endif
#endif
