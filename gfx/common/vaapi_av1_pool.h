/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RARCH_VAAPI_AV1_POOL_H
#define RARCH_VAAPI_AV1_POOL_H
#include "vaapi_av1_submit.h"
RETRO_BEGIN_DECLS

#if RARCH_VAAPI_AV1_PARAMS_AVAILABLE
typedef struct vaapi_av1_pool
{
   const vaapi_av1_session_t *session;
   VADisplay display;
   vaapi_av1_submit_ops_t ops;
   vaapi_av1_submit_t transactions[32];
   vaapi_av1_params_t params;
   VASliceParameterBufferAV1 slices[4096];
   uint64_t generation[32], next_generation;
   uint32_t reads[32], writes[32], valid, ready;
   unsigned exports[32], outputs[32], logical_slots;
   int mapping[32];
   bool active, broken, grain;
} vaapi_av1_pool_t;

/* Cold-allocated, initially zeroed storage. Owner thread only, including lease
 * returns from renderer retirement. Session/bindings outlive close. Logical
 * DPB slots are renamed when older exported incarnations remain held. */
bool vaapi_av1_pool_init(vaapi_av1_pool_t *pool,
      const vaapi_av1_session_t *session, const vaapi_av1_submit_ops_t *ops,
      unsigned logical_slots);
/* Grain retains separate reference/display pairs; at least 18 native slots. */
bool vaapi_av1_pool_init_grain(vaapi_av1_pool_t *pool,
      const vaapi_av1_session_t *session, const vaapi_av1_submit_ops_t *ops,
      unsigned logical_slots);
uint32_t vaapi_av1_pool_held(const vaapi_av1_pool_t *pool);
/* 0 accepted, 1 physical backpressure without submission, negative failure.
 * The caller commits shared reference state only after acceptance. */
int vaapi_av1_pool_decode(vaapi_av1_pool_t *pool,
      const rav1_sequence_t *sequence, const rav1_hw_picture_t *picture,
      const uint8_t *data, size_t size);
/* Physical slot: at most one zero-timeout check. Buffer destruction retries
 * retain the transaction without another completion check. */
int vaapi_av1_pool_poll(vaapi_av1_pool_t *pool, unsigned slot);
/* Independent exports of show_existing share a surface incarnation. */
bool vaapi_av1_pool_acquire(vaapi_av1_pool_t *pool,
      unsigned slot, uint64_t *ticket);
bool vaapi_av1_pool_current(const vaapi_av1_pool_t *pool,
      unsigned slot, uint64_t ticket);
bool vaapi_av1_pool_release(vaapi_av1_pool_t *pool,
      unsigned slot, uint64_t ticket);
bool vaapi_av1_pool_close(vaapi_av1_pool_t *pool);
/* Only after successful termination of this display, with no exports. */
bool vaapi_av1_pool_terminated(vaapi_av1_pool_t *pool, VADisplay display);
#endif
RETRO_END_DECLS
#endif
