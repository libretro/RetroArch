/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef MEDIACODEC_VP9_BACKEND_H
#define MEDIACODEC_VP9_BACKEND_H
#include "mediacodec_vp9_decoder.h"
#include <retro_atomic.h>
RETRO_BEGIN_DECLS

typedef struct mediacodec_vp9_backend
{
   rvp9_hw_backend_t offer;
   void *user;
   JNIEnv *(*environment)(void *user);
   unsigned max_input, max_sessions;
   retro_atomic_int_t active;
} mediacodec_vp9_backend_t;

/* Cold setup only; does not register or advertise hardware support. Caller
 * admits a validated renderer before publishing offer. environment returns the
 * calling decoder-worker's JNIEnv (e.g. rjni_env), never a saved foreign env.
 * Storage/user/callback outlive all admitted decoders and retired contexts.
 * max_input is the explicit packet staging bound, <=64 MiB. At most 1..8
 * sessions including retirement may coexist; seek storms refuse excess opens
 * without waiting. Each admitted session has one cold retirement worker,
 * created before native startup; decode/output/export add no thread, wait or
 * lock. Failed startup and close return without joining; active includes
 * deferred cleanup. */
bool mediacodec_vp9_backend_init(mediacodec_vp9_backend_t *backend,
      JNIEnv *(*environment)(void *), void *user, unsigned max_input,
      unsigned max_sessions);
unsigned mediacodec_vp9_backend_active(mediacodec_vp9_backend_t *backend);
RETRO_END_DECLS
#endif
