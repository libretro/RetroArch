/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_METAL_HEVC_DRIVER_H
#define RETROARCH_METAL_HEVC_DRIVER_H
#include "videotoolbox_hevc_registry.h"
#include <dispatch/dispatch.h>
#ifdef HAVE_RVP9
#include "metal_vp9_driver.h"
#include <TargetConditionals.h>
#include <dlfcn.h>
#endif

#ifdef VIDEOTOOLBOX_HEVC_REGISTRY
#define METAL_HEVC_DRIVER 1
/* The including platform supplies its setup-time decode-setting policy. */
static bool metal_hevc_decode_enabled(void *user);
static videotoolbox_hevc_registry_t metal_hevc_registry;
static retro_atomic_ptr_t metal_hevc_active_registry;
static dispatch_once_t metal_hevc_once;
static dispatch_queue_t metal_hevc_retirement_queue;
#ifdef METAL_VP9_DRIVER
static metal_vp9_driver_t metal_hevc_vp9;

static videotoolbox_vp9_register_t metal_hevc_vp9_registration(void)
{
   videotoolbox_vp9_register_t registration = NULL;
#if TARGET_OS_OSX && !TARGET_OS_MACCATALYST
   if (@available(macOS 11.0, *))
   {
      void *symbol = dlsym(RTLD_DEFAULT,
            "VTRegisterSupplementalVideoDecoderIfAvailable");
      if (sizeof(registration) == sizeof(symbol))
         memcpy(&registration, &symbol, sizeof(registration));
   }
#endif
   return registration;
}
#endif


static void *metal_hevc_retain(void *device) { return (void*)CFRetain(device); }
static void metal_hevc_release(void *device) { CFRelease(device); }
static void metal_hevc_enqueue(void *user, void (*run)(void*), void *work)
{
   (void)user;
   dispatch_async_f(metal_hevc_retirement_queue, work, run);
}
static VIDEOTOOLBOX_HEVC_AVAILABLE void metal_hevc_initialize(void *unused)
{
   slock_t *lock = slock_new();
   (void)unused;
   if (!lock) return;
   metal_hevc_retirement_queue = dispatch_queue_create(
         "org.libretro.metal.hevc.retirement", DISPATCH_QUEUE_SERIAL);
   if (!metal_hevc_retirement_queue)
   { slock_free(lock); return; }
   if (!videotoolbox_hevc_registry_init(&metal_hevc_registry, lock,
            metal_hevc_retain, metal_hevc_release, metal_hevc_decode_enabled,
            NULL, metal_hevc_enqueue, NULL, true))
   { slock_free(lock); return; }
#ifdef METAL_VP9_DRIVER
   metal_vp9_driver_init(&metal_hevc_vp9, lock, metal_hevc_retain,
         metal_hevc_release, metal_hevc_decode_enabled,
         metal_hevc_vp9_registration(), &metal_hevc_registry.owner);
#endif
   /* Process-lifetime storage/queue survive every driver and queued owner. */
   retro_atomic_store_release_ptr(&metal_hevc_active_registry, &metal_hevc_registry);
}
static bool metal_hevc_start_supported(void *owner, void *device, void *sdr_pipeline, bool hdr_enabled)
{
   videotoolbox_hevc_registry_t *registry;
   if (!owner || !device || !sdr_pipeline) return false;
   if (@available(macOS 10.14, iOS 12.0, tvOS 12.0, *))
   {
      dispatch_once_f(&metal_hevc_once, NULL, metal_hevc_initialize);
      registry = (videotoolbox_hevc_registry_t*)retro_atomic_load_acquire_ptr(&metal_hevc_active_registry);
      if (!registry || !videotoolbox_hevc_registry_publish_support(
               registry, owner, device, hdr_enabled))
         return false;
#ifdef METAL_VP9_DRIVER
      metal_vp9_driver_start(&metal_hevc_vp9, owner, device, hdr_enabled);
#endif
      return true;
   }
   return false;
}
static bool metal_hevc_start(void *owner, void *device, void *sdr_pipeline)
{
   return metal_hevc_start_supported(owner, device, sdr_pipeline, false);
}
static void metal_hevc_stop(void *owner)
{
   videotoolbox_hevc_registry_t *registry = (videotoolbox_hevc_registry_t*)
      retro_atomic_load_acquire_ptr(&metal_hevc_active_registry);
   if (registry)
   {
#ifdef METAL_VP9_DRIVER
      metal_vp9_driver_stop(&metal_hevc_vp9, owner);
#endif
      videotoolbox_hevc_registry_withdraw(registry, owner);
   }
}
static bool metal_hevc_owns(void *owner)
{
   const videotoolbox_hevc_registry_t *registry = (const videotoolbox_hevc_registry_t*)
      retro_atomic_load_acquire_ptr(&metal_hevc_active_registry);
   return registry && videotoolbox_hevc_registry_is_owner(registry, owner);
}
#endif
#endif
