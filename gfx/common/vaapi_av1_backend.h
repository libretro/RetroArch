/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_AV1_BACKEND_H
#define RARCH_VAAPI_AV1_BACKEND_H
#include "vaapi_av1_owner.h"
#include <retro_atomic.h>
RETRO_BEGIN_DECLS

typedef struct vaapi_av1_backend
{
   rav1_hw_backend_t offer;
   void *owner;
   unsigned max_sessions;
   retro_atomic_int_t epoch, major_id, minor_id, active;
} vaapi_av1_backend_t;

/* Initialize once. Storage and immutable configuration outlive setup readers,
 * sessions and deferred retirement. Requires threads and lock-free atomics. */
bool vaapi_av1_backend_init(
      vaapi_av1_backend_t *backend, unsigned max_sessions);
/* One serialized renderer lifecycle owner. Publish only after native PRIME
 * import prerequisites and the actual render-device identity are verified. */
bool vaapi_av1_backend_publish(vaapi_av1_backend_t *backend, void *owner,
      unsigned major_id, unsigned minor_id);
bool vaapi_av1_backend_withdraw(vaapi_av1_backend_t *backend, void *owner);

RETRO_END_DECLS
#endif
