/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_VAAPI_AV1_SUBMIT_H
#define RARCH_VAAPI_AV1_SUBMIT_H
#include "vaapi_hevc_submit.h"
#include "vaapi_av1_session.h"
#include "vaapi_av1_picture.h"
typedef vaapi_hevc_submit_ops_t vaapi_av1_submit_ops_t;
typedef struct vaapi_av1_submit
{
   vaapi_hevc_submit_t native;
   VASurfaceID display_surface;
   VAStatus display_completion;
   bool display_complete;
} vaapi_av1_submit_t;
RETRO_BEGIN_DECLS
/* Decoder owner; false may retain native ownership. Both grain destinations
 * must complete before retirement. No completion check waits. */
bool vaapi_av1_submit_start(vaapi_av1_submit_t *transaction,
      const vaapi_av1_session_t *session, const vaapi_av1_submit_ops_t *ops,
      const VADecPictureParameterBufferAV1 *picture,
      const VASliceParameterBufferAV1 *tiles, unsigned count,
      const uint8_t *packet, unsigned size);
int vaapi_av1_submit_poll(vaapi_av1_submit_t *transaction);
bool vaapi_av1_submit_retire(vaapi_av1_submit_t *transaction);
bool vaapi_av1_submit_terminated(
      vaapi_av1_submit_t *transaction, VADisplay display);
RETRO_END_DECLS
#endif
