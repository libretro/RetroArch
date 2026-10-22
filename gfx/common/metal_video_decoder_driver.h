/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_METAL_VIDEO_DECODER_DRIVER_H
#define RETROARCH_METAL_VIDEO_DECODER_DRIVER_H
#include "metal_decoder_snapshot.h"
#ifdef HAVE_RMP4
#include "videotoolbox_hevc_registry.h"
#endif
#ifdef HAVE_RVP9
#include "videotoolbox_vp9_backend.h"
#include <TargetConditionals.h>
#include <dlfcn.h>
#endif
#include <dispatch/dispatch.h>

#if defined(METAL_DECODER_SNAPSHOT) && (defined(VIDEOTOOLBOX_HEVC_REGISTRY) \
      || defined(VIDEOTOOLBOX_VP9_OWNER))
#define METAL_VIDEO_DECODER_DRIVER 1
static bool metal_video_decoder_enabled(void *user);
static dispatch_once_t metal_video_decoder_once;
static retro_atomic_ptr_t metal_video_decoder_active;
#ifdef VIDEOTOOLBOX_HEVC_REGISTRY
static videotoolbox_hevc_registry_t metal_video_decoder_hevc;
static dispatch_queue_t metal_video_decoder_retirement_queue;
#else
static metal_decoder_snapshot_t metal_video_decoder_snapshot;
#endif
#ifdef VIDEOTOOLBOX_VP9_OWNER
static videotoolbox_vp9_backend_t metal_video_decoder_vp9;

static videotoolbox_vp9_register_t metal_video_decoder_registration(void)
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

static void *metal_video_decoder_retain(void *device)
{
   return (void *)CFRetain(device);
}

static void metal_video_decoder_release(void *device)
{
   CFRelease(device);
}

#ifdef VIDEOTOOLBOX_HEVC_REGISTRY
static void metal_video_decoder_enqueue(void *user, void (*run)(void *),
      void *work)
{
   (void)user;
   dispatch_async_f(metal_video_decoder_retirement_queue, work, run);
}
#endif

static void metal_video_decoder_publish(void *unused)
{
   (void)unused;
#ifdef VIDEOTOOLBOX_HEVC_REGISTRY
   videotoolbox_hevc_registry_publish_offer(&metal_video_decoder_hevc);
#endif
#ifdef VIDEOTOOLBOX_VP9_OWNER
   rvp9_set_hw_backend(&metal_video_decoder_vp9.offer);
#endif
}

static void metal_video_decoder_withdraw(void *unused)
{
   (void)unused;
#ifdef VIDEOTOOLBOX_HEVC_REGISTRY
   videotoolbox_hevc_registry_withdraw_offer(&metal_video_decoder_hevc);
#endif
#ifdef VIDEOTOOLBOX_VP9_OWNER
   rvp9_unset_hw_backend(&metal_video_decoder_vp9.offer);
#endif
}

#ifdef VIDEOTOOLBOX_HEVC_REGISTRY
static VIDEOTOOLBOX_HEVC_AVAILABLE void
#else
static void
#endif
metal_video_decoder_initialize(void *unused)
{
   metal_decoder_snapshot_t *snapshot;
   slock_t *lock = slock_new();
   (void)unused;
   if (!lock)
      return;
#ifdef VIDEOTOOLBOX_HEVC_REGISTRY
   metal_video_decoder_retirement_queue = dispatch_queue_create(
         "org.libretro.metal.hevc.retirement", DISPATCH_QUEUE_SERIAL);
   if (!metal_video_decoder_retirement_queue)
   {
      slock_free(lock);
      return;
   }
   if (!videotoolbox_hevc_registry_init(&metal_video_decoder_hevc, lock,
            metal_video_decoder_retain, metal_video_decoder_release,
            metal_video_decoder_enabled, NULL, metal_video_decoder_enqueue,
            NULL, true))
   {
      slock_free(lock);
      return;
   }
   snapshot = &metal_video_decoder_hevc.snapshot;
#else
   snapshot = &metal_video_decoder_snapshot;
   if (!metal_decoder_snapshot_init(snapshot, lock,
            metal_video_decoder_retain, metal_video_decoder_release))
   {
      slock_free(lock);
      return;
   }
#endif
#ifdef VIDEOTOOLBOX_VP9_OWNER
   if (!videotoolbox_vp9_backend_init(&metal_video_decoder_vp9, snapshot,
            metal_decoder_snapshot_acquire, metal_video_decoder_release,
            metal_decoder_snapshot_current, metal_video_decoder_enabled,
            metal_video_decoder_registration(), 8u * 1024 * 1024, 1))
      return;
#endif
   retro_atomic_store_release_ptr(&metal_video_decoder_active, snapshot);
}

static bool metal_video_decoder_start_supported(void *owner, void *device,
      void *sdr_pipeline, bool hdr_enabled)
{
   metal_decoder_snapshot_t *snapshot;
   if (!owner || !device || !sdr_pipeline)
      return false;
   if (@available(macOS 10.14, iOS 12.0, tvOS 12.0, *))
   {
      dispatch_once_f(&metal_video_decoder_once, NULL,
            metal_video_decoder_initialize);
      snapshot = (metal_decoder_snapshot_t *)retro_atomic_load_acquire_ptr(
            &metal_video_decoder_active);
      return snapshot && metal_decoder_snapshot_publish(snapshot, owner,
            device, hdr_enabled, metal_video_decoder_publish, NULL);
   }
   return false;
}

static void metal_video_decoder_stop(void *owner)
{
   metal_decoder_snapshot_t *snapshot = (metal_decoder_snapshot_t *)
         retro_atomic_load_acquire_ptr(&metal_video_decoder_active);
   if (snapshot)
      metal_decoder_snapshot_withdraw(snapshot, owner,
            metal_video_decoder_withdraw, NULL);
}

static bool metal_video_decoder_owns(void *owner)
{
   const metal_decoder_snapshot_t *snapshot =
         (const metal_decoder_snapshot_t *)retro_atomic_load_acquire_ptr(
               &metal_video_decoder_active);
   return snapshot && metal_decoder_snapshot_is_owner(snapshot, owner);
}
#endif
#endif
