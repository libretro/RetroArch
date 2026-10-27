/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_METAL_VP9_DRIVER_H
#define RETROARCH_METAL_VP9_DRIVER_H
#include "videotoolbox_vp9_backend.h"

#if defined(VIDEOTOOLBOX_VP9_OWNER) && defined(METAL_DECODER_SNAPSHOT)
#define METAL_VP9_DRIVER 1

typedef struct metal_vp9_driver
{
   metal_decoder_snapshot_t snapshot;
   videotoolbox_vp9_backend_t backend;
   bool initialized;
} metal_vp9_driver_t;

static void metal_vp9_driver_offer(void *opaque)
{
   metal_vp9_driver_t *driver = (metal_vp9_driver_t *)opaque;
   rvp9_set_hw_backend(&driver->backend.offer);
}

static void metal_vp9_driver_withdraw(void *opaque)
{
   metal_vp9_driver_t *driver = (metal_vp9_driver_t *)opaque;
   rvp9_unset_hw_backend(&driver->backend.offer);
}

/* The renderer supplies its existing setup lock. */
static INLINE bool metal_vp9_driver_init(metal_vp9_driver_t *driver,
      slock_t *lock, void *(*retain)(void *), void (*release)(void *),
      bool (*enabled)(void *), videotoolbox_vp9_register_t registration,
      const retro_atomic_ptr_t *current_owner)
{
   if (!driver)
      return false;
   memset(driver, 0, sizeof(*driver));
   if (!metal_decoder_snapshot_init(&driver->snapshot, lock, retain, release)
         || !videotoolbox_vp9_backend_init(&driver->backend, &driver->snapshot,
               metal_decoder_snapshot_acquire, release,
               metal_decoder_snapshot_current, enabled, registration,
               8u * 1024 * 1024, 1))
      return false;
   driver->snapshot.current_owner = current_owner;
   driver->initialized = true;
   return true;
}

static INLINE bool metal_vp9_driver_start(metal_vp9_driver_t *driver,
      void *owner, void *device, bool hdr)
{
   return driver && driver->initialized
         && metal_decoder_snapshot_publish(&driver->snapshot, owner, device,
               hdr, metal_vp9_driver_offer, driver);
}

static INLINE bool metal_vp9_driver_stop(
      metal_vp9_driver_t *driver, void *owner)
{
   return driver && driver->initialized
         && metal_decoder_snapshot_withdraw(&driver->snapshot, owner,
               metal_vp9_driver_withdraw, driver);
}
#endif
#endif
