/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RARCH_VAAPI_VP9_POOL_H
#define RARCH_VAAPI_VP9_POOL_H
#include "vaapi_vp9_submit.h"
RETRO_BEGIN_DECLS
typedef struct vaapi_vp9_pool
{
   const vaapi_vp9_session_t *session;
   VADisplay display;
   vaapi_vp9_submit_ops_t ops;
   vaapi_vp9_submit_t transactions[32];
   vaapi_vp9_picture_t work;
   uint64_t generation[32], next_generation;
   uint32_t reads[32], valid, ready, exports;
   int previous_slot;
   bool active, broken;
} vaapi_vp9_pool_t;
/* Cold owner, initially zeroed storage. Session/functions outlive retirement.
 * Shared rvp9 state selects logical DPB slots; this pool protects native use.
 */
bool vaapi_vp9_pool_init(vaapi_vp9_pool_t *pool,
      const vaapi_vp9_session_t *session, const vaapi_vp9_submit_ops_t *ops);
uint32_t vaapi_vp9_pool_held(const vaapi_vp9_pool_t *pool);
/* Owner only. 0 accepted, 1 busy without submission, -1 refused/broken.
 * Native create copies input before acceptance; no heap allocation here. */
int vaapi_vp9_pool_decode(vaapi_vp9_pool_t *pool, const uint8_t *packet,
      size_t size, const rvp9_hw_picture_t *picture);
/* At most one zero-timeout native completion query for this slot.
 * 0 decoded, 1 pending, -1 failure. Cleanup failures remain held for retry. */
int vaapi_vp9_pool_poll(vaapi_vp9_pool_t *pool, unsigned slot);
/* One export per surface incarnation. Ordinary failures leave ticket intact. */
bool vaapi_vp9_pool_acquire(
      vaapi_vp9_pool_t *pool, unsigned slot, uint64_t *ticket);
bool vaapi_vp9_pool_release(
      vaapi_vp9_pool_t *pool, unsigned slot, uint64_t ticket);
bool vaapi_vp9_pool_close(vaapi_vp9_pool_t *pool);
/* Only after matching successful vaTerminate and all renderer exports retire.
 */
bool vaapi_vp9_pool_terminated(vaapi_vp9_pool_t *pool, VADisplay display);
RETRO_END_DECLS
#endif
