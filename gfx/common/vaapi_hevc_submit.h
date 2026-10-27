/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#ifndef RARCH_VAAPI_HEVC_SUBMIT_H
#define RARCH_VAAPI_HEVC_SUBMIT_H
#include "vaapi_hevc_session.h"
RETRO_BEGIN_DECLS

typedef struct vaapi_hevc_submit_ops
{
   VAStatus (*create)(VADisplay, VAContextID, VABufferType, unsigned, unsigned,
         void *, VABufferID *);
   VAStatus (*destroy)(VADisplay, VABufferID);
   VAStatus (*begin)(VADisplay, VAContextID, VASurfaceID);
   VAStatus (*render)(VADisplay, VAContextID, VABufferID *, int);
   VAStatus (*end)(VADisplay, VAContextID);
   VAStatus (*sync)(VADisplay, VASurfaceID, uint64_t);
} vaapi_hevc_submit_ops_t;

typedef struct vaapi_hevc_submit
{
   vaapi_hevc_submit_ops_t ops;
   VADisplay display;
   VABufferID buffers[5];
   VASurfaceID surface;
   VAStatus error, completion;
   unsigned count;
   bool active, submitted, complete, poisoned;
} vaapi_hevc_submit_t;

/* Decoder owner only. Prepared picture/slices must match this session's codec
 * state and slice mode. Input is copied by create; no native buffer is mapped.
 * False may retain ownership: retire before reusing the transaction or surface.
 * A poisoned transaction requires successful display termination. */
bool vaapi_hevc_submit_start(vaapi_hevc_submit_t *t,
      const vaapi_hevc_session_t *s, const vaapi_hevc_submit_ops_t *ops,
      const VAPictureParameterBufferHEVC *picture,
      const VAIQMatrixBufferHEVC *matrix, const void *slices, unsigned count,
      const uint8_t *data, unsigned size);
/* NORMAL subset entries contain coded entry_point_offset_minus1 values in
 * slice order. Slice subset ranges must cover the array exactly. The optional
 * fifth native buffer follows the same transaction/retirement ownership. */
bool vaapi_hevc_submit_start_entries(vaapi_hevc_submit_t *t,
      const vaapi_hevc_session_t *s, const vaapi_hevc_submit_ops_t *ops,
      const VAPictureParameterBufferHEVC *picture,
      const VAIQMatrixBufferHEVC *matrix,
      const VASliceParameterBufferHEVC *slices, unsigned count,
      const uint8_t *data, unsigned size, const uint32_t *entries,
      unsigned num_entries);
/* One zero-timeout completion check: 0 complete, 1 pending, -1 error.
 * A completed error is still retired; uncertain failures poison ownership. */
int vaapi_hevc_submit_poll(vaapi_hevc_submit_t *t);
/* No wait. Pending/poisoned work refuses; failed buffer destruction is retried.
 */
bool vaapi_hevc_submit_retire(vaapi_hevc_submit_t *t);
/* Only after actual successful vaTerminate of the matching display. */
bool vaapi_hevc_submit_terminated(vaapi_hevc_submit_t *t, VADisplay display);
RETRO_END_DECLS
#endif
