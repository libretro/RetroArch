/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_HEVC_REGISTRY_H
#define RETROARCH_VIDEOTOOLBOX_HEVC_REGISTRY_H
#include "videotoolbox_hevc_backend.h"
#include "metal_decoder_snapshot.h"

#if defined(VIDEOTOOLBOX_HEVC_SLOTS) && defined(METAL_DECODER_SNAPSHOT)
#define VIDEOTOOLBOX_HEVC_REGISTRY 1

typedef struct videotoolbox_hevc_registry
{
   videotoolbox_hevc_backend_t backend;
   metal_decoder_snapshot_t snapshot;
   void *user;
   bool (*enabled)(void *);
} videotoolbox_hevc_registry_t;

static void *videotoolbox_hevc_registry_acquire(void *opaque)
{
   videotoolbox_hevc_registry_t *registry =
         (videotoolbox_hevc_registry_t *)opaque;
   metal_decoder_snapshot_view_t view;
   return metal_decoder_snapshot_acquire(&registry->snapshot, &view)
                ? view.device
                : NULL;
}

static bool videotoolbox_hevc_registry_enabled(void *opaque)
{
   videotoolbox_hevc_registry_t *registry =
         (videotoolbox_hevc_registry_t *)opaque;
   return !registry->enabled || registry->enabled(registry->user);
}

static bool videotoolbox_hevc_registry_hdr_enabled(void *opaque)
{
   videotoolbox_hevc_registry_t *registry =
         (videotoolbox_hevc_registry_t *)opaque;
   return retro_atomic_load_acquire_int(&registry->snapshot.hdr) != 0;
}

static void videotoolbox_hevc_registry_publish_offer(void *opaque)
{
   videotoolbox_hevc_registry_t *registry =
         (videotoolbox_hevc_registry_t *)opaque;
   rh265_set_hw_backend(&registry->backend.offer);
}

static void videotoolbox_hevc_registry_withdraw_offer(void *opaque)
{
   videotoolbox_hevc_registry_t *registry =
         (videotoolbox_hevc_registry_t *)opaque;
   rh265_unset_hw_backend(&registry->backend.offer);
}

/* Storage, lock and callbacks outlive all setup callers and opened sessions. */
static INLINE VIDEOTOOLBOX_HEVC_AVAILABLE bool videotoolbox_hevc_registry_init(
      videotoolbox_hevc_registry_t *registry, slock_t *lock,
      void *(*retain)(void *), void (*release)(void *), bool (*enabled)(void *),
      void *user, videotoolbox_hevc_enqueue_t enqueue, void *enqueue_user,
      bool sdr_only)
{
   if (!registry)
      return false;
   memset(registry, 0, sizeof(*registry));
   if (!enqueue
         || !metal_decoder_snapshot_init(
               &registry->snapshot, lock, retain, release))
      return false;
   registry->enabled = enabled;
   registry->user    = user;
   videotoolbox_hevc_backend_init(&registry->backend, registry,
         videotoolbox_hevc_registry_acquire, release,
         videotoolbox_hevc_registry_enabled, enqueue, enqueue_user, sdr_only);
   registry->backend.hdr_enabled = videotoolbox_hevc_registry_hdr_enabled;
   return true;
}

static INLINE bool videotoolbox_hevc_registry_publish_support(
      videotoolbox_hevc_registry_t *registry, void *owner, void *device,
      bool hdr_enabled)
{
   return registry
          && metal_decoder_snapshot_publish(&registry->snapshot, owner, device,
                hdr_enabled, videotoolbox_hevc_registry_publish_offer,
                registry);
}

static INLINE bool videotoolbox_hevc_registry_publish(
      videotoolbox_hevc_registry_t *registry, void *owner, void *device)
{
   return videotoolbox_hevc_registry_publish_support(
         registry, owner, device, false);
}

static INLINE bool videotoolbox_hevc_registry_withdraw(
      videotoolbox_hevc_registry_t *registry, void *owner)
{
   return registry
          && metal_decoder_snapshot_withdraw(&registry->snapshot, owner,
                videotoolbox_hevc_registry_withdraw_offer, registry);
}

static INLINE bool videotoolbox_hevc_registry_is_owner(
      const videotoolbox_hevc_registry_t *registry, const void *owner)
{
   return registry
          && metal_decoder_snapshot_is_owner(&registry->snapshot, owner);
}
#endif
#endif
