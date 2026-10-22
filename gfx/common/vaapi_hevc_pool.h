/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RARCH_VAAPI_HEVC_POOL_H
#define RARCH_VAAPI_HEVC_POOL_H
#include "vaapi_hevc_packet.h"
#include "vaapi_hevc_long_packet.h"
RETRO_BEGIN_DECLS

typedef struct vaapi_hevc_pool
{
   const vaapi_hevc_session_t *session;
   VADisplay display;
   vaapi_hevc_short_work_t *work;
   vaapi_hevc_long_work_t *long_work;
   rh265_video *source;
   vaapi_hevc_submit_ops_t ops;
   vaapi_hevc_submit_t transactions[32];
   uint64_t generation[32], next_generation, last_serial;
   int32_t poc[32];
   uint32_t reads[32], valid, ready, exports;
   bool active, broken;
} vaapi_hevc_pool_t;

/* Cold-allocated pool/work/data, initially zeroed. Session and native bindings
 * outlive close. Owner-thread only, including lease returns delivered by the
 * renderer's existing retirement mechanism. Shared rh265 state owns DPB/output
 * selection; this pool additionally protects native writes, reads and exports.
 */
bool vaapi_hevc_pool_init(vaapi_hevc_pool_t *p,
      const vaapi_hevc_session_t *session, const vaapi_hevc_submit_ops_t *ops,
      vaapi_hevc_short_work_t *work);
/* NORMAL owner-thread parser borrow ends at detach. Retirement never queries
 * source. Workspace/data and session outlive native transaction retirement. */
bool vaapi_hevc_pool_init_long(vaapi_hevc_pool_t *p,
      const vaapi_hevc_session_t *session, const vaapi_hevc_submit_ops_t *ops,
      rh265_video *source, vaapi_hevc_long_work_t *work);
uint32_t vaapi_hevc_pool_held(const vaapi_hevc_pool_t *p);
/* 0 accepted (caller commits prepared shared DPB), 1 physical backpressure,
 * -1 refused/broken. A failed native command requires termination recovery. */
int vaapi_hevc_pool_decode(vaapi_hevc_pool_t *p,
      const rh265_video_params_t *params, const rh265_access_unit_t *au,
      const rh265_hw_picture_t *picture, const rh265_nal_t *slices);
/* One slot, at most one zero-timeout check. 0 decoded, 1 pending, -1 error.
 * Cleanup failures remain held and are retried without another native check. */
int vaapi_hevc_pool_poll(vaapi_hevc_pool_t *p, unsigned slot);
/* One export ticket per surface incarnation. The renderer's frame object may
 * have multiple references; return the ticket once, after its last GPU use. */
static INLINE bool vaapi_hevc_pool_acquire(vaapi_hevc_pool_t *p,unsigned slot,uint64_t *ticket)
{
 uint32_t bit;
 if(!p || !p->active || p->broken || !ticket || slot>=p->session->slots)return false;
 bit=UINT32_C(1)<<slot;
 if(!(p->ready&bit) || (p->exports&bit) || !p->generation[slot])return false;
 p->exports|=bit;*ticket=p->generation[slot];return true;
}
static INLINE bool vaapi_hevc_pool_release(vaapi_hevc_pool_t *p,unsigned slot,uint64_t ticket)
{
 uint32_t bit;
 if(!p || !p->active || slot>=p->session->slots || !ticket)return false;
 bit=UINT32_C(1)<<slot;
 if(!(p->exports&bit) || p->generation[slot]!=ticket)return false;
 p->exports&=~bit;return true;
}
bool vaapi_hevc_pool_close(vaapi_hevc_pool_t *p);
/* Only after actual successful termination of this display, with no exports. */
bool vaapi_hevc_pool_terminated(vaapi_hevc_pool_t *p, VADisplay display);
RETRO_END_DECLS
#endif
