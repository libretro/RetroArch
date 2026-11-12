/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_VT_PACKET_H
#define RECORD_VT_PACKET_H
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>
RETRO_BEGIN_DECLS
typedef struct record_vt_packet record_vt_packet_t;
typedef struct record_vt_packet_view
{
   const void *data;
   size_t size;
   uint64_t dts, pts;
   bool keyframe;
} record_vt_packet_view_t;
/* Worker owner. Borrows the expected native format until free. Cold bounded
 * storage is used only for fragmented CoreMedia blocks. */
record_vt_packet_t *record_vt_packet_new(void *format, unsigned timescale, size_t capacity);
/* Caller keeps the CMSampleBuffer alive until release. Repeats the same loan;
 * a different sample is refused while live. No payload copy for contiguous data.
 * Returns 1 for a loan, 0 busy, -1 invalid input. */
int record_vt_packet_read(record_vt_packet_t *packet, void *sample, record_vt_packet_view_t *view);
bool record_vt_packet_release(record_vt_packet_t *packet);
bool record_vt_packet_free(record_vt_packet_t *packet);
RETRO_END_DECLS
#endif
