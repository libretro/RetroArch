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
   uint64_t generation[32], next_generation;
   uint32_t reads[32], valid, ready;
   unsigned exports[32], logical_slots;
   int mapping[32];
   bool active, broken;
} vaapi_vp9_pool_t;

/* Cold-allocated, initially zeroed storage. Owner thread only, including lease
 * returns from renderer retirement. Session/bindings outlive close. Logical
 * DPB slots are renamed when older exported incarnations remain held. */
bool vaapi_vp9_pool_init(vaapi_vp9_pool_t *pool,
      const vaapi_vp9_session_t *session, const vaapi_vp9_submit_ops_t *ops,
      unsigned logical_slots);
uint32_t vaapi_vp9_pool_held(const vaapi_vp9_pool_t *pool);
/* 0 accepted, 1 physical backpressure without submission, negative failure.
 * The caller commits shared reference state only after acceptance. */
int vaapi_vp9_pool_decode(vaapi_vp9_pool_t *pool,
      const rvp9_hw_picture_t *picture, const uint8_t *data, size_t size);
/* Physical slot: at most one zero-timeout check. Buffer destruction retries
 * retain the transaction without another completion check. */
int vaapi_vp9_pool_poll(vaapi_vp9_pool_t *pool, unsigned slot);
/* Independent exports of show_existing_frame share a surface incarnation. */
bool vaapi_vp9_pool_acquire(vaapi_vp9_pool_t *pool,
      unsigned slot, uint64_t *ticket);
bool vaapi_vp9_pool_current(const vaapi_vp9_pool_t *pool,
      unsigned slot, uint64_t ticket);
bool vaapi_vp9_pool_release(vaapi_vp9_pool_t *pool,
      unsigned slot, uint64_t ticket);
bool vaapi_vp9_pool_close(vaapi_vp9_pool_t *pool);
/* Only after successful termination of this display, with no exports. */
bool vaapi_vp9_pool_terminated(vaapi_vp9_pool_t *pool, VADisplay display);
RETRO_END_DECLS
#endif
