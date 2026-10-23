/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_AV1_POOL_H
#define RARCH_VAAPI_AV1_POOL_H
#include "vaapi_av1_submit.h"
RETRO_BEGIN_DECLS
typedef struct vaapi_av1_pool
{
   const vaapi_av1_session_t *session;
   VADisplay display;
   vaapi_av1_submit_ops_t ops;
   vaapi_av1_submit_t transactions[32];
   VADecPictureParameterBufferAV1 work;
   VASliceParameterBufferAV1 tiles[4096];
   uint64_t generation[32], next_generation;
   uint32_t reads[32], writes[32], valid, references, ready, exports;
   bool active, broken;
} vaapi_av1_pool_t;
/* Cold owner storage. All slots supplied to decode are physical surfaces. */
bool vaapi_av1_pool_init(vaapi_av1_pool_t *pool,
      const vaapi_av1_session_t *session, const vaapi_av1_submit_ops_t *ops);
uint32_t vaapi_av1_pool_held(const vaapi_av1_pool_t *pool);
/* 0 accepted, 1 busy without submission, -1 failure. Display is a separate
 * physical destination for grain, otherwise the picture destination. */
int vaapi_av1_pool_decode(vaapi_av1_pool_t *pool, const uint8_t *packet,
      size_t size, const rav1_sequence_t *sequence,
      const rav1_hw_picture_t *picture, unsigned display_slot);
/* At most two zero-timeout native queries; pending work retains both writes. */
int vaapi_av1_pool_poll(vaapi_av1_pool_t *pool, unsigned reference_slot);
bool vaapi_av1_pool_acquire(
      vaapi_av1_pool_t *pool, unsigned slot, uint64_t *ticket);
bool vaapi_av1_pool_release(
      vaapi_av1_pool_t *pool, unsigned slot, uint64_t ticket);
bool vaapi_av1_pool_close(vaapi_av1_pool_t *pool);
/* Only after matching successful vaTerminate and all exports have returned. */
bool vaapi_av1_pool_terminated(vaapi_av1_pool_t *pool, VADisplay display);
RETRO_END_DECLS
#endif
