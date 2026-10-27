/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef MEDIACODEC_HEVC_REGISTRY_H
#define MEDIACODEC_HEVC_REGISTRY_H
#include "mediacodec_hevc_backend.h"
#include <retro_inline.h>
#include <limits.h>
#include <string.h>
#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_HAS_PTR)                     \
      && defined(RETRO_ATOMIC_LOCK_FREE)
#define MEDIACODEC_HEVC_REGISTRY 1
RETRO_BEGIN_DECLS
typedef struct mediacodec_hevc_registry
{
   mediacodec_hevc_backend_t backend;
   rh265_hw_backend_t offer;
   void *owner;
   retro_atomic_int_t epoch;
} mediacodec_hevc_registry_t;
static INLINE bool mediacodec_hevc_registry_open(void *opaque,
      const rh265_video *source, const rh265_video_params_t *params,
      unsigned slots, rh265_hw_ops_t *ops)
{
   mediacodec_hevc_registry_t *r = (mediacodec_hevc_registry_t *)opaque;
   int epoch;
   if (!ops)
      return false;
   memset(ops, 0, sizeof(*ops));
   epoch = retro_atomic_load_acquire_int(&r->epoch);
   if (epoch <= 0 || !(epoch & 1))
      return false;
   if (!r->backend.offer.open(
             r->backend.offer.user, source, params, slots, ops))
      return false;
   if (retro_atomic_load_acquire_int(&r->epoch) != epoch)
   {
      r->backend.offer.close(ops);
      return false;
   }
   return true;
}
/* Initialize once. Registry/configuration storage outlives all setup callers,
 * opened sessions and deferred retirement. No renderer object is captured. */
static INLINE bool mediacodec_hevc_registry_init(mediacodec_hevc_registry_t *r,
      JNIEnv *(*environment)(void *), void *user, unsigned max_input,
      unsigned max_sessions)
{
   if (!r)
      return false;
   memset(r, 0, sizeof(*r));
   if (!mediacodec_hevc_backend_init(
             &r->backend, environment, user, max_input, max_sessions))
      return false;
   retro_atomic_int_init(&r->epoch, 0);
   r->offer.user = r;
   r->offer.open = mediacodec_hevc_registry_open;
   r->offer.close = r->backend.offer.close;
   return true;
}
/* Publish/withdraw share one driver lifecycle owner. Native renderer admission
 * must precede publication; an offer alone does not prove hardware support. */
static INLINE bool mediacodec_hevc_registry_publish(
      mediacodec_hevc_registry_t *r, void *owner)
{
   int epoch;
   if (!r || !r->offer.open || !owner)
      return false;
   epoch = retro_atomic_load_acquire_int(&r->epoch);
   if (epoch < 0)
      return false;
   if (epoch >= INT_MAX - 2)
   {
      retro_atomic_store_release_int(&r->epoch, -1);
      r->owner = NULL;
      rh265_unset_hw_backend(&r->offer);
      return false;
   }
   r->owner = owner;
   retro_atomic_store_release_int(&r->epoch, epoch + (epoch & 1 ? 2 : 1));
   rh265_set_hw_backend(&r->offer);
   return true;
}
static INLINE bool mediacodec_hevc_registry_withdraw(
      mediacodec_hevc_registry_t *r, void *owner)
{
   int epoch;
   if (!r || !r->offer.open || !owner || r->owner != owner)
      return false;
   epoch = retro_atomic_load_acquire_int(&r->epoch);
   retro_atomic_store_release_int(&r->epoch, epoch == INT_MAX ? -1 : epoch + 1);
   r->owner = NULL;
   rh265_unset_hw_backend(&r->offer);
   return true;
}
RETRO_END_DECLS
#endif
#endif
